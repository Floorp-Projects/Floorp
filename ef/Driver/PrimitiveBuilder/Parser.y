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

%{
#include <stdlib.h>

#include "Fundamentals.h"
#include "Primitives.h"
#include "GraphBuilder.h"
#include "Value.h"
#include "Vector.h"
#include "cstubs.h"

#define YYPARSE_PARAM gb
#define builder (*(GraphBuilder *)gb)
%}

%pure_parser

%union 
{
  char                         *yyString;
  Value                         yyValue;
  ValueKind                     yyValueKind;
  PrimitiveOperation            yyPrimitiveOperation;
  Vector<ConsumerOrConstant *> *yyConsumerVector;
  ConsumerOrConstant           *yyConsumer;
  char                         *yyProducer;
};


%token <yyPrimitiveOperation> PRIMITIVE
%token LEFT_ASSIGNMENT
%token RIGHT_ASSIGNMENT
%token <yyString> STRING
%token <yyValue> NUMBER
%token <yyValueKind> KIND
%token <yyString> LABEL
%token PHI

%type <yyConsumer> arg
%type <yyConsumerVector> args
%type <yyProducer> result
%type <yyValueKind> kind
%type <yyString> phi

%%

input: /* empty */
     | input line
;

line: '\n'                 
{
}
   | label '\n'
{
}
   | instruction '\n'
{
}
   | label instruction '\n'
{
}
   | error '\n'           
{
  yyerrok;
}
     ;

label: LABEL
{
  builder.defineLabel($1);
}
;

instruction: PRIMITIVE kind args RIGHT_ASSIGNMENT result 
{
  builder.buildPrimitive($1, *$3, $5);
  delete $3; // delete the vector
  free($5);
}
           | PHI kind args RIGHT_ASSIGNMENT result
{
  char *var = builder.getPhiVariable(*$3);
  builder.aliasVariable(var, $5);
  delete $3; // delete the vector
  free($5);
  free(var);
}
           | result LEFT_ASSIGNMENT PRIMITIVE kind args
{
  builder.buildPrimitive($3, *$5, $1);
  delete $5; // delete the vector
  free($1);
}
           | result LEFT_ASSIGNMENT PHI kind args
{
  char *var = builder.getPhiVariable(*$5);
  builder.aliasVariable(var, $1);
  delete $5; // delete the vector
  free($1);
  free(var);
}
           | PRIMITIVE kind args
{
  builder.buildPrimitive($1, *$3);
  delete $3; // delete the vector
}
           ;

kind: /* empty */
{
  $$ = vkInt;
}
    | KIND
{
  $$ = $1
}
    ;

args:
{
  $$ = new Vector<ConsumerOrConstant *>();
} 
    | arg
{
  $$ = new Vector<ConsumerOrConstant *>();
  $$->append($1);
}
    | args ',' arg
{
  $$ = $1;
  $$->append($3);
}
    ;

arg: STRING
{
  $$ = new ConsumerOrConstant($1);
  free($1);
}
   | NUMBER
{
  $$ = new ConsumerOrConstant($1);
}
   | phi
{
  $$ = new ConsumerOrConstant($1);
  free($1);
}

result: STRING
{
  $$ = $1;
}

phi: PHI '(' args ')'
{
  $$ = builder.getPhiVariable(*$3);
}
   | PHI '(' args ')' '@' STRING
{
  $$ = builder.getPhiVariable(*$3, $6);
}

%%

int
yyerror(char *string)
{
  fprintf (stderr, "%s at line %d\n", string, yylineno);
  exit(EXIT_FAILURE);
  return 0;
}
