/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef _WIN32
#include "msvc_pragma.h"
#endif


#include <algorithm>
#include <assert.h>

#include "world.h"
#include "strings.h"
#include "utilities.h"
#include "js2value.h"

#include <map>
#include <algorithm>
#include <list>
#include <stack>

#include "reader.h"
#include "parser.h"
#include "js2engine.h"
#include "regexp.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

typedef struct RECharSet {
    bool converted;
    bool sense;
    uint16 length;
    union {
        uint8 *bits;
        struct {
            uint16 startIndex;
            uint16 length;
        } src;
    } u;
} RECharSet;

namespace JavaScript {      
namespace MetaData {

void JS_ReportOutOfMemory(JS2Metadata *meta)
{
    meta->reportError(Exception::internalError, "out of memory", 0);
}

void js_ReportCompileErrorNumber(JS2Metadata *meta, const char *message, const jschar *extra)
{
    meta->reportError(Exception::syntaxError, message, 0, extra);
}

void JS_ReportErrorNumber(JS2Metadata *meta, const char *message)
{
    meta->reportError(Exception::syntaxError, message, 0);
}


#define JS_FALSE false
#define JS_TRUE true

#define JSMSG_MISSING_PAREN         "Missing parentheses, {0}"
#define JSMSG_TOO_MANY_PARENS       "Too many parentheses, {0}"
#define JSMSG_BAD_QUANTIFIER        "Bad quantifer specification, {0}"
#define JSMSG_MIN_TOO_BIG           "Range minimun too big, {0}"
#define JSMSG_MAX_TOO_BIG           "Range maximum too big, {0}"
#define JSMSG_OUT_OF_ORDER          "Range out of order, {0}"
#define JSMSG_BAD_CLASS_RANGE       "Bad range in class"
#define JSMSG_TRAILING_SLASH        "Trailing slash, {0}"
#define JSMSG_UNTERM_CLASS          "Unterminated class, {0}"
#define JSMSG_UNTERM_QUANTIFIER     "Unterminated quantifier"

#define LINE_SEPARATOR      0x2028
#define PARAGRAPH_SEPARATOR 0x2029



#define JS_ASSERT(x) ASSERT(x)
#define JSSTRING_CHARS(x) (x)


#define JS7_ISDEC(c)    ((c) < 128 && isdigit(c))
#define JS7_UNDEC(c)    ((c) - '0')
#define JS7_ISLET(c)    ((c) < 128 && isalpha(c))

#define JUMP_OFFSET_LEN         2
#define JUMP_OFFSET_HI(off)     ((jsbytecode)((off) >> 8))
#define JUMP_OFFSET_LO(off)     ((jsbytecode)(off))
#define GET_JUMP_OFFSET(pc)     ((int16)(((pc)[1] << 8) | (pc)[2]))
#define SET_JUMP_OFFSET(pc,off) ((pc)[1] = JUMP_OFFSET_HI(off),               \
				 (pc)[2] = JUMP_OFFSET_LO(off))
#define JUMP_OFFSET_MIN         ((int16)0x8000)
#define JUMP_OFFSET_MAX         ((int16)0x7fff)


typedef bool JSBool;
typedef uint32 uintN;
typedef int32 intN;
typedef uint32 jsint;
typedef char16 JSString;
typedef char16 JSSubString;



typedef struct REMatchState {
    const jschar *cp;
    RECapture parens[1];      /* first of 're->parenCount' captures, 
                               * allocated at end of this struct.
                               */
} REMatchState;

/* Note : contiguity of 'simple opcodes' is important for simpleMatch() */
typedef enum REOp {
    REOP_EMPTY         = 0,  /* match rest of input against rest of r.e. */
    REOP_ALT           = 1,  /* alternative subexpressions in kid and next */
    REOP_SIMPLE_START  = 2,  /* start of 'simple opcodes' */
    REOP_BOL           = 2,  /* beginning of input (or line if multiline) */
    REOP_EOL           = 3,  /* end of input (or line if multiline) */
    REOP_WBDRY         = 4,  /* match "" at word boundary */
    REOP_WNONBDRY      = 5,  /* match "" at word non-boundary */
    REOP_DOT           = 6,  /* stands for any character */
    REOP_DIGIT         = 7,  /* match a digit char: [0-9] */
    REOP_NONDIGIT      = 8,  /* match a non-digit char: [^0-9] */
    REOP_ALNUM         = 9,  /* match an alphanumeric char: [0-9a-z_A-Z] */
    REOP_NONALNUM      = 10, /* match a non-alphanumeric char: [^0-9a-z_A-Z] */
    REOP_SPACE         = 11, /* match a whitespace char */
    REOP_NONSPACE      = 12, /* match a non-whitespace char */
    REOP_BACKREF       = 13, /* back-reference (e.g., \1) to a parenthetical */
    REOP_FLAT          = 14, /* match a flat string */
    REOP_FLAT1         = 15, /* match a single char */
    REOP_FLATi         = 16, /* case-independent REOP_FLAT */
    REOP_FLAT1i        = 17, /* case-independent REOP_FLAT1 */
    REOP_UCFLAT1       = 18, /* single Unicode char */
    REOP_UCFLAT1i      = 19, /* case-independent REOP_UCFLAT1 */
    REOP_UCFLAT        = 20, /* flat Unicode string; len immediate counts chars */
    REOP_UCFLATi       = 21, /* case-independent REOP_UCFLAT */
    REOP_CLASS         = 22, /* character class with index */
    REOP_NCLASS        = 23, /* negated character class with index */
    REOP_SIMPLE_END    = 23, /* end of 'simple opcodes' */
    REOP_QUANT         = 25, /* quantified atom: atom{1,2} */
    REOP_STAR          = 26, /* zero or more occurrences of kid */
    REOP_PLUS          = 27, /* one or more occurrences of kid */
    REOP_OPT           = 28, /* optional subexpression in kid */
    REOP_LPAREN        = 29, /* left paren bytecode: kid is u.num'th sub-regexp */
    REOP_RPAREN        = 30, /* right paren bytecode */
    REOP_JUMP          = 31, /* for deoptimized closure loops */
    REOP_DOTSTAR       = 32, /* optimize .* to use a single opcode */
    REOP_ANCHOR        = 33, /* like .* but skips left context to unanchored r.e. */
    REOP_EOLONLY       = 34, /* $ not preceded by any pattern */
    REOP_BACKREFi      = 37, /* case-independent REOP_BACKREF */
    REOP_LPARENNON     = 41, /* non-capturing version of REOP_LPAREN */
    REOP_ASSERT        = 43, /* zero width positive lookahead assertion */
    REOP_ASSERT_NOT    = 44, /* zero width negative lookahead assertion */
    REOP_ASSERTTEST    = 45, /* sentinel at end of assertion child */
    REOP_ASSERTNOTTEST = 46, /* sentinel at end of !assertion child */
    REOP_MINIMALSTAR   = 47, /* non-greedy version of * */
    REOP_MINIMALPLUS   = 48, /* non-greedy version of + */
    REOP_MINIMALOPT    = 49, /* non-greedy version of ? */
    REOP_MINIMALQUANT  = 50, /* non-greedy version of {} */
    REOP_ENDCHILD      = 51, /* sentinel at end of quantifier child */
    REOP_REPEAT        = 52, /* directs execution of greedy quantifier */
    REOP_MINIMALREPEAT = 53, /* directs execution of non-greedy quantifier */
    REOP_ALTPREREQ     = 54, /* prerequisite for ALT, either of two chars */
    REOP_ALTPREREQ2    = 55, /* prerequisite for ALT, a char or a class */
    REOP_ENDALT        = 56, /* end of final alternate */
    REOP_CONCAT        = 57, /* concatenation of terms (parse time only) */

    REOP_END
} REOp;

#define REOP_IS_SIMPLE(op)  (((op) >= REOP_SIMPLE_START) && ((op) <= REOP_SIMPLE_END))

struct RENode {
    REOp            op;         /* r.e. op bytecode */
    RENode          *next;      /* next in concatenation order */
    void            *kid;       /* first operand */
    union {
        void        *kid2;      /* second operand */
        jsint       num;        /* could be a number */
        jsint       parenIndex; /* or a parenthesis index */
        struct {                /* or a quantifier range */
            uint16  min;
            uint16  max;
            JSBool  greedy;
        } range;
        struct {                /* or a character class */
            uint16  startIndex;
            uint16  kidlen;     /* length of string at kid, in jschars */
            uint16  bmsize;     /* bitmap size, based on max char code */
            uint16  index;      /* index into class list */
            JSBool  sense;
        } ucclass;
        struct {                /* or a literal sequence */
            jschar  chr;        /* of one character */
            uint16  length;     /* or many (via the kid) */      
        } flat;
        struct {
            RENode  *kid2;      /* second operand from ALT */
            jschar  ch1;        /* match char for ALTPREREQ */
            jschar  ch2;        /* ditto, or class index for ALTPREREQ2 */
        } altprereq;
    } u;
};


#define RE_IS_LETTER(c)      ( ((c >= 'A') && (c <= 'Z')) ||               \
                                ((c >= 'a') && (c <= 'z')) )
#define RE_IS_LINE_TERM(c)  ( (c == '\n') || (c == '\r') ||               \
                                (c == LINE_SEPARATOR) || (c == PARAGRAPH_SEPARATOR))

#define CLASS_CACHE_SIZE (4)
typedef struct CompilerState {
    JS2Metadata     *meta;
    Pool<RENode>    *reNodePool;
    bool            strict;
    const jschar    *cpbegin;
    const jschar    *cpend;
    const jschar    *cp;
    uintN           flags;
    uint16          parenCount;
    uint16          classCount;   /* number of [] encountered */
    size_t          progLength;   /* estimated bytecode length */
    uintN           treeDepth;    /* maximum depth of parse tree */
    RENode          *result;    
    struct {
        const jschar *start;        /* small cache of class strings */
        uint16 length;              /* since they're often the same */
        uint16 index;
    } classCache[CLASS_CACHE_SIZE];
} CompilerState;

#define NO_MAX ((uint16)(-1))

typedef struct REProgState {
    jsbytecode *continue_pc;        /* current continuation data */
    REOp continue_op;                  
    uint16 index;                   /* progress in text */
    uintN parenSoFar;               /* highest indexed paren started */
    union {
        struct {
            uint16 min;             /* current quantifier limits */
            uint16 max;
        } quantifier;
        struct {
            size_t top;             /* backtrack stack state */
            size_t sz;
        } assertion;
    } u;
} REProgState;

typedef struct REBackTrackData {
    size_t sz;                      /* size of previous stack entry */
    jsbytecode *backtrack_pc;       /* where to backtrack to */
    REOp backtrack_op;
    const jschar *cp;               /* index in text of match at backtrack */
    intN parenIndex;                /* start index of saved paren contents */
    uint16 parenCount;              /* # of saved paren contents */
    uint16 precedingStateTop;       /* number of parent states */
    /* saved parent states follow */
    /* saved paren contents follow */
} REBackTrackData;

#define INITIAL_STATESTACK (100)
#define INITIAL_BACKTRACK (8000)

typedef struct REGlobalData {
    JSBool globalMultiline;
    JS2RegExp *regexp;              /* the RE in execution */   
    JSBool ok;                      /* runtime error (out_of_memory only?) */
    size_t start;                   /* offset to start at */
    ptrdiff_t skipped;              /* chars skipped anchoring this r.e. */
    const jschar *cpbegin, *cpend;  /* text base address and limit */

    REProgState *stateStack;        /* stack of state of current parents */
    uint16 stateStackTop;
    uint16 maxStateStack;

    REBackTrackData *backTrackStack;/* stack of matched-so-far positions */
    REBackTrackData *backTrackSP;
    size_t maxBackTrack;
    size_t cursz;                   /* size of current stack entry */

} REGlobalData;

bool JS_ISWORD(jschar ch)
{
    CharInfo chi(ch);
    return ch == '_' || isAlphanumeric(chi);
}

bool JS_ISSPACE(jschar ch)
{
    CharInfo chi(ch);
    return isSpace(chi);
}

bool JS_ISDIGIT(jschar ch)
{
    CharInfo chi(ch);
    return isDecimalDigit(chi);
}

/*
 * 1. If IgnoreCase is false, return ch.
 * 2. Let u be ch converted to upper case as if by calling 
 *    String.prototype.toUpperCase on the one-character string ch.
 * 3. If u does not consist of a single character, return ch.
 * 4. Let cu be u's character.
 * 5. If ch's code point value is greater than or equal to decimal 128 and cu's
 *    code point value is less than decimal 128, then return ch.
 * 6. Return cu.
 */
static jschar
canonicalize(jschar ch)
{
    jschar cu = toUpper(ch);
    if ((ch >= 128) && (cu < 128)) return ch;
    return cu;
}

/* Construct and initialize an RENode, returning NULL for out-of-memory */
static RENode *
NewRENode(CompilerState *state, REOp op)
{
    RENode *ren;
    ren = new (*state->reNodePool) RENode();

    if (!ren) {
        JS_ReportOutOfMemory(state->meta);
        return NULL;
    }
    ren->op = op;
    ren->next = NULL;
    ren->kid = NULL;
    return ren;
}

/*
 * Validates and converts hex ascii value.
 */
static JSBool
isASCIIHexDigit(jschar c, uintN *digit)
{
    uintN cv = c;

    if (cv < '0')
        return JS_FALSE;
    if (cv <= '9') {
        *digit = cv - '0';
        return JS_TRUE;
    }
    cv |= 0x20;
    if (cv >= 'a' && cv <= 'f') {
        *digit = cv - 'a' + 10;
        return JS_TRUE;
    }
    return JS_FALSE;
}


typedef struct {
    REOp op;
    const jschar *errPos;
    uint16 parenIndex;
} REOpData;


/* 
 * Process the op against the two top operands, reducing them to a single
 * operand in the penultimate slot. Update progLength and treeDepth.
 */
static JSBool 
processOp(CompilerState *state, REOpData *opData, RENode **operandStack, intN operandSP)
{
    RENode *result;

    switch (opData->op) {
    case REOP_ALT:
        result = NewRENode(state, REOP_ALT);
        if (!result) 
            return JS_FALSE;
        result->kid = operandStack[operandSP - 2];
        result->u.kid2 = operandStack[operandSP - 1];
        operandStack[operandSP - 2] = result;
        /* 
         * look at both alternates to see if there's a FLAT or a CLASS at
         * the start of each. If so, use a prerequisite match
         */
        ++state->treeDepth;
        if ((((RENode *)(result->kid))->op == REOP_FLAT)
                && (((RENode *)(result->u.kid2))->op == REOP_FLAT)
                && ((state->flags & JSREG_FOLD) == 0) ) {
            result->op = REOP_ALTPREREQ;
            result->u.altprereq.ch1 
                = ((RENode *)(result->kid))->u.flat.chr;
            result->u.altprereq.ch2 
                = ((RENode *)(result->u.kid2))->u.flat.chr;
            /* ALTPREREQ, <end>, uch1, uch2, <next>, ..., 
                                            JUMP, <end> ... ENDALT */
            state->progLength += 13;
        }
        else
            if ((((RENode *)(result->kid))->op == REOP_CLASS)
                    && (((RENode *)(result->kid))->u.ucclass.index < 256)
                    && (((RENode *)(result->u.kid2))->op == REOP_FLAT)
                    && ((state->flags & JSREG_FOLD) == 0) ) {
                result->op = REOP_ALTPREREQ2;
                result->u.altprereq.ch1 
                    = ((RENode *)(result->u.kid2))->u.flat.chr;
                result->u.altprereq.ch2 
                    = ((RENode *)(result->kid))->u.ucclass.index;
                /* ALTPREREQ2, <end>, uch1, uch2, <next>, ..., 
                                                JUMP, <end> ... ENDALT */
                state->progLength += 13;
            }
            else
                if ((((RENode *)(result->kid))->op == REOP_FLAT)
                        && (((RENode *)(result->u.kid2))->op == REOP_CLASS)
                        && (((RENode *)(result->u.kid2))->u.ucclass.index < 256)
                        && ((state->flags & JSREG_FOLD) == 0) ) {
                    result->op = REOP_ALTPREREQ2;
                    result->u.altprereq.ch1 
                        = ((RENode *)(result->kid))->u.flat.chr;
                    result->u.altprereq.ch2 
                        = ((RENode *)(result->u.kid2))->u.ucclass.index;
                    /* ALTPREREQ2, <end>, uch1, uch2, <next>, ..., 
                                                  JUMP, <end> ... ENDALT */
                    state->progLength += 13;
                }
                else
                    /* ALT, <next>, ..., JUMP, <end> ... ENDALT */
                    state->progLength += 7;
        break;
    case REOP_CONCAT:
        result = operandStack[operandSP - 2];
        while (result->next)
            result = result->next;
        result->next = operandStack[operandSP - 1];
        break;
    case REOP_ASSERT:
    case REOP_ASSERT_NOT:
    case REOP_LPARENNON:
    case REOP_LPAREN:
        /* These should have been processed by a close paren. */
        js_ReportCompileErrorNumber(state->meta, JSMSG_MISSING_PAREN, opData->errPos);
        return JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * Parser forward declarations.
 */
static JSBool parseTerm(CompilerState *state);
static JSBool parseQuantifier(CompilerState *state);

/*
 * Top-down regular expression grammar, based closely on Perl4.
 *
 *  regexp:     altern                  A regular expression is one or more
 *              altern '|' regexp       alternatives separated by vertical bar.
 */

#define INITIAL_STACK_SIZE (128)
static JSBool 
parseRegExp(CompilerState *state)
{
    const jschar *errPos;
    uint16 parenIndex;
    RENode *operand;
    REOpData *operatorStack;
    RENode **operandStack;
    REOp op;
    intN i;
    JSBool result = JS_FALSE;

    intN operatorSP = 0, operatorStackSize = INITIAL_STACK_SIZE;
    intN operandSP = 0, operandStackSize = INITIAL_STACK_SIZE;

    /* Watch out for empty regexp */
    if (state->cp == state->cpend) {
        state->result = NewRENode(state, REOP_EMPTY);
        return JS_TRUE;
    }

    operatorStack = (REOpData *)malloc(sizeof(REOpData) * operatorStackSize);
    if (!operatorStack)
        return JS_FALSE;

    operandStack = (RENode **)malloc(sizeof(RENode *) * operandStackSize);
    if (!operandStack)
        goto out;


    while (JS_TRUE) {
        if (state->cp != state->cpend) {
            switch (*state->cp) {
                /* balance '(' */
            case '(':           /* balance ')' */
                errPos = state->cp;
                ++state->cp;
                if ((state->cp < state->cpend) && (*state->cp == '?')
                        && ( (state->cp[1] == '=')
                                || (state->cp[1] == '!')
                                || (state->cp[1] == ':') )) {
                    ++state->cp;
                    if (state->cp == state->cpend) {
                        js_ReportCompileErrorNumber(state->meta, JSMSG_MISSING_PAREN,
                                                    errPos);
                        goto out;
                    }
                    switch (*state->cp++) {
                    case '=':
                        op = REOP_ASSERT;
                        /* ASSERT, <next>, ... ASSERTTEST */
                        state->progLength += 4;    
                        break;
                    case '!':
                        op = REOP_ASSERT_NOT;
                        /* ASSERTNOT, <next>, ... ASSERTNOTTEST */
                        state->progLength += 4;
                        break;
                    case ':':
                        op = REOP_LPARENNON;
                        break;
                    }
                    parenIndex = state->parenCount;
                }
                else {
                    op = REOP_LPAREN;
                    /* LPAREN, <index>, ... RPAREN, <index> */
                    state->progLength += 6;
                    parenIndex = state->parenCount++;
                    if (state->parenCount == 0) {
                        js_ReportCompileErrorNumber(state->meta, JSMSG_TOO_MANY_PARENS,
                                                    errPos);
                        goto out;
                    }
                }
                goto pushOperator;
            case '|':
            case ')':
                /* Expected an operand before these, so make an empty one */
                operand = NewRENode(state, REOP_EMPTY);
                if (!operand)
                    goto out;
                goto pushOperand;
            default:
                if (!parseTerm(state))
                    goto out;
                operand = state->result;
pushOperand:
                if (operandSP == operandStackSize) {
                    operandStackSize += operandStackSize;
                    operandStack = 
                      (RENode **)realloc(operandStack,
                                            sizeof(RENode *) * operandStackSize);
                    if (!operandStack)
                        goto out;
                }
                operandStack[operandSP++] = operand;
                break;
            }
        }
            /* At the end; process remaining operators */
restartOperator:
        if (state->cp == state->cpend) {
            while (operatorSP) {
                --operatorSP;
                if (!processOp(state, &operatorStack[operatorSP], 
                               operandStack, operandSP))
                    goto out;
                --operandSP;
            }
            JS_ASSERT(operandSP == 1);
            state->result = operandStack[0];
            result = JS_TRUE;
            goto out;
        }
        switch (*state->cp) {
        case '|': 
            /* Process any stacked 'concat' operators */
            ++state->cp;
            while (operatorSP 
                    && (operatorStack[operatorSP - 1].op == REOP_CONCAT)) {
                --operatorSP;
                if (!processOp(state, &operatorStack[operatorSP], 
                               operandStack, operandSP))
                    goto out;
                --operandSP;
            }
            op = REOP_ALT;
            goto pushOperator;

        case ')':
            /* If there's not a stacked open parentheses,we
             * accept the close as a flat.
             */
            for (i = operatorSP - 1; i >= 0; i--)
                if ((operatorStack[i].op == REOP_ASSERT)
                        || (operatorStack[i].op == REOP_ASSERT_NOT)
                        || (operatorStack[i].op == REOP_LPARENNON)
                        || (operatorStack[i].op == REOP_LPAREN))
                    break;
            if (i == -1) {
                if (!parseTerm(state))
                    goto out;
                operand = state->result;
                goto pushOperand;
            }
            ++state->cp;
            /* process everything on the stack until the open */
            while (JS_TRUE) {
                JS_ASSERT(operatorSP);                    
                --operatorSP;
                switch (operatorStack[operatorSP].op) {
                case REOP_ASSERT:
                case REOP_ASSERT_NOT:
                case REOP_LPAREN:
                    operand = NewRENode(state, operatorStack[operatorSP].op);
                    if (!operand)
                        goto out;
                    operand->u.parenIndex 
                                    = operatorStack[operatorSP].parenIndex;
                    JS_ASSERT(operandSP);
                    operand->kid = operandStack[operandSP - 1];
                    operandStack[operandSP - 1] = operand;
                    ++state->treeDepth;
                    /* fall thru... */
                case REOP_LPARENNON:
                    state->result = operandStack[operandSP - 1];
                    if (!parseQuantifier(state))
                        goto out;
                    operandStack[operandSP - 1] = state->result;
                    goto restartOperator;
                default:
                    if (!processOp(state, &operatorStack[operatorSP], 
                                   operandStack, operandSP))
                        goto out;
                    --operandSP;
                    break;
                }
            }
            break;
        default:
            /* Anything else is the start of the next term */
            op = REOP_CONCAT;
pushOperator:
            if (operatorSP == operatorStackSize) {
                operatorStackSize += operatorStackSize;
                operatorStack = 
                    (REOpData *)realloc(operatorStack,
                                           sizeof(REOpData) * operatorStackSize);
                if (!operatorStack)
                    goto out;
            }
            operatorStack[operatorSP].op = op;
            operatorStack[operatorSP].errPos = errPos;
            operatorStack[operatorSP++].parenIndex = parenIndex;
            break;
        }
    }
out:
    if (operatorStack)
        free(operatorStack);
    if (operandStack)
        free(operandStack);
    return result;
}

/*
 * Extract and return a decimal value at state->cp, the
 * initial character 'c' has already been read.
 */
static intN 
getDecimalValue(jschar c, CompilerState *state)
{
    intN value = JS7_UNDEC(c);
    while (state->cp < state->cpend) {
        c = *state->cp; 
        if (!JS7_ISDEC(c))
            break;
        value = (10 * value) + JS7_UNDEC(c);
        ++state->cp;
    }
    return value;
}

/*
 * Calculate the total size of the bitmap required for a class expression.
 */
static JSBool 
calculateBitmapSize(CompilerState *state, RENode *target, const jschar *src,
                    const jschar *end)
{
    jschar rangeStart, c;
    uintN n, digit, nDigits, i;
    uintN max = 0;
    JSBool inRange = JS_FALSE;

    target->u.ucclass.bmsize = 0;
    target->u.ucclass.sense = JS_TRUE;

    if (src == end)
        return JS_TRUE;

    if (*src == '^') {
        ++src;
        target->u.ucclass.sense = JS_FALSE;
    }
    
    while (src != end) {
        uintN localMax = 0;
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
                if (((src + 1) < end) && RE_IS_LETTER(src[1]))
                    localMax = (jschar)(*src++ & 0x1F);
                else
                    localMax = '\\';
                break;
            case 'x':
                nDigits = 2;
                goto lexHex;
            case 'u':
                nDigits = 4;
lexHex:
                n = 0;
                for (i = 0; (i < nDigits) && (src < end); i++) {
                    c = *src++;
                    if (!isASCIIHexDigit(c, &digit)) {
                        /* 
                         * Back off to accepting the original 
                         *'\' as a literal. 
                         */
                        src -= (i + 1);
                        n = '\\';
                        break;
                    }
                    n = (n << 4) | digit;
                }
                localMax = n;
                break;
            case 'd':
                if (inRange) {
                    JS_ReportErrorNumber(state->meta, JSMSG_BAD_CLASS_RANGE);
                    return JS_FALSE;
                }
                localMax = '9';
                break;
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                if (inRange) {
                    JS_ReportErrorNumber(state->meta, JSMSG_BAD_CLASS_RANGE);
                    return JS_FALSE;
                }
                target->u.ucclass.bmsize = 65535;
                return JS_TRUE;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                /*
                 *  This is a non-ECMA extension - decimal escapes (in this
                 *  case, octal!) are supposed to be an error inside class
                 *  ranges, but supported here for backwards compatibility.
                 *
                 */
                n = JS7_UNDEC(c);
                c = *src;
                if ('0' <= c && c <= '7') {
                    src++;
                    n = 8 * n + JS7_UNDEC(c);
                    c = *src;
                    if ('0' <= c && c <= '7') {
                        src++;
                        i = 8 * n + JS7_UNDEC(c);
                        if (i <= 0377)
                            n = i;
                        else
                            src--;
                    }
                }
                localMax = n;
                break;

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
                JS_ReportErrorNumber(state->meta, JSMSG_BAD_CLASS_RANGE);
                return JS_FALSE;
            }
            inRange = JS_FALSE;
        }
        else {
            if (src < (end - 1)) {
                if (*src == '-') {
                    ++src;
                    inRange = JS_TRUE;
                    rangeStart = (jschar)localMax;
                    continue;
                }
            }
        }
        if (state->flags & JSREG_FOLD) {
            c = canonicalize((jschar)localMax);
            if (c > localMax)
                localMax = c;
        }
        if (localMax > max)
            max = localMax;
    }
    target->u.ucclass.bmsize = max;
    return JS_TRUE;
}

/*
 *  item:       assertion               An item is either an assertion or
 *              quantatom               a quantified atom.
 *
 *  assertion:  '^'                     Assertions match beginning of string
 *                                      (or line if the class static property
 *                                      RegExp.multiline is true).
 *              '$'                     End of string (or line if the class
 *                                      static property RegExp.multiline is
 *                                      true).
 *              '\b'                    Word boundary (between \w and \W).
 *              '\B'                    Word non-boundary.
 *
 *  quantatom:  atom                    An unquantified atom.
 *              quantatom '{' n ',' m '}'
 *                                      Atom must occur between n and m times.
 *              quantatom '{' n ',' '}' Atom must occur at least n times.
 *              quantatom '{' n '}'     Atom must occur exactly n times.
 *              quantatom '*'           Zero or more times (same as {0,}).
 *              quantatom '+'           One or more times (same as {1,}).
 *              quantatom '?'           Zero or one time (same as {0,1}).
 *
 *              any of which can be optionally followed by '?' for ungreedy
 *
 *  atom:       '(' regexp ')'          A parenthesized regexp (what matched
 *                                      can be addressed using a backreference,
 *                                      see '\' n below).
 *              '.'                     Matches any char except '\n'.
 *              '[' classlist ']'       A character class.
 *              '[' '^' classlist ']'   A negated character class.
 *              '\f'                    Form Feed.
 *              '\n'                    Newline (Line Feed).
 *              '\r'                    Carriage Return.
 *              '\t'                    Horizontal Tab.
 *              '\v'                    Vertical Tab.
 *              '\d'                    A digit (same as [0-9]).
 *              '\D'                    A non-digit.
 *              '\w'                    A word character, [0-9a-z_A-Z].
 *              '\W'                    A non-word character.
 *              '\s'                    A whitespace character, [ \b\f\n\r\t\v].
 *              '\S'                    A non-whitespace character.
 *              '\' n                   A backreference to the nth (n decimal
 *                                      and positive) parenthesized expression.
 *              '\' octal               An octal escape sequence (octal must be
 *                                      two or three digits long, unless it is
 *                                      0 for the null character).
 *              '\x' hex                A hex escape (hex must be two digits).
 *              '\u' unicode            A unicode escape (must be four digits).
 *              '\c' ctrl               A control character, ctrl is a letter.
 *              '\' literalatomchar     Any character except one of the above
 *                                      that follow '\' in an atom.
 *              otheratomchar           Any character not first among the other
 *                                      atom right-hand sides.
 */
static JSBool 
parseTerm(CompilerState *state)
{
    jschar c = *state->cp++;
    uintN nDigits;
    uintN num, tmp, n, i;
    const jschar *termStart;
    JSBool foundCachedCopy;

    switch (c) {
    /* assertions and atoms */
    case '^':
        state->result = NewRENode(state, REOP_BOL);
        if (!state->result) 
            return JS_FALSE;
        state->progLength++;
        return JS_TRUE;
    case '$':
        state->result = NewRENode(state, REOP_EOL);
        if (!state->result) 
            return JS_FALSE;
        state->progLength++;
        return JS_TRUE;
    case '\\':
        if (state->cp >= state->cpend) {
            /* a trailing '\' is an error */
            js_ReportCompileErrorNumber(state->meta, JSMSG_TRAILING_SLASH, state->cp);
            return JS_FALSE;
        }
        c = *state->cp++;
        switch (c) {
        /* assertion escapes */
        case 'b' :
            state->result = NewRENode(state, REOP_WBDRY);
            if (!state->result) 
                return JS_FALSE;
            state->progLength++;
            return JS_TRUE;
        case 'B':
            state->result = NewRENode(state, REOP_WNONBDRY);
            if (!state->result) 
                return JS_FALSE;
            state->progLength++;
            return JS_TRUE;
        /* Decimal escape */
        case '0':
            if (state->strict)
                c = 0;
            else {
     doOctal:
                num = 0;
                while (state->cp < state->cpend) {
                    if ('0' <= (c = *state->cp) && c <= '7') {
                        state->cp++;
                        tmp = 8 * num + (uintN)JS7_UNDEC(c);
                        if (tmp > 0377)
                            break;
                        num = tmp;
                    }
                    else
                        break;
                }
                c = (jschar)(num);
            }
    doFlat:
            state->result = NewRENode(state, REOP_FLAT);
            if (!state->result) 
                return JS_FALSE;
            state->result->u.flat.chr = c;
            state->result->u.flat.length = 1;                    
            state->progLength += 3;
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
            termStart = state->cp - 1;
            num = (uintN)getDecimalValue(c, state);
            if (num > 9 &&
                num > state->parenCount &&
                !(state->strict)) {
                state->cp = termStart;
                goto doOctal;
            }
            state->result = NewRENode(state, REOP_BACKREF);
            if (!state->result) 
                return JS_FALSE;
            state->result->u.parenIndex = num - 1;
            state->progLength += 3;
            break;
        /* Control escape */
        case 'f':
            c = 0xC;
            goto doFlat;
        case 'n':
            c = 0xA;
            goto doFlat;
        case 'r':
            c = 0xD;
            goto doFlat;
        case 't':
            c = 0x9;
            goto doFlat;
        case 'v':
            c = 0xB;
            goto doFlat;
        /* Control letter */
        case 'c':
            if (((state->cp + 1) < state->cpend) &&
                                RE_IS_LETTER(state->cp[1]))
                c = (jschar)(*state->cp++ & 0x1F);
            else {
                /* back off to accepting the original '\' as a literal */
                --state->cp;
                c = '\\';
            }
            goto doFlat;
        /* HexEscapeSequence */
        case 'x':
            nDigits = 2;
            goto lexHex;
        /* UnicodeEscapeSequence */
        case 'u':
            nDigits = 4;
lexHex:
            n = 0;
            for (i = 0; (i < nDigits) 
                    && (state->cp < state->cpend); i++) {
                uintN digit;
                c = *state->cp++;
                if (!isASCIIHexDigit(c, &digit)) {
                    /* 
                     *  back off to accepting the original 
                     *  'u' or 'x' as a literal 
                     */
                    state->cp -= (i + 2);
                    n = *state->cp++;
                    break;
                }
                n = (n << 4) | digit;
            }
            c = (jschar)(n);
            goto doFlat;
        /* Character class escapes */
        case 'd':
            state->result = NewRENode(state, REOP_DIGIT);
doSimple:
            if (!state->result) 
                return JS_FALSE;
            state->progLength++;
            break;
        case 'D':
            state->result = NewRENode(state, REOP_NONDIGIT);
            goto doSimple;
        case 's':
            state->result = NewRENode(state, REOP_SPACE);
            goto doSimple;
        case 'S':
            state->result = NewRENode(state, REOP_NONSPACE);
            goto doSimple;
        case 'w':
            state->result = NewRENode(state, REOP_ALNUM);
            goto doSimple;
        case 'W':
            state->result = NewRENode(state, REOP_NONALNUM);
            goto doSimple;
        /* IdentityEscape */
        default:
            state->result = NewRENode(state, REOP_FLAT);
            if (!state->result) 
                return JS_FALSE;
            state->result->u.flat.chr = c;
            state->result->u.flat.length = 1;                    
            state->result->kid = (void *)(state->cp - 1);
            state->progLength += 3;
            break;
        }
        break;
    case '[':
        state->result = NewRENode(state, REOP_CLASS);
        if (!state->result) 
            return JS_FALSE;
        termStart = state->cp;
        state->result->u.ucclass.startIndex = termStart - state->cpbegin;
        while (JS_TRUE) {
            if (state->cp == state->cpend) {
                js_ReportCompileErrorNumber(state->meta, JSMSG_UNTERM_CLASS, termStart);
                return JS_FALSE;
            }
            if (*state->cp == '\\')
                state->cp++;
            else {
                if (*state->cp == ']') {
                    state->result->u.ucclass.kidlen = state->cp - termStart;
                    break;
                }
            }
            state->cp++;
        }
        foundCachedCopy = JS_FALSE;
        for (i = 0; i < CLASS_CACHE_SIZE; i++) {
            if (state->classCache[i].start) {
                if (state->classCache[i].length == state->result->u.ucclass.kidlen) {
                    foundCachedCopy = JS_TRUE;
                    for (n = 0; n < state->classCache[i].length; n++) {
                        if (state->classCache[i].start[n] != termStart[n]) {
                            foundCachedCopy = JS_FALSE;
                            break;
                        }
                    }
                    if (foundCachedCopy) {
                        state->result->u.ucclass.index = state->classCache[i].index;
                        break;
                    }
                }
            }
            else {
                state->classCache[i].start = termStart;
                state->classCache[i].length = state->result->u.ucclass.kidlen;
                state->classCache[i].index = state->classCount;
                break;
            }
        }
        if (!foundCachedCopy)
            state->result->u.ucclass.index = state->classCount++;
        /* 
         * Call calculateBitmapSize now as we want any errors it finds 
         * to be reported during the parse phase, not at execution.
         */
        if (!calculateBitmapSize(state, state->result, termStart, state->cp++))
            return JS_FALSE;
        state->progLength += 3; /* CLASS, <index> */
        break;

    case '.':
        state->result = NewRENode(state, REOP_DOT);
        goto doSimple;
    case '*':
    case '+':
    case '?':        
        js_ReportCompileErrorNumber(state->meta, JSMSG_BAD_QUANTIFIER, state->cp - 1);
        return JS_FALSE;
    case '{':
        /* Treat left-curly in a non-quantifier context as an error only 
         * if it's followed immediately by a decimal digit.
         * This is an Perl extension.
         */
        if ((state->cp != state->cpend) && JS7_ISDEC(*state->cp)) {
            js_ReportCompileErrorNumber(state->meta,
                                        JSMSG_BAD_QUANTIFIER, state->cp - 1);
            return JS_FALSE;
        }
        /* fall thru... */
    default:
        state->result = NewRENode(state, REOP_FLAT);
        if (!state->result) 
            return JS_FALSE;
        state->result->u.flat.chr = c;
        state->result->u.flat.length = 1;                    
        state->result->kid = (void *)(state->cp - 1);
        state->progLength += 3;
        break;
    }
    return parseQuantifier(state);
}

static JSBool 
parseQuantifier(CompilerState *state)
{
    RENode *term;
    term = state->result;
    if (state->cp < state->cpend) {
        switch (*state->cp) {
        case '+':
            state->result = NewRENode(state, REOP_QUANT);
            if (!state->result) 
                return JS_FALSE;
            state->result->u.range.min = 1;
            state->result->u.range.max = NO_MAX;
            /* <PLUS>, <next> ... <ENDCHILD> */
            state->progLength += 4;
            goto quantifier;
        case '*':
            state->result = NewRENode(state, REOP_QUANT);
            if (!state->result) 
                return JS_FALSE;
            state->result->u.range.min = 0;
            state->result->u.range.max = NO_MAX;
            /* <STAR>, <next> ... <ENDCHILD> */
            state->progLength += 4;
            goto quantifier;
        case '?':
            state->result = NewRENode(state, REOP_QUANT);
            if (!state->result) 
                return JS_FALSE;
            state->result->u.range.min = 0;
            state->result->u.range.max = 1;
            /* <OPT>, <next> ... <ENDCHILD> */
            state->progLength += 4;
            goto quantifier;
        case '{':       /* balance '}' */
            {
                const char *err;
                intN min = 0;
                intN max = -1;
                jschar c;
                const jschar *errp = state->cp++;
            
                c = *state->cp;
                if (JS7_ISDEC(c)) {
                    ++state->cp;
                    min = getDecimalValue(c, state);
                    c = *state->cp;
                }
                else {
                    /* For Perl etc. compatibility, if a curly is not
                     * followed by a proper digit, back off from it
                     * being a quantifier, and chew it up as a literal
                     * atom next time instead.
                     */
                    --state->cp;
                    return JS_TRUE; 
                }
                state->result = NewRENode(state, REOP_QUANT);
                if (!state->result) 
                    return JS_FALSE;

                if (min >> 16) {
                    err = JSMSG_MIN_TOO_BIG;
                    goto quantError;
                }
                if (c == ',') {
                    c = *++state->cp;
                    if (JS7_ISDEC(c)) {
                        ++state->cp;
                        max = getDecimalValue(c, state);
                        c = *state->cp;
                        if (max >> 16) {
                            err = JSMSG_MAX_TOO_BIG;
                            goto quantError;
                        }
                        if (min > max) {
                            err = JSMSG_OUT_OF_ORDER;
                            goto quantError;
                        }
                    }
                }
                else {
                    max = min;
                }
                state->result->u.range.min = min;
                state->result->u.range.max = max;
                /* QUANT, <min>, <max>, <next> ... <ENDCHILD> */
                state->progLength += 8; 
                                            /* balance '{' */
                if (c == '}')
                    goto quantifier;
                else {
                    err = JSMSG_UNTERM_QUANTIFIER;
quantError:
                    js_ReportCompileErrorNumber(state->meta, err, errp);
                    return JS_FALSE;
                }
            }
        }
    }
    return JS_TRUE;

quantifier:
    ++state->treeDepth;
    ++state->cp;
    state->result->kid = term;
    if ((state->cp < state->cpend) && (*state->cp == '?')) {
        ++state->cp;
        state->result->u.range.greedy = JS_FALSE;
    }
    else
        state->result->u.range.greedy = JS_TRUE;  
    return JS_TRUE;
}

#define CHECK_OFFSET(diff) (JS_ASSERT(((diff) >= -32768) && ((diff) <= 32767))) 
#define SET_OFFSET(pc,off) ((pc)[0] = JUMP_OFFSET_HI(off),                     \
                                 (pc)[1] = JUMP_OFFSET_LO(off))
#define GET_OFFSET(pc)     ((int16)(((pc)[0] << 8) | (pc)[1]))
#define OFFSET_LEN         (2)
#define GET_ARG(pc)        GET_OFFSET(pc)
#define SET_ARG(pc,arg)    SET_OFFSET(pc,arg)
#define ARG_LEN            OFFSET_LEN

/*
 * Recursively generate bytecode for the tree rooted at t. Iteratively.
 */

typedef struct {
    RENode *nextAlt;
    jsbytecode *nextAltFixup, *nextTermFixup, *endTermFixup;
    RENode *continueNode;
    REOp continueOp;
} EmitStateStackEntry;

static jsbytecode *
emitREBytecode(CompilerState *state, JS2RegExp *re, intN treeDepth, 
               jsbytecode *pc, RENode *t)
{
    ptrdiff_t diff;
    RECharSet *charSet;
    EmitStateStackEntry *emitStateSP, *emitStateStack = NULL;
    REOp op;

    if (treeDepth) {
        emitStateStack = 
            (EmitStateStackEntry *)malloc(sizeof(EmitStateStackEntry)
                                                                * treeDepth);
        if (!emitStateStack)
            return NULL;
    }
    emitStateSP = emitStateStack;
    op = t->op;

    while (JS_TRUE) {
        *pc++ = op;
        switch (op) {
        case REOP_EMPTY:
            --pc;
            break;

        case REOP_ALTPREREQ2:
        case REOP_ALTPREREQ:
            JS_ASSERT(emitStateSP);
            emitStateSP->endTermFixup = pc;
            pc += OFFSET_LEN;
            SET_ARG(pc, t->u.altprereq.ch1);
            pc += ARG_LEN;
            SET_ARG(pc, t->u.altprereq.ch2);
            pc += ARG_LEN;

            emitStateSP->nextAltFixup = pc;    /* address of next alternate */
            pc += OFFSET_LEN;

            emitStateSP->continueNode = t;
            emitStateSP->continueOp = REOP_JUMP;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->kid);
            op = t->op;
            continue;
            
        case REOP_JUMP:
            emitStateSP->nextTermFixup = pc;    /* address of following term */
            pc += OFFSET_LEN;
            diff = pc - emitStateSP->nextAltFixup;
            CHECK_OFFSET(diff);
            SET_OFFSET(emitStateSP->nextAltFixup, diff);
            emitStateSP->continueOp = REOP_ENDALT;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->u.kid2);
            op = t->op;
            continue;

        case REOP_ENDALT:
            diff = pc - emitStateSP->nextTermFixup;
            CHECK_OFFSET(diff);
            SET_OFFSET(emitStateSP->nextTermFixup, diff);
            if (t->op != REOP_ALT) {
                diff = pc - emitStateSP->endTermFixup;
                CHECK_OFFSET(diff);
                SET_OFFSET(emitStateSP->endTermFixup, diff);
            }
            break;

        case REOP_ALT:
            JS_ASSERT(emitStateSP);
            emitStateSP->nextAltFixup = pc; /* address of pointer to next alternate */
            pc += OFFSET_LEN;
            emitStateSP->continueNode = t;
            emitStateSP->continueOp = REOP_JUMP;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->kid);
            op = t->op;
            continue;

        case REOP_FLAT:
            /*
             * Consecutize FLAT's if possible.
             */
            if (t->kid) {
                while (t->next && (t->next->op == REOP_FLAT) 
                        && (((jschar*)(t->kid) + t->u.flat.length) 
                                        == (jschar*)(t->next->kid))) {
                    t->u.flat.length += t->next->u.flat.length;
                    t->next = t->next->next;
                }
            }
            if (t->kid && (t->u.flat.length > 1)) {
                if (state->flags & JSREG_FOLD)
                    pc[-1] = REOP_FLATi;
                else
                    pc[-1] = REOP_FLAT;
                SET_ARG(pc, (jschar *)(t->kid) - state->cpbegin);
                pc += ARG_LEN;
                SET_ARG(pc, t->u.flat.length);
                pc += ARG_LEN;
            }
            else {
                if (t->u.flat.chr < 256) {
                    if (state->flags & JSREG_FOLD)
                        pc[-1] = REOP_FLAT1i;
                    else
                        pc[-1] = REOP_FLAT1;
                    *pc++ = (jsbytecode)(t->u.flat.chr);
                }
                else {
                    if (state->flags & JSREG_FOLD)
                        pc[-1] = REOP_UCFLAT1i;
                    else
                        pc[-1] = REOP_UCFLAT1;
                    SET_ARG(pc, t->u.flat.chr);
                    pc += ARG_LEN;
                }
            }
            break;

        case REOP_LPAREN:
            JS_ASSERT(emitStateSP);
            SET_ARG(pc, t->u.parenIndex);
            pc += ARG_LEN;
            emitStateSP->continueNode = t;
            emitStateSP->continueOp = REOP_RPAREN;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->kid);
            op = t->op;
            continue;
        case REOP_RPAREN:
            SET_ARG(pc, t->u.parenIndex);
            pc += ARG_LEN;
            break;

        case REOP_BACKREF:
            SET_ARG(pc, t->u.parenIndex);
            pc += ARG_LEN;
            break;
        case REOP_ASSERT:
            JS_ASSERT(emitStateSP);
            emitStateSP->nextTermFixup = pc;
            pc += OFFSET_LEN;
            emitStateSP->continueNode = t;
            emitStateSP->continueOp = REOP_ASSERTTEST;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->kid);
            op = t->op;
            continue;
        case REOP_ASSERTTEST:
        case REOP_ASSERTNOTTEST:
            diff = pc - emitStateSP->nextTermFixup;
            CHECK_OFFSET(diff);
            SET_OFFSET(emitStateSP->nextTermFixup, diff);
            break;
        case REOP_ASSERT_NOT:
            JS_ASSERT(emitStateSP);
            emitStateSP->nextTermFixup = pc;
            pc += OFFSET_LEN;
            emitStateSP->continueNode = t;
            emitStateSP->continueOp = REOP_ASSERTNOTTEST;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->kid);
            op = t->op;
            continue;
        case REOP_QUANT:
            JS_ASSERT(emitStateSP);
            if ((t->u.range.min == 0) && (t->u.range.max == (uint16)(-1)))
                pc[-1] = (t->u.range.greedy) ? REOP_STAR : REOP_MINIMALSTAR;
            else
            if ((t->u.range.min == 0) && (t->u.range.max == 1))
                pc[-1] = (t->u.range.greedy) ? REOP_OPT : REOP_MINIMALOPT;
            else
            if ((t->u.range.min == 1) && (t->u.range.max == (uint16)(-1)))
                pc[-1] = (t->u.range.greedy) ? REOP_PLUS : REOP_MINIMALPLUS;
            else {
                if (!t->u.range.greedy) pc[-1] = REOP_MINIMALQUANT;
                SET_ARG(pc, t->u.range.min);
                pc += ARG_LEN;
                SET_ARG(pc, t->u.range.max);
                pc += ARG_LEN;
            }
            emitStateSP->nextTermFixup = pc;
            pc += OFFSET_LEN;
            emitStateSP->continueNode = t;
            emitStateSP->continueOp = REOP_ENDCHILD;
            ++emitStateSP;
            JS_ASSERT((emitStateSP - emitStateStack) <= treeDepth);
            t = (RENode *)(t->kid);
            op = t->op;
            continue;
        case REOP_ENDCHILD:
            diff = pc - emitStateSP->nextTermFixup;
            CHECK_OFFSET(diff);
            SET_OFFSET(emitStateSP->nextTermFixup, diff);
            break;
        case REOP_CLASS:
            if (!t->u.ucclass.sense)
                pc[-1] = REOP_NCLASS;
            SET_ARG(pc, t->u.ucclass.index);
            pc += ARG_LEN;
            charSet = &re->classList[t->u.ucclass.index];
            charSet->converted = JS_FALSE;
            charSet->length = t->u.ucclass.bmsize;
            charSet->u.src.startIndex = t->u.ucclass.startIndex;
            charSet->u.src.length = t->u.ucclass.kidlen;
            charSet->sense = t->u.ucclass.sense;
            break;
        default:
            break;
        }
        t = t->next;
        if (t == NULL) {
            if (emitStateSP == emitStateStack)
                break;
            --emitStateSP;
            t = emitStateSP->continueNode;
            op = emitStateSP->continueOp;
        }
        else
            op = t->op;
    }
    if (emitStateStack)
        free(emitStateStack);
    return pc;
}

/*
 * Save the current state of the match - the position in the input
 * text as well as the position in the bytecode. The state of any 
 * parent expressions is also saved (preceding state).
 * Contents of parenCount parentheses from parenIndex are also saved.
 */
static REBackTrackData *
pushBackTrackState(REGlobalData *gData, REOp op,
                   jsbytecode *target, REMatchState *x, const jschar *cp,
                   intN parenIndex, intN parenCount)
{
    intN i;
    REBackTrackData *result 
        = (REBackTrackData *)((char *)(gData->backTrackSP) + gData->cursz);

    size_t sz = sizeof(REBackTrackData)
                    + gData->stateStackTop * sizeof(REProgState)
                    + parenCount * sizeof(RECapture);


    if (((char *)result + sz) 
                    > (char *)gData->backTrackStack + gData->maxBackTrack) {
        ptrdiff_t offset = (char *)result - (char *)gData->backTrackStack;
        gData->backTrackStack 
            = (REBackTrackData *)realloc(gData->backTrackStack, 
                                              gData->maxBackTrack
                                               + gData->maxBackTrack);
        gData->maxBackTrack <<= 1;
        if (!gData->backTrackStack)
            return NULL;
        result = (REBackTrackData *)((char *)gData->backTrackStack + offset);
    }
    gData->backTrackSP = result;
    result->sz = gData->cursz;
    gData->cursz = sz;

    result->backtrack_op = op;
    result->backtrack_pc = target;
    result->cp = cp;
    result->parenCount = parenCount;

    result->precedingStateTop = gData->stateStackTop;
    JS_ASSERT(gData->stateStackTop);
    memcpy(result + 1, gData->stateStack, 
           sizeof(REProgState) * result->precedingStateTop);
    
    if (parenCount != -1) {
        result->parenIndex = parenIndex;
        memcpy((char *)(result + 1) 
                    + sizeof(REProgState) * result->precedingStateTop,
               &x->parens[parenIndex], 
               sizeof(RECapture) * parenCount);
        for (i = 0; i < parenCount; i++)
            x->parens[parenIndex + i].index = -1;
    }

    return result;
}


/*
 *   Consecutive literal characters.
 */
static REMatchState *
flatNMatcher(REGlobalData *gData, REMatchState *x, const jschar *matchChars, 
             intN length)
{
    intN i;
    if ((x->cp + length) > gData->cpend)
        return NULL;
    for (i = 0; i < length; i++) {
        if (matchChars[i] != x->cp[i])
            return NULL;
    }
    x->cp += length;
    return x;
}

static REMatchState *
flatNIMatcher(REGlobalData *gData, REMatchState *x, const jschar *matchChars, 
              intN length)
{
    intN i;
    if ((x->cp + length) > gData->cpend)
        return NULL;
    for (i = 0; i < length; i++) {
        if (canonicalize(matchChars[i]) 
                != canonicalize(x->cp[i]))
            return NULL;
    }
    x->cp += length;
    return x;
}

/*
 * 1. Evaluate DecimalEscape to obtain an EscapeValue E.
 * 2. If E is not a character then go to step 6.
 * 3. Let ch be E's character.
 * 4. Let A be a one-element RECharSet containing the character ch.
 * 5. Call CharacterSetMatcher(A, false) and return its Matcher result.
 * 6. E must be an integer. Let n be that integer.
 * 7. If n=0 or n>NCapturingParens then throw a SyntaxError exception.
 * 8. Return an internal Matcher closure that takes two arguments, a State x 
 *    and a Continuation c, and performs the following:
 *     1. Let cap be x's captures internal array.
 *     2. Let s be cap[n].
 *     3. If s is undefined, then call c(x) and return its result.
 *     4. Let e be x's endIndex.
 *     5. Let len be s's length.
 *     6. Let f be e+len.
 *     7. If f>InputLength, return failure.
 *     8. If there exists an integer i between 0 (inclusive) and len (exclusive)
 *        such that Canonicalize(s[i]) is not the same character as 
 *        Canonicalize(Input [e+i]), then return failure.
 *     9. Let y be the State (f, cap).
 *     10. Call c(y) and return its result.
 */
static REMatchState *
backrefMatcher(REGlobalData *gData, REMatchState *x, uintN parenIndex)
{
    uintN len;
    uintN i;
    const jschar *parenContent;
    RECapture *s = &x->parens[parenIndex];
    if (s->index == -1)
        return x;

    len = s->length;
    if ((x->cp + len) > gData->cpend)
        return NULL;
    
    parenContent = &gData->cpbegin[s->index];
    if (gData->regexp->flags & JSREG_FOLD) {
        for (i = 0; i < len; i++) {
            if (canonicalize(parenContent[i]) 
                                    != canonicalize(x->cp[i]))
                return NULL;
        }
    }
    else {
        for (i = 0; i < len; i++) {
            if (parenContent[i] != x->cp[i])
                return NULL;
        }
    }
    x->cp += len;
    return x;
}


/* Add a single character to the RECharSet */
static void 
addCharacterToCharSet(RECharSet *cs, jschar c)
{
    uintN byteIndex = (uintN)(c / 8);
    JS_ASSERT(c <= cs->length);
    cs->u.bits[byteIndex] |= 1 << (c & 0x7);
}


/* Add a character range, c1 to c2 (inclusive) to the RECharSet */
static void 
addCharacterRangeToCharSet(RECharSet *cs, jschar c1, jschar c2)
{
    uintN i;

    uintN byteIndex1 = (uintN)(c1 / 8);
    uintN byteIndex2 = (uintN)(c2 / 8);

    JS_ASSERT((c2 <= cs->length) && (c1 <= c2));

    c1 &= 0x7;
    c2 &= 0x7;

    if (byteIndex1 == byteIndex2)
        cs->u.bits[byteIndex1] |= ((uint8)(0xFF) >> (7 - (c2 - c1))) << c1;
    else {
        cs->u.bits[byteIndex1] |= 0xFF << c1;
        for (i = byteIndex1 + 1; i < byteIndex2; i++)
            cs->u.bits[i] = 0xFF;
        cs->u.bits[byteIndex2] |= (uint8)(0xFF) >> (7 - c2);
    }
}

/* Compile the source of the class into a RECharSet */
static JSBool 
processCharSet(REGlobalData *gData, RECharSet *charSet)
{
    const jschar *src = JSSTRING_CHARS(gData->regexp->source) 
                                        + charSet->u.src.startIndex;
    const jschar *end = src + charSet->u.src.length;

    jschar rangeStart, thisCh;
    uintN byteLength;
    jschar c;
    uintN n;
    intN nDigits;
    intN i;
    JSBool inRange = JS_FALSE;

    JS_ASSERT(!charSet->converted);
    charSet->converted = JS_TRUE;

    byteLength = (charSet->length / 8) + 1;    
    charSet->u.bits = (uint8 *)malloc(byteLength);
    if (!charSet->u.bits)
        return JS_FALSE;
    memset(charSet->u.bits, 0, byteLength);
    
    if (src == end)
        return JS_TRUE;

    if (*src == '^') {
        JS_ASSERT(charSet->sense == JS_FALSE);
        ++src;
    }
    else
        JS_ASSERT(charSet->sense == JS_TRUE);


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
                if (((src + 1) < end) && JS_ISWORD(src[1]))
                    thisCh = (jschar)(*src++ & 0x1F);
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
                n = 0;
                for (i = 0; (i < nDigits) && (src < end); i++) {
                    uintN digit;
                    c = *src++;
                    if (!isASCIIHexDigit(c, &digit)) {
                        /* 
                         * Back off to accepting the original '\' 
                         * as a literal
                         */
                        src -= (i + 1);
                        n = '\\';
                        break;
                    }
                    n = (n << 4) | digit;
                }
                thisCh = (jschar)(n);
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                /*
                 *  This is a non-ECMA extension - decimal escapes (in this
                 *  case, octal!) are supposed to be an error inside class
                 *  ranges, but supported here for backwards compatibility.
                 *
                 */
                n = JS7_UNDEC(c);
                c = *src;
                if ('0' <= c && c <= '7') {
                    src++;
                    n = 8 * n + JS7_UNDEC(c);
                    c = *src;
                    if ('0' <= c && c <= '7') {
                        src++;
                        i = 8 * n + JS7_UNDEC(c);
                        if (i <= 0377)
                            n = i;
                        else
                            src--;
                    }
                }
                thisCh = (jschar)(n);
                break;

            case 'd':
                addCharacterRangeToCharSet(charSet, '0', '9');
                continue;   /* don't need range processing */
            case 'D':
                addCharacterRangeToCharSet(charSet, 0, '0' - 1);
                addCharacterRangeToCharSet(charSet, (jschar)('9' + 1),
                                            (jschar)(charSet->length));
                continue;
            case 's':
                for (i = (intN)(charSet->length); i >= 0; i--)
                    if (JS_ISSPACE(i))
                        addCharacterToCharSet(charSet, (jschar)(i));
                continue;
            case 'S':
                for (i = (intN)(charSet->length); i >= 0; i--)
                    if (!JS_ISSPACE(i))
                        addCharacterToCharSet(charSet, (jschar)(i));
                continue;
            case 'w':
                for (i = (intN)(charSet->length); i >= 0; i--)
                    if (JS_ISWORD(i))
                        addCharacterToCharSet(charSet, (jschar)(i));
                continue;
            case 'W':
                for (i = (intN)(charSet->length); i >= 0; i--)
                    if (!JS_ISWORD(i))
                        addCharacterToCharSet(charSet, (jschar)(i));
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
            if (gData->regexp->flags & JSREG_FOLD) {
                jschar minch = (jschar)65535;
                jschar maxch = 0;
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
            inRange = JS_FALSE;
        }
        else {
            if (gData->regexp->flags & JSREG_FOLD)
                addCharacterToCharSet(charSet, canonicalize(thisCh));
            addCharacterToCharSet(charSet, thisCh);
            if (src < (end - 1)) {
                if (*src == '-') {
                    ++src;
                    inRange = JS_TRUE;
                    rangeStart = thisCh;
                }
            }
        }
    }
    return JS_TRUE;
}

void
js_DestroyRegExp(JS2RegExp *re)
{
    uintN i;
    if (re->classList) {
        for (i = 0; i < re->classCount; i++) {
            if (re->classList[i].converted)
                free(re->classList[i].u.bits);
            re->classList[i].u.bits = NULL;
        }
        free(re->classList);
    }
}

static JSBool
reallocStateStack(REGlobalData *gData)
{
    size_t sz = sizeof(REProgState) * gData->maxStateStack;
    gData->maxStateStack <<= 1;
    gData->stateStack 
        = (REProgState *)realloc(gData->stateStack, sz + sz);
    if (!gData->stateStack) {
        gData->ok = JS_FALSE;
        return JS_FALSE;
    }
    return JS_TRUE;
}

/*
*   Apply the current op against the given input to see if
*   it's going to match or fail. Return false if we don't 
*   get a match, true if we do and update the state of the
*   input and pc if the update flag is true.
*/
static REMatchState *simpleMatch(REGlobalData *gData, REMatchState *x, 
                                 REOp op, jsbytecode **startpc, JSBool update)
{
    REMatchState *result = NULL;
    jschar matchCh;
    intN parenIndex;
    intN offset, length, index;
    jsbytecode *pc = *startpc;  /* pc has already been incremented past op */
    const jschar *source;
    const jschar *startcp = x->cp;
    jschar ch;
    RECharSet *charSet;


    switch (op) {
    default:
        JS_ASSERT(JS_FALSE);
    case REOP_BOL:
        if (x->cp != gData->cpbegin) {
            if (gData->globalMultiline ||
                        (gData->regexp->flags & JSREG_MULTILINE)) {
                if (!RE_IS_LINE_TERM(x->cp[-1]))
                    break;
            }
            else
                break;
        }
        result = x;
        break;
    case REOP_EOL:
        if (x->cp != gData->cpend) {
            if (gData->globalMultiline ||
                        (gData->regexp->flags & JSREG_MULTILINE)) {
                if (!RE_IS_LINE_TERM(*x->cp))
                    break;
            }
            else
                break;
        }
        result = x;
        break;
    case REOP_WBDRY:
        if ((x->cp == gData->cpbegin || !JS_ISWORD(x->cp[-1])) 
                           ^ !((x->cp != gData->cpend) && JS_ISWORD(*x->cp)))
            result = x;
        break;
    case REOP_WNONBDRY:
        if ((x->cp == gData->cpbegin || !JS_ISWORD(x->cp[-1]))
                            ^ ((x->cp != gData->cpend) && JS_ISWORD(*x->cp)))
            result = x;
        break;
    case REOP_DOT:
        if (x->cp != gData->cpend && !RE_IS_LINE_TERM(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_DIGIT:
        if (x->cp != gData->cpend && JS_ISDIGIT(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_NONDIGIT:
        if (x->cp != gData->cpend && !JS_ISDIGIT(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_ALNUM:
        if (x->cp != gData->cpend && JS_ISWORD(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_NONALNUM:
        if (x->cp != gData->cpend && !JS_ISWORD(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_SPACE:
        if (x->cp != gData->cpend && JS_ISSPACE(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_NONSPACE:
        if (x->cp != gData->cpend && !JS_ISSPACE(*x->cp)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_BACKREF:
        parenIndex = GET_ARG(pc);
        pc += ARG_LEN;
        result = backrefMatcher(gData, x, parenIndex);
        break;
    case REOP_FLAT:
        offset = GET_ARG(pc);
        pc += ARG_LEN;
        length = GET_ARG(pc);
        pc += ARG_LEN;
        source = JSSTRING_CHARS(gData->regexp->source) + offset;
        if ((x->cp + length) <= gData->cpend) {
            for (index = 0; index < length; index++) {
                if (source[index] != x->cp[index])
                    return NULL;
            }
            x->cp += length;
            result = x;
        }
        break;
    case REOP_FLAT1:
        matchCh = *pc++;
        if ((x->cp != gData->cpend) && (*x->cp == matchCh)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_FLATi:
        offset = GET_ARG(pc);
        pc += ARG_LEN;
        length = GET_ARG(pc);
        pc += ARG_LEN;
        source = JSSTRING_CHARS(gData->regexp->source);
        result = flatNIMatcher(gData, x, source + offset, length);
        break;
    case REOP_FLAT1i:
        matchCh = *pc++;
        if ((x->cp != gData->cpend) 
                && (canonicalize(*x->cp) == canonicalize(matchCh))) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_UCFLAT1:
        matchCh = GET_ARG(pc);
        pc += ARG_LEN;
        if ((x->cp != gData->cpend) && (*x->cp == matchCh)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_UCFLAT1i:
        matchCh = GET_ARG(pc);
        pc += ARG_LEN;
        if ((x->cp != gData->cpend) 
                && (canonicalize(*x->cp) == canonicalize(matchCh))) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_CLASS:
        index = GET_ARG(pc);
        pc += ARG_LEN;
        if (x->cp != gData->cpend) {
            charSet = &gData->regexp->classList[index];
            JS_ASSERT(charSet->converted);
            ch = *x->cp;
            index = ch / 8;
            if ((charSet->length != 0) && 
                   ( (ch <= charSet->length)
                   && ((charSet->u.bits[index] & (1 << (ch & 0x7))) != 0) )) {
                result = x;
                result->cp++;
            }
        }
        break;
    case REOP_NCLASS:
        index = GET_ARG(pc);
        pc += ARG_LEN;
        if (x->cp != gData->cpend) {
            charSet = &gData->regexp->classList[index];
            JS_ASSERT(charSet->converted);
            ch = *x->cp;
            index = ch / 8;
            if ((charSet->length == 0) || 
                   ( (ch > charSet->length)
                   || ((charSet->u.bits[index] & (1 << (ch & 0x7))) == 0) )) {
                result = x;
                result->cp++;
            }
        }
        break;
    }
    if (result != NULL) {
        if (update)
            *startpc = pc;
        else
            x->cp = startcp;
        return result;
    }
    x->cp = startcp;
    return NULL;
}

static REMatchState *
executeREBytecode(REGlobalData *gData, REMatchState *x, JSBool allowSkip)
{
    REMatchState *result;
    REBackTrackData *backTrackData;
    intN offset;
    jsbytecode *nextpc;
    REOp nextop;
    RECapture *cap;
    REProgState *curState;
    const jschar *startcp;
    uintN parenIndex, k;
    uintN parenSoFar = 0;

    jschar matchCh1, matchCh2;
    RECharSet *charSet;

    JSBool anchor;
    jsbytecode *pc = gData->regexp->program;
    REOp op = (REOp)(*pc++);

    /*
     *   If the first node is a simple match, step the index into
     *   the string until that match is made, or fail if it can't be
     *   found at all.
     */
    if (allowSkip && REOP_IS_SIMPLE(op)) {
        anchor = JS_FALSE;
        while (x->cp <= gData->cpend) {
            nextpc = pc;    /* reset back to start each time */
            result = simpleMatch(gData, x, op, &nextpc, JS_TRUE);
            if (result) {
                anchor = JS_TRUE;
                x = result;
                pc = nextpc;    /* accept skip to next opcode */
                op = (REOp)(*pc++);
                break;
            }
            else {
                gData->skipped++;
                x->cp++;
            }
        }
        if (!anchor)
            return NULL;
    }

    while (JS_TRUE) {
        if (REOP_IS_SIMPLE(op))
            result = simpleMatch(gData, x, op, &pc, JS_TRUE);
        else {
            curState = &gData->stateStack[gData->stateStackTop];
            switch (op) {
            case REOP_EMPTY:
                result = x;
                break;

            case REOP_ALTPREREQ2:
                nextpc = pc + GET_OFFSET(pc);   /* start of next op */
                pc += ARG_LEN;
                matchCh2 = GET_ARG(pc);
                pc += ARG_LEN;
                k = GET_ARG(pc);
                pc += ARG_LEN;

                if (x->cp != gData->cpend) {
                    charSet = &gData->regexp->classList[k];
                    if (!charSet->converted)
                        if (!processCharSet(gData, charSet))
                            return JS_FALSE;
                    matchCh1 = *x->cp;
                    k = matchCh1 / 8;
                    if ((charSet->length != 0) && 
                           ( (matchCh1 <= charSet->length)
                           && ((charSet->u.bits[k] 
                                    & (1 << (matchCh1 & 0x7))) != 0) )) {
                        result = NULL;
                        break;
                    }
                }
                else {
                    result = NULL;
                    break;
                }

                if ((x->cp == gData->cpend) || (*x->cp != matchCh2)) {
                    result = NULL;
                    break;
                }
                goto doAlt;

            case REOP_ALTPREREQ:
                nextpc = pc + GET_OFFSET(pc);   /* start of next op */
                pc += ARG_LEN;
                matchCh1 = GET_ARG(pc);
                pc += ARG_LEN;
                matchCh2 = GET_ARG(pc);
                pc += ARG_LEN;
                if ((x->cp == gData->cpend) 
                        || ((*x->cp != matchCh1) && (*x->cp != matchCh2))) {
                    result = NULL;
                    break;
                }
                /* else false thru... */

            case REOP_ALT:
doAlt:
                nextpc = pc + GET_OFFSET(pc);   /* start of next alternate */
                pc += ARG_LEN;                  /* start of this alternate */
                curState->parenSoFar = parenSoFar;
                ++gData->stateStackTop;
                if (gData->stateStackTop == gData->maxStateStack)
                    if (!reallocStateStack(gData)) 
                        return NULL;
                op = (REOp)(*pc++);
                startcp = x->cp;
                if (REOP_IS_SIMPLE(op)) {
                    if (!simpleMatch(gData, x, op, &pc, JS_TRUE)) {
                        op = (REOp)(*nextpc++);
                        pc = nextpc;
                        continue;
                    }
                    else { /* accept the match and move on */
                        result = x;
                        op = (REOp)(*pc++);
                    }
                }
                nextop = (REOp)(*nextpc++);
                if (!pushBackTrackState(gData, nextop, nextpc, x, startcp, 0, 0))
                    return NULL;
                continue;

            /*
             * Occurs at (succesful) end of REOP_ALT, 
             */
            case REOP_JUMP:
                --gData->stateStackTop;
                offset = GET_OFFSET(pc);
                pc += offset;
                op = (REOp)(*pc++);
                continue;
            
            /*
             * Occurs at last (succesful) end of REOP_ALT, 
             */
            case REOP_ENDALT:
                --gData->stateStackTop;
                op = (REOp)(*pc++);
                continue;
            
            case REOP_LPAREN:
                parenIndex = GET_ARG(pc);
                if ((parenIndex + 1) > parenSoFar)
                    parenSoFar = parenIndex + 1;
                pc += ARG_LEN;
                x->parens[parenIndex].index = x->cp - gData->cpbegin;
                x->parens[parenIndex].length = 0;
                op = (REOp)(*pc++);
                continue;
            case REOP_RPAREN:
                parenIndex = GET_ARG(pc);
                pc += ARG_LEN;
                cap = &x->parens[parenIndex];
                cap->length = x->cp - (gData->cpbegin + cap->index);
                op = (REOp)(*pc++);
                continue;

            case REOP_ASSERT:
                nextpc = pc + GET_OFFSET(pc);  /* start of term after ASSERT */
                pc += ARG_LEN;                 /* start of ASSERT child */
                op = (REOp)(*pc++);
                if (REOP_IS_SIMPLE(op) 
                        && !simpleMatch(gData, x, op, &pc, JS_FALSE)) {
                    result = NULL;
                    break;
                }
                else {
                    curState->u.assertion.top 
                        = (char *)gData->backTrackSP 
                                    - (char *)gData->backTrackStack;
                    curState->u.assertion.sz = gData->cursz;
                    curState->index = x->cp - gData->cpbegin;
                    curState->parenSoFar = parenSoFar;
                    ++gData->stateStackTop;
                    if (gData->stateStackTop == gData->maxStateStack)
                        if (!reallocStateStack(gData)) 
                            return NULL;
                    if (!pushBackTrackState(gData, REOP_ASSERTTEST, 
                                            nextpc, x, x->cp, 0, 0))
                        return NULL;
                }
                continue;
            case REOP_ASSERT_NOT:
                nextpc = pc + GET_OFFSET(pc);
                pc += ARG_LEN;
                op = (REOp)(*pc++);
                if (REOP_IS_SIMPLE(op)
                    /* Note - fail to fail! */
                        && simpleMatch(gData, x, op, &pc, JS_FALSE)) {
                    result = NULL;
                    break;
                }
                else {
                    curState->u.assertion.top 
                        = (char *)gData->backTrackSP 
                                    - (char *)gData->backTrackStack;
                    curState->u.assertion.sz = gData->cursz;
                    curState->index = x->cp - gData->cpbegin;
                    curState->parenSoFar = parenSoFar;
                    ++gData->stateStackTop;
                    if (gData->stateStackTop == gData->maxStateStack)
                        if (!reallocStateStack(gData)) 
                            return NULL;
                    if (!pushBackTrackState(gData, REOP_ASSERTNOTTEST, 
                                            nextpc, x, x->cp, 0, 0))
                        return NULL;
                }
                continue;
            case REOP_ASSERTTEST:
                --gData->stateStackTop;
                --curState;
                x->cp = gData->cpbegin + curState->index;
                gData->backTrackSP
                    = (REBackTrackData *)((char *)gData->backTrackStack
                                                + curState->u.assertion.top);
                gData->cursz = curState->u.assertion.sz;
                if (result != NULL)
                    result = x;
                break;
            case REOP_ASSERTNOTTEST:
                --gData->stateStackTop;
                --curState;
                x->cp = gData->cpbegin + curState->index;
                gData->backTrackSP 
                    = (REBackTrackData *)((char *)gData->backTrackStack 
                                                + curState->u.assertion.top);
                gData->cursz = curState->u.assertion.sz;
                if (result == NULL)
                    result = x;
                else
                    result = NULL;
                break;

            case REOP_END:
                if (x != NULL)
                    return x;
                break;

            case REOP_STAR:
                curState->u.quantifier.min = 0;
                curState->u.quantifier.max = NO_MAX;
                goto quantcommon;
            case REOP_PLUS:
                curState->u.quantifier.min = 1;
                curState->u.quantifier.max = NO_MAX;
                goto quantcommon;
            case REOP_OPT:
                curState->u.quantifier.min = 0;
                curState->u.quantifier.max = 1;
                goto quantcommon;
            case REOP_QUANT:
                curState->u.quantifier.min = GET_ARG(pc);
                pc += ARG_LEN;
                curState->u.quantifier.max = GET_ARG(pc);
                pc += ARG_LEN;
quantcommon:
                if (curState->u.quantifier.max == 0) {
                    pc = pc + GET_OFFSET(pc);
                    op = (REOp)(*pc++);
                    result = x;
                    continue;
                }
                /* Step over <next> */
                nextpc = pc + ARG_LEN;
                op = (REOp)(*nextpc++);
                startcp = x->cp;
                if (REOP_IS_SIMPLE(op)) {
                    if (!simpleMatch(gData, x, op, &nextpc, JS_TRUE)) {
                        if (curState->u.quantifier.min == 0)
                            result = x;
                        else
                            result = NULL;
                        pc = pc + GET_OFFSET(pc);
                        break;
                    }
                    else {
                        op = (REOp)(*nextpc++);
                        result = x;
                    }
                }
                curState->index = startcp - gData->cpbegin;
                curState->continue_op = REOP_REPEAT;
                curState->continue_pc = pc;
                curState->parenSoFar = parenSoFar;
                ++gData->stateStackTop;
                if (gData->stateStackTop == gData->maxStateStack)
                    if (!reallocStateStack(gData)) 
                        return NULL;
                if (curState->u.quantifier.min == 0)
                    if (!pushBackTrackState(gData, REOP_REPEAT, 
                                            pc, x, startcp, 0, 0))
                        return NULL;
                pc = nextpc;
                continue;

            case REOP_ENDCHILD: /* marks the end of a quantifier child */
                pc = curState[-1].continue_pc;
                op = curState[-1].continue_op;
                continue;

            case REOP_REPEAT:
                --curState;
repeatAgain:
                --gData->stateStackTop;
                if (result == NULL) {
                    /*
                     *  There's been a failure, see if we have enough children.
                     */
                    if (curState->u.quantifier.min == 0) {
                        result = x;
                        goto repeatDone;
                    }
                    break;
                }
                else {
                    if ((curState->u.quantifier.min == 0)
                            && (x->cp == gData->cpbegin + curState->index)) {
                        /* matched an empty string, that'll get us nowhere */
                        result = NULL;
                        break;
                    }
                    if (curState->u.quantifier.min != 0) 
                        curState->u.quantifier.min--;
                    if (curState->u.quantifier.max != (uint16)(-1)) 
                        curState->u.quantifier.max--;
                    if (curState->u.quantifier.max == 0) {
                        result = x;
                        goto repeatDone;
                    }
                    nextpc = pc + ARG_LEN;
                    nextop = (REOp)(*nextpc);
                    startcp = x->cp;
                    if (REOP_IS_SIMPLE(nextop)) {
                        nextpc++;
                        if (!simpleMatch(gData, x, nextop, &nextpc, JS_TRUE)) {
                            if (curState->u.quantifier.min == 0) {
                                result = x;
                                goto repeatDone;
                            }
                            else
                                result = NULL;
                            break;
                        }
                        result = x;
                    }
                    curState->index = startcp - gData->cpbegin;
                    ++gData->stateStackTop;
                    if (gData->stateStackTop == gData->maxStateStack)
                        if (!reallocStateStack(gData)) 
                            return NULL;
                    if (curState->u.quantifier.min == 0)
                        if (!pushBackTrackState(gData, REOP_REPEAT,
                                                pc, x, startcp, 
                                                curState->parenSoFar, 
                                                parenSoFar 
                                                    - curState->parenSoFar)) 
                            return NULL;
                    if (*nextpc == REOP_ENDCHILD)
                        goto repeatAgain;
                    pc = nextpc;
                    op = (REOp)(*pc++);
                    parenSoFar = curState->parenSoFar;
                }
                continue;                       
repeatDone:
                pc = pc + GET_OFFSET(pc);
                break;
                

            case REOP_MINIMALSTAR:
                curState->u.quantifier.min = 0;
                curState->u.quantifier.max = NO_MAX;
                goto minimalquantcommon;
            case REOP_MINIMALPLUS:
                curState->u.quantifier.min = 1;
                curState->u.quantifier.max = NO_MAX;
                goto minimalquantcommon;
            case REOP_MINIMALOPT:
                curState->u.quantifier.min = 0;
                curState->u.quantifier.max = 1;
                goto minimalquantcommon;
            case REOP_MINIMALQUANT:
                curState->u.quantifier.min = GET_ARG(pc);
                pc += ARG_LEN;
                curState->u.quantifier.max = GET_ARG(pc);
                pc += ARG_LEN;
minimalquantcommon:
                curState->index = x->cp - gData->cpbegin;
                curState->parenSoFar = parenSoFar;
                ++gData->stateStackTop;
                if (gData->stateStackTop == gData->maxStateStack)
                    if (!reallocStateStack(gData)) 
                        return NULL;
                if (curState->u.quantifier.min != 0) {
                    curState->continue_op = REOP_MINIMALREPEAT;
                    curState->continue_pc = pc;
                    /* step over <next> */
                    pc += ARG_LEN;
                    op = (REOp)(*pc++);
                }
                else {
                    if (!pushBackTrackState(gData, REOP_MINIMALREPEAT, 
                                            pc, x, x->cp, 0, 0))
                        return NULL;
                    --gData->stateStackTop;
                    pc = pc + GET_OFFSET(pc);
                    op = (REOp)(*pc++);
                }
                continue;

            case REOP_MINIMALREPEAT:
                --gData->stateStackTop;
                --curState;

                if (result == NULL) {
                    /* 
                     * Non-greedy failure - try to consume another child.
                     */
                    if ((curState->u.quantifier.max == (uint16)(-1))
                            || (curState->u.quantifier.max > 0)) {
                        curState->index = x->cp - gData->cpbegin;
                        curState->continue_op = REOP_MINIMALREPEAT;
                        curState->continue_pc = pc;
                        pc += ARG_LEN;
                        for (k = curState->parenSoFar; k < parenSoFar; k++)
                            x->parens[k].index = -1;
                        ++gData->stateStackTop;
                        if (gData->stateStackTop == gData->maxStateStack)
                            if (!reallocStateStack(gData)) 
                                return NULL;
                        op = (REOp)(*pc++);
                        continue;
                    }
                    else {
                        /* Don't need to adjust pc since we're going to pop. */
                        break;
                    }
                }
                else {
                    if ((curState->u.quantifier.min == 0)
                            && (x->cp == gData->cpbegin + curState->index)) {
                        /* Matched an empty string, that'll get us nowhere. */
                        result = NULL;
                        break;
                    }
                    if (curState->u.quantifier.min != 0) 
                        curState->u.quantifier.min--;
                    if (curState->u.quantifier.max != (uint16)(-1)) 
                        curState->u.quantifier.max--;
                    if (curState->u.quantifier.min != 0) {
                        curState->continue_op = REOP_MINIMALREPEAT;
                        curState->continue_pc = pc;
                        pc += ARG_LEN;
                        for (k = curState->parenSoFar; k < parenSoFar; k++)
                            x->parens[k].index = -1;
                        curState->index = x->cp - gData->cpbegin;
                        ++gData->stateStackTop;
                        if (gData->stateStackTop == gData->maxStateStack)
                            if (!reallocStateStack(gData)) 
                                return NULL;
                        op = (REOp)(*pc++);
                        continue;                       
                    }
                    else {
                        curState->index = x->cp - gData->cpbegin;
                        curState->parenSoFar = parenSoFar;
                        ++gData->stateStackTop;
                        if (gData->stateStackTop == gData->maxStateStack)
                            if (!reallocStateStack(gData)) 
                                return NULL;
                        if (!pushBackTrackState(gData, REOP_MINIMALREPEAT, 
                                                pc, x, x->cp, 
                                                curState->parenSoFar, 
                                                parenSoFar 
                                                    - curState->parenSoFar)) 
                            return NULL;
                        --gData->stateStackTop;
                        pc = pc + GET_OFFSET(pc);
                        op = (REOp)(*pc++);
                        continue;
                    }
                }

            default:
                JS_ASSERT(JS_FALSE);

            }
        }
        /*
         *  If the match failed and there's a backtrack option, take it.
         *  Otherwise this is a complete and utter failure.
         */
        if (result == NULL) {
            if (gData->cursz > 0) {
                backTrackData = gData->backTrackSP;
                gData->cursz = backTrackData->sz;
                gData->backTrackSP 
                    = (REBackTrackData *)((char *)backTrackData 
                                                    - backTrackData->sz);
                x->cp = backTrackData->cp;
                pc = backTrackData->backtrack_pc; 
                op = backTrackData->backtrack_op;                
                gData->stateStackTop = backTrackData->precedingStateTop;
                JS_ASSERT(gData->stateStackTop);   
                
                memcpy(gData->stateStack, backTrackData + 1, 
                       sizeof(REProgState) * backTrackData->precedingStateTop);
                curState = &gData->stateStack[gData->stateStackTop - 1];

                if (backTrackData->parenCount) {
                    memcpy(&x->parens[backTrackData->parenIndex], 
                           (char *)(backTrackData + 1) + sizeof(REProgState) * backTrackData->precedingStateTop,
                           sizeof(RECapture) * backTrackData->parenCount);
                    parenSoFar = backTrackData->parenIndex + backTrackData->parenCount;
                }
                else {
                    for (k = curState->parenSoFar; k < parenSoFar; k++)
                        x->parens[k].index = -1;
                    parenSoFar = curState->parenSoFar;
                }
                continue;
            }
            else
                return NULL;
        }
        else
            x = result;
        
        /*
         *  Continue with the expression.
         */
        op = (REOp)*pc++;
    }
    return NULL;
}

static REMatchState *
MatchRegExp(REGlobalData *gData, REMatchState *x)
{
    REMatchState *result;
    const jschar *cp = x->cp;
    const jschar *cp2;
    uintN j;

    /* 
     * Have to include the position beyond the last character
     * in order to detect end-of-input/line condition.
     */
    for (cp2 = cp; cp2 <= gData->cpend; cp2++) {
        gData->skipped = cp2 - cp;
        x->cp = cp2;
        for (j = 0; j < gData->regexp->parenCount; j++)
            x->parens[j].index = -1;
        result = executeREBytecode(gData, x, true);
        if (!gData->ok || result)
            return result;
        gData->backTrackSP = gData->backTrackStack;
        gData->cursz = 0;
        gData->stateStackTop = 0;
        cp2 = cp + gData->skipped;
    }
    return NULL;
}


static REMatchState *
initMatch(REGlobalData *gData, JS2RegExp *re)
{
    REMatchState *result;
    uintN i;
    
    gData->maxBackTrack = INITIAL_BACKTRACK;
    gData->backTrackStack = (REBackTrackData *)malloc(INITIAL_BACKTRACK);

    if (!gData->backTrackStack)
        return NULL;
    gData->backTrackSP = gData->backTrackStack;
    gData->cursz = 0;


    gData->maxStateStack = INITIAL_STATESTACK;
    gData->stateStack = (REProgState *)malloc(sizeof(REProgState) * INITIAL_STATESTACK);
    if (!gData->stateStack)
        return NULL;
    gData->stateStackTop = 0;

    gData->regexp = re;
    gData->ok = JS_TRUE;

    result = (REMatchState *)malloc(sizeof(REMatchState) 
                               + (re->parenCount - 1) * sizeof(RECapture));
    if (!result)
        return NULL;

    for (i = 0; i < re->classCount; i++)
        if (!re->classList[i].converted)
            if (!processCharSet(gData, &re->classList[i]))
                return NULL;

    return result;
}

/*
 * Call the recursive matcher to do the real work.  Return null on mismatch.
 * On match, return the completed MatchResult structure.
 */
REMatchResult *REExecute(JS2Metadata *meta, JS2RegExp *re, const jschar *str, uint32 index, uint32 length, bool globalMultiline)
{
    REGlobalData gData;
    REMatchState *x, *result;
    const jschar *cp;
    uint32 start, p;
    REMatchResult *returnValue = NULL;

    start = index;
    if (start > length)
        start = length;
    cp = JSSTRING_CHARS(str);
    gData.cpbegin = cp;
    gData.cpend = cp + length;
    cp += start;
    gData.start = start;
    gData.skipped = 0;

    x = initMatch(&gData, re);
    gData.globalMultiline = globalMultiline;
    if (!x)
        return JS_FALSE;
    x->cp = cp;

    result = MatchRegExp(&gData, x);
    if (!gData.ok) 
        goto out;
    if (!result)
        goto out;

    returnValue = (REMatchResult *)malloc(sizeof(REMatchResult) + (re->parenCount - 1) * sizeof(RECapture));
    returnValue->startIndex = gData.start + gData.skipped;
    returnValue->endIndex = result->cp - str;
    returnValue->parenCount = re->parenCount;
    for (p = 0; p < re->parenCount; p++) {
        returnValue->parens[p] = result->parens[p];
    }
out:
    free(x);
	free(gData.stateStack);
	free(gData.backTrackStack);
    return returnValue;
}

REMatchResult *REMatch(JS2Metadata *meta, JS2RegExp *re, const jschar *str, uint32 length)
{
    REGlobalData gData;
    REMatchState *x, *result;
    const jschar *cp;
    uint32 j, p;
	REMatchResult *returnValue = NULL;

    cp = JSSTRING_CHARS(str);
    gData.cpbegin = cp;
    gData.cpend = cp + length;
    gData.start = 0;
    gData.skipped = 0;

    x = initMatch(&gData, re);
    gData.globalMultiline = false;
    if (!x)
        return JS_FALSE;
    x->cp = cp;

    for (j = 0; j < re->parenCount; j++)
        x->parens[j].index = -1;
    result = executeREBytecode(&gData, x, false);
    if (!gData.ok) 
        goto out;
    if (!result)
        goto out;

    returnValue = (REMatchResult *)malloc(sizeof(REMatchResult) + (re->parenCount - 1) * sizeof(RECapture));
    returnValue->startIndex = gData.skipped;
    returnValue->endIndex = result->cp - str;
    returnValue->parenCount = re->parenCount;
    for (p = 0; p < re->parenCount; p++) {
        returnValue->parens[p] = result->parens[p];
    }
out:
    free(x);
	free(gData.stateStack);
	free(gData.backTrackStack);
    return returnValue;
}

// Compile the flag source and build a flag bit set. Return true/false for success/failure
bool parseFlags(JS2Metadata *meta, const jschar *flagStr, uint32 length, uint32 *flags)
{
    uint32 i;
    *flags = 0;
    for (i = 0; i < length; i++) {
        switch (flagStr[i]) {
        case 'g':
            *flags |= JSREG_GLOB; break;
        case 'i':
            *flags |= JSREG_FOLD; break;
        case 'm':
            *flags |= JSREG_MULTILINE; break;
        default:
            return false;
        }
    }
    return true;
}

#define JS_HOWMANY(x,y) (((x)+(y)-1)/(y))
#define JS_ROUNDUP(x,y) (JS_HOWMANY(x,y)*(y))

// Compile the source re, return NULL for failure (error functions called)
JS2RegExp *RECompile(JS2Metadata *meta, const jschar *str, uint32 length, uint32 flags, bool flat)
{    
    JS2RegExp *re;
    CompilerState state;
    size_t resize;
    jsbytecode *endPC;
    uint32 i;
    size_t len;

    re = NULL;
    state.meta = meta;
    state.reNodePool = new Pool<RENode>(32);
    state.strict = false;
    state.cpbegin = state.cp = JSSTRING_CHARS(str);
    state.cpend = state.cp + length;
    state.flags = flags;
    state.parenCount = 0;
    state.classCount = 0;
    state.progLength = 0;
    state.treeDepth = 0;
    for (i = 0; i < CLASS_CACHE_SIZE; i++)
        state.classCache[i].start = NULL;

    len = length;
    if (len != 0 && flat) {
        state.result = NewRENode(&state, REOP_FLAT);
        state.result->u.flat.chr = *state.cpbegin;
        state.result->u.flat.length = length;                    
        state.result->kid = (void *)(state.cpbegin);
        state.progLength += 5;
    }
    else
        if (!parseRegExp(&state))
            goto out;

    resize = sizeof *re + state.progLength + 1;
    re = (JS2RegExp *) malloc(JS_ROUNDUP(resize, sizeof(uint32)));
    if (!re)
        goto out;

    re->classCount = state.classCount;
    if (state.classCount) {
        re->classList = (RECharSet *)malloc(sizeof(RECharSet)
                                                    * state.classCount);
        if (!re->classList) 
            goto out;
    }
    else
        re->classList = NULL;
    endPC = emitREBytecode(&state, re, state.treeDepth, re->program, state.result);
    if (!endPC) {
        re = NULL;
        goto out;
    }
    *endPC++ = REOP_END;
    JS_ASSERT(endPC <= (re->program + (state.progLength + 1)));

    re->parenCount = state.parenCount;
    re->flags = flags;
    re->source = str;

out:
    delete state.reNodePool;
    return re;
}

}   // namespace MetaData
}   // namespace JavaScript

