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
/*
	NADGrammarTypes.c

	Scott M. Silver
*/

#include "NADGrammarTypes.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


/*
	makes
	"inStr1, inStr2"
*/
extern char*
myCommaStrCat(const char* inStr1, const char* inStr2)
{
	size_t	len = strlen(inStr1) + strlen(inStr2);
	char*	rv;
	
	rv = (char*) malloc(len + 1 + 2);	/* plus 2 for ", " */
	assert(rv);
	
	sprintf(rv, "%s, %s", inStr1, inStr2);
	
	return (rv);
}


/*
	makes
	"inStr1(inStr2)"

*/
extern char*
myParenStrCat(const char* inStr1, const char* inStr2)
{
	size_t	len = strlen(inStr1) + strlen(inStr2);
	char*	rv;
	
	rv = (char*) malloc(len + 1 + 2);	/* plus 2 for two parentheses */
	assert(rv);
	
	sprintf(rv, "%s(%s)", inStr1, inStr2);
	
	return (rv);
}


/*
	assume we do NOT have to make copies of any passed in arguments
*/
extern ParameterType*
makeParameterType(char* inTypeStr, int inStartOffset)
{
	ParameterType*	rv;
	
	assert(inTypeStr);
	rv = (ParameterType*) malloc(sizeof(ParameterType));
	assert(rv);
	
	rv->name = inTypeStr;
	rv->start = inStartOffset;
	
	return (rv);
}

/*
	assume we do NOT have to make copies of any passed in arguments
*/
extern Parameter*
makeParameter(ParameterType* inType, char* inNameStr)
{
	Parameter*	rv;
	
	assert(inType);
	rv = (Parameter*) malloc(sizeof(Parameter));
	assert(rv);
	
	rv->type = inType;
	rv->name = inNameStr;
	
	return (rv);
}


/*
	assume we do NOT have to make copies of any passed in arguments
*/
extern ParamList*
makeParamList(Parameter* inParam1, Parameter* inParam2)
{
	ParamList*	rv;
	
	rv = (ParamList*) malloc(sizeof(ParamList));
	assert(rv);
	
	rv->param1 = inParam1;
	rv->param2 = inParam2;
	
	return (rv);
}

/*
	assume we do NOT have to make copies of any passed in arguments
*/
extern Prototype* 
makePrototype(ParamList* inParamList, char* inReturnVal, char* inPrimitive)
{
	Prototype*	rv;
	
	rv = (Prototype*) malloc(sizeof(Prototype));
	assert(rv);
	
	rv->paramList = inParamList;
	rv->returnVal = inReturnVal;
	rv->primitive = inPrimitive;
	
	return (rv);
}


/*
	assume we do NOT have to make copies of any passed in arguments
*/
extern Rule* 
makeRule(Prototype* inPrototype, int inCost, int inHasCode)
{
	Rule*	rv;
	
	rv = (Rule*) malloc(sizeof(Rule));
	assert(rv);
	
	rv->prototype = inPrototype;
	rv->cost = inCost;
	rv->hasCode = inHasCode;
	
	return (rv);
}


/* this limit is arbitrary */
#define kMaximumRules 1000
Rule*	gRules[kMaximumRules];
Rule**	gNextRule = gRules;

extern void
addRule(Rule* inRule)
{
	assert (gNextRule < gRules + kMaximumRules);
	*gNextRule++ = inRule;
}
	
#ifdef __MWERKS__
char* 
strdup(const char* inString)
{
	char* rv;
	
	assert(inString);
	rv = (char*) malloc(strlen(inString) + 1);
	assert(rv);
	
	return strcpy(rv, inString);
}
#endif
