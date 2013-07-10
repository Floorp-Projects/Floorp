/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS execution context. */

#ifndef jscntxt_h
#define jscntxt_h

#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"

#include <string.h>

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprvtd.h"

#include "js/HashTable.h"
#include "js/Vector.h"
#include "vm/Runtime.h"
#include "vm/Stack.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#pragma warning(push)
#pragma warning(disable:4355) /* Silence warning about "this" used in base member initializer list */
#endif

namespace js {

struct CallsiteCloneKey {
    /* The original function that we are cloning. */
    JSFunction *original;

    /* The script of the call. */
    JSScript *script;

    /* The offset of the call. */
    uint32_t offset;

    CallsiteCloneKey(JSFunction *f, JSScript *s, uint32_t o) : original(f), script(s), offset(o) {}

    typedef CallsiteCloneKey Lookup;

    static inline uint32_t hash(CallsiteCloneKey key) {
        return uint32_t(size_t(key.script->code + key.offset) ^ size_t(key.original));
    }

    static inline bool match(const CallsiteCloneKey &a, const CallsiteCloneKey &b) {
        return a.script == b.script && a.offset == b.offset && a.original == b.original;
    }
};

typedef HashMap<CallsiteCloneKey,
                ReadBarriered<JSFunction>,
                CallsiteCloneKey,
                SystemAllocPolicy> CallsiteCloneTable;

JSFunction *CloneFunctionAtCallsite(JSContext *cx, HandleFunction fun,
                                    HandleScript script, jsbytecode *pc);

typedef HashSet<JSObject *> ObjectSet;
typedef HashSet<Shape *> ShapeSet;

/* Detects cycles when traversing an object graph. */
class AutoCycleDetector
{
    JSContext *cx;
    RootedObject obj;
    bool cyclic;
    uint32_t hashsetGenerationAtInit;
    ObjectSet::AddPtr hashsetAddPointer;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoCycleDetector(JSContext *cx, HandleObject objArg
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), obj(cx, objArg), cyclic(true)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoCycleDetector();

    bool init();

    bool foundCycle() { return cyclic; }
};

/* Updates references in the cycle detection set if the GC moves them. */
extern void
TraceCycleDetectionSet(JSTracer *trc, ObjectSet &set);

struct AutoResolving;

class ForkJoinSlice;

/*
 * ThreadSafeContext is the base class for both JSContext, the "normal"
 * sequential context, and ForkJoinSlice, the per-thread parallel context used
 * in PJS.
 *
 * When cast to a ThreadSafeContext, the only usable operations are casting
 * back to the context from which it came, and generic allocation
 * operations. These generic versions branch internally based on whether the
 * underneath context is really a JSContext or a ForkJoinSlice, and are in
 * general more expensive than using the context directly.
 *
 * Thus, ThreadSafeContext should only be used for VM functions that may be
 * called in both sequential and parallel execution. The most likely class of
 * VM functions that do these are those that allocate commonly used data
 * structures, such as concatenating strings and extending elements.
 */
struct ThreadSafeContext : js::ContextFriendFields,
                           public MallocProvider<ThreadSafeContext>
{
  public:
    enum ContextKind {
        Context_JS,
        Context_ForkJoin
    };

  private:
    ContextKind contextKind_;

  public:
    PerThreadData *perThreadData;

    explicit ThreadSafeContext(JSRuntime *rt, PerThreadData *pt, ContextKind kind);

    bool isJSContext() const;
    JSContext *asJSContext();

    bool isForkJoinSlice() const;
    ForkJoinSlice *asForkJoinSlice();

#ifdef JSGC_GENERATIONAL
    inline bool hasNursery() const {
        return isJSContext();
    }

    inline js::Nursery &nursery() {
        JS_ASSERT(hasNursery());
        return runtime_->gcNursery;
    }
#endif

    /* Cut outs for string operations. */
    StaticStrings &staticStrings() { return runtime_->staticStrings; }
    JSAtomState &names() { return runtime_->atomState; }

    /*
     * Allocator used when allocating GCThings on this context. If we are a
     * JSContext, this is the Zone allocator of the JSContext's zone. If we
     * are the per-thread data of a ForkJoinSlice, this is a per-thread
     * allocator.
     *
     * This does not live in PerThreadData because the notion of an allocator
     * is only per-thread in PJS. The runtime (and the main thread) can have
     * more than one zone, each with its own allocator, and it's up to the
     * context to specify what compartment and zone we are operating in.
     */
  protected:
    Allocator *allocator_;

  public:
    static size_t offsetOfAllocator() { return offsetof(ThreadSafeContext, allocator_); }

    inline Allocator *const allocator();

    /* GC support. */
    AllowGC allowGC() const {
        switch (contextKind_) {
          case Context_JS:
            return CanGC;
          case Context_ForkJoin:
            return NoGC;
          default:
            /* Silence warnings. */
            MOZ_ASSUME_UNREACHABLE("Bad context kind");
        }
    }

    template <typename T>
    bool isInsideCurrentZone(T thing) const {
        return thing->isInsideZone(zone_);
    }

    void *onOutOfMemory(void *p, size_t nbytes) {
        return runtime_->onOutOfMemory(p, nbytes, isJSContext() ? asJSContext() : NULL);
    }
    inline void updateMallocCounter(size_t nbytes) {
        /* Note: this is racy. */
        runtime_->updateMallocCounter(zone_, nbytes);
    }
    void reportAllocationOverflow() {
        js_ReportAllocationOverflow(isJSContext() ? asJSContext() : NULL);
    }
};

} /* namespace js */

struct JSContext : js::ThreadSafeContext,
                   public mozilla::LinkedListElement<JSContext>
{
    explicit JSContext(JSRuntime *rt);
    ~JSContext();

    JSRuntime *runtime() const { return runtime_; }
    JSCompartment *compartment() const { return compartment_; }

    inline JS::Zone *zone() const {
        JS_ASSERT_IF(!compartment(), !zone_);
        JS_ASSERT_IF(compartment(), js::GetCompartmentZone(compartment()) == zone_);
        return zone_;
    }
    js::PerThreadData &mainThread() const { return runtime()->mainThread; }

  private:
    /* Exception state -- the exception member is a GC root by definition. */
    bool                throwing;            /* is there a pending exception? */
    js::Value           exception;           /* most-recently-thrown exception */

    /* Per-context options. */
    unsigned            options_;            /* see jsapi.h for JSOPTION_* */

  public:
    int32_t             reportGranularity;  /* see vm/Probes.h */

    js::AutoResolving   *resolvingList;

    /* True if generating an error, to prevent runaway recursion. */
    bool                generatingError;

    inline void setCompartment(JSCompartment *comp);

    /*
     * "Entering" a compartment changes cx->compartment (which changes
     * cx->global). Note that this does not push any StackFrame which means
     * that it is possible for cx->fp()->compartment() != cx->compartment.
     * This is not a problem since, in general, most places in the VM cannot
     * know that they were called from script (e.g., they may have been called
     * through the JSAPI via JS_CallFunction) and thus cannot expect fp.
     *
     * Compartments should be entered/left in a LIFO fasion. The depth of this
     * enter/leave stack is maintained by enterCompartmentDepth_ and queried by
     * hasEnteredCompartment.
     *
     * To enter a compartment, code should prefer using AutoCompartment over
     * manually calling cx->enterCompartment/leaveCompartment.
     */
  private:
    unsigned            enterCompartmentDepth_;
  public:
    bool hasEnteredCompartment() const {
        return enterCompartmentDepth_ > 0;
    }
#ifdef DEBUG
    unsigned getEnterCompartmentDepth() const {
        return enterCompartmentDepth_;
    }
#endif

    inline void enterCompartment(JSCompartment *c);
    inline void leaveCompartment(JSCompartment *oldCompartment);

    /* See JS_SaveFrameChain/JS_RestoreFrameChain. */
  private:
    struct SavedFrameChain {
        SavedFrameChain(JSCompartment *comp, unsigned count)
          : compartment(comp), enterCompartmentCount(count) {}
        JSCompartment *compartment;
        unsigned enterCompartmentCount;
    };
    typedef js::Vector<SavedFrameChain, 1, js::SystemAllocPolicy> SaveStack;
    SaveStack           savedFrameChains_;
  public:
    bool saveFrameChain();
    void restoreFrameChain();

    /*
     * When no compartments have been explicitly entered, the context's
     * compartment will be set to the compartment of the "default compartment
     * object".
     */
  private:
    JSObject *defaultCompartmentObject_;
  public:
    inline void setDefaultCompartmentObject(JSObject *obj);
    inline void setDefaultCompartmentObjectIfUnset(JSObject *obj);
    JSObject *maybeDefaultCompartmentObject() const { return defaultCompartmentObject_; }

    /*
     * Current global. This is only safe to use within the scope of the
     * AutoCompartment from which it's called.
     */
    inline js::Handle<js::GlobalObject*> global() const;

    /* Wrap cx->exception for the current compartment. */
    void wrapPendingException();

    /* State for object and array toSource conversion. */
    js::ObjectSet       cycleDetectorSet;

    /* Per-context optional error reporter. */
    JSErrorReporter     errorReporter;

    /* Branch callback. */
    JSOperationCallback operationCallback;

    /* Client opaque pointers. */
    void                *data;
    void                *data2;

    inline js::RegExpStatics *regExpStatics();

  public:

    /*
     * Return:
     * - The newest scripted frame's version, if there is such a frame.
     * - The version from the compartment.
     * - The default version.
     *
     * Note: if this ever shows up in a profile, just add caching!
     */
    JSVersion findVersion() const;

    void setOptions(unsigned opts) {
        JS_ASSERT((opts & JSOPTION_MASK) == opts);
        options_ = opts;
    }

    unsigned options() const { return options_; }

    bool hasOption(unsigned opt) const {
        JS_ASSERT((opt & JSOPTION_MASK) == opt);
        return !!(options_ & opt);
    }

    bool hasExtraWarningsOption() const { return hasOption(JSOPTION_EXTRA_WARNINGS); }
    bool hasWErrorOption() const { return hasOption(JSOPTION_WERROR); }

    js::LifoAlloc &tempLifoAlloc() { return runtime()->tempLifoAlloc; }
    inline js::LifoAlloc &analysisLifoAlloc();
    inline js::LifoAlloc &typeLifoAlloc();

    inline js::PropertyTree &propertyTree();

#ifdef JS_THREADSAFE
    unsigned            outstandingRequests;/* number of JS_BeginRequest calls
                                               without the corresponding
                                               JS_EndRequest. */
#endif

    /* Stored here to avoid passing it around as a parameter. */
    unsigned               resolveFlags;

    /* Location to stash the iteration value between JSOP_MOREITER and JSOP_ITERNEXT. */
    js::Value           iterValue;

    bool jitIsBroken;

    inline bool typeInferenceEnabled() const;

    void updateJITEnabled();

    /* Whether this context has JS frames on the stack. */
    bool currentlyRunning() const;

    bool currentlyRunningInInterpreter() const {
        return mainThread().activation()->isInterpreter();
    }
    bool currentlyRunningInJit() const {
        return mainThread().activation()->isJit();
    }
    js::StackFrame *interpreterFrame() const {
        return mainThread().activation()->asInterpreter()->current();
    }
    js::FrameRegs &interpreterRegs() const {
        return mainThread().activation()->asInterpreter()->regs();
    }

    /*
     * Get the topmost script and optional pc on the stack. By default, this
     * function only returns a JSScript in the current compartment, returning
     * NULL if the current script is in a different compartment. This behavior
     * can be overridden by passing ALLOW_CROSS_COMPARTMENT.
     */
    enum MaybeAllowCrossCompartment {
        DONT_ALLOW_CROSS_COMPARTMENT = false,
        ALLOW_CROSS_COMPARTMENT = true
    };
    inline JSScript *currentScript(jsbytecode **pc = NULL,
                                   MaybeAllowCrossCompartment = DONT_ALLOW_CROSS_COMPARTMENT) const;

#ifdef MOZ_TRACE_JSCALLS
    /* Function entry/exit debugging callback. */
    JSFunctionCallback    functionCallback;

    void doFunctionCallback(const JSFunction *fun,
                            const JSScript *scr,
                            int entering) const
    {
        if (functionCallback)
            functionCallback(fun, scr, this, entering);
    }
#endif

  private:
    /* Innermost-executing generator or null if no generator are executing. */
    JSGenerator *innermostGenerator_;
  public:
    JSGenerator *innermostGenerator() const { return innermostGenerator_; }
    void enterGenerator(JSGenerator *gen);
    void leaveGenerator(JSGenerator *gen);

    void *onOutOfMemory(void *p, size_t nbytes) {
        return runtime()->onOutOfMemory(p, nbytes, this);
    }
    void updateMallocCounter(size_t nbytes) {
        runtime()->updateMallocCounter(zone(), nbytes);
    }
    void reportAllocationOverflow() {
        js_ReportAllocationOverflow(this);
    }

    bool isExceptionPending() {
        return throwing;
    }

    js::Value getPendingException() {
        JS_ASSERT(throwing);
        return exception;
    }

    void setPendingException(js::Value v);

    void clearPendingException() {
        throwing = false;
        exception.setUndefined();
    }

#ifdef DEBUG
    /*
     * Controls whether a quadratic-complexity assertion is performed during
     * stack iteration; defaults to true.
     */
    bool stackIterAssertionEnabled;
#endif

    /*
     * See JS_SetTrustedPrincipals in jsapi.h.
     * Note: !cx->compartment is treated as trusted.
     */
    bool runningWithTrustedPrincipals() const;

    JS_FRIEND_API(size_t) sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    void mark(JSTracer *trc);

  private:
    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    JS_FRIEND_API(void) checkMallocGCPressure(void *p);
}; /* struct JSContext */

namespace js {

struct AutoResolving {
  public:
    enum Kind {
        LOOKUP,
        WATCH
    };

    AutoResolving(JSContext *cx, HandleObject obj, HandleId id, Kind kind = LOOKUP
                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : context(cx), object(obj), id(id), kind(kind), link(cx->resolvingList)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(obj);
        cx->resolvingList = this;
    }

    ~AutoResolving() {
        JS_ASSERT(context->resolvingList == this);
        context->resolvingList = link;
    }

    bool alreadyStarted() const {
        return link && alreadyStartedSlow();
    }

  private:
    bool alreadyStartedSlow() const;

    JSContext           *const context;
    HandleObject        object;
    HandleId            id;
    Kind                const kind;
    AutoResolving       *const link;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

class JSAutoResolveFlags
{
  public:
    JSAutoResolveFlags(JSContext *cx, unsigned flags
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mContext(cx), mSaved(cx->resolveFlags)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        cx->resolveFlags = flags;
    }

    ~JSAutoResolveFlags() { mContext->resolveFlags = mSaved; }

  private:
    JSContext *mContext;
    unsigned mSaved;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

namespace js {

/*
 * Enumerate all contexts in a runtime.
 */
class ContextIter {
    JSContext *iter;

public:
    explicit ContextIter(JSRuntime *rt) {
        iter = rt->contextList.getFirst();
    }

    bool done() const {
        return !iter;
    }

    void next() {
        JS_ASSERT(!done());
        iter = iter->getNext();
    }

    JSContext *get() const {
        JS_ASSERT(!done());
        return iter;
    }

    operator JSContext *() const {
        return get();
    }

    JSContext *operator ->() const {
        return get();
    }
};

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext *
NewContext(JSRuntime *rt, size_t stackChunkSize);

enum DestroyContextMode {
    DCM_NO_GC,
    DCM_FORCE_GC,
    DCM_NEW_FAILED
};

extern void
DestroyContext(JSContext *cx, DestroyContextMode mode);

enum ErrorArgumentsType {
    ArgumentsAreUnicode,
    ArgumentsAreASCII
};

} /* namespace js */

#ifdef va_start
extern JSBool
js_ReportErrorVA(JSContext *cx, unsigned flags, const char *format, va_list ap);

extern JSBool
js_ReportErrorNumberVA(JSContext *cx, unsigned flags, JSErrorCallback callback,
                       void *userRef, const unsigned errorNumber,
                       js::ErrorArgumentsType argumentsType, va_list ap);

extern bool
js_ReportErrorNumberUCArray(JSContext *cx, unsigned flags, JSErrorCallback callback,
                            void *userRef, const unsigned errorNumber,
                            const jschar **args);

extern JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
                        void *userRef, const unsigned errorNumber,
                        char **message, JSErrorReport *reportp,
                        js::ErrorArgumentsType argumentsType, va_list ap);
#endif

namespace js {

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
extern void
ReportUsageError(JSContext *cx, HandleObject callee, const char *msg);

/*
 * Prints a full report and returns true if the given report is non-NULL and
 * the report doesn't have the JSREPORT_WARNING flag set or reportWarnings is
 * true.
 * Returns false otherwise, printing just the message if the report is NULL.
 */
extern bool
PrintError(JSContext *cx, FILE *file, const char *message, JSErrorReport *report,
           bool reportWarnings);
} /* namespace js */

/*
 * Report an exception using a previously composed JSErrorReport.
 * XXXbe remove from "friend" API
 */
extern JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *report);

extern void
js_ReportIsNotDefined(JSContext *cx, const char *name);

/*
 * Report an attempt to access the property of a null or undefined value (v).
 */
extern JSBool
js_ReportIsNullOrUndefined(JSContext *cx, int spindex, js::HandleValue v,
                           js::HandleString fallback);

extern void
js_ReportMissingArg(JSContext *cx, js::HandleValue v, unsigned arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern JSBool
js_ReportValueErrorFlags(JSContext *cx, unsigned flags, const unsigned errorNumber,
                         int spindex, js::HandleValue v, js::HandleString fallback,
                         const char *arg1, const char *arg2);

#define js_ReportValueError(cx,errorNumber,spindex,v,fallback)                \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, NULL, NULL))

#define js_ReportValueError2(cx,errorNumber,spindex,v,fallback,arg1)          \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, NULL))

#define js_ReportValueError3(cx,errorNumber,spindex,v,fallback,arg1,arg2)     \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, arg2))

extern const JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

#ifdef JS_THREADSAFE
# define JS_ASSERT_REQUEST_DEPTH(cx)  JS_ASSERT((cx)->runtime()->requestDepth >= 1)
#else
# define JS_ASSERT_REQUEST_DEPTH(cx)  ((void) 0)
#endif

/*
 * Invoke the operation callback and return false if the current execution
 * is to be terminated.
 */
extern JSBool
js_InvokeOperationCallback(JSContext *cx);

extern JSBool
js_HandleExecutionInterrupt(JSContext *cx);

/*
 * If the operation callback flag was set, call the operation callback.
 * This macro can run the full GC. Return true if it is OK to continue and
 * false otherwise.
 */
static MOZ_ALWAYS_INLINE bool
JS_CHECK_OPERATION_LIMIT(JSContext *cx)
{
    JS_ASSERT_REQUEST_DEPTH(cx);
    return !cx->runtime()->interrupt || js_InvokeOperationCallback(cx);
}

namespace js {

/************************************************************************/

class AutoStringVector : public AutoVectorRooter<JSString *>
{
  public:
    explicit AutoStringVector(JSContext *cx
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<JSString *>(cx, STRINGVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoShapeVector : public AutoVectorRooter<Shape *>
{
  public:
    explicit AutoShapeVector(JSContext *cx
                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<Shape *>(cx, SHAPEVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoValueArray : public AutoGCRooter
{
    Value *start_;
    unsigned length_;
    SkipRoot skip;

  public:
    AutoValueArray(JSContext *cx, Value *start, unsigned length
                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, VALARRAY), start_(start), length_(length), skip(cx, start, length)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    Value *start() { return start_; }
    unsigned length() const { return length_; }

    MutableHandleValue handleAt(unsigned i)
    {
        JS_ASSERT(i < length_);
        return MutableHandleValue::fromMarkedLocation(&start_[i]);
    }
    HandleValue handleAt(unsigned i) const
    {
        JS_ASSERT(i < length_);
        return HandleValue::fromMarkedLocation(&start_[i]);
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoObjectObjectHashMap : public AutoHashMapRooter<JSObject *, JSObject *>
{
  public:
    explicit AutoObjectObjectHashMap(JSContext *cx
                                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoHashMapRooter<JSObject *, JSObject *>(cx, OBJOBJHASHMAP)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoObjectUnsigned32HashMap : public AutoHashMapRooter<JSObject *, uint32_t>
{
  public:
    explicit AutoObjectUnsigned32HashMap(JSContext *cx
                                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoHashMapRooter<JSObject *, uint32_t>(cx, OBJU32HASHMAP)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoObjectHashSet : public AutoHashSetRooter<JSObject *>
{
  public:
    explicit AutoObjectHashSet(JSContext *cx
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoHashSetRooter<JSObject *>(cx, OBJHASHSET)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoAssertNoException
{
#ifdef DEBUG
    JSContext *cx;
    bool hadException;
#endif

  public:
    AutoAssertNoException(JSContext *cx)
#ifdef DEBUG
      : cx(cx),
        hadException(cx->isExceptionPending())
#endif
    {
    }

    ~AutoAssertNoException()
    {
        JS_ASSERT_IF(!hadException, !cx->isExceptionPending());
    }
};

/*
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ContextAllocPolicy
{
    JSContext *const cx_;

  public:
    ContextAllocPolicy(JSContext *cx) : cx_(cx) {}
    JSContext *context() const { return cx_; }
    void *malloc_(size_t bytes) { return cx_->malloc_(bytes); }
    void *calloc_(size_t bytes) { return cx_->calloc_(bytes); }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) { return cx_->realloc_(p, oldBytes, bytes); }
    void free_(void *p) { js_free(p); }
    void reportAllocOverflow() const { js_ReportAllocationOverflow(cx_); }
};

/* Exposed intrinsics so that Ion may inline them. */
JSBool intrinsic_ToObject(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_IsCallable(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_ThrowError(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_NewDenseArray(JSContext *cx, unsigned argc, Value *vp);

JSBool intrinsic_UnsafePutElements(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_UnsafeSetReservedSlot(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_UnsafeGetReservedSlot(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_NewObjectWithClassPrototype(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_HaveSameClass(JSContext *cx, unsigned argc, Value *vp);

JSBool intrinsic_ShouldForceSequential(JSContext *cx, unsigned argc, Value *vp);
JSBool intrinsic_NewParallelArray(JSContext *cx, unsigned argc, Value *vp);

#ifdef DEBUG
JSBool intrinsic_Dump(JSContext *cx, unsigned argc, Value *vp);
#endif

} /* namespace js */

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

#endif /* jscntxt_h */
