/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef jscntxt_h___
#define jscntxt_h___
/*
 * JS execution context.
 */
#include "jsarena.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jslong.h"
#include "jsatom.h"
#include "jsconfig.h"
#include "jsdhash.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsregexp.h"

JS_BEGIN_EXTERN_C

typedef enum JSGCMode { JS_NO_GC, JS_MAYBE_GC, JS_FORCE_GC } JSGCMode;

typedef enum JSRuntimeState {
    JSRTS_DOWN,
    JSRTS_LAUNCHING,
    JSRTS_UP,
    JSRTS_LANDING
} JSRuntimeState;

struct JSRuntime {
    JSRuntimeState      state;

    /* Garbage collector state, used by jsgc.c. */
    JSArenaPool         gcArenaPool;
    JSDHashTable        gcRootsHash;
    JSDHashTable        *gcLocksHash;
    JSGCThing           *gcFreeList;
    jsrefcount          gcDisabled;
    uint32              gcBytes;
    uint32              gcLastBytes;
    uint32              gcMaxBytes;
    uint32              gcLevel;
    uint32              gcNumber;
    JSPackedBool        gcPoke;
    JSPackedBool        gcRunning;
    JSGCCallback        gcCallback;
    uint32              gcMallocBytes;
#ifdef JS_GCMETER
    JSGCStats           gcStats;
#endif

    /* Literal table maintained by jsatom.c functions. */
    JSAtomState         atomState;

    /* Random number generator state, used by jsmath.c. */
    JSBool              rngInitialized;
    int64               rngMultiplier;
    int64               rngAddend;
    int64               rngMask;
    int64               rngSeed;
    jsdouble            rngDscale;

    /* Well-known numbers held for use by this runtime's contexts. */
    jsdouble            *jsNaN;
    jsdouble            *jsNegativeInfinity;
    jsdouble            *jsPositiveInfinity;

    /* Empty string held for use by this runtime's contexts. */
    JSString            *emptyString;

    /* List of active contexts sharing this runtime. */
    JSCList             contextList;

    /* These are used for debugging -- see jsprvtd.h and jsdbgapi.h. */
    JSTrapHandler       interruptHandler;
    void                *interruptHandlerData;
    JSNewScriptHook     newScriptHook;
    void                *newScriptHookData;
    JSDestroyScriptHook destroyScriptHook;
    void                *destroyScriptHookData;
    JSTrapHandler       debuggerHandler;
    void                *debuggerHandlerData;
    JSSourceHandler     sourceHandler;
    void                *sourceHandlerData;
    JSInterpreterHook   executeHook;
    void                *executeHookData;
    JSInterpreterHook   callHook;
    void                *callHookData;
    JSObjectHook        objectHook;
    void                *objectHookData;
    JSTrapHandler       throwHook;
    void                *throwHookData;
    JSDebugErrorHook    debugErrorHook;
    void                *debugErrorHookData;

    /* More debugging state, see jsdbgapi.c. */
    JSCList             trapList;
    JSCList             watchPointList;

    /* Weak links to properties, indexed by quickened get/set opcodes. */
    /* XXX must come after JSCLists or MSVC alignment bug bites empty lists */
    JSPropertyCache     propertyCache;

    /* Client opaque pointer */
    void                *data;

#ifdef JS_THREADSAFE
    /* These combine to interlock the GC and new requests. */
    PRLock              *gcLock;
    PRCondVar           *gcDone;
    PRCondVar           *requestDone;
    uint32              requestCount;
    jsword              gcThread;

    /* Lock and owning thread pointer for JS_LOCK_RUNTIME. */
    PRLock              *rtLock;
#ifdef DEBUG
    jsword              rtLockOwner;
#endif

    /* Used to synchronize down/up state change; uses rtLock. */
    PRCondVar           *stateChange;

    /* Used to serialize cycle checks when setting __proto__ or __parent__. */
    PRLock              *setSlotLock;
    PRCondVar           *setSlotDone;
    JSBool              setSlotBusy;
    JSScope             *setSlotScope;  /* deadlock avoidance, see jslock.c */

    /*
     * State for sharing single-threaded scopes, once a second thread tries to
     * lock a scope.  The scopeSharingDone condvar is protected by rt->gcLock,
     * to minimize number of locks taken in JS_EndRequest.
     *
     * The scopeSharingTodo linked list is likewise "global" per runtime, not
     * one-list-per-context, to conserve space over all contexts, optimizing
     * for the likely case that scopes become shared rarely, and among a very
     * small set of threads (contexts).
     */
    PRCondVar           *scopeSharingDone;
    JSScope             *scopeSharingTodo;

/*
 * Magic terminator for the rt->scopeSharingTodo linked list, threaded through
 * scope->u.link.  This hack allows us to test whether a scope is on the list
 * by asking whether scope->u.link is non-null.  We use a large, likely bogus
 * pointer here to distinguish this value from any valid u.count (small int)
 * value.
 */
#define NO_SCOPE_SHARING_TODO   ((JSScope *) 0xfeedbeef)
#endif

    /* Security principals serialization support. */
    JSPrincipalsTranscoder principalsTranscoder;

#ifdef DEBUG
    /* Function invocation metering. */
    jsrefcount          inlineCalls;
    jsrefcount          nativeCalls;
    jsrefcount          nonInlineCalls;
    jsrefcount          constructs;

    /* Scope lock and related metering. */
    jsrefcount          claimAttempts;
    jsrefcount          claimedScopes;
    jsrefcount          deadContexts;
    jsrefcount          deadlocksAvoided;
    jsrefcount          liveScopes;
    jsrefcount          sharedScopes;
    jsrefcount          totalScopes;
    jsrefcount          badUndependStrings;

    /* String instrumentation. */
    jsrefcount          liveStrings;
    jsrefcount          totalStrings;
    jsrefcount          liveDependentStrings;
    jsrefcount          totalDependentStrings;
    double              lengthSum;
    double              lengthSquaredSum;
    double              strdepLengthSum;
    double              strdepLengthSquaredSum;
#endif
};

#ifdef DEBUG
# define JS_RUNTIME_METER(rt, which)    JS_ATOMIC_INCREMENT(&(rt)->which)
# define JS_RUNTIME_UNMETER(rt, which)  JS_ATOMIC_DECREMENT(&(rt)->which)
#else
# define JS_RUNTIME_METER(rt, which)    /* nothing */
# define JS_RUNTIME_UNMETER(rt, which)  /* nothing */
#endif

#define JS_ENABLE_GC(rt)    JS_ATOMIC_DECREMENT(&(rt)->gcDisabled);
#define JS_DISABLE_GC(rt)   JS_ATOMIC_INCREMENT(&(rt)->gcDisabled);

#ifdef JS_ARGUMENT_FORMATTER_DEFINED
/*
 * Linked list mapping format strings for JS_{Convert,Push}Arguments{,VA} to
 * formatter functions.  Elements are sorted in non-increasing format string
 * length order.
 */
struct JSArgumentFormatMap {
    const char          *format;
    size_t              length;
    JSArgumentFormatter formatter;
    JSArgumentFormatMap *next;
};
#endif

struct JSStackHeader {
    uintN               nslots;
    JSStackHeader       *down;
};

#define JS_STACK_SEGMENT(sh)    ((jsval *)(sh) + 2)

/*
 * Key and entry types for the JSContext.resolvingTable hash table, typedef'd
 * here because all consumers need to see these declarations (and not just the
 * typedef names, as would be the case for an opaque pointer-to-typedef'd-type
 * declaration), along with cx->resolvingTable.
 */
typedef struct JSResolvingKey {
    JSObject            *obj;
    jsid                id;
} JSResolvingKey;

typedef struct JSResolvingEntry {
    JSDHashEntryHdr     hdr;
    JSResolvingKey      key;
} JSResolvingEntry;

struct JSContext {
    JSCList             links;

    /* Interpreter activation count. */
    uintN               interpLevel;

    /* Runtime version control identifier and equality operators. */
    JSVersion           version;
    jsbytecode          jsop_eq;
    jsbytecode          jsop_ne;

    /* Data shared by threads in an address space. */
    JSRuntime           *runtime;

    /* Stack arena pool and frame pointer register. */
    JSArenaPool         stackPool;
    JSStackFrame        *fp;

    /* Temporary arena pools used while compiling and decompiling. */
    JSArenaPool         codePool;
    JSArenaPool         notePool;
    JSArenaPool         tempPool;

    /* Top-level object and pointer to top stack frame's scope chain. */
    JSObject            *globalObject;

    /* Most recently created things by type, members of the GC's root set. */
    JSGCThing           *newborn[GCX_NTYPES];

    /* Regular expression class statics (XXX not shared globally). */
    JSRegExpStatics     regExpStatics;

    /* State for object and array toSource conversion. */
    JSSharpObjectMap    sharpObjectMap;

    /* Argument formatter support for JS_{Convert,Push}Arguments{,VA}. */
    JSArgumentFormatMap *argumentFormatMap;

    /* Last message string and trace file for debugging. */
    char                *lastMessage;
#ifdef DEBUG
    void                *tracefp;
#endif

    /* Per-context optional user callbacks. */
    JSBranchCallback    branchCallback;
    JSErrorReporter     errorReporter;

    /* Client opaque pointer */
    void                *data;

    /* GC and thread-safe state. */
    JSStackFrame        *dormantFrameChain; /* dormant stack frame to scan */
#ifdef JS_THREADSAFE
    jsword              thread;
    jsrefcount          requestDepth;
    JSScope             *scopeToShare;      /* weak reference, see jslock.c */
#endif

#if JS_HAS_LVALUE_RETURN
    /*
     * Secondary return value from native method called on the left-hand side
     * of an assignment operator.  The native should store the object in which
     * to set a property in *rval, and return the property's id expressed as a
     * jsval by calling JS_SetCallReturnValue2(cx, idval).
     */
    jsval               rval2;
    JSPackedBool        rval2set;
#endif

    /* True if clearing a scope -- used to coalesce property cache flushes. */
    JSPackedBool        clearingScope;

    /* Exception state (NB: throwing packs with clearingScope, above). */
    JSPackedBool        throwing;           /* is there a pending exception? */
    jsval               exception;          /* most-recently-thrown exceptin */

    /* Per-context options. */
    uint32              options;            /* see jsapi.h for JSOPTION_* */

    /* Locale specific callbacks for string conversion. */
    JSLocaleCallbacks   *localeCallbacks;

    /*
     * cx->resolving is non-zero if we are init'ing standard classes lazily, or
     * if we are otherwise recursing indirectly from js_LookupProperty through
     * a JSClass.resolve hook.  It is used together with cx->resolvingTable to
     * limit runaway recursion (see jsapi.c and jsobj.c).
     */
    uint32              resolving;
    JSDHashTable        *resolvingTable;

    /* PDL of stack headers describing stack slots not rooted by argv, etc. */
    JSStackHeader       *stackHeaders;
};

/* Slightly more readable macros, also to hide bitset implementation detail. */
#define JS_HAS_STRICT_OPTION(cx)    ((cx)->options & JSOPTION_STRICT)
#define JS_HAS_WERROR_OPTION(cx)    ((cx)->options & JSOPTION_WERROR)

extern JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize);

extern void
js_DestroyContext(JSContext *cx, JSGCMode gcmode);

extern JSBool
js_LiveContext(JSRuntime *rt, JSContext *cx);

extern JSContext *
js_ContextIterator(JSRuntime *rt, JSContext **iterp);

/*
 * Report an exception, which is currently realized as a printf-style format
 * string and its arguments.
 */
typedef enum JSErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "js.msg"
#undef MSG_DEF
    JSErr_Limit
} JSErrNum;

extern const JSErrorFormatString *
js_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber);

#ifdef va_start
extern JSBool
js_ReportErrorVA(JSContext *cx, uintN flags, const char *format, va_list ap);

extern JSBool
js_ReportErrorNumberVA(JSContext *cx, uintN flags, JSErrorCallback callback,
		       void *userRef, const uintN errorNumber,
                       JSBool charArgs, va_list ap);

extern JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
			void *userRef, const uintN errorNumber,
			char **message, JSErrorReport *reportp,
                        JSBool *warningp, JSBool charArgs, va_list ap);
#endif

extern void
js_ReportOutOfMemory(JSContext *cx, JSErrorCallback errorCallback);

/*
 * Report an exception using a previously composed JSErrorReport.
 * XXXbe remove from "friend" API
 */
extern JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *report);

extern void
js_ReportIsNotDefined(JSContext *cx, const char *name);

extern JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

JS_END_EXTERN_C

#endif /* jscntxt_h___ */
