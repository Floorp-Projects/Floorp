/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* thread-safe container of information for resolving url values */

#include "mozilla/URLExtraData.h"

#include "nsProxyRelease.h"
#include "NullPrincipalURI.h"

namespace mozilla {

StaticRefPtr<URLExtraData> URLExtraData::sDummy;

/* static */ void
URLExtraData::InitDummy()
{
  sDummy = new URLExtraData(NullPrincipalURI::Create(),
                            nullptr,
                            NullPrincipal::Create());
}

/* static */ void
URLExtraData::ReleaseDummy()
{
  sDummy = nullptr;
}

URLExtraData::~URLExtraData()
{
  if (!NS_IsMainThread()) {
    NS_ReleaseOnMainThread("URLExtraData::mBaseURI", mBaseURI.forget());
    NS_ReleaseOnMainThread("URLExtraData::mReferrer", mReferrer.forget());
    NS_ReleaseOnMainThread("URLExtraData::mPrincipal", mPrincipal.forget());
  }
}

} // namespace mozilla
