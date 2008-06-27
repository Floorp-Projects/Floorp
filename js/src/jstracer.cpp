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
int 
FrameStack::nativeOffset(void* p) const
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

TraceRecorder::TraceRecorder(JSStackFrame& _stackFrame, JSFrameRegs& _entryState) :
    frameStack(_stackFrame)
{
    entryState = _entryState;
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

static void
loadContext(JSContext* cx, TraceRecorder* r, void* p)
{
    JS_ASSERT(r->frameStack.contains(p));
    r->tracker.set(p, 
            r->lir->insLoadi(r->tracker.get(&r->entryState.sp),
                    r->frameStack.nativeOffset(p)));
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

    TraceRecorder* recorder = new TraceRecorder(*cx->fp, regs);
    tm->recorder = recorder;

    InterpState state;
    state.ip = NULL;
    state.sp = NULL;
    state.rp = NULL;
    state.f = NULL;

    Fragment* fragment = tm->fragmento->getLoop(state);
    LirBuffer* lirbuf = new (&gc) LirBuffer(tm->fragmento, builtins);
#ifdef DEBUG    
    lirbuf->names = new (&gc) LirNameMap(&gc, builtins, tm->fragmento->labels);
#endif    
    fragment->lirbuf = lirbuf;
    LirWriter* lir = new (&gc) LirBufWriter(lirbuf);
    lir->ins0(LIR_trace);
    fragment->param0 = lir->insImm8(LIR_param, Assembler::argRegs[0], 0);
    fragment->param1 = lir->insImm8(LIR_param, Assembler::argRegs[1], 0);

    recorder->fragment = fragment;
    recorder->lir = lir;

    recorder->tracker.set(cx, lir->insLoadi(fragment->param0, 0));
    recorder->tracker.set(&recorder->entryState.sp, lir->insLoadi(fragment->param0, 4));

    JSStackFrame* fp = cx->fp;
    unsigned n;
    for (n = 0; n < fp->argc; ++n)
        loadContext(cx, recorder, &fp->argv[n]);
    for (n = 0; n < fp->nvars; ++n) 
        loadContext(cx, recorder, &fp->vars[n]);
    for (n = 0; n < (unsigned)(regs.sp - fp->spbase); ++n)
        loadContext(cx, recorder, &fp->spbase[n]);

    return true;
}

void
js_EndRecording(JSContext* cx, JSFrameRegs& regs)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->recorder != NULL) {
        TraceRecorder* recorder = tm->recorder;
        recorder->fragment->lastIns = recorder->lir->ins0(LIR_loop);
        compile(tm->fragmento->assm(), recorder->fragment);
        delete recorder;
        tm->recorder = NULL;
    }
}
