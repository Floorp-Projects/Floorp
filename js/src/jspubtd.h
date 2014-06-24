/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jspubtd_h
#define jspubtd_h

/*
 * JS public API typedefs.
 */

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/NullPtr.h"
#include "mozilla/PodOperations.h"

#include "jsprototypes.h"
#include "jstypes.h"

#include "js/TypeDecls.h"

#if defined(JSGC_USE_EXACT_ROOTING) || defined(JS_DEBUG)
# define JSGC_TRACK_EXACT_ROOTS
#endif

namespace JS {

class AutoIdVector;
class CallArgs;

template <typename T>
class Rooted;

class JS_FRIEND_API(CompileOptions);
class JS_FRIEND_API(ReadOnlyCompileOptions);
class JS_FRIEND_API(OwningCompileOptions);
class JS_PUBLIC_API(CompartmentOptions);

struct Zone;

} /* namespace JS */

namespace js {
struct ContextFriendFields;
} // namespace js

/*
 * Run-time version enumeration.  For compile-time version checking, please use
 * the JS_HAS_* macros in jsversion.h, or use MOZJS_MAJOR_VERSION,
 * MOZJS_MINOR_VERSION, MOZJS_PATCH_VERSION, and MOZJS_ALPHA definitions.
 */
enum JSVersion {
    JSVERSION_ECMA_3  = 148,
    JSVERSION_1_6     = 160,
    JSVERSION_1_7     = 170,
    JSVERSION_1_8     = 180,
    JSVERSION_ECMA_5  = 185,
    JSVERSION_DEFAULT = 0,
    JSVERSION_UNKNOWN = -1,
    JSVERSION_LATEST  = JSVERSION_ECMA_5
};

/* Result of typeof operator enumeration. */
enum JSType {
    JSTYPE_VOID,                /* undefined */
    JSTYPE_OBJECT,              /* object */
    JSTYPE_FUNCTION,            /* function */
    JSTYPE_STRING,              /* string */
    JSTYPE_NUMBER,              /* number */
    JSTYPE_BOOLEAN,             /* boolean */
    JSTYPE_NULL,                /* null */
    JSTYPE_SYMBOL,              /* symbol */
    JSTYPE_LIMIT
};

/* Dense index into cached prototypes and class atoms for standard objects. */
enum JSProtoKey {
#define PROTOKEY_AND_INITIALIZER(name,code,init,clasp) JSProto_##name = code,
    JS_FOR_EACH_PROTOTYPE(PROTOKEY_AND_INITIALIZER)
#undef PROTOKEY_AND_INITIALIZER
    JSProto_LIMIT
};

/*
 * This enum type is used to control the behavior of a JSObject property
 * iterator function that has type JSNewEnumerate.
 */
enum JSIterateOp {
    /* Create new iterator state over enumerable properties. */
    JSENUMERATE_INIT,

    /* Create new iterator state over all properties. */
    JSENUMERATE_INIT_ALL,

    /* Iterate once. */
    JSENUMERATE_NEXT,

    /* Destroy iterator state. */
    JSENUMERATE_DESTROY
};

/* See Value::gcKind() and JSTraceCallback in Tracer.h. */
enum JSGCTraceKind {
    JSTRACE_OBJECT,
    JSTRACE_STRING,
    JSTRACE_SYMBOL,
    JSTRACE_SCRIPT,

    /*
     * Trace kinds internal to the engine. The embedding can only see them if
     * it implements JSTraceCallback.
     */
    JSTRACE_LAZY_SCRIPT,
    JSTRACE_JITCODE,
    JSTRACE_SHAPE,
    JSTRACE_BASE_SHAPE,
    JSTRACE_TYPE_OBJECT,
    JSTRACE_LAST = JSTRACE_TYPE_OBJECT
};

/* Struct forward declarations. */
struct JSClass;
struct JSCompartment;
struct JSConstDoubleSpec;
struct JSCrossCompartmentCall;
struct JSErrorReport;
struct JSExceptionState;
struct JSFunctionSpec;
struct JSIdArray;
struct JSLocaleCallbacks;
struct JSObjectMap;
struct JSPrincipals;
struct JSPropertyDescriptor;
struct JSPropertyName;
struct JSPropertySpec;
struct JSRuntime;
struct JSSecurityCallbacks;
struct JSStructuredCloneCallbacks;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;
class JS_PUBLIC_API(JSTracer);

class JSFlatString;

#ifdef JS_THREADSAFE
typedef struct PRCallOnceType   JSCallOnceType;
#else
typedef bool                    JSCallOnceType;
#endif
typedef bool                    (*JSInitCallback)(void);

/*
 * Generic trace operation that calls JS_CallTracer on each traceable thing
 * stored in data.
 */
typedef void
(* JSTraceDataOp)(JSTracer *trc, void *data);

void js_FinishGC(JSRuntime *rt);

namespace js {
namespace gc {
class StoreBuffer;
void MarkPersistentRootedChains(JSTracer *);
void FinishPersistentRootedChains(JSRuntime *);
}
}

namespace JS {

typedef void (*OffThreadCompileCallback)(void *token, void *callbackData);

namespace shadow {

struct Runtime
{
    /* Restrict zone access during Minor GC. */
    bool needsBarrier_;

#ifdef JSGC_GENERATIONAL
  private:
    js::gc::StoreBuffer *gcStoreBufferPtr_;
#endif

  public:
    explicit Runtime(
#ifdef JSGC_GENERATIONAL
        js::gc::StoreBuffer *storeBuffer
#endif
    )
      : needsBarrier_(false)
#ifdef JSGC_GENERATIONAL
      , gcStoreBufferPtr_(storeBuffer)
#endif
    {}

    bool needsBarrier() const {
        return needsBarrier_;
    }

#ifdef JSGC_GENERATIONAL
    js::gc::StoreBuffer *gcStoreBufferPtr() { return gcStoreBufferPtr_; }
#endif

    static JS::shadow::Runtime *asShadowRuntime(JSRuntime *rt) {
        return reinterpret_cast<JS::shadow::Runtime*>(rt);
    }

    /* Allow inlining of PersistentRooted constructors and destructors. */
  private:
    template <typename Referent> friend class JS::PersistentRooted;
    friend void js::gc::MarkPersistentRootedChains(JSTracer *);
    friend void js::gc::FinishPersistentRootedChains(JSRuntime *rt);

    mozilla::LinkedList<PersistentRootedFunction> functionPersistentRooteds;
    mozilla::LinkedList<PersistentRootedId>       idPersistentRooteds;
    mozilla::LinkedList<PersistentRootedObject>   objectPersistentRooteds;
    mozilla::LinkedList<PersistentRootedScript>   scriptPersistentRooteds;
    mozilla::LinkedList<PersistentRootedString>   stringPersistentRooteds;
    mozilla::LinkedList<PersistentRootedValue>    valuePersistentRooteds;

    /* Specializations of this return references to the appropriate list. */
    template<typename Referent>
    inline mozilla::LinkedList<PersistentRooted<Referent> > &getPersistentRootedList();
};

template<>
inline mozilla::LinkedList<PersistentRootedFunction>
&Runtime::getPersistentRootedList<JSFunction *>() { return functionPersistentRooteds; }

template<>
inline mozilla::LinkedList<PersistentRootedId>
&Runtime::getPersistentRootedList<jsid>() { return idPersistentRooteds; }

template<>
inline mozilla::LinkedList<PersistentRootedObject>
&Runtime::getPersistentRootedList<JSObject *>() { return objectPersistentRooteds; }

template<>
inline mozilla::LinkedList<PersistentRootedScript>
&Runtime::getPersistentRootedList<JSScript *>() { return scriptPersistentRooteds; }

template<>
inline mozilla::LinkedList<PersistentRootedString>
&Runtime::getPersistentRootedList<JSString *>() { return stringPersistentRooteds; }

template<>
inline mozilla::LinkedList<PersistentRootedValue>
&Runtime::getPersistentRootedList<Value>() { return valuePersistentRooteds; }

} /* namespace shadow */

class JS_PUBLIC_API(AutoGCRooter)
{
  public:
    AutoGCRooter(JSContext *cx, ptrdiff_t tag);
    AutoGCRooter(js::ContextFriendFields *cx, ptrdiff_t tag);

    ~AutoGCRooter() {
        MOZ_ASSERT(this == *stackTop);
        *stackTop = down;
    }

    /* Implemented in gc/RootMarking.cpp. */
    inline void trace(JSTracer *trc);
    static void traceAll(JSTracer *trc);
    static void traceAllWrappers(JSTracer *trc);

    /* T must be a context type */
    template<typename T>
    static void traceAllInContext(T* cx, JSTracer *trc) {
        for (AutoGCRooter *gcr = cx->autoGCRooters; gcr; gcr = gcr->down)
            gcr->trace(trc);
    }

  protected:
    AutoGCRooter * const down;

    /*
     * Discriminates actual subclass of this being used.  If non-negative, the
     * subclass roots an array of values of the length stored in this field.
     * If negative, meaning is indicated by the corresponding value in the enum
     * below.  Any other negative value indicates some deeper problem such as
     * memory corruption.
     */
    ptrdiff_t tag_;

    enum {
        VALARRAY =     -2, /* js::AutoValueArray */
        PARSER =       -3, /* js::frontend::Parser */
        SHAPEVECTOR =  -4, /* js::AutoShapeVector */
        IDARRAY =      -6, /* js::AutoIdArray */
        DESCVECTOR =   -7, /* js::AutoPropDescVector */
        VALVECTOR =   -10, /* js::AutoValueVector */
        IDVECTOR =    -13, /* js::AutoIdVector */
        OBJVECTOR =   -14, /* js::AutoObjectVector */
        STRINGVECTOR =-15, /* js::AutoStringVector */
        SCRIPTVECTOR =-16, /* js::AutoScriptVector */
        NAMEVECTOR =  -17, /* js::AutoNameVector */
        HASHABLEVALUE=-18, /* js::HashableValue */
        IONMASM =     -19, /* js::jit::MacroAssembler */
        IONALLOC =    -20, /* js::jit::AutoTempAllocatorRooter */
        WRAPVECTOR =  -21, /* js::AutoWrapperVector */
        WRAPPER =     -22, /* js::AutoWrapperRooter */
        OBJOBJHASHMAP=-23, /* js::AutoObjectObjectHashMap */
        OBJU32HASHMAP=-24, /* js::AutoObjectUnsigned32HashMap */
        OBJHASHSET =  -25, /* js::AutoObjectHashSet */
        JSONPARSER =  -26, /* js::JSONParser */
        CUSTOM =      -27, /* js::CustomAutoRooter */
        FUNVECTOR =   -28  /* js::AutoFunctionVector */
    };

  private:
    AutoGCRooter ** const stackTop;

    /* No copy or assignment semantics. */
    AutoGCRooter(AutoGCRooter &ida) MOZ_DELETE;
    void operator=(AutoGCRooter &ida) MOZ_DELETE;
};

} /* namespace JS */

namespace js {

/*
 * Parallel operations in general can have one of three states. They may
 * succeed, fail, or "bail", where bail indicates that the code encountered an
 * unexpected condition and should be re-run sequentially. Different
 * subcategories of the "bail" state are encoded as variants of TP_RETRY_*.
 */
enum ParallelResult { TP_SUCCESS, TP_RETRY_SEQUENTIALLY, TP_RETRY_AFTER_GC, TP_FATAL };

struct ThreadSafeContext;
class ForkJoinContext;
class ExclusiveContext;

class Allocator;

enum ThingRootKind
{
    THING_ROOT_OBJECT,
    THING_ROOT_SHAPE,
    THING_ROOT_BASE_SHAPE,
    THING_ROOT_TYPE_OBJECT,
    THING_ROOT_STRING,
    THING_ROOT_SYMBOL,
    THING_ROOT_JIT_CODE,
    THING_ROOT_SCRIPT,
    THING_ROOT_LAZY_SCRIPT,
    THING_ROOT_ID,
    THING_ROOT_VALUE,
    THING_ROOT_TYPE,
    THING_ROOT_BINDINGS,
    THING_ROOT_PROPERTY_DESCRIPTOR,
    THING_ROOT_PROP_DESC,
    THING_ROOT_LIMIT
};

/*
 * This list enumerates the different types of conceptual stacks we have in
 * SpiderMonkey. In reality, they all share the C stack, but we allow different
 * stack limits depending on the type of code running.
 */
enum StackKind
{
    StackForSystemCode,      // C++, such as the GC, running on behalf of the VM.
    StackForTrustedScript,   // Script running with trusted principals.
    StackForUntrustedScript, // Script running with untrusted principals.
    StackKindCount
};

template <typename T>
struct RootKind;

/*
 * Specifically mark the ThingRootKind of externally visible types, so that
 * JSAPI users may use JSRooted... types without having the class definition
 * available.
 */
template<typename T, ThingRootKind Kind>
struct SpecificRootKind
{
    static ThingRootKind rootKind() { return Kind; }
};

template <> struct RootKind<JSObject *> : SpecificRootKind<JSObject *, THING_ROOT_OBJECT> {};
template <> struct RootKind<JSFlatString *> : SpecificRootKind<JSFlatString *, THING_ROOT_STRING> {};
template <> struct RootKind<JSFunction *> : SpecificRootKind<JSFunction *, THING_ROOT_OBJECT> {};
template <> struct RootKind<JSString *> : SpecificRootKind<JSString *, THING_ROOT_STRING> {};
template <> struct RootKind<JS::Symbol *> : SpecificRootKind<JS::Symbol *, THING_ROOT_SYMBOL> {};
template <> struct RootKind<JSScript *> : SpecificRootKind<JSScript *, THING_ROOT_SCRIPT> {};
template <> struct RootKind<jsid> : SpecificRootKind<jsid, THING_ROOT_ID> {};
template <> struct RootKind<JS::Value> : SpecificRootKind<JS::Value, THING_ROOT_VALUE> {};

struct ContextFriendFields
{
  protected:
    JSRuntime *const     runtime_;

    /* The current compartment. */
    JSCompartment       *compartment_;

    /* The current zone. */
    JS::Zone            *zone_;

  public:
    explicit ContextFriendFields(JSRuntime *rt)
      : runtime_(rt), compartment_(nullptr), zone_(nullptr), autoGCRooters(nullptr)
    {
#ifdef JSGC_TRACK_EXACT_ROOTS
        mozilla::PodArrayZero(thingGCRooters);
#endif
    }

    static const ContextFriendFields *get(const JSContext *cx) {
        return reinterpret_cast<const ContextFriendFields *>(cx);
    }

    static ContextFriendFields *get(JSContext *cx) {
        return reinterpret_cast<ContextFriendFields *>(cx);
    }

#ifdef JSGC_TRACK_EXACT_ROOTS
  private:
    /*
     * Stack allocated GC roots for stack GC heap pointers, which may be
     * overwritten if moved during a GC.
     */
    JS::Rooted<void*> *thingGCRooters[THING_ROOT_LIMIT];

  public:
    template <class T>
    inline JS::Rooted<T> *gcRooters() {
        js::ThingRootKind kind = RootKind<T>::rootKind();
        return reinterpret_cast<JS::Rooted<T> *>(thingGCRooters[kind]);
    }

#endif

    void checkNoGCRooters();

    /* Stack of thread-stack-allocated GC roots. */
    JS::AutoGCRooter   *autoGCRooters;

    friend JSRuntime *GetRuntime(const JSContext *cx);
    friend JSCompartment *GetContextCompartment(const JSContext *cx);
    friend JS::Zone *GetContextZone(const JSContext *cx);
    template <typename T> friend class JS::Rooted;
};

/*
 * Inlinable accessors for JSContext.
 *
 * - These must not be available on the more restricted superclasses of
 *   JSContext, so we can't simply define them on ContextFriendFields.
 *
 * - They're perfectly ordinary JSContext functionality, so ought to be
 *   usable without resorting to jsfriendapi.h, and when JSContext is an
 *   incomplete type.
 */
inline JSRuntime *
GetRuntime(const JSContext *cx)
{
    return ContextFriendFields::get(cx)->runtime_;
}

inline JSCompartment *
GetContextCompartment(const JSContext *cx)
{
    return ContextFriendFields::get(cx)->compartment_;
}

inline JS::Zone *
GetContextZone(const JSContext *cx)
{
    return ContextFriendFields::get(cx)->zone_;
}

class PerThreadData;

struct PerThreadDataFriendFields
{
  private:
    // Note: this type only exists to permit us to derive the offset of
    // the perThread data within the real JSRuntime* type in a portable
    // way.
    struct RuntimeDummy : JS::shadow::Runtime
    {
        struct PerThreadDummy {
            void *field1;
            uintptr_t field2;
#ifdef JS_DEBUG
            uint64_t field3;
#endif
        } mainThread;
    };

  public:

    PerThreadDataFriendFields();

#ifdef JSGC_TRACK_EXACT_ROOTS
  private:
    /*
     * Stack allocated GC roots for stack GC heap pointers, which may be
     * overwritten if moved during a GC.
     */
    JS::Rooted<void*> *thingGCRooters[THING_ROOT_LIMIT];

  public:
    template <class T>
    inline JS::Rooted<T> *gcRooters() {
        js::ThingRootKind kind = RootKind<T>::rootKind();
        return reinterpret_cast<JS::Rooted<T> *>(thingGCRooters[kind]);
    }
#endif

    /* Limit pointer for checking native stack consumption. */
    uintptr_t nativeStackLimit[StackKindCount];

    static const size_t RuntimeMainThreadOffset = offsetof(RuntimeDummy, mainThread);

    static inline PerThreadDataFriendFields *get(js::PerThreadData *pt) {
        return reinterpret_cast<PerThreadDataFriendFields *>(pt);
    }

    static inline PerThreadDataFriendFields *getMainThread(JSRuntime *rt) {
        // mainThread must always appear directly after |JS::shadow::Runtime|.
        // Tested by a JS_STATIC_ASSERT in |jsfriendapi.cpp|
        return reinterpret_cast<PerThreadDataFriendFields *>(
            reinterpret_cast<char*>(rt) + RuntimeMainThreadOffset);
    }

    static inline const PerThreadDataFriendFields *getMainThread(const JSRuntime *rt) {
        // mainThread must always appear directly after |JS::shadow::Runtime|.
        // Tested by a JS_STATIC_ASSERT in |jsfriendapi.cpp|
        return reinterpret_cast<const PerThreadDataFriendFields *>(
            reinterpret_cast<const char*>(rt) + RuntimeMainThreadOffset);
    }

    template <typename T> friend class JS::Rooted;
};

} /* namespace js */

#endif /* jspubtd_h */
