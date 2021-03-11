/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_COMMONMETADATA_H_
#define DOM_QUOTA_COMMONMETADATA_H_

#include <utility>
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsString.h"

namespace mozilla::dom::quota {

struct PrincipalMetadata {
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;

  // These explicit constructors exist to prevent accidental aggregate
  // initialization which could for example initialize mSuffix as group and
  // mGroup as origin (if only two string arguments are used).
  PrincipalMetadata() = default;

  PrincipalMetadata(nsCString aSuffix, nsCString aGroup, nsCString aOrigin)
      : mSuffix{std::move(aSuffix)},
        mGroup{std::move(aGroup)},
        mOrigin{std::move(aOrigin)} {}
};

struct OriginMetadata : public PrincipalMetadata {
  PersistenceType mPersistenceType;

  OriginMetadata() = default;

  OriginMetadata(nsCString aSuffix, nsCString aGroup, nsCString aOrigin,
                 PersistenceType aPersistenceType)
      : PrincipalMetadata(std::move(aSuffix), std::move(aGroup),
                          std::move(aOrigin)),
        mPersistenceType(aPersistenceType) {}

  OriginMetadata(PrincipalMetadata&& aPrincipalMetadata,
                 PersistenceType aPersistenceType)
      : PrincipalMetadata(std::move(aPrincipalMetadata)),
        mPersistenceType(aPersistenceType) {}
};

struct FullOriginMetadata : OriginMetadata {
  bool mPersisted;
  int64_t mTimestamp;

  // XXX Only default construction is needed for now.
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_COMMONMETADATA_H_
