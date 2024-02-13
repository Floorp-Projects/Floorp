/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <array>

#include "ds/SinglyLinkedList.h"
#include "jsapi-tests/tests.h"

using namespace js;

struct IntElement {
  int value;
  IntElement* next = nullptr;

  explicit IntElement(int v) : value(v) {}
};
using TestList = SinglyLinkedList<IntElement>;

BEGIN_TEST(testSinglyLinkedList) {
  // Test empty lists.

  TestList list;
  CHECK(list.isEmpty());
  CHECK(!list.first());
  CHECK(!list.last());
  CHECK(CountList(list) == 0);

  // Test list pushBack and first/last accessors.

  list.pushBack(MakeElement(1));
  CHECK(!list.isEmpty());
  CHECK(list.first()->value == 1);
  CHECK(list.last()->value == 1);
  CHECK(CheckList<1>(list));

  list.pushBack(MakeElement(2));
  list.pushBack(MakeElement(3));
  CHECK(!list.isEmpty());
  CHECK(list.first()->value == 1);
  CHECK(list.last()->value == 3);
  CHECK((CheckList<1, 2, 3>(list)));

  // Test popFront.

  IntElement* e = list.popFront();
  CHECK(e->value == 1);
  js_delete(e);
  CHECK(list.first()->value == 2);
  CHECK((CheckList<2, 3>(list)));

  e = list.popFront();
  CHECK(e->value == 2);
  js_delete(e);
  CHECK(list.first()->value == 3);

  //  Test pushFront.

  list.pushFront(MakeElement(2));
  CHECK(list.first()->value == 2);
  CHECK((CheckList<2, 3>(list)));

  list.pushFront(MakeElement(1));
  CHECK(list.first()->value == 1);
  CHECK((CheckList<1, 2, 3>(list)));

  // Test moveFrontToBack.

  list.moveFrontToBack();
  CHECK(list.first()->value == 2);
  CHECK(list.last()->value == 1);
  CHECK((CheckList<2, 3, 1>(list)));
  list.moveFrontToBack();
  list.moveFrontToBack();
  CHECK((CheckList<1, 2, 3>(list)));

  // Test move constructor and assignment.

  TestList list2(std::move(list));
  CHECK(list.isEmpty());
  CHECK((CheckList<1, 2, 3>(list2)));

  list = std::move(list2);
  CHECK(list2.isEmpty());
  CHECK((CheckList<1, 2, 3>(list)));

  // Test release.

  IntElement* head = list.release();
  CHECK(list.isEmpty());
  CHECK(head->value == 1);
  CHECK(head->next->value == 2);
  CHECK(head->next->next->value == 3);
  CHECK(!head->next->next->next);

  // Test construct from linked list.

  list = TestList(head, head->next->next);
  CHECK((CheckList<1, 2, 3>(list)));

  // Test append.

  CHECK(list2.isEmpty());
  list.append(std::move(list2));
  CHECK((CheckList<1, 2, 3>(list)));
  CHECK(list2.isEmpty());

  list2.pushBack(MakeElement(4));
  list2.pushBack(MakeElement(5));
  list2.pushBack(MakeElement(6));
  list.append(std::move(list2));
  CHECK((CheckList<1, 2, 3, 4, 5, 6>(list)));
  CHECK(list2.isEmpty());

  // Cleanup.

  while (!list.isEmpty()) {
    js_delete(list.popFront());
  }
  CHECK(list.isEmpty());
  CHECK(!list.first());
  CHECK(!list.last());
  CHECK(CountList(list) == 0);

  return true;
}

IntElement* MakeElement(int value) {
  IntElement* element = js_new<IntElement>(value);
  MOZ_RELEASE_ASSERT(element);
  return element;
}

size_t CountList(const TestList& list) {
  size_t i = 0;
  for (auto iter = list.iter(); !iter.done(); iter.next()) {
    i++;
  }
  return i;
}

template <int... Values>
bool CheckList(const TestList& list) {
  int expected[] = {Values...};
  constexpr size_t N = std::size(expected);

  size_t i = 0;
  for (auto iter = list.iter(); !iter.done(); iter.next()) {
    CHECK(i < N);
    CHECK(iter->value == expected[i]);
    i++;
  }

  CHECK(i == N);

  return true;
}

END_TEST(testSinglyLinkedList)
