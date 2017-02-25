/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_PlatformChild_h
#define mozilla_a11y_PlatformChild_h

#include "mozilla/mscom/Registration.h"

namespace mozilla {
namespace a11y {

class PlatformChild
{
public:
  PlatformChild();

  PlatformChild(PlatformChild&) = delete;
  PlatformChild(PlatformChild&&) = delete;
  PlatformChild& operator=(PlatformChild&) = delete;
  PlatformChild& operator=(PlatformChild&&) = delete;

private:
  UniquePtr<mozilla::mscom::RegisteredProxy> mCustomProxy;
  UniquePtr<mozilla::mscom::RegisteredProxy> mIA2Proxy;
  UniquePtr<mozilla::mscom::RegisteredProxy> mAccTypelib;
  UniquePtr<mozilla::mscom::RegisteredProxy> mMiscTypelib;
  UniquePtr<mozilla::mscom::RegisteredProxy> mSdnTypelib;
};

} // namespace mozilla
} // namespace a11y

#endif // mozilla_a11y_PlatformChild_h

