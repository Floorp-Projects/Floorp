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
#include "mozilla/EnumeratedArray.h"
#include "mozilla/LinkedList.h"
#include "mozilla/PodOperations.h"

#include "jsprototypes.h"
#include "jstypes.h"

#include "js/Result.h"
#include "js/TraceKind.h"
#include "js/TypeDecls.h"

#if defined(JS_GC_ZEAL) || defined(DEBUG)
# define JSGC_HASH_TABLE_CHECKS
#endif

namespace JS {

class AutoIdVector;
class CallArgs;

template <typename T>
class Rooted;

class JS_FRIEND_API(CompileOptions);
class JS_FRIEND_API(ReadOnlyCompileOptions);
class JS_FRIEND_API(OwningCompileOptions);
class JS_FRIEND_API(TransitiveCompileOptions);
class JS_PUBLIC_API(CompartmentOptions);

class Value;
struct Zone;

} // namespace JS

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
    JSTYPE_UNDEFINED,           /* undefined */
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

/* Struct forward declarations. */
struct JSClass;
struct JSCompartment;
struct JSCrossCompartmentCall;
class JSErrorReport;
struct JSExceptionState;
struct JSFunctionSpec;
struct JSLocaleCallbacks;
struct JSObjectMap;
struct JSPrincipals;
struct JSPropertyName;
struct JSPropertySpec;
struct JSRuntime;
struct JSSecurityCallbacks;
struct JSStructuredCloneCallbacks;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;
class JS_PUBLIC_API(JSTracer);

class JSFlatString;

typedef bool                    (*JSInitCallback)(void);

template<typename T> struct JSConstScalarSpec;
typedef JSConstScalarSpec<double> JSConstDoubleSpec;
typedef JSConstScalarSpec<int32_t> JSConstIntegerSpec;

/*
 * Generic trace operation that calls JS::TraceEdge on each traceable thing's
 * location reachable from data.
 */
typedef void
(* JSTraceDataOp)(JSTracer* trc, void* data);

namespace js {
namespace gc {
class AutoTraceSession;
class StoreBuffer;
} // namespace gc

class CooperatingContext;

inline JSCompartment* GetContextCompartment(const JSContext* cx);
inline JS::Zone* GetContextZone(const JSContext* cx);

// Whether the current thread is permitted access to any part of the specified
// runtime or zone.
JS_FRIEND_API(bool)
CurrentThreadCanAccessRuntime(const JSRuntime* rt);

#ifdef DEBUG
JS_FRIEND_API(bool)
CurrentThreadIsPerformingGC();
#endif

} // namespace js

namespace JS {

class JS_PUBLIC_API(AutoEnterCycleCollection);
class JS_PUBLIC_API(AutoAssertOnBarrier);
struct JS_PUBLIC_API(PropertyDescriptor);

typedef void (*OffThreadCompileCallback)(void* token, void* callbackData);

enum class HeapState {
    Idle,             // doing nothing with the GC heap
    Tracing,          // tracing the GC heap without collecting, e.g. IterateCompartments()
    MajorCollecting,  // doing a GC of the major heap
    MinorCollecting,  // doing a GC of the minor heap (nursery)
    CycleCollecting   // in the "Unlink" phase of cycle collection
};

JS_PUBLIC_API(HeapState)
CurrentThreadHeapState();

static inline bool
CurrentThreadIsHeapBusy()
{
    return CurrentThreadHeapState() != HeapState::Idle;
}

static inline bool
CurrentThreadIsHeapTracing()
{
    return CurrentThreadHeapState() == HeapState::Tracing;
}

static inline bool
CurrentThreadIsHeapMajorCollecting()
{
    return CurrentThreadHeapState() == HeapState::MajorCollecting;
}

static inline bool
CurrentThreadIsHeapMinorCollecting()
{
    return CurrentThreadHeapState() == HeapState::MinorCollecting;
}

static inline bool
CurrentThreadIsHeapCollecting()
{
    HeapState state = CurrentThreadHeapState();
    return state == HeapState::MajorCollecting || state == HeapState::MinorCollecting;
}

static inline bool
CurrentThreadIsHeapCycleCollecting()
{
    return CurrentThreadHeapState() == HeapState::CycleCollecting;
}

// Decorates the Unlinking phase of CycleCollection so that accidental use
// of barriered accessors results in assertions instead of leaks.
class MOZ_STACK_CLASS JS_PUBLIC_API(AutoEnterCycleCollection)
{
#ifdef DEBUG
  public:
    explicit AutoEnterCycleCollection(JSRuntime* rt);
    ~AutoEnterCycleCollection();
#else
  public:
    explicit AutoEnterCycleCollection(JSRuntime* rt) {}
    ~AutoEnterCycleCollection() {}
#endif
};

class RootingContext;

class JS_PUBLIC_API(AutoGCRooter)
{
  public:
    AutoGCRooter(JSContext* cx, ptrdiff_t tag);
    AutoGCRooter(RootingContext* cx, ptrdiff_t tag);

    ~AutoGCRooter() {
        MOZ_ASSERT(this == *stackTop);
        *stackTop = down;
    }

    /* Implemented in gc/RootMarking.cpp. */
    inline void trace(JSTracer* trc);
    static void traceAll(const js::CooperatingContext& target, JSTracer* trc);
    static void traceAllWrappers(const js::CooperatingContext& target, JSTracer* trc);

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
        VALVECTOR =   -10, /* js::AutoValueVector */
        IDVECTOR =    -11, /* js::AutoIdVector */
        OBJVECTOR =   -14, /* js::AutoObjectVector */
        IONMASM =     -19, /* js::jit::MacroAssembler */
        WRAPVECTOR =  -20, /* js::AutoWrapperVector */
        WRAPPER =     -21, /* js::AutoWrapperRooter */
        CUSTOM =      -26  /* js::CustomAutoRooter */
    };

    static ptrdiff_t GetTag(const Value& value) { return VALVECTOR; }
    static ptrdiff_t GetTag(const jsid& id) { return IDVECTOR; }
    static ptrdiff_t GetTag(JSObject* obj) { return OBJVECTOR; }

  private:
    AutoGCRooter ** const stackTop;

    /* No copy or assignment semantics. */
    AutoGCRooter(AutoGCRooter& ida) = delete;
    void operator=(AutoGCRooter& ida) = delete;
};

// Our instantiations of Rooted<void*> and PersistentRooted<void*> require an
// instantiation of MapTypeToRootKind.
template <>
struct MapTypeToRootKind<void*> {
    static const RootKind kind = RootKind::Traceable;
};

using RootedListHeads = mozilla::EnumeratedArray<RootKind, RootKind::Limit,
                                                 Rooted<void*>*>;

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

// Superclass of JSContext which can be used for rooting data in use by the
// current thread but that does not provide all the functions of a JSContext.
class RootingContext
{
    // Stack GC roots for Rooted GC heap pointers.
    RootedListHeads stackRoots_;
    template <typename T> friend class JS::Rooted;

    // Stack GC roots for AutoFooRooter classes.
    JS::AutoGCRooter* autoGCRooters_;
    friend class JS::AutoGCRooter;

  public:
    RootingContext();

    void traceStackRoots(JSTracer* trc);
    void checkNoGCRooters();

  protected:
    // The remaining members in this class should only be accessed through
    // JSContext pointers. They are unrelated to rooting and are in place so
    // that inlined API functions can directly access the data.

    /* The current compartment. */
    JSCompartment*      compartment_;

    /* The current zone. */
    JS::Zone*           zone_;

  public:
    /* Limit pointer for checking native stack consumption. */
    uintptr_t nativeStackLimit[StackKindCount];

    static const RootingContext* get(const JSContext* cx) {
        return reinterpret_cast<const RootingContext*>(cx);
    }

    static RootingContext* get(JSContext* cx) {
        return reinterpret_cast<RootingContext*>(cx);
    }

    friend JSCompartment* js::GetContextCompartment(const JSContext* cx);
    friend JS::Zone* js::GetContextZone(const JSContext* cx);
};

} /* namespace JS */

namespace js {

/*
 * Inlinable accessors for JSContext.
 *
 * - These must not be available on the more restricted superclasses of
 *   JSContext, so we can't simply define them on RootingContext.
 *
 * - They're perfectly ordinary JSContext functionality, so ought to be
 *   usable without resorting to jsfriendapi.h, and when JSContext is an
 *   incomplete type.
 */
inline JSCompartment*
GetContextCompartment(const JSContext* cx)
{
    return JS::RootingContext::get(cx)->compartment_;
}

inline JS::Zone*
GetContextZone(const JSContext* cx)
{
    return JS::RootingContext::get(cx)->zone_;
}

} /* namespace js */

MOZ_BEGIN_EXTERN_C

// Defined in NSPR prio.h.
typedef struct PRFileDesc PRFileDesc;

MOZ_END_EXTERN_C

#endif /* jspubtd_h */
