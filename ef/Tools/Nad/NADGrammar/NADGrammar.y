%{
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "NADGrammarTypes.h"
#include "NADOutput.h"
#include <string.h>
#include <assert.h>
#ifdef WIN32
#include <malloc.h>
int yylex(void);
int yyerror();
#endif
int gDebugLevel = 0;
#define kParseDebug 1
#define DPRINTF(inLevel, inParenthesizedString)		if (gDebugLevel >= (inLevel)) printf inParenthesizedString
#ifdef __MWERKS__
#pragma require_prototypes off
#endif
%}

%union {
	int cost;
	char* string;
	ParamList* paramList;
	Prototype* prototype;
	Parameter* parameter;
	ParameterType* paramType;
}

%token STRING NUMBER COST DERIVES CODEBLOCK LINEDIRECTIVE

%type <paramList> 	ParameterList
%type <prototype> 	PrototypeDescriptor
%type <parameter> 	ParameterDescriptor
%type <cost> 		CostDescriptor
%type <paramType> 	ParameterType
%type <string> 		CompoundParameterType
%type <cost> 		CodeSection
%type <string> 		InCompoundParameter

%token <string>		STRING
%token <cost>		NUMBER

%%
Line:					LINEDIRECTIVE
	|					Statement
	|					Line
	|					/* epsilon */	
	;
	
Statement: 				Line PrototypeDescriptor CostDescriptor CodeSection 	{ 
																					DPRINTF(kParseDebug, ("got Rule\n")); 
																					addRule(makeRule($2, $3, $4)); 
																					echoEmitDefinitionFooter(); 
																				}
	;		

CodeSection:			CODEBLOCK												{	$$ = 1; }
	|					/* epsilon */											{	printf("thisPrimitive;"); $$ = 0; }
	;
	
PrototypeDescriptor:	STRING DERIVES STRING '(' ParameterList ')' 			{ 
																					DPRINTF(kParseDebug, ("got Prototype\n")); 
																					$$ = makePrototype($5, $1, $3); 
																					echoEmitDefinitionHeader($$); 
																				} 
	;
	
ParameterList:			ParameterDescriptor ','	ParameterDescriptor 			{ 
																					DPRINTF(kParseDebug, ("got ParameterList2\n")); 
																					$$ = makeParamList($1, $3); 
																				}
	| 					ParameterDescriptor 									{ 	
																					DPRINTF(kParseDebug, ("got ParameterList1\n")); 
																					$$ = makeParamList($1, NULL); 
																				}
	| 					/* epsilon */											{ 
																					DPRINTF(kParseDebug, ("got ParameterList1\n")); 
																					$$ = makeParamList(NULL, NULL); 
																				}
	;

CostDescriptor:			COST '(' NUMBER ')' 									{ 
																					DPRINTF(kParseDebug, ("cost = %d\n", $3)); 
																					$$ = $3; 
																				}
	| 					/* epsilon */											{ 
																					DPRINTF(kParseDebug, ("default cost 0\n")); 
																					$$ = 0; 
																				}
	;	
	
ParameterDescriptor: 	ParameterType STRING									{ 
																					DPRINTF(kParseDebug, ("got ParameterDescriptor2\n")); 
																					$$ = makeParameter($1, $2); 
																				}
	|					ParameterType											{
																					DPRINTF(kParseDebug, ("got ParameterDescriptor1\n")); 
																					$$ = makeParameter($1, NULL);
																				}
	;

ParameterType:			CompoundParameterType '[' NUMBER ']'					{ 	$$ = makeParameterType($1, $3); }
	|					CompoundParameterType									{	$$ = makeParameterType($1, 0);	}
	;

CompoundParameterType:	STRING '(' InCompoundParameter ')'						{	$$ = myParenStrCat($1, $3); }
	|					STRING													{	$$ = $1; }
	;
	
InCompoundParameter:	STRING ',' STRING										{	$$ = myCommaStrCat($1, $3); }
	|					STRING													{	$$ = $1; }
	|					CompoundParameterType									{	$$ = $1; }
	;
%%

#ifdef __MWERKS__
#pragma require_prototypes reset
#endif


