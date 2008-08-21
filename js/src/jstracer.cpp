/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
#include "jsbit.h"              // low-level (NSPR-based) headers next
#include "jsprf.h"
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
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jstracer.h"

#include "jsautooplen.h"        // generated headers last

/* Number of iterations of a loop before we start tracing. */
#define HOTLOOP 2

/* Number of times we wait to exit on a side exit before we try to extend the tree. */
#define HOTEXIT 1

/* Max call depths for inlining. */
#define MAX_CALLDEPTH 5

/* Max number of type mismatchs before we trash the tree. */
#define MAX_MISMATCH 5

/* Max native stack size. */
#define MAX_NATIVE_STACK_SLOTS 1024

/* Max call stack size. */
#define MAX_CALL_STACK_ENTRIES 64

#ifdef DEBUG
#define ABORT_TRACE(msg)   do { debug_only_v(fprintf(stdout, "abort: %d: %s\n", __LINE__, msg);)  return false; } while (0)
#else
#define ABORT_TRACE(msg)   return false
#endif

#ifdef DEBUG
static struct {
    uint64
        recorderStarted, recorderAborted, traceCompleted, sideExitIntoInterpreter,
        typeMapMismatchAtEntry, returnToDifferentLoopHeader, traceTriggered,
        globalShapeMismatchAtEntry, treesTrashed, slotPromoted,
        unstableLoopVariable;
} stat = { 0LL, };
#define AUDIT(x) (stat.x++)
#else
#define AUDIT(x) ((void)0)
#endif

#define INS_CONST(c)    addName(lir->insImm(c), #c)
#define INS_CONSTPTR(p) addName(lir->insImmPtr((void*) (p)), #p)

using namespace avmplus;
using namespace nanojit;

static GC gc = GC();
static avmplus::AvmCore s_core = avmplus::AvmCore();
static avmplus::AvmCore* core = &s_core;

/* We really need a better way to configure the JIT. Shaver, where is my fancy JIT object? */
static bool nesting_enabled = true;
static bool oracle_enabled = true;

#ifdef DEBUG
static bool verbose_debug = getenv("TRACEMONKEY") && strstr(getenv("TRACEMONKEY"), "verbose");
#define debug_only_v(x) if (verbose_debug) { x; }
#else
#define debug_only_v(x)
#endif

/* The entire VM shares one oracle. Collisions and concurrent updates are tolerated and worst
   case cause performance regressions. */
static Oracle oracle;

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
    return get(v) != NULL;
}

#if defined NANOJIT_64BIT
#define PAGEMASK	0x7ff
#else
#define PAGEMASK	0xfff
#endif

LIns*
Tracker::get(const void* v) const
{
    struct Tracker::Page* p = findPage(v);
    if (!p)
        return NULL;
    return p->map[(jsuword(v) & PAGEMASK) >> 2];
}

void
Tracker::set(const void* v, LIns* i)
{
    struct Tracker::Page* p = findPage(v);
    if (!p)
        p = addPage(v);
    p->map[(jsuword(v) & PAGEMASK) >> 2] = i;
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

static inline uint8 getCoercedType(jsval v)
{
    return isInt32(v) ? JSVAL_INT : JSVAL_TAG(v);
}

/* Tell the oracle that a certain global variable should not be demoted. */
void
Oracle::markGlobalSlotUndemotable(JSScript* script, unsigned slot)
{
    _dontDemote.set(&gc, (slot % ORACLE_SIZE));
}

/* Consult with the oracle whether we shouldn't demote a certain global variable. */
bool
Oracle::isGlobalSlotUndemotable(JSScript* script, unsigned slot) const
{
    return !oracle_enabled || _dontDemote.get(slot % ORACLE_SIZE);
}

/* Tell the oracle that a certain slot at a certain bytecode location should not be demoted. */
void
Oracle::markStackSlotUndemotable(JSScript* script, jsbytecode* ip, unsigned slot)
{
    uint32 hash = uint32(intptr_t(ip)) + (slot << 5);
    hash %= ORACLE_SIZE;
    _dontDemote.set(&gc, hash);
}

/* Consult with the oracle whether we shouldn't demote a certain slot. */
bool
Oracle::isStackSlotUndemotable(JSScript* script, jsbytecode* ip, unsigned slot) const
{
    uint32 hash = uint32(intptr_t(ip)) + (slot << 5);
    hash %= ORACLE_SIZE;
    return !oracle_enabled || _dontDemote.get(hash);
}

/* Clear the oracle. */
void
Oracle::clear()
{
    _dontDemote.reset();
}

static LIns* demote(LirWriter *out, LInsp i)
{
    if (i->isCall())
        return callArgN(i,0);
    if (i->isop(LIR_i2f) || i->isop(LIR_u2f))
        return i->oprnd1();
    if (i->isconst())
        return i;
    AvmAssert(i->isconstq());
    double cf = i->constvalf();
    int32_t ci = cf > 0x7fffffff ? uint32_t(cf) : int32_t(cf);
    return out->insImm(ci);
}

static bool isPromoteInt(LIns* i)
{
    jsdouble d;
    return i->isop(LIR_i2f) || i->isconst() ||
        (i->isconstq() && ((d = i->constvalf()) == (jsdouble)(jsint)d));
}

static bool isPromoteUint(LIns* i)
{
    jsdouble d;
    return i->isop(LIR_u2f) || i->isconst() ||
        (i->isconstq() && ((d = i->constvalf()) == (jsdouble)(jsuint)d));
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
                  out->insGuard(LIR_xt, out->ins1(LIR_ov, result),
                                recorder.snapshot(OVERFLOW_EXIT));
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
                if (!overflowSafe(d0) || !overflowSafe(d1)) {
                    out->insGuard(LIR_xt, out->ins1(LIR_ov, result),
                                  recorder.snapshot(OVERFLOW_EXIT));
                }
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
          case F_DoubleToUint32:
            if (s0->isconstq())
                return out->insImm(js_DoubleToECMAUint32(s0->constvalf()));
            if (s0->isop(LIR_i2f) || s0->isop(LIR_u2f)) {
                return s0->oprnd1();
            }
            break;
          case F_DoubleToInt32:
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
            if (s0->isCall() && s0->fid() == F_UnboxDouble) {
                LIns* args2[] = { callArgN(s0, 0) };
                return out->insCall(F_UnboxInt32, args2);
            }
            if (s0->isCall() && s0->fid() == F_StringToNumber) {
                // callArgN's ordering is that as seen by the builtin, not as stored in args here.
                // True story!
                LIns* args2[] = { callArgN(s0, 1), callArgN(s0, 0) };
                return out->insCall(F_StringToInt32, args2);
            }
            break;
          case F_BoxDouble:
            JS_ASSERT(s0->isQuad());
            if (s0->isop(LIR_i2f)) {
                LIns* args2[] = { s0->oprnd1(), args[1] };
                return out->insCall(F_BoxInt32, args2);
            }
            if (s0->isCall() && s0->fid() == F_UnboxDouble) 
                return callArgN(s0, 0);
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

/* Iterate over all interned global variables. */
#define FORALL_GLOBAL_SLOTS(cx, ngslots, gslots, code)                        \
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
    JS_END_MACRO

/* Iterate over all slots in the frame, consisting of args, vars, and stack
   (except for the top-level frame which does not have args or vars. */
#define FORALL_FRAME_SLOTS(fp, depth, code)                                   \
    JS_BEGIN_MACRO                                                            \
        jsval* vp;                                                            \
        jsval* vpstop;                                                        \
        if (fp->callee) {                                                     \
            if (depth == 0) {                                                 \
                SET_VPNAME("callee");                                         \
                vp = &fp->argv[-2];                                           \
                { code; }                                                     \
                SET_VPNAME("this");                                           \
                vp = &fp->argv[-1];                                           \
                { code; }                                                     \
                SET_VPNAME("argv");                                           \
                unsigned nargs = JS_MAX(fp->fun->nargs, fp->argc);            \
                vp = &fp->argv[0]; vpstop = &fp->argv[nargs];                 \
                while (vp < vpstop) { code; ++vp; INC_VPNUM(); }              \
            }                                                                 \
            SET_VPNAME("vars");                                               \
            vp = fp->slots; vpstop = &fp->slots[fp->script->nfixed];          \
            while (vp < vpstop) { code; ++vp; INC_VPNUM(); }                  \
        }                                                                     \
        SET_VPNAME("stack");                                                  \
        vp = StackBase(fp); vpstop = fp->regs->sp;                            \
        while (vp < vpstop) { code; ++vp; INC_VPNUM(); }                      \
        if (fsp < fspstop - 1) {                                              \
            JSStackFrame* fp2 = fsp[1];                                       \
            int missing = fp2->fun->nargs - fp2->argc;                        \
            if (missing > 0) {                                                \
                SET_VPNAME("missing");                                        \
                vp = fp->regs->sp;                                            \
                vpstop = vp + missing;                                        \
                while (vp < vpstop) { code; ++vp; INC_VPNUM(); }              \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

/* Iterate over all slots in each pending frame. */
#define FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth, code)                   \
    JS_BEGIN_MACRO                                                            \
        DEF_VPNAME;                                                           \
        unsigned n;                                                           \
        JSStackFrame* currentFrame = cx->fp;                                  \
        JSStackFrame* entryFrame;                                             \
        JSStackFrame* fp = currentFrame;                                      \
        for (n = 0; n < callDepth; ++n) { fp = fp->down; }                    \
        entryFrame = fp;                                                      \
        unsigned frames = callDepth+1;                                        \
        JSStackFrame** fstack =                                               \
            (JSStackFrame**) alloca(frames * sizeof (JSStackFrame*));         \
        JSStackFrame** fspstop = &fstack[frames];                             \
        JSStackFrame** fsp = fspstop-1;                                       \
        fp = currentFrame;                                                    \
        for (;; fp = fp->down) { *fsp-- = fp; if (fp == entryFrame) break; }  \
        unsigned depth;                                                       \
        for (depth = 0, fsp = fstack; fsp < fspstop; ++fsp, ++depth) {        \
            fp = *fsp;                                                        \
            FORALL_FRAME_SLOTS(fp, depth, code);                              \
        }                                                                     \
    JS_END_MACRO

#define FORALL_SLOTS(cx, ngslots, gslots, callDepth, code)                    \
    JS_BEGIN_MACRO                                                            \
        FORALL_GLOBAL_SLOTS(cx, ngslots, gslots, code);                       \
        FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth, code);                  \
    JS_END_MACRO

/* Calculate the total number of native frame slots we need from this frame
   all the way back to the entry frame, including the current stack usage. */
static unsigned
nativeStackSlots(JSContext *cx, unsigned callDepth)
{
    JSStackFrame* fp = cx->fp;
    unsigned slots = 0;
#if defined _DEBUG
    unsigned int origCallDepth = callDepth;
#endif
    for (;;) {
        unsigned operands = fp->regs->sp - StackBase(fp);
        JS_ASSERT(operands <= unsigned(fp->script->nslots - fp->script->nfixed));
        slots += operands;
        if (fp->callee)
            slots += fp->script->nfixed;
        if (callDepth-- == 0) {
            if (fp->callee) {
                unsigned nargs = JS_MAX(fp->fun->nargs, fp->argc);
                slots += 2/*callee,this*/ + nargs;
            }
#if defined _DEBUG
            unsigned int m = 0;
            FORALL_SLOTS_IN_PENDING_FRAMES(cx, origCallDepth, m++);
            JS_ASSERT(m == slots);
#endif
            return slots;
        }
        JSStackFrame* fp2 = fp;
        fp = fp->down;
        int missing = fp2->fun->nargs - fp2->argc;
        if (missing > 0)
            slots += missing;
    }
    JS_NOT_REACHED("nativeStackSlots");
}

/* Capture the type map for the selected slots of the global object. */
void
TypeMap::captureGlobalTypes(JSContext* cx, SlotList& slots)
{
    unsigned ngslots = slots.length();
    uint16* gslots = slots.data();
    setLength(ngslots);
    uint8* map = data();
    uint8* m = map;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        uint8 type = getCoercedType(*vp);
        if ((type == JSVAL_INT) && oracle.isGlobalSlotUndemotable(cx->fp->script, gslots[n]))
            type = JSVAL_DOUBLE;
        *m++ = type;
    );
}

/* Capture the type map for the currently pending stack frames. */
void
TypeMap::captureStackTypes(JSContext* cx, unsigned callDepth)
{
    setLength(nativeStackSlots(cx, callDepth));
    uint8* map = data();
    uint8* m = map;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        uint8 type = getCoercedType(*vp);
        if ((type == JSVAL_INT) &&
            oracle.isStackSlotUndemotable(cx->fp->script, cx->fp->regs->pc, unsigned(m - map))) {
            type = JSVAL_DOUBLE;
        }
        *m++ = type;
    );
}

/* Compare this type map to another one and see whether they match. */
bool
TypeMap::matches(TypeMap& other) const
{
    if (length() != other.length())
        return false;
    return !memcmp(data(), other.data(), length());
}

/* Use the provided storage area to create a new type map that contains the partial type map
   with the rest of it filled up from the complete type map. */
static void
mergeTypeMaps(uint8** partial, unsigned* plength, uint8* complete, unsigned clength, uint8* mem)
{
    unsigned l = *plength;
    JS_ASSERT(l < clength);
    memcpy(mem, *partial, l * sizeof(uint8));
    memcpy(mem + l, complete + l, (clength - l) * sizeof(uint8));
    *partial = mem;
    *plength = clength;
}

TraceRecorder::TraceRecorder(JSContext* cx, GuardRecord* _anchor, Fragment* _fragment,
        TreeInfo* ti, unsigned ngslots, uint8* globalTypeMap, uint8* stackTypeMap,
        GuardRecord* innermostNestedGuard)
{
    JS_ASSERT(!_fragment->vmprivate && ti);

    this->cx = cx;
    this->traceMonitor = &JS_TRACE_MONITOR(cx);
    this->globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    this->anchor = _anchor;
    this->fragment = _fragment;
    this->lirbuf = _fragment->lirbuf;
    this->treeInfo = ti;
    this->callDepth = _fragment->calldepth;
    JS_ASSERT(!_anchor || _anchor->calldepth == _fragment->calldepth);
    this->atoms = cx->fp->script->atomMap.vector;

    
    debug_only_v(printf("recording starting from %s:%u@%u\n", cx->fp->script->filename,
                        js_PCToLineNumber(cx, cx->fp->script, cx->fp->regs->pc),
                        cx->fp->regs->pc - cx->fp->script->code););

    lir = lir_buf_writer = new (&gc) LirBufWriter(lirbuf);
#ifdef DEBUG
    if (verbose_debug)
        lir = verbose_filter = new (&gc) VerboseWriter(&gc, lir, lirbuf->names);
#endif
    lir = cse_filter = new (&gc) CseFilter(lir, &gc);
    lir = expr_filter = new (&gc) ExprFilter(lir);
    lir = func_filter = new (&gc) FuncFilter(lir, *this);
    lir->ins0(LIR_trace);

    if (!nanojit::AvmCore::config.tree_opt || fragment->root == fragment) {
        lirbuf->state = addName(lir->insParam(0), "state");
        lirbuf->param1 = addName(lir->insParam(1), "param1");
    }
    lirbuf->sp = addName(lir->insLoad(LIR_ldp, lirbuf->state, (int)offsetof(InterpState, sp)), "sp");
    lirbuf->rp = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, rp)), "rp");
    cx_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, cx)), "cx");
    gp_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, gp)), "gp");
    eos_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, eos)), "eos");
    eor_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, eor)), "eor");

    /* read into registers all values on the stack and all globals we know so far */
    import(treeInfo, lirbuf->sp, ngslots, callDepth, globalTypeMap, stackTypeMap);

    /* If we are attached to a tree call guard, make sure the guard the inner tree exited from
       is what we expect it to be. */
    if (_anchor && _anchor->exit->exitType == NESTED_EXIT) {
        LIns* nested_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, 
                                                offsetof(InterpState, nestedExit)), "nestedExit");
        guard(true, lir->ins2(LIR_eq, nested_ins, lir->insImmPtr(innermostNestedGuard)), NESTED_EXIT);
    }
}

TraceRecorder::~TraceRecorder()
{
    JS_ASSERT(treeInfo);
    if (fragment->root == fragment && !fragment->root->code()) {
        JS_ASSERT(!fragment->root->vmprivate);
        delete treeInfo;
    }
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

/* Determine the offset in the native global frame for a jsval we track */
ptrdiff_t
TraceRecorder::nativeGlobalOffset(jsval* p) const
{
    JS_ASSERT(isGlobal(p));
    if (size_t(p - globalObj->fslots) < JS_INITIAL_NSLOTS)
        return size_t(p - globalObj->fslots) * sizeof(double);
    return ((p - globalObj->dslots) + JS_INITIAL_NSLOTS) * sizeof(double);
}

/* Determine whether a value is a global stack slot */
bool
TraceRecorder::isGlobal(jsval* p) const
{
    return ((size_t(p - globalObj->fslots) < JS_INITIAL_NSLOTS) ||
            (size_t(p - globalObj->dslots) < (STOBJ_NSLOTS(globalObj) - JS_INITIAL_NSLOTS)));
}

/* Determine the offset in the native stack for a jsval we track */
ptrdiff_t
TraceRecorder::nativeStackOffset(jsval* p) const
{
#ifdef DEBUG
    size_t slow_offset = 0;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        if (vp == p) goto done;
        slow_offset += sizeof(double)
    );

    /*
     * If it's not in a pending frame, it must be on the stack of the current frame above
     * sp but below fp->slots + script->nslots.
     */
    JS_ASSERT(size_t(p - cx->fp->slots) < cx->fp->script->nslots);
    slow_offset += size_t(p - cx->fp->regs->sp) * sizeof(double);

done:
#define RETURN(offset) { JS_ASSERT((offset) == slow_offset); return offset; }
#else
#define RETURN(offset) { return offset; }
#endif
    size_t offset = 0;
    JSStackFrame* currentFrame = cx->fp;
    JSStackFrame* entryFrame;
    JSStackFrame* fp = currentFrame;
    for (unsigned n = 0; n < callDepth; ++n) { fp = fp->down; }
    entryFrame = fp;
    unsigned frames = callDepth+1;
    JSStackFrame** fstack = (JSStackFrame **)alloca(frames * sizeof (JSStackFrame *));
    JSStackFrame** fspstop = &fstack[frames];
    JSStackFrame** fsp = fspstop-1;
    fp = currentFrame;
    for (;; fp = fp->down) { *fsp-- = fp; if (fp == entryFrame) break; }
    for (fsp = fstack; fsp < fspstop; ++fsp) {
        fp = *fsp;
        if (fp->callee) {
            if (fsp == fstack) {
                unsigned nargs = JS_MAX(fp->fun->nargs, fp->argc);
                if (size_t(p - &fp->argv[-2]) < 2/*callee,this*/ + nargs)
                    RETURN(offset + size_t(p - &fp->argv[-2]) * sizeof(double));
                offset += (2/*callee,this*/ + nargs) * sizeof(double);
            }
            if (size_t(p - &fp->slots[0]) < fp->script->nfixed)
                RETURN(offset + size_t(p - &fp->slots[0]) * sizeof(double));
            offset += fp->script->nfixed * sizeof(double);
        }
        jsval* spbase = StackBase(fp);
        if (size_t(p - spbase) < size_t(fp->regs->sp - spbase))
            RETURN(offset + size_t(p - spbase) * sizeof(double));
        offset += size_t(fp->regs->sp - spbase) * sizeof(double);
        if (fsp < fspstop - 1) {
            JSStackFrame* fp2 = fsp[1];
            int missing = fp2->fun->nargs - fp2->argc;
            if (missing > 0) {
                if (size_t(p - fp->regs->sp) < size_t(missing))
                    RETURN(offset + size_t(p - fp->regs->sp) * sizeof(double));
                offset += size_t(missing) * sizeof(double);
            }
        }
    }

    /*
     * If it's not in a pending frame, it must be on the stack of the current frame above
     * sp but below fp->slots + script->nslots.
     */
    JS_ASSERT(size_t(p - currentFrame->slots) < currentFrame->script->nslots);
    offset += size_t(p - currentFrame->regs->sp) * sizeof(double);
    RETURN(offset);
#undef RETURN
}

/* Track the maximum number of native frame slots we need during
   execution. */
void
TraceRecorder::trackNativeStackUse(unsigned slots)
{
    if (slots > treeInfo->maxNativeStackSlots)
        treeInfo->maxNativeStackSlots = slots;
}

/* Unbox a jsval into a slot. Slots are wide enough to hold double values
   directly (instead of storing a pointer to them). */
static bool
ValueToNative(JSContext* cx, jsval v, uint8 type, double* slot)
{
    unsigned tag = JSVAL_TAG(v);
    switch (type) {
      case JSVAL_INT:
        jsint i;
        if (JSVAL_IS_INT(v))
            *(jsint*)slot = JSVAL_TO_INT(v);
        else if ((tag == JSVAL_DOUBLE) && JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(v), i))
            *(jsint*)slot = i;
        else if (v == JSVAL_VOID)
            *(jsint*)slot = 0;
        else {
            debug_only_v(printf("int != tag%lu(value=%lu) ", JSVAL_TAG(v), v);)
            return false;
        }
        debug_only_v(printf("int<%d> ", *(jsint*)slot);)
        return true;
      case JSVAL_DOUBLE:
        jsdouble d;
        if (JSVAL_IS_INT(v))
            d = JSVAL_TO_INT(v);
        else if (tag == JSVAL_DOUBLE)
            d = *JSVAL_TO_DOUBLE(v);
        else if (v == JSVAL_VOID)
            d = js_NaN;
        else {
            debug_only_v(printf("double != tag%lu ", JSVAL_TAG(v));)
            return false;
        }
        *(jsdouble*)slot = d;
        debug_only_v(printf("double<%g> ", d);)
        return true;
      case JSVAL_BOOLEAN:
        if (tag != JSVAL_BOOLEAN) {
             debug_only_v(printf("bool != tag%u ", tag);)
             return false;
        }
        *(JSBool*)slot = JSVAL_TO_BOOLEAN(v);
        debug_only_v(printf("boolean<%d> ", *(bool*)slot);)
        return true;
      case JSVAL_STRING:
        if (v == JSVAL_VOID) {
            *(JSString**)slot = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]); 
            return true;
        } 
        if (tag != JSVAL_STRING) {
            debug_only_v(printf("string != tag%u ", tag);)
            return false;
        }
        *(JSString**)slot = JSVAL_TO_STRING(v);
        debug_only_v(printf("string<%p> ", *(JSString**)slot);)
        return true;
      default:
        JS_ASSERT(type == JSVAL_OBJECT);
        if (v == JSVAL_VOID) {
            *(JSObject**)slot = NULL;
            return true;
        }
        if (tag != JSVAL_OBJECT) {
            debug_only_v(printf("object != tag%u ", tag);)
            return false;
        }
        *(JSObject**)slot = JSVAL_TO_OBJECT(v);
        debug_only_v(printf("object<%p:%s> ", JSVAL_TO_OBJECT(v),
                            JSVAL_IS_NULL(v)
                            ? "null"
                            : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);)
        return true;
    }
}

/* Box a value from the native stack back into the jsval format. Integers
   that are too large to fit into a jsval are automatically boxed into
   heap-allocated doubles. */
static bool
NativeToValue(JSContext* cx, jsval& v, uint8 type, double* slot)
{
    jsint i;
    jsdouble d;
    switch (type) {
      case JSVAL_BOOLEAN:
        v = BOOLEAN_TO_JSVAL(*(bool*)slot);
        debug_only_v(printf("boolean<%d> ", *(bool*)slot);)
        break;
      case JSVAL_INT:
        i = *(jsint*)slot;
        debug_only_v(printf("int<%d> ", i);)
      store_int:
        if (INT_FITS_IN_JSVAL(i)) {
            v = INT_TO_JSVAL(i);
            break;
        }
        d = (jsdouble)i;
        goto store_double;
      case JSVAL_DOUBLE:
        d = *slot;
        debug_only_v(printf("double<%g> ", d);)
        if (JSDOUBLE_IS_INT(d, i))
            goto store_int;
      store_double:
        /* Its safe to trigger the GC here since we rooted all strings/objects and all the
           doubles we already processed. */
        return js_NewDoubleInRootedValue(cx, d, &v) ? true : false;
      case JSVAL_STRING:
        v = STRING_TO_JSVAL(*(JSString**)slot);
        debug_only_v(printf("string<%p> ", *(JSString**)slot);)
        break;
      default:
        JS_ASSERT(type == JSVAL_OBJECT);
        v = OBJECT_TO_JSVAL(*(JSObject**)slot);
        debug_only_v(printf("object<%p:%s> ", JSVAL_TO_OBJECT(v),
                            JSVAL_IS_NULL(v)
                            ? "null"
                            : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);)
        break;
    }
    return true;
}

/* Attempt to unbox the given list of interned globals onto the native global frame, checking
   along the way that the supplied type-mao holds. */
static bool
BuildNativeGlobalFrame(JSContext* cx, unsigned ngslots, uint16* gslots, uint8* mp, double* np)
{
    debug_only_v(printf("global: ");)
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        if (!ValueToNative(cx, *vp, *mp, np + gslots[n]))
            return false;
        ++mp;
    );
    debug_only_v(printf("\n");)
    return true;
}

/* Attempt to unbox the given JS frame onto a native frame, checking along the way that the
   supplied type-map holds. */
static bool
BuildNativeStackFrame(JSContext* cx, unsigned callDepth, uint8* mp, double* np)
{
    debug_only_v(printf("stack: ");)
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        debug_only_v(printf("%s%u=", vpname, vpnum);)
        if (!ValueToNative(cx, *vp, *mp, np))
            return false;
        ++mp; ++np;
    );
    debug_only_v(printf("\n");)
    return true;
}

/* Box the given native frame into a JS frame. This only fails due to a hard error
   (out of memory for example). */
static int
FlushNativeGlobalFrame(JSContext* cx, unsigned ngslots, uint16* gslots, uint8* mp, double* np)
{
    uint8* mp_base = mp;
    /* Root all string and object references first (we don't need to call the GC for this). */
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        if ((*mp == JSVAL_STRING || *mp == JSVAL_OBJECT) &&
            !NativeToValue(cx, *vp, *mp, np + gslots[n])) {
            return -1;
        }
        ++mp;
    );
    /* Now do this again but this time for all values (properly quicker than actually checking
       the type and excluding strings and objects). The GC might kick in when we store doubles,
       but everything is rooted now (all strings/objects and all doubles we already boxed). */
    mp = mp_base;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        if (!NativeToValue(cx, *vp, *mp, np + gslots[n]))
            return -1;
        ++mp;
    );
    debug_only_v(printf("\n");)
    return mp - mp_base;
}

/* Box the given native stack frame into the virtual machine stack. This only fails due to a
   hard error (out of memory for example). */
static int
FlushNativeStackFrame(JSContext* cx, unsigned callDepth, uint8* mp, double* np, jsval* stopAt)
{
    uint8* mp_base = mp;
    double* np_base = np;
    /* Root all string and object references first (we don't need to call the GC for this). */
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        if (vp == stopAt) goto skip1;
        if ((*mp == JSVAL_STRING || *mp == JSVAL_OBJECT) && !NativeToValue(cx, *vp, *mp, np))
            return -1;
        ++mp; ++np
    );
skip1:
    // Restore thisp from the now-restored argv[-1] in each pending frame.
    unsigned n = callDepth;
    for (JSStackFrame* fp = cx->fp; n-- != 0; fp = fp->down)
        fp->thisp = JSVAL_TO_OBJECT(fp->argv[-1]);

    /* Now do this again but this time for all values (properly quicker than actually checking
       the type and excluding strings and objects). The GC might kick in when we store doubles,
       but everything is rooted now (all strings/objects and all doubles we already boxed). */
    mp = mp_base;
    np = np_base;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        if (vp == stopAt) goto skip2;
        debug_only_v(printf("%s%u=", vpname, vpnum);)
        if (!NativeToValue(cx, *vp, *mp, np))
            return -1;
        ++mp; ++np
    );
skip2:
    debug_only_v(printf("\n");)
    return mp - mp_base;
}

/* Emit load instructions onto the trace that read the initial stack state. */
void
TraceRecorder::import(LIns* base, ptrdiff_t offset, jsval* p, uint8& t,
                      const char *prefix, uintN index, JSStackFrame *fp)
{
    LIns* ins;
    if (t == JSVAL_INT) { /* demoted */
        JS_ASSERT(isInt32(*p));
        /* Ok, we have a valid demotion attempt pending, so insert an integer
           read and promote it to double since all arithmetic operations expect
           to see doubles on entry. The first op to use this slot will emit a
           f2i cast which will cancel out the i2f we insert here. */
        ins = lir->insLoadi(base, offset);
        ins = lir->ins1(LIR_i2f, ins);
    } else {
        JS_ASSERT(isNumber(*p) == (t == JSVAL_DOUBLE));
        if (t == JSVAL_DOUBLE) {
            ins = lir->insLoad(LIR_ldq, base, offset);
        } else {
            ins = lir->insLoad(LIR_ldp, base, offset);
        }
    }
    tracker.set(p, ins);
#ifdef DEBUG
    char name[64];
    JS_ASSERT(strlen(prefix) < 10);
    void* mark = NULL;
    jsuword* localNames = NULL;
    const char* funName = NULL;
    if (*prefix == 'a' || *prefix == 'v') {
        mark = JS_ARENA_MARK(&cx->tempPool);
        localNames = js_GetLocalNameArray(cx, fp->fun, &cx->tempPool);
        funName = fp->fun->atom ? js_AtomToPrintableString(cx, fp->fun->atom) : "<anonymous>";
    }
    if (!strcmp(prefix, "argv")) {
        if (index < fp->fun->nargs) {
            JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(localNames[index]);
            JS_snprintf(name, sizeof name, "$%s.%s", funName, js_AtomToPrintableString(cx, atom));
        } else {
            JS_snprintf(name, sizeof name, "$%s.<arg%d>", funName, index);
        }
    } else if (!strcmp(prefix, "vars")) {
        JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(localNames[fp->fun->nargs + index]);
        JS_snprintf(name, sizeof name, "$%s.%s", funName, js_AtomToPrintableString(cx, atom));
    } else {
        JS_snprintf(name, sizeof name, "$%s%d", prefix, index);
    }

    if (mark)
        JS_ARENA_RELEASE(&cx->tempPool, mark);
    addName(ins, name);

    static const char* typestr[] = {
        "object", "int", "double", "3", "string", "5", "boolean", "any"
    };
    debug_only_v(printf("import vp=%p name=%s type=%s flags=%d\n", p, name, typestr[t & 7], t >> 3););
#endif
}

void
TraceRecorder::import(TreeInfo* treeInfo, LIns* sp, unsigned ngslots, unsigned callDepth,
                      uint8* globalTypeMap, uint8* stackTypeMap)
{
    /* If we get a partial list that doesn't have all the types (i.e. recording from a side
       exit that was recorded but we added more global slots later), merge the missing types
       from the entry type map. This is safe because at the loop edge we verify that we
       have compatible types for all globals (entry type and loop edge type match). While
       a different trace of the tree might have had a guard with a different type map for
       these slots we just filled in here (the guard we continue from didn't know about them),
       since we didn't take that particular guard the only way we could have ended up here
       is if that other trace had at its end a compatible type distribution with the entry
       map. Since thats exactly what we used to fill in the types our current side exit
       didn't provide, this is always safe to do. */
    unsigned length;
    if (ngslots < (length = traceMonitor->globalTypeMap->length()))
        mergeTypeMaps(&globalTypeMap, &ngslots,
                      traceMonitor->globalTypeMap->data(), length,
                      (uint8*)alloca(sizeof(uint8) * length));
    JS_ASSERT(ngslots == traceMonitor->globalTypeMap->length());

    /* the first time we compile a tree this will be empty as we add entries lazily */
    uint16* gslots = traceMonitor->globalSlots->data();
    uint8* m = globalTypeMap;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        import(gp_ins, nativeGlobalOffset(vp), vp, *m, vpname, vpnum, NULL);
        m++;
    );
    ptrdiff_t offset = -treeInfo->nativeStackBase;
    m = stackTypeMap;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        import(sp, offset, vp, *m, vpname, vpnum, fp);
        m++; offset += sizeof(double);
    );
}

/* Lazily import a global slot if we don't already have it in the tracker. */
bool
TraceRecorder::lazilyImportGlobalSlot(unsigned slot)
{
    if (slot != (uint16)slot) /* we use a table of 16-bit ints, bail out if that's not enough */
        return false;
    jsval* vp = &STOBJ_GET_SLOT(globalObj, slot);
    if (tracker.has(vp))
        return true; /* we already have it */
    unsigned index = traceMonitor->globalSlots->length();
    /* If this the first global we are adding, remember the shape of the global object. */
    if (index == 0)
        traceMonitor->globalShape = OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->shape;
    /* Add the slot to the list of interned global slots. */
    traceMonitor->globalSlots->add(slot);
    uint8 type = getCoercedType(*vp);
    if ((type == JSVAL_INT) && oracle.isGlobalSlotUndemotable(cx->fp->script, slot))
        type = JSVAL_DOUBLE;
    traceMonitor->globalTypeMap->add(type);
    import(gp_ins, slot*sizeof(double), vp, type, "global", index, NULL);
    return true;
}

/* Write back a value onto the stack or global frames. */
LIns*
TraceRecorder::writeBack(LIns* i, LIns* base, ptrdiff_t offset)
{
    /* Sink all type casts targeting the stack into the side exit by simply storing the original
       (uncasted) value. Each guard generates the side exit map based on the types of the
       last stores to every stack location, so its safe to not perform them on-trace. */
    if (isPromoteInt(i))
        i = ::demote(lir, i);
    return lir->insStorei(i, base, offset);
}

/* Update the tracker, then issue a write back store. */
void
TraceRecorder::set(jsval* p, LIns* i, bool initializing)
{
    JS_ASSERT(initializing || tracker.has(p));
    tracker.set(p, i);
    /* If we are writing to this location for the first time, calculate the offset into the
       native frame manually, otherwise just look up the last load or store associated with
       the same source address (p) and use the same offset/base. */
    LIns* x;
    if ((x = nativeFrameTracker.get(p)) == NULL) {
        if (isGlobal(p))
            x = writeBack(i, gp_ins, nativeGlobalOffset(p));
        else
            x = writeBack(i, lirbuf->sp, -treeInfo->nativeStackBase + nativeStackOffset(p));
        nativeFrameTracker.set(p, x);
    } else {
#define ASSERT_VALID_CACHE_HIT(base, offset)                                  \
    JS_ASSERT(base == lirbuf->sp || base == gp_ins);                          \
    JS_ASSERT(offset == ((base == lirbuf->sp)                                 \
        ? -treeInfo->nativeStackBase + nativeStackOffset(p)                   \
        : nativeGlobalOffset(p)));                                            \

        if (x->isop(LIR_st) || x->isop(LIR_stq)) {
            ASSERT_VALID_CACHE_HIT(x->oprnd2(), x->oprnd3()->constval());
            writeBack(i, x->oprnd2(), x->oprnd3()->constval());
        } else {
            JS_ASSERT(x->isop(LIR_sti) || x->isop(LIR_stqi));
            ASSERT_VALID_CACHE_HIT(x->oprnd2(), x->immdisp());
            writeBack(i, x->oprnd2(), x->immdisp());
        }
    }
#undef ASSERT_VALID_CACHE_HIT
}

LIns*
TraceRecorder::get(jsval* p)
{
    return tracker.get(p);
}

/* Determine whether a bytecode location (pc) terminates a loop or is a path within the loop. */
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

      case JSOP_IFEQ:
      case JSOP_IFEQX:
      case JSOP_IFNE:
      case JSOP_IFNEX:
        /*
         * Forward jumps are usually intra-branch, but for-in loops jump to the trailing enditer to
         * clean up, so check for that case here.
         */
        if (pc[GET_JUMP_OFFSET(pc)] == JSOP_ENDITER)
            return true;
        return GET_JUMP_OFFSET(pc) < 0;

      default:;
    }
    return false;
}

struct FrameInfo {
    JSObject*       callee;     // callee function object
    jsbytecode*     callpc;     // pc of JSOP_CALL in caller script
    union {
        struct {
            uint16  spdist;     // distance from fp->slots to fp->regs->sp at JSOP_CALL
            uint16  argc;       // actual argument count, may be < fun->nargs
        } s;
        uint32      word;       // for spdist/argc LIR store in record_JSOP_CALL
    };
};

/* Promote slots if necessary to match the called tree' type map and report error if thats
   impossible. */
bool
TraceRecorder::adjustCallerTypes(Fragment* f)
{
    JSTraceMonitor* tm = traceMonitor;
    uint8* m = tm->globalTypeMap->data();
    uint16* gslots = traceMonitor->globalSlots->data();
    unsigned ngslots = traceMonitor->globalSlots->length();
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots, 
        LIns* i = get(vp);
        bool isPromote = isPromoteInt(i);
        if (isPromote && *m == JSVAL_DOUBLE) 
            lir->insStorei(get(vp), gp_ins, nativeGlobalOffset(vp));
        ++m;
    );
    m = ((TreeInfo*)f->vmprivate)->stackTypeMap.data();
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0,
        LIns* i = get(vp);
        bool isPromote = isPromoteInt(i);
        if (isPromote && *m == JSVAL_DOUBLE) 
            lir->insStorei(get(vp), lirbuf->sp, 
                           -treeInfo->nativeStackBase + nativeStackOffset(vp));
        ++m;
    );
    return true;
}

/* Find a peer fragment that we can call, considering our current type distribution. */
bool TraceRecorder::selectCallablePeerFragment(Fragment** first)
{
    /* Until we have multiple trees per start point this is always the first fragment. */
    return (*first)->code();
}

SideExit*
TraceRecorder::snapshot(ExitType exitType)
{
    JSStackFrame* fp = cx->fp;
    if (exitType == BRANCH_EXIT && js_IsLoopExit(cx, fp->script, fp->regs->pc))
        exitType = LOOP_EXIT;
    /* Generate the entry map and stash it in the trace. */
    unsigned stackSlots = nativeStackSlots(cx, callDepth);
    /* It's sufficient to track the native stack use here since all stores above the
       stack watermark defined by guards are killed. */
    trackNativeStackUse(stackSlots + 1);
    /* reserve space for the type map */
    unsigned ngslots = traceMonitor->globalSlots->length();
    LIns* data = lir_buf_writer->skip((stackSlots + ngslots) * sizeof(uint8));
    /* setup side exit structure */
    memset(&exit, 0, sizeof(exit));
    exit.from = fragment;
    exit.calldepth = callDepth;
    exit.numGlobalSlots = ngslots;
    exit.numStackSlots = stackSlots;
    exit.exitType = exitType;
    exit.ip_adj = fp->regs->pc - (jsbytecode*)fragment->root->ip;
    exit.sp_adj = (stackSlots * sizeof(double)) - treeInfo->nativeStackBase;
    exit.rp_adj = exit.calldepth * sizeof(FrameInfo);
    uint8* m = exit.typeMap = (uint8 *)data->payload();
    /* Determine the type of a store by looking at the current type of the actual value the
       interpreter is using. For numbers we have to check what kind of store we used last
       (integer or double) to figure out what the side exit show reflect in its typemap. */
    FORALL_SLOTS(cx, ngslots, traceMonitor->globalSlots->data(), callDepth,
        LIns* i = get(vp);
        *m = isNumber(*vp)
               ? (isPromoteInt(i) ? JSVAL_INT : JSVAL_DOUBLE)
               : JSVAL_TAG(*vp);
        JS_ASSERT((*m != JSVAL_INT) || isInt32(*vp));
        ++m;
    );
    JS_ASSERT(unsigned(m - exit.typeMap) == ngslots + stackSlots);
    return &exit;
}

/* Emit a guard for condition (cond), expecting to evaluate to boolean result (expected). */
LIns*
TraceRecorder::guard(bool expected, LIns* cond, ExitType exitType)
{
    return lir->insGuard(expected ? LIR_xf : LIR_xt,
                         cond,
                         snapshot(exitType));
}

/* Try to match the type of a slot to type t. checkType is used to verify that the type of
   values flowing into the loop edge is compatible with the type we expect in the loop header. */
bool
TraceRecorder::checkType(jsval& v, uint8 t, bool& unstable)
{
    if (t == JSVAL_INT) { /* initially all whole numbers cause the slot to be demoted */
        if (!isNumber(v))
            return false; /* not a number? type mismatch */
        LIns* i = get(&v);
        if (!i->isop(LIR_i2f)) {
            debug_only_v(printf("int slot is !isInt32, slot #%d, triggering re-compilation\n",
                                !isGlobal(&v)
                                ? nativeStackOffset(&v)
                                : nativeGlobalOffset(&v)););
            AUDIT(slotPromoted);
            unstable = true;
            return true; /* keep checking types, but request re-compilation */
        }
        /* Looks good, slot is an int32, the last instruction should be i2f. */
        JS_ASSERT(isInt32(v) && i->isop(LIR_i2f));
        /* We got the final LIR_i2f as we expected. Overwrite the value in that
           slot with the argument of i2f since we want the integer store to flow along
           the loop edge, not the casted value. */
        set(&v, i->oprnd1());
        return true;
    }
    if (t == JSVAL_DOUBLE) {
        if (!isNumber(v))
            return false; /* not a number? type mismatch */
        LIns* i = get(&v);
        /* We sink i2f conversions into the side exit, but at the loop edge we have to make
           sure we promote back to double if at loop entry we want a double. */
        if (isPromoteInt(i)) 
            set(&v, lir->ins1(LIR_i2f, i));
        return true;
    }
    /* for non-number types we expect a precise match of the type */
#ifdef DEBUG
    if (JSVAL_TAG(v) != t) {
        debug_only_v(printf("Type mismatch: val %c, map %c ", "OID?S?B"[JSVAL_TAG(v)],
                            "OID?S?B"[t]););
    }
#endif
    return JSVAL_TAG(v) == t;
}

static void
js_TrashTree(JSContext* cx, Fragment* f);

/* Make sure that the current values in the given stack frame and all stack frames
   up and including entryFrame are type-compatible with the entry map. */
bool
TraceRecorder::verifyTypeStability()
{
    unsigned ngslots = traceMonitor->globalSlots->length();
    uint16* gslots = traceMonitor->globalSlots->data();
    uint8* typemap = traceMonitor->globalTypeMap->data();
    JS_ASSERT(traceMonitor->globalTypeMap->length() == ngslots);
    bool recompile = false;
    uint8* m = typemap;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        bool demote = false;
        if (!checkType(*vp, *m, demote))
            return false;
        if (demote) {
            oracle.markGlobalSlotUndemotable(cx->fp->script, gslots[n]);
            recompile = true;
        }
        ++m
    );
    typemap = treeInfo->stackTypeMap.data();
    m = typemap;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        bool demote = false;
        if (!checkType(*vp, *m, demote))
            return false;
        if (demote) {
            oracle.markStackSlotUndemotable(cx->fp->script, (jsbytecode*)fragment->ip,
                    unsigned(m - typemap));
            recompile = true;
        }
        ++m
    );
    if (recompile)
        js_TrashTree(cx, fragment);
    return !recompile;
}

/* Check whether the current pc location is the loop header of the loop this recorder records. */
bool
TraceRecorder::isLoopHeader(JSContext* cx) const
{
    return cx->fp->regs->pc == fragment->root->ip;
}

/* Complete and compile a trace and link it to the existing tree if appropriate. */
void
TraceRecorder::closeLoop(Fragmento* fragmento)
{
    if (!verifyTypeStability()) {
        AUDIT(unstableLoopVariable);
        debug_only_v(printf("Trace rejected: unstable loop variables.\n");)
        return;
    }
    if (treeInfo->maxNativeStackSlots >= MAX_NATIVE_STACK_SLOTS) {
        debug_only_v(printf("Trace rejected: excessive stack use.\n"));
        fragment->blacklist();
        return;
    }
    SideExit *exit = snapshot(LOOP_EXIT);
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
    JS_ASSERT(fragment->code());
    JS_ASSERT(!fragment->vmprivate);
    if (fragment == fragment->root)
        fragment->vmprivate = treeInfo;
	/* :TODO: windows support */
#if defined DEBUG && !defined WIN32
    char* label = (char*)malloc(strlen(cx->fp->script->filename) + 64);
    sprintf(label, "%s:%u", cx->fp->script->filename,
            js_PCToLineNumber(cx, cx->fp->script, cx->fp->regs->pc));
    fragmento->labels->add(fragment, sizeof(Fragment), 0, label);
    free(label);
#endif
}

/* Emit code to adjust the stack to match the inner tree's stack expectations. */
void
TraceRecorder::prepareTreeCall(Fragment* inner)
{
    TreeInfo* ti = (TreeInfo*)inner->vmprivate;
    inner_sp_ins = lirbuf->sp;
    /* The inner tree expects to be called from the current frame. If the outer tree (this
       trace) is currently inside a function inlining code (calldepth > 0), we have to advance
       the native stack pointer such that we match what the inner trace expects to see. We
       move it back when we come out of the inner tree call. */
    if (callDepth > 0) {
        /* Calculate the amount we have to lift the native stack pointer by to compensate for
           any outer frames that the inner tree doesn't expect but the outer tree has. */
        ptrdiff_t sp_adj = nativeStackOffset(&cx->fp->argv[-2]);
        /* Calculate the amount we have to lift the call stack by */
        ptrdiff_t rp_adj = callDepth * sizeof(FrameInfo);
        /* Guard that we have enough stack space for the tree we are trying to call on top
           of the new value for sp. */
        debug_only_v(printf("sp_adj=%d outer=%d inner=%d\n",
                          sp_adj, treeInfo->nativeStackBase, ti->nativeStackBase));
        LIns* sp_top = lir->ins2i(LIR_piadd, lirbuf->sp,
                - treeInfo->nativeStackBase /* rebase sp to beginning of outer tree's stack */
                + sp_adj /* adjust for stack in outer frame inner tree can't see */
                + ti->maxNativeStackSlots * sizeof(double)); /* plus the inner tree's stack */
        guard(true, lir->ins2(LIR_lt, sp_top, eos_ins), OOM_EXIT);
        /* Guard that we have enough call stack space. */
        LIns* rp_top = lir->ins2i(LIR_piadd, lirbuf->rp, rp_adj +
                ti->maxCallDepth * sizeof(FrameInfo));
        guard(true, lir->ins2(LIR_lt, rp_top, eor_ins), OOM_EXIT);
        /* We have enough space, so adjust sp and rp to their new level. */
        lir->insStorei(inner_sp_ins = lir->ins2i(LIR_piadd, lirbuf->sp,
                - treeInfo->nativeStackBase /* rebase sp to beginning of outer tree's stack */
                + sp_adj /* adjust for stack in outer frame inner tree can't see */
                + ti->nativeStackBase), /* plus the inner tree's stack base */
                lirbuf->state, offsetof(InterpState, sp));
        lir->insStorei(lir->ins2i(LIR_piadd, lirbuf->rp, rp_adj),
                lirbuf->state, offsetof(InterpState, rp));
    }
}

/* Record a call to an inner tree. */
void
TraceRecorder::emitTreeCall(Fragment* inner, GuardRecord* lr)
{
    TreeInfo* ti = (TreeInfo*)inner->vmprivate;
    /* Invoke the inner tree. */
    LIns* args[] = { lir->insImmPtr(inner), lirbuf->state }; /* reverse order */
    LIns* ret = lir->insCall(F_CallTree, args);
    /* Read back all registers, in case the called tree changed any of them. */
    SideExit* exit = lr->exit;
    import(ti, inner_sp_ins, exit->numGlobalSlots, exit->calldepth,
           exit->typeMap, exit->typeMap + exit->numGlobalSlots);
    /* Store the guard pointer in case we exit on an unexpected guard */
    lir->insStorei(ret, lirbuf->state, offsetof(InterpState, nestedExit));
    /* Restore sp and rp to their original values (we still have them in a register). */
    if (callDepth > 0) {
        lir->insStorei(lirbuf->sp, lirbuf->state, offsetof(InterpState, sp));
        lir->insStorei(lirbuf->rp, lirbuf->state, offsetof(InterpState, rp));
    }
    /* Guard that we come out of the inner tree along the same side exit we came out when
       we called the inner tree at recording time. */
    guard(true, lir->ins2(LIR_eq, ret, lir->insImmPtr(lr)), NESTED_EXIT);
}

int
nanojit::StackFilter::getTop(LInsp guard)
{
    if (sp == frag->lirbuf->sp)
        return guard->exit()->sp_adj + sizeof(double);
    JS_ASSERT(sp == frag->lirbuf->rp);
    return guard->exit()->rp_adj + sizeof(FrameInfo);
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
    verbose_only(rec->sid = exit->sid);
}

void
nanojit::Assembler::asm_bailout(LIns *guard, Register state)
{
    /* we adjust ip/sp/rp when exiting from the tree in the recovery code */
}

void
nanojit::Fragment::onDestroy()
{
    if (root == this) {
        delete mergeCounts;
        delete lirbuf;
    }
    delete (TreeInfo *)vmprivate;
}

void
js_DeleteRecorder(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    delete tm->recorder;
    tm->recorder = NULL;
}

static bool
js_StartRecorder(JSContext* cx, GuardRecord* anchor, Fragment* f, TreeInfo* ti,
        unsigned ngslots, uint8* globalTypeMap, uint8* stackTypeMap, 
        GuardRecord* expectedInnerExit)
{
    /* start recording if no exception during construction */
    JS_TRACE_MONITOR(cx).recorder = new (&gc) TraceRecorder(cx, anchor, f, ti,
            ngslots, globalTypeMap, stackTypeMap, expectedInnerExit);
    if (cx->throwing) {
        js_AbortRecording(cx, NULL, "setting up recorder failed");
        return false;
    }
    /* clear any leftover error state */
    JS_TRACE_MONITOR(cx).fragmento->assm()->setError(None);
    return true;
}

static void
js_TrashTree(JSContext* cx, Fragment* f)
{
    JS_ASSERT((!f->code()) == (!f->vmprivate));
    if (!f->code())
        return;
    AUDIT(treesTrashed);
    debug_only_v(printf("Trashing tree info.\n");)
    Fragmento* fragmento = JS_TRACE_MONITOR(cx).fragmento;
    delete (TreeInfo*)f->vmprivate;
    f->vmprivate = NULL;
    f->releaseCode(fragmento);
}

static unsigned
js_SynthesizeFrame(JSContext* cx, const FrameInfo& fi)
{
    JS_ASSERT(HAS_FUNCTION_CLASS(fi.callee));

    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, fi.callee);
    JS_ASSERT(FUN_INTERPRETED(fun));

    JSArena* a = cx->stackPool.current;
    void* newmark = (void*) a->avail;
    JSScript* script = fun->u.i.script;

    // Assert that we have a correct sp distance from cx->fp->slots in fi.
    JS_ASSERT(js_ReconstructStackDepth(cx, cx->fp->script, fi.callpc) ==
              uintN(fi.s.spdist - cx->fp->script->nfixed));

    uintN nframeslots = JS_HOWMANY(sizeof(JSInlineFrame), sizeof(jsval));
    size_t nbytes = (nframeslots + script->nslots) * sizeof(jsval);

    /* Allocate the inline frame with its vars and operands. */
    jsval* newsp;
    if (a->avail + nbytes <= a->limit) {
        newsp = (jsval *) a->avail;
        a->avail += nbytes;
    } else {
        JS_ARENA_ALLOCATE_CAST(newsp, jsval *, &cx->stackPool, nbytes);
        if (!newsp) {
            js_ReportOutOfScriptQuota(cx);
            return 0;
        }
    }

    /* Claim space for the stack frame and initialize it. */
    JSInlineFrame* newifp = (JSInlineFrame *) newsp;
    newsp += nframeslots;

    newifp->frame.callobj = NULL;
    newifp->frame.argsobj = NULL;
    newifp->frame.varobj = NULL;
    newifp->frame.script = script;
    newifp->frame.callee = fi.callee;
    newifp->frame.fun = fun;

    newifp->frame.argc = fi.s.argc;
    newifp->callerRegs.pc = fi.callpc;
    newifp->callerRegs.sp = cx->fp->slots + fi.s.spdist;
    newifp->frame.argv = newifp->callerRegs.sp - JS_MAX(fun->nargs, fi.s.argc);
    JS_ASSERT(newifp->frame.argv >= StackBase(cx->fp));

    newifp->frame.rval = JSVAL_VOID;
    newifp->frame.down = cx->fp;
    newifp->frame.annotation = NULL;
    newifp->frame.scopeChain = OBJ_GET_PARENT(cx, fi.callee);
    newifp->frame.sharpDepth = 0;
    newifp->frame.sharpArray = NULL;
    newifp->frame.flags = 0;
    newifp->frame.dormantNext = NULL;
    newifp->frame.xmlNamespace = NULL;
    newifp->frame.blockChain = NULL;
    newifp->mark = newmark;
    newifp->frame.thisp = NULL; // will be set by js_ExecuteTree -> FlushNativeStackFrame

    newifp->frame.regs = cx->fp->regs;
    newifp->frame.regs->pc = script->code;
    newifp->frame.regs->sp = newsp + script->nfixed;
    newifp->frame.slots = newsp;
    if (script->staticDepth < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[script->staticDepth];
        newifp->frame.displaySave = *disp;
        *disp = &newifp->frame;
    }
#ifdef DEBUG
    newifp->frame.pcDisabledSave = 0;
#endif

    cx->fp->regs = &newifp->callerRegs;
    cx->fp = &newifp->frame;

    // FIXME: we must count stack slots from caller's operand stack up to (but not including)
    // callee's, including missing arguments. Could we shift everything down to the caller's
    // fp->slots (where vars start) and avoid some of the complexity?
    return (fi.s.spdist - cx->fp->down->script->nfixed) +
           ((fun->nargs > cx->fp->argc) ? fun->nargs - cx->fp->argc : 0) +
           script->nfixed;
}

bool
js_RecordTree(JSContext* cx, JSTraceMonitor* tm, Fragment* f)
{
    /* Make sure the global type map didn't change on us. */
    uint32 globalShape = OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->shape;
    if (tm->globalShape != globalShape) {
        debug_only(printf("Global shape mismatch (%u vs. %u) in RecordTree, flushing cache.\n",
                          globalShape, tm->globalShape);)
        js_FlushJITCache(cx);
        return false;
    }
    TypeMap current;
    current.captureGlobalTypes(cx, *tm->globalSlots);
    if (!current.matches(*tm->globalTypeMap)) {
        js_FlushJITCache(cx);
        debug_only(printf("Global type map mismatch in RecordTree, flushing cache.\n");)
        return false;
    }
    
    AUDIT(recorderStarted);

    /* Try to find an unused peer fragment, or allocate a new one. */
    while (f->code() && f->peer)
        f = f->peer;
    if (f->code())
        f = JS_TRACE_MONITOR(cx).fragmento->newLoop(f->ip);

    f->calldepth = 0;
    f->root = f;
    /* allocate space to store the LIR for this tree */
    if (!f->lirbuf) {
        f->lirbuf = new (&gc) LirBuffer(tm->fragmento, builtins);
#ifdef DEBUG
        f->lirbuf->names = new (&gc) LirNameMap(&gc, builtins, tm->fragmento->labels);
#endif
    }

    JS_ASSERT(!f->code() && !f->vmprivate);

    /* setup the VM-private treeInfo structure for this fragment */
    TreeInfo* ti = new (&gc) TreeInfo(f);

    /* capture the coerced type of each active slot in the stack type map */
    ti->stackTypeMap.captureStackTypes(cx, 0/*callDepth*/);

    /* determine the native frame layout at the entry point */
    unsigned entryNativeStackSlots = ti->stackTypeMap.length();
    JS_ASSERT(entryNativeStackSlots == nativeStackSlots(cx, 0/*callDepth*/));
    ti->nativeStackBase = (entryNativeStackSlots -
            (cx->fp->regs->sp - StackBase(cx->fp))) * sizeof(double);
    ti->maxNativeStackSlots = entryNativeStackSlots;
    ti->maxCallDepth = 0;

    /* recording primary trace */
    return js_StartRecorder(cx, NULL, f, ti,
                            tm->globalSlots->length(), tm->globalTypeMap->data(), 
                            ti->stackTypeMap.data(), NULL);
}

static bool
js_AttemptToExtendTree(JSContext* cx, GuardRecord* lr, Fragment* f, GuardRecord* expectedInnerExit)
{
    JS_ASSERT(f->root == f && f->vmprivate);

    debug_only_v(printf("trying to attach another branch to the tree\n");)

    Fragment* c;
    if (!(c = lr->target)) {
        c = JS_TRACE_MONITOR(cx).fragmento->createBranch(lr, lr->exit);
        c->spawnedFrom = lr->guard;
        c->parent = f;
        lr->exit->target = c;
        lr->target = c;
        c->root = f;
    }

    if (++c->hits() >= HOTEXIT) {
        /* start tracing secondary trace from this point */
        c->lirbuf = f->lirbuf;
        SideExit* e = lr->exit;
        unsigned ngslots = e->numGlobalSlots;
        uint8* globalTypeMap = e->typeMap;
        uint8* stackTypeMap = globalTypeMap + ngslots;
        return js_StartRecorder(cx, lr, c, (TreeInfo*)f->vmprivate,
                                ngslots, globalTypeMap, stackTypeMap, expectedInnerExit);
    }
    return false;
}

static GuardRecord*
js_ExecuteTree(JSContext* cx, Fragment** treep, uintN& inlineCallCount, 
               GuardRecord** innermostNestedGuardp);

bool
js_ContinueRecording(JSContext* cx, TraceRecorder* r, jsbytecode* oldpc, uintN& inlineCallCount)
{
#ifdef JS_THREADSAFE
    if (OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->title.ownercx != cx) {
        debug_only_v(printf("Global object not owned by this context.\n"););
        return false; /* we stay away from shared global objects */
    }
#endif
    Fragmento* fragmento = JS_TRACE_MONITOR(cx).fragmento;
    if (r->isLoopHeader(cx)) { /* did we hit the start point? */
        if (fragmento->assm()->error()) {
            js_AbortRecording(cx, oldpc, "Error during recording");
            /* if we ran out of memory, flush the cache */
            if (fragmento->assm()->error() == OutOMem)
                js_FlushJITCache(cx);
            return false; /* we are done recording */
        }
        AUDIT(traceCompleted);
        r->closeLoop(fragmento);
        js_DeleteRecorder(cx);
        return false; /* done recording */
    }
    /* does this branch go to an inner loop? */
    Fragment* f = fragmento->getLoop(cx->fp->regs->pc);
    if (nesting_enabled && 
        f && /* must have a fragment at that location */
        r->selectCallablePeerFragment(&f) && /* is there a potentially matching peer fragment? */
        r->adjustCallerTypes(f)) { /* make sure we can make our arguments fit */
        r->prepareTreeCall(f);
        GuardRecord* innermostNestedGuard = NULL;
        GuardRecord* lr = js_ExecuteTree(cx, &f, inlineCallCount, &innermostNestedGuard);
        if (!lr) {
            js_AbortRecording(cx, oldpc, "Couldn't call inner tree");
            return false;
        }
        switch (lr->exit->exitType) {
        case LOOP_EXIT:
            /* If the inner tree exited on an unknown loop exit, grow the tree around it. */
            if (innermostNestedGuard) {
                js_AbortRecording(cx, oldpc, "Inner tree took different side exit, abort recording");
                return js_AttemptToExtendTree(cx, innermostNestedGuard, innermostNestedGuard->from->root, lr);
            }
            /* emit a call to the inner tree and continue recording the outer tree trace */
            r->emitTreeCall(f, lr);
            return true;
        case BRANCH_EXIT:
            /* abort recording the outer tree, extend the inner tree */
            js_AbortRecording(cx, oldpc, "Inner tree is trying to grow, abort outer recording");
            return js_AttemptToExtendTree(cx, lr, lr->from->root, NULL);
        default:
            debug_only_v(printf("exit_type=%d\n", lr->exit->exitType);)
            js_AbortRecording(cx, oldpc, "Inner tree not suitable for calling");
            return false;
        }
    }
    /* not returning to our own loop header, not an inner loop we can call, abort trace */
    AUDIT(returnToDifferentLoopHeader);
    debug_only_v(printf("loop edge %d -> %d, header %d\n",
            oldpc - cx->fp->script->code,
            cx->fp->regs->pc - cx->fp->script->code,
            (jsbytecode*)r->getFragment()->root->ip - cx->fp->script->code));
    js_AbortRecording(cx, oldpc, "Loop edge does not return to header");
    return false; /* done recording */
}

static inline GuardRecord*
js_ExecuteTree(JSContext* cx, Fragment** treep, uintN& inlineCallCount, 
               GuardRecord** innermostNestedGuardp)
{
    Fragment* f = *treep;

    /* if we don't have a compiled tree available for this location, bail out */
    if (!f->code()) {
        JS_ASSERT(!f->vmprivate);
        return NULL;
    }
    JS_ASSERT(f->vmprivate);

    AUDIT(traceTriggered);

    /* execute previously recorded trace */
    TreeInfo* ti = (TreeInfo*)f->vmprivate;

    debug_only_v(printf("entering trace at %s:%u@%u, native stack slots: %u\n",
                        cx->fp->script->filename,
                        js_PCToLineNumber(cx, cx->fp->script, cx->fp->regs->pc),
                        cx->fp->regs->pc - cx->fp->script->code, ti->maxNativeStackSlots););

    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    unsigned ngslots = tm->globalSlots->length();
    uint16* gslots = tm->globalSlots->data();
    JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    unsigned globalFrameSize = STOBJ_NSLOTS(globalObj);
    double* global = (double*)alloca((globalFrameSize+1) * sizeof(double));
    debug_only(*(uint64*)&global[globalFrameSize] = 0xdeadbeefdeadbeefLL;)
    double* stack = (double*)alloca(MAX_NATIVE_STACK_SLOTS * sizeof(double));

    /* If any of our trees uses globals, the shape of the global object must not change and
       the global type map must remain applicable at all times (we expect absolute type 
       stability for globals). */
    if (ngslots &&
        (OBJ_SCOPE(globalObj)->shape != tm->globalShape || 
         !BuildNativeGlobalFrame(cx, ngslots, gslots, tm->globalTypeMap->data(), global))) {
        AUDIT(globalShapeMismatchAtEntry);
        debug_only_v(printf("Global shape mismatch (%u vs. %u), flushing cache.\n",
                            OBJ_SCOPE(globalObj)->shape, tm->globalShape);)
        const void* ip = f->ip;
        js_FlushJITCache(cx);
        *treep = tm->fragmento->newLoop(ip);
        return NULL;
    }

    if (!BuildNativeStackFrame(cx, 0/*callDepth*/, ti->stackTypeMap.data(), stack)) {
        AUDIT(typeMapMismatchAtEntry);
        debug_only_v(printf("type-map mismatch.\n");)
        if (++ti->mismatchCount > MAX_MISMATCH) {
            debug_only_v(printf("excessive mismatches, flushing tree.\n"));
            js_TrashTree(cx, f);
            f->blacklist();
        }
        return NULL;
    }

    ti->mismatchCount = 0;

    double* entry_sp = &stack[ti->nativeStackBase/sizeof(double)];
    FrameInfo* callstack = (FrameInfo*) alloca(MAX_CALL_STACK_ENTRIES * sizeof(FrameInfo));

    InterpState state;
    state.sp = (void*)entry_sp;
    state.eos = ((double*)state.sp) + MAX_NATIVE_STACK_SLOTS;
    state.rp = callstack;
    state.eor = callstack + MAX_CALL_STACK_ENTRIES;
    state.gp = global;
    state.cx = cx;
    union { NIns *code; GuardRecord* (FASTCALL *func)(InterpState*, Fragment*); } u;
    u.code = f->code();

#if defined(DEBUG) && defined(NANOJIT_IA32)
    uint64 start = rdtsc();
#endif

    JS_ASSERT(!cx->gcDontBlock);
    cx->gcDontBlock = JS_TRUE;
    GuardRecord* lr = u.func(&state, NULL);
    cx->gcDontBlock = JS_FALSE;

    /* If we bail out on a nested exit, the compiled code returns the outermost nesting
       guard but what we are really interested in is the innermost guard that we hit
       instead of the guard we were expecting there. */
    if (lr->exit->exitType == NESTED_EXIT) {
        /* Unwind all frames held by nested outer trees (since the innermost tree's frame which
           we restore below doesn't contain such frames. */
        do {
            if (innermostNestedGuardp)
                *innermostNestedGuardp = lr;
            debug_only_v(printf("processing tree call guard %p, calldepth=%d\n", lr, lr->calldepth);)
            unsigned calldepth = lr->calldepth;
            if (calldepth > 0) {
                /* We found a nesting guard that holds a frame, write it back. */
                for (unsigned i = 0; i < calldepth; ++i)
                    js_SynthesizeFrame(cx, callstack[i]);
                /* Restore the native stack excluding the current frame, which the next tree
                   call guard or the innermost tree exit guard will restore. */
                unsigned slots = FlushNativeStackFrame(cx, calldepth,
                        lr->exit->typeMap + lr->exit->numGlobalSlots,
                        stack, &cx->fp->argv[-2]);
                callstack += calldepth;
                inlineCallCount += calldepth;
                stack += slots;
            }
            JS_ASSERT(lr->guard->oprnd1()->oprnd2()->isconstp());
            lr = (GuardRecord*)lr->guard->oprnd1()->oprnd2()->constvalp();
        } while (lr->exit->exitType == NESTED_EXIT);

        /* We restored the nested frames, now we just need to deal with the innermost guard. */
        lr = state.nestedExit;
    }

    /* sp_adj and ip_adj are relative to the tree we exit out of, not the tree we
       entered into (which might be different in the presence of nested trees). */
    ti = (TreeInfo*)lr->from->root->vmprivate;

    /* We already synthesized the frames around the innermost guard. Here we just deal
       with additional frames inside the tree we are bailing out from. */
    unsigned calldepth = lr->calldepth;
    unsigned calldepth_slots = 0;
    for (unsigned n = 0; n < calldepth; ++n)
        calldepth_slots += js_SynthesizeFrame(cx, callstack[n]);

    /* Adjust sp and pc relative to the tree we exited from (not the tree we entered
       into). These are our final values for sp and pc since js_SynthesizeFrame has
       already taken care of all frames in between. */
    SideExit* e = lr->exit;
    JSStackFrame* fp = cx->fp;

    /* If we are not exiting from an inlined frame the state->sp is spbase, otherwise spbase
       is whatever slots frames around us consume. */
    fp->regs->pc = (jsbytecode*)lr->from->root->ip + e->ip_adj;
    fp->regs->sp = StackBase(fp) + (e->sp_adj / sizeof(double)) - calldepth_slots;
    JS_ASSERT(fp->slots + fp->script->nfixed +
              js_ReconstructStackDepth(cx, cx->fp->script, fp->regs->pc) == fp->regs->sp);

#if defined(DEBUG) && defined(NANOJIT_IA32)
    if (verbose_debug) {
        printf("leaving trace at %s:%u@%u, exitType=%d, sp=%d, ip=%p, cycles=%llu\n",
               fp->script->filename, js_PCToLineNumber(cx, fp->script, fp->regs->pc),
               fp->regs->pc - fp->script->code,
               lr->exit->exitType,
               fp->regs->sp - StackBase(fp), lr->jmp,
               (rdtsc() - start));
    }
#endif

    /* If this trace is part of a tree, later branches might have added additional globals for
       with we don't have any type information available in the side exit. We merge in this
       information from the entry type-map. See also comment in the constructor of TraceRecorder
       why this is always safe to do. */
    unsigned exit_gslots = e->numGlobalSlots;
    JS_ASSERT(ngslots == tm->globalTypeMap->length());
    JS_ASSERT(ngslots >= exit_gslots);
    uint8* globalTypeMap = e->typeMap;
    if (exit_gslots < ngslots)
        mergeTypeMaps(&globalTypeMap, &exit_gslots, tm->globalTypeMap->data(), ngslots,
                      (uint8*)alloca(sizeof(uint8) * ngslots));
    JS_ASSERT(exit_gslots == tm->globalTypeMap->length());

    /* write back interned globals */
    FlushNativeGlobalFrame(cx, exit_gslots, gslots, globalTypeMap, global);
    JS_ASSERT(globalFrameSize == STOBJ_NSLOTS(globalObj));
    JS_ASSERT(*(uint64*)&global[globalFrameSize] == 0xdeadbeefdeadbeefLL);

    /* write back native stack frame */
    debug_only(unsigned slots =)
    FlushNativeStackFrame(cx, e->calldepth, e->typeMap + e->numGlobalSlots, stack, NULL);
    JS_ASSERT(slots == e->numStackSlots);

    AUDIT(sideExitIntoInterpreter);

    if (!lr) /* did the tree actually execute? */
        return NULL;

    /* Adjust inlineCallCount (we already compensated for any outer nested frames). */
    inlineCallCount += lr->calldepth;

    return lr;
}

bool
js_LoopEdge(JSContext* cx, jsbytecode* oldpc, uintN& inlineCallCount)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* is the recorder currently active? */
    if (tm->recorder) {
        if (js_ContinueRecording(cx, tm->recorder, oldpc, inlineCallCount))
            return true;
        /* recording was aborted, treat like a regular loop edge hit */
    }
    JS_ASSERT(!tm->recorder);

    /* check if our quick cache has an entry for this ip, otherwise ask fragmento. */
    jsbytecode* pc = cx->fp->regs->pc;
    Fragment* f;
    JSFragmentCacheEntry* cacheEntry = &tm->fcache[jsuword(pc) & JS_FRAGMENT_CACHE_MASK];
    if (cacheEntry->pc == pc) {
        f = cacheEntry->fragment;
    } else {
        f = tm->fragmento->getLoop(pc);
        if (!f)
            f = tm->fragmento->newLoop(pc);
        cacheEntry->pc = pc;
        cacheEntry->fragment = f;
    }

    /* If there is a chance that js_ExecuteTree will actually succeed, invoke it (either the
       first fragment must contain some code, or at least it must have a peer fragment). */
    GuardRecord* lr = NULL;
    GuardRecord* innermostNestedGuard = NULL;
    if (f->code() || f->peer)
        lr = js_ExecuteTree(cx, &f, inlineCallCount, &innermostNestedGuard);
    if (!lr) {
        JS_ASSERT(!tm->recorder);
        /* If we don't have compiled code for this entry point (none recorded or we trashed it),
           count the number of hits and trigger the recorder if appropriate. */
        if (!f->code() && (++f->hits() >= HOTLOOP))
            return js_RecordTree(cx, tm, f);
        return false;
    }
    /* If we exit on a branch, or on a tree call guard, try to grow the inner tree (in case
       of a branch exit), or the tree nested around the tree we exited from (in case of the
       tree call guard). */
    SideExit* exit = lr->exit;
    switch (exit->exitType) {
    case BRANCH_EXIT:
        return js_AttemptToExtendTree(cx, lr, lr->from->root, NULL);
    case LOOP_EXIT:
        if (innermostNestedGuard)
            return js_AttemptToExtendTree(cx, innermostNestedGuard, innermostNestedGuard->from->root, lr);
        return false;
    default:        
        /* No, this was an unusual exit (i.e. out of memory/GC), so just resume interpretation. */
        return false;
    }
}

void
js_AbortRecording(JSContext* cx, jsbytecode* abortpc, const char* reason)
{
    AUDIT(recorderAborted);
    if (cx->fp) {
        debug_only_v(if (!abortpc) abortpc = cx->fp->regs->pc;
                     printf("Abort recording (line %d, pc %d): %s.\n",
                            js_PCToLineNumber(cx, cx->fp->script, abortpc),
                            abortpc - cx->fp->script->code, reason);)
    }
    JS_ASSERT(JS_TRACE_MONITOR(cx).recorder != NULL);
    Fragment* f = JS_TRACE_MONITOR(cx).recorder->getFragment();
    JS_ASSERT(!f->vmprivate);
    f->blacklist();
    js_DeleteRecorder(cx);
    if (f->root == f) {
        /* we are recording a primary trace */
        js_TrashTree(cx, f);
    }
}

extern void
js_InitJIT(JSTraceMonitor *tm)
{
    if (!tm->fragmento) {
        JS_ASSERT(!tm->globalSlots && !tm->globalTypeMap);
        Fragmento* fragmento = new (&gc) Fragmento(core, 24);
        verbose_only(fragmento->labels = new (&gc) LabelMap(core, NULL);)
        fragmento->assm()->setCallTable(builtins);
        fragmento->pageFree(fragmento->pageAlloc()); // FIXME: prime page cache
        tm->fragmento = fragmento;
        tm->globalSlots = new (&gc) SlotList();
        tm->globalTypeMap = new (&gc) TypeMap();
    }
#if !defined XP_WIN
    debug_only(memset(&stat, 0, sizeof(stat)));
#endif
}

extern void
js_FinishJIT(JSTraceMonitor *tm)
{
#ifdef DEBUG
    printf("recorder: started(%llu), aborted(%llu), completed(%llu), different header(%llu), "
           "trees trashed(%llu), slot promoted(%llu), "
           "unstable loop variable(%llu)\n",
           stat.recorderStarted, stat.recorderAborted,
           stat.traceCompleted, stat.returnToDifferentLoopHeader, stat.treesTrashed,
           stat.slotPromoted, stat.unstableLoopVariable);
    printf("monitor: triggered(%llu), exits (%llu), type mismatch(%llu), "
           "global mismatch(%llu)\n", stat.traceTriggered, stat.sideExitIntoInterpreter,
           stat.typeMapMismatchAtEntry, stat.globalShapeMismatchAtEntry);
#endif
    if (tm->fragmento != NULL) {
        JS_ASSERT(tm->globalSlots && tm->globalTypeMap);
        verbose_only(delete tm->fragmento->labels;)
        delete tm->fragmento;
        tm->fragmento = NULL;
        delete tm->globalSlots;
        tm->globalSlots = NULL;
        delete tm->globalTypeMap;
        tm->globalTypeMap = NULL;
    }
}

extern void
js_FlushJITOracle(JSContext* cx)
{
    oracle.clear();
}

extern void
js_FlushJITCache(JSContext* cx)
{
    debug_only_v(printf("Flushing cache.\n"););
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->recorder)
        js_AbortRecording(cx, NULL, "flush cache");
    Fragmento* fragmento = tm->fragmento;
    if (fragmento) {
        fragmento->clearFrags();
#ifdef DEBUG
        JS_ASSERT(fragmento->labels);
        delete fragmento->labels;
        fragmento->labels = new (&gc) LabelMap(core, NULL);
#endif
    }
    memset(&tm->fcache, 0, sizeof(tm->fcache));
    if (cx->fp) {
        tm->globalShape = OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->shape;
        tm->globalSlots->clear();
        tm->globalTypeMap->clear();
    }
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
    JS_ASSERT(n < cx->fp->script->nslots);
    return cx->fp->slots[n];
}

jsval&
TraceRecorder::stackval(int n) const
{
    jsval* sp = cx->fp->regs->sp;
    JS_ASSERT(size_t((sp + n) - StackBase(cx->fp)) < StackDepth(cx->fp->script));
    return sp[n];
}

LIns*
TraceRecorder::scopeChain() const
{
    return lir->insLoad(LIR_ldp,
                        lir->insLoad(LIR_ldp, cx_ins, offsetof(JSContext, fp)),
                        offsetof(JSStackFrame, scopeChain));
}

static inline bool
FrameInRange(JSStackFrame* fp, JSStackFrame *target, unsigned callDepth)
{
    while (fp != target) {
        if (callDepth-- == 0)
            return false;
        if (!(fp = fp->down))
            return false;
    }
    return true;
}

bool
TraceRecorder::activeCallOrGlobalSlot(JSObject* obj, jsval*& vp)
{
    JS_ASSERT(obj != globalObj);

    JSAtom* atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    JSObject* obj2;
    JSProperty* prop;
    if (js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &obj2, &prop) < 0 || !prop)
        ABORT_TRACE("failed to find name in non-global scope chain");

    if (obj == globalObj) {
        JSScopeProperty* sprop = (JSScopeProperty*) prop;
        if (obj2 != obj || !SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj))) {
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            ABORT_TRACE("prototype or slotless globalObj property");
        }

        if (!lazilyImportGlobalSlot(sprop->slot))
             ABORT_TRACE("lazy import of global slot failed");
        vp = &STOBJ_GET_SLOT(obj, sprop->slot);
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        return true;
    }

    if (obj == obj2 && OBJ_GET_CLASS(cx, obj) == &js_CallClass) {
        JSStackFrame* cfp = (JSStackFrame*) JS_GetPrivate(cx, obj);
        if (cfp && FrameInRange(cx->fp, cfp, callDepth)) {
            JSScopeProperty* sprop = (JSScopeProperty*) prop;
            uintN slot = sprop->shortid;

            if (sprop->getter == js_GetCallArg) {
                JS_ASSERT(slot < cfp->fun->nargs);
                vp = &cfp->argv[slot];
            } else {
                JS_ASSERT(sprop->getter == js_GetCallVar);
                JS_ASSERT(slot < cfp->script->nslots);
                vp = &cfp->slots[slot];
            }
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            return true;
        }
    }

    OBJ_DROP_PROPERTY(cx, obj2, prop);
    ABORT_TRACE("fp->scopeChain is not global or active call object");
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
    return lir->insCall(F_DoubleToInt32, &f);
}

bool
TraceRecorder::ifop()
{
    jsval& v = stackval(-1);
    if (JSVAL_TAG(v) == JSVAL_BOOLEAN) {
        guard(JSVAL_TO_BOOLEAN(v) != 1,
              lir->ins_eq0(lir->ins2(LIR_eq, get(&v), lir->insImm(1))),
              BRANCH_EXIT);
    } else if (JSVAL_IS_OBJECT(v)) {
        guard(JSVAL_IS_NULL(v), lir->ins_eq0(get(&v)), BRANCH_EXIT);
    } else if (isNumber(v)) {
        jsdouble d = asNumber(v);
        jsdpun u;
        u.d = 0;
        guard((d == 0 || JSDOUBLE_IS_NaN(d)), lir->ins2(LIR_feq, get(&v), lir->insImmq(u.u64)), BRANCH_EXIT);
    } else if (JSVAL_IS_STRING(v)) {
        guard(JSSTRING_LENGTH(JSVAL_TO_STRING(v)) == 0,
              lir->ins_eq0(lir->ins2(LIR_piand,
                                     lir->insLoad(LIR_ldp, 
                                                  get(&v), 
                                                  (int)offsetof(JSString, length)),
                                     INS_CONSTPTR(JSSTRING_LENGTH_MASK))),
              BRANCH_EXIT);
    } else {
        JS_NOT_REACHED("ifop");
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
    LIns* x;
    bool cond;
    if (JSVAL_IS_STRING(l) && JSVAL_IS_STRING(r)) {
        LIns* args[] = { get(&r), get(&l) };
        x = lir->ins1(LIR_i2f, lir->insCall(F_CompareStrings, args));
        x = lir->ins2i(op, x, 0);
        jsint result = js_CompareStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        switch (op) {
          case LIR_flt:
            cond = result < 0;
            break;
          case LIR_fgt:
            cond = result > 0;
            break;
          case LIR_fle:
            cond = result <= 0;
            break;
          case LIR_fge:
            cond = result >= 0;
            break;
          default:
            JS_NOT_REACHED("unexpected comparison op for strings");
            return false;
        }
    } else if (isNumber(l) || isNumber(r)) {
        jsval tmp[2] = {l, r};
        JSAutoTempValueRooter tvr(cx, 2, tmp);

        // TODO: coerce non-numbers to numbers if it's not string-on-string above
        LIns* l_ins = get(&l);
        LIns* r_ins = get(&r);
        jsdouble lnum;
        jsdouble rnum;
        LIns* args[] = { get(&l), cx_ins };
        if (JSVAL_IS_STRING(l)) {
            l_ins = lir->insCall(F_StringToNumber, args);
        } else if (JSVAL_TAG(l) == JSVAL_BOOLEAN) {
            /*
             * What I really want here is for undefined to be type-specialized
             * differently from real booleans.  Failing that, I want to be able
             * to cmov on quads.  Failing that, I want to have small forward
             * branched.  Failing that, I want to be able to ins_choose on quads
             * without cmov.  Failing that, eat flaming builtin!
             */
            l_ins = lir->insCall(F_BooleanToNumber, args);
        } else if (!isNumber(l)) {
            ABORT_TRACE("unsupported LHS type for cmp vs number");
        }
        lnum = js_ValueToNumber(cx, &tmp[0]);

        args[0] = get(&r);
        if (JSVAL_IS_STRING(r)) {
            r_ins = lir->insCall(F_StringToNumber, args);
        } else if (JSVAL_TAG(r) == JSVAL_BOOLEAN) {
            // See above for the sob story.
            r_ins = lir->insCall(F_BooleanToNumber, args);
        } else if (!isNumber(r)) {
            ABORT_TRACE("unsupported RHS type for cmp vs number");
        }
        rnum = js_ValueToNumber(cx, &tmp[1]);

        x = lir->ins2(op, l_ins, r_ins);

        if (negate)
            x = lir->ins_eq0(x);
        switch (op) {
          case LIR_flt:
            cond = lnum < rnum;
            break;
          case LIR_fgt:
            cond = lnum > rnum;
            break;
          case LIR_fle:
            cond = lnum <= rnum;
            break;
          case LIR_fge:
            cond = lnum >= rnum;
            break;
          default:
            JS_ASSERT(op == LIR_feq);
            cond = (lnum == rnum) ^ negate;
            break;
        }
    } else {
        ABORT_TRACE("unsupported operand types for cmp");
    }

    /* The interpreter fuses comparisons and the following branch,
       so we have to do that here as well. */
    if (cx->fp->regs->pc[1] == JSOP_IFEQ || cx->fp->regs->pc[1] == JSOP_IFNE)
        guard(cond, x, BRANCH_EXIT);

    /* We update the stack after the guard. This is safe since
       the guard bails out at the comparison and the interpreter
       will this re-execute the comparison. This way the
       value of the condition doesn't have to be calculated and
       saved on the stack in most cases. */
    set(&l, x);
    return true;
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
    LIns* a = get(&l);
    LIns* b = get(&r);
    bool leftNumber = isNumber(l), rightNumber = isNumber(r);
    if ((op >= LIR_sub && op <= LIR_ush) ||  // sub, mul, (callh), or, xor, (not,) lsh, rsh, ush
        (op >= LIR_fsub && op <= LIR_fdiv)) { // fsub, fmul, fdiv
        LIns* args[] = { NULL, cx_ins };
        if (JSVAL_IS_STRING(l)) {
            args[0] = a;
            a = lir->insCall(F_StringToNumber, args);
            leftNumber = true;
        }
        if (JSVAL_IS_STRING(r)) {
            args[0] = b;
            b = lir->insCall(F_StringToNumber, args);
            rightNumber = true;
        }
    }
    if (leftNumber && rightNumber) {
        if (intop) {
            a = lir->insCall(op == LIR_ush ? F_DoubleToUint32 : F_DoubleToInt32, &a);
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

JS_STATIC_ASSERT(offsetof(JSObjectOps, newObjectMap) == 0);

bool
TraceRecorder::map_is_native(JSObjectMap* map, LIns* map_ins, LIns*& ops_ins, size_t op_offset)
{
    ops_ins = addName(lir->insLoad(LIR_ldp, map_ins, offsetof(JSObjectMap, ops)), "ops");
    LIns* n = lir->insLoad(LIR_ldp, ops_ins, op_offset);

#define OP(ops) (*(JSObjectOp*) ((char*)(ops) + op_offset))

    if (OP(map->ops) == OP(&js_ObjectOps)) {
        guard(true, addName(lir->ins2(LIR_eq, n, lir->insImmPtr((void*) OP(&js_ObjectOps))),
                            "guard(native-map)"),
              MISMATCH_EXIT);
        return true;
    }

#undef OP
    ABORT_TRACE("non-native map");
}

bool
TraceRecorder::test_property_cache(JSObject* obj, LIns* obj_ins, JSObject*& obj2, jsuword& pcval)
{
    // Mimic JSOP_CALLPROP's special case to skip up from a dense array to find
    // Array.prototype methods.
    JSObject* aobj = obj;
    if (OBJ_IS_DENSE_ARRAY(cx, obj)) {
        aobj = OBJ_GET_PROTO(cx, obj);
        obj_ins = stobj_get_fslot(obj_ins, JSSLOT_PROTO);
    }

    LIns* map_ins = lir->insLoad(LIR_ldp, obj_ins, (int)offsetof(JSObject, map));
    LIns* ops_ins;

    // Interpreter calls to PROPERTY_CACHE_TEST guard on native object ops
    // (newObjectMap == js_ObjectOps.newObjectMap) which is required to use
    // native objects (those whose maps are scopes), or even more narrow
    // conditions required because the cache miss case will call a particular
    // object-op (js_GetProperty, js_SetProperty).
    //
    // We parameterize using offsetof and guard on match against the hook at
    // the given offset in js_ObjectOps. TraceRecorder::record_JSOP_SETPROP
    // guards the js_SetProperty case.
    uint32 format = js_CodeSpec[*cx->fp->regs->pc].format;
    uint32 mode = JOF_MODE(format);
    size_t op_offset = 0;
    if (mode == JOF_PROP || mode == JOF_VARPROP) {
        JS_ASSERT(!(format & JOF_SET));
        op_offset = offsetof(JSObjectOps, getProperty);
    } else {
        JS_ASSERT(mode == JOF_NAME);
    }

    if (!map_is_native(aobj->map, map_ins, ops_ins, op_offset))
        return false;

    JSAtom* atom;
    JSPropCacheEntry* entry;
    PROPERTY_CACHE_TEST(cx, cx->fp->regs->pc, aobj, obj2, entry, atom);
    if (!atom) {
        pcval = entry->vword;
        return true;
    }

    // FIXME: suppressed exceptions from js_FindPropertyHelper and js_LookupPropertyWithFlags
    JSProperty* prop;
    JSScopeProperty* sprop;
    jsid id = ATOM_TO_JSID(atom);
    if (JOF_OPMODE(*cx->fp->regs->pc) == JOF_NAME) {
        JS_ASSERT(aobj == obj);
        if (js_FindPropertyHelper(cx, id, &obj, &obj2, &prop, &entry) < 0)
            ABORT_TRACE("failed to find name");
    } else {
        int protoIndex = js_LookupPropertyWithFlags(cx, aobj, id, 0, &obj2, &prop);
        if (protoIndex < 0)
            ABORT_TRACE("failed to lookup property");

        if (prop) {
            sprop = (JSScopeProperty*) prop;
            js_FillPropertyCache(cx, aobj, OBJ_SCOPE(aobj)->shape, 0, protoIndex, obj2, sprop,
                                 &entry);
        }
    }

    if (!prop) {
        // Propagate obj from js_FindPropertyHelper to record_JSOP_BINDNAME
        // via our obj2 out-parameter.
        obj2 = obj;
        pcval = PCVAL_NULL;
        return true;
    }

    if (!entry) {
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        ABORT_TRACE("failed to fill property cache");
    }

    if (PCVCAP_TAG(entry->vcap) <= 1) {
        if (aobj != globalObj) {
            LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)),
                                      "shape");
            guard(true, addName(lir->ins2i(LIR_eq, shape_ins, entry->kshape), "guard(shape)"),
                  MISMATCH_EXIT);
        }
    } else {
        JS_ASSERT(entry->kpc == (jsbytecode*) atom);
        JS_ASSERT(entry->kshape == jsuword(aobj));
    }

    if (PCVCAP_TAG(entry->vcap) >= 1) {
        JS_ASSERT(OBJ_SCOPE(obj2)->shape == PCVCAP_SHAPE(entry->vcap));

        LIns* obj2_ins = stobj_get_fslot(obj_ins, JSSLOT_PROTO);
        map_ins = lir->insLoad(LIR_ldp, obj2_ins, (int)offsetof(JSObject, map));
        LIns* ops_ins;
        if (!map_is_native(obj2->map, map_ins, ops_ins)) {
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            return false;
        }

        LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)), "shape");
        guard(true,
              addName(lir->ins2i(LIR_eq, shape_ins, PCVCAP_SHAPE(entry->vcap)),
                      "guard(vcap_shape)"),
              MISMATCH_EXIT);
    }

    sprop = (JSScopeProperty*) prop;
    JSScope* scope = OBJ_SCOPE(obj2);

    jsval v;

    if (format & JOF_CALLOP) {
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

    if ((((format & JOF_SET) && SPROP_HAS_STUB_SETTER(sprop)) ||
         SPROP_HAS_STUB_GETTER(sprop)) &&
        SPROP_HAS_VALID_SLOT(sprop, scope)) {
        /* Stub accessor of appropriate form and valid slot: cache slot. */
        pcval = SLOT_TO_PCVAL(sprop->slot);
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        return true;
    }

    ABORT_TRACE("no cacheable property found");
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

    /* No such property means invalid slot, which callers must check for first. */
    if (PCVAL_IS_NULL(pcval)) {
        slot = SPROP_INVALID_SLOT;
        return true;
    }

    /* Insist if setting on obj being the directly addressed object. */
    uint32 setflags = (js_CodeSpec[*cx->fp->regs->pc].format & (JOF_SET | JOF_INCDEC));
    if (setflags && obj2 != obj)
        ABORT_TRACE("JOF_SET opcode hit prototype chain");

    /* Don't trace getter or setter calls, our caller wants a direct slot. */
    if (PCVAL_IS_SPROP(pcval)) {
        JSScopeProperty* sprop = PCVAL_TO_SPROP(pcval);

        if (setflags && !SPROP_HAS_STUB_SETTER(sprop))
            ABORT_TRACE("non-stub setter");
        if (setflags != JOF_SET && !SPROP_HAS_STUB_GETTER(sprop))
            ABORT_TRACE("non-stub getter");
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
            dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
        addName(lir->insStorei(v_ins, dslots_ins,
                               (slot - JS_INITIAL_NSLOTS) * sizeof(jsval)),
                "set_slot(dslots");
    }
}

LIns*
TraceRecorder::stobj_get_fslot(LIns* obj_ins, unsigned slot)
{
    JS_ASSERT(slot < JS_INITIAL_NSLOTS);
    return lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, fslots) + slot * sizeof(jsval));
}

LIns*
TraceRecorder::stobj_get_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins)
{
    if (slot < JS_INITIAL_NSLOTS)
        return stobj_get_fslot(obj_ins, slot);

    if (!dslots_ins)
        dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    return lir->insLoad(LIR_ldp, dslots_ins, (slot - JS_INITIAL_NSLOTS) * sizeof(jsval));
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

// So box_jsval can emit no LIR_or at all to tag an object jsval.
JS_STATIC_ASSERT(JSVAL_OBJECT == 0);

bool
TraceRecorder::box_jsval(jsval v, LIns*& v_ins)
{
    if (isNumber(v)) {
        LIns* args[] = { v_ins, cx_ins };
        v_ins = lir->insCall(F_BoxDouble, args);
        guard(false, lir->ins2(LIR_eq, v_ins, INS_CONSTPTR(JSVAL_ERROR_COOKIE)),
              OOM_EXIT);
        return true;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        v_ins = lir->ins2i(LIR_or, lir->ins2i(LIR_pilsh, v_ins, JSVAL_TAGBITS), JSVAL_BOOLEAN);
        return true;
      case JSVAL_OBJECT:
        return true;
      case JSVAL_STRING:
        v_ins = lir->ins2(LIR_or, v_ins, INS_CONST(JSVAL_STRING));
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
                                     lir->ins2(LIR_piand, v_ins, INS_CONSTPTR(JSVAL_INT)),
                                     lir->ins2i(LIR_eq,
                                                lir->ins2(LIR_piand, v_ins,
                                                          INS_CONSTPTR(JSVAL_TAGMASK)),
                                                JSVAL_DOUBLE))),
              MISMATCH_EXIT);
        v_ins = lir->insCall(F_UnboxDouble, &v_ins);
        return true;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        guard(true,
              lir->ins2i(LIR_eq,
                         lir->ins2(LIR_piand, v_ins, INS_CONSTPTR(JSVAL_TAGMASK)),
                         JSVAL_BOOLEAN),
              MISMATCH_EXIT);
         v_ins = lir->ins2i(LIR_ush, v_ins, JSVAL_TAGBITS);
         return true;
       case JSVAL_OBJECT:
        guard(true,
              lir->ins2i(LIR_eq,
                         lir->ins2(LIR_piand, v_ins, INS_CONSTPTR(JSVAL_TAGMASK)),
                         JSVAL_OBJECT),
              MISMATCH_EXIT);
        return true;
      case JSVAL_STRING:
        guard(true,
              lir->ins2i(LIR_eq,
                        lir->ins2(LIR_piand, v_ins, INS_CONSTPTR(JSVAL_TAGMASK)),
                        JSVAL_STRING),
              MISMATCH_EXIT);
        v_ins = lir->ins2(LIR_piand, v_ins, INS_CONSTPTR(~JSVAL_TAGMASK));
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
        guard(false, lir->ins_eq0(this_ins), MISMATCH_EXIT);
    } else { /* in global code */
        JS_ASSERT(!JSVAL_IS_NULL(cx->fp->argv[-1]));
        this_ins = scopeChain();
    }
    return true;
}

bool
TraceRecorder::guardClass(JSObject* obj, LIns* obj_ins, JSClass* clasp)
{
    if (STOBJ_GET_CLASS(obj) != clasp)
        return false;

    LIns* class_ins = stobj_get_fslot(obj_ins, JSSLOT_CLASS);
    class_ins = lir->ins2(LIR_piand, class_ins, lir->insImmPtr((void*)~3));

    char namebuf[32];
    JS_snprintf(namebuf, sizeof namebuf, "guard(class is %s)", clasp->name);
    guard(true, addName(lir->ins2(LIR_eq, class_ins, lir->insImmPtr(clasp)), namebuf),
            MISMATCH_EXIT);
    return true;
}

bool
TraceRecorder::guardDenseArray(JSObject* obj, LIns* obj_ins)
{
    return guardClass(obj, obj_ins, &js_ArrayClass);
}

bool
TraceRecorder::guardDenseArrayIndex(JSObject* obj, jsint idx, LIns* obj_ins,
                                    LIns* dslots_ins, LIns* idx_ins)
{
    jsuint length = ARRAY_DENSE_LENGTH(obj);
    if (!((jsuint)idx < length && idx < obj->fslots[JSSLOT_ARRAY_LENGTH]))
        return false;

    LIns* length_ins = stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH);

    // guard(index >= 0)
    guard(true, lir->ins2i(LIR_ge, idx_ins, 0), MISMATCH_EXIT);

    // guard(index < length)
    guard(true, lir->ins2(LIR_lt, idx_ins, length_ins), MISMATCH_EXIT);

    // guard(index < capacity)
    guard(false, lir->ins_eq0(dslots_ins), MISMATCH_EXIT);
    guard(true,
          lir->ins2(LIR_lt, idx_ins, lir->insLoad(LIR_ldp, dslots_ins, 0 - sizeof(jsval))),
          MISMATCH_EXIT);
    return true;
}

void
TraceRecorder::clearFrameSlotsFromCache()
{
    /* Clear out all slots of this frame in the nativeFrameTracker. Different locations on the
       VM stack might map to different locations on the native stack depending on the
       number of arguments (i.e.) of the next call, so we have to make sure we map
       those in to the cache with the right offsets. */
    JSStackFrame* fp = cx->fp;
    jsval* vp;
    jsval* vpstop;
    if (fp->callee) {
        vp = &fp->argv[-2];
        vpstop = &fp->argv[JS_MAX(fp->fun->nargs,fp->argc)];
        while (vp < vpstop)
            nativeFrameTracker.set(vp++, (LIns*)0);
    }
    vp = &fp->slots[0];
    vpstop = &fp->slots[fp->script->nslots];
    while (vp < vpstop)
        nativeFrameTracker.set(vp++, (LIns*)0);
}

bool
TraceRecorder::record_EnterFrame()
{
    if (++callDepth >= MAX_CALLDEPTH)
        ABORT_TRACE("exceeded maximum call depth");
    debug_only_v(printf("EnterFrame %s, callDepth=%d\n",
                        js_AtomToPrintableString(cx, cx->fp->fun->atom),
                        callDepth););
    JSStackFrame* fp = cx->fp;
    LIns* void_ins = lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID));

    jsval* vp = &fp->argv[fp->argc];
    jsval* vpstop = vp + (fp->fun->nargs - fp->argc);
    while (vp < vpstop) {
        if (vp >= fp->down->regs->sp)
            nativeFrameTracker.set(vp, (LIns*)0);
        set(vp++, void_ins, true);
    }

    vp = &fp->slots[0];
    vpstop = vp + fp->script->nfixed;
    while (vp < vpstop)
        set(vp++, void_ins, true);
    return true;
}

bool
TraceRecorder::record_LeaveFrame()
{
    debug_only_v(
        if (cx->fp->fun)
            printf("LeaveFrame (back to %s), callDepth=%d\n",
                   js_AtomToPrintableString(cx, cx->fp->fun->atom),
                   callDepth);
        );
    if (callDepth-- <= 0)
        ABORT_TRACE("returned out of a loop we started tracing");

    // LeaveFrame gets called after the interpreter popped the frame and
    // stored rval, so cx->fp not cx->fp->down, and -1 not 0.
    atoms = cx->fp->script->atomMap.vector;
    set(&stackval(-1), rval_ins, true);
    return true;
}

bool TraceRecorder::record_JSOP_INTERRUPT()
{
    return false;
}

bool
TraceRecorder::record_JSOP_PUSH()
{
    stack(0, lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID)));
    return true;
}

bool
TraceRecorder::record_JSOP_POPV()
{
    // We should not have to implement JSOP_POPV or JSOP_STOP's rval setting.
    return false;
}

bool TraceRecorder::record_JSOP_ENTERWITH()
{
    return false;
}
bool TraceRecorder::record_JSOP_LEAVEWITH()
{
    return false;
}

bool
TraceRecorder::record_JSOP_RETURN()
{
    jsval& rval = stackval(-1);
    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
        // guard JSVAL_IS_PRIMITIVE(rval)
    }
    debug_only_v(printf("returning from %s\n", js_AtomToPrintableString(cx, cx->fp->fun->atom)););
    rval_ins = get(&rval);
    clearFrameSlotsFromCache();
    return true;
}

bool
TraceRecorder::record_JSOP_GOTO()
{
    return true;
}

bool
TraceRecorder::record_JSOP_IFEQ()
{
    return ifop();
}

bool
TraceRecorder::record_JSOP_IFNE()
{
    return ifop();
}

bool
TraceRecorder::record_JSOP_ARGUMENTS()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DUP()
{
    stack(0, get(&stackval(-1)));
    return true;
}

bool
TraceRecorder::record_JSOP_DUP2()
{
    stack(0, get(&stackval(-2)));
    stack(1, get(&stackval(-1)));
    return true;
}

bool
TraceRecorder::record_JSOP_SETCONST()
{
    return false;
}

bool
TraceRecorder::record_JSOP_BITOR()
{
    return binary(LIR_or);
}

bool
TraceRecorder::record_JSOP_BITXOR()
{
    return binary(LIR_xor);
}

bool
TraceRecorder::record_JSOP_BITAND()
{
    return binary(LIR_and);
}

// See FIXME for JSOP_STRICTEQ before evolving JSOP_EQ to handle mixed types.
bool
TraceRecorder::record_JSOP_EQ()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (JSVAL_IS_STRING(l) && JSVAL_IS_STRING(r)) {
        LIns* args[] = { get(&r), get(&l) };
        bool cond = js_EqualStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        LIns* x = lir->ins_eq0(lir->ins_eq0(lir->insCall(F_EqualStrings, args)));
        /* The interpreter fuses comparisons and the following branch,
           so we have to do that here as well. */
        if (cx->fp->regs->pc[1] == JSOP_IFEQ || cx->fp->regs->pc[1] == JSOP_IFNE)
            guard(cond, x, BRANCH_EXIT);

        /* We update the stack after the guard. This is safe since
           the guard bails out at the comparison and the interpreter
           will this re-execute the comparison. This way the
           value of the condition doesn't have to be calculated and
           saved on the stack in most cases. */
        set(&l, x);
        return true;
    }
    if (JSVAL_IS_OBJECT(l) && JSVAL_IS_OBJECT(r)) {
        bool cond = (l == r);
        LIns* x = lir->ins2(LIR_eq, get(&l), get(&r));
        /* The interpreter fuses comparisons and the following branch,
           so we have to do that here as well. */
        if (cx->fp->regs->pc[1] == JSOP_IFEQ || cx->fp->regs->pc[1] == JSOP_IFNE)
            guard(cond, x, BRANCH_EXIT);

        /* We update the stack after the guard. This is safe since
           the guard bails out at the comparison and the interpreter
           will this re-execute the comparison. This way the
           value of the condition doesn't have to be calculated and
           saved on the stack in most cases. */
        set(&l, x);
        return true;
    }
    return cmp(LIR_feq);
}

// See FIXME for JSOP_STRICTNE before evolving JSOP_NE to handle mixed types.
bool
TraceRecorder::record_JSOP_NE()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (JSVAL_IS_STRING(l) && JSVAL_IS_STRING(r)) {
        LIns* args[] = { get(&r), get(&l) };
        bool cond = !js_EqualStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        LIns* x = lir->ins_eq0(lir->insCall(F_EqualStrings, args));
        /* The interpreter fuses comparisons and the following branch,
           so we have to do that here as well. */
        if (cx->fp->regs->pc[1] == JSOP_IFEQ || cx->fp->regs->pc[1] == JSOP_IFNE)
            guard(cond, x, BRANCH_EXIT);

        /* We update the stack after the guard. This is safe since
           the guard bails out at the comparison and the interpreter
           will this re-execute the comparison. This way the
           value of the condition doesn't have to be calculated and
           saved on the stack in most cases. */
        set(&l, x);
        return true;
    }
    if (JSVAL_IS_OBJECT(l) && JSVAL_IS_OBJECT(r)) {
        bool cond = (l != r);
        LIns* x = lir->ins_eq0(lir->ins2(LIR_eq, get(&l), get(&r)));
        /* The interpreter fuses comparisons and the following branch,
           so we have to do that here as well. */
        if (cx->fp->regs->pc[1] == JSOP_IFEQ || cx->fp->regs->pc[1] == JSOP_IFNE)
            guard(cond, x, BRANCH_EXIT);

        /* We update the stack after the guard. This is safe since
           the guard bails out at the comparison and the interpreter
           will this re-execute the comparison. This way the
           value of the condition doesn't have to be calculated and
           saved on the stack in most cases. */
        set(&l, x);
        return true;
    }
    return cmp(LIR_feq, true);
}

bool
TraceRecorder::record_JSOP_LT()
{
    return cmp(LIR_flt);
}

bool
TraceRecorder::record_JSOP_LE()
{
    return cmp(LIR_fle);
}

bool
TraceRecorder::record_JSOP_GT()
{
    return cmp(LIR_fgt);
}

bool
TraceRecorder::record_JSOP_GE()
{
    return cmp(LIR_fge);
}

bool
TraceRecorder::record_JSOP_LSH()
{
    return binary(LIR_lsh);
}

bool
TraceRecorder::record_JSOP_RSH()
{
    return binary(LIR_rsh);
}

bool
TraceRecorder::record_JSOP_URSH()
{
    return binary(LIR_ush);
}

bool
TraceRecorder::record_JSOP_ADD()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    if (JSVAL_IS_STRING(l)) {
        LIns* args[] = { NULL, get(&l), cx_ins };
        if (JSVAL_IS_STRING(r)) {
            args[0] = get(&r);
        } else {
            LIns* args2[] = { get(&r), cx_ins };
            if (JSVAL_IS_NUMBER(r)) {
                args[0] = lir->insCall(F_NumberToString, args2);
            } else if (JSVAL_IS_OBJECT(r)) {
                args[0] = lir->insCall(F_ObjectToString, args2);
            } else {
                ABORT_TRACE("untraceable right operand to string-JSOP_ADD");
            }
            guard(false, lir->ins_eq0(args[0]), OOM_EXIT);
        }
        LIns* concat = lir->insCall(F_ConcatStrings, args);
        guard(false, lir->ins_eq0(concat), OOM_EXIT);
        set(&l, concat);
        return true;
    }
    return binary(LIR_fadd);
}

bool
TraceRecorder::record_JSOP_SUB()
{
    return binary(LIR_fsub);
}

bool
TraceRecorder::record_JSOP_MUL()
{
    return binary(LIR_fmul);
}

bool
TraceRecorder::record_JSOP_DIV()
{
    return binary(LIR_fdiv);
}

bool
TraceRecorder::record_JSOP_MOD()
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

bool
TraceRecorder::record_JSOP_NOT()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_BOOLEAN(v) || JSVAL_IS_OBJECT(v)) {
        set(&v, lir->ins_eq0(get(&v)));
        return true;
    }
    return false;
}

bool
TraceRecorder::record_JSOP_BITNOT()
{
    return unary(LIR_not);
}

bool
TraceRecorder::record_JSOP_NEG()
{
    return unary(LIR_fneg);
}

enum JSTNErrType { INFALLIBLE, FAIL_NULL, FAIL_NEG, FAIL_VOID };
struct JSTraceableNative {
    JSFastNative native;
    int          builtin;
    const char  *prefix;
    const char  *argtypes;
    JSTNErrType  errtype;
    JSClass     *tclasp;
};

JSBool
js_Array(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

bool
TraceRecorder::record_JSOP_NEW()
{
    jsbytecode *pc = cx->fp->regs->pc;
    /* Get immediate argc and find the constructor function. */
    unsigned argc = GET_ARGC(cx->fp->regs->pc);
    jsval& fval = stackval(0 - (2 + argc));
    JS_ASSERT(&fval >= StackBase(cx->fp));

    /*
     * Require that the callee be a function object, to avoid guarding on its
     * class here. We know if the callee and this were pushed by JSOP_CALLNAME
     * or JSOP_CALLPROP that callee is a *particular* function, since these hit
     * the property cache and guard on the object (this) in which the callee
     * was found. So it's sufficient to test here that the particular function
     * is interpreted, not guard on that condition.
     *
     * Bytecode sequences that push shapeless callees must guard on the callee
     * class being Function and the function being interpreted.
     */
    JS_ASSERT(VALUE_IS_FUNCTION(cx, fval));
    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(fval));

    if (FUN_INTERPRETED(fun)) {
        LIns* args[] = { get(&fval), cx_ins };
        LIns* tv_ins = lir->insCall(F_FastNewObject, args);
        guard(false, lir->ins_eq0(tv_ins), OOM_EXIT);
        jsval& tv = stackval(0 - (1 + argc));
        set(&tv, tv_ins);
        return interpretedFunctionCall(fval, fun, argc);
    }

    static JSTraceableNative knownNatives[] = {
        { (JSFastNative)js_Array, F_Array_1int, "fC", "i", FAIL_NULL, NULL },
    };

    for (uintN i = 0; i < JS_ARRAY_LENGTH(knownNatives); i++) {
        JSTraceableNative* known = &knownNatives[i];
        if ((JSFastNative)fun->u.n.native != known->native)
            continue;

        uintN knownargc = strlen(known->argtypes);
        if (argc != knownargc)
            continue;

        intN prefixc = strlen(known->prefix);
        LIns* args[5];
        LIns** argp = &args[argc + prefixc - 1];
        char argtype;

#if defined _DEBUG
        memset(args, 0xCD, sizeof(args));
#endif

        jsval& thisval = stackval(0 - (argc + 1));
        LIns* thisval_ins = get(&thisval);
        if (known->tclasp &&
            !JSVAL_IS_PRIMITIVE(thisval) &&
            !guardClass(JSVAL_TO_OBJECT(thisval), thisval_ins, known->tclasp)) {
            continue; /* might have another specialization for |this| */
        }

#define HANDLE_PREFIX(i)                                                       \
    JS_BEGIN_MACRO                                                             \
        argtype = known->prefix[i];                                            \
        if (argtype == 'C') {                                                  \
            *argp = cx_ins;                                                    \
        } else if (argtype == 'T') {                                           \
            *argp = thisval_ins;                                               \
        } else if (argtype == 'R') {                                           \
            *argp = lir->insImmPtr((void*)cx->runtime);                        \
        } else if (argtype == 'P') {                                           \
            *argp = lir->insImmPtr(pc);                                        \
        } else if (argtype == 'f') {                                           \
            *argp = lir->insImmPtr((void*)JSVAL_TO_OBJECT(fval));              \
        } else {                                                               \
            JS_NOT_REACHED("unknown prefix arg type");                         \
        }                                                                      \
        argp--;                                                                \
    JS_END_MACRO

        switch (prefixc) {
          case 3:
            HANDLE_PREFIX(2);
            /* FALL THROUGH */
          case 2:
            HANDLE_PREFIX(1);
            /* FALL THROUGH */
          case 1:
            HANDLE_PREFIX(0);
            /* FALL THROUGH */
          case 0:
            break;
          default:
            JS_NOT_REACHED("illegal number of prefix args");
        }

#undef HANDLE_PREFIX

#define HANDLE_ARG(i)                                                          \
    {                                                                          \
        jsval& arg = stackval(-(i + 1));                                       \
        argtype = known->argtypes[i];                                          \
        if (argtype == 'd' || argtype == 'i') {                                \
            if (!isNumber(arg))                                                \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
            if (argtype == 'i')                                                \
                *argp = f2i(*argp);                                            \
        } else if (argtype == 's') {                                           \
            if (!JSVAL_IS_STRING(arg))                                         \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
        } else if (argtype == 'r') {                                           \
            if (!VALUE_IS_REGEXP(cx, arg))                                     \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
        } else if (argtype == 'f') {                                           \
            if (!VALUE_IS_FUNCTION(cx, arg))                                   \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
        } else {                                                               \
            continue;     /* might have another specialization for arg */      \
        }                                                                      \
        argp--;                                                                \
    }

        switch (strlen(known->argtypes)) {
          case 4:
            HANDLE_ARG(3);
            /* FALL THROUGH */
          case 3:
            HANDLE_ARG(2);
            /* FALL THROUGH */
          case 2:
            HANDLE_ARG(1);
            /* FALL THROUGH */
          case 1:
            HANDLE_ARG(0);
            /* FALL THROUGH */
          case 0:
            break;
          default:
            JS_NOT_REACHED("illegal number of args to traceable native");
        }

#undef HANDLE_ARG

#if defined _DEBUG
        JS_ASSERT(args[0] != (LIns *)0xcdcdcdcd);
#endif

        LIns* res_ins = lir->insCall(known->builtin, args);
        switch (known->errtype) {
          case FAIL_NULL:
            guard(false, lir->ins_eq0(res_ins), OOM_EXIT);
            break;
          case FAIL_NEG:
          {
            res_ins = lir->ins1(LIR_i2f, res_ins);
            jsdpun u;
            u.d = 0.0;
            guard(false, lir->ins2(LIR_flt, res_ins, lir->insImmq(u.u64)), OOM_EXIT);
            break;
          }
          case FAIL_VOID:
            guard(false, lir->ins2i(LIR_eq, res_ins, 2), OOM_EXIT);
            break;
          default:;
        }
        set(&fval, res_ins);
        return true;
    }

    if (fun->u.n.clasp)
        ABORT_TRACE("can't trace native constructor");

    ABORT_TRACE("can't trace unknown constructor");
}

bool
TraceRecorder::record_JSOP_DELNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DELPROP()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DELELEM()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TYPEOF()
{
    jsval& r = stackval(-1);
    LIns* type;
    if (JSVAL_IS_STRING(r)) {
        type = lir->insImmPtr((void*)cx->runtime->atomState.typeAtoms[JSTYPE_STRING]);
    } else if (isNumber(r)) {
        type = lir->insImmPtr((void*)cx->runtime->atomState.typeAtoms[JSTYPE_NUMBER]);
    } else {
        LIns* args[] = { get(&r), cx_ins };
        if (JSVAL_TAG(r) == JSVAL_BOOLEAN) {
            // We specialize identically for boolean and undefined. We must not have a hole here.
            // Pass the unboxed type here, since TypeOfBoolean knows how to handle it.
            JS_ASSERT(JSVAL_TO_BOOLEAN(r) <= 2);
            type = lir->insCall(F_TypeOfBoolean, args);
        } else {
            JS_ASSERT(JSVAL_IS_OBJECT(r));
            type = lir->insCall(F_TypeOfObject, args);
        }
    }
    set(&r, type);
    return true;
}

bool
TraceRecorder::record_JSOP_VOID()
{
    stack(0, lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID)));
    return true;
}

JSBool
js_num_parseFloat(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_num_parseInt(JSContext* cx, uintN argc, jsval* vp);

bool
TraceRecorder::record_JSOP_INCNAME()
{
    return incName(1);
}

bool
TraceRecorder::record_JSOP_INCPROP()
{
    return incProp(1);
}

bool
TraceRecorder::record_JSOP_INCELEM()
{
    return incElem(1);
}

bool
TraceRecorder::record_JSOP_DECNAME()
{
    return incName(-1);
}

bool
TraceRecorder::record_JSOP_DECPROP()
{
    return incProp(-1);
}

bool
TraceRecorder::record_JSOP_DECELEM()
{
    return incElem(-1);
}

bool
TraceRecorder::incName(jsint incr, bool pre)
{
    jsval* vp;
    if (!name(vp))
        return false;
    LIns* v_ins = get(vp);
    if (!inc(*vp, v_ins, incr, pre))
        return false;
    set(vp, v_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_NAMEINC()
{
    return incName(1, false);
}

bool
TraceRecorder::record_JSOP_PROPINC()
{
    return incProp(1, false);
}

// XXX consolidate with record_JSOP_GETELEM code...
bool
TraceRecorder::record_JSOP_ELEMINC()
{
    return incElem(1, false);
}

bool
TraceRecorder::record_JSOP_NAMEDEC()
{
    return incName(-1, true);
}

bool
TraceRecorder::record_JSOP_PROPDEC()
{
    return incProp(-1, false);
}

bool
TraceRecorder::record_JSOP_ELEMDEC()
{
    return incElem(-1, false);
}

bool
TraceRecorder::record_JSOP_GETPROP()
{
    return getProp(stackval(-1));
}

bool
TraceRecorder::record_JSOP_SETPROP()
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
    if (!PCVAL_IS_SPROP(entry->vword))
        ABORT_TRACE("hit non-sprop cache value");

    LIns* map_ins = lir->insLoad(LIR_ldp, obj_ins, (int)offsetof(JSObject, map));
    LIns* ops_ins;
    if (!map_is_native(obj->map, map_ins, ops_ins, offsetof(JSObjectOps, setProperty)))
        return false;

    // The global object's shape is guarded at trace entry.
    if (obj != globalObj) {
        LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)), "shape");
        guard(true, addName(lir->ins2i(LIR_eq, shape_ins, kshape), "guard(shape)"),
                MISMATCH_EXIT);
    }

    JSScope* scope = OBJ_SCOPE(obj);
    JSScopeProperty* sprop = PCVAL_TO_SPROP(entry->vword);
    if (scope->object != obj || !SCOPE_HAS_PROPERTY(scope, sprop)) {
        LIns* args[] = { lir->insImmPtr(sprop), obj_ins, cx_ins };
        LIns* ok_ins = lir->insCall(F_AddProperty, args);
        guard(false, lir->ins_eq0(ok_ins), MISMATCH_EXIT);
    }

    LIns* dslots_ins = NULL;
    LIns* v_ins = get(&r);
    LIns* boxed_ins = v_ins;
    if (!box_jsval(r, boxed_ins))
        return false;
    if (!native_set(obj_ins, sprop, dslots_ins, boxed_ins))
        return false;
    if (*pc == JSOP_SETPROP && pc[JSOP_SETPROP_LENGTH] != JSOP_POP)
        stack(-2, v_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_GETELEM()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (JSVAL_IS_STRING(l) && JSVAL_IS_INT(r)) {
        LIns* args[] = { f2i(get(&r)), get(&l), cx_ins };
        LIns* unitstr_ins = lir->insCall(F_String_getelem, args);
        guard(false, lir->ins_eq0(unitstr_ins), MISMATCH_EXIT);
        set(&l, unitstr_ins);
        return true;
    }

    if (!JSVAL_IS_PRIMITIVE(l) && JSVAL_IS_STRING(r)) {
        jsval v;
        jsid id;

        if (!js_ValueToStringId(cx, r, &id))
            return false;
        r = ID_TO_VALUE(id);
        if (!OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(l), id, &v))
            return false;

        LIns* args[] = { get(&r), get(&l), cx_ins };
        LIns* v_ins = lir->insCall(F_Any_getelem, args);
        guard(false, lir->ins2(LIR_eq, v_ins, INS_CONSTPTR(JSVAL_ERROR_COOKIE)),
              MISMATCH_EXIT);
        if (!unbox_jsval(v, v_ins))
            ABORT_TRACE("JSOP_GETELEM");
        set(&l, v_ins);
        return true;
    }

    jsval* vp;
    LIns* v_ins;
    LIns* addr_ins;
    if (!elem(l, r, vp, v_ins, addr_ins))
        return false;
    set(&l, v_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_SETELEM()
{
    jsval& v = stackval(-1);
    jsval& r = stackval(-2);
    jsval& l = stackval(-3);

    /* no guards for type checks, trace specialized this already */
    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("left JSOP_SETELEM operand is not an object");
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);

    if (JSVAL_IS_STRING(r)) {
        LIns* v_ins = get(&v);
        LIns* unboxed_v_ins = v_ins;
        if (!box_jsval(v, v_ins))
            ABORT_TRACE("boxing string-indexed JSOP_SETELEM value");
        LIns* args[] = { v_ins, get(&r), get(&l), cx_ins };
        LIns* ok_ins = lir->insCall(F_Any_setelem, args);
        guard(false, lir->ins_eq0(ok_ins), MISMATCH_EXIT);
        set(&l, unboxed_v_ins);
        return true;
    }
    if (!JSVAL_IS_INT(r))
        ABORT_TRACE("non-string, non-int JSOP_SETELEM index");

    /* make sure the object is actually a dense array */
    if (!guardDenseArray(obj, obj_ins))
        ABORT_TRACE("not a dense array");

    /* check that the index is within bounds */
    LIns* idx_ins = f2i(get(&r));

    /* we have to check that its really an integer, but this check will to go away
       once we peel the loop type down to integer for this slot */
    guard(true, lir->ins2(LIR_feq, get(&r), lir->ins1(LIR_i2f, idx_ins)), MISMATCH_EXIT);
    /* ok, box the value we are storing, store it and we are done */
    LIns* v_ins = get(&v);
    LIns* boxed_ins = v_ins;
    if (!box_jsval(v, boxed_ins))
        ABORT_TRACE("boxing failed");
    LIns* args[] = { boxed_ins, idx_ins, obj_ins, cx_ins };
    LIns* res_ins = lir->insCall(F_Array_dense_setelem, args);
    guard(false, lir->ins_eq0(res_ins), MISMATCH_EXIT);

    jsbytecode* pc = cx->fp->regs->pc;
    if (*pc == JSOP_SETELEM && pc[JSOP_SETELEM_LENGTH] != JSOP_POP)
        set(&l, v_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_CALLNAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj) {
        jsval* vp;
        if (!activeCallOrGlobalSlot(obj, vp))
            return false;
        stack(0, get(vp));
        stack(1, lir->insImmPtr(NULL));
        return true;
    }

    LIns* obj_ins = scopeChain();
    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;

    if (PCVAL_IS_NULL(pcval) || !PCVAL_IS_OBJECT(pcval))
        ABORT_TRACE("callee is not an object");
    JS_ASSERT(HAS_FUNCTION_CLASS(PCVAL_TO_OBJECT(pcval)));

    stack(0, lir->insImmPtr(PCVAL_TO_OBJECT(pcval)));
    stack(1, obj_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_GETUPVAR()
{
    ABORT_TRACE("GETUPVAR");
}

bool
TraceRecorder::record_JSOP_CALLUPVAR()
{
    ABORT_TRACE("CALLUPVAR");
}

bool
TraceRecorder::guardShapelessCallee(jsval& callee)
{
    if (!VALUE_IS_FUNCTION(cx, callee))
        ABORT_TRACE("shapeless callee is not a function");

    guard(true,
          addName(lir->ins2(LIR_eq, get(&callee), INS_CONSTPTR(JSVAL_TO_OBJECT(callee))),
                  "guard(shapeless callee)"),
          MISMATCH_EXIT);
    return true;
}

bool
TraceRecorder::interpretedFunctionCall(jsval& fval, JSFunction* fun, uintN argc)
{
    JSStackFrame* fp = cx->fp;

    // TODO: track the copying via the tracker...
    if (argc < fun->nargs &&
        jsuword(fp->regs->sp + (fun->nargs - argc)) > cx->stackPool.current->limit) {
        ABORT_TRACE("can't trace calls with too few args requiring argv move");
    }

    FrameInfo fi = {
        JSVAL_TO_OBJECT(fval),
        fp->regs->pc,
        fp->regs->sp - fp->slots,
        argc
    };

    unsigned callDepth = getCallDepth();
    if (callDepth >= treeInfo->maxCallDepth)
        treeInfo->maxCallDepth = callDepth + 1;

    lir->insStorei(lir->insImmPtr(fi.callee), lirbuf->rp,
                   callDepth * sizeof(FrameInfo) + offsetof(FrameInfo, callee));
    lir->insStorei(lir->insImmPtr(fi.callpc), lirbuf->rp,
                   callDepth * sizeof(FrameInfo) + offsetof(FrameInfo, callpc));
    lir->insStorei(lir->insImm(fi.word), lirbuf->rp,
                   callDepth * sizeof(FrameInfo) + offsetof(FrameInfo, word));

    atoms = fun->u.i.script->atomMap.vector;
    return true;
}

JSBool
js_math_sin(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_math_cos(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_math_pow(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_math_sqrt(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_str_substring(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_str_fromCharCode(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_str_charCodeAt(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_str_charAt(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_str_concat(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_math_random(JSContext* cx, uintN argc, jsval* vp);

JSBool
js_math_floor(JSContext* cx, uintN argc, jsval* vp);

bool
TraceRecorder::record_JSOP_CALL()
{
    jsbytecode *pc = cx->fp->regs->pc;
    uintN argc = GET_ARGC(pc);
    jsval& fval = stackval(0 - (argc + 2));
    jsval& tval = stackval(0 - (argc + 1));

    LIns* this_ins = get(&tval);
    if (this_ins->isconstp() && !this_ins->constvalp() && !guardShapelessCallee(fval))
        return false;
    
    /*
     * Require that the callee be a function object, to avoid guarding on its
     * class here. We know if the callee and this were pushed by JSOP_CALLNAME
     * or JSOP_CALLPROP that callee is a *particular* function, since these hit
     * the property cache and guard on the object (this) in which the callee
     * was found. So it's sufficient to test here that the particular function
     * is interpreted, not guard on that condition.
     *
     * Bytecode sequences that push shapeless callees must guard on the callee
     * class being Function and the function being interpreted.
     */
    JS_ASSERT(VALUE_IS_FUNCTION(cx, fval));
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(fval));

    if (FUN_INTERPRETED(fun))
        return interpretedFunctionCall(fval, fun, argc);

    /*
     * Handle dear old eval here, it's a slow native but a special one, judging
     * from benchmarks. FIXME: we need a post-eval recording hook to know which
     * type tag to unbox from.
     */
    if (FUN_SLOW_NATIVE(fun)) 
        ABORT_TRACE("slow native");

    static JSTraceableNative knownNatives[] = {
        { js_array_join,               F_Array_p_join,         "TC",  "s",    FAIL_NULL,   NULL },
        { js_math_sin,                 F_Math_sin,             "",    "d",    INFALLIBLE,  NULL },
        { js_math_cos,                 F_Math_cos,             "",    "d",    INFALLIBLE,  NULL },
        { js_math_pow,                 F_Math_pow,             "",   "dd",    INFALLIBLE,  NULL },
        { js_math_sqrt,                F_Math_sqrt,            "",    "d",    INFALLIBLE,  NULL },
        { js_math_floor,               F_Math_floor,           "",    "d",    INFALLIBLE,  NULL },
        { js_math_random,              F_Math_random,          "R",    "",    INFALLIBLE,  NULL },
        { js_num_parseInt,             F_ParseInt,             "C",   "s",    INFALLIBLE,  NULL },
        { js_num_parseFloat,           F_ParseFloat,           "C",   "s",    INFALLIBLE,  NULL },
        { js_obj_hasOwnProperty,       F_Object_p_hasOwnProperty,
                                                               "TC",  "s",    FAIL_VOID,   NULL },
        { js_obj_propertyIsEnumerable, F_Object_p_propertyIsEnumerable,
                                                               "TC",  "s",    FAIL_VOID,   NULL },
        { js_str_charAt,               F_String_getelem,       "TC",  "i",    FAIL_NULL,   NULL },
        { js_str_charCodeAt,           F_String_p_charCodeAt,  "T",   "i",    FAIL_NEG,    NULL },
        { js_str_concat,               F_String_p_concat_1int, "TC",  "i",    FAIL_NULL,   NULL },
        { js_str_fromCharCode,         F_String_fromCharCode,  "C",   "i",    FAIL_NULL,   NULL },
        { js_str_match,                F_String_p_match,       "PTC", "r",    FAIL_VOID,   NULL },
        { js_str_replace,              F_String_p_replace_str, "TC", "sr",    FAIL_NULL,   NULL },
        { js_str_replace,              F_String_p_replace_str3,"TC","sss",    FAIL_NULL,   NULL },
        { js_str_split,                F_String_p_split,       "TC",  "s",    FAIL_NULL,   NULL },
        { js_str_substring,            F_String_p_substring,   "TC", "ii",    FAIL_NULL,   NULL },
        { js_str_substring,            F_String_p_substring_1, "TC",  "i",    FAIL_NULL,   NULL },
    };

    for (uintN i = 0; i < JS_ARRAY_LENGTH(knownNatives); i++) {
        JSTraceableNative* known = &knownNatives[i];
        if ((JSFastNative)fun->u.n.native != known->native)
            continue;

        uintN knownargc = strlen(known->argtypes);
        if (argc != knownargc)
            continue;

        intN prefixc = strlen(known->prefix);
        LIns* args[5];
        LIns** argp = &args[argc + prefixc - 1];
        char argtype;

#if defined _DEBUG
        memset(args, 0xCD, sizeof(args));
#endif

        jsval& thisval = stackval(0 - (argc + 1));
        LIns* thisval_ins = get(&thisval);
        if (known->tclasp &&
            !JSVAL_IS_PRIMITIVE(thisval) &&
            !guardClass(JSVAL_TO_OBJECT(thisval), thisval_ins, known->tclasp)) {
            continue; /* might have another specialization for |this| */
        }

#define HANDLE_PREFIX(i)                                                       \
    JS_BEGIN_MACRO                                                             \
        argtype = known->prefix[i];                                            \
        if (argtype == 'C') {                                                  \
            *argp = cx_ins;                                                    \
        } else if (argtype == 'T') {                                           \
            *argp = thisval_ins;                                               \
        } else if (argtype == 'R') {                                           \
            *argp = lir->insImmPtr((void*)cx->runtime);                        \
        } else if (argtype == 'P') {                                           \
            *argp = lir->insImmPtr(pc);                                        \
        } else {                                                               \
            JS_NOT_REACHED("unknown prefix arg type");                         \
        }                                                                      \
        argp--;                                                                \
    JS_END_MACRO

        switch (prefixc) {
          case 3:
            HANDLE_PREFIX(2);
            /* FALL THROUGH */
          case 2:
            HANDLE_PREFIX(1);
            /* FALL THROUGH */
          case 1:
            HANDLE_PREFIX(0);
            /* FALL THROUGH */
          case 0:
            break;
          default:
            JS_NOT_REACHED("illegal number of prefix args");
        }

#undef HANDLE_PREFIX

#define HANDLE_ARG(i)                                                          \
    {                                                                          \
        jsval& arg = stackval(-(i + 1));                                       \
        argtype = known->argtypes[i];                                          \
        if (argtype == 'd' || argtype == 'i') {                                \
            if (!isNumber(arg))                                                \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
            if (argtype == 'i')                                                \
                *argp = f2i(*argp);                                            \
        } else if (argtype == 's') {                                           \
            if (!JSVAL_IS_STRING(arg))                                         \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
        } else if (argtype == 'r') {                                           \
            if (!VALUE_IS_REGEXP(cx, arg))                                     \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
        } else if (argtype == 'f') {                                           \
            if (!VALUE_IS_FUNCTION(cx, arg))                                   \
                continue; /* might have another specialization for arg */      \
            *argp = get(&arg);                                                 \
        } else {                                                               \
            continue;     /* might have another specialization for arg */      \
        }                                                                      \
        argp--;                                                                \
    }

        switch (strlen(known->argtypes)) {
          case 4:
            HANDLE_ARG(3);
            /* FALL THROUGH */
          case 3:
            HANDLE_ARG(2);
            /* FALL THROUGH */
          case 2:
            HANDLE_ARG(1);
            /* FALL THROUGH */
          case 1:
            HANDLE_ARG(0);
            /* FALL THROUGH */
          case 0:
            break;
          default:
            JS_NOT_REACHED("illegal number of args to traceable native");
        }

#undef HANDLE_ARG

#if defined _DEBUG
        JS_ASSERT(args[0] != (LIns *)0xcdcdcdcd);
#endif

        LIns* res_ins = lir->insCall(known->builtin, args);
        switch (known->errtype) {
          case FAIL_NULL:
            guard(false, lir->ins_eq0(res_ins), OOM_EXIT);
            break;
          case FAIL_NEG:
          {
            res_ins = lir->ins1(LIR_i2f, res_ins);
            jsdpun u;
            u.d = 0.0;
            guard(false, lir->ins2(LIR_flt, res_ins, lir->insImmq(u.u64)), OOM_EXIT);
            break;
          }
          case FAIL_VOID:
            guard(false, lir->ins2i(LIR_eq, res_ins, 2), OOM_EXIT);
            break;
          default:;
        }
        set(&fval, res_ins);
        return true;
    }

    /* Didn't find it. */
    ABORT_TRACE("unknown native");
}

bool
TraceRecorder::name(jsval*& vp)
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj)
        return activeCallOrGlobalSlot(obj, vp);

    /* Can't use prop here, because we don't want unboxing from global slots. */
    LIns* obj_ins = scopeChain();
    uint32 slot;
    if (!test_property_cache_direct_slot(obj, obj_ins, slot))
        return false;

    if (slot == SPROP_INVALID_SLOT)
        ABORT_TRACE("name op can't find named property");

    if (!lazilyImportGlobalSlot(slot))
        ABORT_TRACE("lazy import of global slot failed");

    vp = &STOBJ_GET_SLOT(obj, slot);
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
    guard(false, lir->ins2(LIR_eq, obj_ins, lir->insImmPtr((void*)globalObj)), MISMATCH_EXIT);

    /*
     * Property cache ensures that we are dealing with an existing property,
     * and guards the shape for us.
     */
    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;

    /* Check for non-existent property reference, which results in undefined. */
    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    if (PCVAL_IS_NULL(pcval)) {
        v_ins = lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID));
        JS_ASSERT(cs.ndefs == 1);
        stack(-cs.nuses, v_ins);
        return true;
    }

    /* Insist if setting on obj being the directly addressed object. */
    uint32 setflags = (cs.format & (JOF_SET | JOF_INCDEC));
    LIns* dslots_ins = NULL;
    if (obj2 != obj) {
        if (setflags)
            ABORT_TRACE("JOF_SET opcode hit prototype chain");

        /*
         * We're getting a proto-property. Walk up the prototype chain emitting
         * proto slot loads, updating obj as we go, leaving obj set to obj2 with
         * obj_ins the last proto-load.
         */
        while (obj != obj2) {
            obj_ins = stobj_get_slot(obj_ins, JSSLOT_PROTO, dslots_ins);
            obj = STOBJ_GET_PROTO(obj);
        }
    }

    /* Don't trace getter or setter calls, our caller wants a direct slot. */
    if (PCVAL_IS_SPROP(pcval)) {
        JSScopeProperty* sprop = PCVAL_TO_SPROP(pcval);

        if (setflags && !SPROP_HAS_STUB_SETTER(sprop))
            ABORT_TRACE("non-stub setter");
        if (setflags != JOF_SET && !SPROP_HAS_STUB_GETTER(sprop)) {
            // FIXME 450335: generalize this away from regexp built-in getters.
            if (setflags == 0 &&
                sprop->getter == js_RegExpClass.getProperty &&
                sprop->shortid < 0) {
                LIns* args[] = { INS_CONSTPTR(sprop), obj_ins, cx_ins };
                v_ins = lir->insCall(F_CallGetter, args);
                if (!unbox_jsval((sprop->shortid == REGEXP_SOURCE) ? JSVAL_STRING : JSVAL_BOOLEAN,
                                 v_ins)) {
                    ABORT_TRACE("unboxing");
                }
                JS_ASSERT(cs.ndefs == 1);
                stack(-cs.nuses, v_ins);
                return true;
            }
            ABORT_TRACE("non-stub getter");
        }
        if (!SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj)))
            ABORT_TRACE("no valid slot");
        slot = sprop->slot;
    } else {
        if (!PCVAL_IS_SLOT(pcval))
            ABORT_TRACE("PCE is not a slot");
        slot = PCVAL_TO_SLOT(pcval);
    }

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
    guard(false, lir->ins2(LIR_eq, obj_ins, lir->insImmPtr((void*)globalObj)), MISMATCH_EXIT);

    /* make sure the object is actually a dense array */
    if (!guardDenseArray(obj, obj_ins))
        return false;

    /* check that the index is within bounds */
    jsint idx = JSVAL_TO_INT(r);
    LIns* idx_ins = f2i(get(&r));

    /* we have to check that its really an integer, but this check will to go away
       once we peel the loop type down to integer for this slot */
    guard(true, lir->ins2(LIR_feq, get(&r), lir->ins1(LIR_i2f, idx_ins)), MISMATCH_EXIT);

    LIns* dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    if (!guardDenseArrayIndex(obj, idx, obj_ins, dslots_ins, idx_ins))
        return false;
    vp = &obj->dslots[idx];

    addr_ins = lir->ins2(LIR_piadd, dslots_ins,
                         lir->ins2i(LIR_pilsh, idx_ins, (sizeof(jsval) == 4) ? 2 : 3));

    /* load the value, check the type (need to check JSVAL_HOLE only for booleans) */
    v_ins = lir->insLoad(LIR_ldp, addr_ins, 0);
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

bool
TraceRecorder::record_JSOP_NAME()
{
    jsval* vp;
    if (!name(vp))
        return false;
    stack(0, get(vp));
    return true;
}

bool
TraceRecorder::record_JSOP_DOUBLE()
{
    jsval v = jsval(atoms[GET_INDEX(cx->fp->regs->pc)]);
    jsdpun u;
    u.d = *JSVAL_TO_DOUBLE(v);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_STRING()
{
    JSAtom* atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    JS_ASSERT(ATOM_IS_STRING(atom));
    stack(0, lir->insImmPtr((void*)ATOM_TO_STRING(atom)));
    return true;
}

bool
TraceRecorder::record_JSOP_ZERO()
{
    jsdpun u;
    u.d = 0.0;
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_ONE()
{
    jsdpun u;
    u.d = 1.0;
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_NULL()
{
    stack(0, lir->insImmPtr(NULL));
    return true;
}

bool
TraceRecorder::record_JSOP_THIS()
{
    LIns* this_ins;
    if (!getThis(this_ins))
        return false;
    stack(0, this_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_FALSE()
{
    stack(0, lir->insImm(0));
    return true;
}

bool
TraceRecorder::record_JSOP_TRUE()
{
    stack(0, lir->insImm(1));
    return true;
}

bool
TraceRecorder::record_JSOP_OR()
{
    return ifop();
}

bool
TraceRecorder::record_JSOP_AND()
{
    return ifop();
}

bool
TraceRecorder::record_JSOP_TABLESWITCH()
{
    return false;
}

bool
TraceRecorder::record_JSOP_LOOKUPSWITCH()
{
    jsval& v = stackval(-1);
    if (isNumber(v)) {
        jsdouble d = asNumber(v);
        jsdpun u;
        u.d = d;
        guard(true,
              addName(lir->ins2(LIR_feq, get(&v), lir->insImmq(u.u64)),
                      "guard(lookupswitch numeric)"),
              BRANCH_EXIT);
    } else if (JSVAL_IS_STRING(v)) {
        LIns* args[] = { get(&v), INS_CONSTPTR(JSVAL_TO_STRING(v)) };
        guard(true,
              addName(lir->ins_eq0(lir->ins_eq0(lir->insCall(F_EqualStrings, args))),
                      "guard(lookupswitch string)"),
              BRANCH_EXIT);
    } else if (JSVAL_IS_BOOLEAN(v)) {
        guard(true,
              addName(lir->ins2(LIR_eq, get(&v), lir->insImm(JSVAL_TO_BOOLEAN(v))),
                      "guard(lookupswitch boolean)"),
              BRANCH_EXIT);
    } else {
        ABORT_TRACE("lookupswitch on object, null, or undefined");
    }
    return true;
}

bool
TraceRecorder::record_JSOP_STRICTEQ()
{
    // FIXME: JSOP_EQ currently compares only like operand types; if it evolves
    // to handle conversions we must insist on like "types" here (care required
    // for 0 == -1, e.g.).
    return record_JSOP_EQ();
}

bool
TraceRecorder::record_JSOP_STRICTNE()
{
    // FIXME: JSOP_NE currently compares only like operand types; if it evolves
    // to handle conversions we must insist on like "types" here (care required
    // for 0 == -1, e.g.).
    return record_JSOP_NE();
}

bool
TraceRecorder::record_JSOP_CLOSURE()
{
    return false;
}

bool
TraceRecorder::record_JSOP_OBJECT()
{
    JSStackFrame* fp = cx->fp;
    JSScript* script = fp->script;
    unsigned index = atoms - script->atomMap.vector + GET_INDEX(fp->regs->pc);

    JSObject* obj;
    JS_GET_SCRIPT_OBJECT(script, index, obj);
    stack(0, lir->insImmPtr((void*) obj));
    return true;
}

bool
TraceRecorder::record_JSOP_POP()
{
    return true;
}

bool
TraceRecorder::record_JSOP_POS()
{
    jsval& r = stackval(-1);
    return isNumber(r);
}

bool
TraceRecorder::record_JSOP_TRAP()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GETARG()
{
    stack(0, arg(GET_ARGNO(cx->fp->regs->pc)));
    return true;
}

bool
TraceRecorder::record_JSOP_SETARG()
{
    arg(GET_ARGNO(cx->fp->regs->pc), stack(-1));
    return true;
}

bool
TraceRecorder::record_JSOP_GETLOCAL()
{
    stack(0, var(GET_SLOTNO(cx->fp->regs->pc)));
    return true;
}

bool
TraceRecorder::record_JSOP_SETLOCAL()
{
    var(GET_SLOTNO(cx->fp->regs->pc), stack(-1));
    return true;
}

bool
TraceRecorder::record_JSOP_UINT16()
{
    jsdpun u;
    u.d = (jsdouble)GET_UINT16(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_NEWINIT()
{
    JSObject* ctor;
    JSProtoKey key = JSProtoKey(GET_INT8(cx->fp->regs->pc));
    if (!js_GetClassObject(cx, globalObj, key, &ctor))
        return false;
    LIns* args[] = { lir->insImmPtr((void*) ctor), cx_ins };
    LIns* v_ins = lir->insCall(F_FastNewObject, args);
    guard(false, lir->ins_eq0(v_ins), OOM_EXIT);
    stack(0, v_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_ENDINIT()
{
    return true;
}

bool
TraceRecorder::record_JSOP_INITPROP()
{
    // The common code avoids stacking the RHS if op is not JSOP_SETPROP.
    return record_JSOP_SETPROP();
}

bool
TraceRecorder::record_JSOP_INITELEM()
{
    return record_JSOP_SETELEM();
}

bool
TraceRecorder::record_JSOP_DEFSHARP()
{
    return false;
}

bool
TraceRecorder::record_JSOP_USESHARP()
{
    return false;
}

bool
TraceRecorder::record_JSOP_INCARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1);
}

bool
TraceRecorder::record_JSOP_INCLOCAL()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), 1);
}

bool
TraceRecorder::record_JSOP_DECARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1);
}

bool
TraceRecorder::record_JSOP_DECLOCAL()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), -1);
}

bool
TraceRecorder::record_JSOP_ARGINC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1, false);
}

bool
TraceRecorder::record_JSOP_LOCALINC()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), 1, false);
}

bool
TraceRecorder::record_JSOP_ARGDEC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1, false);
}

bool
TraceRecorder::record_JSOP_LOCALDEC()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), -1, false);
}

bool
TraceRecorder::record_JSOP_ITER()
{
    jsval& v = stackval(-1);
    if (!JSVAL_IS_PRIMITIVE(v)) {
        jsuint flags = cx->fp->regs->pc[1];
        LIns* args[] = { get(&v), INS_CONST(flags), cx_ins };
        LIns* v_ins = lir->insCall(F_FastValueToIterator, args);
        guard(false, lir->ins_eq0(v_ins), MISMATCH_EXIT);
        set(&v, v_ins);
        return true;
    }

    ABORT_TRACE("for-in on a primitive value");
}

bool
TraceRecorder::forInLoop(jsval* vp)
{
    jsval& iterobj_val = stackval(-1);
    if (!JSVAL_IS_PRIMITIVE(iterobj_val)) {
        LIns* args[] = { get(&iterobj_val), cx_ins };
        LIns* v_ins = lir->insCall(F_FastCallIteratorNext, args);
        guard(false, lir->ins2(LIR_eq, v_ins, INS_CONSTPTR(JSVAL_ERROR_COOKIE)), OOM_EXIT);

        LIns* flag_ins = lir->ins_eq0(lir->ins2(LIR_eq, v_ins, INS_CONSTPTR(JSVAL_HOLE)));
        LIns* iter_ins = get(vp);
        if (!box_jsval(JSVAL_STRING, iter_ins))
            return false;
        iter_ins = lir->ins_choose(flag_ins, v_ins, iter_ins, true);
        if (!unbox_jsval(JSVAL_STRING, iter_ins))
            return false;
        set(vp, iter_ins);
        stack(0, flag_ins);
        return true;
    }

    ABORT_TRACE("for-in on a primitive value");
}

bool
TraceRecorder::record_JSOP_ENDITER()
{
    LIns* args[] = { stack(-1), cx_ins };
    LIns* ok_ins = lir->insCall(F_CloseIterator, args);
    guard(false, lir->ins_eq0(ok_ins), MISMATCH_EXIT);
    return true;
}

bool
TraceRecorder::record_JSOP_FORNAME()
{
    jsval* vp;
    return name(vp) && forInLoop(vp);
}

bool
TraceRecorder::record_JSOP_FORPROP()
{
    return false;
}

bool
TraceRecorder::record_JSOP_FORELEM()
{
    return false;
}

bool
TraceRecorder::record_JSOP_FORARG()
{
    return forInLoop(&argval(GET_ARGNO(cx->fp->regs->pc)));
}

bool
TraceRecorder::record_JSOP_FORLOCAL()
{
    return forInLoop(&varval(GET_SLOTNO(cx->fp->regs->pc)));
}

bool
TraceRecorder::record_JSOP_FORCONST()
{
    return false;
}

bool
TraceRecorder::record_JSOP_POPN()
{
    return true;
}

bool
TraceRecorder::record_JSOP_BINDNAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj)
        ABORT_TRACE("JSOP_BINDNAME crosses global scopes");

    LIns* obj_ins = scopeChain();
    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;
    JS_ASSERT(obj2 == obj);

    stack(0, obj_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_SETNAME()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(l));

    /*
     * Trace cases that are global code or in lightweight functions scoped by
     * the global object only.
     */
    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj != cx->fp->scopeChain || obj != globalObj)
        return false;

    jsval* vp;
    if (!name(vp))
        return false;
    LIns* r_ins = get(&r);
    set(vp, r_ins);

    if (cx->fp->regs->pc[JSOP_SETNAME_LENGTH] != JSOP_POP)
        stack(-2, r_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_THROW()
{
    return false;
}

bool
TraceRecorder::record_JSOP_IN()
{
    return false;
}

bool
TraceRecorder::record_JSOP_INSTANCEOF()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DEBUGGER()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GOSUB()
{
    return false;
}

bool
TraceRecorder::record_JSOP_RETSUB()
{
    return false;
}

bool
TraceRecorder::record_JSOP_EXCEPTION()
{
    return false;
}

bool
TraceRecorder::record_JSOP_LINENO()
{
    return true;
}

bool
TraceRecorder::record_JSOP_CONDSWITCH()
{
    return true;
}

bool
TraceRecorder::record_JSOP_CASE()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DEFAULT()
{
    return false;
}

bool
TraceRecorder::record_JSOP_EVAL()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ENUMELEM()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GETTER()
{
    return false;
}

bool
TraceRecorder::record_JSOP_SETTER()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DEFFUN()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DEFCONST()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DEFVAR()
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

bool
TraceRecorder::record_JSOP_ANONFUNOBJ()
{
    JSFunction* fun;
    JSFrameRegs& regs = *cx->fp->regs;
    JSScript* script = cx->fp->script;
    LOAD_FUNCTION(0); // needs script, regs, fun

    JSObject* obj = FUN_OBJECT(fun);
    if (OBJ_GET_PARENT(cx, obj) != cx->fp->scopeChain)
        ABORT_TRACE("can't trace with activation object on scopeChain");

    stack(0, lir->insImmPtr(obj));
    return true;
}

bool
TraceRecorder::record_JSOP_NAMEDFUNOBJ()
{
    return false;
}

bool
TraceRecorder::record_JSOP_SETLOCALPOP()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GROUP()
{
    return true; // no-op
}

bool
TraceRecorder::record_JSOP_SETCALL()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TRY()
{
    return true;
}

bool
TraceRecorder::record_JSOP_FINALLY()
{
    return true;
}

bool
TraceRecorder::record_JSOP_NOP()
{
    return true;
}

bool
TraceRecorder::record_JSOP_ARGSUB()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ARGCNT()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DEFLOCALFUN()
{
    JSFunction* fun;
    JSFrameRegs& regs = *cx->fp->regs;
    JSScript* script = cx->fp->script;
    LOAD_FUNCTION(SLOTNO_LEN); // needs script, regs, fun

    var(GET_SLOTNO(regs.pc), INS_CONSTPTR(FUN_OBJECT(fun)));
    return true;
}

bool
TraceRecorder::record_JSOP_GOTOX()
{
    return false;
}

bool
TraceRecorder::record_JSOP_IFEQX()
{
    return record_JSOP_IFEQ();
}

bool
TraceRecorder::record_JSOP_IFNEX()
{
    return record_JSOP_IFNE();
}

bool
TraceRecorder::record_JSOP_ORX()
{
    return record_JSOP_OR();
}

bool
TraceRecorder::record_JSOP_ANDX()
{
    return record_JSOP_AND();
}

bool
TraceRecorder::record_JSOP_GOSUBX()
{
    return record_JSOP_GOSUB();
}

bool
TraceRecorder::record_JSOP_CASEX()
{
    return record_JSOP_CASE();
}

bool
TraceRecorder::record_JSOP_DEFAULTX()
{
    return record_JSOP_DEFAULT();
}

bool
TraceRecorder::record_JSOP_TABLESWITCHX()
{
    return record_JSOP_TABLESWITCH();
}

bool
TraceRecorder::record_JSOP_LOOKUPSWITCHX()
{
    return record_JSOP_LOOKUPSWITCH();
}

bool
TraceRecorder::record_JSOP_BACKPATCH()
{
    return true;
}

bool
TraceRecorder::record_JSOP_BACKPATCH_POP()
{
    return true;
}

bool
TraceRecorder::record_JSOP_THROWING()
{
    return false;
}

bool
TraceRecorder::record_JSOP_SETRVAL()
{
    // If we implement this, we need to update JSOP_STOP.
    return false;
}

bool
TraceRecorder::record_JSOP_RETRVAL()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GETGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    stack(0, get(&STOBJ_GET_SLOT(cx->fp->scopeChain, slot)));
    return true;
}

bool
TraceRecorder::record_JSOP_SETGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    set(&STOBJ_GET_SLOT(cx->fp->scopeChain, slot), stack(-1));
    return true;
}

bool
TraceRecorder::record_JSOP_INCGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), 1);
}

bool
TraceRecorder::record_JSOP_DECGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), -1);
}

bool
TraceRecorder::record_JSOP_GVARINC()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), 1, false);
}

bool
TraceRecorder::record_JSOP_GVARDEC()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(cx->fp->scopeChain, slot), -1, false);
}

bool
TraceRecorder::record_JSOP_REGEXP()
{
    return false;
}

// begin JS_HAS_XML_SUPPORT

bool
TraceRecorder::record_JSOP_DEFXMLNS()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ANYNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_QNAMEPART()
{
    return false;
}

bool
TraceRecorder::record_JSOP_QNAMECONST()
{
    return false;
}

bool
TraceRecorder::record_JSOP_QNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TOATTRNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TOATTRVAL()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ADDATTRNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ADDATTRVAL()
{
    return false;
}

bool
TraceRecorder::record_JSOP_BINDXMLNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_SETXMLNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_DESCENDANTS()
{
    return false;
}

bool
TraceRecorder::record_JSOP_FILTER()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ENDFILTER()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TOXML()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TOXMLLIST()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLTAGEXPR()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLELTEXPR()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLOBJECT()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLCDATA()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLCOMMENT()
{
    return false;
}

bool
TraceRecorder::record_JSOP_XMLPI()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GETFUNNS()
{
    return false;
}

bool
TraceRecorder::record_JSOP_STARTXML()
{
    return false;
}

bool
TraceRecorder::record_JSOP_STARTXMLEXPR()
{
    return false;
}

// end JS_HAS_XML_SUPPORT

bool
TraceRecorder::record_JSOP_CALLPROP()
{
    jsval& l = stackval(-1);
    JSObject* obj;
    LIns* obj_ins;
    if (!JSVAL_IS_PRIMITIVE(l)) {
        obj = JSVAL_TO_OBJECT(l);
        obj_ins = get(&l);
        stack(0, obj_ins); // |this| for subsequent call
    } else {
        jsint i;
        debug_only(const char* protoname = NULL;)
        if (JSVAL_IS_STRING(l)) {
            i = JSProto_String;
            debug_only(protoname = "String.prototype";)
        } else if (JSVAL_IS_NUMBER(l)) {
            i = JSProto_Number;
            debug_only(protoname = "Number.prototype";)
        } else if (JSVAL_IS_BOOLEAN(l)) {
            i = JSProto_Boolean;
            debug_only(protoname = "Boolean.prototype";)
        } else {
            JS_ASSERT(JSVAL_IS_NULL(l) || JSVAL_IS_VOID(l));
            ABORT_TRACE("callprop on null or void");
        }

        if (!js_GetClassPrototype(cx, NULL, INT_TO_JSID(i), &obj))
            ABORT_TRACE("GetClassPrototype failed!");

        obj_ins = lir->insImmPtr((void*)obj);
        debug_only(obj_ins = addName(obj_ins, protoname);)
        stack(0, get(&l)); // use primitive as |this|
    }

    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;

    if (PCVAL_IS_NULL(pcval) || !PCVAL_IS_OBJECT(pcval))
        ABORT_TRACE("callee is not an object");
    JS_ASSERT(HAS_FUNCTION_CLASS(PCVAL_TO_OBJECT(pcval)));

    stack(-1, lir->insImmPtr(PCVAL_TO_OBJECT(pcval)));
    return true;
}

bool
TraceRecorder::record_JSOP_DELDESC()
{
    return false;
}

bool
TraceRecorder::record_JSOP_UINT24()
{
    jsdpun u;
    u.d = (jsdouble)GET_UINT24(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_INDEXBASE()
{
    atoms += GET_INDEXBASE(cx->fp->regs->pc);
    return true;
}

bool
TraceRecorder::record_JSOP_RESETBASE()
{
    atoms = cx->fp->script->atomMap.vector;
    return true;
}

bool
TraceRecorder::record_JSOP_RESETBASE0()
{
    atoms = cx->fp->script->atomMap.vector;
    return true;
}

bool
TraceRecorder::record_JSOP_CALLELEM()
{
    return false;
}

bool
TraceRecorder::record_JSOP_STOP()
{
    /*
     * We know falling off the end of a constructor returns the new object that
     * was passed in via fp->argv[-1], while falling off the end of a function
     * returns undefined.
     *
     * NB: we do not support script rval (eval, API users who want the result
     * of the last expression-statement, debugger API calls).
     */
    JSStackFrame *fp = cx->fp;
    if (fp->flags & JSFRAME_CONSTRUCTING) {
        JS_ASSERT(OBJECT_TO_JSVAL(fp->thisp) == fp->argv[-1]);
        rval_ins = get(&fp->argv[-1]);
    } else {
        rval_ins = lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_VOID));
    }
    clearFrameSlotsFromCache();
    return true;
}

bool
TraceRecorder::record_JSOP_GETXPROP()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("primitive-this for GETXPROP?");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj != cx->fp->scopeChain || obj != globalObj)
        return false;

    jsval* vp;
    if (!name(vp))
        return false;
    stack(-1, get(vp));
    return true;
}

bool
TraceRecorder::record_JSOP_CALLXMLNAME()
{
    return false;
}

bool
TraceRecorder::record_JSOP_TYPEOFEXPR()
{
    return record_JSOP_TYPEOF();
}

bool
TraceRecorder::record_JSOP_ENTERBLOCK()
{
    return false;
}

bool
TraceRecorder::record_JSOP_LEAVEBLOCK()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GENERATOR()
{
    return false;
}

bool
TraceRecorder::record_JSOP_YIELD()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ARRAYPUSH()
{
    return false;
}

bool
TraceRecorder::record_JSOP_ENUMCONSTELEM()
{
    return false;
}

bool
TraceRecorder::record_JSOP_LEAVEBLOCKEXPR()
{
    return false;
}

bool
TraceRecorder::record_JSOP_GETTHISPROP()
{
    LIns* this_ins;

    /* its safe to just use cx->fp->thisp here because getThis() returns false if thisp
       is not available */
    return getThis(this_ins) && getProp(cx->fp->thisp, this_ins);
}

bool
TraceRecorder::record_JSOP_GETARGPROP()
{
    return getProp(argval(GET_ARGNO(cx->fp->regs->pc)));
}

bool
TraceRecorder::record_JSOP_GETLOCALPROP()
{
    return getProp(varval(GET_SLOTNO(cx->fp->regs->pc)));
}

bool
TraceRecorder::record_JSOP_INDEXBASE1()
{
    atoms += 1 << 16;
    return true;
}

bool
TraceRecorder::record_JSOP_INDEXBASE2()
{
    atoms += 2 << 16;
    return true;
}

bool
TraceRecorder::record_JSOP_INDEXBASE3()
{
    atoms += 3 << 16;
    return true;
}

bool
TraceRecorder::record_JSOP_CALLGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_CALLNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    jsval& v = STOBJ_GET_SLOT(cx->fp->scopeChain, slot);
    stack(0, get(&v));
    stack(1, lir->insImmPtr(NULL));
    return true;
}

bool
TraceRecorder::record_JSOP_CALLLOCAL()
{
    uintN slot = GET_SLOTNO(cx->fp->regs->pc);
    stack(0, var(slot));
    stack(1, lir->insImmPtr(NULL));
    return true;
}

bool
TraceRecorder::record_JSOP_CALLARG()
{
    uintN slot = GET_ARGNO(cx->fp->regs->pc);
    stack(0, arg(slot));
    stack(1, lir->insImmPtr(NULL));
    return true;
}

bool
TraceRecorder::record_JSOP_NULLTHIS()
{
    stack(0, lir->insImmPtr(NULL));
    return true;
}

bool
TraceRecorder::record_JSOP_INT8()
{
    jsdpun u;
    u.d = (jsdouble)GET_INT8(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_INT32()
{
    jsdpun u;
    u.d = (jsdouble)GET_INT32(cx->fp->regs->pc);
    stack(0, lir->insImmq(u.u64));
    return true;
}

bool
TraceRecorder::record_JSOP_LENGTH()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_STRING(l))
            ABORT_TRACE("non-string primitives unsupported");
        LIns* str_ins = get(&l);
        LIns* len_ins = lir->insLoad(LIR_ldp, str_ins, (int)offsetof(JSString, length));

        LIns* masked_len_ins = lir->ins2(LIR_piand,
                                         len_ins,
                                         INS_CONSTPTR(JSSTRING_LENGTH_MASK));

        LIns *choose_len_ins =
            lir->ins_choose(lir->ins_eq0(lir->ins2(LIR_piand,
                                                   len_ins,
                                                   INS_CONSTPTR(JSSTRFLAG_DEPENDENT))),
                            masked_len_ins,
                            lir->ins_choose(lir->ins_eq0(lir->ins2(LIR_piand,
                                                                   len_ins,
                                                                   INS_CONSTPTR(JSSTRFLAG_PREFIX))),
                                            lir->ins2(LIR_piand,
                                                      len_ins,
                                                      INS_CONSTPTR(JSSTRDEP_LENGTH_MASK)),
                                            masked_len_ins,
                                            true),
                            true);

        set(&l, lir->ins1(LIR_i2f, choose_len_ins));
        return true;
    }

    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (!OBJ_IS_DENSE_ARRAY(cx, obj))
        ABORT_TRACE("only dense arrays supported");
    if (!guardDenseArray(obj, get(&l)))
        ABORT_TRACE("OBJ_IS_DENSE_ARRAY but not?!?");
    LIns* v_ins = lir->ins1(LIR_i2f, stobj_get_fslot(get(&l), JSSLOT_ARRAY_LENGTH));
    set(&l, v_ins);
    return true;
}

bool
TraceRecorder::record_JSOP_NEWARRAY()
{
    return false;
}

bool
TraceRecorder::record_JSOP_HOLE()
{
    stack(0, lir->insImm(JSVAL_TO_BOOLEAN(JSVAL_HOLE)));
    return true;
}

#define UNUSED(op) bool TraceRecorder::record_##op() { return false; }

UNUSED(JSOP_UNUSED76)
UNUSED(JSOP_UNUSED77)
UNUSED(JSOP_UNUSED78)
UNUSED(JSOP_UNUSED79)
UNUSED(JSOP_UNUSED201)
UNUSED(JSOP_UNUSED202)
UNUSED(JSOP_UNUSED203)
UNUSED(JSOP_UNUSED204)
UNUSED(JSOP_UNUSED205)
UNUSED(JSOP_UNUSED206)
UNUSED(JSOP_UNUSED207)
UNUSED(JSOP_UNUSED219)
UNUSED(JSOP_UNUSED226)
