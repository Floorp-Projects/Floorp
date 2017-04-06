/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include "AccessibleHandlerControl.h"

#include "AccessibleHandler.h"

#include "AccessibleEventId.h"

#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace a11y {

mscom::SingletonFactory<AccessibleHandlerControl> gControlFactory;

HRESULT
AccessibleHandlerControl::Create(AccessibleHandlerControl** aOutObject)
{
  if (!aOutObject) {
    return E_INVALIDARG;
  }

  RefPtr<AccessibleHandlerControl> ctl(new AccessibleHandlerControl());
  ctl.forget(aOutObject);
  return S_OK;
}

AccessibleHandlerControl::AccessibleHandlerControl()
  : mRefCnt(0)
  , mCacheGen(0)
  , mIA2Proxy(mscom::RegisterProxy(L"ia2marshal.dll"))
  , mHandlerProxy(mscom::RegisterProxy())
{
  MOZ_ASSERT(mIA2Proxy);
}

HRESULT
AccessibleHandlerControl::QueryInterface(REFIID aIid, void** aOutInterface)
{
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  if (aIid == IID_IUnknown || aIid == IID_IHandlerControl) {
    RefPtr<IHandlerControl> ctl(this);
    ctl.forget(aOutInterface);
    return S_OK;
  }

  *aOutInterface = nullptr;
  return E_NOINTERFACE;
}

ULONG
AccessibleHandlerControl::AddRef()
{
  return ++mRefCnt;
}

ULONG
AccessibleHandlerControl::Release()
{
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }
  return result;
}

HRESULT
AccessibleHandlerControl::Invalidate()
{
  ++mCacheGen;
  return S_OK;
}

HRESULT
AccessibleHandlerControl::GetHandlerTypeInfo(ITypeInfo** aOutTypeInfo)
{
  if (!mHandlerProxy) {
    return E_UNEXPECTED;
  }

  return mHandlerProxy->GetTypeInfoForGuid(CLSID_AccessibleHandler,
                                           aOutTypeInfo);
}

} // namespace a11y
} // namespace mozilla
