/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifdef STANDALONE
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifndef ASSERT
#include <assert.h>
#define ASSERT assert
#endif


#include "regexp.h"


typedef unsigned char REbool;
enum { RE_FALSE, RE_TRUE };



typedef enum REOp {
    REOP_EMPTY,         /* an empty alternative */

    REOP_ALT,           /* a tree of alternates */
    REOP_ENDALT,        /* flags end of alternate, signals jump to next */

    REOP_BOL,           /* start of line or input string '^' */
    REOP_EOL,           /* end of line or input string '$' */

    REOP_DOT,
    REOP_CLASS,         /* '[...]' */

    REOP_PAREN,         /* capturing */
    REOP_CLOSEPAREN,    /* continuation for end of paren */
    REOP_BACKREF,

    REOP_WBND,          /* word boundary '\b' */
    REOP_UNWBND,        /* not a word boundary '\B' */

    REOP_ASSERT,        /* '(?= ... )' */
    REOP_ASSERTTEST,    /* end of child for above */
    REOP_ASSERTNOT,     /* '(?! ... )' */
    REOP_ASSERTNOTTEST, /* end of child for above */

    REOP_FLAT,          /* literal characters (data.length count) */
                        /* tree node of FLAT gets transformed into : */
    REOP_FLATNi,        /* 'n' literals characters, ignore case */
    REOP_FLATN,         /* 'n' literals characters, case sensitive */
    REOP_FLAT1i,        /* 1 literal characters, ignore case */
    REOP_FLAT1,         /* 1 literal characters, case sensitive */

    REOP_DEC,           /* decimal digit '\d' */
    REOP_UNDEC,         /* not a decimal digit '\D' */

    REOP_WS,            /* whitespace or line terminator '\s' */
    REOP_UNWS,          /* not whitespace '\S' */

    REOP_LETDIG,        /* letter or digit or '_' '\w' */
    REOP_UNLETDIG,      /* not letter or digit or '_' '\W' */

    REOP_QUANT,         /* '+', '*', '?' as tree node or '{..}'  */
    REOP_STAR,          /* Bytecode versions, to save space ... */
    REOP_OPT,
    REOP_PLUS,
    REOP_MINIMALSTAR,
    REOP_MINIMALOPT,
    REOP_MINIMALPLUS,
    REOP_MINIMALQUANT,

    REOP_REPEAT,        /* intermediate op for processing quant */
    REOP_MINIMALREPEAT, /* and ditto for non greedy case */

    REOP_ENDCHILD,      /* end of child for quantifier */

    REOP_END            /* the holy grail */

} REOp;

#define RE_ISDEC(c)         ( (c >= '0') && (c <= '9') )
#define RE_UNDEC(c)         (c - '0')

#define RE_ISLETTER(c)      ( ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) )
#define RE_ISLETDIG(c)      ( RE_ISLETTER(c) || RE_ISDEC(c) )

#define RE_ISLINETERM(c)    ( (c == '\n') || (c == '\r') )


typedef struct REContinuationData {
    REOp op;              /* not necessarily the same as *pc */
    REuint8 *pc;
} REContinuationData;

typedef struct RENode {
    REOp kind;
    RENode *next;               /* links consecutive terms */
    void *child;
    REuint32 parenIndex;        /* for QUANT, PAREN, BACKREF */ 
    union {
        void *child2;           /* for ALT */
        struct {
            REint32 min;
            REint32 max;            /* -1 for infinity */
            REbool greedy;
            REint32 parenCount;     /* #parens in quantified term */
        } quantifier;
        struct {
            REchar ch;              /* for FLAT1 */
            REuint32 length;        /* for FLATN */
        } flat;
        struct {
            REint32 classIndex;     /* index into classList in REState */
            const REchar *end;      /* last character of source */
            REuint32 length;        /* calculated bitmap length */
        } chclass;
    } data;
} RENode;

typedef struct REProgState {
    REint32 min;
    REint32 max;
    REint32 parenCount;
    REint32 parenIndex;
    REint32 index;
    REContinuationData continuation;
} REProgState;

#define INITIAL_STATESTACK (2000)
REProgState *stateStack;
REuint32 stateStackTop;
REuint32 maxStateStack;

typedef struct REGlobalData {
    REState *regexp;            /* the RE in execution */   
    REint32 length;             /* length of input string */
    const REchar *input;        /* the input string */
    RE_Error error;             /* runtime error code (out_of_memory only?) */
    REint32 lastParen;          /* highest paren set so far */
    REbool globalMultiline;     /* as specified for current execution */
} REGlobalData;

typedef struct REBackTrackData {
    REContinuationData continuation;    /* where to backtrack to */
    REMatchState *state;                     /* the state of the match */
    REint32 lastParen;
    REProgState *precedingState;
    REint32 precedingStateTop;
} REBackTrackData;

#define INITIAL_BACKTRACK (20)
REint32 maxBackTrack;
REBackTrackData *backTrackStack = NULL;
REint32 backTrackStackTop;

/*
    Allocate space for a state and copy x into it.
*/
static REMatchState *copyState(REMatchState *x)
{
    REuint32 sz = sizeof(REMatchState) + (x->parenCount * sizeof(RECapture));
    REMatchState *result = (REMatchState *)malloc(sz);
    memcpy(result, x, sz);
    return result;
}

/*
    Copy state.
*/
static void recoverState(REMatchState *into, REMatchState *from)
{
    memcpy(into, from, sizeof(REMatchState)
                                + (from->parenCount * sizeof(RECapture)));
}

/*
    Bottleneck for any errors.
*/
static void reportRegExpError(RE_Error *errP, RE_Error err) 
{
    *errP = err;
}

/* forward declarations for parser routines */
REbool parseDisjunction(REState *parseState);
REbool parseAlternative(REState *parseState);
REbool parseTerm(REState *parseState);


static REbool isASCIIHexDigit(REchar c, REuint32 *digit)
{
    REuint32 cv = c;

    if (cv < '0')
        return RE_FALSE;
    if (cv <= '9') {
        *digit = cv - '0';
        return RE_TRUE;
    }
    cv |= 0x20;
    if (cv >= 'a' && cv <= 'f') {
        *digit = cv - 'a' + 10;
        return RE_TRUE;
    }
    return RE_FALSE;
}


/*
    Allocate & initialize a new node.
*/
static RENode *newRENode(REState *pState, REOp kind)
{
    RENode *result = (RENode *)malloc(sizeof(RENode));
    if (result == NULL) {
        reportRegExpError(&pState->error, RE_OUT_OF_MEMORY);
        return NULL;
    }
    result->kind = kind;
    result->data.flat.length = 1;
    result->next = NULL;
    result->child = NULL;
    return result;
}

REbool parseDisjunction(REState *parseState)
{
    if (!parseAlternative(parseState)) return RE_FALSE;
    
    if ((parseState->src != parseState->srcEnd) 
                    && (*parseState->src == '|')) {
        RENode *altResult;
        ++parseState->src;
        altResult = newRENode(parseState, REOP_ALT);
        if (!altResult) return RE_FALSE;
        altResult->child = parseState->result;
        if (!parseDisjunction(parseState)) return RE_FALSE;
        altResult->data.child2 = parseState->result;
        parseState->result = altResult;
        parseState->codeLength += 9; /* alt, <next>, ..., goto, <end> */
    }
    return RE_TRUE;
}

/*
 *   Return a single REOP_Empty node for an empty Alternative, 
 *   a single REOP_xxx node for a single term and a next field 
 *   linked list for a list of terms for more than one term. 
 *   Consecutive FLAT1 nodes get combined into a single FLATN
 */
REbool parseAlternative(REState *parseState)
{
    RENode *headTerm = NULL;
    RENode *tailTerm = NULL;
    while (RE_TRUE) {
        if ((parseState->src == parseState->srcEnd)
                || (*parseState->src == ')')
                || (*parseState->src == '|')) {
            if (!headTerm) {
                parseState->result = newRENode(parseState, REOP_EMPTY);
                if (!parseState->result) return RE_FALSE;
            }
            else
                parseState->result = headTerm;
            return RE_TRUE;
        }
        if (!parseTerm(parseState)) return RE_FALSE;
        if (headTerm == NULL)
            headTerm = parseState->result;
        else {
            if (tailTerm == NULL) {
                if ((headTerm->kind == REOP_FLAT) 
                        && (parseState->result->kind == headTerm->kind)
                        && (parseState->result->child 
                            == (REchar *)(headTerm->child) 
                                    + headTerm->data.flat.length) ) {
                    headTerm->data.flat.length 
                                    += parseState->result->data.flat.length;
                    free(parseState->result);
                }
                else {
                    headTerm->next = parseState->result;
                    tailTerm = parseState->result;
                    while (tailTerm->next) tailTerm = tailTerm->next;
                }
            }
            else {
                if ((tailTerm->kind == REOP_FLAT) 
                        && (parseState->result->kind == tailTerm->kind)
                        && (parseState->result->child 
                            == (REchar *)(tailTerm->child) 
                                    + tailTerm->data.flat.length) ) {
                    tailTerm->data.flat.length 
                                    += parseState->result->data.flat.length;
                    free(parseState->result);
                }
                else {
                    tailTerm->next = parseState->result;
                    tailTerm = tailTerm->next;
                    while (tailTerm->next) tailTerm = tailTerm->next;
                }
            }
        }
    }
}

static REint32 getDecimalValue(REchar c, REState *parseState)
{
    REint32 value = RE_UNDEC(c);
    while (parseState->src < parseState->srcEnd) {
        c = *parseState->src; 
        if (RE_ISDEC(c)) {
            value = (10 * value) + RE_UNDEC(c);
            ++parseState->src;
        }
        else
            break;
    }
    return value;
}

/* calculate the total size of the bitmap required for a class expression */

static REbool calculateBitmapSize(REState *pState, RENode *target)

{

    REchar rangeStart = 0;
    const REchar *src = (const REchar *)(target->child);
    const REchar *end = target->data.chclass.end;

    REchar c;
    REuint32 nDigits;
    REuint32 i;
    REuint32 max = 0;
    REbool inRange = RE_FALSE;

    target->data.chclass.length = 0;

    if (src == end)
        return RE_TRUE;

    if (*src == '^')
        ++src;
    
    while (src != end) {
        REuint32 localMax = 0;
        switch (*src) {
        case '\\':
            ++src;
            c = *src++;
            switch (c) {
            case 'b':
                localMax = 0x8;
                break;
            case 'f':
                localMax = 0xC;
                break;
            case 'n':
                localMax = 0xA;
                break;
            case 'r':
                localMax = 0xD;
                break;
            case 't':
                localMax = 0x9;
                break;
            case 'v':
                localMax = 0xB;
                break;
            case 'c':
                if (((src + 1) < end) && RE_ISLETTER(src[1]))
                    localMax = (REchar)(*src++ & 0x1F);
                else
                    localMax = '\\';
                break;
            case 'x':
                nDigits = 2;
                goto lexHex;
            case 'u':
                nDigits = 4;
lexHex:
                {
                    REuint32 n = 0;
                    for (i = 0; (i < nDigits) && (src < end); i++) {
                        REuint32 digit;
                        c = *src++;
                        if (!isASCIIHexDigit(c, &digit)) {
                            /* back off to accepting the original 
                             *'\' as a literal 
                             */
                            src -= (i + 1);
                            n = '\\';
                            break;
                        }
                        n = (n << 4) | digit;
                    }
                    localMax = n;
                }
                break;
            case 'd':
                if (inRange) {
                    reportRegExpError(&pState->error, RE_WRONG_RANGE);
                    return RE_FALSE;
                }
                localMax = '9';
                break;
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                if (inRange) {
                    reportRegExpError(&pState->error, RE_WRONG_RANGE);
                    return RE_FALSE;
                }
                target->data.chclass.length = 65536;
                return RE_TRUE;
            default:
                localMax = c;
                break;
            }
            break;
        default:
            localMax = *src++;
            break;
        }
        if (inRange) {
            if (rangeStart > localMax) {
                reportRegExpError(&pState->error, RE_WRONG_RANGE);
                return RE_FALSE;
            }
            inRange = RE_FALSE;
        }
        else {
            if (src < (end - 1)) {
                if (*src == '-') {
                    ++src;
                    inRange = RE_TRUE;
                    rangeStart = (REchar)localMax;
                    continue;
                }
            }
        }
        if (pState->flags & RE_IGNORECASE) {
            c = canonicalize((REchar)localMax);
            if (c > localMax)
                localMax = c;
        }
        if (localMax > max)
            max = localMax;
    }
    target->data.chclass.length = max + 1;
    return RE_TRUE;
}


REbool parseTerm(REState *parseState)
{
    REchar c = *parseState->src++;
    REuint32 nDigits;
    REuint32 parenBaseCount = parseState->parenCount;
    REuint32 num, tmp;
    RENode *term;
    REchar *numStart;

    switch (c) {
    /* assertions and atoms */
    case '^':
        parseState->result = newRENode(parseState, REOP_BOL);
        if (!parseState->result) return RE_FALSE;
        parseState->codeLength++;
        break;
    case '$':
        parseState->result = newRENode(parseState, REOP_EOL);
        if (!parseState->result) return RE_FALSE;
        parseState->codeLength++;
        break;
    case '\\':
        if (parseState->src < parseState->srcEnd) {
            c = *parseState->src++;
            switch (c) {
            /* assertion escapes */
            case 'b' :
                parseState->result = newRENode(parseState, REOP_WBND);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            case 'B':
                parseState->result = newRENode(parseState, REOP_UNWBND);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            /* Decimal escape */
            case '0':
                if (parseState->version == RE_VERSION_1) {
                    /* octal escape */
doOctal:
                    num = 0;
                    while (parseState->src < parseState->srcEnd) {
                        c = *parseState->src;
                        if ((c >= '0') && (c <= '7')) {
                            parseState->src++;
                            tmp = 8 * num + (REuint32)RE_UNDEC(c);
                            if (tmp > 0377)
                                break;
                            num = tmp;
                        }
                        else
                            break;
                    }
                    parseState->result = newRENode(parseState, REOP_FLAT);
                    parseState->codeLength += 3;
                    if (!parseState->result) return RE_FALSE;
                    parseState->result->data.flat.ch = (REchar)(num);
                }
                else {
                    parseState->result = newRENode(parseState, REOP_FLAT);
                    if (!parseState->result) return RE_FALSE;
                    parseState->result->data.flat.ch = 0;
                    parseState->codeLength += 3;
                }
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                numStart = parseState->src - 1;
                num = (REuint32)getDecimalValue(c, parseState);
                if (parseState->version == RE_VERSION_1) {
                    /* 
                     * n in [8-9] and > count of parentheses, 
                     * then revert to '8' or '9', ignoring the '\' 
                     */
                    if (((num == 8) || (num == 9)) 
                            && (num > parseState->parenCount)) {
                        parseState->result = newRENode(parseState, REOP_FLAT);
                        parseState->codeLength += 3;
                        if (!parseState->result) return RE_FALSE;
                        parseState->result->data.flat.ch = (REchar)(num + '0');                    
                    }                    
                    /* 
                     * more than 1 digit, or a number greater than
                     * the count of parentheses => it's an octal 
                     */
                    if (((parseState->src - numStart) > 1)
                                    || (num > parseState->parenCount)) {
	                parseState->src = numStart;
                        goto doOctal;
                    }
                    parseState->result = newRENode(parseState, REOP_BACKREF);
                    if (!parseState->result) return RE_FALSE;
                    parseState->result->parenIndex = num - 1;
                    parseState->codeLength += 3;
                }
                else {
                    parseState->result = newRENode(parseState, REOP_BACKREF);
                    if (!parseState->result) return RE_FALSE;
                    parseState->result->parenIndex = num - 1;
                    parseState->codeLength += 3;
                }
                break;
            /* Control escape */
            case 'f':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                parseState->result->data.flat.ch = 0xC;
                parseState->codeLength += 3;
                break;
            case 'n':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                parseState->result->data.flat.ch = 0xA;
                parseState->codeLength += 3;
                break;
            case 'r':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                parseState->result->data.flat.ch = 0xD;
                parseState->codeLength += 3;
                break;
            case 't':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                parseState->result->data.flat.ch = 0x9;
                parseState->codeLength += 3;
                break;
            case 'v':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                parseState->result->data.flat.ch = 0xB;
                parseState->codeLength += 3;
                break;
            /* Control letter */
            case 'c':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                if (((parseState->src + 1) < parseState->srcEnd) &&
                                    RE_ISLETTER(parseState->src[1]))
                    parseState->result->data.flat.ch 
                                        = (REchar)(*parseState->src++ & 0x1F);
                else {
                    /* back off to accepting the original '\' as a literal */
                    --parseState->src;
                    parseState->result->data.flat.ch = '\\';
                    parseState->result->child = (void *)parseState->src;
                }
                parseState->codeLength += 3;
                break;
            /* HexEscapeSequence */
            case 'x':
                nDigits = 2;
                goto lexHex;
            /* UnicodeEscapeSequence */
            case 'u':
                nDigits = 4;
lexHex:
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                {
                    REuint32 n = 0;
                    REuint32 i;
                    for (i = 0; (i < nDigits) 
                            && (parseState->src < parseState->srcEnd); i++) {
                        REuint32 digit;
                        c = *parseState->src++;
                        if (!isASCIIHexDigit(c, &digit)) {
                            /* 
                             *  back off to accepting the original 
                             *  'u' or 'x' as a literal 
                             */
                            parseState->src -= (i + 2);
                            n = *parseState->src++;
                            break;
                        }
                        n = (n << 4) | digit;
                    }
                    parseState->result->data.flat.ch = (REchar)(n);
                }
                parseState->codeLength += 3;
                break;
            /* Character class escapes */
            case 'd':
                parseState->result = newRENode(parseState, REOP_DEC);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            case 'D':
                parseState->result = newRENode(parseState, REOP_UNDEC);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            case 's':
                parseState->result = newRENode(parseState, REOP_WS);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            case 'S':
                parseState->result = newRENode(parseState, REOP_UNWS);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            case 'w':
                parseState->result = newRENode(parseState, REOP_LETDIG);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            case 'W':
                parseState->result = newRENode(parseState, REOP_UNLETDIG);
                if (!parseState->result) return RE_FALSE;
                parseState->codeLength++;
                break;
            /* IdentityEscape */
            default:
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return RE_FALSE;
                parseState->result->data.flat.ch = c;
                parseState->result->child = (void *)(parseState->src - 1);
                parseState->codeLength += 3;
                break;
            }
            break;
        }
        else {
            /* a trailing '\' is an error */
            reportRegExpError(&parseState->error, RE_TRAILING_SLASH);
            return RE_FALSE;
        }
    case '(':
        {
            RENode *result = NULL;
            if ((*parseState->src == '?') 
                    && ( (parseState->src[1] == '=')
                            || (parseState->src[1] == '!')
                            || (parseState->src[1] == ':') )) {
                ++parseState->src;
                switch (*parseState->src++) {
                case '=':
                    result = newRENode(parseState, REOP_ASSERT);
                    if (!result) return RE_FALSE;
                    /* ASSERT, <next>, ... ASSERTTEST */
                    parseState->codeLength += 4;    
                    break;
                case '!':
                    result = newRENode(parseState, REOP_ASSERTNOT);
                    if (!result) return RE_FALSE;
                        /* ASSERTNOT, <next>, ... ASSERTNOTTEST */
                    parseState->codeLength += 4;
                    break;
                }
            }
            else {
                result = newRENode(parseState, REOP_PAREN);
                /* PAREN, <index>, ... CLOSEPAREN, <index> */
                parseState->codeLength += 6;
                if (!result) return RE_FALSE;
                result->parenIndex = parseState->parenCount++;
            }
            if (!parseDisjunction(parseState)) return RE_FALSE;
            if ((parseState->src == parseState->srcEnd)
                    || (*parseState->src != ')')) {
                reportRegExpError(&parseState->error, RE_UNCLOSED_PAREN);
                return RE_FALSE;
            }
            else {
                ++parseState->src;
            }
            if (result) {
                result->child = parseState->result;
                parseState->result = result;
            }
            break;
        }
    case '[':
        parseState->result = newRENode(parseState, REOP_CLASS);
        if (!parseState->result) return RE_FALSE;
        parseState->result->child = (void *)(parseState->src);
        while (RE_TRUE) {
            if (parseState->src == parseState->srcEnd) {
                reportRegExpError(&parseState->error, RE_UNCLOSED_CLASS);
                return RE_FALSE;
            }
            if (*parseState->src == '\\') {
                ++parseState->src;
                if (RE_ISDEC(*parseState->src)) {
                    reportRegExpError(&parseState->error, RE_BACKREF_IN_CLASS);
                    return RE_FALSE;
                }
            }
            else {
                if (*parseState->src == ']') {
                    parseState->result->data.chclass.end = parseState->src++;
                    break;
                }
            }
            ++parseState->src;
        }
        parseState->result->data.chclass.classIndex = (int32)parseState->classCount++;
        /* Call calculateBitmapSize now as we want any errors it finds 
          to be reported during the parse phase, not at execution */
        if (!calculateBitmapSize(parseState, parseState->result))
            return RE_FALSE;
        parseState->codeLength += 3; /* CLASS, <index> */
        break;

    case '.':
        parseState->result = newRENode(parseState, REOP_DOT);
        if (!parseState->result) return RE_FALSE;
        parseState->codeLength++;
        break;
    default:
        parseState->result = newRENode(parseState, REOP_FLAT);
        if (!parseState->result) return RE_FALSE;
        parseState->result->data.flat.ch = c;
        parseState->result->child = (void *)(parseState->src - 1);
        parseState->codeLength += 3;
        break;
    }

    term = parseState->result;
    if (parseState->src < parseState->srcEnd) {
        switch (*parseState->src) {
        case '+':
            parseState->result = newRENode(parseState, REOP_QUANT);
            if (!parseState->result) return RE_FALSE;
            parseState->result->data.quantifier.min = 1;
            parseState->result->data.quantifier.max = -1;
            /* <PLUS>, <parencount>, <parenindex>, <next> ... <ENDCHILD> */
            parseState->codeLength += 8;
            goto quantifier;
        case '*':
            parseState->result = newRENode(parseState, REOP_QUANT);
            if (!parseState->result) return RE_FALSE;
            parseState->result->data.quantifier.min = 0;
            parseState->result->data.quantifier.max = -1;
            /* <STAR>, <parencount>, <parenindex>, <next> ... <ENDCHILD> */
            parseState->codeLength += 8;
            goto quantifier;
        case '?':
            parseState->result = newRENode(parseState, REOP_QUANT);
            if (!parseState->result) return RE_FALSE;
            parseState->result->data.quantifier.min = 0;
            parseState->result->data.quantifier.max = 1;
            /* <OPT>, <parencount>, <parenindex>, <next> ... <ENDCHILD> */
            parseState->codeLength += 8;
            goto quantifier;
        case '{':
            {
                REint32 min = 0;
                REint32 max = -1;
                REchar c;
                ++parseState->src;
            
                parseState->result = newRENode(parseState, REOP_QUANT);
                if (!parseState->result) return RE_FALSE;

                c = *parseState->src;
                if (RE_ISDEC(c)) {
                    ++parseState->src;
                    min = getDecimalValue(c, parseState);
                    c = *parseState->src;
                }
                if (c == ',') {
                    c = *++parseState->src;
                    if (RE_ISDEC(c)) {
                        ++parseState->src;
                        max = getDecimalValue(c, parseState);
                        c = *parseState->src;
                    }
                }
                else
                    max = min;
                parseState->result->data.quantifier.min = min;
                parseState->result->data.quantifier.max = max;
                /* QUANT, <min>, <max>, <parencount>, 
                                        <parenindex>, <next> ... <ENDCHILD> */
                parseState->codeLength += 12; 
                if (c == '}')
                    goto quantifier;
                else {
                    reportRegExpError(&parseState->error, RE_UNCLOSED_BRACKET);
                    return RE_FALSE;
                }
            }
        }
    }
    return RE_TRUE;

quantifier:
    ++parseState->src;
    parseState->result->child = term;
    parseState->result->parenIndex = parenBaseCount;
    parseState->result->data.quantifier.parenCount 
                                = (REint32)(parseState->parenCount - parenBaseCount);
    if ((parseState->src < parseState->srcEnd) && (*parseState->src == '?')) {
        ++parseState->src;
        parseState->result->data.quantifier.greedy = RE_FALSE;
    }
    else
        parseState->result->data.quantifier.greedy = RE_TRUE;  
    return RE_TRUE;
}






/*
1. Let e be x's endIndex.
2. If e is zero, return RE_TRUE.
3. If Multiline is RE_FALSE, return RE_FALSE.
4. If the character Input[e-1] is one of the line terminator characters <LF>,
   <CR>, <LS>, or <PS>, return RE_TRUE.
5. Return RE_FALSE.
*/
static REMatchState *bolMatcher(REGlobalData *gData, REMatchState *x)
{
    REuint32 e = (REuint32)x->endIndex;
    if (e != 0) {
        if (gData->globalMultiline ||
                    (gData->regexp->flags & RE_MULTILINE)) {
            if (!RE_ISLINETERM(gData->input[e - 1]))
                return NULL;
        }
        else
            return NULL;
    }
    return x;
}

/*
1. Let e be x's endIndex.
2. If e is equal to InputLength, return RE_TRUE.
3. If multiline is RE_FALSE, return RE_FALSE.
4. If the character Input[e] is one of the line terminator characters <LF>, 
   <CR>, <LS>, or <PS>, return RE_TRUE.
5. Return RE_FALSE.
*/
static REMatchState *eolMatcher(REGlobalData *gData, REMatchState *x)
{
    REint32 e = x->endIndex;
    if (e != gData->length) {
        if (gData->globalMultiline ||
                    (gData->regexp->flags & RE_MULTILINE)) {
            if (!RE_ISLINETERM(gData->input[e]))
                return NULL;
        }
        else
            return NULL;
    }
    return x;
}


/*
1. If e == -1 or e == InputLength, return RE_FALSE.
2. Let c be the character Input[e].
3. If c is one of the sixty-three characters in the table below, return RE_TRUE.
a b c d e f g h i j k l m n o p q r s t u v w x y z
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
0 1 2 3 4 5 6 7 8 9 _
4. Return RE_FALSE.
*/
static REbool isWordChar(REint32 e, REGlobalData *gData)
{
    REchar c;
    if ((e == -1) || (e == (REint32)(gData->length)))
        return RE_FALSE;
    c = gData->input[e];
    if (RE_ISLETDIG(c) || (c == '_'))
        return RE_TRUE;
    return RE_FALSE;
}

/*
1. Let e be x's endIndex.
2. Call IsWordChar(e-1) and let a be the boolean result.
3. Call IsWordChar(e) and let b be the boolean result.
for '\b'
4. If a is RE_TRUE and b is RE_FALSE, return RE_TRUE.
5. If a is RE_FALSE and b is RE_TRUE, return RE_TRUE.
6. Return RE_FALSE.

for '\B'
4. If a is RE_TRUE and b is RE_FALSE, return RE_FALSE.
5. If a is RE_FALSE and b is RE_TRUE, return RE_FALSE.
6. Return RE_TRUE.
*/
static REMatchState *wbndMatcher(REGlobalData *gData, REMatchState *x, REbool sense)
{
    REint32 e = (REint32)(x->endIndex);

    REbool a = isWordChar(e - 1, gData);
    REbool b = isWordChar(e, gData);
    
    if (sense) {
        if ((a && !b) || (!a && b))
            return x;
        else
            return NULL;
    }
    else {
        if ((a && !b) || (!a && b))
            return NULL;
        else
            return x;
    }
}

/*
1. Let A be the set of all characters except the four line terminator 
   characters <LF>, <CR>, <LS>, or <PS>.
2. Call CharacterSetMatcher(A, RE_FALSE) and return its Matcher result.
*/
static REMatchState *dotMatcher(REGlobalData *gData, REMatchState *x)
{
    REchar ch;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;
    ch = gData->input[e];
    if (RE_ISLINETERM(ch))
        return NULL;
    x->endIndex++;
    return x;
}

/*
    \d evaluates by returning the ten-element set of characters containing the
    characters 0 through 9 inclusive.
    \D evaluates by returning the set of all characters not included in the set
    returned by \d.
*/
static REMatchState *decMatcher(REGlobalData *gData, REMatchState *x, REbool sense)
{
    REchar ch;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;
    ch = gData->input[e];
    if (RE_ISDEC(ch) != sense)
        return NULL;
    x->endIndex++;
    return x;
}

/*
    \s evaluates by returning the set of characters containing
    the characters that are on the right-hand side of the WhiteSpace 
    (section 7.2) or LineTerminator (section 7.3) productions.
    \S evaluates by returning the set of all characters not 
    included in the set returned by \s.
*/
static REMatchState *wsMatcher(REGlobalData *gData, REMatchState *x, REbool sense)
{
    REchar ch;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;
    ch = gData->input[e];
    if (RE_ISSPACE(ch) != sense)
        return NULL;
    x->endIndex++;
    return x;
}

/*
    \w evaluates by returning the set of characters containing the sixty-three
    characters:
    a b c d e f g h i j k l m n o p q r s t u v w x y z
    A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
    0 1 2 3 4 5 6 7 8 9 _
    \W evaluates by returning the set of all characters not included in the set
    returned by \w.
*/
static REMatchState *letdigMatcher(REGlobalData *gData, REMatchState *x, REbool sense)
{
    REchar ch;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;
    ch = gData->input[e];
    if ((RE_ISLETDIG(ch) || (ch == '_')) != sense)
        return NULL;
    x->endIndex++;
    return x;
}

/*
1. Return an internal Matcher closure that takes two arguments, a State x 
and a Continuation c, and performs the following:
    1. Let e be x's endIndex.
    2. If e == InputLength, return failure.
    3. Let c be the character Input[e].
    4. Let cc be the result of Canonicalize(c).
    5. If invert is RE_TRUE, go to step 8.
    6. If there does not exist a member a of set A such that Canonicalize(a) 
       == cc, then return failure.
    7. Go to step 9.
    8. If there exists a member a of set A such that Canonicalize(a) == cc,
       then return failure.
    9. Let cap be x's captures internal array.
    10. Let y be the State (e+1, cap).
    11. Call c(y) and return its result.
*/
static REMatchState *flatMatcher(REGlobalData *gData, REMatchState *x, REchar matchCh)
{
    REchar ch;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;
    ch = gData->input[e];

    if (ch != matchCh)
        return NULL;
    x->endIndex++;
    return x;
}

static REMatchState *flatIMatcher(REGlobalData *gData, REMatchState *x, REchar matchCh)
{
    REchar ch;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;
    ch = gData->input[e];

    if (canonicalize(ch) != canonicalize(matchCh))
        return NULL;
    x->endIndex++;
    return x;
}

/*
    Consecutive literal characters.
*/
static REMatchState *flatNMatcher(REGlobalData *gData, REMatchState *x, 
                             REchar *matchChars, REint32 length)
{
    REint32 e = x->endIndex;
    REint32 i;
    if ((e + length) > gData->length)
        return NULL;
    for (i = 0; i < length; i++) {
        if (matchChars[i] != gData->input[e + i])
            return NULL;
    }
    x->endIndex += length;
    return x;
}

static REMatchState *flatNIMatcher(REGlobalData *gData, REMatchState *x,
                              REchar *matchChars, REint32 length)
{
    REint32 e = x->endIndex;
    REint32 i;
    if ((e + length) > gData->length)
        return NULL;
    for (i = 0; i < length; i++) {
        if (canonicalize(matchChars[i]) 
                != canonicalize(gData->input[e + i]))
            return NULL;
    }
    x->endIndex += length;
    return x;
}

/* Add a single character to the RECharSet */

static void addCharacterToCharSet(RECharSet *cs, REchar c)

{
    REuint32 byteIndex = (REuint32)(c / 8);
    ASSERT(c < cs->length);
    cs->bits[byteIndex] |= 1 << (c & 0x7);
}


/* Add a character range, c1 to c2 (inclusive) to the RECharSet */

static void addCharacterRangeToCharSet(RECharSet *cs, REchar c1, REchar c2)

{
    REuint32 i;

    REuint32 byteIndex1 = (REuint32)(c1 / 8);
    REuint32 byteIndex2 = (REuint32)(c2 / 8);

    ASSERT((c2 <= cs->length) && (c1 <= c2));

    c1 &= 0x7;
    c2 &= 0x7;

    if (byteIndex1 == byteIndex2) {
        cs->bits[byteIndex1] |= ((REuint8)(0xFF) >> (7 - (c2 - c1))) << c1;
    }
    else {
        cs->bits[byteIndex1] |= 0xFF << c1;
        for (i = byteIndex1 + 1; i < byteIndex2; i++)
            cs->bits[i] = 0xFF;
        cs->bits[byteIndex2] |= (REuint8)(0xFF) >> (7 - c2);
    }
}


/* Compile the source of the class into a RECharSet */

static REbool processCharSet(REState *pState, RENode *target)
{
    REchar rangeStart = 0, thisCh;
    const REchar *src = (const REchar *)(target->child);
    const REchar *end = target->data.chclass.end;

    REuint32 byteLength;
    REchar c;
    REint32 nDigits;
    REint32 i;
    REbool inRange = RE_FALSE;

    RECharSet *charSet = &pState->classList[target->data.chclass.classIndex];
    charSet->length = target->data.chclass.length;
    charSet->sense = RE_TRUE;
    byteLength = (charSet->length / 8) + 1;    
    charSet->bits = (REuint8 *)malloc(byteLength);
    if (!charSet->bits)
        return RE_FALSE;
    memset(charSet->bits, 0, byteLength);
    
    if (src == end) {
        return RE_TRUE;
    }

    if (*src == '^') {
        charSet->sense = RE_FALSE;
        ++src;
    }

    while (src != end) {
        switch (*src) {
        case '\\':
            ++src;
            c = *src++;
            switch (c) {
            case 'b':
                thisCh = 0x8;
                break;
            case 'f':
                thisCh = 0xC;
                addCharacterToCharSet(charSet, 0xC);
                break;
            case 'n':
                thisCh = 0xA;
                break;
            case 'r':
                thisCh = 0xD;
                break;
            case 't':
                thisCh = 0x9;
                break;
            case 'v':
                thisCh = 0xB;
                break;
            case 'c':
                if (((src + 1) < end) && RE_ISLETTER(src[1]))
                    thisCh = (REchar)(*src++ & 0x1F);
                else {
                    --src;
                    thisCh = '\\';
                }
                break;
            case 'x':
                nDigits = 2;
                goto lexHex;
            case 'u':
                nDigits = 4;
lexHex:
                {
                    REuint32 n = 0;
                    for (i = 0; (i < nDigits) && (src < end); i++) {
                        REuint32 digit;
                        c = *src++;
                        if (!isASCIIHexDigit(c, &digit)) {
                            /* back off to accepting the original '\' 
                             * as a literal
                             */
                            src -= (i + 1);
                            n = '\\';
                            break;
                        }
                        n = (n << 4) | digit;
                    }
                    thisCh = (REchar)(n);
                }
                break;
            case 'd':
                addCharacterRangeToCharSet(charSet, '0', '9');
                continue;   /* don't need range processing */
            case 'D':
                addCharacterRangeToCharSet(charSet, 0, '0' - 1);
                addCharacterRangeToCharSet(charSet, (REchar)('9' + 1),
                                            (REchar)(charSet->length - 1));
                continue;
            case 's':
                for (i = (REint32)(charSet->length - 1); i >= 0; i--)
                    if (RE_ISSPACE(i))
                        addCharacterToCharSet(charSet, (REchar)(i));
                continue;
            case 'S':
                for (i = (REint32)(charSet->length - 1); i >= 0; i--)
                    if (!RE_ISSPACE(i))
                        addCharacterToCharSet(charSet, (REchar)(i));
                continue;
            case 'w':
                for (i = (REint32)(charSet->length - 1); i >= 0; i--)
                    if (RE_ISLETDIG(i))
                        addCharacterToCharSet(charSet, (REchar)(i));
                continue;
            case 'W':
                for (i = (REint32)(charSet->length - 1); i >= 0; i--)
                    if (!RE_ISLETDIG(i))
                        addCharacterToCharSet(charSet, (REchar)(i));
                continue;
            default:
                thisCh = c;
                break;

            }
            break;

        default:
            thisCh = *src++;
            break;

        }
        if (inRange) {
            if (pState->flags & RE_IGNORECASE) {
                REchar minch = (REchar)65535;
                REchar maxch = 0;
                /*

                    yuk
                
                */
                if (rangeStart < minch) minch = rangeStart;
                if (thisCh < minch) minch = thisCh;
                if (canonicalize(rangeStart) < minch) 
                                            minch = canonicalize(rangeStart);
                if (canonicalize(thisCh) < minch) minch = canonicalize(thisCh);

                if (rangeStart > maxch) maxch = rangeStart;
                if (thisCh > maxch) maxch = thisCh;
                if (canonicalize(rangeStart) > maxch) 
                                            maxch = canonicalize(rangeStart);
                if (canonicalize(thisCh) > maxch) maxch = canonicalize(thisCh);
                addCharacterRangeToCharSet(charSet, minch, maxch);
            }
            else
                addCharacterRangeToCharSet(charSet, rangeStart, thisCh);
            inRange = RE_FALSE;
        }
        else {
            if (pState->flags & RE_IGNORECASE)
                addCharacterToCharSet(charSet, canonicalize(thisCh));
            addCharacterToCharSet(charSet, thisCh);
            if (src < (end - 1)) {
                if (*src == '-') {
                    ++src;
                    inRange = RE_TRUE;
                    rangeStart = thisCh;
                }
            }
        }
    }
    return RE_TRUE;
}


/*
    Initialize the character set if it this is the first call.
    Test the bit - if the ^ flag was specified, non-inclusion is a success
*/
static REMatchState *classMatcher(REGlobalData *gData, REMatchState *x, REint32 index)
{
    REchar ch;
    RECharSet *charSet;
    REint32 byteIndex;
    REint32 e = x->endIndex;
    if (e == gData->length)
        return NULL;

/*
    if (target->data.chclass.charSet->bits == NULL) {
        if (!processCharSet(((globalData->regexp->flags & IGNORECASE) != 0), target))
            return NULL;
    }
    charSet = target->data.chclass.charSet;
*/
    charSet = &gData->regexp->classList[index];

    ch = gData->input[e];
    byteIndex = ch / 8;
    if (charSet->sense) {
        if ((charSet->length == 0) || 
             ( (ch > charSet->length)
                || ((charSet->bits[byteIndex] & (1 << (ch & 0x7))) == 0) ))
            return NULL;
    }
    else {
        if (! ((charSet->length == 0) || 
                 ( (ch > charSet->length)
                    || ((charSet->bits[byteIndex] & (1 << (ch & 0x7))) == 0) )))
            return NULL;
    }

    if (charSet->length)    /* match empty character */
        x->endIndex++;
    return x;
}



/*
1. Evaluate DecimalEscape to obtain an EscapeValue E.
2. If E is not a character then go to step 6.
3. Let ch be E's character.
4. Let A be a one-element RECharSet containing the character ch.
5. Call CharacterSetMatcher(A, RE_FALSE) and return its Matcher result.
6. E must be an integer. Let n be that integer.
7. If n=0 or n>NCapturingParens then throw a SyntaxError exception.
8. Return an internal Matcher closure that takes two arguments, a State x 
   and a Continuation c, and performs the following:
    1. Let cap be x's captures internal array.
    2. Let s be cap[n].
    3. If s is undefined, then call c(x) and return its result.
    4. Let e be x's endIndex.
    5. Let len be s's length.
    6. Let f be e+len.
    7. If f>InputLength, return failure.
    8. If there exists an integer i between 0 (inclusive) and len (exclusive)
       such that Canonicalize(s[i]) is not the same character as 
       Canonicalize(Input [e+i]), then return failure.
    9. Let y be the State (f, cap).
    10. Call c(y) and return its result.
*/

static REMatchState *backrefMatcher(REGlobalData *gData,
                               REMatchState *x, REuint32 parenIndex)
{
    REuint32 e;
    REuint32 len;
    REint32 f;
    REuint32 i;
    const REchar *parenContent;
    RECapture *s = &x->parens[parenIndex];
    if (s->index == -1)
        return x;

    e = (REuint32)x->endIndex;
    len = (REuint32)s->length;
    f = (REint32)(e + len);
    if (f > gData->length)
        return NULL;
    
    parenContent = &gData->input[s->index];
    if (gData->regexp->flags & RE_IGNORECASE) {
        for (i = 0; i < len; i++) {
            if (canonicalize(parenContent[i]) 
                                    != canonicalize(gData->input[e + i]))
                return NULL;
        }
    }
    else {
        for (i = 0; i < len; i++) {
            if (parenContent[i] != gData->input[e + i])
                return NULL;
        }
    }
    x->endIndex = f;
    return x;
}

/*
 *   free memory the RENode t and it's children
 */
static void freeRENode(RENode *t)
{
    RENode *n;
    while (t) {
        switch (t->kind) {
        case REOP_ALT:  
            freeRENode((RENode *)(t->child));
            freeRENode((RENode *)(t->data.child2));
            break;
        case REOP_QUANT:
            freeRENode((RENode *)(t->child));
            break;
        case REOP_PAREN:
            freeRENode((RENode *)(t->child));
            break;
        case REOP_ASSERT:
            freeRENode((RENode *)(t->child));
            break;
        case REOP_ASSERTNOT:
            freeRENode((RENode *)(t->child));
            break;
        }
        n = t->next;
        free(t);
        t = n;
    }
}

#define ARG_LEN                     (2)
#define CHECK_RANGE(branch, target) (ASSERT((((target) - (branch)) >= -32768) && (((target) - (branch)) <= 32767))) 
#define EMIT_ARG(pc, a)             ((pc)[0] = (REuint8)((a) >> 8), (pc)[1] = (REuint8)(a), (pc) += ARG_LEN)
#define EMIT_BRANCH(pc)             ((pc) += ARG_LEN)
#define EMIT_FIXUP(branch, target)  (EMIT_ARG((branch), (target) - (branch)))
#define GET_ARG(pc)                 ((REuint32)(((pc)[0] << 8) | (pc)[1]))

static REuint8 *emitREBytecode(REState *pState, REuint8 *pc, RENode *t)
{
    RENode *nextAlt;
    REuint8 *nextAltFixup, *nextTermFixup;

    while (t) {
        *pc++ = t->kind;
        switch (t->kind) {
        case REOP_EMPTY:
            --pc;
            break;
        case REOP_ALT:
            nextAlt = (RENode *)(t->data.child2);
            nextAltFixup = pc;
            EMIT_BRANCH(pc);    /* address of next alternate */
            pc = emitREBytecode(pState, pc, (RENode *)(t->child));
            *pc++ = REOP_ENDALT;
            nextTermFixup = pc;
            EMIT_BRANCH(pc);    /* address of following term */
            CHECK_RANGE(nextAltFixup, pc);
            EMIT_FIXUP(nextAltFixup, pc);
            pc = emitREBytecode(pState, pc, nextAlt);

            *pc++ = REOP_ENDALT;
            nextAltFixup = pc;
            EMIT_BRANCH(pc);

            CHECK_RANGE(nextTermFixup, pc);
            EMIT_FIXUP(nextTermFixup, pc);

            CHECK_RANGE(nextAltFixup, pc);
            EMIT_FIXUP(nextAltFixup, pc);
            break;
        case REOP_FLAT:
            if (t->child && (t->data.flat.length > 1)) {
                if (pState->flags & RE_IGNORECASE)
                    pc[-1] = REOP_FLATNi;
                else
                    pc[-1] = REOP_FLATN;
                EMIT_ARG(pc, (REchar *)(t->child) - pState->srcStart);
                EMIT_ARG(pc, t->data.flat.length);
            }
            else {  /* XXX original Monkey code separated ASCII and Unicode cases to save extra byte */
                if (pState->flags & RE_IGNORECASE)
                    pc[-1] = REOP_FLAT1i;
                else
                    pc[-1] = REOP_FLAT1;
                EMIT_ARG(pc, t->data.flat.ch);
            }
            break;
        case REOP_PAREN:
            EMIT_ARG(pc, t->parenIndex);
            pc = emitREBytecode(pState, pc, (RENode *)(t->child));
            *pc++ = REOP_CLOSEPAREN;
            EMIT_ARG(pc, t->parenIndex);
            break;
        case REOP_BACKREF:
            EMIT_ARG(pc, t->parenIndex);
            break;
        case REOP_ASSERT:
            nextTermFixup = pc;
            EMIT_BRANCH(pc);
            pc = emitREBytecode(pState, pc, (RENode *)(t->child));
            *pc++ = REOP_ASSERTTEST;
            CHECK_RANGE(nextTermFixup, pc);
            EMIT_FIXUP(nextTermFixup, pc);
            break;
        case REOP_ASSERTNOT:
            nextTermFixup = pc;
            EMIT_BRANCH(pc);
            pc = emitREBytecode(pState, pc, (RENode *)(t->child));
            *pc++ = REOP_ASSERTNOTTEST;
            CHECK_RANGE(nextTermFixup, pc);
            EMIT_FIXUP(nextTermFixup, pc);
            break;
        case REOP_QUANT:
            if ((t->data.quantifier.min == 0) && (t->data.quantifier.max == -1))
                pc[-1] = (t->data.quantifier.greedy) 
                                        ? REOP_STAR : REOP_MINIMALSTAR;
            else
            if ((t->data.quantifier.min == 0) && (t->data.quantifier.max == 1))
                pc[-1] = (t->data.quantifier.greedy) 
                                        ? REOP_OPT : REOP_MINIMALOPT;
            else
            if ((t->data.quantifier.min == 1) && (t->data.quantifier.max == -1))
                pc[-1] = (t->data.quantifier.greedy) 
                                        ? REOP_PLUS : REOP_MINIMALPLUS;
            else {
                if (!t->data.quantifier.greedy) pc[-1] = REOP_MINIMALQUANT;
                EMIT_ARG(pc, t->data.quantifier.min);
                EMIT_ARG(pc, t->data.quantifier.max);
            }
            EMIT_ARG(pc, t->data.quantifier.parenCount);
            EMIT_ARG(pc, t->parenIndex);
            nextTermFixup = pc;
            EMIT_BRANCH(pc);
            pc = emitREBytecode(pState, pc, (RENode *)(t->child));
            *pc++ = REOP_ENDCHILD;
            CHECK_RANGE(nextTermFixup, pc);
            EMIT_FIXUP(nextTermFixup, pc);
            break;
        case REOP_CLASS:
            EMIT_ARG(pc, t->data.chclass.classIndex);
            processCharSet(pState, t);
            break;
        }
        t = t->next;
    }
    return pc;
}

static REBackTrackData *pushBackTrackState(REGlobalData *gData, REOp op, 
                                    REuint8 *target, REMatchState *x)
{
    REBackTrackData *result;
    if (backTrackStackTop == maxBackTrack) {
        maxBackTrack <<= 1;
        backTrackStack = (REBackTrackData *)realloc(backTrackStack, 
                                sizeof(REBackTrackData) * maxBackTrack);
        if (!backTrackStack) {
            reportRegExpError(&gData->error, RE_OUT_OF_MEMORY);
            return NULL;
        }
    }
    result = &backTrackStack[backTrackStackTop++];
    result->continuation.op = op;
    result->continuation.pc = target;
    result->state = copyState(x);
    result->lastParen = gData->lastParen;

    result->precedingStateTop = (REint32)stateStackTop;
    if (stateStackTop) {
        result->precedingState = (REProgState *)malloc(sizeof(REProgState) 
                                                        * stateStackTop);
        if (!result->precedingState) {
            reportRegExpError(&gData->error, RE_OUT_OF_MEMORY);
            return NULL;
        }
        memcpy(result->precedingState, stateStack, sizeof(REProgState) 
                                                         * stateStackTop);
    }
    else
        result->precedingState = NULL;

    return result;
}

static REMatchState *executeREBytecode(REuint8 *pc, REGlobalData *gData, REMatchState *x)
{
    REOp op = (REOp)(*pc++);
    REContinuationData currentContinuation;
    REMatchState *result = NULL;
    REBackTrackData *backTrackData;
    REint32 k, length, offset, index;
    REuint32 parenIndex;
    REbool anchor = RE_FALSE;
    REchar anchorCh = 0;
    REchar matchCh;
    REuint8 *nextpc;
    REOp nextop;

    currentContinuation.pc = NULL;
    currentContinuation.op = REOP_END;

    /*
     *   If the first node is a literal match, step the index into
     *   the string until that match is made, or fail if it can't be
     *   found at all.
     */
    switch (op) {
    case REOP_FLAT1:
    case REOP_FLAT1i:
        anchorCh = (REchar)GET_ARG(pc);
        anchor = RE_TRUE;
        break;
    case REOP_FLATN:        
    case REOP_FLATNi:
        k = (REint32)GET_ARG(pc);
        anchorCh = gData->regexp->srcStart[k];
        anchor = RE_TRUE;
        break;
    }
    if (anchor) {
        anchor = RE_FALSE;
        for (k = x->endIndex; k < gData->length; k++) {
            matchCh = gData->input[k];
            if ((matchCh == anchorCh) ||                
                    ((gData->regexp->flags & RE_IGNORECASE)
                    && (canonicalize(matchCh) == canonicalize(anchorCh)))) {
                x->endIndex = k;
                x->startIndex = k;  /* inform caller that we bumped along */
                anchor = RE_TRUE;
                break;
            }
        }
        if (!anchor)
            return NULL;
    }

    while (RE_TRUE) {
        switch (op) {
        case REOP_EMPTY:
            result = x;
            break;
        case REOP_BOL:
            result = bolMatcher(gData, x);
            break;
        case REOP_EOL:
            result = eolMatcher(gData, x);
            break;
        case REOP_WBND:
            result = wbndMatcher(gData, x, RE_TRUE);
            break;
        case REOP_UNWBND:
            result = wbndMatcher(gData, x, RE_FALSE);
            break;
        case REOP_DOT:
            result = dotMatcher(gData, x);
            break;
        case REOP_DEC:
            result = decMatcher(gData, x, RE_TRUE);
            break;
        case REOP_UNDEC:
            result = decMatcher(gData, x, RE_FALSE);
            break;
        case REOP_WS:
            result = wsMatcher(gData, x, RE_TRUE);
            break;
        case REOP_UNWS:
            result = wsMatcher(gData, x, RE_FALSE);
            break;
        case REOP_LETDIG:
            result = letdigMatcher(gData, x, RE_TRUE);
            break;
        case REOP_UNLETDIG:
            result = letdigMatcher(gData, x, RE_FALSE);
            break;
        case REOP_FLATN:
            offset = (REint32)GET_ARG(pc);
            pc += ARG_LEN;
            length = (REint32)GET_ARG(pc);
            pc += ARG_LEN;
            result = flatNMatcher(gData, x, gData->regexp->srcStart + offset, 
                                                                        length);
            break;
        case REOP_FLATNi:
            offset = (REint32)GET_ARG(pc);
            pc += ARG_LEN;
            length = (REint32)GET_ARG(pc);
            pc += ARG_LEN;
            result = flatNIMatcher(gData, x, gData->regexp->srcStart + offset,
                                                                        length);
            break;
        case REOP_FLAT1:
            matchCh = (REchar)GET_ARG(pc);
            pc += ARG_LEN;
            result = flatMatcher(gData, x, matchCh);
            break;
        case REOP_FLAT1i:
            matchCh = (REchar)GET_ARG(pc);
            pc += ARG_LEN;
            result = flatIMatcher(gData, x, matchCh);
            break;

        case REOP_ALT:
            nextpc = pc + GET_ARG(pc);
            nextop = (REOp)(*nextpc++);
            stateStack[stateStackTop].continuation = currentContinuation;
            ++stateStackTop;
            pushBackTrackState(gData, nextop, nextpc, x);
            pc += ARG_LEN;
            op = (REOp)(*pc++);
            continue;

        case REOP_ENDALT:
            --stateStackTop;
            currentContinuation = stateStack[stateStackTop].continuation;
            offset = (REint32)GET_ARG(pc);
            pc += offset;
            op = (REOp)(*pc++);
            continue;

            
        case REOP_PAREN:
            parenIndex = GET_ARG(pc);
            pc += ARG_LEN;
            x->parens[parenIndex].index = x->endIndex;
            x->parens[parenIndex].length = 0;
            op = (REOp)(*pc++);
            continue;
        case REOP_CLOSEPAREN:
            parenIndex = GET_ARG(pc);
            pc += ARG_LEN;
            x->parens[parenIndex].length = x->endIndex 
                                                - x->parens[parenIndex].index;
            if ((REint32)parenIndex > gData->lastParen)
                gData->lastParen = (REint32)parenIndex;
            op = (REOp)(*pc++);
            continue;
        case REOP_BACKREF:
            parenIndex = GET_ARG(pc);
            pc += ARG_LEN;
            result = backrefMatcher(gData, x, (uint32)parenIndex);
            break;

        case REOP_ASSERT:
            stateStack[stateStackTop].continuation = currentContinuation;
            stateStack[stateStackTop].parenCount = backTrackStackTop;
            stateStack[stateStackTop].index = x->endIndex;
            ++stateStackTop;
            if (!pushBackTrackState(gData, REOP_ASSERTTEST, 
                                                        pc + GET_ARG(pc), x))
                return NULL;
            pc += ARG_LEN;
            op = (REOp)(*pc++);
            continue;
        case REOP_ASSERTNOT:
            stateStack[stateStackTop].continuation = currentContinuation;
            stateStack[stateStackTop].parenCount = backTrackStackTop;
            stateStack[stateStackTop].index = x->endIndex;
            ++stateStackTop;
            if (!pushBackTrackState(gData, REOP_ASSERTNOTTEST, 
                                                        pc + GET_ARG(pc), x))
                return NULL;
            pc += ARG_LEN;
            op = (REOp)(*pc++);
            continue;
        case REOP_ASSERTTEST:
            --stateStackTop;
            x->endIndex = stateStack[stateStackTop].index;
            for (k = stateStack[stateStackTop].parenCount; 
                    k < backTrackStackTop; k++) {
                if (backTrackStack[k].precedingState)
                    free(backTrackStack[k].precedingState);
                free(backTrackStack[k].state);
            }
            backTrackStackTop = stateStack[stateStackTop].parenCount;
            currentContinuation = stateStack[stateStackTop].continuation;                
            if (result != NULL)
                result = x;
            break;
        case REOP_ASSERTNOTTEST:
            --stateStackTop;
            x->endIndex = stateStack[stateStackTop].index;
            for (k = stateStack[stateStackTop].parenCount; 
                    k < backTrackStackTop; k++) {
                if (backTrackStack[k].precedingState)
                    free(backTrackStack[k].precedingState);
                free(backTrackStack[k].state);
            }
            backTrackStackTop = stateStack[stateStackTop].parenCount;
            currentContinuation = stateStack[stateStackTop].continuation;                
            if (result == NULL)
                result = x;
            else
                result = NULL;
            break;

        case REOP_CLASS:
            index = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            result = classMatcher(gData, x, index);
            if (gData->error != RE_NO_ERROR) return NULL;
            break;

        case REOP_END:
            if (x != NULL)
                return x;
            break;

        case REOP_STAR:
            stateStack[stateStackTop].min = 0;
            stateStack[stateStackTop].max = -1;
            goto quantcommon;
        case REOP_PLUS:
            stateStack[stateStackTop].min = 1;
            stateStack[stateStackTop].max = -1;
            goto quantcommon;
        case REOP_OPT:
            stateStack[stateStackTop].min = 0;
            stateStack[stateStackTop].max = 1;
            goto quantcommon;
        case REOP_QUANT:
            stateStack[stateStackTop].min = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            stateStack[stateStackTop].max = (int32)GET_ARG(pc);
            pc += ARG_LEN;
quantcommon:
            stateStack[stateStackTop].parenCount = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            stateStack[stateStackTop].parenIndex = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            stateStack[stateStackTop].index = x->endIndex;
            stateStack[stateStackTop].continuation = currentContinuation;
            ++stateStackTop;
            currentContinuation.op = REOP_REPEAT;
            currentContinuation.pc = pc;
            if (!pushBackTrackState(gData, REOP_REPEAT, pc, x)) return NULL;
            pc += ARG_LEN;
            op = (REOp)(*pc++);
            continue;

        case REOP_ENDCHILD:
            pc = currentContinuation.pc;
            op = currentContinuation.op;
            continue;

        case REOP_REPEAT:
            --stateStackTop;
            if (result == NULL) {
                /*
                 *  There's been a failure, see if we have enough children
                 */
                currentContinuation = stateStack[stateStackTop].continuation;
                if (stateStack[stateStackTop].min == 0)
                    result = x;
                pc = pc + GET_ARG(pc);
                break;
            }
            else {
                if ((stateStack[stateStackTop].min == 0)
                        && (x->endIndex == stateStack[stateStackTop].index)) {
                    /* matched an empty string, that'll get us nowhere */
                    result = NULL;
                    currentContinuation = stateStack[stateStackTop].continuation;
                    pc = pc + GET_ARG(pc);
                    break;
                }
                if (stateStack[stateStackTop].min > 0) 
                                            stateStack[stateStackTop].min--;
                if (stateStack[stateStackTop].max != -1) 
                                            stateStack[stateStackTop].max--;
                if (stateStack[stateStackTop].max == 0) {
                    result = x;
                    currentContinuation = stateStack[stateStackTop].continuation;
                    pc = pc + GET_ARG(pc);
                    break;
                }
                stateStack[stateStackTop].index = x->endIndex;
                ++stateStackTop;
                currentContinuation.op = REOP_REPEAT;
                currentContinuation.pc = pc;
                if (!pushBackTrackState(gData, REOP_REPEAT, pc, x)) return NULL;
                pc += ARG_LEN;
                op = (REOp)(*pc++);
                parenIndex = (REuint32)stateStack[stateStackTop - 1].parenIndex;
                for (k = 0; k <= stateStack[stateStackTop - 1].parenCount; k++)
                    x->parens[parenIndex + k].index = -1;
            }
            continue;                       

        case REOP_MINIMALSTAR:
            stateStack[stateStackTop].min = 0;
            stateStack[stateStackTop].max = -1;
            goto minimalquantcommon;
        case REOP_MINIMALPLUS:
            stateStack[stateStackTop].min = 1;
            stateStack[stateStackTop].max = -1;
            goto minimalquantcommon;
        case REOP_MINIMALOPT:
            stateStack[stateStackTop].min = 0;
            stateStack[stateStackTop].max = 1;
            goto minimalquantcommon;
        case REOP_MINIMALQUANT:
            stateStack[stateStackTop].min = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            stateStack[stateStackTop].max = (int32)GET_ARG(pc);
            pc += ARG_LEN;
minimalquantcommon:
            stateStack[stateStackTop].parenCount = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            stateStack[stateStackTop].parenIndex = (int32)GET_ARG(pc);
            pc += ARG_LEN;
            stateStack[stateStackTop].index = x->endIndex;
            stateStack[stateStackTop].continuation = currentContinuation;
            ++stateStackTop;
            if (stateStack[stateStackTop - 1].min > 0) {
                currentContinuation.op = REOP_MINIMALREPEAT;
                currentContinuation.pc = pc;
                pc += ARG_LEN;
                op = (REOp)(*pc++);
            }
            else {
                if (!pushBackTrackState(gData, REOP_MINIMALREPEAT, pc, x))
                    return NULL;
                --stateStackTop;
                pc = pc + GET_ARG(pc);
                op = (REOp)(*pc++);
            }
            continue;

        case REOP_MINIMALREPEAT:
            --stateStackTop;
            currentContinuation = stateStack[stateStackTop].continuation;

            if (result == NULL) {
                /* 
                 * Non-greedy failure - try to consume another child
                 */
                if ((stateStack[stateStackTop].max == -1)
                        || (stateStack[stateStackTop].max > 0)) {
                    parenIndex = (REuint32)stateStack[stateStackTop].parenIndex;
                    for (k = 0; k <= stateStack[stateStackTop].parenCount; k++)
                        x->parens[parenIndex + k].index = -1;
                    stateStack[stateStackTop].index = x->endIndex;
                    stateStack[stateStackTop].continuation = currentContinuation;
                    ++stateStackTop;
                    currentContinuation.op = REOP_MINIMALREPEAT;
                    currentContinuation.pc = pc;
                    pc += ARG_LEN;
                    op = (REOp)(*pc++);
                    continue;
                }
                else {
                    currentContinuation = stateStack[stateStackTop].continuation;
                    break;
                }
            }
            else {
                if ((stateStack[stateStackTop].min == 0)
                        && (x->endIndex == stateStack[stateStackTop].index)) {
                    /* matched an empty string, that'll get us nowhere */
                    result = NULL;
                    currentContinuation = stateStack[stateStackTop].continuation;
                    break;
                }
                if (stateStack[stateStackTop].min > 0) 
                                                stateStack[stateStackTop].min--;
                if (stateStack[stateStackTop].max != -1) 
                                                stateStack[stateStackTop].max--;
                if (stateStack[stateStackTop].min > 0) {
                    parenIndex = (REuint32)stateStack[stateStackTop].parenIndex;
                    for (k = 0; k <= stateStack[stateStackTop].parenCount; k++)
                        x->parens[parenIndex + k].index = -1;
                    stateStack[stateStackTop].index = x->endIndex;
                    ++stateStackTop;
                    currentContinuation.op = REOP_MINIMALREPEAT;
                    currentContinuation.pc = pc;
                    pc += ARG_LEN;
                    op = (REOp)(*pc++);
                    continue;                       
                }
                else {
                    stateStack[stateStackTop].index = x->endIndex;
                    ++stateStackTop;
                    if (!pushBackTrackState(gData, REOP_MINIMALREPEAT, pc, x)) 
                        return NULL;
                    --stateStackTop;
                    pc = pc + GET_ARG(pc);
                    op = (REOp)(*pc++);
                    continue;
                }
            }



        }
        /*
         *  If the match failed and there's a backtrack option, take it.
         *  Otherwise this is a match failure.
         */
        if (result == NULL) {
            if (backTrackStackTop > 0) {
                backTrackStackTop--;
                backTrackData = &backTrackStack[backTrackStackTop];

                gData->lastParen = backTrackData->lastParen;

                recoverState(x, backTrackData->state);
                free(backTrackData->state);

                for (k = 0; k < backTrackData->precedingStateTop; k++) {
                    stateStack[k] = backTrackData->precedingState[k];
                }
                stateStackTop = (REuint32)backTrackData->precedingStateTop;
                if (backTrackData->precedingState)
                    free(backTrackData->precedingState);

                if (stateStackTop > 0)
                    currentContinuation = stateStack[stateStackTop - 1].continuation;
                
                pc = backTrackData->continuation.pc; 
                op = backTrackData->continuation.op;
                continue;
            }
            else
                return NULL;
        }
        else
            x = result;
        
        /*
         *  Continue with the expression. If this the end of the child, use
         *  the current continuation.
         */
        op = (REOp)*pc++;
        if (op == REOP_ENDCHILD) {
            pc = currentContinuation.pc;
            op = currentContinuation.op;
        }
    }
    return NULL;
}

/*
 *  Throw away the RegExp and all data associated with it.
 */
void REfreeRegExp(REState *pState)
{
    REuint32 i;
    if (pState->result) freeRENode(pState->result);
    if (pState->pc) free(pState->pc);
    for (i = 0; i < pState->classCount; i++) {
        free(pState->classList[i].bits);
    }
    if (pState->srcStart) free(pState->srcStart);
    free(pState->classList);
    free(pState);
}

RE_Error parseFlags(const REchar *flagsSource, REint32 flagsLength, REuint32 *flags)
{
    REint32 i;
    *flags = 0;

    for (i = 0; i < flagsLength; i++) {
        switch (flagsSource[i]) {
        case 'g':
            *flags |= RE_GLOBAL; break;
        case 'i':
            *flags |= RE_IGNORECASE; break;
        case 'm':
            *flags |= RE_MULTILINE; break;
        default:
            return RE_BAD_FLAG;
        }
    }
    return RE_NO_ERROR;
}

/* 
* Parse the regexp - errors are reported via the registered error function 
* and NULL is returned. Otherwise the regexp is compiled and the completed 
* ParseState returned.
*/
REState *REParse(const REchar *source, REint32 sourceLength, 
                      REuint32 flags, RE_Version version)
{
    REuint8 *endPC;
    RENode *t;
    REState *pState = (REState *)malloc(sizeof(REState));
    if (!pState) return NULL;
    pState->srcStart = (REchar *)malloc(sizeof(REchar) * sourceLength);
    if (!pState->srcStart) goto fail;
    memcpy(pState->srcStart, source, sizeof(REchar) * sourceLength);
    pState->srcEnd = pState->srcStart + sourceLength;
    pState->src = pState->srcStart;
    pState->parenCount = 0;
    pState->flags = flags;
    pState->version = version;
    pState->classList = NULL;
    pState->classCount = 0;
    pState->codeLength = 0;

    if (parseDisjunction(pState)) {
        t = pState->result;
        if (t) {
            while (t->next) t = t->next;    
            t->next = newRENode(pState, REOP_END);
            if (!t->next)
                goto fail;
        }
        else
            pState->result = newRENode(pState, REOP_END);
        if (pState->classCount) {
            pState->classList = (RECharSet *)malloc(sizeof(RECharSet)
                                                        * pState->classCount);
            if (!pState->classList) goto fail;
        }
        pState->pc = (REuint8 *)malloc(sizeof(REuint8) * pState->codeLength + 1);
        if (!pState->pc) goto fail;
        endPC = emitREBytecode(pState, pState->pc, pState->result);
        freeRENode(pState->result);
        pState->result = NULL;
        ASSERT(endPC <= (pState->pc + (pState->codeLength + 1)));
        return pState;
    }
fail:
    if (pState->srcStart) free(pState->srcStart);
    if (pState->classList) free(pState->classList);
    free(pState);
    return NULL;
}

static REMatchState *initMatch(REGlobalData *gData, REState *pState,
                            const REchar *text, REint32 length, int globalMultiline)
{
    REMatchState *result;
    REint32 j;

    if (!backTrackStack) {
        maxBackTrack = INITIAL_BACKTRACK;
        backTrackStack = (REBackTrackData *)malloc(sizeof(REBackTrackData) 
                                                            * maxBackTrack);
        if (!backTrackStack) {
            reportRegExpError(&gData->error, RE_OUT_OF_MEMORY);
            return NULL;
        }
    }
    if (!stateStack) {
        maxStateStack = INITIAL_STATESTACK;
        stateStack = (REProgState *)malloc(sizeof(REProgState) 
                                                         * maxStateStack);
        if (!stateStack) {
            reportRegExpError(&gData->error, RE_OUT_OF_MEMORY);
            return NULL;
        }
    }

    result = (REMatchState *)malloc(sizeof(REMatchState) 
                        + (pState->parenCount * sizeof(RECapture)));
    if (!result) {
        reportRegExpError(&gData->error, RE_OUT_OF_MEMORY);
        return NULL;
    }

    result->parenCount = (REint32)pState->parenCount;
    for (j = 0; j < result->parenCount; j++)
        result->parens[j].index = -1;
    result->startIndex = 0;
    result->endIndex = 0;

    pState->error = RE_NO_ERROR;
    
    gData->regexp = pState;
    gData->input = text;
    gData->length = length;
    gData->error = RE_NO_ERROR;
    gData->lastParen = 0;
    gData->globalMultiline = (REbool)globalMultiline;
    
    backTrackStackTop = 0;
    stateStackTop = 0;
    return result;
}

/*
 *  The [[Match]] implementation
 *
 */
REMatchState *REMatch(REState *pState, const REchar *text, REint32 length)
{
    REint32 j;
    REGlobalData gData;
    REMatchState *result;
    REMatchState *x = initMatch(&gData, pState, text, length, 0);
    if (!x)
        return NULL;

    result = executeREBytecode(pState->pc, &gData, x);
    for (j = 0; j < backTrackStackTop; j++) {
        if (backTrackStack[j].precedingState)
            free(backTrackStack[j].precedingState);
        free(backTrackStack[j].state);
    }
    if (gData.error != RE_NO_ERROR) return NULL;
    return result;
}

/*
 *  Execute the RegExp against the supplied text, filling in the REMatchState.
 *
 */
REMatchState *REExecute(REState *pState, const REchar *text, REint32 offset, REint32 length, int globalMultiline)
{
    REMatchState *result;

    REGlobalData gData;
    REint32 i;
    
    REMatchState *x = initMatch(&gData, pState, text, length, globalMultiline);
    if (!x)
        return NULL;

    x->startIndex = offset;
    x->endIndex = offset;
    while (RE_TRUE) {
        result = executeREBytecode(pState->pc, &gData, x);
        for (i = 0; i < backTrackStackTop; i++) {
            if (backTrackStack[i].precedingState)
                free(backTrackStack[i].precedingState);
            free(backTrackStack[i].state);
        }

        backTrackStackTop = 0;
        stateStackTop = 0;
        if (gData.error != RE_NO_ERROR) {
            pState->error = gData.error;
            return NULL;
        }
        if (result == NULL) {
            x->startIndex++;
            if (x->startIndex > length) {
                free(x);
                return NULL;
            }
            x->endIndex = x->startIndex;
        }
        else
            break;
    }
    return result;
}

#ifdef STANDALONE

REchar *widen(char *str, int length)
{
    int i;
    REchar *result = (REchar *)malloc(sizeof(REchar) * (length + 1));
    for (i = 0; i < length; i++)
        result[i] = str[i];
    return result;
}

REchar canonicalize(REchar ch)
{
    if ((ch >= 'a') && (ch <= 'z'))
        return (ch - 'a') + 'A';
    else
        return ch;
}

int main(int argc, char* argv[])
{
    char regexpInput[128];
    char *regexpSrc;
    char str[128];
    REMatchState *result;
    int regexpLength;
    char *flagSrc;
    int flagSrcLength;
    REint32 i, j;

    printf("Delimit regexp by /  / (with flags following) and strings by \"  \"\n");
    while (RE_TRUE) {
        REchar *regexpWideSrc;
        REchar *flagWideSrc;
        REState *pState;

        printf("regexp : ");
        scanf("%s", regexpInput);
        regexpSrc = regexpInput;
        if (*regexpSrc != '/')
            break;
        regexpSrc++;
        flagSrc = strrchr(regexpSrc, '/');
        if (flagSrc == NULL)
            break;
        regexpLength = flagSrc - regexpSrc;
        if (flagSrc[1]) {
            flagSrc++;
            flagSrcLength = strlen(flagSrc);
        }
        else {
            flagSrc = NULL;
            flagSrcLength = 0;
        }

        regexpWideSrc = widen(regexpSrc, regexpLength);
        flagWideSrc = widen(flagSrc, flagSrcLength);
        pState = REParse(regexpWideSrc, regexpLength, 
                                flagWideSrc, flagSrcLength, RE_TRUE);
        if (pState) {
            while (RE_TRUE) {
                printf("string : ");
                scanf("%s", str);
                if (*str != '"') 
                    break;
                else {
                    int strLength = strlen(str + 1) - 1;
                    REchar *widestr = widen(str + 1, strLength);
                    result = REExecute(pState, widestr, strLength);
                    if (result) {
                        printf("\"");
                        for (i = result->startIndex; i < result->endIndex; i++) 
                            printf("%c", str[i + 1]);
                        printf("\"");
                        for (i = 0; i < result->n; i++) {
                            printf(",");
                            if (result->parens[i].index != -1) {
                                printf("\"");
                                for (j = 0; j < result->parens[i].length; j++) 
                                    printf("%c", str[j + 1 + result->parens[i].index]);
                                printf("\"");
                            }
                            else
                                printf("undefined");
                        }
                        printf("\n");
                        free(result);
                    }
                    else
                        printf("failed\n");
                    free(widestr);
                }
            }
            freeRegExp(pState);
        }
        else
            printf("regexp failed to parse\n");
        free(regexpWideSrc);
        free(flagWideSrc);
    }
    return 0;
}
#endif
