/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/gfx/IterableArena.h"
#include <string>

using namespace mozilla;
using namespace mozilla::gfx;

#ifdef A
#  undef A
#endif

#ifdef B
#  undef B
#endif

// to avoid having symbols that collide easily like A and B in the global
// namespace
namespace test_arena {

class A;
class B;

class Base {
 public:
  virtual ~Base() = default;
  virtual A* AsA() { return nullptr; }
  virtual B* AsB() { return nullptr; }
};

static int sDtorItemA = 0;
static int sDtorItemB = 0;

class A : public Base {
 public:
  virtual A* AsA() override { return this; }

  explicit A(uint64_t val) : mVal(val) {}
  ~A() { ++sDtorItemA; }

  uint64_t mVal;
};

class B : public Base {
 public:
  virtual B* AsB() override { return this; }

  explicit B(const string& str) : mVal(str) {}
  ~B() { ++sDtorItemB; }

  std::string mVal;
};

struct BigStruct {
  uint64_t mVal;
  uint8_t data[120];

  explicit BigStruct(uint64_t val) : mVal(val) {}
};

static void TestArenaAlloc(IterableArena::ArenaType aType) {
  sDtorItemA = 0;
  sDtorItemB = 0;
  IterableArena arena(aType, 256);

  // An empty arena has no items to iterate over.
  {
    int iterations = 0;
    arena.ForEach([&](void* item) { iterations++; });
    ASSERT_EQ(iterations, 0);
  }

  auto a1 = arena.Alloc<A>(42);
  auto b1 = arena.Alloc<B>("Obladi oblada");
  auto a2 = arena.Alloc<A>(1337);
  auto b2 = arena.Alloc<B>("Yellow submarine");
  auto b3 = arena.Alloc<B>("She's got a ticket to ride");

  // Alloc returns a non-negative offset if the allocation succeeded.
  ASSERT_TRUE(a1 >= 0);
  ASSERT_TRUE(a2 >= 0);
  ASSERT_TRUE(b1 >= 0);
  ASSERT_TRUE(b2 >= 0);
  ASSERT_TRUE(b3 >= 0);

  ASSERT_TRUE(arena.GetStorage(a1) != nullptr);
  ASSERT_TRUE(arena.GetStorage(a2) != nullptr);
  ASSERT_TRUE(arena.GetStorage(b1) != nullptr);
  ASSERT_TRUE(arena.GetStorage(b2) != nullptr);
  ASSERT_TRUE(arena.GetStorage(b3) != nullptr);

  ASSERT_TRUE(((Base*)arena.GetStorage(a1))->AsA() != nullptr);
  ASSERT_TRUE(((Base*)arena.GetStorage(a2))->AsA() != nullptr);

  ASSERT_TRUE(((Base*)arena.GetStorage(b1))->AsB() != nullptr);
  ASSERT_TRUE(((Base*)arena.GetStorage(b2))->AsB() != nullptr);
  ASSERT_TRUE(((Base*)arena.GetStorage(b3))->AsB() != nullptr);

  ASSERT_EQ(((Base*)arena.GetStorage(a1))->AsA()->mVal, (uint64_t)42);
  ASSERT_EQ(((Base*)arena.GetStorage(a2))->AsA()->mVal, (uint64_t)1337);

  ASSERT_EQ(((Base*)arena.GetStorage(b1))->AsB()->mVal,
            std::string("Obladi oblada"));
  ASSERT_EQ(((Base*)arena.GetStorage(b2))->AsB()->mVal,
            std::string("Yellow submarine"));
  ASSERT_EQ(((Base*)arena.GetStorage(b3))->AsB()->mVal,
            std::string("She's got a ticket to ride"));

  {
    int iterations = 0;
    arena.ForEach([&](void* item) { iterations++; });
    ASSERT_EQ(iterations, 5);
  }

  // Typically, running the destructors of the elements in the arena will is
  // done manually like this:
  arena.ForEach([](void* item) { ((Base*)item)->~Base(); });
  arena.Clear();
  ASSERT_EQ(sDtorItemA, 2);
  ASSERT_EQ(sDtorItemB, 3);

  // An empty arena has no items to iterate over (we just cleared it).
  {
    int iterations = 0;
    arena.ForEach([&](void* item) { iterations++; });
    ASSERT_EQ(iterations, 0);
  }
}

static void TestArenaLimit(IterableArena::ArenaType aType,
                           bool aShouldReachLimit) {
  IterableArena arena(aType, 128);

  // A non-growable arena should return a negative offset when running out
  // of space, without crashing.
  // We should not run out of space with a growable arena (unless the os is
  // running out of memory but this isn't expected for this test).
  bool reachedLimit = false;
  for (int i = 0; i < 100; ++i) {
    auto offset = arena.Alloc<A>(42);
    if (offset < 0) {
      reachedLimit = true;
      break;
    }
  }
  ASSERT_EQ(reachedLimit, aShouldReachLimit);
}

}  // namespace test_arena

using namespace test_arena;

TEST(Moz2D, FixedArena)
{
  TestArenaAlloc(IterableArena::FIXED_SIZE);
  TestArenaLimit(IterableArena::FIXED_SIZE, true);
}

TEST(Moz2D, GrowableArena)
{
  TestArenaAlloc(IterableArena::GROWABLE);
  TestArenaLimit(IterableArena::GROWABLE, false);

  IterableArena arena(IterableArena::GROWABLE, 16);
  // sizeof(BigStruct) is more than twice the initial capacity, make sure that
  // this doesn't blow everything up, since the arena doubles its storage size
  // each time it grows (until it finds a size that fits).
  auto a = arena.Alloc<BigStruct>(1);
  auto b = arena.Alloc<BigStruct>(2);
  auto c = arena.Alloc<BigStruct>(3);

  // Offsets should also still point to the appropriate values after
  // reallocation.
  ASSERT_EQ(((BigStruct*)arena.GetStorage(a))->mVal, (uint64_t)1);
  ASSERT_EQ(((BigStruct*)arena.GetStorage(b))->mVal, (uint64_t)2);
  ASSERT_EQ(((BigStruct*)arena.GetStorage(c))->mVal, (uint64_t)3);

  arena.Clear();
}
