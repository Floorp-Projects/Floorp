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

template <typename T>
Tracker<T>::Tracker()
{
    pagelist = 0;
}

template <typename T>
Tracker<T>::~Tracker()
{
    clear();
}

template <typename T> long
Tracker<T>::getPageBase(const void* v) const
{
    return ((long)v) & (~(NJ_PAGE_SIZE-1));
}

template <typename T> struct Tracker<T>::Page*
Tracker<T>::findPage(const void* v) const
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

template <typename T> struct Tracker<T>::Page*
Tracker<T>::addPage(const void* v) {
    long base = getPageBase(v);
    struct Tracker<T>::Page* p = (struct Tracker<T>::Page*)
        GC::Alloc(sizeof(struct Tracker<T>::Page) + (NJ_PAGE_SIZE >> 2) * sizeof(T));
    p->base = base;
    p->next = pagelist;
    pagelist = p;
    return p;
}

template <typename T> void
Tracker<T>::clear()
{
    while (pagelist) {
        Page* p = pagelist;
        pagelist = pagelist->next;
        GC::Free(p);
    }
}

template <typename T> T
Tracker<T>::get(const void* v) const
{
    struct Page* p = findPage(v);
    JS_ASSERT(p != 0); /* we must have a page for the slot we are looking for */
    T i = p->map[(((long)v) & 0xfff) >> 2];
    JS_ASSERT(i != 0);
#ifdef DEBUG    
    //printf("get %p, which is %s\n",v, nanojit::lirNames[i->opcode()]);
#endif    
    return i;
}

template <typename T> void
Tracker<T>::set(const void* v, T i)
{
#ifdef DEBUG    
    //printf("set %p to %s\n", v, nanojit::lirNames[i->opcode()]);
#endif    
    struct Tracker::Page* p = findPage(v);
    if (!p)
        p = addPage(v);
    p->map[(((long)v) & 0xfff) >> 2] = i;
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

    JSTraceRecorder* recorder = new JSTraceRecorder(regs);
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
    JSStackFrame* fp = cx->fp;

    recorder->tracker.set(cx, lir->insLoadi(fragment->param0, 0));
    recorder->tracker.set(&recorder->entryState.sp, lir->insLoadi(fragment->param0, 4));

#define LOAD_CONTEXT(p)                                                       \
    recorder->tracker.set(p,                                                  \
        lir->insLoadi(recorder->tracker.get(&recorder->entryState.sp),        \
                      STACK_OFFSET(p)))

    unsigned n;
    for (n = 0; n < fp->argc; ++n)
        LOAD_CONTEXT(&fp->argv[n]);
    for (n = 0; n < fp->nvars; ++n) 
        LOAD_CONTEXT(&fp->vars[n]);
    for (n = 0; n < (unsigned)(regs.sp - fp->spbase); ++n)
        LOAD_CONTEXT(&fp->spbase[n]);

    recorder->fragment = fragment;
    recorder->lir = lir;

    return true;
}

void
js_EndRecording(JSContext* cx, JSFrameRegs& regs)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->recorder != NULL) {
        JSTraceRecorder* recorder = tm->recorder;
        recorder->fragment->lastIns = recorder->lir->ins0(LIR_loop);
        compile(tm->fragmento->assm(), recorder->fragment);
        delete recorder;
        tm->recorder = NULL;
    }
}
