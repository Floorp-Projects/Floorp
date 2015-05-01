/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TracingAPI_h
#define js_TracingAPI_h

#include "jsalloc.h"
#include "jspubtd.h"

#include "js/HashTable.h"

class JS_PUBLIC_API(JSTracer);

namespace JS {
class JS_PUBLIC_API(CallbackTracer);
template <typename T> class Heap;
template <typename T> class TenuredHeap;
}

// When tracing a thing, the GC needs to know about the layout of the object it
// is looking at. There are a fixed number of different layouts that the GC
// knows about. The "trace kind" is a static map which tells which layout a GC
// thing has.
//
// Although this map is public, the details are completely hidden. Not all of
// the matching C++ types are exposed, and those that are, are opaque.
//
// See Value::gcKind() and JSTraceCallback in Tracer.h for more details.
enum JSGCTraceKind
{
    // These trace kinds have a publicly exposed, although opaque, C++ type.
    // Note: The order here is determined by our Value packing. Other users
    //       should sort alphabetically, for consistency.
    JSTRACE_OBJECT = 0x00,
    JSTRACE_STRING = 0x01,
    JSTRACE_SYMBOL = 0x02,
    JSTRACE_SCRIPT = 0x03,

    // Shape details are exposed through JS_TraceShapeCycleCollectorChildren.
    JSTRACE_SHAPE = 0x04,

    // ObjectGroup details are exposed through JS_TraceObjectGroupCycleCollectorChildren.
    JSTRACE_OBJECT_GROUP = 0x05,

    // The kind associated with a nullptr.
    JSTRACE_NULL = 0x06,

    // A kind that indicates the real kind should be looked up in the arena.
    JSTRACE_OUTOFLINE = 0x07,

    // The following kinds do not have an exposed C++ idiom.
    JSTRACE_BASE_SHAPE = 0x0F,
    JSTRACE_JITCODE = 0x1F,
    JSTRACE_LAZY_SCRIPT = 0x2F,

    JSTRACE_LAST = JSTRACE_OBJECT_GROUP
};

namespace JS {
// Returns a static string equivalent of |kind|.
JS_FRIEND_API(const char*)
GCTraceKindToAscii(JSGCTraceKind kind);
}

// Tracer callback, called for each traceable thing directly referenced by a
// particular object or runtime structure. It is the callback responsibility
// to ensure the traversal of the full object graph via calling eventually
// JS_TraceChildren on the passed thing. In this case the callback must be
// prepared to deal with cycles in the traversal graph.
//
// kind argument is one of JSTRACE_OBJECT, JSTRACE_STRING or a tag denoting
// internal implementation-specific traversal kind. In the latter case the only
// operations on thing that the callback can do is to call JS_TraceChildren or
// JS_GetTraceThingInfo.
//
// If eagerlyTraceWeakMaps is true, when we trace a WeakMap visit all
// of its mappings. This should be used in cases where the tracer
// wants to use the existing liveness of entries.
typedef void
(* JSTraceCallback)(JS::CallbackTracer* trc, void** thingp, JSGCTraceKind kind);

enum WeakMapTraceKind {
    DoNotTraceWeakMaps = 0,
    TraceWeakMapValues = 1,
    TraceWeakMapKeysValues = 2
};

class JS_PUBLIC_API(JSTracer)
{
  public:
    // Return the runtime set on the tracer.
    JSRuntime* runtime() const { return runtime_; }

    // Return the weak map tracing behavior set on this tracer.
    WeakMapTraceKind eagerlyTraceWeakMaps() const { return eagerlyTraceWeakMaps_; }

    // An intermediate state on the road from C to C++ style dispatch.
    enum TracerKindTag {
        MarkingTracer,
        CallbackTracer
    };
    bool isMarkingTracer() const { return tag_ == MarkingTracer; }
    bool isCallbackTracer() const { return tag_ == CallbackTracer; }
    inline JS::CallbackTracer* asCallbackTracer();

  protected:
    JSTracer(JSRuntime* rt, TracerKindTag tag,
             WeakMapTraceKind weakTraceKind = TraceWeakMapValues)
      : runtime_(rt), tag_(tag), eagerlyTraceWeakMaps_(weakTraceKind)
    {}

  private:
    JSRuntime*          runtime_;
    TracerKindTag       tag_;
    WeakMapTraceKind    eagerlyTraceWeakMaps_;
};

namespace JS {

class AutoTracingName;
class AutoTracingIndex;
class AutoTracingCallback;
class AutoOriginalTraceLocation;

class JS_PUBLIC_API(CallbackTracer) : public JSTracer
{
  public:
    CallbackTracer(JSRuntime* rt, JSTraceCallback traceCallback,
                   WeakMapTraceKind weakTraceKind = TraceWeakMapValues)
      : JSTracer(rt, JSTracer::CallbackTracer, weakTraceKind), callback(traceCallback),
        contextName_(nullptr), contextIndex_(InvalidIndex), contextFunctor_(nullptr),
        contextRealLocation_(nullptr)
    {}

    // Update the trace callback.
    void setTraceCallback(JSTraceCallback traceCallback) {
        callback = traceCallback;
    }

    // Test if the given callback is the same as our callback.
    bool hasCallback(JSTraceCallback maybeCallback) const {
        return maybeCallback == callback;
    }

    // Call the callback.
    void invoke(void** thing, JSGCTraceKind kind) {
        callback(this, thing, kind);
    }

    // Access to the tracing context:
    // When tracing with a JS::CallbackTracer, we invoke the callback with the
    // edge location and the type of target. This is useful for operating on
    // the edge in the abstract or on the target thing, satisfying most common
    // use cases.  However, some tracers need additional detail about the
    // specific edge that is being traced in order to be useful. Unfortunately,
    // the raw pointer to the edge that we provide is not enough information to
    // infer much of anything useful about that edge.
    //
    // In order to better support use cases that care in particular about edges
    // -- as opposed to the target thing -- tracing implementations are
    // responsible for providing extra context information about each edge they
    // trace, as it is traced. This contains, at a minimum, an edge name and,
    // when tracing an array, the index. Further specialization can be achived
    // (with some complexity), by associating a functor with the tracer so
    // that, when requested, the user can generate totally custom edge
    // descriptions.

    // Returns the current edge's name. It is only valid to call this when
    // inside the trace callback, however, the edge name will always be set.
    const char* contextName() const { MOZ_ASSERT(contextName_); return contextName_; }

    // Returns the current edge's index, if marked as part of an array of edges.
    // This must be called only inside the trace callback. When not tracing an
    // array, the value will be InvalidIndex.
    const static size_t InvalidIndex = size_t(-1);
    size_t contextIndex() const { return contextIndex_; }

    // Build a description of this edge in the heap graph. This call may invoke
    // the context functor, if set, which may inspect arbitrary areas of the
    // heap. On the other hand, the description provided by this method may be
    // substantially more accurate and useful than those provided by only the
    // contextName and contextIndex.
    void getTracingEdgeName(char* buffer, size_t bufferSize);

    // The trace implementation may associate a callback with one or more edges
    // using AutoTracingDetails. This functor is called by getTracingEdgeName
    // and is responsible for providing a textual representation of the
    // currently being traced edge. The callback has access to the full heap,
    // including the currently set tracing context.
    class ContextFunctor {
      public:
        virtual void operator()(CallbackTracer* trc, char* buf, size_t bufsize) = 0;
    };

    // Return the original heap tracing location if the raw thingp reference
    // has been moved. This is generally only useful for heap analyses that
    // need to build an accurate model of the heap, and thus is only accurate
    // when built with JS_GC_ZEAL.
    void*const* tracingLocation(void** thingp) {
        return contextRealLocation_ ? contextRealLocation_ : thingp;
    }

  private:
    // Exposed publicly for several callers that need to check if the tracer
    // calling them is of the right type.
    JSTraceCallback callback;

    friend class AutoTracingName;
    const char* contextName_;

    friend class AutoTracingIndex;
    size_t contextIndex_;

    friend class AutoTracingDetails;
    ContextFunctor* contextFunctor_;

    friend class AutoOriginalTraceLocation;
    void*const* contextRealLocation_;
};

// Set the name portion of the tracer's context for the current edge.
class AutoTracingName
{
    CallbackTracer* trc_;
    const char* prior_;

  public:
    AutoTracingName(CallbackTracer* trc, const char* name) : trc_(trc), prior_(trc->contextName_) {
        MOZ_ASSERT(name);
        trc->contextName_ = name;
    }
    ~AutoTracingName() {
        MOZ_ASSERT(trc_->contextName_);
        trc_->contextName_ = prior_;
    }
};

// Set the index portion of the tracer's context for the current range.
class AutoTracingIndex
{
    CallbackTracer* trc_;

  public:
    explicit AutoTracingIndex(JSTracer* trc, size_t initial = 0) : trc_(nullptr) {
        if (trc->isCallbackTracer()) {
            trc_ = trc->asCallbackTracer();
            MOZ_ASSERT(trc_->contextIndex_ == CallbackTracer::InvalidIndex);
            trc_->contextIndex_ = initial;
        }
    }
    ~AutoTracingIndex() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextIndex_ != CallbackTracer::InvalidIndex);
            trc_->contextIndex_ = CallbackTracer::InvalidIndex;
        }
    }

    void operator++() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextIndex_ != CallbackTracer::InvalidIndex);
            ++trc_->contextIndex_;
        }
    }
};

// Set a context callback for the trace callback to use, if it needs a detailed
// edge description.
class AutoTracingDetails
{
    CallbackTracer* trc_;

  public:
    AutoTracingDetails(JSTracer* trc, CallbackTracer::ContextFunctor& func) : trc_(nullptr) {
        if (trc->isCallbackTracer()) {
            trc_ = trc->asCallbackTracer();
            MOZ_ASSERT(trc_->contextFunctor_ == nullptr);
            trc_->contextFunctor_ = &func;
        }
    }
    ~AutoTracingDetails() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextFunctor_);
            trc_->contextFunctor_ = nullptr;
        }
    }
};

// Some dynamic analyses depend on knowing the edge source location as it
// exists in the object graph. When marking some types of things, e.g. Value
// edges, it is necessary to copy into a temporary on the stack. This class
// records the original location if we need to copy the tracee, so that the
// relevant analyses can continue to operate correctly.
class AutoOriginalTraceLocation
{
#ifdef JS_GC_ZEAL
    CallbackTracer* trc_;

  public:
    template <typename T>
    AutoOriginalTraceLocation(JSTracer* trc, T*const* realLocation) : trc_(nullptr) {
        if (trc->isCallbackTracer() && trc->asCallbackTracer()->contextRealLocation_ == nullptr) {
            trc_ = trc->asCallbackTracer();
            trc_->contextRealLocation_ = reinterpret_cast<void*const*>(realLocation);
        }
    }
    ~AutoOriginalTraceLocation() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextRealLocation_);
            trc_->contextRealLocation_ = nullptr;
        }
    }
#else
  public:
    template <typename T>
    AutoOriginalTraceLocation(JSTracer* trc, T*const* realLocation) {}
#endif
};

} // namespace JS

JS::CallbackTracer*
JSTracer::asCallbackTracer()
{
    MOZ_ASSERT(isCallbackTracer());
    return static_cast<JS::CallbackTracer*>(this);
}

// The JS_Call*Tracer family of functions traces the given GC thing reference.
// This performs the tracing action configured on the given JSTracer:
// typically calling the JSTracer::callback or marking the thing as live.
//
// The argument to JS_Call*Tracer is an in-out param: when the function
// returns, the garbage collector might have moved the GC thing. In this case,
// the reference passed to JS_Call*Tracer will be updated to the object's new
// location. Callers of this method are responsible for updating any state
// that is dependent on the object's address. For example, if the object's
// address is used as a key in a hashtable, then the object must be removed
// and re-inserted with the correct hash.
//
extern JS_PUBLIC_API(void)
JS_CallValueTracer(JSTracer* trc, JS::Heap<JS::Value>* valuep, const char* name);

extern JS_PUBLIC_API(void)
JS_CallIdTracer(JSTracer* trc, JS::Heap<jsid>* idp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallObjectTracer(JSTracer* trc, JS::Heap<JSObject*>* objp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallStringTracer(JSTracer* trc, JS::Heap<JSString*>* strp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallScriptTracer(JSTracer* trc, JS::Heap<JSScript*>* scriptp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallFunctionTracer(JSTracer* trc, JS::Heap<JSFunction*>* funp, const char* name);

// The following JS_CallUnbarriered*Tracer functions should only be called where
// you know for sure that a heap post barrier is not required.  Use with extreme
// caution!
extern JS_PUBLIC_API(void)
JS_CallUnbarrieredValueTracer(JSTracer* trc, JS::Value* valuep, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredIdTracer(JSTracer* trc, jsid* idp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredObjectTracer(JSTracer* trc, JSObject** objp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredStringTracer(JSTracer* trc, JSString** strp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredScriptTracer(JSTracer* trc, JSScript** scriptp, const char* name);

template <typename HashSetEnum>
inline void
JS_CallHashSetObjectTracer(JSTracer* trc, HashSetEnum& e, JSObject* const& key, const char* name)
{
    JSObject* updated = key;
    JS::AutoOriginalTraceLocation reloc(trc, &key);
    JS_CallUnbarrieredObjectTracer(trc, &updated, name);
    if (updated != key)
        e.rekeyFront(updated);
}

// Trace an object that is known to always be tenured.  No post barriers are
// required in this case.
extern JS_PUBLIC_API(void)
JS_CallTenuredObjectTracer(JSTracer* trc, JS::TenuredHeap<JSObject*>* objp, const char* name);

extern JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer* trc, void* thing, JSGCTraceKind kind);

extern JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer* trc);

namespace JS {
typedef js::HashSet<Zone*, js::DefaultHasher<Zone*>, js::SystemAllocPolicy> ZoneSet;
}

// Trace every value within |zones| that is wrapped by a cross-compartment
// wrapper from a zone that is not an element of |zones|.
extern JS_PUBLIC_API(void)
JS_TraceIncomingCCWs(JSTracer* trc, const JS::ZoneSet& zones);

extern JS_PUBLIC_API(void)
JS_GetTraceThingInfo(char* buf, size_t bufsize, JSTracer* trc,
                     void* thing, JSGCTraceKind kind, bool includeDetails);

#endif /* js_TracingAPI_h */
