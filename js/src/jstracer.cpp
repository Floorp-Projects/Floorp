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

template<typename T>
Tracker<T>::Tracker()
{
    pagelist = 0;
}

template<typename T>
Tracker<T>::~Tracker()
{
    clear();
}

template<typename T> jsuword
Tracker<T>::getPageBase(const void* v) const
{
    return jsuword(v) & ~jsuword(NJ_PAGE_SIZE-1);
}

template<typename T> struct Tracker<T>::Page*
Tracker<T>::findPage(const void* v) const
{
    jsuword base = getPageBase(v);
    struct Tracker<T>::Page* p = pagelist;
    while (p) {
        if (p->base == base) {
            return p;
        }
        p = p->next;
    }
    return 0;
}

template <typename T> struct Tracker<T>::Page*
Tracker<T>::addPage(const void* v) {
    jsuword base = getPageBase(v);
    struct Tracker::Page* p = (struct Tracker::Page*)
        GC::Alloc(sizeof(struct Tracker::Page) + (NJ_PAGE_SIZE >> 2) * sizeof(LInsp));
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
    struct Tracker<T>::Page* p = findPage(v);
    JS_ASSERT(p != 0); /* we must have a page for the slot we are looking for */
    T i = p->map[(jsuword(v) & 0xfff) >> 2];
    JS_ASSERT(i != 0);
    return i;
}

template <typename T> void
Tracker<T>::set(const void* v, T ins)
{
    struct Tracker<T>::Page* p = findPage(v);
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
    return d == (jsint)d;
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

static bool isPromoteInt(LIns *i)
{
    jsdouble d;
    return i->isop(LIR_i2f) || (i->isconstq() && ((d = i->constvalf()) == (jsdouble)(jsint)d)); 
}

static bool isPromoteUint(LIns *i)
{
    jsdouble d;
    return i->isop(LIR_u2f) || (i->isconstq() && ((d = i->constvalf()) == (jsdouble)(jsuint)d));
}

static bool isPromote(LIns *i)
{
    return isPromoteInt(i) || isPromoteUint(i);;
}

class FuncFilter: public LirWriter
{
    TraceRecorder& recorder;
public:
    FuncFilter(LirWriter *out, TraceRecorder& _recorder):
        LirWriter(out), recorder(_recorder)
    {
    }

    LInsp ins1(LOpcode v, LInsp s0)
    {
        switch (v) {
        case LIR_i2f:
            if (s0->oprnd1()->isCall() && s0->imm8() == F_doubleToInt32)
                return callArgN(s0->oprnd1(), 1);
            break;
        case LIR_u2f:
            if (s0->oprnd1()->isCall() && s0->imm8() == F_doubleToUint32)
                return callArgN(s0->oprnd1(), 1);
            break;
        default:;
        }
        return out->ins1(v, s0);
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
        } else if (v == LIR_fadd || v == LIR_fsub || v == LIR_fmul) {
            if (isPromoteInt(s0) && isPromoteInt(s1)) {
                // demote fop to op
                v = (LOpcode)((int)v & ~LIR64);
                LIns* result = out->ins2(v, demote(out, s1), demote(out, s0));
                out->insGuard(LIR_xt, out->ins1(LIR_ov, result), recorder.snapshot());
                return out->ins1(LIR_i2f, result);
            }
        }
        return out->ins2(v, s1, s0);
    }

    LInsp insCall(int32_t fid, LInsp args[])
    {
        LInsp s0 = args[0];
        switch (fid) {
        case F_doubleToInt32:
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
                LInsp i = s0->oprnd1();
                return out->insCall(F_BoxInt32, &i);
            }
            break;
        }
        return out->insCall(fid, args);
    }
};

class ExitFilter: public LirWriter
{
    TraceRecorder& recorder;
public:
    ExitFilter(LirWriter *out, TraceRecorder& _recorder):
        LirWriter(out), recorder(_recorder)
    {
    }

    /* Determine the type of a store by looking at the current type of the actual value the
       interpreter is using. For numbers we have to check what kind of store we used last
       (integer or double) to figure out what the side exit show reflect in its typemap. */
    int getStoreType(jsval& v) {
        int t = isNumber(v)
            ? (recorder.get(&v)->isQuad() ? JSVAL_DOUBLE : JSVAL_INT)
            : JSVAL_TAG(v);
#ifdef DEBUG
         printf("%c", "OID?S?B"[t]);
#endif
         return t;
    }

    /* Write out a type map for the current scopes and all outer scopes,
       up until the entry scope. */
    void
    buildExitMap(JSStackFrame* fp, JSFrameRegs& regs, uint8* m)
    {
#ifdef DEBUG
        printf("side exit type map: ");
#endif
        JSStackFrame* global = recorder.getGlobalFrame();
        for (unsigned n = 0; n < global->script->ngvars; ++n)
            *m++ = (global->vars[n] != JSVAL_NULL)
                ? getStoreType(STOBJ_GET_SLOT(global->varobj, 
                        (uint32)JSVAL_TO_INT(global->vars[n])))
                : TYPEMAP_TYPE_ANY;
        if (fp != recorder.getEntryFrame())
            buildExitMap(fp->down, *fp->down->regs, m);
        if (fp->down) {
            for (unsigned n = 0; n < fp->argc; ++n)
                *m++ = getStoreType(fp->argv[n]);
            for (unsigned n = 0; n < fp->nvars; ++n)
                *m++ = getStoreType(fp->vars[n]);
        }
        for (jsval* sp = fp->spbase; sp < regs.sp; ++sp)
            *m++ = getStoreType(*sp);
#ifdef DEBUG
        printf("\n");
#endif
    }

    virtual LInsp insGuard(LOpcode v, LIns *c, SideExit *x) {
        VMSideExitInfo* i = (VMSideExitInfo*)x->vmprivate;
        buildExitMap(recorder.getFp(), recorder.getRegs(), i->typeMap);
        return out->insGuard(v, c, x);
    }

    /* Sink all type casts into the stack into the side exit by simply storing the original
       (uncasted) value. Each guard generates the side exit map based on the types of the
       last stores to every stack location, so its safe to not perform them on-trace. */
    virtual LInsp insStore(LIns* value, LIns* base, LIns* disp) {
        if (base == recorder.getFragment()->sp && isPromote(value))
            value = demote(out, value);
        return out->insStore(value, base, disp);
    }

    virtual LInsp insStorei(LIns* value, LIns* base, int32_t d) {
        if (base == recorder.getFragment()->sp && isPromote(value))
            value = demote(out, value);
        return out->insStorei(value, base, d);
    }
};

TraceRecorder::TraceRecorder(JSContext* cx, Fragmento* fragmento, Fragment* _fragment)
{
    this->cx = cx;
    JSStackFrame* global = cx->fp;
    while (global->down)
        global = global->down;
    this->global = global;
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
    lir = exit_filter = new (&gc) ExitFilter(lir, *this);
    lir = func_filter = new (&gc) FuncFilter(lir, *this);
    lir->ins0(LIR_trace);
    if (fragment->vmprivate == NULL) {
        /* generate the entry map and stash it in the trace */
        unsigned entryNativeFrameSlots = nativeFrameSlots(entryFrame, entryRegs);
        LIns* data = lir_buf_writer->skip(sizeof(VMFragmentInfo) +
                entryNativeFrameSlots * sizeof(char));
        fragmentInfo = (VMFragmentInfo*)data->payload();
        fragmentInfo->entryNativeFrameSlots = entryNativeFrameSlots;
        fragmentInfo->maxNativeFrameSlots = entryNativeFrameSlots;
        /* build the entry type map */
        uint8* m = fragmentInfo->typeMap;
        for (unsigned n = 0; n < global->script->ngvars; ++n)
            *m++ = (global->vars[n] != JSVAL_NULL)
                ? getCoercedType(STOBJ_GET_SLOT(global->varobj, 
                        (uint32)JSVAL_TO_INT(global->vars[n])))
                : TYPEMAP_TYPE_ANY;
        if (entryFrame->down) {
            for (unsigned n = 0; n < entryFrame->argc; ++n)
                *m++ = getCoercedType(entryFrame->argv[n]);
            for (unsigned n = 0; n < entryFrame->nvars; ++n)
                *m++ = getCoercedType(entryFrame->vars[n]);
        }
        for (jsval* sp = entryFrame->spbase; sp < entryRegs.sp; ++sp)
            *m++ = getCoercedType(*sp);
        fragmentInfo->nativeStackBase = nativeFrameOffset(&cx->fp->spbase[0]);
    } else {
        fragmentInfo = (VMFragmentInfo*)fragment->vmprivate;
    }
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
    uint8* m = fragmentInfo->typeMap;
    
    for (unsigned n = 0; n < (unsigned)global->script->ngvars; ++n, ++m)
        if (global->vars[n] != JSVAL_NULL) 
            import(&STOBJ_GET_SLOT(global->varobj, (uint32)JSVAL_TO_INT(global->vars[n])), 
                    *m, "gvar", n);
    if (entryFrame->down) {
        for (n = 0; n < fp->argc; ++n, ++m)
            import(&fp->argv[n], *m, "arg", n);
        for (n = 0; n < fp->nvars; ++n)
            import(&fp->vars[n], *m, "var", n);
    }
    for (n = 0; n < unsigned(fp->regs->sp - fp->spbase); ++n, ++m)
        import(&fp->spbase[n], *m, "stack", n);

    recompileFlag = false;
}

TraceRecorder::~TraceRecorder()
{
#ifdef DEBUG
    delete lirbuf->names;
    delete verbose_filter;
#endif
    delete cse_filter;
    delete expr_filter;
    delete exit_filter;
    delete func_filter;
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

/* Determine whether an address is part of a currently active frame (or the global scope). */
bool
TraceRecorder::onFrame(void* p) const
{
    return isGlobal(p) || findFrame(p) != NULL;
}

/* Determine whether an address points to a global variable (gvar). */
bool
TraceRecorder::isGlobal(void* p) const
{
    JSObject* varobj = global->varobj;
    /* has to be in either one of the fslots or dslots of varobj */
    if ((p >= varobj->fslots) && (p < varobj->fslots + JS_INITIAL_NSLOTS))
        return true;
    return (varobj->dslots &&
            p >= varobj->dslots && (p < varobj->dslots + STOBJ_NSLOTS(varobj) - JS_INITIAL_NSLOTS));
}

/* Calculate the total number of native frame slots we need from this frame
   all the way back to the entry frame, including the current stack usage. */
unsigned
TraceRecorder::nativeFrameSlots(JSStackFrame* fp, JSFrameRegs& regs) const
{
    unsigned slots = global->script->ngvars;
    for (;;) {
        slots += (regs.sp - fp->spbase);
        if (fp->down)
            slots += fp->argc + fp->nvars;
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
    if (isGlobal(p)) {
        JSObject* varobj = global->varobj;
        /* Its not safe to pull in globals in heavy-weight functions */
        JS_ASSERT(varobj->fslots[JSSLOT_PARENT] == JSVAL_NULL);
        if (vp >= varobj->fslots && vp < varobj->fslots + JS_INITIAL_NSLOTS)
            return size_t(vp - varobj->fslots) * sizeof(double);
        if (vp >= varobj->dslots && vp < varobj->dslots + 
                STOBJ_NSLOTS(varobj) - JS_INITIAL_NSLOTS)
            return size_t(vp - varobj->dslots + JS_INITIAL_NSLOTS) * sizeof(double);
        JS_NOT_REACHED("nativeFrameOffset");
    }
    /* Globals sit at the very beginning for the native frame, before all the values
       on each frame (starting with the entry frame.) So skip over the frames in between
       the frame we find the value on, and also over the globals. */
    size_t offset = global->script->ngvars;
    JSStackFrame* fp = findFrame(p);
    JS_ASSERT(fp != NULL); // must be on the frame somewhere
    for (JSStackFrame* fp2 = fp; fp2 != entryFrame; fp2 = fp2->down)
        offset += (fp2->argc + fp2->nvars + size_t(fp2->regs->sp - fp2->spbase));
    if ((vp >= fp->argv) && (vp < fp->argv + fp->argc))
        return (offset + size_t(vp - fp->argv)) * sizeof(double);
    offset += fp->argc;
    // FIXME: fixing bug 441686 collapses the vars and spbase cases
    if ((vp >= fp->vars) && (vp < fp->vars + fp->nvars))
        return (offset + size_t(vp - fp->vars)) * sizeof(double);
    offset += fp->nvars;
    JS_ASSERT((vp >= fp->spbase) && (vp < fp->spbase + fp->script->depth));
    return (offset + size_t(vp - fp->spbase)) * sizeof(double);
}

/* Track the maximum number of native frame slots we need during
   execution. */
void
TraceRecorder::trackNativeFrameUse(unsigned slots)
{
    if (slots > fragmentInfo->maxNativeFrameSlots)
        fragmentInfo->maxNativeFrameSlots = slots;
}

/* Unbox a jsval into a slot. Slots are wide enough to hold double values
   directly (instead of storing a pointer to them). */
static bool
unbox_jsval(jsval v, uint8 t, double* slot)
{
    jsuint type = TYPEMAP_GET_TYPE(t);
    if (type == TYPEMAP_TYPE_ANY)
        return true;
    if (type == JSVAL_INT) {
        jsint i;
        if (JSVAL_IS_INT(v))
            *(jsint*)slot = JSVAL_TO_INT(v);
        else if (JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(v), i))
            *(jsint*)slot = i;
        else
            return false;
        return true;
    }
    if (type == JSVAL_DOUBLE) {
        jsdouble d;
        if (JSVAL_IS_INT(v))
            d = JSVAL_TO_INT(v);
        else if (JSVAL_IS_DOUBLE(v))
            d = *JSVAL_TO_DOUBLE(v);
        else
            return false;
        *(jsdouble*)slot = d;
        return true;
    }
    if (JSVAL_TAG(v) != type)
        return false;
    switch (JSVAL_TAG(v)) {
    case JSVAL_BOOLEAN:
        *(bool*)slot = JSVAL_TO_BOOLEAN(v);
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
box_jsval(JSContext* cx, jsval& v, uint8 t, double* slot)
{
    jsuint type = TYPEMAP_GET_TYPE(t);
    if (type == TYPEMAP_TYPE_ANY)
        return true;
    jsint i;
    jsdouble d;
    switch (type) {
      case JSVAL_BOOLEAN:
        v = BOOLEAN_TO_JSVAL(*(bool*)slot);
        break;
      case JSVAL_INT:
        i = *(jsint*)slot;
      store_int:
        if (INT_FITS_IN_JSVAL(i)) {
            v = INT_TO_JSVAL(i);
            break;
        }
        d = (jsdouble)i;
        goto store_double;
      case JSVAL_DOUBLE:
        d = *slot;
        if (JSDOUBLE_IS_INT(d, i))
            goto store_int;
      store_double:
        /* GC is not allowed to hit as we come out of the native frame. We have to teach
           the GC how to scan native frames to avoid this race condition. */
        JS_ASSERT(cx->doubleFreeList != NULL);
        return js_NewDoubleInRootedValue(cx, d, &v);
      case JSVAL_STRING:
        v = STRING_TO_JSVAL(*(JSString**)slot);
        break;
      default:
        JS_ASSERT(t == JSVAL_OBJECT);
        v = OBJECT_TO_JSVAL(*(JSObject**)slot);
        break;
    }
    return true;
}

/* Attempt to unbox the given JS frame into a native frame, checking along the way that the
   supplied typemap holds. */
static bool
unbox(JSStackFrame* fp, JSFrameRegs& regs, uint8* m, double* native)
{
    JSStackFrame* global = fp;
    while (global->down)
        global = global->down;
    JSObject* varobj = global->varobj;
    unsigned ngvars = global->script->ngvars;
    for (unsigned n = 0; n < ngvars; ++n) {
        jsval slotval = global->vars[n];
        if (!JSVAL_IS_INT(slotval))
            continue;
        /* we don't get here if the global object is not owned by this context */
        if (!unbox_jsval(STOBJ_GET_SLOT(varobj, (uint32)JSVAL_TO_INT(slotval)), 
                *m++, native++))
            return false;
    }
    // TODO: handle deep bailouts
    jsval* vp;
    if (fp->down) {
        for (vp = fp->argv; vp < fp->argv + fp->argc; ++vp)
            if (!unbox_jsval(*vp, *m++, native++))
                return false;
        for (vp = fp->vars; vp < fp->vars + fp->nvars; ++vp)
            if (!unbox_jsval(*vp, *m++, native++))
                return false;
    }
    for (vp = fp->spbase; vp < regs.sp; ++vp)
        if (!unbox_jsval(*vp, *m++, native++))
            return false;
    return true;
}

/* Box the given native frame into a JS frame. This only fails due to a hard error
   (out of memory for example). */
static bool
box(JSContext* cx, JSStackFrame* fp, JSFrameRegs& regs, uint8* m, double* native)
{
    JSStackFrame* global = fp;
    while (global->down)
        global = global->down;
    JSObject* varobj = global->varobj;
    unsigned ngvars = global->script->ngvars;
    unsigned n;
    for (n = 0; n < ngvars; ++n)
        if (!box_jsval(cx, STOBJ_GET_SLOT(varobj, n), *m++, native++))
            return false;
    jsval* vp;
    if (fp->down) {
        for (vp = fp->argv; vp < fp->argv + fp->argc; ++vp)
            if (!box_jsval(cx, *vp, *m++, native++))
                return false;
        for (vp = fp->vars; vp < fp->vars + fp->nvars; ++vp)
            if (!box_jsval(cx, *vp, *m++, native++))
                return false;
    }
    for (vp = fp->spbase; vp < regs.sp; ++vp)
        if (!box_jsval(cx, *vp, *m++, native++))
            return false;
    return true;
}

/* Emit load instructions onto the trace that read the initial stack state. */
void
TraceRecorder::import(jsval* p, uint8& t, char *prefix, int index)
{
    JS_ASSERT(TYPEMAP_GET_TYPE(t) != TYPEMAP_TYPE_ANY);
    JS_ASSERT(onFrame(p));
    LIns* ins;
    /* Calculate the offset of this slot relative to the entry stack-pointer value of the
       native stack. Arguments and locals are to the left of the stack pointer (offset
       less than 0). Stack cells start at offset 0. Ed defined the semantics of the stack,
       not me, so don't blame the messenger. */
    ssize_t offset = -fragmentInfo->nativeStackBase + nativeFrameOffset(p) + 8;
    if (TYPEMAP_GET_TYPE(t) == JSVAL_INT) { /* demoted */
        JS_ASSERT(isInt32(*p));
        /* Ok, we have a valid demotion attempt pending, so insert an integer
           read and promote it to double since all arithmetic operations expect
           to see doubles on entry. The first op to use this slot will emit a
           f2i cast which will cancel out the i2f we insert here. */
        ins = lir->ins1(LIR_i2f, lir->insLoadi(fragment->sp, offset));
    } else {
        JS_ASSERT(isNumber(*p) == (TYPEMAP_GET_TYPE(t) == JSVAL_DOUBLE));
        ins = lir->insLoad(t == JSVAL_DOUBLE ? LIR_ldq : LIR_ld, fragment->sp, offset);
    }
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

JSStackFrame* 
TraceRecorder::getGlobalFrame() const
{
    return global;
}

JSStackFrame*
TraceRecorder::getEntryFrame() const
{
    return entryFrame;
}

JSStackFrame*
TraceRecorder::getFp() const
{
    return cx->fp;
}

JSFrameRegs&
TraceRecorder::getRegs() const
{
    return *cx->fp->regs;
}

Fragment*
TraceRecorder::getFragment() const
{
    return fragment;
}

SideExit*
TraceRecorder::snapshot()
{
    /* generate the entry map and stash it in the trace */
    unsigned slots = nativeFrameSlots(cx->fp, *cx->fp->regs);
    trackNativeFrameUse(slots);
    /* reserve space for the type map, ExitFilter will write it out for us */
    LIns* data = lir_buf_writer->skip(sizeof(VMSideExitInfo) + slots * sizeof(char));
    VMSideExitInfo* si = (VMSideExitInfo*)data->payload();
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

bool
TraceRecorder::checkType(jsval& v, uint8& t)
{
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
                    printf("demoting type of an entry slot #%d, triggering re-compilation\n",
                            nativeFrameOffset(&v));
#endif
                    JS_ASSERT(!TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DEMOTE) ||
                            TYPEMAP_GET_FLAG(t, TYPEMAP_FLAG_DONT_DEMOTE));
                    TYPEMAP_SET_FLAG(t, TYPEMAP_FLAG_DEMOTE);
                    TYPEMAP_SET_TYPE(t, JSVAL_INT);
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
#ifdef DEBUG
            printf("demoting type of a slot #%d failed, locking it and re-compiling\n",
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
TraceRecorder::verifyTypeStability(JSStackFrame* fp, JSFrameRegs& regs, uint8* m)
{
    if (fp != entryFrame)
        verifyTypeStability(fp->down, *fp->down->regs, m);
    else {
        /* global variables sit at the very beginning of the native frame */
        JSObject* varobj = global->varobj;
        unsigned ngvars = global->script->ngvars;
        unsigned n;
        for (n = 0; n < ngvars; ++n)
            if (!checkType(STOBJ_GET_SLOT(varobj, n), *m++)) {
#ifdef DEBUG
                printf(" (gvar %d)\n", n);
#endif
                return false;
            }
    }
    if (fp->down) {
        for (unsigned n = 0; n < fp->argc; ++n, ++m)
            if (!checkType(fp->argv[n], *m)) {
#ifdef DEBUG
                printf(" (arg %d)\n", n);
#endif
                return false;
            }
        for (unsigned n = 0; n < fp->nvars; ++n, ++m)
            if (!checkType(fp->vars[n], *m)) {
#ifdef DEBUG
                printf(" (var %d)\n", n);
#endif
                return false;
            }
    }
    for (jsval* sp = fp->spbase; sp < regs.sp; ++sp, ++m)
        if (!checkType(*sp, *m))
            return false;
    return !recompileFlag;
}

void
TraceRecorder::closeLoop(Fragmento* fragmento)
{
    if (!verifyTypeStability(entryFrame, entryRegs, fragmentInfo->typeMap)) {
#ifdef DEBUG
        printf("Trace rejected: unstable loop variables.\n");
#endif
        return;
    }
    fragment->lastIns = lir->ins0(LIR_loop);
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

#define HOTLOOP1 10
#define HOTLOOP2 13
#define HOTLOOP3 37

bool
js_LoopEdge(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

#ifdef JS_THREADSAFE    
    if (GET_SCOPE(varobj)->title.owner_cx != cx) {
#ifdef DEBUG        
        printf("Global object not owned by this context.\n");
#endif        
        return false; /* we stay away from shared global objects */
    }
#endif    

    /* is the recorder currently active? */
    if (tm->recorder) {
        if (tm->recorder->loopEdge())
            return true; /* keep recording */
        js_DeleteRecorder(cx);
        return false; /* done recording */
    }

    Fragment* f = tm->fragmento->getLoop(cx->fp->regs->pc);
    if (!f->code()) {
        int hits = ++f->hits();
        if (!f->isBlacklisted() && hits >= HOTLOOP1) {
            if (hits == HOTLOOP1 || hits == HOTLOOP2 || hits == HOTLOOP3) {
                tm->recorder = new (&gc) TraceRecorder(cx, tm->fragmento, f);
                return true; /* start recording */
            }
            if (hits > HOTLOOP3)
                f->blacklist();
        }
        return false;
    }

    /* execute previously recorded race */    
    VMFragmentInfo* fi = (VMFragmentInfo*)f->vmprivate;
    double native[fi->maxNativeFrameSlots+1];
#ifdef DEBUG
    *(uint64*)&native[fi->maxNativeFrameSlots] = 0xdeadbeefdeadbeefLL;
#endif
    if (!unbox(cx->fp, *cx->fp->regs, fi->typeMap, native)) {
#ifdef DEBUG
        printf("typemap mismatch, skipping trace.\n");
#endif
        return false;
    }
    double* entry_sp = &native[fi->nativeStackBase/sizeof(double) +
                               (cx->fp->regs->sp - cx->fp->spbase - 1)];
    InterpState state;
    state.ip = cx->fp->regs->pc;
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
TraceRecorder::gvarval(unsigned n) const
{
    JS_ASSERT((n >= 0) && (n < STOBJ_NSLOTS(global->varobj)));
    return STOBJ_GET_SLOT(cx->fp->varobj, n);
}

jsval&
TraceRecorder::argval(unsigned n) const
{
    JS_ASSERT((n >= 0) && (n < cx->fp->argc));
    return cx->fp->argv[n];
}

jsval&
TraceRecorder::varval(unsigned n) const
{
    JS_ASSERT((n >= 0) && (n < cx->fp->nvars));
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
TraceRecorder::gvar(unsigned n)
{
    return get(&gvarval(n));
}

void 
TraceRecorder::gvar(unsigned n, LIns* i)
{
    set(&gvarval(n), i);
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

LIns* TraceRecorder::f2i(LIns* f)
{
    return lir->insCall(F_doubleToInt32, &f);
}

bool TraceRecorder::ifop(bool sense)
{
    jsval& v = stackval(-1);
    LIns* cond_ins;
    bool cond;
    if (JSVAL_IS_BOOLEAN(v)) {
        cond_ins = lir->ins_eq0(get(&v));
        cond = JSVAL_TO_BOOLEAN(v);
    } else {
        return false;
    }

    if (!sense) {
        cond = !cond;
        cond_ins = lir->ins_eq0(cond_ins);
    }
    guard(cond, cond_ins);
    return true;
}

bool
TraceRecorder::inc(jsval& v, jsint incr, bool pre)
{
    if (isNumber(v)) {
        LIns* before = get(&v);
        LIns* after;
        jsdouble d = (jsdouble)incr;
        after = lir->ins2(LIR_fadd, before, lir->insImmq(*(uint64_t*)&d));
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
            JS_ASSERT(cond == LIR_feq);
            cond = asNumber(l) == asNumber(r);
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

bool
TraceRecorder::test_property_cache(JSObject* obj, LIns* obj_ins, JSObject*& obj2,
                                   JSPropCacheEntry*& entry)
{
    LIns* map_ins = lir->insLoadi(obj_ins, offsetof(JSObject, map));
    if (!map_is_native(obj->map, map_ins))
        return false;

    JSAtom* atom;
    PROPERTY_CACHE_TEST(cx, cx->fp->regs->pc, obj, obj2, entry, atom);
    if (atom)
        return false;

    if (PCVCAP_TAG(entry->vcap == 1))
        return false; // need to look in the prototype, NYI

    if (OBJ_SCOPE(obj)->object != obj)
        return false; // need to normalize to the owner of the shared scope, NYI

    LIns* shape_ins = lir->insLoadi(map_ins, offsetof(JSScope, shape));
#ifdef DEBUG
    lirbuf->names->addName(shape_ins, "shape");
#endif
    guard(true, lir->ins2i(LIR_eq, shape_ins, OBJ_SCOPE(obj)->shape));
    return true;
}

void
TraceRecorder::stobj_set_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins)
{
    if (slot < JS_INITIAL_NSLOTS) {
        lir->insStorei(v_ins,
                       obj_ins,
                       offsetof(JSObject, fslots) + slot * sizeof(jsval));
    } else {
        if (!dslots_ins)
            dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
        lir->insStorei(v_ins,
                       dslots_ins,
                       (slot - JS_INITIAL_NSLOTS) * sizeof(jsval));
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
    return false;
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
        v_ins = lir->insImm(JSVAL_VOID);
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
        guard(true, lir->ins_eq0(
                lir->ins_eq0(
                        lir->ins2(LIR_and, v_ins,
                                lir->insImmPtr((void*)(JSVAL_INT | JSVAL_DOUBLE))))));
        v_ins = lir->insCall(F_UnboxDouble, &v_ins);
        return true;
    }
    switch (JSVAL_TAG(v)) {
    case JSVAL_BOOLEAN:
        guard(true, lir->ins2i(LIR_eq, lir->ins2(LIR_and, v_ins, lir->insImmPtr((void*)~JSVAL_TRUE)),
                 JSVAL_BOOLEAN));
         v_ins = lir->ins2i(LIR_ush, v_ins, JSVAL_TAGBITS);
         return true;
    }
    return false;
}

bool TraceRecorder::guardThatObjectIsDenseArray(JSObject* obj, LIns* obj_ins, LIns*& dslots_ins)
{
    if (!OBJ_IS_DENSE_ARRAY(cx, obj))
        return false;
    // guard(OBJ_GET_CLASS(obj) == &js_ArrayClass);
    LIns* class_ins = stobj_get_slot(obj_ins, JSSLOT_CLASS, dslots_ins);
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
    if (!dslots_ins)
        dslots_ins = lir->insLoadi(obj_ins, offsetof(JSObject, dslots));
    LIns* length_ins = stobj_get_slot(obj_ins, JSSLOT_ARRAY_LENGTH, dslots_ins);
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
    stack(0, lir->insImm(JSVAL_VOID));
    return true;
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
    return ifop(true);
}
bool TraceRecorder::JSOP_IFNE()
{
    return ifop(false);
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
    stack(0, get(&stackval(-1)));
    return true;
}
bool TraceRecorder::JSOP_DUP2()
{
    stack(0, get(&stackval(-2)));
    stack(1, get(&stackval(-1)));
    return true;
}
bool TraceRecorder::JSOP_SETCONST()
{
    return false;
}
bool TraceRecorder::JSOP_BITOR()
{
    return binary(LIR_or);
}
bool TraceRecorder::JSOP_BITXOR()
{
    return binary(LIR_xor);
}
bool TraceRecorder::JSOP_BITAND()
{
    return binary(LIR_and);
}
bool TraceRecorder::JSOP_EQ()
{
    return cmp(LIR_feq);
}
bool TraceRecorder::JSOP_NE()
{
    return cmp(LIR_feq, true);
}
bool TraceRecorder::JSOP_LT()
{
    return cmp(LIR_flt);
}
bool TraceRecorder::JSOP_LE()
{
    return cmp(LIR_fle);
}
bool TraceRecorder::JSOP_GT()
{
    return cmp(LIR_fgt);
}
bool TraceRecorder::JSOP_GE()
{
    return cmp(LIR_fge);
}
bool TraceRecorder::JSOP_LSH()
{
    return binary(LIR_lsh);
}
bool TraceRecorder::JSOP_RSH()
{
    return binary(LIR_rsh);
}
bool TraceRecorder::JSOP_URSH()
{
    return binary(LIR_ush);
}
bool TraceRecorder::JSOP_ADD()
{
    return binary(LIR_fadd);
}
bool TraceRecorder::JSOP_SUB()
{
    return binary(LIR_fsub);
}
bool TraceRecorder::JSOP_MUL()
{
    return binary(LIR_fmul);
}
bool TraceRecorder::JSOP_DIV()
{
    return binary(LIR_fdiv);
}
bool TraceRecorder::JSOP_MOD()
{
    return false;
}
bool TraceRecorder::JSOP_NOT()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_BOOLEAN(v)) {
        set(&v, lir->ins_eq0(get(&v)));
        return true;
    }
    return false;
}
bool TraceRecorder::JSOP_BITNOT()
{
    return unary(LIR_not);
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
    if (!JSVAL_IS_INT(r) || JSVAL_IS_PRIMITIVE(l))
        return false;
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
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
    jsval v = obj->dslots[idx];
    /* ok, we can trace this case since we now have the value and thus know the type */
    LIns* addr = lir->ins2(LIR_add, dslots_ins,
            lir->ins2i(LIR_lsh, idx_ins, sizeof(jsval) == 4 ? 2 : 3));
    /* load the value, check the type (need to check JSVAL_HOLE only for booleans) */
    LIns* v_ins = lir->insLoadi(addr, 0);
    if (!unbox_jsval(v, v_ins))
        return false;
    set(&l, v_ins);
    return true;
}
bool TraceRecorder::JSOP_SETELEM()
{
    jsval& v = stackval(-1);
    jsval& r = stackval(-2);
    jsval& l = stackval(-3);
    /* no guards for type checks, trace specialized this already */
    if (!JSVAL_IS_INT(r) || JSVAL_IS_PRIMITIVE(l))
        return false;
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
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
    JSObject* obj;
    JSObject* obj2;
    JSPropCacheEntry* entry;

    LIns* obj_ins = lir->insLoadi(lir->insLoadi(cx_ins, offsetof(JSContext, fp)),
                                  offsetof(JSStackFrame, scopeChain));
    obj = cx->fp->scopeChain;
    if (!test_property_cache(obj, obj_ins, obj2, entry))
        return false;

    if (!PCVAL_IS_SLOT(entry->vword))
        return false;

    LIns* dslots_ins = NULL;
    uint32 slot = PCVAL_TO_SLOT(entry->vword);
    LIns* v_ins = stobj_get_slot(obj_ins, slot, dslots_ins);
    if (!unbox_jsval(STOBJ_GET_SLOT(obj, slot), v_ins))
        return false;

    stack(0, v_ins);
    return true;
}
bool TraceRecorder::JSOP_DOUBLE()
{
    jsval v = (jsval)cx->fp->script->atomMap.vector[GET_INDEX(cx->fp->regs->pc)];
    stack(0, lir->insImmq(*(uint64_t*)JSVAL_TO_DOUBLE(v)));
    return true;
}
bool TraceRecorder::JSOP_STRING()
{
    return false;
}
bool TraceRecorder::JSOP_ZERO()
{
    jsdouble d = (jsdouble)0;
    stack(0, lir->insImmq(*(uint64_t*)&d));
    return true;
}
bool TraceRecorder::JSOP_ONE()
{
    jsdouble d = (jsdouble)1;
    stack(0, lir->insImmq(*(uint64_t*)&d));
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
    return false;
}
bool TraceRecorder::JSOP_AND()
{
    return false;
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
    return true;
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
    jsdouble d = (jsdouble)GET_UINT16(cx->fp->regs->pc);
    stack(0, lir->insImmq(*(uint64_t*)&d));
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
    return true;
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
    return true;
}
bool TraceRecorder::JSOP_CONDSWITCH()
{
    return true;
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
    return true;
}
bool TraceRecorder::JSOP_FINALLY()
{
    return true;
}
bool TraceRecorder::JSOP_NOP()
{
    return true;
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
    return JSOP_IFEQ();
}
bool TraceRecorder::JSOP_IFNEX()
{
    return JSOP_IFNE();
}
bool TraceRecorder::JSOP_ORX()
{
    return JSOP_OR();
}
bool TraceRecorder::JSOP_ANDX()
{
    return JSOP_AND();
}
bool TraceRecorder::JSOP_GOSUBX()
{
    return JSOP_GOSUB();
}
bool TraceRecorder::JSOP_CASEX()
{
    return JSOP_CASE();
}
bool TraceRecorder::JSOP_DEFAULTX()
{
    return JSOP_DEFAULT();
}
bool TraceRecorder::JSOP_TABLESWITCHX()
{
    return JSOP_TABLESWITCH();
}
bool TraceRecorder::JSOP_LOOKUPSWITCHX()
{
    return JSOP_LOOKUPSWITCH();
}
bool TraceRecorder::JSOP_BACKPATCH()
{
    return true;
}
bool TraceRecorder::JSOP_BACKPATCH_POP()
{
    return true;
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
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.
    uint32 slot = JSVAL_TO_INT(slotval);
    stack(0, gvar(slot));
    return true;
}
bool TraceRecorder::JSOP_SETGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.
    uint32 slot = JSVAL_TO_INT(slotval);
    gvar(slot, stack(-1));
    return true;
}
bool TraceRecorder::JSOP_INCGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(gvarval(slot), 1, true);
}
bool TraceRecorder::JSOP_DECGVAR()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(gvarval(slot), -1, true);
}
bool TraceRecorder::JSOP_GVARINC()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(gvarval(slot), 1, false);
}
bool TraceRecorder::JSOP_GVARDEC()
{
    jsval slotval = cx->fp->vars[GET_VARNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
    uint32 slot = JSVAL_TO_INT(slotval);
    return inc(gvarval(slot), -1, false);
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
    jsdouble d = (jsdouble) GET_UINT24(cx->fp->regs->pc);
    stack(0, lir->insImmq(*(uint64_t*)&d));
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
    return true;
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
    jsdouble d = (jsdouble)GET_INT8(cx->fp->regs->pc);
    stack(0, lir->insImmq(*(uint64_t*)&d));
    return true;
}
bool TraceRecorder::JSOP_INT32()
{
    jsdouble d = (jsdouble)GET_INT32(cx->fp->regs->pc);
    stack(0, lir->insImmq(*(uint64_t*)&d));
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
    stack(0, lir->insImm(JSVAL_HOLE));
    return true;
}
