/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShadowParts_h
#define mozilla_ShadowParts_h

#include "nsAtomHashKeys.h"
#include "nsTHashtable.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsStringFwd.h"

namespace mozilla {

class ShadowParts final {
 public:
  ShadowParts(ShadowParts&&) = default;
  ShadowParts(const ShadowParts&) = delete;
  static ShadowParts Parse(const nsAString&);

  using PartList = AutoTArray<RefPtr<nsAtom>, 1>;

  PartList* Get(nsAtom* aName) const { return mMappings.Get(aName); }

  nsAtom* GetReverse(nsAtom* aName) const {
    return mReverseMappings.GetWeak(aName);
  }

#ifdef DEBUG
  void Dump() const;
#endif

 private:
  ShadowParts() = default;

  // TODO(emilio): If the two hashtables take a lot of memory we should consider
  // just using an nsTArray<Pair<>> or something.
  nsClassHashtable<nsAtomHashKey, PartList> mMappings;
  nsRefPtrHashtable<nsAtomHashKey, nsAtom> mReverseMappings;
};

}  // namespace mozilla

#endif  // mozilla_ShadowParts_h
