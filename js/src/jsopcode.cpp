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

#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#include "ds/Sort.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/TokenStream.h"
#include "vm/Debugger.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"

#include "vm/RegExpObject-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;

/*
 * Index limit must stay within 32 bits.
 */
JS_STATIC_ASSERT(sizeof(uint32_t) * JS_BITS_PER_BYTE >= INDEX_LIMIT_LOG2 + 1);

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

unsigned js_NumCodeSpecs = JS_ARRAY_LENGTH(js_CodeSpec);

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

/*
 * Array of JS bytecode names used by PC count JSON, DEBUG-only js_Disassemble
 * and JIT debug spew.
 */
const char *js_CodeName[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    name,
#include "jsopcode.tbl"
#undef OPDEF
};

/************************************************************************/

#define COUNTS_LEN 16

typedef Vector<char, 8> DupBuffer;

static bool
Dup(const char *chars, DupBuffer *cb)
{
    return cb->append(chars, strlen(chars) + 1);
}

size_t
js_GetVariableBytecodeLength(jsbytecode *pc)
{
    unsigned ncases;
    int32_t low, high;

    JSOp op = JSOp(*pc);
    JS_ASSERT(js_CodeSpec[op].length == -1);
    switch (op) {
      case JSOP_TABLESWITCH:
        /* Structure: default-jump case-low case-high case1-jump ... */
        pc += JUMP_OFFSET_LEN;
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
        ncases = (unsigned)(high - low + 1);
        return 1 + 3 * JUMP_OFFSET_LEN + ncases * JUMP_OFFSET_LEN;

      default:
        /* Structure: default-jump case-count (case1-value case1-jump) ... */
        JS_ASSERT(op == JSOP_LOOKUPSWITCH);
        pc += JUMP_OFFSET_LEN;
        ncases = GET_UINT16(pc);
        return 1 + JUMP_OFFSET_LEN + UINT16_LEN + ncases * (UINT32_INDEX_LEN + JUMP_OFFSET_LEN);
    }
}

static uint32_t
NumBlockSlots(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(*pc == JSOP_ENTERBLOCK || *pc == JSOP_ENTERLET0 || *pc == JSOP_ENTERLET1);
    JS_STATIC_ASSERT(JSOP_ENTERBLOCK_LENGTH == JSOP_ENTERLET0_LENGTH);
    JS_STATIC_ASSERT(JSOP_ENTERBLOCK_LENGTH == JSOP_ENTERLET1_LENGTH);

    return script->getObject(GET_UINT32_INDEX(pc))->asStaticBlock().slotCount();
}

unsigned
js::StackUses(JSScript *script, jsbytecode *pc)
{
    JSOp op = (JSOp) *pc;
    const JSCodeSpec &cs = js_CodeSpec[op];
    if (cs.nuses >= 0)
        return cs.nuses;

    JS_ASSERT(js_CodeSpec[op].nuses == -1);
    switch (op) {
      case JSOP_POPN:
        return GET_UINT16(pc);
      case JSOP_LEAVEBLOCK:
        return GET_UINT16(pc);
      case JSOP_LEAVEBLOCKEXPR:
        return GET_UINT16(pc) + 1;
      case JSOP_ENTERLET0:
        return NumBlockSlots(script, pc);
      case JSOP_ENTERLET1:
        return NumBlockSlots(script, pc) + 1;
      default:
        /* stack: fun, this, [argc arguments] */
        JS_ASSERT(op == JSOP_NEW || op == JSOP_CALL || op == JSOP_EVAL ||
                  op == JSOP_FUNCALL || op == JSOP_FUNAPPLY);
        return 2 + GET_ARGC(pc);
    }
}

unsigned
js::StackDefs(JSScript *script, jsbytecode *pc)
{
    JSOp op = (JSOp) *pc;
    const JSCodeSpec &cs = js_CodeSpec[op];
    if (cs.ndefs >= 0)
        return cs.ndefs;

    uint32_t n = NumBlockSlots(script, pc);
    return op == JSOP_ENTERLET1 ? n + 1 : n;
}

static const char * countBaseNames[] = {
    "interp",
    "mjit",
    "mjit_calls",
    "mjit_code",
    "mjit_pics"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) == OpcodeCounts::BASE_COUNT);

static const char * countAccessNames[] = {
    "infer_mono",
    "infer_di",
    "infer_poly",
    "infer_barrier",
    "infer_nobarrier",
    "observe_undefined",
    "observe_null",
    "observe_boolean",
    "observe_int32",
    "observe_double",
    "observe_string",
    "observe_object"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) +
                 JS_ARRAY_LENGTH(countAccessNames) == OpcodeCounts::ACCESS_COUNT);

static const char * countElementNames[] = {
    "id_int",
    "id_double",
    "id_other",
    "id_unknown",
    "elem_typed",
    "elem_packed",
    "elem_dense",
    "elem_other"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) +
                 JS_ARRAY_LENGTH(countAccessNames) +
                 JS_ARRAY_LENGTH(countElementNames) == OpcodeCounts::ELEM_COUNT);

static const char * countPropertyNames[] = {
    "prop_static",
    "prop_definite",
    "prop_other"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) +
                 JS_ARRAY_LENGTH(countAccessNames) +
                 JS_ARRAY_LENGTH(countPropertyNames) == OpcodeCounts::PROP_COUNT);

static const char * countArithNames[] = {
    "arith_int",
    "arith_double",
    "arith_other",
    "arith_unknown",
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) +
                 JS_ARRAY_LENGTH(countArithNames) == OpcodeCounts::ARITH_COUNT);

/* static */ const char *
OpcodeCounts::countName(JSOp op, size_t which)
{
    JS_ASSERT(which < numCounts(op));

    if (which < BASE_COUNT)
        return countBaseNames[which];

    if (accessOp(op)) {
        if (which < ACCESS_COUNT)
            return countAccessNames[which - BASE_COUNT];
        if (elementOp(op))
            return countElementNames[which - ACCESS_COUNT];
        if (propertyOp(op))
            return countPropertyNames[which - ACCESS_COUNT];
        JS_NOT_REACHED("bad op");
        return NULL;
    }

    if (arithOp(op))
        return countArithNames[which - BASE_COUNT];

    JS_NOT_REACHED("bad op");
    return NULL;
}

#ifdef DEBUG

JS_FRIEND_API(void)
js_DumpPCCounts(JSContext *cx, JSScript *script, js::Sprinter *sp)
{
    JS_ASSERT(script->pcCounters);

    jsbytecode *pc = script->code;
    while (pc < script->code + script->length) {
        JSOp op = JSOp(*pc);

        int len = js_CodeSpec[op].length;
        jsbytecode *next = (len != -1) ? pc + len : pc + js_GetVariableBytecodeLength(pc);

        if (!js_Disassemble1(cx, script, pc, pc - script->code, true, sp))
            return;

        size_t total = OpcodeCounts::numCounts(op);
        double *raw = script->getCounts(pc).rawCounts();

        Sprint(sp, "                  {");
        bool printed = false;
        for (size_t i = 0; i < total; i++) {
            double val = raw[i];
            if (val) {
                if (printed)
                    Sprint(sp, ", ");
                Sprint(sp, "\"%s\": %.0f", OpcodeCounts::countName(op, i), val);
                printed = true;
            }
        }
        Sprint(sp, "}\n");

        pc = next;
    }
}

/*
 * If pc != NULL, include a prefix indicating whether the PC is at the current line.
 * If counts != NULL, include a counter of the number of times each op was executed.
 */
JS_FRIEND_API(JSBool)
js_DisassembleAtPC(JSContext *cx, JSScript *script, JSBool lines, jsbytecode *pc, Sprinter *sp)
{
    jsbytecode *next, *end;
    unsigned len;

    sp->put("loc   ");
    if (lines)
        sp->put("line");
    sp->put("  op\n");
    sp->put("----- ");
    if (lines)
        sp->put("----");
    sp->put("  --\n");

    next = script->code;
    end = next + script->length;
    while (next < end) {
        if (next == script->main())
            sp->put("main:\n");
        if (pc != NULL) {
            if (pc == next)
                sp->put("--> ");
            else
                sp->put("    ");
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
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return JS_FALSE;
    JSBool ok = js_DisassembleAtPC(cx, cx->fp()->script(), true, cx->regs().pc, &sprinter);
    fprintf(stdout, "%s", sprinter.string());
    return ok;
}

JSBool
js_DumpScript(JSContext *cx, JSScript *script)
{
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return JS_FALSE;
    JSBool ok = js_Disassemble(cx, script, true, &sprinter);
    fprintf(stdout, "%s", sprinter.string());
    return ok;
}

static char *
QuoteString(Sprinter *sp, JSString *str, uint32_t quote);

static bool
ToDisassemblySource(JSContext *cx, jsval v, JSAutoByteString *bytes)
{
    if (JSVAL_IS_STRING(v)) {
        Sprinter sprinter(cx);
        if (!sprinter.init())
            return false;
        char *nbytes = QuoteString(&sprinter, JSVAL_TO_STRING(v), '"');
        if (!nbytes)
            return false;
        nbytes = JS_sprintf_append(NULL, "%s", nbytes);
        if (!nbytes)
            return false;
        bytes->initBytes(nbytes);
        return true;
    }

    if (cx->runtime->gcRunning || cx->runtime->noGCOrAllocationCheck) {
        char *source = JS_sprintf_append(NULL, "<value>");
        if (!source)
            return false;
        bytes->initBytes(source);
        return true;
    }

    if (!JSVAL_IS_PRIMITIVE(v)) {
        JSObject *obj = JSVAL_TO_OBJECT(v);
        if (obj->isBlock()) {
            char *source = JS_sprintf_append(NULL, "depth %d {", obj->asBlock().stackDepth());
            if (!source)
                return false;

            Shape::Range r = obj->lastProperty()->all();
            while (!r.empty()) {
                const Shape &shape = r.front();
                JSAtom *atom = JSID_IS_INT(shape.propid())
                               ? cx->runtime->atomState.emptyAtom
                               : JSID_TO_ATOM(shape.propid());

                JSAutoByteString bytes;
                if (!js_AtomToPrintableString(cx, atom, &bytes))
                    return false;

                r.popFront();
                source = JS_sprintf_append(source, "%s: %d%s",
                                           bytes.ptr(), shape.shortid(),
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

        if (obj->isFunction()) {
            JSString *str = JS_DecompileFunction(cx, obj->toFunction(), JS_DONT_PRETTY_PRINT);
            if (!str)
                return false;
            return bytes->encode(cx, str);
        }

        if (obj->isRegExp()) {
            JSString *source = obj->asRegExp().toString(cx);
            if (!source)
                return false;
            JS::Anchor<JSString *> anchor(source);
            return bytes->encode(cx, source);
        }
    }

    return !!js_ValueToPrintable(cx, v, bytes, true);
}

JS_FRIEND_API(unsigned)
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc,
                unsigned loc, JSBool lines, Sprinter *sp)
{
    JSOp op = (JSOp)*pc;
    if (op >= JSOP_LIMIT) {
        char numBuf1[12], numBuf2[12];
        JS_snprintf(numBuf1, sizeof numBuf1, "%d", op);
        JS_snprintf(numBuf2, sizeof numBuf2, "%d", JSOP_LIMIT);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BYTECODE_TOO_BIG, numBuf1, numBuf2);
        return 0;
    }
    const JSCodeSpec *cs = &js_CodeSpec[op];
    ptrdiff_t len = (ptrdiff_t) cs->length;
    Sprint(sp, "%05u:", loc);
    if (lines)
        Sprint(sp, "%4u", JS_PCToLineNumber(cx, script, pc));
    Sprint(sp, "  %s", js_CodeName[op]);

    switch (JOF_TYPE(cs->format)) {
      case JOF_BYTE:
          // Scan the trynotes to find the associated catch block
          // and make the try opcode look like a jump instruction
          // with an offset. This simplifies code coverage analysis
          // based on this disassembled output.
          if (op == JSOP_TRY) {
              JSTryNoteArray *trynotes = script->trynotes();
              uint32_t i;
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

      case JOF_JUMP: {
        ptrdiff_t off = GET_JUMP_OFFSET(pc);
        Sprint(sp, " %u (%+d)", loc + (int) off, (int) off);
        break;
      }

      case JOF_ATOM: {
        Value v = StringValue(script->getAtom(GET_UINT32_INDEX(pc)));
        JSAutoByteString bytes;
        if (!ToDisassemblySource(cx, v, &bytes))
            return 0;
        Sprint(sp, " %s", bytes.ptr());
        break;
      }

      case JOF_DOUBLE: {
        Value v = script->getConst(GET_UINT32_INDEX(pc));
        JSAutoByteString bytes;
        if (!ToDisassemblySource(cx, v, &bytes))
            return 0;
        Sprint(sp, " %s", bytes.ptr());
        break;
      }

      case JOF_OBJECT: {
        /* Don't call obj.toSource if analysis/inference is active. */
        if (cx->compartment->activeAnalysis) {
            Sprint(sp, " object");
            break;
        }

        JSObject *obj = script->getObject(GET_UINT32_INDEX(pc));
        {
            JSAutoByteString bytes;
            if (!ToDisassemblySource(cx, ObjectValue(*obj), &bytes))
                return 0;
            Sprint(sp, " %s", bytes.ptr());
        }
        break;
      }

      case JOF_REGEXP: {
        JSObject *obj = script->getRegExp(GET_UINT32_INDEX(pc));
        JSAutoByteString bytes;
        if (!ToDisassemblySource(cx, ObjectValue(*obj), &bytes))
            return 0;
        Sprint(sp, " %s", bytes.ptr());
        break;
      }

      case JOF_TABLESWITCH:
      {
        int32_t i, low, high;

        ptrdiff_t off = GET_JUMP_OFFSET(pc);
        jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
        low = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        Sprint(sp, " defaultOffset %d low %d high %d", int(off), low, high);
        for (i = low; i <= high; i++) {
            off = GET_JUMP_OFFSET(pc2);
            Sprint(sp, "\n\t%d: %d", i, int(off));
            pc2 += JUMP_OFFSET_LEN;
        }
        len = 1 + pc2 - pc;
        break;
      }

      case JOF_LOOKUPSWITCH:
      {
        jsatomid npairs;

        ptrdiff_t off = GET_JUMP_OFFSET(pc);
        jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
        npairs = GET_UINT16(pc2);
        pc2 += UINT16_LEN;
        Sprint(sp, " offset %d npairs %u", int(off), unsigned(npairs));
        while (npairs) {
            uint32_t constIndex = GET_UINT32_INDEX(pc2);
            pc2 += UINT32_INDEX_LEN;
            off = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            JSAutoByteString bytes;
            if (!ToDisassemblySource(cx, script->getConst(constIndex), &bytes))
                return 0;
            Sprint(sp, "\n\t%s: %d", bytes.ptr(), int(off));
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

      case JOF_SLOTOBJECT: {
        Sprint(sp, " %u", GET_SLOTNO(pc));
        JSObject *obj = script->getObject(GET_UINT32_INDEX(pc + SLOTNO_LEN));
        JSAutoByteString bytes;
        if (!ToDisassemblySource(cx, ObjectValue(*obj), &bytes))
            return 0;
        Sprint(sp, " %s", bytes.ptr());
        break;
      }

      {
        int i;

      case JOF_UINT16PAIR:
        i = (int)GET_UINT16(pc);
        Sprint(sp, " %d", i);
        pc += UINT16_LEN;
        /* FALL THROUGH */

      case JOF_UINT16:
        i = (int)GET_UINT16(pc);
        goto print_int;

      case JOF_UINT24:
        JS_ASSERT(op == JSOP_UINT24 || op == JSOP_NEWARRAY);
        i = (int)GET_UINT24(pc);
        goto print_int;

      case JOF_UINT8:
        i = GET_UINT8(pc);
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
      }

      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%lx", (unsigned long) cs->format);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNKNOWN_FORMAT, numBuf);
        return 0;
      }
    }
    sp->put("\n");
    return len;
}

#endif /* DEBUG */

/************************************************************************/

const size_t Sprinter::DefaultSize = 64;

bool 
Sprinter::realloc_(size_t newSize)
{
    JS_ASSERT(newSize > (size_t) offset);
    char *newBuf = (char *) context->realloc_(base, newSize);
    if (!newBuf)
        return false;
    base = newBuf;
    size = newSize;
    base[size - 1] = 0;
    return true;
}

Sprinter::Sprinter(JSContext *cx)
  : context(cx),
#ifdef DEBUG
    initialized(false),
#endif
    base(NULL), size(0), offset(0)
{ }

Sprinter::~Sprinter()
{
#ifdef DEBUG
    if (initialized)
        checkInvariants();
#endif
    context->free_(base);
}

bool
Sprinter::init()
{
    JS_ASSERT(!initialized);
    base = (char *) context->malloc_(DefaultSize);
    if (!base)
        return false;
#ifdef DEBUG
    initialized = true;
#endif
    *base = 0;
    size = DefaultSize;
    base[size - 1] = 0;
    return true;
}

void
Sprinter::checkInvariants() const
{
    JS_ASSERT(initialized);
    JS_ASSERT((size_t) offset < size);
    JS_ASSERT(base[size - 1] == 0);
}

const char *
Sprinter::string() const
{
    return base;
}

const char *
Sprinter::stringEnd() const
{
    return base + offset;
}

char *
Sprinter::stringAt(ptrdiff_t off) const
{
    JS_ASSERT(off >= 0 && (size_t) off < size);
    return base + off;
}

char &
Sprinter::operator[](size_t off)
{
    JS_ASSERT(off >= 0 && (size_t) off < size);
    return *(base + off);
}

bool
Sprinter::empty() const
{
    return *base == 0;
}

char *
Sprinter::reserve(size_t len)
{
    InvariantChecker ic(this);

    while (len + 1 > size - offset) { /* Include trailing \0 */
        if (!realloc_(size * 2))
            return NULL;
    }

    char *sb = base + offset;
    offset += len;
    return sb;
}

char *
Sprinter::reserveAndClear(size_t len)
{
    char *sb = reserve(len);
    if (sb)
        memset(sb, 0, len);
    return sb;
}

ptrdiff_t
Sprinter::put(const char *s, size_t len)
{
    InvariantChecker ic(this);

    const char *oldBase = base;
    const char *oldEnd = base + size;

    ptrdiff_t oldOffset = offset;
    char *bp = reserve(len);
    if (!bp)
        return -1;

    /* s is within the buffer already */
    if (s >= oldBase && s < oldEnd) {
        /* buffer was realloc'ed */
        if (base != oldBase)
            s = stringAt(s - oldBase);  /* this is where it lives now */
        memmove(bp, s, len);
    } else {
        js_memcpy(bp, s, len);
    }

    bp[len] = 0;
    return oldOffset;
}

ptrdiff_t
Sprinter::put(const char *s)
{
    return put(s, strlen(s));
}

ptrdiff_t
Sprinter::putString(JSString *s)
{
    InvariantChecker ic(this);

    size_t length = s->length();
    const jschar *chars = s->getChars(context);
    if (!chars)
        return -1;

    size_t size = GetDeflatedStringLength(context, chars, length);
    if (size == (size_t) -1)
        return -1;

    ptrdiff_t oldOffset = offset;
    char *buffer = reserve(size);
    if (!buffer)
        return -1;
    DeflateStringToBuffer(context, chars, length, buffer, &size);
    buffer[size] = 0;

    return oldOffset;
}

int
Sprinter::printf(const char *fmt, ...)
{
    InvariantChecker ic(this);

    do {
        va_list va;
        va_start(va, fmt);
        int i = vsnprintf(base + offset, size - offset, fmt, va);
        va_end(va);

        if (i > -1 && (size_t) i < size - offset) {
            offset += i;
            return i;
        }
    } while (realloc_(size * 2));

    return -1;
}

void
Sprinter::setOffset(const char *end)
{
    JS_ASSERT(end >= base && end < base + size);
    offset = end - base;
}

void
Sprinter::setOffset(ptrdiff_t off)
{
    JS_ASSERT(off >= 0 && (size_t) off < size);
    offset = off;
}

ptrdiff_t
Sprinter::getOffset() const
{
    return offset;
}

ptrdiff_t
Sprinter::getOffsetOf(const char *string) const
{
    JS_ASSERT(string >= base && string < base + size);
    return string - base;
}

ptrdiff_t
js::Sprint(Sprinter *sp, const char *format, ...)
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
    offset = sp->put(bp);
    sp->context->free_(bp);
    return offset;
}

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
    '\0'
};

#define DONT_ESCAPE     0x10000

static char *
QuoteString(Sprinter *sp, JSString *str, uint32_t quote)
{
    /* Sample off first for later return value pointer computation. */
    JSBool dontEscape = (quote & DONT_ESCAPE) != 0;
    jschar qc = (jschar) quote;
    ptrdiff_t offset = sp->getOffset();
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

        {
            ptrdiff_t len = t - s;
            ptrdiff_t base = sp->getOffset();
            char *bp = sp->reserve(len);
            if (!bp)
                return NULL;

            for (ptrdiff_t i = 0; i < len; ++i)
                (*sp)[base + i] = (char) *s++;
            (*sp)[base + len] = 0;
        }

        if (t == z)
            break;

        /* Use js_EscapeMap, \u, or \x only if necessary. */
        bool ok;
        const char *e;
        if (!(c >> 8) && c != 0 && (e = strchr(js_EscapeMap, (int)c)) != NULL) {
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
     * the return below gives a valid result.
     */
    if (offset == sp->getOffset() && Sprint(sp, "") < 0)
        return NULL;

    return sp->stringAt(offset);
}

JSString *
js_QuoteString(JSContext *cx, JSString *str, jschar quote)
{
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return NULL;
    char *bytes = QuoteString(&sprinter, str, quote);
    JSString *escstr = bytes ? JS_NewStringCopyZ(cx, bytes) : NULL;
    return escstr;
}

/************************************************************************/

/*
 * Information for associating the decompilation of each opcode in a script
 * with the place where it appears in the text for the decompilation of the
 * entire script (or the function containing the script).
 */
struct DecompiledOpcode
{
    /* Decompiled text of this opcode. */
    const char *text;

    /* Bytecode into which this opcode was nested, or NULL. */
    jsbytecode *parent;

    /*
     * Offset into the parent's decompiled text of the decompiled text of this
     * opcode. For opcodes with a NULL parent, this was emitted directly into
     * the permanent output at the given offset.
     */
    int32_t parentOffset;

    /*
     * Surrounded by parentheses when printed, which parentOffset does not
     * account for.
     */
    bool parenthesized;

    DecompiledOpcode()
        : text(NULL), parent(NULL), parentOffset(-1), parenthesized(false)
    {}
};

struct JSPrinter
{
    Sprinter        sprinter;       /* base class state */
    LifoAlloc       pool;           /* string allocation pool */
    unsigned           indent;         /* indentation in spaces */
    bool            pretty;         /* pretty-print: indent, use newlines */
    bool            grouped;        /* in parenthesized expression context */
    bool            strict;         /* in code marked strict */
    JSScript        *script;        /* script being printed */
    jsbytecode      *dvgfence;      /* DecompileExpression fencepost */
    jsbytecode      **pcstack;      /* DecompileExpression modeled stack */
    JSFunction      *fun;           /* interpreted function */
    Vector<JSAtom *> *localNames;   /* argument and variable names */
    Vector<DecompiledOpcode> *decompiledOpcodes; /* optional state for decompiled ops */

    DecompiledOpcode &decompiled(jsbytecode *pc) {
        JS_ASSERT(decompiledOpcodes);
        return (*decompiledOpcodes)[pc - script->code];
    }
};

JSPrinter *
js_NewPrinter(JSContext *cx, const char *name, JSFunction *fun,
              unsigned indent, JSBool pretty, JSBool grouped, JSBool strict)
{
    JSPrinter *jp = (JSPrinter *) cx->malloc_(sizeof(JSPrinter));
    if (!jp)
        return NULL;
    new (&jp->sprinter) Sprinter(cx);
    if (!jp->sprinter.init())
        return NULL;
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
    jp->decompiledOpcodes = NULL;
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
    JSContext *cx = jp->sprinter.context;
    jp->pool.freeAll();
    Foreground::delete_(jp->localNames);
    jp->sprinter.Sprinter::~Sprinter();
    cx->free_(jp);
}

JSString *
js_GetPrinterOutput(JSPrinter *jp)
{
    JSContext *cx = jp->sprinter.context;
    return JS_NewStringCopyZ(cx, jp->sprinter.string());
}

/* Mark the parent and offset into the parent's text for a printed opcode. */
static inline void
UpdateDecompiledParent(JSPrinter *jp, jsbytecode *pc, jsbytecode *parent, size_t offset)
{
    if (jp->decompiledOpcodes && pc) {
        jp->decompiled(pc).parent = parent;
        jp->decompiled(pc).parentOffset = offset;
    }
}

/*
 * NB: Indexed by SRC_DECL_* defines from frontend/BytecodeEmitter.h.
 */
static const char * const var_prefix[] = {"var ", "const ", "let "};

static const char *
VarPrefix(jssrcnote *sn)
{
    if (sn && (SN_TYPE(sn) == SRC_DECL || SN_TYPE(sn) == SRC_GROUPASSIGN)) {
        ptrdiff_t type = js_GetSrcNoteOffset(sn, 0);
        if ((unsigned)type <= SRC_DECL_LET)
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
    if (jp->sprinter.put(bp, (size_t)cc) < 0)
        cc = -1;
    jp->sprinter.context->free_(bp);

    va_end(ap);
    return cc;
}

JSBool
js_puts(JSPrinter *jp, const char *s)
{
    return jp->sprinter.put(s) >= 0;
}

/************************************************************************/

struct SprintStack
{
    Sprinter    sprinter;       /* sprinter for postfix to infix buffering */
    ptrdiff_t   *offsets;       /* stack of postfix string offsets */
    jsbytecode  *opcodes;       /* parallel stack of JS opcodes */
    jsbytecode  **bytecodes;    /* actual script bytecode pushing the value */
    unsigned       top;            /* top of stack index */
    unsigned       inArrayInit;    /* array initialiser/comprehension level */
    JSBool      inGenExp;       /* in generator expression */
    JSPrinter   *printer;       /* permanent output goes here */

    explicit SprintStack(JSContext *cx)
      : sprinter(cx), offsets(NULL),
        opcodes(NULL), bytecodes(NULL), top(0), inArrayInit(0),
        inGenExp(JS_FALSE), printer(NULL)
    { }
};

/*
 * Set the decompiled text of an opcode, according to an offset into the
 * print stack's sprinter buffer.
 */
static inline bool
UpdateDecompiledText(SprintStack *ss, jsbytecode *pc, ptrdiff_t todo)
{
    JSPrinter *jp = ss->printer;

    if (jp->decompiledOpcodes && jp->decompiled(pc).text == NULL) {
        const char *text = ss->sprinter.stringAt(todo);
        size_t len = strlen(text) + 1;

        char *ntext = ss->printer->pool.newArrayUninitialized<char>(len);
        if (!ntext) {
            js_ReportOutOfMemory(ss->sprinter.context);
            return false;
        }

        js_memcpy(ntext, text, len);
        jp->decompiled(pc).text = const_cast<const char *>(ntext);
    }

    return true;
}

static inline const char *
SprintDupeStr(SprintStack *ss, const char *str)
{
    size_t len = strlen(str) + 1;

    const char *nstr = ss->printer->pool.newArrayUninitialized<char>(len);
    if (nstr) {
        js_memcpy((char *) nstr, str, len);
    } else {
        js_ReportOutOfMemory(ss->sprinter.context);
        nstr = "";
    }

    return nstr;
}

/* Place an opcode's decompiled text into a printer's permanent output. */
static inline void
SprintOpcodePermanent(JSPrinter *jp, const char *str, jsbytecode *pc)
{
    ptrdiff_t offset = jp->sprinter.getOffset();
    UpdateDecompiledParent(jp, pc, NULL, offset);
    js_printf(jp, "%s", str);
}

/*
 * Place an opcode's decompiled text into the printed output for another
 * opcode parentpc, where startOffset indicates the printer offset for the
 * start of parentpc.
 */
static inline void
SprintOpcode(SprintStack *ss, const char *str, jsbytecode *pc,
             jsbytecode *parentpc, ptrdiff_t startOffset)
{
    if (startOffset < 0) {
        JS_ASSERT(ss->sprinter.context->isExceptionPending());
        return;
    }
    ptrdiff_t offset = ss->sprinter.getOffset();
    UpdateDecompiledParent(ss->printer, pc, parentpc, offset - startOffset);
    ss->sprinter.put(str);
}

/*
 * Copy the decompiled text for an opcode to all other ops which it was
 * decomposed into.
 */
static inline void
CopyDecompiledTextForDecomposedOp(JSPrinter *jp, jsbytecode *pc)
{
    JS_ASSERT(js_CodeSpec[*pc].format & JOF_DECOMPOSE);

    if (jp->decompiledOpcodes) {
        size_t len = GetDecomposeLength(pc, js_CodeSpec[*pc].length);

        const char *text = jp->decompiled(pc).text;

        jsbytecode *pc2 = pc + GetBytecodeLength(pc);
        for (; pc2 < pc + len; pc2 += GetBytecodeLength(pc2)) {
            jp->decompiled(pc2).text = text;
            jp->decompiled(pc2).parent = pc;
            jp->decompiled(pc2).parentOffset = 0;
        }
    }
}

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
static int
ReconstructPCStack(JSContext *cx, JSScript *script, jsbytecode *pc,
                   jsbytecode **pcstack, jsbytecode **lastDecomposedPC);

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
GetOff(SprintStack *ss, unsigned i)
{
    ptrdiff_t off;
    jsbytecode *pc;
    char *bytes;

    off = ss->offsets[i];
    if (off >= 0)
        return off;

    JS_ASSERT(ss->printer->pcstack);
    if (off <= -2 && ss->printer->pcstack) {
        pc = ss->printer->pcstack[-2 - off];
        bytes = DecompileExpression(ss->sprinter.context, ss->printer->script,
                                    ss->printer->fun, pc);
        if (!bytes)
            return 0;
        if (bytes != FAILED_EXPRESSION_DECOMPILER) {
            off = ss->sprinter.put(bytes);
            if (off < 0)
                off = 0;
            ss->offsets[i] = off;
            ss->sprinter.context->free_(bytes);
            return off;
        }

        if (!*ss->sprinter.string()) {
            memset(ss->sprinter.stringAt(0), 0, ss->sprinter.getOffset());
            ss->offsets[i] = -1;
        }
    }
    return 0;
}

static const char *
GetStr(SprintStack *ss, unsigned i)
{
    ptrdiff_t off = GetOff(ss, i);
    return ss->sprinter.stringAt(off);
}

/*
 * Gap between stacked strings to allow for insertion of parens and commas
 * when auto-parenthesizing expressions and decompiling array initialisers.
 */
#define PAREN_SLOP      (2 + 1)

/* Fake opcodes (see jsopcode.h) must not overflow unsigned 8-bit space. */
JS_STATIC_ASSERT(JSOP_FAKE_LIMIT <= 255);

static inline void
AddParenSlop(SprintStack *ss)
{
    ss->sprinter.reserveAndClear(PAREN_SLOP);
}

static JSBool
PushOff(SprintStack *ss, ptrdiff_t off, JSOp op, jsbytecode *pc = NULL)
{
    unsigned top;

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
    ss->bytecodes[top] = pc;
    ss->top = ++top;

    AddParenSlop(ss);
    return JS_TRUE;
}

static bool
PushStr(SprintStack *ss, const char *str, JSOp op)
{
    ptrdiff_t off = ss->sprinter.put(str);
    if (off < 0)
        return false;
    return PushOff(ss, off, op);
}

static ptrdiff_t
PopOffPrec(SprintStack *ss, uint8_t prec, jsbytecode **ppc = NULL)
{
    unsigned top;
    const JSCodeSpec *topcs;
    ptrdiff_t off;

    if (ppc)
        *ppc = NULL;

    /* ss->top points to the next free slot; be paranoid about underflow. */
    top = ss->top;
    JS_ASSERT(top != 0);
    if (top == 0)
        return 0;

    ss->top = --top;
    off = GetOff(ss, top);
    topcs = &js_CodeSpec[ss->opcodes[top]];

    jsbytecode *pc = ss->bytecodes[top];
    if (ppc)
        *ppc = pc;

    if (topcs->prec != 0 && topcs->prec < prec) {
        ss->offsets[top] = off - 2;
        ss->sprinter.setOffset(off - 2);
        off = Sprint(&ss->sprinter, "(%s)", ss->sprinter.stringAt(off));
        if (ss->printer->decompiledOpcodes && pc)
            ss->printer->decompiled(pc).parenthesized = true;
    } else {
        ss->sprinter.setOffset(off);
    }
    return off;
}

static const char *
PopStrPrec(SprintStack *ss, uint8_t prec, jsbytecode **ppc = NULL)
{
    ptrdiff_t off;

    off = PopOffPrec(ss, prec, ppc);
    return ss->sprinter.stringAt(off);
}

/*
 * As for PopStrPrec, but duplicates the string into the printer's arena.
 * Strings returned by PopStrPrec are otherwise invalidated if any new text
 * is printed into ss.
 */
static const char *
PopStrPrecDupe(SprintStack *ss, uint8_t prec, jsbytecode **ppc = NULL)
{
    const char *str = PopStrPrec(ss, prec, ppc);
    return SprintDupeStr(ss, str);
}

static ptrdiff_t
PopOff(SprintStack *ss, JSOp op, jsbytecode **ppc = NULL)
{
    return PopOffPrec(ss, js_CodeSpec[op].prec, ppc);
}

static const char *
PopStr(SprintStack *ss, JSOp op, jsbytecode **ppc = NULL)
{
    return PopStrPrec(ss, js_CodeSpec[op].prec, ppc);
}

static const char *
PopStrDupe(SprintStack *ss, JSOp op, jsbytecode **ppc = NULL)
{
    return PopStrPrecDupe(ss, js_CodeSpec[op].prec, ppc);
}

/*
 * Pop a condition expression for if/while. JSOP_IFEQ's precedence forces
 * extra parens around assignment, which avoids a strict-mode warning.
 */
static const char *
PopCondStr(SprintStack *ss, jsbytecode **ppc = NULL)
{
    JSOp op = (js_CodeSpec[ss->opcodes[ss->top - 1]].format & JOF_SET)
              ? JSOP_IFEQ
              : JSOP_NOP;
    return PopStr(ss, op, ppc);
}

static inline bool
IsInitializerOp(unsigned char op)
{
    return op == JSOP_NEWINIT || op == JSOP_NEWARRAY || op == JSOP_NEWOBJECT;
}

struct TableEntry {
    jsval       key;
    ptrdiff_t   offset;
    JSAtom      *label;
    int         order;          /* source order for stable tableswitch sort */
};

inline bool
CompareTableEntries(const TableEntry &a, const TableEntry &b, bool *lessOrEqualp)
{
    *lessOrEqualp = (a.offset != b.offset) ? a.offset <= b.offset : a.order <= b.order;
    return true;
}

static ptrdiff_t
SprintDoubleValue(Sprinter *sp, jsval v, JSOp *opp)
{
    double d;
    ptrdiff_t todo;
    char *s;

    JS_ASSERT(JSVAL_IS_DOUBLE(v));
    d = JSVAL_TO_DOUBLE(v);
    if (JSDOUBLE_IS_NEGZERO(d)) {
        todo = sp->put("-0");
        *opp = JSOP_NEG;
    } else if (!JSDOUBLE_IS_FINITE(d)) {
        /* Don't use Infinity and NaN, as local variables may shadow them. */
        todo = sp->put(JSDOUBLE_IS_NaN(d)
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
Decompile(SprintStack *ss, jsbytecode *pc, int nb);

static JSBool
DecompileSwitch(SprintStack *ss, TableEntry *table, unsigned tableLength,
                jsbytecode *pc, ptrdiff_t switchLength,
                ptrdiff_t defaultOffset, JSBool isCondSwitch)
{
    JSContext *cx;
    JSPrinter *jp;
    ptrdiff_t off, off2, diff, caseExprOff, todo;
    const char *rval;
    unsigned i;
    jsval key;
    JSString *str;

    cx = ss->sprinter.context;
    jp = ss->printer;

    jsbytecode *lvalpc;
    const char *lval = PopStr(ss, JSOP_NOP, &lvalpc);

    /* JSOP_CONDSWITCH doesn't pop, unlike JSOP_{LOOKUP,TABLE}SWITCH. */
    if (isCondSwitch)
        ss->top++;

    js_printf(jp, "\tswitch (");
    SprintOpcodePermanent(jp, lval, lvalpc);
    js_printf(jp, ") {\n");

    if (tableLength) {
        diff = table[0].offset - defaultOffset;
        if (diff > 0) {
            jp->indent += 2;
            js_printf(jp, "\t%s:\n", js_default_str);
            jp->indent += 2;
            if (!Decompile(ss, pc + defaultOffset, diff))
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
                if (!Decompile(ss, pc + caseExprOff, nextCaseExprOff - caseExprOff))
                    return JS_FALSE;
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
                    str = ToString(cx, key);
                    if (!str)
                        return JS_FALSE;
                }
                if (todo >= 0) {
                    rval = ss->sprinter.stringAt(todo);
                } else {
                    rval = QuoteString(&ss->sprinter, str, (jschar)
                                       (JSVAL_IS_STRING(key) ? '"' : 0));
                    if (!rval)
                        return JS_FALSE;
                }
                ss->sprinter.setOffset(rval);
                jp->indent += 2;
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
                js_printf(jp, "\t%s:\n", js_default_str);
                jp->indent += 2;
            }
            if (!Decompile(ss, pc + off, off2 - off))
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
GetArgOrVarAtom(JSPrinter *jp, unsigned slot)
{
    LOCAL_ASSERT_RV(jp->fun, NULL);
    LOCAL_ASSERT_RV(slot < jp->fun->script()->bindings.countLocalNames(), NULL);
    JSAtom *name = (*jp->localNames)[slot];
#if !JS_HAS_DESTRUCTURING
    LOCAL_ASSERT_RV(name, NULL);
#endif
    return name;
}

#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, "")

static const char *
GetLocalInSlot(SprintStack *ss, int i, int slot, JSObject *obj)
{
    for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();

        if (shape.shortid() == slot) {
            /* Ignore the empty destructuring dummy. */
            if (!JSID_IS_ATOM(shape.propid()))
                continue;

            JSAtom *atom = JSID_TO_ATOM(shape.propid());
            const char *rval = QuoteString(&ss->sprinter, atom, 0);
            if (!rval)
                return NULL;

            ss->sprinter.setOffset(rval);
            return rval;
        }
    }

    return GetStr(ss, i);
}

const char *
GetLocal(SprintStack *ss, int i)
{
    ptrdiff_t off = ss->offsets[i];
    if (off >= 0)
        return ss->sprinter.stringAt(off);

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

    // In case of a let variable, the stack points to a JSOP_ENTERBLOCK opcode.
    // Get the object number from the block instead of iterating all objects and
    // hoping the right object is found.
    if (off <= -2 && ss->printer->pcstack) {
        jsbytecode *pc = ss->printer->pcstack[-2 - off];

        JS_ASSERT(ss->printer->script->code <= pc);
        JS_ASSERT(pc < (ss->printer->script->code + ss->printer->script->length));

        if (JSOP_ENTERBLOCK == (JSOp)*pc) {
            JSObject *obj = script->getObject(GET_UINT32_INDEX(pc));

            if (obj->isBlock()) {
                uint32_t depth = obj->asBlock().stackDepth();
                uint32_t count = obj->asBlock().slotCount();
                if (jsuint(i - depth) < jsuint(count))
                    return GetLocalInSlot(ss, i, int(i - depth), obj);
            }
        }
    }

    // Iterate over all objects.
    for (jsatomid j = 0, n = script->objects()->length; j != n; j++) {
        JSObject *obj = script->getObject(j);

        if (obj->isBlock()) {
            uint32_t depth = obj->asBlock().stackDepth();
            uint32_t count = obj->asBlock().slotCount();
            if (jsuint(i - depth) < jsuint(count))
                return GetLocalInSlot(ss, i, int(i - depth), obj);
        }
    }

    return GetStr(ss, i);
}

#undef LOCAL_ASSERT

static JSBool
IsVarSlot(JSPrinter *jp, jsbytecode *pc, int *indexp)
{
    unsigned slot;

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

#define LOAD_ATOM(PCOFF) (atom = (jp->script->getAtom(GET_UINT32_INDEX((pc) + PCOFF))))

typedef Vector<JSAtom *, 8> AtomVector;
typedef AtomVector::Range AtomRange;

#if JS_HAS_DESTRUCTURING

#define LOCAL_ASSERT(expr)  LOCAL_ASSERT_RV(expr, NULL)
#define LOAD_OP_DATA(pc)    (oplen = (cs = &js_CodeSpec[op=(JSOp)*pc])->length)

static jsbytecode *
DecompileDestructuring(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc,
                       AtomRange *letNames = NULL);

/*
 * Decompile a single element of a compound {}/[] destructuring lhs, sprinting
 * the result in-place (without pushing/popping the stack) and advancing the pc
 * to either the next element or the final pop.
 *
 * For normal (SRC_DESTRUCT) destructuring, the names of assigned/initialized
 * variables are read from their slots. However, for SRC_DESTRUCTLET, the slots
 * have not been pushed yet; the caller must pass the names to use via
 * 'letNames'. Each variable initialized in this destructuring lhs results in
 * popping a name from 'letNames'.
 */
static jsbytecode *
DecompileDestructuringLHS(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc, JSBool *hole,
                          AtomRange *letNames = NULL)
{
    JSPrinter *jp;
    JSOp op;
    const JSCodeSpec *cs;
    unsigned oplen;
    int i;
    const char *lval, *xval;
    JSAtom *atom;

    *hole = JS_FALSE;
    jp = ss->printer;
    LOAD_OP_DATA(pc);

    switch (op) {
      case JSOP_POP:
        *hole = JS_TRUE;
        if (ss->sprinter.put(", ", 2) < 0)
            return NULL;
        break;

      case JSOP_PICK:
        /*
         * For 'let ([x, y] = y)', the emitter generates
         *
         *     push evaluation of y
         *     dup
         *   1 one
         *   2 getelem
         *   3 pick
         *   4 two
         *     getelem
         *     pick
         *     pop
         *
         * Thus 'x' consists of 1 - 3. The caller (DecompileDestructuring or
         * DecompileGroupAssignment) will have taken care of 1 - 2, so pc is
         * now pointing at 3. The pick indicates a primitive let var init so
         * pop a name and advance the pc to 4.
         */
        LOCAL_ASSERT(letNames && !letNames->empty());
        if (!QuoteString(&ss->sprinter, letNames->popCopyFront(), 0))
            return NULL;
        break;

      case JSOP_DUP:
      {
        /* Compound lhs, e.g., '[x,y]' in 'let [[x,y], z] = a;'. */
        pc = DecompileDestructuring(ss, pc, endpc, letNames);
        if (!pc)
            return NULL;
        if (pc == endpc)
            return pc;
        LOAD_OP_DATA(pc);

        /*
         * By its post-condition, DecompileDestructuring pushed one string
         * containing the whole decompiled lhs. Our post-condition is to sprint
         * in-place so pop/concat this pushed string.
         */
        lval = PopStr(ss, JSOP_NOP);
        if (ss->sprinter.put(lval) < 0)
            return NULL;

        LOCAL_ASSERT(*pc == JSOP_POP);

        /*
         * To put block slots in the right place, the emitter follows a
         * compound lhs with a pick (if at least one slot was pushed). The pick
         * is not part of the compound lhs so DecompileDestructuring did not
         * advance over it but it is part of the lhs so advance over it here.
         */
        jsbytecode *nextpc = pc + JSOP_POP_LENGTH;
        LOCAL_ASSERT(nextpc <= endpc);
        if (letNames && *nextpc == JSOP_PICK) {
            LOCAL_ASSERT(nextpc < endpc);
            pc = nextpc;
            LOAD_OP_DATA(pc);
        }
        break;
      }

      case JSOP_SETARG:
      case JSOP_SETLOCAL:
        LOCAL_ASSERT(!letNames);
        LOCAL_ASSERT(pc[oplen] == JSOP_POP || pc[oplen] == JSOP_POPN);
        /* FALL THROUGH */
      case JSOP_SETLOCALPOP:
        LOCAL_ASSERT(!letNames);
        if (op == JSOP_SETARG) {
            atom = GetArgOrVarAtom(jp, GET_SLOTNO(pc));
            LOCAL_ASSERT(atom);
            if (!QuoteString(&ss->sprinter, atom, 0))
                return NULL;
        } else if (IsVarSlot(jp, pc, &i)) {
            atom = GetArgOrVarAtom(jp, i);
            LOCAL_ASSERT(atom);
            if (!QuoteString(&ss->sprinter, atom, 0))
                return NULL;
        } else {
            lval = GetLocal(ss, i);
            if (!lval || ss->sprinter.put(lval) < 0)
                return NULL;
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

      default: {
        LOCAL_ASSERT(!letNames);
        /*
         * We may need to auto-parenthesize the left-most value decompiled
         * here, so add back PAREN_SLOP temporarily.  Then decompile until the
         * opcode that would reduce the stack depth to (ss->top-1), which we
         * pass to Decompile encoded as -(ss->top-1) - 1 or just -ss->top for
         * the nb parameter.
         */
        ptrdiff_t todo = ss->sprinter.getOffset();
        ss->sprinter.reserve(PAREN_SLOP);
        pc = Decompile(ss, pc, -((int)ss->top));
        if (!pc)
            return NULL;
        if (pc == endpc)
            return pc;
        LOAD_OP_DATA(pc);
        LOCAL_ASSERT(op == JSOP_ENUMELEM || op == JSOP_ENUMCONSTELEM);
        xval = PopStr(ss, JSOP_NOP);
        lval = PopStr(ss, JSOP_GETPROP);
        ss->sprinter.setOffset(todo);
        if (*lval == '\0') {
            /* lval is from JSOP_BINDNAME, so just print xval. */
            todo = ss->sprinter.put(xval);
        } else if (*xval == '\0') {
            /* xval is from JSOP_SETCALL or JSOP_BINDXMLNAME, print lval. */
            todo = ss->sprinter.put(lval);
        } else {
            todo = Sprint(&ss->sprinter,
                          (JOF_OPMODE(ss->opcodes[ss->top+1]) == JOF_XMLNAME)
                          ? "%s.%s"
                          : "%s[%s]",
                          lval, xval);
        }
        if (todo < 0)
            return NULL;
        break;
      }
    }

    LOCAL_ASSERT(pc < endpc);
    pc += oplen;
    return pc;
}

/*
 * Decompile a destructuring lhs object or array initialiser, including nested
 * destructuring initialisers. On return a single string is pushed containing
 * the entire lhs (regardless of how many variables were bound). Thus, the
 * caller must take care of fixing up the decompiler stack.
 *
 * See DecompileDestructuringLHS for description of 'letNames'.
 */
static jsbytecode *
DecompileDestructuring(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc,
                       AtomRange *letNames)
{
    LOCAL_ASSERT(*pc == JSOP_DUP);
    pc += JSOP_DUP_LENGTH;

    JSContext *cx = ss->sprinter.context;
    JSPrinter *jp = ss->printer;
    jsbytecode *startpc = pc;

    /*
     * Set head so we can rewrite '[' to '{' as needed.  Back up PAREN_SLOP
     * chars so the destructuring decompilation accumulates contiguously in
     * ss->sprinter starting with "[".
     */
    ptrdiff_t head = ss->sprinter.put("[", 1);
    if (head < 0 || !PushOff(ss, head, JSOP_NOP))
        return NULL;
    ss->sprinter.setOffset(ss->sprinter.getOffset() - PAREN_SLOP);
    LOCAL_ASSERT(head == ss->sprinter.getOffset() - 1);
    LOCAL_ASSERT(ss->sprinter[head] == '[');

    int lasti = -1;

    while (pc < endpc) {
#if JS_HAS_DESTRUCTURING_SHORTHAND
        ptrdiff_t nameoff = -1;
#endif

        const JSCodeSpec *cs;
        unsigned oplen;
        JSOp op;
        LOAD_OP_DATA(pc);

        int i;
        double d;
        switch (op) {
          case JSOP_POP:
            /* Empty destructuring lhs. */
            LOCAL_ASSERT(startpc == pc);
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
            d = jp->script->getConst(GET_UINT32_INDEX(pc)).toDouble();
            LOCAL_ASSERT(JSDOUBLE_IS_FINITE(d) && !JSDOUBLE_IS_NEGZERO(d));
            i = (int)d;

          do_getelem:
          {
            jssrcnote *sn = js_GetSrcNote(jp->script, pc);
            pc += oplen;
            if (pc == endpc)
                return pc;
            LOAD_OP_DATA(pc);
            LOCAL_ASSERT(op == JSOP_GETELEM);

            /* Distinguish object from array by opcode or source note. */
            if (sn && SN_TYPE(sn) == SRC_INITPROP) {
                ss->sprinter[head] = '{';
                if (Sprint(&ss->sprinter, "%g: ", d) < 0)
                    return NULL;
            } else {
                /* Sanity check for the gnarly control flow above. */
                LOCAL_ASSERT(i == d);

                /* Fill in any holes (holes at the end don't matter). */
                while (++lasti < i) {
                    if (ss->sprinter.put(", ", 2) < 0)
                        return NULL;
                }
            }
            break;
          }

          case JSOP_GETPROP:
          case JSOP_LENGTH:
          {
            JSAtom *atom;
            LOAD_ATOM(0);
            ss->sprinter[head] = '{';
#if JS_HAS_DESTRUCTURING_SHORTHAND
            nameoff = ss->sprinter.getOffset();
#endif
            if (!QuoteString(&ss->sprinter, atom, IsIdentifier(atom) ? 0 : (jschar)'\''))
                return NULL;
            if (ss->sprinter.put(": ", 2) < 0)
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
        JSBool hole;
        pc = DecompileDestructuringLHS(ss, pc, endpc, &hole, letNames);
        if (!pc)
            return NULL;

#if JS_HAS_DESTRUCTURING_SHORTHAND
        if (nameoff >= 0) {
            ptrdiff_t offset, initlen;

            offset = ss->sprinter.getOffset();
            LOCAL_ASSERT(ss->sprinter[offset] == '\0');
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
                name = ss->sprinter.stringAt(nameoff);
                if (!strncmp(name + namelen, ": ", 2) &&
                    !strncmp(name, name + namelen + 2, namelen)) {
                    offset -= namelen + 2;
                    ss->sprinter[offset] = '\0';
                    ss->sprinter.setOffset(offset);
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
        jssrcnote *sn = js_GetSrcNote(jp->script, pc);
        if (!sn)
            break;
        if (SN_TYPE(sn) != SRC_CONTINUE) {
            LOCAL_ASSERT(SN_TYPE(sn) == SRC_DESTRUCT || SN_TYPE(sn) == SRC_DESTRUCTLET);
            break;
        }

        if (!hole && ss->sprinter.put(", ", 2) < 0)
            return NULL;

        pc += JSOP_DUP_LENGTH;
    }

out:
    const char *lval = ss->sprinter.stringAt(head);
    if (ss->sprinter.put((*lval == '[') ? "]" : "}", 1) < 0)
        return NULL;
    return pc;
}

static jsbytecode *
DecompileGroupAssignment(SprintStack *ss, jsbytecode *pc, jsbytecode *endpc,
                         jssrcnote *sn, ptrdiff_t *todop)
{
    JSOp op;
    const JSCodeSpec *cs;
    unsigned oplen, start, end, i;
    ptrdiff_t todo;
    JSBool hole;
    const char *rval;

    LOAD_OP_DATA(pc);
    LOCAL_ASSERT(op == JSOP_GETLOCAL);

    todo = Sprint(&ss->sprinter, "%s[", VarPrefix(sn));
    if (todo < 0 || !PushOff(ss, todo, JSOP_NOP))
        return NULL;
    ss->sprinter.setOffset(ss->sprinter.getOffset() - PAREN_SLOP);

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
        if (op != JSOP_GETLOCAL)
            break;
        if (!hole && ss->sprinter.put(", ", 2) < 0)
            return NULL;
    }

    LOCAL_ASSERT(op == JSOP_POPN);
    if (ss->sprinter.put("] = [", 5) < 0)
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

    if (ss->sprinter.put("]", 1) < 0)
        return NULL;
    ss->sprinter.setOffset(ss->offsets[i]);
    ss->top = start;
    *todop = todo;
    return pc;
}

#undef LOCAL_ASSERT
#undef LOAD_OP_DATA

#endif /* JS_HAS_DESTRUCTURING */

#define LOCAL_ASSERT(expr)    LOCAL_ASSERT_RV(expr, false)

/*
 * The names of the vars of a let block/expr are stored as the ids of the
 * shapes of the block object. Shapes are stored in a singly-linked list in
 * reverse order of addition. This function takes care of putting the names
 * back in declaration order.
 */
static bool
GetBlockNames(JSContext *cx, StaticBlockObject &blockObj, AtomVector *atoms)
{
    size_t numAtoms = blockObj.slotCount();
    LOCAL_ASSERT(numAtoms > 0);
    if (!atoms->resize(numAtoms))
        return false;

    unsigned i = numAtoms;
    for (Shape::Range r = blockObj.lastProperty()->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        LOCAL_ASSERT(shape.hasShortID());
        --i;
        LOCAL_ASSERT((unsigned)shape.shortid() == i);
        (*atoms)[i] = JSID_IS_INT(shape.propid())
                      ? cx->runtime->atomState.emptyAtom
                      : JSID_TO_ATOM(shape.propid());
    }

    LOCAL_ASSERT(i == 0);
    return true;
}

static bool
PushBlockNames(JSContext *cx, SprintStack *ss, const AtomVector &atoms)
{
    for (size_t i = 0; i < atoms.length(); i++) {
        const char *name = QuoteString(&ss->sprinter, atoms[i], 0);
        if (!name || !PushOff(ss, ss->sprinter.getOffsetOf(name), JSOP_ENTERBLOCK))
            return false;
    }
    return true;
}

/*
 * In the scope of a let, the variables' (decompiler) stack slots must contain
 * the corresponding variable's name. This function updates the N top slots
 * with the N variable names stored in 'atoms'.
 */
static bool
AssignBlockNamesToPushedSlots(JSContext *cx, SprintStack *ss, const AtomVector &atoms)
{
    /* For simplicity, just pop and push. */
    LOCAL_ASSERT(atoms.length() <= (unsigned)ss->top);
    for (size_t i = 0; i < atoms.length(); ++i)
        PopStr(ss, JSOP_NOP);
    return PushBlockNames(cx, ss, atoms);
}

static const char SkipString[] = "/*skip*/";
static const char DestructuredString[] = "/*destructured*/";
static const unsigned DestructuredStringLength = ArrayLength(DestructuredString) - 1;

static ptrdiff_t
SprintLetBody(JSContext *cx, JSPrinter *jp, SprintStack *ss, jsbytecode *pc, ptrdiff_t bodyLength,
              const char *headChars)
{
    if (pc[bodyLength] == JSOP_LEAVEBLOCK) {
        js_printf(jp, "\tlet (%s) {\n", headChars);
        jp->indent += 4;
        if (!Decompile(ss, pc, bodyLength))
            return -1;
        jp->indent -= 4;
        js_printf(jp, "\t}\n");
        return -2;
    }

    LOCAL_ASSERT_RV(pc[bodyLength] == JSOP_LEAVEBLOCKEXPR, -1);
    if (!Decompile(ss, pc, bodyLength))
        return -1;

    const char *bodyChars = PopStr(ss, JSOP_SETNAME);
    const char *format = *bodyChars == '{' ? "let (%s) (%s)" : "let (%s) %s";
    return Sprint(&ss->sprinter, format, headChars, bodyChars);
}

/*
 * Get the token to prefix the '=' in an assignment operation, checking whether
 * the last operation was a getter, setter or compound assignment. For compound
 * assignments, marks parents for the lhs and rhs of the operation in the
 * compound assign. For an assignment such as 'a += b', the lhs will appear
 * twice in the bytecode, in read and write operations. We defer generation of
 * the offsets for the initial arithmetic operation until the entire compound
 * assign has been processed.
 */
static const char *
GetTokenForAssignment(JSPrinter *jp, jssrcnote *sn, JSOp lastop,
                      jsbytecode *pc, jsbytecode *rvalpc,
                      jsbytecode **lastlvalpc, jsbytecode **lastrvalpc)
{
    const char *token;
    if (sn && SN_TYPE(sn) == SRC_ASSIGNOP) {
        if (lastop == JSOP_GETTER) {
            token = js_getter_str;
        } else if (lastop == JSOP_SETTER) {
            token = js_setter_str;
        } else {
            token = CodeToken[lastop];
            if (*lastlvalpc && *lastrvalpc) {
                UpdateDecompiledParent(jp, *lastlvalpc, pc, 0);
                UpdateDecompiledParent(jp, *lastrvalpc, rvalpc, 0);
            }
        }
    } else {
        token = "";
    }
    *lastlvalpc = NULL;
    *lastrvalpc = NULL;
    return token;
}

static ptrdiff_t
SprintNormalFor(JSContext *cx, JSPrinter *jp, SprintStack *ss, const char *initPrefix,
                const char *init, jsbytecode *initpc, jsbytecode **ppc, ptrdiff_t *plen)
{
    jsbytecode *pc = *ppc;
    jssrcnote *sn = js_GetSrcNote(jp->script, pc);
    JS_ASSERT(SN_TYPE(sn) == SRC_FOR);

    /* Print the keyword and the possibly empty init-part. */
    js_printf(jp, "\tfor (%s", initPrefix);
    SprintOpcodePermanent(jp, init, initpc);
    js_printf(jp, ";");

    /* Skip the JSOP_NOP or JSOP_POP bytecode. */
    JS_ASSERT(*pc == JSOP_NOP || *pc == JSOP_POP);
    pc += JSOP_NOP_LENGTH;

    /* Get the cond, next, and loop-closing tail offsets. */
    ptrdiff_t cond = js_GetSrcNoteOffset(sn, 0);
    ptrdiff_t next = js_GetSrcNoteOffset(sn, 1);
    ptrdiff_t tail = js_GetSrcNoteOffset(sn, 2);

    /*
     * If this loop has a condition, then pc points at a goto
     * targeting the condition.
     */
    jsbytecode *pc2 = pc;
    if (cond != tail) {
        LOCAL_ASSERT(*pc == JSOP_GOTO);
        pc2 += JSOP_GOTO_LENGTH;
    }
    LOCAL_ASSERT(tail + GET_JUMP_OFFSET(pc + tail) == pc2 - pc);

    if (cond != tail) {
        /* Decompile the loop condition. */
        if (!Decompile(ss, pc + cond, tail - cond))
            return -1;
        js_printf(jp, " ");
        jsbytecode *condpc;
        const char *cond = PopStr(ss, JSOP_NOP, &condpc);
        SprintOpcodePermanent(jp, cond, condpc);
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
        unsigned saveTop = ss->top;

        if (!Decompile(ss, pc + next, cond - next - JSOP_POP_LENGTH))
            return -1;
        LOCAL_ASSERT(ss->top - saveTop <= 1U);
        jsbytecode *updatepc = NULL;
        const char *update = (ss->top == saveTop)
                             ? ss->sprinter.stringEnd()
                             : PopStr(ss, JSOP_NOP, &updatepc);
        js_printf(jp, " ");
        SprintOpcodePermanent(jp, update, updatepc);
    }

    /* Do the loop body. */
    js_printf(jp, ") {\n");
    jp->indent += 4;
    next -= pc2 - pc;
    if (!Decompile(ss, pc2, next))
        return -1;
    jp->indent -= 4;
    js_printf(jp, "\t}\n");

    /* Set len so pc skips over the entire loop. */
    *ppc = pc;
    *plen = tail + js_CodeSpec[pc[tail]].length;
    return -2;
}

#undef LOCAL_ASSERT

static JSBool
InitSprintStack(JSContext *cx, SprintStack *ss, JSPrinter *jp, unsigned depth)
{
    if (!ss->sprinter.init())
        return JS_FALSE;
    ss->sprinter.setOffset(PAREN_SLOP);

    /* Allocate the parallel (to avoid padding) offset, opcode and bytecode stacks. */
    size_t offsetsz = depth * sizeof(ptrdiff_t);
    size_t opcodesz = depth * sizeof(jsbytecode);
    size_t bytecodesz = depth * sizeof(jsbytecode *);
    void *space = cx->tempLifoAlloc().alloc(offsetsz + opcodesz + bytecodesz);
    if (!space) {
        js_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    ss->offsets = (ptrdiff_t *) space;
    ss->opcodes = (jsbytecode *) ((char *)space + offsetsz);
    ss->bytecodes = (jsbytecode **) ((char *)space + offsetsz + opcodesz);

    ss->top = ss->inArrayInit = 0;
    ss->inGenExp = JS_FALSE;
    ss->printer = jp;
    return JS_TRUE;
}

template <typename T>
void
Swap(T &a, T &b)
{
    T tmp = a;
    a = b;
    b = tmp;
}

/*
 * If nb is non-negative, decompile nb bytecodes starting at pc.  Otherwise
 * the decompiler starts at pc and continues until it reaches an opcode for
 * which decompiling would result in the stack depth equaling -(nb + 1).
 */
static jsbytecode *
Decompile(SprintStack *ss, jsbytecode *pc, int nb)
{
    JSContext *cx;
    JSPrinter *jp, *jp2;
    jsbytecode *startpc, *endpc, *pc2, *done, *lvalpc, *rvalpc, *xvalpc;
    ptrdiff_t tail, todo, len, oplen, cond, next;
    JSOp op, lastop, saveop;
    const JSCodeSpec *cs;
    jssrcnote *sn, *sn2;
    const char *lval, *rval, *xval, *fmt, *token;
    unsigned nuses;
    int i, argc;
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
#define DECOMPILE_CODE_CLEANUP(pc,nb,cleanup) if (!Decompile(ss, pc, nb)) cleanup
#define DECOMPILE_CODE(pc,nb) DECOMPILE_CODE_CLEANUP(pc,nb,return NULL)
#define TOP_STR()             GetStr(ss, ss->top - 1)
#define POP_STR()             PopStr(ss, op)
#define POP_STR_PREC(prec)    PopStrPrec(ss, prec)

/*
 * Given an atom already fetched from jp->script's atom map, quote/escape its
 * string appropriately into rval, and select fmt from the quoted and unquoted
 * alternatives.
 */
#define GET_QUOTE_AND_FMT(qfmt, ufmt, rval)                                   \
    JS_BEGIN_MACRO                                                            \
        jschar quote_;                                                        \
        if (!IsIdentifier(atom)) {                                            \
            quote_ = '\'';                                                    \
            fmt = qfmt;                                                       \
        } else {                                                              \
            quote_ = 0;                                                       \
            fmt = ufmt;                                                       \
        }                                                                     \
        rval = QuoteString(&ss->sprinter, atom, quote_);                      \
        rval = SprintDupeStr(ss, rval);                                       \
        if (!rval)                                                            \
            return NULL;                                                      \
    JS_END_MACRO

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

    jsbytecode *lastlvalpc = NULL, *lastrvalpc = NULL;

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
    bool forOf = false;
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
        saveop = op;
        len = oplen = cs->length;
        nuses = StackUses(jp->script, pc);

        /*
         * Here it is possible that nuses > ss->top when the op has a hidden
         * source note. But when nb < 0 we assume that the caller knows that
         * Decompile would never meet such opcodes.
         */
        if (nb < 0) {
            LOCAL_ASSERT(ss->top >= nuses);
            unsigned ndefs = StackDefs(jp->script, pc);
            if ((unsigned) -(nb + 1) == ss->top - nuses + ndefs)
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
             * the bytecode at pc, or if at an inner opcode of a 'fat' outer
             * opcode at pc, so we don't decompile more than the error
             * expression.
             */
            uint32_t format = cs->format;
            bool matchPC = false;
            if (StackFrame *fp = js_GetScriptedCaller(cx, NULL)) {
                jsbytecode *npc = fp->pcQuadratic(cx);
                if (pc == npc) {
                    matchPC = true;
                } else if (format & JOF_DECOMPOSE) {
                    if (unsigned(npc - pc) < GetDecomposeLength(pc, js_CodeSpec[*pc].length))
                        matchPC = true;
                }
            }
            if ((matchPC || (pc == startpc && nuses != 0)) &&
                format & (JOF_SET|JOF_DEL|JOF_INCDEC|JOF_VARPROP)) {
                uint32_t mode = JOF_MODE(format);
                if (mode == JOF_NAME) {
                    /*
                     * JOF_NAME does not imply JOF_ATOM, so we must check for
                     * the QARG and QVAR format types, and translate those to
                     * JSOP_GETARG or JSOP_GETLOCAL appropriately, instead of
                     * to JSOP_NAME.
                     */
                    uint32_t type = JOF_TYPE(format);
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

        jsbytecode *pushpc = pc;

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
                    rval = PopStr(ss, op, &lastrvalpc);
                    (void)PopStr(ss, op, &lastlvalpc);

                    /* Print only the right operand of the assignment-op. */
                    todo = ss->sprinter.put(rval);
                } else if (!inXML) {
                    rval = PopStrPrecDupe(ss, cs->prec + !!(cs->format & JOF_LEFTASSOC), &rvalpc);
                    lval = PopStrPrec(ss, cs->prec + !(cs->format & JOF_LEFTASSOC), &lvalpc);
                    todo = ss->sprinter.getOffset();
                    SprintOpcode(ss, lval, lvalpc, pc, todo);
                    Sprint(&ss->sprinter, " %s ", token);
                    SprintOpcode(ss, rval, rvalpc, pc, todo);
                } else {
                    /* In XML, just concatenate the two operands. */
                    LOCAL_ASSERT(op == JSOP_ADD);
                    rval = POP_STR();
                    lval = POP_STR();
                    todo = Sprint(&ss->sprinter, ss_format, lval, rval);
                }
                break;

              case 1:
                rval = PopStrDupe(ss, op, &rvalpc);
                todo = ss->sprinter.put(token);
                SprintOpcode(ss, rval, rvalpc, pc, todo);
                break;

              case 0:
                sn = js_GetSrcNote(jp->script, pc);
                if (sn && SN_TYPE(sn) == SRC_CONTINUE) {
                    /* Hoisted let decl (e.g. 'y' in 'let (x) { let y; }'). */
                    todo = ss->sprinter.put(SkipString);
                    break;
                }
                todo = ss->sprinter.put(token);
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
                    LOCAL_ASSERT(pc[tail] == JSOP_IFNE);
                    js_printf(jp, "\tdo {\n");
                    jp->indent += 4;
                    DECOMPILE_CODE(pc, tail);
                    jp->indent -= 4;
                    js_printf(jp, "\t} while (");
                    rval = PopCondStr(ss, &rvalpc);
                    SprintOpcodePermanent(jp, rval, rvalpc);
                    js_printf(jp, ");\n");
                    pc += tail;
                    len = js_CodeSpec[*pc].length;
                    todo = -2;
                    break;

                  case SRC_FOR:
                    /* for loop with empty initializer. */
                    todo = SprintNormalFor(cx, jp, ss, "", "", NULL, &pc, &len);
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
                    if (ok && !jp2->sprinter.empty())
                        js_puts(jp, jp2->sprinter.string());
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

              case JSOP_LABEL:
                sn = js_GetSrcNote(jp->script, pc);
                todo = -2;
                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
                  case SRC_LABEL:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    jp->indent -= 4;
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    ss->sprinter.setOffset(rval);
                    js_printf(jp, "\t%s:\n", rval);
                    jp->indent += 4;
                    break;

                  case SRC_LABELBRACE:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    ss->sprinter.setOffset(rval);
                    js_printf(jp, "\t%s: {\n", rval);
                    jp->indent += 4;
                    break;

                  default:
                    JS_NOT_REACHED("JSOP_LABEL without source note");
                    break;
                }
                break;

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
                /*
                 * JSOP_GOSUB has no effect on the decompiler's string stack
                 * because the next op in bytecode order finds the stack
                 * balanced by a JSOP_RETSUB executed elsewhere.
                 */
                todo = -2;
                break;

              case JSOP_POPN:
              {
                unsigned newtop, oldtop;

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
                    for (unsigned i = newtop; i < oldtop; i++) {
                        rval = ss->sprinter.stringAt(ss->offsets[i]);
                        if (Sprint(&ss->sprinter, ss_format,
                                   (i == newtop) ? "" : ", ",
                                   (i == oldtop - 1 && *rval == '\0')
                                   ? ", " : rval) < 0) {
                            return NULL;
                        }
                    }
                    if (ss->sprinter.put("]", 1) < 0)
                        return NULL;

                    /*
                     * If this is an empty group assignment, we have no stack
                     * budget into which we can push our result string. Adjust
                     * ss->sprinter.offset so that our consumer can find the
                     * empty group assignment decompilation.
                     */
                    if (newtop == oldtop) {
                        ss->sprinter.setOffset(todo);
                    } else {
                        /*
                         * Kill newtop before the end_groupassignment: label by
                         * retracting/popping early.
                         */
                        LOCAL_ASSERT(newtop < oldtop);
                        ss->sprinter.setOffset(GetOff(ss, newtop));
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
                     * Sprinter::put.
                     */
                    rval = ss->sprinter.stringAt(todo);
                    rvalpc = NULL;
                    todo = -2;
                    pc2 = pc + oplen;

                    if (*pc2 == JSOP_NOP) {
                        sn = js_GetSrcNote(jp->script, pc2);
                        if (sn) {
                            if (SN_TYPE(sn) == SRC_FOR) {
                                op = JSOP_NOP;
                                pc = pc2;
                                todo = SprintNormalFor(cx, jp, ss, "", rval, rvalpc, &pc, &len);
                                break;
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
                            todo = ss->sprinter.put(rval);
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
                    ss->sprinter.setOffset(GetOff(ss, newtop));
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
                    rval = PopStr(ss, op, &rvalpc);
                    todo = SprintNormalFor(cx, jp, ss, "", rval, rvalpc, &pc, &len);
                    break;

                  case SRC_PCDELTA:
                    /* Comma operator: use JSOP_POP for correct precedence. */
                    op = JSOP_POP;

                    /* Pop and save to avoid blowing stack depth budget. */
                    lval = PopStrDupe(ss, op, &lvalpc);

                    /*
                     * The offset tells distance to the end of the right-hand
                     * operand of the comma operator.
                     */
                    pushpc = pc;
                    done = pc + len;
                    pc += js_GetSrcNoteOffset(sn, 0);
                    len = 0;

                    if (!Decompile(ss, done, pc - done))
                        return NULL;

                    /* Pop Decompile result and print comma expression. */
                    rval = PopStrDupe(ss, op, &rvalpc);
                    todo = ss->sprinter.getOffset();
                    SprintOpcode(ss, lval, lvalpc, pushpc, todo);
                    ss->sprinter.put(", ");
                    SprintOpcode(ss, rval, rvalpc, pushpc, todo);
                    break;

                  case SRC_HIDDEN:
                    /* Hide this pop, it's from a goto in a with or for/in. */
                    todo = -2;
                    break;

                  case SRC_CONTINUE:
                    /* Pop the stack, don't print: end of a for-let-in. */
                    (void) PopOff(ss, op);
                    todo = -2;
                    break;

                  default:
                  {
                    /* Turn off parens around a yield statement. */
                    if (ss->opcodes[ss->top-1] == JSOP_YIELD)
                        op = JSOP_NOP;

                    jsbytecode *rvalpc;
                    rval = PopStr(ss, op, &rvalpc);

                    /*
                     * Don't emit decompiler-pushed strings that are not
                     * handled by other opcodes. They are pushed onto the
                     * stack to help model the interpreter stack and should
                     * not appear in the decompiler's output.
                     */
                    if (*rval != '\0' && (rval[0] != '/' || rval[1] != '*')) {
                        bool parens =
                            *rval == '{' ||
                            (strncmp(rval, js_function_str, 8) == 0 &&
                             rval[8] == ' ');
                        js_printf(jp, parens ? "\t(" : "\t");
                        SprintOpcodePermanent(jp, rval, rvalpc);
                        js_printf(jp, parens ? ");\n" : ";\n");
                    } else {
                        LOCAL_ASSERT(*rval == '\0' ||
                                     strcmp(rval, exception_cookie) == 0);
                    }
                    todo = -2;
                    break;
                  }
                }
                sn = NULL;
                break;

              case JSOP_ENTERWITH:
                LOCAL_ASSERT(!js_GetSrcNote(jp->script, pc));
                rval = PopStr(ss, op, &rvalpc);
                js_printf(jp, "\twith (");
                SprintOpcodePermanent(jp, rval, rvalpc);
                js_printf(jp, ") {\n");
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
                obj = jp->script->getObject(GET_UINT32_INDEX(pc));
                AtomVector atoms(cx);
                StaticBlockObject &blockObj = obj->asStaticBlock();

                if (!GetBlockNames(cx, blockObj, &atoms) || !PushBlockNames(cx, ss, atoms))
                    return NULL;

                sn = js_GetSrcNote(jp->script, pc);
                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
#if JS_HAS_BLOCK_SCOPE
                  case SRC_BRACE:
                    js_printf(jp, "\t{\n");
                    jp->indent += 4;
                    len = js_GetSrcNoteOffset(sn, 0);
                    if (!Decompile(ss, pc + oplen, len - oplen))
                        return NULL;
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");
                    break;
#endif

                  case SRC_CATCH:
                    jp->indent -= 4;
                    js_printf(jp, "\t} catch (");

                    pc2 = pc;
                    pc += oplen;
                    LOCAL_ASSERT(*pc == JSOP_EXCEPTION);
                    pc += JSOP_EXCEPTION_LENGTH;
                    todo = Sprint(&ss->sprinter, exception_cookie);
                    if (todo < 0 || !PushOff(ss, todo, JSOP_EXCEPTION))
                        return NULL;

                    if (*pc == JSOP_DUP) {
                        sn2 = js_GetSrcNote(jp->script, pc);
                        if (!sn2 || SN_TYPE(sn2) != SRC_DESTRUCT) {
                            /*
                             * This is a dup to save the exception for later.
                             * It is emitted only when the catch head contains
                             * an exception guard.
                             */
                            LOCAL_ASSERT(js_GetSrcNoteOffset(sn, 0) != 0);
                            pc += JSOP_DUP_LENGTH;
                            todo = Sprint(&ss->sprinter, exception_cookie);
                            if (todo < 0 || !PushOff(ss, todo, JSOP_EXCEPTION))
                                return NULL;
                        }
                    }

#if JS_HAS_DESTRUCTURING
                    if (*pc == JSOP_DUP) {
                        pc = DecompileDestructuring(ss, pc, endpc);
                        if (!pc)
                            return NULL;
                        LOCAL_ASSERT(*pc == JSOP_POP);
                        pc += JSOP_POP_LENGTH;
                        lval = PopStr(ss, JSOP_NOP);
                        js_puts(jp, lval);
                    } else {
#endif
                        LOCAL_ASSERT(*pc == JSOP_SETLOCALPOP);
                        pc += JSOP_SETLOCALPOP_LENGTH;
                        LOCAL_ASSERT(blockObj.slotCount() >= 1);
                        if (!QuoteString(&jp->sprinter, atoms[0], 0))
                            return NULL;
#if JS_HAS_DESTRUCTURING
                    }
#endif

                    /*
                     * Pop the exception_cookie (or its dup in the case of a
                     * guarded catch head) off the stack now.
                     */
                    rval = PopStr(ss, JSOP_NOP);
                    LOCAL_ASSERT(strcmp(rval, exception_cookie) == 0);

                    len = js_GetSrcNoteOffset(sn, 0);
                    if (len) {
                        len -= pc - pc2;
                        LOCAL_ASSERT(len > 0);
                        js_printf(jp, " if ");
                        if (!Decompile(ss, pc, len))
                            return NULL;
                        js_printf(jp, "%s", POP_STR());
                        pc += len;
                        LOCAL_ASSERT(*pc == JSOP_IFEQ);
                        pc += js_CodeSpec[*pc].length;
                    }

                    js_printf(jp, ") {\n");
                    jp->indent += 4;
                    len = 0;
                    break;
                  default:;
                }

                todo = -2;
              }
              break;

              case JSOP_LEAVEBLOCK:
              case JSOP_LEAVEBLOCKEXPR:
              {
                unsigned top, depth;

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
                    if ((unsigned)js_GetSrcNoteOffset(sn, 0) != ss->top) {
                        LOCAL_ASSERT((unsigned)js_GetSrcNoteOffset(sn, 0)
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
                ss->sprinter.setOffset(GetOff(ss, top));
                if (op == JSOP_LEAVEBLOCKEXPR)
                    todo = ss->sprinter.put(rval);
                break;
              }

              case JSOP_ENTERLET0:
              {
                obj = jp->script->getObject(GET_UINT32_INDEX(pc));
                StaticBlockObject &blockObj = obj->asStaticBlock();

                AtomVector atoms(cx);
                if (!GetBlockNames(cx, blockObj, &atoms))
                    return NULL;

                sn = js_GetSrcNote(jp->script, pc);
                LOCAL_ASSERT(SN_TYPE(sn) == SRC_DECL);
                ptrdiff_t letData = js_GetSrcNoteOffset(sn, 0);
                bool groupAssign = LetDataToGroupAssign(letData);
                unsigned letDepth = blockObj.stackDepth();
                LOCAL_ASSERT(letDepth == (unsigned)ss->top - blockObj.slotCount());
                LOCAL_ASSERT(atoms.length() == blockObj.slotCount());

                /*
                 * Build the list of decompiled rhs expressions. Do this before
                 * sprinting the let-head since GetStr can inject stuff on top
                 * of the stack (in case js_DecompileValueGenerator).
                 */
                Vector<const char *> rhsExprs(cx);
                if (!rhsExprs.resize(atoms.length()))
                    return NULL;
                for (size_t i = 0; i < rhsExprs.length(); ++i)
                    rhsExprs[i] = SprintDupeStr(ss, GetStr(ss, letDepth + i));

                /* Build the let head starting at headBegin. */
                ptrdiff_t headBegin = ss->sprinter.getOffset();

                /*
                 * For group assignment, prepend the '[lhs-vars] = [' here,
                 * append rhsExprs in the next loop and append ']' after.
                 */
                if (groupAssign) {
                    if (Sprint(&ss->sprinter, "[") < 0)
                        return NULL;
                    for (size_t i = 0; i < atoms.length(); ++i) {
                        if (i && Sprint(&ss->sprinter, ", ") < 0)
                            return NULL;
                        if (!QuoteString(&ss->sprinter, atoms[i], 0))
                            return NULL;
                    }
                    if (Sprint(&ss->sprinter, "] = [") < 0)
                        return NULL;
                }

                for (size_t i = 0; i < atoms.length(); ++i) {
                    const char *rhs = rhsExprs[i];
                    if (!strcmp(rhs, SkipString))
                        continue;

                    if (i && Sprint(&ss->sprinter, ", ") < 0)
                        return NULL;

                    if (groupAssign) {
                        if (ss->sprinter.put(rhs) < 0)
                            return NULL;
                    } else if (!strncmp(rhs, DestructuredString, DestructuredStringLength)) {
                        if (ss->sprinter.put(rhs + DestructuredStringLength) < 0)
                            return NULL;
                    } else {
                        JS_ASSERT(atoms[i] != cx->runtime->atomState.emptyAtom);
                        if (!QuoteString(&ss->sprinter, atoms[i], 0))
                            return NULL;
                        if (*rhs) {
                            uint8_t prec = js_CodeSpec[ss->opcodes[letDepth + i]].prec;
                            const char *fmt = prec && prec < js_CodeSpec[JSOP_SETLOCAL].prec
                                              ? " = (%s)"
                                              : " = %s";
                            if (Sprint(&ss->sprinter, fmt, rhs) < 0)
                                return NULL;
                        }
                    }
                }

                if (groupAssign && Sprint(&ss->sprinter, "]") < 0)
                    return NULL;

                /* Clone the let head chars before clobbering the stack. */
                DupBuffer head(cx);
                if (!Dup(ss->sprinter.stringAt(headBegin), &head))
                    return NULL;
                if (!AssignBlockNamesToPushedSlots(cx, ss, atoms))
                    return NULL;

                /* Detect 'for (let ...)' desugared into 'let (...) {for}'. */
                jsbytecode *nextpc = pc + JSOP_ENTERLET0_LENGTH;
                if (*nextpc == JSOP_NOP) {
                    jssrcnote *nextsn = js_GetSrcNote(jp->script, nextpc);
                    if (nextsn && SN_TYPE(nextsn) == SRC_FOR) {
                        pc = nextpc;
                        todo = SprintNormalFor(cx, jp, ss, "let ", head.begin(), pc, &pc, &len);
                        break;
                    }
                }

                /* Decompile the body and then complete the let block/expr. */
                len = LetDataToOffset(letData);
                pc = nextpc;
                saveop = (JSOp) pc[len];
                todo = SprintLetBody(cx, jp, ss, pc, len, head.begin());
                break;
              }

              /*
               * With 'for (let lhs in rhs)' and 'switch (c) { let-decl }',
               * placeholder slots have already been pushed (by JSOP_UNDEFINED).
               * In both the for-let-in and switch-hoisted-let cases:
               *  - there is a non-let slot on top of the stack (hence enterlet1)
               *  - there is no further special let-handling required:
               *    for-let-in will decompile the let head when it decompiles
               *    the loop body prologue; there is no let head to decompile
               *    with switch.
               * Hence, the only thing to do is update the let vars' slots with
               * their names, taking care to preserve the iter/condition value
               * on top of the stack.
               */
              case JSOP_ENTERLET1:
              {
                obj = jp->script->getObject(GET_UINT32_INDEX(pc));
                StaticBlockObject &blockObj = obj->asStaticBlock();

                AtomVector atoms(cx);
                if (!GetBlockNames(cx, blockObj, &atoms))
                    return NULL;

                LOCAL_ASSERT(js_GetSrcNote(jp->script, pc) == NULL);
                LOCAL_ASSERT(ss->top - 1 == blockObj.stackDepth() + blockObj.slotCount());
                jsbytecode *nextpc = pc + JSOP_ENTERLET1_LENGTH;
                if (*nextpc == JSOP_GOTO) {
                    LOCAL_ASSERT(SN_TYPE(js_GetSrcNote(jp->script, nextpc)) == SRC_FOR_IN);
                } else {
                    LOCAL_ASSERT(*nextpc == JSOP_CONDSWITCH ||
                                 *nextpc == JSOP_TABLESWITCH ||
                                 *nextpc == JSOP_LOOKUPSWITCH);
                }

                DupBuffer rhs(cx);
                if (!Dup(PopStr(ss, JSOP_NOP), &rhs))
                    return NULL;
                if (!AssignBlockNamesToPushedSlots(cx, ss, atoms))
                    return NULL;
                if (!PushStr(ss, rhs.begin(), op))
                    return NULL;
                todo = -2;
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

                unsigned index = GET_UINT16(pc);
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
                LOCAL_ASSERT((unsigned)i < ss->top);
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
                rval = PopStrDupe(ss, op, &rvalpc);
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
                    rval = PopStr(ss, op, &rvalpc);
                    bool parens = (*rval == '{');
                    if (parens)
                        js_printf(jp, "(");
                    SprintOpcodePermanent(jp, rval, rvalpc);
                    js_printf(jp, parens ? ")%s" : "%s",
                              ((fun->flags & JSFUN_LAMBDA) || !fun->atom)
                              ? ""
                              : ";");
                    todo = -2;
                    break;
                }
                /* FALL THROUGH */

              case JSOP_SETRVAL:
                rval = PopStr(ss, op, &rvalpc);
                if (*rval != '\0') {
                    js_printf(jp, "\t%s ", js_return_str);
                    SprintOpcodePermanent(jp, rval, rvalpc);
                    js_printf(jp, ";\n");
                } else {
                    js_printf(jp, "\t%s;\n", js_return_str);
                }
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
                           : ss->sprinter.put(js_yield_str);
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
                unsigned pos = ss->top;
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
                unsigned forpos = pos + 1;
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
                    xval = ss->sprinter.stringAt(ss->offsets[forpos]);
                    ss->sprinter.setOffset(PAREN_SLOP);
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
                LOCAL_ASSERT(ss->sprinter[start] == '[' ||
                             ss->sprinter[start] == '#');
                LOCAL_ASSERT(forpos < ss->top);
                xval = ss->sprinter.stringAt(ss->offsets[forpos]);
                lval = ss->sprinter.stringAt(start);
                ss->sprinter.setOffset(lval);

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
                rval = PopStr(ss, op, &rvalpc);
                js_printf(jp, "\t%s ", js_throw_str);
                SprintOpcodePermanent(jp, rval, rvalpc);
                js_printf(jp, ";\n");
                break;

              case JSOP_ITER:
                forOf = (GET_UINT8(pc) == JSITER_FOR_OF);
                foreach = (GET_UINT8(pc) & (JSITER_FOREACH | JSITER_KEYVALUE)) ==
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
                    cond = GET_JUMP_OFFSET(pc);
                    next = js_GetSrcNoteOffset(sn, 0);
                    tail = js_GetSrcNoteOffset(sn, 1);
                    JS_ASSERT(pc[next] == JSOP_POP);
                    JS_ASSERT(pc[cond] == JSOP_LOOPENTRY);
                    cond += JSOP_LOOPENTRY_LENGTH;
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
                            ss->sprinter.setOffset(ss->offsets[ss->top] - PAREN_SLOP);
                            if (Sprint(&ss->sprinter, " %s (%s %s %s)",
                                       foreach ? js_for_each_str : js_for_str,
                                       lval,
                                       forOf ? "of" : "in",
                                       rval) < 0) {
                                return NULL;
                            }

                            /*
                             * Do not AddParenSlop here, as we will push the
                             * top-most offset again, which will add paren slop
                             * for us. We must push to balance the stack budget
                             * when nesting for heads in a comprehension.
                             */
                            todo = ss->offsets[ss->top - 1];
                        } else {
                            todo = Sprint(&ss->sprinter, " %s (%s %s %s)",
                                          foreach ? js_for_each_str : js_for_str,
                                          lval,
                                          forOf ? "of" : "in",
                                          rval);
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
                        js_printf(jp, "\t%s (%s%s %s %s) {\n",
                                  foreach ? js_for_each_str : js_for_str,
                                  xval,
                                  lval,
                                  forOf ? "of" : "in",
                                  rval);
                        jp->indent += 4;
                        DECOMPILE_CODE(pc + next + JSOP_POP_LENGTH, cond - next - JSOP_POP_LENGTH);
                        jp->indent -= 4;
                        js_printf(jp, "\t}\n");
                    }

                    pc += tail;
                    LOCAL_ASSERT(*pc == JSOP_IFNE);
                    len = js_CodeSpec[*pc].length;
                    break;

                  case SRC_WHILE:
                    cond = GET_JUMP_OFFSET(pc);
                    tail = js_GetSrcNoteOffset(sn, 0);
                    DECOMPILE_CODE(pc + cond, tail - cond);
                    js_printf(jp, "\twhile (");
                    rval = PopCondStr(ss, &rvalpc);
                    SprintOpcodePermanent(jp, rval, rvalpc);
                    js_printf(jp, ") {\n");
                    jp->indent += 4;
                    DECOMPILE_CODE(pc + oplen, cond - oplen);
                    jp->indent -= 4;
                    js_printf(jp, "\t}\n");
                    pc += tail;
                    LOCAL_ASSERT(*pc == JSOP_IFNE);
                    len = js_CodeSpec[*pc].length;
                    todo = -2;
                    break;

                  case SRC_CONT2LABEL:
                    GET_SOURCE_NOTE_ATOM(sn, atom);
                    rval = QuoteString(&ss->sprinter, atom, 0);
                    if (!rval)
                        return NULL;
                    ss->sprinter.setOffset(rval);
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
                    ss->sprinter.setOffset(rval);
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
              {
                JSBool elseif = JS_FALSE;

              if_again:
                len = GET_JUMP_OFFSET(pc);
                sn = js_GetSrcNote(jp->script, pc);

                switch (sn ? SN_TYPE(sn) : SRC_NULL) {
                  case SRC_IF:
                  case SRC_IF_ELSE:
                    rval = PopCondStr(ss, &rvalpc);
                    if (ss->inArrayInit || ss->inGenExp) {
                        LOCAL_ASSERT(SN_TYPE(sn) == SRC_IF);
                        ss->sprinter.setOffset(ss->sprinter.getOffset() - PAREN_SLOP);
                        if (Sprint(&ss->sprinter, " if (%s)", rval) < 0)
                            return NULL;
                        AddParenSlop(ss);
                    } else {
                        js_printf(jp, elseif ? " if (" : "\tif (");
                        SprintOpcodePermanent(jp, rval, rvalpc);
                        js_printf(jp, ") {\n");
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
                        LOCAL_ASSERT(*pc == JSOP_GOTO);
                        oplen = js_CodeSpec[*pc].length;
                        len = GET_JUMP_OFFSET(pc);
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
                    xval = PopStrDupe(ss, op, &xvalpc);
                    len = js_GetSrcNoteOffset(sn, 0);
                    DECOMPILE_CODE(pc + oplen, len - oplen);
                    lval = PopStrDupe(ss, op, &lvalpc);
                    pushpc = pc;
                    pc += len;
                    LOCAL_ASSERT(*pc == JSOP_GOTO);
                    oplen = js_CodeSpec[*pc].length;
                    len = GET_JUMP_OFFSET(pc);
                    DECOMPILE_CODE(pc + oplen, len - oplen);
                    rval = PopStrDupe(ss, op, &rvalpc);
                    todo = ss->sprinter.getOffset();
                    SprintOpcode(ss, xval, xvalpc, pushpc, todo);
                    ss->sprinter.put(" ? ");
                    SprintOpcode(ss, lval, lvalpc, pushpc, todo);
                    ss->sprinter.put(" : ");
                    SprintOpcode(ss, rval, rvalpc, pushpc, todo);
                    break;

                  default:
                    break;
                }
                break;
              }

              case JSOP_IFNE:
                LOCAL_ASSERT(0);
                break;

              case JSOP_OR:
                xval = "||";

              do_logical_connective:
                /* Top of stack is the first clause in a disjunction (||). */
                lval = PopStrDupe(ss, op, &lvalpc);
                done = pc + GET_JUMP_OFFSET(pc);
                pushpc = pc;
                pc += len;
                JS_ASSERT(*pc == JSOP_POP);
                pc += JSOP_POP_LENGTH;
                len = done - pc;
                if (!Decompile(ss, pc, len))
                    return NULL;
                rval = PopStrDupe(ss, op, &rvalpc);
                if (!rval)
                    return NULL;
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, lval, lvalpc, pushpc, todo);
                if (jp->pretty &&
                    jp->indent + 4 + strlen(lval) + 4 + strlen(rval) > 75) {
                    Sprint(&ss->sprinter, " %s\n", xval);
                    Sprint(&ss->sprinter, "%*s", jp->indent + 4, "");
                } else {
                    Sprint(&ss->sprinter, " %s ", xval);
                }
                SprintOpcode(ss, rval, rvalpc, pushpc, todo);
                break;

              case JSOP_AND:
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
                    todo = ss->sprinter.put(lval);
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
                todo = ss->sprinter.put(rval);
                if (todo < 0 || !PushOff(ss, todo,
                                         (JSOp) ss->opcodes[ss->top-2])) {
                    return NULL;
                }
                /* FALL THROUGH */

              case JSOP_DUP:
#if JS_HAS_DESTRUCTURING
                sn = js_GetSrcNote(jp->script, pc);
                if (sn) {
                    if (SN_TYPE(sn) == SRC_DESTRUCT) {
                        pc = DecompileDestructuring(ss, pc, endpc);
                        if (!pc)
                            return NULL;

                        lval = POP_STR();  /* Pop the decompiler result. */
                        rval = POP_STR();  /* Pop the initializer expression. */

                        if (strcmp(rval, forelem_cookie) == 0) {
                            todo = Sprint(&ss->sprinter, ss_format,
                                          VarPrefix(sn), lval);

                            /* Skip POP so the SRC_FOR_IN code can pop for itself. */
                            if (*pc == JSOP_POP)
                                len = JSOP_POP_LENGTH;
                        } else {
                            todo = Sprint(&ss->sprinter, "%s%s = %s",
                                          VarPrefix(sn), lval, rval);
                        }

                        op = saveop = JSOP_ENUMELEM;
                        len = 0;
                    } else {
                        LOCAL_ASSERT(SN_TYPE(sn) == SRC_DESTRUCTLET);

                        ptrdiff_t offsetToLet = js_GetSrcNoteOffset(sn, 0);
                        LOCAL_ASSERT(*(pc + offsetToLet) == JSOP_ENTERLET0);

                        obj = jp->script->getObject(GET_UINT32_INDEX(pc + offsetToLet));
                        StaticBlockObject &blockObj = obj->asStaticBlock();

                        uint32_t blockDepth = blockObj.stackDepth();
                        LOCAL_ASSERT(blockDepth < ss->top);
                        LOCAL_ASSERT(ss->top <= blockDepth + blockObj.slotCount());

                        AtomVector atoms(cx);
                        if (!GetBlockNames(cx, blockObj, &atoms))
                            return NULL;

                        /*
                         * Skip any initializers preceding this one. E.g., in
                         *   let (w=1, x=2, [y,z] = a) { ... }
                         * skip 'w' and 'x' for the JSOP_DUP of '[y,z] = a'.
                         */
                        AtomRange letNames = atoms.all();
                        uint32_t curDepth = ss->top - 1 /* initializer */;
                        for (uint32_t i = blockDepth; i < curDepth; ++i)
                            letNames.popFront();

                        /*
                         * Pop and copy the rhs before it gets clobbered.
                         * Use JSOP_SETLOCAL's precedence since this is =.
                         */
                        DupBuffer rhs(cx);
                        if (!Dup(PopStr(ss, JSOP_SETLOCAL), &rhs))
                            return NULL;

                        /* Destructure, tracking how many vars were bound. */
                        size_t remainBefore = letNames.remain();
                        pc = DecompileDestructuring(ss, pc, endpc, &letNames);
                        if (!pc)
                            return NULL;
                        size_t remainAfter = letNames.remain();

                        /*
                         * Merge the lhs and rhs and prefix with a cookie to
                         * tell enterlet0 not to prepend "name = ".
                         */
                        const char *lhs = PopStr(ss, JSOP_NOP);
                        ptrdiff_t off = Sprint(&ss->sprinter, "%s%s = %s",
                                               DestructuredString, lhs, rhs.begin());
                        if (off < 0 || !PushOff(ss, off, JSOP_NOP))
                            return NULL;

                        /*
                         * Only one slot has been pushed (holding the entire
                         * decompiled destructuring expression). However, the
                         * abstract depth needs one slot per bound var, so push
                         * empty strings for the remainder. We don't have to
                         * worry about empty destructuring because the parser
                         * ensures that there is always at least one pushed
                         * slot for each destructuring lhs.
                         */
                        LOCAL_ASSERT(remainBefore >= remainAfter);
                        LOCAL_ASSERT(remainBefore > remainAfter || remainAfter > 0);
                        for (size_t i = remainBefore - 1; i > remainAfter; --i) {
                            if (!PushStr(ss, SkipString, JSOP_NOP))
                                return NULL;
                        }

                        LOCAL_ASSERT(*pc == JSOP_POP);
                        pc += JSOP_POP_LENGTH;

                        /* Eat up the JSOP_UNDEFINED following empty destructuring. */
                        if (remainBefore == remainAfter) {
                            LOCAL_ASSERT(*pc == JSOP_UNDEFINED);
                            pc += JSOP_UNDEFINED_LENGTH;
                        }

                        len = 0;
                        todo = -2;
                    }
                    break;
                }
#endif

                rval = GetStr(ss, ss->top-1);
                saveop = (JSOp) ss->opcodes[ss->top-1];
                todo = ss->sprinter.put(rval);
                break;

              case JSOP_SWAP:
                Swap(ss->offsets[ss->top-1], ss->offsets[ss->top-2]);
                Swap(ss->opcodes[ss->top-1], ss->opcodes[ss->top-2]);
                Swap(ss->bytecodes[ss->top-1], ss->bytecodes[ss->top-2]);
                todo = -2;
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
                rval = PopStrDupe(ss, op, &rvalpc);
                if (op == JSOP_SETNAME || op == JSOP_SETGNAME)
                    (void) PopOff(ss, op);

              do_setlval:
                sn = js_GetSrcNote(jp->script, pc - 1);
                todo = ss->sprinter.getOffset();
                if (sn && SN_TYPE(sn) == SRC_ASSIGNOP) {
                    const char *token =
                        GetTokenForAssignment(jp, sn, lastop, pc, rvalpc,
                                              &lastlvalpc, &lastrvalpc);
                    Sprint(&ss->sprinter, "%s %s= ", lval, token);
                    SprintOpcode(ss, rval, rvalpc, pc, todo);
                } else {
                    sn = js_GetSrcNote(jp->script, pc);
                    const char *prefix = VarPrefix(sn);
                    Sprint(&ss->sprinter, "%s%s = ", prefix, lval);
                    SprintOpcode(ss, rval, rvalpc, pc, todo);
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
              {
                argc = GET_ARGC(pc);
                const char **argv = (const char **)
                    cx->malloc_((size_t)(argc + 1) * sizeof *argv);
                if (!argv)
                    return NULL;
                jsbytecode **argbytecodes = (jsbytecode **)
                    cx->malloc_((size_t)(argc + 1) * sizeof *argbytecodes);
                if (!argbytecodes) {
                    cx->free_(argv);
                    return NULL;
                }

                op = JSOP_SETNAME;
                for (i = argc; i > 0; i--)
                    argv[i] = PopStrDupe(ss, op, &argbytecodes[i]);

                /* Skip the JSOP_PUSHOBJ-created empty string. */
                LOCAL_ASSERT(ss->top >= 2);
                (void) PopOff(ss, op);

                /*
                 * Special case: new (x(y)(z)) must be parenthesized like so.
                 * Same for new (x(y).z) -- contrast with new x(y).z.
                 * See PROPAGATE_CALLNESS.
                 */
                op = (JSOp) ss->opcodes[ss->top - 1];
                argv[0] = PopStrDupe(ss,
                                     (saveop == JSOP_NEW &&
                                      (op == JSOP_CALL ||
                                       op == JSOP_EVAL ||
                                       op == JSOP_FUNCALL ||
                                       op == JSOP_FUNAPPLY ||
                                       op == JSOP_CALLPROP ||
                                       op == JSOP_CALLELEM))
                                     ? JSOP_NAME
                                     : saveop,
                                     &lvalpc);
                op = saveop;

                lval = "(", rval = ")";
                todo = ss->sprinter.getOffset();
                if (op == JSOP_NEW) {
                    if (argc == 0)
                        lval = rval = "";
                    Sprint(&ss->sprinter, "%s ", js_new_str);
                }
                SprintOpcode(ss, argv[0], lvalpc, pc, todo);
                ss->sprinter.put(lval);

                for (i = 1; i <= argc; i++) {
                    SprintOpcode(ss, argv[i], argbytecodes[i], pc, todo);
                    if (i < argc)
                        ss->sprinter.put(", ");
                }
                ss->sprinter.put(rval);

                cx->free_(argv);
                cx->free_(argbytecodes);

                break;
              }

              case JSOP_SETCALL:
                todo = Sprint(&ss->sprinter, "");
                break;

              case JSOP_DELNAME:
                LOAD_ATOM(0);
                lval = QuoteString(&ss->sprinter, atom, 0);
                if (!lval)
                    return NULL;
                ss->sprinter.setOffset(lval);
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
              {
                const char *prefix = (op == JSOP_VOID) ? js_void_str : js_typeof_str;
                rval = PopStrDupe(ss, op, &rvalpc);
                todo = ss->sprinter.getOffset();
                Sprint(&ss->sprinter, "%s ", prefix);
                SprintOpcode(ss, rval, rvalpc, pc, todo);
                break;
              }

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
                ss->sprinter.setOffset(lval);
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
                ss->sprinter.setOffset(lval);
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

                GET_QUOTE_AND_FMT("[%s]", ".%s", rval);
                PROPAGATE_CALLNESS();
                lval = PopStr(ss, op, &lvalpc);
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, lval, lvalpc, pc, todo);
                Sprint(&ss->sprinter, fmt, rval);
                break;

              case JSOP_SETPROP:
              case JSOP_SETMETHOD:
              {
                LOAD_ATOM(0);
                GET_QUOTE_AND_FMT("[%s] %s= ", ".%s %s= ", xval);
                rval = PopStrDupe(ss, op, &rvalpc);

                /*
                 * Force precedence below the numeric literal opcodes, so that
                 * 42..foo or 10000..toString(16), e.g., decompile with parens
                 * around the left-hand side of dot.
                 */
                op = JSOP_GETPROP;
                lval = PopStr(ss, op, &lvalpc);
                sn = js_GetSrcNote(jp->script, pc - 1);
                const char *token =
                    GetTokenForAssignment(jp, sn, lastop, pc, rvalpc,
                                          &lastlvalpc, &lastrvalpc);
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, lval, lvalpc, pc, todo);
                Sprint(&ss->sprinter, fmt, xval, token);
                SprintOpcode(ss, rval, rvalpc, pc, todo);
                break;
              }

              case JSOP_GETELEM2:
                (void) PopOff(ss, lastop);
                /* FALL THROUGH */
              case JSOP_CALLELEM:
              case JSOP_GETELEM:
                op = JSOP_NOP;          /* turn off parens */
                xval = PopStrDupe(ss, op, &xvalpc);
                op = saveop;
                PROPAGATE_CALLNESS();
                lval = PopStr(ss, op, &lvalpc);
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, lval, lvalpc, pc, todo);
                if (*xval != '\0') {
                    bool xml = (JOF_OPMODE(lastop) == JOF_XMLNAME);
                    ss->sprinter.put(xml ? "." : "[");
                    SprintOpcode(ss, xval, xvalpc, pc, todo);
                    ss->sprinter.put(xml ? "" : "]");
                }
                break;

              case JSOP_SETELEM:
              {
                rval = PopStrDupe(ss, op, &rvalpc);
                op = JSOP_NOP;          /* turn off parens */
                xval = PopStrDupe(ss, op, &xvalpc);
                cs = &js_CodeSpec[ss->opcodes[ss->top]];
                op = JSOP_GETELEM;      /* lval must have high precedence */
                lval = PopStr(ss, op, &lvalpc);
                op = saveop;
                if (*xval == '\0')
                    goto do_setlval;
                sn = js_GetSrcNote(jp->script, pc - 1);
                bool xml = (JOF_MODE(cs->format) == JOF_XMLNAME);
                const char *token =
                    GetTokenForAssignment(jp, sn, lastop, pc, rvalpc,
                                          &lastlvalpc, &lastrvalpc);
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, lval, lvalpc, pc, todo);
                ss->sprinter.put(xml ? "." : "[");
                SprintOpcode(ss, xval, xvalpc, pc, todo);
                ss->sprinter.put(xml ? "" : "]");
                Sprint(&ss->sprinter, " %s= ", token);
                SprintOpcode(ss, rval, rvalpc, pc, todo);
                break;
              }

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
                ss->sprinter.setOffset(rval);
                todo = Sprint(&ss->sprinter, sss_format,
                              VarPrefix(sn), lval, rval);
                break;

              case JSOP_UINT16:
                i = (int) GET_UINT16(pc);
                goto do_sprint_int;

              case JSOP_UINT24:
                i = (int) GET_UINT24(pc);
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
                val = jp->script->getConst(GET_UINT32_INDEX(pc));
                todo = SprintDoubleValue(&ss->sprinter, val, &saveop);
                break;
              }

              case JSOP_STRING:
                LOAD_ATOM(0);
                rval = QuoteString(&ss->sprinter, atom, inXML ? DONT_ESCAPE : '"');
                if (!rval)
                    return NULL;
                todo = ss->sprinter.getOffsetOf(rval);
                break;

              case JSOP_LAMBDA:
              case JSOP_LAMBDA_FC:
#if JS_HAS_GENERATOR_EXPRS
                sn = js_GetSrcNote(jp->script, pc);
                if (sn && SN_TYPE(sn) == SRC_GENEXP) {
                    Vector<JSAtom *> *innerLocalNames;
                    Vector<JSAtom *> *outerLocalNames;
                    JSScript *inner, *outer;
                    Vector<DecompiledOpcode> *decompiledOpcodes;
                    SprintStack ss2(cx);
                    JSFunction *outerfun;

                    fun = jp->script->getFunction(GET_UINT32_INDEX(pc));

                    /*
                     * All allocation when decompiling is LIFO, using malloc or,
                     * more commonly, arena-allocating from cx->tempLifoAlloc
                     * Therefore after InitSprintStack succeeds, we must release
                     * to mark before returning.
                     */
                    LifoAllocScope las(&cx->tempLifoAlloc());
                    if (fun->script()->bindings.hasLocalNames()) {
                        innerLocalNames = cx->new_<Vector<JSAtom *> >(cx);
                        if (!innerLocalNames ||
                            !fun->script()->bindings.getLocalNameArray(cx, innerLocalNames))
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
                    decompiledOpcodes = jp->decompiledOpcodes;
                    LOCAL_ASSERT(UnsignedPtrDiff(pc, outer->code) <= outer->length);
                    jp->script = inner;
                    jp->fun = fun;
                    jp->localNames = innerLocalNames;
                    jp->decompiledOpcodes = NULL;

                    /*
                     * Decompile only the main bytecode, to avoid tripping over
                     * new prolog ops that have stack effects.
                     */
                    ok = Decompile(&ss2, inner->main(), inner->length - inner->mainOffset)
                         != NULL;
                    jp->script = outer;
                    jp->fun = outerfun;
                    jp->localNames = outerLocalNames;
                    jp->decompiledOpcodes = decompiledOpcodes;
                    if (!ok)
                        return NULL;

                    /*
                     * Advance over this op and its global |this| push, and
                     * arrange to advance over the call to this lambda.
                     */
                    pc += len;
                    LOCAL_ASSERT(*pc == JSOP_UNDEFINED);
                    pc += JSOP_UNDEFINED_LENGTH;
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
                    if (op == JSOP_LOOPHEAD || op == JSOP_NOP)
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
                     * Alas, we have to malloc a copy of the result left on the
                     * top of ss2 because both ss and ss2 arena-allocate from
                     * cx's tempLifoAlloc
                     */
                    rval = JS_strdup(cx, PopStr(&ss2, op));
                    las.releaseEarly();
                    if (!rval)
                        return NULL;
                    todo = ss->sprinter.put(rval);
                    cx->free_((void *)rval);
                    break;
                }
#endif /* JS_HAS_GENERATOR_EXPRS */
                /* FALL THROUGH */

                fun = jp->script->getFunction(GET_UINT32_INDEX(pc));
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
                todo = ss->sprinter.putString(str);
                break;

              case JSOP_CALLEE:
                JS_ASSERT(jp->fun && jp->fun->atom);
                todo = ss->sprinter.putString(jp->fun->atom);
                break;

              case JSOP_OBJECT:
                obj = jp->script->getObject(GET_UINT32_INDEX(pc));
                str = js_ValueToSource(cx, ObjectValue(*obj));
                if (!str)
                    return NULL;
                goto sprint_string;

              case JSOP_REGEXP:
                obj = jp->script->getRegExp(GET_UINT32_INDEX(pc));
                str = obj->asRegExp().toString(cx);
                if (!str)
                    return NULL;
                goto sprint_string;

              case JSOP_TABLESWITCH:
              {
                ptrdiff_t off, off2;
                int32_t j, n, low, high;
                TableEntry *table, *tmp;

                sn = js_GetSrcNote(jp->script, pc);
                LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
                len = js_GetSrcNoteOffset(sn, 0);
                off = GET_JUMP_OFFSET(pc);
                pc2 = pc + JUMP_OFFSET_LEN;
                low = GET_JUMP_OFFSET(pc2);
                pc2 += JUMP_OFFSET_LEN;
                high = GET_JUMP_OFFSET(pc2);
                pc2 += JUMP_OFFSET_LEN;

                n = high - low + 1;
                if (n == 0) {
                    table = NULL;
                    j = 0;
                    ok = true;
                } else {
                    table = (TableEntry *)
                            cx->malloc_((size_t)n * sizeof *table);
                    if (!table)
                        return NULL;
                    for (i = j = 0; i < n; i++) {
                        table[j].label = NULL;
                        off2 = GET_JUMP_OFFSET(pc2);
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
                        pc2 += JUMP_OFFSET_LEN;
                    }
                    tmp = (TableEntry *)
                          cx->malloc_((size_t)j * sizeof *table);
                    if (tmp) {
                        MergeSort(table, size_t(j), tmp, CompareTableEntries);
                        Foreground::free_(tmp);
                        ok = true;
                    } else {
                        ok = false;
                    }
                }

                if (ok)
                    ok = DecompileSwitch(ss, table, (unsigned)j, pc, len, off, false);
                cx->free_(table);
                if (!ok)
                    return NULL;
                todo = -2;
                break;
              }

              case JSOP_LOOKUPSWITCH:
              {
                ptrdiff_t off, off2;
                jsatomid npairs, k;
                TableEntry *table;

                sn = js_GetSrcNote(jp->script, pc);
                LOCAL_ASSERT(sn && SN_TYPE(sn) == SRC_SWITCH);
                len = js_GetSrcNoteOffset(sn, 0);
                off = GET_JUMP_OFFSET(pc);
                pc2 = pc + JUMP_OFFSET_LEN;
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
                    uint32_t constIndex = GET_UINT32_INDEX(pc2);
                    pc2 += UINT32_INDEX_LEN;
                    off2 = GET_JUMP_OFFSET(pc2);
                    pc2 += JUMP_OFFSET_LEN;
                    table[k].key = jp->script->getConst(constIndex);
                    table[k].offset = off2;
                }

                ok = DecompileSwitch(ss, table, (unsigned)npairs, pc, len, off,
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
                int ncases;
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
                    LOCAL_ASSERT(*pc2 == JSOP_CASE || *pc2 == JSOP_DEFAULT);
                    if (*pc2 == JSOP_DEFAULT) {
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
                    LOCAL_ASSERT(*pc2 == JSOP_CASE || *pc2 == JSOP_DEFAULT);
                    caseOff = pc2 - pc;
                    table[i].key = INT_TO_JSVAL((int32_t) caseOff);
                    table[i].offset = caseOff + GET_JUMP_OFFSET(pc2);
                    if (*pc2 == JSOP_CASE) {
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
                off += GET_JUMP_OFFSET(pc2);

                ok = DecompileSwitch(ss, table, (unsigned)ncases, pc, len, off,
                                     JS_TRUE);
                cx->free_(table);
                if (!ok)
                    return NULL;
                todo = -2;
                break;
              }

              case JSOP_CASE:
              {
                lval = PopStr(ss, op, &lvalpc);
                if (!lval)
                    return NULL;
                js_printf(jp, "\tcase ");
                SprintOpcodePermanent(jp, lval, lvalpc);
                js_printf(jp, ":\n");
                todo = -2;
                break;
              }

              case JSOP_DEFFUN:
                fun = jp->script->getFunction(GET_UINT32_INDEX(pc));
                todo = -2;
                goto do_function;

              case JSOP_HOLE:
                todo = ss->sprinter.put("", 0);
                break;

              case JSOP_NEWINIT:
              {
                i = GET_UINT8(pc);
                LOCAL_ASSERT(i == JSProto_Array || i == JSProto_Object);

                todo = ss->sprinter.getOffset();
                if (i == JSProto_Array) {
                    ++ss->inArrayInit;
                    if (ss->sprinter.put("[") < 0)
                        return NULL;
                } else {
                    if (ss->sprinter.put("{") < 0)
                        return NULL;
                }
                break;
              }

              case JSOP_NEWARRAY:
              {
                todo = ss->sprinter.getOffset();
                ++ss->inArrayInit;
                if (ss->sprinter.put("[") < 0)
                    return NULL;
                break;
              }

              case JSOP_NEWOBJECT:
              {
                todo = ss->sprinter.getOffset();
                if (ss->sprinter.put("{") < 0)
                    return NULL;
                break;
              }

              case JSOP_ENDINIT:
              {
                JSBool inArray;

                op = JSOP_NOP;           /* turn off parens */
                rval = PopStr(ss, op, &rvalpc);
                sn = js_GetSrcNote(jp->script, pc);

                /* Skip any #n= prefix to find the opening bracket. */
                for (xval = rval; *xval != '[' && *xval != '{'; xval++)
                    continue;
                inArray = (*xval == '[');
                if (inArray)
                    --ss->inArrayInit;
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, rval, rvalpc, pc, todo);
                Sprint(&ss->sprinter, "%s%c",
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
                rval = PopStr(ss, JSOP_SETNAME, &rvalpc);

                /* Turn off all parens for xval and lval, which we control. */
                xval = PopStr(ss, JSOP_NOP);
                lval = PopStr(ss, JSOP_NOP, &lvalpc);
                sn = js_GetSrcNote(jp->script, pc);

                if (sn && SN_TYPE(sn) == SRC_INITPROP) {
                    atom = NULL;
                    goto do_initprop;
                }
                maybeComma = isFirst ? "" : ", ";
                todo = Sprint(&ss->sprinter, "%s%s", lval, maybeComma);
                SprintOpcode(ss, rval, rvalpc, pc, todo);
                break;

              case JSOP_INITPROP:
              case JSOP_INITMETHOD:
                LOAD_ATOM(0);
                xval = QuoteString(&ss->sprinter, atom, jschar(IsIdentifier(atom) ? 0 : '\''));
                if (!xval)
                    return NULL;
                isFirst = IsInitializerOp(ss->opcodes[ss->top - 2]);
                rval = PopStrDupe(ss, op, &rvalpc);
                lval = PopStr(ss, op, &lvalpc);
                /* fall through */

              do_initprop:
                todo = ss->sprinter.getOffset();
                SprintOpcode(ss, lval, lvalpc, pc, todo);
                maybeComma = isFirst ? "" : ", ";
                if (lastop == JSOP_GETTER || lastop == JSOP_SETTER) {
                    const char *end = rval + strlen(rval);

                    if (*rval == '(')
                        ++rval, --end;
                    LOCAL_ASSERT(strncmp(rval, js_function_str, 8) == 0);
                    LOCAL_ASSERT(rval[8] == ' ');
                    rval += 8 + 1;
                    LOCAL_ASSERT(*end ? *end == ')' : end[-1] == '}');
                    Sprint(&ss->sprinter, "%s%s %s%s%.*s",
                           maybeComma,
                           (lastop == JSOP_GETTER)
                           ? js_get_str : js_set_str,
                           xval,
                           (rval[0] != '(') ? " " : "",
                           end - rval, rval);
                } else {
                    Sprint(&ss->sprinter, "%s%s: ", maybeComma, xval);
                    SprintOpcode(ss, rval, rvalpc, pc, todo);
                }
                break;
              }

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
                    todo = ss->sprinter.put("@*", 2);
                } else {
                    todo = ss->sprinter.put("*", 1);
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
                ss->sprinter.setOffset(rval);
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
                rval = PopStrDupe(ss, op, &rvalpc);
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
                todo = ss->sprinter.put("<![CDATA[", 9);
                if (!QuoteString(&ss->sprinter, atom, DONT_ESCAPE))
                    return NULL;
                ss->sprinter.put("]]>", 3);
                break;

              case JSOP_XMLCOMMENT:
                LOAD_ATOM(0);
                todo = ss->sprinter.put("<!--", 4);
                if (!QuoteString(&ss->sprinter, atom, DONT_ESCAPE))
                    return NULL;
                ss->sprinter.put("-->", 3);
                break;

              case JSOP_XMLPI:
                LOAD_ATOM(0);
                rval = JS_strdup(cx, POP_STR());
                if (!rval)
                    return NULL;
                todo = ss->sprinter.put("<?", 2);
                ok = QuoteString(&ss->sprinter, atom, 0) &&
                     (*rval == '\0' ||
                      (ss->sprinter.put(" ", 1) >= 0 &&
                       ss->sprinter.put(rval)));
                cx->free_((char *)rval);
                if (!ok)
                    return NULL;
                ss->sprinter.put("?>", 2);
                break;

              case JSOP_GETFUNNS:
                todo = ss->sprinter.put(js_function_str, 8);
                break;
#endif /* JS_HAS_XML_SUPPORT */

              default:
                todo = -2;
                break;
            }
        }

        if (cx->isExceptionPending()) {
            /* OOMs while printing to a string do not immediately return. */
            return NULL;
        }

        if (todo < 0) {
            /* -2 means "don't push", -1 means reported error. */
            JS_ASSERT(todo == -2);
            if (todo == -1)
                return NULL;
        } else {
            if (!UpdateDecompiledText(ss, pushpc, todo))
                return NULL;
            if (!PushOff(ss, todo, saveop, pushpc))
                return NULL;
            if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
                CopyDecompiledTextForDecomposedOp(jp, pc);
        }

        if (op == JSOP_CALLXMLNAME) {
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
#undef TOP_STR
#undef POP_STR
#undef POP_STR_PREC
#undef LOCAL_ASSERT
#undef GET_QUOTE_AND_FMT
#undef GET_ATOM_QUOTE_AND_FMT

    return pc;
}

static JSBool
DecompileCode(JSPrinter *jp, JSScript *script, jsbytecode *pc, unsigned len,
              unsigned pcdepth)
{
    JSContext *cx = jp->sprinter.context;

    unsigned depth = StackDepth(script);
    JS_ASSERT(pcdepth <= depth);

    /* Initialize a sprinter for use with the offset stack. */
    LifoAllocScope las(&cx->tempLifoAlloc());
    SprintStack ss(cx);
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
        for (unsigned i = 0; i < pcdepth; i++) {
            ss.offsets[i] = -2 - (ptrdiff_t)i;
            ss.opcodes[i] = *jp->pcstack[i];
        }
    }

    /* Call recursive subroutine to do the hard work. */
    JSScript *oldscript = jp->script;
    jp->script = script;
    bool ok = Decompile(&ss, pc, len) != NULL;
    jp->script = oldscript;

    /* If the given code didn't empty the stack, do it now. */
    if (ok && ss.top) {
        const char *last;
        do {
            last = ss.sprinter.stringAt(PopOff(&ss, JSOP_POP));
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
                     unsigned indent, JSBool pretty, JSBool grouped, JSBool strict,
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
    JSFunction *fun = jp->fun;
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
        SprintStack ss(jp->sprinter.context);
#endif

        /* Print the parameters. */
        jsbytecode *pc = script->main();
        jsbytecode *endpc = pc + script->length;
        JSBool ok = JS_TRUE;

#if JS_HAS_DESTRUCTURING
        ss.printer = NULL;
        jp->script = script;
#endif

        for (unsigned i = 0; i < fun->nargs; i++) {
            if (i > 0)
                js_puts(jp, ", ");

            JSAtom *param = GetArgOrVarAtom(jp, i);

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
                todo = jp->sprinter.put(lval);
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
js_DecompileValueGenerator(JSContext *cx, int spindex, jsval v,
                           JSString *fallback)
{
    StackFrame *fp;
    JSScript *script;
    jsbytecode *pc;

    JS_ASSERT(spindex < 0 ||
              spindex == JSDVG_IGNORE_STACK ||
              spindex == JSDVG_SEARCH_STACK);

    if (!cx->hasfp() || !cx->fp()->isScriptFrame())
        goto do_fallback;

    fp = js_GetTopStackFrame(cx, FRAME_EXPAND_ALL);
    script = fp->script();
    pc = cx->regs().pc;
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
        jsbytecode *lastDecomposedPC = NULL;
        int pcdepth = ReconstructPCStack(cx, script, pc, pcstack, &lastDecomposedPC);
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
             *
             * If we are in the middle of a decomposed opcode, use the outer
             * 'fat' opcode itself. Any source notes for the operation which
             * are needed during decompilation will be associated with the
             * outer opcode.
             */
            if (sp < stackBase + pcdepth) {
                pc = pcstack[sp - stackBase];
                if (lastDecomposedPC) {
                    size_t len = GetDecomposeLength(lastDecomposedPC,
                                                    js_CodeSpec[*lastDecomposedPC].length);
                    if (unsigned(pc - lastDecomposedPC) < len)
                        pc = lastDecomposedPC;
                }
            }
        }

      release_pcstack:
        cx->free_(pcstack);
        if (pcdepth < 0)
            goto do_fallback;
    }

    {
        char *name = DecompileExpression(cx, script, fp->maybeFun(), pc);
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
    JS_ASSERT(script->code <= pc && pc < script->code + script->length);

    JSOp op = (JSOp) *pc;

    /* None of these stack-writing ops generates novel values. */
    JS_ASSERT(op != JSOP_CASE && op != JSOP_DUP && op != JSOP_DUP2);

    /*
     * |this| could convert to a very long object initialiser, so cite it by
     * its keyword name instead.
     */
    if (op == JSOP_THIS)
        return JS_strdup(cx, js_this_str);

    /*
     * JSOP_BINDNAME is special: it generates a value, the base object of a
     * reference.  But if it is the generating op for a diagnostic produced by
     * js_DecompileValueGenerator, the name being bound is irrelevant.  Just
     * fall back to the base object.
     */
    if (op == JSOP_BINDNAME)
        return FAILED_EXPRESSION_DECOMPILER;

    /* NAME ops are self-contained, others require left or right context. */
    const JSCodeSpec *cs = &js_CodeSpec[op];
    jsbytecode *begin = pc;
    jsbytecode *end = pc + cs->length;
    switch (JOF_MODE(cs->format)) {
      case JOF_PROP:
      case JOF_ELEM:
      case JOF_XMLNAME:
      case 0: {
        jssrcnote *sn = js_GetSrcNote(script, pc);
        if (!sn)
            return FAILED_EXPRESSION_DECOMPILER;
        switch (SN_TYPE(sn)) {
          case SRC_PCBASE:
            begin -= js_GetSrcNoteOffset(sn, 0);
            break;
          case SRC_PCDELTA:
            end = begin + js_GetSrcNoteOffset(sn, 0);
            begin += cs->length;
            break;
          default:
            return FAILED_EXPRESSION_DECOMPILER;
        }
        break;
      }
      default:;
    }

    /*
     * Include the trailing SWAP when decompiling CALLPROP or CALLELEM ops,
     * so that the result is the entire access rather than the lvalue.
     */
    if (op == JSOP_CALLPROP || op == JSOP_CALLELEM) {
        JS_ASSERT(*end == JSOP_SWAP);
        end += JSOP_SWAP_LENGTH;
    }

    ptrdiff_t len = end - begin;
    if (len <= 0)
        return FAILED_EXPRESSION_DECOMPILER;

    struct Guard {
        jsbytecode **pcstack;
        JSPrinter *printer;
        Guard() : pcstack(NULL), printer(NULL) {}
        ~Guard() {
            if (printer)
                js_DestroyPrinter(printer);
            Foreground::free_(pcstack);
        }
    } g;

    g.pcstack = (jsbytecode **)OffTheBooks::malloc_(StackDepth(script) * sizeof *g.pcstack);
    if (!g.pcstack)
        return NULL;

    int pcdepth = ReconstructPCStack(cx, script, begin, g.pcstack, NULL);
    if (pcdepth < 0)
         return FAILED_EXPRESSION_DECOMPILER;

    g.printer = js_NewPrinter(cx, "js_DecompileValueGenerator", fun, 0, false, false, false);
    if (!g.printer)
        return NULL;

    g.printer->dvgfence = end;
    g.printer->pcstack = g.pcstack;
    if (!DecompileCode(g.printer, script, begin, (unsigned) len, (unsigned) pcdepth))
        return NULL;

    return JS_strdup(cx, g.printer->sprinter.string());
}

unsigned
js_ReconstructStackDepth(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return ReconstructPCStack(cx, script, pc, NULL, NULL);
}

#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, -1);

static int
SimulateOp(JSContext *cx, JSScript *script, JSOp op, const JSCodeSpec *cs,
           jsbytecode *pc, jsbytecode **pcstack, unsigned &pcdepth)
{
    unsigned nuses = StackUses(script, pc);
    unsigned ndefs = StackDefs(script, pc);
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
            for (unsigned i = 0; i != ndefs; ++i)
                pcstack[pcdepth + i] = pc;
        }
        break;

      case JSOP_CASE:
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

static int
ReconstructPCStack(JSContext *cx, JSScript *script, jsbytecode *target,
                   jsbytecode **pcstack, jsbytecode **lastDecomposedPC)
{
    /*
     * Walk forward from script->main and compute the stack depth and stack of
     * operand-generating opcode PCs in pcstack.
     *
     * FIXME: Code to compute oplen copied from js_Disassemble1 and reduced.
     * FIXME: Optimize to use last empty-stack sequence point.
     */

    LOCAL_ASSERT(script->code <= target && target < script->code + script->length);
    jsbytecode *pc = script->code;
    unsigned pcdepth = 0;
    ptrdiff_t oplen;
    for (; pc < target; pc += oplen) {
        JSOp op = JSOp(*pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        oplen = cs->length;
        if (oplen < 0)
            oplen = js_GetVariableBytecodeLength(pc);

        if (cs->format & JOF_DECOMPOSE) {
            if (lastDecomposedPC)
                *lastDecomposedPC = pc;
            continue;
        }

        /*
         * A (C ? T : E) expression requires skipping either T (if target is in
         * E) or both T and E (if target is after the whole expression) before
         * adjusting pcdepth based on the JSOP_IFEQ at pc that tests condition
         * C. We know that the stack depth can't change from what it was with
         * C on top of stack.
         */
        jssrcnote *sn = js_GetSrcNote(script, pc);
        if (sn && SN_TYPE(sn) == SRC_COND) {
            ptrdiff_t jmpoff = js_GetSrcNoteOffset(sn, 0);
            if (pc + jmpoff < target) {
                pc += jmpoff;
                op = JSOp(*pc);
                JS_ASSERT(op == JSOP_GOTO);
                cs = &js_CodeSpec[op];
                oplen = cs->length;
                JS_ASSERT(oplen > 0);
                ptrdiff_t jmplen = GET_JUMP_OFFSET(pc);
                if (pc + jmplen < target) {
                    oplen = (unsigned) jmplen;
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
     *
     * If we see any of these sequences, the result is only tested for nullness:
     * - call / ifeq
     * - call / not / ifeq
     */

    if (*pc != JSOP_CALL)
        return true;

    pc += JSOP_CALL_LENGTH;

    if (*pc == JSOP_POP)
        return false;

    if (*pc == JSOP_NOT)
        pc += JSOP_NOT_LENGTH;

    return (*pc != JSOP_IFEQ);
}

extern bool
IsValidBytecodeOffset(JSContext *cx, JSScript *script, size_t offset)
{
    // This could be faster (by following jump instructions if the target is <= offset).
    for (BytecodeRange r(script); !r.empty(); r.popFront()) {
        size_t here = r.frontOffset();
        if (here >= offset)
            return here == offset;
    }
    return false;
}

JS_FRIEND_API(size_t)
GetPCCountScriptCount(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    if (!rt->scriptPCCounters)
        return 0;

    return rt->scriptPCCounters->length();
}

enum MaybeComma {NO_COMMA, COMMA};

static void
AppendJSONProperty(StringBuffer &buf, const char *name, MaybeComma comma = COMMA)
{
    if (comma)
        buf.append(',');

    buf.append('\"');
    buf.appendInflated(name, strlen(name));
    buf.appendInflated("\":", 2);
}

static void
AppendArrayJSONProperties(JSContext *cx, StringBuffer &buf,
                          double *values, const char **names, unsigned count, MaybeComma &comma)
{
    for (unsigned i = 0; i < count; i++) {
        if (values[i]) {
            AppendJSONProperty(buf, names[i], comma);
            comma = COMMA;
            NumberValueToStringBuffer(cx, DoubleValue(values[i]), buf);
        }
    }
}

JS_FRIEND_API(JSString *)
GetPCCountScriptSummary(JSContext *cx, size_t index)
{
    JSRuntime *rt = cx->runtime;

    if (!rt->scriptPCCounters || index >= rt->scriptPCCounters->length()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BUFFER_TOO_SMALL);
        return NULL;
    }

    ScriptOpcodeCountsPair info = (*rt->scriptPCCounters)[index];
    JSScript *script = info.script;

    /*
     * OOM on buffer appends here will not be caught immediately, but since
     * StringBuffer uses a ContextAllocPolicy will trigger an exception on the
     * context if they occur, which we'll catch before returning.
     */
    StringBuffer buf(cx);

    buf.append('{');

    AppendJSONProperty(buf, "file", NO_COMMA);
    JSString *str = JS_NewStringCopyZ(cx, script->filename);
    if (!str || !(str = JS_ValueToSource(cx, StringValue(str))))
        return NULL;
    buf.append(str);

    AppendJSONProperty(buf, "line");
    NumberValueToStringBuffer(cx, Int32Value(script->lineno), buf);

    if (script->function()) {
        JSAtom *atom = script->function()->atom;
        if (atom) {
            AppendJSONProperty(buf, "name");
            if (!(str = JS_ValueToSource(cx, StringValue(atom))))
                return NULL;
            buf.append(str);
        }
    }

    double baseTotals[OpcodeCounts::BASE_COUNT] = {0.0};
    double accessTotals[OpcodeCounts::ACCESS_COUNT - OpcodeCounts::BASE_COUNT] = {0.0};
    double elementTotals[OpcodeCounts::ELEM_COUNT - OpcodeCounts::ACCESS_COUNT] = {0.0};
    double propertyTotals[OpcodeCounts::PROP_COUNT - OpcodeCounts::ACCESS_COUNT] = {0.0};
    double arithTotals[OpcodeCounts::ARITH_COUNT - OpcodeCounts::BASE_COUNT] = {0.0};

    for (unsigned i = 0; i < script->length; i++) {
        OpcodeCounts &counts = info.getCounts(script->code + i);
        if (!counts)
            continue;

        JSOp op = (JSOp)script->code[i];
        unsigned numCounts = OpcodeCounts::numCounts(op);

        for (unsigned j = 0; j < numCounts; j++) {
            double value = counts.get(j);
            if (j < OpcodeCounts::BASE_COUNT) {
                baseTotals[j] += value;
            } else if (OpcodeCounts::accessOp(op)) {
                if (j < OpcodeCounts::ACCESS_COUNT)
                    accessTotals[j - OpcodeCounts::BASE_COUNT] += value;
                else if (OpcodeCounts::elementOp(op))
                    elementTotals[j - OpcodeCounts::ACCESS_COUNT] += value;
                else if (OpcodeCounts::propertyOp(op))
                    propertyTotals[j - OpcodeCounts::ACCESS_COUNT] += value;
                else
                    JS_NOT_REACHED("Bad opcode");
            } else if (OpcodeCounts::arithOp(op)) {
                arithTotals[j - OpcodeCounts::BASE_COUNT] += value;
            } else {
                JS_NOT_REACHED("Bad opcode");
            }
        }
    }

    AppendJSONProperty(buf, "totals");
    buf.append('{');

    MaybeComma comma = NO_COMMA;

    AppendArrayJSONProperties(cx, buf, baseTotals, countBaseNames,
                              JS_ARRAY_LENGTH(baseTotals), comma);
    AppendArrayJSONProperties(cx, buf, accessTotals, countAccessNames,
                              JS_ARRAY_LENGTH(accessTotals), comma);
    AppendArrayJSONProperties(cx, buf, elementTotals, countElementNames,
                              JS_ARRAY_LENGTH(elementTotals), comma);
    AppendArrayJSONProperties(cx, buf, propertyTotals, countPropertyNames,
                              JS_ARRAY_LENGTH(propertyTotals), comma);
    AppendArrayJSONProperties(cx, buf, arithTotals, countArithNames,
                              JS_ARRAY_LENGTH(arithTotals), comma);

    buf.append('}');
    buf.append('}');

    if (cx->isExceptionPending())
        return NULL;

    return buf.finishString();
}

struct AutoDestroyPrinter
{
    JSPrinter *jp;
    AutoDestroyPrinter(JSPrinter *jp) : jp(jp) {}
    ~AutoDestroyPrinter() { js_DestroyPrinter(jp); }
};

static bool
GetPCCountJSON(JSContext *cx, const ScriptOpcodeCountsPair &info, StringBuffer &buf)
{
    JSScript *script = info.script;

    buf.append('{');
    AppendJSONProperty(buf, "text", NO_COMMA);

    Vector<DecompiledOpcode> decompiledOpcodes(cx);
    if (!decompiledOpcodes.reserve(script->length))
        return false;

    for (unsigned i = 0; i < script->length; i++)
        decompiledOpcodes.infallibleAppend(DecompiledOpcode());

    JSFunction *fun = script->function();
    JSPrinter *jp = js_NewPrinter(cx, "", fun, 4, true, false, false);
    if (!jp)
        return false;
    AutoDestroyPrinter destroy(jp);

    jp->decompiledOpcodes = &decompiledOpcodes;

    if (fun) {
        if (!js_DecompileFunction(jp))
            return false;
    } else {
        if (!js_DecompileScript(jp, script))
            return false;
    }
    JSString *str = js_GetPrinterOutput(jp);
    if (!str || !(str = JS_ValueToSource(cx, StringValue(str))))
        return false;

    buf.append(str);

    AppendJSONProperty(buf, "opcodes");
    buf.append('[');
    bool comma = false;

    SrcNoteLineScanner scanner(script->notes(), script->lineno);

    for (jsbytecode *pc = script->code;
         pc < script->code + script->length;
         pc += GetBytecodeLength(pc))
    {
        size_t offset = pc - script->code;

        JSOp op = (JSOp) *pc;

        if (comma)
            buf.append(',');
        comma = true;

        buf.append('{');

        AppendJSONProperty(buf, "id", NO_COMMA);
        NumberValueToStringBuffer(cx, Int32Value(pc - script->code), buf);

        scanner.advanceTo(offset);

        AppendJSONProperty(buf, "line");
        NumberValueToStringBuffer(cx, Int32Value(scanner.getLine()), buf);

        {
            const char *name = js_CodeName[op];
            AppendJSONProperty(buf, "name");
            buf.append('\"');
            buf.appendInflated(name, strlen(name));
            buf.append('\"');
        }

        DecompiledOpcode *search = &decompiledOpcodes[offset];
        size_t textBias = 0;
        while (search->parent) {
            textBias += search->parentOffset;
            if (search->parenthesized)
                textBias++;
            search = &decompiledOpcodes[search->parent - script->code];
        }

        int32_t printedOffset = search->parentOffset;
        if (printedOffset != -1) {
            printedOffset += textBias;
            if (search->parenthesized)
                printedOffset++;
            AppendJSONProperty(buf, "textOffset");
            NumberValueToStringBuffer(cx, Int32Value(printedOffset), buf);
        }

        const char *text = decompiledOpcodes[offset].text;
        if (text && *text != 0) {
            AppendJSONProperty(buf, "text");
            JSString *str = JS_NewStringCopyZ(cx, text);
            if (!str || !(str = JS_ValueToSource(cx, StringValue(str))))
                return false;
            buf.append(str);
        }

        OpcodeCounts &counts = info.getCounts(pc);
        unsigned numCounts = OpcodeCounts::numCounts(op);

        AppendJSONProperty(buf, "counts");
        buf.append('{');

        MaybeComma comma = NO_COMMA;
        for (unsigned i = 0; i < numCounts; i++) {
            double value = counts.get(i);
            if (value > 0) {
                AppendJSONProperty(buf, OpcodeCounts::countName(op, i), comma);
                comma = COMMA;
                NumberValueToStringBuffer(cx, DoubleValue(value), buf);
            }
        }

        buf.append('}');
        buf.append('}');
    }

    buf.append(']');
    buf.append('}');

    return !cx->isExceptionPending();
}

JS_FRIEND_API(JSString *)
GetPCCountScriptContents(JSContext *cx, size_t index)
{
    JSRuntime *rt = cx->runtime;

    if (!rt->scriptPCCounters || index >= rt->scriptPCCounters->length()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BUFFER_TOO_SMALL);
        return NULL;
    }

    const ScriptOpcodeCountsPair &info = (*rt->scriptPCCounters)[index];
    JSScript *script = info.script;

    StringBuffer buf(cx);

    if (!script->function() && !script->compileAndGo)
        return buf.finishString();

    {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, script->function() ? (JSObject *) script->function() : script->global()))
            return NULL;

        if (!GetPCCountJSON(cx, info, buf))
            return NULL;
    }

    return buf.finishString();
}

} // namespace js
