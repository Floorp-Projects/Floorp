/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TracingAPI_h
#define js_TracingAPI_h

#include "js/AllocPolicy.h"
#include "js/GCTypeMacros.h"
#include "js/HashTable.h"
#include "js/HeapAPI.h"
#include "js/TraceKind.h"

class JS_PUBLIC_API JSTracer;

namespace JS {
class JS_PUBLIC_API CallbackTracer;
template <typename T>
class Heap;
template <typename T>
class TenuredHeap;

/** Returns a static string equivalent of |kind|. */
JS_FRIEND_API const char* GCTraceKindToAscii(JS::TraceKind kind);

/** Returns the base size in bytes of the GC thing of kind |kind|. */
JS_FRIEND_API size_t GCTraceKindSize(JS::TraceKind kind);

}  // namespace JS

enum WeakMapTraceKind {
  /**
   * Do not trace into weak map keys or values during traversal. Users must
   * handle weak maps manually.
   */
  DoNotTraceWeakMaps,

  /**
   * Do true ephemeron marking with a weak key lookup marking phase. This is
   * the default for GCMarker.
   */
  ExpandWeakMaps,

  /**
   * Trace through to all values, irrespective of whether the keys are live
   * or not. Used for non-marking tracers.
   */
  TraceWeakMapValues,

  /**
   * Trace through to all keys and values, irrespective of whether the keys
   * are live or not. Used for non-marking tracers.
   */
  TraceWeakMapKeysValues
};

class JS_PUBLIC_API JSTracer {
 public:
  // Return the runtime set on the tracer.
  JSRuntime* runtime() const { return runtime_; }

  // Return the weak map tracing behavior currently set on this tracer.
  WeakMapTraceKind weakMapAction() const { return weakMapAction_; }

  enum class TracerKindTag {
    // Marking path: a tracer used only for marking liveness of cells, not
    // for moving them. The kind will transition to WeakMarking after
    // everything reachable by regular edges has been marked.
    Marking,

    // A tracer that traverses the graph for the purposes of moving objects
    // from the nursery to the tenured area.
    Tenuring,

    // General-purpose traversal that invokes a callback on each cell.
    // Traversing children is the responsibility of the callback.
    Callback
  };
  bool isMarkingTracer() const { return tag_ == TracerKindTag::Marking; }
  bool isTenuringTracer() const { return tag_ == TracerKindTag::Tenuring; }
  bool isCallbackTracer() const { return tag_ == TracerKindTag::Callback; }
  inline JS::CallbackTracer* asCallbackTracer();
  bool traceWeakEdges() const { return traceWeakEdges_; }
  bool canSkipJsids() const { return canSkipJsids_; }
#ifdef DEBUG
  bool checkEdges() { return checkEdges_; }
#endif

  // Get the current GC number. Only call this method if |isMarkingTracer()|
  // is true.
  uint32_t gcNumberForMarking() const;

 protected:
  JSTracer(JSRuntime* rt, TracerKindTag tag,
           WeakMapTraceKind weakTraceKind = TraceWeakMapValues)
      : runtime_(rt),
        weakMapAction_(weakTraceKind),
        tag_(tag),
        traceWeakEdges_(true),
#ifdef DEBUG
        checkEdges_(true),
#endif
        canSkipJsids_(false) {
  }

  void setTraceWeakEdges(bool value) { traceWeakEdges_ = value; }

#ifdef DEBUG
  // Set whether to check edges are valid in debug builds.
  void setCheckEdges(bool check) { checkEdges_ = check; }
#endif

 private:
  JSRuntime* const runtime_;
  const WeakMapTraceKind weakMapAction_;
  const TracerKindTag tag_;

  // Whether the tracer should trace weak edges. GCMarker sets this to false.
  bool traceWeakEdges_;

#ifdef DEBUG
  bool checkEdges_;
#endif

 protected:
  bool canSkipJsids_;
};

namespace JS {

class AutoTracingName;
class AutoTracingIndex;
class AutoTracingCallback;

class JS_PUBLIC_API CallbackTracer : public JSTracer {
 public:
  CallbackTracer(JSRuntime* rt,
                 WeakMapTraceKind weakTraceKind = TraceWeakMapValues)
      : JSTracer(rt, JSTracer::TracerKindTag::Callback, weakTraceKind),
        contextName_(nullptr),
        contextIndex_(InvalidIndex),
        contextFunctor_(nullptr) {}
  CallbackTracer(JSContext* cx,
                 WeakMapTraceKind weakTraceKind = TraceWeakMapValues);

  // Override these methods to receive notification when an edge is visited
  // with the type contained in the callback. The default implementation
  // dispatches to the fully-generic onChild implementation, so for cases that
  // do not care about boxing overhead and do not need the actual edges,
  // just override the generic onChild.
  virtual bool onObjectEdge(JSObject** objp) {
    return onChild(JS::GCCellPtr(*objp));
  }
  virtual bool onStringEdge(JSString** strp) {
    return onChild(JS::GCCellPtr(*strp));
  }
  virtual bool onSymbolEdge(JS::Symbol** symp) {
    return onChild(JS::GCCellPtr(*symp));
  }
  virtual bool onBigIntEdge(JS::BigInt** bip) {
    return onChild(JS::GCCellPtr(*bip));
  }
  virtual bool onScriptEdge(js::BaseScript** scriptp) {
    return onChild(JS::GCCellPtr(*scriptp));
  }
  virtual bool onShapeEdge(js::Shape** shapep) {
    return onChild(JS::GCCellPtr(*shapep, JS::TraceKind::Shape));
  }
  virtual bool onObjectGroupEdge(js::ObjectGroup** groupp) {
    return onChild(JS::GCCellPtr(*groupp, JS::TraceKind::ObjectGroup));
  }
  virtual bool onBaseShapeEdge(js::BaseShape** basep) {
    return onChild(JS::GCCellPtr(*basep, JS::TraceKind::BaseShape));
  }
  virtual bool onJitCodeEdge(js::jit::JitCode** codep) {
    return onChild(JS::GCCellPtr(*codep, JS::TraceKind::JitCode));
  }
  virtual bool onScopeEdge(js::Scope** scopep) {
    return onChild(JS::GCCellPtr(*scopep, JS::TraceKind::Scope));
  }
  virtual bool onRegExpSharedEdge(js::RegExpShared** sharedp) {
    return onChild(JS::GCCellPtr(*sharedp, JS::TraceKind::RegExpShared));
  }

  // Override this method to receive notification when a node in the GC
  // heap graph is visited.
  // This method should return whether the edge still exists, i.e. return false
  // if 'thing' is cleared by the CallbackTracer, return true otherwise.
  virtual bool onChild(const JS::GCCellPtr& thing) = 0;

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
  const char* contextName() const {
    MOZ_ASSERT(contextName_);
    return contextName_;
  }

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

#ifdef DEBUG
  enum class TracerKind {
    Unspecified,
    Moving,
    GrayBuffering,
    VerifyTraceProtoAndIface,
    ClearEdges,
    UnmarkGray,
    Sweeping
  };
  virtual TracerKind getTracerKind() const { return TracerKind::Unspecified; }
#endif

  // In C++, overriding a method hides all methods in the base class with
  // that name, not just methods with that signature. Thus, the typed edge
  // methods have to have distinct names to allow us to override them
  // individually, which is freqently useful if, for example, we only want to
  // process only one type of edge.
  bool dispatchToOnEdge(JSObject** objp) { return onObjectEdge(objp); }
  bool dispatchToOnEdge(JSString** strp) { return onStringEdge(strp); }
  bool dispatchToOnEdge(JS::Symbol** symp) { return onSymbolEdge(symp); }
  bool dispatchToOnEdge(JS::BigInt** bip) { return onBigIntEdge(bip); }
  bool dispatchToOnEdge(js::BaseScript** scriptp) {
    return onScriptEdge(scriptp);
  }
  bool dispatchToOnEdge(js::Shape** shapep) { return onShapeEdge(shapep); }
  bool dispatchToOnEdge(js::ObjectGroup** groupp) {
    return onObjectGroupEdge(groupp);
  }
  bool dispatchToOnEdge(js::BaseShape** basep) {
    return onBaseShapeEdge(basep);
  }
  bool dispatchToOnEdge(js::jit::JitCode** codep) {
    return onJitCodeEdge(codep);
  }
  bool dispatchToOnEdge(js::Scope** scopep) { return onScopeEdge(scopep); }
  bool dispatchToOnEdge(js::RegExpShared** sharedp) {
    return onRegExpSharedEdge(sharedp);
  }

 protected:
  // If this is set to false, then the tracer will skip some jsids
  // to improve performance. This is needed for the cycle collector.
  void setCanSkipJsids(bool value) { canSkipJsids_ = value; }

 private:
  friend class AutoTracingName;
  const char* contextName_;

  friend class AutoTracingIndex;
  size_t contextIndex_;

  friend class AutoTracingDetails;
  ContextFunctor* contextFunctor_;
};

// Set the name portion of the tracer's context for the current edge.
class MOZ_RAII AutoTracingName {
  CallbackTracer* trc_;
  const char* prior_;

 public:
  AutoTracingName(CallbackTracer* trc, const char* name)
      : trc_(trc), prior_(trc->contextName_) {
    MOZ_ASSERT(name);
    trc->contextName_ = name;
  }
  ~AutoTracingName() {
    MOZ_ASSERT(trc_->contextName_);
    trc_->contextName_ = prior_;
  }
};

// Set the index portion of the tracer's context for the current range.
class MOZ_RAII AutoTracingIndex {
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
class MOZ_RAII AutoTracingDetails {
  CallbackTracer* trc_;

 public:
  AutoTracingDetails(JSTracer* trc, CallbackTracer::ContextFunctor& func)
      : trc_(nullptr) {
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

}  // namespace JS

JS::CallbackTracer* JSTracer::asCallbackTracer() {
  MOZ_ASSERT(isCallbackTracer());
  return static_cast<JS::CallbackTracer*>(this);
}

namespace js {

class AbstractGeneratorObject;
class SavedFrame;

namespace gc {

#define JS_DECLARE_TRACE_EXTERNAL_EDGE(type)                               \
  extern JS_PUBLIC_API void TraceExternalEdge(JSTracer* trc, type* thingp, \
                                              const char* name);

// Declare edge-tracing function overloads for public GC pointer types.
JS_FOR_EACH_PUBLIC_GC_POINTER_TYPE(JS_DECLARE_TRACE_EXTERNAL_EDGE)
JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(JS_DECLARE_TRACE_EXTERNAL_EDGE)

#undef JS_DECLARE_TRACE_EXTERNAL_EDGE

}  // namespace gc
}  // namespace js

namespace JS {

// The JS::TraceEdge family of functions traces the given GC thing reference.
// This performs the tracing action configured on the given JSTracer: typically
// calling the JSTracer::callback or marking the thing as live.
//
// The argument to JS::TraceEdge is an in-out param: when the function returns,
// the garbage collector might have moved the GC thing. In this case, the
// reference passed to JS::TraceEdge will be updated to the thing's new
// location. Callers of this method are responsible for updating any state that
// is dependent on the object's address. For example, if the object's address
// is used as a key in a hashtable, then the object must be removed and
// re-inserted with the correct hash.
//
// Note that while |edgep| must never be null, it is fine for |*edgep| to be
// nullptr.

template <typename T>
inline void TraceEdge(JSTracer* trc, JS::Heap<T>* thingp, const char* name) {
  MOZ_ASSERT(thingp);
  if (*thingp) {
    js::gc::TraceExternalEdge(trc, thingp->unsafeGet(), name);
  }
}

template <typename T>
inline void TraceEdge(JSTracer* trc, JS::TenuredHeap<T>* thingp,
                      const char* name) {
  MOZ_ASSERT(thingp);
  if (T ptr = thingp->unbarrieredGetPtr()) {
    js::gc::TraceExternalEdge(trc, &ptr, name);
    thingp->setPtr(ptr);
  }
}

// Edges that are always traced as part of root marking do not require
// incremental barriers. |JS::UnsafeTraceRoot| overloads allow for marking
// non-barriered pointers but assert that this happens during root marking.
//
// Note that while |edgep| must never be null, it is fine for |*edgep| to be
// nullptr.
#define JS_DECLARE_UNSAFE_TRACE_ROOT(type)                              \
  extern JS_PUBLIC_API void UnsafeTraceRoot(JSTracer* trc, type* edgep, \
                                            const char* name);

// Declare edge-tracing function overloads for public GC pointer types.
JS_FOR_EACH_PUBLIC_GC_POINTER_TYPE(JS_DECLARE_UNSAFE_TRACE_ROOT)
JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(JS_DECLARE_UNSAFE_TRACE_ROOT)

// We also require overloads for these purely-internal types.  These overloads
// ought not be in public headers, and they should use a different name in order
// to not be *actual* overloads, but for the moment we still declare them here.
JS_DECLARE_UNSAFE_TRACE_ROOT(js::AbstractGeneratorObject*)
JS_DECLARE_UNSAFE_TRACE_ROOT(js::SavedFrame*)

#undef JS_DECLARE_UNSAFE_TRACE_ROOT

extern JS_PUBLIC_API void TraceChildren(JSTracer* trc, GCCellPtr thing);

using ZoneSet =
    js::HashSet<Zone*, js::DefaultHasher<Zone*>, js::SystemAllocPolicy>;
using CompartmentSet =
    js::HashSet<JS::Compartment*, js::DefaultHasher<JS::Compartment*>,
                js::SystemAllocPolicy>;

/**
 * Trace every value within |compartments| that is wrapped by a
 * cross-compartment wrapper from a compartment that is not an element of
 * |compartments|.
 */
extern JS_PUBLIC_API void TraceIncomingCCWs(
    JSTracer* trc, const JS::CompartmentSet& compartments);

}  // namespace JS

extern JS_PUBLIC_API void JS_GetTraceThingInfo(char* buf, size_t bufsize,
                                               JSTracer* trc, void* thing,
                                               JS::TraceKind kind,
                                               bool includeDetails);

namespace js {

// Trace an edge that is not a GC root and is not wrapped in a barriered
// wrapper for some reason.
//
// This method does not check if |*edgep| is non-null before tracing through
// it, so callers must check any nullable pointer before calling this method.
extern JS_PUBLIC_API void UnsafeTraceManuallyBarrieredEdge(JSTracer* trc,
                                                           JSObject** edgep,
                                                           const char* name);

// Not part of the public API, but declared here so we can use it in
// GCPolicyAPI.h
template <typename T>
inline bool TraceManuallyBarrieredWeakEdge(JSTracer* trc, T* thingp,
                                           const char* name);

template <typename T>
class BarrieredBase;

template <typename T>
inline bool TraceWeakEdge(JSTracer* trc, BarrieredBase<T>* thingp,
                          const char* name);

namespace gc {

// Return true if the given edge is not live and is about to be swept.
template <typename T>
extern JS_PUBLIC_API bool EdgeNeedsSweep(JS::Heap<T>* edgep);

// Not part of the public API, but declared here so we can use it in GCPolicy
// which is.
template <typename T>
bool IsAboutToBeFinalizedUnbarriered(T* thingp);

}  // namespace gc

#ifdef DEBUG
/*
 * Return whether the runtime is currently being destroyed, for use in
 * assertions.
 */
extern JS_FRIEND_API bool RuntimeIsBeingDestroyed();
#endif

}  // namespace js

#endif /* js_TracingAPI_h */
