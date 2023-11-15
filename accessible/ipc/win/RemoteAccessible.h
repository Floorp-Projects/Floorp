/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessible_h
#define mozilla_a11y_RemoteAccessible_h

#include "mozilla/a11y/RemoteAccessibleBase.h"
#include "mozilla/a11y/Role.h"

namespace mozilla {
namespace a11y {

/**
 * Windows specific functionality for an accessibility tree node that originated
 * in the parent process.
 */
class RemoteAccessible : public RemoteAccessibleBase<RemoteAccessible> {
 public:
  RemoteAccessible(uint64_t aID,
                   DocAccessibleParent* aDoc, role aRole, AccType aType,
                   AccGenericType aGenericTypes, uint8_t aRoleMapEntryIndex)
      : RemoteAccessibleBase(aID, aDoc, aRole, aType, aGenericTypes,
                             aRoleMapEntryIndex) {
    MOZ_COUNT_CTOR(RemoteAccessible);
  }

  MOZ_COUNTED_DTOR(RemoteAccessible)

#include "mozilla/a11y/RemoteAccessibleShared.h"

 protected:
  explicit RemoteAccessible(DocAccessibleParent* aThisAsDoc)
      : RemoteAccessibleBase(aThisAsDoc) {
    MOZ_COUNT_CTOR(RemoteAccessible);
  }
};

////////////////////////////////////////////////////////////////////////////////
// RemoteAccessible downcasting method

inline RemoteAccessible* Accessible::AsRemote() {
  return IsRemote() ? static_cast<RemoteAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
