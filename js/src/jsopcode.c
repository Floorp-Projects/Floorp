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
 * JS bytecode descriptors, disassemblers, and decompilers.
 */
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#include "prdtoa.h"
#include "prprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

char js_new_str[]     = "new";
char js_delete_str[]  = "delete";
char js_typeof_str[]  = "typeof";
char js_void_str[]    = "void";
char js_null_str[]    = "null";
char js_this_str[]    = "this";
char js_false_str[]   = "false";
char js_true_str[]    = "true";

char *js_incop_str[]  = {"++", "--"};

/* Pollute the namespace locally for MSVC Win16, but not for WatCom.  */
#ifdef __WINDOWS_386__
    #ifdef FAR
	#undef FAR
    #endif
#else  /* !__WINDOWS_386__ */
    #ifndef FAR
	#define FAR
    #endif
#endif /* !__WINDOWS_386__ */

JSCodeSpec FAR js_CodeSpec[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    {name,token,length,nuses,ndefs,prec,format},
#include "jsopcode.def"
#undef OPDEF
};

uintN js_NumCodeSpecs = sizeof js_CodeSpec / sizeof js_CodeSpec[0];

/************************************************************************/

#ifdef DEBUG

void
js_Disassemble(JSContext *cx, JSScript *script, JSBool lines, FILE *fp)
{
    jsbytecode *pc, *end;
    uintN len;

    pc = script->code;
    end = pc + script->length;
    while (pc < end) {
	len = js_Disassemble1(cx, script, pc, pc - script->code, lines, fp);
	if (!len)
	    return;
	pc += len;
    }
}

uintN
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc, uintN loc,
		JSBool lines, FILE *fp)
{
    JSOp op;
    JSCodeSpec *cs;
    intN len, off;
    JSAtom *atom;
    JSString *str;
    char *cstr;

    op = *pc;
    if (op >= JSOP_LIMIT) {
	JS_ReportError(cx, "bytecode %d too large", op);
	return 0;
    }
    cs = &js_CodeSpec[op];
    len = (intN)cs->length;
    fprintf(fp, "%05u:", loc);
    if (lines)
	fprintf(fp, "%4u", JS_PCToLineNumber(cx, script, pc));
    fprintf(fp, "  %s", cs->name);
    switch (cs->format & JOF_TYPEMASK) {
      case JOF_BYTE:
	if (op == JSOP_TRAP) {
	    op = JS_GetTrapOpcode(cx, script, pc);
	    if (op == JSOP_LIMIT)
		return 0;
	    len = (intN)js_CodeSpec[op].length;
	}
	break;

      case JOF_JUMP:
	off = GET_JUMP_OFFSET(pc);
	fprintf(fp, " %u (%d)", loc + off, off);
	break;

      case JOF_CONST:
	atom = GET_ATOM(cx, script, pc);
	str = js_ValueToString(cx, ATOM_KEY(atom));
	if (!str)
	    return 0;
	cstr = js_DeflateString(cx, str->chars, str->length);
	if (!cstr)
	    return 0;
	fprintf(fp, (op == JSOP_STRING) ? " \"%s\"" : " %s", cstr);
	JS_free(cx, cstr);
	break;

      case JOF_UINT16:
	fprintf(fp, " %u", GET_ARGC(pc));
	break;

#if JS_HAS_SWITCH_STATEMENT
      case JOF_TABLESWITCH:
      {
	jsbytecode *pc2, *end;
	jsint i, low, high;

	pc2 = pc;
	off = GET_JUMP_OFFSET(pc2);
	end = pc + off;
	pc2 += 2;
	low = GET_JUMP_OFFSET(pc2);
	pc2 += 2;
	high = GET_JUMP_OFFSET(pc2);
	pc2 += 2;
	fprintf(fp, " defaultOffset %d low %d high %d", off, low, high);
	if (pc2 + 1 < end) {
	    for (i = low; i <= high; i++) {
		off = GET_JUMP_OFFSET(pc2);
		fprintf(fp, "\n\t%d: %d", i, off);
		pc2 += 2;
	    }
	}
	len = 1 + pc2 - pc;
	break;
      }

      case JOF_LOOKUPSWITCH:
      {
	jsbytecode *pc2 = pc;
	uintN npairs;
	jsval key;

	off = GET_JUMP_OFFSET(pc2);
	pc2 += 2;
	npairs = (uintN) GET_ATOM_INDEX(pc2);
	pc2 += 2;
	fprintf(fp, " defaultOffset %d npairs %u", off, npairs);
	while (npairs) {
	    atom = GET_ATOM(cx, script, pc2);
	    pc2 += 2;
	    off = GET_JUMP_OFFSET(pc2);
	    pc2 += 2;

	    key = ATOM_KEY(atom);
	    str = js_ValueToString(cx, key);
	    if (!str)
		return 0;
	    cstr = js_DeflateString(cx, str->chars, str->length);
	    if (!cstr)
		return 0;
	    if (JSVAL_IS_STRING(key))
		fprintf(fp, "\n\t\"%s\": %d", cstr, off);
	    else
		fprintf(fp, "\n\t%s: %d", cstr, off);
	    JS_free(cx, cstr);
	    npairs--;
	}
	len = 1 + pc2 - pc;
	break;
      }
#endif /* JS_HAS_SWITCH_STATEMENT */

      case JOF_QARG:
	fprintf(fp, " %u", GET_ARGNO(pc));
	break;

      case JOF_QVAR:
	fprintf(fp, " %u", GET_VARNO(pc));
	break;

      default:
	JS_ReportError(cx, "unknown bytecode format %x", cs->format);
	return 0;
    }
    fputs("\n", fp);
    return len;
}

#endif /* DEBUG */

/************************************************************************/

/*
 * Sprintf, but with unlimited and automatically allocated buffering.
*/
typedef struct Sprinter {
    JSContext       *context;       /* context executing the decompiler */
    PRArenaPool     *pool;          /* string allocation pool */
    char            *base;          /* base address of buffer in pool */
    size_t          size;           /* size of buffer allocated at base */
    ptrdiff_t       offset;         /* offset of next free char in buffer */
} Sprinter;

#define INIT_SPRINTER(cx, sp, ap, off) \
    ((sp)->context = cx, (sp)->pool = ap, (sp)->base = NULL, (sp)->size = 0,  \
     (sp)->offset = off)

#define OFF2STR(sp,off) ((sp)->base + (off))
#define STR2OFF(sp,str) ((str) - (sp)->base)

static JSBool
SprintAlloc(Sprinter *sp, size_t nb)
{
    if (!sp->base) {
	PR_ARENA_ALLOCATE(sp->base, sp->pool, nb);
    } else {
	PR_ARENA_GROW(sp->base, sp->pool, sp->size, nb);
    }
    if (!sp->base) {
	JS_ReportOutOfMemory(sp->context);
	return JS_FALSE;
    }
    sp->size += nb;
    return JS_TRUE;
}

static ptrdiff_t
SprintPut(Sprinter *sp, const char *s, size_t len)
{
    ptrdiff_t nb, offset;
    char *bp;

    /* Allocate space for s, including the '\0' at the end. */
    nb = (sp->offset + len + 1) - sp->size;
    if (nb > 0 && !SprintAlloc(sp, nb))
	return -1;

    /* Advance offset and copy s into sp's buffer. */
    offset = sp->offset;
    sp->offset += len;
    bp = sp->base + offset;
    memcpy(bp, s, len);
    bp[len] = 0;
    return offset;
}

static ptrdiff_t
Sprint(Sprinter *sp, const char *format, ...)
{
    va_list ap;
    char *bp;
    ptrdiff_t offset;

    va_start(ap, format);
    bp = PR_vsmprintf(format, ap);	/* XXX vsaprintf */
    va_end(ap);
    if (!bp) {
	JS_ReportOutOfMemory(sp->context);
	return -1;
    }
    offset = SprintPut(sp, bp, strlen(bp));
    free(bp);
    return offset;
}

jschar js_EscapeMap[] = {
    '\b', 'b',
    '\f', 'f',
    '\n', 'n',
    '\r', 'r',
    '\t', 't',
    '\v', 'v',
    '"',  '"',
    '\'', '\''
};

static char *
EscapeString(Sprinter *sp, JSString *str, jschar quote)
{
    ptrdiff_t off, len, nb;
    const jschar *s, *t, *u;
    char *bp;
    jschar c;
    JSBool ok;

    off = sp->offset;
    s = str->chars;
    t = s;
    c = *t;
    do {
	while (JS_ISPRINT(c) && c != quote && !(c >> 8))
	    c = *++t;
	len = t - s;

	/* Allocate space for s, including the '\0' at the end. */
	nb = (sp->offset + len + 1) - sp->size;
	if (nb > 0 && !SprintAlloc(sp, nb))
	    return NULL;

	/* Advance sp->offset and copy s into sp's buffer. */
	bp = sp->base + sp->offset;
	sp->offset += len;
	while (--len >= 0)
	    *bp++ = (char) *s++;
	*bp = '\0';

	if (c == 0)
	    break;
	if ((u = js_strchr(js_EscapeMap, c)) != NULL)
	    ok = Sprint(sp, "\\%c", (char)u[1]) >= 0;
	else
	    ok = Sprint(sp, (c >> 8) ? "\\u%04X" : "\\x%02X", c) >= 0;
	if (!ok)
	    return NULL;
	s = ++t;
	c = *t;
    } while (c != 0);
    return OFF2STR(sp, off);
}

JSString *
js_EscapeString(JSContext *cx, JSString *str, jschar quote)
{
    void *mark;
    Sprinter sprinter;
    char *bytes;
    JSString *escstr;

    mark = PR_ARENA_MARK(&cx->tempPool);
    INIT_SPRINTER(cx, &sprinter, &cx->tempPool, 0);
    bytes = EscapeString(&sprinter, str, quote);
    escstr = bytes ? JS_NewStringCopyZ(cx, bytes) : NULL;
    PR_ARENA_RELEASE(&cx->tempPool, mark);
    return escstr;
}

/************************************************************************/

struct JSPrinter {
    Sprinter        sprinter;       /* base class state */
    PRArenaPool     pool;           /* string allocation pool */
    uintN           indent;         /* indentation in spaces */
    JSScript        *script;        /* script being printed */
};

JSPrinter *
js_NewPrinter(JSContext *cx, const char *name, uintN indent)
{
    JSPrinter *jp;

    jp = JS_malloc(cx, sizeof(JSPrinter));
    if (!jp)
	return NULL;
    INIT_SPRINTER(cx, &jp->sprinter, &jp->pool, 0);
    PR_InitArenaPool(&jp->pool, name, 256, 1);
    jp->indent = indent;
    jp->script = NULL;
    return jp;
}

void
js_DestroyPrinter(JSPrinter *jp)
{
    PR_FinishArenaPool(&jp->pool);
    JS_free(jp->sprinter.context, jp);
}

JSString *
js_GetPrinterOutput(JSPrinter *jp)
{
    JSContext *cx;
    JSString *str;

    cx = jp->sprinter.context;
    if (!jp->sprinter.base)
	return cx->runtime->emptyString;
    str = JS_NewStringCopyZ(cx, jp->sprinter.base);
    if (!str)
	return NULL;
    PR_FreeArenaPool(&jp->pool);
    INIT_SPRINTER(cx, &jp->sprinter, &jp->pool, 0);
    return str;
}

int
js_printf(JSPrinter *jp, char *format, ...)
{
    va_list ap;
    char *bp;
    int cc;

    va_start(ap, format);

    /* Expand magic tab into a run of jp->indent spaces. */
    if (*format == '\t') {
	if (Sprint(&jp->sprinter, "%*s", jp->indent, "") < 0)
	    return -1;
	format++;
    }

    /* Allocate temp space, convert format, and put. */
    bp = PR_vsmprintf(format, ap);	/* XXX vsaprintf */
    if (!bp) {
	JS_ReportOutOfMemory(jp->sprinter.context);
	return -1;
    }
    cc = strlen(bp);
    if (SprintPut(&jp->sprinter, bp, (size_t)cc) < 0)
	cc = -1;
    free(bp);

    va_end(ap);
    return cc;
}

JSBool
js_puts(JSPrinter *jp, char *s)
{
    return SprintPut(&jp->sprinter, s, strlen(s)) >= 0;
}

/************************************************************************/

typedef struct SprintStack {
    Sprinter    sprinter;       /* sprinter for postfix to infix buffering */
    ptrdiff_t   *offsets;       /* stack of postfix string offsets */
    jsbytecode  *opcodes;       /* parallel stack of JS opcodes */
    uintN       top;            /* top of stack index */
    JSPrinter   *printer;       /* permanent output goes here */
} SprintStack;

/* Gap between stacked strings to allow for insertion of parens and commas. */
#define PAREN_SLOP	(2 + 1)

/*
 * These pseudo-ops help js_ValueToSource decompile JSOP_SETNAME, JSOP_SETPROP,
 * and JSOP_SETELEM, respectively.  See the first assertion in PushOff.
 */
#define JSOP_GETPROP2   (JSOP_GETPROP + 128)
#define JSOP_GETELEM2   (JSOP_GETELEM + 128)

static JSBool
PushOff(SprintStack *ss, ptrdiff_t off, JSOp op)
{
    PR_ASSERT(JSOP_LIMIT <= 128);
    if (!SprintAlloc(&ss->sprinter, PAREN_SLOP))
	return JS_FALSE;
    PR_ASSERT(ss->top < ss->printer->script->depth);
    ss->offsets[ss->top] = off;
    ss->opcodes[ss->top++] = (op >= 128) ? op - 128 : op;
    ss->sprinter.offset += PAREN_SLOP;
    return JS_TRUE;
}

static ptrdiff_t
PopOff(SprintStack *ss, JSOp op)
{
    uintN top;
    JSCodeSpec *cs, *topcs;
    ptrdiff_t off;

    top   = --ss->top;
    cs    = &js_CodeSpec[op];
    topcs = &js_CodeSpec[ss->opcodes[top]];
    if (topcs->prec != 0 && topcs->prec < cs->prec) {
	ss->offsets[top] -= 2;
	ss->sprinter.offset = ss->offsets[top];
	off = Sprint(&ss->sprinter, "(%s)",
		     OFF2STR(&ss->sprinter, ss->sprinter.offset + 2));
    } else {
	off = ss->sprinter.offset = ss->offsets[top];
    }
    return off;
}

#if JS_HAS_SWITCH_STATEMENT
typedef struct TableEntry {
    jsval       key;
    ptrdiff_t   offset;
} TableEntry;

static int
CompareOffsets(const void *v1, const void *v2, void *arg)
{
    const TableEntry *te1 = v1, *te2 = v2;

    return te1->offset - te2->offset;
}

static JSBool
Decompile(SprintStack *ss, jsbytecode *pc, intN nb);

static JSBool
DecompileSwitch(SprintStack *ss, TableEntry *table, uintN tableLength,
		jsbytecode *pc, ptrdiff_t switchLength, ptrdiff_t defaultOffset)
{
    JSContext *cx;
    JSPrinter *jp;
    char *lval, *rval;
    uintN i;
    ptrdiff_t diff, off, off2;
    jsval key;
    JSString *str;

    cx = ss->sprinter.context;
    jp = ss->printer;

    lval = OFF2STR(&ss->sprinter, PopOff(ss, JSOP_NOP));
    js_printf(jp, "\tswitch (%s) {\n", lval);

    if (tableLength) {
	diff = table[0].offset - defaultOffset;
	if (diff > 0) {
	    jp->indent += 2;
	    js_printf(jp, "\tdefault:\n");
	    jp->indent += 2;
	    if (!Decompile(ss, pc + defaultOffset, diff))
		return JS_FALSE;
	    jp->indent -= 4;
	}
	for (i = 0; i < tableLength; i++) {
	    off = table[i].offset;
	    if (i + 1 < tableLength)
		off2 = table[i + 1].offset;
	    else
		off2 = switchLength;
	    key = table[i].key;
	    str = js_ValueToString(cx, key);
	    if (!str)
		return JS_FALSE;
	    jp->indent += 2;
	    if (JSVAL_IS_STRING(key)) {
		rval = EscapeString(&ss->sprinter, str, '"');
		js_printf(jp, "\tcase \"%s\":\n", rval);
	    } else {
		js_printf(jp, "\tcase %s:\n", JS_GetStringBytes(str));
	    }
	    jp->indent += 2;
	    if (off <= defaultOffset && defaultOffset < off2) {
		diff = defaultOffset - off;
		if (diff != 0) {
		    if (!Decompile(ss, pc + off, diff))
			return JS_FALSE;
		    off = defaultOffset;
		}
		jp->indent -= 2;
		js_printf(jp, "\tdefault:\n");
		jp->indent += 2;
	    }
	    if (!Decompile(ss, pc + off, off2 - off))
		return JS_FALSE;
	    jp->indent -= 4;
	}
    }
    if (defaultOffset == switchLength) {
	jp->indent += 2;
	js_printf(jp, "\tdefault:\n");
	jp->indent -= 2;
    }
    js_printf(jp, "\t}\n");
    return JS_TRUE;
}
#endif

static JSAtom *
GetSlotAtom(JSSymbol *sym, uintN slot)
{
    for (; sym; sym = sym->next) {
	if ((uintN)JSVAL_TO_INT(sym_property(sym)->id) == slot)
	    return sym_atom(sym);
    }
    return NULL;
}

static JSBool
Decompile(SprintStack *ss, jsbytecode *pc, intN nb)
{
    JSContext *cx;
    JSPrinter *jp;
    jsbytecode *endpc, *done;
    ptrdiff_t len, todo, cond, next, tail;
    JSOp op, lastop, saveop;
    JSCodeSpec *cs, *topcs;
    jssrcnote *sn;
    const char *lval, *rval, *xval;
    jsint i, argc;
    char **argv;
    JSAtom *atom;
    JSBool ok;
    jsval key;

/*
 * Local macros
*/
#define DECOMPILE_CODE(pc,nb)	if (!Decompile(ss, pc, nb)) return JS_FALSE
#define POP_STR()		OFF2STR(&ss->sprinter, PopOff(ss, op))
#define LOCAL_ASSERT(expr)	PR_ASSERT(expr); if (!(expr)) return JS_FALSE

    cx = ss->sprinter.context;
    jp = ss->printer;
    endpc = pc + nb;
    todo = -2;			/* NB: different from Sprint() error return. */
    op = JSOP_NOP;
    while (pc < endpc) {
	lastop = op;
	op = *pc;
	cs = &js_CodeSpec[op >= 128 ? op - 128 : op];
	len = cs->length;

	if (cs->token) {
	    switch (cs->nuses) {
	      case 2:
		rval = POP_STR();
		lval = POP_STR();
		if ((sn = js_GetSrcNote(jp->script, pc)) &&
		    SN_TYPE(sn) == SRC_ASSIGNOP) {
		    /* Print only the right operand of the assignment-op. */
		    todo = SprintPut(&ss->sprinter, rval, strlen(rval));
		} else {
		    todo = Sprint(&ss->sprinter, "%s %s %s",
				  lval, cs->token, rval);
		}
		break;

	      case 1:
		rval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s%s", cs->token, rval);
		break;

	      case 0:
		todo = SprintPut(&ss->sprinter, cs->token, strlen(cs->token));
		break;

	      default:
		todo = -2;
		break;
	    }
	} else {
	    switch (op) {
	      case JSOP_NOP:
		/*
		 * Check for extra user parenthesization, a do-while loop,
		 * a for-loop with an empty initializer part, or a labeled
		 * statement.
		 */
		sn = js_GetSrcNote(jp->script, pc);
		todo = -2;
		switch (sn ? SN_TYPE(sn) : SRC_NULL) {
		  case SRC_PAREN:
		    /* Use last real op so PopOff adds parens if needed. */
		    todo = PopOff(ss, lastop);

		    /* Now add user-supplied parens only if PopOff did not. */
		    cs    = &js_CodeSpec[lastop];
		    topcs = &js_CodeSpec[ss->opcodes[ss->top]];
		    if (topcs->prec >= cs->prec) {
			todo = Sprint(&ss->sprinter, "(%s)",
				      OFF2STR(&ss->sprinter, todo));
		    }
		    break;

#if JS_HAS_DO_WHILE_LOOP
		  case SRC_WHILE:
		    js_printf(jp, "\tdo {\n");	/* balance} */
		    jp->indent += 4;
		    break;
#endif /* JS_HAS_DO_WHILE_LOOP */

		  case SRC_FOR:
		    rval = "";

		  forloop:
		    /* Skip the JSOP_NOP or JSOP_POP bytecode. */
		    pc++;

		    /* Get the cond, next, and loop-closing tail offsets. */
		    cond = js_GetSrcNoteOffset(sn, 0);
		    next = js_GetSrcNoteOffset(sn, 1);
		    tail = js_GetSrcNoteOffset(sn, 2);
		    LOCAL_ASSERT(tail + GET_JUMP_OFFSET(pc + tail) == 0);

		    /* Print the keyword and the possibly empty init-part. */
		    js_printf(jp, "\tfor (%s;", rval);

		    if (pc[cond] == JSOP_IFEQ) {
			/* Decompile the loop condition. */
			DECOMPILE_CODE(pc, cond);
			js_printf(jp, " %s", POP_STR());
		    }

		    /* Need a semicolon whether or not there was a cond. */
		    js_puts(jp, ";");

		    if (pc[next] != JSOP_GOTO) {
			/* Decompile the loop updater. */
			DECOMPILE_CODE(pc + next, tail - next - 1);
			js_printf(jp, " %s", POP_STR());
		    }

		    /* Do the loop body. */
		    js_puts(jp, ") {\n");
		    jp->indent += 4;
		    DECOMPILE_CODE(pc + cond + 3, next - cond - 3);
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");

		    /* Set len so pc skips over the entire loop. */
		    len = tail + 3;
		    break;

		  case SRC_LABEL:
		    atom = js_GetAtom(cx, &jp->script->atomMap,
				      (jsatomid) js_GetSrcNoteOffset(sn, 0));
		    jp->indent -= 4;
		    js_printf(jp, "\t%s:\n", ATOM_BYTES(atom));
		    jp->indent += 4;
		    break;

		  case SRC_LABELBRACE:
		    atom = js_GetAtom(cx, &jp->script->atomMap,
				      (jsatomid) js_GetSrcNoteOffset(sn, 0));
		    js_printf(jp, "\t%s: {\n", ATOM_BYTES(atom));
		    jp->indent += 4;
		    break;

		  case SRC_ENDBRACE:
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");
		    break;

		  default:;
		}
		break;

	      case JSOP_PUSH:
	      case JSOP_PUSHOBJ:
		todo = Sprint(&ss->sprinter, "");
		break;

	      case JSOP_POP:
	      case JSOP_POPV:
		sn = js_GetSrcNote(jp->script, pc);
		switch (sn ? SN_TYPE(sn) : SRC_NULL) {
		  case SRC_FOR:
		    rval = POP_STR();
		    todo = -2;
		    goto forloop;

		  case SRC_COMMA:
		    /* Pop and save to avoid blowing stack depth budget. */
		    lval = JS_strdup(cx, POP_STR());
		    if (!lval)
			return JS_FALSE;

		    /* The offset tells distance to next comma, or to end. */
		    done = pc + len;
		    pc += js_GetSrcNoteOffset(sn, 0);
		    LOCAL_ASSERT(pc == endpc ||
				 *pc == JSOP_POP || *pc == JSOP_POPV ||
				 *pc == JSOP_NOP);

		    if (!Decompile(ss, done, pc - done)) {
			JS_free(cx, (char *)lval);
			return JS_FALSE;
		    }

		    /* Pop Decompile result and print comma expression. */
		    rval = POP_STR();
		    todo = Sprint(&ss->sprinter, "%s, %s", lval, rval);
		    JS_free(cx, (char *)lval);
		    len = 0;
		    break;

		  case SRC_HIDDEN:
		    /* Hide this pop, it's from a goto in a with or for/in. */
		    todo = -2;
		    break;

		  default:
		    rval = POP_STR();
		    if (*rval != '\0')
			js_printf(jp, "\t%s;\n", rval);
		    todo = -2;
		    break;
		}
		break;

	      case JSOP_ENTERWITH:
		rval = POP_STR();
		js_printf(jp, "\twith (%s) {\n", rval);
		jp->indent += 4;
		todo = -2;
		break;

	      case JSOP_LEAVEWITH:
		sn = js_GetSrcNote(jp->script, pc);
		todo = -2;
		if (sn && SN_TYPE(sn) == SRC_HIDDEN)
		    break;
		jp->indent -= 4;
		js_printf(jp, "\t}\n");
		break;

	      case JSOP_RETURN:
		rval = POP_STR();
		if (*rval != '\0')
		    js_printf(jp, "\t%s %s;\n", cs->name, rval);
		else
		    js_printf(jp, "\t%s;\n", cs->name);
		todo = -2;
		break;

	      case JSOP_GOTO:
		sn = js_GetSrcNote(jp->script, pc);
		switch (sn ? SN_TYPE(sn) : SRC_NULL) {
		  case SRC_CONT2LABEL:
		    atom = js_GetAtom(cx, &jp->script->atomMap,
				      (jsatomid) js_GetSrcNoteOffset(sn, 0));
		    js_printf(jp, "\tcontinue %s;\n", ATOM_BYTES(atom));
		    break;
		  case SRC_CONTINUE:
		    js_printf(jp, "\tcontinue;\n");
		    break;
		  case SRC_BREAK2LABEL:
		    atom = js_GetAtom(cx, &jp->script->atomMap,
				      (jsatomid) js_GetSrcNoteOffset(sn, 0));
		    js_printf(jp, "\tbreak %s;\n", ATOM_BYTES(atom));
		    break;
		  default:
		    js_printf(jp, "\tbreak;\n");
		    break;
		}
		todo = -2;
		break;

	      case JSOP_IFEQ:
		len = GET_JUMP_OFFSET(pc);
		sn = js_GetSrcNote(jp->script, pc);

		switch (sn ? SN_TYPE(sn) : SRC_NULL) {
		  case SRC_IF:
		  case SRC_IF_ELSE:
		    rval = POP_STR();
		    js_printf(jp, "\tif (%s) {\n", rval);
		    jp->indent += 4;
		    if (SN_TYPE(sn) == SRC_IF) {
			DECOMPILE_CODE(pc + 3, len - 3);
		    } else {
			DECOMPILE_CODE(pc + 3, len - 6);
			jp->indent -= 4;
			pc += len - 3;
			len = GET_JUMP_OFFSET(pc);
			js_printf(jp, "\t} else {\n");
			jp->indent += 4;
			DECOMPILE_CODE(pc + 3, len - 3);
		    }
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");
		    todo = -2;
		    break;

		  case SRC_WHILE:
		    rval = POP_STR();
		    js_printf(jp, "\twhile (%s) {\n", rval);
		    jp->indent += 4;
		    DECOMPILE_CODE(pc + 3, len - 6);
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");
		    todo = -2;
		    break;

		  case SRC_COND:
		    xval = JS_strdup(cx, POP_STR());
		    if (!xval)
			return JS_FALSE;
		    DECOMPILE_CODE(pc + 3, len - 6);
		    lval = JS_strdup(cx, POP_STR());
		    if (!lval) {
			JS_free(cx, (void *)xval);
			return JS_FALSE;
		    }
		    pc += len - 3;
		    LOCAL_ASSERT(*pc == JSOP_GOTO);
		    len = GET_JUMP_OFFSET(pc);
		    DECOMPILE_CODE(pc + 3, len - 3);
		    rval = POP_STR();
		    todo = Sprint(&ss->sprinter, "%s ? %s : %s",
				  xval, lval, rval);
		    JS_free(cx, (void *)xval);
		    JS_free(cx, (void *)lval);
		    break;

		  default:
#if JS_BUG_SHORT_CIRCUIT
		    {
			/* top is the first clause in a disjunction (||). */
			jsbytecode *ifeq;

			lval = JS_strdup(cx, POP_STR());
			if (!lval)
			    return JS_FALSE;
			ifeq = pc + len;
			LOCAL_ASSERT(pc[3] == JSOP_TRUE);
			pc += 4;
			LOCAL_ASSERT(*pc == JSOP_GOTO);
			done = pc + GET_JUMP_OFFSET(pc);
			pc += 3;
			DECOMPILE_CODE(pc, done - ifeq);
			rval = POP_STR();
			todo = Sprint(&ss->sprinter, "%s || %s", lval, rval);
			JS_free(cx, (void *)lval);
			len = done - pc;
		    }
#endif
		    break;
		}
		break;

	      case JSOP_IFNE:
#if JS_HAS_DO_WHILE_LOOP
		/* Check for a do-while loop's upward branch. */
		sn = js_GetSrcNote(jp->script, pc);
		if (sn && SN_TYPE(sn) == SRC_WHILE) {
		    jp->indent -= 4;
		    /* {balance: */
		    js_printf(jp, "\t} while (%s);\n", POP_STR());
		    todo = -2;
		    break;
		}
#endif /* JS_HAS_DO_WHILE_LOOP */

#if JS_BUG_SHORT_CIRCUIT
		{
		    jsbytecode *ifne;

		    /* This bytecode is used only for conjunction (&&). */
		    lval = JS_strdup(cx, POP_STR());
		    if (!lval)
			return JS_FALSE;
		    ifne = pc + GET_JUMP_OFFSET(pc);
		    len++;
		    pc += len;
		    LOCAL_ASSERT(pc[-1] == JSOP_FALSE);
		    LOCAL_ASSERT(*pc == JSOP_GOTO);
		    done = pc + GET_JUMP_OFFSET(pc);
		    pc += 3;
		    DECOMPILE_CODE(pc, done - ifne);
		    rval = POP_STR();
		    todo = Sprint(&ss->sprinter, "%s && %s", lval, rval);
		    JS_free(cx, (void *)lval);
		    len = done - pc;
		}
#endif
		break;

#if !JS_BUG_SHORT_CIRCUIT
	      case JSOP_OR:
		/* Top of stack is the first clause in a disjunction (||). */
		lval = JS_strdup(cx, POP_STR());
		if (!lval)
		    return JS_FALSE;
		done = pc + GET_JUMP_OFFSET(pc);
		pc += len;
		len = done - pc;
		DECOMPILE_CODE(pc, len);
		rval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s || %s", lval, rval);
		JS_free(cx, (char *)lval);
		break;

	      case JSOP_AND:
		/* Top of stack is the first clause in a conjunction (&&). */
		lval = JS_strdup(cx, POP_STR());
		if (!lval)
		    return JS_FALSE;
		done = pc + GET_JUMP_OFFSET(pc);
		pc += len;
		len = done - pc;
		DECOMPILE_CODE(pc, len);
		rval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s && %s", lval, rval);
		JS_free(cx, (char *)lval);
		break;
#endif

	      case JSOP_FORNAME:
		atom = GET_ATOM(cx, jp->script, pc);
		rval = POP_STR();
		xval = 0;
		lval = ATOM_BYTES(atom);
		sn = js_GetSrcNote(jp->script, pc);
		pc += 3;
		goto do_forinloop;

	      case JSOP_FORPROP:
		atom = GET_ATOM(cx, jp->script, pc);
		rval = POP_STR();
		xval = 0;
		lval = POP_STR();
		sn = NULL;
		pc += 3;
		goto do_forinloop;

	      case JSOP_FORELEM:
		rval = POP_STR();
		xval = POP_STR();
		lval = POP_STR();
		sn = NULL;
		pc++;

	      do_forinloop:
		LOCAL_ASSERT(*pc == JSOP_IFEQ);
		len = GET_JUMP_OFFSET(pc);
		js_printf(jp, "\tfor (%s%s",
			  (sn && SN_TYPE(sn) == SRC_VAR) ? "var " : "", lval);
		if (xval)
		    js_printf(jp, "[%s]", xval);
		js_printf(jp, " in %s) {\n", rval);
		jp->indent += 4;
		DECOMPILE_CODE(pc + 3, len - 6);
		jp->indent -= 4;
		js_printf(jp, "\t}\n");
		todo = -2;
		break;

	      case JSOP_DUP2:
		rval = OFF2STR(&ss->sprinter, ss->offsets[ss->top-2]);
		todo = SprintPut(&ss->sprinter, rval, strlen(rval));
		if (todo < 0 || !PushOff(ss, todo, op))
		    return JS_FALSE;
		/* FALL THROUGH */

	      case JSOP_DUP:
		rval = OFF2STR(&ss->sprinter, ss->offsets[ss->top-1]);
		todo = SprintPut(&ss->sprinter, rval, strlen(rval));
		break;

	      case JSOP_SETARG:
		atom = GetSlotAtom(jp->script->args, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_setname;

	      case JSOP_SETVAR:
		atom = GetSlotAtom(jp->script->vars, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		goto do_setname;

	      case JSOP_SETNAME:
		atom = GET_ATOM(cx, jp->script, pc);
	      do_setname:
		lval = ATOM_BYTES(atom);
		rval = POP_STR();
		if ((sn = js_GetSrcNote(jp->script, pc - 1)) &&
		    SN_TYPE(sn) == SRC_ASSIGNOP) {
		    todo = Sprint(&ss->sprinter, "%s %s= %s",
				  lval, js_CodeSpec[pc[-1]].token, rval);
		} else {
		    sn = js_GetSrcNote(jp->script, pc);
		    todo = Sprint(&ss->sprinter, "%s%s = %s",
				  (sn && SN_TYPE(sn) == SRC_VAR) ? "var " : "",
				  lval, rval);
		}
		break;

	      case JSOP_NEW:
	      case JSOP_CALL:
		saveop = op;
		op = JSOP_NOP;           /* turn off parens */
		argc = GET_ARGC(pc);
		argv = JS_malloc(cx, (size_t)(argc + 1) * sizeof *argv);
		if (!argv)
		    return JS_FALSE;

		ok = JS_TRUE;
		for (i = argc; i > 0; i--) {
		    argv[i] = JS_strdup(cx, POP_STR());
		    if (!argv[i]) {
			ok = JS_FALSE;
			break;
		    }
		}

		/* Skip the JSOP_PUSHOBJ-created empty string. */
		LOCAL_ASSERT(ss->top >= 2);
		(void) PopOff(ss, op);

		/* Get the callee's decompiled image in argv[0]. */
		argv[0] = JS_strdup(cx, POP_STR());
		if (!argv[i])
		    ok = JS_FALSE;

		lval = "(", rval = ")";
		if (saveop == JSOP_NEW) {
		    todo = Sprint(&ss->sprinter, "%s %s%s",
				  js_new_str, argv[0], lval);
		} else {
		    todo = Sprint(&ss->sprinter, "%s%s",
				  argv[0], lval);
		}
		if (todo < 0)
		    ok = JS_FALSE;

		for (i = 1; i <= argc; i++) {
		    if (!argv[i] ||
			Sprint(&ss->sprinter, "%s%s",
			       argv[i], (i < argc) ? ", " : "") < 0) {
			ok = JS_FALSE;
			break;
		    }
		}
		if (Sprint(&ss->sprinter, rval) < 0)
		    ok = JS_FALSE;

		for (i = 0; i <= argc; i++) {
		    if (argv[i])
			JS_free(cx, argv[i]);
		}
		JS_free(cx, argv);
		if (!ok)
		    return JS_FALSE;
		op = saveop;
		break;

	      case JSOP_DELNAME:
		atom = GET_ATOM(cx, jp->script, pc);
		lval = ATOM_BYTES(atom);
		todo = Sprint(&ss->sprinter, "%s %s", js_delete_str, lval);
		break;

	      case JSOP_DELPROP:
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s %s.%s",
			      js_delete_str, lval, ATOM_BYTES(atom));
		break;

	      case JSOP_DELELEM:
		xval = POP_STR();
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s %s[%s]",
			      js_delete_str, lval, xval);
		break;

	      case JSOP_TYPEOF:
	      case JSOP_VOID:
		rval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s %s", cs->name, rval);
		break;

	      case JSOP_INCARG:
	      case JSOP_DECARG:
		atom = GetSlotAtom(jp->script->args, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_incatom;

	      case JSOP_INCVAR:
	      case JSOP_DECVAR:
		atom = GetSlotAtom(jp->script->vars, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		goto do_incatom;

	      case JSOP_INCNAME:
	      case JSOP_DECNAME:
		atom = GET_ATOM(cx, jp->script, pc);
	      do_incatom:
		lval = ATOM_BYTES(atom);
		todo = Sprint(&ss->sprinter, "%s%s",
			      js_incop_str[!(cs->format & JOF_INC)], lval);
		break;

	      case JSOP_INCPROP:
	      case JSOP_DECPROP:
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s%s.%s",
			      js_incop_str[!(cs->format & JOF_INC)],
			      lval, ATOM_BYTES(atom));
		break;

	      case JSOP_INCELEM:
	      case JSOP_DECELEM:
		xval = POP_STR();
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s%s[%s]",
			      js_incop_str[!(cs->format & JOF_INC)],
			      lval, xval);
		break;

	      case JSOP_ARGINC:
	      case JSOP_ARGDEC:
		atom = GetSlotAtom(jp->script->args, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_atominc;

	      case JSOP_VARINC:
	      case JSOP_VARDEC:
		atom = GetSlotAtom(jp->script->vars, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		goto do_atominc;

	      case JSOP_NAMEINC:
	      case JSOP_NAMEDEC:
		atom = GET_ATOM(cx, jp->script, pc);
	      do_atominc:
		lval = ATOM_BYTES(atom);
		todo = Sprint(&ss->sprinter, "%s%s",
			      lval, js_incop_str[!(cs->format & JOF_INC)]);
		break;

	      case JSOP_PROPINC:
	      case JSOP_PROPDEC:
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s.%s%s",
			      lval, ATOM_BYTES(atom),
			      js_incop_str[!(cs->format & JOF_INC)]);
		break;

	      case JSOP_ELEMINC:
	      case JSOP_ELEMDEC:
		xval = POP_STR();
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s[%s]%s",
			      lval, xval,
			      js_incop_str[!(cs->format & JOF_INC)]);
		break;

	      case JSOP_GETPROP2:
		op = JSOP_GETPROP;
		(void) PopOff(ss, lastop);
		/* FALL THROUGH */

	      case JSOP_GETPROP:
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s.%s", lval, ATOM_BYTES(atom));
		break;

	      case JSOP_SETPROP:
		rval = POP_STR();
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		if ((sn = js_GetSrcNote(jp->script, pc - 1)) &&
		    SN_TYPE(sn) == SRC_ASSIGNOP) {
		    todo = Sprint(&ss->sprinter, "%s.%s %s= %s",
				  lval, ATOM_BYTES(atom),
				  js_CodeSpec[pc[-1]].token, rval);
		} else {
		    todo = Sprint(&ss->sprinter, "%s.%s = %s",
				  lval, ATOM_BYTES(atom), rval);
		}
		break;

	      case JSOP_GETELEM2:
		op = JSOP_GETELEM;
		(void) PopOff(ss, lastop);
		/* FALL THROUGH */

	      case JSOP_GETELEM:
		op = JSOP_NOP;           /* turn off parens */
		xval = POP_STR();
		op = JSOP_GETELEM;
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s[%s]", lval, xval);
		break;

	      case JSOP_SETELEM:
		op = JSOP_NOP;           /* turn off parens */
		rval = POP_STR();
		xval = POP_STR();
		op = JSOP_SETELEM;
		lval = POP_STR();
		if ((sn = js_GetSrcNote(jp->script, pc - 1)) &&
		    SN_TYPE(sn) == SRC_ASSIGNOP) {
		    todo = Sprint(&ss->sprinter, "%s[%s] %s= %s",
				  lval, xval,
				  js_CodeSpec[pc[-1]].token, rval);
		} else {
		    todo = Sprint(&ss->sprinter, "%s[%s] = %s",
				  lval, xval, rval);
		}
		break;

	      case JSOP_GETARG:
		atom = GetSlotAtom(jp->script->args, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_name;

	      case JSOP_GETVAR:
		atom = GetSlotAtom(jp->script->vars, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		goto do_name;

	      case JSOP_NAME:
		atom = GET_ATOM(cx, jp->script, pc);
	      do_name:
		sn = js_GetSrcNote(jp->script, pc);
		todo = Sprint(&ss->sprinter,
			      (sn && SN_TYPE(sn) == SRC_VAR) ? "var %s" : "%s",
			      ATOM_BYTES(atom));
		break;

	      case JSOP_UINT16:
		i = (jsint) GET_ATOM_INDEX(pc);
		todo = Sprint(&ss->sprinter, "%u", (unsigned) i);
		break;

	      case JSOP_NUMBER:
	      case JSOP_STRING:
	      case JSOP_OBJECT:
		atom = GET_ATOM(cx, jp->script, pc);
		key = ATOM_KEY(atom);
		if (JSVAL_IS_INT(key)) {
		    long ival = (long)JSVAL_TO_INT(key);
		    todo = Sprint(&ss->sprinter, "%ld", ival);
		} else if (JSVAL_IS_DOUBLE(key)) {
		    char buf[32];
		    PR_cnvtf(buf, sizeof buf, 20, *JSVAL_TO_DOUBLE(key));
		    todo = Sprint(&ss->sprinter, buf);
		} else if (JSVAL_IS_STRING(key)) {
		    rval = EscapeString(&ss->sprinter, ATOM_TO_STRING(atom),
					'"');
		    if (!rval)
			return JS_FALSE;
		    todo = Sprint(&ss->sprinter, "\"%s\"", rval);
		} else if (JSVAL_IS_OBJECT(key)) {
		    JSString *str = js_ObjectToString(cx, JSVAL_TO_OBJECT(key));
		    if (!str)
			return JS_FALSE;
		    todo = SprintPut(&ss->sprinter,
				     JS_GetStringBytes(str),
				     str->length);
		} else {
		    todo = -2;
		}
		break;

#if JS_HAS_SWITCH_STATEMENT
	      case JSOP_TABLESWITCH:
	      {
		jsbytecode *pc2, *end;
		ptrdiff_t off, off2;
		jsint j, n, low, high;
		TableEntry *table;

		sn = js_GetSrcNote(jp->script, pc);
		len = js_GetSrcNoteOffset(sn, 0);
		pc2 = pc;
		off = GET_JUMP_OFFSET(pc2);
		end = pc + off;
		pc2 += 2;
		low = GET_JUMP_OFFSET(pc2);
		pc2 += 2;
		high = GET_JUMP_OFFSET(pc2);

		n = high - low + 1;
		table = JS_malloc(cx, (size_t)n * sizeof *table);
		if (!table)
		    return JS_FALSE;
		if (pc2 + 3 >= end) {
		    j = 0;
		} else {
		    for (i = j = 0; i < n; i++) {
			pc2 += 2;
			off2 = GET_JUMP_OFFSET(pc2);
			if (off2) {
			    table[j].key = INT_TO_JSVAL(low + i);
			    table[j++].offset = off2;
			}
		    }
		}
		js_qsort(table, (size_t)j, sizeof *table, CompareOffsets, NULL);

		ok = DecompileSwitch(ss, table, (uintN)j, pc, len, off);
		JS_free(cx, table);
		if (!ok)
		    return ok;
		todo = -2;
		break;
	      }

	      case JSOP_LOOKUPSWITCH:
	      {
		jsbytecode *pc2 = pc;
		ptrdiff_t off, off2;
		jsint npairs;
		TableEntry *table;

		sn = js_GetSrcNote(jp->script, pc);
		len = js_GetSrcNoteOffset(sn, 0);
		off = GET_JUMP_OFFSET(pc2);
		pc2 += 2;
		npairs = (jsint) GET_ATOM_INDEX(pc2);

		table = JS_malloc(cx, (size_t)npairs * sizeof *table);
		if (!table)
		    return JS_FALSE;
		for (i = 0; i < npairs; i++) {
		    pc2 += 2;
		    atom = GET_ATOM(cx, jp->script, pc2);
		    pc2 += 2;
		    off2 = GET_JUMP_OFFSET(pc2);
		    table[i].key = ATOM_KEY(atom);
		    table[i].offset = off2;
		}

		ok = DecompileSwitch(ss, table, (uintN)npairs, pc, len, off);
		JS_free(cx, table);
		if (!ok)
		    return ok;
		todo = -2;
		break;
	      }
#endif /* JS_HAS_SWITCH_STATEMENT */

#if !JS_BUG_FALLIBLE_EQOPS
	      case JSOP_NEW_EQ:
	      case JSOP_NEW_NE:
		rval = POP_STR();
		lval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s %c%s %s",
			      lval,
			      (op == JSOP_NEW_EQ) ? '=' : '!',
#if JS_HAS_TRIPLE_EQOPS
			      JSVERSION_IS_ECMA(cx->version) ? "==" :
#endif
			      "=",
			      rval);
		break;
#endif /* !JS_BUG_FALLIBLE_EQOPS */

#if JS_HAS_LEXICAL_CLOSURE
	      case JSOP_CLOSURE:
	      {
		JSObject *obj;
		JSFunction *fun;
		JSPrinter *jp2;
		JSString *str;

		atom = GET_ATOM(cx, jp->script, pc);
		PR_ASSERT(ATOM_IS_OBJECT(atom));
		obj = ATOM_TO_OBJECT(atom);
		fun = JS_GetPrivate(cx, obj);
		jp2 = js_NewPrinter(cx,
				    fun->atom ? ATOM_BYTES(fun->atom) : NULL,
				    jp->indent);
		if (!jp2)
		    return JS_FALSE;
		todo = -1;
		if (js_DecompileFunction(jp2, fun, JS_FALSE)) {
		    str = js_GetPrinterOutput(jp2);
		    if (str) {
			todo = SprintPut(&ss->sprinter,
					 JS_GetStringBytes(str),
					 str->length);
		    }
		}
		js_DestroyPrinter(jp2);
		break;
	      }
#endif /* JS_HAS_LEXICAL_CLOSURE */

#if JS_HAS_EXPORT_IMPORT
	      case JSOP_EXPORTALL:
		js_printf(jp, "\texport *\n");
		todo = -2;
		break;

	      case JSOP_EXPORTNAME:
		atom = GET_ATOM(cx, jp->script, pc);
		js_printf(jp, "\texport %s\n", ATOM_BYTES(atom));
		todo = -2;
		break;

	      case JSOP_IMPORTALL:
		lval = POP_STR();
		js_printf(jp, "\timport %s.*\n", lval);
		todo = -2;
		break;

	      case JSOP_IMPORTPROP:
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		js_printf(jp, "\timport %s.%s\n", lval, ATOM_BYTES(atom));
		todo = -2;
		break;

	      case JSOP_IMPORTELEM:
		xval = POP_STR();
		op = JSOP_GETELEM;
		lval = POP_STR();
		js_printf(jp, "\timport %s[%s]\n", lval, xval);
		todo = -2;
		break;
#endif /* JS_HAS_EXPORT_IMPORT */

	      case JSOP_TRAP:
		op = JS_GetTrapOpcode(cx, jp->script, pc);
		if (op == JSOP_LIMIT)
		    return JS_FALSE;
		*pc = op;
		cs = &js_CodeSpec[op];
		len = cs->length;
		DECOMPILE_CODE(pc, len);
		*pc = JSOP_TRAP;
		todo = -2;
		break;

#ifdef JS_HAS_OBJECT_LITERAL
	      case JSOP_NEWINIT:
		LOCAL_ASSERT(ss->top >= 2);
		(void) PopOff(ss, op);
		lval = POP_STR();
#if JS_HAS_SHARP_VARS
		op = pc[len];
		if (op == JSOP_DEFSHARP) {
		    pc += len;
		    cs = &js_CodeSpec[op];
		    len = cs->length;
		    i = (jsint) GET_ATOM_INDEX(pc);
		    todo = Sprint(&ss->sprinter, "#%u=%c",
				  (unsigned) i,
				  (*lval == 'O') ? '{' : '[');
		    /* balance}] */
		} else
#endif /* JS_HAS_SHARP_VARS */
		{
		    todo = Sprint(&ss->sprinter, (*lval == 'O') ? "{" : "[");
		    /* balance}] */
		}
		break;

	      case JSOP_ENDINIT:
		rval = POP_STR();
		sn = js_GetSrcNote(jp->script, pc);
		todo = Sprint(&ss->sprinter, "%s%s%c",
			      rval,
			      (sn && SN_TYPE(sn) == SRC_COMMA) ? ", " : "",
			      /* [balance */
			      (*rval == '{') ? '}' : ']');
		break;

	      case JSOP_INITPROP:
		rval = POP_STR();
		atom = GET_ATOM(cx, jp->script, pc);
		xval = ATOM_BYTES(atom);
		lval = POP_STR();
	      do_initprop:
		todo = Sprint(&ss->sprinter, "%s%s%s:%s",
			      lval, (lval[1] != '\0') ? ", " : "",
			      xval, rval);
		break;

	      case JSOP_INITELEM:
		rval = POP_STR();
		xval = POP_STR();
		lval = POP_STR();
		sn = js_GetSrcNote(jp->script, pc);
		if (sn && SN_TYPE(sn) == SRC_LABEL)
		    goto do_initprop;
		todo = Sprint(&ss->sprinter, "%s%s%s",
			      lval,
			      (lval[1] != '\0' || *xval != '0') ? ", " : "",
			      rval);
		break;

#if JS_HAS_SHARP_VARS
	      case JSOP_USESHARP:
		i = (jsint) GET_ATOM_INDEX(pc);
		todo = Sprint(&ss->sprinter, "#%u#", (unsigned) i);
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_OBJECT_LITERAL */

	      default:
		todo = -2;
		break;
	    }
	}

	if (todo < 0) {
	    if (todo == -1)
		return JS_FALSE;
	} else {
	    if (!PushOff(ss, todo, op))
		return JS_FALSE;
	}
	pc += len;
    }

/*
 * Undefine local macros.
 */
#undef DECOMPILE_CODE
#undef POP_STR
#undef LOCAL_ASSERT

    return JS_TRUE;
}


JSBool
js_DecompileCode(JSPrinter *jp, JSScript *script, jsbytecode *pc, uintN len)
{
    SprintStack ss;
    JSContext *cx;
    void *mark;
    JSScript *oldscript;
    JSBool ok;
    char *last;

    /* Initialize a sprinter for use with the offset stack. */
    ss.printer = jp;
    cx = jp->sprinter.context;
    mark = PR_ARENA_MARK(&cx->tempPool);
    INIT_SPRINTER(cx, &ss.sprinter, &cx->tempPool, PAREN_SLOP);

    /* Initialize the offset and opcode stacks. */
    ss.offsets = JS_malloc(cx, script->depth * sizeof *ss.offsets);
    ss.opcodes = JS_malloc(cx, script->depth * sizeof *ss.opcodes);
    if (!ss.offsets || !ss.opcodes) {
	if (ss.offsets)
	    JS_free(cx, ss.offsets);
	return JS_FALSE;
    }
    ss.top = 0;

    /* Call recursive subroutine to do the hard work. */
    oldscript = jp->script;
    jp->script = script;
    ok = Decompile(&ss, pc, len);
    jp->script = oldscript;

    /* If the given code didn't empty the stack, do it now. */
    if (ss.top) {
	do {
	    last = OFF2STR(&ss.sprinter, PopOff(&ss, JSOP_NOP));
	} while (ss.top);
	js_printf(jp, "%s", last);
    }

    /* Free and clear temporary stuff. */
    JS_free(cx, ss.offsets);
    JS_free(cx, ss.opcodes);
    PR_ARENA_RELEASE(&cx->tempPool, mark);
    return ok;
}

JSBool
js_DecompileScript(JSPrinter *jp, JSScript *script)
{
    PR_ASSERT(JS_IS_LOCKED(jp->sprinter.context));
    return js_DecompileCode(jp, script, script->code, (uintN)script->length);
}

JSBool
js_DecompileFunction(JSPrinter *jp, JSFunction *fun, JSBool newlines)
{
    JSSymbol *arg;
    JSAtom *atom;
    uintN indent;

    PR_ASSERT(JS_IS_LOCKED(jp->sprinter.context));
    if (newlines) {
	js_puts(jp, "\n");
	js_printf(jp, "\t");
    }
    js_printf(jp, "function %s(", fun->atom ? ATOM_BYTES(fun->atom) : "");
    if (fun->script) {
	for (arg = fun->script->args; arg; arg = arg->next) {
	    atom = sym_atom(arg);
	    js_printf(jp, "%s%s", ATOM_BYTES(atom), arg->next ? ", " : "");
	}
    }
    js_puts(jp, ") {\n");
    indent = jp->indent;
    jp->indent += 4;
    if (fun->call) {
	js_printf(jp, "\t[native code]\n");
    } else {
	if (!js_DecompileScript(jp, fun->script)) {
	    jp->indent = indent;
	    return JS_FALSE;
	}
    }
    jp->indent -= 4;
    js_printf(jp, "\t}");
    if (newlines)
	js_puts(jp, "\n");
    return JS_TRUE;
}

JSString *
js_ValueToSource(JSContext *cx, jsval v)
{
    JSStackFrame *fp;
    JSScript *script;
    jsbytecode *pc, *begin, *end, *tmp;
    uint32 format, mode;
    intN depth;
    jsval *limit;
    jssrcnote *sn;
    uintN len;
    JSPrinter *jp;
    JSString *name;

    fp = cx->fp;
    script = fp->script;
    if (!script)
	return JS_ValueToString(cx, v);

    pc = fp->pc;
    format = js_CodeSpec[*pc].format;
    mode = (format & JOF_MODEMASK);

    /* Don't look on the stack for element ops, the index will throw us. */
    if (mode != JOF_ELEM) {
	limit = (jsval *)cx->stackPool.current->avail;
	depth = (intN)script->depth;
	if (fp->sp < limit && fp->sp[0] == v)
	    pc = (jsbytecode *)fp->sp[0 - depth];
	if (!pc && fp->sp + 1 < limit && fp->sp[1] == v)
	    pc = (jsbytecode *)fp->sp[1 - depth];

	if (pc != fp->pc) {
	    if ((pruword)(pc - script->code) >= (pruword)script->length) {
		pc = fp->pc;
	    } else {
		format = js_CodeSpec[*pc].format;
		mode = (format & JOF_MODEMASK);
	    }
	}
    }

    if (mode != JOF_NAME &&
	(!(sn = js_GetSrcNote(script, pc)) ||
	 SN_TYPE(sn) != SRC_PCBASE)) {
	return JS_ValueToString(cx, v);
    }

    if (mode == JOF_NAME) {
	begin = pc;
	end = pc + 3;
    } else {
	begin = pc - js_GetSrcNoteOffset(sn, 0);
	end = fp->pc;
    }
    len = end - begin;

    if (format & (JOF_SET | JOF_DEL | JOF_INCDEC | JOF_IMPORT)) {
	/* These ops require bytecode source extension. */
	tmp = JS_malloc(cx, (len + 3) * sizeof(jsbytecode));
	if (!tmp)
	    return NULL;
	memcpy(tmp, begin, len * sizeof(jsbytecode));
	if (mode == JOF_NAME) {
	    tmp[0] = JSOP_NAME;
	} else if (mode == JOF_PROP) {
	    tmp[len++] = (format & JOF_SET) ? JSOP_GETPROP2 : JSOP_GETPROP;
	    tmp[len++] = pc[1];
	    tmp[len++] = pc[2];
	} else {
	    tmp[len++] = (format & JOF_SET) ? JSOP_GETELEM2 : JSOP_GETELEM;
	}
	begin = tmp;
    } else {
	/* No need to extend script bytecode. */
	tmp = NULL;
    }

    JS_LOCK(cx);
    jp = js_NewPrinter(cx, "js_ValueToSource", 0);
    if (jp && js_DecompileCode(jp, script, begin, len))
	name = js_GetPrinterOutput(jp);
    else
	name = NULL;
    js_DestroyPrinter(jp);
    JS_UNLOCK(cx);
    if (tmp)
	JS_free(cx, tmp);
    return name;
}
