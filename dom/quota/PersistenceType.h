/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_persistencetype_h__
#define mozilla_dom_quota_persistencetype_h__

#include <cstdint>
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/fallible.h"
#include "nsStringFwd.h"

class nsIFile;

namespace mozilla::dom::quota {

enum PersistenceType {
  PERSISTENCE_TYPE_PERSISTENT = 0,
  PERSISTENCE_TYPE_TEMPORARY,
  PERSISTENCE_TYPE_DEFAULT,

  // Only needed for IPC serialization helper, should never be used in code.
  PERSISTENCE_TYPE_INVALID
};

static const PersistenceType kAllPersistenceTypes[] = {
    PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
    PERSISTENCE_TYPE_DEFAULT};

static const PersistenceType kBestEffortPersistenceTypes[] = {
    PERSISTENCE_TYPE_TEMPORARY, PERSISTENCE_TYPE_DEFAULT};

bool IsValidPersistenceType(PersistenceType aPersistenceType);

bool IsBestEffortPersistenceType(const PersistenceType aPersistenceType);

nsLiteralCString PersistenceTypeToString(PersistenceType aPersistenceType);

Maybe<PersistenceType> PersistenceTypeFromString(const nsACString& aString,
                                                 const fallible_t&);

PersistenceType PersistenceTypeFromString(const nsACString& aString);

Maybe<PersistenceType> PersistenceTypeFromInt32(int32_t aInt32,
                                                const fallible_t&);

// aFile is expected to be a repository directory (not some file or directory
// within that).
Maybe<PersistenceType> PersistenceTypeFromFile(nsIFile& aFile,
                                               const fallible_t&);

inline PersistenceType ComplementaryPersistenceType(
    const PersistenceType aPersistenceType) {
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT ||
             aPersistenceType == PERSISTENCE_TYPE_TEMPORARY);

  if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
    return PERSISTENCE_TYPE_TEMPORARY;
  }

  return PERSISTENCE_TYPE_DEFAULT;
}

}  // namespace mozilla::dom::quota

#endif  // mozilla_dom_quota_persistencetype_h__
