/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

typedef struct JSPropertyTreeEntry {
    JSDHashEntryHdr     hdr;
    JSScopeProperty     *child;
} JSPropertyTreeEntry;

struct JSRuntime {
    /* Runtime state, synchronized by the stateChange/gcLock condvar/lock. */
    JSRuntimeState      state;

    /* Garbage collector state, used by jsgc.c. */
    JSArenaPool         gcArenaPool;
    JSDHashTable        gcRootsHash;
    JSDHashTable        *gcLocksHash;
    JSGCThing           *gcFreeList;
    jsrefcount          gcKeepAtoms;
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

    /* List of active contexts sharing this runtime; protected by gcLock. */
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

    /* Used to synchronize down/up state change; protected by gcLock. */
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
#endif /* JS_THREADSAFE */

    /*
     * Check property accessibility for objects of arbitrary class.  Used at
     * present to check f.caller accessibility for any function object f.
     */
    JSCheckAccessOp     checkObjectAccess;

    /* Security principals serialization support. */
    JSPrincipalsTranscoder principalsTranscoder;

    /* Shared scope property tree, and allocator for its nodes. */
    JSDHashTable        propertyTreeHash;
    JSScopeProperty     *propertyFreeList;
    JSArenaPool         propertyArenaPool;

    /* Script filename table. */
    struct JSHashTable  *scriptFilenameTable;
#ifdef JS_THREADSAFE
    PRLock              *scriptFilenameTableLock;
#endif

    /* Number localization, used by jsnum.c */
    const char          *thousandsSeparator;
    const char          *decimalSeparator;
    const char          *numGrouping;

#ifdef DEBUG
    /* Function invocation metering. */
    jsrefcount          inlineCalls;
    jsrefcount          nativeCalls;
    jsrefcount          nonInlineCalls;
    jsrefcount          constructs;

    /* Scope lock and property metering. */
    jsrefcount          claimAttempts;
    jsrefcount          claimedScopes;
    jsrefcount          deadContexts;
    jsrefcount          deadlocksAvoided;
    jsrefcount          liveScopes;
    jsrefcount          sharedScopes;
    jsrefcount          totalScopes;
    jsrefcount          badUndependStrings;
    jsrefcount          liveScopeProps;
    jsrefcount          totalScopeProps;
    jsrefcount          livePropTreeNodes;
    jsrefcount          duplicatePropTreeNodes;
    jsrefcount          totalPropTreeNodes;
    jsrefcount          propTreeKidsChunks;
    jsrefcount          middleDeleteFixups;

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

#define JS_KEEP_ATOMS(rt)   JS_ATOMIC_INCREMENT(&(rt)->gcKeepAtoms);
#define JS_UNKEEP_ATOMS(rt) JS_ATOMIC_DECREMENT(&(rt)->gcKeepAtoms);

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
    uint32              flags;
} JSResolvingEntry;

#define JSRESFLAG_LOOKUP        0x1     /* resolving id from lookup */
#define JSRESFLAG_WATCH         0x2     /* resolving id from watch */

typedef struct JSLocalRootChunk JSLocalRootChunk;

#define JSLRS_CHUNK_SHIFT       6
#define JSLRS_CHUNK_SIZE        JS_BIT(JSLRS_CHUNK_SHIFT)
#define JSLRS_CHUNK_MASK        JS_BITMASK(JSLRS_CHUNK_SHIFT)

struct JSLocalRootChunk {
    jsval               roots[JSLRS_CHUNK_SIZE];
    JSLocalRootChunk    *down;
};

typedef struct JSLocalRootStack {
    uint16              scopeMark;
    uint16              rootCount;
    JSLocalRootChunk    *topChunk;
    JSLocalRootChunk    firstChunk;
} JSLocalRootStack;

#define JSLRS_NULL_MARK ((uint16) -1)

struct JSContext {
    JSCList             links;

    /* Interpreter activation count. */
    uintN               interpLevel;

    /* Limit pointer for checking stack consumption during recursion. */
    jsuword             stackLimit;

    /* Runtime version control identifier and equality operators. */
    JSVersion           version;
    jsbytecode          jsop_eq;
    jsbytecode          jsop_ne;

    /* Data shared by threads in an address space. */
    JSRuntime           *runtime;

    /* Stack arena pool and frame pointer register. */
    JSArenaPool         stackPool;
    JSStackFrame        *fp;

    /* Temporary arena pool used while compiling and decompiling. */
    JSArenaPool         tempPool;

    /* Top-level object and pointer to top stack frame's scope chain. */
    JSObject            *globalObject;

    /* Most recently created things by type, members of the GC's root set. */
    JSGCThing           *newborn[GCX_NTYPES];

    /* Atom root for the last-looked-up atom on this context. */
    JSAtom              *lastAtom;

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
    JSScope             *lockedSealedScope; /* weak ref, for low-cost sealed
                                               scope locking */
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

    /*
     * True if creating an exception object, to prevent runaway recursion.
     * NB: creatingException packs with rval2set, #if JS_HAS_LVALUE_RETURN,
     * and with throwing, below.
     */
    JSPackedBool        creatingException;

    /*
     * Exception state -- the exception member is a GC root by definition.
     * NB: throwing packs with creatingException and rval2set, above.
     */
    JSPackedBool        throwing;           /* is there a pending exception? */
    jsval               exception;          /* most-recently-thrown exception */

    /* Per-context options. */
    uint32              options;            /* see jsapi.h for JSOPTION_* */

    /* Locale specific callbacks for string conversion. */
    JSLocaleCallbacks   *localeCallbacks;

    /*
     * cx->resolvingTable is non-null and non-empty if we are initializing
     * standard classes lazily, or if we are otherwise recursing indirectly
     * from js_LookupProperty through a JSClass.resolve hook.  It is used to
     * limit runaway recursion (see jsapi.c and jsobj.c).
     */
    JSDHashTable        *resolvingTable;

    /* PDL of stack headers describing stack slots not rooted by argv, etc. */
    JSStackHeader       *stackHeaders;

    /* Optional hook to find principals for an object being accessed on cx. */
    JSObjectPrincipalsFinder findObjectPrincipals;

    /* Optional stack of scoped local GC roots. */
    JSLocalRootStack    *localRootStack;
};

/*
 * Slightly more readable macros, also to hide bitset implementation detail.
 * XXX beware non-boolean truth values, which belie the bitset hiding claim!
 */
#define JS_HAS_STRICT_OPTION(cx)        ((cx)->options & JSOPTION_STRICT)
#define JS_HAS_WERROR_OPTION(cx)        ((cx)->options & JSOPTION_WERROR)
#define JS_HAS_COMPILE_N_GO_OPTION(cx)  ((cx)->options & JSOPTION_COMPILE_N_GO)
#define JS_HAS_ATLINE_OPTION(cx)        ((cx)->options & JSOPTION_ATLINE)

extern JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize);

extern void
js_DestroyContext(JSContext *cx, JSGCMode gcmode);

/*
 * Return true if cx points to a context in rt->contextList, else return false.
 * NB: the caller (see jslock.c:ClaimScope) must hold rt->gcLock.
 */
extern JSBool
js_ValidContextPointer(JSRuntime *rt, JSContext *cx);

/*
 * If unlocked, acquire and release rt->gcLock around *iterp update; otherwise
 * the caller must be holding rt->gcLock.
 */
extern JSContext *
js_ContextIterator(JSRuntime *rt, JSBool unlocked, JSContext **iterp);

/*
 * JSClass.resolve and watchpoint recursion damping machinery.
 */
extern JSBool
js_StartResolving(JSContext *cx, JSResolvingKey *key, uint32 flag,
                  JSResolvingEntry **entryp);

extern void
js_StopResolving(JSContext *cx, JSResolvingKey *key, uint32 flag,
                 JSResolvingEntry *entry, uint32 generation);

/*
 * Local root set management.
 */
extern JSBool
js_EnterLocalRootScope(JSContext *cx);

extern void
js_LeaveLocalRootScope(JSContext *cx);

extern void
js_ForgetLocalRoot(JSContext *cx, jsval v);

extern int
js_PushLocalRoot(JSContext *cx, JSLocalRootStack *lrs, jsval v);

extern void
js_MarkLocalRoots(JSContext *cx, JSLocalRootStack *lrs);

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

/*
 * See JS_SetThreadStackLimit in jsapi.c, where we check that the stack grows
 * in the expected direction.  On Unix-y systems, JS_STACK_GROWTH_DIRECTION is
 * computed on the build host by jscpucfg.c and written into jsautocfg.h.  The
 * macro is hardcoded in jscpucfg.h on Windows and Mac systems (for historical
 * reasons pre-dating autoconf usage).
 */
#if JS_STACK_GROWTH_DIRECTION > 0
# define JS_CHECK_STACK_SIZE(cx, lval)  ((jsuword)&(lval) < (cx)->stackLimit)
#else
# define JS_CHECK_STACK_SIZE(cx, lval)  ((jsuword)&(lval) > (cx)->stackLimit)
#endif

JS_END_EXTERN_C

#endif /* jscntxt_h___ */
