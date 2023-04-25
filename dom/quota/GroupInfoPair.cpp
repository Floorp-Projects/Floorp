/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GroupInfoPair.h"

namespace mozilla::dom::quota {

RefPtr<GroupInfo>& GroupInfoPair::GetGroupInfoForPersistenceType(
    PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageGroupInfo;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageGroupInfo;
    case PERSISTENCE_TYPE_PRIVATE:
      return mPrivateStorageGroupInfo;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

}  // namespace mozilla::dom::quota
