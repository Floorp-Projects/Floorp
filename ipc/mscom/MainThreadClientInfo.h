/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadClientInfo
#define mozilla_mscom_MainThreadClientInfo

#include "mozilla/RefPtr.h"
#include "nsString.h"

#include <objidl.h>

namespace mozilla {
namespace mscom {

class MainThreadClientInfo final : public IMessageFilter
{
public:
  static HRESULT Create(MainThreadClientInfo** aOutObj);

  DWORD GetLastRemoteCallThreadId() const;
  void Detach();

  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  STDMETHODIMP_(DWORD) HandleInComingCall(DWORD aCallType, HTASK aCallerTid,
                                          DWORD aTickCount,
                                          LPINTERFACEINFO aInterfaceInfo) override;
  STDMETHODIMP_(DWORD) RetryRejectedCall(HTASK aCalleeTid, DWORD aTickCount,
                                         DWORD aRejectType) override;
  STDMETHODIMP_(DWORD) MessagePending(HTASK aCalleeTid, DWORD aTickCount,
                                      DWORD aPendingType) override;

private:
  MainThreadClientInfo();
  ~MainThreadClientInfo() = default;

private:
  ULONG mRefCnt;
  RefPtr<IMessageFilter> mPrevFilter;
  DWORD mLastRemoteCallTid;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_MainThreadClientInfo

