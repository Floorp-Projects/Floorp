/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/EnsureMTA.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticLocalPtr.h"
#include "mozilla/SystemGroup.h"
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

    SystemGroup::Dispatch(
        TaskCategory::Other,
        NS_NewRunnableFunction("mscom::EnsureMTA::GetMTAThread",
                               setClearOnShutdown));

    return bgData;
  }());

  MOZ_ASSERT(sMTAData);

  return sMTAData->GetThread();
}

}  // namespace mscom
}  // namespace mozilla
