/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * JS regular expressions, after Perl.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscan.h"
#include "jsstr.h"

#ifdef XP_MAC
#include <MacMemory.h>
#endif

#if JS_HAS_REGEXPS

/* Note : contiguity of 'simple opcodes' is important for SimpleMatch() */
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

#define REOP_IS_SIMPLE(op)  ((unsigned)((op) - REOP_SIMPLE_START) <           \
                             (unsigned)REOP_SIMPLE_END)

struct RENode {
    REOp            op;         /* r.e. op bytecode */
    RENode          *next;      /* next in concatenation order */
    void            *kid;       /* first operand */
    union {
        void        *kid2;      /* second operand */
        jsint       num;        /* could be a number */
        uint16      parenIndex; /* or a parenthesis index */
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

#define RE_IS_LETTER(c)     (((c >= 'A') && (c <= 'Z')) ||                    \
                             ((c >= 'a') && (c <= 'z')) )
#define RE_IS_LINE_TERM(c)  ((c == '\n') || (c == '\r') ||                    \
                             (c == LINE_SEPARATOR) || (c == PARA_SEPARATOR))

#define CLASS_CACHE_SIZE    4

typedef struct CompilerState {
    JSContext       *context;
    JSTokenStream   *tokenStream; /* For reporting errors */
    const jschar    *cpbegin;
    const jschar    *cpend;
    const jschar    *cp;
    uint16          flags;
    uint16          parenCount;
    uint16          classCount;   /* number of [] encountered */
    uint16          treeDepth;    /* maximum depth of parse tree */
    size_t          progLength;   /* estimated bytecode length */
    RENode          *result;
    struct {
        const jschar *start;        /* small cache of class strings */
        uint16 length;              /* since they're often the same */
        uint16 index;
    } classCache[CLASS_CACHE_SIZE];
} CompilerState;

typedef struct RECapture {
    int32 index;               /* start of contents, -1 for empty  */
    uint16 length;             /* length of capture */
} RECapture;

typedef struct REMatchState {
    const jschar *cp;
    RECapture parens[1];      /* first of 're->parenCount' captures,
                               * allocated at end of this struct.
                               */
} REMatchState;

struct REBackTrackData;

typedef struct REProgState {
    jsbytecode *continue_pc;        /* current continuation data */
    jsbytecode continue_op;
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
    jsbytecode backtrack_op;
    const jschar *cp;               /* index in text of match at backtrack */
    uint16 parenIndex;              /* start index of saved paren contents */
    uint16 parenCount;              /* # of saved paren contents */
    uint16 saveStateStackTop;       /* number of parent states */
    /* saved parent states follow */
    /* saved paren contents follow */
} REBackTrackData;

#define INITIAL_STATESTACK (100)
#define INITIAL_BACKTRACK (8000)

typedef struct REGlobalData {
    JSContext *cx;
    JSRegExp *regexp;               /* the RE in execution */
    JSBool ok;                      /* runtime error (out_of_memory only?) */
    size_t start;                   /* offset to start at */
    ptrdiff_t skipped;              /* chars skipped anchoring this r.e. */
    const jschar *cpbegin, *cpend;  /* text base address and limit */

    REProgState *stateStack;        /* stack of state of current parents */
    uint16 stateStackTop;
    uint16 stateStackLimit;

    REBackTrackData *backTrackStack;/* stack of matched-so-far positions */
    REBackTrackData *backTrackSP;
    size_t backTrackStackSize;
    size_t cursz;                   /* size of current stack entry */

    JSArenaPool pool;               /* I don't understand but it's faster to
                                     * use this than to malloc/free the three
                                     * items that are allocated from this pool
                                     */

} REGlobalData;


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
upcase(jschar ch)
{
    jschar cu = JS_TOUPPER(ch);
    if (ch >= 128 && cu < 128)
        return ch;
    return cu;
}

static jschar
downcase(jschar ch)
{
    jschar cl = JS_TOLOWER(ch);
    if (cl >= 128 && ch < 128)
        return ch;
    return cl;
}

/* Construct and initialize an RENode, returning NULL for out-of-memory */
static RENode *
NewRENode(CompilerState *state, REOp op)
{
    JSContext *cx;
    RENode *ren;

    cx = state->context;
    JS_ARENA_ALLOCATE_CAST(ren, RENode *, &cx->tempPool, sizeof *ren);
    if (!ren) {
        JS_ReportOutOfMemory(cx);
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
ProcessOp(CompilerState *state, REOpData *opData, RENode **operandStack, intN operandSP)
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
        if (((RENode *) result->kid)->op == REOP_FLAT &&
            ((RENode *) result->u.kid2)->op == REOP_FLAT &&
            (state->flags & JSREG_FOLD) == 0) {
            result->op = REOP_ALTPREREQ;
            result->u.altprereq.ch1 = ((RENode *) result->kid)->u.flat.chr;
            result->u.altprereq.ch2 = ((RENode *) result->u.kid2)->u.flat.chr;
            /* ALTPREREQ, <end>, uch1, uch2, <next>, ...,
                                            JUMP, <end> ... ENDALT */
            state->progLength += 13;
        }
        else
        if (((RENode *) result->kid)->op == REOP_CLASS &&
            ((RENode *) result->kid)->u.ucclass.index < 256 &&
            ((RENode *) result->u.kid2)->op == REOP_FLAT &&
            (state->flags & JSREG_FOLD) == 0) {
            result->op = REOP_ALTPREREQ2;
            result->u.altprereq.ch1 = ((RENode *) result->u.kid2)->u.flat.chr;
            result->u.altprereq.ch2 = ((RENode *) result->kid)->u.ucclass.index;
            /* ALTPREREQ2, <end>, uch1, uch2, <next>, ...,
                                            JUMP, <end> ... ENDALT */
            state->progLength += 13;
        }
        else
        if (((RENode *) result->kid)->op == REOP_FLAT &&
            ((RENode *) result->u.kid2)->op == REOP_CLASS &&
            ((RENode *) result->u.kid2)->u.ucclass.index < 256 &&
            (state->flags & JSREG_FOLD) == 0) {
            result->op = REOP_ALTPREREQ2;
            result->u.altprereq.ch1 = ((RENode *) result->kid)->u.flat.chr;
            result->u.altprereq.ch2 =
                ((RENode *) result->u.kid2)->u.ucclass.index;
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
        js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                    NULL, JSREPORT_ERROR,
                                    JSMSG_MISSING_PAREN, opData->errPos);
        return JS_FALSE;
    default:;
    }
    return JS_TRUE;
}

/*
 * Parser forward declarations.
 */
static JSBool ParseTerm(CompilerState *state);
static JSBool ParseQuantifier(CompilerState *state);

/*
 * Top-down regular expression grammar, based closely on Perl4.
 *
 *  regexp:     altern                  A regular expression is one or more
 *              altern '|' regexp       alternatives separated by vertical bar.
 */
#define INITIAL_STACK_SIZE  128

static JSBool
ParseRegExp(CompilerState *state)
{
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
        return (state->result != NULL);
    }

    operatorStack = (REOpData *)
        JS_malloc(state->context, sizeof(REOpData) * operatorStackSize);
    if (!operatorStack)
        return JS_FALSE;

    operandStack = (RENode **)
        JS_malloc(state->context, sizeof(RENode *) * operandStackSize);
    if (!operandStack)
        goto out;

    while (JS_TRUE) {
        parenIndex = state->parenCount;
        if (state->cp == state->cpend) {
            /*
             * If we are at the end of the regexp and we're short one or more
             * operands, the regexp must have the form /x|/ or some such, with
             * left parentheses making us short more than one operand.
             */
            if (operatorSP >= operandSP) {
                operand = NewRENode(state, REOP_EMPTY);
                if (!operand)
                    goto out;
                goto pushOperand;
            }
        } else {
            switch (*state->cp) {
                /* balance '(' */
            case '(':           /* balance ')' */
                ++state->cp;
                if (state->cp + 1 < state->cpend &&
                    *state->cp == '?' &&
                    (state->cp[1] == '=' ||
                     state->cp[1] == '!' ||
                     state->cp[1] == ':')) {
                    switch (state->cp[1]) {
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
                    default:
                        op = REOP_LPARENNON;
                        break;
                    }
                    state->cp += 2;
                } else {
                    op = REOP_LPAREN;
                    /* LPAREN, <index>, ... RPAREN, <index> */
                    state->progLength += 6;
                    state->parenCount++;
                    if (state->parenCount == 65535) {
                        js_ReportCompileErrorNumber(state->context,
                                                    state->tokenStream,
                                                    NULL, JSREPORT_ERROR,
                                                    JSMSG_TOO_MANY_PARENS);
                        goto out;
                    }
                }
                goto pushOperator;
            case ')':
                /* If there's not a stacked open parenthesis, throw
                 * a syntax error.
                 */
                for (i = operatorSP - 1; ; i--) {
                    if (i < 0) {
                        js_ReportCompileErrorNumber(state->context,
                                                    state->tokenStream,
                                                    NULL, JSREPORT_ERROR,
                                                    JSMSG_UNMATCHED_RIGHT_PAREN);
                        goto out;
                    }
                    if (operatorStack[i].op == REOP_ASSERT ||
                        operatorStack[i].op == REOP_ASSERT_NOT ||
                        operatorStack[i].op == REOP_LPARENNON ||
                        operatorStack[i].op == REOP_LPAREN) {
                        break;
                    }
                }
                /* fall thru... */
            case '|':
                /* Expected an operand before these, so make an empty one */
                operand = NewRENode(state, REOP_EMPTY);
                if (!operand)
                    goto out;
                goto pushOperand;
            default:
                if (!ParseTerm(state))
                    goto out;
                operand = state->result;
pushOperand:
                if (operandSP == operandStackSize) {
                    operandStackSize += operandStackSize;
                    operandStack =
                      (RENode **)JS_realloc(state->context, operandStack,
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
                if (!ProcessOp(state, &operatorStack[operatorSP],
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
            while (operatorSP &&
                   operatorStack[operatorSP - 1].op == REOP_CONCAT) {
                --operatorSP;
                if (!ProcessOp(state, &operatorStack[operatorSP],
                               operandStack, operandSP))
                    goto out;
                --operandSP;
            }
            op = REOP_ALT;
            goto pushOperator;

        case ')':
            /* If there's not a stacked open parenthesis,we
             * accept the close as a flat.
             */
            for (i = operatorSP - 1; ; i--) {
                if (i < 0) {
                    js_ReportCompileErrorNumber(state->context,
                                                state->tokenStream,
                                                NULL, JSREPORT_ERROR,
                                                JSMSG_UNMATCHED_RIGHT_PAREN);
                    goto out;
                }
                if (operatorStack[i].op == REOP_ASSERT ||
                    operatorStack[i].op == REOP_ASSERT_NOT ||
                    operatorStack[i].op == REOP_LPARENNON ||
                    operatorStack[i].op == REOP_LPAREN) {
                    break;
                }
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
                    operand->u.parenIndex =
                        operatorStack[operatorSP].parenIndex;
                    JS_ASSERT(operandSP);
                    operand->kid = operandStack[operandSP - 1];
                    operandStack[operandSP - 1] = operand;
                    ++state->treeDepth;
                    /* fall thru... */
                case REOP_LPARENNON:
                    state->result = operandStack[operandSP - 1];
                    if (!ParseQuantifier(state))
                        goto out;
                    operandStack[operandSP - 1] = state->result;
                    goto restartOperator;
                default:
                    if (!ProcessOp(state, &operatorStack[operatorSP],
                                   operandStack, operandSP))
                        goto out;
                    --operandSP;
                    break;
                }
            }
            break;
        case '+':
        case '*':
        case '?':
        case '{':
            js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                        NULL, JSREPORT_ERROR,
                                        JSMSG_BAD_QUANTIFIER, state->cp);
            result = JS_FALSE;
            goto out;
        default:
            /* Anything else is the start of the next term */
            op = REOP_CONCAT;
pushOperator:
            if (operatorSP == operatorStackSize) {
                operatorStackSize += operatorStackSize;
                operatorStack =
                    (REOpData *)JS_realloc(state->context, operatorStack,
                                           sizeof(REOpData) * operatorStackSize);
                if (!operatorStack)
                    goto out;
            }
            operatorStack[operatorSP].op = op;
            operatorStack[operatorSP].errPos = state->cp;
            operatorStack[operatorSP++].parenIndex = parenIndex;
            break;
        }
    }
out:
    if (operatorStack)
        JS_free(state->context, operatorStack);
    if (operandStack)
        JS_free(state->context, operandStack);
    return result;
}

/*
 * Hack two bits in CompilerState.flags, for use within FindParenCount to flag
 * its being on the stack, and to propagate errors to its callers.
 */
#define JSREG_FIND_PAREN_COUNT  0x8000
#define JSREG_FIND_PAREN_ERROR  0x4000

/*
 * Magic return value from FindParenCount and GetDecimalValue, to indicate
 * overflow beyond GetDecimalValue's max parameter, or a computed maximum if
 * its findMax parameter is non-null.
 */
#define OVERFLOW_VALUE          ((uintN)-1)

static uintN
FindParenCount(CompilerState *state)
{
    CompilerState temp;
    int i;

    if (state->flags & JSREG_FIND_PAREN_COUNT)
        return OVERFLOW_VALUE;

    /*
     * Copy state into temp, flag it so we never report an invalid backref,
     * and reset its members to parse the entire regexp.  This is obviously
     * suboptimal, but GetDecimalValue calls us only if a backref appears to
     * refer to a forward parenthetical, which is rare.
     */
    temp = *state;
    temp.flags |= JSREG_FIND_PAREN_COUNT;
    temp.cp = temp.cpbegin;
    temp.parenCount = 0;
    temp.classCount = 0;
    temp.progLength = 0;
    temp.treeDepth = 0;
    for (i = 0; i < CLASS_CACHE_SIZE; i++)
        temp.classCache[i].start = NULL;

    if (!ParseRegExp(&temp)) {
        state->flags |= JSREG_FIND_PAREN_ERROR;
        return OVERFLOW_VALUE;
    }
    return temp.parenCount;
}

/*
 * Extract and return a decimal value at state->cp.  The initial character c
 * has already been read.  Return OVERFLOW_VALUE if the result exceeds max.
 * Callers who pass a non-null findMax should test JSREG_FIND_PAREN_ERROR in
 * state->flags to discover whether an error occurred under findMax.
 */
static uintN
GetDecimalValue(jschar c, uintN max, uintN (*findMax)(CompilerState *state),
                CompilerState *state)
{
    uintN value = JS7_UNDEC(c);
    JSBool overflow = (value > max && (!findMax || value > findMax(state)));

    /* The following restriction allows simpler overflow checks. */
    JS_ASSERT(max <= ((uintN)-1 - 9) / 10);
    while (state->cp < state->cpend) {
        c = *state->cp;
        if (!JS7_ISDEC(c))
            break;
        value = 10 * value + JS7_UNDEC(c);
        if (!overflow && value > max && (!findMax || value > findMax(state)))
            overflow = JS_TRUE;
        ++state->cp;
    }
    return overflow ? OVERFLOW_VALUE : value;
}

/*
 * Calculate the total size of the bitmap required for a class expression.
 */
static JSBool
CalculateBitmapSize(CompilerState *state, RENode *target, const jschar *src,
                    const jschar *end)
{
    uintN max = 0;
    JSBool inRange = JS_FALSE;
    jschar c, rangeStart = 0;
    uintN n, digit, nDigits, i;

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
                if (src + 1 < end && RE_IS_LETTER(src[1]))
                    localMax = (jschar) (*src++ & 0x1F);
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
                        src -= i + 1;
                        n = '\\';
                        break;
                    }
                    n = (n << 4) | digit;
                }
                localMax = n;
                break;
            case 'd':
                if (inRange) {
                    JS_ReportErrorNumber(state->context,
                                         js_GetErrorMessage, NULL,
                                         JSMSG_BAD_CLASS_RANGE);
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
                    JS_ReportErrorNumber(state->context,
                                         js_GetErrorMessage, NULL,
                                         JSMSG_BAD_CLASS_RANGE);
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
                JS_ReportErrorNumber(state->context,
                                     js_GetErrorMessage, NULL,
                                     JSMSG_BAD_CLASS_RANGE);
                return JS_FALSE;
            }
            inRange = JS_FALSE;
        } else {
            if (src < end - 1) {
                if (*src == '-') {
                    ++src;
                    inRange = JS_TRUE;
                    rangeStart = (jschar)localMax;
                    continue;
                }
            }
        }
        if (state->flags & JSREG_FOLD) {
            c = JS_MAX(upcase((jschar)localMax), downcase((jschar)localMax));
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
ParseTerm(CompilerState *state)
{
    jschar c = *state->cp++;
    uintN nDigits;
    uintN num, tmp, n, i;
    const jschar *termStart;

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
            js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                        NULL, JSREPORT_ERROR,
                                        JSMSG_TRAILING_SLASH);
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
            if (JS_HAS_STRICT_OPTION(state->context)) {
                if (!js_ReportCompileErrorNumber(state->context,
                                                 state->tokenStream,
                                                 NULL,
                                                 JSREPORT_WARNING |
                                                 JSREPORT_STRICT,
                                                 JSMSG_INVALID_BACKREF)) {
                    return JS_FALSE;
                }
                c = 0;
            } else {
     doOctal:
                num = 0;
                while (state->cp < state->cpend) {
                    c = *state->cp;
                    if (c < '0' || '7' < c)
                        break;
                    state->cp++;
                    tmp = 8 * num + (uintN)JS7_UNDEC(c);
                    if (tmp > 0377)
                        break;
                    num = tmp;
                }
                c = (jschar)num;
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
            num = GetDecimalValue(c, state->parenCount, FindParenCount, state);
            if (state->flags & JSREG_FIND_PAREN_ERROR)
                return JS_FALSE;
            if (num == OVERFLOW_VALUE) {
                if (!JS_HAS_STRICT_OPTION(state->context)) {
                    state->cp = termStart;
                    goto doOctal;
                }
                if (!js_ReportCompileErrorNumber(state->context,
                                                 state->tokenStream,
                                                 NULL,
                                                 JSREPORT_WARNING |
                                                 JSREPORT_STRICT,
                                                 (c >= '8')
                                                 ? JSMSG_INVALID_BACKREF
                                                 : JSMSG_BAD_BACKREF)) {
                    return JS_FALSE;
                }
                num = 0x10000;
            }
            JS_ASSERT(1 <= num && num <= 0x10000);
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
            if (state->cp + 1 < state->cpend && RE_IS_LETTER(state->cp[1])) {
                c = (jschar) (*state->cp++ & 0x1F);
            } else {
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
            for (i = 0; i < nDigits && state->cp < state->cpend; i++) {
                uintN digit;
                c = *state->cp++;
                if (!isASCIIHexDigit(c, &digit)) {
                    /*
                     *  back off to accepting the original
                     *  'u' or 'x' as a literal
                     */
                    state->cp -= i + 2;
                    n = *state->cp++;
                    break;
                }
                n = (n << 4) | digit;
            }
            c = (jschar) n;
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
            state->result->kid = (void *) (state->cp - 1);
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
                js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                            NULL, JSREPORT_ERROR,
                                            JSMSG_UNTERM_CLASS, termStart);
                return JS_FALSE;
            }
            if (*state->cp == '\\') {
                state->cp++;
            } else {
                if (*state->cp == ']') {
                    state->result->u.ucclass.kidlen = state->cp - termStart;
                    break;
                }
            }
            state->cp++;
        }
        for (i = 0; i < CLASS_CACHE_SIZE; i++) {
            if (!state->classCache[i].start) {
                state->classCache[i].start = termStart;
                state->classCache[i].length = state->result->u.ucclass.kidlen;
                state->classCache[i].index = state->classCount;
                break;
            }
            if (state->classCache[i].length ==
                state->result->u.ucclass.kidlen) {
                for (n = 0; ; n++) {
                    if (n == state->classCache[i].length) {
                        state->result->u.ucclass.index =
                            state->classCache[i].index;
                        goto claim;
                    }
                    if (state->classCache[i].start[n] != termStart[n])
                        break;
                }
            }
        }
        state->result->u.ucclass.index = state->classCount++;
    claim:
        /*
         * Call CalculateBitmapSize now as we want any errors it finds
         * to be reported during the parse phase, not at execution.
         */
        if (!CalculateBitmapSize(state, state->result, termStart, state->cp++))
            return JS_FALSE;
        state->progLength += 3; /* CLASS, <index> */
        break;

    case '.':
        state->result = NewRENode(state, REOP_DOT);
        goto doSimple;
    case '*':
    case '+':
    case '?':
        js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                    NULL, JSREPORT_ERROR,
                                    JSMSG_BAD_QUANTIFIER, state->cp - 1);
        return JS_FALSE;
    default:
        state->result = NewRENode(state, REOP_FLAT);
        if (!state->result)
            return JS_FALSE;
        state->result->u.flat.chr = c;
        state->result->u.flat.length = 1;
        state->result->kid = (void *) (state->cp - 1);
        state->progLength += 3;
        break;
    }
    return ParseQuantifier(state);
}

static JSBool
ParseQuantifier(CompilerState *state)
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
            state->result->u.range.max = (uint16)-1;
            /* <PLUS>, <next> ... <ENDCHILD> */
            state->progLength += 4;
            goto quantifier;
        case '*':
            state->result = NewRENode(state, REOP_QUANT);
            if (!state->result)
                return JS_FALSE;
            state->result->u.range.min = 0;
            state->result->u.range.max = (uint16)-1;
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
                intN err;
                uintN min, max;
                jschar c;
                const jschar *errp = state->cp++;

                c = *state->cp;
                if (JS7_ISDEC(c)) {
                    ++state->cp;
                    min = GetDecimalValue(c, 0xFFFF, NULL, state);
                    c = *state->cp;

                    if (min == OVERFLOW_VALUE) {
                        err = JSMSG_MIN_TOO_BIG;
                        goto quantError;
                    }
                    if (c == ',') {
                        c = *++state->cp;
                        if (JS7_ISDEC(c)) {
                            ++state->cp;
                            max = GetDecimalValue(c, 0xFFFF, NULL, state);
                            c = *state->cp;
                            if (max == OVERFLOW_VALUE) {
                                err = JSMSG_MAX_TOO_BIG;
                                goto quantError;
                            }
                            if (min > max) {
                                err = JSMSG_OUT_OF_ORDER;
                                goto quantError;
                            }
                        } else {
                            max = (uintN)-1;
                        }
                    } else {
                        max = min;
                    }
                    if (c == '}') {
                        state->result = NewRENode(state, REOP_QUANT);
                        if (!state->result)
                            return JS_FALSE;
                        state->result->u.range.min = min;
                        state->result->u.range.max = max;
                        /* QUANT, <min>, <max>, <next> ... <ENDCHILD> */
                        state->progLength += 8;
                        goto quantifier;
                    }
                }
                state->cp = errp;
                return JS_TRUE;
quantError:
                js_ReportCompileErrorNumber(state->context,
                                            state->tokenStream,
                                            NULL, JSREPORT_ERROR,
                                            err, errp);
                return JS_FALSE;
            }
        }
    }
    return JS_TRUE;

quantifier:
    ++state->treeDepth;
    ++state->cp;
    state->result->kid = term;
    if (state->cp < state->cpend && *state->cp == '?') {
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
EmitREBytecode(CompilerState *state, JSRegExp *re, intN treeDepth,
               jsbytecode *pc, RENode *t)
{
    ptrdiff_t diff;
    RECharSet *charSet;
    EmitStateStackEntry *emitStateSP, *emitStateStack = NULL;
    REOp op;

    if (treeDepth) {
        emitStateStack =
            (EmitStateStackEntry *)JS_malloc(state->context,
                                             sizeof(EmitStateStackEntry) *
                                             treeDepth);
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
            t = (RENode *) t->kid;
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
            t = (RENode *) t->u.kid2;
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
            t = (RENode *) t->kid;
            op = t->op;
            continue;

        case REOP_FLAT:
            /*
             * Consecutize FLAT's if possible.
             */
            if (t->kid) {
                while (t->next &&
                       t->next->op == REOP_FLAT &&
                       (jschar*)t->kid + t->u.flat.length ==
                       (jschar*)t->next->kid) {
                    t->u.flat.length += t->next->u.flat.length;
                    t->next = t->next->next;
                }
            }
            if (t->kid && (t->u.flat.length > 1)) {
                if (state->flags & JSREG_FOLD)
                    pc[-1] = REOP_FLATi;
                else
                    pc[-1] = REOP_FLAT;
                SET_ARG(pc, (jschar *)t->kid - state->cpbegin);
                pc += ARG_LEN;
                SET_ARG(pc, t->u.flat.length);
                pc += ARG_LEN;
            } else if (t->u.flat.chr < 256) {
                if (state->flags & JSREG_FOLD)
                    pc[-1] = REOP_FLAT1i;
                else
                    pc[-1] = REOP_FLAT1;
                *pc++ = (jsbytecode) t->u.flat.chr;
            } else {
                if (state->flags & JSREG_FOLD)
                    pc[-1] = REOP_UCFLAT1i;
                else
                    pc[-1] = REOP_UCFLAT1;
                SET_ARG(pc, t->u.flat.chr);
                pc += ARG_LEN;
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
            t = (RENode *) t->kid;
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
            t = (RENode *) t->kid;
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
            t = (RENode *) t->kid;
            op = t->op;
            continue;
        case REOP_QUANT:
            JS_ASSERT(emitStateSP);
            if (t->u.range.min == 0 && t->u.range.max == (uint16)-1) {
                pc[-1] = (t->u.range.greedy) ? REOP_STAR : REOP_MINIMALSTAR;
            } else if (t->u.range.min == 0 && t->u.range.max == 1) {
                pc[-1] = (t->u.range.greedy) ? REOP_OPT : REOP_MINIMALOPT;
            } else if (t->u.range.min == 1 && t->u.range.max == (uint16) -1) {
                pc[-1] = (t->u.range.greedy) ? REOP_PLUS : REOP_MINIMALPLUS;
            } else {
                if (!t->u.range.greedy)
                    pc[-1] = REOP_MINIMALQUANT;
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
            t = (RENode *) t->kid;
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
        if (!t) {
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
        JS_free(state->context, emitStateStack);
    return pc;
}


JSRegExp *
js_NewRegExp(JSContext *cx, JSTokenStream *ts,
             JSString *str, uintN flags, JSBool flat)
{
    JSRegExp *re;
    void *mark;
    CompilerState state;
    size_t resize;
    jsbytecode *endPC;
    uintN i;
    size_t len;

    re = NULL;
    mark = JS_ARENA_MARK(&cx->tempPool);

    state.context = cx;
    state.tokenStream = ts;
    state.cpbegin = state.cp = JSSTRING_CHARS(str);
    state.cpend = state.cp + JSSTRING_LENGTH(str);
    state.flags = flags;
    state.parenCount = 0;
    state.classCount = 0;
    state.progLength = 0;
    state.treeDepth = 0;
    for (i = 0; i < CLASS_CACHE_SIZE; i++)
        state.classCache[i].start = NULL;

    len = JSSTRING_LENGTH(str);

    if (len != 0 && flat) {
        state.result = NewRENode(&state, REOP_FLAT);
        state.result->u.flat.chr = *state.cpbegin;
        state.result->u.flat.length = JSSTRING_LENGTH(str);
        state.result->kid = (void *) state.cpbegin;
        state.progLength += 5;
    } else {
        if (!ParseRegExp(&state))
            goto out;
    }
    resize = sizeof *re + state.progLength + 1;
    re = (JSRegExp *) JS_malloc(cx, JS_ROUNDUP(resize, sizeof(jsword)));
    if (!re)
        goto out;

    re->nrefs = 1;
    re->classCount = state.classCount;
    if (re->classCount) {
        re->classList = (RECharSet *)
            JS_malloc(cx, re->classCount * sizeof(RECharSet));
        if (!re->classList) {
            js_DestroyRegExp(cx, re);
            re = NULL;
            goto out;
        }
    } else {
        re->classList = NULL;
    }
    endPC = EmitREBytecode(&state, re, state.treeDepth, re->program, state.result);
    if (!endPC) {
        js_DestroyRegExp(cx, re);
        re = NULL;
        goto out;
    }
    *endPC++ = REOP_END;
    JS_ASSERT(endPC <= (re->program + (state.progLength + 1)));

    re->flags = flags;
    re->cloneIndex = 0;
    re->parenCount = state.parenCount;
    re->source = str;

out:
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    return re;
}

JSRegExp *
js_NewRegExpOpt(JSContext *cx, JSTokenStream *ts,
                JSString *str, JSString *opt, JSBool flat)
{
    uintN flags;
    jschar *s;
    size_t i, n;
    char charBuf[2];

    flags = 0;
    if (opt) {
        s = JSSTRING_CHARS(opt);
        for (i = 0, n = JSSTRING_LENGTH(opt); i < n; i++) {
            switch (s[i]) {
            case 'g':
                flags |= JSREG_GLOB;
                break;
            case 'i':
                flags |= JSREG_FOLD;
                break;
            case 'm':
                flags |= JSREG_MULTILINE;
                break;
            default:
                charBuf[0] = (char)s[i];
                charBuf[1] = '\0';
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_FLAG, charBuf);
                return NULL;
            }
        }
    }
    return js_NewRegExp(cx, ts, str, flags, flat);
}


#define HOLD_REGEXP(cx, re) JS_ATOMIC_INCREMENT(&(re)->nrefs)
#define DROP_REGEXP(cx, re) js_DestroyRegExp(cx, re)

/*
 * Save the current state of the match - the position in the input
 * text as well as the position in the bytecode. The state of any
 * parent expressions is also saved (preceding state).
 * Contents of parenCount parentheses from parenIndex are also saved.
 */
static REBackTrackData *
PushBackTrackState(REGlobalData *gData, REOp op,
                   jsbytecode *target, REMatchState *x, const jschar *cp,
                   uintN parenIndex, intN parenCount)
{
    intN i;
    REBackTrackData *result =
        (REBackTrackData *) ((char *)gData->backTrackSP + gData->cursz);

    size_t sz = sizeof(REBackTrackData) +
                gData->stateStackTop * sizeof(REProgState) +
                parenCount * sizeof(RECapture);

    ptrdiff_t btsize = gData->backTrackStackSize;
    ptrdiff_t btincr = ((char *)result + sz) -
                       ((char *)gData->backTrackStack + btsize);

    if (btincr > 0) {
        ptrdiff_t offset = (char *)result - (char *)gData->backTrackStack;

        btincr = JS_ROUNDUP(btincr, btsize);
        JS_ARENA_GROW_CAST(gData->backTrackStack, REBackTrackData *,
                           &gData->pool, btsize, btincr);
        if (!gData->backTrackStack)
            return NULL;
        gData->backTrackStackSize = btsize + btincr;
        result = (REBackTrackData *) ((char *)gData->backTrackStack + offset);
    }
    gData->backTrackSP = result;
    result->sz = gData->cursz;
    gData->cursz = sz;

    result->backtrack_op = op;
    result->backtrack_pc = target;
    result->cp = cp;
    result->parenCount = parenCount;

    result->saveStateStackTop = gData->stateStackTop;
    JS_ASSERT(gData->stateStackTop);
    memcpy(result + 1, gData->stateStack,
           sizeof(REProgState) * result->saveStateStackTop);

    /* FIXME: parenCount should be uintN */
    JS_ASSERT(parenCount >= 0);
    if (parenCount > 0) {
        result->parenIndex = parenIndex;
        memcpy((char *)(result + 1) +
               sizeof(REProgState) * result->saveStateStackTop,
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
#if 0
static REMatchState *
FlatNMatcher(REGlobalData *gData, REMatchState *x, jschar *matchChars,
             intN length)
{
    intN i;
    if (x->cp + length > gData->cpend)
        return NULL;
    for (i = 0; i < length; i++) {
        if (matchChars[i] != x->cp[i])
            return NULL;
    }
    x->cp += length;
    return x;
}
#endif

static REMatchState *
FlatNIMatcher(REGlobalData *gData, REMatchState *x, jschar *matchChars,
              intN length)
{
    intN i;
    if (x->cp + length > gData->cpend)
        return NULL;
    for (i = 0; i < length; i++) {
        if (upcase(matchChars[i]) != upcase(x->cp[i]))
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
BackrefMatcher(REGlobalData *gData, REMatchState *x, uintN parenIndex)
{
    uintN len;
    uintN i;
    const jschar *parenContent;
    RECapture *s = &x->parens[parenIndex];
    if (s->index == -1)
        return x;

    len = s->length;
    if (x->cp + len > gData->cpend)
        return NULL;

    parenContent = &gData->cpbegin[s->index];
    if (gData->regexp->flags & JSREG_FOLD) {
        for (i = 0; i < len; i++) {
            if (upcase(parenContent[i]) != upcase(x->cp[i]))
                return NULL;
        }
    } else {
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
AddCharacterToCharSet(RECharSet *cs, jschar c)
{
    uintN byteIndex = (uintN)(c >> 3);
    JS_ASSERT(c <= cs->length);
    cs->u.bits[byteIndex] |= 1 << (c & 0x7);
}


/* Add a character range, c1 to c2 (inclusive) to the RECharSet */
static void
AddCharacterRangeToCharSet(RECharSet *cs, jschar c1, jschar c2)
{
    uintN i;

    uintN byteIndex1 = (uintN)(c1 >> 3);
    uintN byteIndex2 = (uintN)(c2 >> 3);

    JS_ASSERT((c2 <= cs->length) && (c1 <= c2));

    c1 &= 0x7;
    c2 &= 0x7;

    if (byteIndex1 == byteIndex2) {
        cs->u.bits[byteIndex1] |= ((uint8)0xFF >> (7 - (c2 - c1))) << c1;
    } else {
        cs->u.bits[byteIndex1] |= 0xFF << c1;
        for (i = byteIndex1 + 1; i < byteIndex2; i++)
            cs->u.bits[i] = 0xFF;
        cs->u.bits[byteIndex2] |= (uint8)0xFF >> (7 - c2);
    }
}

/* Compile the source of the class into a RECharSet */
static JSBool
ProcessCharSet(REGlobalData *gData, RECharSet *charSet)
{
    const jschar *src = JSSTRING_CHARS(gData->regexp->source) +
                        charSet->u.src.startIndex;
    const jschar *end = src + charSet->u.src.length;

    JSBool inRange = JS_FALSE;
    jschar rangeStart = 0;
    uintN byteLength, n;
    jschar c, thisCh;
    intN nDigits, i;

    JS_ASSERT(!charSet->converted);
    charSet->converted = JS_TRUE;

    byteLength = (charSet->length >> 3) + 1;
    charSet->u.bits = (uint8 *)JS_malloc(gData->cx, byteLength);
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
                if (src + 1 < end && JS_ISWORD(src[1])) {
                    thisCh = (jschar)(*src++ & 0x1F);
                } else {
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
                        src -= i + 1;
                        n = '\\';
                        break;
                    }
                    n = (n << 4) | digit;
                }
                thisCh = (jschar)n;
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
                thisCh = (jschar)n;
                break;

            case 'd':
                AddCharacterRangeToCharSet(charSet, '0', '9');
                continue;   /* don't need range processing */
            case 'D':
                AddCharacterRangeToCharSet(charSet, 0, '0' - 1);
                AddCharacterRangeToCharSet(charSet,
                                           (jschar)('9' + 1),
                                           (jschar)charSet->length);
                continue;
            case 's':
                for (i = (intN)charSet->length; i >= 0; i--)
                    if (JS_ISSPACE(i))
                        AddCharacterToCharSet(charSet, (jschar)i);
                continue;
            case 'S':
                for (i = (intN)charSet->length; i >= 0; i--)
                    if (!JS_ISSPACE(i))
                        AddCharacterToCharSet(charSet, (jschar)i);
                continue;
            case 'w':
                for (i = (intN)charSet->length; i >= 0; i--)
                    if (JS_ISWORD(i))
                        AddCharacterToCharSet(charSet, (jschar)i);
                continue;
            case 'W':
                for (i = (intN)charSet->length; i >= 0; i--)
                    if (!JS_ISWORD(i))
                        AddCharacterToCharSet(charSet, (jschar)i);
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
                AddCharacterRangeToCharSet(charSet, upcase(rangeStart),
                                                    upcase(thisCh));
                AddCharacterRangeToCharSet(charSet, downcase(rangeStart),
                                                    downcase(thisCh));
            } else {
                AddCharacterRangeToCharSet(charSet, rangeStart, thisCh);
            }
            inRange = JS_FALSE;
        } else {
            if (gData->regexp->flags & JSREG_FOLD) {
                AddCharacterToCharSet(charSet, upcase(thisCh));
                AddCharacterToCharSet(charSet, downcase(thisCh));
            } else {
                AddCharacterToCharSet(charSet, thisCh);
            }
            if (src < end - 1) {
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
js_DestroyRegExp(JSContext *cx, JSRegExp *re)
{
    if (JS_ATOMIC_DECREMENT(&re->nrefs) == 0) {
        if (re->classList) {
            uintN i;
            for (i = 0; i < re->classCount; i++) {
                if (re->classList[i].converted)
                    JS_free(cx, re->classList[i].u.bits);
                re->classList[i].u.bits = NULL;
            }
            JS_free(cx, re->classList);
        }
        JS_free(cx, re);
    }
}

static JSBool
ReallocStateStack(REGlobalData *gData)
{
    uint16 limit = gData->stateStackLimit;
    size_t sz = sizeof(REProgState) * limit;

    JS_ARENA_GROW_CAST(gData->stateStack, REProgState *, &gData->pool, sz, sz);
    if (!gData->stateStack) {
        gData->ok = JS_FALSE;
        return JS_FALSE;
    }
    gData->stateStackLimit = limit + limit;
    return JS_TRUE;
}

#define PUSH_STATE_STACK(data)                                                \
    JS_BEGIN_MACRO                                                            \
        ++(data)->stateStackTop;                                              \
        if ((data)->stateStackTop == (data)->stateStackLimit &&               \
            !ReallocStateStack((data))) {                                     \
            return NULL;                                                      \
        }                                                                     \
    JS_END_MACRO

/*
 * Apply the current op against the given input to see if it's going to match
 * or fail. Return false if we don't get a match, true if we do and update the
 * state of the input and pc if the update flag is true.
 */
static REMatchState *
SimpleMatch(REGlobalData *gData, REMatchState *x, REOp op,
            jsbytecode **startpc, JSBool update)
{
    REMatchState *result = NULL;
    jschar matchCh;
    uintN parenIndex;
    intN offset, length, index;
    jsbytecode *pc = *startpc;  /* pc has already been incremented past op */
    jschar *source;
    const jschar *startcp = x->cp;
    jschar ch;
    RECharSet *charSet;

    switch (op) {
    case REOP_BOL:
        if (x->cp != gData->cpbegin) {
            if (!gData->cx->regExpStatics.multiline &&
                !(gData->regexp->flags & JSREG_MULTILINE)) {
                break;
            }
            if (!RE_IS_LINE_TERM(x->cp[-1]))
                break;
        }
        result = x;
        break;
    case REOP_EOL:
        if (x->cp != gData->cpend) {
            if (!gData->cx->regExpStatics.multiline &&
                !(gData->regexp->flags & JSREG_MULTILINE)) {
                break;
            }
            if (!RE_IS_LINE_TERM(*x->cp))
                break;
        }
        result = x;
        break;
    case REOP_WBDRY:
        if ((x->cp == gData->cpbegin || !JS_ISWORD(x->cp[-1])) ^
            !(x->cp != gData->cpend && JS_ISWORD(*x->cp))) {
            result = x;
        }
        break;
    case REOP_WNONBDRY:
        if ((x->cp == gData->cpbegin || !JS_ISWORD(x->cp[-1])) ^
            (x->cp != gData->cpend && JS_ISWORD(*x->cp))) {
            result = x;
        }
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
        result = BackrefMatcher(gData, x, parenIndex);
        break;
    case REOP_FLAT:
        offset = GET_ARG(pc);
        pc += ARG_LEN;
        length = GET_ARG(pc);
        pc += ARG_LEN;
        source = JSSTRING_CHARS(gData->regexp->source) + offset;
        if (x->cp + length <= gData->cpend) {
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
        if (x->cp != gData->cpend && *x->cp == matchCh) {
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
        result = FlatNIMatcher(gData, x, source + offset, length);
        break;
    case REOP_FLAT1i:
        matchCh = *pc++;
        if (x->cp != gData->cpend && upcase(*x->cp) == upcase(matchCh)) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_UCFLAT1:
        matchCh = GET_ARG(pc);
        pc += ARG_LEN;
        if (x->cp != gData->cpend && *x->cp == matchCh) {
            result = x;
            result->cp++;
        }
        break;
    case REOP_UCFLAT1i:
        matchCh = GET_ARG(pc);
        pc += ARG_LEN;
        if (x->cp != gData->cpend && upcase(*x->cp) == upcase(matchCh)) {
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
            index = ch >> 3;
            if (charSet->length != 0 &&
                ch <= charSet->length &&
                (charSet->u.bits[index] & (1 << (ch & 0x7)))) {
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
            index = ch >> 3;
            if (charSet->length == 0 ||
                ch > charSet->length ||
                !(charSet->u.bits[index] & (1 << (ch & 0x7)))) {
                result = x;
                result->cp++;
            }
        }
        break;
    default:
        JS_ASSERT(JS_FALSE);
    }
    if (result) {
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
ExecuteREBytecode(REGlobalData *gData, REMatchState *x)
{
    REMatchState *result = NULL;
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
    REOp op = (REOp) *pc++;

    /*
     * If the first node is a simple match, step the index into the string
     * until that match is made, or fail if it can't be found at all.
     */
    if (REOP_IS_SIMPLE(op)) {
        anchor = JS_FALSE;
        while (x->cp <= gData->cpend) {
            nextpc = pc;    /* reset back to start each time */
            result = SimpleMatch(gData, x, op, &nextpc, JS_TRUE);
            if (result) {
                anchor = JS_TRUE;
                x = result;
                pc = nextpc;    /* accept skip to next opcode */
                op = (REOp) *pc++;
                break;
            }
            gData->skipped++;
            x->cp++;
        }
        if (!anchor)
            return NULL;
    }

    while (JS_TRUE) {
        if (REOP_IS_SIMPLE(op)) {
            result = SimpleMatch(gData, x, op, &pc, JS_TRUE);
        } else {
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
                    if (*x->cp == matchCh2)
                        goto doAlt;

                    charSet = &gData->regexp->classList[k];
                    if (!charSet->converted)
                        if (!ProcessCharSet(gData, charSet))
                            return NULL;
                    matchCh1 = *x->cp;
                    k = matchCh1 >> 3;
                    if ((charSet->length == 0 ||
                         matchCh1 > charSet->length ||
                         !(charSet->u.bits[k] & (1 << (matchCh1 & 0x7)))) ^
                        charSet->sense) {
                        goto doAlt;
                    }
                }
                result = NULL;
                break;

            case REOP_ALTPREREQ:
                nextpc = pc + GET_OFFSET(pc);   /* start of next op */
                pc += ARG_LEN;
                matchCh1 = GET_ARG(pc);
                pc += ARG_LEN;
                matchCh2 = GET_ARG(pc);
                pc += ARG_LEN;
                if (x->cp == gData->cpend ||
                    (*x->cp != matchCh1 && *x->cp != matchCh2)) {
                    result = NULL;
                    break;
                }
                /* else false thru... */

            case REOP_ALT:
            doAlt:
                nextpc = pc + GET_OFFSET(pc);   /* start of next alternate */
                pc += ARG_LEN;                  /* start of this alternate */
                curState->parenSoFar = parenSoFar;
                PUSH_STATE_STACK(gData);
                op = (REOp) *pc++;
                startcp = x->cp;
                if (REOP_IS_SIMPLE(op)) {
                    if (!SimpleMatch(gData, x, op, &pc, JS_TRUE)) {
                        op = (REOp) *nextpc++;
                        pc = nextpc;
                        continue;
                    }
                    result = x;
                    op = (REOp) *pc++;
                }
                nextop = (REOp) *nextpc++;
                if (!PushBackTrackState(gData, nextop, nextpc, x, startcp, 0, 0))
                    return NULL;
                continue;

            /*
             * Occurs at (succesful) end of REOP_ALT,
             */
            case REOP_JUMP:
                --gData->stateStackTop;
                offset = GET_OFFSET(pc);
                pc += offset;
                op = (REOp) *pc++;
                continue;

            /*
             * Occurs at last (succesful) end of REOP_ALT,
             */
            case REOP_ENDALT:
                --gData->stateStackTop;
                op = (REOp) *pc++;
                continue;

            case REOP_LPAREN:
                parenIndex = GET_ARG(pc);
                if (parenIndex + 1 > parenSoFar)
                    parenSoFar = parenIndex + 1;
                pc += ARG_LEN;
                x->parens[parenIndex].index = x->cp - gData->cpbegin;
                x->parens[parenIndex].length = 0;
                op = (REOp) *pc++;
                continue;
            case REOP_RPAREN:
                parenIndex = GET_ARG(pc);
                pc += ARG_LEN;
                cap = &x->parens[parenIndex];
                cap->length = x->cp - (gData->cpbegin + cap->index);
                op = (REOp) *pc++;
                continue;

            case REOP_ASSERT:
                nextpc = pc + GET_OFFSET(pc);  /* start of term after ASSERT */
                pc += ARG_LEN;                 /* start of ASSERT child */
                op = (REOp) *pc++;
                if (REOP_IS_SIMPLE(op) &&
                    !SimpleMatch(gData, x, op, &pc, JS_FALSE)) {
                    result = NULL;
                    break;
                }
                curState->u.assertion.top =
                    (char *)gData->backTrackSP - (char *)gData->backTrackStack;
                curState->u.assertion.sz = gData->cursz;
                curState->index = x->cp - gData->cpbegin;
                curState->parenSoFar = parenSoFar;
                PUSH_STATE_STACK(gData);
                if (!PushBackTrackState(gData, REOP_ASSERTTEST,
                                        nextpc, x, x->cp, 0, 0)) {
                    return NULL;
                }
                continue;
            case REOP_ASSERT_NOT:
                nextpc = pc + GET_OFFSET(pc);
                pc += ARG_LEN;
                op = (REOp) *pc++;
                if (REOP_IS_SIMPLE(op) /* Note - fail to fail! */ &&
                    SimpleMatch(gData, x, op, &pc, JS_FALSE) &&
                    pc == nextpc) {
                    result = NULL;
                    break;
                }
                curState->u.assertion.top
                    = (char *)gData->backTrackSP -
                      (char *)gData->backTrackStack;
                curState->u.assertion.sz = gData->cursz;
                curState->index = x->cp - gData->cpbegin;
                curState->parenSoFar = parenSoFar;
                PUSH_STATE_STACK(gData);
                if (!PushBackTrackState(gData, REOP_ASSERTNOTTEST,
                                        nextpc, x, x->cp, 0, 0))
                    return NULL;
                continue;
            case REOP_ASSERTTEST:
                --gData->stateStackTop;
                --curState;
                x->cp = gData->cpbegin + curState->index;
                gData->backTrackSP =
                    (REBackTrackData *) ((char *)gData->backTrackStack +
                                         curState->u.assertion.top);
                gData->cursz = curState->u.assertion.sz;
                if (result)
                    result = x;
                break;
            case REOP_ASSERTNOTTEST:
                --gData->stateStackTop;
                --curState;
                x->cp = gData->cpbegin + curState->index;
                gData->backTrackSP =
                    (REBackTrackData *) ((char *)gData->backTrackStack +
                                         curState->u.assertion.top);
                gData->cursz = curState->u.assertion.sz;
                result = (!result) ? x : NULL;
                break;

            case REOP_END:
                if (x)
                    return x;
                break;

            case REOP_STAR:
                curState->u.quantifier.min = 0;
                curState->u.quantifier.max = (uint16)-1;
                goto quantcommon;
            case REOP_PLUS:
                curState->u.quantifier.min = 1;
                curState->u.quantifier.max = (uint16)-1;
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
                    op = (REOp) *pc++;
                    result = x;
                    continue;
                }
                /* Step over <next> */
                nextpc = pc + ARG_LEN;
                op = (REOp) *nextpc++;
                startcp = x->cp;
                if (REOP_IS_SIMPLE(op)) {
                    if (!SimpleMatch(gData, x, op, &nextpc, JS_TRUE)) {
                        if (curState->u.quantifier.min == 0)
                            result = x;
                        else
                            result = NULL;
                        pc = pc + GET_OFFSET(pc);
                        break;
                    }
                    op = (REOp) *nextpc++;
                    result = x;
                }
                curState->index = startcp - gData->cpbegin;
                curState->continue_op = REOP_REPEAT;
                curState->continue_pc = pc;
                curState->parenSoFar = parenSoFar;
                PUSH_STATE_STACK(gData);
                if (curState->u.quantifier.min == 0 &&
                    !PushBackTrackState(gData, REOP_REPEAT, pc, x, startcp,
                                        0, 0)) {
                    return NULL;
                }
                pc = nextpc;
                continue;

            case REOP_ENDCHILD: /* marks the end of a quantifier child */
                pc = curState[-1].continue_pc;
                op = curState[-1].continue_op;
                continue;

            case REOP_REPEAT:
                --curState;
                do {
                    --gData->stateStackTop;
                    if (!result) {
                        /* Failed, see if we have enough children. */
                        if (curState->u.quantifier.min == 0)
                            goto repeatDone;
                        goto break_switch;
                    }
                    if (curState->u.quantifier.min == 0 &&
                        x->cp == gData->cpbegin + curState->index) {
                        /* matched an empty string, that'll get us nowhere */
                        result = NULL;
                        goto break_switch;
                    }
                    if (curState->u.quantifier.min != 0)
                        curState->u.quantifier.min--;
                    if (curState->u.quantifier.max != (uint16) -1)
                        curState->u.quantifier.max--;
                    if (curState->u.quantifier.max == 0)
                        goto repeatDone;
                    nextpc = pc + ARG_LEN;
                    nextop = (REOp) *nextpc;
                    startcp = x->cp;
                    if (REOP_IS_SIMPLE(nextop)) {
                        nextpc++;
                        if (!SimpleMatch(gData, x, nextop, &nextpc, JS_TRUE)) {
                            if (curState->u.quantifier.min == 0)
                                goto repeatDone;
                            result = NULL;
                            goto break_switch;
                        }
                        result = x;
                    }
                    curState->index = startcp - gData->cpbegin;
                    PUSH_STATE_STACK(gData);
                    if (curState->u.quantifier.min == 0 &&
                        !PushBackTrackState(gData, REOP_REPEAT,
                                            pc, x, startcp,
                                            curState->parenSoFar,
                                            parenSoFar -
                                            curState->parenSoFar)) {
                        return NULL;
                    }
                } while (*nextpc == REOP_ENDCHILD);
                pc = nextpc;
                op = (REOp) *pc++;
                parenSoFar = curState->parenSoFar;
                continue;

            repeatDone:
                result = x;
                pc += GET_OFFSET(pc);
                goto break_switch;

            case REOP_MINIMALSTAR:
                curState->u.quantifier.min = 0;
                curState->u.quantifier.max = (uint16)-1;
                goto minimalquantcommon;
            case REOP_MINIMALPLUS:
                curState->u.quantifier.min = 1;
                curState->u.quantifier.max = (uint16)-1;
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
                PUSH_STATE_STACK(gData);
                if (curState->u.quantifier.min != 0) {
                    curState->continue_op = REOP_MINIMALREPEAT;
                    curState->continue_pc = pc;
                    /* step over <next> */
                    pc += ARG_LEN;
                    op = (REOp) *pc++;
                } else {
                    if (!PushBackTrackState(gData, REOP_MINIMALREPEAT,
                                            pc, x, x->cp, 0, 0)) {
                        return NULL;
                    }
                    --gData->stateStackTop;
                    pc = pc + GET_OFFSET(pc);
                    op = (REOp) *pc++;
                }
                continue;

            case REOP_MINIMALREPEAT:
                --gData->stateStackTop;
                --curState;

                if (!result) {
                    /*
                     * Non-greedy failure - try to consume another child.
                     */
                    if (curState->u.quantifier.max == (uint16) -1 ||
                        curState->u.quantifier.max > 0) {
                        curState->index = x->cp - gData->cpbegin;
                        curState->continue_op = REOP_MINIMALREPEAT;
                        curState->continue_pc = pc;
                        pc += ARG_LEN;
                        for (k = curState->parenSoFar; k < parenSoFar; k++)
                            x->parens[k].index = -1;
                        PUSH_STATE_STACK(gData);
                        op = (REOp) *pc++;
                        continue;
                    }
                    /* Don't need to adjust pc since we're going to pop. */
                    break;
                }
                if (curState->u.quantifier.min == 0 &&
                    x->cp == gData->cpbegin + curState->index) {
                    /* Matched an empty string, that'll get us nowhere. */
                    result = NULL;
                    break;
                }
                if (curState->u.quantifier.min != 0)
                    curState->u.quantifier.min--;
                if (curState->u.quantifier.max != (uint16) -1)
                    curState->u.quantifier.max--;
                if (curState->u.quantifier.min != 0) {
                    curState->continue_op = REOP_MINIMALREPEAT;
                    curState->continue_pc = pc;
                    pc += ARG_LEN;
                    for (k = curState->parenSoFar; k < parenSoFar; k++)
                        x->parens[k].index = -1;
                    curState->index = x->cp - gData->cpbegin;
                    PUSH_STATE_STACK(gData);
                    op = (REOp) *pc++;
                    continue;
                }
                curState->index = x->cp - gData->cpbegin;
                curState->parenSoFar = parenSoFar;
                PUSH_STATE_STACK(gData);
                if (!PushBackTrackState(gData, REOP_MINIMALREPEAT,
                                        pc, x, x->cp,
                                        curState->parenSoFar,
                                        parenSoFar - curState->parenSoFar)) {
                    return NULL;
                }
                --gData->stateStackTop;
                pc = pc + GET_OFFSET(pc);
                op = (REOp) *pc++;
                continue;

            default:
                JS_ASSERT(JS_FALSE);
                result = NULL;
            }
        break_switch:;
        }
        /*
         *  If the match failed and there's a backtrack option, take it.
         *  Otherwise this is a complete and utter failure.
         */
        if (!result) {
            if (gData->cursz == 0)
                return NULL;
            backTrackData = gData->backTrackSP;
            gData->cursz = backTrackData->sz;
            gData->backTrackSP =
                (REBackTrackData *) ((char *)backTrackData - backTrackData->sz);
            x->cp = backTrackData->cp;
            pc = backTrackData->backtrack_pc;
            op = backTrackData->backtrack_op;
            gData->stateStackTop = backTrackData->saveStateStackTop;
            JS_ASSERT(gData->stateStackTop);

            memcpy(gData->stateStack, backTrackData + 1,
                   sizeof(REProgState) * backTrackData->saveStateStackTop);
            curState = &gData->stateStack[gData->stateStackTop - 1];

            if (backTrackData->parenCount) {
                memcpy(&x->parens[backTrackData->parenIndex],
                       (char *)(backTrackData + 1) +
                       sizeof(REProgState) * backTrackData->saveStateStackTop,
                       sizeof(RECapture) * backTrackData->parenCount);
                parenSoFar = backTrackData->parenIndex + backTrackData->parenCount;
            } else {
                for (k = curState->parenSoFar; k < parenSoFar; k++)
                    x->parens[k].index = -1;
                parenSoFar = curState->parenSoFar;
            }
            continue;
        }
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
        result = ExecuteREBytecode(gData, x);
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
InitMatch(JSContext *cx, REGlobalData *gData, JSRegExp *re)
{
    REMatchState *result;
    uintN i;

    gData->backTrackStackSize = INITIAL_BACKTRACK;
    JS_ARENA_ALLOCATE_CAST(gData->backTrackStack, REBackTrackData *,
                           &gData->pool,
                           INITIAL_BACKTRACK);
    if (!gData->backTrackStack)
        return NULL;
    gData->backTrackSP = gData->backTrackStack;
    gData->cursz = 0;


    gData->stateStackLimit = INITIAL_STATESTACK;
    JS_ARENA_ALLOCATE_CAST(gData->stateStack, REProgState *,
                           &gData->pool,
                           sizeof(REProgState) * INITIAL_STATESTACK);
    if (!gData->stateStack)
        return NULL;
    gData->stateStackTop = 0;

    gData->cx = cx;
    gData->regexp = re;
    gData->ok = JS_TRUE;

    JS_ARENA_ALLOCATE_CAST(result, REMatchState *,
                           &gData->pool,
                           sizeof(REMatchState)
                               + (re->parenCount - 1) * sizeof(RECapture));
    if (!result)
        return NULL;

    for (i = 0; i < re->classCount; i++)
        if (!re->classList[i].converted)
            if (!ProcessCharSet(gData, &re->classList[i]))
                return NULL;

    return result;
}

JSBool
js_ExecuteRegExp(JSContext *cx, JSRegExp *re, JSString *str, size_t *indexp,
                 JSBool test, jsval *rval)
{
    REGlobalData gData;
    REMatchState *x, *result;

    const jschar *cp, *ep;
    size_t i, length, start;
    JSSubString *morepar;
    JSBool ok;
    JSRegExpStatics *res;
    ptrdiff_t matchlen;
    uintN num, morenum;
    JSString *parstr, *matchstr;
    JSObject *obj;

    RECapture *parsub = NULL;

    /*
     * It's safe to load from cp because JSStrings have a zero at the end,
     * and we never let cp get beyond cpend.
     */
    start = *indexp;
    length = JSSTRING_LENGTH(str);
    if (start > length)
        start = length;
    cp = JSSTRING_CHARS(str);
    gData.cpbegin = cp;
    gData.cpend = cp + length;
    cp += start;
    gData.start = start;
    gData.skipped = 0;

    JS_InitArenaPool(&gData.pool, "RegExpPool", 8096, 4);
    x = InitMatch(cx, &gData, re);
    if (!x)
        return JS_FALSE;
    x->cp = cp;

    /*
     * Call the recursive matcher to do the real work.  Return null on mismatch
     * whether testing or not.  On match, return an extended Array object.
     */
    result = MatchRegExp(&gData, x);
    ok = gData.ok;
    if (!ok)
        goto out;
    if (!result) {
        *rval = JSVAL_NULL;
        goto out;
    }
    cp = result->cp;
    i = PTRDIFF(cp, gData.cpbegin, jschar);
    *indexp = i;
    matchlen = i - (start + gData.skipped);
    ep = cp;
    cp -= matchlen;

    if (test) {
        /*
         * Testing for a match and updating cx->regExpStatics: don't allocate
         * an array object, do return true.
         */
        *rval = JSVAL_TRUE;

        /* Avoid warning.  (gcc doesn't detect that obj is needed iff !test); */
        obj = NULL;
    } else {
        /*
         * The array returned on match has element 0 bound to the matched
         * string, elements 1 through state.parenCount bound to the paren
         * matches, an index property telling the length of the left context,
         * and an input property referring to the input string.
         */
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj) {
            ok = JS_FALSE;
            goto out;
        }
        *rval = OBJECT_TO_JSVAL(obj);

#define DEFVAL(val, id) {                                                     \
    ok = js_DefineProperty(cx, obj, id, val,                                  \
                           JS_PropertyStub, JS_PropertyStub,                  \
                           JSPROP_ENUMERATE, NULL);                           \
    if (!ok) {                                                                \
        cx->newborn[GCX_OBJECT] = NULL;                                       \
        cx->newborn[GCX_STRING] = NULL;                                       \
        goto out;                                                             \
    }                                                                         \
}

        matchstr = js_NewStringCopyN(cx, cp, matchlen, 0);
        if (!matchstr) {
            cx->newborn[GCX_OBJECT] = NULL;
            ok = JS_FALSE;
            goto out;
        }
        DEFVAL(STRING_TO_JSVAL(matchstr), INT_TO_JSID(0));
    }

    res = &cx->regExpStatics;
    res->input = str;
    res->parenCount = re->parenCount;
    if (re->parenCount == 0) {
        res->lastParen = js_EmptySubString;
    } else {
        for (num = 0; num < re->parenCount; num++) {
            parsub = &result->parens[num];
            if (num < 9) {
                if (parsub->index == -1) {
                    res->parens[num].chars = NULL;
                    res->parens[num].length = 0;
                } else {
                    res->parens[num].chars = gData.cpbegin + parsub->index;
                    res->parens[num].length = parsub->length;
                }
            } else {
                morenum = num - 9;
                morepar = res->moreParens;
                if (!morepar) {
                    res->moreLength = 10;
                    morepar = (JSSubString*)
                        JS_malloc(cx, 10 * sizeof(JSSubString));
                } else if (morenum >= res->moreLength) {
                    res->moreLength += 10;
                    morepar = (JSSubString*)
                        JS_realloc(cx, morepar, res->moreLength *
                                                sizeof(JSSubString));
                }
                if (!morepar) {
                    cx->newborn[GCX_OBJECT] = NULL;
                    cx->newborn[GCX_STRING] = NULL;
                    ok = JS_FALSE;
                    goto out;
                }
                res->moreParens = morepar;
                if (parsub->index == -1) {
                    morepar[morenum].chars = NULL;
                    morepar[morenum].length = 0;
                } else {
                    morepar[morenum].chars = gData.cpbegin + parsub->index;
                    morepar[morenum].length = parsub->length;
                }
            }
            if (test)
                continue;
            if (parsub->index == -1) {
                ok = js_DefineProperty(cx, obj, INT_TO_JSID(num + 1),
                                       JSVAL_VOID, NULL, NULL,
                                       JSPROP_ENUMERATE, NULL);
            } else {
                parstr = js_NewStringCopyN(cx, gData.cpbegin + parsub->index,
                                           parsub->length, 0);
                if (!parstr) {
                    cx->newborn[GCX_OBJECT] = NULL;
                    cx->newborn[GCX_STRING] = NULL;
                    ok = JS_FALSE;
                    goto out;
                }
                ok = js_DefineProperty(cx, obj, INT_TO_JSID(num + 1),
                                       STRING_TO_JSVAL(parstr), NULL, NULL,
                                       JSPROP_ENUMERATE, NULL);
            }
            if (!ok) {
                cx->newborn[GCX_OBJECT] = NULL;
                cx->newborn[GCX_STRING] = NULL;
                goto out;
            }
        }
        if (parsub->index == -1) {
            res->lastParen = js_EmptySubString;
        } else {
            res->lastParen.chars = gData.cpbegin + parsub->index;
            res->lastParen.length = parsub->length;
        }
    }

    if (!test) {
        /*
         * Define the index and input properties last for better for/in loop
         * order (so they come after the elements).
         */
        DEFVAL(INT_TO_JSVAL(start + gData.skipped),
               ATOM_TO_JSID(cx->runtime->atomState.indexAtom));
        DEFVAL(STRING_TO_JSVAL(str),
               ATOM_TO_JSID(cx->runtime->atomState.inputAtom));
    }

#undef DEFVAL

    res->lastMatch.chars = cp;
    res->lastMatch.length = matchlen;
    if (cx->version == JSVERSION_1_2) {
        /*
         * JS1.2 emulated Perl4.0.1.8 (patch level 36) for global regexps used
         * in scalar contexts, and unintentionally for the string.match "list"
         * psuedo-context.  On "hi there bye", the following would result:
         *
         * Language     while(/ /g){print("$`");}   s/ /$`/g
         * perl4.036    "hi", "there"               "hihitherehi therebye"
         * perl5        "hi", "hi there"            "hihitherehi therebye"
         * js1.2        "hi", "there"               "hihitheretherebye"
         */
        res->leftContext.chars = JSSTRING_CHARS(str) + start;
        res->leftContext.length = gData.skipped;
    } else {
        /*
         * For JS1.3 and ECMAv2, emulate Perl5 exactly:
         *
         * js1.3        "hi", "hi there"            "hihitherehi therebye"
         */
        res->leftContext.chars = JSSTRING_CHARS(str);
        res->leftContext.length = start + gData.skipped;
    }
    res->rightContext.chars = ep;
    res->rightContext.length = gData.cpend - ep;

out:
    JS_FinishArenaPool(&gData.pool);
    return ok;
}

/************************************************************************/

enum regexp_tinyid {
    REGEXP_SOURCE       = -1,
    REGEXP_GLOBAL       = -2,
    REGEXP_IGNORE_CASE  = -3,
    REGEXP_LAST_INDEX   = -4,
    REGEXP_MULTILINE    = -5
};

#define REGEXP_PROP_ATTRS (JSPROP_PERMANENT|JSPROP_SHARED)

static JSPropertySpec regexp_props[] = {
    {"source",     REGEXP_SOURCE,      REGEXP_PROP_ATTRS | JSPROP_READONLY,0,0},
    {"global",     REGEXP_GLOBAL,      REGEXP_PROP_ATTRS | JSPROP_READONLY,0,0},
    {"ignoreCase", REGEXP_IGNORE_CASE, REGEXP_PROP_ATTRS | JSPROP_READONLY,0,0},
    {"lastIndex",  REGEXP_LAST_INDEX,  REGEXP_PROP_ATTRS,0,0},
    {"multiline",  REGEXP_MULTILINE,   REGEXP_PROP_ATTRS | JSPROP_READONLY,0,0},
    {0,0,0,0,0}
};

static JSBool
regexp_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSRegExp *re;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);
    if (slot == REGEXP_LAST_INDEX)
        return JS_GetReservedSlot(cx, obj, 0, vp);

    JS_LOCK_OBJ(cx, obj);
    re = (JSRegExp *) JS_GetInstancePrivate(cx, obj, &js_RegExpClass, NULL);
    if (re) {
	switch (slot) {
	  case REGEXP_SOURCE:
	    *vp = STRING_TO_JSVAL(re->source);
	    break;
	  case REGEXP_GLOBAL:
	    *vp = BOOLEAN_TO_JSVAL((re->flags & JSREG_GLOB) != 0);
	    break;
	  case REGEXP_IGNORE_CASE:
	    *vp = BOOLEAN_TO_JSVAL((re->flags & JSREG_FOLD) != 0);
	    break;
	  case REGEXP_MULTILINE:
	    *vp = BOOLEAN_TO_JSVAL((re->flags & JSREG_MULTILINE) != 0);
	    break;
	}
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

static JSBool
regexp_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBool ok;
    jsint slot;
    jsdouble lastIndex;

    ok = JS_TRUE;
    if (!JSVAL_IS_INT(id))
	return ok;
    slot = JSVAL_TO_INT(id);
    if (slot == REGEXP_LAST_INDEX) {
        if (!js_ValueToNumber(cx, *vp, &lastIndex))
            return JS_FALSE;
        lastIndex = js_DoubleToInteger(lastIndex);
        ok = js_NewNumberValue(cx, lastIndex, vp) &&
             JS_SetReservedSlot(cx, obj, 0, *vp);
    }
    return ok;
}

/*
 * RegExp class static properties and their Perl counterparts:
 *
 *  RegExp.input                $_
 *  RegExp.multiline            $*
 *  RegExp.lastMatch            $&
 *  RegExp.lastParen            $+
 *  RegExp.leftContext          $`
 *  RegExp.rightContext         $'
 */
enum regexp_static_tinyid {
    REGEXP_STATIC_INPUT         = -1,
    REGEXP_STATIC_MULTILINE     = -2,
    REGEXP_STATIC_LAST_MATCH    = -3,
    REGEXP_STATIC_LAST_PAREN    = -4,
    REGEXP_STATIC_LEFT_CONTEXT  = -5,
    REGEXP_STATIC_RIGHT_CONTEXT = -6
};

JSBool
js_InitRegExpStatics(JSContext *cx, JSRegExpStatics *res)
{
    JS_ClearRegExpStatics(cx);
    return js_AddRoot(cx, &res->input, "res->input");
}

void
js_FreeRegExpStatics(JSContext *cx, JSRegExpStatics *res)
{
    if (res->moreParens) {
        JS_free(cx, res->moreParens);
        res->moreParens = NULL;
    }
    js_RemoveRoot(cx->runtime, &res->input);
}

static JSBool
regexp_static_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSRegExpStatics *res;
    JSString *str;
    JSSubString *sub;

    res = &cx->regExpStatics;
    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    slot = JSVAL_TO_INT(id);
    switch (slot) {
      case REGEXP_STATIC_INPUT:
        *vp = res->input ? STRING_TO_JSVAL(res->input)
                         : JS_GetEmptyStringValue(cx);
        return JS_TRUE;
      case REGEXP_STATIC_MULTILINE:
        *vp = BOOLEAN_TO_JSVAL(res->multiline);
        return JS_TRUE;
      case REGEXP_STATIC_LAST_MATCH:
        sub = &res->lastMatch;
        break;
      case REGEXP_STATIC_LAST_PAREN:
        sub = &res->lastParen;
        break;
      case REGEXP_STATIC_LEFT_CONTEXT:
        sub = &res->leftContext;
        break;
      case REGEXP_STATIC_RIGHT_CONTEXT:
        sub = &res->rightContext;
        break;
      default:
        sub = REGEXP_PAREN_SUBSTRING(res, slot);
        break;
    }
    str = js_NewStringCopyN(cx, sub->chars, sub->length, 0);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
regexp_static_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSRegExpStatics *res;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    res = &cx->regExpStatics;
    /* XXX use if-else rather than switch to keep MSVC1.52 from crashing */
    if (JSVAL_TO_INT(id) == REGEXP_STATIC_INPUT) {
        if (!JSVAL_IS_STRING(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
            return JS_FALSE;
        }
        res->input = JSVAL_TO_STRING(*vp);
    } else if (JSVAL_TO_INT(id) == REGEXP_STATIC_MULTILINE) {
        if (!JSVAL_IS_BOOLEAN(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_BOOLEAN, vp)) {
            return JS_FALSE;
        }
        res->multiline = JSVAL_TO_BOOLEAN(*vp);
    }
    return JS_TRUE;
}

static JSPropertySpec regexp_static_props[] = {
    {"input",
     REGEXP_STATIC_INPUT,
     JSPROP_ENUMERATE|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_setProperty},
    {"multiline",
     REGEXP_STATIC_MULTILINE,
     JSPROP_ENUMERATE|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_setProperty},
    {"lastMatch",
     REGEXP_STATIC_LAST_MATCH,
     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"lastParen",
     REGEXP_STATIC_LAST_PAREN,
     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"leftContext",
     REGEXP_STATIC_LEFT_CONTEXT,
     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"rightContext",
     REGEXP_STATIC_RIGHT_CONTEXT,
     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},

    /* XXX should have block scope and local $1, etc. */
    {"$1", 0, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$2", 1, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$3", 2, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$4", 3, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$5", 4, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$6", 5, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$7", 6, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$8", 7, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$9", 8, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_SHARED,
     regexp_static_getProperty,    regexp_static_getProperty},

    {0,0,0,0,0}
};

static void
regexp_finalize(JSContext *cx, JSObject *obj)
{
    JSRegExp *re;

    re = (JSRegExp *) JS_GetPrivate(cx, obj);
    if (!re)
        return;
    js_DestroyRegExp(cx, re);
}

/* Forward static prototype. */
static JSBool
regexp_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
            jsval *rval);

static JSBool
regexp_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return regexp_exec(cx, JSVAL_TO_OBJECT(argv[-2]), argc, argv, rval);
}

#if JS_HAS_XDR

#include "jsxdrapi.h"

static JSBool
regexp_xdrObject(JSXDRState *xdr, JSObject **objp)
{
    JSRegExp *re;
    JSString *source;
    uint32 flagsword;
    JSObject *obj;

    if (xdr->mode == JSXDR_ENCODE) {
        re = (JSRegExp *) JS_GetPrivate(xdr->cx, *objp);
        if (!re)
            return JS_FALSE;
        source = re->source;
        flagsword = ((uint32)re->cloneIndex << 16) | re->flags;
    }
    if (!JS_XDRString(xdr, &source) ||
        !JS_XDRUint32(xdr, &flagsword)) {
        return JS_FALSE;
    }
    if (xdr->mode == JSXDR_DECODE) {
        obj = js_NewObject(xdr->cx, &js_RegExpClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        re = js_NewRegExp(xdr->cx, NULL, source, (uint16)flagsword, JS_FALSE);
        if (!re)
            return JS_FALSE;
        if (!JS_SetPrivate(xdr->cx, obj, re) ||
            !js_SetLastIndex(xdr->cx, obj, 0)) {
            js_DestroyRegExp(xdr->cx, re);
            return JS_FALSE;
        }
        re->cloneIndex = (uint16)(flagsword >> 16);
        *objp = obj;
    }
    return JS_TRUE;
}

#else  /* !JS_HAS_XDR */

#define regexp_xdrObject NULL

#endif /* !JS_HAS_XDR */

static uint32
regexp_mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSRegExp *re = (JSRegExp *) JS_GetPrivate(cx, obj);
    if (re)
        JS_MarkGCThing(cx, re->source, "source", arg);
    return 0;
}

JSClass js_RegExpClass = {
    js_RegExp_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,  JS_PropertyStub,  regexp_getProperty, regexp_setProperty,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,     regexp_finalize,
    NULL,             NULL,             regexp_call,        NULL,
    regexp_xdrObject, NULL,             regexp_mark,        0
};

static const jschar empty_regexp_ucstr[] = {'(', '?', ':', ')', 0};

JSBool
js_regexp_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
    JSRegExp *re;
    const jschar *source;
    jschar *chars;
    size_t length, nflags;
    uintN flags;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
        return JS_FALSE;
    JS_LOCK_OBJ(cx, obj);
    re = (JSRegExp *) JS_GetPrivate(cx, obj);
    if (!re) {
        JS_UNLOCK_OBJ(cx, obj);
        *rval = STRING_TO_JSVAL(cx->runtime->emptyString);
        return JS_TRUE;
    }

    source = JSSTRING_CHARS(re->source);
    length = JSSTRING_LENGTH(re->source);
    if (length == 0) {
        source = empty_regexp_ucstr;
        length = sizeof(empty_regexp_ucstr) / sizeof(jschar) - 1;
    }
    length += 2;
    nflags = 0;
    for (flags = re->flags; flags != 0; flags &= flags - 1)
        nflags++;
    chars = (jschar*) JS_malloc(cx, (length + nflags + 1) * sizeof(jschar));
    if (!chars) {
        JS_UNLOCK_OBJ(cx, obj);
        return JS_FALSE;
    }

    chars[0] = '/';
    js_strncpy(&chars[1], source, length - 2);
    chars[length-1] = '/';
    if (nflags) {
        if (re->flags & JSREG_GLOB)
            chars[length++] = 'g';
        if (re->flags & JSREG_FOLD)
            chars[length++] = 'i';
        if (re->flags & JSREG_MULTILINE)
            chars[length++] = 'm';
    }
    JS_UNLOCK_OBJ(cx, obj);
    chars[length] = 0;

    str = js_NewString(cx, chars, length, 0);
    if (!str) {
        JS_free(cx, chars);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
regexp_compile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    JSString *opt, *str;
    JSRegExp *oldre, *re;
    JSBool ok, ok2;
    JSObject *obj2;
    size_t length, nbytes;
    const jschar *cp, *start, *end;
    jschar *nstart, *ncp, *tmp;

    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
        return JS_FALSE;
    opt = NULL;
    if (argc == 0) {
        str = cx->runtime->emptyString;
    } else {
        if (JSVAL_IS_OBJECT(argv[0])) {
            /*
             * If we get passed in a RegExp object we construct a new
             * RegExp that is a duplicate of it by re-compiling the
             * original source code. ECMA requires that it be an error
             * here if the flags are specified. (We must use the flags
             * from the original RegExp also).
             */
            obj2 = JSVAL_TO_OBJECT(argv[0]);
            if (obj2 && OBJ_GET_CLASS(cx, obj2) == &js_RegExpClass) {
                if (argc >= 2 && !JSVAL_IS_VOID(argv[1])) { /* 'flags' passed */
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_NEWREGEXP_FLAGGED);
                    return JS_FALSE;
                }
                JS_LOCK_OBJ(cx, obj2);
                re = (JSRegExp *) JS_GetPrivate(cx, obj2);
                if (!re) {
                    JS_UNLOCK_OBJ(cx, obj2);
                    return JS_FALSE;
                }
                re = js_NewRegExp(cx, NULL, re->source, re->flags, JS_FALSE);
                JS_UNLOCK_OBJ(cx, obj2);
                goto created;
            }
        }
        str = js_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        argv[0] = STRING_TO_JSVAL(str);
        if (argc > 1) {
            if (JSVAL_IS_VOID(argv[1])) {
                opt = NULL;
            } else {
                opt = js_ValueToString(cx, argv[1]);
                if (!opt)
                    return JS_FALSE;
                argv[1] = STRING_TO_JSVAL(opt);
            }
        }

        /* Escape any naked slashes in the regexp source. */
        length = JSSTRING_LENGTH(str);
        start = JSSTRING_CHARS(str);
        end = start + length;
        nstart = ncp = NULL;
        for (cp = start; cp < end; cp++) {
            if (*cp == '/' && (cp == start || cp[-1] != '\\')) {
                nbytes = (++length + 1) * sizeof(jschar);
                if (!nstart) {
                    nstart = (jschar *) JS_malloc(cx, nbytes);
                    if (!nstart)
                        return JS_FALSE;
                    ncp = nstart + (cp - start);
                    js_strncpy(nstart, start, cp - start);
                } else {
                    tmp = (jschar *) JS_realloc(cx, nstart, nbytes);
                    if (!tmp) {
                        JS_free(cx, nstart);
                        return JS_FALSE;
                    }
                    ncp = tmp + (ncp - nstart);
                    nstart = tmp;
                }
                *ncp++ = '\\';
            }
            if (nstart)
                *ncp++ = *cp;
        }

        if (nstart) {
            /* Don't forget to store the backstop after the new string. */
            JS_ASSERT((size_t)(ncp - nstart) == length);
            *ncp = 0;
            str = js_NewString(cx, nstart, length, 0);
            if (!str) {
                JS_free(cx, nstart);
                return JS_FALSE;
            }
            argv[0] = STRING_TO_JSVAL(str);
        }
    }

    re = js_NewRegExpOpt(cx, NULL, str, opt, JS_FALSE);
created:
    if (!re)
        return JS_FALSE;
    JS_LOCK_OBJ(cx, obj);
    oldre = (JSRegExp *) JS_GetPrivate(cx, obj);
    ok = JS_SetPrivate(cx, obj, re);
    ok2 = js_SetLastIndex(cx, obj, 0);
    JS_UNLOCK_OBJ(cx, obj);
    if (!ok) {
        js_DestroyRegExp(cx, re);
        return JS_FALSE;
    }
    if (oldre)
        js_DestroyRegExp(cx, oldre);
    *rval = OBJECT_TO_JSVAL(obj);
    return ok2;
}

static JSBool
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                JSBool test, jsval *rval)
{
    JSBool ok;
    JSRegExp *re;
    jsdouble lastIndex;
    JSString *str;
    size_t i;

    ok = JS_InstanceOf(cx, obj, &js_RegExpClass, argv);
    if (!ok)
	return JS_FALSE;
    JS_LOCK_OBJ(cx, obj);
    re = (JSRegExp *) JS_GetPrivate(cx, obj);
    if (!re) {
        JS_UNLOCK_OBJ(cx, obj);
        return JS_TRUE;
    }

    /* NB: we must reach out: after this paragraph, in order to drop re. */
    HOLD_REGEXP(cx, re);
    if (re->flags & JSREG_GLOB) {
        ok = js_GetLastIndex(cx, obj, &lastIndex);
    } else {
        lastIndex = 0;
    }
    JS_UNLOCK_OBJ(cx, obj);
    if (!ok)
        goto out;

    /* Now that obj is unlocked, it's safe to (potentially) grab the GC lock. */
    if (argc == 0) {
        str = cx->regExpStatics.input;
        if (!str) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_NO_INPUT,
                                 JS_GetStringBytes(re->source),
                                 (re->flags & JSREG_GLOB) ? "g" : "",
                                 (re->flags & JSREG_FOLD) ? "i" : "",
                                 (re->flags & JSREG_MULTILINE) ? "m" : "");
            ok = JS_FALSE;
            goto out;
        }
    } else {
        str = js_ValueToString(cx, argv[0]);
        if (!str)
            goto out;
        argv[0] = STRING_TO_JSVAL(str);
    }

    if (lastIndex < 0 || JSSTRING_LENGTH(str) < lastIndex) {
        ok = js_SetLastIndex(cx, obj, 0);
        *rval = JSVAL_NULL;
    } else {
        i = (size_t) lastIndex;
        ok = js_ExecuteRegExp(cx, re, str, &i, test, rval);
        if (ok && (re->flags & JSREG_GLOB))
            ok = js_SetLastIndex(cx, obj, (*rval == JSVAL_NULL) ? 0 : i);
    }

out:
    DROP_REGEXP(cx, re);
    return ok;
}

static JSBool
regexp_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return regexp_exec_sub(cx, obj, argc, argv, JS_FALSE, rval);
}

static JSBool
regexp_test(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!regexp_exec_sub(cx, obj, argc, argv, JS_TRUE, rval))
        return JS_FALSE;
    if (*rval != JSVAL_TRUE)
        *rval = JSVAL_FALSE;
    return JS_TRUE;
}

static JSFunctionSpec regexp_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   js_regexp_toString,     0,0,0},
#endif
    {js_toString_str,   js_regexp_toString,     0,0,0},
    {"compile",         regexp_compile,         1,0,0},
    {"exec",            regexp_exec,            0,0,0},
    {"test",            regexp_test,            0,0,0},
    {0,0,0,0,0}
};

static JSBool
RegExp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
        /*
         * If first arg is regexp and no flags are given, just return the arg.
         * (regexp_compile detects the regexp + flags case and throws a
         * TypeError.)  See 10.15.3.1.
         */
        if ((argc < 2 || JSVAL_IS_VOID(argv[1])) &&
            JSVAL_IS_OBJECT(argv[0]) &&
            OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(argv[0])) == &js_RegExpClass) {
            *rval = argv[0];
            return JS_TRUE;
        }

        /* Otherwise, replace obj with a new RegExp object. */
        obj = js_NewObject(cx, &js_RegExpClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
    }
    return regexp_compile(cx, obj, argc, argv, rval);
}

JSObject *
js_InitRegExpClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto, *ctor;
    jsval rval;

    proto = JS_InitClass(cx, obj, NULL, &js_RegExpClass, RegExp, 1,
                         regexp_props, regexp_methods,
                         regexp_static_props, NULL);

    if (!proto || !(ctor = JS_GetConstructor(cx, proto)))
        return NULL;
    if (!JS_AliasProperty(cx, ctor, "input",        "$_") ||
        !JS_AliasProperty(cx, ctor, "multiline",    "$*") ||
        !JS_AliasProperty(cx, ctor, "lastMatch",    "$&") ||
        !JS_AliasProperty(cx, ctor, "lastParen",    "$+") ||
        !JS_AliasProperty(cx, ctor, "leftContext",  "$`") ||
        !JS_AliasProperty(cx, ctor, "rightContext", "$'")) {
        goto bad;
    }

    /* Give RegExp.prototype private data so it matches the empty string. */
    if (!regexp_compile(cx, proto, 0, NULL, &rval))
        goto bad;
    return proto;

bad:
    JS_DeleteProperty(cx, obj, js_RegExpClass.name);
    return NULL;
}

JSObject *
js_NewRegExpObject(JSContext *cx, JSTokenStream *ts,
                   jschar *chars, size_t length, uintN flags)
{
    JSString *str;
    JSObject *obj;
    JSRegExp *re;

    str = js_NewStringCopyN(cx, chars, length, 0);
    if (!str)
        return NULL;
    re = js_NewRegExp(cx, ts,  str, flags, JS_FALSE);
    if (!re)
        return NULL;
    obj = js_NewObject(cx, &js_RegExpClass, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, re) || !js_SetLastIndex(cx, obj, 0)) {
        js_DestroyRegExp(cx, re);
        return NULL;
    }
    return obj;
}

JSObject *
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *parent)
{
    JSObject *clone;
    JSRegExp *re;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_RegExpClass);
    clone = js_NewObject(cx, &js_RegExpClass, NULL, parent);
    if (!clone)
        return NULL;
    re = JS_GetPrivate(cx, obj);
    if (!JS_SetPrivate(cx, clone, re) || !js_SetLastIndex(cx, clone, 0)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    HOLD_REGEXP(cx, re);
    return clone;
}

JSBool
js_GetLastIndex(JSContext *cx, JSObject *obj, jsdouble *lastIndex)
{
    jsval v;

    return JS_GetReservedSlot(cx, obj, 0, &v) &&
           js_ValueToNumber(cx, v, lastIndex);
}

JSBool
js_SetLastIndex(JSContext *cx, JSObject *obj, jsdouble lastIndex)
{
    jsval v;

    return js_NewNumberValue(cx, lastIndex, &v) &&
           JS_SetReservedSlot(cx, obj, 0, v);
}

#endif /* JS_HAS_REGEXPS */
