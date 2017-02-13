/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_NurseryAwareHashMap_h
#define gc_NurseryAwareHashMap_h

namespace js {

namespace detail {
// This class only handles the incremental case and does not deal with nursery
// pointers. The only users should be for NurseryAwareHashMap; it is defined
// externally because we need a GCPolicy for its use in the contained map.
template <typename T>
class UnsafeBareReadBarriered : public ReadBarrieredBase<T>
{
  public:
    UnsafeBareReadBarriered() : ReadBarrieredBase<T>(JS::GCPolicy<T>::initial()) {}
    MOZ_IMPLICIT UnsafeBareReadBarriered(const T& v) : ReadBarrieredBase<T>(v) {}
    explicit UnsafeBareReadBarriered(const UnsafeBareReadBarriered& v) : ReadBarrieredBase<T>(v) {}
    UnsafeBareReadBarriered(UnsafeBareReadBarriered&& v)
      : ReadBarrieredBase<T>(mozilla::Move(v))
    {}

    UnsafeBareReadBarriered& operator=(const UnsafeBareReadBarriered& v) {
        this->value = v.value;
        return *this;
    }

    UnsafeBareReadBarriered& operator=(const T& v) {
        this->value = v;
        return *this;
    }

    const T get() const {
        if (!InternalBarrierMethods<T>::isMarkable(this->value))
            return JS::GCPolicy<T>::initial();
        this->read();
        return this->value;
    }

    explicit operator bool() const {
        return bool(this->value);
    }

    const T unbarrieredGet() const { return this->value; }
    T* unsafeGet() { return &this->value; }
    T const* unsafeGet() const { return &this->value; }
};
} // namespace detail

// The "nursery aware" hash map is a special case of GCHashMap that is able to
// treat nursery allocated members weakly during a minor GC: e.g. it allows for
// nursery allocated objects to be collected during nursery GC where a normal
// hash table treats such edges strongly.
//
// Doing this requires some strong constraints on what can be stored in this
// table and how it can be accessed. At the moment, this table assumes that
// all values contain a strong reference to the key. It also requires the
// policy to contain an |isTenured| and |needsSweep| members, which is fairly
// non-standard. This limits its usefulness to the CrossCompartmentMap at the
// moment, but might serve as a useful base for other tables in future.
template <typename Key,
          typename Value,
          typename HashPolicy = DefaultHasher<Key>,
          typename AllocPolicy = TempAllocPolicy>
class NurseryAwareHashMap
{
    using BarrieredValue = detail::UnsafeBareReadBarriered<Value>;
    using MapType = GCRekeyableHashMap<Key, BarrieredValue, HashPolicy, AllocPolicy>;

    // Contains entries that have a nursery-allocated key or value (or both).
    MapType nurseryMap_;

    // All entries in this map have a tenured key and value.
    MapType tenuredMap_;

    // Keys and values usually have the same lifetime (for the WrapperMap we
    // ensure this when we allocate the wrapper object). If this flag is set,
    // it means nurseryMap_ contains a tenured key with a nursery allocated
    // value.
    bool nurseryMapContainsTenuredKeys_;

  public:
    using Lookup = typename MapType::Lookup;

    class Ptr {
        friend class NurseryAwareHashMap;

        typename MapType::Ptr ptr_;
        bool isNurseryMap_;

      public:
        Ptr(typename MapType::Ptr ptr, bool isNurseryMap)
          : ptr_(ptr), isNurseryMap_(isNurseryMap)
        {}

        const typename MapType::Entry& operator*() const { return *ptr_; }
        const typename MapType::Entry* operator->() const { return &*ptr_; }

        bool found() const { return ptr_.found(); }
        explicit operator bool() const { return bool(ptr_); }
    };

    explicit NurseryAwareHashMap(AllocPolicy a = AllocPolicy())
      : nurseryMap_(a), tenuredMap_(a), nurseryMapContainsTenuredKeys_(false)
    {}

    MOZ_MUST_USE bool init(uint32_t len = 16) {
        return nurseryMap_.init(len) && tenuredMap_.init(len);
    }

    bool empty() const { return nurseryMap_.empty() && tenuredMap_.empty(); }

    Ptr lookup(const Lookup& l) const {
        if (JS::GCPolicy<Key>::isTenured(l)) {
            // If we find the key in the tenuredMap_, we're done. If we don't
            // find it there and we know nurseryMap_ contains tenured keys
            // (hopefully uncommon), we have to try nurseryMap_ as well.
            typename MapType::Ptr p = tenuredMap_.lookup(l);
            if (p || !nurseryMapContainsTenuredKeys_)
                return Ptr(p, /* isNurseryMap = */ false);
        }
        return Ptr(nurseryMap_.lookup(l), /* isNurseryMap = */ true);
    }

    void remove(Ptr p) {
        if (p.isNurseryMap_)
            nurseryMap_.remove(p.ptr_);
        else
            tenuredMap_.remove(p.ptr_);
    }

    class Enum {
        // First iterate over the nursery map. When nurseryEnum_ becomes
        // empty() we switch to tenuredEnum_.
        typename MapType::Enum nurseryEnum_;
        typename MapType::Enum tenuredEnum_;

        const typename MapType::Enum& currentEnum() const {
            return nurseryEnum_.empty() ? tenuredEnum_ : nurseryEnum_;
        }
        typename MapType::Enum& currentEnum() {
            return nurseryEnum_.empty() ? tenuredEnum_ : nurseryEnum_;
        }

      public:
        explicit Enum(NurseryAwareHashMap& namap)
          : nurseryEnum_(namap.nurseryMap_), tenuredEnum_(namap.tenuredMap_)
        {}

        typename MapType::Entry& front() const { return currentEnum().front(); }
        void popFront() { currentEnum().popFront(); }
        void removeFront() { currentEnum().removeFront(); }

        bool empty() const { return nurseryEnum_.empty() && tenuredEnum_.empty(); }
    };

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        size_t size = nurseryMap_.sizeOfExcludingThis(mallocSizeOf);
        size += tenuredMap_.sizeOfExcludingThis(mallocSizeOf);
        return size;
    }
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        size_t size = nurseryMap_.sizeOfIncludingThis(mallocSizeOf);
        size += tenuredMap_.sizeOfIncludingThis(mallocSizeOf);
        return size;
    }

    MOZ_MUST_USE bool putNew(const Key& k, const Value& v) {
        MOZ_ASSERT(!tenuredMap_.has(k));
        MOZ_ASSERT(!nurseryMap_.has(k));

        bool tenuredKey = JS::GCPolicy<Key>::isTenured(k);
        if (tenuredKey && JS::GCPolicy<Value>::isTenured(v))
            return tenuredMap_.putNew(k, v);

        if (tenuredKey)
            nurseryMapContainsTenuredKeys_ = true;

        return nurseryMap_.putNew(k, v);
    }

    MOZ_MUST_USE bool put(const Key& k, const Value& v) {
        // For simplicity, just remove the entry and forward to putNew for now.
        // Performance-sensitive callers should prefer putNew.
        if (Ptr p = lookup(k))
            remove(p);
        return putNew(k, v);
    }

    void sweepAfterMinorGC(JSTracer* trc) {
        for (typename MapType::Enum e(nurseryMap_); !e.empty(); e.popFront()) {
            auto& key = e.front().key();
            auto& value = e.front().value();

            // Drop the entry if the value is not marked.
            if (JS::GCPolicy<BarrieredValue>::needsSweep(&value))
                continue;

            // Insert the key/value in the tenured map, if the value is still
            // needed.
            //
            // Note that this currently assumes that each Value will contain a
            // strong reference to Key, as per its use as the
            // CrossCompartmentWrapperMap. We may need to make the following
            // behavior more dynamic if we use this map in other nursery-aware
            // contexts.

            Key keyCopy(key);
            mozilla::DebugOnly<bool> sweepKey = JS::GCPolicy<Key>::needsSweep(&keyCopy);
            MOZ_ASSERT(!sweepKey);

            AutoEnterOOMUnsafeRegion oomUnsafe;
            if (!tenuredMap_.putNew(keyCopy, value))
                oomUnsafe.crash("NurseryAwareHashMap sweepAfterMinorGC");
        }

        nurseryMap_.clear();
        nurseryMapContainsTenuredKeys_ = false;
    }

    void sweep() {
        MOZ_ASSERT(nurseryMap_.empty());
        tenuredMap_.sweep();
    }
};

} // namespace js

namespace JS {
template <typename T>
struct GCPolicy<js::detail::UnsafeBareReadBarriered<T>>
{
    static void trace(JSTracer* trc, js::detail::UnsafeBareReadBarriered<T>* thingp,
                      const char* name)
    {
        js::TraceEdge(trc, thingp, name);
    }
    static bool needsSweep(js::detail::UnsafeBareReadBarriered<T>* thingp) {
        return js::gc::IsAboutToBeFinalized(thingp);
    }
};
} // namespace JS

#endif // gc_NurseryAwareHashMap_h
