/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessibleWrap.h"
#include "nsIPersistentProperties2.h"

using namespace mozilla::a11y;

ProxyAccessibleWrap::ProxyAccessibleWrap(ProxyAccessible* aProxy)
  : AccessibleWrap(nullptr, nullptr)
{
  mType = eProxyType;
  mBits.proxy = aProxy;

  if (aProxy->mHasValue) {
    mStateFlags |= eHasNumericValue;
  }

  if (aProxy->mIsSelection) {
    mGenericTypes |= eSelect;
  }

  if (aProxy->mIsHyperText) {
    mGenericTypes |= eHyperText;
  }

  auto doc = reinterpret_cast<DocProxyAccessibleWrap*>(
    Proxy()->Document()->GetWrapper());
  if (doc) {
    mID = AcquireID();
    doc->AddID(mID, this);
  }
}

void
ProxyAccessibleWrap::Shutdown()
{
  auto doc = reinterpret_cast<DocProxyAccessibleWrap*>(
    Proxy()->Document()->GetWrapper());
  if (mID && doc) {
    doc->RemoveID(mID);
    ReleaseID(mID);
    mID = 0;
  }

  mBits.proxy = nullptr;
  mStateFlags |= eIsDefunct;
}
