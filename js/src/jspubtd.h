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

#include "js/TraceKind.h"
#include "js/TypeDecls.h"

#if defined(JS_GC_ZEAL) || defined(DEBUG)
# define JSGC_HASH_TABLE_CHECKS
#endif

namespace JS {

template <typename T>
class AutoVectorRooter;
typedef AutoVectorRooter<jsid> AutoIdVector;
class CallArgs;

template <typename T>
class Rooted;

class JS_FRIEND_API(CompileOptions);
class JS_FRIEND_API(ReadOnlyCompileOptions);
class JS_FRIEND_API(OwningCompileOptions);
class JS_FRIEND_API(TransitiveCompileOptions);
class JS_PUBLIC_API(CompartmentOptions);

struct RootingContext;
class Value;
struct Zone;

} /* namespace JS */

namespace js {
class RootLists;
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
} // namespace js

namespace JS {

struct PropertyDescriptor;

typedef void (*OffThreadCompileCallback)(void* token, void* callbackData);

enum class HeapState {
    Idle,             // doing nothing with the GC heap
    Tracing,          // tracing the GC heap without collecting, e.g. IterateCompartments()
    MajorCollecting,  // doing a GC of the major heap
    MinorCollecting   // doing a GC of the minor heap (nursery)
};

namespace shadow {

struct Runtime
{
  protected:
    // Allow inlining of heapState checks.
    friend class js::gc::AutoTraceSession;
    JS::HeapState heapState_;

    js::gc::StoreBuffer* gcStoreBufferPtr_;

  public:
    Runtime()
      : heapState_(JS::HeapState::Idle)
      , gcStoreBufferPtr_(nullptr)
    {}

    bool isHeapBusy() const { return heapState_ != JS::HeapState::Idle; }
    bool isHeapMajorCollecting() const { return heapState_ == JS::HeapState::MajorCollecting; }
    bool isHeapMinorCollecting() const { return heapState_ == JS::HeapState::MinorCollecting; }
    bool isHeapCollecting() const { return isHeapMinorCollecting() || isHeapMajorCollecting(); }

    js::gc::StoreBuffer* gcStoreBufferPtr() { return gcStoreBufferPtr_; }

    static JS::shadow::Runtime* asShadowRuntime(JSRuntime* rt) {
        return reinterpret_cast<JS::shadow::Runtime*>(rt);
    }

  protected:
    void setGCStoreBufferPtr(js::gc::StoreBuffer* storeBuffer) {
        gcStoreBufferPtr_ = storeBuffer;
    }
};

} /* namespace shadow */

class JS_PUBLIC_API(AutoGCRooter)
{
  public:
    AutoGCRooter(JSContext* cx, ptrdiff_t tag);
    AutoGCRooter(JS::RootingContext* cx, ptrdiff_t tag);

    ~AutoGCRooter() {
        MOZ_ASSERT(this == *stackTop);
        *stackTop = down;
    }

    /* Implemented in gc/RootMarking.cpp. */
    inline void trace(JSTracer* trc);
    static void traceAll(JSTracer* trc);
    static void traceAllWrappers(JSTracer* trc);

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

} /* namespace JS */

namespace js {

class ExclusiveContext;

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

using RootedListHeads = mozilla::EnumeratedArray<JS::RootKind, JS::RootKind::Limit,
                                                 JS::Rooted<void*>*>;

// Abstracts JS rooting mechanisms so they can be shared between the JSContext
// and JSRuntime.
class RootLists
{
    // Stack GC roots for Rooted GC heap pointers.
    RootedListHeads stackRoots_;
    template <typename T> friend class JS::Rooted;

    // Stack GC roots for AutoFooRooter classes.
    JS::AutoGCRooter* autoGCRooters_;
    friend class JS::AutoGCRooter;

    // Heap GC roots for PersistentRooted pointers.
    mozilla::EnumeratedArray<JS::RootKind, JS::RootKind::Limit,
                             mozilla::LinkedList<JS::PersistentRooted<void*>>> heapRoots_;
    template <typename T> friend class JS::PersistentRooted;

  public:
    RootLists() : autoGCRooters_(nullptr) {
        for (auto& stackRootPtr : stackRoots_)
            stackRootPtr = nullptr;
    }

    ~RootLists() {
        // The semantics of PersistentRooted containing pointers and tagged
        // pointers are somewhat different from those of PersistentRooted
        // containing a structure with a trace method. PersistentRooted
        // containing pointers are allowed to outlive the owning RootLists,
        // whereas those containing a traceable structure are not.
        //
        // The purpose of this feature is to support lazy initialization of
        // global references for the several places in Gecko that do not have
        // access to a tighter context, but that still need to refer to GC
        // pointers. For such pointers, FinishPersistentRootedChains ensures
        // that the contained references are nulled out when the owning
        // RootLists dies to prevent UAF errors.
        //
        // However, for RootKind::Traceable, we do not know the concrete type
        // of the held thing, so we simply cannot do this without accruing
        // extra overhead and complexity for all users for a case that is
        // unlikely to ever be used in practice. For this reason, the following
        // assertion disallows usage of PersistentRooted<Traceable> that
        // outlives the RootLists.
        MOZ_ASSERT(heapRoots_[JS::RootKind::Traceable].isEmpty());
    }

    void traceStackRoots(JSTracer* trc);
    void checkNoGCRooters();

    void tracePersistentRoots(JSTracer* trc);
    void finishPersistentRoots();
};

} // namespace js

namespace JS {

/*
 * JS::RootingContext is a base class of ContextFriendFields and JSContext.
 * This class can be used to let code construct a Rooted<> or PersistentRooted<>
 * instance, without giving it full access to the JSContext.
 */
struct RootingContext
{
    js::RootLists roots;

#ifdef DEBUG
    // Whether the derived class is a JSContext or an ExclusiveContext.
    bool isJSContext;
#endif

    explicit RootingContext(bool isJSContextArg)
#ifdef DEBUG
      : isJSContext(isJSContextArg)
#endif
    {}

    static RootingContext* get(JSContext* cx) {
        return reinterpret_cast<RootingContext*>(cx);
    }
};

} // namespace JS

namespace js {

struct ContextFriendFields : public JS::RootingContext
{
  protected:
    /* The current compartment. */
    JSCompartment*      compartment_;

    /* The current zone. */
    JS::Zone*           zone_;

  public:
    /* Limit pointer for checking native stack consumption. */
    uintptr_t nativeStackLimit[js::StackKindCount];

    explicit ContextFriendFields(bool isJSContext);

    static const ContextFriendFields* get(const JSContext* cx) {
        return reinterpret_cast<const ContextFriendFields*>(cx);
    }

    static ContextFriendFields* get(JSContext* cx) {
        return reinterpret_cast<ContextFriendFields*>(cx);
    }

    friend JSCompartment* GetContextCompartment(const JSContext* cx);
    friend JS::Zone* GetContextZone(const JSContext* cx);
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
inline JSCompartment*
GetContextCompartment(const JSContext* cx)
{
    return ContextFriendFields::get(cx)->compartment_;
}

inline JS::Zone*
GetContextZone(const JSContext* cx)
{
    return ContextFriendFields::get(cx)->zone_;
}

} /* namespace js */

MOZ_BEGIN_EXTERN_C

// Defined in NSPR prio.h.
typedef struct PRFileDesc PRFileDesc;

MOZ_END_EXTERN_C

#endif /* jspubtd_h */
