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
	NADOutput.c

	Scott M. Silver
*/

#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <va_list.h>
#endif
#include <stdarg.h>
#include "NADOutput.h"
#include "NADGrammarTypes.h"
#include <time.h>
#include <assert.h>

extern FILE*	yyout;
extern char*	gEmitterName;

static void echoYYOut(const char* inFormat, ...);
static void fprintBurgRule(FILE* inFile, Rule* inRule, int inRuleNo);
static void fprintCase(FILE* infile, Rule* inRule, int inRuleNo);
static void	fprintCaseBegin(FILE* inFile);
static void fprintCaseEnd(FILE* inFile);
static void fprintDeclaration(FILE* inFile, Rule* inRule, int inRuleNo);
static void fprintRuleHeader(FILE* inFile, Prototype* inPrototype, int inRuleNo, int inDefinition);


extern int gNextRuleNumber;		/* next rule number */

void
echoEmitDefinitionHeader(Prototype* inPrototype)
{
	fprintRuleHeader(yyout, inPrototype, gNextRuleNumber++, 1);	
	echoYYOut("\n{");
}

void
echoEmitDefinitionFooter()
{
	echoYYOut("}\n");
}

/*
	fprintBurgRule
	
	Recreate the actual "BURG" style rule 
*/
extern Rule**	gRules;
extern Rule**	gNextRule;
extern int		gFirstRuleNumber;

void fprintRules(FILE* inBurgFile, FILE* inEmitterFile, FILE* inEmitterHeaderFile)
{
	Rule**	curRule;
	int ruleNo;
	
	ruleNo = gFirstRuleNumber;
	fprintCaseBegin(inEmitterFile);
	curRule = (Rule**)&gRules;	/* maybe I just don't understand C */
	for (; curRule < gNextRule; curRule++, ruleNo++)
	{
		fprintBurgRule(inBurgFile, *curRule, ruleNo);
		fprintCase(inEmitterFile, *curRule, ruleNo);
		fprintDeclaration(inEmitterHeaderFile, *curRule, ruleNo);
	}
	fprintCaseEnd(inEmitterFile);
}

void
fprintBurgRule(FILE* inFile, Rule* inRule, int inRuleNo)
{
	Prototype*	prototype = inRule->prototype;
	fprintf(inFile, "%s: %s", prototype->returnVal, prototype->primitive);
	if (prototype->paramList->param1)
	{
		fprintf(inFile, "(%s", prototype->paramList->param1->type->name);
	
		if (prototype->paramList->param2)
			fprintf(inFile, ", %s)", prototype->paramList->param2->type->name);
		else
			fprintf(inFile, ")");
	}
	else
		assert(!prototype->paramList->param2); /* must have param1 */
	
	fprintf(inFile, " = %d (%d);\n", inRuleNo, inRule->cost);
}

void
fprintCaseBegin(FILE* inFile)
{
	fprintf(inFile, "void %s::\n"
					"emitPrimitive(Primitive& inPrimitive, int inRule)\n"
					"{\n"
					"\tswitch (inRule)\n"
					"\t{\n", gEmitterName);
}


void
fprintCaseEnd(FILE* inFile)
{
	fprintf(inFile, "\t}\n\n"
					"}\n");
}

void
fprintCase(FILE* inFile, Rule* inRule, int inRuleNo)
{
	Prototype*	prototype = inRule->prototype;
	int			curInput;

	if (inRule->hasCode)
	{	
		if (inRule->prototype->paramList->param1)
			curInput = inRule->prototype->paramList->param1->type->start;
		else
			curInput = 0;

		fprintf(inFile, "\t\tcase%4d: ", inRuleNo);
		fprintf(inFile, "\t\t\temitRule%d(inPrimitive", inRuleNo);
		if (prototype->paramList->param1)
			fprintf(inFile, ", inPrimitive.nthInputVariable(%d)", curInput++);
			
		if (prototype->paramList->param2)
			fprintf(inFile, ", inPrimitive.nthInputVariable(%d)", curInput++);

		fprintf(inFile, "); break;\n");
	}
}


void
fprintDeclaration(FILE* inFile, Rule* inRule, int inRuleNo)
{
	Prototype*	prototype = inRule->prototype;
	
	fprintf(inFile, "\t");
	fprintRuleHeader(inFile, prototype, inRuleNo, 0);
	fprintf(inFile, ";\n");
}

void
fprintRuleHeader(FILE* inFile, Prototype* inPrototype, int inRuleNo, int inDefinition)
{
	int unused = 0;
	
	if (inDefinition)
		fprintf(inFile, "void %s::\n", gEmitterName);
	else
		fprintf(inFile, "void\t");
			
	fprintf(inFile, "emitRule%d(Primitive& thisPrimitive", inRuleNo);

	if (inPrototype->paramList->param1)
	{
		fprintf(inFile, ", ");
		if (inPrototype->paramList->param1->name)
			fprintf(inFile, "DataNode& %s", inPrototype->paramList->param1->name);
		else
			fprintf(inFile, "DataNode& /*inUnused%d*/", unused++);
	}
	
	if (inPrototype->paramList->param2)
	{
		fprintf(inFile, ", ");
		if (inPrototype->paramList->param2->name)
			fprintf(inFile, "DataNode& %s", inPrototype->paramList->param2->name);
		else
			fprintf(inFile, "DataNode& /*inUnused%d*/", unused++);
	}
	fprintf(inFile, ")");
}

void
fprintGeneratedHeader(FILE* inFile, const char* inFileName)
{
	time_t	currentTime;
	
	time(&currentTime);
	fprintf(inFile, "// %s\n" 
					"//\n"
					"// Generated File. Do not edit.\n"
					"//\n"
					"// %s\n", inFileName, ctime(&currentTime));
}


/*
	echoYYOut
	
	Print stuff to stdout
*/
static void
echoYYOut(const char* inFormat, ...)
{
	va_list ap;
	
	va_start(ap, inFormat);
	vfprintf(yyout, inFormat, ap);		
}



