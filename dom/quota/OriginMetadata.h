/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINMETADATA_H_
#define DOM_QUOTA_ORIGINMETADATA_H_

#include <utility>
#include "nsString.h"

namespace mozilla::dom::quota {

struct OriginMetadata {
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;

  // These explicit constructors exist to prevent accidental aggregate
  // initialization which could for example initialize mSuffix as group and
  // mGroup as origin (if only two string arguments are used).
  OriginMetadata() = default;

  OriginMetadata(nsCString aSuffix, nsCString aGroup, nsCString aOrigin)
      : mSuffix{std::move(aSuffix)},
        mGroup{std::move(aGroup)},
        mOrigin{std::move(aOrigin)} {}
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGINMETADATA_H_
