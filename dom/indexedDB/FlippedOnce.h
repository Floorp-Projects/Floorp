/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_flippedonce_h__
#define mozilla_dom_indexeddb_flippedonce_h__

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

namespace mozilla {

// A state-restricted bool, which can only be flipped once. It isn't required to
// be flipped during its lifetime.
template <bool Initial>
class FlippedOnce {
 public:
  FlippedOnce(const FlippedOnce&) = delete;
  FlippedOnce& operator=(const FlippedOnce&) = delete;
  FlippedOnce(FlippedOnce&&) = default;
  FlippedOnce& operator=(FlippedOnce&&) = default;

  constexpr FlippedOnce() = default;

  MOZ_IMPLICIT constexpr operator bool() const { return mValue; };

  constexpr void Flip() {
    MOZ_ASSERT(mValue == Initial);
    EnsureFlipped();
  }

  constexpr void EnsureFlipped() { mValue = !Initial; }

 private:
  bool mValue = Initial;
};
}  // namespace mozilla

#endif
