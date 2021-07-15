/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerMarkers.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

#include <objbase.h>
#include <objidlbase.h>

// {9DBE6B28-E5E7-4FDE-AF00-9404604E74DC}
static const GUID GUID_MozProfilerMarkerExtension = {
    0x9dbe6b28, 0xe5e7, 0x4fde, {0xaf, 0x0, 0x94, 0x4, 0x60, 0x4e, 0x74, 0xdc}};

namespace {

class ProfilerMarkerChannelHook final : public IChannelHook {
  ~ProfilerMarkerChannelHook() = default;

 public:
  ProfilerMarkerChannelHook() : mRefCnt(0) {}

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  /**
   * IChannelHook exposes six methods: The Client* methods are called when
   * a client is sending an IPC request, whereas the Server* methods are called
   * when a server is receiving an IPC request.
   *
   * For our purposes, we only care about the client-side methods. The COM
   * runtime invokes the methods in the following order:
   * 1. ClientGetSize, where the hook specifies the size of its payload;
   * 2. ClientFillBuffer, where the hook fills the channel's buffer with its
   *    payload information. NOTE: This method is only called when ClientGetSize
   *    specifies a non-zero payload size. For our purposes, since we are not
   *    sending a payload, this method will never be called!
   * 3. ClientNotify, when the response has been received from the server.
   *
   * Since we want to use these hooks to record the beginning and end of a COM
   * IPC call, we use ClientGetSize for logging the start, and ClientNotify for
   * logging the end.
   *
   * Finally, our implementation responds to any request matching our extension
   * ID, however we only care about main thread COM calls.
   */

  // IChannelHook
  STDMETHODIMP_(void)
  ClientGetSize(REFGUID aExtensionId, REFIID aIid,
                ULONG* aOutDataSize) override;

  // No-op (see the large comment above)
  STDMETHODIMP_(void)
  ClientFillBuffer(REFGUID aExtensionId, REFIID aIid, ULONG* aDataSize,
                   void* aDataBuf) override {}

  STDMETHODIMP_(void)
  ClientNotify(REFGUID aExtensionId, REFIID aIid, ULONG aDataSize,
               void* aDataBuffer, DWORD aDataRep, HRESULT aFault) override;

  // We don't care about the server-side notifications, so leave as no-ops.
  STDMETHODIMP_(void)
  ServerNotify(REFGUID aExtensionId, REFIID aIid, ULONG aDataSize,
               void* aDataBuf, DWORD aDataRep) override {}

  STDMETHODIMP_(void)
  ServerGetSize(REFGUID aExtensionId, REFIID aIid, HRESULT aFault,
                ULONG* aOutDataSize) override {}

  STDMETHODIMP_(void)
  ServerFillBuffer(REFGUID aExtensionId, REFIID aIid, ULONG* aDataSize,
                   void* aDataBuf, HRESULT aFault) override {}

 private:
  void BuildMarkerName(REFIID aIid, nsACString& aOutMarkerName);

 private:
  mozilla::Atomic<ULONG> mRefCnt;
};

HRESULT ProfilerMarkerChannelHook::QueryInterface(REFIID aIid,
                                                  void** aOutInterface) {
  if (aIid == IID_IChannelHook || aIid == IID_IUnknown) {
    RefPtr<IChannelHook> ptr(this);
    ptr.forget(aOutInterface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

ULONG ProfilerMarkerChannelHook::AddRef() { return ++mRefCnt; }

ULONG ProfilerMarkerChannelHook::Release() {
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }

  return result;
}

void ProfilerMarkerChannelHook::BuildMarkerName(REFIID aIid,
                                                nsACString& aOutMarkerName) {
  aOutMarkerName.AssignLiteral("ORPC Call for ");

  nsAutoCString iidStr;
  mozilla::mscom::DiagnosticNameForIID(aIid, iidStr);
  aOutMarkerName.Append(iidStr);
}

void ProfilerMarkerChannelHook::ClientGetSize(REFGUID aExtensionId, REFIID aIid,
                                              ULONG* aOutDataSize) {
  if (aExtensionId == GUID_MozProfilerMarkerExtension) {
    if (NS_IsMainThread()) {
      nsAutoCString markerName;
      BuildMarkerName(aIid, markerName);
      PROFILER_MARKER(markerName, IPC, mozilla::MarkerTiming::IntervalStart(),
                      Tracing, "MSCOM");
    }

    if (aOutDataSize) {
      // We don't add any payload data to the channel
      *aOutDataSize = 0UL;
    }
  }
}

void ProfilerMarkerChannelHook::ClientNotify(REFGUID aExtensionId, REFIID aIid,
                                             ULONG aDataSize, void* aDataBuffer,
                                             DWORD aDataRep, HRESULT aFault) {
  if (NS_IsMainThread() && aExtensionId == GUID_MozProfilerMarkerExtension) {
    nsAutoCString markerName;
    BuildMarkerName(aIid, markerName);
    PROFILER_MARKER(markerName, IPC, mozilla::MarkerTiming::IntervalEnd(),
                    Tracing, "MSCOM");
  }
}

}  // anonymous namespace

static void RegisterChannelHook() {
  RefPtr<ProfilerMarkerChannelHook> hook(new ProfilerMarkerChannelHook());
  mozilla::DebugOnly<HRESULT> hr =
      ::CoRegisterChannelHook(GUID_MozProfilerMarkerExtension, hook);
  MOZ_ASSERT(SUCCEEDED(hr));
}

namespace {

class ProfilerStartupObserver final : public nsIObserver {
  ~ProfilerStartupObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(ProfilerStartupObserver, nsIObserver)

NS_IMETHODIMP ProfilerStartupObserver::Observe(nsISupports* aSubject,
                                               const char* aTopic,
                                               const char16_t* aData) {
  if (strcmp(aTopic, "profiler-started")) {
    return NS_OK;
  }

  RegisterChannelHook();

  // Once we've set the channel hook, we don't care about this notification
  // anymore; our channel hook will remain set for the lifetime of the process.
  nsCOMPtr<nsIObserverService> obsServ(mozilla::services::GetObserverService());
  MOZ_ASSERT(!!obsServ);
  if (!obsServ) {
    return NS_OK;
  }

  obsServ->RemoveObserver(this, "profiler-started");
  return NS_OK;
}

}  // anonymous namespace

namespace mozilla {
namespace mscom {

void InitProfilerMarkers() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }

  if (profiler_is_active()) {
    // If the profiler is already running, we'll immediately register our
    // channel hook.
    RegisterChannelHook();
    return;
  }

  // The profiler is not running yet. To avoid unnecessary invocations of the
  // channel hook, we won't bother with installing it until the profiler starts.
  // Set up an observer to watch for this.
  nsCOMPtr<nsIObserverService> obsServ(mozilla::services::GetObserverService());
  MOZ_ASSERT(!!obsServ);
  if (!obsServ) {
    return;
  }

  nsCOMPtr<nsIObserver> obs(new ProfilerStartupObserver());
  obsServ->AddObserver(obs, "profiler-started", false);
}

}  // namespace mscom
}  // namespace mozilla
