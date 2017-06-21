/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadClientInfo.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"

#include <objbase.h>

namespace mozilla {
namespace mscom {

/* static */
HRESULT
MainThreadClientInfo::Create(MainThreadClientInfo** aOutObj)
{
  MOZ_ASSERT(aOutObj && NS_IsMainThread());
  *aOutObj = nullptr;

  RefPtr<MainThreadClientInfo> obj(new MainThreadClientInfo());

  RefPtr<IMessageFilter> prevFilter;
  HRESULT hr = ::CoRegisterMessageFilter(obj.get(),
                                         getter_AddRefs(prevFilter));
  if (FAILED(hr)) {
    return hr;
  }

  obj->mPrevFilter = prevFilter.forget();

  obj.forget(aOutObj);
  return S_OK;
}

DWORD
MainThreadClientInfo::GetLastRemoteCallThreadId() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mLastRemoteCallTid;
}

HRESULT
MainThreadClientInfo::QueryInterface(REFIID aIid, void** aOutInterface)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  if (aIid == IID_IUnknown || aIid == IID_IMessageFilter) {
    RefPtr<IMessageFilter> filter(this);
    filter.forget(aOutInterface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

ULONG
MainThreadClientInfo::AddRef()
{
  MOZ_ASSERT(NS_IsMainThread());

  return ++mRefCnt;
}

ULONG
MainThreadClientInfo::Release()
{
  MOZ_ASSERT(NS_IsMainThread());

  ULONG newCount = --mRefCnt;
  if (!newCount) {
    delete this;
  }
  return newCount;
}

DWORD
MainThreadClientInfo::HandleInComingCall(DWORD aCallType, HTASK aCallerTid,
                                         DWORD aTickCount,
                                         LPINTERFACEINFO aInterfaceInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  // aCallerTid is an HTASK for historical reasons but is actually just a
  // regular DWORD Thread ID.
  mLastRemoteCallTid =
    static_cast<DWORD>(reinterpret_cast<uintptr_t>(aCallerTid));

  if (!mPrevFilter) {
    return SERVERCALL_ISHANDLED;
  }

  return mPrevFilter->HandleInComingCall(aCallType, aCallerTid, aTickCount,
                                         aInterfaceInfo);
}

DWORD
MainThreadClientInfo::RetryRejectedCall(HTASK aCalleeTid, DWORD aTickCount,
                                        DWORD aRejectType)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mPrevFilter) {
    return 0;
  }

  return mPrevFilter->RetryRejectedCall(aCalleeTid, aTickCount, aRejectType);
}

DWORD
MainThreadClientInfo::MessagePending(HTASK aCalleeTid, DWORD aTickCount,
                                     DWORD aPendingType)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mPrevFilter) {
    return PENDINGMSG_WAITNOPROCESS;
  }

  return mPrevFilter->MessagePending(aCalleeTid, aTickCount, aPendingType);
}

MainThreadClientInfo::MainThreadClientInfo()
  : mRefCnt(0)
  , mLastRemoteCallTid(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
MainThreadClientInfo::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  ::CoRegisterMessageFilter(mPrevFilter, nullptr);
  mPrevFilter = nullptr;
}

} // namespace mscom
} // namespace mozilla
