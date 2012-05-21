/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "COMMessageFilter.h"
#include "base/message_loop.h"
#include "mozilla/plugins/PluginModuleChild.h"

#include <stdio.h>

namespace mozilla {
namespace plugins {

HRESULT
COMMessageFilter::QueryInterface(REFIID riid, void** ppv)
{
  if (riid == IID_IUnknown || riid == IID_IMessageFilter) {
    *ppv = static_cast<IMessageFilter*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

DWORD COMMessageFilter::AddRef()
{
  ++mRefCnt;
  return mRefCnt;
}

DWORD COMMessageFilter::Release()
{
  DWORD r = --mRefCnt;
  if (0 == r)
    delete this;
  return r;
}

DWORD 
COMMessageFilter::HandleInComingCall(DWORD dwCallType,
				     HTASK htaskCaller,
				     DWORD dwTickCount,
				     LPINTERFACEINFO lpInterfaceInfo)
{
  if (mPreviousFilter)
    return mPreviousFilter->HandleInComingCall(dwCallType, htaskCaller,
					       dwTickCount, lpInterfaceInfo);
  return SERVERCALL_ISHANDLED;
}

DWORD
COMMessageFilter::RetryRejectedCall(HTASK htaskCallee,
				    DWORD dwTickCount,
				    DWORD dwRejectType)
{
  if (mPreviousFilter)
    return mPreviousFilter->RetryRejectedCall(htaskCallee, dwTickCount,
					      dwRejectType);
  return -1;
}

DWORD
COMMessageFilter::MessagePending(HTASK htaskCallee,
				 DWORD dwTickCount,
				 DWORD dwPendingType)
{
  mPlugin->FlushPendingRPCQueue();
  if (mPreviousFilter)
    return mPreviousFilter->MessagePending(htaskCallee, dwTickCount,
					   dwPendingType);
  return PENDINGMSG_WAITNOPROCESS;
}

void
COMMessageFilter::Initialize(PluginModuleChild* module)
{
  nsRefPtr<COMMessageFilter> f = new COMMessageFilter(module);
  ::CoRegisterMessageFilter(f, getter_AddRefs(f->mPreviousFilter));
}

} // namespace plugins
} // namespace mozilla
