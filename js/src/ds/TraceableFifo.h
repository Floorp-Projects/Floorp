/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TraceableFifo_h
#define js_TraceableFifo_h

#include "ds/Fifo.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"

namespace js {

// A TraceableFifo is a Fifo with an additional trace method that knows how to
// visit all of the items stored in the Fifo. For Fifos that contain GC things,
// this is usually more convenient than manually iterating and marking the
// contents.
//
// Most types of GC pointers as keys and values can be traced with no extra
// infrastructure. For structs and non-gc-pointer members, ensure that there is
// a specialization of DefaultGCPolicy<T> with an appropriate trace method
// available to handle the custom type. Generic helpers can be found in
// js/public/TracingAPI.h. Generic helpers can be found in
// js/public/TracingAPI.h.
//
// Note that although this Fifo's trace will deal correctly with moved items, it
// does not itself know when to barrier or trace items. To function properly it
// must either be used with Rooted, or barriered and traced manually.
template <typename T,
          size_t MinInlineCapacity = 0,
          typename AllocPolicy = TempAllocPolicy>
class TraceableFifo
  : public js::Fifo<T, MinInlineCapacity, AllocPolicy>,
    public JS::Traceable
{
    using Base = js::Fifo<T, MinInlineCapacity, AllocPolicy>;

  public:
    explicit TraceableFifo(AllocPolicy alloc = AllocPolicy()) : Base(alloc) {}

    TraceableFifo(TraceableFifo&& rhs) : Base(mozilla::Move(rhs)) { }
    TraceableFifo& operator=(TraceableFifo&& rhs) { return Base::operator=(mozilla::Move(rhs)); }

    TraceableFifo(const TraceableFifo&) = delete;
    TraceableFifo& operator=(const TraceableFifo&) = delete;

    static void trace(TraceableFifo* tf, JSTracer* trc) {
        for (size_t i = 0; i < tf->front_.length(); ++i)
            DefaultGCPolicy<T>::trace(trc, &tf->front_[i], "fifo element");
        for (size_t i = 0; i < tf->rear_.length(); ++i)
            DefaultGCPolicy<T>::trace(trc, &tf->rear_[i], "fifo element");
    }
};

template <typename Outer, typename T, size_t Capacity, typename AllocPolicy>
class TraceableFifoOperations
{
    using TF = TraceableFifo<T, Capacity, AllocPolicy>;
    const TF& fifo() const { return static_cast<const Outer*>(this)->extract(); }

  public:
    size_t length() const { return fifo().length(); }
    bool empty() const { return fifo().empty(); }
    const T& front() const { return fifo().front(); }
};

template <typename Outer, typename T, size_t Capacity, typename AllocPolicy>
class MutableTraceableFifoOperations
  : public TraceableFifoOperations<Outer, T, Capacity, AllocPolicy>
{
    using TF = TraceableFifo<T, Capacity, AllocPolicy>;
    TF& fifo() { return static_cast<Outer*>(this)->extract(); }

  public:
    T& front() { return fifo().front(); }

    template<typename U> bool pushBack(U&& u) { return fifo().pushBack(mozilla::Forward<U>(u)); }
    template<typename... Args> bool emplaceBack(Args&&... args) {
        return fifo().emplaceBack(mozilla::Forward<Args...>(args...));
    }

    bool popFront() { return fifo().popFront(); }
    void clear() { fifo().clear(); }
};

template <typename A, size_t B, typename C>
class RootedBase<TraceableFifo<A,B,C>>
  : public MutableTraceableFifoOperations<JS::Rooted<TraceableFifo<A,B,C>>, A,B,C>
{
    using TF = TraceableFifo<A,B,C>;

    friend class TraceableFifoOperations<JS::Rooted<TF>, A,B,C>;
    const TF& extract() const { return *static_cast<const JS::Rooted<TF>*>(this)->address(); }

    friend class MutableTraceableFifoOperations<JS::Rooted<TF>, A,B,C>;
    TF& extract() { return *static_cast<JS::Rooted<TF>*>(this)->address(); }
};

template <typename A, size_t B, typename C>
class MutableHandleBase<TraceableFifo<A,B,C>>
  : public MutableTraceableFifoOperations<JS::MutableHandle<TraceableFifo<A,B,C>>, A,B,C>
{
    using TF = TraceableFifo<A,B,C>;

    friend class TraceableFifoOperations<JS::MutableHandle<TF>, A,B,C>;
    const TF& extract() const {
        return *static_cast<const JS::MutableHandle<TF>*>(this)->address();
    }

    friend class MutableTraceableFifoOperations<JS::MutableHandle<TF>, A,B,C>;
    TF& extract() { return *static_cast<JS::MutableHandle<TF>*>(this)->address(); }
};

template <typename A, size_t B, typename C>
class HandleBase<TraceableFifo<A,B,C>>
  : public TraceableFifoOperations<JS::Handle<TraceableFifo<A,B,C>>, A,B,C>
{
    using TF = TraceableFifo<A,B,C>;

    friend class TraceableFifoOperations<JS::Handle<TF>, A,B,C>;
    const TF& extract() const {
        return *static_cast<const JS::Handle<TF>*>(this)->address();
    }
};

} // namespace js

#endif // js_TraceableFifo_h
