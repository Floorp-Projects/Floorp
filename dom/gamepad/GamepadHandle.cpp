/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadHandle.h"
#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"

namespace mozilla::dom {

GamepadHandle::GamepadHandle(uint32_t aValue, GamepadHandleKind aKind)
    : mValue(aValue), mKind(aKind) {
  MOZ_RELEASE_ASSERT(mValue);
}

GamepadHandleKind GamepadHandle::GetKind() const { return mKind; }

PLDHashNumber GamepadHandle::Hash() const {
  return HashGeneric(mValue, uint8_t(mKind));
}

bool operator==(const GamepadHandle& a, const GamepadHandle& b) {
  return (a.mValue == b.mValue) && (a.mKind == b.mKind);
}

bool operator!=(const GamepadHandle& a, const GamepadHandle& b) {
  return !(a == b);
}

bool operator<(const GamepadHandle& a, const GamepadHandle& b) {
  if (a.mKind == b.mKind) {
    return a.mValue < b.mValue;
  }
  // Arbitrarily order them by kind
  return uint8_t(a.mKind) < uint8_t(b.mKind);
}

}  // namespace mozilla::dom
