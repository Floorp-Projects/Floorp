/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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


typedef unsigned char uint8;


typedef enum REOp {
    REOP_EMPTY,         /* an empty alternative */

    REOP_ALT,           /* a tree of alternates */
    REOP_NEXTALT,       /* continuation into thereof */

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
    REOP_ASSERTNOT,     /* '(?! ... )' */
    REOP_ASSERTTEST,    /* continuation point for above */

    REOP_FLAT,          /* literal characters (data.length count) */
    REOP_FLATi,         /* ditto, with ignorecase */


    REOP_DEC,           /* decimal digit '\d' */
    REOP_UNDEC,         /* not a decimal digit '\D' */

    REOP_WS,            /* whitespace or line terminator '\s' */
    REOP_UNWS,          /* not whitespace '\S' */

    REOP_LETDIG,        /* letter or digit or '_' '\w' */
    REOP_UNLETDIG,      /* not letter or digit or '_' '\W' */

    REOP_QUANT,         /* '+', '*', '?' or '{..}' */
    REOP_REPEAT,        /* continuation into quant */
    REOP_MINIMALREPEAT, /* continuation into quant */

    REOP_END

} REOp;

#define RE_ISDEC(c)         ( (c >= '0') && (c <= '9') )
#define RE_UNDEC(c)         (c - '0')

#define RE_ISWS(c)          ( (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') )
#define RE_ISLETTER(c)      ( ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) )
#define RE_ISLETDIG(c)      ( RE_ISLETTER(c) || RE_ISDEC(c) )

#define RE_ISLINETERM(c)    ( (c == '\n') || (c == '\r') )


typedef struct CharSet {
    uint8 *bits;
    REuint32 length;
} CharSet;

typedef struct REContinuationData {
    REOp op;              /* not necessarily the same as node->kind */
    RENode *node;
} REContinuationData;

typedef struct RENode {

    REContinuationData continuation;

    REOp kind;

    RENode *next;       /* links consecutive terms */

    void *child;
    
    REuint32 parenIndex;

        
    REuint32 index;               /* runtime-data */
    REint32 count;
    
    union {
        void *child2;
        struct {
            REint32 min;
            REint32 max;          /* -1 for infinity */
            REbool greedy;
            REint32 parenCount;  /* #parens in quantified term */
        } quantifier;
        struct {
            REchar ch;
            REuint32 length;
        } flat;
        struct {
            const REchar *end;
            CharSet *charSet;
            REbool sense;
        } chclass;
    } data;

} RENode;

typedef struct RENodeState {
    RENode *node;
    REint32 count;
    REuint32 index;
} RENodeState;

#define INITIAL_STATESTACK (20)
RENodeState *nodeStateStack;
REuint32 nodeStateStackTop;
REuint32 maxNodeStateStack;

typedef struct REGlobalData {
    REuint32 flags;             /* flags from the RE in execution */   
    REuint32 length;            /* length of input string */
    const REchar *input;        /* the input string */
    REError error;              /* runtime error code (out_of_memory only?) */
} REGlobalData;


typedef struct REBackTrackData {
    REContinuationData continuation;    /* where to backtrack to */
    REState *state;                     /* the state of the match */

    RENodeState nodeState;              /* the state of the executing node */

    RENodeState *precedingNodeState;    /* the state of all parent nodes */
    REint32 precedingNodeStateTop;

} REBackTrackData;

#define INITIAL_BACKTRACK (20)
REint32 maxBackTrack;
REBackTrackData *backTrackStack = NULL;
REint32 backTrackStackTop;

/*
    Allocate space for a state and copy x into it.
*/
static REState *copyState(REState *x)
{
    REState *result = (REState *)malloc(sizeof(REState) 
                                            + (x->n * sizeof(RECapture)));
    memcpy(result, x, sizeof(REState) + (x->n * sizeof(RECapture)));
    return result;
}

/*
    Copy state.
*/
static void recoverState(REState *into, REState *from)
{
    memcpy(into, from, sizeof(REState) + (from->n * sizeof(RECapture)));
}

/*
    Bottleneck for any errors.
*/
static void reportRegExpError(REError *errP, REError err) 
{
    *errP = err;
}

/*
    Push the back track data, growing the stack as necessary.
*/
static REBackTrackData *pushBackTrack(REGlobalData *gData, REOp op, 
                                      RENode *node, REState *x)
{
    REBackTrackData *result;
    if (backTrackStackTop == maxBackTrack) {
        maxBackTrack <<= 1;
        backTrackStack = (REBackTrackData *)realloc(backTrackStack, 
                                sizeof(REBackTrackData) * maxBackTrack);
        if (!backTrackStack) {
            reportRegExpError(&gData->error, OUT_OF_MEMORY);
            return NULL;
        }
    }
    result = &backTrackStack[backTrackStackTop++];
    result->continuation.op = op;
    result->continuation.node = node;
    result->state = copyState(x);
    result->nodeState.count = node->count;
    result->nodeState.index = node->index;

    result->precedingNodeStateTop = nodeStateStackTop;
    if (nodeStateStackTop) {
        result->precedingNodeState = (RENodeState *)malloc(sizeof(RENodeState) 
                                                        * nodeStateStackTop);
        if (!result->precedingNodeState) {
            reportRegExpError(&gData->error, OUT_OF_MEMORY);
            return NULL;
        }
        memcpy(result->precedingNodeState, nodeStateStack, sizeof(RENodeState) 
                                                         * nodeStateStackTop);
    }
    else
        result->precedingNodeState = NULL;

    return result;
}

static REbool pushNodeState(REGlobalData *gData, RENode *node, REint32 count,
                     REuint32 index)
{
    if (nodeStateStackTop == maxNodeStateStack) {
        maxNodeStateStack <<= 1;
        nodeStateStack = (RENodeState *)realloc(nodeStateStack, 
                                sizeof(RENodeState) * maxNodeStateStack);
        if (!nodeStateStack) {
            reportRegExpError(&gData->error, OUT_OF_MEMORY);
            return false;
        }
    }
    nodeStateStack[nodeStateStackTop].node = node;
    nodeStateStack[nodeStateStackTop].count = count;
    nodeStateStack[nodeStateStackTop].index = index;
    nodeStateStackTop++;
    return true;
}

/* forward declarations for parser routines */
REbool parseDisjunction(REParseState *parseState);
REbool parseAlternative(REParseState *parseState);
REbool parseTerm(REParseState *parseState);


static REbool isASCIIHexDigit(REchar c, REuint32 *digit)
{
    REuint32 cv = c;

    if (cv < '0')
        return false;
    if (cv <= '9') {
        *digit = cv - '0';
        return true;
    }
    cv |= 0x20;
    if (cv >= 'a' && cv <= 'f') {
        *digit = cv - 'a' + 10;
        return true;
    }
    return false;
}


/*
    Allocate & initialize a new node.
*/
static RENode *newRENode(REParseState *parseState, REOp kind)
{
    RENode *result = (RENode *)malloc(sizeof(RENode));
    if (result == NULL) {
        reportRegExpError(&parseState->error, OUT_OF_MEMORY);
        return NULL;
    }
    result->kind = kind;
    if (kind == REOP_FLAT) {
        if (parseState->flags & IGNORECASE)
            result->kind = REOP_FLATi;
        result->data.flat.length = 1;
    }
    result->next = NULL;
    result->child = NULL;
    return result;
}

REbool parseDisjunction(REParseState *parseState)
{
    if (!parseAlternative(parseState)) return false;
    
    if ((parseState->src != parseState->srcEnd) 
                    && (*parseState->src == '|')) {
        RENode *altResult;
        ++parseState->src;
        altResult = newRENode(parseState, REOP_ALT);
        if (!altResult) return false;
        altResult->child = parseState->result;
        if (!parseDisjunction(parseState)) return false;
        altResult->data.child2 = parseState->result;
        parseState->result = altResult;
    }
    return true;
}

/*
 *   Return a single REOP_Empty node for an empty Alternative, 
 *   a single REOP_xxx node for a single term and a next field 
 *   linked list for a list of terms for more than one term. 
 *   Consecutive FLAT1 nodes get combined into a single FLATN
 */
REbool parseAlternative(REParseState *parseState)
{
    RENode *headTerm = NULL;
    RENode *tailTerm = NULL;
    while (true) {
        if ((parseState->src == parseState->srcEnd)
                || (*parseState->src == ')')
                || (*parseState->src == '|')) {
            if (!headTerm) {
                parseState->result = newRENode(parseState, REOP_EMPTY);
                if (!parseState->result) return false;
            }
            else
                parseState->result = headTerm;
            return true;
        }
        if (!parseTerm(parseState)) return false;
        if (headTerm == NULL)
            headTerm = parseState->result;
        else {
            if (tailTerm == NULL) {
                if (((headTerm->kind == REOP_FLAT) 
                                || (headTerm->kind == REOP_FLATi))
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
                }
            }
            else {
                if (((tailTerm->kind == REOP_FLAT) 
                                || (tailTerm->kind == REOP_FLATi))
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
                }
            }
        }
    }
}

static REint32 getDecimalValue(REchar c, REParseState *parseState)
{
    REint32 value = RE_UNDEC(c);
    c = *parseState->src; 
    while (RE_ISDEC(c) && (parseState->src < parseState->srcEnd)) { 
        value = (10 * value) + RE_UNDEC(c); 
        c = *++parseState->src;
    }
    return value;
}

/* calculate the total size of the bitmap required for a class expression */

static REbool calculateBitmapSize(REParseState *pState, RENode *target)

{

    REchar rangeStart;
    const REchar *src = (const REchar *)(target->child);
    const REchar *end = target->data.chclass.end;

    REchar c;
    REuint32 nDigits;
    REuint32 i;
    REuint32 max = 0;
    bool inRange = false;

    CharSet *charSet = (CharSet *)malloc(sizeof(CharSet));
    if (!charSet) return false;
    target->data.chclass.charSet = charSet;
    charSet->length = 0;
    charSet->bits = NULL;

    if (src == end)
        return true;

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
                    reportRegExpError(&pState->error, WRONG_RANGE);
                    return false;
                }
                localMax = '9';
                break;
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                if (inRange) {
                    reportRegExpError(&pState->error, WRONG_RANGE);
                    return false;
                }
                charSet->length = 65536;
                return true;
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
                reportRegExpError(&pState->error, WRONG_RANGE);
                return false;
            }
            inRange = false;
        }
        else {
            if (src < (end - 1)) {
                if (*src == '-') {
                    ++src;
                    inRange = true;
                    rangeStart = (REchar)localMax;
                    continue;
                }
            }
        }
        if (pState->flags & IGNORECASE) {
            c = canonicalize((REchar)localMax);
            if (c > localMax)
                localMax = c;
        }
        if (localMax > max)
            max = localMax;
    }
    charSet->length = max + 1;
    return true;
}


REbool parseTerm(REParseState *parseState)
{
    REchar c = *parseState->src++;
    REuint32 nDigits;
    REuint32 parenBaseCount = parseState->parenCount;
    REuint32 num, tmp;
    RENode *term;

    switch (c) {
    /* assertions and atoms */
    case '^':
        parseState->result = newRENode(parseState, REOP_BOL);
        if (!parseState->result) return false;
        break;
    case '$':
        parseState->result = newRENode(parseState, REOP_EOL);
        if (!parseState->result) return false;
        break;
    case '\\':
        if (parseState->src < parseState->srcEnd) {
            c = *parseState->src++;
            switch (c) {
            /* assertion escapes */
            case 'b' :
                parseState->result = newRENode(parseState, REOP_WBND);
                if (!parseState->result) return false;
                break;
            case 'B':
                parseState->result = newRENode(parseState, REOP_UNWBND);
                if (!parseState->result) return false;
                break;
            /* Decimal escape */
            case '0':
                if (parseState->oldSyntax) {
                    /* octal escape (supported for backward compatibility) */
                    num = 0;
                    while ((parseState->src < parseState->srcEnd) 
                                && ('0' <= (c = *parseState->src++))
                                && (c <= '7')) {
                        tmp = 8 * num + (REuint32)RE_UNDEC(c);
                        if (tmp > 0377)
                            break;
                        num = tmp;
                    }
                    parseState->src--;
                    parseState->result = newRENode(parseState, REOP_FLAT);
                    if (!parseState->result) return false;
                    parseState->result->data.flat.ch = (REchar)(num);
                }
                else {
                    parseState->result = newRENode(parseState, REOP_FLAT);
                    if (!parseState->result) return false;
                    parseState->result->data.flat.ch = 0;
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
                parseState->result = newRENode(parseState, REOP_BACKREF);
                if (!parseState->result) return false;
                parseState->result->parenIndex
                             = (REuint32)(getDecimalValue(c, parseState) - 1);
                break;
            /* Control escape */
            case 'f':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
                parseState->result->data.flat.ch = 0xC;
                break;
            case 'n':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
                parseState->result->data.flat.ch = 0xA;
                break;
            case 'r':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
                parseState->result->data.flat.ch = 0xD;
                break;
            case 't':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
                parseState->result->data.flat.ch = 0x9;
                break;
            case 'v':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
                parseState->result->data.flat.ch = 0xB;
                break;
            /* Control letter */
            case 'c':
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
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
                if (!parseState->result) return false;
                {
                    REuint32 n = 0;
                    REuint32 i;
                    for (i = 0; (i < nDigits) 
                            && (parseState->src < parseState->srcEnd); i++) {
                        REuint32 digit;
                        c = *parseState->src++;
                        if (!isASCIIHexDigit(c, &digit)) {
                            /* back off to accepting the original 
                             *'u' or 'x' as a literal 
                             */
                            parseState->src -= (i + 2);
                            n = *parseState->src++;
                            break;
                        }
                        n = (n << 4) | digit;
                    }
                    parseState->result->data.flat.ch = (REchar)(n);
                }
                break;
            /* Character class escapes */
            case 'd':
                parseState->result = newRENode(parseState, REOP_DEC);
                if (!parseState->result) return false;
                break;
            case 'D':
                parseState->result = newRENode(parseState, REOP_UNDEC);
                if (!parseState->result) return false;
                break;
            case 's':
                parseState->result = newRENode(parseState, REOP_WS);
                if (!parseState->result) return false;
                break;
            case 'S':
                parseState->result = newRENode(parseState, REOP_UNWS);
                if (!parseState->result) return false;
                break;
            case 'w':
                parseState->result = newRENode(parseState, REOP_LETDIG);
                if (!parseState->result) return false;
                break;
            case 'W':
                parseState->result = newRENode(parseState, REOP_UNLETDIG);
                if (!parseState->result) return false;
                break;
            /* IdentityEscape */
            default:
                parseState->result = newRENode(parseState, REOP_FLAT);
                if (!parseState->result) return false;
                parseState->result->data.flat.ch = c;
                parseState->result->child = (void *)(parseState->src - 1);
                break;
            }
            break;
        }
        else {
            /* a trailing '\' is an error */
            reportRegExpError(&parseState->error, TRAILING_SLASH);
            return false;
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
                    if (!result) return false;
                    break;
                case '!':
                    result = newRENode(parseState, REOP_ASSERTNOT);
                    if (!result) return false;
                    break;
                }
            }
            else {
                result = newRENode(parseState, REOP_PAREN);
                if (!result) return false;
                result->parenIndex = parseState->parenCount++;
            }
            if (!parseDisjunction(parseState)) return false;
            if ((parseState->src == parseState->srcEnd)
                    || (*parseState->src != ')')) {
                reportRegExpError(&parseState->error, UNCLOSED_PAREN);
                return false;
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
        if (!parseState->result) return false;
        parseState->result->child = (void *)(parseState->src);
        parseState->result->data.chclass.charSet = NULL;
        while (true) {
            if (parseState->src == parseState->srcEnd) {
                reportRegExpError(&parseState->error, UNCLOSED_CLASS);
                return false;
            }
            if (*parseState->src == '\\') {
                ++parseState->src;
                if (RE_ISDEC(*parseState->src)) {
                    reportRegExpError(&parseState->error, BACKREF_IN_CLASS);
                    return false;
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
        if (!calculateBitmapSize(parseState, parseState->result))
            return false;
        break;

    case '.':
        parseState->result = newRENode(parseState, REOP_DOT);
        if (!parseState->result) return false;
        break;
    default:
        parseState->result = newRENode(parseState, REOP_FLAT);
        if (!parseState->result) return false;
        parseState->result->data.flat.ch = c;
        parseState->result->child = (void *)(parseState->src - 1);
        break;
    }

    term = parseState->result;
    if (parseState->src < parseState->srcEnd) {
        switch (*parseState->src) {
        case '+':
            parseState->result = newRENode(parseState, REOP_QUANT);
            if (!parseState->result) return false;
            parseState->result->data.quantifier.min = 1;
            parseState->result->data.quantifier.max = -1;
            goto quantifier;
        case '*':
            parseState->result = newRENode(parseState, REOP_QUANT);
            if (!parseState->result) return false;
            parseState->result->data.quantifier.min = 0;
            parseState->result->data.quantifier.max = -1;
            goto quantifier;
        case '?':
            parseState->result = newRENode(parseState, REOP_QUANT);
            if (!parseState->result) return false;
            parseState->result->data.quantifier.min = 0;
            parseState->result->data.quantifier.max = 1;
            goto quantifier;
        case '{':
            {
                REint32 min = 0;
                REint32 max = -1;
                REchar c;
                ++parseState->src;
            
                parseState->result = newRENode(parseState, REOP_QUANT);
                if (!parseState->result) return false;

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
                if (c == '}')
                    goto quantifier;
                else {
                    reportRegExpError(&parseState->error, UNCLOSED_BRACKET);
                    return false;
                }
            }
        }
    }
    return true;

quantifier:
    ++parseState->src;
    parseState->result->child = term;
    parseState->result->parenIndex = parenBaseCount;
    parseState->result->data.quantifier.parenCount 
                                = parseState->parenCount - parenBaseCount;
    if ((parseState->src < parseState->srcEnd) && (*parseState->src == '?')) {
        ++parseState->src;
        parseState->result->data.quantifier.greedy = false;
    }
    else
        parseState->result->data.quantifier.greedy = true;  
    return true;
}






/*
1. Let e be x's endIndex.
2. If e is zero, return true.
3. If Multiline is false, return false.
4. If the character Input[e-1] is one of the line terminator characters <LF>,
   <CR>, <LS>, or <PS>, return true.
5. Return false.
*/
static REState *bolMatcher(REGlobalData *globalData, REState *x)
{
    REuint32 e = x->endIndex;
    if (e != 0) {
        if (globalData->flags & MULTILINE) {
            if (!RE_ISLINETERM(globalData->input[e - 1]))
                return NULL;
        }
        else
            return NULL;
    }
    return x;
}

/*
1. Let e be x's endIndex.
2. If e is equal to InputLength, return true.
3. If multiline is false, return false.
4. If the character Input[e] is one of the line terminator characters <LF>, 
   <CR>, <LS>, or <PS>, return true.
5. Return false.
*/
static REState *eolMatcher(REGlobalData *globalData, REState *x)
{
    REuint32 e = x->endIndex;
    if (e != globalData->length) {
        if (globalData->flags & MULTILINE) {
            if (!RE_ISLINETERM(globalData->input[e]))
                return NULL;
        }
        else
            return NULL;
    }
    return x;
}


/*
1. If e == -1 or e == InputLength, return false.
2. Let c be the character Input[e].
3. If c is one of the sixty-three characters in the table below, return true.
a b c d e f g h i j k l m n o p q r s t u v w x y z
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
0 1 2 3 4 5 6 7 8 9 _
4. Return false.
*/
static REbool isWordChar(REint32 e, REGlobalData *globalData)
{
    REchar c;
    if ((e == -1) || (e == (REint32)(globalData->length)))
        return false;
    c = globalData->input[e];
    if (RE_ISLETDIG(c) || (c == '_'))
        return true;
    return false;
}

/*
1. Let e be x's endIndex.
2. Call IsWordChar(e-1) and let a be the boolean result.
3. Call IsWordChar(e) and let b be the boolean result.
for '\b'
4. If a is true and b is false, return true.
5. If a is false and b is true, return true.
6. Return false.

for '\B'
4. If a is true and b is false, return false.
5. If a is false and b is true, return false.
6. Return true.
*/
static REState *wbndMatcher(REGlobalData *globalData, REState *x, REbool sense)
{
    REint32 e = (REint32)(x->endIndex);

    REbool a = isWordChar(e - 1, globalData);
    REbool b = isWordChar(e, globalData);
    
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
2. Call CharacterSetMatcher(A, false) and return its Matcher result.
*/
static REState *dotMatcher(REGlobalData *globalData, REState *x)
{
    REchar ch;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    ch = globalData->input[e];
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
static REState *decMatcher(REGlobalData *globalData, REState *x, REbool sense)
{
    REchar ch;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    ch = globalData->input[e];
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
static REState *wsMatcher(REGlobalData *globalData, REState *x, REbool sense)
{
    REchar ch;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    ch = globalData->input[e];
    if (RE_ISWS(ch) != sense)
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
static REState *letdigMatcher(REGlobalData *globalData, REState *x, REbool sense)
{
    REchar ch;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    ch = globalData->input[e];
    if (RE_ISLETDIG(ch) != sense)
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
    5. If invert is true, go to step 8.
    6. If there does not exist a member a of set A such that Canonicalize(a) 
       == cc, then return failure.
    7. Go to step 9.
    8. If there exists a member a of set A such that Canonicalize(a) == cc,
       then return failure.
    9. Let cap be x's captures internal array.
    10. Let y be the State (e+1, cap).
    11. Call c(y) and return its result.
*/
static REState *flatMatcher(REGlobalData *globalData, REState *x, REchar matchCh)
{
    REchar ch;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    ch = globalData->input[e];

    if (ch != matchCh)
        return NULL;
    x->endIndex++;
    return x;
}

static REState *flatIMatcher(REGlobalData *globalData, REState *x, REchar matchCh)
{
    REchar ch;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    ch = globalData->input[e];

    if (canonicalize(ch) != canonicalize(matchCh))
        return NULL;
    x->endIndex++;
    return x;
}

/*
    Consecutive literal characters.
*/
static REState *flatNMatcher(REGlobalData *globalData, REState *x, 
                             REchar *matchChars, REuint32 length)
{
    REuint32 e = x->endIndex;
    REuint32 i;
    if ((e + length) > globalData->length)
        return NULL;
    for (i = 0; i < length; i++) {
        if (matchChars[i] != globalData->input[e + i])
            return NULL;
    }
    x->endIndex += length;
    return x;
}

static REState *flatNIMatcher(REGlobalData *globalData, REState *x,
                              REchar *matchChars, REuint32 length)
{
    REuint32 e = x->endIndex;
    REuint32 i;
    if ((e + length) > globalData->length)
        return NULL;
    for (i = 0; i < length; i++) {
        if (canonicalize(matchChars[i]) 
                != canonicalize(globalData->input[e + i]))
            return NULL;
    }
    x->endIndex += length;
    return x;
}

/* Add a single character to the CharSet */

static void addCharacterToCharSet(CharSet *cs, REchar c)

{
    REuint32 byteIndex = (REuint32)(c / 8);
    ASSERT(c < cs->length);
    cs->bits[byteIndex] |= 1 << (c & 0x7);
}


/* Add a character range, c1 to c2 (inclusive) to the CharSet */

static void addCharacterRangeToCharSet(CharSet *cs, REchar c1, REchar c2)

{
    REuint32 i;

    REuint32 byteIndex1 = (REuint32)(c1 / 8);
    REuint32 byteIndex2 = (REuint32)(c2 / 8);

    ASSERT((c2 <= cs->length) && (c1 <= c2));

    c1 &= 0x7;
    c2 &= 0x7;

    if (byteIndex1 == byteIndex2) {
        cs->bits[byteIndex1] |= ((uint8)(0xFF) >> (7 - (c2 - c1))) << c1;
    }
    else {
        cs->bits[byteIndex1] |= 0xFF << c1;
        for (i = byteIndex1 + 1; i < byteIndex2; i++)
            cs->bits[i] = 0xFF;
        cs->bits[byteIndex2] |= (uint8)(0xFF) >> (7 - c2);
    }
}


/* Compile the source of the class into a CharSet */

static REbool processCharSet(REGlobalData *globalData, RENode *target)

{

    REchar rangeStart, thisCh;
    const REchar *src = (const REchar *)(target->child);
    const REchar *end = target->data.chclass.end;


    REchar c;
    REint32 nDigits;
    REint32 i;
    REuint32 length;
    CharSet *charSet;

    bool inRange = false;

    charSet = target->data.chclass.charSet;
    length = (charSet->length / 8) + 1;
    charSet->bits = (uint8 *)malloc(length);

    if (!charSet->bits)
        return false;

    memset(charSet->bits, 0, length);
    target->data.chclass.sense = true;

    if (src == end)
        return true;

    if (*src == '^') {
        target->data.chclass.sense = false;
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
                    if (RE_ISWS(i))
                        addCharacterToCharSet(charSet, (REchar)(i));
                continue;
            case 'S':
                for (i = (REint32)(charSet->length - 1); i >= 0; i--)
                    if (!RE_ISWS(i))
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
            if (globalData->flags & IGNORECASE) {
                REchar minch = (REchar)65535;
                REchar maxch = 0;
                /*

                    yuk
                
                */
                if (rangeStart < minch) minch = rangeStart;
                if (thisCh < minch) minch = thisCh;
                if (canonicalize(rangeStart) < minch) minch = canonicalize(rangeStart);
                if (canonicalize(thisCh) < minch) minch = canonicalize(thisCh);

                if (rangeStart > maxch) maxch = rangeStart;
                if (thisCh > maxch) maxch = thisCh;
                if (canonicalize(rangeStart) > maxch) maxch = canonicalize(rangeStart);
                if (canonicalize(thisCh) > maxch) maxch = canonicalize(thisCh);
                addCharacterRangeToCharSet(charSet, minch, maxch);
            }
            else
                addCharacterRangeToCharSet(charSet, rangeStart, thisCh);
            inRange = false;
        }
        else {
            if (globalData->flags & IGNORECASE)
                addCharacterToCharSet(charSet, canonicalize(thisCh));
            addCharacterToCharSet(charSet, thisCh);
            if (src < (end - 1)) {
                if (*src == '-') {
                    ++src;
                    inRange = true;
                    rangeStart = thisCh;
                }
            }
        }
    }
    return true;
}


/*
    Initialize the character set if it this is the first call.
    Test the bit - if the ^ flag was specified, non-inclusion is a success
*/
static REState *classMatcher(REGlobalData *globalData, 
                             REState *x, RENode *target)
{
    REchar ch;
    CharSet *charSet;
    REuint32 byteIndex;
    REuint32 e = x->endIndex;
    if (e == globalData->length)
        return NULL;
    if (target->data.chclass.charSet->bits == NULL) {
        if (!processCharSet(globalData, target))
            return NULL;
    }
    charSet = target->data.chclass.charSet;
    ch = globalData->input[e];
    byteIndex = (REuint32)(ch / 8);
    if (target->data.chclass.sense) {

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
4. Let A be a one-element CharSet containing the character ch.
5. Call CharacterSetMatcher(A, false) and return its Matcher result.
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

static REState *backrefMatcher(REGlobalData *globalData, 
                               REState *x, RENode *child)
{
    REuint32 e;
    REuint32 len;
    REuint32 f;
    REuint32 i;
    const REchar *parenContent;
    RECapture *s = &x->parens[child->parenIndex];
    if (s->index == -1)
        return x;

    e = x->endIndex;
    len = s->length;
    f = e + len;
    if (f > globalData->length)
        return NULL;
    
    parenContent = &globalData->input[s->index];
    if (globalData->flags & IGNORECASE) {
        for (i = 0; i < len; i++) {
            if (canonicalize(parenContent[i]) 
                            != canonicalize(globalData->input[e + i]))
                return NULL;
        }
    }
    else {
        for (i = 0; i < len; i++) {
            if (parenContent[i] != globalData->input[e + i])
                return NULL;
        }
    }
    x->endIndex = f;
    return x;
}

/*
    free memory the RENode t and it's children, plus any attached memory
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
        case REOP_CLASS:
            if (t->data.chclass.charSet)
                free(t->data.chclass.charSet);
            break;
        }
        n = t->next;
        free(t);
        t = n;
    }
}

/*
 *  Throw away the RegExp and all data associated with it.
 */
void freeRegExp(REParseState *pState)
{
    freeRENode(pState->result);
    free(pState);
}

/*
 *  Iterate over the linked RENodes, attempting a match at each. When the
 *  end of list is reached, the 'currentContinuation' specifies the next
 *  node and op (which may be different from that in the node).
 *  The backTrackStack maintains a stack of so-far succesful states. When
 *  a match fails the state on the top of the stack is re-instated.
 *
 */
static REState *executeRENode(RENode *t, REGlobalData *globalData, REState *x)
{
    REOp op = t->kind;
    REContinuationData currentContinuation;
    REState *result;
    REBackTrackData *backTrackData;
    REint32 k;

    currentContinuation.node = NULL;

    while (true) {      /* loop over next links & current continuation */
        switch (op) {
        case REOP_EMPTY:
            result = x;
            break;
        case REOP_BOL:
            result = bolMatcher(globalData, x);
            break;
        case REOP_EOL:
            result = eolMatcher(globalData, x);
            break;
        case REOP_WBND:
            result = wbndMatcher(globalData, x, true);
            break;
        case REOP_UNWBND:
            result = wbndMatcher(globalData, x, false);
            break;
        case REOP_DOT:
            result = dotMatcher(globalData, x);
            break;
        case REOP_DEC:
            result = decMatcher(globalData, x, true);
            break;
        case REOP_UNDEC:
            result = decMatcher(globalData, x, false);
            break;
        case REOP_WS:
            result = wsMatcher(globalData, x, true);
            break;
        case REOP_UNWS:
            result = wsMatcher(globalData, x, false);
            break;
        case REOP_LETDIG:
            result = letdigMatcher(globalData, x, true);
            break;
        case REOP_UNLETDIG:
            result = letdigMatcher(globalData, x, false);
            break;
        case REOP_FLAT:
            if (t->child)
                result = flatNMatcher(globalData, x, (REchar *)(t->child),
                                                         t->data.flat.length);
            else
                result = flatMatcher(globalData, x, t->data.flat.ch);
            break;
        case REOP_FLATi:
            if (t->child)
                result = flatNIMatcher(globalData, x, (REchar *)(t->child),
                                                         t->data.flat.length);
            else
                result = flatIMatcher(globalData, x, t->data.flat.ch);
            break;

/* keep the current continuation and provide the alternate path 
 * as a back track opportunity 
 */
        case REOP_ALT:  
            t->continuation = currentContinuation;
            currentContinuation.node = t;
            currentContinuation.op = REOP_NEXTALT;
            if (!pushBackTrack(globalData, REOP_NEXTALT, t, x)) return NULL;
            t = (RENode *)(t->child);
            ASSERT(t);
            op = t->kind;
            continue;
        case REOP_NEXTALT:
            if (result == NULL) {
                currentContinuation.node = t;
                currentContinuation.op = REOP_NEXTALT;
                t = (RENode *)(t->data.child2);
                ASSERT(t);
                op = t->kind;
                continue;
            }
            else {
                result = x;
                currentContinuation = t->continuation;
                break;
            }                
            
/* the child will evntually terminate, so provide a capturing state 
 * as the continuation 
 */
        case REOP_PAREN:
            t->continuation = currentContinuation;
            currentContinuation.op = REOP_CLOSEPAREN;
            currentContinuation.node = t;
            x->parens[t->parenIndex].index = (REint32)(x->endIndex);
            x->parens[t->parenIndex].length = 0;
            t = (RENode *)(t->child);
            ASSERT(t);
            op = t->kind;
            continue;
        case REOP_CLOSEPAREN:
            x->parens[t->parenIndex].length = x->endIndex 
                                            - x->parens[t->parenIndex].index;
            currentContinuation = t->continuation;
            break;

        case REOP_QUANT:
            t->continuation = currentContinuation;
            t->count = 0;
            t->index = x->endIndex;

            if (t->data.quantifier.greedy) {
                /*
                 * Save the current zero-count state, then jump to the child.
                 */
                backTrackData = pushBackTrack(globalData, REOP_REPEAT, t, x);
                if (!backTrackData) return NULL;
                nodeStateStack[nodeStateStackTop].node = t;
                nodeStateStack[nodeStateStackTop].count = t->count;
                nodeStateStack[nodeStateStackTop].index = x->endIndex;
                ++nodeStateStackTop;
                currentContinuation.node = t;
                currentContinuation.op = REOP_REPEAT;                
                t = (RENode *)(t->child);
                op = t->kind;
                continue;
            }
            else {
                /*
                 * Non-greedy, only run the child if the minimum 
                 * requirement hasn't been met
                 */
                if (t->count < t->data.quantifier.min) {
                    nodeStateStack[nodeStateStackTop].node = t;
                    nodeStateStack[nodeStateStackTop].count = t->count;
                    nodeStateStack[nodeStateStackTop].index = x->endIndex;
                    ++nodeStateStackTop;
                    currentContinuation.node = t;
                    currentContinuation.op = REOP_MINIMALREPEAT;
                    t = (RENode *)(t->child);
                    op = t->kind;
                    continue;
                }
                else {
                    backTrackData = pushBackTrack(globalData, 
                                                    REOP_MINIMALREPEAT, t, x);
                    if (!backTrackData) return NULL;
                    result = x;
                    break;
                }
            }

        case REOP_REPEAT:
            if (result == NULL) {
                /*
                 *  There's been a failure, see if we have enough children
                 */
                currentContinuation = t->continuation;
                if (t->count >= t->data.quantifier.min)
                    result = x;
                break;
            }
            else {
                /*
                 * Pop us off the stack
                 */
                --nodeStateStackTop;
                ASSERT(nodeStateStack[nodeStateStackTop].node == t);

                if ((t->count >= t->data.quantifier.min)
                        && (x->endIndex == t->index)) {
                    /* matched an empty string, that'll get us nowhere */
                    result = NULL;
                    currentContinuation = t->continuation;
                    break;
                }
                ++t->count;
                backTrackData = pushBackTrack(globalData, REOP_REPEAT, t, x);
                if (!backTrackData) return NULL;
                if (t->count == t->data.quantifier.max) {
                    currentContinuation = t->continuation;
                    result = NULL;
                    break;
                }
                else {
                    nodeStateStack[nodeStateStackTop].node = t;
                    nodeStateStack[nodeStateStackTop].count = t->count;
                    nodeStateStack[nodeStateStackTop].index = x->endIndex;
                    ++nodeStateStackTop;
                    for (k = 0; k <= t->data.quantifier.parenCount; k++)
                        x->parens[t->parenIndex + k].index = -1;
                    t->index = x->endIndex;
                    currentContinuation.node = t;
                    currentContinuation.op = REOP_REPEAT;
                    t = (RENode *)(t->child);
                    op = t->kind;
                }
            }
            continue;                       
        case REOP_MINIMALREPEAT:
            if (result == NULL) {
                /* 
                 * Non-greedy failure - try to consume another child
                 */
                if ((t->data.quantifier.max == -1)
                        || (t->count < t->data.quantifier.max)) {
                    for (k = 0; k <= t->data.quantifier.parenCount; k++)
                        x->parens[t->parenIndex + k].index = -1;
                    nodeStateStack[nodeStateStackTop].node = t;
                    nodeStateStack[nodeStateStackTop].count = t->count;
                    nodeStateStack[nodeStateStackTop].index = x->endIndex;
                    ++nodeStateStackTop;
                    currentContinuation.node = t;
                    currentContinuation.op = REOP_MINIMALREPEAT;
                    t = (RENode *)(t->child);
                    op = t->kind;
                    continue;
                }
                else
                    break;
            }
            else {
                --nodeStateStackTop;
                ASSERT(nodeStateStack[nodeStateStackTop].node == t);

                if ((t->count >= t->data.quantifier.min)
                        && (x->endIndex == t->index)) {
                    /* matched an empty string, that'll get us nowhere */
                    result = NULL;
                    currentContinuation = t->continuation;
                    break;
                }
                ++t->count;
                if (t->count < t->data.quantifier.min) {
                    for (k = 0; k <= t->data.quantifier.parenCount; k++)
                        x->parens[t->parenIndex + k].index = -1;
                    nodeStateStack[nodeStateStackTop].node = t;
                    nodeStateStack[nodeStateStackTop].count = t->count;
                    nodeStateStack[nodeStateStackTop].index = x->endIndex;
                    ++nodeStateStackTop;
                    currentContinuation.node = t;
                    currentContinuation.op = REOP_MINIMALREPEAT;
                    t->index = x->endIndex;
                    t = (RENode *)(t->child);
                    op = t->kind;
                    continue;                       
                }
                else {
                    backTrackData = pushBackTrack(globalData, 
                                                    REOP_MINIMALREPEAT, t, x);
                    if (!backTrackData) return NULL;
                    currentContinuation = t->continuation;
                    break;
                }
            }


        case REOP_BACKREF:
            result = backrefMatcher(globalData, x, t);
            break;

/* supersede the continuation with an assertion tester */
        case REOP_ASSERT:               
            t->continuation = currentContinuation;
            currentContinuation.node = t;
            currentContinuation.op = REOP_ASSERTTEST;
            t->index = x->endIndex;
            t->count = backTrackStackTop;
            
            t = (RENode *)(t->child);
            ASSERT(t);
            op = t->kind;
            continue;
/* also provide the assertion tester as the backtrack state */
        case REOP_ASSERTNOT:
            t->continuation = currentContinuation;
            currentContinuation.node = t;
            currentContinuation.op = REOP_ASSERTTEST;
            t->index = x->endIndex;
            t->count = backTrackStackTop;

            backTrackData = pushBackTrack(globalData, REOP_ASSERTTEST, t, x);
            if (!backTrackData) return NULL;

            t = (RENode *)(t->child);
            ASSERT(t);
            op = t->kind;
            continue;
        case REOP_ASSERTTEST:
            backTrackStackTop = t->count;
            x->endIndex = t->index;
            if (t->kind == REOP_ASSERT) {
                if (result != NULL) {
                    result = x;
                }
            }
            else {
                if (result == NULL)
                    result = x;
                else {
                    result = NULL;
                }
            }
            currentContinuation = t->continuation;
            break;

        case REOP_CLASS:
            result = classMatcher(globalData, x, t);
            if (globalData->error != NO_ERROR) return NULL;
            break;
        case REOP_END:
            if (x != NULL)
                return x;
            break;
        }
        /*
         *  If the match failed and there's a backtrack option, take it.
         *  Otherwise this is a match failure.
         */
        if (result == NULL) {
            if (backTrackStackTop > 0) {
                backTrackStackTop--;
                backTrackData = &backTrackStack[backTrackStackTop];

                recoverState(x, backTrackData->state);
                free(backTrackData->state);

                for (k = 0; k < backTrackData->precedingNodeStateTop; k++) {
                    RENode *n = backTrackData->precedingNodeState[k].node;
                    n->count = backTrackData->precedingNodeState[k].count;
                    n->index = backTrackData->precedingNodeState[k].index;
                    nodeStateStack[k] = backTrackData->precedingNodeState[k];
                }
                nodeStateStackTop = backTrackData->precedingNodeStateTop;
                if (backTrackData->precedingNodeState)
                    free(backTrackData->precedingNodeState);
                
                t = backTrackData->continuation.node;

                t->count = backTrackData->nodeState.count;
                t->index = backTrackData->nodeState.index;

                op = backTrackData->continuation.op;
                continue;
            }
            else
                return NULL;
        }
        else
            x = result;
        
        /*
         *  Continue with the expression. If there is no next link, use
         *  the current continuation.
         */
        t = t->next;
        if (t)
            op = t->kind;
        else {
            t = currentContinuation.node;
            ASSERT(t);
            op = currentContinuation.op;
            currentContinuation.op = t->continuation.op;
            currentContinuation.node = t->continuation.node;
        }
    }
    return NULL;
}

/* 
* Parse the regexp - errors are reported via the registered error function 
* and NULL is returned. Otherwise the regexp is compiled and the completed 
* ParseState returned.
*/
REParseState *REParse(const REchar *source, REuint32 sourceLength, 
                      const REchar *flags, REuint32 flagsLength, 
                      REbool oldSyntax)
{
    REuint32 i;
    RENode *t;
    REParseState *pState = (REParseState *)malloc(sizeof(REParseState));
    pState->src = source;
    pState->srcEnd = source + sourceLength;
    pState->parenCount = 0;
    pState->lastIndex = 0;
    pState->flags = 0;
    pState->oldSyntax = oldSyntax;

    for (i = 0; i < flagsLength; i++) {
        switch (flags[i]) {
        case 'g':
            pState->flags |= GLOBAL; break;
        case 'i':
            pState->flags |= IGNORECASE; break;
        case 'm':
            pState->flags |= MULTILINE; break;
        default:
            reportRegExpError(&pState->error, BAD_FLAG); 
            free(pState);
            return NULL;
        }
    }
    if (parseDisjunction(pState)) {
        t = pState->result;
        if (t) {
            while (t->next) t = t->next;    
            t->next = newRENode(pState, REOP_END);
            if (!t->next) {
                free(pState);
                return NULL;
            }
        }
        else
            pState->result = newRENode(pState, REOP_END);
        return pState;
    }
    else {
        free(pState);
        return NULL;
    }
}

static REbool initStacks(REGlobalData *gData)
{
    if (!backTrackStack) {
        maxBackTrack = INITIAL_BACKTRACK;
        backTrackStack = (REBackTrackData *)malloc(sizeof(REBackTrackData) 
                                                            * maxBackTrack);
        if (!backTrackStack) {
            reportRegExpError(&gData->error, OUT_OF_MEMORY);
            return false;
        }
    }
    if (!nodeStateStack) {
        maxNodeStateStack = INITIAL_STATESTACK;
        nodeStateStack = (RENodeState *)malloc(sizeof(RENodeState) 
                                                         * maxNodeStateStack);
        if (!nodeStateStack) {
            reportRegExpError(&gData->error, OUT_OF_MEMORY);
            return false;
        }
    }
    backTrackStackTop = 0;
    nodeStateStackTop = 0;
    return true;
}

/*
 *  The [[Match]] implementation
 *
 */
REState *REMatch(REParseState *parseState, const REchar *text, 
                   REint32 length)
{
    REint32 j;
    REGlobalData gData;
    REState *result;
    REState *x = (REState *)malloc(sizeof(REState) 
                        + (parseState->parenCount * sizeof(RECapture)));

    if (!x) {
        reportRegExpError(&gData.error, OUT_OF_MEMORY);
        return NULL;
    }

    gData.flags = parseState->flags;
    gData.input = text;
    gData.length = length;
    gData.error = NO_ERROR;
    
    x->n = parseState->parenCount;
    for (j = 0; j < x->n; j++)
        x->parens[j].index = -1;
    x->endIndex = 0;

    if (!initStacks(&gData)) {
        free(x);
        return NULL;
    }

    result = executeRENode(parseState->result, &gData, x);
    for (j = 0; j < backTrackStackTop; j++)
        free(backTrackStack[j].state);
    if (gData.error != NO_ERROR) return NULL;
    return result;
}

/*
 *  Execute the RegExp against the supplied text, filling in the REState.
 *
 */
REState *REExecute(REParseState *parseState, const REchar *text, 
                   REint32 length)
{
    REState *result;

    REGlobalData gData;
    REint32 i;
    REint32 j;
    
    REState *x = (REState *)malloc(sizeof(REState) 
                        + (parseState->parenCount * sizeof(RECapture)));
    if (!x) {
        reportRegExpError(&gData.error, OUT_OF_MEMORY);
        return NULL;
    }

    x->n = parseState->parenCount;
    for (j = 0; j < x->n; j++)
        x->parens[j].index = -1;

    if (parseState->flags & GLOBAL)
        i = parseState->lastIndex;
    else
        i = 0;

    if ((i < 0) || (i > (REint32)length)) {
        parseState->lastIndex = 0;
        free(x);
        return NULL;
    }
        
    gData.flags = parseState->flags;
    gData.input = text;
    gData.length = length;
    gData.error = NO_ERROR;

    if (!initStacks(&gData)) {
        free(x);
        return NULL;
    }

    /*
        Simple anchoring optimization -
    */
    if (parseState->result->kind == REOP_FLAT) {
        REchar matchCh = parseState->result->data.flat.ch;
        if (parseState->result->child)
            matchCh = *((REchar *)parseState->result->child);
        for (j = i; j < length; j++) {
            if (text[j] == matchCh) {
                i = (REint32)j;
                break;
            }
        }
        if (j == length) {
            parseState->lastIndex = 0;
            free(x);
            return NULL;
        }
    }

    while (true) {
        x->endIndex = (REuint32)i;
        backTrackStackTop = 0;
        nodeStateStackTop = 0;
        result = executeRENode(parseState->result, &gData, x);
        for (j = 0; j < backTrackStackTop; j++)
            free(backTrackStack[j].state);
        if (gData.error != NO_ERROR) return NULL;
        if (result == NULL) {
            i++;
            if (i > (REint32)length) {
                parseState->lastIndex = 0;
                result = NULL;
                free(x);
                break;
            }
        }
        else {
            if (parseState->flags & GLOBAL)
                parseState->lastIndex = (REint32)(result->endIndex);
            result->length = result->endIndex - i;
            result->endIndex = (REuint32)(i);
            break;
        }
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

int main(int argc, char* argv[])
{
    char regexpInput[128];
    char *regexpSrc;
    char str[128];
    REState *result;
    int regexpLength;
    char *flagSrc;
    int flagSrcLength;
    REuint32 i, j;

    printf("Delimit regexp by /  / (with flags following) and strings by \"  \"\n");
    while (true) {
        REchar *regexpWideSrc;
        REchar *flagWideSrc;
        REParseState *pState;

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
                                flagWideSrc, flagSrcLength, true);
        if (pState) {
            while (true) {
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
                        for (i = 0; i < result->length; i++) 
                            printf("%c", str[i + 1 + result->endIndex]);
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
