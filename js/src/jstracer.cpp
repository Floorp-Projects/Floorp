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
#include "jstracer.h"
#include "jsxml.h"

#include "jsatominlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"        // generated headers last
#include "imacros.c.out"

using namespace nanojit;

#if JS_HAS_XML_SUPPORT
#define RETURN_VALUE_IF_XML(val, ret)                                         \
    JS_BEGIN_MACRO                                                            \
        if (!JSVAL_IS_PRIMITIVE(val) &&                                       \
            OBJECT_IS_XML(BOGUS_CX, JSVAL_TO_OBJECT(val))) {                  \
            RETURN_VALUE("xml detected", ret);                                \
        }                                                                     \
    JS_END_MACRO
#else
#define RETURN_IF_XML(val, ret) ((void) 0)
#endif

#define RETURN_IF_XML_A(val) RETURN_VALUE_IF_XML(val, ARECORD_STOP)
#define RETURN_IF_XML(val)   RETURN_VALUE_IF_XML(val, RECORD_STOP)

/*
 * Never use JSVAL_IS_BOOLEAN because it restricts the value (true, false) and
 * the type. What you want to use is JSVAL_IS_SPECIAL(x) and then handle the
 * undefined case properly (bug 457363).
 */
#undef JSVAL_IS_BOOLEAN
#define JSVAL_IS_BOOLEAN(x) JS_STATIC_ASSERT(0)

JS_STATIC_ASSERT(sizeof(JSTraceType) == 1);

/* Map to translate a type tag into a printable representation. */
static const char typeChar[] = "OIDXSNBF";
static const char tagChar[]  = "OIDISIBI";

/* Blacklist parameters. */

/*
 * Number of iterations of a loop where we start tracing.  That is, we don't
 * start tracing until the beginning of the HOTLOOP-th iteration.
 */
#define HOTLOOP 2

/* Attempt recording this many times before blacklisting permanently. */
#define BL_ATTEMPTS 2

/* Skip this many hits before attempting recording again, after an aborted attempt. */
#define BL_BACKOFF 32

/* Number of times we wait to exit on a side exit before we try to extend the tree. */
#define HOTEXIT 1

/* Number of times we try to extend the tree along a side exit. */
#define MAXEXIT 3

/* Maximum number of peer trees allowed. */
#define MAXPEERS 9

/* Max number of hits to a RECURSIVE_UNLINKED exit before we trash the tree. */
#define MAX_RECURSIVE_UNLINK_HITS 64

/* Max call depths for inlining. */
#define MAX_CALLDEPTH 10

/* Max number of slots in a table-switch. */
#define MAX_TABLE_SWITCH 256

/* Max memory needed to rebuild the interpreter stack when falling off trace. */
#define MAX_INTERP_STACK_BYTES                                                \
    (MAX_NATIVE_STACK_SLOTS * sizeof(jsval) +                                 \
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
js_InitJITStatsClass(JSContext *cx, JSObject *glob)
{
    JS_InitClass(cx, glob, NULL, &jitstats_class, NULL, 0, jitstats_props, NULL, NULL, NULL);
}

#define AUDIT(x) (jitstats.x++)
#else
#define AUDIT(x) ((void)0)
#endif /* JS_JIT_SPEW */

/*
 * INS_CONSTPTR can be used to embed arbitrary pointers into the native code. It should not
 * be used directly to embed GC thing pointers. Instead, use the INS_CONSTOBJ/FUN/STR/SPROP
 * variants which ensure that the embedded pointer will be kept alive across GCs.
 */

#define INS_CONST(c)          addName(lir->insImm(c), #c)
#define INS_CONSTPTR(p)       addName(lir->insImmPtr(p), #p)
#define INS_CONSTWORD(v)      addName(lir->insImmPtr((void *) (v)), #v)
#define INS_CONSTVAL(v)       addName(insImmVal(v), #v)
#define INS_CONSTOBJ(obj)     addName(insImmObj(obj), #obj)
#define INS_CONSTFUN(fun)     addName(insImmFun(fun), #fun)
#define INS_CONSTSTR(str)     addName(insImmStr(str), #str)
#define INS_CONSTSPROP(sprop) addName(insImmSprop(sprop), #sprop)
#define INS_ATOM(atom)        INS_CONSTSTR(ATOM_TO_STRING(atom))
#define INS_NULL()            INS_CONSTPTR(NULL)
#define INS_VOID()            INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID))

static avmplus::AvmCore s_core = avmplus::AvmCore();
static avmplus::AvmCore* core = &s_core;

/* Allocator SPI implementation. */

void*
nanojit::Allocator::allocChunk(size_t nbytes)
{
    VMAllocator *vma = (VMAllocator*)this;
    JS_ASSERT(!vma->outOfMemory());
    void *p = calloc(1, nbytes);
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
        free(p);
}

void
nanojit::Allocator::postReset() {
    VMAllocator *vma = (VMAllocator*)this;
    vma->mOutOfMemory = false;
    vma->mSize = 0;
}

static void OutOfMemoryAbort()
{
    JS_NOT_REACHED("out of memory");
    abort();
}

#ifdef JS_JIT_SPEW
static void
DumpPeerStability(JSTraceMonitor* tm, const void* ip, JSObject* globalObj, uint32 globalShape, uint32 argc);
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
LogControl js_LogController;

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

    js_LogController.lcbits = 0;

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
            "  full         everything except 'treevis' and 'nocodeaddrs'\n"
            "  tracer       tracer lifetime (FIXME:better description)\n"
            "  recorder     trace recording stuff (FIXME:better description)\n"
            "  abort        show trace recording aborts\n"
            "  stats        show trace recording stats\n"
            "  regexp       show compilation & entry for regexps\n"
            "  treevis      spew that tracevis/tree.py can parse\n"
            "  ------ options for Nanojit ------\n"
            "  fragprofile  count entries and exits for each fragment\n"
            "  liveness     show LIR liveness at start of rdr pipeline\n"
            "  readlir      show LIR as it enters the reader pipeline\n"
            "  aftersf      show LIR after StackFilter\n"
            "  assembly     show final aggregated assembly code\n"
            "  regalloc     show regalloc state in 'assembly' output\n"
            "  activation   show activation state in 'assembly' output\n"
            "  nocodeaddrs  omit code addresses in 'assembly' output\n"
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
    if (strstr(tmf, "regexp")   || strstr(tmf, "full")) bits |= LC_TMRegexp;
    if (strstr(tmf, "treevis"))                         bits |= LC_TMTreeVis;

    /* flags for nanojit */
    if (strstr(tmf, "fragprofile"))                       bits |= LC_FragProfile;
    if (strstr(tmf, "liveness")   || strstr(tmf, "full")) bits |= LC_Liveness;
    if (strstr(tmf, "activation") || strstr(tmf, "full")) bits |= LC_Activation;
    if (strstr(tmf, "readlir")    || strstr(tmf, "full")) bits |= LC_ReadLIR;
    if (strstr(tmf, "aftersf")    || strstr(tmf, "full")) bits |= LC_AfterSF;
    if (strstr(tmf, "regalloc")   || strstr(tmf, "full")) bits |= LC_RegAlloc;
    if (strstr(tmf, "assembly")   || strstr(tmf, "full")) bits |= LC_Assembly;
    if (strstr(tmf, "nocodeaddrs"))                       bits |= LC_NoCodeAddrs;

    js_LogController.lcbits = bits;
    return;

}
#endif

/* ------------------ Frag-level profiling support ------------------ */

#ifdef JS_JIT_SPEW

/*
 * All the allocations done by this profile data-collection and
 * display machinery, are done in JSTraceMonitor::profAlloc.  That is
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

/* A mapping of Fragment.profFragID to FragPI */
typedef HashMap<uint32,FragPI> FragStatsMap;

void
js_FragProfiling_FragFinalizer(Fragment* f, JSTraceMonitor* tm)
{
    // Recover profiling data from 'f', which is logically at the end
    // of its useful lifetime.
    if (!(js_LogController.lcbits & LC_FragProfile))
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
js_FragProfiling_showResults(JSTraceMonitor* tm)
{
    uint32_t topFragID[N_TOP_BLOCKS];
    FragPI   topPI[N_TOP_BLOCKS];
    uint64_t totCount = 0, cumulCount;
    uint32_t totSE = 0;
    size_t   totCodeB = 0, totExitB = 0;
    memset(topFragID, 0, sizeof(topFragID));
    memset(topPI,     0, sizeof(topPI));
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
        AvmAssert(r >= 0 && r <= N_TOP_BLOCKS);
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

    js_LogController.printf(
        "\n----------------- Per-fragment execution counts ------------------\n");
    js_LogController.printf(
        "\nTotal count = %llu\n\n", (unsigned long long int)totCount);

    js_LogController.printf(
        "           Entry counts         Entry counts       ----- Static -----\n");
    js_LogController.printf(
        "         ------Self------     ----Cumulative---   Exits  Cbytes Xbytes   FragID\n");
    js_LogController.printf("\n");

    if (totCount == 0)
        totCount = 1; /* avoid division by zero */
    cumulCount = 0;
    int r;
    for (r = 0; r < N_TOP_BLOCKS; r++) {
        if (topFragID[r] == 0)
            break;
        cumulCount += (uint64_t)topPI[r].count;
        js_LogController.printf("%3d:     %5.2f%% %9u     %6.2f%% %9llu"
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
    js_LogController.printf("\nTotal displayed code bytes = %u, "
                            "exit bytes = %u\n"
                            "Total displayed static exits = %d\n\n",
                            (unsigned int)totCodeB, (unsigned int)totExitB, totSE);

    js_LogController.printf("Analysis by exit counts\n\n");

    for (r = 0; r < N_TOP_BLOCKS; r++) {
        if (topFragID[r] == 0)
            break;
        js_LogController.printf("FragID=%06u, total count %u:\n", topFragID[r],
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
            js_LogController.printf("   GuardID=%03u    %7u (%5.2f%%)\n",
                                    gpi.guardID, gpi.count,
                                    100.0 * (double)gpi.count / (double)totThisFrag);
        }
        js_LogController.printf("   Looped (%03u)   %7u (%5.2f%%)\n",
                                topPI[r].largestGuardID+1,
                                madeItToEnd,
                                100.0 * (double)madeItToEnd /  (double)totThisFrag);
        NanoAssert(madeItToEnd <= topPI[r].count); // else unsigned underflow
        js_LogController.printf("\n");
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
        default:
            fprintf(out, "[invalid %%%c]", *p);
        }
    }

#undef GET_ARG

    return JS_TRUE;
}

JS_DEFINE_CALLINFO_3(extern, BOOL, PrintOnTrace, CHARPTR, UINT32, DOUBLEPTR, 0, 0)

// This version is not intended to be called directly: usually it is easier to
// use one of the other overloads.
void
TraceRecorder::tprint(const char *format, int count, nanojit::LIns *insa[])
{
    size_t size = strlen(format) + 1;
    char* data = (char*) traceMonitor->traceAlloc->alloc(size);
    memcpy(data, format, size);

    double *args = (double*) traceMonitor->traceAlloc->alloc(count * sizeof(double));
    for (int i = 0; i < count; ++i) {
        JS_ASSERT(insa[i]);
        lir->insStorei(insa[i], INS_CONSTPTR(args), sizeof(double) * i);
    }

    LIns* args_ins[] = { INS_CONSTPTR(args), INS_CONST(count), INS_CONSTPTR(data) };
    LIns* call_ins = lir->insCall(&PrintOnTrace_ci, args_ins);
    guard(false, lir->ins_eq0(call_ins), MISMATCH_EXIT);    
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

/*
 * The entire VM shares one oracle. Collisions and concurrent updates are
 * tolerated and worst case cause performance regressions.
 */
static Oracle oracle;

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
    struct TrackerPage* p = (struct TrackerPage*) calloc(1, sizeof(*p));
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
        free(p);
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

static inline jsuint
argSlots(JSStackFrame* fp)
{
    return JS_MAX(fp->argc, fp->fun->nargs);
}

static inline bool
isNumber(jsval v)
{
    return JSVAL_IS_INT(v) || JSVAL_IS_DOUBLE(v);
}

static inline jsdouble
asNumber(jsval v)
{
    JS_ASSERT(isNumber(v));
    if (JSVAL_IS_DOUBLE(v))
        return *JSVAL_TO_DOUBLE(v);
    return (jsdouble)JSVAL_TO_INT(v);
}

static inline bool
isInt32(jsval v)
{
    if (!isNumber(v))
        return false;
    jsdouble d = asNumber(v);
    jsint i;
    return !!JSDOUBLE_IS_INT(d, i);
}

static inline jsint
asInt32(jsval v)
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

/* Return TT_DOUBLE for all numbers (int and double) and the tag otherwise. */
static inline JSTraceType
GetPromotedType(jsval v)
{
    if (JSVAL_IS_INT(v))
        return TT_DOUBLE;
    if (JSVAL_IS_OBJECT(v)) {
        if (JSVAL_IS_NULL(v))
            return TT_NULL;
        if (HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(v)))
            return TT_FUNCTION;
        return TT_OBJECT;
    }
    uint8_t tag = JSVAL_TAG(v);
    JS_ASSERT(tag == JSVAL_DOUBLE || tag == JSVAL_STRING || tag == JSVAL_SPECIAL);
    JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_DOUBLE) == JSVAL_DOUBLE);
    JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_STRING) == JSVAL_STRING);
    JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_PSEUDOBOOLEAN) == JSVAL_SPECIAL);
    return JSTraceType(tag);
}

/* Return TT_INT32 for all whole numbers that fit into signed 32-bit and the tag otherwise. */
static inline JSTraceType
getCoercedType(jsval v)
{
    if (isInt32(v))
        return TT_INT32;
    if (JSVAL_IS_OBJECT(v)) {
        if (JSVAL_IS_NULL(v))
            return TT_NULL;
        if (HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(v)))
            return TT_FUNCTION;
        return TT_OBJECT;
    }
    uint8_t tag = JSVAL_TAG(v);
    JS_ASSERT(tag == JSVAL_DOUBLE || tag == JSVAL_STRING || tag == JSVAL_SPECIAL);
    JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_DOUBLE) == JSVAL_DOUBLE);
    JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_STRING) == JSVAL_STRING);
    JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_PSEUDOBOOLEAN) == JSVAL_SPECIAL);
    return JSTraceType(tag);
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
    HashAccum(h, uintptr_t(cx->fp->script), ORACLE_MASK);
    HashAccum(h, uintptr_t(pc), ORACLE_MASK);
    HashAccum(h, uintptr_t(slot), ORACLE_MASK);
    return int(h);
}

static JS_REQUIRES_STACK inline int
GlobalSlotHash(JSContext* cx, unsigned slot)
{
    uintptr_t h = HASH_SEED;
    JSStackFrame* fp = cx->fp;

    while (fp->down)
        fp = fp->down;

    HashAccum(h, uintptr_t(fp->script), ORACLE_MASK);
    HashAccum(h, uintptr_t(OBJ_SHAPE(JS_GetGlobalForObject(cx, fp->scopeChain))), ORACLE_MASK);
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
    #ifdef DEBUG_dvander
    printf("MGSU: %d [%08x]: %d\n", slot, GlobalSlotHash(cx, slot),
           _globalDontDemote.get(GlobalSlotHash(cx, slot)));
    #endif
    _globalDontDemote.set(GlobalSlotHash(cx, slot));
}

/* Consult with the oracle whether we shouldn't demote a certain global variable. */
JS_REQUIRES_STACK bool
Oracle::isGlobalSlotUndemotable(JSContext* cx, unsigned slot) const
{
    #ifdef DEBUG_dvander
    printf("IGSU: %d [%08x]: %d\n", slot, GlobalSlotHash(cx, slot),
           _globalDontDemote.get(GlobalSlotHash(cx, slot)));
    #endif
    return _globalDontDemote.get(GlobalSlotHash(cx, slot));
}

/* Tell the oracle that a certain slot at a certain stack slot should not be demoted. */
JS_REQUIRES_STACK void
Oracle::markStackSlotUndemotable(JSContext* cx, unsigned slot, const void* pc)
{
    #ifdef DEBUG_dvander
    printf("MSSU: %p:%d [%08x]: %d\n", pc, slot, StackSlotHash(cx, slot, pc),
           _stackDontDemote.get(StackSlotHash(cx, slot, pc)));
    #endif
    _stackDontDemote.set(StackSlotHash(cx, slot, pc));
}

JS_REQUIRES_STACK void
Oracle::markStackSlotUndemotable(JSContext* cx, unsigned slot)
{
    markStackSlotUndemotable(cx, slot, cx->fp->regs->pc);
}

/* Consult with the oracle whether we shouldn't demote a certain slot. */
JS_REQUIRES_STACK bool
Oracle::isStackSlotUndemotable(JSContext* cx, unsigned slot, const void* pc) const
{
    #ifdef DEBUG_dvander
    printf("ISSU: %p:%d [%08x]: %d\n", pc, slot, StackSlotHash(cx, slot, pc),
           _stackDontDemote.get(StackSlotHash(cx, slot, pc)));
    #endif
    return _stackDontDemote.get(StackSlotHash(cx, slot, pc));
}

JS_REQUIRES_STACK bool
Oracle::isStackSlotUndemotable(JSContext* cx, unsigned slot) const
{
    return isStackSlotUndemotable(cx, slot, cx->fp->regs->pc);
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

void
Oracle::clearDemotability()
{
    _stackDontDemote.reset();
    _globalDontDemote.reset();
    _pcDontDemote.reset();
}

JS_REQUIRES_STACK static JS_INLINE void
MarkSlotUndemotable(JSContext* cx, TreeInfo* ti, unsigned slot)
{
    if (slot < ti->nStackTypes) {
        oracle.markStackSlotUndemotable(cx, slot);
        return;
    }

    uint16* gslots = ti->globalSlots->data();
    oracle.markGlobalSlotUndemotable(cx, gslots[slot - ti->nStackTypes]);
}

JS_REQUIRES_STACK static JS_INLINE void
MarkSlotUndemotable(JSContext* cx, TreeInfo* ti, unsigned slot, const void* pc)
{
    if (slot < ti->nStackTypes) {
        oracle.markStackSlotUndemotable(cx, slot, pc);
        return;
    }

    uint16* gslots = ti->globalSlots->data();
    oracle.markGlobalSlotUndemotable(cx, gslots[slot - ti->nStackTypes]);
}

static JS_REQUIRES_STACK inline bool
IsSlotUndemotable(JSContext* cx, TreeInfo* ti, unsigned slot, const void* ip)
{
    if (slot < ti->nStackTypes)
        return oracle.isStackSlotUndemotable(cx, slot, ip);

    uint16* gslots = ti->globalSlots->data();
    return oracle.isGlobalSlotUndemotable(cx, gslots[slot - ti->nStackTypes]);
}

static JS_REQUIRES_STACK inline bool
IsSlotUndemotable(JSContext* cx, TreeInfo* ti, unsigned slot)
{
    return IsSlotUndemotable(cx, ti, slot, cx->fp->regs->pc);
}

class FrameInfoCache
{
    struct Entry : public JSDHashEntryHdr
    {
        FrameInfo *fi;
    };

    static JSBool
    MatchFrameInfo(JSDHashTable *table, const JSDHashEntryHdr *entry, const void *key) {
        const FrameInfo* fi1 = ((const Entry*)entry)->fi;
        const FrameInfo* fi2 = (const FrameInfo*)key;
        if (memcmp(fi1, fi2, sizeof(FrameInfo)) != 0)
            return JS_FALSE;
        return memcmp(fi1->get_typemap(), fi2->get_typemap(),
                      fi1->callerHeight * sizeof(JSTraceType)) == 0;
    }

    static JSDHashNumber
    HashFrameInfo(JSDHashTable *table, const void *key) {
        FrameInfo* fi = (FrameInfo*)key;
        size_t len = sizeof(FrameInfo) + fi->callerHeight * sizeof(JSTraceType);

        JSDHashNumber h = 0;
        const unsigned char *s = (const unsigned char*)fi;
        for (size_t i = 0; i < len; i++, s++)
            h = JS_ROTATE_LEFT32(h, 4) ^ *s;
        return h;
    }

    static const JSDHashTableOps FrameCacheOps;

    JSDHashTable *table;
    VMAllocator *allocator;

  public:
    FrameInfoCache(VMAllocator *allocator) : allocator(allocator) {
        init();
    }

    ~FrameInfoCache() {
        clear();
    }

    void clear() {
        if (table) {
            JS_DHashTableDestroy(table);
            table = NULL;
        }
    }

    bool reset() {
        clear();
        return init();
    }

    bool init() {
        table = JS_NewDHashTable(&FrameCacheOps, NULL, sizeof(Entry),
                                 JS_DHASH_DEFAULT_CAPACITY(32));
        return table != NULL;
    }

    FrameInfo *memoize(const FrameInfo *fi) {
        Entry *entry = (Entry*)JS_DHashTableOperate(table, fi, JS_DHASH_ADD);
        if (!entry)
            return NULL;
        if (!entry->fi) {
            FrameInfo* n = (FrameInfo*)
                allocator->alloc(sizeof(FrameInfo) + fi->callerHeight * sizeof(JSTraceType));
            memcpy(n, fi, sizeof(FrameInfo) + fi->callerHeight * sizeof(JSTraceType));
            entry->fi = n;
        }
        return entry->fi;
    }
};

const JSDHashTableOps FrameInfoCache::FrameCacheOps =
{
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    FrameInfoCache::HashFrameInfo,
    FrameInfoCache::MatchFrameInfo,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};


struct PCHashEntry : public JSDHashEntryStub {
    size_t          count;
};

#define PC_HASH_COUNT 1024

static void
Blacklist(jsbytecode* pc)
{
    AUDIT(blacklisted);
    JS_ASSERT(*pc == JSOP_TRACE || *pc == JSOP_NOP || *pc == JSOP_CALL);
    if (*pc == JSOP_CALL) {
        JS_ASSERT(*(pc + JSOP_CALL_LENGTH) == JSOP_TRACE ||
                  *(pc + JSOP_CALL_LENGTH) == JSOP_NOP);
        *(pc + JSOP_CALL_LENGTH) = JSOP_NOP;
    } else if (*pc == JSOP_TRACE) {
        *pc = JSOP_NOP;
    }
}

static bool
IsBlacklisted(jsbytecode* pc)
{
    if (*pc == JSOP_NOP)
        return true;
    if (*pc == JSOP_CALL)
        return *(pc + JSOP_CALL_LENGTH) == JSOP_NOP;
    return false;
}

static void
Backoff(JSContext *cx, jsbytecode* pc, Fragment* tree = NULL)
{
    /* N.B. This code path cannot assume the recorder is/is not alive. */
    JSDHashTable *table = &JS_TRACE_MONITOR(cx).recordAttempts;

    if (table->ops) {
        PCHashEntry *entry = (PCHashEntry *)
            JS_DHashTableOperate(table, pc, JS_DHASH_ADD);

        if (entry) {
            if (!entry->key) {
                entry->key = pc;
                JS_ASSERT(entry->count == 0);
            }
            JS_ASSERT(JS_DHASH_ENTRY_IS_LIVE(&(entry->hdr)));
            if (entry->count++ > (BL_ATTEMPTS * MAXPEERS)) {
                entry->count = 0;
                Blacklist(pc);
                return;
            }
        }
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
    JSDHashTable *table = &JS_TRACE_MONITOR(cx).recordAttempts;
    if (table->ops) {
        PCHashEntry *entry = (PCHashEntry *)
            JS_DHashTableOperate(table, pc, JS_DHASH_LOOKUP);

        if (JS_DHASH_ENTRY_IS_FREE(&(entry->hdr)))
            return;
        JS_ASSERT(JS_DHASH_ENTRY_IS_LIVE(&(entry->hdr)));
        entry->count = 0;
    }
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
RawLookupFirstPeer(JSTraceMonitor* tm, const void *ip, JSObject* globalObj,
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
LookupLoop(JSTraceMonitor* tm, const void *ip, JSObject* globalObj,
                uint32 globalShape, uint32 argc)
{
    TreeFragment *_, **prevTreeNextp;
    RawLookupFirstPeer(tm, ip, globalObj, globalShape, argc, _, prevTreeNextp);
    return *prevTreeNextp;
}

static TreeFragment*
LookupOrAddLoop(JSTraceMonitor* tm, const void *ip, JSObject* globalObj,
                uint32 globalShape, uint32 argc)
{
    TreeFragment *firstInBucket, **prevTreeNextp;
    RawLookupFirstPeer(tm, ip, globalObj, globalShape, argc, firstInBucket, prevTreeNextp);
    if (TreeFragment *f = *prevTreeNextp)
        return f;

    verbose_only(
    uint32_t profFragID = (js_LogController.lcbits & LC_FragProfile)
                          ? (++(tm->lastFragID)) : 0;
    )
    TreeFragment* f = new (*tm->dataAlloc) TreeFragment(ip, globalObj, globalShape, argc
                                                    verbose_only(, profFragID));
    f->root = f;                /* f is the root of a new tree */
    *prevTreeNextp = f;         /* insert f at the end of the vmfragments bucket-list */
    f->next = NULL;
    f->first = f;               /* initialize peer-list at f */
    f->peer = NULL;
    return f;
}

static TreeFragment*
AddNewPeerToPeerList(JSTraceMonitor* tm, TreeFragment* peer)
{
    JS_ASSERT(peer);
    verbose_only(
    uint32_t profFragID = (js_LogController.lcbits & LC_FragProfile)
                          ? (++(tm->lastFragID)) : 0;
    )
    TreeFragment* f = new (*tm->dataAlloc) TreeFragment(peer->ip, peer->globalObj,
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

#ifdef DEBUG
static void
AssertTreeIsUnique(JSTraceMonitor* tm, TreeFragment* f, TreeInfo* ti)
{
    JS_ASSERT(f->root == f);

    /*
     * Check for duplicate entry type maps.  This is always wrong and hints at
     * trace explosion since we are trying to stabilize something without
     * properly connecting peer edges.
     */
    TreeInfo* ti_other;
    for (TreeFragment* peer = LookupLoop(tm, f->ip, f->globalObj, f->globalShape, f->argc);
         peer != NULL;
         peer = peer->peer) {
        if (!peer->code() || peer == f)
            continue;
        ti_other = peer->treeInfo;
        JS_ASSERT(ti_other);
        JS_ASSERT(!ti->typeMap.matches(ti_other->typeMap));
    }
}
#endif

static void
AttemptCompilation(JSContext *cx, JSTraceMonitor* tm, JSObject* globalObj, jsbytecode* pc,
                   uint32 argc)
{
    /* If we already permanently blacklisted the location, undo that. */
    JS_ASSERT(*pc == JSOP_NOP || *pc == JSOP_TRACE || *pc == JSOP_CALL);
    if (*pc == JSOP_NOP)
        *pc = JSOP_TRACE;
    ResetRecordingAttempts(cx, pc);

    /* Breathe new life into all peer fragments at the designated loop header. */
    TreeFragment* f = LookupLoop(tm, pc, globalObj, OBJ_SHAPE(globalObj), argc);
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

// Forward declarations.
JS_DEFINE_CALLINFO_1(static, DOUBLE, i2f,  INT32, 1, 1)
JS_DEFINE_CALLINFO_1(static, DOUBLE, u2f, UINT32, 1, 1)

static bool
isi2f(LIns* i)
{
    if (i->isop(LIR_i2f))
        return true;

    if (nanojit::AvmCore::config.soft_float &&
        i->isop(LIR_qjoin) &&
        i->oprnd1()->isop(LIR_pcall) &&
        i->oprnd2()->isop(LIR_callh)) {
        if (i->oprnd1()->callInfo() == &i2f_ci)
            return true;
    }

    return false;
}

static bool
isu2f(LIns* i)
{
    if (i->isop(LIR_u2f))
        return true;

    if (nanojit::AvmCore::config.soft_float &&
        i->isop(LIR_qjoin) &&
        i->oprnd1()->isop(LIR_pcall) &&
        i->oprnd2()->isop(LIR_callh)) {
        if (i->oprnd1()->callInfo() == &u2f_ci)
            return true;
    }

    return false;
}

static LIns*
iu2fArg(LIns* i)
{
    if (nanojit::AvmCore::config.soft_float &&
        i->isop(LIR_qjoin)) {
        return i->oprnd1()->arg(0);
    }

    return i->oprnd1();
}

static LIns*
demote(LirWriter *out, LIns* i)
{
    if (i->isCall())
        return i->callArgN(0);
    if (isi2f(i) || isu2f(i))
        return iu2fArg(i);
    if (i->isconst())
        return i;
    JS_ASSERT(i->isconstf());
    double cf = i->imm64f();
    int32_t ci = cf > 0x7fffffff ? uint32_t(cf) : int32_t(cf);
    return out->insImm(ci);
}

static bool
isPromoteInt(LIns* i)
{
    if (isi2f(i) || i->isconst())
        return true;
    if (!i->isconstf())
        return false;
    jsdouble d = i->imm64f();
    return d == jsdouble(jsint(d)) && !JSDOUBLE_IS_NEGZERO(d);
}

static bool
isPromoteUint(LIns* i)
{
    if (isu2f(i) || i->isconst())
        return true;
    if (!i->isconstf())
        return false;
    jsdouble d = i->imm64f();
    return d == jsdouble(jsuint(d)) && !JSDOUBLE_IS_NEGZERO(d);
}

static bool
isPromote(LIns* i)
{
    return isPromoteInt(i) || isPromoteUint(i);
}

static bool
IsConst(LIns* i, int32_t c)
{
    return i->isconst() && i->imm32() == c;
}

/*
 * Determine whether this operand is guaranteed to not overflow the specified
 * integer operation.
 */
static bool
IsOverflowSafe(LOpcode op, LIns* i)
{
    LIns* c;
    switch (op) {
      case LIR_add:
      case LIR_sub:
          return (i->isop(LIR_and) && ((c = i->oprnd2())->isconst()) &&
                  ((c->imm32() & 0xc0000000) == 0)) ||
                 (i->isop(LIR_rsh) && ((c = i->oprnd2())->isconst()) &&
                  ((c->imm32() > 0)));
    default:
        JS_ASSERT(op == LIR_mul);
    }
    return (i->isop(LIR_and) && ((c = i->oprnd2())->isconst()) &&
            ((c->imm32() & 0xffff0000) == 0)) ||
           (i->isop(LIR_ush) && ((c = i->oprnd2())->isconst()) &&
            ((c->imm32() >= 16)));
}

/* soft float support */

static jsdouble FASTCALL
fneg(jsdouble x)
{
    return -x;
}
JS_DEFINE_CALLINFO_1(static, DOUBLE, fneg, DOUBLE, 1, 1)

static jsdouble FASTCALL
i2f(int32 i)
{
    return i;
}

static jsdouble FASTCALL
u2f(jsuint u)
{
    return u;
}

static int32 FASTCALL
fcmpeq(jsdouble x, jsdouble y)
{
    return x==y;
}
JS_DEFINE_CALLINFO_2(static, INT32, fcmpeq, DOUBLE, DOUBLE, 1, 1)

static int32 FASTCALL
fcmplt(jsdouble x, jsdouble y)
{
    return x < y;
}
JS_DEFINE_CALLINFO_2(static, INT32, fcmplt, DOUBLE, DOUBLE, 1, 1)

static int32 FASTCALL
fcmple(jsdouble x, jsdouble y)
{
    return x <= y;
}
JS_DEFINE_CALLINFO_2(static, INT32, fcmple, DOUBLE, DOUBLE, 1, 1)

static int32 FASTCALL
fcmpgt(jsdouble x, jsdouble y)
{
    return x > y;
}
JS_DEFINE_CALLINFO_2(static, INT32, fcmpgt, DOUBLE, DOUBLE, 1, 1)

static int32 FASTCALL
fcmpge(jsdouble x, jsdouble y)
{
    return x >= y;
}
JS_DEFINE_CALLINFO_2(static, INT32, fcmpge, DOUBLE, DOUBLE, 1, 1)

static jsdouble FASTCALL
fmul(jsdouble x, jsdouble y)
{
    return x * y;
}
JS_DEFINE_CALLINFO_2(static, DOUBLE, fmul, DOUBLE, DOUBLE, 1, 1)

static jsdouble FASTCALL
fadd(jsdouble x, jsdouble y)
{
    return x + y;
}
JS_DEFINE_CALLINFO_2(static, DOUBLE, fadd, DOUBLE, DOUBLE, 1, 1)

static jsdouble FASTCALL
fdiv(jsdouble x, jsdouble y)
{
    return x / y;
}
JS_DEFINE_CALLINFO_2(static, DOUBLE, fdiv, DOUBLE, DOUBLE, 1, 1)

static jsdouble FASTCALL
fsub(jsdouble x, jsdouble y)
{
    return x - y;
}
JS_DEFINE_CALLINFO_2(static, DOUBLE, fsub, DOUBLE, DOUBLE, 1, 1)

// replace fpu ops with function calls
class SoftFloatFilter: public LirWriter
{
public:
    SoftFloatFilter(LirWriter *out) : LirWriter(out)
    {}

    LIns *hi(LIns *q) {
        return ins1(LIR_qhi, q);
    }
    LIns *lo(LIns *q) {
        return ins1(LIR_qlo, q);
    }

    LIns *split(LIns *a) {
        if (a->isQuad() && !a->isop(LIR_qjoin)) {
            // all quad-sized args must be qjoin's for soft-float
            a = ins2(LIR_qjoin, lo(a), hi(a));
        }
        return a;
    }

    LIns *split(const CallInfo *call, LInsp args[]) {
        LIns *lo = out->insCall(call, args);
        LIns *hi = out->ins1(LIR_callh, lo);
        return out->ins2(LIR_qjoin, lo, hi);
    }

    LIns *fcall1(const CallInfo *call, LIns *a) {
        LIns *args[] = { split(a) };
        return split(call, args);
    }

    LIns *fcall2(const CallInfo *call, LIns *a, LIns *b) {
        LIns *args[] = { split(b), split(a) };
        return split(call, args);
    }

    LIns *fcmp(const CallInfo *call, LIns *a, LIns *b) {
        LIns *args[] = { split(b), split(a) };
        return out->ins2(LIR_eq, out->insCall(call, args), out->insImm(1));
    }

    LIns *ins1(LOpcode op, LIns *a) {
        switch (op) {
        case LIR_i2f:
            return fcall1(&i2f_ci, a);
        case LIR_u2f:
            return fcall1(&u2f_ci, a);
        case LIR_fneg:
            return fcall1(&fneg_ci, a);
        case LIR_fret:
            return out->ins1(op, split(a));
        default:
            return out->ins1(op, a);
        }
    }

    LIns *ins2(LOpcode op, LIns *a, LIns *b) {
        switch (op) {
        case LIR_fadd:
            return fcall2(&fadd_ci, a, b);
        case LIR_fsub:
            return fcall2(&fsub_ci, a, b);
        case LIR_fmul:
            return fcall2(&fmul_ci, a, b);
        case LIR_fdiv:
            return fcall2(&fdiv_ci, a, b);
        case LIR_feq:
            return fcmp(&fcmpeq_ci, a, b);
        case LIR_flt:
            return fcmp(&fcmplt_ci, a, b);
        case LIR_fgt:
            return fcmp(&fcmpgt_ci, a, b);
        case LIR_fle:
            return fcmp(&fcmple_ci, a, b);
        case LIR_fge:
            return fcmp(&fcmpge_ci, a, b);
        default:
            ;
        }
        return out->ins2(op, a, b);
    }

    LIns *insCall(const CallInfo *ci, LInsp args[]) {
        uint32_t argt = ci->_argtypes;

        for (uint32_t i = 0, argsizes = argt >> ARGSIZE_SHIFT; argsizes != 0; i++, argsizes >>= ARGSIZE_SHIFT)
            args[i] = split(args[i]);

        if ((argt & ARGSIZE_MASK_ANY) == ARGSIZE_F) {
            // this function returns a double as two 32bit values, so replace
            // call with qjoin(qhi(call), call)
            return split(ci, args);
        } else {
            return out->insCall(ci, args);
        }
    }
};

class FuncFilter: public LirWriter
{
public:
    FuncFilter(LirWriter* out):
        LirWriter(out)
    {
    }

    LIns* ins2(LOpcode v, LIns* s0, LIns* s1)
    {
        if (s0 == s1 && v == LIR_feq) {
            if (isPromote(s0)) {
                // double(int) and double(uint) cannot be nan
                return insImm(1);
            }
            if (s0->isop(LIR_fmul) || s0->isop(LIR_fsub) || s0->isop(LIR_fadd)) {
                LIns* lhs = s0->oprnd1();
                LIns* rhs = s0->oprnd2();
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
                   s0->isop(LIR_lsh) && IsConst(s0->oprnd2(), 16) &&
                   s1->isop(LIR_and) && IsConst(s1->oprnd2(), 0xffff)) {
            LIns* msw = s0->oprnd1();
            LIns* lsw = s1->oprnd1();
            LIns* x;
            LIns* y;
            if (lsw->isop(LIR_add) &&
                lsw->oprnd1()->isop(LIR_and) &&
                lsw->oprnd2()->isop(LIR_and) &&
                IsConst(lsw->oprnd1()->oprnd2(), 0xffff) &&
                IsConst(lsw->oprnd2()->oprnd2(), 0xffff) &&
                msw->isop(LIR_add) &&
                msw->oprnd1()->isop(LIR_add) &&
                msw->oprnd2()->isop(LIR_rsh) &&
                msw->oprnd1()->oprnd1()->isop(LIR_rsh) &&
                msw->oprnd1()->oprnd2()->isop(LIR_rsh) &&
                IsConst(msw->oprnd2()->oprnd2(), 16) &&
                IsConst(msw->oprnd1()->oprnd1()->oprnd2(), 16) &&
                IsConst(msw->oprnd1()->oprnd2()->oprnd2(), 16) &&
                (x = lsw->oprnd1()->oprnd1()) == msw->oprnd1()->oprnd1()->oprnd1() &&
                (y = lsw->oprnd2()->oprnd1()) == msw->oprnd1()->oprnd2()->oprnd1() &&
                lsw == msw->oprnd2()->oprnd1()) {
                return out->ins2(LIR_add, x, y);
            }
        }

        return out->ins2(v, s0, s1);
    }

    LIns* insCall(const CallInfo *ci, LIns* args[])
    {
        if (ci == &js_DoubleToUint32_ci) {
            LIns* s0 = args[0];
            if (s0->isconstf())
                return out->insImm(js_DoubleToECMAUint32(s0->imm64f()));
            if (isi2f(s0) || isu2f(s0))
                return iu2fArg(s0);
        } else if (ci == &js_DoubleToInt32_ci) {
            LIns* s0 = args[0];
            if (s0->isconstf())
                return out->insImm(js_DoubleToECMAInt32(s0->imm64f()));
            if (s0->isop(LIR_fadd) || s0->isop(LIR_fsub)) {
                LIns* lhs = s0->oprnd1();
                LIns* rhs = s0->oprnd2();
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
                    LIns* args2[] = { s0->callArgN(0) };
                    return out->insCall(&js_UnboxInt32_ci, args2);
                } else if (ci2 == &js_StringToNumber_ci) {
                    // callArgN's ordering is that as seen by the builtin, not as stored in
                    // args here. True story!
                    LIns* args2[] = { s0->callArgN(1), s0->callArgN(0) };
                    return out->insCall(&js_StringToInt32_ci, args2);
                } else if (ci2 == &js_String_p_charCodeAt0_ci) {
                    // Use a fast path builtin for a charCodeAt that converts to an int right away.
                    LIns* args2[] = { s0->callArgN(0) };
                    return out->insCall(&js_String_p_charCodeAt0_int_ci, args2);
                } else if (ci2 == &js_String_p_charCodeAt_ci) {
                    LIns* idx = s0->callArgN(1);
                    // If the index is not already an integer, force it to be an integer.
                    idx = isPromote(idx)
                        ? demote(out, idx)
                        : out->insCall(&js_DoubleToInt32_ci, &idx);
                    LIns* args2[] = { idx, s0->callArgN(0) };
                    return out->insCall(&js_String_p_charCodeAt_int_ci, args2);
                }
            }
        } else if (ci == &js_BoxDouble_ci) {
            LIns* s0 = args[0];
            JS_ASSERT(s0->isQuad());
            if (isPromoteInt(s0)) {
                LIns* args2[] = { demote(out, s0), args[1] };
                return out->insCall(&js_BoxInt32_ci, args2);
            }
            if (s0->isCall() && s0->callInfo() == &js_UnboxDouble_ci)
                return s0->callArgN(0);
        }
        return out->insCall(ci, args);
    }
};

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
VisitFrameSlots(Visitor &visitor, unsigned depth, JSStackFrame *fp,
                JSStackFrame *up)
{
    if (depth > 0 && !VisitFrameSlots(visitor, depth-1, fp->down, fp))
        return false;

    if (fp->argv) {
        if (depth == 0) {
            visitor.setStackSlotKind("args");
            if (!visitor.visitStackSlots(&fp->argv[-2], argSlots(fp) + 2, fp))
                return false;
        }
        visitor.setStackSlotKind("arguments");
        if (!visitor.visitStackSlots(&fp->argsobj, 1, fp))
            return false;
        visitor.setStackSlotKind("var");
        if (!visitor.visitStackSlots(fp->slots, fp->script->nfixed, fp))
            return false;
    }
    visitor.setStackSlotKind("stack");
    JS_ASSERT(fp->regs->sp >= StackBase(fp));
    if (!visitor.visitStackSlots(StackBase(fp),
                                 size_t(fp->regs->sp - StackBase(fp)),
                                 fp)) {
        return false;
    }
    if (up) {
        int missing = up->fun->nargs - up->argc;
        if (missing > 0) {
            visitor.setStackSlotKind("missing");
            if (!visitor.visitStackSlots(fp->regs->sp, size_t(missing), fp))
                return false;
        }
    }
    return true;
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
VisitStackSlots(Visitor &visitor, JSContext *cx, unsigned callDepth)
{
    return VisitFrameSlots(visitor, callDepth, cx->fp, NULL);
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitGlobalSlots(Visitor &visitor, JSContext *cx, JSObject *globalObj,
                 unsigned ngslots, uint16 *gslots)
{
    for (unsigned n = 0; n < ngslots; ++n) {
        unsigned slot = gslots[n];
        visitor.visitGlobalSlot(&STOBJ_GET_SLOT(globalObj, slot), n, slot);
    }
}

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitGlobalSlots(Visitor &visitor, JSContext *cx, TreeInfo *ti)
{
    JSObject* globalObj = ti->globalObj();
    SlotList& gslots = *ti->globalSlots;
    VisitGlobalSlots(visitor, cx, globalObj, gslots.length(), gslots.data());
}

class AdjustCallerTypeVisitor;

template <typename Visitor>
static JS_REQUIRES_STACK JS_ALWAYS_INLINE void
VisitGlobalSlots(Visitor &visitor, JSContext *cx, SlotList &gslots)
{
    VisitGlobalSlots(visitor, cx, JS_GetGlobalForObject(cx, cx->fp->scopeChain),
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
    VisitSlots(visitor, cx, JS_GetGlobalForObject(cx, cx->fp->scopeChain),
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
    VisitSlots(visitor, cx, JS_GetGlobalForObject(cx, cx->fp->scopeChain),
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
    jsval* mStop;
public:
    JS_ALWAYS_INLINE CountSlotsVisitor(jsval* stop = NULL) :
        mCount(0),
        mDone(false),
        mStop(stop)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        if (mDone)
            return false;
        if (mStop && size_t(mStop - vp) < count) {
            mCount += size_t(mStop - vp);
            mDone = true;
            return false;
        }
        mCount += count;
        return true;
    }

    JS_ALWAYS_INLINE unsigned count() {
        return mCount;
    }

    JS_ALWAYS_INLINE bool stopped() {
        return mDone;
    }
};

/*
 * Calculate the total number of native frame slots we need from this frame all
 * the way back to the entry frame, including the current stack usage.
 */
JS_REQUIRES_STACK unsigned
NativeStackSlots(JSContext *cx, unsigned callDepth)
{
    JSStackFrame* fp = cx->fp;
    unsigned slots = 0;
    unsigned depth = callDepth;
    for (;;) {
        /*
         * Duplicate native stack layout computation: see VisitFrameSlots
         * header comment.
         */
        unsigned operands = fp->regs->sp - StackBase(fp);
        slots += operands;
        if (fp->argv)
            slots += fp->script->nfixed + 1 /*argsobj*/;
        if (depth-- == 0) {
            if (fp->argv)
                slots += 2/*callee,this*/ + argSlots(fp);
#ifdef DEBUG
            CountSlotsVisitor visitor;
            VisitStackSlots(visitor, cx, callDepth);
            JS_ASSERT(visitor.count() == slots && !visitor.stopped());
#endif
            return slots;
        }
        JSStackFrame* fp2 = fp;
        fp = fp->down;
        int missing = fp2->fun->nargs - fp2->argc;
        if (missing > 0)
            slots += missing;
    }
    JS_NOT_REACHED("NativeStackSlots");
}

class CaptureTypesVisitor : public SlotVisitorBase
{
    JSContext* mCx;
    JSTraceType* mTypeMap;
    JSTraceType* mPtr;

public:
    JS_ALWAYS_INLINE CaptureTypesVisitor(JSContext* cx, JSTraceType* typeMap) :
        mCx(cx),
        mTypeMap(typeMap),
        mPtr(typeMap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
            JSTraceType type = getCoercedType(*vp);
            if (type == TT_INT32 &&
                oracle.isGlobalSlotUndemotable(mCx, slot))
                type = TT_DOUBLE;
            JS_ASSERT(type != TT_JSVAL);
            debug_only_printf(LC_TMTracer,
                              "capture type global%d: %d=%c\n",
                              n, type, typeChar[type]);
            *mPtr++ = type;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, int count, JSStackFrame* fp) {
        for (int i = 0; i < count; ++i) {
            JSTraceType type = getCoercedType(vp[i]);
            if (type == TT_INT32 &&
                oracle.isStackSlotUndemotable(mCx, length()))
                type = TT_DOUBLE;
            JS_ASSERT(type != TT_JSVAL);
            debug_only_printf(LC_TMTracer,
                              "capture type %s%d: %d=%c\n",
                              stackSlotKind(), i, type, typeChar[type]);
            *mPtr++ = type;
        }
        return true;
    }

    JS_ALWAYS_INLINE uintptr_t length() {
        return mPtr - mTypeMap;
    }
};

/*
 * Capture the type map for the selected slots of the global object and currently pending
 * stack frames.
 */
JS_REQUIRES_STACK void
TypeMap::captureTypes(JSContext* cx, JSObject* globalObj, SlotList& slots, unsigned callDepth)
{
    setLength(NativeStackSlots(cx, callDepth) + slots.length());
    CaptureTypesVisitor visitor(cx, data());
    VisitSlots(visitor, cx, globalObj, callDepth, slots);
    JS_ASSERT(visitor.length() == length());
}

JS_REQUIRES_STACK void
TypeMap::captureMissingGlobalTypes(JSContext* cx, JSObject* globalObj, SlotList& slots, unsigned stackSlots)
{
    unsigned oldSlots = length() - stackSlots;
    int diff = slots.length() - oldSlots;
    JS_ASSERT(diff >= 0);
    setLength(length() + diff);
    CaptureTypesVisitor visitor(cx, data() + stackSlots + oldSlots);
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
TypeMap::fromRaw(JSTraceType* other, unsigned numSlots)
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
MergeTypeMaps(JSTraceType** partial, unsigned* plength, JSTraceType* complete, unsigned clength, JSTraceType* mem)
{
    unsigned l = *plength;
    JS_ASSERT(l < clength);
    memcpy(mem, *partial, l * sizeof(JSTraceType));
    memcpy(mem + l, complete + l, (clength - l) * sizeof(JSTraceType));
    *partial = mem;
    *plength = clength;
}

/* Specializes a tree to any missing globals, including any dependent trees. */
static JS_REQUIRES_STACK void
SpecializeTreesToMissingGlobals(JSContext* cx, JSObject* globalObj, TreeInfo* root)
{
    TreeInfo* ti = root;

    ti->typeMap.captureMissingGlobalTypes(cx, globalObj, *ti->globalSlots, ti->nStackTypes);
    JS_ASSERT(ti->globalSlots->length() == ti->typeMap.length() - ti->nStackTypes);

    for (unsigned i = 0; i < root->dependentTrees.length(); i++) {
        ti = root->dependentTrees[i]->treeInfo;

        /* ti can be NULL if we hit the recording tree in emitTreeCall; this is harmless. */
        if (ti && ti->nGlobalTypes() < ti->globalSlots->length())
            SpecializeTreesToMissingGlobals(cx, globalObj, ti);
    }
    for (unsigned i = 0; i < root->linkedTrees.length(); i++) {
        ti = root->linkedTrees[i]->treeInfo;
        if (ti && ti->nGlobalTypes() < ti->globalSlots->length())
            SpecializeTreesToMissingGlobals(cx, globalObj, ti);
    }
}

static JS_REQUIRES_STACK void
ResetJITImpl(JSContext* cx);

#ifdef MOZ_TRACEVIS
static JS_INLINE JS_REQUIRES_STACK void
ResetJIT(JSContext* cx, TraceVisFlushReason r)
{
    js_LogTraceVisEvent(cx, S_RESET, r);
    ResetJITImpl(cx);
}
#else
#define ResetJIT(cx, r) ResetJITImpl(cx)
#endif

static void
TrashTree(JSContext* cx, TreeFragment* f);

template <class T>
static T&
InitConst(const T &t)
{
    return const_cast<T &>(t);
}

JS_REQUIRES_STACK
TraceRecorder::TraceRecorder(JSContext* cx, VMSideExit* anchor, VMFragment* fragment,
                             TreeInfo* ti, unsigned stackSlots, unsigned ngslots, JSTraceType* typeMap,
                             VMSideExit* innermost, jsbytecode* outer, uint32 outerArgc,
                             RecordReason recordReason)
  : cx(cx),
    traceMonitor(&JS_TRACE_MONITOR(cx)),
    fragment(fragment),
    treeInfo(ti),
    recordReason(recordReason),
    globalObj(ti->globalObj()),
    outer(outer),
    outerArgc(outerArgc),
    lexicalBlock(cx->fp->blockChain),
    anchor(anchor),
    lir(NULL),
    cx_ins(NULL),
    eos_ins(NULL),
    eor_ins(NULL),
    loopLabel(NULL),
    lirbuf(traceMonitor->lirbuf),
    mark(*traceMonitor->traceAlloc),
    numSideExitsBefore(treeInfo->sideExits.length()),
    tracker(),
    nativeFrameTracker(),
    global_dslots(NULL),
    callDepth(anchor ? anchor->calldepth : 0),
    atoms(FrameAtomBase(cx, cx->fp)),
    cfgMerges(&tempAlloc()),
    trashSelf(false),
    whichTreesToTrash(&tempAlloc()),
    rval_ins(NULL),
    native_rval_ins(NULL),
    newobj_ins(NULL),
    pendingSpecializedNative(NULL),
    pendingUnboxSlot(NULL),
    pendingGuardCondition(NULL),
    pendingLoop(true),
    generatedSpecializedNative(),
    tempTypeMap(cx)
{
    JS_ASSERT(globalObj == JS_GetGlobalForObject(cx, cx->fp->scopeChain));
    JS_ASSERT(cx->fp->regs->pc == (jsbytecode*)fragment->ip);
    JS_ASSERT(fragment->root == treeInfo->rootFragment);
    JS_ASSERT_IF(fragment->root == fragment, !fragment->root->treeInfo);
    JS_ASSERT(ti);

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

    guardedShapeTable.ops = NULL;

#ifdef JS_JIT_SPEW
    debug_only_print0(LC_TMMinimal, "\n");
    debug_only_printf(LC_TMMinimal, "Recording starting from %s:%u@%u (FragID=%06u)\n",
                      ti->treeFileName, ti->treeLineNumber, ti->treePCOffset,
                      fragment->profFragID);

    debug_only_printf(LC_TMTracer, "globalObj=%p, shape=%d\n",
                      (void*)this->globalObj, OBJ_SHAPE(this->globalObj));
    debug_only_printf(LC_TMTreeVis, "TREEVIS RECORD FRAG=%p ANCHOR=%p\n", (void*)fragment,
                      (void*)anchor);
#endif

    nanojit::LirWriter*& lir = InitConst(this->lir);
    lir = new (tempAlloc()) LirBufWriter(lirbuf);
#ifdef DEBUG
    lir = new (tempAlloc()) SanityFilter(lir);
#endif
    debug_only_stmt(
        if (js_LogController.lcbits & LC_TMRecorder) {
           lir = new (tempAlloc()) VerboseWriter(tempAlloc(), lir, lirbuf->names,
                                               &js_LogController);
        }
    )
    if (nanojit::AvmCore::config.soft_float)
        lir = new (tempAlloc()) SoftFloatFilter(lir);
    lir = new (tempAlloc()) CseFilter(lir, tempAlloc());
    lir = new (tempAlloc()) ExprFilter(lir);
    lir = new (tempAlloc()) FuncFilter(lir);
#ifdef DEBUG
    lir = new (tempAlloc()) SanityFilter(lir);
#endif
    lir->ins0(LIR_start);

    for (int i = 0; i < NumSavedRegs; ++i)
        lir->insParam(i, 1);
#ifdef DEBUG
    for (int i = 0; i < NumSavedRegs; ++i)
        addName(lirbuf->savedRegs[i], regNames[Assembler::savedRegs[i]]);
#endif

    lirbuf->state = addName(lir->insParam(0, 0), "state");

    if (fragment == fragment->root)
        InitConst(loopLabel) = lir->ins0(LIR_label);

    // if profiling, drop a label, so the assembler knows to put a
    // frag-entry-counter increment at this point.  If there's a
    // loopLabel, use that; else we'll have to make a dummy label
    // especially for this purpose.
    verbose_only( if (js_LogController.lcbits & LC_FragProfile) {
        LIns* entryLabel = NULL;
        if (fragment == fragment->root) {
            entryLabel = loopLabel;
        } else {
            entryLabel = lir->ins0(LIR_label);
        }
        NanoAssert(entryLabel);
        NanoAssert(!fragment->loopLabel);
        fragment->loopLabel = entryLabel;
    })

    lirbuf->sp = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, sp)), "sp");
    lirbuf->rp = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, rp)), "rp");
    InitConst(cx_ins) = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, cx)), "cx");
    InitConst(eos_ins) = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, eos)), "eos");
    InitConst(eor_ins) = addName(lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, eor)), "eor");

    /* If we came from exit, we might not have enough global types. */
    if (ti->globalSlots->length() > ti->nGlobalTypes())
        SpecializeTreesToMissingGlobals(cx, globalObj, ti);

    /* read into registers all values on the stack and all globals we know so far */
    import(treeInfo, lirbuf->sp, stackSlots, ngslots, callDepth, typeMap);

    /* Finish handling RECURSIVE_SLURP_FAIL_EXIT in startRecorder. */
    if (anchor && anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT)
        return;

    if (fragment == fragment->root) {
        /*
         * We poll the operation callback request flag. It is updated asynchronously whenever
         * the callback is to be invoked.
         */
        LIns* x = lir->insLoad(LIR_ld, cx_ins, offsetof(JSContext, operationCallbackFlag));
        guard(true, lir->ins_eq0(x), snapshot(TIMEOUT_EXIT));
    }

    /*
     * If we are attached to a tree call guard, make sure the guard the inner
     * tree exited from is what we expect it to be.
     */
    if (anchor && anchor->exitType == NESTED_EXIT) {
        LIns* nested_ins = addName(lir->insLoad(LIR_ldp, lirbuf->state,
                                                offsetof(InterpState, outermostTreeExitGuard)),
                                                "outermostTreeExitGuard");
        guard(true, lir->ins2(LIR_peq, nested_ins, INS_CONSTPTR(innermost)), NESTED_EXIT);
    }
}

TraceRecorder::~TraceRecorder()
{
    /* Should already have been adjusted by callers before calling delete. */
    JS_ASSERT(traceMonitor->recorder != this);
    JS_ASSERT(fragment->root == treeInfo->rootFragment);

    if (trashSelf)
        TrashTree(cx, fragment->root);

    for (unsigned int i = 0; i < whichTreesToTrash.length(); i++)
        TrashTree(cx, whichTreesToTrash[i]);

    /* Purge the tempAlloc used during recording. */
    tempAlloc().reset();
    traceMonitor->lirbuf->clear();

    forgetGuardedShapes();
}

inline bool
JSTraceMonitor::outOfMemory() const
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
    JS_ASSERT_IF(fragment == fragment->root, fragment->toTreeFragment()->treeInfo);

    AUDIT(traceCompleted);
    mark.commit();

    /* Grab local copies of members needed after |delete this|. */
    JSContext* localcx = cx;
    JSTraceMonitor* localtm = traceMonitor;

    localtm->recorder = NULL;
    delete this;

    /* Catch OOM that occurred during recording. */
    if (localtm->outOfMemory() || js_OverfullJITCache(localtm)) {
        ResetJIT(localcx, FR_OOM);
        return ARECORD_ABORTED;
    }
    return ARECORD_COMPLETED;
}

/* This function aborts a recorder and any pending outer recorders. */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::finishAbort(const char* reason)
{
    JS_ASSERT(traceMonitor->recorder == this);
    JS_ASSERT(!fragment->code());
    JS_ASSERT_IF(fragment == fragment->root, !fragment->toTreeFragment()->treeInfo);

    AUDIT(recorderAborted);
#ifdef DEBUG
    debug_only_printf(LC_TMAbort,
                      "Abort recording of tree %s:%d@%d at %s:%d@%d: %s.\n",
                      treeInfo->treeFileName,
                      treeInfo->treeLineNumber,
                      treeInfo->treePCOffset,
                      cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp),
                      FramePCOffset(cx->fp),
                      reason);
#endif
    Backoff(cx, (jsbytecode*) fragment->root->ip, fragment->root);

    /*
     * If this is the primary trace and we didn't succeed compiling, trash the
     * TreeInfo object. Otherwise, remove the VMSideExits we added while
     * recording, which are about to be invalid.
     *
     * BIG FAT WARNING: resetting the length is only a valid strategy as long as
     * there may be only one recorder active for a single TreeInfo at a time.
     * Otherwise, we may be throwing away another recorder's valid side exits.
     */
    if (fragment->root == fragment) {
        TrashTree(cx, fragment->toTreeFragment());
    } else {
        JS_ASSERT(numSideExitsBefore <= fragment->root->treeInfo->sideExits.length());
        fragment->root->treeInfo->sideExits.setLength(numSideExitsBefore);
    }

    /* Grab local copies of members needed after |delete this|. */
    JSContext* localcx = cx;
    JSTraceMonitor* localtm = traceMonitor;

    localtm->recorder = NULL;
    delete this;
    if (localtm->outOfMemory() || js_OverfullJITCache(localtm))
        ResetJIT(localcx, FR_OOM);
    return ARECORD_ABORTED;
}

/* Add debug information to a LIR instruction as we emit it. */
inline LIns*
TraceRecorder::addName(LIns* ins, const char* name)
{
#ifdef JS_JIT_SPEW
    /*
     * We'll only ask for verbose Nanojit when .lcbits > 0, so there's no point
     * in adding names otherwise.
     */
    if (js_LogController.lcbits > 0)
        lirbuf->names->addName(ins, name);
#endif
    return ins;
}

inline LIns*
TraceRecorder::insImmVal(jsval val)
{
    if (JSVAL_IS_TRACEABLE(val))
        treeInfo->gcthings.addUnique(val);
    return lir->insImmWord(val);
}

inline LIns*
TraceRecorder::insImmObj(JSObject* obj)
{
    treeInfo->gcthings.addUnique(OBJECT_TO_JSVAL(obj));
    return lir->insImmPtr((void*)obj);
}

inline LIns*
TraceRecorder::insImmFun(JSFunction* fun)
{
    treeInfo->gcthings.addUnique(OBJECT_TO_JSVAL(FUN_OBJECT(fun)));
    return lir->insImmPtr((void*)fun);
}

inline LIns*
TraceRecorder::insImmStr(JSString* str)
{
    treeInfo->gcthings.addUnique(STRING_TO_JSVAL(str));
    return lir->insImmPtr((void*)str);
}

inline LIns*
TraceRecorder::insImmSprop(JSScopeProperty* sprop)
{
    treeInfo->sprops.addUnique(sprop);
    return lir->insImmPtr((void*)sprop);
}

inline LIns*
TraceRecorder::p2i(nanojit::LIns* ins)
{
#ifdef NANOJIT_64BIT
    return lir->ins1(LIR_qlo, ins);
#else
    return ins;
#endif
}

/* Determine the offset in the native global frame for a jsval we track. */
ptrdiff_t
TraceRecorder::nativeGlobalOffset(jsval* p) const
{
    JS_ASSERT(isGlobal(p));
    if (size_t(p - globalObj->fslots) < JS_INITIAL_NSLOTS)
        return size_t(p - globalObj->fslots) * sizeof(double);
    return ((p - globalObj->dslots) + JS_INITIAL_NSLOTS) * sizeof(double);
}

/* Determine whether a value is a global stack slot. */
bool
TraceRecorder::isGlobal(jsval* p) const
{
    return ((size_t(p - globalObj->fslots) < JS_INITIAL_NSLOTS) ||
            (size_t(p - globalObj->dslots) < (STOBJ_NSLOTS(globalObj) - JS_INITIAL_NSLOTS)));
}

/*
 * Return the offset in the native stack for the given jsval. More formally,
 * |p| must be the address of a jsval that is represented in the native stack
 * area. The return value is the offset, from InterpState::stackBase, in bytes,
 * where the native representation of |*p| is stored. To get the offset
 * relative to InterpState::sp, subtract TreeInfo::nativeStackBase.
 */
JS_REQUIRES_STACK ptrdiff_t
TraceRecorder::nativeStackOffset(jsval* p) const
{
    CountSlotsVisitor visitor(p);
    VisitStackSlots(visitor, cx, callDepth);
    size_t offset = visitor.count() * sizeof(double);

    /*
     * If it's not in a pending frame, it must be on the stack of the current
     * frame above sp but below fp->slots + script->nslots.
     */
    if (!visitor.stopped()) {
        JS_ASSERT(size_t(p - cx->fp->slots) < cx->fp->script->nslots);
        offset += size_t(p - cx->fp->regs->sp) * sizeof(double);
    }
    return offset;
}
/*
 * Return the offset, from InterpState:sp, for the given jsval. Shorthand for:
 *  -TreeInfo::nativeStackBase + nativeStackOffset(p).
 */
inline JS_REQUIRES_STACK ptrdiff_t
TraceRecorder::nativespOffset(jsval* p) const
{
    return -treeInfo->nativeStackBase + nativeStackOffset(p);
}

/* Track the maximum number of native frame slots we need during execution. */
inline void
TraceRecorder::trackNativeStackUse(unsigned slots)
{
    if (slots > treeInfo->maxNativeStackSlots)
        treeInfo->maxNativeStackSlots = slots;
}

/*
 * Unbox a jsval into a slot. Slots are wide enough to hold double values
 * directly (instead of storing a pointer to them). We assert instead of
 * type checking. The caller must ensure the types are compatible.
 */
static void
ValueToNative(JSContext* cx, jsval v, JSTraceType type, double* slot)
{
    uint8_t tag = JSVAL_TAG(v);
    switch (type) {
      case TT_OBJECT:
        JS_ASSERT(tag == JSVAL_OBJECT);
        JS_ASSERT(!JSVAL_IS_NULL(v) && !HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(v)));
        *(JSObject**)slot = JSVAL_TO_OBJECT(v);
        debug_only_printf(LC_TMTracer,
                          "object<%p:%s> ", (void*)JSVAL_TO_OBJECT(v),
                          JSVAL_IS_NULL(v)
                          ? "null"
                          : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);
        return;

      case TT_INT32:
        jsint i;
        if (JSVAL_IS_INT(v))
            *(jsint*)slot = JSVAL_TO_INT(v);
        else if (tag == JSVAL_DOUBLE && JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(v), i))
            *(jsint*)slot = i;
        else
            JS_ASSERT(JSVAL_IS_INT(v));
        debug_only_printf(LC_TMTracer, "int<%d> ", *(jsint*)slot);
        return;

      case TT_DOUBLE:
        jsdouble d;
        if (JSVAL_IS_INT(v))
            d = JSVAL_TO_INT(v);
        else
            d = *JSVAL_TO_DOUBLE(v);
        JS_ASSERT(JSVAL_IS_INT(v) || JSVAL_IS_DOUBLE(v));
        *(jsdouble*)slot = d;
        debug_only_printf(LC_TMTracer, "double<%g> ", d);
        return;

      case TT_JSVAL:
        JS_NOT_REACHED("found jsval type in an entry type map");
        return;

      case TT_STRING:
        JS_ASSERT(tag == JSVAL_STRING);
        *(JSString**)slot = JSVAL_TO_STRING(v);
        debug_only_printf(LC_TMTracer, "string<%p> ", (void*)(*(JSString**)slot));
        return;

      case TT_NULL:
        JS_ASSERT(tag == JSVAL_OBJECT);
        *(JSObject**)slot = NULL;
        debug_only_print0(LC_TMTracer, "null ");
        return;

      case TT_PSEUDOBOOLEAN:
        /* Watch out for pseudo-booleans. */
        JS_ASSERT(tag == JSVAL_SPECIAL);
        *(JSBool*)slot = JSVAL_TO_SPECIAL(v);
        debug_only_printf(LC_TMTracer, "pseudoboolean<%d> ", *(JSBool*)slot);
        return;

      case TT_FUNCTION: {
        JS_ASSERT(tag == JSVAL_OBJECT);
        JSObject* obj = JSVAL_TO_OBJECT(v);
        *(JSObject**)slot = obj;
#ifdef DEBUG
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, obj);
        debug_only_printf(LC_TMTracer,
                          "function<%p:%s> ", (void*) obj,
                          fun->atom
                          ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
                          : "unnamed");
#endif
        return;
      }
    }

    JS_NOT_REACHED("unexpected type");
}

void
JSTraceMonitor::flush()
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
                    js_FragProfiling_FragFinalizer(p, this);
            }
        }
    )

    verbose_only(
        for (Seq<Fragment*>* f = branches; f; f = f->tail)
            js_FragProfiling_FragFinalizer(f->head, this);
    )

    frameCache->reset();
    dataAlloc->reset();
    traceAlloc->reset();
    codeAlloc->reset();
    tempAlloc->reset();
    reTempAlloc->reset();

    Allocator& alloc = *dataAlloc;

    for (size_t i = 0; i < MONITOR_N_GLOBAL_STATES; ++i) {
        globalStates[i].globalShape = -1;
        globalStates[i].globalSlots = new (alloc) SlotList(&alloc);
    }

    assembler = new (alloc) Assembler(*codeAlloc, alloc, alloc, core, &js_LogController);
    lirbuf = new (alloc) LirBuffer(*tempAlloc);
    reLirBuf = new (alloc) LirBuffer(*reTempAlloc);
    verbose_only( branches = NULL; )

#ifdef DEBUG
    labels = new (alloc) LabelMap(alloc, &js_LogController);
    reLirBuf->names =
    lirbuf->names = new (alloc) LirNameMap(alloc, labels);
#endif

    memset(&vmfragments[0], 0, FRAGMENT_TABLE_SIZE * sizeof(TreeFragment*));
    reFragments = new (alloc) REHashMap(alloc);

    needFlush = JS_FALSE;
}

static inline void
MarkTreeInfo(JSTracer* trc, TreeInfo *ti)
{
    jsval* vp = ti->gcthings.data();
    unsigned len = ti->gcthings.length();
    while (len--) {
        jsval v = *vp++;
        JS_SET_TRACING_NAME(trc, "jitgcthing");
        JS_CallTracer(trc, JSVAL_TO_TRACEABLE(v), JSVAL_TRACE_KIND(v));
    }
    JSScopeProperty** spropp = ti->sprops.data();
    len = ti->sprops.length();
    while (len--) {
        JSScopeProperty* sprop = *spropp++;
        sprop->trace(trc);
    }
}

void
JSTraceMonitor::mark(JSTracer* trc)
{
    if (!trc->context->runtime->gcFlushCodeCaches) {
        for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
            TreeFragment* f = vmfragments[i];
            while (f) {
                if (TreeInfo* ti = f->treeInfo)
                    MarkTreeInfo(trc, ti);
                TreeFragment* peer = f->peer;
                while (peer) {
                    if (TreeInfo* ti = peer->treeInfo)
                        MarkTreeInfo(trc, ti);
                    peer = peer->peer;
                }
                f = f->next;
            }
        }
        if (recorder)
            MarkTreeInfo(trc, recorder->getTreeInfo());
    }
}

/*
 * Box a value from the native stack back into the jsval format. Integers that
 * are too large to fit into a jsval are automatically boxed into
 * heap-allocated doubles.
 */
bool
js_NativeToValue(JSContext* cx, jsval& v, JSTraceType type, double* slot)
{
    bool ok;
    jsint i;
    jsdouble d;
    switch (type) {
      case TT_OBJECT:
        v = OBJECT_TO_JSVAL(*(JSObject**)slot);
        JS_ASSERT(v != JSVAL_ERROR_COOKIE); /* don't leak JSVAL_ERROR_COOKIE */
        debug_only_printf(LC_TMTracer,
                          "object<%p:%s> ", (void*)JSVAL_TO_OBJECT(v),
                          JSVAL_IS_NULL(v)
                          ? "null"
                          : STOBJ_GET_CLASS(JSVAL_TO_OBJECT(v))->name);
        break;

      case TT_INT32:
        i = *(jsint*)slot;
        debug_only_printf(LC_TMTracer, "int<%d> ", i);
      store_int:
        if (INT_FITS_IN_JSVAL(i)) {
            v = INT_TO_JSVAL(i);
            break;
        }
        d = (jsdouble)i;
        goto store_double;
      case TT_DOUBLE:
        d = *slot;
        debug_only_printf(LC_TMTracer, "double<%g> ", d);
        if (JSDOUBLE_IS_INT(d, i))
            goto store_int;
      store_double:
        ok = js_NewDoubleInRootedValue(cx, d, &v);
        if (!ok) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;

      case TT_JSVAL:
        v = *(jsval*)slot;
        JS_ASSERT(v != JSVAL_ERROR_COOKIE); /* don't leak JSVAL_ERROR_COOKIE */
        debug_only_printf(LC_TMTracer, "box<%p> ", (void*)v);
        break;

      case TT_STRING:
        v = STRING_TO_JSVAL(*(JSString**)slot);
        debug_only_printf(LC_TMTracer, "string<%p> ", (void*)(*(JSString**)slot));
        break;

      case TT_NULL:
        JS_ASSERT(*(JSObject**)slot == NULL);
        v = JSVAL_NULL;
        debug_only_printf(LC_TMTracer, "null<%p> ", (void*)(*(JSObject**)slot));
        break;

      case TT_PSEUDOBOOLEAN:
        /* Watch out for pseudo-booleans. */
        v = SPECIAL_TO_JSVAL(*(JSBool*)slot);
        debug_only_printf(LC_TMTracer, "boolean<%d> ", *(JSBool*)slot);
        break;

      case TT_FUNCTION: {
        JS_ASSERT(HAS_FUNCTION_CLASS(*(JSObject**)slot));
        v = OBJECT_TO_JSVAL(*(JSObject**)slot);
#ifdef DEBUG
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(v));
        debug_only_printf(LC_TMTracer,
                          "function<%p:%s> ", (void*)JSVAL_TO_OBJECT(v),
                          fun->atom
                          ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
                          : "unnamed");
#endif
        break;
      }
    }
    return true;
}

class BuildNativeFrameVisitor : public SlotVisitorBase
{
    JSContext *mCx;
    JSTraceType *mTypeMap;
    double *mGlobal;
    double *mStack;
public:
    BuildNativeFrameVisitor(JSContext *cx,
                            JSTraceType *typemap,
                            double *global,
                            double *stack) :
        mCx(cx),
        mTypeMap(typemap),
        mGlobal(global),
        mStack(stack)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        debug_only_printf(LC_TMTracer, "global%d: ", n);
        ValueToNative(mCx, *vp, *mTypeMap++, &mGlobal[slot]);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, int count, JSStackFrame* fp) {
        for (int i = 0; i < count; ++i) {
            debug_only_printf(LC_TMTracer, "%s%d: ", stackSlotKind(), i);
            ValueToNative(mCx, *vp++, *mTypeMap++, mStack++);
        }
        return true;
    }
};

static JS_REQUIRES_STACK void
BuildNativeFrame(JSContext *cx, JSObject *globalObj, unsigned callDepth,
                 unsigned ngslots, uint16 *gslots,
                 JSTraceType *typeMap, double *global, double *stack)
{
    BuildNativeFrameVisitor visitor(cx, typeMap, global, stack);
    VisitSlots(visitor, cx, globalObj, callDepth, ngslots, gslots);
    debug_only_print0(LC_TMTracer, "\n");
}

class FlushNativeGlobalFrameVisitor : public SlotVisitorBase
{
    JSContext *mCx;
    JSTraceType *mTypeMap;
    double *mGlobal;
public:
    FlushNativeGlobalFrameVisitor(JSContext *cx,
                                  JSTraceType *typeMap,
                                  double *global) :
        mCx(cx),
        mTypeMap(typeMap),
        mGlobal(global)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        debug_only_printf(LC_TMTracer, "global%d=", n);
        JS_ASSERT(JS_THREAD_DATA(mCx)->waiveGCQuota);
        if (!js_NativeToValue(mCx, *vp, *mTypeMap++, &mGlobal[slot]))
            OutOfMemoryAbort();
    }
};

class FlushNativeStackFrameVisitor : public SlotVisitorBase
{
    JSContext *mCx;
    const JSTraceType *mInitTypeMap;
    const JSTraceType *mTypeMap;
    double *mStack;
    jsval *mStop;
    unsigned mIgnoreSlots;
public:
    FlushNativeStackFrameVisitor(JSContext *cx,
                                 const JSTraceType *typeMap,
                                 double *stack,
                                 jsval *stop,
                                 unsigned ignoreSlots) :
        mCx(cx),
        mInitTypeMap(typeMap),
        mTypeMap(typeMap),
        mStack(stack),
        mStop(stop),
        mIgnoreSlots(ignoreSlots)
    {}

    const JSTraceType* getTypeMap()
    {
        return mTypeMap;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        JS_ASSERT(JS_THREAD_DATA(mCx)->waiveGCQuota);
        for (size_t i = 0; i < count; ++i) {
            if (vp == mStop)
                return false;
            debug_only_printf(LC_TMTracer, "%s%u=", stackSlotKind(), unsigned(i));
            if (unsigned(mTypeMap - mInitTypeMap) >= mIgnoreSlots) {
                if (!js_NativeToValue(mCx, *vp, *mTypeMap, mStack))
                    OutOfMemoryAbort();
            }
            vp++;
            mTypeMap++;
            mStack++;
        }
        return true;
    }
};

/* Box the given native frame into a JS frame. This is infallible. */
static JS_REQUIRES_STACK void
FlushNativeGlobalFrame(JSContext *cx, JSObject *globalObj, double *global, unsigned ngslots,
                       uint16 *gslots, JSTraceType *typemap)
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
StackDepthFromCallStack(InterpState* state, uint32 callDepth)
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
inline JSTraceType
GetUpvarOnTrace(JSContext* cx, uint32 upvarLevel, int32 slot, uint32 callDepth, double* result)
{
    InterpState* state = cx->interpState;
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
        uint32 argc = state->outermostTree->rootFragment->argc;
        uint32 native_slot = T::native_slot(argc, slot);
        *result = state->stackBase[native_slot];
        return state->callstackBase[0]->get_typemap()[native_slot];
    }

    /*
     * If we did not find the upvar in the frames for the active traces,
     * then we simply get the value from the interpreter state.
     */
    JS_ASSERT(upvarLevel < JS_DISPLAY_SIZE);
    JSStackFrame* fp = cx->display[upvarLevel];
    jsval v = T::interp_get(fp, slot);
    JSTraceType type = getCoercedType(v);
    ValueToNative(cx, v, type, result);
    return type;
}

// For this traits type, 'slot' is the argument index, which may be -2 for callee.
struct UpvarArgTraits {
    static jsval interp_get(JSStackFrame* fp, int32 slot) {
        return fp->argv[slot];
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
    static jsval interp_get(JSStackFrame* fp, int32 slot) {
        return fp->slots[slot];
    }

    static uint32 native_slot(uint32 argc, int32 slot) {
        return 3 /*callee,this,arguments*/ + argc + slot;
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
    static jsval interp_get(JSStackFrame* fp, int32 slot) {
        return fp->slots[slot + fp->script->nfixed];
    }

    static uint32 native_slot(uint32 argc, int32 slot) {
        /*
         * Locals are not imported by the tracer when the frame has no
         * function, so we do not add fp->script->nfixed.
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
    jsid     id;
    uint32   slot;
    uint32   callDepth;
    uint32   resolveFlags;
};

/*
 * Generic function to read upvars from Call objects of active heavyweight functions.
 *     call       Callee Function object in which the upvar is accessed.
 */
template<typename T>
inline uint32
GetFromClosure(JSContext* cx, JSObject* call, const ClosureVarInfo* cv, double* result)
{
    JS_ASSERT(OBJ_GET_CLASS(cx, call) == &js_CallClass);

    InterpState* state = cx->interpState;

#ifdef DEBUG
    int32 stackOffset = StackDepthFromCallStack(state, cv->callDepth);
    FrameInfo** fip = state->rp + cv->callDepth;
    while (--fip > state->callstackBase) {
        FrameInfo* fi = *fip;
        JSObject* callee = *(JSObject**)(&state->stackBase[stackOffset]);
        if (callee == call) {
            // This is not reachable as long as JSOP_LAMBDA is not traced:
            // - The upvar is found at this point only if the upvar was defined on a frame that was
            //   entered on this trace.
            // - The upvar definition must be (dynamically, and thus on trace) before the definition
            //   of the function that uses the upvar.
            // - Therefore, if the upvar is found at this point, the function definition JSOP_LAMBDA
            //   is on the trace.
            JS_NOT_REACHED("JSOP_NAME variable found in outer trace");
        }
        stackOffset -= fi->callerHeight;
    }
#endif

    /*
     * Here we specifically want to check the call object of the trace entry frame.
     */
    uint32 slot = cv->slot;
    VOUCH_DOES_NOT_REQUIRE_STACK();
    if (cx->fp->callobj == call) {
        slot = T::adj_slot(cx->fp, slot);
        *result = state->stackBase[slot];
        return state->callstackBase[0]->get_typemap()[slot];
    }

    JSStackFrame* fp = (JSStackFrame*) call->getPrivate();
    jsval v;
    if (fp) {
        v = T::slots(fp)[slot];
    } else {
        JS_ASSERT(cv->resolveFlags != JSRESOLVE_INFER);
        JSAutoResolveFlags rf(cx, cv->resolveFlags);
#ifdef DEBUG
        JSBool rv =
#endif
            js_GetPropertyHelper(cx, call, cv->id, JSGET_METHOD_BARRIER, &v);
        JS_ASSERT(rv);
    }
    JSTraceType type = getCoercedType(v);
    ValueToNative(cx, v, type, result);
    return type;
}

struct ArgClosureTraits
{
    static inline uint32 adj_slot(JSStackFrame* fp, uint32 slot) { return fp->argc + slot; }
    static inline jsval* slots(JSStackFrame* fp) { return fp->argv; }
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
    static inline uint32 adj_slot(JSStackFrame* fp, uint32 slot) { return slot; }
    static inline jsval* slots(JSStackFrame* fp) { return fp->slots; }
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
 *                  cx->fp when we make this call.  If this is not called as a
 *                  result of a nested exit, callDepth is 0.
 * @param mp an array of JSTraceTypes that indicate what the types of the things
 *           on the stack are.
 * @param np pointer to the native stack.  We want to copy values from here to
 *           the JS stack as needed.
 * @param stopFrame if non-null, this frame and everything above it should not
 *                  be restored.
 * @return the number of things we popped off of np.
 */
static JS_REQUIRES_STACK int
FlushNativeStackFrame(JSContext* cx, unsigned callDepth, const JSTraceType* mp, double* np,
                      JSStackFrame* stopFrame, unsigned ignoreSlots)
{
    jsval* stopAt = stopFrame ? &stopFrame->argv[-2] : NULL;

    /* Root all string and object references first (we don't need to call the GC for this). */
    FlushNativeStackFrameVisitor visitor(cx, mp, np, stopAt, ignoreSlots);
    VisitStackSlots(visitor, cx, callDepth);

    // Restore thisv from the now-restored argv[-1] in each pending frame.
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
            if (fp->argv) {
                if (fp->argsobj && 
                    js_GetArgsPrivateNative(JSVAL_TO_OBJECT(fp->argsobj))) {
                    JSVAL_TO_OBJECT(fp->argsobj)->setPrivate(fp);
                }

                JS_ASSERT(JSVAL_IS_OBJECT(fp->argv[-1]));
                JS_ASSERT(HAS_FUNCTION_CLASS(fp->calleeObject()));
                JS_ASSERT(GET_FUNCTION_PRIVATE(cx, fp->callee()) == fp->fun);

                /*
                 * SynthesizeFrame sets scopeChain to NULL, because we can't calculate the
                 * correct scope chain until we have the final callee. Calculate the real
                 * scope object here.
                 */
                if (!fp->scopeChain) {
                    fp->scopeChain = OBJ_GET_PARENT(cx, fp->calleeObject());
                    if (fp->fun->flags & JSFUN_HEAVYWEIGHT) {
                        /*
                         * Set hookData to null because the failure case for js_GetCallObject
                         * involves it calling the debugger hook.
                         *
                         * Allocating the Call object must not fail, so use an object
                         * previously reserved by ExecuteTree if needed.
                         */
                        void* hookData = ((JSInlineFrame*)fp)->hookData;
                        ((JSInlineFrame*)fp)->hookData = NULL;
                        JS_ASSERT(JS_THREAD_DATA(cx)->waiveGCQuota);
#ifdef DEBUG
                        JSObject *obj =
#endif
                            js_GetCallObject(cx, fp);
                        JS_ASSERT(obj);
                        ((JSInlineFrame*)fp)->hookData = hookData;
                    }
                }
                fp->thisv = fp->argv[-1];
                if (fp->flags & JSFRAME_CONSTRUCTING) // constructors always compute 'this'
                    fp->flags |= JSFRAME_COMPUTED_THIS;
            }
        }
    }
    debug_only_print0(LC_TMTracer, "\n");
    return visitor.getTypeMap() - mp;
}

/* Emit load instructions onto the trace that read the initial stack state. */
JS_REQUIRES_STACK void
TraceRecorder::import(LIns* base, ptrdiff_t offset, jsval* p, JSTraceType t,
                      const char *prefix, uintN index, JSStackFrame *fp)
{
    LIns* ins;
    if (t == TT_INT32) { /* demoted */
        JS_ASSERT(isInt32(*p));

        /*
         * Ok, we have a valid demotion attempt pending, so insert an integer
         * read and promote it to double since all arithmetic operations expect
         * to see doubles on entry. The first op to use this slot will emit a
         * f2i cast which will cancel out the i2f we insert here.
         */
        ins = lir->insLoad(LIR_ld, base, offset);
        ins = lir->ins1(LIR_i2f, ins);
    } else {
        JS_ASSERT_IF(t != TT_JSVAL, isNumber(*p) == (t == TT_DOUBLE));
        if (t == TT_DOUBLE) {
            ins = lir->insLoad(LIR_ldq, base, offset);
        } else if (t == TT_PSEUDOBOOLEAN) {
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
        if (fp->fun->hasLocalNames())
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
        "object", "int", "double", "jsval", "string", "null", "boolean", "function"
    };
    debug_only_printf(LC_TMTracer, "import vp=%p name=%s type=%s flags=%d\n",
                      (void*)p, name, typestr[t & 7], t >> 3);
#endif
}

class ImportGlobalSlotVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    LIns *mBase;
    JSTraceType *mTypemap;
public:
    ImportGlobalSlotVisitor(TraceRecorder &recorder,
                            LIns *base,
                            JSTraceType *typemap) :
        mRecorder(recorder),
        mBase(base),
        mTypemap(typemap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        JS_ASSERT(*mTypemap != TT_JSVAL);
        mRecorder.import(mBase, mRecorder.nativeGlobalOffset(vp),
                         vp, *mTypemap++, "global", n, NULL);
    }
};

class ImportBoxedStackSlotVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    LIns *mBase;
    ptrdiff_t mStackOffset;
    JSTraceType *mTypemap;
    JSStackFrame *mFp;
public:
    ImportBoxedStackSlotVisitor(TraceRecorder &recorder,
                                LIns *base,
                                ptrdiff_t stackOffset,
                                JSTraceType *typemap) :
        mRecorder(recorder),
        mBase(base),
        mStackOffset(stackOffset),
        mTypemap(typemap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            if (*mTypemap == TT_JSVAL) {
                mRecorder.import(mBase, mStackOffset, vp, TT_JSVAL,
                                 "jsval", i, fp);
                LIns *vp_ins = mRecorder.unbox_jsval(*vp, mRecorder.get(vp),
                                                     mRecorder.copy(mRecorder.anchor));
                mRecorder.set(vp, vp_ins);
            }
            vp++;
            mTypemap++;
            mStackOffset += sizeof(double);
        }
        return true;
    }
};

class ImportUnboxedStackSlotVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    LIns *mBase;
    ptrdiff_t mStackOffset;
    JSTraceType *mTypemap;
    JSStackFrame *mFp;
public:
    ImportUnboxedStackSlotVisitor(TraceRecorder &recorder,
                                  LIns *base,
                                  ptrdiff_t stackOffset,
                                  JSTraceType *typemap) :
        mRecorder(recorder),
        mBase(base),
        mStackOffset(stackOffset),
        mTypemap(typemap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            if (*mTypemap != TT_JSVAL) {
                mRecorder.import(mBase, mStackOffset, vp++, *mTypemap,
                                 stackSlotKind(), i, fp);
            }
            mTypemap++;
            mStackOffset += sizeof(double);
        }
        return true;
    }
};

JS_REQUIRES_STACK void
TraceRecorder::import(TreeInfo* treeInfo, LIns* sp, unsigned stackSlots, unsigned ngslots,
                      unsigned callDepth, JSTraceType* typeMap)
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

    JSTraceType* globalTypeMap = typeMap + stackSlots;
    unsigned length = treeInfo->nGlobalTypes();

    /*
     * This is potentially the typemap of the side exit and thus shorter than
     * the tree's global type map.
     */
    if (ngslots < length) {
        MergeTypeMaps(&globalTypeMap /* out param */, &ngslots /* out param */,
                      treeInfo->globalTypeMap(), length,
                      (JSTraceType*)alloca(sizeof(JSTraceType) * length));
    }
    JS_ASSERT(ngslots == treeInfo->nGlobalTypes());
    ptrdiff_t offset = -treeInfo->nativeStackBase;

    /*
     * Check whether there are any values on the stack we have to unbox and do
     * that first before we waste any time fetching the state from the stack.
     */
    if (!anchor || anchor->exitType != RECURSIVE_SLURP_FAIL_EXIT) {
        ImportBoxedStackSlotVisitor boxedStackVisitor(*this, sp, offset, typeMap);
        VisitStackSlots(boxedStackVisitor, cx, callDepth);
    }

    ImportGlobalSlotVisitor globalVisitor(*this, eos_ins, globalTypeMap);
    VisitGlobalSlots(globalVisitor, cx, globalObj, ngslots,
                     treeInfo->globalSlots->data());

    if (!anchor || anchor->exitType != RECURSIVE_SLURP_FAIL_EXIT) {
        ImportUnboxedStackSlotVisitor unboxedStackVisitor(*this, sp, offset,
                                                          typeMap);
        VisitStackSlots(unboxedStackVisitor, cx, callDepth);
    }
}

JS_REQUIRES_STACK bool
TraceRecorder::isValidSlot(JSScope* scope, JSScopeProperty* sprop)
{
    uint32 setflags = (js_CodeSpec[*cx->fp->regs->pc].format & (JOF_SET | JOF_INCDEC | JOF_FOR));

    if (setflags) {
        if (!SPROP_HAS_STUB_SETTER(sprop))
            RETURN_VALUE("non-stub setter", false);
        if (sprop->attrs & JSPROP_READONLY)
            RETURN_VALUE("writing to a read-only property", false);
    }

    /* This check applies even when setflags == 0. */
    if (setflags != JOF_SET && !SPROP_HAS_STUB_GETTER(sprop)) {
        JS_ASSERT(!sprop->isMethod());
        RETURN_VALUE("non-stub getter", false);
    }

    if (!SPROP_HAS_VALID_SLOT(sprop, scope))
        RETURN_VALUE("slotless obj property", false);

    return true;
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
    if (STOBJ_NSLOTS(globalObj) > MAX_GLOBAL_SLOTS)
        return false;
    jsval* vp = &STOBJ_GET_SLOT(globalObj, slot);
    if (known(vp))
        return true; /* we already have it */
    unsigned index = treeInfo->globalSlots->length();

    /* Add the slot to the list of interned global slots. */
    JS_ASSERT(treeInfo->nGlobalTypes() == treeInfo->globalSlots->length());
    treeInfo->globalSlots->add(slot);
    JSTraceType type = getCoercedType(*vp);
    if (type == TT_INT32 && oracle.isGlobalSlotUndemotable(cx, slot))
        type = TT_DOUBLE;
    treeInfo->typeMap.add(type);
    import(eos_ins, slot*sizeof(double), vp, type, "global", index, NULL);
    SpecializeTreesToMissingGlobals(cx, globalObj, treeInfo);
    return true;
}

/* Write back a value onto the stack or global frames. */
LIns*
TraceRecorder::writeBack(LIns* i, LIns* base, ptrdiff_t offset, bool demote)
{
    /*
     * Sink all type casts targeting the stack into the side exit by simply storing the original
     * (uncasted) value. Each guard generates the side exit map based on the types of the
     * last stores to every stack location, so it's safe to not perform them on-trace.
     */
    if (demote && isPromoteInt(i))
        i = ::demote(lir, i);
    return lir->insStorei(i, base, offset);
}

/* Update the tracker, then issue a write back store. */
JS_REQUIRES_STACK void
TraceRecorder::set(jsval* p, LIns* i, bool initializing, bool demote)
{
    JS_ASSERT(i != NULL);
    JS_ASSERT(initializing || known(p));
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
        if (isGlobal(p))
            x = writeBack(i, eos_ins, nativeGlobalOffset(p), demote);
        else
            x = writeBack(i, lirbuf->sp, nativespOffset(p), demote);
        nativeFrameTracker.set(p, x);
    } else {
        JS_ASSERT(x->isop(LIR_sti) || x->isop(LIR_stqi));

        int disp;
        LIns *base = x->oprnd2();
#ifdef NANOJIT_ARM
        if (base->isop(LIR_piadd)) {
            disp = base->oprnd2()->imm32();
            base = base->oprnd1();
        } else
#endif
        disp = x->disp();

        JS_ASSERT(base == lirbuf->sp || base == eos_ins);
        JS_ASSERT(disp == ((base == lirbuf->sp)
                            ? nativespOffset(p)
                            : nativeGlobalOffset(p)));

        writeBack(i, base, disp, demote);
    }
}

JS_REQUIRES_STACK LIns*
TraceRecorder::get(jsval* p)
{
    JS_ASSERT(known(p));
    checkForGlobalObjectReallocation();
    return tracker.get(p);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::addr(jsval* p)
{
    return isGlobal(p)
           ? lir->ins2(LIR_piadd, eos_ins, INS_CONSTWORD(nativeGlobalOffset(p)))
           : lir->ins2(LIR_piadd, lirbuf->sp,
                       INS_CONSTWORD(nativespOffset(p)));
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
        debug_only_print0(LC_TMTracer,
                          "globalObj->dslots relocated, updating tracker\n");
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
    nanojit::LirWriter *mLir;
    JSTraceType *mTypeMap;
public:
    AdjustCallerGlobalTypesVisitor(TraceRecorder &recorder,
                                   JSTraceType *typeMap) :
        mRecorder(recorder),
        mCx(mRecorder.cx),
        mLirbuf(mRecorder.lirbuf),
        mLir(mRecorder.lir),
        mTypeMap(typeMap)
    {}

    JSTraceType* getTypeMap()
    {
        return mTypeMap;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        LIns *ins = mRecorder.get(vp);
        bool isPromote = isPromoteInt(ins);
        if (isPromote && *mTypeMap == TT_DOUBLE) {
            mLir->insStorei(mRecorder.get(vp), mRecorder.eos_ins,
                            mRecorder.nativeGlobalOffset(vp));

            /*
             * Aggressively undo speculation so the inner tree will compile
             * if this fails.
             */
            oracle.markGlobalSlotUndemotable(mCx, slot);
        }
        JS_ASSERT(!(!isPromote && *mTypeMap == TT_INT32));
        ++mTypeMap;
    }
};

class AdjustCallerStackTypesVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSContext *mCx;
    nanojit::LirBuffer *mLirbuf;
    nanojit::LirWriter *mLir;
    unsigned mSlotnum;
    JSTraceType *mTypeMap;
public:
    AdjustCallerStackTypesVisitor(TraceRecorder &recorder,
                                  JSTraceType *typeMap) :
        mRecorder(recorder),
        mCx(mRecorder.cx),
        mLirbuf(mRecorder.lirbuf),
        mLir(mRecorder.lir),
        mSlotnum(0),
        mTypeMap(typeMap)
    {}

    JSTraceType* getTypeMap()
    {
        return mTypeMap;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            LIns *ins = mRecorder.get(vp);
            bool isPromote = isPromoteInt(ins);
            if (isPromote && *mTypeMap == TT_DOUBLE) {
                mLir->insStorei(mRecorder.get(vp), mLirbuf->sp,
                                mRecorder.nativespOffset(vp));

                /*
                 * Aggressively undo speculation so the inner tree will compile
                 * if this fails.
                 */
                oracle.markStackSlotUndemotable(mCx, mSlotnum);
            }
            JS_ASSERT(!(!isPromote && *mTypeMap == TT_INT32));
            ++vp;
            ++mTypeMap;
            ++mSlotnum;
        }
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
    TreeInfo* ti = f->treeInfo;

    AdjustCallerGlobalTypesVisitor globalVisitor(*this, ti->globalTypeMap());
    VisitGlobalSlots(globalVisitor, cx, *treeInfo->globalSlots);

    AdjustCallerStackTypesVisitor stackVisitor(*this, ti->stackTypeMap());
    VisitStackSlots(stackVisitor, cx, 0);

    JS_ASSERT(f == f->root);
}

JS_REQUIRES_STACK JSTraceType
TraceRecorder::determineSlotType(jsval* vp)
{
    JSTraceType m;
    LIns* i = get(vp);
    if (isNumber(*vp)) {
        m = isPromoteInt(i) ? TT_INT32 : TT_DOUBLE;
    } else if (JSVAL_IS_OBJECT(*vp)) {
        if (JSVAL_IS_NULL(*vp))
            m = TT_NULL;
        else if (HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(*vp)))
            m = TT_FUNCTION;
        else
            m = TT_OBJECT;
    } else {
        JS_ASSERT(JSVAL_TAG(*vp) == JSVAL_STRING || JSVAL_IS_SPECIAL(*vp));
        JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_STRING) == JSVAL_STRING);
        JS_STATIC_ASSERT(static_cast<jsvaltag>(TT_PSEUDOBOOLEAN) == JSVAL_SPECIAL);
        m = JSTraceType(JSVAL_TAG(*vp));
    }
    JS_ASSERT(m != TT_INT32 || isInt32(*vp));
    return m;
}

class DetermineTypesVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSTraceType *mTypeMap;
public:
    DetermineTypesVisitor(TraceRecorder &recorder,
                          JSTraceType *typeMap) :
        mRecorder(recorder),
        mTypeMap(typeMap)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        *mTypeMap++ = mRecorder.determineSlotType(vp);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i)
            *mTypeMap++ = mRecorder.determineSlotType(vp++);
        return true;
    }

    JSTraceType* getTypeMap()
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
                      (void*)exit->from, (void*)cx->fp->regs->pc, cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp), FramePCOffset(cx->fp));
    debug_only_print0(LC_TMTreeVis, " STACK=\"");
    for (unsigned i = 0; i < exit->numStackSlots; i++)
        debug_only_printf(LC_TMTreeVis, "%c", typeChar[exit->stackTypeMap()[i]]);
    debug_only_print0(LC_TMTreeVis, "\" GLOBALS=\"");
    for (unsigned i = 0; i < exit->numGlobalSlots; i++)
        debug_only_printf(LC_TMTreeVis, "%c", typeChar[exit->globalTypeMap()[i]]);
    debug_only_print0(LC_TMTreeVis, "\"\n");
}
#endif

JS_REQUIRES_STACK VMSideExit*
TraceRecorder::snapshot(ExitType exitType)
{
    JSStackFrame* fp = cx->fp;
    JSFrameRegs* regs = fp->regs;
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
    unsigned ngslots = treeInfo->globalSlots->length();
    unsigned typemap_size = (stackSlots + ngslots) * sizeof(JSTraceType);

    /* Use the recorder-local temporary type map. */
    JSTraceType* typemap = NULL;
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
               treeInfo->globalSlots->data());
    JS_ASSERT(unsigned(detVisitor.getTypeMap() - typemap) ==
              ngslots + stackSlots);

    /*
     * If this snapshot is for a side exit that leaves a boxed jsval result on
     * the stack, make a note of this in the typemap. Examples include the
     * builtinStatus guard after calling a _FAIL builtin, a JSFastNative, or
     * GetPropertyByName; and the type guard in unbox_jsval after such a call
     * (also at the beginning of a trace branched from such a type guard).
     */
    if (pendingUnboxSlot ||
        (pendingSpecializedNative && (pendingSpecializedNative->flags & JSTN_UNBOX_AFTER))) {
        unsigned pos = stackSlots - 1;
        if (pendingUnboxSlot == cx->fp->regs->sp - 2)
            pos = stackSlots - 2;
        typemap[pos] = TT_JSVAL;
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
    VMSideExit** exits = treeInfo->sideExits.data();
    unsigned nexits = treeInfo->sideExits.length();
    if (exitType == LOOP_EXIT) {
        for (unsigned n = 0; n < nexits; ++n) {
            VMSideExit* e = exits[n];
            if (e->pc == pc && e->imacpc == fp->imacpc &&
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
        traceAlloc().alloc(sizeof(VMSideExit) + (stackSlots + ngslots) * sizeof(JSTraceType));

    /* Setup side exit structure. */
    exit->from = fragment;
    exit->calldepth = callDepth;
    exit->numGlobalSlots = ngslots;
    exit->numStackSlots = stackSlots;
    exit->numStackSlotsBelowCurrentFrame = cx->fp->argv ?
                                           nativeStackOffset(&cx->fp->argv[-2]) / sizeof(double) :
                                           0;
    exit->exitType = exitType;
    exit->block = fp->blockChain;
    if (fp->blockChain)
        treeInfo->gcthings.addUnique(OBJECT_TO_JSVAL(fp->blockChain));
    exit->pc = pc;
    exit->imacpc = fp->imacpc;
    exit->sp_adj = (stackSlots * sizeof(double)) - treeInfo->nativeStackBase;
    exit->rp_adj = exit->calldepth * sizeof(FrameInfo*);
    exit->nativeCalleeWord = 0;
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
    GuardRecord* gr = new (traceAlloc()) GuardRecord();

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

/*
 * Emit a guard for condition (cond), expecting to evaluate to boolean result
 * (expected) and using the supplied side exit if the conditon doesn't hold.
 */
JS_REQUIRES_STACK void
TraceRecorder::guard(bool expected, LIns* cond, VMSideExit* exit)
{
    debug_only_printf(LC_TMRecorder,
                      "    About to try emitting guard code for "
                      "SideExit=%p exitType=%s\n",
                      (void*)exit, getExitName(exit->exitType));

    GuardRecord* guardRec = createGuardRecord(exit);

    if (exit->exitType == LOOP_EXIT)
        treeInfo->sideExits.add(exit);

    if (!cond->isCond()) {
        expected = !expected;
        cond = cond->isQuad() ? lir->ins_peq0(cond) : lir->ins_eq0(cond);
    }

    LIns* guardIns =
        lir->insGuard(expected ? LIR_xf : LIR_xt, cond, guardRec);
    if (!guardIns) {
        debug_only_print0(LC_TMRecorder,
                          "    redundant guard, eliminated, no codegen\n");
    }
}

JS_REQUIRES_STACK VMSideExit*
TraceRecorder::copy(VMSideExit* copy)
{
    size_t typemap_size = copy->numGlobalSlots + copy->numStackSlots;
    VMSideExit* exit = (VMSideExit*)
        traceAlloc().alloc(sizeof(VMSideExit) + typemap_size * sizeof(JSTraceType));

    /* Copy side exit structure. */
    memcpy(exit, copy, sizeof(VMSideExit) + typemap_size * sizeof(JSTraceType));
    exit->guards = NULL;
    exit->from = fragment;
    exit->target = NULL;

    if (exit->exitType == LOOP_EXIT)
        treeInfo->sideExits.add(exit);
#if defined JS_JIT_SPEW
    TreevisLogExit(cx, exit);
#endif
    return exit;
}

/*
 * Emit a guard for condition (cond), expecting to evaluate to boolean result
 * (expected) and generate a side exit with type exitType to jump to if the
 * condition does not hold.
 */
JS_REQUIRES_STACK void
TraceRecorder::guard(bool expected, LIns* cond, ExitType exitType)
{
    guard(expected, cond, snapshot(exitType));
}

/*
 * Determine whether any context associated with the same thread as cx is
 * executing native code.
 */
static inline bool
ProhibitFlush(JSContext* cx)
{
    if (cx->interpState) // early out if the given is in native code
        return true;

    JSCList *cl;

#ifdef JS_THREADSAFE
    JSThread* thread = cx->thread;
    for (cl = thread->contextList.next; cl != &thread->contextList; cl = cl->next)
        if (CX_FROM_THREAD_LINKS(cl)->interpState)
            return true;
#else
    JSRuntime* rt = cx->runtime;
    for (cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next)
        if (js_ContextFromLinkField(cl)->interpState)
            return true;
#endif
    return false;
}

static JS_REQUIRES_STACK void
ResetJITImpl(JSContext* cx)
{
    if (!TRACING_ENABLED(cx))
        return;
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    debug_only_print0(LC_TMTracer, "Flushing cache.\n");
    if (tm->recorder)
        js_AbortRecording(cx, "flush cache");
    if (ProhibitFlush(cx)) {
        debug_only_print0(LC_TMTracer, "Deferring JIT flush due to deep bail.\n");
        tm->needFlush = JS_TRUE;
        return;
    }
    tm->flush();
}

JS_REQUIRES_STACK void
js_ResetJIT(JSContext* cx)
{
    ResetJIT(cx, FR_OOM);
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
    if (treeInfo->maxNativeStackSlots >= MAX_NATIVE_STACK_SLOTS) {
        debug_only_print0(LC_TMTracer, "Blacklist: excessive stack use.\n");
        Blacklist((jsbytecode*) fragment->root->ip);
        return ARECORD_STOP;
    }
    if (anchor && anchor->exitType != CASE_EXIT)
        ++fragment->root->branchCount;
    if (outOfMemory())
        return ARECORD_STOP;

    Assembler *assm = traceMonitor->assembler;
    JS_ASSERT(assm->error() == nanojit::None);
    nanojit::compile(assm, fragment, tempAlloc() verbose_only(, traceMonitor->labels));

    if (assm->error() != nanojit::None) {
        assm->setError(nanojit::None);
        debug_only_print0(LC_TMTracer, "Blacklisted: error during compilation\n");
        Blacklist((jsbytecode*) fragment->root->ip);
        return ARECORD_STOP;
    }

    if (outOfMemory())
        return ARECORD_STOP;
    ResetRecordingAttempts(cx, (jsbytecode*) fragment->ip);
    ResetRecordingAttempts(cx, (jsbytecode*) fragment->root->ip);
    if (anchor) {
#ifdef NANOJIT_IA32
        if (anchor->exitType == CASE_EXIT)
            assm->patch(anchor, anchor->switchInfo);
        else
#endif
            assm->patch(anchor);
    }
    JS_ASSERT(fragment->code());
    JS_ASSERT_IF(fragment == fragment->root, !fragment->root->treeInfo);
    if (fragment == fragment->root)
        fragment->root->treeInfo = treeInfo;

    /* :TODO: windows support */
#if defined DEBUG && !defined WIN32
    const char* filename = cx->fp->script->filename;
    char* label = (char*)js_malloc((filename ? strlen(filename) : 7) + 16);
    sprintf(label, "%s:%u", filename ? filename : "<stdin>",
            js_FramePCToLineNumber(cx, cx->fp));
    traceMonitor->labels->add(fragment, sizeof(Fragment), 0, label);
    js_free(label);
#endif
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

    target->treeInfo->dependentTrees.addUnique(exit->root());
    exit->root()->treeInfo->linkedTrees.addUnique(target);
}

/* Results of trying to connect an arbitrary type A with arbitrary type B */
enum TypeCheckResult
{
    TypeCheck_Okay,         /* Okay: same type */
    TypeCheck_Promote,      /* Okay: Type A needs f2i() */
    TypeCheck_Demote,       /* Okay: Type A needs i2f() */
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
        SlotInfo(jsval* vp, bool promoteInt)
          : vp(vp), promoteInt(promoteInt), lastCheck(TypeCheck_Bad), type(getCoercedType(*vp))
        {}
        SlotInfo(jsval* vp, JSTraceType t)
          : vp(vp), promoteInt(t == TT_INT32), lastCheck(TypeCheck_Bad), type(t)
        {}
        jsval           *vp;
        bool            promoteInt;
        TypeCheckResult lastCheck;
        JSTraceType     type;
    };

    SlotMap(TraceRecorder& rec)
        : mRecorder(rec),
          mCx(rec.cx),
          slots(NULL)
    {
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot)
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
    checkTypes(TreeInfo* ti)
    {
        if (length() != ti->typeMap.length())
            return TypeConsensus_Bad;

        bool has_undemotes = false;
        for (unsigned i = 0; i < length(); i++) {
            TypeCheckResult result = checkType(i, ti->typeMap[i]);
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
    addSlot(jsval* vp)
    {
        slots.add(SlotInfo(vp, isPromoteInt(mRecorder.get(vp))));
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    addSlot(JSTraceType t)
    {
        slots.add(SlotInfo(NULL, t));
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    addSlot(jsval *vp, JSTraceType t)
    {
        slots.add(SlotInfo(vp, t));
    }

    JS_REQUIRES_STACK void
    markUndemotes()
    {
        for (unsigned i = 0; i < length(); i++) {
            if (get(i).lastCheck == TypeCheck_Undemote)
                MarkSlotUndemotable(mRecorder.cx, mRecorder.treeInfo, i);
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
        if (info.lastCheck == TypeCheck_Promote) {
            JS_ASSERT(info.type == TT_INT32 || info.type == TT_DOUBLE);
            mRecorder.set(info.vp, mRecorder.f2i(mRecorder.get(info.vp)));
        } else if (info.lastCheck == TypeCheck_Demote) {
            JS_ASSERT(info.type == TT_INT32 || info.type == TT_DOUBLE);
            JS_ASSERT(mRecorder.get(info.vp)->isQuad());

            /* Never demote this final i2f. */
            mRecorder.set(info.vp, mRecorder.get(info.vp), false, false);
        }
    }

  private:
    TypeCheckResult
    checkType(unsigned i, JSTraceType t)
    {
        debug_only_printf(LC_TMTracer,
                          "checkType slot %d: interp=%c typemap=%c isNum=%d promoteInt=%d\n",
                          i,
                          typeChar[slots[i].type],
                          typeChar[t],
                          slots[i].type == TT_INT32 || slots[i].type == TT_DOUBLE,
                          slots[i].promoteInt);
        switch (t) {
          case TT_INT32:
            if (slots[i].type != TT_INT32 && slots[i].type != TT_DOUBLE)
                return TypeCheck_Bad; /* Not a number? Type mismatch. */
            /* This is always a type mismatch, we can't close a double to an int. */
            if (!slots[i].promoteInt)
                return TypeCheck_Undemote;
            /* Looks good, slot is an int32, the last instruction should be promotable. */
            JS_ASSERT_IF(slots[i].vp, isInt32(*slots[i].vp) && slots[i].promoteInt);
            return slots[i].vp ? TypeCheck_Promote : TypeCheck_Okay;
          case TT_DOUBLE:
            if (slots[i].type != TT_INT32 && slots[i].type != TT_DOUBLE)
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

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp)
    {
        for (size_t i = 0; i < count; i++)
            addSlot(&vp[i]);
        return true;
    }
};

JS_REQUIRES_STACK TypeConsensus
TraceRecorder::selfTypeStability(SlotMap& slotMap)
{
    debug_only_printf(LC_TMTracer, "Checking type stability against self=%p\n", (void*)fragment);
    TypeConsensus consensus = slotMap.checkTypes(treeInfo);

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
    TreeFragment* root = fragment->root;
    TreeFragment* peer = LookupLoop(traceMonitor, ip, root->globalObj, root->globalShape, root->argc);

    /* This condition is possible with recursion */
    JS_ASSERT_IF(!peer, fragment->root->ip != ip);
    if (!peer)
        return TypeConsensus_Bad;
    bool onlyUndemotes = false;
    for (; peer != NULL; peer = peer->peer) {
        if (!peer->treeInfo || peer == fragment)
            continue;
        debug_only_printf(LC_TMTracer, "Checking type stability against peer=%p\n", (void*)peer);
        TypeConsensus consensus = slotMap.checkTypes(peer->treeInfo);
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
    VisitSlots(slotMap, cx, 0, *treeInfo->globalSlots);
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
     * case this loop was blacklisted in the meantime, JSOP_NOP.
     */
    JS_ASSERT((*cx->fp->regs->pc == JSOP_TRACE || *cx->fp->regs->pc == JSOP_NOP ||
               *cx->fp->regs->pc == JSOP_RETURN) && !cx->fp->imacpc);

    if (callDepth != 0) {
        debug_only_print0(LC_TMTracer,
                          "Blacklisted: stack depth mismatch, possible recursion.\n");
        Blacklist((jsbytecode*) fragment->root->ip);
        trashSelf = true;
        return ARECORD_STOP;
    }

    JS_ASSERT_IF(exit->exitType == UNSTABLE_LOOP_EXIT,
                 exit->numStackSlots == treeInfo->nStackTypes);
    JS_ASSERT_IF(exit->exitType != UNSTABLE_LOOP_EXIT, exit->exitType == RECURSIVE_UNLINKED_EXIT);
    JS_ASSERT_IF(exit->exitType == RECURSIVE_UNLINKED_EXIT,
                 exit->recursive_pc != fragment->root->ip);

    TreeFragment* peer = NULL;
    TreeFragment* root = fragment->root;

    TypeConsensus consensus = TypeConsensus_Bad;

    if (exit->exitType == UNSTABLE_LOOP_EXIT)
        consensus = selfTypeStability(slotMap);
    if (consensus != TypeConsensus_Okay) {
        const void* ip = exit->exitType == RECURSIVE_UNLINKED_EXIT ?
                         exit->recursive_pc : fragment->root->ip;
        TypeConsensus peerConsensus = peerTypeStability(slotMap, ip, &peer);
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
        fragment->lastIns = lir->insGuard(LIR_x, NULL, createGuardRecord(exit));

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
            uexit->next = treeInfo->unstableExits;
            treeInfo->unstableExits = uexit;
        } else {
            JS_ASSERT(peer->code());
            exit->target = peer;
            debug_only_printf(LC_TMTracer,
                              "Joining type-unstable trace to target fragment %p.\n",
                              (void*)peer);
            peer->treeInfo->dependentTrees.addUnique(fragment->root);
            treeInfo->linkedTrees.addUnique(peer);
        }
    } else {
        exit->exitType = LOOP_EXIT;
        debug_only_printf(LC_TMTreeVis, "TREEVIS CHANGEEXIT EXIT=%p TYPE=%s\n", (void*)exit,
                          getExitName(LOOP_EXIT));

        JS_ASSERT((fragment == fragment->root) == !!loopLabel);
        if (loopLabel) {
            lir->insBranch(LIR_j, NULL, loopLabel);
            lir->ins1(LIR_live, lirbuf->state);
        }

        exit->target = fragment->root;
        fragment->lastIns = lir->insGuard(LIR_x, NULL, createGuardRecord(exit));
    }

    CHECK_STATUS_A(compile());

    debug_only_printf(LC_TMTreeVis, "TREEVIS CLOSELOOP EXIT=%p PEER=%p\n", (void*)exit, (void*)peer);

    peer = LookupLoop(traceMonitor, root->ip, root->globalObj, root->globalShape, root->argc);
    JS_ASSERT(peer);
    joinEdgesToEntry(peer);

    debug_only_stmt(DumpPeerStability(traceMonitor, peer->ip, peer->globalObj,
                                      peer->globalShape, peer->argc);)

    debug_only_print0(LC_TMTracer,
                      "updating specializations on dependent and linked trees\n");
    if (fragment->root->treeInfo)
        SpecializeTreesToMissingGlobals(cx, globalObj, fragment->root->treeInfo);

    /*
     * If this is a newly formed tree, and the outer tree has not been compiled yet, we
     * should try to compile the outer tree again.
     */
    if (outer)
        AttemptCompilation(cx, traceMonitor, globalObj, outer, outerArgc);
#ifdef JS_JIT_SPEW
    debug_only_printf(LC_TMMinimal,
                      "Recording completed at  %s:%u@%u via closeLoop (FragID=%06u)\n",
                      cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp),
                      FramePCOffset(cx->fp),
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
    if (exit->numGlobalSlots < exit->root()->treeInfo->nGlobalTypes()) {
        typeMap.fromRaw(exit->root()->treeInfo->globalTypeMap() + exit->numGlobalSlots,
                        exit->root()->treeInfo->nGlobalTypes() - exit->numGlobalSlots);
    }
}

static JS_REQUIRES_STACK TypeConsensus
TypeMapLinkability(JSContext* cx, const TypeMap& typeMap, TreeFragment* peer)
{
    const TypeMap& peerMap = peer->treeInfo->typeMap;
    unsigned minSlots = JS_MIN(typeMap.length(), peerMap.length());
    TypeConsensus consensus = TypeConsensus_Okay;
    for (unsigned i = 0; i < minSlots; i++) {
        if (typeMap[i] == peerMap[i])
            continue;
        if (typeMap[i] == TT_INT32 && peerMap[i] == TT_DOUBLE &&
            IsSlotUndemotable(cx, peer->treeInfo, i, peer->ip)) {
            consensus = TypeConsensus_Undemotes;
        } else {
            return TypeConsensus_Bad;
        }
    }
    return consensus;
}

static JS_REQUIRES_STACK unsigned
FindUndemotesInTypemaps(JSContext* cx, const TypeMap& typeMap, TreeInfo* treeInfo,
                        Queue<unsigned>& undemotes)
{
    undemotes.setLength(0);
    unsigned minSlots = JS_MIN(typeMap.length(), treeInfo->typeMap.length());
    for (unsigned i = 0; i < minSlots; i++) {
        if (typeMap[i] == TT_INT32 && treeInfo->typeMap[i] == TT_DOUBLE) {
            undemotes.add(i);
        } else if (typeMap[i] != treeInfo->typeMap[i]) {
            return 0;
        }
    }
    for (unsigned i = 0; i < undemotes.length(); i++)
        MarkSlotUndemotable(cx, treeInfo, undemotes[i]);
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
        TreeInfo* ti = peer->treeInfo;
        if (!ti)
            continue;
        UnstableExit* uexit = ti->unstableExits;
        while (uexit != NULL) {
            /* :TODO: these exits go somewhere else. */
            if (uexit->exit->exitType == RECURSIVE_UNLINKED_EXIT) {
                uexit = uexit->next;
                continue;
            }
            /* Build the full typemap for this unstable exit */
            FullMapFromExit(typeMap, uexit->exit);
            /* Check its compatibility against this tree */
            TypeConsensus consensus = TypeMapLinkability(cx, typeMap, fragment->root);
            JS_ASSERT_IF(consensus == TypeConsensus_Okay, peer != fragment);
            if (consensus == TypeConsensus_Okay) {
                debug_only_printf(LC_TMTracer,
                                  "Joining type-stable trace to target exit %p->%p.\n",
                                  (void*)uexit->fragment, (void*)uexit->exit);
                /* It's okay! Link together and remove the unstable exit. */
                JoinPeers(traceMonitor->assembler, uexit->exit, (TreeFragment*)fragment);
                uexit = ti->removeUnstableExit(uexit->exit);
            } else {
                /* Check for int32->double slots that suggest trashing. */
                if (FindUndemotesInTypemaps(cx, typeMap, treeInfo, undemotes)) {
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
    if (callDepth != 0) {
        debug_only_print0(LC_TMTracer, "Blacklisted: stack depth mismatch, possible recursion.\n");
        Blacklist((jsbytecode*) fragment->root->ip);
        trashSelf = true;
        return ARECORD_STOP;
    }

    if (recordReason != Record_Branch)
        RETURN_STOP_A("control flow should have been recursive");

    fragment->lastIns =
        lir->insGuard(LIR_x, NULL, createGuardRecord(exit));

    CHECK_STATUS_A(compile());

    debug_only_printf(LC_TMTreeVis, "TREEVIS ENDLOOP EXIT=%p\n", (void*)exit);

    TreeFragment* root = fragment->root;
    joinEdgesToEntry(LookupLoop(traceMonitor, root->ip, root->globalObj,
                                root->globalShape, root->argc));
    debug_only_stmt(DumpPeerStability(traceMonitor, root->ip, root->globalObj,
                                      root->globalShape, root->argc);)

    /*
     * Note: this must always be done, in case we added new globals on trace
     * and haven't yet propagated those to linked and dependent trees.
     */
    debug_only_print0(LC_TMTracer,
                      "updating specializations on dependent and linked trees\n");
    if (fragment->root->treeInfo)
        SpecializeTreesToMissingGlobals(cx, globalObj, fragment->root->treeInfo);

    /*
     * If this is a newly formed tree, and the outer tree has not been compiled
     * yet, we should try to compile the outer tree again.
     */
    if (outer)
        AttemptCompilation(cx, traceMonitor, globalObj, outer, outerArgc);
#ifdef JS_JIT_SPEW
    debug_only_printf(LC_TMMinimal,
                      "Recording completed at  %s:%u@%u via endLoop (FragID=%06u)\n",
                      cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp),
                      FramePCOffset(cx->fp),
                      fragment->profFragID);
    debug_only_print0(LC_TMTracer, "\n");
#endif

    return finishSuccessfully();
}

/* Emit code to adjust the stack to match the inner tree's stack expectations. */
JS_REQUIRES_STACK void
TraceRecorder::prepareTreeCall(TreeFragment* inner, LIns*& inner_sp_ins)
{
    TreeInfo* ti = inner->treeInfo;
    inner_sp_ins = lirbuf->sp;
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
        ptrdiff_t sp_adj = nativeStackOffset(&cx->fp->argv[-2]);

        /* Calculate the amount we have to lift the call stack by. */
        ptrdiff_t rp_adj = callDepth * sizeof(FrameInfo*);

        /*
         * Guard that we have enough stack space for the tree we are trying to
         * call on top of the new value for sp.
         */
        debug_only_printf(LC_TMTracer,
                          "sp_adj=%lld outer=%lld inner=%lld\n",
                          (long long int)sp_adj,
                          (long long int)treeInfo->nativeStackBase,
                          (long long int)ti->nativeStackBase);
        ptrdiff_t sp_offset = 
                - treeInfo->nativeStackBase /* rebase sp to beginning of outer tree's stack */
                + sp_adj /* adjust for stack in outer frame inner tree can't see */
                + ti->maxNativeStackSlots * sizeof(double); /* plus the inner tree's stack */
        LIns* sp_top = lir->ins2(LIR_piadd, lirbuf->sp, INS_CONSTWORD(sp_offset));
        guard(true, lir->ins2(LIR_plt, sp_top, eos_ins), exit);

        /* Guard that we have enough call stack space. */
        ptrdiff_t rp_offset = rp_adj + ti->maxCallDepth * sizeof(FrameInfo*);
        LIns* rp_top = lir->ins2(LIR_piadd, lirbuf->rp, INS_CONSTWORD(rp_offset));
        guard(true, lir->ins2(LIR_plt, rp_top, eor_ins), exit);

        sp_offset =
                - treeInfo->nativeStackBase /* rebase sp to beginning of outer tree's stack */
                + sp_adj /* adjust for stack in outer frame inner tree can't see */
                + ti->nativeStackBase; /* plus the inner tree's stack base */
        /* We have enough space, so adjust sp and rp to their new level. */
        lir->insStorei(inner_sp_ins = lir->ins2(LIR_piadd, lirbuf->sp, INS_CONSTWORD(sp_offset)),
                lirbuf->state, offsetof(InterpState, sp));
        lir->insStorei(lir->ins2(LIR_piadd, lirbuf->rp, INS_CONSTWORD(rp_adj)),
                lirbuf->state, offsetof(InterpState, rp));
    }

    /*
     * The inner tree will probably access stack slots. So tell nanojit not to
     * discard or defer stack writes before emitting the call tree code.
     *
     * (The ExitType of this snapshot is nugatory. The exit can't be taken.)
     */
    GuardRecord* guardRec = createGuardRecord(exit);
    lir->insGuard(LIR_xbarrier, NULL, guardRec);
}

static unsigned
BuildGlobalTypeMapFromInnerTree(Queue<JSTraceType>& typeMap, VMSideExit* inner)
{
#if defined DEBUG
    unsigned initialSlots = typeMap.length();
#endif
    /* First, use the innermost exit's global typemap. */
    typeMap.add(inner->globalTypeMap(), inner->numGlobalSlots);

    /* Add missing global types from the innermost exit's tree. */
    TreeInfo* innerTree = inner->root()->treeInfo;
    unsigned slots = inner->numGlobalSlots;
    if (slots < innerTree->nGlobalTypes()) {
        typeMap.add(innerTree->globalTypeMap() + slots, innerTree->nGlobalTypes() - slots);
        slots = innerTree->nGlobalTypes();
    }
    JS_ASSERT(typeMap.length() - initialSlots == slots);
    return slots;
}

/* Record a call to an inner tree. */
JS_REQUIRES_STACK void
TraceRecorder::emitTreeCall(TreeFragment* inner, VMSideExit* exit, LIns* inner_sp_ins)
{
    /* Invoke the inner tree. */
    LIns* args[] = { lirbuf->state }; /* reverse order */
    /* Construct a call info structure for the target tree. */
    CallInfo* ci = new (traceAlloc()) CallInfo();
    ci->_address = uintptr_t(inner->code());
    JS_ASSERT(ci->_address);
    ci->_argtypes = ARGSIZE_P | ARGSIZE_P << ARGSIZE_SHIFT;
    ci->_cse = ci->_fold = 0;
    ci->_abi = ABI_FASTCALL;
#ifdef DEBUG
    ci->_name = "fragment";
#endif
    LIns* rec = lir->insCall(ci, args);
    LIns* lr = lir->insLoad(LIR_ldp, rec, offsetof(GuardRecord, exit));
    LIns* nested = lir->insBranch(LIR_jt,
                                  lir->ins2i(LIR_eq,
                                             lir->insLoad(LIR_ld, lr, offsetof(VMSideExit, exitType)),
                                             NESTED_EXIT),
                                  NULL);

    /*
     * If the tree exits on a regular (non-nested) guard, keep updating lastTreeExitGuard
     * with that guard. If we mismatch on a tree call guard, this will contain the last
     * non-nested guard we encountered, which is the innermost loop or branch guard.
     */
    lir->insStorei(lr, lirbuf->state, offsetof(InterpState, lastTreeExitGuard));
    LIns* done1 = lir->insBranch(LIR_j, NULL, NULL);

    /*
     * The tree exited on a nested guard. This only occurs once a tree call guard mismatches
     * and we unwind the tree call stack. We store the first (innermost) tree call guard in state
     * and we will try to grow the outer tree the failing call was in starting at that guard.
     */
    nested->setTarget(lir->ins0(LIR_label));
    LIns* done2 = lir->insBranch(LIR_jf,
                                 lir->ins_peq0(lir->insLoad(LIR_ldp,
                                                            lirbuf->state,
                                                            offsetof(InterpState, lastTreeCallGuard))),
                                 NULL);
    lir->insStorei(lr, lirbuf->state, offsetof(InterpState, lastTreeCallGuard));
    lir->insStorei(lir->ins2(LIR_piadd,
                             lir->insLoad(LIR_ldp, lirbuf->state, offsetof(InterpState, rp)),
                             lir->ins_i2p(lir->ins2i(LIR_lsh,
                                                     lir->insLoad(LIR_ld, lr, offsetof(VMSideExit, calldepth)),
                                                     sizeof(void*) == 4 ? 2 : 3))),
                   lirbuf->state,
                   offsetof(InterpState, rpAtLastTreeCall));
    LIns* label = lir->ins0(LIR_label);
    done1->setTarget(label);
    done2->setTarget(label);

    /*
     * Keep updating outermostTreeExit so that InterpState always contains the most recent
     * side exit.
     */
    lir->insStorei(lr, lirbuf->state, offsetof(InterpState, outermostTreeExitGuard));

    /* Read back all registers, in case the called tree changed any of them. */
#ifdef DEBUG
    JSTraceType* map;
    size_t i;
    map = exit->globalTypeMap();
    for (i = 0; i < exit->numGlobalSlots; i++)
        JS_ASSERT(map[i] != TT_JSVAL);
    map = exit->stackTypeMap();
    for (i = 0; i < exit->numStackSlots; i++)
        JS_ASSERT(map[i] != TT_JSVAL);
#endif

    /*
     * Bug 502604 - It is illegal to extend from the outer typemap without
     * first extending from the inner. Make a new typemap here.
     */
    TypeMap fullMap(NULL);
    fullMap.add(exit->stackTypeMap(), exit->numStackSlots);
    BuildGlobalTypeMapFromInnerTree(fullMap, exit);

    TreeInfo* ti = inner->treeInfo;
    import(ti, inner_sp_ins, exit->numStackSlots, fullMap.length() - exit->numStackSlots,
           exit->calldepth, fullMap.data());

    /* Restore sp and rp to their original values (we still have them in a register). */
    if (callDepth > 0) {
        lir->insStorei(lirbuf->sp, lirbuf->state, offsetof(InterpState, sp));
        lir->insStorei(lirbuf->rp, lirbuf->state, offsetof(InterpState, rp));
    }

    /*
     * Guard that we come out of the inner tree along the same side exit we came out when
     * we called the inner tree at recording time.
     */
    VMSideExit* nestedExit = snapshot(NESTED_EXIT);
    JS_ASSERT(exit->exitType == LOOP_EXIT);
    guard(true, lir->ins2(LIR_peq, lr, INS_CONSTPTR(exit)), nestedExit);
    debug_only_printf(LC_TMTreeVis, "TREEVIS TREECALL INNER=%p EXIT=%p GUARD=%p\n", (void*)inner,
                      (void*)nestedExit, (void*)exit);

    /* Register us as a dependent tree of the inner tree. */
    inner->treeInfo->dependentTrees.addUnique(fragment->root);
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

/*
 * Invert the direction of the guard if this is a loop edge that is not
 * taken (thin loop).
 */
JS_REQUIRES_STACK void
TraceRecorder::emitIf(jsbytecode* pc, bool cond, LIns* x)
{
    ExitType exitType;
    if (IsLoopEdge(pc, (jsbytecode*)fragment->root->ip)) {
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
        if (x->isconst()) {
            pendingLoop = (x->imm32() == int32(cond));
            return;
        }
    } else {
        exitType = BRANCH_EXIT;
    }
    if (!x->isconst())
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
    if (IsLoopEdge(pc, (jsbytecode*)fragment->root->ip)) {
        /*
         * If we compile a loop, the trace should have a zero stack balance at
         * the loop edge. Currently we are parked on a comparison op or
         * IFNE/IFEQ, so advance pc to the loop header and adjust the stack
         * pointer and pretend we have reached the loop header.
         */
        if (pendingLoop) {
            JS_ASSERT(!cx->fp->imacpc && (pc == cx->fp->regs->pc || pc == cx->fp->regs->pc + 1));
            bool fused = pc != cx->fp->regs->pc;
            JSFrameRegs orig = *cx->fp->regs;

            cx->fp->regs->pc = (jsbytecode*)fragment->root->ip;
            cx->fp->regs->sp -= fused ? 2 : 1;

            JSContext* localcx = cx;
            AbortableRecordingStatus ars = closeLoop();
            *localcx->fp->regs = orig;
            return ars;
        } else {
            return endLoop();
        }
    }
    return ARECORD_CONTINUE;
}

bool
TraceRecorder::hasMethod(JSObject* obj, jsid id)
{
    if (!obj)
        return false;

    JSObject* pobj;
    JSProperty* prop;
    int protoIndex = obj->lookupProperty(cx, id, &pobj, &prop);
    if (protoIndex < 0 || !prop)
        return false;

    bool found = false;
    if (OBJ_IS_NATIVE(pobj)) {
        JSScope* scope = OBJ_SCOPE(pobj);
        JSScopeProperty* sprop = (JSScopeProperty*) prop;

        if (SPROP_HAS_STUB_GETTER_OR_IS_METHOD(sprop) &&
            SPROP_HAS_VALID_SLOT(sprop, scope)) {
            jsval v = LOCKED_OBJ_GET_SLOT(pobj, sprop->slot);
            if (VALUE_IS_FUNCTION(cx, v)) {
                found = true;
                if (!scope->branded()) {
                    scope->brandingShapeChange(cx, sprop->slot, v);
                    scope->setBranded();
                }
            }
        }
    }

    pobj->dropProperty(cx, prop);
    return found;
}

JS_REQUIRES_STACK bool
TraceRecorder::hasIteratorMethod(JSObject* obj)
{
    JS_ASSERT(cx->fp->regs->sp + 2 <= cx->fp->slots + cx->fp->script->nslots);

    return hasMethod(obj, ATOM_TO_JSID(cx->runtime->atomState.iteratorAtom));
}

void
nanojit::StackFilter::getTops(LIns* guard, int& spTop, int& rpTop)
{
    VMSideExit* e = (VMSideExit*)guard->record()->exit;
    spTop = e->sp_adj;
    rpTop = e->rp_adj;
}

#if defined NJ_VERBOSE
void
nanojit::LirNameMap::formatGuard(LIns *i, char *out)
{
    VMSideExit *x;

    x = (VMSideExit *)i->record()->exit;
    sprintf(out,
            "%s: %s %s -> pc=%p imacpc=%p sp%+ld rp%+ld (GuardID=%03d)",
            formatRef(i),
            lirNames[i->opcode()],
            i->oprnd1() ? formatRef(i->oprnd1()) : "",
            (void *)x->pc,
            (void *)x->imacpc,
            (long int)x->sp_adj,
            (long int)x->rp_adj,
            i->record()->profGuardID);
}
#endif

/*
 * Check whether the shape of the global object has changed. The return value
 * indicates whether the recorder is still active.  If 'false', any active
 * recording has been aborted and the JIT may have been reset.
 */
static JS_REQUIRES_STACK bool
CheckGlobalObjectShape(JSContext* cx, JSTraceMonitor* tm, JSObject* globalObj,
                       uint32 *shape = NULL, SlotList** slots = NULL)
{
    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return false;
    }

    if (STOBJ_NSLOTS(globalObj) > MAX_GLOBAL_SLOTS) {
        if (tm->recorder)
            js_AbortRecording(cx, "too many slots in global object");
        return false;
    }

    uint32 globalShape = OBJ_SHAPE(globalObj);

    if (tm->recorder) {
        TreeFragment* root = tm->recorder->getFragment()->root;
        TreeInfo* ti = tm->recorder->getTreeInfo();

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
            *slots = ti->globalSlots;
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
                             TreeInfo* ti, unsigned stackSlots, unsigned ngslots,
                             JSTraceType* typeMap, VMSideExit* expectedInnerExit,
                             jsbytecode* outer, uint32 outerArgc, RecordReason recordReason)
{
    JSTraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(!tm->needFlush);
    JS_ASSERT_IF(cx->fp->imacpc, f->root != f);

    tm->recorder = new TraceRecorder(cx, anchor, f, ti, stackSlots,
                                     ngslots, typeMap, expectedInnerExit,
                                     outer, outerArgc, recordReason);

    if (!tm->recorder || tm->outOfMemory() || js_OverfullJITCache(tm)) {
        ResetJIT(cx, FR_OOM);
        return false;
    }

    /*
     * If slurping failed, there's no reason to start recording again. Emit LIR
     * to capture the rest of the slots, then immediately compile and finish.
     */
    if (anchor && anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT) {
        tm->recorder->slurpDownFrames((jsbytecode*)anchor->recursive_pc - JSOP_CALL_LENGTH);
        if (tm->recorder)
            tm->recorder->finishAbort("Failed to slurp down frames");
        return false;
    }

    return true;
}

static void
TrashTree(JSContext* cx, TreeFragment* f)
{
    JS_ASSERT((!f->code()) == (!f->treeInfo));
    JS_ASSERT(f == f->root);
    debug_only_printf(LC_TMTreeVis, "TREEVIS TRASH FRAG=%p\n", (void*)f);

    if (!f->code())
        return;
    AUDIT(treesTrashed);
    debug_only_print0(LC_TMTracer, "Trashing tree info.\n");
    TreeInfo* ti = f->treeInfo;
    f->treeInfo = NULL;
    f->setCode(NULL);
    TreeFragment** data = ti->dependentTrees.data();
    unsigned length = ti->dependentTrees.length();
    for (unsigned n = 0; n < length; ++n)
        TrashTree(cx, data[n]);
    data = ti->linkedTrees.data();
    length = ti->linkedTrees.length();
    for (unsigned n = 0; n < length; ++n)
        TrashTree(cx, data[n]);
}

static int
SynthesizeFrame(JSContext* cx, const FrameInfo& fi, JSObject* callee)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, callee);
    JS_ASSERT(FUN_INTERPRETED(fun));

    /* Assert that we have a correct sp distance from cx->fp->slots in fi. */
    JSStackFrame* fp = cx->fp;
    JS_ASSERT_IF(!fi.imacpc,
                 js_ReconstructStackDepth(cx, fp->script, fi.pc) ==
                 uintN(fi.spdist - fp->script->nfixed));

    uintN nframeslots = JS_HOWMANY(sizeof(JSInlineFrame), sizeof(jsval));
    JSScript* script = fun->u.i.script;
    size_t nbytes = (nframeslots + script->nslots) * sizeof(jsval);

    /* Code duplicated from inline_call: case in js_Interpret (FIXME). */
    JSArena* a = cx->stackPool.current;
    void* newmark = (void*) a->avail;
    uintN argc = fi.get_argc();
    jsval* vp = fp->slots + fi.spdist - (2 + argc);
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
        JS_ARENA_ALLOCATE_CAST(newsp, jsval *, &cx->stackPool, nbytes);
        if (!newsp)
            OutOfMemoryAbort();

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
    newifp->frame.fun = fun;

    bool constructing = fi.is_constructing();
    newifp->frame.argc = argc;
    newifp->callerRegs.pc = fi.pc;
    newifp->callerRegs.sp = fp->slots + fi.spdist;
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
    newifp->frame.scopeChain = NULL; // will be updated in FlushNativeStackFrame
    newifp->frame.flags = constructing ? JSFRAME_CONSTRUCTING : 0;
    newifp->frame.dormantNext = NULL;
    newifp->frame.blockChain = NULL;
    newifp->mark = newmark;
    newifp->frame.thisv = JSVAL_NULL; // will be updated in FlushNativeStackFrame

    newifp->frame.regs = fp->regs;
    newifp->frame.regs->pc = script->code;
    newifp->frame.regs->sp = newsp + script->nfixed;
    newifp->frame.imacpc = NULL;
    newifp->frame.slots = newsp;
    if (script->staticLevel < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[script->staticLevel];
        newifp->frame.displaySave = *disp;
        *disp = &newifp->frame;
    }

    /*
     * Note that fp->script is still the caller's script; set the callee
     * inline frame's idea of caller version from its version.
     */
    newifp->callerVersion = (JSVersion) fp->script->version;

    // After this paragraph, fp and cx->fp point to the newly synthesized frame.
    fp->regs = &newifp->callerRegs;
    fp = cx->fp = &newifp->frame;

    /*
     * If there's a call hook, invoke it to compute the hookData used by
     * debuggers that cooperate with the interpreter.
     */
    JSInterpreterHook hook = cx->debugHooks->callHook;
    if (hook) {
        newifp->hookData = hook(cx, fp, JS_TRUE, 0, cx->debugHooks->callHookData);
    } else {
        newifp->hookData = NULL;
    }

    /*
     * Duplicate native stack layout computation: see VisitFrameSlots header comment.
     *
     * FIXME - We must count stack slots from caller's operand stack up to (but
     * not including) callee's, including missing arguments. Could we shift
     * everything down to the caller's fp->slots (where vars start) and avoid
     * some of the complexity?
     */
    return (fi.spdist - fp->down->script->nfixed) +
           ((fun->nargs > fp->argc) ? fun->nargs - fp->argc : 0) +
           script->nfixed + 1/*argsobj*/;
}

static void
SynthesizeSlowNativeFrame(InterpState& state, JSContext *cx, VMSideExit *exit)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    void *mark;
    JSInlineFrame *ifp;

    /* This allocation is infallible: ExecuteTree reserved enough stack. */
    mark = JS_ARENA_MARK(&cx->stackPool);
    JS_ARENA_ALLOCATE_CAST(ifp, JSInlineFrame *, &cx->stackPool, sizeof(JSInlineFrame));
    if (!ifp)
        OutOfMemoryAbort();

    JSStackFrame *fp = &ifp->frame;
    fp->regs = NULL;
    fp->imacpc = NULL;
    fp->slots = NULL;
    fp->callobj = NULL;
    fp->argsobj = NULL;
    fp->varobj = cx->fp->varobj;
    fp->script = NULL;
    fp->thisv = state.nativeVp[1];
    fp->argc = state.nativeVpLen - 2;
    fp->argv = state.nativeVp + 2;
    fp->fun = GET_FUNCTION_PRIVATE(cx, fp->calleeObject());
    fp->rval = JSVAL_VOID;
    fp->down = cx->fp;
    fp->annotation = NULL;
    JS_ASSERT(cx->fp->scopeChain);
    fp->scopeChain = cx->fp->scopeChain;
    fp->blockChain = NULL;
    fp->flags = exit->constructing() ? JSFRAME_CONSTRUCTING : 0;
    fp->dormantNext = NULL;
    fp->displaySave = NULL;

    ifp->mark = mark;
    cx->fp = fp;
}

/*
 * Create a TreeInfo in preparation for starting a recorder. If one cannot be
 * allocated, reset the JIT and return NULL.
 */
static JS_REQUIRES_STACK TreeInfo*
CreateTreeInfo(JSContext* cx, TreeFragment* f, JSObject* globalObj, SlotList* globalSlots)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Set up the VM-private treeInfo structure for this fragment. */
    TreeInfo* ti = new (*tm->traceAlloc) TreeInfo(tm->dataAlloc, f, globalSlots);

    /* Capture the coerced type of each active slot in the type map. */
    ti->typeMap.captureTypes(cx, globalObj, *globalSlots, 0 /* callDepth */);
    ti->nStackTypes = ti->typeMap.length() - globalSlots->length();

#ifdef DEBUG
    AssertTreeIsUnique(tm, f, ti);
    ti->treeFileName = cx->fp->script->filename;
    ti->treeLineNumber = js_FramePCToLineNumber(cx, cx->fp);
    ti->treePCOffset = FramePCOffset(cx->fp);
#endif
#ifdef JS_JIT_SPEW
    debug_only_printf(LC_TMTreeVis, "TREEVIS CREATETREE ROOT=%p PC=%p FILE=\"%s\" LINE=%d OFFS=%d",
                      (void*)f, f->ip, ti->treeFileName, ti->treeLineNumber,
                      FramePCOffset(cx->fp));
    debug_only_print0(LC_TMTreeVis, " STACK=\"");
    for (unsigned i = 0; i < ti->nStackTypes; i++)
        debug_only_printf(LC_TMTreeVis, "%c", typeChar[ti->typeMap[i]]);
    debug_only_print0(LC_TMTreeVis, "\" GLOBALS=\"");
    for (unsigned i = 0; i < ti->nGlobalTypes(); i++)
        debug_only_printf(LC_TMTreeVis, "%c", typeChar[ti->typeMap[ti->nStackTypes + i]]);
    debug_only_print0(LC_TMTreeVis, "\"\n");
#endif

    /* Determine the native frame layout at the entry point. */
    unsigned entryNativeStackSlots = ti->nStackTypes;
    JS_ASSERT(entryNativeStackSlots == NativeStackSlots(cx, 0 /* callDepth */));
    ti->nativeStackBase = (entryNativeStackSlots -
            (cx->fp->regs->sp - StackBase(cx->fp))) * sizeof(double);
    ti->maxNativeStackSlots = entryNativeStackSlots;
    ti->maxCallDepth = 0;
    ti->script = cx->fp->script;

    return ti;
}

static JS_REQUIRES_STACK bool
RecordTree(JSContext* cx, JSTraceMonitor* tm, TreeFragment* peer, jsbytecode* outer,
           uint32 outerArgc, JSObject* globalObj, uint32 globalShape,
           SlotList* globalSlots, uint32 argc, RecordReason reason)
{
    /* Try to find an unused peer fragment, or allocate a new one. */
    TreeFragment* f = peer;
    while (f->code() && f->peer)
        f = f->peer;
    if (f->code())
        f = AddNewPeerToPeerList(tm, f);
    JS_ASSERT(f->root == f);

    /* save a local copy for use after JIT flush */
    const void* localRootIP = f->root->ip;

    /* Make sure the global type map didn't change on us. */
    if (!CheckGlobalObjectShape(cx, tm, globalObj)) {
        Backoff(cx, (jsbytecode*) localRootIP);
        return false;
    }

    AUDIT(recorderStarted);

    if (tm->outOfMemory() || js_OverfullJITCache(tm)) {
        Backoff(cx, (jsbytecode*) f->root->ip);
        ResetJIT(cx, FR_OOM);
        debug_only_print0(LC_TMTracer,
                          "Out of memory recording new tree, flushing cache.\n");
        return false;
    }

    JS_ASSERT(!f->code() && !f->treeInfo);

    TreeInfo* ti = CreateTreeInfo(cx, f, globalObj, globalSlots);
    if (!ti)
        return false;

    /* Recording primary trace. */
    return TraceRecorder::startRecorder(cx, NULL, f, ti,
                                        ti->nStackTypes,
                                        ti->globalSlots->length(),
                                        ti->typeMap.data(), NULL,
                                        outer, outerArgc, reason);
}

static JS_REQUIRES_STACK TypeConsensus
FindLoopEdgeTarget(JSContext* cx, VMSideExit* exit, TreeFragment** peerp)
{
    TreeFragment* from = exit->root();
    TreeInfo* from_ti = from->treeInfo;

    JS_ASSERT(from->code());

    TypeMap typeMap(NULL);
    FullMapFromExit(typeMap, exit);
    JS_ASSERT(typeMap.length() - exit->numStackSlots == from_ti->nGlobalTypes());

    /* Mark all double slots as undemotable */
    uint16* gslots = from_ti->globalSlots->data();
    for (unsigned i = 0; i < typeMap.length(); i++) {
        if (typeMap[i] == TT_DOUBLE) {
            if (exit->exitType == RECURSIVE_UNLINKED_EXIT) {
                if (i < exit->numStackSlots)
                    oracle.markStackSlotUndemotable(cx, i, exit->recursive_pc);
                else
                    oracle.markGlobalSlotUndemotable(cx, gslots[i - exit->numStackSlots]);
            }
            if (i < from_ti->nStackTypes)
                oracle.markStackSlotUndemotable(cx, i, from->ip);
            else if (i >= exit->numStackSlots)
                oracle.markGlobalSlotUndemotable(cx, gslots[i - exit->numStackSlots]);
        }
    }

    JS_ASSERT(exit->exitType == UNSTABLE_LOOP_EXIT ||
              (exit->exitType == RECURSIVE_UNLINKED_EXIT && exit->recursive_pc));

    TreeFragment* firstPeer = NULL;
    if (exit->exitType == UNSTABLE_LOOP_EXIT || exit->recursive_pc == from->ip) {
        firstPeer = from->first;
    } else {
        firstPeer = LookupLoop(&JS_TRACE_MONITOR(cx), exit->recursive_pc, from->globalObj,
                               from->globalShape, from->argc);
    }

    for (TreeFragment* peer = firstPeer; peer; peer = peer->peer) {
        TreeInfo* peer_ti = peer->treeInfo;
        if (!peer_ti)
            continue;
        JS_ASSERT(peer->argc == from->argc);
        JS_ASSERT(exit->numStackSlots == peer_ti->nStackTypes);
        TypeConsensus consensus = TypeMapLinkability(cx, typeMap, peer);
        if (consensus == TypeConsensus_Okay || consensus == TypeConsensus_Undemotes) {
            *peerp = peer;
            return consensus;
        }
    }

    return TypeConsensus_Bad;
}

UnstableExit*
TreeInfo::removeUnstableExit(VMSideExit* exit)
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

static JS_REQUIRES_STACK bool
AttemptToStabilizeTree(JSContext* cx, JSObject* globalObj, VMSideExit* exit, jsbytecode* outer,
                       uint32 outerArgc)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return false;
    }

    TreeFragment* from = exit->root();
    TreeInfo* from_ti = from->treeInfo;

    TreeFragment* peer = NULL;
    TypeConsensus consensus = FindLoopEdgeTarget(cx, exit, &peer);
    if (consensus == TypeConsensus_Okay) {
        TreeInfo* peer_ti = peer->treeInfo;
        JS_ASSERT(from_ti->globalSlots == peer_ti->globalSlots);
        JS_ASSERT_IF(exit->exitType == UNSTABLE_LOOP_EXIT,
                     from_ti->nStackTypes == peer_ti->nStackTypes);
        JS_ASSERT(exit->numStackSlots == peer_ti->nStackTypes);
        /* Patch this exit to its peer */
        JoinPeers(tm->assembler, exit, peer);
        /*
         * Update peer global types. The |from| fragment should already be updated because it on
         * the execution path, and somehow connected to the entry trace.
         */
        if (peer_ti->nGlobalTypes() < peer_ti->globalSlots->length())
            SpecializeTreesToMissingGlobals(cx, globalObj, peer_ti);
        JS_ASSERT(from_ti->nGlobalTypes() == from_ti->globalSlots->length());
        /* This exit is no longer unstable, so remove it. */
        if (exit->exitType == UNSTABLE_LOOP_EXIT)
            from_ti->removeUnstableExit(exit);
        debug_only_stmt(DumpPeerStability(tm, peer->ip, from->globalObj, from->globalShape, from->argc);)
        return false;
    } else if (consensus == TypeConsensus_Undemotes) {
        /* The original tree is unconnectable, so trash it. */
        TrashTree(cx, peer);
        return false;
    }

    /* Don't bother recording if the exit doesn't expect this PC */
    if (exit->exitType == RECURSIVE_UNLINKED_EXIT) {
        if (++exit->hitcount >= MAX_RECURSIVE_UNLINK_HITS) {
            Blacklist((jsbytecode*)from->ip);
            TrashTree(cx, from);
            return false;
        }
        if (exit->recursive_pc != cx->fp->regs->pc)
            return false;
        from = LookupLoop(tm, exit->recursive_pc, from->globalObj, from->globalShape, cx->fp->argc);
        if (!from)
            return false;
        /* use stale TI for RecordTree - since from might not have one anymore. */
    }

    JS_ASSERT(from == from->root);

    /* If this tree has been blacklisted, don't try to record a new one. */
    if (*(jsbytecode*)from->ip == JSOP_NOP)
        return false;

    return RecordTree(cx, tm, from->first, outer, outerArgc, from->globalObj,
                      from->globalShape, from_ti->globalSlots, cx->fp->argc,
                      Record_Branch);
}

static JS_REQUIRES_STACK VMFragment*
CreateBranchFragment(JSContext* cx, TreeFragment* root, VMSideExit* anchor)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    verbose_only(
    uint32_t profFragID = (js_LogController.lcbits & LC_FragProfile)
                          ? (++(tm->lastFragID)) : 0;
    )

    VMFragment* f = new (*tm->dataAlloc) VMFragment(cx->fp->regs->pc verbose_only(, profFragID));

    debug_only_printf(LC_TMTreeVis, "TREEVIS CREATEBRANCH ROOT=%p FRAG=%p PC=%p FILE=\"%s\""
                      " LINE=%d ANCHOR=%p OFFS=%d\n",
                      (void*)root, (void*)f, (void*)cx->fp->regs->pc, cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp), (void*)anchor,
                      FramePCOffset(cx->fp));
    verbose_only( tm->branches = new (*tm->dataAlloc) Seq<Fragment*>(f, tm->branches); )

    f->root = root;
    if (anchor)
        anchor->target = f;
    return f;
}

static JS_REQUIRES_STACK bool
AttemptToExtendTree(JSContext* cx, VMSideExit* anchor, VMSideExit* exitedFrom, jsbytecode* outer
#ifdef MOZ_TRACEVIS
    , TraceVisStateObj* tvso = NULL
#endif
    )
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    JS_ASSERT(!tm->recorder);

    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
#ifdef MOZ_TRACEVIS
        if (tvso) tvso->r = R_FAIL_EXTEND_FLUSH;
#endif
        return false;
    }

    TreeFragment* f = anchor->root();
    JS_ASSERT(f->treeInfo);

    /*
     * Don't grow trees above a certain size to avoid code explosion due to
     * tail duplication.
     */
    if (f->branchCount >= MAX_BRANCHES) {
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
        c->ip = cx->fp->regs->pc;
        JS_ASSERT(c->root == f);
    }

    debug_only_printf(LC_TMTracer,
                      "trying to attach another branch to the tree (hits = %d)\n", c->hits());

    int32_t& hits = c->hits();
    if (outer || (hits++ >= HOTEXIT && hits <= HOTEXIT+MAXEXIT)) {
        /* start tracing secondary trace from this point */
        unsigned stackSlots;
        unsigned ngslots;
        JSTraceType* typeMap;
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
        bool rv = TraceRecorder::startRecorder(cx, anchor, c, f->treeInfo,
                                               stackSlots, ngslots, typeMap, exitedFrom,
                                               outer, cx->fp->argc, Record_Branch);
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

static JS_REQUIRES_STACK VMSideExit*
ExecuteTree(JSContext* cx, TreeFragment* f, uintN& inlineCallCount,
            VMSideExit** innermostNestedGuardp);

JS_REQUIRES_STACK bool
TraceRecorder::recordLoopEdge(JSContext* cx, TraceRecorder* r, uintN& inlineCallCount)
{
#ifdef JS_THREADSAFE
    if (OBJ_SCOPE(JS_GetGlobalForObject(cx, cx->fp->scopeChain))->title.ownercx != cx) {
        js_AbortRecording(cx, "Global object not owned by this context");
        return false; /* we stay away from shared global objects */
    }
#endif

    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Process needFlush and deep abort requests. */
    if (tm->needFlush) {
        ResetJIT(cx, FR_DEEP_BAIL);
        return false;
    }

    JS_ASSERT(r->fragment && !r->fragment->lastIns);
    TreeFragment* root = r->fragment->root;
    TreeFragment* first = LookupOrAddLoop(tm, cx->fp->regs->pc, root->globalObj,
                                        root->globalShape, cx->fp->argc);

    /*
     * Make sure the shape of the global object still matches (this might flush
     * the JIT cache).
     */
    JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    uint32 globalShape = -1;
    SlotList* globalSlots = NULL;
    if (!CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots)) {
        JS_ASSERT(!tm->recorder);
        return false;
    }

    debug_only_printf(LC_TMTracer,
                      "Looking for type-compatible peer (%s:%d@%d)\n",
                      cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp),
                      FramePCOffset(cx->fp));

    // Find a matching inner tree. If none can be found, compile one.
    TreeFragment* f = r->findNestedCompatiblePeer(first);
    if (!f || !f->code()) {
        AUDIT(noCompatInnerTrees);

        TreeFragment* outerFragment = root;
        jsbytecode* outer = (jsbytecode*) outerFragment->ip;
        uint32 outerArgc = outerFragment->argc;
        uint32 argc = cx->fp->argc;
        js_AbortRecording(cx, "No compatible inner tree");

        return RecordTree(cx, tm, first, outer, outerArgc, globalObj, globalShape,
                          globalSlots, argc, Record_Branch);
    }

    return r->attemptTreeCall(f, inlineCallCount) == ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::attemptTreeCall(TreeFragment* f, uintN& inlineCallCount)
{
    /*
     * It is absolutely forbidden to have recursive loops tree call themselves
     * because it could accidentally pop frames owned by the parent call, and
     * there is no way to deal with this yet. We could have to set a "start of
     * poppable rp stack" variable, and if that unequals "real start of rp stack",
     * it would be illegal to pop frames.
     * --
     * In the interim, just do tree calls knowing that they won't go into
     * recursive trees that can pop parent frames.
     */
    if (f->treeInfo->script == cx->fp->script) {
        if (f->treeInfo->recursion >= Recursion_Unwinds) {
            Blacklist(cx->fp->script->code);
            js_AbortRecording(cx, "Inner tree is an unsupported type of recursion");
            return ARECORD_ABORTED;
        } else {
            f->treeInfo->recursion = Recursion_Disallowed;
        }
    }

    adjustCallerTypes(f);
    LIns* inner_sp_ins;
    prepareTreeCall(f, inner_sp_ins);

#ifdef DEBUG
    unsigned oldInlineCallCount = inlineCallCount;
#endif

    JSContext *localCx = cx;

    VMSideExit* innermostNestedGuard = NULL;
    VMSideExit* lr = ExecuteTree(cx, f, inlineCallCount, &innermostNestedGuard);

    /* ExecuteTree can reenter the interpreter and kill |this|. */
    if (!TRACE_RECORDER(localCx))
        return ARECORD_ABORTED;

    if (!lr) {
        js_AbortRecording(cx, "Couldn't call inner tree");
        return ARECORD_ABORTED;
    }

    TreeFragment* outerFragment = fragment->root;
    jsbytecode* outer = (jsbytecode*) outerFragment->ip;
    switch (lr->exitType) {
      case RECURSIVE_LOOP_EXIT:
      case LOOP_EXIT:
        /* If the inner tree exited on an unknown loop exit, grow the tree around it. */
        if (innermostNestedGuard) {
            js_AbortRecording(cx, "Inner tree took different side exit, abort current "
                              "recording and grow nesting tree");
            return AttemptToExtendTree(localCx, innermostNestedGuard, lr, outer) ?
                ARECORD_CONTINUE : ARECORD_ABORTED;
        }

        JS_ASSERT(oldInlineCallCount == inlineCallCount);

        /* Emit a call to the inner tree and continue recording the outer tree trace. */
        emitTreeCall(f, lr, inner_sp_ins);
        return ARECORD_CONTINUE;

      case UNSTABLE_LOOP_EXIT:
      {
        /* Abort recording so the inner loop can become type stable. */
        JSObject* _globalObj = globalObj;
        js_AbortRecording(cx, "Inner tree is trying to stabilize, abort outer recording");
        return AttemptToStabilizeTree(localCx, _globalObj, lr, outer, outerFragment->argc) ?
            ARECORD_CONTINUE : ARECORD_ABORTED;
      }

      case OVERFLOW_EXIT:
        oracle.markInstructionUndemotable(cx->fp->regs->pc);
        /* FALL THROUGH */
      case RECURSIVE_SLURP_FAIL_EXIT:
      case RECURSIVE_SLURP_MISMATCH_EXIT:
      case RECURSIVE_MISMATCH_EXIT:
      case RECURSIVE_EMPTY_RP_EXIT:
      case BRANCH_EXIT:
      case CASE_EXIT: {
          /* Abort recording the outer tree, extend the inner tree. */
          js_AbortRecording(cx, "Inner tree is trying to grow, abort outer recording");
          return AttemptToExtendTree(localCx, lr, NULL, outer) ? ARECORD_CONTINUE : ARECORD_ABORTED;
      }

      case NESTED_EXIT:
          JS_NOT_REACHED("NESTED_EXIT should be replaced by innermost side exit");
      default:
        debug_only_printf(LC_TMTracer, "exit_type=%s\n", getExitName(lr->exitType));
        js_AbortRecording(cx, "Inner tree not suitable for calling");
        return ARECORD_ABORTED;
    }
}

static bool
IsEntryTypeCompatible(jsval* vp, JSTraceType* m)
{
    unsigned tag = JSVAL_TAG(*vp);

    debug_only_printf(LC_TMTracer, "%c/%c ", tagChar[tag], typeChar[*m]);

    switch (*m) {
      case TT_OBJECT:
        if (tag == JSVAL_OBJECT && !JSVAL_IS_NULL(*vp) &&
            !HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(*vp))) {
            return true;
        }
        debug_only_printf(LC_TMTracer, "object != tag%u ", tag);
        return false;
      case TT_INT32:
        jsint i;
        if (JSVAL_IS_INT(*vp))
            return true;
        if (tag == JSVAL_DOUBLE && JSDOUBLE_IS_INT(*JSVAL_TO_DOUBLE(*vp), i))
            return true;
        debug_only_printf(LC_TMTracer, "int != tag%u(value=%lu) ", tag, (unsigned long)*vp);
        return false;
      case TT_DOUBLE:
        if (JSVAL_IS_INT(*vp) || tag == JSVAL_DOUBLE)
            return true;
        debug_only_printf(LC_TMTracer, "double != tag%u ", tag);
        return false;
      case TT_JSVAL:
        JS_NOT_REACHED("shouldn't see jsval type in entry");
        return false;
      case TT_STRING:
        if (tag == JSVAL_STRING)
            return true;
        debug_only_printf(LC_TMTracer, "string != tag%u ", tag);
        return false;
      case TT_NULL:
        if (JSVAL_IS_NULL(*vp))
            return true;
        debug_only_printf(LC_TMTracer, "null != tag%u ", tag);
        return false;
      case TT_PSEUDOBOOLEAN:
        if (tag == JSVAL_SPECIAL)
            return true;
        debug_only_printf(LC_TMTracer, "bool != tag%u ", tag);
        return false;
      default:
        JS_ASSERT(*m == TT_FUNCTION);
        if (tag == JSVAL_OBJECT && !JSVAL_IS_NULL(*vp) &&
            HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(*vp))) {
            return true;
        }
        debug_only_printf(LC_TMTracer, "fun != tag%u ", tag);
        return false;
    }
}

class TypeCompatibilityVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
    JSContext *mCx;
    JSTraceType *mTypeMap;
    unsigned mStackSlotNum;
    bool mOk;
public:
    TypeCompatibilityVisitor (TraceRecorder &recorder,
                              JSTraceType *typeMap) :
        mRecorder(recorder),
        mCx(mRecorder.cx),
        mTypeMap(typeMap),
        mStackSlotNum(0),
        mOk(true)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        debug_only_printf(LC_TMTracer, "global%d=", n);
        if (!IsEntryTypeCompatible(vp, mTypeMap)) {
            mOk = false;
        } else if (!isPromoteInt(mRecorder.get(vp)) && *mTypeMap == TT_INT32) {
            oracle.markGlobalSlotUndemotable(mCx, slot);
            mOk = false;
        } else if (JSVAL_IS_INT(*vp) && *mTypeMap == TT_DOUBLE) {
            oracle.markGlobalSlotUndemotable(mCx, slot);
        }
        mTypeMap++;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            debug_only_printf(LC_TMTracer, "%s%u=", stackSlotKind(), unsigned(i));
            if (!IsEntryTypeCompatible(vp, mTypeMap)) {
                mOk = false;
            } else if (!isPromoteInt(mRecorder.get(vp)) && *mTypeMap == TT_INT32) {
                oracle.markStackSlotUndemotable(mCx, mStackSlotNum);
                mOk = false;
            } else if (JSVAL_IS_INT(*vp) && *mTypeMap == TT_DOUBLE) {
                oracle.markStackSlotUndemotable(mCx, mStackSlotNum);
            }
            vp++;
            mTypeMap++;
            mStackSlotNum++;
        }
        return true;
    }

    bool isOk() {
        return mOk;
    }
};

JS_REQUIRES_STACK TreeFragment*
TraceRecorder::findNestedCompatiblePeer(TreeFragment* f)
{
    JSTraceMonitor* tm;

    tm = &JS_TRACE_MONITOR(cx);
    unsigned int ngslots = treeInfo->globalSlots->length();

    TreeInfo* ti;
    for (; f != NULL; f = f->peer) {
        if (!f->code())
            continue;

        ti = f->treeInfo;

        debug_only_printf(LC_TMTracer, "checking nested types %p: ", (void*)f);

        if (ngslots > ti->nGlobalTypes())
            SpecializeTreesToMissingGlobals(cx, globalObj, ti);

        /*
         * Determine whether the typemap of the inner tree matches the outer
         * tree's current state. If the inner tree expects an integer, but the
         * outer tree doesn't guarantee an integer for that slot, we mark the
         * slot undemotable and mismatch here. This will force a new tree to be
         * compiled that accepts a double for the slot. If the inner tree
         * expects a double, but the outer tree has an integer, we can proceed,
         * but we mark the location undemotable.
         */
        TypeCompatibilityVisitor visitor(*this, ti->typeMap.data());
        VisitSlots(visitor, cx, 0, *treeInfo->globalSlots);

        debug_only_printf(LC_TMTracer, " %s\n", visitor.isOk() ? "match" : "");
        if (visitor.isOk())
            return f;
    }

    return NULL;
}

class CheckEntryTypeVisitor : public SlotVisitorBase
{
    bool mOk;
    JSTraceType *mTypeMap;
public:
    CheckEntryTypeVisitor(JSTraceType *typeMap) :
        mOk(true),
        mTypeMap(typeMap)
    {}

    JS_ALWAYS_INLINE void checkSlot(jsval *vp, char const *name, int i) {
        debug_only_printf(LC_TMTracer, "%s%d=", name, i);
        JS_ASSERT(*(uint8_t*)mTypeMap != 0xCD);
        mOk = IsEntryTypeCompatible(vp, mTypeMap++);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE void
    visitGlobalSlot(jsval *vp, unsigned n, unsigned slot) {
        if (mOk)
            checkSlot(vp, "global", n);
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(jsval *vp, size_t count, JSStackFrame* fp) {
        for (size_t i = 0; i < count; ++i) {
            if (!mOk)
                break;
            checkSlot(vp++, stackSlotKind(), i);
        }
        return mOk;
    }

    bool isOk() {
        return mOk;
    }
};

/**
 * Check if types are usable for trace execution.
 *
 * @param cx            Context.
 * @param ti            Tree info of peer we're testing.
 * @return              True if compatible (with or without demotions), false otherwise.
 */
static JS_REQUIRES_STACK bool
CheckEntryTypes(JSContext* cx, JSObject* globalObj, TreeInfo* ti)
{
    unsigned int ngslots = ti->globalSlots->length();

    JS_ASSERT(ti->nStackTypes == NativeStackSlots(cx, 0));

    if (ngslots > ti->nGlobalTypes())
        SpecializeTreesToMissingGlobals(cx, globalObj, ti);

    JS_ASSERT(ti->typeMap.length() == NativeStackSlots(cx, 0) + ngslots);
    JS_ASSERT(ti->typeMap.length() == ti->nStackTypes + ngslots);
    JS_ASSERT(ti->nGlobalTypes() == ngslots);

    CheckEntryTypeVisitor visitor(ti->typeMap.data());
    VisitSlots(visitor, cx, 0, *ti->globalSlots);

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
        if (!f->treeInfo)
            continue;
        debug_only_printf(LC_TMTracer,
                          "checking vm types %p (ip: %p): ", (void*)f, f->ip);
        if (CheckEntryTypes(cx, globalObj, f->treeInfo))
            return f;
        ++count;
    }
    return NULL;
}

/*
 * For the native stacks and global frame, reuse the storage in |tm->storage|.
 * This reuse depends on the invariant that only one trace uses |tm->storage| at
 * a time. This is subtley correct in lieu of deep bail; see comment for
 * |deepBailSp| in js_DeepBail.
 */
JS_ALWAYS_INLINE
InterpState::InterpState(JSContext* cx, JSTraceMonitor* tm, TreeInfo* ti,
                         uintN& inlineCallCount, VMSideExit** innermostNestedGuardp)
  : cx(cx),
    stackBase(tm->storage.stack()),
    sp(stackBase + ti->nativeStackBase / sizeof(double)),
    eos(tm->storage.global()),
    callstackBase(tm->storage.callstack()),
    sor(callstackBase),
    rp(callstackBase),
    eor(callstackBase + JS_MIN(MAX_CALL_STACK_ENTRIES,
                               JS_MAX_INLINE_CALL_COUNT - inlineCallCount)),
    lastTreeExitGuard(NULL),
    lastTreeCallGuard(NULL),
    rpAtLastTreeCall(NULL),
    outermostTree(ti),
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
    prev = cx->interpState;
    cx->interpState = this;

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
    memset(tm->storage.stack(), 0xCD, MAX_NATIVE_STACK_SLOTS * sizeof(double));
    memset(tm->storage.callstack(), 0xCD, MAX_CALL_STACK_ENTRIES * sizeof(FrameInfo*));
#endif
}

JS_ALWAYS_INLINE
InterpState::~InterpState()
{
    JS_ASSERT(!nativeVp);

    cx->interpState = prev;
    JS_TRACE_MONITOR(cx).tracecx = NULL;
}

/* Call |f|, return the exit taken. */
static JS_ALWAYS_INLINE VMSideExit*
ExecuteTrace(JSContext* cx, Fragment* f, InterpState& state)
{
    JS_ASSERT(!cx->bailExit);
    union { NIns *code; GuardRecord* (FASTCALL *func)(InterpState*); } u;
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
ScopeChainCheck(JSContext* cx, TreeInfo* ti, TreeFragment* f)
{
    JS_ASSERT(ti->globalObj() == f->globalObj);
    JS_ASSERT(ti->globalObj() == JS_GetGlobalForObject(cx, cx->fp->scopeChain));

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
    JSObject* child = cx->fp->scopeChain;
    while (JSObject* parent = OBJ_GET_PARENT(cx, child)) {
        if (!js_IsCacheableNonGlobalScope(child)) {
            debug_only_print0(LC_TMTracer,"Blacklist: non-cacheable object on scope chain.\n");
            Blacklist((jsbytecode*) f->root->ip);
            return false;
        }
        child = parent;
    }
    JS_ASSERT(child == f->globalObj);

    if (!(OBJ_GET_CLASS(cx, f->globalObj)->flags & JSCLASS_IS_GLOBAL)) {
        debug_only_print0(LC_TMTracer, "Blacklist: non-global at root of scope chain.\n");
        Blacklist((jsbytecode*) f->root->ip);
        return false;
    }

    /* Make sure the global object is sane. */
    JS_ASSERT(STOBJ_NSLOTS(f->globalObj) <= MAX_GLOBAL_SLOTS);
    JS_ASSERT(ti->nGlobalTypes() == ti->globalSlots->length());
    JS_ASSERT_IF(ti->globalSlots->length() != 0,
                 OBJ_SHAPE(f->globalObj) == f->globalShape);
    return true;
}

static void
LeaveTree(InterpState&, VMSideExit* lr);

static JS_REQUIRES_STACK VMSideExit*
ExecuteTree(JSContext* cx, TreeFragment* f, uintN& inlineCallCount,
            VMSideExit** innermostNestedGuardp)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_EXECUTE);
#endif
    JS_ASSERT(f->root == f && f->code() && f->treeInfo);
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    TreeInfo* ti = f->treeInfo;

    if (!ScopeChainCheck(cx, ti, f))
        return NULL;

    /* Initialize trace state. */
    InterpState state(cx, tm, ti, inlineCallCount, innermostNestedGuardp);
    double* stack = tm->storage.stack();
    double* global = tm->storage.global();
    JSObject* globalObj = f->globalObj;
    unsigned ngslots = ti->globalSlots->length();
    uint16* gslots = ti->globalSlots->data();

    BuildNativeFrame(cx, globalObj, 0 /* callDepth */, ngslots, gslots,
                     ti->typeMap.data(), global, stack);

    AUDIT(traceTriggered);
    debug_only_printf(LC_TMTracer,
                      "entering trace at %s:%u@%u, native stack slots: %u code: %p\n",
                      cx->fp->script->filename,
                      js_FramePCToLineNumber(cx, cx->fp),
                      FramePCOffset(cx->fp),
                      ti->maxNativeStackSlots,
                      f->code());

    debug_only_stmt(uint32 globalSlots = STOBJ_NSLOTS(globalObj);)
    debug_only_stmt(*(uint64*)&tm->storage.global()[globalSlots] = 0xdeadbeefdeadbeefLL;)

    /* Execute trace. */
#ifdef MOZ_TRACEVIS
    VMSideExit* lr = (TraceVisStateObj(cx, S_NATIVE), ExecuteTrace(cx, f, state));
#else
    VMSideExit* lr = ExecuteTrace(cx, f, state);
#endif

    JS_ASSERT(*(uint64*)&tm->storage.global()[globalSlots] == 0xdeadbeefdeadbeefLL);
    JS_ASSERT_IF(lr->exitType == LOOP_EXIT, !lr->calldepth);

    /* Restore interpreter state. */
    LeaveTree(state, lr);
    return state.innermost;
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
LeaveTree(InterpState& state, VMSideExit* lr)
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
    bool bailed = innermost->exitType == STATUS_EXIT && (bs & JSBUILTIN_BAILED);
    if (bailed) {
        /*
         * Deep-bail case.
         *
         * A _FAIL native already called LeaveTree. We already reconstructed
         * the interpreter stack, in pre-call state, with pc pointing to the
         * CALL/APPLY op, for correctness. Then we continued in native code.
         *
         * First, if we just returned from a slow native, pop its stack frame.
         */
        if (!cx->fp->script) {
            JSStackFrame *fp = cx->fp;
            JS_ASSERT(FUN_SLOW_NATIVE(fp->fun));
            JS_ASSERT(!fp->regs);
            JS_ASSERT(fp->down->regs != &((JSInlineFrame *) fp)->callerRegs);
            cx->fp = fp->down;
            JS_ARENA_RELEASE(&cx->stackPool, ((JSInlineFrame *) fp)->mark);
        }
        JS_ASSERT(cx->fp->script);

        if (!(bs & JSBUILTIN_ERROR)) {
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
            JSFrameRegs* regs = cx->fp->regs;
            JSOp op = (JSOp) *regs->pc;
            JS_ASSERT(op == JSOP_CALL || op == JSOP_APPLY || op == JSOP_NEW ||
                      op == JSOP_GETPROP || op == JSOP_GETTHISPROP || op == JSOP_GETARGPROP ||
                      op == JSOP_GETLOCALPROP || op == JSOP_LENGTH ||
                      op == JSOP_GETELEM || op == JSOP_CALLELEM ||
                      op == JSOP_SETPROP || op == JSOP_SETNAME || op == JSOP_SETMETHOD ||
                      op == JSOP_SETELEM || op == JSOP_INITELEM ||
                      op == JSOP_INSTANCEOF);

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
            JS_ASSERT_IF(!cx->fp->imacpc,
                         cx->fp->slots + cx->fp->script->nfixed +
                         js_ReconstructStackDepth(cx, cx->fp->script, regs->pc) ==
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
            JSTraceType* typeMap = innermost->stackTypeMap();
            for (int i = 1; i <= cs.ndefs; i++) {
                if (!js_NativeToValue(cx,
                                      regs->sp[-i],
                                      typeMap[innermost->numStackSlots - i],
                                      (jsdouble *) state.deepBailSp
                                      + innermost->sp_adj / sizeof(jsdouble) - i)) {
                    OutOfMemoryAbort();
                }
            }
        }
        return;
    }

    /* Save the innermost FrameInfo for guardUpRecursion */
    if (innermost->exitType == RECURSIVE_MISMATCH_EXIT) {
        /* There should never be a static calldepth for a recursive mismatch. */
        JS_ASSERT(innermost->calldepth == 0);
        /* There must be at least one item on the rp stack. */
        JS_ASSERT(callstack < rp);
        /* :TODO: don't be all squirrelin' this in here */
        innermost->recursive_down = *(rp - 1);
    }

    /* Slurp failure should have no frames */
    JS_ASSERT_IF(innermost->exitType == RECURSIVE_SLURP_FAIL_EXIT,
                 innermost->calldepth == 0 && callstack == rp);

    while (callstack < rp) {
        FrameInfo* fi = *callstack;
        /* Peek at the callee native slot in the not-yet-synthesized down frame. */
        JSObject* callee = *(JSObject**)&stack[fi->callerHeight];

        /*
         * Synthesize a stack frame and write out the values in it using the
         * type map pointer on the native call stack.
         */
        SynthesizeFrame(cx, *fi, callee);
        int slots = FlushNativeStackFrame(cx, 1 /* callDepth */, (*callstack)->get_typemap(),
                                          stack, cx->fp, 0);
#ifdef DEBUG
        JSStackFrame* fp = cx->fp;
        debug_only_printf(LC_TMTracer,
                          "synthesized deep frame for %s:%u@%u, slots=%d, fi=%p\n",
                          fp->script->filename,
                          js_FramePCToLineNumber(cx, fp),
                          FramePCOffset(fp),
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
    unsigned calldepth_slots = 0;
    unsigned calleeOffset = 0;
    for (unsigned n = 0; n < calldepth; ++n) {
        /* Peek at the callee native slot in the not-yet-synthesized down frame. */
        calleeOffset += callstack[n]->callerHeight;
        JSObject* callee = *(JSObject**)&stack[calleeOffset];

        /* Reconstruct the frame. */
        calldepth_slots += SynthesizeFrame(cx, *callstack[n], callee);
        ++*state.inlineCallCountp;
#ifdef DEBUG
        JSStackFrame* fp = cx->fp;
        debug_only_printf(LC_TMTracer,
                          "synthesized shallow frame for %s:%u@%u\n",
                          fp->script->filename, js_FramePCToLineNumber(cx, fp),
                          FramePCOffset(fp));
#endif
    }

    /*
     * Adjust sp and pc relative to the tree we exited from (not the tree we
     * entered into).  These are our final values for sp and pc since
     * SynthesizeFrame has already taken care of all frames in between. But
     * first we recover fp->blockChain, which comes from the side exit
     * struct.
     */
    JSStackFrame* fp = cx->fp;

    fp->blockChain = innermost->block;

    /*
     * If we are not exiting from an inlined frame, the state->sp is spbase.
     * Otherwise spbase is whatever slots frames around us consume.
     */
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

    debug_only_printf(LC_TMTracer,
                      "leaving trace at %s:%u@%u, op=%s, lr=%p, exitType=%s, sp=%lld, "
                      "calldepth=%d, cycles=%llu\n",
                      fp->script->filename,
                      js_FramePCToLineNumber(cx, fp),
                      FramePCOffset(fp),
                      js_CodeName[fp->imacpc ? *fp->imacpc : *fp->regs->pc],
                      (void*)lr,
                      getExitName(lr->exitType),
                      (long long int)(fp->regs->sp - StackBase(fp)),
                      calldepth,
                      (unsigned long long int)cycles);

    /*
     * If this trace is part of a tree, later branches might have added
     * additional globals for which we don't have any type information
     * available in the side exit. We merge in this information from the entry
     * type-map. See also the comment in the constructor of TraceRecorder
     * regarding why this is always safe to do.
     */
    TreeInfo* outermostTree = state.outermostTree;
    uint16* gslots = outermostTree->globalSlots->data();
    unsigned ngslots = outermostTree->globalSlots->length();
    JS_ASSERT(ngslots == outermostTree->nGlobalTypes());
    JSTraceType* globalTypeMap;

    /* Are there enough globals? */
    Queue<JSTraceType> typeMap(0);
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
        JS_ASSERT(innermost->root()->treeInfo->nGlobalTypes() == ngslots);
        JS_ASSERT(innermost->root()->treeInfo->nGlobalTypes() > innermost->numGlobalSlots);
        typeMap.ensure(ngslots);
#ifdef DEBUG
        unsigned check_ngslots =
#endif
        BuildGlobalTypeMapFromInnerTree(typeMap, innermost);
        JS_ASSERT(check_ngslots == ngslots);
        globalTypeMap = typeMap.data();
    }

    /* Write back the topmost native stack frame. */
    unsigned ignoreSlots = innermost->exitType == RECURSIVE_SLURP_FAIL_EXIT ?
                           innermost->numStackSlots - 1 : 0;
#ifdef DEBUG
    int slots =
#endif
        FlushNativeStackFrame(cx, innermost->calldepth,
                              innermost->stackTypeMap(),
                              stack, NULL, ignoreSlots);
    JS_ASSERT(unsigned(slots) == innermost->numStackSlots);

    if (innermost->nativeCalleeWord)
        SynthesizeSlowNativeFrame(state, cx, innermost);

    /* Write back interned globals. */
    JS_ASSERT(state.eos == state.stackBase + MAX_NATIVE_STACK_SLOTS);
    JSObject* globalObj = outermostTree->globalObj();
    FlushNativeGlobalFrame(cx, globalObj, state.eos, ngslots, gslots, globalTypeMap);
#ifdef DEBUG
    /* Verify that our state restoration worked. */
    for (JSStackFrame* fp = cx->fp; fp; fp = fp->down) {
        JS_ASSERT_IF(fp->argv, JSVAL_IS_OBJECT(fp->argv[-1]));
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
js_MonitorLoopEdge(JSContext* cx, uintN& inlineCallCount, RecordReason reason)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_MONITOR);
#endif

    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);

    /* Is the recorder currently active? */
    if (tm->recorder) {
        jsbytecode* innerLoopHeaderPC = cx->fp->regs->pc;

        if (TraceRecorder::recordLoopEdge(cx, tm->recorder, inlineCallCount))
            return true;

        /*
         * recordLoopEdge will invoke an inner tree if we have a matching
         * one. If we arrive here, that tree didn't run to completion and
         * instead we mis-matched or the inner tree took a side exit other than
         * the loop exit. We are thus no longer guaranteed to be parked on the
         * same loop header js_MonitorLoopEdge was called for. In fact, this
         * might not even be a loop header at all. Hence if the program counter
         * no longer hovers over the inner loop header, return to the
         * interpreter and do not attempt to trigger or record a new tree at
         * this location.
         */
         if (innerLoopHeaderPC != cx->fp->regs->pc) {
#ifdef MOZ_TRACEVIS
             tvso.r = R_INNER_SIDE_EXIT;
#endif
             return false;
         }
    }
    JS_ASSERT(!tm->recorder);

    /*
     * Make sure the shape of the global object still matches (this might flush
     * the JIT cache).
     */
    JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
    uint32 globalShape = -1;
    SlotList* globalSlots = NULL;

    if (!CheckGlobalObjectShape(cx, tm, globalObj, &globalShape, &globalSlots)) {
        Backoff(cx, cx->fp->regs->pc);
        return false;
    }

    /* Do not enter the JIT code with a pending operation callback. */
    if (cx->operationCallbackFlag) {
#ifdef MOZ_TRACEVIS
        tvso.r = R_CALLBACK_PENDING;
#endif
        return false;
    }

    jsbytecode* pc = cx->fp->regs->pc;
    uint32 argc = cx->fp->argc;

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
            return false;
        }

        /*
         * We can give RecordTree the root peer. If that peer is already taken,
         * it will walk the peer list and find us a free slot or allocate a new
         * tree if needed.
         */
        bool rv = RecordTree(cx, tm, f->first, NULL, 0, globalObj, globalShape,
                             globalSlots, argc, reason);
#ifdef MOZ_TRACEVIS
        if (!rv)
            tvso.r = R_FAIL_RECORD_TREE;
#endif
        return rv;
    }

    debug_only_printf(LC_TMTracer,
                      "Looking for compat peer %d@%d, from %p (ip: %p)\n",
                      js_FramePCToLineNumber(cx, cx->fp),
                      FramePCOffset(cx->fp), (void*)f, f->ip);

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
        return false;
    }

    /*
     * Trees that only unwind recursive frames usually won't do much work, and
     * most time will be spent entering and exiting ExecuteTree(). There's no
     * benefit to doing this until the down-recursive side completes.
     */
    if (match->treeInfo->recursion == Recursion_Unwinds)
        return false;

    VMSideExit* lr = NULL;
    VMSideExit* innermostNestedGuard = NULL;

    lr = ExecuteTree(cx, match, inlineCallCount, &innermostNestedGuard);
    if (!lr) {
#ifdef MOZ_TRACEVIS
        tvso.r = R_FAIL_EXECUTE_TREE;
#endif
        return false;
    }

    /*
     * If we exit on a branch, or on a tree call guard, try to grow the inner
     * tree (in case of a branch exit), or the tree nested around the tree we
     * exited from (in case of the tree call guard).
     */
    bool rv;
    switch (lr->exitType) {
      case RECURSIVE_UNLINKED_EXIT:
      case UNSTABLE_LOOP_EXIT:
          rv = AttemptToStabilizeTree(cx, globalObj, lr, NULL, 0);
#ifdef MOZ_TRACEVIS
          if (!rv)
              tvso.r = R_FAIL_STABILIZE;
#endif
          return rv;

      case OVERFLOW_EXIT:
        oracle.markInstructionUndemotable(cx->fp->regs->pc);
        /* FALL THROUGH */
      case RECURSIVE_SLURP_FAIL_EXIT:
      case RECURSIVE_SLURP_MISMATCH_EXIT:
      case RECURSIVE_EMPTY_RP_EXIT:
      case RECURSIVE_MISMATCH_EXIT:
      case BRANCH_EXIT:
      case CASE_EXIT:
        return AttemptToExtendTree(cx, lr, NULL, NULL
#ifdef MOZ_TRACEVIS
                                          , &tvso
#endif
                 );

      case RECURSIVE_LOOP_EXIT:
      case LOOP_EXIT:
        if (innermostNestedGuard)
            return AttemptToExtendTree(cx, innermostNestedGuard, lr, NULL
#ifdef MOZ_TRACEVIS
                                            , &tvso
#endif
                   );
#ifdef MOZ_TRACEVIS
        tvso.r = R_NO_EXTEND_OUTER;
#endif
        return false;

#ifdef MOZ_TRACEVIS
      case MISMATCH_EXIT:  tvso.r = R_MISMATCH_EXIT;  return false;
      case OOM_EXIT:       tvso.r = R_OOM_EXIT;       return false;
      case TIMEOUT_EXIT:   tvso.r = R_TIMEOUT_EXIT;   return false;
      case DEEP_BAIL_EXIT: tvso.r = R_DEEP_BAIL_EXIT; return false;
      case STATUS_EXIT:    tvso.r = R_STATUS_EXIT;    return false;
#endif

      default:
        /*
         * No, this was an unusual exit (i.e. out of memory/GC), so just resume
         * interpretation.
         */
#ifdef MOZ_TRACEVIS
        tvso.r = R_OTHER_EXIT;
#endif
        return false;
    }
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::monitorRecording(JSOp op)
{
    JSTraceMonitor &localtm = JS_TRACE_MONITOR(cx);
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
        guard(true, pendingGuardCondition, STATUS_EXIT);
        pendingGuardCondition = NULL;
    }

    /* Handle one-shot request to unbox the result of a property get. */
    if (pendingUnboxSlot) {
        LIns* val_ins = get(pendingUnboxSlot);
        val_ins = unbox_jsval(*pendingUnboxSlot, val_ins, snapshot(BRANCH_EXIT));
        set(pendingUnboxSlot, val_ins);
        pendingUnboxSlot = 0;
    }

    debug_only_stmt(
        if (js_LogController.lcbits & LC_TMRecorder) {
            js_Disassemble1(cx, cx->fp->script, cx->fp->regs->pc,
                            cx->fp->imacpc
                                ? 0 : cx->fp->regs->pc - cx->fp->script->code,
                            !cx->fp->imacpc, stdout);
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
    bool wasInImacro = (cx->fp->imacpc != NULL);
#endif
    switch (op) {
      default:
          status = ARECORD_ERROR;
          goto stop_recording;
# define OPDEF(x,val,name,token,length,nuses,ndefs,prec,format)               \
      case x:                                                                 \
        status = this->record_##x();                                            \
        if (JSOP_IS_IMACOP(x))                                                \
            goto imacro;                                                      \
        break;
# include "jsopcode.tbl"
# undef OPDEF
    }

    /* Careful, |this| may have been deleted. */
    JS_ASSERT(status != ARECORD_IMACRO);
    JS_ASSERT_IF(!wasInImacro, localcx->fp->imacpc == NULL);

  imacro:
    /* |this| may be killed while recording. */
    if (status == ARECORD_COMPLETED) {
        JS_ASSERT(localtm.recorder != this);
        return localtm.recorder ? ARECORD_CONTINUE : ARECORD_COMPLETED;
    }
    if (status == ARECORD_ABORTED) {
        JS_ASSERT(!localtm.recorder);
        return ARECORD_ABORTED;
    }

  stop_recording:
    /* Handle lazy abort / OOM. */
    if (outOfMemory() || js_OverfullJITCache(&localtm)) {
        ResetJIT(cx, FR_OOM);
        return ARECORD_ABORTED;
    }
    if (StatusAbortsRecording(status)) {
        js_AbortRecording(cx, js_CodeName[op]);
        return ARECORD_ABORTED;
    }

    return status;
}

JS_REQUIRES_STACK void
js_AbortRecording(JSContext* cx, const char* reason)
{
#ifdef DEBUG
    JS_ASSERT(TRACE_RECORDER(cx));
    TRACE_RECORDER(cx)->finishAbort(reason);
#else
    TRACE_RECORDER(cx)->finishAbort("[no reason]");
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
extern "C" int js_arm_try_thumb_op();
extern "C" int js_arm_try_armv6t2_op();
extern "C" int js_arm_try_armv5_op();
extern "C" int js_arm_try_armv6_op();
extern "C" int js_arm_try_armv7_op();
extern "C" int js_arm_try_vfp_op();

static bool
js_arm_check_thumb() {
    bool ret = false;
    __try {
        js_arm_try_thumb_op();
        ret = true;
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        ret = false;
    }
    return ret;
}

static bool
js_arm_check_thumb2() {
    bool ret = false;
    __try {
        js_arm_try_armv6t2_op();
        ret = true;
    } __except(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        ret = false;
    }
    return ret;
}

static unsigned int
js_arm_check_arch() {
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
js_arm_check_vfp() {
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
js_disable_debugger_exceptions()
{
    // 2 == TLSSLOT_KERNEL
    DWORD kctrl = (DWORD) TlsGetValue(2);
    // 0x12 = TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG
    kctrl |= 0x12;
    TlsSetValue(2, (LPVOID) kctrl);
}

static void
js_enable_debugger_exceptions()
{
    // 2 == TLSSLOT_KERNEL
    DWORD kctrl = (DWORD) TlsGetValue(2);
    // 0x12 = TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG
    kctrl &= ~0x12;
    TlsSetValue(2, (LPVOID) kctrl);
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

// Assume ARMv4 by default.
static unsigned int arm_arch = 4;
static bool arm_has_thumb = false;
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
                arm_has_thumb = (hwcap & 4) != 0;
                arm_has_vfp = (hwcap & 64) != 0;
                arm_has_iwmmxt = (hwcap & 512) != 0;
                // this flag is only present on kernel 2.6.29
                arm_has_neon = (hwcap & 4096) != 0;
            } else if (aux.a_type == AT_PLATFORM) {
                const char *plat = (const char*) aux.a_un.a_val;
                if (getenv("ARM_FORCE_PLATFORM"))
                    plat = getenv("ARM_FORCE_PLATFORM");
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
                else
                {
                    // For production code, ignore invalid (or unexpected) platform strings and
                    // fall back to the default. For debug code, use an assertion to catch this
                    // when not running in scratchbox.
                    if (getenv("_SBOX_DIR") == NULL)
                        JS_ASSERT(false);
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

static bool
js_arm_check_thumb() {
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_has_thumb;
}

static bool
js_arm_check_thumb2() {
    if (!arm_tests_initialized)
        arm_read_auxv();

    // ARMv6T2 also supports Thumb2, but Linux doesn't provide an easy way to test for this as
    // there is no associated bit in auxv. ARMv7 always supports Thumb2, and future architectures
    // are assumed to be backwards-compatible.
    return (arm_arch >= 7);
}

static unsigned int
js_arm_check_arch() {
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_arch;
}

static bool
js_arm_check_vfp() {
    if (!arm_tests_initialized)
        arm_read_auxv();

    return arm_has_vfp;
}

#else
#warning Not sure how to check for architecture variant on your platform. Assuming ARMv4.
static bool
js_arm_check_thumb() { return false; }
static bool
js_arm_check_thumb2() { return false; }
static unsigned int
js_arm_check_arch() { return 4; }
static bool
js_arm_check_vfp() { return false; }
#endif

#ifndef HAVE_ENABLE_DISABLE_DEBUGGER_EXCEPTIONS
static void
js_enable_debugger_exceptions() { }
static void
js_disable_debugger_exceptions() { }
#endif

#endif /* NANOJIT_ARM */

#define K *1024
#define M K K
#define G K M

void
js_SetMaxCodeCacheBytes(JSContext* cx, uint32 bytes)
{
    JSTraceMonitor* tm = &JS_THREAD_DATA(cx)->traceMonitor;
    JS_ASSERT(tm->codeAlloc && tm->dataAlloc && tm->traceAlloc);
    if (bytes > 1 G)
        bytes = 1 G;
    if (bytes < 128 K)
        bytes = 128 K;
    tm->maxCodeCacheBytes = bytes;
}

void
js_InitJIT(JSTraceMonitor *tm)
{
#if defined JS_JIT_SPEW
    tm->profAlloc = NULL;
    /* Set up debug logging. */
    if (!did_we_set_up_debug_logging) {
        InitJITLogController();
        did_we_set_up_debug_logging = true;
    }
    /* Set up fragprofiling, if required. */
    if (js_LogController.lcbits & LC_FragProfile) {
        tm->profAlloc = new VMAllocator();
        tm->profTab = new (*tm->profAlloc) FragStatsMap(*tm->profAlloc);
    }
    tm->lastFragID = 0;
#else
    memset(&js_LogController, 0, sizeof(js_LogController));
#endif

    if (!did_we_check_processor_features) {
#if defined NANOJIT_IA32
        avmplus::AvmCore::config.use_cmov =
            avmplus::AvmCore::config.sse2 = CheckForSSE2();
        avmplus::AvmCore::config.fixed_esp = true;
#endif
#if defined NANOJIT_ARM

        js_disable_debugger_exceptions();

        bool            arm_vfp     = js_arm_check_vfp();
        bool            arm_thumb   = js_arm_check_thumb();
        bool            arm_thumb2  = js_arm_check_thumb2();
        unsigned int    arm_arch    = js_arm_check_arch();

        js_enable_debugger_exceptions();

        avmplus::AvmCore::config.vfp        = arm_vfp;
        avmplus::AvmCore::config.soft_float = !arm_vfp;
        avmplus::AvmCore::config.thumb      = arm_thumb;
        avmplus::AvmCore::config.thumb2     = arm_thumb2;
        avmplus::AvmCore::config.arch       = arm_arch;

        // Sanity-check the configuration detection.
        //  * We don't understand architectures prior to ARMv4.
        JS_ASSERT(arm_arch >= 4);
        //  * All architectures support Thumb with the possible exception of ARMv4.
        JS_ASSERT((arm_thumb) || (arm_arch == 4));
        //  * Only ARMv6T2 and ARMv7(+) support Thumb2, but ARMv6 does not.
        JS_ASSERT((arm_thumb2) || (arm_arch <= 6));
        //  * All architectures that support Thumb2 also support Thumb.
        JS_ASSERT((arm_thumb2 && arm_thumb) || (!arm_thumb2));
#endif
        did_we_check_processor_features = true;
    }

    /* Set the default size for the code cache to 16MB. */
    tm->maxCodeCacheBytes = 16 M;

    if (!tm->recordAttempts.ops) {
        JS_DHashTableInit(&tm->recordAttempts, JS_DHashGetStubOps(),
                          NULL, sizeof(PCHashEntry),
                          JS_DHASH_DEFAULT_CAPACITY(PC_HASH_COUNT));
    }

    JS_ASSERT(!tm->dataAlloc && !tm->traceAlloc && !tm->codeAlloc);
    tm->dataAlloc = new VMAllocator();
    tm->traceAlloc = new VMAllocator();
    tm->tempAlloc = new VMAllocator();
    tm->reTempAlloc = new VMAllocator();
    tm->codeAlloc = new CodeAlloc();
    tm->frameCache = new FrameInfoCache(tm->dataAlloc);
    tm->flush();
    verbose_only( tm->branches = NULL; )

#if !defined XP_WIN
    debug_only(memset(&jitstats, 0, sizeof(jitstats)));
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
js_FinishJIT(JSTraceMonitor *tm)
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

    if (tm->recordAttempts.ops)
        JS_DHashTableFinish(&tm->recordAttempts);

#ifdef DEBUG
    // Recover profiling data from expiring Fragments, and display
    // final results.
    if (js_LogController.lcbits & LC_FragProfile) {
        for (Seq<Fragment*>* f = tm->branches; f; f = f->tail) {
            js_FragProfiling_FragFinalizer(f->head, tm);
        }
        for (size_t i = 0; i < FRAGMENT_TABLE_SIZE; ++i) {
            for (TreeFragment *f = tm->vmfragments[i]; f; f = f->next) {
                JS_ASSERT(f->root == f);
                for (TreeFragment *p = f; p; p = p->peer)
                    js_FragProfiling_FragFinalizer(p, tm);
            }
        }
        REHashMap::Iter iter(*(tm->reFragments));
        while (iter.next()) {
            VMFragment* frag = (VMFragment*)iter.value();
            js_FragProfiling_FragFinalizer(frag, tm);
        }

        js_FragProfiling_showResults(tm);
        delete tm->profAlloc;

    } else {
        NanoAssert(!tm->profTab);
        NanoAssert(!tm->profAlloc);
    }
#endif

    memset(&tm->vmfragments[0], 0, FRAGMENT_TABLE_SIZE * sizeof(TreeFragment*));

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
}

void
js_PurgeJITOracle()
{
    oracle.clear();
}

static JSDHashOperator
PurgeScriptRecordingAttempts(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number, void *arg)
{
    PCHashEntry *e = (PCHashEntry *)hdr;
    JSScript *script = (JSScript *)arg;
    jsbytecode *pc = (jsbytecode *)e->key;

    if (JS_UPTRDIFF(pc, script->code) < script->length)
        return JS_DHASH_REMOVE;
    return JS_DHASH_NEXT;
}


JS_REQUIRES_STACK void
js_PurgeScriptFragments(JSContext* cx, JSScript* script)
{
    if (!TRACING_ENABLED(cx))
        return;
    debug_only_printf(LC_TMTracer,
                      "Purging fragments for JSScript %p.\n", (void*)script);

    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
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
                    verbose_only( js_FragProfiling_FragFinalizer(frag, tm); )
                    TrashTree(cx, frag);
                } while ((frag = frag->peer) != NULL);
                continue;
            }
            fragp = &frag->next;
        }
    }

    JS_DHashTableEnumerate(&tm->recordAttempts, PurgeScriptRecordingAttempts, script);
}

bool
js_OverfullJITCache(JSTraceMonitor* tm)
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
js_DeepBail(JSContext *cx)
{
    JS_ASSERT(JS_ON_TRACE(cx));

    /*
     * Exactly one context on the current thread is on trace. Find out which
     * one. (Most callers cannot guarantee that it's cx.)
     */
    JSTraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    JSContext *tracecx = tm->tracecx;

    /* It's a bug if a non-FAIL_STATUS builtin gets here. */
    JS_ASSERT(tracecx->bailExit);

    tm->tracecx = NULL;
    debug_only_print0(LC_TMTracer, "Deep bail.\n");
    LeaveTree(*tracecx->interpState, tracecx->bailExit);
    tracecx->bailExit = NULL;

    InterpState* state = tracecx->interpState;
    state->builtinStatus |= JSBUILTIN_BAILED;

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

/*
 * Return the frame of a call object if that frame is part of the current
 * trace. |depthp| is an optional outparam: if it is non-null, it will be
 * filled in with the depth of the call object's frame relevant to cx->fp.
 */
JS_REQUIRES_STACK JSStackFrame*
TraceRecorder::frameIfInRange(JSObject* obj, unsigned* depthp) const
{
    JSStackFrame* ofp = (JSStackFrame*) obj->getPrivate();
    JSStackFrame* fp = cx->fp;
    for (unsigned depth = 0; depth <= callDepth; ++depth) {
        if (fp == ofp) {
            if (depthp)
                *depthp = depth;
            return ofp;
        }
        if (!(fp = fp->down))
            break;
    }
    return NULL;
}

JS_DEFINE_CALLINFO_4(extern, UINT32, GetClosureVar, CONTEXT, OBJECT, CVIPTR, DOUBLEPTR, 0, 0)
JS_DEFINE_CALLINFO_4(extern, UINT32, GetClosureArg, CONTEXT, OBJECT, CVIPTR, DOUBLEPTR, 0, 0)

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
TraceRecorder::scopeChainProp(JSObject* obj, jsval*& vp, LIns*& ins, NameResult& nr)
{
    JS_ASSERT(obj != globalObj);

    JSTraceMonitor &localtm = *traceMonitor;

    JSAtom* atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    JSObject* obj2;
    JSProperty* prop;
    bool ok = js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &obj2, &prop);

    /* js_FindProperty can reenter the interpreter and kill |this|. */
    if (!localtm.recorder)
        return ARECORD_ABORTED;

    if (!ok)
        RETURN_ERROR_A("error in js_FindProperty");

    if (!prop)
        RETURN_STOP_A("failed to find name in non-global scope chain");

    if (obj == globalObj) {
        // Even if the property is on the global object, we must guard against
        // the creation of properties that shadow the property in the middle
        // of the scope chain if we are in a function.
        if (cx->fp->argv) {
            LIns* obj_ins;
            JSObject* parent = STOBJ_GET_PARENT(cx->fp->calleeObject());
            LIns* parent_ins = stobj_get_parent(get(&cx->fp->argv[-2]));
            CHECK_STATUS_A(traverseScopeChain(parent, parent_ins, obj, obj_ins));
        }

        JSScopeProperty* sprop = (JSScopeProperty*) prop;

        if (obj2 != obj) {
            obj2->dropProperty(cx, prop);
            RETURN_STOP_A("prototype property");
        }
        if (!isValidSlot(OBJ_SCOPE(obj), sprop)) {
            obj2->dropProperty(cx, prop);
            return ARECORD_STOP;
        }
        if (!lazilyImportGlobalSlot(sprop->slot)) {
            obj2->dropProperty(cx, prop);
            RETURN_STOP_A("lazy import of global slot failed");
        }
        vp = &STOBJ_GET_SLOT(obj, sprop->slot);
        ins = get(vp);
        obj2->dropProperty(cx, prop);
        nr.tracked = true;
        return ARECORD_CONTINUE;
    }

    if (obj == obj2 && OBJ_GET_CLASS(cx, obj) == &js_CallClass)
        return InjectStatus(callProp(obj, prop, ATOM_TO_JSID(atom), vp, ins, nr));

    obj2->dropProperty(cx, prop);
    RETURN_STOP_A("fp->scopeChain is not global or active call object");
}

/*
 * Generate LIR to access a property of a Call object.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callProp(JSObject* obj, JSProperty* prop, jsid id, jsval*& vp,
                        LIns*& ins, NameResult& nr)
{
    JSScopeProperty *sprop = (JSScopeProperty*) prop;

    JSOp op = JSOp(*cx->fp->regs->pc);
    uint32 setflags = (js_CodeSpec[op].format & (JOF_SET | JOF_INCDEC | JOF_FOR));
    if (setflags && (sprop->attrs & JSPROP_READONLY))
        RETURN_STOP("writing to a read-only property");

    uintN slot = sprop->shortid;

    vp = NULL;
    uintN upvar_slot = SPROP_INVALID_SLOT;
    JSStackFrame* cfp = (JSStackFrame*) obj->getPrivate();
    if (cfp) {
        if (sprop->getter == js_GetCallArg) {
            JS_ASSERT(slot < cfp->fun->nargs);
            vp = &cfp->argv[slot];
            upvar_slot = slot;
            nr.v = *vp;
        } else if (sprop->getter == js_GetCallVar) {
            JS_ASSERT(slot < cfp->script->nslots);
            vp = &cfp->slots[slot];
            upvar_slot = cx->fp->fun->nargs + slot;
            nr.v = *vp;
        } else {
            RETURN_STOP("dynamic property of Call object");
        }
        obj->dropProperty(cx, prop);

        if (frameIfInRange(obj)) {
            // At this point we are guaranteed to be looking at an active call oject
            // whose properties are stored in the corresponding JSStackFrame.
            ins = get(vp);
            nr.tracked = true;
            return RECORD_CONTINUE;
        }
    } else {
        // Call objects do not yet have sprop->isMethod() properties, but they
        // should. See bug 514046, for which this code is future-proof. Remove
        // this comment when that bug is fixed (so, FIXME: 514046).
#ifdef DEBUG
        JSBool rv =
#endif
            js_GetPropertyHelper(cx, obj, sprop->id,
                                 (op == JSOP_CALLNAME)
                                 ? JSGET_NO_METHOD_BARRIER
                                 : JSGET_METHOD_BARRIER,
                                 &nr.v);
        JS_ASSERT(rv);
        obj->dropProperty(cx, prop);
    }

    LIns* obj_ins;
    JSObject* parent = STOBJ_GET_PARENT(cx->fp->calleeObject());
    LIns* parent_ins = stobj_get_parent(get(&cx->fp->argv[-2]));
    CHECK_STATUS(traverseScopeChain(parent, parent_ins, obj, obj_ins));

    ClosureVarInfo* cv = new (traceAlloc()) ClosureVarInfo();
    cv->id = id;
    cv->slot = slot;
    cv->callDepth = callDepth;
    cv->resolveFlags = cx->resolveFlags == JSRESOLVE_INFER
                     ? js_InferFlags(cx, 0)
                     : cx->resolveFlags;

    LIns* outp = lir->insAlloc(sizeof(double));
    LIns* args[] = {
        outp,
        INS_CONSTPTR(cv),
        obj_ins,
        cx_ins
    };
    const CallInfo* ci;
    if (sprop->getter == js_GetCallArg)
        ci = &GetClosureArg_ci;
    else
        ci = &GetClosureVar_ci;

    LIns* call_ins = lir->insCall(ci, args);
    JSTraceType type = getCoercedType(nr.v);
    guard(true,
          addName(lir->ins2(LIR_eq, call_ins, lir->insImm(type)),
                  "guard(type-stable name access)"),
          BRANCH_EXIT);
    ins = stackLoad(outp, type);
    nr.tracked = false;
    nr.obj = obj;
    nr.obj_ins = obj_ins;
    nr.sprop = sprop;
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
    set(&stackval(n), i, n >= 0);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::alu(LOpcode v, jsdouble v0, jsdouble v1, LIns* s0, LIns* s1)
{
    /*
     * To even consider this operation for demotion, both operands have to be
     * integers and the oracle must not give us a negative hint for the
     * instruction.
     */
    if (oracle.isInstructionUndemotable(cx->fp->regs->pc) || !isPromoteInt(s0) || !isPromoteInt(s1)) {
    out:
        if (v == LIR_fmod) {
            LIns* args[] = { s1, s0 };
            return lir->insCall(&js_dmod_ci, args);
        }
        LIns* result = lir->ins2(v, s0, s1);
        JS_ASSERT_IF(s0->isconstf() && s1->isconstf(), result->isconstf());
        return result;
    }

    jsdouble r;
    switch (v) {
    case LIR_fadd:
        r = v0 + v1;
        break;
    case LIR_fsub:
        r = v0 - v1;
        break;
#if !defined NANOJIT_ARM
    case LIR_fmul:
        r = v0 * v1;
        if (r == 0.0)
            goto out;
        break;
#endif
#if defined NANOJIT_IA32 || defined NANOJIT_X64
    case LIR_fdiv:
        if (v1 == 0)
            goto out;
        r = v0 / v1;
        break;
    case LIR_fmod:
        if (v0 < 0 || v1 == 0 || (s1->isconstf() && v1 < 0))
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

    LIns* d0 = ::demote(lir, s0);
    LIns* d1 = ::demote(lir, s1);

    /*
     * Speculatively emit an integer operation, betting that at runtime we
     * will get integer results again.
     */
    VMSideExit* exit;
    LIns* result;
    switch (v) {
#if defined NANOJIT_IA32 || defined NANOJIT_X64
      case LIR_fdiv:
        if (d0->isconst() && d1->isconst())
            return lir->ins1(LIR_i2f, lir->insImm(jsint(r)));

        exit = snapshot(OVERFLOW_EXIT);

        /*
         * If the divisor is greater than zero its always safe to execute
         * the division. If not, we have to make sure we are not running
         * into -2147483648 / -1, because it can raise an overflow exception.
         */
        if (!d1->isconst()) {
            LIns* gt = lir->insBranch(LIR_jt, lir->ins2i(LIR_gt, d1, 0), NULL);
            guard(false, lir->ins_eq0(d1), exit);
            guard(false, lir->ins2(LIR_and,
                                   lir->ins2i(LIR_eq, d0, 0x80000000),
                                   lir->ins2i(LIR_eq, d1, -1)), exit);
            gt->setTarget(lir->ins0(LIR_label));
        } else {
            if (d1->imm32() == -1)
                guard(false, lir->ins2i(LIR_eq, d0, 0x80000000), exit);
        }
        result = lir->ins2(v = LIR_div, d0, d1);

        /* As long the modulus is zero, the result is an integer. */
        guard(true, lir->ins_eq0(lir->ins1(LIR_mod, result)), exit);

        /* Don't lose a -0. */
        guard(false, lir->ins_eq0(result), exit);
        break;

      case LIR_fmod: {
        if (d0->isconst() && d1->isconst())
            return lir->ins1(LIR_i2f, lir->insImm(jsint(r)));

        exit = snapshot(OVERFLOW_EXIT);

        /* Make sure we don't trigger division by zero at runtime. */
        if (!d1->isconst())
            guard(false, lir->ins_eq0(d1), exit);
        result = lir->ins1(v = LIR_mod, lir->ins2(LIR_div, d0, d1));

        /* If the result is not 0, it is always within the integer domain. */
        LIns* branch = lir->insBranch(LIR_jf, lir->ins_eq0(result), NULL);

        /*
         * If the result is zero, we must exit if the lhs is negative since
         * the result is -0 in this case, which is not in the integer domain.
         */
        guard(false, lir->ins2i(LIR_lt, d0, 0), exit);
        branch->setTarget(lir->ins0(LIR_label));
        break;
      }
#endif

      default:
        v = (LOpcode)((int)v & ~LIR64);
        result = lir->ins2(v, d0, d1);

        /*
         * If the operands guarantee that the result will be an integer (i.e.
         * z = x + y with 0 <= (x|y) <= 0xffff guarantees z <= fffe0001), we
         * don't have to guard against an overflow. Otherwise we emit a guard
         * that will inform the oracle and cause a non-demoted trace to be
         * attached that uses floating-point math for this operation.
         */
        if (!result->isconst() && (!IsOverflowSafe(v, d0) || !IsOverflowSafe(v, d1))) {
            exit = snapshot(OVERFLOW_EXIT);
            guard(false, lir->ins1(LIR_ov, result), exit);
            if (v == LIR_mul) // make sure we don't lose a -0
                guard(false, lir->ins_eq0(result), exit);
        }
        break;
    }
    JS_ASSERT_IF(d0->isconst() && d1->isconst(),
                 result->isconst() && result->imm32() == jsint(r));
    return lir->ins1(LIR_i2f, result);
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
    } else if (JSVAL_IS_SPECIAL(v)) {
        ci = &js_BooleanOrUndefinedToString_ci;
    } else {
        /*
         * Callers must deal with non-primitive (non-null object) values by
         * calling an imacro. We don't try to guess about which imacro, with
         * what valueOf hint, here.
         */
        JS_ASSERT(JSVAL_IS_NULL(v));
        return INS_ATOM(cx->runtime->atomState.nullAtom);
    }

    v_ins = lir->insCall(ci, args);
    guard(false, lir->ins_peq0(v_ins), OOM_EXIT);
    return v_ins;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::call_imacro(jsbytecode* imacro)
{
    JSStackFrame* fp = cx->fp;
    JSFrameRegs* regs = fp->regs;

    /* We cannot nest imacros, only tail-call. */
    if (fp->imacpc) {
        /* Dereference is safe since imacros are JSOP_STOP-terminated. */
        if (regs->pc[js_CodeSpec[*regs->pc].length] != JSOP_STOP)
            return RECORD_STOP;
        regs->pc = imacro;
        return RECORD_IMACRO;
    }

    fp->imacpc = regs->pc;
    regs->pc = imacro;
    atoms = COMMON_ATOMS_START(&cx->runtime->atomState);
    return RECORD_IMACRO;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::ifop()
{
    jsval& v = stackval(-1);
    LIns* v_ins = get(&v);
    bool cond;
    LIns* x;

    if (JSVAL_IS_NULL(v)) {
        cond = false;
        x = lir->insImm(0);
    } else if (!JSVAL_IS_PRIMITIVE(v)) {
        cond = true;
        x = lir->insImm(1);
    } else if (JSVAL_IS_SPECIAL(v)) {
        /* Test for boolean is true, negate later if we are testing for false. */
        cond = JSVAL_TO_SPECIAL(v) == JS_TRUE;
        x = lir->ins2i(LIR_eq, v_ins, 1);
    } else if (isNumber(v)) {
        jsdouble d = asNumber(v);
        cond = !JSDOUBLE_IS_NaN(d) && d;
        x = lir->ins2(LIR_and,
                      lir->ins2(LIR_feq, v_ins, v_ins),
                      lir->ins_eq0(lir->ins2(LIR_feq, v_ins, lir->insImmf(0))));
    } else if (JSVAL_IS_STRING(v)) {
        cond = JSVAL_TO_STRING(v)->length() != 0;
        x = lir->ins2(LIR_piand,
                      lir->insLoad(LIR_ldp,
                                   v_ins,
                                   (int)offsetof(JSString, mLength)),
                      INS_CONSTWORD(JSString::LENGTH_MASK));
    } else {
        JS_NOT_REACHED("ifop");
        return ARECORD_STOP;
    }

    jsbytecode* pc = cx->fp->regs->pc;
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
    jsval& v = stackval(-1);

    /* No need to guard if the condition can't match any of the cases. */
    if (!isNumber(v))
        return ARECORD_CONTINUE;

    /* No need to guard if the condition is constant. */
    LIns* v_ins = f2i(get(&v));
    if (v_ins->isconst() || v_ins->isconstq())
        return ARECORD_CONTINUE;

    jsbytecode* pc = cx->fp->regs->pc;
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
        low = GET_JUMPX_OFFSET(pc);
        pc += JUMPX_OFFSET_LEN;
        high = GET_JUMPX_OFFSET(pc);
    }

    /* Cap maximum table-switch size for modesty. */
    if ((high + 1 - low) > MAX_TABLE_SWITCH)
        return InjectStatus(switchop());

    /* Generate switch LIR. */
    SwitchInfo* si = new (traceAlloc()) SwitchInfo();
    si->count = high + 1 - low;
    si->table = 0;
    si->index = (uint32) -1;
    LIns* diff = lir->ins2(LIR_sub, v_ins, lir->insImm(low));
    LIns* cmp = lir->ins2(LIR_ult, diff, lir->insImm(si->count));
    lir->insGuard(LIR_xf, cmp, createGuardRecord(snapshot(DEFAULT_EXIT)));
    lir->insStorei(diff, lir->insImmPtr(&si->index), 0);
    VMSideExit* exit = snapshot(CASE_EXIT);
    exit->switchInfo = si;
    LIns* guardIns = lir->insGuard(LIR_xtbl, diff, createGuardRecord(exit));
    fragment->lastIns = guardIns;
    CHECK_STATUS_A(compile());
    return finishSuccessfully();
}
#endif

static JS_ALWAYS_INLINE int32_t
UnboxBooleanOrUndefined(jsval v)
{
    /* Although this says 'special', we really only expect 3 special values: */
    JS_ASSERT(v == JSVAL_TRUE || v == JSVAL_FALSE || v == JSVAL_VOID);
    return JSVAL_TO_SPECIAL(v);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::switchop()
{
    jsval& v = stackval(-1);
    LIns* v_ins = get(&v);

    /* No need to guard if the condition is constant. */
    if (v_ins->isconst() || v_ins->isconstq())
        return RECORD_CONTINUE;
    if (isNumber(v)) {
        jsdouble d = asNumber(v);
        guard(true,
              addName(lir->ins2(LIR_feq, v_ins, lir->insImmf(d)),
                      "guard(switch on numeric)"),
              BRANCH_EXIT);
    } else if (JSVAL_IS_STRING(v)) {
        LIns* args[] = { INS_CONSTSTR(JSVAL_TO_STRING(v)), v_ins };
        guard(true,
              addName(lir->ins_eq0(lir->ins_eq0(lir->insCall(&js_EqualStrings_ci, args))),
                      "guard(switch on string)"),
              BRANCH_EXIT);
    } else if (JSVAL_IS_SPECIAL(v)) {
        guard(true,
              addName(lir->ins2(LIR_eq, v_ins, lir->insImm(UnboxBooleanOrUndefined(v))),
                      "guard(switch on boolean)"),
              BRANCH_EXIT);
    } else {
        RETURN_STOP("switch on object or null");
    }
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::inc(jsval& v, jsint incr, bool pre)
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
TraceRecorder::inc(jsval v, LIns*& v_ins, jsint incr, bool pre)
{
    LIns* v_after;
    CHECK_STATUS(incHelper(v, v_ins, v_after, incr));

    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    JS_ASSERT(cs.ndefs == 1);
    stack(-cs.nuses, pre ? v_after : v_ins);
    v_ins = v_after;
    return RECORD_CONTINUE;
}

/*
 * Do an increment operation without storing anything to the stack.
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::incHelper(jsval v, LIns* v_ins, LIns*& v_after, jsint incr)
{
    if (!isNumber(v))
        RETURN_STOP("can only inc numbers");
    v_after = alu(LIR_fadd, asNumber(v), incr, v_ins, lir->insImmf(incr));
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::incProp(jsint incr, bool pre)
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l))
        RETURN_STOP_A("incProp on primitive");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);

    uint32 slot;
    LIns* v_ins;
    CHECK_STATUS_A(prop(obj, obj_ins, &slot, &v_ins, NULL));

    if (slot == SPROP_INVALID_SLOT)
        RETURN_STOP_A("incProp on invalid slot");

    jsval& v = STOBJ_GET_SLOT(obj, slot);
    CHECK_STATUS_A(inc(v, v_ins, incr, pre));

    LIns* dslots_ins = NULL;
    stobj_set_slot(obj_ins, slot, dslots_ins, box_jsval(v, v_ins));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::incElem(jsint incr, bool pre)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    jsval* vp;
    LIns* v_ins;
    LIns* addr_ins;

    if (JSVAL_IS_PRIMITIVE(l) || !JSVAL_IS_INT(r) ||
        !guardDenseArray(JSVAL_TO_OBJECT(l), get(&l))) {
        return RECORD_STOP;
    }

    CHECK_STATUS(denseArrayElement(l, r, vp, v_ins, addr_ins));
    if (!addr_ins) // if we read a hole, abort
        return RECORD_STOP;
    CHECK_STATUS(inc(*vp, v_ins, incr, pre));
    lir->insStorei(box_jsval(*vp, v_ins), addr_ins, 0);
    return RECORD_CONTINUE;
}

static bool
EvalCmp(LOpcode op, double l, double r)
{
    bool cond;
    switch (op) {
      case LIR_feq:
        cond = (l == r);
        break;
      case LIR_flt:
        cond = l < r;
        break;
      case LIR_fgt:
        cond = l > r;
        break;
      case LIR_fle:
        cond = l <= r;
        break;
      case LIR_fge:
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
    if (op == LIR_feq)
        return !!js_EqualStrings(l, r);
    return EvalCmp(op, js_CompareStrings(l, r), 0);
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

    JSTraceType ltag = GetPromotedType(l);
    if (ltag != GetPromotedType(r)) {
        cond = !equal;
        x = lir->insImm(cond);
    } else if (ltag == TT_STRING) {
        LIns* args[] = { r_ins, l_ins };
        x = lir->ins2i(LIR_eq, lir->insCall(&js_EqualStrings_ci, args), equal);
        cond = !!js_EqualStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
    } else {
        LOpcode op;
        if (ltag == TT_DOUBLE)
            op = LIR_feq;
        else if (ltag == TT_NULL || ltag == TT_OBJECT || ltag == TT_FUNCTION)
            op = LIR_peq;
        else
            op = LIR_eq;
        x = lir->ins2(op, l_ins, r_ins);
        if (!equal)
            x = lir->ins_eq0(x);
        cond = (ltag == TT_DOUBLE)
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

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::equality(bool negate, bool tryBranchAfterCond)
{
    jsval& rval = stackval(-1);
    jsval& lval = stackval(-2);
    LIns* l_ins = get(&lval);
    LIns* r_ins = get(&rval);

    return equalityHelper(lval, rval, l_ins, r_ins, negate, tryBranchAfterCond, lval);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::equalityHelper(jsval l, jsval r, LIns* l_ins, LIns* r_ins,
                              bool negate, bool tryBranchAfterCond,
                              jsval& rval)
{
    LOpcode op = LIR_eq;
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

    if (GetPromotedType(l) == GetPromotedType(r)) {
        if (JSVAL_TAG(l) == JSVAL_OBJECT || JSVAL_IS_SPECIAL(l)) {
            if (JSVAL_TAG(l) == JSVAL_OBJECT)
                op = LIR_peq;
            cond = (l == r);
        } else if (JSVAL_IS_STRING(l)) {
            args[0] = r_ins, args[1] = l_ins;
            l_ins = lir->insCall(&js_EqualStrings_ci, args);
            r_ins = lir->insImm(1);
            cond = !!js_EqualStrings(JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        } else {
            JS_ASSERT(isNumber(l) && isNumber(r));
            cond = (asNumber(l) == asNumber(r));
            op = LIR_feq;
        }
    } else if (JSVAL_IS_NULL(l) && JSVAL_IS_SPECIAL(r)) {
        l_ins = lir->insImm(JSVAL_TO_SPECIAL(JSVAL_VOID));
        cond = (r == JSVAL_VOID);
    } else if (JSVAL_IS_SPECIAL(l) && JSVAL_IS_NULL(r)) {
        r_ins = lir->insImm(JSVAL_TO_SPECIAL(JSVAL_VOID));
        cond = (l == JSVAL_VOID);
    } else if (isNumber(l) && JSVAL_IS_STRING(r)) {
        args[0] = r_ins, args[1] = cx_ins;
        r_ins = lir->insCall(&js_StringToNumber_ci, args);
        cond = (asNumber(l) == js_StringToNumber(cx, JSVAL_TO_STRING(r)));
        op = LIR_feq;
    } else if (JSVAL_IS_STRING(l) && isNumber(r)) {
        args[0] = l_ins, args[1] = cx_ins;
        l_ins = lir->insCall(&js_StringToNumber_ci, args);
        cond = (js_StringToNumber(cx, JSVAL_TO_STRING(l)) == asNumber(r));
        op = LIR_feq;
    } else {
        if (JSVAL_IS_SPECIAL(l)) {
            bool isVoid = !!JSVAL_IS_VOID(l);
            guard(isVoid,
                  lir->ins2(LIR_eq, l_ins, INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID))),
                  BRANCH_EXIT);
            if (!isVoid) {
                args[0] = l_ins, args[1] = cx_ins;
                l_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
                l = (l == JSVAL_VOID)
                    ? cx->runtime->NaNValue
                    : INT_TO_JSVAL(l == JSVAL_TRUE);
                return equalityHelper(l, r, l_ins, r_ins, negate,
                                      tryBranchAfterCond, rval);
            }
        } else if (JSVAL_IS_SPECIAL(r)) {
            bool isVoid = !!JSVAL_IS_VOID(r);
            guard(isVoid,
                  lir->ins2(LIR_eq, r_ins, INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID))),
                  BRANCH_EXIT);
            if (!isVoid) {
                args[0] = r_ins, args[1] = cx_ins;
                r_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
                r = (r == JSVAL_VOID)
                    ? cx->runtime->NaNValue
                    : INT_TO_JSVAL(r == JSVAL_TRUE);
                return equalityHelper(l, r, l_ins, r_ins, negate,
                                      tryBranchAfterCond, rval);
            }
        } else {
            if ((JSVAL_IS_STRING(l) || isNumber(l)) && !JSVAL_IS_PRIMITIVE(r)) {
                RETURN_IF_XML_A(r);
                return InjectStatus(call_imacro(equality_imacros.any_obj));
            }
            if (!JSVAL_IS_PRIMITIVE(l) && (JSVAL_IS_STRING(r) || isNumber(r))) {
                RETURN_IF_XML_A(l);
                return InjectStatus(call_imacro(equality_imacros.obj_any));
            }
        }

        l_ins = lir->insImm(0);
        r_ins = lir->insImm(1);
        cond = false;
    }

    /* If the operands aren't numbers, compare them as integers. */
    LIns* x = lir->ins2(op, l_ins, r_ins);
    if (negate) {
        x = lir->ins_eq0(x);
        cond = !cond;
    }

    jsbytecode* pc = cx->fp->regs->pc;

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
        RETURN_IF_XML_A(l);
        if (!JSVAL_IS_PRIMITIVE(r)) {
            RETURN_IF_XML_A(r);
            return InjectStatus(call_imacro(binary_imacros.obj_obj));
        }
        return InjectStatus(call_imacro(binary_imacros.obj_any));
    }
    if (!JSVAL_IS_PRIMITIVE(r)) {
        RETURN_IF_XML_A(r);
        return InjectStatus(call_imacro(binary_imacros.any_obj));
    }

    /* 11.8.5 steps 3, 16-21. */
    if (JSVAL_IS_STRING(l) && JSVAL_IS_STRING(r)) {
        LIns* args[] = { r_ins, l_ins };
        l_ins = lir->insCall(&js_CompareStrings_ci, args);
        r_ins = lir->insImm(0);
        cond = EvalCmp(op, JSVAL_TO_STRING(l), JSVAL_TO_STRING(r));
        goto do_comparison;
    }

    /* 11.8.5 steps 4-5. */
    if (!JSVAL_IS_NUMBER(l)) {
        LIns* args[] = { l_ins, cx_ins };
        switch (JSVAL_TAG(l)) {
          case JSVAL_SPECIAL:
            l_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
            break;
          case JSVAL_STRING:
            l_ins = lir->insCall(&js_StringToNumber_ci, args);
            break;
          case JSVAL_OBJECT:
            if (JSVAL_IS_NULL(l)) {
                l_ins = lir->insImmf(0.0);
                break;
            }
            // FALL THROUGH
          case JSVAL_INT:
          case JSVAL_DOUBLE:
          default:
            JS_NOT_REACHED("JSVAL_IS_NUMBER if int/double, objects should "
                           "have been handled at start of method");
            RETURN_STOP_A("safety belt");
        }
    }
    if (!JSVAL_IS_NUMBER(r)) {
        LIns* args[] = { r_ins, cx_ins };
        switch (JSVAL_TAG(r)) {
          case JSVAL_SPECIAL:
            r_ins = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
            break;
          case JSVAL_STRING:
            r_ins = lir->insCall(&js_StringToNumber_ci, args);
            break;
          case JSVAL_OBJECT:
            if (JSVAL_IS_NULL(r)) {
                r_ins = lir->insImmf(0.0);
                break;
            }
            // FALL THROUGH
          case JSVAL_INT:
          case JSVAL_DOUBLE:
          default:
            JS_NOT_REACHED("JSVAL_IS_NUMBER if int/double, objects should "
                           "have been handled at start of method");
            RETURN_STOP_A("safety belt");
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
    cond = EvalCmp(op, lnum, rnum);
    fp = true;

    /* 11.8.5 steps 6-15. */
  do_comparison:
    /*
     * If the result is not a number or it's not a quad, we must use an integer
     * compare.
     */
    if (!fp) {
        JS_ASSERT(op >= LIR_feq && op <= LIR_fge);
        op = LOpcode(op + (LIR_eq - LIR_feq));
    }
    x = lir->ins2(op, l_ins, r_ins);

    jsbytecode* pc = cx->fp->regs->pc;

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
        return RECORD_CONTINUE;
    }
    return RECORD_STOP;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::binary(LOpcode op)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (!JSVAL_IS_PRIMITIVE(l)) {
        RETURN_IF_XML(l);
        if (!JSVAL_IS_PRIMITIVE(r)) {
            RETURN_IF_XML(r);
            return call_imacro(binary_imacros.obj_obj);
        }
        return call_imacro(binary_imacros.obj_any);
    }
    if (!JSVAL_IS_PRIMITIVE(r)) {
        RETURN_IF_XML(r);
        return call_imacro(binary_imacros.any_obj);
    }

    bool intop = !(op & LIR64);
    LIns* a = get(&l);
    LIns* b = get(&r);

    bool leftIsNumber = isNumber(l);
    jsdouble lnum = leftIsNumber ? asNumber(l) : 0;

    bool rightIsNumber = isNumber(r);
    jsdouble rnum = rightIsNumber ? asNumber(r) : 0;

    if ((op >= LIR_sub && op <= LIR_ush) ||   // sub, mul, (callh), or, xor, (not,) lsh, rsh, ush
        (op >= LIR_fsub && op <= LIR_fmod)) { // fsub, fmul, fdiv, fmod
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
    if (JSVAL_IS_SPECIAL(l)) {
        LIns* args[] = { a, cx_ins };
        a = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
        lnum = js_BooleanOrUndefinedToNumber(cx, JSVAL_TO_SPECIAL(l));
        leftIsNumber = true;
    }
    if (JSVAL_IS_SPECIAL(r)) {
        LIns* args[] = { b, cx_ins };
        b = lir->insCall(&js_BooleanOrUndefinedToNumber_ci, args);
        rnum = js_BooleanOrUndefinedToNumber(cx, JSVAL_TO_SPECIAL(r));
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
        return RECORD_CONTINUE;
    }
    return RECORD_STOP;
}

struct GuardedShapeEntry : public JSDHashEntryStub
{
    JSObject* obj;
};

#if defined DEBUG_notme && defined XP_UNIX
#include <stdio.h>

static FILE* shapefp = NULL;

static void
DumpShape(JSObject* obj, const char* prefix)
{
    JSScope* scope = OBJ_SCOPE(obj);

    if (!shapefp) {
        shapefp = fopen("/tmp/shapes.dump", "w");
        if (!shapefp)
            return;
    }

    fprintf(shapefp, "\n%s: shape %u flags %x\n", prefix, scope->shape, scope->flags);
    for (JSScopeProperty* sprop = scope->lastProperty(); sprop; sprop = sprop->parent) {
        if (JSID_IS_ATOM(sprop->id)) {
            fprintf(shapefp, " %s", JS_GetStringBytes(JSVAL_TO_STRING(ID_TO_VALUE(sprop->id))));
        } else {
            JS_ASSERT(!JSID_IS_OBJECT(sprop->id));
            fprintf(shapefp, " %d", JSID_TO_INT(sprop->id));
        }
        fprintf(shapefp, " %u %p %p %x %x %d\n",
                sprop->slot, sprop->getter, sprop->setter, sprop->attrs, sprop->flags,
                sprop->shortid);
    }
    fflush(shapefp);
}

static JSDHashOperator
DumpShapeEnumerator(JSDHashTable* table, JSDHashEntryHdr* hdr, uint32 number, void* arg)
{
    GuardedShapeEntry* entry = (GuardedShapeEntry*) hdr;
    const char* prefix = (const char*) arg;

    DumpShape(entry->obj, prefix);
    return JS_DHASH_NEXT;
}

void
TraceRecorder::dumpGuardedShapes(const char* prefix)
{
    if (guardedShapeTable.ops)
        JS_DHashTableEnumerate(&guardedShapeTable, DumpShapeEnumerator, (void*) prefix);
}
#endif /* DEBUG_notme && XP_UNIX */

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardShape(LIns* obj_ins, JSObject* obj, uint32 shape, const char* guardName,
                          LIns* map_ins, VMSideExit* exit)
{
    if (!guardedShapeTable.ops) {
        JS_DHashTableInit(&guardedShapeTable, JS_DHashGetStubOps(), NULL,
                          sizeof(GuardedShapeEntry), JS_DHASH_MIN_SIZE);
    }

    // Test (with add if missing) for a remembered guard for (obj_ins, obj).
    GuardedShapeEntry* entry = (GuardedShapeEntry*)
        JS_DHashTableOperate(&guardedShapeTable, obj_ins, JS_DHASH_ADD);
    if (!entry) {
        JS_ReportOutOfMemory(cx);
        return RECORD_ERROR;
    }

    // If already guarded, check that the shape matches.
    if (entry->key) {
        JS_ASSERT(entry->key == obj_ins);
        JS_ASSERT(entry->obj == obj);
        return RECORD_CONTINUE;
    }

    // Not yet guarded. Remember obj_ins along with obj (for invalidation).
    entry->key = obj_ins;
    entry->obj = obj;

#if defined DEBUG_notme && defined XP_UNIX
    DumpShape(obj, "guard");
    fprintf(shapefp, "for obj_ins %p\n", obj_ins);
#endif

    // Finally, emit the shape guard.
    LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)), "shape");
    guard(true,
          addName(lir->ins2i(LIR_eq, shape_ins, shape), guardName),
          exit);
    return RECORD_CONTINUE;
}

static JSDHashOperator
ForgetGuardedShapesForObject(JSDHashTable* table, JSDHashEntryHdr* hdr, uint32 number, void* arg)
{
    GuardedShapeEntry* entry = (GuardedShapeEntry*) hdr;
    if (entry->obj == arg) {
#if defined DEBUG_notme && defined XP_UNIX
        DumpShape(entry->obj, "forget");
#endif
        return JS_DHASH_REMOVE;
    }
    return JS_DHASH_NEXT;
}

void
TraceRecorder::forgetGuardedShapesForObject(JSObject* obj)
{
    if (guardedShapeTable.ops)
        JS_DHashTableEnumerate(&guardedShapeTable, ForgetGuardedShapesForObject, obj);
}

void
TraceRecorder::forgetGuardedShapes()
{
    if (guardedShapeTable.ops) {
#if defined DEBUG_notme && defined XP_UNIX
        dumpGuardedShapes("forget-all");
#endif
        JS_DHashTableFinish(&guardedShapeTable);
        guardedShapeTable.ops = NULL;
    }
}

JS_STATIC_ASSERT(offsetof(JSObjectOps, objectMap) == 0);

inline LIns*
TraceRecorder::map(LIns* obj_ins)
{
    return addName(lir->insLoad(LIR_ldp, obj_ins, (int) offsetof(JSObject, map)), "map");
}

bool
TraceRecorder::map_is_native(JSObjectMap* map, LIns* map_ins, LIns*& ops_ins, size_t op_offset)
{
    JS_ASSERT(op_offset < sizeof(JSObjectOps));
    JS_ASSERT(op_offset % sizeof(void *) == 0);

#define OP(ops) (*(void **) ((uint8 *) (ops) + op_offset))
    void* ptr = OP(map->ops);
    if (ptr != OP(&js_ObjectOps))
        return false;
#undef OP

    ops_ins = addName(lir->insLoad(LIR_ldcp, map_ins, int(offsetof(JSObjectMap, ops))), "ops");
    LIns* n = lir->insLoad(LIR_ldcp, ops_ins, op_offset);
    guard(true,
          addName(lir->ins2(LIR_peq, n, INS_CONSTPTR(ptr)), "guard(native-map)"),
          BRANCH_EXIT);

    return true;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardNativePropertyOp(JSObject* aobj, LIns* map_ins)
{
    /*
     * Interpreter calls to PROPERTY_CACHE_TEST guard on native object ops
     * which is required to use native objects (those whose maps are scopes),
     * or even more narrow conditions required because the cache miss case
     * will call a particular object-op (js_GetProperty, js_SetProperty).
     *
     * We parameterize using offsetof and guard on match against the hook at
     * the given offset in js_ObjectOps. TraceRecorder::record_JSOP_SETPROP
     * guards the js_SetProperty case.
     */
    uint32 format = js_CodeSpec[*cx->fp->regs->pc].format;
    uint32 mode = JOF_MODE(format);

    // No need to guard native-ness of global object.
    JS_ASSERT(OBJ_IS_NATIVE(globalObj));
    if (aobj != globalObj) {
        size_t op_offset = offsetof(JSObjectOps, objectMap);
        if (mode == JOF_PROP || mode == JOF_VARPROP) {
            op_offset = (format & JOF_SET)
                        ? offsetof(JSObjectOps, setProperty)
                        : offsetof(JSObjectOps, getProperty);
        } else {
            JS_ASSERT(mode == JOF_NAME);
        }

        LIns* ops_ins;
        if (!map_is_native(aobj->map, map_ins, ops_ins, op_offset))
            RETURN_STOP("non-native map");
    }
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::test_property_cache(JSObject* obj, LIns* obj_ins, JSObject*& obj2, jsuword& pcval)
{
    jsbytecode* pc = cx->fp->regs->pc;
    JS_ASSERT(*pc != JSOP_INITPROP && *pc != JSOP_INITMETHOD &&
              *pc != JSOP_SETNAME && *pc != JSOP_SETPROP && *pc != JSOP_SETMETHOD);

    // Mimic the interpreter's special case for dense arrays by skipping up one
    // hop along the proto chain when accessing a named (not indexed) property,
    // typically to find Array.prototype methods.
    JSObject* aobj = obj;
    if (OBJ_IS_DENSE_ARRAY(cx, obj)) {
        guardDenseArray(obj, obj_ins, BRANCH_EXIT);
        aobj = OBJ_GET_PROTO(cx, obj);
        obj_ins = stobj_get_proto(obj_ins);
    }

    LIns* map_ins = map(obj_ins);

    CHECK_STATUS_A(guardNativePropertyOp(aobj, map_ins));

    JSAtom* atom;
    JSPropCacheEntry* entry;
    PROPERTY_CACHE_TEST(cx, pc, aobj, obj2, entry, atom);
    if (!atom) {
        // Null atom means that obj2 is locked and must now be unlocked.
        JS_UNLOCK_OBJ(cx, obj2);
    } else {
        // Miss: pre-fill the cache for the interpreter, as well as for our needs.
        jsid id = ATOM_TO_JSID(atom);
        JSProperty* prop;
        if (JOF_OPMODE(*pc) == JOF_NAME) {
            JS_ASSERT(aobj == obj);

            JSTraceMonitor &localtm = *traceMonitor;
            entry = js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop);

            /* js_FindPropertyHelper can reenter the interpreter and kill |this|. */
            if (!localtm.recorder)
                return ARECORD_ABORTED;

            if (!entry)
                RETURN_ERROR_A("error in js_FindPropertyHelper");
            if (entry == JS_NO_PROP_CACHE_FILL)
                RETURN_STOP_A("cannot cache name");
        } else {
            JSTraceMonitor &localtm = *traceMonitor;
            JSContext *localcx = cx;
            int protoIndex = js_LookupPropertyWithFlags(cx, aobj, id,
                                                        cx->resolveFlags,
                                                        &obj2, &prop);

            /* js_LookupPropertyWithFlags can reenter the interpreter and kill |this|. */
            if (!localtm.recorder) {
                if (prop)
                    obj2->dropProperty(localcx, prop);
                return ARECORD_ABORTED;
            }

            if (protoIndex < 0)
                RETURN_ERROR_A("error in js_LookupPropertyWithFlags");

            if (prop) {
                if (!OBJ_IS_NATIVE(obj2)) {
                    obj2->dropProperty(cx, prop);
                    RETURN_STOP_A("property found on non-native object");
                }
                entry = js_FillPropertyCache(cx, aobj, 0, protoIndex, obj2,
                                             (JSScopeProperty*) prop, false);
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

            // Use PCVAL_NULL to return "no such property" to our caller.
            pcval = PCVAL_NULL;
            return ARECORD_CONTINUE;
        }

        obj2->dropProperty(cx, prop);
        if (!entry)
            RETURN_STOP_A("failed to fill property cache");
    }

#ifdef JS_THREADSAFE
    // There's a potential race in any JS_THREADSAFE embedding that's nuts
    // enough to share mutable objects on the scope or proto chain, but we
    // don't care about such insane embeddings. Anyway, the (scope, proto)
    // entry->vcap coordinates must reach obj2 from aobj at this point.
    JS_ASSERT(cx->requestDepth);
#endif

    return InjectStatus(guardPropertyCacheHit(obj_ins, map_ins, aobj, obj2, entry, pcval));
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardPropertyCacheHit(LIns* obj_ins,
                                     LIns* map_ins,
                                     JSObject* aobj,
                                     JSObject* obj2,
                                     JSPropCacheEntry* entry,
                                     jsuword& pcval)
{
    VMSideExit* exit = snapshot(BRANCH_EXIT);

    uint32 vshape = PCVCAP_SHAPE(entry->vcap);

    // Check for first-level cache hit and guard on kshape if possible.
    // Otherwise guard on key object exact match.
    if (PCVCAP_TAG(entry->vcap) <= 1) {
        if (aobj != globalObj)
            CHECK_STATUS(guardShape(obj_ins, aobj, entry->kshape, "guard_kshape", map_ins, exit));

        if (entry->adding()) {
            if (aobj == globalObj)
                RETURN_STOP("adding a property to the global object");

            LIns *vshape_ins = addName(
                lir->insLoad(LIR_ld,
                             addName(lir->insLoad(LIR_ldcp, cx_ins, offsetof(JSContext, runtime)),
                                     "runtime"),
                             offsetof(JSRuntime, protoHazardShape)),
                "protoHazardShape");
            guard(true,
                  addName(lir->ins2i(LIR_eq, vshape_ins, vshape), "guard_protoHazardShape"),
                  MISMATCH_EXIT);
        }
    } else {
#ifdef DEBUG
        JSOp op = js_GetOpcode(cx, cx->fp->script, cx->fp->regs->pc);
        JSAtom *pcatom;
        if (op == JSOP_LENGTH) {
            pcatom = cx->runtime->atomState.lengthAtom;
        } else {
            ptrdiff_t pcoff = (JOF_TYPE(js_CodeSpec[op].format) == JOF_SLOTATOM) ? SLOTNO_LEN : 0;
            GET_ATOM_FROM_BYTECODE(cx->fp->script, cx->fp->regs->pc, pcoff, pcatom);
        }
        JS_ASSERT(entry->kpc == (jsbytecode *) pcatom);
        JS_ASSERT(entry->kshape == jsuword(aobj));
#endif
        if (aobj != globalObj && !obj_ins->isconstp()) {
            guard(true,
                  addName(lir->ins2(LIR_peq, obj_ins, INS_CONSTOBJ(aobj)), "guard_kobj"),
                  exit);
        }
    }

    // For any hit that goes up the scope and/or proto chains, we will need to
    // guard on the shape of the object containing the property.
    if (PCVCAP_TAG(entry->vcap) >= 1) {
        JS_ASSERT(OBJ_SHAPE(obj2) == vshape);

        LIns* obj2_ins;
        if (PCVCAP_TAG(entry->vcap) == 1) {
            // Duplicate the special case in PROPERTY_CACHE_TEST.
            obj2_ins = addName(stobj_get_proto(obj_ins), "proto");
            guard(false, lir->ins_peq0(obj2_ins), exit);
        } else {
            obj2_ins = INS_CONSTOBJ(obj2);
        }
        CHECK_STATUS(guardShape(obj2_ins, obj2, vshape, "guard_vshape", map(obj2_ins), exit));
    }

    pcval = entry->vword;
    return RECORD_CONTINUE;
}

void
TraceRecorder::stobj_set_fslot(LIns *obj_ins, unsigned slot, LIns* v_ins)
{
    lir->insStorei(v_ins, obj_ins, offsetof(JSObject, fslots) + slot * sizeof(jsval));
}

void
TraceRecorder::stobj_set_dslot(LIns *obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins)
{
    if (!dslots_ins)
        dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    lir->insStorei(v_ins, dslots_ins, slot * sizeof(jsval));
}

void
TraceRecorder::stobj_set_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins, LIns* v_ins)
{
    if (slot < JS_INITIAL_NSLOTS) {
        stobj_set_fslot(obj_ins, slot, v_ins);
    } else {
        stobj_set_dslot(obj_ins, slot - JS_INITIAL_NSLOTS, dslots_ins, v_ins);
    }
}

LIns*
TraceRecorder::stobj_get_fslot(LIns* obj_ins, unsigned slot)
{
    JS_ASSERT(slot < JS_INITIAL_NSLOTS);
    return lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, fslots) + slot * sizeof(jsval));
}

LIns*
TraceRecorder::stobj_get_dslot(LIns* obj_ins, unsigned index, LIns*& dslots_ins)
{
    if (!dslots_ins)
        dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    return lir->insLoad(LIR_ldp, dslots_ins, index * sizeof(jsval));
}

LIns*
TraceRecorder::stobj_get_slot(LIns* obj_ins, unsigned slot, LIns*& dslots_ins)
{
    if (slot < JS_INITIAL_NSLOTS)
        return stobj_get_fslot(obj_ins, slot);
    return stobj_get_dslot(obj_ins, slot - JS_INITIAL_NSLOTS, dslots_ins);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::box_jsval(jsval v, LIns* v_ins)
{
    if (isNumber(v)) {
        LIns* args[] = { v_ins, cx_ins };
        v_ins = lir->insCall(&js_BoxDouble_ci, args);
        guard(false, lir->ins2(LIR_peq, v_ins, INS_CONSTWORD(JSVAL_ERROR_COOKIE)),
              OOM_EXIT);
        return v_ins;
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_SPECIAL:
        return lir->ins2(LIR_pior, lir->ins2i(LIR_pilsh, lir->ins_u2p(v_ins), JSVAL_TAGBITS),
                         INS_CONSTWORD(JSVAL_SPECIAL));
      case JSVAL_OBJECT:
        return v_ins;
      default:
        JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING);
        return lir->ins2(LIR_pior, v_ins, INS_CONSTWORD(JSVAL_STRING));
    }
}

JS_REQUIRES_STACK LIns*
TraceRecorder::unbox_jsval(jsval v, LIns* v_ins, VMSideExit* exit)
{
    if (isNumber(v)) {
        // JSVAL_IS_NUMBER(v)
        guard(false,
              lir->ins_eq0(lir->ins2(LIR_or,
                                     p2i(lir->ins2(LIR_piand, v_ins, INS_CONSTWORD(JSVAL_INT))),
                                     lir->ins2(LIR_peq,
                                               lir->ins2(LIR_piand, v_ins,
                                                         INS_CONSTWORD(JSVAL_TAGMASK)),
                                               INS_CONSTWORD(JSVAL_DOUBLE)))),
              exit);
        LIns* args[] = { v_ins };
        return lir->insCall(&js_UnboxDouble_ci, args);
    }
    switch (JSVAL_TAG(v)) {
      case JSVAL_SPECIAL:
        guard(true,
              lir->ins2(LIR_peq,
                        lir->ins2(LIR_piand, v_ins, INS_CONSTWORD(JSVAL_TAGMASK)),
                        INS_CONSTWORD(JSVAL_SPECIAL)),
              exit);
        return p2i(lir->ins2i(LIR_pursh, v_ins, JSVAL_TAGBITS));

      case JSVAL_OBJECT:
        if (JSVAL_IS_NULL(v)) {
            // JSVAL_NULL maps to type TT_NULL, so insist that v_ins == 0 here.
            guard(true, lir->ins_peq0(v_ins), exit);
        } else {
            guard(false, lir->ins_peq0(v_ins), exit);
            guard(true,
                  lir->ins2(LIR_peq,
                            lir->ins2(LIR_piand, v_ins, INS_CONSTWORD(JSVAL_TAGMASK)),
                            INS_CONSTWORD(JSVAL_OBJECT)),
                  exit);

            /*
             * LIR_ldcp is ok to use here even though Array classword can
             * change, because no object's classword can ever change from
             * &js_ArrayClass to &js_FunctionClass.
             */
            guard(HAS_FUNCTION_CLASS(JSVAL_TO_OBJECT(v)),
                  lir->ins2(LIR_peq,
                            lir->ins2(LIR_piand,
                                      lir->insLoad(LIR_ldcp, v_ins, offsetof(JSObject, classword)),
                                      INS_CONSTWORD(~JSSLOT_CLASS_MASK_BITS)),
                            INS_CONSTPTR(&js_FunctionClass)),
                  exit);
        }
        return v_ins;

      default:
        JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING);
        guard(true,
              lir->ins2(LIR_peq,
                        lir->ins2(LIR_piand, v_ins, INS_CONSTWORD(JSVAL_TAGMASK)),
                        INS_CONSTWORD(JSVAL_STRING)),
              exit);
        return lir->ins2(LIR_piand, v_ins, addName(lir->insImmWord(~JSVAL_TAGMASK),
                                                   "~JSVAL_TAGMASK"));
    }
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getThis(LIns*& this_ins)
{
    /*
     * js_ComputeThisForFrame updates cx->fp->argv[-1], so sample it into 'original' first.
     */
    jsval original = JSVAL_NULL;
    if (cx->fp->argv) {
        original = cx->fp->argv[-1];
        if (!JSVAL_IS_PRIMITIVE(original) &&
            guardClass(JSVAL_TO_OBJECT(original), get(&cx->fp->argv[-1]), &js_WithClass, snapshot(MISMATCH_EXIT))) {
            RETURN_STOP("can't trace getThis on With object");
        }
    }

    JSObject* thisObj = js_ComputeThisForFrame(cx, cx->fp);
    if (!thisObj)
        RETURN_ERROR("js_ComputeThisForName failed");

    /* In global code, bake in the global object as 'this' object. */
    if (!cx->fp->callee()) {
        JS_ASSERT(callDepth == 0);
        this_ins = INS_CONSTOBJ(thisObj);

        /*
         * We don't have argv[-1] in global code, so we don't update the
         * tracker here.
         */
        return RECORD_CONTINUE;
    }

    jsval& thisv = cx->fp->argv[-1];
    JS_ASSERT(JSVAL_IS_OBJECT(thisv));

    /*
     * Traces type-specialize between null and objects, so if we currently see
     * a null value in argv[-1], this trace will only match if we see null at
     * runtime as well.  Bake in the global object as 'this' object, updating
     * the tracker as well. We can only detect this condition prior to calling
     * js_ComputeThisForFrame, since it updates the interpreter's copy of
     * argv[-1].
     */
    JSClass* clasp = NULL;;
    if (JSVAL_IS_NULL(original) ||
        (((clasp = STOBJ_GET_CLASS(JSVAL_TO_OBJECT(original))) == &js_CallClass) ||
         (clasp == &js_BlockClass))) {
        if (clasp)
            guardClass(JSVAL_TO_OBJECT(original), get(&thisv), clasp, snapshot(BRANCH_EXIT));
        JS_ASSERT(!JSVAL_IS_PRIMITIVE(thisv));
        if (thisObj != globalObj)
            RETURN_STOP("global object was wrapped while recording");
        this_ins = INS_CONSTOBJ(thisObj);
        set(&thisv, this_ins);
        return RECORD_CONTINUE;
    }

    this_ins = get(&thisv);

    JSObject* wrappedGlobal = globalObj->thisObject(cx);
    if (!wrappedGlobal)
        RETURN_ERROR("globalObj->thisObject hook threw in getThis");

    /*
     * The only unwrapped object that needs to be wrapped that we can get here
     * is the global object obtained throught the scope chain.
     */
    this_ins = lir->ins_choose(lir->ins_peq0(stobj_get_parent(this_ins)),
                               INS_CONSTOBJ(wrappedGlobal),
                               this_ins, avmplus::AvmCore::use_cmov());
    return RECORD_CONTINUE;
}


LIns*
TraceRecorder::getStringLength(LIns* str_ins)
{
    LIns* len_ins = lir->insLoad(LIR_ldp, str_ins, (int)offsetof(JSString, mLength));

    LIns* masked_len_ins = lir->ins2(LIR_piand,
                                     len_ins,
                                     INS_CONSTWORD(JSString::LENGTH_MASK));

    LIns* real_len =
        lir->ins_choose(lir->ins_peq0(lir->ins2(LIR_piand,
                                                len_ins,
                                                INS_CONSTWORD(JSString::DEPENDENT))),
                        masked_len_ins,
                        lir->ins_choose(lir->ins_peq0(lir->ins2(LIR_piand,
                                                                len_ins,
                                                                INS_CONSTWORD(JSString::PREFIX))),
                                        lir->ins2(LIR_piand,
                                                  len_ins,
                                                  INS_CONSTWORD(JSString::DEPENDENT_LENGTH_MASK)),
                                        masked_len_ins, avmplus::AvmCore::use_cmov()),
                        avmplus::AvmCore::use_cmov());
    return p2i(real_len);
}

JS_REQUIRES_STACK bool
TraceRecorder::guardClass(JSObject* obj, LIns* obj_ins, JSClass* clasp, VMSideExit* exit)
{
    bool cond = STOBJ_GET_CLASS(obj) == clasp;

    LIns* class_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, classword));
    class_ins = lir->ins2(LIR_piand, class_ins, INS_CONSTWORD(~JSSLOT_CLASS_MASK_BITS));

    char namebuf[32];
    JS_snprintf(namebuf, sizeof namebuf, "guard(class is %s)", clasp->name);
    guard(cond, addName(lir->ins2(LIR_peq, class_ins, INS_CONSTPTR(clasp)), namebuf), exit);
    return cond;
}

JS_REQUIRES_STACK bool
TraceRecorder::guardDenseArray(JSObject* obj, LIns* obj_ins, ExitType exitType)
{
    return guardClass(obj, obj_ins, &js_ArrayClass, snapshot(exitType));
}

JS_REQUIRES_STACK bool
TraceRecorder::guardDenseArray(JSObject* obj, LIns* obj_ins, VMSideExit* exit)
{
    return guardClass(obj, obj_ins, &js_ArrayClass, exit);
}

JS_REQUIRES_STACK bool
TraceRecorder::guardHasPrototype(JSObject* obj, LIns* obj_ins,
                                 JSObject** pobj, LIns** pobj_ins,
                                 VMSideExit* exit)
{
    *pobj = obj->getProto();
    *pobj_ins = stobj_get_proto(obj_ins);

    bool cond = *pobj == NULL;
    guard(cond, addName(lir->ins_peq0(*pobj_ins), "guard(proto-not-null)"), exit);
    return !cond;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardPrototypeHasNoIndexedProperties(JSObject* obj, LIns* obj_ins, ExitType exitType)
{
    /*
     * Guard that no object along the prototype chain has any indexed
     * properties which might become visible through holes in the array.
     */
    VMSideExit* exit = snapshot(exitType);

    if (js_PrototypeHasIndexedProperties(cx, obj))
        return RECORD_STOP;

    while (guardHasPrototype(obj, obj_ins, &obj, &obj_ins, exit))
        CHECK_STATUS(guardShape(obj_ins, obj, OBJ_SHAPE(obj), "guard(shape)", map(obj_ins), exit));
    return RECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::guardNotGlobalObject(JSObject* obj, LIns* obj_ins)
{
    if (obj == globalObj)
        RETURN_STOP("reference aliases global object");
    guard(false, lir->ins2(LIR_peq, obj_ins, INS_CONSTOBJ(globalObj)), MISMATCH_EXIT);
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK void
TraceRecorder::clearFrameSlotsFromCache()
{
    /*
     * Clear out all slots of this frame in the nativeFrameTracker. Different
     * locations on the VM stack might map to different locations on the native
     * stack depending on the number of arguments (i.e.) of the next call, so
     * we have to make sure we map those in to the cache with the right
     * offsets.
     */
    JSStackFrame* fp = cx->fp;
    jsval* vp;
    jsval* vpstop;

    /*
     * Duplicate native stack layout computation: see VisitFrameSlots header comment.
     * This doesn't do layout arithmetic, but it must clear out all the slots defined as
     * imported by VisitFrameSlots.
     */
    if (fp->argv) {
        vp = &fp->argv[-2];
        vpstop = &fp->argv[argSlots(fp)];
        while (vp < vpstop)
            nativeFrameTracker.set(vp++, (LIns*)0);
        nativeFrameTracker.set(&fp->argsobj, (LIns*)0);
    }
    vp = &fp->slots[0];
    vpstop = &fp->slots[fp->script->nslots];
    while (vp < vpstop)
        nativeFrameTracker.set(vp++, (LIns*)0);
}

/*
 * If we have created an |arguments| object for the frame, we must copy the
 * argument values into the object as properties in case it is used after
 * this frame returns.
 */
JS_REQUIRES_STACK void
TraceRecorder::putArguments()
{
    if (cx->fp->argsobj && cx->fp->argc) {
        LIns* argsobj_ins = get(&cx->fp->argsobj);
        LIns* args_ins = lir->insAlloc(sizeof(jsval) * cx->fp->argc);
        for (uintN i = 0; i < cx->fp->argc; ++i) {
            LIns* arg_ins = box_jsval(cx->fp->argv[i], get(&cx->fp->argv[i]));
            lir->insStorei(arg_ins, args_ins, i * sizeof(jsval));
        }
        LIns* args[] = { args_ins, argsobj_ins, cx_ins };
        lir->insCall(&js_PutArguments_ci, args);
    }
}

static JS_REQUIRES_STACK inline bool
IsTraceableRecursion(JSContext *cx)
{
    JSStackFrame *fp = cx->fp;
    JSStackFrame *down = cx->fp->down;
    if (!down)
        return false;
    if (down->script != fp->script)
        return false;
    if (down->argc != fp->argc)
        return false;
    if (fp->argc != fp->fun->nargs)
        return false;
    if (fp->imacpc || down->imacpc)
        return false;
    if ((fp->flags & JSFRAME_CONSTRUCTING) || (down->flags & JSFRAME_CONSTRUCTING))
        return false;
    if (*fp->script->code != JSOP_TRACE)
        return false;
    return true;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_EnterFrame(uintN& inlineCallCount)
{
    JSStackFrame* fp = cx->fp;

    if (++callDepth >= MAX_CALLDEPTH)
        RETURN_STOP_A("exceeded maximum call depth");

    debug_only_printf(LC_TMTracer, "EnterFrame %s, callDepth=%d\n",
                      js_AtomToPrintableString(cx, cx->fp->fun->atom),
                      callDepth);
    debug_only_stmt(
        if (js_LogController.lcbits & LC_TMRecorder) {
            js_Disassemble(cx, cx->fp->script, JS_TRUE, stdout);
            debug_only_print0(LC_TMTracer, "----\n");
        }
    )
    LIns* void_ins = INS_VOID();

    // Duplicate native stack layout computation: see VisitFrameSlots header comment.
    // This doesn't do layout arithmetic, but it must initialize in the tracker all the
    // slots defined as imported by VisitFrameSlots.
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
    set(&fp->argsobj, INS_NULL(), true);

    /*
     * Check for recursion. This is a special check for recursive cases that can be
     * a trace-tree, just like a loop. If recursion acts weird, for example
     * differing argc or existence of an imacpc, it's not something this code is
     * concerned about. That should pass through below to not regress pre-recursion
     * functionality.
     */
    if (IsTraceableRecursion(cx) && treeInfo->script == cx->fp->script) {
        if (treeInfo->recursion == Recursion_Disallowed)
            RETURN_STOP_A("recursion not allowed in this tree");
        if (treeInfo->script != cx->fp->script)
            RETURN_STOP_A("recursion does not match original tree");
        return InjectStatus(downRecursion());
    }

    /* Try inlining one level in case this recursion doesn't go too deep. */
    if (fp->script == fp->down->script &&
        fp->down->down && fp->down->down->script == fp->script) {
        RETURN_STOP_A("recursion started inlining");
    }

    TreeFragment* root = fragment->root;
    TreeFragment* first = LookupLoop(&JS_TRACE_MONITOR(cx), fp->regs->pc, root->globalObj,
                                   root->globalShape, fp->argc);
    if (!first)
        return ARECORD_CONTINUE;
    TreeFragment* f = findNestedCompatiblePeer(first);
    if (!f) {
        /*
         * If there were no compatible peers, but there were peers at all, then it is probable that
         * an inner recursive function is type mismatching. Start a new recorder that must be
         * recursive.
         */
        for (f = first; f; f = f->peer) {
            if (f->treeInfo && f->treeInfo->recursion == Recursion_Detected) {
                /* Since this recorder is about to die, save its values. */
                if (++first->hits() <= HOTLOOP)
                    return ARECORD_STOP;
                if (IsBlacklisted((jsbytecode*)f->ip))
                    RETURN_STOP_A("inner recursive tree is blacklisted");
                JSContext* _cx = cx;
                SlotList* globalSlots = treeInfo->globalSlots;
                JSTraceMonitor* tm = traceMonitor;
                js_AbortRecording(cx, "trying to compile inner recursive tree");
                if (RecordTree(_cx, tm, first, NULL, 0, first->globalObj, first->globalShape,
                               globalSlots, _cx->fp->argc, Record_EnterFrame)) {
                    JS_ASSERT(tm->recorder);
                }
                break;
            }
        }
        return ARECORD_CONTINUE;
    } else if (f) {
        /*
         * Make sure the shape of the global object still matches (this might
         * flush the JIT cache).
         */
        JSObject* globalObj = JS_GetGlobalForObject(cx, cx->fp->scopeChain);
        uint32 globalShape = -1;
        SlotList* globalSlots = NULL;
        if (!CheckGlobalObjectShape(cx, traceMonitor, globalObj, &globalShape, &globalSlots))
            return ARECORD_ABORTED;
        return attemptTreeCall(f, inlineCallCount);
    }

   return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_LeaveFrame()
{
    debug_only_stmt(
        if (cx->fp->fun)
            debug_only_printf(LC_TMTracer,
                              "LeaveFrame (back to %s), callDepth=%d\n",
                              js_AtomToPrintableString(cx, cx->fp->fun->atom),
                              callDepth);
        );

    JS_ASSERT(js_CodeSpec[js_GetOpcode(cx, cx->fp->script,
              cx->fp->regs->pc)].length == JSOP_CALL_LENGTH);

    if (callDepth-- <= 0)
        RETURN_STOP_A("returned out of a loop we started tracing");

    // LeaveFrame gets called after the interpreter popped the frame and
    // stored rval, so cx->fp not cx->fp->down, and -1 not 0.
    atoms = FrameAtomBase(cx, cx->fp);
    set(&stackval(-1), rval_ins, true);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PUSH()
{
    stack(0, INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POPV()
{
    jsval& rval = stackval(-1);
    LIns *rval_ins = box_jsval(rval, get(&rval));

    // Store it in cx->fp->rval. NB: Tricky dependencies. cx->fp is the right
    // frame because POPV appears only in global and eval code and we don't
    // trace JSOP_EVAL or leaving the frame where tracing started.
    LIns *fp_ins = lir->insLoad(LIR_ldp, cx_ins, offsetof(JSContext, fp));
    lir->insStorei(rval_ins, fp_ins, offsetof(JSStackFrame, rval));
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

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RETURN()
{
    /* A return from callDepth 0 terminates the current loop, except for recursion. */
    if (callDepth == 0) {
        if (IsTraceableRecursion(cx) && treeInfo->recursion != Recursion_Disallowed &&
            treeInfo->script == cx->fp->script) {
            return InjectStatus(upRecursion());
        } else {
            AUDIT(returnLoopExits);
            return endLoop();
        }
    }

    putArguments();

    /* If we inlined this function call, make the return value available to the caller code. */
    jsval& rval = stackval(-1);
    JSStackFrame *fp = cx->fp;
    if ((cx->fp->flags & JSFRAME_CONSTRUCTING) && JSVAL_IS_PRIMITIVE(rval)) {
        JS_ASSERT(fp->thisv == fp->argv[-1]);
        rval_ins = get(&fp->argv[-1]);
    } else {
        rval_ins = get(&rval);
    }
    debug_only_printf(LC_TMTracer,
                      "returning from %s\n",
                      js_AtomToPrintableString(cx, cx->fp->fun->atom));
    clearFrameSlotsFromCache();

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
    jssrcnote* sn = js_GetSrcNote(cx->fp->script, cx->fp->regs->pc);

    if (sn && (SN_TYPE(sn) == SRC_BREAK || SN_TYPE(sn) == SRC_CONT2LABEL)) {
        AUDIT(breakLoopExits);
        return endLoop();
    }
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFEQ()
{
    trackCfgMerges(cx->fp->regs->pc);
    return ifop();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFNE()
{
    return ifop();
}

LIns*
TraceRecorder::newArguments()
{
    LIns* global_ins = INS_CONSTOBJ(globalObj);
    LIns* argc_ins = INS_CONST(cx->fp->argc);
    LIns* callee_ins = get(&cx->fp->argv[-2]);
    LIns* argv_ins = cx->fp->argc
        ? lir->ins2(LIR_piadd, lirbuf->sp, 
                    lir->insImmWord(nativespOffset(&cx->fp->argv[0])))
        : INS_CONSTPTR((void *) 2);
    js_ArgsPrivateNative *apn = js_ArgsPrivateNative::create(traceAlloc(), cx->fp->argc);
    for (uintN i = 0; i < cx->fp->argc; ++i) {
        apn->typemap()[i] = determineSlotType(&cx->fp->argv[i]);
    }

    LIns* args[] = { INS_CONSTPTR(apn), argv_ins, callee_ins, argc_ins, global_ins, cx_ins };
    LIns* call_ins = lir->insCall(&js_Arguments_ci, args);
    guard(false, lir->ins_peq0(call_ins), OOM_EXIT);
    return call_ins;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGUMENTS()
{
    if (cx->fp->flags & JSFRAME_OVERRIDE_ARGS)
        RETURN_STOP_A("Can't trace |arguments| if |arguments| is assigned to");

    LIns* a_ins = get(&cx->fp->argsobj);
    LIns* args_ins;
    if (a_ins->opcode() == LIR_int) {
        // |arguments| is set to 0 by EnterFrame on this trace, so call to create it.
        args_ins = newArguments();
    } else {
        // Generate LIR to create arguments only if it has not already been created.
        
        LIns* mem_ins = lir->insAlloc(sizeof(jsval));

        LIns* br1 = lir->insBranch(LIR_jt, lir->ins_peq0(a_ins), NULL);
        lir->insStorei(a_ins, mem_ins, 0);
        LIns* br2 = lir->insBranch(LIR_j, NULL, NULL);

        LIns* label1 = lir->ins0(LIR_label);
        br1->setTarget(label1);

        LIns* call_ins = newArguments();
        lir->insStorei(call_ins, mem_ins, 0);

        LIns* label2 = lir->ins0(LIR_label);
        br2->setTarget(label2);

        args_ins = lir->insLoad(LIR_ldp, mem_ins, 0);
    }

    stack(0, args_ins);
    set(&cx->fp->argsobj, args_ins);
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
    jsval& l = stackval(-2);
    jsval& r = stackval(-1);
    LIns* l_ins = get(&l);
    LIns* r_ins = get(&r);
    set(&r, l_ins);
    set(&l, r_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_PICK()
{
    jsval* sp = cx->fp->regs->sp;
    jsint n = cx->fp->regs->pc[1];
    JS_ASSERT(sp - (n+1) >= StackBase(cx->fp));
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
    return InjectStatus(binary(LIR_or));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITXOR()
{
    return InjectStatus(binary(LIR_xor));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITAND()
{
    return InjectStatus(binary(LIR_and));
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
    return relational(LIR_flt, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LE()
{
    return relational(LIR_fle, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GT()
{
    return relational(LIR_fgt, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GE()
{
    return relational(LIR_fge, true);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LSH()
{
    return InjectStatus(binary(LIR_lsh));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RSH()
{
    return InjectStatus(binary(LIR_rsh));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_URSH()
{
    return InjectStatus(binary(LIR_ush));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ADD()
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);

    if (!JSVAL_IS_PRIMITIVE(l)) {
        RETURN_IF_XML_A(l);
        if (!JSVAL_IS_PRIMITIVE(r)) {
            RETURN_IF_XML_A(r);
            return InjectStatus(call_imacro(add_imacros.obj_obj));
        }
        return InjectStatus(call_imacro(add_imacros.obj_any));
    }
    if (!JSVAL_IS_PRIMITIVE(r)) {
        RETURN_IF_XML_A(r);
        return InjectStatus(call_imacro(add_imacros.any_obj));
    }

    if (JSVAL_IS_STRING(l) || JSVAL_IS_STRING(r)) {
        LIns* args[] = { stringify(r), stringify(l), cx_ins };
        LIns* concat = lir->insCall(&js_ConcatStrings_ci, args);
        guard(false, lir->ins_peq0(concat), OOM_EXIT);
        set(&l, concat);
        return ARECORD_CONTINUE;
    }

    return InjectStatus(binary(LIR_fadd));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SUB()
{
    return InjectStatus(binary(LIR_fsub));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_MUL()
{
    return InjectStatus(binary(LIR_fmul));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DIV()
{
    return InjectStatus(binary(LIR_fdiv));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_MOD()
{
    return InjectStatus(binary(LIR_fmod));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NOT()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_SPECIAL(v)) {
        set(&v, lir->ins_eq0(lir->ins2i(LIR_eq, get(&v), 1)));
        return ARECORD_CONTINUE;
    }
    if (isNumber(v)) {
        LIns* v_ins = get(&v);
        set(&v, lir->ins2(LIR_or, lir->ins2(LIR_feq, v_ins, lir->insImmf(0)),
                                  lir->ins_eq0(lir->ins2(LIR_feq, v_ins, v_ins))));
        return ARECORD_CONTINUE;
    }
    if (JSVAL_TAG(v) == JSVAL_OBJECT) {
        set(&v, lir->ins_peq0(get(&v)));
        return ARECORD_CONTINUE;
    }
    JS_ASSERT(JSVAL_IS_STRING(v));
    set(&v, lir->ins_peq0(lir->ins2(LIR_piand,
                                    lir->insLoad(LIR_ldp, get(&v), (int)offsetof(JSString, mLength)),
                                    INS_CONSTWORD(JSString::LENGTH_MASK))));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BITNOT()
{
    return InjectStatus(unary(LIR_not));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEG()
{
    jsval& v = stackval(-1);

    if (!JSVAL_IS_PRIMITIVE(v)) {
        RETURN_IF_XML_A(v);
        return InjectStatus(call_imacro(unary_imacros.sign));
    }

    if (isNumber(v)) {
        LIns* a = get(&v);

        /*
         * If we're a promoted integer, we have to watch out for 0s since -0 is
         * a double. Only follow this path if we're not an integer that's 0 and
         * we're not a double that's zero.
         */
        if (!oracle.isInstructionUndemotable(cx->fp->regs->pc) &&
            isPromoteInt(a) &&
            (!JSVAL_IS_INT(v) || JSVAL_TO_INT(v) != 0) &&
            (!JSVAL_IS_DOUBLE(v) || !JSDOUBLE_IS_NEGZERO(*JSVAL_TO_DOUBLE(v))) &&
            -asNumber(v) == (int)-asNumber(v)) {
            a = lir->ins1(LIR_neg, ::demote(lir, a));
            if (!a->isconst()) {
                VMSideExit* exit = snapshot(OVERFLOW_EXIT);
                guard(false, lir->ins1(LIR_ov, a), exit);
                guard(false, lir->ins2i(LIR_eq, a, 0), exit);
            }
            a = lir->ins1(LIR_i2f, a);
        } else {
            a = lir->ins1(LIR_fneg, a);
        }

        set(&v, a);
        return ARECORD_CONTINUE;
    }

    if (JSVAL_IS_NULL(v)) {
        set(&v, lir->insImmf(-0.0));
        return ARECORD_CONTINUE;
    }

    JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING || JSVAL_IS_SPECIAL(v));

    LIns* args[] = { get(&v), cx_ins };
    set(&v, lir->ins1(LIR_fneg,
                      lir->insCall(JSVAL_IS_STRING(v)
                                   ? &js_StringToNumber_ci
                                   : &js_BooleanOrUndefinedToNumber_ci,
                                   args)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POS()
{
    jsval& v = stackval(-1);

    if (!JSVAL_IS_PRIMITIVE(v)) {
        RETURN_IF_XML_A(v);
        return InjectStatus(call_imacro(unary_imacros.sign));
    }

    if (isNumber(v))
        return ARECORD_CONTINUE;

    if (JSVAL_IS_NULL(v)) {
        set(&v, lir->insImmf(0));
        return ARECORD_CONTINUE;
    }

    JS_ASSERT(JSVAL_TAG(v) == JSVAL_STRING || JSVAL_IS_SPECIAL(v));

    LIns* args[] = { get(&v), cx_ins };
    set(&v, lir->insCall(JSVAL_IS_STRING(v)
                         ? &js_StringToNumber_ci
                         : &js_BooleanOrUndefinedToNumber_ci,
                         args));
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
    jsval& v = stackval(-1);
    RETURN_IF_XML_A(v);
    return ARECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::getClassPrototype(JSObject* ctor, LIns*& proto_ins)
{
    // ctor must be a function created via js_InitClass.
#ifdef DEBUG
    JSClass *clasp = FUN_CLASP(GET_FUNCTION_PRIVATE(cx, ctor));
    JS_ASSERT(clasp);

    JSTraceMonitor &localtm = JS_TRACE_MONITOR(cx);
#endif

    jsval pval;
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
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(pval));
    JSObject *proto = JSVAL_TO_OBJECT(pval);
    JS_ASSERT_IF(clasp != &js_ArrayClass, OBJ_SCOPE(proto)->emptyScope->clasp == clasp);

    proto_ins = INS_CONSTOBJ(proto);
    return RECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::getClassPrototype(JSProtoKey key, LIns*& proto_ins)
{
#ifdef DEBUG
    JSTraceMonitor &localtm = JS_TRACE_MONITOR(cx);
#endif

    JSObject* proto;
    if (!js_GetClassPrototype(cx, globalObj, INT_TO_JSID(key), &proto))
        RETURN_ERROR("error in js_GetClassPrototype");

    // This should not have reentered.
    JS_ASSERT(localtm.recorder);

    // If we might end up passing the proto to JSObject::initSharingEmptyScope,
    // we must check here that proto has a matching emptyScope. We skip the
    // check for Array.prototype because new arrays, being non-native, are
    // never initialized using initSharingEmptyScope.
    if (key != JSProto_Array) {
        if (!OBJ_IS_NATIVE(proto))
            RETURN_STOP("non-native class prototype");
        JSEmptyScope *emptyScope = OBJ_SCOPE(proto)->emptyScope;
        if (!emptyScope || JSCLASS_CACHED_PROTO_KEY(emptyScope->clasp) != key)
            RETURN_STOP("class prototype is not the standard one");
    }

    proto_ins = INS_CONSTOBJ(proto);
    return RECORD_CONTINUE;
}

#define IGNORE_NATIVE_CALL_COMPLETE_CALLBACK ((JSSpecializedNative*)1)

RecordingStatus
TraceRecorder::newString(JSObject* ctor, uint32 argc, jsval* argv, jsval* rval)
{
    JS_ASSERT(argc == 1);

    if (!JSVAL_IS_PRIMITIVE(argv[0])) {
        RETURN_IF_XML(argv[0]);
        return call_imacro(new_imacros.String);
    }

    LIns* proto_ins;
    CHECK_STATUS(getClassPrototype(ctor, proto_ins));

    LIns* args[] = { stringify(argv[0]), proto_ins, cx_ins };
    LIns* obj_ins = lir->insCall(&js_String_tn_ci, args);
    guard(false, lir->ins_peq0(obj_ins), OOM_EXIT);

    set(rval, obj_ins);
    pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
    return RECORD_CONTINUE;
}

RecordingStatus
TraceRecorder::newArray(JSObject* ctor, uint32 argc, jsval* argv, jsval* rval)
{
    LIns *proto_ins;
    CHECK_STATUS(getClassPrototype(ctor, proto_ins));

    LIns *arr_ins;
    if (argc == 0) {
        // arr_ins = js_NewEmptyArray(cx, Array.prototype)
        LIns *args[] = { proto_ins, cx_ins };
        arr_ins = lir->insCall(&js_NewEmptyArray_ci, args);
        guard(false, lir->ins_peq0(arr_ins), OOM_EXIT);
    } else if (argc == 1 && JSVAL_IS_NUMBER(argv[0])) {
        // arr_ins = js_NewEmptyArray(cx, Array.prototype, length)
        LIns *args[] = { f2i(get(argv)), proto_ins, cx_ins }; // FIXME: is this 64-bit safe?
        arr_ins = lir->insCall(&js_NewEmptyArrayWithLength_ci, args);
        guard(false, lir->ins_peq0(arr_ins), OOM_EXIT);
    } else {
        // arr_ins = js_NewArrayWithSlots(cx, Array.prototype, argc)
        LIns *args[] = { INS_CONST(argc), proto_ins, cx_ins };
        arr_ins = lir->insCall(&js_NewArrayWithSlots_ci, args);
        guard(false, lir->ins_peq0(arr_ins), OOM_EXIT);

        // arr->dslots[i] = box_jsval(vp[i]);  for i in 0..argc
        LIns *dslots_ins = NULL;
        for (uint32 i = 0; i < argc && !outOfMemory(); i++) {
            LIns *elt_ins = box_jsval(argv[i], get(&argv[i]));
            stobj_set_dslot(arr_ins, i, dslots_ins, elt_ins);
        }

        if (argc > 0)
            stobj_set_fslot(arr_ins, JSSLOT_ARRAY_COUNT, INS_CONST(argc));
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
     * JSBUILTIN_ERROR bit of cx->builtinStatus.
     *
     * If the return value (ok_ins) is true, status' == status. Otherwise
     * status' = status | JSBUILTIN_ERROR. We calculate (rval&1)^1, which is 1
     * if rval is JS_FALSE (error), and then shift that by 1, which is the log2
     * of JSBUILTIN_ERROR.
     */
    JS_STATIC_ASSERT(((JS_TRUE & 1) ^ 1) << 1 == 0);
    JS_STATIC_ASSERT(((JS_FALSE & 1) ^ 1) << 1 == JSBUILTIN_ERROR);
    status_ins = lir->ins2(LIR_or,
                           status_ins,
                           lir->ins2i(LIR_lsh,
                                      lir->ins2i(LIR_xor,
                                                 lir->ins2i(LIR_and, ok_ins, 1),
                                                 1),
                                      1));
    lir->insStorei(status_ins, lirbuf->state, (int) offsetof(InterpState, builtinStatus));
}

JS_REQUIRES_STACK void
TraceRecorder::emitNativePropertyOp(JSScope* scope, JSScopeProperty* sprop, LIns* obj_ins,
                                    bool setflag, LIns* boxed_ins)
{
    JS_ASSERT(!(sprop->attrs & (setflag ? JSPROP_SETTER : JSPROP_GETTER)));
    JS_ASSERT(setflag ? !SPROP_HAS_STUB_SETTER(sprop) : !SPROP_HAS_STUB_GETTER_OR_IS_METHOD(sprop));

    enterDeepBailCall();

    // It is unsafe to pass the address of an object slot as the out parameter,
    // because the getter or setter could end up resizing the object's dslots.
    // Instead, use a word of stack and root it in nativeVp.
    LIns* vp_ins = lir->insAlloc(sizeof(jsval));
    lir->insStorei(vp_ins, lirbuf->state, offsetof(InterpState, nativeVp));
    lir->insStorei(INS_CONST(1), lirbuf->state, offsetof(InterpState, nativeVpLen));
    if (setflag)
        lir->insStorei(boxed_ins, vp_ins, 0);

    CallInfo* ci = new (traceAlloc()) CallInfo();
    ci->_address = uintptr_t(setflag ? sprop->setter : sprop->getter);
    ci->_argtypes = ARGSIZE_I << (0*ARGSIZE_SHIFT) |
                    ARGSIZE_P << (1*ARGSIZE_SHIFT) |
                    ARGSIZE_P << (2*ARGSIZE_SHIFT) |
                    ARGSIZE_P << (3*ARGSIZE_SHIFT) |
                    ARGSIZE_P << (4*ARGSIZE_SHIFT);
    ci->_cse = ci->_fold = 0;
    ci->_abi = ABI_CDECL;
#ifdef DEBUG
    ci->_name = "JSPropertyOp";
#endif
    LIns* args[] = { vp_ins, INS_CONSTVAL(SPROP_USERID(sprop)), obj_ins, cx_ins };
    LIns* ok_ins = lir->insCall(ci, args);

    // Cleanup. Immediately clear nativeVp before we might deep bail.
    lir->insStorei(INS_NULL(), lirbuf->state, offsetof(InterpState, nativeVp));
    leaveDeepBailCall();

    // Guard that the call succeeded and builtinStatus is still 0.
    // If the native op succeeds but we deep-bail here, the result value is
    // lost!  Therefore this can only be used for setters of shared properties.
    // In that case we ignore the result value anyway.
    LIns* status_ins = lir->insLoad(LIR_ld,
                                    lirbuf->state,
                                    (int) offsetof(InterpState, builtinStatus));
    propagateFailureToBuiltinStatus(ok_ins, status_ins);
    guard(true, lir->ins_eq0(status_ins), STATUS_EXIT);

    // Re-load the value--but this is currently unused, so commented out.
    //boxed_ins = lir->insLoad(LIR_ldp, vp_ins, 0);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::emitNativeCall(JSSpecializedNative* sn, uintN argc, LIns* args[], bool rooted)
{
    bool constructing = !!(sn->flags & JSTN_CONSTRUCTOR);

    if (JSTN_ERRTYPE(sn) == FAIL_STATUS) {
        // This needs to capture the pre-call state of the stack. So do not set
        // pendingSpecializedNative before taking this snapshot.
        JS_ASSERT(!pendingSpecializedNative);

        // Take snapshot for js_DeepBail and store it in cx->bailExit.
        // If we are calling a slow native, add information to the side exit
        // for SynthesizeSlowNativeFrame.
        VMSideExit* exit = enterDeepBailCall();
        JSObject* funobj = JSVAL_TO_OBJECT(stackval(0 - (2 + argc)));
        if (FUN_SLOW_NATIVE(GET_FUNCTION_PRIVATE(cx, funobj))) {
            exit->setNativeCallee(funobj, constructing);
            treeInfo->gcthings.addUnique(OBJECT_TO_JSVAL(funobj));
        }
    }

    LIns* res_ins = lir->insCall(sn->builtin, args);

    // Immediately unroot the vp as soon we return since we might deep bail next.
    if (rooted)
        lir->insStorei(INS_NULL(), lirbuf->state, offsetof(InterpState, nativeVp));

    rval_ins = res_ins;
    switch (JSTN_ERRTYPE(sn)) {
      case FAIL_NULL:
        guard(false, lir->ins_peq0(res_ins), OOM_EXIT);
        break;
      case FAIL_NEG:
        res_ins = lir->ins1(LIR_i2f, res_ins);
        guard(false, lir->ins2(LIR_flt, res_ins, lir->insImmf(0)), OOM_EXIT);
        break;
      case FAIL_VOID:
        guard(false, lir->ins2i(LIR_eq, res_ins, JSVAL_TO_SPECIAL(JSVAL_VOID)), OOM_EXIT);
        break;
      case FAIL_COOKIE:
        guard(false, lir->ins2(LIR_peq, res_ins, INS_CONSTWORD(JSVAL_ERROR_COOKIE)), OOM_EXIT);
        break;
      default:;
    }

    set(&stackval(0 - (2 + argc)), res_ins);

    /*
     * The return value will be processed by NativeCallComplete since
     * we have to know the actual return value type for calls that return
     * jsval (like Array_p_pop).
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
    JSStackFrame* fp = cx->fp;
    jsbytecode *pc = fp->regs->pc;

    jsval& fval = stackval(0 - (2 + argc));
    jsval& tval = stackval(0 - (1 + argc));

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
                if (JSVAL_IS_PRIMITIVE(tval))
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'S') { /* this, as a string */
                if (!JSVAL_IS_STRING(tval))
                    goto next_specialization;
                *argp = this_ins;
            } else if (argtype == 'f') {
                *argp = INS_CONSTOBJ(JSVAL_TO_OBJECT(fval));
            } else if (argtype == 'p') {
                CHECK_STATUS(getClassPrototype(JSVAL_TO_OBJECT(fval), *argp));
            } else if (argtype == 'R') {
                *argp = INS_CONSTPTR(cx->runtime);
            } else if (argtype == 'P') {
                // FIXME: Set pc to imacpc when recording JSOP_CALL inside the
                //        JSOP_GETELEM imacro (bug 476559).
                if ((*pc == JSOP_CALL) &&
                    fp->imacpc && *fp->imacpc == JSOP_GETELEM)
                    *argp = INS_CONSTPTR(fp->imacpc);
                else
                    *argp = INS_CONSTPTR(pc);
            } else if (argtype == 'D') { /* this, as a number */
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

            argtype = sn->argtypes[i];
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
                *argp = box_jsval(arg, *argp);
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

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::callNative(uintN argc, JSOp mode)
{
    LIns* args[5];

    JS_ASSERT(mode == JSOP_CALL || mode == JSOP_NEW || mode == JSOP_APPLY);

    jsval* vp = &stackval(0 - (2 + argc));
    JSObject* funobj = JSVAL_TO_OBJECT(vp[0]);
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, funobj);
    JSFastNative native = (JSFastNative)fun->u.n.native;

    switch (argc) {
      case 1:
        if (isNumber(vp[2]) &&
            (native == js_math_ceil || native == js_math_floor || native == js_math_round)) {
            LIns* a = get(&vp[2]);
            if (isPromote(a)) {
                set(&vp[0], a);
                pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                return RECORD_CONTINUE;
            }
        }
        break;

      case 2:
        if (isNumber(vp[2]) && isNumber(vp[3]) &&
            (native == js_math_min || native == js_math_max)) {
            LIns* a = get(&vp[2]);
            LIns* b = get(&vp[3]);
            if (isPromote(a) && isPromote(b)) {
                a = ::demote(lir, a);
                b = ::demote(lir, b);
                set(&vp[0],
                    lir->ins1(LIR_i2f,
                              lir->ins_choose(lir->ins2((native == js_math_min)
                                                        ? LIR_lt
                                                        : LIR_gt, a, b),
                                              a, b, avmplus::AvmCore::use_cmov())));
                pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
                return RECORD_CONTINUE;
            }
        }
        break;
    }

    if (fun->flags & JSFUN_TRCINFO) {
        JSNativeTraceInfo *trcinfo = FUN_TRCINFO(fun);
        JS_ASSERT(trcinfo && (JSFastNative)fun->u.n.native == trcinfo->native);

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
    uintN vplen = 2 + JS_MAX(argc, unsigned(FUN_MINARGS(fun))) + fun->u.n.extra;
    if (!(fun->flags & JSFUN_FAST_NATIVE))
        vplen++; // slow native return value slot
    LIns* invokevp_ins = lir->insAlloc(vplen * sizeof(jsval));

    // vp[0] is the callee.
    lir->insStorei(INS_CONSTVAL(OBJECT_TO_JSVAL(funobj)), invokevp_ins, 0);

    // Calculate |this|.
    LIns* this_ins;
    if (mode == JSOP_NEW) {
        JSClass* clasp = fun->u.n.clasp;
        JS_ASSERT(clasp != &js_SlowArrayClass);
        if (!clasp)
            clasp = &js_ObjectClass;
        JS_ASSERT(((jsuword) clasp & 3) == 0);

        // Abort on |new Function|. js_NewInstance would allocate a regular-
        // sized JSObject, not a Function-sized one. (The Function ctor would
        // deep-bail anyway but let's not go there.)
        if (clasp == &js_FunctionClass)
            RETURN_STOP("new Function");

        if (clasp->getObjectOps)
            RETURN_STOP("new with non-native ops");

        args[0] = INS_CONSTOBJ(funobj);
        args[1] = INS_CONSTPTR(clasp);
        args[2] = cx_ins;
        newobj_ins = lir->insCall(&js_NewInstance_ci, args);
        guard(false, lir->ins_peq0(newobj_ins), OOM_EXIT);
        this_ins = newobj_ins; /* boxing an object is a no-op */
    } else if (JSFUN_BOUND_METHOD_TEST(fun->flags)) {
        /* |funobj| was rooted above already. */
        this_ins = INS_CONSTWORD(OBJECT_TO_JSVAL(OBJ_GET_PARENT(cx, funobj)));
    } else {
        this_ins = get(&vp[1]);

        /*
         * For fast natives, 'null' or primitives are fine as as 'this' value.
         * For slow natives we have to ensure the object is substituted for the
         * appropriate global object or boxed object value. JSOP_NEW allocates its
         * own object so it's guaranteed to have a valid 'this' value.
         */
        if (!(fun->flags & JSFUN_FAST_NATIVE)) {
            if (JSVAL_IS_NULL(vp[1])) {
                JSObject* thisObj = js_ComputeThis(cx, JS_FALSE, vp + 2);
                if (!thisObj)
                    RETURN_ERROR("error in js_ComputeGlobalThis");
                this_ins = INS_CONSTOBJ(thisObj);
            } else if (!JSVAL_IS_OBJECT(vp[1])) {
                RETURN_STOP("slow native(primitive, args)");
            } else {
                if (guardClass(JSVAL_TO_OBJECT(vp[1]), this_ins, &js_WithClass, snapshot(MISMATCH_EXIT)))
                    RETURN_STOP("can't trace slow native invocation on With object");

                this_ins = lir->ins_choose(lir->ins_peq0(stobj_get_parent(this_ins)),
                                           INS_CONSTOBJ(globalObj),
                                           this_ins, avmplus::AvmCore::use_cmov());
            }
        }
        this_ins = box_jsval(vp[1], this_ins);
    }
    lir->insStorei(this_ins, invokevp_ins, 1 * sizeof(jsval));

    // Populate argv.
    for (uintN n = 2; n < 2 + argc; n++) {
        LIns* i = box_jsval(vp[n], get(&vp[n]));
        lir->insStorei(i, invokevp_ins, n * sizeof(jsval));

        // For a very long argument list we might run out of LIR space, so
        // check inside the loop.
        if (outOfMemory())
            RETURN_STOP("out of memory in argument list");
    }

    // Populate extra slots, including the return value slot for a slow native.
    if (2 + argc < vplen) {
        LIns* undef_ins = INS_CONSTWORD(JSVAL_VOID);
        for (uintN n = 2 + argc; n < vplen; n++) {
            lir->insStorei(undef_ins, invokevp_ins, n * sizeof(jsval));

            if (outOfMemory())
                RETURN_STOP("out of memory in extra slots");
        }
    }

    // Set up arguments for the JSNative or JSFastNative.
    uint32 types;
    if (fun->flags & JSFUN_FAST_NATIVE) {
        if (mode == JSOP_NEW)
            RETURN_STOP("untraceable fast native constructor");
        native_rval_ins = invokevp_ins;
        args[0] = invokevp_ins;
        args[1] = lir->insImm(argc);
        args[2] = cx_ins;
        types = ARGSIZE_I << (0*ARGSIZE_SHIFT) |
                ARGSIZE_P << (1*ARGSIZE_SHIFT) |
                ARGSIZE_I << (2*ARGSIZE_SHIFT) |
                ARGSIZE_P << (3*ARGSIZE_SHIFT);
    } else {
        int32_t offset = (vplen - 1) * sizeof(jsval);
        native_rval_ins = lir->ins2(LIR_piadd, invokevp_ins, INS_CONSTWORD(offset));
        args[0] = native_rval_ins;
        args[1] = lir->ins2(LIR_piadd, invokevp_ins, INS_CONSTWORD(2 * sizeof(jsval)));
        args[2] = lir->insImm(argc);
        args[3] = this_ins;
        args[4] = cx_ins;
        types = ARGSIZE_I << (0*ARGSIZE_SHIFT) |
                ARGSIZE_P << (1*ARGSIZE_SHIFT) |
                ARGSIZE_P << (2*ARGSIZE_SHIFT) |
                ARGSIZE_I << (3*ARGSIZE_SHIFT) |
                ARGSIZE_P << (4*ARGSIZE_SHIFT) |
                ARGSIZE_P << (5*ARGSIZE_SHIFT);
    }

    // Generate CallInfo and a JSSpecializedNative structure on the fly.
    // Do not use JSTN_UNBOX_AFTER for mode JSOP_NEW because
    // record_NativeCallComplete unboxes the result specially.

    CallInfo* ci = new (traceAlloc()) CallInfo();
    ci->_address = uintptr_t(fun->u.n.native);
    ci->_cse = ci->_fold = 0;
    ci->_abi = ABI_CDECL;
    ci->_argtypes = types;
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
    lir->insStorei(INS_CONST(vplen), lirbuf->state, offsetof(InterpState, nativeVpLen));
    lir->insStorei(invokevp_ins, lirbuf->state, offsetof(InterpState, nativeVp));

    // argc is the original argc here. It is used to calculate where to place
    // the return value.
    return emitNativeCall(&generatedSpecializedNative, argc, args, true);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::functionCall(uintN argc, JSOp mode)
{
    jsval& fval = stackval(0 - (2 + argc));
    JS_ASSERT(&fval >= StackBase(cx->fp));

    if (!VALUE_IS_FUNCTION(cx, fval))
        RETURN_STOP("callee is not a function");

    jsval& tval = stackval(0 - (1 + argc));

    /*
     * If callee is not constant, it's a shapeless call and we have to guard
     * explicitly that we will get this callee again at runtime.
     */
    if (!get(&fval)->isconstp())
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
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(fval));

    if (FUN_INTERPRETED(fun)) {
        if (mode == JSOP_NEW) {
            LIns* args[] = { get(&fval), INS_CONSTPTR(&js_ObjectClass), cx_ins };
            LIns* tv_ins = lir->insCall(&js_NewInstance_ci, args);
            guard(false, lir->ins_peq0(tv_ins), OOM_EXIT);
            set(&tval, tv_ins);
        }
        return interpretedFunctionCall(fval, fun, argc, mode == JSOP_NEW);
    }

    if (FUN_SLOW_NATIVE(fun)) {
        JSNative native = fun->u.n.native;
        jsval* argv = &tval + 1;
        if (native == js_Array)
            return newArray(JSVAL_TO_OBJECT(fval), argc, argv, &fval);
        if (native == js_String && argc == 1) {
            if (mode == JSOP_NEW)
                return newString(JSVAL_TO_OBJECT(fval), 1, argv, &fval);
            if (!JSVAL_IS_PRIMITIVE(argv[0])) {
                RETURN_IF_XML(argv[0]);
                return call_imacro(call_imacros.String);
            }
            set(&fval, stringify(argv[0]));
            pendingSpecializedNative = IGNORE_NATIVE_CALL_COMPLETE_CALLBACK;
            return RECORD_CONTINUE;
        }
    }

    return callNative(argc, mode);
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEW()
{
    uintN argc = GET_ARGC(cx->fp->regs->pc);
    cx->fp->assertValidStackDepth(argc + 2);
    return InjectStatus(functionCall(argc, JSOP_NEW));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELNAME()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELPROP()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DELELEM()
{
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TYPEOF()
{
    jsval& r = stackval(-1);
    LIns* type;
    if (JSVAL_IS_STRING(r)) {
        type = INS_ATOM(cx->runtime->atomState.typeAtoms[JSTYPE_STRING]);
    } else if (isNumber(r)) {
        type = INS_ATOM(cx->runtime->atomState.typeAtoms[JSTYPE_NUMBER]);
    } else if (VALUE_IS_FUNCTION(cx, r)) {
        type = INS_ATOM(cx->runtime->atomState.typeAtoms[JSTYPE_FUNCTION]);
    } else {
        LIns* args[] = { get(&r), cx_ins };
        if (JSVAL_IS_SPECIAL(r)) {
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
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_VOID()
{
    stack(-1, INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID)));
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
    jsval* vp;
    LIns* v_ins;
    LIns* v_after;
    NameResult nr;

    CHECK_STATUS_A(name(vp, v_ins, nr));
    jsval v = nr.tracked ? *vp : nr.v;
    CHECK_STATUS_A(incHelper(v, v_ins, v_after, incr));
    LIns* v_result = pre ? v_after : v_ins;
    if (nr.tracked) {
        set(vp, v_after);
        stack(0, v_result);
        return ARECORD_CONTINUE;
    }

    if (OBJ_GET_CLASS(cx, nr.obj) != &js_CallClass)
        RETURN_STOP_A("incName on unsupported object class");

    CHECK_STATUS_A(setCallProp(nr.obj, nr.obj_ins, nr.sprop, v_after, v));
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
    jsval& l = stackval(-2);
    if (JSVAL_IS_PRIMITIVE(l))
        RETURN_STOP_A("primitive this for SETPROP");

    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (obj->map->ops->setProperty != js_SetProperty)
        RETURN_STOP_A("non-native JSObjectOps::setProperty");
    return ARECORD_CONTINUE;
}

/* Emit a specialized, inlined copy of js_NativeSet. */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::nativeSet(JSObject* obj, LIns* obj_ins, JSScopeProperty* sprop,
                         jsval v, LIns* v_ins)
{
    JSScope* scope = OBJ_SCOPE(obj);
    uint32 slot = sprop->slot;

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
    JS_ASSERT(SPROP_HAS_STUB_SETTER(sprop) || slot == SPROP_INVALID_SLOT);

    // Box the value to be stored, if necessary.
    LIns* boxed_ins = NULL;
    if (!SPROP_HAS_STUB_SETTER(sprop) || (slot != SPROP_INVALID_SLOT && obj != globalObj))
        boxed_ins = box_jsval(v, v_ins);

    // Call the setter, if any.
    if (!SPROP_HAS_STUB_SETTER(sprop))
        emitNativePropertyOp(scope, sprop, obj_ins, true, boxed_ins);

    // Store the value, if this property has a slot.
    if (slot != SPROP_INVALID_SLOT) {
        JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, scope));
        JS_ASSERT(!(sprop->attrs & JSPROP_SHARED));
        if (obj == globalObj) {
            if (!lazilyImportGlobalSlot(slot))
                RETURN_STOP("lazy import of global slot failed");
            set(&STOBJ_GET_SLOT(obj, slot), v_ins);
        } else {
            LIns* dslots_ins = NULL;
            stobj_set_slot(obj_ins, slot, dslots_ins, boxed_ins);
        }
    }

    return RECORD_CONTINUE;
}

static JSBool FASTCALL
MethodWriteBarrier(JSContext* cx, JSObject* obj, JSScopeProperty* sprop, JSObject* funobj)
{
    JSAutoTempValueRooter tvr(cx, funobj);

    return OBJ_SCOPE(obj)->methodWriteBarrier(cx, sprop, tvr.value());
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, MethodWriteBarrier, CONTEXT, OBJECT, SCOPEPROP, OBJECT,
                     0, 0)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::setProp(jsval &l, JSPropCacheEntry* entry, JSScopeProperty* sprop,
                       jsval &v, LIns*& v_ins)
{
    if (entry == JS_NO_PROP_CACHE_FILL)
        RETURN_STOP("can't trace uncacheable property set");
    JS_ASSERT_IF(PCVCAP_TAG(entry->vcap) >= 1, sprop->attrs & JSPROP_SHARED);
    if (!SPROP_HAS_STUB_SETTER(sprop) && sprop->slot != SPROP_INVALID_SLOT)
        RETURN_STOP("can't trace set of property with setter and slot");
    if (sprop->attrs & JSPROP_SETTER)
        RETURN_STOP("can't trace JavaScript function setter");

    // These two cases are errors and can't be traced.
    if (sprop->attrs & JSPROP_GETTER)
        RETURN_STOP("can't assign to property with script getter but no setter");
    if (sprop->attrs & JSPROP_READONLY)
        RETURN_STOP("can't assign to readonly property");

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(l));
    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);
    JSScope* scope = OBJ_SCOPE(obj);

    JS_ASSERT_IF(entry->vcap == PCVCAP_MAKE(entry->kshape, 0, 0), scope->hasProperty(sprop));

    // Fast path for CallClass. This is about 20% faster than the general case.
    v_ins = get(&v);
    if (OBJ_GET_CLASS(cx, obj) == &js_CallClass)
        return setCallProp(obj, obj_ins, sprop, v_ins, v);

    // Find obj2. If entry->adding(), the TAG bits are all 0.
    JSObject* obj2 = obj;
    for (jsuword i = PCVCAP_TAG(entry->vcap) >> PCVCAP_PROTOBITS; i; i--)
        obj2 = OBJ_GET_PARENT(cx, obj2);
    for (jsuword j = PCVCAP_TAG(entry->vcap) & PCVCAP_PROTOMASK; j; j--)
        obj2 = OBJ_GET_PROTO(cx, obj2);
    scope = OBJ_SCOPE(obj2);
    JS_ASSERT_IF(entry->adding(), obj2 == obj);

    // Guard before anything else.
    LIns* map_ins = map(obj_ins);
    CHECK_STATUS(guardNativePropertyOp(obj, map_ins));
    jsuword pcval;
    CHECK_STATUS(guardPropertyCacheHit(obj_ins, map_ins, obj, obj2, entry, pcval));
    JS_ASSERT(scope->object == obj2);
    JS_ASSERT(scope->hasProperty(sprop));
    JS_ASSERT_IF(obj2 != obj, sprop->attrs & JSPROP_SHARED);

    /*
     * Setting a function-valued property might need to rebrand the object, so
     * we emit a call to the method write barrier. There's no need to guard on
     * this, because functions have distinct trace-type from other values and
     * branded-ness is implied by the shape, which we've already guarded on.
     */
    if (scope->brandedOrHasMethodBarrier() && VALUE_IS_FUNCTION(cx, v) && entry->directHit()) {
        if (obj == globalObj)
            RETURN_STOP("can't trace function-valued property set in branded global scope");

        enterDeepBailCall();
        LIns* args[] = { v_ins, INS_CONSTSPROP(sprop), obj_ins, cx_ins };
        LIns* ok_ins = lir->insCall(&MethodWriteBarrier_ci, args);
        guard(false, lir->ins_eq0(ok_ins), OOM_EXIT);
        leaveDeepBailCall();
    }

    // Add a property to the object if necessary.
    if (entry->adding()) {
        JS_ASSERT(!(sprop->attrs & JSPROP_SHARED));
        if (obj == globalObj)
            RETURN_STOP("adding a property to the global object");

        LIns* args[] = { INS_CONSTSPROP(sprop), obj_ins, cx_ins };
        LIns* ok_ins = lir->insCall(&js_AddProperty_ci, args);
        guard(false, lir->ins_eq0(ok_ins), OOM_EXIT);
    }

    return nativeSet(obj, obj_ins, sprop, v, v_ins);
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::setCallProp(JSObject *callobj, LIns *callobj_ins, JSScopeProperty *sprop,
                           LIns *v_ins, jsval v)
{
    // Set variables in on-trace-stack call objects by updating the tracker.
    JSStackFrame *fp = frameIfInRange(callobj);
    if (fp) {
        if (sprop->setter == SetCallArg) {
            jsint slot = JSVAL_TO_INT(SPROP_USERID(sprop));
            jsval *vp2 = &fp->argv[slot];
            set(vp2, v_ins);
            return RECORD_CONTINUE;
        }
        if (sprop->setter == SetCallVar) {
            jsint slot = JSVAL_TO_INT(SPROP_USERID(sprop));
            jsval *vp2 = &fp->slots[slot];
            set(vp2, v_ins);
            return RECORD_CONTINUE;
        }
        RETURN_STOP("can't trace special CallClass setter");
    }

    // Set variables in off-trace-stack call objects by calling standard builtins.
    const CallInfo* ci = NULL;
    if (sprop->setter == SetCallArg)
        ci = &js_SetCallArg_ci;
    else if (sprop->setter == SetCallVar)
        ci = &js_SetCallVar_ci;
    else
        RETURN_STOP("can't trace special CallClass setter");

    LIns* args[] = {
        box_jsval(v, v_ins),
        INS_CONST(SPROP_USERID(sprop)),
        callobj_ins,
        cx_ins
    };
    LIns* call_ins = lir->insCall(ci, args);
    guard(false, addName(lir->ins_eq0(call_ins), "guard(set upvar)"), STATUS_EXIT);
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_SetPropHit(JSPropCacheEntry* entry, JSScopeProperty* sprop)
{
    jsval& r = stackval(-1);
    jsval& l = stackval(-2);
    LIns* v_ins;
    CHECK_STATUS_A(setProp(l, entry, sprop, r, v_ins));

    jsbytecode* pc = cx->fp->regs->pc;
    switch (*pc) {
      case JSOP_SETPROP:
      case JSOP_SETNAME:
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
    // Take snapshot for js_DeepBail and store it in cx->bailExit.
    VMSideExit* exit = snapshot(DEEP_BAIL_EXIT);
    lir->insStorei(INS_CONSTPTR(exit), cx_ins, offsetof(JSContext, bailExit));

    // Tell nanojit not to discard or defer stack writes before this call.
    GuardRecord* guardRec = createGuardRecord(exit);
    lir->insGuard(LIR_xbarrier, NULL, guardRec);

    // Forget about guarded shapes, since deep bailers can reshape the world.
    forgetGuardedShapes();
    return exit;
}

JS_REQUIRES_STACK void
TraceRecorder::leaveDeepBailCall()
{
    // Keep cx->bailExit null when it's invalid.
    lir->insStorei(INS_NULL(), cx_ins, offsetof(JSContext, bailExit));
}

JS_REQUIRES_STACK void
TraceRecorder::finishGetProp(LIns* obj_ins, LIns* vp_ins, LIns* ok_ins, jsval* outp)
{
    // Store the boxed result (and this-object, if JOF_CALLOP) before the
    // guard. The deep-bail case requires this. If the property get fails,
    // these slots will be ignored anyway.
    LIns* result_ins = lir->insLoad(LIR_ldp, vp_ins, 0);
    set(outp, result_ins, true);
    if (js_CodeSpec[*cx->fp->regs->pc].format & JOF_CALLOP)
        set(outp + 1, obj_ins, true);

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
        *idp = ATOM_TO_JSID((JSAtom*) STRING_TO_JSVAL(name));
        return true;
    }

    JSAtom* atom = js_AtomizeString(cx, name, 0);
    if (!atom)
        return false;
    *namep = ATOM_TO_STRING(atom); /* write back to GC root */
    *idp = ATOM_TO_JSID(atom);
    return true;
}

static JSBool FASTCALL
GetPropertyByName(JSContext* cx, JSObject* obj, JSString** namep, jsval* vp)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

    jsid id;
    if (!RootedStringToId(cx, namep, &id) || !obj->getProperty(cx, id, vp)) {
        js_SetBuiltinError(cx);
        return false;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyByName, CONTEXT, OBJECT, STRINGPTR, JSVALPTR,
                     0, 0)

// Convert the value in a slot to a string and store the resulting string back
// in the slot (typically in order to root it).
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::primitiveToStringInPlace(jsval* vp)
{
    jsval v = *vp;
    JS_ASSERT(JSVAL_IS_PRIMITIVE(v));

    if (!JSVAL_IS_STRING(v)) {
        // v is not a string. Turn it into one. js_ValueToString is safe
        // because v is not an object.
        JSString *str = js_ValueToString(cx, v);
        if (!str)
            RETURN_ERROR("failed to stringify element id");
        v = STRING_TO_JSVAL(str);
        set(vp, stringify(*vp));

        // Write the string back to the stack to save the interpreter some work
        // and to ensure snapshots get the correct type for this slot.
        *vp = v;
    }
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyByName(LIns* obj_ins, jsval* idvalp, jsval* outp)
{
    CHECK_STATUS(primitiveToStringInPlace(idvalp));
    enterDeepBailCall();

    // Call GetPropertyByName. The vp parameter points to stack because this is
    // what the interpreter currently does. obj and id are rooted on the
    // interpreter stack, but the slot at vp is not a root.
    LIns* vp_ins = addName(lir->insAlloc(sizeof(jsval)), "vp");
    LIns* idvalp_ins = addName(addr(idvalp), "idvalp");
    LIns* args[] = {vp_ins, idvalp_ins, obj_ins, cx_ins};
    LIns* ok_ins = lir->insCall(&GetPropertyByName_ci, args);

    // GetPropertyByName can assign to *idvalp, so the tracker has an incorrect
    // entry for that address. Correct it. (If the value in the address is
    // never used again, the usual case, Nanojit will kill this load.)
    tracker.set(idvalp, lir->insLoad(LIR_ldp, idvalp_ins, 0));

    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

static JSBool FASTCALL
GetPropertyByIndex(JSContext* cx, JSObject* obj, int32 index, jsval* vp)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

    JSAutoTempIdRooter idr(cx);
    if (!js_Int32ToId(cx, index, idr.addr()) || !obj->getProperty(cx, idr.id(), vp)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyByIndex, CONTEXT, OBJECT, INT32, JSVALPTR, 0, 0)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyByIndex(LIns* obj_ins, LIns* index_ins, jsval* outp)
{
    index_ins = makeNumberInt32(index_ins);

    // See note in getPropertyByName about vp.
    enterDeepBailCall();
    LIns* vp_ins = addName(lir->insAlloc(sizeof(jsval)), "vp");
    LIns* args[] = {vp_ins, index_ins, obj_ins, cx_ins};
    LIns* ok_ins = lir->insCall(&GetPropertyByIndex_ci, args);
    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

static JSBool FASTCALL
GetPropertyById(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
    js_LeaveTraceIfGlobalObject(cx, obj);
    if (!obj->getProperty(cx, id, vp)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyById,
                     CONTEXT, OBJECT, JSVAL, JSVALPTR,                  0, 0)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyById(LIns* obj_ins, jsval* outp)
{
    // Find the atom.
    JSAtom* atom;
    jsbytecode* pc = cx->fp->regs->pc;
    const JSCodeSpec& cs = js_CodeSpec[*pc];
    if (*pc == JSOP_LENGTH) {
        atom = cx->runtime->atomState.lengthAtom;
    } else if (JOF_TYPE(cs.format) == JOF_ATOM) {
        atom = atoms[GET_INDEX(pc)];
    } else {
        JS_ASSERT(JOF_TYPE(cs.format) == JOF_SLOTATOM);
        atom = atoms[GET_INDEX(pc + SLOTNO_LEN)];
    }

    // Call GetPropertyById. See note in getPropertyByName about vp.
    enterDeepBailCall();
    jsid id = ATOM_TO_JSID(atom);
    LIns* vp_ins = addName(lir->insAlloc(sizeof(jsval)), "vp");
    LIns* args[] = {vp_ins, INS_CONSTWORD(id), obj_ins, cx_ins};
    LIns* ok_ins = lir->insCall(&GetPropertyById_ci, args);
    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

/* Manually inlined, specialized copy of js_NativeGet. */
static JSBool FASTCALL
GetPropertyWithNativeGetter(JSContext* cx, JSObject* obj, JSScopeProperty* sprop, jsval* vp)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

#ifdef DEBUG
    JSProperty* prop;
    JSObject* pobj;
    JS_ASSERT(obj->lookupProperty(cx, sprop->id, &pobj, &prop));
    JS_ASSERT(prop == (JSProperty*) sprop);
    obj->dropProperty(cx, prop);
#endif

    // JSScopeProperty::get contains a special case for With objects. We can
    // elide it here because With objects are, we claim, never on the operand
    // stack while recording.
    JS_ASSERT(STOBJ_GET_CLASS(obj) != &js_WithClass);

    *vp = JSVAL_VOID;
    if (!sprop->getter(cx, obj, SPROP_USERID(sprop), vp)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, GetPropertyWithNativeGetter,
                     CONTEXT, OBJECT, SCOPEPROP, JSVALPTR,              0, 0)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::getPropertyWithNativeGetter(LIns* obj_ins, JSScopeProperty* sprop, jsval* outp)
{
    JS_ASSERT(!(sprop->attrs & JSPROP_GETTER));
    JS_ASSERT(sprop->slot == SPROP_INVALID_SLOT);
    JS_ASSERT(!SPROP_HAS_STUB_GETTER_OR_IS_METHOD(sprop));

    // Call GetPropertyWithNativeGetter. See note in getPropertyByName about vp.
    // FIXME - We should call the getter directly. Using a builtin function for
    // now because it buys some extra asserts. See bug 508310.
    enterDeepBailCall();
    LIns* vp_ins = addName(lir->insAlloc(sizeof(jsval)), "vp");
    LIns* args[] = {vp_ins, INS_CONSTPTR(sprop), obj_ins, cx_ins};
    LIns* ok_ins = lir->insCall(&GetPropertyWithNativeGetter_ci, args);
    finishGetProp(obj_ins, vp_ins, ok_ins, outp);
    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETELEM()
{
    bool call = *cx->fp->regs->pc == JSOP_CALLELEM;

    jsval& idx = stackval(-1);
    jsval& lval = stackval(-2);

    LIns* obj_ins = get(&lval);
    LIns* idx_ins = get(&idx);

    // Special case for array-like access of strings.
    if (JSVAL_IS_STRING(lval) && isInt32(idx)) {
        if (call)
            RETURN_STOP_A("JSOP_CALLELEM on a string");
        int i = asInt32(idx);
        if (size_t(i) >= JSVAL_TO_STRING(lval)->length())
            RETURN_STOP_A("Invalid string index in JSOP_GETELEM");
        idx_ins = makeNumberInt32(idx_ins);
        LIns* args[] = { idx_ins, obj_ins, cx_ins };
        LIns* unitstr_ins = lir->insCall(&js_String_getelem_ci, args);
        guard(false, lir->ins_peq0(unitstr_ins), MISMATCH_EXIT);
        set(&lval, unitstr_ins);
        return ARECORD_CONTINUE;
    }

    if (JSVAL_IS_PRIMITIVE(lval))
        RETURN_STOP_A("JSOP_GETLEM on a primitive");
    RETURN_IF_XML_A(lval);

    JSObject* obj = JSVAL_TO_OBJECT(lval);
    if (obj == globalObj)
        RETURN_STOP_A("JSOP_GETELEM on global");
    LIns* v_ins;

    /* Property access using a string name or something we have to stringify. */
    if (!JSVAL_IS_INT(idx)) {
        if (!JSVAL_IS_PRIMITIVE(idx))
            RETURN_STOP_A("object used as index");

        return InjectStatus(getPropertyByName(obj_ins, &idx, &lval));
    }

    if (STOBJ_GET_CLASS(obj) == &js_ArgumentsClass) {
        unsigned depth;
        JSStackFrame *afp = guardArguments(obj, obj_ins, &depth);
        if (afp) {
            uintN int_idx = JSVAL_TO_INT(idx);
            jsval* vp = &afp->argv[int_idx];
            if (idx_ins->isconstf()) {
                if (int_idx >= 0 && int_idx < afp->argc)
                    v_ins = get(vp);
                else
                    v_ins = INS_VOID();
            } else {
                // If the index is not a constant expression, we generate LIR to load the value from
                // the native stack area. The guard on js_ArgumentClass above ensures the up-to-date
                // value has been written back to the native stack area.
                idx_ins = makeNumberInt32(idx_ins);
                if (int_idx >= 0 && int_idx < afp->argc) {
                    JSTraceType type = getCoercedType(*vp);

                    // Guard that the argument has the same type on trace as during recording.
                    LIns* typemap_ins;
                    if (callDepth == depth) {
                        // In this case, we are in the same frame where the arguments object was created.
                        // The entry type map is not necessarily up-to-date, so we capture a new type map
                        // for this point in the code.
                        unsigned stackSlots = NativeStackSlots(cx, 0 /* callDepth */);
                        JSTraceType* typemap = new (traceAlloc()) JSTraceType[stackSlots];
                        DetermineTypesVisitor detVisitor(*this, typemap);
                        VisitStackSlots(detVisitor, cx, 0);
                        typemap_ins = INS_CONSTPTR(typemap + 2 /* callee, this */);
                    } else {
                        // In this case, we are in a deeper frame from where the arguments object was
                        // created. The type map at the point of the call out from the creation frame
                        // is accurate.
                        // Note: this relies on the assumption that we abort on setting an element of
                        // an arguments object in any deeper frame.
                        LIns* fip_ins = lir->insLoad(LIR_ldp, lirbuf->rp, (callDepth-depth)*sizeof(FrameInfo*));
                        typemap_ins = lir->ins2(LIR_add, fip_ins, INS_CONST(sizeof(FrameInfo) + 2/*callee,this*/ * sizeof(JSTraceType)));
                    }

                    LIns* typep_ins = lir->ins2(LIR_piadd, typemap_ins,
                                                lir->ins_u2p(lir->ins2(LIR_mul,
                                                                       idx_ins,
                                                                       INS_CONST(sizeof(JSTraceType)))));
                    LIns* type_ins = lir->insLoad(LIR_ldcb, typep_ins, 0);
                    guard(true,
                          addName(lir->ins2(LIR_eq, type_ins, lir->insImm(type)),
                                  "guard(type-stable upvar)"),
                          BRANCH_EXIT);

                    // Read the value out of the native stack area.
                    guard(true, lir->ins2(LIR_ult, idx_ins, INS_CONST(afp->argc)),
                          snapshot(BRANCH_EXIT));
                    size_t stackOffset = nativespOffset(&afp->argv[0]);
                    LIns* args_addr_ins = lir->ins2(LIR_piadd, lirbuf->sp, INS_CONSTWORD(stackOffset));
                    LIns* argi_addr_ins = lir->ins2(LIR_piadd,
                                                    args_addr_ins,
                                                    lir->ins_u2p(lir->ins2(LIR_mul,
                                                                           idx_ins,
                                                                           INS_CONST(sizeof(double)))));
                    v_ins = stackLoad(argi_addr_ins, type);
                } else {
                    guard(false, lir->ins2(LIR_ult, idx_ins, INS_CONST(afp->argc)),
                          snapshot(BRANCH_EXIT));
                    v_ins = INS_VOID();
                }
            }
            JS_ASSERT(v_ins);
            set(&lval, v_ins);
            return ARECORD_CONTINUE;
        }
        RETURN_STOP_A("can't reach arguments object's frame");
    }
    if (js_IsDenseArray(obj)) {
        // Fast path for dense arrays accessed with a integer index.
        jsval* vp;
        LIns* addr_ins;

        guardDenseArray(obj, obj_ins, BRANCH_EXIT);
        CHECK_STATUS_A(denseArrayElement(lval, idx, vp, v_ins, addr_ins));
        set(&lval, v_ins);
        if (call)
            set(&idx, obj_ins);
        return ARECORD_CONTINUE;
    }

    return InjectStatus(getPropertyByIndex(obj_ins, idx_ins, &lval));
}

/* Functions used by JSOP_SETELEM */

static JSBool FASTCALL
SetPropertyByName(JSContext* cx, JSObject* obj, JSString** namep, jsval* vp)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

    jsid id;
    if (!RootedStringToId(cx, namep, &id) || !obj->setProperty(cx, id, vp)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, SetPropertyByName, CONTEXT, OBJECT, STRINGPTR, JSVALPTR,
                     0, 0)

static JSBool FASTCALL
InitPropertyByName(JSContext* cx, JSObject* obj, JSString** namep, jsval val)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

    jsid id;
    if (!RootedStringToId(cx, namep, &id) ||
        !obj->defineProperty(cx, id, val, NULL, NULL, JSPROP_ENUMERATE)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, InitPropertyByName, CONTEXT, OBJECT, STRINGPTR, JSVAL,
                     0, 0)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::initOrSetPropertyByName(LIns* obj_ins, jsval* idvalp, jsval* rvalp, bool init)
{
    CHECK_STATUS(primitiveToStringInPlace(idvalp));

    LIns* rval_ins = box_jsval(*rvalp, get(rvalp));

    enterDeepBailCall();

    LIns* ok_ins;
    LIns* idvalp_ins = addName(addr(idvalp), "idvalp");
    if (init) {
        LIns* args[] = {rval_ins, idvalp_ins, obj_ins, cx_ins};
        ok_ins = lir->insCall(&InitPropertyByName_ci, args);
    } else {
        // See note in getPropertyByName about vp.
        LIns* vp_ins = addName(lir->insAlloc(sizeof(jsval)), "vp");
        lir->insStorei(rval_ins, vp_ins, 0);
        LIns* args[] = {vp_ins, idvalp_ins, obj_ins, cx_ins};
        ok_ins = lir->insCall(&SetPropertyByName_ci, args);
    }
    pendingGuardCondition = ok_ins;

    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

static JSBool FASTCALL
SetPropertyByIndex(JSContext* cx, JSObject* obj, int32 index, jsval* vp)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

    JSAutoTempIdRooter idr(cx);
    if (!js_Int32ToId(cx, index, idr.addr()) || !obj->setProperty(cx, idr.id(), vp)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, SetPropertyByIndex, CONTEXT, OBJECT, INT32, JSVALPTR, 0, 0)

static JSBool FASTCALL
InitPropertyByIndex(JSContext* cx, JSObject* obj, int32 index, jsval val)
{
    js_LeaveTraceIfGlobalObject(cx, obj);

    JSAutoTempIdRooter idr(cx);
    if (!js_Int32ToId(cx, index, idr.addr()) ||
        !obj->defineProperty(cx, idr.id(), val, NULL, NULL, JSPROP_ENUMERATE)) {
        js_SetBuiltinError(cx);
        return JS_FALSE;
    }
    return cx->interpState->builtinStatus == 0;
}
JS_DEFINE_CALLINFO_4(static, BOOL_FAIL, InitPropertyByIndex, CONTEXT, OBJECT, INT32, JSVAL, 0, 0)

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::initOrSetPropertyByIndex(LIns* obj_ins, LIns* index_ins, jsval* rvalp, bool init)
{
    index_ins = makeNumberInt32(index_ins);

    LIns* rval_ins = box_jsval(*rvalp, get(rvalp));

    enterDeepBailCall();

    LIns* ok_ins;
    if (init) {
        LIns* args[] = {rval_ins, index_ins, obj_ins, cx_ins};
        ok_ins = lir->insCall(&InitPropertyByIndex_ci, args);
    } else {
        // See note in getPropertyByName about vp.
        LIns* vp_ins = addName(lir->insAlloc(sizeof(jsval)), "vp");
        lir->insStorei(rval_ins, vp_ins, 0);
        LIns* args[] = {vp_ins, index_ins, obj_ins, cx_ins};
        ok_ins = lir->insCall(&SetPropertyByIndex_ci, args);
    }
    pendingGuardCondition = ok_ins;

    leaveDeepBailCall();
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETELEM()
{
    jsval& v = stackval(-1);
    jsval& idx = stackval(-2);
    jsval& lval = stackval(-3);

    if (JSVAL_IS_PRIMITIVE(lval))
        RETURN_STOP_A("left JSOP_SETELEM operand is not an object");
    RETURN_IF_XML_A(lval);

    JSObject* obj = JSVAL_TO_OBJECT(lval);
    LIns* obj_ins = get(&lval);
    LIns* idx_ins = get(&idx);
    LIns* v_ins = get(&v);

    if (JS_InstanceOf(cx, obj, &js_ArgumentsClass, NULL))
        RETURN_STOP_A("can't trace setting elements of the |arguments| object");

    if (!JSVAL_IS_INT(idx)) {
        if (!JSVAL_IS_PRIMITIVE(idx))
            RETURN_STOP_A("non-primitive index");
        CHECK_STATUS_A(initOrSetPropertyByName(obj_ins, &idx, &v,
                                             *cx->fp->regs->pc == JSOP_INITELEM));
    } else if (JSVAL_TO_INT(idx) < 0 || !OBJ_IS_DENSE_ARRAY(cx, obj)) {
        CHECK_STATUS_A(initOrSetPropertyByIndex(obj_ins, idx_ins, &v,
                                              *cx->fp->regs->pc == JSOP_INITELEM));
    } else {
        // Fast path: assigning to element of dense array.

        // Make sure the array is actually dense.
        if (!guardDenseArray(obj, obj_ins, BRANCH_EXIT))
            return ARECORD_STOP;

        // The index was on the stack and is therefore a LIR float. Force it to
        // be an integer.
        idx_ins = makeNumberInt32(idx_ins);

        // Box the value so we can use one builtin instead of having to add one
        // builtin for every storage type. Special case for integers though,
        // since they are so common.
        LIns* res_ins;
        LIns* args[] = { NULL, idx_ins, obj_ins, cx_ins };
        if (isNumber(v)) {
            if (isPromoteInt(v_ins)) {
                args[0] = ::demote(lir, v_ins);
                res_ins = lir->insCall(&js_Array_dense_setelem_int_ci, args);
            } else {
                args[0] = v_ins;
                res_ins = lir->insCall(&js_Array_dense_setelem_double_ci, args);
            }
        } else {
            LIns* args[] = { box_jsval(v, v_ins), idx_ins, obj_ins, cx_ins };
            res_ins = lir->insCall(&js_Array_dense_setelem_ci, args);
        }
        guard(false, lir->ins_eq0(res_ins), MISMATCH_EXIT);
    }

    jsbytecode* pc = cx->fp->regs->pc;
    if (*pc == JSOP_SETELEM && pc[JSOP_SETELEM_LENGTH] != JSOP_POP)
        set(&lval, v_ins);

    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLNAME()
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj) {
        jsval* vp;
        LIns* ins;
        NameResult nr;
        CHECK_STATUS_A(scopeChainProp(obj, vp, ins, nr));
        stack(0, ins);
        stack(1, INS_CONSTOBJ(globalObj));
        return ARECORD_CONTINUE;
    }

    LIns* obj_ins = scopeChain();
    JSObject* obj2;
    jsuword pcval;

    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    if (PCVAL_IS_NULL(pcval) || !PCVAL_IS_OBJECT(pcval))
        RETURN_STOP_A("callee is not an object");

    JS_ASSERT(HAS_FUNCTION_CLASS(PCVAL_TO_OBJECT(pcval)));

    stack(0, INS_CONSTOBJ(PCVAL_TO_OBJECT(pcval)));
    stack(1, obj_ins);
    return ARECORD_CONTINUE;
}

JS_DEFINE_CALLINFO_5(extern, UINT32, GetUpvarArgOnTrace, CONTEXT, UINT32, INT32, UINT32,
                     DOUBLEPTR, 0, 0)
JS_DEFINE_CALLINFO_5(extern, UINT32, GetUpvarVarOnTrace, CONTEXT, UINT32, INT32, UINT32,
                     DOUBLEPTR, 0, 0)
JS_DEFINE_CALLINFO_5(extern, UINT32, GetUpvarStackOnTrace, CONTEXT, UINT32, INT32, UINT32,
                     DOUBLEPTR, 0, 0)

/*
 * Record LIR to get the given upvar. Return the LIR instruction for the upvar
 * value. NULL is returned only on a can't-happen condition with an invalid
 * typemap. The value of the upvar is returned as v.
 */
JS_REQUIRES_STACK LIns*
TraceRecorder::upvar(JSScript* script, JSUpvarArray* uva, uintN index, jsval& v)
{
    /*
     * Try to find the upvar in the current trace's tracker. For &vr to be
     * the address of the jsval found in js_GetUpvar, we must initialize
     * vr directly with the result, so it is a reference to the same location.
     * It does not work to assign the result to v, because v is an already
     * existing reference that points to something else.
     */
    uint32 cookie = uva->vector[index];
    jsval& vr = js_GetUpvar(cx, script->staticLevel, cookie);
    v = vr;

    if (known(&vr))
        return get(&vr);

    /*
     * The upvar is not in the current trace, so get the upvar value exactly as
     * the interpreter does and unbox.
     */
    uint32 level = script->staticLevel - UPVAR_FRAME_SKIP(cookie);
    uint32 cookieSlot = UPVAR_FRAME_SLOT(cookie);
    JSStackFrame* fp = cx->display[level];
    const CallInfo* ci;
    int32 slot;
    if (!fp->fun) {
        ci = &GetUpvarStackOnTrace_ci;
        slot = cookieSlot;
    } else if (cookieSlot < fp->fun->nargs) {
        ci = &GetUpvarArgOnTrace_ci;
        slot = cookieSlot;
    } else if (cookieSlot == CALLEE_UPVAR_SLOT) {
        ci = &GetUpvarArgOnTrace_ci;
        slot = -2;
    } else {
        ci = &GetUpvarVarOnTrace_ci;
        slot = cookieSlot - fp->fun->nargs;
    }

    LIns* outp = lir->insAlloc(sizeof(double));
    LIns* args[] = {
        outp,
        INS_CONST(callDepth),
        INS_CONST(slot),
        INS_CONST(level),
        cx_ins
    };
    LIns* call_ins = lir->insCall(ci, args);
    JSTraceType type = getCoercedType(v);
    guard(true,
          addName(lir->ins2(LIR_eq, call_ins, lir->insImm(type)),
                  "guard(type-stable upvar)"),
          BRANCH_EXIT);
    return stackLoad(outp, type);
}

/*
 * Generate LIR to load a value from the native stack. This method ensures that
 * the correct LIR load operator is used.
 */
LIns* TraceRecorder::stackLoad(LIns* base, uint8 type)
{
    LOpcode loadOp;
    switch (type) {
      case TT_DOUBLE:
        loadOp = LIR_ldq;
        break;
      case TT_OBJECT:
      case TT_STRING:
      case TT_FUNCTION:
      case TT_NULL:
        loadOp = LIR_ldp;
        break;
      case TT_INT32:
      case TT_PSEUDOBOOLEAN:
        loadOp = LIR_ld;
        break;
      case TT_JSVAL:
      default:
        JS_NOT_REACHED("found jsval type in an upvar type map entry");
        return NULL;
    }

    LIns* result = lir->insLoad(loadOp, base, 0);
    if (type == TT_INT32)
        result = lir->ins1(LIR_i2f, result);
    return result;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETUPVAR()
{
    uintN index = GET_UINT16(cx->fp->regs->pc);
    JSScript *script = cx->fp->script;
    JSUpvarArray* uva = script->upvars();
    JS_ASSERT(index < uva->length);

    jsval v;
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
    stack(1, INS_NULL());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETDSLOT()
{
    JSObject* callee = cx->fp->calleeObject();
    LIns* callee_ins = get(&cx->fp->argv[-2]);

    unsigned index = GET_UINT16(cx->fp->regs->pc);
    LIns* dslots_ins = NULL;
    LIns* v_ins = stobj_get_dslot(callee_ins, index, dslots_ins);

    stack(0, unbox_jsval(callee->dslots[index], v_ins, snapshot(BRANCH_EXIT)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLDSLOT()
{
    CHECK_STATUS_A(record_JSOP_GETDSLOT());
    stack(1, INS_NULL());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::guardCallee(jsval& callee)
{
    JS_ASSERT(VALUE_IS_FUNCTION(cx, callee));

    VMSideExit* branchExit = snapshot(BRANCH_EXIT);
    JSObject* callee_obj = JSVAL_TO_OBJECT(callee);
    LIns* callee_ins = get(&callee);

    treeInfo->gcthings.addUnique(callee);
    guard(true,
          lir->ins2(LIR_peq,
                    stobj_get_private(callee_ins),
                    INS_CONSTPTR(callee_obj->getPrivate())),
          branchExit);
    guard(true,
          lir->ins2(LIR_peq,
                    stobj_get_parent(callee_ins),
                    INS_CONSTOBJ(OBJ_GET_PARENT(cx, callee_obj))),
          branchExit);
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
    JS_ASSERT(STOBJ_GET_CLASS(obj) == &js_ArgumentsClass);

    JSStackFrame *afp = frameIfInRange(obj, depthp);
    if (!afp)
        return NULL;

    VMSideExit *exit = snapshot(MISMATCH_EXIT);
    guardClass(obj, obj_ins, &js_ArgumentsClass, exit);

    LIns* args_ins = get(&afp->argsobj);
    LIns* cmp = lir->ins2(LIR_peq, args_ins, obj_ins);
    lir->insGuard(LIR_xf, cmp, createGuardRecord(exit));
    return afp;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::interpretedFunctionCall(jsval& fval, JSFunction* fun, uintN argc, bool constructing)
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
        LIns* rval_ins = constructing ? stack(-1 - argc) : INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID));
        stack(-2 - argc, rval_ins);
        return RECORD_CONTINUE;
    }

    if (JS_GetGlobalForObject(cx, JSVAL_TO_OBJECT(fval)) != globalObj)
        RETURN_STOP("JSOP_CALL or JSOP_NEW crosses global scopes");

    JSStackFrame* fp = cx->fp;

    // TODO: track the copying via the tracker...
    if (argc < fun->nargs &&
        jsuword(fp->regs->sp + (fun->nargs - argc)) > cx->stackPool.current->limit) {
        RETURN_STOP("can't trace calls with too few args requiring argv move");
    }

    // Generate a type map for the outgoing frame and stash it in the LIR
    unsigned stackSlots = NativeStackSlots(cx, 0 /* callDepth */);
    FrameInfo* fi = (FrameInfo*)
        tempAlloc().alloc(sizeof(FrameInfo) + stackSlots * sizeof(JSTraceType));
    JSTraceType* typemap = (JSTraceType*)(fi + 1);

    DetermineTypesVisitor detVisitor(*this, typemap);
    VisitStackSlots(detVisitor, cx, 0);

    JS_ASSERT(argc < FrameInfo::CONSTRUCTING_FLAG);

    treeInfo->gcthings.addUnique(fval);
    fi->block = fp->blockChain;
    if (fp->blockChain)
        treeInfo->gcthings.addUnique(OBJECT_TO_JSVAL(fp->blockChain));
    fi->pc = fp->regs->pc;
    fi->imacpc = fp->imacpc;
    fi->spdist = fp->regs->sp - fp->slots;
    fi->set_argc(argc, constructing);
    fi->callerHeight = stackSlots - (2 + argc);
    fi->callerArgc = fp->argc;

    if (callDepth >= treeInfo->maxCallDepth)
        treeInfo->maxCallDepth = callDepth + 1;

    fi = traceMonitor->frameCache->memoize(fi);
    if (!fi)
        RETURN_STOP("out of memory");
    lir->insStorei(INS_CONSTPTR(fi), lirbuf->rp, callDepth * sizeof(FrameInfo*));

#if defined JS_JIT_SPEW
    debug_only_printf(LC_TMTracer, "iFC frameinfo=%p, stack=%d, map=", (void*)fi,
                      fi->callerHeight);
    for (unsigned i = 0; i < fi->callerHeight; i++)
        debug_only_printf(LC_TMTracer, "%c", typeChar[fi->get_typemap()[i]]);
    debug_only_print0(LC_TMTracer, "\n");
#endif

    atoms = fun->u.i.script->atomMap.vector;
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALL()
{
    uintN argc = GET_ARGC(cx->fp->regs->pc);
    cx->fp->assertValidStackDepth(argc + 2);
    return InjectStatus(functionCall(argc,
                                     (cx->fp->imacpc && *cx->fp->imacpc == JSOP_APPLY)
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
    JSStackFrame* fp = cx->fp;
    jsbytecode *pc = fp->regs->pc;
    uintN argc = GET_ARGC(pc);
    cx->fp->assertValidStackDepth(argc + 2);

    jsval* vp = fp->regs->sp - (argc + 2);
    jsuint length = 0;
    JSObject* aobj = NULL;
    LIns* aobj_ins = NULL;

    JS_ASSERT(!fp->imacpc);

    if (!VALUE_IS_FUNCTION(cx, vp[0]))
        return record_JSOP_CALL();
    RETURN_IF_XML_A(vp[0]);

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
    if (argc > 0 && !JSVAL_IS_OBJECT(vp[2]))
        return record_JSOP_CALL();

    /*
     * Guard on the identity of this, which is the function we are applying.
     */
    if (!VALUE_IS_FUNCTION(cx, vp[1]))
        RETURN_STOP_A("callee is not a function");
    CHECK_STATUS_A(guardCallee(vp[1]));

    if (apply && argc >= 2) {
        if (argc != 2)
            RETURN_STOP_A("apply with excess arguments");
        if (JSVAL_IS_PRIMITIVE(vp[3]))
            RETURN_STOP_A("arguments parameter of apply is primitive");
        aobj = JSVAL_TO_OBJECT(vp[3]);
        aobj_ins = get(&vp[3]);

        /*
         * We trace dense arrays and arguments objects. The code we generate
         * for apply uses imacros to handle a specific number of arguments.
         */
        if (OBJ_IS_DENSE_ARRAY(cx, aobj)) {
            guardDenseArray(aobj, aobj_ins);
            length = jsuint(aobj->fslots[JSSLOT_ARRAY_LENGTH]);
            guard(true,
                  lir->ins2i(LIR_eq,
                             p2i(stobj_get_fslot(aobj_ins, JSSLOT_ARRAY_LENGTH)),
                             length),
                  BRANCH_EXIT);
        } else if (OBJ_GET_CLASS(cx, aobj) == &js_ArgumentsClass) {
            unsigned depth;
            JSStackFrame *afp = guardArguments(aobj, aobj_ins, &depth);
            if (!afp)
                RETURN_STOP_A("can't reach arguments object's frame");
            length = afp->argc;
        } else {
            RETURN_STOP_A("arguments parameter of apply is not a dense array or argments object");
        }

        if (length >= JS_ARRAY_LENGTH(apply_imacro_table))
            RETURN_STOP_A("too many arguments to apply");

        return InjectStatus(call_imacro(apply_imacro_table[length]));
    }

    if (argc >= JS_ARRAY_LENGTH(call_imacro_table))
        RETURN_STOP_A("too many arguments to call");

    return InjectStatus(call_imacro(call_imacro_table[argc]));
}

static JSBool FASTCALL
CatchStopIteration_tn(JSContext* cx, JSBool ok, jsval* vp)
{
    if (!ok && cx->throwing && js_ValueIsStopIteration(cx->exception)) {
        cx->throwing = JS_FALSE;
        cx->exception = JSVAL_VOID;
        *vp = JSVAL_HOLE;
        return JS_TRUE;
    }
    return ok;
}

JS_DEFINE_TRCINFO_1(CatchStopIteration_tn,
    (3, (static, BOOL, CatchStopIteration_tn, CONTEXT, BOOL, JSVALPTR, 0, 0)))

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_NativeCallComplete()
{
    if (pendingSpecializedNative == IGNORE_NATIVE_CALL_COMPLETE_CALLBACK)
        return ARECORD_CONTINUE;

    jsbytecode* pc = cx->fp->regs->pc;

    JS_ASSERT(pendingSpecializedNative);
    JS_ASSERT(*pc == JSOP_CALL || *pc == JSOP_APPLY || *pc == JSOP_NEW || *pc == JSOP_SETPROP);

    jsval& v = stackval(-1);
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
     * boxed if the type guard generated by unbox_jsval() fails.
     */

    if (JSTN_ERRTYPE(pendingSpecializedNative) == FAIL_STATUS) {
        /* Keep cx->bailExit null when it's invalid. */
        lir->insStorei(INS_NULL(), cx_ins, (int) offsetof(JSContext, bailExit));

        LIns* status = lir->insLoad(LIR_ld, lirbuf->state, (int) offsetof(InterpState, builtinStatus));
        if (pendingSpecializedNative == &generatedSpecializedNative) {
            LIns* ok_ins = v_ins;

            /*
             * Custom implementations of Iterator.next() throw a StopIteration exception.
             * Catch and clear it and set the return value to JSVAL_HOLE in this case.
             */
            if (uintptr_t(pc - nextiter_imacros.custom_iter_next) <
                sizeof(nextiter_imacros.custom_iter_next)) {
                LIns* args[] = { native_rval_ins, ok_ins, cx_ins }; /* reverse order */
                ok_ins = lir->insCall(&CatchStopIteration_tn_ci, args);
            }

            /*
             * If we run a generic traceable native, the return value is in the argument
             * vector for native function calls. The actual return value of the native is a JSBool
             * indicating the error status.
             */
            v_ins = lir->insLoad(LIR_ldp, native_rval_ins, 0);
            if (*pc == JSOP_NEW) {
                LIns* x = lir->ins_peq0(lir->ins2(LIR_piand, v_ins, INS_CONSTWORD(JSVAL_TAGMASK)));
                x = lir->ins_choose(x, v_ins, INS_CONSTWORD(0), avmplus::AvmCore::use_cmov());
                v_ins = lir->ins_choose(lir->ins_peq0(x), newobj_ins, x, avmplus::AvmCore::use_cmov());
            }
            set(&v, v_ins);

            propagateFailureToBuiltinStatus(ok_ins, status);
        }
        guard(true, lir->ins_eq0(status), STATUS_EXIT);
    }

    if (pendingSpecializedNative->flags & JSTN_UNBOX_AFTER) {
        /*
         * If we side exit on the unboxing code due to a type change, make sure that the boxed
         * value is actually currently associated with that location, and that we are talking
         * about the top of the stack here, which is where we expected boxed values.
         */
        JS_ASSERT(&v == &cx->fp->regs->sp[-1] && get(&v) == v_ins);
        set(&v, unbox_jsval(v, v_ins, snapshot(BRANCH_EXIT)));
    } else if (JSTN_ERRTYPE(pendingSpecializedNative) == FAIL_NEG) {
        /* Already added i2f in functionCall. */
        JS_ASSERT(JSVAL_IS_NUMBER(v));
    } else {
        /* Convert the result to double if the builtin returns int32. */
        if (JSVAL_IS_NUMBER(v) &&
            (pendingSpecializedNative->builtin->_argtypes & ARGSIZE_MASK_ANY) == ARGSIZE_I) {
            set(&v, lir->ins1(LIR_i2f, v_ins));
        }
    }

    // We'll null pendingSpecializedNative in monitorRecording, on the next op
    // cycle.  There must be a next op since the stack is non-empty.
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::name(jsval*& vp, LIns*& ins, NameResult& nr)
{
    JSObject* obj = cx->fp->scopeChain;
    if (obj != globalObj)
        return scopeChainProp(obj, vp, ins, nr);

    /* Can't use prop here, because we don't want unboxing from global slots. */
    LIns* obj_ins = scopeChain();
    uint32 slot;

    JSObject* obj2;
    jsuword pcval;

    /*
     * Property cache ensures that we are dealing with an existing property,
     * and guards the shape for us.
     */
    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    /* Abort if property doesn't exist (interpreter will report an error.) */
    if (PCVAL_IS_NULL(pcval))
        RETURN_STOP_A("named property not found");

    /* Insist on obj being the directly addressed object. */
    if (obj2 != obj)
        RETURN_STOP_A("name() hit prototype chain");

    /* Don't trace getter or setter calls, our caller wants a direct slot. */
    if (PCVAL_IS_SPROP(pcval)) {
        JSScopeProperty* sprop = PCVAL_TO_SPROP(pcval);
        if (!isValidSlot(OBJ_SCOPE(obj), sprop))
            RETURN_STOP_A("name() not accessing a valid slot");
        slot = sprop->slot;
    } else {
        if (!PCVAL_IS_SLOT(pcval))
            RETURN_STOP_A("PCE is not a slot");
        slot = PCVAL_TO_SLOT(pcval);
    }

    if (!lazilyImportGlobalSlot(slot))
        RETURN_STOP_A("lazy import of global slot failed");

    vp = &STOBJ_GET_SLOT(obj, slot);
    ins = get(vp);
    nr.tracked = true;
    return ARECORD_CONTINUE;
}

static JSObject* FASTCALL
MethodReadBarrier(JSContext* cx, JSObject* obj, JSScopeProperty* sprop, JSObject* funobj)
{
    JSAutoTempValueRooter tvr(cx, funobj);

    if (!OBJ_SCOPE(obj)->methodReadBarrier(cx, sprop, tvr.addr()))
        return NULL;
    JS_ASSERT(VALUE_IS_FUNCTION(cx, tvr.value()));
    return JSVAL_TO_OBJECT(tvr.value());
}
JS_DEFINE_CALLINFO_4(static, OBJECT_FAIL, MethodReadBarrier, CONTEXT, OBJECT, SCOPEPROP, OBJECT,
                     0, 0)

/*
 * Get a property. The current opcode has JOF_ATOM.
 *
 * There are two modes. The caller must pass nonnull pointers for either outp
 * or both slotp and v_insp. In the latter case, we require a plain old
 * property with a slot; if the property turns out to be anything else, abort
 * tracing (rather than emit a call to a native getter or GetAnyProperty).
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::prop(JSObject* obj, LIns* obj_ins, uint32 *slotp, LIns** v_insp, jsval *outp)
{
    JS_ASSERT((slotp && v_insp && !outp) || (!slotp && !v_insp && outp));

    /*
     * Can't specialize to assert obj != global, must guard to avoid aliasing
     * stale homes of stacked global variables.
     */
    CHECK_STATUS_A(guardNotGlobalObject(obj, obj_ins));

    /*
     * Property cache ensures that we are dealing with an existing property,
     * and guards the shape for us.
     */
    JSObject* obj2;
    jsuword pcval;
    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    /* Check for non-existent property reference, which results in undefined. */
    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    if (PCVAL_IS_NULL(pcval)) {
        if (slotp)
            RETURN_STOP_A("property not found");

        /*
         * We could specialize to guard on just JSClass.getProperty, but a mere
         * class guard is simpler and slightly faster.
         */
        if (OBJ_GET_CLASS(cx, obj)->getProperty != JS_PropertyStub) {
            RETURN_STOP_A("can't trace through access to undefined property if "
                                 "JSClass.getProperty hook isn't stubbed");
        }
        guardClass(obj, obj_ins, OBJ_GET_CLASS(cx, obj), snapshot(MISMATCH_EXIT));

        /*
         * This trace will be valid as long as neither the object nor any object
         * on its prototype chain changes shape.
         *
         * FIXME: This loop can become a single shape guard once bug 497789 has
         * been fixed.
         */
        VMSideExit* exit = snapshot(BRANCH_EXIT);
        do {
            LIns* map_ins = map(obj_ins);
            LIns* ops_ins;
            if (map_is_native(obj->map, map_ins, ops_ins)) {
                CHECK_STATUS_A(InjectStatus(guardShape(obj_ins, obj, OBJ_SHAPE(obj), "guard(shape)",
                                                       map_ins, exit)));
            } else if (!guardDenseArray(obj, obj_ins, exit)) {
                RETURN_STOP_A("non-native object involved in undefined property access");
            }
        } while (guardHasPrototype(obj, obj_ins, &obj, &obj_ins, exit));

        set(outp, INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID)), true);
        return ARECORD_CONTINUE;
    }

    uint32 setflags = (cs.format & (JOF_INCDEC | JOF_FOR));
    JS_ASSERT(!(cs.format & JOF_SET));

    JSScopeProperty* sprop;
    uint32 slot;
    bool isMethod;

    if (PCVAL_IS_SPROP(pcval)) {
        sprop = PCVAL_TO_SPROP(pcval);
        JS_ASSERT(OBJ_SCOPE(obj2)->hasProperty(sprop));

        if (setflags && !SPROP_HAS_STUB_SETTER(sprop))
            RETURN_STOP_A("non-stub setter");
        if (setflags && (sprop->attrs & JSPROP_READONLY))
            RETURN_STOP_A("writing to a readonly property");
        if (!SPROP_HAS_STUB_GETTER_OR_IS_METHOD(sprop)) {
            if (slotp)
                RETURN_STOP_A("can't trace non-stub getter for this opcode");
            if (sprop->attrs & JSPROP_GETTER)
                RETURN_STOP_A("script getter");
            if (sprop->slot == SPROP_INVALID_SLOT)
                return InjectStatus(getPropertyWithNativeGetter(obj_ins, sprop, outp));
            return InjectStatus(getPropertyById(obj_ins, outp));
        }
        if (!SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj2)))
            RETURN_STOP_A("no valid slot");
        slot = sprop->slot;
        isMethod = sprop->isMethod();
        JS_ASSERT_IF(isMethod, OBJ_SCOPE(obj2)->hasMethodBarrier());
    } else {
        if (!PCVAL_IS_SLOT(pcval))
            RETURN_STOP_A("PCE is not a slot");
        slot = PCVAL_TO_SLOT(pcval);
        sprop = NULL;
        isMethod = false;
    }

    /* We have a slot. Check whether it is direct or in a prototype. */
    if (obj2 != obj) {
        if (setflags)
            RETURN_STOP_A("JOF_INCDEC|JOF_FOR opcode hit prototype chain");

        /*
         * We're getting a proto-property. Walk up the prototype chain emitting
         * proto slot loads, updating obj as we go, leaving obj set to obj2 with
         * obj_ins the last proto-load.
         */
        do {
            obj_ins = stobj_get_proto(obj_ins);
            obj = STOBJ_GET_PROTO(obj);
        } while (obj != obj2);
    }

    LIns* dslots_ins = NULL;
    LIns* v_ins = unbox_jsval(STOBJ_GET_SLOT(obj, slot),
                              stobj_get_slot(obj_ins, slot, dslots_ins),
                              snapshot(BRANCH_EXIT));

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
    if (isMethod && !cx->fp->imacpc) {
        enterDeepBailCall();
        LIns* args[] = { v_ins, INS_CONSTSPROP(sprop), obj_ins, cx_ins };
        v_ins = lir->insCall(&MethodReadBarrier_ci, args);
        leaveDeepBailCall();
    }

    if (slotp) {
        *slotp = slot;
        *v_insp = v_ins;
    }
    if (outp)
        set(outp, v_ins, true);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK RecordingStatus
TraceRecorder::denseArrayElement(jsval& oval, jsval& ival, jsval*& vp, LIns*& v_ins,
                                 LIns*& addr_ins)
{
    JS_ASSERT(JSVAL_IS_OBJECT(oval) && JSVAL_IS_INT(ival));

    JSObject* obj = JSVAL_TO_OBJECT(oval);
    LIns* obj_ins = get(&oval);
    jsint idx = JSVAL_TO_INT(ival);
    LIns* idx_ins = makeNumberInt32(get(&ival));
    LIns* pidx_ins = lir->ins_u2p(idx_ins);

    VMSideExit* exit = snapshot(BRANCH_EXIT);

    /* check that the index is within bounds */
    LIns* dslots_ins = lir->insLoad(LIR_ldp, obj_ins, offsetof(JSObject, dslots));
    jsuint capacity = js_DenseArrayCapacity(obj);
    bool within = (jsuint(idx) < jsuint(obj->fslots[JSSLOT_ARRAY_LENGTH]) && jsuint(idx) < capacity);
    if (!within) {
        /* If idx < 0, stay on trace (and read value as undefined, since this is a dense array). */
        LIns* br1 = NULL;
        if (MAX_DSLOTS_LENGTH > MAX_DSLOTS_LENGTH32 && !idx_ins->isconst()) {
            /* Only 64-bit machines support large enough arrays for this. */
            JS_ASSERT(sizeof(jsval) == 8);
            br1 = lir->insBranch(LIR_jt,
                                 lir->ins2i(LIR_lt, idx_ins, 0),
                                 NULL);
        }

        /* If not idx < length, stay on trace (and read value as undefined). */
        LIns* br2 = lir->insBranch(LIR_jf,
                                   lir->ins2(LIR_pult,
                                             pidx_ins,
                                             stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH)),
                                   NULL);

        /* If dslots is NULL, stay on trace (and read value as undefined). */
        LIns* br3 = lir->insBranch(LIR_jt, lir->ins_peq0(dslots_ins), NULL);

        /* If not idx < capacity, stay on trace (and read value as undefined). */
        LIns* br4 = lir->insBranch(LIR_jf,
                                   lir->ins2(LIR_pult,
                                             pidx_ins,
                                             lir->insLoad(LIR_ldp,
                                                          dslots_ins,
                                                          -(int)sizeof(jsval))),
                                   NULL);
        lir->insGuard(LIR_x, NULL, createGuardRecord(exit));
        LIns* label = lir->ins0(LIR_label);
        if (br1)
            br1->setTarget(label);
        br2->setTarget(label);
        br3->setTarget(label);
        br4->setTarget(label);

        CHECK_STATUS(guardPrototypeHasNoIndexedProperties(obj, obj_ins, MISMATCH_EXIT));

        // Return undefined and indicate that we didn't actually read this (addr_ins).
        v_ins = lir->insImm(JSVAL_TO_SPECIAL(JSVAL_VOID));
        addr_ins = NULL;
        return RECORD_CONTINUE;
    }

    /* Guard against negative index */
    if (MAX_DSLOTS_LENGTH > MAX_DSLOTS_LENGTH32 && !idx_ins->isconst()) {
        /* Only 64-bit machines support large enough arrays for this. */
        JS_ASSERT(sizeof(jsval) == 8);
        guard(false,
              lir->ins2i(LIR_lt, idx_ins, 0),
              exit);
    }

    /* Guard array length */
    guard(true,
          lir->ins2(LIR_pult, pidx_ins, stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH)),
          exit);

    /* dslots must not be NULL */
    guard(false,
          lir->ins_peq0(dslots_ins),
          exit);

    /* Guard array capacity */
    guard(true,
          lir->ins2(LIR_pult,
                    pidx_ins,
                    lir->insLoad(LIR_ldp, dslots_ins, 0 - (int)sizeof(jsval))),
          exit);

    /* Load the value and guard on its type to unbox it. */
    vp = &obj->dslots[jsuint(idx)];
    addr_ins = lir->ins2(LIR_piadd, dslots_ins,
                         lir->ins2i(LIR_pilsh, pidx_ins, (sizeof(jsval) == 4) ? 2 : 3));
    v_ins = unbox_jsval(*vp, lir->insLoad(LIR_ldp, addr_ins, 0), exit);

    if (JSVAL_IS_SPECIAL(*vp)) {
        /*
         * If we read a hole from the array, convert it to undefined and guard
         * that there are no indexed properties along the prototype chain.
         */
        LIns* br = lir->insBranch(LIR_jf,
                                  lir->ins2i(LIR_eq, v_ins, JSVAL_TO_SPECIAL(JSVAL_HOLE)),
                                  NULL);
        CHECK_STATUS(guardPrototypeHasNoIndexedProperties(obj, obj_ins, MISMATCH_EXIT));
        br->setTarget(lir->ins0(LIR_label));

        /* Don't let the hole value escape. Turn it into an undefined. */
        v_ins = lir->ins2i(LIR_and, v_ins, ~(JSVAL_HOLE_FLAG >> JSVAL_TAGBITS));
    }
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::getProp(JSObject* obj, LIns* obj_ins)
{
    const JSCodeSpec& cs = js_CodeSpec[*cx->fp->regs->pc];
    JS_ASSERT(cs.ndefs == 1);
    return prop(obj, obj_ins, NULL, NULL, &stackval(-cs.nuses));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::getProp(jsval& v)
{
    if (JSVAL_IS_PRIMITIVE(v))
        RETURN_STOP_A("primitive lhs");

    return getProp(JSVAL_TO_OBJECT(v), get(&v));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NAME()
{
    jsval* vp;
    LIns* v_ins;
    NameResult nr;
    CHECK_STATUS_A(name(vp, v_ins, nr));
    stack(0, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DOUBLE()
{
    jsval v = jsval(atoms[GET_INDEX(cx->fp->regs->pc)]);
    stack(0, lir->insImmf(*JSVAL_TO_DOUBLE(v)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_STRING()
{
    JSAtom* atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    JS_ASSERT(ATOM_IS_STRING(atom));
    stack(0, INS_ATOM(atom));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ZERO()
{
    stack(0, lir->insImmf(0));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ONE()
{
    stack(0, lir->insImmf(1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NULL()
{
    stack(0, INS_NULL());
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
    stack(0, lir->insImm(0));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_TRUE()
{
    stack(0, lir->insImm(1));
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
    JSStackFrame* fp = cx->fp;
    JSScript* script = fp->script;
    unsigned index = atoms - script->atomMap.vector + GET_INDEX(fp->regs->pc);

    JSObject* obj;
    obj = script->getObject(index);
    stack(0, INS_CONSTOBJ(obj));
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
    stack(0, arg(GET_ARGNO(cx->fp->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETARG()
{
    arg(GET_ARGNO(cx->fp->regs->pc), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETLOCAL()
{
    stack(0, var(GET_SLOTNO(cx->fp->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETLOCAL()
{
    var(GET_SLOTNO(cx->fp->regs->pc), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_UINT16()
{
    stack(0, lir->insImmf(GET_UINT16(cx->fp->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEWINIT()
{
    JSProtoKey key = JSProtoKey(GET_INT8(cx->fp->regs->pc));
    LIns* proto_ins;
    CHECK_STATUS_A(getClassPrototype(key, proto_ins));

    LIns* args[] = { proto_ins, cx_ins };
    const CallInfo *ci = (key == JSProto_Array) ? &js_NewEmptyArray_ci : &js_Object_tn_ci;
    LIns* v_ins = lir->insCall(ci, args);
    guard(false, lir->ins_peq0(v_ins), OOM_EXIT);
    stack(0, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENDINIT()
{
#ifdef DEBUG
    jsval& v = stackval(-1);
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
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
    return record_JSOP_SETELEM();
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
    return InjectStatus(inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCLOCAL()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->fp->regs->pc)), 1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECARG()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECLOCAL()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->fp->regs->pc)), -1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGINC()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->fp->regs->pc)), 1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LOCALINC()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->fp->regs->pc)), 1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGDEC()
{
    return InjectStatus(inc(argval(GET_ARGNO(cx->fp->regs->pc)), -1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LOCALDEC()
{
    return InjectStatus(inc(varval(GET_SLOTNO(cx->fp->regs->pc)), -1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IMACOP()
{
    JS_ASSERT(cx->fp->imacpc);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ITER()
{
    jsval& v = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(v))
        RETURN_STOP_A("for-in on a primitive value");
    RETURN_IF_XML_A(v);

    jsuint flags = cx->fp->regs->pc[1];

    if (hasIteratorMethod(JSVAL_TO_OBJECT(v))) {
        if (flags == JSITER_ENUMERATE)
            return InjectStatus(call_imacro(iter_imacros.for_in));
        if (flags == (JSITER_ENUMERATE | JSITER_FOREACH))
            return InjectStatus(call_imacro(iter_imacros.for_each));
    } else {
        if (flags == JSITER_ENUMERATE)
            return InjectStatus(call_imacro(iter_imacros.for_in_native));
        if (flags == (JSITER_ENUMERATE | JSITER_FOREACH))
            return InjectStatus(call_imacro(iter_imacros.for_each_native));
    }
    RETURN_STOP_A("unimplemented JSITER_* flags");
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_NEXTITER()
{
    jsval& iterobj_val = stackval(-2);
    if (JSVAL_IS_PRIMITIVE(iterobj_val))
        RETURN_STOP_A("for-in on a primitive value");
    RETURN_IF_XML_A(iterobj_val);
    JSObject* iterobj = JSVAL_TO_OBJECT(iterobj_val);
    JSClass* clasp = STOBJ_GET_CLASS(iterobj);
    LIns* iterobj_ins = get(&iterobj_val);
    guardClass(iterobj, iterobj_ins, clasp, snapshot(BRANCH_EXIT));
    if (clasp == &js_IteratorClass || clasp == &js_GeneratorClass)
        return InjectStatus(call_imacro(nextiter_imacros.native_iter_next));
    return InjectStatus(call_imacro(nextiter_imacros.custom_iter_next));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ENDITER()
{
    LIns* args[] = { stack(-2), cx_ins };
    LIns* ok_ins = lir->insCall(&js_CloseIterator_ci, args);
    guard(false, lir->ins_eq0(ok_ins), MISMATCH_EXIT);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORNAME()
{
    jsval* vp;
    LIns* x_ins;
    NameResult nr;
    CHECK_STATUS_A(name(vp, x_ins, nr));
    if (!nr.tracked)
        RETURN_STOP_A("forname on non-tracked value not supported");
    set(vp, stack(-1));
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
    return record_JSOP_DUP();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORARG()
{
    return record_JSOP_SETARG();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_FORLOCAL()
{
    return record_JSOP_SETLOCAL();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_POPN()
{
    return ARECORD_CONTINUE;
}

/*
 * Generate LIR to reach |obj2| from |obj| by traversing the scope chain. The generated code
 * also ensures that any call objects found have not changed shape.
 *
 *      obj               starting object
 *      obj_ins           LIR instruction representing obj
 *      obj2              end object for traversal
 *      obj2_ins   [out]  LIR instruction representing obj2
 */
JS_REQUIRES_STACK RecordingStatus
TraceRecorder::traverseScopeChain(JSObject *obj, LIns *obj_ins, JSObject *obj2, LIns *&obj2_ins)
{
    VMSideExit* exit = NULL;
    for (;;) {
        if (obj != globalObj) {
            if (!js_IsCacheableNonGlobalScope(obj))
                RETURN_STOP("scope chain lookup crosses non-cacheable object");

            // We must guard on the shape of all call objects for heavyweight functions
            // that we traverse on the scope chain: if the shape changes, a variable with
            // the same name may have been inserted in the scope chain.
            if (STOBJ_GET_CLASS(obj) == &js_CallClass &&
                JSFUN_HEAVYWEIGHT_TEST(js_GetCallObjectFunction(obj)->flags)) {
                LIns* map_ins = map(obj_ins);
                LIns* shape_ins = addName(lir->insLoad(LIR_ld, map_ins, offsetof(JSScope, shape)),
                                          "obj_shape");
                if (!exit)
                    exit = snapshot(BRANCH_EXIT);
                guard(true,
                      addName(lir->ins2i(LIR_eq, shape_ins, OBJ_SHAPE(obj)), "guard_shape"),
                      exit);
            }
        }

        if (obj == obj2)
            break;

        obj = STOBJ_GET_PARENT(obj);
        if (!obj)
            RETURN_STOP("target object not reached on scope chain");
        obj_ins = stobj_get_parent(obj_ins);
    }

    obj2_ins = obj_ins;
    return RECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_BINDNAME()
{
    JSStackFrame *fp = cx->fp;
    JSObject *obj;

    if (!fp->fun) {
        obj = fp->scopeChain;

        // In global code, fp->scopeChain can only contain blocks whose values
        // are still on the stack.  We never use BINDNAME to refer to these.
        while (OBJ_GET_CLASS(cx, obj) == &js_BlockClass) {
            // The block's values are still on the stack.
            JS_ASSERT(obj->getPrivate() == fp);
            obj = OBJ_GET_PARENT(cx, obj);
            // Blocks always have parents.
            JS_ASSERT(obj);
        }

        if (obj != globalObj)
            RETURN_STOP_A("BINDNAME in global code resolved to non-global object");

        /*
         * The trace is specialized to this global object. Furthermore, we know it
         * is the sole 'global' object on the scope chain: we set globalObj to the
         * scope chain element with no parent, and we reached it starting from the
         * function closure or the current scopeChain, so there is nothing inner to
         * it. Therefore this must be the right base object.
         */
        stack(0, INS_CONSTOBJ(obj));
        return ARECORD_CONTINUE;
    }

    // We can't trace BINDNAME in functions that contain direct calls to eval,
    // as they might add bindings which previously-traced references would have
    // to see.
    if (JSFUN_HEAVYWEIGHT_TEST(fp->fun->flags))
        RETURN_STOP_A("BINDNAME in heavyweight function.");

    // We don't have the scope chain on trace, so instead we get a start object
    // that is on the scope chain and doesn't skip the target object (the one
    // that contains the property).
    jsval *callee = &cx->fp->argv[-2];
    obj = STOBJ_GET_PARENT(JSVAL_TO_OBJECT(*callee));
    if (obj == globalObj) {
        stack(0, INS_CONSTOBJ(obj));
        return ARECORD_CONTINUE;
    }
    LIns *obj_ins = stobj_get_parent(get(callee));

    // Find the target object.
    JSAtom *atom = atoms[GET_INDEX(cx->fp->regs->pc)];
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj2 = js_FindIdentifierBase(cx, fp->scopeChain, id);
    if (obj2 != globalObj && STOBJ_GET_CLASS(obj2) != &js_CallClass)
        RETURN_STOP_A("BINDNAME on non-global, non-call object");

    // Generate LIR to get to the target object from the start object.
    LIns *obj2_ins;
    CHECK_STATUS_A(traverseScopeChain(obj, obj_ins, obj2, obj2_ins));

    // If |obj2| is the global object, we can refer to it directly instead of walking up
    // the scope chain. There may still be guards on intervening call objects.
    stack(0, obj2 == globalObj ? INS_CONSTOBJ(obj2) : obj2_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETNAME()
{
    jsval& l = stackval(-2);
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(l));

    /*
     * Trace only cases that are global code, in lightweight functions
     * scoped by the global object only, or in call objects.
     */
    JSObject* obj = JSVAL_TO_OBJECT(l);
    if (OBJ_GET_CLASS(cx, obj) == &js_CallClass)
        return ARECORD_CONTINUE;
    if (obj != cx->fp->scopeChain || obj != globalObj)
        RETURN_STOP_A("JSOP_SETNAME left operand is not the global object");

    // The rest of the work is in record_SetPropHit.
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
    jsval& rval = stackval(-1);
    jsval& lval = stackval(-2);

    if (JSVAL_IS_PRIMITIVE(rval))
        RETURN_STOP_A("JSOP_IN on non-object right operand");
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
            RETURN_ERROR_A("left operand of JSOP_IN didn't convert to a string-id");
        LIns* args[] = { get(&lval), obj_ins, cx_ins };
        x = lir->insCall(&js_HasNamedProperty_ci, args);
    } else {
        RETURN_STOP_A("string or integer expected");
    }

    guard(false, lir->ins2i(LIR_eq, x, JSVAL_TO_SPECIAL(JSVAL_VOID)), OOM_EXIT);
    x = lir->ins2i(LIR_eq, x, 1);

    JSTraceMonitor &localtm = *traceMonitor;
    JSContext *localcx = cx;

    JSObject* obj2;
    JSProperty* prop;
    bool ok = obj->lookupProperty(cx, id, &obj2, &prop);

    /* lookupProperty can reenter the interpreter and kill |this|. */
    if (!localtm.recorder) {
        if (prop)
            obj2->dropProperty(localcx, prop);
        return ARECORD_STOP;
    }

    if (!ok)
        RETURN_ERROR_A("obj->lookupProperty failed in JSOP_IN");
    bool cond = prop != NULL;
    if (prop)
        obj2->dropProperty(cx, prop);

    /*
     * The interpreter fuses comparisons and the following branch, so we have
     * to do that here as well.
     */
    fuseIf(cx->fp->regs->pc + 1, cond, x);

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
HasInstance(JSContext* cx, JSObject* ctor, jsval val)
{
    JSBool result = JS_FALSE;
    if (!ctor->map->ops->hasInstance(cx, ctor, val, &result))
        js_SetBuiltinError(cx);
    return result;
}
JS_DEFINE_CALLINFO_3(static, BOOL_FAIL, HasInstance, CONTEXT, OBJECT, JSVAL, 0, 0)

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INSTANCEOF()
{
    // If the rhs isn't an object, we are headed for a TypeError.
    jsval& ctor = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(ctor))
        RETURN_STOP_A("non-object on rhs of instanceof");

    jsval& val = stackval(-2);
    LIns* val_ins = box_jsval(val, get(&val));

    enterDeepBailCall();
    LIns* args[] = {val_ins, get(&ctor), cx_ins};
    stack(-2, lir->insCall(&HasInstance_ci, args));
    LIns* status_ins = lir->insLoad(LIR_ld,
                                    lirbuf->state,
                                    offsetof(InterpState, builtinStatus));
    pendingGuardCondition = lir->ins_eq0(status_ins);
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
    return ARECORD_STOP;
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
    jsatomid index = GET_INDEX(cx->fp->regs->pc + pcoff);
    index += atoms - cx->fp->script->atomMap.vector;
    return index;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LAMBDA()
{
    JSFunction* fun;
    fun = cx->fp->script->getFunction(getFullIndex());

    /*
     * Emit code to clone a null closure parented by this recorder's global
     * object, in order to preserve function object evaluation rules observable
     * via identity and mutation. But don't clone if our result is consumed by
     * JSOP_SETMETHOD or JSOP_INITMETHOD, since we optimize away the clone for
     * these combinations and clone only if the "method value" escapes.
     *
     * See jsops.cpp, the JSOP_LAMBDA null closure case. The JSOP_SETMETHOD and
     * JSOP_INITMETHOD logic governing the early ARECORD_CONTINUE returns below
     * must agree with the corresponding break-from-do-while(0) logic there.
     */
    if (FUN_NULL_CLOSURE(fun) && OBJ_GET_PARENT(cx, FUN_OBJECT(fun)) == globalObj) {
        JSOp op2 = JSOp(cx->fp->regs->pc[JSOP_LAMBDA_LENGTH]);

        if (op2 == JSOP_SETMETHOD) {
            jsval lval = stackval(-1);

            if (!JSVAL_IS_PRIMITIVE(lval) &&
                OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(lval)) == &js_ObjectClass) {
                stack(0, INS_CONSTOBJ(FUN_OBJECT(fun)));
                return ARECORD_CONTINUE;
            }
        } else if (op2 == JSOP_INITMETHOD) {
            stack(0, INS_CONSTOBJ(FUN_OBJECT(fun)));
            return ARECORD_CONTINUE;
        }

        LIns *proto_ins;
        CHECK_STATUS_A(getClassPrototype(JSProto_Function, proto_ins));

        LIns* args[] = { INS_CONSTOBJ(globalObj), proto_ins, INS_CONSTFUN(fun), cx_ins };
        LIns* x = lir->insCall(&js_NewNullClosure_ci, args);
        stack(0, x);
        return ARECORD_CONTINUE;
    }
    return ARECORD_STOP;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LAMBDA_FC()
{
    JSFunction* fun;
    fun = cx->fp->script->getFunction(getFullIndex());

    if (OBJ_GET_PARENT(cx, FUN_OBJECT(fun)) != globalObj)
        return ARECORD_STOP;

    LIns* args[] = {
        INS_CONSTOBJ(globalObj),
        INS_CONSTFUN(fun),
        cx_ins
    };
    LIns* call_ins = lir->insCall(&js_AllocFlatClosure_ci, args);
    guard(false,
          addName(lir->ins2(LIR_peq, call_ins, INS_NULL()),
                  "guard(js_AllocFlatClosure)"),
          OOM_EXIT);

    if (fun->u.i.nupvars) {
        JSUpvarArray *uva = fun->u.i.script->upvars();
        for (uint32 i = 0, n = uva->length; i < n; i++) {
            jsval v;
            LIns* upvar_ins = upvar(fun->u.i.script, uva, i, v);
            if (!upvar_ins)
                return ARECORD_STOP;
            LIns* dslots_ins = NULL;
            stobj_set_dslot(call_ins, i, dslots_ins, box_jsval(v, upvar_ins));
        }
    }

    stack(0, call_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLEE()
{
    stack(0, get(&cx->fp->argv[-2]));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETLOCALPOP()
{
    var(GET_SLOTNO(cx->fp->regs->pc), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_IFPRIMTOP()
{
    // Traces are type-specialized, including null vs. object, so we need do
    // nothing here. The upstream unbox_jsval called after valueOf or toString
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
    JSStackFrame* fp = cx->fp;
    if (!(fp->fun->flags & JSFUN_HEAVYWEIGHT)) {
        uintN slot = GET_ARGNO(fp->regs->pc);
        if (slot < fp->argc)
            stack(0, get(&cx->fp->argv[slot]));
        else
            stack(0, INS_VOID());
        return ARECORD_CONTINUE;
    }
    RETURN_STOP_A("can't trace JSOP_ARGSUB hard case");
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_ARGCNT()
{
    if (cx->fp->fun->flags & JSFUN_HEAVYWEIGHT)
        RETURN_STOP_A("can't trace heavyweight JSOP_ARGCNT");

    // argc is fixed on trace, so ideally we would simply generate LIR for
    // constant argc. But the user can mutate arguments.length in the
    // interpreter, so we have to check for that in the trace entry frame.
    // We also have to check that arguments.length has not been mutated
    // at record time, because if so we will generate incorrect constant
    // LIR, which will assert in alu().
    if (cx->fp->argsobj && js_IsOverriddenArgsLength(JSVAL_TO_OBJECT(cx->fp->argsobj)))
        RETURN_STOP_A("can't trace JSOP_ARGCNT if arguments.length has been modified");
    LIns *a_ins = get(&cx->fp->argsobj);
    if (callDepth == 0) {
        LIns *br = lir->insBranch(LIR_jt, lir->ins_peq0(a_ins), NULL);

        // The following implements js_IsOverriddenArgsLength on trace.
        // The '2' bit is set if length was overridden.
        LIns *len_ins = stobj_get_fslot(a_ins, JSSLOT_ARGS_LENGTH);
        LIns *ovr_ins = lir->ins2(LIR_piand, len_ins, INS_CONSTWORD(2));

        guard(true, lir->ins_peq0(ovr_ins), snapshot(BRANCH_EXIT));
        LIns *label = lir->ins0(LIR_label);
        br->setTarget(label);
    }
    stack(0, lir->insImmf(cx->fp->argc));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_DefLocalFunSetSlot(uint32 slot, JSObject* obj)
{
    JSFunction* fun = GET_FUNCTION_PRIVATE(cx, obj);

    if (FUN_NULL_CLOSURE(fun) && OBJ_GET_PARENT(cx, FUN_OBJECT(fun)) == globalObj) {
        LIns *proto_ins;
        CHECK_STATUS_A(getClassPrototype(JSProto_Function, proto_ins));

        LIns* args[] = { INS_CONSTOBJ(globalObj), proto_ins, INS_CONSTFUN(fun), cx_ins };
        LIns* x = lir->insCall(&js_NewNullClosure_ci, args);
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
TraceRecorder::record_JSOP_GETGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return ARECORD_CONTINUE; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    stack(0, get(&STOBJ_GET_SLOT(globalObj, slot)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SETGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        return ARECORD_CONTINUE; // We will see JSOP_NAME from the interpreter's jump, so no-op here.

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    set(&STOBJ_GET_SLOT(globalObj, slot), stack(-1));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INCGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
        return ARECORD_CONTINUE;

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(STOBJ_GET_SLOT(globalObj, slot), 1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_DECGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
        return ARECORD_CONTINUE;

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(STOBJ_GET_SLOT(globalObj, slot), -1));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GVARINC()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
        return ARECORD_CONTINUE;

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(STOBJ_GET_SLOT(globalObj, slot), 1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GVARDEC()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        // We will see JSOP_INCNAME from the interpreter's jump, so no-op here.
        return ARECORD_CONTINUE;

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    return InjectStatus(inc(STOBJ_GET_SLOT(globalObj, slot), -1, false));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_REGEXP()
{
    return ARECORD_STOP;
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
TraceRecorder::record_JSOP_XMLOBJECT()
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
    jsval& l = stackval(-1);
    JSObject* obj;
    LIns* obj_ins;
    LIns* this_ins;
    if (!JSVAL_IS_PRIMITIVE(l)) {
        obj = JSVAL_TO_OBJECT(l);
        obj_ins = get(&l);
        this_ins = obj_ins; // |this| for subsequent call
    } else {
        jsint i;
        debug_only_stmt(const char* protoname = NULL;)
        if (JSVAL_IS_STRING(l)) {
            i = JSProto_String;
            debug_only_stmt(protoname = "String.prototype";)
        } else if (JSVAL_IS_NUMBER(l)) {
            i = JSProto_Number;
            debug_only_stmt(protoname = "Number.prototype";)
        } else if (JSVAL_IS_SPECIAL(l)) {
            if (l == JSVAL_VOID)
                RETURN_STOP_A("callprop on void");
            guard(false, lir->ins2i(LIR_eq, get(&l), JSVAL_TO_SPECIAL(JSVAL_VOID)), MISMATCH_EXIT);
            i = JSProto_Boolean;
            debug_only_stmt(protoname = "Boolean.prototype";)
        } else {
            JS_ASSERT(JSVAL_IS_NULL(l) || JSVAL_IS_VOID(l));
            RETURN_STOP_A("callprop on null or void");
        }

        if (!js_GetClassPrototype(cx, NULL, INT_TO_JSID(i), &obj))
            RETURN_ERROR_A("GetClassPrototype failed!");

        obj_ins = INS_CONSTOBJ(obj);
        debug_only_stmt(obj_ins = addName(obj_ins, protoname);)
        this_ins = get(&l); // use primitive as |this|
    }

    JSObject* obj2;
    jsuword pcval;
    CHECK_STATUS_A(test_property_cache(obj, obj_ins, obj2, pcval));

    if (PCVAL_IS_NULL(pcval) || !PCVAL_IS_OBJECT(pcval))
        RETURN_STOP_A("callee is not an object");
    JS_ASSERT(HAS_FUNCTION_CLASS(PCVAL_TO_OBJECT(pcval)));

    if (JSVAL_IS_PRIMITIVE(l)) {
        JSFunction* fun = GET_FUNCTION_PRIVATE(cx, PCVAL_TO_OBJECT(pcval));
        if (!PRIMITIVE_THIS_TEST(fun, l))
            RETURN_STOP_A("callee does not accept primitive |this|");
    }

    stack(0, this_ins);
    stack(-1, INS_CONSTOBJ(PCVAL_TO_OBJECT(pcval)));
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
    stack(0, lir->insImmf(GET_UINT24(cx->fp->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INDEXBASE()
{
    atoms += GET_INDEXBASE(cx->fp->regs->pc);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RESETBASE()
{
    atoms = cx->fp->script->atomMap.vector;
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_RESETBASE0()
{
    atoms = cx->fp->script->atomMap.vector;
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
    JSStackFrame *fp = cx->fp;

    if (fp->imacpc) {
        /*
         * End of imacro, so return true to the interpreter immediately. The
         * interpreter's JSOP_STOP case will return from the imacro, back to
         * the pc after the calling op, still in the same JSStackFrame.
         */
        atoms = fp->script->atomMap.vector;
        return ARECORD_CONTINUE;
    }

    putArguments();

    /*
     * We know falling off the end of a constructor returns the new object that
     * was passed in via fp->argv[-1], while falling off the end of a function
     * returns undefined.
     *
     * NB: we do not support script rval (eval, API users who want the result
     * of the last expression-statement, debugger API calls).
     */
    if (fp->flags & JSFRAME_CONSTRUCTING) {
        JS_ASSERT(fp->thisv == fp->argv[-1]);
        rval_ins = get(&fp->argv[-1]);
    } else {
        rval_ins = INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID));
    }
    clearFrameSlotsFromCache();
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETXPROP()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l))
        RETURN_STOP_A("primitive-this for GETXPROP?");

    jsval* vp;
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
    obj = cx->fp->script->getObject(getFullIndex(0));

    LIns* void_ins = INS_CONST(JSVAL_TO_SPECIAL(JSVAL_VOID));
    for (int i = 0, n = OBJ_BLOCK_COUNT(cx, obj); i < n; i++)
        stack(i, void_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LEAVEBLOCK()
{
    /* We mustn't exit the lexical block we began recording in. */
    if (cx->fp->blockChain != lexicalBlock)
        return ARECORD_CONTINUE;
    else
        return ARECORD_STOP;
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
    uint32_t slot = GET_UINT16(cx->fp->regs->pc);
    JS_ASSERT(cx->fp->script->nfixed <= slot);
    JS_ASSERT(cx->fp->slots + slot < cx->fp->regs->sp - 1);
    jsval &arrayval = cx->fp->slots[slot];
    JS_ASSERT(JSVAL_IS_OBJECT(arrayval));
    JS_ASSERT(OBJ_IS_DENSE_ARRAY(cx, JSVAL_TO_OBJECT(arrayval)));
    LIns *array_ins = get(&arrayval);
    jsval &elt = stackval(-1);
    LIns *elt_ins = box_jsval(elt, get(&elt));

    LIns *args[] = { elt_ins, array_ins, cx_ins };
    LIns *ok_ins = lir->insCall(&js_ArrayCompPush_ci, args);
    guard(false, lir->ins_eq0(ok_ins), OOM_EXIT);
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
    int n = -1 - GET_UINT16(cx->fp->regs->pc);
    stack(n, v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETTHISPROP()
{
    LIns* this_ins;

    CHECK_STATUS_A(getThis(this_ins));

    /*
     * It's safe to just use cx->fp->thisv here because getThis() returns
     * ARECORD_STOP if thisv is not available.
     */
    JS_ASSERT(cx->fp->flags & JSFRAME_COMPUTED_THIS);
    CHECK_STATUS_A(getProp(JSVAL_TO_OBJECT(cx->fp->thisv), this_ins));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETARGPROP()
{
    return getProp(argval(GET_ARGNO(cx->fp->regs->pc)));
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_GETLOCALPROP()
{
    return getProp(varval(GET_SLOTNO(cx->fp->regs->pc)));
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
TraceRecorder::record_JSOP_CALLGVAR()
{
    jsval slotval = cx->fp->slots[GET_SLOTNO(cx->fp->regs->pc)];
    if (JSVAL_IS_NULL(slotval))
        // We will see JSOP_CALLNAME from the interpreter's jump, so no-op here.
        return ARECORD_CONTINUE;

    uint32 slot = JSVAL_TO_INT(slotval);

    if (!lazilyImportGlobalSlot(slot))
         RETURN_STOP_A("lazy import of global slot failed");

    jsval& v = STOBJ_GET_SLOT(globalObj, slot);
    stack(0, get(&v));
    stack(1, INS_NULL());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLLOCAL()
{
    uintN slot = GET_SLOTNO(cx->fp->regs->pc);
    stack(0, var(slot));
    stack(1, INS_NULL());
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLARG()
{
    uintN slot = GET_ARGNO(cx->fp->regs->pc);
    stack(0, arg(slot));
    stack(1, INS_NULL());
    return ARECORD_CONTINUE;
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
        js_SetBuiltinError(cx);
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
        js_SetBuiltinError(cx);
        return JSVAL_ERROR_COOKIE;
    }
    return tvr.value();
}

JS_DEFINE_TRCINFO_1(ObjectToIterator,
    (4, (static, OBJECT_FAIL, ObjectToIterator_tn, CONTEXT, PC, THIS, INT32, 0, 0)))
JS_DEFINE_TRCINFO_1(CallIteratorNext,
    (3, (static, JSVAL_FAIL,  CallIteratorNext_tn, CONTEXT, PC, THIS,        0, 0)))

static const struct BuiltinFunctionInfo {
    JSNativeTraceInfo *ti;
    int nargs;
} builtinFunctionInfo[JSBUILTIN_LIMIT] = {
    {&ObjectToIterator_trcinfo,   1},
    {&CallIteratorNext_trcinfo,   0},
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
                                         JS_DATA_TO_FUNC_PTR(JSNative, bfi->ti),
                                         bfi->nargs,
                                         JSFUN_FAST_NATIVE | JSFUN_TRCINFO,
                                         NULL,
                                         NULL);
        if (fun) {
            funobj = FUN_OBJECT(fun);
            STOBJ_CLEAR_PROTO(funobj);
            STOBJ_CLEAR_PARENT(funobj);

            JS_LOCK_GC(rt);
            if (!rt->builtinFunctions[index]) /* retest now that the lock is held */
                rt->builtinFunctions[index] = funobj;
            else
                funobj = rt->builtinFunctions[index];
            JS_UNLOCK_GC(rt);
        }
    }
    return funobj;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CALLBUILTIN()
{
    JSObject *obj = js_GetBuiltinFunction(cx, GET_INDEX(cx->fp->regs->pc));
    if (!obj)
        RETURN_ERROR_A("error in js_GetBuiltinFunction");

    stack(0, get(&stackval(-1)));
    stack(-1, INS_CONSTOBJ(obj));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INT8()
{
    stack(0, lir->insImmf(GET_INT8(cx->fp->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_INT32()
{
    stack(0, lir->insImmf(GET_INT32(cx->fp->regs->pc)));
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_LENGTH()
{
    jsval& l = stackval(-1);
    if (JSVAL_IS_PRIMITIVE(l)) {
        if (!JSVAL_IS_STRING(l))
            RETURN_STOP_A("non-string primitive JSOP_LENGTH unsupported");
        set(&l, lir->ins1(LIR_i2f, getStringLength(get(&l))));
        return ARECORD_CONTINUE;
    }

    JSObject* obj = JSVAL_TO_OBJECT(l);
    LIns* obj_ins = get(&l);

    if (STOBJ_GET_CLASS(obj) == &js_ArgumentsClass) {
        unsigned depth;
        JSStackFrame *afp = guardArguments(obj, obj_ins, &depth);
        if (!afp)
            RETURN_STOP_A("can't reach arguments object's frame");

        LIns* v_ins = lir->ins1(LIR_i2f, INS_CONST(afp->argc));
        set(&l, v_ins);
        return ARECORD_CONTINUE;
    }

    LIns* v_ins;
    if (OBJ_IS_ARRAY(cx, obj)) {
        if (OBJ_IS_DENSE_ARRAY(cx, obj)) {
            if (!guardDenseArray(obj, obj_ins, BRANCH_EXIT)) {
                JS_NOT_REACHED("OBJ_IS_DENSE_ARRAY but not?!?");
                return ARECORD_STOP;
            }
        } else {
            if (!guardClass(obj, obj_ins, &js_SlowArrayClass, snapshot(BRANCH_EXIT)))
                RETURN_STOP_A("can't trace length property access on non-array");
        }
        v_ins = lir->ins1(LIR_i2f, p2i(stobj_get_fslot(obj_ins, JSSLOT_ARRAY_LENGTH)));
    } else {
        if (!OBJ_IS_NATIVE(obj))
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

    uint32 len = GET_UINT16(cx->fp->regs->pc);
    cx->fp->assertValidStackDepth(len);

    LIns* args[] = { lir->insImm(len), proto_ins, cx_ins };
    LIns* v_ins = lir->insCall(&js_NewArrayWithSlots_ci, args);
    guard(false, lir->ins_peq0(v_ins), OOM_EXIT);

    LIns* dslots_ins = NULL;
    uint32 count = 0;
    for (uint32 i = 0; i < len; i++) {
        jsval& v = stackval(int(i) - int(len));
        if (v != JSVAL_HOLE)
            count++;
        LIns* elt_ins = box_jsval(v, get(&v));
        stobj_set_dslot(v_ins, i, dslots_ins, elt_ins);
    }

    if (count > 0)
        stobj_set_fslot(v_ins, JSSLOT_ARRAY_COUNT, INS_CONST(count));

    stack(-int(len), v_ins);
    return ARECORD_CONTINUE;
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_HOLE()
{
    stack(0, INS_CONST(JSVAL_TO_SPECIAL(JSVAL_HOLE)));
    return ARECORD_CONTINUE;
}

AbortableRecordingStatus
TraceRecorder::record_JSOP_TRACE()
{
    return ARECORD_CONTINUE;
}

static const uint32 sMaxConcatNSize = 32;

/*
 * Copy the result of defvalue.string back into concatn's arguments, clean the
 * stack, and return a pointer to the argument that was just overwritten.
 */
JS_REQUIRES_STACK jsval *
js_ConcatPostImacroStackCleanup(uint32 argc, JSFrameRegs &regs,
                                TraceRecorder *recorder)
{
    JS_ASSERT(*regs.pc == JSOP_IMACOP);

    /* Pop the argument offset and imacro return value. */
    jsint offset = JSVAL_TO_INT(*--regs.sp);
    jsval *imacroResult = --regs.sp;

    /* Replace non-primitive argument with new primitive argument. */
    jsval *vp = regs.sp - offset;
    JS_ASSERT(regs.sp - argc <= vp && vp < regs.sp);
    if (recorder)
        recorder->set(vp, recorder->get(imacroResult));
    *vp = *imacroResult;

    return vp;
}

/*
 * Initially, concatn takes N arguments on the stack, where N is immediate
 * operand.  To convert these arguments to primitives, we must repeatedly call
 * the defvalue.string imacro.  To achieve this iteration, defvalue.string ends
 * with imacop.  Hence, this function is called multiple times, each time with
 * one less non-primitive.  To keep track of where we are in the loop, we must
 * push an additional index value on the stack.  Hence, on all subsequent
 * entries, the stack is organized as follows (bottom to top):
 *
 *   prim[1]
 *   ...
 *   prim[i-1]
 *   nonprim[i]     argument to imacro
 *   arg[i+1]
 *   ...
 *   arg[N]
 *   primarg[i]     nonprim[i] converted to primitive
 *   i
 *
 * Hence, the stack setup on entry to this function (and JSOP_CONCATN in the
 * interpreter, on trace abort) is dependent on whether an imacro is in
 * progress.  When all of concatn's arguments are primitive, it emits a builtin
 * call and allows the actual JSOP_CONCATN to be executed by the interpreter.
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_CONCATN()
{
    JSStackFrame *fp = cx->fp;
    JSFrameRegs &regs = *fp->regs;

    /*
     * If we are in an imacro, we must have just finished a call to
     * defvalue.string.  Continue where we left off last time.
     */
    uint32 argc;
    jsval *loopStart;
    if (fp->imacpc) {
        JS_ASSERT(*fp->imacpc == JSOP_CONCATN);
        argc = GET_ARGC(fp->imacpc);
        loopStart = js_ConcatPostImacroStackCleanup(argc, regs, this) + 1;
    } else {
        argc = GET_ARGC(regs.pc);
        JS_ASSERT(argc > 0);
        loopStart = regs.sp - argc;

        /* Prevent code/alloca explosion. */
        if (argc > sMaxConcatNSize)
            return ARECORD_STOP;
    }

    /* Convert non-primitives to primitives using defvalue.string. */
    for (jsval *vp = loopStart; vp != regs.sp; ++vp) {
        if (!JSVAL_IS_PRIMITIVE(*vp)) {
            /*
             * In addition to the jsval we want the imacro to convert to
             * primitive, pass through the offset of the argument on the stack.
             */
            jsint offset = regs.sp - vp;

            /* Push the non-primitive to convert. */
            set(regs.sp, get(vp), true);
            *regs.sp++ = *vp;

            /* Push the argument index. */
            set(regs.sp, lir->insImm(offset), true);
            *regs.sp++ = INT_TO_JSVAL(offset);

            /* Nested imacro call OK because this is a tail call. */
            return InjectStatus(call_imacro(defvalue_imacros.string));
        }
    }

    /* Build an array of the stringified primitives. */
    int32_t bufSize = argc * sizeof(JSString *);
    LIns *buf_ins = lir->insAlloc(bufSize);
    int32_t d = 0;
    for (jsval *vp = regs.sp - argc; vp != regs.sp; ++vp, d += sizeof(void *))
        lir->insStorei(stringify(*vp), buf_ins, d);

    /* Perform concatenation using a builtin. */
    LIns *args[] = { lir->insImm(argc), buf_ins, cx_ins };
    LIns *concat = lir->insCall(&js_ConcatN_ci, args);
    guard(false, lir->ins_peq0(concat), OOM_EXIT);

    /* Update tracker with result. */
    jsval *afterPop = regs.sp - (argc - 1);
    set(afterPop - 1, concat);

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

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::record_JSOP_SHARPINIT()
{
    return ARECORD_STOP;
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
DumpPeerStability(JSTraceMonitor* tm, const void* ip, JSObject* globalObj, uint32 globalShape,
                  uint32 argc)
{
    TreeFragment* f;
    TreeInfo* ti;
    bool looped = false;
    unsigned length = 0;

    for (f = LookupLoop(tm, ip, globalObj, globalShape, argc); f != NULL; f = f->peer) {
        if (!f->treeInfo)
            continue;
        debug_only_printf(LC_TMRecorder, "Stability of fragment %p:\nENTRY STACK=", (void*)f);
        ti = f->treeInfo;
        if (looped)
            JS_ASSERT(ti->nStackTypes == length);
        for (unsigned i = 0; i < ti->nStackTypes; i++)
            debug_only_printf(LC_TMRecorder, "%c", typeChar[ti->stackTypeMap()[i]]);
        debug_only_print0(LC_TMRecorder, " GLOBALS=");
        for (unsigned i = 0; i < ti->nGlobalTypes(); i++)
            debug_only_printf(LC_TMRecorder, "%c", typeChar[ti->globalTypeMap()[i]]);
        debug_only_print0(LC_TMRecorder, "\n");
        UnstableExit* uexit = ti->unstableExits;
        while (uexit != NULL) {
            debug_only_print0(LC_TMRecorder, "EXIT  ");
            JSTraceType* m = uexit->exit->fullTypeMap();
            debug_only_print0(LC_TMRecorder, "STACK=");
            for (unsigned i = 0; i < uexit->exit->numStackSlots; i++)
                debug_only_printf(LC_TMRecorder, "%c", typeChar[m[i]]);
            debug_only_print0(LC_TMRecorder, " GLOBALS=");
            for (unsigned i = 0; i < uexit->exit->numGlobalSlots; i++) {
                debug_only_printf(LC_TMRecorder, "%c",
                                  typeChar[m[uexit->exit->numStackSlots + i]]);
            }
            debug_only_print0(LC_TMRecorder, "\n");
            uexit = uexit->next;
        }
        length = ti->nStackTypes;
        looped = true;
    }
}
#endif

#ifdef MOZ_TRACEVIS

FILE* traceVisLogFile = NULL;
JSHashTable *traceVisScriptTable = NULL;

JS_FRIEND_API(bool)
JS_StartTraceVis(const char* filename = "tracevis.dat")
{
    if (traceVisLogFile) {
        // If we're currently recording, first we must stop.
        JS_StopTraceVis();
    }

    traceVisLogFile = fopen(filename, "wb");
    if (!traceVisLogFile)
        return false;

    return true;
}

JS_FRIEND_API(JSBool)
js_StartTraceVis(JSContext *cx, JSObject *obj,
                 uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok;

    if (argc > 0 && JSVAL_IS_STRING(argv[0])) {
        JSString *str = JSVAL_TO_STRING(argv[0]);
        char *filename = js_DeflateString(cx, str->chars(), str->length());
        if (!filename)
            goto error;
        ok = JS_StartTraceVis(filename);
        cx->free(filename);
    } else {
        ok = JS_StartTraceVis();
    }

    if (ok) {
        fprintf(stderr, "started TraceVis recording\n");
        return JS_TRUE;
    }

  error:
    JS_ReportError(cx, "failed to start TraceVis recording");
    return JS_FALSE;
}

JS_FRIEND_API(bool)
JS_StopTraceVis()
{
    if (!traceVisLogFile)
        return false;

    fclose(traceVisLogFile); // not worth checking the result
    traceVisLogFile = NULL;

    return true;
}

JS_FRIEND_API(JSBool)
js_StopTraceVis(JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_StopTraceVis();

    if (ok)
        fprintf(stderr, "stopped TraceVis recording\n");
    else
        JS_ReportError(cx, "TraceVis isn't running");

    return ok;
}

#endif /* MOZ_TRACEVIS */

JS_REQUIRES_STACK void
js_CaptureStackTypes(JSContext* cx, unsigned callDepth, JSTraceType* typeMap)
{
    CaptureTypesVisitor capVisitor(cx, typeMap);
    VisitStackSlots(capVisitor, cx, callDepth);
}

JS_REQUIRES_STACK void
TraceRecorder::determineGlobalTypes(JSTraceType* typeMap)
{
    DetermineTypesVisitor detVisitor(*this, typeMap);
    VisitGlobalSlots(detVisitor, cx, *treeInfo->globalSlots);
}

#include "jsrecursion.cpp"

