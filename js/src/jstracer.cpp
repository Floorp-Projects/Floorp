/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "jsbit.h"              // low-level (NSPR-based) headers next
#include "jsprf.h"
#include <math.h>               // standard headers next

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#ifdef _MSC_VER
#define alloca _alloca
#endif
#endif
#ifdef SOLARIS
#include <alloca.h>
#endif
#include <limits.h>

#include "nanojit/nanojit.h"
#include "jsapi.h"              // higher-level library and API headers
#include "jsarray.h"
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
#include "jsdate.h"
#include "jsstaticcheck.h"
#include "jstracer.h"

#include "jsautooplen.h"        // generated headers last
#include "imacros.c.out"

/* Never use JSVAL_IS_BOOLEAN because it restricts the value (true, false) and
   the type. What you want to use is JSVAL_TAG(x) == JSVAL_BOOLEAN and then
   handle the undefined case properly (bug 457363). */
#undef JSVAL_IS_BOOLEAN
#define JSVAL_IS_BOOLEAN(x) JS_STATIC_ASSERT(0)

/* Use a fake tag to represent boxed values, borrowing from the integer tag
   range since we only use JSVAL_INT to indicate integers. */
#define JSVAL_BOXED 3

/* Another fake jsval tag, used to distinguish null from object values. */
#define JSVAL_TNULL 5

/* Map to translate a type tag into a printable representation. */
static const char typeChar[] = "OIDVSNB?";
static const char tagChar[]  = "OIDISIBI";

/* Blacklist parameters. */

/* Number of iterations of a loop where we start tracing.  That is, we don't
   start tracing until the beginning of the HOTLOOP-th iteration. */
#define HOTLOOP 2

/* Attempt recording this many times before blacklisting permanently. */
#define BL_ATTEMPTS 2

/* Skip this many future hits before allowing recording again after blacklisting. */
#define BL_BACKOFF 32

/* Number of times we wait to exit on a side exit before we try to extend the tree. */
#define HOTEXIT 1

/* Number of times we try to extend the tree along a side exit. */
#define MAXEXIT 3

/* Maximum number of peer trees allowed. */
#define MAXPEERS 9

/* Max call depths for inlining. */
#define MAX_CALLDEPTH 10

/* Max native stack size. */
#define MAX_NATIVE_STACK_SLOTS 1024

/* Max call stack size. */
#define MAX_CALL_STACK_ENTRIES 64

/* Max memory you can allocate in a LIR buffer via a single skip() call. */
#define MAX_SKIP_BYTES (NJ_PAGE_SIZE - LIR_FAR_SLOTS)

/* Max memory needed to rebuild the interpreter stack when falling off trace. */
#define MAX_INTERP_STACK_BYTES                                                \
    (MAX_NATIVE_STACK_SLOTS * sizeof(jsval) +                                 \
     MAX_CALL_STACK_ENTRIES * sizeof(JSInlineFrame))

/* Max number of branches per tree. */
#define MAX_BRANCHES 32

#ifdef JS_JIT_SPEW
#define debug_only_a(x) if (js_verboseAbort || js_verboseDebug ) { x; }
#define ABORT_TRACE(msg)   do { debug_only_a(fprintf(stdout, "abort: %d: %s\n", __LINE__, msg);)  return false; } while (0)
#else
#define debug_only_a(x)
#define ABORT_TRACE(msg)   return false
#endif

#ifdef JS_JIT_SPEW
struct __jitstats {
#define JITSTAT(x) uint64 x;
#include "jitstats.tbl"
#undef JITSTAT
} jitstats = { 0LL, };

JS_STATIC_ASSERT(sizeof(jitstats) % sizeof(uint64) == 0);

enum jitstat_ids {
#define JITSTAT(x) STAT ## x ## ID,
#include "jitstats.tbl"
#undef JITSTAT
    STAT_IDS_TOTAL
};

static JSPropertySpec jitstats_props[] = {
#define JITSTAT(x) { #x, STAT ## x ## ID, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT },
#include "jitstats.tbl"
#undef JITSTAT
    { 0 }
};

static JSBool
jitstats_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    int index = -1;

    if (JSVAL_IS_STRING(id)) {
        JSString* str = JSVAL_TO_STRING(id);
        if (strcmp(JS_GetStringBytes(str), "HOTLOOP") == 0) {
            *vp = INT_TO_JSVAL(HOTLOOP);
            return JS_TRUE;
        }
    }

    if (JSVAL_IS_INT(id))
        index = JSVAL_TO_INT(id);

    uint64 result = 0;
    switch (index) {
#define JITSTAT(x) case STAT ## x ## ID: result = jitstats.x; break;
#include "jitstats.tbl"
#undef JITSTAT
      default:
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    if (result < JSVAL_INT_MAX) {
        *vp = INT_TO_JSVAL(result);
        return JS_TRUE;
    }
    char retstr[64];
    JS_snprintf(retstr, sizeof retstr, "%llu", result);
    *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, retstr));
    return JS_TRUE;
}

JSClass jitstats_class = {
    "jitstats",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,       JS_PropertyStub,
    jitstats_getProperty,  JS_PropertyStub,
    JS_EnumerateStub,      JS_ResolveStub,
    JS_ConvertStub,        JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

void
js_InitJITStatsClass(JSContext *cx, JSObject *glob)
{
    JS_InitClass(cx, glob, NULL, &jitstats_class, NULL, 0, jitstats_props, NULL, NULL, NULL);
}

#define AUDIT(x) (jitstats.x++)
#else
#define AUDIT(x) ((void)0)
#endif /* JS_JIT_SPEW */

#define INS_CONST(c)        addName(lir->insImm(c), #c)
#define INS_CONSTPTR(p)     addName(lir->insImmPtr(p), #p)
#define INS_CONSTFUNPTR(p)  addName(lir->insImmPtr(JS_FUNC_TO_DATA_PTR(void*, p)), #p)

using namespace avmplus;
using namespace nanojit;

static GC gc = GC();
static avmplus::AvmCore s_core = avmplus::AvmCore();
static avmplus::AvmCore* core = &s_core;

#ifdef JS_JIT_SPEW
void
js_DumpPeerStability(JSTraceMonitor* tm, const void* ip, uint32 globalShape);
#endif

/* We really need a better way to configure the JIT. Shaver, where is my fancy JIT object? */
static bool nesting_enabled = true;
static bool did_we_check_processor_features = false;

#ifdef JS_JIT_SPEW
bool js_verboseDebug = getenv("TRACEMONKEY") && strstr(getenv("TRACEMONKEY"), "verbose");
bool js_verboseStats = getenv("TRACEMONKEY") && strstr(getenv("TRACEMONKEY"), "stats");
bool js_verboseAbort = getenv("TRACEMONKEY") && strstr(getenv("TRACEMONKEY"), "abort");
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
#define PAGEMASK 0x7ff
#else
#define PAGEMASK 0xfff
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

static inline jsint asInt32(jsval v)
{
    JS_ASSERT(isNumber(v));
    if (JSVAL_IS_INT(v))
        return JSVAL_TO_INT(v);
#ifdef DEBUG
    jsint i;
    JS_ASSERT(JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(v), i));
#endif
    return jsint(*JSVAL_TO_DOUBLE(v));
}

/* Return JSVAL_DOUBLE for all numbers (int and double) and the tag otherwise. */
static inline uint8 getPromotedType(jsval v)
{
    return JSVAL_IS_INT(v) ? JSVAL_DOUBLE : JSVAL_IS_NULL(v) ? JSVAL_TNULL : uint8(JSVAL_TAG(v));
}

/* Return JSVAL_INT for all whole numbers that fit into signed 32-bit and the tag otherwise. */
static inline uint8 getCoercedType(jsval v)
{
    return isInt32(v) ? JSVAL_INT : JSVAL_IS_NULL(v) ? JSVAL_TNULL : uint8(JSVAL_TAG(v));
}

/*
 * Constant seed and accumulate step borrowed from the DJB hash.
 */

#define ORACLE_MASK (ORACLE_SIZE - 1)
#define FRAGMENT_TABLE_MASK (FRAGMENT_TABLE_SIZE - 1)
#define HASH_SEED 5381

static inline void
hash_accum(uintptr_t& h, uintptr_t i, uintptr_t mask)
{
    h = ((h << 5) + h + (mask & i)) & mask;
}

JS_REQUIRES_STACK static inline int
stackSlotHash(JSContext* cx, unsigned slot)
{
    uintptr_t h = HASH_SEED;
    hash_accum(h, uintptr_t(cx->fp->script), ORACLE_MASK);
    hash_accum(h, uintptr_t(cx->fp->regs->pc), ORACLE_MASK);
    hash_accum(h, uintptr_t(slot), ORACLE_MASK);
    return int(h);
}

JS_REQUIRES_STACK static inline int
globalSlotHash(JSContext* cx, unsigned slot)
{
    uintptr_t h = HASH_SEED;
    JSStackFrame* fp = cx->fp;

    while (fp->down)
        fp = fp->down;

    hash_accum(h, uintptr_t(fp->script), ORACLE_MASK);
    hash_accum(h, uintptr_t(OBJ_SHAPE(JS_GetGlobalForObject(cx, fp->scopeChain))),
               ORACLE_MASK);
    hash_accum(h, uintptr_t(slot), ORACLE_MASK);
    return int(h);
}

Oracle::Oracle()
{
    clear();
}

/* Tell the oracle that a certain global variable should not be demoted. */
JS_REQUIRES_STACK void
Oracle::markGlobalSlotUndemotable(JSContext* cx, unsigned slot)
{
    _globalDontDemote.set(&gc, globalSlotHash(cx, slot));
}

/* Consult with the oracle whether we shouldn't demote a certain global variable. */
JS_REQUIRES_STACK bool
Oracle::isGlobalSlotUndemotable(JSContext* cx, unsigned slot) const
{
    return _globalDontDemote.get(globalSlotHash(cx, slot));
}

/* Tell the oracle that a certain slot at a certain bytecode location should not be demoted. */
JS_REQUIRES_STACK void
Oracle::markStackSlotUndemotable(JSContext* cx, unsigned slot)
{
    _stackDontDemote.set(&gc, stackSlotHash(cx, slot));
}

/* Consult with the oracle whether we shouldn't demote a certain slot. */
JS_REQUIRES_STACK bool
Oracle::isStackSlotUndemotable(JSContext* cx, unsigned slot) const
{
    return _stackDontDemote.get(stackSlotHash(cx, slot));
}

void
Oracle::clearDemotability()
{
    _stackDontDemote.reset();
    _globalDontDemote.reset();
}

static void
js_Blacklist(Fragment* tree)
{
    JS_ASSERT(tree->root == tree);
    jsbytecode* pc = (jsbytecode*)tree->ip;
    JS_ASSERT(*pc == JSOP_LOOP || *pc == JSOP_NOP);
    *pc = JSOP_NOP;
}

static void
js_Backoff(Fragment* tree, const jsbytecode* where)
{
    JS_ASSERT(tree->root == tree);
    if (++tree->recordAttempts > BL_ATTEMPTS) {
        js_Blacklist(tree);
        return;
    }
    tree->hits() -= BL_BACKOFF;
}

static inline size_t
fragmentHash(const void *ip, uint32 globalShape)
{
    uintptr_t h = HASH_SEED;
    hash_accum(h, uintptr_t(ip), FRAGMENT_TABLE_MASK);
    hash_accum(h, uintptr_t(globalShape), FRAGMENT_TABLE_MASK);
    return size_t(h);
}

struct VMFragment : public Fragment
{
    VMFragment(const void* _ip, uint32 _globalShape) :
        Fragment(_ip),
        next(NULL),
        globalShape(_globalShape)
    {}
    VMFragment* next;
    uint32 globalShape;
};


static VMFragment*
getVMFragment(JSTraceMonitor* tm, const void *ip, uint32 globalShape)
{
    size_t h = fragmentHash(ip, globalShape);
    VMFragment* vf = tm->vmfragments[h];
    while (vf &&
           ! (vf->globalShape == globalShape &&
              vf->ip == ip)) {
        vf = vf->next;
    }
    return vf;
}

static Fragment*
getLoop(JSTraceMonitor* tm, const void *ip, uint32 globalShape)
{
    return getVMFragment(tm, ip, globalShape);
}

static Fragment*
getAnchor(JSTraceMonitor* tm, const void *ip, uint32 globalShape)
{
    VMFragment *f = new (&gc) VMFragment(ip, globalShape);
    JS_ASSERT(f);

    Fragment *p = getVMFragment(tm, ip, globalShape);

    if (p) {
        f->first = p;
        /* append at the end of the peer list */
        Fragment* next;
        while ((next = p->peer) != NULL)
            p = next;
        p->peer = f;
    } else {
        /* this is the first fragment */
        f->first = f;
        size_t h = fragmentHash(ip, globalShape);
        f->next = tm->vmfragments[h];
        tm->vmfragments[h] = f;
    }
    f->anchor = f;
    f->root = f;
    f->kind = LoopTrace;
    return f;
}

static void
js_AttemptCompilation(JSTraceMonitor* tm, JSObject* globalObj, jsbytecode* pc)
{
    Fragment* f = getLoop(tm, pc, OBJ_SHAPE(globalObj));
    JS_ASSERT(f->root == f);
    /*
     * Breath new live into all peer fragments at the designated loop header. If we already
     * permanently blacklisted the location, undo that.
     */
    f = f->first;
    while (f) {
        JS_ASSERT(f->root == f);
        JS_ASSERT(*(jsbytecode*)f->ip == JSOP_NOP || *(jsbytecode*)f->ip == JSOP_LOOP);
        *(jsbytecode*)f->ip = JSOP_LOOP;
        --f->recordAttempts;
        f->hits() = HOTLOOP;
        f = f->peer;
    }
}

JS_DEFINE_CALLINFO_1(static, DOUBLE,    i2f, INT32,                 1, 1)
JS_DEFINE_CALLINFO_1(static, DOUBLE,    u2f, UINT32,                1, 1)

static bool isi2f(LInsp i)
{
    if (i->isop(LIR_i2f))
        return true;

    if (nanojit::AvmCore::config.soft_float &&
        i->isop(LIR_qjoin) &&
        i->oprnd1()->isop(LIR_call) &&
        i->oprnd2()->isop(LIR_callh))
    {
        if (i->oprnd1()->callInfo() == &i2f_ci)
            return true;
    }

    return false;
}

static bool isu2f(LInsp i)
{
    if (i->isop(LIR_u2f))
        return true;

    if (nanojit::AvmCore::config.soft_float &&
        i->isop(LIR_qjoin) &&
        i->oprnd1()->isop(LIR_call) &&
        i->oprnd2()->isop(LIR_callh))
    {
        if (i->oprnd1()->callInfo() == &u2f_ci)
            return true;
    }

    return false;
}

static LInsp iu2fArg(LInsp i)
{
    if (nanojit::AvmCore::config.soft_float &&
        i->isop(LIR_qjoin))
    {
        return i->oprnd1()->arg(0);
    }

    return i->oprnd1();
}


static LIns* demote(LirWriter *out, LInsp i)
{
    if (i->isCall())
        return callArgN(i, 0);
    if (isi2f(i) || isu2f(i))
        return iu2fArg(i);
    if (i->isconst())
        return i;
    AvmAssert(i->isconstq());
    double cf = i->constvalf();
    int32_t ci = cf > 0x7fffffff ? uint32_t(cf) : int32_t(cf);
    return out->insImm(ci);
}

static bool isPromoteInt(LIns* i)
{
    if (isi2f(i) || i->isconst())
        return true;
    if (!i->isconstq())
        return false;
    jsdouble d = i->constvalf();
    return d == jsdouble(jsint(d)) && !JSDOUBLE_IS_NEGZERO(d);
}

static bool isPromoteUint(LIns* i)
{
    if (isu2f(i) || i->isconst())
        return true;
    if (!i->isconstq())
        return false;
    jsdouble d = i->constvalf();
    return d == jsdouble(jsuint(d)) && !JSDOUBLE_IS_NEGZERO(d);
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

/* soft float support */

JS_DEFINE_CALLINFO_1(static, DOUBLE,    fneg, DOUBLE,               1, 1)
JS_DEFINE_CALLINFO_2(static, INT32,     fcmpeq, DOUBLE, DOUBLE,     1, 1)
JS_DEFINE_CALLINFO_2(static, INT32,     fcmplt, DOUBLE, DOUBLE,     1, 1)
JS_DEFINE_CALLINFO_2(static, INT32,     fcmple, DOUBLE, DOUBLE,     1, 1)
JS_DEFINE_CALLINFO_2(static, INT32,     fcmpgt, DOUBLE, DOUBLE,     1, 1)
JS_DEFINE_CALLINFO_2(static, INT32,     fcmpge, DOUBLE, DOUBLE,     1, 1)
JS_DEFINE_CALLINFO_2(static, DOUBLE,    fmul, DOUBLE, DOUBLE,       1, 1)
JS_DEFINE_CALLINFO_2(static, DOUBLE,    fadd, DOUBLE, DOUBLE,       1, 1)
JS_DEFINE_CALLINFO_2(static, DOUBLE,    fdiv, DOUBLE, DOUBLE,       1, 1)
JS_DEFINE_CALLINFO_2(static, DOUBLE,    fsub, DOUBLE, DOUBLE,       1, 1)

jsdouble FASTCALL
fneg(jsdouble x)
{
    return -x;
}

jsdouble FASTCALL
i2f(int32 i)
{
    return i;
}

jsdouble FASTCALL
u2f(jsuint u)
{
    return u;
}

int32 FASTCALL
fcmpeq(jsdouble x, jsdouble y)
{
    return x==y;
}

int32 FASTCALL
fcmplt(jsdouble x, jsdouble y)
{
    return x < y;
}

int32 FASTCALL
fcmple(jsdouble x, jsdouble y)
{
    return x <= y;
}

int32 FASTCALL
fcmpgt(jsdouble x, jsdouble y)
{
    return x > y;
}

int32 FASTCALL
fcmpge(jsdouble x, jsdouble y)
{
    return x >= y;
}

jsdouble FASTCALL
fmul(jsdouble x, jsdouble y)
{
    return x * y;
}

jsdouble FASTCALL
fadd(jsdouble x, jsdouble y)
{
    return x + y;
}

jsdouble FASTCALL
fdiv(jsdouble x, jsdouble y)
{
    return x / y;
}

jsdouble FASTCALL
fsub(jsdouble x, jsdouble y)
{
    return x - y;
}

class SoftFloatFilter: public LirWriter
{
public:
    SoftFloatFilter(LirWriter* out):
        LirWriter(out)
    {
    }

    LInsp quadCall(const CallInfo *ci, LInsp args[]) {
        LInsp qlo, qhi;

        qlo = out->insCall(ci, args);
        qhi = out->ins1(LIR_callh, qlo);
        return out->qjoin(qlo, qhi);
    }

    LInsp ins1(LOpcode v, LInsp s0)
    {
        if (v == LIR_fneg)
            return quadCall(&fneg_ci, &s0);

        if (v == LIR_i2f)
            return quadCall(&i2f_ci, &s0);

        if (v == LIR_u2f)
            return quadCall(&u2f_ci, &s0);

        return out->ins1(v, s0);
    }

    LInsp ins2(LOpcode v, LInsp s0, LInsp s1)
    {
        LInsp args[2];
        LInsp bv;

        // change the numeric value and order of these LIR opcodes and die
        if (LIR_fadd <= v && v <= LIR_fdiv) {
            static const CallInfo *fmap[] = { &fadd_ci, &fsub_ci, &fmul_ci, &fdiv_ci };

            args[0] = s1;
            args[1] = s0;

            return quadCall(fmap[v - LIR_fadd], args);
        }

        if (LIR_feq <= v && v <= LIR_fge) {
            static const CallInfo *fmap[] = { &fcmpeq_ci, &fcmplt_ci, &fcmpgt_ci, &fcmple_ci, &fcmpge_ci };

            args[0] = s1;
            args[1] = s0;

            bv = out->insCall(fmap[v - LIR_feq], args);
            return out->ins2(LIR_eq, bv, out->insImm(1));
        }

        return out->ins2(v, s0, s1);
    }

    LInsp insCall(const CallInfo *ci, LInsp args[])
    {
        // if the return type is ARGSIZE_F, we have
        // to do a quadCall ( qjoin(call,callh) )
        if ((ci->_argtypes & 3) == ARGSIZE_F)
            return quadCall(ci, args);

        return out->insCall(ci, args);
    }
};

class FuncFilter: public LirWriter
{
public:
    FuncFilter(LirWriter* out):
        LirWriter(out)
    {
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
#ifdef NANOJIT_ARM
        else if (v == LIR_lsh ||
                 v == LIR_rsh ||
                 v == LIR_ush)
        {
            // needed on ARM -- arm doesn't mask shifts to 31 like x86 does
            if (s1->isconst())
                s1->setimm16(s1->constval() & 31);
            else
                s1 = out->ins2(LIR_and, s1, out->insImm(31));
            return out->ins2(v, s0, s1);
        }
#endif

        return out->ins2(v, s0, s1);
    }

    LInsp insCall(const CallInfo *ci, LInsp args[])
    {
        LInsp s0 = args[0];
        if (ci == &js_DoubleToUint32_ci) {
            if (s0->isconstq())
                return out->insImm(js_DoubleToECMAUint32(s0->constvalf()));
            if (isi2f(s0) || isu2f(s0))
                return iu2fArg(s0);
        } else if (ci == &js_DoubleToInt32_ci) {
            if (s0->isconstq())
                return out->insImm(js_DoubleToECMAInt32(s0->constvalf()));
            if (s0->isop(LIR_fadd) || s0->isop(LIR_fsub)) {
                LInsp lhs = s0->oprnd1();
                LInsp rhs = s0->oprnd2();
                if (isPromote(lhs) && isPromote(rhs)) {
                    LOpcode op = LOpcode(s0->opcode() & ~LIR64);
                    return out->ins2(op, demote(out, lhs), demote(out, rhs));
                }
            }
            if (isi2f(s0) || isu2f(s0))
                return iu2fArg(s0);
            // XXX ARM -- check for qjoin(call(UnboxDouble),call(UnboxDouble))
            if (s0->isCall()) {
                const CallInfo* ci2 = s0->callInfo();
                if (ci2 == &js_UnboxDouble_ci) {
                    LIns* args2[] = { callArgN(s0, 0) };
                    return out->insCall(&js_UnboxInt32_ci, args2);
                } else if (ci2 == &js_StringToNumber_ci) {
                    // callArgN's ordering is that as seen by the builtin, not as stored in
                    // args here. True story!
                    LIns* args2[] = { callArgN(s0, 1), callArgN(s0, 0) };
                    return out->insCall(&js_StringToInt32_ci, args2);
                } else if (ci2 == &js_String_p_charCodeAt0_ci) {
                    // Use a fast path builtin for a charCodeAt that converts to an int right away.
                    LIns* args2[] = { callArgN(s0, 0) };
                    return out->insCall(&js_String_p_charCodeAt0_int_ci, args2);
                } else if (ci2 == &js_String_p_charCodeAt_ci) {
                    LIns* idx = callArgN(s0, 1);
                    // If the index is not already an integer, force it to be an integer.
                    idx = isPromote(idx)
                        ? demote(out, idx)
                        : out->insCall(&js_DoubleToInt32_ci, &idx);
                    LIns* args2[] = { idx, callArgN(s0, 0) };
                    return out->insCall(&js_String_p_charCodeAt_int_ci, args2);
                }
            }
        } else if (ci == &js_BoxDouble_ci) {
            JS_ASSERT(s0->isQuad());
            if (isi2f(s0)) {
                LIns* args2[] = { iu2fArg(s0), args[1] };
                return out->insCall(&js_BoxInt32_ci, args2);
            }
            if (s0->isCall() && s0->callInfo() == &js_UnboxDouble_ci)
                return callArgN(s0, 0);
        }
        return out->insCall(ci, args);
    }
};

/* In debug mode vpname contains a textual description of the type of the
   slot during the forall iteration over all slots. If JS_JIT_SPEW is not
   defined, vpnum is set to a very large integer to catch invalid uses of
   it. Non-debug code should never use vpnum. */
#ifdef JS_JIT_SPEW
#define DEF_VPNAME          const char* vpname; unsigned vpnum
#define SET_VPNAME(name)    do { vpname = name; vpnum = 0; } while(0)
#define INC_VPNUM()         do { ++vpnum; } while(0)
#else
#define DEF_VPNAME          do {} while (0)
#define vpname ""
#define vpnum 0x40000000
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
                vp = &fp->argv[0]; vpstop = &fp->argv[fp->fun->nargs];        \
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
        FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth, code);                  \
        FORALL_GLOBAL_SLOTS(cx, ngslots, gslots, code);                       \
    JS_END_MACRO

/* Calculate the total number of native frame slots we need from this frame
   all the way back to the entry frame, including the current stack usage. */
JS_REQUIRES_STACK unsigned
js_NativeStackSlots(JSContext *cx, unsigned callDepth)
{
    JSStackFrame* fp = cx->fp;
    unsigned slots = 0;
#if defined _DEBUG
    unsigned int origCallDepth = callDepth;
#endif
    for (;;) {
        unsigned operands = fp->regs->sp - StackBase(fp);
        slots += operands;
        if (fp->callee)
            slots += fp->script->nfixed;
        if (callDepth-- == 0) {
            if (fp->callee)
                slots += 2/*callee,this*/ + fp->fun->nargs;
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
    JS_NOT_REACHED("js_NativeStackSlots");
}

/*
 * Capture the type map for the selected slots of the global object and currently pending
 * stack frames.
 */
JS_REQUIRES_STACK void
TypeMap::captureTypes(JSContext* cx, SlotList& slots, unsigned callDepth)
{
    unsigned ngslots = slots.length();
    uint16* gslots = slots.data();
    setLength(js_NativeStackSlots(cx, callDepth) + ngslots);
    uint8* map = data();
    uint8* m = map;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        uint8 type = getCoercedType(*vp);
        if ((type == JSVAL_INT) && oracle.isStackSlotUndemotable(cx, unsigned(m - map)))
            type = JSVAL_DOUBLE;
        JS_ASSERT(type != JSVAL_BOXED);
        debug_only_v(printf("capture stack type %s%d: %d=%c\n", vpname, vpnum, type, typeChar[type]);)
        JS_ASSERT(uintptr_t(m - map) < length());
        *m++ = type;
    );
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        uint8 type = getCoercedType(*vp);
        if ((type == JSVAL_INT) && oracle.isGlobalSlotUndemotable(cx, gslots[n]))
            type = JSVAL_DOUBLE;
        JS_ASSERT(type != JSVAL_BOXED);
        debug_only_v(printf("capture global type %s%d: %d=%c\n", vpname, vpnum, type, typeChar[type]);)
        JS_ASSERT(uintptr_t(m - map) < length());
        *m++ = type;
    );
    JS_ASSERT(uintptr_t(m - map) == length());
}

JS_REQUIRES_STACK void
TypeMap::captureMissingGlobalTypes(JSContext* cx, SlotList& slots, unsigned stackSlots)
{
    unsigned oldSlots = length() - stackSlots;
    int diff = slots.length() - oldSlots;
    JS_ASSERT(diff >= 0);
    unsigned ngslots = slots.length();
    uint16* gslots = slots.data();
    setLength(length() + diff);
    uint8* map = data() + stackSlots;
    uint8* m = map;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        if (n >= oldSlots) {
            uint8 type = getCoercedType(*vp);
            if ((type == JSVAL_INT) && oracle.isGlobalSlotUndemotable(cx, gslots[n]))
                type = JSVAL_DOUBLE;
            JS_ASSERT(type != JSVAL_BOXED);
            debug_only_v(printf("capture global type %s%d: %d=%c\n", vpname, vpnum, type, typeChar[type]);)
            *m = type;
            JS_ASSERT((m > map + oldSlots) || (*m == type));
        }
        m++;
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

/* Specializes a tree to any missing globals, including any dependent trees. */
static JS_REQUIRES_STACK void
specializeTreesToMissingGlobals(JSContext* cx, TreeInfo* root)
{
    TreeInfo* ti = root;

    ti->typeMap.captureMissingGlobalTypes(cx, *ti->globalSlots, ti->nStackTypes);
    JS_ASSERT(ti->globalSlots->length() == ti->typeMap.length() - ti->nStackTypes);

    for (unsigned i = 0; i < root->dependentTrees.length(); i++) {
        ti = (TreeInfo*)root->dependentTrees.data()[i]->vmprivate;
        /* ti can be NULL if we hit the recording tree in emitTreeCall; this is harmless. */
        if (ti && ti->nGlobalTypes() < ti->globalSlots->length())
            specializeTreesToMissingGlobals(cx, ti);
    }
    for (unsigned i = 0; i < root->linkedTrees.length(); i++) {
        ti = (TreeInfo*)root->linkedTrees.data()[i]->vmprivate;
        if (ti && ti->nGlobalTypes() < ti->globalSlots->length())
            specializeTreesToMissingGlobals(cx, ti);
    }
}

static void
js_TrashTree(JSContext* cx, Fragment* f);

JS_REQUIRES_STACK
TraceRecorder::TraceRecorder(JSContext* cx, VMSideExit* _anchor, Fragment* _fragment,
        TreeInfo* ti, unsigned stackSlots, unsigned ngslots, uint8* typeMap,
        VMSideExit* innermostNestedGuard, jsbytecode* outer)
{
    JS_ASSERT(!_fragment->vmprivate && ti);

    this->cx = cx;
    this->traceMonitor = &JS_TRACE_MONITOR(cx);
    this->globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    this->lexicalBlock = cx->fp->blockChain;
    this->anchor = _anchor;
    this->fragment = _fragment;
    this->lirbuf = _fragment->lirbuf;
    this->treeInfo = ti;
    this->callDepth = _anchor ? _anchor->calldepth : 0;
    this->atoms = FrameAtomBase(cx, cx->fp);
    this->deepAborted = false;
    this->trashSelf = false;
    this->global_dslots = this->globalObj->dslots;
    this->terminate = false;
    this->wasRootFragment = _fragment == _fragment->root;
    this->outer = outer;
    
    debug_only_v(printf("recording starting from %s:%u@%u\n",
                        ti->treeFileName, ti->treeLineNumber, ti->treePCOffset);)
    debug_only_v(printf("globalObj=%p, shape=%d\n", (void*)this->globalObj, OBJ_SHAPE(this->globalObj));)

    lir = lir_buf_writer = new (&gc) LirBufWriter(lirbuf);
    debug_only_v(lir = verbose_filter = new (&gc) VerboseWriter(&gc, lir, lirbuf->names);)
    if (nanojit::AvmCore::config.soft_float)
        lir = float_filter = new (&gc) SoftFloatFilter(lir);
    else
        float_filter = 0;
    lir = cse_filter = new (&gc) CseFilter(lir, &gc);
    lir = expr_filter = new (&gc) ExprFilter(lir);
    lir = func_filter = new (&gc) FuncFilter(lir);
    lir->ins0(LIR_start);

    if (!nanojit::AvmCore::config.tree_opt || fragment->root == fragment)
        lirbuf->state = addName(lir->insParam(0, 0), "state");

    lirbuf->sp = addName(lir->insLoad(LIR_ldp, lirbuf->state, (int)offsetof(InterpState, sp)), "sp");
    lirbuf->rp = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, rp)), "rp");
    cx_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, cx)), "cx");
    eos_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, eos)), "eos");
    eor_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, eor)), "eor");
    globalObj_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, globalObj)), "globalObj");

    /* If we came from exit, we might not have enough global types. */
    if (ti->globalSlots->length() > ti->nGlobalTypes())
        specializeTreesToMissingGlobals(cx, ti);

    /* read into registers all values on the stack and all globals we know so far */
    import(treeInfo, lirbuf->sp, stackSlots, ngslots, callDepth, typeMap);

    if (fragment == fragment->root) {
        /*
         * We poll the operation callback request flag. It is updated asynchronously whenever 
         * the callback is to be invoked.
         */
        LIns* x = lir->insLoadi(cx_ins, offsetof(JSContext, operationCallbackFlag));
        guard(true, lir->ins_eq0(x), snapshot(TIMEOUT_EXIT));
    }

    /* If we are attached to a tree call guard, make sure the guard the inner tree exited from
       is what we expect it to be. */
    if (_anchor && _anchor->exitType == NESTED_EXIT) {
        LIns* nested_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state,
                                                offsetof(InterpState, lastTreeExitGuard)),
                                                "lastTreeExitGuard");
        guard(true, lir->ins2(LIR_eq, nested_ins, INS_CONSTPTR(innermostNestedGuard)), NESTED_EXIT);
    }
}

TreeInfo::~TreeInfo()
{
    UnstableExit* temp;
    
    while (unstableExits) {
        temp = unstableExits->next;
        delete unstableExits;
        unstableExits = temp;
    }
}

TraceRecorder::~TraceRecorder()
{
    JS_ASSERT(nextRecorderToAbort == NULL);
    JS_ASSERT(treeInfo && (fragment || wasDeepAborted()));
#ifdef DEBUG
    TraceRecorder* tr = JS_TRACE_MONITOR(cx).abortStack;
    while (tr != NULL)
    {
        JS_ASSERT(this != tr);
        tr = tr->nextRecorderToAbort;
    }
#endif
    if (fragment) {
        if (wasRootFragment && !fragment->root->code()) {
            JS_ASSERT(!fragment->root->vmprivate);
            delete treeInfo;
        }

        if (trashSelf)
            js_TrashTree(cx, fragment->root);

        for (unsigned int i = 0; i < whichTreesToTrash.length(); i++)
            js_TrashTree(cx, whichTreesToTrash.get(i));
    } else if (wasRootFragment) {
        delete treeInfo;
    }
#ifdef DEBUG
    delete verbose_filter;
#endif
    delete cse_filter;
    delete expr_filter;
    delete func_filter;
    delete float_filter;
    delete lir_buf_writer;
}

void TraceRecorder::removeFragmentoReferences()
{
    fragment = NULL;
}

/* Add debug information to a LIR instruction as we emit it. */
inline LIns*
TraceRecorder::addName(LIns* ins, const char* name)
{
#ifdef JS_JIT_SPEW
    if (js_verboseDebug)
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
        return sizeof(InterpState) + size_t(p - globalObj->fslots) * sizeof(double);
    return sizeof(InterpState) + ((p - globalObj->dslots) + JS_INITIAL_NSLOTS) * sizeof(double);
}

/* Determine whether a value is a global stack slot */
bool
TraceRecorder::isGlobal(jsval* p) const
{
    return ((size_t(p - globalObj->fslots) < JS_INITIAL_NSLOTS) ||
            (size_t(p - globalObj->dslots) < (STOBJ_NSLOTS(globalObj) - JS_INITIAL_NSLOTS)));
}

/* Determine the offset in the native stack for a jsval we track */
JS_REQUIRES_STACK ptrdiff_t
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
                if (size_t(p - &fp->argv[-2]) < size_t(2/*callee,this*/ + fp->fun->nargs))
                    RETURN(offset + size_t(p - &fp->argv[-2]) * sizeof(double));
                offset += (2/*callee,this*/ + fp->fun->nargs) * sizeof(double);
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

/* Unbox a jsval into a slot. Slots are wide enough to hold double values directly (instead of
   storing a pointer to them). We now assert instead of type checking, the caller must ensure the
   types are compatible. */
static void
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
        else
            JS_ASSERT(JSVAL_IS_INT(v));
        debug_only_v(printf("int<%d> ", *(jsint*)slot);)
        return;
      case JSVAL_DOUBLE:
        jsdouble d;
        if (JSVAL_IS_INT(v))
            d = JSVAL_TO_INT(v);
        else
            d = *JSVAL_TO_DOUBLE(v);
        JS_ASSERT(JSVAL_IS_INT(v) || JSVAL_IS_DOUBLE(v));
        *(jsdouble*)slot = d;
        debug_only_v(printf("double<%g> ", d);)
        return;
      case JSVAL_BOOLEAN:
        /* Watch out for pseudo-booleans. */
        JS_ASSERT(tag == JSVAL_BOOLEAN);
        *(JSBool*)slot = JSVAL_TO_PSEUDO_BOOLEAN(v);
        debug_only_v(printf("boolean<%d> ", *(JSBool*)slot);)
        return;
      case JSVAL_STRING:
        JS_ASSERT(tag == JSVAL_STRING);
        *(JSString**)slot = JSVAL_TO_STRING(v);
        debug_only_v(printf("string<%p> ", (void*)(*(JSString**)slot));)
        return;
      case JSVAL_TNULL:
        JS_ASSERT(tag == JSVAL_OBJECT);
        *(JSObject**)slot = NULL;
        return;
      default:
        /* Note: we should never see JSVAL_BOXED in an entry type map. */
        JS_ASSERT(type == JSVAL_OBJECT);
        JS_ASSERT(tag == JSVAL_OBJECT);
        *(JSObject**)slot = JSVAL_TO_OBJECT(v);
        debug_only_v(printf("object<%p:%s> ", (void*)JSVAL_TO_OBJECT(v),
                            JSVAL_IS_NULL(v)
                            ? "null"
                            : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);)
        return;
    }
}

/* We maintain an emergency pool of doubles so we can recover safely if a trace runs
   out of memory (doubles or objects). */
static jsval
AllocateDoubleFromReservedPool(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(tm->reservedDoublePoolPtr > tm->reservedDoublePool);
    return *--tm->reservedDoublePoolPtr;
}

static bool
js_ReplenishReservedPool(JSContext* cx, JSTraceMonitor* tm)
{
    /* We should not be called with a full pool. */
    JS_ASSERT((size_t) (tm->reservedDoublePoolPtr - tm->reservedDoublePool) <
              MAX_NATIVE_STACK_SLOTS);

    /*
     * When the GC runs in js_NewDoubleInRootedValue, it resets
     * tm->reservedDoublePoolPtr back to tm->reservedDoublePool.
     */
    JSRuntime* rt = cx->runtime;
    uintN gcNumber = rt->gcNumber;
    uintN lastgcNumber = gcNumber;
    jsval* ptr = tm->reservedDoublePoolPtr;
    while (ptr < tm->reservedDoublePool + MAX_NATIVE_STACK_SLOTS) {
        if (!js_NewDoubleInRootedValue(cx, 0.0, ptr))
            goto oom;

        /* Check if the last call to js_NewDoubleInRootedValue GC'd. */
        if (rt->gcNumber != lastgcNumber) {
            lastgcNumber = rt->gcNumber;
            JS_ASSERT(tm->reservedDoublePoolPtr == tm->reservedDoublePool);
            ptr = tm->reservedDoublePool;

            /*
             * Have we GC'd more than once? We're probably running really
             * low on memory, bail now.
             */
            if (uintN(rt->gcNumber - gcNumber) > uintN(1))
                goto oom;
            continue;
        }
        ++ptr;
    }
    tm->reservedDoublePoolPtr = ptr;
    return true;

oom:
    /*
     * Already massive GC pressure, no need to hold doubles back.
     * We won't run any native code anyway.
     */
    tm->reservedDoublePoolPtr = tm->reservedDoublePool;
    return false;
}

/* Box a value from the native stack back into the jsval format. Integers
   that are too large to fit into a jsval are automatically boxed into
   heap-allocated doubles. */
static void
NativeToValue(JSContext* cx, jsval& v, uint8 type, double* slot)
{
    jsint i;
    jsdouble d;
    switch (type) {
      case JSVAL_BOOLEAN:
        /* Watch out for pseudo-booleans. */
        v = PSEUDO_BOOLEAN_TO_JSVAL(*(JSBool*)slot);
        debug_only_v(printf("boolean<%d> ", *(JSBool*)slot);)
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
      store_double: {
        /* Its not safe to trigger the GC here, so use an emergency heap if we are out of
           double boxes. */
        if (cx->doubleFreeList) {
#ifdef DEBUG
            JSBool ok =
#endif
                js_NewDoubleInRootedValue(cx, d, &v);
            JS_ASSERT(ok);
            return;
        }
        v = AllocateDoubleFromReservedPool(cx);
        JS_ASSERT(JSVAL_IS_DOUBLE(v) && *JSVAL_TO_DOUBLE(v) == 0.0);
        *JSVAL_TO_DOUBLE(v) = d;
        return;
      }
      case JSVAL_STRING:
        v = STRING_TO_JSVAL(*(JSString**)slot);
        JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING); /* if this fails the pointer was not aligned */
        debug_only_v(printf("string<%p> ", (void*)(*(JSString**)slot));)
        break;
      case JSVAL_BOXED:
        v = *(jsval*)slot;
        JS_ASSERT(v != JSVAL_ERROR_COOKIE); /* don't leak JSVAL_ERROR_COOKIE */
        debug_only_v(printf("box<%p> ", (void*)v));
        break;
      case JSVAL_TNULL:
        JS_ASSERT(*(JSObject**)slot == NULL);
        v = JSVAL_NULL;
        debug_only_v(printf("null<%p> ", (void*)(*(JSObject**)slot)));
        break;
      default:
        JS_ASSERT(type == JSVAL_OBJECT);
        v = OBJECT_TO_JSVAL(*(JSObject**)slot);
        JS_ASSERT(JSVAL_TAG(v) == JSVAL_OBJECT); /* if this fails the pointer was not aligned */
        JS_ASSERT(v != JSVAL_ERROR_COOKIE); /* don't leak JSVAL_ERROR_COOKIE */
        debug_only_v(printf("object<%p:%s> ", (void*)JSVAL_TO_OBJECT(v),
                            JSVAL_IS_NULL(v)
                            ? "null"
                            : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);)
        break;
    }
}

/* Attempt to unbox the given list of interned globals onto the native global frame. */
static JS_REQUIRES_STACK void
BuildNativeGlobalFrame(JSContext* cx, unsigned ngslots, uint16* gslots, uint8* mp, double* np)
{
    debug_only_v(printf("global: ");)
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        ValueToNative(cx, *vp, *mp, np + gslots[n]);
        ++mp;
    );
    debug_only_v(printf("\n");)
}

/* Attempt to unbox the given JS frame onto a native frame. */
static JS_REQUIRES_STACK void
BuildNativeStackFrame(JSContext* cx, unsigned callDepth, uint8* mp, double* np)
{
    debug_only_v(printf("stack: ");)
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        debug_only_v(printf("%s%u=", vpname, vpnum);)
        ValueToNative(cx, *vp, *mp, np);
        ++mp; ++np;
    );
    debug_only_v(printf("\n");)
}

/* Box the given native frame into a JS frame. This is infallible. */
static JS_REQUIRES_STACK int
FlushNativeGlobalFrame(JSContext* cx, unsigned ngslots, uint16* gslots, uint8* mp, double* np)
{
    uint8* mp_base = mp;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        debug_only_v(printf("%s%u=", vpname, vpnum);)
        NativeToValue(cx, *vp, *mp, np + gslots[n]);
        ++mp;
    );
    debug_only_v(printf("\n");)
    return mp - mp_base;
}

/**
 * Box the given native stack frame into the virtual machine stack. This
 * is infallible.
 *
 * @param callDepth the distance between the entry frame into our trace and
 *                  cx->fp when we make this call.  If this is not called as a
 *                  result of a nested exit, callDepth is 0.
 * @param mp pointer to an array of type tags (JSVAL_INT, etc.) that indicate
 *           what the types of the things on the stack are.
 * @param np pointer to the native stack.  We want to copy values from here to
 *           the JS stack as needed.
 * @param stopFrame if non-null, this frame and everything above it should not
 *                  be restored.
 * @return the number of things we popped off of np.
 */
static JS_REQUIRES_STACK int
FlushNativeStackFrame(JSContext* cx, unsigned callDepth, uint8* mp, double* np,
                      JSStackFrame* stopFrame)
{
    jsval* stopAt = stopFrame ? &stopFrame->argv[-2] : NULL;
    uint8* mp_base = mp;
    /* Root all string and object references first (we don't need to call the GC for this). */
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        if (vp == stopAt) goto skip;
        debug_only_v(printf("%s%u=", vpname, vpnum);)
        NativeToValue(cx, *vp, *mp, np);
        ++mp; ++np
    );
skip:
    // Restore thisp from the now-restored argv[-1] in each pending frame.
    // Keep in mind that we didn't restore frames at stopFrame and above!
    // Scope to keep |fp| from leaking into the macros we're using.
    {
        unsigned n = callDepth+1; // +1 to make sure we restore the entry frame
        JSStackFrame* fp = cx->fp;
        if (stopFrame) {
            for (; fp != stopFrame; fp = fp->down) {
                JS_ASSERT(n != 0);
                --n;
            }
            // Skip over stopFrame itself.
            JS_ASSERT(n != 0);
            --n;
            fp = fp->down;
        }
        for (; n != 0; fp = fp->down) {
            --n;
            if (fp->callee) { // might not have it if the entry frame is global
                JS_ASSERT(JSVAL_IS_OBJECT(fp->argv[-1]));
                fp->thisp = JSVAL_TO_OBJECT(fp->argv[-1]);
            }
        }
    }
    debug_only_v(printf("\n");)
    return mp - mp_base;
}

/* Emit load instructions onto the trace that read the initial stack state. */
JS_REQUIRES_STACK void
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
        JS_ASSERT(t == JSVAL_BOXED || isNumber(*p) == (t == JSVAL_DOUBLE));
        if (t == JSVAL_DOUBLE) {
            ins = lir->insLoad(LIR_ldq, base, offset);
        } else if (t == JSVAL_BOOLEAN) {
            ins = lir->insLoad(LIR_ld, base, offset);
        } else {
            ins = lir->insLoad(LIR_ldp, base, offset);
        }
    }
    checkForGlobalObjectReallocation();
    tracker.set(p, ins);
#ifdef DEBUG
    char name[64];
    JS_ASSERT(strlen(prefix) < 10);
    void* mark = NULL;
    jsuword* localNames = NULL;
    const char* funName = NULL;
    if (*prefix == 'a' || *prefix == 'v') {
        mark = JS_ARENA_MARK(&cx->tempPool);
        if (JS_GET_LOCAL_NAME_COUNT(fp->fun) != 0)
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
    debug_only_v(printf("import vp=%p name=%s type=%s flags=%d\n",
                        (void*)p, name, typestr[t & 7], t >> 3);)
#endif
}

JS_REQUIRES_STACK void
TraceRecorder::import(TreeInfo* treeInfo, LIns* sp, unsigned stackSlots, unsigned ngslots,
                      unsigned callDepth, uint8* typeMap)
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

    uint8* globalTypeMap = typeMap + stackSlots;
    unsigned length = treeInfo->nGlobalTypes();

    /* This is potentially the typemap of the side exit and thus shorter than the tree's
       global type map. */
    if (ngslots < length)
        mergeTypeMaps(&globalTypeMap/*out param*/, &ngslots/*out param*/,
                      treeInfo->globalTypeMap(), length,
                      (uint8*)alloca(sizeof(uint8) * length));
    JS_ASSERT(ngslots == treeInfo->nGlobalTypes());

    /* the first time we compile a tree this will be empty as we add entries lazily */
    uint16* gslots = treeInfo->globalSlots->data();
    uint8* m = globalTypeMap;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        import(lirbuf->state, nativeGlobalOffset(vp), vp, *m, vpname, vpnum, NULL);
        m++;
    );
    ptrdiff_t offset = -treeInfo->nativeStackBase;
    m = typeMap;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, callDepth,
        import(sp, offset, vp, *m, vpname, vpnum, fp);
        m++; offset += sizeof(double);
    );
}

JS_REQUIRES_STACK bool
TraceRecorder::isValidSlot(JSScope* scope, JSScopeProperty* sprop)
{
    uint32 setflags = (js_CodeSpec[*cx->fp->regs->pc].format & (JOF_SET | JOF_INCDEC | JOF_FOR));

    if (setflags) {
        if (!SPROP_HAS_STUB_SETTER(sprop))
            ABORT_TRACE("non-stub setter");
        if (sprop->attrs & JSPROP_READONLY)
            ABORT_TRACE("writing to a read-only property");
    }
    /* This check applies even when setflags == 0. */
    if (setflags != JOF_SET && !SPROP_HAS_STUB_GETTER(sprop))
        ABORT_TRACE("non-stub getter");

    if (!SPROP_HAS_VALID_SLOT(sprop, scope))
        ABORT_TRACE("slotless obj property");

    return true;
}

/* Lazily import a global slot if we don't already have it in the tracker. */
JS_REQUIRES_STACK bool
TraceRecorder::lazilyImportGlobalSlot(unsigned slot)
{
    if (slot != uint16(slot)) /* we use a table of 16-bit ints, bail out if that's not enough */
        return false;
    jsval* vp = &STOBJ_GET_SLOT(globalObj, slot);
    if (known(vp))
        return true; /* we already have it */
    unsigned index = treeInfo->globalSlots->length();
    /* Add the slot to the list of interned global slots. */
    JS_ASSERT(treeInfo->nGlobalTypes() == treeInfo->globalSlots->length());
    treeInfo->globalSlots->add(slot);
    uint8 type = getCoercedType(*vp);
    if ((type == JSVAL_INT) && oracle.isGlobalSlotUndemotable(cx, slot))
        type = JSVAL_DOUBLE;
    treeInfo->typeMap.add(type);
    import(lirbuf->state, sizeof(struct InterpState) + slot*sizeof(double),
           vp, type, "global", index, NULL);
    specializeTreesToMissingGlobals(cx, treeInfo);
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
JS_REQUIRES_STACK void
TraceRecorder::set(jsval* p, LIns* i, bool initializing)
{
    JS_ASSERT(i != NULL);
    JS_ASSERT(initializing || known(p));
    checkForGlobalObjectReallocation();
    tracker.set(p, i);
    /* If we are writing to this location for the first time, calculate the offset into the
       native frame manually, otherwise just look up the last load or store associated with
       the same source address (p) and use the same offset/base. */
    LIns* x = nativeFrameTracker.get(p);
    if (!x) {
        if (isGlobal(p))
            x = writeBack(i, lirbuf->state, nativeGlobalOffset(p));
        else
            x = writeBack(i, lirbuf->sp, -treeInfo->nativeStackBase + nativeStackOffset(p));
        nativeFrameTracker.set(p, x);
    } else {
#define ASSERT_VALID_CACHE_HIT(base, offset)                                  \
    JS_ASSERT(base == lirbuf->sp || base == lirbuf->state);                   \
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

JS_REQUIRES_STACK LIns*
TraceRecorder::get(jsval* p)
{
    checkForGlobalObjectReallocation();
    return tracker.get(p);
}

JS_REQUIRES_STACK bool
TraceRecorder::known(jsval* p)
{
    checkForGlobalObjectReallocation();
    return tracker.has(p);
}

/*
 * The dslots of the global object are sometimes reallocated by the interpreter.
 * This function check for that condition and re-maps the entries of the tracker
 * accordingly.
 */
JS_REQUIRES_STACK void
TraceRecorder::checkForGlobalObjectReallocation()
{
    if (global_dslots != globalObj->dslots) {
        debug_only_v(printf("globalObj->dslots relocated, updating tracker\n");)
        jsval* src = global_dslots;
        jsval* dst = globalObj->dslots;
        jsuint length = globalObj->dslots[-1] - JS_INITIAL_NSLOTS;
        LIns** map = (LIns**)alloca(sizeof(LIns*) * length);
        for (jsuint n = 0; n < length; ++n) {
            map[n] = tracker.get(src);
            tracker.set(src++, NULL);
        }
        for (jsuint n = 0; n < length; ++n)
            tracker.set(dst++, map[n]);
        global_dslots = globalObj->dslots;
    }
}

/* Determine whether the current branch instruction terminates the loop. */
static bool
js_IsLoopExit(jsbytecode* pc, jsbytecode* header)
{
    switch (*pc) {
      case JSOP_LT:
      case JSOP_GT:
      case JSOP_LE:
      case JSOP_GE:
      case JSOP_NE:
      case JSOP_EQ:
        /* These ops try to dispatch a JSOP_IFEQ or JSOP_IFNE that follows. */
        JS_ASSERT(js_CodeSpec[*pc].length == 1);
        pc++;
        break;

      default:
        for (;;) {
            if (*pc == JSOP_AND || *pc == JSOP_OR)
                pc += GET_JUMP_OFFSET(pc);
            else if (*pc == JSOP_ANDX || *pc == JSOP_ORX)
                pc += GET_JUMPX_OFFSET(pc);
            else
                break;
        }
    }

    switch (*pc) {
      case JSOP_IFEQ:
      case JSOP_IFNE:
        /*
         * Forward jumps are usually intra-branch, but for-in loops jump to the
         * trailing enditer to clean up, so check for that case here.
         */
        if (pc[GET_JUMP_OFFSET(pc)] == JSOP_ENDITER)
            return true;
        return pc + GET_JUMP_OFFSET(pc) == header;

      case JSOP_IFEQX:
      case JSOP_IFNEX:
        if (pc[GET_JUMPX_OFFSET(pc)] == JSOP_ENDITER)
            return true;
        return pc + GET_JUMPX_OFFSET(pc) == header;

      default:;
    }
    return false;
}

/* Determine whether the current branch is a loop edge (taken or not taken). */
static JS_REQUIRES_STACK bool
js_IsLoopEdge(jsbytecode* pc, jsbytecode* header)
{
    switch (*pc) {
      case JSOP_IFEQ:
      case JSOP_IFNE:
        return ((pc + GET_JUMP_OFFSET(pc)) == header);
      case JSOP_IFEQX:
      case JSOP_IFNEX:
        return ((pc + GET_JUMPX_OFFSET(pc)) == header);
      default:
        JS_ASSERT((*pc == JSOP_AND) || (*pc == JSOP_ANDX) ||
                  (*pc == JSOP_OR) || (*pc == JSOP_ORX));
    }
    return false;
}

/* Promote slots if necessary to match the called tree' type map and report error if thats
   impossible. */
JS_REQUIRES_STACK bool
TraceRecorder::adjustCallerTypes(Fragment* f)
{
    uint16* gslots = treeInfo->globalSlots->data();
    unsigned ngslots = treeInfo->globalSlots->length();
    JS_ASSERT(ngslots == treeInfo->nGlobalTypes());
    TreeInfo* ti = (TreeInfo*)f->vmprivate;
    bool ok = true;
    uint8* map = ti->globalTypeMap();
    uint8* m = map;
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        LIns* i = get(vp);
        bool isPromote = isPromoteInt(i);
        if (isPromote && *m == JSVAL_DOUBLE)
            lir->insStorei(get(vp), lirbuf->state, nativeGlobalOffset(vp));
        else if (!isPromote && *m == JSVAL_INT) {
            debug_only_v(printf("adjusting will fail, %s%d, slot %d\n", vpname, vpnum, m - map);)
            oracle.markGlobalSlotUndemotable(cx, gslots[n]);
            ok = false;
        }
        ++m;
    );
    JS_ASSERT(unsigned(m - map) == ti->nGlobalTypes());
    map = ti->stackTypeMap();
    m = map;
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0,
        LIns* i = get(vp);
        bool isPromote = isPromoteInt(i);
        if (isPromote && *m == JSVAL_DOUBLE) {
            lir->insStorei(get(vp), lirbuf->sp,
                           -treeInfo->nativeStackBase + nativeStackOffset(vp));
            /* Aggressively undo speculation so the inner tree will compile if this fails. */
            oracle.markStackSlotUndemotable(cx, unsigned(m - map));
        } else if (!isPromote && *m == JSVAL_INT) {
            debug_only_v(printf("adjusting will fail, %s%d, slot %d\n", vpname, vpnum, m - map);)
            ok = false;
            oracle.markStackSlotUndemotable(cx, unsigned(m - map));
        } else if (JSVAL_IS_INT(*vp) && *m == JSVAL_DOUBLE) {
            /* Aggressively undo speculation so the inner tree will compile if this fails. */
            oracle.markStackSlotUndemotable(cx, unsigned(m - map));
        }
        ++m;
    );
    JS_ASSERT(unsigned(m - map) == ti->nStackTypes);
    JS_ASSERT(f == f->root);
    return ok;
}

JS_REQUIRES_STACK uint8
TraceRecorder::determineSlotType(jsval* vp)
{
    uint8 m;
    LIns* i = get(vp);
    m = isNumber(*vp)
        ? (isPromoteInt(i) ? JSVAL_INT : JSVAL_DOUBLE)
        : JSVAL_IS_NULL(*vp)
        ? JSVAL_TNULL
        : JSVAL_TAG(*vp);
    JS_ASSERT((m != JSVAL_INT) || isInt32(*vp));
    return m;
}

JS_REQUIRES_STACK LIns*
TraceRecorder::snapshot(ExitType exitType)
{
    JSStackFrame* fp = cx->fp;
    JSFrameRegs* regs = fp->regs;
    jsbytecode* pc = regs->pc;
    if (exitType == BRANCH_EXIT && js_IsLoopExit(pc, (jsbytecode*)fragment->root->ip))
        exitType = LOOP_EXIT;

    /* Check for a return-value opcode that needs to restart at the next instruction. */
    const JSCodeSpec& cs = js_CodeSpec[*pc];

    /* WARNING: don't return before restoring the original pc if (resumeAfter). */
    bool resumeAfter = (pendingTraceableNative &&
                        JSTN_ERRTYPE(pendingTraceableNative) == FAIL_STATUS);
    if (resumeAfter) {
        JS_ASSERT(*pc == JSOP_CALL || *pc == JSOP_APPLY);
        pc += cs.length;
        regs->pc = pc;
        MUST_FLOW_THROUGH("restore_pc");
    }

    /* Generate the entry map for the (possibly advanced) pc and stash it in the trace. */
    unsigned stackSlots = js_NativeStackSlots(cx, callDepth);

    /* It's sufficient to track the native stack use here since all stores above the
       stack watermark defined by guards are killed. */
    trackNativeStackUse(stackSlots + 1);

    /* Capture the type map into a temporary location. */
    unsigned ngslots = treeInfo->globalSlots->length();
    unsigned typemap_size = (stackSlots + ngslots) * sizeof(uint8);
    uint8* typemap = (uint8*)alloca(typemap_size);
    uint8* m = typemap;

    /* Determine the type of a store by looking at the current type of the actual value the
       interpreter is using. For numbers we have to check what kind of store we used last
       (integer or double) to figure out what the side exit show reflect in its typemap. */
    FORALL_SLOTS(cx, ngslots, treeInfo->globalSlots->data(), callDepth,
        *m++ = determineSlotType(vp);
    );
    JS_ASSERT(unsigned(m - typemap) == ngslots + stackSlots);

    /* If we are capturing the stack state on a specific instruction, the value on
       the top of the stack is a boxed value. */
    if (resumeAfter) {
        if (pendingTraceableNative->flags & JSTN_UNBOX_AFTER)
            typemap[stackSlots - 1] = JSVAL_BOXED;

        /* Now restore the the original pc (after which early returns are ok). */
        MUST_FLOW_LABEL(restore_pc);
        regs->pc = pc - cs.length;
    } else {
        /* If we take a snapshot on a goto, advance to the target address. This avoids inner
           trees returning on a break goto, which the outer recorder then would confuse with
           a break in the outer tree. */
        if (*pc == JSOP_GOTO)
            pc += GET_JUMP_OFFSET(pc);
        else if (*pc == JSOP_GOTOX)
            pc += GET_JUMPX_OFFSET(pc);
    }

    JS_STATIC_ASSERT (sizeof(GuardRecord) + sizeof(VMSideExit) < MAX_SKIP_BYTES);

    /* Check if we already have a matching side exit. If so use that side exit structure,
       otherwise we have to create our own. */
    VMSideExit** exits = treeInfo->sideExits.data();
    unsigned nexits = treeInfo->sideExits.length();
    if (exitType == LOOP_EXIT) {
        for (unsigned n = 0; n < nexits; ++n) {
            VMSideExit* e = exits[n];
            if (e->pc == pc && e->imacpc == fp->imacpc &&
                !memcmp(getFullTypeMap(exits[n]), typemap, typemap_size)) {
                LIns* data = lir->skip(sizeof(GuardRecord));
                GuardRecord* rec = (GuardRecord*)data->payload();
                /* setup guard record structure with shared side exit */
                memset(rec, 0, sizeof(GuardRecord));
                VMSideExit* exit = exits[n];
                rec->exit = exit;
                exit->addGuard(rec);
                AUDIT(mergedLoopExits);
                return data;
            }
        }
    }

    if (sizeof(GuardRecord) +
        sizeof(VMSideExit) +
        (stackSlots + ngslots) * sizeof(uint8) >= MAX_SKIP_BYTES) {
        /**
         * ::snapshot() is infallible in the sense that callers don't
         * expect errors; but this is a trace-aborting error condition. So
         * mangle the request to consume zero slots, and mark the tree as
         * to-be-trashed. This should be safe as the trace will be aborted
         * before assembly or execution due to the call to
         * trackNativeStackUse above.
         */
        stackSlots = 0;
        ngslots = 0;
        trashSelf = true;
    }

    /* We couldn't find a matching side exit, so create our own side exit structure. */
    LIns* data = lir->skip(sizeof(GuardRecord) +
                           sizeof(VMSideExit) +
                           (stackSlots + ngslots) * sizeof(uint8));
    GuardRecord* rec = (GuardRecord*)data->payload();
    VMSideExit* exit = (VMSideExit*)(rec + 1);
    /* setup guard record structure */
    memset(rec, 0, sizeof(GuardRecord));
    rec->exit = exit;
    /* setup side exit structure */
    memset(exit, 0, sizeof(VMSideExit));
    exit->from = fragment;
    exit->calldepth = callDepth;
    exit->numGlobalSlots = ngslots;
    exit->numStackSlots = stackSlots;
    exit->numStackSlotsBelowCurrentFrame = cx->fp->callee
        ? nativeStackOffset(&cx->fp->argv[-2])/sizeof(double)
        : 0;
    exit->exitType = exitType;
    exit->addGuard(rec);
    exit->block = fp->blockChain;
    exit->pc = pc;
    exit->imacpc = fp->imacpc;
    exit->sp_adj = (stackSlots * sizeof(double)) - treeInfo->nativeStackBase;
    exit->rp_adj = exit->calldepth * sizeof(FrameInfo*);
    memcpy(getFullTypeMap(exit), typemap, typemap_size);

    /* BIG FAT WARNING: If compilation fails, we currently don't reset the lirbuf so its safe
       to keep references to the side exits here. If we ever start rewinding those lirbufs,
       we have to make sure we purge the side exits that then no longer will be in valid
       memory. */
    if (exitType == LOOP_EXIT)
        treeInfo->sideExits.add(exit);
    return data;
}

/* Emit a guard for condition (cond), expecting to evaluate to boolean result (expected)
   and using the supplied side exit if the conditon doesn't hold. */
LIns*
TraceRecorder::guard(bool expected, LIns* cond, LIns* exit)
{
    if (!cond->isCond()) {
        expected = !expected;
        cond = lir->ins_eq0(cond);
    }
    return lir->insGuard(expected ? LIR_xf : LIR_xt, cond, exit);
}

/* Emit a guard for condition (cond), expecting to evaluate to boolean result (expected)
   and generate a side exit with type exitType to jump to if the condition does not hold. */
JS_REQUIRES_STACK LIns*
TraceRecorder::guard(bool expected, LIns* cond, ExitType exitType)
{
    return guard(expected, cond, snapshot(exitType));
}

/* Try to match the type of a slot to type t. checkType is used to verify that the type of
 * values flowing into the loop edge is compatible with the type we expect in the loop header.
 *
 * @param v             Value.
 * @param t             Typemap entry for value.
 * @param stage_val     Outparam for set() address.
 * @param stage_ins     Outparam for set() instruction.
 * @param stage_count   Outparam for set() buffer count.
 * @return              True if types are compatible, false otherwise.
 */
JS_REQUIRES_STACK bool
TraceRecorder::checkType(jsval& v, uint8 t, jsval*& stage_val, LIns*& stage_ins,
                         unsigned& stage_count)
{
    if (t == JSVAL_INT) { /* initially all whole numbers cause the slot to be demoted */
        debug_only_v(printf("checkType(tag=1, t=%d, isnum=%d, i2f=%d) stage_count=%d\n",
                            t,
                            isNumber(v),
                            isPromoteInt(get(&v)),
                            stage_count);)
        if (!isNumber(v))
            return false; /* not a number? type mismatch */
        LIns* i = get(&v);
        /* This is always a type mismatch, we can't close a double to an int. */
        if (!isPromoteInt(i))
            return false;
        /* Looks good, slot is an int32, the last instruction should be promotable. */
        JS_ASSERT(isInt32(v) && isPromoteInt(i));
        /* Overwrite the value in this slot with the argument promoted back to an integer. */
        stage_val = &v;
        stage_ins = f2i(i);
        stage_count++;
        return true;
    }
    if (t == JSVAL_DOUBLE) {
        debug_only_v(printf("checkType(tag=2, t=%d, isnum=%d, promote=%d) stage_count=%d\n",
                            t,
                            isNumber(v),
                            isPromoteInt(get(&v)),
                            stage_count);)
        if (!isNumber(v))
            return false; /* not a number? type mismatch */
        LIns* i = get(&v);
        /* We sink i2f conversions into the side exit, but at the loop edge we have to make
           sure we promote back to double if at loop entry we want a double. */
        if (isPromoteInt(i)) {
            stage_val = &v;
            stage_ins = lir->ins1(LIR_i2f, i);
            stage_count++;
        }
        return true;
    }
    if (t == JSVAL_TNULL && JSVAL_IS_NULL(v))
        return true;
    /* for non-number types we expect a precise match of the type */
    uint8 vt = getCoercedType(v);
#ifdef DEBUG
    if (vt != t) {
        debug_only_v(printf("Type mismatch: val %c, map %c ", typeChar[vt],
                            typeChar[t]);)
    }
#endif
    debug_only_v(printf("checkType(vt=%d, t=%d) stage_count=%d\n",
                        (int) vt, t, stage_count);)
    return vt == t;
}

/**
 * Make sure that the current values in the given stack frame and all stack frames
 * up and including entryFrame are type-compatible with the entry map.
 *
 * @param root_peer         First fragment in peer list.
 * @param stable_peer       Outparam for first type stable peer.
 * @param demote            True if stability was achieved through demotion.
 * @return                  True if type stable, false otherwise.
 */
JS_REQUIRES_STACK bool
TraceRecorder::deduceTypeStability(Fragment* root_peer, Fragment** stable_peer, bool& demote)
{
    uint8* m;
    uint8* typemap;
    unsigned ngslots = treeInfo->globalSlots->length();
    uint16* gslots = treeInfo->globalSlots->data();
    JS_ASSERT(ngslots == treeInfo->nGlobalTypes());

    if (stable_peer)
        *stable_peer = NULL;

    /*
     * Rather than calculate all of this stuff twice, it gets cached locally.  The "stage" buffers
     * are for calls to set() that will change the exit types.
     */
    bool success;
    unsigned stage_count;
    jsval** stage_vals = (jsval**)alloca(sizeof(jsval*) * (treeInfo->typeMap.length()));
    LIns** stage_ins = (LIns**)alloca(sizeof(LIns*) * (treeInfo->typeMap.length()));

    /* First run through and see if we can close ourselves - best case! */
    stage_count = 0;
    success = false;

    debug_only_v(printf("Checking type stability against self=%p\n", (void*)fragment);)

    m = typemap = treeInfo->globalTypeMap();
    FORALL_GLOBAL_SLOTS(cx, ngslots, gslots,
        debug_only_v(printf("%s%d ", vpname, vpnum);)
        if (!checkType(*vp, *m, stage_vals[stage_count], stage_ins[stage_count], stage_count)) {
            /* If the failure was an int->double, tell the oracle. */
            if (*m == JSVAL_INT && isNumber(*vp) && !isPromoteInt(get(vp))) {
                oracle.markGlobalSlotUndemotable(cx, gslots[n]);
                demote = true;
            } else {
                goto checktype_fail_1;
            }
        }
        ++m;
    );
    m = typemap = treeInfo->stackTypeMap();
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0,
        debug_only_v(printf("%s%d ", vpname, vpnum);)
        if (!checkType(*vp, *m, stage_vals[stage_count], stage_ins[stage_count], stage_count)) {
            if (*m == JSVAL_INT && isNumber(*vp) && !isPromoteInt(get(vp))) {
                oracle.markStackSlotUndemotable(cx, unsigned(m - typemap));
                demote = true;
            } else {
                goto checktype_fail_1;
            }
        }
        ++m;
    );

    success = true;

checktype_fail_1:
    /* If we got a success and we don't need to recompile, we should just close here. */
    if (success && !demote) {
        for (unsigned i = 0; i < stage_count; i++)
            set(stage_vals[i], stage_ins[i]);
        return true;
    /* If we need to trash, don't bother checking peers. */
    } else if (trashSelf) {
        return false;
    }

    demote = false;

    /* At this point the tree is about to be incomplete, so let's see if we can connect to any
     * peer fragment that is type stable.
     */
    Fragment* f;
    TreeInfo* ti;
    for (f = root_peer; f != NULL; f = f->peer) {
        debug_only_v(printf("Checking type stability against peer=%p (code=%p)\n", (void*)f, f->code());)
        if (!f->code())
            continue;
        ti = (TreeInfo*)f->vmprivate;
        /* Don't allow varying stack depths */
        if ((ti->nStackTypes != treeInfo->nStackTypes) ||
            (ti->typeMap.length() != treeInfo->typeMap.length()) ||
            (ti->globalSlots->length() != treeInfo->globalSlots->length()))
            continue;
        stage_count = 0;
        success = false;

        m = ti->globalTypeMap();
        FORALL_GLOBAL_SLOTS(cx, treeInfo->globalSlots->length(), treeInfo->globalSlots->data(),
                if (!checkType(*vp, *m, stage_vals[stage_count], stage_ins[stage_count], stage_count))
                    goto checktype_fail_2;
                ++m;
            );

        m = ti->stackTypeMap();
        FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0,
                if (!checkType(*vp, *m, stage_vals[stage_count], stage_ins[stage_count], stage_count))
                    goto checktype_fail_2;
                ++m;
            );

        success = true;

checktype_fail_2:
        if (success) {
            /*
             * There was a successful match.  We don't care about restoring the saved staging, but
             * we do need to clear the original undemote list.
             */
            for (unsigned i = 0; i < stage_count; i++)
                set(stage_vals[i], stage_ins[i]);
            if (stable_peer)
                *stable_peer = f;
            demote = false;
            return false;
        }
    }

    /*
     * If this is a loop trace and it would be stable with demotions, build an undemote list
     * and return true.  Our caller should sniff this and trash the tree, recording a new one
     * that will assumedly stabilize.
     */
    if (demote && fragment->kind == LoopTrace) {
        typemap = m = treeInfo->globalTypeMap();
        FORALL_GLOBAL_SLOTS(cx, treeInfo->globalSlots->length(), treeInfo->globalSlots->data(),
            if (*m == JSVAL_INT) {
                JS_ASSERT(isNumber(*vp));
                if (!isPromoteInt(get(vp)))
                    oracle.markGlobalSlotUndemotable(cx, gslots[n]);
            } else if (*m == JSVAL_DOUBLE) {
                JS_ASSERT(isNumber(*vp));
                oracle.markGlobalSlotUndemotable(cx, gslots[n]);
            } else {
                JS_ASSERT(*m == JSVAL_TAG(*vp));
            }
            m++;
        );

        typemap = m = treeInfo->stackTypeMap();
        FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0,
            if (*m == JSVAL_INT) {
                JS_ASSERT(isNumber(*vp));
                if (!isPromoteInt(get(vp)))
                    oracle.markStackSlotUndemotable(cx, unsigned(m - typemap));
            } else if (*m == JSVAL_DOUBLE) {
                JS_ASSERT(isNumber(*vp));
                oracle.markStackSlotUndemotable(cx, unsigned(m - typemap));
            } else {
                JS_ASSERT((*m == JSVAL_TNULL) ? JSVAL_IS_NULL(*vp) : *m == JSVAL_TAG(*vp));
            }
            m++;
        );
        return true;
    } else {
        demote = false;
    }

    return false;
}

/* Check whether the current pc location is the loop header of the loop this recorder records. */
JS_REQUIRES_STACK bool
TraceRecorder::isLoopHeader(JSContext* cx) const
{
    return cx->fp->regs->pc == fragment->root->ip;
}

/* Compile the current fragment. */
JS_REQUIRES_STACK void
TraceRecorder::compile(JSTraceMonitor* tm)
{
    Fragmento* fragmento = tm->fragmento;
    if (treeInfo->maxNativeStackSlots >= MAX_NATIVE_STACK_SLOTS) {
        debug_only_v(printf("Blacklist: excessive stack use.\n"));
        js_Blacklist(fragment->root);
        return;
    }
    if (anchor && anchor->exitType != CASE_EXIT)
        ++treeInfo->branchCount;
    if (lirbuf->outOMem()) {
        fragmento->assm()->setError(nanojit::OutOMem);
        return;
    }
    ::compile(fragmento->assm(), fragment);
    if (fragmento->assm()->error() == nanojit::OutOMem)
        return;
    if (fragmento->assm()->error() != nanojit::None) {
        debug_only_v(printf("Blacklisted: error during compilation\n");)
        js_Blacklist(fragment->root);
        return;
    }
    if (anchor) {
#ifdef NANOJIT_IA32
        if (anchor->exitType == CASE_EXIT)
            fragmento->assm()->patch(anchor, anchor->switchInfo);
        else
#endif
            fragmento->assm()->patch(anchor);
    }
    JS_ASSERT(fragment->code());
    JS_ASSERT(!fragment->vmprivate);
    if (fragment == fragment->root)
        fragment->vmprivate = treeInfo;
    /* :TODO: windows support */
#if defined DEBUG && !defined WIN32
    const char* filename = cx->fp->script->filename;
    char* label = (char*)malloc((filename ? strlen(filename) : 7) + 16);
    sprintf(label, "%s:%u", filename ? filename : "<stdin>",
            js_FramePCToLineNumber(cx, cx->fp));
    fragmento->labels->add(fragment, sizeof(Fragment), 0, label);
    free(label);
#endif
    AUDIT(traceCompleted);
}

static bool
js_JoinPeersIfCompatible(Fragmento* frago, Fragment* stableFrag, TreeInfo* stableTree,
                         VMSideExit* exit)
{
    JS_ASSERT(exit->numStackSlots == stableTree->nStackTypes);

    /* Must have a matching type unstable exit. */
    if ((exit->numGlobalSlots + exit->numStackSlots != stableTree->typeMap.length()) ||
        memcmp(getFullTypeMap(exit), stableTree->typeMap.data(), stableTree->typeMap.length())) {
       return false;
    }

    exit->target = stableFrag;
    frago->assm()->patch(exit);

    stableTree->dependentTrees.addUnique(exit->from->root);
    ((TreeInfo*)exit->from->root->vmprivate)->linkedTrees.addUnique(stableFrag);

    return true;
}

/* Complete and compile a trace and link it to the existing tree if appropriate. */
JS_REQUIRES_STACK bool
TraceRecorder::closeLoop(JSTraceMonitor* tm, bool& demote)
{
    bool stable;
    LIns* exitIns;
    Fragment* peer;
    VMSideExit* exit;
    Fragment* peer_root;
    Fragmento* fragmento = tm->fragmento;

    exitIns = snapshot(UNSTABLE_LOOP_EXIT);
    exit = (VMSideExit*)((GuardRecord*)exitIns->payload())->exit;

    if (callDepth != 0) {
        debug_only_v(printf("Blacklisted: stack depth mismatch, possible recursion.\n");)
        js_Blacklist(fragment->root);
        trashSelf = true;
        return false;
    }

    JS_ASSERT(exit->numStackSlots == treeInfo->nStackTypes);

    peer_root = getLoop(traceMonitor, fragment->root->ip, treeInfo->globalShape);
    JS_ASSERT(peer_root != NULL);
    stable = deduceTypeStability(peer_root, &peer, demote);

    #if DEBUG
    if (!stable)
        AUDIT(unstableLoopVariable);
    #endif

    if (trashSelf) {
        debug_only_v(printf("Trashing tree from type instability.\n");)
        return false;
    }

    if (stable && demote) {
        JS_ASSERT(fragment->kind == LoopTrace);
        return false;
    }

    if (!stable) {
        fragment->lastIns = lir->insGuard(LIR_x, lir->insImm(1), exitIns);

        /*
         * If we didn't find a type stable peer, we compile the loop anyway and
         * hope it becomes stable later.
         */
        if (!peer) {
            /*
             * If such a fragment does not exist, let's compile the loop ahead
             * of time anyway.  Later, if the loop becomes type stable, we will
             * connect these two fragments together.
             */
            debug_only_v(printf("Trace has unstable loop variable with no stable peer, "
                                "compiling anyway.\n");)
            UnstableExit* uexit = new UnstableExit;
            uexit->fragment = fragment;
            uexit->exit = exit;
            uexit->next = treeInfo->unstableExits;
            treeInfo->unstableExits = uexit;

            /*
             * If we walked out of a loop, this exit is wrong. We need to back
             * up to the if operation.
             */
            if (walkedOutOfLoop()) {
                exit->pc = terminate_pc;
                exit->imacpc = terminate_imacpc;
            }
        } else {
            JS_ASSERT(peer->code());
            exit->target = peer;
            debug_only_v(printf("Joining type-unstable trace to target fragment %p.\n", (void*)peer);)
            stable = true;
            ((TreeInfo*)peer->vmprivate)->dependentTrees.addUnique(fragment->root);
            treeInfo->linkedTrees.addUnique(peer);
        }

        compile(tm);
    } else {
        exit->target = fragment->root;
        fragment->lastIns = lir->insGuard(LIR_loop, lir->insImm(1), exitIns);
        compile(tm);
    }

    if (fragmento->assm()->error() != nanojit::None)
        return false;

    joinEdgesToEntry(fragmento, peer_root);

    debug_only_v(printf("updating specializations on dependent and linked trees\n"))
    if (fragment->root->vmprivate)
        specializeTreesToMissingGlobals(cx, (TreeInfo*)fragment->root->vmprivate);

    /* 
     * If this is a newly formed tree, and the outer tree has not been compiled yet, we
     * should try to compile the outer tree again.
     */
    if (outer)
        js_AttemptCompilation(tm, globalObj, outer);
    
    debug_only_v(printf("recording completed at %s:%u@%u via closeLoop\n",
                        cx->fp->script->filename,
                        js_FramePCToLineNumber(cx, cx->fp),
                        FramePCOffset(cx->fp));)
    return true;
}

JS_REQUIRES_STACK void
TraceRecorder::joinEdgesToEntry(Fragmento* fragmento, Fragment* peer_root)
{
    if (fragment->kind == LoopTrace) {
        TreeInfo* ti;
        Fragment* peer;
        uint8* t1, *t2;
        UnstableExit* uexit, **unext;
        uint32* stackDemotes = (uint32*)alloca(sizeof(uint32) * treeInfo->nStackTypes);
        uint32* globalDemotes = (uint32*)alloca(sizeof(uint32) * treeInfo->nGlobalTypes());

        for (peer = peer_root; peer != NULL; peer = peer->peer) {
            if (!peer->code())
                continue;
            ti = (TreeInfo*)peer->vmprivate;
            uexit = ti->unstableExits;
            unext = &ti->unstableExits;
            while (uexit != NULL) {
                bool remove = js_JoinPeersIfCompatible(fragmento, fragment, treeInfo, uexit->exit);
                JS_ASSERT(!remove || fragment != peer);
                debug_only_v(if (remove) {
                             printf("Joining type-stable trace to target exit %p->%p.\n",
                                    (void*)uexit->fragment, (void*)uexit->exit); });
                if (!remove) {
                    /* See if this exit contains mismatch demotions, which imply trashing a tree.
                       This is actually faster than trashing the original tree as soon as the
                       instability is detected, since we could have compiled a fairly stable
                       tree that ran faster with integers. */
                    unsigned stackCount = 0;
                    unsigned globalCount = 0;
                    t1 = treeInfo->stackTypeMap();
                    t2 = getStackTypeMap(uexit->exit);
                    for (unsigned i = 0; i < uexit->exit->numStackSlots; i++) {
                        if (t2[i] == JSVAL_INT && t1[i] == JSVAL_DOUBLE) {
                            stackDemotes[stackCount++] = i;
                        } else if (t2[i] != t1[i]) {
                            stackCount = 0;
                            break;
                        }
                    }
                    t1 = treeInfo->globalTypeMap();
                    t2 = getGlobalTypeMap(uexit->exit);
                    for (unsigned i = 0; i < uexit->exit->numGlobalSlots; i++) {
                        if (t2[i] == JSVAL_INT && t1[i] == JSVAL_DOUBLE) {
                            globalDemotes[globalCount++] = i;
                        } else if (t2[i] != t1[i]) {
                            globalCount = 0;
                            stackCount = 0;
                            break;
                        }
                    }
                    if (stackCount || globalCount) {
                        for (unsigned i = 0; i < stackCount; i++)
                            oracle.markStackSlotUndemotable(cx, stackDemotes[i]);
                        for (unsigned i = 0; i < globalCount; i++)
                            oracle.markGlobalSlotUndemotable(cx, ti->globalSlots->data()[globalDemotes[i]]);
                        JS_ASSERT(peer == uexit->fragment->root);
                        if (fragment == peer)
                            trashSelf = true;
                        else
                            whichTreesToTrash.addUnique(uexit->fragment->root);
                        break;
                    }
                }
                if (remove) {
                    *unext = uexit->next;
                    delete uexit;
                    uexit = *unext;
                } else {
                    unext = &uexit->next;
                    uexit = uexit->next;
                }
            }
        }
    }

    debug_only_v(js_DumpPeerStability(traceMonitor, peer_root->ip, treeInfo->globalShape);)
}

/* Emit an always-exit guard and compile the tree (used for break statements. */
JS_REQUIRES_STACK void
TraceRecorder::endLoop(JSTraceMonitor* tm)
{
    LIns* exitIns = snapshot(LOOP_EXIT);

    if (callDepth != 0) {
        debug_only_v(printf("Blacklisted: stack depth mismatch, possible recursion.\n");)
        js_Blacklist(fragment->root);
        trashSelf = true;
        return;
    }

    fragment->lastIns = lir->insGuard(LIR_x, lir->insImm(1), exitIns);
    compile(tm);

    if (tm->fragmento->assm()->error() != nanojit::None)
        return;

    joinEdgesToEntry(tm->fragmento, getLoop(tm, fragment->root->ip, treeInfo->globalShape));

    /* Note: this must always be done, in case we added new globals on trace and haven't yet 
       propagated those to linked and dependent trees. */
    debug_only_v(printf("updating specializations on dependent and linked trees\n"))
    if (fragment->root->vmprivate)
        specializeTreesToMissingGlobals(cx, (TreeInfo*)fragment->root->vmprivate);

    /* 
     * If this is a newly formed tree, and the outer tree has not been compiled yet, we
     * should try to compile the outer tree again.
     */
    if (outer)
        js_AttemptCompilation(tm, globalObj, outer);
    
    debug_only_v(printf("recording completed at %s:%u@%u via endLoop\n",
                        cx->fp->script->filename,
                        js_FramePCToLineNumber(cx, cx->fp),
                        FramePCOffset(cx->fp));)
}

/* Emit code to adjust the stack to match the inner tree's stack expectations. */
JS_REQUIRES_STACK void
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
        ptrdiff_t rp_adj = callDepth * sizeof(FrameInfo*);
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
                ti->maxCallDepth * sizeof(FrameInfo*));
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
JS_REQUIRES_STACK void
TraceRecorder::emitTreeCall(Fragment* inner, VMSideExit* exit)
{
    TreeInfo* ti = (TreeInfo*)inner->vmprivate;
    /* Invoke the inner tree. */
    LIns* args[] = { INS_CONSTPTR(inner), lirbuf->state }; /* reverse order */
    LIns* ret = lir->insCall(&js_CallTree_ci, args);
    /* Read back all registers, in case the called tree changed any of them. */
    import(ti, inner_sp_ins, exit->numStackSlots, exit->numGlobalSlots,
           exit->calldepth, getFullTypeMap(exit));
    /* Restore sp and rp to their original values (we still have them in a register). */
    if (callDepth > 0) {
        lir->insStorei(lirbuf->sp, lirbuf->state, offsetof(InterpState, sp));
        lir->insStorei(lirbuf->rp, lirbuf->state, offsetof(InterpState, rp));
    }
    /* Guard that we come out of the inner tree along the same side exit we came out when
       we called the inner tree at recording time. */
    guard(true, lir->ins2(LIR_eq, ret, INS_CONSTPTR(exit)), NESTED_EXIT);
    /* Register us as a dependent tree of the inner tree. */
    ((TreeInfo*)inner->vmprivate)->dependentTrees.addUnique(fragment->root);
    treeInfo->linkedTrees.addUnique(inner);
}

/* Add a if/if-else control-flow merge point to the list of known merge points. */
JS_REQUIRES_STACK void
TraceRecorder::trackCfgMerges(jsbytecode* pc)
{
    /* If we hit the beginning of an if/if-else, then keep track of the merge point after it. */
    JS_ASSERT((*pc == JSOP_IFEQ) || (*pc == JSOP_IFEQX));
    jssrcnote* sn = js_GetSrcNote(cx->fp->script, pc);
    if (sn != NULL) {
        if (SN_TYPE(sn) == SRC_IF) {
            cfgMerges.add((*pc == JSOP_IFEQ)
                          ? pc + GET_JUMP_OFFSET(pc)
                          : pc + GET_JUMPX_OFFSET(pc));
        } else if (SN_TYPE(sn) == SRC_IF_ELSE)
            cfgMerges.add(pc + js_GetSrcNoteOffset(sn, 0));
    }
}

/* Invert the direction of the guard if this is a loop edge that is not
   taken (thin loop). */
JS_REQUIRES_STACK void
TraceRecorder::flipIf(jsbytecode* pc, bool& cond)
{
    if (js_IsLoopEdge(pc, (jsbytecode*)fragment->root->ip)) {
        switch (*pc) {
          case JSOP_IFEQ:
          case JSOP_IFEQX:
            if (!cond)
                return;
            break;
          case JSOP_IFNE:
          case JSOP_IFNEX:
            if (cond)
                return;
            break;
          default:
            JS_NOT_REACHED("flipIf");
        }
        /* We are about to walk out of the loop, so terminate it with
           an inverse loop condition. */
        debug_only_v(printf("Walking out of the loop, terminating it anyway.\n");)
        cond = !cond;
        terminate = true;
        /* If when we get to closeLoop the tree is decided to be type unstable, we need to
           reverse this logic because the loop won't be closed after all.  Store the real
           value of the IP the interpreter expects, so we can use it in our final LIR_x.
         */
        if (*pc == JSOP_IFEQX || *pc == JSOP_IFNEX)
            pc += GET_JUMPX_OFFSET(pc);
        else
            pc += GET_JUMP_OFFSET(pc);
        terminate_pc = pc;
        terminate_imacpc = cx->fp->imacpc;
    }
}

/* Emit code for a fused IFEQ/IFNE. */
JS_REQUIRES_STACK void
TraceRecorder::fuseIf(jsbytecode* pc, bool cond, LIns* x)
{
    if (x->isconst()) // no need to guard if condition is constant
        return;
    if (*pc == JSOP_IFEQ) {
        flipIf(pc, cond);
        guard(cond, x, BRANCH_EXIT);
        trackCfgMerges(pc);
    } else if (*pc == JSOP_IFNE) {
        flipIf(pc, cond);
        guard(cond, x, BRANCH_EXIT);
    }
}

bool
TraceRecorder::hasMethod(JSObject* obj, jsid id)
{
    if (!obj)
        return false;

    JSObject* pobj;
    JSProperty* prop;
    int protoIndex = OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop);
    if (protoIndex < 0 || !prop)
        return false;

    bool found = false;
    if (OBJ_IS_NATIVE(pobj)) {
        JSScope* scope = OBJ_SCOPE(pobj);
        JSScopeProperty* sprop = (JSScopeProperty*) prop;

        if (SPROP_HAS_STUB_GETTER(sprop) &&
            SPROP_HAS_VALID_SLOT(sprop, scope)) {
            jsval v = LOCKED_OBJ_GET_SLOT(pobj, sprop->slot);
            if (VALUE_IS_FUNCTION(cx, v)) {
                found = true;
                if (!SCOPE_IS_BRANDED(scope)) {
                    SCOPE_MAKE_UNIQUE_SHAPE(cx, scope);
                    SCOPE_SET_BRANDED(scope);
                }
            }
        }
    }

    OBJ_DROP_PROPERTY(cx, pobj, prop);
    return found;
}

JS_REQUIRES_STACK bool
TraceRecorder::hasIteratorMethod(JSObject* obj)
{
    JS_ASSERT(cx->fp->regs->sp + 2 <= cx->fp->slots + cx->fp->script->nslots);

    return hasMethod(obj, ATOM_TO_JSID(cx->runtime->atomState.iteratorAtom));
}

int
nanojit::StackFilter::getTop(LInsp guard)
{
    VMSideExit* e = (VMSideExit*)guard->record()->exit;
    if (sp == lirbuf->sp)
        return e->sp_adj;
    JS_ASSERT(sp == lirbuf->rp);
    return e->rp_adj;
}

#if defined NJ_VERBOSE
void
nanojit::LirNameMap::formatGuard(LIns *i, char *out)
{
    VMSideExit *x;

    x = (VMSideExit *)i->record()->exit;
    sprintf(out,
            "%s: %s %s -> pc=%p imacpc=%p sp%+ld rp%+ld",
            formatRef(i),
            lirNames[i->opcode()],
            i->oprnd1()->isCond() ? formatRef(i->oprnd1()) : "",
            (void *)x->pc,
            (void *)x->imacpc,
            (long int)x->sp_adj,
            (long int)x->rp_adj);
}
#endif

void
nanojit::Fragment::onDestroy()
{
    delete (TreeInfo *)vmprivate;
}

static JS_REQUIRES_STACK bool
js_DeleteRecorder(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Aborting and completing a trace end up here. */
    delete tm->recorder;
    tm->recorder = NULL;

    /*
     * If we ran out of memory, flush the code cache.
     */
    if (JS_TRACE_MONITOR(cx).fragmento->assm()->error() == OutOMem) {
        js_FlushJITCache(cx);
        return false;
    }

    return true;
}

/**
 * Checks whether the shape of the global object has changed.
 */
static inline bool
js_CheckGlobalObjectShape(JSContext* cx, JSTraceMonitor* tm, JSObject* globalObj,
                          uint32 *shape=NULL, SlotList** slots=NULL)
{
    if (tm->needFlush) {
        tm->needFlush = JS_FALSE;
        return false;
    }

    uint32 globalShape = OBJ_SHAPE(globalObj);

    if (tm->recorder) {
        TreeInfo* ti = tm->recorder->getTreeInfo();
        /* Check the global shape matches the recorder's treeinfo's shape. */
        if (globalShape != ti->globalShape) {
            AUDIT(globalShapeMismatchAtEntry);
            debug_only_v(printf("Global shape mismatch (%u vs. %u), flushing cache.\n",
                                globalShape, ti->globalShape);)
            return false;
        }
        if (shape)
            *shape = globalShape;
        if (slots)
            *slots = ti->globalSlots;
        return true;
    }

    /* No recorder, search for a tracked global-state (or allocate one). */
    for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
        GlobalState &state = tm->globalStates[i];

        if (state.globalShape == (uint32) -1) {
            state.globalShape = globalShape;
            JS_ASSERT(state.globalSlots);
            JS_ASSERT(state.globalSlots->length() == 0);
        }

        if (tm->globalStates[i].globalShape == globalShape) {
            if (shape)
                *shape = globalShape;
            if (slots)
                *slots = state.globalSlots;
            return true;
        }
    }

    /* No currently-tracked-global found and no room to allocate, abort. */
    AUDIT(globalShapeMismatchAtEntry);
    debug_only_v(printf("No global slotlist for global shape %u, flushing cache.\n",
                        globalShape));
    return false;
}

static JS_REQUIRES_STACK bool
js_StartRecorder(JSContext* cx, VMSideExit* anchor, Fragment* f, TreeInfo* ti,
                 unsigned stackSlots, unsigned ngslots, uint8* typeMap,
                 VMSideExit* expectedInnerExit, jsbytecode* outer)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(f->root != f || !cx->fp->imacpc);

    if (JS_TRACE_MONITOR(cx).prohibitRecording)
        return false;

    /* start recording if no exception during construction */
    tm->recorder = new (&gc) TraceRecorder(cx, anchor, f, ti,
                                           stackSlots, ngslots, typeMap,
                                           expectedInnerExit, outer);

    if (cx->throwing) {
        js_AbortRecording(cx, "setting up recorder failed");
        return false;
    }
    /* clear any leftover error state */
    tm->fragmento->assm()->setError(None);
    return true;
}

static void
js_TrashTree(JSContext* cx, Fragment* f)
{
    JS_ASSERT((!f->code()) == (!f->vmprivate));
    JS_ASSERT(f == f->root);
    if (!f->code())
        return;
    AUDIT(treesTrashed);
    debug_only_v(printf("Trashing tree info.\n");)
    Fragmento* fragmento = JS_TRACE_MONITOR(cx).fragmento;
    TreeInfo* ti = (TreeInfo*)f->vmprivate;
    f->vmprivate = NULL;
    f->releaseCode(fragmento);
    Fragment** data = ti->dependentTrees.data();
    unsigned length = ti->dependentTrees.length();
    for (unsigned n = 0; n < length; ++n)
        js_TrashTree(cx, data[n]);
    delete ti;
    JS_ASSERT(!f->code() && !f->vmprivate);
}

static int
js_SynthesizeFrame(JSContext* cx, const FrameInfo& fi)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JS_ASSERT(HAS_FUNCTION_CLASS(fi.callee));

    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, fi.callee);
    JS_ASSERT(FUN_INTERPRETED(fun));

    /* Assert that we have a correct sp distance from cx->fp->slots in fi. */
    JSStackFrame* fp = cx->fp;
    JS_ASSERT_IF(!fi.imacpc,
                 js_ReconstructStackDepth(cx, fp->script, fi.pc)
                 == uintN(fi.s.spdist - fp->script->nfixed));

    uintN nframeslots = JS_HOWMANY(sizeof(JSInlineFrame), sizeof(jsval));
    JSScript* script = fun->u.i.script;
    size_t nbytes = (nframeslots + script->nslots) * sizeof(jsval);

    /* Code duplicated from inline_call: case in js_Interpret (FIXME). */
    JSArena* a = cx->stackPool.current;
    void* newmark = (void*) a->avail;
    uintN argc = fi.s.argc & 0x7fff;
    jsval* vp = fp->slots + fi.s.spdist - (2 + argc);
    uintN missing = 0;
    jsval* newsp;

    if (fun->nargs > argc) {
        const JSFrameRegs& regs = *fp->regs;

        newsp = vp + 2 + fun->nargs;
        JS_ASSERT(newsp > regs.sp);
        if ((jsuword) newsp <= a->limit) {
            if ((jsuword) newsp > a->avail)
                a->avail = (jsuword) newsp;
            jsval* argsp = newsp;
            do {
                *--argsp = JSVAL_VOID;
            } while (argsp != regs.sp);
            missing = 0;
        } else {
            missing = fun->nargs - argc;
            nbytes += (2 + fun->nargs) * sizeof(jsval);
        }
    }

    /* Allocate the inline frame with its vars and operands. */
    if (a->avail + nbytes <= a->limit) {
        newsp = (jsval *) a->avail;
        a->avail += nbytes;
        JS_ASSERT(missing == 0);
    } else {
        /* This allocation is infallible: js_ExecuteTree reserved enough stack. */
        JS_ARENA_ALLOCATE_CAST(newsp, jsval *, &cx->stackPool, nbytes);
        JS_ASSERT(newsp);

        /*
         * Move args if the missing ones overflow arena a, then push
         * undefined for the missing args.
         */
        if (missing) {
            memcpy(newsp, vp, (2 + argc) * sizeof(jsval));
            vp = newsp;
            newsp = vp + 2 + argc;
            do {
                *newsp++ = JSVAL_VOID;
            } while (--missing != 0);
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

    bool constructing = (fi.s.argc & 0x8000) != 0;
    newifp->frame.argc = argc;
    newifp->callerRegs.pc = fi.pc;
    newifp->callerRegs.sp = fp->slots + fi.s.spdist;
    fp->imacpc = fi.imacpc;

#ifdef DEBUG
    if (fi.block != fp->blockChain) {
        for (JSObject* obj = fi.block; obj != fp->blockChain; obj = STOBJ_GET_PARENT(obj))
            JS_ASSERT(obj);
    }
#endif
    fp->blockChain = fi.block;

    newifp->frame.argv = newifp->callerRegs.sp - argc;
    JS_ASSERT(newifp->frame.argv);
#ifdef DEBUG
    // Initialize argv[-1] to a known-bogus value so we'll catch it if
    // someone forgets to initialize it later.
    newifp->frame.argv[-1] = JSVAL_HOLE;
#endif
    JS_ASSERT(newifp->frame.argv >= StackBase(fp) + 2);

    newifp->frame.rval = JSVAL_VOID;
    newifp->frame.down = fp;
    newifp->frame.annotation = NULL;
    newifp->frame.scopeChain = OBJ_GET_PARENT(cx, fi.callee);
    newifp->frame.sharpDepth = 0;
    newifp->frame.sharpArray = NULL;
    newifp->frame.flags = constructing ? JSFRAME_CONSTRUCTING : 0;
    newifp->frame.dormantNext = NULL;
    newifp->frame.xmlNamespace = NULL;
    newifp->frame.blockChain = NULL;
    newifp->mark = newmark;
    newifp->frame.thisp = NULL; // will be set by js_ExecuteTree -> FlushNativeStackFrame

    newifp->frame.regs = fp->regs;
    newifp->frame.regs->pc = script->code;
    newifp->frame.regs->sp = newsp + script->nfixed;
    newifp->frame.imacpc = NULL;
    newifp->frame.slots = newsp;
    if (script->staticDepth < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[script->staticDepth];
        newifp->frame.displaySave = *disp;
        *disp = &newifp->frame;
    }
#ifdef DEBUG
    newifp->frame.pcDisabledSave = 0;
#endif

    /*
     * Note that fp->script is still the caller's script; set the callee
     * inline frame's idea of caller version from its version.
     */
    newifp->callerVersion = (JSVersion) fp->script->version;

    // After this paragraph, fp and cx->fp point to the newly synthesized frame.
    fp->regs = &newifp->callerRegs;
    fp = cx->fp = &newifp->frame;

    if (fun->flags & JSFUN_HEAVYWEIGHT) {
        /*
         * Set hookData to null because the failure case for js_GetCallObject
         * involves it calling the debugger hook.
         *
         * Allocating the Call object must not fail, so use an object
         * previously reserved by js_ExecuteTrace if needed.
         */
        newifp->hookData = NULL;
        JS_ASSERT(!JS_TRACE_MONITOR(cx).useReservedObjects);
        JS_TRACE_MONITOR(cx).useReservedObjects = JS_TRUE;
#ifdef DEBUG
        JSObject *obj =
#endif
            js_GetCallObject(cx, &newifp->frame);
        JS_ASSERT(obj);
        JS_TRACE_MONITOR(cx).useReservedObjects = JS_FALSE;
    }

    /*
     * If there's a call hook, invoke it to compute the hookData used by
     * debuggers that cooperate with the interpreter.
     */
    JSInterpreterHook hook = cx->debugHooks->callHook;
    if (hook) {
        newifp->hookData = hook(cx, &newifp->frame, JS_TRUE, 0,
                                cx->debugHooks->callHookData);
    } else {
        newifp->hookData = NULL;
    }

    // FIXME? we must count stack slots from caller's operand stack up to (but not including)
    // callee's, including missing arguments. Could we shift everything down to the caller's
    // fp->slots (where vars start) and avoid some of the complexity?
    return (fi.s.spdist - fp->down->script->nfixed) +
           ((fun->nargs > fp->argc) ? fun->nargs - fp->argc : 0) +
           script->nfixed;
}

JS_REQUIRES_STACK bool
js_RecordTree(JSContext* cx, JSTraceMonitor* tm, Fragment* f, jsbytecode* outer,
              uint32 globalShape, SlotList* globalSlots)
{
    JS_ASSERT(f->root == f);

    /* Make sure the global type map didn't change on us. */
    JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    if (!js_CheckGlobalObjectShape(cx, tm, globalObj)) {
        js_FlushJITCache(cx);
        return false;
    }

    AUDIT(recorderStarted);

    /* Try to find an unused peer fragment, or allocate a new one. */
    while (f->code() && f->peer)
        f = f->peer;
    if (f->code())
        f = getAnchor(&JS_TRACE_MONITOR(cx), f->root->ip, globalShape);

    if (!f) {
        js_FlushJITCache(cx);
        return false;
    }

    f->root = f;
    f->lirbuf = tm->lirbuf;

    if (f->lirbuf->outOMem()) {
        js_FlushJITCache(cx);
        debug_only_v(printf("Out of memory recording new tree, flushing cache.\n");)
        return false;
    }

    JS_ASSERT(!f->code() && !f->vmprivate);

    /* setup the VM-private treeInfo structure for this fragment */
    TreeInfo* ti = new (&gc) TreeInfo(f, globalShape, globalSlots);

    /* capture the coerced type of each active slot in the type map */
    ti->typeMap.captureTypes(cx, *globalSlots, 0/*callDepth*/);
    ti->nStackTypes = ti->typeMap.length() - globalSlots->length();

    /* Check for duplicate entry type maps.  This is always wrong and hints at trace explosion
       since we are trying to stabilize something without properly connecting peer edges. */
    #ifdef DEBUG
    TreeInfo* ti_other;
    for (Fragment* peer = getLoop(tm, f->root->ip, globalShape); peer != NULL; peer = peer->peer) {
        if (!peer->code() || peer == f)
            continue;
        ti_other = (TreeInfo*)peer->vmprivate;
        JS_ASSERT(ti_other);
        JS_ASSERT(!ti->typeMap.matches(ti_other->typeMap));
    }
    ti->treeFileName = cx->fp->script->filename;
    ti->treeLineNumber = js_FramePCToLineNumber(cx, cx->fp);
    ti->treePCOffset = FramePCOffset(cx->fp);
    #endif

    /* determine the native frame layout at the entry point */
    unsigned entryNativeStackSlots = ti->nStackTypes;
    JS_ASSERT(entryNativeStackSlots == js_NativeStackSlots(cx, 0/*callDepth*/));
    ti->nativeStackBase = (entryNativeStackSlots -
            (cx->fp->regs->sp - StackBase(cx->fp))) * sizeof(double);
    ti->maxNativeStackSlots = entryNativeStackSlots;
    ti->maxCallDepth = 0;
    ti->script = cx->fp->script;

    /* recording primary trace */
    if (!js_StartRecorder(cx, NULL, f, ti,
                          ti->nStackTypes,
                          ti->globalSlots->length(),
                          ti->typeMap.data(), NULL, outer)) {
        return false;
    }

    return true;
}

JS_REQUIRES_STACK static inline bool
isSlotUndemotable(JSContext* cx, TreeInfo* ti, unsigned slot)
{
    if (slot < ti->nStackTypes)
        return oracle.isStackSlotUndemotable(cx, slot);

    uint16* gslots = ti->globalSlots->data();
    return oracle.isGlobalSlotUndemotable(cx, gslots[slot - ti->nStackTypes]);
}

JS_REQUIRES_STACK static bool
js_AttemptToStabilizeTree(JSContext* cx, VMSideExit* exit, jsbytecode* outer)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    Fragment* from = exit->from->root;
    TreeInfo* from_ti = (TreeInfo*)from->vmprivate;

    JS_ASSERT(exit->from->root->code());

    /* Make sure any doubles are not accidentally undemoted */
    uint8* m = getStackTypeMap(exit);
    for (unsigned i = 0; i < exit->numStackSlots; i++) {
        if (m[i] == JSVAL_DOUBLE)
            oracle.markStackSlotUndemotable(cx, i);
    }
    m = getGlobalTypeMap(exit);
    for (unsigned i = 0; i < exit->numGlobalSlots; i++) {
        if (m[i] == JSVAL_DOUBLE)
            oracle.markGlobalSlotUndemotable(cx, from_ti->globalSlots->data()[i]);
    }

    /* If this exit does not have enough globals, there might exist a peer with more globals that we
     * can join to.
     */
    bool bound = false;
    for (Fragment* f = from->first; f != NULL; f = f->peer) {
        if (!f->code())
            continue;
        TreeInfo* ti = (TreeInfo*)f->vmprivate;
        JS_ASSERT(exit->numStackSlots == ti->nStackTypes);
        /* Check the minimum number of slots that need to be compared. */
        unsigned checkSlots = JS_MIN(exit->numStackSlots + exit->numGlobalSlots, ti->typeMap.length());
        m = getFullTypeMap(exit);
        uint8* m2 = ti->typeMap.data();
        /* Analyze the exit typemap against the peer typemap.
         * Two conditions are important:
         * 1) Typemaps are identical: these peers can be attached.
         * 2) Typemaps do not match, but only contain I->D mismatches.
         *    In this case, the original tree must be trashed because it
         *    will never connect to any peer.
         */
        bool matched = true;
        bool undemote = false;
        for (uint32 i = 0; i < checkSlots; i++) {
            /* If the types are equal we're okay. */
            if (m[i] == m2[i])
                continue;
            matched = false;
            /* If there's an I->D that cannot be resolved, flag it.
             * Otherwise, break and go to the next peer.
             */
            if (m[i] == JSVAL_INT && m2[i] == JSVAL_DOUBLE && isSlotUndemotable(cx, ti, i)) {
                undemote = true;
            } else {
                undemote = false;
                break;
            }
        }
        if (matched) {
            JS_ASSERT(from_ti->globalSlots == ti->globalSlots);
            JS_ASSERT(from_ti->nStackTypes == ti->nStackTypes);
            /* Capture missing globals on both trees and link the fragments together. */
            if (from != f) {
                ti->dependentTrees.addUnique(from);
                from_ti->linkedTrees.addUnique(f);
            }
            if (ti->nGlobalTypes() < ti->globalSlots->length())
                specializeTreesToMissingGlobals(cx, ti);
            exit->target = f;
            tm->fragmento->assm()->patch(exit);
            /* Now erase this exit from the unstable exit list. */
            UnstableExit** tail = &from_ti->unstableExits;
            for (UnstableExit* uexit = from_ti->unstableExits; uexit != NULL; uexit = uexit->next) {
                if (uexit->exit == exit) {
                    *tail = uexit->next;
                    delete uexit;
                    bound = true;
                    break;
                }
                tail = &uexit->next;
            }
            JS_ASSERT(bound);
            debug_only_v(js_DumpPeerStability(tm, f->ip, from_ti->globalShape);)
            break;
        } else if (undemote) {
            /* The original tree is unconnectable, so trash it. */
            js_TrashTree(cx, f);
            /* We shouldn't attempt to record now, since we'll hit a duplicate. */
            return false;
        }
    }
    if (bound)
        return false;

    return js_RecordTree(cx, tm, from->first, outer, from_ti->globalShape, from_ti->globalSlots);
}

static JS_REQUIRES_STACK bool
js_AttemptToExtendTree(JSContext* cx, VMSideExit* anchor, VMSideExit* exitedFrom, jsbytecode* outer)
{
    Fragment* f = anchor->from->root;
    JS_ASSERT(f->vmprivate);
    TreeInfo* ti = (TreeInfo*)f->vmprivate;

    /* Don't grow trees above a certain size to avoid code explosion due to tail duplication. */
    if (ti->branchCount >= MAX_BRANCHES)
        return false;

    Fragment* c;
    if (!(c = anchor->target)) {
        c = JS_TRACE_MONITOR(cx).fragmento->createBranch(anchor, cx->fp->regs->pc);
        c->spawnedFrom = anchor;
        c->parent = f;
        anchor->target = c;
        c->root = f;
    }

    debug_only_v(printf("trying to attach another branch to the tree (hits = %d)\n", c->hits());)

    int32_t& hits = c->hits();
    if (outer || hits++ >= HOTEXIT && hits <= HOTEXIT+MAXEXIT) {
        /* start tracing secondary trace from this point */
        c->lirbuf = f->lirbuf;
        unsigned stackSlots;
        unsigned ngslots;
        uint8* typeMap;
        TypeMap fullMap;
        if (exitedFrom == NULL) {
            /* If we are coming straight from a simple side exit, just use that exit's type map
               as starting point. */
            ngslots = anchor->numGlobalSlots;
            stackSlots = anchor->numStackSlots;
            typeMap = getFullTypeMap(anchor);
        } else {
            /* If we side-exited on a loop exit and continue on a nesting guard, the nesting
               guard (anchor) has the type information for everything below the current scope,
               and the actual guard we exited from has the types for everything in the current
               scope (and whatever it inlined). We have to merge those maps here. */
            VMSideExit* e1 = anchor;
            VMSideExit* e2 = exitedFrom;
            fullMap.add(getStackTypeMap(e1), e1->numStackSlotsBelowCurrentFrame);
            fullMap.add(getStackTypeMap(e2), e2->numStackSlots);
            stackSlots = fullMap.length();
            fullMap.add(getGlobalTypeMap(e2), e2->numGlobalSlots);
            ngslots = e2->numGlobalSlots;
            typeMap = fullMap.data();
        }
        return js_StartRecorder(cx, anchor, c, (TreeInfo*)f->vmprivate, stackSlots,
                                ngslots, typeMap, exitedFrom, outer);
    }
    return false;
}

static JS_REQUIRES_STACK VMSideExit*
js_ExecuteTree(JSContext* cx, Fragment* f, uintN& inlineCallCount,
               VMSideExit** innermostNestedGuardp);

static JS_REQUIRES_STACK bool
js_CloseLoop(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    Fragmento* fragmento = tm->fragmento;
    TraceRecorder* r = tm->recorder;
    JS_ASSERT(fragmento && r);
    bool walkedOutOfLoop = r->walkedOutOfLoop();

    if (fragmento->assm()->error()) {
        js_AbortRecording(cx, "Error during recording");
        return false;
    }

    bool demote = false;
    Fragment* f = r->getFragment();
    TreeInfo* ti = r->getTreeInfo();
    uint32 globalShape = ti->globalShape;
    SlotList* globalSlots = ti->globalSlots;
    r->closeLoop(tm, demote);

    /*
     * If js_DeleteRecorder flushed the code cache, we can't rely on f any more.
     */
    if (!js_DeleteRecorder(cx))
        return false;

    /*
     * If we just walked out of a thin loop, we can't immediately start the
     * compiler again here since we didn't return to the loop header.
     */
    if (demote && !walkedOutOfLoop)
        return js_RecordTree(cx, tm, f, NULL, globalShape, globalSlots);
    return false;
}

JS_REQUIRES_STACK bool
js_RecordLoopEdge(JSContext* cx, TraceRecorder* r, uintN& inlineCallCount)
{
#ifdef JS_THREADSAFE
    if (OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->title.ownercx != cx) {
        js_AbortRecording(cx, "Global object not owned by this context");
        return false; /* we stay away from shared global objects */
    }
#endif
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    TreeInfo* ti = r->getTreeInfo();
    /* Process deep abort requests. */
    if (r->wasDeepAborted()) {
        js_AbortRecording(cx, "deep abort requested");
        return false;
    }
    /* If we hit our own loop header, close the loop and compile the trace. */
    if (r->isLoopHeader(cx))
        return js_CloseLoop(cx);
    /* does this branch go to an inner loop? */
    Fragment* f = getLoop(&JS_TRACE_MONITOR(cx), cx->fp->regs->pc, ti->globalShape);
    if (nesting_enabled && f) {

        /* Cannot handle treecalls with callDepth > 0 and argc > nargs, see bug 480244. */
        if (r->getCallDepth() > 0 && 
            cx->fp->argc > cx->fp->fun->nargs) {
            js_AbortRecording(cx, "Can't call inner tree with extra args in pending frame");
            return false;
        }

        /* Make sure inner tree call will not run into an out-of-memory condition. */
        if (tm->reservedDoublePoolPtr < (tm->reservedDoublePool + MAX_NATIVE_STACK_SLOTS) &&
            !js_ReplenishReservedPool(cx, tm)) {
            js_AbortRecording(cx, "Couldn't call inner tree (out of memory)");
            return false;
        }

        /* Make sure the shape of the global object still matches (this might flush
           the JIT cache). */
        JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
        uint32 globalShape = -1;
        SlotList* globalSlots = NULL;
        if (!js_CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots)) {
            js_AbortRecording(cx, "Couldn't call inner tree (prep failed)");
            return false;
        }

        debug_only_v(printf("Looking for type-compatible peer (%s:%d@%d)\n",
                            cx->fp->script->filename,
                            js_FramePCToLineNumber(cx, cx->fp),
                            FramePCOffset(cx->fp));)

        /* Find an acceptable peer, make sure our types fit. */
        Fragment* empty;
        bool success = false;

        f = r->findNestedCompatiblePeer(f, &empty);
        if (f && f->code())
            success = r->adjustCallerTypes(f);

        if (!success) {
            AUDIT(noCompatInnerTrees);

            jsbytecode* outer = (jsbytecode*)tm->recorder->getFragment()->root->ip;
            js_AbortRecording(cx, "No compatible inner tree");

            f = empty;
            if (!f) {
                f = getAnchor(tm, cx->fp->regs->pc, globalShape);
                if (!f) {
                    js_FlushJITCache(cx);
                    return false;
                }
            }
            return js_RecordTree(cx, tm, f, outer, globalShape, globalSlots);
        }

        r->prepareTreeCall(f);
        VMSideExit* innermostNestedGuard = NULL;
        VMSideExit* lr = js_ExecuteTree(cx, f, inlineCallCount, &innermostNestedGuard);
        if (!lr || r->wasDeepAborted()) {
            if (!lr)
                js_AbortRecording(cx, "Couldn't call inner tree");
            return false;
        }
        jsbytecode* outer = (jsbytecode*)tm->recorder->getFragment()->root->ip;
        switch (lr->exitType) {
          case LOOP_EXIT:
            /* If the inner tree exited on an unknown loop exit, grow the tree around it. */
            if (innermostNestedGuard) {
                js_AbortRecording(cx, "Inner tree took different side exit, abort current "
                                      "recording and grow nesting tree");
                return js_AttemptToExtendTree(cx, innermostNestedGuard, lr, outer);
            }
            /* emit a call to the inner tree and continue recording the outer tree trace */
            r->emitTreeCall(f, lr);
            return true;
        case UNSTABLE_LOOP_EXIT:
            /* abort recording so the inner loop can become type stable. */
            js_AbortRecording(cx, "Inner tree is trying to stabilize, abort outer recording");
            return js_AttemptToStabilizeTree(cx, lr, outer);
        case BRANCH_EXIT:
            /* abort recording the outer tree, extend the inner tree */
            js_AbortRecording(cx, "Inner tree is trying to grow, abort outer recording");
            return js_AttemptToExtendTree(cx, lr, NULL, outer);
        default:
            debug_only_v(printf("exit_type=%d\n", lr->exitType);)
            js_AbortRecording(cx, "Inner tree not suitable for calling");
            return false;
        }
    }

    /* not returning to our own loop header, not an inner loop we can call, abort trace */
    AUDIT(returnToDifferentLoopHeader);
    JS_ASSERT(!cx->fp->imacpc);
    debug_only_v(printf("loop edge to %d, header %d\n",
                 cx->fp->regs->pc - cx->fp->script->code,
                 (jsbytecode*)r->getFragment()->root->ip - cx->fp->script->code));
    js_AbortRecording(cx, "Loop edge does not return to header");
    return false;
}

static bool
js_IsEntryTypeCompatible(jsval* vp, uint8* m)
{
    unsigned tag = JSVAL_TAG(*vp);

    debug_only_v(printf("%c/%c ", tagChar[tag], typeChar[*m]);)

    switch (*m) {
      case JSVAL_INT:
        jsint i;
        if (JSVAL_IS_INT(*vp))
            return true;
        if ((tag == JSVAL_DOUBLE) && JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(*vp), i))
            return true;
        debug_only_v(printf("int != tag%u(value=%lu) ", tag, (unsigned long)*vp);)
        return false;
      case JSVAL_DOUBLE:
        if (JSVAL_IS_INT(*vp) || tag == JSVAL_DOUBLE)
            return true;
        debug_only_v(printf("double != tag%u ", tag);)
        return false;
      case JSVAL_BOOLEAN:
        if (tag == JSVAL_BOOLEAN)
            return true;
        debug_only_v(printf("bool != tag%u", tag);)
        return false;
      case JSVAL_STRING:
        if (tag == JSVAL_STRING)
            return true;
        debug_only_v(printf("string != tag%u", tag);)
        return false;
      case JSVAL_TNULL:
        return JSVAL_IS_NULL(*vp);
      default:
        JS_ASSERT(*m == JSVAL_OBJECT);
        if (tag == JSVAL_OBJECT && !JSVAL_IS_NULL(*vp))
            return true;
        debug_only_v(printf("object != tag%u", tag);)
        return false;
    }
}

JS_REQUIRES_STACK Fragment*
TraceRecorder::findNestedCompatiblePeer(Fragment* f, Fragment** empty)
{
    Fragment* demote;
    JSTraceMonitor* tm;
    unsigned max_demotes;

    if (empty)
        *empty = NULL;
    demote = NULL;

    tm = &JS_TRACE_MONITOR(cx);
    unsigned int ngslots = treeInfo->globalSlots->length();
    uint16* gslots = treeInfo->globalSlots->data();

    /* We keep a maximum tally - we want to select the peer most likely to work so we don't keep
     * recording.
     */
    max_demotes = 0;

    TreeInfo* ti;
    for (; f != NULL; f = f->peer) {
        if (!f->code()) {
            if (empty)
                *empty = f;
            continue;
        }

        unsigned demotes = 0;
        ti = (TreeInfo*)f->vmprivate;

        debug_only_v(printf("checking nested types %p: ", (void*)f);)

        if (ngslots > ti->nGlobalTypes())
            specializeTreesToMissingGlobals(cx, ti);

        uint8* m = ti->typeMap.data();

        FORALL_SLOTS(cx, ngslots, gslots, 0,
            debug_only_v(printf("%s%d=", vpname, vpnum);)
            if (!js_IsEntryTypeCompatible(vp, m))
                goto check_fail;
            if (*m == JSVAL_STRING && *vp == JSVAL_VOID)
                goto check_fail;
            if (*m == JSVAL_INT && !isPromoteInt(get(vp)))
                demotes++;
            m++;
        );
        JS_ASSERT(unsigned(m - ti->typeMap.data()) == ti->typeMap.length());

        debug_only_v(printf(" (demotes %d)\n", demotes);)

        if (demotes) {
            if (demotes > max_demotes) {
                demote = f;
                max_demotes = demotes;
            }
            continue;
        } else {
            return f;
        }

check_fail:
        debug_only_v(printf("\n"));
        continue;
    }

    if (demote)
        return demote;

   return NULL;
}

/**
 * Check if types are usable for trace execution.
 *
 * @param cx            Context.
 * @param ti            Tree info of peer we're testing.
 * @return              True if compatible (with or without demotions), false otherwise.
 */
static JS_REQUIRES_STACK bool
js_CheckEntryTypes(JSContext* cx, TreeInfo* ti)
{
    unsigned int ngslots = ti->globalSlots->length();
    uint16* gslots = ti->globalSlots->data();

    JS_ASSERT(ti->nStackTypes == js_NativeStackSlots(cx, 0));

    if (ngslots > ti->nGlobalTypes())
        specializeTreesToMissingGlobals(cx, ti);

    uint8* m = ti->typeMap.data();

    JS_ASSERT(ti->typeMap.length() == js_NativeStackSlots(cx, 0) + ngslots);
    JS_ASSERT(ti->typeMap.length() == ti->nStackTypes + ngslots);
    JS_ASSERT(ti->nGlobalTypes() == ngslots);
    FORALL_SLOTS(cx, ngslots, gslots, 0,
        debug_only_v(printf("%s%d=", vpname, vpnum);)
        JS_ASSERT(*m != 0xCD);
        if (!js_IsEntryTypeCompatible(vp, m))
            goto check_fail;
        m++;
    );
    JS_ASSERT(unsigned(m - ti->typeMap.data()) == ti->typeMap.length());

    debug_only_v(printf("\n");)
    return true;

check_fail:
    debug_only_v(printf("\n");)
    return false;
}

/**
 * Find an acceptable entry tree given a PC.
 *
 * @param cx            Context.
 * @param f             First peer fragment.
 * @param nodemote      If true, will try to find a peer that does not require demotion.
 * @out   count         Number of fragments consulted.
 */
static JS_REQUIRES_STACK Fragment*
js_FindVMCompatiblePeer(JSContext* cx, Fragment* f, uintN& count)
{
    count = 0;
    for (; f != NULL; f = f->peer) {
        if (f->vmprivate == NULL)
            continue;
        debug_only_v(printf("checking vm types %p (ip: %p): ", (void*)f, f->ip);)
        if (js_CheckEntryTypes(cx, (TreeInfo*)f->vmprivate))
            return f;
        ++count;
    }
    return NULL;
}

static void
LeaveTree(InterpState&, VMSideExit* lr);

/**
 * Executes a tree.
 */
static JS_REQUIRES_STACK VMSideExit*
js_ExecuteTree(JSContext* cx, Fragment* f, uintN& inlineCallCount,
               VMSideExit** innermostNestedGuardp)
{
    JS_ASSERT(f->code() && f->vmprivate);
    JS_ASSERT(cx->builtinStatus == 0);

    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    TreeInfo* ti = (TreeInfo*)f->vmprivate;
    unsigned ngslots = ti->globalSlots->length();
    uint16* gslots = ti->globalSlots->data();
    unsigned globalFrameSize = STOBJ_NSLOTS(globalObj);

    /* Make sure the global object is sane. */
    JS_ASSERT(!ngslots || (OBJ_SHAPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain)) == ti->globalShape));
    /* Make sure our caller replenished the double pool. */
    JS_ASSERT(tm->reservedDoublePoolPtr >= tm->reservedDoublePool + MAX_NATIVE_STACK_SLOTS);

    /* Reserve objects and stack space now, to make leaving the tree infallible. */
    if (!js_ReserveObjects(cx, MAX_CALL_STACK_ENTRIES))
        return NULL;

    /* Setup the interpreter state block, which is followed by the native global frame. */
    InterpState* state = (InterpState*)alloca(sizeof(InterpState) + (globalFrameSize+1)*sizeof(double));
    state->cx = cx;
    state->globalObj = globalObj;
    state->inlineCallCountp = &inlineCallCount;
    state->innermostNestedGuardp = innermostNestedGuardp;
    state->outermostTree = ti;
    state->lastTreeExitGuard = NULL;
    state->lastTreeCallGuard = NULL;
    state->rpAtLastTreeCall = NULL;

    /* Setup the native global frame. */
    double* global = (double*)(state+1);

    /* Setup the native stack frame. */
    double stack_buffer[MAX_NATIVE_STACK_SLOTS];
    state->stackBase = stack_buffer;
    state->sp = stack_buffer + (ti->nativeStackBase/sizeof(double));
    state->eos = stack_buffer + MAX_NATIVE_STACK_SLOTS;

    /* Setup the native call stack frame. */
    FrameInfo* callstack_buffer[MAX_CALL_STACK_ENTRIES];
    state->callstackBase = callstack_buffer;
    state->rp = callstack_buffer;
    state->eor = callstack_buffer + MAX_CALL_STACK_ENTRIES;

    void *reserve;
    state->stackMark = JS_ARENA_MARK(&cx->stackPool);
    JS_ARENA_ALLOCATE(reserve, &cx->stackPool, MAX_INTERP_STACK_BYTES);
    if (!reserve)
        return NULL;

#ifdef DEBUG
    memset(stack_buffer, 0xCD, sizeof(stack_buffer));
    memset(global, 0xCD, (globalFrameSize+1)*sizeof(double));
#endif

    debug_only(*(uint64*)&global[globalFrameSize] = 0xdeadbeefdeadbeefLL;)
    debug_only_v(printf("entering trace at %s:%u@%u, native stack slots: %u code: %p\n",
                        cx->fp->script->filename,
                        js_FramePCToLineNumber(cx, cx->fp),
                        FramePCOffset(cx->fp),
                        ti->maxNativeStackSlots,
                        f->code());)

    JS_ASSERT(ti->nGlobalTypes() == ngslots);

    if (ngslots)
        BuildNativeGlobalFrame(cx, ngslots, gslots, ti->globalTypeMap(), global);
    BuildNativeStackFrame(cx, 0/*callDepth*/, ti->typeMap.data(), stack_buffer);

    union { NIns *code; GuardRecord* (FASTCALL *func)(InterpState*, Fragment*); } u;
    u.code = f->code();

#ifdef EXECUTE_TREE_TIMER
    state->startTime = rdtsc();
#endif

    /* Set a flag that indicates to the runtime system that we are running in native code
       now and we don't want automatic GC to happen. Instead we will get a silent failure,
       which will cause a trace exit at which point the interpreter re-tries the operation
       and eventually triggers the GC. */
    JS_ASSERT(!tm->onTrace);
    tm->onTrace = true;
    cx->interpState = state;

    debug_only(fflush(NULL);)
    GuardRecord* rec;
#if defined(JS_NO_FASTCALL) && defined(NANOJIT_IA32)
    SIMULATE_FASTCALL(rec, state, NULL, u.func);
#else
    rec = u.func(state, NULL);
#endif
    VMSideExit* lr = (VMSideExit*)rec->exit;

    AUDIT(traceTriggered);

    JS_ASSERT(lr->exitType != LOOP_EXIT || !lr->calldepth);
    tm->onTrace = false;
    LeaveTree(*state, lr);
    return state->innermost;
}

static JS_FORCES_STACK void
LeaveTree(InterpState& state, VMSideExit* lr)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JSContext* cx = state.cx;
    FrameInfo** callstack = state.callstackBase;
    double* stack = state.stackBase;

    /* Except if we find that this is a nested bailout, the guard the call returned is the
       one we have to use to adjust pc and sp. */
    VMSideExit* innermost = lr;

    /* While executing a tree we do not update state.sp and state.rp even if they grow. Instead,
       guards tell us by how much sp and rp should be incremented in case of a side exit. When
       calling a nested tree, however, we actively adjust sp and rp. If we have such frames
       from outer trees on the stack, then rp will have been adjusted. Before we can process
       the stack of the frames of the tree we directly exited from, we have to first work our
       way through the outer frames and generate interpreter frames for them. Once the call
       stack (rp) is empty, we can process the final frames (which again are not directly
       visible and only the guard we exited on will tells us about). */
    FrameInfo** rp = (FrameInfo**)state.rp;
    if (lr->exitType == NESTED_EXIT) {
        VMSideExit* nested = state.lastTreeCallGuard;
        if (!nested) {
            /* If lastTreeCallGuard is not set in state, we only have a single level of
               nesting in this exit, so lr itself is the innermost and outermost nested
               guard, and hence we set nested to lr. The calldepth of the innermost guard
               is not added to state.rp, so we do it here manually. For a nesting depth
               greater than 1 the CallTree builtin already added the innermost guard's
               calldepth to state.rpAtLastTreeCall. */
            nested = lr;
            rp += lr->calldepth;
        } else {
            /* During unwinding state.rp gets overwritten at every step and we restore
               it here to its state at the innermost nested guard. The builtin already
               added the calldepth of that innermost guard to rpAtLastTreeCall. */
            rp = (FrameInfo**)state.rpAtLastTreeCall;
        }
        innermost = state.lastTreeExitGuard;
        if (state.innermostNestedGuardp)
            *state.innermostNestedGuardp = nested;
        JS_ASSERT(nested);
        JS_ASSERT(nested->exitType == NESTED_EXIT);
        JS_ASSERT(state.lastTreeExitGuard);
        JS_ASSERT(state.lastTreeExitGuard->exitType != NESTED_EXIT);
    }

    int32_t bs = cx->builtinStatus;
    cx->builtinStatus = 0;
    bool bailed = innermost->exitType == STATUS_EXIT && (bs & JSBUILTIN_BAILED);
    if (bailed) {
        /*
         * Deep-bail case.
         *
         * A _FAIL native already called LeaveTree. We already reconstructed
         * the interpreter stack, in pre-call state, with pc pointing to the
         * CALL/APPLY op, for correctness. Then we continued in native code.
         */
        if (!(bs & JSBUILTIN_ERROR)) {
            /*
             * The native succeeded (no exception or error). After it returned, the
             * trace stored the return value (at the top of the native stack) and
             * then immediately flunked the guard on cx->builtinStatus.
             *
             * Now LeaveTree has been called again from the tail of
             * js_ExecuteTree. We are about to return to the interpreter. Adjust
             * the top stack frame to resume on the next op.
             */
            JS_ASSERT(*cx->fp->regs->pc == JSOP_CALL || *cx->fp->regs->pc == JSOP_APPLY);
            uintN argc = GET_ARGC(cx->fp->regs->pc);
            cx->fp->regs->pc += JSOP_CALL_LENGTH;
            cx->fp->regs->sp -= argc + 1;
            JS_ASSERT_IF(!cx->fp->imacpc,
                         cx->fp->slots + cx->fp->script->nfixed +
                         js_ReconstructStackDepth(cx, cx->fp->script, cx->fp->regs->pc) ==
                         cx->fp->regs->sp);

            /*
             * The return value was not available when we reconstructed the stack,
             * but we have it now. Box it.
             */
            uint8* typeMap = getStackTypeMap(innermost);
            NativeToValue(cx,
                          cx->fp->regs->sp[-1],
                          typeMap[innermost->numStackSlots - 1],
                          (jsdouble *) state.sp + innermost->sp_adj / sizeof(jsdouble) - 1);
        }
        JS_TRACE_MONITOR(cx).prohibitRecording = false;
        return;
    }

    JS_ARENA_RELEASE(&cx->stackPool, state.stackMark);
    while (callstack < rp) {
        /* Synthesize a stack frame and write out the values in it using the type map pointer
           on the native call stack. */
        js_SynthesizeFrame(cx, **callstack);
        int slots = FlushNativeStackFrame(cx, 1/*callDepth*/, (uint8*)(*callstack+1), stack, cx->fp);
#ifdef DEBUG
        JSStackFrame* fp = cx->fp;
        debug_only_v(printf("synthesized deep frame for %s:%u@%u, slots=%d\n",
                            fp->script->filename,
                            js_FramePCToLineNumber(cx, fp),
                            FramePCOffset(fp),
                            slots);)
#endif
        /* Keep track of the additional frames we put on the interpreter stack and the native
           stack slots we consumed. */
        ++*state.inlineCallCountp;
        ++callstack;
        stack += slots;
    }

    /* We already synthesized the frames around the innermost guard. Here we just deal
       with additional frames inside the tree we are bailing out from. */
    JS_ASSERT(rp == callstack);
    unsigned calldepth = innermost->calldepth;
    unsigned calldepth_slots = 0;
    for (unsigned n = 0; n < calldepth; ++n) {
        calldepth_slots += js_SynthesizeFrame(cx, *callstack[n]);
        ++*state.inlineCallCountp;
#ifdef DEBUG
        JSStackFrame* fp = cx->fp;
        debug_only_v(printf("synthesized shallow frame for %s:%u@%u\n",
                            fp->script->filename, js_FramePCToLineNumber(cx, fp),
                            FramePCOffset(fp));)
#endif
    }

    /* Adjust sp and pc relative to the tree we exited from (not the tree we entered into).
       These are our final values for sp and pc since js_SynthesizeFrame has already taken
       care of all frames in between. But first we recover fp->blockChain, which comes from
       the side exit struct. */
    JSStackFrame* fp = cx->fp;

    fp->blockChain = innermost->block;

    /* If we are not exiting from an inlined frame the state->sp is spbase, otherwise spbase
       is whatever slots frames around us consume. */
    fp->regs->pc = innermost->pc;
    fp->imacpc = innermost->imacpc;
    fp->regs->sp = StackBase(fp) + (innermost->sp_adj / sizeof(double)) - calldepth_slots;
    JS_ASSERT_IF(!fp->imacpc,
                 fp->slots + fp->script->nfixed +
                 js_ReconstructStackDepth(cx, fp->script, fp->regs->pc) == fp->regs->sp);

#ifdef EXECUTE_TREE_TIMER
    uint64 cycles = rdtsc() - state.startTime;
#elif defined(JS_JIT_SPEW)
    uint64 cycles = 0;
#endif

    debug_only_v(printf("leaving trace at %s:%u@%u, op=%s, lr=%p, exitType=%d, sp=%d, "
                        "calldepth=%d, cycles=%llu\n",
                        fp->script->filename,
                        js_FramePCToLineNumber(cx, fp),
                        FramePCOffset(fp),
                        js_CodeName[fp->imacpc ? *fp->imacpc : *fp->regs->pc],
                        (void*)lr,
                        lr->exitType,
                        fp->regs->sp - StackBase(fp),
                        calldepth,
                        cycles));

    /* If this trace is part of a tree, later branches might have added additional globals for
       which we don't have any type information available in the side exit. We merge in this
       information from the entry type-map. See also comment in the constructor of TraceRecorder
       why this is always safe to do. */
    TreeInfo* outermostTree = state.outermostTree;
    uint16* gslots = outermostTree->globalSlots->data();
    unsigned ngslots = outermostTree->globalSlots->length();
    JS_ASSERT(ngslots == outermostTree->nGlobalTypes());
    uint8* globalTypeMap;

    /* Are there enough globals? This is the ideal fast path. */
    if (innermost->numGlobalSlots == ngslots) {
        globalTypeMap = getGlobalTypeMap(innermost);
    /* Otherwise, merge the typemap of the innermost entry and exit together.  This should always
       work because it is invalid for nested trees or linked trees to have incompatible types.
       Thus, whenever a new global type is lazily added into a tree, all dependent and linked
       trees are immediately specialized (see bug 476653). */
    } else {
        TreeInfo* ti = (TreeInfo*)innermost->from->root->vmprivate;
        JS_ASSERT(ti->nGlobalTypes() == ngslots);
        JS_ASSERT(ti->nGlobalTypes() > innermost->numGlobalSlots);
        globalTypeMap = (uint8*)alloca(ngslots * sizeof(uint8));
        memcpy(globalTypeMap, getGlobalTypeMap(innermost), innermost->numGlobalSlots);
        memcpy(globalTypeMap + innermost->numGlobalSlots,
               ti->globalTypeMap() + innermost->numGlobalSlots,
               ti->nGlobalTypes() - innermost->numGlobalSlots);
    }

    /* write back interned globals */
    double* global = (double*)(&state + 1);
    FlushNativeGlobalFrame(cx, ngslots, gslots, globalTypeMap, global);
    JS_ASSERT(*(uint64*)&global[STOBJ_NSLOTS(state.globalObj)] == 0xdeadbeefdeadbeefLL);

    /* write back native stack frame */
#ifdef DEBUG
    int slots =
#endif
        FlushNativeStackFrame(cx, innermost->calldepth,
                              getStackTypeMap(innermost),
                              stack, NULL);
    JS_ASSERT(unsigned(slots) == innermost->numStackSlots);

#ifdef DEBUG
    // Verify that our state restoration worked.
    for (JSStackFrame* fp = cx->fp; fp; fp = fp->down) {
        JS_ASSERT(!fp->callee || JSVAL_IS_OBJECT(fp->argv[-1]));
        JS_ASSERT_IF(fp->callee && fp->thisp != JSVAL_TO_OBJECT(fp->argv[-1]),
                     !(fp->flags & JSFRAME_COMPUTED_THIS) && !fp->thisp);
    }
#endif
#ifdef JS_JIT_SPEW
    if (innermost->exitType != TIMEOUT_EXIT)
        AUDIT(sideExitIntoInterpreter);
    else
        AUDIT(timeoutIntoInterpreter);
#endif

    state.innermost = innermost;
}

JS_REQUIRES_STACK bool
js_MonitorLoopEdge(JSContext* cx, uintN& inlineCallCount)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Is the recorder currently active? */
    if (tm->recorder) {
        jsbytecode* innerLoopHeaderPC = cx->fp->regs->pc;

        if (js_RecordLoopEdge(cx, tm->recorder, inlineCallCount))
            return true;

        /*
         * js_RecordLoopEdge will invoke an inner tree if we have a matching one. If we
         * arrive here, that tree didn't run to completion and instead we mis-matched
         * or the inner tree took a side exit other than the loop exit. We are thus
         * no longer guaranteed to be parked on the same loop header js_MonitorLoopEdge
         * was called for. In fact, this might not even be a loop header at all. Hence
         * if the program counter no longer hovers over the inner loop header, return to
         * the interpreter and do not attempt to trigger or record a new tree at this
         * location.
         */
        if (innerLoopHeaderPC != cx->fp->regs->pc)
            return false;
    }
    JS_ASSERT(!tm->recorder);

    /* Check the pool of reserved doubles (this might trigger a GC). */
    if (tm->reservedDoublePoolPtr < (tm->reservedDoublePool + MAX_NATIVE_STACK_SLOTS) &&
        !js_ReplenishReservedPool(cx, tm)) {
        return false; /* Out of memory, don't try to record now. */
    }

    /* Make sure the shape of the global object still matches (this might flush the JIT cache). */
    JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    uint32 globalShape = -1;
    SlotList* globalSlots = NULL;

    if (!js_CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots))
        js_FlushJITCache(cx);

    /* Do not enter the JIT code with a pending operation callback. */
    if (cx->operationCallbackFlag)
        return false;
    
    jsbytecode* pc = cx->fp->regs->pc;

    Fragment* f = getLoop(tm, pc, globalShape);
    if (!f)
        f = getAnchor(tm, pc, globalShape);

    if (!f) {
        js_FlushJITCache(cx);
        return false;
    }

    /* If we have no code in the anchor and no peers, we definitively won't be able to
       activate any trees so, start compiling. */
    if (!f->code() && !f->peer) {
    record:
        if (++f->hits() < HOTLOOP)
            return false;
        /* We can give RecordTree the root peer. If that peer is already taken, it will
           walk the peer list and find us a free slot or allocate a new tree if needed. */
        return js_RecordTree(cx, tm, f->first, NULL, globalShape, globalSlots);
    }

    debug_only_v(printf("Looking for compat peer %d@%d, from %p (ip: %p)\n",
                        js_FramePCToLineNumber(cx, cx->fp),
                        FramePCOffset(cx->fp), (void*)f, f->ip);)

    uintN count;
    Fragment* match = js_FindVMCompatiblePeer(cx, f, count);
    if (!match) {
        if (count < MAXPEERS)
            goto record;
        /* If we hit the max peers ceiling, don't try to lookup fragments all the time. Thats
           expensive. This must be a rather type-unstable loop. */
        debug_only_v(printf("Blacklisted: too many peer trees.\n");)
        js_Blacklist(f->root);
        return false;
    }

    VMSideExit* lr = NULL;
    VMSideExit* innermostNestedGuard = NULL;

    lr = js_ExecuteTree(cx, match, inlineCallCount, &innermostNestedGuard);
    if (!lr)
        return false;

    /* If we exit on a branch, or on a tree call guard, try to grow the inner tree (in case
       of a branch exit), or the tree nested around the tree we exited from (in case of the
       tree call guard). */
    switch (lr->exitType) {
      case UNSTABLE_LOOP_EXIT:
        return js_AttemptToStabilizeTree(cx, lr, NULL);
      case BRANCH_EXIT:
      case CASE_EXIT:
        return js_AttemptToExtendTree(cx, lr, NULL, NULL);
      case LOOP_EXIT:
        if (innermostNestedGuard)
            return js_AttemptToExtendTree(cx, innermostNestedGuard, lr, NULL);
        return false;
      default:
        /* No, this was an unusual exit (i.e. out of memory/GC), so just resume interpretation. */
        return false;
    }
}

JS_REQUIRES_STACK JSMonitorRecordingStatus
TraceRecorder::monitorRecording(JSContext* cx, TraceRecorder* tr, JSOp op)
{
    if (tr->lirbuf->outOMem()) {
        js_AbortRecording(cx, "no more LIR memory");
        js_FlushJITCache(cx);
        return JSMRS_STOP;
    }

    /* Process deepAbort() requests now. */
    if (tr->wasDeepAborted()) {
        js_AbortRecording(cx, "deep abort requested");
        return JSMRS_STOP;
    }

    if (tr->walkedOutOfLoop()) {
        if (!js_CloseLoop(cx))
            return JSMRS_STOP;
    } else {
        /* Clear one-shot state used to communicate between record_JSOP_CALL and post-
           opcode-case-guts record hook (record_FastNativeCallComplete). */
        tr->pendingTraceableNative = NULL;

        jsbytecode* pc = cx->fp->regs->pc;

        /* If we hit a break, end the loop and generate an always taken loop exit guard.
           For other downward gotos (like if/else) continue recording. */
        if (*pc == JSOP_GOTO || *pc == JSOP_GOTOX) {
            jssrcnote* sn = js_GetSrcNote(cx->fp->script, pc);
            if (sn && SN_TYPE(sn) == SRC_BREAK) {
                AUDIT(breakLoopExits);
                tr->endLoop(&JS_TRACE_MONITOR(cx));
                js_DeleteRecorder(cx);
                return JSMRS_STOP; /* done recording */
            }
        }

        /* An explicit return from callDepth 0 should end the loop, not abort it. */
        if (*pc == JSOP_RETURN && tr->callDepth == 0) {
            AUDIT(returnLoopExits);
            tr->endLoop(&JS_TRACE_MONITOR(cx));
            js_DeleteRecorder(cx);
            return JSMRS_STOP; /* done recording */
        }

#ifdef NANOJIT_IA32
        /* Handle tableswitches specially -- prepare a jump table if needed. */
        if (*pc == JSOP_TABLESWITCH || *pc == JSOP_TABLESWITCHX) {
            LIns* guardIns = tr->tableswitch();
            if (guardIns) {
                tr->fragment->lastIns = guardIns;
                tr->compile(&JS_TRACE_MONITOR(cx));
                js_DeleteRecorder(cx);
                return JSMRS_STOP;
            }
        }
#endif
    }

    /* If op is not a break or a return from a loop, continue recording and follow the
       trace. We check for imacro-calling bytecodes inside each switch case to resolve
       the if (JSOP_IS_IMACOP(x)) conditions at compile time. */

    bool flag;
    switch (op) {
      default: goto abort_recording;
# define OPDEF(x,val,name,token,length,nuses,ndefs,prec,format)               \
      case x:                                                                 \
        debug_only_v(                                                         \
            js_Disassemble1(cx, cx->fp->script, cx->fp->regs->pc,             \
                            (cx->fp->imacpc)                                  \
                            ? 0                                               \
                            : cx->fp->regs->pc - cx->fp->script->code,        \
                            !cx->fp->imacpc, stdout);)                        \
        flag = tr->record_##x();                                              \
        if (JSOP_IS_IMACOP(x))                                                \
            goto imacro;                                                      \
        break;
# include "jsopcode.tbl"
# undef OPDEF
    }

    if (flag)
        return JSMRS_CONTINUE;
    goto abort_recording;

  imacro:
    /* We save macro-generated code size also via bool TraceRecorder::record_JSOP_*
       return type, instead of a three-state: OK, ABORTED, IMACRO_STARTED. But the
       price of this is the JSFRAME_IMACRO_START frame flag. We need one more bit
       to detect that TraceRecorder::call_imacro was invoked by the record_JSOP_
       method. */
    if (flag)
        return JSMRS_CONTINUE;
    if (cx->fp->flags & JSFRAME_IMACRO_START) {
        cx->fp->flags &= ~JSFRAME_IMACRO_START;
        return JSMRS_IMACRO;
    }

  abort_recording:
    js_AbortRecording(cx, js_CodeName[op]);
    return JSMRS_STOP;
}

JS_REQUIRES_STACK void
js_AbortRecording(JSContext* cx, const char* reason)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(tm->recorder != NULL);
    AUDIT(recorderAborted);

    /* Abort the trace and blacklist its starting point. */
    Fragment* f = tm->recorder->getFragment();
    if (!f) {
        js_DeleteRecorder(cx);
        return;
    }
    JS_ASSERT(!f->vmprivate);

#ifdef DEBUG
    TreeInfo* ti = tm->recorder->getTreeInfo();
    debug_only_a(printf("Abort recording of tree %s:%d@%d at %s:%d@%d: %s.\n",
                        ti->treeFileName,
                        ti->treeLineNumber,
                        ti->treePCOffset,
                        cx->fp->script->filename,
                        js_FramePCToLineNumber(cx, cx->fp),
                        FramePCOffset(cx->fp),
                        reason);)
#endif

    js_Backoff(f->root, cx->fp->regs->pc);

    /*
     * If js_DeleteRecorder flushed the code cache, we can't rely on f any more.
     */
    if (!js_DeleteRecorder(cx))
        return;

    /*
     * If this is the primary trace and we didn't succeed compiling, trash the
     * TreeInfo object.
     */
    if (!f->code() && (f->root == f))
        js_TrashTree(cx, f);
}

#if defined NANOJIT_IA32
static bool
js_CheckForSSE2()
{
    int features = 0;
#if defined _MSC_VER
    __asm
    {
        pushad
        mov eax, 1
        cpuid
        mov features, edx
        popad
    }
#elif defined __GNUC__
    asm("xchg %%esi, %%ebx\n" /* we can't clobber ebx on gcc (PIC register) */
        "mov $0x01, %%eax\n"
        "cpuid\n"
        "mov %%edx, %0\n"
        "xchg %%esi, %%ebx\n"
        : "=m" (features)
        : /* We have no inputs */
        : "%eax", "%esi", "%ecx", "%edx"
       );
#elif defined __SUNPRO_C || defined __SUNPRO_CC
    asm("push %%ebx\n"
        "mov $0x01, %%eax\n"
        "cpuid\n"
        "pop %%ebx\n"
        : "=d" (features)
        : /* We have no inputs */
        : "%eax", "%ecx"
       );
#endif
    return (features & (1<<26)) != 0;
}
#endif

#if defined(NANOJIT_ARM)

#if defined(_MSC_VER) && defined(WINCE)

// these come in from jswince.asm
extern "C" int js_arm_try_armv6t2_op();
extern "C" int js_arm_try_vfp_op();

static bool
js_arm_check_armv6t2() {
    bool ret = false;
    __try {
        js_arm_try_armv6t2_op();
        ret = true;
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        ret = false;
    }
    return ret;
}

static bool
js_arm_check_vfp() {
    bool ret = false;
    __try {
        js_arm_try_vfp_op();
        ret = true;
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        ret = false;
    }
    return ret;
}

#elif defined(__GNUC__) && defined(AVMPLUS_LINUX)

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <elf.h>

static bool arm_has_v7 = false;
static bool arm_has_v6 = false;
static bool arm_has_vfp = false;
static bool arm_has_neon = false;
static bool arm_has_iwmmxt = false;
static bool arm_tests_initialized = false;

static void
arm_read_auxv() {
    int fd;
    Elf32_auxv_t aux;

    fd = open("/proc/self/auxv", O_RDONLY);
    if (fd > 0) {
        while (read(fd, &aux, sizeof(Elf32_auxv_t))) {
            if (aux.a_type == AT_HWCAP) {
                uint32_t hwcap = aux.a_un.a_val;
                if (getenv("ARM_FORCE_HWCAP"))
                    hwcap = strtoul(getenv("ARM_FORCE_HWCAP"), NULL, 0);
                // hardcode these values to avoid depending on specific versions
                // of the hwcap header, e.g. HWCAP_NEON
                arm_has_vfp = (hwcap & 64) != 0;
                arm_has_iwmmxt = (hwcap & 512) != 0;
                // this flag is only present on kernel 2.6.29
                arm_has_neon = (hwcap & 4096) != 0;
            } else if (aux.a_type == AT_PLATFORM) {
                const char *plat = (const char*) aux.a_un.a_val;
                if (getenv("ARM_FORCE_PLATFORM"))
                    plat = getenv("ARM_FORCE_PLATFORM");
                if (strncmp(plat, "v7l", 3) == 0) {
                    arm_has_v7 = true;
                    arm_has_v6 = true;
                } else if (strncmp(plat, "v6l", 3) == 0) {
                    arm_has_v6 = true;
                }
            }
        }
        close (fd);

        // if we don't have 2.6.29, we have to do this hack; set
        // the env var to trust HWCAP.
        if (!getenv("ARM_TRUST_HWCAP") && arm_has_v7)
            arm_has_neon = true;
    }

    arm_tests_initialized = true;
}

static bool
js_arm_check_armv6t2() {
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_has_v7;
}

static bool
js_arm_check_vfp() {
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_has_vfp;
}

#else
#warning Not sure how to check for armv6t2 and vfp on your platform, assuming neither present.
static bool
js_arm_check_armv6t2() { return false; }
static bool
js_arm_check_vfp() { return false; }
#endif

#endif /* NANOJIT_ARM */

void
js_InitJIT(JSTraceMonitor *tm)
{
    if (!did_we_check_processor_features) {
#if defined NANOJIT_IA32
        avmplus::AvmCore::config.use_cmov =
        avmplus::AvmCore::config.sse2 = js_CheckForSSE2();
#endif
#if defined NANOJIT_ARM
        avmplus::AvmCore::config.vfp = js_arm_check_vfp();
        avmplus::AvmCore::config.soft_float = !avmplus::AvmCore::config.vfp;
        avmplus::AvmCore::config.v6t2 = js_arm_check_armv6t2();
#endif
        did_we_check_processor_features = true;
    }

    if (!tm->fragmento) {
        JS_ASSERT(!tm->reservedDoublePool);
        Fragmento* fragmento = new (&gc) Fragmento(core, 24);
        verbose_only(fragmento->labels = new (&gc) LabelMap(core, NULL);)
        tm->fragmento = fragmento;
        tm->lirbuf = new (&gc) LirBuffer(fragmento, NULL);
#ifdef DEBUG
        tm->lirbuf->names = new (&gc) LirNameMap(&gc, NULL, tm->fragmento->labels);
#endif
        for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
            tm->globalStates[i].globalShape = -1;
            JS_ASSERT(!tm->globalStates[i].globalSlots);
            tm->globalStates[i].globalSlots = new (&gc) SlotList();
        }
        tm->reservedDoublePoolPtr = tm->reservedDoublePool = new jsval[MAX_NATIVE_STACK_SLOTS];
        memset(tm->vmfragments, 0, sizeof(tm->vmfragments));
    }
    if (!tm->reFragmento) {
        Fragmento* fragmento = new (&gc) Fragmento(core, 20);
        verbose_only(fragmento->labels = new (&gc) LabelMap(core, NULL);)
        tm->reFragmento = fragmento;
        tm->reLirBuf = new (&gc) LirBuffer(fragmento, NULL);
    }
#if !defined XP_WIN
    debug_only(memset(&jitstats, 0, sizeof(jitstats)));
#endif
}

void
js_FinishJIT(JSTraceMonitor *tm)
{
#ifdef JS_JIT_SPEW
    if (js_verboseStats && jitstats.recorderStarted) {
        printf("recorder: started(%llu), aborted(%llu), completed(%llu), different header(%llu), "
               "trees trashed(%llu), slot promoted(%llu), unstable loop variable(%llu), "
               "breaks(%llu), returns(%llu), unstableInnerCalls(%llu)\n",
               jitstats.recorderStarted, jitstats.recorderAborted, jitstats.traceCompleted,
               jitstats.returnToDifferentLoopHeader, jitstats.treesTrashed, jitstats.slotPromoted,
               jitstats.unstableLoopVariable, jitstats.breakLoopExits, jitstats.returnLoopExits,
               jitstats.noCompatInnerTrees);
        printf("monitor: triggered(%llu), exits(%llu), type mismatch(%llu), "
               "global mismatch(%llu)\n", jitstats.traceTriggered, jitstats.sideExitIntoInterpreter,
               jitstats.typeMapMismatchAtEntry, jitstats.globalShapeMismatchAtEntry);
    }
#endif
    if (tm->fragmento != NULL) {
        JS_ASSERT(tm->reservedDoublePool);
        verbose_only(delete tm->fragmento->labels;)
#ifdef DEBUG
        delete tm->lirbuf->names;
        tm->lirbuf->names = NULL;
#endif
        delete tm->lirbuf;
        tm->lirbuf = NULL;
        for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
            VMFragment* f = tm->vmfragments[i];
            while(f) {
                VMFragment* next = f->next;
                tm->fragmento->clearFragment(f);
                f = next;
            }
            tm->vmfragments[i] = NULL;
        }
        delete tm->fragmento;
        tm->fragmento = NULL;
        for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
            JS_ASSERT(tm->globalStates[i].globalSlots);
            delete tm->globalStates[i].globalSlots;
        }
        delete[] tm->reservedDoublePool;
        tm->reservedDoublePool = tm->reservedDoublePoolPtr = NULL;
    }
    if (tm->reFragmento != NULL) {
        delete tm->reLirBuf;
        verbose_only(delete tm->reFragmento->labels;)
        delete tm->reFragmento;
    }
}

void
TraceRecorder::pushAbortStack()
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    JS_ASSERT(tm->abortStack != this);

    nextRecorderToAbort = tm->abortStack;
    tm->abortStack = this;
}

void
TraceRecorder::popAbortStack()
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    JS_ASSERT(tm->abortStack == this);

    tm->abortStack = nextRecorderToAbort;
    nextRecorderToAbort = NULL;
}

void
js_FlushJITOracle(JSContext* cx)
{
    if (!TRACING_ENABLED(cx))
        return;
    oracle.clear();
}

JS_REQUIRES_STACK void
js_FlushScriptFragments(JSContext* cx, JSScript* script)
{
    if (!TRACING_ENABLED(cx))
        return;
    debug_only_v(printf("Flushing fragments for JSScript %p.\n", (void*)script);)
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
        for (VMFragment **f = &(tm->vmfragments[i]); *f; ) {
            /* Disable future use of any script-associated VMFragment.*/
            if (JS_UPTRDIFF((*f)->ip, script->code) < script->length) {
                debug_only_v(printf("Disconnecting VMFragment %p "
                                    "with ip %p, in range [%p,%p).\n",
                                    (void*)(*f), (*f)->ip, script->code,
                                    script->code + script->length));
                VMFragment* next = (*f)->next;
                if (tm->fragmento)
                    tm->fragmento->clearFragment(*f);
                *f = next;
            } else {
                f = &((*f)->next);
            }
        }
    }
}

JS_REQUIRES_STACK void
js_FlushJITCache(JSContext* cx)
{
    if (!TRACING_ENABLED(cx))
        return;
    debug_only_v(printf("Flushing cache.\n");)
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->recorder)
        js_AbortRecording(cx, "flush cache");
    TraceRecorder* tr;
    while ((tr = tm->abortStack) != NULL) {
        tr->removeFragmentoReferences();
        tr->deepAbort();
        tr->popAbortStack();
    }
    Fragmento* fragmento = tm->fragmento;
    if (fragmento) {
        fragmento->clearFrags();
#ifdef DEBUG
        JS_ASSERT(fragmento->labels);
        fragmento->labels->clear();
#endif
        tm->lirbuf->rewind();
        for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
            VMFragment* f = tm->vmfragments[i];
            while(f) {
                VMFragment* next = f->next;
                fragmento->clearFragment(f);
                f = next;
            }
            tm->vmfragments[i] = NULL;
        }
        for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
            tm->globalStates[i].globalShape = -1;
            tm->globalStates[i].globalSlots->clear();
        }
    }
}

JS_FORCES_STACK JS_FRIEND_API(void)
js_DeepBail(JSContext *cx)
{
    JS_ASSERT(JS_ON_TRACE(cx));

    /* It's a bug if a non-FAIL_STATUS builtin gets here. */
    JS_ASSERT(cx->bailExit);

    JS_TRACE_MONITOR(cx).onTrace = false;
    JS_TRACE_MONITOR(cx).prohibitRecording = true;
    LeaveTree(*cx->interpState, cx->bailExit);
#ifdef DEBUG
    cx->bailExit = NULL;
#endif
    cx->builtinStatus |= JSBUILTIN_BAILED;
}

JS_REQUIRES_STACK jsval&
TraceRecorder::argval(unsigned n) const
{
    JS_ASSERT(n < cx->fp->fun->nargs);
    return cx->fp->argv[n];
}

JS_REQUIRES_STACK jsval&
TraceRecorder::varval(unsigned n) const
{
    JS_ASSERT(n < cx->fp->script->nslots);
    return cx->fp->slots[n];
}

JS_REQUIRES_STACK jsval&
TraceRecorder::stackval(int n) const
{
    jsval* sp = cx->fp->regs->sp;
    return sp[n];
}

JS_REQUIRES_STACK LIns*
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

JS_REQUIRES_STACK bool
TraceRecorder::activeCallOrGlobalSlot(JSObject* obj, jsval*& vp)
{
    // Lookup a name in the scope chain, arriving at a property either in the
    // global object or some call object's fp->slots, and import that property
    // into the trace's native stack frame. This could theoretically do *lookup*
    // through the property cache, but there is little performance to be gained
    // by doing so since at trace-execution time the underlying object (call
    // object or global object) will not be consulted at all: the jsval*
    // returned from this function will map (in the tracker) to a LIns* directly
    // defining a slot in the trace's native stack.

    JS_ASSERT(obj != globalObj);

    JSAtom* atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    JSObject* obj2;
    JSProperty* prop;
    if (js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &obj2, &prop) < 0 || !prop)
        ABORT_TRACE("failed to find name in non-global scope chain");

    if (obj == globalObj) {
        JSScopeProperty* sprop = (JSScopeProperty*) prop;

        if (obj2 != obj) {
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            ABORT_TRACE("prototype property");
        }
        if (!isValidSlot(OBJ_SCOPE(obj), sprop)) {
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            return false;
        }
        if (!lazilyImportGlobalSlot(sprop->slot)) {
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            ABORT_TRACE("lazy import of global slot failed");
        }
        vp = &STOBJ_GET_SLOT(obj, sprop->slot);
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        return true;
    }

    if (wasDeepAborted())
        ABORT_TRACE("deep abort from property lookup");

    if (obj == obj2 && OBJ_GET_CLASS(cx, obj) == &js_CallClass) {
        JSStackFrame* cfp = (JSStackFrame*) JS_GetPrivate(cx, obj);
        if (cfp && FrameInRange(cx->fp, cfp, callDepth)) {
            JSScopeProperty* sprop = (JSScopeProperty*) prop;
            uintN slot = sprop->shortid;

            vp = NULL;
            if (sprop->getter == js_GetCallArg) {
                JS_ASSERT(slot < cfp->fun->nargs);
                vp = &cfp->argv[slot];
            } else if (sprop->getter == js_GetCallVar) {
                JS_ASSERT(slot < cfp->script->nslots);
                vp = &cfp->slots[slot];
            }
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            if (!vp)
                ABORT_TRACE("dynamic property of Call object");
            return true;
        }
    }

    OBJ_DROP_PROPERTY(cx, obj2, prop);
    ABORT_TRACE("fp->scopeChain is not global or active call object");
}

JS_REQUIRES_STACK LIns*
TraceRecorder::arg(unsigned n)
{
    return get(&argval(n));
}

JS_REQUIRES_STACK void
TraceRecorder::arg(unsigned n, LIns* i)
{
    set(&argval(n), i);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::var(unsigned n)
{
    return get(&varval(n));
}

JS_REQUIRES_STACK void
TraceRecorder::var(unsigned n, LIns* i)
{
    set(&varval(n), i);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::stack(int n)
{
    return get(&stackval(n));
}

JS_REQUIRES_STACK void
TraceRecorder::stack(int n, LIns* i)
{
    set(&stackval(n), i, n >= 0);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::alu(LOpcode v, jsdouble v0, jsdouble v1, LIns* s0, LIns* s1)
{
    if (v == LIR_fadd || v == LIR_fsub) {
        jsdouble r;
        if (v == LIR_fadd)
            r = v0 + v1;
        else
            r = v0 - v1;
        /*
         * Calculate the result of the addition for the current values. If the
         * value is not within the integer range, don't even try to demote
         * here.
         */
        if (!JSDOUBLE_IS_NEGZERO(r) && (jsint(r) == r) && isPromoteInt(s0) && isPromoteInt(s1)) {
            LIns* d0 = ::demote(lir, s0);
            LIns* d1 = ::demote(lir, s1);
            /*
             * If the inputs are constant, generate an integer constant for
             * this operation.
             */
            if (d0->isconst() && d1->isconst())
                return lir->ins1(LIR_i2f, lir->insImm(jsint(r)));
            /*
             * Speculatively generate code that will perform the addition over
             * the integer inputs as an integer addition/subtraction and exit
             * if that fails.
             */
            v = (LOpcode)((int)v & ~LIR64);
            LIns* result = lir->ins2(v, d0, d1);
            if (!result->isconst() && (!overflowSafe(d0) || !overflowSafe(d1)))
                lir->insGuard(LIR_xt, lir->ins1(LIR_ov, result), snapshot(OVERFLOW_EXIT));
            return lir->ins1(LIR_i2f, result);
        }
        /*
         * The result doesn't fit into the integer domain, so either generate
         * a floating point constant or a floating point operation.
         */
        if (s0->isconst() && s1->isconst())
            return lir->insImmf(r);
        return lir->ins2(v, s0, s1);
    }
    return lir->ins2(v, s0, s1);
}

LIns*
TraceRecorder::f2i(LIns* f)
{
    return lir->insCall(&js_DoubleToInt32_ci, &f);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::makeNumberInt32(LIns* f)
{
    JS_ASSERT(f->isQuad());
    LIns* x;
    if (!isPromote(f)) {
        x = f2i(f);
        guard(true, lir->ins2(LIR_feq, f, lir->ins1(LIR_i2f, x)), MISMATCH_EXIT);
    } else {
        x = ::demote(lir, f);
    }
    return x;
}

JS_REQUIRES_STACK LIns*
TraceRecorder::stringify(jsval& v)
{
    LIns* v_ins = get(&v);
    if (JSVAL_IS_STRING(v))
        return v_ins;

    LIns* args[] = { v_ins, cx_ins };
    const CallInfo* ci;
    if (JSVAL_IS_NUMBER(v)) {
        ci = &js_NumberToString_ci;
    } else if (JSVAL_TAG(v) == JSVAL_BOOLEAN) {
        ci = &js_BooleanOrUndefinedToString_ci;
    } else {
        /*
         * Callers must deal with non-primitive (non-null object) values by
         * calling an imacro. We don't try to guess about which imacro, with
         * what valueOf hint, here.
         */
        JS_ASSERT(JSVAL_IS_NULL(v));
        return INS_CONSTPTR(ATOM_TO_STRING(cx->runtime->atomState.nullAtom));
    }

    v_ins = lir->insCall(ci, args);
    guard(false, lir->ins_eq0(v_ins), OOM_EXIT);
    return v_ins;
}

JS_REQUIRES_STACK bool
TraceRecorder::call_imacro(jsbytecode* imacro)
{
    JSStackFrame* fp = cx->fp;
    JSFrameRegs* regs = fp->regs;

    if (!fp->imacpc) {
        fp->imacpc = regs->pc;
        fp->flags |= JSFRAME_IMACRO_START;
        regs->pc = imacro;
        atoms = COMMON_ATOMS_START(&cx->runtime->atomState);
    }
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::ifop()
{
    jsval& v = stackval(-1);
    LIns* v_ins = get(&v);
    bool cond;
    LIns* x;

    /* No need to guard if condition is constant. */
    if (v_ins->isconst() || v_ins->isconstq())
        return true;

    /* No need to guard if type strictly determines true or false value. */
    if (JSVAL_TAG(v) == JSVAL_OBJECT)
        return true;

    if (JSVAL_TAG(v) == JSVAL_BOOLEAN) {
        /* Test for boolean is true, negate later if we are testing for false. */
        cond = JSVAL_TO_PSEUDO_BOOLEAN(v) == JS_TRUE;
        x = lir->ins2i(LIR_eq, v_ins, 1);
    } else if (isNumber(v)) {
        jsdouble d = asNumber(v);
        cond = !JSDOUBLE_IS_NaN(d) && d;
        x = lir->ins2(LIR_and,
                      lir->ins2(LIR_feq, v_ins, v_ins),
                      lir->ins_eq0(lir->ins2(LIR_feq, v_ins, lir->insImmq(0))));
    } else if (JSVAL_IS_STRING(v)) {
        cond = JSSTRING_LENGTH(JSVAL_TO_STRING(v)) != 0;
        x = lir->ins2(LIR_piand,
                      lir->insLoad(LIR_ldp,
                                   v_ins,
                                   (int)offsetof(JSString, length)),
                      INS_CONSTPTR(reinterpret_cast<void *>(JSSTRING_LENGTH_MASK)));
    } else {
        JS_NOT_REACHED("ifop");
        return false;
    }
    flipIf(cx->fp->regs->pc, cond);
    guard(cond, x, BRANCH_EXIT);
    return true;
}

#ifdef NANOJIT_IA32
/* Record LIR for a tableswitch or tableswitchx op. We record LIR only the
   "first" time we hit the op. Later, when we start traces after exiting that
   trace, we just patch. */
JS_REQUIRES_STACK LIns*
TraceRecorder::tableswitch()
{
    jsval& v = stackval(-1);
    if (!isNumber(v))
        return NULL;

    /* no need to guard if condition is constant */
    LIns* v_ins = f2i(get(&v));
    if (v_ins->isconst() || v_ins->isconstq())
        return NULL;

    jsbytecode* pc = cx->fp->regs->pc;
    /* Starting a new trace after exiting a trace via switch. */
    if (anchor &&
        (anchor->exitType == CASE_EXIT || anchor->exitType == DEFAULT_EXIT) &&
        fragment->ip == pc) {
        return NULL;
    }

    /* Decode jsop. */
    jsint low, high;
    if (*pc == JSOP_TABLESWITCH) {
        pc += JUMP_OFFSET_LEN;
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
    } else {
        pc += JUMPX_OFFSET_LEN;
        low = GET_JUMPX_OFFSET(pc);
        pc += JUMPX_OFFSET_LEN;
        high = GET_JUMPX_OFFSET(pc);
    }

    /* Really large tables won't fit in a page. This is a conservative check.
       If it matters in practice we need to go off-page. */
    if ((high + 1 - low) * sizeof(intptr_t*) + 128 > (unsigned) LARGEST_UNDERRUN_PROT) {
        // This throws away the return value of switchop but it seems
        // ok because switchop always returns true.
        (void) switchop();
        return NULL;
    }

    /* Generate switch LIR. */
    LIns* si_ins = lir_buf_writer->skip(sizeof(SwitchInfo));
    SwitchInfo* si = (SwitchInfo*) si_ins->payload();
    si->count = high + 1 - low;
    si->table = 0;
    si->index = (uint32) -1;
    LIns* diff = lir->ins2(LIR_sub, v_ins, lir->insImm(low));
    LIns* cmp = lir->ins2(LIR_ult, diff, lir->insImm(si->count));
    lir->insGuard(LIR_xf, cmp, snapshot(DEFAULT_EXIT));
    lir->insStore(diff, lir->insImmPtr(&si->index), lir->insImm(0));
    LIns* exit = snapshot(CASE_EXIT);
    ((GuardRecord*) exit->payload())->exit->switchInfo = si;
    return lir->insGuard(LIR_xtbl, diff, exit);
}
#endif

JS_REQUIRES_STACK bool
TraceRecorder::switchop()
{
    jsval& v = stackval(-1);
    LIns* v_ins = get(&v);
    /* no need to guard if condition is constant */
    if (v_ins->isconst() || v_ins->isconstq())
        return true;
    if (isNumber(v)) {
        jsdouble d = asNumber(v);
        guard(true,
              addName(lir->ins2(LIR_feq, v_ins, lir->insImmf(d)),
                      "guard(switch on numeric)"),
              BRANCH_EXIT);
    } else if (JSVAL_IS_STRING(v)) {
        LIns* args[] = { v_ins, INS_CONSTPTR(JSVAL_TO_STRING(v)) };
        guard(true,
              addName(lir->ins_eq0(lir->ins_eq0(lir->insCall(&js_EqualStrings_ci, args))),
                      "guard(switch on string)"),
              BRANCH_EXIT);
    } else if (JSVAL_TAG(v) == JSVAL_BOOLEAN) {
        guard(true,
              addName(lir->ins2(LIR_eq, v_ins, lir->insImm(JSVAL_TO_PUBLIC_PSEUDO_BOOLEAN(v))),
                      "guard(switch on boolean)"),
              BRANCH_EXIT);
    } else {
        ABORT_TRACE("switch on object or null");
    }
    return true;
}

JS_REQUIRES_STACK bool
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
JS_REQUIRES_STACK bool
TraceRecorder::inc(jsval& v, LIns*& v_ins, jsint incr, bool pre)
{
    if (!isNumber(v))
        ABORT_TRACE("can only inc numbers");

    LIns* v_after = alu(LIR_fadd, asNumber(v), incr, v_ins, lir->insImmf(incr));

    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    JS_ASSERT(cs.ndefs == 1);
    stack(-cs.nuses, pre ? v_after : v_ins);
    v_ins = v_after;
    return true;
}

JS_REQUIRES_STACK bool
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

    if (slot == SPROP_INVALID_SLOT)
        ABORT_TRACE("incProp on invalid slot");

    jsval& v = STOBJ_GET_SLOT(obj, slot);
    if (!inc(v, v_ins, incr, pre))
        return false;

    box_jsval(v, v_ins);

    LIns* dslots_ins = NULL;
    stobj_set_slot(obj_ins, slot, dslots_ins, v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::incElem(jsint incr, bool pre)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    jsval* vp;
    LIns* v_ins;
    LIns* addr_ins;
    if (!elem(l, r, vp, v_ins, addr_ins))
        return false;
    if (!addr_ins) // if we read a hole, abort
        return false;
    if (!inc(*vp, v_ins, incr, pre))
        return false;
    box_jsval(*vp, v_ins);
    lir->insStorei(v_ins, addr_ins, 0);
    return true;
}

static bool
evalCmp(LOpcode op, double result)
{
    bool cond;
    switch (op) {
      case LIR_feq:
        cond = (result == 0);
        break;
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
        JS_NOT_REACHED("unexpected comparison op");
        return false;
    }
    return cond;
}

static bool
evalCmp(LOpcode op, double l, double r)
{
    return evalCmp(op, l - r);
}

static bool
evalCmp(LOpcode op, JSString* l, JSString* r)
{
    if (op == LIR_feq)
        return js_EqualStrings(l, r);
    return evalCmp(op, js_CompareStrings(l, r));
}

JS_REQUIRES_STACK void
TraceRecorder::strictEquality(bool equal, bool cmpCase)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    LIns* l_ins = get(&l);
    LIns* r_ins = get(&r);
    LIns* x;
    bool cond;

    uint8 ltag = getPromotedType(l);
    if (ltag != getPromotedType(r)) {
        cond = !equal;
        x = lir->insImm(cond);
    } else if (ltag == JSVAL_STRING) {
        LIns* args[] = { r_ins, l_ins };
        x = lir->ins2i(LIR_eq, lir->insCall(&js_EqualStrings_ci, args), equal);
        cond = js_EqualStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
    } else {
        LOpcode op = (ltag != JSVAL_DOUBLE) ? LIR_eq : LIR_feq;
        x = lir->ins2(op, l_ins, r_ins);
        if (!equal)
            x = lir->ins_eq0(x);
        cond = (ltag == JSVAL_DOUBLE)
               ? asNumber(l) == asNumber(r)
               : l == r;
    }
    cond = (cond == equal);

    if (cmpCase) {
        /* Only guard if the same path may not always be taken. */
        if (!x->isconst())
            guard(cond, x, BRANCH_EXIT);
        return;
    }

    set(&l, x);
}

JS_REQUIRES_STACK bool
TraceRecorder::equality(bool negate, bool tryBranchAfterCond)
{
    jsval& rval = stackval(-1);
    jsval& lval = stackval(-2);
    LIns* l_ins = get(&lval);
    LIns* r_ins = get(&rval);

    return equalityHelper(lval, rval, l_ins, r_ins, negate, tryBranchAfterCond, lval);
}

JS_REQUIRES_STACK bool
TraceRecorder::equalityHelper(jsval l, jsval r, LIns* l_ins, LIns* r_ins,
                              bool negate, bool tryBranchAfterCond,
                              jsval& rval)
{
    bool fp = false;
    bool cond;
    LIns* args[] = { NULL, NULL };

    /*
     * The if chain below closely mirrors that found in 11.9.3, in general
     * deviating from that ordering of ifs only to account for SpiderMonkey's
     * conflation of booleans and undefined and for the possibility of
     * confusing objects and null.  Note carefully the spec-mandated recursion
     * in the final else clause, which terminates because Number == T recurs
     * only if T is Object, but that must recur again to convert Object to
     * primitive, and ToPrimitive throws if the object cannot be converted to
     * a primitive value (which would terminate recursion).
     */

    if (getPromotedType(l) == getPromotedType(r)) {
        if (JSVAL_TAG(l) == JSVAL_OBJECT || JSVAL_TAG(l) == JSVAL_BOOLEAN) {
            cond = (l == r);
        } else if (JSVAL_IS_STRING(l)) {
            args[0] = r_ins, args[1] = l_ins;
            l_ins = lir->insCall(&js_EqualStrings_ci, args);
            r_ins = lir->insImm(1);
            cond = js_EqualStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        } else {
            JS_ASSERT(isNumber(l) && isNumber(r));
            cond = (asNumber(l) == asNumber(r));
            fp = true;
        }
    } else if (JSVAL_IS_NULL(l) && JSVAL_TAG(r) == JSVAL_BOOLEAN) {
        l_ins = lir->insImm(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
        cond = (r == JSVAL_VOID);
    } else if (JSVAL_TAG(l) == JSVAL_BOOLEAN && JSVAL_IS_NULL(r)) {
        r_ins = lir->insImm(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
        cond = (l == JSVAL_VOID);
    } else if (isNumber(l) && JSVAL_IS_STRING(r)) {
        args[0] = r_ins, args[1] = cx_ins;
        r_ins = lir->insCall(&js_StringToNumber_ci, args);
        cond = (asNumber(l) == js_StringToNumber(cx, JSVAL_TO_STRING(r)));
        fp = true;
    } else if (JSVAL_IS_STRING(l) && isNumber(r)) {
        args[0] = l_ins, args[1] = cx_ins;
        l_ins = lir->insCall(&js_StringToNumber_ci, args);
        cond = (js_StringToNumber(cx, JSVAL_TO_STRING(l)) == asNumber(r));
        fp = true;
    } else {
        if (JSVAL_TAG(l) == JSVAL_BOOLEAN) {
            bool isVoid = JSVAL_IS_VOID(l);
            guard(isVoid,
                  lir->ins2(LIR_eq, l_ins, INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID))),
                  BRANCH_EXIT);
            if (!isVoid) {
                args[0] = l_ins, args[1] = cx_ins;
                l_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
                l = (l == JSVAL_VOID)
                    ? DOUBLE_TO_JSVAL(cx->runtime->jsNaN)
                    : INT_TO_JSVAL(l == JSVAL_TRUE);
                return equalityHelper(l, r, l_ins, r_ins, negate,
                                      tryBranchAfterCond, rval);
            }
        } else if (JSVAL_TAG(r) == JSVAL_BOOLEAN) {
            bool isVoid = JSVAL_IS_VOID(r);
            guard(isVoid,
                  lir->ins2(LIR_eq, r_ins, INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID))),
                  BRANCH_EXIT);
            if (!isVoid) {
                args[0] = r_ins, args[1] = cx_ins;
                r_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
                r = (r == JSVAL_VOID)
                    ? DOUBLE_TO_JSVAL(cx->runtime->jsNaN)
                    : INT_TO_JSVAL(r == JSVAL_TRUE);
                return equalityHelper(l, r, l_ins, r_ins, negate,
                                      tryBranchAfterCond, rval);
            }
        } else {
            if ((JSVAL_IS_STRING(l) || isNumber(l)) && !JSVAL_IS_PRIMITIVE(r))
                return call_imacro(equality_imacros.any_obj);
            if (!JSVAL_IS_PRIMITIVE(l) && (JSVAL_IS_STRING(r) || isNumber(r)))
                return call_imacro(equality_imacros.obj_any);
        }

        l_ins = lir->insImm(0);
        r_ins = lir->insImm(1);
        cond = false;
    }

    /* If the operands aren't numbers, compare them as integers. */
    LOpcode op = fp ? LIR_feq : LIR_eq;
    LIns* x = lir->ins2(op, l_ins, r_ins);
    if (negate) {
        x = lir->ins_eq0(x);
        cond = !cond;
    }

    /*
     * Don't guard if the same path is always taken.  If it isn't, we have to
     * fuse comparisons and the following branch, because the interpreter does
     * that.
     */
    if (tryBranchAfterCond && !x->isconst())
        fuseIf(cx->fp->regs->pc + 1, cond, x);

    /*
     * We update the stack after the guard. This is safe since the guard bails
     * out at the comparison and the interpreter will therefore re-execute the
     * comparison. This way the value of the condition doesn't have to be
     * calculated and saved on the stack in most cases.
     */
    set(&rval, x);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::relational(LOpcode op, bool tryBranchAfterCond)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    LIns* x = NULL;
    bool cond;
    LIns* l_ins = get(&l);
    LIns* r_ins = get(&r);
    bool fp = false;
    jsdouble lnum, rnum;

    /*
     * 11.8.5 if either argument is an object with a function-valued valueOf
     * property; if both arguments are objects with non-function-valued valueOf
     * properties, abort.
     */
    if (!JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_PRIMITIVE(r))
            return call_imacro(binary_imacros.obj_obj);
        return call_imacro(binary_imacros.obj_any);
    }
    if (!JSVAL_IS_PRIMITIVE(r))
        return call_imacro(binary_imacros.any_obj);

    /* 11.8.5 steps 3, 16-21. */
    if (JSVAL_IS_STRING(l) && JSVAL_IS_STRING(r)) {
        LIns* args[] = { r_ins, l_ins };
        l_ins = lir->insCall(&js_CompareStrings_ci, args);
        r_ins = lir->insImm(0);
        cond = evalCmp(op, JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        goto do_comparison;
    }

    /* 11.8.5 steps 4-5. */
    if (!JSVAL_IS_NUMBER(l)) {
        LIns* args[] = { l_ins, cx_ins };
        switch (JSVAL_TAG(l)) {
          case JSVAL_BOOLEAN:
            l_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
            break;
          case JSVAL_STRING:
            l_ins = lir->insCall(&js_StringToNumber_ci, args);
            break;
          case JSVAL_OBJECT:
            if (JSVAL_IS_NULL(l)) {
                l_ins = lir->insImmq(0);
                break;
            }
            // FALL THROUGH
          case JSVAL_INT:
          case JSVAL_DOUBLE:
          default:
            JS_NOT_REACHED("JSVAL_IS_NUMBER if int/double, objects should "
                           "have been handled at start of method");
            ABORT_TRACE("safety belt");
        }
    }
    if (!JSVAL_IS_NUMBER(r)) {
        LIns* args[] = { r_ins, cx_ins };
        switch (JSVAL_TAG(r)) {
          case JSVAL_BOOLEAN:
            r_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
            break;
          case JSVAL_STRING:
            r_ins = lir->insCall(&js_StringToNumber_ci, args);
            break;
          case JSVAL_OBJECT:
            if (JSVAL_IS_NULL(r)) {
                r_ins = lir->insImmq(0);
                break;
            }
            // FALL THROUGH
          case JSVAL_INT:
          case JSVAL_DOUBLE:
          default:
            JS_NOT_REACHED("JSVAL_IS_NUMBER if int/double, objects should "
                           "have been handled at start of method");
            ABORT_TRACE("safety belt");
        }
    }
    {
        jsval tmp = JSVAL_NULL;
        JSAutoTempValueRooter tvr(cx, 1, &tmp);

        tmp = l;
        lnum = js_ValueToNumber(cx, &tmp);
        tmp = r;
        rnum = js_ValueToNumber(cx, &tmp);
    }
    cond = evalCmp(op, lnum, rnum);
    fp = true;

    /* 11.8.5 steps 6-15. */
  do_comparison:
    /* If the result is not a number or it's not a quad, we must use an integer compare. */
    if (!fp) {
        JS_ASSERT(op >= LIR_feq && op <= LIR_fge);
        op = LOpcode(op + (LIR_eq - LIR_feq));
    }
    x = lir->ins2(op, l_ins, r_ins);

    /*
     * Don't guard if the same path is always taken.  If it isn't, we have to
     * fuse comparisons and the following branch, because the interpreter does
     * that.
     */
    if (tryBranchAfterCond && !x->isconst())
        fuseIf(cx->fp->regs->pc + 1, cond, x);

    /*
     * We update the stack after the guard. This is safe since the guard bails
     * out at the comparison and the interpreter will therefore re-execute the
     * comparison. This way the value of the condition doesn't have to be
     * calculated and saved on the stack in most cases.
     */
    set(&l, x);
    return true;
}

JS_REQUIRES_STACK bool
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

JS_REQUIRES_STACK bool
TraceRecorder::binary(LOpcode op)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (!JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_PRIMITIVE(r))
            return call_imacro(binary_imacros.obj_obj);
        return call_imacro(binary_imacros.obj_any);
    }
    if (!JSVAL_IS_PRIMITIVE(r))
        return call_imacro(binary_imacros.any_obj);

    bool intop = !(op & LIR64);
    LIns* a = get(&l);
    LIns* b = get(&r);

    bool leftIsNumber = isNumber(l);
    jsdouble lnum = leftIsNumber ? asNumber(l) : 0;

    bool rightIsNumber = isNumber(r);
    jsdouble rnum = rightIsNumber ? asNumber(r) : 0;

    if ((op >= LIR_sub && op <= LIR_ush) ||  // sub, mul, (callh), or, xor, (not,) lsh, rsh, ush
        (op >= LIR_fsub && op <= LIR_fdiv)) { // fsub, fmul, fdiv
        LIns* args[2];
        if (JSVAL_IS_STRING(l)) {
            args[0] = a;
            args[1] = cx_ins;
            a = lir->insCall(&js_StringToNumber_ci, args);
            lnum = js_StringToNumber(cx, JSVAL_TO_STRING(l));
            leftIsNumber = true;
        }
        if (JSVAL_IS_STRING(r)) {
            args[0] = b;
            args[1] = cx_ins;
            b = lir->insCall(&js_StringToNumber_ci, args);
            rnum = js_StringToNumber(cx, JSVAL_TO_STRING(r));
            rightIsNumber = true;
        }
    }
    if (JSVAL_TAG(l) == JSVAL_BOOLEAN) {
        LIns* args[] = { a, cx_ins };
        a = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
        lnum = js_BooleanOrUndefinedToNumber(cx, JSVAL_TO_PSEUDO_BOOLEAN(l));
        leftIsNumber = true;
    }
    if (JSVAL_TAG(r) == JSVAL_BOOLEAN) {
        LIns* args[] = { b, cx_ins };
        b = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
        rnum = js_BooleanOrUndefinedToNumber(cx, JSVAL_TO_PSEUDO_BOOLEAN(r));
        rightIsNumber = true;
    }
    if (leftIsNumber && rightIsNumber) {
        if (intop) {
            LIns *args[] = { a };
            a = lir->insCall(op == LIR_ush ? &js_DoubleToUint32_ci : &js_DoubleToInt32_ci, args);
            b = f2i(b);
        }
        a = alu(op, lnum, rnum, a, b);
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
#define OP(ops) (*(JSObjectOp*) ((char*)(ops) + op_offset))
    if (OP(map->ops) != OP(&js_ObjectOps))
        return false;

    ops_ins = addName(lir->insLoad(LIR_ldp, map_ins, offsetof(JSObjectMap, ops)), "ops");
    LIns* n = lir->insLoad(LIR_ldp, ops_ins, op_offset);
    guard(true,
          addName(lir->ins2(LIR_eq, n, INS_CONSTFUNPTR(OP(&js_ObjectOps))), "guard(native-map)"),
          BRANCH_EXIT);
#undef OP

    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::test_property_cache(JSObject* obj, LIns* obj_ins, JSObject*& obj2, jsuword& pcval)
{
    jsbytecode* pc = cx->fp->regs->pc;
    JS_ASSERT(*pc != JSOP_INITPROP && *pc != JSOP_SETNAME && *pc != JSOP_SETPROP);

    // Mimic the interpreter's special case for dense arrays by skipping up one
    // hop along the proto chain when accessing a named (not indexed) property,
    // typically to find Array.prototype methods.
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
    uint32 format = js_CodeSpec[*pc].format;
    uint32 mode = JOF_MODE(format);

    // No need to guard native-ness of global object.
    JS_ASSERT(OBJ_IS_NATIVE(globalObj));
    if (aobj != globalObj) {
        size_t op_offset = 0;
        if (mode == JOF_PROP || mode == JOF_VARPROP) {
            JS_ASSERT(!(format & JOF_SET));
            op_offset = offsetof(JSObjectOps, getProperty);
        } else {
            JS_ASSERT(mode == JOF_NAME);
        }

        if (!map_is_native(aobj->map, map_ins, ops_ins, op_offset))
            ABORT_TRACE("non-native map");
    }

    JSAtom* atom;
    JSPropCacheEntry* entry;
    PROPERTY_CACHE_TEST(cx, pc, aobj, obj2, entry, atom);
    if (!atom) {
        // Null atom means that obj2 is locked and must now be unlocked.
        JS_UNLOCK_OBJ(cx, obj2);
    } else {
        // Miss: pre-fill the cache for the interpreter, as well as for our needs.
        // FIXME: 452357 - correctly propagate exceptions into the interpreter from
        // js_FindPropertyHelper, js_LookupPropertyWithFlags, and elsewhere.
        jsid id = ATOM_TO_JSID(atom);
        JSProperty* prop;
        if (JOF_OPMODE(*pc) == JOF_NAME) {
            JS_ASSERT(aobj == obj);
            if (js_FindPropertyHelper(cx, id, &obj, &obj2, &prop, &entry) < 0)
                ABORT_TRACE("failed to find name");
        } else {
            int protoIndex = js_LookupPropertyWithFlags(cx, aobj, id,
                                                        cx->resolveFlags,
                                                        &obj2, &prop);
            if (protoIndex < 0)
                ABORT_TRACE("failed to lookup property");

            if (prop) {
                js_FillPropertyCache(cx, aobj, OBJ_SHAPE(aobj), 0, protoIndex, obj2,
                                     (JSScopeProperty*) prop, &entry);
            }
        }

        if (!prop) {
            // Propagate obj from js_FindPropertyHelper to record_JSOP_BINDNAME
            // via our obj2 out-parameter. If we are recording JSOP_SETNAME and
            // the global it's assigning does not yet exist, create it.
            obj2 = obj;

            // Use PCVAL_NULL to return "no such property" to our caller.
            pcval = PCVAL_NULL;
            return true;
        }

        OBJ_DROP_PROPERTY(cx, obj2, prop);
        if (!entry)
            ABORT_TRACE("failed to fill property cache");
    }

    if (wasDeepAborted())
        ABORT_TRACE("deep abort from property lookup");

#ifdef JS_THREADSAFE
    // There's a potential race in any JS_THREADSAFE embedding that's nuts
    // enough to share mutable objects on the scope or proto chain, but we
    // don't care about such insane embeddings. Anyway, the (scope, proto)
    // entry->vcap coordinates must reach obj2 from aobj at this point.
    JS_ASSERT(cx->requestDepth);
#endif

    // Emit guard(s), common code for both hit and miss cases.
    // Check for first-level cache hit and guard on kshape if possible.
    // Otherwise guard on key object exact match.
    if (PCVCAP_TAG(entry->vcap) <= 1) {
        if (aobj != globalObj) {
            LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)),
                                      "shape");
            guard(true, addName(lir->ins2i(LIR_eq, shape_ins, entry->kshape), "guard(kshape)"),
                  BRANCH_EXIT);
        }
    } else {
#ifdef DEBUG
        JSOp op = JSOp(*pc);
        ptrdiff_t pcoff = (op == JSOP_GETARGPROP) ? ARGNO_LEN :
                          (op == JSOP_GETLOCALPROP) ? SLOTNO_LEN : 0;
        jsatomid index = js_GetIndexFromBytecode(cx, cx->fp->script, pc, pcoff);
        JS_ASSERT(entry->kpc == (jsbytecode*) atoms[index]);
        JS_ASSERT(entry->kshape == jsuword(aobj));
#endif
        if (aobj != globalObj && !obj_ins->isconstp()) {
            guard(true, addName(lir->ins2i(LIR_eq, obj_ins, entry->kshape), "guard(kobj)"),
                  BRANCH_EXIT);
        }
    }

    // For any hit that goes up the scope and/or proto chains, we will need to
    // guard on the shape of the object containing the property.
    if (PCVCAP_TAG(entry->vcap) >= 1) {
        jsuword vcap = entry->vcap;
        uint32 vshape = PCVCAP_SHAPE(vcap);
        JS_ASSERT(OBJ_SHAPE(obj2) == vshape);

        LIns* obj2_ins;
        if (PCVCAP_TAG(entry->vcap) == 1) {
            // Duplicate the special case in PROPERTY_CACHE_TEST.
            obj2_ins = stobj_get_fslot(obj_ins, JSSLOT_PROTO);
            guard(false, lir->ins_eq0(obj2_ins), BRANCH_EXIT);
        } else {
            obj2_ins = INS_CONSTPTR(obj2);
        }
        map_ins = lir->insLoad(LIR_ldp, obj2_ins, (int)offsetof(JSObject, map));
        if (!map_is_native(obj2->map, map_ins, ops_ins))
            ABORT_TRACE("non-native map");

        LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)),
                                  "shape");
        guard(true,
              addName(lir->ins2i(LIR_eq, shape_ins, vshape), "guard(vshape)"),
              BRANCH_EXIT);
    }

    pcval = entry->vword;
    return true;
}

JS_REQUIRES_STACK bool
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

    /* Insist on obj being the directly addressed object. */
    if (obj2 != obj)
        ABORT_TRACE("test_property_cache_direct_slot hit prototype chain");

    /* Don't trace getter or setter calls, our caller wants a direct slot. */
    if (PCVAL_IS_SPROP(pcval)) {
        JSScopeProperty* sprop = PCVAL_TO_SPROP(pcval);

        if (!isValidSlot(OBJ_SCOPE(obj), sprop))
            return false;

        slot = sprop->slot;
    } else {
        if (!PCVAL_IS_SLOT(pcval))
            ABORT_TRACE("PCE is not a slot");
        slot = PCVAL_TO_SLOT(pcval);
    }
    return true;
}

void
TraceRecorder::stobj_set_dslot(LIns *obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins,
                               const char *name)
{
    if (!dslots_ins)
        dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    addName(lir->insStorei(v_ins, dslots_ins, slot * sizeof(jsval)), name);
}

void
TraceRecorder::stobj_set_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins)
{
    if (slot < JS_INITIAL_NSLOTS) {
        addName(lir->insStorei(v_ins, obj_ins,
                               offsetof(JSObject, fslots) + slot * sizeof(jsval)),
                "set_slot(fslots)");
    } else {
        stobj_set_dslot(obj_ins, slot - JS_INITIAL_NSLOTS, dslots_ins, v_ins,
                        "set_slot(dslots)");
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
        v_ins = INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
    return true;
}

JS_REQUIRES_STACK void
TraceRecorder::box_jsval(jsval v, LIns*& v_ins)
{
    if (isNumber(v)) {
        LIns* args[] = { v_ins, cx_ins };
        v_ins = lir->insCall(&js_BoxDouble_ci, args);
        guard(false, lir->ins2(LIR_eq, v_ins, INS_CONST(JSVAL_ERROR_COOKIE)),
              OOM_EXIT);
        return;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        v_ins = lir->ins2i(LIR_pior, lir->ins2i(LIR_pilsh, v_ins, JSVAL_TAGBITS), JSVAL_BOOLEAN);
        return;
      case JSVAL_OBJECT:
        return;
      default:
        JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING);
        v_ins = lir->ins2(LIR_pior, v_ins, INS_CONST(JSVAL_STRING));
        return;
    }
}

JS_REQUIRES_STACK void
TraceRecorder::unbox_jsval(jsval v, LIns*& v_ins)
{
    if (isNumber(v)) {
        // JSVAL_IS_NUMBER(v)
        guard(false,
              lir->ins_eq0(lir->ins2(LIR_pior,
                                     lir->ins2(LIR_piand, v_ins, INS_CONST(JSVAL_INT)),
                                     lir->ins2i(LIR_eq,
                                                lir->ins2(LIR_piand, v_ins,
                                                          INS_CONST(JSVAL_TAGMASK)),
                                                JSVAL_DOUBLE))),
              MISMATCH_EXIT);
        LIns* args[] = { v_ins };
        v_ins = lir->insCall(&js_UnboxDouble_ci, args);
        return;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_BOOLEAN:
        guard(true,
              lir->ins2i(LIR_eq,
                         lir->ins2(LIR_piand, v_ins, INS_CONST(JSVAL_TAGMASK)),
                         JSVAL_BOOLEAN),
              MISMATCH_EXIT);
        v_ins = lir->ins2i(LIR_ush, v_ins, JSVAL_TAGBITS);
        return;
      case JSVAL_OBJECT:
        if (JSVAL_IS_NULL(v)) {
            // JSVAL_NULL maps to type JSVAL_TNULL, so insist that v_ins == 0 here.
            guard(true, lir->ins_eq0(v_ins), MISMATCH_EXIT);
        } else {
            // We must guard that v_ins has JSVAL_OBJECT tag but is not JSVAL_NULL.
            LIns* exit = snapshot(MISMATCH_EXIT);
            guard(true,
                  lir->ins2i(LIR_eq,
                             lir->ins2(LIR_piand, v_ins, INS_CONST(JSVAL_TAGMASK)),
                             JSVAL_OBJECT),
                  exit);
            guard(false, lir->ins_eq0(v_ins), exit);
        }
        return;
      default:
        JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING);
        guard(true,
              lir->ins2i(LIR_eq,
                        lir->ins2(LIR_piand, v_ins, INS_CONST(JSVAL_TAGMASK)),
                        JSVAL_STRING),
              MISMATCH_EXIT);
        v_ins = lir->ins2(LIR_piand, v_ins, INS_CONST(~JSVAL_TAGMASK));
        return;
    }
}

JS_REQUIRES_STACK bool
TraceRecorder::getThis(LIns*& this_ins)
{
    if (cx->fp->callee) { /* in a function */
        if (JSVAL_IS_NULL(cx->fp->argv[-1]))
            return false;
        this_ins = get(&cx->fp->argv[-1]);
        guard(false, lir->ins_eq0(this_ins), MISMATCH_EXIT);
    } else { /* in global code */
        this_ins = scopeChain();
    }
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::guardClass(JSObject* obj, LIns* obj_ins, JSClass* clasp, LIns* exit)
{
    bool cond = STOBJ_GET_CLASS(obj) == clasp;

    LIns* class_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, classword));
    class_ins = lir->ins2(LIR_piand, class_ins, lir->insImm(~3));

    char namebuf[32];
    JS_snprintf(namebuf, sizeof namebuf, "guard(class is %s)", clasp->name);
    guard(cond, addName(lir->ins2(LIR_eq, class_ins, INS_CONSTPTR(clasp)), namebuf), exit);
    return cond;
}

JS_REQUIRES_STACK bool
TraceRecorder::guardDenseArray(JSObject* obj, LIns* obj_ins, ExitType exitType)
{
    return guardClass(obj, obj_ins, &js_ArrayClass, snapshot(exitType));
}

JS_REQUIRES_STACK bool
TraceRecorder::guardDenseArrayIndex(JSObject* obj, jsint idx, LIns* obj_ins,
                                    LIns* dslots_ins, LIns* idx_ins, ExitType exitType)
{
    jsuint capacity = js_DenseArrayCapacity(obj);

    bool cond = (jsuint(idx) < jsuint(obj->fslots[JSSLOT_ARRAY_LENGTH]) && jsuint(idx) < capacity);
    if (cond) {
        /* Guard array length */
        LIns* exit = guard(true,
                           lir->ins2(LIR_ult, idx_ins, stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH)),
                           exitType)->oprnd2();
        /* dslots must not be NULL */
        guard(false,
              lir->ins_eq0(dslots_ins),
              exit);
        /* Guard array capacity */
        guard(true,
              lir->ins2(LIR_ult,
                        idx_ins,
                        lir->insLoad(LIR_ldp, dslots_ins, 0 - (int)sizeof(jsval))),
              exit);
    } else {
        /* If not idx < length, stay on trace (and read value as undefined). */
        LIns* br1 = lir->insBranch(LIR_jf,
                                   lir->ins2(LIR_ult,
                                             idx_ins,
                                             stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH)),
                                   NULL);
        /* If dslots is NULL, stay on trace (and read value as undefined). */
        LIns* br2 = lir->insBranch(LIR_jt, lir->ins_eq0(dslots_ins), NULL);
        /* If not idx < capacity, stay on trace (and read value as undefined). */
        LIns* br3 = lir->insBranch(LIR_jf,
                                   lir->ins2(LIR_ult,
                                             idx_ins,
                                             lir->insLoad(LIR_ldp, dslots_ins, 0 - (int)sizeof(jsval))),
                                   NULL);
        lir->insGuard(LIR_x, lir->insImm(1), snapshot(exitType));
        LIns* label = lir->ins0(LIR_label);
        br1->target(label);
        br2->target(label);
        br3->target(label);
    }
    return cond;
}

bool
TraceRecorder::guardNotGlobalObject(JSObject* obj, LIns* obj_ins)
{
    if (obj == globalObj)
        ABORT_TRACE("reference aliases global object");
    guard(false, lir->ins2(LIR_eq, obj_ins, globalObj_ins), MISMATCH_EXIT);
    return true;
}

JS_REQUIRES_STACK void
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
        vpstop = &fp->argv[fp->fun->nargs];
        while (vp < vpstop)
            nativeFrameTracker.set(vp++, (LIns*)0);
    }
    vp = &fp->slots[0];
    vpstop = &fp->slots[fp->script->nslots];
    while (vp < vpstop)
        nativeFrameTracker.set(vp++, (LIns*)0);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_EnterFrame()
{
    JSStackFrame* fp = cx->fp;

    if (++callDepth >= MAX_CALLDEPTH)
        ABORT_TRACE("exceeded maximum call depth");
    // FIXME: Allow and attempt to inline a single level of recursion until we compile
    //        recursive calls as independent trees (459301).
    if (fp->script == fp->down->script && fp->down->down && fp->down->down->script == fp->script)
        ABORT_TRACE("recursive call");

    debug_only_v(printf("EnterFrame %s, callDepth=%d\n",
                        js_AtomToPrintableString(cx, cx->fp->fun->atom),
                        callDepth);)
    debug_only_v(
        js_Disassemble(cx, cx->fp->script, JS_TRUE, stdout);
        printf("----\n");)
    LIns* void_ins = INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));

    jsval* vp = &fp->argv[fp->argc];
    jsval* vpstop = vp + ptrdiff_t(fp->fun->nargs) - ptrdiff_t(fp->argc);
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

JS_REQUIRES_STACK bool
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
    atoms = FrameAtomBase(cx, cx->fp);
    set(&stackval(-1), rval_ins, true);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_PUSH()
{
    stack(0, INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_POPV()
{
    jsval& rval = stackval(-1);
    LIns *rval_ins = get(&rval);
    box_jsval(rval, rval_ins);

    // Store it in cx->fp->rval. NB: Tricky dependencies. cx->fp is the right
    // frame because POPV appears only in global and eval code and we don't
    // trace JSOP_EVAL or leaving the frame where tracing started.
    LIns *fp_ins = lir->insLoad(LIR_ldp, cx_ins, offsetof(JSContext, fp));
    lir->insStorei(rval_ins, fp_ins, offsetof(JSStackFrame, rval));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENTERWITH()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LEAVEWITH()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_RETURN()
{
    jsval& rval = stackval(-1);
    JSStackFrame *fp = cx->fp;
    if ((cx->fp->flags & JSFRAME_CONSTRUCTING) && JSVAL_IS_PRIMITIVE(rval)) {
        JS_ASSERT(OBJECT_TO_JSVAL(fp->thisp) == fp->argv[-1]);
        rval_ins = get(&fp->argv[-1]);
    } else {
        rval_ins = get(&rval);
    }
    debug_only_v(printf("returning from %s\n", js_AtomToPrintableString(cx, cx->fp->fun->atom));)
    clearFrameSlotsFromCache();
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GOTO()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IFEQ()
{
    trackCfgMerges(cx->fp->regs->pc);
    return ifop();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IFNE()
{
    return ifop();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ARGUMENTS()
{
#if 1
    ABORT_TRACE("can't trace arguments yet");
#else
    LIns* args[] = { cx_ins };
    LIns* a_ins = lir->insCall(&js_Arguments_ci, args);
    guard(false, lir->ins_eq0(a_ins), OOM_EXIT);
    stack(0, a_ins);
    return true;
#endif
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DUP()
{
    stack(0, get(&stackval(-1)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DUP2()
{
    stack(0, get(&stackval(-2)));
    stack(1, get(&stackval(-1)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SWAP()
{
    jsval& l = stackval(-2);
    jsval& r = stackval(-1);
    LIns* l_ins = get(&l);
    LIns* r_ins = get(&r);
    set(&r, l_ins);
    set(&l, r_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_PICK()
{
    jsval* sp = cx->fp->regs->sp;
    jsint n = cx->fp->regs->pc[1];
    JS_ASSERT(sp - (n+1) >= StackBase(cx->fp));
    LIns* top = get(sp - (n+1));
    for (jsint i = 0; i < n; ++i)
        set(sp - (n+1) + i, get(sp - n + i));
    set(&sp[-1], top);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETCONST()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BITOR()
{
    return binary(LIR_or);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BITXOR()
{
    return binary(LIR_xor);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BITAND()
{
    return binary(LIR_and);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_EQ()
{
    return equality(false, true);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NE()
{
    return equality(true, true);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LT()
{
    return relational(LIR_flt, true);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LE()
{
    return relational(LIR_fle, true);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GT()
{
    return relational(LIR_fgt, true);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GE()
{
    return relational(LIR_fge, true);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LSH()
{
    return binary(LIR_lsh);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_RSH()
{
    return binary(LIR_rsh);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_URSH()
{
    return binary(LIR_ush);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ADD()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (!JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_PRIMITIVE(r))
            return call_imacro(add_imacros.obj_obj);
        return call_imacro(add_imacros.obj_any);
    }
    if (!JSVAL_IS_PRIMITIVE(r))
        return call_imacro(add_imacros.any_obj);

    if (JSVAL_IS_STRING(l) || JSVAL_IS_STRING(r)) {
        LIns* args[] = { stringify(r), stringify(l), cx_ins };
        LIns* concat = lir->insCall(&js_ConcatStrings_ci, args);
        guard(false, lir->ins_eq0(concat), OOM_EXIT);
        set(&l, concat);
        return true;
    }

    return binary(LIR_fadd);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SUB()
{
    return binary(LIR_fsub);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_MUL()
{
    return binary(LIR_fmul);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DIV()
{
    return binary(LIR_fdiv);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_MOD()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (!JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_PRIMITIVE(r))
            return call_imacro(binary_imacros.obj_obj);
        return call_imacro(binary_imacros.obj_any);
    }
    if (!JSVAL_IS_PRIMITIVE(r))
        return call_imacro(binary_imacros.any_obj);

    if (isNumber(l) && isNumber(r)) {
        LIns* l_ins = get(&l);
        LIns* r_ins = get(&r);
        LIns* x;
        /* We can't demote this in a filter since we need the actual values of l and r. */
        if (isPromote(l_ins) && isPromote(r_ins) && asNumber(l) >= 0 && asNumber(r) > 0) {
            LIns* args[] = { ::demote(lir, r_ins), ::demote(lir, l_ins) };
            x = lir->insCall(&js_imod_ci, args);
            guard(false, lir->ins2(LIR_eq, x, lir->insImm(-1)), BRANCH_EXIT);
            x = lir->ins1(LIR_i2f, x);
        } else {
            LIns* args[] = { r_ins, l_ins };
            x = lir->insCall(&js_dmod_ci, args);
        }
        set(&l, x);
        return true;
    }
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NOT()
{
    jsval& v = stackval(-1);
    if (JSVAL_TAG(v) == JSVAL_BOOLEAN) {
        set(&v, lir->ins_eq0(lir->ins2i(LIR_eq, get(&v), 1)));
        return true;
    }
    if (isNumber(v)) {
        LIns* v_ins = get(&v);
        set(&v, lir->ins2(LIR_or, lir->ins2(LIR_feq, v_ins, lir->insImmq(0)),
                                  lir->ins_eq0(lir->ins2(LIR_feq, v_ins, v_ins))));
        return true;
    }
    if (JSVAL_TAG(v) == JSVAL_OBJECT) {
        set(&v, lir->ins_eq0(get(&v)));
        return true;
    }
    JS_ASSERT(JSVAL_IS_STRING(v));
    set(&v, lir->ins_eq0(lir->ins2(LIR_piand,
                                   lir->insLoad(LIR_ldp, get(&v), (int)offsetof(JSString, length)),
                                   INS_CONSTPTR(reinterpret_cast<void *>(JSSTRING_LENGTH_MASK)))));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BITNOT()
{
    return unary(LIR_not);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NEG()
{
    jsval& v = stackval(-1);

    if (!JSVAL_IS_PRIMITIVE(v))
        return call_imacro(unary_imacros.sign);

    if (isNumber(v)) {
        LIns* a = get(&v);

        /* If we're a promoted integer, we have to watch out for 0s since -0 is a double.
           Only follow this path if we're not an integer that's 0 and we're not a double
           that's zero.
         */
        if (isPromoteInt(a) &&
            (!JSVAL_IS_INT(v) || JSVAL_TO_INT(v) != 0) &&
            (!JSVAL_IS_DOUBLE(v) || !JSDOUBLE_IS_NEGZERO(*JSVAL_TO_DOUBLE(v))) &&
            -asNumber(v) == (int)-asNumber(v)) {
            a = lir->ins1(LIR_neg, ::demote(lir, a));
            if (!a->isconst()) {
                LIns* exit = snapshot(OVERFLOW_EXIT);
                lir->insGuard(LIR_xt, lir->ins1(LIR_ov, a), exit);
                lir->insGuard(LIR_xt, lir->ins2(LIR_eq, a, lir->insImm(0)), exit);
            }
            a = lir->ins1(LIR_i2f, a);
        } else {
            a = lir->ins1(LIR_fneg, a);
        }

        set(&v, a);
        return true;
    }

    if (JSVAL_IS_NULL(v)) {
        set(&v, lir->insImmf(-0.0));
        return true;
    }

    JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING || JSVAL_TAG(v) == JSVAL_BOOLEAN);

    LIns* args[] = { get(&v), cx_ins };
    set(&v, lir->ins1(LIR_fneg,
                      lir->insCall(JSVAL_IS_STRING(v)
                                   ? &js_StringToNumber_ci
                                   : &js_BooleanOrUndefinedToNumber_ci,
                                   args)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_POS()
{
    jsval& v = stackval(-1);

    if (!JSVAL_IS_PRIMITIVE(v))
        return call_imacro(unary_imacros.sign);

    if (isNumber(v))
        return true;

    if (JSVAL_IS_NULL(v)) {
        set(&v, lir->insImmq(0));
        return true;
    }

    JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING || JSVAL_TAG(v) == JSVAL_BOOLEAN);

    LIns* args[] = { get(&v), cx_ins };
    set(&v, lir->insCall(JSVAL_IS_STRING(v)
                         ? &js_StringToNumber_ci
                         : &js_BooleanOrUndefinedToNumber_ci,
                         args));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_PRIMTOP()
{
    // Either this opcode does nothing or we couldn't have traced here, because
    // we'd have thrown an exception -- so do nothing if we actually hit this.
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_OBJTOP()
{
    // See the comment in record_JSOP_PRIMTOP.
    return true;
}

JSBool
js_Array(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

JSBool
js_Object(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool
js_Date(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

bool
TraceRecorder::getClassPrototype(JSObject* ctor, LIns*& proto_ins)
{
    jsval pval;

    if (!OBJ_GET_PROPERTY(cx, ctor,
                          ATOM_TO_JSID(cx->runtime->atomState
                                       .classPrototypeAtom),
                          &pval)) {
        ABORT_TRACE("error getting prototype from constructor");
    }
    if (JSVAL_TAG(pval) != JSVAL_OBJECT)
        ABORT_TRACE("got primitive prototype from constructor");
#ifdef DEBUG
    JSBool ok, found;
    uintN attrs;
    ok = JS_GetPropertyAttributes(cx, ctor, js_class_prototype_str, &attrs, &found);
    JS_ASSERT(ok);
    JS_ASSERT(found);
    JS_ASSERT((~attrs & (JSPROP_READONLY | JSPROP_PERMANENT)) == 0);
#endif
    proto_ins = INS_CONSTPTR(JSVAL_TO_OBJECT(pval));
    return true;
}

bool
TraceRecorder::newArray(JSObject *ctor, uint32 argc, jsval *argv, jsval *rval)
{
    LIns *proto_ins, *arr_ins;
    if (!getClassPrototype(ctor, proto_ins))
        return false;

    if (argc == 0) {
        LIns *args[] = { proto_ins, cx_ins };
        arr_ins = lir->insCall(&js_FastNewArray_ci, args);
        guard(false, lir->ins_eq0(arr_ins), OOM_EXIT);
    } else if (argc == 1 && (JSVAL_IS_NUMBER(argv[0]) || JSVAL_IS_STRING(argv[0]))) {
        bool num = JSVAL_IS_NUMBER(argv[0]);
        LIns *arg_ins = get(argv);
        if (num)
            arg_ins = f2i(arg_ins);
        LIns *args[] = { arg_ins, proto_ins, cx_ins };
        arr_ins = lir->insCall(num ? &js_FastNewArrayWithLength_ci : &js_Array_1str_ci, args);
        guard(false, lir->ins_eq0(arr_ins), OOM_EXIT);
    } else {
        // arr_ins = js_NewUninitializedArray(cx, Array.prototype, argc)
        LIns *args[] = { INS_CONST(argc), proto_ins, cx_ins };
        arr_ins = lir->insCall(&js_NewUninitializedArray_ci, args);
        guard(false, lir->ins_eq0(arr_ins), OOM_EXIT);

        // arr->dslots[i] = box_jsval(vp[i]);  for i in 0..argc
        LIns *dslots_ins = NULL;
        for (uint32 i = 0; i < argc; i++) {
            LIns *elt_ins = get(argv + i);
            box_jsval(argv[i], elt_ins);
            stobj_set_dslot(arr_ins, i, dslots_ins, elt_ins, "set_array_elt");
        }
    }
    set(rval, arr_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::functionCall(bool constructing, uintN argc)
{
    JSStackFrame* fp = cx->fp;
    jsbytecode *pc = fp->regs->pc;

    jsval& fval = stackval(0 - (2 + argc));
    JS_ASSERT(&fval >= StackBase(fp));

    if (!VALUE_IS_FUNCTION(cx, fval))
        ABORT_TRACE("callee is not a function");

    jsval& tval = stackval(0 - (argc + 1));
    LIns* this_ins = get(&tval);

    /*
     * If this is NULL, this is a shapeless call. If we observe a shapeless call
     * at recording time, the call at this point will always be shapeless so we
     * can make the decision based on recording-time introspection of this.
     */
    if (tval == JSVAL_NULL && !guardCallee(fval))
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
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(fval));

    if (FUN_INTERPRETED(fun)) {
        if (constructing) {
            LIns* args[] = { get(&fval), cx_ins };
            LIns* tv_ins = lir->insCall(&js_NewInstance_ci, args);
            guard(false, lir->ins_eq0(tv_ins), OOM_EXIT);
            set(&tval, tv_ins);
        }
        return interpretedFunctionCall(fval, fun, argc, constructing);
    }

    if (FUN_SLOW_NATIVE(fun)) {
        JSNative native = fun->u.n.native;
        if (native == js_Array)
            return newArray(FUN_OBJECT(fun), argc, &tval + 1, &fval);
        if (native == js_String && argc == 1) {
            jsval& v = stackval(-1);
            if (!JSVAL_IS_PRIMITIVE(v))
                return call_imacro(call_imacros.String);
            LIns* v_ins = stringify(v);
            if (constructing) {
                LIns *proto_ins;
                if (!getClassPrototype(FUN_OBJECT(fun), proto_ins))
                    return false;
                LIns *args[] = { v_ins, proto_ins, cx_ins };
                v_ins = lir->insCall(&js_String_tn_ci, args);
                guard(false, lir->ins_eq0(v_ins), OOM_EXIT);
            }
            set(&fval, v_ins);
            return true;
        }
    }

    if (!(fun->flags & JSFUN_TRACEABLE))
        ABORT_TRACE("untraceable native");

    JSTraceableNative* known = FUN_TRCINFO(fun);
    JS_ASSERT(known && (JSFastNative)fun->u.n.native == known->native);

    LIns* args[5];
    do {
        if (((known->flags & JSTN_CONSTRUCTOR) != 0) != constructing)
            continue;

        uintN knownargc = strlen(known->argtypes);
        if (argc != knownargc)
            continue;

        intN prefixc = strlen(known->prefix);
        JS_ASSERT(prefixc <= 3);
        LIns** argp = &args[argc + prefixc - 1];
        char argtype;

#if defined _DEBUG
        memset(args, 0xCD, sizeof(args));
#endif

        uintN i;
        for (i = prefixc; i--; ) {
            argtype = known->prefix[i];
            if (argtype == 'C') {
                *argp = cx_ins;
            } else if (argtype == 'T') {   /* this, as an object */
                if (JSVAL_IS_PRIMITIVE(tval))
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'S') {   /* this, as a string */
                if (!JSVAL_IS_STRING(tval))
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'f') {
                *argp = INS_CONSTPTR(JSVAL_TO_OBJECT(fval));
            } else if (argtype == 'p') {
                if (!getClassPrototype(JSVAL_TO_OBJECT(fval), *argp))
                    return false;
            } else if (argtype == 'R') {
                *argp = INS_CONSTPTR(cx->runtime);
            } else if (argtype == 'P') {
                // FIXME: Set pc to imacpc when recording JSOP_CALL inside the
                //        JSOP_GETELEM imacro (bug 476559).
                if (*pc == JSOP_CALL && fp->imacpc && *fp->imacpc == JSOP_GETELEM)
                    *argp = INS_CONSTPTR(fp->imacpc);
                else
                    *argp = INS_CONSTPTR(pc);
            } else if (argtype == 'D') {  /* this, as a number */
                if (!isNumber(tval))
                    goto next_specialization;
                *argp = this_ins;
            } else {
                JS_NOT_REACHED("unknown prefix arg type");
            }
            argp--;
        }

        for (i = knownargc; i--; ) {
            jsval& arg = stackval(0 - (i + 1));
            *argp = get(&arg);

            argtype = known->argtypes[i];
            if (argtype == 'd' || argtype == 'i') {
                if (!isNumber(arg))
                    goto next_specialization;
                if (argtype == 'i')
                    *argp = f2i(*argp);
            } else if (argtype == 'o') {
                if (JSVAL_IS_PRIMITIVE(arg))
                    goto next_specialization;
            } else if (argtype == 's') {
                if (!JSVAL_IS_STRING(arg))
                    goto next_specialization;
            } else if (argtype == 'r') {
                if (!VALUE_IS_REGEXP(cx, arg))
                    goto next_specialization;
            } else if (argtype == 'f') {
                if (!VALUE_IS_FUNCTION(cx, arg))
                    goto next_specialization;
            } else if (argtype == 'v') {
                box_jsval(arg, *argp);
            } else {
                goto next_specialization;
            }
            argp--;
        }
        goto success;

next_specialization:;
    } while ((known++)->flags & JSTN_MORE);

    if (!constructing)
        ABORT_TRACE("unknown native");
    if (!(fun->flags & JSFUN_TRACEABLE) && FUN_CLASP(fun))
        ABORT_TRACE("can't trace native constructor");
    ABORT_TRACE("can't trace unknown constructor");

success:
#if defined _DEBUG
    JS_ASSERT(args[0] != (LIns *)0xcdcdcdcd);
#endif

    if (JSTN_ERRTYPE(known) == FAIL_STATUS) {
        // This needs to capture the pre-call state of the stack. So do not set
        // pendingTraceableNative before taking this snapshot.
        JS_ASSERT(!pendingTraceableNative);

        // Take snapshot for deep LeaveTree and store it in cx->bailExit.
        LIns* rec_ins = snapshot(DEEP_BAIL_EXIT);
        GuardRecord* rec = (GuardRecord *) rec_ins->payload();
        JS_ASSERT(rec->exit);
        lir->insStorei(INS_CONSTPTR(rec->exit), cx_ins, offsetof(JSContext, bailExit));

        // Tell nanojit not to discard or defer stack writes before this call.
        lir->insGuard(LIR_xbarrier, rec_ins, rec_ins);
    }

    LIns* res_ins = lir->insCall(known->builtin, args);
    if (!constructing)
        rval_ins = res_ins;
    switch (JSTN_ERRTYPE(known)) {
      case FAIL_NULL:
        guard(false, lir->ins_eq0(res_ins), OOM_EXIT);
        break;
      case FAIL_NEG:
      {
        res_ins = lir->ins1(LIR_i2f, res_ins);
        guard(false, lir->ins2(LIR_flt, res_ins, lir->insImmq(0)), OOM_EXIT);
        break;
      }
      case FAIL_VOID:
        guard(false, lir->ins2i(LIR_eq, res_ins, JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID)), OOM_EXIT);
        break;
      case FAIL_COOKIE:
        guard(false, lir->ins2(LIR_eq, res_ins, INS_CONST(JSVAL_ERROR_COOKIE)), OOM_EXIT);
        break;
      default:;
    }
    set(&fval, res_ins);

    if (!constructing) {
        /*
         * The return value will be processed by FastNativeCallComplete since
         * we have to know the actual return value type for calls that return
         * jsval (like Array_p_pop).
         */
        pendingTraceableNative = known;
    }

    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NEW()
{
    return functionCall(true, GET_ARGC(cx->fp->regs->pc));
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DELNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DELPROP()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DELELEM()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TYPEOF()
{
    jsval& r = stackval(-1);
    LIns* type;
    if (JSVAL_IS_STRING(r)) {
        type = INS_CONSTPTR(ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_STRING]));
    } else if (isNumber(r)) {
        type = INS_CONSTPTR(ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_NUMBER]));
    } else {
        LIns* args[] = { get(&r), cx_ins };
        if (JSVAL_TAG(r) == JSVAL_BOOLEAN) {
            // We specialize identically for boolean and undefined. We must not have a hole here.
            // Pass the unboxed type here, since TypeOfBoolean knows how to handle it.
            JS_ASSERT(r == JSVAL_TRUE || r == JSVAL_FALSE || r == JSVAL_VOID);
            type = lir->insCall(&js_TypeOfBoolean_ci, args);
        } else {
            JS_ASSERT(JSVAL_TAG(r) == JSVAL_OBJECT);
            type = lir->insCall(&js_TypeOfObject_ci, args);
        }
    }
    set(&r, type);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_VOID()
{
    stack(-1, INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INCNAME()
{
    return incName(1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INCPROP()
{
    return incProp(1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INCELEM()
{
    return incElem(1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DECNAME()
{
    return incName(-1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DECPROP()
{
    return incProp(-1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DECELEM()
{
    return incElem(-1);
}

JS_REQUIRES_STACK bool
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

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NAMEINC()
{
    return incName(1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_PROPINC()
{
    return incProp(1, false);
}

// XXX consolidate with record_JSOP_GETELEM code...
JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ELEMINC()
{
    return incElem(1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NAMEDEC()
{
    return incName(-1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_PROPDEC()
{
    return incProp(-1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ELEMDEC()
{
    return incElem(-1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETPROP()
{
    return getProp(stackval(-1));
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETPROP()
{
    jsval& l = stackval(-2);
    if (JSVAL_IS_PRIMITIVE(l))
        ABORT_TRACE("primitive this for SETPROP");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj->map->ops->setProperty != js_SetProperty)
        ABORT_TRACE("non-native JSObjectOps::setProperty");
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_SetPropHit(JSPropCacheEntry* entry, JSScopeProperty* sprop)
{
    jsbytecode* pc = cx->fp->regs->pc;
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(l));
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);

    if (!isValidSlot(OBJ_SCOPE(obj), sprop))
        return false;

    if (obj == globalObj) {
        JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj)));
        uint32 slot = sprop->slot;
        if (!lazilyImportGlobalSlot(slot))
            ABORT_TRACE("lazy import of global slot failed");

        LIns* r_ins = get(&r);

        if (JSVAL_IS_OBJECT(r)) {
            /*
             * Writing a function into the global object might rebrand it. We don't trace
             * that case.
             */
            if (VALUE_IS_FUNCTION(cx, r))
                ABORT_TRACE("potential rebranding of the global object");

            /*
             * If a regular object was written, we have to guard that it's not a function
             * at execution time either. FIXME: We should split function and object into
             * separate types when on trace (bug 481273).
             */
            guardClass(obj, obj_ins, &js_FunctionClass, snapshot(MISMATCH_EXIT));
            set(&STOBJ_GET_SLOT(obj, slot), r_ins);
        } else {
            set(&STOBJ_GET_SLOT(obj, slot), r_ins);
        }

        JS_ASSERT(*pc != JSOP_INITPROP);
        if (pc[JSOP_SETPROP_LENGTH] != JSOP_POP)
            set(&l, r_ins);
        return true;
    }

    // The global object's shape is guarded at trace entry, all others need a guard here.
    LIns* map_ins = lir->insLoad(LIR_ldp, obj_ins, (int)offsetof(JSObject, map));
    LIns* ops_ins;
    if (!map_is_native(obj->map, map_ins, ops_ins, offsetof(JSObjectOps, setProperty)))
        ABORT_TRACE("non-native map");

    LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)), "shape");
    guard(true, addName(lir->ins2i(LIR_eq, shape_ins, entry->kshape), "guard(shape)"),
          BRANCH_EXIT);

    if (entry->kshape != PCVCAP_SHAPE(entry->vcap)) {
        LIns* args[] = { INS_CONSTPTR(sprop), obj_ins, cx_ins };
        LIns* ok_ins = lir->insCall(&js_AddProperty_ci, args);
        guard(false, lir->ins_eq0(ok_ins), OOM_EXIT);
    }

    LIns* dslots_ins = NULL;
    LIns* v_ins = get(&r);
    LIns* boxed_ins = v_ins;
    box_jsval(r, boxed_ins);
    if (!native_set(obj_ins, sprop, dslots_ins, boxed_ins))
        return false;

    if (*pc != JSOP_INITPROP && pc[JSOP_SETPROP_LENGTH] != JSOP_POP)
        set(&l, v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_SetPropMiss(JSPropCacheEntry* entry)
{
    if (entry->kpc != cx->fp->regs->pc || !PCVAL_IS_SPROP(entry->vword))
        ABORT_TRACE("can't trace uncacheable property set");

    JSScopeProperty* sprop = PCVAL_TO_SPROP(entry->vword);

#ifdef DEBUG
    jsval& l = stackval(-2);
    JSObject* obj = JSVAL_TO_OBJECT(l);
    JSScope* scope = OBJ_SCOPE(obj);
    JS_ASSERT(scope->object == obj);
    JS_ASSERT(scope->shape == PCVCAP_SHAPE(entry->vcap));
    JS_ASSERT(SCOPE_HAS_PROPERTY(scope, sprop));
#endif

    return record_SetPropHit(entry, sprop);
}

/* Functions used by JSOP_GETELEM. */

static JSBool
GetProperty(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    jsid id;

    JS_ASSERT_NOT_ON_TRACE(cx);
    JS_ASSERT(cx->fp->imacpc && argc == 1);
    argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_STRING(argv[0]));
    if (!js_ValueToStringId(cx, argv[0], &id))
        return JS_FALSE;
    argv[0] = ID_TO_VALUE(id);
    return OBJ_GET_PROPERTY(cx, JS_THIS_OBJECT(cx, vp), id, &JS_RVAL(cx, vp));
}

static jsval FASTCALL
GetProperty_tn(JSContext *cx, jsbytecode *pc, JSObject *obj, JSString *name)
{
    JSAutoTempIdRooter idr(cx);
    JSAutoTempValueRooter tvr(cx);

    if (!js_ValueToStringId(cx, STRING_TO_JSVAL(name), idr.addr()) ||
        !OBJ_GET_PROPERTY(cx, obj, idr.id(), tvr.addr())) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
        *tvr.addr() = JSVAL_ERROR_COOKIE;
    }
    return tvr.value();
}

static JSBool
GetElement(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    jsid id;

    JS_ASSERT_NOT_ON_TRACE(cx);
    JS_ASSERT(cx->fp->imacpc && argc == 1);
    argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_NUMBER(argv[0]));
    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    argv[0] = ID_TO_VALUE(id);
    return OBJ_GET_PROPERTY(cx, JS_THIS_OBJECT(cx, vp), id, &JS_RVAL(cx, vp));
}

static jsval FASTCALL
GetElement_tn(JSContext* cx, jsbytecode *pc, JSObject* obj, int32 index)
{
    JSAutoTempValueRooter tvr(cx);
    JSAutoTempIdRooter idr(cx);

    if (!js_Int32ToId(cx, index, idr.addr())) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
        return JSVAL_ERROR_COOKIE;
    }
    if (!OBJ_GET_PROPERTY(cx, obj, idr.id(), tvr.addr())) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
        *tvr.addr() = JSVAL_ERROR_COOKIE;
    }
    return tvr.value();
}

JS_DEFINE_TRCINFO_1(GetProperty,
    (4, (static, JSVAL_FAIL,    GetProperty_tn, CONTEXT, PC, THIS, STRING,      0, 0)))
JS_DEFINE_TRCINFO_1(GetElement,
    (4, (extern, JSVAL_FAIL,    GetElement_tn,  CONTEXT, PC, THIS, INT32,       0, 0)))

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETELEM()
{
    jsval& idx = stackval(-1);
    jsval& lval = stackval(-2);

    LIns* obj_ins = get(&lval);
    LIns* idx_ins = get(&idx);

    // Special case for array-like access of strings.
    if (JSVAL_IS_STRING(lval) && isInt32(idx)) {
        int i = asInt32(idx);
        if (size_t(i) >= JSSTRING_LENGTH(JSVAL_TO_STRING(lval)))
            ABORT_TRACE("Invalid string index in JSOP_GETELEM");
        idx_ins = makeNumberInt32(idx_ins);
        LIns* args[] = { idx_ins, obj_ins, cx_ins };
        LIns* unitstr_ins = lir->insCall(&js_String_getelem_ci, args);
        guard(false, lir->ins_eq0(unitstr_ins), MISMATCH_EXIT);
        set(&lval, unitstr_ins);
        return true;
    }

    if (JSVAL_IS_PRIMITIVE(lval))
        ABORT_TRACE("JSOP_GETLEM on a primitive");

    JSObject* obj = JSVAL_TO_OBJECT(lval);
    jsval id;
    LIns* v_ins;

    /* Property access using a string name or something we have to stringify. */
    if (!JSVAL_IS_INT(idx)) {
        if (!JSVAL_IS_PRIMITIVE(idx))
            ABORT_TRACE("non-primitive index");
        // If index is not a string, turn it into a string.
        if (!js_InternNonIntElementId(cx, obj, idx, &id))
            ABORT_TRACE("failed to intern non-int element id");
        set(&idx, stringify(idx));

        // Store the interned string to the stack to save the interpreter from redoing this work.
        idx = ID_TO_VALUE(id);

        // The object is not guaranteed to be a dense array at this point, so it might be the
        // global object, which we have to guard against.
        if (!guardNotGlobalObject(obj, obj_ins))
            return false;

        return call_imacro(getelem_imacros.getprop);
    }

    // Invalid dense array index or not a dense array.
    if (JSVAL_TO_INT(idx) < 0 || !OBJ_IS_DENSE_ARRAY(cx, obj)) {
        if (!guardNotGlobalObject(obj, obj_ins))
            return false;

        return call_imacro(getelem_imacros.getelem);
    }

    // Fast path for dense arrays accessed with a non-negative integer index.
    jsval* vp;
    LIns* addr_ins;
    if (!elem(lval, idx, vp, v_ins, addr_ins))
        return false;
    set(&lval, v_ins);
    return true;
}

/* Functions used by JSOP_SETELEM */

static JSBool
SetProperty(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    jsid id;

    JS_ASSERT(argc == 2);
    argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_STRING(argv[0]));
    if (!js_ValueToStringId(cx, argv[0], &id))
        return JS_FALSE;
    argv[0] = ID_TO_VALUE(id);
    if (!OBJ_SET_PROPERTY(cx, JS_THIS_OBJECT(cx, vp), id, &argv[1]))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool FASTCALL
SetProperty_tn(JSContext* cx, JSObject* obj, JSString* idstr, jsval v)
{
    JSAutoTempValueRooter tvr(cx, v);
    JSAutoTempIdRooter idr(cx);

    if (!js_ValueToStringId(cx, STRING_TO_JSVAL(idstr), idr.addr()) ||
        !OBJ_SET_PROPERTY(cx, obj, idr.id(), tvr.addr())) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
    }
    return JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID);
}

static JSBool
SetElement(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    jsid id;

    JS_ASSERT(argc == 2);
    argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_NUMBER(argv[0]));
    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    argv[0] = ID_TO_VALUE(id);
    if (!OBJ_SET_PROPERTY(cx, JS_THIS_OBJECT(cx, vp), id, &argv[1]))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool FASTCALL
SetElement_tn(JSContext* cx, JSObject* obj, int32 index, jsval v)
{
    JSAutoTempIdRooter idr(cx);
    JSAutoTempValueRooter tvr(cx, v);

    if (!js_Int32ToId(cx, index, idr.addr()) ||
        !OBJ_SET_PROPERTY(cx, obj, idr.id(), tvr.addr())) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
    }
    return JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID);
}

JS_DEFINE_TRCINFO_1(SetProperty,
    (4, (extern, BOOL_FAIL,     SetProperty_tn, CONTEXT, THIS, STRING, JSVAL,   0, 0)))
JS_DEFINE_TRCINFO_1(SetElement,
    (4, (extern, BOOL_FAIL,     SetElement_tn,  CONTEXT, THIS, INT32, JSVAL,    0, 0)))

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETELEM()
{
    jsval& v = stackval(-1);
    jsval& idx = stackval(-2);
    jsval& lval = stackval(-3);

    /* no guards for type checks, trace specialized this already */
    if (JSVAL_IS_PRIMITIVE(lval))
        ABORT_TRACE("left JSOP_SETELEM operand is not an object");

    JSObject* obj = JSVAL_TO_OBJECT(lval);
    LIns* obj_ins = get(&lval);
    LIns* idx_ins = get(&idx);
    LIns* v_ins = get(&v);
    jsid id;

    if (!JSVAL_IS_INT(idx)) {
        // If index is not a string, turn it into a string.
        if (!js_InternNonIntElementId(cx, obj, idx, &id))
            ABORT_TRACE("failed to intern non-int element id");
        set(&idx, stringify(idx));

        // Store the interned string to the stack to save the interpreter from redoing this work.
        idx = ID_TO_VALUE(id);

        // The object is not guaranteed to be a dense array at this point, so it might be the
        // global object, which we have to guard against.
        if (!guardNotGlobalObject(obj, obj_ins))
            return false;

        return call_imacro((*cx->fp->regs->pc == JSOP_INITELEM)
                           ? initelem_imacros.initprop
                           : setelem_imacros.setprop);
    }

    if (JSVAL_TO_INT(idx) < 0 || !OBJ_IS_DENSE_ARRAY(cx, obj)) {
        if (!guardNotGlobalObject(obj, obj_ins))
            return false;

        return call_imacro((*cx->fp->regs->pc == JSOP_INITELEM)
                           ? initelem_imacros.initelem
                           : setelem_imacros.setelem);
    }

    // Make sure the array is actually dense.
    if (!guardDenseArray(obj, obj_ins, BRANCH_EXIT))
        return false;

    // Fast path for dense arrays accessed with a non-negative integer index. In case the trace
    // calculated the index using the FPU, force it to be an integer.
    idx_ins = makeNumberInt32(idx_ins);

    // Box the value so we can use one builtin instead of having to add one builtin for every
    // storage type.
    LIns* boxed_v_ins = v_ins;
    box_jsval(v, boxed_v_ins);

    LIns* args[] = { boxed_v_ins, idx_ins, obj_ins, cx_ins };
    LIns* res_ins = lir->insCall(&js_Array_dense_setelem_ci, args);
    guard(false, lir->ins_eq0(res_ins), MISMATCH_EXIT);

    jsbytecode* pc = cx->fp->regs->pc;
    if (*pc == JSOP_SETELEM && pc[JSOP_SETELEM_LENGTH] != JSOP_POP)
        set(&lval, v_ins);

    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLNAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj) {
        jsval* vp;
        if (!activeCallOrGlobalSlot(obj, vp))
            return false;
        stack(0, get(vp));
        stack(1, globalObj_ins);
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

    stack(0, INS_CONSTPTR(PCVAL_TO_OBJECT(pcval)));
    stack(1, obj_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETUPVAR()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLUPVAR()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::guardCallee(jsval& callee)
{
    JS_ASSERT(VALUE_IS_FUNCTION(cx, callee));

    LIns* exit = snapshot(BRANCH_EXIT);
    JSObject* callee_obj = JSVAL_TO_OBJECT(callee);
    LIns* callee_ins = get(&callee);

    /*
     * NB: The following guard guards at runtime that the callee is a
     * function. Even if the given value is an object that doesn't have
     * a private slot, the value we're matching against is not forgeable.
     */
    guard(true,
          lir->ins2(LIR_eq,
                    lir->ins2(LIR_piand,
                              stobj_get_fslot(callee_ins, JSSLOT_PRIVATE),
                              INS_CONSTPTR((void*)(~JSVAL_INT))),
                    INS_CONSTPTR(OBJ_GET_PRIVATE(cx, callee_obj))),
          exit);
    guard(true,
          lir->ins2(LIR_eq,
                    stobj_get_fslot(callee_ins, JSSLOT_PARENT),
                    INS_CONSTPTR(OBJ_GET_PARENT(cx, callee_obj))),
          exit);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::interpretedFunctionCall(jsval& fval, JSFunction* fun, uintN argc, bool constructing)
{
    if (JS_GetGlobalForObject(cx, JSVAL_TO_OBJECT(fval)) != globalObj)
        ABORT_TRACE("JSOP_CALL or JSOP_NEW crosses global scopes");

    JSStackFrame* fp = cx->fp;

    // TODO: track the copying via the tracker...
    if (argc < fun->nargs &&
        jsuword(fp->regs->sp + (fun->nargs - argc)) > cx->stackPool.current->limit) {
        ABORT_TRACE("can't trace calls with too few args requiring argv move");
    }

    // Generate a type map for the outgoing frame and stash it in the LIR
    unsigned stackSlots = js_NativeStackSlots(cx, 0/*callDepth*/);
    if (sizeof(FrameInfo) + stackSlots * sizeof(uint8) > MAX_SKIP_BYTES)
        ABORT_TRACE("interpreted function call requires saving too much stack");
    LIns* data = lir->skip(sizeof(FrameInfo) + stackSlots * sizeof(uint8));
    FrameInfo* fi = (FrameInfo*)data->payload();
    uint8* typemap = (uint8 *)(fi + 1);
    uint8* m = typemap;
    /* Determine the type of a store by looking at the current type of the actual value the
       interpreter is using. For numbers we have to check what kind of store we used last
       (integer or double) to figure out what the side exit show reflect in its typemap. */
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0/*callDepth*/,
        *m++ = determineSlotType(vp);
    );

    if (argc >= 0x8000)
        ABORT_TRACE("too many arguments");

    fi->callee = JSVAL_TO_OBJECT(fval);
    fi->block = fp->blockChain;
    fi->pc = fp->regs->pc;
    fi->imacpc = fp->imacpc;
    fi->s.spdist = fp->regs->sp - fp->slots;
    fi->s.argc = argc | (constructing ? 0x8000 : 0);

    unsigned callDepth = getCallDepth();
    if (callDepth >= treeInfo->maxCallDepth)
        treeInfo->maxCallDepth = callDepth + 1;

    lir->insStorei(INS_CONSTPTR(fi), lirbuf->rp, callDepth * sizeof(FrameInfo*));

    atoms = fun->u.i.script->atomMap.vector;
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALL()
{
    return functionCall(false, GET_ARGC(cx->fp->regs->pc));
}

static jsbytecode* apply_imacro_table[] = {
    apply_imacros.apply0,
    apply_imacros.apply1,
    apply_imacros.apply2,
    apply_imacros.apply3,
    apply_imacros.apply4,
    apply_imacros.apply5,
    apply_imacros.apply6,
    apply_imacros.apply7,
    apply_imacros.apply8
};

static jsbytecode* call_imacro_table[] = {
    apply_imacros.call0,
    apply_imacros.call1,
    apply_imacros.call2,
    apply_imacros.call3,
    apply_imacros.call4,
    apply_imacros.call5,
    apply_imacros.call6,
    apply_imacros.call7,
    apply_imacros.call8
};

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_APPLY()
{
    JSStackFrame* fp = cx->fp;
    jsbytecode *pc = fp->regs->pc;
    uintN argc = GET_ARGC(pc);
    jsval* vp = fp->regs->sp - (argc + 2);
    JS_ASSERT(vp >= StackBase(fp));
    jsuint length = 0;
    JSObject* aobj = NULL;
    LIns* aobj_ins = NULL;

    JS_ASSERT(!fp->imacpc);

    if (!VALUE_IS_FUNCTION(cx, vp[0]))
        return record_JSOP_CALL();

    JSObject* obj = JSVAL_TO_OBJECT(vp[0]);
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, obj);
    if (FUN_INTERPRETED(fun))
        return record_JSOP_CALL();

    bool apply = (JSFastNative)fun->u.n.native == js_fun_apply;
    if (!apply && (JSFastNative)fun->u.n.native != js_fun_call)
        return record_JSOP_CALL();

    /*
     * We don't trace apply and call with a primitive 'this', which is the
     * first positional parameter.
     */
    if (argc > 0 && JSVAL_IS_PRIMITIVE(vp[2]))
        return record_JSOP_CALL();

    /*
     * Guard on the identity of this, which is the function we are applying.
     */
    if (!VALUE_IS_FUNCTION(cx, vp[1]))
        ABORT_TRACE("callee is not a function");
    if (!guardCallee(vp[1]))
        return false;

    if (apply && argc >= 2) {
        if (argc != 2)
            ABORT_TRACE("apply with excess arguments");
        if (JSVAL_IS_PRIMITIVE(vp[3]))
            ABORT_TRACE("arguments parameter of apply is primitive");
        aobj = JSVAL_TO_OBJECT(vp[3]);
        aobj_ins = get(&vp[3]);

        /*
         * We expect a dense array for the arguments (the other
         * frequent case is the arguments object, but that we
         * don't trace at the moment).
         */
        if (!guardDenseArray(aobj, aobj_ins))
            ABORT_TRACE("arguments parameter of apply is not a dense array");

        /*
         * We trace only apply calls with a certain number of arguments.
         */
        length = jsuint(aobj->fslots[JSSLOT_ARRAY_LENGTH]);
        if (length >= JS_ARRAY_LENGTH(apply_imacro_table))
            ABORT_TRACE("too many arguments to apply");

        /*
         * Make sure the array has the same length at runtime.
         */
        guard(true,
              lir->ins2i(LIR_eq,
                         stobj_get_fslot(aobj_ins, JSSLOT_ARRAY_LENGTH),
                         length),
              BRANCH_EXIT);

        return call_imacro(apply_imacro_table[length]);
    }

    if (argc >= JS_ARRAY_LENGTH(call_imacro_table))
        ABORT_TRACE("too many arguments to call");

    return call_imacro(call_imacro_table[argc]);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_FastNativeCallComplete()
{
    JS_ASSERT(pendingTraceableNative);

    /* At this point the generated code has already called the native function
       and we can no longer fail back to the original pc location (JSOP_CALL)
       because that would cause the interpreter to re-execute the native
       function, which might have side effects.

       Instead, snapshot(), which is invoked from unbox_jsval() below, will see
       that we are currently parked on a traceable native's JSOP_CALL
       instruction, and it will advance the pc to restore by the length of the
       current opcode.  If the native's return type is jsval, snapshot() will
       also indicate in the type map that the element on top of the stack is a
       boxed value which doesn't need to be boxed if the type guard generated
       by unbox_jsval() fails. */

    if (JSTN_ERRTYPE(pendingTraceableNative) == FAIL_STATUS) {
#ifdef DEBUG
        // Keep cx->bailExit null when it's invalid.
        lir->insStorei(INS_CONSTPTR(NULL), cx_ins, (int) offsetof(JSContext, bailExit));
#endif
        guard(true,
              lir->ins_eq0(
                  lir->insLoad(LIR_ld, cx_ins, (int) offsetof(JSContext, builtinStatus))),
              STATUS_EXIT);
    }

    JS_ASSERT(*cx->fp->regs->pc == JSOP_CALL ||
              *cx->fp->regs->pc == JSOP_APPLY);

    jsval& v = stackval(-1);
    LIns* v_ins = get(&v);

    bool ok = true;
    if (pendingTraceableNative->flags & JSTN_UNBOX_AFTER) {
        unbox_jsval(v, v_ins);
        set(&v, v_ins);
    } else if (JSTN_ERRTYPE(pendingTraceableNative) == FAIL_NEG) {
        /* Already added i2f in functionCall. */
        JS_ASSERT(JSVAL_IS_NUMBER(v));
    } else {
        /* Convert the result to double if the builtin returns int32. */
        if (JSVAL_IS_NUMBER(v) &&
            (pendingTraceableNative->builtin->_argtypes & 3) == nanojit::ARGSIZE_LO) {
            set(&v, lir->ins1(LIR_i2f, v_ins));
        }
    }

    // We'll null pendingTraceableNative in monitorRecording, on the next op cycle.
    // There must be a next op since the stack is non-empty.
    return ok;
}

JS_REQUIRES_STACK bool
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
        ABORT_TRACE("named property not found");

    if (!lazilyImportGlobalSlot(slot))
        ABORT_TRACE("lazy import of global slot failed");

    vp = &STOBJ_GET_SLOT(obj, slot);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::prop(JSObject* obj, LIns* obj_ins, uint32& slot, LIns*& v_ins)
{
    /*
     * Can't specialize to assert obj != global, must guard to avoid aliasing
     * stale homes of stacked global variables.
     */
    if (!guardNotGlobalObject(obj, obj_ins))
        return false;

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
        /*
         * This trace will be valid as long as neither the object nor any object
         * on its prototype chain change shape.
         */
        LIns* exit = snapshot(BRANCH_EXIT);
        for (;;) {
            LIns* map_ins = lir->insLoad(LIR_ldp, obj_ins, (int)offsetof(JSObject, map));
            LIns* ops_ins;
            if (map_is_native(obj->map, map_ins, ops_ins)) {
                LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)),
                                          "shape");
                guard(true,
                      addName(lir->ins2i(LIR_eq, shape_ins, OBJ_SHAPE(obj)), "guard(shape)"),
                      exit);
            } else if (!guardDenseArray(obj, obj_ins, BRANCH_EXIT))
                ABORT_TRACE("non-native object involved in undefined property access");

            obj = JSVAL_TO_OBJECT(obj->fslots[JSSLOT_PROTO]);
            if (!obj)
                break;
            obj_ins = stobj_get_fslot(obj_ins, JSSLOT_PROTO);
        }

        v_ins = INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
        JS_ASSERT(cs.ndefs == 1);
        stack(-cs.nuses, v_ins);
        slot = SPROP_INVALID_SLOT;
        return true;
    }

    /* Insist if setting on obj being the directly addressed object. */
    uint32 setflags = (cs.format & (JOF_SET | JOF_INCDEC | JOF_FOR));
    LIns* dslots_ins = NULL;

    /* Don't trace getter or setter calls, our caller wants a direct slot. */
    if (PCVAL_IS_SPROP(pcval)) {
        JSScopeProperty* sprop = PCVAL_TO_SPROP(pcval);

        if (setflags && !SPROP_HAS_STUB_SETTER(sprop))
            ABORT_TRACE("non-stub setter");
        if (setflags && (sprop->attrs & JSPROP_READONLY))
            ABORT_TRACE("writing to a readonly property");
        if (setflags != JOF_SET && !SPROP_HAS_STUB_GETTER(sprop)) {
            // FIXME 450335: generalize this away from regexp built-in getters.
            if (setflags == 0 &&
                sprop->getter == js_RegExpClass.getProperty &&
                sprop->shortid < 0) {
                if (sprop->shortid == REGEXP_LAST_INDEX)
                    ABORT_TRACE("can't trace regexp.lastIndex yet");
                LIns* args[] = { INS_CONSTPTR(sprop), obj_ins, cx_ins };
                v_ins = lir->insCall(&js_CallGetter_ci, args);
                guard(false, lir->ins2(LIR_eq, v_ins, INS_CONST(JSVAL_ERROR_COOKIE)), OOM_EXIT);
                unbox_jsval((sprop->shortid == REGEXP_SOURCE) ? JSVAL_STRING : JSVAL_BOOLEAN,
                             v_ins);
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

    v_ins = stobj_get_slot(obj_ins, slot, dslots_ins);
    unbox_jsval(STOBJ_GET_SLOT(obj, slot), v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::elem(jsval& oval, jsval& idx, jsval*& vp, LIns*& v_ins, LIns*& addr_ins)
{
    /* no guards for type checks, trace specialized this already */
    if (JSVAL_IS_PRIMITIVE(oval) || !JSVAL_IS_INT(idx))
        return false;

    JSObject* obj = JSVAL_TO_OBJECT(oval);
    LIns* obj_ins = get(&oval);

    /* make sure the object is actually a dense array */
    if (!guardDenseArray(obj, obj_ins))
        return false;

    /* check that the index is within bounds */
    jsint i = JSVAL_TO_INT(idx);
    LIns* idx_ins = makeNumberInt32(get(&idx));

    LIns* dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    if (!guardDenseArrayIndex(obj, i, obj_ins, dslots_ins, idx_ins, BRANCH_EXIT)) {
        /*
         * If we read a hole, make sure at recording time and at runtime that nothing along
         * the prototype has numeric properties.
         */
        if (js_PrototypeHasIndexedProperties(cx, obj))
            return false;

        LIns* exit = snapshot(BRANCH_EXIT);
        while ((obj = JSVAL_TO_OBJECT(obj->fslots[JSSLOT_PROTO])) != NULL) {
            obj_ins = stobj_get_fslot(obj_ins, JSSLOT_PROTO);
            LIns* map_ins = lir->insLoad(LIR_ldp, obj_ins, (int)offsetof(JSObject, map));
            LIns* ops_ins;
            if (!map_is_native(obj->map, map_ins, ops_ins))
                ABORT_TRACE("non-native object involved along prototype chain");

            LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)),
                                      "shape");
            guard(true,
                  addName(lir->ins2i(LIR_eq, shape_ins, OBJ_SHAPE(obj)), "guard(shape)"),
                  exit);
        }

        // Return undefined and indicate that we didn't actually read this (addr_ins).
        v_ins = lir->insImm(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
        addr_ins = NULL;
        return true;
    }

    // We can't "see through" a hole to a possible Array.prototype property, so
    // we abort here and guard below (after unboxing).
    vp = &obj->dslots[i];
    if (*vp == JSVAL_HOLE)
        ABORT_TRACE("can't see through hole in dense array");

    addr_ins = lir->ins2(LIR_piadd, dslots_ins,
                         lir->ins2i(LIR_pilsh, idx_ins, (sizeof(jsval) == 4) ? 2 : 3));

    /* Load the value and guard on its type to unbox it. */
    v_ins = lir->insLoad(LIR_ldp, addr_ins, 0);
    unbox_jsval(*vp, v_ins);

    if (JSVAL_TAG(*vp) == JSVAL_BOOLEAN) {
        // Optimize to guard for a hole only after untagging, so we know that
        // we have a boolean, to avoid an extra guard for non-boolean values.
        guard(false, lir->ins2(LIR_eq, v_ins, INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_HOLE))),
              MISMATCH_EXIT);
    }
    return true;
}

JS_REQUIRES_STACK bool
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

JS_REQUIRES_STACK bool
TraceRecorder::getProp(jsval& v)
{
    if (JSVAL_IS_PRIMITIVE(v))
        ABORT_TRACE("primitive lhs");

    return getProp(JSVAL_TO_OBJECT(v), get(&v));
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NAME()
{
    jsval* vp;
    if (!name(vp))
        return false;
    stack(0, get(vp));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DOUBLE()
{
    jsval v = jsval(atoms[GET_INDEX(cx->fp->regs->pc)]);
    stack(0, lir->insImmf(*JSVAL_TO_DOUBLE(v)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_STRING()
{
    JSAtom* atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    JS_ASSERT(ATOM_IS_STRING(atom));
    stack(0, INS_CONSTPTR(ATOM_TO_STRING(atom)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ZERO()
{
    stack(0, lir->insImmq(0));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ONE()
{
    stack(0, lir->insImmf(1));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NULL()
{
    stack(0, INS_CONSTPTR(NULL));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_THIS()
{
    LIns* this_ins;
    if (!getThis(this_ins))
        return false;
    stack(0, this_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FALSE()
{
    stack(0, lir->insImm(0));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TRUE()
{
    stack(0, lir->insImm(1));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_OR()
{
    return ifop();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_AND()
{
    return ifop();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TABLESWITCH()
{
#ifdef NANOJIT_IA32
    return true;
#else
    return switchop();
#endif
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LOOKUPSWITCH()
{
    return switchop();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_STRICTEQ()
{
    strictEquality(true, false);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_STRICTNE()
{
    strictEquality(false, false);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_OBJECT()
{
    JSStackFrame* fp = cx->fp;
    JSScript* script = fp->script;
    unsigned index = atoms - script->atomMap.vector + GET_INDEX(fp->regs->pc);

    JSObject* obj;
    JS_GET_SCRIPT_OBJECT(script, index, obj);
    stack(0, INS_CONSTPTR(obj));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_POP()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TRAP()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETARG()
{
    stack(0, arg(GET_ARGNO(cx->fp->regs->pc)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETARG()
{
    arg(GET_ARGNO(cx->fp->regs->pc), stack(-1));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETLOCAL()
{
    stack(0, var(GET_SLOTNO(cx->fp->regs->pc)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETLOCAL()
{
    var(GET_SLOTNO(cx->fp->regs->pc), stack(-1));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_UINT16()
{
    stack(0, lir->insImmf(GET_UINT16(cx->fp->regs->pc)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NEWINIT()
{
    JSProtoKey key = JSProtoKey(GET_INT8(cx->fp->regs->pc));
    JSObject* obj;
    const CallInfo *ci;

    if (!js_GetClassPrototype(cx, globalObj, INT_TO_JSID(key), &obj))
        return false;
    ci = (key == JSProto_Array) ? &js_FastNewArray_ci : &js_Object_tn_ci;
    LIns* args[] = { INS_CONSTPTR(obj), cx_ins };
    LIns* v_ins = lir->insCall(ci, args);
    guard(false, lir->ins_eq0(v_ins), OOM_EXIT);
    stack(0, v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENDINIT()
{
#ifdef DEBUG
    jsval& v = stackval(-1);
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
#endif
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INITPROP()
{
    // All the action is in record_SetPropHit.
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INITELEM()
{
    return record_JSOP_SETELEM();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFSHARP()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_USESHARP()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INCARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INCLOCAL()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), 1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DECARG()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DECLOCAL()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), -1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ARGINC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LOCALINC()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), 1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ARGDEC()
{
    return inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LOCALDEC()
{
    return inc(varval(GET_SLOTNO(cx->fp->regs->pc)), -1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IMACOP()
{
    JS_ASSERT(cx->fp->imacpc);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ITER()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(v))
        ABORT_TRACE("for-in on a primitive value");

    jsuint flags = cx->fp->regs->pc[1];

    if (hasIteratorMethod(JSVAL_TO_OBJECT(v))) {
        if (flags == JSITER_ENUMERATE)
            return call_imacro(iter_imacros.for_in);
        if (flags == (JSITER_ENUMERATE | JSITER_FOREACH))
            return call_imacro(iter_imacros.for_each);
    } else {
        if (flags == JSITER_ENUMERATE)
            return call_imacro(iter_imacros.for_in_native);
        if (flags == (JSITER_ENUMERATE | JSITER_FOREACH))
            return call_imacro(iter_imacros.for_each_native);
    }
    ABORT_TRACE("unimplemented JSITER_* flags");
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NEXTITER()
{
    jsval& iterobj_val = stackval(-2);
    if (JSVAL_IS_PRIMITIVE(iterobj_val))
        ABORT_TRACE("for-in on a primitive value");

    LIns* iterobj_ins = get(&iterobj_val);
    if (guardClass(JSVAL_TO_OBJECT(iterobj_val), iterobj_ins, &js_IteratorClass,
                   snapshot(BRANCH_EXIT))) {
        return call_imacro(nextiter_imacros.native_iter_next);
    }
    return call_imacro(nextiter_imacros.custom_iter_next);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENDITER()
{
    LIns* args[] = { stack(-2), cx_ins };
    LIns* ok_ins = lir->insCall(&js_CloseIterator_ci, args);
    guard(false, lir->ins_eq0(ok_ins), MISMATCH_EXIT);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FORNAME()
{
    jsval* vp;
    if (name(vp)) {
        set(vp, stack(-1));
        return true;
    }
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FORPROP()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FORELEM()
{
    return record_JSOP_DUP();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FORARG()
{
    return record_JSOP_SETARG();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FORLOCAL()
{
    return record_JSOP_SETLOCAL();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_POPN()
{
    return true;
}

JS_REQUIRES_STACK bool
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

    if (PCVAL_IS_NULL(pcval))
        ABORT_TRACE("JSOP_BINDNAME is trying to add a new property");

    if (obj2 != obj)
        ABORT_TRACE("JSOP_BINDNAME found a non-direct property on the global object");

    stack(0, obj_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETNAME()
{
    jsval& l = stackval(-2);
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(l));

    /*
     * Trace cases that are global code or in lightweight functions scoped by
     * the global object only.
     */
    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj != cx->fp->scopeChain || obj != globalObj)
        ABORT_TRACE("JSOP_SETNAME left operand is not the global object");

    // The rest of the work is in record_SetPropHit.
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_THROW()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IN()
{
    jsval& rval = stackval(-1);
    jsval& lval = stackval(-2);

    if (JSVAL_IS_PRIMITIVE(rval))
        ABORT_TRACE("JSOP_IN on non-object right operand");
    JSObject* obj = JSVAL_TO_OBJECT(rval);
    LIns* obj_ins = get(&rval);

    jsid id;
    LIns* x;
    if (JSVAL_IS_INT(lval)) {
        id = INT_JSVAL_TO_JSID(lval);
        LIns* args[] = { makeNumberInt32(get(&lval)), obj_ins, cx_ins };
        x = lir->insCall(&js_HasNamedPropertyInt32_ci, args);
    } else if (JSVAL_IS_STRING(lval)) {
        if (!js_ValueToStringId(cx, lval, &id))
            ABORT_TRACE("left operand of JSOP_IN didn't convert to a string-id");
        LIns* args[] = { get(&lval), obj_ins, cx_ins };
        x = lir->insCall(&js_HasNamedProperty_ci, args);
    } else {
        ABORT_TRACE("string or integer expected");
    }

    guard(false, lir->ins2i(LIR_eq, x, JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID)), OOM_EXIT);
    x = lir->ins2i(LIR_eq, x, 1);

    JSObject* obj2;
    JSProperty* prop;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        ABORT_TRACE("OBJ_LOOKUP_PROPERTY failed in JSOP_IN");
    bool cond = prop != NULL;
    if (prop)
        OBJ_DROP_PROPERTY(cx, obj2, prop);

    /* The interpreter fuses comparisons and the following branch,
       so we have to do that here as well. */
    fuseIf(cx->fp->regs->pc + 1, cond, x);

    /* We update the stack after the guard. This is safe since
       the guard bails out at the comparison and the interpreter
       will therefore re-execute the comparison. This way the
       value of the condition doesn't have to be calculated and
       saved on the stack in most cases. */
    set(&lval, x);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INSTANCEOF()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEBUGGER()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GOSUB()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_RETSUB()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_EXCEPTION()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LINENO()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CONDSWITCH()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CASE()
{
    strictEquality(true, true);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFAULT()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_EVAL()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENUMELEM()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETTER()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETTER()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFFUN()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFCONST()
{
    return false;
}

JS_REQUIRES_STACK bool
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

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ANONFUNOBJ()
{
    JSFunction* fun;
    JSFrameRegs& regs = *cx->fp->regs;
    JSScript* script = cx->fp->script;
    LOAD_FUNCTION(0); // needs script, regs, fun

    JSObject* obj = FUN_OBJECT(fun);
    if (OBJ_GET_PARENT(cx, obj) != cx->fp->scopeChain)
        ABORT_TRACE("can't trace with activation object on scopeChain");

    stack(0, INS_CONSTPTR(obj));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NAMEDFUNOBJ()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETLOCALPOP()
{
    var(GET_SLOTNO(cx->fp->regs->pc), stack(-1));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IFPRIMTOP()
{
    // Traces are type-specialized, including null vs. object, so we need do
    // nothing here. The upstream unbox_jsval called after valueOf or toString
    // from an imacro (e.g.) will fork the trace for us, allowing us to just
    // follow along mindlessly :-).
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETCALL()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TRY()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FINALLY()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NOP()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ARGSUB()
{
    JSStackFrame* fp = cx->fp;
    if (!(fp->fun->flags & JSFUN_HEAVYWEIGHT)) {
        uintN slot = GET_ARGNO(fp->regs->pc);
        if (slot < fp->fun->nargs && slot < fp->argc && !fp->argsobj) {
            stack(0, get(&cx->fp->argv[slot]));
            return true;
        }
    }
    ABORT_TRACE("can't trace JSOP_ARGSUB hard case");
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ARGCNT()
{
    if (!(cx->fp->fun->flags & JSFUN_HEAVYWEIGHT)) {
        stack(0, lir->insImmf(cx->fp->argc));
        return true;
    }
    ABORT_TRACE("can't trace heavyweight JSOP_ARGCNT");
}

JS_REQUIRES_STACK bool
TraceRecorder::record_DefLocalFunSetSlot(uint32 slot, JSObject* obj)
{
    var(slot, INS_CONSTPTR(obj));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFLOCALFUN()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GOTOX()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IFEQX()
{
    return record_JSOP_IFEQ();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_IFNEX()
{
    return record_JSOP_IFNE();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ORX()
{
    return record_JSOP_OR();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ANDX()
{
    return record_JSOP_AND();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GOSUBX()
{
    return record_JSOP_GOSUB();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CASEX()
{
    strictEquality(true, true);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFAULTX()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TABLESWITCHX()
{
#ifdef NANOJIT_IA32
    return true;
#else
    return switchop();
#endif
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LOOKUPSWITCHX()
{
    return switchop();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BACKPATCH()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BACKPATCH_POP()
{
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_THROWING()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETRVAL()
{
    // If we implement this, we need to update JSOP_STOP.
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_RETRVAL()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    stack(0, get(&STOBJ_GET_SLOT(globalObj, slot)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    set(&STOBJ_GET_SLOT(globalObj, slot), stack(-1));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INCGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(globalObj, slot), 1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DECGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(globalObj, slot), -1);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GVARINC()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(globalObj, slot), 1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GVARDEC()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    return inc(STOBJ_GET_SLOT(globalObj, slot), -1, false);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_REGEXP()
{
    return false;
}

// begin JS_HAS_XML_SUPPORT

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DEFXMLNS()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ANYNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_QNAMEPART()
{
    return record_JSOP_STRING();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_QNAMECONST()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_QNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TOATTRNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TOATTRVAL()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ADDATTRNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ADDATTRVAL()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_BINDXMLNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_SETXMLNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DESCENDANTS()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_FILTER()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENDFILTER()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TOXML()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TOXMLLIST()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLTAGEXPR()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLELTEXPR()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLOBJECT()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLCDATA()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLCOMMENT()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_XMLPI()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETFUNNS()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_STARTXML()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_STARTXMLEXPR()
{
    return false;
}

// end JS_HAS_XML_SUPPORT

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLPROP()
{
    jsval& l = stackval(-1);
    JSObject* obj;
    LIns* obj_ins;
    LIns* this_ins;
    if (!JSVAL_IS_PRIMITIVE(l)) {
        obj = JSVAL_TO_OBJECT(l);
        obj_ins = get(&l);
        this_ins = obj_ins; // |this| for subsequent call
        if (JSVAL_IS_NULL(l))
            ABORT_TRACE("callprop on null");
        if (!this_ins->isconstp())
            guard(false, lir->ins_eq0(this_ins), MISMATCH_EXIT);
    } else {
        jsint i;
        debug_only(const char* protoname = NULL;)
        if (JSVAL_IS_STRING(l)) {
            i = JSProto_String;
            debug_only(protoname = "String.prototype";)
        } else if (JSVAL_IS_NUMBER(l)) {
            i = JSProto_Number;
            debug_only(protoname = "Number.prototype";)
        } else if (JSVAL_TAG(l) == JSVAL_BOOLEAN) {
            if (l == JSVAL_VOID)
                ABORT_TRACE("callprop on void");
            guard(false, lir->ins2i(LIR_eq, get(&l), JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID)), MISMATCH_EXIT);
            i = JSProto_Boolean;
            debug_only(protoname = "Boolean.prototype";)
        } else {
            JS_ASSERT(JSVAL_IS_NULL(l) || JSVAL_IS_VOID(l));
            ABORT_TRACE("callprop on null or void");
        }

        if (!js_GetClassPrototype(cx, NULL, INT_TO_JSID(i), &obj))
            ABORT_TRACE("GetClassPrototype failed!");

        obj_ins = INS_CONSTPTR(obj);
        debug_only(obj_ins = addName(obj_ins, protoname);)
        this_ins = get(&l); // use primitive as |this|
    }

    JSObject* obj2;
    jsuword pcval;
    if (!test_property_cache(obj, obj_ins, obj2, pcval))
        return false;

    if (PCVAL_IS_NULL(pcval) || !PCVAL_IS_OBJECT(pcval))
        ABORT_TRACE("callee is not an object");
    JS_ASSERT(HAS_FUNCTION_CLASS(PCVAL_TO_OBJECT(pcval)));

    if (JSVAL_IS_PRIMITIVE(l)) {
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, PCVAL_TO_OBJECT(pcval));
        if (!PRIMITIVE_THIS_TEST(fun, l))
            ABORT_TRACE("callee does not accept primitive |this|");
    }

    stack(0, this_ins);
    stack(-1, INS_CONSTPTR(PCVAL_TO_OBJECT(pcval)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_DELDESC()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_UINT24()
{
    stack(0, lir->insImmf(GET_UINT24(cx->fp->regs->pc)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INDEXBASE()
{
    atoms += GET_INDEXBASE(cx->fp->regs->pc);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_RESETBASE()
{
    atoms = cx->fp->script->atomMap.vector;
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_RESETBASE0()
{
    atoms = cx->fp->script->atomMap.vector;
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLELEM()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_STOP()
{
    JSStackFrame *fp = cx->fp;

    if (fp->imacpc) {
        // End of imacro, so return true to the interpreter immediately. The
        // interpreter's JSOP_STOP case will return from the imacro, back to
        // the pc after the calling op, still in the same JSStackFrame.
        atoms = fp->script->atomMap.vector;
        return true;
    }

    /*
     * We know falling off the end of a constructor returns the new object that
     * was passed in via fp->argv[-1], while falling off the end of a function
     * returns undefined.
     *
     * NB: we do not support script rval (eval, API users who want the result
     * of the last expression-statement, debugger API calls).
     */
    if (fp->flags & JSFRAME_CONSTRUCTING) {
        JS_ASSERT(OBJECT_TO_JSVAL(fp->thisp) == fp->argv[-1]);
        rval_ins = get(&fp->argv[-1]);
    } else {
        rval_ins = INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
    }
    clearFrameSlotsFromCache();
    return true;
}

JS_REQUIRES_STACK bool
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

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLXMLNAME()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_TYPEOFEXPR()
{
    return record_JSOP_TYPEOF();
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENTERBLOCK()
{
    JSScript* script = cx->fp->script;
    JSFrameRegs& regs = *cx->fp->regs;
    JSObject* obj;
    JS_GET_SCRIPT_OBJECT(script, GET_FULL_INDEX(0), obj);

    LIns* void_ins = INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_VOID));
    for (int i = 0, n = OBJ_BLOCK_COUNT(cx, obj); i < n; i++)
        stack(i, void_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LEAVEBLOCK()
{
    /* We mustn't exit the lexical block we began recording in.  */
    return cx->fp->blockChain != lexicalBlock;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GENERATOR()
{
    return false;
#if 0
    JSStackFrame* fp = cx->fp;
    if (fp->callobj || fp->argsobj || fp->varobj)
        ABORT_TRACE("can't trace hard-case generator");

    // Generate a type map for the outgoing frame and stash it in the LIR
    unsigned stackSlots = js_NativeStackSlots(cx, 0/*callDepth*/);
    if (stackSlots > MAX_SKIP_BYTES)
        ABORT_TRACE("generator requires saving too much stack");
    LIns* data = lir->skip(stackSlots * sizeof(uint8));
    uint8* typemap = (uint8 *)data->payload();
    uint8* m = typemap;
    /* Determine the type of a store by looking at the current type of the actual value the
       interpreter is using. For numbers we have to check what kind of store we used last
       (integer or double) to figure out what the side exit show reflect in its typemap. */
    FORALL_SLOTS_IN_PENDING_FRAMES(cx, 0/*callDepth*/,
        *m++ = determineSlotType(vp);
    );
    FlushNativeStackFrame(cx, 0, typemap, state.???, NULL);

    LIns* args[] = { INS_CONST(fp->argc), INS_CONSTPTR(fp->callee), cx_ins };
    LIns* g_ins = lir->insCall(&js_FastNewGenerator_ci, args);
    guard(false, lir->ins_eq0(g_ins), OOM_EXIT);
    return true;
#endif
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_YIELD()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ARRAYPUSH()
{
    uint32_t slot = GET_UINT16(cx->fp->regs->pc);
    JS_ASSERT(cx->fp->script->nfixed <= slot);
    JS_ASSERT(cx->fp->slots + slot < cx->fp->regs->sp - 1);
    jsval &arrayval = cx->fp->slots[slot];
    JS_ASSERT(JSVAL_IS_OBJECT(arrayval));
    JS_ASSERT(OBJ_IS_DENSE_ARRAY(cx, JSVAL_TO_OBJECT(arrayval)));
    LIns *array_ins = get(&arrayval);
    jsval &elt = stackval(-1);
    LIns *elt_ins = get(&elt);
    box_jsval(elt, elt_ins);

    LIns *args[] = { elt_ins, array_ins, cx_ins };
    LIns *ok_ins = lir->insCall(&js_ArrayCompPush_ci, args);
    guard(false, lir->ins_eq0(ok_ins), OOM_EXIT);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_ENUMCONSTELEM()
{
    return false;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LEAVEBLOCKEXPR()
{
    LIns* v_ins = stack(-1);
    int n = -1 - GET_UINT16(cx->fp->regs->pc);
    stack(n, v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETTHISPROP()
{
    LIns* this_ins;

    /* its safe to just use cx->fp->thisp here because getThis() returns false if thisp
       is not available */
    return getThis(this_ins) && getProp(cx->fp->thisp, this_ins);
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETARGPROP()
{
    return getProp(argval(GET_ARGNO(cx->fp->regs->pc)));
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_GETLOCALPROP()
{
    return getProp(varval(GET_SLOTNO(cx->fp->regs->pc)));
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INDEXBASE1()
{
    atoms += 1 << 16;
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INDEXBASE2()
{
    atoms += 2 << 16;
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INDEXBASE3()
{
    atoms += 3 << 16;
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return true; // We will see JSOP_CALLNAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         ABORT_TRACE("lazy import of global slot failed");

    jsval& v = STOBJ_GET_SLOT(globalObj, slot);
    stack(0, get(&v));
    stack(1, INS_CONSTPTR(NULL));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLLOCAL()
{
    uintN slot = GET_SLOTNO(cx->fp->regs->pc);
    stack(0, var(slot));
    stack(1, INS_CONSTPTR(NULL));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLARG()
{
    uintN slot = GET_ARGNO(cx->fp->regs->pc);
    stack(0, arg(slot));
    stack(1, INS_CONSTPTR(NULL));
    return true;
}

/* Functions for use with JSOP_CALLBUILTIN. */

static JSBool
ObjectToIterator(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_INT(argv[0]));
    JS_SET_RVAL(cx, vp, JS_THIS(cx, vp));
    return js_ValueToIterator(cx, JSVAL_TO_INT(argv[0]), &JS_RVAL(cx, vp));
}

static JSObject* FASTCALL
ObjectToIterator_tn(JSContext* cx, jsbytecode* pc, JSObject *obj, int32 flags)
{
    jsval v = OBJECT_TO_JSVAL(obj);
    JSBool ok = js_ValueToIterator(cx, flags, &v);

    if (!ok) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
        return NULL;
    }
    return JSVAL_TO_OBJECT(v);
}

static JSBool
CallIteratorNext(JSContext *cx, uintN argc, jsval *vp)
{
    return js_CallIteratorNext(cx, JS_THIS_OBJECT(cx, vp), &JS_RVAL(cx, vp));
}

static jsval FASTCALL
CallIteratorNext_tn(JSContext* cx, jsbytecode* pc, JSObject* iterobj)
{
    JSAutoTempValueRooter tvr(cx);
    JSBool ok = js_CallIteratorNext(cx, iterobj, tvr.addr());

    if (!ok) {
        cx->builtinStatus |= JSBUILTIN_ERROR;
        return JSVAL_ERROR_COOKIE;
    }
    return tvr.value();
}

JS_DEFINE_TRCINFO_1(ObjectToIterator,
    (4, (static, OBJECT_FAIL, ObjectToIterator_tn, CONTEXT, PC, THIS, INT32, 0, 0)))
JS_DEFINE_TRCINFO_1(CallIteratorNext,
    (3, (static, JSVAL_FAIL,  CallIteratorNext_tn, CONTEXT, PC, THIS,        0, 0)))

static const struct BuiltinFunctionInfo {
    JSTraceableNative *tn;
    int nargs;
} builtinFunctionInfo[JSBUILTIN_LIMIT] = {
    {ObjectToIterator_trcinfo,   1},
    {CallIteratorNext_trcinfo,   0},
    {GetProperty_trcinfo,        1},
    {GetElement_trcinfo,         1},
    {SetProperty_trcinfo,        2},
    {SetElement_trcinfo,         2}
};

JSObject *
js_GetBuiltinFunction(JSContext *cx, uintN index)
{
    JSRuntime *rt = cx->runtime;
    JSObject *funobj = rt->builtinFunctions[index];

    if (!funobj) {
        /* Use NULL parent and atom. Builtin functions never escape to scripts. */
        JS_ASSERT(index < JS_ARRAY_LENGTH(builtinFunctionInfo));
        const BuiltinFunctionInfo *bfi = &builtinFunctionInfo[index];
        JSFunction *fun = js_NewFunction(cx,
                                         NULL,
                                         JS_DATA_TO_FUNC_PTR(JSNative, bfi->tn),
                                         bfi->nargs,
                                         JSFUN_FAST_NATIVE | JSFUN_TRACEABLE,
                                         NULL,
                                         NULL);
        if (fun) {
            funobj = FUN_OBJECT(fun);
            STOBJ_CLEAR_PROTO(funobj);
            STOBJ_CLEAR_PARENT(funobj);

            JS_LOCK_GC(rt);
            if (!rt->builtinFunctions[index])  /* retest now that the lock is held */
                rt->builtinFunctions[index] = funobj;
            else
                funobj = rt->builtinFunctions[index];
            JS_UNLOCK_GC(rt);
        }
    }
    return funobj;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_CALLBUILTIN()
{
    JSObject *obj = js_GetBuiltinFunction(cx, GET_INDEX(cx->fp->regs->pc));
    if (!obj)
        return false;

    stack(0, get(&stackval(-1)));
    stack(-1, INS_CONSTPTR(obj));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NULLTHIS()
{
    stack(0, INS_CONSTPTR(NULL));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INT8()
{
    stack(0, lir->insImmf(GET_INT8(cx->fp->regs->pc)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_INT32()
{
    stack(0, lir->insImmf(GET_INT32(cx->fp->regs->pc)));
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_LENGTH()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_STRING(l))
            ABORT_TRACE("non-string primitive JSOP_LENGTH unsupported");
        LIns* str_ins = get(&l);
        LIns* len_ins = lir->insLoad(LIR_ldp, str_ins, (int)offsetof(JSString, length));

        LIns* masked_len_ins = lir->ins2(LIR_piand,
                                         len_ins,
                                         INS_CONSTPTR(reinterpret_cast<void *>(JSSTRING_LENGTH_MASK)));

        LIns* choose_len_ins =
            lir->ins_choose(lir->ins_eq0(lir->ins2(LIR_piand,
                                                   len_ins,
                                                   INS_CONSTPTR(reinterpret_cast<void *>(JSSTRFLAG_DEPENDENT)))),
                            masked_len_ins,
                            lir->ins_choose(lir->ins_eq0(lir->ins2(LIR_piand,
                                                                   len_ins,
                                                                   INS_CONSTPTR(reinterpret_cast<void *>(JSSTRFLAG_PREFIX)))),
                                            lir->ins2(LIR_piand,
                                                      len_ins,
                                                      INS_CONSTPTR(reinterpret_cast<void *>(JSSTRDEP_LENGTH_MASK))),
                                            masked_len_ins));

        set(&l, lir->ins1(LIR_i2f, choose_len_ins));
        return true;
    }

    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
    LIns* v_ins;
    if (OBJ_IS_ARRAY(cx, obj)) {
        if (OBJ_IS_DENSE_ARRAY(cx, obj)) {
            if (!guardDenseArray(obj, obj_ins, BRANCH_EXIT))
                JS_NOT_REACHED("OBJ_IS_DENSE_ARRAY but not?!?");
        } else {
            if (!guardClass(obj, obj_ins, &js_SlowArrayClass, snapshot(BRANCH_EXIT)))
                ABORT_TRACE("can't trace length property access on non-array");
        }
        v_ins = lir->ins1(LIR_i2f, stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH));
    } else {
        if (!OBJ_IS_NATIVE(obj))
            ABORT_TRACE("can't trace length property access on non-array, non-native object");
        return getProp(obj, obj_ins);
    }
    set(&l, v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_NEWARRAY()
{
    JSObject* proto;
    const CallInfo* ci = &js_NewUninitializedArray_ci;
    if (!js_GetClassPrototype(cx, globalObj, INT_TO_JSID(JSProto_Array), &proto))
        return false;

    uint32 len = GET_UINT24(cx->fp->regs->pc);
    LIns* args[] = { lir->insImm(len), INS_CONSTPTR(proto), cx_ins };
    LIns* v_ins = lir->insCall(ci, args);
    guard(false, lir->ins_eq0(v_ins), OOM_EXIT);

    LIns* dslots_ins = NULL;
    for (uint32 i = 0; i < len; i++) {
        jsval& v = stackval(int(i) - int(len));
        LIns* elt_ins = get(&v);
        box_jsval(v, elt_ins);
        stobj_set_dslot(v_ins, i, dslots_ins, elt_ins, "set_array_elt");
    }

    stack(-int(len), v_ins);
    return true;
}

JS_REQUIRES_STACK bool
TraceRecorder::record_JSOP_HOLE()
{
    stack(0, INS_CONST(JSVAL_TO_PSEUDO_BOOLEAN(JSVAL_HOLE)));
    return true;
}

bool
TraceRecorder::record_JSOP_LOOP()
{
    return true;
}

#ifdef JS_JIT_SPEW
/* Prints information about entry typemaps and unstable exits for all peers at a PC */
void
js_DumpPeerStability(JSTraceMonitor* tm, const void* ip, uint32 globalShape)
{
    Fragment* f;
    TreeInfo* ti;
    bool looped = false;
    unsigned length = 0;

    for (f = getLoop(tm, ip, globalShape); f != NULL; f = f->peer) {
        if (!f->vmprivate)
            continue;
        printf("fragment %p:\nENTRY: ", (void*)f);
        ti = (TreeInfo*)f->vmprivate;
        if (looped)
            JS_ASSERT(ti->nStackTypes == length);
        for (unsigned i = 0; i < ti->nStackTypes; i++)
            printf("S%d ", ti->stackTypeMap()[i]);
        for (unsigned i = 0; i < ti->nGlobalTypes(); i++)
            printf("G%d ", ti->globalTypeMap()[i]);
        printf("\n");
        UnstableExit* uexit = ti->unstableExits;
        while (uexit != NULL) {
            printf("EXIT:  ");
            uint8* m = getFullTypeMap(uexit->exit);
            for (unsigned i = 0; i < uexit->exit->numStackSlots; i++)
                printf("S%d ", m[i]);
            for (unsigned i = 0; i < uexit->exit->numGlobalSlots; i++)
                printf("G%d ", m[uexit->exit->numStackSlots + i]);
            printf("\n");
            uexit = uexit->next;
        }
        length = ti->nStackTypes;
        looped = true;
    }
}
#endif

#define UNUSED(n)                                                             \
    JS_REQUIRES_STACK bool                                                    \
    TraceRecorder::record_JSOP_UNUSED##n() {                                  \
        JS_NOT_REACHED("JSOP_UNUSED" # n);                                    \
        return false;                                                         \
    }

UNUSED(203)
UNUSED(204)
UNUSED(205)
UNUSED(206)
UNUSED(207)
UNUSED(208)
UNUSED(209)
UNUSED(219)
