/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
	NADGrammarTypes.h

	Scott M. Silver
*/

#ifndef NADGRAMMARTYPES_H
#define NADGRAMMARTYPES_H

typedef struct
{
	char* 	name;		/* name of type */
	int		start;		/* beginning at which offset */
} ParameterType;

typedef struct
{
	ParameterType*	type;	/* must be non-NULL */
	char*			name;	/* can be NULL */
} Parameter;

typedef struct
{
	Parameter* param1;	/* can be NULL */
	Parameter* param2;	/* can be NULL */
} ParamList;

typedef struct
{
	ParamList*	paramList;	/* must be non-NULL */
	char*		returnVal;	/* must be non-NULL */
	char*		primitive;	/* must be non-NULL */
} Prototype;

typedef struct
{
	Prototype*	prototype;	/* must be non-NULL */
	int			cost;	
	int			hasCode;
} Rule;

extern ParameterType* 	makeParameterType(char* inTypeStr, int inStartOffset);
extern Parameter* 		makeParameter(ParameterType* inParamType, char* inNameStr);
extern ParamList* 		makeParamList(Parameter* inParam1, Parameter* inParam2);
extern Prototype* 		makePrototype(ParamList* inParamList, char* inReturnVal, char* inPrimitive);
extern Rule*  			makeRule(Prototype* inPrototype, int inCost, int inHasCode);
extern void				addRule(Rule* inNewRule);
extern char*			myParenStrCat(const char* inStr1, const char* inStr2);
extern char*			myCommaStrCat(const char* inStr1, const char* inStr2);

#ifdef __MWERKS__
char* 				strdup(const char* inString);
#endif

#endif /* NADGRAMMARTYPES_H */
