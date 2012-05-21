/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InlineMap_h__
#define InlineMap_h__

#include "js/HashTable.h"

namespace js {

/*
 * A type can only be used as an InlineMap key if zero is an invalid key value
 * (and thus may be used as a tombstone value by InlineMap).
 */
template <typename T> struct ZeroIsReserved         { static const bool result = false; };
template <typename T> struct ZeroIsReserved<T *>    { static const bool result = true; };

template <typename K, typename V, size_t InlineElems>
class InlineMap
{
  public:
    typedef HashMap<K, V, DefaultHasher<K>, TempAllocPolicy> WordMap;

    struct InlineElem
    {
        K key;
        V value;
    };

  private:
    typedef typename WordMap::Ptr       WordMapPtr;
    typedef typename WordMap::AddPtr    WordMapAddPtr;
    typedef typename WordMap::Range     WordMapRange;

    size_t          inlNext;
    size_t          inlCount;
    InlineElem      inl[InlineElems];
    WordMap         map;

    void checkStaticInvariants() {
        JS_STATIC_ASSERT(ZeroIsReserved<K>::result);
    }

    bool usingMap() const {
        return inlNext > InlineElems;
    }

    bool switchToMap() {
        JS_ASSERT(inlNext == InlineElems);

        if (map.initialized()) {
            map.clear();
        } else {
            if (!map.init(count()))
                return false;
            JS_ASSERT(map.initialized());
        }

        for (InlineElem *it = inl, *end = inl + inlNext; it != end; ++it) {
            if (it->key && !map.putNew(it->key, it->value))
                return false;
        }

        inlNext = InlineElems + 1;
        JS_ASSERT(map.count() == inlCount);
        JS_ASSERT(usingMap());
        return true;
    }

    JS_NEVER_INLINE
    bool switchAndAdd(const K &key, const V &value) {
        if (!switchToMap())
            return false;

        return map.putNew(key, value);
    }

  public:
    explicit InlineMap(JSContext *cx)
      : inlNext(0), inlCount(0), map(cx) {
        checkStaticInvariants(); /* Force the template to instantiate the static invariants. */
    }

    class Entry
    {
        friend class InlineMap;
        const K &key_;
        const V &value_;

        Entry(const K &key, const V &value) : key_(key), value_(value) {}

      public:
        const K &key() { return key_; }
        const V &value() { return value_; }
    }; /* class Entry */

    class Ptr
    {
        friend class InlineMap;

        WordMapPtr  mapPtr;
        InlineElem  *inlPtr;
        bool        isInlinePtr;

        typedef Ptr ******* ConvertibleToBool;

        explicit Ptr(WordMapPtr p) : mapPtr(p), isInlinePtr(false) {}
        explicit Ptr(InlineElem *ie) : inlPtr(ie), isInlinePtr(true) {}
        void operator==(const Ptr &other);

      public:
        /* Leaves Ptr uninitialized. */
        Ptr() {
#ifdef DEBUG
            inlPtr = (InlineElem *) 0xbad;
            isInlinePtr = true;
#endif
        }

        /* Default copy constructor works for this structure. */

        bool found() const {
            return isInlinePtr ? bool(inlPtr) : mapPtr.found();
        }

        operator ConvertibleToBool() const {
            return ConvertibleToBool(found());
        }

        K &key() {
            JS_ASSERT(found());
            return isInlinePtr ? inlPtr->key : mapPtr->key;
        }

        V &value() {
            JS_ASSERT(found());
            return isInlinePtr ? inlPtr->value : mapPtr->value;
        }
    }; /* class Ptr */

    class AddPtr
    {
        friend class InlineMap;

        WordMapAddPtr   mapAddPtr;
        InlineElem      *inlAddPtr;
        bool            isInlinePtr;
        /* Indicates whether inlAddPtr is a found result or an add pointer. */
        bool            inlPtrFound;

        AddPtr(InlineElem *ptr, bool found)
          : inlAddPtr(ptr), isInlinePtr(true), inlPtrFound(found)
        {}

        AddPtr(const WordMapAddPtr &p) : mapAddPtr(p), isInlinePtr(false) {}

        void operator==(const AddPtr &other);

        typedef AddPtr ******* ConvertibleToBool;

      public:
        AddPtr() {}

        bool found() const {
            return isInlinePtr ? inlPtrFound : mapAddPtr.found();
        }

        operator ConvertibleToBool() const {
            return found() ? ConvertibleToBool(1) : ConvertibleToBool(0);
        }

        V &value() {
            JS_ASSERT(found());
            if (isInlinePtr)
                return inlAddPtr->value;
            return mapAddPtr->value;
        }
    }; /* class AddPtr */

    size_t count() {
        return usingMap() ? map.count() : inlCount;
    }

    bool empty() const {
        return usingMap() ? map.empty() : !inlCount;
    }

    void clear() {
        inlNext = 0;
        inlCount = 0;
    }

    bool isMap() const {
        return usingMap();
    }

    const WordMap &asMap() const {
        JS_ASSERT(isMap());
        return map;
    }

    const InlineElem *asInline() const {
        JS_ASSERT(!isMap());
        return inl;
    }

    const InlineElem *inlineEnd() const {
        JS_ASSERT(!isMap());
        return inl + inlNext;
    }

    JS_ALWAYS_INLINE
    Ptr lookup(const K &key) {
        if (usingMap())
            return Ptr(map.lookup(key));

        for (InlineElem *it = inl, *end = inl + inlNext; it != end; ++it) {
            if (it->key == key)
                return Ptr(it);
        }

        return Ptr(NULL);
    }

    JS_ALWAYS_INLINE
    AddPtr lookupForAdd(const K &key) {
        if (usingMap())
            return AddPtr(map.lookupForAdd(key));

        for (InlineElem *it = inl, *end = inl + inlNext; it != end; ++it) {
            if (it->key == key)
                return AddPtr(it, true);
        }

        /*
         * The add pointer that's returned here may indicate the limit entry of
         * the linear space, in which case the |add| operation will initialize
         * the map if necessary and add the entry there.
         */
        return AddPtr(inl + inlNext, false);
    }

    JS_ALWAYS_INLINE
    bool add(AddPtr &p, const K &key, const V &value) {
        JS_ASSERT(!p);

        if (p.isInlinePtr) {
            InlineElem *addPtr = p.inlAddPtr;
            JS_ASSERT(addPtr == inl + inlNext);

            /* Switching to map mode before we add this pointer. */
            if (addPtr == inl + InlineElems)
                return switchAndAdd(key, value);

            JS_ASSERT(!p.found());
            JS_ASSERT(uintptr_t(inl + inlNext) == uintptr_t(p.inlAddPtr));
            p.inlAddPtr->key = key;
            p.inlAddPtr->value = value;
            ++inlCount;
            ++inlNext;
            return true;
        }

        return map.add(p.mapAddPtr, key, value);
    }

    JS_ALWAYS_INLINE
    bool put(const K &key, const V &value) {
        AddPtr p = lookupForAdd(key);
        if (p) {
            p.value() = value;
            return true;
        }
        return add(p, key, value);
    }

    void remove(Ptr p) {
        JS_ASSERT(p);
        if (p.isInlinePtr) {
            JS_ASSERT(inlCount > 0);
            JS_ASSERT(p.inlPtr->key != NULL);
            p.inlPtr->key = NULL;
            --inlCount;
            return;
        }
        JS_ASSERT(map.initialized() && usingMap());
        map.remove(p.mapPtr);
    }

    void remove(const K &key) {
        if (Ptr p = lookup(key))
            remove(p);
    }

    class Range
    {
        friend class InlineMap;

        WordMapRange    mapRange;
        InlineElem      *cur;
        InlineElem      *end;
        bool            isInline;

        explicit Range(WordMapRange r)
          : cur(NULL), end(NULL), /* Avoid GCC 4.3.3 over-warning. */
            isInline(false) {
            mapRange = r;
            JS_ASSERT(!isInlineRange());
        }

        Range(const InlineElem *begin, const InlineElem *end_)
          : cur(const_cast<InlineElem *>(begin)),
            end(const_cast<InlineElem *>(end_)),
            isInline(true) {
            advancePastNulls(cur);
            JS_ASSERT(isInlineRange());
        }

        bool checkInlineRangeInvariants() const {
            JS_ASSERT(uintptr_t(cur) <= uintptr_t(end));
            JS_ASSERT_IF(cur != end, cur->key != NULL);
            return true;
        }

        bool isInlineRange() const {
            JS_ASSERT_IF(isInline, checkInlineRangeInvariants());
            return isInline;
        }

        void advancePastNulls(InlineElem *begin) {
            InlineElem *newCur = begin;
            while (newCur < end && NULL == newCur->key)
                ++newCur;
            JS_ASSERT(uintptr_t(newCur) <= uintptr_t(end));
            cur = newCur;
        }

        void bumpCurPtr() {
            JS_ASSERT(isInlineRange());
            advancePastNulls(cur + 1);
        }

        void operator==(const Range &other);

      public:
        bool empty() const {
            return isInlineRange() ? cur == end : mapRange.empty();
        }

        Entry front() {
            JS_ASSERT(!empty());
            if (isInlineRange())
                return Entry(cur->key, cur->value);
            return Entry(mapRange.front().key, mapRange.front().value);
        }

        void popFront() {
            JS_ASSERT(!empty());
            if (isInlineRange())
                bumpCurPtr();
            else
                mapRange.popFront();
        }
    }; /* class Range */

    Range all() const {
        return usingMap() ? Range(map.all()) : Range(inl, inl + inlNext);
    }
}; /* class InlineMap */

} /* namespace js */

#endif
