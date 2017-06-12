/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INITGUID

#include "mozilla/a11y/HandlerProvider.h"

#include "Accessible2_3.h"
#include "HandlerData.h"
#include "HandlerData_i.c"
#include "mozilla/Assertions.h"
#include "mozilla/a11y/AccessibleWrap.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/AgileReference.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Ptr.h"
#include "mozilla/mscom/StructStream.h"
#include "mozilla/mscom/Utils.h"
#include "nsThreadUtils.h"

#include <memory.h>

namespace mozilla {
namespace a11y {

HandlerProvider::HandlerProvider(REFIID aIid,
                               mscom::InterceptorTargetPtr<IUnknown> aTarget)
  : mRefCnt(0)
  , mMutex("mozilla::a11y::HandlerProvider::mMutex")
  , mTargetUnkIid(aIid)
  , mTargetUnk(Move(aTarget))
{
}

HRESULT
HandlerProvider::QueryInterface(REFIID riid, void** ppv)
{
  if (!ppv) {
    return E_INVALIDARG;
  }

  RefPtr<IUnknown> punk;

  if (riid == IID_IUnknown || riid == IID_IGeckoBackChannel) {
    punk = static_cast<IGeckoBackChannel*>(this);
  }

  if (!punk) {
    return E_NOINTERFACE;
  }

  punk.forget(ppv);
  return S_OK;
}

ULONG
HandlerProvider::AddRef()
{
  return ++mRefCnt;
}

ULONG
HandlerProvider::Release()
{
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }
  return result;
}

HRESULT
HandlerProvider::GetHandler(NotNull<CLSID*> aHandlerClsid)
{
  if (!IsTargetInterfaceCacheable()) {
    return E_NOINTERFACE;
  }

  *aHandlerClsid = CLSID_AccessibleHandler;
  return S_OK;
}

void
HandlerProvider::GetAndSerializePayload(const MutexAutoLock&)
{
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (mSerializer) {
    return;
  }

  IA2Payload payload{};

  if (!mscom::InvokeOnMainThread("HandlerProvider::BuildIA2Data",
                                 this, &HandlerProvider::BuildIA2Data,
                                 &payload.mData) ||
      !payload.mData.mUniqueId) {
    return;
  }

  // But we set mGeckoBackChannel on the current thread which resides in the
  // MTA. This is important to ensure that COM always invokes
  // IGeckoBackChannel methods in an MTA background thread.

  RefPtr<IGeckoBackChannel> payloadRef(this);
  // AddRef/Release pair for this reference is handled by payloadRef
  payload.mGeckoBackChannel = this;

  mSerializer = MakeUnique<mscom::StructToStream>(payload, &IA2Payload_Encode);

  // Now that we have serialized payload, we should free any BSTRs that were
  // allocated in BuildIA2Data.
  ClearIA2Data(payload.mData);
}

HRESULT
HandlerProvider::GetHandlerPayloadSize(NotNull<DWORD*> aOutPayloadSize)
{
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!IsTargetInterfaceCacheable()) {
    *aOutPayloadSize = mscom::StructToStream::GetEmptySize();
    return S_OK;
  }

  MutexAutoLock lock(mMutex);

  GetAndSerializePayload(lock);

  if (!mSerializer) {
    return E_FAIL;
  }

  *aOutPayloadSize = mSerializer->GetSize();
  return S_OK;
}

void
HandlerProvider::BuildIA2Data(IA2Data* aOutIA2Data)
{
  MOZ_ASSERT(aOutIA2Data);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTargetUnk);
  MOZ_ASSERT(IsTargetInterfaceCacheable());

  RefPtr<NEWEST_IA2_INTERFACE>
    target(static_cast<NEWEST_IA2_INTERFACE*>(mTargetUnk.get()));

  // NB: get_uniqueID should be the final property retrieved in this method,
  // as its presence is used to determine whether the rest of this data
  // retrieval was successful.
  HRESULT hr = target->get_uniqueID(&aOutIA2Data->mUniqueId);
  if (FAILED(hr)) {
    ClearIA2Data(*aOutIA2Data);
  }
}

void
HandlerProvider::ClearIA2Data(IA2Data& aData)
{
  ZeroMemory(&aData, sizeof(IA2Data));
}

bool
HandlerProvider::IsTargetInterfaceCacheable()
{
  return MarshalAs(mTargetUnkIid) == NEWEST_IA2_IID;
}

HRESULT
HandlerProvider::WriteHandlerPayload(NotNull<IStream*> aStream)
{
  MutexAutoLock lock(mMutex);

  if (!mSerializer) {
    mscom::StructToStream emptyStruct;
    return emptyStruct.Write(aStream);
  }

  HRESULT hr = mSerializer->Write(aStream);

  mSerializer.reset();

  return hr;
}

REFIID
HandlerProvider::MarshalAs(REFIID aIid)
{
  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  if (aIid == IID_IDispatch || aIid == IID_IAccessible ||
      aIid == IID_IAccessible2 || aIid == IID_IAccessible2_2 ||
      aIid == IID_IAccessible2_3) {
    // This should always be the newest IA2 interface ID
    return NEWEST_IA2_IID;
  }
  // Otherwise we juse return the identity.
  return aIid;
}

HRESULT
HandlerProvider::NewInstance(REFIID aIid,
                             mscom::InterceptorTargetPtr<IUnknown> aTarget,
                             NotNull<mscom::IHandlerProvider**> aOutNewPayload)
{
  RefPtr<IHandlerProvider> newPayload(new HandlerProvider(aIid, Move(aTarget)));
  newPayload.forget(aOutNewPayload.get());
  return S_OK;
}

void
HandlerProvider::SetHandlerControlOnMainThread(DWORD aPid,
                                              mscom::ProxyUniquePtr<IHandlerControl> aCtrl)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto content = dom::ContentChild::GetSingleton();
  MOZ_ASSERT(content);

  IHandlerControlHolder holder(CreateHolderFromHandlerControl(Move(aCtrl)));
  Unused << content->SendA11yHandlerControl(aPid, holder);
}

HRESULT
HandlerProvider::put_HandlerControl(long aPid, IHandlerControl* aCtrl)
{
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!aCtrl) {
    return E_INVALIDARG;
  }

  auto ptrProxy = mscom::ToProxyUniquePtr(aCtrl);

  if (!mscom::InvokeOnMainThread("HandlerProvider::SetHandlerControlOnMainThread",
                                 this,
                                 &HandlerProvider::SetHandlerControlOnMainThread,
                                 static_cast<DWORD>(aPid), Move(ptrProxy))) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT
HandlerProvider::Refresh(IA2Data* aOutData)
{
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!mscom::InvokeOnMainThread("HandlerProvider::BuildIA2Data",
                                 this, &HandlerProvider::BuildIA2Data,
                                 aOutData)) {
    return E_FAIL;
  }

  return S_OK;
}

} // namespace a11y
} // namespace mozilla

