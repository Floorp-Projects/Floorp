/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
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

typedef struct RENode RENode;

typedef enum REOp {
    REOP_EMPTY      = 0,  /* match rest of input against rest of r.e. */
    REOP_ALT        = 1,  /* alternative subexpressions in kid and next */
    REOP_BOL        = 2,  /* beginning of input (or line if multiline) */
    REOP_EOL        = 3,  /* end of input (or line if multiline) */
    REOP_WBDRY      = 4,  /* match "" at word boundary */
    REOP_WNONBDRY   = 5,  /* match "" at word non-boundary */
    REOP_QUANT      = 6,  /* quantified atom: atom{1,2} */
    REOP_STAR       = 7,  /* zero or more occurrences of kid */
    REOP_PLUS       = 8,  /* one or more occurrences of kid */
    REOP_OPT        = 9,  /* optional subexpression in kid */
    REOP_LPAREN     = 10, /* left paren bytecode: kid is u.num'th sub-regexp */
    REOP_RPAREN     = 11, /* right paren bytecode */
    REOP_DOT        = 12, /* stands for any character */
    REOP_CCLASS     = 13, /* character class: [a-f] */
    REOP_DIGIT      = 14, /* match a digit char: [0-9] */
    REOP_NONDIGIT   = 15, /* match a non-digit char: [^0-9] */
    REOP_ALNUM      = 16, /* match an alphanumeric char: [0-9a-z_A-Z] */
    REOP_NONALNUM   = 17, /* match a non-alphanumeric char: [^0-9a-z_A-Z] */
    REOP_SPACE      = 18, /* match a whitespace char */
    REOP_NONSPACE   = 19, /* match a non-whitespace char */
    REOP_BACKREF    = 20, /* back-reference (e.g., \1) to a parenthetical */
    REOP_FLAT       = 21, /* match a flat string */
    REOP_FLAT1      = 22, /* match a single char */
    REOP_JUMP       = 23, /* for deoptimized closure loops */
    REOP_DOTSTAR    = 24, /* optimize .* to use a single opcode */
    REOP_ANCHOR     = 25, /* like .* but skips left context to unanchored r.e. */
    REOP_EOLONLY    = 26, /* $ not preceded by any pattern */
    REOP_UCFLAT     = 27, /* flat Unicode string; len immediate counts chars */
    REOP_UCFLAT1    = 28, /* single Unicode char */
    REOP_UCCLASS    = 29, /* Unicode character class, vector of chars to match */
    REOP_NUCCLASS   = 30, /* negated Unicode character class */
    REOP_BACKREFi   = 31, /* case-independent REOP_BACKREF */
    REOP_FLATi      = 32, /* case-independent REOP_FLAT */
    REOP_FLAT1i     = 33, /* case-independent REOP_FLAT1 */
    REOP_UCFLATi    = 34, /* case-independent REOP_UCFLAT */
    REOP_UCFLAT1i   = 35, /* case-independent REOP_UCFLAT1 */
    REOP_ANCHOR1    = 36, /* first-char discriminating REOP_ANCHOR */
    REOP_NCCLASS    = 37, /* negated 8-bit character class */
    REOP_DOTSTARMIN = 38, /* ungreedy version of REOP_DOTSTAR */
    REOP_LPARENNON  = 39, /* non-capturing version of REOP_LPAREN */
    REOP_RPARENNON  = 40, /* non-capturing version of REOP_RPAREN */
    REOP_ASSERT     = 41, /* zero width positive lookahead assertion */
    REOP_ASSERT_NOT = 42, /* zero width negative lookahead assertion */
    REOP_END
} REOp;

#define REOP_FLATLEN_MAX 255    /* maximum length of FLAT string */

struct RENode {
    uint8           op;         /* packed r.e. op bytecode */
    uint8           flags;      /* flags, see below */
#ifdef DEBUG
    uint16          offset;     /* bytecode offset */
#endif
    RENode          *next;      /* next in concatenation order */
    void            *kid;       /* first operand */
    union {
	void        *kid2;      /* second operand */
	jsint       num;        /* could be a number */
	jschar      chr;        /* or a character */
	struct {                /* or a quantifier range */
	    uint16  min;
	    uint16  max;
	} range;
	struct {                /* or a Unicode character class */
	    uint16  kidlen;     /* length of string at kid, in jschars */
	    uint16  bmsize;     /* bitmap size, based on max char code */
            uint8   *bitmap;
	} ucclass;
    } u;
};

#define REOP(ren)   ((REOp)(ren)->op)

#define RENODE_ANCHORED 0x01    /* anchored at the front */
#define RENODE_SINGLE   0x02    /* matches a single char */
#define RENODE_NONEMPTY 0x04    /* does not match empty string */
#define RENODE_ISNEXT   0x08    /* ren is next after at least one node */
#define RENODE_GOODNEXT 0x10    /* ren->next is a tree-like edge in the graph */
#define RENODE_ISJOIN   0x20    /* ren is a join point in the graph */
#define RENODE_REALLOK  0x40    /* REOP_FLAT owns tempPool space to realloc */
#define RENODE_MINIMAL  0x80    /* un-greedy matching for ? * + {} */

typedef struct CompilerState {
    JSContext       *context;
    JSTokenStream   *tokenStream; /* For reporting errors */
    const jschar    *cpbegin;
    const jschar    *cpend;
    const jschar    *cp;
    uintN           flags;
    uintN           parenCount;
    size_t          progLength;
} CompilerState;

#ifdef DEBUG

#include <stdio.h>

static char *reopname[] = {
    "empty",
    "alt",
    "bol",
    "eol",
    "wbdry",
    "wnonbdry",
    "quant",
    "star",
    "plus",
    "opt",
    "lparen",
    "rparen",
    "dot",
    "cclass",
    "digit",
    "nondigit",
    "alnum",
    "nonalnum",
    "space",
    "nonspace",
    "backref",
    "flat",
    "flat1",
    "jump",
    "dotstar",
    "anchor",
    "eolonly",
    "ucflat",
    "ucflat1",
    "ucclass",
    "nucclass",
    "backrefi",
    "flati",
    "flat1i",
    "ucflati",
    "ucflat1i",
    "anchor1",
    "ncclass",
    "dotstar_min",
    "lparen_non",
    "rparen_non",
    "assert",
    "assert_not",
    "end"
};

static void
PrintChar(jschar c)
{
    if (c >> 8)
	printf("\\u%04X", c);
    else
#if !defined XP_PC || !defined _MSC_VER || _MSC_VER > 800
	putchar((char)c);
#else
	/* XXX is there a better way with MSVC1.52? */
	printf("%c", c);
#endif
}

static int gOffset = 0;

static JSBool
DumpRegExp(JSContext *cx, RENode *ren)
{
    static int level;
    JSBool ok;
    int i, len;
    jschar *cp;
    char *cstr;

    if (level == 0)
	printf("level offset  flags  description\n");
    level++;
    ok = JS_TRUE;
    do {
	printf("%5d %6d %6d %c%c%c%c%c%c%c  %s",
	       level,
               (int)ren->offset, (ren->next) ? (int)ren->next->offset : -1,
	       (ren->flags & RENODE_ANCHORED) ? 'A' : '-',
	       (ren->flags & RENODE_SINGLE)   ? 'S' : '-',
	       (ren->flags & RENODE_NONEMPTY) ? 'F' : '-',	/* F for full */
	       (ren->flags & RENODE_ISNEXT)   ? 'N' : '-',	/* N for next */
	       (ren->flags & RENODE_GOODNEXT) ? 'G' : '-',
	       (ren->flags & RENODE_ISJOIN)   ? 'J' : '-',
	       (ren->flags & RENODE_MINIMAL)  ? 'M' : '-',
	       reopname[ren->op]);

	switch (REOP(ren)) {
	  case REOP_ALT:
	    printf(" %d\n", ren->next->offset);
	    ok = DumpRegExp(cx, (RENode*) ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_STAR:
	  case REOP_PLUS:
	  case REOP_OPT:
	  case REOP_ANCHOR1:
	  case REOP_ASSERT:
	  case REOP_ASSERT_NOT:
	    printf("\n");
	    ok = DumpRegExp(cx, (RENode*) ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_QUANT:
	    printf(" next %d min %d max %d\n",
		   ren->next->offset, ren->u.range.min, ren->u.range.max);
	    ok = DumpRegExp(cx, (RENode*) ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_LPAREN:
	    printf(" num %d\n", (int)ren->u.num);
	    ok = DumpRegExp(cx, (RENode*) ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_LPARENNON:
            printf("\n");
	    ok = DumpRegExp(cx, (RENode*) ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_RPAREN:
	    printf(" num %d\n", (int)ren->u.num);
	    break;

	  case REOP_CCLASS:
	    len = (jschar *)ren->u.kid2 - (jschar *)ren->kid;
	    cstr = js_DeflateString(cx, (jschar *)ren->kid, len);
	    if (!cstr) {
		ok = JS_FALSE;
		goto out;
	    }
	    printf(" [%s]\n", cstr);
	    JS_free(cx, cstr);
	    break;

	  case REOP_BACKREF:
	    printf(" num %d\n", (int)ren->u.num);
	    break;

	  case REOP_FLAT:
	    len = (jschar *)ren->u.kid2 - (jschar *)ren->kid;
	    cstr = js_DeflateString(cx, (jschar *)ren->kid, len);
	    if (!cstr) {
		ok = JS_FALSE;
		goto out;
	    }
	    printf(" %s (%d)\n", cstr, len);
	    JS_free(cx, cstr);
	    break;

	  case REOP_FLAT1:
	    printf(" %c ('\\%o')\n", (char)ren->u.chr, ren->u.chr);
	    break;

	  case REOP_JUMP:
	    printf(" %d\n", ren->next->offset);
	    break;

	  case REOP_UCFLAT:
	    cp = (jschar*) ren->kid;
	    len = (jschar *)ren->u.kid2 - cp;
	    for (i = 0; i < len; i++)
		PrintChar(cp[i]);
            printf("\n");
	    break;

	  case REOP_UCFLAT1:
	    PrintChar(ren->u.chr);
	    printf("\n");
	    break;

	  case REOP_UCCLASS:
	    cp = (jschar*) ren->kid;
	    len = ren->u.ucclass.kidlen;
	    printf(" [");
	    for (i = 0; i < len; i++)
		PrintChar(cp[i]);
	    printf("]\n");
	    break;

	  default:
	    printf("\n");
	    break;
	}

	if (!(ren->flags & RENODE_GOODNEXT))
	    break;
    } while ((ren = ren->next) != NULL);
out:
    level--;
    return ok;
}

#endif /* DEBUG */

static RENode *
NewRENode(CompilerState *state, REOp op, void *kid)
{
    JSContext *cx;
    RENode *ren;

    cx = state->context;
    ren = (RENode*) JS_malloc(cx, sizeof *ren);
    if (!ren) {
	JS_ReportOutOfMemory(cx);
	return NULL;
    }
    ren->op = (uint8)op;
    ren->flags = 0;
#ifdef DEBUG
    ren->offset = gOffset++;
#endif
    ren->next = NULL;
    ren->kid = kid;
    return ren;
}

static JSBool
FixNext(CompilerState *state, RENode *ren1, RENode *ren2, RENode *oldnext)
{
    JSBool goodnext;
    RENode *next, *kid, *ren;

    goodnext = ren2 && !(ren2->flags & RENODE_ISNEXT);

    /*
     * Find the final node in a list of alternatives, or concatenations, or
     * even a concatenation of alternatives followed by non-alternatives (e.g.
     * ((x|y)z)w where ((x|y)z) is ren1 and w is ren2).
     */
    for (; (next = ren1->next) != NULL && next != oldnext; ren1 = next) {
	if (REOP(ren1) == REOP_ALT) {
	    /* Find the end of this alternative's operand list. */
	    kid = (RENode*) ren1->kid;
	    if (REOP(kid) == REOP_JUMP)
		continue;
	    for (ren = kid; ren->next; ren = ren->next)
		JS_ASSERT(REOP(ren) != REOP_ALT);

	    /* Append a jump node to all but the last alternative. */
	    ren->next = NewRENode(state, REOP_JUMP, NULL);
	    if (!ren->next)
		return JS_FALSE;
	    ren->next->flags |= RENODE_ISNEXT;
	    ren->flags |= RENODE_GOODNEXT;

	    /* Recur to fix all descendent nested alternatives. */
	    if (!FixNext(state, kid, ren2, oldnext))
		return JS_FALSE;
	}
    }

    /*
     * Now ren1 points to the last alternative, or to the final node on a
     * concatenation list.  Set its next link to ren2, flagging a join point
     * if appropriate.
     */
    if (ren2) {
	if (!(ren2->flags & RENODE_ISNEXT))
	    ren2->flags |= RENODE_ISNEXT;
	else
	    ren2->flags |= RENODE_ISJOIN;
    }
    ren1->next = ren2;
    if (goodnext)
	ren1->flags |= RENODE_GOODNEXT;

    /*
     * The following ops have a kid subtree through which to recur.  Here is
     * where we fix the next links under the final ALT node's kid.
     */
    switch (REOP(ren1)) {
      case REOP_ALT:
      case REOP_QUANT:
      case REOP_STAR:
      case REOP_PLUS:
      case REOP_OPT:
      case REOP_LPAREN:
      case REOP_LPARENNON:
      case REOP_ASSERT:
      case REOP_ASSERT_NOT:
	if (!FixNext(state, (RENode*) ren1->kid, ren2, oldnext))
	    return JS_FALSE;
	break;
      default:;
    }
    return JS_TRUE;
}

static JSBool
SetNext(CompilerState *state, RENode *ren1, RENode *ren2)
{
    return FixNext(state, ren1, ren2, NULL);
}

/*
 * Parser forward declarations.
 */
typedef RENode *REParser(CompilerState *state);

static REParser ParseRegExp;
static REParser ParseAltern;
static REParser ParseItem;
static REParser ParseQuantAtom;
static REParser ParseAtom;

/*
 * Top-down regular expression grammar, based closely on Perl4.
 *
 *  regexp:     altern                  A regular expression is one or more
 *              altern '|' regexp       alternatives separated by vertical bar.
 */
static RENode *
ParseRegExp(CompilerState *state)
{
    RENode *ren, *kid, *ren1, *ren2;
    const jschar *cp;

    ren = ParseAltern(state);
    if (!ren)
	return NULL;
    cp = state->cp;
    if ((cp < state->cpend) && (*cp == '|')) {
	kid = ren;
	ren = NewRENode(state, REOP_ALT, kid);
	if (!ren)
	    return NULL;
	ren->flags = kid->flags & (RENODE_ANCHORED | RENODE_NONEMPTY);
	ren1 = ren;
	do {
	    /* (balance: */
	    state->cp = ++cp;
	    if (*cp == '|' || *cp == ')') {
		kid = NewRENode(state, REOP_EMPTY, NULL);
	    } else {
		kid = ParseAltern(state);
		cp = state->cp;
	    }
	    if (!kid)
		return NULL;
	    ren2 = NewRENode(state, REOP_ALT, kid);
	    if (!ren2)
		return NULL;
	    ren1->next = ren2;
	    ren1->flags |= RENODE_GOODNEXT;
	    ren2->flags = (kid->flags & (RENODE_ANCHORED | RENODE_NONEMPTY))
			  | RENODE_ISNEXT;
	    ren1 = ren2;
	} while ((cp < state->cpend) && (*cp == '|'));
    }
    return ren;
}

/*
 *  altern:     item                    An alternative is one or more items,
 *              item altern             concatenated together.
 */
static RENode *
ParseAltern(CompilerState *state)
{
    RENode *ren, *ren1, *ren2;
    uintN flags;
    const jschar *cp;
    jschar c;

    ren = ren1 = ParseItem(state);
    if (!ren)
	return NULL;
    flags = 0;
    cp = state->cp;
    /* (balance: */
    while ((cp < state->cpend) && (c = *cp) != '|' && c != ')') {
	ren2 = ParseItem(state);
	if (!ren2)
	    return NULL;
	if (!SetNext(state, ren1, ren2))
	    return NULL;
	flags |= ren2->flags;
	ren1 = ren2;
	cp = state->cp;
    }

    /*
     * Propagate NONEMPTY to the front of a concatenation list, so that the
     * first alternative in (^a|b) is considered non-empty.  The first node
     * in a list may match the empty string (as ^ does), but if the list is
     * non-empty, then the first node's NONEMPTY flag must be set.
     */
    ren->flags |= flags & RENODE_NONEMPTY;
    return ren;
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
 */
static RENode *
ParseItem(CompilerState *state)
{
    const jschar *cp;
    RENode *ren;
    REOp op;

    cp = state->cp;
    if (cp < state->cpend)
        switch (*cp) {
          case '^':
	    state->cp = cp + 1;
	    ren = NewRENode(state, REOP_BOL, NULL);
	    if (ren)
	        ren->flags |= RENODE_ANCHORED;
	    return ren;

          case '$':
	    state->cp = cp + 1;
	    return NewRENode(state,
			     (cp == state->cpbegin ||
			      ((cp[-1] == '(' || cp[-1] == '|') && /*balance)*/
			       (cp - 1 == state->cpbegin || cp[-2] != '\\')))
			     ? REOP_EOLONLY
			     : REOP_EOL,
			     NULL);

          case '\\':
	    switch (*++cp) {
	      case 'b':
	        op = REOP_WBDRY;
	        break;
	      case 'B':
	        op = REOP_WNONBDRY;
	        break;
	      default:
	        return ParseQuantAtom(state);
	    }

	    /*
	     * Word boundaries and non-boundaries are flagged as non-empty so they
	     * will be prefixed by an anchoring node.
	     */
	    state->cp = cp + 1;
	    ren = NewRENode(state, op, NULL);
	    if (ren)
	        ren->flags |= RENODE_NONEMPTY;
	    return ren;

          default:;
        }
    return ParseQuantAtom(state);
}

/*
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
*/
static RENode *
ParseQuantAtom(CompilerState *state)
{
    RENode *ren, *ren2;
    const jschar *cp, *up;
    jschar c;
    uint32 min, max;

    ren = ParseAtom(state);
    if (!ren)
	return NULL;

    cp = state->cp;
loop:
    if (cp < state->cpend)
        switch (*cp) {
          case '{':
	    c = *++cp;
	    if (!JS7_ISDEC(c)) {
                js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                            NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_QUANTIFIER, state->cp);
	        return NULL;
	    }
	    min = (uint32)JS7_UNDEC(c);
	    for (c = *++cp; JS7_ISDEC(c); c = *++cp) {
	        min = 10 * min + (uint32)JS7_UNDEC(c);
	        if (min >> 16) {
                    js_ReportCompileErrorNumber(state->context,
                                                state->tokenStream, NULL,
                                                JSREPORT_ERROR,
                                                JSMSG_MIN_TOO_BIG, state->cp);
		    return NULL;
	        }
	    }
	    if (*cp == ',') {
	        up = ++cp;
	        if (JS7_ISDEC(*cp)) {
		    max = (uint32)JS7_UNDEC(*cp);
		    for (c = *++cp; JS7_ISDEC(c); c = *++cp) {
		        max = 10 * max + (uint32)JS7_UNDEC(c);
		        if (max >> 16) {
                            js_ReportCompileErrorNumber(state->context,
                                                        state->tokenStream,
                                                        NULL,
                                                        JSREPORT_ERROR,
                                                        JSMSG_MAX_TOO_BIG, up);
			    return NULL;
		        }
		    }
		    if (max == 0)
		        goto zero_quant;
		    if (min > max) {
                        js_ReportCompileErrorNumber(state->context,
                                                    state->tokenStream,
                                                    NULL,
                                                    JSREPORT_ERROR,
                                                    JSMSG_OUT_OF_ORDER, up);
		        return NULL;
		    }
	        } else {
		    /* 0 means no upper bound. */
		    max = 0;
	        }
	    } else {
	        /* Exactly n times. */
	        if (min == 0) {
          zero_quant:
                    js_ReportCompileErrorNumber(state->context,
                                                state->tokenStream,
                                                NULL,
                                                JSREPORT_ERROR,
                                                JSMSG_ZERO_QUANTIFIER,
                                                state->cp);
		    return NULL;
	        }
	        max = min;
	    }
	    if (*cp != '}') {
                js_ReportCompileErrorNumber(state->context,
                                            state->tokenStream,
                                            NULL,
                                            JSREPORT_ERROR,
                                            JSMSG_UNTERM_QUANTIFIER, state->cp);
	        return NULL;
	    }
	    cp++;

	    ren2 = NewRENode(state, REOP_QUANT, ren);
	    if (!ren2)
	        return NULL;
	    if (min > 0 && (ren->flags & RENODE_NONEMPTY))
	        ren2->flags |= RENODE_NONEMPTY;
	    ren2->u.range.min = (uint16)min;
	    ren2->u.range.max = (uint16)max;
	    ren = ren2;
            goto parseMinimalFlag;

          case '*':
	    cp++; 
	    ren = NewRENode(state, REOP_STAR, ren);
    parseMinimalFlag :
            if (*cp == '?') {
                ren->flags |= RENODE_MINIMAL;
                cp++;
            }
	    goto loop;

          case '+':
	    cp++;
	    ren2 = NewRENode(state, REOP_PLUS, ren);
	    if (!ren2)
	        return NULL;
	    if (ren->flags & RENODE_NONEMPTY)
	        ren2->flags |= RENODE_NONEMPTY;
	    ren = ren2;
	    goto parseMinimalFlag;

          case '?':
	    cp++;
	    ren = NewRENode(state, REOP_OPT, ren);
	    goto parseMinimalFlag;
        }

    state->cp = cp;
    return ren;
}

/*
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
 *              '\c' ctrl               A control character, ctrl is a letter.
 *              '\' literalatomchar     Any character except one of the above
 *                                      that follow '\' in an atom.
 *              otheratomchar           Any character not first among the other
 *                                      atom right-hand sides.
 */
static jschar metachars[] = {
    '|', '^', '$', '{', '*', '+', '?', '(', ')', '.', '[', '\\', '}', 0
};

static jschar closurechars[] = {
    '{', '*', '+', '?', 0	/* balance} */
};

static RENode *
ParseAtom(CompilerState *state)
{
    const jschar *cp, *ocp;
    uintN tmp, num, len;
    RENode *ren, *ren2;
    jschar c;
    REOp op;

    cp = ocp = state->cp;

        /* handle /|a/ by returning an empty node for the leftside */
    if ((cp == state->cpend) || (*cp == '|'))
        return NewRENode(state, REOP_EMPTY, NULL);

    ren = NULL; /* suppress warning */
    switch (*cp) {
      case '(':
        num = -1;   /* suppress warning */
        op = REOP_END;
        if (cp[1] == '?') {
            switch (cp[2]) {
                case ':' :
                    op = REOP_LPARENNON;
                    break;
                case '=' :
                    op = REOP_ASSERT;
                    break;
                case '!' :
                    op = REOP_ASSERT_NOT;
                    break;
            }
        }
        if (op == REOP_END) {
            num = state->parenCount++;	/* \1 is numbered 0, etc. */
            op = REOP_LPAREN;
            cp++;
        }
        else
            cp += 3;
        state->cp = cp;
        /* Handle empty paren */
        if (*cp == ')') {
            ren2 = NewRENode(state, REOP_EMPTY, NULL);
        }
        else {
            ren2 = ParseRegExp(state);
            if (!ren2)
                return NULL;
            cp = state->cp;
            if (*cp != ')') {
                js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                            NULL,
                                            JSREPORT_ERROR,
                                            JSMSG_MISSING_PAREN, ocp);
                return NULL;
            }
        }
        cp++;
        ren = NewRENode(state, op, ren2);
        if (!ren)
	    return NULL;
        ren->flags = ren2->flags & (RENODE_ANCHORED | RENODE_NONEMPTY);
        ren->u.num = num;
        if ((op == REOP_LPAREN) || (op == REOP_LPARENNON)) {
                /* Assume RPAREN ops immediately succeed LPAREN ops */
            ren2 = NewRENode(state, (REOp)(op + 1), NULL);
            if (!ren2 || !SetNext(state, ren, ren2))
                return NULL;
            ren2->u.num = num;
        }
        break;

      case '.':
	cp++;
        op = REOP_DOT;
        if (*cp == '*') {
	    cp++;
            op = REOP_DOTSTAR;
            if (*cp == '?') {
                cp++;
                op = REOP_DOTSTARMIN;
            }
        }
	ren = NewRENode(state, op, NULL);
	if (ren && op == REOP_DOT)
	    ren->flags = RENODE_SINGLE | RENODE_NONEMPTY;
	break;

      case '[':
        ++cp;
        ren = NewRENode(state, REOP_CCLASS, (void *)cp);
        if (!ren)
            return NULL;

        do {
            if (cp == state->cpend) {
                js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                            NULL,
                                            JSREPORT_ERROR,
                                            JSMSG_UNTERM_CLASS, ocp);
                return NULL;
            }
            if ((c = *++cp) == ']')
                break;
            if (c == '\\' && (cp+1 != state->cpend))
                cp++;
        } while (JS_TRUE);
        ren->u.kid2 = (void *)cp++;
        
        ren->u.ucclass.bitmap = NULL;

        /* Since we rule out [] and [^], we can set the non-empty flag. */
	ren->flags = RENODE_SINGLE | RENODE_NONEMPTY;
	break;

      case '\\':
	c = *++cp;
        if (cp == state->cpend) {
            js_ReportCompileErrorNumber(state->context, state->tokenStream,
                                        NULL,
                                        JSREPORT_ERROR,
                                        JSMSG_TRAILING_SLASH);
	    return NULL;
        }
	switch (c) {
	  case 'f':
	  case 'n':
	  case 'r':
	  case 't':
	  case 'v':
	    ren = NewRENode(state, REOP_FLAT1, NULL);
	    c = js_strchr(js_EscapeMap, c)[-1];
	    break;

	  case 'd':
	    ren = NewRENode(state, REOP_DIGIT, NULL);
	    break;
	  case 'D':
	    ren = NewRENode(state, REOP_NONDIGIT, NULL);
	    break;
	  case 'w':
	    ren = NewRENode(state, REOP_ALNUM, NULL);
	    break;
	  case 'W':
	    ren = NewRENode(state, REOP_NONALNUM, NULL);
	    break;
	  case 's':
	    ren = NewRENode(state, REOP_SPACE, NULL);
	    break;
	  case 'S':
	    ren = NewRENode(state, REOP_NONSPACE, NULL);
	    break;
	  case '0':
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':
            /*
                Yuk. Keeping the old style \n interpretation for 1.2
                compatibility.
            */
            if (state->context->version != JSVERSION_DEFAULT &&
			      state->context->version <= JSVERSION_1_4) {
                switch (c) {
	          case '0':
do_octal:
	            num = 0;
	            while ('0' <= (c = *++cp) && c <= '7') {
		        tmp = 8 * num + (uintN)JS7_UNDEC(c);
		        if (tmp > 0377)
		            break;
		        num = tmp;
	            }
	            cp--;
	            ren = NewRENode(state, REOP_FLAT1, NULL);
	            c = (jschar)num;
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
	            num = (uintN)JS7_UNDEC(c);
	            tmp = 1;
                    for (c = *++cp; JS7_ISDEC(c); c = *++cp, tmp++)
		        num = 10 * num + (uintN)JS7_UNDEC(c);
                    /* n in [8-9] and > count of parenetheses, then revert to
                    '8' or '9', ignoring the '\' */
                    if (((num == 8) || (num == 9)) && (num > state->parenCount)) {
	                ocp = --cp; /* skip beyond the '\' */
                        goto do_flat;
                    }
                    /* more than 1 digit, or a number greater than
                        the count of parentheses => it's an octal */
                    if ((tmp > 1) || (num > state->parenCount)) {
		        cp = ocp;
		        goto do_octal;
	            }
	            cp--;
	            ren = NewRENode(state, REOP_BACKREF, NULL);
	            if (!ren)
		        return NULL;
	            ren->u.num = num - 1;	/* \1 is numbered 0, etc. */

	            /* Avoid common chr- and flags-setting code after switch. */
	            ren->flags = RENODE_NONEMPTY;
	            goto bump_cp;
                }
            }
            else {
                if (c == '0') {
            	    ren = NewRENode(state, REOP_FLAT1, NULL);
                    c = 0;
                }
                else {
                    num = (uintN)JS7_UNDEC(c);
                    for (c = *++cp; JS7_ISDEC(c); c = *++cp)
		        num = 10 * num + (uintN)JS7_UNDEC(c);
                    cp--;
	            ren = NewRENode(state, REOP_BACKREF, NULL);
	            if (!ren)
		        return NULL;
	            ren->u.num = num - 1;	/* \1 is numbered 0, etc. */
	            /* Avoid common chr- and flags-setting code after switch. */
	            ren->flags = RENODE_NONEMPTY;
	            goto bump_cp;
                }
            }
            break;

	  case 'x':
	    ocp = cp;
	    c = *++cp;
	    if (JS7_ISHEX(c)) {
		num = JS7_UNHEX(c);
		c = *++cp;
		if (JS7_ISHEX(c)) {
		    num <<= 4;
		    num += JS7_UNHEX(c);
		} else {
                    if (state->context->version != JSVERSION_DEFAULT &&
			      state->context->version <= JSVERSION_1_4)
		        cp--;	/* back up so cp points to last hex char */
                    else
                        goto nothex; /* ECMA 2 insists on pairs */
		}
	    } else {
nothex:
		cp = ocp;	/* \xZZ is xZZ (Perl does \0ZZ!) */
		num = 'x';
	    }
	    ren = NewRENode(state, REOP_FLAT1, NULL);
	    c = (jschar)num;
	    break;

	  case 'c':
	    c = *++cp;
	    if (!JS7_ISLET(c)) {
		cp -= 2;
		ocp = cp;
		goto do_flat;
	    }
	    c = JS_TOUPPER(c);
	    c = JS_TOCTRL(c);
	    ren = NewRENode(state, REOP_FLAT1, NULL);
	    break;

	  case 'u':
	    if (JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]) &&
		JS7_ISHEX(cp[3]) && JS7_ISHEX(cp[4])) {
		c = (((((JS7_UNHEX(cp[1]) << 4) + JS7_UNHEX(cp[2])) << 4)
		      + JS7_UNHEX(cp[3])) << 4) + JS7_UNHEX(cp[4]);
		cp += 4;
		ren = NewRENode(state, REOP_FLAT1, NULL);
		break;
	    }

	    /* Unlike Perl \xZZ, we take \uZZZ to be literal-u then ZZZ. */
	    ocp = cp;
	    goto do_flat;

	  default:
	    ocp = cp;
	    goto do_flat;
	}

	/* Common chr- and flags-setting code for escape opcodes. */
	if (ren) {
	    ren->u.chr = c;
	    ren->flags = RENODE_SINGLE | RENODE_NONEMPTY;
	}

      bump_cp:
	/* Skip to next unparsed char. */
	cp++;
	break;

      default:

      do_flat:
	while ((c = *++cp) && (cp != state->cpend) && !js_strchr(metachars, c))
	    ;
	len = (uintN)(cp - ocp);
	if ((cp != state->cpend) && len > 1 && js_strchr(closurechars, c)) {
	    cp--;
	    len--;
	}
	if (len > REOP_FLATLEN_MAX) {
	    len = REOP_FLATLEN_MAX;
	    cp = ocp + len;
	}
	ren = NewRENode(state, (len == 1) ? REOP_FLAT1 : REOP_FLAT,
			(void *)ocp);
	if (!ren)
	    return NULL;
	ren->flags = RENODE_NONEMPTY;
	if (len > 1) {
	    ren->u.kid2 = (void *)cp;
	} else {
	    ren->flags |= RENODE_SINGLE;
	    ren->u.chr = *ocp;
	}
	break;
    }

    state->cp = cp;
    return ren;
}

JSRegExp *
js_NewRegExp(JSContext *cx, JSTokenStream *ts,
             JSString *str, uintN flags, JSBool flat)
{
    JSRegExp *re;
    void *mark;
    CompilerState state;
    RENode *ren, *ren2, *end;
    size_t resize;

    const jschar *cp;
    size_t len;

    re = NULL;
    mark = JS_ARENA_MARK(&cx->tempPool);

    state.context = cx;
    state.tokenStream = ts;
    state.cpbegin = state.cp = JSSTRING_CHARS(str);
    state.cpend = state.cp + JSSTRING_LENGTH(str);
    state.flags = flags;
    state.parenCount = 0;
    state.progLength = 0;

    len = JSSTRING_LENGTH(str);
    if (len != 0 && flat) {
        cp = state.cpbegin;
        ren = NULL;
        do {
            ren2 = NewRENode(&state, 
                             (len == 1) ? REOP_FLAT1 : REOP_FLAT,
                             (void *)cp);
	    if (!ren2)
	        goto out;
	    ren2->flags = RENODE_NONEMPTY;
	    if (len > 1) {
                if (len > REOP_FLATLEN_MAX) {
                    cp += REOP_FLATLEN_MAX;
                    len -= REOP_FLATLEN_MAX;
                }
                else {
                    cp += len;
                    len = 0;
                }
	        ren2->u.kid2 = (void *)cp;                
	    } else {
	        ren2->flags |= RENODE_SINGLE;
	        ren2->u.chr = *cp;
                len = 0;
            }
            if (ren) {
                if (!SetNext(&state, ren, ren2))
                    goto out;
            }
            else
                ren = ren2;
        } while (len > 0);
    }
    else
        ren = ParseRegExp(&state);
    if (!ren)
	goto out;

    end = NewRENode(&state, REOP_END, NULL);
    if (!end || !SetNext(&state, ren, end))
	goto out;

#ifdef DEBUG_notme
    if (!DumpRegExp(cx, ren))
	goto out;
#endif

    resize = sizeof *re + state.progLength - 1;
    re = (JSRegExp *) JS_malloc(cx, JS_ROUNDUP(resize, sizeof(jsword)));
    if (!re)
	goto out;
    re->nrefs = 1;
    re->source = str;
    re->lastIndex = 0;
    re->parenCount = state.parenCount;
    re->flags = flags;

    re->ren = ren;

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
	      default: {
		char charBuf[2] = " ";
		charBuf[0] = (char)s[i];
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_FLAG, charBuf);
		return NULL;
	      }
	    }
	}
    }
    return js_NewRegExp(cx, ts, str, flags, flat);
}

static void freeRENtree(JSContext *cx, RENode *ren,  RENode *stop)
{
    while (ren && (ren != stop)) {
        RENode *n;
        switch (ren->op) {
            case REOP_ALT: {
                        RENode *altStop = (RENode *)(ren->next);
                        while (altStop && (altStop->op == REOP_ALT))
                            altStop = altStop->next;
                        freeRENtree(cx, (RENode *)(ren->kid), altStop);
                    }
                    break;
            case REOP_QUANT:
            case REOP_PLUS:
            case REOP_STAR:
            case REOP_OPT:
            case REOP_LPAREN:
            case REOP_LPARENNON:
            case REOP_ASSERT:
            case REOP_ASSERT_NOT:
                freeRENtree(cx, (RENode *)(ren->kid), (RENode *)(ren->next));
                break;
            case REOP_CCLASS:
                if (ren->u.ucclass.bitmap)
                    JS_free(cx, ren->u.ucclass.bitmap);
                break;
        }
        n = ren->next;
        JS_free(cx, ren);
        ren = n;
    }
}

#define HOLD_REGEXP(cx, re) JS_ATOMIC_INCREMENT(&(re)->nrefs)
#define DROP_REGEXP(cx, re) js_DestroyRegExp(cx, re)

void
js_DestroyRegExp(JSContext *cx, JSRegExp *re)
{
    if (JS_ATOMIC_DECREMENT(&re->nrefs) == 0) {
        freeRENtree(cx, re->ren, NULL);
        JS_free(cx, re);
    }
}

typedef struct MatchState {
    JSContext       *context;           /* for access to regExpStatics */
    JSBool          anchoring;          /* true if multiline anchoring ^/$ */
    jsbytecode      *pcend;             /* pc limit (fencepost) */
    const jschar    *cpbegin, *cpend;   /* cp base address and limit */
    size_t          start;              /* offset from cpbegin to start at */
    ptrdiff_t       skipped;            /* chars skipped anchoring this r.e. */
    uint8           flags;              /* flags */
    uintN           parenCount;         /* number of paren substring matches */
    JSSubString     *maybeParens;       /* possible paren substring pointers */
    JSSubString     *parens;            /* certain paren substring matches */
    JSBool          ok;                 /* indicates runtime error during matching */
} MatchState;

/*
 * Returns updated cp on match, null on mismatch.
 */

static JSBool matchChar(int flags, jschar c, jschar c2)
{
    if (c == c2)
        return JS_TRUE;
    else
        if ((flags & JSREG_FOLD) != 0) {
            return ((c = JS_TOUPPER(c)) == (c2 = JS_TOUPPER(c2)))
                    || (JS_TOLOWER(c) == JS_TOLOWER(c2));
        }
        else
            return JS_FALSE;
}

static const jschar *matchRENodes(MatchState *state, RENode *ren, RENode *stop,
                                                const jschar *cp);

typedef struct {
    MatchState *state;
    RENode *kid;
    RENode *next;
    RENode *stop;
    int kidCount;
    int maxKid;
} GreedyState;


static const jschar *greedyRecurse(GreedyState *grState, const jschar *cp, const jschar *previousKid)
{
    const jschar *kidMatch;
    const jschar *match;
    int num = grState->state->parenCount;

#ifdef XP_MAC
    if (StackSpace() < 16384) {
        JS_ReportOutOfMemory (grState->state->context);
        grState->state->ok = JS_FALSE;
        return NULL;
    }
#endif

/*
*    When the kid match fails, we reset the parencount and run any 
*    previously succesful kid in order to restablish it's paren
*    contents.
*/

    kidMatch = matchRENodes(grState->state, grState->kid, grState->next, cp);
    if (kidMatch == NULL) {        
        match = matchRENodes(grState->state, grState->next, grState->stop, cp);
        if (match) {
            grState->state->parenCount = num;
            if (previousKid != NULL)
                matchRENodes(grState->state, grState->kid, grState->next, previousKid);
            return cp;
        }
            return NULL;
    }
    else {
        if (kidMatch == cp) {
            if (previousKid != NULL)
                matchRENodes(grState->state, grState->kid, grState->next, previousKid);
            return kidMatch;    /* no point pursuing an empty match forever */
        }
        if (!grState->maxKid || (++grState->kidCount < grState->maxKid)) {
            match = greedyRecurse(grState, kidMatch, cp);
            if (match)
                return match;
            if (grState->maxKid)
            --grState->kidCount;
        }
        grState->state->parenCount = num;
        if (matchRENodes(grState->state, grState->next, grState->stop, kidMatch)) {
            matchRENodes(grState->state, grState->kid, grState->next, cp);
            return kidMatch;
        }
            return NULL;
    }
}

static const jschar *matchGreedyKid(MatchState *state, RENode *ren, RENode *stop,
                        int kidCount, const jschar *cp, const jschar *previousKid)
{
    GreedyState grState;
    const jschar *match;
    grState.state = state;
    grState.kid = (RENode *)ren->kid;
    grState.next = ren->next;
    grState.kidCount = kidCount;
    grState.maxKid = (ren->op == REOP_QUANT) ? ren->u.range.max : 0;
    /*
        We try to match the sub-tree to completion first, and if that
        doesn't work, match only up to the given end of the sub-tree.
    */
    grState.stop = NULL;
    match = greedyRecurse(&grState, cp, previousKid);
    if (match || !stop) 
        return match;
    grState.kidCount = kidCount;
    grState.stop = stop;
    return greedyRecurse(&grState, cp, previousKid);
}


static const jschar *matchNonGreedyKid(MatchState *state, RENode *ren,
                        int kidCount, int maxKid,
                        const jschar *cp)
{
    const jschar *kidMatch;
    const jschar *match;

    match = matchRENodes(state, ren->next, NULL, cp);
    if (match != NULL) return cp;
    kidMatch = matchRENodes(state, (RENode *)ren->kid, ren->next, cp);
    if (kidMatch == NULL)
        return NULL;
    if (kidMatch == cp)
        return kidMatch;    /* no point pursuing an empty match forever */
        return matchNonGreedyKid(state, ren, kidCount, maxKid, kidMatch);
    }

static void calcBMSize(MatchState *state, RENode *ren)
{
    const jschar *cp  = (const jschar *) ren->kid;
    const jschar *cp2 = (const jschar *) ren->u.kid2;
    uintN maxc = 0;
    jschar c, c2;
    while (cp < cp2) {
	c = *cp++;
	if (c == '\\') {
	    if (cp + 5 <= cp2 && *cp == 'u' &&
		JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]) &&
		JS7_ISHEX(cp[3]) && JS7_ISHEX(cp[4])) {
		c = (((((JS7_UNHEX(cp[1]) << 4)
			+ JS7_UNHEX(cp[2])) << 4)
		      + JS7_UNHEX(cp[3])) << 4)
		    + JS7_UNHEX(cp[4]);
		cp += 5;
	    } else {
                if (maxc < 255) maxc = 255;
        	/*
		 * Octal and hex escapes can't be > 255.  Skip this
		 * backslash and let the loop pass over the remaining
		 * escape sequence as if it were text to match.
		 */
		continue;
	    }
	}
	if (state->flags & JSREG_FOLD) {
	    /*
	     * Don't assume that lowercase are above uppercase, or
	     * that c is either even when c has upper and lowercase
	     * versions.
	     */
	    if ((c2 = JS_TOUPPER(c)) > maxc)
		maxc = c2;
	    if ((c2 = JS_TOLOWER(c2)) > maxc)
		maxc = c2;
	}
	if (c > maxc)
	    maxc = c;
    }
    ren->u.ucclass.bmsize = (uint16)((size_t)(maxc + JS_BITS_PER_BYTE) 
                                                    / JS_BITS_PER_BYTE);
}

static JSBool buildBitmap(MatchState *state, RENode *ren)
{
    uintN   i, n, b, c, lastc, foldc, nchars;
    uint8   *bitmap;
    uint8   fill;
    JSBool  inrange;
    const jschar *cp = (const jschar *) ren->kid;
    const jschar *end = (const jschar *) ren->u.kid2;
    const jschar *ocp;

    calcBMSize(state, ren);
    ren->u.ucclass.bitmap = bitmap = (uint8*) JS_malloc(state->context, 
                                            ren->u.ucclass.bmsize);
    if (!bitmap) {
	JS_ReportOutOfMemory(state->context);
	return JS_FALSE;
    }

    if (*cp == '^') {
	fill = 0xff;
	cp++;
    } else {
	fill = 0;
    }

    n = (uintN)ren->u.ucclass.bmsize;
    for (i = 0; i < n; i++)
	bitmap[i] = fill;
    nchars = n * JS_BITS_PER_BYTE;

/* Split ops up into statements to keep MSVC1.52 from crashing. */
#define MATCH_BIT(c)    { i = (c) >> 3; b = (c) & 7; b = 1 << b;              \
		  if (fill) bitmap[i] &= ~b; else bitmap[i] |= b; }

    lastc = nchars;
    inrange = JS_FALSE;

    while (cp < end) {
	c = (uintN) *cp++;
	if (c == '\\') {
	    c = *cp++;
	    switch (c) {
	      case 'b':
	      case 'f':
	      case 'n':
	      case 'r':
	      case 't':
	      case 'v':
		c = js_strchr(js_EscapeMap, (jschar)c)[-1];
		break;

#define CHECK_RANGE() if (inrange) { MATCH_BIT(lastc); MATCH_BIT('-');        \
			     inrange = JS_FALSE; }

	      case 'd':
		CHECK_RANGE();
		for (c = '0'; c <= '9'; c++)
		    MATCH_BIT(c);
		continue;

	      case 'D':
		CHECK_RANGE();
		for (c = 0; c < '0'; c++)
		    MATCH_BIT(c);
		for (c = '9' + 1; c < nchars; c++)
		    MATCH_BIT(c);
		continue;

	      case 'w':
		CHECK_RANGE();
		for (c = 0; c < nchars; c++)
		    if (JS_ISWORD(c))
			MATCH_BIT(c);
		continue;

	      case 'W':
		CHECK_RANGE();
		for (c = 0; c < nchars; c++)
		    if (!JS_ISWORD(c))
			MATCH_BIT(c);
		continue;

	      case 's':
		CHECK_RANGE();
		for (c = 0; c < nchars; c++)
		    if (JS_ISSPACE(c))
			MATCH_BIT(c);
		continue;

	      case 'S':
		CHECK_RANGE();
		for (c = 0; c < nchars; c++)
		    if (!JS_ISSPACE(c))
			MATCH_BIT(c);
		continue;

#undef CHECK_RANGE

	      case '0':
	      case '1':
	      case '2':
	      case '3':
	      case '4':
	      case '5':
	      case '6':
	      case '7':
		n = JS7_UNDEC(c);
		ocp = cp - 2;
		c = *cp;
		if ('0' <= c && c <= '7') {
		    cp++;
		    n = 8 * n + JS7_UNDEC(c);

		    c = *cp;
		    if ('0' <= c && c <= '7') {
			cp++;
			i = 8 * n + JS7_UNDEC(c);
			if (i <= 0377)
			    n = i;
			else
			    cp--;
		    }
		}
		c = n;
		break;

	      case 'x':
		ocp = cp;
		c = *cp++;
		if (JS7_ISHEX(c)) {
		    n = JS7_UNHEX(c);
		    c = *cp++;
		    if (JS7_ISHEX(c)) {
			n <<= 4;
			n += JS7_UNHEX(c);
		    }
		} else {
		    cp = ocp;	/* \xZZ is xZZ (Perl does \0ZZ!) */
		    n = 'x';
		}
		c = n;
		break;

	      case 'u':
		if (JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1]) &&
		    JS7_ISHEX(cp[2]) && JS7_ISHEX(cp[3])) {
		    n = (((((JS7_UNHEX(cp[0]) << 4)
			    + JS7_UNHEX(cp[1])) << 4)
			  + JS7_UNHEX(cp[2])) << 4)
			+ JS7_UNHEX(cp[3]);
		    c = n;
		    cp += 4;
		}
		break;

	      case 'c':
		c = *cp++;
		c = JS_TOUPPER(c);
		c = JS_TOCTRL(c);
		break;
	    }
	}

	if (inrange) {
	    if (lastc > c) {
		JS_ReportErrorNumber(state->context,
				     js_GetErrorMessage, NULL,
				     JSMSG_BAD_CLASS_RANGE);
		return JS_FALSE;
	    }
	    inrange = JS_FALSE;
	} else {
	    /* Set lastc so we match just c's bit in the for loop. */
	    lastc = c;

	    /* [balance: */
	    if (*cp == '-' && cp + 1 < end && cp[1] != ']') {
		cp++;
		inrange = JS_TRUE;
		continue;
	    }
	}

	/* Match characters in the range [lastc, c]. */
	for (; lastc <= c; lastc++) {
	    MATCH_BIT(lastc);
	    if (state->flags & JSREG_FOLD) {
		/*
		 * Must do both upper and lower for Turkish dotless i,
		 * Georgian, etc.
		 */
		foldc = JS_TOUPPER(lastc);
		MATCH_BIT(foldc);
		foldc = JS_TOLOWER(foldc);
		MATCH_BIT(foldc);
	    }
	}
	lastc = c;
    }
    return JS_TRUE;
}

static const jschar *matchRENodes(MatchState *state, RENode *ren, RENode *stop,
                                                const jschar *cp)
{
    const jschar *cp2, *kidMatch, *lastKid, *source, *cpend = state->cpend;
    jschar c;
    JSSubString *parsub;
    uintN i, b, bit, num, length;

    while ((ren != stop) && (ren != NULL))
    {
      switch (ren->op) {
        case REOP_EMPTY:
          break;
        case REOP_ALT:
          if (ren->next->op != REOP_ALT) {
              ren = (RENode *)ren->kid;
              continue;
          }
          num = state->parenCount;
          kidMatch = matchRENodes(state, (RENode *)ren->kid, stop, cp);
          if (kidMatch != NULL)
              return kidMatch;
          for (i = num; i < state->parenCount; i++)
              state->parens[i].length = 0;
          state->parenCount = num;
          break;
        case REOP_QUANT:
          lastKid = NULL;
          for (num = 0; num < ren->u.range.min; num++) {
              kidMatch = matchRENodes(state, (RENode *)ren->kid,
                                          ren->next, cp);
              if (kidMatch == NULL)
                  return NULL;
              lastKid = cp;
              cp = kidMatch;
          }
          if (num == ren->u.range.max) break;
          if ((ren->flags & RENODE_MINIMAL) == 0) {
              cp2 = matchGreedyKid(state, ren, stop, num, cp, lastKid);
              if (cp2 == NULL) {
                  if (lastKid)
                      cp = matchRENodes(state, (RENode *)ren->kid, ren->next,
                                        lastKid);
              }
              else
                  cp = cp2;
          }
          else {
              cp = matchNonGreedyKid(state, ren, num,
                              ren->u.range.max, cp);
              if (cp == NULL)
                  return NULL;
          }
          break;
        case REOP_PLUS:
          kidMatch = matchRENodes(state, (RENode *)ren->kid,
                                          ren->next, cp);
          if (kidMatch == NULL)
              return NULL;
          if ((ren->flags & RENODE_MINIMAL) == 0) {
              cp2 = matchGreedyKid(state, ren, stop, 1, kidMatch, cp);
              if (cp2 == NULL)
                  cp = matchRENodes(state, (RENode *)ren->kid, ren->next, cp);
              else
                  cp = cp2;
          }
          else
              cp = matchNonGreedyKid(state, ren, 1, 0, kidMatch);
          if (cp == NULL)
              return NULL;
          break;
        case REOP_STAR:
          if ((ren->flags & RENODE_MINIMAL) == 0) {
              cp2 = matchGreedyKid(state, ren, stop, 0, cp, NULL);
              if (cp2)
                  cp = cp2;
          }
          else {
              cp = matchNonGreedyKid(state, ren, 0, 0, cp);
              if (cp == NULL)
                  return NULL;
          }
          break;
        case REOP_OPT:
          num = state->parenCount;
          if (ren->flags & RENODE_MINIMAL) {
              const jschar *restMatch = matchRENodes(state, ren->next,
                                            stop, cp);
              if (restMatch != NULL)
                  return restMatch;
          }
          kidMatch = matchRENodes(state, (RENode *)ren->kid,
                                        ren->next, cp);
          if (kidMatch == NULL) {
              state->parenCount = num;
              break;
          }
          else {
              const jschar *restMatch = matchRENodes(state, ren->next,
                                            stop, kidMatch);
              if (restMatch == NULL) {
                    /* need to undo the result of running the kid */
                  state->parenCount = num;
                  break;
              }
              else
                  return restMatch;
          }
        case REOP_LPARENNON:
          ren = (RENode *)ren->kid;
          continue;
        case REOP_RPARENNON:
          break;
        case REOP_LPAREN:
          num = ren->u.num;
          ren = (RENode *)ren->kid;
	  parsub = &state->parens[num];
	  parsub->chars = cp;
          parsub->length = 0;
          if (num >= state->parenCount)
              state->parenCount = num + 1;
          continue;
        case REOP_RPAREN:
          num = ren->u.num;
          parsub = &state->parens[num];
          parsub->length = cp - parsub->chars;
          break;
        case REOP_ASSERT:
          kidMatch = matchRENodes(state, (RENode *)ren->kid,
                                            ren->next, cp);
          if (kidMatch == NULL)
              return NULL;
          break;
        case REOP_ASSERT_NOT:
          kidMatch = matchRENodes(state, (RENode *)ren->kid,
                                            ren->next, cp);
          if (kidMatch != NULL)
              return NULL;
          break;
        case REOP_BACKREF:
          num = ren->u.num;
          if (num >= state->parenCount) {
	    JS_ReportErrorNumber(state->context, js_GetErrorMessage, NULL,
				 JSMSG_BAD_BACKREF);
            state->ok = JS_FALSE;
            return NULL;
          }
          parsub = &state->parens[num];
          if ((cp + parsub->length) > cpend)
              return NULL;
          else {
              cp2 = parsub->chars;
              for (i = 0; i < parsub->length; i++) {
                  if (!matchChar(state->flags, *cp++, *cp2++))
                      return NULL;
              }
          }
          break;
        case REOP_CCLASS:
          if (cp != cpend) {
              if (ren->u.ucclass.bitmap == NULL) {
                  if (!buildBitmap(state, ren)) {
                      state->ok = JS_FALSE;
                      return NULL;
                  }
              }
              c = *cp;
              b = (c >> 3);
              if (b >= ren->u.ucclass.bmsize) {
                  cp2 = (jschar*) ren->kid;
                  if (*cp2 == '^')
                      cp++;
                  else
                      return NULL;
              }
              else {
                  bit = c & 7;
                  bit = 1 << bit;
                  if ((ren->u.ucclass.bitmap[b] & bit) != 0)
                      cp++;
                  else
                      return NULL;
              }
          }
          else
              return NULL;
          break;
        case REOP_DOT:
          if ((cp != cpend) && (*cp != '\n'))
              cp++;
          else
              return NULL;
          break;
        case REOP_DOTSTARMIN:
          for (cp2 = cp; cp2 < cpend; cp2++) {
              const jschar *cp3 = matchRENodes(state, ren->next, stop, cp2);
              if (cp3 != NULL)
                  return cp3;
              if (*cp2 == '\n')
                  return NULL;
          }
          return NULL;
        case REOP_DOTSTAR:
          for (cp2 = cp; cp2 < cpend; cp2++)
              if (*cp2 == '\n')
                  break;
          while (cp2 >= cp) {
              const jschar *cp3 = matchRENodes(state, ren->next, NULL, cp2);
              if (cp3 != NULL) {
                  cp = cp2;
                  break;
              }
              cp2--;
          }
          break;
       case REOP_WBDRY:
          if (((cp == state->cpbegin) || !JS_ISWORD(cp[-1]))
                  ^ ((cp >= cpend) || !JS_ISWORD(*cp)))
                ; /* leave cp */
          else
              return NULL;
          break;
       case REOP_WNONBDRY:
          if (((cp == state->cpbegin) || !JS_ISWORD(cp[-1]))
                  ^ ((cp < cpend) && JS_ISWORD(*cp)))
                ; /* leave cp */
          else
              return NULL;
          break;
        case REOP_EOLONLY:
        case REOP_EOL:
          if (cp == cpend)
              ; /* leave cp */
          else {
              if (state->context->regExpStatics.multiline
                    || ((state->flags & JSREG_MULTILINE) != 0))
                  if (*cp == '\n')
                      ;/* leave cp */
                  else
                      return NULL;
              else
                  return NULL;
          }
          break;
        case REOP_BOL:
          if (cp != state->cpbegin) {
              if ((cp < cpend)
                   && (state->context->regExpStatics.multiline
                      || ((state->flags & JSREG_MULTILINE) != 0))) {
                  if (cp[-1] == '\n') {
                      break;
                  }
              }
              return NULL;
          }
          /* leave cp */
          break;
        case REOP_DIGIT:
          if ((cp != cpend) && JS_ISDIGIT(*cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_NONDIGIT:
          if ((cp != cpend) && !JS_ISDIGIT(*cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_ALNUM:
          if ((cp != cpend) && JS_ISWORD(*cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_NONALNUM:
          if ((cp != cpend) && !JS_ISWORD(*cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_SPACE:
          if ((cp != cpend) && JS_ISSPACE(*cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_NONSPACE:
          if ((cp != cpend) && !JS_ISSPACE(*cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_FLAT1:
          if ((cp != cpend)
                    && matchChar(state->flags, ren->u.chr, *cp))
              cp++;
          else
              return NULL;
          break;
        case REOP_FLAT:
          source = (jschar*) ren->kid;
          length = (const jschar *)ren->u.kid2 - source;
          if ((cp + length) > cpend)
              return NULL;
          else {
              for (i = 0; i < length; i++) {
                  if (!matchChar(state->flags, *cp++, *source++))
                      return NULL;
              }
          }
          break;
        case REOP_JUMP:
          break;
        case REOP_END:
          break;
        default :
          JS_ASSERT(JS_FALSE);
          break;
        }
        ren = ren->next;
    }
    return cp;
}

static const jschar *
MatchRegExp(MatchState *state, RENode *ren, const jschar *cp)
{
    const jschar *cp2, *cp3;
    /* have to include the position beyond the last character
     in order to detect end-of-input/line condition */
    for (cp2 = cp; cp2 <= state->cpend; cp2++) {
        state->skipped = cp2 - cp;
        state->parenCount = 0;
        cp3 = matchRENodes(state, ren, NULL, cp2);
        if (!state->ok) return NULL;
        if (cp3 != NULL)
            return cp3;
    }
    return NULL;
}


JSBool
js_ExecuteRegExp(JSContext *cx, JSRegExp *re, JSString *str, size_t *indexp,
		 JSBool test, jsval *rval)
{
    MatchState state;
    const jschar *cp, *ep;
    size_t i, length, start;
    void *mark;
    JSSubString *parsub, *morepar;
    JSBool ok;
    JSRegExpStatics *res;
    ptrdiff_t matchlen;
    uintN num, morenum;
    JSString *parstr, *matchstr;
    JSObject *obj;

    /*
     * Initialize a state struct to minimize recursive argument traffic.
     */
    state.context = cx;
    state.anchoring = JS_FALSE;
    state.flags = re->flags;

    /*
     * It's safe to load from cp because JSStrings have a zero at the end,
     * and we never let cp get beyond cpend.
     */
    start = *indexp;
    length = JSSTRING_LENGTH(str);
    if (start > length)
	start = length;
    cp = JSSTRING_CHARS(str);
    state.cpbegin = cp;
    state.cpend = cp + length;
    cp += start;
    state.start = start;
    state.skipped = 0;

    /*
     * Use the temporary arena pool to grab space for parenthetical matches.
     * After the JS_ARENA_ALLOCATE early return on error, goto out to be sure
     * to free this memory.
     */
    length = 2 * sizeof(JSSubString) * re->parenCount;
    mark = JS_ARENA_MARK(&cx->tempPool);
    JS_ARENA_ALLOCATE_CAST(parsub, JSSubString *, &cx->tempPool, length);
    if (!parsub) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    memset(parsub, 0, length);
    state.parenCount = 0;
    state.maybeParens = parsub;
    state.parens = parsub + re->parenCount;
    state.ok = JS_TRUE;

    /*
     * Call the recursive matcher to do the real work.  Return null on mismatch
     * whether testing or not.  On match, return an extended Array object.
     */
    cp = MatchRegExp(&state, re->ren, cp);
    if (!(ok = state.ok)) goto out;
    if (!cp) {
	*rval = JSVAL_NULL;
	goto out;
    }
    i = PTRDIFF(cp, state.cpbegin, jschar);
    *indexp = i;
    matchlen = i - (start + state.skipped);
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
	DEFVAL(STRING_TO_JSVAL(matchstr), INT_TO_JSVAL(0));
    }

    res = &cx->regExpStatics;
    JS_ASSERT(state.parenCount <= re->parenCount);
    if (state.parenCount == 0) {
	res->parenCount = 0;
	res->lastParen = js_EmptySubString;
    } else {
	for (num = 0; num < state.parenCount; num++) {
	    parsub = &state.parens[num];
	    if (num < 9) {
		res->parens[num] = *parsub;
	    } else {
		morenum = num - 9;
		morepar = res->moreParens;
		if (!morepar) {
		    res->moreLength = 10;
		    morepar = (JSSubString*) JS_malloc(cx,
                                                       10 * sizeof(JSSubString));
		} else if (morenum > res->moreLength) {
		    res->moreLength += 10;
		    morepar = (JSSubString*) JS_realloc(cx, morepar,
					 res->moreLength * sizeof(JSSubString));
		}
		if (!morepar) {
		    cx->newborn[GCX_OBJECT] = NULL;
		    cx->newborn[GCX_STRING] = NULL;
		    ok = JS_FALSE;
		    goto out;
		}
		res->moreParens = morepar;
		morepar[morenum] = *parsub;
	    }
	    if (test)
		continue;
	    parstr = js_NewStringCopyN(cx, parsub->chars, parsub->length, 0);
	    if (!parstr) {
		cx->newborn[GCX_OBJECT] = NULL;
		cx->newborn[GCX_STRING] = NULL;
		ok = JS_FALSE;
		goto out;
	    }
	    ok = js_DefineProperty(cx, obj, INT_TO_JSVAL(num + 1),
				   STRING_TO_JSVAL(parstr), NULL, NULL,
				   JSPROP_ENUMERATE, NULL);
	    if (!ok) {
		cx->newborn[GCX_OBJECT] = NULL;
		cx->newborn[GCX_STRING] = NULL;
		goto out;
	    }
	}
	res->parenCount = num;
	res->lastParen = *parsub;
    }

    if (!test) {
	/*
	 * Define the index and input properties last for better for/in loop
	 * order (so they come after the elements).
	 */
	DEFVAL(INT_TO_JSVAL(start + state.skipped),
	       (jsid)cx->runtime->atomState.indexAtom);
	DEFVAL(STRING_TO_JSVAL(str),
	       (jsid)cx->runtime->atomState.inputAtom);
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
	res->leftContext.length = state.skipped;
    } else {
	/*
	 * For JS1.3 and ECMAv2, emulate Perl5 exactly:
	 *
	 * js1.3        "hi", "hi there"            "hihitherehi therebye"
	 */
	res->leftContext.chars = JSSTRING_CHARS(str);
	res->leftContext.length = start + state.skipped;
    }
    res->rightContext.chars = ep;
    res->rightContext.length = state.cpend - ep;

out:
    JS_ARENA_RELEASE(&cx->tempPool, mark);
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

#define REGEXP_PROP_ATTRS (JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_SHARED)

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
    double lastIndex;
    JSRegExp *re;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);
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
	  case REGEXP_LAST_INDEX:
            /* NB: early unlock/return, so we don't deadlock with the GC. */
            lastIndex = re->lastIndex;
            JS_UNLOCK_OBJ(cx, obj);
            return js_NewNumberValue(cx, lastIndex, vp);
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
    jsint slot;
    JSRegExp *re;
    jsdouble d;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);
    if (slot == REGEXP_LAST_INDEX) {
        if (!js_ValueToNumber(cx, *vp, &d))
            return JS_FALSE;
        d = js_DoubleToInteger(d);
        JS_LOCK_OBJ(cx, obj);
        re = (JSRegExp *) JS_GetInstancePrivate(cx, obj, &js_RegExpClass, NULL);
        if (re)
            re->lastIndex = (uintN)d;
        JS_UNLOCK_OBJ(cx, obj);
    }
    return JS_TRUE;
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
    uint8 flags;

    if (xdr->mode == JSXDR_ENCODE) {
	re = (JSRegExp *) JS_GetPrivate(xdr->cx, *objp);
	if (!re)
	    return JS_FALSE;
	source = re->source;
	flags = re->flags;
    }
    if (!JS_XDRString(xdr, &source) ||
	!JS_XDRUint8(xdr, &flags)) {
	return JS_FALSE;
    }
    if (xdr->mode == JSXDR_DECODE) {
	*objp = js_NewObject(xdr->cx, &js_RegExpClass, NULL, NULL);
	if (!*objp)
	    return JS_FALSE;
	re = js_NewRegExp(xdr->cx, NULL, source, flags, JS_FALSE);
	if (!re)
	    return JS_FALSE;
	if (!JS_SetPrivate(xdr->cx, *objp, re)) {
	    js_DestroyRegExp(xdr->cx, re);
	    return JS_FALSE;
	}
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
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  regexp_getProperty, regexp_setProperty,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,     regexp_finalize,
    NULL,             NULL,             regexp_call,        NULL,
    regexp_xdrObject, NULL,             regexp_mark,        0
};

static JSBool
regexp_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
    JSRegExp *re;
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

    length = JSSTRING_LENGTH(re->source) + 2;
    nflags = 0;
    for (flags = re->flags; flags != 0; flags &= flags - 1)
	nflags++;
    chars = (jschar*) JS_malloc(cx, (length + nflags + 1) * sizeof(jschar));
    if (!chars) {
        JS_UNLOCK_OBJ(cx, obj);
        return JS_FALSE;
    }

    chars[0] = '/';
    js_strncpy(&chars[1], JSSTRING_CHARS(re->source), length - 2);
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
    JSBool ok;
    JSObject *obj2;

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
                goto madeit;
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
    }
    re = js_NewRegExpOpt(cx, NULL, str, opt, JS_FALSE);
madeit:
    if (!re)
	return JS_FALSE;
    JS_LOCK_OBJ(cx, obj);
    oldre = (JSRegExp *) JS_GetPrivate(cx, obj);
    ok = JS_SetPrivate(cx, obj, re);
    JS_UNLOCK_OBJ(cx, obj);
    if (!ok) {
	js_DestroyRegExp(cx, re);
    } else {
        if (oldre)
            js_DestroyRegExp(cx, oldre);
        *rval = OBJECT_TO_JSVAL(obj);
    }
    return ok;
}

static JSBool
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		JSBool test, jsval *rval)
{
    JSBool ok;
    JSRegExp *re;
    JSString *str;
    size_t i;

    ok = JS_FALSE;
    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
	return ok;
    JS_LOCK_OBJ(cx, obj);
    re = (JSRegExp *) JS_GetPrivate(cx, obj);
    if (!re) {
	JS_UNLOCK_OBJ(cx, obj);
	return JS_TRUE;
    }

    /* NB: we must reach out: after this paragraph, in order to drop re. */
    HOLD_REGEXP(cx, re);
    i = (re->flags & JSREG_GLOB) ? re->lastIndex : 0;
    JS_UNLOCK_OBJ(cx, obj);

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
            goto out;
	}
    } else {
	str = js_ValueToString(cx, argv[0]);
	if (!str)
	    goto out;
	argv[0] = STRING_TO_JSVAL(str);
    }

    ok = js_ExecuteRegExp(cx, re, str, &i, test, rval);
    JS_LOCK_OBJ(cx, obj);
    if (re->flags & JSREG_GLOB)
	re->lastIndex = (*rval == JSVAL_NULL) ? 0 : i;
    JS_UNLOCK_OBJ(cx, obj);

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
    {js_toSource_str,   regexp_toString,        0,0,0},
#endif
    {js_toString_str,   regexp_toString,        0,0,0},
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
        if ((argc < 2 || JSVAL_IS_VOID(argv[1])) && JSVAL_IS_OBJECT(argv[0]) &&
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
    if (!obj || !JS_SetPrivate(cx, obj, re)) {
	js_DestroyRegExp(cx, re);
	return NULL;
    }
    return obj;
}

#endif /* JS_HAS_REGEXPS */
