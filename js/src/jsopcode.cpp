/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=99:
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
 * JS bytecode descriptors, disassemblers, and decompilers.
 */
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsstaticcheck.h"
#include "jsvector.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::gc;

/*
 * Index limit must stay within 32 bits.
 */
JS_STATIC_ASSERT(sizeof(uint32) * JS_BITS_PER_BYTE >= INDEX_LIMIT_LOG2 + 1);

/* Verify JSOP_XXX_LENGTH constant definitions. */
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)               \
    JS_STATIC_ASSERT(op##_LENGTH == length);
#include "jsopcode.tbl"
#undef OPDEF

static const char js_incop_strs[][3] = {"++", "--"};
static const char js_for_each_str[]  = "for each";

const JSCodeSpec js_CodeSpec[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    {length,nuses,ndefs,prec,format},
#include "jsopcode.tbl"
#undef OPDEF
};

uintN js_NumCodeSpecs = JS_ARRAY_LENGTH(js_CodeSpec);

/*
 * Each element of the array is either a source literal associated with JS
 * bytecode or null.
 */
static const char *CodeToken[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    token,
#include "jsopcode.tbl"
#undef OPDEF
};

#if defined(DEBUG) || defined(JS_JIT_SPEW) || defined(JS_METHODJIT_SPEW)
/*
 * Array of JS bytecode names used by DEBUG-only js_Disassemble and by
 * JIT debug spew.
 */
const char *js_CodeName[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    name,
#include "jsopcode.tbl"
#undef OPDEF
};
#endif

/************************************************************************/

#define COUNTS_LEN 16

static ptrdiff_t
GetJumpOffset(jsbytecode *pc, jsbytecode *pc2)
{
    uint32 type;

    type = JOF_OPTYPE(*pc);
    if (JOF_TYPE_IS_EXTENDED_JUMP(type))
        return GET_JUMPX_OFFSET(pc2);
    return GET_JUMP_OFFSET(pc2);
}

uintN
js_GetIndexFromBytecode(JSContext *cx, JSScript *script, jsbytecode *pc,
                        ptrdiff_t pcoff)
{
    JSOp op = js_GetOpcode(cx, script, pc);
    JS_ASSERT(js_CodeSpec[op].length >= 1 + pcoff + UINT16_LEN);

    /*
     * We need to detect index base prefix. It presents when resetbase
     * follows the bytecode.
     */
    uintN span = js_CodeSpec[op].length;
    uintN base = 0;
    if (pc - script->code + span < script->length) {
        JSOp next = js_GetOpcode(cx, script, pc + span);
        if (next == JSOP_RESETBASE) {
            JS_ASSERT(js_GetOpcode(cx, script, pc - JSOP_INDEXBASE_LENGTH) == JSOP_INDEXBASE);
            base = GET_INDEXBASE(pc - JSOP_INDEXBASE_LENGTH);
        } else if (next == JSOP_RESETBASE0) {
            JSOp prev = js_GetOpcode(cx, script, pc - 1);
            JS_ASSERT(JSOP_INDEXBASE1 <= prev && prev <= JSOP_INDEXBASE3);
            base = (prev - JSOP_INDEXBASE1 + 1) << 16;
        }
    }
    return base + GET_UINT16(pc + pcoff);
}

size_t
js_GetVariableBytecodeLength(JSOp op, jsbytecode *pc)
{
    uintN jmplen, ncases;
    jsint low, high;

    JS_ASSERT(js_CodeSpec[op].length == -1);
    switch (op) {
      case JSOP_TABLESWITCHX:
        jmplen = JUMPX_OFFSET_LEN;
        goto do_table;
      case JSOP_TABLESWITCH:
        jmplen = JUMP_OFFSET_LEN;
      do_table:
        /* Structure: default-jump case-low case-high case1-jump ... */
        pc += jmplen;
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
        ncases = (uintN)(high - low + 1);
        return 1 + jmplen + INDEX_LEN + INDEX_LEN + ncases * jmplen;

      case JSOP_LOOKUPSWITCHX:
        jmplen = JUMPX_OFFSET_LEN;
        goto do_lookup;
      default:
        JS_ASSERT(op == JSOP_LOOKUPSWITCH);
        jmplen = JUMP_OFFSET_LEN;
      do_lookup:
        /* Structure: default-jump case-count (case1-value case1-jump) ... */
        pc += jmplen;
        ncases = GET_UINT16(pc);
        return 1 + jmplen + INDEX_LEN + ncases * (INDEX_LEN + jmplen);
    }
}

uintN
js_GetVariableStackUses(JSOp op, jsbytecode *pc)
{
    JS_ASSERT(*pc == op || *pc == JSOP_TRAP);
    JS_ASSERT(js_CodeSpec[op].nuses == -1);
    switch (op) {
      case JSOP_POPN:
        return GET_UINT16(pc);
      case JSOP_LEAVEBLOCK:
        return GET_UINT16(pc);
      case JSOP_LEAVEBLOCKEXPR:
        return GET_UINT16(pc) + 1;
      default:
        /* stack: fun, this, [argc arguments] */
        JS_ASSERT(op == JSOP_NEW || op == JSOP_CALL || op == JSOP_EVAL ||
                  op == JSOP_FUNCALL || op == JSOP_FUNAPPLY);
        return 2 + GET_ARGC(pc);
    }
}

uintN
js_GetEnterBlockStackDefs(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSObject *obj;

    JS_ASSERT(*pc == JSOP_ENTERBLOCK || *pc == JSOP_TRAP);
    GET_OBJECT_FROM_BYTECODE(script, pc, 0, obj);
    return OBJ_BLOCK_COUNT(cx, obj);
}

class AutoScriptUntrapper {
    JSContext *cx;
    JSScript *script;
    jsbytecode *origPC;
    jsbytecode *newPC;
#ifdef DEBUG
    bool assertionBefore;
#endif

public:
    AutoScriptUntrapper(JSContext *cx, JSScript *script, jsbytecode **pc)
        : cx(cx), script(script), origPC(*pc)
#ifdef DEBUG
          , assertionBefore(false)
#endif
    {
        jsbytecode *newCode = js_UntrapScriptCode(cx, script);
        if (newCode == script->code) {
            // No change needed
            newPC = origPC;
        } else {
            *pc = newPC = origPC + (newCode - script->code);
            script->code = newCode;
#ifdef DEBUG
            assertionBefore = cx->stackIterAssertionEnabled;
            cx->stackIterAssertionEnabled = false;
#endif
        }
    }
    ~AutoScriptUntrapper()
    {
        ptrdiff_t delta = newPC - origPC;
        if (delta) {
            jsbytecode *oldCode = script->code - delta;
            cx->free_(script->code);
            script->code = oldCode;
#ifdef DEBUG
            cx->stackIterAssertionEnabled = assertionBefore;
#endif
        }
    }
};

#ifdef DEBUG

/*
 * If pc != NULL, include a prefix indicating whether the PC is at the current line.
 * If counts != NULL, include a counter of the number of times each op was executed.
 */
JS_FRIEND_API(JSBool)
js_DisassembleAtPC(JSContext *cx, JSScript *script, JSBool lines, jsbytecode *pc, Sprinter *sp)
{
    jsbytecode *next, *end;
    uintN len;

    SprintCString(sp, "loc   ");
    if (script->pcCounters)
        Sprint(sp, "counts%*s x ", COUNTS_LEN - strlen("counts"), "");
    if (lines)
        SprintCString(sp, "line");
    SprintCString(sp, "  op\n");
    SprintCString(sp, "----- ");
    if (script->pcCounters) {
        for (int i = 0; i < COUNTS_LEN; ++i)
            SprintCString(sp, "-");
        SprintCString(sp, "   ");
    }
    if (lines)
        SprintCString(sp, "----");
    SprintCString(sp, "  --\n");

    next = script->code;
    end = next + script->length;
    while (next < end) {
        if (next == script->main())
            SprintCString(sp, "main:\n");
        if (pc != NULL) {
            if (pc == next)
                SprintCString(sp, "--> ");
            else
                SprintCString(sp, "    ");
        }
        len = js_Disassemble1(cx, script, next, next - script->code, lines, sp);
        if (!len)
            return JS_FALSE;
        next += len;
    }
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_Disassemble(JSContext *cx, JSScript *script, JSBool lines, Sprinter *sp)
{
    return js_DisassembleAtPC(cx, script, lines, NULL, sp);
}

JS_FRIEND_API(JSBool)
js_DumpPC(JSContext *cx)
{
    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);
    JSBool ok = js_DisassembleAtPC(cx, cx->fp()->script(), true, cx->regs().pc, &sprinter);
    fprintf(stdout, "%s", sprinter.base);
    return ok;
}

JSBool
js_DumpScript(JSContext *cx, JSScript *script)
{
    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);
    JSBool ok = js_Disassemble(cx, script, true, &sprinter);
    fprintf(stdout, "%s", sprinter.base);
    return ok;
}

static char *
QuoteString(Sprinter *sp, JSString *str, uint32 quote);

static bool
ToDisassemblySource(JSContext *cx, jsval v, JSAutoByteString *bytes)
{
    if (JSVAL_IS_STRING(v)) {
        Sprinter sprinter;
        LifoAlloc &tla = cx->tempLifoAlloc();
        LifoAllocScope las(&tla);
        INIT_SPRINTER(cx, &sprinter, &tla, 0);
        char *nbytes = QuoteString(&sprinter, JSVAL_TO_STRING(v), '"');
        if (!nbytes)
            return false;
        nbytes = JS_sprintf_append(NULL, "%s", nbytes);
        if (!nbytes)
            return false;
        bytes->initBytes(nbytes);
        return true;
    }

    if (cx->runtime->gcRunning || JS_THREAD_DATA(cx)->noGCOrAllocationCheck) {
        char *source = JS_sprintf_append(NULL, "<value>");
        if (!source)
            return false;
        bytes->initBytes(source);
        return true;
    }

    if (!JSVAL_IS_PRIMITIVE(v)) {
        JSObject *obj = JSVAL_TO_OBJECT(v);
        Class *clasp = obj->getClass();

        if (clasp == &BlockClass) {
            char *source = JS_sprintf_append(NULL, "depth %d {", OBJ_BLOCK_DEPTH(cx, obj));
            if (!source)
                return false;

            Shape::Range r = obj->lastProperty()->all();
            while (!r.empty()) {
                const Shape &shape = r.front();
                JSAutoByteString bytes;
                if (!js_AtomToPrintableString(cx, JSID_TO_ATOM(shape.propid), &bytes))
                    return false;

                r.popFront();
                source = JS_sprintf_append(source, "%s: %d%s",
                                           bytes.ptr(), shape.shortid,
                                           !r.empty() ? ", " : "");
                if (!source)
                    return false;
            }

            source = JS_sprintf_append(source, "}");
            if (!source)
                return false;
            bytes->initBytes(source);
            return true;
        }

        if (clasp == &FunctionClass) {
            JSFunction *fun = obj->getFunctionPrivate();
            JSString *str = JS_DecompileFunction(cx, fun, JS_DONT_PRETTY_PRINT);
            if (!str)
                return false;
            return bytes->encode(cx, str);
        }

        if (clasp == &RegExpClass) {
            AutoValueRooter tvr(cx);
            if (!js_regexp_toString(cx, obj, tvr.addr()))
                return false;
            return bytes->encode(cx, tvr.value().toString());
        }
    }

    return !!js_ValueToPrintable(cx, v, bytes, true);
}

JS_FRIEND_API(uintN)
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc,
                uintN loc, JSBool lines, Sprinter *sp)
{
    JSOp op;
    const JSCodeSpec *cs;
    ptrdiff_t len, off, jmplen;
    uint32 type;
    JSAtom *atom;
    uintN index;
    JSObject *obj;
    jsval v;
    jsint i;
    JSPCCounters& counts(script->pcCounters);

    AutoScriptUntrapper untrapper(cx, script, &pc);

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
    len = (ptrdiff_t) cs->length;
    Sprint(sp, "%05u:", loc);
    if (counts) {
        ptrdiff_t start = Sprint(sp, "%.0f", counts.get(0, loc));
        for (size_t i = 1; i < JSPCCounters::NUM_COUNTERS; ++i)
            Sprint(sp, "/%.0f", counts.get(i, loc));
        int l = Sprint(sp, "") - start;
        if (l < COUNTS_LEN)
            Sprint(sp, "%*s", COUNTS_LEN - l, "");
        double mjitHits = counts.get(JSPCCounters::METHODJIT, loc);
        if (mjitHits) {
            Sprint(sp, "  %.0f/%.0f",
                   counts.get(JSPCCounters::METHODJIT_CODE, loc) / mjitHits,
                   counts.get(JSPCCounters::METHODJIT_PICS, loc) / mjitHits);
        }
        Sprint(sp, " x ");
    }
    if (lines)
        Sprint(sp, "%4u", JS_PCToLineNumber(cx, script, pc));
    Sprint(sp, "  %s", js_CodeName[op]);
    type = JOF_TYPE(cs->format);
    switch (type) {
      case JOF_BYTE:
          // Scan the trynotes to find the associated catch block
          // and make the try opcode look like a jump instruction
          // with an offset. This simplifies code coverage analysis
          // based on this disassembled output.
          if (op == JSOP_TRY) {
              JSTryNoteArray *trynotes = script->trynotes();
              uint32 i;
              for(i = 0; i < trynotes->length; i++) {
                  JSTryNote note = trynotes->vector[i];
                  if (note.kind == JSTRY_CATCH && note.start == loc + 1) {
                      Sprint(sp, " %u (%+d)",
                             (unsigned int) (loc+note.length+1),
                             (int) (note.length+1));
                      break;
                  }
              }
          }
        break;

      case JOF_JUMP:
      case JOF_JUMPX:
        off = GetJumpOffset(pc, pc);
        Sprint(sp, " %u (%+d)", loc + (intN) off, (intN) off);
        break;

      case JOF_ATOM:
      case JOF_OBJECT:
      case JOF_REGEXP:
        index = js_GetIndexFromBytecode(cx, script, pc, 0);
        if (type == JOF_ATOM) {
            if (op == JSOP_DOUBLE) {
                v = script->getConst(index);
            } else {
                JS_GET_SCRIPT_ATOM(script, pc, index, atom);
                v = STRING_TO_JSVAL(atom);
            }
        } else {
            if (type == JOF_OBJECT) {
                /* Don't call obj.toSource if analysis/inference is active. */
                if (cx->compartment->activeAnalysis) {
                    Sprint(sp, " object");
                    break;
                }
                obj = script->getObject(index);
            } else {
                obj = script->getRegExp(index);
            }
            v = OBJECT_TO_JSVAL(obj);
        }
        {
            JSAutoByteString bytes;
            if (!ToDisassemblySource(cx, v, &bytes))
                return 0;
            Sprint(sp, " %s", bytes.ptr());
        }
        break;

      case JOF_UINT16PAIR:
        i = (jsint)GET_UINT16(pc);
        Sprint(sp, " %d", i);
        pc += UINT16_LEN;
        /* FALL THROUGH */

      case JOF_UINT16:
        i = (jsint)GET_UINT16(pc);
        goto print_int;

      case JOF_TABLESWITCH:
      case JOF_TABLESWITCHX:
      {
        jsbytecode *pc2;
        jsint i, low, high;

        jmplen = (type == JOF_TABLESWITCH) ? JUMP_OFFSET_LEN
                                           : JUMPX_OFFSET_LEN;
        pc2 = pc;
        off = GetJumpOffset(pc, pc2);
        pc2 += jmplen;
        low = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        Sprint(sp, " defaultOffset %d low %d high %d", intN(off), low, high);
        for (i = low; i <= high; i++) {
            off = GetJumpOffset(pc, pc2);
            Sprint(sp, "\n\t%d: %d", i, intN(off));
            pc2 += jmplen;
        }
        len = 1 + pc2 - pc;
        break;
      }

      case JOF_LOOKUPSWITCH:
      case JOF_LOOKUPSWITCHX:
      {
        jsbytecode *pc2;
        jsatomid npairs;

        jmplen = (type == JOF_LOOKUPSWITCH) ? JUMP_OFFSET_LEN
                                            : JUMPX_OFFSET_LEN;
        pc2 = pc;
        off = GetJumpOffset(pc, pc2);
        pc2 += jmplen;
        npairs = GET_UINT16(pc2);
        pc2 += UINT16_LEN;
        Sprint(sp, " offset %d npairs %u", intN(off), uintN(npairs));
        while (npairs) {
            uint16 constIndex = GET_INDEX(pc2);
            pc2 += INDEX_LEN;
            off = GetJumpOffset(pc, pc2);
            pc2 += jmplen;

            JSAutoByteString bytes;
            if (!ToDisassemblySource(cx, script->getConst(constIndex), &bytes))
                return 0;
            Sprint(sp, "\n\t%s: %d", bytes.ptr(), intN(off));
            npairs--;
        }
        len = 1 + pc2 - pc;
        break;
      }

      case JOF_QARG:
        Sprint(sp, " %u", GET_ARGNO(pc));
        break;

      case JOF_LOCAL:
        Sprint(sp, " %u", GET_SLOTNO(pc));
        break;

      case JOF_SLOTATOM:
      case JOF_SLOTOBJECT: {
        Sprint(sp, " %u", GET_SLOTNO(pc));
        index = js_GetIndexFromBytecode(cx, script, pc, SLOTNO_LEN);
        if (type == JOF_SLOTATOM) {
            JS_GET_SCRIPT_ATOM(script, pc, index, atom);
            v = STRING_TO_JSVAL(atom);
        } else {
            obj = script->getObject(index);
            v = OBJECT_TO_JSVAL(obj);
        }

        JSAutoByteString bytes;
        if (!ToDisassemblySource(cx, v, &bytes))
            return 0;
        Sprint(sp, " %s", bytes.ptr());
        break;
      }

      case JOF_UINT24:
        JS_ASSERT(op == JSOP_UINT24 || op == JSOP_NEWARRAY);
        i = (jsint)GET_UINT24(pc);
        goto print_int;

      case JOF_UINT8:
        i = pc[1];
        goto print_int;

      case JOF_INT8:
        i = GET_INT8(pc);
        goto print_int;

      case JOF_INT32:
        JS_ASSERT(op == JSOP_INT32);
        i = GET_INT32(pc);
      print_int:
        Sprint(sp, " %d", i);
        break;

      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%lx", (unsigned long) cs->format);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNKNOWN_FORMAT, numBuf);
        return 0;
      }
    }
    SprintCString(sp, "\n");
    return len;
}

#endif /* DEBUG */

/************************************************************************/

#define OFF2STR(sp,off) ((sp)->base + (off))
#define STR2OFF(sp,str) ((str) - (sp)->base)
#define RETRACT(sp,str) ((sp)->offset = STR2OFF(sp, str))

static JSBool
SprintEnsureBuffer(Sprinter *sp, size_t len)
{
    ptrdiff_t nb;
    char *base;

    nb = (sp->offset + len + 1) - sp->size;
    if (nb < 0)
        return JS_TRUE;
    base = sp->base;
    if (!base)
        base = static_cast<char *>(sp->pool->allocUnaligned(nb));
    else
        base = static_cast<char *>(sp->pool->reallocUnaligned(base, sp->size, nb));
    if (!base) {
        js_ReportOutOfMemory(sp->context);
        return JS_FALSE;
    }
    sp->base = base;
    sp->size += nb;
    return JS_TRUE;
}

namespace js {

char *
SprintReserveAmount(Sprinter *sp, size_t len)
{
    /* Allocate space for s, including the '\0' at the end. */
    if (!SprintEnsureBuffer(sp, len))
        return NULL;

    /* Advance offset and return the previous offset for caller to fill. */
    ptrdiff_t offset = sp->offset;
    sp->offset += len;
    return sp->base + offset;
}

ptrdiff_t
SprintPut(Sprinter *sp, const char *s, size_t len)
{
    ptrdiff_t offset = sp->size; /* save old size */
    char *bp = sp->base;         /* save old base */

    /* Allocate space for s, including the '\0' at the end. */
    if (!SprintEnsureBuffer(sp, len))
        return -1;

    if (sp->base != bp &&               /* buffer was realloc'ed */
        s >= bp && s < bp + offset) {   /* s was within the buffer */
        s = sp->base + (s - bp);        /* this is where it lives now */
    }

    /* Advance offset and copy s into sp's buffer. */
    offset = sp->offset;
    sp->offset += len;
    bp = sp->base + offset;
    memmove(bp, s, len);
    bp[len] = 0;
    return offset;
}

ptrdiff_t
SprintCString(Sprinter *sp, const char *s)
{
    return SprintPut(sp, s, strlen(s));
}

ptrdiff_t
SprintString(Sprinter *sp, JSString *str)
{
    size_t length = str->length();
    const jschar *chars = str->getChars(sp->context);
    if (!chars)
        return -1;

    size_t size = GetDeflatedStringLength(sp->context, chars, length);
    if (size == (size_t)-1 || !SprintEnsureBuffer(sp, size))
        return -1;

    ptrdiff_t offset = sp->offset;
    sp->offset += size;
    DeflateStringToBuffer(sp->context, chars, length, sp->base + offset, &size);
    sp->base[sp->offset] = 0;
    return offset;
}

ptrdiff_t
Sprint(Sprinter *sp, const char *format, ...)
{
    va_list ap;
    char *bp;
    ptrdiff_t offset;

    va_start(ap, format);
    bp = JS_vsmprintf(format, ap);      /* XXX vsaprintf */
    va_end(ap);
    if (!bp) {
        JS_ReportOutOfMemory(sp->context);
        return -1;
    }
    offset = SprintCString(sp, bp);
    sp->context->free_(bp);
    return offset;
}

} // namespace js

const char js_EscapeMap[] = {
    '\b', 'b',
    '\f', 'f',
    '\n', 'n',
    '\r', 'r',
    '\t', 't',
    '\v', 'v',
    '"',  '"',
    '\'', '\'',
    '\\', '\\',
    '\0', '0'
};

#define DONT_ESCAPE     0x10000

static char *
QuoteString(Sprinter *sp, JSString *str, uint32 quote)
{
    /* Sample off first for later return value pointer computation. */
    JSBool dontEscape = (quote & DONT_ESCAPE) != 0;
    jschar qc = (jschar) quote;
    ptrdiff_t off = sp->offset;
    if (qc && Sprint(sp, "%c", (char)qc) < 0)
        return NULL;

    const jschar *s = str->getChars(sp->context);
    if (!s)
        return NULL;
    const jschar *z = s + str->length();

    /* Loop control variables: z points at end of string sentinel. */
    for (const jschar *t = s; t < z; s = ++t) {
        /* Move t forward from s past un-quote-worthy characters. */
        jschar c = *t;
        while (c < 127 && isprint(c) && c != qc && c != '\\' && c != '\t') {
            c = *++t;
            if (t == z)
                break;
        }
        ptrdiff_t len = t - s;

        /* Allocate space for s, including the '\0' at the end. */
        if (!SprintEnsureBuffer(sp, len))
            return NULL;

        /* Advance sp->offset and copy s into sp's buffer. */
        char *bp = sp->base + sp->offset;
        sp->offset += len;
        while (--len >= 0)
            *bp++ = (char) *s++;
        *bp = '\0';

        if (t == z)
            break;

        /* Use js_EscapeMap, \u, or \x only if necessary. */
        bool ok;
        const char *e;
        if (!(c >> 8) && (e = strchr(js_EscapeMap, (int)c)) != NULL) {
            ok = dontEscape
                 ? Sprint(sp, "%c", (char)c) >= 0
                 : Sprint(sp, "\\%c", e[1]) >= 0;
        } else {
            /*
             * Use \x only if the high byte is 0 and we're in a quoted string,
             * because ECMA-262 allows only \u, not \x, in Unicode identifiers
             * (see bug 621814).
             */
            ok = Sprint(sp, (qc && !(c >> 8)) ? "\\x%02X" : "\\u%04X", c) >= 0;
        }
        if (!ok)
            return NULL;
    }

    /* Sprint the closing quote and return the quoted string. */
    if (qc && Sprint(sp, "%c", (char)qc) < 0)
        return NULL;

    /*
     * If we haven't Sprint'd anything yet, Sprint an empty string so that
     * the OFF2STR below gives a valid result.
     */
    if (off == sp->offset && Sprint(sp, "") < 0)
        return NULL;
    return OFF2STR(sp, off);
}

JSString *
js_QuoteString(JSContext *cx, JSString *str, jschar quote)
{
    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);
    char *bytes = QuoteString(&sprinter, str, quote);
    JSString *escstr = bytes ? JS_NewStringCopyZ(cx, bytes) : NULL;
    return escstr;
}

/************************************************************************/

struct JSPrinter {
    Sprinter        sprinter;       /* base class state */
    LifoAlloc       pool;           /* string allocation pool */
    uintN           indent;         /* indentation in spaces */
    bool            pretty;         /* pretty-print: indent, use newlines */
    bool            grouped;        /* in parenthesized expression context */
    bool            strict;         /* in code marked strict */
    JSScript        *script;        /* script being printed */
    jsbytecode      *dvgfence;      /* DecompileExpression fencepost */
    jsbytecode      **pcstack;      /* DecompileExpression modeled stack */
    JSFunction      *fun;           /* interpreted function */
    Vector<JSAtom *> *localNames;   /* argument and variable names */
};

JSPrinter *
js_NewPrinter(JSContext *cx, const char *name, JSFunction *fun,
              uintN indent, JSBool pretty, JSBool grouped, JSBool strict)
{
    JSPrinter *jp = (JSPrinter *) cx->malloc_(sizeof(JSPrinter));
    if (!jp)
        return NULL;
    INIT_SPRINTER(cx, &jp->sprinter, &jp->pool, 0);
    new (&jp->pool) LifoAlloc(1024);
    jp->indent = indent;
    jp->pretty = !!pretty;
    jp->grouped = !!grouped;
    jp->strict = !!strict;
    jp->script = NULL;
    jp->dvgfence = NULL;
    jp->pcstack = NULL;
    jp->fun = fun;
    jp->localNames = NULL;
    if (fun && fun->isInterpreted() && fun->script()->bindings.hasLocalNames()) {
        jp->localNames = cx->new_<Vector<JSAtom *> >(cx);
        if (!jp->localNames || !fun->script()->bindings.getLocalNameArray(cx, jp->localNames)) {
            js_DestroyPrinter(jp);
            return NULL;
        }
    }
    return jp;
}

void
js_DestroyPrinter(JSPrinter *jp)
{
    jp->pool.freeAll();
    Foreground::delete_(jp->localNames);
    jp->sprinter.context->free_(jp);
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
    jp->pool.freeAll();
    INIT_SPRINTER(cx, &jp->sprinter, &jp->pool, 0);
    return str;
}

/*
 * NB: Indexed by SRC_DECL_* defines from jsemit.h.
 */
static const char * const var_prefix[] = {"var ", "const ", "let "};

static const char *
VarPrefix(jssrcnote *sn)
{
    if (sn && (SN_TYPE(sn) == SRC_DECL || SN_TYPE(sn) == SRC_GROUPASSIGN)) {
        ptrdiff_t type = js_GetSrcNoteOffset(sn, 0);
        if ((uintN)type <= SRC_DECL_LET)
            return var_prefix[type];
    }
    return "";
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
        format++;
        if (jp->pretty && Sprint(&jp->sprinter, "%*s", jp->indent, "") < 0) {
            va_end(ap);
            return -1;
        }
    }

    /* Suppress newlines (must be once per format, at the end) if not pretty. */
    fp = NULL;
    if (!jp->pretty && format[cc = strlen(format) - 1] == '\n') {
        fp = JS_strdup(jp->sprinter.context, format);
        if (!fp) {
            va_end(ap);
            return -1;
        }
        fp[cc] = '\0';
        format = fp;
    }

    /* Allocate temp space, convert format, and put. */
    bp = JS_vsmprintf(format, ap);      /* XXX vsaprintf */
    if (fp) {
        jp->sprinter.context->free_(fp);
        format = NULL;
    }
    if (!bp) {
        JS_ReportOutOfMemory(jp->sprinter.context);
        va_end(ap);
        return -1;
    }

    cc = strlen(bp);
    if (SprintPut(&jp->sprinter, bp, (size_t)cc) < 0)
        cc = -1;
    jp->sprinter.context->free_(bp);

    va_end(ap);
    return cc;
}

JSBool
js_puts(JSPrinter *jp, const char *s)
{
    return SprintCString(&jp->sprinter, s) >= 0;
}

/************************************************************************/

typedef struct SprintStack {
    Sprinter    sprinter;       /* sprinter for postfix to infix buffering */
    ptrdiff_t   *offsets;       /* stack of postfix string offsets */
    jsbytecode  *opcodes;       /* parallel stack of JS opcodes */
    uintN       top;            /* top of stack index */
    uintN       inArrayInit;    /* array initialiser/comprehension level */
    JSBool      inGenExp;       /* in generator expression */
    JSPrinter   *printer;       /* permanent output goes here */
} SprintStack;

/*
 * Find the depth of the operand stack when the interpreter reaches the given
 * pc in script. pcstack must have space for least script->depth elements. On
 * return it will contain pointers to opcodes that populated the interpreter's
 * current operand stack.
 *
 * This function cannot raise an exception or error. However, due to a risk of
 * potential bugs when modeling the stack, the function returns -1 if it
 * detects an inconsistency in the model. Such an inconsistency triggers an
 * assert in a debug build.
 */
static intN
ReconstructPCStack(JSContext *cx, JSScript *script, jsbytecode *pc,
                   jsbytecode **pcstack);

#define FAILED_EXPRESSION_DECOMPILER ((char *) 1)

/*
 * Decompile a part of expression up to the given pc. The function returns
 * NULL on out-of-memory, or the FAILED_EXPRESSION_DECOMPILER sentinel when
 * the decompiler fails due to a bug and/or unimplemented feature, or the
 * decompiled string on success.
 */
static char *
DecompileExpression(JSContext *cx, JSScript *script, JSFunction *fun,
                    jsbytecode *pc);

/*
 * Get a stacked offset from ss->sprinter.base, or if the stacked value |off|
 * is negative, fetch the generating pc from printer->pcstack[-2 - off] and
 * decompile the code that generated the missing value.  This is used when
 * reporting errors, where the model stack will lack |pcdepth| non-negative
 * offsets (see DecompileExpression and DecompileCode).
 *
 * If the stacked offset is -1, return 0 to index the NUL padding at the start
 * of ss->sprinter.base.  If this happens, it means there is a decompiler bug
 * to fix, but it won't violate memory safety.
 */
static ptrdiff_t
GetOff(SprintStack *ss, uintN i)
{
    ptrdiff_t off;
    jsbytecode *pc;
    char *bytes;

    off = ss->offsets[i];
    if (off >= 0)
        return off;

    JS_ASSERT(off <= -2);
    JS_ASSERT(ss->printer->pcstack);
    if (off <= -2 && ss->printer->pcstack) {
        pc = ss->printer->pcstack[-2 - off];
        bytes = DecompileExpression(ss->sprinter.context, ss->printer->script,
                                    ss->printer->fun, pc);
        if (!bytes)
            return 0;
        if (bytes != FAILED_EXPRESSION_DECOMPILER) {
            off = SprintCString(&ss->sprinter, bytes);
            if (off < 0)
                off = 0;
            ss->offsets[i] = off;
            ss->sprinter.context->free_(bytes);
            return off;
        }
        if (!ss->sprinter.base && SprintPut(&ss->sprinter, "", 0) >= 0) {
            memset(ss->sprinter.base, 0, ss->sprinter.offset);
            ss->offsets[i] = -1;
        }
    }
    return 0;
}

static const char *
GetStr(SprintStack *ss, uintN i)
{
    ptrdiff_t off;

    /*
     * Must call GetOff before using ss->sprinter.base, since it may be null
     * until bootstrapped by GetOff.
     */
    off = GetOff(ss, i);
    return OFF2STR(&ss->sprinter, off);
}

/*
 * Gap between stacked strings to allow for insertion of parens and commas
 * when auto-parenthesizing expressions and decompiling array initialisers.
 */
#define PAREN_SLOP      (2 + 1)

/* Fake opcodes (see jsopcode.h) must not overflow unsigned 8-bit space. */
JS_STATIC_ASSERT(JSOP_FAKE_LIMIT <= 255);

static void
AddParenSlop(SprintStack *ss)
{
    memset(OFF2STR(&ss->sprinter, ss->sprinter.offset), 0, PAREN_SLOP);
    ss->sprinter.offset += PAREN_SLOP;
}

static JSBool
PushOff(SprintStack *ss, ptrdiff_t off, JSOp op)
{
    uintN top;

    if (!SprintEnsureBuffer(&ss->sprinter, PAREN_SLOP))
        return JS_FALSE;

    /* ss->top points to the next free slot; be paranoid about overflow. */
    top = ss->top;
    JS_ASSERT(top < StackDepth(ss->printer->script));
    if (top >= StackDepth(ss->printer->script)) {
        JS_ReportOutOfMemory(ss->sprinter.context);
        return JS_FALSE;
    }

    /* The opcodes stack must contain real bytecodes that index js_CodeSpec. */
    ss->offsets[top] = off;
    ss->opcodes[top] = jsbytecode((op == JSOP_GETPROP2) ? JSOP_GETPROP
                                : (op == JSOP_GETELEM2) ? JSOP_GETELEM
                                : op);
    ss->top = ++top;
    AddParenSlop(ss);
    return JS_TRUE;
}

static ptrdiff_t
PopOffPrec(SprintStack *ss, uint8 prec)
{
    uintN top;
    const JSCodeSpec *topcs;
    ptrdiff_t off;

    /* ss->top points to the next free slot; be paranoid about underflow. */
    top = ss->top;
    JS_ASSERT(top != 0);
    if (top == 0)
        return 0;

    ss->top = --top;
    off = GetOff(ss, top);
    topcs = &js_CodeSpec[ss->opcodes[top]];
    if (topcs->prec != 0 && topcs->prec < prec) {
        ss->sprinter.offset = ss->offsets[top] = off - 2;
        off = Sprint(&ss->sprinter, "(%s)", OFF2STR(&ss->sprinter, off));
    } else {
        ss->sprinter.offset = off;
    }
    return off;
}

static const char *
PopStrPrec(SprintStack *ss, uint8 prec)
{
    ptrdiff_t off;

    off = PopOffPrec(ss, prec);
    return OFF2STR(&ss->sprinter, off);
}

static ptrdiff_t
PopOff(SprintStack *ss, JSOp op)
{
    return PopOffPrec(ss, js_CodeSpec[op].prec);
}

static const char *
PopStr(SprintStack *ss, JSOp op)
{
    return PopStrPrec(ss, js_CodeSpec[op].prec);
}

static inline bool
IsInitializerOp(unsigned char op)
{
    return op == JSOP_NEWINIT || op == JSOP_NEWARRAY || op == JSOP_NEWOBJECT;
}

typedef struct TableEntry {
    jsval       key;
    ptrdiff_t   offset;
    JSAtom      *label;
    jsint       order;          /* source order for stable tableswitch sort */
} TableEntry;

static JSBool
CompareOffsets(void *arg, const void *v1, const void *v2, int *result)
{
    ptrdiff_t offset_diff;
    const TableEntry *te1 = (const TableEntry *) v1,
                     *te2 = (const TableEntry *) v2;

    offset_diff = te1->offset - te2->offset;
    *result = (offset_diff == 0 ? te1->order - te2->order
               : offset_diff < 0 ? -1
               : 1);
    return JS_TRUE;
}

static ptrdiff_t
SprintDoubleValue(Sprinter *sp, jsval v, JSOp *opp)
{
    jsdouble d;
    ptrdiff_t todo;
    char *s;

    JS_ASSERT(JSVAL_IS_DOUBLE(v));
    d = JSVAL_TO_DOUBLE(v);
    if (JSDOUBLE_IS_NEGZERO(d)) {
        todo = SprintCString(sp, "-0");
        *opp = JSOP_NEG;
    } else if (!JSDOUBLE_IS_FINITE(d)) {
        /* Don't use Infinity and NaN, they're mutable. */
        todo = SprintCString(sp,
                             JSDOUBLE_IS_NaN(d)
                             ? "0 / 0"
                             : (d < 0)
                             ? "1 / -0"
                             : "1 / 0");
        *opp = JSOP_DIV;
    } else {
        ToCStringBuf cbuf;
        s = NumberToCString(sp->context, &cbuf, d);
        if (!s) {
            JS_ReportOutOfMemory(sp->context);
            return -1;
        }
        JS_ASSERT(strcmp(s, js_Infinity_str) &&
                  (*s != '-' ||
                   strcmp(s + 1, js_Infinity_str)) &&
                  strcmp(s, js_NaN_str));
        todo = Sprint(sp, s);
    }
    return todo;
}

static jsbytecode *
Decompile(SprintStack *ss, jsbytecode *pc, intN nb, JSOp nextop);

static JSBool
DecompileSwitch(SprintStack *ss, TableEntry *table, uintN tableLength,
                jsbytecode *pc, ptrdiff_t switchLength,
                ptrdiff_t defaultOffset, JSBool isCondSwitch)
{
    JSContext *cx;
    JSPrinter *jp;
    ptrdiff_t off, off2, diff, caseExprOff, todo;
    char *lval, *rval;
    uintN i;
    jsval key;
    JSString *str;

    cx = ss->sprinter.context;
    jp = ss->printer;

    /* JSOP_CONDSWITCH doesn't pop, unlike JSOP_{LOOKUP,TABLE}SWITCH. */
    off = isCondSwitch ? GetOff(ss, ss->top-1) : PopOff(ss, JSOP_NOP);
    lval = OFF2STR(&ss->sprinter, off);

    js_printf(jp, "\tswitch (%s) {\n", lval);

    if (tableLength) {
        diff = table[0].offset - defaultOffset;
        if (diff > 0) {
            jp->indent += 2;
            js_printf(jp, "\t%s:\n", js_default_str);
            jp->indent += 2;
            if (!Decompile(ss, pc + defaultOffset, diff, JSOP_NOP))
                return JS_FALSE;
            jp->indent -= 4;
        }

        caseExprOff = isCondSwitch ? JSOP_CONDSWITCH_LENGTH : 0;

        for (i = 0; i < tableLength; i++) {
            off = table[i].offset;
            off2 = (i + 1 < tableLength) ? table[i + 1].offset : switchLength;

            key = table[i].key;
            if (isCondSwitch) {
                ptrdiff_t nextCaseExprOff;

                /*
                 * key encodes the JSOP_CASE bytecode's offset from switchtop.
                 * The next case expression follows immediately, unless we are
                 * at the last case.
                 */
                nextCaseExprOff = (ptrdiff_t)JSVAL_TO_INT(key);
                nextCaseExprOff += js_CodeSpec[pc[nextCaseExprOff]].length;
                jp->indent += 2;
                if (!Decompile(ss, pc + caseExprOff,
                               nextCaseExprOff - caseExprOff, JSOP_NOP)) {
                    return JS_FALSE;
                }
                caseExprOff = nextCaseExprOff;

                /* Balance the stack as if this JSOP_CASE matched. */
                --ss->top;
            } else {
                /*
                 * key comes from an atom, not the decompiler, so we need to
                 * quote it if it's a string literal.  But if table[i].label
                 * is non-null, key was constant-propagated and label is the
                 * name of the const we should show as the case label.  We set
                 * key to undefined so this identifier is escaped, if required
                 * by non-ASCII characters, but not quoted, by QuoteString.
                 */
                todo = -1;
                if (table[i].label) {
                    str = table[i].label;
                    key = JSVAL_VOID;
                } else if (JSVAL_IS_DOUBLE(key)) {
                    JSOp junk;

                    todo = SprintDoubleValue(&ss->sprinter, key, &junk);
                    if (todo < 0)
                        return JS_FALSE;
                    str = NULL;
                } else {
                    str = js_ValueToString(cx, key);
                    if (!str)
                        return JS_FALSE;
                }
                if (todo >= 0) {
                    rval = OFF2STR(&ss->sprinter, todo);
                } else {
                    rval = QuoteString(&ss->sprinter, str, (jschar)
                                       (JSVAL_IS_STRING(key) ? '"' : 0));
                    if (!rval)
                        return JS_FALSE;
                }
                RETRACT(&ss->sprinter, rval);
                jp->indent += 2;
                js_printf(jp, "\tcase %s:\n", rval);
            }

            jp->indent += 2;
            if (off <= defaultOffset && defaultOffset < off2) {
                diff = defaultOffset - off;
                if (diff != 0) {
                    if (!Decompile(ss, pc + off, diff, JSOP_NOP))
                        return JS_FALSE;
                    off = defaultOffset;
                }
                jp->indent -= 2;
                js_printf(jp, "\t%s:\n", js_default_str);
                jp->indent += 2;
            }
            if (!Decompile(ss, pc + off, off2 - off, JSOP_NOP))
                return JS_FALSE;
            jp->indent -= 4;

            /* Re-balance as if last JSOP_CASE or JSOP_DEFAULT mismatched. */
            if (isCondSwitch)
                ++ss->top;
        }
    }

    if (defaultOffset == switchLength) {
        jp->indent += 2;
        js_printf(jp, "\t%s:;\n", js_default_str);
        jp->indent -= 2;
    }
    js_printf(jp, "\t}\n");

    /* By the end of a JSOP_CONDSWITCH, the discriminant has been popped. */
    if (isCondSwitch)
        --ss->top;
    return JS_TRUE;
}

#define LOCAL_ASSERT_CUSTOM(expr, BAD_EXIT)                                   \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(expr);                                                      \
        if (!(expr)) { BAD_EXIT; }                                            \
    JS_END_MACRO

#define LOCAL_ASSERT_RV(expr, rv)                                             \
    LOCAL_ASSERT_CUSTOM(expr, return (rv))

static JSAtom *
GetArgOrVarAtom(JSPrinter *jp, uintN slot)
{
    LOCAL_ASSERT_RV(jp->fun, NULL);
    LOCAL_ASSERT_RV(slot < jp->fun->script()->bindings.countLocalNames(), NULL);
    JSAtom *name = (*jp->localNames)[slot];
#if !JS_HAS_DESTRUCTURING
    LOCAL_ASSERT_RV(name, NULL);
#endif
    return name;
}

const char *
GetLocal(SprintStack *ss, jsint i)
{
#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, "")

    ptrdiff_t off = ss->offsets[i];
    if (off >= 0)
        return OFF2STR(&ss->sprinter, off);

    /*
     * We must be called from js_DecompileValueGenerator (via Decompile) when
     * dereferencing a local that's undefined or null. Search script->objects
     * for the block containing this local by its stack index, i.
     *
     * In case of destructuring's use of JSOP_GETLOCAL, however, there may be
     * no such local. This could mean no blocks (no script objects at all, or
     * none of the script's object literals are blocks), or the stack slot i is
     * not in a block. In either case, return GetStr(ss, i).
     */
    JSScript *script = ss->printer->script;
    if (!JSScript::isValidOffset(script->objectsOffset))
        return GetStr(ss, i);

    for (jsatomid j = 0, n = script->objects()->length; j != n; j++) {
        JSObject *obj = script->getObject(j);
        if (obj->isBlock()) {
            jsint depth = OBJ_BLOCK_DEPTH(cx, obj);
            jsint count = OBJ_BLOCK_COUNT(cx, obj);

            if (jsuint(i - depth) < jsuint(count)) {
                jsint slot = i - depth;

                for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
                    const Shape &shape = r.front();

                    if (shape.shortid == slot) {
                        LOCAL_ASSERT(JSID_IS_ATOM(shape.propid));

                        JSAtom *atom = JSID_TO_ATOM(shape.propid);
                        const char *rval = QuoteString(&ss->sprinter, atom, 0);
                        if (!rval)
                            return NULL;

                        RETRACT(&ss->sprinter, rval);
                        return rval;
                    }
                }

                break;
            }
        }
    }

    return GetStr(ss, i);

#undef LOCAL_ASSERT
}

static JSBool
IsVarSlot(JSPrinter *jp, jsbytecode *pc, jsint *indexp)
{
    uintN slot;

    slot = GET_SLOTNO(pc);
    if (slot < jp->script->nfixed) {
        /* The slot refers to a variable with name stored in jp->localNames. */
        *indexp = jp->fun->nargs + slot;
        return JS_TRUE;
    }

    /* We have a local which index is relative to the stack base. */
    slot -= jp->script->nfixed;
    JS_ASSERT(slot < StackDepth(jp->script));
    *indexp = slot;
    return JS_FALSE;
}

#define LOAD_ATOM(PCOFF)                                                      \
    GET_ATOM_FROM_BYTECODE(jp->script, pc, PCOFF, atom)

#if JS_HAS_DESTRUCTURING

#define LOCAL_ASSERT(expr)  LOCAL_ASSERT_RV(expr, NULL)
#define LOAD_OP_DATA(pc)    (oplen = (cs = &js_CodeSpec[op=(JSOp)*pc])->length)

static jsbytecode *
DecompileDestructuring(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc);

static jsbytecode *
DecompileDestructuringLHS(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc,
                          JSBool *hole)
{
    JSContext *cx;
    JSPrinter *jp;
    JSOp op;
    const JSCodeSpec *cs;
    uintN oplen;
    jsint i;
    const char *lval, *xval;
    ptrdiff_t todo;
    JSAtom *atom;

    *hole = JS_FALSE;
    cx = ss->sprinter.context;
    jp = ss->printer;
    LOAD_OP_DATA(pc);

    switch (op) {
      case JSOP_POP:
        *hole = JS_TRUE;
        todo = SprintPut(&ss->sprinter, ", ", 2);
        break;

      case JSOP_DUP:
        pc = DecompileDestructuring(ss, pc, endpc);
        if (!pc)
            return NULL;
        if (pc == endpc)
            return pc;
        LOAD_OP_DATA(pc);
        lval = PopStr(ss, JSOP_NOP);
        todo = SprintCString(&ss->sprinter, lval);
        if (op == JSOP_POPN)
            return pc;
        LOCAL_ASSERT(*pc == JSOP_POP);
        break;

      case JSOP_SETARG:
      case JSOP_SETLOCAL:
        LOCAL_ASSERT(pc[oplen] == JSOP_POP || pc[oplen] == JSOP_POPN);
        /* FALL THROUGH */

      case JSOP_SETLOCALPOP:
        atom = NULL;
        lval = NULL;
        if (op == JSOP_SETARG) {
            atom = GetArgOrVarAtom(jp, GET_SLOTNO(pc));
            LOCAL_ASSERT(atom);
        } else if (IsVarSlot(jp, pc, &i)) {
            atom = GetArgOrVarAtom(jp, i);
            LOCAL_ASSERT(atom);
        } else {
            lval = GetLocal(ss, i);
        }
        {
            JSAutoByteString bytes;
            if (atom)
                lval = js_AtomToPrintableString(cx, atom, &bytes);
            LOCAL_ASSERT(lval);
            todo = SprintCString(&ss->sprinter, lval);
        }
        if (op != JSOP_SETLOCALPOP) {
            pc += oplen;
            if (pc == endpc)
                return pc;
            LOAD_OP_DATA(pc);
            if (op == JSOP_POPN)
                return pc;
            LOCAL_ASSERT(op == JSOP_POP);
        }
        break;

      default:
        /*
         * We may need to auto-parenthesize the left-most value decompiled
         * here, so add back PAREN_SLOP temporarily.  Then decompile until the
         * opcode that would reduce the stack depth to (ss->top-1), which we
         * pass to Decompile encoded as -(ss->top-1) - 1 or just -ss->top for
         * the nb parameter.
         */
        todo = ss->sprinter.offset;
        ss->sprinter.offset = todo + PAREN_SLOP;
        pc = Decompile(ss, pc, -((intN)ss->top), JSOP_NOP);
        if (!pc)
            return NULL;
        if (pc == endpc)
            return pc;
        LOAD_OP_DATA(pc);
        LOCAL_ASSERT(op == JSOP_ENUMELEM || op == JSOP_ENUMCONSTELEM);
        xval = PopStr(ss, JSOP_NOP);
        lval = PopStr(ss, JSOP_GETPROP);
        ss->sprinter.offset = todo;
        if (*lval == '\0') {
            /* lval is from JSOP_BINDNAME, so just print xval. */
            todo = SprintCString(&ss->sprinter, xval);
        } else if (*xval == '\0') {
            /* xval is from JSOP_SETCALL or JSOP_BINDXMLNAME, print lval. */
            todo = SprintCString(&ss->sprinter, lval);
        } else {
            todo = Sprint(&ss->sprinter,
                          (JOF_OPMODE(ss->opcodes[ss->top+1]) == JOF_XMLNAME)
                          ? "%s.%s"
                          : "%s[%s]",
                          lval, xval);
        }
        break;
    }

    if (todo < 0)
        return NULL;

    LOCAL_ASSERT(pc < endpc);
    pc += oplen;
    return pc;
}

/*
 * Starting with a SRC_DESTRUCT-annotated JSOP_DUP, decompile a destructuring
 * left-hand side object or array initialiser, including nested destructuring
 * initialisers.  On successful return, the decompilation will be pushed on ss
 * and the return value will point to the POP or GROUP bytecode following the
 * destructuring expression.
 *
 * At any point, if pc is equal to endpc and would otherwise advance, we stop
 * immediately and return endpc.
 */
static jsbytecode *
DecompileDestructuring(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc)
{
    ptrdiff_t head;
    JSContext *cx;
    JSPrinter *jp;
    JSOp op;
    const JSCodeSpec *cs;
    uintN oplen;
    jsint i, lasti;
    jsdouble d;
    const char *lval;
    JSAtom *atom;
    jssrcnote *sn;
    JSBool hole;

    LOCAL_ASSERT(*pc == JSOP_DUP);
    pc += JSOP_DUP_LENGTH;

    /*
     * Set head so we can rewrite '[' to '{' as needed.  Back up PAREN_SLOP
     * chars so the destructuring decompilation accumulates contiguously in
     * ss->sprinter starting with "[".
     */
    head = SprintPut(&ss->sprinter, "[", 1);
    if (head < 0 || !PushOff(ss, head, JSOP_NOP))
        return NULL;
    ss->sprinter.offset -= PAREN_SLOP;
    LOCAL_ASSERT(head == ss->sprinter.offset - 1);
    LOCAL_ASSERT(*OFF2STR(&ss->sprinter, head) == '[');

    cx = ss->sprinter.context;
    jp = ss->printer;
    lasti = -1;

    while (pc < endpc) {
#if JS_HAS_DESTRUCTURING_SHORTHAND
        ptrdiff_t nameoff = -1;
#endif

        LOAD_OP_DATA(pc);

        switch (op) {
          case JSOP_POP:
            pc += oplen;
            goto out;

          /* Handle the optimized number-pushing opcodes. */
          case JSOP_ZERO:   d = i = 0; goto do_getelem;
          case JSOP_ONE:    d = i = 1; goto do_getelem;
          case JSOP_UINT16: d = i = GET_UINT16(pc); goto do_getelem;
          case JSOP_UINT24: d = i = GET_UINT24(pc); goto do_getelem;
          case JSOP_INT8:   d = i = GET_INT8(pc);   goto do_getelem;
          case JSOP_INT32:  d = i = GET_INT32(pc);  goto do_getelem;

          case JSOP_DOUBLE:
            GET_DOUBLE_FROM_BYTECODE(jp->script, pc, 0, d);
            LOCAL_ASSERT(JSDOUBLE_IS_FINITE(d) && !JSDOUBLE_IS_NEGZERO(d));
            i = (jsint)d;

          do_getelem:
            sn = js_GetSrcNote(jp->script, pc);
            pc += oplen;
            if (pc == endpc)
                return pc;
            LOAD_OP_DATA(pc);
            LOCAL_ASSERT(op == JSOP_GETELEM);

            /* Distinguish object from array by opcode or source note. */
            if (sn && SN_TYPE(sn) == SRC_INITPROP) {
                *OFF2STR(&ss->sprinter, head) = '{';
                if (Sprint(&ss->sprinter, "%g: ", d) < 0)
                    return NULL;
            } else {
                /* Sanity check for the gnarly control flow above. */
                LOCAL_ASSERT(i == d);

                /* Fill in any holes (holes at the end don't matter). */
                while (++lasti < i) {
                    if (SprintPut(&ss->sprinter, ", ", 2) < 0)
                        return NULL;
                }
            }
            break;

          case JSOP_CALLPROP:
          case JSOP_GETPROP:
          case JSOP_LENGTH:
          {
            LOAD_ATOM(0);
            *OFF2STR(&ss->sprinter, head) = '{';
#if JS_HAS_DESTRUCTURING_SHORTHAND
            nameoff = ss->sprinter.offset;
#endif
            if (!QuoteString(&ss->sprinter, atom,
                             js_IsIdentifier(atom) ? 0 : (jschar)'\'')) {
                return NULL;
            }
            if (SprintPut(&ss->sprinter, ": ", 2) < 0)
                return NULL;
            break;
          }

          default:
            LOCAL_ASSERT(0);
        }

        pc += oplen;
        if (pc == endpc)
            return pc;

        /*
         * Decompile the left-hand side expression whose bytecode starts at pc
         * and continues for a bounded number of bytecodes or stack operations
         * (and which in any event stops before endpc).
         */
        pc = DecompileDestructuringLHS(ss, pc, endpc, &hole);
        if (!pc)
            return NULL;

#if JS_HAS_DESTRUCTURING_SHORTHAND
        if (nameoff >= 0) {
            ptrdiff_t offset, initlen;

            offset = ss->sprinter.offset;
            LOCAL_ASSERT(*OFF2STR(&ss->sprinter, offset) == '\0');
            initlen = offset - nameoff;
            LOCAL_ASSERT(initlen >= 4);

            /* Early check to rule out odd "name: lval" length. */
            if (((size_t)initlen & 1) == 0) {
                size_t namelen;
                const char *name;

                /*
                 * Even "name: lval" string length: check for "x: x" and the
                 * like, and apply the shorthand if we can.
                 */
                namelen = (size_t)(initlen - 2) >> 1;
                name = OFF2STR(&ss->sprinter, nameoff);
                if (!strncmp(name + namelen, ": ", 2) &&
                    !strncmp(name, name + namelen + 2, namelen)) {
                    offset -= namelen + 2;
                    *OFF2STR(&ss->sprinter, offset) = '\0';
                    ss->sprinter.offset = offset;
                }
            }
        }
#endif

        if (pc == endpc || *pc != JSOP_DUP)
            break;

        /*
         * We should stop if JSOP_DUP is either without notes or its note is
         * not SRC_CONTINUE. The former happens when JSOP_DUP duplicates the
         * last destructuring reference implementing an op= assignment like in
         * '([t] = z).y += x'. In the latter case the note is SRC_DESTRUCT and
         * means another destructuring initialiser abuts this one like in
         * '[a] = [b] = c'.
         */
        sn = js_GetSrcNote(jp->script, pc);
        if (!sn)
            break;
        if (SN_TYPE(sn) != SRC_CONTINUE) {
            LOCAL_ASSERT(SN_TYPE(sn) == SRC_DESTRUCT);
            break;
        }

        if (!hole && SprintPut(&ss->sprinter, ", ", 2) < 0)
            return NULL;

        pc += JSOP_DUP_LENGTH;
    }

out:
    lval = OFF2STR(&ss->sprinter, head);
    if (SprintPut(&ss->sprinter, (*lval == '[') ? "]" : "}", 1) < 0)
        return NULL;
    return pc;
}

static jsbytecode *
DecompileGroupAssignment(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc,
                         jssrcnote *sn, ptrdiff_t *todop)
{
    JSOp op;
    const JSCodeSpec *cs;
    uintN oplen, start, end, i;
    ptrdiff_t todo;
    JSBool hole;
    const char *rval;

    LOAD_OP_DATA(pc);
    LOCAL_ASSERT(op == JSOP_PUSH || op == JSOP_GETLOCAL);

    todo = Sprint(&ss->sprinter, "%s[", VarPrefix(sn));
    if (todo < 0 || !PushOff(ss, todo, JSOP_NOP))
        return NULL;
    ss->sprinter.offset -= PAREN_SLOP;

    for (;;) {
        pc += oplen;
        if (pc == endpc)
            return pc;
        pc = DecompileDestructuringLHS(ss, pc, endpc, &hole);
        if (!pc)
            return NULL;
        if (pc == endpc)
            return pc;
        LOAD_OP_DATA(pc);
        if (op != JSOP_PUSH && op != JSOP_GETLOCAL)
            break;
        if (!hole && SprintPut(&ss->sprinter, ", ", 2) < 0)
            return NULL;
    }

    LOCAL_ASSERT(op == JSOP_POPN);
    if (SprintPut(&ss->sprinter, "] = [", 5) < 0)
        return NULL;

    end = ss->top - 1;
    start = end - GET_UINT16(pc);
    for (i = start; i < end; i++) {
        rval = GetStr(ss, i);
        if (Sprint(&ss->sprinter,
                   (i == start) ? "%s" : ", %s",
                   (i == end - 1 && *rval == '\0') ? ", " : rval) < 0) {
            return NULL;
        }
    }

    if (SprintPut(&ss->sprinter, "]", 1) < 0)
        return NULL;
    ss->sprinter.offset = ss->offsets[i];
    ss->top = start;
    *todop = todo;
    return pc;
}

#undef LOCAL_ASSERT
#undef LOAD_OP_DATA

#endif /* JS_HAS_DESTRUCTURING */

static JSBool
InitSprintStack(JSContext *cx, SprintStack *ss, JSPrinter *jp, uintN depth)
{
    INIT_SPRINTER(cx, &ss->sprinter, &cx->tempLifoAlloc(), PAREN_SLOP);

    /* Allocate the parallel (to avoid padding) offset and opcode stacks. */
    size_t offsetsz = depth * sizeof(ptrdiff_t);
    size_t opcodesz = depth * sizeof(jsbytecode);
    void *space = cx->tempLifoAlloc().alloc(offsetsz + opcodesz);
    if (!space) {
        js_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    ss->offsets = (ptrdiff_t *) space;
    ss->opcodes = (jsbytecode *) ((char *)space + offsetsz);

    ss->top = ss->inArrayInit = 0;
    ss->inGenExp = JS_FALSE;
    ss->printer = jp;
    return JS_TRUE;
}

/*
 * If nb is non-negative, decompile nb bytecodes starting at pc.  Otherwise
 * the decompiler starts at pc and continues until it reaches an opcode for
 * which decompiling would result in the stack depth equaling -(nb + 1).
 *
 * The nextop parameter is either JSOP_NOP or the "next" opcode in order of
 * abstract interpretation (not necessarily physically next in a bytecode
 * vector). So nextop is JSOP_POP for the last operand in a comma expression,
 * or JSOP_AND for the right operand of &&.
 */
static jsbytecode *
Decompile(SprintStack *ss, jsbytecode *pc, intN nb, JSOp nextop)
{
    JSContext *cx;
    JSPrinter *jp, *jp2;
    jsbytecode *startpc, *endpc, *pc2, *done;
    ptrdiff_t tail, todo, len, oplen, cond, next;
    JSOp op, lastop, saveop;
    const JSCodeSpec *cs;
    jssrcnote *sn, *sn2;
    const char *lval, *rval, *xval, *fmt, *token;
    uintN nuses;
    jsint i, argc;
    char **argv;
    JSAtom *atom;
    JSObject *obj;
    JSFunction *fun = NULL; /* init to shut GCC up */
    JSString *str;
    JSBool ok;
#if JS_HAS_XML_SUPPORT
    JSBool foreach, inXML, quoteAttr;
#else
#define inXML JS_FALSE
#endif
    jsval val;

    static const char exception_cookie[] = "/*EXCEPTION*/";
    static const char retsub_pc_cookie[] = "/*RETSUB_PC*/";
    static const char forelem_cookie[]   = "/*FORELEM*/";
    static const char with_cookie[]      = "/*WITH*/";
    static const char dot_format[]       = "%s.%s";
    static const char index_format[]     = "%s[%s]";
    static const char predot_format[]    = "%s%s.%s";
    static const char postdot_format[]   = "%s.%s%s";
    static const char preindex_format[]  = "%s%s[%s]";
    static const char postindex_format[] = "%s[%s]%s";
    static const char ss_format[]        = "%s%s";
    static const char sss_format[]       = "%s%s%s";

    /* Argument and variables decompilation uses the following to share code. */
    JS_STATIC_ASSERT(ARGNO_LEN == SLOTNO_LEN);

/*
 * Local macros
 */
#define LOCAL_ASSERT(expr)    LOCAL_ASSERT_RV(expr, NULL)
#define DECOMPILE_CODE_CLEANUP(pc,nb,cleanup) if (!Decompile(ss, pc, nb, JSOP_NOP)) cleanup
#define DECOMPILE_CODE(pc,nb) DECOMPILE_CODE_CLEANUP(pc,nb,return NULL)
#define NEXT_OP(pc)           (((pc) + (len) == endpc) ? nextop : pc[len])
#define TOP_STR()             GetStr(ss, ss->top - 1)
#define POP_STR()             PopStr(ss, op)
#define POP_STR_PREC(prec)    PopStrPrec(ss, prec)

/*
 * Pop a condition expression for if/while. JSOP_IFEQ's precedence forces
 * extra parens around assignment, which avoids a strict-mode warning.
 */
#define POP_COND_STR()                                                        \
    PopStr(ss, (js_CodeSpec[ss->opcodes[ss->top - 1]].format & JOF_SET)       \
               ? JSOP_IFEQ                                                    \
               : JSOP_NOP)

#define ATOM_IS_IDENTIFIER(atom) js_IsIdentifier(atom)

/*
 * Given an atom already fetched from jp->script's atom map, quote/escape its
 * string appropriately into rval, and select fmt from the quoted and unquoted
 * alternatives.
 */
#define GET_QUOTE_AND_FMT(qfmt, ufmt, rval)                                   \
    JS_BEGIN_MACRO                                                            \
        jschar quote_;                                                        \
        if (!ATOM_IS_IDENTIFIER(atom)) {                                      \
            quote_ = '\'';                                                    \
            fmt = qfmt;                                                       \
        } else {                                                              \
            quote_ = 0;                                                       \
            fmt = ufmt;                                                       \
        }                                                                     \
        rval = QuoteString(&ss->sprinter, atom, quote_);                      \
        if (!rval)                                                            \
            return NULL;                                                      \
    JS_END_MACRO

#define LOAD_OBJECT(PCOFF)                                                    \
    GET_OBJECT_FROM_BYTECODE(jp->script, pc, PCOFF, obj)

#define LOAD_FUNCTION(PCOFF)                                                  \
    GET_FUNCTION_FROM_BYTECODE(jp->script, pc, PCOFF, fun)

#define LOAD_REGEXP(PCOFF)                                                    \
    GET_REGEXP_FROM_BYTECODE(jp->script, pc, PCOFF, obj)

#define GET_SOURCE_NOTE_ATOM(sn, atom)                                        \
    JS_BEGIN_MACRO                                                            \
        jsatomid atomIndex_ = (jsatomid) js_GetSrcNoteOffset((sn), 0);        \
                                                                              \
        LOCAL_ASSERT(atomIndex_ < jp->script->natoms);                        \
        (atom) = jp->script->atoms[atomIndex_];                               \
    JS_END_MACRO

/*
 * Get atom from jp->script's atom map, quote/escape its string appropriately
 * into rval, and select fmt from the quoted and unquoted alternatives.
 */
#define GET_ATOM_QUOTE_AND_FMT(qfmt, ufmt, rval)                              \
    JS_BEGIN_MACRO                                                            \
        LOAD_ATOM(0);                                                         \
        GET_QUOTE_AND_FMT(qfmt, ufmt, rval);                                  \
    JS_END_MACRO

/*
 * Per spec, new x(y).z means (new x(y))).z. For example new (x(y).z) must
 * decompile with the constructor parenthesized, but new x.z should not. The
 * normal rules give x(y).z and x.z identical precedence: both are produced by
 * JSOP_GETPROP.
 *
 * Therefore, we need to know in case JSOP_NEW whether the constructor
 * expression contains any unparenthesized function calls. So when building a
 * MemberExpression or CallExpression, we set ss->opcodes[n] to JSOP_CALL if
 * this is true. x(y).z gets JSOP_CALL, not JSOP_GETPROP.
 */
#define PROPAGATE_CALLNESS()                                                  \
    JS_BEGIN_MACRO                                                            \
        if (ss->opcodes[ss->top - 1] == JSOP_CALL ||                          \
            ss->opcodes[ss->top - 1] == JSOP_EVAL ||                          \
            ss->opcodes[ss->top - 1] == JSOP_FUNCALL ||                       \
            ss->opcodes[ss->top - 1] == JSOP_FUNAPPLY) {                      \
            saveop = JSOP_CALL;                                               \
        }                                                                     \
    JS_END_MACRO

    cx = ss->sprinter.context;
    JS_CHECK_RECURSION(cx, return NULL);

    jp = ss->printer;
    startpc = pc;
    endpc = (nb < 0) ? jp->script->code + jp->script->length : pc + nb;
    tail = -1;
    todo = -2;                  /* NB: different from Sprint() error return. */
    saveop = JSOP_NOP;
    sn = NULL;
    rval = NULL;
#if JS_HAS_XML_SUPPORT
    foreach = inXML = quoteAttr = JS_FALSE;
#endif

    while (nb < 0 || pc < endpc) {
        /*
         * Move saveop to lastop so prefixed bytecodes can take special action
         * while sharing maximal code.  Set op and saveop to the new bytecode,
         * use op in POP_STR to trigger automatic parenthesization, but push
         * saveop at the bottom of the loop if this op pushes.  Thus op may be
         * set to nop or otherwise mutated to suppress auto-parens.
         */
        lastop = saveop;
        op = (JSOp) *pc;
        cs = &js_CodeSpec[op];
        if (cs->format & JOF_INDEXBASE) {
            /*
             * The decompiler uses js_GetIndexFromBytecode to get atoms and
             * objects and ignores these suffix/prefix bytecodes, thus
             * simplifying code that must process JSOP_GETTER/JSOP_SETTER
             * prefixes.
             */
            pc += cs->length;
            if (pc >= endpc)
                break;
            op = (JSOp) *pc;
            cs = &js_CodeSpec[op];
        }
        saveop = op;
        len = oplen = cs->length;
        nuses = js_GetStackUses(cs, op, pc);

        /*
         * Here it is possible that nuses > ss->top when the op has a hidden
         * source note. But when nb < 0 we assume that the caller knows that
         * Decompile would never meet such opcodes.
         */
        if (nb < 0) {
            LOCAL_ASSERT(ss->top >= nuses);
            uintN ndefs = js_GetStackDefs(cx, cs, op, jp->script, pc);
            if ((uintN) -(nb + 1) == ss->top - nuses + ndefs)
                return pc;
        }

        /*
         * Save source literal associated with JS now before the following
         * rewrite changes op. See bug 380197.
         */
        token = CodeToken[op];

        if (pc + oplen == jp->dvgfence) {
            /*
             * Rewrite non-get ops to their "get" format if the error is in
             * the bytecode at pc, so we don't decompile more than the error
             * expression.
             */
            StackFrame *fp = js_GetScriptedCaller(cx, NULL);
            uint32 format = cs->format;
            if (((fp && pc == fp->pcQuadratic(cx)) ||
                 (pc == startpc && nuses != 0)) &&
                format & (JOF_SET|JOF_DEL|JOF_INCDEC|JOF_FOR|JOF_VARPROP)) {
                uint32 mode = JOF_MODE(format);
                if (mode == JOF_NAME) {
                    /*
                     * JOF_NAME does not imply JOF_ATOM, so we must check for
                     * the QARG and QVAR format types, and translate those to
                     * JSOP_GETARG or JSOP_GETLOCAL appropriately, instead of
                     * to JSOP_NAME.
                     */
                    uint32 type = JOF_TYPE(format);
                    op = (type == JOF_QARG)
                         ? JSOP_GETARG
                         : (type == JOF_LOCAL)
                         ? JSOP_GETLOCAL
                         : JSOP_NAME;

                    JS_ASSERT(js_CodeSpec[op].nuses >= 0);
                    i = nuses - js_CodeSpec[op].nuses;
                    while (--i >= 0)
                        PopOff(ss, JSOP_NOP);
                } else {
                    /*
                     * We must replace the faulting pc's bytecode with a
                     * corresponding JSOP_GET* code.  For JSOP_SET{PROP,ELEM},
                     * we must use the "2nd" form of JSOP_GET{PROP,ELEM}, to
                     * throw away the assignment op's right-hand operand and
                     * decompile it as if it were a GET of its left-hand
                     * operand.
                     */
                    if (mode == JOF_PROP) {
                        op = (JSOp) ((format & JOF_SET)
                                     ? JSOP_GETPROP2
                                     : JSOP_GETPROP);
                    } else if (mode == JOF_ELEM) {
                        op = (JSOp) ((format & JOF_SET)
                                     ? JSOP_GETELEM2
                                     : JSOP_GETELEM);
                    } else {
                        /*
                         * Unknown mode (including mode 0) means that op is
                         * uncategorized for our purposes, so we must write
                         * per-op special case code here.
                         */
                        switch (op) {
                          case JSOP_ENUMELEM:
                          case JSOP_ENUMCONSTELEM:
                            op = JSOP_GETELEM;
                            break;
                          case JSOP_SETXMLNAME:
                            op = JSOp(JSOP_GETELEM2);
                            break;
                          default:
                            LOCAL_ASSERT(0);
                        }
                    }
                }
            }

            saveop = op;
            if (op >= JSOP_LIMIT) {
                if (op == JSOP_GETPROP2)
                    saveop = JSOP_GETPROP;
                else if (op == JSOP_GETELEM2)
                    saveop = JSOP_GETELEM;
            }

            jp->dvgfence = NULL;
        }

        if (token) {
            switch (nuses) {
              case 2:
                sn = js_GetSrcNote(jp->script, pc);
                if (sn && SN_TYPE(sn) == SRC_ASSIGNOP) {
                    /*
                     * Avoid over-parenthesizing y in x op= y based on its
                     * expansion: x = x op y (replace y by z = w to see the
                     * problem).
                     */
                    op = (JSOp) pc[oplen];
                    rval = POP_STR();
                    lval = POP_STR();
                    /* Print only the right operand of the assignment-op. */
                    todo = SprintCString(&ss->sprinter, rval);
                    op = saveop;
                } else if (!inXML) {
                    rval = POP_STR_PREC(cs->prec + !!(cs->format & JOF_LEFTASSOC));
                    lval = POP_STR_PREC(cs->prec + !(cs->format & JOF_LEFTASSOC));
                    todo = Sprint(&ss->sprinter, "%s %s %s",
                                  lval, token, rval);
                } else {
                    /* In XML, just concatenate the two operands. */
                    LOCAL_ASSERT(op == JSOP_ADD);
                    rval = POP_STR();
                    lval = POP_STR();
                    todo = Sprint(&ss->sprinter, ss_format, lval, rval);
                }
                break;

              case 1:
                rval = POP_STR();
                todo = Sprint(&ss->sprinter, ss_format, token, rval);
                break;

              case 0:
                todo = SprintCString(&ss->sprinter, token);
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
                  case SRC_WHILE:
                    /* First instuction (NOP) contains offset to condition */
                    ++pc;
                    /* Second instruction (TRACE) contains offset to JSOP_IFNE */
                    sn = js_GetSrcNote(jp->script, pc);
                    tail = js_GetSrcNoteOffset(sn, 0);
                    LOCAL_ASSERT(pc[tail] == JSOP_IFNE ||
                                 pc[tail] == JSOP_IFNEX);
                    js_printf(jp, "\tdo {\n");
                    jp->indent += 4;
                    DECOMPILE_CODE(pc, tail);
                    jp->indent -= 4;
                    js_printf(jp, "\t} while (%s);\n", POP_COND_STR());
                    pc += tail;
                    len = js_CodeSpec[*pc].length;
                    todo = -2;
                    break;

                  case SRC_FOR:
                    rval = "";

                  do_forloop:
                    JS_ASSERT(SN_TYPE(sn) == SRC_FOR);

                    /* Skip the JSOP_NOP or JSOP_POP bytecode. */
                    pc += JSOP_NOP_LENGTH;

                    /* Get the cond, next, and loop-closing tail offsets. */
                    cond = js_GetSrcNoteOffset(sn, 0);
                    next = js_GetSrcNoteOffset(sn, 1);
                    tail = js_GetSrcNoteOffset(sn, 2);

                    /*
                     * If this loop has a condition, then pc points at a goto
                     * targeting the condition.
                     */
                    pc2 = pc;
                    if (cond != tail) {
                        LOCAL_ASSERT(*pc == JSOP_GOTO || *pc == JSOP_GOTOX);
                        pc2 += (*pc == JSOP_GOTO) ? JSOP_GOTO_LENGTH : JSOP_GOTOX_LENGTH;
                    }
                    LOCAL_ASSERT(tail + GetJumpOffset(pc+tail, pc+tail) == pc2 - pc);

                    /* Print the keyword and the possibly empty init-part. */
                    js_printf(jp, "\tfor (%s;", rval);

                    if (cond != tail) {
                        /* Decompile the loop condition. */
                        DECOMPILE_CODE(pc + cond, tail - cond);
                        js_printf(jp, " %s", POP_STR());
                    }

                    /* Need a semicolon whether or not there was a cond. */
                    js_puts(jp, ";");

                    if (next != cond) {
                        /*
                         * Decompile the loop updater. It may end in a JSOP_POP
                         * that we skip; or in a JSOP_POPN that we do not skip,
                         * followed by a JSOP_NOP (skipped as if it's a POP).
                         * We cope with the difference between these two cases
                         * by checking for stack imbalance and popping if there
                         * is an rval.
                         */
                        uintN saveTop = ss->top;

                        DECOMPILE_CODE(pc + next, cond - next - JSOP_POP_LENGTH);
                        LOCAL_ASSERT(ss->top - saveTop <= 1U);
                        rval = (ss->top == saveTop)
                               ? ss->sprinter.base + ss->sprinter.offset
                               : POP_STR();
                        js_printf(jp, " %s", rval);
                    }

                    /* Do the loop body. */
                    js_printf(jp, ") {\n");
                    jp->indent += 4;
                    next -= pc2 - pc;
                    DECOMPILE_CODE(pc2, next);
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");

                    /* Set len so pc skips over the entire loop. */
                    len = tail + js_CodeSpec[pc[tail]].length;
                    break;

                  case SRC_LABEL:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    jp->indent -= 4;
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    RETRACT(&ss->sprinter, rval);
                    js_printf(jp, "\t%s:\n", rval);
                    jp->indent += 4;
                    break;

                  case SRC_LABELBRACE:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    RETRACT(&ss->sprinter, rval);
                    js_printf(jp, "\t%s: {\n", rval);
                    jp->indent += 4;
                    break;

                  case SRC_ENDBRACE:
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");
                    break;

                  case SRC_FUNCDEF:
                    fun = jp->script->getFunction(js_GetSrcNoteOffset(sn, 0));
                  do_function:
                    js_puts(jp, "\n");
                    jp2 = js_NewPrinter(cx, "nested_function", fun,
                                        jp->indent, jp->pretty, jp->grouped,
                                        jp->strict);
                    if (!jp2)
                        return NULL;
                    ok = js_DecompileFunction(jp2);
                    if (ok && jp2->sprinter.base)
                        js_puts(jp, jp2->sprinter.base);
                    js_DestroyPrinter(jp2);
                    if (!ok)
                        return NULL;
                    js_puts(jp, "\n\n");
                    break;

                  case SRC_BRACE:
                    js_printf(jp, "\t{\n");
                    jp->indent += 4;
                    len = js_GetSrcNoteOffset(sn, 0);
                    DECOMPILE_CODE(pc + oplen, len - oplen);
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");
                    break;

                  default:;
                }
                break;

              case JSOP_PUSH:
#if JS_HAS_DESTRUCTURING
                sn = js_GetSrcNote(jp->script, pc);
                if (sn && SN_TYPE(sn) == SRC_GROUPASSIGN) {
                    pc = DecompileGroupAssignment(ss, pc, endpc, sn, &todo);
                    if (!pc)
                        return NULL;
                    LOCAL_ASSERT(*pc == JSOP_POPN);
                    len = oplen = JSOP_POPN_LENGTH;
                    goto end_groupassignment;
                }
#endif
                /* FALL THROUGH */

              case JSOP_BINDNAME:
              case JSOP_BINDGNAME:
                todo = Sprint(&ss->sprinter, "");
                break;

              case JSOP_TRY:
                js_printf(jp, "\ttry {\n");
                jp->indent += 4;
                todo = -2;
                break;

              case JSOP_FINALLY:
                jp->indent -= 4;
                js_printf(jp, "\t} finally {\n");
                jp->indent += 4;

                /*
                 * We push push the pair of exception/restsub cookies to
                 * simulate the effects [gosub] or control transfer during
                 * exception capturing on the stack.
                 */
                todo = Sprint(&ss->sprinter, exception_cookie);
                if (todo < 0 || !PushOff(ss, todo, op))
                    return NULL;
                todo = Sprint(&ss->sprinter, retsub_pc_cookie);
                break;

              case JSOP_RETSUB:
                rval = POP_STR();
                LOCAL_ASSERT(strcmp(rval, retsub_pc_cookie) == 0);
                lval = POP_STR();
                LOCAL_ASSERT(strcmp(lval, exception_cookie) == 0);
                todo = -2;
                break;

              case JSOP_GOSUB:
              case JSOP_GOSUBX:
                /*
                 * JSOP_GOSUB and GOSUBX have no effect on the decompiler's
                 * string stack because the next op in bytecode order finds
                 * the stack balanced by a JSOP_RETSUB executed elsewhere.
                 */
                todo = -2;
                break;

              case JSOP_POPN:
              {
                uintN newtop, oldtop;

                /*
                 * The compiler models operand stack depth and fixes the stack
                 * pointer on entry to a catch clause based on its depth model.
                 * The decompiler must match the code generator's model, which
                 * is why JSOP_FINALLY pushes a cookie that JSOP_RETSUB pops.
                 */
                oldtop = ss->top;
                newtop = oldtop - GET_UINT16(pc);
                LOCAL_ASSERT(newtop <= oldtop);
                todo = -2;

                sn = js_GetSrcNote(jp->script, pc);
                if (sn && SN_TYPE(sn) == SRC_HIDDEN)
                    break;
#if JS_HAS_DESTRUCTURING
                if (sn && SN_TYPE(sn) == SRC_GROUPASSIGN) {
                    todo = Sprint(&ss->sprinter, "%s[] = [",
                                  VarPrefix(sn));
                    if (todo < 0)
                        return NULL;
                    for (uintN i = newtop; i < oldtop; i++) {
                        rval = OFF2STR(&ss->sprinter, ss->offsets[i]);
                        if (Sprint(&ss->sprinter, ss_format,
                                   (i == newtop) ? "" : ", ",
                                   (i == oldtop - 1 && *rval == '\0')
                                   ? ", " : rval) < 0) {
                            return NULL;
                        }
                    }
                    if (SprintPut(&ss->sprinter, "]", 1) < 0)
                        return NULL;

                    /*
                     * If this is an empty group assignment, we have no stack
                     * budget into which we can push our result string. Adjust
                     * ss->sprinter.offset so that our consumer can find the
                     * empty group assignment decompilation.
                     */
                    if (newtop == oldtop) {
                        ss->sprinter.offset = todo;
                    } else {
                        /*
                         * Kill newtop before the end_groupassignment: label by
                         * retracting/popping early.  Control will either jump
                         * to do_forloop: or do_letheadbody: or else break from
                         * our case JSOP_POPN: after the switch (*pc2) below.
                         */
                        LOCAL_ASSERT(newtop < oldtop);
                        ss->sprinter.offset = GetOff(ss, newtop);
                        ss->top = newtop;
                    }

                  end_groupassignment:
                    LOCAL_ASSERT(*pc == JSOP_POPN);

                    /*
                     * Thread directly to the next opcode if we can, to handle
                     * the special cases of a group assignment in the first or
                     * last part of a for(;;) loop head, or in a let block or
                     * expression head.
                     *
                     * NB: todo at this point indexes space in ss->sprinter
                     * that is liable to be overwritten.  The code below knows
                     * exactly how long rval lives, or else copies it down via
                     * SprintCString.
                     */
                    rval = OFF2STR(&ss->sprinter, todo);
                    todo = -2;
                    pc2 = pc + oplen;

                    /* Skip a block chain annotation if one appears here. */
                    if (*pc2 == JSOP_NOP) {
                        if (pc2[JSOP_NOP_LENGTH] == JSOP_NULLBLOCKCHAIN)
                            pc2 += JSOP_NOP_LENGTH + JSOP_NULLBLOCKCHAIN_LENGTH;
                        else if (pc2[JSOP_NOP_LENGTH] == JSOP_BLOCKCHAIN)
                            pc2 += JSOP_NOP_LENGTH + JSOP_BLOCKCHAIN_LENGTH;
                    }

                    if (*pc2 == JSOP_NOP) {
                        sn = js_GetSrcNote(jp->script, pc2);
                        if (sn) {
                            if (SN_TYPE(sn) == SRC_FOR) {
                                op = JSOP_NOP;
                                pc = pc2;
                                goto do_forloop;
                            }

                            if (SN_TYPE(sn) == SRC_DECL) {
                                if (ss->top == StackDepth(jp->script)) {
                                    /*
                                     * This must be an empty destructuring
                                     * in the head of a let whose body block
                                     * is also empty.
                                     */
                                    pc = pc2 + JSOP_NOP_LENGTH;
                                    len = js_GetSrcNoteOffset(sn, 0);
                                    LOCAL_ASSERT(pc[len] == JSOP_LEAVEBLOCK);
                                    js_printf(jp, "\tlet (%s) {\n", rval);
                                    js_printf(jp, "\t}\n");
                                    break;
                                }
                                todo = SprintCString(&ss->sprinter, rval);
                                if (todo < 0 || !PushOff(ss, todo, JSOP_NOP))
                                    return NULL;
                                op = JSOP_POP;
                                pc = pc2 + JSOP_NOP_LENGTH;
                                goto do_letheadbody;
                            }
                        } else {
                            /*
                             * An unnannotated NOP following a POPN must be the
                             * third part of for(;;) loop head. If the POPN's
                             * immediate operand is 0, then we may have no slot
                             * on the sprint-stack in which to push our result
                             * string. In this case the result can be recovered
                             * at ss->sprinter.base + ss->sprinter.offset.
                             */
                            if (GET_UINT16(pc) == 0)
                                break;
                            todo = SprintCString(&ss->sprinter, rval);
                            saveop = JSOP_NOP;
                        }
                    }

                    /*
                     * If control flow reaches this point with todo still -2,
                     * just print rval as an expression statement.
                     */
                    if (todo == -2)
                        js_printf(jp, "\t%s;\n", rval);
                    break;
                }
#endif
                if (newtop < oldtop) {
                    ss->sprinter.offset = GetOff(ss, newtop);
                    ss->top = newtop;
                }
                break;
              }

              case JSOP_EXCEPTION:
                /* The catch decompiler handles this op itself. */
                LOCAL_ASSERT(JS_FALSE);
                break;

              case JSOP_POP:
                /*
                 * By default, do not automatically parenthesize when popping
                 * a stacked expression decompilation.  We auto-parenthesize
                 * only when JSOP_POP is annotated with SRC_PCDELTA, meaning
                 * comma operator.
                 */
                op = JSOP_POPV;
                /* FALL THROUGH */

              case JSOP_POPV:
                sn = js_GetSrcNote(jp->script, pc);
                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
                  case SRC_FOR:
                    /* Force parens around 'in' expression at 'for' front. */
                    if (ss->opcodes[ss->top-1] == JSOP_IN)
                        op = JSOP_LSH;
                    rval = POP_STR();
                    todo = -2;
                    goto do_forloop;

                  case SRC_PCDELTA:
                    /* Comma operator: use JSOP_POP for correct precedence. */
                    op = JSOP_POP;

                    /* Pop and save to avoid blowing stack depth budget. */
                    lval = JS_strdup(cx, POP_STR());
                    if (!lval)
                        return NULL;

                    /*
                     * The offset tells distance to the end of the right-hand
                     * operand of the comma operator.
                     */
                    done = pc + len;
                    pc += js_GetSrcNoteOffset(sn, 0);
                    len = 0;

                    if (!Decompile(ss, done, pc - done, JSOP_POP)) {
                        cx->free_((char *)lval);
                        return NULL;
                    }

                    /* Pop Decompile result and print comma expression. */
                    rval = POP_STR();
                    todo = Sprint(&ss->sprinter, "%s, %s", lval, rval);
                    cx->free_((char *)lval);
                    break;

                  case SRC_HIDDEN:
                    /* Hide this pop, it's from a goto in a with or for/in. */
                    todo = -2;
                    break;

                  case SRC_DECL:
                    /* This pop is at the end of the let block/expr head. */
                    pc += JSOP_POP_LENGTH;
#if JS_HAS_DESTRUCTURING
                  do_letheadbody:
#endif
                    len = js_GetSrcNoteOffset(sn, 0);
                    if (pc[len] == JSOP_LEAVEBLOCK) {
                        js_printf(jp, "\tlet (%s) {\n", POP_STR());
                        jp->indent += 4;
                        DECOMPILE_CODE(pc, len);
                        jp->indent -= 4;
                        js_printf(jp, "\t}\n");
                        todo = -2;
                    } else {
                        LOCAL_ASSERT(pc[len] == JSOP_LEAVEBLOCKEXPR);

                        lval = JS_strdup(cx, PopStr(ss, JSOP_NOP));
                        if (!lval)
                            return NULL;

                        /* Set saveop to reflect what we will push. */
                        saveop = JSOP_LEAVEBLOCKEXPR;
                        if (!Decompile(ss, pc, len, saveop)) {
                            cx->free_((char *)lval);
                            return NULL;
                        }
                        rval = PopStr(ss, JSOP_SETNAME);
                        todo = Sprint(&ss->sprinter,
                                      (*rval == '{')
                                      ? "let (%s) (%s)"
                                      : "let (%s) %s",
                                      lval, rval);
                        cx->free_((char *)lval);
                    }
                    break;

                  default:
                    /* Turn off parens around a yield statement. */
                    if (ss->opcodes[ss->top-1] == JSOP_YIELD)
                        op = JSOP_NOP;

                    rval = POP_STR();

                    /*
                     * Don't emit decompiler-pushed strings that are not
                     * handled by other opcodes. They are pushed onto the
                     * stack to help model the interpreter stack and should
                     * not appear in the decompiler's output.
                     */
                    if (*rval != '\0' && (rval[0] != '/' || rval[1] != '*')) {
                        js_printf(jp,
                                  (*rval == '{' ||
                                   (strncmp(rval, js_function_str, 8) == 0 &&
                                    rval[8] == ' '))
                                  ? "\t(%s);\n"
                                  : "\t%s;\n",
                                  rval);
                    } else {
                        LOCAL_ASSERT(*rval == '\0' ||
                                     strcmp(rval, exception_cookie) == 0);
                    }
                    todo = -2;
                    break;
                }
                sn = NULL;
                break;

              case JSOP_ENTERWITH:
                LOCAL_ASSERT(!js_GetSrcNote(jp->script, pc));
                rval = POP_STR();
                js_printf(jp, "\twith (%s) {\n", rval);
                jp->indent += 4;
                todo = Sprint(&ss->sprinter, with_cookie);
                break;

              case JSOP_LEAVEWITH:
                sn = js_GetSrcNote(jp->script, pc);
                todo = -2;
                if (sn && SN_TYPE(sn) == SRC_HIDDEN)
                    break;
                rval = POP_STR();
                LOCAL_ASSERT(strcmp(rval, with_cookie) == 0);
                jp->indent -= 4;
                js_printf(jp, "\t}\n");
                break;

              case JSOP_ENTERBLOCK:
              {
                JSAtom **atomv, *smallv[5];

                LOAD_OBJECT(0);
                argc = OBJ_BLOCK_COUNT(cx, obj);
                if ((size_t)argc <= JS_ARRAY_LENGTH(smallv)) {
                    atomv = smallv;
                } else {
                    atomv = (JSAtom **) cx->malloc_(argc * sizeof(JSAtom *));
                    if (!atomv)
                        return NULL;
                }

                MUST_FLOW_THROUGH("enterblock_out");
#define LOCAL_ASSERT_OUT(expr) LOCAL_ASSERT_CUSTOM(expr, ok = JS_FALSE; \
                                                   goto enterblock_out)
                for (Shape::Range r = obj->lastProperty()->all(); !r.empty(); r.popFront()) {
                    const Shape &shape = r.front();

                    if (!shape.hasShortID())
                        continue;
                    LOCAL_ASSERT_OUT(shape.shortid < argc);
                    atomv[shape.shortid] = JSID_TO_ATOM(shape.propid);
                }
                ok = JS_TRUE;
                for (i = 0; i < argc; i++) {
                    atom = atomv[i];
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval ||
                        !PushOff(ss, STR2OFF(&ss->sprinter, rval), op)) {
                        ok = JS_FALSE;
                        goto enterblock_out;
                    }
                }

                sn = js_GetSrcNote(jp->script, pc);
                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
#if JS_HAS_BLOCK_SCOPE
                  case SRC_BRACE:
                    js_printf(jp, "\t{\n");
                    jp->indent += 4;
                    len = js_GetSrcNoteOffset(sn, 0);
                    ok = Decompile(ss, pc + oplen, len - oplen, JSOP_NOP)
                         != NULL;
                    if (!ok)
                        goto enterblock_out;
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");
                    break;
#endif

                  case SRC_CATCH:
                    jp->indent -= 4;
                    js_printf(jp, "\t} catch (");

                    pc2 = pc;
                    pc += oplen;
                    LOCAL_ASSERT_OUT(*pc == JSOP_EXCEPTION);
                    pc += JSOP_EXCEPTION_LENGTH;
                    todo = Sprint(&ss->sprinter, exception_cookie);
                    if (todo < 0 || !PushOff(ss, todo, JSOP_EXCEPTION)) {
                        ok = JS_FALSE;
                        goto enterblock_out;
                    }

                    if (*pc == JSOP_DUP) {
                        sn2 = js_GetSrcNote(jp->script, pc);
                        if (!sn2 || SN_TYPE(sn2) != SRC_DESTRUCT) {
                            /*
                             * This is a dup to save the exception for later.
                             * It is emitted only when the catch head contains
                             * an exception guard.
                             */
                            LOCAL_ASSERT_OUT(js_GetSrcNoteOffset(sn, 0) != 0);
                            pc += JSOP_DUP_LENGTH;
                            todo = Sprint(&ss->sprinter, exception_cookie);
                            if (todo < 0 ||
                                !PushOff(ss, todo, JSOP_EXCEPTION)) {
                                ok = JS_FALSE;
                                goto enterblock_out;
                            }
                        }
                    }

#if JS_HAS_DESTRUCTURING
                    if (*pc == JSOP_DUP) {
                        pc = DecompileDestructuring(ss, pc, endpc);
                        if (!pc) {
                            ok = JS_FALSE;
                            goto enterblock_out;
                        }
                        LOCAL_ASSERT_OUT(*pc == JSOP_POP);
                        pc += JSOP_POP_LENGTH;
                        lval = PopStr(ss, JSOP_NOP);
                        js_puts(jp, lval);
                    } else {
#endif
                        LOCAL_ASSERT_OUT(*pc == JSOP_SETLOCALPOP);
                        i = GET_SLOTNO(pc) - jp->script->nfixed;
                        pc += JSOP_SETLOCALPOP_LENGTH;
                        atom = atomv[i - OBJ_BLOCK_DEPTH(cx, obj)];
                        str = atom;
                        if (!QuoteString(&jp->sprinter, str, 0)) {
                            ok = JS_FALSE;
                            goto enterblock_out;
                        }
#if JS_HAS_DESTRUCTURING
                    }
#endif

                    /*
                     * Pop the exception_cookie (or its dup in the case of a
                     * guarded catch head) off the stack now.
                     */
                    rval = PopStr(ss, JSOP_NOP);
                    LOCAL_ASSERT_OUT(strcmp(rval, exception_cookie) == 0);

                    len = js_GetSrcNoteOffset(sn, 0);
                    if (len) {
                        len -= pc - pc2;
                        LOCAL_ASSERT_OUT(len > 0);
                        js_printf(jp, " if ");
                        ok = Decompile(ss, pc, len, JSOP_NOP) != NULL;
                        if (!ok)
                            goto enterblock_out;
                        js_printf(jp, "%s", POP_STR());
                        pc += len;
                        LOCAL_ASSERT_OUT(*pc == JSOP_IFEQ || *pc == JSOP_IFEQX);
                        pc += js_CodeSpec[*pc].length;
                    }

                    js_printf(jp, ") {\n");
                    jp->indent += 4;
                    len = 0;
                    break;
                  default:
                    break;
                }

                todo = -2;

#undef LOCAL_ASSERT_OUT
              enterblock_out:
                if (atomv != smallv)
                    cx->free_(atomv);
                if (!ok)
                    return NULL;
              }
              break;

              case JSOP_LEAVEBLOCK:
              case JSOP_LEAVEBLOCKEXPR:
              {
                uintN top, depth;

                sn = js_GetSrcNote(jp->script, pc);
                todo = -2;
                if (op == JSOP_LEAVEBLOCKEXPR) {
                    LOCAL_ASSERT(SN_TYPE(sn) == SRC_PCBASE);
                    rval = POP_STR();
                } else if (sn) {
                    LOCAL_ASSERT(op == JSOP_LEAVEBLOCK);
                    if (SN_TYPE(sn) == SRC_HIDDEN)
                        break;

                    /*
                     * This JSOP_LEAVEBLOCK must be for a catch block. If sn's
                     * offset does not equal the model stack depth, there must
                     * be a copy of the exception value on the stack due to a
                     * catch guard (see above, the JSOP_ENTERBLOCK + SRC_CATCH
                     * case code).
                     */
                    LOCAL_ASSERT(SN_TYPE(sn) == SRC_CATCH);
                    if ((uintN)js_GetSrcNoteOffset(sn, 0) != ss->top) {
                        LOCAL_ASSERT((uintN)js_GetSrcNoteOffset(sn, 0)
                                     == ss->top - 1);
                        rval = POP_STR();
                        LOCAL_ASSERT(strcmp(rval, exception_cookie) == 0);
                    }
                }
                top = ss->top;
                depth = GET_UINT16(pc);
                LOCAL_ASSERT(top >= depth);
                top -= depth;
                ss->top = top;
                ss->sprinter.offset = GetOff(ss, top);
                if (op == JSOP_LEAVEBLOCKEXPR)
                    todo = SprintCString(&ss->sprinter, rval);
                break;
              }

              case JSOP_GETFCSLOT:
              case JSOP_CALLFCSLOT:
              {
                if (!jp->fun)
                    jp->fun = jp->script->getCallerFunction();

                if (!jp->localNames) {
                    JS_ASSERT(fun == jp->fun);
                    jp->localNames = cx->new_<Vector<JSAtom *> >(cx);
                    if (!jp->localNames ||
                        !jp->fun->script()->bindings.getLocalNameArray(cx, jp->localNames))
                    {
                        return NULL;
                    }
                }

                uintN index = GET_UINT16(pc);
                if (index < jp->fun->script()->bindings.countUpvars()) {
                    index += jp->fun->script()->bindings.countArgsAndVars();
                } else {
                    JSUpvarArray *uva;
#ifdef DEBUG
                    /*
                     * We must be in an eval called from jp->fun, where
                     * jp->script is the eval-compiled script.
                     *
                     * However, it's possible that a js_Invoke already
                     * pushed a frame trying to call Construct on an
                     * object that's not a constructor, causing us to be
                     * called with an intervening frame on the stack.
                     */
                    StackFrame *fp = js_GetTopStackFrame(cx, FRAME_EXPAND_NONE);
                    if (fp) {
                        while (!fp->isEvalFrame())
                            fp = fp->prev();
                        JS_ASSERT(fp->script() == jp->script);
                        JS_ASSERT(fp->prev()->fun() == jp->fun);
                        JS_ASSERT(jp->fun->isInterpreted());
                        JS_ASSERT(jp->script != jp->fun->script());
                        JS_ASSERT(JSScript::isValidOffset(jp->script->upvarsOffset));
                    }
#endif
                    uva = jp->script->upvars();
                    index = uva->vector[index].slot();
                }
                atom = GetArgOrVarAtom(jp, index);
                goto do_name;
              }

              case JSOP_CALLLOCAL:
              case JSOP_GETLOCAL:
                if (IsVarSlot(jp, pc, &i)) {
                    atom = GetArgOrVarAtom(jp, i);
                    LOCAL_ASSERT(atom);
                    goto do_name;
                }
                LOCAL_ASSERT((uintN)i < ss->top);
                sn = js_GetSrcNote(jp->script, pc);

#if JS_HAS_DESTRUCTURING
                if (sn && SN_TYPE(sn) == SRC_GROUPASSIGN) {
                    /*
                     * Distinguish a js_DecompileValueGenerator call that
                     * targets op alone, from decompilation of a full group
                     * assignment sequence, triggered by SRC_GROUPASSIGN
                     * annotating the first JSOP_GETLOCAL in the sequence.
                     */
                    if (endpc - pc > JSOP_GETLOCAL_LENGTH || pc > startpc) {
                        pc = DecompileGroupAssignment(ss, pc, endpc, sn, &todo);
                        if (!pc)
                            return NULL;
                        LOCAL_ASSERT(*pc == JSOP_POPN);
                        len = oplen = JSOP_POPN_LENGTH;
                        goto end_groupassignment;
                    }

                    /* Null sn to prevent bogus VarPrefix'ing below. */
                    sn = NULL;
                }
#endif

                rval = GetLocal(ss, i);
                todo = Sprint(&ss->sprinter, ss_format, VarPrefix(sn), rval);
                break;

              case JSOP_SETLOCAL:
              case JSOP_SETLOCALPOP:
                if (IsVarSlot(jp, pc, &i)) {
                    atom = GetArgOrVarAtom(jp, i);
                    LOCAL_ASSERT(atom);
                    goto do_setname;
                }
                lval = GetLocal(ss, i);
                rval = POP_STR();
                goto do_setlval;

              case JSOP_INCLOCAL:
              case JSOP_DECLOCAL:
                if (IsVarSlot(jp, pc, &i)) {
                    atom = GetArgOrVarAtom(jp, i);
                    LOCAL_ASSERT(atom);
                    goto do_incatom;
                }
                lval = GetLocal(ss, i);
                goto do_inclval;

              case JSOP_LOCALINC:
              case JSOP_LOCALDEC:
                if (IsVarSlot(jp, pc, &i)) {
                    atom = GetArgOrVarAtom(jp, i);
                    LOCAL_ASSERT(atom);
                    goto do_atominc;
                }
                lval = GetLocal(ss, i);
                goto do_lvalinc;

              case JSOP_RETRVAL:
                todo = -2;
                break;

              case JSOP_RETURN:
                LOCAL_ASSERT(jp->fun);
                fun = jp->fun;
                if (fun->flags & JSFUN_EXPR_CLOSURE) {
                    /* Turn on parens around comma-expression here. */
                    op = JSOP_SETNAME;
                    rval = POP_STR();
                    js_printf(jp, (*rval == '{') ? "(%s)%s" : ss_format,
                              rval,
                              ((fun->flags & JSFUN_LAMBDA) || !fun->atom)
                              ? ""
                              : ";");
                    todo = -2;
                    break;
                }
                /* FALL THROUGH */

              case JSOP_SETRVAL:
                rval = POP_STR();
                if (*rval != '\0')
                    js_printf(jp, "\t%s %s;\n", js_return_str, rval);
                else
                    js_printf(jp, "\t%s;\n", js_return_str);
                todo = -2;
                break;

#if JS_HAS_GENERATORS
              case JSOP_YIELD:
#if JS_HAS_GENERATOR_EXPRS
                if (!ss->inGenExp || !(sn = js_GetSrcNote(jp->script, pc)))
#endif
                {
                    /* Turn off most parens. */
                    op = JSOP_SETNAME;
                    rval = POP_STR();
                    todo = (*rval != '\0')
                           ? Sprint(&ss->sprinter,
                                    (strncmp(rval, js_yield_str, 5) == 0 &&
                                     (rval[5] == ' ' || rval[5] == '\0'))
                                    ? "%s (%s)"
                                    : "%s %s",
                                    js_yield_str, rval)
                           : SprintCString(&ss->sprinter, js_yield_str);
                    break;
                }

#if JS_HAS_GENERATOR_EXPRS
                LOCAL_ASSERT(SN_TYPE(sn) == SRC_HIDDEN);
                /* FALL THROUGH */
#endif

              case JSOP_ARRAYPUSH:
              {
                /* Turn off most parens. */
                op = JSOP_SETNAME;

                /* Pop the expression being pushed or yielded. */
                rval = POP_STR();

                /*
                 * Skip the for loop head stacked by JSOP_GOTO:SRC_FOR_IN until
                 * we hit a block local slot (note empty destructuring patterns
                 * result in unit-count blocks).
                 */
                uintN pos = ss->top;
                while (pos != 0) {
                    op = (JSOp) ss->opcodes[--pos];
                    if (op == JSOP_ENTERBLOCK)
                        break;
                }
                JS_ASSERT(op == JSOP_ENTERBLOCK);

                /*
                 * Here, forpos must index the space before the left-most |for|
                 * in the single string of accumulated |for| heads and optional
                 * final |if (condition)|.
                 */
                uintN forpos = pos + 1;
                LOCAL_ASSERT(forpos < ss->top);

                /*
                 * Now move pos downward over the block's local slots. Even an
                 * empty destructuring pattern has one (dummy) local.
                 */
                while (ss->opcodes[pos] == JSOP_ENTERBLOCK) {
                    if (pos == 0)
                        break;
                    --pos;
                }

#if JS_HAS_GENERATOR_EXPRS
                if (saveop == JSOP_YIELD) {
                    /*
                     * Generator expression: decompile just rval followed by
                     * the string starting at forpos. Leave the result string
                     * in ss->offsets[0] so it can be recovered by our caller
                     * (the JSOP_ANONFUNOBJ with SRC_GENEXP case). Bump the
                     * top of stack to balance yield, which is an expression
                     * (so has neutral stack balance).
                     */
                    LOCAL_ASSERT(pos == 0);
                    xval = OFF2STR(&ss->sprinter, ss->offsets[forpos]);
                    ss->sprinter.offset = PAREN_SLOP;
                    todo = Sprint(&ss->sprinter, ss_format, rval, xval);
                    if (todo < 0)
                        return NULL;
                    ss->offsets[0] = todo;
                    ++ss->top;
                    return pc;
                }
#endif /* JS_HAS_GENERATOR_EXPRS */

                /*
                 * Array comprehension: retract the sprinter to the beginning
                 * of the array initialiser and decompile "[<rval> for ...]".
                 */
                JS_ASSERT(jp->script->nfixed + pos == GET_UINT16(pc));
                LOCAL_ASSERT(ss->opcodes[pos] == JSOP_NEWINIT);

                ptrdiff_t start = ss->offsets[pos];
                LOCAL_ASSERT(ss->sprinter.base[start] == '[' ||
                             ss->sprinter.base[start] == '#');
                LOCAL_ASSERT(forpos < ss->top);
                xval = OFF2STR(&ss->sprinter, ss->offsets[forpos]);
                lval = OFF2STR(&ss->sprinter, start);
                RETRACT(&ss->sprinter, lval);

                todo = Sprint(&ss->sprinter, sss_format, lval, rval, xval);
                if (todo < 0)
                    return NULL;
                ss->offsets[pos] = todo;
                todo = -2;
                break;
              }
#endif /* JS_HAS_GENERATORS */

              case JSOP_THROWING:
                todo = -2;
                break;

              case JSOP_THROW:
                sn = js_GetSrcNote(jp->script, pc);
                todo = -2;
                if (sn && SN_TYPE(sn) == SRC_HIDDEN)
                    break;
                rval = POP_STR();
                js_printf(jp, "\t%s %s;\n", js_throw_str, rval);
                break;

              case JSOP_ITER:
                foreach = (pc[1] & (JSITER_FOREACH | JSITER_KEYVALUE)) ==
                          JSITER_FOREACH;
                todo = -2;
                break;

              case JSOP_MOREITER:
                JS_NOT_REACHED("JSOP_MOREITER");
                break;

              case JSOP_ENDITER:
                sn = js_GetSrcNote(jp->script, pc);
                todo = -2;
                if (sn && SN_TYPE(sn) == SRC_HIDDEN)
                    break;
                (void) PopOff(ss, op);
                break;

              case JSOP_GOTO:
              case JSOP_GOTOX:
                sn = js_GetSrcNote(jp->script, pc);
                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
                  case SRC_FOR_IN:
                    /*
                     * The bytecode around pc looks like this:
                     *     <<RHS>>
                     *     iter
                     * pc: goto/gotox C         [src_for_in(B, D)]
                     *  A: <<LHS = iternext>>
                     *  B: pop                  [maybe a src_decl_var/let]
                     *     <<S>>
                     *  C: moreiter
                     *     ifne/ifnex A
                     *     enditer
                     *  D: ...
                     *
                     * In an array comprehension or generator expression, we
                     * construct the for-head and store it in the slot pushed
                     * by JSOP_ITER, then recurse to decompile S. The
                     * culminating JSOP_ARRAYPUSH or JSOP_YIELD instruction
                     * (which S must contain, by construction) glues all the
                     * clauses together.
                     *
                     * Otherwise this is a for-in statement. We eagerly output
                     * the for-head and recurse to decompile the controlled
                     * statement S.
                     *
                     * We never decompile the obligatory JSOP_POP,
                     * JSOP_MOREITER or JSOP_IFNE, though we do quick asserts
                     * to check that they are there.
                     */
                    cond = GetJumpOffset(pc, pc);
                    next = js_GetSrcNoteOffset(sn, 0);
                    tail = js_GetSrcNoteOffset(sn, 1);
                    JS_ASSERT(pc[next] == JSOP_POP);
                    JS_ASSERT(pc[cond] == JSOP_MOREITER);
                    DECOMPILE_CODE(pc + oplen, next - oplen);
                    lval = POP_STR();

                    /*
                     * This string "<next>" comes from jsopcode.tbl. It stands
                     * for the result pushed by JSOP_ITERNEXT.
                     */
                    JS_ASSERT(strcmp(lval + strlen(lval) - 9, " = <next>") == 0);
                    const_cast<char *>(lval)[strlen(lval) - 9] = '\0';
                    LOCAL_ASSERT(ss->top >= 1);

                    if (ss->inArrayInit || ss->inGenExp) {
                        rval = POP_STR();
                        if (ss->top >= 1 && ss->opcodes[ss->top - 1] == JSOP_FORLOCAL) {
                            ss->sprinter.offset = ss->offsets[ss->top] - PAREN_SLOP;
                            if (Sprint(&ss->sprinter, " %s (%s in %s)",
                                       foreach ? js_for_each_str : js_for_str,
                                       lval, rval) < 0) {
                                return NULL;
                            }

                            /*
                             * Do not AddParentSlop here, as we will push the
                             * top-most offset again, which will add paren slop
                             * for us. We must push to balance the stack budget
                             * when nesting for heads in a comprehension.
                             */
                            todo = ss->offsets[ss->top - 1];
                        } else {
                            todo = Sprint(&ss->sprinter, " %s (%s in %s)",
                                          foreach ? js_for_each_str : js_for_str,
                                          lval, rval);
                        }
                        if (todo < 0 || !PushOff(ss, todo, JSOP_FORLOCAL))
                            return NULL;
                        DECOMPILE_CODE(pc + next + JSOP_POP_LENGTH, cond - next - JSOP_POP_LENGTH);
                    } else {
                        /*
                         * As above, rval or an extension of it must remain
                         * stacked during loop body decompilation.
                         */
                        rval = GetStr(ss, ss->top - 1);
                        xval = VarPrefix(js_GetSrcNote(jp->script, pc + next));
                        js_printf(jp, "\t%s (%s%s in %s) {\n",
                                  foreach ? js_for_each_str : js_for_str,
                                  xval, lval, rval);
                        jp->indent += 4;
                        DECOMPILE_CODE(pc + next + JSOP_POP_LENGTH, cond - next - JSOP_POP_LENGTH);
                        jp->indent -= 4;
                        js_printf(jp, "\t}\n");
                    }

                    pc += tail;
                    LOCAL_ASSERT(*pc == JSOP_IFNE || *pc == JSOP_IFNEX);
                    len = js_CodeSpec[*pc].length;
                    break;

                  case SRC_WHILE:
                    cond = GetJumpOffset(pc, pc);
                    tail = js_GetSrcNoteOffset(sn, 0);
                    DECOMPILE_CODE(pc + cond, tail - cond);
                    js_printf(jp, "\twhile (%s) {\n", POP_COND_STR());
                    jp->indent += 4;
                    DECOMPILE_CODE(pc + oplen, cond - oplen);
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");
                    pc += tail;
                    LOCAL_ASSERT(*pc == JSOP_IFNE || *pc == JSOP_IFNEX);
                    len = js_CodeSpec[*pc].length;
                    todo = -2;
                    break;

                  case SRC_CONT2LABEL:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    RETRACT(&ss->sprinter, rval);
                    js_printf(jp, "\tcontinue %s;\n", rval);
                    break;

                  case SRC_CONTINUE:
                    js_printf(jp, "\tcontinue;\n");
                    break;

                  case SRC_BREAK2LABEL:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    RETRACT(&ss->sprinter, rval);
                    js_printf(jp, "\tbreak %s;\n", rval);
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
              case JSOP_IFEQX:
              {
                JSBool elseif = JS_FALSE;

              if_again:
                len = GetJumpOffset(pc, pc);
                sn = js_GetSrcNote(jp->script, pc);

                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
                  case SRC_IF:
                  case SRC_IF_ELSE:
                    rval = POP_COND_STR();
                    if (ss->inArrayInit || ss->inGenExp) {
                        LOCAL_ASSERT(SN_TYPE(sn) == SRC_IF);
                        ss->sprinter.offset -= PAREN_SLOP;
                        if (Sprint(&ss->sprinter, " if (%s)", rval) < 0)
                            return NULL;
                        AddParenSlop(ss);
                    } else {
                        js_printf(jp,
                                  elseif ? " if (%s) {\n" : "\tif (%s) {\n",
                                  rval);
                        jp->indent += 4;
                    }

                    if (SN_TYPE(sn) == SRC_IF) {
                        DECOMPILE_CODE(pc + oplen, len - oplen);
                    } else {
                        LOCAL_ASSERT(!ss->inArrayInit && !ss->inGenExp);
                        tail = js_GetSrcNoteOffset(sn, 0);
                        DECOMPILE_CODE(pc + oplen, tail - oplen);
                        jp->indent -= 4;
                        pc += tail;
                        LOCAL_ASSERT(*pc == JSOP_GOTO || *pc == JSOP_GOTOX);
                        oplen = js_CodeSpec[*pc].length;
                        len = GetJumpOffset(pc, pc);
                        js_printf(jp, "\t} else");

                        /*
                         * If the second offset for sn is non-zero, it tells
                         * the distance from the goto around the else, to the
                         * ifeq for the if inside the else that forms an "if
                         * else if" chain.  Thus cond spans the condition of
                         * the second if, so we simply decompile it and start
                         * over at label if_again.
                         */
                        cond = js_GetSrcNoteOffset(sn, 1);
                        if (cond != 0) {
                            cond -= tail;
                            DECOMPILE_CODE(pc + oplen, cond - oplen);
                            pc += cond;
                            oplen = js_CodeSpec[*pc].length;
                            elseif = JS_TRUE;
                            goto if_again;
                        }

                        js_printf(jp, " {\n");
                        jp->indent += 4;
                        DECOMPILE_CODE(pc + oplen, len - oplen);
                    }

                    if (!ss->inArrayInit && !ss->inGenExp) {
                        jp->indent -= 4;
                        js_printf(jp, "\t}\n");
                    }
                    todo = -2;
                    break;

                  case SRC_COND:
                    xval = JS_strdup(cx, POP_STR());
                    if (!xval)
                        return NULL;
                    len = js_GetSrcNoteOffset(sn, 0);
                    lval = NULL;
                    DECOMPILE_CODE_CLEANUP(pc + oplen, len - oplen, goto src_cond_error);
                    lval = JS_strdup(cx, POP_STR());
                    if (!lval) goto src_cond_error;
                    pc += len;
                    LOCAL_ASSERT(*pc == JSOP_GOTO || *pc == JSOP_GOTOX);
                    oplen = js_CodeSpec[*pc].length;
                    len = GetJumpOffset(pc, pc);
                    DECOMPILE_CODE_CLEANUP(pc + oplen, len - oplen, goto src_cond_error);
                    rval = POP_STR();
                    todo = Sprint(&ss->sprinter, "%s ? %s : %s",
                                  xval, lval, rval);
                    cx->free_((void *)xval);
                    cx->free_((void *)lval);
                    break;

                    src_cond_error:
                        cx->free_((void *)xval);
                        if (lval)
                           cx->free_((void *)lval);
                        return NULL;

                  default:
                    break;
                }
                break;
              }

              case JSOP_IFNE:
              case JSOP_IFNEX:
                LOCAL_ASSERT(0);
                break;

              case JSOP_OR:
              case JSOP_ORX:
                xval = "||";

              do_logical_connective:
                /* Top of stack is the first clause in a disjunction (||). */
                lval = JS_strdup(cx, POP_STR());
                if (!lval)
                    return NULL;
                done = pc + GetJumpOffset(pc, pc);
                pc += len;
                len = done - pc;
                if (!Decompile(ss, pc, len, op)) {
                    cx->free_((char *)lval);
                    return NULL;
                }
                rval = POP_STR();
                if (jp->pretty &&
                    jp->indent + 4 + strlen(lval) + 4 + strlen(rval) > 75) {
                    rval = JS_strdup(cx, rval);
                    if (!rval) {
                        tail = -1;
                    } else {
                        todo = Sprint(&ss->sprinter, "%s %s\n", lval, xval);
                        tail = Sprint(&ss->sprinter, "%*s%s",
                                      jp->indent + 4, "", rval);
                        cx->free_((char *)rval);
                    }
                    if (tail < 0)
                        todo = -1;
                } else {
                    todo = Sprint(&ss->sprinter, "%s %s %s", lval, xval, rval);
                }
                cx->free_((char *)lval);
                break;

              case JSOP_AND:
              case JSOP_ANDX:
                xval = "&&";
                goto do_logical_connective;

              case JSOP_ENUMELEM:
              case JSOP_ENUMCONSTELEM:
                /*
                 * The stack has the object under the (top) index expression.
                 * The "rval" property id is underneath those two on the stack.
                 * The for loop body net and gross lengths can now be adjusted
                 * to account for the length of the indexing expression that
                 * came after JSOP_FORELEM and before JSOP_ENUMELEM.
                 */
                atom = NULL;
                op = JSOP_NOP;          /* turn off parens around xval */
                xval = POP_STR();
                op = JSOP_GETELEM;      /* lval must have high precedence */
                lval = POP_STR();
                op = saveop;
                rval = POP_STR();
                LOCAL_ASSERT(strcmp(rval, forelem_cookie) == 0);
                if (*xval == '\0') {
                    todo = SprintCString(&ss->sprinter, lval);
                } else {
                    todo = Sprint(&ss->sprinter,
                                  (JOF_OPMODE(lastop) == JOF_XMLNAME)
                                  ? dot_format
                                  : index_format,
                                  lval, xval);
                }
                break;

              case JSOP_GETTER:
              case JSOP_SETTER:
                todo = -2;
                break;

              case JSOP_DUP2:
                rval = GetStr(ss, ss->top-2);
                todo = SprintCString(&ss->sprinter, rval);
                if (todo < 0 || !PushOff(ss, todo,
                                         (JSOp) ss->opcodes[ss->top-2])) {
                    return NULL;
                }
                /* FALL THROUGH */

              case JSOP_DUP:
#if JS_HAS_DESTRUCTURING
                sn = js_GetSrcNote(jp->script, pc);
                if (sn) {
                    LOCAL_ASSERT(SN_TYPE(sn) == SRC_DESTRUCT);
                    pc = DecompileDestructuring(ss, pc, endpc);
                    if (!pc)
                        return NULL;
                    len = 0;
                    lval = POP_STR();
                    op = saveop = JSOP_ENUMELEM;
                    rval = POP_STR();

                    if (strcmp(rval, forelem_cookie) == 0) {
                        todo = Sprint(&ss->sprinter, ss_format,
                                      VarPrefix(sn), lval);

                        // Skip POP so the SRC_FOR_IN code can pop for itself.
                        if (*pc == JSOP_POP)
                            len = JSOP_POP_LENGTH;
                    } else {
                        todo = Sprint(&ss->sprinter, "%s%s = %s",
                                      VarPrefix(sn), lval, rval);
                    }
                    break;
                }
#endif

                rval = GetStr(ss, ss->top-1);
                saveop = (JSOp) ss->opcodes[ss->top-1];
                todo = SprintCString(&ss->sprinter, rval);
                break;

              case JSOP_SETARG:
                atom = GetArgOrVarAtom(jp, GET_ARGNO(pc));
                LOCAL_ASSERT(atom);
                goto do_setname;

              case JSOP_SETCONST:
              case JSOP_SETNAME:
              case JSOP_SETGNAME:
                LOAD_ATOM(0);

              do_setname:
                lval = QuoteString(&ss->sprinter, atom, 0);
                if (!lval)
                    return NULL;
                rval = POP_STR();
                if (op == JSOP_SETNAME || op == JSOP_SETGNAME)
                    (void) PopOff(ss, op);

              do_setlval:
                sn = js_GetSrcNote(jp->script, pc - 1);
                if (sn && SN_TYPE(sn) == SRC_ASSIGNOP) {
                    todo = Sprint(&ss->sprinter, "%s %s= %s",
                                  lval,
                                  (lastop == JSOP_GETTER)
                                  ? js_getter_str
                                  : (lastop == JSOP_SETTER)
                                  ? js_setter_str
                                  : CodeToken[lastop],
                                  rval);
                } else {
                    sn = js_GetSrcNote(jp->script, pc);
                    todo = Sprint(&ss->sprinter, "%s%s = %s",
                                  VarPrefix(sn), lval, rval);
                }
                if (op == JSOP_SETLOCALPOP) {
                    if (!PushOff(ss, todo, saveop))
                        return NULL;
                    rval = POP_STR();
                    LOCAL_ASSERT(*rval != '\0');
                    js_printf(jp, "\t%s;\n", rval);
                    todo = -2;
                }
                break;

              case JSOP_NEW:
              case JSOP_CALL:
              case JSOP_EVAL:
              case JSOP_FUNCALL:
              case JSOP_FUNAPPLY:
                argc = GET_ARGC(pc);
                argv = (char **)
                    cx->malloc_((size_t)(argc + 1) * sizeof *argv);
                if (!argv)
                    return NULL;

                op = JSOP_SETNAME;
                ok = JS_TRUE;
                for (i = argc; i > 0; i--)
                    argv[i] = JS_strdup(cx, POP_STR());

                /* Skip the JSOP_PUSHOBJ-created empty string. */
                LOCAL_ASSERT(ss->top >= 2);
                (void) PopOff(ss, op);

                /*
                 * Special case: new (x(y)(z)) must be parenthesized like so.
                 * Same for new (x(y).z) -- contrast with new x(y).z.
                 * See PROPAGATE_CALLNESS.
                 */
                op = (JSOp) ss->opcodes[ss->top - 1];
                lval = PopStr(ss,
                              (saveop == JSOP_NEW &&
                               (op == JSOP_CALL ||
                                op == JSOP_EVAL ||
                                op == JSOP_FUNCALL ||
                                op == JSOP_FUNAPPLY ||
                                (js_CodeSpec[op].format & JOF_CALLOP)))
                              ? JSOP_NAME
                              : saveop);
                op = saveop;

                argv[0] = JS_strdup(cx, lval);
                if (!argv[0])
                    ok = JS_FALSE;

                lval = "(", rval = ")";
                if (op == JSOP_NEW) {
                    if (argc == 0)
                        lval = rval = "";
                    todo = Sprint(&ss->sprinter, "%s %s%s",
                                  js_new_str, argv[0], lval);
                } else {
                    todo = Sprint(&ss->sprinter, ss_format,
                                  argv[0], lval);
                }
                if (todo < 0)
                    ok = JS_FALSE;

                for (i = 1; i <= argc; i++) {
                    if (!argv[i] ||
                        Sprint(&ss->sprinter, ss_format,
                               argv[i], (i < argc) ? ", " : "") < 0) {
                        ok = JS_FALSE;
                        break;
                    }
                }
                if (Sprint(&ss->sprinter, rval) < 0)
                    ok = JS_FALSE;

                for (i = 0; i <= argc; i++)
                    cx->free_(argv[i]);
                cx->free_(argv);
                if (!ok)
                    return NULL;
                break;

              case JSOP_SETCALL:
                todo = Sprint(&ss->sprinter, "");
                break;

              case JSOP_DELNAME:
                LOAD_ATOM(0);
                lval = QuoteString(&ss->sprinter, atom, 0);
                if (!lval)
                    return NULL;
                RETRACT(&ss->sprinter, lval);
              do_delete_lval:
                todo = Sprint(&ss->sprinter, "%s %s", js_delete_str, lval);
                break;

              case JSOP_DELPROP:
                GET_ATOM_QUOTE_AND_FMT("%s %s[%s]", "%s %s.%s", rval);
                op = JSOP_GETPROP;
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, fmt, js_delete_str, lval, rval);
                break;

              case JSOP_DELELEM:
                op = JSOP_NOP;          /* turn off parens */
                xval = POP_STR();
                op = JSOP_GETPROP;
                lval = POP_STR();
                if (*xval == '\0')
                    goto do_delete_lval;
                todo = Sprint(&ss->sprinter,
                              (JOF_OPMODE(lastop) == JOF_XMLNAME)
                              ? "%s %s.%s"
                              : "%s %s[%s]",
                              js_delete_str, lval, xval);
                break;

#if JS_HAS_XML_SUPPORT
              case JSOP_DELDESC:
                xval = POP_STR();
                op = JSOP_GETPROP;
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s %s..%s",
                              js_delete_str, lval, xval);
                break;
#endif

              case JSOP_TYPEOFEXPR:
              case JSOP_TYPEOF:
              case JSOP_VOID:
                rval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s %s",
                              (op == JSOP_VOID) ? js_void_str : js_typeof_str,
                              rval);
                break;

              case JSOP_INCARG:
              case JSOP_DECARG:
                atom = GetArgOrVarAtom(jp, GET_ARGNO(pc));
                LOCAL_ASSERT(atom);
                goto do_incatom;

              case JSOP_INCNAME:
              case JSOP_DECNAME:
              case JSOP_INCGNAME:
              case JSOP_DECGNAME:
                LOAD_ATOM(0);
              do_incatom:
                lval = QuoteString(&ss->sprinter, atom, 0);
                if (!lval)
                    return NULL;
                RETRACT(&ss->sprinter, lval);
              do_inclval:
                todo = Sprint(&ss->sprinter, ss_format,
                              js_incop_strs[!(cs->format & JOF_INC)], lval);
                if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
                    len += GetDecomposeLength(pc, js_CodeSpec[*pc].length);
                break;

              case JSOP_INCPROP:
              case JSOP_DECPROP:
                GET_ATOM_QUOTE_AND_FMT(preindex_format, predot_format, rval);

                /*
                 * Force precedence below the numeric literal opcodes, so that
                 * 42..foo or 10000..toString(16), e.g., decompile with parens
                 * around the left-hand side of dot.
                 */
                op = JSOP_GETPROP;
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, fmt,
                              js_incop_strs[!(cs->format & JOF_INC)],
                              lval, rval);
                len += GetDecomposeLength(pc, JSOP_INCPROP_LENGTH);
                break;

              case JSOP_INCELEM:
              case JSOP_DECELEM:
                op = JSOP_NOP;          /* turn off parens */
                xval = POP_STR();
                op = JSOP_GETELEM;
                lval = POP_STR();
                if (*xval != '\0') {
                    todo = Sprint(&ss->sprinter,
                                  (JOF_OPMODE(lastop) == JOF_XMLNAME)
                                  ? predot_format
                                  : preindex_format,
                                  js_incop_strs[!(cs->format & JOF_INC)],
                                  lval, xval);
                } else {
                    todo = Sprint(&ss->sprinter, ss_format,
                                  js_incop_strs[!(cs->format & JOF_INC)], lval);
                }
                len += GetDecomposeLength(pc, JSOP_INCELEM_LENGTH);
                break;

              case JSOP_ARGINC:
              case JSOP_ARGDEC:
                atom = GetArgOrVarAtom(jp, GET_ARGNO(pc));
                LOCAL_ASSERT(atom);
                goto do_atominc;

              case JSOP_NAMEINC:
              case JSOP_NAMEDEC:
              case JSOP_GNAMEINC:
              case JSOP_GNAMEDEC:
                LOAD_ATOM(0);
              do_atominc:
                lval = QuoteString(&ss->sprinter, atom, 0);
                if (!lval)
                    return NULL;
                RETRACT(&ss->sprinter, lval);
              do_lvalinc:
                todo = Sprint(&ss->sprinter, ss_format,
                              lval, js_incop_strs[!(cs->format & JOF_INC)]);
                if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
                    len += GetDecomposeLength(pc, js_CodeSpec[*pc].length);
                break;

              case JSOP_PROPINC:
              case JSOP_PROPDEC:
                GET_ATOM_QUOTE_AND_FMT(postindex_format, postdot_format, rval);

                /*
                 * Force precedence below the numeric literal opcodes, so that
                 * 42..foo or 10000..toString(16), e.g., decompile with parens
                 * around the left-hand side of dot.
                 */
                op = JSOP_GETPROP;
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, fmt, lval, rval,
                              js_incop_strs[!(cs->format & JOF_INC)]);
                len += GetDecomposeLength(pc, JSOP_PROPINC_LENGTH);
                break;

              case JSOP_ELEMINC:
              case JSOP_ELEMDEC:
                op = JSOP_NOP;          /* turn off parens */
                xval = POP_STR();
                op = JSOP_GETELEM;
                lval = POP_STR();
                if (*xval != '\0') {
                    todo = Sprint(&ss->sprinter,
                                  (JOF_OPMODE(lastop) == JOF_XMLNAME)
                                  ? postdot_format
                                  : postindex_format,
                                  lval, xval,
                                  js_incop_strs[!(cs->format & JOF_INC)]);
                } else {
                    todo = Sprint(&ss->sprinter, ss_format,
                                  lval, js_incop_strs[!(cs->format & JOF_INC)]);
                }
                len += GetDecomposeLength(pc, JSOP_ELEMINC_LENGTH);
                break;

              case JSOP_GETPROP2:
                op = JSOP_GETPROP;
                (void) PopOff(ss, lastop);
                /* FALL THROUGH */

              case JSOP_CALLPROP:
              case JSOP_GETPROP:
              case JSOP_GETXPROP:
              case JSOP_LENGTH:
                LOAD_ATOM(0);

                GET_QUOTE_AND_FMT(index_format, dot_format, rval);
                PROPAGATE_CALLNESS();
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, fmt, lval, rval);
                break;

              case JSOP_SETPROP:
              case JSOP_SETMETHOD:
                LOAD_ATOM(0);
                GET_QUOTE_AND_FMT("%s[%s] %s= %s", "%s.%s %s= %s", xval);
                rval = POP_STR();

                /*
                 * Force precedence below the numeric literal opcodes, so that
                 * 42..foo or 10000..toString(16), e.g., decompile with parens
                 * around the left-hand side of dot.
                 */
                op = JSOP_GETPROP;
                lval = POP_STR();
                sn = js_GetSrcNote(jp->script, pc - 1);
                todo = Sprint(&ss->sprinter, fmt, lval, xval,
                              (sn && SN_TYPE(sn) == SRC_ASSIGNOP)
                              ? (lastop == JSOP_GETTER)
                                ? js_getter_str
                                : (lastop == JSOP_SETTER)
                                ? js_setter_str
                                : CodeToken[lastop]
                              : "",
                              rval);
                break;

              case JSOP_GETELEM2:
                op = JSOP_GETELEM;
                (void) PopOff(ss, lastop);
                /* FALL THROUGH */

              case JSOP_CALLELEM:
              case JSOP_GETELEM:
                op = JSOP_NOP;          /* turn off parens */
                xval = POP_STR();
                op = saveop;
                PROPAGATE_CALLNESS();
                lval = POP_STR();
                if (*xval == '\0') {
                    todo = Sprint(&ss->sprinter, "%s", lval);
                } else {
                    todo = Sprint(&ss->sprinter,
                                  (JOF_OPMODE(lastop) == JOF_XMLNAME)
                                  ? dot_format
                                  : index_format,
                                  lval, xval);
                }
                break;

              case JSOP_SETELEM:
                rval = POP_STR();
                op = JSOP_NOP;          /* turn off parens */
                xval = POP_STR();
                cs = &js_CodeSpec[ss->opcodes[ss->top]];
                op = JSOP_GETELEM;      /* lval must have high precedence */
                lval = POP_STR();
                op = saveop;
                if (*xval == '\0')
                    goto do_setlval;
                sn = js_GetSrcNote(jp->script, pc - 1);
                todo = Sprint(&ss->sprinter,
                              (JOF_MODE(cs->format) == JOF_XMLNAME)
                              ? "%s.%s %s= %s"
                              : "%s[%s] %s= %s",
                              lval, xval,
                              (sn && SN_TYPE(sn) == SRC_ASSIGNOP)
                              ? (lastop == JSOP_GETTER)
                                ? js_getter_str
                                : (lastop == JSOP_SETTER)
                                ? js_setter_str
                                : CodeToken[lastop]
                              : "",
                              rval);
                break;

              case JSOP_ARGSUB:
                i = (jsint) GET_ARGNO(pc);
                todo = Sprint(&ss->sprinter, "%s[%d]",
                              js_arguments_str, (int) i);
                break;

              case JSOP_ARGCNT:
                todo = Sprint(&ss->sprinter, dot_format,
                              js_arguments_str, js_length_str);
                break;

              case JSOP_CALLARG:
              case JSOP_GETARG:
                i = GET_ARGNO(pc);
                atom = GetArgOrVarAtom(jp, i);
#if JS_HAS_DESTRUCTURING
                if (!atom) {
                    todo = Sprint(&ss->sprinter, "%s[%d]", js_arguments_str, i);
                    break;
                }
#else
                LOCAL_ASSERT(atom);
#endif
                goto do_name;

              case JSOP_CALLNAME:
              case JSOP_NAME:
              case JSOP_GETGNAME:
              case JSOP_CALLGNAME:
                LOAD_ATOM(0);
              do_name:
                lval = "";
#if JS_HAS_XML_SUPPORT
              do_qname:
#endif
                sn = js_GetSrcNote(jp->script, pc);
                rval = QuoteString(&ss->sprinter, atom, inXML ? DONT_ESCAPE : 0);
                if (!rval)
                    return NULL;
                RETRACT(&ss->sprinter, rval);
                todo = Sprint(&ss->sprinter, sss_format,
                              VarPrefix(sn), lval, rval);
                break;

              case JSOP_UINT16:
                i = (jsint) GET_UINT16(pc);
                goto do_sprint_int;

              case JSOP_UINT24:
                i = (jsint) GET_UINT24(pc);
                goto do_sprint_int;

              case JSOP_INT8:
                i = GET_INT8(pc);
                goto do_sprint_int;

              case JSOP_INT32:
                i = GET_INT32(pc);
              do_sprint_int:
                todo = Sprint(&ss->sprinter, "%d", i);
                break;

              case JSOP_DOUBLE:
              {
                double d;
                GET_DOUBLE_FROM_BYTECODE(jp->script, pc, 0, d);
                val = DOUBLE_TO_JSVAL(d);
                todo = SprintDoubleValue(&ss->sprinter, val, &saveop);
                break;
              }

              case JSOP_STRING:
                LOAD_ATOM(0);
                rval = QuoteString(&ss->sprinter, atom, inXML ? DONT_ESCAPE : '"');
                if (!rval)
                    return NULL;
                todo = STR2OFF(&ss->sprinter, rval);
                break;

              case JSOP_LAMBDA:
              case JSOP_LAMBDA_FC:
#if JS_HAS_GENERATOR_EXPRS
                sn = js_GetSrcNote(jp->script, pc);
                if (sn && SN_TYPE(sn) == SRC_GENEXP) {
                    Vector<JSAtom *> *innerLocalNames;
                    Vector<JSAtom *> *outerLocalNames;
                    JSScript *inner, *outer;
                    SprintStack ss2;
                    JSFunction *outerfun;

                    LOAD_FUNCTION(0);

                    /*
                     * All allocation when decompiling is LIFO, using malloc
                     * or, more commonly, arena-allocating from cx->tempPool.
                     * Therefore after InitSprintStack succeeds, we must
                     * release to mark before returning.
                     */
                    LifoAllocScope las(&cx->tempLifoAlloc());
                    if (fun->script()->bindings.hasLocalNames()) {
                        innerLocalNames = cx->new_<Vector<JSAtom *> >(cx);
                        if (!innerLocalNames ||
                            fun->script()->bindings.getLocalNameArray(cx, innerLocalNames))
                        {
                            return NULL;
                        }
                    } else {
                        innerLocalNames = NULL;
                    }
                    inner = fun->script();
                    if (!InitSprintStack(cx, &ss2, jp, StackDepth(inner)))
                        return NULL;
                    ss2.inGenExp = JS_TRUE;

                    /*
                     * Recursively decompile this generator function as an
                     * un-parenthesized generator expression. The ss->inGenExp
                     * special case of JSOP_YIELD shares array comprehension
                     * decompilation code that leaves the result as the single
                     * string pushed on ss2.
                     */
                    outer = jp->script;
                    outerfun = jp->fun;
                    outerLocalNames = jp->localNames;
                    LOCAL_ASSERT(JS_UPTRDIFF(pc, outer->code) <= outer->length);
                    jp->script = inner;
                    jp->fun = fun;
                    jp->localNames = innerLocalNames;

                    /*
                     * Decompile only the main bytecode, to avoid tripping over
                     * new prolog ops that have stack effects.
                     */
                    ok = Decompile(&ss2, inner->main(), inner->length - inner->mainOffset,
                                   JSOP_NOP)
                         != NULL;
                    jp->script = outer;
                    jp->fun = outerfun;
                    jp->localNames = outerLocalNames;
                    if (!ok)
                        return NULL;

                    /*
                     * Advance over this op and its global |this| push, and
                     * arrange to advance over the call to this lambda.
                     */
                    pc += len;
                    if (*pc == JSOP_BLOCKCHAIN) {
                        pc += JSOP_BLOCKCHAIN_LENGTH;
                    } else {
                        LOCAL_ASSERT(*pc == JSOP_NULLBLOCKCHAIN);
                        pc += JSOP_NULLBLOCKCHAIN_LENGTH;
                    }
                    LOCAL_ASSERT(*pc == JSOP_PUSH);
                    pc += JSOP_PUSH_LENGTH;
                    LOCAL_ASSERT(*pc == JSOP_CALL);
                    LOCAL_ASSERT(GET_ARGC(pc) == 0);
                    len = JSOP_CALL_LENGTH;

                    /*
                     * Arrange to parenthesize this genexp unless:
                     *
                     *  1. It is the complete expression consumed by a control
                     *     flow bytecode such as JSOP_TABLESWITCH whose syntax
                     *     always parenthesizes the controlling expression.
                     *  2. It is the sole argument to a function call.
                     *
                     * But if this genexp runs up against endpc, parenthesize
                     * regardless.  (This can happen if we are called from
                     * DecompileExpression or recursively from case
                     * JSOP_{NOP,AND,OR}.)
                     *
                     * There's no special case for |if (genexp)| because the
                     * compiler optimizes that to |if (true)|.
                     */
                    pc2 = pc + len;
                    op = JSOp(*pc2);
                    if (op == JSOP_TRACE || op == JSOP_NOP)
                        pc2 += JSOP_NOP_LENGTH;
                    LOCAL_ASSERT(pc2 < endpc ||
                                 endpc < outer->code + outer->length);
                    LOCAL_ASSERT(ss2.top == 1);
                    ss2.opcodes[0] = JSOP_POP;
                    if (pc2 == endpc) {
                        op = JSOP_SETNAME;
                    } else {
                        op = (JSOp) *pc2;
                        op = ((js_CodeSpec[op].format & JOF_PARENHEAD) ||
                              ((js_CodeSpec[op].format & JOF_INVOKE) && GET_ARGC(pc2) == 1))
                             ? JSOP_POP
                             : JSOP_SETNAME;

                        /*
                         * Stack this result as if it's a name and not an
                         * anonymous function, so it doesn't get decompiled as
                         * a generator function in a getter or setter context.
                         * The precedence level is the same for JSOP_NAME and
                         * JSOP_LAMBDA.
                         */
                        LOCAL_ASSERT(js_CodeSpec[JSOP_NAME].prec ==
                                     js_CodeSpec[saveop].prec);
                        saveop = JSOP_NAME;
                    }

                    /*
                     * Alas, we have to malloc a copy of the result left on
                     * the top of ss2 because both ss and ss2 arena-allocate
                     * from cx's tempPool.
                     */
                    rval = JS_strdup(cx, PopStr(&ss2, op));
                    las.releaseEarly();
                    if (!rval)
                        return NULL;
                    todo = SprintCString(&ss->sprinter, rval);
                    cx->free_((void *)rval);
                    break;
                }
#endif /* JS_HAS_GENERATOR_EXPRS */
                /* FALL THROUGH */

                LOAD_FUNCTION(0);
                {
                    /*
                     * Always parenthesize expression closures. We can't force
                     * saveop to a low-precedence op to arrange for auto-magic
                     * parenthesization without confusing getter/setter code
                     * that checks for JSOP_LAMBDA.
                     */
                    bool grouped = !(fun->flags & JSFUN_EXPR_CLOSURE);
                    bool strict = jp->script->strictModeCode;
                    str = js_DecompileToString(cx, "lambda", fun, 0, 
                                               false, grouped, strict,
                                               js_DecompileFunction);
                    if (!str)
                        return NULL;
                }
              sprint_string:
                todo = SprintString(&ss->sprinter, str);
                break;

              case JSOP_CALLEE:
                JS_ASSERT(jp->fun && jp->fun->atom);
                todo = SprintString(&ss->sprinter, jp->fun->atom);
                break;

              case JSOP_OBJECT:
                LOAD_OBJECT(0);
                str = js_ValueToSource(cx, ObjectValue(*obj));
                if (!str)
                    return NULL;
                goto sprint_string;

              case JSOP_REGEXP:
                GET_REGEXP_FROM_BYTECODE(jp->script, pc, 0, obj);
                if (!js_regexp_toString(cx, obj, &val))
                    return NULL;
                str = JSVAL_TO_STRING(val);
                goto sprint_string;

              case JSOP_TABLESWITCH:
              case JSOP_TABLESWITCHX:
              {
                ptrdiff_t jmplen, off, off2;
                jsint j, n, low, high;
                TableEntry *table, *tmp;

                sn = js_GetSrcNote(jp->script, pc);
                LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
                len = js_GetSrcNoteOffset(sn, 0);
                jmplen = (op == JSOP_TABLESWITCH) ? JUMP_OFFSET_LEN
                                                  : JUMPX_OFFSET_LEN;
                pc2 = pc;
                off = GetJumpOffset(pc, pc2);
                pc2 += jmplen;
                low = GET_JUMP_OFFSET(pc2);
                pc2 += JUMP_OFFSET_LEN;
                high = GET_JUMP_OFFSET(pc2);
                pc2 += JUMP_OFFSET_LEN;

                n = high - low + 1;
                if (n == 0) {
                    table = NULL;
                    j = 0;
                    ok = JS_TRUE;
                } else {
                    table = (TableEntry *)
                            cx->malloc_((size_t)n * sizeof *table);
                    if (!table)
                        return NULL;
                    for (i = j = 0; i < n; i++) {
                        table[j].label = NULL;
                        off2 = GetJumpOffset(pc, pc2);
                        if (off2) {
                            sn = js_GetSrcNote(jp->script, pc2);
                            if (sn) {
                                LOCAL_ASSERT(SN_TYPE(sn) == SRC_LABEL);
                                GET_SOURCE_NOTE_ATOM(sn, table[j].label);
                            }
                            table[j].key = INT_TO_JSVAL(low + i);
                            table[j].offset = off2;
                            table[j].order = j;
                            j++;
                        }
                        pc2 += jmplen;
                    }
                    tmp = (TableEntry *)
                          cx->malloc_((size_t)j * sizeof *table);
                    if (tmp) {
                        VOUCH_DOES_NOT_REQUIRE_STACK();
                        ok = js_MergeSort(table, (size_t)j, sizeof(TableEntry),
                                          CompareOffsets, NULL, tmp,
                                          JS_SORTING_GENERIC);
                        cx->free_(tmp);
                    } else {
                        ok = JS_FALSE;
                    }
                }

                if (ok) {
                    ok = DecompileSwitch(ss, table, (uintN)j, pc, len, off,
                                         JS_FALSE);
                }
                cx->free_(table);
                if (!ok)
                    return NULL;
                todo = -2;
                break;
              }

              case JSOP_LOOKUPSWITCH:
              case JSOP_LOOKUPSWITCHX:
              {
                ptrdiff_t jmplen, off, off2;
                jsatomid npairs, k;
                TableEntry *table;

                sn = js_GetSrcNote(jp->script, pc);
                LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
                len = js_GetSrcNoteOffset(sn, 0);
                jmplen = (op == JSOP_LOOKUPSWITCH) ? JUMP_OFFSET_LEN
                                                   : JUMPX_OFFSET_LEN;
                pc2 = pc;
                off = GetJumpOffset(pc, pc2);
                pc2 += jmplen;
                npairs = GET_UINT16(pc2);
                pc2 += UINT16_LEN;

                table = (TableEntry *)
                    cx->malloc_((size_t)npairs * sizeof *table);
                if (!table)
                    return NULL;
                for (k = 0; k < npairs; k++) {
                    sn = js_GetSrcNote(jp->script, pc2);
                    if (sn) {
                        LOCAL_ASSERT(SN_TYPE(sn) == SRC_LABEL);
                        GET_SOURCE_NOTE_ATOM(sn, table[k].label);
                    } else {
                        table[k].label = NULL;
                    }
                    uint16 constIndex = GET_INDEX(pc2);
                    pc2 += INDEX_LEN;
                    off2 = GetJumpOffset(pc, pc2);
                    pc2 += jmplen;
                    table[k].key = jp->script->getConst(constIndex);
                    table[k].offset = off2;
                }

                ok = DecompileSwitch(ss, table, (uintN)npairs, pc, len, off,
                                     JS_FALSE);
                cx->free_(table);
                if (!ok)
                    return NULL;
                todo = -2;
                break;
              }

              case JSOP_CONDSWITCH:
              {
                ptrdiff_t off, off2, caseOff;
                jsint ncases;
                TableEntry *table;

                sn = js_GetSrcNote(jp->script, pc);
                LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
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
                    LOCAL_ASSERT(*pc2 == JSOP_CASE || *pc2 == JSOP_DEFAULT ||
                                 *pc2 == JSOP_CASEX || *pc2 == JSOP_DEFAULTX);
                    if (*pc2 == JSOP_DEFAULT || *pc2 == JSOP_DEFAULTX) {
                        /* End of cases, but count default as a case. */
                        off2 = 0;
                    } else {
                        sn = js_GetSrcNote(jp->script, pc2);
                        LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_PCDELTA);
                        off2 = js_GetSrcNoteOffset(sn, 0);
                    }
                }

                /*
                 * Allocate table and rescan the cases using their srcnotes,
                 * stashing each case's delta from switch top in table[i].key,
                 * and the distance to its statements in table[i].offset.
                 */
                table = (TableEntry *)
                    cx->malloc_((size_t)ncases * sizeof *table);
                if (!table)
                    return NULL;
                pc2 = pc;
                off2 = off;
                for (i = 0; i < ncases; i++) {
                    pc2 += off2;
                    LOCAL_ASSERT(*pc2 == JSOP_CASE || *pc2 == JSOP_DEFAULT ||
                                 *pc2 == JSOP_CASEX || *pc2 == JSOP_DEFAULTX);
                    caseOff = pc2 - pc;
                    table[i].key = INT_TO_JSVAL((jsint) caseOff);
                    table[i].offset = caseOff + GetJumpOffset(pc2, pc2);
                    if (*pc2 == JSOP_CASE || *pc2 == JSOP_CASEX) {
                        sn = js_GetSrcNote(jp->script, pc2);
                        LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_PCDELTA);
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
                off += GetJumpOffset(pc2, pc2);

                ok = DecompileSwitch(ss, table, (uintN)ncases, pc, len, off,
                                     JS_TRUE);
                cx->free_(table);
                if (!ok)
                    return NULL;
                todo = -2;
                break;
              }

              case JSOP_CASE:
              case JSOP_CASEX:
              {
                lval = POP_STR();
                if (!lval)
                    return NULL;
                js_printf(jp, "\tcase %s:\n", lval);
                todo = -2;
                break;
              }

              case JSOP_DEFFUN:
              case JSOP_DEFFUN_FC:
                LOAD_FUNCTION(0);
                todo = -2;
                goto do_function;
                break;

              case JSOP_TRAP:
                saveop = op = JS_GetTrapOpcode(cx, jp->script, pc);
                *pc = op;
                cs = &js_CodeSpec[op];
                len = cs->length;
                DECOMPILE_CODE(pc, len);
                if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
                    len += GetDecomposeLength(pc, js_CodeSpec[*pc].length);
                *pc = JSOP_TRAP;
                todo = -2;
                break;

              case JSOP_HOLE:
                todo = SprintPut(&ss->sprinter, "", 0);
                break;

              case JSOP_NEWINIT:
              {
                i = pc[1];
                LOCAL_ASSERT(i == JSProto_Array || i == JSProto_Object);

                todo = ss->sprinter.offset;
#if JS_HAS_SHARP_VARS
                op = (JSOp)pc[len];
                if (op == JSOP_SHARPINIT)
                    op = (JSOp)pc[len += JSOP_SHARPINIT_LENGTH];
                if (op == JSOP_DEFSHARP) {
                    pc += len;
                    cs = &js_CodeSpec[op];
                    len = cs->length;
                    if (Sprint(&ss->sprinter, "#%u=",
                               (unsigned) (jsint) GET_UINT16(pc + UINT16_LEN))
                        < 0) {
                        return NULL;
                    }
                }
#endif /* JS_HAS_SHARP_VARS */
                if (i == JSProto_Array) {
                    ++ss->inArrayInit;
                    if (SprintCString(&ss->sprinter, "[") < 0)
                        return NULL;
                } else {
                    if (SprintCString(&ss->sprinter, "{") < 0)
                        return NULL;
                }
                break;
              }

              case JSOP_NEWARRAY:
              {
                todo = ss->sprinter.offset;
                ++ss->inArrayInit;
                if (SprintCString(&ss->sprinter, "[") < 0)
                    return NULL;
                break;
              }

              case JSOP_NEWOBJECT:
              {
                todo = ss->sprinter.offset;
                if (SprintCString(&ss->sprinter, "{") < 0)
                    return NULL;
                break;
              }

              case JSOP_ENDINIT:
              {
                JSBool inArray;

                op = JSOP_NOP;           /* turn off parens */
                rval = POP_STR();
                sn = js_GetSrcNote(jp->script, pc);

                /* Skip any #n= prefix to find the opening bracket. */
                for (xval = rval; *xval != '[' && *xval != '{'; xval++)
                    continue;
                inArray = (*xval == '[');
                if (inArray)
                    --ss->inArrayInit;
                todo = Sprint(&ss->sprinter, "%s%s%c",
                              rval,
                              (sn && SN_TYPE(sn) == SRC_CONTINUE) ? ", " : "",
                              inArray ? ']' : '}');
                break;
              }

              {
                JSBool isFirst;
                const char *maybeComma;

              case JSOP_INITELEM:
                isFirst = IsInitializerOp(ss->opcodes[ss->top - 3]);

                /* Turn off most parens. */
                op = JSOP_SETNAME;
                rval = POP_STR();

                /* Turn off all parens for xval and lval, which we control. */
                op = JSOP_NOP;
                xval = POP_STR();
                lval = POP_STR();
                sn = js_GetSrcNote(jp->script, pc);

                if (sn && SN_TYPE(sn) == SRC_INITPROP) {
                    atom = NULL;
                    goto do_initprop;
                }
                maybeComma = isFirst ? "" : ", ";
                todo = Sprint(&ss->sprinter, sss_format,
                              lval,
                              maybeComma,
                              rval);
                break;

              case JSOP_INITPROP:
              case JSOP_INITMETHOD:
                LOAD_ATOM(0);
                xval = QuoteString(&ss->sprinter, atom,
                                   jschar(ATOM_IS_IDENTIFIER(atom) ? 0 : '\''));
                if (!xval)
                    return NULL;
                isFirst = IsInitializerOp(ss->opcodes[ss->top - 2]);
                rval = POP_STR();
                lval = POP_STR();
                /* fall through */

              do_initprop:
                maybeComma = isFirst ? "" : ", ";
                if (lastop == JSOP_GETTER || lastop == JSOP_SETTER) {
                    const char *end = rval + strlen(rval);

                    if (*rval == '(')
                        ++rval, --end;
                    LOCAL_ASSERT(strncmp(rval, js_function_str, 8) == 0);
                    LOCAL_ASSERT(rval[8] == ' ');
                    rval += 8 + 1;
                    LOCAL_ASSERT(*end ? *end == ')' : end[-1] == '}');
                    todo = Sprint(&ss->sprinter, "%s%s%s %s%s%.*s",
                                    lval,
                                    maybeComma,
                                    (lastop == JSOP_GETTER)
                                    ? js_get_str : js_set_str,
                                    xval,
                                    (rval[0] != '(') ? " " : "",
                                    end - rval, rval);
                } else {
                    todo = Sprint(&ss->sprinter, "%s%s%s: %s",
                                  lval, maybeComma, xval, rval);
                }
                break;
              }

#if JS_HAS_SHARP_VARS
              case JSOP_DEFSHARP:
                i = (jsint) GET_UINT16(pc + UINT16_LEN);
                rval = POP_STR();
                todo = Sprint(&ss->sprinter, "#%u=%s", (unsigned) i, rval);
                break;

              case JSOP_USESHARP:
                i = (jsint) GET_UINT16(pc + UINT16_LEN);
                todo = Sprint(&ss->sprinter, "#%u#", (unsigned) i);
                break;
#endif /* JS_HAS_SHARP_VARS */

              case JSOP_DEBUGGER:
                js_printf(jp, "\tdebugger;\n");
                todo = -2;
                break;

#if JS_HAS_XML_SUPPORT
              case JSOP_STARTXML:
              case JSOP_STARTXMLEXPR:
                inXML = op == JSOP_STARTXML;
                todo = -2;
                break;

              case JSOP_DEFXMLNS:
                rval = POP_STR();
                js_printf(jp, "\t%s %s %s = %s;\n",
                          js_default_str, js_xml_str, js_namespace_str, rval);
                todo = -2;
                break;

              case JSOP_ANYNAME:
                if (pc[JSOP_ANYNAME_LENGTH] == JSOP_TOATTRNAME) {
                    len += JSOP_TOATTRNAME_LENGTH;
                    todo = SprintPut(&ss->sprinter, "@*", 2);
                } else {
                    todo = SprintPut(&ss->sprinter, "*", 1);
                }
                break;

              case JSOP_QNAMEPART:
                LOAD_ATOM(0);
                if (pc[JSOP_QNAMEPART_LENGTH] == JSOP_TOATTRNAME) {
                    saveop = JSOP_TOATTRNAME;
                    len += JSOP_TOATTRNAME_LENGTH;
                    lval = "@";
                    goto do_qname;
                }
                goto do_name;

              case JSOP_QNAMECONST:
                LOAD_ATOM(0);
                rval = QuoteString(&ss->sprinter, atom, 0);
                if (!rval)
                    return NULL;
                RETRACT(&ss->sprinter, rval);
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s::%s", lval, rval);
                break;

              case JSOP_QNAME:
                rval = POP_STR();
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s::[%s]", lval, rval);
                break;

              case JSOP_TOATTRNAME:
                op = JSOP_NOP;           /* turn off parens */
                rval = POP_STR();
                todo = Sprint(&ss->sprinter, "@[%s]", rval);
                break;

              case JSOP_TOATTRVAL:
                todo = -2;
                break;

              case JSOP_ADDATTRNAME:
                rval = POP_STR();
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s %s", lval, rval);
                /* This gets reset by all XML tag expressions. */
                quoteAttr = JS_TRUE;
                break;

              case JSOP_ADDATTRVAL:
                rval = POP_STR();
                lval = POP_STR();
                if (quoteAttr)
                    todo = Sprint(&ss->sprinter, "%s=\"%s\"", lval, rval);
                else
                    todo = Sprint(&ss->sprinter, "%s=%s", lval, rval);
                break;

              case JSOP_BINDXMLNAME:
                /* Leave the name stacked and push a dummy string. */
                todo = Sprint(&ss->sprinter, "");
                break;

              case JSOP_SETXMLNAME:
                /* Pop the r.h.s., the dummy string, and the name. */
                rval = POP_STR();
                (void) PopOff(ss, op);
                lval = POP_STR();
                goto do_setlval;

              case JSOP_XMLELTEXPR:
              case JSOP_XMLTAGEXPR:
                todo = Sprint(&ss->sprinter, "{%s}", POP_STR());
                inXML = JS_TRUE;
                /* If we're an attribute value, we shouldn't quote this. */
                quoteAttr = JS_FALSE;
                break;

              case JSOP_TOXMLLIST:
                op = JSOP_NOP;           /* turn off parens */
                todo = Sprint(&ss->sprinter, "<>%s</>", POP_STR());
                inXML = JS_FALSE;
                break;

              case JSOP_TOXML:
              case JSOP_CALLXMLNAME:
              case JSOP_XMLNAME:
              case JSOP_FILTER:
                /* These ops indicate the end of XML expressions. */
                inXML = JS_FALSE;
                todo = -2;
                break;

              case JSOP_ENDFILTER:
                rval = POP_STR();
                PROPAGATE_CALLNESS();
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s.(%s)", lval, rval);
                break;

              case JSOP_DESCENDANTS:
                rval = POP_STR();
                PROPAGATE_CALLNESS();
                lval = POP_STR();
                todo = Sprint(&ss->sprinter, "%s..%s", lval, rval);
                break;

              case JSOP_XMLCDATA:
                LOAD_ATOM(0);
                todo = SprintPut(&ss->sprinter, "<![CDATA[", 9);
                if (!QuoteString(&ss->sprinter, atom, DONT_ESCAPE))
                    return NULL;
                SprintPut(&ss->sprinter, "]]>", 3);
                break;

              case JSOP_XMLCOMMENT:
                LOAD_ATOM(0);
                todo = SprintPut(&ss->sprinter, "<!--", 4);
                if (!QuoteString(&ss->sprinter, atom, DONT_ESCAPE))
                    return NULL;
                SprintPut(&ss->sprinter, "-->", 3);
                break;

              case JSOP_XMLPI:
                LOAD_ATOM(0);
                rval = JS_strdup(cx, POP_STR());
                if (!rval)
                    return NULL;
                todo = SprintPut(&ss->sprinter, "<?", 2);
                ok = QuoteString(&ss->sprinter, atom, 0) &&
                     (*rval == '\0' ||
                      (SprintPut(&ss->sprinter, " ", 1) >= 0 &&
                       SprintCString(&ss->sprinter, rval)));
                cx->free_((char *)rval);
                if (!ok)
                    return NULL;
                SprintPut(&ss->sprinter, "?>", 2);
                break;

              case JSOP_GETFUNNS:
                todo = SprintPut(&ss->sprinter, js_function_str, 8);
                break;
#endif /* JS_HAS_XML_SUPPORT */

              default:
                todo = -2;
                break;
            }
        }

        if (todo < 0) {
            /* -2 means "don't push", -1 means reported error. */
            if (todo == -1)
                return NULL;
        } else {
            if (!PushOff(ss, todo, saveop))
                return NULL;
        }

        if (cs->format & JOF_CALLOP) {
            todo = Sprint(&ss->sprinter, "");
            if (todo < 0 || !PushOff(ss, todo, saveop))
                return NULL;
        }

        pc += len;
    }

/*
 * Undefine local macros.
 */
#undef inXML
#undef DECOMPILE_CODE
#undef NEXT_OP
#undef TOP_STR
#undef POP_STR
#undef POP_STR_PREC
#undef LOCAL_ASSERT
#undef ATOM_IS_IDENTIFIER
#undef GET_QUOTE_AND_FMT
#undef GET_ATOM_QUOTE_AND_FMT

    return pc;
}

static JSBool
DecompileCode(JSPrinter *jp, JSScript *script, jsbytecode *pc, uintN len,
              uintN pcdepth)
{
    uintN depth, i;
    SprintStack ss;
    JSContext *cx;
    JSScript *oldscript;
    char *last;

    depth = StackDepth(script);
    JS_ASSERT(pcdepth <= depth);
    cx = jp->sprinter.context;

    AutoScriptUntrapper untrapper(cx, script, &pc);

    /* Initialize a sprinter for use with the offset stack. */
    LifoAllocScope las(&cx->tempLifoAlloc());
    if (!InitSprintStack(cx, &ss, jp, depth))
        return false;

    /*
     * If we are called from js_DecompileValueGenerator with a portion of
     * script's bytecode that starts with a non-zero model stack depth given
     * by pcdepth, attempt to initialize the missing string offsets in ss to
     * |spindex| negative indexes from fp->sp for the activation fp in which
     * the error arose.
     *
     * See js_DecompileValueGenerator for how its |spindex| parameter is used,
     * and see also GetOff, which makes use of the ss.offsets[i] < -1 that are
     * potentially stored below.
     */
    ss.top = pcdepth;
    if (pcdepth != 0) {
        for (i = 0; i < pcdepth; i++) {
            ss.offsets[i] = -2 - (ptrdiff_t)i;
            ss.opcodes[i] = *jp->pcstack[i];
        }
    }

    /* Call recursive subroutine to do the hard work. */
    oldscript = jp->script;
    jp->script = script;
    bool ok = Decompile(&ss, pc, len, JSOP_NOP) != NULL;
    jp->script = oldscript;

    /* If the given code didn't empty the stack, do it now. */
    if (ok && ss.top) {
        do {
            last = OFF2STR(&ss.sprinter, PopOff(&ss, JSOP_POP));
        } while (ss.top > pcdepth);
        js_printf(jp, "%s", last);
    }

    return ok;
}

/*
 * Decompile a function body, expression closure expression, or complete
 * script. Start at |pc|; go to the end of |script|. Include a directive
 * prologue, if appropriate.
 */
static JSBool
DecompileBody(JSPrinter *jp, JSScript *script, jsbytecode *pc)
{
    /* Print a strict mode code directive, if needed. */
    if (script->strictModeCode && !jp->strict) {
        if (jp->fun && (jp->fun->flags & JSFUN_EXPR_CLOSURE)) {
            /*
             * We have no syntax for strict function expressions;
             * at least give a hint.
             */
            js_printf(jp, "\t/* use strict */ \n");
        } else {
            js_printf(jp, "\t\"use strict\";\n");
        }
        jp->strict = true;
    }

    jsbytecode *end = script->code + script->length;
    return DecompileCode(jp, script, pc, end - pc, 0);
}

JSBool
js_DecompileScript(JSPrinter *jp, JSScript *script)
{
    return DecompileBody(jp, script, script->code);
}

JSString *
js_DecompileToString(JSContext *cx, const char *name, JSFunction *fun,
                     uintN indent, JSBool pretty, JSBool grouped, JSBool strict,
                     JSDecompilerPtr decompiler)
{
    JSPrinter *jp;
    JSString *str;

    jp = js_NewPrinter(cx, name, fun, indent, pretty, grouped, strict);
    if (!jp)
        return NULL;
    if (decompiler(jp))
        str = js_GetPrinterOutput(jp);
    else
        str = NULL;
    js_DestroyPrinter(jp);
    return str;
}

static const char native_code_str[] = "\t[native code]\n";

JSBool
js_DecompileFunctionBody(JSPrinter *jp)
{
    JSScript *script;

    JS_ASSERT(jp->fun);
    JS_ASSERT(!jp->script);
    if (!jp->fun->isInterpreted()) {
        js_printf(jp, native_code_str);
        return JS_TRUE;
    }

    script = jp->fun->script();
    return DecompileBody(jp, script, script->code);
}

JSBool
js_DecompileFunction(JSPrinter *jp)
{
    JSFunction *fun;
    uintN i;
    JSAtom *param;
    jsbytecode *pc, *endpc;
    JSBool ok;

    fun = jp->fun;
    JS_ASSERT(fun);
    JS_ASSERT(!jp->script);

    /*
     * If pretty, conform to ECMA-262 Edition 3, 15.3.4.2, by decompiling a
     * FunctionDeclaration.  Otherwise, check the JSFUN_LAMBDA flag and force
     * an expression by parenthesizing.
     */
    if (jp->pretty) {
        js_printf(jp, "\t");
    } else {
        if (!jp->grouped && (fun->flags & JSFUN_LAMBDA))
            js_puts(jp, "(");
    }

    js_printf(jp, "%s ", js_function_str);
    if (fun->atom && !QuoteString(&jp->sprinter, fun->atom, 0))
        return JS_FALSE;
    js_puts(jp, "(");

    if (!fun->isInterpreted()) {
        js_printf(jp, ") {\n");
        jp->indent += 4;
        js_printf(jp, native_code_str);
        jp->indent -= 4;
        js_printf(jp, "\t}");
    } else {
        JSScript *script = fun->script();
#if JS_HAS_DESTRUCTURING
        SprintStack ss;
#endif

        /* Print the parameters. */
        pc = script->main();
        AutoScriptUntrapper untrapper(jp->sprinter.context, script, &pc);
        endpc = pc + script->length;
        ok = JS_TRUE;

#if JS_HAS_DESTRUCTURING
        ss.printer = NULL;
        jp->script = script;
        LifoAllocScope las(&jp->sprinter.context->tempLifoAlloc());
#endif

        for (i = 0; i < fun->nargs; i++) {
            if (i > 0)
                js_puts(jp, ", ");

            param = GetArgOrVarAtom(jp, i);

#if JS_HAS_DESTRUCTURING
#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, JS_FALSE)

            if (!param) {
                ptrdiff_t todo;
                const char *lval;

                LOCAL_ASSERT(*pc == JSOP_GETARG);
                pc += JSOP_GETARG_LENGTH;
                LOCAL_ASSERT(*pc == JSOP_DUP);
                if (!ss.printer) {
                    ok = InitSprintStack(jp->sprinter.context, &ss, jp, StackDepth(script));
                    if (!ok)
                        break;
                }
                pc = DecompileDestructuring(&ss, pc, endpc);
                if (!pc) {
                    ok = JS_FALSE;
                    break;
                }
                LOCAL_ASSERT(*pc == JSOP_POP);
                pc += JSOP_POP_LENGTH;
                lval = PopStr(&ss, JSOP_NOP);
                todo = SprintCString(&jp->sprinter, lval);
                if (todo < 0) {
                    ok = JS_FALSE;
                    break;
                }
                continue;
            }

#undef LOCAL_ASSERT
#endif

            if (!QuoteString(&jp->sprinter, param, 0)) {
                ok = JS_FALSE;
                break;
            }
        }

#if JS_HAS_DESTRUCTURING
        jp->script = NULL;
        las.releaseEarly();
#endif
        if (!ok)
            return JS_FALSE;
        js_printf(jp, ") ");
        if (!(fun->flags & JSFUN_EXPR_CLOSURE)) {
            js_printf(jp, "{\n");
            jp->indent += 4;
        }

        ok = DecompileBody(jp, script, pc);
        if (!ok)
            return JS_FALSE;

        if (!(fun->flags & JSFUN_EXPR_CLOSURE)) {
            jp->indent -= 4;
            js_printf(jp, "\t}");
        }
    }

    if (!jp->pretty && !jp->grouped && (fun->flags & JSFUN_LAMBDA))
        js_puts(jp, ")");

    return JS_TRUE;
}

char *
js_DecompileValueGenerator(JSContext *cx, intN spindex, jsval v,
                           JSString *fallback)
{
    StackFrame *fp;
    JSScript *script;
    jsbytecode *pc;

    JS_ASSERT(spindex < 0 ||
              spindex == JSDVG_IGNORE_STACK ||
              spindex == JSDVG_SEARCH_STACK);

    LeaveTrace(cx);
    
    if (!cx->hasfp() || !cx->fp()->isScriptFrame())
        goto do_fallback;

    fp = js_GetTopStackFrame(cx, FRAME_EXPAND_ALL);
    script = fp->script();
    pc = fp->hasImacropc() ? fp->imacropc() : cx->regs().pc;
    JS_ASSERT(script->code <= pc && pc < script->code + script->length);

    if (pc < script->main())
        goto do_fallback;
    
    if (spindex != JSDVG_IGNORE_STACK) {
        jsbytecode **pcstack;

        /*
         * Prepare computing pcstack containing pointers to opcodes that
         * populated interpreter's stack with its current content.
         */
        pcstack = (jsbytecode **)
                  cx->malloc_(StackDepth(script) * sizeof *pcstack);
        if (!pcstack)
            return NULL;
        intN pcdepth = ReconstructPCStack(cx, script, pc, pcstack);
        if (pcdepth < 0)
            goto release_pcstack;

        if (spindex != JSDVG_SEARCH_STACK) {
            JS_ASSERT(spindex < 0);
            pcdepth += spindex;
            if (pcdepth < 0)
                goto release_pcstack;
            pc = pcstack[pcdepth];
        } else {
            /*
             * We search from fp->sp to base to find the most recently
             * calculated value matching v under assumption that it is
             * it that caused exception, see bug 328664.
             */
            Value *stackBase = fp->base();
            Value *sp = cx->regs().sp;
            do {
                if (sp == stackBase) {
                    pcdepth = -1;
                    goto release_pcstack;
                }
            } while (*--sp != v);

            /*
             * The value may have come from beyond stackBase + pcdepth, meaning
             * that it came from a temporary slot pushed by the interpreter or
             * arguments pushed for an Invoke call. Only update pc if beneath
             * stackBase + pcdepth. If above, the value may or may not be
             * produced by the current pc. Since it takes a fairly contrived
             * combination of calls to produce a situation where this is not
             * what we want, we just use the current pc.
             */
            if (sp < stackBase + pcdepth)
                pc = pcstack[sp - stackBase];
        }

      release_pcstack:
        cx->free_(pcstack);
        if (pcdepth < 0)
            goto do_fallback;
    }

    {
        jsbytecode* basepc = cx->regs().pc;
        jsbytecode* savedImacropc = fp->maybeImacropc();
        if (savedImacropc) {
            cx->regs().pc = savedImacropc;
            fp->clearImacropc();
        }

        /*
         * FIXME: bug 489843. Stack reconstruction may have returned a pc
         * value *inside* an imacro; this would confuse the decompiler.
         */
        char *name;
        if (savedImacropc && size_t(pc - script->code) >= script->length)
            name = FAILED_EXPRESSION_DECOMPILER;
        else
            name = DecompileExpression(cx, script, fp->maybeFun(), pc);

        if (savedImacropc) {
            cx->regs().pc = basepc;
            fp->setImacropc(savedImacropc);
        }

        if (name != FAILED_EXPRESSION_DECOMPILER)
            return name;
    }

  do_fallback:
    if (!fallback) {
        fallback = js_ValueToSource(cx, v);
        if (!fallback)
            return NULL;
    }
    size_t length = fallback->length();
    const jschar *chars = fallback->getChars(cx);
    if (!chars)
        return NULL;
    return DeflateString(cx, chars, length);
}

static char *
DecompileExpression(JSContext *cx, JSScript *script, JSFunction *fun,
                    jsbytecode *pc)
{
    JSOp op;
    const JSCodeSpec *cs;
    jsbytecode *begin, *end;
    jssrcnote *sn;
    ptrdiff_t len;
    jsbytecode **pcstack;
    intN pcdepth;
    JSPrinter *jp;
    char *name;

    JS_ASSERT(script->code <= pc && pc < script->code + script->length);

    pcstack = NULL;
    AutoScriptUntrapper untrapper(cx, script, &pc);
    op = (JSOp) *pc;

    /* None of these stack-writing ops generates novel values. */
    JS_ASSERT(op != JSOP_CASE && op != JSOP_CASEX &&
              op != JSOP_DUP && op != JSOP_DUP2);

    /* JSOP_PUSH is used to generate undefined for group assignment holes. */
    if (op == JSOP_PUSH) {
        name = JS_strdup(cx, js_undefined_str);
        goto out;
    }

    /*
     * |this| could convert to a very long object initialiser, so cite it by
     * its keyword name instead.
     */
    if (op == JSOP_THIS) {
        name = JS_strdup(cx, js_this_str);
        goto out;
    }

    /*
     * JSOP_BINDNAME is special: it generates a value, the base object of a
     * reference.  But if it is the generating op for a diagnostic produced by
     * js_DecompileValueGenerator, the name being bound is irrelevant.  Just
     * fall back to the base object.
     */
    if (op == JSOP_BINDNAME) {
        name = FAILED_EXPRESSION_DECOMPILER;
        goto out;
    }

    /* NAME ops are self-contained, others require left or right context. */
    cs = &js_CodeSpec[op];
    begin = pc;
    end = pc + cs->length;
    switch (JOF_MODE(cs->format)) {
      case JOF_PROP:
      case JOF_ELEM:
      case JOF_XMLNAME:
      case 0:
        sn = js_GetSrcNote(script, pc);
        if (!sn) {
            name = FAILED_EXPRESSION_DECOMPILER;
            goto out;
        }
        switch (SN_TYPE(sn)) {
          case SRC_PCBASE:
            begin -= js_GetSrcNoteOffset(sn, 0);
            break;
          case SRC_PCDELTA:
            end = begin + js_GetSrcNoteOffset(sn, 0);
            begin += cs->length;
            break;
          default:
            name = FAILED_EXPRESSION_DECOMPILER;
            goto out;
        }
        break;
      default:;
    }
    len = end - begin;
    if (len <= 0) {
        name = FAILED_EXPRESSION_DECOMPILER;
        goto out;
    }

    pcstack = (jsbytecode **)
              cx->malloc_(StackDepth(script) * sizeof *pcstack);
    if (!pcstack) {
        name = NULL;
        goto out;
    }

    MUST_FLOW_THROUGH("out");
    pcdepth = ReconstructPCStack(cx, script, begin, pcstack);
    if (pcdepth < 0) {
         name = FAILED_EXPRESSION_DECOMPILER;
         goto out;
    }

    name = NULL;
    jp = js_NewPrinter(cx, "js_DecompileValueGenerator", fun, 0,
                       false, false, false);
    if (jp) {
        jp->dvgfence = end;
        jp->pcstack = pcstack;
        if (DecompileCode(jp, script, begin, (uintN) len, (uintN) pcdepth)) {
            name = (jp->sprinter.base) ? jp->sprinter.base : (char *) "";
            name = JS_strdup(cx, name);
        }
        js_DestroyPrinter(jp);
    }

out:
    cx->free_(pcstack);
    return name;
}

uintN
js_ReconstructStackDepth(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return ReconstructPCStack(cx, script, pc, NULL);
}

#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, -1);

static intN
SimulateOp(JSContext *cx, JSScript *script, JSOp op, const JSCodeSpec *cs,
           jsbytecode *pc, jsbytecode **pcstack, uintN &pcdepth)
{
    uintN nuses = js_GetStackUses(cs, op, pc);
    uintN ndefs = js_GetStackDefs(cx, cs, op, script, pc);
    LOCAL_ASSERT(pcdepth >= nuses);
    pcdepth -= nuses;
    LOCAL_ASSERT(pcdepth + ndefs <= StackDepth(script));

    /*
     * Fill the slots that the opcode defines withs its pc unless it just
     * reshuffles the stack. In the latter case we want to preserve the
     * opcode that generated the original value.
     */
    switch (op) {
      default:
        if (pcstack) {
            for (uintN i = 0; i != ndefs; ++i)
                pcstack[pcdepth + i] = pc;
        }
        break;

      case JSOP_CASE:
      case JSOP_CASEX:
        /* Keep the switch value. */
        JS_ASSERT(ndefs == 1);
        break;

      case JSOP_DUP:
        JS_ASSERT(ndefs == 2);
        if (pcstack)
            pcstack[pcdepth + 1] = pcstack[pcdepth];
        break;

      case JSOP_DUP2:
        JS_ASSERT(ndefs == 4);
        if (pcstack) {
            pcstack[pcdepth + 2] = pcstack[pcdepth];
            pcstack[pcdepth + 3] = pcstack[pcdepth + 1];
        }
        break;

      case JSOP_SWAP:
        JS_ASSERT(ndefs == 2);
        if (pcstack) {
            jsbytecode *tmp = pcstack[pcdepth + 1];
            pcstack[pcdepth + 1] = pcstack[pcdepth];
            pcstack[pcdepth] = tmp;
        }
        break;
    }
    pcdepth += ndefs;
    return pcdepth;
}

#ifdef JS_TRACER

#undef LOCAL_ASSERT
#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_CUSTOM(expr, goto failure);

static intN
SimulateImacroCFG(JSContext *cx, JSScript *script,
                  uintN pcdepth, jsbytecode *pc, jsbytecode *target,
                  jsbytecode **pcstack)
{
    size_t nbytes = 0;
    jsbytecode** tmp_pcstack = NULL;
    if (pcstack) {
        nbytes = StackDepth(script) * sizeof *pcstack;
        tmp_pcstack = (jsbytecode **) cx->malloc_(nbytes);
        if (!tmp_pcstack)
            return -1;
        memcpy(tmp_pcstack, pcstack, nbytes);
    }

    ptrdiff_t oplen;
    for (; pc < target; pc += oplen) {
        JSOp op = js_GetOpcode(cx, script, pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        oplen = cs->length;
        if (oplen < 0)
            oplen = js_GetVariableBytecodeLength(pc);

        if (SimulateOp(cx, script, op, cs, pc, tmp_pcstack, pcdepth) < 0)
            goto failure;

        uint32 type = cs->format & JOF_TYPEMASK;
        if (type == JOF_JUMP || type == JOF_JUMPX) {
            ptrdiff_t jmpoff = (type == JOF_JUMP) ? GET_JUMP_OFFSET(pc)
                                                  : GET_JUMPX_OFFSET(pc);
            LOCAL_ASSERT(jmpoff >= 0);
            intN tmp_pcdepth = SimulateImacroCFG(cx, script, pcdepth, pc + jmpoff,
                                                 target, tmp_pcstack);
            if (tmp_pcdepth >= 0) {
                pcdepth = uintN(tmp_pcdepth);
                goto success;
            }

            if (op == JSOP_GOTO || op == JSOP_GOTOX)
                goto failure;
        }
    }

    if (pc > target)
        goto failure;

    LOCAL_ASSERT(pc == target);

  success:
    if (tmp_pcstack) {
        memcpy(pcstack, tmp_pcstack, nbytes);
        cx->free_(tmp_pcstack);
    }
    return pcdepth;

  failure:
    if (tmp_pcstack)
        cx->free_(tmp_pcstack);
    return -1;
}

#undef LOCAL_ASSERT
#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, -1);

static intN
ReconstructImacroPCStack(JSContext *cx, JSScript *script,
                         jsbytecode *imacstart, jsbytecode *target,
                         jsbytecode **pcstack)
{
    /*
     * Begin with a recursive call back to ReconstructPCStack to pick up
     * the state-of-the-world at the *start* of the imacro.
     */
    StackFrame *fp = js_GetScriptedCaller(cx, NULL);
    JS_ASSERT(fp->hasImacropc());
    intN pcdepth = ReconstructPCStack(cx, script, fp->imacropc(), pcstack);
    if (pcdepth < 0)
        return pcdepth;
    return SimulateImacroCFG(cx, script, pcdepth, imacstart, target, pcstack);
}

extern jsbytecode* js_GetImacroStart(jsbytecode* pc);
#endif

static intN
ReconstructPCStack(JSContext *cx, JSScript *script, jsbytecode *target,
                   jsbytecode **pcstack)
{
    /*
     * Walk forward from script->main and compute the stack depth and stack of
     * operand-generating opcode PCs in pcstack.
     *
     * FIXME: Code to compute oplen copied from js_Disassemble1 and reduced.
     * FIXME: Optimize to use last empty-stack sequence point.
     */
#ifdef JS_TRACER
    jsbytecode *imacstart = js_GetImacroStart(target);

    if (imacstart)
        return ReconstructImacroPCStack(cx, script, imacstart, target, pcstack);
#endif

    LOCAL_ASSERT(script->code <= target && target < script->code + script->length);
    jsbytecode *pc = script->code;
    uintN pcdepth = 0;
    ptrdiff_t oplen;
    for (; pc < target; pc += oplen) {
        JSOp op = js_GetOpcode(cx, script, pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        oplen = cs->length;
        if (oplen < 0)
            oplen = js_GetVariableBytecodeLength(pc);

        if (cs->format & JOF_DECOMPOSE)
            continue;

        /*
         * A (C ? T : E) expression requires skipping either T (if target is in
         * E) or both T and E (if target is after the whole expression) before
         * adjusting pcdepth based on the JSOP_IFEQ or JSOP_IFEQX at pc that
         * tests condition C.  We know that the stack depth can't change from
         * what it was with C on top of stack.
         */
        jssrcnote *sn = js_GetSrcNote(script, pc);
        if (sn && SN_TYPE(sn) == SRC_COND) {
            ptrdiff_t jmpoff = js_GetSrcNoteOffset(sn, 0);
            if (pc + jmpoff < target) {
                pc += jmpoff;
                op = js_GetOpcode(cx, script, pc);
                JS_ASSERT(op == JSOP_GOTO || op == JSOP_GOTOX);
                cs = &js_CodeSpec[op];
                oplen = cs->length;
                JS_ASSERT(oplen > 0);
                ptrdiff_t jmplen = GetJumpOffset(pc, pc);
                if (pc + jmplen < target) {
                    oplen = (uintN) jmplen;
                    continue;
                }

                /*
                 * Ok, target lies in E. Manually pop C off the model stack,
                 * since we have moved beyond the IFEQ now.
                 */
                LOCAL_ASSERT(pcdepth != 0);
                --pcdepth;
            }
        }

        /* Ignore early-exit code, which is annotated SRC_HIDDEN. */
        if (sn && SN_TYPE(sn) == SRC_HIDDEN)
            continue;

        if (SimulateOp(cx, script, op, cs, pc, pcstack, pcdepth) < 0)
            return -1;

    }
    LOCAL_ASSERT(pc == target);
    return pcdepth;
}

#undef LOCAL_ASSERT
#undef LOCAL_ASSERT_RV

namespace js {

bool
CallResultEscapes(jsbytecode *pc)
{
    /*
     * If we see any of these sequences, the result is unused:
     * - call / pop
     * - call / trace / pop
     *
     * If we see any of these sequences, the result is only tested for nullness:
     * - call / ifeq
     * - call / trace / ifeq
     * - call / not / ifeq
     * - call / trace / not / ifeq
     */

    if (*pc != JSOP_CALL)
        return true;

    pc += JSOP_CALL_LENGTH;

    if (*pc == JSOP_TRACE)
        pc += JSOP_TRACE_LENGTH;

    if (*pc == JSOP_POP)
        return false;

    if (*pc == JSOP_NOT)
        pc += JSOP_NOT_LENGTH;

    return (*pc != JSOP_IFEQ);
}

size_t
GetBytecodeLength(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSOp op = js_GetOpcode(cx, script, pc);
    JS_ASSERT(op < JSOP_LIMIT);
    JS_ASSERT(op != JSOP_TRAP);
    if (js_CodeSpec[op].length != -1)
        return js_CodeSpec[op].length;
    return js_GetVariableBytecodeLength(op, pc);
}

extern bool
IsValidBytecodeOffset(JSContext *cx, JSScript *script, size_t offset)
{
    // This could be faster (by following jump instructions if the target is <= offset).
    for (BytecodeRange r(cx, script); !r.empty(); r.popFront()) {
        size_t here = r.frontOffset();
        if (here >= offset)
            return here == offset;
    }
    return false;
}

} // namespace js
