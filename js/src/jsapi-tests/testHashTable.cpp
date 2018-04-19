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
    for (IntMap::Range r = am.all(); !r.empty(); r.popFront()) {
        if (!bm.has(r.front().key())) {
            equal = false;
            fprintf(stderr, "B does not have %x which is in A\n", r.front().key());
        }
    }
    for (IntMap::Range r = bm.all(); !r.empty(); r.popFront()) {
        if (!am.has(r.front().key())) {
            equal = false;
            fprintf(stderr, "A does not have %x which is in B\n", r.front().key());
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
    for (IntSet::Range r = am.all(); !r.empty(); r.popFront()) {
        if (!bm.has(r.front())) {
            equal = false;
            fprintf(stderr, "B does not have %x which is in A\n", r.front());
        }
    }
    for (IntSet::Range r = bm.all(); !r.empty(); r.popFront()) {
        if (!am.has(r.front())) {
            equal = false;
            fprintf(stderr, "A does not have %x which is in B\n", r.front());
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
    if (!tmp.init())
        return false;

    for (IntMap::Range r = m->all(); !r.empty(); r.popFront()) {
        if (NewKeyFunction::shouldBeRemoved(r.front().key()))
            continue;
        uint32_t hi = NewKeyFunction::rekey(r.front().key());
        if (tmp.has(hi))
            return false;
        if (!tmp.putNew(hi, r.front().value()))
            return false;
    }

    m->clear();
    for (IntMap::Range r = tmp.all(); !r.empty(); r.popFront()) {
        if (!m->putNew(r.front().key(), r.front().value()))
            return false;
    }

    return true;
}

template <class NewKeyFunction>
static bool
SlowRekey(IntSet* s) {
    IntSet tmp;
    if (!tmp.init())
        return false;

    for (IntSet::Range r = s->all(); !r.empty(); r.popFront()) {
        if (NewKeyFunction::shouldBeRemoved(r.front()))
            continue;
        uint32_t hi = NewKeyFunction::rekey(r.front());
        if (tmp.has(hi))
            return false;
        if (!tmp.putNew(hi))
            return false;
    }

    s->clear();
    for (IntSet::Range r = tmp.all(); !r.empty(); r.popFront()) {
        if (!s->putNew(r.front()))
            return false;
    }

    return true;
}

BEGIN_TEST(testHashRekeyManual)
{
    IntMap am, bm;
    CHECK(am.init());
    CHECK(bm.init());
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "map1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&am, &bm, i));
        CHECK(MapsAreEqual(am, bm));

        for (IntMap::Enum e(am); !e.empty(); e.popFront()) {
            uint32_t tmp = LowToHigh::rekey(e.front().key());
            if (tmp != e.front().key())
                e.rekeyFront(tmp);
        }
        CHECK(SlowRekey<LowToHigh>(&bm));

        CHECK(MapsAreEqual(am, bm));
        am.clear();
        bm.clear();
    }

    IntSet as, bs;
    CHECK(as.init());
    CHECK(bs.init());
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "set1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&as, &bs, i));
        CHECK(SetsAreEqual(as, bs));

        for (IntSet::Enum e(as); !e.empty(); e.popFront()) {
            uint32_t tmp = LowToHigh::rekey(e.front());
            if (tmp != e.front())
                e.rekeyFront(tmp);
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
    CHECK(am.init());
    CHECK(bm.init());
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "map2: %lu\n", i);
#endif
        CHECK(AddLowKeys(&am, &bm, i));
        CHECK(MapsAreEqual(am, bm));

        for (IntMap::Enum e(am); !e.empty(); e.popFront()) {
            if (LowToHighWithRemoval::shouldBeRemoved(e.front().key())) {
                e.removeFront();
            } else {
                uint32_t tmp = LowToHighWithRemoval::rekey(e.front().key());
                if (tmp != e.front().key())
                    e.rekeyFront(tmp);
            }
        }
        CHECK(SlowRekey<LowToHighWithRemoval>(&bm));

        CHECK(MapsAreEqual(am, bm));
        am.clear();
        bm.clear();
    }

    IntSet as, bs;
    CHECK(as.init());
    CHECK(bs.init());
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "set1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&as, &bs, i));
        CHECK(SetsAreEqual(as, bs));

        for (IntSet::Enum e(as); !e.empty(); e.popFront()) {
            if (LowToHighWithRemoval::shouldBeRemoved(e.front())) {
                e.removeFront();
            } else {
                uint32_t tmp = LowToHighWithRemoval::rekey(e.front());
                if (tmp != e.front())
                    e.rekeyFront(tmp);
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
        new(this) MoveOnlyType(mozilla::Move(rhs));
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
    CHECK(set.init());

    MoveOnlyType a(1);

    CHECK(set.put(mozilla::Move(a))); // This shouldn't generate a compiler error.

    return true;
}
END_TEST(testHashSetOfMoveOnlyType)

#if defined(DEBUG)

// Add entries to a HashMap using lookupWithDefault until either we get an OOM,
// or the table has been resized a few times.
static bool
LookupWithDefaultUntilResize() {
    IntMap m;

    if (!m.init())
        return false;

    // Add entries until we've resized the table four times.
    size_t lastCapacity = m.capacity();
    size_t resizes = 0;
    uint32_t key = 0;
    while (resizes < 4) {
        if (!m.lookupWithDefault(key++, 0))
            return false;

        size_t capacity = m.capacity();
        if (capacity != lastCapacity) {
            resizes++;
            lastCapacity = capacity;
        }
    }

    return true;
}

BEGIN_TEST(testHashMapLookupWithDefaultOOM)
{
    uint32_t timeToFail;
    for (timeToFail = 1; timeToFail < 1000; timeToFail++) {
        js::oom::SimulateOOMAfter(timeToFail, js::THREAD_TYPE_MAIN, false);
        LookupWithDefaultUntilResize();
    }

    js::oom::ResetSimulatedOOM();
    return true;
}

END_TEST(testHashMapLookupWithDefaultOOM)
#endif // defined(DEBUG)

BEGIN_TEST(testHashTableMovableEnum)
{
    IntSet set;
    CHECK(set.init());

    // Exercise returning a hash table Enum object from a function.

    CHECK(set.put(1));
    for (auto e = enumerateSet(set); !e.empty(); e.popFront())
        e.removeFront();
    CHECK(set.count() == 0);

    // Test moving an Enum object explicitly.

    CHECK(set.put(1));
    CHECK(set.put(2));
    CHECK(set.put(3));
    CHECK(set.count() == 3);
    {
        auto e1 = IntSet::Enum(set);
        CHECK(!e1.empty());
        e1.removeFront();
        e1.popFront();

        auto e2 = mozilla::Move(e1);
        CHECK(!e2.empty());
        e2.removeFront();
        e2.popFront();
    }

    CHECK(set.count() == 1);
    return true;
}

IntSet::Enum enumerateSet(IntSet& set)
{
    return IntSet::Enum(set);
}

END_TEST(testHashTableMovableEnum)
