/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#ifndef mozilla_a11y_AccessibleHandlerControl_h
#define mozilla_a11y_AccessibleHandlerControl_h

#include "Factory.h"
#include "HandlerData.h"
#include "mozilla/mscom/Registration.h"

namespace mozilla {
namespace a11y {

class AccessibleHandlerControl final : public IHandlerControl
{
public:
  static HRESULT Create(AccessibleHandlerControl** aOutObject);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IHandlerControl
  STDMETHODIMP Invalidate() override;

  uint32_t GetCacheGen() const
  {
    return mCacheGen;
  }

  HRESULT GetHandlerTypeInfo(ITypeInfo** aOutTypeInfo);

private:
  AccessibleHandlerControl();
  ~AccessibleHandlerControl() = default;

  ULONG mRefCnt;
  uint32_t mCacheGen;
  UniquePtr<mscom::RegisteredProxy> mIA2Proxy;
  UniquePtr<mscom::RegisteredProxy> mHandlerProxy;
};

extern mscom::SingletonFactory<AccessibleHandlerControl> gControlFactory;

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_AccessibleHandlerControl_h
