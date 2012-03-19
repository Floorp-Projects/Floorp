#include "tests.h"

#include "js/HashTable.h"

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
MapsAreEqual(IntMap &am, IntMap &bm)
{
    bool equal = true;
    if (am.count() != bm.count()) {
        equal = false;
        fprintf(stderr, "A.count() == %u and B.count() == %u\n", am.count(), bm.count());
    }
    for (IntMap::Range r = am.all(); !r.empty(); r.popFront()) {
        if (!bm.has(r.front().key)) {
            equal = false;
            fprintf(stderr, "B does not have %x which is in A\n", r.front().key);
        }
    }
    for (IntMap::Range r = bm.all(); !r.empty(); r.popFront()) {
        if (!am.has(r.front().key)) {
            equal = false;
            fprintf(stderr, "A does not have %x which is in B\n", r.front().key);
        }
    }
    return equal;
}

static bool
SetsAreEqual(IntSet &am, IntSet &bm)
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
AddLowKeys(IntMap *am, IntMap *bm, int seed)
{
    size_t i = 0;
    srand(seed);
    while (i < TestSize) {
        uint32_t n = rand() & 0x0000FFFF;
        if (!am->has(n)) {
            if (bm->has(n))
                return false;
            am->putNew(n, n);
            bm->putNew(n, n);
            i++;
        }
    }
    return true;
}

static bool
AddLowKeys(IntSet *as, IntSet *bs, int seed)
{
    size_t i = 0;
    srand(seed);
    while (i < TestSize) {
        uint32_t n = rand() & 0x0000FFFF;
        if (!as->has(n)) {
            if (bs->has(n))
                return false;
            as->putNew(n);
            bs->putNew(n);
            i++;
        }
    }
    return true;
}

template <class NewKeyFunction>
static bool
SlowRekey(IntMap *m) {
    IntMap tmp;
    tmp.init();

    for (IntMap::Range r = m->all(); !r.empty(); r.popFront()) {
        if (NewKeyFunction::shouldBeRemoved(r.front().key))
            continue;
        uint32_t hi = NewKeyFunction::rekey(r.front().key);
        if (tmp.has(hi))
            return false;
        tmp.putNew(hi, r.front().value);
    }

    m->clear();
    for (IntMap::Range r = tmp.all(); !r.empty(); r.popFront()) {
        m->putNew(r.front().key, r.front().value);
    }

    return true;
}

template <class NewKeyFunction>
static bool
SlowRekey(IntSet *s) {
    IntSet tmp;
    tmp.init();

    for (IntSet::Range r = s->all(); !r.empty(); r.popFront()) {
        if (NewKeyFunction::shouldBeRemoved(r.front()))
            continue;
        uint32_t hi = NewKeyFunction::rekey(r.front());
        if (tmp.has(hi))
            return false;
        tmp.putNew(hi);
    }

    s->clear();
    for (IntSet::Range r = tmp.all(); !r.empty(); r.popFront()) {
        s->putNew(r.front());
    }

    return true;
}

BEGIN_TEST(testHashRekeyManual)
{
    IntMap am, bm;
    am.init();
    bm.init();
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "map1: %lu\n", i);
#endif
        CHECK(AddLowKeys(&am, &bm, i));
        CHECK(MapsAreEqual(am, bm));

        for (IntMap::Enum e(am); !e.empty(); e.popFront()) {
            uint32_t tmp = LowToHigh::rekey(e.front().key);
            if (tmp != e.front().key)
                e.rekeyFront(tmp);
        }
        CHECK(SlowRekey<LowToHigh>(&bm));

        CHECK(MapsAreEqual(am, bm));
        am.clear();
        bm.clear();
    }

    IntSet as, bs;
    as.init();
    bs.init();
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
    am.init();
    bm.init();
    for (size_t i = 0; i < TestIterations; ++i) {
#ifdef FUZZ
        fprintf(stderr, "map2: %lu\n", i);
#endif
        CHECK(AddLowKeys(&am, &bm, i));
        CHECK(MapsAreEqual(am, bm));

        for (IntMap::Enum e(am); !e.empty(); e.popFront()) {
            if (LowToHighWithRemoval::shouldBeRemoved(e.front().key)) {
                e.removeFront();
            } else {
                uint32_t tmp = LowToHighWithRemoval::rekey(e.front().key);
                if (tmp != e.front().key)
                    e.rekeyFront(tmp);
            }
        }
        CHECK(SlowRekey<LowToHighWithRemoval>(&bm));

        CHECK(MapsAreEqual(am, bm));
        am.clear();
        bm.clear();
    }

    IntSet as, bs;
    as.init();
    bs.init();
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

