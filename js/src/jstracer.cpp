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
 *   Brendan Eich <brendan@mozilla.org
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
        if (p->base == base)
            return p;
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

FrameStack::FrameStack(JSStackFrame& entryFrame)
{
    depth = 0;
    enter(entryFrame);
}

FrameStack::~FrameStack()
{
}

bool
FrameStack::enter(JSStackFrame& frame)
{
    if (depth >= (sizeof(stack)/sizeof(JSStackFrame*)))
        return false;
    stack[depth++] = &frame;
    return false;
}

void
FrameStack::leave()
{
    JS_ASSERT(depth > 0);
    --depth;
}

/* Find the frame that this address belongs to (if any). */
JSStackFrame*
FrameStack::findFrame(void* p) const
{
    for (uint32 n = 0; n < depth; ++n) {
        JSStackFrame* fp = stack[n];
        if ((p >= &fp->argv[0] && p < &fp->argv[fp->argc]) ||
            (p >= &fp->vars[0] && p < &fp->vars[fp->nvars]) ||
            (p >= &fp->spbase[0] && p < &fp->spbase[fp->script->depth]))
            return fp;
    }
    return NULL;
}

/* Determine whether an address is part of a currently active frame. */
bool
FrameStack::contains(void* p) const
{
    return findFrame(p) != NULL;
}

/* Determine the offset in the native frame (marshal) for an address
   that is part of a currently active frame. */
uint32_t
FrameStack::nativeFrameOffset(void* p) const
{
    JSStackFrame* fp = findFrame(p);
    JS_ASSERT(fp == stack[0]); // todo: calculate nested frame offsets
    if (p >= &fp->argv[0] && p < &fp->argv[fp->argc])
        return uint32_t((jsval*)p - &fp->argv[0]) * sizeof(long);
    if (p >= &fp->vars[0] && p < &fp->vars[fp->nvars])
        return (fp->argc + uint32_t((jsval*)p - &fp->vars[0])) * sizeof(long);
    JS_ASSERT((p >= &fp->spbase[0] && p < &fp->spbase[fp->script->depth]));
    return (fp->argc + fp->nvars + uint32_t((jsval*)p - &fp->spbase[0])) * sizeof(long);
}

uint32_t
FrameStack::nativeFrameSize() const
{
    uint32_t size = 0;
    for (uint32_t n = 0; n < depth; ++n) {
        JSStackFrame* fp = stack[n];
        size += fp->argc + fp->nvars + fp->script->depth;
    }
    return size;
}

using namespace avmplus;
using namespace nanojit;

static avmplus::AvmCore* core = new avmplus::AvmCore();
static GC gc = GC();

#define LO ARGSIZE_LO
#define F  ARGSIZE_F
#define Q  ARGSIZE_Q

#ifdef DEBUG
#define NAME(op) ,#op
#else
#define NAME(op)
#endif

#define BUILTIN1(op, at0, atr, tr, t0, cse, fold) \
    { 0, (at0 | (atr << 2)), cse, fold NAME(op) },
#define BUILTIN2(op, at0, at1, atr, tr, t0, t1, cse, fold) \
    { 0, (at0 | (at1 << 2) | (atr << 4)), cse, fold NAME(op) },
#define BUILTIN3(op, at0, at1, at2, atr, tr, t0, t1, t2, cse, fold) \
    { 0, (at0 | (at1 << 2) | (at2 << 4) | (atr << 6)), cse, fold NAME(op) },

static struct CallInfo builtins[] = {
#include "builtins.tbl"
};

#undef NAME
#undef BUILTIN1
#undef BUILTIN2
#undef BUILTIN3

TraceRecorder::TraceRecorder(JSContext* cx, JSFrameRegs& regs, Fragmento* fragmento) :
    frameStack(*cx->fp)
{
    entryState = regs;

    InterpState state;
    state.ip = NULL;
    state.sp = NULL;
    state.rp = NULL;
    state.f = NULL;
        
    fragment = fragmento->getLoop(state);
    lirbuf = new (&gc) LirBuffer(fragmento, builtins);
#ifdef DEBUG    
    lirbuf->names = new (&gc) LirNameMap(&gc, builtins, fragmento->labels);
#endif    
    fragment->lirbuf = lirbuf;
    lir = new (&gc) LirBufWriter(lirbuf);
    lir->ins0(LIR_trace);
    fragment->param0 = lir->insImm8(LIR_param, Assembler::argRegs[0], 0);
    fragment->param1 = lir->insImm8(LIR_param, Assembler::argRegs[1], 0);
    
    init(&cx, lir->insLoadi(fragment->param0, 0));
    init(&entryState.sp, lir->insLoadi(fragment->param0, 4));

    JSStackFrame* fp = cx->fp;
    unsigned n;
    for (n = 0; n < fp->argc; ++n)
        load(&fp->argv[n]);
    for (n = 0; n < fp->nvars; ++n) 
        load(&fp->vars[n]);
    for (n = 0; n < (unsigned)(regs.sp - fp->spbase); ++n)
        load(&fp->spbase[n]);
}

TraceRecorder::~TraceRecorder()
{
    delete lir;
#ifdef DEBUG    
    delete lirbuf->names;
#endif
    delete lirbuf;
    delete fragment;
}

void 
TraceRecorder::load(void* p)
{
    JS_ASSERT(frameStack.contains(p));
    tracker.set(p, lir->insLoadi(tracker.get(&entryState.sp), 
            frameStack.nativeFrameOffset(p)));
}

void 
TraceRecorder::init(void* p, LIns* i)
{
    tracker.set(p, i);
}

void 
TraceRecorder::set(void* p, LIns* i)
{
    init(p, i);
    if (frameStack.contains(p))
        lir->insStorei(i, get(&entryState.sp), 
                frameStack.nativeFrameOffset(p));
}

LIns* 
TraceRecorder::get(void* p)
{
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
TraceRecorder::unary(nanojit::LOpcode op, void* a, void* v)
{
    set(v, lir->ins1(op, get(a)));
}

void 
TraceRecorder::binary(nanojit::LOpcode op, void* a, void* b, void* v)
{
    set(v, lir->ins2(op, get(a), get(b)));
}

void
TraceRecorder::binary0(nanojit::LOpcode op, void* a, void* v)
{
    set(v, lir->ins2i(op, get(a), 0)); 
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
TraceRecorder::iinc(void* a, int incr, void* v, JSFrameRegs& regs)
{
    LIns* ov = lir->ins2(LIR_add, get(a), lir->insImm(incr));
    // This check is actually supposed to happen before iinc, however, 
    // we arrive at iinc only if CAN_DO_INC_DEC passed, so we know that the
    // inverse of it (overflow check) must evaluate to false in the trace.
    // We delay setting v to the result of the calculation until after the 
    // guard to make sure the result is not communicated to the interpreter 
    // in case this guard fails (as it was supposed to execute _before_ the 
    // add, not after.)
    guard_ov(false, a, regs); 
    set(v, ov);
}

SideExit*
TraceRecorder::snapshot(SideExit& exit, JSFrameRegs& regs)
{
    memset(&exit, 0, sizeof(exit));
    exit.from = fragment;
    return &exit;
}

void
TraceRecorder::guard_0(bool expected, void* a, JSFrameRegs& regs)
{
    SideExit exit;
    lir->insGuard(expected ? LIR_xf : LIR_xt, 
            get(a), 
            snapshot(exit, regs));
}

void
TraceRecorder::guard_h(bool expected, void* a, JSFrameRegs& regs)
{
    SideExit exit;
    lir->insGuard(expected ? LIR_xf : LIR_xt, 
            lir->ins1(LIR_callh, get(a)), 
            snapshot(exit, regs));
}

void
TraceRecorder::guard_ov(bool expected, void* a, JSFrameRegs& regs)
{
#if 0    
    SideExit exit;
    lir->insGuard(expected ? LIR_xf : LIR_xt, 
            lir->ins1(LIR_ov, get(a)), 
            snapshot(exit, regs));
#endif    
}

void
TraceRecorder::closeLoop(Fragmento* fragmento)
{
    fragment->lastIns = lir->ins0(LIR_loop);
    compile(fragmento->assm(), fragment);
}

bool
js_StartRecording(JSContext* cx, JSFrameRegs& regs)
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

    tm->recorder = new TraceRecorder(cx, regs, tm->fragmento);

    return true;
}

void
js_EndRecording(JSContext* cx, JSFrameRegs& regs)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->recorder != NULL) {
        tm->recorder->closeLoop(tm->fragmento);
        delete tm->recorder;
        tm->recorder = NULL;
    }
}
