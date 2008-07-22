/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
 *   Mike Shaver <shaver@mozilla.org>
 *   David Anderson <danderson@mozilla.com>
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

#include "jsstddef.h"           // always first
#include "jsprf.h"              // low-level (NSPR-based) headers next
#include <math.h>               // standard headers next
#ifdef _MSC_VER
#include <malloc.h>
#define alloca _alloca
#endif

#include "nanojit/avmplus.h"    // nanojit
#include "nanojit/nanojit.h"
#include "jsarray.h"            // higher-level library and API headers
#include "jsbool.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsscope.h"
#include "jsemit.h"
#include "jstracer.h"

#include "jsautooplen.h"        // generated headers last

#ifdef DEBUG
#define ABORT_TRACE(msg)   do { fprintf(stdout, "abort: %d: %s\n", __LINE__, msg); return false; } while(0)
#else
#define ABORT_TRACE(msg)   return false
#endif

#ifdef DEBUG
static struct {
    uint64 recorderStarted, recorderAborted, traceCompleted, sideExitIntoInterpreter,
    typeMapMismatchAtEntry, returnToDifferentLoopHeader, traceTriggered,
    globalShapeMismatchAtEntry, typeMapTrashed, slotDemoted, slotPromoted, unstableLoopVariable;
} stat = { 0LL, };
#define AUDIT(x) (stat.x++)
#else
#define AUDIT(x) ((void)0)
#endif DEBUG

using namespace avmplus;
using namespace nanojit;

static GC gc = GC();
static avmplus::AvmCore* core = new (&gc) avmplus::AvmCore();

Tracker::Tracker()
{
    pagelist = 0;
}

Tracker::~Tracker()
{
    clear();
}

jsuword
Tracker::getPageBase(const void* v) const
{
    return jsuword(v) & ~jsuword(NJ_PAGE_SIZE-1);
}

struct Tracker::Page*
Tracker::findPage(const void* v) const
{
    jsuword base = getPageBase(v);
    struct Tracker::Page* p = pagelist;
    while (p) {
        if (p->base == base) {
            return p;
        }
        p = p->next;
    }
    return 0;
}

struct Tracker::Page*
Tracker::addPage(const void* v) {
    jsuword base = getPageBase(v);
    struct Tracker::Page* p = (struct Tracker::Page*)
        GC::Alloc(sizeof(*p) - sizeof(p->map) + (NJ_PAGE_SIZE >> 2) * sizeof(LIns*));
    p->base = base;
    p->next = pagelist;
    pagelist = p;
    return p;
}

void
Tracker::clear()
{
    while (pagelist) {
        Page* p = pagelist;
        pagelist = pagelist->next;
        GC::Free(p);
    }
}

bool
Tracker::has(const void *v) const
{
    struct Tracker::Page* p = findPage(v);
    if (!p)
        return false;
    return p->map[(jsuword(v) & 0xfff) >> 2] != NULL;
}

LIns*
Tracker::get(const void* v) const
{
    struct Tracker::Page* p = findPage(v);
    JS_ASSERT(p); /* we must have a page for the slot we are looking for */
    LIns* i = p->map[(jsuword(v) & 0xfff) >> 2];
    JS_ASSERT(i);
    return i;
}

void
Tracker::set(const void* v, LIns* i)
{
    struct Tracker::Page* p = findPage(v);
    if (!p)
        p = addPage(v);
    p->map[(jsuword(v) & 0xfff) >> 2] = i;
}

/*
 * Return the coerced type of a value. If it's a number, we always return JSVAL_DOUBLE, no matter
 * whether it's represented as an int or as a double.
 */
static inline int getCoercedType(jsval v)
{
    if (JSVAL_IS_INT(v))
        return JSVAL_DOUBLE;
    return JSVAL_TAG(v);
}

static inline bool isNumber(jsval v)
{
    return JSVAL_IS_INT(v) || JSVAL_IS_DOUBLE(v);
}

static inline jsdouble asNumber(jsval v)
{
    JS_ASSERT(isNumber(v));
    if (JSVAL_IS_DOUBLE(v))
        return *JSVAL_TO_DOUBLE(v);
    return (jsdouble)JSVAL_TO_INT(v);
}

static inline bool isInt32(jsval v)
{
    if (!isNumber(v))
        return false;
    jsdouble d = asNumber(v);
    jsint i;
    return JSDOUBLE_IS_INT(d, i);
}

static LIns* demote(LirWriter *out, LInsp i)
{
    if (i->isCall())
        return callArgN(i,0);
    if (i->isop(LIR_i2f) || i->isop(LIR_u2f))
        return i->oprnd1();
    AvmAssert(i->isconstq());
    double cf = i->constvalf();
    int32_t ci = cf > 0x7fffffff ? uint32_t(cf) : int32_t(cf);
    return out->insImm(ci);
}

static bool isPromoteInt(LIns* i)
{
    jsdouble d;
    return i->isop(LIR_i2f) || (i->isconstq() && ((d = i->constvalf()) == (jsdouble)(jsint)d));
}

static bool isPromoteUint(LIns* i)
{
    jsdouble d;
    return i->isop(LIR_u2f) || (i->isconstq() && ((d = i->constvalf()) == (jsdouble)(jsuint)d));
}

static bool isPromote(LIns* i)
{
    return isPromoteInt(i) || isPromoteUint(i);
}

static bool isconst(LIns* i, int32_t c)
{
    return i->isconst() && i->constval() == c;
}

static bool overflowSafe(LIns* i)
{
    LIns* c;
    return (i->isop(LIR_and) && ((c = i->oprnd2())->isconst()) &&
            ((c->constval() & 0xc0000000) == 0)) ||
           (i->isop(LIR_rsh) && ((c = i->oprnd2())->isconst()) &&
            ((c->constval() > 0)));
}

class FuncFilter: public LirWriter
{
    TraceRecorder& recorder;
public:
    FuncFilter(LirWriter* out, TraceRecorder& _recorder):
        LirWriter(out), recorder(_recorder)
    {
    }

    LInsp ins1(LOpcode v, LInsp s0)
    {
        switch (v) {
          case LIR_fneg:
              if (isPromoteInt(s0)) {
                  LIns* result = out->ins1(LIR_neg, demote(out, s0));
                  out->insGuard(LIR_xt, out->ins1(LIR_ov, result), recorder.snapshot());
                  return out->ins1(LIR_i2f, result);
              }
              break;
          default:;
        }
        return out->ins1(v, s0);
    }

    LInsp ins2(LOpcode v, LInsp s0, LInsp s1)
    {
        if (s0 == s1 && v == LIR_feq) {
            if (isPromote(s0)) {
                // double(int) and double(uint) cannot be nan
                return insImm(1);
            }
            if (s0->isop(LIR_fmul) || s0->isop(LIR_fsub) || s0->isop(LIR_fadd)) {
                LInsp lhs = s0->oprnd1();
                LInsp rhs = s0->oprnd2();
                if (isPromote(lhs) && isPromote(rhs)) {
                    // add/sub/mul promoted ints can't be nan
                    return insImm(1);
                }
            }
        } else if (LIR_feq <= v && v <= LIR_fge) {
            if (isPromoteInt(s0) && isPromoteInt(s1)) {
                // demote fcmp to cmp
                v = LOpcode(v + (LIR_eq - LIR_feq));
                return out->ins2(v, demote(out, s0), demote(out, s1));
            } else if (isPromoteUint(s0) && isPromoteUint(s1)) {
                // uint compare
                v = LOpcode(v + (LIR_eq - LIR_feq));
                if (v != LIR_eq)
                    v = LOpcode(v + (LIR_ult - LIR_lt)); // cmp -> ucmp
                return out->ins2(v, demote(out, s0), demote(out, s1));
            }
        } else if (v == LIR_fadd || v == LIR_fsub) {
            /* demoting multiplication seems to be tricky since it can quickly overflow the
               value range of int32 */
            if (isPromoteInt(s0) && isPromoteInt(s1)) {
                // demote fop to op
                v = (LOpcode)((int)v & ~LIR64);
                LIns* d0;
                LIns* d1;
                LIns* result = out->ins2(v, d0 = demote(out, s0), d1 = demote(out, s1));
                if (!overflowSafe(d0) || !overflowSafe(d1))
                    out->insGuard(LIR_xt, out->ins1(LIR_ov, result), recorder.snapshot());
                return out->ins1(LIR_i2f, result);
            }
        } else if (v == LIR_or && 
                   s0->isop(LIR_lsh) && isconst(s0->oprnd2(), 16) &&
                   s1->isop(LIR_and) && isconst(s1->oprnd2(), 0xffff)) {
            LIns* msw = s0->oprnd1();
            LIns* lsw = s1->oprnd1();
            LIns* x;
            LIns* y;
            if (lsw->isop(LIR_add) &&
                lsw->oprnd1()->isop(LIR_and) && 
                lsw->oprnd2()->isop(LIR_and) &&
                isconst(lsw->oprnd1()->oprnd2(), 0xffff) && 
                isconst(lsw->oprnd2()->oprnd2(), 0xffff) &&
                msw->isop(LIR_add) && 
                msw->oprnd1()->isop(LIR_add) && 
                msw->oprnd2()->isop(LIR_rsh) &&
                msw->oprnd1()->oprnd1()->isop(LIR_rsh) &&
                msw->oprnd1()->oprnd2()->isop(LIR_rsh) &&
                isconst(msw->oprnd2()->oprnd2(), 16) &&
                isconst(msw->oprnd1()->oprnd1()->oprnd2(), 16) &&
                isconst(msw->oprnd1()->oprnd2()->oprnd2(), 16) &&
                (x = lsw->oprnd1()->oprnd1()) == msw->oprnd1()->oprnd1()->oprnd1() &&
                (y = lsw->oprnd2()->oprnd1()) == msw->oprnd1()->oprnd2()->oprnd1() &&
                lsw == msw->oprnd2()->oprnd1()) {
                return out->ins2(LIR_add, x, y);
            }
        }
        return out->ins2(v, s0, s1);
    }

    LInsp insCall(uint32_t fid, LInsp args[])
    {
        LInsp s0 = args[0];
        switch (fid) {
          case F_doubleToUint32:
            if (s0->isconstq())
                return out->insImm(js_DoubleToECMAUint32(s0->constvalf()));
            if (s0->isop(LIR_i2f) || s0->isop(LIR_u2f)) {
                return s0->oprnd1();
            }
            break;
          case F_doubleToInt32:
            if (s0->isconstq())
                return out->insImm(js_DoubleToECMAInt32(s0->constvalf()));
            if (s0->isop(LIR_fadd) || s0->isop(LIR_fsub) || s0->isop(LIR_fmul)) {
                LInsp lhs = s0->oprnd1();
                LInsp rhs = s0->oprnd2();
                if (isPromote(lhs) && isPromote(rhs)) {
                    LOpcode op = LOpcode(s0->opcode() & ~LIR64);
                    return out->ins2(op, demote(out, lhs), demote(out, rhs));
                }
            }
            if (s0->isop(LIR_i2f) || s0->isop(LIR_u2f)) {
                return s0->oprnd1();
            }
            break;
          case F_BoxDouble:
            JS_ASSERT(s0->isQuad());
            if (s0->isop(LIR_i2f)) {
                LIns* args2[] = { s0->oprnd1(), args[1] };
                return out->insCall(F_BoxInt32, args2);
            }
            break;
        }
        return out->insCall(fid, args);
    }
};

/* In debug mode vpname contains a textual description of the type of the
   slot during the forall iteration over al slots. */
#ifdef DEBUG
#define DEF_VPNAME          const char* vpname; unsigned vpnum
#define SET_VPNAME(name)    do { vpname = name; vpnum = 0; } while(0)
#define INC_VPNUM()         do { ++vpnum; } while(0)
#else
#define DEF_VPNAME          do {} while (0)
#define vpname ""
#define vpnum 0
#define SET_VPNAME(name)    ((void)0)
#define INC_VPNUM()         ((void)0)
#endif

/* This macro can be used to iterate over all slots in currently pending
   frames that make up the native frame, including global variables and
   frames consisting of rval, args, vars, and stack (except for the top-
   level frame which does not have args or vars. */
#define FORALL_SLOTS_IN_PENDING_FRAMES(cx, ngslots, gslots, callDepth, code)  \
    JS_BEGIN_MACRO                                                            \
        DEF_VPNAME;                                                           \
        JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);  \
        unsigned n;                                                           \
        jsval* vp;                                                            \
        SET_VPNAME("global");                                                 \
        for (n = 0; n < ngslots; ++n) {                                       \
            vp = &STOBJ_GET_SLOT(globalObj, gslots[n]);                       \
            { code; }                                                         \
            INC_VPNUM();                                                      \
        }                                                                     \
        JSStackFrame* currentFrame = cx->fp;                                  \
        JSStackFrame* entryFrame;                                             \
        JSStackFrame* fp = currentFrame;                                      \
        for (n = 0; n < callDepth; ++n) { fp = fp->down; }                    \
        entryFrame = fp;                                                      \
        unsigned frames = callDepth+1;                                        \
        JSStackFrame** fstack = (JSStackFrame **)alloca(frames * sizeof (JSStackFrame *)); \
        JSStackFrame** fspstop = &fstack[frames];                             \
        JSStackFrame** fsp = fspstop-1;                                       \
        fp = currentFrame;                                                    \
        for (;; fp = fp->down) { *fsp-- = fp; if (fp == entryFrame) break; }  \
        for (fsp = fstack; fsp < fspstop; ++fsp) {                            \
            JSStackFrame* f = *fsp;                                           \
            jsval* vpstop;                                                    \
            SET_VPNAME("rval");                                               \
            vp = &f->rval; code;                                              \
            if (f->callee) {                                                  \
                SET_VPNAME("this");                                           \
                vp = &f->argv[-1];                                            \
                code;                                                         \
                SET_VPNAME("argv");                                           \
                vp = &f->argv[0]; vpstop = &f->argv[f->fun->nargs];           \
                while (vp < vpstop) { code; ++vp; INC_VPNUM(); }              \
                SET_VPNAME("vars");                                           \
                vp = &f->vars[0]; vpstop = &f->vars[f->nvars];                \
                while (vp < vpstop) { code; ++vp; INC_VPNUM(); }              \
            }                                                                 \
            SET_VPNAME("stack");                                              \
            vp = f->spbase; vpstop = f->regs->sp;                             \
            while (vp < vpstop) { code; ++vp; INC_VPNUM(); }                  \
        }                                                                     \
    JS_END_MACRO

TraceRecorder::TraceRecorder(JSContext* cx, GuardRecord* _anchor,
        Fragment* _fragment, uint8* typeMap)
{
    this->cx = cx;
    this->globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    this->anchor = _anchor;
    this->fragment = _fragment;
    this->lirbuf = _fragment->lirbuf;
    this->treeInfo = (TreeInfo*)_fragment->root->vmprivate;
    JS_ASSERT(treeInfo);
    this->entryRegs = &treeInfo->entryRegs;
    this->callDepth = _fragment->calldepth;
    JS_ASSERT(!_anchor || _anchor->calldepth == _fragment->calldepth);
    this->atoms = cx->fp->script->atomMap.vector;

#ifdef DEBUG
    printf("recording starting from %s:%u@%u\n", cx->fp->script->filename,
           js_PCToLineNumber(cx, cx->fp->script, entryRegs->pc),
           entryRegs->pc - cx->fp->script->code);
#endif

    lir = lir_buf_writer = new (&gc) LirBufWriter(lirbuf);
#ifdef DEBUG
    lir = verbose_filter = new (&gc) VerboseWriter(&gc, lir, lirbuf->names);
#endif
    lir = cse_filter = new (&gc) CseFilter(lir, &gc);
    lir = expr_filter = new (&gc) ExprFilter(lir);
    lir = func_filter = new (&gc) FuncFilter(lir, *this);
    lir->ins0(LIR_trace);

    lirbuf->state = addName(lir->insParam(0), "state");
    lirbuf->param1 = addName(lir->insParam(1), "param1");
    lirbuf->sp = addName(lir->insLoadi(lirbuf->state, offsetof(InterpState, sp)), "sp");
    lirbuf->rp = addName(lir->insLoadi(lirbuf->state, offsetof(InterpState, rp)), "rp");
    cx_ins = addName(lir->insLoadi(lirbuf->state, offsetof(InterpState, cx)), "cx");

    uint8* m = typeMap;
    jsuword* localNames = NULL;
#ifdef DEBUG
    void* mark = NULL;
    if (cx->fp->fun) {
        mark = JS_ARENA_MARK(&cx->tempPool);
        localNames = js_GetLocalNameArray(cx, cx->fp->fun, &cx->tempPool);
    }
#else
    localNames = NULL;
#endif
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, treeInfo->ngslots, treeInfo->gslots, callDepth,
        import(vp, *m, vpname, vpnum, localNames); m++
    );
#ifdef DEBUG
    JS_ARENA_RELEASE(&cx->tempPool, mark);
#endif

    recompileFlag = false;
}

TraceRecorder::~TraceRecorder()
{
#ifdef DEBUG
    delete verbose_filter;
#endif
    delete cse_filter;
    delete expr_filter;
    delete func_filter;
    delete lir_buf_writer;
}

/* Add debug information to a LIR instruction as we emit it. */
inline LIns*
TraceRecorder::addName(LIns* ins, const char* name)
{
#ifdef DEBUG
    lirbuf->names->addName(ins, name);
#endif
    return ins;
}

/* Determine the current call depth (starting with the entry frame.) */
unsigned
TraceRecorder::getCallDepth() const
{
    return callDepth;
}

static int
findInternableGlobals(JSContext* cx, JSStackFrame* fp, uint16* slots)
{
    JSObject* globalObj = JS_GetGlobalForObject(cx, fp->scopeChain);
    unsigned count = 0;
    unsigned n;
    JSAtom** atoms = fp->script->atomMap.vector;
    unsigned natoms = fp->script->atomMap.length;
    bool FIXME_bug445262_sawMath = false;
    for (n = 0; n < natoms + 1; ++n) {
        JSAtom* atom;
        if (n < natoms) {
            atom = atoms[n];
            if (atom == CLASS_ATOM(cx, Math))
                FIXME_bug445262_sawMath = true;
        } else {
            if (FIXME_bug445262_sawMath)
                break;
            atom = CLASS_ATOM(cx, Math);
        }
        if (!ATOM_IS_STRING(atom))
            continue;
        jsid id = ATOM_TO_JSID(atom);
        JSObject* pobj;
        JSProperty *prop;
        JSScopeProperty* sprop;
        if (!js_LookupProperty(cx, globalObj, id, &pobj, &prop))
            return -1;
        if (!prop)
            continue; /* property not found -- string constant? */
        if (pobj == globalObj) {
            sprop = (JSScopeProperty*) prop;
            unsigned slot;
            if (SPROP_HAS_STUB_GETTER(sprop) &&
                SPROP_HAS_STUB_SETTER(sprop) &&
                ((slot = sprop->slot) == (uint16)slot) &&
                !VALUE_IS_FUNCTION(cx, STOBJ_GET_SLOT(globalObj, slot))) {
                if (slots)
                    *slots++ = slot;
                ++count;
            }
        }
        JS_UNLOCK_OBJ(cx, pobj);
    }
    return count;
}

/* Calculate the total number of native frame slots we need from this frame
   all the way back to the entry frame, including the current stack usage. */
static unsigned nativeFrameSlots(unsigned ngslots, unsigned callDepth, 
        JSStackFrame* fp, JSFrameRegs& regs)
{
    unsigned slots = ngslots;
    for (;;) {
        slots += 1/*rval*/ + (regs.sp - fp->spbase);
        if (fp->callee)
            slots += 1/*this*/ + fp->fun->nargs + fp->nvars;
        if (callDepth-- == 0)
            return slots;
        fp = fp->down;
    }
    JS_NOT_REACHED("nativeFrameSlots");
}

/* Determine the offset in the native frame (marshal) for an address
   that is part of a currently active frame. */
size_t
TraceRecorder::nativeFrameOffset(jsval* p) const
{
    JSStackFrame* currentFrame = cx->fp;
    size_t offset = 0;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, treeInfo->ngslots, treeInfo->gslots, callDepth,
        if (vp == p) return offset;
        offset += sizeof(double)
    );
    /* if its not in a pending frame, it must be on the stack of the current frame above
       sp but below script->depth */
    JS_ASSERT(size_t(p - currentFrame->regs->sp) < currentFrame->script->depth);
    offset += size_t(p - currentFrame->regs->sp) * sizeof(double);
    return offset;
}

/* Track the maximum number of native frame slots we need during
   execution. */
void
TraceRecorder::trackNativeFrameUse(unsigned slots)
{
    if (slots > treeInfo->maxNativeFrameSlots)
        treeInfo->maxNativeFrameSlots = slots;
}

/* Unbox a jsval into a slot. Slots are wide enough to hold double values
   directly (instead of storing a pointer to them). */
static bool
unbox_jsval(jsval v, uint8 t, double* slot)
{
    jsuint type = TYPEMAP_GET_TYPE(t);
    if (type == TYPEMAP_TYPE_ANY) {
        debug_only(printf("any ");)
        return true;
    }
    if (type == JSVAL_INT) {
        jsint i;
        if (JSVAL_IS_INT(v))
            *(jsint*)slot = JSVAL_TO_INT(v);
        else if (JSVAL_IS_DOUBLE(v) && JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(v), i))
            *(jsint*)slot = i;
        else {
            debug_only(printf("int != tag%lu(value=%lu) ", JSVAL_TAG(v), v);)
            return false;
        }
        debug_only(printf("int<%d> ", *(jsint*)slot);)
        return true;
    }
    if (type == JSVAL_DOUBLE) {
        jsdouble d;
        if (JSVAL_IS_INT(v))
            d = JSVAL_TO_INT(v);
        else if (JSVAL_IS_DOUBLE(v))
            d = *JSVAL_TO_DOUBLE(v);
        else {
            debug_only(printf("double != tag%lu ", JSVAL_TAG(v));)
            return false;
        }
        *(jsdouble*)slot = d;
        debug_only(printf("double<%g> ", d);)
        return true;
    }
    if (JSVAL_TAG(v) != type) {
        debug_only(printf("%d != tag%lu ", type, JSVAL_TAG(v));)
        ABORT_TRACE("type mismatch");
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        *(bool*)slot = JSVAL_TO_BOOLEAN(v);
        debug_only(printf("boolean<%d> ", *(bool*)slot);)
        break;
      case JSVAL_STRING:
        *(JSString**)slot = JSVAL_TO_STRING(v);
        debug_only(printf("string<%p> ", *(JSString**)slot);)
        break;
      default:
        JS_ASSERT(JSVAL_IS_OBJECT(v));
        *(JSObject**)slot = JSVAL_TO_OBJECT(v);
        debug_only(printf("object<%p:%s> ", JSVAL_TO_OBJECT(v),
                            JSVAL_IS_NULL(v)
                            ? "null"
                            : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);)
    }
    return true;
}

/* Box a value from the native stack back into the jsval format. Integers
   that are too large to fit into a jsval are automatically boxed into
   heap-allocated doubles. */
static bool
box_jsval(JSContext* cx, jsval& v, uint8 t, double* slot)
{
    jsuint type = TYPEMAP_GET_TYPE(t);
    if (type == TYPEMAP_TYPE_ANY) {
        debug_only(printf("any ");)
        return true;
    }
    jsint i;
    jsdouble d;
    switch (type) {
      case JSVAL_BOOLEAN:
        v = BOOLEAN_TO_JSVAL(*(bool*)slot);
        debug_only(printf("boolean<%d> ", *(bool*)slot);)
        break;
      case JSVAL_INT:
        i = *(jsint*)slot;
        debug_only(printf("int<%d> ", i);)
      store_int:
        if (INT_FITS_IN_JSVAL(i)) {
            v = INT_TO_JSVAL(i);
            break;
        }
        d = (jsdouble)i;
        goto store_double;
      case JSVAL_DOUBLE:
        d = *slot;
        debug_only(printf("double<%g> ", d);)
        if (JSDOUBLE_IS_INT(d, i))
            goto store_int;
      store_double:
        /* Its safe to trigger the GC here since we rooted all strings/objects and all the
           doubles we already processed. */
        return js_NewDoubleInRootedValue(cx, d, &v);
      case JSVAL_STRING:
        v = STRING_TO_JSVAL(*(JSString**)slot);
        debug_only(printf("string<%p> ", *(JSString**)slot);)
        break;
      default:
        JS_ASSERT(t == JSVAL_OBJECT);
        v = OBJECT_TO_JSVAL(*(JSObject**)slot);
        debug_only(printf("object<%p> ", *(JSObject**)slot);)
        break;
    }
    return true;
}

/* Attempt to unbox the given JS frame into a native frame, checking along the way that the
   supplied typemap holds. */
static bool
unbox(JSContext* cx, unsigned ngslots, uint16* gslots, unsigned callDepth,
        uint8* map, double* native)
{
    debug_only(printf("unbox native@%p ", native);)
    double* np = native;
    uint8* mp = map;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, ngslots, gslots, callDepth,
        if (!unbox_jsval(*vp, *mp, np))
            return false;
        ++mp; ++np;
    );
    debug_only(printf("\n");)
    return true;
}

/* Box the given native frame into a JS frame. This only fails due to a hard error
   (out of memory for example). */
static bool
box(JSContext* cx, unsigned ngslots, uint16* gslots, unsigned callDepth,
    uint8* map, double* native)
{
    debug_only(printf("box native@%p ", native);)
    double* np = native;
    uint8* mp = map;
    /* Root all string and object references first (we don't need to call the GC for this). */
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, ngslots, gslots, callDepth,
        if ((*mp == JSVAL_STRING || *mp == JSVAL_OBJECT) && !box_jsval(cx, *vp, *mp, np))
            return false;
        ++mp; ++np
    );
    /* Now do this again but this time for all values (properly quicker than actually checking
       the type and excluding strings and objects). The GC might kick in when we store doubles,
       but everything is rooted now (all strings/objects and all doubles we already boxed). */
    np = native;
    mp = map;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, ngslots, gslots, callDepth,
        if (!box_jsval(cx, *vp, *mp, np))
            return false;
        ++mp; ++np
    );
    debug_only(printf("\n");)
    return true;
}

/* Emit load instructions onto the trace that read the initial stack state. */
void
TraceRecorder::import(jsval* p, uint8& t, const char *prefix, int index, jsuword *localNames)
{
    JS_ASSERT(TYPEMAP_GET_TYPE(t) != TYPEMAP_TYPE_ANY);
    LIns* ins;
    /* Calculate the offset of this slot relative to the entry stack-pointer value of the
       native stack. Arguments and locals are to the left of the stack pointer (offset
       less than 0). Stack cells start at offset 0. Ed defined the semantics of the stack,
       not me, so don't blame the messenger. */
    ptrdiff_t offset = -treeInfo->nativeStackBase + nativeFrameOffset(p) + 8;
    if (TYPEMAP_GET_TYPE(t) == JSVAL_INT) { /* demoted */
        JS_ASSERT(isInt32(*p));
        /* Ok, we have a valid demotion attempt pending, so insert an integer
           read and promote it to double since all arithmetic operations expect
           to see doubles on entry. The first op to use this slot will emit a
           f2i cast which will cancel out the i2f we insert here. */
        ins = lir->ins1(LIR_i2f, lir->insLoadi(lirbuf->sp, offset));
    } else {
        JS_ASSERT(isNumber(*p) == (TYPEMAP_GET_TYPE(t) == JSVAL_DOUBLE));
        ins = lir->insLoad(t == JSVAL_DOUBLE ? LIR_ldq : LIR_ld, lirbuf->sp, offset);
    }
    tracker.set(p, ins);
#ifdef DEBUG
    char name[64];
    JS_ASSERT(strlen(prefix) < 10);
    if (!strcmp(prefix, "argv")) {
        JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(localNames[index]);
        JS_snprintf(name, sizeof name, "$%s.%s", js_AtomToPrintableString(cx, cx->fp->fun->atom),
                    js_AtomToPrintableString(cx, atom));
    } else if (!strcmp(prefix, "vars")) {
        JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(localNames[index + cx->fp->fun->nargs]);
        JS_snprintf(name, sizeof name, "$%s.%s", js_AtomToPrintableString(cx, cx->fp->fun->atom),
                    js_AtomToPrintableString(cx, atom));
#if 0
    } else if (!strcmp(prefix, "global")) {
        /*
         * Index here is over a set of atoms that has skipped non-strings, so
         * can't easily find matching atom in script's map.  Just use $global<n> for now.
         */
        JSAtom *atom = cx->fp->script->atomMap.vector[index];
        JS_snprintf(name, sizeof name, "$%s", js_AtomToPrintableString(cx, atom));
#endif
    } else {
        JS_snprintf(name, sizeof name, "$%s%d", prefix, index);
    }
    addName(ins, name);

    static const char* typestr[] = {
        "object", "int", "double", "3", "string", "5", "boolean", "any"
    };
    printf("import vp=%p name=%s type=%s flags=%d\n", p, name, typestr[t & 7], t >> 3);
#endif
}

/* Update the tracker, then issue a write back store. */
void
TraceRecorder::set(jsval* p, LIns* i, bool initializing)
{
    JS_ASSERT(initializing || tracker.has(p));
    tracker.set(p, i);
    /* Sink all type casts targeting the stack into the side exit by simply storing the original
       (uncasted) value. Each guard generates the side exit map based on the types of the
       last stores to every stack location, so its safe to not perform them on-trace. */
    if (isPromoteInt(i))
        i = ::demote(lir, i);
    lir->insStorei(i, lirbuf->sp, -treeInfo->nativeStackBase + nativeFrameOffset(p) + 8);
}

LIns*
TraceRecorder::get(jsval* p)
{
    return tracker.get(p);
}

SideExit*
TraceRecorder::snapshot()
{
    /* generate the entry map and stash it in the trace */
    unsigned slots = nativeFrameSlots(treeInfo->ngslots, callDepth, cx->fp, *cx->fp->regs);
    trackNativeFrameUse(slots);
    /* reserve space for the type map */
    LIns* data = lir_buf_writer->skip(slots * sizeof(uint8));
    /* setup side exit structure */
    memset(&exit, 0, sizeof(exit));
    exit.from = fragment;
    exit.calldepth = getCallDepth();
    exit.ip_adj = cx->fp->regs->pc - entryRegs->pc;
    exit.sp_adj = (cx->fp->regs->sp - entryRegs->sp) * sizeof(double);
    exit.rp_adj = exit.calldepth;
    uint8* m = exit.typeMap = (uint8 *)data->payload();
    TreeInfo* ti = (TreeInfo*)fragment->root->vmprivate;
    /* Determine the type of a store by looking at the current type of the actual value the
       interpreter is using. For numbers we have to check what kind of store we used last
       (integer or double) to figure out what the side exit show reflect in its typemap. */
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, ti->ngslots, ti->gslots, callDepth,
        LIns* i = get(vp);
        *m++ = isNumber(*vp)
            ? (isPromoteInt(i) ? JSVAL_INT : JSVAL_DOUBLE)
            : JSVAL_TAG(*vp);
    );
    return &exit;
}

LIns*
TraceRecorder::guard(bool expected, LIns* cond)
{
    return lir->insGuard(expected ? LIR_xf : LIR_xt,
                         cond,
                         snapshot());
}

bool
TraceRecorder::checkType(jsval& v, uint8& t)
{
    if (t == TYPEMAP_TYPE_ANY) /* ignore unused slots */
        return true;
    if (isNumber(v)) {
        /* Initially we start out all numbers as JSVAL_DOUBLE in the type map. If we still
           see a number in v, its a valid trace but we might want to ask to demote the
           slot if we know or suspect that its integer. */
        LIns* i = get(&v);
        if (TYPEMAP_GET_TYPE(t) == JSVAL_DOUBLE) {
            if (isInt32(v) && !TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DONT_DEMOTE)) {
                /* If the value associated with v via the tracker comes from a i2f operation,
                   we can be sure it will always be an int. If we see INCVAR, we similarly
                   speculate that the result will be int, even though this is not
                   guaranteed and this might cause the entry map to mismatch and thus
                   the trace never to be entered. */
                if (i->isop(LIR_i2f) ||
                        (i->isop(LIR_fadd) && i->oprnd2()->isconstq() &&
                                fabs(i->oprnd2()->constvalf()) == 1.0)) {
#ifdef DEBUG
                    printf("demoting type of an entry slot #%ld, triggering re-compilation\n",
                            nativeFrameOffset(&v));
#endif
                    JS_ASSERT(!TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DEMOTE) ||
                            TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DONT_DEMOTE));
                    TYPEMAP_SET_FLAG(t, TYPEMAP_FLAG_DEMOTE);
                    TYPEMAP_SET_TYPE(t, JSVAL_INT);
                    AUDIT(slotDemoted);
                    recompileFlag = true;
                    return true; /* keep going */
                }
            }
            return true;
        }
        /* Looks like we are compiling an integer slot. The recorder always casts to doubles
           after each integer operation, or emits an operation that produces a double right
           away. If we started with an integer, we must arrive here pointing at a i2f cast.
           If not, than demoting the slot didn't work out. Flag the slot to be not
           demoted again. */
        JS_ASSERT(TYPEMAP_GET_TYPE(t) == JSVAL_INT &&
                TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DEMOTE) &&
                !TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DONT_DEMOTE));
        if (!i->isop(LIR_i2f)) {
            AUDIT(slotPromoted);
#ifdef DEBUG
            printf("demoting type of a slot #%ld failed, locking it and re-compiling\n",
                    nativeFrameOffset(&v));
#endif
            TYPEMAP_SET_FLAG(t, TYPEMAP_FLAG_DONT_DEMOTE);
            TYPEMAP_SET_TYPE(t, JSVAL_DOUBLE);
            recompileFlag = true;
            return true; /* keep going, recompileFlag will trigger error when we are done with
                            all the slots */

        }
        JS_ASSERT(isInt32(v));
        /* Looks like we got the final LIR_i2f as we expected. Overwrite the value in that
           slot with the argument of i2f since we want the integer store to flow along
           the loop edge, not the casted value. */
        set(&v, i->oprnd1());
        return true;
    }
    /* for non-number types we expect a precise match of the type */
#ifdef DEBUG
    if (JSVAL_TAG(v) != TYPEMAP_GET_TYPE(t)) {
        printf("Type mismatch: val %c, map %c ", "OID?S?B"[JSVAL_TAG(v)],
               "OID?S?B"[t]);
    }
#endif
    return JSVAL_TAG(v) == TYPEMAP_GET_TYPE(t);
}

/* Make sure that the current values in the given stack frame and all stack frames
   up and including entryFrame are type-compatible with the entry map. */
bool
TraceRecorder::verifyTypeStability(uint8* m)
{
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, treeInfo->ngslots, treeInfo->gslots, callDepth,
        if (!checkType(*vp, *m))
            return false;
        ++m
    );
    return !recompileFlag;
}

void
TraceRecorder::closeLoop(Fragmento* fragmento)
{
    if (!verifyTypeStability(treeInfo->typeMap)) {
        AUDIT(unstableLoopVariable);
        debug_only(printf("Trace rejected: unstable loop variables.\n");)
        return;
    }
    SideExit *exit = snapshot();
    exit->target = fragment->root;
    if (fragment == fragment->root) {
        fragment->lastIns = lir->insGuard(LIR_loop, lir->insImm(1), exit);
    } else {
        fragment->lastIns = lir->insGuard(LIR_x, lir->insImm(1), exit);
    }
    compile(fragmento->assm(), fragment);
    if (anchor) {
        fragment->addLink(anchor);
        fragmento->assm()->patch(anchor);
    }
#ifdef DEBUG
    char* label;
    asprintf(&label, "%s:%u", cx->fp->script->filename,
             js_PCToLineNumber(cx, cx->fp->script, cx->fp->regs->pc));
    fragmento->labels->add(fragment, sizeof(Fragment), 0, label);
#endif
}

int
nanojit::StackFilter::getTop(LInsp guard)
{
    if (sp == frag->lirbuf->sp)
        return guard->exit()->sp_adj + 8;
    JS_ASSERT(sp == frag->lirbuf->rp);
    return guard->exit()->rp_adj + 4;
}

#if defined NJ_VERBOSE
void
nanojit::LirNameMap::formatGuard(LIns *i, char *out)
{
    uint32_t ip;
    SideExit *x;

    x = (SideExit *)i->exit();
    ip = intptr_t(x->from->ip) + x->ip_adj;
    sprintf(out,
        "%s: %s %s -> %s sp%+ld rp%+ld",
        formatRef(i),
        lirNames[i->opcode()],
        i->oprnd1()->isCond() ? formatRef(i->oprnd1()) : "",
        labels->format((void *)ip),
        x->sp_adj,
        x->rp_adj
        );
}
#endif

void
nanojit::Assembler::initGuardRecord(LIns *guard, GuardRecord *rec)
{
    SideExit *exit;

    exit = guard->exit();
    rec->guard = guard;
    rec->calldepth = exit->calldepth;
    rec->exit = exit;
    debug_only(rec->sid = exit->sid);
}

void
nanojit::Assembler::asm_bailout(LIns *guard, Register state)
{
    /* we adjust ip/sp/rp when exiting from the tree in the recovery code */
}

void
js_DeleteRecorder(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    delete tm->recorder;
    tm->recorder = NULL;
}

bool
js_StartRecorder(JSContext* cx, GuardRecord* anchor, Fragment* f, uint8* typeMap)
{
    /* start recording if no exception during construction */
    JS_TRACE_MONITOR(cx).recorder = new (&gc) TraceRecorder(cx, anchor, f, typeMap);
    if (cx->throwing) {
        js_AbortRecording(cx, NULL, "setting up recorder failed");
        return false;
    }
    return true;
}

static bool
js_IsLoopExit(JSContext* cx, JSScript* script, jsbytecode* pc)
{
    switch (*pc) {
      case JSOP_LT:
      case JSOP_GT:
      case JSOP_LE:
      case JSOP_GE:
      case JSOP_NE:
      case JSOP_EQ:
        JS_ASSERT(js_CodeSpec[*pc].length == 1);
        pc++;
        /* FALL THROUGH */

      case JSOP_IFNE:
      case JSOP_IFNEX:
        return GET_JUMP_OFFSET(pc) < 0;

      default:;
    }
    return false;
}

#define HOTLOOP 10
#define HOTEXIT 0

bool
js_LoopEdge(JSContext* cx, jsbytecode* oldpc)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* is the recorder currently active? */
    if (tm->recorder) {
        TraceRecorder* r = tm->recorder;
#ifdef JS_THREADSAFE
        /* XXX should this test not be earlier, to avoid even recording? */
        if (OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->title.ownercx != cx) {
#ifdef DEBUG
            printf("Global object not owned by this context.\n");
#endif
            return false; /* we stay away from shared global objects */
        }
#endif
        if (cx->fp->regs->pc == r->getFragment()->root->ip) { /* did we hit the start point? */
            AUDIT(traceCompleted);
            r->closeLoop(JS_TRACE_MONITOR(cx).fragmento);
            js_DeleteRecorder(cx);
        } else {
            AUDIT(returnToDifferentLoopHeader);
            debug_only(printf("loop edge %d -> %d, header %d\n",
                              oldpc - cx->fp->script->code,
                              cx->fp->regs->pc - cx->fp->script->code,
                              (jsbytecode*)r->getFragment()->root->ip - cx->fp->script->code));
            js_AbortRecording(cx, oldpc, "Loop edge does not return to header");
        }
        return false; /* done recording */
    }

    Fragment* f = tm->fragmento->getLoop(cx->fp->regs->pc);
    if (!f->code()) {
        if (++f->hits() >= HOTLOOP) {
            AUDIT(recorderStarted);
            f->calldepth = 0;
            f->root = f;
            /* allocate space to store the LIR for this tree */
            if (!f->lirbuf) {
                f->lirbuf = new (&gc) LirBuffer(tm->fragmento, builtins);
#ifdef DEBUG
                f->lirbuf->names = new (&gc) LirNameMap(&gc, builtins, tm->fragmento->labels);
#endif
            }
            /* create the tree anchor structure */
            TreeInfo* ti = (TreeInfo*)f->vmprivate;
            if (!ti) {
                /* setup the VM-private treeInfo structure for this fragment */
                ti = new TreeInfo(); // TODO: deallocate when fragment dies
                f->vmprivate = ti;

                /* create the list of global properties we want to intern */
                int internableGlobals = findInternableGlobals(cx, cx->fp, NULL);
                if (internableGlobals < 0)
                    return false;
                ti->gslots = (uint16*)malloc(sizeof(uint16) * internableGlobals);
                if ((ti->ngslots = findInternableGlobals(cx, cx->fp, ti->gslots)) < 0)
                    return false;
                JS_ASSERT(ti->ngslots == (unsigned) internableGlobals);
                ti->globalShape = OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->shape;

                /* determine the native frame layout at the entry point */
                unsigned entryNativeFrameSlots = nativeFrameSlots(ti->ngslots,
                        0/*callDepth*/, cx->fp, *cx->fp->regs);
                ti->entryRegs = *cx->fp->regs;
                ti->entryNativeFrameSlots = entryNativeFrameSlots;
                ti->nativeStackBase = (entryNativeFrameSlots -
                        (cx->fp->regs->sp - cx->fp->spbase)) * sizeof(double);
                ti->maxNativeFrameSlots = entryNativeFrameSlots;
                ti->maxCallDepth = 0;
            }

            /* capture the entry type map if we don't have one yet (or we threw it away) */
            if (!ti->typeMap) {
                ti->typeMap = (uint8*)malloc(ti->entryNativeFrameSlots * sizeof(uint8));
                uint8* m = ti->typeMap;

                /* remember the coerced type of each active slot in the type map */
                FORALL_SLOTS_IN_PENDING_FRAMES(cx, ti->ngslots, ti->gslots, 0/*callDepth*/,
                    *m++ = getCoercedType(*vp)
                );
            }
            /* recording primary trace */
            return js_StartRecorder(cx, NULL, f, ti->typeMap);
        }
        return false;
    }

    AUDIT(traceTriggered);

    /* execute previously recorded trace */
    TreeInfo* ti = (TreeInfo*)f->vmprivate;
    if (OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->shape != ti->globalShape) {
        AUDIT(globalShapeMismatchAtEntry);
        debug_only(printf("global shape mismatch, discarding trace (started pc %u line %u).\n",
                          (jsbytecode*)f->root->ip - cx->fp->script->code,
                          js_PCToLineNumber(cx, cx->fp->script, (jsbytecode*)f->root->ip));)
        f->releaseCode(tm->fragmento);
        return false;
    }

    double* native = (double *)alloca((ti->maxNativeFrameSlots+1) * sizeof(double));
    debug_only(*(uint64*)&native[ti->maxNativeFrameSlots] = 0xdeadbeefdeadbeefLL;)
    if (!unbox(cx, ti->ngslots, ti->gslots, 0 /*callDepth*/, ti->typeMap, native)) {
        AUDIT(typeMapMismatchAtEntry);
        debug_only(printf("type-map mismatch, skipping trace.\n");)
        return false;
    }
    double* entry_sp = &native[ti->nativeStackBase/sizeof(double) +
                               (cx->fp->regs->sp - cx->fp->spbase - 1)];
    JSObject** callstack = (JSObject**)alloca(ti->maxCallDepth * sizeof(JSObject*));
    InterpState state;
    state.ip = cx->fp->regs->pc;
    state.sp = (void*)entry_sp;
    state.rp = callstack;
    state.cx = cx;
    union { NIns *code; GuardRecord* (FASTCALL *func)(InterpState*, Fragment*); } u;
    u.code = f->code();
#if defined(DEBUG) && defined(NANOJIT_IA32)
    printf("entering trace at %s:%u@%u, sp=%p\n",
           cx->fp->script->filename, js_PCToLineNumber(cx, cx->fp->script, cx->fp->regs->pc),
           cx->fp->regs->pc - cx->fp->script->code,
           state.sp);
    uint64 start = rdtsc();
#endif
    GuardRecord* lr = u.func(&state, NULL);
    JS_ASSERT(lr->calldepth == 0);
    cx->fp->regs->sp += (lr->exit->sp_adj / sizeof(double));
    cx->fp->regs->pc += lr->exit->ip_adj;
#if defined(DEBUG) && defined(NANOJIT_IA32)
    printf("leaving trace at %s:%u@%u, sp=%p, ip=%p, cycles=%llu\n",
           cx->fp->script->filename, js_PCToLineNumber(cx, cx->fp->script, cx->fp->regs->pc),
           cx->fp->regs->pc - cx->fp->script->code,
           state.sp, lr->jmp,
           (rdtsc() - start));
#endif
    box(cx, ti->ngslots, ti->gslots, lr->calldepth, lr->exit->typeMap, native);
    JS_ASSERT(*(uint64*)&native[ti->maxNativeFrameSlots] == 0xdeadbeefdeadbeefLL);

    AUDIT(sideExitIntoInterpreter);

    /* if the side exit terminates the loop, don't try to attach a trace here */
    if (js_IsLoopExit(cx, cx->fp->script, cx->fp->regs->pc))
        return false;

    debug_only(printf("trying to attach another branch to the tree\n");)

    Fragment* c;
    if (!(c = lr->target)) {
        c = tm->fragmento->createBranch(lr, lr->exit);
        c->spawnedFrom = lr->guard;
        c->parent = f;
        lr->exit->target = c;
        lr->target = c;
        c->root = f;
        c->calldepth = lr->calldepth;
    }

    if (++c->hits() >= HOTEXIT) {
        /* start tracing secondary trace from this point */
        c->lirbuf = f->lirbuf;
        return js_StartRecorder(cx, lr, c, lr->guard->exit()->typeMap);
    }
    return false;
}

void
js_AbortRecording(JSContext* cx, jsbytecode* abortpc, const char* reason)
{
    AUDIT(recorderAborted);
    debug_only(if (!abortpc) abortpc = cx->fp->regs->pc;
               printf("Abort recording (line %d, pc %d): %s.\n",
                      js_PCToLineNumber(cx, cx->fp->script, abortpc),
                      abortpc - cx->fp->script->code, reason);)
    JS_ASSERT(JS_TRACE_MONITOR(cx).recorder != NULL);
    Fragment* f = JS_TRACE_MONITOR(cx).recorder->getFragment();
    f->blacklist();
    if (f->root == f) {
        AUDIT(typeMapTrashed);
        debug_only(printf("Root fragment aborted, trashing the type map.\n");)
        TreeInfo* ti = (TreeInfo*)f->vmprivate;
        JS_ASSERT(ti->typeMap);
        ti->typeMap = NULL;
    }
    js_DeleteRecorder(cx);
}

extern void
js_InitJIT(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (!tm->fragmento) {
        Fragmento* fragmento = new (&gc) Fragmento(core, 24);
        debug_only(fragmento->labels = new (&gc) LabelMap(core, NULL);)
        fragmento->assm()->setCallTable(builtins);
        fragmento->pageFree(fragmento->pageAlloc()); // FIXME: prime page cache
        tm->fragmento = fragmento;
    }
    debug_only(memset(&stat, 0, sizeof(stat)));
}

extern void
js_DestroyJIT(JSContext* cx)
{
#ifdef DEBUG
    printf("recorder: started(%llu), aborted(%llu), completed(%llu), different header(%llu), "
           "type map trashed(%llu), slot demoted(%llu), slot promoted(%llu), "
           "unstable loop variable(%llu)\n", stat.recorderStarted, stat.recorderAborted,
           stat.traceCompleted, stat.returnToDifferentLoopHeader, stat.typeMapTrashed,
           stat.slotDemoted, stat.slotPromoted, stat.unstableLoopVariable);
    printf("monitor: triggered(%llu), exits (%llu), type mismatch(%llu), "
           "global mismatch(%llu)\n", stat.traceTriggered, stat.sideExitIntoInterpreter,
           stat.typeMapMismatchAtEntry, stat.globalShapeMismatchAtEntry);
#endif
}

jsval&
TraceRecorder::argval(unsigned n) const
{
    JS_ASSERT(n < cx->fp->fun->nargs);
    return cx->fp->argv[n];
}

jsval&
TraceRecorder::varval(unsigned n) const
{
    JS_ASSERT(n < cx->fp->nvars);
    return cx->fp->vars[n];
}

jsval&
TraceRecorder::stackval(int n) const
{
    jsval* sp = cx->fp->regs->sp;
    JS_ASSERT(size_t((sp + n) - cx->fp->spbase) < cx->fp->script->depth);
    return sp[n];
}

LIns*
TraceRecorder::arg(unsigned n)
{
    return get(&argval(n));
}

void
TraceRecorder::arg(unsigned n, LIns* i)
{
    set(&argval(n), i);
}

LIns*
TraceRecorder::var(unsigned n)
{
    return get(&varval(n));
}

void
TraceRecorder::var(unsigned n, LIns* i)
{
    set(&varval(n), i);
}

LIns*
TraceRecorder::stack(int n)
{
    return get(&stackval(n));
}

void
TraceRecorder::stack(int n, LIns* i)
{
    set(&stackval(n), i, n >= 0);
}

LIns* TraceRecorder::f2i(LIns* f)
{
    return lir->insCall(F_doubleToInt32, &f);
}

bool TraceRecorder::ifop()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_BOOLEAN(v)) {
        guard(!JSVAL_TO_BOOLEAN(v), lir->ins_eq0(get(&v)));
    } else if (JSVAL_IS_OBJECT(v)) {
        guard(!JSVAL_IS_NULL(v), lir->ins_eq0(get(&v)));
    } else if (isNumber(v)) {
        jsdouble d = asNumber(v);
        jsdpun u;
        u.d = 0;
        /* XXX need to handle NaN! */
        guard(d == 0, lir->ins2(LIR_feq, get(&v), lir->insImmq(u.u64)));
    } else if (JSVAL_IS_STRING(v)) {
        ABORT_TRACE("strings not supported");
    } else {
        ABORT_TRACE("unknown type, wtf");
    }
    return true;
}

bool
TraceRecorder::inc(jsval& v, jsint incr, bool pre)
{
    LIns* v_ins = get(&v);
    if (!inc(v, v_ins, incr, pre))
        return false;
    set(&v, v_ins);
    return true;
}

/* 
 * On exit, v_ins is the incremented unboxed value, and the appropriate
 * value (pre- or post-increment as described by pre) is stacked.
 */
bool
TraceRecorder::inc(jsval& v, LIns*& v_ins, jsint incr, bool pre)
{
    if (!isNumber(v))
        ABORT_TRACE("can only inc numbers");

    jsdpun u;
    u.d = jsdouble(incr);

    LIns* v_after = lir->ins2(LIR_fadd, v_ins, lir->insImmq(u.u64));

    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    JS_ASSERT(cs.ndefs == 1);
    stack(-cs.nuses, pre ? v_after : v_ins);
    v_ins = v_after;
    return true;
}

bool
TraceRecorder::incProp(jsint incr, bool pre)
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("incProp on primitive");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);

    uint32 slot;
    LIns* v_ins;
    if (!prop(obj, obj_ins, slot, v_ins))
        return false;

    jsval& v = STOBJ_GET_SLOT(obj, slot);
    if (!inc(v, v_ins, incr, pre))
        return false;

    if (!box_jsval(v, v_ins))
        return false;

    LIns* dslots_ins = NULL;
    stobj_set_slot(obj_ins, slot, dslots_ins, v_ins);
    return true;
}

bool
TraceRecorder::incElem(jsint incr, bool pre)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    jsval* vp;
    LIns* v_ins;
    LIns* addr_ins;
    if (!elem(l, r, vp, v_ins, addr_ins))
        return false;
    if (!inc(*vp, v_ins, incr, pre))
        return false;
    if (!box_jsval(*vp, v_ins))
        return false;
    lir->insStorei(v_ins, addr_ins, 0);
    return true;
}

bool
TraceRecorder::cmp(LOpcode op, bool negate)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (isNumber(l) && isNumber(r)) {
        LIns* x = lir->ins2(op, get(&l), get(&r));
        if (negate)
            x = lir->ins_eq0(x);
        bool cond;
        switch (op) {
          case LIR_flt:
            cond = asNumber(l) < asNumber(r);
            break;
          case LIR_fgt:
            cond = asNumber(l) > asNumber(r);
            break;
          case LIR_fle:
            cond = asNumber(l) <= asNumber(r);
            break;
          case LIR_fge:
            cond = asNumber(l) >= asNumber(r);
            break;
          default:
            JS_ASSERT(op == LIR_feq);
            cond = (asNumber(l) == asNumber(r)) ^ negate;
            break;
        }

        /* The interpreter fuses comparisons and the following branch,
           so we have to do that here as well. */
        if (cx->fp->regs->pc[1] == JSOP_IFEQ || cx->fp->regs->pc[1] == JSOP_IFNE)
            guard(cond, x);

        /* We update the stack after the guard. This is safe since
           the guard bails out at the comparison and the interpreter
           will this re-execute the comparison. This way the
           value of the condition doesn't have to be calculated and
           saved on the stack in most cases. */
        set(&l, x);
        return true;
    }
    return false;
}

bool
TraceRecorder::unary(LOpcode op)
{
    jsval& v = stackval(-1);
    bool intop = !(op & LIR64);
    if (isNumber(v)) {
        LIns* a = get(&v);
        if (intop)
            a = f2i(a);
        a = lir->ins1(op, a);
        if (intop)
            a = lir->ins1(LIR_i2f, a);
        set(&v, a);
        return true;
    }
    return false;
}

bool
TraceRecorder::binary(LOpcode op)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    bool intop = !(op & LIR64);
    if (isNumber(l) && isNumber(r)) {
        LIns* a = get(&l);
        LIns* b = get(&r);
        if (intop) {
            a = lir->insCall(op == LIR_ush ? F_doubleToUint32 : F_doubleToInt32, &a);
            b = f2i(b);
        }
        a = lir->ins2(op, a, b);
        if (intop)
            a = lir->ins1(op == LIR_ush ? LIR_u2f : LIR_i2f, a);
        set(&l, a);
        return true;
    }
    return false;
}

bool
TraceRecorder::map_is_native(JSObjectMap* map, LIns* map_ins)
{
    LIns* ops = addName(lir->insLoadi(map_ins, offsetof(JSObjectMap, ops)), "ops");
    if (map->ops == &js_ObjectOps) {
        guard(true, addName(lir->ins2(LIR_eq, ops, lir->insImmPtr(&js_ObjectOps)),
                            "guard(native-ops)"));
        return true;
    }
    LIns* n = lir->insLoadi(ops, offsetof(JSObjectOps, newObjectMap));
    if (map->ops->newObjectMap == js_ObjectOps.newObjectMap) {
        guard(true, addName(lir->ins2(LIR_eq, n, lir->insImmPtr((void*)js_ObjectOps.newObjectMap)),
                            "guard(native-map)"));
        return true;
    }
    ABORT_TRACE("non-native map");
}

bool
TraceRecorder::test_property_cache(JSObject* obj, LIns* obj_ins, JSObject*& obj2, jsuword& pcval)
{
    LIns* map_ins = lir->insLoadi(obj_ins, offsetof(JSObject, map));
    if (!map_is_native(obj->map, map_ins))
        return false;

    JSAtom* atom;
    JSPropCacheEntry* entry;
    PROPERTY_CACHE_TEST(cx, cx->fp->regs->pc, obj, obj2, entry, atom);
    if (!atom) {
        pcval = entry->vword;
        return true;
    }

    JSProperty* prop;
    JSScopeProperty* sprop;
    jsid id = ATOM_TO_JSID(atom);
    if (JOF_OPMODE(*cx->fp->regs->pc) == JOF_NAME) {
        if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
            ABORT_TRACE("failed to find name");
    } else {
        if (!js_LookupProperty(cx, obj, id, &obj2, &prop))
            ABORT_TRACE("failed to lookup property");
    }

    sprop = (JSScopeProperty*)prop;
    JSScope* scope = OBJ_SCOPE(obj2);

    jsval v;
    const JSCodeSpec *cs = &js_CodeSpec[*cx->fp->regs->pc];

    if (cs->format & JOF_CALLOP) {
        if (SPROP_HAS_STUB_GETTER(sprop) && SPROP_HAS_VALID_SLOT(sprop, scope) &&
            VALUE_IS_FUNCTION(cx, (v = STOBJ_GET_SLOT(obj2, sprop->slot))) &&
            SCOPE_IS_BRANDED(scope)) {

            /* Call op, "pure" sprop, function value, branded scope: cache method. */
            pcval = JSVAL_OBJECT_TO_PCVAL(v);
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            return true;
        }

        OBJ_DROP_PROPERTY(cx, obj2, prop);
        ABORT_TRACE("can't fast method value for call op");
    }

    if ((((cs->format & JOF_SET) && SPROP_HAS_STUB_SETTER(sprop)) ||
         SPROP_HAS_STUB_GETTER(sprop)) &&
        SPROP_HAS_VALID_SLOT(sprop, scope)) {

        /* Stub accessor of appropriate form and valid slot: cache slot. */
        pcval = SLOT_TO_PCVAL(sprop->slot);
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        return true;
    }

    ABORT_TRACE("not cacheable property find");
}

bool
TraceRecorder::test_property_cache_direct_slot(JSObject* obj, LIns* obj_ins, uint32& slot)
{
    JSObject* obj2;
    jsuword pcval;

    /*
     * Property cache ensures that we are dealing with an existing property,
     * and guards the shape for us.
     */
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;

    /* Handle only gets and sets on the directly addressed object. */
    if (obj2 != obj)
        ABORT_TRACE("PC hit on prototype chain");

    /* Don't trace setter calls, our caller wants a direct slot. */
    if (PCVAL_IS_SPROP(pcval)) {
        JS_ASSERT(js_CodeSpec[*cx->fp->regs->pc].format & JOF_SET);
        JSScopeProperty* sprop = PCVAL_TO_SPROP(pcval);

        if (!SPROP_HAS_STUB_SETTER(sprop))
            ABORT_TRACE("non-stub setter");
        if (!SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj)))
            ABORT_TRACE("no valid slot");
        slot = sprop->slot;
    } else {
        if (!PCVAL_IS_SLOT(pcval))
            ABORT_TRACE("PCE is not a slot");
        slot = PCVAL_TO_SLOT(pcval);
    }
    return true;
}

void
TraceRecorder::stobj_set_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins)
{
    if (slot < JS_INITIAL_NSLOTS) {
        addName(lir->insStorei(v_ins, obj_ins,
                               offsetof(JSObject, fslots) + slot * sizeof(jsval)),
                "set_slot(fslots)");
    } else {
        if (!dslots_ins)
            dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
        addName(lir->insStorei(v_ins, dslots_ins,
                               (slot - JS_INITIAL_NSLOTS) * sizeof(jsval)),
                "set_slot(dslots");
    }
}

LIns*
TraceRecorder::stobj_get_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins)
{
    if (slot < JS_INITIAL_NSLOTS) {
        return lir->insLoadi(obj_ins,
                             offsetof(JSObject, fslots) + slot * sizeof(jsval));
    }

    if (!dslots_ins)
        dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    return lir->insLoadi(dslots_ins, (slot - JS_INITIAL_NSLOTS) * sizeof(jsval));
}

bool
TraceRecorder::native_set(LIns* obj_ins, JSScopeProperty* sprop, LIns*& dslots_ins, LIns* v_ins)
{
    if (SPROP_HAS_STUB_SETTER(sprop) && sprop->slot != SPROP_INVALID_SLOT) {
        stobj_set_slot(obj_ins, sprop->slot, dslots_ins, v_ins);
        return true;
    }
    ABORT_TRACE("unallocated or non-stub sprop");
}

bool
TraceRecorder::native_get(LIns* obj_ins, LIns* pobj_ins, JSScopeProperty* sprop,
        LIns*& dslots_ins, LIns*& v_ins)
{
    if (!SPROP_HAS_STUB_GETTER(sprop))
        return false;

    if (sprop->slot != SPROP_INVALID_SLOT)
        v_ins = stobj_get_slot(pobj_ins, sprop->slot, dslots_ins);
    else
        v_ins = lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID));
    return true;
}

bool
TraceRecorder::box_jsval(jsval v, LIns*& v_ins)
{
    if (isNumber(v)) {
        LIns* args[] = { v_ins, cx_ins };
        v_ins = lir->insCall(F_BoxDouble, args);
        guard(false, lir->ins2(LIR_eq, v_ins, lir->insImmPtr((void*)JSVAL_ERROR_COOKIE)));
        return true;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        v_ins = lir->ins2i(LIR_or, lir->ins2i(LIR_lsh, v_ins, JSVAL_TAGBITS), JSVAL_BOOLEAN);
        return true;
    }
    return false;
}

bool
TraceRecorder::unbox_jsval(jsval v, LIns*& v_ins)
{
    if (isNumber(v)) {
        // JSVAL_IS_NUMBER(v)
        guard(false,
              lir->ins_eq0(lir->ins2(LIR_or,
                                     lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)JSVAL_INT)),
                                     lir->ins2i(LIR_eq,
                                                lir->ins2(LIR_and, v_ins,
                                                          lir->insImmPtr((void*)JSVAL_TAGMASK)),
                                                JSVAL_DOUBLE))));
        v_ins = lir->insCall(F_UnboxDouble, &v_ins);
        return true;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        guard(true,
              lir->ins2i(LIR_eq,
                         lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)JSVAL_TAGMASK)),
                         JSVAL_BOOLEAN));
         v_ins = lir->ins2i(LIR_ush, v_ins, JSVAL_TAGBITS);
         return true;
       case JSVAL_OBJECT:
        guard(true,
              lir->ins2i(LIR_eq,
                         lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)JSVAL_TAGMASK)),
                         JSVAL_OBJECT));
        return true;

    }
    return false;
}

bool
TraceRecorder::getThis(LIns*& this_ins)
{
    if (cx->fp->callee) { /* in a function */
        if (JSVAL_IS_NULL(cx->fp->argv[-1]))
            return false;
        this_ins = get(&cx->fp->argv[-1]);
        guard(false, lir->ins_eq0(this_ins));
    } else { /* in global code */
        JS_ASSERT(!JSVAL_IS_NULL(cx->fp->argv[-1]));
        this_ins = lir->insLoadi(lir->insLoadi(cx_ins, offsetof(JSContext, fp)),
                offsetof(JSStackFrame, thisp));
    }
    return true;
}

bool
TraceRecorder::guardThatObjectHasClass(JSObject* obj, LIns* obj_ins,
                                       JSClass* cls, LIns*& dslots_ins)
{
    if (STOBJ_GET_CLASS(obj) != cls)
        return false;
    LIns* class_ins = stobj_get_slot(obj_ins, JSSLOT_CLASS, dslots_ins);
    class_ins = lir->ins2(LIR_and, class_ins, lir->insImmPtr((void*)~3));
    guard(true, lir->ins2(LIR_eq, class_ins, lir->insImmPtr(cls)));
    return true;
}

bool TraceRecorder::guardThatObjectIsDenseArray(JSObject* obj, LIns* obj_ins, LIns*& dslots_ins)
{
    return guardThatObjectHasClass(obj, obj_ins, &js_ArrayClass, dslots_ins);
}

bool TraceRecorder::guardDenseArrayIndexWithinBounds(JSObject* obj, jsint idx,
        LIns* obj_ins, LIns*& dslots_ins, LIns* idx_ins)
{
    jsuint length = ARRAY_DENSE_LENGTH(obj);
    if (!((jsuint)idx < length && idx < obj->fslots[JSSLOT_ARRAY_LENGTH]))
        return false;
    if (!dslots_ins)
        dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    LIns* length_ins = stobj_get_slot(obj_ins, JSSLOT_ARRAY_LENGTH, dslots_ins);
    // guard(index >= 0)
    guard(true, lir->ins2i(LIR_ge, idx_ins, 0));
    // guard(index < length)
    guard(true, lir->ins2(LIR_lt, idx_ins, length_ins));
    // guard(index < capacity)
    guard(false, lir->ins_eq0(dslots_ins));
    guard(true, lir->ins2(LIR_lt, idx_ins,
                          lir->insLoadi(dslots_ins, -sizeof(jsval))));
    return true;
}

bool TraceRecorder::record_JSOP_INTERRUPT()
{
    return false;
}
bool TraceRecorder::record_JSOP_PUSH()
{
    stack(0, lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID)));
    return true;
}
bool TraceRecorder::record_JSOP_POPV()
{
    jsval& v = stackval(-1);
    set(&cx->fp->rval, get(&v));
    return true;
}
bool TraceRecorder::record_JSOP_ENTERWITH()
{
    return false;
}
bool TraceRecorder::record_JSOP_LEAVEWITH()
{
    return false;
}
bool TraceRecorder::record_JSOP_RETURN()
{
    if (callDepth <= 0)
        return false;
    // this only works if we have a contiguous stack, which CALL enforces
    set(&cx->fp->argv[-2], stack(-1));
    --callDepth; // must not decrement callDepth until after the set 
    atoms = cx->fp->down->script->atomMap.vector;
    return true;
}
bool TraceRecorder::record_JSOP_GOTO()
{
    return true;
}
bool TraceRecorder::record_JSOP_IFEQ()
{
    return ifop();
}
bool TraceRecorder::record_JSOP_IFNE()
{
    return ifop();
}
bool TraceRecorder::record_JSOP_ARGUMENTS()
{
    return false;
}
bool TraceRecorder::record_JSOP_FORARG()
{
    return false;
}
bool TraceRecorder::record_JSOP_FORVAR()
{
    return false;
}
bool TraceRecorder::record_JSOP_DUP()
{
    stack(0, get(&stackval(-1)));
    return true;
}
bool TraceRecorder::record_JSOP_DUP2()
{
    stack(0, get(&stackval(-2)));
    stack(1, get(&stackval(-1)));
    return true;
}
bool TraceRecorder::record_JSOP_SETCONST()
{
    return false;
}
bool TraceRecorder::record_JSOP_BITOR()
{
    return binary(LIR_or);
}
bool TraceRecorder::record_JSOP_BITXOR()
{
    return binary(LIR_xor);
}
bool TraceRecorder::record_JSOP_BITAND()
{
    return binary(LIR_and);
}
bool TraceRecorder::record_JSOP_EQ()
{
    return cmp(LIR_feq);
}
bool TraceRecorder::record_JSOP_NE()
{
    return cmp(LIR_feq, true);
}
bool TraceRecorder::record_JSOP_LT()
{
    return cmp(LIR_flt);
}
bool TraceRecorder::record_JSOP_LE()
{
    return cmp(LIR_fle);
}
bool TraceRecorder::record_JSOP_GT()
{
    return cmp(LIR_fgt);
}
bool TraceRecorder::record_JSOP_GE()
{
    return cmp(LIR_fge);
}
bool TraceRecorder::record_JSOP_LSH()
{
    return binary(LIR_lsh);
}
bool TraceRecorder::record_JSOP_RSH()
{
    return binary(LIR_rsh);
}
bool TraceRecorder::record_JSOP_URSH()
{
    return binary(LIR_ush);
}
bool TraceRecorder::record_JSOP_ADD()
{
    return binary(LIR_fadd);
}
bool TraceRecorder::record_JSOP_SUB()
{
    return binary(LIR_fsub);
}
bool TraceRecorder::record_JSOP_MUL()
{
    return binary(LIR_fmul);
}
bool TraceRecorder::record_JSOP_DIV()
{
    return binary(LIR_fdiv);
}
bool TraceRecorder::record_JSOP_MOD()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (isNumber(l) && isNumber(r)) {
        LIns* args[] = { get(&r), get(&l) };
        set(&l, lir->insCall(F_dmod, args));
        return true;
    }
    return false;
}
bool TraceRecorder::record_JSOP_NOT()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_BOOLEAN(v)) {
        set(&v, lir->ins_eq0(get(&v)));
        return true;
    }
    return false;
}
bool TraceRecorder::record_JSOP_BITNOT()
{
    return unary(LIR_not);
}
bool TraceRecorder::record_JSOP_NEG()
{
    return unary(LIR_fneg);
}
bool TraceRecorder::record_JSOP_NEW()
{
    return false;
}
bool TraceRecorder::record_JSOP_DELNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_DELPROP()
{
    return false;
}
bool TraceRecorder::record_JSOP_DELELEM()
{
    return false;
}
bool TraceRecorder::record_JSOP_TYPEOF()
{
    return false;
}
bool TraceRecorder::record_JSOP_VOID()
{
    stack(0, lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID)));
    return true;
}
bool TraceRecorder::record_JSOP_INCNAME()
{
    return false;
}

bool TraceRecorder::record_JSOP_INCPROP()
{
    return incProp(1);
}

bool TraceRecorder::record_JSOP_INCELEM()
{
    return incElem(1);
}

bool TraceRecorder::record_JSOP_DECNAME()
{
    return false;
}

bool TraceRecorder::record_JSOP_DECPROP()
{
    return incProp(-1);
}

bool TraceRecorder::record_JSOP_DECELEM()
{
    return incElem(-1);
}

bool TraceRecorder::record_JSOP_NAMEINC()
{
    return false;
}

bool TraceRecorder::record_JSOP_PROPINC()
{
    return incProp(1, false);
}

// XXX consolidate with record_JSOP_GETELEM code...
bool TraceRecorder::record_JSOP_ELEMINC()
{
    return incElem(1, false);
}

bool TraceRecorder::record_JSOP_NAMEDEC()
{
    return false;
}

bool TraceRecorder::record_JSOP_PROPDEC()
{
    return incProp(-1, false);
}

bool TraceRecorder::record_JSOP_ELEMDEC()
{
    return incElem(-1, false);
}

bool TraceRecorder::record_JSOP_GETPROP()
{
    return getProp(stackval(-1));
}

bool TraceRecorder::record_JSOP_SETPROP()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("primitive this for SETPROP");

    JSObject* obj = JSVAL_TO_OBJECT(l);

    if (obj->map->ops->setProperty != js_SetProperty)
        ABORT_TRACE("non-native setProperty");

    LIns* obj_ins = get(&l);

    JSPropertyCache* cache = &JS_PROPERTY_CACHE(cx);
    uint32 kshape = OBJ_SCOPE(obj)->shape;
    jsbytecode* pc = cx->fp->regs->pc;

    JSPropCacheEntry* entry = &cache->table[PROPERTY_CACHE_HASH_PC(pc, kshape)];
    if (entry->kpc != pc || entry->kshape != kshape)
        ABORT_TRACE("cache miss");

    /* XXX share with test_property_cache */
    LIns* map_ins = lir->insLoadi(obj_ins, offsetof(JSObject, map));
    if (!map_is_native(obj->map, map_ins))
        return false;

    /* XXX share with test_property_cache */
    if (obj != globalObj) { // global object's shape is guarded at trace entry
        LIns* shape_ins = addName(lir->insLoadi(map_ins, offsetof(JSScope, shape)), "shape");
        guard(true, addName(lir->ins2i(LIR_eq, shape_ins, kshape), "guard(shape)"));
    }

    JSScope* scope = OBJ_SCOPE(obj);
    if (scope->object != obj)
        ABORT_TRACE("not scope owner");

    JSScopeProperty* sprop = PCVAL_TO_SPROP(entry->vword);
    if (!SCOPE_HAS_PROPERTY(scope, sprop))
        ABORT_TRACE("sprop not in scope");

    LIns* dslots_ins = NULL;
    LIns* v_ins = get(&r);
    LIns* boxed_ins = v_ins;
    if (!box_jsval(r, boxed_ins))
        return false;
    if (!native_set(obj_ins, sprop, dslots_ins, boxed_ins))
        return false;
    if (pc[JSOP_SETPROP_LENGTH] != JSOP_POP)
        stack(-2, v_ins);
    return true;
}

bool TraceRecorder::record_JSOP_GETELEM()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    jsval* vp;
    LIns* v_ins;
    LIns* addr_ins;
    if (!elem(l, r, vp, v_ins, addr_ins))
        return false;
    set(&l, v_ins);
    return true;
}

bool TraceRecorder::record_JSOP_SETELEM()
{
    jsval& v = stackval(-1);
    jsval& r = stackval(-2);
    jsval& l = stackval(-3);

    /* no guards for type checks, trace specialized this already */
    if (!JSVAL_IS_INT(r) || JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("not array[int]");
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);

    /* make sure the object is actually a dense array */
    LIns* dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    if (!guardThatObjectIsDenseArray(obj, obj_ins, dslots_ins))
        ABORT_TRACE("not a dense array");

    /* check that the index is within bounds */
    jsint idx = JSVAL_TO_INT(r);
    LIns* idx_ins = f2i(get(&r));

    /* we have to check that its really an integer, but this check will to go away
       once we peel the loop type down to integer for this slot */
    guard(true, lir->ins2(LIR_feq, get(&r), lir->ins1(LIR_i2f, idx_ins)));
    if (!guardDenseArrayIndexWithinBounds(obj, idx, obj_ins, dslots_ins, idx_ins))
        ABORT_TRACE("index out of bounds");

    /* get us the address of the array slot */
    LIns* addr = lir->ins2(LIR_add, dslots_ins,
                           lir->ins2i(LIR_lsh, idx_ins, JS_BYTES_PER_WORD_LOG2));
    LIns* oldval = lir->insLoad(LIR_ld, addr, 0);
    LIns* isHole = lir->ins2(LIR_eq, oldval, lir->insImmPtr((void*)JSVAL_HOLE));
    LIns* count = lir->insLoadi(obj_ins,
                                offsetof(JSObject, fslots[JSSLOT_ARRAY_COUNT]));
    lir->insStorei(lir->ins2(LIR_add, count, isHole), obj_ins,
                   offsetof(JSObject, fslots[JSSLOT_ARRAY_COUNT]));

    /* ok, box the value we are storing, store it and we are done */
    LIns* v_ins = get(&v);
    LIns* boxed_ins = v_ins;
    if (!box_jsval(v, boxed_ins))
        return false;
    lir->insStorei(boxed_ins, addr, 0);
    set(&l, v_ins);
    return true;
}

bool TraceRecorder::record_JSOP_CALLNAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj)
        ABORT_TRACE("fp->scopeChain is not global object");

    LIns* obj_ins = lir->insLoadi(lir->insLoadi(cx_ins, offsetof(JSContext, fp)),
                                  offsetof(JSStackFrame, scopeChain));
    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        ABORT_TRACE("missed prop");

    if (!PCVAL_IS_OBJECT(pcval))
        ABORT_TRACE("PCE not object");

    stack(0, lir->insImmPtr(PCVAL_TO_OBJECT(pcval)));
    stack(1, obj_ins);
    return true;
}

JSBool
js_math_sin(JSContext *cx, uintN argc, jsval *vp);

JSBool
js_math_cos(JSContext *cx, uintN argc, jsval *vp);

JSBool
js_math_pow(JSContext *cx, uintN argc, jsval *vp);

JSBool
js_math_sqrt(JSContext *cx, uintN argc, jsval *vp);

bool TraceRecorder::record_JSOP_CALL()
{
    uintN argc = GET_ARGC(cx->fp->regs->pc);
    jsval& fval = stackval(-(argc + 2));

    if (!VALUE_IS_FUNCTION(cx, fval))
        ABORT_TRACE("CALL on non-function");

    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(fval));
    if (FUN_INTERPRETED(fun)) {
        // TODO: make sure args are not copied, or track the copying via the tracker
        if (fun->nargs != argc)
            ABORT_TRACE("can't trace function calls with arity mismatch");
        unsigned callDepth = getCallDepth();
        lir->insStorei(lir->insImmPtr(JSVAL_TO_OBJECT(fval)),
                lirbuf->rp, callDepth * sizeof(JSObject*));
        if (callDepth+1 > treeInfo->maxCallDepth)
            treeInfo->maxCallDepth = callDepth+1;
        atoms = fun->u.i.script->atomMap.vector;
        return true;
    }

    if (FUN_SLOW_NATIVE(fun))
        ABORT_TRACE("slow native");

    struct JSTraceableNative {
        JSFastNative native;
        int          builtin;
        intN         argc;
        const char  *argtypes;
    } knownNatives[] = {
        { js_math_sin,  F_Math_sin,  1, "d" },
        { js_math_cos,  F_Math_cos,  1, "d" },
        { js_math_pow,  F_Math_pow,  2, "dd" },
        { js_math_sqrt, F_Math_sqrt, 1, "d" }
    };

    for (uintN i = 0; i < JS_ARRAY_LENGTH(knownNatives); i++) {
        JSTraceableNative* known = &knownNatives[i];
        if ((JSFastNative)fun->u.n.native != known->native)
            continue;

        LIns* args[5];
        LIns** argp = &args[argc-1];
        switch (known->argc) {
          case 2:
            JS_ASSERT(known->argtypes[1] == 'd');
            if (!isNumber(stackval(-2)))
                ABORT_TRACE("2nd arg must be numeric");
            *argp = get(&stackval(-2));
            argp--;
            /* FALL THROUGH */
          case 1:
            JS_ASSERT(known->argtypes[0] == 'd');
            if (!isNumber(stackval(-1)))
                ABORT_TRACE("1st arg must be numeric");
            *argp = get(&stackval(-1));
            /* FALL THROUGH */
          case 0:
            break;
          default:
            JS_ASSERT(0 && "illegal number of args to traceable native");
        }

        set(&fval, lir->insCall(known->builtin, args));
        return true;
    }

    /* Didn't find it. */
    ABORT_TRACE("unknown native");
}

bool
TraceRecorder::record_EnterFrame()
{
    ++callDepth;
    JSStackFrame* fp = cx->fp;
    LIns* void_ins = lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID));
    set(&fp->rval, void_ins, true);
    unsigned n;
    for (n = 0; n < fp->nvars; ++n)
        set(&fp->vars[n], void_ins, true);
    return true;
}

bool
TraceRecorder::prop(JSObject* obj, LIns* obj_ins, uint32& slot, LIns*& v_ins)
{
    /*
     * Can't specialize to assert obj != global, must guard to avoid aliasing
     * stale homes of stacked global variables.
     */
    if (obj == globalObj)
        ABORT_TRACE("prop op aliases global");
    guard(false, lir->ins2(LIR_eq, obj_ins, lir->insImmPtr((void*)globalObj)));

    if (!test_property_cache_direct_slot(obj, obj_ins, slot))
        return false;

    LIns* dslots_ins = NULL;
    v_ins = stobj_get_slot(obj_ins, slot, dslots_ins);
    if (!unbox_jsval(STOBJ_GET_SLOT(obj, slot), v_ins))
        ABORT_TRACE("unboxing");
    return true;
}

bool
TraceRecorder::elem(jsval& l, jsval& r, jsval*& vp, LIns*& v_ins, LIns*& addr_ins)
{
    /* no guards for type checks, trace specialized this already */
    if (!JSVAL_IS_INT(r) || JSVAL_IS_PRIMITIVE(l))
        return false;

    /*
     * Can't specialize to assert obj != global, must guard to avoid aliasing
     * stale homes of stacked global variables.
     */
    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj == globalObj)
        ABORT_TRACE("elem op aliases global");
    LIns* obj_ins = get(&l);
    guard(false, lir->ins2(LIR_eq, obj_ins, lir->insImmPtr((void*)globalObj)));

    /* make sure the object is actually a dense array */
    LIns* dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    if (!guardThatObjectIsDenseArray(obj, obj_ins, dslots_ins))
        return false;

    /* check that the index is within bounds */
    jsint idx = JSVAL_TO_INT(r);
    LIns* idx_ins = f2i(get(&r));

    /* we have to check that its really an integer, but this check will to go away
       once we peel the loop type down to integer for this slot */
    guard(true, lir->ins2(LIR_feq, get(&r), lir->ins1(LIR_i2f, idx_ins)));
    if (!guardDenseArrayIndexWithinBounds(obj, idx, obj_ins, dslots_ins, idx_ins))
        return false;
    vp = &obj->dslots[idx];

    addr_ins = lir->ins2(LIR_add, dslots_ins,
                         lir->ins2i(LIR_lsh, idx_ins, sizeof(jsval) == 4 ? 2 : 3));
    /* load the value, check the type (need to check JSVAL_HOLE only for booleans) */
    v_ins = lir->insLoad(LIR_ld, addr_ins, 0);
    return unbox_jsval(*vp, v_ins);
}

bool
TraceRecorder::getProp(JSObject* obj, LIns* obj_ins)
{
    uint32 slot;
    LIns* v_ins;
    if (!prop(obj, obj_ins, slot, v_ins))
        return false;

    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    JS_ASSERT(cs.ndefs == 1);
    stack(-cs.nuses, v_ins);
    return true;
}

bool
TraceRecorder::getProp(jsval& v)
{
    if (JSVAL_IS_PRIMITIVE(v))
        ABORT_TRACE("primitive lhs");

    return getProp(JSVAL_TO_OBJECT(v), get(&v));
}

bool TraceRecorder::record_JSOP_NAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj)
        return false;

    LIns* obj_ins = lir->insLoadi(lir->insLoadi(cx_ins, offsetof(JSContext, fp)),
                                  offsetof(JSStackFrame, scopeChain));

    /* Can't use getProp here, because we don't want unboxing. */
    uint32 slot;
    if (!test_property_cache_direct_slot(obj, obj_ins, slot))
        return false;

    if (!tracker.has(&STOBJ_GET_SLOT(obj, slot)))
        ABORT_TRACE("JSOP_NAME on non-interned global: save us, upvar!");

    stack(0, get(&STOBJ_GET_SLOT(obj, slot)));
    return true;
}

bool TraceRecorder::record_JSOP_DOUBLE()
{
    jsval v = jsval(atoms[GET_INDEX(cx->fp->regs->pc)]);
    jsdpun u;
    u.d = *JSVAL_TO_DOUBLE(v);
    stack(0, lir->insImmq(u.u64));
    return true;
}
bool TraceRecorder::record_JSOP_STRING()
{
    return false;
}
bool TraceRecorder::record_JSOP_ZERO()
{
    jsdpun u;
    u.d = 0.0;
    stack(0, lir->insImmq(u.u64));
    return true;
}
bool TraceRecorder::record_JSOP_ONE()
{
    jsdpun u;
    u.d = 1.0;
    stack(0, lir->insImmq(u.u64));
    return true;
}
bool TraceRecorder::record_JSOP_NULL()
{
    stack(0, lir->insImmPtr(NULL));
    return true;
}
bool TraceRecorder::record_JSOP_THIS()
{
    LIns* this_ins;
    if (!getThis(this_ins))
        return false;
    stack(0, this_ins);
    return true;
}
bool TraceRecorder::record_JSOP_FALSE()
{
    stack(0, lir->insImm(0));
    return true;
}
bool TraceRecorder::record_JSOP_TRUE()
{
    stack(0, lir->insImm(1));
    return true;
}
bool TraceRecorder::record_JSOP_OR()
{
    return false;
}
bool TraceRecorder::record_JSOP_AND()
{
    return false;
}
bool TraceRecorder::record_JSOP_TABLESWITCH()
{
    return false;
}
bool TraceRecorder::record_JSOP_LOOKUPSWITCH()
{
    return false;
}
bool TraceRecorder::record_JSOP_STRICTEQ()
{
    return false;
}
bool TraceRecorder::record_JSOP_STRICTNE()
{
    return false;
}
bool TraceRecorder::record_JSOP_CLOSURE()
{
    return false;
}
bool TraceRecorder::record_JSOP_EXPORTALL()
{
    return false;
}
bool TraceRecorder::record_JSOP_EXPORTNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_IMPORTALL()
{
    return false;
}
bool TraceRecorder::record_JSOP_IMPORTPROP()
{
    return false;
}
bool TraceRecorder::record_JSOP_IMPORTELEM()
{
    return false;
}
bool TraceRecorder::record_JSOP_OBJECT()
{
    return false;
}
bool TraceRecorder::record_JSOP_POP()
{
    return true;
}
bool TraceRecorder::record_JSOP_POS()
{
    return false;
}
bool TraceRecorder::record_JSOP_TRAP()
{
    return false;
}
bool TraceRecorder::record_JSOP_GETARG()
{
    stack(0, arg(GET_ARGNO(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::record_JSOP_SETARG()
{
    arg(GET_ARGNO(cx->fp->regs->pc), stack(-1));
    return true;
}
bool TraceRecorder::record_JSOP_GETVAR()
{
    stack(0, var(GET_VARNO(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::record_JSOP_SETVAR()
{
    var(GET_VARNO(cx->fp->regs->pc), stack(-1));
    return true;
}
bool TraceRecorder::record_JSOP_UINT16()
{
    jsdpun u;
    u.d = (jsdouble)GET_UINT16(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}
bool TraceRecorder::record_JSOP_NEWINIT()
{
    return false;
}
bool TraceRecorder::record_JSOP_ENDINIT()
{
    return false;
}
bool TraceRecorder::record_JSOP_INITPROP()
{
    return false;
}
bool TraceRecorder::record_JSOP_INITELEM()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEFSHARP()
{
    return false;
}
bool TraceRecorder::record_JSOP_USESHARP()
{
    return false;
}
bool TraceRecorder::record_JSOP_INCARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1);
}
bool TraceRecorder::record_JSOP_INCVAR()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), 1);
}
bool TraceRecorder::record_JSOP_DECARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1);
}
bool TraceRecorder::record_JSOP_DECVAR()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), -1);
}
bool TraceRecorder::record_JSOP_ARGINC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1, false);
}
bool TraceRecorder::record_JSOP_VARINC()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), 1, false);
}
bool TraceRecorder::record_JSOP_ARGDEC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1, false);
}
bool TraceRecorder::record_JSOP_VARDEC()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), -1, false);
}
bool TraceRecorder::record_JSOP_ITER()
{
    return false;
}
bool TraceRecorder::record_JSOP_FORNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_FORPROP()
{
    return false;
}
bool TraceRecorder::record_JSOP_FORELEM()
{
    return false;
}
bool TraceRecorder::record_JSOP_POPN()
{
    return true;
}
bool TraceRecorder::record_JSOP_BINDNAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj)
        return false;

    LIns* obj_ins = lir->insLoadi(lir->insLoadi(cx_ins, offsetof(JSContext, fp)),
                                  offsetof(JSStackFrame, scopeChain));
    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;

    stack(0, obj_ins);
    return true;
}

bool TraceRecorder::record_JSOP_SETNAME()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (JSVAL_IS_PRIMITIVE(l))
        return false;

    /*
     * Trace cases that are global code or in lightweight functions scoped by
     * the global object only.
     */
    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj != cx->fp->scopeChain || obj != globalObj)
        return false;

    LIns* obj_ins = get(&l);
    uint32 slot;
    if (!test_property_cache_direct_slot(obj, obj_ins, slot))
        return false;

    LIns* r_ins = get(&r);
    set(&STOBJ_GET_SLOT(obj, slot), r_ins);

    if (cx->fp->regs->pc[JSOP_SETNAME_LENGTH] != JSOP_POP)
        stack(-2, r_ins);
    return true;
}

bool TraceRecorder::record_JSOP_THROW()
{
    return false;
}
bool TraceRecorder::record_JSOP_IN()
{
    return false;
}
bool TraceRecorder::record_JSOP_INSTANCEOF()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEBUGGER()
{
    return false;
}
bool TraceRecorder::record_JSOP_GOSUB()
{
    return false;
}
bool TraceRecorder::record_JSOP_RETSUB()
{
    return false;
}
bool TraceRecorder::record_JSOP_EXCEPTION()
{
    return false;
}
bool TraceRecorder::record_JSOP_LINENO()
{
    return true;
}
bool TraceRecorder::record_JSOP_CONDSWITCH()
{
    return true;
}
bool TraceRecorder::record_JSOP_CASE()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEFAULT()
{
    return false;
}
bool TraceRecorder::record_JSOP_EVAL()
{
    return false;
}
bool TraceRecorder::record_JSOP_ENUMELEM()
{
    return false;
}
bool TraceRecorder::record_JSOP_GETTER()
{
    return false;
}
bool TraceRecorder::record_JSOP_SETTER()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEFFUN()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEFCONST()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEFVAR()
{
    return false;
}
bool TraceRecorder::record_JSOP_ANONFUNOBJ()
{
    return false;
}
bool TraceRecorder::record_JSOP_NAMEDFUNOBJ()
{
    return false;
}
bool TraceRecorder::record_JSOP_SETLOCALPOP()
{
    return false;
}
bool TraceRecorder::record_JSOP_GROUP()
{
    return true; // no-op
}
bool TraceRecorder::record_JSOP_SETCALL()
{
    return false;
}
bool TraceRecorder::record_JSOP_TRY()
{
    return true;
}
bool TraceRecorder::record_JSOP_FINALLY()
{
    return true;
}
bool TraceRecorder::record_JSOP_NOP()
{
    return true;
}
bool TraceRecorder::record_JSOP_ARGSUB()
{
    return false;
}
bool TraceRecorder::record_JSOP_ARGCNT()
{
    return false;
}

/*
 * XXX could hoist out to jsinterp.h and share with jsinterp.cpp, but
 * XXX jsopcode.cpp has different definitions of same-named macros.
 */
#define GET_FULL_INDEX(PCOFF)                                                 \
    (atoms - script->atomMap.vector + GET_INDEX(regs.pc + PCOFF))

#define LOAD_FUNCTION(PCOFF)                                                  \
    JS_GET_SCRIPT_FUNCTION(script, GET_FULL_INDEX(PCOFF), fun)

bool TraceRecorder::record_JSOP_DEFLOCALFUN()
{
    JSFunction* fun;
    JSFrameRegs& regs = *cx->fp->regs;
    JSScript* script = cx->fp->script;
    LOAD_FUNCTION(VARNO_LEN);

    JSObject* obj = FUN_OBJECT(fun);
    if (OBJ_GET_PARENT(cx, obj) != cx->fp->scopeChain)
        ABORT_TRACE("can't trace with activation object on scopeChain");

    var(GET_VARNO(regs.pc), lir->insImmPtr(obj));
    return true;
}

bool TraceRecorder::record_JSOP_GOTOX()
{
    return false;
}
bool TraceRecorder::record_JSOP_IFEQX()
{
    return record_JSOP_IFEQ();
}
bool TraceRecorder::record_JSOP_IFNEX()
{
    return record_JSOP_IFNE();
}
bool TraceRecorder::record_JSOP_ORX()
{
    return record_JSOP_OR();
}
bool TraceRecorder::record_JSOP_ANDX()
{
    return record_JSOP_AND();
}
bool TraceRecorder::record_JSOP_GOSUBX()
{
    return record_JSOP_GOSUB();
}
bool TraceRecorder::record_JSOP_CASEX()
{
    return record_JSOP_CASE();
}
bool TraceRecorder::record_JSOP_DEFAULTX()
{
    return record_JSOP_DEFAULT();
}
bool TraceRecorder::record_JSOP_TABLESWITCHX()
{
    return record_JSOP_TABLESWITCH();
}
bool TraceRecorder::record_JSOP_LOOKUPSWITCHX()
{
    return record_JSOP_LOOKUPSWITCH();
}
bool TraceRecorder::record_JSOP_BACKPATCH()
{
    return true;
}
bool TraceRecorder::record_JSOP_BACKPATCH_POP()
{
    return true;
}
bool TraceRecorder::record_JSOP_THROWING()
{
    return false;
}
bool TraceRecorder::record_JSOP_SETRVAL()
{
    return false;
}
bool TraceRecorder::record_JSOP_RETRVAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_GETGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);
    stack(0, get(&STOBJ_GET_SLOT(cx->fp->scopeChain, slot)));
    return true;
}

bool TraceRecorder::record_JSOP_SETGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);
    set(&STOBJ_GET_SLOT(cx->fp->scopeChain, slot), stack(-1));
    return true;
}

bool TraceRecorder::record_JSOP_INCGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), 1);
}

bool TraceRecorder::record_JSOP_DECGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), -1);
}

bool TraceRecorder::record_JSOP_GVARINC()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), 1, false);
}

bool TraceRecorder::record_JSOP_GVARDEC()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), -1, false);
}

bool TraceRecorder::record_JSOP_REGEXP()
{
    return false;
}
bool TraceRecorder::record_JSOP_DEFXMLNS()
{
    return false;
}
bool TraceRecorder::record_JSOP_ANYNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_QNAMEPART()
{
    return false;
}
bool TraceRecorder::record_JSOP_QNAMECONST()
{
    return false;
}
bool TraceRecorder::record_JSOP_QNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_TOATTRNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_TOATTRVAL()
{
    return false;
}
bool TraceRecorder::record_JSOP_ADDATTRNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_ADDATTRVAL()
{
    return false;
}
bool TraceRecorder::record_JSOP_BINDXMLNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_SETXMLNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLNAME()
{
    return false;
}
bool TraceRecorder::record_JSOP_DESCENDANTS()
{
    return false;
}
bool TraceRecorder::record_JSOP_FILTER()
{
    return false;
}
bool TraceRecorder::record_JSOP_ENDFILTER()
{
    return false;
}
bool TraceRecorder::record_JSOP_TOXML()
{
    return false;
}
bool TraceRecorder::record_JSOP_TOXMLLIST()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLTAGEXPR()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLELTEXPR()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLOBJECT()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLCDATA()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLCOMMENT()
{
    return false;
}
bool TraceRecorder::record_JSOP_XMLPI()
{
    return false;
}

bool TraceRecorder::record_JSOP_CALLPROP()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("CALLPROP on primitive");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        ABORT_TRACE("missed prop");

    if (!PCVAL_IS_OBJECT(pcval))
        ABORT_TRACE("PCE not object");

    stack(-1, lir->insImmPtr(PCVAL_TO_OBJECT(pcval)));
    stack(0, obj_ins);
    return true;
}

bool TraceRecorder::record_JSOP_GETFUNNS()
{
    return false;
}
bool TraceRecorder::record_JSOP_UNUSED186()
{
    return false;
}
bool TraceRecorder::record_JSOP_DELDESC()
{
    return false;
}
bool TraceRecorder::record_JSOP_UINT24()
{
    jsdpun u;
    u.d = (jsdouble)GET_UINT24(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}
bool TraceRecorder::record_JSOP_INDEXBASE()
{
    atoms += GET_INDEXBASE(cx->fp->regs->pc);
    return true;
}

bool TraceRecorder::record_JSOP_RESETBASE()
{
    atoms = cx->fp->script->atomMap.vector;
    return true;
}

bool TraceRecorder::record_JSOP_RESETBASE0()
{
    atoms = cx->fp->script->atomMap.vector;
    return true;
}

bool TraceRecorder::record_JSOP_STARTXML()
{
    return false;
}

bool TraceRecorder::record_JSOP_STARTXMLEXPR()
{
    return false;
}

bool TraceRecorder::record_JSOP_CALLELEM()
{
    return false;
}

bool TraceRecorder::record_JSOP_STOP()
{
    return (callDepth-- > 0);
}

bool TraceRecorder::record_JSOP_GETXPROP()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("primitive-this for GETXPROP?");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    JS_ASSERT(obj == cx->fp->scopeChain);
    LIns* obj_ins = get(&l);

    /* Can't use get_prop here, because we don't want unboxing. */
    uint32 slot;
    if (!test_property_cache_direct_slot(obj, obj_ins, slot))
        return false;

    stack(-1, get(&STOBJ_GET_SLOT(obj, slot)));
    return true;
}

bool TraceRecorder::record_JSOP_CALLXMLNAME()
{
    return false;
}

bool TraceRecorder::record_JSOP_TYPEOFEXPR()
{
    return false;
}

bool TraceRecorder::record_JSOP_ENTERBLOCK()
{
    return false;
}

bool TraceRecorder::record_JSOP_LEAVEBLOCK()
{
    return false;
}

bool TraceRecorder::record_JSOP_GETLOCAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_SETLOCAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_INCLOCAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_DECLOCAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_LOCALINC()
{
    return false;
}

bool TraceRecorder::record_JSOP_LOCALDEC()
{
    return false;
}

bool TraceRecorder::record_JSOP_FORLOCAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_FORCONST()
{
    return false;
}

bool TraceRecorder::record_JSOP_ENDITER()
{
    return false;
}

bool TraceRecorder::record_JSOP_GENERATOR()
{
    return false;
}

bool TraceRecorder::record_JSOP_YIELD()
{
    return false;
}

bool TraceRecorder::record_JSOP_ARRAYPUSH()
{
    return false;
}

bool TraceRecorder::record_JSOP_UNUSED213()
{
    return false;
}

bool TraceRecorder::record_JSOP_ENUMCONSTELEM()
{
    return false;
}

bool TraceRecorder::record_JSOP_LEAVEBLOCKEXPR()
{
    return false;
}

bool TraceRecorder::record_JSOP_GETTHISPROP()
{
    LIns* this_ins;

    /* its safe to just use cx->fp->thisp here because getThis() returns false if thisp
       is not available */
    return getThis(this_ins) && getProp(cx->fp->thisp, this_ins);
}

bool TraceRecorder::record_JSOP_GETARGPROP()
{
    return getProp(argval(GET_ARGNO(cx->fp->regs->pc)));
}

bool TraceRecorder::record_JSOP_GETVARPROP()
{
    return getProp(varval(GET_VARNO(cx->fp->regs->pc)));
}

bool TraceRecorder::record_JSOP_GETLOCALPROP()
{
    return false;
}

bool TraceRecorder::record_JSOP_INDEXBASE1()
{
    atoms += 1 << 16;
    return true;
}

bool TraceRecorder::record_JSOP_INDEXBASE2()
{
    atoms += 2 << 16;
    return true;
}

bool TraceRecorder::record_JSOP_INDEXBASE3()
{
    atoms += 3 << 16;
    return true;
}

bool TraceRecorder::record_JSOP_CALLGVAR()
{
    return record_JSOP_GETGVAR();
}

bool TraceRecorder::record_JSOP_CALLVAR()
{
    stack(1, lir->insImmPtr(0));
    return record_JSOP_GETVAR();
}

bool TraceRecorder::record_JSOP_CALLARG()
{
    stack(1, lir->insImmPtr(0));
    return record_JSOP_GETARG();
}

bool TraceRecorder::record_JSOP_CALLLOCAL()
{
    return false;
}

bool TraceRecorder::record_JSOP_INT8()
{
    jsdpun u;
    u.d = (jsdouble)GET_INT8(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool TraceRecorder::record_JSOP_INT32()
{
    jsdpun u;
    u.d = (jsdouble)GET_INT32(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool TraceRecorder::record_JSOP_LENGTH()
{
    jsval& l = stackval(-1);
    JSObject *obj;
    if (JSVAL_IS_PRIMITIVE(l) || !OBJ_IS_DENSE_ARRAY(cx, (obj = JSVAL_TO_OBJECT(l))))
        ABORT_TRACE("only dense arrays supported");
    LIns* dslots_ins;
    if (!guardThatObjectIsDenseArray(obj, get(&l), dslots_ins))
        ABORT_TRACE("OBJ_IS_DENSE_ARRAY but not?!?");
    LIns* v_ins = lir->ins1(LIR_i2f, stobj_get_slot(get(&l), JSSLOT_ARRAY_LENGTH, dslots_ins));
    set(&l, v_ins);
    return true;
}

bool TraceRecorder::record_JSOP_NEWARRAY()
{
    return false;
}

bool TraceRecorder::record_JSOP_HOLE()
{
    stack(0, lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_HOLE)));
    return true;
}
