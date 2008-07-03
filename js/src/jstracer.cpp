/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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

#define jstracer_cpp___

#include "nanojit/avmplus.h"
#include "nanojit/nanojit.h"

using namespace nanojit;

#include "jsinterp.cpp"

Tracker::Tracker()
{
    pagelist = 0;
}

Tracker::~Tracker()
{
    clear();
}

long
Tracker::getPageBase(const void* v) const
{
    return ((long)v) & (~(NJ_PAGE_SIZE-1));
}

struct Tracker::Page*
Tracker::findPage(const void* v) const
{
    long base = getPageBase(v);
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
    long base = getPageBase(v);
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
    LIns* i = p->map[(((long)v) & 0xfff) >> 2];
    JS_ASSERT(i != 0);
    return i;
}

void
Tracker::set(const void* v, LIns* ins)
{
    struct Tracker::Page* p = findPage(v);
    if (!p)
        p = addPage(v);
    p->map[(((long)v) & 0xfff) >> 2] = ins;
}

using namespace avmplus;
using namespace nanojit;

static GC gc = GC();
static avmplus::AvmCore* core = new (&gc) avmplus::AvmCore();

#define LO ARGSIZE_LO
#define F  ARGSIZE_F
#define Q  ARGSIZE_Q

#ifdef DEBUG
#define NAME(op) ,#op
#else
#define NAME(op)
#endif

void builtin_unimplemented(void) {
    JS_ASSERT(0);
}

#define builtin_DOUBLE_IS_INT builtin_unimplemented
#define builtin_StringToDouble builtin_unimplemented
#define builtin_ObjectToDouble builtin_unimplemented
#define builtin_ValueToBoolean builtin_unimplemented

jsint builtin_DoubleToECMAInt32(jsdouble d) {
    return js_DoubleToECMAInt32(d);
}

#define BUILTIN1(op, at0, atr, tr, t0, cse, fold) \
    { (intptr_t)&builtin_##op, (at0 << 2) | atr, cse, fold NAME(op) },
#define BUILTIN2(op, at0, at1, atr, tr, t0, t1, cse, fold) \
    { 0, (at0 << 4) | (at1 << 2) | atr, cse, fold NAME(op) },
#define BUILTIN3(op, at0, at1, at2, atr, tr, t0, t1, t2, cse, fold) \
    { 0, (at0 << 6) | (at1 << 4) | (at2 << 2) | atr, cse, fold NAME(op) },

static struct CallInfo builtins[] = {
#include "builtins.tbl"
};

#undef NAME
#undef BUILTIN1
#undef BUILTIN2
#undef BUILTIN3

static void
buildTypeMap(JSStackFrame* entryFrame, JSStackFrame* fp, JSFrameRegs& regs, char* m);

TraceRecorder::TraceRecorder(JSContext* cx, Fragmento* fragmento, Fragment* _fragment) 
{
    this->cx = cx;
    this->fragment = _fragment;
    entryFrame = cx->fp;
    entryRegs = *(entryFrame->regs);

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
    LIns* data = lir_buf_writer->skip(sizeof(VMFragmentInfo) + entryNativeFrameSlots * sizeof(char));
    VMFragmentInfo* fi = (VMFragmentInfo*)data->payload();
    buildTypeMap(entryFrame, entryFrame, entryRegs, fi->typeMap);
    entryTypeMap = fi->typeMap;
    fragment->vmprivate = fi;
    fragment->param0 = lir->insImm8(LIR_param, Assembler::argRegs[0], 0);
    fragment->param1 = lir->insImm8(LIR_param, Assembler::argRegs[1], 0);
    fragment->sp = lir->insLoadi(fragment->param0, offsetof(InterpState, sp));
    cx_ins = lir->insLoadi(fragment->param0, offsetof(InterpState, f));
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
    for (n = 0; n < (unsigned)(fp->regs->sp - fp->spbase); ++n)
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
TraceRecorder::calldepth() const
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
    JSStackFrame* fp = cx->fp;
    while (1) {
        if ((p >= &fp->argv[0] && p < &fp->argv[fp->argc]) ||
            (p >= &fp->vars[0] && p < &fp->vars[fp->nvars]) ||
            (p >= &fp->spbase[0] && p < &fp->spbase[fp->script->depth]))
            return fp;
        if (fp == entryFrame)
            return NULL;
        fp = fp->down;
    }
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
    unsigned size = 0;
    while (1) {
        size += fp->argc + fp->nvars + (regs.sp - fp->spbase);
        if (fp == entryFrame)
            return size;
        fp = fp->down;
    } 
}

/* Determine the offset in the native frame (marshal) for an address
   that is part of a currently active frame. */
unsigned
TraceRecorder::nativeFrameOffset(void* p) const
{
    JSStackFrame* fp = findFrame(p);
    JS_ASSERT(fp != NULL); // must be on the frame somewhere
    unsigned offset = 0;
    if (p >= &fp->argv[0] && p < &fp->argv[fp->argc])
        offset = unsigned((jsval*)p - &fp->argv[0]);
    else if (p >= &fp->vars[0] && p < &fp->vars[fp->nvars])
        offset = (fp->argc + unsigned((jsval*)p - &fp->vars[0]));
    else {
        JS_ASSERT((p >= &fp->spbase[0] && p < &fp->spbase[fp->script->depth]));
        offset = (fp->argc + fp->nvars + unsigned((jsval*)p - &fp->spbase[0]));
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
static inline int gettag(jsval v)
{
    if (JSVAL_IS_INT(v))
        return JSVAL_INT;
    return JSVAL_TAG(v);
}   

/* Write out a type map for the current scopes and all outer scopes,
   up until the entry scope. */
static void
buildTypeMap(JSStackFrame* entryFrame, JSStackFrame* fp, JSFrameRegs& regs, char* m)
{
    if (fp != entryFrame)
        buildTypeMap(entryFrame, fp->down, *fp->down->regs, m);
    for (unsigned n = 0; n < fp->argc; ++n)
        *m++ = gettag(fp->argv[n]);
    for (unsigned n = 0; n < fp->nvars; ++n)
        *m++ = gettag(fp->vars[n]);
    for (jsval* sp = fp->spbase; sp < regs.sp; ++sp)
        *m++ = gettag(*sp);
}

/* Make sure that all loop-carrying values have a stable type. */
static bool
verifyTypeStability(JSStackFrame* entryFrame, JSStackFrame* fp, JSFrameRegs& regs, char* m)
{
    if (fp != entryFrame)
        verifyTypeStability(entryFrame, fp->down, *fp->down->regs, m);
    for (unsigned n = 0; n < fp->argc; ++n)
        if (*m++ != gettag(fp->argv[n]))
            return false;
    for (unsigned n = 0; n < fp->nvars; ++n)
        if (*m++ != gettag(fp->vars[n]))
            return false;
    for (jsval* sp = fp->spbase; sp < regs.sp; ++sp)
        if (*m++ != gettag(*sp))
            return false;
    return true;
}

/* Unbox a jsval into a slot. Slots are wide enough to hold double values 
   directly (instead of storing a pointer to them). */
static bool
unbox_jsval(jsval v, int t, double* slot)
{
    if (t != gettag(v))
        return false;
    switch (t) {
    case JSVAL_BOOLEAN:
        *(bool*)slot = JSVAL_TO_BOOLEAN(v);
        break;
    case JSVAL_INT:
        *(jsint*)slot = JSVAL_TO_INT(v);
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
        JS_ASSERT(INT_FITS_IN_JSVAL(i)); 
        *vp = INT_TO_JSVAL(i);
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
    LIns *ins = lir->insLoad(JSVAL_IS_DOUBLE(*p) ? LIR_ldq : LIR_ld,
            fragment->sp, nativeFrameOffset(p));
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
        lir->insStorei(i, fragment->sp, nativeFrameOffset(p));
}

LIns* 
TraceRecorder::get(void* p)
{
    if (p == cx)
        return cx_ins;
    return tracker.get(p);
}

void 
TraceRecorder::copy(void* a, void* v)
{
    set(v, get(a));
}

void
TraceRecorder::imm(jsint i, void* v)
{
    set(v, lir->insImm(i));
}

void 
TraceRecorder::imm(jsdouble d, void* v)
{
    set(v, lir->insImmq(*(uint64_t*)&d));
}

void 
TraceRecorder::unary(LOpcode op, void* a, void* v)
{
    set(v, lir->ins1(op, get(a)));
}

void 
TraceRecorder::binary(LOpcode op, void* a, void* b, void* v)
{
    set(v, lir->ins2(op, get(a), get(b)));
}

void
TraceRecorder::binary0(LOpcode op, void* a, void* v)
{
    set(v, lir->ins2i(op, get(a), 0)); 
}

void
TraceRecorder::choose(void* cond, void* iftrue, void* iffalse, void* v)
{
    set(v, lir->ins_choose(get(cond), get(iftrue), get(iffalse), true));
}

void
TraceRecorder::choose_eqi(void* a, int32_t b, void* iftrue, void* iffalse, void* v)
{
    set(v, lir->ins_choose(lir->ins2i(LIR_eq, get(a), b), get(iftrue), get(iffalse), true));
}

void 
TraceRecorder::call(int id, void* a, void* v)
{
    LInsp args[] = { get(a) };
    set(v, lir->insCall(id, args));
}

void 
TraceRecorder::call(int id, void* a, void* b, void* v)
{
    LInsp args[] = { get(a), get(b) };
    set(v, lir->insCall(id, args));
}

void 
TraceRecorder::call(int id, void* a, void* b, void* c, void* v)
{
    LInsp args[] = { get(a), get(b), get(c) };
    set(v, lir->insCall(id, args));
}

void
TraceRecorder::iinc(void* a, int incr, void* v)
{
    LIns* ov = lir->ins2(LIR_add, get(a), lir->insImm(incr));
    // This check is actually supposed to happen before iinc, however, 
    // we arrive at iinc only if CAN_DO_INC_DEC passed, so we know that the
    // inverse of it (overflow check) must evaluate to false in the trace.
    // We delay setting v to the result of the calculation until after the 
    // guard to make sure the result is not communicated to the interpreter 
    // in case this guard fails (as it was supposed to execute _before_ the 
    // add, not after.)
    guard_ov(false, a); 
    set(v, ov);
}

/* Mark the current state of the interpreter in the side exit structure. This
   is the state we want to return to if we have to bail out through a guard. */
void 
TraceRecorder::mark()
{
    markRegs = *cx->fp->regs;
}

/* Reset the interpreter to the last mark point. This is used when ending or
   aborting the recorder, at which point the non-tracing interpreter will
   re-execute the instruction. */
void 
TraceRecorder::recover()
{
    *cx->fp->regs = markRegs;
}

SideExit*
TraceRecorder::snapshot()
{
    /* generate the entry map and stash it in the trace */
    unsigned slots = nativeFrameSlots(cx->fp, markRegs);
    trackNativeFrameUse(slots);
    LIns* data = lir_buf_writer->skip(sizeof(VMSideExitInfo) + slots * sizeof(char));
    VMSideExitInfo* si = (VMSideExitInfo*)data->payload();
    buildTypeMap(entryFrame, cx->fp, markRegs, si->typeMap);
    /* setup side exit structure */
    memset(&exit, 0, sizeof(exit));
#ifdef DEBUG    
    exit.from = fragment;
#endif    
    exit.calldepth = calldepth();
    exit.sp_adj = (markRegs.sp - entryRegs.sp + 1) * sizeof(double);
    exit.ip_adj = markRegs.pc - entryRegs.pc;
    exit.vmprivate = si;
    return &exit;
}

void
TraceRecorder::guard_0(bool expected, void* a)
{
    lir->insGuard(expected ? LIR_xf : LIR_xt, 
            get(a), 
            snapshot());
}

void
TraceRecorder::guard_h(bool expected, void* a)
{
    lir->insGuard(expected ? LIR_xf : LIR_xt, 
            lir->ins1(LIR_callh, get(a)), 
            snapshot());
}

void
TraceRecorder::guard_ov(bool expected, void* a)
{
    lir->insGuard(expected ? LIR_xf : LIR_xt, 
            lir->ins1(LIR_ov, get(a)), 
            snapshot());
}

void
TraceRecorder::guard_eq(bool expected, void* a, void* b)
{
    lir->insGuard(expected ? LIR_xf : LIR_xt,
                  lir->ins2(LIR_eq, get(a), get(b)),
                  snapshot());
}

void
TraceRecorder::guard_eqi(bool expected, void* a, int i)
{
    lir->insGuard(expected ? LIR_xf : LIR_xt,
                  lir->ins2i(LIR_eq, get(a), i),
                  snapshot());
}

void
TraceRecorder::closeLoop(Fragmento* fragmento)
{
    if (!verifyTypeStability(entryFrame, entryFrame, entryRegs, entryTypeMap)) {
#ifdef DEBUG
        printf("Trace rejected: unstable loop variables.\n");
#endif        
        return;
    }
    fragment->lastIns = lir->ins0(LIR_loop);
    ((VMFragmentInfo*)fragment->vmprivate)->maxNativeFrameSlots = maxNativeFrameSlots;
    compile(fragmento->assm(), fragment);
}

void
TraceRecorder::load(void* a, int32_t i, void* v)
{
    set(v, lir->insLoadi(get(a), i));
}

Fragment*
js_LookupFragment(JSContext* cx, jsbytecode* pc)
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

    InterpState state;
    state.ip = (FOpcodep)pc;
    
    Fragment* f = tm->fragmento->getLoop(state);
    
    if (!f->code())
        return f;
    
    VMFragmentInfo* fi = (VMFragmentInfo*)f->vmprivate;
    double native[fi->maxNativeFrameSlots+1];
#ifdef DEBUG
    *(uint64*)&native[fi->maxNativeFrameSlots] = 0xdeadbeefdeadbeefLL;
#endif    
    unbox(cx->fp, *cx->fp->regs, fi->typeMap, native);
    state.sp = native;
    state.rp = NULL;
    state.f = (void*)cx;
    union { NIns *code; GuardRecord* (FASTCALL *func)(InterpState*, Fragment*); } u;
    u.code = f->code();
    GuardRecord* lr = u.func(&state, NULL);
    cx->fp->regs->sp += (((double*)state.sp - native) - 1);
    cx->fp->regs->pc = (jsbytecode*)state.ip;
    box(cx, cx->fp, *cx->fp->regs, ((VMSideExitInfo*)lr->vmprivate)->typeMap, native);
#ifdef DEBUG
    JS_ASSERT(*(uint64*)&native[fi->maxNativeFrameSlots] == 0xdeadbeefdeadbeefLL);
#endif    
    return f;
}

bool
js_StartRecording(JSContext* cx, Fragment* fragment)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    tm->recorder = new (&gc) TraceRecorder(cx, tm->fragmento, fragment);
    return true;
}

static void 
js_DeleteRecorder(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    delete tm->recorder;
    tm->recorder = NULL;
}

void
js_AbortRecording(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(tm->recorder != NULL);
    //tm->recorder->abort();
    tm->recorder->recover();
    js_DeleteRecorder(cx);
}

void
js_EndRecording(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(tm->recorder != NULL);
    tm->recorder->closeLoop(tm->fragmento);
    tm->recorder->recover();
    js_DeleteRecorder(cx);
}
