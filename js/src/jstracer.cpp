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

#include "jsstdint.h"
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
#include "jsdate.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jsmath.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jstl.h"
#include "jstracer.h"
#include "jsxml.h"
#include "jstypedarray.h"

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsfuninlines.h"
#include "jsinterpinlines.h"
#include "jspropertycacheinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jscntxtinlines.h"
#include "jsopcodeinlines.h"

#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#endif

#include "jsautooplen.h"        // generated headers last
#include "imacros.c.out"

#if defined(NANOJIT_ARM) && defined(__GNUC__) && defined(AVMPLUS_LINUX)
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <elf.h>
#endif

namespace nanojit {
using namespace js;
using namespace js::gc;
using namespace js::tjit;

/* Implement embedder-specific nanojit members. */

void*
nanojit::Allocator::allocChunk(size_t nbytes)
{
    VMAllocator *vma = (VMAllocator*)this;
    JS_ASSERT(!vma->outOfMemory());
    void *p = js_calloc(nbytes);
    if (!p) {
        JS_ASSERT(nbytes < sizeof(vma->mReserve));
        vma->mOutOfMemory = true;
        p = (void*) &vma->mReserve[0];
    }
    vma->mSize += nbytes;
    return p;
}

void
nanojit::Allocator::freeChunk(void *p) {
    VMAllocator *vma = (VMAllocator*)this;
    if (p != &vma->mReserve[0])
        js_free(p);
}

void
nanojit::Allocator::postReset() {
    VMAllocator *vma = (VMAllocator*)this;
    vma->mOutOfMemory = false;
    vma->mSize = 0;
}

int
StackFilter::getTop(LIns* guard)
{
    VMSideExit* e = (VMSideExit*)guard->record()->exit;
    return e->sp_adj;
}

#if defined NJ_VERBOSE
void
LInsPrinter::formatGuard(InsBuf *buf, LIns *ins)
{
    RefBuf b1, b2;
    VMSideExit *x = (VMSideExit *)ins->record()->exit;
    VMPI_snprintf(buf->buf, buf->len,
            "%s: %s %s -> pc=%p imacpc=%p sp%+ld rp%+ld (GuardID=%03d)",
            formatRef(&b1, ins),
            lirNames[ins->opcode()],
            ins->oprnd1() ? formatRef(&b2, ins->oprnd1()) : "",
            (void *)x->pc,
            (void *)x->imacpc,
            (long int)x->sp_adj,
            (long int)x->rp_adj,
            ins->record()->profGuardID);
}

void
LInsPrinter::formatGuardXov(InsBuf *buf, LIns *ins)
{
    RefBuf b1, b2, b3;
    VMSideExit *x = (VMSideExit *)ins->record()->exit;
    VMPI_snprintf(buf->buf, buf->len,
            "%s = %s %s, %s -> pc=%p imacpc=%p sp%+ld rp%+ld (GuardID=%03d)",
            formatRef(&b1, ins),
            lirNames[ins->opcode()],
            formatRef(&b2, ins->oprnd1()),
            formatRef(&b3, ins->oprnd2()),
            (void *)x->pc,
            (void *)x->imacpc,
            (long int)x->sp_adj,
            (long int)x->rp_adj,
            ins->record()->profGuardID);
}

const char*
nanojit::LInsPrinter::accNames[] = {
    "state",        // (1 <<  0) == ACCSET_STATE
    "sp",           // (1 <<  1) == ACCSET_STACK
    "rp",           // (1 <<  2) == ACCSET_RSTACK
    "cx",           // (1 <<  3) == ACCSET_CX
    "eos",          // (1 <<  4) == ACCSET_EOS
    "alloc",        // (1 <<  5) == ACCSET_ALLOC
    "regs",         // (1 <<  6) == ACCSET_FRAMEREGS
    "sf",           // (1 <<  7) == ACCSET_STACKFRAME
    "rt",           // (1 <<  8) == ACCSET_RUNTIME

    "objclasp",     // (1 <<  9) == ACCSET_OBJ_CLASP
    "objflags",     // (1 << 10) == ACCSET_OBJ_FLAGS
    "objshape",     // (1 << 11) == ACCSET_OBJ_SHAPE
    "objproto",     // (1 << 12) == ACCSET_OBJ_PROTO
    "objparent",    // (1 << 13) == ACCSET_OBJ_PARENT
    "objprivate",   // (1 << 14) == ACCSET_OBJ_PRIVATE
    "objcapacity",  // (1 << 15) == ACCSET_OBJ_CAPACITY
    "objslots",     // (1 << 16) == ACCSET_OBJ_SLOTS

    "slots",        // (1 << 17) == ACCSET_SLOTS
    "tarray",       // (1 << 18) == ACCSET_TARRAY
    "tdata",        // (1 << 19) == ACCSET_TARRAY_DATA
    "iter",         // (1 << 20) == ACCSET_ITER
    "iterprops",    // (1 << 21) == ACCSET_ITER_PROPS
    "str",          // (1 << 22) == ACCSET_STRING
    "strmchars",    // (1 << 23) == ACCSET_STRING_MCHARS
    "typemap",      // (1 << 24) == ACCSET_TYPEMAP
    "fcslots",      // (1 << 25) == ACCSET_FCSLOTS
    "argsdata",     // (1 << 26) == ACCSET_ARGS_DATA

    "?!"            // this entry should never be used, have it just in case
};
#endif

} /* namespace nanojit */

JS_DEFINE_CALLINFO_2(extern, STRING, js_IntToString, CONTEXT, INT32, 1, nanojit::ACCSET_NONE)

namespace js {

using namespace nanojit;

#if JS_HAS_XML_SUPPORT
#define RETURN_VALUE_IF_XML(val, ret)                                         \
    JS_BEGIN_MACRO                                                            \
        if (!val.isPrimitive() && val.toObject().isXML())        \
            RETURN_VALUE("xml detected", ret);                                \
    JS_END_MACRO
#else
#define RETURN_IF_XML(val, ret) ((void) 0)
#endif

#define RETURN_IF_XML_A(val) RETURN_VALUE_IF_XML(val, ARECORD_STOP)
#define RETURN_IF_XML(val)   RETURN_VALUE_IF_XML(val, RECORD_STOP)

JS_STATIC_ASSERT(sizeof(JSValueType) == 1);
JS_STATIC_ASSERT(offsetof(TraceNativeStorage, stack_global_buf) % 16 == 0);

/* Map to translate a type tag into a printable representation. */
#ifdef DEBUG
static char
TypeToChar(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_DOUBLE: return 'D';
      case JSVAL_TYPE_INT32: return 'I';
      case JSVAL_TYPE_STRING: return 'S';
      case JSVAL_TYPE_OBJECT: return '!';
      case JSVAL_TYPE_BOOLEAN: return 'B';
      case JSVAL_TYPE_NULL: return 'N';
      case JSVAL_TYPE_UNDEFINED: return 'U';
      case JSVAL_TYPE_MAGIC: return 'M';
      case JSVAL_TYPE_FUNOBJ: return 'F';
      case JSVAL_TYPE_NONFUNOBJ: return 'O';
      case JSVAL_TYPE_BOXED: return '#';
      case JSVAL_TYPE_STRORNULL: return 's';
      case JSVAL_TYPE_OBJORNULL: return 'o';
      case JSVAL_TYPE_UNINITIALIZED: return '*';
    }
    return '?';
}

static char
ValueToTypeChar(const Value &v)
{
    if (v.isInt32()) return 'I';
    if (v.isDouble()) return 'D';
    if (v.isString()) return 'S';
    if (v.isObject()) return v.toObject().isFunction() ? 'F' : 'O';
    if (v.isBoolean()) return 'B';
    if (v.isNull()) return 'N';
    if (v.isUndefined()) return 'U';
    if (v.isMagic()) return 'M';
    return '?';
}
#endif


/* Blacklist parameters. */

/*
 * Number of iterations of a loop where we start tracing.  That is, we don't
 * start tracing until the beginning of the HOTLOOP-th iteration.
 */
#define HOTLOOP 8

/* Attempt recording this many times before blacklisting permanently. */
#define BL_ATTEMPTS 2

/* Skip this many hits before attempting recording again, after an aborted attempt. */
#define BL_BACKOFF 32

/*
 * If, after running a trace CHECK_LOOP_ITERS times, it hasn't done MIN_LOOP_ITERS
 * iterations, we blacklist it.
*/
#define MIN_LOOP_ITERS 200
#define LOOP_CHECK_ITERS 10

#ifdef DEBUG
#define LOOP_COUNT_MAX 100000000
#else
#define LOOP_COUNT_MAX MIN_LOOP_ITERS
#endif

/* Number of times we wait to exit on a side exit before we try to extend the tree. */
#define HOTEXIT 1

/* Number of times we try to extend the tree along a side exit. */
#define MAXEXIT 3

/* Maximum number of peer trees allowed. */
#define MAXPEERS 9

/* Max call depths for inlining. */
#define MAX_CALLDEPTH 10

/* Max number of slots in a table-switch. */
#define MAX_TABLE_SWITCH 256

/* Max memory needed to rebuild the interpreter stack when falling off trace. */
#define MAX_INTERP_STACK_BYTES                                                \
    (MAX_NATIVE_STACK_SLOTS * sizeof(Value) +                                 \
     MAX_CALL_STACK_ENTRIES * sizeof(JSInlineFrame) +                         \
     sizeof(JSInlineFrame)) /* possibly slow native frame at top of stack */

/* Max number of branches per tree. */
#define MAX_BRANCHES 32

#define CHECK_STATUS(expr)                                                    \
    JS_BEGIN_MACRO                                                            \
        RecordingStatus _status = (expr);                                     \
        if (_status != RECORD_CONTINUE)                                       \
          return _status;                                                     \
    JS_END_MACRO

#define CHECK_STATUS_A(expr)                                                  \
    JS_BEGIN_MACRO                                                            \
        AbortableRecordingStatus _status = InjectStatus((expr));              \
        if (_status != ARECORD_CONTINUE)                                      \
          return _status;                                                     \
    JS_END_MACRO

#ifdef JS_JIT_SPEW
#define RETURN_VALUE(msg, value)                                              \
    JS_BEGIN_MACRO                                                            \
        debug_only_printf(LC_TMAbort, "trace stopped: %d: %s\n", __LINE__, (msg)); \
        return (value);                                                       \
    JS_END_MACRO
#else
#define RETURN_VALUE(msg, value)   return (value)
#endif

#define RETURN_STOP(msg)     RETURN_VALUE(msg, RECORD_STOP)
#define RETURN_STOP_A(msg)   RETURN_VALUE(msg, ARECORD_STOP)
#define RETURN_ERROR(msg)    RETURN_VALUE(msg, RECORD_ERROR)
#define RETURN_ERROR_A(msg)  RETURN_VALUE(msg, ARECORD_ERROR)

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

static JSBool
jitstats_getOnTrace(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    *vp = BOOLEAN_TO_JSVAL(JS_ON_TRACE(cx));
    return true;
}

static JSPropertySpec jitstats_props[] = {
#define JITSTAT(x) { #x, STAT ## x ## ID, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT },
#include "jitstats.tbl"
#undef JITSTAT
    { "onTrace", 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT, jitstats_getOnTrace, NULL },
    { 0 }
};

static JSBool
jitstats_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    int index = -1;

    if (JSID_IS_STRING(id)) {
        JSString* str = JSID_TO_STRING(id);
        if (strcmp(JS_GetStringBytes(str), "HOTLOOP") == 0) {
            *vp = INT_TO_JSVAL(HOTLOOP);
            return JS_TRUE;
        }

        if (strcmp(JS_GetStringBytes(str), "profiler") == 0) {
            *vp = BOOLEAN_TO_JSVAL(cx->profilingEnabled);
            return JS_TRUE;
        }
    }

    if (JSID_IS_INT(id))
        index = JSID_TO_INT(id);

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
        *vp = INT_TO_JSVAL(jsint(result));
        return JS_TRUE;
    }
    char retstr[64];
    JS_snprintf(retstr, sizeof retstr, "%llu", result);
    *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, retstr));
    return JS_TRUE;
}

JSClass jitstats_class = {
    "jitstats",
    0,
    JS_PropertyStub,       JS_PropertyStub,
    jitstats_getProperty,  JS_PropertyStub,
    JS_EnumerateStub,      JS_ResolveStub,
    JS_ConvertStub,        NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

void
InitJITStatsClass(JSContext *cx, JSObject *glob)
{
    JS_InitClass(cx, glob, NULL, &jitstats_class, NULL, 0, jitstats_props, NULL, NULL, NULL);
}

#define AUDIT(x) (jitstats.x++)
#else
#define AUDIT(x) ((void)0)
#endif /* JS_JIT_SPEW */

static avmplus::AvmCore s_core = avmplus::AvmCore();
static avmplus::AvmCore* core = &s_core;

static void OutOfMemoryAbort()
{
    JS_NOT_REACHED("out of memory");
    abort();
}

#ifdef JS_JIT_SPEW
static void
DumpPeerStability(TraceMonitor* tm, const void* ip, JSObject* globalObj, uint32 globalShape, uint32 argc);
#endif

/*
 * We really need a better way to configure the JIT. Shaver, where is
 * my fancy JIT object?
 *
 * NB: this is raced on, if jstracer.cpp should ever be running MT.
 * I think it's harmless tho.
 */
static bool did_we_check_processor_features = false;

/* ------ Debug logging control ------ */

/*
 * All the logging control stuff lives in here.  It is shared between
 * all threads, but I think that's OK.
 */
LogControl LogController;

#ifdef JS_JIT_SPEW

/*
 * NB: this is raced on too, if jstracer.cpp should ever be running MT.
 * Also harmless.
 */
static bool did_we_set_up_debug_logging = false;

static void
InitJITLogController()
{
    char *tm, *tmf;
    uint32_t bits;

    LogController.lcbits = 0;

    tm = getenv("TRACEMONKEY");
    if (tm) {
        fflush(NULL);
        printf(
            "The environment variable $TRACEMONKEY has been replaced by $TMFLAGS.\n"
            "Try 'TMFLAGS=help js -j' for a list of options.\n"
        );
        exit(0);
    }

    tmf = getenv("TMFLAGS");
    if (!tmf) return;

    /* Using strstr() is really a cheap hack as far as flag decoding goes. */
    if (strstr(tmf, "help")) {
        fflush(NULL);
        printf(
            "usage: TMFLAGS=option,option,option,... where options can be:\n"
            "\n"
            "  help         show this message\n"
            "  ------ options for jstracer & jsregexp ------\n"
            "  minimal      ultra-minimalist output; try this first\n"
            "  full         everything except 'treevis' and 'fragprofile'\n"
            "  tracer       tracer lifetime (FIXME:better description)\n"
            "  recorder     trace recording stuff (FIXME:better description)\n"
            "  abort        show trace recording aborts\n"
            "  stats        show trace recording stats\n"
            "  regexp       show compilation & entry for regexps\n"
            "  profiler     show loop profiles as they are profiled\n"
            "  treevis      spew that tracevis/tree.py can parse\n"
            "  ------ options for Nanojit ------\n"
            "  fragprofile  count entries and exits for each fragment\n"
            "  liveness     show LIR liveness at start of reader pipeline\n"
            "  readlir      show LIR as it enters the reader pipeline\n"
            "  aftersf      show LIR after StackFilter\n"
            "  afterdce     show LIR after dead code elimination\n"
            "  native       show native code (interleaved with 'afterdce')\n"
            "  regalloc     show regalloc state in 'native' output\n"
            "  activation   show activation state in 'native' output\n"
            "\n"
        );
        exit(0);
        /*NOTREACHED*/
    }

    bits = 0;

    /* flags for jstracer.cpp */
    if (strstr(tmf, "minimal")  || strstr(tmf, "full")) bits |= LC_TMMinimal;
    if (strstr(tmf, "tracer")   || strstr(tmf, "full")) bits |= LC_TMTracer;
    if (strstr(tmf, "recorder") || strstr(tmf, "full")) bits |= LC_TMRecorder;
    if (strstr(tmf, "abort")    || strstr(tmf, "full")) bits |= LC_TMAbort;
    if (strstr(tmf, "stats")    || strstr(tmf, "full")) bits |= LC_TMStats;
    if (strstr(tmf, "profiler") || strstr(tmf, "full")) bits |= LC_TMProfiler;
    if (strstr(tmf, "treevis"))                         bits |= LC_TMTreeVis;

    /* flags for nanojit */
    if (strstr(tmf, "fragprofile"))                       bits |= LC_FragProfile;
    if (strstr(tmf, "liveness")   || strstr(tmf, "full")) bits |= LC_Liveness;
    if (strstr(tmf, "readlir")    || strstr(tmf, "full")) bits |= LC_ReadLIR;
    if (strstr(tmf, "aftersf")    || strstr(tmf, "full")) bits |= LC_AfterSF;
    if (strstr(tmf, "afterdce")   || strstr(tmf, "full")) bits |= LC_AfterDCE;
    if (strstr(tmf, "native")     || strstr(tmf, "full")) bits |= LC_Native;
    if (strstr(tmf, "regalloc")   || strstr(tmf, "full")) bits |= LC_RegAlloc;
    if (strstr(tmf, "activation") || strstr(tmf, "full")) bits |= LC_Activation;

    LogController.lcbits = bits;
    return;

}
#endif

/* ------------------ Frag-level profiling support ------------------ */

#ifdef JS_JIT_SPEW

/*
 * All the allocations done by this profile data-collection and
 * display machinery, are done in TraceMonitor::profAlloc.  That is
 * emptied out at the end of js_FinishJIT.  It has a lifetime from
 * js_InitJIT to js_FinishJIT, which exactly matches the span
 * js_FragProfiling_init to js_FragProfiling_showResults.
 */
template<class T>
static
Seq<T>* reverseInPlace(Seq<T>* seq)
{
    Seq<T>* prev = NULL;
    Seq<T>* curr = seq;
    while (curr) {
        Seq<T>* next = curr->tail;
        curr->tail = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

// The number of top blocks to show in the profile
#define N_TOP_BLOCKS 50

// Contains profile info for a single guard
struct GuardPI {
    uint32_t guardID; // identifying number
    uint32_t count;   // count.
};

struct FragPI {
    uint32_t count;          // entry count for this Fragment
    uint32_t nStaticExits;   // statically: the number of exits
    size_t nCodeBytes;       // statically: the number of insn bytes in the main fragment
    size_t nExitBytes;       // statically: the number of insn bytes in the exit paths
    Seq<GuardPI>* guards;    // guards, each with its own count
    uint32_t largestGuardID; // that exists in .guards
};

void
FragProfiling_FragFinalizer(Fragment* f, TraceMonitor* tm)
{
    // Recover profiling data from 'f', which is logically at the end
    // of its useful lifetime.
    if (!(LogController.lcbits & LC_FragProfile))
        return;

    NanoAssert(f);
    // Valid profFragIDs start at 1
    NanoAssert(f->profFragID >= 1);
    // Should be called exactly once per Fragment.  This will assert if
    // you issue the same FragID to more than one Fragment.
    NanoAssert(!tm->profTab->containsKey(f->profFragID));

    FragPI pi = { f->profCount,
                  f->nStaticExits,
                  f->nCodeBytes,
                  f->nExitBytes,
                  NULL, 0 };

    // Begin sanity check on the guards
    SeqBuilder<GuardPI> guardsBuilder(*tm->profAlloc);
    GuardRecord* gr;
    uint32_t nGs = 0;
    uint32_t sumOfDynExits = 0;
    for (gr = f->guardsForFrag; gr; gr = gr->nextInFrag) {
         nGs++;
         // Also copy the data into our auxiliary structure.
         // f->guardsForFrag is in reverse order, and so this
         // copy preserves that ordering (->add adds at end).
         // Valid profGuardIDs start at 1.
         NanoAssert(gr->profGuardID > 0);
         sumOfDynExits += gr->profCount;
         GuardPI gpi = { gr->profGuardID, gr->profCount };
         guardsBuilder.add(gpi);
         if (gr->profGuardID > pi.largestGuardID)
             pi.largestGuardID = gr->profGuardID;
    }
    pi.guards = guardsBuilder.get();
    // And put the guard list in forwards order
    pi.guards = reverseInPlace(pi.guards);

    // Why is this so?  Because nGs is the number of guards
    // at the time the LIR was generated, whereas f->nStaticExits
    // is the number of them observed by the time it makes it
    // through to the assembler.  It can be the case that LIR
    // optimisation removes redundant guards; hence we expect
    // nGs to always be the same or higher.
    NanoAssert(nGs >= f->nStaticExits);

    // Also we can assert that the sum of the exit counts
    // can't exceed the entry count.  It'd be nice to assert that
    // they are exactly equal, but we can't because we don't know
    // how many times we got to the end of the trace.
    NanoAssert(f->profCount >= sumOfDynExits);

    // End sanity check on guards

    tm->profTab->put(f->profFragID, pi);
}

static void
FragProfiling_showResults(TraceMonitor* tm)
{
    uint32_t topFragID[N_TOP_BLOCKS];
    FragPI   topPI[N_TOP_BLOCKS];
    uint64_t totCount = 0, cumulCount;
    uint32_t totSE = 0;
    size_t   totCodeB = 0, totExitB = 0;
    PodArrayZero(topFragID);
    PodArrayZero(topPI);
    FragStatsMap::Iter iter(*tm->profTab);
    while (iter.next()) {
        uint32_t fragID  = iter.key();
        FragPI   pi      = iter.value();
        uint32_t count   = pi.count;
        totCount += (uint64_t)count;
        /* Find the rank for this entry, in tops */
        int r = N_TOP_BLOCKS-1;
        while (true) {
            if (r == -1)
                break;
            if (topFragID[r] == 0) {
                r--;
                continue;
            }
            if (count > topPI[r].count) {
                r--;
                continue;
            }
            break;
        }
        r++;
        NanoAssert(r >= 0 && r <= N_TOP_BLOCKS);
        /* This entry should be placed at topPI[r], and entries
           at higher numbered slots moved up one. */
        if (r < N_TOP_BLOCKS) {
            for (int s = N_TOP_BLOCKS-1; s > r; s--) {
                topFragID[s] = topFragID[s-1];
                topPI[s]     = topPI[s-1];
            }
            topFragID[r] = fragID;
            topPI[r]     = pi;
        }
    }

    LogController.printf(
        "\n----------------- Per-fragment execution counts ------------------\n");
    LogController.printf(
        "\nTotal count = %llu\n\n", (unsigned long long int)totCount);

    LogController.printf(
        "           Entry counts         Entry counts       ----- Static -----\n");
    LogController.printf(
        "         ------Self------     ----Cumulative---   Exits  Cbytes Xbytes   FragID\n");
    LogController.printf("\n");

    if (totCount == 0)
        totCount = 1; /* avoid division by zero */
    cumulCount = 0;
    int r;
    for (r = 0; r < N_TOP_BLOCKS; r++) {
        if (topFragID[r] == 0)
            break;
        cumulCount += (uint64_t)topPI[r].count;
        LogController.printf("%3d:     %5.2f%% %9u     %6.2f%% %9llu"
                             "     %3d   %5u  %5u   %06u\n",
                             r,
                             (double)topPI[r].count * 100.0 / (double)totCount,
                             topPI[r].count,
                             (double)cumulCount * 100.0 / (double)totCount,
                             (unsigned long long int)cumulCount,
                             topPI[r].nStaticExits,
                             (unsigned int)topPI[r].nCodeBytes,
                             (unsigned int)topPI[r].nExitBytes,
                             topFragID[r]);
        totSE += (uint32_t)topPI[r].nStaticExits;
        totCodeB += topPI[r].nCodeBytes;
        totExitB += topPI[r].nExitBytes;
    }
    LogController.printf("\nTotal displayed code bytes = %u, "
                            "exit bytes = %u\n"
                            "Total displayed static exits = %d\n\n",
                            (unsigned int)totCodeB, (unsigned int)totExitB, totSE);

    LogController.printf("Analysis by exit counts\n\n");

    for (r = 0; r < N_TOP_BLOCKS; r++) {
        if (topFragID[r] == 0)
            break;
        LogController.printf("FragID=%06u, total count %u:\n", topFragID[r],
                                topPI[r].count);
        uint32_t madeItToEnd = topPI[r].count;
        uint32_t totThisFrag = topPI[r].count;
        if (totThisFrag == 0)
            totThisFrag = 1;
        GuardPI gpi;
        // visit the guards, in forward order
        for (Seq<GuardPI>* guards = topPI[r].guards; guards; guards = guards->tail) {
            gpi = (*guards).head;
            if (gpi.count == 0)
                continue;
            madeItToEnd -= gpi.count;
            LogController.printf("   GuardID=%03u    %7u (%5.2f%%)\n",
                                    gpi.guardID, gpi.count,
                                    100.0 * (double)gpi.count / (double)totThisFrag);
        }
        LogController.printf("   Looped (%03u)   %7u (%5.2f%%)\n",
                                topPI[r].largestGuardID+1,
                                madeItToEnd,
                                100.0 * (double)madeItToEnd /  (double)totThisFrag);
        NanoAssert(madeItToEnd <= topPI[r].count); // else unsigned underflow
        LogController.printf("\n");
    }

    tm->profTab = NULL;
}

#endif

/* ----------------------------------------------------------------- */

#ifdef DEBUG
static const char*
getExitName(ExitType type)
{
    static const char* exitNames[] =
    {
    #define MAKE_EXIT_STRING(x) #x,
    JS_TM_EXITCODES(MAKE_EXIT_STRING)
    #undef MAKE_EXIT_STRING
    NULL
    };

    JS_ASSERT(type < TOTAL_EXIT_TYPES);

    return exitNames[type];
}

static JSBool FASTCALL
PrintOnTrace(char* format, uint32 argc, double *argv)
{
    union {
        struct {
            uint32 lo;
            uint32 hi;
        } i;
        double   d;
        char     *cstr;
        JSObject *o;
        JSString *s;
    } u;

#define GET_ARG() JS_BEGIN_MACRO          \
        if (argi >= argc) { \
        fprintf(out, "[too few args for format]"); \
        break;       \
} \
    u.d = argv[argi++]; \
    JS_END_MACRO

    FILE *out = stderr;

    uint32 argi = 0;
    for (char *p = format; *p; ++p) {
        if (*p != '%') {
            putc(*p, out);
            continue;
        }
        char ch = *++p;
        if (!ch) {
            fprintf(out, "[trailing %%]");
            continue;
        }

        switch (ch) {
        case 'a':
            GET_ARG();
            fprintf(out, "[%u:%u 0x%x:0x%x %f]", u.i.lo, u.i.hi, u.i.lo, u.i.hi, u.d);
            break;
        case 'd':
            GET_ARG();
            fprintf(out, "%d", u.i.lo);
            break;
        case 'u':
            GET_ARG();
            fprintf(out, "%u", u.i.lo);
            break;
        case 'x':
            GET_ARG();
            fprintf(out, "%x", u.i.lo);
            break;
        case 'f':
            GET_ARG();
            fprintf(out, "%f", u.d);
            break;
        case 'o':
            GET_ARG();
            js_DumpObject(u.o);
            break;
        case 's':
            GET_ARG();
            {
                size_t length = u.s->length();
                // protect against massive spew if u.s is a bad pointer.
                if (length > 1 << 16)
                    length = 1 << 16;
                jschar *chars = u.s->chars();
                for (unsigned i = 0; i < length; ++i) {
                    jschar co = chars[i];
                    if (co < 128)
                        putc(co, out);
                    else if (co < 256)
                        fprintf(out, "\\u%02x", co);
                    else
                        fprintf(out, "\\u%04x", co);
                }
            }
            break;
        case 'S':
            GET_ARG();
            fprintf(out, "%s", u.cstr);
            break;
        case 'v': {
            GET_ARG();
            Value *v = (Value *) u.i.lo;
            js_DumpValue(*v);
            break;
        }
        default:
            fprintf(out, "[invalid %%%c]", *p);
        }
    }

#undef GET_ARG

    return JS_TRUE;
}

JS_DEFINE_CALLINFO_3(extern, BOOL, PrintOnTrace, CHARPTR, UINT32, DOUBLEPTR, 0, ACCSET_STORE_ANY)

// This version is not intended to be called directly: usually it is easier to
// use one of the other overloads.
void
TraceRecorder::tprint(const char *format, int count, nanojit::LIns *insa[])
{
    size_t size = strlen(format) + 1;
    char* data = (char*) traceMonitor->traceAlloc->alloc(size);
    memcpy(data, format, size);

    double *args = (double*) traceMonitor->traceAlloc->alloc(count * sizeof(double));
    LIns* argsp_ins = w.nameImmpNonGC(args);
    for (int i = 0; i < count; ++i)
        w.stTprintArg(insa, argsp_ins, i);

    LIns* args_ins[] = { w.nameImmpNonGC(args), w.nameImmi(count), w.nameImmpNonGC(data) };
    LIns* call_ins = w.call(&PrintOnTrace_ci, args_ins);
    guard(false, w.eqi0(call_ins), MISMATCH_EXIT);
}

// Generate a 'printf'-type call from trace for debugging.
void
TraceRecorder::tprint(const char *format)
{
    LIns* insa[] = { NULL };
    tprint(format, 0, insa);
}

void
TraceRecorder::tprint(const char *format, LIns *ins)
{
    LIns* insa[] = { ins };
    tprint(format, 1, insa);
}

void
TraceRecorder::tprint(const char *format, LIns *ins1, LIns *ins2)
{
    LIns* insa[] = { ins1, ins2 };
    tprint(format, 2, insa);
}

void
TraceRecorder::tprint(const char *format, LIns *ins1, LIns *ins2, LIns *ins3)
{
    LIns* insa[] = { ins1, ins2, ins3 };
    tprint(format, 3, insa);
}

void
TraceRecorder::tprint(const char *format, LIns *ins1, LIns *ins2, LIns *ins3, LIns *ins4)
{
    LIns* insa[] = { ins1, ins2, ins3, ins4 };
    tprint(format, 4, insa);
}

void
TraceRecorder::tprint(const char *format, LIns *ins1, LIns *ins2, LIns *ins3, LIns *ins4,
                      LIns *ins5)
{
    LIns* insa[] = { ins1, ins2, ins3, ins4, ins5 };
    tprint(format, 5, insa);
}

void
TraceRecorder::tprint(const char *format, LIns *ins1, LIns *ins2, LIns *ins3, LIns *ins4,
                      LIns *ins5, LIns *ins6)
{
    LIns* insa[] = { ins1, ins2, ins3, ins4, ins5, ins6 };
    tprint(format, 6, insa);
}
#endif

Tracker::Tracker()
{
    pagelist = NULL;
}

Tracker::~Tracker()
{
    clear();
}

inline jsuword
Tracker::getTrackerPageBase(const void* v) const
{
    return jsuword(v) & ~TRACKER_PAGE_MASK;
}

inline jsuword
Tracker::getTrackerPageOffset(const void* v) const
{
    return (jsuword(v) & TRACKER_PAGE_MASK) >> 2;
}

struct Tracker::TrackerPage*
Tracker::findTrackerPage(const void* v) const
{
    jsuword base = getTrackerPageBase(v);
    struct Tracker::TrackerPage* p = pagelist;
    while (p) {
        if (p->base == base)
            return p;
        p = p->next;
    }
    return NULL;
}

struct Tracker::TrackerPage*
Tracker::addTrackerPage(const void* v)
{
    jsuword base = getTrackerPageBase(v);
    struct TrackerPage* p = (struct TrackerPage*) js_calloc(sizeof(*p));
    p->base = base;
    p->next = pagelist;
    pagelist = p;
    return p;
}

void
Tracker::clear()
{
    while (pagelist) {
        TrackerPage* p = pagelist;
        pagelist = pagelist->next;
        js_free(p);
    }
}

bool
Tracker::has(const void *v) const
{
    return get(v) != NULL;
}

LIns*
Tracker::get(const void* v) const
{
    struct Tracker::TrackerPage* p = findTrackerPage(v);
    if (!p)
        return NULL;
    return p->map[getTrackerPageOffset(v)];
}

void
Tracker::set(const void* v, LIns* i)
{
    struct Tracker::TrackerPage* p = findTrackerPage(v);
    if (!p)
        p = addTrackerPage(v);
    p->map[getTrackerPageOffset(v)] = i;
}

static inline bool
hasInt32Repr(const Value &v)
{
    if (!v.isNumber())
        return false;
    if (v.isInt32())
        return true;
    int32_t _;
    return JSDOUBLE_IS_INT32(v.toDouble(), &_);
}

static inline jsint
asInt32(const Value &v)
{
    JS_ASSERT(v.isNumber());
    if (v.isInt32())
        return v.toInt32();
#ifdef DEBUG
    int32_t _;
    JS_ASSERT(JSDOUBLE_IS_INT32(v.toDouble(), &_));
#endif
    return jsint(v.toDouble());
}

/*
 * Return JSVAL_TYPE_DOUBLE for all numbers (int and double). Split
 * JSVAL_TYPE_OBJECT into JSVAL_TYPE_FUNOBJ  and JSVAL_TYPE_NONFUNOBJ.
 * Otherwise, just return the value's type.
 */
static inline JSValueType
getPromotedType(const Value &v)
{
    if (v.isNumber())
        return JSVAL_TYPE_DOUBLE;
    if (v.isObject())
        return v.toObject().isFunction() ? JSVAL_TYPE_FUNOBJ : JSVAL_TYPE_NONFUNOBJ;
    return v.extractNonDoubleObjectTraceType();
}

/*
 * Return JSVAL_TYPE_INT32 for all whole numbers that fit into signed 32-bit.
 * Split JSVAL_TYPE_OBJECT into JSVAL_TYPE_FUNOBJ and JSVAL_TYPE_NONFUNOBJ.
 * Otherwise, just return the value's type.
 */
static inline JSValueType
getCoercedType(const Value &v)
{
    if (v.isNumber()) {
        int32_t _;
        return (v.isInt32() || JSDOUBLE_IS_INT32(v.toDouble(), &_))
               ? JSVAL_TYPE_INT32
               : JSVAL_TYPE_DOUBLE;
    }
    if (v.isObject())
        return v.toObject().isFunction() ? JSVAL_TYPE_FUNOBJ : JSVAL_TYPE_NONFUNOBJ;
    return v.extractNonDoubleObjectTraceType();
}

static inline JSValueType
getFrameObjPtrTraceType(void *p, JSStackFrame *fp)
{
    if (p == fp->addressOfScopeChain()) {
        JS_ASSERT(*(JSObject **)p != NULL);
        return JSVAL_TYPE_NONFUNOBJ;
    }
    JS_ASSERT(p == fp->addressOfArgs());
    return fp->hasArgsObj() ? JSVAL_TYPE_NONFUNOBJ : JSVAL_TYPE_NULL;
}

static inline bool
isFrameObjPtrTraceType(JSValueType t)
{
    return t == JSVAL_TYPE_NULL || t == JSVAL_TYPE_NONFUNOBJ;
}

/* Constant seed and accumulate step borrowed from the DJB hash. */

const uintptr_t ORACLE_MASK = ORACLE_SIZE - 1;
JS_STATIC_ASSERT((ORACLE_MASK & ORACLE_SIZE) == 0);

const uintptr_t FRAGMENT_TABLE_MASK = FRAGMENT_TABLE_SIZE - 1;
JS_STATIC_ASSERT((FRAGMENT_TABLE_MASK & FRAGMENT_TABLE_SIZE) == 0);

const uintptr_t HASH_SEED = 5381;

static inline void
HashAccum(uintptr_t& h, uintptr_t i, uintptr_t mask)
{
    h = ((h << 5) + h + (mask & i)) & mask;
}

static JS_REQUIRES_STACK inline int
StackSlotHash(JSContext* cx, unsigned slot, const void* pc)
{
    uintptr_t h = HASH_SEED;
    HashAccum(h, uintptr_t(cx->fp()->script()), ORACLE_MASK);
    HashAccum(h, uintptr_t(pc), ORACLE_MASK);
    HashAccum(h, uintptr_t(slot), ORACLE_MASK);
    return int(h);
}

static JS_REQUIRES_STACK inline int
GlobalSlotHash(JSContext* cx, unsigned slot)
{
    uintptr_t h = HASH_SEED;
    JSStackFrame* fp = cx->fp();

    while (fp->prev())
        fp = fp->prev();

    HashAccum(h, uintptr_t(fp->maybeScript()), ORACLE_MASK);
    HashAccum(h, uintptr_t(fp->scopeChain().getGlobal()->shape()), ORACLE_MASK);
    HashAccum(h, uintptr_t(slot), ORACLE_MASK);
    return int(h);
}

static inline int
PCHash(jsbytecode* pc)
{
    return int(uintptr_t(pc) & ORACLE_MASK);
}

Oracle::Oracle()
{
    /* Grow the oracle bitsets to their (fixed) size here, once. */
    _stackDontDemote.set(ORACLE_SIZE-1);
    _globalDontDemote.set(ORACLE_SIZE-1);
    clear();
}

/* Tell the oracle that a certain global variable should not be demoted. */
JS_REQUIRES_STACK void
Oracle::markGlobalSlotUndemotable(JSContext* cx, unsigned slot)
{
    _globalDontDemote.set(GlobalSlotHash(cx, slot));
}

/* Consult with the oracle whether we shouldn't demote a certain global variable. */
JS_REQUIRES_STACK bool
Oracle::isGlobalSlotUndemotable(JSContext* cx, unsigned slot) const
{
    return _globalDontDemote.get(GlobalSlotHash(cx, slot));
}

/* Tell the oracle that a certain slot at a certain stack slot should not be demoted. */
JS_REQUIRES_STACK void
Oracle::markStackSlotUndemotable(JSContext* cx, unsigned slot, const void* pc)
{
    _stackDontDemote.set(StackSlotHash(cx, slot, pc));
}

JS_REQUIRES_STACK void
Oracle::markStackSlotUndemotable(JSContext* cx, unsigned slot)
{
    markStackSlotUndemotable(cx, slot, cx->regs->pc);
}

/* Consult with the oracle whether we shouldn't demote a certain slot. */
JS_REQUIRES_STACK bool
Oracle::isStackSlotUndemotable(JSContext* cx, unsigned slot, const void* pc) const
{
    return _stackDontDemote.get(StackSlotHash(cx, slot, pc));
}

JS_REQUIRES_STACK bool
Oracle::isStackSlotUndemotable(JSContext* cx, unsigned slot) const
{
    return isStackSlotUndemotable(cx, slot, cx->regs->pc);
}

/* Tell the oracle that a certain slot at a certain bytecode location should not be demoted. */
void
Oracle::markInstructionUndemotable(jsbytecode* pc)
{
    _pcDontDemote.set(PCHash(pc));
}

/* Consult with the oracle whether we shouldn't demote a certain bytecode location. */
bool
Oracle::isInstructionUndemotable(jsbytecode* pc) const
{
    return _pcDontDemote.get(PCHash(pc));
}

/* Tell the oracle that the instruction at bytecode location should use a stronger (slower) test for -0. */
void
Oracle::markInstructionSlowZeroTest(jsbytecode* pc)
{
    _pcSlowZeroTest.set(PCHash(pc));
}

/* Consult with the oracle whether we should use a stronger (slower) test for -0. */
bool
Oracle::isInstructionSlowZeroTest(jsbytecode* pc) const
{
    return _pcSlowZeroTest.get(PCHash(pc));
}

void
Oracle::clearDemotability()
{
    _stackDontDemote.reset();
    _globalDontDemote.reset();
    _pcDontDemote.reset();
    _pcSlowZeroTest.reset();
}

JS_REQUIRES_STACK void
TraceRecorder::markSlotUndemotable(LinkableFragment* f, unsigned slot)
{
    if (slot < f->nStackTypes) {
        traceMonitor->oracle->markStackSlotUndemotable(cx, slot);
        return;
    }

    uint16* gslots = f->globalSlots->data();
    traceMonitor->oracle->markGlobalSlotUndemotable(cx, gslots[slot - f->nStackTypes]);
}

JS_REQUIRES_STACK void
TraceRecorder::markSlotUndemotable(LinkableFragment* f, unsigned slot, const void* pc)
{
    if (slot < f->nStackTypes) {
        traceMonitor->oracle->markStackSlotUndemotable(cx, slot, pc);
        return;
    }

    uint16* gslots = f->globalSlots->data();
    traceMonitor->oracle->markGlobalSlotUndemotable(cx, gslots[slot - f->nStackTypes]);
}

static JS_REQUIRES_STACK bool
IsSlotUndemotable(Oracle* oracle, JSContext* cx, LinkableFragment* f, unsigned slot, const void* ip)
{
    if (slot < f->nStackTypes)
        return !oracle || oracle->isStackSlotUndemotable(cx, slot, ip);

    uint16* gslots = f->globalSlots->data();
    return !oracle || oracle->isGlobalSlotUndemotable(cx, gslots[slot - f->nStackTypes]);
}

class FrameInfoCache
{
    struct HashPolicy
    {
        typedef FrameInfo *Lookup;
        static HashNumber hash(const FrameInfo* fi) {
            size_t len = sizeof(FrameInfo) + fi->callerHeight * sizeof(JSValueType);
            HashNumber h = 0;
            const unsigned char *s = (const unsigned char*)fi;
            for (size_t i = 0; i < len; i++, s++)
                h = JS_ROTATE_LEFT32(h, 4) ^ *s;
            return h;
        }

        static bool match(const FrameInfo* fi1, const FrameInfo* fi2) {
            if (memcmp(fi1, fi2, sizeof(FrameInfo)) != 0)
                return false;
            return memcmp(fi1->get_typemap(), fi2->get_typemap(),
                          fi1->callerHeight * sizeof(JSValueType)) == 0;
        }
    };

    typedef HashSet<FrameInfo *, HashPolicy, SystemAllocPolicy> FrameSet;

    FrameSet set;
    VMAllocator *allocator;

  public:

    FrameInfoCache(VMAllocator *allocator);

    void reset() {
        set.clear();
    }

    FrameInfo *memoize(FrameInfo *fi) {
        FrameSet::AddPtr p = set.lookupForAdd(fi);
        if (!p) {
            FrameInfo* n = (FrameInfo*)
                allocator->alloc(sizeof(FrameInfo) + fi->callerHeight * sizeof(JSValueType));
            memcpy(n, fi, sizeof(FrameInfo) + fi->callerHeight * sizeof(JSValueType));
            if (!set.add(p, n))
                return NULL;
        }

        return *p;
    }
};

FrameInfoCache::FrameInfoCache(VMAllocator *allocator)
  : allocator(allocator)
{
    if (!set.init())
        OutOfMemoryAbort();
}

#define PC_HASH_COUNT 1024

static void
Blacklist(jsbytecode* pc)
{
    AUDIT(blacklisted);
    JS_ASSERT(*pc == JSOP_TRACE || *pc == JSOP_NOTRACE);
    *pc = JSOP_NOTRACE;
}

static void
Unblacklist(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(*pc == JSOP_NOTRACE || *pc == JSOP_TRACE);
    if (*pc == JSOP_NOTRACE) {
        *pc = JSOP_TRACE;

#ifdef JS_METHODJIT
        /* This code takes care of unblacklisting in the method JIT. */
        js::mjit::EnableTraceHint(script, pc, GET_UINT16(pc));
#endif
    }
}

static bool
IsBlacklisted(jsbytecode* pc)
{
    if (*pc == JSOP_NOTRACE)
        return true;
    if (*pc == JSOP_CALL)
        return *(pc + JSOP_CALL_LENGTH) == JSOP_NOTRACE;
    return false;
}

static void
Backoff(JSContext *cx, jsbytecode* pc, Fragment* tree = NULL)
{
    /* N.B. This code path cannot assume the recorder is/is not alive. */
    RecordAttemptMap &table = *JS_TRACE_MONITOR(cx).recordAttempts;
    if (RecordAttemptMap::AddPtr p = table.lookupForAdd(pc)) {
        if (p->value++ > (BL_ATTEMPTS * MAXPEERS)) {
            p->value = 0;
            Blacklist(pc);
            return;
        }
    } else {
        table.add(p, pc, 0);
    }

    if (tree) {
        tree->hits() -= BL_BACKOFF;

        /*
         * In case there is no entry or no table (due to OOM) or some
         * serious imbalance in the recording-attempt distribution on a
         * multitree, give each tree another chance to blacklist here as
         * well.
         */
        if (++tree->recordAttempts > BL_ATTEMPTS)
            Blacklist(pc);
    }
}

static void
ResetRecordingAttempts(JSContext *cx, jsbytecode* pc)
{
    RecordAttemptMap &table = *JS_TRACE_MONITOR(cx).recordAttempts;
    if (RecordAttemptMap::Ptr p = table.lookup(pc))
        p->value = 0;
}

static inline size_t
FragmentHash(const void *ip, JSObject* globalObj, uint32 globalShape, uint32 argc)
{
    uintptr_t h = HASH_SEED;
    HashAccum(h, uintptr_t(ip), FRAGMENT_TABLE_MASK);
    HashAccum(h, uintptr_t(globalObj), FRAGMENT_TABLE_MASK);
    HashAccum(h, uintptr_t(globalShape), FRAGMENT_TABLE_MASK);
    HashAccum(h, uintptr_t(argc), FRAGMENT_TABLE_MASK);
    return size_t(h);
}

static void
RawLookupFirstPeer(TraceMonitor* tm, const void *ip, JSObject* globalObj,
                   uint32 globalShape, uint32 argc,
                   TreeFragment*& firstInBucket, TreeFragment**& prevTreeNextp)
{
    size_t h = FragmentHash(ip, globalObj, globalShape, argc);
    TreeFragment** ppf = &tm->vmfragments[h];
    firstInBucket = *ppf;
    for (; TreeFragment* pf = *ppf; ppf = &pf->next) {
        if (pf->globalObj == globalObj &&
            pf->globalShape == globalShape &&
            pf->ip == ip &&
            pf->argc == argc) {
            prevTreeNextp = ppf;
            return;
        }
    }
    prevTreeNextp = ppf;
    return;
}

static TreeFragment*
LookupLoop(TraceMonitor* tm, const void *ip, JSObject* globalObj,
                uint32 globalShape, uint32 argc)
{
    TreeFragment *_, **prevTreeNextp;
    RawLookupFirstPeer(tm, ip, globalObj, globalShape, argc, _, prevTreeNextp);
    return *prevTreeNextp;
}

static TreeFragment*
LookupOrAddLoop(TraceMonitor* tm, const void *ip, JSObject* globalObj,
                uint32 globalShape, uint32 argc)
{
    TreeFragment *firstInBucket, **prevTreeNextp;
    RawLookupFirstPeer(tm, ip, globalObj, globalShape, argc, firstInBucket, prevTreeNextp);
    if (TreeFragment *f = *prevTreeNextp)
        return f;

    verbose_only(
    uint32_t profFragID = (LogController.lcbits & LC_FragProfile)
                          ? (++(tm->lastFragID)) : 0;
    )
    TreeFragment* f = new (*tm->dataAlloc) TreeFragment(ip, tm->dataAlloc, globalObj, globalShape,
                                                        argc verbose_only(, profFragID));
    f->root = f;                /* f is the root of a new tree */
    *prevTreeNextp = f;         /* insert f at the end of the vmfragments bucket-list */
    f->next = NULL;
    f->first = f;               /* initialize peer-list at f */
    f->peer = NULL;
    return f;
}

static TreeFragment*
AddNewPeerToPeerList(TraceMonitor* tm, TreeFragment* peer)
{
    JS_ASSERT(peer);
    verbose_only(
    uint32_t profFragID = (LogController.lcbits & LC_FragProfile)
                          ? (++(tm->lastFragID)) : 0;
    )
    TreeFragment* f = new (*tm->dataAlloc) TreeFragment(peer->ip, tm->dataAlloc, peer->globalObj,
                                                        peer->globalShape, peer->argc
                                                        verbose_only(, profFragID));
    f->root = f;                /* f is the root of a new tree */
    f->first = peer->first;     /* add f to peer list */
    f->peer = peer->peer;
    peer->peer = f;
    /* only the |first| Fragment of a peer list needs a valid |next| field */
    debug_only(f->next = (TreeFragment*)0xcdcdcdcd);
    return f;
}

JS_REQUIRES_STACK void
TreeFragment::initialize(JSContext* cx, SlotList *globalSlots, bool speculate)
{
    this->dependentTrees.clear();
    this->linkedTrees.clear();
    this->globalSlots = globalSlots;

    /* Capture the coerced type of each active slot in the type map. */
    this->typeMap.captureTypes(cx, globalObj, *globalSlots, 0 /* callDepth */, speculate);
    this->nStackTypes = this->typeMap.length() - globalSlots->length();
    this->spOffsetAtEntry = cx->regs->sp - cx->fp()->base();

#ifdef DEBUG
    this->treeFileName = cx->fp()->script()->filename;
    this->treeLineNumber = js_FramePCToLineNumber(cx, cx->fp());
    this->treePCOffset = FramePCOffset(cx, cx->fp());
#endif
    this->script = cx->fp()->script();
    this->gcthings.clear();
    this->shapes.clear();
    this->unstableExits = NULL;
    this->sideExits.clear();

    /* Determine the native frame layout at the entry point. */
    this->nativeStackBase = (nStackTypes - (cx->regs->sp - cx->fp()->base())) *
                             sizeof(double);
    this->maxNativeStackSlots = nStackTypes;
    this->maxCallDepth = 0;
    this->execs = 0;
    this->iters = 0;
}

UnstableExit*
TreeFragment::removeUnstableExit(VMSideExit* exit)
{
    /* Now erase this exit from the unstable exit list. */
    UnstableExit** tail = &this->unstableExits;
    for (UnstableExit* uexit = this->unstableExits; uexit != NULL; uexit = uexit->next) {
        if (uexit->exit == exit) {
            *tail = uexit->next;
            return *tail;
        }
        tail = &uexit->next;
    }
    JS_NOT_REACHED("exit not in unstable exit list");
    return NULL;
}

#ifdef DEBUG
static void
AssertTreeIsUnique(TraceMonitor* tm, TreeFragment* f)
{
    JS_ASSERT(f->root == f);

    /*
     * Check for duplicate entry type maps.  This is always wrong and hints at
     * trace explosion since we are trying to stabilize something without
     * properly connecting peer edges.
     */
    for (TreeFragment* peer = LookupLoop(tm, f->ip, f->globalObj, f->globalShape, f->argc);
         peer != NULL;
         peer = peer->peer) {
        if (!peer->code() || peer == f)
            continue;
        JS_ASSERT(!f->typeMap.matches(peer->typeMap));
    }
}
#endif

static void
AttemptCompilation(JSContext *cx, JSObject* globalObj,
                   JSScript* script, jsbytecode* pc, uint32 argc)
{
    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);

    /* If we already permanently blacklisted the location, undo that. */
    Unblacklist(script, pc);
    ResetRecordingAttempts(cx, pc);

    /* Breathe new life into all peer fragments at the designated loop header. */
    TreeFragment* f = LookupLoop(tm, pc, globalObj, globalObj->shape(), argc);
    if (!f) {
        /*
         * If the global object's shape changed, we can't easily find the
         * corresponding loop header via a hash table lookup. In this
         * we simply bail here and hope that the fragment has another
         * outstanding compilation attempt. This case is extremely rare.
         */
        return;
    }
    JS_ASSERT(f->root == f);
    f = f->first;
    while (f) {
        JS_ASSERT(f->root == f);
        --f->recordAttempts;
        f->hits() = HOTLOOP;
        f = f->peer;
    }
}

static const CallInfo *
fcallinfo(LIns *ins)
{
    return ins->isop(LIR_calld) ? ins->callInfo() : NULL;
}

/*
 * Determine whether this operand is guaranteed to not overflow the specified
 * integer operation.
 */
static void
ChecksRequired(LOpcode op, LIns* op1, LIns* op2,
               bool* needsOverflowCheck, bool* needsNegZeroCheck)
{
    Interval x = Interval::of(op1, 3);
    Interval y = Interval::of(op2, 3);
    Interval z(0, 0);

    switch (op) {
      case LIR_addi:
        z = Interval::add(x, y);
        *needsNegZeroCheck = false;
        break;

      case LIR_subi:
        z = Interval::sub(x, y);
        *needsNegZeroCheck = false;
        break;
        
      case LIR_muli: {
        z = Interval::mul(x, y);
        // A would-be negative zero result can only occur if we have 
        // mul(0, -n) or mul(-n, 0), where n != 0.  In particular, a multiply
        // where one operand is a positive immediate cannot result in negative
        // zero.
        //
        // This assumes that -0 cannot be an operand;  if one had occurred we
        // would have already exited the trace in order to promote the
        // computation back to doubles.
        *needsNegZeroCheck = (x.canBeZero() && y.canBeNegative()) ||
                             (y.canBeZero() && x.canBeNegative());
        break;
      }

      default:
        JS_NOT_REACHED("needsOverflowCheck");
    }

    *needsOverflowCheck = z.hasOverflowed;
}

/*
 * JSStackFrame::numActualArgs is only defined for function frames. Since the
 * actual arguments of the entry frame are kept on trace, argc is included in
 * the tuple identifying a fragment so that two fragments for the same loop but
 * recorded with different number of actual arguments are treated as two
 * completely separate trees. For this particular use, we define the number of
 * actuals for global and eval frames to be 0.
 */
static inline uintN
entryFrameArgc(JSContext *cx)
{
    JSStackFrame *fp = cx->fp();
    return fp->isGlobalFrame() || fp->isEvalFrame() ? 0 : fp->numActualArgs();
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
VisitStackAndArgs(Visitor &visitor, JSStackFrame *fp, JSStackFrame *next, Value *stack)
{
    if (JS_LIKELY(!next->hasOverflowArgs()))
        return visitor.visitStackSlots(stack, next->formalArgsEnd() - stack, fp);

    /*
     * In the case of nactual > nformal, the formals are copied by the VM onto
     * the top of the stack. We only want to mark the formals once, so we
     * carefully mark only the canonical actual arguments (as defined by
     * JSStackFrame::canonicalActualArg).
     */
    uintN nactual = next->numActualArgs();
    Value *actuals = next->actualArgs();
    size_t nstack = (actuals - 2 /* callee,this */) - stack;
    if (!visitor.visitStackSlots(stack, nstack, fp))
        return false;
    uintN nformal = next->numFormalArgs();
    Value *formals = next->formalArgs();
    if (!visitor.visitStackSlots(formals - 2, 2 + nformal, fp))
        return false;
    return visitor.visitStackSlots(actuals + nformal, nactual - nformal, fp);
}

/*
 * Visit the values in the given JSStackFrame that the tracer cares about. This
 * visitor function is (implicitly) the primary definition of the native stack
 * area layout. There are a few other independent pieces of code that must be
 * maintained to assume the same layout. They are marked like this:
 *
 *   Duplicate native stack layout computation: see VisitFrameSlots header comment.
 */
template <typename Visitor>
static JS_REQUIRES_STACK bool
VisitFrameSlots(Visitor &visitor, JSContext *cx, unsigned depth, JSStackFrame *fp,
                JSStackFrame *next)
{
    JS_ASSERT_IF(!next, cx->fp() == fp);

    if (depth > 0 && !VisitFrameSlots(visitor, cx, depth-1, fp->prev(), fp))
        return false;

    if (depth == 0) {
        if (fp->isGlobalFrame()) {
            visitor.setStackSlotKind("global");
            Value *base = fp->slots() + fp->globalScript()->nfixed;
            if (next)
                return VisitStackAndArgs(visitor, fp, next, base);
            return visitor.visitStackSlots(base, cx->regs->sp - base, fp);
        }

        if (JS_UNLIKELY(fp->isEvalFrame())) {
            visitor.setStackSlotKind("eval");
            if (!visitor.visitStackSlots(&fp->calleeValue(), 2, fp))
                return false;
        } else {
            /*
             * Only the bottom function frame must visit its arguments; for all
             * other frames, arguments are visited by the prev-frame.
             */
            visitor.setStackSlotKind("args");
            uintN nformal = fp->numFormalArgs();
            if (!visitor.visitStackSlots(fp->formalArgs() - 2, 2 + nformal, fp))
                return false;
            if (JS_UNLIKELY(fp->hasOverflowArgs())) {
                if (!visitor.visitStackSlots(fp->actualArgs() + nformal,
                                             fp->numActualArgs() - nformal, fp))
                    return false;
            }
        }
    }

    JS_ASSERT(fp->isFunctionFrame());

    /*
     * We keep two members of JSStackFrame on trace: the args obj pointer and
     * the scope chain pointer. The visitor must take care not to treat these
     * as js::Value-typed variables, since they are unboxed pointers.
     * Moreover, JSStackFrame compresses the args obj pointer with nactual, so
     * fp->addressOfArgs() is not really a JSObject**: the visitor must treat
     * !fp->hasArgsObj() as a null args obj pointer. Hence, visitFrameObjPtr
     * is only passed a void *.
     */
    visitor.setStackSlotKind("arguments");
    if (!visitor.visitFrameObjPtr(fp->addressOfArgs(), fp))
        return false;
    visitor.setStackSlotKind("scopeChain");
    if (!visitor.visitFrameObjPtr(fp->addressOfScopeChain(), fp))
        return false;

    visitor.setStackSlotKind("slots");
    if (next)
        return VisitStackAndArgs(visitor, fp, next, fp->slots());
    return visitor.visitStackSlots(fp->slots(), cx->regs->sp - fp->slots(), fp);
}

// Number of native frame slots used for 'special' values between args and vars.
// Currently the two values are |arguments| (args object) and |scopeChain|.
const int SPECIAL_FRAME_SLOTS = 2;

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
VisitStackSlots(Visitor &visitor, JSContext *cx, unsigned callDepth)
{
    return VisitFrameSlots(visitor, cx, callDepth, cx->fp(), NULL);
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitGlobalSlots(Visitor &visitor, JSContext *cx, JSObject *globalObj,
                 unsigned ngslots, uint16 *gslots)
{
    for (unsigned n = 0; n < ngslots; ++n) {
        unsigned slot = gslots[n];
        visitor.visitGlobalSlot(&globalObj->getSlotRef(slot), n, slot);
    }
}

class AdjustCallerTypeVisitor;

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitGlobalSlots(Visitor &visitor, JSContext *cx, SlotList &gslots)
{
    VisitGlobalSlots(visitor, cx, cx->fp()->scopeChain().getGlobal(),
                     gslots.length(), gslots.data());
}


template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitSlots(Visitor& visitor, JSContext* cx, JSObject* globalObj,
           unsigned callDepth, unsigned ngslots, uint16* gslots)
{
    if (VisitStackSlots(visitor, cx, callDepth))
        VisitGlobalSlots(visitor, cx, globalObj, ngslots, gslots);
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitSlots(Visitor& visitor, JSContext* cx, unsigned callDepth,
           unsigned ngslots, uint16* gslots)
{
    VisitSlots(visitor, cx, cx->fp()->scopeChain().getGlobal(),
               callDepth, ngslots, gslots);
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitSlots(Visitor &visitor, JSContext *cx, JSObject *globalObj,
           unsigned callDepth, const SlotList& slots)
{
    VisitSlots(visitor, cx, globalObj, callDepth, slots.length(),
               slots.data());
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitSlots(Visitor &visitor, JSContext *cx, unsigned callDepth,
           const SlotList& slots)
{
    VisitSlots(visitor, cx, cx->fp()->scopeChain().getGlobal(),
               callDepth, slots.length(), slots.data());
}


class SlotVisitorBase {
#if defined JS_JIT_SPEW
protected:
    char const *mStackSlotKind;
public:
    SlotVisitorBase() : mStackSlotKind(NULL) {}
    JS_ALWAYS_INLINE const char *stackSlotKind() { return mStackSlotKind; }
    JS_ALWAYS_INLINE void setStackSlotKind(char const *k) {
        mStackSlotKind = k;
    }
#else
public:
    JS_ALWAYS_INLINE const char *stackSlotKind() { return NULL; }
    JS_ALWAYS_INLINE void setStackSlotKind(char const *k) {}
#endif
};

struct CountSlotsVisitor : public SlotVisitorBase
{
    unsigned mCount;
    bool mDone;
    const void* mStop;
public:
    JS_ALWAYS_INLINE CountSlotsVisitor(const void* stop = NULL) :
        mCount(0),
        mDone(false),
        mStop(stop)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        if (mDone)
            return false;
        if (mStop && size_t(((const Value *)mStop) - vp) < count) {
            mCount += size_t(((const Value *)mStop) - vp);
            mDone = true;
            return false;
        }
        mCount += count;
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        if (mDone)
            return false;
        if (mStop && mStop == p) {
            mDone = true;
            return false;
        }
        mCount++;
        return true;
    }

    JS_ALWAYS_INLINE unsigned count() {
        return mCount;
    }

    JS_ALWAYS_INLINE bool stopped() {
        return mDone;
    }
};

static JS_REQUIRES_STACK JS_ALWAYS_INLINE unsigned
CountStackAndArgs(JSStackFrame *next, Value *stack)
{
    if (JS_LIKELY(!next->hasOverflowArgs()))
        return (Value *)next - stack;
    size_t nvals = (next->formalArgs() - 2 /* callee, this */) - stack;
    JS_ASSERT(nvals == unsigned((next->actualArgs() - 2) - stack) + (2 + next->numActualArgs()));
    return nvals;
}

static JS_ALWAYS_INLINE uintN
NumSlotsBeforeFixed(JSStackFrame *fp)
{
    uintN numArgs = fp->isEvalFrame() ? 0 : Max(fp->numActualArgs(), fp->numFormalArgs());
    return 2 + numArgs + SPECIAL_FRAME_SLOTS;
}

/*
 * Calculate the total number of native frame slots we need from this frame all
 * the way back to the entry frame, including the current stack usage.
 *
 * Duplicate native stack layout computation: see VisitFrameSlots header comment.
 */
JS_REQUIRES_STACK unsigned
NativeStackSlots(JSContext *cx, unsigned callDepth)
{
    JSStackFrame *fp = cx->fp();
    JSStackFrame *next = NULL;
    unsigned slots = 0;
    unsigned depth = callDepth;

    for (; depth > 0; --depth, next = fp, fp = fp->prev()) {
        JS_ASSERT(fp->isFunctionFrame() && !fp->isEvalFrame());
        slots += SPECIAL_FRAME_SLOTS;
        if (next)
            slots += CountStackAndArgs(next, fp->slots());
        else
            slots += cx->regs->sp - fp->slots();
    }

    Value *start;
    if (fp->isGlobalFrame()) {
        start = fp->slots() + fp->globalScript()->nfixed;
    } else {
        start = fp->slots();
        slots += NumSlotsBeforeFixed(fp);
    }
    if (next)
        slots += CountStackAndArgs(next, start);
    else
        slots += cx->regs->sp - start;

#ifdef DEBUG
    CountSlotsVisitor visitor;
    VisitStackSlots(visitor, cx, callDepth);
    JS_ASSERT(visitor.count() == slots && !visitor.stopped());
#endif
    return slots;
}

class CaptureTypesVisitor : public SlotVisitorBase
{
    JSContext* mCx;
    JSValueType* mTypeMap;
    JSValueType* mPtr;
    Oracle   * mOracle;

public:
    JS_ALWAYS_INLINE CaptureTypesVisitor(JSContext* cx, JSValueType* typeMap, bool speculate) :
        mCx(cx),
        mTypeMap(typeMap),
        mPtr(typeMap),
        mOracle(speculate ? JS_TRACE_MONITOR(cx).oracle : NULL) {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
            JSValueType type = getCoercedType(*vp);
            if (type == JSVAL_TYPE_INT32 && (!mOracle || mOracle->isGlobalSlotUndemotable(mCx, slot)))
                type = JSVAL_TYPE_DOUBLE;
            JS_ASSERT(type != JSVAL_TYPE_BOXED);
            debug_only_printf(LC_TMTracer,
                              "capture type global%d: %c\n",
                              n, TypeToChar(type));
            *mPtr++ = type;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, int count, JSStackFrame* fp) {
        for (int i = 0; i < count; ++i) {
            JSValueType type = getCoercedType(vp[i]);
            if (type == JSVAL_TYPE_INT32 && (!mOracle || mOracle->isStackSlotUndemotable(mCx, length())))
                type = JSVAL_TYPE_DOUBLE;
            JS_ASSERT(type != JSVAL_TYPE_BOXED);
            debug_only_printf(LC_TMTracer,
                              "capture type %s%d: %c\n",
                              stackSlotKind(), i, TypeToChar(type));
            *mPtr++ = type;
        }
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        JSValueType type = getFrameObjPtrTraceType(p, fp);
        debug_only_printf(LC_TMTracer,
                          "capture type %s%d: %c\n",
                          stackSlotKind(), 0, TypeToChar(type));
        *mPtr++ = type;
        return true;
    }

    JS_ALWAYS_INLINE uintptr_t length() {
        return mPtr - mTypeMap;
    }
};

void
TypeMap::set(unsigned stackSlots, unsigned ngslots,
             const JSValueType* stackTypeMap, const JSValueType* globalTypeMap)
{
    setLength(ngslots + stackSlots);
    memcpy(data(), stackTypeMap, stackSlots * sizeof(JSValueType));
    memcpy(data() + stackSlots, globalTypeMap, ngslots * sizeof(JSValueType));
}

/*
 * Capture the type map for the selected slots of the global object and currently pending
 * stack frames.
 */
JS_REQUIRES_STACK void
TypeMap::captureTypes(JSContext* cx, JSObject* globalObj, SlotList& slots, unsigned callDepth,
                      bool speculate)
{
    setLength(NativeStackSlots(cx, callDepth) + slots.length());
    CaptureTypesVisitor visitor(cx, data(), speculate);
    VisitSlots(visitor, cx, globalObj, callDepth, slots);
    JS_ASSERT(visitor.length() == length());
}

JS_REQUIRES_STACK void
TypeMap::captureMissingGlobalTypes(JSContext* cx, JSObject* globalObj, SlotList& slots, unsigned stackSlots,
                                   bool speculate)
{
    unsigned oldSlots = length() - stackSlots;
    int diff = slots.length() - oldSlots;
    JS_ASSERT(diff >= 0);
    setLength(length() + diff);
    CaptureTypesVisitor visitor(cx, data() + stackSlots + oldSlots, speculate);
    VisitGlobalSlots(visitor, cx, globalObj, diff, slots.data() + oldSlots);
}

/* Compare this type map to another one and see whether they match. */
bool
TypeMap::matches(TypeMap& other) const
{
    if (length() != other.length())
        return false;
    return !memcmp(data(), other.data(), length());
}

void
TypeMap::fromRaw(JSValueType* other, unsigned numSlots)
{
    unsigned oldLength = length();
    setLength(length() + numSlots);
    for (unsigned i = 0; i < numSlots; i++)
        get(oldLength + i) = other[i];
}

/*
 * Use the provided storage area to create a new type map that contains the
 * partial type map with the rest of it filled up from the complete type
 * map.
 */
static void
MergeTypeMaps(JSValueType** partial, unsigned* plength, JSValueType* complete, unsigned clength, JSValueType* mem)
{
    unsigned l = *plength;
    JS_ASSERT(l < clength);
    memcpy(mem, *partial, l * sizeof(JSValueType));
    memcpy(mem + l, complete + l, (clength - l) * sizeof(JSValueType));
    *partial = mem;
    *plength = clength;
}

/*
 * Specializes a tree to any specifically missing globals, including any
 * dependent trees.
 */
static JS_REQUIRES_STACK void
SpecializeTreesToLateGlobals(JSContext* cx, TreeFragment* root, JSValueType* globalTypeMap,
                            unsigned numGlobalSlots)
{
    for (unsigned i = root->nGlobalTypes(); i < numGlobalSlots; i++)
        root->typeMap.add(globalTypeMap[i]);

    JS_ASSERT(root->nGlobalTypes() == numGlobalSlots);

    for (unsigned i = 0; i < root->dependentTrees.length(); i++) {
        TreeFragment* tree = root->dependentTrees[i];
        if (tree->code() && tree->nGlobalTypes() < numGlobalSlots)
            SpecializeTreesToLateGlobals(cx, tree, globalTypeMap, numGlobalSlots);
    }
    for (unsigned i = 0; i < root->linkedTrees.length(); i++) {
        TreeFragment* tree = root->linkedTrees[i];
        if (tree->code() && tree->nGlobalTypes() < numGlobalSlots)
            SpecializeTreesToLateGlobals(cx, tree, globalTypeMap, numGlobalSlots);
    }
}

/* Specializes a tree to any missing globals, including any dependent trees. */
static JS_REQUIRES_STACK void
SpecializeTreesToMissingGlobals(JSContext* cx, JSObject* globalObj, TreeFragment* root)
{
    /* If we already have a bunch of peer trees, try to be as generic as possible. */
    size_t count = 0;
    for (TreeFragment *f = root->first; f; f = f->peer, ++count);
    bool speculate = count < MAXPEERS-1;

    root->typeMap.captureMissingGlobalTypes(cx, globalObj, *root->globalSlots, root->nStackTypes,
                                            speculate);
    JS_ASSERT(root->globalSlots->length() == root->typeMap.length() - root->nStackTypes);

    SpecializeTreesToLateGlobals(cx, root, root->globalTypeMap(), root->nGlobalTypes());
}

static void
ResetJITImpl(JSContext* cx);

#ifdef MOZ_TRACEVIS
static JS_INLINE void
ResetJIT(JSContext* cx, TraceVisFlushReason r)
{
    LogTraceVisEvent(cx, S_RESET, r);
    ResetJITImpl(cx);
}
#else
# define ResetJIT(cx, reason) ResetJITImpl(cx)
#endif

void
FlushJITCache(JSContext *cx)
{
    ResetJIT(cx, FR_OOM);
}

static void
TrashTree(TreeFragment* f);

JS_REQUIRES_STACK
TraceRecorder::TraceRecorder(JSContext* cx, VMSideExit* anchor, VMFragment* fragment,
                             unsigned stackSlots, unsigned ngslots, JSValueType* typeMap,
                             VMSideExit* innermost, JSScript* outerScript, jsbytecode* outerPC,
                             uint32 outerArgc, bool speculate)
  : cx(cx),
    traceMonitor(&JS_TRACE_MONITOR(cx)),
    oracle(speculate ? JS_TRACE_MONITOR(cx).oracle : NULL),
    fragment(fragment),
    tree(fragment->root),
    globalObj(tree->globalObj),
    outerScript(outerScript),
    outerPC(outerPC),
    outerArgc(outerArgc),
    anchor(anchor),
    cx_ins(NULL),
    eos_ins(NULL),
    eor_ins(NULL),
    loopLabel(NULL),
    importTypeMap(&tempAlloc()),
    lirbuf(new (tempAlloc()) LirBuffer(tempAlloc())),
    mark(*traceMonitor->traceAlloc),
    numSideExitsBefore(tree->sideExits.length()),
    tracker(),
    nativeFrameTracker(),
    global_slots(NULL),
    callDepth(anchor ? anchor->calldepth : 0),
    atoms(FrameAtomBase(cx, cx->fp())),
    consts(cx->fp()->script()->constOffset
           ? cx->fp()->script()->consts()->vector
           : NULL),
    strictModeCode_ins(NULL),
    cfgMerges(&tempAlloc()),
    trashSelf(false),
    whichTreesToTrash(&tempAlloc()),
    guardedShapeTable(cx),
    rval_ins(NULL),
    native_rval_ins(NULL),
    newobj_ins(NULL),
    pendingSpecializedNative(NULL),
    pendingUnboxSlot(NULL),
    pendingGuardCondition(NULL),
    pendingLoop(true),
    generatedSpecializedNative(),
    tempTypeMap(cx),
    w(&tempAlloc(), lirbuf)
{
    JS_ASSERT(globalObj == cx->fp()->scopeChain().getGlobal());
    JS_ASSERT(globalObj->hasOwnShape());
    JS_ASSERT(cx->regs->pc == (jsbytecode*)fragment->ip);

#ifdef DEBUG
    lirbuf->printer = new (tempAlloc()) LInsPrinter(tempAlloc(), TM_NUM_USED_ACCS);
#endif

    /*
     * Reset the fragment state we care about in case we got a recycled
     * fragment.  This includes resetting any profiling data we might have
     * accumulated.
     */
    fragment->lastIns = NULL;
    fragment->setCode(NULL);
    fragment->lirbuf = lirbuf;
    verbose_only( fragment->profCount = 0; )
    verbose_only( fragment->nStaticExits = 0; )
    verbose_only( fragment->nCodeBytes = 0; )
    verbose_only( fragment->nExitBytes = 0; )
    verbose_only( fragment->guardNumberer = 1; )
    verbose_only( fragment->guardsForFrag = NULL; )
    verbose_only( fragment->loopLabel = NULL; )

    /*
     * Don't change fragment->profFragID, though.  Once the identity of the
     * Fragment is set up (for profiling purposes), we can't change it.
     */

    if (!guardedShapeTable.init())
        abort();

#ifdef JS_JIT_SPEW
    debug_only_print0(LC_TMMinimal, "\n");
    debug_only_printf(LC_TMMinimal, "Recording starting from %s:%u@%u (FragID=%06u)\n",
                      tree->treeFileName, tree->treeLineNumber, tree->treePCOffset,
                      fragment->profFragID);

    debug_only_printf(LC_TMTracer, "globalObj=%p, shape=%d\n",
                      (void*)this->globalObj, this->globalObj->shape());
    debug_only_printf(LC_TMTreeVis, "TREEVIS RECORD FRAG=%p ANCHOR=%p\n", (void*)fragment,
                      (void*)anchor);
#endif

    /* This creates the LIR writer pipeline. */
    w.init(&LogController);

    w.start();

    for (int i = 0; i < NumSavedRegs; ++i)
        w.paramp(i, 1);
#ifdef DEBUG
    for (int i = 0; i < NumSavedRegs; ++i)
        w.name(lirbuf->savedRegs[i], regNames[REGNUM(Assembler::savedRegs[i])]);
#endif

    lirbuf->state = w.name(w.paramp(0, 0), "state");

    if (fragment == fragment->root) {
        w.comment("begin-loop");
        InitConst(loopLabel) = w.label();
    }
    w.comment("begin-setup");

    // if profiling, drop a label, so the assembler knows to put a
    // frag-entry-counter increment at this point.  If there's a
    // loopLabel, use that; else we'll have to make a dummy label
    // especially for this purpose.
    verbose_only( if (LogController.lcbits & LC_FragProfile) {
        LIns* entryLabel = NULL;
        if (fragment == fragment->root) {
            entryLabel = loopLabel;
        } else {
            entryLabel = w.label();
        }
        NanoAssert(entryLabel);
        NanoAssert(!fragment->loopLabel);
        fragment->loopLabel = entryLabel;
    })

    lirbuf->sp = w.name(w.ldpStateField(sp), "sp");
    lirbuf->rp = w.name(w.ldpStateField(rp), "rp");
    InitConst(cx_ins) = w.name(w.ldpStateField(cx), "cx");
    InitConst(eos_ins) = w.name(w.ldpStateField(eos), "eos");
    InitConst(eor_ins) = w.name(w.ldpStateField(eor), "eor");

    strictModeCode_ins = w.name(w.immi(cx->fp()->script()->strictModeCode), "strict");

    /* If we came from exit, we might not have enough global types. */
    if (tree->globalSlots->length() > tree->nGlobalTypes())
        SpecializeTreesToMissingGlobals(cx, globalObj, tree);

    /* read into registers all values on the stack and all globals we know so far */
    import(tree, lirbuf->sp, stackSlots, ngslots, callDepth, typeMap);

    if (fragment == fragment->root) {
        /*
         * We poll the operation callback request flag. It is updated asynchronously whenever
         * the callback is to be invoked. We can use w.nameImmpNonGC here as JIT-ed code is per
         * thread and cannot outlive the corresponding JSThreadData.
         */
        w.comment("begin-interruptFlags-check");
        LIns* flagptr = w.nameImmpNonGC((void *) &JS_THREAD_DATA(cx)->interruptFlags);
        LIns* x = w.ldiVolatile(flagptr);
        guard(true, w.eqi0(x), TIMEOUT_EXIT);
        w.comment("end-interruptFlags-check");

        /*
         * Count the number of iterations run by a trace, so that we can blacklist if
         * the trace runs too few iterations to be worthwhile. Do this only if the methodjit
         * is on--otherwise we must try to trace as much as possible.
         */
#ifdef JS_METHODJIT
        if (cx->methodJitEnabled) {
            w.comment("begin-count-loop-iterations");
            LIns* counterPtr = w.nameImmpNonGC((void *) &JS_THREAD_DATA(cx)->iterationCounter);
            LIns* counterValue = w.ldiVolatile(counterPtr);
            LIns* test = w.ltiN(counterValue, LOOP_COUNT_MAX);
            LIns *branch = w.jfUnoptimizable(test);
            /* 
             * stiVolatile() uses ACCSET_STORE_ANY;  If LICM is implemented
             * (bug 545406) this counter will need its own region.
             */
            w.stiVolatile(w.addi(counterValue, w.nameImmi(1)), counterPtr);
            w.label(branch);
            w.comment("end-count-loop-iterations");
        }
#endif
    }

    /*
     * If we are attached to a tree call guard, make sure the guard the inner
     * tree exited from is what we expect it to be.
     */
    if (anchor && anchor->exitType == NESTED_EXIT) {
        LIns* nested_ins = w.name(w.ldpStateField(outermostTreeExitGuard),
                                  "outermostTreeExitGuard");
        guard(true, w.eqp(nested_ins, w.nameImmpNonGC(innermost)), NESTED_EXIT);
    }

    w.comment("end-setup");
}

TraceRecorder::~TraceRecorder()
{
    /* Should already have been adjusted by callers before calling delete. */
    JS_ASSERT(traceMonitor->recorder != this);

    if (trashSelf)
        TrashTree(fragment->root);

    for (unsigned int i = 0; i < whichTreesToTrash.length(); i++)
        TrashTree(whichTreesToTrash[i]);

    /* Purge the tempAlloc used during recording. */
    tempAlloc().reset();

    forgetGuardedShapes();
}

inline bool
TraceMonitor::outOfMemory() const
{
    return dataAlloc->outOfMemory() ||
           tempAlloc->outOfMemory() ||
           traceAlloc->outOfMemory();
}

/*
 * This function destroys the recorder after a successful recording, possibly
 * starting a suspended outer recorder.
 */
AbortableRecordingStatus
TraceRecorder::finishSuccessfully()
{
    JS_ASSERT(traceMonitor->recorder == this);
    JS_ASSERT(fragment->lastIns && fragment->code());

    AUDIT(traceCompleted);
    mark.commit();

    /* Grab local copies of members needed after |delete this|. */
    JSContext* localcx = cx;
    TraceMonitor* localtm = traceMonitor;

    localtm->recorder = NULL;
    delete this;

    /* Catch OOM that occurred during recording. */
    if (localtm->outOfMemory() || OverfullJITCache(localtm)) {
        ResetJIT(localcx, FR_OOM);
        return ARECORD_ABORTED;
    }
    return ARECORD_COMPLETED;
}

/* This function aborts a recorder and any pending outer recorders. */
JS_REQUIRES_STACK TraceRecorder::AbortResult
TraceRecorder::finishAbort(const char* reason)
{
    JS_ASSERT(traceMonitor->recorder == this);
    JS_ASSERT(!fragment->code());

    AUDIT(recorderAborted);
#ifdef DEBUG
    debug_only_printf(LC_TMMinimal,
                      "Abort recording of tree %s:%d@%d at %s:%d@%d: %s.\n",
                      tree->treeFileName,
                      tree->treeLineNumber,
                      tree->treePCOffset,
                      cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()),
                      reason);
#endif
    Backoff(cx, (jsbytecode*) fragment->root->ip, fragment->root);

    /*
     * If this is the primary trace and we didn't succeed compiling, trash the
     * tree. Otherwise, remove the VMSideExits we added while recording, which
     * are about to be invalid.
     *
     * BIG FAT WARNING: resetting the length is only a valid strategy as long as
     * there may be only one recorder active for a single TreeInfo at a time.
     * Otherwise, we may be throwing away another recorder's valid side exits.
     */
    if (fragment->root == fragment) {
        TrashTree(fragment->toTreeFragment());
    } else {
        JS_ASSERT(numSideExitsBefore <= fragment->root->sideExits.length());
        fragment->root->sideExits.setLength(numSideExitsBefore);
    }

    /* Grab local copies of members needed after |delete this|. */
    JSContext* localcx = cx;
    TraceMonitor* localtm = traceMonitor;

    localtm->recorder = NULL;
    delete this;
    if (localtm->outOfMemory() || OverfullJITCache(localtm)) {
        ResetJIT(localcx, FR_OOM);
        return JIT_RESET;
    }
    return NORMAL_ABORT;
}

inline LIns*
TraceRecorder::w_immpObjGC(JSObject* obj)
{
    tree->gcthings.addUnique(ObjectValue(*obj));
    return w.immpNonGC((void*)obj);
}

inline LIns*
TraceRecorder::w_immpFunGC(JSFunction* fun)
{
    tree->gcthings.addUnique(ObjectValue(*fun));
    return w.immpNonGC((void*)fun);
}

inline LIns*
TraceRecorder::w_immpStrGC(JSString* str)
{
    tree->gcthings.addUnique(StringValue(str));
    return w.immpNonGC((void*)str);
}

inline LIns*
TraceRecorder::w_immpShapeGC(const Shape* shape)
{
    tree->shapes.addUnique(shape);
    return w.immpNonGC((void*)shape);
}

inline LIns*
TraceRecorder::w_immpIdGC(jsid id)
{
    if (JSID_IS_GCTHING(id))
        tree->gcthings.addUnique(IdToValue(id));
    return w.immpNonGC((void*)JSID_BITS(id));
}

ptrdiff_t
TraceRecorder::nativeGlobalSlot(const Value* p) const
{
    JS_ASSERT(isGlobal(p));
    return ptrdiff_t(p - globalObj->slots);
}

/* Determine the offset in the native global frame for a jsval we track. */
ptrdiff_t
TraceRecorder::nativeGlobalOffset(const Value* p) const
{
    return nativeGlobalSlot(p) * sizeof(double);
}

/* Determine whether a value is a global stack slot. */
bool
TraceRecorder::isGlobal(const Value* p) const
{
    return (size_t(p - globalObj->slots) < globalObj->numSlots());
}

bool
TraceRecorder::isVoidPtrGlobal(const void* p) const
{
    return isGlobal((const Value *)p);
}

/*
 * Return the offset in the native stack for the given jsval. More formally,
 * |p| must be the address of a jsval that is represented in the native stack
 * area. The return value is the offset, from TracerState::stackBase, in bytes,
 * where the native representation of |*p| is stored. To get the offset
 * relative to TracerState::sp, subtract TreeFragment::nativeStackBase.
 */
JS_REQUIRES_STACK ptrdiff_t
TraceRecorder::nativeStackOffsetImpl(const void* p) const
{
    CountSlotsVisitor visitor(p);
    VisitStackSlots(visitor, cx, callDepth);
    size_t offset = visitor.count() * sizeof(double);

    /*
     * If it's not in a pending frame, it must be on the stack of the current
     * frame above sp but below fp->slots() + script->nslots.
     */
    if (!visitor.stopped()) {
        const Value *vp = (const Value *)p;
        JS_ASSERT(size_t(vp - cx->fp()->slots()) < cx->fp()->numSlots());
        offset += size_t(vp - cx->regs->sp) * sizeof(double);
    }
    return offset;
}

JS_REQUIRES_STACK inline ptrdiff_t
TraceRecorder::nativeStackOffset(const Value* p) const
{
    return nativeStackOffsetImpl(p);
}

JS_REQUIRES_STACK inline ptrdiff_t
TraceRecorder::nativeStackSlotImpl(const void* p) const
{
    return nativeStackOffsetImpl(p) / sizeof(double);
}

JS_REQUIRES_STACK inline ptrdiff_t
TraceRecorder::nativeStackSlot(const Value* p) const
{
    return nativeStackSlotImpl(p);
}

/*
 * Return the offset, from TracerState:sp, for the given jsval. Shorthand for:
 *  -TreeFragment::nativeStackBase + nativeStackOffset(p).
 */
inline JS_REQUIRES_STACK ptrdiff_t
TraceRecorder::nativespOffsetImpl(const void* p) const
{
    return -tree->nativeStackBase + nativeStackOffsetImpl(p);
}

inline JS_REQUIRES_STACK ptrdiff_t
TraceRecorder::nativespOffset(const Value* p) const
{
    return nativespOffsetImpl(p);
}

/* Track the maximum number of native frame slots we need during execution. */
inline void
TraceRecorder::trackNativeStackUse(unsigned slots)
{
    if (slots > tree->maxNativeStackSlots)
        tree->maxNativeStackSlots = slots;
}

/*
 * Unbox a jsval into a slot. Slots are wide enough to hold double values
 * directly (instead of storing a pointer to them). We assert instead of
 * type checking. The caller must ensure the types are compatible.
 */
static inline void
ValueToNative(const Value &v, JSValueType type, double* slot)
{
    JS_ASSERT(type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET);
    if (type > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)
        v.unboxNonDoubleTo((uint64 *)slot);
    else if (type == JSVAL_TYPE_INT32)
        *(int32_t *)slot = v.isInt32() ? v.toInt32() : (int32_t)v.toDouble();
    else
        *(double *)slot = v.toNumber();

#ifdef DEBUG
    int32_t _;
    switch (type) {
      case JSVAL_TYPE_NONFUNOBJ: {
        JS_ASSERT(!IsFunctionObject(v));
        debug_only_printf(LC_TMTracer,
                          "object<%p:%s> ", (void*)*(JSObject **)slot,
                          v.toObject().getClass()->name);
        return;
      }

      case JSVAL_TYPE_INT32:
        JS_ASSERT(v.isInt32() || (v.isDouble() && JSDOUBLE_IS_INT32(v.toDouble(), &_)));
        debug_only_printf(LC_TMTracer, "int<%d> ", *(jsint *)slot);
        return;

      case JSVAL_TYPE_DOUBLE:
        JS_ASSERT(v.isNumber());
        debug_only_printf(LC_TMTracer, "double<%g> ", *(jsdouble *)slot);
        return;

      case JSVAL_TYPE_BOXED:
        JS_NOT_REACHED("found jsval type in an entry type map");
        return;

      case JSVAL_TYPE_STRING:
        JS_ASSERT(v.isString());
        debug_only_printf(LC_TMTracer, "string<%p> ", (void*)*(JSString**)slot);
        return;

      case JSVAL_TYPE_NULL:
        JS_ASSERT(v.isNull());
        debug_only_print0(LC_TMTracer, "null ");
        return;

      case JSVAL_TYPE_BOOLEAN:
        JS_ASSERT(v.isBoolean());
        debug_only_printf(LC_TMTracer, "special<%d> ", *(JSBool*)slot);
        return;

      case JSVAL_TYPE_UNDEFINED:
        JS_ASSERT(v.isUndefined());
        debug_only_print0(LC_TMTracer, "undefined ");
        return;

      case JSVAL_TYPE_MAGIC:
        JS_ASSERT(v.isMagic());
        debug_only_print0(LC_TMTracer, "hole ");
        return;

      case JSVAL_TYPE_FUNOBJ: {
        JS_ASSERT(IsFunctionObject(v));
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, &v.toObject());
        debug_only_printf(LC_TMTracer,
                          "function<%p:%s> ", (void*)*(JSObject **)slot,
                          fun->atom
                          ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
                          : "unnamed");
        return;
      }
      default:
        JS_NOT_REACHED("unexpected type");
        break;
    }
#endif
}

void
TraceMonitor::flush()
{
    /* flush should only be called after all recorders have been aborted. */
    JS_ASSERT(!recorder);
    AUDIT(cacheFlushed);

    // recover profiling data from expiring Fragments
    verbose_only(
        for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
            for (TreeFragment *f = vmfragments[i]; f; f = f->next) {
                JS_ASSERT(f->root == f);
                for (TreeFragment *p = f; p; p = p->peer)
                    FragProfiling_FragFinalizer(p, this);
            }
        }
    )

    verbose_only(
        for (Seq<Fragment*>* f = branches; f; f = f->tail)
            FragProfiling_FragFinalizer(f->head, this);
    )

    flushEpoch++;

    frameCache->reset();
    dataAlloc->reset();
    traceAlloc->reset();
    codeAlloc->reset();
    tempAlloc->reset();
    reTempAlloc->reset();
    oracle->clear();
    loopProfiles->clear();

    Allocator& alloc = *dataAlloc;

    for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
        globalStates[i].globalShape = -1;
        globalStates[i].globalSlots = new (alloc) SlotList(&alloc);
    }

    assembler = new (alloc) Assembler(*codeAlloc, alloc, alloc, core, &LogController, avmplus::AvmCore::config);
    verbose_only( branches = NULL; )

    PodArrayZero(vmfragments);
    reFragments = new (alloc) REHashMap(alloc);

    needFlush = JS_FALSE;
}

inline bool
HasUnreachableGCThings(TreeFragment *f)
{
    /*
     * We do not check here for dead scripts as JSScript is not a GC thing.
     * Instead PurgeScriptFragments is used to remove dead script fragments.
     * See bug 584860.
     */
    if (IsAboutToBeFinalized(f->globalObj))
        return true;
    Value* vp = f->gcthings.data();
    for (unsigned len = f->gcthings.length(); len; --len) {
        Value &v = *vp++;
        JS_ASSERT(v.isMarkable());
        if (IsAboutToBeFinalized(v.toGCThing()))
            return true;
    }
    const Shape** shapep = f->shapes.data();
    for (unsigned len = f->shapes.length(); len; --len) {
        const Shape* shape = *shapep++;
        if (!shape->marked())
            return true;
    }
    return false;
}

void
TraceMonitor::sweep()
{
    JS_ASSERT(!ontrace());
    debug_only_print0(LC_TMTracer, "Purging fragments with dead things");

    for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
        TreeFragment** fragp = &vmfragments[i];
        while (TreeFragment* frag = *fragp) {
            TreeFragment* peer = frag;
            do {
                if (HasUnreachableGCThings(peer))
                    break;
                peer = peer->peer;
            } while (peer);
            if (peer) {
                debug_only_printf(LC_TMTracer,
                                  "TreeFragment peer %p has dead gc thing."
                                  "Disconnecting tree %p with ip %p\n",
                                  (void *) peer, (void *) frag, frag->ip);
                JS_ASSERT(frag->root == frag);
                *fragp = frag->next;
                do {
                    verbose_only( FragProfiling_FragFinalizer(frag, this); )
                    TrashTree(frag);
                    frag = frag->peer;
                } while (frag);
            } else {
                fragp = &frag->next;
            }
        }
    }

    if (recorder && HasUnreachableGCThings(recorder->getTree()))
        recorder->finishAbort("dead GC things");
}

/*
 * Box a value from the native stack back into the Value format.
 */
static inline void
NativeToValue(JSContext* cx, Value& v, JSValueType type, double* slot)
{
    if (type == JSVAL_TYPE_DOUBLE) {
        v.setNumber(*slot);
    } else if (JS_LIKELY(type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET)) {
        v.boxNonDoubleFrom(type, (uint64 *)slot);
    } else if (type == JSVAL_TYPE_STRORNULL) {
        JSString *str = *(JSString **)slot;
        v = str ? StringValue(str) : NullValue();
    } else if (type == JSVAL_TYPE_OBJORNULL) {
        JSObject *obj = *(JSObject **)slot;
        v = obj ? ObjectValue(*obj) : NullValue();
    } else {
        JS_ASSERT(type == JSVAL_TYPE_BOXED);
        JS_STATIC_ASSERT(sizeof(Value) == sizeof(double));
        v = *(Value *)slot;
    }

#ifdef DEBUG
    switch (type) {
      case JSVAL_TYPE_NONFUNOBJ:
        JS_ASSERT(!IsFunctionObject(v));
        debug_only_printf(LC_TMTracer,
                          "object<%p:%s> ",
                          (void*) &v.toObject(),
                          v.toObject().getClass()->name);
        break;
      case JSVAL_TYPE_INT32:
        debug_only_printf(LC_TMTracer, "int<%d> ", v.toInt32());
        break;
      case JSVAL_TYPE_DOUBLE:
        debug_only_printf(LC_TMTracer, "double<%g> ", v.toNumber());
        break;
      case JSVAL_TYPE_STRING:
        debug_only_printf(LC_TMTracer, "string<%p> ", (void*)v.toString());
        break;
      case JSVAL_TYPE_NULL:
        JS_ASSERT(v.isNull());
        debug_only_print0(LC_TMTracer, "null ");
        break;
      case JSVAL_TYPE_BOOLEAN:
        debug_only_printf(LC_TMTracer, "bool<%d> ", v.toBoolean());
        break;
      case JSVAL_TYPE_UNDEFINED:
        JS_ASSERT(v.isUndefined());
        debug_only_print0(LC_TMTracer, "undefined ");
        break;
      case JSVAL_TYPE_MAGIC:
        debug_only_printf(LC_TMTracer, "magic<%d> ", v.whyMagic());
        break;
      case JSVAL_TYPE_FUNOBJ: {
        JS_ASSERT(IsFunctionObject(v));
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, &v.toObject());
        debug_only_printf(LC_TMTracer,
                          "function<%p:%s> ", (void*) &v.toObject(),
                          fun->atom
                          ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
                          : "unnamed");
        break;
      }
      case JSVAL_TYPE_STRORNULL:
        debug_only_printf(LC_TMTracer, "nullablestr<%p> ", v.isNull() ? NULL : (void *)v.toString());
        break;
      case JSVAL_TYPE_OBJORNULL:
        debug_only_printf(LC_TMTracer, "nullablestr<%p> ", v.isNull() ? NULL : (void *)&v.toObject());
        break;
      case JSVAL_TYPE_BOXED:
        debug_only_printf(LC_TMTracer, "box<%llx> ", (long long unsigned int)v.asRawBits());
        break;
      default:
        JS_NOT_REACHED("unexpected type");
        break;
    }
#endif
}

void
ExternNativeToValue(JSContext* cx, Value& v, JSValueType type, double* slot)
{
    return NativeToValue(cx, v, type, slot);
}

class BuildNativeFrameVisitor : public SlotVisitorBase
{
    JSContext *mCx;
    JSValueType *mTypeMap;
    double *mGlobal;
    double *mStack;
public:
    BuildNativeFrameVisitor(JSContext *cx,
                            JSValueType *typemap,
                            double *global,
                            double *stack) :
        mCx(cx),
        mTypeMap(typemap),
        mGlobal(global),
        mStack(stack)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
        debug_only_printf(LC_TMTracer, "global%d: ", n);
        ValueToNative(*vp, *mTypeMap++, &mGlobal[slot]);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, int count, JSStackFrame* fp) {
        for (int i = 0; i < count; ++i) {
            debug_only_printf(LC_TMTracer, "%s%d: ", stackSlotKind(), i);
            ValueToNative(*vp++, *mTypeMap++, mStack++);
        }
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        debug_only_printf(LC_TMTracer, "%s%d: ", stackSlotKind(), 0);
        if (p == fp->addressOfScopeChain())
            *(JSObject **)mStack = &fp->scopeChain();
        else
            *(JSObject **)mStack = fp->hasArgsObj() ? &fp->argsObj() : NULL;
#ifdef DEBUG
        if (*mTypeMap == JSVAL_TYPE_NULL) {
            JS_ASSERT(*(JSObject **)mStack == NULL);
            debug_only_print0(LC_TMTracer, "null ");
        } else {
            JS_ASSERT(*mTypeMap == JSVAL_TYPE_NONFUNOBJ);
            JS_ASSERT(!(*(JSObject **)p)->isFunction());
            debug_only_printf(LC_TMTracer,
                              "object<%p:%s> ", *(void **)p,
                              (*(JSObject **)p)->getClass()->name);
        }
#endif
        mTypeMap++;
        mStack++;
        return true;
    }
};

static JS_REQUIRES_STACK void
BuildNativeFrame(JSContext *cx, JSObject *globalObj, unsigned callDepth,
                 unsigned ngslots, uint16 *gslots,
                 JSValueType *typeMap, double *global, double *stack)
{
    BuildNativeFrameVisitor visitor(cx, typeMap, global, stack);
    VisitSlots(visitor, cx, globalObj, callDepth, ngslots, gslots);
    debug_only_print0(LC_TMTracer, "\n");
}

class FlushNativeGlobalFrameVisitor : public SlotVisitorBase
{
    JSContext *mCx;
    JSValueType *mTypeMap;
    double *mGlobal;
public:
    FlushNativeGlobalFrameVisitor(JSContext *cx,
                                  JSValueType *typeMap,
                                  double *global) :
        mCx(cx),
        mTypeMap(typeMap),
        mGlobal(global)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
        debug_only_printf(LC_TMTracer, "global%d=", n);
        JS_ASSERT(JS_THREAD_DATA(mCx)->waiveGCQuota);
        NativeToValue(mCx, *vp, *mTypeMap++, &mGlobal[slot]);
    }
};

class FlushNativeStackFrameVisitor : public SlotVisitorBase
{
    JSContext *mCx;
    const JSValueType *mInitTypeMap;
    const JSValueType *mTypeMap;
    double *mStack;
public:
    FlushNativeStackFrameVisitor(JSContext *cx,
                                 const JSValueType *typeMap,
                                 double *stack) :
        mCx(cx),
        mInitTypeMap(typeMap),
        mTypeMap(typeMap),
        mStack(stack)
    {}

    const JSValueType* getTypeMap()
    {
        return mTypeMap;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        JS_ASSERT(JS_THREAD_DATA(mCx)->waiveGCQuota);
        for (size_t i = 0; i < count; ++i) {
            debug_only_printf(LC_TMTracer, "%s%u=", stackSlotKind(), unsigned(i));
            NativeToValue(mCx, *vp, *mTypeMap, mStack);
            vp++;
            mTypeMap++;
            mStack++;
        }
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        JS_ASSERT(JS_THREAD_DATA(mCx)->waiveGCQuota);
        debug_only_printf(LC_TMTracer, "%s%u=", stackSlotKind(), 0);
        JSObject *frameobj = *(JSObject **)mStack;
        JS_ASSERT((frameobj == NULL) == (*mTypeMap == JSVAL_TYPE_NULL));
        if (p == fp->addressOfArgs()) {
            if (frameobj) {
                JS_ASSERT_IF(fp->hasArgsObj(), frameobj == &fp->argsObj());
                fp->setArgsObj(*frameobj);
                frameobj->setPrivate(fp);
                debug_only_printf(LC_TMTracer,
                                  "argsobj<%p> ",
                                  (void *)frameobj);
            } else {
                JS_ASSERT(!fp->hasArgsObj());
                debug_only_print0(LC_TMTracer,
                                  "argsobj<null> ");
            }
            /* else, SynthesizeFrame has initialized fp->args.nactual */
        } else {
            JS_ASSERT(p == fp->addressOfScopeChain());
            if (frameobj->isCall() &&
                !frameobj->getPrivate() &&
                &fp->callee() == &frameobj->getCallObjCallee())
            {
                JS_ASSERT(&fp->scopeChain() == JSStackFrame::sInvalidScopeChain);
                frameobj->setPrivate(fp);
                fp->setScopeChainAndCallObj(*frameobj);
            } else {
                fp->setScopeChainNoCallObj(*frameobj);
            }
            debug_only_printf(LC_TMTracer,
                              "scopechain<%p> ",
                              (void *)frameobj);
        }
#ifdef DEBUG
        JSValueType type = *mTypeMap;
        if (type == JSVAL_TYPE_NULL) {
            debug_only_print0(LC_TMTracer, "null ");
        } else {
            JS_ASSERT(type == JSVAL_TYPE_NONFUNOBJ);
            JS_ASSERT(!frameobj->isFunction());
            debug_only_printf(LC_TMTracer,
                              "object<%p:%s> ",
                              *(void **)p,
                              frameobj->getClass()->name);
        }
#endif
        mTypeMap++;
        mStack++;
        return true;
    }
};

/* Box the given native frame into a JS frame. This is infallible. */
static JS_REQUIRES_STACK void
FlushNativeGlobalFrame(JSContext *cx, JSObject *globalObj, double *global, unsigned ngslots,
                       uint16 *gslots, JSValueType *typemap)
{
    FlushNativeGlobalFrameVisitor visitor(cx, typemap, global);
    VisitGlobalSlots(visitor, cx, globalObj, ngslots, gslots);
    debug_only_print0(LC_TMTracer, "\n");
}

/*
 * Returns the number of values on the native stack, excluding the innermost
 * frame. This walks all FrameInfos on the native frame stack and sums the
 * slot usage of each frame.
 */
static int32
StackDepthFromCallStack(TracerState* state, uint32 callDepth)
{
    int32 nativeStackFramePos = 0;

    // Duplicate native stack layout computation: see VisitFrameSlots header comment.
    for (FrameInfo** fip = state->callstackBase; fip < state->rp + callDepth; fip++)
        nativeStackFramePos += (*fip)->callerHeight;
    return nativeStackFramePos;
}

/*
 * Generic function to read upvars on trace from slots of active frames.
 *     T   Traits type parameter. Must provide static functions:
 *             interp_get(fp, slot)     Read the value out of an interpreter frame.
 *             native_slot(argc, slot)  Return the position of the desired value in the on-trace
 *                                      stack frame (with position 0 being callee).
 *
 *     upvarLevel  Static level of the function containing the upvar definition
 *     slot        Identifies the value to get. The meaning is defined by the traits type.
 *     callDepth   Call depth of current point relative to trace entry
 */
template<typename T>
inline JSValueType
GetUpvarOnTrace(JSContext* cx, uint32 upvarLevel, int32 slot, uint32 callDepth, double* result)
{
    TracerState* state = cx->tracerState;
    FrameInfo** fip = state->rp + callDepth;

    /*
     * First search the FrameInfo call stack for an entry containing our
     * upvar, namely one with level == upvarLevel. The first FrameInfo is a
     * transition from the entry frame to some callee. However, it is not
     * known (from looking at the FrameInfo) whether the entry frame had a
     * callee. Rather than special-case this or insert more logic into the
     * loop, instead just stop before that FrameInfo (i.e. |> base| instead of
     * |>= base|), and let the code after the loop handle it.
     */
    int32 stackOffset = StackDepthFromCallStack(state, callDepth);
    while (--fip > state->callstackBase) {
        FrameInfo* fi = *fip;

        /*
         * The loop starts aligned to the top of the stack, so move down to the first meaningful
         * callee. Then read the callee directly from the frame.
         */
        stackOffset -= fi->callerHeight;
        JSObject* callee = *(JSObject**)(&state->stackBase[stackOffset]);
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, callee);
        uintN calleeLevel = fun->u.i.script->staticLevel;
        if (calleeLevel == upvarLevel) {
            /*
             * Now find the upvar's value in the native stack. stackOffset is
             * the offset of the start of the activation record corresponding
             * to *fip in the native stack.
             */
            uint32 native_slot = T::native_slot(fi->callerArgc, slot);
            *result = state->stackBase[stackOffset + native_slot];
            return fi->get_typemap()[native_slot];
        }
    }

    // Next search the trace entry frame, which is not in the FrameInfo stack.
    if (state->outermostTree->script->staticLevel == upvarLevel) {
        uint32 argc = state->outermostTree->argc;
        uint32 native_slot = T::native_slot(argc, slot);
        *result = state->stackBase[native_slot];
        return state->callstackBase[0]->get_typemap()[native_slot];
    }

    /*
     * If we did not find the upvar in the frames for the active traces,
     * then we simply get the value from the interpreter state.
     */
    JS_ASSERT(upvarLevel < UpvarCookie::UPVAR_LEVEL_LIMIT);
    JSStackFrame* fp = cx->findFrameAtLevel(upvarLevel);
    Value v = T::interp_get(fp, slot);
    JSValueType type = getCoercedType(v);
    ValueToNative(v, type, result);
    return type;
}

// For this traits type, 'slot' is the argument index, which may be -2 for callee.
struct UpvarArgTraits {
    static Value interp_get(JSStackFrame* fp, int32 slot) {
        return fp->formalArg(slot);
    }

    static uint32 native_slot(uint32 argc, int32 slot) {
        return 2 /*callee,this*/ + slot;
    }
};

uint32 JS_FASTCALL
GetUpvarArgOnTrace(JSContext* cx, uint32 upvarLevel, int32 slot, uint32 callDepth, double* result)
{
    return GetUpvarOnTrace<UpvarArgTraits>(cx, upvarLevel, slot, callDepth, result);
}

// For this traits type, 'slot' is an index into the local slots array.
struct UpvarVarTraits {
    static Value interp_get(JSStackFrame* fp, int32 slot) {
        return fp->slots()[slot];
    }

    static uint32 native_slot(uint32 argc, int32 slot) {
        return 4 /*callee,this,arguments,scopeChain*/ + argc + slot;
    }
};

uint32 JS_FASTCALL
GetUpvarVarOnTrace(JSContext* cx, uint32 upvarLevel, int32 slot, uint32 callDepth, double* result)
{
    return GetUpvarOnTrace<UpvarVarTraits>(cx, upvarLevel, slot, callDepth, result);
}

/*
 * For this traits type, 'slot' is an index into the stack area (within slots,
 * after nfixed) of a frame with no function. (On trace, the top-level frame is
 * the only one that can have no function.)
 */
struct UpvarStackTraits {
    static Value interp_get(JSStackFrame* fp, int32 slot) {
        return fp->slots()[slot + fp->numFixed()];
    }

    static uint32 native_slot(uint32 argc, int32 slot) {
        /*
         * Locals are not imported by the tracer when the frame has no
         * function, so we do not add fp->getFixedCount().
         */
        JS_ASSERT(argc == 0);
        return slot;
    }
};

uint32 JS_FASTCALL
GetUpvarStackOnTrace(JSContext* cx, uint32 upvarLevel, int32 slot, uint32 callDepth,
                     double* result)
{
    return GetUpvarOnTrace<UpvarStackTraits>(cx, upvarLevel, slot, callDepth, result);
}

// Parameters needed to access a value from a closure on trace.
struct ClosureVarInfo
{
    uint32   slot;
#ifdef DEBUG
    uint32   callDepth;
#endif
};

/*
 * Generic function to read upvars from Call objects of active heavyweight functions.
 *     call       Callee Function object in which the upvar is accessed.
 */
template<typename T>
inline uint32
GetFromClosure(JSContext* cx, JSObject* call, const ClosureVarInfo* cv, double* result)
{
    JS_ASSERT(call->isCall());

#ifdef DEBUG
    TracerState* state = cx->tracerState;
    FrameInfo** fip = state->rp + cv->callDepth;
    int32 stackOffset = StackDepthFromCallStack(state, cv->callDepth);
    while (--fip > state->callstackBase) {
        FrameInfo* fi = *fip;

        /*
         * The loop starts aligned to the top of the stack, so move down to the first meaningful
         * callee. Then read the callee directly from the frame.
         */
        stackOffset -= fi->callerHeight;
        JSObject* callee = *(JSObject**)(&state->stackBase[stackOffset]);
        if (callee == call) {
            // This is not reachable as long as the tracer guards on the identity of the callee's
            // parent when making a call:
            //
            // - We can only reach this point if we execute JSOP_LAMBDA on trace, then call the
            //   function created by the lambda, and then execute a JSOP_NAME on trace.
            // - Each time we execute JSOP_LAMBDA we get a function with a different parent.
            // - When we execute the call to the new function, we exit trace because the parent
            //   is different.
            JS_NOT_REACHED("JSOP_NAME variable found in outer trace");
        }
    }
#endif

    // We already guarded on trace that we aren't touching an outer tree's entry frame
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JSStackFrame* fp = (JSStackFrame*) call->getPrivate();
    JS_ASSERT(fp != cx->fp());

    Value v;
    if (fp) {
        v = T::get_slot(fp, cv->slot);
    } else {
        /*
         * Get the value from the object. We know we have a Call object, and
         * that our slot index is fine, so don't monkey around with calling the
         * property getter (which just looks in the slot) or calling
         * js_GetReservedSlot. Just get the slot directly. Note the static
         * asserts in jsfun.cpp which make sure Call objects use slots.
         */
        JS_ASSERT(cv->slot < T::slot_count(call));
        v = T::get_slot(call, cv->slot);
    }
    JSValueType type = getCoercedType(v);
    ValueToNative(v, type, result);
    return type;
}

struct ArgClosureTraits
{
    // Get the right frame slots to use our slot index with.
    // See also UpvarArgTraits.
    static inline Value get_slot(JSStackFrame* fp, unsigned slot) {
        JS_ASSERT(slot < fp->numFormalArgs());
        return fp->formalArg(slot);
    }

    // Get the right object slots to use our slot index with.
    static inline Value get_slot(JSObject* obj, unsigned slot) {
        return obj->getSlot(slot_offset(obj) + slot);
    }

    // Get the offset of our object slots from the object's slots pointer.
    static inline uint32 slot_offset(JSObject* obj) {
        return JSObject::CALL_RESERVED_SLOTS;
    }

    // Get the maximum slot index of this type that should be allowed
    static inline uint16 slot_count(JSObject* obj) {
        return obj->getCallObjCalleeFunction()->nargs;
    }

private:
    ArgClosureTraits();
};

uint32 JS_FASTCALL
GetClosureArg(JSContext* cx, JSObject* callee, const ClosureVarInfo* cv, double* result)
{
    return GetFromClosure<ArgClosureTraits>(cx, callee, cv, result);
}

struct VarClosureTraits
{
    // See also UpvarVarTraits.
    static inline Value get_slot(JSStackFrame* fp, unsigned slot) {
        JS_ASSERT(slot < fp->fun()->u.i.nvars);
        return fp->slots()[slot];
    }

    static inline Value get_slot(JSObject* obj, unsigned slot) {
        return obj->getSlot(slot_offset(obj) + slot);
    }

    static inline uint32 slot_offset(JSObject* obj) {
        return JSObject::CALL_RESERVED_SLOTS +
               obj->getCallObjCalleeFunction()->nargs;
    }

    static inline uint16 slot_count(JSObject* obj) {
        return obj->getCallObjCalleeFunction()->u.i.nvars;
    }

private:
    VarClosureTraits();
};

uint32 JS_FASTCALL
GetClosureVar(JSContext* cx, JSObject* callee, const ClosureVarInfo* cv, double* result)
{
    return GetFromClosure<VarClosureTraits>(cx, callee, cv, result);
}

/**
 * Box the given native stack frame into the virtual machine stack. This
 * is infallible.
 *
 * @param callDepth the distance between the entry frame into our trace and
 *                  cx->fp() when we make this call.  If this is not called as a
 *                  result of a nested exit, callDepth is 0.
 * @param mp an array of JSValueType that indicate what the types of the things
 *           on the stack are.
 * @param np pointer to the native stack.  We want to copy values from here to
 *           the JS stack as needed.
 * @return the number of things we popped off of np.
 */
static JS_REQUIRES_STACK int
FlushNativeStackFrame(JSContext* cx, unsigned callDepth, const JSValueType* mp, double* np)
{
    /* Root all string and object references first (we don't need to call the GC for this). */
    FlushNativeStackFrameVisitor visitor(cx, mp, np);
    VisitStackSlots(visitor, cx, callDepth);

    debug_only_print0(LC_TMTracer, "\n");
    return visitor.getTypeMap() - mp;
}

/* Emit load instructions onto the trace that read the initial stack state. */
JS_REQUIRES_STACK void
TraceRecorder::importImpl(Address addr, const void* p, JSValueType t,
                          const char *prefix, uintN index, JSStackFrame *fp)
{
    LIns* ins;
    if (t == JSVAL_TYPE_INT32) { /* demoted */
        JS_ASSERT(hasInt32Repr(*(const Value *)p));

        /*
         * Ok, we have a valid demotion attempt pending, so insert an integer
         * read and promote it to double since all arithmetic operations expect
         * to see doubles on entry. The first op to use this slot will emit a
         * d2i cast which will cancel out the i2d we insert here.
         */
        ins = w.ldi(addr);
        ins = w.i2d(ins);
    } else {
        JS_ASSERT_IF(t != JSVAL_TYPE_BOXED && !isFrameObjPtrTraceType(t),
                     ((const Value *)p)->isNumber() == (t == JSVAL_TYPE_DOUBLE));
        if (t == JSVAL_TYPE_DOUBLE) {
            ins = w.ldd(addr);
        } else if (t == JSVAL_TYPE_BOOLEAN) {
            ins = w.ldi(addr);
        } else if (t == JSVAL_TYPE_UNDEFINED) {
            ins = w.immiUndefined();
        } else if (t == JSVAL_TYPE_MAGIC) {
            ins = w.ldi(addr);
        } else {
            ins = w.ldp(addr);
        }
    }
    checkForGlobalObjectReallocation();
    tracker.set(p, ins);

#ifdef DEBUG
    char name[64];
    JS_ASSERT(strlen(prefix) < 11);
    void* mark = NULL;
    jsuword* localNames = NULL;
    const char* funName = NULL;
    if (*prefix == 'a' || *prefix == 'v') {
        mark = JS_ARENA_MARK(&cx->tempPool);
        if (fp->fun()->hasLocalNames())
            localNames = fp->fun()->getLocalNameArray(cx, &cx->tempPool);
        funName = fp->fun()->atom
                ? js_AtomToPrintableString(cx, fp->fun()->atom)
                : "<anonymous>";
    }
    if (!strcmp(prefix, "argv")) {
        if (index < fp->numFormalArgs()) {
            JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(localNames[index]);
            JS_snprintf(name, sizeof name, "$%s.%s", funName, js_AtomToPrintableString(cx, atom));
        } else {
            JS_snprintf(name, sizeof name, "$%s.<arg%d>", funName, index);
        }
    } else if (!strcmp(prefix, "vars")) {
        JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(localNames[fp->numFormalArgs() + index]);
        JS_snprintf(name, sizeof name, "$%s.%s", funName, js_AtomToPrintableString(cx, atom));
    } else {
        JS_snprintf(name, sizeof name, "$%s%d", prefix, index);
    }

    if (mark)
        JS_ARENA_RELEASE(&cx->tempPool, mark);
    w.name(ins, name);

    debug_only_printf(LC_TMTracer, "import vp=%p name=%s type=%c\n",
                      p, name, TypeToChar(t));
#endif
}

JS_REQUIRES_STACK void
TraceRecorder::import(Address addr, const Value* p, JSValueType t,
                      const char *prefix, uintN index, JSStackFrame *fp)
{
    return importImpl(addr, p, t, prefix, index, fp);
}

class ImportBoxedStackSlotVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    LIns *mBase;
    ptrdiff_t mStackOffset;
    JSValueType *mTypemap;
    JSStackFrame *mFp;
public:
    ImportBoxedStackSlotVisitor(TraceRecorder &recorder,
                                LIns *base,
                                ptrdiff_t stackOffset,
                                JSValueType *typemap) :
        mRecorder(recorder),
        mBase(base),
        mStackOffset(stackOffset),
        mTypemap(typemap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            if (*mTypemap == JSVAL_TYPE_BOXED) {
                mRecorder.import(StackAddress(mBase, mStackOffset), vp, JSVAL_TYPE_BOXED,
                                 "jsval", i, fp);
                LIns *vp_ins = mRecorder.unbox_value(*vp,
                                                     StackAddress(mBase, mStackOffset),
                                                     mRecorder.copy(mRecorder.anchor));
                mRecorder.set(vp, vp_ins);
            }
            vp++;
            mTypemap++;
            mStackOffset += sizeof(double);
        }
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame *fp) {
        JS_ASSERT(*mTypemap != JSVAL_TYPE_BOXED);
        mTypemap++;
        mStackOffset += sizeof(double);
        return true;
    }
};

JS_REQUIRES_STACK void
TraceRecorder::import(TreeFragment* tree, LIns* sp, unsigned stackSlots, unsigned ngslots,
                      unsigned callDepth, JSValueType* typeMap)
{
    /*
     * If we get a partial list that doesn't have all the types (i.e. recording
     * from a side exit that was recorded but we added more global slots
     * later), merge the missing types from the entry type map. This is safe
     * because at the loop edge we verify that we have compatible types for all
     * globals (entry type and loop edge type match). While a different trace
     * of the tree might have had a guard with a different type map for these
     * slots we just filled in here (the guard we continue from didn't know
     * about them), since we didn't take that particular guard the only way we
     * could have ended up here is if that other trace had at its end a
     * compatible type distribution with the entry map. Since that's exactly
     * what we used to fill in the types our current side exit didn't provide,
     * this is always safe to do.
     */

    JSValueType* globalTypeMap = typeMap + stackSlots;
    unsigned length = tree->nGlobalTypes();

    /*
     * This is potentially the typemap of the side exit and thus shorter than
     * the tree's global type map.
     */
    if (ngslots < length) {
        MergeTypeMaps(&globalTypeMap /* out param */, &ngslots /* out param */,
                      tree->globalTypeMap(), length,
                      (JSValueType*)alloca(sizeof(JSValueType) * length));
    }
    JS_ASSERT(ngslots == tree->nGlobalTypes());

    /*
     * Check whether there are any values on the stack we have to unbox and do
     * that first before we waste any time fetching the state from the stack.
     */
    ImportBoxedStackSlotVisitor boxedStackVisitor(*this, sp, -tree->nativeStackBase, typeMap);
    VisitStackSlots(boxedStackVisitor, cx, callDepth);

    /*
     * Remember the import type map so we can lazily import later whatever
     * we need.
     */
    importTypeMap.set(importStackSlots = stackSlots,
                      importGlobalSlots = ngslots,
                      typeMap, globalTypeMap);
}

JS_REQUIRES_STACK bool
TraceRecorder::isValidSlot(JSObject *obj, const Shape* shape)
{
    uint32 setflags = (js_CodeSpec[*cx->regs->pc].format & (JOF_SET | JOF_INCDEC | JOF_FOR));

    if (setflags) {
        if (!shape->hasDefaultSetter())
            RETURN_VALUE("non-stub setter", false);
        if (!shape->writable())
            RETURN_VALUE("writing to a read-only property", false);
    }

    /* This check applies even when setflags == 0. */
    if (setflags != JOF_SET && !shape->hasDefaultGetter()) {
        JS_ASSERT(!shape->isMethod());
        RETURN_VALUE("non-stub getter", false);
    }

    if (!obj->containsSlot(shape->slot))
        RETURN_VALUE("invalid-slot obj property", false);

    return true;
}

/* Lazily import a global slot if we don't already have it in the tracker. */
JS_REQUIRES_STACK void
TraceRecorder::importGlobalSlot(unsigned slot)
{
    JS_ASSERT(slot == uint16(slot));
    JS_ASSERT(globalObj->numSlots() <= MAX_GLOBAL_SLOTS);

    Value* vp = &globalObj->getSlotRef(slot);
    JS_ASSERT(!known(vp));

    /* Add the slot to the list of interned global slots. */
    JSValueType type;
    int index = tree->globalSlots->offsetOf(uint16(slot));
    if (index == -1) {
        type = getCoercedType(*vp);
        if (type == JSVAL_TYPE_INT32 && (!oracle || oracle->isGlobalSlotUndemotable(cx, slot)))
            type = JSVAL_TYPE_DOUBLE;
        index = (int)tree->globalSlots->length();
        tree->globalSlots->add(uint16(slot));
        tree->typeMap.add(type);
        SpecializeTreesToMissingGlobals(cx, globalObj, tree);
        JS_ASSERT(tree->nGlobalTypes() == tree->globalSlots->length());
    } else {
        type = importTypeMap[importStackSlots + index];
        JS_ASSERT(type != JSVAL_TYPE_UNINITIALIZED);
    }
    import(EosAddress(eos_ins, slot * sizeof(double)), vp, type, "global", index, NULL);
}

/* Lazily import a global slot if we don't already have it in the tracker. */
JS_REQUIRES_STACK bool
TraceRecorder::lazilyImportGlobalSlot(unsigned slot)
{
    if (slot != uint16(slot)) /* we use a table of 16-bit ints, bail out if that's not enough */
        return false;
    /*
     * If the global object grows too large, alloca in ExecuteTree might fail,
     * so abort tracing on global objects with unreasonably many slots.
     */
    if (globalObj->numSlots() > MAX_GLOBAL_SLOTS)
        return false;
    Value* vp = &globalObj->getSlotRef(slot);
    if (known(vp))
        return true; /* we already have it */
    importGlobalSlot(slot);
    return true;
}

/* Write back a value onto the stack or global frames. */
LIns*
TraceRecorder::writeBack(LIns* ins, LIns* base, ptrdiff_t offset, bool shouldDemote)
{
    /*
     * Sink all type casts targeting the stack into the side exit by simply storing the original
     * (uncasted) value. Each guard generates the side exit map based on the types of the
     * last stores to every stack location, so it's safe to not perform them on-trace.
     */
    JS_ASSERT(base == lirbuf->sp || base == eos_ins);
    if (shouldDemote && IsPromoteInt(ins))
        ins = w.demote(ins);
    Address addr = (base == lirbuf->sp)
                 ? (Address)StackAddress(base, offset)
                 : (Address)EosAddress(base, offset);
    return w.st(ins, addr);
}

/* Update the tracker, then issue a write back store. */
JS_REQUIRES_STACK void
TraceRecorder::setImpl(void* p, LIns* i, bool demote)
{
    JS_ASSERT(i != NULL);
    checkForGlobalObjectReallocation();
    tracker.set(p, i);

    /*
     * If we are writing to this location for the first time, calculate the
     * offset into the native frame manually. Otherwise just look up the last
     * load or store associated with the same source address (p) and use the
     * same offset/base.
     */
    LIns* x = nativeFrameTracker.get(p);
    if (!x) {
        if (isVoidPtrGlobal(p))
            x = writeBack(i, eos_ins, nativeGlobalOffset((Value *)p), demote);
        else
            x = writeBack(i, lirbuf->sp, nativespOffsetImpl(p), demote);
        nativeFrameTracker.set(p, x);
    } else {
#if defined NANOJIT_64BIT
        JS_ASSERT( x->isop(LIR_stq) || x->isop(LIR_sti) || x->isop(LIR_std));
#else
        JS_ASSERT( x->isop(LIR_sti) || x->isop(LIR_std));
#endif

        ptrdiff_t disp;
        LIns *base = x->oprnd2();
        if (base->isop(LIR_addp) && base->oprnd2()->isImmP()) {
            disp = ptrdiff_t(base->oprnd2()->immP());
            base = base->oprnd1();
        } else {
            disp = x->disp();
        }

        JS_ASSERT(base == lirbuf->sp || base == eos_ins);
        JS_ASSERT(disp == ((base == lirbuf->sp)
                            ? nativespOffsetImpl(p)
                            : nativeGlobalOffset((Value *)p)));

        writeBack(i, base, disp, demote);
    }
}

JS_REQUIRES_STACK inline void
TraceRecorder::set(Value* p, LIns* i, bool demote)
{
    return setImpl(p, i, demote);
}

JS_REQUIRES_STACK void
TraceRecorder::setFrameObjPtr(void* p, LIns* i, bool demote)
{
    JS_ASSERT(isValidFrameObjPtr(p));
    return setImpl(p, i, demote);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::attemptImport(const Value* p)
{
    if (LIns* i = getFromTracker(p))
        return i;

    /* If the variable was not known, it could require a lazy import. */
    CountSlotsVisitor countVisitor(p);
    VisitStackSlots(countVisitor, cx, callDepth);

    if (countVisitor.stopped() || size_t(p - cx->fp()->slots()) < cx->fp()->numSlots())
        return get(p);

    return NULL;
}

inline nanojit::LIns*
TraceRecorder::getFromTrackerImpl(const void* p)
{
    checkForGlobalObjectReallocation();
    return tracker.get(p);
}

inline nanojit::LIns*
TraceRecorder::getFromTracker(const Value* p)
{
    return getFromTrackerImpl(p);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::getImpl(const void *p)
{
    LIns* x = getFromTrackerImpl(p);
    if (x)
        return x;
    if (isVoidPtrGlobal(p)) {
        unsigned slot = nativeGlobalSlot((const Value *)p);
        JS_ASSERT(tree->globalSlots->offsetOf(uint16(slot)) != -1);
        importGlobalSlot(slot);
    } else {
        unsigned slot = nativeStackSlotImpl(p);
        JSValueType type = importTypeMap[slot];
        JS_ASSERT(type != JSVAL_TYPE_UNINITIALIZED);
        importImpl(StackAddress(lirbuf->sp, -tree->nativeStackBase + slot * sizeof(jsdouble)),
                   p, type, "stack", slot, cx->fp());
    }
    JS_ASSERT(knownImpl(p));
    return tracker.get(p);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::get(const Value *p)
{
    return getImpl(p);
}

#ifdef DEBUG
bool
TraceRecorder::isValidFrameObjPtr(void *p)
{
    JSStackFrame *fp = cx->fp();
    for (; fp; fp = fp->prev()) {
        if (fp->addressOfScopeChain() == p || fp->addressOfArgs() == p)
            return true;
    }
    return false;
}
#endif

JS_REQUIRES_STACK LIns*
TraceRecorder::getFrameObjPtr(void *p)
{
    JS_ASSERT(isValidFrameObjPtr(p));
    return getImpl(p);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::addr(Value* p)
{
    return isGlobal(p)
           ? w.addp(eos_ins, w.nameImmw(nativeGlobalOffset(p)))
           : w.addp(lirbuf->sp, w.nameImmw(nativespOffset(p)));
}

JS_REQUIRES_STACK inline bool
TraceRecorder::knownImpl(const void* p)
{
    checkForGlobalObjectReallocation();
    return tracker.has(p);
}

JS_REQUIRES_STACK inline bool
TraceRecorder::known(const Value* vp)
{
    return knownImpl(vp);
}

JS_REQUIRES_STACK inline bool
TraceRecorder::known(JSObject** p)
{
    return knownImpl(p);
}

/*
 * The slots of the global object are sometimes reallocated by the interpreter.
 * This function check for that condition and re-maps the entries of the tracker
 * accordingly.
 */
JS_REQUIRES_STACK void
TraceRecorder::checkForGlobalObjectReallocationHelper()
{
    debug_only_print0(LC_TMTracer, "globalObj->slots relocated, updating tracker\n");
    Value* src = global_slots;
    Value* dst = globalObj->getSlots();
    jsuint length = globalObj->capacity;
    LIns** map = (LIns**)alloca(sizeof(LIns*) * length);
    for (jsuint n = 0; n < length; ++n) {
        map[n] = tracker.get(src);
        tracker.set(src++, NULL);
    }
    for (jsuint n = 0; n < length; ++n)
        tracker.set(dst++, map[n]);
    global_slots = globalObj->getSlots();
}

/* Determine whether the current branch is a loop edge (taken or not taken). */
static JS_REQUIRES_STACK bool
IsLoopEdge(jsbytecode* pc, jsbytecode* header)
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

class AdjustCallerGlobalTypesVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSContext *mCx;
    nanojit::LirBuffer *mLirbuf;
    JSValueType *mTypeMap;
public:
    AdjustCallerGlobalTypesVisitor(TraceRecorder &recorder,
                                   JSValueType *typeMap) :
        mRecorder(recorder),
        mCx(mRecorder.cx),
        mLirbuf(mRecorder.lirbuf),
        mTypeMap(typeMap)
    {}

    JSValueType* getTypeMap()
    {
        return mTypeMap;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
        LIns *ins = mRecorder.get(vp);
        bool isPromote = IsPromoteInt(ins);
        if (isPromote && *mTypeMap == JSVAL_TYPE_DOUBLE) {
            mRecorder.w.st(mRecorder.get(vp),
                           EosAddress(mRecorder.eos_ins, mRecorder.nativeGlobalOffset(vp)));
            /*
             * Aggressively undo speculation so the inner tree will compile
             * if this fails.
             */
            JS_TRACE_MONITOR(mCx).oracle->markGlobalSlotUndemotable(mCx, slot);
        }
        JS_ASSERT(!(!isPromote && *mTypeMap == JSVAL_TYPE_INT32));
        ++mTypeMap;
    }
};

class AdjustCallerStackTypesVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSContext *mCx;
    nanojit::LirBuffer *mLirbuf;
    unsigned mSlotnum;
    JSValueType *mTypeMap;
public:
    AdjustCallerStackTypesVisitor(TraceRecorder &recorder,
                                  JSValueType *typeMap) :
        mRecorder(recorder),
        mCx(mRecorder.cx),
        mLirbuf(mRecorder.lirbuf),
        mSlotnum(0),
        mTypeMap(typeMap)
    {}

    JSValueType* getTypeMap()
    {
        return mTypeMap;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        /* N.B. vp may actually point to a JSObject*. */
        for (size_t i = 0; i < count; ++i) {
            LIns *ins = mRecorder.get(vp);
            bool isPromote = IsPromoteInt(ins);
            if (isPromote && *mTypeMap == JSVAL_TYPE_DOUBLE) {
                mRecorder.w.st(ins, StackAddress(mLirbuf->sp, mRecorder.nativespOffset(vp)));
                /*
                 * Aggressively undo speculation so the inner tree will compile
                 * if this fails.
                 */
                JS_TRACE_MONITOR(mCx).oracle->markStackSlotUndemotable(mCx, mSlotnum);
            }
            JS_ASSERT(!(!isPromote && *mTypeMap == JSVAL_TYPE_INT32));
            ++vp;
            ++mTypeMap;
            ++mSlotnum;
        }
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        JS_ASSERT(*mTypeMap != JSVAL_TYPE_BOXED);
        ++mTypeMap;
        ++mSlotnum;
        return true;
    }
};

/*
 * Promote slots if necessary to match the called tree's type map. This
 * function is infallible and must only be called if we are certain that it is
 * possible to reconcile the types for each slot in the inner and outer trees.
 */
JS_REQUIRES_STACK void
TraceRecorder::adjustCallerTypes(TreeFragment* f)
{
    AdjustCallerGlobalTypesVisitor globalVisitor(*this, f->globalTypeMap());
    VisitGlobalSlots(globalVisitor, cx, *tree->globalSlots);

    AdjustCallerStackTypesVisitor stackVisitor(*this, f->stackTypeMap());
    VisitStackSlots(stackVisitor, cx, 0);

    JS_ASSERT(f == f->root);
}

JS_REQUIRES_STACK inline JSValueType
TraceRecorder::determineSlotType(Value* vp)
{
    if (vp->isNumber()) {
        LIns *i = getFromTracker(vp);
        JSValueType t;
        if (i) {
            t = IsPromoteInt(i) ? JSVAL_TYPE_INT32 : JSVAL_TYPE_DOUBLE;
        } else if (isGlobal(vp)) {
            int offset = tree->globalSlots->offsetOf(uint16(nativeGlobalSlot(vp)));
            JS_ASSERT(offset != -1);
            t = importTypeMap[importStackSlots + offset];
        } else {
            t = importTypeMap[nativeStackSlot(vp)];
        }
        JS_ASSERT(t != JSVAL_TYPE_UNINITIALIZED);
        JS_ASSERT_IF(t == JSVAL_TYPE_INT32, hasInt32Repr(*vp));
        return t;
    }

    if (vp->isObject())
        return vp->toObject().isFunction() ? JSVAL_TYPE_FUNOBJ : JSVAL_TYPE_NONFUNOBJ;
    return vp->extractNonDoubleObjectTraceType();
}

class DetermineTypesVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSValueType *mTypeMap;
public:
    DetermineTypesVisitor(TraceRecorder &recorder,
                          JSValueType *typeMap) :
        mRecorder(recorder),
        mTypeMap(typeMap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
        *mTypeMap++ = mRecorder.determineSlotType(vp);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i)
            *mTypeMap++ = mRecorder.determineSlotType(vp++);
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        *mTypeMap++ = getFrameObjPtrTraceType(p, fp);
        return true;
    }

    JSValueType* getTypeMap()
    {
        return mTypeMap;
    }
};

#if defined JS_JIT_SPEW
JS_REQUIRES_STACK static void
TreevisLogExit(JSContext* cx, VMSideExit* exit)
{
    debug_only_printf(LC_TMTreeVis, "TREEVIS ADDEXIT EXIT=%p TYPE=%s FRAG=%p PC=%p FILE=\"%s\""
                      " LINE=%d OFFS=%d", (void*)exit, getExitName(exit->exitType),
                      (void*)exit->from, (void*)cx->regs->pc, cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()), FramePCOffset(cx, cx->fp()));
    debug_only_print0(LC_TMTreeVis, " STACK=\"");
    for (unsigned i = 0; i < exit->numStackSlots; i++)
        debug_only_printf(LC_TMTreeVis, "%c", TypeToChar(exit->stackTypeMap()[i]));
    debug_only_print0(LC_TMTreeVis, "\" GLOBALS=\"");
    for (unsigned i = 0; i < exit->numGlobalSlots; i++)
        debug_only_printf(LC_TMTreeVis, "%c", TypeToChar(exit->globalTypeMap()[i]));
    debug_only_print0(LC_TMTreeVis, "\"\n");
}
#endif

JS_REQUIRES_STACK VMSideExit*
TraceRecorder::snapshot(ExitType exitType)
{
    JSStackFrame* const fp = cx->fp();
    JSFrameRegs* const regs = cx->regs;
    jsbytecode* pc = regs->pc;

    /*
     * Check for a return-value opcode that needs to restart at the next
     * instruction.
     */
    const JSCodeSpec& cs = js_CodeSpec[*pc];

    /*
     * When calling a _FAIL native, make the snapshot's pc point to the next
     * instruction after the CALL or APPLY. Even on failure, a _FAIL native
     * must not be called again from the interpreter.
     */
    bool resumeAfter = (pendingSpecializedNative &&
                        JSTN_ERRTYPE(pendingSpecializedNative) == FAIL_STATUS);
    if (resumeAfter) {
        JS_ASSERT(*pc == JSOP_CALL || *pc == JSOP_APPLY || *pc == JSOP_NEW ||
                  *pc == JSOP_SETPROP || *pc == JSOP_SETNAME);
        pc += cs.length;
        regs->pc = pc;
        MUST_FLOW_THROUGH("restore_pc");
    }

    /*
     * Generate the entry map for the (possibly advanced) pc and stash it in
     * the trace.
     */
    unsigned stackSlots = NativeStackSlots(cx, callDepth);

    /*
     * It's sufficient to track the native stack use here since all stores
     * above the stack watermark defined by guards are killed.
     */
    trackNativeStackUse(stackSlots + 1);

    /* Capture the type map into a temporary location. */
    unsigned ngslots = tree->globalSlots->length();
    unsigned typemap_size = (stackSlots + ngslots) * sizeof(JSValueType);

    /* Use the recorder-local temporary type map. */
    JSValueType* typemap = NULL;
    if (tempTypeMap.resize(typemap_size))
        typemap = tempTypeMap.begin(); /* crash if resize() fails. */

    /*
     * Determine the type of a store by looking at the current type of the
     * actual value the interpreter is using. For numbers we have to check what
     * kind of store we used last (integer or double) to figure out what the
     * side exit show reflect in its typemap.
     */
    DetermineTypesVisitor detVisitor(*this, typemap);
    VisitSlots(detVisitor, cx, callDepth, ngslots,
               tree->globalSlots->data());
    JS_ASSERT(unsigned(detVisitor.getTypeMap() - typemap) ==
              ngslots + stackSlots);

    /*
     * If this snapshot is for a side exit that leaves a boxed Value result on
     * the stack, make a note of this in the typemap. Examples include the
     * builtinStatus guard after calling a _FAIL builtin, a JSFastNative, or
     * GetPropertyByName; and the type guard in unbox_value after such a call
     * (also at the beginning of a trace branched from such a type guard).
     */
    if (pendingUnboxSlot ||
        (pendingSpecializedNative && (pendingSpecializedNative->flags & JSTN_UNBOX_AFTER))) {
        unsigned pos = stackSlots - 1;
        if (pendingUnboxSlot == cx->regs->sp - 2)
            pos = stackSlots - 2;
        typemap[pos] = JSVAL_TYPE_BOXED;
    } else if (pendingSpecializedNative &&
               (pendingSpecializedNative->flags & JSTN_RETURN_NULLABLE_STR)) {
        typemap[stackSlots - 1] = JSVAL_TYPE_STRORNULL;
    } else if (pendingSpecializedNative &&
               (pendingSpecializedNative->flags & JSTN_RETURN_NULLABLE_OBJ)) {
        typemap[stackSlots - 1] = JSVAL_TYPE_OBJORNULL;
    } 

    /* Now restore the the original pc (after which early returns are ok). */
    if (resumeAfter) {
        MUST_FLOW_LABEL(restore_pc);
        regs->pc = pc - cs.length;
    } else {
        /*
         * If we take a snapshot on a goto, advance to the target address. This
         * avoids inner trees returning on a break goto, which the outer
         * recorder then would confuse with a break in the outer tree.
         */
        if (*pc == JSOP_GOTO)
            pc += GET_JUMP_OFFSET(pc);
        else if (*pc == JSOP_GOTOX)
            pc += GET_JUMPX_OFFSET(pc);
    }

    /*
     * Check if we already have a matching side exit; if so we can return that
     * side exit instead of creating a new one.
     */
    VMSideExit** exits = tree->sideExits.data();
    unsigned nexits = tree->sideExits.length();
    if (exitType == LOOP_EXIT) {
        for (unsigned n = 0; n < nexits; ++n) {
            VMSideExit* e = exits[n];
            if (e->pc == pc && (e->imacpc == fp->maybeImacropc()) &&
                ngslots == e->numGlobalSlots &&
                !memcmp(exits[n]->fullTypeMap(), typemap, typemap_size)) {
                AUDIT(mergedLoopExits);
#if defined JS_JIT_SPEW
                TreevisLogExit(cx, e);
#endif
                return e;
            }
        }
    }

    /* We couldn't find a matching side exit, so create a new one. */
    VMSideExit* exit = (VMSideExit*)
        traceAlloc().alloc(sizeof(VMSideExit) + (stackSlots + ngslots) * sizeof(JSValueType));

    /* Setup side exit structure. */
    exit->from = fragment;
    exit->calldepth = callDepth;
    exit->numGlobalSlots = ngslots;
    exit->numStackSlots = stackSlots;
    exit->numStackSlotsBelowCurrentFrame = cx->fp()->isFunctionFrame() ?
                                           nativeStackOffset(&cx->fp()->calleeValue()) / sizeof(double) :
                                           0;
    exit->exitType = exitType;
    exit->pc = pc;
    exit->imacpc = fp->maybeImacropc();
    exit->sp_adj = (stackSlots * sizeof(double)) - tree->nativeStackBase;
    exit->rp_adj = exit->calldepth * sizeof(FrameInfo*);
    exit->lookupFlags = js_InferFlags(cx, 0);
    memcpy(exit->fullTypeMap(), typemap, typemap_size);

#if defined JS_JIT_SPEW
    TreevisLogExit(cx, exit);
#endif
    return exit;
}

JS_REQUIRES_STACK GuardRecord*
TraceRecorder::createGuardRecord(VMSideExit* exit)
{
#ifdef JS_JIT_SPEW
    // For debug builds, place the guard records in a longer lasting
    // pool.  This is because the fragment profiler will look at them
    // relatively late in the day, after they would have been freed,
    // in some cases, had they been allocated in traceAlloc().
    GuardRecord* gr = new (dataAlloc()) GuardRecord();
#else
    // The standard place (for production builds).
    GuardRecord* gr = new (traceAlloc()) GuardRecord();
#endif

    gr->exit = exit;
    exit->addGuard(gr);

    // gr->profCount is calloc'd to zero
    verbose_only(
        gr->profGuardID = fragment->guardNumberer++;
        gr->nextInFrag = fragment->guardsForFrag;
        fragment->guardsForFrag = gr;
    )

    return gr;
}

/* Test if 'ins' is in a form that can be used as a guard/branch condition. */
static bool
isCond(LIns* ins)
{
    return ins->isCmp() || ins->isImmI(0) || ins->isImmI(1);
}

/* Ensure 'ins' is in a form suitable for a guard/branch condition. */
void
TraceRecorder::ensureCond(LIns** ins, bool* cond)
{
    if (!isCond(*ins)) {
        *cond = !*cond;
        *ins = (*ins)->isI() ? w.eqi0(*ins) : w.eqp0(*ins);
    }
}

/*
 * Emit a guard for condition (cond), expecting to evaluate to boolean result
 * (expected) and using the supplied side exit if the condition doesn't hold.
 *
 * Callers shouldn't generate guards that always exit (which can occur due to
 * optimization of the guard condition) because it's bad for both compile-time
 * speed (all the code generated after the guard is dead) and run-time speed
 * (fragment that always exit are slow).  This function has two modes for
 * handling an always-exit guard;  which mode is used depends on the value of
 * abortIfAlwaysExits:
 *
 * - abortIfAlwaysExits == false:  This is the default mode.  If the guard
 *   will always exit, we assert (in debug builds) as a signal that we are
 *   generating bad traces.  (In optimized builds that lack assertions the
 *   guard will be generated correctly, so the code will be slow but safe.) In
 *   this mode, the caller is responsible for not generating an always-exit
 *   guard.  The return value will always be RECORD_CONTINUE, so the caller
 *   need not check it.
 *
 * - abortIfAlwaysExits == true:  If the guard will always exit, we abort
 *   recording and return RECORD_STOP;  otherwise we generate the guard
 *   normally and return RECORD_CONTINUE.  This mode can be used when the
 *   caller doesn't know ahead of time whether the guard will always exit.  In
 *   this mode, the caller must check the return value.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guard(bool expected, LIns* cond, VMSideExit* exit,
                     bool abortIfAlwaysExits/* = false */)
{
    if (exit->exitType == LOOP_EXIT)
        tree->sideExits.add(exit);

    JS_ASSERT(isCond(cond));

    if ((cond->isImmI(0) && expected) || (cond->isImmI(1) && !expected)) {
        if (abortIfAlwaysExits) {
            /* The guard always exits, the caller must check for an abort. */
            RETURN_STOP("Constantly false guard detected");
        }
        /*
         * If this assertion fails, first decide if you want recording to
         * abort in the case where the guard always exits.  If not, find a way
         * to detect that case and avoid calling guard().  Otherwise, change
         * the invocation of guard() so it passes in abortIfAlwaysExits=true,
         * and have the caller check the return value, eg. using
         * CHECK_STATUS().  (In optimized builds, we'll fall through to the
         * insGuard() below and an always-exits guard will be inserted, which
         * is correct but sub-optimal.)
         */
        JS_ASSERT(0);
    }

    /*
     * Nb: if the guard is never taken, no instruction will be created and
     * insGuard() will return NULL.  This is a good thing.
     */
    GuardRecord* guardRec = createGuardRecord(exit);
    expected ? w.xf(cond, guardRec) : w.xt(cond, guardRec);
    return RECORD_CONTINUE;
}

/*
 * Emit a guard for condition (cond), expecting to evaluate to boolean result
 * (expected) and generate a side exit with type exitType to jump to if the
 * condition does not hold.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guard(bool expected, LIns* cond, ExitType exitType,
                     bool abortIfAlwaysExits/* = false */)
{
    return guard(expected, cond, snapshot(exitType), abortIfAlwaysExits);
}

/*
 * Emit a guard a 32-bit integer arithmetic operation op(d0, d1) and
 * using the supplied side exit if it overflows.
 */
JS_REQUIRES_STACK LIns*
TraceRecorder::guard_xov(LOpcode op, LIns* d0, LIns* d1, VMSideExit* exit)
{
    JS_ASSERT(exit->exitType == OVERFLOW_EXIT);

    GuardRecord* guardRec = createGuardRecord(exit);
    switch (op) {
      case LIR_addi:
        return w.addxovi(d0, d1, guardRec);
      case LIR_subi:
        return w.subxovi(d0, d1, guardRec);
      case LIR_muli:
        return w.mulxovi(d0, d1, guardRec);
      default:
        break;
    }
    JS_NOT_REACHED("unexpected opcode");
    return NULL;
}

JS_REQUIRES_STACK VMSideExit*
TraceRecorder::copy(VMSideExit* copy)
{
    size_t typemap_size = copy->numGlobalSlots + copy->numStackSlots;
    VMSideExit* exit = (VMSideExit*)
        traceAlloc().alloc(sizeof(VMSideExit) + typemap_size * sizeof(JSValueType));

    /* Copy side exit structure. */
    memcpy(exit, copy, sizeof(VMSideExit) + typemap_size * sizeof(JSValueType));
    exit->guards = NULL;
    exit->from = fragment;
    exit->target = NULL;

    if (exit->exitType == LOOP_EXIT)
        tree->sideExits.add(exit);
#if defined JS_JIT_SPEW
    TreevisLogExit(cx, exit);
#endif
    return exit;
}

/*
 * Determine whether any context associated with the same thread as cx is
 * executing native code.
 */
static inline bool
ProhibitFlush(JSContext* cx)
{
    if (cx->tracerState) // early out if the given is in native code
        return true;

    JSCList *cl;

#ifdef JS_THREADSAFE
    JSThread* thread = cx->thread;
    for (cl = thread->contextList.next; cl != &thread->contextList; cl = cl->next)
        if (CX_FROM_THREAD_LINKS(cl)->tracerState)
            return true;
#else
    JSRuntime* rt = cx->runtime;
    for (cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next)
        if (js_ContextFromLinkField(cl)->tracerState)
            return true;
#endif
    return false;
}

static void
ResetJITImpl(JSContext* cx)
{
    if (!cx->traceJitEnabled)
        return;
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    debug_only_print0(LC_TMTracer, "Flushing cache.\n");
    if (tm->recorder) {
        JS_ASSERT_NOT_ON_TRACE(cx);
        AbortRecording(cx, "flush cache");
    }
    if (ProhibitFlush(cx)) {
        debug_only_print0(LC_TMTracer, "Deferring JIT flush due to deep bail.\n");
        tm->needFlush = JS_TRUE;
        return;
    }
    tm->flush();
}

/* Compile the current fragment. */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::compile()
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_COMPILE);
#endif

    if (traceMonitor->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return ARECORD_ABORTED;
    }
    if (tree->maxNativeStackSlots >= MAX_NATIVE_STACK_SLOTS) {
        debug_only_print0(LC_TMTracer, "Blacklist: excessive stack use.\n");
        Blacklist((jsbytecode*)tree->ip);
        return ARECORD_STOP;
    }
    if (anchor && anchor->exitType != CASE_EXIT)
        ++tree->branchCount;
    if (outOfMemory())
        return ARECORD_STOP;

    /* :TODO: windows support */
#if defined DEBUG && !defined WIN32
    /* Associate a filename and line number with the fragment. */
    const char* filename = cx->fp()->script()->filename;
    char* label = (char*)js_malloc((filename ? strlen(filename) : 7) + 16);
    if (label) {
        sprintf(label, "%s:%u", filename ? filename : "<stdin>",
                js_FramePCToLineNumber(cx, cx->fp()));
        lirbuf->printer->addrNameMap->addAddrRange(fragment, sizeof(Fragment), 0, label);
        js_free(label);
    }
#endif

    Assembler *assm = traceMonitor->assembler;
    JS_ASSERT(assm->error() == nanojit::None);
    assm->compile(fragment, tempAlloc(), /*optimize*/true verbose_only(, lirbuf->printer));

    if (assm->error() != nanojit::None) {
        assm->setError(nanojit::None);
        debug_only_print0(LC_TMTracer, "Blacklisted: error during compilation\n");
        Blacklist((jsbytecode*)tree->ip);
        return ARECORD_STOP;
    }

    if (outOfMemory())
        return ARECORD_STOP;
    ResetRecordingAttempts(cx, (jsbytecode*)fragment->ip);
    ResetRecordingAttempts(cx, (jsbytecode*)tree->ip);
    if (anchor) {
#ifdef NANOJIT_IA32
        if (anchor->exitType == CASE_EXIT)
            assm->patch(anchor, anchor->switchInfo);
        else
#endif
            assm->patch(anchor);
    }
    JS_ASSERT(fragment->code());
    JS_ASSERT_IF(fragment == fragment->root, fragment->root == tree);

    return ARECORD_CONTINUE;
}

static void
JoinPeers(Assembler* assm, VMSideExit* exit, TreeFragment* target)
{
    exit->target = target;
    assm->patch(exit);

    debug_only_printf(LC_TMTreeVis, "TREEVIS JOIN ANCHOR=%p FRAG=%p\n", (void*)exit, (void*)target);

    if (exit->root() == target)
        return;

    target->dependentTrees.addUnique(exit->root());
    exit->root()->linkedTrees.addUnique(target);
}

/* Results of trying to connect an arbitrary type A with arbitrary type B */
enum TypeCheckResult
{
    TypeCheck_Okay,         /* Okay: same type */
    TypeCheck_Promote,      /* Okay: Type A needs d2i() */
    TypeCheck_Demote,       /* Okay: Type A needs i2d() */
    TypeCheck_Undemote,     /* Bad: Slot is undemotable */
    TypeCheck_Bad           /* Bad: incompatible types */
};

class SlotMap : public SlotVisitorBase
{
  public:
    struct SlotInfo
    {
        SlotInfo()
          : vp(NULL), promoteInt(false), lastCheck(TypeCheck_Bad)
        {}
        SlotInfo(Value* vp, bool promoteInt)
          : vp(vp), promoteInt(promoteInt), lastCheck(TypeCheck_Bad), type(getCoercedType(*vp))
        {}
        SlotInfo(JSValueType t)
          : vp(NULL), promoteInt(false), lastCheck(TypeCheck_Bad), type(t)
        {}
        SlotInfo(Value* vp, JSValueType t)
          : vp(vp), promoteInt(t == JSVAL_TYPE_INT32), lastCheck(TypeCheck_Bad), type(t)
        {}
        void            *vp;
        bool            promoteInt;
        TypeCheckResult lastCheck;
        JSValueType     type;
    };

    SlotMap(TraceRecorder& rec)
        : mRecorder(rec),
          mCx(rec.cx),
          slots(NULL)
    {
    }

    virtual ~SlotMap()
    {
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot)
    {
        addSlot(vp);
    }

    JS_ALWAYS_INLINE SlotMap::SlotInfo&
    operator [](unsigned i)
    {
        return slots[i];
    }

    JS_ALWAYS_INLINE SlotMap::SlotInfo&
    get(unsigned i)
    {
        return slots[i];
    }

    JS_ALWAYS_INLINE unsigned
    length()
    {
        return slots.length();
    }

    /**
     * Possible return states:
     *
     * TypeConsensus_Okay:      All types are compatible. Caller must go through slot list and handle
     *                          promote/demotes.
     * TypeConsensus_Bad:       Types are not compatible. Individual type check results are undefined.
     * TypeConsensus_Undemotes: Types would be compatible if slots were marked as undemotable
     *                          before recording began. Caller can go through slot list and mark
     *                          such slots as undemotable.
     */
    JS_REQUIRES_STACK TypeConsensus
    checkTypes(LinkableFragment* f)
    {
        if (length() != f->typeMap.length())
            return TypeConsensus_Bad;

        bool has_undemotes = false;
        for (unsigned i = 0; i < length(); i++) {
            TypeCheckResult result = checkType(i, f->typeMap[i]);
            if (result == TypeCheck_Bad)
                return TypeConsensus_Bad;
            if (result == TypeCheck_Undemote)
                has_undemotes = true;
            slots[i].lastCheck = result;
        }
        if (has_undemotes)
            return TypeConsensus_Undemotes;
        return TypeConsensus_Okay;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    addSlot(Value* vp)
    {
        bool promoteInt = false;
        if (vp->isNumber()) {
            if (LIns* i = mRecorder.getFromTracker(vp)) {
                promoteInt = IsPromoteInt(i);
            } else if (mRecorder.isGlobal(vp)) {
                int offset = mRecorder.tree->globalSlots->offsetOf(uint16(mRecorder.nativeGlobalSlot(vp)));
                JS_ASSERT(offset != -1);
                promoteInt = mRecorder.importTypeMap[mRecorder.importStackSlots + offset] ==
                             JSVAL_TYPE_INT32;
            } else {
                promoteInt = mRecorder.importTypeMap[mRecorder.nativeStackSlot(vp)] ==
                             JSVAL_TYPE_INT32;
            }
        }
        slots.add(SlotInfo(vp, promoteInt));
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    addSlot(JSValueType t)
    {
        slots.add(SlotInfo(NULL, t));
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    addSlot(Value *vp, JSValueType t)
    {
        slots.add(SlotInfo(vp, t));
    }

    JS_REQUIRES_STACK void
    markUndemotes()
    {
        for (unsigned i = 0; i < length(); i++) {
            if (get(i).lastCheck == TypeCheck_Undemote)
                mRecorder.markSlotUndemotable(mRecorder.tree, i);
        }
    }

    JS_REQUIRES_STACK virtual void
    adjustTail(TypeConsensus consensus)
    {
    }

    JS_REQUIRES_STACK virtual void
    adjustTypes()
    {
        for (unsigned i = 0; i < length(); i++)
            adjustType(get(i));
    }

  protected:
    JS_REQUIRES_STACK virtual void
    adjustType(SlotInfo& info) {
        JS_ASSERT(info.lastCheck != TypeCheck_Undemote && info.lastCheck != TypeCheck_Bad);
#ifdef DEBUG
        if (info.lastCheck == TypeCheck_Promote) {
            JS_ASSERT(info.type == JSVAL_TYPE_INT32 || info.type == JSVAL_TYPE_DOUBLE);
            /*
             * This should only happen if the slot has a trivial conversion, i.e.
             * IsPromoteInt() is true.  We check this.  
             *
             * Note that getFromTracker() will return NULL if the slot was
             * never used, in which case we don't do the check.  We could
             * instead called mRecorder.get(info.vp) and always check, but
             * get() has side-effects, which is not good in an assertion.
             * Not checking unused slots isn't so bad.
             */
            LIns* ins = mRecorder.getFromTrackerImpl(info.vp);
            JS_ASSERT_IF(ins, IsPromoteInt(ins));
        } else 
#endif
        if (info.lastCheck == TypeCheck_Demote) {
            JS_ASSERT(info.type == JSVAL_TYPE_INT32 || info.type == JSVAL_TYPE_DOUBLE);
            JS_ASSERT(mRecorder.getImpl(info.vp)->isD());

            /* Never demote this final i2d. */
            mRecorder.setImpl(info.vp, mRecorder.getImpl(info.vp), false);
        }
    }

  private:
    TypeCheckResult
    checkType(unsigned i, JSValueType t)
    {
        debug_only_printf(LC_TMTracer,
                          "checkType slot %d: interp=%c typemap=%c isNum=%d promoteInt=%d\n",
                          i,
                          TypeToChar(slots[i].type),
                          TypeToChar(t),
                          slots[i].type == JSVAL_TYPE_INT32 || slots[i].type == JSVAL_TYPE_DOUBLE,
                          slots[i].promoteInt);
        switch (t) {
          case JSVAL_TYPE_INT32:
            if (slots[i].type != JSVAL_TYPE_INT32 && slots[i].type != JSVAL_TYPE_DOUBLE)
                return TypeCheck_Bad; /* Not a number? Type mismatch. */
            /* This is always a type mismatch, we can't close a double to an int. */
            if (!slots[i].promoteInt)
                return TypeCheck_Undemote;
            /* Looks good, slot is an int32, the last instruction should be promotable. */
            JS_ASSERT_IF(slots[i].vp, hasInt32Repr(*(const Value *)slots[i].vp) && slots[i].promoteInt);
            return slots[i].vp ? TypeCheck_Promote : TypeCheck_Okay;
          case JSVAL_TYPE_DOUBLE:
            if (slots[i].type != JSVAL_TYPE_INT32 && slots[i].type != JSVAL_TYPE_DOUBLE)
                return TypeCheck_Bad; /* Not a number? Type mismatch. */
            if (slots[i].promoteInt)
                return slots[i].vp ? TypeCheck_Demote : TypeCheck_Bad;
            return TypeCheck_Okay;
          default:
            return slots[i].type == t ? TypeCheck_Okay : TypeCheck_Bad;
        }
        JS_NOT_REACHED("shouldn't fall through type check switch");
    }
  protected:
    TraceRecorder& mRecorder;
    JSContext* mCx;
    Queue<SlotInfo> slots;
};

class DefaultSlotMap : public SlotMap
{
  public:
    DefaultSlotMap(TraceRecorder& tr) : SlotMap(tr)
    {
    }
    
    virtual ~DefaultSlotMap()
    {
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp)
    {
        for (size_t i = 0; i < count; i++)
            addSlot(&vp[i]);
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp)
    {
        addSlot(getFrameObjPtrTraceType(p, fp));
        return true;
    }
};

JS_REQUIRES_STACK TypeConsensus
TraceRecorder::selfTypeStability(SlotMap& slotMap)
{
    debug_only_printf(LC_TMTracer, "Checking type stability against self=%p\n", (void*)fragment);
    TypeConsensus consensus = slotMap.checkTypes(tree);

    /* Best case: loop jumps back to its own header */
    if (consensus == TypeConsensus_Okay)
        return TypeConsensus_Okay;

    /* If the only thing keeping this loop from being stable is undemotions, then mark relevant
     * slots as undemotable.
     */
    if (consensus == TypeConsensus_Undemotes)
        slotMap.markUndemotes();

    return consensus;
}

JS_REQUIRES_STACK TypeConsensus
TraceRecorder::peerTypeStability(SlotMap& slotMap, const void* ip, TreeFragment** pPeer)
{
    /* See if there are any peers that would make this stable */
    JS_ASSERT(fragment->root == tree);
    TreeFragment* peer = LookupLoop(traceMonitor, ip, tree->globalObj, tree->globalShape, tree->argc);

    /* This condition is possible with recursion */
    JS_ASSERT_IF(!peer, tree->ip != ip);
    if (!peer)
        return TypeConsensus_Bad;
    bool onlyUndemotes = false;
    for (; peer != NULL; peer = peer->peer) {
        if (!peer->code() || peer == fragment)
            continue;
        debug_only_printf(LC_TMTracer, "Checking type stability against peer=%p\n", (void*)peer);
        TypeConsensus consensus = slotMap.checkTypes(peer);
        if (consensus == TypeConsensus_Okay) {
            *pPeer = peer;
            /* Return this even though there will be linkage; the trace itself is not stable.
             * Caller should inspect ppeer to check for a compatible peer.
             */
            return TypeConsensus_Okay;
        }
        if (consensus == TypeConsensus_Undemotes)
            onlyUndemotes = true;
    }

    return onlyUndemotes ? TypeConsensus_Undemotes : TypeConsensus_Bad;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::closeLoop()
{
    return closeLoop(snapshot(UNSTABLE_LOOP_EXIT));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::closeLoop(VMSideExit* exit)
{
    DefaultSlotMap slotMap(*this);
    VisitSlots(slotMap, cx, 0, *tree->globalSlots);
    return closeLoop(slotMap, exit);
}

/*
 * Complete and compile a trace and link it to the existing tree if
 * appropriate.  Returns ARECORD_ABORTED or ARECORD_STOP, depending on whether
 * the recorder was deleted. Outparam is always set.
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::closeLoop(SlotMap& slotMap, VMSideExit* exit)
{
    /*
     * We should have arrived back at the loop header, and hence we don't want
     * to be in an imacro here and the opcode should be either JSOP_TRACE or, in
     * case this loop was blacklisted in the meantime, JSOP_NOTRACE.
     */
    JS_ASSERT((*cx->regs->pc == JSOP_TRACE || *cx->regs->pc == JSOP_NOTRACE) &&
              !cx->fp()->hasImacropc());

    if (callDepth != 0) {
        debug_only_print0(LC_TMTracer,
                          "Blacklisted: stack depth mismatch, possible recursion.\n");
        Blacklist((jsbytecode*)tree->ip);
        trashSelf = true;
        return ARECORD_STOP;
    }

    JS_ASSERT_IF(exit->exitType == UNSTABLE_LOOP_EXIT,
                 exit->numStackSlots == tree->nStackTypes);

    JS_ASSERT(fragment->root == tree);

    TreeFragment* peer = NULL;

    TypeConsensus consensus = TypeConsensus_Bad;

    if (exit->exitType == UNSTABLE_LOOP_EXIT)
        consensus = selfTypeStability(slotMap);
    if (consensus != TypeConsensus_Okay) {
        TypeConsensus peerConsensus = peerTypeStability(slotMap, tree->ip, &peer);
        /* If there was a semblance of a stable peer (even if not linkable), keep the result. */
        if (peerConsensus != TypeConsensus_Bad)
            consensus = peerConsensus;
    }

#if DEBUG
    if (consensus != TypeConsensus_Okay || peer)
        AUDIT(unstableLoopVariable);
#endif

    JS_ASSERT(!trashSelf);

    /*
     * This exit is indeed linkable to something now. Process any promote or
     * demotes that are pending in the slot map.
     */
    if (consensus == TypeConsensus_Okay)
        slotMap.adjustTypes();

    /* Give up-recursion a chance to pop the stack frame. */
    slotMap.adjustTail(consensus);

    if (consensus != TypeConsensus_Okay || peer) {
        fragment->lastIns = w.x(createGuardRecord(exit));

        /* If there is a peer, there must have been an "Okay" consensus. */
        JS_ASSERT_IF(peer, consensus == TypeConsensus_Okay);

        /* Compile as a type-unstable loop, and hope for a connection later. */
        if (!peer) {
            /*
             * If such a fragment does not exist, let's compile the loop ahead
             * of time anyway.  Later, if the loop becomes type stable, we will
             * connect these two fragments together.
             */
            debug_only_print0(LC_TMTracer,
                              "Trace has unstable loop variable with no stable peer, "
                              "compiling anyway.\n");
            UnstableExit* uexit = new (traceAlloc()) UnstableExit;
            uexit->fragment = fragment;
            uexit->exit = exit;
            uexit->next = tree->unstableExits;
            tree->unstableExits = uexit;
        } else {
            JS_ASSERT(peer->code());
            exit->target = peer;
            debug_only_printf(LC_TMTracer,
                              "Joining type-unstable trace to target fragment %p.\n",
                              (void*)peer);
            peer->dependentTrees.addUnique(tree);
            tree->linkedTrees.addUnique(peer);
        }
    } else {
        exit->exitType = LOOP_EXIT;
        debug_only_printf(LC_TMTreeVis, "TREEVIS CHANGEEXIT EXIT=%p TYPE=%s\n", (void*)exit,
                          getExitName(LOOP_EXIT));

        JS_ASSERT((fragment == fragment->root) == !!loopLabel);
        if (loopLabel) {
            w.j(loopLabel);
            w.comment("end-loop");
            w.livep(lirbuf->state);
        }

        exit->target = tree;
        /*
         * This guard is dead code.  However, it must be present because it
         * can keep alive values on the stack.  Without it, StackFilter can
         * remove some stack stores that it shouldn't.  See bug 582766 comment
         * 19.
         */
        fragment->lastIns = w.x(createGuardRecord(exit));
    }

    CHECK_STATUS_A(compile());

    debug_only_printf(LC_TMTreeVis, "TREEVIS CLOSELOOP EXIT=%p PEER=%p\n", (void*)exit, (void*)peer);

    JS_ASSERT(LookupLoop(traceMonitor, tree->ip, tree->globalObj, tree->globalShape, tree->argc) ==
              tree->first);
    JS_ASSERT(tree->first);

    peer = tree->first;
    joinEdgesToEntry(peer);

    debug_only_stmt(DumpPeerStability(traceMonitor, peer->ip, peer->globalObj,
                                      peer->globalShape, peer->argc);)

    debug_only_print0(LC_TMTracer,
                      "updating specializations on dependent and linked trees\n");
    if (tree->code())
        SpecializeTreesToMissingGlobals(cx, globalObj, tree);

    /*
     * If this is a newly formed tree, and the outer tree has not been compiled yet, we
     * should try to compile the outer tree again.
     */
    if (outerPC)
        AttemptCompilation(cx, globalObj, outerScript, outerPC, outerArgc);
#ifdef JS_JIT_SPEW
    debug_only_printf(LC_TMMinimal,
                      "Recording completed at  %s:%u@%u via closeLoop (FragID=%06u)\n",
                      cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()),
                      fragment->profFragID);
    debug_only_print0(LC_TMMinimal, "\n");
#endif

    return finishSuccessfully();
}

static void
FullMapFromExit(TypeMap& typeMap, VMSideExit* exit)
{
    typeMap.setLength(0);
    typeMap.fromRaw(exit->stackTypeMap(), exit->numStackSlots);
    typeMap.fromRaw(exit->globalTypeMap(), exit->numGlobalSlots);
    /* Include globals that were later specialized at the root of the tree. */
    if (exit->numGlobalSlots < exit->root()->nGlobalTypes()) {
        typeMap.fromRaw(exit->root()->globalTypeMap() + exit->numGlobalSlots,
                        exit->root()->nGlobalTypes() - exit->numGlobalSlots);
    }
}

static JS_REQUIRES_STACK TypeConsensus
TypeMapLinkability(JSContext* cx, const TypeMap& typeMap, TreeFragment* peer)
{
    const TypeMap& peerMap = peer->typeMap;
    unsigned minSlots = JS_MIN(typeMap.length(), peerMap.length());
    TypeConsensus consensus = TypeConsensus_Okay;
    for (unsigned i = 0; i < minSlots; i++) {
        if (typeMap[i] == peerMap[i])
            continue;
        if (typeMap[i] == JSVAL_TYPE_INT32 && peerMap[i] == JSVAL_TYPE_DOUBLE &&
            IsSlotUndemotable(JS_TRACE_MONITOR(cx).oracle, cx, peer, i, peer->ip)) {
            consensus = TypeConsensus_Undemotes;
        } else {
            return TypeConsensus_Bad;
        }
    }
    return consensus;
}

JS_REQUIRES_STACK unsigned
TraceRecorder::findUndemotesInTypemaps(const TypeMap& typeMap, LinkableFragment* f,
                        Queue<unsigned>& undemotes)
{
    undemotes.setLength(0);
    unsigned minSlots = JS_MIN(typeMap.length(), f->typeMap.length());
    for (unsigned i = 0; i < minSlots; i++) {
        if (typeMap[i] == JSVAL_TYPE_INT32 && f->typeMap[i] == JSVAL_TYPE_DOUBLE) {
            undemotes.add(i);
        } else if (typeMap[i] != f->typeMap[i]) {
            return 0;
        }
    }
    for (unsigned i = 0; i < undemotes.length(); i++)
        markSlotUndemotable(f, undemotes[i]);
    return undemotes.length();
}

JS_REQUIRES_STACK void
TraceRecorder::joinEdgesToEntry(TreeFragment* peer_root)
{
    if (fragment->root != fragment)
        return;

    TypeMap typeMap(NULL);
    Queue<unsigned> undemotes(NULL);

    for (TreeFragment* peer = peer_root; peer; peer = peer->peer) {
        if (!peer->code())
            continue;
        UnstableExit* uexit = peer->unstableExits;
        while (uexit != NULL) {
            /* Build the full typemap for this unstable exit */
            FullMapFromExit(typeMap, uexit->exit);
            /* Check its compatibility against this tree */
            TypeConsensus consensus = TypeMapLinkability(cx, typeMap, tree);
            JS_ASSERT_IF(consensus == TypeConsensus_Okay, peer != fragment);
            if (consensus == TypeConsensus_Okay) {
                debug_only_printf(LC_TMTracer,
                                  "Joining type-stable trace to target exit %p->%p.\n",
                                  (void*)uexit->fragment, (void*)uexit->exit);

                /*
                 * See bug 531513. Before linking these trees, make sure the
                 * peer's dependency graph is up to date.
                 */
                TreeFragment* from = uexit->exit->root();
                if (from->nGlobalTypes() < tree->nGlobalTypes()) {
                    SpecializeTreesToLateGlobals(cx, from, tree->globalTypeMap(),
                                                 tree->nGlobalTypes());
                }

                /* It's okay! Link together and remove the unstable exit. */
                JS_ASSERT(tree == fragment);
                JoinPeers(traceMonitor->assembler, uexit->exit, tree);
                uexit = peer->removeUnstableExit(uexit->exit);
            } else {
                /* Check for int32->double slots that suggest trashing. */
                if (findUndemotesInTypemaps(typeMap, tree, undemotes)) {
                    JS_ASSERT(peer == uexit->fragment->root);
                    if (fragment == peer)
                        trashSelf = true;
                    else
                        whichTreesToTrash.addUnique(uexit->fragment->root);
                    return;
                }
                uexit = uexit->next;
            }
        }
    }
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::endLoop()
{
    return endLoop(snapshot(LOOP_EXIT));
}

/* Emit an always-exit guard and compile the tree (used for break statements. */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::endLoop(VMSideExit* exit)
{
    JS_ASSERT(fragment->root == tree);

    if (callDepth != 0) {
        debug_only_print0(LC_TMTracer, "Blacklisted: stack depth mismatch, possible recursion.\n");
        Blacklist((jsbytecode*)tree->ip);
        trashSelf = true;
        return ARECORD_STOP;
    }

    fragment->lastIns = w.x(createGuardRecord(exit));

    CHECK_STATUS_A(compile());

    debug_only_printf(LC_TMTreeVis, "TREEVIS ENDLOOP EXIT=%p\n", (void*)exit);

    JS_ASSERT(LookupLoop(traceMonitor, tree->ip, tree->globalObj, tree->globalShape, tree->argc) ==
              tree->first);

    joinEdgesToEntry(tree->first);

    debug_only_stmt(DumpPeerStability(traceMonitor, tree->ip, tree->globalObj,
                                      tree->globalShape, tree->argc);)

    /*
     * Note: this must always be done, in case we added new globals on trace
     * and haven't yet propagated those to linked and dependent trees.
     */
    debug_only_print0(LC_TMTracer,
                      "updating specializations on dependent and linked trees\n");
    if (tree->code())
        SpecializeTreesToMissingGlobals(cx, globalObj, fragment->root);

    /*
     * If this is a newly formed tree, and the outer tree has not been compiled
     * yet, we should try to compile the outer tree again.
     */
    if (outerPC)
        AttemptCompilation(cx, globalObj, outerScript, outerPC, outerArgc);
#ifdef JS_JIT_SPEW
    debug_only_printf(LC_TMMinimal,
                      "Recording completed at  %s:%u@%u via endLoop (FragID=%06u)\n",
                      cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()),
                      fragment->profFragID);
    debug_only_print0(LC_TMTracer, "\n");
#endif

    return finishSuccessfully();
}

/* Emit code to adjust the stack to match the inner tree's stack expectations. */
JS_REQUIRES_STACK void
TraceRecorder::prepareTreeCall(TreeFragment* inner)
{
    VMSideExit* exit = snapshot(OOM_EXIT);

    /*
     * The inner tree expects to be called from the current frame. If the outer
     * tree (this trace) is currently inside a function inlining code
     * (calldepth > 0), we have to advance the native stack pointer such that
     * we match what the inner trace expects to see. We move it back when we
     * come out of the inner tree call.
     */
    if (callDepth > 0) {
        /*
         * Calculate the amount we have to lift the native stack pointer by to
         * compensate for any outer frames that the inner tree doesn't expect
         * but the outer tree has.
         */
        ptrdiff_t sp_adj = nativeStackOffset(&cx->fp()->calleeValue());

        /* Calculate the amount we have to lift the call stack by. */
        ptrdiff_t rp_adj = callDepth * sizeof(FrameInfo*);

        /*
         * Guard that we have enough stack space for the tree we are trying to
         * call on top of the new value for sp.
         */
        debug_only_printf(LC_TMTracer,
                          "sp_adj=%lld outer=%lld inner=%lld\n",
                          (long long int)sp_adj,
                          (long long int)tree->nativeStackBase,
                          (long long int)inner->nativeStackBase);
        ptrdiff_t sp_offset =
                - tree->nativeStackBase /* rebase sp to beginning of outer tree's stack */
                + sp_adj /* adjust for stack in outer frame inner tree can't see */
                + inner->maxNativeStackSlots * sizeof(double); /* plus the inner tree's stack */
        LIns* sp_top = w.addp(lirbuf->sp, w.nameImmw(sp_offset));
        guard(true, w.ltp(sp_top, eos_ins), exit);

        /* Guard that we have enough call stack space. */
        ptrdiff_t rp_offset = rp_adj + inner->maxCallDepth * sizeof(FrameInfo*);
        LIns* rp_top = w.addp(lirbuf->rp, w.nameImmw(rp_offset));
        guard(true, w.ltp(rp_top, eor_ins), exit);

        sp_offset =
                - tree->nativeStackBase /* rebase sp to beginning of outer tree's stack */
                + sp_adj /* adjust for stack in outer frame inner tree can't see */
                + inner->nativeStackBase; /* plus the inner tree's stack base */
        /* We have enough space, so adjust sp and rp to their new level. */
        w.stStateField(w.addp(lirbuf->sp, w.nameImmw(sp_offset)), sp);
        w.stStateField(w.addp(lirbuf->rp, w.nameImmw(rp_adj)), rp);
    }

    /*
     * The inner tree will probably access stack slots. So tell nanojit not to
     * discard or defer stack writes before emitting the call tree code.
     *
     * (The ExitType of this snapshot is nugatory. The exit can't be taken.)
     */
    w.xbarrier(createGuardRecord(exit));
}

static unsigned
BuildGlobalTypeMapFromInnerTree(Queue<JSValueType>& typeMap, VMSideExit* inner)
{
#if defined DEBUG
    unsigned initialSlots = typeMap.length();
#endif
    /* First, use the innermost exit's global typemap. */
    typeMap.add(inner->globalTypeMap(), inner->numGlobalSlots);

    /* Add missing global types from the innermost exit's tree. */
    TreeFragment* innerFrag = inner->root();
    unsigned slots = inner->numGlobalSlots;
    if (slots < innerFrag->nGlobalTypes()) {
        typeMap.add(innerFrag->globalTypeMap() + slots, innerFrag->nGlobalTypes() - slots);
        slots = innerFrag->nGlobalTypes();
    }
    JS_ASSERT(typeMap.length() - initialSlots == slots);
    return slots;
}

/* Record a call to an inner tree. */
JS_REQUIRES_STACK void
TraceRecorder::emitTreeCall(TreeFragment* inner, VMSideExit* exit)
{
    /* Invoke the inner tree. */
    LIns* args[] = { lirbuf->state }; /* reverse order */
    /* Construct a call info structure for the target tree. */
    CallInfo* ci = new (traceAlloc()) CallInfo();
    ci->_address = uintptr_t(inner->code());
    JS_ASSERT(ci->_address);
    ci->_typesig = CallInfo::typeSig1(ARGTYPE_P, ARGTYPE_P);
    ci->_isPure = 0;
    ci->_storeAccSet = ACCSET_STORE_ANY;
    ci->_abi = ABI_FASTCALL;
#ifdef DEBUG
    ci->_name = "fragment";
#endif
    LIns* rec = w.call(ci, args);
    LIns* lr = w.ldpGuardRecordExit(rec);
    LIns* nested = w.jtUnoptimizable(w.eqiN(w.ldiVMSideExitField(lr, exitType), NESTED_EXIT));

    /*
     * If the tree exits on a regular (non-nested) guard, keep updating lastTreeExitGuard
     * with that guard. If we mismatch on a tree call guard, this will contain the last
     * non-nested guard we encountered, which is the innermost loop or branch guard.
     */
    w.stStateField(lr, lastTreeExitGuard);
    LIns* done1 = w.j(NULL);

    /*
     * The tree exited on a nested guard. This only occurs once a tree call guard mismatches
     * and we unwind the tree call stack. We store the first (innermost) tree call guard in state
     * and we will try to grow the outer tree the failing call was in starting at that guard.
     */
    w.label(nested);
    LIns* done2 = w.jfUnoptimizable(w.eqp0(w.ldpStateField(lastTreeCallGuard)));
    w.stStateField(lr, lastTreeCallGuard);
    w.stStateField(w.addp(w.ldpStateField(rp),
                          w.i2p(w.lshiN(w.ldiVMSideExitField(lr, calldepth),
                                        sizeof(void*) == 4 ? 2 : 3))),
                   rpAtLastTreeCall);
    w.label(done1, done2);

    /*
     * Keep updating outermostTreeExit so that TracerState always contains the most recent
     * side exit.
     */
    w.stStateField(lr, outermostTreeExitGuard);

    /* Read back all registers, in case the called tree changed any of them. */
#ifdef DEBUG
    JSValueType* map;
    size_t i;
    map = exit->globalTypeMap();
    for (i = 0; i < exit->numGlobalSlots; i++)
        JS_ASSERT(map[i] != JSVAL_TYPE_BOXED);
    map = exit->stackTypeMap();
    for (i = 0; i < exit->numStackSlots; i++)
        JS_ASSERT(map[i] != JSVAL_TYPE_BOXED);
#endif

    /*
     * Clear anything from the tracker that the inner tree could have written so
     * that it will be lazily reloaded from the native stack. The only portion
     * of the native stack that may be written to by both the inner and outer
     * tree are the slots associated with the inner tree's entry frame. We may
     * be certain of this by the following argument:
     *
     *  0. The only way for the inner tree to mutate the outer tree's upvars
     *     is by writing to the call object associated with cx->fp or the
     *     callDepth frames below it.
     *  1. To write to a given call object, the innerTree must be executing with
     *     that call object on its scope chain.
     *  2. Only function frames' scope chains may contain call objects.
     *  3. An interpreted function frame's scope chain is set to be the parent
     *     of the callee when the callee is called.
     *  4. On trace, there is a guard on the identity of the callee's parent.
     *  5. From 1, 2, 3 and 4, any call object written to on trace will be the
     *     exact same call object as was observed at record time.
     *  6. Inner trees are recorded before outer trees.
     *  7. From 5 and 6, the only call object created by an outer tree that can
     *     be written to by an inner tree before the call object's associated
     *     stack frame is popped is cx->fp->callObj.
     *  8. setCallProp has an explicit guard against writing to cx->fp->callObj.
     *  9. From 0, 7 and 8, an inner tree never mutates its outer tree's upvars.
     */
    clearCurrentFrameSlotsFromTracker(tracker);
    SlotList& gslots = *tree->globalSlots;
    for (unsigned i = 0; i < gslots.length(); i++) {
        unsigned slot = gslots[i];
        Value* vp = &globalObj->getSlotRef(slot);
        tracker.set(vp, NULL);
    }

    /* Set stack slots from the innermost frame. */
    importTypeMap.setLength(NativeStackSlots(cx, callDepth));
#ifdef DEBUG
    for (unsigned i = importStackSlots; i < importTypeMap.length(); i++)
        importTypeMap[i] = JSVAL_TYPE_UNINITIALIZED;
#endif
    unsigned startOfInnerFrame = importTypeMap.length() - exit->numStackSlots;
    for (unsigned i = 0; i < exit->numStackSlots; i++)
        importTypeMap[startOfInnerFrame + i] = exit->stackTypeMap()[i];
    importStackSlots = importTypeMap.length();
    JS_ASSERT(importStackSlots == NativeStackSlots(cx, callDepth));

    /*
     * Bug 502604 - It is illegal to extend from the outer typemap without
     * first extending from the inner. Make a new typemap here.
     */
    BuildGlobalTypeMapFromInnerTree(importTypeMap, exit);

    importGlobalSlots = importTypeMap.length() - importStackSlots;
    JS_ASSERT(importGlobalSlots == tree->globalSlots->length());

    /* Restore sp and rp to their original values (we still have them in a register). */
    if (callDepth > 0) {
        w.stStateField(lirbuf->sp, sp);
        w.stStateField(lirbuf->rp, rp);
    }

    /*
     * Guard that we come out of the inner tree along the same side exit we came out when
     * we called the inner tree at recording time.
     */
    VMSideExit* nestedExit = snapshot(NESTED_EXIT);
    JS_ASSERT(exit->exitType == LOOP_EXIT);
    guard(true, w.eqp(lr, w.nameImmpNonGC(exit)), nestedExit);
    debug_only_printf(LC_TMTreeVis, "TREEVIS TREECALL INNER=%p EXIT=%p GUARD=%p\n", (void*)inner,
                      (void*)nestedExit, (void*)exit);

    /* Register us as a dependent tree of the inner tree. */
    inner->dependentTrees.addUnique(fragment->root);
    tree->linkedTrees.addUnique(inner);
}

/* Add a if/if-else control-flow merge point to the list of known merge points. */
JS_REQUIRES_STACK void
TraceRecorder::trackCfgMerges(jsbytecode* pc)
{
    /* If we hit the beginning of an if/if-else, then keep track of the merge point after it. */
    JS_ASSERT((*pc == JSOP_IFEQ) || (*pc == JSOP_IFEQX));
    jssrcnote* sn = js_GetSrcNote(cx->fp()->script(), pc);
    if (sn != NULL) {
        if (SN_TYPE(sn) == SRC_IF) {
            cfgMerges.add((*pc == JSOP_IFEQ)
                          ? pc + GET_JUMP_OFFSET(pc)
                          : pc + GET_JUMPX_OFFSET(pc));
        } else if (SN_TYPE(sn) == SRC_IF_ELSE)
            cfgMerges.add(pc + js_GetSrcNoteOffset(sn, 0));
    }
}

/*
 * Invert the direction of the guard if this is a loop edge that is not
 * taken (thin loop).
 */
JS_REQUIRES_STACK void
TraceRecorder::emitIf(jsbytecode* pc, bool cond, LIns* x)
{
    ExitType exitType;
    JS_ASSERT(isCond(x));
    if (IsLoopEdge(pc, (jsbytecode*)tree->ip)) {
        exitType = LOOP_EXIT;

        /*
         * If we are about to walk out of the loop, generate code for the
         * inverse loop condition, pretending we recorded the case that stays
         * on trace.
         */
        if ((*pc == JSOP_IFEQ || *pc == JSOP_IFEQX) == cond) {
            JS_ASSERT(*pc == JSOP_IFNE || *pc == JSOP_IFNEX || *pc == JSOP_IFEQ || *pc == JSOP_IFEQX);
            debug_only_print0(LC_TMTracer,
                              "Walking out of the loop, terminating it anyway.\n");
            cond = !cond;
        }

        /*
         * Conditional guards do not have to be emitted if the condition is
         * constant. We make a note whether the loop condition is true or false
         * here, so we later know whether to emit a loop edge or a loop end.
         */
        if (x->isImmI()) {
            pendingLoop = (x->immI() == int32(cond));
            return;
        }
    } else {
        exitType = BRANCH_EXIT;
    }
    if (!x->isImmI())
        guard(cond, x, exitType);
}

/* Emit code for a fused IFEQ/IFNE. */
JS_REQUIRES_STACK void
TraceRecorder::fuseIf(jsbytecode* pc, bool cond, LIns* x)
{
    if (*pc == JSOP_IFEQ || *pc == JSOP_IFNE) {
        emitIf(pc, cond, x);
        if (*pc == JSOP_IFEQ)
            trackCfgMerges(pc);
    }
}

/* Check whether we have reached the end of the trace. */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::checkTraceEnd(jsbytecode *pc)
{
    if (IsLoopEdge(pc, (jsbytecode*)tree->ip)) {
        /*
         * If we compile a loop, the trace should have a zero stack balance at
         * the loop edge. Currently we are parked on a comparison op or
         * IFNE/IFEQ, so advance pc to the loop header and adjust the stack
         * pointer and pretend we have reached the loop header.
         */
        if (pendingLoop) {
            JS_ASSERT(!cx->fp()->hasImacropc() && (pc == cx->regs->pc || pc == cx->regs->pc + 1));
            JSFrameRegs orig = *cx->regs;

            cx->regs->pc = (jsbytecode*)tree->ip;
            cx->regs->sp = cx->fp()->base() + tree->spOffsetAtEntry;

            JSContext* localcx = cx;
            AbortableRecordingStatus ars = closeLoop();
            *localcx->regs = orig;
            return ars;
        }

        return endLoop();
    }
    return ARECORD_CONTINUE;
}

/*
 * Check whether the shape of the global object has changed. The return value
 * indicates whether the recorder is still active.  If 'false', any active
 * recording has been aborted and the JIT may have been reset.
 */
static JS_REQUIRES_STACK bool
CheckGlobalObjectShape(JSContext* cx, TraceMonitor* tm, JSObject* globalObj,
                       uint32 *shape = NULL, SlotList** slots = NULL)
{
    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return false;
    }

    if (globalObj->numSlots() > MAX_GLOBAL_SLOTS) {
        if (tm->recorder)
            AbortRecording(cx, "too many slots in global object");
        return false;
    }

    /*
     * The global object must have a unique shape. That way, if an operand
     * isn't the global at record time, a shape guard suffices to ensure
     * that it isn't the global at run time.
     */
    if (!globalObj->hasOwnShape()) {
        if (!globalObj->globalObjectOwnShapeChange(cx)) {
            debug_only_print0(LC_TMTracer,
                              "Can't record: failed to give globalObj a unique shape.\n");
            return false;
        }
    }

    uint32 globalShape = globalObj->shape();

    if (tm->recorder) {
        TreeFragment* root = tm->recorder->getFragment()->root;

        /* Check the global shape matches the recorder's treeinfo's shape. */
        if (globalObj != root->globalObj || globalShape != root->globalShape) {
            AUDIT(globalShapeMismatchAtEntry);
            debug_only_printf(LC_TMTracer,
                              "Global object/shape mismatch (%p/%u vs. %p/%u), flushing cache.\n",
                              (void*)globalObj, globalShape, (void*)root->globalObj,
                              root->globalShape);
            Backoff(cx, (jsbytecode*) root->ip);
            ResetJIT(cx, FR_GLOBAL_SHAPE_MISMATCH);
            return false;
        }
        if (shape)
            *shape = globalShape;
        if (slots)
            *slots = root->globalSlots;
        return true;
    }

    /* No recorder, search for a tracked global-state (or allocate one). */
    for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
        GlobalState &state = tm->globalStates[i];

        if (state.globalShape == uint32(-1)) {
            state.globalObj = globalObj;
            state.globalShape = globalShape;
            JS_ASSERT(state.globalSlots);
            JS_ASSERT(state.globalSlots->length() == 0);
        }

        if (state.globalObj == globalObj && state.globalShape == globalShape) {
            if (shape)
                *shape = globalShape;
            if (slots)
                *slots = state.globalSlots;
            return true;
        }
    }

    /* No currently-tracked-global found and no room to allocate, abort. */
    AUDIT(globalShapeMismatchAtEntry);
    debug_only_printf(LC_TMTracer,
                      "No global slotlist for global shape %u, flushing cache.\n",
                      globalShape);
    ResetJIT(cx, FR_GLOBALS_FULL);
    return false;
}

/*
 * Return whether or not the recorder could be started. If 'false', the JIT has
 * been reset in response to an OOM.
 */
bool JS_REQUIRES_STACK
TraceRecorder::startRecorder(JSContext* cx, VMSideExit* anchor, VMFragment* f,
                             unsigned stackSlots, unsigned ngslots,
                             JSValueType* typeMap, VMSideExit* expectedInnerExit,
                             JSScript* outerScript, jsbytecode* outerPC, uint32 outerArgc,
                             bool speculate)
{
    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(!tm->needFlush);
    JS_ASSERT_IF(cx->fp()->hasImacropc(), f->root != f);

    tm->recorder = new TraceRecorder(cx, anchor, f, stackSlots, ngslots, typeMap,
                                     expectedInnerExit, outerScript, outerPC, outerArgc,
                                     speculate);

    if (!tm->recorder || tm->outOfMemory() || OverfullJITCache(tm)) {
        ResetJIT(cx, FR_OOM);
        return false;
    }

    return true;
}

static void
TrashTree(TreeFragment* f)
{
    JS_ASSERT(f == f->root);
    debug_only_printf(LC_TMTreeVis, "TREEVIS TRASH FRAG=%p\n", (void*)f);

    if (!f->code())
        return;
    AUDIT(treesTrashed);
    debug_only_print0(LC_TMTracer, "Trashing tree info.\n");
    f->setCode(NULL);
    TreeFragment** data = f->dependentTrees.data();
    unsigned length = f->dependentTrees.length();
    for (unsigned n = 0; n < length; ++n)
        TrashTree(data[n]);
    data = f->linkedTrees.data();
    length = f->linkedTrees.length();
    for (unsigned n = 0; n < length; ++n)
        TrashTree(data[n]);
}

static void
SynthesizeFrame(JSContext* cx, const FrameInfo& fi, JSObject* callee)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    /* Assert that we have a correct sp distance from cx->fp()->slots in fi. */
    JSStackFrame* const fp = cx->fp();
    JS_ASSERT_IF(!fi.imacpc,
                 js_ReconstructStackDepth(cx, fp->script(), fi.pc) ==
                 uintN(fi.spdist - fp->numFixed()));

    /* Use the just-flushed prev-frame to get the callee function. */
    JSFunction* newfun = callee->getFunctionPrivate();
    JSScript* newscript = newfun->script();

    /* Fill in the prev-frame's sp. */
    JSFrameRegs *regs = cx->regs;
    regs->sp = fp->slots() + fi.spdist;
    regs->pc = fi.pc;
    if (fi.imacpc)
        fp->setImacropc(fi.imacpc);

    /* Set argc/flags then mimic JSOP_CALL. */
    uintN argc = fi.get_argc();
    uint32 flags = fi.is_constructing ()
                   ? JSFRAME_CONSTRUCTING | JSFRAME_CONSTRUCTING
                   : 0;

    /* Get pointer to new/frame/slots, prepare arguments. */
    StackSpace &stack = cx->stack();
    JSStackFrame *newfp = stack.getInlineFrame(cx, regs->sp, argc, newfun,
                                               newscript, &flags);

    /* Initialize frame; do not need to initialize locals. */
    newfp->initCallFrame(cx, *callee, newfun, argc, flags);

#ifdef DEBUG
    /* The stack is conservatively marked, so we can leave non-canonical args uninitialized. */
    if (newfp->hasOverflowArgs()) {
        Value *beg = newfp->actualArgs() - 2;
        Value *end = newfp->actualArgs() + newfp->numFormalArgs();
        for (Value *p = beg; p != end; ++p)
            p->setMagic(JS_ARG_POISON);
    }

    /* These should be initialized by FlushNativeStackFrame. */
    newfp->thisValue().setMagic(JS_THIS_POISON);
    newfp->setScopeChainNoCallObj(*JSStackFrame::sInvalidScopeChain);
#endif

    /* Officially push the frame. */
    stack.pushInlineFrame(cx, newscript, newfp, cx->regs);

    /* Call object will be set by FlushNativeStackFrame. */

    /* Call the debugger hook if present. */
    JSInterpreterHook hook = cx->debugHooks->callHook;
    if (hook) {
        newfp->setHookData(hook(cx, newfp, JS_TRUE, 0,
                                cx->debugHooks->callHookData));
    }
}

static JS_REQUIRES_STACK bool
RecordTree(JSContext* cx, TreeFragment* first, JSScript* outerScript, jsbytecode* outerPC,
           uint32 outerArgc, SlotList* globalSlots)
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Try to find an unused peer fragment, or allocate a new one. */
    JS_ASSERT(first->first == first);
    TreeFragment* f = NULL;
    size_t count = 0;
    for (TreeFragment* peer = first; peer; peer = peer->peer, ++count) {
        if (!peer->code())
            f = peer;
    }
    if (!f)
        f = AddNewPeerToPeerList(tm, first);
    JS_ASSERT(f->root == f);

    /* Disable speculation if we are starting to accumulate a lot of trees. */
    bool speculate = count < MAXPEERS-1;

    /* save a local copy for use after JIT flush */
    const void* localRootIP = f->root->ip;

    /* Make sure the global type map didn't change on us. */
    if (!CheckGlobalObjectShape(cx, tm, f->globalObj)) {
        Backoff(cx, (jsbytecode*) localRootIP);
        return false;
    }

    AUDIT(recorderStarted);

    if (tm->outOfMemory() || OverfullJITCache(tm)) {
        Backoff(cx, (jsbytecode*) f->root->ip);
        ResetJIT(cx, FR_OOM);
        debug_only_print0(LC_TMTracer,
                          "Out of memory recording new tree, flushing cache.\n");
        return false;
    }

    JS_ASSERT(!f->code());

    f->initialize(cx, globalSlots, speculate);

#ifdef DEBUG
    AssertTreeIsUnique(tm, f);
#endif
#ifdef JS_JIT_SPEW
    debug_only_printf(LC_TMTreeVis, "TREEVIS CREATETREE ROOT=%p PC=%p FILE=\"%s\" LINE=%d OFFS=%d",
                      (void*)f, f->ip, f->treeFileName, f->treeLineNumber,
                      FramePCOffset(cx, cx->fp()));
    debug_only_print0(LC_TMTreeVis, " STACK=\"");
    for (unsigned i = 0; i < f->nStackTypes; i++)
        debug_only_printf(LC_TMTreeVis, "%c", TypeToChar(f->typeMap[i]));
    debug_only_print0(LC_TMTreeVis, "\" GLOBALS=\"");
    for (unsigned i = 0; i < f->nGlobalTypes(); i++)
        debug_only_printf(LC_TMTreeVis, "%c", TypeToChar(f->typeMap[f->nStackTypes + i]));
    debug_only_print0(LC_TMTreeVis, "\"\n");
#endif

    /* Recording primary trace. */
    return TraceRecorder::startRecorder(cx, NULL, f, f->nStackTypes,
                                        f->globalSlots->length(),
                                        f->typeMap.data(), NULL,
                                        outerScript, outerPC, outerArgc, speculate);
}

static JS_REQUIRES_STACK TypeConsensus
FindLoopEdgeTarget(JSContext* cx, VMSideExit* exit, TreeFragment** peerp)
{
    TreeFragment* from = exit->root();

    JS_ASSERT(from->code());
    Oracle* oracle = JS_TRACE_MONITOR(cx).oracle;

    TypeMap typeMap(NULL);
    FullMapFromExit(typeMap, exit);
    JS_ASSERT(typeMap.length() - exit->numStackSlots == from->nGlobalTypes());

    /* Mark all double slots as undemotable */
    uint16* gslots = from->globalSlots->data();
    for (unsigned i = 0; i < typeMap.length(); i++) {
        if (typeMap[i] == JSVAL_TYPE_DOUBLE) {
            if (i < from->nStackTypes)
                oracle->markStackSlotUndemotable(cx, i, from->ip);
            else if (i >= exit->numStackSlots)
                oracle->markGlobalSlotUndemotable(cx, gslots[i - exit->numStackSlots]);
        }
    }

    JS_ASSERT(exit->exitType == UNSTABLE_LOOP_EXIT);

    TreeFragment* firstPeer = from->first;

    for (TreeFragment* peer = firstPeer; peer; peer = peer->peer) {
        if (!peer->code())
            continue;
        JS_ASSERT(peer->argc == from->argc);
        JS_ASSERT(exit->numStackSlots == peer->nStackTypes);
        TypeConsensus consensus = TypeMapLinkability(cx, typeMap, peer);
        if (consensus == TypeConsensus_Okay || consensus == TypeConsensus_Undemotes) {
            *peerp = peer;
            return consensus;
        }
    }

    return TypeConsensus_Bad;
}

static JS_REQUIRES_STACK bool
AttemptToStabilizeTree(JSContext* cx, JSObject* globalObj, VMSideExit* exit,
                       JSScript* outerScript, jsbytecode* outerPC, uint32 outerArgc)
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return false;
    }

    TreeFragment* from = exit->root();

    TreeFragment* peer = NULL;
    TypeConsensus consensus = FindLoopEdgeTarget(cx, exit, &peer);
    if (consensus == TypeConsensus_Okay) {
        JS_ASSERT(from->globalSlots == peer->globalSlots);
        JS_ASSERT_IF(exit->exitType == UNSTABLE_LOOP_EXIT,
                     from->nStackTypes == peer->nStackTypes);
        JS_ASSERT(exit->numStackSlots == peer->nStackTypes);
        /* Patch this exit to its peer */
        JoinPeers(tm->assembler, exit, peer);
        /*
         * Update peer global types. The |from| fragment should already be updated because it on
         * the execution path, and somehow connected to the entry trace.
         */
        if (peer->nGlobalTypes() < peer->globalSlots->length())
            SpecializeTreesToMissingGlobals(cx, globalObj, peer);
        JS_ASSERT(from->nGlobalTypes() == from->globalSlots->length());
        /* This exit is no longer unstable, so remove it. */
        if (exit->exitType == UNSTABLE_LOOP_EXIT)
            from->removeUnstableExit(exit);
        debug_only_stmt(DumpPeerStability(tm, peer->ip, globalObj, from->globalShape, from->argc);)
        return false;
    } else if (consensus == TypeConsensus_Undemotes) {
        /* The original tree is unconnectable, so trash it. */
        TrashTree(peer);
        return false;
    }

    SlotList *globalSlots = from->globalSlots;

    JS_ASSERT(from == from->root);

    /* If this tree has been blacklisted, don't try to record a new one. */
    if (*(jsbytecode*)from->ip == JSOP_NOTRACE)
        return false;

    return RecordTree(cx, from->first, outerScript, outerPC, outerArgc, globalSlots);
}

static JS_REQUIRES_STACK VMFragment*
CreateBranchFragment(JSContext* cx, TreeFragment* root, VMSideExit* anchor)
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    verbose_only(
    uint32_t profFragID = (LogController.lcbits & LC_FragProfile)
                          ? (++(tm->lastFragID)) : 0;
    )

    VMFragment* f = new (*tm->dataAlloc) VMFragment(cx->regs->pc verbose_only(, profFragID));

    debug_only_printf(LC_TMTreeVis, "TREEVIS CREATEBRANCH ROOT=%p FRAG=%p PC=%p FILE=\"%s\""
                      " LINE=%d ANCHOR=%p OFFS=%d\n",
                      (void*)root, (void*)f, (void*)cx->regs->pc, cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()), (void*)anchor,
                      FramePCOffset(cx, cx->fp()));
    verbose_only( tm->branches = new (*tm->dataAlloc) Seq<Fragment*>(f, tm->branches); )

    f->root = root;
    if (anchor)
        anchor->target = f;
    return f;
}

static JS_REQUIRES_STACK bool
AttemptToExtendTree(JSContext* cx, VMSideExit* anchor, VMSideExit* exitedFrom,
                    JSScript *outerScript, jsbytecode* outerPC
#ifdef MOZ_TRACEVIS
    , TraceVisStateObj* tvso = NULL
#endif
    )
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(!tm->recorder);

    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
#ifdef MOZ_TRACEVIS
        if (tvso) tvso->r = R_FAIL_EXTEND_FLUSH;
#endif
        return false;
    }

    TreeFragment* f = anchor->root();
    JS_ASSERT(f->code());

    /*
     * Don't grow trees above a certain size to avoid code explosion due to
     * tail duplication.
     */
    if (f->branchCount >= MAX_BRANCHES) {
#ifdef JS_METHODJIT
        if (cx->methodJitEnabled && cx->profilingEnabled)
            Blacklist((jsbytecode *)f->ip);
#endif
#ifdef MOZ_TRACEVIS
        if (tvso) tvso->r = R_FAIL_EXTEND_MAX_BRANCHES;
#endif
        return false;
    }

    VMFragment* c = (VMFragment*)anchor->target;
    if (!c) {
        c = CreateBranchFragment(cx, f, anchor);
    } else {
        /*
         * If we are recycling a fragment, it might have a different ip so reset it
         * here. This can happen when attaching a branch to a NESTED_EXIT, which
         * might extend along separate paths (i.e. after the loop edge, and after a
         * return statement).
         */
        c->ip = cx->regs->pc;
        JS_ASSERT(c->root == f);
    }

    debug_only_printf(LC_TMTracer,
                      "trying to attach another branch to the tree (hits = %d)\n", c->hits());

    int32_t& hits = c->hits();
    int32_t maxHits = HOTEXIT + MAXEXIT;
    if (anchor->exitType == CASE_EXIT)
        maxHits *= anchor->switchInfo->count;
    if (outerPC || (hits++ >= HOTEXIT && hits <= maxHits)) {
        /* start tracing secondary trace from this point */
        unsigned stackSlots;
        unsigned ngslots;
        JSValueType* typeMap;
        TypeMap fullMap(NULL);
        if (!exitedFrom) {
            /*
             * If we are coming straight from a simple side exit, just use that
             * exit's type map as starting point.
             */
            ngslots = anchor->numGlobalSlots;
            stackSlots = anchor->numStackSlots;
            typeMap = anchor->fullTypeMap();
        } else {
            /*
             * If we side-exited on a loop exit and continue on a nesting
             * guard, the nesting guard (anchor) has the type information for
             * everything below the current scope, and the actual guard we
             * exited from has the types for everything in the current scope
             * (and whatever it inlined). We have to merge those maps here.
             */
            VMSideExit* e1 = anchor;
            VMSideExit* e2 = exitedFrom;
            fullMap.add(e1->stackTypeMap(), e1->numStackSlotsBelowCurrentFrame);
            fullMap.add(e2->stackTypeMap(), e2->numStackSlots);
            stackSlots = fullMap.length();
            ngslots = BuildGlobalTypeMapFromInnerTree(fullMap, e2);
            JS_ASSERT(ngslots >= e1->numGlobalSlots); // inner tree must have all globals
            JS_ASSERT(ngslots == fullMap.length() - stackSlots);
            typeMap = fullMap.data();
        }
        JS_ASSERT(ngslots >= anchor->numGlobalSlots);
        bool rv = TraceRecorder::startRecorder(cx, anchor, c, stackSlots, ngslots, typeMap,
                                               exitedFrom, outerScript, outerPC, f->argc,
                                               hits < maxHits);
#ifdef MOZ_TRACEVIS
        if (!rv && tvso)
            tvso->r = R_FAIL_EXTEND_START;
#endif
        return rv;
    }
#ifdef MOZ_TRACEVIS
    if (tvso) tvso->r = R_FAIL_EXTEND_COLD;
#endif
    return false;
}

static JS_REQUIRES_STACK bool
ExecuteTree(JSContext* cx, TreeFragment* f, uintN& inlineCallCount,
            VMSideExit** innermostNestedGuardp, VMSideExit** lrp);

static inline MonitorResult
RecordingIfTrue(bool b)
{
    return b ? MONITOR_RECORDING : MONITOR_NOT_RECORDING;
}

/*
 * A postcondition of recordLoopEdge is that if recordLoopEdge does not return
 * MONITOR_RECORDING, the recording has been aborted.
 */
JS_REQUIRES_STACK MonitorResult
TraceRecorder::recordLoopEdge(JSContext* cx, TraceRecorder* r, uintN& inlineCallCount)
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Process needFlush and deep abort requests. */
    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return MONITOR_NOT_RECORDING;
    }

    JS_ASSERT(r->fragment && !r->fragment->lastIns);
    TreeFragment* root = r->fragment->root;
    TreeFragment* first = LookupOrAddLoop(tm, cx->regs->pc, root->globalObj,
                                          root->globalShape, entryFrameArgc(cx));

    /*
     * Make sure the shape of the global object still matches (this might flush
     * the JIT cache).
     */
    JSObject* globalObj = cx->fp()->scopeChain().getGlobal();
    uint32 globalShape = -1;
    SlotList* globalSlots = NULL;
    if (!CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots)) {
        JS_ASSERT(!tm->recorder);
        return MONITOR_NOT_RECORDING;
    }

    debug_only_printf(LC_TMTracer,
                      "Looking for type-compatible peer (%s:%d@%d)\n",
                      cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()));

    // Find a matching inner tree. If none can be found, compile one.
    TreeFragment* f = r->findNestedCompatiblePeer(first);
    if (!f || !f->code()) {
        AUDIT(noCompatInnerTrees);

        TreeFragment* outerFragment = root;
        JSScript* outerScript = outerFragment->script;
        jsbytecode* outerPC = (jsbytecode*) outerFragment->ip;
        uint32 outerArgc = outerFragment->argc;
        JS_ASSERT(entryFrameArgc(cx) == first->argc);

        if (AbortRecording(cx, "No compatible inner tree") == JIT_RESET)
            return MONITOR_NOT_RECORDING;

        return RecordingIfTrue(RecordTree(cx, first, outerScript, outerPC, outerArgc, globalSlots));
    }

    AbortableRecordingStatus status = r->attemptTreeCall(f, inlineCallCount);
    if (status == ARECORD_CONTINUE)
        return MONITOR_RECORDING;
    if (status == ARECORD_ERROR) {
        if (TRACE_RECORDER(cx))
            AbortRecording(cx, "Error returned while recording loop edge");
        return MONITOR_ERROR;
    }
    JS_ASSERT(status == ARECORD_ABORTED && !tm->recorder);
    return MONITOR_NOT_RECORDING;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::attemptTreeCall(TreeFragment* f, uintN& inlineCallCount)
{
    adjustCallerTypes(f);
    prepareTreeCall(f);

#ifdef DEBUG
    uintN oldInlineCallCount = inlineCallCount;
#endif

    JSContext *localCx = cx;

    // Refresh the import type map so the tracker can reimport values after the
    // call with their correct types. The inner tree must not change the type of
    // any variable in a frame above the current one (i.e., upvars).
    //
    // Note that DetermineTypesVisitor may call determineSlotType, which may
    // read from the (current, stale) import type map, but this is safe here.
    // The reason is that determineSlotType will read the import type map only
    // if there is not a tracker instruction for that value, which means that
    // value has not been written yet, so that type map entry is up to date.
    importTypeMap.setLength(NativeStackSlots(cx, callDepth));
    DetermineTypesVisitor visitor(*this, importTypeMap.data());
    VisitStackSlots(visitor, cx, callDepth);

    VMSideExit* innermostNestedGuard = NULL;
    VMSideExit* lr;
    bool ok = ExecuteTree(cx, f, inlineCallCount, &innermostNestedGuard, &lr);

    /*
     * If ExecuteTree reentered the interpreter, it may have killed |this|
     * and/or caused an error, which must be propagated.
     */
    JS_ASSERT_IF(TRACE_RECORDER(localCx), TRACE_RECORDER(localCx) == this);
    if (!ok)
        return ARECORD_ERROR;
    if (!TRACE_RECORDER(localCx))
        return ARECORD_ABORTED;

    if (!lr) {
        AbortRecording(cx, "Couldn't call inner tree");
        return ARECORD_ABORTED;
    }

    TreeFragment* outerFragment = tree;
    JSScript* outerScript = outerFragment->script;
    jsbytecode* outerPC = (jsbytecode*) outerFragment->ip;
    switch (lr->exitType) {
      case LOOP_EXIT:
        /* If the inner tree exited on an unknown loop exit, grow the tree around it. */
        if (innermostNestedGuard) {
            if (AbortRecording(cx, "Inner tree took different side exit, abort current "
                                   "recording and grow nesting tree") == JIT_RESET) {
                return ARECORD_ABORTED;
            }
            return AttemptToExtendTree(localCx, innermostNestedGuard, lr, outerScript, outerPC)
                   ? ARECORD_CONTINUE
                   : ARECORD_ABORTED;
        }

        JS_ASSERT(oldInlineCallCount == inlineCallCount);

        /* Emit a call to the inner tree and continue recording the outer tree trace. */
        emitTreeCall(f, lr);
        return ARECORD_CONTINUE;

      case UNSTABLE_LOOP_EXIT:
      {
        /* Abort recording so the inner loop can become type stable. */
        JSObject* _globalObj = globalObj;
        if (AbortRecording(cx, "Inner tree is trying to stabilize, "
                               "abort outer recording") == JIT_RESET) {
            return ARECORD_ABORTED;
        }
        return AttemptToStabilizeTree(localCx, _globalObj, lr, outerScript, outerPC,
                                      outerFragment->argc)
               ? ARECORD_CONTINUE
               : ARECORD_ABORTED;
      }

      case MUL_ZERO_EXIT:
      case OVERFLOW_EXIT:
        if (lr->exitType == MUL_ZERO_EXIT)
            traceMonitor->oracle->markInstructionSlowZeroTest(cx->regs->pc);
        else
            traceMonitor->oracle->markInstructionUndemotable(cx->regs->pc);
        /* FALL THROUGH */
      case BRANCH_EXIT:
      case CASE_EXIT:
        /* Abort recording the outer tree, extend the inner tree. */
        if (AbortRecording(cx, "Inner tree is trying to grow, "
                               "abort outer recording") == JIT_RESET) {
            return ARECORD_ABORTED;
        }
        return AttemptToExtendTree(localCx, lr, NULL, outerScript, outerPC)
               ? ARECORD_CONTINUE
               : ARECORD_ABORTED;

      case NESTED_EXIT:
        JS_NOT_REACHED("NESTED_EXIT should be replaced by innermost side exit");
      default:
        debug_only_printf(LC_TMTracer, "exit_type=%s\n", getExitName(lr->exitType));
        AbortRecording(cx, "Inner tree not suitable for calling");
        return ARECORD_ABORTED;
    }
}

static inline bool
IsEntryTypeCompatible(const Value &v, JSValueType type)
{
    bool ok;

    JS_ASSERT(type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET);
    JS_ASSERT(type != JSVAL_TYPE_OBJECT);   /* JSVAL_TYPE_OBJECT does not belong in a type map */

    if (v.isInt32()) {
        ok = (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_DOUBLE);

    } else if (v.isDouble()) {
        int32_t _;
        ok = (type == JSVAL_TYPE_DOUBLE) || 
             (type == JSVAL_TYPE_INT32 && JSDOUBLE_IS_INT32(v.toDouble(), &_));

    } else if (v.isObject()) {
        ok = v.toObject().isFunction()
           ? type == JSVAL_TYPE_FUNOBJ
           : type == JSVAL_TYPE_NONFUNOBJ;

    } else {
        ok = v.extractNonDoubleObjectTraceType() == type;
    }
#ifdef DEBUG
    char ttag = TypeToChar(type);
    char vtag = ValueToTypeChar(v);
    debug_only_printf(LC_TMTracer, "%c/%c ", vtag, ttag);
    if (!ok)
        debug_only_printf(LC_TMTracer, "%s", "(incompatible types)");
#endif
    return ok;
}

static inline bool
IsFrameObjPtrTypeCompatible(void *p, JSStackFrame *fp, JSValueType type)
{
    if (p == fp->addressOfScopeChain())
        return type == JSVAL_TYPE_NONFUNOBJ;
    JS_ASSERT(p == fp->addressOfArgs());
    JS_ASSERT(type == JSVAL_TYPE_NONFUNOBJ || type == JSVAL_TYPE_NULL);
    return fp->hasArgsObj() == (type == JSVAL_TYPE_NONFUNOBJ);
}

class TypeCompatibilityVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSContext *mCx;
    Oracle *mOracle;
    JSValueType *mTypeMap;
    unsigned mStackSlotNum;
    bool mOk;
public:
    TypeCompatibilityVisitor (TraceRecorder &recorder,
                              JSValueType *typeMap) :
        mRecorder(recorder),
        mCx(mRecorder.cx),
        mOracle(JS_TRACE_MONITOR(mCx).oracle),
        mTypeMap(typeMap),
        mStackSlotNum(0),
        mOk(true)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
        debug_only_printf(LC_TMTracer, "global%d=", n);
        if (!IsEntryTypeCompatible(*vp, *mTypeMap)) {
            mOk = false;
        } else if (!IsPromoteInt(mRecorder.get(vp)) && *mTypeMap == JSVAL_TYPE_INT32) {
            mOracle->markGlobalSlotUndemotable(mCx, slot);
            mOk = false;
        } else if (vp->isInt32() && *mTypeMap == JSVAL_TYPE_DOUBLE) {
            mOracle->markGlobalSlotUndemotable(mCx, slot);
        }
        mTypeMap++;
    }

    /*
     * For the below two methods, one may be inclined to 'return false' early
     * when mOk is set to 'false'. Don't do that. It is very important to run
     * through the whole list to let all mis-matching slots get marked
     * undemotable in the oracle.
     */

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            debug_only_printf(LC_TMTracer, "%s%u=", stackSlotKind(), unsigned(i));
            if (!IsEntryTypeCompatible(*vp, *mTypeMap)) {
                mOk = false;
            } else if (!IsPromoteInt(mRecorder.get(vp)) && *mTypeMap == JSVAL_TYPE_INT32) {
                mOracle->markStackSlotUndemotable(mCx, mStackSlotNum);
                mOk = false;
            } else if (vp->isInt32() && *mTypeMap == JSVAL_TYPE_DOUBLE) {
                mOracle->markStackSlotUndemotable(mCx, mStackSlotNum);
            }
            vp++;
            mTypeMap++;
            mStackSlotNum++;
        }
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame* fp) {
        debug_only_printf(LC_TMTracer, "%s%u=", stackSlotKind(), 0);
        if (!IsFrameObjPtrTypeCompatible(p, fp, *mTypeMap))
            mOk = false;
        mTypeMap++;
        mStackSlotNum++;
        return true;
    }

    bool isOk() {
        return mOk;
    }
};

JS_REQUIRES_STACK TreeFragment*
TraceRecorder::findNestedCompatiblePeer(TreeFragment* f)
{
    TraceMonitor* tm;

    tm = &JS_TRACE_MONITOR(cx);
    unsigned int ngslots = tree->globalSlots->length();

    for (; f != NULL; f = f->peer) {
        if (!f->code())
            continue;

        debug_only_printf(LC_TMTracer, "checking nested types %p: ", (void*)f);

        if (ngslots > f->nGlobalTypes())
            SpecializeTreesToMissingGlobals(cx, globalObj, f);

        /*
         * Determine whether the typemap of the inner tree matches the outer
         * tree's current state. If the inner tree expects an integer, but the
         * outer tree doesn't guarantee an integer for that slot, we mark the
         * slot undemotable and mismatch here. This will force a new tree to be
         * compiled that accepts a double for the slot. If the inner tree
         * expects a double, but the outer tree has an integer, we can proceed,
         * but we mark the location undemotable.
         */
        TypeCompatibilityVisitor visitor(*this, f->typeMap.data());
        VisitSlots(visitor, cx, 0, *tree->globalSlots);

        debug_only_printf(LC_TMTracer, " %s\n", visitor.isOk() ? "match" : "");
        if (visitor.isOk())
            return f;
    }

    return NULL;
}

class CheckEntryTypeVisitor : public SlotVisitorBase
{
    bool mOk;
    JSValueType *mTypeMap;
public:
    CheckEntryTypeVisitor(JSValueType *typeMap) :
        mOk(true),
        mTypeMap(typeMap)
    {}

    JS_ALWAYS_INLINE void checkSlot(const Value &v, char const *name, int i) {
        debug_only_printf(LC_TMTracer, "%s%d=", name, i);
        JS_ASSERT(*(uint8_t*)mTypeMap != 0xCD);
        mOk = IsEntryTypeCompatible(v, *mTypeMap++);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(Value *vp, unsigned n, unsigned slot) {
        if (mOk)
            checkSlot(*vp, "global", n);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            if (!mOk)
                break;
            checkSlot(*vp++, stackSlotKind(), i);
        }
        return mOk;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(void* p, JSStackFrame *fp) {
        debug_only_printf(LC_TMTracer, "%s%d=", stackSlotKind(), 0);
        JS_ASSERT(*(uint8_t*)mTypeMap != 0xCD);
        return mOk = IsFrameObjPtrTypeCompatible(p, fp, *mTypeMap++);
    }

    bool isOk() {
        return mOk;
    }
};

/**
 * Check if types are usable for trace execution.
 *
 * @param cx            Context.
 * @param f             Tree of peer we're testing.
 * @return              True if compatible (with or without demotions), false otherwise.
 */
static JS_REQUIRES_STACK bool
CheckEntryTypes(JSContext* cx, JSObject* globalObj, TreeFragment* f)
{
    unsigned int ngslots = f->globalSlots->length();

    JS_ASSERT(f->nStackTypes == NativeStackSlots(cx, 0));

    if (ngslots > f->nGlobalTypes())
        SpecializeTreesToMissingGlobals(cx, globalObj, f);

    JS_ASSERT(f->typeMap.length() == NativeStackSlots(cx, 0) + ngslots);
    JS_ASSERT(f->typeMap.length() == f->nStackTypes + ngslots);
    JS_ASSERT(f->nGlobalTypes() == ngslots);

    CheckEntryTypeVisitor visitor(f->typeMap.data());
    VisitSlots(visitor, cx, 0, *f->globalSlots);

    debug_only_print0(LC_TMTracer, "\n");
    return visitor.isOk();
}

/**
 * Find an acceptable entry tree given a PC.
 *
 * @param cx            Context.
 * @param globalObj     Global object.
 * @param f             First peer fragment.
 * @param nodemote      If true, will try to find a peer that does not require demotion.
 * @out   count         Number of fragments consulted.
 */
static JS_REQUIRES_STACK TreeFragment*
FindVMCompatiblePeer(JSContext* cx, JSObject* globalObj, TreeFragment* f, uintN& count)
{
    count = 0;
    for (; f != NULL; f = f->peer) {
        if (!f->code())
            continue;
        debug_only_printf(LC_TMTracer,
                          "checking vm types %p (ip: %p): ", (void*)f, f->ip);
        if (CheckEntryTypes(cx, globalObj, f))
            return f;
        ++count;
    }
    return NULL;
}

/*
 * For the native stacks and global frame, reuse the storage in |tm->storage|.
 * This reuse depends on the invariant that only one trace uses |tm->storage| at
 * a time. This is subtley correct in lieu of deep bail; see comment for
 * |deepBailSp| in DeepBail.
 */
JS_ALWAYS_INLINE
TracerState::TracerState(JSContext* cx, TraceMonitor* tm, TreeFragment* f,
                         uintN& inlineCallCount, VMSideExit** innermostNestedGuardp)
  : cx(cx),
    stackBase(tm->storage->stack()),
    sp(stackBase + f->nativeStackBase / sizeof(double)),
    eos(tm->storage->global()),
    callstackBase(tm->storage->callstack()),
    sor(callstackBase),
    rp(callstackBase),
    eor(callstackBase + JS_MIN(MAX_CALL_STACK_ENTRIES,
                               JS_MAX_INLINE_CALL_COUNT - inlineCallCount)),
    lastTreeExitGuard(NULL),
    lastTreeCallGuard(NULL),
    rpAtLastTreeCall(NULL),
    outermostTree(f),
    inlineCallCountp(&inlineCallCount),
    innermostNestedGuardp(innermostNestedGuardp),
#ifdef EXECUTE_TREE_TIMER
    startTime(rdtsc()),
#endif
    builtinStatus(0),
    nativeVp(NULL)
{
    JS_ASSERT(!tm->tracecx);
    tm->tracecx = cx;
    prev = cx->tracerState;
    cx->tracerState = this;

    JS_ASSERT(eos == stackBase + MAX_NATIVE_STACK_SLOTS);
    JS_ASSERT(sp < eos);

    /*
     * inlineCallCount has already been incremented, if being invoked from
     * EnterFrame. It is okay to have a 0-frame restriction since the JIT
     * might not need any frames.
     */
    JS_ASSERT(inlineCallCount <= JS_MAX_INLINE_CALL_COUNT);

#ifdef DEBUG
    /*
     * Cannot 0xCD-fill global frame since it may overwrite a bailed outer
     * ExecuteTree's 0xdeadbeefdeadbeef marker.
     */
    memset(tm->storage->stack(), 0xCD, MAX_NATIVE_STACK_SLOTS * sizeof(double));
    memset(tm->storage->callstack(), 0xCD, MAX_CALL_STACK_ENTRIES * sizeof(FrameInfo*));
#endif
}

JS_ALWAYS_INLINE
TracerState::~TracerState()
{
    JS_ASSERT(!nativeVp);

    cx->tracerState = prev;
    JS_TRACE_MONITOR(cx).tracecx = NULL;
}

/* Call |f|, return the exit taken. */
static JS_ALWAYS_INLINE VMSideExit*
ExecuteTrace(JSContext* cx, Fragment* f, TracerState& state)
{
    JS_ASSERT(!cx->bailExit);
    union { NIns *code; GuardRecord* (FASTCALL *func)(TracerState*); } u;
    u.code = f->code();
    GuardRecord* rec;
#if defined(JS_NO_FASTCALL) && defined(NANOJIT_IA32)
    SIMULATE_FASTCALL(rec, state, NULL, u.func);
#else
    rec = u.func(&state);
#endif
    JS_ASSERT(!cx->bailExit);
    return (VMSideExit*)rec->exit;
}

/* Check whether our assumptions about the incoming scope-chain are upheld. */
static JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
ScopeChainCheck(JSContext* cx, TreeFragment* f)
{
    JS_ASSERT(f->globalObj == cx->fp()->scopeChain().getGlobal());

    /*
     * The JIT records and expects to execute with two scope-chain
     * assumptions baked-in:
     *
     *   1. That the bottom of the scope chain is global, in the sense of
     *      JSCLASS_IS_GLOBAL.
     *
     *   2. That the scope chain between fp and the global is free of
     *      "unusual" native objects such as HTML forms or other funny
     *      things.
     *
     * #2 is checked here while following the scope-chain links, via
     * js_IsCacheableNonGlobalScope, which consults a whitelist of known
     * class types; once a global is found, it's checked for #1. Failing
     * either check causes an early return from execution.
     */
    JSObject* child = &cx->fp()->scopeChain();
    while (JSObject* parent = child->getParent()) {
        if (!js_IsCacheableNonGlobalScope(child)) {
            debug_only_print0(LC_TMTracer,"Blacklist: non-cacheable object on scope chain.\n");
            Blacklist((jsbytecode*) f->root->ip);
            return false;
        }
        child = parent;
    }
    JS_ASSERT(child == f->globalObj);

    if (!(f->globalObj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
        debug_only_print0(LC_TMTracer, "Blacklist: non-global at root of scope chain.\n");
        Blacklist((jsbytecode*) f->root->ip);
        return false;
    }

    return true;
}

static void
LeaveTree(TraceMonitor *tm, TracerState&, VMSideExit* lr);

/* Return false if the interpreter should goto error. */
static JS_REQUIRES_STACK bool
ExecuteTree(JSContext* cx, TreeFragment* f, uintN& inlineCallCount,
            VMSideExit** innermostNestedGuardp, VMSideExit **lrp)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_EXECUTE);
#endif
    JS_ASSERT(f->root == f && f->code());
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    if (!ScopeChainCheck(cx, f) || !cx->stack().ensureEnoughSpaceToEnterTrace() ||
        inlineCallCount + f->maxCallDepth > JS_MAX_INLINE_CALL_COUNT) {
        *lrp = NULL;
        return true;
    }

    /* Make sure the global object is sane. */
    JS_ASSERT(f->globalObj->numSlots() <= MAX_GLOBAL_SLOTS);
    JS_ASSERT(f->nGlobalTypes() == f->globalSlots->length());
    JS_ASSERT_IF(f->globalSlots->length() != 0,
                 f->globalObj->shape() == f->globalShape);

    /* Initialize trace state. */
    TracerState state(cx, tm, f, inlineCallCount, innermostNestedGuardp);
    double* stack = tm->storage->stack();
    double* global = tm->storage->global();
    JSObject* globalObj = f->globalObj;
    unsigned ngslots = f->globalSlots->length();
    uint16* gslots = f->globalSlots->data();

    BuildNativeFrame(cx, globalObj, 0 /* callDepth */, ngslots, gslots,
                     f->typeMap.data(), global, stack);

    AUDIT(traceTriggered);
    debug_only_printf(LC_TMTracer, "entering trace at %s:%u@%u, execs: %u code: %p\n",
                      cx->fp()->script()->filename,
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()),
           f->execs,
           f->code());

    debug_only_stmt(uint32 globalSlots = globalObj->numSlots();)
    debug_only_stmt(*(uint64*)&tm->storage->global()[globalSlots] = 0xdeadbeefdeadbeefLL;)

    /* Execute trace. */
    JS_THREAD_DATA(cx)->iterationCounter = 0;
    debug_only(int64 t0 = PRMJ_Now();)
#ifdef MOZ_TRACEVIS
    VMSideExit* lr = (TraceVisStateObj(cx, S_NATIVE), ExecuteTrace(cx, f, state));
#else
    VMSideExit* lr = ExecuteTrace(cx, f, state);
#endif
    debug_only(int64 t1 = PRMJ_Now();)

    JS_ASSERT(*(uint64*)&tm->storage->global()[globalSlots] == 0xdeadbeefdeadbeefLL);
    JS_ASSERT_IF(lr->exitType == LOOP_EXIT, !lr->calldepth);

    /* Restore interpreter state. */
    LeaveTree(tm, state, lr);

    *lrp = state.innermost;
    bool ok = !(state.builtinStatus & BUILTIN_ERROR);
    JS_ASSERT_IF(cx->throwing, !ok);

    size_t iters = JS_THREAD_DATA(cx)->iterationCounter;

    f->execs++;
    f->iters += iters;

#ifdef DEBUG
    JSStackFrame *fp = cx->fp();
    const char *prefix = "";
    if (iters == LOOP_COUNT_MAX)
        prefix = ">";
    debug_only_printf(LC_TMMinimal, "  [%.3f ms] Tree at line %u executed for %s%u iterations;"
                      " executed %u times; leave for %s at %s:%u (%s)\n",
                      double(t1-t0) / PRMJ_USEC_PER_MSEC,
                      f->treeLineNumber, prefix, (uintN)iters, f->execs,
                      getExitName(lr->exitType),
                      fp->script()->filename,
                      js_FramePCToLineNumber(cx, fp),
                      js_CodeName[fp->hasImacropc() ? *fp->imacropc() : *cx->regs->pc]);
#endif
    
#ifdef JS_METHODJIT
    if (cx->methodJitEnabled) {
        if (lr->exitType == LOOP_EXIT && f->iters < MIN_LOOP_ITERS
            && f->execs >= LOOP_CHECK_ITERS)
        {
            debug_only_printf(LC_TMMinimal, "  Blacklisting at line %u (executed only %d iters)\n",
                              f->treeLineNumber, f->iters);
            Blacklist((jsbytecode *)f->ip);
        }
    }
#endif
    return ok;
}

class Guardian {
    bool *flagp;
public:
    Guardian(bool *flagp) {
        this->flagp = flagp;
        JS_ASSERT(!*flagp);
        *flagp = true;
    }

    ~Guardian() {
        JS_ASSERT(*flagp);
        *flagp = false;
    }
};

static JS_FORCES_STACK void
LeaveTree(TraceMonitor *tm, TracerState& state, VMSideExit* lr)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JSContext* cx = state.cx;

    /* Temporary waive the soft GC quota to make sure LeaveTree() doesn't fail. */
    Guardian waiver(&JS_THREAD_DATA(cx)->waiveGCQuota);

    FrameInfo** callstack = state.callstackBase;
    double* stack = state.stackBase;

    /*
     * Except if we find that this is a nested bailout, the guard the call
     * returned is the one we have to use to adjust pc and sp.
     */
    VMSideExit* innermost = lr;

    /*
     * While executing a tree we do not update state.sp and state.rp even if
     * they grow. Instead, guards tell us by how much sp and rp should be
     * incremented in case of a side exit. When calling a nested tree, however,
     * we actively adjust sp and rp. If we have such frames from outer trees on
     * the stack, then rp will have been adjusted. Before we can process the
     * stack of the frames of the tree we directly exited from, we have to
     * first work our way through the outer frames and generate interpreter
     * frames for them. Once the call stack (rp) is empty, we can process the
     * final frames (which again are not directly visible and only the guard we
     * exited on will tells us about).
     */
    FrameInfo** rp = (FrameInfo**)state.rp;
    if (lr->exitType == NESTED_EXIT) {
        VMSideExit* nested = state.lastTreeCallGuard;
        if (!nested) {
            /*
             * If lastTreeCallGuard is not set in state, we only have a single
             * level of nesting in this exit, so lr itself is the innermost and
             * outermost nested guard, and hence we set nested to lr. The
             * calldepth of the innermost guard is not added to state.rp, so we
             * do it here manually. For a nesting depth greater than 1 the
             * call tree code already added the innermost guard's calldepth
             * to state.rpAtLastTreeCall.
             */
            nested = lr;
            rp += lr->calldepth;
        } else {
            /*
             * During unwinding state.rp gets overwritten at every step and we
             * restore it here to its state at the innermost nested guard. The
             * builtin already added the calldepth of that innermost guard to
             * rpAtLastTreeCall.
             */
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

    int32_t bs = state.builtinStatus;
    bool bailed = innermost->exitType == STATUS_EXIT && (bs & BUILTIN_BAILED);
    if (bailed) {
        /*
         * Deep-bail case.
         *
         * A _FAIL native already called LeaveTree. We already reconstructed
         * the interpreter stack, in pre-call state, with pc pointing to the
         * CALL/APPLY op, for correctness. Then we continued in native code.
         */
        if (!(bs & (BUILTIN_ERROR | BUILTIN_NO_FIXUP_NEEDED))) {
            /*
             * The builtin or native deep-bailed but finished successfully
             * (no exception or error).
             *
             * After it returned, the JIT code stored the results of the
             * builtin or native at the top of the native stack and then
             * immediately flunked the guard on state->builtinStatus.
             *
             * Now LeaveTree has been called again from the tail of
             * ExecuteTree. We are about to return to the interpreter. Adjust
             * the top stack frame to resume on the next op.
             */
            JSFrameRegs* regs = cx->regs;
            JSOp op = (JSOp) *regs->pc;
            JS_ASSERT(op == JSOP_CALL || op == JSOP_APPLY || op == JSOP_NEW ||
                      op == JSOP_GETPROP || op == JSOP_GETTHISPROP || op == JSOP_GETARGPROP ||
                      op == JSOP_GETLOCALPROP || op == JSOP_LENGTH ||
                      op == JSOP_GETELEM || op == JSOP_CALLELEM || op == JSOP_CALLPROP ||
                      op == JSOP_SETPROP || op == JSOP_SETNAME || op == JSOP_SETMETHOD ||
                      op == JSOP_SETELEM || op == JSOP_INITELEM || op == JSOP_ENUMELEM ||
                      op == JSOP_INSTANCEOF ||
                      op == JSOP_ITER || op == JSOP_MOREITER || op == JSOP_ENDITER ||
                      op == JSOP_FORARG || op == JSOP_FORLOCAL ||
                      op == JSOP_FORNAME || op == JSOP_FORPROP || op == JSOP_FORELEM ||
                      op == JSOP_DELPROP || op == JSOP_DELELEM);

            /*
             * JSOP_SETELEM can be coalesced with a JSOP_POP in the interpeter.
             * Since this doesn't re-enter the recorder, the post-state snapshot
             * is invalid. Fix it up here.
             */
            if (op == JSOP_SETELEM && JSOp(regs->pc[JSOP_SETELEM_LENGTH]) == JSOP_POP) {
                regs->sp -= js_CodeSpec[JSOP_SETELEM].nuses;
                regs->sp += js_CodeSpec[JSOP_SETELEM].ndefs;
                regs->pc += JSOP_SETELEM_LENGTH;
                op = JSOP_POP;
            }

            const JSCodeSpec& cs = js_CodeSpec[op];
            regs->sp -= (cs.format & JOF_INVOKE) ? GET_ARGC(regs->pc) + 2 : cs.nuses;
            regs->sp += cs.ndefs;
            regs->pc += cs.length;
            JS_ASSERT_IF(!cx->fp()->hasImacropc(),
                         cx->fp()->slots() + cx->fp()->numFixed() +
                         js_ReconstructStackDepth(cx, cx->fp()->script(), regs->pc) ==
                         regs->sp);

            /*
             * If there's a tree call around the point that we deep exited at,
             * then state.sp and state.rp were restored to their original
             * values before the tree call and sp might be less than deepBailSp,
             * which we sampled when we were told to deep bail.
             */
            JS_ASSERT(state.deepBailSp >= state.stackBase && state.sp <= state.deepBailSp);

            /*
             * As explained above, the JIT code stored a result value or values
             * on the native stack. Transfer them to the interpreter stack now.
             * (Some opcodes, like JSOP_CALLELEM, produce two values, hence the
             * loop.)
             */
            JSValueType* typeMap = innermost->stackTypeMap();
            for (int i = 1; i <= cs.ndefs; i++) {
                NativeToValue(cx,
                              regs->sp[-i],
                              typeMap[innermost->numStackSlots - i],
                              (jsdouble *) state.deepBailSp
                              + innermost->sp_adj / sizeof(jsdouble) - i);
            }
        }
        return;
    }

    while (callstack < rp) {
        FrameInfo* fi = *callstack;
        /* Peek at the callee native slot in the not-yet-synthesized prev frame. */
        JSObject* callee = *(JSObject**)&stack[fi->callerHeight];

        /*
         * Flush the slots for cx->fp() (which will become cx->fp()->prev after
         * SynthesizeFrame). Since a frame's arguments (including callee
         * and thisv) are part of the frame, we only want to flush up to the
         * next frame's arguments, so set cx->regs->sp to to not include said
         * arguments. The upcoming call to SynthesizeFrame will reset regs->sp
         * to its correct value.
         */
        cx->regs->sp = cx->fp()->slots() + (fi->spdist - (2 + fi->get_argc()));
        int slots = FlushNativeStackFrame(cx, 0 /* callDepth */, fi->get_typemap(), stack);

        /* Finish initializing cx->fp() and push a new cx->fp(). */
        SynthesizeFrame(cx, *fi, callee);
#ifdef DEBUG
        JSStackFrame* fp = cx->fp();
        debug_only_printf(LC_TMTracer,
                          "synthesized deep frame for %s:%u@%u, slots=%d, fi=%p\n",
                          fp->script()->filename,
                          js_FramePCToLineNumber(cx, fp),
                          FramePCOffset(cx, fp),
                          slots,
                          (void*)*callstack);
#endif
        /*
         * Keep track of the additional frames we put on the interpreter stack
         * and the native stack slots we consumed.
         */
        ++*state.inlineCallCountp;
        ++callstack;
        stack += slots;
    }

    /*
     * We already synthesized the frames around the innermost guard. Here we
     * just deal with additional frames inside the tree we are bailing out
     * from.
     */
    JS_ASSERT(rp == callstack);
    unsigned calldepth = innermost->calldepth;
    unsigned calleeOffset = 0;
    for (unsigned n = 0; n < calldepth; ++n) {
        /* Peek at the callee native slot in the not-yet-synthesized prev frame. */
        calleeOffset += callstack[n]->callerHeight;
        JSObject* callee = *(JSObject**)&stack[calleeOffset];

        /* Reconstruct the frame. */
        SynthesizeFrame(cx, *callstack[n], callee);
        ++*state.inlineCallCountp;
#ifdef DEBUG
        JSStackFrame* fp = cx->fp();
        debug_only_printf(LC_TMTracer,
                          "synthesized shallow frame for %s:%u@%u\n",
                          fp->script()->filename, js_FramePCToLineNumber(cx, fp),
                          FramePCOffset(cx, fp));
#endif
    }

    /*
     * Adjust sp and pc relative to the tree we exited from (not the tree we
     * entered into).  These are our final values for sp and pc since
     * SynthesizeFrame has already taken care of all frames in between.
     */
    JSStackFrame* const fp = cx->fp();

    /*
     * If we are not exiting from an inlined frame, the state->sp is spbase.
     * Otherwise spbase is whatever slots frames around us consume.
     */
    cx->regs->pc = innermost->pc;
    if (innermost->imacpc)
        fp->setImacropc(innermost->imacpc);
    else
        fp->clearImacropc();

    /*
     * Set cx->regs->regs for the top frame. Since the top frame does not have a
     * FrameInfo (a FrameInfo is only pushed for calls), we basically need to
     * compute the offset from fp->slots() to the top of the stack based on the
     * number of native slots allocated for this function.
     *
     * Duplicate native stack layout computation: see VisitFrameSlots header comment.
     */
    uintN slotOffset = innermost->numStackSlots - innermost->numStackSlotsBelowCurrentFrame;
    if (fp->isGlobalFrame()) {
        /* Global nfixed slots are not kept on the native stack, so add them back. */
        slotOffset += fp->globalScript()->nfixed;
    } else {
        /* A frame's native slots includes args and frame ptrs, so strip them off. */
        slotOffset -= NumSlotsBeforeFixed(fp);
    }
    cx->regs->sp = fp->slots() + slotOffset;

    /* Assert that we computed sp correctly. */
    JS_ASSERT_IF(!fp->hasImacropc(),
                 fp->slots() + fp->numFixed() +
                 js_ReconstructStackDepth(cx, fp->script(), cx->regs->pc) == cx->regs->sp);

#ifdef EXECUTE_TREE_TIMER
    uint64 cycles = rdtsc() - state.startTime;
#elif defined(JS_JIT_SPEW)
    uint64 cycles = 0;
#endif
    debug_only_printf(LC_TMTracer,
                      "leaving trace at %s:%u@%u, op=%s, lr=%p, exitType=%s, sp=%lld, "
                      "calldepth=%d, cycles=%llu\n",
                      fp->script()->filename,
                      js_FramePCToLineNumber(cx, fp),
                      FramePCOffset(cx, fp),
                      js_CodeName[fp->hasImacropc() ? *fp->imacropc() : *cx->regs->pc],
                      (void*)lr,
                      getExitName(lr->exitType),
                      (long long int)(cx->regs->sp - fp->base()),
                      calldepth,
                      (unsigned long long int)cycles);

#ifdef DEBUG
    int slots =
#endif
        FlushNativeStackFrame(cx, innermost->calldepth, innermost->stackTypeMap(), stack);
    JS_ASSERT(unsigned(slots) == innermost->numStackSlots);

    /*
     * If this trace is part of a tree, later branches might have added
     * additional globals for which we don't have any type information
     * available in the side exit. We merge in this information from the entry
     * type-map. See also the comment in the constructor of TraceRecorder
     * regarding why this is always safe to do.
     */
    TreeFragment* outermostTree = state.outermostTree;
    uint16* gslots = outermostTree->globalSlots->data();
    unsigned ngslots = outermostTree->globalSlots->length();
    JS_ASSERT(ngslots == outermostTree->nGlobalTypes());
    JSValueType* globalTypeMap;

    /* Are there enough globals? */
    TypeMap& typeMap = *tm->cachedTempTypeMap;
    typeMap.clear();
    if (innermost->numGlobalSlots == ngslots) {
        /* Yes. This is the ideal fast path. */
        globalTypeMap = innermost->globalTypeMap();
    } else {
        /*
         * No. Merge the typemap of the innermost entry and exit together. This
         * should always work because it is invalid for nested trees or linked
         * trees to have incompatible types. Thus, whenever a new global type
         * is lazily added into a tree, all dependent and linked trees are
         * immediately specialized (see bug 476653).
         */
        JS_ASSERT(innermost->root()->nGlobalTypes() == ngslots);
        JS_ASSERT(innermost->root()->nGlobalTypes() > innermost->numGlobalSlots);
        typeMap.ensure(ngslots);
#ifdef DEBUG
        unsigned check_ngslots =
#endif
        BuildGlobalTypeMapFromInnerTree(typeMap, innermost);
        JS_ASSERT(check_ngslots == ngslots);
        globalTypeMap = typeMap.data();
    }

    /* Write back interned globals. */
    JS_ASSERT(state.eos == state.stackBase + MAX_NATIVE_STACK_SLOTS);
    JSObject* globalObj = outermostTree->globalObj;
    FlushNativeGlobalFrame(cx, globalObj, state.eos, ngslots, gslots, globalTypeMap);

#ifdef JS_JIT_SPEW
    if (innermost->exitType != TIMEOUT_EXIT)
        AUDIT(sideExitIntoInterpreter);
    else
        AUDIT(timeoutIntoInterpreter);
#endif

    state.innermost = innermost;
}

JS_REQUIRES_STACK MonitorResult
RecordLoopEdge(JSContext* cx, uintN& inlineCallCount)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_MONITOR);
#endif

    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Is the recorder currently active? */
    if (tm->recorder) {
        jsbytecode* pc = cx->regs->pc;
        if (pc == tm->recorder->tree->ip) {
            tm->recorder->closeLoop();
        } else {
            MonitorResult r = TraceRecorder::recordLoopEdge(cx, tm->recorder, inlineCallCount);
            JS_ASSERT((r == MONITOR_RECORDING) == (TRACE_RECORDER(cx) != NULL));
            if (r == MONITOR_RECORDING || r == MONITOR_ERROR)
                return r;


            /*
             * recordLoopEdge will invoke an inner tree if we have a matching
             * one. If we arrive here, that tree didn't run to completion and
             * instead we mis-matched or the inner tree took a side exit other than
             * the loop exit. We are thus no longer guaranteed to be parked on the
             * same loop header RecordLoopEdge was called for. In fact, this
             * might not even be a loop header at all. Hence if the program counter
             * no longer hovers over the inner loop header, return to the
             * interpreter and do not attempt to trigger or record a new tree at
             * this location.
             */
            if (pc != cx->regs->pc) {
#ifdef MOZ_TRACEVIS
                tvso.r = R_INNER_SIDE_EXIT;
#endif
                return MONITOR_NOT_RECORDING;
            }
        }
    }
    JS_ASSERT(!tm->recorder);

    /*
     * Make sure the shape of the global object still matches (this might flush
     * the JIT cache).
     */
    JSObject* globalObj = cx->fp()->scopeChain().getGlobal();
    uint32 globalShape = -1;
    SlotList* globalSlots = NULL;

    if (!CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots)) {
        Backoff(cx, cx->regs->pc);
        return MONITOR_NOT_RECORDING;
    }

    /* Do not enter the JIT code with a pending operation callback. */
    if (JS_THREAD_DATA(cx)->interruptFlags) {
#ifdef MOZ_TRACEVIS
        tvso.r = R_CALLBACK_PENDING;
#endif
        return MONITOR_NOT_RECORDING;
    }

    jsbytecode* pc = cx->regs->pc;
    uint32 argc = entryFrameArgc(cx);

    TreeFragment* f = LookupOrAddLoop(tm, pc, globalObj, globalShape, argc);

    /*
     * If we have no code in the anchor and no peers, we definitively won't be
     * able to activate any trees, so start compiling.
     */
    if (!f->code() && !f->peer) {
    record:
        if (++f->hits() < HOTLOOP) {
#ifdef MOZ_TRACEVIS
            tvso.r = f->hits() < 1 ? R_BACKED_OFF : R_COLD;
#endif
            return MONITOR_NOT_RECORDING;
        }

        if (!ScopeChainCheck(cx, f)) {
#ifdef MOZ_TRACEVIS
            tvso.r = R_FAIL_SCOPE_CHAIN_CHECK;
#endif
            return MONITOR_NOT_RECORDING;
        }

        /*
         * We can give RecordTree the root peer. If that peer is already taken,
         * it will walk the peer list and find us a free slot or allocate a new
         * tree if needed.
         */
        bool rv = RecordTree(cx, f->first, NULL, 0, NULL, globalSlots);
#ifdef MOZ_TRACEVIS
        if (!rv)
            tvso.r = R_FAIL_RECORD_TREE;
#endif
        return RecordingIfTrue(rv);
    }

    debug_only_printf(LC_TMTracer,
                      "Looking for compat peer %d@%d, from %p (ip: %p)\n",
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()), (void*)f, f->ip);

    uintN count;
    TreeFragment* match = FindVMCompatiblePeer(cx, globalObj, f, count);
    if (!match) {
        if (count < MAXPEERS)
            goto record;

        /*
         * If we hit the max peers ceiling, don't try to lookup fragments all
         * the time. That's expensive. This must be a rather type-unstable loop.
         */
        debug_only_print0(LC_TMTracer, "Blacklisted: too many peer trees.\n");
        Blacklist((jsbytecode*) f->root->ip);
#ifdef MOZ_TRACEVIS
        tvso.r = R_MAX_PEERS;
#endif
        return MONITOR_NOT_RECORDING;
    }

    VMSideExit* lr = NULL;
    VMSideExit* innermostNestedGuard = NULL;

    if (!ExecuteTree(cx, match, inlineCallCount, &innermostNestedGuard, &lr))
        return MONITOR_ERROR;

    if (!lr) {
#ifdef MOZ_TRACEVIS
        tvso.r = R_FAIL_EXECUTE_TREE;
#endif
        return MONITOR_NOT_RECORDING;
    }

    /*
     * If we exit on a branch, or on a tree call guard, try to grow the inner
     * tree (in case of a branch exit), or the tree nested around the tree we
     * exited from (in case of the tree call guard).
     */
    bool rv;
    switch (lr->exitType) {
      case UNSTABLE_LOOP_EXIT:
        rv = AttemptToStabilizeTree(cx, globalObj, lr, NULL, 0, NULL);
#ifdef MOZ_TRACEVIS
        if (!rv)
            tvso.r = R_FAIL_STABILIZE;
#endif
        return RecordingIfTrue(rv);

      case MUL_ZERO_EXIT:
      case OVERFLOW_EXIT:
        if (lr->exitType == MUL_ZERO_EXIT)
            tm->oracle->markInstructionSlowZeroTest(cx->regs->pc);
        else
            tm->oracle->markInstructionUndemotable(cx->regs->pc);
        /* FALL THROUGH */
      case BRANCH_EXIT:
      case CASE_EXIT:
        rv = AttemptToExtendTree(cx, lr, NULL, NULL, NULL
#ifdef MOZ_TRACEVIS
                                                   , &tvso
#endif
                                 );
        return RecordingIfTrue(rv);

      case LOOP_EXIT:
        if (innermostNestedGuard) {
            rv = AttemptToExtendTree(cx, innermostNestedGuard, lr, NULL, NULL
#ifdef MOZ_TRACEVIS
                                                                       , &tvso
#endif
                                     );
            return RecordingIfTrue(rv);
        }
#ifdef MOZ_TRACEVIS
        tvso.r = R_NO_EXTEND_OUTER;
#endif
        return MONITOR_NOT_RECORDING;

#ifdef MOZ_TRACEVIS
      case MISMATCH_EXIT:
        tvso.r = R_MISMATCH_EXIT;
        return MONITOR_NOT_RECORDING;
      case OOM_EXIT:
        tvso.r = R_OOM_EXIT;
        return MONITOR_NOT_RECORDING;
      case TIMEOUT_EXIT:
        tvso.r = R_TIMEOUT_EXIT;
        return MONITOR_NOT_RECORDING;
      case DEEP_BAIL_EXIT:
        tvso.r = R_DEEP_BAIL_EXIT;
        return MONITOR_NOT_RECORDING;
      case STATUS_EXIT:
        tvso.r = R_STATUS_EXIT;
        return MONITOR_NOT_RECORDING;
#endif

      default:
        /*
         * No, this was an unusual exit (i.e. out of memory/GC), so just resume
         * interpretation.
         */
#ifdef MOZ_TRACEVIS
        tvso.r = R_OTHER_EXIT;
#endif
        return MONITOR_NOT_RECORDING;
    }
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::monitorRecording(JSOp op)
{
    TraceMonitor &localtm = JS_TRACE_MONITOR(cx);
    debug_only_stmt( JSContext *localcx = cx; )

    /* Process needFlush requests now. */
    if (localtm.needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return ARECORD_ABORTED;
    }
    JS_ASSERT(!fragment->lastIns);

    /*
     * Clear one-shot state used to communicate between record_JSOP_CALL and post-
     * opcode-case-guts record hook (record_NativeCallComplete).
     */
    pendingSpecializedNative = NULL;
    newobj_ins = NULL;

    /* Handle one-shot request from finishGetProp or INSTANCEOF to snapshot post-op state and guard. */
    if (pendingGuardCondition) {
        LIns* cond = pendingGuardCondition;
        bool expected = true;
        /* Put 'cond' in a form suitable for a guard/branch condition if it's not already. */
        ensureCond(&cond, &expected);
        guard(expected, cond, STATUS_EXIT);
        pendingGuardCondition = NULL;
    }

    /* Handle one-shot request to unbox the result of a property get. */
    if (pendingUnboxSlot) {
        LIns* val_ins = get(pendingUnboxSlot);
        /*
         * We need to know from where to unbox the value. Since pendingUnboxSlot
         * is only set in finishGetProp, we can depend on LIns* tracked for
         * pendingUnboxSlot to have this information.
         */
        LIns* unboxed_ins = unbox_value(*pendingUnboxSlot,
                                        AnyAddress(val_ins->oprnd1(), val_ins->disp()),
                                        snapshot(BRANCH_EXIT));
        set(pendingUnboxSlot, unboxed_ins);
        pendingUnboxSlot = 0;
    }

    debug_only_stmt(
        if (LogController.lcbits & LC_TMRecorder) {
            debug_only_print0(LC_TMRecorder, "\n");
            js_Disassemble1(cx, cx->fp()->script(), cx->regs->pc,
                            cx->fp()->hasImacropc()
                                ? 0 : cx->regs->pc - cx->fp()->script()->code,
                            !cx->fp()->hasImacropc(), stdout);
        }
    )

    /*
     * If op is not a break or a return from a loop, continue recording and
     * follow the trace. We check for imacro-calling bytecodes inside each
     * switch case to resolve the if (JSOP_IS_IMACOP(x)) conditions at compile
     * time.
     */

    AbortableRecordingStatus status;
#ifdef DEBUG
    bool wasInImacro = (cx->fp()->hasImacropc());
#endif
    switch (op) {
      default:
          AbortRecording(cx, "unsupported opcode");
          status = ARECORD_ERROR;
          break;
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)              \
      case op:                                                                \
        w.comment(#op);                                                       \
        status = this->record_##op();                                         \
        break;
# include "jsopcode.tbl"
# undef OPDEF
    }

    /* N.B. |this| may have been deleted. */

    if (!JSOP_IS_IMACOP(op)) {
        JS_ASSERT(status != ARECORD_IMACRO);
        JS_ASSERT_IF(!wasInImacro, !localcx->fp()->hasImacropc());
    }

    if (localtm.recorder) {
        JS_ASSERT(status != ARECORD_ABORTED);
        JS_ASSERT(localtm.recorder == this);

        /* |this| recorder completed, but a new one started; keep recording. */
        if (status == ARECORD_COMPLETED)
            return ARECORD_CONTINUE;

        /* Handle lazy aborts; propagate the 'error' status. */
        if (StatusAbortsRecorderIfActive(status)) {
            AbortRecording(cx, js_CodeName[op]);
            return status == ARECORD_ERROR ? ARECORD_ERROR : ARECORD_ABORTED;
        }

        if (outOfMemory() || OverfullJITCache(&localtm)) {
            ResetJIT(cx, FR_OOM);

            /*
             * If the status returned was ARECORD_IMACRO, then we just
             * changed cx->regs, we need to tell the interpreter to sync
             * its local variables.
             */
            return status == ARECORD_IMACRO ? ARECORD_IMACRO_ABORTED : ARECORD_ABORTED;
        }
    } else {
        JS_ASSERT(status == ARECORD_COMPLETED ||
                  status == ARECORD_ABORTED ||
                  status == ARECORD_ERROR);
    }
    return status;
}

JS_REQUIRES_STACK TraceRecorder::AbortResult
AbortRecording(JSContext* cx, const char* reason)
{
#ifdef DEBUG
    JS_ASSERT(TRACE_RECORDER(cx));
    return TRACE_RECORDER(cx)->finishAbort(reason);
#else
    return TRACE_RECORDER(cx)->finishAbort("[no reason]");
#endif
}

#if defined NANOJIT_IA32
static bool
CheckForSSE2()
{
    char *c = getenv("X86_FORCE_SSE2");
    if (c)
        return (!strcmp(c, "true") ||
                !strcmp(c, "1") ||
                !strcmp(c, "yes"));

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
extern "C" int js_arm_try_armv5_op();
extern "C" int js_arm_try_armv6_op();
extern "C" int js_arm_try_armv7_op();
extern "C" int js_arm_try_vfp_op();

static unsigned int
arm_check_arch()
{
    unsigned int arch = 4;
    __try {
        js_arm_try_armv5_op();
        arch = 5;
        js_arm_try_armv6_op();
        arch = 6;
        js_arm_try_armv7_op();
        arch = 7;
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
    }
    return arch;
}

static bool
arm_check_vfp()
{
#ifdef WINCE_WINDOWS_MOBILE
    return false;
#else
    bool ret = false;
    __try {
        js_arm_try_vfp_op();
        ret = true;
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        ret = false;
    }
    return ret;
#endif
}

#define HAVE_ENABLE_DISABLE_DEBUGGER_EXCEPTIONS 1

/* See "Suppressing Exception Notifications while Debugging", at
 * http://msdn.microsoft.com/en-us/library/ms924252.aspx
 */
static void
disable_debugger_exceptions()
{
    // 2 == TLSSLOT_KERNEL
    DWORD kctrl = (DWORD) TlsGetValue(2);
    // 0x12 = TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG
    kctrl |= 0x12;
    TlsSetValue(2, (LPVOID) kctrl);
}

static void
enable_debugger_exceptions()
{
    // 2 == TLSSLOT_KERNEL
    DWORD kctrl = (DWORD) TlsGetValue(2);
    // 0x12 = TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG
    kctrl &= ~0x12;
    TlsSetValue(2, (LPVOID) kctrl);
}

#elif defined(__GNUC__) && defined(AVMPLUS_LINUX)

// Assume ARMv4 by default.
static unsigned int arm_arch = 4;
static bool arm_has_vfp = false;
static bool arm_has_neon = false;
static bool arm_has_iwmmxt = false;
static bool arm_tests_initialized = false;

#ifdef ANDROID
// we're actually reading /proc/cpuinfo, but oh well
static void
arm_read_auxv()
{
  char buf[1024];
  char* pos;
  const char* ver_token = "CPU architecture: ";
  FILE* f = fopen("/proc/cpuinfo", "r");
  fread(buf, sizeof(char), 1024, f);
  fclose(f);
  pos = strstr(buf, ver_token);
  if (pos) {
    int ver = *(pos + strlen(ver_token)) - '0';
    arm_arch = ver;
  }
  arm_has_neon = strstr(buf, "neon") != NULL;
  arm_has_vfp = strstr(buf, "vfp") != NULL;
  arm_has_iwmmxt = strstr(buf, "iwmmxt") != NULL;
  arm_tests_initialized = true;
}

#else

static void
arm_read_auxv()
{
    int fd;
    Elf32_auxv_t aux;

    fd = open("/proc/self/auxv", O_RDONLY);
    if (fd > 0) {
        while (read(fd, &aux, sizeof(Elf32_auxv_t))) {
            if (aux.a_type == AT_HWCAP) {
                uint32_t hwcap = aux.a_un.a_val;
                if (getenv("ARM_FORCE_HWCAP"))
                    hwcap = strtoul(getenv("ARM_FORCE_HWCAP"), NULL, 0);
                else if (getenv("_SBOX_DIR"))
                    continue;  // Ignore the rest, if we're running in scratchbox
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
                else if (getenv("_SBOX_DIR"))
                    continue;  // Ignore the rest, if we're running in scratchbox
                // The platform string has the form "v[0-9][lb]". The "l" or "b" indicate little-
                // or big-endian variants and the digit indicates the version of the platform.
                // We can only accept ARMv4 and above, but allow anything up to ARMv9 for future
                // processors. Architectures newer than ARMv7 are assumed to be
                // backwards-compatible with ARMv7.
                if ((plat[0] == 'v') &&
                    (plat[1] >= '4') && (plat[1] <= '9') &&
                    ((plat[2] == 'l') || (plat[2] == 'b')))
                {
                    arm_arch = plat[1] - '0';
                }
            }
        }
        close (fd);

        // if we don't have 2.6.29, we have to do this hack; set
        // the env var to trust HWCAP.
        if (!getenv("ARM_TRUST_HWCAP") && (arm_arch >= 7))
            arm_has_neon = true;
    }

    arm_tests_initialized = true;
}

#endif

static unsigned int
arm_check_arch()
{
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_arch;
}

static bool
arm_check_vfp()
{
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_has_vfp;
}

#else
#warning Not sure how to check for architecture variant on your platform. Assuming ARMv4.
static unsigned int
arm_check_arch() { return 4; }
static bool
arm_check_vfp() { return false; }
#endif

#ifndef HAVE_ENABLE_DISABLE_DEBUGGER_EXCEPTIONS
static void
enable_debugger_exceptions() { }
static void
disable_debugger_exceptions() { }
#endif

#endif /* NANOJIT_ARM */

#define K *1024
#define M K K
#define G K M

void
SetMaxCodeCacheBytes(JSContext* cx, uint32 bytes)
{
    TraceMonitor* tm = &JS_THREAD_DATA(cx)->traceMonitor;
    JS_ASSERT(tm->codeAlloc && tm->dataAlloc && tm->traceAlloc);
    if (bytes > 1 G)
        bytes = 1 G;
    if (bytes < 128 K)
        bytes = 128 K;
    tm->maxCodeCacheBytes = bytes;
}

void
InitJIT(TraceMonitor *tm)
{
#if defined JS_JIT_SPEW
    tm->profAlloc = NULL;
    /* Set up debug logging. */
    if (!did_we_set_up_debug_logging) {
        InitJITLogController();
        did_we_set_up_debug_logging = true;
    }
    /* Set up fragprofiling, if required. */
    if (LogController.lcbits & LC_FragProfile) {
        tm->profAlloc = new VMAllocator();
        tm->profTab = new (*tm->profAlloc) FragStatsMap(*tm->profAlloc);
    }
    tm->lastFragID = 0;
#else
    PodZero(&LogController);
#endif

    if (!did_we_check_processor_features) {
#if defined NANOJIT_IA32
        avmplus::AvmCore::config.i386_use_cmov =
            avmplus::AvmCore::config.i386_sse2 = CheckForSSE2();
        avmplus::AvmCore::config.i386_fixed_esp = true;
#endif
#if defined NANOJIT_ARM

        disable_debugger_exceptions();

        bool            arm_vfp     = arm_check_vfp();
        unsigned int    arm_arch    = arm_check_arch();

        enable_debugger_exceptions();

        avmplus::AvmCore::config.arm_vfp        = arm_vfp;
        avmplus::AvmCore::config.soft_float     = !arm_vfp;
        avmplus::AvmCore::config.arm_arch       = arm_arch;

        // Sanity-check the configuration detection.
        //  * We don't understand architectures prior to ARMv4.
        JS_ASSERT(arm_arch >= 4);
#endif
        did_we_check_processor_features = true;
    }

    /* Set the default size for the code cache to 16MB. */
    tm->maxCodeCacheBytes = 16 M;

    tm->oracle = new Oracle();

    tm->profile = NULL;
    
    tm->recordAttempts = new RecordAttemptMap;
    if (!tm->recordAttempts->init(PC_HASH_COUNT))
        abort();

    tm->loopProfiles = new LoopProfileMap;
    if (!tm->loopProfiles->init(PC_HASH_COUNT))
        abort();

    tm->flushEpoch = 0;
    
    JS_ASSERT(!tm->dataAlloc && !tm->traceAlloc && !tm->codeAlloc);
    tm->dataAlloc = new VMAllocator();
    tm->traceAlloc = new VMAllocator();
    tm->tempAlloc = new VMAllocator();
    tm->reTempAlloc = new VMAllocator();
    tm->codeAlloc = new CodeAlloc();
    tm->frameCache = new FrameInfoCache(tm->dataAlloc);
    tm->storage = new TraceNativeStorage();
    tm->cachedTempTypeMap = new TypeMap(0);
    tm->flush();
    verbose_only( tm->branches = NULL; )

#if !defined XP_WIN
    debug_only(PodZero(&jitstats));
#endif

#ifdef JS_JIT_SPEW
    /* Architecture properties used by test cases. */
    jitstats.archIsIA32 = 0;
    jitstats.archIs64BIT = 0;
    jitstats.archIsARM = 0;
    jitstats.archIsSPARC = 0;
    jitstats.archIsPPC = 0;
#if defined NANOJIT_IA32
    jitstats.archIsIA32 = 1;
#endif
#if defined NANOJIT_64BIT
    jitstats.archIs64BIT = 1;
#endif
#if defined NANOJIT_ARM
    jitstats.archIsARM = 1;
#endif
#if defined NANOJIT_SPARC
    jitstats.archIsSPARC = 1;
#endif
#if defined NANOJIT_PPC
    jitstats.archIsPPC = 1;
#endif
#if defined NANOJIT_X64
    jitstats.archIsAMD64 = 1;
#endif
#endif
}

void
FinishJIT(TraceMonitor *tm)
{
    JS_ASSERT(!tm->recorder);

#ifdef JS_JIT_SPEW
    if (jitstats.recorderStarted) {
        char sep = ':';
        debug_only_print0(LC_TMStats, "recorder");
#define RECORDER_JITSTAT(_ident, _name)                             \
        debug_only_printf(LC_TMStats, "%c " _name "(%llu)", sep,    \
                          (unsigned long long int)jitstats._ident); \
        sep = ',';
#define JITSTAT(x) /* nothing */
#include "jitstats.tbl"
#undef JITSTAT
#undef RECORDER_JITSTAT
        debug_only_print0(LC_TMStats, "\n");

        sep = ':';
        debug_only_print0(LC_TMStats, "monitor");
#define MONITOR_JITSTAT(_ident, _name)                              \
        debug_only_printf(LC_TMStats, "%c " _name "(%llu)", sep,    \
                          (unsigned long long int)jitstats._ident); \
        sep = ',';
#define JITSTAT(x) /* nothing */
#include "jitstats.tbl"
#undef JITSTAT
#undef MONITOR_JITSTAT
        debug_only_print0(LC_TMStats, "\n");
    }
#endif

    delete tm->recordAttempts;
    delete tm->loopProfiles;
    delete tm->oracle;

#ifdef DEBUG
    // Recover profiling data from expiring Fragments, and display
    // final results.
    if (LogController.lcbits & LC_FragProfile) {
        for (Seq<Fragment*>* f = tm->branches; f; f = f->tail) {
            FragProfiling_FragFinalizer(f->head, tm);
        }
        for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
            for (TreeFragment *f = tm->vmfragments[i]; f; f = f->next) {
                JS_ASSERT(f->root == f);
                for (TreeFragment *p = f; p; p = p->peer)
                    FragProfiling_FragFinalizer(p, tm);
            }
        }
        REHashMap::Iter iter(*(tm->reFragments));
        while (iter.next()) {
            VMFragment* frag = (VMFragment*)iter.value();
            FragProfiling_FragFinalizer(frag, tm);
        }

        FragProfiling_showResults(tm);
        delete tm->profAlloc;

    } else {
        NanoAssert(!tm->profTab);
        NanoAssert(!tm->profAlloc);
    }
#endif

    PodArrayZero(tm->vmfragments);

    if (tm->frameCache) {
        delete tm->frameCache;
        tm->frameCache = NULL;
    }

    if (tm->codeAlloc) {
        delete tm->codeAlloc;
        tm->codeAlloc = NULL;
    }

    if (tm->dataAlloc) {
        delete tm->dataAlloc;
        tm->dataAlloc = NULL;
    }

    if (tm->traceAlloc) {
        delete tm->traceAlloc;
        tm->traceAlloc = NULL;
    }

    if (tm->tempAlloc) {
        delete tm->tempAlloc;
        tm->tempAlloc = NULL;
    }

    if (tm->reTempAlloc) {
        delete tm->reTempAlloc;
        tm->reTempAlloc = NULL;
    }

    if (tm->storage) {
        delete tm->storage;
        tm->storage = NULL;
    }

    delete tm->cachedTempTypeMap;
    tm->cachedTempTypeMap = NULL;
}

JS_REQUIRES_STACK void
PurgeScriptFragments(JSContext* cx, JSScript* script)
{
    debug_only_printf(LC_TMTracer,
                      "Purging fragments for JSScript %p.\n", (void*)script);

    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* A recorder script is being evaluated and can not be destroyed or GC-ed. */
    JS_ASSERT_IF(tm->recorder, 
                 JS_UPTRDIFF(tm->recorder->getTree()->ip, script->code) >= script->length);

    for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
        TreeFragment** fragp = &tm->vmfragments[i];
        while (TreeFragment* frag = *fragp) {
            if (JS_UPTRDIFF(frag->ip, script->code) < script->length) {
                /* This fragment is associated with the script. */
                debug_only_printf(LC_TMTracer,
                                  "Disconnecting TreeFragment %p "
                                  "with ip %p, in range [%p,%p).\n",
                                  (void*)frag, frag->ip, script->code,
                                  script->code + script->length);

                JS_ASSERT(frag->root == frag);
                *fragp = frag->next;
                do {
                    verbose_only( FragProfiling_FragFinalizer(frag, tm); )
                    TrashTree(frag);
                } while ((frag = frag->peer) != NULL);
                continue;
            }
            fragp = &frag->next;
        }
    }

    RecordAttemptMap &table = *tm->recordAttempts;
    for (RecordAttemptMap::Enum e(table); !e.empty(); e.popFront()) {
        if (JS_UPTRDIFF(e.front().key, script->code) < script->length)
            e.removeFront();
    }
}

bool
OverfullJITCache(TraceMonitor* tm)
{
    /*
     * You might imagine the outOfMemory flag on the allocator is sufficient
     * to model the notion of "running out of memory", but there are actually
     * two separate issues involved:
     *
     *  1. The process truly running out of memory: malloc() or mmap()
     *     failed.
     *
     *  2. The limit we put on the "intended size" of the tracemonkey code
     *     cache, in pages, has been exceeded.
     *
     * Condition 1 doesn't happen very often, but we're obliged to try to
     * safely shut down and signal the rest of spidermonkey when it
     * does. Condition 2 happens quite regularly.
     *
     * Presently, the code in this file doesn't check the outOfMemory condition
     * often enough, and frequently misuses the unchecked results of
     * lirbuffer insertions on the asssumption that it will notice the
     * outOfMemory flag "soon enough" when it returns to the monitorRecording
     * function. This turns out to be a false assumption if we use outOfMemory
     * to signal condition 2: we regularly provoke "passing our intended
     * size" and regularly fail to notice it in time to prevent writing
     * over the end of an artificially self-limited LIR buffer.
     *
     * To mitigate, though not completely solve, this problem, we're
     * modeling the two forms of memory exhaustion *separately* for the
     * time being: condition 1 is handled by the outOfMemory flag inside
     * nanojit, and condition 2 is being handled independently *here*. So
     * we construct our allocators to use all available memory they like,
     * and only report outOfMemory to us when there is literally no OS memory
     * left. Merely purging our cache when we hit our highwater mark is
     * handled by the (few) callers of this function.
     *
     */
    jsuint maxsz = tm->maxCodeCacheBytes;
    VMAllocator *dataAlloc = tm->dataAlloc;
    VMAllocator *traceAlloc = tm->traceAlloc;
    CodeAlloc *codeAlloc = tm->codeAlloc;

    return (codeAlloc->size() + dataAlloc->size() + traceAlloc->size() > maxsz);
}

JS_FORCES_STACK JS_FRIEND_API(void)
DeepBail(JSContext *cx)
{
    JS_ASSERT(JS_ON_TRACE(cx));

    /*
     * Exactly one context on the current thread is on trace. Find out which
     * one. (Most callers cannot guarantee that it's cx.)
     */
    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    JSContext *tracecx = tm->tracecx;

    /* It's a bug if a non-FAIL_STATUS builtin gets here. */
    JS_ASSERT(tracecx->bailExit);

    tm->tracecx = NULL;
    debug_only_print0(LC_TMTracer, "Deep bail.\n");
    LeaveTree(tm, *tracecx->tracerState, tracecx->bailExit);
    tracecx->bailExit = NULL;

    TracerState* state = tracecx->tracerState;
    state->builtinStatus |= BUILTIN_BAILED;

    /*
     * Between now and the LeaveTree in ExecuteTree, |tm->storage| may be reused
     * if another trace executes before the currently executing native returns.
     * However, all such traces will complete by the time the currently
     * executing native returns and the return value is written to the native
     * stack. After that point, no traces may execute until the LeaveTree in
     * ExecuteTree, hence the invariant is maintained that only one trace uses
     * |tm->storage| at a time.
     */
    state->deepBailSp = state->sp;
}

JS_REQUIRES_STACK Value&
TraceRecorder::argval(unsigned n) const
{
    JS_ASSERT(n < cx->fp()->numFormalArgs());
    return cx->fp()->formalArg(n);
}

JS_REQUIRES_STACK Value&
TraceRecorder::varval(unsigned n) const
{
    JS_ASSERT(n < cx->fp()->numSlots());
    return cx->fp()->slots()[n];
}

JS_REQUIRES_STACK Value&
TraceRecorder::stackval(int n) const
{
    return cx->regs->sp[n];
}

JS_REQUIRES_STACK void
TraceRecorder::updateAtoms()
{
    JSScript *script = cx->fp()->script();
    atoms = FrameAtomBase(cx, cx->fp());
    consts = cx->fp()->hasImacropc() || script->constOffset == 0
           ? 0 
           : script->consts()->vector;
    strictModeCode_ins = w.name(w.immi(script->strictModeCode), "strict");
}

JS_REQUIRES_STACK void
TraceRecorder::updateAtoms(JSScript *script)
{
    atoms = script->atomMap.vector;
    consts = script->constOffset == 0 ? 0 : script->consts()->vector;
    strictModeCode_ins = w.name(w.immi(script->strictModeCode), "strict");
}

/*
 * Generate LIR to compute the scope chain.
 */
JS_REQUIRES_STACK LIns*
TraceRecorder::scopeChain()
{
    return cx->fp()->isFunctionFrame()
           ? getFrameObjPtr(cx->fp()->addressOfScopeChain())
           : entryScopeChain();
}

/*
 * Generate LIR to compute the scope chain on entry to the trace. This is
 * generally useful only for getting to the global object, because only
 * the global object is guaranteed to be present.
 */
JS_REQUIRES_STACK LIns*
TraceRecorder::entryScopeChain() const
{
    return w.ldpStackFrameScopeChain(entryFrameIns());
}

/*
 * Generate LIR to compute the stack frame on entry to the trace.
 */
JS_REQUIRES_STACK LIns*
TraceRecorder::entryFrameIns() const
{
    return w.ldpFrameFp(w.ldpContextField(regs));
}

/*
 * Return the frame of a call object if that frame is part of the current
 * trace. |depthp| is an optional outparam: if it is non-null, it will be
 * filled in with the depth of the call object's frame relevant to cx->fp().
 */
JS_REQUIRES_STACK JSStackFrame*
TraceRecorder::frameIfInRange(JSObject* obj, unsigned* depthp) const
{
    JSStackFrame* ofp = (JSStackFrame*) obj->getPrivate();
    JSStackFrame* fp = cx->fp();
    for (unsigned depth = 0; depth <= callDepth; ++depth) {
        if (fp == ofp) {
            if (depthp)
                *depthp = depth;
            return ofp;
        }
        if (!(fp = fp->prev()))
            break;
    }
    return NULL;
}

JS_DEFINE_CALLINFO_4(extern, UINT32, GetClosureVar, CONTEXT, OBJECT, CVIPTR, DOUBLEPTR,
                     0, ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_4(extern, UINT32, GetClosureArg, CONTEXT, OBJECT, CVIPTR, DOUBLEPTR,
                     0, ACCSET_STORE_ANY)

/*
 * Search the scope chain for a property lookup operation at the current PC and
 * generate LIR to access the given property. Return RECORD_CONTINUE on success,
 * otherwise abort and return RECORD_STOP. There are 3 outparams:
 *
 *     vp           the address of the current property value
 *     ins          LIR instruction representing the property value on trace
 *     NameResult   describes how to look up name; see comment for NameResult in jstracer.h
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::scopeChainProp(JSObject* chainHead, Value*& vp, LIns*& ins, NameResult& nr)
{
    JS_ASSERT(chainHead == &cx->fp()->scopeChain());
    JS_ASSERT(chainHead != globalObj);

    TraceMonitor &localtm = *traceMonitor;

    JSAtom* atom = atoms[GET_INDEX(cx->regs->pc)];
    JSObject* obj2;
    JSProperty* prop;
    JSObject *obj = chainHead;
    if (!js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &obj2, &prop))
        RETURN_ERROR_A("error in js_FindProperty");

    /* js_FindProperty can reenter the interpreter and kill |this|. */
    if (!localtm.recorder)
        return ARECORD_ABORTED;

    if (!prop)
        RETURN_STOP_A("failed to find name in non-global scope chain");

    if (obj == globalObj) {
        // Even if the property is on the global object, we must guard against
        // the creation of properties that shadow the property in the middle
        // of the scope chain.
        LIns *head_ins;
        if (cx->fp()->isFunctionFrame()) {
            // Skip any Call object when inside a function. Any reference to a
            // Call name the compiler resolves statically and we do not need
            // to match shapes of the Call objects.
            chainHead = cx->fp()->callee().getParent();
            head_ins = w.ldpObjParent(get(&cx->fp()->calleeValue()));
        } else {
            head_ins = scopeChain();
        }
        LIns *obj_ins;
        CHECK_STATUS_A(traverseScopeChain(chainHead, head_ins, obj, obj_ins));

        if (obj2 != obj)
            RETURN_STOP_A("prototype property");

        Shape* shape = (Shape*) prop;
        if (!isValidSlot(obj, shape))
            return ARECORD_STOP;
        if (!lazilyImportGlobalSlot(shape->slot))
            RETURN_STOP_A("lazy import of global slot failed");
        vp = &obj->getSlotRef(shape->slot);
        ins = get(vp);
        nr.tracked = true;
        return ARECORD_CONTINUE;
    }

    if (obj == obj2 && obj->isCall()) {
        AbortableRecordingStatus status =
            InjectStatus(callProp(obj, prop, ATOM_TO_JSID(atom), vp, ins, nr));
        return status;
    }

    RETURN_STOP_A("fp->scopeChain is not global or active call object");
}

/*
 * Generate LIR to access a property of a Call object.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callProp(JSObject* obj, JSProperty* prop, jsid id, Value*& vp,
                        LIns*& ins, NameResult& nr)
{
    Shape *shape = (Shape*) prop;

    JSOp op = JSOp(*cx->regs->pc);
    uint32 setflags = (js_CodeSpec[op].format & (JOF_SET | JOF_INCDEC | JOF_FOR));
    if (setflags && !shape->writable())
        RETURN_STOP("writing to a read-only property");

    uintN slot = uint16(shape->shortid);

    vp = NULL;
    JSStackFrame* cfp = (JSStackFrame*) obj->getPrivate();
    if (cfp) {
        if (shape->getterOp() == js_GetCallArg) {
            JS_ASSERT(slot < cfp->numFormalArgs());
            vp = &cfp->formalArg(slot);
            nr.v = *vp;
        } else if (shape->getterOp() == js_GetCallVar ||
                   shape->getterOp() == js_GetCallVarChecked) {
            JS_ASSERT(slot < cfp->numSlots());
            vp = &cfp->slots()[slot];
            nr.v = *vp;
        } else {
            RETURN_STOP("dynamic property of Call object");
        }

        // Now assert that our use of shape->shortid was in fact kosher.
        JS_ASSERT(shape->hasShortID());

        if (frameIfInRange(obj)) {
            // At this point we are guaranteed to be looking at an active call oject
            // whose properties are stored in the corresponding JSStackFrame.
            ins = get(vp);
            nr.tracked = true;
            return RECORD_CONTINUE;
        }
    } else {
        // Call objects do not yet have shape->isMethod() properties, but they
        // should. See bug 514046, for which this code is future-proof. Remove
        // this comment when that bug is fixed (so, FIXME: 514046).
#ifdef DEBUG
        JSBool rv =
#endif
            js_GetPropertyHelper(cx, obj, shape->id,
                                 (op == JSOP_CALLNAME)
                                 ? JSGET_NO_METHOD_BARRIER
                                 : JSGET_METHOD_BARRIER,
                                 &nr.v);
        JS_ASSERT(rv);
    }

    LIns* obj_ins;
    JSObject* parent = cx->fp()->callee().getParent();
    LIns* parent_ins = w.ldpObjParent(get(&cx->fp()->calleeValue()));
    CHECK_STATUS(traverseScopeChain(parent, parent_ins, obj, obj_ins));

    if (!cfp) {
        // Because the parent guard in guardCallee ensures this Call object
        // will be the same object now and on trace, and because once a Call
        // object loses its frame it never regains one, on trace we will also
        // have a null private in the Call object. So all we need to do is
        // write the value to the Call object's slot.
        if (shape->getterOp() == js_GetCallArg) {
            JS_ASSERT(slot < ArgClosureTraits::slot_count(obj));
            slot += ArgClosureTraits::slot_offset(obj);
        } else if (shape->getterOp() == js_GetCallVar ||
                   shape->getterOp() == js_GetCallVarChecked) {
            JS_ASSERT(slot < VarClosureTraits::slot_count(obj));
            slot += VarClosureTraits::slot_offset(obj);
        } else {
            RETURN_STOP("dynamic property of Call object");
        }

        // Now assert that our use of shape->shortid was in fact kosher.
        JS_ASSERT(shape->hasShortID());

        ins = unbox_slot(obj, obj_ins, slot, snapshot(BRANCH_EXIT));
    } else {
        ClosureVarInfo* cv = new (traceAlloc()) ClosureVarInfo();
        cv->slot = slot;
#ifdef DEBUG
        cv->callDepth = callDepth;
#endif

        // Even though the frame is out of range, later we might be called as an
        // inner trace such that the target variable is defined in the outer trace
        // entry frame. For simplicity, we just fall off trace.
        guard(false,
              w.eqp(entryFrameIns(), w.ldpObjPrivate(obj_ins)),
              MISMATCH_EXIT);

        LIns* outp = w.allocp(sizeof(double));
        LIns* args[] = {
            outp,
            w.nameImmpNonGC(cv),
            obj_ins,
            cx_ins
        };
        const CallInfo* ci;
        if (shape->getterOp() == js_GetCallArg) {
            ci = &GetClosureArg_ci;
        } else if (shape->getterOp() == js_GetCallVar ||
                   shape->getterOp() == js_GetCallVarChecked) {
            ci = &GetClosureVar_ci;
        } else {
            RETURN_STOP("dynamic property of Call object");
        }

        // Now assert that our use of shape->shortid was in fact kosher.
        JS_ASSERT(shape->hasShortID());

        LIns* call_ins = w.call(ci, args);

        JSValueType type = getCoercedType(nr.v);
        guard(true,
              w.name(w.eqi(call_ins, w.immi(type)), "guard(type-stable name access)"),
              BRANCH_EXIT);
        ins = stackLoad(AllocSlotsAddress(outp), type);
    }
    nr.tracked = false;
    nr.obj = obj;
    nr.obj_ins = obj_ins;
    nr.shape = shape;
    return RECORD_CONTINUE;
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
    set(&stackval(n), i);
}

/* Leave trace iff one operand is negative and the other is non-negative. */
JS_REQUIRES_STACK void
TraceRecorder::guardNonNeg(LIns* d0, LIns* d1, VMSideExit* exit)
{
    if (d0->isImmI())
        JS_ASSERT(d0->immI() >= 0);
    else
        guard(false, w.ltiN(d0, 0), exit);

    if (d1->isImmI())
        JS_ASSERT(d1->immI() >= 0);
    else
        guard(false, w.ltiN(d1, 0), exit);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::alu(LOpcode v, jsdouble v0, jsdouble v1, LIns* s0, LIns* s1)
{
    /*
     * To even consider this operation for demotion, both operands have to be
     * integers and the oracle must not give us a negative hint for the
     * instruction.
     */
    if (!oracle || oracle->isInstructionUndemotable(cx->regs->pc) ||
        !IsPromoteInt(s0) || !IsPromoteInt(s1)) {
    out:
        if (v == LIR_modd) {
            LIns* args[] = { s1, s0 };
            return w.call(&js_dmod_ci, args);
        }
        LIns* result = w.ins2(v, s0, s1);
        JS_ASSERT_IF(s0->isImmD() && s1->isImmD(), result->isImmD());
        return result;
    }

    jsdouble r;
    switch (v) {
    case LIR_addd:
        r = v0 + v1;
        break;
    case LIR_subd:
        r = v0 - v1;
        break;
    case LIR_muld:
        r = v0 * v1;
        if (r == 0.0 && (v0 < 0.0 || v1 < 0.0))
            goto out;
        break;
#if defined NANOJIT_IA32 || defined NANOJIT_X64
    case LIR_divd:
        if (v1 == 0)
            goto out;
        r = v0 / v1;
        break;
    case LIR_modd:
        if (v0 < 0 || v1 == 0 || (s1->isImmD() && v1 < 0))
            goto out;
        r = js_dmod(v0, v1);
        break;
#endif
    default:
        goto out;
    }

    /*
     * The result must be an integer at record time, otherwise there is no
     * point in trying to demote it.
     */
    if (jsint(r) != r || JSDOUBLE_IS_NEGZERO(r))
        goto out;

    LIns* d0 = w.demote(s0);
    LIns* d1 = w.demote(s1);

    /*
     * Speculatively emit an integer operation, betting that at runtime we
     * will get integer results again.
     */
    VMSideExit* exit = NULL;
    LIns* result;
    switch (v) {
#if defined NANOJIT_IA32 || defined NANOJIT_X64
      case LIR_divd:
        if (d0->isImmI() && d1->isImmI())
            return w.i2d(w.immi(jsint(r)));

        exit = snapshot(OVERFLOW_EXIT);

        /*
         * If the divisor is greater than zero its always safe to execute
         * the division. If not, we have to make sure we are not running
         * into -2147483648 / -1, because it can raise an overflow exception.
         */
        if (!d1->isImmI()) {
            if (MaybeBranch mbr = w.jt(w.gtiN(d1, 0))) {
                guard(false, w.eqi0(d1), exit);
                guard(true,  w.eqi0(w.andi(w.eqiN(d0, 0x80000000),
                                           w.eqiN(d1, -1))), exit);
                w.label(mbr);
            }
        } else {
            if (d1->immI() == -1)
                guard(false, w.eqiN(d0, 0x80000000), exit);
        }
        v = LIR_divi;
        result = w.divi(d0, d1);

        /* As long as the modulus is zero, the result is an integer. */
        guard(true, w.eqi0(w.modi(result)), exit);

        /* Don't lose a -0. */
        guard(false, w.eqi0(result), exit);
        break;

      case LIR_modd: {
        if (d0->isImmI() && d1->isImmI())
            return w.i2d(w.immi(jsint(r)));

        exit = snapshot(OVERFLOW_EXIT);

        /* Make sure we don't trigger division by zero at runtime. */
        if (!d1->isImmI())
            guard(false, w.eqi0(d1), exit);
        v = LIR_modi;
        result = w.modi(w.divi(d0, d1));

        /* If the result is not 0, it is always within the integer domain. */
        if (MaybeBranch mbr = w.jf(w.eqi0(result))) {
            /*
             * If the result is zero, we must exit if the lhs is negative since
             * the result is -0 in this case, which is not in the integer domain.
             */
            guard(false, w.ltiN(d0, 0), exit);
            w.label(mbr);
        }
        break;
      }
#endif

      default:
        v = arithOpcodeD2I(v);
        JS_ASSERT(v == LIR_addi || v == LIR_muli || v == LIR_subi);

        /*
         * If the operands guarantee that the result will be an integer (e.g.
         * z = x * y with 0 <= (x|y) <= 0xffff guarantees z <= fffe0001), we
         * don't have to guard against an overflow. Otherwise we emit a guard
         * that will inform the oracle and cause a non-demoted trace to be
         * attached that uses floating-point math for this operation.
         */
        bool needsOverflowCheck = true, needsNegZeroCheck = true;
        ChecksRequired(v, d0, d1, &needsOverflowCheck, &needsNegZeroCheck);
        if (needsOverflowCheck) {
            exit = snapshot(OVERFLOW_EXIT);
            result = guard_xov(v, d0, d1, exit);
        } else {
            result = w.ins2(v, d0, d1);
        }
        if (needsNegZeroCheck) {
            JS_ASSERT(v == LIR_muli);
            /*
             * Make sure we don't lose a -0. We exit if the result is zero and if
             * either operand is negative. We start out using a weaker guard, checking
             * if either argument is negative. If this ever fails, we recompile with
             * a stronger, but slower, guard.
             */
            if (v0 < 0.0 || v1 < 0.0
                || !oracle || oracle->isInstructionSlowZeroTest(cx->regs->pc))
            {
                if (!exit)
                    exit = snapshot(OVERFLOW_EXIT);

                guard(true,
                      w.eqi0(w.andi(w.eqi0(result),
                                    w.ori(w.ltiN(d0, 0),
                                          w.ltiN(d1, 0)))),
                      exit);
            } else {
                guardNonNeg(d0, d1, snapshot(MUL_ZERO_EXIT));
            }
        }
        break;
    }
    JS_ASSERT_IF(d0->isImmI() && d1->isImmI(), result->isImmI(jsint(r)));
    return w.i2d(result);
}

LIns*
TraceRecorder::d2i(LIns* d, bool resultCanBeImpreciseIfFractional)
{
    if (d->isImmD())
        return w.immi(js_DoubleToECMAInt32(d->immD()));
    if (d->isop(LIR_i2d) || d->isop(LIR_ui2d))
        return d->oprnd1();
    if (d->isop(LIR_addd) || d->isop(LIR_subd)) {
        LIns* lhs = d->oprnd1();
        LIns* rhs = d->oprnd2();
        if (IsPromote(lhs) && IsPromote(rhs)) {
            LOpcode op = arithOpcodeD2I(d->opcode());
            return w.ins2(op, w.demote(lhs), w.demote(rhs));
        }
    }
    if (d->isCall()) {
        const CallInfo* ci = d->callInfo();
        if (ci == &js_UnboxDouble_ci) {
#if JS_BITS_PER_WORD == 32
            LIns *tag_ins = d->callArgN(0);
            LIns *payload_ins = d->callArgN(1);
            LIns* args[] = { payload_ins, tag_ins };
            return w.call(&js_UnboxInt32_ci, args);
#else
            LIns* val_ins = d->callArgN(0);
            LIns* args[] = { val_ins };
            return w.call(&js_UnboxInt32_ci, args);
#endif
        }
        if (ci == &js_StringToNumber_ci) {
            LIns* args[] = { d->callArgN(1), d->callArgN(0) };
            return w.call(&js_StringToInt32_ci, args);
        }
    }
    return resultCanBeImpreciseIfFractional
         ? w.rawD2i(d)
         : w.call(&js_DoubleToInt32_ci, &d);
}

LIns*
TraceRecorder::d2u(LIns* d)
{
    if (d->isImmD())
        return w.immi(js_DoubleToECMAUint32(d->immD()));
    if (d->isop(LIR_i2d) || d->isop(LIR_ui2d))
        return d->oprnd1();
    return w.call(&js_DoubleToUint32_ci, &d);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::makeNumberInt32(LIns* d, LIns** out)
{
    JS_ASSERT(d->isD());
    if (IsPromote(d)) {
        *out = w.demote(d);
        return RECORD_CONTINUE;
    }

    // This means "convert double to int if it's integral, otherwise
    // exit".  We first convert the double to an int, then convert it back
    // and exit if the two doubles don't match.  If 'f' is a non-integral
    // immediate we'll end up aborting.
    *out = d2i(d, /* resultCanBeImpreciseIfFractional = */true);
    return guard(true, w.eqd(d, w.i2d(*out)), MISMATCH_EXIT, /* abortIfAlwaysExits = */true);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::stringify(const Value& v)
{
    LIns* v_ins = get(&v);
    if (v.isString())
        return v_ins;

    LIns* args[] = { v_ins, cx_ins };
    const CallInfo* ci;
    if (v.isNumber()) {
        ci = &js_NumberToString_ci;
    } else if (v.isUndefined()) {
        return w.immpAtomGC(cx->runtime->atomState.booleanAtoms[2]);
    } else if (v.isBoolean()) {
        ci = &js_BooleanIntToString_ci;
    } else {
        /*
         * Callers must deal with non-primitive (non-null object) values by
         * calling an imacro. We don't try to guess about which imacro, with
         * what valueOf hint, here.
         */
        JS_ASSERT(v.isNull());
        return w.immpAtomGC(cx->runtime->atomState.nullAtom);
    }

    v_ins = w.call(ci, args);
    guard(false, w.eqp0(v_ins), OOM_EXIT);
    return v_ins;
}

JS_REQUIRES_STACK bool
TraceRecorder::canCallImacro() const
{
    /* We cannot nest imacros. */
    return !cx->fp()->hasImacropc();
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callImacro(jsbytecode* imacro)
{
    return canCallImacro() ? callImacroInfallibly(imacro) : RECORD_STOP;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callImacroInfallibly(jsbytecode* imacro)
{
    JSStackFrame* fp = cx->fp();
    JS_ASSERT(!fp->hasImacropc());
    JSFrameRegs* regs = cx->regs;
    fp->setImacropc(regs->pc);
    regs->pc = imacro;
    updateAtoms();
    return RECORD_IMACRO;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::ifop()
{
    Value& v = stackval(-1);
    LIns* v_ins = get(&v);
    bool cond;
    LIns* x;

    if (v.isNull() || v.isUndefined()) {
        cond = false;
        x = w.immi(0);
    } else if (!v.isPrimitive()) {
        cond = true;
        x = w.immi(1);
    } else if (v.isBoolean()) {
        /* Test for boolean is true, negate later if we are testing for false. */
        cond = v.isTrue();
        x = w.eqiN(v_ins, 1);
    } else if (v.isNumber()) {
        jsdouble d = v.toNumber();
        cond = !JSDOUBLE_IS_NaN(d) && d;
        x = w.eqi0(w.eqi0(w.andi(w.eqd(v_ins, v_ins), w.eqi0(w.eqd0(v_ins)))));
    } else if (v.isString()) {
        cond = v.toString()->length() != 0;
        x = w.eqi0(w.eqp0(w.getStringLength(v_ins)));
    } else {
        JS_NOT_REACHED("ifop");
        return ARECORD_STOP;
    }

    jsbytecode* pc = cx->regs->pc;
    emitIf(pc, cond, x);
    return checkTraceEnd(pc);
}

#ifdef NANOJIT_IA32
/*
 * Record LIR for a tableswitch or tableswitchx op. We record LIR only the
 * "first" time we hit the op. Later, when we start traces after exiting that
 * trace, we just patch.
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::tableswitch()
{
    Value& v = stackval(-1);

    /* No need to guard if the condition can't match any of the cases. */
    if (!v.isNumber())
        return ARECORD_CONTINUE;

    /* No need to guard if the condition is constant. */
    LIns* v_ins = d2i(get(&v));
    if (v_ins->isImmI())
        return ARECORD_CONTINUE;

    jsbytecode* pc = cx->regs->pc;
    /* Starting a new trace after exiting a trace via switch. */
    if (anchor &&
        (anchor->exitType == CASE_EXIT || anchor->exitType == DEFAULT_EXIT) &&
        fragment->ip == pc) {
        return ARECORD_CONTINUE;
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
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
    }

    /* 
     * If there are no cases, this is a no-op. The default case immediately
     * follows in the bytecode and is always taken, so we need no special
     * action to handle it.
     */
    int count = high + 1 - low;
    JS_ASSERT(count >= 0);
    if (count == 0)
        return ARECORD_CONTINUE;

    /* Cap maximum table-switch size for modesty. */
    if (count > MAX_TABLE_SWITCH)
        return InjectStatus(switchop());

    /* Generate switch LIR. */
    SwitchInfo* si = new (traceAlloc()) SwitchInfo();
    si->count = count;
    si->table = 0;
    si->index = (uint32) -1;
    LIns* diff = w.subi(v_ins, w.immi(low));
    LIns* cmp = w.ltui(diff, w.immi(si->count));
    guard(true, cmp, DEFAULT_EXIT);
    // We use AnyAddress;  it's imprecise but this case is rare and not worth its
    // own access region.
    w.st(diff, AnyAddress(w.immpNonGC(&si->index)));
    VMSideExit* exit = snapshot(CASE_EXIT);
    exit->switchInfo = si;
    LIns* guardIns = w.xtbl(diff, createGuardRecord(exit));
    fragment->lastIns = guardIns;
    CHECK_STATUS_A(compile());
    return finishSuccessfully();
}
#endif

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::switchop()
{
    Value& v = stackval(-1);
    LIns* v_ins = get(&v);

    /* No need to guard if the condition is constant. */
    if (v_ins->isImmAny())
        return RECORD_CONTINUE;
    if (v.isNumber()) {
        jsdouble d = v.toNumber();
        guard(true,
              w.name(w.eqd(v_ins, w.immd(d)), "guard(switch on numeric)"),
              BRANCH_EXIT);
    } else if (v.isString()) {
        LIns* args[] = { w.immpStrGC(v.toString()), v_ins };
        guard(true,
              w.name(w.eqi0(w.eqi0(w.call(&js_EqualStrings_ci, args))),
                     "guard(switch on string)"),
              BRANCH_EXIT);
    } else if (v.isBoolean()) {
        guard(true,
              w.name(w.eqi(v_ins, w.immi(v.isTrue())), "guard(switch on boolean)"),
              BRANCH_EXIT);
    } else if (v.isUndefined()) {
        // This is a unit type, so no guard is needed.
    } else {
        RETURN_STOP("switch on object or null");
    }
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::inc(Value& v, jsint incr, bool pre)
{
    LIns* v_ins = get(&v);
    CHECK_STATUS(inc(v, v_ins, incr, pre));
    set(&v, v_ins);
    return RECORD_CONTINUE;
}

/*
 * On exit, v_ins is the incremented unboxed value, and the appropriate value
 * (pre- or post-increment as described by pre) is stacked.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::inc(const Value &v, LIns*& v_ins, jsint incr, bool pre)
{
    LIns* v_after;
    CHECK_STATUS(incHelper(v, v_ins, v_after, incr));

    const JSCodeSpec& cs = js_CodeSpec[*cx->regs->pc];
    JS_ASSERT(cs.ndefs == 1);
    stack(-cs.nuses, pre ? v_after : v_ins);
    v_ins = v_after;
    return RECORD_CONTINUE;
}

/*
 * Do an increment operation without storing anything to the stack.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::incHelper(const Value &v, LIns* v_ins, LIns*& v_after, jsint incr)
{
    // FIXME: Bug 606071 on making this work for objects.
    if (!v.isPrimitive())
        RETURN_STOP("can inc primitives only");

    if (v.isUndefined()) {
        v_after = w.immd(js_NaN);
    } else if (v.isNull()) {
        v_after = w.immd(incr);
    } else {
        if (v.isBoolean()) {
            v_ins = w.i2d(v_ins);
        } else if (v.isString()) {
            LIns* args[] = { v_ins, cx_ins };
            v_ins = w.call(&js_StringToNumber_ci, args);
        } else {
            JS_ASSERT(v.isNumber());
        }

        jsdouble num;
        AutoValueRooter tvr(cx);
        *tvr.addr() = v;
        ValueToNumber(cx, tvr.value(), &num);
        v_after = alu(LIR_addd, num, incr, v_ins, w.immd(incr));
    }

    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::incProp(jsint incr, bool pre)
{
    Value& l = stackval(-1);
    if (l.isPrimitive())
        RETURN_STOP_A("incProp on primitive");

    JSObject* obj = &l.toObject();
    LIns* obj_ins = get(&l);

    uint32 slot;
    LIns* v_ins;
    CHECK_STATUS_A(prop(obj, obj_ins, &slot, &v_ins, NULL));

    if (slot == SHAPE_INVALID_SLOT)
        RETURN_STOP_A("incProp on invalid slot");

    Value& v = obj->getSlotRef(slot);
    CHECK_STATUS_A(inc(v, v_ins, incr, pre));

    LIns* slots_ins = NULL;
    stobj_set_slot(obj, obj_ins, slot, slots_ins, v, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::incElem(jsint incr, bool pre)
{
    Value& r = stackval(-1);
    Value& l = stackval(-2);
    Value* vp;
    LIns* v_ins;
    LIns* addr_ins;

    if (!l.isPrimitive() && l.toObject().isDenseArray() && r.isInt32()) {
        guardDenseArray(get(&l), MISMATCH_EXIT);
        CHECK_STATUS(denseArrayElement(l, r, vp, v_ins, addr_ins, snapshot(BRANCH_EXIT)));
        if (!addr_ins) // if we read a hole, abort
            return RECORD_STOP;
        CHECK_STATUS(inc(*vp, v_ins, incr, pre));
        box_value_into(*vp, v_ins, DSlotsAddress(addr_ins));
        return RECORD_CONTINUE;
    }

    return callImacro((incr == 1)
                      ? pre ? incelem_imacros.incelem : incelem_imacros.eleminc
                      : pre ? decelem_imacros.decelem : decelem_imacros.elemdec);
}

static bool
EvalCmp(LOpcode op, double l, double r)
{
    bool cond;
    switch (op) {
      case LIR_eqd:
        cond = (l == r);
        break;
      case LIR_ltd:
        cond = l < r;
        break;
      case LIR_gtd:
        cond = l > r;
        break;
      case LIR_led:
        cond = l <= r;
        break;
      case LIR_ged:
        cond = l >= r;
        break;
      default:
        JS_NOT_REACHED("unexpected comparison op");
        return false;
    }
    return cond;
}

static bool
EvalCmp(LOpcode op, JSString* l, JSString* r)
{
    if (op == LIR_eqd)
        return !!js_EqualStrings(l, r);
    return EvalCmp(op, js_CompareStrings(l, r), 0);
}

JS_REQUIRES_STACK void
TraceRecorder::strictEquality(bool equal, bool cmpCase)
{
    Value& r = stackval(-1);
    Value& l = stackval(-2);
    LIns* l_ins = get(&l);
    LIns* r_ins = get(&r);
    LIns* x;
    bool cond;

    JSValueType ltag = getPromotedType(l);
    if (ltag != getPromotedType(r)) {
        cond = !equal;
        x = w.immi(cond);
    } else if (ltag == JSVAL_TYPE_STRING) {
        LIns* args[] = { r_ins, l_ins };
        x = w.eqiN(w.call(&js_EqualStrings_ci, args), equal);
        cond = !!js_EqualStrings(l.toString(), r.toString());
    } else {
        if (ltag == JSVAL_TYPE_DOUBLE)
            x = w.eqd(l_ins, r_ins);
        else if (ltag == JSVAL_TYPE_NULL || ltag == JSVAL_TYPE_NONFUNOBJ || ltag == JSVAL_TYPE_FUNOBJ)
            x = w.eqp(l_ins, r_ins);
        else
            x = w.eqi(l_ins, r_ins);
        if (!equal)
            x = w.eqi0(x);
        cond = (ltag == JSVAL_TYPE_DOUBLE)
               ? l.toNumber() == r.toNumber()
               : l == r;
    }
    cond = (cond == equal);

    if (cmpCase) {
        /* Only guard if the same path may not always be taken. */
        if (!x->isImmI())
            guard(cond, x, BRANCH_EXIT);
        return;
    }

    set(&l, x);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::equality(bool negate, bool tryBranchAfterCond)
{
    Value& rval = stackval(-1);
    Value& lval = stackval(-2);
    LIns* l_ins = get(&lval);
    LIns* r_ins = get(&rval);

    return equalityHelper(lval, rval, l_ins, r_ins, negate, tryBranchAfterCond, lval);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::equalityHelper(Value& l, Value& r, LIns* l_ins, LIns* r_ins,
                              bool negate, bool tryBranchAfterCond,
                              Value& rval)
{
    LOpcode op = LIR_eqi;
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
        if (l.isUndefined() || l.isNull()) {
            cond = true;
            if (l.isNull())
                op = LIR_eqp;
        } else if (l.isObject()) {
            if (l.toObject().getClass()->ext.equality)
                RETURN_STOP_A("Can't trace extended class equality operator");
            LIns* flags_ins = w.ldiObjFlags(l_ins);
            LIns* flag_ins = w.andi(flags_ins, w.nameImmui(JSObject::HAS_EQUALITY));
            guard(true, w.eqi0(flag_ins), BRANCH_EXIT);

            op = LIR_eqp;
            cond = (l == r);
        } else if (l.isBoolean()) {
            JS_ASSERT(r.isBoolean());
            cond = (l == r);
        } else if (l.isString()) {
            JSString *l_str = l.toString();
            JSString *r_str = r.toString();
            if (!l_str->isRope() && !r_str->isRope() && l_str->length() == 1 && r_str->length() == 1) {
                VMSideExit *exit = snapshot(BRANCH_EXIT);
                LIns *c = w.nameImmw(1);
                guard(true, w.eqp(w.getStringLength(l_ins), c), exit);
                guard(true, w.eqp(w.getStringLength(r_ins), c), exit);
                l_ins = w.getStringChar0(l_ins);
                r_ins = w.getStringChar0(r_ins);
            } else {
                args[0] = r_ins, args[1] = l_ins;
                l_ins = w.call(&js_EqualStrings_ci, args);
                r_ins = w.immi(1);
            }
            cond = !!js_EqualStrings(l.toString(), r.toString());
        } else {
            JS_ASSERT(l.isNumber() && r.isNumber());
            cond = (l.toNumber() == r.toNumber());
            op = LIR_eqd;
        }
    } else if (l.isNull() && r.isUndefined()) {
        l_ins = w.immiUndefined();
        cond = true;
    } else if (l.isUndefined() && r.isNull()) {
        r_ins = w.immiUndefined();
        cond = true;
    } else if (l.isNumber() && r.isString()) {
        args[0] = r_ins, args[1] = cx_ins;
        r_ins = w.call(&js_StringToNumber_ci, args);
        cond = (l.toNumber() == js_StringToNumber(cx, r.toString()));
        op = LIR_eqd;
    } else if (l.isString() && r.isNumber()) {
        args[0] = l_ins, args[1] = cx_ins;
        l_ins = w.call(&js_StringToNumber_ci, args);
        cond = (js_StringToNumber(cx, l.toString()) == r.toNumber());
        op = LIR_eqd;
    } else {
        // Below we may assign to l or r, which modifies the interpreter state.
        // This is fine as long as we also update the tracker.
        if (l.isBoolean()) {
            l_ins = w.i2d(l_ins);
            set(&l, l_ins);
            l.setInt32(l.isTrue());
            return equalityHelper(l, r, l_ins, r_ins, negate,
                                  tryBranchAfterCond, rval);
        }
        if (r.isBoolean()) {
            r_ins = w.i2d(r_ins);
            set(&r, r_ins);
            r.setInt32(r.isTrue());
            return equalityHelper(l, r, l_ins, r_ins, negate,
                                  tryBranchAfterCond, rval);
        }
        if ((l.isString() || l.isNumber()) && !r.isPrimitive()) {
            CHECK_STATUS_A(guardNativeConversion(r));
            return InjectStatus(callImacro(equality_imacros.any_obj));
        }
        if (!l.isPrimitive() && (r.isString() || r.isNumber())) {
            CHECK_STATUS_A(guardNativeConversion(l));
            return InjectStatus(callImacro(equality_imacros.obj_any));
        }

        l_ins = w.immi(0);
        r_ins = w.immi(1);
        cond = false;
    }

    /* If the operands aren't numbers, compare them as integers. */
    LIns* x = w.ins2(op, l_ins, r_ins);
    if (negate) {
        x = w.eqi0(x);
        cond = !cond;
    }

    jsbytecode* pc = cx->regs->pc;

    /*
     * Don't guard if the same path is always taken.  If it isn't, we have to
     * fuse comparisons and the following branch, because the interpreter does
     * that.
     */
    if (tryBranchAfterCond)
        fuseIf(pc + 1, cond, x);

    /*
     * There is no need to write out the result of this comparison if the trace
     * ends on this operation.
     */
    if (pc[1] == JSOP_IFNE || pc[1] == JSOP_IFEQ)
        CHECK_STATUS_A(checkTraceEnd(pc + 1));

    /*
     * We update the stack after the guard. This is safe since the guard bails
     * out at the comparison and the interpreter will therefore re-execute the
     * comparison. This way the value of the condition doesn't have to be
     * calculated and saved on the stack in most cases.
     */
    set(&rval, x);

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::relational(LOpcode op, bool tryBranchAfterCond)
{
    Value& r = stackval(-1);
    Value& l = stackval(-2);
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
    if (!l.isPrimitive()) {
        CHECK_STATUS_A(guardNativeConversion(l));
        if (!r.isPrimitive()) {
            CHECK_STATUS_A(guardNativeConversion(r));
            return InjectStatus(callImacro(binary_imacros.obj_obj));
        }
        return InjectStatus(callImacro(binary_imacros.obj_any));
    }
    if (!r.isPrimitive()) {
        CHECK_STATUS_A(guardNativeConversion(r));
        return InjectStatus(callImacro(binary_imacros.any_obj));
    }

    /* 11.8.5 steps 3, 16-21. */
    if (l.isString() && r.isString()) {
        LIns* args[] = { r_ins, l_ins };
        l_ins = w.call(&js_CompareStrings_ci, args);
        r_ins = w.immi(0);
        cond = EvalCmp(op, l.toString(), r.toString());
        goto do_comparison;
    }

    /* 11.8.5 steps 4-5. */
    if (!l.isNumber()) {
        LIns* args[] = { l_ins, cx_ins };
        if (l.isBoolean()) {
            l_ins = w.i2d(l_ins);
        } else if (l.isUndefined()) {
            l_ins = w.immd(js_NaN);
        } else if (l.isString()) {
            l_ins = w.call(&js_StringToNumber_ci, args);
        } else if (l.isNull()) {
            l_ins = w.immd(0.0);
        } else {
            JS_NOT_REACHED("JSVAL_IS_NUMBER if int/double, objects should "
                           "have been handled at start of method");
            RETURN_STOP_A("safety belt");
        }
    }
    if (!r.isNumber()) {
        LIns* args[] = { r_ins, cx_ins };
        if (r.isBoolean()) {
            r_ins = w.i2d(r_ins);
        } else if (r.isUndefined()) {
            r_ins = w.immd(js_NaN);
        } else if (r.isString()) {
            r_ins = w.call(&js_StringToNumber_ci, args);
        } else if (r.isNull()) {
            r_ins = w.immd(0.0);
        } else {
            JS_NOT_REACHED("JSVAL_IS_NUMBER if int/double, objects should "
                           "have been handled at start of method");
            RETURN_STOP_A("safety belt");
        }
    }
    {
        AutoValueRooter tvr(cx);
        *tvr.addr() = l;
        ValueToNumber(cx, tvr.value(), &lnum);
        *tvr.addr() = r;
        ValueToNumber(cx, tvr.value(), &rnum);
    }
    cond = EvalCmp(op, lnum, rnum);
    fp = true;

    /* 11.8.5 steps 6-15. */
  do_comparison:
    /*
     * If the result is not a number or it's not a quad, we must use an integer
     * compare.
     */
    if (!fp) {
        JS_ASSERT(isCmpDOpcode(op));
        op = cmpOpcodeD2I(op);
    }
    x = w.ins2(op, l_ins, r_ins);

    jsbytecode* pc = cx->regs->pc;

    /*
     * Don't guard if the same path is always taken.  If it isn't, we have to
     * fuse comparisons and the following branch, because the interpreter does
     * that.
     */
    if (tryBranchAfterCond)
        fuseIf(pc + 1, cond, x);

    /*
     * There is no need to write out the result of this comparison if the trace
     * ends on this operation.
     */
    if (pc[1] == JSOP_IFNE || pc[1] == JSOP_IFEQ)
        CHECK_STATUS_A(checkTraceEnd(pc + 1));

    /*
     * We update the stack after the guard. This is safe since the guard bails
     * out at the comparison and the interpreter will therefore re-execute the
     * comparison. This way the value of the condition doesn't have to be
     * calculated and saved on the stack in most cases.
     */
    set(&l, x);

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::unary(LOpcode op)
{
    Value& v = stackval(-1);
    bool intop = retTypes[op] == LTy_I;
    if (v.isNumber()) {
        LIns* a = get(&v);
        if (intop)
            a = d2i(a);
        a = w.ins1(op, a);
        if (intop)
            a = w.i2d(a);
        set(&v, a);
        return RECORD_CONTINUE;
    }
    return RECORD_STOP;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::binary(LOpcode op)
{
    Value& r = stackval(-1);
    Value& l = stackval(-2);

    if (!l.isPrimitive()) {
        CHECK_STATUS(guardNativeConversion(l));
        if (!r.isPrimitive()) {
            CHECK_STATUS(guardNativeConversion(r));
            return callImacro(binary_imacros.obj_obj);
        }
        return callImacro(binary_imacros.obj_any);
    }
    if (!r.isPrimitive()) {
        CHECK_STATUS(guardNativeConversion(r));
        return callImacro(binary_imacros.any_obj);
    }

    bool intop = retTypes[op] == LTy_I;
    LIns* a = get(&l);
    LIns* b = get(&r);

    bool leftIsNumber = l.isNumber();
    jsdouble lnum = leftIsNumber ? l.toNumber() : 0;

    bool rightIsNumber = r.isNumber();
    jsdouble rnum = rightIsNumber ? r.toNumber() : 0;

    if (l.isString()) {
        NanoAssert(op != LIR_addd); // LIR_addd/IS_STRING case handled by record_JSOP_ADD()
        LIns* args[] = { a, cx_ins };
        a = w.call(&js_StringToNumber_ci, args);
        lnum = js_StringToNumber(cx, l.toString());
        leftIsNumber = true;
    }
    if (r.isString()) {
        NanoAssert(op != LIR_addd); // LIR_addd/IS_STRING case handled by record_JSOP_ADD()
        LIns* args[] = { b, cx_ins };
        b = w.call(&js_StringToNumber_ci, args);
        rnum = js_StringToNumber(cx, r.toString());
        rightIsNumber = true;
    }
    if (l.isBoolean()) {
        a = w.i2d(a);
        lnum = l.toBoolean();
        leftIsNumber = true;
    } else if (l.isUndefined()) {
        a = w.immd(js_NaN);
        lnum = js_NaN;
        leftIsNumber = true;
    }
    if (r.isBoolean()) {
        b = w.i2d(b);
        rnum = r.toBoolean();
        rightIsNumber = true;
    } else if (r.isUndefined()) {
        b = w.immd(js_NaN);
        rnum = js_NaN;
        rightIsNumber = true;
    }
    if (leftIsNumber && rightIsNumber) {
        if (intop) {
            a = (op == LIR_rshui) ? d2u(a) : d2i(a);
            b = d2i(b);
        }
        a = alu(op, lnum, rnum, a, b);
        if (intop)
            a = (op == LIR_rshui) ? w.ui2d(a) : w.i2d(a);
        set(&l, a);
        return RECORD_CONTINUE;
    }
    return RECORD_STOP;
}

#if defined DEBUG_notme && defined XP_UNIX
#include <stdio.h>

static FILE* shapefp = NULL;

static void
DumpShape(JSObject* obj, const char* prefix)
{
    if (!shapefp) {
        shapefp = fopen("/tmp/shapes.dump", "w");
        if (!shapefp)
            return;
    }

    fprintf(shapefp, "\n%s: shape %u flags %x\n", prefix, obj->shape(), obj->flags);
    for (Shape::Range r = obj->lastProperty()->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();

        if (JSID_IS_ATOM(shape.id)) {
            fprintf(shapefp, " %s", JS_GetStringBytes(JSID_TO_STRING(shape.id)));
        } else {
            JS_ASSERT(!JSID_IS_OBJECT(shape.id));
            fprintf(shapefp, " %d", JSID_TO_INT(shape.id));
        }
        fprintf(shapefp, " %u %p %p %x %x %d\n",
                shape.slot, shape.getter, shape.setter, shape.attrs, shape.flags, shape.shortid);
    }
    fflush(shapefp);
}

void
TraceRecorder::dumpGuardedShapes(const char* prefix)
{
    for (GuardedShapeTable::Range r = guardedShapeTable.all(); !r.empty(); r.popFront())
        DumpShape(r.front().value, prefix);
}
#endif /* DEBUG_notme && XP_UNIX */

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardShape(LIns* obj_ins, JSObject* obj, uint32 shape, const char* guardName,
                          VMSideExit* exit)
{
    // Test (with add if missing) for a remembered guard for (obj_ins, obj).
    GuardedShapeTable::AddPtr p = guardedShapeTable.lookupForAdd(obj_ins);
    if (p) {
        JS_ASSERT(p->value == obj);
        return RECORD_CONTINUE;
    }
    if (!guardedShapeTable.add(p, obj_ins, obj))
        return RECORD_ERROR;

    if (obj == globalObj) {
        // In this case checking object identity is equivalent and faster.
        guard(true,
              w.name(w.eqp(obj_ins, w.immpObjGC(globalObj)), "guard_global"),
              exit);
        return RECORD_CONTINUE;
    }

#if defined DEBUG_notme && defined XP_UNIX
    DumpShape(obj, "guard");
    fprintf(shapefp, "for obj_ins %p\n", obj_ins);
#endif

    // Finally, emit the shape guard.
    guard(true, w.name(w.eqiN(w.ldiObjShape(obj_ins), shape), guardName), exit);
    return RECORD_CONTINUE;
}

void
TraceRecorder::forgetGuardedShapesForObject(JSObject* obj)
{
    for (GuardedShapeTable::Enum e(guardedShapeTable); !e.empty(); e.popFront()) {
        if (e.front().value == obj) {
#if defined DEBUG_notme && defined XP_UNIX
            DumpShape(entry->obj, "forget");
#endif
            e.removeFront();
        }
    }
}

void
TraceRecorder::forgetGuardedShapes()
{
#if defined DEBUG_notme && defined XP_UNIX
    dumpGuardedShapes("forget-all");
#endif
    guardedShapeTable.clear();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::test_property_cache(JSObject* obj, LIns* obj_ins, JSObject*& obj2, PCVal& pcval)
{
    jsbytecode* pc = cx->regs->pc;
    JS_ASSERT(*pc != JSOP_INITPROP && *pc != JSOP_INITMETHOD &&
              *pc != JSOP_SETNAME && *pc != JSOP_SETPROP && *pc != JSOP_SETMETHOD);

    // Mimic the interpreter's special case for dense arrays by skipping up one
    // hop along the proto chain when accessing a named (not indexed) property,
    // typically to find Array.prototype methods.
    JSObject* aobj = obj;
    if (obj->isDenseArray()) {
        guardDenseArray(obj_ins, BRANCH_EXIT);
        aobj = obj->getProto();
        obj_ins = w.ldpObjProto(obj_ins);
    }

    if (!aobj->isNative())
        RETURN_STOP_A("non-native object");

    JSAtom* atom;
    PropertyCacheEntry* entry;
    JS_PROPERTY_CACHE(cx).test(cx, pc, aobj, obj2, entry, atom);
    if (atom) {
        // Miss: pre-fill the cache for the interpreter, as well as for our needs.
        // FIXME: bug 458271.
        jsid id = ATOM_TO_JSID(atom);

        // The lookup below may change object shapes.
        forgetGuardedShapes();

        JSProperty* prop;
        if (JOF_OPMODE(*pc) == JOF_NAME) {
            JS_ASSERT(aobj == obj);

            TraceMonitor &localtm = *traceMonitor;
            entry = js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop);
            if (!entry)
                RETURN_ERROR_A("error in js_FindPropertyHelper");

            /* js_FindPropertyHelper can reenter the interpreter and kill |this|. */
            if (!localtm.recorder)
                return ARECORD_ABORTED;

            if (entry == JS_NO_PROP_CACHE_FILL)
                RETURN_STOP_A("cannot cache name");
        } else {
            TraceMonitor &localtm = *traceMonitor;
            int protoIndex = js_LookupPropertyWithFlags(cx, aobj, id,
                                                        cx->resolveFlags,
                                                        &obj2, &prop);

            if (protoIndex < 0)
                RETURN_ERROR_A("error in js_LookupPropertyWithFlags");

            /* js_LookupPropertyWithFlags can reenter the interpreter and kill |this|. */
            if (!localtm.recorder)
                return ARECORD_ABORTED;

            if (prop) {
                if (!obj2->isNative())
                    RETURN_STOP_A("property found on non-native object");
                entry = JS_PROPERTY_CACHE(cx).fill(cx, aobj, 0, protoIndex, obj2,
                                                   (Shape*) prop);
                JS_ASSERT(entry);
                if (entry == JS_NO_PROP_CACHE_FILL)
                    entry = NULL;
            }

        }

        if (!prop) {
            // Propagate obj from js_FindPropertyHelper to record_JSOP_BINDNAME
            // via our obj2 out-parameter. If we are recording JSOP_SETNAME and
            // the global it's assigning does not yet exist, create it.
            obj2 = obj;

            // Use a null pcval to return "no such property" to our caller.
            pcval.setNull();
            return ARECORD_CONTINUE;
        }

        if (!entry)
            RETURN_STOP_A("failed to fill property cache");
    }

#ifdef JS_THREADSAFE
    // There's a potential race in any JS_THREADSAFE embedding that's nuts
    // enough to share mutable objects on the scope or proto chain, but we
    // don't care about such insane embeddings. Anyway, the (scope, proto)
    // entry->vcap coordinates must reach obj2 from aobj at this point.
    JS_ASSERT(cx->thread->data.requestDepth);
#endif

    return InjectStatus(guardPropertyCacheHit(obj_ins, aobj, obj2, entry, pcval));
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardPropertyCacheHit(LIns* obj_ins,
                                     JSObject* aobj,
                                     JSObject* obj2,
                                     PropertyCacheEntry* entry,
                                     PCVal& pcval)
{
    VMSideExit* exit = snapshot(BRANCH_EXIT);

    uint32 vshape = entry->vshape();

    // Special case for the global object, which may be aliased to get a property value.
    // To catch cross-global property accesses we must check against globalObj identity.
    // But a JOF_NAME mode opcode needs no guard, as we ensure the global object's shape
    // never changes, and name ops can't reach across a global object ('with' aborts).
    if (aobj == globalObj) {
        if (entry->adding())
            RETURN_STOP("adding a property to the global object");

        JSOp op = js_GetOpcode(cx, cx->fp()->script(), cx->regs->pc);
        if (JOF_OPMODE(op) != JOF_NAME) {
            guard(true,
                  w.name(w.eqp(obj_ins, w.immpObjGC(globalObj)), "guard_global"),
                  exit);
        }
    } else {
        CHECK_STATUS(guardShape(obj_ins, aobj, entry->kshape, "guard_kshape", exit));
    }

    if (entry->adding()) {
        LIns *vshape_ins =
            w.ldiRuntimeProtoHazardShape(w.name(w.ldpConstContextField(runtime), "runtime"));

        guard(true,
              w.name(w.eqiN(vshape_ins, vshape), "guard_protoHazardShape"),
              MISMATCH_EXIT);
    }

    // For any hit that goes up the scope and/or proto chains, we will need to
    // guard on the shape of the object containing the property.
    if (entry->vcapTag() >= 1) {
        JS_ASSERT(obj2->shape() == vshape);
        if (obj2 == globalObj)
            RETURN_STOP("hitting the global object via a prototype chain");

        LIns* obj2_ins;
        if (entry->vcapTag() == 1) {
            // Duplicate the special case in PropertyCache::test.
            obj2_ins = w.ldpObjProto(obj_ins);
            guard(false, w.eqp0(obj2_ins), exit);
        } else {
            obj2_ins = w.immpObjGC(obj2);
        }
        CHECK_STATUS(guardShape(obj2_ins, obj2, vshape, "guard_vshape", exit));
    }

    pcval = entry->vword;
    return RECORD_CONTINUE;
}

void
TraceRecorder::stobj_set_fslot(LIns *obj_ins, unsigned slot, const Value &v, LIns* v_ins)
{
    box_value_into(v, v_ins, FSlotsAddress(obj_ins, slot));
}

void
TraceRecorder::stobj_set_dslot(LIns *obj_ins, unsigned slot, LIns*& slots_ins, 
                               const Value &v, LIns* v_ins)
{
    if (!slots_ins)
        slots_ins = w.name(w.ldpObjSlots(obj_ins), "slots");
    box_value_into(v, v_ins, DSlotsAddress(slots_ins, slot));
}

void
TraceRecorder::stobj_set_slot(JSObject *obj, LIns* obj_ins, unsigned slot, LIns*& slots_ins,
                              const Value &v, LIns* v_ins)
{
    /*
     * A shape guard must have already been generated for obj, which will
     * ensure that future objects have the same number of fixed slots.
     */
    if (!obj->hasSlotsArray()) {
        JS_ASSERT(slot < obj->numSlots());
        stobj_set_fslot(obj_ins, slot, v, v_ins);
    } else {
        stobj_set_dslot(obj_ins, slot, slots_ins, v, v_ins);
    }
}

LIns*
TraceRecorder::unbox_slot(JSObject *obj, LIns *obj_ins, uint32 slot, VMSideExit *exit)
{
    /* Same guarantee about fixed slots as stobj_set_slot. */
    Address addr = (!obj->hasSlotsArray())
                 ? (Address)FSlotsAddress(obj_ins, slot)
                 : (Address)DSlotsAddress(w.ldpObjSlots(obj_ins), slot);

    return unbox_value(obj->getSlot(slot), addr, exit);
}

#if JS_BITS_PER_WORD == 32

void
TraceRecorder::box_undefined_into(Address addr)
{
    w.stiValueTag(w.nameImmui(JSVAL_TAG_UNDEFINED), addr);
    w.stiValuePayload(w.nameImmi(0), addr);
}

void
TraceRecorder::box_null_into(Address addr)
{
    w.stiValueTag(w.nameImmui(JSVAL_TAG_NULL), addr);
    w.stiValuePayload(w.nameImmi(0), addr);
}

inline LIns*
TraceRecorder::unbox_number_as_double(Address addr, LIns *tag_ins, VMSideExit *exit)
{
    guard(true, w.leui(tag_ins, w.nameImmui(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET)), exit);
    LIns *val_ins = w.ldiValuePayload(addr);
    LIns* args[] = { val_ins, tag_ins };
    return w.call(&js_UnboxDouble_ci, args);
}

inline LIns*
TraceRecorder::unbox_non_double_object(Address addr, LIns* tag_ins,
                                       JSValueType type, VMSideExit* exit)
{
    LIns *val_ins;
    if (type == JSVAL_TYPE_UNDEFINED) {
        val_ins = w.immiUndefined();
    } else if (type == JSVAL_TYPE_NULL) {
        val_ins = w.immpNull();
    } else {
        JS_ASSERT(type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_OBJECT ||
                  type == JSVAL_TYPE_STRING || type == JSVAL_TYPE_BOOLEAN ||
                  type == JSVAL_TYPE_MAGIC);
        val_ins = w.ldiValuePayload(addr);
    }

    guard(true, w.eqi(tag_ins, w.nameImmui(JSVAL_TYPE_TO_TAG(type))), exit);
    return val_ins;
}

LIns*
TraceRecorder::unbox_object(Address addr, LIns* tag_ins, JSValueType type, VMSideExit* exit)
{
    JS_ASSERT(type == JSVAL_TYPE_FUNOBJ || type == JSVAL_TYPE_NONFUNOBJ);
    guard(true, w.eqi(tag_ins, w.nameImmui(JSVAL_TAG_OBJECT)), exit);
    LIns *payload_ins = w.ldiValuePayload(addr);
    if (type == JSVAL_TYPE_FUNOBJ)
        guardClass(payload_ins, &js_FunctionClass, exit, LOAD_NORMAL);
    else
        guardNotClass(payload_ins, &js_FunctionClass, exit, LOAD_NORMAL);
    return payload_ins;
}

LIns*
TraceRecorder::unbox_value(const Value &v, Address addr, VMSideExit *exit, bool force_double)
{
    LIns *tag_ins = w.ldiValueTag(addr);

    if (v.isNumber() && force_double)
        return unbox_number_as_double(addr, tag_ins, exit);

    if (v.isInt32()) {
        guard(true, w.eqi(tag_ins, w.nameImmui(JSVAL_TAG_INT32)), exit);
        return w.i2d(w.ldiValuePayload(addr));
    }

    if (v.isDouble()) {
        guard(true, w.ltui(tag_ins, w.nameImmui(JSVAL_TAG_CLEAR)), exit);
        return w.ldd(addr);
    }

    if (v.isObject()) {
        JSValueType type = v.toObject().isFunction() ? JSVAL_TYPE_FUNOBJ : JSVAL_TYPE_NONFUNOBJ;
        return unbox_object(addr, tag_ins, type, exit);
    }

    JSValueType type = v.extractNonDoubleObjectTraceType();
    return unbox_non_double_object(addr, tag_ins, type, exit);
}

void
TraceRecorder::unbox_any_object(Address addr, LIns **obj_ins, LIns **is_obj_ins)
{
    LIns *tag_ins = w.ldiValueTag(addr);
    *is_obj_ins = w.eqi(tag_ins, w.nameImmui(JSVAL_TAG_OBJECT));
    *obj_ins = w.ldiValuePayload(addr);
}

LIns*
TraceRecorder::is_boxed_true(Address addr)
{
    LIns *tag_ins = w.ldiValueTag(addr);
    LIns *bool_ins = w.eqi(tag_ins, w.nameImmui(JSVAL_TAG_BOOLEAN));
    LIns *payload_ins = w.ldiValuePayload(addr);
    return w.andi(bool_ins, payload_ins);
}

LIns*
TraceRecorder::is_boxed_magic(Address addr, JSWhyMagic why)
{
    LIns *tag_ins = w.ldiValueTag(addr);
    return w.eqi(tag_ins, w.nameImmui(JSVAL_TAG_MAGIC));
}

void
TraceRecorder::box_value_into(const Value &v, LIns *v_ins, Address addr)
{
    if (v.isNumber()) {
        JS_ASSERT(v_ins->isD());
        if (fcallinfo(v_ins) == &js_UnboxDouble_ci) {
            w.stiValueTag(v_ins->callArgN(0), addr);
            w.stiValuePayload(v_ins->callArgN(1), addr);
        } else if (IsPromoteInt(v_ins)) {
            LIns *int_ins = w.demote(v_ins);
            w.stiValueTag(w.nameImmui(JSVAL_TAG_INT32), addr);
            w.stiValuePayload(int_ins, addr);
        } else {
            w.std(v_ins, addr);
        }
        return;
    }

    if (v.isUndefined()) {
        box_undefined_into(addr);
    } else if (v.isNull()) {
        box_null_into(addr);
    } else {
        JSValueTag tag = v.isObject() ? JSVAL_TAG_OBJECT : v.extractNonDoubleObjectTraceTag();
        w.stiValueTag(w.nameImmui(tag), addr);
        w.stiValuePayload(v_ins, addr);
    }
}

LIns*
TraceRecorder::box_value_for_native_call(const Value &v, LIns *v_ins)
{
    return box_value_into_alloc(v, v_ins);
}

#elif JS_BITS_PER_WORD == 64

void
TraceRecorder::box_undefined_into(Address addr)
{
    w.stq(w.nameImmq(JSVAL_BITS(JSVAL_VOID)), addr);
}

inline LIns *
TraceRecorder::non_double_object_value_has_type(LIns *v_ins, JSValueType type)
{
    return w.eqi(w.q2i(w.rshuqN(v_ins, JSVAL_TAG_SHIFT)),
                 w.nameImmui(JSVAL_TYPE_TO_TAG(type)));
}

inline LIns *
TraceRecorder::unpack_ptr(LIns *v_ins)
{
    return w.andq(v_ins, w.nameImmq(JSVAL_PAYLOAD_MASK));
}

inline LIns *
TraceRecorder::unbox_number_as_double(LIns *v_ins, VMSideExit *exit)
{
    guard(true,
          w.ltuq(v_ins, w.nameImmq(JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET)),
          exit);
    LIns* args[] = { v_ins };
    return w.call(&js_UnboxDouble_ci, args);
}

inline nanojit::LIns*
TraceRecorder::unbox_non_double_object(LIns* v_ins, JSValueType type, VMSideExit* exit)
{
    JS_ASSERT(type <= JSVAL_UPPER_INCL_TYPE_OF_VALUE_SET);
    LIns *unboxed_ins;
    if (type == JSVAL_TYPE_UNDEFINED) {
        unboxed_ins = w.immiUndefined();
    } else if (type == JSVAL_TYPE_NULL) {
        unboxed_ins = w.immpNull();
    } else if (type >= JSVAL_LOWER_INCL_TYPE_OF_PTR_PAYLOAD_SET) {
        unboxed_ins = unpack_ptr(v_ins);
    } else {
        JS_ASSERT(type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN || type == JSVAL_TYPE_MAGIC);
        unboxed_ins = w.q2i(v_ins);
    }

    guard(true, non_double_object_value_has_type(v_ins, type), exit);
    return unboxed_ins;
}

LIns*
TraceRecorder::unbox_object(LIns* v_ins, JSValueType type, VMSideExit* exit)
{
    JS_STATIC_ASSERT(JSVAL_TYPE_OBJECT == JSVAL_UPPER_INCL_TYPE_OF_VALUE_SET);
    JS_ASSERT(type == JSVAL_TYPE_FUNOBJ || type == JSVAL_TYPE_NONFUNOBJ);
    guard(true,
          w.geuq(v_ins, w.nameImmq(JSVAL_SHIFTED_TAG_OBJECT)),
          exit);
    v_ins = unpack_ptr(v_ins);
    if (type == JSVAL_TYPE_FUNOBJ)
        guardClass(v_ins, &js_FunctionClass, exit, LOAD_NORMAL);
    else
        guardNotClass(v_ins, &js_FunctionClass, exit, LOAD_NORMAL);
    return v_ins;
}

LIns*
TraceRecorder::unbox_value(const Value &v, Address addr, VMSideExit *exit, bool force_double)
{
    LIns *v_ins = w.ldq(addr);

    if (v.isNumber() && force_double)
        return unbox_number_as_double(v_ins, exit);

    if (v.isInt32()) {
        guard(true, non_double_object_value_has_type(v_ins, JSVAL_TYPE_INT32), exit);
        return w.i2d(w.q2i(v_ins));
    }

    if (v.isDouble()) {
        guard(true, w.leuq(v_ins, w.nameImmq(JSVAL_SHIFTED_TAG_MAX_DOUBLE)), exit);
        return w.qasd(v_ins);
    }

    if (v.isObject()) {
        JSValueType type = v.toObject().isFunction() ? JSVAL_TYPE_FUNOBJ : JSVAL_TYPE_NONFUNOBJ;
        return unbox_object(v_ins, type, exit);
    }

    JSValueType type = v.extractNonDoubleObjectTraceType();
    return unbox_non_double_object(v_ins, type, exit);
}

void
TraceRecorder::unbox_any_object(Address addr, LIns **obj_ins, LIns **is_obj_ins)
{
    JS_STATIC_ASSERT(JSVAL_TYPE_OBJECT == JSVAL_UPPER_INCL_TYPE_OF_VALUE_SET);
    LIns *v_ins = w.ldq(addr);
    *is_obj_ins = w.geuq(v_ins, w.nameImmq(JSVAL_TYPE_OBJECT));
    *obj_ins = unpack_ptr(v_ins);
}

LIns*
TraceRecorder::is_boxed_true(Address addr)
{
    LIns *v_ins = w.ldq(addr);
    return w.eqq(v_ins, w.immq(JSVAL_BITS(JSVAL_TRUE)));
}

LIns*
TraceRecorder::is_boxed_magic(Address addr, JSWhyMagic why)
{
    LIns *v_ins = w.ldq(addr);
    return w.eqq(v_ins, w.nameImmq(BUILD_JSVAL(JSVAL_TAG_MAGIC, why)));
}

LIns*
TraceRecorder::box_value_for_native_call(const Value &v, LIns *v_ins)
{
    if (v.isNumber()) {
        JS_ASSERT(v_ins->isD());
        if (fcallinfo(v_ins) == &js_UnboxDouble_ci)
            return v_ins->callArgN(0);
        if (IsPromoteInt(v_ins)) {
            return w.orq(w.ui2uq(w.demote(v_ins)),
                         w.nameImmq(JSVAL_SHIFTED_TAG_INT32));
        }
        return w.dasq(v_ins);
    }

    if (v.isNull())
        return w.nameImmq(JSVAL_BITS(JSVAL_NULL));
    if (v.isUndefined())
        return w.nameImmq(JSVAL_BITS(JSVAL_VOID));

    JSValueTag tag = v.isObject() ? JSVAL_TAG_OBJECT : v.extractNonDoubleObjectTraceTag();
    uint64 shiftedTag = ((uint64)tag) << JSVAL_TAG_SHIFT;
    LIns *shiftedTag_ins = w.nameImmq(shiftedTag);

    if (v.hasPtrPayload())
        return w.orq(v_ins, shiftedTag_ins);
    return w.orq(w.ui2uq(v_ins), shiftedTag_ins);
}

void
TraceRecorder::box_value_into(const Value &v, LIns *v_ins, Address addr)
{
    LIns *boxed_ins = box_value_for_native_call(v, v_ins);
    w.st(boxed_ins, addr);
}

#endif  /* JS_BITS_PER_WORD */

LIns*
TraceRecorder::box_value_into_alloc(const Value &v, LIns *v_ins)
{
    LIns *alloc_ins = w.allocp(sizeof(Value));
    box_value_into(v, v_ins, AllocSlotsAddress(alloc_ins));
    return alloc_ins;
}

LIns*
TraceRecorder::is_string_id(LIns *id_ins)
{
    return w.eqp0(w.andp(id_ins, w.nameImmw(JSID_TYPE_MASK)));
}

LIns *
TraceRecorder::unbox_string_id(LIns *id_ins)
{
    JS_STATIC_ASSERT(JSID_TYPE_STRING == 0);
    return id_ins;
}

LIns *
TraceRecorder::unbox_int_id(LIns *id_ins)
{
    return w.rshiN(w.p2i(id_ins), 1);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getThis(LIns*& this_ins)
{
    JSStackFrame *fp = cx->fp();

    if (fp->isGlobalFrame()) {
        // Top-level code. It is an invariant of the interpreter that fp->thisv
        // is non-null. Furthermore, we would not be recording if globalObj
        // were not at the end of the scope chain, so `this` can only be one
        // object, which we can burn into the trace.
        JS_ASSERT(!fp->thisValue().isPrimitive());

#ifdef DEBUG
        JSObject *obj = globalObj->thisObject(cx);
        if (!obj)
            RETURN_ERROR("thisObject hook failed");
        JS_ASSERT(&fp->thisValue().toObject() == obj);
#endif

        this_ins = w.immpObjGC(&fp->thisValue().toObject());
        return RECORD_CONTINUE;
    }

    JS_ASSERT(fp->callee().getGlobal() == globalObj);    
    Value& thisv = fp->thisValue();

    if (fp->fun()->inStrictMode() || thisv.isObject()) {
        /*
         * fp->thisValue() has already been computed. Since the
         * type-specialization of traces distinguishes between computed and
         * uncomputed |this|, the same will be true at run time (or we
         * won't get this far).
         */
        this_ins = get(&fp->thisValue());
        return RECORD_CONTINUE;
    }

    /* Don't bother tracing calls on wrapped primitive |this| values. */
    if (!thisv.isNullOrUndefined())
        RETURN_STOP("wrapping primitive |this|");

    /*
     * Compute 'this' now. The result is globalObj->thisObject(), which is
     * trace-constant. getThisObject writes back to fp->thisValue(), so do
     * the same on trace.
     */
    if (!fp->computeThis(cx))
        RETURN_ERROR("computeThis failed");

    /* thisv is a reference, so it'll see the newly computed |this|. */
    this_ins = w.immpObjGC(globalObj);
    set(&thisv, this_ins);
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK void
TraceRecorder::guardClassHelper(bool cond, LIns* obj_ins, Class* clasp, VMSideExit* exit,
                                LoadQual loadQual)
{
    LIns* class_ins = w.ldpObjClasp(obj_ins, loadQual);

#ifdef JS_JIT_SPEW
    char namebuf[32];
    JS_snprintf(namebuf, sizeof namebuf, "guard(class is %s)", clasp->name);
#else
    static const char namebuf[] = "";
#endif
    guard(cond, w.name(w.eqp(class_ins, w.nameImmpNonGC(clasp)), namebuf), exit);
}

JS_REQUIRES_STACK void
TraceRecorder::guardClass(LIns* obj_ins, Class* clasp, VMSideExit* exit, LoadQual loadQual)
{
    guardClassHelper(true, obj_ins, clasp, exit, loadQual);
}

JS_REQUIRES_STACK void
TraceRecorder::guardNotClass(LIns* obj_ins, Class* clasp, VMSideExit* exit, LoadQual loadQual)
{
    guardClassHelper(false, obj_ins, clasp, exit, loadQual);
}

JS_REQUIRES_STACK void
TraceRecorder::guardDenseArray(LIns* obj_ins, ExitType exitType)
{
    guardClass(obj_ins, &js_ArrayClass, snapshot(exitType), LOAD_NORMAL);
}

JS_REQUIRES_STACK void
TraceRecorder::guardDenseArray(LIns* obj_ins, VMSideExit* exit)
{
    guardClass(obj_ins, &js_ArrayClass, exit, LOAD_NORMAL);
}

JS_REQUIRES_STACK bool
TraceRecorder::guardHasPrototype(JSObject* obj, LIns* obj_ins,
                                 JSObject** pobj, LIns** pobj_ins,
                                 VMSideExit* exit)
{
    *pobj = obj->getProto();
    *pobj_ins = w.ldpObjProto(obj_ins);

    bool cond = *pobj == NULL;
    guard(cond, w.name(w.eqp0(*pobj_ins), "guard(proto-not-null)"), exit);
    return !cond;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardPrototypeHasNoIndexedProperties(JSObject* obj, LIns* obj_ins, VMSideExit *exit)
{
    /*
     * Guard that no object along the prototype chain has any indexed
     * properties which might become visible through holes in the array.
     */
    if (js_PrototypeHasIndexedProperties(cx, obj))
        return RECORD_STOP;

    while (guardHasPrototype(obj, obj_ins, &obj, &obj_ins, exit))
        CHECK_STATUS(guardShape(obj_ins, obj, obj->shape(), "guard(shape)", exit));
    return RECORD_CONTINUE;
}

/*
 * Guard that the object stored in v has the ECMA standard [[DefaultValue]]
 * method. Several imacros require this.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardNativeConversion(Value& v)
{
    JSObject* obj = &v.toObject();
    LIns* obj_ins = get(&v);

    ConvertOp convert = obj->getClass()->convert;
    if (convert != Valueify(JS_ConvertStub) && convert != js_TryValueOf)
        RETURN_STOP("operand has convert hook");

    VMSideExit* exit = snapshot(BRANCH_EXIT);
    if (obj->isNative()) {
        // The common case. Guard on shape rather than class because it'll
        // often be free: we're about to do a shape guard anyway to get the
        // .valueOf property of this object, and this guard will be cached.
        CHECK_STATUS(guardShape(obj_ins, obj, obj->shape(),
                                "guardNativeConversion", exit));
    } else {
        // We could specialize to guard on just JSClass.convert, but a mere
        // class guard is simpler and slightly faster.
        guardClass(obj_ins, obj->getClass(), snapshot(MISMATCH_EXIT), LOAD_NORMAL);
    }
    return RECORD_CONTINUE;
}

/*
 * Clear out all slots of this frame in the nativeFrameTracker. Different
 * locations on the VM stack might map to different locations on the native
 * stack depending on the number of arguments (i.e.) of the next call, so we
 * have to make sure we map those in to the cache with the right offsets.
 */
JS_REQUIRES_STACK void
TraceRecorder::clearCurrentFrameSlotsFromTracker(Tracker& which)
{
    JSStackFrame *const fp = cx->fp();

    /*
     * Duplicate native stack layout computation: see VisitFrameSlots header comment.
     * This doesn't do layout arithmetic, but it must clear out all the slots defined as
     * imported by VisitFrameSlots.
     */
    if (fp->isGlobalFrame()) {
        Value *vp = fp->slots() + fp->globalScript()->nfixed;
        Value *vpend = fp->slots() + fp->globalScript()->nslots;
        for (; vp < vpend; ++vp)
            which.set(vp, (LIns*)0);
        return;
    }

    if (!fp->isEvalFrame()) {
        /* For simplicitly, flush 'em all, even non-canonical arg slots. */
        Value *vp = fp->actualArgs() - 2 /* callee, this */;
        Value *vpend = fp->formalArgsEnd();
        for (; vp < vpend; ++vp)
            which.set(vp, (LIns*)0);
    }

    which.set(fp->addressOfArgs(), (LIns*)0);
    which.set(fp->addressOfScopeChain(), (LIns*)0);

    Value *vp = fp->slots();
    Value *vpend = fp->slots() + fp->functionScript()->nslots;
    for (; vp < vpend; ++vp)
        which.set(vp, (LIns*)0);
}

class BoxArg
{
  public:
    BoxArg(TraceRecorder *tr, Address addr)
        : tr(tr), addr(addr) {}
    TraceRecorder *tr;
    Address addr;
    void operator()(uintN argi, Value *src) {
        tr->box_value_into(*src, tr->get(src), OffsetAddress(addr, argi * sizeof(Value)));
    }
};

/*
 * If we have created an |arguments| object for the frame, we must copy the
 * argument values into the object as properties in case it is used after
 * this frame returns.
 */
JS_REQUIRES_STACK void
TraceRecorder::putActivationObjects()
{
    JSStackFrame *const fp = cx->fp();
    bool have_args = fp->hasArgsObj() && !fp->argsObj().isStrictArguments();
    bool have_call = fp->isFunctionFrame() && fp->fun()->isHeavyweight();

    if (!have_args && !have_call)
        return;

    uintN nformal = fp->numFormalArgs();
    uintN nactual = fp->numActualArgs();
    uintN nargs = have_args && have_call ? Max(nformal, nactual)
                                         : have_args ? nactual : nformal;

    LIns *args_ins;
    if (nargs > 0) {
        args_ins = w.allocp(nargs * sizeof(Value));
        /* Don't copy all the actuals if we are only boxing for the callobj. */
        Address addr = AllocSlotsAddress(args_ins);
        if (nargs == nactual)
            fp->forEachCanonicalActualArg(BoxArg(this, addr));
        else
            fp->forEachFormalArg(BoxArg(this, addr));
    } else {
        args_ins = w.nameImmpNonGC(0);
    }

    if (have_args) {
        LIns* argsobj_ins = getFrameObjPtr(fp->addressOfArgs());
        LIns* args[] = { args_ins, argsobj_ins, cx_ins };
        w.call(&js_PutArgumentsOnTrace_ci, args);
    }

    if (have_call) {
        int nslots = fp->fun()->countVars();
        LIns* slots_ins;
        if (nslots) {
            slots_ins = w.allocp(sizeof(Value) * nslots);
            for (int i = 0; i < nslots; ++i) {
                box_value_into(fp->slots()[i], get(&fp->slots()[i]), 
                               AllocSlotsAddress(slots_ins, i));
            }
        } else {
            slots_ins = w.nameImmpNonGC(0);
        }

        LIns* scopeChain_ins = getFrameObjPtr(fp->addressOfScopeChain());
        LIns* args[] = { slots_ins, w.nameImmi(nslots), args_ins,
                         w.nameImmi(fp->numFormalArgs()), scopeChain_ins, cx_ins };
        w.call(&js_PutCallObjectOnTrace_ci, args);
    }
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_EnterFrame()
{
    JSStackFrame* const fp = cx->fp();

    if (++callDepth >= MAX_CALLDEPTH)
        RETURN_STOP_A("exceeded maximum call depth");

    debug_only_printf(LC_TMTracer, "EnterFrame %s, callDepth=%d\n",
                      js_AtomToPrintableString(cx, cx->fp()->fun()->atom),
                      callDepth);
    debug_only_stmt(
        if (LogController.lcbits & LC_TMRecorder) {
            js_Disassemble(cx, cx->fp()->script(), JS_TRUE, stdout);
            debug_only_print0(LC_TMTracer, "----\n");
        }
    )
    LIns* void_ins = w.immiUndefined();

    // Before we enter this frame, we need to clear out any dangling insns left
    // in the tracer. While we also clear when returning from a function, it is
    // possible to have the following sequence of stack usage:
    //
    //  [fp1]*****************   push
    //  [fp1]*****               pop
    //  [fp1]*****[fp2]          call
    //  [fp1]*****[fp2]***       push
    //
    // Duplicate native stack layout computation: see VisitFrameSlots header comment.

    // args: carefully copy stack layout
    uintN nactual = fp->numActualArgs();
    uintN nformal = fp->numFormalArgs();
    if (nactual < nformal) {
        // Fill in missing with void.
        JS_ASSERT(fp->actualArgs() == fp->formalArgs());
        Value *beg = fp->formalArgs() + nactual;
        Value *end = fp->formalArgsEnd();
        for (Value *vp = beg; vp != end; ++vp) {
            nativeFrameTracker.set(vp, NULL);
            set(vp, void_ins);
        }
    } else if (nactual > nformal) {
        // Although the VM clones the formal args to the top of the stack, due
        // to the fact that we only track the canonical arguments (in argument
        // order), the native stack offset of the arguments doesn't change. The
        // only thing that changes is which js::Value* in the tracker maps to
        // that slot. Thus, all we need to do here is fixup the trackers, not
        // emit any actual copying on trace.
        JS_ASSERT(fp->actualArgs() != fp->formalArgs());
        JS_ASSERT(fp->hasOverflowArgs());
        Value *srcbeg = fp->actualArgs() - 2;
        Value *srcend = fp->actualArgs() + nformal;
        Value *dstbeg = fp->formalArgs() - 2;
        for (Value *src = srcbeg, *dst = dstbeg; src != srcend; ++src, ++dst) {
            nativeFrameTracker.set(dst, NULL);
            tracker.set(dst, tracker.get(src));
            nativeFrameTracker.set(src, NULL);
            tracker.set(src, NULL);
        }
    }

    // argsObj: clear and set to null
    nativeFrameTracker.set(fp->addressOfArgs(), NULL);
    setFrameObjPtr(fp->addressOfArgs(), w.immpNull());

    // scopeChain: clear, initialize before snapshot, set below
    nativeFrameTracker.set(fp->addressOfScopeChain(), NULL);
    setFrameObjPtr(fp->addressOfScopeChain(), w.immpNull());

    // nfixed: clear and set to undefined
    Value *vp = fp->slots();
    Value *vpstop = vp + fp->numFixed();
    for (; vp < vpstop; ++vp) {
        nativeFrameTracker.set(vp, NULL);
        set(vp, void_ins);
    }

    // nfixed to nslots: clear
    vp = fp->base();
    vpstop = fp->slots() + fp->numSlots();
    for (; vp < vpstop; ++vp)
        nativeFrameTracker.set(vp, NULL);

    LIns* callee_ins = get(&cx->fp()->calleeValue());
    LIns* scopeChain_ins = w.ldpObjParent(callee_ins);

    // set scopeChain for real
    if (cx->fp()->fun()->isHeavyweight()) {
        if (js_IsNamedLambda(cx->fp()->fun()))
            RETURN_STOP_A("can't call named lambda heavyweight on trace");

        LIns* fun_ins = w.nameImmpNonGC(cx->fp()->fun());

        LIns* args[] = { scopeChain_ins, callee_ins, fun_ins, cx_ins };
        LIns* call_ins = w.call(&js_CreateCallObjectOnTrace_ci, args);
        guard(false, w.eqp0(call_ins), OOM_EXIT);

        setFrameObjPtr(fp->addressOfScopeChain(), call_ins);
    } else {
        setFrameObjPtr(fp->addressOfScopeChain(), scopeChain_ins);
    }

    /* Try inlining one level in case this recursion doesn't go too deep. */
    if (fp->script() == fp->prev()->script() &&
        fp->prev()->prev() && fp->prev()->prev()->script() == fp->script()) {
        RETURN_STOP_A("recursion started inlining");
    }

    if (fp->isConstructing()) {
        LIns* args[] = { callee_ins, w.nameImmpNonGC(&js_ObjectClass), cx_ins };
        LIns* tv_ins = w.call(&js_CreateThisFromTrace_ci, args);
        guard(false, w.eqp0(tv_ins), OOM_EXIT);
        set(&fp->thisValue(), tv_ins);
    }

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_LeaveFrame()
{
    debug_only_stmt(JSStackFrame *fp = cx->fp();)

    JS_ASSERT(js_CodeSpec[js_GetOpcode(cx, fp->script(),
              cx->regs->pc)].length == JSOP_CALL_LENGTH);

    if (callDepth-- <= 0)
        RETURN_STOP_A("returned out of a loop we started tracing");

    // LeaveFrame gets called after the interpreter popped the frame and
    // stored rval, so cx->fp() not cx->fp()->prev, and -1 not 0.
    updateAtoms();
    set(&stackval(-1), rval_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PUSH()
{
    stack(0, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POPV()
{
    Value& rval = stackval(-1);

    // Store it in cx->fp()->rval. NB: Tricky dependencies. cx->fp() is the right
    // frame because POPV appears only in global and eval code and we don't
    // trace JSOP_EVAL or leaving the frame where tracing started.
    LIns *fp_ins = entryFrameIns();
    box_value_into(rval, get(&rval), StackFrameAddress(fp_ins,
                                                       JSStackFrame::offsetOfReturnValue()));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENTERWITH()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LEAVEWITH()
{
    return ARECORD_STOP;
}

#ifdef MOZ_TRACE_JSCALLS
// Usually, cx->doFunctionCallback() is invoked via DTrace::enterJSFun
// and friends, but the DTrace:: probes use fp and therefore would
// need to break out of tracing. So we define a functionProbe()
// callback to be called by generated code when a Javascript function
// is entered or exited.
static JSBool JS_FASTCALL
functionProbe(JSContext *cx, JSFunction *fun, JSBool enter)
{
    cx->doFunctionCallback(fun, FUN_SCRIPT(fun), enter);
    return true;
}

JS_DEFINE_CALLINFO_3(static, BOOL, functionProbe, CONTEXT, FUNCTION, BOOL,
                     0, ACCSET_STORE_ANY)
#endif

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RETURN()
{
    /* A return from callDepth 0 terminates the current loop, except for recursion. */
    if (callDepth == 0) {
        AUDIT(returnLoopExits);
        return endLoop();
    }

    putActivationObjects();

#ifdef MOZ_TRACE_JSCALLS
    if (cx->functionCallback) {
        LIns* args[] = { w.nameImmi(0), w.nameImmpNonGC(cx->fp()->fun()), cx_ins };
        LIns* call_ins = w.call(&functionProbe_ci, args);
        guard(false, w.eqi0(call_ins), MISMATCH_EXIT);
    }
#endif

    /* If we inlined this function call, make the return value available to the caller code. */
    Value& rval = stackval(-1);
    JSStackFrame *fp = cx->fp();
    if (fp->isConstructing() && rval.isPrimitive()) {
        rval_ins = get(&fp->thisValue());
    } else {
        rval_ins = get(&rval);
    }
    debug_only_printf(LC_TMTracer,
                      "returning from %s\n",
                      js_AtomToPrintableString(cx, fp->fun()->atom));
    clearCurrentFrameSlotsFromTracker(nativeFrameTracker);

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GOTO()
{
    /*
     * If we hit a break or a continue to an outer loop, end the loop and
     * generate an always-taken loop exit guard.  For other downward gotos
     * (like if/else) continue recording.
     */
    jssrcnote* sn = js_GetSrcNote(cx->fp()->script(), cx->regs->pc);

    if (sn && (SN_TYPE(sn) == SRC_BREAK || SN_TYPE(sn) == SRC_CONT2LABEL)) {
        AUDIT(breakLoopExits);
        return endLoop();
    }
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFEQ()
{
    trackCfgMerges(cx->regs->pc);
    return ifop();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFNE()
{
    return ifop();
}

LIns*
TraceRecorder::newArguments(LIns* callee_ins, bool strict)
{
    LIns* global_ins = w.immpObjGC(globalObj);
    LIns* argc_ins = w.nameImmi(cx->fp()->numActualArgs());

    LIns* args[] = { callee_ins, argc_ins, global_ins, cx_ins };
    LIns* argsobj_ins = w.call(&js_NewArgumentsOnTrace_ci, args);
    guard(false, w.eqp0(argsobj_ins), OOM_EXIT);

    if (strict) {
        LIns* argsData_ins = w.getObjPrivatizedSlot(argsobj_ins, JSObject::JSSLOT_ARGS_DATA);
        ptrdiff_t slotsOffset = offsetof(ArgumentsData, slots);
        cx->fp()->forEachCanonicalActualArg(BoxArg(this, ArgsSlotsAddress(argsData_ins,
                                                                          slotsOffset)));
    }

    return argsobj_ins;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGUMENTS()
{
    JSStackFrame* const fp = cx->fp();

    /* In an eval, 'arguments' will be a BINDNAME, which we don't trace. */
    JS_ASSERT(!fp->isEvalFrame());

    if (fp->hasOverriddenArgs())
        RETURN_STOP_A("Can't trace |arguments| if |arguments| is assigned to");

    LIns* a_ins = getFrameObjPtr(fp->addressOfArgs());
    LIns* args_ins;
    LIns* callee_ins = get(&fp->calleeValue());
    bool strict = fp->fun()->inStrictMode();
    if (a_ins->isImmP()) {
        // |arguments| is set to 0 by EnterFrame on this trace, so call to create it.
        args_ins = newArguments(callee_ins, strict);
    } else {
        // Generate LIR to create arguments only if it has not already been created.

        LIns* mem_ins = w.allocp(sizeof(JSObject *));

        LIns* isZero_ins = w.eqp0(a_ins);
        if (isZero_ins->isImmI(0)) {
            w.stAlloc(a_ins, mem_ins);
        } else if (isZero_ins->isImmI(1)) {
            LIns* call_ins = newArguments(callee_ins, strict);
            w.stAlloc(call_ins, mem_ins);
        } else {
            LIns* br1 = w.jtUnoptimizable(isZero_ins);
            w.stAlloc(a_ins, mem_ins);
            LIns* br2 = w.j(NULL);
            w.label(br1);

            LIns* call_ins = newArguments(callee_ins, strict);
            w.stAlloc(call_ins, mem_ins);
            w.label(br2);
        }
        args_ins = w.ldpAlloc(mem_ins);
    }

    stack(0, args_ins);
    setFrameObjPtr(fp->addressOfArgs(), args_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DUP()
{
    stack(0, get(&stackval(-1)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DUP2()
{
    stack(0, get(&stackval(-2)));
    stack(1, get(&stackval(-1)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SWAP()
{
    Value& l = stackval(-2);
    Value& r = stackval(-1);
    LIns* l_ins = get(&l);
    LIns* r_ins = get(&r);
    set(&r, l_ins);
    set(&l, r_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PICK()
{
    Value* sp = cx->regs->sp;
    jsint n = cx->regs->pc[1];
    JS_ASSERT(sp - (n+1) >= cx->fp()->base());
    LIns* top = get(sp - (n+1));
    for (jsint i = 0; i < n; ++i)
        set(sp - (n+1) + i, get(sp - n + i));
    set(&sp[-1], top);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETCONST()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITOR()
{
    return InjectStatus(binary(LIR_ori));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITXOR()
{
    return InjectStatus(binary(LIR_xori));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITAND()
{
    return InjectStatus(binary(LIR_andi));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_EQ()
{
    return equality(false, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NE()
{
    return equality(true, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LT()
{
    return relational(LIR_ltd, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LE()
{
    return relational(LIR_led, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GT()
{
    return relational(LIR_gtd, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GE()
{
    return relational(LIR_ged, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LSH()
{
    return InjectStatus(binary(LIR_lshi));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RSH()
{
    return InjectStatus(binary(LIR_rshi));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_URSH()
{
    return InjectStatus(binary(LIR_rshui));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ADD()
{
    Value& r = stackval(-1);
    Value& l = stackval(-2);

    if (!l.isPrimitive()) {
        CHECK_STATUS_A(guardNativeConversion(l));
        if (!r.isPrimitive()) {
            CHECK_STATUS_A(guardNativeConversion(r));
            return InjectStatus(callImacro(add_imacros.obj_obj));
        }
        return InjectStatus(callImacro(add_imacros.obj_any));
    }
    if (!r.isPrimitive()) {
        CHECK_STATUS_A(guardNativeConversion(r));
        return InjectStatus(callImacro(add_imacros.any_obj));
    }

    if (l.isString() || r.isString()) {
        LIns* args[] = { stringify(r), stringify(l), cx_ins };
        LIns* concat = w.call(&js_ConcatStrings_ci, args);
        guard(false, w.eqp0(concat), OOM_EXIT);
        set(&l, concat);
        return ARECORD_CONTINUE;
    }

    return InjectStatus(binary(LIR_addd));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SUB()
{
    return InjectStatus(binary(LIR_subd));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_MUL()
{
    return InjectStatus(binary(LIR_muld));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DIV()
{
    return InjectStatus(binary(LIR_divd));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_MOD()
{
    return InjectStatus(binary(LIR_modd));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NOT()
{
    Value& v = stackval(-1);
    if (v.isBoolean() || v.isUndefined()) {
        set(&v, w.eqi0(w.eqiN(get(&v), 1)));
        return ARECORD_CONTINUE;
    }
    if (v.isNumber()) {
        LIns* v_ins = get(&v);
        set(&v, w.ori(w.eqd0(v_ins), w.eqi0(w.eqd(v_ins, v_ins))));
        return ARECORD_CONTINUE;
    }
    if (v.isObjectOrNull()) {
        set(&v, w.eqp0(get(&v)));
        return ARECORD_CONTINUE;
    }
    JS_ASSERT(v.isString());
    set(&v, w.eqp0(w.getStringLength(get(&v))));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITNOT()
{
    return InjectStatus(unary(LIR_noti));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEG()
{
    Value& v = stackval(-1);

    if (!v.isPrimitive()) {
        CHECK_STATUS_A(guardNativeConversion(v));
        return InjectStatus(callImacro(unary_imacros.sign));
    }

    if (v.isNumber()) {
        LIns* a = get(&v);

        /*
         * If we're a promoted integer, we have to watch out for 0s since -0 is
         * a double. Only follow this path if we're not an integer that's 0 and
         * we're not a double that's zero.
         */
        if (oracle &&
            !oracle->isInstructionUndemotable(cx->regs->pc) &&
            IsPromoteInt(a) &&
            (!v.isInt32() || v.toInt32() != 0) &&
            (!v.isDouble() || v.toDouble() != 0) &&
            -v.toNumber() == (int)-v.toNumber())
        {
            VMSideExit* exit = snapshot(OVERFLOW_EXIT);
            a = guard_xov(LIR_subi, w.immi(0), w.demote(a), exit);
            if (!a->isImmI() && a->isop(LIR_subxovi)) {
                guard(false, w.eqiN(a, 0), exit); // make sure we don't lose a -0
            }
            a = w.i2d(a);
        } else {
            a = w.negd(a);
        }

        set(&v, a);
        return ARECORD_CONTINUE;
    }

    if (v.isNull()) {
        set(&v, w.immd(-0.0));
        return ARECORD_CONTINUE;
    }

    if (v.isUndefined()) {
        set(&v, w.immd(js_NaN));
        return ARECORD_CONTINUE;
    }

    if (v.isString()) {
        LIns* args[] = { get(&v), cx_ins };
        set(&v, w.negd(w.call(&js_StringToNumber_ci, args)));
        return ARECORD_CONTINUE;
    }

    JS_ASSERT(v.isBoolean());
    set(&v, w.negd(w.i2d(get(&v))));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POS()
{
    Value& v = stackval(-1);

    if (!v.isPrimitive()) {
        CHECK_STATUS_A(guardNativeConversion(v));
        return InjectStatus(callImacro(unary_imacros.sign));
    }

    if (v.isNumber())
        return ARECORD_CONTINUE;

    if (v.isNull()) {
        set(&v, w.immd(0));
        return ARECORD_CONTINUE;
    }
    if (v.isUndefined()) {
        set(&v, w.immd(js_NaN));
        return ARECORD_CONTINUE;
    }

    if (v.isString()) {
        LIns* args[] = { get(&v), cx_ins };
        set(&v, w.call(&js_StringToNumber_ci, args));
        return ARECORD_CONTINUE;
    }

    JS_ASSERT(v.isBoolean());
    set(&v, w.i2d(get(&v)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PRIMTOP()
{
    // Either this opcode does nothing or we couldn't have traced here, because
    // we'd have thrown an exception -- so do nothing if we actually hit this.
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_OBJTOP()
{
    Value& v = stackval(-1);
    RETURN_IF_XML_A(v);
    return ARECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::getClassPrototype(JSObject* ctor, LIns*& proto_ins)
{
    // ctor must be a function created via js_InitClass.
#ifdef DEBUG
    Class *clasp = FUN_CLASP(GET_FUNCTION_PRIVATE(cx, ctor));
    JS_ASSERT(clasp);

    TraceMonitor &localtm = JS_TRACE_MONITOR(cx);
#endif

    Value pval;
    if (!ctor->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom), &pval))
        RETURN_ERROR("error getting prototype from constructor");

    // ctor.prototype is a permanent data property, so this lookup cannot have
    // deep-aborted.
    JS_ASSERT(localtm.recorder);

#ifdef DEBUG
    JSBool ok, found;
    uintN attrs;
    ok = JS_GetPropertyAttributes(cx, ctor, js_class_prototype_str, &attrs, &found);
    JS_ASSERT(ok);
    JS_ASSERT(found);
    JS_ASSERT((~attrs & (JSPROP_READONLY | JSPROP_PERMANENT)) == 0);
#endif

    // Since ctor was built by js_InitClass, we can assert (rather than check)
    // that pval is usable.
    JS_ASSERT(!pval.isPrimitive());
    JSObject *proto = &pval.toObject();
    JS_ASSERT_IF(clasp != &js_ArrayClass, proto->emptyShapes[0]->getClass() == clasp);

    proto_ins = w.immpObjGC(proto);
    return RECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::getClassPrototype(JSProtoKey key, LIns*& proto_ins)
{
#ifdef DEBUG
    TraceMonitor &localtm = JS_TRACE_MONITOR(cx);
#endif

    JSObject* proto;
    if (!js_GetClassPrototype(cx, globalObj, key, &proto))
        RETURN_ERROR("error in js_GetClassPrototype");

    // This should not have reentered.
    JS_ASSERT(localtm.recorder);

#ifdef DEBUG
    /* Double-check that a native proto has a matching emptyShape. */
    if (key != JSProto_Array) {
        JS_ASSERT(proto->isNative());
        JS_ASSERT(proto->emptyShapes);
        EmptyShape *empty = proto->emptyShapes[0];
        JS_ASSERT(empty);
        JS_ASSERT(JSCLASS_CACHED_PROTO_KEY(empty->getClass()) == key);
    }
#endif

    proto_ins = w.immpObjGC(proto);
    return RECORD_CONTINUE;
}

#define IGNORE_NATIVE_CALL_COMPLETE_CALLBACK ((JSSpecializedNative*)1)

RecordingStatus
TraceRecorder::newString(JSObject* ctor, uint32 argc, Value* argv, Value* rval)
{
    JS_ASSERT(argc == 1);

    if (!argv[0].isPrimitive()) {
        CHECK_STATUS(guardNativeConversion(argv[0]));
        return callImacro(new_imacros.String);
    }

    LIns* proto_ins;
    CHECK_STATUS(getClassPrototype(ctor, proto_ins));

    LIns* args[] = { stringify(argv[0]), proto_ins, cx_ins };
    LIns* obj_ins = w.call(&js_String_tn_ci, args);
    guard(false, w.eqp0(obj_ins), OOM_EXIT);

    set(rval, obj_ins);
    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
    return RECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::newArray(JSObject* ctor, uint32 argc, Value* argv, Value* rval)
{
    LIns *proto_ins;
    CHECK_STATUS(getClassPrototype(ctor, proto_ins));

    LIns *arr_ins;
    if (argc == 0 || (argc == 1 && argv[0].isNumber())) {
        LIns *args[] = { argc == 0 ? w.immi(0) : d2i(get(argv)), proto_ins, cx_ins };
        arr_ins = w.call(&js_NewEmptyArray_ci, args);
        guard(false, w.eqp0(arr_ins), OOM_EXIT);
    } else {
        LIns *args[] = { w.nameImmi(argc), proto_ins, cx_ins };
        arr_ins = w.call(&js_NewPreallocatedArray_ci, args);
        guard(false, w.eqp0(arr_ins), OOM_EXIT);

        // arr->slots[i] = box_jsval(vp[i]);  for i in 0..argc
        LIns *slots_ins = NULL;
        for (uint32 i = 0; i < argc && !outOfMemory(); i++) {
            stobj_set_dslot(arr_ins, i, slots_ins, argv[i], get(&argv[i]));
        }
    }

    set(rval, arr_ins);
    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK void
TraceRecorder::propagateFailureToBuiltinStatus(LIns* ok_ins, LIns*& status_ins)
{
    /*
     * Check the boolean return value (ok_ins) of a native JSNative,
     * JSFastNative, or JSPropertyOp hook for failure. On failure, set the
     * BUILTIN_ERROR bit of cx->builtinStatus.
     *
     * If the return value (ok_ins) is true, status' == status. Otherwise
     * status' = status | BUILTIN_ERROR. We calculate (rval&1)^1, which is 1
     * if rval is JS_FALSE (error), and then shift that by 1, which is the log2
     * of BUILTIN_ERROR.
     */
    JS_STATIC_ASSERT(((JS_TRUE & 1) ^ 1) << 1 == 0);
    JS_STATIC_ASSERT(((JS_FALSE & 1) ^ 1) << 1 == BUILTIN_ERROR);
    status_ins = w.ori(status_ins, w.lshiN(w.xoriN(w.andiN(ok_ins, 1), 1), 1));
    w.stStateField(status_ins, builtinStatus);
}

JS_REQUIRES_STACK void
TraceRecorder::emitNativePropertyOp(const Shape* shape, LIns* obj_ins,
                                    bool setflag, LIns* addr_boxed_val_ins)
{
    JS_ASSERT(addr_boxed_val_ins->isop(LIR_allocp));
    JS_ASSERT(setflag ? !shape->hasSetterValue() : !shape->hasGetterValue());
    JS_ASSERT(setflag ? !shape->hasDefaultSetter() : !shape->hasDefaultGetterOrIsMethod());

    enterDeepBailCall();

    w.stStateField(addr_boxed_val_ins, nativeVp);
    w.stStateField(w.nameImmi(1), nativeVpLen);

    CallInfo* ci = new (traceAlloc()) CallInfo();
    ci->_address = uintptr_t(setflag ? shape->setterOp() : shape->getterOp());
    ci->_typesig = CallInfo::typeSig4(ARGTYPE_I, ARGTYPE_P, ARGTYPE_P, ARGTYPE_P, ARGTYPE_P);
    ci->_isPure = 0;
    ci->_storeAccSet = ACCSET_STORE_ANY;
    ci->_abi = ABI_CDECL;
#ifdef DEBUG
    ci->_name = "JSPropertyOp";
#endif
    LIns* args[] = { addr_boxed_val_ins, w.immpIdGC(SHAPE_USERID(shape)), obj_ins, cx_ins };
    LIns* ok_ins = w.call(ci, args);

    // Cleanup. Immediately clear nativeVp before we might deep bail.
    w.stStateField(w.immpNull(), nativeVp);
    leaveDeepBailCall();

    // Guard that the call succeeded and builtinStatus is still 0.
    // If the native op succeeds but we deep-bail here, the result value is
    // lost!  Therefore this can only be used for setters of shared properties.
    // In that case we ignore the result value anyway.
    LIns* status_ins = w.ldiStateField(builtinStatus);
    propagateFailureToBuiltinStatus(ok_ins, status_ins);
    guard(true, w.eqi0(status_ins), STATUS_EXIT);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::emitNativeCall(JSSpecializedNative* sn, uintN argc, LIns* args[], bool rooted)
{
    if (JSTN_ERRTYPE(sn) == FAIL_STATUS) {
        // This needs to capture the pre-call state of the stack. So do not set
        // pendingSpecializedNative before taking this snapshot.
        JS_ASSERT(!pendingSpecializedNative);

        // Take snapshot for DeepBail and store it in cx->bailExit.
        enterDeepBailCall();
    }

    LIns* res_ins = w.call(sn->builtin, args);

    // Immediately unroot the vp as soon we return since we might deep bail next.
    if (rooted)
        w.stStateField(w.immpNull(), nativeVp);

    rval_ins = res_ins;
    switch (JSTN_ERRTYPE(sn)) {
      case FAIL_NULL:
        guard(false, w.eqp0(res_ins), OOM_EXIT);
        break;
      case FAIL_NEG:
        res_ins = w.i2d(res_ins);
        guard(false, w.ltdN(res_ins, 0), OOM_EXIT);
        break;
      case FAIL_NEITHER:
          guard(false, w.eqiN(res_ins, JS_NEITHER), OOM_EXIT);
        break;
      default:;
    }

    set(&stackval(0 - (2 + argc)), res_ins);

    /*
     * The return value will be processed by NativeCallComplete since
     * we have to know the actual return value type for calls that return
     * jsval.
     */
    pendingSpecializedNative = sn;

    return RECORD_CONTINUE;
}

/*
 * Check whether we have a specialized implementation for this native
 * invocation.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callSpecializedNative(JSNativeTraceInfo *trcinfo, uintN argc,
                                     bool constructing)
{
    JSStackFrame* const fp = cx->fp();
    jsbytecode *pc = cx->regs->pc;

    Value& fval = stackval(0 - (2 + argc));
    Value& tval = stackval(0 - (1 + argc));

    LIns* this_ins = get(&tval);

    LIns* args[nanojit::MAXARGS];
    JSSpecializedNative *sn = trcinfo->specializations;
    JS_ASSERT(sn);
    do {
        if (((sn->flags & JSTN_CONSTRUCTOR) != 0) != constructing)
            continue;

        uintN knownargc = strlen(sn->argtypes);
        if (argc != knownargc)
            continue;

        intN prefixc = strlen(sn->prefix);
        JS_ASSERT(prefixc <= 3);
        LIns** argp = &args[argc + prefixc - 1];
        char argtype;

#if defined DEBUG
        memset(args, 0xCD, sizeof(args));
#endif

        uintN i;
        for (i = prefixc; i--; ) {
            argtype = sn->prefix[i];
            if (argtype == 'C') {
                *argp = cx_ins;
            } else if (argtype == 'T') { /* this, as an object */
                if (tval.isPrimitive())
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'S') { /* this, as a string */
                if (!tval.isString())
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'f') {
                *argp = w.immpObjGC(&fval.toObject());
            } else if (argtype == 'p') {
                CHECK_STATUS(getClassPrototype(&fval.toObject(), *argp));
            } else if (argtype == 'R') {
                *argp = w.nameImmpNonGC(cx->runtime);
            } else if (argtype == 'P') {
                // FIXME: Set pc to imacpc when recording JSOP_CALL inside the
                //        JSOP_GETELEM imacro (bug 476559).
                if ((*pc == JSOP_CALL) &&
                    fp->hasImacropc() && *fp->imacropc() == JSOP_GETELEM)
                    *argp = w.nameImmpNonGC(fp->imacropc());
                else
                    *argp = w.nameImmpNonGC(pc);
            } else if (argtype == 'D') { /* this, as a number */
                if (!tval.isNumber())
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'M') {
                MathCache *mathCache = JS_THREAD_DATA(cx)->getMathCache(cx);
                if (!mathCache)
                    return RECORD_ERROR;
                *argp = w.nameImmpNonGC(mathCache);
            } else {
                JS_NOT_REACHED("unknown prefix arg type");
            }
            argp--;
        }

        for (i = knownargc; i--; ) {
            Value& arg = stackval(0 - (i + 1));
            *argp = get(&arg);

            argtype = sn->argtypes[i];
            if (argtype == 'd' || argtype == 'i') {
                if (!arg.isNumber())
                    goto next_specialization;
                if (argtype == 'i')
                    *argp = d2i(*argp);
            } else if (argtype == 'o') {
                if (arg.isPrimitive())
                    goto next_specialization;
            } else if (argtype == 's') {
                if (!arg.isString())
                    goto next_specialization;
            } else if (argtype == 'r') {
                if (!VALUE_IS_REGEXP(cx, arg))
                    goto next_specialization;
            } else if (argtype == 'f') {
                if (!IsFunctionObject(arg))
                    goto next_specialization;
            } else if (argtype == 'v') {
                *argp = box_value_for_native_call(arg, *argp);
            } else {
                goto next_specialization;
            }
            argp--;
        }
#if defined DEBUG
        JS_ASSERT(args[0] != (LIns *)0xcdcdcdcd);
#endif
        return emitNativeCall(sn, argc, args, false);

next_specialization:;
    } while ((sn++)->flags & JSTN_MORE);

    return RECORD_STOP;
}

static JSBool FASTCALL
ceilReturningInt(jsdouble x, int32 *out)
{
    jsdouble r = js_math_ceil_impl(x);
    return JSDOUBLE_IS_INT32(r, out);
}

static JSBool FASTCALL
floorReturningInt(jsdouble x, int32 *out)
{
    jsdouble r = js_math_floor_impl(x);
    return JSDOUBLE_IS_INT32(r, out);
}

static JSBool FASTCALL
roundReturningInt(jsdouble x, int32 *out)
{
    jsdouble r = js_math_round_impl(x);
    return JSDOUBLE_IS_INT32(r, out);
}

/*
 * These functions store into their second argument, so they need to
 * be annotated accordingly. To be future-proof, we use ACCSET_STORE_ANY
 * so that new callers don't have to remember to update the annotation.
 */
JS_DEFINE_CALLINFO_2(static, BOOL, ceilReturningInt, DOUBLE, INT32PTR, 0, ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_2(static, BOOL, floorReturningInt, DOUBLE, INT32PTR, 0, ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_2(static, BOOL, roundReturningInt, DOUBLE, INT32PTR, 0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callFloatReturningInt(uintN argc, const nanojit::CallInfo *ci)
{
    Value& arg = stackval(-1);
    LIns* resptr_ins = w.allocp(sizeof(int32));
    LIns* args[] = { resptr_ins, get(&arg) };
    LIns* fits_ins = w.call(ci, args);

    guard(false, w.eqi0(fits_ins), OVERFLOW_EXIT);

    LIns* res_ins = w.ldiAlloc(resptr_ins);

    set(&stackval(0 - (2 + argc)), w.i2d(res_ins));

    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;

    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callNative(uintN argc, JSOp mode)
{
    LIns* args[5];

    JS_ASSERT(mode == JSOP_CALL || mode == JSOP_NEW || mode == JSOP_APPLY);

    Value* vp = &stackval(0 - (2 + argc));
    JSObject* funobj = &vp[0].toObject();
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, funobj);
    Native native = fun->u.n.native;

    switch (argc) {
      case 1:
        if (vp[2].isNumber() && mode == JSOP_CALL) {
            if (native == js_math_ceil || native == js_math_floor || native == js_math_round) {
                LIns* a = get(&vp[2]);
                int32 result;
                if (IsPromote(a)) {
                    set(&vp[0], a);
                    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                    return RECORD_CONTINUE;
                }
                if (native == js_math_floor) {
                    if (floorReturningInt(vp[2].toNumber(), &result))
                        return callFloatReturningInt(argc, &floorReturningInt_ci);
                } else if (native == js_math_ceil) {
                    if (ceilReturningInt(vp[2].toNumber(), &result))
                        return callFloatReturningInt(argc, &ceilReturningInt_ci);
                } else if (native == js_math_round) {
                    if (roundReturningInt(vp[2].toNumber(), &result))
                        return callFloatReturningInt(argc, &roundReturningInt_ci);
                }
            } else if (native == js_math_abs) {
                LIns* a = get(&vp[2]);
                if (IsPromoteInt(a)) {
                    a = w.demote(a);
                    /* abs(INT_MIN) can't be done using integers;  exit if we see it. */
                    LIns* intMin_ins = w.name(w.immi(0x80000000), "INT_MIN");
                    LIns* isIntMin_ins = w.name(w.eqi(a, intMin_ins), "isIntMin");
                    guard(false, isIntMin_ins, MISMATCH_EXIT);
                    LIns* neg_ins = w.negi(a);
                    LIns* isNeg_ins = w.name(w.ltiN(a, 0), "isNeg");
                    LIns* abs_ins = w.name(w.cmovi(isNeg_ins, neg_ins, a), "abs");
                    set(&vp[0], w.i2d(abs_ins));
                    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                    return RECORD_CONTINUE;
                }
            }
            if (vp[1].isString()) {
                JSString *str = vp[1].toString();
                if (native == js_str_charAt) {
                    jsdouble i = vp[2].toNumber();
                    if (i < 0 || i >= str->length())
                        RETURN_STOP("charAt out of bounds");
                    LIns* str_ins = get(&vp[1]);
                    LIns* idx_ins = get(&vp[2]);
                    LIns* char_ins;
                    CHECK_STATUS(getCharAt(str, str_ins, idx_ins, mode, &char_ins));
                    set(&vp[0], char_ins);
                    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                    return RECORD_CONTINUE;
                } else if (native == js_str_charCodeAt) {
                    jsdouble i = vp[2].toNumber();
                    if (i < 0 || i >= str->length())
                        RETURN_STOP("charCodeAt out of bounds");
                    LIns* str_ins = get(&vp[1]);
                    LIns* idx_ins = get(&vp[2]);
                    LIns* charCode_ins;
                    CHECK_STATUS(getCharCodeAt(str, str_ins, idx_ins, &charCode_ins));
                    set(&vp[0], charCode_ins);
                    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                    return RECORD_CONTINUE;
                }
            }
        } else if (vp[2].isString() && mode == JSOP_CALL) {
            if (native == js_regexp_exec) {
                jsbytecode *pc = cx->regs->pc;
                /*
                 * If we see any of these sequences, the result is unused:
                 * - call / pop
                 * - call / trace / pop
                 *
                 * If we see any of these sequences, the result is only tested for nullness:
                 * - call / ifeq
                 * - call / trace / ifeq
                 * - call / not / ifeq
                 * - call / trace / not / ifeq
                 *
                 * In either case, we replace the call to RegExp.exec() on the
                 * stack with a call to RegExp.test() because "r.exec(s) !=
                 * null" is equivalent to "r.test(s)".  This avoids building
                 * the result array, which can be expensive.  This requires
                 * that RegExp.prototype.test() hasn't been changed;  we check this.
                 */
                if (pc[0] == JSOP_CALL) {
                    if ((pc[JSOP_CALL_LENGTH] == JSOP_POP) ||
                        (pc[JSOP_CALL_LENGTH] == JSOP_TRACE &&
                         pc[JSOP_CALL_LENGTH + JSOP_TRACE_LENGTH] == JSOP_POP) ||
                        (pc[JSOP_CALL_LENGTH] == JSOP_IFEQ) ||
                        (pc[JSOP_CALL_LENGTH] == JSOP_TRACE &&
                         pc[JSOP_CALL_LENGTH + JSOP_TRACE_LENGTH] == JSOP_IFEQ) ||
                        (pc[JSOP_CALL_LENGTH] == JSOP_NOT &&
                         pc[JSOP_CALL_LENGTH + JSOP_NOT_LENGTH] == JSOP_IFEQ) ||
                        (pc[JSOP_CALL_LENGTH] == JSOP_TRACE &&
                         pc[JSOP_CALL_LENGTH + JSOP_TRACE_LENGTH] == JSOP_NOT &&
                         pc[JSOP_CALL_LENGTH + JSOP_TRACE_LENGTH + JSOP_NOT_LENGTH] == JSOP_IFEQ))
                    {
                        JSObject* proto;
                        jsid id = ATOM_TO_JSID(cx->runtime->atomState.testAtom);
                        /* Get RegExp.prototype.test() and check it hasn't been changed. */
                        if (js_GetClassPrototype(cx, NULL, JSProto_RegExp, &proto)) {
                            if (JSObject *tmp = HasNativeMethod(proto, id, js_regexp_test)) {
                                vp[0] = ObjectValue(*tmp);
                                funobj = tmp;
                                fun = tmp->getFunctionPrivate();
                                native = js_regexp_test;
                            }
                        }
                    }
                }
            }
        }
        break;

      case 2:
        if (vp[2].isNumber() && vp[3].isNumber() && mode == JSOP_CALL &&
            (native == js_math_min || native == js_math_max)) {
            LIns* a = get(&vp[2]);
            LIns* b = get(&vp[3]);
            if (IsPromote(a) && IsPromote(b)) {
                a = w.demote(a);
                b = w.demote(b);
                LIns* cmp = (native == js_math_min) ? w.lti(a, b) : w.gti(a, b);
                set(&vp[0], w.i2d(w.cmovi(cmp, a, b)));
                pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                return RECORD_CONTINUE;
            }
        }
        break;
    }

    if (fun->flags & JSFUN_TRCINFO) {
        JSNativeTraceInfo *trcinfo = FUN_TRCINFO(fun);
        JS_ASSERT(trcinfo && fun->u.n.native == trcinfo->native);

        /* Try to call a type specialized version of the native. */
        if (trcinfo->specializations) {
            RecordingStatus status = callSpecializedNative(trcinfo, argc, mode == JSOP_NEW);
            if (status != RECORD_STOP)
                return status;
        }
    }

    if (native == js_fun_apply || native == js_fun_call)
        RETURN_STOP("trying to call native apply or call");

    // Allocate the vp vector and emit code to root it.
    uintN vplen = 2 + argc;
    LIns* invokevp_ins = w.allocp(vplen * sizeof(Value));

    // vp[0] is the callee.
    box_value_into(vp[0], w.immpObjGC(funobj), AllocSlotsAddress(invokevp_ins));

    // Calculate |this|.
    LIns* this_ins;
    if (mode == JSOP_NEW) {
        Class* clasp = fun->u.n.clasp;
        JS_ASSERT(clasp != &js_SlowArrayClass);
        if (!clasp)
            clasp = &js_ObjectClass;
        JS_ASSERT(((jsuword) clasp & 3) == 0);

        // Abort on |new Function|. js_CreateThis would allocate a regular-
        // sized JSObject, not a Function-sized one. (The Function ctor would
        // deep-bail anyway but let's not go there.)
        if (clasp == &js_FunctionClass)
            RETURN_STOP("new Function");

        if (!clasp->isNative())
            RETURN_STOP("new with non-native ops");

        if (fun->isConstructor()) {
            vp[1].setMagicWithObjectOrNullPayload(NULL);
            newobj_ins = w.immpMagicNull();

            /* Treat this as a regular call, the constructor will behave correctly. */
            mode = JSOP_CALL;
        } else {
            args[0] = w.immpObjGC(funobj);
            args[1] = w.nameImmpNonGC(clasp);
            args[2] = cx_ins;
            newobj_ins = w.call(&js_CreateThisFromTrace_ci, args);
            guard(false, w.eqp0(newobj_ins), OOM_EXIT);

            /*
             * emitNativeCall may take a snapshot below. To avoid having a type
             * mismatch (e.g., where get(&vp[1]) is an object and vp[1] is
             * null), we make sure vp[1] is some object. The actual object
             * doesn't matter; JSOP_NEW and InvokeConstructor both overwrite
             * vp[1] without observing its value.
             *
             * N.B. tracing specializes for functions, so pick a non-function.
             */
            vp[1].setObject(*globalObj);
        }
        this_ins = newobj_ins;
    } else {
        this_ins = get(&vp[1]);
    }
    set(&vp[1], this_ins);
    box_value_into(vp[1], this_ins, AllocSlotsAddress(invokevp_ins, 1));

    // Populate argv.
    for (uintN n = 2; n < 2 + argc; n++) {
        box_value_into(vp[n], get(&vp[n]), AllocSlotsAddress(invokevp_ins, n));
        // For a very long argument list we might run out of LIR space, so
        // check inside the loop.
        if (outOfMemory())
            RETURN_STOP("out of memory in argument list");
    }

    // Populate extra slots, including the return value slot for a slow native.
    if (2 + argc < vplen) {
        for (uintN n = 2 + argc; n < vplen; n++) {
            box_undefined_into(AllocSlotsAddress(invokevp_ins, n));
            if (outOfMemory())
                RETURN_STOP("out of memory in extra slots");
        }
    }

    // Set up arguments for the JSNative or JSFastNative.
    if (mode == JSOP_NEW)
        RETURN_STOP("untraceable fast native constructor");
    native_rval_ins = invokevp_ins;
    args[0] = invokevp_ins;
    args[1] = w.immi(argc);
    args[2] = cx_ins;
    uint32 typesig = CallInfo::typeSig3(ARGTYPE_I, ARGTYPE_P, ARGTYPE_I, ARGTYPE_P);

    // Generate CallInfo and a JSSpecializedNative structure on the fly.
    // Do not use JSTN_UNBOX_AFTER for mode JSOP_NEW because
    // record_NativeCallComplete unboxes the result specially.

    CallInfo* ci = new (traceAlloc()) CallInfo();
    ci->_address = uintptr_t(fun->u.n.native);
    ci->_isPure = 0;
    ci->_storeAccSet = ACCSET_STORE_ANY;
    ci->_abi = ABI_CDECL;
    ci->_typesig = typesig;
#ifdef DEBUG
    ci->_name = JS_GetFunctionName(fun);
 #endif

    // Generate a JSSpecializedNative structure on the fly.
    generatedSpecializedNative.builtin = ci;
    generatedSpecializedNative.flags = FAIL_STATUS | ((mode == JSOP_NEW)
                                                        ? JSTN_CONSTRUCTOR
                                                        : JSTN_UNBOX_AFTER);
    generatedSpecializedNative.prefix = NULL;
    generatedSpecializedNative.argtypes = NULL;

    // We only have to ensure that the values we wrote into the stack buffer
    // are rooted if we actually make it to the call, so only set nativeVp and
    // nativeVpLen immediately before emitting the call code. This way we avoid
    // leaving trace with a bogus nativeVp because we fall off trace while unboxing
    // values into the stack buffer.
    w.stStateField(w.nameImmi(vplen), nativeVpLen);
    w.stStateField(invokevp_ins, nativeVp);

    // argc is the original argc here. It is used to calculate where to place
    // the return value.
    return emitNativeCall(&generatedSpecializedNative, argc, args, true);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::functionCall(uintN argc, JSOp mode)
{
    Value& fval = stackval(0 - (2 + argc));
    JS_ASSERT(&fval >= cx->fp()->base());

    if (!IsFunctionObject(fval))
        RETURN_STOP("callee is not a function");

    Value& tval = stackval(0 - (1 + argc));

    /*
     * If callee is not constant, it's a shapeless call and we have to guard
     * explicitly that we will get this callee again at runtime.
     */
    if (!get(&fval)->isImmP())
        CHECK_STATUS(guardCallee(fval));

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
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, &fval.toObject());

#ifdef MOZ_TRACE_JSCALLS
    if (cx->functionCallback) {
        JSScript *script = FUN_SCRIPT(fun);
        if (! script || ! script->isEmpty()) {
            LIns* args[] = { w.nameImmi(1), w.nameImmpNonGC(fun), cx_ins };
            LIns* call_ins = w.call(&functionProbe_ci, args);
            guard(false, w.eqi0(call_ins), MISMATCH_EXIT);
        }
    }
#endif

    if (FUN_INTERPRETED(fun))
        return interpretedFunctionCall(fval, fun, argc, mode == JSOP_NEW);

    Native native = fun->maybeNative();
    Value* argv = &tval + 1;
    if (native == js_Array)
        return newArray(&fval.toObject(), argc, argv, &fval);
    if (native == js_String && argc == 1) {
        if (mode == JSOP_NEW)
            return newString(&fval.toObject(), 1, argv, &fval);
        if (!argv[0].isPrimitive()) {
            CHECK_STATUS(guardNativeConversion(argv[0]));
            return callImacro(call_imacros.String);
        }
        set(&fval, stringify(argv[0]));
        pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
        return RECORD_CONTINUE;
    }

    RecordingStatus rs = callNative(argc, mode);
#ifdef MOZ_TRACE_JSCALLS
    if (cx->functionCallback) {
        LIns* args[] = { w.nameImmi(0), w.nameImmpNonGC(fun), cx_ins };
        LIns* call_ins = w.call(&functionProbe_ci, args);
        guard(false, w.eqi0(call_ins), MISMATCH_EXIT);
    }
#endif
    return rs;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEW()
{
    uintN argc = GET_ARGC(cx->regs->pc);
    cx->assertValidStackDepth(argc + 2);
    return InjectStatus(functionCall(argc, JSOP_NEW));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELNAME()
{
    return ARECORD_STOP;
}

JSBool JS_FASTCALL
DeleteIntKey(JSContext* cx, JSObject* obj, int32 i, JSBool strict)
{
    LeaveTraceIfGlobalObject(cx, obj);
    Value v = BooleanValue(false);
    jsid id;
    if (INT_FITS_IN_JSID(i)) {
        id = INT_TO_JSID(i);
    } else {
        if (!js_ValueToStringId(cx, Int32Value(i), &id)) {
            SetBuiltinError(cx);
            return false;
        }
    }

    if (!obj->deleteProperty(cx, id, &v, strict))
        SetBuiltinError(cx);
    return v.toBoolean();
}
JS_DEFINE_CALLINFO_4(extern, BOOL_FAIL, DeleteIntKey, CONTEXT, OBJECT, INT32, BOOL,
                     0, ACCSET_STORE_ANY)

JSBool JS_FASTCALL
DeleteStrKey(JSContext* cx, JSObject* obj, JSString* str, JSBool strict)
{
    LeaveTraceIfGlobalObject(cx, obj);
    Value v = BooleanValue(false);
    jsid id;

    /*
     * NB: JSOP_DELPROP does not need js_ValueToStringId to atomize, but (see
     * jsatominlines.h) that helper early-returns if the computed property name
     * string is already atomized, and we are *not* on a perf-critical path!
     */
    if (!js_ValueToStringId(cx, StringValue(str), &id) || !obj->deleteProperty(cx, id, &v, strict))
        SetBuiltinError(cx);
    return v.toBoolean();
}
JS_DEFINE_CALLINFO_4(extern, BOOL_FAIL, DeleteStrKey, CONTEXT, OBJECT, STRING, BOOL,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELPROP()
{
    Value& lval = stackval(-1);
    if (lval.isPrimitive())
        RETURN_STOP_A("JSOP_DELPROP on primitive base expression");
    if (&lval.toObject() == globalObj)
        RETURN_STOP_A("JSOP_DELPROP on global property");

    JSAtom* atom = atoms[GET_INDEX(cx->regs->pc)];

    enterDeepBailCall();
    LIns* args[] = { strictModeCode_ins, w.immpAtomGC(atom), get(&lval), cx_ins };
    LIns* rval_ins = w.call(&DeleteStrKey_ci, args);

    LIns* status_ins = w.ldiStateField(builtinStatus);
    pendingGuardCondition = w.eqi0(status_ins);
    leaveDeepBailCall();

    set(&lval, rval_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELELEM()
{
    Value& lval = stackval(-2);
    if (lval.isPrimitive())
        RETURN_STOP_A("JSOP_DELELEM on primitive base expression");
    if (&lval.toObject() == globalObj)
        RETURN_STOP_A("JSOP_DELELEM on global property");

    Value& idx = stackval(-1);
    LIns* rval_ins;

    enterDeepBailCall();
    if (hasInt32Repr(idx)) {
        LIns* num_ins;
        CHECK_STATUS_A(makeNumberInt32(get(&idx), &num_ins));
        LIns* args[] = { strictModeCode_ins, num_ins, get(&lval), cx_ins };
        rval_ins = w.call(&DeleteIntKey_ci, args);
    } else if (idx.isString()) {
        LIns* args[] = { strictModeCode_ins, get(&idx), get(&lval), cx_ins };
        rval_ins = w.call(&DeleteStrKey_ci, args);
    } else {
        RETURN_STOP_A("JSOP_DELELEM on non-int, non-string index");
    }

    LIns* status_ins = w.ldiStateField(builtinStatus);
    pendingGuardCondition = w.eqi0(status_ins);
    leaveDeepBailCall();

    set(&lval, rval_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TYPEOF()
{
    Value& r = stackval(-1);
    LIns* type;
    if (r.isString()) {
        type = w.immpAtomGC(cx->runtime->atomState.typeAtoms[JSTYPE_STRING]);
    } else if (r.isNumber()) {
        type = w.immpAtomGC(cx->runtime->atomState.typeAtoms[JSTYPE_NUMBER]);
    } else if (r.isUndefined()) {
        type = w.immpAtomGC(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    } else if (r.isBoolean()) {
        type = w.immpAtomGC(cx->runtime->atomState.typeAtoms[JSTYPE_BOOLEAN]);
    } else if (r.isNull()) {
        type = w.immpAtomGC(cx->runtime->atomState.typeAtoms[JSTYPE_OBJECT]);
    } else {
        if (r.toObject().isFunction()) {
            type = w.immpAtomGC(cx->runtime->atomState.typeAtoms[JSTYPE_FUNCTION]);
        } else {
            LIns* args[] = { get(&r), cx_ins };
            type = w.call(&js_TypeOfObject_ci, args);
        }
    }
    set(&r, type);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_VOID()
{
    stack(-1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCNAME()
{
    return incName(1);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCPROP()
{
    return incProp(1);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCELEM()
{
    return InjectStatus(incElem(1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECNAME()
{
    return incName(-1);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECPROP()
{
    return incProp(-1);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECELEM()
{
    return InjectStatus(incElem(-1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::incName(jsint incr, bool pre)
{
    Value* vp;
    LIns* v_ins;
    LIns* v_after;
    NameResult nr;

    CHECK_STATUS_A(name(vp, v_ins, nr));
    Value v = nr.tracked ? *vp : nr.v;
    CHECK_STATUS_A(incHelper(v, v_ins, v_after, incr));
    LIns* v_result = pre ? v_after : v_ins;
    if (nr.tracked) {
        set(vp, v_after);
        stack(0, v_result);
        return ARECORD_CONTINUE;
    }

    if (!nr.obj->isCall())
        RETURN_STOP_A("incName on unsupported object class");

    CHECK_STATUS_A(setCallProp(nr.obj, nr.obj_ins, nr.shape, v_after, v));
    stack(0, v_result);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NAMEINC()
{
    return incName(1, false);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PROPINC()
{
    return incProp(1, false);
}

// XXX consolidate with record_JSOP_GETELEM code...
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ELEMINC()
{
    return InjectStatus(incElem(1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NAMEDEC()
{
    return incName(-1, false);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PROPDEC()
{
    return incProp(-1, false);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ELEMDEC()
{
    return InjectStatus(incElem(-1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETPROP()
{
    return getProp(stackval(-1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETPROP()
{
    Value& l = stackval(-2);
    if (l.isPrimitive())
        RETURN_STOP_A("primitive this for SETPROP");

    JSObject* obj = &l.toObject();
    if (obj->getOps()->setProperty)
        RETURN_STOP_A("non-native js::ObjectOps::setProperty");
    return ARECORD_CONTINUE;
}

/* Emit a specialized, inlined copy of js_NativeSet. */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::nativeSet(JSObject* obj, LIns* obj_ins, const Shape* shape,
                         const Value &v, LIns* v_ins)
{
    uint32 slot = shape->slot;

    /*
     * We do not trace assignment to properties that have both a nonstub setter
     * and a slot, for several reasons.
     *
     * First, that would require sampling rt->propertyRemovals before and after
     * (see js_NativeSet), and even more code to handle the case where the two
     * samples differ. A mere guard is not enough, because you can't just bail
     * off trace in the middle of a property assignment without storing the
     * value and making the stack right.
     *
     * If obj is the global object, there are two additional problems. We would
     * have to emit still more code to store the result in the object (not the
     * native global frame) if the setter returned successfully after
     * deep-bailing.  And we would have to cope if the run-time type of the
     * setter's return value differed from the record-time type of v, in which
     * case unboxing would fail and, having called a native setter, we could
     * not just retry the instruction in the interpreter.
     */
    JS_ASSERT(shape->hasDefaultSetter() || slot == SHAPE_INVALID_SLOT);

    // Call the setter, if any.
    if (!shape->hasDefaultSetter())
        emitNativePropertyOp(shape, obj_ins, true, box_value_into_alloc(v, v_ins));

    // Store the value, if this property has a slot.
    if (slot != SHAPE_INVALID_SLOT) {
        JS_ASSERT(obj->containsSlot(shape->slot));
        JS_ASSERT(shape->hasSlot());
        if (obj == globalObj) {
            if (!lazilyImportGlobalSlot(slot))
                RETURN_STOP("lazy import of global slot failed");
            set(&obj->getSlotRef(slot), v_ins);
        } else {
            LIns* slots_ins = NULL;
            stobj_set_slot(obj, obj_ins, slot, slots_ins, v, v_ins);
        }
    }

    return RECORD_CONTINUE;
}

static JSBool FASTCALL
MethodWriteBarrier(JSContext* cx, JSObject* obj, Shape* shape, JSObject* funobj)
{
    bool ok = obj->methodWriteBarrier(cx, *shape, ObjectValue(*funobj));
    JS_ASSERT(cx->tracerState->builtinStatus == 0);
    return ok;
}
JS_DEFINE_CALLINFO_4(static, BOOL_RETRY, MethodWriteBarrier, CONTEXT, OBJECT, SHAPE, OBJECT,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::setProp(Value &l, PropertyCacheEntry* entry, const Shape* shape,
                       Value &v, LIns*& v_ins, bool isDefinitelyAtom)
{
    if (entry == JS_NO_PROP_CACHE_FILL)
        RETURN_STOP("can't trace uncacheable property set");
    JS_ASSERT_IF(entry->vcapTag() >= 1, !shape->hasSlot());

    JS_ASSERT(!l.isPrimitive());
    JSObject* obj = &l.toObject();

    if (!shape->hasDefaultSetter() && shape->slot != SHAPE_INVALID_SLOT && !obj->isCall())
        RETURN_STOP("can't trace set of property with setter and slot");
    if (shape->hasSetterValue())
        RETURN_STOP("can't trace JavaScript function setter");

    // These two cases are errors and can't be traced.
    if (shape->hasGetterValue())
        RETURN_STOP("can't assign to property with script getter but no setter");
    if (!shape->writable())
        RETURN_STOP("can't assign to readonly property");

    LIns* obj_ins = get(&l);

    JS_ASSERT_IF(entry->directHit(), obj->nativeContains(*shape));

    // Fast path for CallClass. This is about 20% faster than the general case.
    v_ins = get(&v);
    if (obj->isCall())
        return setCallProp(obj, obj_ins, shape, v_ins, v);

    // Find obj2. If entry->adding(), the TAG bits are all 0.
    JSObject* obj2 = obj;
    for (jsuword i = entry->scopeIndex(); i; i--)
        obj2 = obj2->getParent();
    for (jsuword j = entry->protoIndex(); j; j--)
        obj2 = obj2->getProto();
    JS_ASSERT_IF(entry->adding(), obj2 == obj);

    // Guard before anything else.
    PCVal pcval;
    CHECK_STATUS(guardPropertyCacheHit(obj_ins, obj, obj2, entry, pcval));
    JS_ASSERT(!obj2->nativeEmpty());
    JS_ASSERT(obj2->nativeContains(*shape));
    JS_ASSERT_IF(obj2 != obj, !shape->hasSlot());

    /*
     * Setting a function-valued property might need to rebrand the object, so
     * we emit a call to the method write barrier. There's no need to guard on
     * this, because functions have distinct trace-type from other values and
     * branded-ness is implied by the shape, which we've already guarded on.
     */
    if (obj2->brandedOrHasMethodBarrier() && IsFunctionObject(v) && entry->directHit()) {
        /*
         * This guarantees that generateOwnShape (possibly called by the
         * methodWriteBarrier) will not leave trace.
         */
        if (obj == globalObj)
            RETURN_STOP("can't trace function-valued property set in branded global scope");

        /*
         * MethodWriteBarrier takes care not to deep bail, but it may reshape
         * the given object, so forget anything we knew about obj's shape.
         */
        guardedShapeTable.remove(obj_ins);
        LIns* args[] = { v_ins, w.immpShapeGC(shape), obj_ins, cx_ins };
        LIns* ok_ins = w.call(&MethodWriteBarrier_ci, args);
        guard(false, w.eqi0(ok_ins), OOM_EXIT);
    }

    // Add a property to the object if necessary.
    if (entry->adding()) {
        JS_ASSERT(shape->hasSlot());
        if (obj == globalObj)
            RETURN_STOP("adding a property to the global object");

        LIns* args[] = { w.immpShapeGC(shape), obj_ins, cx_ins };
        const CallInfo *ci = isDefinitelyAtom ? &js_AddAtomProperty_ci : &js_AddProperty_ci;
        LIns* ok_ins = w.call(ci, args);
        guard(false, w.eqi0(ok_ins), OOM_EXIT);
    }

    return nativeSet(obj, obj_ins, shape, v, v_ins);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::setUpwardTrackedVar(Value* stackVp, const Value &v, LIns* v_ins)
{
    JSValueType stackT = determineSlotType(stackVp);
    JSValueType otherT = getCoercedType(v);

    bool promote = true;

    if (stackT != otherT) {
        if (stackT == JSVAL_TYPE_DOUBLE && otherT == JSVAL_TYPE_INT32 && IsPromoteInt(v_ins))
            promote = false;
        else
            RETURN_STOP("can't trace this upvar mutation");
    }

    set(stackVp, v_ins, promote);

    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::setCallProp(JSObject *callobj, LIns *callobj_ins, const Shape *shape,
                           LIns *v_ins, const Value &v)
{
    // Set variables in on-trace-stack call objects by updating the tracker.
    JSStackFrame *fp = frameIfInRange(callobj);
    if (fp) {
        if (shape->setterOp() == SetCallArg) {
            JS_ASSERT(shape->hasShortID());
            uintN slot = uint16(shape->shortid);
            Value *vp2 = &fp->formalArg(slot);
            CHECK_STATUS(setUpwardTrackedVar(vp2, v, v_ins));
            return RECORD_CONTINUE;
        }
        if (shape->setterOp() == SetCallVar) {
            JS_ASSERT(shape->hasShortID());
            uintN slot = uint16(shape->shortid);
            Value *vp2 = &fp->slots()[slot];
            CHECK_STATUS(setUpwardTrackedVar(vp2, v, v_ins));
            return RECORD_CONTINUE;
        }
        RETURN_STOP("can't trace special CallClass setter");
    }

    if (!callobj->getPrivate()) {
        // Because the parent guard in guardCallee ensures this Call object
        // will be the same object now and on trace, and because once a Call
        // object loses its frame it never regains one, on trace we will also
        // have a null private in the Call object. So all we need to do is
        // write the value to the Call object's slot.
        intN slot = uint16(shape->shortid);
        if (shape->setterOp() == SetCallArg) {
            JS_ASSERT(slot < ArgClosureTraits::slot_count(callobj));
            slot += ArgClosureTraits::slot_offset(callobj);
        } else if (shape->setterOp() == SetCallVar) {
            JS_ASSERT(slot < VarClosureTraits::slot_count(callobj));
            slot += VarClosureTraits::slot_offset(callobj);
        } else {
            RETURN_STOP("can't trace special CallClass setter");
        }

        // Now assert that the shortid get we did above was ok. Have to do it
        // after the RETURN_STOP above, since in that case we may in fact not
        // have a valid shortid; but we don't use it in that case anyway.
        JS_ASSERT(shape->hasShortID());

        LIns* slots_ins = NULL;
        stobj_set_dslot(callobj_ins, slot, slots_ins, v, v_ins);
        return RECORD_CONTINUE;
    }

    // This is the hard case: we have a JSStackFrame private, but it's not in
    // range.  During trace execution we may or may not have a JSStackFrame
    // anymore.  Call the standard builtins, which handle that situation.

    // Set variables in off-trace-stack call objects by calling standard builtins.
    const CallInfo* ci = NULL;
    if (shape->setterOp() == SetCallArg)
        ci = &js_SetCallArg_ci;
    else if (shape->setterOp() == SetCallVar)
        ci = &js_SetCallVar_ci;
    else
        RETURN_STOP("can't trace special CallClass setter");

    // Even though the frame is out of range, later we might be called as an
    // inner trace such that the target variable is defined in the outer trace
    // entry frame. For simplicity, we just fall off trace.
    guard(false,
          w.eqp(entryFrameIns(), w.ldpObjPrivate(callobj_ins)),
          MISMATCH_EXIT);

    LIns* args[] = {
        box_value_for_native_call(v, v_ins),
        w.nameImmw(JSID_BITS(SHAPE_USERID(shape))),
        callobj_ins,
        cx_ins
    };
    LIns* call_ins = w.call(ci, args);
    guard(false, w.name(w.eqi0(call_ins), "guard(set upvar)"), STATUS_EXIT);

    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_SetPropHit(PropertyCacheEntry* entry, const Shape* shape)
{
    Value& r = stackval(-1);
    Value& l = stackval(-2);
    LIns* v_ins;

    jsbytecode* pc = cx->regs->pc;

    bool isDefinitelyAtom = (*pc == JSOP_SETPROP);
    CHECK_STATUS_A(setProp(l, entry, shape, r, v_ins, isDefinitelyAtom));

    switch (*pc) {
      case JSOP_SETPROP:
      case JSOP_SETNAME:
      case JSOP_SETGNAME:
      case JSOP_SETMETHOD:
        if (pc[JSOP_SETPROP_LENGTH] != JSOP_POP)
            set(&l, v_ins);
        break;

      default:;
    }

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK VMSideExit*
TraceRecorder::enterDeepBailCall()
{
    // Take snapshot for DeepBail and store it in cx->bailExit.
    VMSideExit* exit = snapshot(DEEP_BAIL_EXIT);
    w.stContextField(w.nameImmpNonGC(exit), bailExit);

    // Tell nanojit not to discard or defer stack writes before this call.
    w.xbarrier(createGuardRecord(exit));

    // Forget about guarded shapes, since deep bailers can reshape the world.
    forgetGuardedShapes();
    return exit;
}

JS_REQUIRES_STACK void
TraceRecorder::leaveDeepBailCall()
{
    // Keep cx->bailExit null when it's invalid.
    w.stContextField(w.immpNull(), bailExit);
}

JS_REQUIRES_STACK void
TraceRecorder::finishGetProp(LIns* obj_ins, LIns* vp_ins, LIns* ok_ins, Value* outp)
{
    // Store the boxed result (and this-object, if JOF_CALLOP) before the
    // guard. The deep-bail case requires this. If the property get fails,
    // these slots will be ignored anyway.
    // N.B. monitorRecording expects get(outp)->isLoad()
    JS_ASSERT(vp_ins->isop(LIR_allocp));
    LIns* result_ins = w.lddAlloc(vp_ins);
    set(outp, result_ins);
    if (js_CodeSpec[*cx->regs->pc].format & JOF_CALLOP)
        set(outp + 1, obj_ins);

    // We need to guard on ok_ins, but this requires a snapshot of the state
    // after this op. monitorRecording will do it for us.
    pendingGuardCondition = ok_ins;

    // Note there is a boxed result sitting on the stack. The caller must leave
    // it there for the time being, since the return type is not yet
    // known. monitorRecording will emit the code to unbox it.
    pendingUnboxSlot = outp;
}

static inline bool
RootedStringToId(JSContext* cx, JSString** namep, jsid* idp)
{
    JSString* name = *namep;
    if (name->isAtomized()) {
        *idp = INTERNED_STRING_TO_JSID(name);
        return true;
    }

    JSAtom* atom = js_AtomizeString(cx, name, 0);
    if (!atom)
        return false;
    *namep = ATOM_TO_STRING(atom); /* write back to GC root */
    *idp = ATOM_TO_JSID(atom);
    return true;
}

static const size_t PIC_TABLE_ENTRY_COUNT = 32;

struct PICTableEntry
{
    jsid    id;
    uint32  shape;
    uint32  slot;
};

struct PICTable
{
    PICTable() : entryCount(0) {}

    PICTableEntry   entries[PIC_TABLE_ENTRY_COUNT];
    uint32          entryCount;

    bool scan(uint32 shape, jsid id, uint32 *slotOut) {
        for (size_t i = 0; i < entryCount; ++i) {
            PICTableEntry &entry = entries[i];
            if (entry.shape == shape && entry.id == id) {
                *slotOut = entry.slot;
                return true;
            }
        }
        return false;
    }

    void update(uint32 shape, jsid id, uint32 slot) {
        if (entryCount >= PIC_TABLE_ENTRY_COUNT)
            return;
        PICTableEntry &newEntry = entries[entryCount++];
        newEntry.shape = shape;
        newEntry.id = id;
        newEntry.slot = slot;
    }
};

static JSBool FASTCALL
GetPropertyByName(JSContext* cx, JSObject* obj, JSString** namep, Value* vp, PICTable *picTable)
{
    LeaveTraceIfGlobalObject(cx, obj);

    jsid id;
    if (!RootedStringToId(cx, namep, &id)) {
        SetBuiltinError(cx);
        return false;
    }
    
    /* Delegate to the op, if present. */
    PropertyIdOp op = obj->getOps()->getProperty;
    if (op) {
        bool result = op(cx, obj, obj, id, vp);
        if (!result)
            SetBuiltinError(cx);
        return cx->tracerState->builtinStatus == 0;
    }

    /* Try to hit in the cache. */
    uint32 slot;
    if (picTable->scan(obj->shape(), id, &slot)) {
        *vp = obj->getSlot(slot);
        return cx->tracerState->builtinStatus == 0;
    }

    const Shape *shape;
    JSObject *holder;
    if (!js_GetPropertyHelperWithShape(cx, obj, obj, id, JSGET_METHOD_BARRIER, vp, &shape,
                                       &holder)) {
        SetBuiltinError(cx);
        return false;
    }

    /* Only update the table when the object is the holder of the property. */
    if (obj == holder && shape->hasSlot()) {
        /*
         * Note: we insert the non-normalized id into the table so you don't need to
         * normalize it before hitting in the table (faster lookup).
         */
        picTable->update(obj->shape(), id, shape->slot);
    }
    
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_5(static, BOOL_FAIL, GetPropertyByName, CONTEXT, OBJECT, STRINGPTR, VALUEPTR,
                     PICTABLE,
                     0, ACCSET_STORE_ANY)

// Convert the value in a slot to a string and store the resulting string back
// in the slot (typically in order to root it).
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::primitiveToStringInPlace(Value* vp)
{
    Value v = *vp;
    JS_ASSERT(v.isPrimitive());

    if (!v.isString()) {
        // v is not a string. Turn it into one. js_ValueToString is safe
        // because v is not an object.
        JSString *str = js_ValueToString(cx, v);
        JS_ASSERT(TRACE_RECORDER(cx) == this);
        if (!str)
            RETURN_ERROR("failed to stringify element id");
        v.setString(str);
        set(vp, stringify(*vp));

        // Write the string back to the stack to save the interpreter some work
        // and to ensure snapshots get the correct type for this slot.
        *vp = v;
    }
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyByName(LIns* obj_ins, Value* idvalp, Value* outp)
{
    CHECK_STATUS(primitiveToStringInPlace(idvalp));
    enterDeepBailCall();

    // Call GetPropertyByName. The vp parameter points to stack because this is
    // what the interpreter currently does. obj and id are rooted on the
    // interpreter stack, but the slot at vp is not a root.
    LIns* vp_ins = w.name(w.allocp(sizeof(Value)), "vp");
    LIns* idvalp_ins = w.name(addr(idvalp), "idvalp");
    PICTable *picTable = new (traceAlloc()) PICTable();
    LIns* pic_ins = w.nameImmpNonGC(picTable);
    LIns* args[] = {pic_ins, vp_ins, idvalp_ins, obj_ins, cx_ins};
    LIns* ok_ins = w.call(&GetPropertyByName_ci, args);

    // GetPropertyByName can assign to *idvalp, so the tracker has an incorrect
    // entry for that address. Correct it. (If the value in the address is
    // never used again, the usual case, Nanojit will kill this load.)
    // The Address could be made more precise with some effort (idvalp_ins may
    // be a stack location), but it's not worth it because this case is rare.
    tracker.set(idvalp, w.ldp(AnyAddress(idvalp_ins)));

    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

static JSBool FASTCALL
GetPropertyByIndex(JSContext* cx, JSObject* obj, int32 index, Value* vp)
{
    LeaveTraceIfGlobalObject(cx, obj);

    AutoIdRooter idr(cx);
    if (!js_Int32ToId(cx, index, idr.addr()) || !obj->getProperty(cx, idr.id(), vp)) {
        SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyByIndex, CONTEXT, OBJECT, INT32, VALUEPTR, 0,
                     ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyByIndex(LIns* obj_ins, LIns* index_ins, Value* outp)
{
    CHECK_STATUS(makeNumberInt32(index_ins, &index_ins));

    // See note in getPropertyByName about vp.
    enterDeepBailCall();
    LIns* vp_ins = w.name(w.allocp(sizeof(Value)), "vp");
    LIns* args[] = {vp_ins, index_ins, obj_ins, cx_ins};
    LIns* ok_ins = w.call(&GetPropertyByIndex_ci, args);
    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

static JSBool FASTCALL
GetPropertyById(JSContext* cx, JSObject* obj, jsid id, Value* vp)
{
    LeaveTraceIfGlobalObject(cx, obj);
    if (!obj->getProperty(cx, id, vp)) {
        SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyById, CONTEXT, OBJECT, JSID, VALUEPTR,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyById(LIns* obj_ins, Value* outp)
{
    // Find the atom.
    JSAtom* atom;
    jsbytecode* pc = cx->regs->pc;
    const JSCodeSpec& cs = js_CodeSpec[*pc];
    if (*pc == JSOP_LENGTH) {
        atom = cx->runtime->atomState.lengthAtom;
    } else if (JOF_TYPE(cs.format) == JOF_ATOM) {
        atom = atoms[GET_INDEX(pc)];
    } else {
        JS_ASSERT(JOF_TYPE(cs.format) == JOF_SLOTATOM);
        atom = atoms[GET_INDEX(pc + SLOTNO_LEN)];
    }

    JS_STATIC_ASSERT(sizeof(jsid) == sizeof(void *));
    jsid id = ATOM_TO_JSID(atom);

    // Call GetPropertyById. See note in getPropertyByName about vp.
    enterDeepBailCall();
    LIns* vp_ins = w.name(w.allocp(sizeof(Value)), "vp");
    LIns* args[] = {vp_ins, w.nameImmw(JSID_BITS(id)), obj_ins, cx_ins};
    LIns* ok_ins = w.call(&GetPropertyById_ci, args);
    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

/* Manually inlined, specialized copy of js_NativeGet. */
static JSBool FASTCALL
GetPropertyWithNativeGetter(JSContext* cx, JSObject* obj, Shape* shape, Value* vp)
{
    LeaveTraceIfGlobalObject(cx, obj);

#ifdef DEBUG
    JSProperty* prop;
    JSObject* pobj;
    JS_ASSERT(obj->lookupProperty(cx, shape->id, &pobj, &prop));
    JS_ASSERT(prop == (JSProperty*) shape);
#endif

    // Shape::get contains a special case for With objects. We can elide it
    // here because With objects are, we claim, never on the operand stack
    // while recording.
    JS_ASSERT(obj->getClass() != &js_WithClass);

    vp->setUndefined();
    if (!shape->getterOp()(cx, obj, SHAPE_USERID(shape), vp)) {
        SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyWithNativeGetter,
                     CONTEXT, OBJECT, SHAPE, VALUEPTR, 0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyWithNativeGetter(LIns* obj_ins, const Shape* shape, Value* outp)
{
    JS_ASSERT(!shape->hasGetterValue());
    JS_ASSERT(shape->slot == SHAPE_INVALID_SLOT);
    JS_ASSERT(!shape->hasDefaultGetterOrIsMethod());

    // Call GetPropertyWithNativeGetter. See note in getPropertyByName about vp.
    // FIXME - We should call the getter directly. Using a builtin function for
    // now because it buys some extra asserts. See bug 508310.
    enterDeepBailCall();
    LIns* vp_ins = w.name(w.allocp(sizeof(Value)), "vp");
    LIns* args[] = {vp_ins, w.nameImmpNonGC(shape), obj_ins, cx_ins};
    LIns* ok_ins = w.call(&GetPropertyWithNativeGetter_ci, args);
    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyWithScriptGetter(JSObject *obj, LIns* obj_ins, const Shape* shape)
{
    if (!canCallImacro())
        RETURN_STOP("cannot trace script getter, already in imacro");

    // Rearrange the stack in preparation for the imacro, taking care to adjust
    // the interpreter state and the tracker in the same way. This adjustment
    // is noted in imacros.jsasm with .fixup tags.
    Value getter = shape->getterValue();
    Value*& sp = cx->regs->sp;
    switch (*cx->regs->pc) {
      case JSOP_GETPROP:
        sp++;
        sp[-1] = sp[-2];
        set(&sp[-1], get(&sp[-2]));
        sp[-2] = getter;
        set(&sp[-2], w.immpObjGC(&getter.toObject()));
        return callImacroInfallibly(getprop_imacros.scriptgetter);

      case JSOP_CALLPROP:
        sp += 2;
        sp[-2] = getter;
        set(&sp[-2], w.immpObjGC(&getter.toObject()));
        sp[-1] = sp[-3];
        set(&sp[-1], get(&sp[-3]));
        return callImacroInfallibly(callprop_imacros.scriptgetter);

      case JSOP_GETTHISPROP:
      case JSOP_GETARGPROP:
      case JSOP_GETLOCALPROP:
        sp += 2;
        sp[-2] = getter;
        set(&sp[-2], w.immpObjGC(&getter.toObject()));
        sp[-1] = ObjectValue(*obj);
        set(&sp[-1], obj_ins);
        return callImacroInfallibly(getthisprop_imacros.scriptgetter);

      default:
        RETURN_STOP("cannot trace script getter for this opcode");
    }
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getCharCodeAt(JSString *str, LIns* str_ins, LIns* idx_ins, LIns** out)
{
    CHECK_STATUS(makeNumberInt32(idx_ins, &idx_ins));
    idx_ins = w.ui2p(idx_ins);
    LIns *lengthAndFlags_ins = w.name(w.ldpStringLengthAndFlags(str_ins), "mLengthAndFlags");
    if (MaybeBranch mbr = w.jt(w.eqp0(w.andp(lengthAndFlags_ins, w.nameImmw(JSString::ROPE_BIT)))))
    {
        w.call(&js_Flatten_ci, &str_ins);
        w.label(mbr);
    }

    guard(true,
          w.ltup(idx_ins, w.rshupN(lengthAndFlags_ins, JSString::FLAGS_LENGTH_SHIFT)),
          snapshot(MISMATCH_EXIT));
    *out = w.i2d(w.getStringChar(str_ins, idx_ins));
    return RECORD_CONTINUE;
}

JS_STATIC_ASSERT(sizeof(JSString) == 16 || sizeof(JSString) == 32);


JS_REQUIRES_STACK LIns*
TraceRecorder::getUnitString(LIns* str_ins, LIns* idx_ins)
{
    LIns *ch_ins = w.getStringChar(str_ins, idx_ins);
    guard(true, w.ltuiN(ch_ins, UNIT_STRING_LIMIT), MISMATCH_EXIT);
    return w.addp(w.nameImmpNonGC(JSString::unitStringTable),
                  w.lshpN(w.ui2p(ch_ins), (sizeof(JSString) == 16) ? 4 : 5));
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getCharAt(JSString *str, LIns* str_ins, LIns* idx_ins, JSOp mode, LIns** out)
{
    CHECK_STATUS(makeNumberInt32(idx_ins, &idx_ins));
    idx_ins = w.ui2p(idx_ins);
    LIns *lengthAndFlags_ins = w.ldpStringLengthAndFlags(str_ins);
    if (MaybeBranch mbr = w.jt(w.eqp0(w.andp(lengthAndFlags_ins,
                                             w.nameImmw(JSString::ROPE_BIT)))))
    {
        w.call(&js_Flatten_ci, &str_ins);
        w.label(mbr);
    }

    LIns* inRange = w.ltup(idx_ins, w.rshupN(lengthAndFlags_ins, JSString::FLAGS_LENGTH_SHIFT));

    if (mode == JSOP_GETELEM) {
        guard(true, inRange, MISMATCH_EXIT);

        *out = getUnitString(str_ins, idx_ins);
    } else {
        LIns *phi_ins = w.allocp(sizeof(JSString *));
        w.stAlloc(w.nameImmpNonGC(cx->runtime->emptyString), phi_ins);

        if (MaybeBranch mbr = w.jf(inRange)) {
            LIns *unitstr_ins = getUnitString(str_ins, idx_ins);
            w.stAlloc(unitstr_ins, phi_ins);
            w.label(mbr);
        }
        *out = w.ldpAlloc(phi_ins);
    }
    return RECORD_CONTINUE;
}

// Typed array tracing depends on EXPANDED_LOADSTORE and F2I
#if NJ_EXPANDED_LOADSTORE_SUPPORTED && NJ_F2I_SUPPORTED
static bool OkToTraceTypedArrays = true;
#else
static bool OkToTraceTypedArrays = false;
#endif

JS_REQUIRES_STACK void
TraceRecorder::guardNotHole(LIns *argsobj_ins, LIns *idx_ins)
{
    // vp = &argsobj->slots[JSSLOT_ARGS_DATA].slots[idx]
    LIns* argsData_ins = w.getObjPrivatizedSlot(argsobj_ins, JSObject::JSSLOT_ARGS_DATA);
    LIns* slotOffset_ins = w.addp(w.nameImmw(offsetof(ArgumentsData, slots)),
                                  w.ui2p(w.muliN(idx_ins, sizeof(Value))));
    LIns* vp_ins = w.addp(argsData_ins, slotOffset_ins);

    guard(false,
          w.name(is_boxed_magic(ArgsSlotsAddress(vp_ins), JS_ARGS_HOLE), "guard(not deleted arg)"),
          MISMATCH_EXIT);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETELEM()
{
    bool call = *cx->regs->pc == JSOP_CALLELEM;

    Value& idx = stackval(-1);
    Value& lval = stackval(-2);

    LIns* obj_ins = get(&lval);
    LIns* idx_ins = get(&idx);

    // Special case for array-like access of strings.
    if (lval.isString() && hasInt32Repr(idx)) {
        if (call)
            RETURN_STOP_A("JSOP_CALLELEM on a string");
        int i = asInt32(idx);
        if (size_t(i) >= lval.toString()->length())
            RETURN_STOP_A("Invalid string index in JSOP_GETELEM");
        LIns* char_ins;
        CHECK_STATUS_A(getCharAt(lval.toString(), obj_ins, idx_ins, JSOP_GETELEM, &char_ins));
        set(&lval, char_ins);
        return ARECORD_CONTINUE;
    }

    if (lval.isPrimitive())
        RETURN_STOP_A("JSOP_GETLEM on a primitive");
    RETURN_IF_XML_A(lval);

    JSObject* obj = &lval.toObject();
    if (obj == globalObj)
        RETURN_STOP_A("JSOP_GETELEM on global");
    LIns* v_ins;

    /* Property access using a string name or something we have to stringify. */
    if (!idx.isInt32()) {
        if (!idx.isPrimitive())
            RETURN_STOP_A("object used as index");

        return InjectStatus(getPropertyByName(obj_ins, &idx, &lval));
    }

    if (obj->isArguments()) {
        // Don't even try to record if out of range or reading a deleted arg
        int32 int_idx = idx.toInt32();
        if (int_idx < 0 || int_idx >= (int32)obj->getArgsInitialLength())
            RETURN_STOP_A("cannot trace arguments with out of range index");
        if (obj->getArgsElement(int_idx).isMagic(JS_ARGS_HOLE))
            RETURN_STOP_A("reading deleted args element");

        // Only trace reading arguments out of active, tracked frame
        unsigned depth;
        JSStackFrame *afp = guardArguments(obj, obj_ins, &depth);
        if (afp) {
            Value* vp = &afp->canonicalActualArg(int_idx);
            if (idx_ins->isImmD()) {
                JS_ASSERT(int_idx == (int32)idx_ins->immD());
                guardNotHole(obj_ins, w.nameImmi(int_idx));
                v_ins = get(vp);
            } else {
                // If the index is not a constant expression, we generate LIR to load the value from
                // the native stack area. The guard on js_ArgumentClass above ensures the up-to-date
                // value has been written back to the native stack area.
                CHECK_STATUS_A(makeNumberInt32(idx_ins, &idx_ins));

                /*
                 * For small nactual,
                 * 0 <= int_idx < nactual iff unsigned(int_idx) < unsigned(nactual).
                 */
                guard(true,
                      w.name(w.ltui(idx_ins, w.nameImmui(afp->numActualArgs())),
                             "guard(upvar index in range)"),
                      MISMATCH_EXIT);

                guardNotHole(obj_ins, idx_ins);

                JSValueType type = getCoercedType(*vp);

                // Guard that the argument has the same type on trace as during recording.
                LIns* typemap_ins;
                if (depth == 0) {
                    // In this case, we are in the same frame where the arguments object was created.
                    // The entry type map is not necessarily up-to-date, so we capture a new type map
                    // for this point in the code.
                    unsigned stackSlots = NativeStackSlots(cx, 0 /* callDepth */);
                    JSValueType* typemap = new (traceAlloc()) JSValueType[stackSlots];
                    DetermineTypesVisitor detVisitor(*this, typemap);
                    VisitStackSlots(detVisitor, cx, 0);
                    typemap_ins = w.nameImmpNonGC(typemap + 2 /* callee, this */);
                } else {
                    // In this case, we are in a deeper frame from where the arguments object was
                    // created. The type map at the point of the call out from the creation frame
                    // is accurate.
                    // Note: this relies on the assumption that we abort on setting an element of
                    // an arguments object in any deeper frame.
                    LIns* fip_ins = w.ldpRstack(lirbuf->rp, (callDepth-depth)*sizeof(FrameInfo*));
                    typemap_ins = w.addp(fip_ins, w.nameImmw(sizeof(FrameInfo) + 2/*callee,this*/ * sizeof(JSValueType)));
                }

                LIns* type_ins = w.lduc2uiConstTypeMapEntry(typemap_ins, idx_ins);
                guard(true,
                      w.name(w.eqi(type_ins, w.immi(type)), "guard(type-stable upvar)"),
                      BRANCH_EXIT);

                // Read the value out of the native stack area.
                size_t stackOffset = nativespOffset(&afp->canonicalActualArg(0));
                LIns* args_addr_ins = w.addp(lirbuf->sp, w.nameImmw(stackOffset));
                LIns* argi_addr_ins = w.addp(args_addr_ins,
                                             w.ui2p(w.muli(idx_ins, w.nameImmi(sizeof(double)))));

                // The Address could be more precise, but ValidateWriter
                // doesn't recognise the complex expression involving 'sp' as
                // an stack access, and it's not worth the effort to be
                // more precise because this case is rare.
                v_ins = stackLoad(AnyAddress(argi_addr_ins), type);
            }
            JS_ASSERT(v_ins);
            set(&lval, v_ins);
            if (call)
                set(&idx, obj_ins);
            return ARECORD_CONTINUE;
        }
        RETURN_STOP_A("can't reach arguments object's frame");
    }

    if (obj->isDenseArray()) {
        // Fast path for dense arrays accessed with a integer index.
        Value* vp;
        LIns* addr_ins;

        VMSideExit* branchExit = snapshot(BRANCH_EXIT);
        guardDenseArray(obj_ins, branchExit);
        CHECK_STATUS_A(denseArrayElement(lval, idx, vp, v_ins, addr_ins, branchExit));
        set(&lval, v_ins);
        if (call)
            set(&idx, obj_ins);
        return ARECORD_CONTINUE;
    }

    if (OkToTraceTypedArrays && js_IsTypedArray(obj)) {
        // Fast path for typed arrays accessed with a integer index.
        Value* vp;
        guardClass(obj_ins, obj->getClass(), snapshot(BRANCH_EXIT), LOAD_CONST);
        CHECK_STATUS_A(typedArrayElement(lval, idx, vp, v_ins));
        set(&lval, v_ins);
        if (call)
            set(&idx, obj_ins);
        return ARECORD_CONTINUE;
    }

    return InjectStatus(getPropertyByIndex(obj_ins, idx_ins, &lval));
}

/* Functions used by JSOP_SETELEM */

static JSBool FASTCALL
SetPropertyByName(JSContext* cx, JSObject* obj, JSString** namep, Value* vp, JSBool strict)
{
    LeaveTraceIfGlobalObject(cx, obj);

    jsid id;
    if (!RootedStringToId(cx, namep, &id) || !obj->setProperty(cx, id, vp, strict)) {
        SetBuiltinError(cx);
        return false;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_5(static, BOOL_FAIL, SetPropertyByName, 
                     CONTEXT, OBJECT, STRINGPTR, VALUEPTR, BOOL,
                     0, ACCSET_STORE_ANY)

static JSBool FASTCALL
InitPropertyByName(JSContext* cx, JSObject* obj, JSString** namep, ValueArgType arg)
{
    LeaveTraceIfGlobalObject(cx, obj);

    jsid id;
    if (!RootedStringToId(cx, namep, &id) ||
        !obj->defineProperty(cx, id, ValueArgToConstRef(arg), NULL, NULL, JSPROP_ENUMERATE)) {
        SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, InitPropertyByName, CONTEXT, OBJECT, STRINGPTR, VALUE,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::initOrSetPropertyByName(LIns* obj_ins, Value* idvalp, Value* rvalp, bool init)
{
    CHECK_STATUS(primitiveToStringInPlace(idvalp));

    if (init) {
        LIns* v_ins = box_value_for_native_call(*rvalp, get(rvalp));
        enterDeepBailCall();
        LIns* idvalp_ins = w.name(addr(idvalp), "idvalp");
        LIns* args[] = {v_ins, idvalp_ins, obj_ins, cx_ins};
        pendingGuardCondition = w.call(&InitPropertyByName_ci, args);
    } else {
        // See note in getPropertyByName about vp.
        LIns* vp_ins = box_value_into_alloc(*rvalp, get(rvalp));
        enterDeepBailCall();
        LIns* idvalp_ins = w.name(addr(idvalp), "idvalp");
        LIns* args[] = { strictModeCode_ins, vp_ins, idvalp_ins, obj_ins, cx_ins };
        pendingGuardCondition = w.call(&SetPropertyByName_ci, args);
    }

    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

static JSBool FASTCALL
SetPropertyByIndex(JSContext* cx, JSObject* obj, int32 index, Value* vp, JSBool strict)
{
    LeaveTraceIfGlobalObject(cx, obj);

    AutoIdRooter idr(cx);
    if (!js_Int32ToId(cx, index, idr.addr()) || !obj->setProperty(cx, idr.id(), vp, strict)) {
        SetBuiltinError(cx);
        return false;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_5(static, BOOL_FAIL, SetPropertyByIndex, CONTEXT, OBJECT, INT32, VALUEPTR, BOOL,
                     0, ACCSET_STORE_ANY)

static JSBool FASTCALL
InitPropertyByIndex(JSContext* cx, JSObject* obj, int32 index, ValueArgType arg)
{
    LeaveTraceIfGlobalObject(cx, obj);

    AutoIdRooter idr(cx);
    if (!js_Int32ToId(cx, index, idr.addr()) ||
        !obj->defineProperty(cx, idr.id(), ValueArgToConstRef(arg), NULL, NULL, JSPROP_ENUMERATE)) {
        SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, InitPropertyByIndex, CONTEXT, OBJECT, INT32, VALUE,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::initOrSetPropertyByIndex(LIns* obj_ins, LIns* index_ins, Value* rvalp, bool init)
{
    CHECK_STATUS(makeNumberInt32(index_ins, &index_ins));

    if (init) {
        LIns* rval_ins = box_value_for_native_call(*rvalp, get(rvalp));
        enterDeepBailCall();
        LIns* args[] = {rval_ins, index_ins, obj_ins, cx_ins};
        pendingGuardCondition = w.call(&InitPropertyByIndex_ci, args);
    } else {
        // See note in getPropertyByName about vp.
        LIns* vp_ins = box_value_into_alloc(*rvalp, get(rvalp));
        enterDeepBailCall();
        LIns* args[] = {strictModeCode_ins, vp_ins, index_ins, obj_ins, cx_ins};
        pendingGuardCondition = w.call(&SetPropertyByIndex_ci, args);
    }

    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::setElem(int lval_spindex, int idx_spindex, int v_spindex)
{
    Value& v = stackval(v_spindex);
    Value& idx = stackval(idx_spindex);
    Value& lval = stackval(lval_spindex);

    if (lval.isPrimitive())
        RETURN_STOP_A("left JSOP_SETELEM operand is not an object");
    RETURN_IF_XML_A(lval);

    JSObject* obj = &lval.toObject();
    LIns* obj_ins = get(&lval);
    LIns* idx_ins = get(&idx);
    LIns* v_ins = get(&v);

    if (obj->isArguments())
        RETURN_STOP_A("can't trace setting elements of the |arguments| object");

    if (obj == globalObj)
        RETURN_STOP_A("can't trace setting elements on the global object");

    if (!idx.isInt32()) {
        if (!idx.isPrimitive())
            RETURN_STOP_A("non-primitive index");
        CHECK_STATUS_A(initOrSetPropertyByName(obj_ins, &idx, &v,
                                             *cx->regs->pc == JSOP_INITELEM));
    } else if (OkToTraceTypedArrays && js_IsTypedArray(obj)) {
        // Fast path: assigning to element of typed array.
        VMSideExit* branchExit = snapshot(BRANCH_EXIT);

        // Ensure array is a typed array and is the same type as what was written
        guardClass(obj_ins, obj->getClass(), branchExit, LOAD_CONST);

        js::TypedArray* tarray = js::TypedArray::fromJSObject(obj);

        LIns* priv_ins = w.ldpObjPrivate(obj_ins);

        // The index was on the stack and is therefore a LIR float; force it to
        // be an integer.                              
        CHECK_STATUS_A(makeNumberInt32(idx_ins, &idx_ins));

        // Ensure idx >= 0 && idx < length (by using uint32)
        guard(true,
              w.ltui(idx_ins, w.ldiConstTypedArrayLength(priv_ins)),
              OVERFLOW_EXIT);

        // We're now ready to store
        LIns* data_ins = w.ldpConstTypedArrayData(priv_ins);
        LIns* pidx_ins = w.ui2p(idx_ins);
        LIns* typed_v_ins = v_ins;

        // If it's not a number, convert objects to NaN,
        // null to 0, and call StringToNumber or BooleanOrUndefinedToNumber
        // for those.
        if (!v.isNumber()) {
            if (v.isNull()) {
                typed_v_ins = w.immd(0);
            } else if (v.isUndefined()) {
                typed_v_ins = w.immd(js_NaN);
            } else if (v.isString()) {
                LIns* args[] = { typed_v_ins, cx_ins };
                typed_v_ins = w.call(&js_StringToNumber_ci, args);
            } else if (v.isBoolean()) {
                JS_ASSERT(v.isBoolean());
                typed_v_ins = w.i2d(typed_v_ins);
            } else {
                typed_v_ins = w.immd(js_NaN);
            }
        }

        switch (tarray->type) {
          case js::TypedArray::TYPE_INT8:
          case js::TypedArray::TYPE_INT16:
          case js::TypedArray::TYPE_INT32:
            typed_v_ins = d2i(typed_v_ins);
            break;
          case js::TypedArray::TYPE_UINT8:
          case js::TypedArray::TYPE_UINT16:
          case js::TypedArray::TYPE_UINT32:
            typed_v_ins = d2u(typed_v_ins);
            break;
          case js::TypedArray::TYPE_UINT8_CLAMPED:
            if (IsPromoteInt(typed_v_ins)) {
                typed_v_ins = w.demote(typed_v_ins);
                typed_v_ins = w.cmovi(w.ltiN(typed_v_ins, 0),
                                      w.immi(0),
                                      w.cmovi(w.gtiN(typed_v_ins, 0xff),
                                                     w.immi(0xff),
                                                     typed_v_ins));
            } else {
                typed_v_ins = w.call(&js_TypedArray_uint8_clamp_double_ci, &typed_v_ins);
            }
            break;
          case js::TypedArray::TYPE_FLOAT32:
          case js::TypedArray::TYPE_FLOAT64:
            // Do nothing, this is already a float
            break;
          default:
            JS_NOT_REACHED("Unknown typed array type in tracer");       
        }

        switch (tarray->type) {
          case js::TypedArray::TYPE_INT8:
          case js::TypedArray::TYPE_UINT8_CLAMPED:
          case js::TypedArray::TYPE_UINT8:
            w.sti2cTypedArrayElement(typed_v_ins, data_ins, pidx_ins);
            break;
          case js::TypedArray::TYPE_INT16:
          case js::TypedArray::TYPE_UINT16:
            w.sti2sTypedArrayElement(typed_v_ins, data_ins, pidx_ins);
            break;
          case js::TypedArray::TYPE_INT32:
          case js::TypedArray::TYPE_UINT32:
            w.stiTypedArrayElement(typed_v_ins, data_ins, pidx_ins);
            break;
          case js::TypedArray::TYPE_FLOAT32:
            w.std2fTypedArrayElement(typed_v_ins, data_ins, pidx_ins);
            break;
          case js::TypedArray::TYPE_FLOAT64:
            w.stdTypedArrayElement(typed_v_ins, data_ins, pidx_ins);
            break;
          default:
            JS_NOT_REACHED("Unknown typed array type in tracer");       
        }
    } else if (idx.toInt32() < 0 || !obj->isDenseArray()) {
        CHECK_STATUS_A(initOrSetPropertyByIndex(obj_ins, idx_ins, &v,
                                                *cx->regs->pc == JSOP_INITELEM));
    } else {
        // Fast path: assigning to element of dense array.
        VMSideExit* branchExit = snapshot(BRANCH_EXIT);
        VMSideExit* mismatchExit = snapshot(MISMATCH_EXIT);

        // Make sure the array is actually dense.
        if (!obj->isDenseArray()) 
            return ARECORD_STOP;
        guardDenseArray(obj_ins, branchExit);

        // The index was on the stack and is therefore a LIR float. Force it to
        // be an integer.
        CHECK_STATUS_A(makeNumberInt32(idx_ins, &idx_ins));

        if (!js_EnsureDenseArrayCapacity(cx, obj, idx.toInt32()))
            RETURN_STOP_A("couldn't ensure dense array capacity for setelem");

        // Grow the array if the index exceeds the capacity.  This happens
        // rarely, eg. less than 1% of the time in SunSpider.
        LIns* capacity_ins = w.ldiDenseArrayCapacity(obj_ins);
        /*
         * It's important that CSE works across this control-flow diamond
         * because it really helps series of interleaved GETELEM and SETELEM
         * operations.  Likewise with the diamond below.
         */
        w.pauseAddingCSEValues();
        if (MaybeBranch mbr = w.jt(w.ltui(idx_ins, capacity_ins))) {
            LIns* args[] = { idx_ins, obj_ins, cx_ins };
            LIns* res_ins = w.call(&js_EnsureDenseArrayCapacity_ci, args);
            guard(false, w.eqi0(res_ins), mismatchExit);
            w.label(mbr);
        }
        w.resumeAddingCSEValues();

        // Get the address of the element.
        LIns *elem_ins = w.getDslotAddress(obj_ins, idx_ins);

        // If we are overwriting a hole:
        // - Guard that we don't have any indexed properties along the prototype chain.
        // - Check if the length has changed;  if so, update it to index+1.
        // This happens moderately often, eg. close to 10% of the time in
        // SunSpider, and for some benchmarks it's close to 100%.
        Address dslotAddr = DSlotsAddress(elem_ins);
        LIns* isHole_ins = w.eqi(
#if JS_BITS_PER_WORD == 32
                                 w.ldiValueTag(dslotAddr),
#else
                                 w.q2i(w.rshuqN(w.ldq(dslotAddr), JSVAL_TAG_SHIFT)),
#endif
                                 w.nameImmui(JSVAL_TAG_MAGIC));
        w.pauseAddingCSEValues();
        if (MaybeBranch mbr1 = w.jf(isHole_ins)) {
            CHECK_STATUS_A(guardPrototypeHasNoIndexedProperties(obj, obj_ins, mismatchExit));
            LIns* length_ins = w.lduiObjPrivate(obj_ins);
            if (MaybeBranch mbr2 = w.jt(w.ltui(idx_ins, length_ins))) {
                LIns* newLength_ins = w.name(w.addiN(idx_ins, 1), "newLength");
                w.stuiObjPrivate(obj_ins, newLength_ins);
                w.label(mbr2);
            }
            w.label(mbr1);
        }
        w.resumeAddingCSEValues();

        // Right, actually set the element.
        box_value_into(v, v_ins, dslotAddr);
    }

    jsbytecode* pc = cx->regs->pc;
    if (*pc == JSOP_SETELEM && pc[JSOP_SETELEM_LENGTH] != JSOP_POP)
        set(&lval, v_ins);

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETELEM()
{
    return setElem(-3, -2, -1);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLNAME()
{
    JSObject* obj = &cx->fp()->scopeChain();
    if (obj != globalObj) {
        Value* vp;
        LIns* ins;
        NameResult nr;
        CHECK_STATUS_A(scopeChainProp(obj, vp, ins, nr));
        stack(0, ins);
        stack(1, w.immiUndefined());
        return ARECORD_CONTINUE;
    }

    LIns* obj_ins = w.immpObjGC(globalObj);
    JSObject* obj2;
    PCVal pcval;

    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    if (pcval.isNull() || !pcval.isFunObj())
        RETURN_STOP_A("callee is not an object");

    stack(0, w.immpObjGC(&pcval.toFunObj()));
    stack(1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_DEFINE_CALLINFO_5(extern, UINT32, GetUpvarArgOnTrace, CONTEXT, UINT32, INT32, UINT32,
                     DOUBLEPTR, 0, ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_5(extern, UINT32, GetUpvarVarOnTrace, CONTEXT, UINT32, INT32, UINT32,
                     DOUBLEPTR, 0, ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_5(extern, UINT32, GetUpvarStackOnTrace, CONTEXT, UINT32, INT32, UINT32,
                     DOUBLEPTR, 0, ACCSET_STORE_ANY)

/*
 * Record LIR to get the given upvar. Return the LIR instruction for the upvar
 * value. NULL is returned only on a can't-happen condition with an invalid
 * typemap. The value of the upvar is returned as v.
 */
JS_REQUIRES_STACK LIns*
TraceRecorder::upvar(JSScript* script, JSUpvarArray* uva, uintN index, Value& v)
{
    /*
     * Try to find the upvar in the current trace's tracker. For &vr to be
     * the address of the jsval found in js::GetUpvar, we must initialize
     * vr directly with the result, so it is a reference to the same location.
     * It does not work to assign the result to v, because v is an already
     * existing reference that points to something else.
     */
    UpvarCookie cookie = uva->vector[index];
    const Value& vr = GetUpvar(cx, script->staticLevel, cookie);
    v = vr;

    if (LIns* ins = attemptImport(&vr))
        return ins;

    /*
     * The upvar is not in the current trace, so get the upvar value exactly as
     * the interpreter does and unbox.
     */
    uint32 level = script->staticLevel - cookie.level();
    uint32 cookieSlot = cookie.slot();
    JSStackFrame* fp = cx->findFrameAtLevel(level);
    const CallInfo* ci;
    int32 slot;
    if (!fp->isFunctionFrame() || fp->isEvalFrame()) {
        ci = &GetUpvarStackOnTrace_ci;
        slot = cookieSlot;
    } else if (cookieSlot < fp->numFormalArgs()) {
        ci = &GetUpvarArgOnTrace_ci;
        slot = cookieSlot;
    } else if (cookieSlot == UpvarCookie::CALLEE_SLOT) {
        ci = &GetUpvarArgOnTrace_ci;
        slot = -2;
    } else {
        ci = &GetUpvarVarOnTrace_ci;
        slot = cookieSlot - fp->numFormalArgs();
    }

    LIns* outp = w.allocp(sizeof(double));
    LIns* args[] = {
        outp,
        w.nameImmi(callDepth),
        w.nameImmi(slot),
        w.nameImmi(level),
        cx_ins
    };
    LIns* call_ins = w.call(ci, args);
    JSValueType type = getCoercedType(v);
    guard(true,
          w.name(w.eqi(call_ins, w.immi(type)), "guard(type-stable upvar)"),
          BRANCH_EXIT);
    return stackLoad(AllocSlotsAddress(outp), type);
}

/*
 * Generate LIR to load a value from the native stack. This method ensures that
 * the correct LIR load operator is used.
 */
LIns*
TraceRecorder::stackLoad(Address addr, uint8 type)
{
    switch (type) {
      case JSVAL_TYPE_DOUBLE:
        return w.ldd(addr);
      case JSVAL_TYPE_NONFUNOBJ:
      case JSVAL_TYPE_STRING:
      case JSVAL_TYPE_FUNOBJ:
      case JSVAL_TYPE_NULL:
        return w.ldp(addr);
      case JSVAL_TYPE_INT32:
        return w.i2d(w.ldi(addr));
      case JSVAL_TYPE_BOOLEAN:
      case JSVAL_TYPE_UNDEFINED:
      case JSVAL_TYPE_MAGIC:
        return w.ldi(addr);
      case JSVAL_TYPE_BOXED:
      default:
        JS_NOT_REACHED("found jsval type in an upvar type map entry");
        return NULL;
    }
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETUPVAR()
{
    uintN index = GET_UINT16(cx->regs->pc);
    JSScript *script = cx->fp()->script();
    JSUpvarArray* uva = script->upvars();
    JS_ASSERT(index < uva->length);

    Value v;
    LIns* upvar_ins = upvar(script, uva, index, v);
    if (!upvar_ins)
        return ARECORD_STOP;
    stack(0, upvar_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLUPVAR()
{
    CHECK_STATUS_A(record_JSOP_GETUPVAR());
    stack(1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETFCSLOT()
{
    JSObject& callee = cx->fp()->callee();
    LIns* callee_ins = get(&cx->fp()->calleeValue());

    LIns* upvars_ins = w.getObjPrivatizedSlot(callee_ins, JSObject::JSSLOT_FLAT_CLOSURE_UPVARS);

    unsigned index = GET_UINT16(cx->regs->pc);
    LIns *v_ins = unbox_value(callee.getFlatClosureUpvar(index),
                              FCSlotsAddress(upvars_ins, index),
                              snapshot(BRANCH_EXIT));
    stack(0, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLFCSLOT()
{
    CHECK_STATUS_A(record_JSOP_GETFCSLOT());
    stack(1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardCallee(Value& callee)
{
    JSObject& callee_obj = callee.toObject();
    JS_ASSERT(callee_obj.isFunction());
    JSFunction* callee_fun = (JSFunction*) callee_obj.getPrivate();

    /*
     * First, guard on the callee's function (JSFunction*) identity. This is
     * necessary since tracing always inlines function calls. But note that
     * TR::functionCall avoids calling TR::guardCallee for constant methods
     * (those hit in the property cache from JSOP_CALLPROP).
     */
    VMSideExit* branchExit = snapshot(BRANCH_EXIT);
    LIns* callee_ins = get(&callee);
    tree->gcthings.addUnique(callee);

    guard(true,
          w.eqp(w.ldpObjPrivate(callee_ins), w.nameImmpNonGC(callee_fun)),
          branchExit);

    /*
     * Second, consider guarding on the parent scope of the callee.
     *
     * As long as we guard on parent scope, we are guaranteed when recording
     * variable accesses for a Call object having no private data that we can
     * emit code that avoids checking for an active JSStackFrame for the Call
     * object (which would hold fresh variable values -- the Call object's
     * slots would be stale until the stack frame is popped). This is because
     * Call objects can't pick up a new stack frame in their private slot once
     * they have none. TR::callProp and TR::setCallProp depend on this fact and
     * document where; if this guard is removed make sure to fix those methods.
     * Search for the "parent guard" comments in them.
     *
     * In general, a loop in an escaping function scoped by Call objects could
     * be traced before the function has returned, and the trace then triggered
     * after, or vice versa. The function must escape, i.e., be a "funarg", or
     * else there's no need to guard callee parent at all. So once we know (by
     * static analysis) that a function may escape, we cannot avoid guarding on
     * either the private data of the Call object or the Call object itself, if
     * we wish to optimize for the particular deactivated stack frame (null
     * private data) case as noted above.
     */
    if (FUN_INTERPRETED(callee_fun) &&
        (!FUN_NULL_CLOSURE(callee_fun) || callee_fun->u.i.nupvars != 0)) {
        JSObject* parent = callee_obj.getParent();

        if (parent != globalObj) {
            if (!parent->isCall())
                RETURN_STOP("closure scoped by neither the global object nor a Call object");

            guard(true,
                  w.eqp(w.ldpObjParent(callee_ins), w.immpObjGC(parent)),
                  branchExit);
        }
    }
    return RECORD_CONTINUE;
}

/*
 * Prepare the given |arguments| object to be accessed on trace. If the return
 * value is non-NULL, then the given |arguments| object refers to a frame on
 * the current trace and is guaranteed to refer to the same frame on trace for
 * all later executions.
 */
JS_REQUIRES_STACK JSStackFrame *
TraceRecorder::guardArguments(JSObject *obj, LIns* obj_ins, unsigned *depthp)
{
    JS_ASSERT(obj->isArguments());

    JSStackFrame *afp = frameIfInRange(obj, depthp);
    if (!afp)
        return NULL;

    VMSideExit *exit = snapshot(MISMATCH_EXIT);
    guardClass(obj_ins, obj->getClass(), exit, LOAD_CONST);

    LIns* args_ins = getFrameObjPtr(afp->addressOfArgs());
    LIns* cmp = w.eqp(args_ins, obj_ins);
    guard(true, cmp, exit);
    return afp;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::interpretedFunctionCall(Value& fval, JSFunction* fun, uintN argc, bool constructing)
{
    /*
     * The function's identity (JSFunction and therefore JSScript) is guarded,
     * so we can optimize for the empty script singleton right away. No need to
     * worry about crossing globals or relocating argv, even, in this case!
     *
     * Note that the interpreter shortcuts empty-script call and construct too,
     * and does not call any TR::record_*CallComplete hook.
     */
    if (fun->u.i.script->isEmpty()) {
        LIns* rval_ins;
        if (constructing) {
            LIns* args[] = { get(&fval), w.nameImmpNonGC(&js_ObjectClass), cx_ins };
            LIns* tv_ins = w.call(&js_CreateThisFromTrace_ci, args);
            guard(false, w.eqp0(tv_ins), OOM_EXIT);
            rval_ins = tv_ins;
        } else {
            rval_ins = w.immiUndefined();
        }
        stack(-2 - argc, rval_ins);
        return RECORD_CONTINUE;
    }

    if (fval.toObject().getGlobal() != globalObj)
        RETURN_STOP("JSOP_CALL or JSOP_NEW crosses global scopes");

    JSStackFrame* const fp = cx->fp();

    // Generate a type map for the outgoing frame and stash it in the LIR
    unsigned stackSlots = NativeStackSlots(cx, 0 /* callDepth */);
    FrameInfo* fi = (FrameInfo*)
        tempAlloc().alloc(sizeof(FrameInfo) + stackSlots * sizeof(JSValueType));
    JSValueType* typemap = (JSValueType*)(fi + 1);

    DetermineTypesVisitor detVisitor(*this, typemap);
    VisitStackSlots(detVisitor, cx, 0);

    JS_ASSERT(argc < FrameInfo::CONSTRUCTING_FLAG);

    tree->gcthings.addUnique(fval);
    fi->pc = cx->regs->pc;
    fi->imacpc = fp->maybeImacropc();
    fi->spdist = cx->regs->sp - fp->slots();
    fi->set_argc(uint16(argc), constructing);
    fi->callerHeight = stackSlots - (2 + argc);
    fi->callerArgc = fp->isGlobalFrame() || fp->isEvalFrame() ? 0 : fp->numActualArgs();

    if (callDepth >= tree->maxCallDepth)
        tree->maxCallDepth = callDepth + 1;

    fi = traceMonitor->frameCache->memoize(fi);
    if (!fi)
        RETURN_STOP("out of memory");
    w.stRstack(w.nameImmpNonGC(fi), lirbuf->rp, callDepth * sizeof(FrameInfo*));

#if defined JS_JIT_SPEW
    debug_only_printf(LC_TMTracer, "iFC frameinfo=%p, stack=%d, map=", (void*)fi,
                      fi->callerHeight);
    for (unsigned i = 0; i < fi->callerHeight; i++)
        debug_only_printf(LC_TMTracer, "%c", TypeToChar(fi->get_typemap()[i]));
    debug_only_print0(LC_TMTracer, "\n");
#endif

    updateAtoms(fun->u.i.script);
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALL()
{
    uintN argc = GET_ARGC(cx->regs->pc);
    cx->assertValidStackDepth(argc + 2);
    return InjectStatus(functionCall(argc,
                                     (cx->fp()->hasImacropc() && *cx->fp()->imacropc() == JSOP_APPLY)
                                        ? JSOP_APPLY
                                        : JSOP_CALL));
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

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_APPLY()
{
    jsbytecode *pc = cx->regs->pc;
    uintN argc = GET_ARGC(pc);
    cx->assertValidStackDepth(argc + 2);

    Value* vp = cx->regs->sp - (argc + 2);
    jsuint length = 0;
    JSObject* aobj = NULL;
    LIns* aobj_ins = NULL;

    JS_ASSERT(!cx->fp()->hasImacropc());

    if (!IsFunctionObject(vp[0]))
        return record_JSOP_CALL();
    RETURN_IF_XML_A(vp[0]);

    JSObject* obj = &vp[0].toObject();
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, obj);
    if (FUN_INTERPRETED(fun))
        return record_JSOP_CALL();

    bool apply = fun->u.n.native == js_fun_apply;
    if (!apply && fun->u.n.native != js_fun_call)
        return record_JSOP_CALL();

    /*
     * We don't trace apply and call with a primitive 'this', which is the
     * first positional parameter, unless 'this' is null.  That's ok.
     */
    if (argc > 0 && !vp[2].isObjectOrNull())
        return record_JSOP_CALL();

    /*
     * Guard on the identity of this, which is the function we are applying.
     */
    if (!IsFunctionObject(vp[1]))
        RETURN_STOP_A("callee is not a function");
    CHECK_STATUS_A(guardCallee(vp[1]));

    if (apply && argc >= 2) {
        if (argc != 2)
            RETURN_STOP_A("apply with excess arguments");
        if (vp[3].isPrimitive())
            RETURN_STOP_A("arguments parameter of apply is primitive");
        aobj = &vp[3].toObject();
        aobj_ins = get(&vp[3]);

        /*
         * We trace dense arrays and arguments objects. The code we generate
         * for apply uses imacros to handle a specific number of arguments.
         */
        if (aobj->isDenseArray()) {
            guardDenseArray(aobj_ins, MISMATCH_EXIT);
            length = aobj->getArrayLength();
            guard(true,
                  w.eqiN(w.lduiObjPrivate(aobj_ins), length),
                  BRANCH_EXIT);
        } else if (aobj->isArguments()) {
            unsigned depth;
            JSStackFrame *afp = guardArguments(aobj, aobj_ins, &depth);
            if (!afp)
                RETURN_STOP_A("can't reach arguments object's frame");
            if (aobj->isArgsLengthOverridden())
                RETURN_STOP_A("can't trace arguments with overridden length");
            guardArgsLengthNotAssigned(aobj_ins);
            length = afp->numActualArgs();
        } else {
            RETURN_STOP_A("arguments parameter of apply is not a dense array or argments object");
        }

        if (length >= JS_ARRAY_LENGTH(apply_imacro_table))
            RETURN_STOP_A("too many arguments to apply");

        return InjectStatus(callImacro(apply_imacro_table[length]));
    }

    if (argc >= JS_ARRAY_LENGTH(call_imacro_table))
        RETURN_STOP_A("too many arguments to call");

    return InjectStatus(callImacro(call_imacro_table[argc]));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_NativeCallComplete()
{
    if (pendingSpecializedNative == IGNORE_NATIVE_CALL_COMPLETE_CALLBACK)
        return ARECORD_CONTINUE;

#ifdef DEBUG
    JS_ASSERT(pendingSpecializedNative);
    jsbytecode* pc = cx->regs->pc;
    JS_ASSERT(*pc == JSOP_CALL || *pc == JSOP_APPLY || *pc == JSOP_NEW || *pc == JSOP_SETPROP);
#endif

    Value& v = stackval(-1);
    LIns* v_ins = get(&v);

    /*
     * At this point the generated code has already called the native function
     * and we can no longer fail back to the original pc location (JSOP_CALL)
     * because that would cause the interpreter to re-execute the native
     * function, which might have side effects.
     *
     * Instead, the snapshot() call below sees that we are currently parked on
     * a traceable native's JSOP_CALL instruction, and it will advance the pc
     * to restore by the length of the current opcode.  If the native's return
     * type is jsval, snapshot() will also indicate in the type map that the
     * element on top of the stack is a boxed value which doesn't need to be
     * boxed if the type guard generated by unbox_value() fails.
     */

    if (JSTN_ERRTYPE(pendingSpecializedNative) == FAIL_STATUS) {
        /* Keep cx->bailExit null when it's invalid. */
        w.stContextField(w.immpNull(), bailExit);

        LIns* status = w.ldiStateField(builtinStatus);
        if (pendingSpecializedNative == &generatedSpecializedNative) {
            LIns* ok_ins = v_ins;

            /*
             * If we run a generic traceable native, the return value is in the argument
             * vector for native function calls. The actual return value of the native is a JSBool
             * indicating the error status.
             */

            Address nativeRvalAddr = AllocSlotsAddress(native_rval_ins);
            if (pendingSpecializedNative->flags & JSTN_CONSTRUCTOR) {
                LIns *cond_ins;
                LIns *x;

                // v_ins    := the object payload from native_rval_ins
                // cond_ins := true if native_rval_ins contains a JSObject*
                unbox_any_object(nativeRvalAddr, &v_ins, &cond_ins);
                // x        := v_ins if native_rval_ins contains a JSObject*, NULL otherwise
                x = w.cmovp(cond_ins, v_ins, w.nameImmw(0));
                // v_ins    := newobj_ins if native_rval_ins doesn't contain a JSObject*,
                //             the object payload from native_rval_ins otherwise
                v_ins = w.cmovp(w.eqp0(x), newobj_ins, x);
            } else {
                v_ins = w.ldd(nativeRvalAddr);
            }
            set(&v, v_ins);

            propagateFailureToBuiltinStatus(ok_ins, status);
        }
        guard(true, w.eqi0(status), STATUS_EXIT);
    }

    if (pendingSpecializedNative->flags & JSTN_UNBOX_AFTER) {
        /*
         * If we side exit on the unboxing code due to a type change, make sure that the boxed
         * value is actually currently associated with that location, and that we are talking
         * about the top of the stack here, which is where we expected boxed values.
         */
        JS_ASSERT(&v == &cx->regs->sp[-1] && get(&v) == v_ins);
        set(&v, unbox_value(v, AllocSlotsAddress(native_rval_ins), snapshot(BRANCH_EXIT)));
    } else if (pendingSpecializedNative->flags &
               (JSTN_RETURN_NULLABLE_STR | JSTN_RETURN_NULLABLE_OBJ)) {
        guard(v.isNull(),
              w.name(w.eqp0(v_ins), "guard(nullness)"),
              BRANCH_EXIT);
    } else if (JSTN_ERRTYPE(pendingSpecializedNative) == FAIL_NEG) {
        /* Already added i2d in functionCall. */
        JS_ASSERT(v.isNumber());
    } else {
        /* Convert the result to double if the builtin returns int32. */
        if (v.isNumber() &&
            pendingSpecializedNative->builtin->returnType() == ARGTYPE_I) {
            set(&v, w.i2d(v_ins));
        }
    }

    // We'll null pendingSpecializedNative in monitorRecording, on the next op
    // cycle.  There must be a next op since the stack is non-empty.
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::name(Value*& vp, LIns*& ins, NameResult& nr)
{
    JSObject* obj = &cx->fp()->scopeChain();
    JSOp op = JSOp(*cx->regs->pc);
    if (js_CodeSpec[op].format & JOF_GNAME)
        obj = obj->getGlobal();
    if (obj != globalObj)
        return scopeChainProp(obj, vp, ins, nr);

    /* Can't use prop here, because we don't want unboxing from global slots. */
    LIns* obj_ins = w.immpObjGC(globalObj);
    uint32 slot;

    JSObject* obj2;
    PCVal pcval;

    /*
     * Property cache ensures that we are dealing with an existing property,
     * and guards the shape for us.
     */
    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    /* Abort if property doesn't exist (interpreter will report an error.) */
    if (pcval.isNull())
        RETURN_STOP_A("named property not found");

    /* Insist on obj being the directly addressed object. */
    if (obj2 != obj)
        RETURN_STOP_A("name() hit prototype chain");

    /* Don't trace getter or setter calls, our caller wants a direct slot. */
    if (pcval.isShape()) {
        const Shape* shape = pcval.toShape();
        if (!isValidSlot(obj, shape))
            RETURN_STOP_A("name() not accessing a valid slot");
        slot = shape->slot;
    } else {
        if (!pcval.isSlot())
            RETURN_STOP_A("PCE is not a slot");
        slot = pcval.toSlot();
    }

    if (!lazilyImportGlobalSlot(slot))
        RETURN_STOP_A("lazy import of global slot failed");

    vp = &obj->getSlotRef(slot);
    ins = get(vp);
    nr.tracked = true;
    return ARECORD_CONTINUE;
}

static JSObject* FASTCALL
MethodReadBarrier(JSContext* cx, JSObject* obj, Shape* shape, JSObject* funobj)
{
    Value v = ObjectValue(*funobj);
    AutoValueRooter tvr(cx, v);

    if (!obj->methodReadBarrier(cx, *shape, tvr.addr()))
        return NULL;
    return &tvr.value().toObject();
}
JS_DEFINE_CALLINFO_4(static, OBJECT_FAIL, MethodReadBarrier, CONTEXT, OBJECT, SHAPE, OBJECT,
                     0, ACCSET_STORE_ANY)

/*
 * Get a property. The current opcode has JOF_ATOM.
 *
 * There are two modes. The caller must pass nonnull pointers for either outp
 * or both slotp and v_insp. In the latter case, we require a plain old
 * property with a slot; if the property turns out to be anything else, abort
 * tracing (rather than emit a call to a native getter or GetAnyProperty).
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::prop(JSObject* obj, LIns* obj_ins, uint32 *slotp, LIns** v_insp, Value *outp)
{
    /*
     * Insist that obj have js_SetProperty as its set object-op. This suffices
     * to prevent a rogue obj from being used on-trace (loaded via obj_ins),
     * because we will guard on shape (or else global object identity) and any
     * object not having the same op must have a different class, and therefore
     * must differ in its shape (or not be the global object).
     */
    if (!obj->isDenseArray() && obj->getOps()->getProperty)
        RETURN_STOP_A("non-dense-array, non-native js::ObjectOps::getProperty");

    JS_ASSERT((slotp && v_insp && !outp) || (!slotp && !v_insp && outp));

    /*
     * Property cache ensures that we are dealing with an existing property,
     * and guards the shape for us.
     */
    JSObject* obj2;
    PCVal pcval;
    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    /* Check for nonexistent property reference, which results in undefined. */
    if (pcval.isNull()) {
        if (slotp)
            RETURN_STOP_A("property not found");

        /*
         * We could specialize to guard on just JSClass.getProperty, but a mere
         * class guard is simpler and slightly faster.
         */
        if (obj->getClass()->getProperty != Valueify(JS_PropertyStub)) {
            RETURN_STOP_A("can't trace through access to undefined property if "
                          "JSClass.getProperty hook isn't stubbed");
        }
        guardClass(obj_ins, obj->getClass(), snapshot(MISMATCH_EXIT), LOAD_NORMAL);

        /*
         * This trace will be valid as long as neither the object nor any object
         * on its prototype chain changes shape.
         *
         * FIXME: This loop can become a single shape guard once bug 497789 has
         * been fixed.
         */
        VMSideExit* exit = snapshot(BRANCH_EXIT);
        do {
            if (obj->isNative()) {
                CHECK_STATUS_A(guardShape(obj_ins, obj, obj->shape(), "guard(shape)", exit));
            } else if (obj->isDenseArray()) {
                guardDenseArray(obj_ins, exit);
            } else {
                RETURN_STOP_A("non-native object involved in undefined property access");
            }
        } while (guardHasPrototype(obj, obj_ins, &obj, &obj_ins, exit));

        set(outp, w.immiUndefined());
        return ARECORD_CONTINUE;
    }

    return InjectStatus(propTail(obj, obj_ins, obj2, pcval, slotp, v_insp, outp));
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::propTail(JSObject* obj, LIns* obj_ins, JSObject* obj2, PCVal pcval,
                        uint32 *slotp, LIns** v_insp, Value *outp)
{
    const JSCodeSpec& cs = js_CodeSpec[*cx->regs->pc];
    uint32 setflags = (cs.format & (JOF_INCDEC | JOF_FOR));
    JS_ASSERT(!(cs.format & JOF_SET));

    const Shape* shape;
    uint32 slot;
    bool isMethod;

    if (pcval.isShape()) {
        shape = pcval.toShape();
        JS_ASSERT(obj2->nativeContains(*shape));

        if (setflags && !shape->hasDefaultSetter())
            RETURN_STOP("non-stub setter");
        if (setflags && !shape->writable())
            RETURN_STOP("writing to a readonly property");
        if (!shape->hasDefaultGetterOrIsMethod()) {
            if (slotp)
                RETURN_STOP("can't trace non-stub getter for this opcode");
            if (shape->hasGetterValue())
                return getPropertyWithScriptGetter(obj, obj_ins, shape);
            if (shape->slot == SHAPE_INVALID_SLOT)
                return getPropertyWithNativeGetter(obj_ins, shape, outp);
            return getPropertyById(obj_ins, outp);
        }
        if (!obj2->containsSlot(shape->slot))
            RETURN_STOP("no valid slot");
        slot = shape->slot;
        isMethod = shape->isMethod();
        JS_ASSERT_IF(isMethod, obj2->hasMethodBarrier());
    } else {
        if (!pcval.isSlot())
            RETURN_STOP("PCE is not a slot");
        slot = pcval.toSlot();
        shape = NULL;
        isMethod = false;
    }

    /* We have a slot. Check whether it is direct or in a prototype. */
    if (obj2 != obj) {
        if (setflags)
            RETURN_STOP("JOF_INCDEC|JOF_FOR opcode hit prototype chain");

        /*
         * We're getting a prototype property. Two cases:
         *
         * 1. If obj2 is obj's immediate prototype we must walk up from obj,
         * since direct and immediate-prototype cache hits key on obj's shape,
         * not its identity.
         *
         * 2. Otherwise obj2 is higher up the prototype chain and we've keyed
         * on obj's identity, and since setting __proto__ reshapes all objects
         * along the old prototype chain, then provided we shape-guard obj2,
         * we can "teleport" directly to obj2 by embedding it as a constant
         * (this constant object instruction will be CSE'ed with the constant
         * emitted by test_property_cache, whose shape is guarded).
         */
        obj_ins = (obj2 == obj->getProto()) ? w.ldpObjProto(obj_ins) : w.immpObjGC(obj2);
        obj = obj2;
    }

    LIns* v_ins;
    if (obj2 == globalObj) {
        if (isMethod)
            RETURN_STOP("get global method");
        if (!lazilyImportGlobalSlot(slot))
            RETURN_STOP("lazy import of global slot failed");
        v_ins = get(&globalObj->getSlotRef(slot));
    } else {
        v_ins = unbox_slot(obj, obj_ins, slot, snapshot(BRANCH_EXIT));
    }

    /*
     * Joined function object stored as a method must be cloned when extracted
     * as a property value other than a callee. Note that shapes cover method
     * value as well as other property attributes and order, so this condition
     * is trace-invariant.
     *
     * We do not impose the method read barrier if in an imacro, assuming any
     * property gets it does (e.g., for 'toString' from JSOP_NEW) will not be
     * leaked to the calling script.
     */
    if (isMethod && !cx->fp()->hasImacropc()) {
        enterDeepBailCall();
        LIns* args[] = { v_ins, w.immpShapeGC(shape), obj_ins, cx_ins };
        v_ins = w.call(&MethodReadBarrier_ci, args);
        leaveDeepBailCall();
    }

    if (slotp) {
        *slotp = slot;
        *v_insp = v_ins;
    }
    if (outp)
        set(outp, v_ins);
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::denseArrayElement(Value& oval, Value& ival, Value*& vp, LIns*& v_ins,
                                 LIns*& addr_ins, VMSideExit* branchExit)
{
    JS_ASSERT(oval.isObject() && ival.isInt32());

    JSObject* obj = &oval.toObject();
    LIns* obj_ins = get(&oval);
    jsint idx = ival.toInt32();
    LIns* idx_ins;
    CHECK_STATUS(makeNumberInt32(get(&ival), &idx_ins));

    /*
     * Arrays have both a length and a capacity, but we only need to check
     * |index < capacity|;  in the case where |length < index < capacity|
     * the entries [length..capacity-1] will have already been marked as
     * holes by resizeDenseArrayElements() so we can read them and get
     * the correct value.
     */
    LIns* capacity_ins = w.ldiDenseArrayCapacity(obj_ins);
    jsuint capacity = obj->getDenseArrayCapacity();
    bool within = (jsuint(idx) < capacity);
    if (!within) {
        /* If not idx < capacity, stay on trace (and read value as undefined). */
        guard(true, w.geui(idx_ins, capacity_ins), branchExit);

        CHECK_STATUS(guardPrototypeHasNoIndexedProperties(obj, obj_ins, snapshot(MISMATCH_EXIT)));

        // Return undefined and indicate that we didn't actually read this (addr_ins).
        v_ins = w.immiUndefined();
        addr_ins = NULL;
        return RECORD_CONTINUE;
    }

    /* Guard that index is within capacity. */
    guard(true, w.ltui(idx_ins, capacity_ins), branchExit);

    /* Load the value and guard on its type to unbox it. */
    vp = &obj->slots[jsuint(idx)];
	JS_ASSERT(sizeof(Value) == 8); // The |3| in the following statement requires this.
    addr_ins = w.addp(w.name(w.ldpObjSlots(obj_ins), "slots"), w.lshpN(w.ui2p(idx_ins), 3));
    v_ins = unbox_value(*vp, DSlotsAddress(addr_ins), branchExit);

    /* Don't let the hole value escape. Turn it into an undefined. */
    if (vp->isMagic()) {
        CHECK_STATUS(guardPrototypeHasNoIndexedProperties(obj, obj_ins, snapshot(MISMATCH_EXIT)));
        v_ins = w.immiUndefined();
    }
    return RECORD_CONTINUE;
}

/* See comments in TypedArrayTemplate<double>::copyIndexToValue. */
LIns *
TraceRecorder::canonicalizeNaNs(LIns *dval_ins)
{
    /* NaNs are the only floating point values that do not == themselves. */
    LIns *isnonnan_ins = w.eqd(dval_ins, dval_ins);
    return w.cmovd(isnonnan_ins, dval_ins, w.immd(js_NaN));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::typedArrayElement(Value& oval, Value& ival, Value*& vp, LIns*& v_ins)
{
    JS_ASSERT(oval.isObject() && ival.isInt32());

    JSObject* obj = &oval.toObject();
    LIns* obj_ins = get(&oval);
    jsint idx = ival.toInt32();
    LIns* idx_ins;
    CHECK_STATUS_A(makeNumberInt32(get(&ival), &idx_ins));
    LIns* pidx_ins = w.ui2p(idx_ins);

    js::TypedArray* tarray = js::TypedArray::fromJSObject(obj);
    JS_ASSERT(tarray);

    /* priv_ins will load the TypedArray* */
    LIns* priv_ins = w.ldpObjPrivate(obj_ins);

    /* for out-of-range, do the same thing that the interpreter does, which is return undefined */
    if ((jsuint) idx >= tarray->length) {
        guard(false,
              w.ltui(idx_ins, w.ldiConstTypedArrayLength(priv_ins)),
              BRANCH_EXIT);
        v_ins = w.immiUndefined();
        return ARECORD_CONTINUE;
    }

    /*
     * Ensure idx < length
     *
     * NOTE! mLength is uint32, but it's guaranteed to fit in a Value
     * int, so we can treat it as either signed or unsigned.
     * If the index happens to be negative, when it's treated as
     * unsigned it'll be a very large int, and thus won't be less than
     * length.
     */
    guard(true,
          w.ltui(idx_ins, w.ldiConstTypedArrayLength(priv_ins)),
          BRANCH_EXIT);

    /* We are now ready to load.  Do a different type of load
     * depending on what type of thing we're loading. */
    LIns* data_ins = w.ldpConstTypedArrayData(priv_ins);

    switch (tarray->type) {
      case js::TypedArray::TYPE_INT8:
        v_ins = w.i2d(w.ldc2iTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_UINT8:
      case js::TypedArray::TYPE_UINT8_CLAMPED:
        v_ins = w.ui2d(w.lduc2uiTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_INT16:
        v_ins = w.i2d(w.lds2iTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_UINT16:
        v_ins = w.ui2d(w.ldus2uiTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_INT32:
        v_ins = w.i2d(w.ldiTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_UINT32:
        v_ins = w.ui2d(w.ldiTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_FLOAT32:
        v_ins = canonicalizeNaNs(w.ldf2dTypedArrayElement(data_ins, pidx_ins));
        break;
      case js::TypedArray::TYPE_FLOAT64:
        v_ins = canonicalizeNaNs(w.lddTypedArrayElement(data_ins, pidx_ins));
        break;
      default:
        JS_NOT_REACHED("Unknown typed array type in tracer");
    }

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::getProp(JSObject* obj, LIns* obj_ins)
{
    JSOp op = JSOp(*cx->regs->pc);
    const JSCodeSpec& cs = js_CodeSpec[op];

    JS_ASSERT(cs.ndefs == 1);
    return prop(obj, obj_ins, NULL, NULL, &stackval(-cs.nuses));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::getProp(Value& v)
{
    if (v.isPrimitive())
        RETURN_STOP_A("primitive lhs");

    return getProp(&v.toObject(), get(&v));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NAME()
{
    Value* vp;
    LIns* v_ins;
    NameResult nr;
    CHECK_STATUS_A(name(vp, v_ins, nr));
    stack(0, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DOUBLE()
{
    double d = consts[GET_INDEX(cx->regs->pc)].toDouble();
    stack(0, w.immd(d));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STRING()
{
    JSAtom* atom = atoms[GET_INDEX(cx->regs->pc)];
    stack(0, w.immpAtomGC(atom));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ZERO()
{
    stack(0, w.immd(0));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ONE()
{
    stack(0, w.immd(1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NULL()
{
    stack(0, w.immpNull());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_THIS()
{
    LIns* this_ins;
    CHECK_STATUS_A(getThis(this_ins));
    stack(0, this_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FALSE()
{
    stack(0, w.immi(0));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TRUE()
{
    stack(0, w.immi(1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_OR()
{
    return ifop();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_AND()
{
    return ifop();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TABLESWITCH()
{
#ifdef NANOJIT_IA32
    /* Handle tableswitches specially -- prepare a jump table if needed. */
    return tableswitch();
#else
    return InjectStatus(switchop());
#endif
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LOOKUPSWITCH()
{
    return InjectStatus(switchop());
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STRICTEQ()
{
    strictEquality(true, false);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STRICTNE()
{
    strictEquality(false, false);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_OBJECT()
{
    JSStackFrame* const fp = cx->fp();
    JSScript* script = fp->script();
    unsigned index = atoms - script->atomMap.vector + GET_INDEX(cx->regs->pc);

    JSObject* obj;
    obj = script->getObject(index);
    stack(0, w.immpObjGC(obj));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POP()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TRAP()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETARG()
{
    stack(0, arg(GET_ARGNO(cx->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETARG()
{
    arg(GET_ARGNO(cx->regs->pc), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETLOCAL()
{
    stack(0, var(GET_SLOTNO(cx->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETLOCAL()
{
    var(GET_SLOTNO(cx->regs->pc), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_UINT16()
{
    stack(0, w.immd(GET_UINT16(cx->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEWINIT()
{
    JSProtoKey key = JSProtoKey(GET_UINT16(cx->regs->pc));
    uint32 count = GET_UINT16(cx->regs->pc + UINT16_LEN);

    LIns* proto_ins;
    CHECK_STATUS_A(getClassPrototype(key, proto_ins));

    LIns *v_ins;
    if (key == JSProto_Array) {
        LIns *args[] = { w.immi(count), cx_ins };
        v_ins = w.call(&js_InitializerArray_ci, args);
    } else {
        LIns *args[] = { w.immi(count), cx_ins };
        v_ins = w.call(&js_InitializerObject_ci, args);
    }
    guard(false, w.eqp0(v_ins), OOM_EXIT);
    stack(0, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENDINIT()
{
#ifdef DEBUG
    Value& v = stackval(-1);
    JS_ASSERT(!v.isPrimitive());
#endif
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INITPROP()
{
    // All the action is in record_SetPropHit.
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INITELEM()
{
    return setElem(-3, -2, -1);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFSHARP()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_USESHARP()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCARG()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->regs->pc)), 1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCLOCAL()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->regs->pc)), 1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECARG()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->regs->pc)), -1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECLOCAL()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->regs->pc)), -1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGINC()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->regs->pc)), 1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LOCALINC()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->regs->pc)), 1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGDEC()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->regs->pc)), -1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LOCALDEC()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->regs->pc)), -1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IMACOP()
{
    JS_ASSERT(cx->fp()->hasImacropc());
    return ARECORD_CONTINUE;
}

static JSBool FASTCALL
ObjectToIterator(JSContext* cx, JSObject *obj, int32 flags, JSObject **objp)
{
    AutoValueRooter tvr(cx, ObjectValue(*obj));
    bool ok = js_ValueToIterator(cx, flags, tvr.addr());
    if (!ok) {
        SetBuiltinError(cx);
        return false;
    }
    *objp = &tvr.value().toObject();
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, ObjectToIterator, CONTEXT, OBJECT, INT32, OBJECTPTR,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ITER()
{
    Value& v = stackval(-1);
    if (v.isPrimitive())
        RETURN_STOP_A("for-in on a primitive value");

    RETURN_IF_XML_A(v);

    LIns *obj_ins = get(&v);
    jsuint flags = cx->regs->pc[1];

    enterDeepBailCall();

    LIns* objp_ins = w.allocp(sizeof(JSObject*));
    LIns* args[] = { objp_ins, w.immi(flags), obj_ins, cx_ins };
    LIns* ok_ins = w.call(&ObjectToIterator_ci, args);

    // We need to guard on ok_ins, but this requires a snapshot of the state
    // after this op. monitorRecording will do it for us.
    pendingGuardCondition = ok_ins;

    leaveDeepBailCall();

    stack(-1, w.name(w.ldpAlloc(objp_ins), "iterobj"));

    return ARECORD_CONTINUE;
}

static JSBool FASTCALL
IteratorMore(JSContext *cx, JSObject *iterobj, Value *vp)
{
    if (!js_IteratorMore(cx, iterobj, vp)) {
        SetBuiltinError(cx, BUILTIN_ERROR_NO_FIXUP_NEEDED);
        return false;
    } else if (cx->tracerState->builtinStatus) {
        SetBuiltinError(cx, BUILTIN_NO_FIXUP_NEEDED);
        return false;
    }
    return true;
}
JS_DEFINE_CALLINFO_3(extern, BOOL_FAIL, IteratorMore, CONTEXT, OBJECT, VALUEPTR,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_MOREITER()
{
    Value& iterobj_val = stackval(-1);
    if (iterobj_val.isPrimitive())
        RETURN_STOP_A("for-in on a primitive value");

    RETURN_IF_XML_A(iterobj_val);

    JSObject* iterobj = &iterobj_val.toObject();
    LIns* iterobj_ins = get(&iterobj_val);
    bool cond;
    LIns* cond_ins;

    /* JSOP_FOR* already guards on this, but in certain rare cases we might record misformed loop traces. */
    if (iterobj->hasClass(&js_IteratorClass)) {
        guardClass(iterobj_ins, &js_IteratorClass, snapshot(BRANCH_EXIT), LOAD_NORMAL);
        NativeIterator *ni = (NativeIterator *) iterobj->getPrivate();
        void *cursor = ni->props_cursor;
        void *end = ni->props_end;

        LIns *ni_ins = w.ldpObjPrivate(iterobj_ins);
        LIns *cursor_ins = w.ldpIterCursor(ni_ins);
        LIns *end_ins = w.ldpIterEnd(ni_ins);

        /* Figure out whether the native iterator contains more values. */
        cond = cursor < end;
        cond_ins = w.ltp(cursor_ins, end_ins);
    } else {
        guardNotClass(iterobj_ins, &js_IteratorClass, snapshot(BRANCH_EXIT), LOAD_NORMAL);

        enterDeepBailCall();

        LIns* vp_ins = w.allocp(sizeof(Value));
        LIns* args[] = { vp_ins, iterobj_ins, cx_ins };
        LIns* ok_ins = w.call(&IteratorMore_ci, args);

        /*
         * We cannot use pendingGuardCondition since monitorRecording may not be
         * triggered if we close the loop below with endLoop. Instead, we guard
         * here with STATUS_EXIT. By default, STATUS_EXIT means "advance the pc
         * and fixup the stack", so IteratorMore sets BUILTIN_NO_FIXUP_NEEDED.
         * If IteratorMore fails, we will reexecute this op in the interpreter,
         * but js_IteratoreMore is idempotent so this is ok.
         */
        guard(false, w.eqi0(ok_ins), STATUS_EXIT);

        leaveDeepBailCall();

        /*
         * The interpreter will call js_IteratorMore again, but that's legal. We have to
         * carefully protect ourselves against reentrancy.
         */
        JSContext *localCx = cx;
        AutoValueRooter rooter(cx);
        if (!js_IteratorMore(cx, iterobj, rooter.addr()))
            RETURN_ERROR_A("error in js_IteratorMore");
        if (!TRACE_RECORDER(localCx))
            return ARECORD_ABORTED;

        cond = (rooter.value().isTrue());
        cond_ins = is_boxed_true(AllocSlotsAddress(vp_ins));
    }

    jsbytecode* pc = cx->regs->pc;

    if (pc[1] == JSOP_IFNE) {
        fuseIf(pc + 1, cond, cond_ins);
        return checkTraceEnd(pc + 1);
    }

    stack(0, cond_ins);

    return ARECORD_CONTINUE;
}

static JSBool FASTCALL
CloseIterator(JSContext *cx, JSObject *iterobj)
{
    if (!js_CloseIterator(cx, iterobj)) {
        SetBuiltinError(cx);
        return false;
    }
    return cx->tracerState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_2(extern, BOOL_FAIL, CloseIterator, CONTEXT, OBJECT, 0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENDITER()
{
    JS_ASSERT(!stackval(-1).isPrimitive());

    enterDeepBailCall();

    LIns* args[] = { stack(-1), cx_ins };
    LIns* ok_ins = w.call(&CloseIterator_ci, args);

    // We need to guard on ok_ins, but this requires a snapshot of the state
    // after this op. monitorRecording will do it for us.
    pendingGuardCondition = ok_ins;

    leaveDeepBailCall();

    return ARECORD_CONTINUE;
}

#if JS_BITS_PER_WORD == 32
JS_REQUIRES_STACK void
TraceRecorder::storeMagic(JSWhyMagic why, Address addr)
{
    w.stiValuePayload(w.immpMagicWhy(why), addr);
    w.stiValueTag(w.immpMagicWhy(JSVAL_TAG_MAGIC), addr);
}
#elif JS_BITS_PER_WORD == 64
JS_REQUIRES_STACK void
TraceRecorder::storeMagic(JSWhyMagic why, Address addr)
{
    LIns *magic = w.nameImmq(BUILD_JSVAL(JSVAL_TAG_MAGIC, why));
    w.stq(magic, addr);
}
#endif

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::unboxNextValue(LIns* &v_ins)
{
    Value &iterobj_val = stackval(-1);
    JSObject *iterobj = &iterobj_val.toObject();
    LIns* iterobj_ins = get(&iterobj_val);

    if (iterobj->hasClass(&js_IteratorClass)) {
        guardClass(iterobj_ins, &js_IteratorClass, snapshot(BRANCH_EXIT), LOAD_NORMAL);
        NativeIterator *ni = (NativeIterator *) iterobj->getPrivate();

        LIns *ni_ins = w.ldpObjPrivate(iterobj_ins);
        LIns *cursor_ins = w.ldpIterCursor(ni_ins);

        /* Emit code to stringify the id if necessary. */
        Address cursorAddr = IterPropsAddress(cursor_ins);
        if (!(((NativeIterator *) iterobj->getPrivate())->flags & JSITER_FOREACH)) {
            /* Read the next id from the iterator. */
            jsid id = *ni->currentKey();
            LIns *id_ins = w.name(w.ldp(cursorAddr), "id");

            /*
             * Most iterations over object properties never have to actually deal with
             * any numeric properties, so we guard here instead of branching.
             */
            guard(JSID_IS_STRING(id), is_string_id(id_ins), BRANCH_EXIT);

            if (JSID_IS_STRING(id)) {
                v_ins = unbox_string_id(id_ins);
            } else {
                /* id is an integer, convert to a string. */
                JS_ASSERT(JSID_IS_INT(id));
                LIns *id_to_int_ins = unbox_int_id(id_ins);
                LIns* args[] = { id_to_int_ins, cx_ins };
                v_ins = w.call(&js_IntToString_ci, args);
                guard(false, w.eqp0(v_ins), OOM_EXIT);
            }

            /* Increment the cursor by one jsid and store it back. */
            cursor_ins = w.addp(cursor_ins, w.nameImmw(sizeof(jsid)));
        } else {
            /* Read the next value from the iterator. */
            Value v = *ni->currentValue();
            v_ins = unbox_value(v, cursorAddr, snapshot(BRANCH_EXIT));

            /* Increment the cursor by one Value and store it back. */
            cursor_ins = w.addp(cursor_ins, w.nameImmw(sizeof(Value)));
        }

        w.stpIterCursor(cursor_ins, ni_ins);
    } else {
        guardNotClass(iterobj_ins, &js_IteratorClass, snapshot(BRANCH_EXIT), LOAD_NORMAL);

        Address iterValueAddr = CxAddress(iterValue);
        v_ins = unbox_value(cx->iterValue, iterValueAddr, snapshot(BRANCH_EXIT));
        storeMagic(JS_NO_ITER_VALUE, iterValueAddr);
    }

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORNAME()
{
    Value* vp;
    LIns* x_ins;
    NameResult nr;
    CHECK_STATUS_A(name(vp, x_ins, nr));
    if (!nr.tracked)
        RETURN_STOP_A("forname on non-tracked value not supported");
    LIns* v_ins;
    CHECK_STATUS_A(unboxNextValue(v_ins));
    set(vp, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORPROP()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORELEM()
{
    LIns* v_ins;
    CHECK_STATUS_A(unboxNextValue(v_ins));
    stack(0, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORARG()
{
    LIns* v_ins;
    CHECK_STATUS_A(unboxNextValue(v_ins));
    arg(GET_ARGNO(cx->regs->pc), v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORLOCAL()
{
    LIns* v_ins;
    CHECK_STATUS_A(unboxNextValue(v_ins));
    var(GET_SLOTNO(cx->regs->pc), v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORGLOBAL()
{
    LIns* v_ins;
    CHECK_STATUS_A(unboxNextValue(v_ins));

    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    set(&globalObj->getSlotRef(slot), v_ins);
    return ARECORD_CONTINUE;
}


JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POPN()
{
    return ARECORD_CONTINUE;
}

/*
 * Generate LIR to reach |obj2| from |obj| by traversing the scope chain. The
 * generated code also ensures that any call objects found have not changed shape.
 *
 *      obj               starting object
 *      obj_ins           LIR instruction representing obj
 *      targetObj         end object for traversal
 *      targetIns [out]   LIR instruction representing obj2
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::traverseScopeChain(JSObject *obj, LIns *obj_ins, JSObject *targetObj,
                                  LIns *&targetIns)
{
    VMSideExit* exit = NULL;

    /*
     * Scope chains are often left "incomplete", and reified lazily when
     * necessary, since doing so is expensive. When creating null and flat
     * closures on trace (the only kinds supported), the global object is
     * hardcoded as the parent, since reifying the scope chain on trace
     * would be extremely difficult. This is because block objects need frame
     * pointers, which do not exist on trace, and thus would require magic
     * similar to arguments objects or reification of stack frames. Luckily,
     * for null and flat closures, these blocks are unnecessary.
     *
     * The problem, as exposed by bug 523793, is that this means creating a
     * fixed traversal on trace can be inconsistent with the shorter scope
     * chain used when executing a trace. To address this, perform an initial
     * sweep of the scope chain to make sure that if there is a heavyweight
     * function with a call object, and there is also a block object, the
     * trace is safely aborted.
     *
     * If there is no call object, we must have arrived at the global object,
     * and can bypass the scope chain traversal completely.
     */
    bool foundCallObj = false;
    bool foundBlockObj = false;
    JSObject* searchObj = obj;

    for (;;) {
        if (searchObj != globalObj) {
            Class* clasp = searchObj->getClass();
            if (clasp == &js_BlockClass) {
                foundBlockObj = true;
            } else if (clasp == &js_CallClass &&
                       searchObj->getCallObjCalleeFunction()->isHeavyweight()) {
                foundCallObj = true;
            }
        }

        if (searchObj == targetObj)
            break;

        searchObj = searchObj->getParent();
        if (!searchObj)
            RETURN_STOP("cannot traverse this scope chain on trace");
    }

    if (!foundCallObj) {
        JS_ASSERT(targetObj == globalObj);
        targetIns = w.nameImmpNonGC(globalObj);
        return RECORD_CONTINUE;
    }

    if (foundBlockObj)
        RETURN_STOP("cannot traverse this scope chain on trace");

    /* There was a call object, or should be a call object now. */
    for (;;) {
        if (obj != globalObj) {
            if (!js_IsCacheableNonGlobalScope(obj))
                RETURN_STOP("scope chain lookup crosses non-cacheable object");

            // We must guard on the shape of all call objects for heavyweight functions
            // that we traverse on the scope chain: if the shape changes, a variable with
            // the same name may have been inserted in the scope chain.
            if (obj->isCall() &&
                obj->getCallObjCalleeFunction()->isHeavyweight()) {
                if (!exit)
                    exit = snapshot(BRANCH_EXIT);
                guard(true,
                      w.name(w.eqiN(w.ldiObjShape(obj_ins), obj->shape()), "guard_shape"),
                      exit);
            }
        }

        JS_ASSERT(!obj->isBlock());

        if (obj == targetObj)
            break;

        obj = obj->getParent();
        obj_ins = w.ldpObjParent(obj_ins);
    }

    targetIns = obj_ins;
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BINDNAME()
{
    JSStackFrame* const fp = cx->fp();
    JSObject *obj;

    if (!fp->isFunctionFrame()) {
        obj = &fp->scopeChain();

#ifdef DEBUG
        JSStackFrame *fp2 = fp;
#endif

        // In global code, fp->scopeChain can only contain blocks whose values
        // are still on the stack.  We never use BINDNAME to refer to these.
        while (obj->isBlock()) {
            // The block's values are still on the stack.
#ifdef DEBUG
            // NB: fp2 can't be a generator frame, because !fp->hasFunction.
            while (obj->getPrivate() != fp2) {
                JS_ASSERT(fp2->isEvalOrDebuggerFrame());
                fp2 = fp2->prev();
                if (!fp2)
                    JS_NOT_REACHED("bad stack frame");
            }
#endif
            obj = obj->getParent();
            // Blocks always have parents.
            JS_ASSERT(obj);
        }

        // If anything other than Block, Call, DeclEnv, and the global object
        // is on the scope chain, we shouldn't be recording. Of those, only
        // Block and global can be present in global code.
        JS_ASSERT(obj == globalObj);

        /*
         * The trace is specialized to this global object. Furthermore, we know it
         * is the sole 'global' object on the scope chain: we set globalObj to the
         * scope chain element with no parent, and we reached it starting from the
         * function closure or the current scopeChain, so there is nothing inner to
         * it. Therefore this must be the right base object.
         */
        stack(0, w.immpObjGC(obj));
        return ARECORD_CONTINUE;
    }

    // We can't trace BINDNAME in functions that contain direct calls to eval,
    // as they might add bindings which previously-traced references would have
    // to see.
    if (JSFUN_HEAVYWEIGHT_TEST(fp->fun()->flags))
        RETURN_STOP_A("BINDNAME in heavyweight function.");

    // We don't have the scope chain on trace, so instead we get a start object
    // that is on the scope chain and doesn't skip the target object (the one
    // that contains the property).
    Value *callee = &cx->fp()->calleeValue();
    obj = callee->toObject().getParent();
    if (obj == globalObj) {
        stack(0, w.immpObjGC(obj));
        return ARECORD_CONTINUE;
    }
    LIns *obj_ins = w.ldpObjParent(get(callee));

    // Find the target object.
    JSAtom *atom = atoms[GET_INDEX(cx->regs->pc)];
    jsid id = ATOM_TO_JSID(atom);
    JSContext *localCx = cx;
    JSObject *obj2 = js_FindIdentifierBase(cx, &fp->scopeChain(), id);
    if (!obj2)
        RETURN_ERROR_A("error in js_FindIdentifierBase");
    if (!TRACE_RECORDER(localCx))
        return ARECORD_ABORTED;
    if (obj2 != globalObj && !obj2->isCall())
        RETURN_STOP_A("BINDNAME on non-global, non-call object");

    // Generate LIR to get to the target object from the start object.
    LIns *obj2_ins;
    CHECK_STATUS_A(traverseScopeChain(obj, obj_ins, obj2, obj2_ins));

    // If |obj2| is the global object, we can refer to it directly instead of walking up
    // the scope chain. There may still be guards on intervening call objects.
    stack(0, obj2 == globalObj ? w.immpObjGC(obj2) : obj2_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETNAME()
{
    // record_SetPropHit does all the work.
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_THROW()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IN()
{
    Value& rval = stackval(-1);
    Value& lval = stackval(-2);

    if (rval.isPrimitive())
        RETURN_STOP_A("JSOP_IN on non-object right operand");
    JSObject* obj = &rval.toObject();
    LIns* obj_ins = get(&rval);

    jsid id;
    LIns* x;
    if (lval.isInt32()) {
        if (!js_Int32ToId(cx, lval.toInt32(), &id))
            RETURN_ERROR_A("OOM converting left operand of JSOP_IN to string");
        LIns* num_ins;
        CHECK_STATUS_A(makeNumberInt32(get(&lval), &num_ins));
        LIns* args[] = { num_ins, obj_ins, cx_ins };
        x = w.call(&js_HasNamedPropertyInt32_ci, args);
    } else if (lval.isString()) {
        if (!js_ValueToStringId(cx, lval, &id))
            RETURN_ERROR_A("left operand of JSOP_IN didn't convert to a string-id");
        LIns* args[] = { get(&lval), obj_ins, cx_ins };
        x = w.call(&js_HasNamedProperty_ci, args);
    } else {
        RETURN_STOP_A("string or integer expected");
    }

    guard(false, w.eqiN(x, JS_NEITHER), OOM_EXIT);
    x = w.eqiN(x, 1);

    TraceMonitor &localtm = *traceMonitor;

    JSObject* obj2;
    JSProperty* prop;
    JSBool ok = obj->lookupProperty(cx, id, &obj2, &prop);

    if (!ok)
        RETURN_ERROR_A("obj->lookupProperty failed in JSOP_IN");

    /* lookupProperty can reenter the interpreter and kill |this|. */
    if (!localtm.recorder)
        return ARECORD_ABORTED;

    bool cond = prop != NULL;

    /*
     * The interpreter fuses comparisons and the following branch, so we have
     * to do that here as well.
     */
    fuseIf(cx->regs->pc + 1, cond, x);

    /*
     * We update the stack after the guard. This is safe since the guard bails
     * out at the comparison and the interpreter will therefore re-execute the
     * comparison. This way the value of the condition doesn't have to be
     * calculated and saved on the stack in most cases.
     */
    set(&lval, x);
    return ARECORD_CONTINUE;
}

static JSBool FASTCALL
HasInstanceOnTrace(JSContext* cx, JSObject* ctor, ValueArgType arg)
{
    const Value &argref = ValueArgToConstRef(arg);
    JSBool result = JS_FALSE;
    if (!HasInstance(cx, ctor, &argref, &result))
        SetBuiltinError(cx);
    return result;
}
JS_DEFINE_CALLINFO_3(static, BOOL_FAIL, HasInstanceOnTrace, CONTEXT, OBJECT, VALUE,
                     0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INSTANCEOF()
{
    // If the rhs isn't an object, we are headed for a TypeError.
    Value& ctor = stackval(-1);
    if (ctor.isPrimitive())
        RETURN_STOP_A("non-object on rhs of instanceof");

    Value& val = stackval(-2);
    LIns* val_ins = box_value_for_native_call(val, get(&val));

    enterDeepBailCall();
    LIns* args[] = {val_ins, get(&ctor), cx_ins};
    stack(-2, w.call(&HasInstanceOnTrace_ci, args));
    LIns* status_ins = w.ldiStateField(builtinStatus);
    pendingGuardCondition = w.eqi0(status_ins);
    leaveDeepBailCall();

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEBUGGER()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GOSUB()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RETSUB()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_EXCEPTION()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LINENO()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BLOCKCHAIN()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NULLBLOCKCHAIN()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CONDSWITCH()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CASE()
{
    strictEquality(true, true);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFAULT()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_EVAL()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENUMELEM()
{
    /*
     * To quote from jsinterp.cpp's JSOP_ENUMELEM case:
     * Funky: the value to set is under the [obj, id] pair.
     */
    return setElem(-2, -1, -3);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETTER()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETTER()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFFUN()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFFUN_FC()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFCONST()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFVAR()
{
    return ARECORD_STOP;
}

jsatomid
TraceRecorder::getFullIndex(ptrdiff_t pcoff)
{
    jsatomid index = GET_INDEX(cx->regs->pc + pcoff);
    index += atoms - cx->fp()->script()->atomMap.vector;
    return index;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LAMBDA()
{
    JSFunction* fun;
    fun = cx->fp()->script()->getFunction(getFullIndex());

    /*
     * Emit code to clone a null closure parented by this recorder's global
     * object, in order to preserve function object evaluation rules observable
     * via identity and mutation. But don't clone if our result is consumed by
     * JSOP_SETMETHOD or JSOP_INITMETHOD, since we optimize away the clone for
     * these combinations and clone only if the "method value" escapes.
     *
     * See jsinterp.cpp, the JSOP_LAMBDA null closure case. The JSOP_SETMETHOD and
     * JSOP_INITMETHOD logic governing the early ARECORD_CONTINUE returns below
     * must agree with the corresponding break-from-do-while(0) logic there.
     */
    if (FUN_NULL_CLOSURE(fun)) {
        if (FUN_OBJECT(fun)->getParent() != globalObj)
            RETURN_STOP_A("Null closure function object parent must be global object");

        jsbytecode *pc2 = js_AdvanceOverBlockchain(cx->regs->pc + JSOP_LAMBDA_LENGTH);
        JSOp op2 = JSOp(*pc2);

        if (op2 == JSOP_INITMETHOD) {
            stack(0, w.immpObjGC(FUN_OBJECT(fun)));
            return ARECORD_CONTINUE;
        }

        if (op2 == JSOP_SETMETHOD) {
            Value lval = stackval(-1);

            if (!lval.isPrimitive() && lval.toObject().canHaveMethodBarrier()) {
                stack(0, w.immpObjGC(FUN_OBJECT(fun)));
                return ARECORD_CONTINUE;
            }
        } else if (fun->joinable()) {
            if (op2 == JSOP_CALL) {
                /*
                 * Array.prototype.sort and String.prototype.replace are
                 * optimized as if they are special form. We know that they
                 * won't leak the joined function object in obj, therefore
                 * we don't need to clone that compiler- created function
                 * object for identity/mutation reasons.
                 */
                int iargc = GET_ARGC(pc2);

                /*
                 * Note that we have not yet pushed obj as the final argument,
                 * so regs.sp[1 - (iargc + 2)], and not regs.sp[-(iargc + 2)],
                 * is the callee for this JSOP_CALL.
                 */
                const Value &cref = cx->regs->sp[1 - (iargc + 2)];
                JSObject *callee;

                if (IsFunctionObject(cref, &callee)) {
                    JSFunction *calleeFun = callee->getFunctionPrivate();
                    Native native = calleeFun->maybeNative();

                    if ((iargc == 1 && native == array_sort) ||
                        (iargc == 2 && native == str_replace)) {
                        stack(0, w.immpObjGC(FUN_OBJECT(fun)));
                        return ARECORD_CONTINUE;
                    }
                }
            } else if (op2 == JSOP_NULL) {
                pc2 += JSOP_NULL_LENGTH;
                op2 = JSOp(*pc2);

                if (op2 == JSOP_CALL && GET_ARGC(pc2) == 0) {
                    stack(0, w.immpObjGC(FUN_OBJECT(fun)));
                    return ARECORD_CONTINUE;
                }
            }
        }

        LIns *proto_ins;
        CHECK_STATUS_A(getClassPrototype(JSProto_Function, proto_ins));

        LIns* args[] = { w.immpObjGC(globalObj), proto_ins, w.immpFunGC(fun), cx_ins };
        LIns* x = w.call(&js_NewNullClosure_ci, args);
        stack(0, x);
        return ARECORD_CONTINUE;
    }

    if (GetBlockChainFast(cx, cx->fp(), JSOP_LAMBDA, JSOP_LAMBDA_LENGTH))
        RETURN_STOP_A("Unable to trace creating lambda in let");

    LIns *proto_ins;
    CHECK_STATUS_A(getClassPrototype(JSProto_Function, proto_ins));
    LIns* scopeChain_ins = scopeChain();
    JS_ASSERT(scopeChain_ins);
    LIns* args[] = { proto_ins, scopeChain_ins, w.nameImmpNonGC(fun), cx_ins };
    LIns* call_ins = w.call(&js_CloneFunctionObject_ci, args);
    guard(false,
          w.name(w.eqp0(call_ins), "guard(js_CloneFunctionObject)"),
          OOM_EXIT);
    stack(0, call_ins);

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LAMBDA_FC()
{
    JSFunction* fun;
    fun = cx->fp()->script()->getFunction(getFullIndex());

    if (FUN_OBJECT(fun)->getParent() != globalObj)
        return ARECORD_STOP;

    if (GetBlockChainFast(cx, cx->fp(), JSOP_LAMBDA_FC, JSOP_LAMBDA_FC_LENGTH))
        RETURN_STOP_A("Unable to trace creating lambda in let");

    LIns* args[] = { scopeChain(), w.immpFunGC(fun), cx_ins };
    LIns* closure_ins = w.call(&js_AllocFlatClosure_ci, args);
    guard(false,
          w.name(w.eqp(closure_ins, w.immpNull()), "guard(js_AllocFlatClosure)"),
          OOM_EXIT);

    if (fun->u.i.nupvars) {
        JSUpvarArray *uva = fun->u.i.script->upvars();
        LIns* upvars_ins = w.getObjPrivatizedSlot(closure_ins,
                                                  JSObject::JSSLOT_FLAT_CLOSURE_UPVARS);

        for (uint32 i = 0, n = uva->length; i < n; i++) {
            Value v;
            LIns* v_ins = upvar(fun->u.i.script, uva, i, v);
            if (!v_ins)
                return ARECORD_STOP;

            box_value_into(v, v_ins, FCSlotsAddress(upvars_ins, i));
        }
    }

    stack(0, closure_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLEE()
{
    stack(0, get(&cx->fp()->calleeValue()));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETLOCALPOP()
{
    var(GET_SLOTNO(cx->regs->pc), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFPRIMTOP()
{
    // Traces are type-specialized, including null vs. object, so we need do
    // nothing here. The upstream unbox_value called after valueOf or toString
    // from an imacro (e.g.) will fork the trace for us, allowing us to just
    // follow along mindlessly :-).
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETCALL()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TRY()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FINALLY()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NOP()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGSUB()
{
    JSStackFrame* const fp = cx->fp();
    if (!fp->fun()->isHeavyweight()) {
        uintN slot = GET_ARGNO(cx->regs->pc);
        if (slot >= fp->numActualArgs())
            RETURN_STOP_A("can't trace out-of-range arguments");
        stack(0, get(&cx->fp()->canonicalActualArg(slot)));
        return ARECORD_CONTINUE;
    }
    RETURN_STOP_A("can't trace JSOP_ARGSUB hard case");
}

JS_REQUIRES_STACK LIns*
TraceRecorder::guardArgsLengthNotAssigned(LIns* argsobj_ins)
{
    // The following implements JSObject::isArgsLengthOverridden on trace.
    // ARGS_LENGTH_OVERRIDDEN_BIT is set if length was overridden.
    LIns *len_ins = w.getArgsLength(argsobj_ins);
    LIns *ovr_ins = w.andi(len_ins, w.nameImmi(JSObject::ARGS_LENGTH_OVERRIDDEN_BIT));
    guard(true, w.eqi0(ovr_ins), MISMATCH_EXIT);
    return len_ins;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGCNT()
{
    JSStackFrame * const fp = cx->fp();

    if (fp->fun()->flags & JSFUN_HEAVYWEIGHT)
        RETURN_STOP_A("can't trace heavyweight JSOP_ARGCNT");

    // argc is fixed on trace, so ideally we would simply generate LIR for
    // constant argc. But the user can mutate arguments.length in the
    // interpreter, so we have to check for that in the trace entry frame.
    // We also have to check that arguments.length has not been mutated
    // at record time, because if so we will generate incorrect constant
    // LIR, which will assert in alu().
    if (fp->hasArgsObj() && fp->argsObj().isArgsLengthOverridden())
        RETURN_STOP_A("can't trace JSOP_ARGCNT if arguments.length has been modified");
    LIns *a_ins = getFrameObjPtr(fp->addressOfArgs());
    if (callDepth == 0) {
        if (MaybeBranch mbr = w.jt(w.eqp0(a_ins))) {
            guardArgsLengthNotAssigned(a_ins);
            w.label(mbr);
        }
    }
    stack(0, w.immd(fp->numActualArgs()));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_DefLocalFunSetSlot(uint32 slot, JSObject* obj)
{
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, obj);

    if (FUN_NULL_CLOSURE(fun) && FUN_OBJECT(fun)->getParent() == globalObj) {
        LIns *proto_ins;
        CHECK_STATUS_A(getClassPrototype(JSProto_Function, proto_ins));

        LIns* args[] = { w.immpObjGC(globalObj), proto_ins, w.immpFunGC(fun), cx_ins };
        LIns* x = w.call(&js_NewNullClosure_ci, args);
        var(slot, x);
        return ARECORD_CONTINUE;
    }

    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFLOCALFUN()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFLOCALFUN_FC()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GOTOX()
{
    return record_JSOP_GOTO();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFEQX()
{
    return record_JSOP_IFEQ();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFNEX()
{
    return record_JSOP_IFNE();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ORX()
{
    return record_JSOP_OR();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ANDX()
{
    return record_JSOP_AND();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GOSUBX()
{
    return record_JSOP_GOSUB();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CASEX()
{
    strictEquality(true, true);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFAULTX()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TABLESWITCHX()
{
    return record_JSOP_TABLESWITCH();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LOOKUPSWITCHX()
{
    return InjectStatus(switchop());
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BACKPATCH()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BACKPATCH_POP()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_THROWING()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETRVAL()
{
    // If we implement this, we need to update JSOP_STOP.
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RETRVAL()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_REGEXP()
{
    JSStackFrame* const fp = cx->fp();
    JSScript* script = fp->script();
    unsigned index = atoms - script->atomMap.vector + GET_INDEX(cx->regs->pc);

    LIns* proto_ins;
    CHECK_STATUS_A(getClassPrototype(JSProto_RegExp, proto_ins));

    LIns* args[] = {
        proto_ins,
        w.immpObjGC(script->getRegExp(index)),
        cx_ins
    };
    LIns* regex_ins = w.call(&js_CloneRegExpObject_ci, args);
    guard(false, w.eqp0(regex_ins), OOM_EXIT);

    stack(0, regex_ins);
    return ARECORD_CONTINUE;
}

// begin JS_HAS_XML_SUPPORT

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DEFXMLNS()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ANYNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_QNAMEPART()
{
    return record_JSOP_STRING();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_QNAMECONST()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_QNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TOATTRNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TOATTRVAL()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ADDATTRNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ADDATTRVAL()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BINDXMLNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETXMLNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_XMLNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DESCENDANTS()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FILTER()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENDFILTER()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TOXML()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TOXMLLIST()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_XMLTAGEXPR()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_XMLELTEXPR()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_XMLCDATA()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_XMLCOMMENT()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_XMLPI()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETFUNNS()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STARTXML()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STARTXMLEXPR()
{
    return ARECORD_STOP;
}

// end JS_HAS_XML_SUPPORT

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLPROP()
{
    Value& l = stackval(-1);
    JSObject* obj;
    LIns* obj_ins;
    LIns* this_ins;
    if (!l.isPrimitive()) {
        obj = &l.toObject();
        obj_ins = get(&l);
        this_ins = obj_ins; // |this| for subsequent call
    } else {
        JSProtoKey protoKey;
        debug_only_stmt(const char* protoname = NULL;)
        if (l.isString()) {
            protoKey = JSProto_String;
            debug_only_stmt(protoname = "String.prototype";)
        } else if (l.isNumber()) {
            protoKey = JSProto_Number;
            debug_only_stmt(protoname = "Number.prototype";)
        } else if (l.isBoolean()) {
            protoKey = JSProto_Boolean;
            debug_only_stmt(protoname = "Boolean.prototype";)
        } else {
            JS_ASSERT(l.isNull() || l.isUndefined());
            RETURN_STOP_A("callprop on null or void");
        }

        if (!js_GetClassPrototype(cx, NULL, protoKey, &obj))
            RETURN_ERROR_A("GetClassPrototype failed!");

        obj_ins = w.immpObjGC(obj);
        debug_only_stmt(obj_ins = w.name(obj_ins, protoname);)
        this_ins = get(&l); // use primitive as |this|
    }

    JSObject* obj2;
    PCVal pcval;
    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    if (pcval.isNull())
        RETURN_STOP_A("callprop of missing method");

    if (pcval.isFunObj()) {
        if (l.isPrimitive()) {
            JSFunction* fun = GET_FUNCTION_PRIVATE(cx, &pcval.toFunObj());
            if (!PrimitiveThisTest(fun, l))
                RETURN_STOP_A("callee does not accept primitive |this|");
        }
        set(&l, w.immpObjGC(&pcval.toFunObj()));
    } else {
        if (l.isPrimitive())
            RETURN_STOP_A("callprop of primitive method");
        JS_ASSERT_IF(pcval.isShape(), !pcval.toShape()->isMethod());
        CHECK_STATUS_A(propTail(obj, obj_ins, obj2, pcval, NULL, NULL, &l));
    }
    stack(0, this_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELDESC()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_UINT24()
{
    stack(0, w.immd(GET_UINT24(cx->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INDEXBASE()
{
    atoms += GET_INDEXBASE(cx->regs->pc);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RESETBASE()
{
    updateAtoms();
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RESETBASE0()
{
    updateAtoms();
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLELEM()
{
    return record_JSOP_GETELEM();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STOP()
{
    /* A return from callDepth 0 terminates the current loop, except for recursion. */
    if (callDepth == 0) {
        AUDIT(returnLoopExits);
        return endLoop();
    }

    JSStackFrame *fp = cx->fp();

    if (fp->hasImacropc()) {
        /*
         * End of imacro, so return true to the interpreter immediately. The
         * interpreter's JSOP_STOP case will return from the imacro, back to
         * the pc after the calling op, still in the same JSStackFrame.
         */
        updateAtoms(fp->script());
        return ARECORD_CONTINUE;
    }

    putActivationObjects();

#ifdef MOZ_TRACE_JSCALLS
    if (cx->functionCallback) {
        LIns* args[] = { w.nameImmi(0), w.nameImmpNonGC(cx->fp()->fun()), cx_ins };
        LIns* call_ins = w.call(&functionProbe_ci, args);
        guard(false, w.eqi0(call_ins), MISMATCH_EXIT);
    }
#endif

    /*
     * We know falling off the end of a constructor returns the new object that
     * was passed in via fp->argv[-1], while falling off the end of a function
     * returns undefined.
     *
     * NB: we do not support script rval (eval, API users who want the result
     * of the last expression-statement, debugger API calls).
     */
    if (fp->isConstructing()) {
        rval_ins = get(&fp->thisValue());
    } else {
        rval_ins = w.immiUndefined();
    }
    clearCurrentFrameSlotsFromTracker(nativeFrameTracker);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETXPROP()
{
    Value& l = stackval(-1);
    if (l.isPrimitive())
        RETURN_STOP_A("primitive-this for GETXPROP?");

    Value* vp;
    LIns* v_ins;
    NameResult nr;
    CHECK_STATUS_A(name(vp, v_ins, nr));
    stack(-1, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLXMLNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TYPEOFEXPR()
{
    return record_JSOP_TYPEOF();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENTERBLOCK()
{
    JSObject* obj;
    obj = cx->fp()->script()->getObject(getFullIndex(0));

    LIns* void_ins = w.immiUndefined();
    for (int i = 0, n = OBJ_BLOCK_COUNT(cx, obj); i < n; i++)
        stack(i, void_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LEAVEBLOCK()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GENERATOR()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_YIELD()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARRAYPUSH()
{
    uint32_t slot = GET_UINT16(cx->regs->pc);
    JS_ASSERT(cx->fp()->numFixed() <= slot);
    JS_ASSERT(cx->fp()->slots() + slot < cx->regs->sp - 1);
    Value &arrayval = cx->fp()->slots()[slot];
    JS_ASSERT(arrayval.isObject());
    JS_ASSERT(arrayval.toObject().isDenseArray());
    LIns *array_ins = get(&arrayval);
    Value &elt = stackval(-1);
    LIns *elt_ins = box_value_for_native_call(elt, get(&elt));

    enterDeepBailCall();

    LIns *args[] = { elt_ins, array_ins, cx_ins };
    pendingGuardCondition = w.call(&js_ArrayCompPush_tn_ci, args);

    leaveDeepBailCall();
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENUMCONSTELEM()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LEAVEBLOCKEXPR()
{
    LIns* v_ins = stack(-1);
    int n = -1 - GET_UINT16(cx->regs->pc);
    stack(n, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETTHISPROP()
{
    LIns* this_ins;

    CHECK_STATUS_A(getThis(this_ins));

    /*
     * It's safe to just use cx->fp->thisValue() here because getThis() returns
     * ARECORD_STOP or ARECORD_ERROR if thisv is not available.
     */
    const Value &thisv = cx->fp()->thisValue();
    if (!thisv.isObject())
        RETURN_STOP_A("primitive this for GETTHISPROP");

    CHECK_STATUS_A(getProp(&thisv.toObject(), this_ins));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETARGPROP()
{
    return getProp(argval(GET_ARGNO(cx->regs->pc)));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETLOCALPROP()
{
    return getProp(varval(GET_SLOTNO(cx->regs->pc)));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INDEXBASE1()
{
    atoms += 1 << 16;
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INDEXBASE2()
{
    atoms += 2 << 16;
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INDEXBASE3()
{
    atoms += 3 << 16;
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLLOCAL()
{
    uintN slot = GET_SLOTNO(cx->regs->pc);
    stack(0, var(slot));
    stack(1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLARG()
{
    uintN slot = GET_ARGNO(cx->regs->pc);
    stack(0, arg(slot));
    stack(1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BINDGNAME()
{
    stack(0, w.immpObjGC(globalObj));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INT8()
{
    stack(0, w.immd(GET_INT8(cx->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INT32()
{
    stack(0, w.immd(GET_INT32(cx->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LENGTH()
{
    Value& l = stackval(-1);
    if (l.isPrimitive()) {
        if (!l.isString())
            RETURN_STOP_A("non-string primitive JSOP_LENGTH unsupported");
        set(&l, w.i2d(w.p2i(w.getStringLength(get(&l)))));
        return ARECORD_CONTINUE;
    }

    JSObject* obj = &l.toObject();
    LIns* obj_ins = get(&l);

    if (obj->isArguments()) {
        unsigned depth;
        JSStackFrame *afp = guardArguments(obj, obj_ins, &depth);
        if (!afp)
            RETURN_STOP_A("can't reach arguments object's frame");

        // We must both check at record time and guard at run time that
        // arguments.length has not been reassigned, redefined or deleted.
        if (obj->isArgsLengthOverridden())
            RETURN_STOP_A("can't trace JSOP_ARGCNT if arguments.length has been modified");
        LIns* slot_ins = guardArgsLengthNotAssigned(obj_ins);

        // slot_ins is the value from the slot; right-shift to get the length
        // (see JSObject::getArgsInitialLength in jsfun.cpp).
        LIns* v_ins = w.i2d(w.rshiN(slot_ins, JSObject::ARGS_PACKED_BITS_COUNT));
        set(&l, v_ins);
        return ARECORD_CONTINUE;
    }

    LIns* v_ins;
    if (obj->isArray()) {
        if (obj->isDenseArray()) {
            guardDenseArray(obj_ins, BRANCH_EXIT);
        } else {
            JS_ASSERT(obj->isSlowArray());
            guardClass(obj_ins, &js_SlowArrayClass, snapshot(BRANCH_EXIT), LOAD_NORMAL);
        }
        v_ins = w.i2d(w.lduiObjPrivate(obj_ins));
    } else if (OkToTraceTypedArrays && js_IsTypedArray(obj)) {
        // Ensure array is a typed array and is the same type as what was written
        guardClass(obj_ins, obj->getClass(), snapshot(BRANCH_EXIT), LOAD_NORMAL);
        v_ins = w.i2d(w.ldiConstTypedArrayLength(w.ldpObjPrivate(obj_ins)));
    } else {
        if (!obj->isNative())
            RETURN_STOP_A("can't trace length property access on non-array, non-native object");
        return getProp(obj, obj_ins);
    }
    set(&l, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEWARRAY()
{
    LIns *proto_ins;
    CHECK_STATUS_A(getClassPrototype(JSProto_Array, proto_ins));

    uint32 len = GET_UINT16(cx->regs->pc);
    cx->assertValidStackDepth(len);

    LIns* args[] = { w.immi(len), proto_ins, cx_ins };
    LIns* v_ins = w.call(&js_NewPreallocatedArray_ci, args);
    guard(false, w.eqp0(v_ins), OOM_EXIT);

    LIns* slots_ins = NULL;
    uint32 count = 0;
    for (uint32 i = 0; i < len; i++) {
        Value& v = stackval(int(i) - int(len));
        if (!v.isMagic())
            count++;
        stobj_set_dslot(v_ins, i, slots_ins, v, get(&v));
    }

    stack(-int(len), v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_HOLE()
{
    stack(0, w.immpMagicWhy(JS_ARRAY_HOLE));
    return ARECORD_CONTINUE;
}

AbortableRecordingStatus
TraceRecorder::record_JSOP_TRACE()
{
    return ARECORD_CONTINUE;
}

AbortableRecordingStatus
TraceRecorder::record_JSOP_NOTRACE()
{
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETMETHOD()
{
    return record_JSOP_SETPROP();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INITMETHOD()
{
    return record_JSOP_INITPROP();
}

JSBool FASTCALL
js_Unbrand(JSContext *cx, JSObject *obj)
{
    return obj->unbrand(cx);
}

JS_DEFINE_CALLINFO_2(extern, BOOL, js_Unbrand, CONTEXT, OBJECT, 0, ACCSET_STORE_ANY)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_UNBRAND()
{
    LIns* args_ins[] = { stack(-1), cx_ins };
    LIns* call_ins = w.call(&js_Unbrand_ci, args_ins);
    guard(false, w.eqi0(call_ins), OOM_EXIT);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_UNBRANDTHIS()
{
    LIns* this_ins;
    RecordingStatus status = getThis(this_ins);
    if (status != RECORD_CONTINUE)
        return InjectStatus(status);

    LIns* args_ins[] = { this_ins, cx_ins };
    LIns* call_ins = w.call(&js_Unbrand_ci, args_ins);
    guard(false, w.eqi0(call_ins), OOM_EXIT);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SHARPINIT()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETGLOBAL()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    stack(0, get(&globalObj->getSlotRef(slot)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETGLOBAL()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    set(&globalObj->getSlotRef(slot), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLGLOBAL()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    Value &v = globalObj->getSlotRef(slot);
    stack(0, get(&v));
    stack(1, w.immiUndefined());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GLOBALDEC()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(globalObj->getSlotRef(slot), -1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECGLOBAL()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(globalObj->getSlotRef(slot), -1, true));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCGLOBAL()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(globalObj->getSlotRef(slot), 1, true));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GLOBALINC()
{
    uint32 slot = cx->fp()->script()->getGlobalSlot(GET_SLOTNO(cx->regs->pc));
    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(globalObj->getSlotRef(slot), 1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETGNAME()
{
    return record_JSOP_NAME();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETGNAME()
{
    return record_JSOP_SETNAME();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GNAMEDEC()
{
    return record_JSOP_NAMEDEC();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GNAMEINC()
{
    return record_JSOP_NAMEINC();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECGNAME()
{
    return record_JSOP_DECNAME();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCGNAME()
{
    return record_JSOP_INCNAME();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLGNAME()
{
    return record_JSOP_CALLNAME();
}

#define DBG_STUB(OP)                                                          \
    JS_REQUIRES_STACK AbortableRecordingStatus                                \
    TraceRecorder::record_##OP()                                              \
    {                                                                         \
        RETURN_STOP_A("can't trace " #OP);                                    \
    }

DBG_STUB(JSOP_GETUPVAR_DBG)
DBG_STUB(JSOP_CALLUPVAR_DBG)
DBG_STUB(JSOP_DEFFUN_DBGFC)
DBG_STUB(JSOP_DEFLOCALFUN_DBGFC)
DBG_STUB(JSOP_LAMBDA_DBGFC)

#ifdef JS_JIT_SPEW
/*
 * Print information about entry typemaps and unstable exits for all peers
 * at a PC.
 */
void
DumpPeerStability(TraceMonitor* tm, const void* ip, JSObject* globalObj, uint32 globalShape,
                  uint32 argc)
{
    TreeFragment* f;
    bool looped = false;
    unsigned length = 0;

    for (f = LookupLoop(tm, ip, globalObj, globalShape, argc); f != NULL; f = f->peer) {
        if (!f->code())
            continue;
        debug_only_printf(LC_TMRecorder, "Stability of fragment %p:\nENTRY STACK=", (void*)f);
        if (looped)
            JS_ASSERT(f->nStackTypes == length);
        for (unsigned i = 0; i < f->nStackTypes; i++)
            debug_only_printf(LC_TMRecorder, "%c", TypeToChar(f->stackTypeMap()[i]));
        debug_only_print0(LC_TMRecorder, " GLOBALS=");
        for (unsigned i = 0; i < f->nGlobalTypes(); i++)
            debug_only_printf(LC_TMRecorder, "%c", TypeToChar(f->globalTypeMap()[i]));
        debug_only_print0(LC_TMRecorder, "\n");
        UnstableExit* uexit = f->unstableExits;
        while (uexit != NULL) {
            debug_only_print0(LC_TMRecorder, "EXIT  ");
            JSValueType* m = uexit->exit->fullTypeMap();
            debug_only_print0(LC_TMRecorder, "STACK=");
            for (unsigned i = 0; i < uexit->exit->numStackSlots; i++)
                debug_only_printf(LC_TMRecorder, "%c", TypeToChar(m[i]));
            debug_only_print0(LC_TMRecorder, " GLOBALS=");
            for (unsigned i = 0; i < uexit->exit->numGlobalSlots; i++) {
                debug_only_printf(LC_TMRecorder, "%c",
                                  TypeToChar(m[uexit->exit->numStackSlots + i]));
            }
            debug_only_print0(LC_TMRecorder, "\n");
            uexit = uexit->next;
        }
        length = f->nStackTypes;
        looped = true;
    }
}
#endif

#ifdef MOZ_TRACEVIS

FILE* traceVisLogFile = NULL;
JSHashTable *traceVisScriptTable = NULL;

JS_FRIEND_API(bool)
StartTraceVis(const char* filename = "tracevis.dat")
{
    if (traceVisLogFile) {
        // If we're currently recording, first we must stop.
        StopTraceVis();
    }

    traceVisLogFile = fopen(filename, "wb");
    if (!traceVisLogFile)
        return false;

    return true;
}

JS_FRIEND_API(JSBool)
StartTraceVisNative(JSContext *cx, uintN argc, jsval *vp)
{
    JSBool ok;

    if (argc > 0 && JSVAL_IS_STRING(JS_ARGV(cx, vp)[0])) {
        JSString *str = JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]);
        char *filename = js_DeflateString(cx, str->chars(), str->length());
        if (!filename)
            goto error;
        ok = StartTraceVis(filename);
        cx->free(filename);
    } else {
        ok = StartTraceVis();
    }

    if (ok) {
        fprintf(stderr, "started TraceVis recording\n");
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }

  error:
    JS_ReportError(cx, "failed to start TraceVis recording");
    return false;
}

JS_FRIEND_API(bool)
StopTraceVis()
{
    if (!traceVisLogFile)
        return false;

    fclose(traceVisLogFile); // not worth checking the result
    traceVisLogFile = NULL;

    return true;
}

JS_FRIEND_API(JSBool)
StopTraceVisNative(JSContext *cx, uintN argc, jsval *vp)
{
    JSBool ok = StopTraceVis();

    if (ok) {
        fprintf(stderr, "stopped TraceVis recording\n");
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
    } else {
        JS_ReportError(cx, "TraceVis isn't running");
    }

    return ok;
}

#endif /* MOZ_TRACEVIS */

JS_REQUIRES_STACK void
TraceRecorder::captureStackTypes(unsigned callDepth, JSValueType* typeMap)
{
    CaptureTypesVisitor capVisitor(cx, typeMap, !!oracle);
    VisitStackSlots(capVisitor, cx, callDepth);
}

JS_REQUIRES_STACK void
TraceRecorder::determineGlobalTypes(JSValueType* typeMap)
{
    DetermineTypesVisitor detVisitor(*this, typeMap);
    VisitGlobalSlots(detVisitor, cx, *tree->globalSlots);
}

#ifdef JS_METHODJIT

class AutoRetBlacklist
{
    jsbytecode* pc;
    bool* blacklist;

  public:
    AutoRetBlacklist(jsbytecode* pc, bool* blacklist)
      : pc(pc), blacklist(blacklist)
    { }

    ~AutoRetBlacklist()
    {
        *blacklist = IsBlacklisted(pc);
    }
};

JS_REQUIRES_STACK TracePointAction
RecordTracePoint(JSContext* cx, uintN& inlineCallCount, bool* blacklist, bool execAllowed)
{
    JSStackFrame* fp = cx->fp();
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    jsbytecode* pc = cx->regs->pc;

    JS_ASSERT(!TRACE_RECORDER(cx));

    JSObject* globalObj = cx->fp()->scopeChain().getGlobal();
    uint32 globalShape = -1;
    SlotList* globalSlots = NULL;

    AutoRetBlacklist autoRetBlacklist(pc, blacklist);

    if (!CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots)) {
        Backoff(cx, pc);
        return TPA_Nothing;
    }

    uint32 argc = entryFrameArgc(cx);
    TreeFragment* tree = LookupOrAddLoop(tm, pc, globalObj, globalShape, argc);

    debug_only_printf(LC_TMTracer,
                      "Looking for compat peer %d@%d, from %p (ip: %p)\n",
                      js_FramePCToLineNumber(cx, cx->fp()),
                      FramePCOffset(cx, cx->fp()), (void*)tree, tree->ip);

    if (tree->code() || tree->peer) {
        uintN count;
        TreeFragment* match = FindVMCompatiblePeer(cx, globalObj, tree, count);
        if (match) {
            VMSideExit* lr = NULL;
            VMSideExit* innermostNestedGuard = NULL;

            if (!execAllowed) {
                /* We've already compiled a trace for it, but we don't want to use that trace. */
                Blacklist((jsbytecode*)tree->root->ip);
                return TPA_Nothing;
            }

            /* Best case - just go and execute. */
            if (!ExecuteTree(cx, match, inlineCallCount, &innermostNestedGuard, &lr))
                return TPA_Error;

            if (!lr)
                return TPA_Nothing;

            switch (lr->exitType) {
              case UNSTABLE_LOOP_EXIT:
                if (!AttemptToStabilizeTree(cx, globalObj, lr, NULL, 0, NULL))
                    return TPA_RanStuff;
                break;

              case MUL_ZERO_EXIT:
              case OVERFLOW_EXIT:
                if (lr->exitType == MUL_ZERO_EXIT)
                    tm->oracle->markInstructionSlowZeroTest(cx->regs->pc);
                else
                    tm->oracle->markInstructionUndemotable(cx->regs->pc);
                /* FALL THROUGH */
              case BRANCH_EXIT:
              case CASE_EXIT:
                if (!AttemptToExtendTree(cx, lr, NULL, NULL, NULL))
                    return TPA_RanStuff;
                break;

              case LOOP_EXIT:
                if (!innermostNestedGuard)
                    return TPA_RanStuff;
                if (!AttemptToExtendTree(cx, innermostNestedGuard, lr, NULL, NULL))
                    return TPA_RanStuff;
                break;

              default:
                return TPA_RanStuff;
            }

            JS_ASSERT(TRACE_RECORDER(cx));

            goto interpret;
        }

        if (count >= MAXPEERS) {
            debug_only_print0(LC_TMTracer, "Blacklisted: too many peer trees.\n");
            Blacklist((jsbytecode*)tree->root->ip);
            return TPA_Nothing;
        }
    }

    if (++tree->hits() < HOTLOOP)
        return TPA_Nothing;
    if (!ScopeChainCheck(cx, tree))
        return TPA_Nothing;
    if (!RecordTree(cx, tree->first, NULL, 0, NULL, globalSlots))
        return TPA_Nothing;

  interpret:
    JS_ASSERT(TRACE_RECORDER(cx));

    /* Locked and loaded with a recorder. Ask the interperter to go run some code. */
    if (!Interpret(cx, fp, inlineCallCount, JSINTERP_RECORD))
        return TPA_Error;

    JS_ASSERT(!cx->throwing);
    
    return TPA_RanStuff;
}

LoopProfile::LoopProfile(JSScript *script, jsbytecode *top, jsbytecode *bottom)
    : script(script),
      top(top),
      bottom(bottom),
      hits(0),
      profiled(false),
      traceOK(false),
      numAllOps(0),
      numSelfOps(0),
      numSelfOpsMult(0),
      branchMultiplier(1),
      shortLoop(false),
      maybeShortLoop(false),
      numInnerLoops(0),
      loopStackDepth(0),
      sp(0)
{
    memset(allOps, 0, sizeof(allOps));
    memset(selfOps, 0, sizeof(selfOps));
}

MonitorResult
LoopProfile::profileLoopEdge(JSContext* cx, uintN& inlineCallCount)
{
    if (cx->regs->pc == top) {
        debug_only_print0(LC_TMProfiler, "Profiling complete (edge)\n");
        decide(cx);
    } else {
        /* Record an inner loop invocation. */
        JSScript *script = cx->fp()->script();
        jsbytecode *pc = cx->regs->pc;
        bool found = false;

        /* We started with the most deeply nested one first, since it gets hit most often.*/
        for (int i = int(numInnerLoops)-1; i >= 0; i--) {
            if (innerLoops[i].script == script && innerLoops[i].top == pc) {
                innerLoops[i].iters++;
                found = true;
            }
        }

        if (!found && numInnerLoops < PROFILE_MAX_INNER_LOOPS)
            innerLoops[numInnerLoops++] = InnerLoop(script, pc, NULL);
    }

    return MONITOR_NOT_RECORDING;
}


static const uintN PROFILE_HOTLOOP = 61;
static const uintN MAX_PROFILE_OPS = 4096;

static jsbytecode *
GetLoopBottom(JSContext *cx)
{
    jsbytecode* pc = cx->regs->pc;
    JSScript *script = cx->fp()->script();
    jssrcnote *sn = js_GetSrcNote(script, pc);
    if (!sn)
        return NULL;
    return pc + js_GetSrcNoteOffset(sn, 0);
}

static LoopProfile *
LookupOrAddProfile(JSContext *cx, TraceMonitor *tm, void** traceData, uintN *traceEpoch)
{
    LoopProfile *prof;

    /*
     * We try to keep a pointer to the loop profile inside the TRACE IC.
     * We also keep a pointer inside a hashtable for when we need to
     * look up nested loops (or when ICs are disabled).
     *
     * Memory for the profile is allocated in the dataAlloc for the
     * trace monitor. Since this thing can get flushed periodically,
     * we use epochs to decide if the profile in the MIC is valid, as
     * follows. Every time the trace monitor is flushed,
     * |tm->flushEpoch| is incremented. When storing the profile in
     * the IC, we store the current |tm->flushEpoch| along with it.
     * Before pulling a profile out of the IC, we check that its
     * stored epoch is still up-to-date with |tm->flushEpoch|.
     * This ensures that no flush has happened in between.
     */

#if JS_MONOIC
    if (*traceData && *traceEpoch == tm->flushEpoch) {
        prof = (LoopProfile *)*traceData;
    } else {
        jsbytecode* pc = cx->regs->pc;
        jsbytecode* bottom = GetLoopBottom(cx);
        if (!bottom)
            return NULL;
        prof = new (*tm->dataAlloc) LoopProfile(cx->fp()->script(), pc, bottom);
        *traceData = prof;
        *traceEpoch = tm->flushEpoch;
        tm->loopProfiles->put(pc, prof);
    }
#else
    LoopProfileMap &table = *tm->loopProfiles;
    jsbytecode* pc = cx->regs->pc;
    if (LoopProfileMap::AddPtr p = table.lookupForAdd(pc)) {
        prof = p->value;
    } else {
        jsbytecode* bottom = GetLoopBottom(cx);
        if (!bottom)
            return NULL;
        prof = new (*tm->dataAlloc) LoopProfile(cx->fp()->script(), pc, bottom);
        table.add(p, pc, prof);
    }
#endif

    return prof;
}

JS_REQUIRES_STACK TracePointAction
MonitorTracePoint(JSContext *cx, uintN& inlineCallCount, bool* blacklist,
                  void** traceData, uintN *traceEpoch)
{
    if (!cx->profilingEnabled)
        return RecordTracePoint(cx, inlineCallCount, blacklist, true);

    *blacklist = false;

    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    /*
     * We may have re-entered Interpret while profiling. We don't profile
     * the nested invocation.
     */
    if (tm->profile)
        return TPA_Nothing;

    LoopProfile *prof = LookupOrAddProfile(cx, tm, traceData, traceEpoch);
    if (!prof) {
        *blacklist = true;
        return TPA_Nothing;
    }

    if (prof->hits++ < PROFILE_HOTLOOP)
        return TPA_Nothing;

    AutoRetBlacklist autoRetBlacklist(cx->regs->pc, blacklist);

    if (prof->profiled) {
        if (prof->traceOK) {
            return RecordTracePoint(cx, inlineCallCount, blacklist, prof->execOK);
        } else {
            return TPA_Nothing;
        }
    }

    debug_only_printf(LC_TMProfiler, "Profiling at line %d\n",
                      js_FramePCToLineNumber(cx, cx->fp()));

    tm->profile = prof;

    if (!Interpret(cx, cx->fp(), inlineCallCount, JSINTERP_PROFILE))
        return TPA_Error;

    JS_ASSERT(!cx->throwing);
    
    return TPA_RanStuff;
}

/*
 * Returns true if pc is within the given loop.
 * If we're in a different script, then we must have come from
 * a call instruction within the loop (since we check if we're within
 * the loop before each instruction) so we're still in the loop.
 */
template<class T>
static inline bool
PCWithinLoop(JSScript *script, jsbytecode *pc, T& loop)
{
    return script != loop.script || (pc >= loop.top && pc <= loop.bottom);
}

LoopProfile::ProfileAction
LoopProfile::profileOperation(JSContext* cx, JSOp op)
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    if (profiled) {
        tm->profile = NULL;
        return ProfComplete;
    }

    jsbytecode *pc = cx->regs->pc;
    JSScript *script = cx->fp()->maybeScript();

    if (!PCWithinLoop(script, pc, *this)) {
        debug_only_printf(LC_TMProfiler, "Profiling complete (loop exit) at %d (line %u)\n",
                          (int)(cx->regs->pc - cx->fp()->script()->code),
                          js_FramePCToLineNumber(cx, cx->fp()));
        tm->profile->decide(cx);
        tm->profile = NULL;
        return ProfComplete;
    }

    while (loopStackDepth > 0 && !PCWithinLoop(script, pc, loopStack[loopStackDepth-1])) {
        debug_only_print0(LC_TMProfiler, "Profiler: Exiting inner loop\n");
        loopStackDepth--;
    }

    if (op == JSOP_TRACE || op == JSOP_NOTRACE) {
        if (pc != top && (loopStackDepth == 0 || pc != loopStack[loopStackDepth-1].top)) {
            if (loopStackDepth == PROFILE_MAX_INNER_LOOPS) {
                debug_only_print0(LC_TMProfiler, "Profiling complete (maxnest)\n");
                tm->profile->decide(cx);
                tm->profile = NULL;
                return ProfComplete;
            }

            debug_only_print0(LC_TMProfiler, "Profiler: Entering inner loop\n");
            loopStack[loopStackDepth++] = InnerLoop(script, pc, GetLoopBottom(cx));
        }
    }

    numAllOps++;
    if (loopStackDepth == 0) {
        numSelfOps++;
        numSelfOpsMult += branchMultiplier;
    }

    if (op == JSOP_ADD || op == JSOP_SUB || op == JSOP_MUL || op == JSOP_DIV) {
        Value& v1 = cx->regs->sp[-1];
        Value& v2 = cx->regs->sp[-2];

        /* If either operand is a double, treat it as a floating-point op. */
        if (v1.isDouble() || v2.isDouble())
            increment(OP_FLOAT);
        else if (v1.isInt32() || v2.isInt32())
            increment(OP_INT);
    }

    if (op == JSOP_EQ || op == JSOP_NE)
        increment(OP_EQ);

    if (op == JSOP_BITOR || op == JSOP_BITXOR || op == JSOP_BITAND
        || op == JSOP_LSH || op == JSOP_RSH || op == JSOP_URSH || op == JSOP_BITNOT)
    {
        increment(OP_BIT);
    }

    if (op == JSOP_EVAL)
        increment(OP_EVAL);

    if (op == JSOP_NEW)
        increment(OP_NEW);

    if (op == JSOP_GETELEM || op == JSOP_SETELEM) {
        Value& lval = cx->regs->sp[op == JSOP_GETELEM ? -2 : -3];
        if (lval.isObject() && js_IsTypedArray(&lval.toObject()))
            increment(OP_TYPED_ARRAY);
    }

    if (op == JSOP_CALL) {
        increment(OP_CALL);

        uintN argc = GET_ARGC(cx->regs->pc);
        Value &v = cx->regs->sp[-((int)argc + 2)];
        JSObject *callee;
        if (IsFunctionObject(v, &callee)) {
            JSFunction *fun = callee->getFunctionPrivate();
            if (fun->isInterpreted()) {
                if (cx->fp()->isFunctionFrame() && fun == cx->fp()->fun())
                    increment(OP_RECURSIVE);
            } else {
                js::Native native = fun->u.n.native;
                if (js_IsMathFunction(JS_JSVALIFY_NATIVE(native)))
                    increment(OP_FLOAT);
            }
        }
    }

    if (op == JSOP_CALLPROP && loopStackDepth == 0)
        branchMultiplier *= mjit::GetCallTargetCount(cx->fp()->script(), cx->regs->pc);

    if (numAllOps >= MAX_PROFILE_OPS) {
        debug_only_print0(LC_TMProfiler, "Profiling complete (maxops)\n");
        tm->profile->decide(cx);
        tm->profile = NULL;
        return ProfComplete;
    }

    /* These are the places where the interpreter skips over branches. */
    jsbytecode *testPC = cx->regs->pc;
    if (op == JSOP_EQ || op == JSOP_NE || op == JSOP_LT || op == JSOP_GT
        || op == JSOP_LE || op == JSOP_GE || op == JSOP_IN || op == JSOP_MOREITER)
    {
        const JSCodeSpec *cs = &js_CodeSpec[op];
        ptrdiff_t oplen = cs->length;
        JSScript *script = cx->fp()->script();
        JS_ASSERT(oplen != -1);

        if (cx->regs->pc - script->code + oplen < ptrdiff_t(script->length))
            if (cx->regs->pc[oplen] == JSOP_IFEQ || cx->regs->pc[oplen] == JSOP_IFNE)
                testPC = cx->regs->pc + oplen;
    }

    /* Check if we're exiting the loop being profiled. */
    JSOp testOp = js_GetOpcode(cx, cx->fp()->script(), testPC);
    if (testOp == JSOP_IFEQ || testOp == JSOP_IFNE || testOp == JSOP_GOTO
        || testOp == JSOP_AND || testOp == JSOP_OR)
    {
        ptrdiff_t len = GET_JUMP_OFFSET(testPC);
        if (testPC + len == top && (op == JSOP_LT || op == JSOP_LE)) {
            StackValue v = stackAt(-1);
            if (v.hasValue && v.value < 8)
                shortLoop = true;
        }

        if (testPC + len == top && (op == JSOP_LT || op == JSOP_LE)
            && cx->regs->sp[-2].isInt32() && cx->regs->sp[-2].toInt32() < 16)
        {
            maybeShortLoop = true;
        }

        if (testOp != JSOP_GOTO && len > 0) {
            bool isConst;
            if (testOp == JSOP_IFEQ || testOp == JSOP_IFNE)
                isConst = stackAt(-1).isConst && stackAt(-2).isConst;
            else
                isConst = stackAt(-1).isConst;

            increment(OP_FWDJUMP);
            if (loopStackDepth == 0 && !isConst)
                branchMultiplier *= 2;
        }
    }

    if (op == JSOP_INT8) {
        stackPush(StackValue(true, GET_INT8(cx->regs->pc)));
    } else if (op == JSOP_STRING) {
        stackPush(StackValue(true));
    } else if (op == JSOP_TYPEOF || op == JSOP_TYPEOFEXPR) {
        stackPush(StackValue(true));
    } else if (op == JSOP_EQ || op == JSOP_NE) {
        StackValue v1 = stackAt(-1);
        StackValue v2 = stackAt(-2);
        stackPush(StackValue(v1.isConst && v2.isConst));
    } else if (op == JSOP_AND) {
        bool b = !!js_ValueToBoolean(cx->regs->sp[-1]);
        StackValue v = stackAt(-1);
        if (b)
            stackPop();
    } else {
        stackClear();
    }
    
    return ProfContinue;
}

static LoopProfile *
LookupLoopProfile(JSContext *cx, jsbytecode *pc)
{
    TraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    LoopProfileMap &table = *tm->loopProfiles;
    if (LoopProfileMap::Ptr p = table.lookup(pc)) {
        JS_ASSERT(p->value->top == pc);
        return p->value;
    } else
        return NULL;
}

/*
 * Returns true if the loop would probably take a long time to
 * compile. Since this function recurses into nested loops, we
 * pass a depth argument to ensure that we don't recurse too
 * far and overflow the stack. Mostly, we're guarding against
 * the possibility that we (incorrectly) track a loop as being
 * nested inside itself, leading to infinite recursion.
 */
bool
LoopProfile::isCompilationExpensive(JSContext *cx, uintN depth)
{
    if (depth == 0)
        return true;

    /* Too many ops to compile? */
    if (numSelfOps == MAX_PROFILE_OPS)
        return true;

    /* Is the code too branchy? */
    if (numSelfOpsMult >= numSelfOps*100000)
        return true;

    /* Ensure that inner loops aren't too expensive. */
    for (uintN i=0; i<numInnerLoops; i++) {
        LoopProfile *prof = LookupLoopProfile(cx, innerLoops[i].top);
        if (prof && prof->isCompilationExpensive(cx, depth-1))
            return true;
    }

    return false;
}

/*
 * This function recognizes loops that are short and that contain
 * jumps. The tracer does badly with these loops because it
 * needs to do a lot of side exits, which are somewhat
 * expensive.
 */
bool
LoopProfile::isCompilationUnprofitable(JSContext *cx, uintN depth)
{
    if (depth == 0)
        return true;

    if (numAllOps < 15 && allOps[OP_FWDJUMP])
        return true;
    
    /* Ensure that inner loops aren't fleeting. */
    for (uintN i=0; i<numInnerLoops; i++) {
        LoopProfile *prof = LookupLoopProfile(cx, innerLoops[i].top);
        if (prof && prof->isCompilationUnprofitable(cx, depth-1))
            return true;
    }

    return false;
}

/* After profiling is done, this method decides whether to trace the loop. */
void
LoopProfile::decide(JSContext *cx)
{
    profiled = true;

#ifdef DEBUG
    uintN line = js_PCToLineNumber(cx, script, top);

    debug_only_printf(LC_TMProfiler, "LOOP %s:%d\n", script->filename, line);

    for (uintN i=0; i<numInnerLoops; i++) {
        InnerLoop &loop = innerLoops[i];
        if (LoopProfile *prof = LookupLoopProfile(cx, loop.top)) {
            uintN line = js_PCToLineNumber(cx, prof->script, prof->top);
            debug_only_printf(LC_TMProfiler, "NESTED %s:%d (%d iters)\n",
                              prof->script->filename, line, loop.iters);
        }
    }
    debug_only_printf(LC_TMProfiler, "FEATURE float %d\n", allOps[OP_FLOAT]);
    debug_only_printf(LC_TMProfiler, "FEATURE int %d\n", allOps[OP_INT]);
    debug_only_printf(LC_TMProfiler, "FEATURE bit %d\n", allOps[OP_BIT]);
    debug_only_printf(LC_TMProfiler, "FEATURE equality %d\n", allOps[OP_EQ]);
    debug_only_printf(LC_TMProfiler, "FEATURE eval %d\n", allOps[OP_EVAL]);
    debug_only_printf(LC_TMProfiler, "FEATURE new %d\n", allOps[OP_NEW]);
    debug_only_printf(LC_TMProfiler, "FEATURE call %d\n", allOps[OP_CALL]);
    debug_only_printf(LC_TMProfiler, "FEATURE typedarray %d\n", allOps[OP_TYPED_ARRAY]);
    debug_only_printf(LC_TMProfiler, "FEATURE fwdjump %d\n", allOps[OP_FWDJUMP]);
    debug_only_printf(LC_TMProfiler, "FEATURE recursive %d\n", allOps[OP_RECURSIVE]);
    debug_only_printf(LC_TMProfiler, "FEATURE shortLoop %d\n", shortLoop);
    debug_only_printf(LC_TMProfiler, "FEATURE maybeShortLoop %d\n", maybeShortLoop);
    debug_only_printf(LC_TMProfiler, "FEATURE numAllOps %d\n", numAllOps);
    debug_only_printf(LC_TMProfiler, "FEATURE selfOps %d\n", numSelfOps);
    debug_only_printf(LC_TMProfiler, "FEATURE selfOpsMult %g\n", numSelfOpsMult);
#endif

    traceOK = false;
    if (count(OP_RECURSIVE)) {
        /* don't trace */
    } else if (count(OP_EVAL)) {
        /* don't trace */
    } else if (numInnerLoops > 3) {
        /* don't trace */
    } else if (shortLoop) {
        /* don't trace */
    } else if (maybeShortLoop && numInnerLoops < 2) {
        /* don't trace */
    } else if (isCompilationExpensive(cx, 4)) {
        /* don't trace */
    } else if (isCompilationUnprofitable(cx, 4)) {
        /* don't trace */
    } else {
        uintN goodOps = 0;

        /* The tracer handles these ops well because of type specialization. */
        goodOps += count(OP_FLOAT)*10 + count(OP_BIT)*10 + count(OP_INT)*5;

        /* The tracer handles these ops well because of inlining. */
        goodOps += (count(OP_CALL) + count(OP_NEW))*20;

        /* The tracer specialized typed array access. */
        goodOps += count(OP_TYPED_ARRAY)*10;

        debug_only_printf(LC_TMProfiler, "FEATURE goodOps %u\n", goodOps);
        
        if (goodOps >= numAllOps)
            traceOK = true;
    }

    debug_only_printf(LC_TMProfiler, "TRACE %s:%d = %d\n", script->filename, line, traceOK);

    if (traceOK) {
        /* Unblacklist the inner loops. */
        for (uintN i=0; i<numInnerLoops; i++) {
            InnerLoop &loop = innerLoops[i];
            LoopProfile *prof = LookupLoopProfile(cx, loop.top);
            if (prof) {
                /*
                 * Note that execOK for the inner loop is left unchanged. So even
                 * if we trace the inner loop, we will never call that trace
                 * on its own. We'll only call it from this trace.
                 */
                prof->profiled = true;
                prof->traceOK = true;
                if (IsBlacklisted(loop.top))
                    debug_only_printf(LC_TMProfiler, "Unblacklisting at %d\n",
                                      js_PCToLineNumber(cx, loop.script, loop.top));
                Unblacklist(loop.script, loop.top);
            }
        }
    }

    if (!traceOK) {
        debug_only_printf(LC_TMProfiler, "Blacklisting at %d\n", line);
        Blacklist(top);
    }

    debug_only_print0(LC_TMProfiler, "\n");

    execOK = traceOK;
}

JS_REQUIRES_STACK MonitorResult
MonitorLoopEdge(JSContext* cx, uintN& inlineCallCount)
{
    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    if (tm->profile)
        return tm->profile->profileLoopEdge(cx, inlineCallCount);
    else
        return RecordLoopEdge(cx, inlineCallCount);
}

void
AbortProfiling(JSContext *cx)
{
    debug_only_print0(LC_TMProfiler, "Profiling complete (aborted)\n");
    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    tm->profile->numSelfOps = MAX_PROFILE_OPS;
    tm->profile->decide(cx);
    tm->profile = NULL;
}

#else /* JS_METHODJIT */

JS_REQUIRES_STACK MonitorResult
MonitorLoopEdge(JSContext* cx, uintN& inlineCallCount)
{
    return RecordLoopEdge(cx, inlineCallCount);
}

#endif /* JS_METHODJIT */

} /* namespace js */

