
/*  W3 Copyright statement 
Copyright 1995 by: Massachusetts Institute of Technology (MIT), INRIA</H2>

This W3C software is being provided by the copyright holders under the
following license. By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with the
following terms and conditions: 

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee or royalty is hereby
granted, provided that the full text of this NOTICE appears on
<EM>ALL</EM> copies of the software and documentation or portions
thereof, including modifications, that you make. 

<B>THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF EXAMPLE,
BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
OR DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used
in advertising or publicity pertaining to the software without
specific, written prior permission.  Title to copyright in this
software and any associated documentation will at all times remain
with copyright holders. 
*/
/* #include "malloc.h" */
#include "prmem.h"
/* #include "sysdep.h"  -- jhines 7/9/97 */
#include "cslutils.h"
#include "csll.h"	/* to define states in stateChange */
#include "csparse.h"

int Total;

extern int ParseDebug;

extern int SEC_ERROR_NO_MEMORY;


PRIVATE
CSError_t spit(char* text, CSLabel_t * pCSMR, PRBool closed)
{
    printf("%s %s\n", text, closed ? "closed" : "opened");
    return CSDoMore_more;
}

LabelTargetCallback_t targetCallback;
StateRet_t targetCallback(CSLabel_t * pCSMR, CSParse_t * pCSParse, CSLLTC_t target, PRBool closed, void * pVoid)
{
    int change = closed ? -target : target;

    Total += change;
    if (!ParseDebug)
        printf("%3d ", change);
/*	printf("%s %s (%d)\n", closed ? "  ending" : "starting", pCSParse->pParseState->note, closed ? -target : target); */
    return StateRet_OK;
}

/* LLErrorHandler_t parseErrorHandler; */
StateRet_t parseErrorHandler(CSLabel_t * pCSLabel, CSParse_t * pCSParse, 
			     const char * token, char demark, 
			     StateRet_t errorCode)
{
    char space[256];
    printf("%20s - %s:", pCSParse->pTargetObject->note, 
	   pCSParse->currentSubState == SubState_X ? "SubState_X" : 
	   pCSParse->currentSubState == SubState_N ? "SubState_N" : 
	   pCSParse->currentSubState == SubState_A ? "SubState_A" : 
	   pCSParse->currentSubState == SubState_B ? "SubState_B" : 
	   pCSParse->currentSubState == SubState_C ? "SubState_C" : 
	   pCSParse->currentSubState == SubState_D ? "SubState_D" : 
	   pCSParse->currentSubState == SubState_E ? "SubState_E" : 
	   pCSParse->currentSubState == SubState_F ? "SubState_F" : 
	   pCSParse->currentSubState == SubState_G ? "SubState_G" : 
	   pCSParse->currentSubState == SubState_H ? "SubState_H" : 
	   "???");
    switch (errorCode) {
        case StateRet_WARN_NO_MATCH:
            if (token)
	        sprintf(space, "Unexpected token \"%s\".\n", token);
	    else
	        sprintf(space, "Unexpected lack of token.\n");
            break;
        case StateRet_WARN_BAD_PUNCT:
            sprintf(space, "Unexpected punctuation \"%c\"", demark);
	    if (token)
	        printf("after token \"%s\".\n", token);
	    else
	        printf(".\n");
            break;
        case StateRet_ERROR_BAD_CHAR:
            sprintf(space, "Unexpected character \"%c\" in token \"%s\".\n", 
		    *pCSParse->pParseContext->pTokenError, token);
            break;
        default:
            sprintf(space, "Internal error: demark:\"%c\" token:\"%s\".\n", 
		    demark, token);
            break;
    }
    printf(space);
/*
    CSLabel_dump(pCSMR);
    HTTrace(pParseState->note);
*/
  return errorCode;
}

/* #if 1 */
#if 0
/* use this main to test input with a series of labels, each on a line. */
int main(int argc, char** argv)
{
    char lineBuf[512];
    CSParse_t * pCSParse = 0;
    CSDoMore_t last = CSDoMore_done;
    FILE * input;

    if (argc > 1) {
        if ((input = fopen(argv[1], "r")) == NULL) {
	    printf("Couldn't open \"%s\".\n", argv[1]);
	    exit(1);
	}
    } else {
	input = stdin;
    }
    if (argc > 2)
        ParseDebug = 1;
    pCSParse = CSParse_newLabel(&targetCallback, &parseErrorHandler);
    while (fgets(lineBuf, sizeof(lineBuf), input)){
        int len;
	char * ptr;
	for (ptr = lineBuf; *ptr; ptr++)
	    if (*ptr == ';') {
	        *ptr = 0;
	        break;
	    }
/*	if (strchr(lineBuf, ';'))
	    *ptr = 0;
        if (lineBuf[0] == ';')
	    continue; */
	len = PL_strlen(lineBuf);
	if (lineBuf[len - 1] == '\r' || lineBuf[len - 1] == '\n') {
	    lineBuf[len-- - 1] = 0;
	}
	if (!lineBuf[0]) {
	    if (last != CSDoMore_done)
	        printf("parsing end error\n");
	    if (pCSParse)
	        CSParse_deleteLabel(pCSParse);
	    pCSParse = CSParse_newLabel(&targetCallback, &parseErrorHandler);
	    last = CSDoMore_done;
	} else {
	    printf("%s ", lineBuf); if (ParseDebug) printf("\n");
	    switch (last = CSParse_parseChunk(pCSParse, lineBuf, (int) PL_strlen(lineBuf), 0)) {
	    case CSDoMore_done:
	        printf("= %d - parsing end\n", Total);
		break;
	    case CSDoMore_error:
		printf("= %d - parsing error\n", Total);
		exit (1);
	    case CSDoMore_more:
		printf("\n");
		break;
	    }
	}
    }
    if (pCSParse)
        CSParse_deleteLabel(pCSParse);
    if (last != CSDoMore_done)
        printf("parsing end error\n");
    return (0);
}

/* #else */
#endif
#if 0

/* use this main to test input of a label list spread out over multiple lines*/
int main(int argc, char** argv)
{
    char lineBuf[512];
    while (gets(lineBuf)){
        CSParse_t * pCSParse;
	if (lineBuf[0] == ';')
	    continue;
	Total = 0;
	printf("%s", lineBuf);
        pCSParse = CSParse_newLabel(&targetCallback, &parseErrorHandler);
        if (CSParse_parseChunk(pCSParse, lineBuf, 
			       (int)PL_strlen(lineBuf), 0) != CSDoMore_done) {
	    printf("parsing end error\n");
	    break;
	}
    CSParse_deleteLabel(pCSParse);
	printf("= %d\n", Total);
    }
    return (0);
}
#endif

#if 0 /* a not-needed but often useful sample implementation of HTTrace */
int HTTrace(const char * fmt, ...)
{
    va_list pArgs;
    va_start(pArgs, fmt);
    return (vfprintf(stderr, fmt, pArgs));
}
#endif
void * HTMemory_malloc (size_t size)
{
    return PR_Malloc(size);
}

void * HTMemory_calloc (size_t nobj, size_t size)
{
    return PR_Calloc(nobj, size);
}

void * HTMemory_realloc (void * p, size_t size)
{
    return PR_Realloc(p, size);
}

void HTMemory_free (void * ptr)
{
    PR_Free(ptr);
}

void HTMemory_outofmem (char * name, char * file, unsigned long line)
{
    HTTrace("%s:%ld failed allocation for \"%s\".\n\
Program aborted.\n",
	     file, line, name);
    /* exit(1); */
   /*  XP_SetError( SEC_ERROR_NO_MEMORY ); */
	exit(1);
    return;
}

