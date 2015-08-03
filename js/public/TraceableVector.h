/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TraceableVector_h
#define js_TraceableVector_h

#include "mozilla/Vector.h"
#include "js/RootingAPI.h"
#include "js/Vector.h"

namespace js {

template <typename> struct DefaultTracer;

// A TraceableVector is a Vector with an additional trace method that knows how
// to visit all of the items stored in the Vector. For vectors that contain GC
// things, this is usually more convenient than manually iterating and marking
// the contents.
//
// Most types of GC pointers as keys and values can be traced with no extra
// infrastructure.  For structs and non-gc-pointer members, ensure that there
// is a specialization of DefaultTracer<T> with an appropriate trace method
// available to handle the custom type.
//
// Note that although this Vector's trace will deal correctly with moved items,
// it does not itself know when to barrier or trace items. To function properly
// it must either be used with Rooted, or barriered and traced manually.
template <typename T,
          size_t MinInlineCapacity = 0,
          typename AllocPolicy = TempAllocPolicy,
          typename TraceFunc = DefaultTracer<T>>
class TraceableVector
  : public mozilla::VectorBase<T,
                               MinInlineCapacity,
                               AllocPolicy,
                               TraceableVector<T, MinInlineCapacity, AllocPolicy, TraceFunc>>,
    public JS::DynamicTraceable
{
    using Base = mozilla::VectorBase<T, MinInlineCapacity, AllocPolicy, TraceableVector>;

  public:
    explicit TraceableVector(AllocPolicy alloc = AllocPolicy()) : Base(alloc) {}
    TraceableVector(TraceableVector&& vec) : Base(mozilla::Forward<TraceableVector>(vec)) {}
    TraceableVector& operator=(TraceableVector&& vec) {
        return Base::operator=(mozilla::Forward<TraceableVector>(vec));
    }

    void trace(JSTracer* trc) override {
        for (size_t i = 0; i < this->length(); ++i)
            TraceFunc::trace(trc, &Base::operator[](i), "vector element");
    }
};

template <typename Outer, typename T, size_t Capacity, typename AllocPolicy, typename TraceFunc>
class TraceableVectorOperations
{
    using Vec = TraceableVector<T, Capacity, AllocPolicy, TraceFunc>;
    const Vec& vec() const { return static_cast<const Outer*>(this)->extract(); }

  public:
    const AllocPolicy& allocPolicy() const { return vec().allocPolicy(); }
    size_t length() const { return vec().length(); }
    bool empty() const { return vec().empty(); }
    size_t capacity() const { return vec().capacity(); }
    const T* begin() const { return vec().begin(); }
    const T* end() const { return vec().end(); }
    const T& operator[](size_t aIndex) const { return vec().operator[](aIndex); }
    const T& back() const { return vec().back(); }
    bool canAppendWithoutRealloc(size_t aNeeded) const { return vec().canAppendWithoutRealloc(); }
};

template <typename Outer, typename T, size_t Capacity, typename AllocPolicy, typename TraceFunc>
class MutableTraceableVectorOperations
  : public TraceableVectorOperations<Outer, T, Capacity, AllocPolicy, TraceFunc>
{
    using Vec = TraceableVector<T, Capacity, AllocPolicy, TraceFunc>;
    const Vec& vec() const { return static_cast<const Outer*>(this)->extract(); }
    Vec& vec() { return static_cast<Outer*>(this)->extract(); }

  public:
    const AllocPolicy& allocPolicy() const { return vec().allocPolicy(); }
    AllocPolicy& allocPolicy() { return vec().allocPolicy(); }
    const T* begin() const { return vec().begin(); }
    T* begin() { return vec().begin(); }
    const T* end() const { return vec().end(); }
    T* end() { return vec().end(); }
    const T& operator[](size_t aIndex) const { return vec().operator[](aIndex); }
    T& operator[](size_t aIndex) { return vec().operator[](aIndex); }
    const T& back() const { return vec().back(); }
    T& back() { return vec().back(); }

    bool initCapacity(size_t aRequest) { return vec().initCapacity(aRequest); }
    bool reserve(size_t aRequest) { return vec().reserve(aRequest); }
    void shrinkBy(size_t aIncr) { vec().shrinkBy(aIncr); }
    bool growBy(size_t aIncr) { return vec().growBy(aIncr); }
    bool resize(size_t aNewLength) { return vec().resize(aNewLength); }
    bool growByUninitialized(size_t aIncr) { return vec().growByUninitialized(aIncr); }
    void infallibleGrowByUninitialized(size_t aIncr) { vec().infallibleGrowByUninitialized(aIncr); }
    bool resizeUninitialized(size_t aNewLength) { return vec().resizeUninitialized(aNewLength); }
    void clear() { vec().clear(); }
    void clearAndFree() { vec().clearAndFree(); }
    template<typename U> bool append(U&& aU) { return vec().append(mozilla::Forward<U>(aU)); }
    template<typename... Args> bool emplaceBack(Args&&... aArgs) {
        return vec().emplaceBack(mozilla::Forward<Args...>(aArgs...));
    }
    template<typename U, size_t O, class BP, class UV>
    bool appendAll(const mozilla::VectorBase<U, O, BP, UV>& aU) { return vec().appendAll(aU); }
    bool appendN(const T& aT, size_t aN) { return vec().appendN(aT, aN); }
    template<typename U> bool append(const U* aBegin, const U* aEnd) {
        return vec().append(aBegin, aEnd);
    }
    template<typename U> bool append(const U* aBegin, size_t aLength) {
        return vec().append(aBegin, aLength);
    }
    template<typename U> void infallibleAppend(U&& aU) {
        vec().infallibleAppend(mozilla::Forward<U>(aU));
    }
    void infallibleAppendN(const T& aT, size_t aN) { vec().infallibleAppendN(aT, aN); }
    template<typename U> void infallibleAppend(const U* aBegin, const U* aEnd) {
        vec().infallibleAppend(aBegin, aEnd);
    }
    template<typename U> void infallibleAppend(const U* aBegin, size_t aLength) {
        vec().infallibleAppend(aBegin, aLength);
    }
    void popBack() { vec().popBack(); }
    T popCopy() { return vec().podCopy(); }
    template<typename U> T* insert(T* aP, U&& aVal) {
        return vec().insert(aP, mozilla::Forward<U>(aVal));
    }
    void erase(T* aT) { vec().erase(aT); }
    void erase(T* aBegin, T* aEnd) { vec().erase(aBegin, aEnd); }
};

template <typename A, size_t B, typename C, typename D>
class RootedBase<TraceableVector<A,B,C,D>>
  : public MutableTraceableVectorOperations<JS::Rooted<TraceableVector<A,B,C,D>>, A,B,C,D>
{
    using Vec = TraceableVector<A,B,C,D>;

    friend class TraceableVectorOperations<JS::Rooted<Vec>, A,B,C,D>;
    const Vec& extract() const { return *static_cast<const JS::Rooted<Vec>*>(this)->address(); }

    friend class MutableTraceableVectorOperations<JS::Rooted<Vec>, A,B,C,D>;
    Vec& extract() { return *static_cast<JS::Rooted<Vec>*>(this)->address(); }
};

template <typename A, size_t B, typename C, typename D>
class MutableHandleBase<TraceableVector<A,B,C,D>>
  : public MutableTraceableVectorOperations<JS::MutableHandle<TraceableVector<A,B,C,D>>, A,B,C,D>
{
    using Vec = TraceableVector<A,B,C,D>;

    friend class TraceableVectorOperations<JS::MutableHandle<Vec>, A,B,C,D>;
    const Vec& extract() const {
        return *static_cast<const JS::MutableHandle<Vec>*>(this)->address();
    }

    friend class MutableTraceableVectorOperations<JS::MutableHandle<Vec>, A,B,C,D>;
    Vec& extract() { return *static_cast<JS::MutableHandle<Vec>*>(this)->address(); }
};

template <typename A, size_t B, typename C, typename D>
class HandleBase<TraceableVector<A,B,C,D>>
  : public TraceableVectorOperations<JS::Handle<TraceableVector<A,B,C,D>>, A,B,C,D>
{
    using Vec = TraceableVector<A,B,C,D>;

    friend class TraceableVectorOperations<JS::Handle<Vec>, A,B,C,D>;
    const Vec& extract() const {
        return *static_cast<const JS::Handle<Vec>*>(this)->address();
    }
};

} // namespace js

#endif // js_TraceableVector_h
