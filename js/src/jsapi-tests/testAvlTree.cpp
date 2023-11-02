/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <set>
#include <stdio.h>

#include "ds/AvlTree.h"

#include "jsapi-tests/tests.h"

using namespace js;

////////////////////////////////////////////////////////////////////////
//                                                                    //
// AvlTree testing interface.                                         //
//                                                                    //
////////////////////////////////////////////////////////////////////////

// The "standard" AVL interface, `class AvlTree` at the end of
// js/src/ds/AvlTree.h, is too restrictive to allow proper testing of the AVL
// tree internals.  In particular it disallows insertion of duplicate values
// and removal of non-present values, and lacks various secondary functions
// such as for counting the number of nodes.
//
// So, for testing, we wrap an alternative interface `AvlTreeTestIF` around
// the core implementation.

template <class T, class C>
class AvlTreeTestIF : public AvlTreeImpl<T, C> {
  // Shorthands for names in the implementation (parent) class.
  using Impl = AvlTreeImpl<T, C>;
  using ImplTag = typename AvlTreeImpl<T, C>::Tag;
  using ImplNode = typename AvlTreeImpl<T, C>::Node;
  using ImplResult = typename AvlTreeImpl<T, C>::Result;
  using ImplNodeAndResult = typename AvlTreeImpl<T, C>::NodeAndResult;

 public:
  explicit AvlTreeTestIF(LifoAlloc* alloc = nullptr) : Impl(alloc) {}

  // Insert `v` if it isn't already there, else leave the tree unchanged.
  // Returns true iff an insertion happened.
  bool testInsert(const T& v) {
    ImplNode* new_root = Impl::insert_worker(v);
    if (!new_root) {
      // OOM
      MOZ_CRASH();
    }
    if (uintptr_t(new_root) == uintptr_t(1)) {
      // Already present
      return false;
    }
    Impl::root_ = new_root;
    return true;
  }

  // Remove `v` if it is present.  Returns true iff a removal happened.
  bool testRemove(const T& v) {
    ImplNodeAndResult pair = Impl::delete_worker(Impl::root_, v);
    ImplNode* new_root = pair.first;
    ImplResult res = pair.second;
    if (res == ImplResult::Error) {
      // `v` isn't in the tree.
      return false;
    } else {
      Impl::root_ = new_root;
      return true;
    }
  }

  // Count number of elements
  size_t testSize_worker(ImplNode* n) const {
    if (n) {
      return 1 + testSize_worker(n->left) + testSize_worker(n->getRight());
    }
    return 0;
  }
  size_t testSize() const { return testSize_worker(Impl::root_); }

  size_t testDepth_worker(ImplNode* n) const {
    if (n) {
      size_t depthL = testDepth_worker(n->left);
      size_t depthR = testDepth_worker(n->getRight());
      return 1 + (depthL > depthR ? depthL : depthR);
    }
    return 0;
  }
  size_t testDepth() const { return testDepth_worker(Impl::root_); }

  bool testContains(const T& v) const {
    ImplNode* node = Impl::find_worker(v);
    return node != nullptr;
  }

  ImplNode* testGetRoot() const { return Impl::root_; }
  ImplNode* testGetFreeList() const { return Impl::freeList_; }

  bool testFreeListLooksValid(size_t maxElems) {
    size_t numElems = 0;
    ImplNode* node = Impl::freeList_;
    while (node) {
      numElems++;
      if (numElems > maxElems) {
        return false;
      }
      if (node->getTag() != ImplTag::Free || node->getRight() != nullptr) {
        return false;
      }
      node = node->left;
    }
    return true;
  }

  // For debugging only
 private:
  void testShow_worker(int depth, const ImplNode* node) const {
    if (node) {
      testShow_worker(depth + 1, node->getRight());
      for (int i = 0; i < depth; i++) {
        printf("   ");
      }
      char* str = node->item.show();
      printf("%s\n", str);
      free(str);
      testShow_worker(depth + 1, node->left);
    }
  }

 public:
  // For debugging only
  void testShow() const { testShow_worker(0, Impl::root_); }

  // AvlTree::Iter is also public; it's just pass-through from AvlTreeImpl.
};

////////////////////////////////////////////////////////////////////////
//                                                                    //
// AvlTree test cases.                                                //
//                                                                    //
////////////////////////////////////////////////////////////////////////

class CmpInt {
  int x_;

 public:
  explicit CmpInt(int x) : x_(x) {}
  ~CmpInt() {}
  static int compare(const CmpInt& me, const CmpInt& other) {
    if (me.x_ < other.x_) return -1;
    if (me.x_ > other.x_) return 1;
    return 0;
  }
  int get() const { return x_; }
  char* show() const {
    const size_t length = 16;
    char* str = (char*)calloc(length, 1);
    snprintf(str, length, "%d", x_);
    return str;
  }
};

bool TreeIsPlausible(const AvlTreeTestIF<CmpInt, CmpInt>& tree,
                     const std::set<int>& should_be_in_tree, int UNIV_MIN,
                     int UNIV_MAX) {
  // Same cardinality
  size_t n_in_set = should_be_in_tree.size();
  size_t n_in_tree = tree.testSize();
  if (n_in_set != n_in_tree) {
    return false;
  }

  // Tree is not wildly out of balance.  Depth should not exceed 1.44 *
  // log2(size).
  size_t tree_depth = tree.testDepth();
  size_t log2_size = 0;
  {
    size_t n = n_in_tree;
    while (n > 0) {
      n = n >> 1;
      log2_size += 1;
    }
  }
  // Actually a tighter limit than stated above.  For these test cases, the
  // tree is either perfectly balanced or within one level of being so (hence
  // the +1).
  if (tree_depth > log2_size + 1) {
    return false;
  }

  // Check that everything that should be in the tree is in it, and vice
  // versa.
  for (int i = UNIV_MIN; i < UNIV_MAX; i++) {
    bool should_be_in = should_be_in_tree.find(i) != should_be_in_tree.end();

    // Look it up with a null comparator (so `contains` compares
    // directly)
    bool is_in = tree.testContains(CmpInt(i));
    if (is_in != should_be_in) {
      return false;
    }
  }

  return true;
}

template <typename T>
bool SetContains(std::set<T> s, const T& v) {
  return s.find(v) != s.end();
}

BEGIN_TEST(testAvlTree_main) {
  static const int UNIV_MIN = 5000;
  static const int UNIV_MAX = 5999;
  static const int UNIV_SIZE = UNIV_MAX - UNIV_MIN + 1;

  LifoAlloc alloc(4096);
  AvlTreeTestIF<CmpInt, CmpInt> tree(&alloc);
  std::set<int> should_be_in_tree;

  // Add numbers to the tree, checking as we go.
  for (int i = UNIV_MIN; i < UNIV_MAX; i++) {
    // Idiotic but simple
    if (i % 3 != 0) {
      continue;
    }
    bool was_added = tree.testInsert(CmpInt(i));
    should_be_in_tree.insert(i);
    CHECK(was_added);
    CHECK(TreeIsPlausible(tree, should_be_in_tree, UNIV_MIN, UNIV_MAX));
  }

  // Then remove the middle half of the tree, also checking.
  for (int i = UNIV_MIN + UNIV_SIZE / 4; i < UNIV_MIN + 3 * (UNIV_SIZE / 4);
       i++) {
    // Note that here, we're asking to delete a bunch of numbers that aren't
    // in the tree.  It should remain valid throughout.
    bool was_removed = tree.testRemove(CmpInt(i));
    bool should_have_been_removed = SetContains(should_be_in_tree, i);
    CHECK(was_removed == should_have_been_removed);
    should_be_in_tree.erase(i);
    CHECK(TreeIsPlausible(tree, should_be_in_tree, UNIV_MIN, UNIV_MAX));
  }

  // Now add some numbers which are already in the tree.
  for (int i = UNIV_MIN; i < UNIV_MIN + UNIV_SIZE / 4; i++) {
    if (i % 3 != 0) {
      continue;
    }
    bool was_added = tree.testInsert(CmpInt(i));
    bool should_have_been_added = !SetContains(should_be_in_tree, i);
    CHECK(was_added == should_have_been_added);
    should_be_in_tree.insert(i);
    CHECK(TreeIsPlausible(tree, should_be_in_tree, UNIV_MIN, UNIV_MAX));
  }

  // Then remove all numbers from the tree, in reverse order.
  for (int ir = UNIV_MIN; ir < UNIV_MAX; ir++) {
    int i = UNIV_MIN + (UNIV_MAX - ir);
    bool was_removed = tree.testRemove(CmpInt(i));
    bool should_have_been_removed = SetContains(should_be_in_tree, i);
    CHECK(was_removed == should_have_been_removed);
    should_be_in_tree.erase(i);
    CHECK(TreeIsPlausible(tree, should_be_in_tree, UNIV_MIN, UNIV_MAX));
  }

  // Now the tree should be empty.
  CHECK(should_be_in_tree.empty());
  CHECK(tree.testSize() == 0);

  // Now delete some more stuff.  Tree should still be empty :-)
  for (int i = UNIV_MIN + 10; i < UNIV_MIN + 100; i++) {
    CHECK(should_be_in_tree.empty());
    CHECK(tree.testSize() == 0);
    bool was_removed = tree.testRemove(CmpInt(i));
    CHECK(!was_removed);
    CHECK(TreeIsPlausible(tree, should_be_in_tree, UNIV_MIN, UNIV_MAX));
  }

  // The tree root should be NULL.
  CHECK(tree.testGetRoot() == nullptr);
  CHECK(tree.testGetFreeList() != nullptr);

  // Check the freelist to the extent we can: it's non-circular, and the
  // elements look plausible.  If it's not shorter than the specified length
  // then it must have a cycle, since the tests above won't have resulted in
  // more than 400 free nodes at the end.
  CHECK(tree.testFreeListLooksValid(400 /*arbitrary*/));

  // Test iteration, first in a tree with 9 nodes.  This tests the general
  // case.
  {
    CHECK(tree.testSize() == 0);
    for (int i = 10; i < 100; i += 10) {
      bool was_inserted = tree.testInsert(CmpInt(i));
      CHECK(was_inserted);
    }

    // Test iteration across the whole tree.
    AvlTreeTestIF<CmpInt, CmpInt>::Iter iter(&tree);
    // `expect` produces (independently) the next expected number.  `remaining`
    // counts the number items of items remaining.
    int expect = 10;
    int remaining = 9;
    while (iter.hasMore()) {
      CmpInt ci = iter.next();
      CHECK(ci.get() == expect);
      expect += 10;
      remaining--;
    }
    CHECK(remaining == 0);

    // Test iteration from a specified start point.
    for (int i = 10; i < 100; i += 10) {
      for (int j = i - 1; j <= i + 1; j++) {
        // Set up `expect` and `remaining`.
        remaining = (100 - i) / 10;
        switch (j % 10) {
          case 0:
            expect = j;
            break;
          case 1:
            expect = j + 9;
            remaining--;
            break;
          case 9:
            expect = j + 1;
            break;
          default:
            MOZ_CRASH();
        }
        AvlTreeTestIF<CmpInt, CmpInt>::Iter iter(&tree, CmpInt(j));
        while (iter.hasMore()) {
          CmpInt ci = iter.next();
          CHECK(ci.get() == expect);
          expect += 10;
          remaining--;
        }
        CHECK(remaining == 0);
      }
    }
  }

  // Now with a completely empty tree.
  {
    AvlTreeTestIF<CmpInt, CmpInt> emptyTree(&alloc);
    CHECK(emptyTree.testSize() == 0);
    // Full tree iteration gets us nothing.
    AvlTreeTestIF<CmpInt, CmpInt>::Iter iter1(&emptyTree);
    CHECK(!iter1.hasMore());
    // Starting iteration with any number gets us nothing.
    AvlTreeTestIF<CmpInt, CmpInt>::Iter iter2(&emptyTree, CmpInt(42));
    CHECK(!iter2.hasMore());
  }

  // Finally with a one-element tree.
  {
    AvlTreeTestIF<CmpInt, CmpInt> unitTree(&alloc);
    bool was_inserted = unitTree.testInsert(CmpInt(1337));
    CHECK(was_inserted);
    CHECK(unitTree.testSize() == 1);
    // Try full tree iteration.
    AvlTreeTestIF<CmpInt, CmpInt>::Iter iter3(&unitTree);
    CHECK(iter3.hasMore());
    CmpInt ci = iter3.next();
    CHECK(ci.get() == 1337);
    CHECK(!iter3.hasMore());
    for (int i = 1336; i <= 1338; i++) {
      int remaining = i < 1338 ? 1 : 0;
      int expect = i < 1338 ? 1337 : 99999 /*we'll never use this*/;
      AvlTreeTestIF<CmpInt, CmpInt>::Iter iter4(&unitTree, CmpInt(i));
      while (iter4.hasMore()) {
        CmpInt ci = iter4.next();
        CHECK(ci.get() == expect);
        remaining--;
        // expect doesn't change, we only expect it (or nothing)
      }
      CHECK(remaining == 0);
    }
  }

  return true;
}
END_TEST(testAvlTree_main)
