/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShadowParts_h
#define mozilla_ShadowParts_h

#include "nsAtom.h"
#include "nsTHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsStringFwd.h"

namespace mozilla {

class ShadowParts final {
 public:
  ShadowParts(ShadowParts&&) = default;
  ShadowParts(const ShadowParts&) = delete;

  static ShadowParts Parse(const nsAString&);

  nsAtom* Get(nsAtom* aName) const { return mMappings.GetWeak(aName); }

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
  nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, nsAtom> mMappings;
  nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, nsAtom> mReverseMappings;
};

}  // namespace mozilla

#endif  // mozilla_ShadowParts_h
