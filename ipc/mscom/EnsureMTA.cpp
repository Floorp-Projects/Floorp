/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/EnsureMTA.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticLocalPtr.h"
#include "nsThreadUtils.h"

#include "private/pprthred.h"

namespace {

class EnterMTARunnable : public mozilla::Runnable {
 public:
  EnterMTARunnable() : mozilla::Runnable("EnterMTARunnable") {}
  NS_IMETHOD Run() override {
    mozilla::DebugOnly<HRESULT> hr =
        ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MOZ_ASSERT(SUCCEEDED(hr));
    return NS_OK;
  }
};

class BackgroundMTAData {
 public:
  BackgroundMTAData() {
    nsCOMPtr<nsIRunnable> runnable = new EnterMTARunnable();
    mozilla::DebugOnly<nsresult> rv =
        NS_NewNamedThread("COM MTA", getter_AddRefs(mThread), runnable);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_NewNamedThread failed");
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  ~BackgroundMTAData() {
    if (mThread) {
      mThread->Dispatch(
          NS_NewRunnableFunction("BackgroundMTAData::~BackgroundMTAData",
                                 &::CoUninitialize),
          NS_DISPATCH_NORMAL);
      mThread->Shutdown();
    }
  }

  nsCOMPtr<nsIThread> GetThread() const { return mThread; }

 private:
  nsCOMPtr<nsIThread> mThread;
};

}  // anonymous namespace

namespace mozilla {
namespace mscom {

/* static */
RefPtr<EnsureMTA::CreateInstanceAgileRefPromise>
EnsureMTA::CreateInstanceInternal(REFCLSID aClsid, REFIID aIid) {
  MOZ_ASSERT(IsCurrentThreadExplicitMTA());

  RefPtr<IUnknown> iface;
  HRESULT hr = ::CoCreateInstance(aClsid, nullptr, CLSCTX_INPROC_SERVER, aIid,
                                  getter_AddRefs(iface));
  if (FAILED(hr)) {
    return CreateInstanceAgileRefPromise::CreateAndReject(hr, __func__);
  }

  // We need to use the two argument constructor for AgileReference because our
  // RefPtr is not parameterized on the specific interface being requested.
  AgileReference agileRef(aIid, iface);
  if (!agileRef) {
    return CreateInstanceAgileRefPromise::CreateAndReject(agileRef.GetHResult(),
                                                          __func__);
  }

  return CreateInstanceAgileRefPromise::CreateAndResolve(std::move(agileRef),
                                                         __func__);
}

/* static */
RefPtr<EnsureMTA::CreateInstanceAgileRefPromise> EnsureMTA::CreateInstance(
    REFCLSID aClsid, REFIID aIid) {
  MOZ_ASSERT(IsCOMInitializedOnCurrentThread());

  const bool isClassOk = IsClassThreadAwareInprocServer(aClsid);
  MOZ_ASSERT(isClassOk,
             "mozilla::mscom::EnsureMTA::CreateInstance is not "
             "safe/performant/necessary to use with this CLSID. This CLSID "
             "either does not support creation from within a multithreaded "
             "apartment, or it is not an in-process server.");
  if (!isClassOk) {
    return CreateInstanceAgileRefPromise::CreateAndReject(CO_E_NOT_SUPPORTED,
                                                          __func__);
  }

  if (IsCurrentThreadExplicitMTA()) {
    // It's safe to immediately call CreateInstanceInternal
    return CreateInstanceInternal(aClsid, aIid);
  }

  // aClsid and aIid are references. Make local copies that we can put into the
  // lambda in case the sources of aClsid or aIid are not static data
  CLSID localClsid = aClsid;
  IID localIid = aIid;

  auto invoker = [localClsid,
                  localIid]() -> RefPtr<CreateInstanceAgileRefPromise> {
    return CreateInstanceInternal(localClsid, localIid);
  };

  nsCOMPtr<nsIThread> mtaThread(GetMTAThread());

  return InvokeAsync(mtaThread->SerialEventTarget(), __func__,
                     std::move(invoker));
}

/* static */
nsCOMPtr<nsIThread> EnsureMTA::GetMTAThread() {
  static StaticLocalAutoPtr<BackgroundMTAData> sMTAData(
      []() -> BackgroundMTAData* {
        BackgroundMTAData* bgData = new BackgroundMTAData();

        auto setClearOnShutdown = [ptr = &sMTAData]() -> void {
          ClearOnShutdown(ptr, ShutdownPhase::ShutdownThreads);
        };

        if (NS_IsMainThread()) {
          setClearOnShutdown();
          return bgData;
        }

        SchedulerGroup::Dispatch(
            TaskCategory::Other,
            NS_NewRunnableFunction("mscom::EnsureMTA::GetMTAThread",
                                   std::move(setClearOnShutdown)));

        return bgData;
      }());

  MOZ_ASSERT(sMTAData);

  return sMTAData->GetThread();
}

}  // namespace mscom
}  // namespace mozilla
