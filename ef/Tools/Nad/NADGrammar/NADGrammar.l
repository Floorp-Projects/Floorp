%{
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "NADGrammarTypes.h"
#include "NADGrammar.tab.h"
#include <string.h>
#include <stdlib.h>
int lineno = 1;
int leaveblock;
int yylex(void);
int isatty(int);
int fileno(FILE*);
%}

%s PERCENT_BRACE
%s COMMANDS
%s STUFF_MODE

%%
%{
	BEGIN COMMANDS;
%}
<COMMANDS>#[ ]*line.*						{ ECHO; return LINEDIRECTIVE;  }
<COMMANDS>\<-								{ return DERIVES; }
<COMMANDS>cost								{ return COST; }
<COMMANDS>[0-9]+							{ yylval.cost = atoi(yytext); return NUMBER; }
<COMMANDS>[ \t]								; 				/* ignore whitespace */
<COMMANDS>\n?\r								{ lineno++; }	/* ignore end of lines */
<COMMANDS>[A-Za-z][A-Za-z0-9_]*				{ yylval.string = strdup(yytext); return STRING; }
<COMMANDS>.									{ return yytext[0]; }
<COMMANDS>"%{"								{ leaveblock = 0; BEGIN(PERCENT_BRACE);}
<PERCENT_BRACE>"%}"							{ BEGIN(COMMANDS); return CODEBLOCK;}
<PERCENT_BRACE>.							{ ECHO; }
<PERCENT_BRACE>\n?\r						{ 
												lineno++; 
												ECHO;
												if (leaveblock == 1)
													BEGIN COMMANDS;
											}
%% 

yywrap()
{
	return 1;
}