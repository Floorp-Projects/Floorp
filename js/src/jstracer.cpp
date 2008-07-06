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

#include <math.h>

#include "nanojit/avmplus.h"
#include "nanojit/nanojit.h"
#include "jsarray.h"
#include "jsbool.h"
#include "jstracer.h"
#include "jscntxt.h"
#include "jsscript.h"
#include "jsprf.h"
#include "jsinterp.h"
#include "jsscope.h"

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
        GC::Alloc(sizeof(struct Tracker::Page) + (NJ_PAGE_SIZE >> 2) * sizeof(LInsp));
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

LIns*
Tracker::get(const void* v) const
{
    struct Tracker::Page* p = findPage(v);
    JS_ASSERT(p != 0); /* we must have a page for the slot we are looking for */
    LIns* i = p->map[(jsuword(v) & 0xfff) >> 2];
    JS_ASSERT(i != 0);
    return i;
}

void
Tracker::set(const void* v, LIns* ins)
{
    struct Tracker::Page* p = findPage(v);
    if (!p)
        p = addPage(v);
    p->map[(jsuword(v) & 0xfff) >> 2] = ins;
}

#define LO ARGSIZE_LO
#define F  ARGSIZE_F
#define Q  ARGSIZE_Q

#ifdef DEBUG
#define NAME(op) ,#op
#else
#define NAME(op)
#endif

#define BUILTIN1(op, at0, atr, tr, t0, cse, fold) \
    { (intptr_t)&builtin_##op, (at0 << 2) | atr, cse, fold NAME(op) },
#define BUILTIN2(op, at0, at1, atr, tr, t0, t1, cse, fold) \
    { (intptr_t)&builtin_##op, (at0 << 4) | (at1 << 2) | atr, cse, fold NAME(op) },
#define BUILTIN3(op, at0, at1, at2, atr, tr, t0, t1, t2, cse, fold) \
    { (intptr_t)&builtin_##op, (at0 << 6) | (at1 << 4) | (at2 << 2) | atr, cse, fold NAME(op) },

static struct CallInfo builtins[] = {
#include "builtins.tbl"
};

#undef NAME
#undef BUILTIN1
#undef BUILTIN2
#undef BUILTIN3

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

static bool isPromoteInt(LIns *i)
{
    return i->isop(LIR_i2f);
}

static bool isPromoteUint(LIns *i)
{
    return i->isop(LIR_u2f);
}

static bool isPromote(LIns *i)
{
    return isPromoteInt(i) || isPromoteUint(i);;
}

class FuncFilter: public LirWriter
{
public:
    FuncFilter(LirWriter *out):
        LirWriter(out)
    {
    }

    LInsp ins2(LOpcode v, LInsp s1, LInsp s0)
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
        } else if (v >= LIR_feq && v <= LIR_fge) {
            if (isPromoteInt(s0) && isPromoteInt(s1)) {
                // demote fcmp to cmp
                v = LOpcode(v + (LIR_eq - LIR_feq));
                return out->ins2(v, demote(out, s1), demote(out, s0));
            } else if (isPromoteUint(s0) && isPromoteUint(s1)) {
                // uint compare
                v = LOpcode(v + (LIR_eq - LIR_feq));
                if (v != LIR_eq)
                    v = LOpcode(v + (LIR_ult - LIR_lt)); // cmp -> ucmp
                return out->ins2(v, demote(out, s1), demote(out, s0));
            }
        }
        return out->ins2(v, s1, s0);
    }

    LInsp insCall(int32_t fid, LInsp args[])
    {
        if (fid == F_DoubleToECMAInt32) {
            LInsp s0 = args[0];
            if (s0->isop(LIR_fadd) || s0->isop(LIR_fsub) || s0->isop(LIR_fmul)) {
                LInsp lhs = s0->oprnd1();
                LInsp rhs = s0->oprnd2();
                if (isPromote(lhs) && isPromote(rhs)) {
                    LOpcode op = LOpcode(s0->opcode() & ~LIR64);
                    return out->ins2(op, demote(out, lhs), demote(out, rhs));
                }
            }
        }
        return out->insCall(fid, args);
    }
};


static void
buildTypeMap(JSStackFrame* entryFrame, JSStackFrame* fp, JSFrameRegs& regs, char* m);

TraceRecorder::TraceRecorder(JSContext* cx, Fragmento* fragmento, Fragment* _fragment)
{
    this->cx = cx;
    this->fragment = _fragment;
    entryFrame = cx->fp;
    entryRegs.pc = entryFrame->regs->pc;
    entryRegs.sp = entryFrame->regs->sp;

#ifdef DEBUG    
    printf("entryRegs.pc=%p opcode=%d\n", entryRegs.pc, *entryRegs.pc);
#endif
    
    fragment->calldepth = 0;
    lirbuf = new (&gc) LirBuffer(fragmento, builtins);
    fragment->lirbuf = lirbuf;
    lir = lir_buf_writer = new (&gc) LirBufWriter(lirbuf);
#ifdef DEBUG
    lirbuf->names = new (&gc) LirNameMap(&gc, builtins, fragmento->labels);
    lir = verbose_filter = new (&gc) VerboseWriter(&gc, lir, lirbuf->names);
#endif
    lir = cse_filter = new (&gc) CseFilter(lir, &gc);
    lir = expr_filter = new (&gc) ExprFilter(lir);
    lir->ins0(LIR_trace);
    /* generate the entry map and stash it in the trace */
    entryNativeFrameSlots = nativeFrameSlots(entryFrame, entryRegs);
    maxNativeFrameSlots = entryNativeFrameSlots;
    LIns* data = lir_buf_writer->skip(sizeof(VMFragmentInfo) + 
            entryNativeFrameSlots * sizeof(char));
    fragmentInfo = (VMFragmentInfo*)data->payload();
    buildTypeMap(entryFrame, entryFrame, entryRegs, fragmentInfo->typeMap);
    fragmentInfo->nativeStackBase = nativeFrameOffset(&cx->fp->spbase[0]);
    fragment->vmprivate = fragmentInfo;
    fragment->param0 = lir->insImm8(LIR_param, Assembler::argRegs[0], 0);
    fragment->param1 = lir->insImm8(LIR_param, Assembler::argRegs[1], 0);
    fragment->sp = lir->insLoadi(fragment->param0, offsetof(InterpState, sp));
    cx_ins = lir->insLoadi(fragment->param0, offsetof(InterpState, cx));
#ifdef DEBUG
    lirbuf->names->addName(fragment->param0, "state");
    lirbuf->names->addName(fragment->sp, "sp");
    lirbuf->names->addName(cx_ins, "cx");
#endif

    JSStackFrame* fp = cx->fp;
    unsigned n;
    for (n = 0; n < fp->argc; ++n)
        import(&fp->argv[n], "arg", n);
    for (n = 0; n < fp->nvars; ++n)
        import(&fp->vars[n], "var", n);
    for (n = 0; n < unsigned(fp->regs->sp - fp->spbase); ++n)
        import(&fp->spbase[n], "stack", n);
}

TraceRecorder::~TraceRecorder()
{
#ifdef DEBUG
    delete lirbuf->names;
    delete verbose_filter;
#endif
    delete cse_filter;
    delete expr_filter;
    delete lir_buf_writer;
}

/* Determine the current call depth (starting with the entry frame.) */
unsigned
TraceRecorder::getCallDepth() const
{
    JSStackFrame* fp = cx->fp;
    unsigned depth = 0;
    while (fp != entryFrame) {
        ++depth;
        fp = fp->down;
    }
    return depth;
}

/* Find the frame that this address belongs to (if any). */
JSStackFrame*
TraceRecorder::findFrame(void* p) const
{
    jsval* vp = (jsval*) p;
    JSStackFrame* fp = cx->fp;
    for (;;) {
        // FIXME: fixing bug 441686 collapses the last two tests here
        if (size_t(vp - fp->argv) < fp->argc ||
            size_t(vp - fp->vars) < fp->nvars ||
            size_t(vp - fp->spbase) < fp->script->depth) {
            return fp;
        }
        if (fp == entryFrame)
           return NULL;
        fp = fp->down;
    }
    JS_NOT_REACHED("findFrame");
}

/* Determine whether an address is part of a currently active frame. */
bool
TraceRecorder::onFrame(void* p) const
{
    return findFrame(p) != NULL;
}

/* Calculate the total number of native frame slots we need from this frame
   all the way back to the entry frame, including the current stack usage. */
unsigned
TraceRecorder::nativeFrameSlots(JSStackFrame* fp, JSFrameRegs& regs) const
{
    unsigned slots = 0;
    for (;;) {
        slots += fp->argc + fp->nvars + (regs.sp - fp->spbase);
        if (fp == entryFrame)
            return slots;
        fp = fp->down;
    }
    JS_NOT_REACHED("nativeFrameSlots");
}

/* Determine the offset in the native frame (marshal) for an address
   that is part of a currently active frame. */
size_t
TraceRecorder::nativeFrameOffset(void* p) const
{
    jsval* vp = (jsval*) p;
    JSStackFrame* fp = findFrame(p);
    JS_ASSERT(fp != NULL); // must be on the frame somewhere
    size_t offset = size_t(vp - fp->argv);
    if (offset >= fp->argc) {
        // FIXME: fixing bug 441686 collapses the vars and spbase cases
        offset = size_t(vp - fp->vars);
        if (offset >= fp->nvars) {
            JS_ASSERT(size_t(vp - fp->spbase) < fp->script->depth);
            offset = fp->nvars + size_t(vp - fp->spbase);
        }
        offset += fp->argc;
    }
    if (fp != entryFrame)
        offset += nativeFrameSlots(fp->down, *fp->regs);
    return offset * sizeof(double);
}

/* Track the maximum number of native frame slots we need during
   execution. */
void
TraceRecorder::trackNativeFrameUse(unsigned slots)
{
    if (slots > maxNativeFrameSlots)
        maxNativeFrameSlots = slots;
}

/* Return the tag of a jsval. Doubles are checked whether they actually
   represent an int, in which case we treat them as JSVAL_INT. */
static inline int getType(jsval v)
{
    if (JSVAL_IS_INT(v))
        return JSVAL_INT;
    jsint i;
    if (JSVAL_IS_DOUBLE(v) && JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(v), i))
        return JSVAL_INT;
    return JSVAL_TAG(v);
}

static inline bool isInt(jsval v)
{
    return getType(v) == JSVAL_INT;
}

static inline bool isDouble(jsval v)
{
    return (getType(v) == JSVAL_DOUBLE) && !isInt(v);
}

static inline bool isNumber(jsval v)
{
    return getType(v) == JSVAL_INT || getType(v) == JSVAL_DOUBLE;
}

static inline bool isTrueOrFalse(jsval v)
{
    return v == JSVAL_TRUE || v == JSVAL_FALSE;
}

static inline jsint asInt(jsval v)
{
    JS_ASSERT(isInt(v));
    if (JSVAL_IS_DOUBLE(v))
        return js_DoubleToECMAInt32(*JSVAL_TO_DOUBLE(v));
    return JSVAL_TO_INT(v);
}

static inline jsdouble asNumber(jsval v)
{
    JS_ASSERT(isNumber(v));
    if (JSVAL_IS_DOUBLE(v))
        return *JSVAL_TO_DOUBLE(v);
    return (jsdouble)JSVAL_TO_INT(v);
}

/* Write out a type map for the current scopes and all outer scopes,
   up until the entry scope. */
static void
buildTypeMap(JSStackFrame* entryFrame, JSStackFrame* fp, JSFrameRegs& regs, char* m)
{
    if (fp != entryFrame)
        buildTypeMap(entryFrame, fp->down, *fp->down->regs, m);
    for (unsigned n = 0; n < fp->argc; ++n)
        *m++ = getType(fp->argv[n]);
    for (unsigned n = 0; n < fp->nvars; ++n)
        *m++ = getType(fp->vars[n]);
    for (jsval* sp = fp->spbase; sp < regs.sp; ++sp)
        *m++ = getType(*sp);
}

/* Make sure that all loop-carrying values have a stable type. */
static bool
verifyTypeStability(JSStackFrame* entryFrame, JSStackFrame* fp, JSFrameRegs& regs, char* m)
{
    if (fp != entryFrame)
        verifyTypeStability(entryFrame, fp->down, *fp->down->regs, m);
    for (unsigned n = 0; n < fp->argc; ++n)
        if (*m++ != getType(fp->argv[n]))
            return false;
    for (unsigned n = 0; n < fp->nvars; ++n)
        if (*m++ != getType(fp->vars[n]))
            return false;
    for (jsval* sp = fp->spbase; sp < regs.sp; ++sp)
        if (*m++ != getType(*sp))
            return false;
    return true;
}

/* Unbox a jsval into a slot. Slots are wide enough to hold double values
   directly (instead of storing a pointer to them). */
static bool
unbox_jsval(jsval v, int t, double* slot)
{
    if (t != getType(v))
        return false;
    switch (t) {
      case JSVAL_BOOLEAN:
        *(bool*)slot = JSVAL_TO_BOOLEAN(v);
        break;
      case JSVAL_INT:
        *(jsint*)slot = asInt(v);
        break;
      case JSVAL_DOUBLE:
        *(jsdouble*)slot = *JSVAL_TO_DOUBLE(v);
        break;
      case JSVAL_STRING:
        *(JSString**)slot = JSVAL_TO_STRING(v);
        break;
      default:
        JS_ASSERT(JSVAL_IS_GCTHING(v));
        *(void**)slot = JSVAL_TO_GCTHING(v);
    }
    return true;
}

/* Box a value from the native stack back into the jsval format. Integers
   that are too large to fit into a jsval are automatically boxed into
   heap-allocated doubles. */
static bool
box_jsval(JSContext* cx, jsval* vp, int t, double* slot)
{
    switch (t) {
      case JSVAL_BOOLEAN:
        *vp = BOOLEAN_TO_JSVAL(*(bool*)slot);
        break;
      case JSVAL_INT:
        jsint i = *(jsint*)slot;
        if (INT_FITS_IN_JSVAL(i))
            *vp = INT_TO_JSVAL(i);
        else
            return js_NewDoubleInRootedValue(cx, (jsdouble)i, vp);
        break;
      case JSVAL_DOUBLE:
        return js_NewDoubleInRootedValue(cx, *slot, vp);
      case JSVAL_STRING:
        *vp = STRING_TO_JSVAL(*(JSString**)slot);
        break;
      default:
        JS_ASSERT(t == JSVAL_OBJECT);
        *vp = OBJECT_TO_JSVAL(*(JSObject**)slot);
        break;
    }
    return true;
}

/* Attempt to unbox the given JS frame into a native frame, checking
   along the way that the supplied typemap holds. */
static bool
unbox(JSStackFrame* fp, JSFrameRegs& regs, char* m, double* native)
{
    jsval* vp;
    for (vp = fp->argv; vp < fp->argv + fp->argc; ++vp)
        if (!unbox_jsval(*vp, (JSType)*m++, native++))
            return false;
    for (vp = fp->vars; vp < fp->vars + fp->nvars; ++vp)
        if (!unbox_jsval(*vp, (JSType)*m++, native++))
            return false;
    for (vp = fp->spbase; vp < regs.sp; ++vp)
        if (!unbox_jsval(*vp, (JSType)*m++, native++))
            return false;
    return true;
}

/* Attempt to unbox the given JS frame into a native frame, checking
   along the way that the supplied typemap holds. */
static bool
box(JSContext* cx, JSStackFrame* fp, JSFrameRegs& regs, char* m, double* native)
{
    jsval* vp;
    for (vp = fp->argv; vp < fp->argv + fp->argc; ++vp)
        if (!box_jsval(cx, vp, (JSType)*m++, native++))
            return false;
    for (vp = fp->vars; vp < fp->vars + fp->nvars; ++vp)
        if (!box_jsval(cx, vp, (JSType)*m++, native++))
            return false;
    for (vp = fp->spbase; vp < regs.sp; ++vp)
        if (!box_jsval(cx, vp, (JSType)*m++, native++))
            return false;
    return true;
}

/* Emit load instructions onto the trace that read the initial stack state. */
void
TraceRecorder::import(jsval* p, char *prefix, int index)
{
    JS_ASSERT(onFrame(p));
    LIns *ins = lir->insLoad(isDouble(*p) ? LIR_ldq : LIR_ld,
            fragment->sp, -fragmentInfo->nativeStackBase + nativeFrameOffset(p) + 8);
    tracker.set(p, ins);
#ifdef DEBUG
    if (prefix) {
        char name[16];
        JS_ASSERT(strlen(prefix) < 10);
        JS_snprintf(name, sizeof name, "$%s%d", prefix, index);
        lirbuf->names->addName(ins, name);
    }
#endif

}

/* Update the tracker. If the value is part of any argv/vars/stack of any
   currently active frame (onFrame), then issue a write back store. */
void
TraceRecorder::set(void* p, LIns* i)
{
    tracker.set(p, i);
    if (onFrame(p))
        lir->insStorei(i, fragment->sp, -fragmentInfo->nativeStackBase + nativeFrameOffset(p) + 8);
}

LIns*
TraceRecorder::get(void* p)
{
    return tracker.get(p);
}

SideExit*
TraceRecorder::snapshot()
{
    /* generate the entry map and stash it in the trace */
    unsigned slots = nativeFrameSlots(cx->fp, *cx->fp->regs);
    trackNativeFrameUse(slots);
    LIns* data = lir_buf_writer->skip(sizeof(VMSideExitInfo) + slots * sizeof(char));
    VMSideExitInfo* si = (VMSideExitInfo*)data->payload();
    buildTypeMap(entryFrame, cx->fp, *cx->fp->regs, si->typeMap);
    /* setup side exit structure */
    memset(&exit, 0, sizeof(exit));
#ifdef DEBUG
    exit.from = fragment;
#endif
    exit.calldepth = getCallDepth();
    exit.sp_adj = (cx->fp->regs->sp - entryRegs.sp) * sizeof(double);
    exit.ip_adj = cx->fp->regs->pc - entryRegs.pc;
    exit.vmprivate = si;
    return &exit;
}

void
TraceRecorder::guard(bool expected, LIns* cond)
{
    lir->insGuard(expected ? LIR_xf : LIR_xt,
                  cond,
                  snapshot());
}

void
TraceRecorder::closeLoop(Fragmento* fragmento)
{
    if (!verifyTypeStability(entryFrame, entryFrame, entryRegs, fragmentInfo->typeMap)) {
#ifdef DEBUG
        printf("Trace rejected: unstable loop variables.\n");
#endif
        return;
    }
    fragment->lastIns = lir->ins0(LIR_loop);
    ((VMFragmentInfo*)fragment->vmprivate)->maxNativeFrameSlots = maxNativeFrameSlots;
    compile(fragmento->assm(), fragment);
}

bool
TraceRecorder::loopEdge()
{
    if (cx->fp->regs->pc == entryRegs.pc) {
        closeLoop(JS_TRACE_MONITOR(cx).fragmento);
        return false; /* done recording */
    }
    return false; /* abort recording */
}

void
TraceRecorder::stop()
{
    fragment->blacklist();
}

void
js_DeleteRecorder(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    delete tm->recorder;
    tm->recorder = NULL;
}

#define HOTLOOP 10

bool
js_LoopEdge(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* is the recorder currently active? */
    if (tm->recorder) {
        if (tm->recorder->loopEdge())
            return true; /* keep recording */
        js_DeleteRecorder(cx);
        return false; /* done recording */
    }

    InterpState state;
    state.ip = (FOpcodep)cx->fp->regs->pc;

    Fragment* f = tm->fragmento->getLoop(state);
    if (!f->code()) {
        if (!f->isBlacklisted() && ++f->hits() > HOTLOOP) {
            tm->recorder = new (&gc) TraceRecorder(cx, tm->fragmento, f);
             return true; /* start recording */
        }
        return false;
    }

    /* execute previously recorded race */
    VMFragmentInfo* fi = (VMFragmentInfo*)f->vmprivate;
    double native[fi->maxNativeFrameSlots+1];
#ifdef DEBUG
    *(uint64*)&native[fi->maxNativeFrameSlots] = 0xdeadbeefdeadbeefLL;
#endif
    unbox(cx->fp, *cx->fp->regs, fi->typeMap, native);
    double* entry_sp = &native[fi->nativeStackBase/sizeof(double) + 
                               (cx->fp->regs->sp - cx->fp->spbase - 1)];
    state.sp = (void*)entry_sp;
    state.rp = NULL;
    state.f = NULL;
    state.cx = cx;
    union { NIns *code; GuardRecord* (FASTCALL *func)(InterpState*, Fragment*); } u;
    u.code = f->code();
#ifdef DEBUG  
    printf("entering trace, pc=%p, sp=%p\n", state.ip, state.sp);
    uint64 start = rdtsc();
#endif    
    GuardRecord* lr = u.func(&state, NULL);
#ifdef DEBUG
    printf("leaving trace, pc=%p, sp=%p, cycles=%llu\n", state.ip, state.sp, 
            (rdtsc() - start));
#endif    
    cx->fp->regs->sp += (((double*)state.sp - entry_sp));
    cx->fp->regs->pc = (jsbytecode*)state.ip;
    box(cx, cx->fp, *cx->fp->regs, ((VMSideExitInfo*)lr->vmprivate)->typeMap, native);
#ifdef DEBUG
    JS_ASSERT(*(uint64*)&native[fi->maxNativeFrameSlots] == 0xdeadbeefdeadbeefLL);
#endif

    return false; /* continue with regular interpreter */
}

void
js_AbortRecording(JSContext* cx, const char* reason)
{
#ifdef DEBUG
    printf("Abort recording: %s.\n", reason);
#endif
    JS_TRACE_MONITOR(cx).recorder->stop();
    js_DeleteRecorder(cx);
}

extern void
js_InitJIT(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (!tm->fragmento) {
        Fragmento* fragmento = new (&gc) Fragmento(core);
#ifdef DEBUG
        fragmento->labels = new (&gc) LabelMap(core, NULL);
#endif
        fragmento->assm()->setCallTable(builtins);
        tm->fragmento = fragmento;
    }
}

jsval&
TraceRecorder::argval(unsigned n) const
{
    JS_ASSERT((n >= 0) && (n <= cx->fp->argc));
    return cx->fp->argv[n];
}

jsval&
TraceRecorder::varval(unsigned n) const
{
    JS_ASSERT((n >= 0) && (n <= cx->fp->nvars));
    return cx->fp->vars[n];
}

jsval&
TraceRecorder::stackval(int n) const
{
    JS_ASSERT((cx->fp->regs->sp + n < cx->fp->spbase + cx->fp->script->depth) && 
            (cx->fp->regs->sp + n >= cx->fp->spbase));
    return cx->fp->regs->sp[n];
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
    set(&stackval(n), i);
}

LIns*
TraceRecorder::i2f(LIns* i)
{
    return lir->ins1(LIR_i2f, i);
}

LIns*
TraceRecorder::u2f(LIns* u)
{
    return lir->ins1(LIR_u2f, u);
}

LIns*
TraceRecorder::f2i(LIns* f)
{
    return lir->insCall(F_DoubleToECMAInt32, &f);
}

LIns*
TraceRecorder::f2u(LIns* f)
{
    return lir->insCall(F_DoubleToECMAUint32, &f);
}

bool
TraceRecorder::inc(jsval& v, jsint incr, bool pre)
{
    if (isInt(v)) {
        LIns* before = get(&v);
        LIns* after = lir->ins2i(LIR_add, before, incr);
        guard(false, lir->ins1(LIR_ov, after));
        set(&v, after);
        stack(0, pre ? after : before);
        return true;
    }
    return false;
}

bool
TraceRecorder::cmp(LOpcode op, bool negate)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (isInt(l) && isInt(r)) {
        LIns* x = lir->ins2(op, get(&l), get(&r));
        if (negate)
            x = lir->ins2i(LIR_eq, x, 0);
        bool cond;
        switch (op) {
          case LIR_lt:
            cond = asInt(l) < asInt(r);
            break;
          case LIR_gt:
            cond = asInt(l) > asInt(r);
            break;
          case LIR_le:
            cond = asInt(l) <= asInt(r);
            break;
          case LIR_ge:
            cond = asInt(l) >= asInt(r);
            break;
          default:
            JS_ASSERT(cond == LIR_eq);
            cond = asInt(l) == asInt(r);
            break;
        }
        /* The interpreter fuses comparisons and the following branch,
           so we have to do that here as well. */
        if (cx->fp->regs->pc[1] == ::JSOP_IFEQ)
            guard(!cond, x);
        else if (cx->fp->regs->pc[1] == ::JSOP_IFNE)
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
TraceRecorder::iunary(LOpcode op)
{
    jsval& v = stackval(-1);
    if (isNumber(v)) {
        set(&v, lir->ins1(op, get(&v)));
        return true;
    }
    return false;
}

bool
TraceRecorder::ibinary(LOpcode op)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (isInt(l) && isInt(r)) {
        set(&l, lir->ins2(op, get(&l), get(&r)));
        return true;
    }
    return false;
}

bool
TraceRecorder::bbinary(LOpcode op)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (isTrueOrFalse(l) && isTrueOrFalse(r)) {
        LIns* result = lir->ins2(op, get(&l), get(&r));
        set(&l, result);
        return true;
    }
    return false;
}

bool
TraceRecorder::map_is_native(JSObjectMap* map, 
        LIns* map_ins)
{
    LIns* ops = lir->insLoadi(map_ins, offsetof(JSObjectMap, ops));
    if (map->ops == &js_ObjectOps) {
        guard(true, lir->ins2(LIR_eq, ops, lir->insImmPtr(&js_ObjectOps)));
        return true;
    }
    LIns* n = lir->insLoadi(ops, offsetof(JSObjectOps, newObjectMap));
    if (map->ops->newObjectMap == js_ObjectOps.newObjectMap) {
        guard(true, lir->ins2(LIR_eq, n, lir->insImmPtr(&js_ObjectOps.newObjectMap)));
        return true;
    }
    return false;
}

void
TraceRecorder::stobj_set_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins)
{
    if (slot < JS_INITIAL_NSLOTS)
        lir->insStorei(v_ins, 
                obj_ins, 
                offsetof(JSObject, fslots) + slot * sizeof(jsval));
    else {
        if (!dslots_ins)
            dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
        lir->insStorei(v_ins, 
                dslots_ins,
                (slot - JS_INITIAL_NSLOTS) * sizeof(jsval));
    }
}    

void
TraceRecorder::stobj_get_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins, LIns*& v_ins)
{
    if (slot < JS_INITIAL_NSLOTS)
        v_ins = lir->insLoadi(obj_ins, 
                offsetof(JSObject, fslots) + slot * sizeof(jsval));
    else {
        if (!dslots_ins)
            dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
        v_ins = lir->insLoadi(
                dslots_ins,
                (slot - JS_INITIAL_NSLOTS) * sizeof(jsval));
    }
}    

bool
TraceRecorder::native_set(LIns* obj_ins, JSScopeProperty* sprop, LIns*& dslots_ins, LIns* v_ins)
{
    if (SPROP_HAS_STUB_SETTER(sprop) && sprop->slot != SPROP_INVALID_SLOT) {
        stobj_set_slot(obj_ins, sprop->slot, dslots_ins, v_ins);
        return true;
    }
    return false;
}

bool
TraceRecorder::native_get(LIns* obj_ins, LIns* pobj_ins, JSScopeProperty* sprop, 
        LIns*& dslots_ins, LIns*& v_ins)
{
    if (SPROP_HAS_STUB_GETTER(sprop)) {
        if (sprop->slot != SPROP_INVALID_SLOT) 
            stobj_get_slot(pobj_ins, sprop->slot, dslots_ins, v_ins);
        else
            v_ins = lir->insImm(JSVAL_VOID);
        return true;
    }        
    return false;
}    

void 
TraceRecorder::guard_jsval_tag(LIns* v_ins, jsuint tag)
{
    guard(true, lir->ins2i(LIR_eq, 
            lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)JSVAL_TAGMASK)),
            tag));
}

LIns*
TraceRecorder::int32_to_jsval(LIns* i_ins)
{
    LIns* args[] = { cx_ins, i_ins };
    LIns* ret = lir->insCall(F_BoxInt32, args);
    guard(false, lir->ins2(LIR_eq, ret, lir->insImmPtr((void*)JSVAL_ERROR_COOKIE)));
    return ret;
}

LIns*
TraceRecorder::double_to_jsval(LIns* d_ins)
{
    LIns* args[] = { cx_ins, d_ins };
    LIns* ret = lir->insCall(F_BoxDouble, args);
    guard(false, lir->ins2(LIR_eq, ret, lir->insImmPtr((void*)JSVAL_ERROR_COOKIE)));
    return ret;
}

LIns*
TraceRecorder::boolean_to_jsval(LIns* b_ins)
{
    return lir->ins2i(LIR_or, lir->ins2i(LIR_lsh, b_ins, JSVAL_TAGBITS), JSVAL_BOOLEAN);
}

JS_STATIC_ASSERT(JSVAL_OBJECT == 0);

LIns*
TraceRecorder::object_to_jsval(LIns* b_ins)
{
    return lir->ins2i(LIR_lsh, b_ins, JSVAL_TAGBITS);
}

LIns*
TraceRecorder::jsval_to_int32(LIns* v_ins)
{
    LIns* ret = lir->insCall(F_UnboxInt32, &v_ins);
    guard(false, lir->ins2i(LIR_eq, ret, INT32_ERROR_COOKIE));
    return ret;
}    

LIns*
TraceRecorder::jsval_to_double(LIns* v_ins)
{
    guard_jsval_tag(v_ins, JSVAL_DOUBLE);
    return lir->insLoadi(lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)~JSVAL_TAGMASK)), 0);
}    

LIns*
TraceRecorder::jsval_to_boolean(LIns* v_ins)
{
    guard(true, lir->ins2i(LIR_eq, lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)~JSVAL_TRUE)), 
            JSVAL_BOOLEAN));
    return lir->ins2i(LIR_ush, v_ins, JSVAL_TAGBITS); 
}

LIns*
TraceRecorder::jsval_to_object(LIns* v_ins)
{
    guard_jsval_tag(v_ins, JSVAL_OBJECT);
    return lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)~JSVAL_TAGMASK));
}

bool TraceRecorder::guardThatObjectIsDenseArray(JSObject* obj, LIns* obj_ins, LIns*& dslots_ins)
{
    if (!OBJ_IS_DENSE_ARRAY(cx, obj))
        return false;
    // guard(OBJ_GET_CLASS(obj) == &js_ArrayClass);
    LIns* class_ins;
    stobj_get_slot(obj_ins, JSSLOT_CLASS, dslots_ins, class_ins);
    class_ins = lir->ins2(LIR_and, class_ins, lir->insImmPtr((void*)~3));
    guard(true, lir->ins2(LIR_eq, class_ins, lir->insImmPtr(&js_ArrayClass)));
    return true;
}

bool TraceRecorder::guardDenseArrayIndexWithinBounds(JSObject* obj, jsint idx, 
        LIns* obj_ins, LIns*& dslots_ins, LIns* idx_ins)
{
    jsuint length = ARRAY_DENSE_LENGTH(obj);
    if (!((jsuint)idx < length && idx < obj->fslots[JSSLOT_ARRAY_LENGTH]))
        return false;
    LIns* length_ins;
    if (!dslots_ins)
        dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    stobj_get_slot(obj_ins, JSSLOT_ARRAY_LENGTH, dslots_ins, length_ins);
    // guard(index < length)
    guard(true, lir->ins2(LIR_lt, idx_ins, length_ins));
    // guard(index < capacity)
    guard(false, lir->ins_eq0(dslots_ins));
    guard(true, lir->ins2(LIR_lt, idx_ins, 
            lir->insLoadi(dslots_ins, -sizeof(jsval))));
    return true;
}

bool TraceRecorder::JSOP_INTERRUPT()
{
    return false;
}
bool TraceRecorder::JSOP_PUSH()
{
    return false;
}
bool TraceRecorder::JSOP_POPV()
{
    return false;
}
bool TraceRecorder::JSOP_ENTERWITH()
{
    return false;
}
bool TraceRecorder::JSOP_LEAVEWITH()
{
    return false;
}
bool TraceRecorder::JSOP_RETURN()
{
    return false;
}
bool TraceRecorder::JSOP_GOTO()
{
    return false;
}
bool TraceRecorder::JSOP_IFEQ()
{
    return false;
}
bool TraceRecorder::JSOP_IFNE()
{
    return false;
}
bool TraceRecorder::JSOP_ARGUMENTS()
{
    return false;
}
bool TraceRecorder::JSOP_FORARG()
{
    return false;
}
bool TraceRecorder::JSOP_FORVAR()
{
    return false;
}
bool TraceRecorder::JSOP_DUP()
{
    return false;
}
bool TraceRecorder::JSOP_DUP2()
{
    return false;
}
bool TraceRecorder::JSOP_SETCONST()
{
    return false;
}
bool TraceRecorder::JSOP_BITOR()
{
    return ibinary(LIR_or);
}
bool TraceRecorder::JSOP_BITXOR()
{
    return ibinary(LIR_xor);
}
bool TraceRecorder::JSOP_BITAND()
{
    return ibinary(LIR_and);
}
bool TraceRecorder::JSOP_EQ()
{
    return cmp(LIR_eq);
}
bool TraceRecorder::JSOP_NE()
{
    return cmp(LIR_eq, true);
}
bool TraceRecorder::JSOP_LT()
{
    return cmp(LIR_lt);
}
bool TraceRecorder::JSOP_LE()
{
    return cmp(LIR_le);
}
bool TraceRecorder::JSOP_GT()
{
    return cmp(LIR_gt);
}
bool TraceRecorder::JSOP_GE()
{
    return cmp(LIR_ge);
}
bool TraceRecorder::JSOP_LSH()
{
    return ibinary(LIR_lsh);
}
bool TraceRecorder::JSOP_RSH()
{
    return ibinary(LIR_rsh);
}
bool TraceRecorder::JSOP_URSH()
{
    return ibinary(LIR_ush);
}
bool TraceRecorder::JSOP_ADD()
{
    return false;
}
bool TraceRecorder::JSOP_SUB()
{
    return false;
}
bool TraceRecorder::JSOP_MUL()
{
    return false;
}
bool TraceRecorder::JSOP_DIV()
{
    return false;
}
bool TraceRecorder::JSOP_MOD()
{
    return false;
}
bool TraceRecorder::JSOP_NOT()
{
    jsval& v = stackval(-1);
    if (isTrueOrFalse(v)) {
        set(&v, lir->ins_eq0(get(&v)));
        return true;
    }
    return false;
}
bool TraceRecorder::JSOP_BITNOT()
{
    return iunary(LIR_not);
}
bool TraceRecorder::JSOP_NEG()
{
    return false;
}
bool TraceRecorder::JSOP_NEW()
{
    return false;
}
bool TraceRecorder::JSOP_DELNAME()
{
    return false;
}
bool TraceRecorder::JSOP_DELPROP()
{
    return false;
}
bool TraceRecorder::JSOP_DELELEM()
{
    return false;
}
bool TraceRecorder::JSOP_TYPEOF()
{
    return false;
}
bool TraceRecorder::JSOP_VOID()
{
    return false;
}
bool TraceRecorder::JSOP_INCNAME()
{
    return false;
}
bool TraceRecorder::JSOP_INCPROP()
{
    return false;
}
bool TraceRecorder::JSOP_INCELEM()
{
    return false;
}
bool TraceRecorder::JSOP_DECNAME()
{
    return false;
}
bool TraceRecorder::JSOP_DECPROP()
{
    return false;
}
bool TraceRecorder::JSOP_DECELEM()
{
    return false;
}
bool TraceRecorder::JSOP_NAMEINC()
{
    return false;
}
bool TraceRecorder::JSOP_PROPINC()
{
    return false;
}
bool TraceRecorder::JSOP_ELEMINC()
{
    return false;
}
bool TraceRecorder::JSOP_NAMEDEC()
{
    return false;
}
bool TraceRecorder::JSOP_PROPDEC()
{
    return false;
}
bool TraceRecorder::JSOP_ELEMDEC()
{
    return false;
}
bool TraceRecorder::JSOP_GETPROP()
{
    return false;
}
bool TraceRecorder::JSOP_SETPROP()
{
    return false;
}
bool TraceRecorder::JSOP_GETELEM()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    /* no guards for type checks, trace specialized this already */
    if (!isInt(r) || JSVAL_IS_PRIMITIVE(l))
        return false;
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
    /* make sure the object is actually a dense array */
    LIns* dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    if (!guardThatObjectIsDenseArray(obj, obj_ins, dslots_ins))
        return false;
    /* check that the index is within bounds */
    jsint idx = asInt(r);
    LIns* idx_ins = get(&r);
    if (!guardDenseArrayIndexWithinBounds(obj, idx, obj_ins, dslots_ins, idx_ins))
        return false;
    jsval v = obj->dslots[idx];
    /* ok, we can trace this case since we now have the value and thus know the type */
    LIns* addr = lir->ins2(LIR_add, dslots_ins, 
            lir->ins2i(LIR_lsh, idx_ins, sizeof(jsval) == 4 ? 2 : 3));
    /* load the value, check the type (need to check JSVAL_HOLE only for booleans) */
    LIns* v_ins = lir->insLoadi(addr, 0);
    if (isInt(v))
        v_ins = jsval_to_int32(v_ins);
    else if (isDouble(v))
        v_ins = jsval_to_double(v_ins);
    else if (isTrueOrFalse(v))
        v_ins = jsval_to_boolean(v_ins);
    else
        return false; /* we don't know how to convert that type */
    set(&l, v_ins);
    return true;
}
bool TraceRecorder::JSOP_SETELEM()
{
    jsval& v = stackval(-1);
    jsval& r = stackval(-2);
    jsval& l = stackval(-3);
    /* no guards for type checks, trace specialized this already */
    if (!isInt(r) || JSVAL_IS_PRIMITIVE(l))
        return false;
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
    /* make sure the object is actually a dense array */
    LIns* dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    if (!guardThatObjectIsDenseArray(obj, obj_ins, dslots_ins))
        return false;
    /* check that the index is within bounds */
    jsint idx = asInt(r);
    LIns* idx_ins = get(&r);
    if (!guardDenseArrayIndexWithinBounds(obj, idx, obj_ins, dslots_ins, idx_ins))
        return false;
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
    LIns* boxed_ins;
    if (isInt(v))
        boxed_ins = int32_to_jsval(v_ins);
    else if (isDouble(v))
        boxed_ins = double_to_jsval(v_ins);
    else if (isTrueOrFalse(v))
        boxed_ins = boolean_to_jsval(v_ins);
    else
        return false; /* don't know how to box this type */
    lir->insStorei(boxed_ins, addr, 0);
    set(&l, v_ins);
    return true;
}
bool TraceRecorder::JSOP_CALLNAME()
{
    return false;
}
bool TraceRecorder::JSOP_CALL()
{
    return false;
}
bool TraceRecorder::JSOP_NAME()
{
    return false;
}
bool TraceRecorder::JSOP_DOUBLE()
{
    return false;
}
bool TraceRecorder::JSOP_STRING()
{
    return false;
}
bool TraceRecorder::JSOP_ZERO()
{
    stack(0, lir->insImm(0));
    return true;
}
bool TraceRecorder::JSOP_ONE()
{
    stack(0, lir->insImm(1));
    return true;
}
bool TraceRecorder::JSOP_NULL()
{
    stack(0, lir->insImmPtr(NULL));
    return true;
}
bool TraceRecorder::JSOP_THIS()
{
    return false;
}
bool TraceRecorder::JSOP_FALSE()
{
    stack(0, lir->insImm(0));
    return true;
}
bool TraceRecorder::JSOP_TRUE()
{
    stack(0, lir->insImm(1));
    return true;
}
bool TraceRecorder::JSOP_OR()
{
    return bbinary(LIR_or);
}
bool TraceRecorder::JSOP_AND()
{
    return bbinary(LIR_and);
}
bool TraceRecorder::JSOP_TABLESWITCH()
{
    return false;
}
bool TraceRecorder::JSOP_LOOKUPSWITCH()
{
    return false;
}
bool TraceRecorder::JSOP_STRICTEQ()
{
    return false;
}
bool TraceRecorder::JSOP_STRICTNE()
{
    return false;
}
bool TraceRecorder::JSOP_CLOSURE()
{
    return false;
}
bool TraceRecorder::JSOP_EXPORTALL()
{
    return false;
}
bool TraceRecorder::JSOP_EXPORTNAME()
{
    return false;
}
bool TraceRecorder::JSOP_IMPORTALL()
{
    return false;
}
bool TraceRecorder::JSOP_IMPORTPROP()
{
    return false;
}
bool TraceRecorder::JSOP_IMPORTELEM()
{
    return false;
}
bool TraceRecorder::JSOP_OBJECT()
{
    return false;
}
bool TraceRecorder::JSOP_POP()
{
    return false;
}
bool TraceRecorder::JSOP_POS()
{
    return false;
}
bool TraceRecorder::JSOP_TRAP()
{
    return false;
}
bool TraceRecorder::JSOP_GETARG()
{
    stack(0, arg(GET_ARGNO(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::JSOP_SETARG()
{
    arg(GET_ARGNO(cx->fp->regs->pc), stack(-1));
    return true;
}
bool TraceRecorder::JSOP_GETVAR()
{
    stack(0, var(GET_VARNO(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::JSOP_SETVAR()
{
    var(GET_VARNO(cx->fp->regs->pc), stack(-1));
    return true;
}
bool TraceRecorder::JSOP_UINT16()
{
    stack(0, lir->insImm((jsint) GET_UINT16(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::JSOP_NEWINIT()
{
    return false;
}
bool TraceRecorder::JSOP_ENDINIT()
{
    return false;
}
bool TraceRecorder::JSOP_INITPROP()
{
    return false;
}
bool TraceRecorder::JSOP_INITELEM()
{
    return false;
}
bool TraceRecorder::JSOP_DEFSHARP()
{
    return false;
}
bool TraceRecorder::JSOP_USESHARP()
{
    return false;
}
bool TraceRecorder::JSOP_INCARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1, true);
}
bool TraceRecorder::JSOP_INCVAR()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), 1, true);
}
bool TraceRecorder::JSOP_DECARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1, true);
}
bool TraceRecorder::JSOP_DECVAR()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), -1, true);
}
bool TraceRecorder::JSOP_ARGINC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1, false);
}
bool TraceRecorder::JSOP_VARINC()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), 1, false);
}
bool TraceRecorder::JSOP_ARGDEC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1, false);
}
bool TraceRecorder::JSOP_VARDEC()
{
    return inc(varval(GET_VARNO(cx->fp->regs->pc)), -1, false);
}
bool TraceRecorder::JSOP_ITER()
{
    return false;
}
bool TraceRecorder::JSOP_FORNAME()
{
    return false;
}
bool TraceRecorder::JSOP_FORPROP()
{
    return false;
}
bool TraceRecorder::JSOP_FORELEM()
{
    return false;
}
bool TraceRecorder::JSOP_POPN()
{
    return false;
}
bool TraceRecorder::JSOP_BINDNAME()
{
    /* BINDNAME is a no-op for the recorder. We wait until we hit the
       SETNAME/SETPROP that uses it. This is safe because the
       interpreter calculates here the scope we will use and we
       will use that value to guard against in SETNAME/SETPROP. */
    return true;
}
bool TraceRecorder::JSOP_SETNAME()
{
    return false;
}
bool TraceRecorder::JSOP_THROW()
{
    return false;
}
bool TraceRecorder::JSOP_IN()
{
    return false;
}
bool TraceRecorder::JSOP_INSTANCEOF()
{
    return false;
}
bool TraceRecorder::JSOP_DEBUGGER()
{
    return false;
}
bool TraceRecorder::JSOP_GOSUB()
{
    return false;
}
bool TraceRecorder::JSOP_RETSUB()
{
    return false;
}
bool TraceRecorder::JSOP_EXCEPTION()
{
    return false;
}
bool TraceRecorder::JSOP_LINENO()
{
    return false;
}
bool TraceRecorder::JSOP_CONDSWITCH()
{
    return false;
}
bool TraceRecorder::JSOP_CASE()
{
    return false;
}
bool TraceRecorder::JSOP_DEFAULT()
{
    return false;
}
bool TraceRecorder::JSOP_EVAL()
{
    return false;
}
bool TraceRecorder::JSOP_ENUMELEM()
{
    return false;
}
bool TraceRecorder::JSOP_GETTER()
{
    return false;
}
bool TraceRecorder::JSOP_SETTER()
{
    return false;
}
bool TraceRecorder::JSOP_DEFFUN()
{
    return false;
}
bool TraceRecorder::JSOP_DEFCONST()
{
    return false;
}
bool TraceRecorder::JSOP_DEFVAR()
{
    return false;
}
bool TraceRecorder::JSOP_ANONFUNOBJ()
{
    return false;
}
bool TraceRecorder::JSOP_NAMEDFUNOBJ()
{
    return false;
}
bool TraceRecorder::JSOP_SETLOCALPOP()
{
    return false;
}
bool TraceRecorder::JSOP_GROUP()
{
    return true; // no-op
}
bool TraceRecorder::JSOP_SETCALL()
{
    return false;
}
bool TraceRecorder::JSOP_TRY()
{
    return false;
}
bool TraceRecorder::JSOP_FINALLY()
{
    return false;
}
bool TraceRecorder::JSOP_NOP()
{
    return false;
}
bool TraceRecorder::JSOP_ARGSUB()
{
    return false;
}
bool TraceRecorder::JSOP_ARGCNT()
{
    return false;
}
bool TraceRecorder::JSOP_DEFLOCALFUN()
{
    return false;
}
bool TraceRecorder::JSOP_GOTOX()
{
    return false;
}
bool TraceRecorder::JSOP_IFEQX()
{
    return false;
}
bool TraceRecorder::JSOP_IFNEX()
{
    return false;
}
bool TraceRecorder::JSOP_ORX()
{
    return false;
}
bool TraceRecorder::JSOP_ANDX()
{
    return false;
}
bool TraceRecorder::JSOP_GOSUBX()
{
    return false;
}
bool TraceRecorder::JSOP_CASEX()
{
    return false;
}
bool TraceRecorder::JSOP_DEFAULTX()
{
    return false;
}
bool TraceRecorder::JSOP_TABLESWITCHX()
{
    return false;
}
bool TraceRecorder::JSOP_LOOKUPSWITCHX()
{
    return false;
}
bool TraceRecorder::JSOP_BACKPATCH()
{
    return false;
}
bool TraceRecorder::JSOP_BACKPATCH_POP()
{
    return false;
}
bool TraceRecorder::JSOP_THROWING()
{
    return false;
}
bool TraceRecorder::JSOP_SETRVAL()
{
    return false;
}
bool TraceRecorder::JSOP_RETRVAL()
{
    return false;
}
bool TraceRecorder::JSOP_GETGVAR()
{
    return false;
}
bool TraceRecorder::JSOP_SETGVAR()
{
    return false;
}
bool TraceRecorder::JSOP_INCGVAR()
{
    return false;
}
bool TraceRecorder::JSOP_DECGVAR()
{
    return false;
}
bool TraceRecorder::JSOP_GVARINC()
{
    return false;
}
bool TraceRecorder::JSOP_GVARDEC()
{
    return false;
}
bool TraceRecorder::JSOP_REGEXP()
{
    return false;
}
bool TraceRecorder::JSOP_DEFXMLNS()
{
    return false;
}
bool TraceRecorder::JSOP_ANYNAME()
{
    return false;
}
bool TraceRecorder::JSOP_QNAMEPART()
{
    return false;
}
bool TraceRecorder::JSOP_QNAMECONST()
{
    return false;
}
bool TraceRecorder::JSOP_QNAME()
{
    return false;
}
bool TraceRecorder::JSOP_TOATTRNAME()
{
    return false;
}
bool TraceRecorder::JSOP_TOATTRVAL()
{
    return false;
}
bool TraceRecorder::JSOP_ADDATTRNAME()
{
    return false;
}
bool TraceRecorder::JSOP_ADDATTRVAL()
{
    return false;
}
bool TraceRecorder::JSOP_BINDXMLNAME()
{
    return false;
}
bool TraceRecorder::JSOP_SETXMLNAME()
{
    return false;
}
bool TraceRecorder::JSOP_XMLNAME()
{
    return false;
}
bool TraceRecorder::JSOP_DESCENDANTS()
{
    return false;
}
bool TraceRecorder::JSOP_FILTER()
{
    return false;
}
bool TraceRecorder::JSOP_ENDFILTER()
{
    return false;
}
bool TraceRecorder::JSOP_TOXML()
{
    return false;
}
bool TraceRecorder::JSOP_TOXMLLIST()
{
    return false;
}
bool TraceRecorder::JSOP_XMLTAGEXPR()
{
    return false;
}
bool TraceRecorder::JSOP_XMLELTEXPR()
{
    return false;
}
bool TraceRecorder::JSOP_XMLOBJECT()
{
    return false;
}
bool TraceRecorder::JSOP_XMLCDATA()
{
    return false;
}
bool TraceRecorder::JSOP_XMLCOMMENT()
{
    return false;
}
bool TraceRecorder::JSOP_XMLPI()
{
    return false;
}
bool TraceRecorder::JSOP_CALLPROP()
{
    return false;
}
bool TraceRecorder::JSOP_GETFUNNS()
{
    return false;
}
bool TraceRecorder::JSOP_UNUSED186()
{
    return false;
}
bool TraceRecorder::JSOP_DELDESC()
{
    return false;
}
bool TraceRecorder::JSOP_UINT24()
{
    stack(0, lir->insImm((jsint) GET_UINT24(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::JSOP_INDEXBASE()
{
    return false;
}
bool TraceRecorder::JSOP_RESETBASE()
{
    return false;
}
bool TraceRecorder::JSOP_RESETBASE0()
{
    return false;
}
bool TraceRecorder::JSOP_STARTXML()
{
    return false;
}
bool TraceRecorder::JSOP_STARTXMLEXPR()
{
    return false;
}
bool TraceRecorder::JSOP_CALLELEM()
{
    return false;
}
bool TraceRecorder::JSOP_STOP()
{
    return false;
}
bool TraceRecorder::JSOP_GETXPROP()
{
    return false;
}
bool TraceRecorder::JSOP_CALLXMLNAME()
{
    return false;
}
bool TraceRecorder::JSOP_TYPEOFEXPR()
{
    return false;
}
bool TraceRecorder::JSOP_ENTERBLOCK()
{
    return false;
}
bool TraceRecorder::JSOP_LEAVEBLOCK()
{
    return false;
}
bool TraceRecorder::JSOP_GETLOCAL()
{
    return false;
}
bool TraceRecorder::JSOP_SETLOCAL()
{
    return false;
}
bool TraceRecorder::JSOP_INCLOCAL()
{
    return false;
}
bool TraceRecorder::JSOP_DECLOCAL()
{
    return false;
}
bool TraceRecorder::JSOP_LOCALINC()
{
    return false;
}
bool TraceRecorder::JSOP_LOCALDEC()
{
    return false;
}
bool TraceRecorder::JSOP_FORLOCAL()
{
    return false;
}
bool TraceRecorder::JSOP_FORCONST()
{
    return false;
}
bool TraceRecorder::JSOP_ENDITER()
{
    return false;
}
bool TraceRecorder::JSOP_GENERATOR()
{
    return false;
}
bool TraceRecorder::JSOP_YIELD()
{
    return false;
}
bool TraceRecorder::JSOP_ARRAYPUSH()
{
    return false;
}
bool TraceRecorder::JSOP_UNUSED213()
{
    return false;
}
bool TraceRecorder::JSOP_ENUMCONSTELEM()
{
    return false;
}
bool TraceRecorder::JSOP_LEAVEBLOCKEXPR()
{
    return false;
}
bool TraceRecorder::JSOP_GETTHISPROP()
{
    return false;
}
bool TraceRecorder::JSOP_GETARGPROP()
{
    return false;
}
bool TraceRecorder::JSOP_GETVARPROP()
{
    return false;
}
bool TraceRecorder::JSOP_GETLOCALPROP()
{
    return false;
}
bool TraceRecorder::JSOP_INDEXBASE1()
{
    return false;
}
bool TraceRecorder::JSOP_INDEXBASE2()
{
    return false;
}
bool TraceRecorder::JSOP_INDEXBASE3()
{
    return false;
}
bool TraceRecorder::JSOP_CALLGVAR()
{
    return false;
}
bool TraceRecorder::JSOP_CALLVAR()
{
    return false;
}
bool TraceRecorder::JSOP_CALLARG()
{
    return false;
}
bool TraceRecorder::JSOP_CALLLOCAL()
{
    return false;
}
bool TraceRecorder::JSOP_INT8()
{
    stack(0, lir->insImm(GET_INT8(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::JSOP_INT32()
{
    stack(0, lir->insImm(GET_INT32(cx->fp->regs->pc)));
    return true;
}
bool TraceRecorder::JSOP_LENGTH()
{
    return false;
}
bool TraceRecorder::JSOP_NEWARRAY()
{
    return false;
}
bool TraceRecorder::JSOP_HOLE()
{
    return false;
}
