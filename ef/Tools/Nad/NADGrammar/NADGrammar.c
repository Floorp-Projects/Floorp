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
	NADGrammar.c

	Scott M. Silver
*/

#ifdef NADGRAMMAR_APP
/* Macintosh cruft for setting stdin */
#include <console.h>
#endif

#include <stdio.h>
#include <assert.h>
#include "NADOutput.h"
#include <stdlib.h>
#include <string.h>

extern int lineno;
extern FILE* yyout;

char*	gEmitterName = "PPCEmitter";
int		gNextRuleNumber;
int 	gFirstRuleNumber;
extern int yyparse();

/*
	[emittername] [emitterfile] [burg rule file] [first burg rule num]
	PPCEmitter PPCEmitterRules PPC.burg 4
*/

main(int argc, char** argv)
{
	extern int yydebug;
	extern int gDebugLevel;

//	yydebug = 0;
	gDebugLevel = 0;		/* set to 1 for interesting results */
	
#ifdef NADGRAMMAR_APP
	/* Macintosh cruft for setting stdin */
	argc = ccommand(&argv);
#endif

	if (argc < 4)
	{	
		fprintf(stderr, "Atleast 3 arguments expected, got %d\n"
						"Usage:\n"
						"NADGrammar	EmitterObjectName GlueFilePrefix BurgRulesFile\n"
						"			<stdin> is the input NAD file\n"
						"			<stdout> is the output of the implementation file\n"
						"example\n"
						"	NADGrammar	PPCEmitter PPCEmitterGlue PPCSpecificRules > PPCEmitterImpl.cpp\n"
						, argc);

		return -1;
	}
	gEmitterName = argv[1];
	gNextRuleNumber = 1;

	fprintGeneratedHeader(stdout, gEmitterName);

	yyparse();
	
	{
		char*	rulesFn = argv[3];
		char*	implFn = (char*) malloc(strlen(argv[2]) + 1 + 4);	/* + ".cpp" */
		char*	headerFn = (char*) malloc(strlen(argv[2]) + 1 + 2);	/* + ".h" */		
		sprintf(implFn, "%s.cpp", argv[2]);
		sprintf(headerFn, "%s.h", argv[2]);
		assert(rulesFn && implFn && headerFn);
		
		{
			FILE* 	rulesFp 	= fopen(rulesFn, "w");
			FILE*	emImplFp	= fopen(implFn, "w");
			FILE*	emHeaderFp	= fopen(headerFn, "w");
			
			assert(rulesFp && emImplFp && emHeaderFp);
			
			fprintGeneratedHeader(emImplFp, implFn);
			fprintGeneratedHeader(emHeaderFp, headerFn);
			fprintRules(rulesFp, emImplFp, emHeaderFp);
		}
	}
	
	return 0;
}

int
yyerror();

extern char* yytext;

int
yyerror()
{
	fprintf(stderr, "%s", yytext);
	fprintf(stderr, "Parse error line: %d\n", lineno);
	exit(-1);
	return (-1);
}
