
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
/*								      CSParse.c
**	PICS CONFIGURATION MANAGER FOR CLIENTS AND SERVERS
**
**	(c) COPYRIGHT MIT 1995.
**	Please first read the full copyright statement in the file COPYRIGHT.
**
**	This module converts application/xpics streams (files or network) to PICS_ class data
**
** History:
**	 4 Dec 95  EGP  start
**	15 Feb 96  EGP  alpha 1
**
** BUGS: no code yet; doesn't actually do anything
*/

/* Library include files */
#include <stdarg.h>
#include "plstr.h"
/* #include "sysdep.h" jhines 7/9/97 */
#include "htchunk.h"
#include "htstring.h"
#include "cslutils.h"
#include "csparse.h"

PUBLIC int ParseDebug = 0;	/* For use with LablPars and RatPars */

PUBLIC PRBool BVal_readVal(BVal_t * pBVal, const char * valueStr)
{
    if (!PL_strcasecmp(valueStr, "true") || 
        !PL_strcasecmp(valueStr, "yes"))
        pBVal->state = BVal_YES;
    else if (PL_strcasecmp(valueStr, "false") && 
             PL_strcasecmp(valueStr, "no"))
        return NO;;
    pBVal->state |= BVal_INITIALIZED;
    return YES;
}

PUBLIC PRBool BVal_initialized(const BVal_t * pBVal)
{
    return (pBVal->state & BVal_INITIALIZED);
}

PUBLIC PRBool BVal_value(const BVal_t * pBVal)
{
    return ((pBVal->state & BVal_YES) ? 1 : 0);
}

PUBLIC void BVal_set(BVal_t * pBVal, PRBool value)
{
	if (value)
		pBVal->state = BVal_YES;
    pBVal->state |= BVal_INITIALIZED;
    return;
}

PUBLIC void BVal_clear(BVal_t * pBVal)
{
    if (pBVal)
      pBVal->state = BVal_UNINITIALIZED;
    return;
}

PUBLIC PRBool FVal_readVal(FVal_t * pFVal, const char * valueStr)
{
    if (!PL_strcasecmp(valueStr, "+INF")) {
        pFVal->stat = FVal_POSITIVE_INF;
        return YES;
    }
    if (!PL_strcasecmp(valueStr, "-INF")) {
        pFVal->stat = FVal_NEGATIVE_INF;
        return YES;
    }
    pFVal->stat = FVal_VALUE;
    sscanf(valueStr, "%f", &pFVal->value);
    return YES;
}

PUBLIC PRBool FVal_initialized(const FVal_t * pFVal)
{
    return (pFVal->stat != FVal_UNINITIALIZED);
}

PUBLIC float FVal_value(const FVal_t * pFVal)
{
    return (pFVal->value);
}

/* C U T T I N G   E D G E   M A T H   T E C H N O L O G Y   H E R E */
PRIVATE PRBool FVal_lessThan(const FVal_t * pSmall, const FVal_t * pBig)
{
    if (pBig->stat == FVal_UNINITIALIZED || pSmall->stat == FVal_UNINITIALIZED)
        return FALSE;
    if (pBig->stat == FVal_POSITIVE_INF || pSmall->stat == FVal_NEGATIVE_INF) {
        if (pSmall->stat == FVal_POSITIVE_INF)
            return FALSE;
        return TRUE;
    }
    if (pBig->stat == FVal_NEGATIVE_INF || pSmall->stat == FVal_POSITIVE_INF) {
        return FALSE;
    }
    return pSmall->value < pBig->value;
}

PUBLIC FVal_t FVal_minus(const FVal_t * pBig, const FVal_t * pSmall)
{
    FVal_t ret = FVal_NEW_UNINITIALIZED;
    /* no notion of 2 time infinity so please keep your limits to a minimum */
    if (pBig->stat == FVal_UNINITIALIZED || pSmall->stat == FVal_UNINITIALIZED)
        return ret;
    FVal_set(&ret, (float)0.0);
    if (pBig->stat == FVal_POSITIVE_INF || pSmall->stat == FVal_NEGATIVE_INF) {
        if (pSmall->stat != FVal_POSITIVE_INF)
            FVal_setInfinite(&ret, 0);
        return ret;
    }
    if (pBig->stat == FVal_NEGATIVE_INF || pSmall->stat == FVal_POSITIVE_INF) {
        if (pSmall->stat != FVal_NEGATIVE_INF)
            FVal_setInfinite(&ret, 0);
        return ret;
    }
    ret.value = pBig->value - pSmall->value;
    return (ret);
}

PUBLIC PRBool FVal_nearerZero(const FVal_t * pRef, const FVal_t * pCheck)
{
    if (pRef->stat == FVal_UNINITIALIZED || pCheck->stat == FVal_UNINITIALIZED || 
        pCheck->stat == FVal_POSITIVE_INF || pCheck->stat == FVal_NEGATIVE_INF)
        return NO;
    if (pRef->stat == FVal_POSITIVE_INF || pRef->stat == FVal_NEGATIVE_INF)
        return YES;
    if (pRef->value < 0.0) {
        if (pCheck->value < 0.0)
            return pCheck->value > pRef->value;
        return pCheck->value < -pRef->value;
    }
    if (pCheck->value < 0.0)
        return pCheck->value > -pRef->value;
    return pCheck->value < pRef->value;
}

PUBLIC PRBool FVal_isZero(const FVal_t * pFVal)
{
    if (pFVal->stat == FVal_VALUE && pFVal->value == 0.0)
        return YES;
    return NO;
}

PUBLIC void FVal_set(FVal_t * pFVal, float value)
{
    pFVal->value = value;
    pFVal->stat = FVal_VALUE;
}

PUBLIC void FVal_setInfinite(FVal_t * pFVal, PRBool negative)
{
    pFVal->stat = negative ? FVal_NEGATIVE_INF : FVal_POSITIVE_INF;
}

PUBLIC int FVal_isInfinite(const FVal_t * pFVal)
{
    return (pFVal->stat == FVal_POSITIVE_INF ? 1 : pFVal->stat == FVal_NEGATIVE_INF ? -1 : 0);
}

PUBLIC void FVal_clear(FVal_t * pFVal)
{
    if (pFVal)
      pFVal->stat = FVal_UNINITIALIZED;
    return;
}

PUBLIC char * FVal_toStr(FVal_t * pFVal)
{
    char * ptr;
    if ((ptr = (char *)HT_MALLOC(40)) == NULL)
	HT_OUTOFMEM("FVal buffer");
    sprintf(ptr, "%.1f", FVal_value(pFVal));
    return ptr;
}

PUBLIC PRBool SVal_readVal(SVal_t * pSVal, const char * valueStr)
{
    pSVal->initialized = YES;
    StrAllocCopy(pSVal->value, valueStr);
    return YES;
}

PUBLIC PRBool SVal_initialized(const SVal_t * pSVal)
{
    return (pSVal->initialized != NO);
}

PUBLIC char * SVal_value(const SVal_t * pSVal)
{
    return (pSVal->value);
}

PUBLIC void SVal_clear(SVal_t * pSVal)
{
    if (SVal_initialized(pSVal)) {
      HT_FREE(pSVal->value);
      pSVal->initialized = NO;
      }
    return;
}

#if 0
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int timeZoneHours;
    int timeZoneMinutes;
#endif
PUBLIC PRBool DVal_readVal(DVal_t * pDVal, const char * valueStr)
{
    char space[] = "1994.11.05T08:15-0500";
    char timeZoneSign;
    char timeZoneMinutesMSB;
    if (PL_strlen(valueStr) != 0x15)
        return NO;
    memcpy(space, valueStr, 0x15);
    timeZoneSign = space[16];
    timeZoneMinutesMSB = space[19];
    space[4] = space[7] = space[10] = space[13] = space[16] = space[19] = 0;
    pDVal->year = atoi(space);
    pDVal->month = atoi(space+5);
    pDVal->day = atoi(space+8);
    pDVal->hour = atoi(space+11);
    pDVal->minute = atoi(space+14);
    pDVal->timeZoneHours = atoi(space+17);
    space[19] = timeZoneMinutesMSB;
    pDVal->timeZoneMinutes = atoi(space+19);
    if (timeZoneSign == '-') {
        pDVal->timeZoneHours = -pDVal->timeZoneHours;
        pDVal->timeZoneMinutes = -pDVal->timeZoneMinutes;
    }
    StrAllocCopy(pDVal->value, valueStr);
    pDVal->initialized = YES;
    return YES;
}

PUBLIC PRBool DVal_initialized(const DVal_t * pDVal)
{
    return (pDVal->initialized != NO);
}

PUBLIC int DVal_compare(const DVal_t * a, const DVal_t * b)
{
    if (a->year > b->year) return 1;
    if (a->year < b->year) return -1;
    if (a->month > b->month) return 1;
    if (a->month < b->month) return -1;
    if (a->day > b->day) return 1;
    if (a->day < b->day) return -1;
    if (a->hour+a->timeZoneHours > b->hour+b->timeZoneHours) return 1;
    if (a->hour+a->timeZoneHours < b->hour+b->timeZoneHours) return -1;
    if (a->minute+a->timeZoneMinutes > b->minute+b->timeZoneMinutes) return 1;
    if (a->minute+a->timeZoneMinutes < b->minute+b->timeZoneMinutes) return -1;
    return 0;
}

PUBLIC char * DVal_value(const DVal_t * pDVal)
{
    return (pDVal->value);
}

PUBLIC void DVal_clear(DVal_t * pDVal)
{
    if (DVal_initialized(pDVal)) {
        HT_FREE(pDVal->value);
	pDVal->initialized = NO;
    }
    return;
}

PUBLIC char * Range_toStr(Range_t * pRange)
{
    HTChunk * pChunk;
    char * ptr;
    pChunk = HTChunk_new(20);
    ptr = FVal_toStr(&pRange->min);
    HTChunk_puts(pChunk, ptr);
    HT_FREE(ptr);
    if (FVal_initialized(&pRange->max)) {
        ptr = FVal_toStr(&pRange->max);
	HTChunk_puts(pChunk, ":");
	HTChunk_puts(pChunk, ptr);
	HT_FREE(ptr);
    }
    return HTChunk_toCString(pChunk);
}

/* Range_gap - find gap between 2 ranges. Either of these ranges may be a 
 * single value (in the min)
 * negative vector indicates that ref is greater than test
 */
PUBLIC FVal_t Range_gap(Range_t * a, Range_t * b)
{
    Range_t aN = *a;
    Range_t bN = *b;
    FVal_t ret = FVal_NEW_UNINITIALIZED;
    if (!FVal_initialized(&a->min) || !FVal_initialized(&b->min))
        return (ret);

    /* set ret for successful 0 returns */
    FVal_set(&ret, (float)0.0);

    /* normalize our ranges */
    if (FVal_lessThan(&aN.max, &aN.min)) {
        aN.max = a->min;
        aN.min = a->max;
    }
    if (FVal_lessThan(&bN.max, &bN.min)) {
        bN.max = b->min;
        bN.min = b->max;
    }

    /* check partial ranges (just a min, no max) */
    if (!FVal_initialized(&aN.max)) {
        if (!FVal_initialized(&bN.max))
            return FVal_minus(&aN.min, &bN.min);
        if (FVal_lessThan(&aN.min, &bN.min))
            return FVal_minus(&bN.min, &aN.min);
        if (FVal_lessThan(&bN.max, &aN.min))
            return FVal_minus(&bN.max, &aN.min);
        return ret;
    }
    /* we have four values to compare */
    {
        FVal_t minDif = FVal_minus(&bN.min, &aN.min);
        FVal_t maxDif = FVal_minus(&bN.max, &aN.max);
        Range_t common;
        common.min = FVal_lessThan(&bN.min, &aN.min) ? aN.min : bN.min;
        common.max = FVal_lessThan(&bN.max, &aN.max) ? bN.max : aN.max;
        if (!FVal_lessThan(&common.max, &common.min))
            return ret;
        /* failure - indicate how far we are off */
        return FVal_nearerZero(&minDif, &maxDif) ? minDif : maxDif;
    }
}

/* ------------------------------------------------------------------------- */

/* C O N S T R U C T O R S */
PUBLIC CSParse_t * CSParse_new(void)
{
    CSParse_t * me;
	if ((me = (CSParse_t *) HT_CALLOC(1, sizeof(CSParse_t))) == NULL)
	    HT_OUTOFMEM("CSParse");
    me->nowIn = NowIn_NEEDOPEN;
    me->token = HTChunk_new(0x10);
	if ((me->pParseContext = (ParseContext_t *) HT_CALLOC(1, sizeof(ParseContext_t))) == NULL)
	    HT_OUTOFMEM("ParseContext_t");
    return me;
}

PUBLIC void CSParse_delete(CSParse_t * me)
{
	HT_FREE(me->pParseContext);
	HTChunk_delete(me->token);
	HT_FREE(me);
}

/* L A B E L   P A R S E R S */
PRIVATE StateRet_t callErrorHandler(CSParse_t * pCSParse, 
				    const char * errorLocation,
				    char demark, StateRet_t errorCode)
{
    char * token = HTChunk_data(pCSParse->token);
    pCSParse->pParseContext->pTokenError = (char *)errorLocation;
    return (*pCSParse->pParseContext->pParseErrorHandler)(pCSParse, token, 
					      demark, StateRet_ERROR_BAD_CHAR);
}

/* CSParse_parseChunk - elemental parse engine for all pics nowIns. This passes 
 * tokenized data into the handler functions in the CSParse_t.handlersOf. These 
 * handlers are responsibel for placing the data in the appropriate target.
 * The text is broken into nowIns and passed a SubParser based on the current 
 * nowIn which is one of:
 *  NowIn_NEEDOPEN - get paren and go to NowIn_ENGINE, text is an error
 *  NowIn_ENGINE - in a containing structure, text goes to engineOf_
 *  NowIn_NEEDCLOSE - get paren and go to NowIn_ENGINE, text is an error
 *  NowIn_END - expect no more text or parens
 *  NowIn_ERROR - 
 */
PUBLIC CSDoMore_t CSParse_parseChunk (CSParse_t * pCSParse, const char * ptr, int len, void * pVoid)
{
    int i;
    if (!len || !ptr)
        return CSDoMore_error;
    for (i = 0; i < len; i++) {
        pCSParse->offset++;
        if (pCSParse->quoteState) {
            if (pCSParse->quoteState == ptr[i]) {
                pCSParse->quoteState = 0;
                pCSParse->demark = ' ';
            }
            else
                HTChunk_putb(pCSParse->token, ptr+i, 1);
            continue;
        }
        if (ptr[i] == SQUOTE || ptr[i] == DQUOTE) {
            if (pCSParse->demark) {
                while ((pCSParse->nowIn = (*pCSParse->pParseContext->engineOf)(pCSParse, ' ', pVoid)) == NowIn_CHAIN)
                	; /* */
                HTChunk_clear(pCSParse->token);
                pCSParse->demark = 0;
            } else if (HTChunk_size(pCSParse->token) && 
/*                  && warn(pCSParse, message_UNEXPECTED_CHARACTER, ptr[i])) */
		       callErrorHandler(pCSParse, ptr+i, ptr[i], 
					StateRet_ERROR_BAD_CHAR) !=StateRet_OK)
		    pCSParse->nowIn = NowIn_ERROR;
            pCSParse->quoteState = ptr[i];
			pCSParse->pParseContext->observedQuotes = YES;
            continue;
        }
        switch (pCSParse->nowIn) {
            case NowIn_NEEDOPEN:
                if (ptr[i] == LPAREN) {
                    pCSParse->nowIn = NowIn_ENGINE;
                    continue;
                }
                if (isspace(ptr[i]))
                    continue;
/*                if (warn(pCSParse, message_UNEXPECTED_CHARACTER, ptr[i])) pCSParse->nowIn = NowIn_ERROR; */
	        if (callErrorHandler(pCSParse, ptr+i, ptr[i], 
				     StateRet_ERROR_BAD_CHAR) !=StateRet_OK)
		    pCSParse->nowIn = NowIn_ERROR;
                continue;
            case NowIn_ENGINE:
                if (isspace(ptr[i])) {
                    if (HTChunk_size(pCSParse->token))
                        pCSParse->demark = ' ';
                    continue;
                }
                if (ptr[i] == LPAREN || ptr[i] == RPAREN || pCSParse->demark) {
		    /* parens override space demarkation */
		    if (ptr[i] == LPAREN) pCSParse->demark = LPAREN;
		    if (ptr[i] == RPAREN) pCSParse->demark = RPAREN;
		    /* call the engine as long as it wants re-entrance */
                    while ((pCSParse->nowIn = (*pCSParse->pParseContext->engineOf)(pCSParse, pCSParse->demark, pVoid)) == NowIn_CHAIN)
                    	; /* */
                    HTChunk_clear(pCSParse->token);
                    pCSParse->demark = 0;
                    if (ptr[i] == LPAREN || ptr[i] == RPAREN)
                        continue;
                    /* continue with next token */
                }
                HTChunk_putb(pCSParse->token, ptr+i, 1);
                continue;
            case NowIn_NEEDCLOSE:
                if (ptr[i] == RPAREN) {
                    pCSParse->nowIn = NowIn_ENGINE;
                    continue;
                }
                if (isspace(ptr[i]))
                    continue;
		if (callErrorHandler(pCSParse, ptr+i, ptr[i], 
				     StateRet_ERROR_BAD_CHAR) !=StateRet_OK)
		    pCSParse->nowIn = NowIn_ERROR;
/*                if (warn(pCSParse, message_UNEXPECTED_CHARACTER, ptr[i])) pCSParse->nowIn = NowIn_ERROR; */
                continue;
            case NowIn_END:
#if 0 /* enable this to tell the parser to check the remainder of 
		 the stream after the parsed object thinks it is done */
                if (isspace(ptr[i]))
                    continue;
/*                if (warn(pCSParse, message_UNEXPECTED_CHARACTER, ptr[i])) pCSParse->nowIn = NowIn_ERROR; */
		if (callErrorHandler(pCSParse, ptr+i, ptr[i], 
				     StateRet_ERROR_BAD_CHAR) !=StateRet_OK)
		    pCSParse->nowIn = NowIn_ERROR;
                continue;
#else
                return CSDoMore_done;
#endif
            case NowIn_MATCHCLOSE:
                if (ptr[i] == RPAREN) {
                    if (!pCSParse->depth)
                        pCSParse->nowIn = NowIn_ENGINE;
                    else
                        pCSParse->depth--;
                }
                if (ptr[i] == LPAREN)
                    pCSParse->depth++;
                continue;
            case NowIn_ERROR:
                return CSDoMore_error;
                break;
            default:
/*                if (warn(pCSParse, message_INTERNAL_ERROR, "bad nowIn")) pCSParse->nowIn = NowIn_ERROR; */
		HTTrace("PICS: Internal error in parser - bad nowIn:%d.\n", 
			pCSParse->nowIn);
		return CSDoMore_error;
        }
    }
    /* check completion */
    return pCSParse->nowIn == NowIn_END ? CSDoMore_done : CSDoMore_more;
}

PUBLIC PRBool Punct_badDemark(Punct_t validPunctuation, char demark)
{
    switch (demark) {
		case ' ': return (!(validPunctuation & Punct_WHITE));
		case LPAREN: return (!(validPunctuation & Punct_LPAREN));
		case RPAREN: return (!(validPunctuation & Punct_RPAREN));
	}
    return YES;
}

#if 0
PRIVATE void Input_dump(char * token, char demark)
{
    char space[256];
    sprintf(space, " %s |%c|\n", token, demark);
    HTTrace(space);
}
#endif
PRIVATE char * CSParse_subState2str(SubState_t subState)
{
    static char space[33];
    space[0] = 0;
    if (subState == SubState_N)
        PL_strcpy(space, "N");
    else if (subState == SubState_X)
        PL_strcpy(space, "X");
    else {
        int i;
	SubState_t comp;
	char ch[] = "A";
	for (i = 1, comp = SubState_A; i < (sizeof(SubState_t)*8 - 1); i++, (*ch)++, comp<<=1)
	    if (comp & subState)
	        PL_strcat(space, ch);
    }
    return space;
}

PRIVATE int ParseTrace(const char * fmt, ...)
{
    va_list pArgs;

    va_start(pArgs, fmt);

    if (!ParseDebug)
        return 0;
    return (vfprintf(stderr, fmt, pArgs));
}

PUBLIC NowIn_t CSParse_targetParser(CSParse_t * pCSParse, char demark, void * pVoid)
{
/*    ParseContext_t * pParseContext = pCSParse->pParseContext; */
    TargetObject_t * pTargetObject = pCSParse->pTargetObject;
    PRBool failedOnPunct = NO;
    char * token = 0;
    StateRet_t ret = StateRet_OK;
    int i;
static NowIn_t lastRet = NowIn_END;

	/* changed by montulli@netscape.com 11/29/97 
     * if (HTChunk_size(pCSParse->token)) {
     *   HTChunk_terminate(pCSParse->token);
     *   token = HTChunk_data(pCSParse->token);
     * }
	 */
	if(HTChunk_data(pCSParse->token))
	{
		HTChunk_terminate(pCSParse->token);
		token = HTChunk_data(pCSParse->token);
	}

	/*Input_dump(token, demark);*/
    for (i = 0; i < pTargetObject->stateTokenCount; i++) {
        StateToken_t * pStateToken = pTargetObject->stateTokens + i;
        pCSParse->pStateToken = pStateToken;

        if (!(pCSParse->currentSubState & pStateToken->validSubStates))
            continue;
        if (pStateToken->pCheck) {  /* use check function */
            StateRet_t checkRes;
            checkRes = (*pStateToken->pCheck)(pCSParse, pStateToken, token, demark);
            switch (checkRes) {
                case StateRet_WARN_BAD_PUNCT:
                    failedOnPunct = YES;
                case StateRet_WARN_NO_MATCH:
                    continue;
                case StateRet_ERROR_BAD_CHAR:
		    (*pCSParse->pParseContext->pParseErrorHandler)(pCSParse, token, demark, StateRet_ERROR_BAD_CHAR);
		    /*                    if (pTargetObject->pDestroy)
		        (*pTargetObject->pDestroy)(pCSParse); */
                    return NowIn_ERROR;
	        default:
		    break;
            }
        } else {                    /* or match by name[s] */
            if (!(pStateToken->command & Command_MATCHANY)) {
	        if (token && pStateToken->name1) {
        	    if (PL_strcasecmp(token, pStateToken->name1) && (!pStateToken->name2 || PL_strcasecmp(token, pStateToken->name2)))
       		        continue;
	        } else {
		    if (token != pStateToken->name1)
	                continue;
	        }
	    }
            if (Punct_badDemark(pStateToken->validPunctuation, demark)) {
                failedOnPunct = YES;
                continue;
            }
        }
/* open or close and do the appropriate callbacks */
	if (lastRet != NowIn_CHAIN)
	    ParseTrace("%30s %c ", token ? token : "", demark);
        ParseTrace("%10s - %s:%10s => ", pCSParse->pTargetObject->note, CSParse_subState2str(pCSParse->currentSubState), pStateToken->note);
	if (pStateToken->command & Command_NOTOKEN) {
	    HTChunk_clear(pCSParse->token);
		token = 0;
	}
	if (pStateToken->command & Command_OPEN && pTargetObject->pOpen)
	    if ((*pTargetObject->pOpen)(pCSParse, token, demark) == StateRet_ERROR)
			return NowIn_ERROR;
    
	if (pStateToken->command & (Command_OPEN|Command_CLOSE) && pCSParse->pParseContext->pTargetChangeCallback) {
	    ParseTrace("%3d", pStateToken->command & Command_CLOSE ? -(int)pTargetObject->targetChange : pTargetObject->targetChange);
	    if ((*pCSParse->pParseContext->pTargetChangeCallback)(pCSParse, pTargetObject, pTargetObject->targetChange, 
		(PRBool)(pStateToken->command & Command_CLOSE), pVoid) == StateRet_ERROR)
		return NowIn_ERROR;
	} else
	    ParseTrace("   ");
        if (pStateToken->command & Command_CLOSE && pTargetObject->pClose)
            ret = (*pTargetObject->pClose)(pCSParse, token, demark);

        if (pStateToken->pPrep && ret != NowIn_ERROR)
            ret = (*pStateToken->pPrep)(pCSParse, token, demark);
        if (pStateToken->pNextTargetObject)
            pCSParse->pTargetObject = pStateToken->pNextTargetObject;
        if (pStateToken->nextSubState != SubState_X)
            pCSParse->currentSubState = pStateToken->nextSubState;
/*
CSLabel_dump(pCSLabel);
HTTrace(pCSParse->pTargetObject->note);
*/
        ParseTrace("%10s - %s", pCSParse->pTargetObject->note, CSParse_subState2str(pCSParse->currentSubState));
        if (pStateToken->command & Command_CHAIN) {
	    ParseTrace(" -O-O-");
            return lastRet = NowIn_CHAIN;
	}
	ParseTrace("\n");
        return lastRet = ret == StateRet_ERROR_BAD_CHAR ? NowIn_ERROR : ret == StateRet_DONE ? NowIn_END : NowIn_ENGINE;
    }
    (*pCSParse->pParseContext->pParseErrorHandler)(pCSParse, token, demark, failedOnPunct ? StateRet_WARN_BAD_PUNCT : StateRet_WARN_NO_MATCH);
    if (pTargetObject->pDestroy)
	    (*pTargetObject->pDestroy)(pCSParse);
    return NowIn_ERROR;
}

