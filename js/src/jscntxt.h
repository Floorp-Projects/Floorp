/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jscntxt_h___
#define jscntxt_h___
/*
 * JS execution context.
 */
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prclist.h"
#include "prlong.h"
#include "jsatom.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsregexp.h"

PR_BEGIN_EXTERN_C

struct JSRuntime {
    /* Garbage collector state, used by jsgc.c. */
    PRArenaPool         gcArenaPool;
    PRArenaPool         gcFlagsPool;
    PRHashTable         *gcRootsHash;
    JSGCThing           *gcFreeList;
    uint32              gcBytes;
    uint32              gcLastBytes;
    uint32              gcMaxBytes;
    uint32              gcLevel;
    uint32              gcNumber;
    JSBool              gcPoke;
    JSGCCallback        gcCallback;
#ifdef JS_GCMETER
    JSGCStats           gcStats;
#endif

    /* Literal table maintained by jsatom.c functions. */
    JSAtomState         atomState;

    /* Random number generator state, used by jsmath.c. */
    JSBool              rngInitialized;
    PRInt64               rngMultiplier;
    PRInt64               rngAddend;
    PRInt64               rngMask;
    PRInt64               rngSeed;
    jsdouble            rngDscale;

    /* Well-known numbers held for use by this runtime's contexts. */
    jsdouble            *jsNaN;
    jsdouble            *jsNegativeInfinity;
    jsdouble            *jsPositiveInfinity;

    /* Empty string held for use by this runtime's contexts. */
    JSString            *emptyString;

    /* List of active contexts sharing this runtime. */
    PRCList             contextList;

    /* These are used for debugging -- see jsprvtd.h and jsdbgapi.h. */
    JSTrapHandler       interruptHandler;
    void                *interruptHandlerData;
    JSNewScriptHook     newScriptHook;
    void                *newScriptHookData;
    JSDestroyScriptHook destroyScriptHook;
    void                *destroyScriptHookData;

    /* More debugging state, see jsdbgapi.c. */
    PRCList             trapList;
    PRCList             watchPointList;

    /* Weak links to properties, indexed by quickened get/set opcodes. */
    /* XXX must come after PRCLists or MSVC alignment bug bites empty lists */
    JSPropertyCache     propertyCache;

#ifdef JS_THREADSAFE
    /* These combine to interlock the GC and new requests. */
    PRLock              *gcLock;
    PRCondVar           *gcDone;
    PRCondVar           *requestDone;
    uint32              requestCount;

    /* Lock and owning thread pointer for JS_LOCK_RUNTIME. */
    JSThinLock          rtLock;
#endif
};

struct JSContext {
    PRCList             links;

    /* Interpreter activation count. */
    uintN               interpLevel;

    /* Runtime version control identifier and equality operators. */
    JSVersion           version;
    jsbytecode          jsop_eq;
    jsbytecode          jsop_ne;

    /* Data shared by threads in an address space. */
    JSRuntime           *runtime;

    /* Stack arena pool and frame pointer register. */
    PRArenaPool         stackPool;
    JSStackFrame        *fp;

    /* Temporary arena pools used while compiling and decompiling. */
    PRArenaPool         codePool;
    PRArenaPool         tempPool;

    /* Top-level object and pointer to top stack frame's scope chain. */
    JSObject            *globalObject;

    /* Most recently created things by type, members of the GC's root set. */
    JSGCThing           *newborn[GCX_NTYPES];

    /* Regular expression class statics (XXX not shared globally). */
    JSRegExpStatics     regExpStatics;

    /* State for object and array toSource conversion. */
    JSSharpObjectMap    sharpObjectMap;

    /* Last message string and trace file for debugging. */
    char                *lastMessage;
#ifdef DEBUG
    void                *tracefp;
#endif

    /* Per-context optional user callbacks. */
    JSBranchCallback    branchCallback;
    JSErrorReporter     errorReporter;

    /* Client opaque pointer */
    void                *pvt;
    
    /* Java environment and JS errors to throw as exceptions. */
    void                *javaEnv;
    void                *savedErrors;

#ifdef JS_THREADSAFE
    prword              thread;
    JSPackedBool        gcActive;
    jsrefcount          requestDepth;
#endif
};

typedef struct JSInterpreterHooks {
    void (*destroyContext)(JSContext *cx);
    void (*destroyScript)(JSContext *cx, JSScript *script);
    void (*destroyFrame)(JSContext *cx, JSStackFrame *frame);
} JSInterpreterHooks;

extern JSInterpreterHooks *js_InterpreterHooks;

extern JS_FRIEND_API(void)
js_SetInterpreterHooks(JSInterpreterHooks *hooks);

extern JSContext *
js_NewContext(JSRuntime *rt, size_t stacksize);

extern void
js_DestroyContext(JSContext *cx);

extern JSContext *
js_ContextIterator(JSRuntime *rt, JSContext **iterp);

/*
 * Report an exception, which is currently realized as a printf-style format
 * string and its arguments.
 */
#ifdef va_start
extern void
js_ReportErrorVA(JSContext *cx, const char *format, va_list ap);
#endif

/*
 * Report an exception using a previously composed JSErrorReport.
 */
extern JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *report);

extern void
js_ReportIsNotDefined(JSContext *cx, const char *name);

PR_END_EXTERN_C

#endif /* jscntxt_h___ */
