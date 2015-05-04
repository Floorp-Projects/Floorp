/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_persistencetype_h__
#define mozilla_dom_quota_persistencetype_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/dom/StorageTypeBinding.h"

BEGIN_QUOTA_NAMESPACE

enum PersistenceType
{
  PERSISTENCE_TYPE_PERSISTENT = 0,
  PERSISTENCE_TYPE_TEMPORARY,
  PERSISTENCE_TYPE_DEFAULT,

  // Only needed for IPC serialization helper, should never be used in code.
  PERSISTENCE_TYPE_INVALID
};

inline void
PersistenceTypeToText(PersistenceType aPersistenceType, nsACString& aText)
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
      aText.AssignLiteral("persistent");
      return;
    case PERSISTENCE_TYPE_TEMPORARY:
      aText.AssignLiteral("temporary");
      return;
    case PERSISTENCE_TYPE_DEFAULT:
      aText.AssignLiteral("default");
      return;

    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

inline PersistenceType
PersistenceTypeFromText(const nsACString& aText)
{
  if (aText.EqualsLiteral("persistent")) {
    return PERSISTENCE_TYPE_PERSISTENT;
  }

  if (aText.EqualsLiteral("temporary")) {
    return PERSISTENCE_TYPE_TEMPORARY;
  }

  if (aText.EqualsLiteral("default")) {
    return PERSISTENCE_TYPE_DEFAULT;
  }

  MOZ_CRASH("Should never get here!");
}

inline nsresult
NullablePersistenceTypeFromText(const nsACString& aText,
                                Nullable<PersistenceType>* aPersistenceType)
{
  if (aText.IsVoid()) {
    *aPersistenceType = Nullable<PersistenceType>();
    return NS_OK;
  }

  if (aText.EqualsLiteral("persistent")) {
    *aPersistenceType = Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT);
    return NS_OK;
  }

  if (aText.EqualsLiteral("temporary")) {
    *aPersistenceType = Nullable<PersistenceType>(PERSISTENCE_TYPE_TEMPORARY);
    return NS_OK;
  }

  if (aText.EqualsLiteral("default")) {
    *aPersistenceType = Nullable<PersistenceType>(PERSISTENCE_TYPE_DEFAULT);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

inline mozilla::dom::StorageType
PersistenceTypeToStorage(PersistenceType aPersistenceType)
{
  return mozilla::dom::StorageType(static_cast<int>(aPersistenceType));
}

inline PersistenceType
PersistenceTypeFromStorage(const Optional<mozilla::dom::StorageType>& aStorage)
{
  if (aStorage.WasPassed()) {
    return PersistenceType(static_cast<int>(aStorage.Value()));
  }

  return PERSISTENCE_TYPE_DEFAULT;
}

inline PersistenceType
ComplementaryPersistenceType(PersistenceType aPersistenceType)
{
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT ||
             aPersistenceType == PERSISTENCE_TYPE_TEMPORARY);

  if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
    return PERSISTENCE_TYPE_TEMPORARY;
  }

  return PERSISTENCE_TYPE_DEFAULT;
}

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_persistencetype_h__
