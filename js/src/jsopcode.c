/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * JS bytecode descriptors, disassemblers, and decompilers.
 */
#include "jsstddef.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsdtoa.h"
#include "jsprf.h"
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

char js_const_str[]         = "const";
char js_var_str[]           = "var";
char js_function_str[]      = "function";
char js_in_str[]            = "in";
char js_instanceof_str[]    = "instanceof";
char js_new_str[]           = "new";
char js_delete_str[]        = "delete";
char js_typeof_str[]        = "typeof";
char js_void_str[]          = "void";
char js_null_str[]          = "null";
char js_this_str[]          = "this";
char js_false_str[]         = "false";
char js_true_str[]          = "true";

char *js_incop_str[]        = {"++", "--"};

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
#include "jsopcode.tbl"
#undef OPDEF
};

uintN js_NumCodeSpecs = sizeof (js_CodeSpec) / sizeof js_CodeSpec[0];

/************************************************************************/

#ifdef DEBUG

JS_FRIEND_API(void)
js_Disassemble(JSContext *cx, JSScript *script, JSBool lines, FILE *fp)
{
    jsbytecode *pc, *end;
    uintN len;

    pc = script->code;
    end = pc + script->length;
    while (pc < end) {
	if (pc == script->main)
	    fputs("main:\n", fp);
	len = js_Disassemble1(cx, script, pc,
                              PTRDIFF(pc, script->code, jsbytecode),
                              lines, fp);
	if (!len)
	    return;
	pc += len;
    }
}

JS_FRIEND_API(uintN)
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc, uintN loc,
		JSBool lines, FILE *fp)
{
    JSOp op;
    JSCodeSpec *cs;
    intN len, off;
    JSAtom *atom;
    JSString *str;
    char *cstr;

    op = (JSOp)*pc;
    if (op >= JSOP_LIMIT) {
	char numBuf1[12], numBuf2[12];
	JS_snprintf(numBuf1, sizeof numBuf1, "%d", op);
	JS_snprintf(numBuf2, sizeof numBuf2, "%d", JSOP_LIMIT);
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
			     JSMSG_BYTECODE_TOO_BIG, numBuf1, numBuf2);
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
	str = js_ValueToSource(cx, ATOM_KEY(atom));
	if (!str)
	    return 0;
	cstr = js_DeflateString(cx, str->chars, str->length);
	if (!cstr)
	    return 0;
	fprintf(fp, " %s", cstr);
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
	pc2 += JUMP_OFFSET_LEN;
	low = GET_JUMP_OFFSET(pc2);
	pc2 += JUMP_OFFSET_LEN;
	high = GET_JUMP_OFFSET(pc2);
	pc2 += JUMP_OFFSET_LEN;
	fprintf(fp, " defaultOffset %d low %d high %d", off, low, high);
        for (i = low; i <= high; i++) {
            off = GET_JUMP_OFFSET(pc2);
            fprintf(fp, "\n\t%d: %d", i, off);
            pc2 += JUMP_OFFSET_LEN;
        }
	len = 1 + pc2 - pc;
	break;
      }

      case JOF_LOOKUPSWITCH:
      {
	jsbytecode *pc2 = pc;
	jsint npairs;

	off = GET_JUMP_OFFSET(pc2);
	pc2 += JUMP_OFFSET_LEN;
	npairs = (jsint) GET_ATOM_INDEX(pc2);
	pc2 += ATOM_INDEX_LEN;
	fprintf(fp, " offset %d npairs %u", off, (uintN) npairs);
	while (npairs) {
	    atom = GET_ATOM(cx, script, pc2);
	    pc2 += ATOM_INDEX_LEN;
	    off = GET_JUMP_OFFSET(pc2);
	    pc2 += JUMP_OFFSET_LEN;

	    str = js_ValueToSource(cx, ATOM_KEY(atom));
	    if (!str)
		return 0;
	    cstr = js_DeflateString(cx, str->chars, str->length);
	    if (!cstr)
		return 0;
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

#if JS_HAS_LEXICAL_CLOSURE
      case JOF_DEFLOCALVAR:
        fprintf(fp, " %u", GET_VARNO(pc));
        pc += VARNO_LEN;
        atom = GET_ATOM(cx, script, pc);
        str = js_ValueToSource(cx, ATOM_KEY(atom));
        if (!str)
            return 0;
        cstr = js_DeflateString(cx, str->chars, str->length);
        if (!cstr)
            return 0;
        fprintf(fp, " %s", cstr);
        JS_free(cx, cstr);
        break;
#endif

      default: {
	char numBuf[12];
	JS_snprintf(numBuf, sizeof numBuf, "%lx", (unsigned long) cs->format);
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
			     JSMSG_UNKNOWN_FORMAT, numBuf);
	return 0;
      }
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
    JSArenaPool     *pool;          /* string allocation pool */
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
	JS_ARENA_ALLOCATE_CAST(sp->base, char *, sp->pool, nb);
    } else {
	JS_ARENA_GROW_CAST(sp->base, char *, sp->pool, sp->size, nb);
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
    bp = JS_vsmprintf(format, ap);	/* XXX vsaprintf */
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
    '\'', '\'',
    '\\', '\\',
    0
};

static char *
QuoteString(Sprinter *sp, JSString *str, jschar quote)
{
    ptrdiff_t off, len, nb;
    const jschar *s, *t, *u, *z;
    char *bp;
    jschar c;
    JSBool ok;

    /* Sample off first for later return value pointer computation. */
    off = sp->offset;
    if (Sprint(sp, "%c", (char)quote) < 0)
	return NULL;

    /* Loop control variables: z points at end of string sentinel. */
    s = str->chars;
    z = s + str->length;
    for (t = s; t < z; s = ++t) {
	/* Move t forward from s past un-quote-worthy characters. */
	c = *t;
	while (JS_ISPRINT(c) && c != quote && c != '\\' && !(c >> 8)) {
	    c = *++t;
	    if (t == z)
		break;
	}
	len = PTRDIFF(t, s, jschar);

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

	if (t == z)
	    break;

	/* Use js_EscapeMap, \u, or \x only if necessary. */
	if ((u = js_strchr(js_EscapeMap, c)) != NULL)
	    ok = Sprint(sp, "\\%c", (char)u[1]) >= 0;
	else
	    ok = Sprint(sp, (c >> 8) ? "\\u%04X" : "\\x%02X", c) >= 0;
	if (!ok)
	    return NULL;
    }

    /* Sprint the closing quote and return the quoted string. */
    if (Sprint(sp, "%c", (char)quote) < 0)
	return NULL;
    return OFF2STR(sp, off);
}

JSString *
js_QuoteString(JSContext *cx, JSString *str, jschar quote)
{
    void *mark;
    Sprinter sprinter;
    char *bytes;
    JSString *escstr;

    mark = JS_ARENA_MARK(&cx->tempPool);
    INIT_SPRINTER(cx, &sprinter, &cx->tempPool, 0);
    bytes = QuoteString(&sprinter, str, quote);
    escstr = bytes ? JS_NewStringCopyZ(cx, bytes) : NULL;
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    return escstr;
}

/************************************************************************/

struct JSPrinter {
    Sprinter        sprinter;       /* base class state */
    JSArenaPool     pool;           /* string allocation pool */
    uintN           indent;         /* indentation in spaces */
    JSBool          pretty;         /* pretty-print: indent, use newlines */
    JSScript        *script;        /* script being printed */
    JSScope         *scope;         /* script function scope */
};

JSPrinter *
js_NewPrinter(JSContext *cx, const char *name, uintN indent, JSBool pretty)
{
    JSPrinter *jp;
    JSStackFrame *fp;
    JSObjectMap *map;

    jp = (JSPrinter *) JS_malloc(cx, sizeof(JSPrinter));
    if (!jp)
	return NULL;
    INIT_SPRINTER(cx, &jp->sprinter, &jp->pool, 0);
    JS_InitArenaPool(&jp->pool, name, 256, 1);
    jp->indent = indent;
    jp->pretty = pretty;
    jp->script = NULL;
    jp->scope = NULL;
    fp = cx->fp;
    if (fp && fp->fun && fp->fun->object) {
	map = fp->fun->object->map;
	if (MAP_IS_NATIVE(map))
	    jp->scope = (JSScope *)map;
    }
    return jp;
}

void
js_DestroyPrinter(JSPrinter *jp)
{
    JS_FinishArenaPool(&jp->pool);
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
    JS_FreeArenaPool(&jp->pool);
    INIT_SPRINTER(cx, &jp->sprinter, &jp->pool, 0);
    return str;
}

int
js_printf(JSPrinter *jp, const char *format, ...)
{
    va_list ap;
    char *bp, *fp;
    int cc;

    if (*format == '\0')
        return 0;

    va_start(ap, format);

    /* If pretty-printing, expand magic tab into a run of jp->indent spaces. */
    if (*format == '\t') {
        if (jp->pretty && Sprint(&jp->sprinter, "%*s", jp->indent, "") < 0)
	    return -1;
	format++;
    }

    /* Suppress newlines (must be once per format, at the end) if not pretty. */
    fp = NULL;
    if (!jp->pretty && format[cc = strlen(format)-1] == '\n') {
        fp = JS_strdup(jp->sprinter.context, format);
        if (!fp)
            return -1;
        fp[cc] = '\0';
        format = fp;
    }

    /* Allocate temp space, convert format, and put. */
    bp = JS_vsmprintf(format, ap);	/* XXX vsaprintf */
    if (fp) {
        JS_free(jp->sprinter.context, fp);
        format = NULL;
    }
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
js_puts(JSPrinter *jp, const char *s)
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
 * These pseudo-ops help js_DecompileValueGenerator decompile JSOP_SETNAME,
 * JSOP_SETPROP, and JSOP_SETELEM, respectively.  See the first assertion in
 * PushOff.
 */
#define JSOP_GETPROP2   254
#define JSOP_GETELEM2   255

static JSBool
PushOff(SprintStack *ss, ptrdiff_t off, JSOp op)
{
    uintN top;

#if JSOP_LIMIT > JSOP_GETPROP2
#error JSOP_LIMIT must be <= JSOP_GETPROP2
#endif
    if (!SprintAlloc(&ss->sprinter, PAREN_SLOP))
	return JS_FALSE;

    /* ss->top points to the next free slot; be paranoid about overflow. */
    top = ss->top;
    JS_ASSERT(top < ss->printer->script->depth);
    if (top >= ss->printer->script->depth) {
	JS_ReportOutOfMemory(ss->sprinter.context);
	return JS_FALSE;
    }

    /* The opcodes stack must contain real bytecodes that index js_CodeSpec. */
    ss->offsets[top] = off;
    ss->opcodes[top] = (op == JSOP_GETPROP2) ? JSOP_GETPROP
		     : (op == JSOP_GETELEM2) ? JSOP_GETELEM
		     : (jsbytecode) op;
    ss->top = ++top;
    ss->sprinter.offset += PAREN_SLOP;
    return JS_TRUE;
}

static ptrdiff_t
PopOff(SprintStack *ss, JSOp op)
{
    uintN top;
    JSCodeSpec *cs, *topcs;
    ptrdiff_t off;

    /* ss->top points to the next free slot; be paranoid about underflow. */
    top = ss->top;
    JS_ASSERT(top != 0);
    if (top == 0)
	return 0;

    ss->top = --top;
    topcs = &js_CodeSpec[ss->opcodes[top]];
    cs = &js_CodeSpec[op];
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
    const TableEntry *te1 = (const TableEntry *) v1,
                     *te2 = (const TableEntry *) v2;

    return te1->offset - te2->offset;
}

static JSBool
Decompile(SprintStack *ss, jsbytecode *pc, intN nb);

static JSBool
DecompileSwitch(SprintStack *ss, TableEntry *table, uintN tableLength,
		jsbytecode *pc, ptrdiff_t switchLength,
		ptrdiff_t defaultOffset, JSBool isCondSwitch)
{
    JSContext *cx;
    JSPrinter *jp;
    char *lval, *rval;
    uintN i;
    ptrdiff_t diff, off, off2, caseExprOff;
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

	caseExprOff = isCondSwitch
                      ? (ptrdiff_t) js_CodeSpec[JSOP_CONDSWITCH].length
                      : 0;

	for (i = 0; i < tableLength; i++) {
	    off = table[i].offset;
	    if (i + 1 < tableLength)
		off2 = table[i + 1].offset;
	    else
		off2 = switchLength;

	    key = table[i].key;
	    if (isCondSwitch) {
		ptrdiff_t nextCaseExprOff;

		/*
		 * key encodes the JSOP_CASE bytecode's offset from switchtop.
		 * The next case expression follows immediately, unless we are
		 * at the last case.
		 */
		nextCaseExprOff = (ptrdiff_t)
		    (JSVAL_TO_INT(key) + js_CodeSpec[JSOP_CASE].length);
		jp->indent += 2;
		if (!Decompile(ss, pc + caseExprOff,
			       nextCaseExprOff - caseExprOff)) {
		    return JS_FALSE;
		}
		caseExprOff = nextCaseExprOff;
	   } else {
		/*
		 * key comes from an atom, not the decompiler, so we need to
		 * quote it if it's a string literal.
		 */
		str = js_ValueToString(cx, key);
		if (!str)
		    return JS_FALSE;
		jp->indent += 2;
		if (JSVAL_IS_STRING(key)) {
		    rval = QuoteString(&ss->sprinter, str, (jschar)'"');
		    if (!rval)
			return JS_FALSE;
		} else {
		    rval = JS_GetStringBytes(str);
		}
		js_printf(jp, "\tcase %s:\n", rval);
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
GetSlotAtom(JSPrinter *jp, JSPropertyOp getter, uintN slot)
{
    JSScope *scope;
    JSScopeProperty *sprop;
    JSObject *obj, *proto;

    scope = jp->scope;
    while (scope) {
        for (sprop = scope->props; sprop; sprop = sprop->next) {
            if (SPROP_GETTER_SCOPE(sprop, scope) != getter)
                continue;
            if ((uintN)JSVAL_TO_INT(sprop->id) == slot)
                return sym_atom(sprop->symbols);
        }
        obj = scope->object;
        if (!obj)
            break;
        proto = OBJ_GET_PROTO(jp->sprinter.context, obj);
        if (!proto)
            break;
        scope = OBJ_SCOPE(proto);
    }
    return NULL;
}

static const char *
VarPrefix(jssrcnote *sn)
{
    char *kw;
    static char buf[8];

    kw = NULL;
    if (sn) {
        if (SN_TYPE(sn) == SRC_VAR)
            kw = js_var_str;
        else if (SN_TYPE(sn) == SRC_CONST)
            kw = js_const_str;
    }
    if (!kw)
        return "";
    JS_snprintf(buf, sizeof buf, "%s ", kw);
    return buf;
}

static JSBool
IsASCIIIdentifier(JSString *str)
{
    size_t n;
    jschar *s, c;

    n = str->length;
    s = str->chars;
    c = *s;
    if (n == 0 || !JS_ISIDENT_START(c) || !JS_ISPRINT(c))
        return JS_FALSE;
    for (n--; n != 0; n--) {
        c = *++s;
        if (!JS_ISIDENT(c) || !JS_ISPRINT(c))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
Decompile(SprintStack *ss, jsbytecode *pc, intN nb)
{
    JSContext *cx;
    JSPrinter *jp, *jp2;
    jsbytecode *endpc, *done, *forelem_done;
    ptrdiff_t len, todo, oplen, cond, next, tail;
    JSOp op, lastop, saveop;
    JSCodeSpec *cs, *topcs;
    jssrcnote *sn;
    const char *lval, *rval = NULL, *xval;
    jsint i, argc;
    char **argv;
    JSAtom *atom;
    JSObject *obj;
    JSFunction *fun;
    JSString *str;
    JSBool ok;
    jsval key;

/*
 * Local macros
*/
#define DECOMPILE_CODE(pc,nb)	if (!Decompile(ss, pc, nb)) return JS_FALSE
#define POP_STR()		OFF2STR(&ss->sprinter, PopOff(ss, op))
#define LOCAL_ASSERT(expr)	JS_ASSERT(expr); if (!(expr)) return JS_FALSE

    cx = ss->sprinter.context;
    jp = ss->printer;
    endpc = pc + nb;
    todo = -2;			/* NB: different from Sprint() error return. */
    op = JSOP_NOP;
    while (pc < endpc) {
	lastop = op;
	op = saveop = (JSOp) *pc;
	if (op >= JSOP_LIMIT) {
	    switch (op) {
	      case JSOP_GETPROP2:
		saveop = JSOP_GETPROP;
		break;
	      case JSOP_GETELEM2:
		saveop = JSOP_GETELEM;
		break;
	      default:;
	    }
	}
	cs = &js_CodeSpec[saveop];
	len = oplen = cs->length;

	if (cs->token) {
	    switch (cs->nuses) {
	      case 2:
		rval = POP_STR();
		lval = POP_STR();
		if ((sn = js_GetSrcNote(jp->script, pc)) != NULL &&
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
#if JS_HAS_GETTER_SETTER
                if (op == JSOP_GETTER || op == JSOP_SETTER) {
                    todo = -2;
                    break;
                }
#endif
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
                 * Check for a do-while loop, a for-loop with an empty
                 * initializer part, a labeled statement, a function
                 * definition, or try/finally.
		 */
		sn = js_GetSrcNote(jp->script, pc);
		todo = -2;
		switch (sn ? SN_TYPE(sn) : SRC_NULL) {
#if JS_HAS_DO_WHILE_LOOP
		  case SRC_WHILE:
		    js_printf(jp, "\tdo {\n");	/* balance} */
		    jp->indent += 4;
		    break;
#endif /* JS_HAS_DO_WHILE_LOOP */

		  case SRC_FOR:
		    rval = "";

		  do_forloop:
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
		    oplen = (cond) ? js_CodeSpec[pc[cond]].length : 0;
		    DECOMPILE_CODE(pc + cond + oplen, next - cond - oplen);
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");

		    /* Set len so pc skips over the entire loop. */
		    len = tail + js_CodeSpec[pc[tail]].length;
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

		  case SRC_CATCH:
		    jp->indent -= 4;
		    sn = js_GetSrcNote(jp->script, pc);
		    pc += oplen;
		    js_printf(jp, "\t} catch ("); /* balance) */
		    pc += 6;	/* name Object, pushobj, exception */
		    js_printf(jp, "%s",
			      ATOM_BYTES(GET_ATOM(cx, jp->script, pc)));
		    len = js_GetSrcNoteOffset(sn, 0);
		    pc += 4;	/* initcatchvar, enterwith */
		    if (len) {
			js_printf(jp, " if ");
			DECOMPILE_CODE(pc, len - 3); /* don't decompile ifeq */
			js_printf(jp, "%s", POP_STR());
			pc += len;
		    }
		    js_printf(jp, ") {\n"); /* balance} */
		    jp->indent += 4;
		    len = 0;
		    break;

		  case SRC_FUNCDEF:
		    atom = js_GetAtom(cx, &jp->script->atomMap,
				      (jsatomid) js_GetSrcNoteOffset(sn, 0));
		    JS_ASSERT(ATOM_IS_OBJECT(atom));
                  do_function:
		    obj = ATOM_TO_OBJECT(atom);
		    fun = (JSFunction *) JS_GetPrivate(cx, obj);
		    jp2 = js_NewPrinter(cx, JS_GetFunctionName(fun),
					jp->indent, jp->pretty);
		    if (!jp2)
			return JS_FALSE;
		    jp2->scope = jp->scope;
		    if (js_DecompileFunction(jp2, fun)) {
			str = js_GetPrinterOutput(jp2);
			if (str)
			    js_printf(jp, "%s\n", JS_GetStringBytes(str));
		    }
		    js_DestroyPrinter(jp2);
		    break;

		  default:;
		}
		break;

              case JSOP_GROUP:
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

	      case JSOP_PUSH:
	      case JSOP_PUSHOBJ:
	      case JSOP_BINDNAME:
		todo = Sprint(&ss->sprinter, "");
		break;

#if JS_HAS_EXCEPTIONS
              case JSOP_TRY:
                js_printf(jp, "\ttry {\n");
                jp->indent += 4;
                break;

              case JSOP_FINALLY:
                jp->indent -= 4;
                js_printf(jp, "\t} finally {\n");
                jp->indent += 4;
                break;

	      case JSOP_GOSUB:
	      case JSOP_RETSUB:
	      case JSOP_SETSP:
		todo = -2;
		break;

	      case JSOP_EXCEPTION:
		sn = js_GetSrcNote(jp->script, pc);
		if (sn && SN_TYPE(sn) == SRC_HIDDEN)
		    todo = -2;
		break;
#endif /* JS_HAS_EXCEPTIONS */

	      case JSOP_POP:
	      case JSOP_POPV:
		sn = js_GetSrcNote(jp->script, pc);
		switch (sn ? SN_TYPE(sn) : SRC_NULL) {
		  case SRC_FOR:
		    rval = POP_STR();
		    todo = -2;
		    goto do_forloop;

		  case SRC_PCDELTA:
		    /* Pop and save to avoid blowing stack depth budget. */
		    lval = JS_strdup(cx, POP_STR());
		    if (!lval)
			return JS_FALSE;

		    /*
                     * The offset tells distance to the end of the right-hand
                     * operand of the comma operator.
                     */
		    done = pc + len;
		    pc += js_GetSrcNoteOffset(sn, 0);
		    len = 0;

		    if (!Decompile(ss, done, pc - done)) {
			JS_free(cx, (char *)lval);
			return JS_FALSE;
		    }

		    /* Pop Decompile result and print comma expression. */
		    rval = POP_STR();
		    todo = Sprint(&ss->sprinter, "%s, %s", lval, rval);
		    JS_free(cx, (char *)lval);
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

	      case JSOP_POP2:
		(void) PopOff(ss, op);
		(void) PopOff(ss, op);
		todo = -2;
		break;

	      case JSOP_ENTERWITH:
		sn = js_GetSrcNote(jp->script, pc);
		todo = -2;
		if (sn && SN_TYPE(sn) == SRC_HIDDEN)
		    break;
		rval = POP_STR();
		js_printf(jp, "\twith (%s) {\n", rval);
		jp->indent += 4;
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

#if JS_HAS_EXCEPTIONS
	      case JSOP_THROW:
		sn = js_GetSrcNote(jp->script, pc);
		todo = -2;
		if (sn && SN_TYPE(sn) == SRC_HIDDEN)
		    break;
		rval = POP_STR();
		js_printf(jp, "\t%s %s;\n", cs->name, rval);
		break;
#endif /* JS_HAS_EXCEPTIONS */

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
		  case SRC_HIDDEN:
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
			DECOMPILE_CODE(pc + oplen, len - oplen);
		    } else {
			DECOMPILE_CODE(pc + oplen,
			    len - (oplen + js_CodeSpec[JSOP_GOTO].length));
			jp->indent -= 4;
			pc += len - oplen;
			LOCAL_ASSERT(*pc == JSOP_GOTO);
			oplen = js_CodeSpec[JSOP_GOTO].length;
			len = GET_JUMP_OFFSET(pc);
			js_printf(jp, "\t} else {\n");
			jp->indent += 4;
			DECOMPILE_CODE(pc + oplen, len - oplen);
		    }
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");
		    todo = -2;
		    break;

		  case SRC_WHILE:
		    rval = POP_STR();
		    js_printf(jp, "\twhile (%s) {\n", rval);
		    jp->indent += 4;
		    DECOMPILE_CODE(pc + oplen,
			len - (oplen + js_CodeSpec[JSOP_GOTO].length));
		    jp->indent -= 4;
		    js_printf(jp, "\t}\n");
		    todo = -2;
		    break;

		  case SRC_COND:
		    xval = JS_strdup(cx, POP_STR());
		    if (!xval)
			return JS_FALSE;
		    DECOMPILE_CODE(pc + oplen,
			len - (oplen + js_CodeSpec[JSOP_GOTO].length));
		    lval = JS_strdup(cx, POP_STR());
		    if (!lval) {
			JS_free(cx, (void *)xval);
			return JS_FALSE;
		    }
		    pc += len - oplen;
		    LOCAL_ASSERT(*pc == JSOP_GOTO);
		    oplen = js_CodeSpec[JSOP_GOTO].length;
		    len = GET_JUMP_OFFSET(pc);
		    DECOMPILE_CODE(pc + oplen, len - oplen);
		    rval = POP_STR();
		    todo = Sprint(&ss->sprinter, "%s ? %s : %s",
				  xval, lval, rval);
		    JS_free(cx, (void *)xval);
		    JS_free(cx, (void *)lval);
		    break;

		  default:
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
		break;

	      case JSOP_OR:
		/* Top of stack is the first clause in a disjunction (||). */
		lval = JS_strdup(cx, POP_STR());
		if (!lval)
		    return JS_FALSE;
		done = pc + GET_JUMP_OFFSET(pc);
		pc += len;
		len = PTRDIFF(done, pc, jsbytecode);
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
		len = PTRDIFF(done, pc, jsbytecode);
		DECOMPILE_CODE(pc, len);
		rval = POP_STR();
		todo = Sprint(&ss->sprinter, "%s && %s", lval, rval);
		JS_free(cx, (char *)lval);
		break;

	      case JSOP_FORARG:
		atom = GetSlotAtom(jp, js_GetArgument, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		lval = ATOM_BYTES(atom);
		goto do_fornameinloop;

	      case JSOP_FORVAR:
		atom = GetSlotAtom(jp, js_GetLocalVariable, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		lval = ATOM_BYTES(atom);
		goto do_fornameinloop;

	      case JSOP_FORNAME:
                atom = GET_ATOM(cx, jp->script, pc);
		lval = ATOM_BYTES(atom);

              do_fornameinloop:
		sn = js_GetSrcNote(jp->script, pc);
		xval = NULL;
		atom = NULL;
		goto do_forinloop;

	      case JSOP_FORPROP:
		xval = NULL;
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
		sn = NULL;

	      do_forinloop:
		pc += oplen;
		LOCAL_ASSERT(*pc == JSOP_IFEQ);
		oplen = js_CodeSpec[JSOP_IFEQ].length;
		len = GET_JUMP_OFFSET(pc);

              do_forinbody:
		js_printf(jp, "\tfor (%s%s", VarPrefix(sn), lval);
		if (atom)
		    js_printf(jp, ".%s", ATOM_BYTES(atom));
		else if (xval)
		    js_printf(jp, "[%s]", xval);
                rval = OFF2STR(&ss->sprinter, ss->offsets[ss->top-1]);
		js_printf(jp, " in %s) {\n", rval);
		jp->indent += 4;
		DECOMPILE_CODE(pc + oplen,
			       len - (oplen + js_CodeSpec[JSOP_GOTO].length));
		jp->indent -= 4;
		js_printf(jp, "\t}\n");
		todo = -2;
		break;

              case JSOP_FORELEM:
		pc++;
		LOCAL_ASSERT(*pc == JSOP_IFEQ);
		len = js_CodeSpec[JSOP_IFEQ].length;

                /*
                 * This gets a little wacky.  Only the length of the for loop
                 * body PLUS the element-indexing expression is known here, so
                 * we pass the after-loop pc to the JSOP_ENUMELEM case, which
                 * is immediately below, to decompile that helper bytecode via
                 * the 'forelem_done' local.
                 */
		forelem_done = pc + GET_JUMP_OFFSET(pc);
                break;

	      case JSOP_ENUMELEM:
		/*
		 * The stack has the object under the (top) index expression.
		 * The "rval" property id is underneath those two on the stack.
		 * The for loop body length can now be adjusted to account for
		 * the length of the indexing expression.
		 */
		atom = NULL;
		xval = POP_STR();
		lval = POP_STR();
		rval = OFF2STR(&ss->sprinter, ss->offsets[ss->top-1]);
		len = forelem_done - pc;
		goto do_forinbody;

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
		atom = GetSlotAtom(jp, js_GetArgument, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_setname;

	      case JSOP_SETVAR:
		atom = GetSlotAtom(jp, js_GetLocalVariable, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		goto do_setname;

	      case JSOP_SETCONST:
	      case JSOP_SETNAME:
		atom = GET_ATOM(cx, jp->script, pc);
	      do_setname:
		lval = ATOM_BYTES(atom);
		rval = POP_STR();
		if (op == JSOP_SETNAME)
		    (void) PopOff(ss, op);
              do_setlval:
		if ((sn = js_GetSrcNote(jp->script, pc - 1)) != NULL &&
		    SN_TYPE(sn) == SRC_ASSIGNOP) {
		    todo = Sprint(&ss->sprinter, "%s %s= %s",
				  lval, js_CodeSpec[lastop].token, rval);
		} else {
		    sn = js_GetSrcNote(jp->script, pc);
		    todo = Sprint(&ss->sprinter, "%s%s = %s",
				  VarPrefix(sn), lval, rval);
		}
		break;

	      case JSOP_NEW:
	      case JSOP_CALL:
	      case JSOP_EVAL:
#if JS_HAS_LVALUE_RETURN
	      case JSOP_SETCALL:
#endif
		saveop = op;
		op = JSOP_NOP;           /* turn off parens */
		argc = GET_ARGC(pc);
		argv = (char **)
                    JS_malloc(cx, (size_t)(argc + 1) * sizeof *argv);
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
#if JS_HAS_LVALUE_RETURN
                if (op == JSOP_SETCALL) {
                    if (!PushOff(ss, todo, op))
                        return JS_FALSE;
                    todo = Sprint(&ss->sprinter, "");
                }
#endif
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
		atom = GetSlotAtom(jp, js_GetArgument, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_incatom;

	      case JSOP_INCVAR:
	      case JSOP_DECVAR:
		atom = GetSlotAtom(jp, js_GetLocalVariable, GET_VARNO(pc));
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
		atom = GetSlotAtom(jp, js_GetArgument, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_atominc;

	      case JSOP_VARINC:
	      case JSOP_VARDEC:
		atom = GetSlotAtom(jp, js_GetLocalVariable, GET_VARNO(pc));
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
                str = ATOM_TO_STRING(atom);
                if (IsASCIIIdentifier(str)) {
                    todo = Sprint(&ss->sprinter, "%s.%s",
                                  lval, JS_GetStringBytes(str));
                } else {
                    lval = JS_strdup(cx, lval);
                    if (!lval)
                        return JS_FALSE;
                    xval = QuoteString(&ss->sprinter, str, (jschar)'\'');
                    todo = Sprint(&ss->sprinter, "%s[%s]", lval, xval);
                    JS_free(cx, (char *)lval);
                }
		break;

	      case JSOP_SETPROP:
		rval = POP_STR();
		atom = GET_ATOM(cx, jp->script, pc);
		lval = POP_STR();
                sn = js_GetSrcNote(jp->script, pc - 1);
                str = ATOM_TO_STRING(atom);
                if (IsASCIIIdentifier(str)) {
                    xval = JS_GetStringBytes(str);
                    if (sn && SN_TYPE(sn) == SRC_ASSIGNOP) {
                        todo = Sprint(&ss->sprinter, "%s.%s %s= %s",
                                      lval, xval,
                                      js_CodeSpec[lastop].token, rval);
                    } else {
                        todo = Sprint(&ss->sprinter, "%s.%s = %s",
                                      lval, xval, rval);
                    }
                } else {
                    rval = JS_strdup(cx, rval);
                    lval = JS_strdup(cx, lval);
                    if (!rval || !lval) {
                        JS_free(cx, (char *)rval);
                        return JS_FALSE;
                    }
                    xval = QuoteString(&ss->sprinter, str, (jschar)'\'');
                    if (sn && SN_TYPE(sn) == SRC_ASSIGNOP) {
                        todo = Sprint(&ss->sprinter, "%s[%s] %s= %s",
                                      lval, xval,
                                      js_CodeSpec[lastop].token, rval);
                    } else {
                        todo = Sprint(&ss->sprinter, "%s[%s] = %s",
                                      lval, xval, rval);
                    }
                    JS_free(cx, (char *)rval);
                    JS_free(cx, (char *)lval);
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
                if (*xval == '\0')
                    todo = Sprint(&ss->sprinter, "%s", lval);
                else
                    todo = Sprint(&ss->sprinter, "%s[%s]", lval, xval);
		break;

	      case JSOP_SETELEM:
		op = JSOP_NOP;           /* turn off parens */
		rval = POP_STR();
		xval = POP_STR();
		op = JSOP_SETELEM;
		lval = POP_STR();
                if (*xval == '\0')
                    goto do_setlval;
                if ((sn = js_GetSrcNote(jp->script, pc - 1)) != NULL &&
                    SN_TYPE(sn) == SRC_ASSIGNOP) {
                    todo = Sprint(&ss->sprinter, "%s[%s] %s= %s",
                                  lval, xval,
                                  js_CodeSpec[lastop].token, rval);
                } else {
                    todo = Sprint(&ss->sprinter, "%s[%s] = %s",
                                  lval, xval, rval);
                }
		break;

              case JSOP_ARGSUB:
                i = (jsint) GET_ATOM_INDEX(pc);
                todo = Sprint(&ss->sprinter, "%s[%d]",
                              js_arguments_str, (int) i);
                break;

              case JSOP_ARGCNT:
                todo = Sprint(&ss->sprinter, "%s.%s",
                              js_arguments_str, js_length_str);
                break;

	      case JSOP_GETARG:
		atom = GetSlotAtom(jp, js_GetArgument, GET_ARGNO(pc));
		LOCAL_ASSERT(atom);
		goto do_name;

	      case JSOP_GETVAR:
		atom = GetSlotAtom(jp, js_GetLocalVariable, GET_VARNO(pc));
		LOCAL_ASSERT(atom);
		goto do_name;

	      case JSOP_NAME:
		atom = GET_ATOM(cx, jp->script, pc);
	      do_name:
		sn = js_GetSrcNote(jp->script, pc);
		todo = Sprint(&ss->sprinter, "%s%s",
			      VarPrefix(sn), ATOM_BYTES(atom));
		break;

	      case JSOP_UINT16:
		i = (jsint) GET_ATOM_INDEX(pc);
		todo = Sprint(&ss->sprinter, "%u", (unsigned) i);
		break;

              case JSOP_NUMBER:
                atom = GET_ATOM(cx, jp->script, pc);
                key = ATOM_KEY(atom);
                if (JSVAL_IS_INT(key)) {
                    long ival = (long)JSVAL_TO_INT(key);
                    todo = Sprint(&ss->sprinter, "%ld", ival);
                } else {
                    char buf[DTOSTR_STANDARD_BUFFER_SIZE];
                    char *numStr = JS_dtostr(buf, sizeof buf, DTOSTR_STANDARD,
                                             0, *JSVAL_TO_DOUBLE(key));
                    if (!numStr) {
                        JS_ReportOutOfMemory(cx);
                        return JS_FALSE;
                    }
                    todo = Sprint(&ss->sprinter, numStr);
                }
                break;

              case JSOP_STRING:
                atom = GET_ATOM(cx, jp->script, pc);
                rval = QuoteString(&ss->sprinter, ATOM_TO_STRING(atom),
                                   (jschar)'"');
                if (!rval)
                    return JS_FALSE;
                todo = Sprint(&ss->sprinter, "%s", rval);
                break;

              case JSOP_OBJECT:
              case JSOP_ANONFUNOBJ:
              case JSOP_NAMEDFUNOBJ:
                atom = GET_ATOM(cx, jp->script, pc);
                str = js_ValueToSource(cx, ATOM_KEY(atom));
                if (!str)
                    return JS_FALSE;
                todo = SprintPut(&ss->sprinter, JS_GetStringBytes(str),
                                 str->length);
                break;

#if JS_HAS_SWITCH_STATEMENT
	      case JSOP_TABLESWITCH:
	      {
		jsbytecode *pc2, *end;
		ptrdiff_t off, off2;
		jsint j, n, low, high;
		TableEntry *table;

		sn = js_GetSrcNote(jp->script, pc);
		JS_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
		len = js_GetSrcNoteOffset(sn, 0);
		pc2 = pc;
		off = GET_JUMP_OFFSET(pc2);
		end = pc + off;
		pc2 += JUMP_OFFSET_LEN;
		low = GET_JUMP_OFFSET(pc2);
		pc2 += JUMP_OFFSET_LEN;
		high = GET_JUMP_OFFSET(pc2);

		n = high - low + 1;
                if (n == 0) {
                    table = NULL;
		    j = 0;
                } else {
                    table = (TableEntry *)
                            JS_malloc(cx, (size_t)n * sizeof *table);
                    if (!table)
                        return JS_FALSE;
		    for (i = j = 0; i < n; i++) {
			pc2 += JUMP_OFFSET_LEN;
			off2 = GET_JUMP_OFFSET(pc2);
			if (off2) {
			    table[j].key = INT_TO_JSVAL(low + i);
			    table[j++].offset = off2;
			}
		    }
                    js_qsort(table, (size_t)j, sizeof *table, CompareOffsets,
                             NULL);
		}

		ok = DecompileSwitch(ss, table, (uintN)j, pc, len, off,
				     JS_FALSE);
		JS_free(cx, table);
		if (!ok)
		    return ok;
		todo = -2;
		break;
	      }

	      case JSOP_LOOKUPSWITCH:
	      {
		jsbytecode *pc2;
		ptrdiff_t off, off2;
		jsint npairs;
		TableEntry *table;

		sn = js_GetSrcNote(jp->script, pc);
		JS_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
		len = js_GetSrcNoteOffset(sn, 0);
		pc2 = pc;
		off = GET_JUMP_OFFSET(pc2);
		pc2 += JUMP_OFFSET_LEN;
		npairs = (jsint) GET_ATOM_INDEX(pc2);
		pc2 += ATOM_INDEX_LEN;

		table = (TableEntry *)
                    JS_malloc(cx, (size_t)npairs * sizeof *table);
		if (!table)
		    return JS_FALSE;
		for (i = 0; i < npairs; i++) {
		    atom = GET_ATOM(cx, jp->script, pc2);
		    pc2 += ATOM_INDEX_LEN;
		    off2 = GET_JUMP_OFFSET(pc2);
		    pc2 += JUMP_OFFSET_LEN;
		    table[i].key = ATOM_KEY(atom);
		    table[i].offset = off2;
		}

		ok = DecompileSwitch(ss, table, (uintN)npairs, pc, len, off,
				     JS_FALSE);
		JS_free(cx, table);
		if (!ok)
		    return ok;
		todo = -2;
		break;
	      }

	      case JSOP_CONDSWITCH:
	      {
		jsbytecode *pc2;
		ptrdiff_t off, off2, caseOff;
		jsint ncases;
		TableEntry *table;

		sn = js_GetSrcNote(jp->script, pc);
		JS_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
		len = js_GetSrcNoteOffset(sn, 0);
		off = js_GetSrcNoteOffset(sn, 1);

		/*
		 * Count the cases using offsets from switch to first case,
		 * and case to case, stored in srcnote immediates.
		 */
		pc2 = pc;
		off2 = off;
		for (ncases = 0; off2 != 0; ncases++) {
		    pc2 += off2;
		    JS_ASSERT(*pc2 == JSOP_CASE || *pc2 == JSOP_DEFAULT);
		    if (*pc2 == JSOP_DEFAULT) {
			/* End of cases, but count default as a case. */
			off2 = 0;
		    } else {
			sn = js_GetSrcNote(jp->script, pc2);
			JS_ASSERT(sn && SN_TYPE(sn) == SRC_PCDELTA);
			off2 = js_GetSrcNoteOffset(sn, 0);
		    }
		}

		/*
		 * Allocate table and rescan the cases using their srcnotes,
		 * stashing each case's delta from switch top in table[i].key,
		 * and the distance to its statements in table[i].offset.
		 */
		table = (TableEntry *)
                    JS_malloc(cx, (size_t)ncases * sizeof *table);
		if (!table)
		    return JS_FALSE;
		pc2 = pc;
		off2 = off;
		for (i = 0; i < ncases; i++) {
		    pc2 += off2;
		    JS_ASSERT(*pc2 == JSOP_CASE || *pc2 == JSOP_DEFAULT);
		    caseOff = pc2 - pc;
		    table[i].key = INT_TO_JSVAL((jsint) caseOff);
		    table[i].offset = caseOff + GET_JUMP_OFFSET(pc2);
		    if (*pc2 == JSOP_CASE) {
			sn = js_GetSrcNote(jp->script, pc2);
			JS_ASSERT(sn && SN_TYPE(sn) == SRC_PCDELTA);
			off2 = js_GetSrcNoteOffset(sn, 0);
		    }
		}

		/*
		 * Find offset of default code by fetching the default offset
		 * from the end of table.  JSOP_CONDSWITCH always has a default
		 * case at the end.
		 */
		off = JSVAL_TO_INT(table[ncases-1].key);
		pc2 = pc + off;
		off += GET_JUMP_OFFSET(pc2);

		ok = DecompileSwitch(ss, table, (uintN)ncases, pc, len, off,
				     JS_TRUE);
		JS_free(cx, table);
		if (!ok)
		    return ok;
		todo = -2;
		break;
	      }

	      case JSOP_CASE:
	      {
		lval = POP_STR();
		if (!lval)
		    return JS_FALSE;
		js_printf(jp, "\tcase %s:\n", lval);
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
		atom = GET_ATOM(cx, jp->script, pc);
		JS_ASSERT(ATOM_IS_OBJECT(atom));
                goto do_function;
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

#if JS_HAS_INITIALIZERS
	      case JSOP_NEWINIT:
		LOCAL_ASSERT(ss->top >= 2);
		(void) PopOff(ss, op);
		lval = POP_STR();
#if JS_HAS_SHARP_VARS
		op = (JSOp)pc[len];
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
			      (sn && SN_TYPE(sn) == SRC_CONTINUE) ? ", " : "",
			      /* [balance */
			      (*rval == '{') ? '}' : ']');
		break;

	      case JSOP_INITPROP:
              case JSOP_INITCATCHVAR:
		rval = POP_STR();
		atom = GET_ATOM(cx, jp->script, pc);
		xval = ATOM_BYTES(atom);
		lval = POP_STR();
	      do_initprop:
#ifdef OLD_GETTER_SETTER
                todo = Sprint(&ss->sprinter, "%s%s%s%s%s:%s",
                              lval,
                              (lval[1] != '\0') ? ", " : "",
                              xval,
                              (lastop == JSOP_GETTER || lastop == JSOP_SETTER)
                              ? " " : "",
                              (lastop == JSOP_GETTER) ? js_getter_str :
                              (lastop == JSOP_SETTER) ? js_setter_str :
                              "",
                              rval);
#else
                if (lastop == JSOP_GETTER || lastop == JSOP_SETTER) {
                    todo = Sprint(&ss->sprinter, "%s%s%s %s%s",
                              lval,
                              (lval[1] != '\0') ? ", " : "",
                              (lastop == JSOP_GETTER)
                              ? js_get_str : js_set_str,
                              xval,
                              rval + strlen(js_function_str) + 1);
                } else {
                    todo = Sprint(&ss->sprinter, "%s%s%s:%s",
                              lval,
                              (lval[1] != '\0') ? ", " : "",
                              xval,
                              rval);
                }
#endif
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
	      case JSOP_DEFSHARP:
		i = (jsint) GET_ATOM_INDEX(pc);
		rval = POP_STR();
		todo = Sprint(&ss->sprinter, "#%u=%s", (unsigned) i, rval);
		break;

	      case JSOP_USESHARP:
		i = (jsint) GET_ATOM_INDEX(pc);
		todo = Sprint(&ss->sprinter, "#%u#", (unsigned) i);
		break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

#if JS_HAS_DEBUGGER_KEYWORD
	      case JSOP_DEBUGGER:
		js_printf(jp, "\tdebugger;\n");
		todo = -2;
		break;
#endif /* JS_HAS_DEBUGGER_KEYWORD */

	      default:
		todo = -2;
		break;
	    }
	}

	if (todo < 0) {
	    /* -2 means "don't push", -1 means reported error. */
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
    void *mark, *space;
    size_t offsetsz, opcodesz;
    JSBool ok;
    JSScript *oldscript;
    char *last;

    /* Initialize a sprinter for use with the offset stack. */
    ss.printer = jp;
    cx = jp->sprinter.context;
    mark = JS_ARENA_MARK(&cx->tempPool);
    INIT_SPRINTER(cx, &ss.sprinter, &cx->tempPool, PAREN_SLOP);

    /* Allocate the parallel (to avoid padding) offset and opcode stacks. */
    offsetsz = script->depth * sizeof(ptrdiff_t);
    opcodesz = script->depth * sizeof(jsbytecode);
    JS_ARENA_ALLOCATE(space, &cx->tempPool, offsetsz + opcodesz);
    if (!space) {
	ok = JS_FALSE;
        goto out;
    }
    ss.offsets = (ptrdiff_t *) space;
    ss.opcodes = (jsbytecode *) ((char *)space + offsetsz);
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

out:
    /* Free all temporary stuff allocated under this call. */
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    return ok;
}

JSBool
js_DecompileScript(JSPrinter *jp, JSScript *script)
{
    return js_DecompileCode(jp, script, script->code, (uintN)script->length);
}

static const char native_code_str[] = "\t[native code]\n";

JSBool
js_DecompileFunctionBody(JSPrinter *jp, JSFunction *fun)
{
    JSScript *script;
    JSScope *scope, *save;
    JSBool ok;

    script = fun->script;
    if (!script) {
	js_printf(jp, native_code_str);
        return JS_TRUE;
    }
    scope = fun->object ? OBJ_SCOPE(fun->object) : NULL;
    save = jp->scope;
    jp->scope = scope;
    ok = js_DecompileCode(jp, script, script->code, (uintN)script->length);
    jp->scope = save;
    return ok;
}

JSBool
js_DecompileFunction(JSPrinter *jp, JSFunction *fun)
{
    JSScope *scope, *oldscope;
    JSScopeProperty *sprop, *snext;
    JSBool ok;
    JSAtom *atom;
    uintN indent;
    intN i;

    /*
     * If pretty, conform to ECMA-262 Edition 3, 15.3.4.2, by decompiling a
     * FunctionDeclaration.  Otherwise, check the JSFUN_LAMBDA flag and force
     * an expression by parenthesizing.
     */
    if (jp->pretty) {
        js_puts(jp, "\n");
        js_printf(jp, "\t");
    } else {
        if (fun->flags & JSFUN_LAMBDA)
            js_puts(jp, "(");
    }
    if (fun->flags & JSFUN_GETTER)
        js_printf(jp, "%s ", js_getter_str);
    else if (fun->flags & JSFUN_SETTER)
        js_printf(jp, "%s ", js_setter_str);
    js_printf(jp, "%s %s(",
              js_function_str, fun->atom ? ATOM_BYTES(fun->atom) : "");

    scope = NULL;
    if (fun->script && fun->object) {
	/*
	 * Print the parameters.
	 *
	 * This code is complicated by the need to handle duplicate parameter
	 * names.  A duplicate parameter is stored as a property with id equal
	 * to the parameter number, but will not be in order in the linked list
	 * of symbols. So for each parameter we search the list of symbols for
	 * the appropriately numbered parameter, which we can then print.
	 */
	for (i = 0; ; i++) {
	    jsid id;
	    atom = NULL;
	    scope = OBJ_SCOPE(fun->object);
	    for (sprop = scope->props; sprop; sprop = snext) {
		snext = sprop->next;
		if (SPROP_GETTER_SCOPE(sprop, scope) != js_GetArgument)
		    continue;
		if (JSVAL_IS_INT(sprop->id) && JSVAL_TO_INT(sprop->id) == i) {
		    atom = sym_atom(sprop->symbols);
		    break;
		}
		id = (jsid) sym_atom(sprop->symbols);
		if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) == i) {
		    atom = (JSAtom *) sprop->id;
		    break;
		}
	    }
	    if (atom == NULL)
		break;
	    js_printf(jp, (i > 0 ? ", %s" : "%s"), ATOM_BYTES(atom));
	}
    }
    js_printf(jp, ") {\n");
    indent = jp->indent;
    jp->indent += 4;
    if (fun->script && fun->object) {
	oldscope = jp->scope;
	jp->scope = scope;
	ok = js_DecompileScript(jp, fun->script);
	jp->scope = oldscope;
	if (!ok) {
	    jp->indent = indent;
	    return JS_FALSE;
	}
    } else {
	js_printf(jp, native_code_str);
    }
    jp->indent -= 4;
    js_printf(jp, "\t}");
    if (jp->pretty) {
        js_puts(jp, "\n");
    } else {
        if (fun->flags & JSFUN_LAMBDA)
            js_puts(jp, ")");
    }
    return JS_TRUE;
}

JSString *
js_DecompileValueGenerator(JSContext *cx, intN spindex, jsval v,
			   JSString *fallback)
{
    JSStackFrame *fp, *down;
    jsbytecode *pc, *begin, *end, *tmp;
    jsval *sp, *base, *limit;
    JSScript *script;
    JSOp op;
    JSCodeSpec *cs;
    uint32 format, mode;
    intN depth;
    jssrcnote *sn;
    uintN len, off;
    JSPrinter *jp;
    JSString *name;

    fp = cx->fp;
    if (!fp)
        goto do_fallback;

    /* Try to find sp's generating pc depth slots under it on the stack. */
    pc = fp->pc;
    if (spindex == JSDVG_SEARCH_STACK) {
        if (!pc) {
            /*
             * Current frame is native: look under it for a scripted call
             * in which a decompilable bytecode string that generated the
             * value as an actual argument might exist.
             */
            JS_ASSERT(!fp->script && fp->fun && fp->fun->native);
            down = fp->down;
            if (!down)
                goto do_fallback;
            script = down->script;
            base = fp->argv;
            limit = base + fp->argc;
        } else {
            /*
             * This should be a script activation, either a top-level
             * script or a scripted function.  But be paranoid about calls
             * to js_DecompileValueGenerator from code that hasn't fully
             * initialized a (default-all-zeroes) frame.
             */
            script = fp->script;
            base = fp->spbase;
            limit = fp->sp;
        }

        /*
         * Pure paranoia about default-zeroed frames being active while
         * js_DecompileValueGenerator is called.  It can't hurt much now;
         * error reporting performance is not an issue.
         */
        if (!script || !base || !limit)
            goto do_fallback;

        /*
         * Try to find operand-generating pc depth slots below sp.
         *
         * In the native case, we know the arguments have generating pc's
         * under them, on account of fp->down->script being non-null: all
         * compiled scripts get depth slots for generating pc's allocated
         * upon activation, at the top of js_Interpret.
         *
         * In the script or scripted function case, the same reasoning
         * applies to fp rather than to fp->down.
         */
        for (sp = base; sp < limit; sp++) {
            if (*sp == v) {
                depth = (intN)script->depth;
                pc = (jsbytecode *) sp[-depth];
                break;
            }
        }
    } else {
        /*
         * At this point, pc may or may not be null, i.e., we could be in
         * a script activation, or we could be in a native frame that was
         * called by another native function.  Check pc and script.
         */
        if (!pc)
            goto do_fallback;
        script = fp->script;
        if (!script)
            goto do_fallback;

        if (spindex != JSDVG_IGNORE_STACK) {
            depth = (intN)script->depth;
            JS_ASSERT(-depth <= spindex && spindex < 0);
            spindex -= depth;

            base = (jsval *) cx->stackPool.current->base;
            limit = (jsval *) cx->stackPool.current->avail;
            sp = fp->sp + spindex;
            if (JS_UPTRDIFF(sp, base) < JS_UPTRDIFF(limit, base))
                pc = (jsbytecode *) *sp;
        }
    }

    /*
     * Again, be paranoid, this time about possibly loading an invalid pc
     * from sp[-(1+depth)].
     */
    if (JS_UPTRDIFF(pc, script->code) >= (jsuword)script->length) {
        pc = fp->pc;
        if (!pc)
            goto do_fallback;
    }
    op = (JSOp) *pc;
    if (op == JSOP_TRAP)
        op = JS_GetTrapOpcode(cx, script, pc);
    cs = &js_CodeSpec[op];
    format = cs->format;
    mode = (format & JOF_MODEMASK);

    /* NAME ops are self-contained, but others require left context. */
    if (mode == JOF_NAME) {
        begin = pc;
    } else {
        sn = js_GetSrcNote(script, pc);
        if (!sn || SN_TYPE(sn) != SRC_PCBASE)
            goto do_fallback;
        begin = pc - js_GetSrcNoteOffset(sn, 0);
    }
    end = pc + cs->length;
    len = PTRDIFF(end, begin, jsbytecode);

    if (format & (JOF_SET | JOF_DEL | JOF_INCDEC | JOF_IMPORT)) {
        tmp = (jsbytecode *) JS_malloc(cx, len * sizeof(jsbytecode));
        if (!tmp)
            return NULL;
        memcpy(tmp, begin, len * sizeof(jsbytecode));
        if (mode == JOF_NAME) {
            tmp[0] = JSOP_NAME;
        } else {
            /*
             * We must replace the faulting pc's bytecode with a corresponding
             * JSOP_GET* code.  For JSOP_SET{PROP,ELEM}, we must use the "2nd"
             * form of JSOP_GET{PROP,ELEM}, to throw away the assignment op's
             * right-hand operand and decompile it as if it were a GET of its
             * left-hand operand.
             */
            off = len - cs->length;
            JS_ASSERT(off == (uintN) PTRDIFF(pc, begin, jsbytecode));
            if (mode == JOF_PROP) {
                tmp[off] = (format & JOF_SET) ? JSOP_GETPROP2 : JSOP_GETPROP;
            } else if (mode == JOF_ELEM) {
                tmp[off] = (format & JOF_SET) ? JSOP_GETELEM2 : JSOP_GETELEM;
            } else {
#if JS_HAS_LVALUE_RETURN
                JS_ASSERT(op == JSOP_SETCALL);
                tmp[off] = JSOP_CALL;
#else
                JS_ASSERT(0);
#endif
            }
        }
        begin = tmp;
    } else {
        /* No need to revise script bytecode. */
        tmp = NULL;
    }

    jp = js_NewPrinter(cx, "js_DecompileValueGenerator", 0, JS_FALSE);
    if (jp && js_DecompileCode(jp, script, begin, len))
        name = js_GetPrinterOutput(jp);
    else
        name = NULL;
    js_DestroyPrinter(jp);
    if (tmp)
        JS_free(cx, tmp);
    return name;

  do_fallback:
    return fallback ? fallback : js_ValueToString(cx, v);
}
