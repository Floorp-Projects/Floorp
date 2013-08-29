/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS bytecode descriptors, disassemblers, and (expression) decompilers.
 */

#include "jsopcode.h"

#include "mozilla/Util.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "jstypes.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsanalyze.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsstr.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/SourceNotes.h"
#include "js/CharacterEncoding.h"
#include "vm/Shape.h"
#include "vm/StringBuffer.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jscompartmentinlines.h"
#include "jsopcodeinlines.h"

#include "jsautooplen.h"

#include "vm/RegExpObject-inl.h"
#include "vm/ScopeObject-inl.h"

using namespace js;
using namespace js::gc;

using js::frontend::IsIdentifier;
using mozilla::ArrayLength;

/*
 * Index limit must stay within 32 bits.
 */
JS_STATIC_ASSERT(sizeof(uint32_t) * JS_BITS_PER_BYTE >= INDEX_LIMIT_LOG2 + 1);

/* Verify JSOP_XXX_LENGTH constant definitions. */
#define OPDEF(op,val,name,token,length,nuses,ndefs,format)               \
    JS_STATIC_ASSERT(op##_LENGTH == length);
#include "jsopcode.tbl"
#undef OPDEF

static const char js_incop_strs[][3] = {"++", "--"};
static const char js_for_each_str[]  = "for each";

const JSCodeSpec js_CodeSpec[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,format) \
    {length,nuses,ndefs,format},
#include "jsopcode.tbl"
#undef OPDEF
};

const unsigned js_NumCodeSpecs = JS_ARRAY_LENGTH(js_CodeSpec);

/*
 * Each element of the array is either a source literal associated with JS
 * bytecode or null.
 */
static const char * const CodeToken[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,format) \
    token,
#include "jsopcode.tbl"
#undef OPDEF
};

/*
 * Array of JS bytecode names used by PC count JSON, DEBUG-only js_Disassemble
 * and JIT debug spew.
 */
const char * const js_CodeName[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,format) \
    name,
#include "jsopcode.tbl"
#undef OPDEF
};

/************************************************************************/

#define COUNTS_LEN 16

size_t
js_GetVariableBytecodeLength(jsbytecode *pc)
{
    JSOp op = JSOp(*pc);
    JS_ASSERT(js_CodeSpec[op].length == -1);
    switch (op) {
      case JSOP_TABLESWITCH: {
        /* Structure: default-jump case-low case-high case1-jump ... */
        pc += JUMP_OFFSET_LEN;
        int32_t low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        int32_t high = GET_JUMP_OFFSET(pc);
        unsigned ncases = unsigned(high - low + 1);
        return 1 + 3 * JUMP_OFFSET_LEN + ncases * JUMP_OFFSET_LEN;
      }
      default:
        JS_NOT_REACHED("Unexpected op");
        return 0;
    }
}

static uint32_t
NumBlockSlots(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(*pc == JSOP_ENTERBLOCK || *pc == JSOP_ENTERLET0 || *pc == JSOP_ENTERLET1);
    JS_STATIC_ASSERT(JSOP_ENTERBLOCK_LENGTH == JSOP_ENTERLET0_LENGTH);
    JS_STATIC_ASSERT(JSOP_ENTERBLOCK_LENGTH == JSOP_ENTERLET1_LENGTH);

    return script->getObject(GET_UINT32_INDEX(pc))->as<StaticBlockObject>().slotCount();
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

static const char * const countBaseNames[] = {
    "interp",
    "mjit",
    "mjit_calls",
    "mjit_code",
    "mjit_pics"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) == PCCounts::BASE_LIMIT);

static const char * const countAccessNames[] = {
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
                 JS_ARRAY_LENGTH(countAccessNames) == PCCounts::ACCESS_LIMIT);

static const char * const countElementNames[] = {
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
                 JS_ARRAY_LENGTH(countElementNames) == PCCounts::ELEM_LIMIT);

static const char * const countPropertyNames[] = {
    "prop_static",
    "prop_definite",
    "prop_other"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) +
                 JS_ARRAY_LENGTH(countAccessNames) +
                 JS_ARRAY_LENGTH(countPropertyNames) == PCCounts::PROP_LIMIT);

static const char * const countArithNames[] = {
    "arith_int",
    "arith_double",
    "arith_other",
    "arith_unknown",
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(countBaseNames) +
                 JS_ARRAY_LENGTH(countArithNames) == PCCounts::ARITH_LIMIT);

/* static */ const char *
PCCounts::countName(JSOp op, size_t which)
{
    JS_ASSERT(which < numCounts(op));

    if (which < BASE_LIMIT)
        return countBaseNames[which];

    if (accessOp(op)) {
        if (which < ACCESS_LIMIT)
            return countAccessNames[which - BASE_LIMIT];
        if (elementOp(op))
            return countElementNames[which - ACCESS_LIMIT];
        if (propertyOp(op))
            return countPropertyNames[which - ACCESS_LIMIT];
        JS_NOT_REACHED("bad op");
        return NULL;
    }

    if (arithOp(op))
        return countArithNames[which - BASE_LIMIT];

    JS_NOT_REACHED("bad op");
    return NULL;
}

#ifdef DEBUG

#ifdef JS_ION
void
js::DumpIonScriptCounts(Sprinter *sp, jit::IonScriptCounts *ionCounts)
{
    Sprint(sp, "IonScript [%lu blocks]:\n", ionCounts->numBlocks());
    for (size_t i = 0; i < ionCounts->numBlocks(); i++) {
        const jit::IonBlockCounts &block = ionCounts->block(i);
        if (block.hitCount() < 10)
            continue;
        Sprint(sp, "BB #%lu [%05u]", block.id(), block.offset());
        for (size_t j = 0; j < block.numSuccessors(); j++)
            Sprint(sp, " -> #%lu", block.successor(j));
        Sprint(sp, " :: %llu hits %u instruction bytes %u spill bytes\n",
               block.hitCount(), block.instructionBytes(), block.spillBytes());
        Sprint(sp, "%s\n", block.code());
    }
}
#endif

void
js_DumpPCCounts(JSContext *cx, HandleScript script, js::Sprinter *sp)
{
    JS_ASSERT(script->hasScriptCounts);

    jsbytecode *pc = script->code;
    while (pc < script->code + script->length) {
        JSOp op = JSOp(*pc);

        int len = js_CodeSpec[op].length;
        jsbytecode *next = (len != -1) ? pc + len : pc + js_GetVariableBytecodeLength(pc);

        if (!js_Disassemble1(cx, script, pc, pc - script->code, true, sp))
            return;

        size_t total = PCCounts::numCounts(op);
        double *raw = script->getPCCounts(pc).rawCounts();

        Sprint(sp, "                  {");
        bool printed = false;
        for (size_t i = 0; i < total; i++) {
            double val = raw[i];
            if (val) {
                if (printed)
                    Sprint(sp, ", ");
                Sprint(sp, "\"%s\": %.0f", PCCounts::countName(op, i), val);
                printed = true;
            }
        }
        Sprint(sp, "}\n");

        pc = next;
    }

#ifdef JS_ION
    jit::IonScriptCounts *ionCounts = script->getIonCounts();

    while (ionCounts) {
        DumpIonScriptCounts(sp, ionCounts);
        ionCounts = ionCounts->previous();
    }
#endif
}

/*
 * If pc != NULL, include a prefix indicating whether the PC is at the current line.
 * If showAll is true, include the source note type and the entry stack depth.
 */
JS_FRIEND_API(JSBool)
js_DisassembleAtPC(JSContext *cx, JSScript *scriptArg, JSBool lines,
                   jsbytecode *pc, bool showAll, Sprinter *sp)
{
    RootedScript script(cx, scriptArg);

    jsbytecode *next, *end;
    unsigned len;

    if (showAll)
        Sprint(sp, "%s:%u\n", script->filename(), script->lineno);

    if (pc != NULL)
        sp->put("    ");
    if (showAll)
        sp->put("sn stack ");
    sp->put("loc   ");
    if (lines)
        sp->put("line");
    sp->put("  op\n");

    if (pc != NULL)
        sp->put("    ");
    if (showAll)
        sp->put("-- ----- ");
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
        if (showAll) {
            jssrcnote *sn = js_GetSrcNote(cx, script, next);
            if (sn) {
                JS_ASSERT(!SN_IS_TERMINATOR(sn));
                jssrcnote *next = SN_NEXT(sn);
                while (!SN_IS_TERMINATOR(next) && SN_DELTA(next) == 0) {
                    Sprint(sp, "%02u\n    ", SN_TYPE(sn));
                    sn = next;
                    next = SN_NEXT(sn);
                }
                Sprint(sp, "%02u ", SN_TYPE(sn));
            }
            else
                sp->put("   ");
            if (script->hasAnalysis() && script->analysis()->maybeCode(next))
                Sprint(sp, "%05u ", script->analysis()->getCode(next).stackDepth);
            else
                sp->put("      ");
        }
        len = js_Disassemble1(cx, script, next, next - script->code, lines, sp);
        if (!len)
            return JS_FALSE;
        next += len;
    }
    return JS_TRUE;
}

JSBool
js_Disassemble(JSContext *cx, HandleScript script, JSBool lines, Sprinter *sp)
{
    return js_DisassembleAtPC(cx, script, lines, NULL, false, sp);
}

JS_FRIEND_API(JSBool)
js_DumpPC(JSContext *cx)
{
    js::gc::AutoSuppressGC suppressGC(cx);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return JS_FALSE;
    ScriptFrameIter iter(cx);
    RootedScript script(cx, iter.script());
    JSBool ok = js_DisassembleAtPC(cx, script, true, iter.pc(), false, &sprinter);
    fprintf(stdout, "%s", sprinter.string());
    return ok;
}

JS_FRIEND_API(JSBool)
js_DumpScript(JSContext *cx, JSScript *scriptArg)
{
    js::gc::AutoSuppressGC suppressGC(cx);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return JS_FALSE;
    RootedScript script(cx, scriptArg);
    JSBool ok = js_Disassemble(cx, script, true, &sprinter);
    fprintf(stdout, "%s", sprinter.string());
    return ok;
}

/*
 * Useful to debug ReconstructPCStack.
 */
JS_FRIEND_API(JSBool)
js_DumpScriptDepth(JSContext *cx, JSScript *scriptArg, jsbytecode *pc)
{
    js::gc::AutoSuppressGC suppressGC(cx);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return JS_FALSE;
    RootedScript script(cx, scriptArg);
    JSBool ok = js_DisassembleAtPC(cx, script, true, pc, true, &sprinter);
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

    if (cx->runtime()->isHeapBusy() || cx->runtime()->noGCOrAllocationCheck) {
        char *source = JS_sprintf_append(NULL, "<value>");
        if (!source)
            return false;
        bytes->initBytes(source);
        return true;
    }

    if (!JSVAL_IS_PRIMITIVE(v)) {
        JSObject *obj = JSVAL_TO_OBJECT(v);
        if (obj->is<BlockObject>()) {
            char *source = JS_sprintf_append(NULL, "depth %d {",
                                             obj->as<BlockObject>().stackDepth());
            if (!source)
                return false;

            Shape::Range<CanGC> r(cx, obj->lastProperty());

            while (!r.empty()) {
                Rooted<Shape*> shape(cx, &r.front());
                JSAtom *atom = JSID_IS_INT(shape->propid())
                               ? cx->names().empty
                               : JSID_TO_ATOM(shape->propid());

                JSAutoByteString bytes;
                if (!js_AtomToPrintableString(cx, atom, &bytes))
                    return false;

                r.popFront();
                source = JS_sprintf_append(source, "%s: %d%s",
                                           bytes.ptr(), shape->shortid(),
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

        if (obj->is<JSFunction>()) {
            JSString *str = JS_DecompileFunction(cx, &obj->as<JSFunction>(), JS_DONT_PRETTY_PRINT);
            if (!str)
                return false;
            return bytes->encodeLatin1(cx, str);
        }

        if (obj->is<RegExpObject>()) {
            JSString *source = obj->as<RegExpObject>().toString(cx);
            if (!source)
                return false;
            JS::Anchor<JSString *> anchor(source);
            return bytes->encodeLatin1(cx, source);
        }
    }

    return !!js_ValueToPrintable(cx, v, bytes, true);
}

unsigned
js_Disassemble1(JSContext *cx, HandleScript script, jsbytecode *pc,
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
              TryNoteArray *trynotes = script->trynotes();
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

      case JOF_SCOPECOORD: {
        Value v = StringValue(ScopeCoordinateName(cx, script, pc));
        JSAutoByteString bytes;
        if (!ToDisassemblySource(cx, v, &bytes))
            return 0;
        ScopeCoordinate sc(pc);
        Sprint(sp, " %s (hops = %u, slot = %u)", bytes.ptr(), sc.hops, sc.slot);
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
        if (cx->compartment()->activeAnalysis) {
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
        JS_ASSERT(op == JSOP_UINT24 || op == JSOP_NEWARRAY || op == JSOP_INITELEM_ARRAY);
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
    base(NULL), size(0), offset(0), reportedOOM(false)
{ }

Sprinter::~Sprinter()
{
#ifdef DEBUG
    if (initialized)
        checkInvariants();
#endif
    js_free(base);
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
    JS_ASSERT(off < size);
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

    size_t size = length;
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

void
Sprinter::reportOutOfMemory() {
    if (reportedOOM)
        return;
    js_ReportOutOfMemory(context);
    reportedOOM = true;
}

bool
Sprinter::hadOutOfMemory() const {
    return reportedOOM;
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
        sp->reportOutOfMemory();
        return -1;
    }
    offset = sp->put(bp);
    js_free(bp);
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

static int ReconstructPCStack(JSContext*, JSScript*, jsbytecode*, jsbytecode**);

static unsigned
StackDepth(JSScript *script)
{
    return script->nslots - script->nfixed;
}

static JSObject *
GetBlockChainAtPC(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    jsbytecode *start = script->main();

    JS_ASSERT(pc >= start && pc < script->code + script->length);

    JSObject *blockChain = NULL;
    for (jsbytecode *p = start; p < pc; p += GetBytecodeLength(p)) {
        JSOp op = JSOp(*p);

        switch (op) {
          case JSOP_ENTERBLOCK:
          case JSOP_ENTERLET0:
          case JSOP_ENTERLET1: {
            JSObject *child = script->getObject(p);
            JS_ASSERT_IF(blockChain, child->as<BlockObject>().stackDepth() >=
                                     blockChain->as<BlockObject>().stackDepth());
            blockChain = child;
            break;
          }
          case JSOP_LEAVEBLOCK:
          case JSOP_LEAVEBLOCKEXPR:
          case JSOP_LEAVEFORLETIN: {
            // Some LEAVEBLOCK instructions are due to early exits via
            // return/break/etc. from block-scoped loops and functions.  We
            // should ignore these instructions, since they don't really signal
            // the end of the block.
            jssrcnote *sn = js_GetSrcNote(cx, script, p);
            if (!(sn && SN_TYPE(sn) == SRC_HIDDEN)) {
                JS_ASSERT(blockChain);
                blockChain = blockChain->as<StaticBlockObject>().enclosingBlock();
                JS_ASSERT_IF(blockChain, blockChain->is<BlockObject>());
            }
            break;
          }
          default:
            break;
        }
    }

    return blockChain;
}

class PCStack
{
    jsbytecode **stack;
    int depth_;

  public:
    PCStack() : stack(NULL), depth_(0) {}
    ~PCStack();
    bool init(JSContext *cx, JSScript *script, jsbytecode *pc);
    int depth() const { return depth_; }
    jsbytecode *operator[](int i) const;
};

PCStack::~PCStack()
{
    js_free(stack);
}

bool
PCStack::init(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    stack = cx->pod_malloc<jsbytecode*>(StackDepth(script));
    if (!stack)
        return false;
    depth_ = ReconstructPCStack(cx, script, pc, stack);
    JS_ASSERT(depth_ >= 0);
    return true;
}

/* Indexes the pcstack. */
jsbytecode *
PCStack::operator[](int i) const
{
    if (i < 0) {
        i += depth_;
        JS_ASSERT(i >= 0);
    }
    JS_ASSERT(i < depth_);
    return stack[i];
}

/*
 * The expression decompiler is invoked by error handling code to produce a
 * string representation of the erroring expression. As it's only a debugging
 * tool, it only supports basic expressions. For anything complicated, it simply
 * puts "(intermediate value)" into the error result.
 *
 * Here's the basic algorithm:
 *
 * 1. Find the stack location of the value whose expression we wish to
 * decompile. The error handler can explicitly pass this as an
 * argument. Otherwise, we search backwards down the stack for the offending
 * value.
 *
 * 2. Call ReconstructPCStack with the current frame's pc. This creates a stack
 * of pcs parallel to the interpreter stack; given an interpreter stack
 * location, the corresponding pc stack location contains the opcode that pushed
 * the value in the interpreter. Now, with the result of step 1, we have the
 * opcode responsible for pushing the value we want to decompile.
 *
 * 3. Pass the opcode to decompilePC. decompilePC is the main decompiler
 * routine, responsible for a string representation of the expression that
 * generated a certain stack location. decompilePC looks at one opcode and
 * returns the JS source equivalent of that opcode.
 *
 * 4. Expressions can, of course, contain subexpressions. For example, the
 * literals "4" and "5" are subexpressions of the addition operator in "4 +
 * 5". If we need to decompile a subexpression, we call decompilePC (step 2)
 * recursively on the operands' pcs. The result is a depth-first traversal of
 * the expression tree.
 *
 */
struct ExpressionDecompiler
{
    JSContext *cx;
    StackFrame *fp;
    RootedScript script;
    RootedFunction fun;
    BindingVector *localNames;
    Sprinter sprinter;

    ExpressionDecompiler(JSContext *cx, JSScript *script, JSFunction *fun)
        : cx(cx),
          script(cx, script),
          fun(cx, fun),
          localNames(NULL),
          sprinter(cx)
    {}
    ~ExpressionDecompiler();
    bool init();
    bool decompilePC(jsbytecode *pc);
    JSAtom *getVar(unsigned slot);
    JSAtom *getArg(unsigned slot);
    JSAtom *findLetVar(jsbytecode *pc, unsigned depth);
    JSAtom *loadAtom(jsbytecode *pc);
    bool quote(JSString *s, uint32_t quote);
    bool write(const char *s);
    bool write(JSString *str);
    bool getOutput(char **out);
};

bool
ExpressionDecompiler::decompilePC(jsbytecode *pc)
{
    JS_ASSERT(script->code <= pc && pc < script->code + script->length);

    PCStack pcstack;
    if (!pcstack.init(cx, script, pc))
        return false;

    JSOp op = (JSOp)*pc;

    if (const char *token = CodeToken[op]) {
        // Handle simple cases of binary and unary operators.
        switch (js_CodeSpec[op].nuses) {
          case 2: {
            jssrcnote *sn = js_GetSrcNote(cx, script, pc);
            if (!sn || SN_TYPE(sn) != SRC_ASSIGNOP)
                return write("(") &&
                       decompilePC(pcstack[-2]) &&
                       write(" ") &&
                       write(token) &&
                       write(" ") &&
                       decompilePC(pcstack[-1]) &&
                       write(")");
            break;
          }
          case 1:
            return write(token) &&
                   write("(") &&
                   decompilePC(pcstack[-1]) &&
                   write(")");
          default:
            break;
        }
    }

    switch (op) {
      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
      case JSOP_NAME:
      case JSOP_CALLNAME:
        return write(loadAtom(pc));
      case JSOP_GETARG:
      case JSOP_CALLARG: {
        unsigned slot = GET_ARGNO(pc);
        JSAtom *atom = getArg(slot);
        return write(atom);
      }
      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL: {
        unsigned i = GET_SLOTNO(pc);
        JSAtom *atom;
        if (i >= script->nfixed) {
            i -= script->nfixed;
            JS_ASSERT(i < unsigned(pcstack.depth()));
            atom = findLetVar(pc, i);
            if (!atom)
                return decompilePC(pcstack[i]); // Destructing temporary
        } else {
            atom = getVar(i);
        }
        JS_ASSERT(atom);
        return write(atom);
      }
      case JSOP_CALLALIASEDVAR:
      case JSOP_GETALIASEDVAR: {
        JSAtom *atom = ScopeCoordinateName(cx, script, pc);
        JS_ASSERT(atom);
        return write(atom);
      }
      case JSOP_LENGTH:
      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        RootedAtom prop(cx, (op == JSOP_LENGTH) ? cx->names().length : loadAtom(pc));
        if (!decompilePC(pcstack[-1]))
            return false;
        if (IsIdentifier(prop)) {
            return write(".") &&
                   quote(prop, '\0');
        }
        return write("[") &&
               quote(prop, '\'') &&
               write("]");
      }
      case JSOP_GETELEM:
      case JSOP_CALLELEM:
        return decompilePC(pcstack[-2]) &&
               write("[") &&
               decompilePC(pcstack[-1]) &&
               write("]");
      case JSOP_NULL:
        return write(js_null_str);
      case JSOP_TRUE:
        return write(js_true_str);
      case JSOP_FALSE:
        return write(js_false_str);
      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_INT8:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_INT32:
        return sprinter.printf("%d", GetBytecodeInteger(pc)) >= 0;
      case JSOP_STRING:
        return quote(loadAtom(pc), '"');
      case JSOP_UNDEFINED:
        return write(js_undefined_str);
      case JSOP_THIS:
        // |this| could convert to a very long object initialiser, so cite it by
        // its keyword name.
        return write(js_this_str);
      case JSOP_CALL:
      case JSOP_FUNCALL:
        return decompilePC(pcstack[-int32_t(GET_ARGC(pc) + 2)]) &&
               write("(...)");
      case JSOP_NEWARRAY:
        return write("[]");
      case JSOP_REGEXP:
      case JSOP_OBJECT: {
        JSObject *obj = (op == JSOP_REGEXP)
                        ? script->getRegExp(GET_UINT32_INDEX(pc))
                        : script->getObject(GET_UINT32_INDEX(pc));
        JSString *str = ValueToSource(cx, ObjectValue(*obj));
        if (!str)
            return false;
        return write(str);
      }
      default:
        break;
    }
    return write("(intermediate value)");
}

ExpressionDecompiler::~ExpressionDecompiler()
{
    js_delete<BindingVector>(localNames);
}

bool
ExpressionDecompiler::init()
{
    assertSameCompartment(cx, script);

    if (!sprinter.init())
        return false;

    localNames = cx->new_<BindingVector>(cx);
    if (!localNames)
        return false;
    RootedScript script_(cx, script);
    if (!FillBindingVector(script_, localNames))
        return false;

    return true;
}

bool
ExpressionDecompiler::write(const char *s)
{
    return sprinter.put(s) >= 0;
}

bool
ExpressionDecompiler::write(JSString *str)
{
    return sprinter.putString(str) >= 0;
}

bool
ExpressionDecompiler::quote(JSString *s, uint32_t quote)
{
    return QuoteString(&sprinter, s, quote) >= 0;
}

JSAtom *
ExpressionDecompiler::loadAtom(jsbytecode *pc)
{
    return script->getAtom(GET_UINT32_INDEX(pc));
}

JSAtom *
ExpressionDecompiler::findLetVar(jsbytecode *pc, unsigned depth)
{
    if (script->hasObjects()) {
        JSObject *chain = GetBlockChainAtPC(cx, script, pc);
        if (!chain)
            return NULL;
        JS_ASSERT(chain->is<BlockObject>());
        do {
            BlockObject &block = chain->as<BlockObject>();
            uint32_t blockDepth = block.stackDepth();
            uint32_t blockCount = block.slotCount();
            if (uint32_t(depth - blockDepth) < uint32_t(blockCount)) {
                for (Shape::Range<NoGC> r(block.lastProperty()); !r.empty(); r.popFront()) {
                    const Shape &shape = r.front();
                    if (shape.shortid() == int(depth - blockDepth))
                        return JSID_TO_ATOM(shape.propid());
                }
            }
            chain = chain->getParent();
        } while (chain && chain->is<BlockObject>());
    }
    return NULL;
}

JSAtom *
ExpressionDecompiler::getArg(unsigned slot)
{
    JS_ASSERT(fun);
    JS_ASSERT(slot < script->bindings.count());
    return (*localNames)[slot].name();
}

JSAtom *
ExpressionDecompiler::getVar(unsigned slot)
{
    JS_ASSERT(fun);
    slot += fun->nargs;
    JS_ASSERT(slot < script->bindings.count());
    return (*localNames)[slot].name();
}

bool
ExpressionDecompiler::getOutput(char **res)
{
    ptrdiff_t len = sprinter.stringEnd() - sprinter.stringAt(0);
    *res = cx->pod_malloc<char>(len + 1);
    if (!*res)
        return false;
    js_memcpy(*res, sprinter.stringAt(0), len);
    (*res)[len] = 0;
    return true;
}

static bool
FindStartPC(JSContext *cx, ScriptFrameIter &iter, int spindex, int skipStackHits, Value v,
            jsbytecode **valuepc)
{
    jsbytecode *current = *valuepc;

    if (spindex == JSDVG_IGNORE_STACK)
        return true;

    /*
     * Fall back on *valuepc as start pc if this frame is calling .apply and
     * the methodjit has "splatted" the arguments array, bumping the caller's
     * stack pointer and skewing it from what static analysis in pcstack.init
     * would compute.
     *
     * FIXME: also fall back if iter.isIon(), since the stack snapshot may be
     * for the previous pc (see bug 831120).
     */
    if (iter.isIon())
        return true;

    *valuepc = NULL;

    PCStack pcstack;
    if (!pcstack.init(cx, iter.script(), current))
        return false;

    if (spindex < 0 && spindex + pcstack.depth() < 0)
        spindex = JSDVG_SEARCH_STACK;

    if (spindex == JSDVG_SEARCH_STACK) {
        size_t index = iter.numFrameSlots();
        JS_ASSERT(index >= size_t(pcstack.depth()));

        // We search from fp->sp to base to find the most recently calculated
        // value matching v under assumption that it is the value that caused
        // the exception.
        int stackHits = 0;
        Value s;
        do {
            if (!index)
                return true;
            s = iter.frameSlotValue(--index);
        } while (s != v || stackHits++ != skipStackHits);

        // If index is out of bounds in pcstack, the blamed value must be one
        // pushed by the current bytecode, so restore *valuepc.
        if (index < size_t(pcstack.depth()))
            *valuepc = pcstack[index];
        else
            *valuepc = current;
    } else {
        *valuepc = pcstack[spindex];
    }
    return true;
}

static bool
DecompileExpressionFromStack(JSContext *cx, int spindex, int skipStackHits, HandleValue v, char **res)
{
    JS_ASSERT(spindex < 0 ||
              spindex == JSDVG_IGNORE_STACK ||
              spindex == JSDVG_SEARCH_STACK);

    *res = NULL;

#ifdef JS_MORE_DETERMINISTIC
    /*
     * Give up if we need deterministic behavior for differential testing.
     * IonMonkey doesn't use StackFrames and this ensures we get the same
     * error messages.
     */
    return true;
#endif

    ScriptFrameIter frameIter(cx);

    if (frameIter.done())
        return true;

    RootedScript script(cx, frameIter.script());
    AutoCompartment ac(cx, &script->global());
    jsbytecode *valuepc = frameIter.pc();
    RootedFunction fun(cx, frameIter.isFunctionFrame()
                           ? frameIter.callee()
                           : NULL);

    JS_ASSERT(script->code <= valuepc && valuepc < script->code + script->length);

    // Give up if in prologue.
    if (valuepc < script->main())
        return true;

    if (!FindStartPC(cx, frameIter, spindex, skipStackHits, v, &valuepc))
        return false;
    if (!valuepc)
        return true;

    ExpressionDecompiler ed(cx, script, fun);
    if (!ed.init())
        return false;
    if (!ed.decompilePC(valuepc))
        return false;

    return ed.getOutput(res);
}

char *
js::DecompileValueGenerator(JSContext *cx, int spindex, HandleValue v,
                            HandleString fallbackArg, int skipStackHits)
{
    RootedString fallback(cx, fallbackArg);
    {
        char *result;
        if (!DecompileExpressionFromStack(cx, spindex, skipStackHits, v, &result))
            return NULL;
        if (result) {
            if (strcmp(result, "(intermediate value)"))
                return result;
            js_free(result);
        }
    }
    if (!fallback) {
        if (v.isUndefined())
            return JS_strdup(cx, js_undefined_str); // Prevent users from seeing "(void 0)"
        fallback = ValueToSource(cx, v);
        if (!fallback)
            return NULL;
    }

    Rooted<JSLinearString *> linear(cx, fallback->ensureLinear(cx));
    if (!linear)
        return NULL;
    TwoByteChars tbchars(linear->chars(), linear->length());
    return LossyTwoByteCharsToNewLatin1CharsZ(cx, tbchars).c_str();
}

static bool
DecompileArgumentFromStack(JSContext *cx, int formalIndex, char **res)
{
    JS_ASSERT(formalIndex >= 0);

    *res = NULL;

#ifdef JS_MORE_DETERMINISTIC
    /* See note in DecompileExpressionFromStack. */
    return true;
#endif

    /*
     * Settle on the nearest script frame, which should be the builtin that
     * called the intrinsic.
     */
    ScriptFrameIter frameIter(cx);
    JS_ASSERT(!frameIter.done());

    /*
     * Get the second-to-top frame, the caller of the builtin that called the
     * intrinsic.
     */
    ++frameIter;
    if (frameIter.done())
        return true;

    RootedScript script(cx, frameIter.script());
    AutoCompartment ac(cx, &script->global());
    jsbytecode *current = frameIter.pc();
    RootedFunction fun(cx, frameIter.isFunctionFrame()
                       ? frameIter.callee()
                       : NULL);

    JS_ASSERT(script->code <= current && current < script->code + script->length);

    if (current < script->main())
        return true;

    /* Don't handle getters, setters or calls from fun.call/fun.apply. */
    if (JSOp(*current) != JSOP_CALL || static_cast<unsigned>(formalIndex) >= GET_ARGC(current))
        return true;

    PCStack pcStack;
    if (!pcStack.init(cx, script, current))
        return false;

    int formalStackIndex = pcStack.depth() - GET_ARGC(current) + formalIndex;
    JS_ASSERT(formalStackIndex >= 0);
    if (formalStackIndex >= pcStack.depth())
        return true;

    ExpressionDecompiler ed(cx, script, fun);
    if (!ed.init())
        return false;
    if (!ed.decompilePC(pcStack[formalStackIndex]))
        return false;

    return ed.getOutput(res);
}

char *
js::DecompileArgument(JSContext *cx, int formalIndex, HandleValue v)
{
    {
        char *result;
        if (!DecompileArgumentFromStack(cx, formalIndex, &result))
            return NULL;
        if (result) {
            if (strcmp(result, "(intermediate value)"))
                return result;
            js_free(result);
        }
    }
    if (v.isUndefined())
        return JS_strdup(cx, js_undefined_str); // Prevent users from seeing "(void 0)"
    RootedString fallback(cx, ValueToSource(cx, v));
    if (!fallback)
        return NULL;

    Rooted<JSLinearString *> linear(cx, fallback->ensureLinear(cx));
    if (!linear)
        return NULL;
    return LossyTwoByteCharsToNewLatin1CharsZ(cx, linear->range()).c_str();
}

unsigned
js_ReconstructStackDepth(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return ReconstructPCStack(cx, script, pc, NULL);
}

#define LOCAL_ASSERT_CUSTOM(expr, BAD_EXIT)                                   \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(expr);                                                      \
        if (!(expr)) { BAD_EXIT; }                                            \
    JS_END_MACRO
#define LOCAL_ASSERT_RV(expr, rv)                                             \
    LOCAL_ASSERT_CUSTOM(expr, return (rv))
#define LOCAL_ASSERT(expr)      LOCAL_ASSERT_RV(expr, -1);

static int
SimulateOp(JSScript *script, JSOp op, const JSCodeSpec *cs,
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

/* Ensure that script analysis reports the same stack depth. */
static void
AssertPCDepth(JSScript *script, jsbytecode *pc, unsigned pcdepth) {
    /*
     * If this assertion fails, run the failing test case under gdb and use the
     * following gdb command to understand the execution path of this function.
     *
     *     call js_DumpScriptDepth(cx, script, pc)
     */
    JS_ASSERT_IF(script->hasAnalysis() && script->analysis()->maybeCode(pc),
                 script->analysis()->getCode(pc).stackDepth == pcdepth);
}

static int
ReconstructPCStack(JSContext *cx, JSScript *script, jsbytecode *target, jsbytecode **pcstack)
{
    /*
     * Walk forward from script->code and compute the stack depth and stack of
     * operand-generating opcode PCs in pcstack.
     *
     * Every instruction has a statically computable stack depth before and
     * after. This function returns the stack depth before the target
     * instruction.
     *
     * The stack depth can be computed from these three common-sense rules...
     *
     *   - The stack depth before the first instruction of the script is 0.
     *
     *   - The stack depth after an instruction is always simply the stack
     *     depth before, plus whatever effect of the instruction has.
     *     SimulateOp computes this.
     *
     *   - Except for three specific cases listed below, the stack depth before
     *     an instruction is simply the stack depth after the preceding one.
     *
     *  ... and these three less obvious rules:
     *
     *   - Special case 1: break, continue, or return statements that exit
     *     certain kinds of blocks.
     *
     *     Rule: The stack depth before the next instruction (following the
     *     GOTO/RETRVAL) equals the stack depth before the first SRC_HIDDEN
     *     instruction.
     *
     *   - Special case 2: The rethrow at the end of conditional catch blocks.
     *
     *     Rule: The stack depth before the next rethrow instruction of a
     *     conditional catch block equals the stack depth at the end of the IFEQ
     *     of the catch condition (which is jumping to this re-throw).
     *
     *   - Special case 3: Conditional expressions (A ? B : C).
     *
     *     Rule: The code for B and C are pushing a result on the stack, so the
     *     stack depth before C equals the stack depth before B, which is one
     *     less than the stack depth after the GOTO ending B.
     */

    LOCAL_ASSERT(script->code <= target && target < script->code + script->length);

    /*
     * Save depth of hidden instructions to recover the depth if a branch is not
     * taken.
     */
    unsigned hpcdepth = unsigned(-1);

    /*
     * Save depth of catch instructions to recover it in the rethrow path, at
     * the end of a conditional catch.
     */
    unsigned cpcdepth = unsigned(-1);

    jsbytecode *pc;
    unsigned pcdepth;
    ptrdiff_t oplen;
    for (pc = script->code, pcdepth = 0; ; pc += oplen) {
        JSOp op = JSOp(*pc);
        oplen = GetBytecodeLength(pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        jssrcnote *sn = js_GetSrcNote(cx, script, pc);

        /*
         * *** Special case 1.a ***
         *
         * Hidden instructions are used to pop scoped variables (leaveblock) in
         * early-exit paths and are not supposed to affect the stack depth when
         * they are not taken. However, when the target is in an early-exit
         * path, hidden instructions need to be taken into account.
         *
         * As we are not following any branch, we need to restore the stack
         * depth after any instruction which is breaking the flow of the
         * program. Sequences of hidden instructions are *included* in the
         * following grammar:
         *
         *   ExitSequence:
         *     EarlyExit                      // return / break / continue
         *   | ConditionalCatchExit           // conditional catch-block exit
         *
         *   EarlyExit:
         *     setrval; ExitSegment* retrval; // return stmt
         *   | ExitSegment* goto;             // break/continue stmt
         *
         *   ConditionalCatchExit:
         *     ExitSegment* hidden goto; CatchRethrow
         *
         *   CatchRethrow:
         *     hidden throw; end-brace nop;   // last catch-block
         *   | hidden throw; finally;         // followed by a finally
         *   | hidden throwing; hidden leaveblock; catch enterblock;
         *                                    // followed by a catch-block
         *
         *   ExitSegment:
         *     hidden leaveblock;             // leave let-block or
         *                                    // leave catch-block
         *   | hidden leavewith;              // leave with-block
         *   | hidden gosub;                  // leave try or
         *                                    // leave catch-block with finally
         *   | hidden enditer;                // leave for-in/of block
         *   | hidden leaveforletin; hidden enditer; hidden popn;
         *                                    // leave for-let-in/of block
         *
         * The following code save the depth of the first hidden
         * instruction. When we hit either the last instruction of the EarlyExit
         * (retrval/goto) or the goto of a ConditionalCacthExit, we restore (see
         * Special case 1.b) the depth after the execution of the instruction.
         *
         * In case of a ConditionalCacthExit, we keep the hidden depth after the
         * hidden goto to again restore (see Special case 1.b) the depth after
         * the throw/throwing instruction of the CatchRethrow.
         *
         * Note: The last instructions of the EarlyExit, such as break, continue
         * and return are not hidden as they are part of the original sources,
         * so we need to consider them as-if they were hidden instruction (do
         * not reset the depth) and restore the depth after their execution.  It
         * is important to restore them after their execution, and not at the
         * next instruction, because they might be followed by another sequence
         * of hidden instruction.
         */
        bool exitPath =
            op == JSOP_GOTO ||
            op == JSOP_RETRVAL ||
            op == JSOP_THROW;

        bool isConditionalCatchExit = false;

        if (sn && SN_TYPE(sn) == SRC_HIDDEN) {
            /* Only ConditionalCatchExit's goto are hidden.  */
            isConditionalCatchExit = op == JSOP_GOTO;
            if (hpcdepth == unsigned(-1))
                hpcdepth = pcdepth;
        } else if (!exitPath) {
            hpcdepth = unsigned(-1);
        }


        /*
         * *** Special case 2 ***
         *
         * The catch depth is used to restore the depth expected in case of
         * rethrow or in case of guard failure.  The depth expected by throw and
         * throwing instructions correspond to the result of enterblock and
         * exception instructions.
         *
         *     enterblock depth d    <-- depth += d
         *     exception             <-- depth += 1
         *
         * This is not a problem, except with conditional catch staements.
         * Conditional catch statements are looking like:
         *
         *     ifeq +...             <-- jump to the rethrow path.
         *     pop                   <-- pop the exception value.
         *
         * Unfortunately, we iterate linearly over the bytecode, so we cannot
         * use a stack to save the depth for each catch statement that we
         * encounter. The trick done here is to save the stack depth before we
         * unwind the scoped variables with leaveblock, and restore the stack
         * depth plus one to account for the missing exception value.
         *
         *     {Catch}  leaveblock d <-- save depth
         *   (          gosub ... )?
         *     {Hidden} goto ...
         *   ( {Hidden} throw )?     <-- restore depth + 1
         *
         * If there is no rethrow, then we should expect an end-brace nop or a
         * finally.
         */
        if (op == JSOP_LEAVEBLOCK && sn && SN_TYPE(sn) == SRC_CATCH) {
            LOCAL_ASSERT(cpcdepth == unsigned(-1));
            cpcdepth = pcdepth;
        } else if (sn && SN_TYPE(sn) == SRC_HIDDEN &&
                   (op == JSOP_THROW || op == JSOP_THROWING))
        {
            /*
             * The current catch block is conditional, we compensate for the pop
             * of the exception added after the ifeq by adding 1 to the restored
             * depth.  This fake slot might not contain the right value, but
             * it will be eliminated with the throw or throwing instruction.
             */
            LOCAL_ASSERT(cpcdepth != unsigned(-1));
            pcdepth = cpcdepth + 1;
            cpcdepth = unsigned(-1);
        } else if (!(op == JSOP_GOTO && sn && SN_TYPE(sn) == SRC_HIDDEN) &&
                   !(op == JSOP_GOSUB && cpcdepth != unsigned(-1)))
        {
            /*
             * We need to ignore gosub and goto when we exit a catch block.
             * Otherwise we might reset the catch depth before hitting the throw
             * or throwing instruction.
             *
             *     {Catch}  leaveblock d
             *   (          gosub ... )?
             *     {Hidden} goto ...
             *   ( {Hidden} throw )?
             *              finally
             *
             * If there is no rethrow, the end-brace nop or the finally
             * instruction will reset the saved depth.
             */
            if (cpcdepth != unsigned(-1))
                LOCAL_ASSERT(op == JSOP_NOP || op == JSOP_FINALLY);
            cpcdepth = unsigned(-1);
        }

        /* At this point, pcdepth is the stack depth *before* the insn at pc. */
        AssertPCDepth(script, pc, pcdepth);
        if (pc >= target)
            break;

        /* Simulate the instruction at pc. */
        if (SimulateOp(script, op, cs, pc, pcstack, pcdepth) < 0)
            return -1;

        /* At this point, pcdepth is the stack depth *after* the insn at pc. */

        /*
         * *** Special case 1.b ***
         *
         * Restore the stack depth after the execution of an EarlyExit or the
         * goto of the ConditionalCatchExit.
         */
        if (exitPath && hpcdepth != unsigned(-1)) {
            pcdepth = hpcdepth;

            /* Keep the depth if we are on the ConditionalCatchExit path. */
            if (!isConditionalCatchExit)
                hpcdepth = unsigned(-1);
        }

        /*
         * *** Special case 3 ***
         *
         * A (C ? T : E) expression requires skipping T if target is in E or
         * after the whole expression, because this expression is pushing a
         * result on the stack and the goto cannot be skipped.
         *
         * 09 001 pc   :  ifeq +11
         *    000 pc+ 5:  null
         *    001 pc+ 6:  goto +... <-- The stack after the goto
         *    000 pc+11:  ...       <-- does not have the same depth.
         *
         */
        if (sn && SN_TYPE(sn) == SRC_COND) {
            ptrdiff_t jmplen = GET_JUMP_OFFSET(pc);
            if (pc + jmplen <= target) {
                /* Target does not lie in T. */
                oplen = jmplen;
            }
        }
    }
    LOCAL_ASSERT(pc == target);
    AssertPCDepth(script, pc, pcdepth);
    return pcdepth;
}

#undef LOCAL_ASSERT
#undef LOCAL_ASSERT_RV

bool
js::CallResultEscapes(jsbytecode *pc)
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
js::IsValidBytecodeOffset(JSContext *cx, JSScript *script, size_t offset)
{
    // This could be faster (by following jump instructions if the target is <= offset).
    for (BytecodeRange r(cx, script); !r.empty(); r.popFront()) {
        size_t here = r.frontOffset();
        if (here >= offset)
            return here == offset;
    }
    return false;
}

JS_FRIEND_API(size_t)
js::GetPCCountScriptCount(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();

    if (!rt->scriptAndCountsVector)
        return 0;

    return rt->scriptAndCountsVector->length();
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
                          double *values, const char * const *names, unsigned count,
                          MaybeComma &comma)
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
js::GetPCCountScriptSummary(JSContext *cx, size_t index)
{
    JSRuntime *rt = cx->runtime();

    if (!rt->scriptAndCountsVector || index >= rt->scriptAndCountsVector->length()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BUFFER_TOO_SMALL);
        return NULL;
    }

    const ScriptAndCounts &sac = (*rt->scriptAndCountsVector)[index];
    RootedScript script(cx, sac.script);

    /*
     * OOM on buffer appends here will not be caught immediately, but since
     * StringBuffer uses a ContextAllocPolicy will trigger an exception on the
     * context if they occur, which we'll catch before returning.
     */
    StringBuffer buf(cx);

    buf.append('{');

    AppendJSONProperty(buf, "file", NO_COMMA);
    JSString *str = JS_NewStringCopyZ(cx, script->filename());
    if (!str || !(str = ValueToSource(cx, StringValue(str))))
        return NULL;
    buf.append(str);

    AppendJSONProperty(buf, "line");
    NumberValueToStringBuffer(cx, Int32Value(script->lineno), buf);

    if (script->function()) {
        JSAtom *atom = script->function()->displayAtom();
        if (atom) {
            AppendJSONProperty(buf, "name");
            if (!(str = ValueToSource(cx, StringValue(atom))))
                return NULL;
            buf.append(str);
        }
    }

    double baseTotals[PCCounts::BASE_LIMIT] = {0.0};
    double accessTotals[PCCounts::ACCESS_LIMIT - PCCounts::BASE_LIMIT] = {0.0};
    double elementTotals[PCCounts::ELEM_LIMIT - PCCounts::ACCESS_LIMIT] = {0.0};
    double propertyTotals[PCCounts::PROP_LIMIT - PCCounts::ACCESS_LIMIT] = {0.0};
    double arithTotals[PCCounts::ARITH_LIMIT - PCCounts::BASE_LIMIT] = {0.0};

    for (unsigned i = 0; i < script->length; i++) {
        PCCounts &counts = sac.getPCCounts(script->code + i);
        if (!counts)
            continue;

        JSOp op = (JSOp)script->code[i];
        unsigned numCounts = PCCounts::numCounts(op);

        for (unsigned j = 0; j < numCounts; j++) {
            double value = counts.get(j);
            if (j < PCCounts::BASE_LIMIT) {
                baseTotals[j] += value;
            } else if (PCCounts::accessOp(op)) {
                if (j < PCCounts::ACCESS_LIMIT)
                    accessTotals[j - PCCounts::BASE_LIMIT] += value;
                else if (PCCounts::elementOp(op))
                    elementTotals[j - PCCounts::ACCESS_LIMIT] += value;
                else if (PCCounts::propertyOp(op))
                    propertyTotals[j - PCCounts::ACCESS_LIMIT] += value;
                else
                    JS_NOT_REACHED("Bad opcode");
            } else if (PCCounts::arithOp(op)) {
                arithTotals[j - PCCounts::BASE_LIMIT] += value;
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

    uint64_t ionActivity = 0;
    jit::IonScriptCounts *ionCounts = sac.getIonCounts();
    while (ionCounts) {
        for (size_t i = 0; i < ionCounts->numBlocks(); i++)
            ionActivity += ionCounts->block(i).hitCount();
        ionCounts = ionCounts->previous();
    }
    if (ionActivity) {
        AppendJSONProperty(buf, "ion", comma);
        NumberValueToStringBuffer(cx, DoubleValue(ionActivity), buf);
    }

    buf.append('}');
    buf.append('}');

    if (cx->isExceptionPending())
        return NULL;

    return buf.finishString();
}

static bool
GetPCCountJSON(JSContext *cx, const ScriptAndCounts &sac, StringBuffer &buf)
{
    RootedScript script(cx, sac.script);

    buf.append('{');
    AppendJSONProperty(buf, "text", NO_COMMA);

    JSString *str = JS_DecompileScript(cx, script, NULL, 0);
    if (!str || !(str = ValueToSource(cx, StringValue(str))))
        return false;

    buf.append(str);

    AppendJSONProperty(buf, "line");
    NumberValueToStringBuffer(cx, Int32Value(script->lineno), buf);

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

        {
            ExpressionDecompiler ed(cx, script, script->function());
            if (!ed.init())
                return false;
            if (!ed.decompilePC(pc))
                return false;
            char *text;
            if (!ed.getOutput(&text))
                return false;
            AppendJSONProperty(buf, "text");
            JSString *str = JS_NewStringCopyZ(cx, text);
            js_free(text);
            if (!str || !(str = ValueToSource(cx, StringValue(str))))
                return false;
            buf.append(str);
        }

        PCCounts &counts = sac.getPCCounts(pc);
        unsigned numCounts = PCCounts::numCounts(op);

        AppendJSONProperty(buf, "counts");
        buf.append('{');

        MaybeComma comma = NO_COMMA;
        for (unsigned i = 0; i < numCounts; i++) {
            double value = counts.get(i);
            if (value > 0) {
                AppendJSONProperty(buf, PCCounts::countName(op, i), comma);
                comma = COMMA;
                NumberValueToStringBuffer(cx, DoubleValue(value), buf);
            }
        }

        buf.append('}');
        buf.append('}');
    }

    buf.append(']');

    jit::IonScriptCounts *ionCounts = sac.getIonCounts();
    if (ionCounts) {
        AppendJSONProperty(buf, "ion");
        buf.append('[');
        bool comma = false;
        while (ionCounts) {
            if (comma)
                buf.append(',');
            comma = true;

            buf.append('[');
            for (size_t i = 0; i < ionCounts->numBlocks(); i++) {
                if (i)
                    buf.append(',');
                const jit::IonBlockCounts &block = ionCounts->block(i);

                buf.append('{');
                AppendJSONProperty(buf, "id", NO_COMMA);
                NumberValueToStringBuffer(cx, Int32Value(block.id()), buf);
                AppendJSONProperty(buf, "offset");
                NumberValueToStringBuffer(cx, Int32Value(block.offset()), buf);
                AppendJSONProperty(buf, "successors");
                buf.append('[');
                for (size_t j = 0; j < block.numSuccessors(); j++) {
                    if (j)
                        buf.append(',');
                    NumberValueToStringBuffer(cx, Int32Value(block.successor(j)), buf);
                }
                buf.append(']');
                AppendJSONProperty(buf, "hits");
                NumberValueToStringBuffer(cx, DoubleValue(block.hitCount()), buf);

                AppendJSONProperty(buf, "code");
                JSString *str = JS_NewStringCopyZ(cx, block.code());
                if (!str || !(str = ValueToSource(cx, StringValue(str))))
                    return false;
                buf.append(str);

                AppendJSONProperty(buf, "instructionBytes");
                NumberValueToStringBuffer(cx, Int32Value(block.instructionBytes()), buf);

                AppendJSONProperty(buf, "spillBytes");
                NumberValueToStringBuffer(cx, Int32Value(block.spillBytes()), buf);

                buf.append('}');
            }
            buf.append(']');

            ionCounts = ionCounts->previous();
        }
        buf.append(']');
    }

    buf.append('}');

    return !cx->isExceptionPending();
}

JS_FRIEND_API(JSString *)
js::GetPCCountScriptContents(JSContext *cx, size_t index)
{
    JSRuntime *rt = cx->runtime();

    if (!rt->scriptAndCountsVector || index >= rt->scriptAndCountsVector->length()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BUFFER_TOO_SMALL);
        return NULL;
    }

    const ScriptAndCounts &sac = (*rt->scriptAndCountsVector)[index];
    JSScript *script = sac.script;

    StringBuffer buf(cx);

    if (!script->function() && !script->compileAndGo)
        return buf.finishString();

    {
        AutoCompartment ac(cx, &script->global());
        if (!GetPCCountJSON(cx, sac, buf))
            return NULL;
    }

    return buf.finishString();
}
