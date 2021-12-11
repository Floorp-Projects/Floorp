/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RecurseGuard_h__
#define RecurseGuard_h__

#include "Utils.h"

// This class acts as a tracker for avoiding infinite recursion when traversing
// chains in CFGs etc.
//
// Constructing a RecurseGuard sets up a shared backing store which tracks the
// currently observed objects. Whenever recursing, use RecurseGuard.recurse(T)
// to construct another RecurseGuard with the same backing store.
//
// The RecurseGuard object will unregister its object when it is destroyed, and
// has a method `isRepeat()` which will return `true` if the item was already
// seen.
template <typename T> class RecurseGuard {
public:
  RecurseGuard(T Thing) : Thing(Thing), Set(new DenseSet<T>()), Repeat(false) {
    Set->insert(Thing);
  }
  RecurseGuard(T Thing, std::shared_ptr<DenseSet<T>> &Set)
      : Thing(Thing), Set(Set), Repeat(false) {
    Repeat = !Set->insert(Thing).second;
  }
  RecurseGuard(const RecurseGuard &) = delete;
  RecurseGuard(RecurseGuard &&Other)
      : Thing(Other.Thing), Set(Other.Set), Repeat(Other.Repeat) {
    Other.Repeat = true;
  }
  ~RecurseGuard() {
    if (!Repeat) {
      Set->erase(Thing);
    }
  }

  bool isRepeat() { return Repeat; }

  T get() { return Thing; }

  operator T() { return Thing; }

  T operator->() { return Thing; }

  RecurseGuard recurse(T NewThing) { return RecurseGuard(NewThing, Set); }

private:
  T Thing;
  std::shared_ptr<DenseSet<T>> Set;
  bool Repeat;
};

#endif // RecurseGuard_h__
