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

%option yylineno

%{
/*
 * includes and local declaration.
 */

#include "Fundamentals.h"
#include "Primitives.h"
#include "Value.h"
#include "GraphBuilder.h"
#include "cstubs.h"
#include "Parser.h"

#define yywrap()			  1
#define YY_DECL			      int yylex(void *yylval)
#define YYLVAL			      ((YYSTYPE *) yylval)
#define returnPrimitive(op)   YYLVAL->yyPrimitiveOperation = (op); return PRIMITIVE;
#define returnKind(kind)      YYLVAL->yyValueKind = (kind); return KIND;

static char *extractLabelName(char *label);

%}

label                        [[:alnum:]]+":"
space                        [ \r\t]+
number                       [0-9]+
string                       [[:alnum:]]+
right_assignment             "->"
left_assignment              ":="
comma                        ","
parenthesis                  "("|")"
at                           "@"


%x comment ccomment nil

%%

{right_assignment}           {return RIGHT_ASSIGNMENT;}
{left_assignment}            {return LEFT_ASSIGNMENT;}
{comma}                      {return yytext[0];}
{parenthesis}                {return yytext[0];}
{at}                         {return yytext[0];}

"_I"                         {returnKind(vkInt);}

"Arg"                        {returnPrimitive(poArg_I)}
"Result"                     {returnPrimitive(poResult_I)}
"Const"                      {returnPrimitive(poConst_I)}
"Mul"                        {returnPrimitive(poMul_I)}
"Add"                        {returnPrimitive(poAdd_I)}
"Sub"                        {returnPrimitive(poSub_I)}
"And"                        {returnPrimitive(poAnd_I)}
"Or"                         {returnPrimitive(poOr_I)}
"Xor"                        {returnPrimitive(poXor_I)}
"Cmp"                        {returnPrimitive(poCmp_I)}
"IfLt"                       {returnPrimitive(poIfLt)}
"IfEq"                       {returnPrimitive(poIfEq)}
"IfLe"                       {returnPrimitive(poIfLe)}
"IfGt"                       {returnPrimitive(poIfGt)}
"IfNe"                       {returnPrimitive(poIfNe)}
"IfGe"                       {returnPrimitive(poIfGe)}
"St"                         {returnPrimitive(poSt_I)}
"Branch"                     {returnPrimitive((PrimitiveOperation)nPrimitiveOperations)}
"Phi"                        {return PHI;}

{string}:                    {YYLVAL->yyString = extractLabelName(yytext); return LABEL;}

{number}                     {sscanf(yytext, "%d", &YYLVAL->yyValue.i); return NUMBER;}
#{number}                    {sscanf(yytext + 1, "%d", &YYLVAL->yyValue.i); return NUMBER;}
{string}                     {YYLVAL->yyString = strdup(yytext); return STRING;}


{space}+
"/*"                         BEGIN(comment);
"//"                         BEGIN(ccomment);
\n                           return '\n';
.

<comment>[^*]*		
<comment>"*"+[^*/]*   
<comment>"*"+"/"			 BEGIN(INITIAL);

<ccomment>.
<ccomment>\n				 { 
							   BEGIN(INITIAL); return '\n'; 
							 }

<nil>.						 { /* to quiet the compiler. */
							   return yytext[0]; 
							   yyunput(0, NULL); 
							   yy_flex_realloc(0, 0); 
							   REJECT;
							 }
%%

static char *
extractLabelName(char *label)
{
  uint32 size = strlen(label);
  char *ret = (char *) malloc(size);
  strncpy(ret, label, size - 1);
  ret[size - 1] = '\0'; 
  return ret;
}
