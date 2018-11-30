/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DisplayListChecker_h__
#define DisplayListChecker_h__

#include <sstream>
#include <mozilla/UniquePtr.h>

class nsDisplayList;

namespace mozilla {

class DisplayListBlueprint;

class DisplayListChecker {
 public:
  DisplayListChecker();
  DisplayListChecker(nsDisplayList* aList, const char* aName);

  ~DisplayListChecker();

  void Set(nsDisplayList* aList, const char* aName);

  explicit operator bool() const { return mBlueprint.get(); }

  // Compare this list with another one, output differences between the two
  // into aDiff.
  // Differences include: Display items from one tree for which a corresponding
  // item (same frame and per-frame key) cannot be found under corresponding
  // parent items.
  // Returns true if trees are similar, false if different.
  bool CompareList(const DisplayListChecker& aOther,
                   std::stringstream& aDiff) const;

  // Output this tree to aSs.
  void Dump(std::stringstream& aSs) const;

 private:
  UniquePtr<DisplayListBlueprint> mBlueprint;
};

}  // namespace mozilla

#endif  // DisplayListChecker_h__
