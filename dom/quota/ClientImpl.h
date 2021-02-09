/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CLIENTIMPL_H_
#define DOM_QUOTA_CLIENTIMPL_H_

#include "mozilla/dom/quota/DirectoryLock.h"

namespace mozilla::dom::quota {

// static
template <typename T>
bool Client::IsLockForObjectContainedInLockTable(
    const T& aObject, const DirectoryLockIdTable& aIds) {
  const auto& maybeDirectoryLock = aObject.MaybeDirectoryLockRef();

  MOZ_ASSERT(maybeDirectoryLock.isSome());

  return aIds.Has(maybeDirectoryLock->Id());
}

// static
template <typename T>
bool Client::IsLockForObjectAcquiredAndContainedInLockTable(
    const T& aObject, const DirectoryLockIdTable& aIds) {
  const auto& maybeDirectoryLock = aObject.MaybeDirectoryLockRef();

  return maybeDirectoryLock && aIds.Has(maybeDirectoryLock->Id());
}

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CLIENTIMPL_H_
