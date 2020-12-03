/*
 * Exported functions of the Wine preprocessor
 *
 * Copyright 1998 Bertho A. Stultiens
 * Copyright 2002 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <time.h>
#include <stdlib.h>

#include "wpp_private.h"
#include "wine/wpp.h"

int ppy_debug, pp_flex_debug;

struct define
{
    struct list    entry;
    char          *name;
    char          *value;
};

static struct list cmdline_defines = LIST_INIT( cmdline_defines );

static void add_cmdline_defines(void)
{
    struct define *def;

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (def->value) pp_add_define( def->name, def->value );
    }
}

static void del_cmdline_defines(void)
{
    struct define *def;

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (def->value) pp_del_define( def->name );
    }
}

static void add_special_defines(void)
{
    time_t now = time(NULL);
    pp_entry_t *ppp;
    char buf[32];

    strftime(buf, sizeof(buf), "\"%b %d %Y\"", localtime(&now));
    pp_add_define( "__DATE__", buf );

    strftime(buf, sizeof(buf), "\"%H:%M:%S\"", localtime(&now));
    pp_add_define( "__TIME__", buf );

    ppp = pp_add_define( "__FILE__", "" );
    ppp->type = def_special;

    ppp = pp_add_define( "__LINE__", "" );
    ppp->type = def_special;
}

static void del_special_defines(void)
{
    pp_del_define( "__DATE__" );
    pp_del_define( "__TIME__" );
    pp_del_define( "__FILE__" );
    pp_del_define( "__LINE__" );
}


/* add a define to the preprocessor list */
void wpp_add_define( const char *name, const char *value )
{
    struct define *def;

    if (!value) value = "";

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (!strcmp( def->name, name ))
        {
            free( def->value );
            def->value = pp_xstrdup(value);
            return;
        }
    }

    def = pp_xmalloc( sizeof(*def) );
    def->name  = pp_xstrdup(name);
    def->value = pp_xstrdup(value);
    list_add_head( &cmdline_defines, &def->entry );
}


/* undefine a previously added definition */
void wpp_del_define( const char *name )
{
    struct define *def;

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (!strcmp( def->name, name ))
        {
            free( def->value );
            def->value = NULL;
            return;
        }
    }
}


/* add a command-line define of the form NAME=VALUE */
void wpp_add_cmdline_define( const char *value )
{
    char *p;
    char *str = pp_xstrdup(value);

    p = strchr( str, '=' );
    if (p) *p++ = 0;
    wpp_add_define( str, p );
    free( str );
}


/* set the various debug flags */
void wpp_set_debug( int lex_debug, int parser_debug, int msg_debug )
{
    pp_flex_debug   = lex_debug;
    ppy_debug       = parser_debug;
    pp_status.debug = msg_debug;
}


/* set the pedantic mode */
void wpp_set_pedantic( int on )
{
    pp_status.pedantic = on;
}


/* the main preprocessor parsing loop */
int wpp_parse( const char *input, FILE *output )
{
    int ret;

    pp_status.input = NULL;
    pp_status.line_number = 1;
    pp_status.char_number = 1;

    pp_push_define_state();
    add_cmdline_defines();
    add_special_defines();

    if (!input) pp_status.file = stdin;
    else if (!(pp_status.file = fopen(input, "rt")))
        ppy_error("Could not open %s\n", input);

    pp_status.input = input ? pp_xstrdup(input) : NULL;

    ppy_out = output;
    pp_writestring("# 1 \"%s\" 1\n", input ? input : "");

    ret = ppy_parse();

    if (input)
    {
	fclose(pp_status.file);
	free(pp_status.input);
    }
    /* Clean if_stack, it could remain dirty on errors */
    while (pp_get_if_depth()) pp_pop_if();
    del_special_defines();
    del_cmdline_defines();
    pp_pop_define_state();
    return ret;
}
