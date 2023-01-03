/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerDependencyFixture.h"

#include "TestHelpers.h"

namespace mozilla::dom::fs::test {

NS_IMPL_ISUPPORTS(QuotaManagerDependencyFixture::RequestResolver,
                  nsIQuotaCallback)

const quota::OriginMetadata& QuotaManagerDependencyFixture::GetOriginMetadata()
    const {
  static const auto testMetadata = GetTestOriginMetadata();
  return testMetadata;
}

nsCOMPtr<nsISerialEventTarget> QuotaManagerDependencyFixture::sBackgroundTarget;

}  // namespace mozilla::dom::fs::test
