/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_COMMessageFilter_h
#define mozilla_plugins_COMMessageFilter_h

#include <objidl.h>
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace plugins {

class PluginModuleChild;

class COMMessageFilter MOZ_FINAL : public IMessageFilter
{
public:
  static void Initialize(PluginModuleChild* plugin);

  COMMessageFilter(PluginModuleChild* plugin)
    : mPlugin(plugin)
  { }

  HRESULT WINAPI QueryInterface(REFIID riid, void** ppv);
  DWORD WINAPI AddRef();
  DWORD WINAPI Release();

  DWORD WINAPI HandleInComingCall(DWORD dwCallType,
                                  HTASK htaskCaller,
                                  DWORD dwTickCount,
                                  LPINTERFACEINFO lpInterfaceInfo);
  DWORD WINAPI RetryRejectedCall(HTASK htaskCallee,
                                 DWORD dwTickCount,
                                 DWORD dwRejectType);
  DWORD WINAPI MessagePending(HTASK htaskCallee,
                              DWORD dwTickCount,
                              DWORD dwPendingType);

private:
  nsAutoRefCnt mRefCnt;
  PluginModuleChild* mPlugin;
  nsRefPtr<IMessageFilter> mPreviousFilter;
};

} // namespace plugins
} // namespace mozilla

#endif // COMMessageFilter_h
