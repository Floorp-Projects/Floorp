/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/**********************************************************************
 e_kit_parser.c
 By Daniel Malmer 
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include <assert.h>

#include "xp_md5.h"
#include "sec.h"

#define OUTPUT_FILENAME "Netscape.lock"

static char buf[BUFSIZ];

int verbose = 0;

int fatal_errors = 0;
int lineno;

#define CHUNKSIZE 64;

char* output_buf = NULL;
int output_max_size = 0;

struct mapping;

typedef int  validation_func(char*);
typedef void processing_func(struct mapping*, char*);

struct mapping {
    char* token;
    char* resource;
    validation_func* validate;
    processing_func* process;
    char* desens[5];
};

extern void error(char*);
extern void warn(char*);

extern void default_handler(struct mapping*, char*);
extern void boolean_handler(struct mapping*, char*);
extern void menu_handler(struct mapping*, char*);
extern void proxy_handler(struct mapping*, char*);
extern void windoze_handler(struct mapping*, char*);
extern void mac_handler(struct mapping*, char*);

extern int boolean_validate(char*);
extern int number_validate(char*);
extern int user_agent_validate(char*);
extern int proxy_type_validate(char*);

extern void append_to_output_file(char*);
extern int matches(char* str1, char* str2);

struct proxy {
  char* name;
  char* resource;
  char* host;
  char* port;
};

struct proxy proxies[] = {
  {"ftp_proxy",    "ftp",    NULL, NULL},
  {"gopher_proxy", "gopher", NULL, NULL},
  {"http_proxy",   "http",   NULL, NULL},
  {"wais_proxy",   "wais",   NULL, NULL},
  {"https_proxy",  "https",  NULL, NULL},
};

#define NUM_PROXIES (sizeof(proxies)/sizeof(proxies[0]))

struct mapping mappings[] = {
{"logo button url", ".logoButtonUrl", NULL, default_handler, {NULL}},
{"home page", ".homePage", NULL, default_handler, 
  {"*general_prefs*startupFrame*startupBox.home",
   "*general_prefs*startupFrame*startupBox.blank",
   "*general_prefs*startupFrame*startupBox.homeText",
  }
},
{"nntp_server", ".nntpServer", NULL, default_handler, 
    {"*mailnews_prefs*newsFrame*newshostText"}
},
{"smtp_server", ".smtpServer", NULL, default_handler,
    {"*mailnews_prefs*outMailFrame*smtpText"}
},
{"pop_server", ".popServer", NULL, default_handler,
    {"*mailnews_prefs*inMailFrame*srvrText"}
},
{"autoload home page", ".autoloadHomePage", boolean_validate, default_handler,
 {"*general_prefs*startupFrame*startupBox.home",
  "*general_prefs*startupFrame*startupBox.blank"
 }
},
{"leave on server", ".leaveOnServer", boolean_validate, boolean_handler, 
    {"*mailnews_prefs*inMailFrame*msgRemove",
     "*mailnews_prefs*inMailFrame*msgLeave"
    }
},
{"proxy type", ".proxyMode", proxy_type_validate, default_handler,
    {"*network_prefs*proxiesFrame*manualToggle",
     "*network_prefs*proxiesFrame*autoToggle",
     "*network_prefs*proxiesFrame*noProxiesToggle"},
},
{"ftp_proxy", ".ftpProxy", NULL, proxy_handler, {NULL}},
{"gopher_proxy", ".gopherProxy", NULL, proxy_handler, {NULL}},
{"http_proxy", ".httpProxy", NULL, proxy_handler, {NULL}},
{"wais_proxy", ".waisProxy", NULL, proxy_handler, {NULL}},
{"https_proxy", ".httpsProxy", NULL, proxy_handler, {NULL}},
{"ftp_proxyport", ".ftpProxyPort", number_validate, proxy_handler, {NULL}},
{"gopher_proxyport", ".gopherProxyPort", number_validate, proxy_handler, {NULL}},
{"http_proxyport", ".httpProxyPort", number_validate, proxy_handler, {NULL}},
{"news_proxyport", ".newsProxyPort", number_validate, proxy_handler, {NULL}},
{"wais_proxyport", ".waisProxyPort", number_validate, proxy_handler, {NULL}},
{"https_proxyport", ".httpsProxyPort", number_validate, proxy_handler, {NULL}},
{"socks_server", ".socksServer", NULL, default_handler, 
    {"*proxiesFrame*socksText"}
},
{"socks_serverport", ".socksServerPort", number_validate, default_handler,
    {"*proxiesFrame*socksPort"}
},
{"no_proxy", ".noProxy", NULL, default_handler, 
    {"*proxiesFrame*noText"}
},
{"auto config url", ".autoconfigUrl", NULL, default_handler, 
    {"*network_prefs*proxiesFrame*locationText"}
},
{"user agent", ".userAgent", user_agent_validate, default_handler, {NULL}},
{"x animation file", ".animationFile", NULL, default_handler, {NULL}},
{"button 1", "*urlBar*inetIndex.labelString", NULL, default_handler, {NULL}},
{"button 2", "*urlBar*inetWhite.labelString", NULL, default_handler, {NULL}},
{"button 3", "*urlBar*inetYellow.labelString", NULL, default_handler, {NULL}},
{"button 4", "*urlBar*whatsNew.labelString", NULL, default_handler, {NULL}},
{"button 5", "*urlBar*whatsCool.labelString", NULL, default_handler, {NULL}},
{"button 1 url", ".inetIndexBUrl", NULL, default_handler, {NULL}},
{"button 2 url", ".inetWhiteBUrl", NULL, default_handler, {NULL}},
{"button 3 url", ".inetYellowBUrl", NULL, default_handler, {NULL}},
{"button 4 url", ".whatsNewBUrl", NULL, default_handler, {NULL}},
{"button 5 url", ".whatsCoolBUrl", NULL, default_handler, {NULL}},
{"directory 1", "*menuBar*inetIndex", NULL, menu_handler, {NULL}},
{"directory 2", "*menuBar*inetWhite", NULL, menu_handler, {NULL}},
{"directory 3", "*menuBar*inetSearch", NULL, menu_handler, {NULL}},
{"directory 4", "*menuBar*whatsNew", NULL, menu_handler, {NULL}},
{"directory 5", "*menuBar*whatsCool", NULL, menu_handler, {NULL}},
{"directory 6", "*menuBar*directoryMenu6", NULL, menu_handler, {NULL}},
{"directory 7", "*menuBar*directoryMenu7", NULL, menu_handler, {NULL}},
{"directory 8", "*menuBar*directoryMenu8", NULL, menu_handler, {NULL}},
{"directory 9", "*menuBar*directoryMenu9", NULL, menu_handler, {NULL}},
{"directory 10", "*menuBar*directoryMenu10", NULL, menu_handler, {NULL}},
{"directory 11", "*menuBar*directoryMenu11", NULL, menu_handler, {NULL}},
{"directory 12", "*menuBar*directoryMenu12", NULL, menu_handler, {NULL}},
{"directory 13", "*menuBar*directoryMenu13", NULL, menu_handler, {NULL}},
{"directory 14", "*menuBar*directoryMenu14", NULL, menu_handler, {NULL}},
{"directory 15", "*menuBar*directoryMenu15", NULL, menu_handler, {NULL}},
{"directory 16", "*menuBar*directoryMenu16", NULL, menu_handler, {NULL}},
{"directory 17", "*menuBar*directoryMenu17", NULL, menu_handler, {NULL}},
{"directory 18", "*menuBar*directoryMenu18", NULL, menu_handler, {NULL}},
{"directory 19", "*menuBar*directoryMenu19", NULL, menu_handler, {NULL}},
{"directory 20", "*menuBar*directoryMenu20", NULL, menu_handler, {NULL}},
{"directory 21", "*menuBar*directoryMenu21", NULL, menu_handler, {NULL}},
{"directory 22", "*menuBar*directoryMenu22", NULL, menu_handler, {NULL}},
{"directory 23", "*menuBar*directoryMenu23", NULL, menu_handler, {NULL}},
{"directory 24", "*menuBar*directoryMenu24", NULL, menu_handler, {NULL}},
{"directory 25", "*menuBar*directoryMenu25", NULL, menu_handler, {NULL}},
{"directory 1 url", ".netscapeUrl", NULL, default_handler, {NULL}},
{"directory 2 url", ".whatsNewUrl", NULL, default_handler, {NULL}},
{"directory 3 url", ".whatsCoolUrl", NULL, default_handler, {NULL}},
{"directory 4 url", ".directoryMenu4Url", NULL, default_handler, {NULL}},
{"directory 5 url", ".galleriaUrl", NULL, default_handler, {NULL}},
{"directory 6 url", ".inetIndexUrl", NULL, default_handler, {NULL}},
{"directory 7 url", ".inetSearchUrl", NULL, default_handler, {NULL}},
{"directory 8 url", ".inetWhiteUrl", NULL, default_handler, {NULL}},
{"directory 9 url", ".inetAboutUrl", NULL, default_handler, {NULL}},
{"directory 10 url", ".directoryMenu10Url", NULL, default_handler, {NULL}},
{"directory 11 url", ".directoryMenu11Url", NULL, default_handler, {NULL}},
{"directory 12 url", ".directoryMenu12Url", NULL, default_handler, {NULL}},
{"directory 13 url", ".directoryMenu13Url", NULL, default_handler, {NULL}},
{"directory 14 url", ".directoryMenu14Url", NULL, default_handler, {NULL}},
{"directory 15 url", ".directoryMenu15Url", NULL, default_handler, {NULL}},
{"directory 16 url", ".directoryMenu16Url", NULL, default_handler, {NULL}},
{"directory 17 url", ".directoryMenu17Url", NULL, default_handler, {NULL}},
{"directory 18 url", ".directoryMenu18Url", NULL, default_handler, {NULL}},
{"directory 19 url", ".directoryMenu19Url", NULL, default_handler, {NULL}},
{"directory 20 url", ".directoryMenu20Url", NULL, default_handler, {NULL}},
{"directory 21 url", ".directoryMenu21Url", NULL, default_handler, {NULL}},
{"directory 22 url", ".directoryMenu22Url", NULL, default_handler, {NULL}},
{"directory 23 url", ".directoryMenu23Url", NULL, default_handler, {NULL}},
{"directory 24 url", ".directoryMenu24Url", NULL, default_handler, {NULL}},
{"directory 25 url", ".directoryMenu25Url", NULL, default_handler, {NULL}},
{"directory 1 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 2 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 3 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 4 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 5 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 6 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 7 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 8 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 9 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 10 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 11 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 12 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 13 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 14 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 15 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 16 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 17 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 18 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 19 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 20 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 21 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 22 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 23 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 24 prompt", NULL, NULL, windoze_handler, {NULL}},
{"directory 25 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 1", "*menuBar*manual", NULL, menu_handler, {NULL}},
{"help 2", "*menuBar*relnotes", NULL, menu_handler, {NULL}},
{"help 3", "*menuBar*productInfo", NULL, menu_handler, {NULL}},
{"help 4", "*menuBar*feedback", NULL, menu_handler, {NULL}},
{"help 5", "*menuBar*intl", NULL, menu_handler, {NULL}},
{"help 6", "*menuBar*abourSecurity", NULL, menu_handler, {NULL}},
{"help 7", "*menuBar*helpMenu7", NULL, menu_handler, {NULL}},
{"help 8", "*menuBar*registration", NULL, menu_handler, {NULL}},
{"help 9", "*menuBar*upgrade", NULL, menu_handler, {NULL}},
{"help 10", "*menuBar*services", NULL, menu_handler, {NULL}},
{"help 11", "*menuBar*aboutUsenet", NULL, menu_handler, {NULL}},
{"help 12", "*menuBar*aboutPlugins", NULL, menu_handler, {NULL}},
{"help 13", "*menuBar*helpMenu13", NULL, menu_handler, {NULL}},
{"help 14", "*menuBar*helpMenu14", NULL, menu_handler, {NULL}},
{"help 15", "*menuBar*helpMenu15", NULL, menu_handler, {NULL}},
{"help 16", "*menuBar*helpMenu16", NULL, menu_handler, {NULL}},
{"help 17", "*menuBar*helpMenu17", NULL, menu_handler, {NULL}},
{"help 18", "*menuBar*helpMenu18", NULL, menu_handler, {NULL}},
{"help 19", "*menuBar*helpMenu19", NULL, menu_handler, {NULL}},
{"help 20", "*menuBar*helpMenu20", NULL, menu_handler, {NULL}},
{"help 21", "*menuBar*helpMenu21", NULL, menu_handler, {NULL}},
{"help 22", "*menuBar*helpMenu22", NULL, menu_handler, {NULL}},
{"help 23", "*menuBar*helpMenu23", NULL, menu_handler, {NULL}},
{"help 24", "*menuBar*helpMenu24", NULL, menu_handler, {NULL}},
{"help 25", "*menuBar*helpMenu25", NULL, menu_handler, {NULL}},
{"help 1 url", ".manualUrl", NULL, default_handler, {NULL}},
{"help 2 url", ".relnotesUrl", NULL, default_handler, {NULL}},
{"help 3 url", ".productInfoUrl", NULL, default_handler, {NULL}},
{"help 4 url", ".feedbackUrl", NULL, default_handler, {NULL}},
{"help 5 url", ".intlUrl", NULL, default_handler, {NULL}},
{"help 6 url", ".aboutSecurityUrl", NULL, default_handler, {NULL}},
{"help 7 url", ".helpMenu7Url", NULL, default_handler, {NULL}},
{"help 8 url", ".registrationUrl", NULL, default_handler, {NULL}},
{"help 9 url", ".upgradeUrl", NULL, default_handler, {NULL}},
{"help 10 url", ".servicesUrl", NULL, default_handler, {NULL}},
{"help 11 url", ".aboutUsenetUrl", NULL, default_handler, {NULL}},
{"help 12 url", ".aboutpluginsUrl", NULL, default_handler, {NULL}},
{"help 13 url", ".helpMenu13Url", NULL, default_handler, {NULL}},
{"help 14 url", ".helpMenu14Url", NULL, default_handler, {NULL}},
{"help 15 url", ".helpMenu15Url", NULL, default_handler, {NULL}},
{"help 16 url", ".helpMenu16Url", NULL, default_handler, {NULL}},
{"help 17 url", ".helpMenu17Url", NULL, default_handler, {NULL}},
{"help 18 url", ".helpMenu18Url", NULL, default_handler, {NULL}},
{"help 19 url", ".helpMenu19Url", NULL, default_handler, {NULL}},
{"help 20 url", ".helpMenu20Url", NULL, default_handler, {NULL}},
{"help 21 url", ".helpMenu21Url", NULL, default_handler, {NULL}},
{"help 22 url", ".helpMenu22Url", NULL, default_handler, {NULL}},
{"help 23 url", ".helpMenu23Url", NULL, default_handler, {NULL}},
{"help 24 url", ".helpMenu24Url", NULL, default_handler, {NULL}},
{"help 25 url", ".helpMenu25Url", NULL, default_handler, {NULL}},
{"help 1 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 2 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 3 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 4 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 5 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 6 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 7 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 8 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 9 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 10 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 11 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 12 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 13 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 14 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 15 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 16 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 17 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 18 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 19 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 20 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 21 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 22 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 23 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 24 prompt", NULL, NULL, windoze_handler, {NULL}},
{"help 25 prompt", NULL, NULL, windoze_handler, {NULL}},
};

#define NUM_MAPPINGS (sizeof(mappings)/sizeof(mappings[0]))


/*
 * boolean_validate
 */
int
boolean_validate(char* arg)
{
    if ( !matches(arg, "yes")   &&
         !matches(arg, "no")    &&
         !matches(arg, "true")  &&
         !matches(arg, "false") ) {
        error("Valid values are 'yes', 'no', 'true' or 'false'.");
        return 0;
    }

    return 1;
}


/*
 * number_validate
 */
int
number_validate(char* arg)
{
    char* ptr;

    for ( ptr = arg; *ptr != '\0'; ptr++ ) {
        if ( !isdigit(*ptr) ) {
            error("Value must be a number.");
            return 0;
        }
    }

    return 1;
}


/*
 * proxy_type_validate
 */
int
proxy_type_validate(char* arg)
{
    if ( matches(arg, "none") ||
         matches(arg, "0") ) {
        strcpy(arg, "0");
        return 1;
    }

    if ( matches(arg, "manual") ||
         matches(arg, "1") ) {
        strcpy(arg, "1");
        return 1;
    }

    if ( matches(arg, "automatic") ||
         matches(arg, "2") ) {
        strcpy(arg, "2");
        return 1;
    }

    error("Valid values are 'None', 'Manual', 'Automatic', or 0-2.");

    return 0;
}


/*
 * user_agent_validate
 */
int
user_agent_validate(char* arg)
{
    char* ptr;

    if ( strlen(arg) > 10 ) {
        error("User Agent strings are limited to 10 characters or less.");
        return 0;
    }

    for ( ptr = arg; *ptr != '\0'; ptr++ ) {
        if ( !isalnum(*ptr) && *ptr != '-' && *ptr != '_' ) {
            error("User Agent strings consist of alphanumerics, '-', and '_'.");
            return 0;
        }
    }

    return 1;
}


/*
 * usage
 */
void
usage(char* progname)
{
    fprintf(stderr, "Usage: %s filename\n", progname);
}


/*
 * warn
 */
void
warn(char* msg)
{
    fprintf(stderr, "Warning: Line %d: %s\n", lineno, msg);
}


/*
 * error
 */
void
error(char* msg)
{
    fatal_errors++;
    fprintf(stderr, "Error: Line %d: %s\n", lineno, msg);
}


/*
 * is_blank
 * Returns True if the line has nothing on it except for whitespace.
 */
int
is_blank(char* line)
{
    assert(line);

    while ( *line && isspace(*line) ) line++;

    return ( *line == '\0' );
}


/*
 * is_comment
 * Returns True if the line is a comment.
 * Comments begin with the '#' character.
 * Comments can only appear on lines by themselves.
 */
int
is_comment(char* line)
{
    assert(line);

    while ( *line && isspace(*line) ) line++;

    return ( *line == '#' );
}


/*
 * is_section
 */
int
is_section(char* line)
{
    for ( ; isspace(*line); line++ ) ;

    if ( *line != '[' ) return 0;

    for ( ; *line != ']' && *line != '\0'; line++ ) ;

    if ( *line != ']' ) return 0;

    for ( line++; *line != '\0'; line++ ) if (!isspace(*line)) return 0;

    return 1;
}


/*
 * get_left_side
 */
char*
get_left_side(char* line)
{
    char* left_side;
    char* ptr;

    assert(line);

    if ( !strchr(line, '=') ) {
        error("Missing '='.");
        return NULL;
    }

    while ( *line && isspace(*line) ) line++;
    
    left_side = strdup(line);

    for ( ptr = strchr(left_side, '='); *ptr == '=' || isspace(*ptr); ptr-- ) {
        *ptr = '\0';
    }

    return left_side; 
}


/*
 * get_right_side
 */
char*
get_right_side(char* line)
{
    char* right_side;
    char* ptr;

    assert(line);

    for ( ptr = strchr(line, '=') + 1; isspace(*ptr); ptr++ ) ;

    right_side = ptr;

    if ( right_side[0] != '\0' ) {
        for ( ptr = strchr(right_side, '\0') - 1; isspace(*ptr); ptr-- ) {
            *ptr = '\0';
        }
    }

    if ( right_side[0] == '"' || *ptr == '"' ) {
        if ( right_side[0] == '"' && *ptr == '"' ) {
            right_side++;
            *ptr = '\0';
        } else {
            error("Unbalanced quotation mark.");
            return NULL;
        }
    }

    return strdup(right_side);
}


/*
 * default_handler
 */
void
default_handler(struct mapping* mapping, char* right_side)
{
    int i;
    assert(mapping);

    sprintf(buf, "Netscape%s: %s\n", mapping->resource, right_side);
    append_to_output_file(buf);

    for ( i = 0; mapping->desens[i] != NULL; i++ ) {
        sprintf(buf, "Netscape%s.sensitive: False\n", mapping->desens[i]);
        append_to_output_file(buf);
    }
}


/*
 * windoze_handler
 */
void
windoze_handler(struct mapping* mapping, char* right_side)
{
  static int warned = 0;

  if ( !warned ) {
    warn("Menu prompts are ignored on this platform.");
    warned = 1;
  }
}


/*
 * mac_handler
 */
void
mac_handler(struct mapping* mapping, char* right_side)
{
    warn("Ignoring rule used only on Macintosh platform.");
}


/*
 * boolean_handler
 * The right side has been validated, so it should only be
 * 'yes', 'no', 'true', or 'false'.
 */
void
boolean_handler(struct mapping* mapping, char* right_side)
{
    if ( matches(right_side, "true") || matches(right_side, "yes") ) {
        default_handler(mapping, "True");
    } else {
        default_handler(mapping, "False");
    }
}


/*
 * get_mnemonic
 */
char
get_mnemonic(char* str)
{
    char mnemonic = '\0';
    char* ptr;

    for ( ptr = str; *ptr != '\0'; ptr++, str++ ) {
        if ( *ptr == '&' ) {
            ptr++;
            if ( *ptr != '&' ) mnemonic = *ptr;
        }
        *str = *ptr;
    }

    *str = *ptr;

    return mnemonic;
}


/*
 * menu_handler
 */
void
menu_handler(struct mapping* mapping, char* right_side)
{
    char mnemonic = get_mnemonic(right_side);
    
    sprintf(buf, "Netscape%s.labelString: %s\n", mapping->resource, right_side);
    append_to_output_file(buf);

    if ( mnemonic ) {
        sprintf(buf, "Netscape%s.mnemonic: %c\n", mapping->resource, mnemonic);
        append_to_output_file(buf);
    }
}


/*
 * proxy_handler
 */
void
proxy_handler(struct mapping* mapping, char* right_side)
{
    int i;

    for ( i = 0; i < NUM_PROXIES; i++ ) {
        if ( strstr(mapping->token, proxies[i].name) ) {
            if ( strstr(mapping->token, "proxyport") ) {
                proxies[i].port = strdup(right_side);
            } else {
                proxies[i].host = strdup(right_side);
            }
        }
    }
}


/*
 * handle_rule
 */
void
handle_rule(struct mapping* mapping, char* line)
{
    char* right_side = get_right_side(line);

    if ( right_side == NULL ) return;

    if ( mapping->validate == NULL || mapping->validate(right_side) == 1 ) {
        assert(mapping->process);
        mapping->process(mapping, right_side);
    }

    free(right_side);
}


/*
 * matches
 */
int
matches(char* str1, char* str2)
{
    assert(str1);
    assert(str2);

    for ( ; *str1 && *str2; str1++, str2++ ) {
        if ( tolower(*str1) != tolower(*str2) ) return 0;
    }

    return ( *str1 == '\0' && *str2 == '\0' );
}


/*
 * process_input_line
 */
void
process_input_line(char* line)
{
    int i;
    char* left_side;

    if ( is_blank(line)   || 
         is_comment(line) ||
         is_section(line)    ) return;

    *strchr(line, '\n') = '\0';

    left_side = get_left_side(line);
    if ( left_side == NULL ) return;

    for ( i = 0; i < NUM_MAPPINGS; i++ ) {
        if ( matches(left_side, mappings[i].token) ) {
            handle_rule(&(mappings[i]), line);
            break;
        }
    }

    if ( i == NUM_MAPPINGS ) {
        sprintf(buf, "Unrecognized token '%s'.", left_side);
        error(buf);
    }

    free(left_side);
}


/*
 * append_to_output_file
 */
void
append_to_output_file(char* line)
{
    while ( output_buf == NULL || 
            strlen(output_buf) + strlen(line) >= output_max_size ) {
        output_max_size+= CHUNKSIZE;
        output_buf = (char*) realloc(output_buf, output_max_size);
    }

    strcat(output_buf, line);
}


/*
 * obscure
 */
void
obscure(char* buf)
{
    int i;
    static int offset = 0;
    int len = buf ? strlen(buf) : 0;

    for ( i = 0; i < len; i++, offset++ ) {
        buf[i]+= ( 40 + (offset % 10) );
    }
}


/*
 * write_hash
 */
void
write_hash(FILE* file)
{
    sprintf(buf, "! %s\n", XP_Md5PCPrintable(output_buf, strlen(output_buf)));

    if ( verbose ) {
        printf("Hash is '%s'", buf);
    }

    obscure(buf);

    if ( fwrite(buf, sizeof(char), strlen(buf), file) != strlen(buf) ) {
        perror("fwrite");
        exit(errno);
    }
}


/*
 * write_output_file
 */
void
write_output_file(void)
{
    FILE* output_file;

    if ( fatal_errors ) {
        fprintf(stderr, "No output file created due to error%s.\n", 
                         fatal_errors == 1 ? "" : "s");
        return;
    }

    if ( output_buf == NULL ) return;

    if ( (output_file = fopen(OUTPUT_FILENAME, "w")) == NULL ) {
        perror(OUTPUT_FILENAME);
        exit(errno);
    }

    write_hash(output_file);

    obscure(output_buf);

    if ( fwrite(output_buf, sizeof(char), strlen(output_buf), output_file) !=
         strlen(output_buf) ) {
        perror(OUTPUT_FILENAME);
        exit(errno);
    }

    if ( fclose(output_file) == -1 ) {
        perror(OUTPUT_FILENAME);
        exit(errno);
    }

    fprintf(stderr, "Wrote '%s'\n", OUTPUT_FILENAME);
}


/*
 * output_proxies
 */
void
output_proxies(void)
{
    int i;

    for ( i = 0; i < NUM_PROXIES; i++ ) {
        if ( proxies[i].host && proxies[i].port ) {
            sprintf(buf, "Netscape.%sProxy: %s:%s\n", proxies[i].resource,  
                                                     proxies[i].host,
                                                     proxies[i].port);
            append_to_output_file(buf);

            sprintf(buf, "Netscape*proxiesBox*%sText.sensitive: False\n", 
                         proxies[i].resource);
            append_to_output_file(buf);
            sprintf(buf, "Netscape*proxiesBox*%sPort.sensitive: False\n", 
                         proxies[i].resource);
            append_to_output_file(buf);

            continue;
        }
        if ( proxies[i].host && !proxies[i].port ) {
            sprintf(buf, "Specified %s without %s_port\n", proxies[i].name,
                                                           proxies[i].name);
            error(buf);
        }
        if ( !proxies[i].host && proxies[i].port ) {
            sprintf(buf, "Specified %s_port without %s\n", proxies[i].name,
                                                           proxies[i].name);
            error(buf);
        }
    }
}


/*
 * main
 */
int
main(int argc, char* argv[])
{
    FILE* file;
    char* filename;

    switch ( argc ) {
        case 2:
            verbose = 0;
            filename = argv[1];
            break;
#ifdef DEBUG
        case 3:
            if ( !strcmp(argv[1], "-v") ) {
                verbose = 1;
                filename = argv[2];
            } else {
                usage(argv[0]);
                exit(1);
            }
            break;
#endif
        default:
            usage(argv[0]);
            exit(1);
            break;
    }
 
    if ( (file = fopen(filename, "r")) == NULL ) {
        perror(filename);
        exit(errno);
    }

    append_to_output_file("! e-kit v2.0b1\n");

    for ( lineno = 1; fgets(buf, sizeof(buf), file) != NULL; lineno++ ) {
        if ( !strchr(buf, '\n') ) {
            warn("Line too long.  Ignored.");
        } else {
            process_input_line(buf);
        }
    }

    output_proxies();

    write_output_file();

    return 0;
}


#ifdef HPUX
/*
 * pull_in_MD5_HashBuf
 * Doesn't get linked in otherwise.
 * You can use this if the '-u MD5_HashBuf' flag
 * isn't supported.
 */
static void
pull_in_MD5_HashBuf(void)
{
    MD5_HashBuf(0, 0, 0);
}
#endif


