/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * JS regular expressions, after Perl.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
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
#include "jsstr.h"

#if JS_HAS_REGEXPS

typedef struct RENode RENode;

typedef enum REOp {
    REOP_EMPTY    = 0,  /* match rest of input against rest of r.e. */
    REOP_ALT      = 1,  /* alternative subexpressions in kid and next */
    REOP_BOL      = 2,  /* beginning of input (or line if multiline) */
    REOP_EOL      = 3,  /* end of input (or line if multiline) */
    REOP_WBDRY    = 4,  /* match "" at word boundary */
    REOP_WNONBDRY = 5,  /* match "" at word non-boundary */
    REOP_QUANT    = 6,  /* quantified atom: atom{1,2} */
    REOP_STAR     = 7,  /* zero or more occurrences of kid */
    REOP_PLUS     = 8,  /* one or more occurrences of kid */
    REOP_OPT      = 9,  /* optional subexpression in kid */
    REOP_LPAREN   = 10, /* left paren bytecode: kid is u.num'th sub-regexp */
    REOP_RPAREN   = 11, /* right paren bytecode */
    REOP_DOT      = 12, /* stands for any character */
    REOP_CCLASS   = 13, /* character class: [a-f] */
    REOP_DIGIT    = 14, /* match a digit char: [0-9] */
    REOP_NONDIGIT = 15, /* match a non-digit char: [^0-9] */
    REOP_ALNUM    = 16, /* match an alphanumeric char: [0-9a-z_A-Z] */
    REOP_NONALNUM = 17, /* match a non-alphanumeric char: [^0-9a-z_A-Z] */
    REOP_SPACE    = 18, /* match a whitespace char */
    REOP_NONSPACE = 19, /* match a non-whitespace char */
    REOP_BACKREF  = 20, /* back-reference (e.g., \1) to a parenthetical */
    REOP_FLAT     = 21, /* match a flat string */
    REOP_FLAT1    = 22, /* match a single char */
    REOP_JUMP     = 23, /* for deoptimized closure loops */
    REOP_DOTSTAR  = 24, /* optimize .* to use a single opcode */
    REOP_ANCHOR   = 25, /* like .* but skips left context to unanchored r.e. */
    REOP_EOLONLY  = 26, /* $ not preceded by any pattern */
    REOP_UCFLAT   = 27, /* flat Unicode string; len immediate counts chars */
    REOP_UCFLAT1  = 28, /* single Unicode char */
    REOP_UCCLASS  = 29, /* Unicode character class, vector of chars to match */
    REOP_NUCCLASS = 30, /* negated Unicode character class */
    REOP_BACKREFi = 31, /* case-independent REOP_BACKREF */
    REOP_FLATi    = 32, /* case-independent REOP_FLAT */
    REOP_FLAT1i   = 33, /* case-independent REOP_FLAT1 */
    REOP_UCFLATi  = 34, /* case-independent REOP_UCFLAT */
    REOP_UCFLAT1i = 35, /* case-independent REOP_UCFLAT1 */
    REOP_ANCHOR1  = 36, /* first-char discriminating REOP_ANCHOR */
    REOP_NCCLASS  = 37, /* negated 8-bit character class */
    REOP_END
} REOp;

#define CCLASS_CHARSET_SIZE 256 /* ISO-Latin-1 */

uint8 reopsize[] = {
    /* EMPTY */         1,
    /* ALT */           3,
    /* BOL */           1,
    /* EOL */           1,
    /* WBDRY */         1,
    /* WNONBDRY */      1,
    /* QUANT */         7,
    /* STAR */          1,
    /* PLUS */          1,
    /* OPT */           1,
    /* LPAREN */        3,
    /* RPAREN */        3,
    /* DOT */           1,
    /* CCLASS */        1 + (CCLASS_CHARSET_SIZE / PR_BITS_PER_BYTE),
    /* DIGIT */         1,
    /* NONDIGIT */      1,
    /* ALNUM */         1,
    /* NONALNUM */      1,
    /* SPACE */         1,
    /* NONSPACE */      1,
    /* BACKREF */       2,
    /* FLAT */          2,	/* (2 = op + len) + [len bytes] */
    /* FLAT1 */         2,
    /* JUMP */          3,
    /* DOTSTAR */       1,
    /* ANCHOR */        1,
    /* EOLONLY */       1,
    /* UCFLAT */        2,	/* (2 = op + len) + [len 2-byte chars] */
    /* UCFLAT1 */       3,      /* op + hibyte + lobyte */
    /* UCCLASS */       3,      /* (3 = op + 2-byte len) + [len bytes] */
    /* NUCCLASS */      3,      /* (3 = op + 2-byte len) + [len bytes] */
    /* BACKREFi */      2,
    /* FLATi */         2,	/* (2 = op + len) + [len bytes] */
    /* FLAT1i */        2,
    /* UCFLATi */       2,	/* (2 = op + len) + [len 2-byte chars] */
    /* UCFLAT1i */      3,      /* op + hibyte + lobyte */
    /* ANCHOR1 */       1,
    /* NCCLASS */       1 + (CCLASS_CHARSET_SIZE / PR_BITS_PER_BYTE),
    /* END */           0,
};

#define REOP_FLATLEN_MAX 255    /* maximum length of FLAT string */

struct RENode {
    uint8           op;         /* packed r.e. op bytecode */
    uint8           flags;      /* flags, see below */
    uint16          offset;     /* bytecode offset */
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

typedef struct CompilerState {
    JSContext       *context;
    const jschar    *cpbegin;
    const jschar    *cp;
    uintN           flags;
    uintN           parenCount;
    size_t          progLength;
} CompilerState;

static RENode *
NewRENode(CompilerState *state, REOp op, void *kid)
{
    JSContext *cx;
    RENode *ren;

    cx = state->context;
    PR_ARENA_ALLOCATE(ren, &cx->tempPool, sizeof *ren);
    if (!ren) {
	JS_ReportOutOfMemory(cx);
	return NULL;
    }
    ren->op = (uint8)op;
    ren->flags = 0;
    ren->offset = 0;
    ren->next = NULL;
    ren->kid = kid;
    return ren;
}

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
	printf("%5d %6d %c%c%c%c%c%c  %s",
	       level,
	       (int)ren->offset,
	       (ren->flags & RENODE_ANCHORED) ? 'A' : '-',
	       (ren->flags & RENODE_SINGLE)   ? 'S' : '-',
	       (ren->flags & RENODE_NONEMPTY) ? 'F' : '-',	/* F for full */
	       (ren->flags & RENODE_ISNEXT)   ? 'N' : '-',	/* N for next */
	       (ren->flags & RENODE_GOODNEXT) ? 'G' : '-',
	       (ren->flags & RENODE_ISJOIN)   ? 'J' : '-',
	       reopname[ren->op]);

	switch (REOP(ren)) {
	  case REOP_ALT:
	    printf(" %d\n", ren->next->offset);
	    ok = DumpRegExp(cx, ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_STAR:
	  case REOP_PLUS:
	  case REOP_OPT:
	  case REOP_ANCHOR1:
	    printf("\n");
	    ok = DumpRegExp(cx, ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_QUANT:
	    printf(" next %d min %d max %d\n",
		   ren->next->offset, ren->u.range.min, ren->u.range.max);
	    ok = DumpRegExp(cx, ren->kid);
	    if (!ok)
		goto out;
	    break;

	  case REOP_LPAREN:
	    printf(" num %d\n", (int)ren->u.num);
	    ok = DumpRegExp(cx, ren->kid);
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
	    cp = ren->kid;
	    len = (jschar *)ren->u.kid2 - cp;
	    for (i = 0; i < len; i++)
		PrintChar(cp[i]);
	    break;

	  case REOP_UCFLAT1:
	    PrintChar(ren->u.chr);
	    break;

	  case REOP_UCCLASS:
	    cp = ren->kid;
	    len = (jschar *)ren->u.kid2 - cp;
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
	    kid = ren1->kid;
	    if (REOP(kid) == REOP_JUMP)
		continue;
	    for (ren = kid; ren->next; ren = ren->next)
		PR_ASSERT(REOP(ren) != REOP_ALT);

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
	if (!FixNext(state, ren1->kid, ren2, oldnext))
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
    if (*cp == '|') {
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
	} while (*cp == '|');
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
    while ((c = *cp) != 0 && c != '|' && c != ')') {
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
    switch (*cp) {
      case '{':
	c = *++cp;
	if (!JS7_ISDEC(c)) {
	    JS_ReportError(state->context, "invalid quantifier %s", state->cp);
	    return NULL;
	}
	min = (uint32)JS7_UNDEC(c);
	for (c = *++cp; JS7_ISDEC(c); c = *++cp) {
	    min = 10 * min + (uint32)JS7_UNDEC(c);
	    if (min >> 16) {
		JS_ReportError(state->context, "overlarge minimum %s",
			       state->cp);
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
			JS_ReportError(state->context, "overlarge maximum %s",
				       up);
			return NULL;
		    }
		}
		if (max == 0)
		    goto zero_quant;
		if (min > max) {
		    JS_ReportError(state->context,
				   "maximum %s less than minimum", up);
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
		JS_ReportError(state->context, "zero quantifier %s", state->cp);
		return NULL;
	    }
	    max = min;
	}
	if (*cp != '}') {
	    JS_ReportError(state->context, "unterminated quantifier %s",
			   state->cp);
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
	goto loop;

      case '*':
	if (!(ren->flags & RENODE_NONEMPTY)) {
	    JS_ReportError(state->context,
			   "regular expression before * could be empty");
	    return NULL;
	}
	cp++;
	ren = NewRENode(state, REOP_STAR, ren);
	goto loop;

      case '+':
	if (!(ren->flags & RENODE_NONEMPTY)) {
	    JS_ReportError(state->context,
			   "regular expression before + could be empty");
	    return NULL;
	}
	cp++;
	ren2 = NewRENode(state, REOP_PLUS, ren);
	if (!ren2)
	    return NULL;
	if (ren->flags & RENODE_NONEMPTY)
	    ren2->flags |= RENODE_NONEMPTY;
	ren = ren2;
	goto loop;

      case '?':
	cp++;
	ren = NewRENode(state, REOP_OPT, ren);
	goto loop;
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
    uintN num, tmp, len;
    RENode *ren, *ren2;
    jschar c;

    cp = ocp = state->cp;
    switch (*cp) {
      case 0:
	ren = NewRENode(state, REOP_EMPTY, NULL);
	break;

      case '(':
	num = state->parenCount++;	/* \1 is numbered 0, etc. */
	state->cp = cp + 1;
	ren2 = ParseRegExp(state);
	if (!ren2)
	    return NULL;
	cp = state->cp;
	if (*cp != ')') {
	    JS_ReportError(state->context, "unterminated parenthetical %s",
			   ocp);
	    return NULL;
	}
	cp++;
	ren = NewRENode(state, REOP_LPAREN, ren2);
	if (!ren)
	    return NULL;
	ren->flags = ren2->flags & (RENODE_ANCHORED | RENODE_NONEMPTY);
	ren->u.num = num;
	ren2 = NewRENode(state, REOP_RPAREN, NULL);
	if (!ren2 || !SetNext(state, ren, ren2))
	    return NULL;
	ren2->u.num = num;
	break;

      case '.':
	cp++;
	if ((c = *cp) == '*')
	    cp++;
	ren = NewRENode(state, (c == '*') ? REOP_DOTSTAR : REOP_DOT, NULL);
	if (ren && REOP(ren) == REOP_DOT)
	    ren->flags = RENODE_SINGLE | RENODE_NONEMPTY;
	break;

      case '[':
	/* A char class must have at least one char in it. */
	if ((c = *++cp) == 0)
	    goto bad_cclass;

	ren = NewRENode(state, REOP_CCLASS, (void *)cp);
	if (!ren)
	    return NULL;

	/* A negated class must have at least one char in it after the ^. */
	if (c == '^' && *++cp == 0)
	    goto bad_cclass;

	while ((c = *++cp) != ']') {
	    if (c == 0) {
      bad_cclass:
		JS_ReportError(state->context,
			       "unterminated character class %s", ocp);
		return NULL;
	    }
	    if (c == '\\' && cp[1] != 0)
		cp++;
	}
	ren->u.kid2 = (void *)cp++;

	/* Since we rule out [] and [^], we can set the non-empty flag. */
	ren->flags = RENODE_SINGLE | RENODE_NONEMPTY;
	break;

      case '\\':
	c = *++cp;
	switch (c) {
	  case 0:
	    JS_ReportError(state->context, "trailing \\ in regular expression");
	    return NULL;

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
	    for (c = *++cp; JS7_ISDEC(c); c = *++cp)
		num = 10 * num - (uintN)JS7_UNDEC(c);
	    if (num > 9 && num > state->parenCount) {
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
		    cp--;	/* back up so cp points to last hex char */
		}
	    } else {
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
	while ((c = *++cp) != 0 && !js_strchr(metachars, c))
	    ;
	len = (uintN)(cp - ocp);
	if (c != 0 && len > 1 && js_strchr(closurechars, c)) {
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

static ptrdiff_t
CountFirstChars(RENode *alt)
{
    ptrdiff_t len, sublen;
    RENode *kid;
    jschar c, *ccp, *ccend;

    len = 0;
    do {
	for (kid = alt->kid; REOP(kid) == REOP_LPAREN; kid = kid->kid)
	    ;
	switch (REOP(kid)) {
	  case REOP_QUANT:
	    if (kid->u.range.min == 0)
	    	return -1;
	    /* FALL THROUGH */
	  case REOP_PLUS:
	  case REOP_ALT:
	    sublen = CountFirstChars(kid);
	    if (sublen < 0)
	    	return sublen;
	    len += sublen;
	    break;
	  case REOP_FLAT:
	    c = *(jschar *)kid->kid;
	    goto count_char;
	  case REOP_FLAT1:
	    c = kid->u.chr;
	  count_char:
	    /* Only '\\' and '-' need escaping within a character class. */
	    if (c == '\\' || c == '-')
	    	len += 2;
	    else
		len++;
	    break;
	  case REOP_CCLASS:
	    ccp = kid->kid;
	    if (*ccp == '^')
		return -1;
	    ccend = kid->u.kid2;
	    len += ccend - ccp;
	    break;
	  case REOP_DIGIT:
	  case REOP_NONDIGIT:
	  case REOP_ALNUM:
	  case REOP_NONALNUM:
	  case REOP_SPACE:
	  case REOP_NONSPACE:
	    len += 2;
	    break;
	  default:
	    return -1;
	}
	/* Test for non-alt so quant and plus execute to here only. */
	if (REOP(alt) != REOP_ALT)
	    break;
	alt = alt->next;
    } while (alt && REOP(alt) == REOP_ALT);
    return len;
}

static ptrdiff_t
StoreChar(jschar *cp, ptrdiff_t i, jschar c, JSBool escape)
{
    ptrdiff_t j;

    /* Suppress dups to avoid making a flat1 into a cclass. */
    for (j = 0; j < i; j++) {
	if (cp[j] == '\\')
	    j++;
	if (cp[j] == c && (!escape || (j > 0 && cp[j-1] == '\\')))
	    return i;
    }

    /* Only '\\' and '-' need escaping within a character class. */
    if (escape || c == '\\' || c == '-')
	cp[i++] = '\\';
    cp[i++] = c;
    return i;
}

static ptrdiff_t
StoreFirstChars(RENode *alt, jschar *cp, ptrdiff_t i)
{
    RENode *kid;
    jschar *ccp, *ccend;

    do {
	for (kid = alt->kid; REOP(kid) == REOP_LPAREN; kid = kid->kid)
	    ;
	switch (REOP(kid)) {
	  case REOP_QUANT:
	    PR_ASSERT(kid->u.range.min != 0);
	    /* FALL THROUGH */
	  case REOP_PLUS:
	  case REOP_ALT:
	    i = StoreFirstChars(kid, cp, i);
	    break;
	  case REOP_FLAT:
	    i = StoreChar(cp, i, *(jschar *)kid->kid, JS_FALSE);
	    break;
	  case REOP_FLAT1:
	    i = StoreChar(cp, i, kid->u.chr, JS_FALSE);
	    break;
	  case REOP_CCLASS:
	    ccend = kid->u.kid2;
	    for (ccp = kid->kid; ccp < ccend; ccp++)
		cp[i++] = *ccp;
	    break;
	  case REOP_DIGIT:
	    i = StoreChar(cp, i, 'd', JS_TRUE);
	    break;
	  case REOP_NONDIGIT:
	    i = StoreChar(cp, i, 'D', JS_TRUE);
	    break;
	  case REOP_ALNUM:
	    i = StoreChar(cp, i, 'w', JS_TRUE);
	    break;
	  case REOP_NONALNUM:
	    i = StoreChar(cp, i, 'W', JS_TRUE);
	    break;
	  case REOP_SPACE:
	    i = StoreChar(cp, i, 's', JS_TRUE);
	    break;
	  case REOP_NONSPACE:
	    i = StoreChar(cp, i, 'S', JS_TRUE);
	    break;
	  default:
	    PR_ASSERT(0);
	}
	/* Test for non-alt so quant and plus execute to here only. */
	if (REOP(alt) != REOP_ALT)
	    break;
	alt = alt->next;
    } while (alt && REOP(alt) == REOP_ALT);
    return i;
}

static JSBool
AnchorRegExp(CompilerState *state, RENode *ren)
{
    RENode *ren2, *kid;
    ptrdiff_t len;
    jschar *cp;
    REOp op;

    for (ren2 = ren; REOP(ren2) == REOP_LPAREN; ren2 = ren2->kid)
	;
    switch (REOP(ren2)) {
      case REOP_ALT:
	len = CountFirstChars(ren2);
	if (len <= 0)
	    goto do_anchor;
	PR_ARENA_ALLOCATE(cp, &state->context->tempPool, len * sizeof(jschar));
	if (!cp) {
	    JS_ReportOutOfMemory(state->context);
	    return JS_FALSE;
	}

	len = StoreFirstChars(ren2, cp, 0);
	if (len == 1) {
	    op = REOP_FLAT1;
	} else if (len == 2 && *cp == '\\') {
	    switch (cp[1]) {
	      case '\\':
	      case '-':
		/* No need for a character class if just '\\' or '-'. */
		cp++;
		op = REOP_FLAT1;
		break;
	      case 'd':
		op = REOP_DIGIT;
		break;
	      case 'D':
		op = REOP_NONDIGIT;
		break;
	      case 'w':
		op = REOP_ALNUM;
		break;
	      case 'W':
		op = REOP_NONALNUM;
		break;
	      case 's':
		op = REOP_SPACE;
		break;
	      case 'S':
		op = REOP_NONSPACE;
		break;
	      default:
		op = REOP_CCLASS;
		break;
	    }
	} else {
	    op = REOP_CCLASS;
	}

      do_first_char:
	kid = NewRENode(state, op, cp);
	if (!kid)
	    return JS_FALSE;
	kid->flags = RENODE_SINGLE | RENODE_NONEMPTY;
	if (op == REOP_FLAT1)
	    kid->u.chr = *cp;
	else if (op == REOP_CCLASS)
	    kid->u.kid2 = cp + len;

	ren2 = NewRENode(state, REOP(ren), ren->kid);
	if (!ren2)
	    return JS_FALSE;
	ren2->flags = ren->flags | RENODE_ISNEXT;
	ren2->next = ren->next;
	ren2->u = ren->u;

	ren->op = REOP_ANCHOR1;
	ren->flags = RENODE_GOODNEXT;
	ren->next = ren2;
	ren->kid = kid;
	ren->u.kid2 = NULL;
	break;

      case REOP_FLAT:
	cp = ren2->kid;
	op = REOP_FLAT1;
	goto do_first_char;

      case REOP_FLAT1:
      	cp = &ren2->u.chr;
      	op = REOP_FLAT1;
	goto do_first_char;

      case REOP_DOTSTAR:
	/*
	 * ".*" is anchored by definition when at the front of a list.
	 */
	break;

      default:
      do_anchor:
	/*
	 * Any node other than dotstar that's unanchored and nonempty must be
	 * prefixed by REOP_ANCHOR.
	 */
	PR_ASSERT(REOP(ren2) != REOP_ANCHOR);
	PR_ASSERT(!(ren2->flags & RENODE_ISNEXT));
	if ((ren2->flags & (RENODE_ANCHORED | RENODE_NONEMPTY))
	    == RENODE_NONEMPTY) {
	    ren2 = NewRENode(state, REOP(ren), ren->kid);
	    if (!ren2)
		return JS_FALSE;
	    ren2->flags = ren->flags | RENODE_ISNEXT;
	    ren2->next = ren->next;
	    ren2->u = ren->u;
	    ren->op = REOP_ANCHOR;
	    ren->flags = RENODE_GOODNEXT;
	    ren->next = ren2;
	    ren->kid = ren->u.kid2 = NULL;
	}
	break;
    }
    return JS_TRUE;
}

static RENode *
CloseTail(CompilerState *state, RENode *alt1, RENode *next)
{
    RENode *alt2, *empty;

    empty = NewRENode(state, REOP_EMPTY, NULL);
    alt2 = NewRENode(state, REOP_ALT, empty);
    if (!alt2 || !empty)
	return NULL;
    alt1->next = alt2;
    alt2->next = empty->next = next;
    if (alt1->flags & RENODE_GOODNEXT)
	alt2->flags |= RENODE_GOODNEXT;
    else
	alt1->flags |= RENODE_GOODNEXT;
    alt2->flags |= RENODE_ISNEXT;
    return alt2;
}

static JSBool
OptimizeRegExp(CompilerState *state, RENode *ren)
{
    RENode *kid, *next, *jump, *alt1;
    uintN flag;
    jschar c, c2, maxc, *cp, *cp2;
    ptrdiff_t len, len2;
    size_t size, incr;
    JSContext *cx;
    JSBool reallok;

    do {
	switch (REOP(ren)) {
	  case REOP_STAR:
	    kid = ren->kid;
	    if (!(kid->flags & RENODE_SINGLE)) {
		/*
		 * If kid is not simple, deoptimize <kid>* as follows (the |__|
		 * are byte placeholders for next/jump offsets):
		 *
		 * FROM: |STAR|<kid>|
		 *
		 *       +-----------------------+
		 *       V                       |
		 * TO:   |ALT|__|__|<kid>|JUMP|__|__|ALT|__|__|EMPTY|
		 *              |                   ^      |        ^
		 *              +-------------------+      +--------+
		 */
		ren->op = REOP_ALT;
		next = ren->next;
		jump = NewRENode(state, REOP_JUMP, NULL);
		if (!jump || !FixNext(state, kid, jump, next))
		    return JS_FALSE;
		jump->next = ren;
		if (ren->flags & RENODE_ISNEXT)
		    ren->flags |= RENODE_ISJOIN;
		if (!CloseTail(state, ren, next))
		    return JS_FALSE;
	    }
	    break;

	  case REOP_PLUS:
	    kid = ren->kid;
	    if (!(kid->flags & RENODE_SINGLE)) {
		/*
		 * FROM: |PLUS|<kid>|
		 *
		 *       +-----------------------+
		 *       V                       |
		 * TO:   |<kid>|ALT|__|__|JUMP|__|__|ALT|__|__|EMPTY|
		 *                    |             ^      |        ^
		 *                    +-------------+      +--------+
		 */
		next = ren->next;
		flag = (ren->flags & RENODE_GOODNEXT);
		*ren = *kid;
		jump = NewRENode(state, REOP_JUMP, NULL);
		alt1 = NewRENode(state, REOP_ALT, jump);
		if (!alt1 || !jump || !FixNext(state, ren, alt1, next))
		    return JS_FALSE;
		alt1->flags |= flag;
		jump->next = ren;
		if (ren->flags & RENODE_ISNEXT)
		    ren->flags |= RENODE_ISJOIN;
		if (!CloseTail(state, alt1, next))
		    return JS_FALSE;
	    }
	    break;

	  case REOP_OPT:
	    kid = ren->kid;
	    if (!(kid->flags & RENODE_SINGLE)) {
		/*
		 * FROM: |OPT|<kid>|
		 *
		 *                               +------------------+
		 *                               |                  v
		 * TO:   |ALT|__|__|<kid>|JUMP|__|__|ALT|__|__|EMPTY|
		 *              |                   ^      |        ^
		 *              +-------------------+      +--------+
		 */
		ren->op = REOP_ALT;
		next = ren->next;
		jump = NewRENode(state, REOP_JUMP, NULL);
		if (!jump || !FixNext(state, kid, jump, next))
		    return JS_FALSE;
		jump->next = next;
		if (!CloseTail(state, ren, next))
		    return JS_FALSE;
		next->flags |= RENODE_ISJOIN;
	    }
	    break;

	  case REOP_FLAT:
	    /*
	     * Coalesce adjacent FLAT and FLAT1 nodes.  Also coalesce FLAT and
	     * FLAT, which can result from deleting a coalesced FLAT1.
	     */
	    while ((next = ren->next) != NULL &&
		   !(next->flags & RENODE_ISJOIN) &&
		   (REOP(next) == REOP_FLAT || REOP(next) == REOP_FLAT1)) {
		if (REOP(next) == REOP_FLAT) {
		    cp2 = next->kid;
		    len2 = PTRDIFF((jschar *)next->u.kid2, cp2, jschar);
		} else {
		    cp2 = &next->u.chr;
		    len2 = 1;
		}
		cp = ren->kid;
		len = PTRDIFF((jschar *)ren->u.kid2, cp, jschar);
		if (len + len2 > REOP_FLATLEN_MAX)
		    break;
		cx = state->context;
		reallok = ren->flags & RENODE_REALLOK;
		if (reallok) {
		    /* Try to extend the last alloc, to fuse FLAT,FLAT1,... */
		    size = (len + 1) * sizeof(jschar);
		    incr = len2 * sizeof(jschar);
		    PR_ARENA_GROW(cp, &cx->tempPool, size, incr);
		} else {
		    size = (len + len2 + 1) * sizeof(jschar);
		    PR_ARENA_ALLOCATE(cp, &cx->tempPool, size);
		}
		if (!cp) {
		    JS_ReportOutOfMemory(cx);
		    return JS_FALSE;
		}
		if (!reallok) {
		    js_strncpy(cp, ren->kid, len);
		    ren->flags |= RENODE_REALLOK;
		}
		js_strncpy(&cp[len], cp2, len2);
		len += len2;
		cp[len] = 0;
	  end_coalesce:
		ren->kid = cp;
		PR_ASSERT(ren->flags & RENODE_GOODNEXT);
		if (!(next->flags & RENODE_GOODNEXT))
		    ren->flags &= ~RENODE_GOODNEXT;
		ren->u.kid2 = cp + len;
		ren->next = next->next;
		next->op = REOP_EMPTY;	/* next should be unreachable! */
	    }
	    break;

	  case REOP_FLAT1:
	    /*
	     * Coalesce adjacent FLAT1 nodes.  Also coalesce FLAT1 and FLAT.
	     * After a single coalesce, we reuse the REOP_FLAT case's code by
	     * jumping into the bottom of its while loop.
	     */
	    next = ren->next;
	    if (next &&
		!(next->flags & RENODE_ISJOIN) &&
		(REOP(next) == REOP_FLAT || REOP(next) == REOP_FLAT1)) {
		if (REOP(next) == REOP_FLAT) {
		    cp2 = next->kid;
		    len = PTRDIFF((jschar *)next->u.kid2, cp2, jschar);
		} else {
		    cp2 = &next->u.chr;
		    len = 1;
		}
		cx = state->context;
		PR_ARENA_ALLOCATE(cp, &cx->tempPool, len + 2);
		if (!cp) {
		    JS_ReportOutOfMemory(cx);
		    return JS_FALSE;
		}
		cp[0] = ren->u.chr;
		js_strncpy(&cp[1], cp2, len);
		cp[++len] = 0;
		ren->op = REOP_FLAT;
		ren->flags |= RENODE_REALLOK;
		goto end_coalesce;
	    }
	    break;

	  default:;
	}

	/*
	 * Set ren's offset and advance progLength by ren's base size.
	 */
	ren->offset = (uint16) state->progLength;
	state->progLength += reopsize[ren->op];

	switch (REOP(ren)) {
	  case REOP_ALT:
	  case REOP_QUANT:
	  case REOP_STAR:
	  case REOP_PLUS:
	  case REOP_OPT:
	  case REOP_LPAREN:
	  case REOP_ANCHOR1:
	    /*
	     * Recur for nodes that have kid links to other nodes.
	     */
	    if (!OptimizeRegExp(state, ren->kid))
		return JS_FALSE;
	    break;

	  case REOP_CCLASS:
	    /*
	     * Check for a nonzero high byte or a \uXXXX escape sequence.
	     */
	    cp  = ren->kid;
	    cp2 = ren->u.kid2;
	    len = PTRDIFF(cp2, cp, jschar);
	    maxc = 0;
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
	    if (maxc >= CCLASS_CHARSET_SIZE) {
		ren->op = (uint8)REOP_UCCLASS;
		size = (size_t)(maxc + PR_BITS_PER_BYTE) / PR_BITS_PER_BYTE;
		ren->u.ucclass.kidlen = (uint16)len;
		ren->u.ucclass.bmsize = (uint16)size;
		state->progLength -= reopsize[REOP_CCLASS];
		state->progLength += reopsize[REOP_UCCLASS] + size;
	    }
	    break;

	  case REOP_FLAT:
	    /*
	     * FLAT takes 2 bytes plus the bytes in the string to match.
	     * If any character has a non-zero high byte, switch to UCFLAT
	     * and double the immediate operand length.
	     */
	    cp  = ren->kid;
	    cp2 = ren->u.kid2;
	    len = PTRDIFF(cp2, cp, jschar);
	    while (cp < cp2) {
		c = *cp++;
		if (c >> 8) {
		    ren->op = (uint8)REOP_UCFLAT;
		    len *= 2;
		    break;
		}
	    }
	    state->progLength += len;
	    break;

	  case REOP_FLAT1:
	    c = ren->u.chr;
	    if (c >> 8) {
		ren->op = (uint8)REOP_UCFLAT1;
		state->progLength++;
	    }
	    break;

	  case REOP_JUMP:
	    /*
	     * Eliminate jumps to jumps.
	     */
	    while ((next = ren->next) != NULL && REOP(next) == REOP_JUMP)
		ren->next = next->next;
	    break;

	  case REOP_END:
	    /*
	     * End of program.
	     */
	    return JS_TRUE;

	  default:;
	}

	if (!(ren->flags & RENODE_GOODNEXT))
	    break;
    } while ((ren = ren->next) != NULL);

    return JS_TRUE;
}

static JSBool
EmitRegExp(CompilerState *state, RENode *ren, JSRegExp *re)
{
    REOp op;
    jsbytecode *pc, fill;
    RENode *next;
    ptrdiff_t diff;
    jschar *cp, *end, *ocp;
    uintN b, c, i, j, n, lastc, foldc, nchars;
    JSBool inrange;

    do {
	op = REOP(ren);
	if (op == REOP_END)
	    return JS_TRUE;

	pc = &re->program[state->progLength];
	state->progLength += reopsize[ren->op];
	pc[0] = ren->op;
	next = ren->next;

	switch (op) {
	  case REOP_ALT:
	    diff = next->offset - ren->offset;
	    SET_JUMP_OFFSET(pc, diff);
	    if (!EmitRegExp(state, ren->kid, re))
		return JS_FALSE;
	    break;

	  case REOP_QUANT:
	    diff = next->offset - ren->offset;
	    SET_JUMP_OFFSET(pc, diff);
	    pc += 2;
	    SET_ARGNO(pc, ren->u.range.min);
	    pc += 2;
	    SET_ARGNO(pc, ren->u.range.max);
	    if (!EmitRegExp(state, ren->kid, re))
		return JS_FALSE;
	    break;

	  case REOP_STAR:
	  case REOP_PLUS:
	  case REOP_OPT:
	  case REOP_ANCHOR1:
	    if (!EmitRegExp(state, ren->kid, re))
		return JS_FALSE;
	    break;

	  case REOP_LPAREN:
	    SET_ARGNO(pc, ren->u.num);
	    if (!EmitRegExp(state, ren->kid, re))
		return JS_FALSE;
	    break;

	  case REOP_RPAREN:
	    SET_ARGNO(pc, ren->u.num);
	    break;

	  case REOP_CCLASS:
	  case REOP_UCCLASS:
	    cp = ren->kid;
	    if (*cp == '^') {
		pc[0] = (jsbytecode)
			((op == REOP_CCLASS) ? REOP_NCCLASS : REOP_NUCCLASS);
		fill = 0xff;
		cp++;
	    } else {
		fill = 0;
	    }
	    pc++;
	    if (op == REOP_CCLASS) {
		end = ren->u.kid2;
		for (i = 0; i < CCLASS_CHARSET_SIZE / PR_BITS_PER_BYTE; i++)
		    pc[i] = fill;
		nchars = CCLASS_CHARSET_SIZE;
	    } else {
		end = cp + ren->u.ucclass.kidlen;
		n = (uintN)ren->u.ucclass.bmsize;
		*pc++ = (jsbytecode)(n >> 8);
		*pc++ = (jsbytecode)n;
		state->progLength += n;
		for (i = 0; i < n; i++)
		    pc[i] = fill;
		nchars = n * PR_BITS_PER_BYTE;
	    }

/* Split ops up into statements to keep MSVC1.52 from crashing. */
#define MATCH_BIT(c)    { i = (c) >> 3; b = (c) & 7; b = 1 << b;              \
			  if (fill) pc[i] &= ~b; else pc[i] |= b; }

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
			JS_ReportError(state->context,
				       "invalid range in character class");
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

#undef MATCH_BIT
	    break;

	  case REOP_BACKREF:
	    if (state->flags & JSREG_FOLD)
		pc[0] = (jsbytecode)REOP_BACKREFi;
	    pc[1] = (jsbytecode)ren->u.num;
	    break;

	  case REOP_FLAT:
	    if (state->flags & JSREG_FOLD)
		pc[0] = (jsbytecode)REOP_FLATi;
	    goto emit_flat;

	  case REOP_UCFLAT:
	    if (state->flags & JSREG_FOLD)
		pc[0] = (jsbytecode)REOP_UCFLATi;
	  emit_flat:
	    cp = ren->kid;
	    diff = (jschar *)ren->u.kid2 - cp;
	    pc[1] = (jsbytecode)diff;
	    pc += 2;
	    state->progLength += diff;
	    if (op == REOP_UCFLAT)
		state->progLength += diff;
	    for (i = j = 0; i < (uintN)diff; i++, j++) {
		c = (uintN)cp[i];

		/*
		 * Lay down immediate chars in native byte order so memcmp
		 * with a JSString's chars works.
		 */
#if IS_BIG_ENDIAN
		if (op == REOP_UCFLAT)
		    pc[j++] = (jsbytecode)(c >> 8);
#endif
		pc[j] = (jsbytecode)c;
#if IS_LITTLE_ENDIAN
		if (op == REOP_UCFLAT)
		    pc[j++] = (jsbytecode)(c >> 8);
#endif
	    }
	    break;

	  case REOP_FLAT1:
	    if (state->flags & JSREG_FOLD)
		pc[0] = (jsbytecode)REOP_FLAT1i;
	    pc[1] = (jsbytecode)ren->u.chr;
	    break;

	  case REOP_UCFLAT1:
	    if (state->flags & JSREG_FOLD)
		pc[0] = (jsbytecode)REOP_UCFLAT1i;
	    c = (uintN)ren->u.chr;
	    pc[1] = (jsbytecode)(c >> 8);
	    pc[2] = (jsbytecode)c;
	    break;

	  case REOP_JUMP:
	    diff = next->offset - ren->offset;
	    SET_JUMP_OFFSET(pc, diff);
	    break;

	  default:;
	}

	if (!(ren->flags & RENODE_GOODNEXT))
	    break;
    } while ((ren = next) != NULL);
    return JS_TRUE;
}

JSRegExp *
js_NewRegExp(JSContext *cx, JSString *str, uintN flags)
{
    JSRegExp *re;
    void *mark;
    CompilerState state;
    RENode *ren, *end;
    size_t resize;

    re = NULL;
    mark = PR_ARENA_MARK(&cx->tempPool);

    state.context = cx;
    state.cpbegin = state.cp = str->chars;
    state.flags = flags;
    state.parenCount = 0;
    state.progLength = 0;

    ren = ParseRegExp(&state);
    if (!ren)
	goto out;

    end = NewRENode(&state, REOP_END, NULL);
    if (!end || !SetNext(&state, ren, end))
	goto out;

    if (!AnchorRegExp(&state, ren))
	goto out;
    if (!OptimizeRegExp(&state, ren))
	goto out;

#ifdef DEBUG_notme
    if (!DumpRegExp(cx, ren))
	goto out;
#endif

    resize = sizeof *re + state.progLength - 1;
    re = JS_malloc(cx, PR_ROUNDUP(resize, sizeof(prword)));
    if (!re)
	goto out;
    re->source = str;
    re->length = state.progLength;
    re->lastIndex = 0;
    re->parenCount = state.parenCount;
    re->flags = flags;

    state.progLength = 0;
    if (!EmitRegExp(&state, ren, re)) {
	js_DestroyRegExp(cx, re);
	re = NULL;
	goto out;
    }

    /* Success: lock re->source string. */
    (void) js_LockGCThing(cx, str);
out:
    PR_ARENA_RELEASE(&cx->tempPool, mark);
    return re;
}

JSRegExp *
js_NewRegExpOpt(JSContext *cx, JSString *str, JSString *opt)
{
    uintN flags;
    jschar *cp;

    flags = 0;
    if (opt) {
	for (cp = opt->chars; *cp; cp++) {
	    switch (*cp) {
	      case 'g':
		flags |= JSREG_GLOB;
		break;
	      case 'i':
		flags |= JSREG_FOLD;
		break;
	      default:
		JS_ReportError(cx, "invalid regular expression flag %c",
			       (char) *cp);
		return NULL;
	    }
	}
    }
    return js_NewRegExp(cx, str, flags);
}

void
js_DestroyRegExp(JSContext *cx, JSRegExp *re)
{
    js_UnlockGCThing(cx, re->source);
    JS_free(cx, re);
}

typedef struct MatchState {
    JSContext       *context;           /* for access to regExpStatics */
    JSBool          anchoring;          /* true if multiline anchoring ^/$ */
    jsbytecode      *pcend;             /* pc limit (fencepost) */
    const jschar    *cpbegin, *cpend;   /* cp base address and limit */
    size_t          start;              /* offset from cpbegin to start at */
    ptrdiff_t       skipped;            /* chars skipped anchoring this r.e. */
    uintN           parenCount;         /* number of paren substring matches */
    JSSubString     *maybeParens;       /* possible paren substring pointers */
    JSSubString     *parens;            /* certain paren substring matches */
} MatchState;

/*
 * Returns updated cp on match, null on mismatch.
 */
static const jschar *
MatchRegExp(MatchState *state, jsbytecode *pc, const jschar *cp)
{
    jsbytecode *pc2, *pcend;
    const jschar *cp2, *cp3, *cpbegin, *cpend;
    REOp op;
    JSBool matched;
    ptrdiff_t i, oplen, altlen, matchlen;
    uintN min, max, num;
    JSSubString *parsub;
    const jschar *parstr;
    size_t parlen;
    jschar c, c2;
    uintN bit, byte, size;

    pcend = state->pcend;
    cpbegin = state->cpbegin;
    cpend = state->cpend;

    while (pc < pcend) {
	op = (REOp) *pc;
	oplen = reopsize[op];

	switch (op) {
	  case REOP_EMPTY:
	    pc += oplen;
	    continue;

	  case REOP_ALT:
	    altlen = GET_JUMP_OFFSET(pc);
	    pc2 = pc + oplen;
	    if ((REOp)pc[altlen] != REOP_ALT) {
		pc = pc2;
		continue;
	    }
	    cp2 = MatchRegExp(state, pc2, cp);
	    if (cp2)
		return cp2;
	    pc += altlen;
	    continue;

	  case REOP_BOL:
	    matched = (cp == cpbegin);
	    if (state->context->regExpStatics.multiline) {
		/* Anchor-search only if RegExp.multiline is true. */
		if (state->anchoring) {
		    if (!matched)
			matched = (cp[-1] == '\n');
		} else {
		    state->anchoring = JS_TRUE;
		    for (cp2 = cp; cp2 < cpend; cp2++) {
			if (cp2 == cpbegin || cp2[-1] == '\n') {
			    cp3 = MatchRegExp(state, pc, cp2);
			    if (cp3) {
				state->skipped = cp2 - cp;
				state->anchoring = JS_FALSE;
				return cp3;
			    }
			}
		    }
		    state->anchoring = JS_FALSE;
		}
	    }
	    matchlen = 0;
	    break;

	  case REOP_EOL:
	  case REOP_EOLONLY:
	    matched = (cp == cpend);
	    if (op == REOP_EOL || state->anchoring) {
		if (!matched && state->context->regExpStatics.multiline)
		    matched = (*cp == '\n');
	    } else {
		/* Always anchor-search EOLONLY, which has no BOL analogue. */
		state->anchoring = JS_TRUE;
		for (cp2 = cp; cp2 <= cpend; cp2++) {
		    if (cp2 == cpend || *cp2 == '\n') {
			cp3 = MatchRegExp(state, pc, cp2);
			if (cp3) {
			    state->anchoring = JS_FALSE;
			    state->skipped = cp2 - cp;
			    return cp3;
			}
		    }
		}
		state->anchoring = JS_FALSE;
	    }
	    matchlen = 0;
	    break;

	  case REOP_WBDRY:
	    matched = (cp == cpbegin || !JS_ISWORD(cp[-1])) ^ !JS_ISWORD(*cp);
	    matchlen = 0;
	    break;

	  case REOP_WNONBDRY:
	    matched = (cp == cpbegin || !JS_ISWORD(cp[-1])) ^ JS_ISWORD(*cp);
	    matchlen = 0;
	    break;

	  case REOP_QUANT:
	    pc2 = pc;
	    oplen = GET_JUMP_OFFSET(pc2);
	    pc2 += 2;
	    min = GET_ARGNO(pc2);
	    pc2 += 2;
	    max = GET_ARGNO(pc2);
	    pc2 += 3;

	    /* Reduce state->pcend so we match only the quantified regexp. */
	    state->pcend = pc + oplen;

	    /* If min is non-zero, insist on at least that many matches. */
	    for (num = 0; num < min; num++) {
		cp = MatchRegExp(state, pc2, cp);
		if (!cp) {
		    state->pcend = pcend;
		    return NULL;
		}
	    }

	    /* Try matches from min to max, or forever if max == 0. */
	    for (; !max || num < max; num++) {
		cp2 = MatchRegExp(state, pc2, cp);
		if (!cp2)
		    break;
		cp = cp2;
	    }

	    /* Restore state->pcend and set match and matchlen. */
	    state->pcend = pcend;
	    matched = (min <= num && (!max || num <= max));
	    matchlen = 0;
	    break;

	  case REOP_LPAREN:
	    num = GET_ARGNO(pc);
	    parsub = &state->maybeParens[num];
	    parstr = parsub->chars;
	    parsub->chars = cp;
	    pc += oplen;
	    cp3 = MatchRegExp(state, pc, cp);
	    if (!cp3) {
		/* Restore so later backrefs work, unlike Perl4. */
		parsub->chars = parstr;
		return NULL;
	    }
	    parsub = &state->parens[num];
	    if (!parsub->chars) {
		cp2 = cpbegin + state->start + state->skipped;
		if (cp < cp2) {
		    parsub->chars = cp2;
		    parsub->length -= cp2 - cp;
		} else {
		    parsub->chars = cp;
		}
	    }
	    return cp3;

	  case REOP_RPAREN:
	    num = GET_ARGNO(pc);
	    parsub = &state->maybeParens[num];
	    parsub->length = parlen = cp - parsub->chars;
	    pc += oplen;
	    cp = MatchRegExp(state, pc, cp);
	    if (cp) {
		parsub = &state->parens[num];
		if (!parsub->chars)
		    parsub->length = parlen;
		if (num >= state->parenCount)
		    state->parenCount = num + 1;
	    }
	    return cp;

	  case REOP_BACKREF:
	    num = (uintN)pc[1];
	    parsub = &state->maybeParens[num];
	    matchlen = (ptrdiff_t)parsub->length;
	    matched = (cp + matchlen <= cpend &&
		       !memcmp(cp, parsub->chars, matchlen * sizeof(jschar)));
	    break;

/*
 * See java.lang.String for more on why both toupper and tolower are needed, in
 * comments for equalsIgnoreCase and regionMatches(boolean ignoreCase, ...).
 */
#define MATCH_CHARS_IGNORING_CASE(c, c2)                                      \
    ((c) == (c2) ||                                                           \
     (c = JS_TOUPPER(c)) == (c2 = JS_TOUPPER(c2)) ||                          \
     JS_TOLOWER(c) == JS_TOLOWER(c2))

	  case REOP_BACKREFi:
	    num = (uintN)pc[1];
	    parsub = &state->maybeParens[num];
	    matchlen = (ptrdiff_t)parsub->length;
	    matched = (cp + matchlen <= cpend);
	    if (matched) {
		for (i = 0; i < matchlen; i++) {
		    c  = cp[i];
		    c2 = parsub->chars[i];
		    matched = MATCH_CHARS_IGNORING_CASE(c, c2);
		    if (!matched)
			break;
		}
	    }
	    break;

#define SINGLE_CASES                                                          \
	  case REOP_DOT:                                                      \
	    matched = (cp != cpend && *cp != '\n');                           \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  NONDOT_SINGLE_CASES                                                 \
/* END SINGLE_CASES */

#define NONDOT_SINGLE_CASES                                                   \
	  case REOP_CCLASS:                                                   \
	  case REOP_NCCLASS:                                                  \
	    c = *cp;                                                          \
	    if (c >= CCLASS_CHARSET_SIZE) {                                   \
		matched = (op == REOP_NCCLASS);                               \
	    } else {                                                          \
		byte = (uintN)c >> 3;                                         \
		bit = c & 7;                                                  \
		bit = 1 << bit;                                               \
		matched = pc[1 + byte] & bit;                                 \
	    }                                                                 \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_DIGIT:                                                    \
	    matched = JS_ISDIGIT(*cp);                                        \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_NONDIGIT:                                                 \
	    matched = !JS_ISDIGIT(*cp);                                       \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_ALNUM:                                                    \
	    matched = JS_ISWORD(*cp);                                         \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_NONALNUM:                                                 \
	    matched = !JS_ISWORD(*cp);                                        \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_SPACE:                                                    \
	    matched = JS_ISSPACE(*cp);                                        \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_NONSPACE:                                                 \
	    matched = !JS_ISSPACE(*cp);                                       \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_FLAT1:                                                    \
	    c  = *cp;                                                         \
	    c2 = (jschar)pc[1];                                               \
	    matched = (c == c2);                                              \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_FLAT1i:                                                   \
	    c  = *cp;                                                         \
	    c2 = (jschar)pc[1];                                               \
	    matched = MATCH_CHARS_IGNORING_CASE(c, c2);                       \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_UCFLAT1:                                                  \
	    c  = *cp;                                                         \
	    c2 = ((pc[1] << 8) | pc[2]);                                      \
	    matched = (c == c2);                                              \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_UCFLAT1i:                                                 \
	    c  = *cp;                                                         \
	    c2 = ((pc[1] << 8) | pc[2]);                                      \
	    matched = MATCH_CHARS_IGNORING_CASE(c, c2);                       \
	    matchlen = 1;                                                     \
	    break;                                                            \
                                                                              \
	  case REOP_UCCLASS:                                                  \
	  case REOP_NUCCLASS:                                                 \
	    size = (pc[1] << 8) | pc[2];                                      \
	    oplen += size;                                                    \
	    c = *cp;                                                          \
	    byte = (uintN)c >> 3;                                             \
	    if (byte >= size) {                                               \
		matched = (op == REOP_NUCCLASS);                              \
	    } else {                                                          \
		bit = c & 7;                                                  \
		bit = 1 << bit;                                               \
		matched = pc[3 + byte] & bit;                                 \
	    }                                                                 \
	    matchlen = 1;                                                     \
	    break;                                                            \
/* END NONDOT_SINGLE_CASES */

	  /*
	   * Macro-expand single-char/single-opcode cases here and below.
	   */
	  SINGLE_CASES

	  case REOP_STAR:
	    op = (REOp) *++pc;
	    oplen = reopsize[op];
	    for (cp2 = cp; cp < cpend; cp++) {
		switch (op) {
		  NONDOT_SINGLE_CASES
		  default:
		    PR_ASSERT(0);
		}
		if (!matched)
		    break;
	    }

	  backtracker:
	    pc += oplen;
	    do {
		cp3 = MatchRegExp(state, pc, cp);
		if (cp3)
		    return cp3;
	    } while (--cp >= cp2);
	    return NULL;

	  case REOP_PLUS:
	    op = (REOp) *++pc;
	    oplen = reopsize[op];
	    for (cp2 = cp; cp < cpend; cp++) {
		switch (op) {
		  SINGLE_CASES
		  default:
		    PR_ASSERT(0);
		}
		if (!matched)
		    break;
	    }
	    if (cp == cp2) {
		/* Did not match once, hope for an alternative. */
		return NULL;
	    }
	    /* Matched one or more times, try rest of regexp. */
	    cp2++;
	    goto backtracker;

	  case REOP_OPT:
	    op = (REOp) *++pc;
	    oplen = reopsize[op];
	    switch (op) {
	      SINGLE_CASES
	      default:
		PR_ASSERT(0);
	    }
	    pc += oplen;
	    if (matched) {
		cp2 = MatchRegExp(state, pc, cp + 1);
		if (cp2)
		    return cp2;
	    }
	    continue;

	  case REOP_FLAT:
	    matchlen = (ptrdiff_t)pc[1];
	    oplen += matchlen;
	    matched = (cp + matchlen <= cpend);
	    if (matched) {
		pc2 = pc + 2;
		for (i = 0; i < matchlen; i++) {
		    matched = (cp[i] == (jschar)pc2[i]);
		    if (!matched)
			break;
		}
	    }
	    break;

	  case REOP_FLATi:
	    matchlen = (ptrdiff_t)pc[1];
	    oplen += matchlen;
	    matched = (cp + matchlen <= cpend);
	    if (matched) {
		pc2 = pc + 2;
		for (i = 0; i < matchlen; i++) {
		    c  = cp[i];
		    c2 = (jschar)pc2[i];
		    matched = MATCH_CHARS_IGNORING_CASE(c, c2);
		    if (!matched)
			break;
		}
	    }
	    break;

	  case REOP_UCFLAT:
	    matchlen = (ptrdiff_t)pc[1];
	    oplen += 2 * matchlen;
	    matched = (cp + matchlen <= cpend &&
		       !memcmp(cp, pc + 2, matchlen * sizeof(jschar)));
	    break;

	  case REOP_UCFLATi:
	    matchlen = (ptrdiff_t)pc[1];
	    oplen += matchlen;
	    matched = (cp + matchlen <= cpend);
	    if (matched) {
		pc2 = pc + 2;
		for (i = 0; i < matchlen; i++) {
		    c  = cp[i];
#if IS_BIG_ENDIAN
		    c2 = *pc2++ << 8;
		    c2 |= *pc2++;
#endif
#if IS_LITTLE_ENDIAN
		    c2 = *pc2++;
		    c2 |= *pc2++ << 8;
#endif
		    matched = MATCH_CHARS_IGNORING_CASE(c, c2);
		    if (!matched)
			break;
		}
	    }
	    break;

	  case REOP_JUMP:
	    oplen = GET_JUMP_OFFSET(pc);
	    pc += oplen;
	    continue;

	  case REOP_DOTSTAR:
	    for (cp2 = cp; cp2 < cpend; cp2++)
		if (*cp2 == '\n')
		    break;
	    for (pc2 = pc + oplen; cp2 >= cp; cp2--) {
		cp3 = MatchRegExp(state, pc2, cp2);
		if (cp3)
		    return cp3;
	    }
	    return NULL;

	  case REOP_ANCHOR:
	    pc2 = pc + oplen;
	    if (pc2 == pcend)
		break;
	    for (cp2 = cp; cp2 < cpend; cp2++) {
		cp3 = MatchRegExp(state, pc2, cp2);
		if (cp3) {
		    state->skipped = cp2 - cp;
		    return cp3;
		}
	    }
	    return NULL;

	  case REOP_ANCHOR1:
	    op = (REOp) *++pc;
	    oplen = reopsize[op];
	    pc2 = pc + oplen;
	    PR_ASSERT(pc2 < pcend);
	    for (cp2 = cp; cp < cpend; cp++) {
		switch (op) {
		  NONDOT_SINGLE_CASES
		  default:
		    PR_ASSERT(0);
		}
		if (matched) {
		    cp3 = MatchRegExp(state, pc2, cp);
		    if (cp3) {
			state->skipped = cp - cp2;
			return cp3;
		    }
		}
	    }
	    return NULL;

#undef MATCH_CHARS_IGNORING_CASE
#undef SINGLE_CASES
#undef NONDOT_SINGLE_CASES

	  default:
	    PR_ASSERT(0);
	    return NULL;
	}

	if (!matched)
	    return NULL;
	pc += oplen;
	if (matchlen) {
	    cp += matchlen;
	    if (cp > cpend)
		cp = cpend;
	}
    }

    return cp;
}

JSBool
js_ExecuteRegExp(JSContext *cx, JSRegExp *re, JSString *str, size_t *indexp,
		 JSBool test, jsval *rval)
{
    MatchState state;
    jsbytecode *pc;
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
    pc = re->program;
    state.pcend = pc + re->length;

    /*
     * It's safe to load from cp because JSStrings have a zero at the end,
     * and we never let cp get beyond cpend.
     */
    start = *indexp;
    if (start > str->length)
	start = str->length;
    cp = str->chars + start;
    state.cpbegin = str->chars;
    state.cpend = str->chars + str->length;
    state.start = start;
    state.skipped = 0;

    /*
     * Use the temporary arena pool to grab space for parenthetical matches.
     * After the PR_ARENA_ALLOCATE early return on error, goto out to be sure
     * to free this memory.
     */
    length = 2 * sizeof(JSSubString) * re->parenCount;
    mark = PR_ARENA_MARK(&cx->tempPool);
    PR_ARENA_ALLOCATE(parsub, &cx->tempPool, length);
    if (!parsub) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    memset(parsub, 0, length);
    state.parenCount = 0;
    state.maybeParens = parsub;
    state.parens = parsub + re->parenCount;
    ok = JS_TRUE;

    /*
     * Call the recursive matcher to do the real work.  Return null on mismatch
     * whether testing or not.  On match, return an extended Array object.
     */
    cp = MatchRegExp(&state, pc, cp);
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
    PR_ASSERT(state.parenCount <= re->parenCount);
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
		    morepar = JS_malloc(cx, 10 * sizeof(JSSubString));
		} else if (morenum > res->moreLength) {
		    res->moreLength += 10;
		    morepar = JS_realloc(cx, morepar,
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
	res->leftContext.chars = str->chars + start;
	res->leftContext.length = state.skipped;
    } else {
	/*
	 * For JS1.3 and ECMAv2, emulate Perl5 exactly:
	 *
	 * js1.3        "hi", "hi there"            "hihitherehi therebye"
	 */
	res->leftContext.chars = str->chars;
	res->leftContext.length = start + state.skipped;
    }
    res->rightContext.chars = ep;
    res->rightContext.length = state.cpend - ep;

out:
    PR_ARENA_RELEASE(&cx->tempPool, mark);
    return ok;
}

/************************************************************************/

enum regexp_tinyid {
    REGEXP_SOURCE       = -1,
    REGEXP_GLOBAL       = -2,
    REGEXP_IGNORE_CASE  = -3,
    REGEXP_LAST_INDEX   = -4
};

static JSPropertySpec regexp_props[] = {
    {"source",      REGEXP_SOURCE,      JSPROP_ENUMERATE | JSPROP_READONLY},
    {"global",      REGEXP_GLOBAL,      JSPROP_ENUMERATE | JSPROP_READONLY},
    {"ignoreCase",  REGEXP_IGNORE_CASE, JSPROP_ENUMERATE | JSPROP_READONLY},
    {"lastIndex",   REGEXP_LAST_INDEX,  JSPROP_ENUMERATE},
    {0}
};

static JSBool
regexp_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSRegExp *re;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);
    JS_LOCK_OBJ(cx, obj);
    re = JS_GetInstancePrivate(cx, obj, &js_RegExpClass, NULL);
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
	    *vp = INT_TO_JSVAL((jsint)re->lastIndex);
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
    JS_LOCK_OBJ(cx, obj);
    re = JS_GetInstancePrivate(cx, obj, &js_RegExpClass, NULL);
    if (re && slot == REGEXP_LAST_INDEX) {
	if (!js_ValueToNumber(cx, *vp, &d))
	    return JS_FALSE;
	re->lastIndex = (size_t)d;
    }
    JS_UNLOCK_OBJ(cx, obj);
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
    js_RemoveRoot(cx, &res->input);
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
     REGEXP_STATIC_INPUT,          JSPROP_ENUMERATE,
     regexp_static_getProperty,    regexp_static_setProperty},
    {"multiline",
     REGEXP_STATIC_MULTILINE,      JSPROP_ENUMERATE,
     regexp_static_getProperty,    regexp_static_setProperty},
    {"lastMatch",
     REGEXP_STATIC_LAST_MATCH,     JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"lastParen",
     REGEXP_STATIC_LAST_PAREN,     JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"leftContext",
     REGEXP_STATIC_LEFT_CONTEXT,   JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"rightContext",
     REGEXP_STATIC_RIGHT_CONTEXT,  JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},

    /* XXX should have block scope and local $1, etc. */
    {"$1", 0, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$2", 1, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$3", 2, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$4", 3, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$5", 4, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$6", 5, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$7", 6, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$8", 7, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},
    {"$9", 8, JSPROP_ENUMERATE|JSPROP_READONLY,
     regexp_static_getProperty,    regexp_static_getProperty},

    {0}
};

static void
regexp_finalize(JSContext *cx, JSObject *obj)
{
    JSRegExp *re;

    re = JS_GetPrivate(cx, obj);
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
	re = JS_GetPrivate(xdr->cx, *objp);
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
	re = js_NewRegExp(xdr->cx, source, flags);
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

JSClass js_RegExpClass = {
    "RegExp",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  regexp_getProperty, regexp_setProperty,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,     regexp_finalize,
    NULL,             NULL,             regexp_call,        NULL,
    regexp_xdrObject,
};

static JSBool
regexp_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	        jsval *rval)
{
    JSBool ok;
    JSRegExp *re;
    jschar *chars;
    size_t length, nflags;
    uintN flags;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
	return JS_FALSE;
    ok = JS_TRUE;
    JS_LOCK_OBJ(cx, obj);
    re = JS_GetPrivate(cx, obj);
    if (!re) {
    	*rval = STRING_TO_JSVAL(cx->runtime->emptyString);
	goto out;
    }

    length = re->source->length + 2;
    nflags = 0;
    for (flags = re->flags; flags != 0; flags &= flags - 1)
	nflags++;
    chars = JS_malloc(cx, (length + nflags + 1) * sizeof(jschar));
    if (!chars) {
	ok = JS_FALSE;
	goto out;
    }

    chars[0] = '/';
    js_strncpy(&chars[1], re->source->chars, length - 2);
    chars[length-1] = '/';
    if (nflags) {
	if (re->flags & JSREG_GLOB)
	    chars[length++] = 'g';
	if (re->flags & JSREG_FOLD)
	    chars[length++] = 'i';
    }
    chars[length] = 0;

    str = js_NewString(cx, chars, length, 0);
    if (!str) {
	JS_free(cx, chars);
	ok = JS_FALSE;
	goto out;
    }
    *rval = STRING_TO_JSVAL(str);
out:
    JS_UNLOCK_OBJ(cx, obj);
    return ok;
}

static JSBool
regexp_compile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    JSString *opt, *str;
    JSRegExp *oldre, *re;
    JSBool ok;

    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
	return JS_FALSE;
    opt = NULL;
    JS_LOCK_OBJ(cx, obj);
    if (argc == 0) {
	str = cx->runtime->emptyString;
    } else {
	str = js_ValueToString(cx, argv[0]);
	if (!str) {
	    ok = JS_FALSE;
	    goto out;
	}
	argv[0] = STRING_TO_JSVAL(str);
	if (argc > 1) {
	    opt = js_ValueToString(cx, argv[1]);
	    if (!opt) {
		ok = JS_FALSE;
		goto out;
	    }
	    argv[1] = STRING_TO_JSVAL(opt);
	}
    }
    re = js_NewRegExpOpt(cx, str, opt);
    if (!re) {
	ok = JS_FALSE;
	goto out;
    }
    oldre = JS_GetPrivate(cx, obj);
    ok = JS_SetPrivate(cx, obj, re);
    if (!ok) {
	js_DestroyRegExp(cx, re);
	goto out;
    }
    if (oldre)
	js_DestroyRegExp(cx, oldre);
    *rval = OBJECT_TO_JSVAL(obj);
out:
    JS_UNLOCK_OBJ(cx, obj);
    return ok;
}

static JSBool
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		JSBool test, jsval *rval)
{
    JSBool ok, locked;
    JSRegExp *re;
    JSString *str;
    size_t i;

    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
	return JS_FALSE;
    re = JS_GetPrivate(cx, obj);
    if (!re)
    	return JS_TRUE;
    ok = locked = JS_FALSE;
    if (argc == 0) {
	str = cx->regExpStatics.input;
	if (!str) {
	    JS_ReportError(cx, "no input for /%s/%s%s",
			   JS_GetStringBytes(re->source),
			   (re->flags & JSREG_GLOB) ? "g" : "",
			   (re->flags & JSREG_FOLD) ? "i" : "");
	    goto out;
	}
    } else {
	str = js_ValueToString(cx, argv[0]);
	if (!str)
	    goto out;
	argv[0] = STRING_TO_JSVAL(str);
    }
    if (re->flags & JSREG_GLOB) {
	JS_LOCK_OBJ(cx, obj);
	locked = JS_TRUE;
	i = re->lastIndex;
    } else {
	i = 0;
    }
    ok = js_ExecuteRegExp(cx, re, str, &i, test, rval);
    if (re->flags & JSREG_GLOB)
	re->lastIndex = (*rval == JSVAL_NULL) ? 0 : i;
out:
    if (locked)
	JS_UNLOCK_OBJ(cx, obj);
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
    {js_toSource_str,   regexp_toString,        0},
#endif
    {js_toString_str,   regexp_toString,        0},
    {"compile",         regexp_compile,         1},
    {"exec",            regexp_exec,            0},
    {"test",            regexp_test,            0},
    {0}
};

static JSBool
RegExp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* If not constructing, replace obj with a new RegExp object. */
    if (!cx->fp->constructing) {
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
js_NewRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
    JSString *str;
    JSObject *obj;
    JSRegExp *re;

    str = js_NewStringCopyN(cx, chars, length, 0);
    if (!str)
	return NULL;
    re = js_NewRegExp(cx, str, flags);
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
