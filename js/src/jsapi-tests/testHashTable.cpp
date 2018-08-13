/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"

#include "js/HashTable.h"
#include "js/Utility.h"

#include "jsapi-tests/tests.h"

//#define FUZZ

typedef js::HashMap<uint32_t, uint32_t, js::DefaultHasher<uint32_t>, js::SystemAllocPolicy> IntMap;
typedef js::HashSet<uint32_t, js::DefaultHasher<uint32_t>, js::SystemAllocPolicy> IntSet;

/*
 * The rekeying test as conducted by adding only keys masked with 0x0000FFFF
 * that are unique. We rekey by shifting left 16 bits.
 */
#ifdef FUZZ
const size_t TestSize = 0x0000FFFF / 2;
const size_t TestIterations = SIZE_MAX;
#else
const size_t TestSize = 10000;
const size_t TestIterations = 10;
#endif

JS_STATIC_ASSERT(TestSize <= 0x0000FFFF / 2);

struct LowToHigh
{
    static uint32_t rekey(uint32_t initial) {
        if (initial > uint32_t(0x0000FFFF))
            return initial;
        return initial << 16;
    }

    static bool shouldBeRemoved(uint32_t initial) {
        return false;
    }
};

struct LowToHighWithRemoval
{
    static uint32_t rekey(uint32_t initial) {
        if (initial > uint32_t(0x0000FFFF))
            return initial;
        return initial << 16;
    }

    static bool shouldBeRemoved(uint32_t initial) {
        if (initial >= 0x00010000)
            return (initial >> 16) % 2 == 0;
        return initial % 2 == 0;
    }
};

static bool
MapsAreEqual(IntMap& am, IntMap& bm)
{
    bool equal = true;
    if (am.count() != bm.count()) {
        equal = false;
        fprintf(stderr, "A.count() == %u and B.count() == %u\n", am.count(), bm.count());
    }
    for (auto iter = am.iter(); !iter.done(); iter.next()) {
        if (!bm.has(iter.get().key())) {
            equal = false;
            fprintf(stderr, "B does not have %x which is in A\n", iter.get().key());
        }
    }
    for (auto iter = bm.iter(); !iter.done(); iter.next()) {
        if (!am.has(iter.get().key())) {
            equal = false;
            fprintf(stderr, "A does not have %x which is in B\n", iter.get().key());
        }
    }
    return equal;
}

static bool
SetsAreEqual(IntSet& am, IntSet& bm)
{
    bool equal = true;
    if (am.count() != bm.count()) {
        equal = false;
        fprintf(stderr, "A.count() == %u and B.count() == %u\n", am.count(), bm.count());
    }
    for (auto iter = am.iter(); !iter.done(); iter.next()) {
        if (!bm.has(iter.get())) {
            equal = false;
            fprintf(stderr, "B does not have %x which is in A\n", iter.get());
        }
    }
    for (auto iter = bm.iter(); !iter.done(); iter.next()) {
        if (!am.has(iter.get())) {
            equal = false;
            fprintf(stderr, "A does not have %x which is in B\n", iter.get());
        }
    }
    return equal;
}

static bool
AddLowKeys(IntMap* am, IntMap* bm, int seed)
{
    size_t i = 0;
    srand(seed);
    while (i < TestSize) {
        uint32_t n = rand() & 0x0000FFFF;
        if (!am->has(n)) {
            if (bm->has(n))
                return false;

            if (!am->putNew(n, n) || !bm->putNew(n, n))
                return false;
            i++;
        }
    }
    return true;
}

static bool
AddLowKeys(IntSet* as, IntSet* bs, int seed)
{
    size_t i = 0;
    srand(seed);
    while (i < TestSize) {
        uint32_t n = rand() & 0x0000FFFF;
        if (!as->has(n)) {
            if (bs->has(n))
                return false;
            if (!as->putNew(n) || !bs->putNew(n))
                return false;
            i++;
        }
    }
    return true;
}

template <class NewKeyFunction>
static bool
SlowRekey(IntMap* m) {
    IntMap tmp;

    for (auto iter = m->iter(); !iter.done(); iter.next()) {
        if (NewKeyFunction::shouldBeRemoved(iter.get().key()))
            continue;
        uint32_t hi = NewKeyFunction::rekey(iter.get().key());
        if (tmp.has(hi))
            return false;
        if (!tmp.putNew(hi, iter.get().value()))
            return false;
    }

    m->clear();
    for (auto iter = tmp.iter(); !iter.done(); iter.next()) {
        if (!m->putNew(iter.get().key(), iter.get().value()))
            return false;
    }

    return true;
}

template <class NewKeyFunction>
static bool
SlowRekey(IntSet* s) {
    IntSet tmp;

    for (auto iter = s->iter(); !iter.done(); iter.next()) {
        if (NewKeyFunction::shouldBeRemoved(iter.get()))
            continue;
        uint32_t hi = NewKeyFunction::rekey(iter.get());
        if (tmp.has(hi))
            return false;
        if (!tmp.putNew(hi))
            return false;
    }

    s->clear();
    for (auto iter = tmp.iter(); !iter.done(); iter.next()) {
        if (!s->putNew(iter.get()))
            return false;
    }

    return true;
}

BEGIN_TEST(testHashRekeyManual)
{
    IntMap am, bm;
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "map1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&am, &bm, i));
        CHECK(MapsAreEqual(am, bm));

        for (auto iter = am.modIter(); !iter.done(); iter.next()) {
            uint32_t tmp = LowToHigh::rekey(iter.get().key());
            if (tmp != iter.get().key())
                iter.rekey(tmp);
        }
        CHECK(SlowRekey<LowToHigh>(&bm));

        CHECK(MapsAreEqual(am, bm));
        am.clear();
        bm.clear();
    }

    IntSet as, bs;
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "set1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&as, &bs, i));
        CHECK(SetsAreEqual(as, bs));

        for (auto iter = as.modIter(); !iter.done(); iter.next()) {
            uint32_t tmp = LowToHigh::rekey(iter.get());
            if (tmp != iter.get())
                iter.rekey(tmp);
        }
        CHECK(SlowRekey<LowToHigh>(&bs));

        CHECK(SetsAreEqual(as, bs));
        as.clear();
        bs.clear();
    }

    return true;
}
END_TEST(testHashRekeyManual)

BEGIN_TEST(testHashRekeyManualRemoval)
{
    IntMap am, bm;
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "map2: %lu\n", i);
#endif
        CHECK(AddLowKeys(&am, &bm, i));
        CHECK(MapsAreEqual(am, bm));

        for (auto iter = am.modIter(); !iter.done(); iter.next()) {
            if (LowToHighWithRemoval::shouldBeRemoved(iter.get().key())) {
                iter.remove();
            } else {
                uint32_t tmp = LowToHighWithRemoval::rekey(iter.get().key());
                if (tmp != iter.get().key())
                    iter.rekey(tmp);
            }
        }
        CHECK(SlowRekey<LowToHighWithRemoval>(&bm));

        CHECK(MapsAreEqual(am, bm));
        am.clear();
        bm.clear();
    }

    IntSet as, bs;
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "set1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&as, &bs, i));
        CHECK(SetsAreEqual(as, bs));

        for (auto iter = as.modIter(); !iter.done(); iter.next()) {
            if (LowToHighWithRemoval::shouldBeRemoved(iter.get())) {
                iter.remove();
            } else {
                uint32_t tmp = LowToHighWithRemoval::rekey(iter.get());
                if (tmp != iter.get())
                    iter.rekey(tmp);
            }
        }
        CHECK(SlowRekey<LowToHighWithRemoval>(&bs));

        CHECK(SetsAreEqual(as, bs));
        as.clear();
        bs.clear();
    }

    return true;
}
END_TEST(testHashRekeyManualRemoval)

// A type that is not copyable, only movable.
struct MoveOnlyType {
    uint32_t val;

    explicit MoveOnlyType(uint32_t val) : val(val) { }

    MoveOnlyType(MoveOnlyType&& rhs) {
        val = rhs.val;
    }

    MoveOnlyType& operator=(MoveOnlyType&& rhs) {
        MOZ_ASSERT(&rhs != this);
        this->~MoveOnlyType();
        new(this) MoveOnlyType(std::move(rhs));
        return *this;
    }

    struct HashPolicy {
        typedef MoveOnlyType Lookup;

        static js::HashNumber hash(const Lookup& lookup) {
            return lookup.val;
        }

        static bool match(const MoveOnlyType& existing, const Lookup& lookup) {
            return existing.val == lookup.val;
        }
    };

  private:
    MoveOnlyType(const MoveOnlyType&) = delete;
    MoveOnlyType& operator=(const MoveOnlyType&) = delete;
};

BEGIN_TEST(testHashSetOfMoveOnlyType)
{
    typedef js::HashSet<MoveOnlyType, MoveOnlyType::HashPolicy, js::SystemAllocPolicy> Set;

    Set set;

    MoveOnlyType a(1);

    CHECK(set.put(std::move(a))); // This shouldn't generate a compiler error.

    return true;
}
END_TEST(testHashSetOfMoveOnlyType)

#if defined(DEBUG)

// Add entries to a HashMap until either we get an OOM, or the table has been
// resized a few times.
static bool
GrowUntilResize()
{
    IntMap m;

    // Add entries until we've resized the table four times.
    size_t lastCapacity = m.capacity();
    size_t resizes = 0;
    uint32_t key = 0;
    while (resizes < 4) {
        auto p = m.lookupForAdd(key);
        if (!p && !m.add(p, key, 0))
            return false;   // OOM'd in lookupForAdd() or add()

        size_t capacity = m.capacity();
        if (capacity != lastCapacity) {
            resizes++;
            lastCapacity = capacity;
        }
        key++;
    }

    return true;
}

BEGIN_TEST(testHashMapGrowOOM)
{
    uint32_t timeToFail;
    for (timeToFail = 1; timeToFail < 1000; timeToFail++) {
        js::oom::SimulateOOMAfter(timeToFail, js::THREAD_TYPE_MAIN, false);
        GrowUntilResize();
    }

    js::oom::ResetSimulatedOOM();
    return true;
}

END_TEST(testHashMapGrowOOM)
#endif // defined(DEBUG)

BEGIN_TEST(testHashTableMovableModIterator)
{
    IntSet set;

    // Exercise returning a hash table ModIterator object from a function.

    CHECK(set.put(1));
    for (auto iter = setModIter(set); !iter.done(); iter.next())
        iter.remove();
    CHECK(set.count() == 0);

    // Test moving an ModIterator object explicitly.

    CHECK(set.put(1));
    CHECK(set.put(2));
    CHECK(set.put(3));
    CHECK(set.count() == 3);
    {
        auto i1 = set.modIter();
        CHECK(!i1.done());
        i1.remove();
        i1.next();

        auto i2 = std::move(i1);
        CHECK(!i2.done());
        i2.remove();
        i2.next();
    }

    CHECK(set.count() == 1);
    return true;
}

IntSet::ModIterator setModIter(IntSet& set)
{
    return set.modIter();
}

END_TEST(testHashTableMovableModIterator)

BEGIN_TEST(testHashLazyStorage)
{
    // The following code depends on the current capacity computation, which
    // could change in the future.
    uint32_t defaultCap = 32;
    uint32_t minCap = 4;

    IntSet set;
    CHECK(set.capacity() == 0);

    CHECK(set.put(1));
    CHECK(set.capacity() == defaultCap);

    set.compact();                  // effectively a no-op
    CHECK(set.capacity() == minCap);

    set.clear();
    CHECK(set.capacity() == minCap);

    set.compact();
    CHECK(set.capacity() == 0);

    CHECK(set.putNew(1));
    CHECK(set.capacity() == minCap);

    set.clear();
    set.compact();
    CHECK(set.capacity() == 0);

    // lookupForAdd() instantiates, even if not followed by add().
    set.lookupForAdd(1);
    CHECK(set.capacity() == minCap);

    set.clear();
    set.compact();
    CHECK(set.capacity() == 0);

    CHECK(set.reserve(0));          // a no-op
    CHECK(set.capacity() == 0);

    CHECK(set.reserve(1));
    CHECK(set.capacity() == minCap);

    CHECK(set.reserve(0));          // a no-op
    CHECK(set.capacity() == minCap);

    CHECK(set.reserve(2));          // effectively a no-op
    CHECK(set.capacity() == minCap);

    // No need to clear here because we didn't add anything.
    set.compact();
    CHECK(set.capacity() == 0);

    CHECK(set.reserve(128));
    CHECK(set.capacity() == 256);
    CHECK(set.reserve(3));          // effectively a no-op
    CHECK(set.capacity() == 256);
    for (int i = 0; i < 8; i++) {
      CHECK(set.putNew(i));
    }
    CHECK(set.count() == 8);
    CHECK(set.capacity() == 256);
    set.compact();
    CHECK(set.capacity() == 16);
    set.compact();                  // effectively a no-op
    CHECK(set.capacity() == 16);
    for (int i = 8; i < 16; i++) {
      CHECK(set.putNew(i));
    }
    CHECK(set.count() == 16);
    CHECK(set.capacity() == 32);
    set.clear();
    CHECK(set.capacity() == 32);
    set.compact();
    CHECK(set.capacity() == 0);

    return true;
}
END_TEST(testHashLazyStorage)

