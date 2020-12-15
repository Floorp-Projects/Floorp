/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_quota_quotainfo_h__
#define dom_quota_quotainfo_h__

#include <utility>
#include "nsString.h"

namespace mozilla::dom::quota {

struct GroupAndOrigin {
  nsCString mGroup;
  nsCString mOrigin;
};

struct QuotaInfo : GroupAndOrigin {
  nsCString mSuffix;

  QuotaInfo() = default;
  QuotaInfo(nsCString aSuffix, nsCString aGroup, nsCString aOrigin)
      : GroupAndOrigin{std::move(aGroup), std::move(aOrigin)},
        mSuffix{std::move(aSuffix)} {}
};

}  // namespace mozilla::dom::quota

#endif
