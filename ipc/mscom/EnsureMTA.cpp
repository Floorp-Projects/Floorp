/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/EnsureMTA.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

#include "private/pprthred.h"

namespace {

class EnterMTARunnable : public mozilla::Runnable
{
public:
  NS_IMETHOD Run() override
  {
    mozilla::DebugOnly<HRESULT> hr = ::CoInitializeEx(nullptr,
                                                      COINIT_MULTITHREADED);
    MOZ_ASSERT(SUCCEEDED(hr));
    return NS_OK;
  }
};

class BackgroundMTAData
{
public:
  BackgroundMTAData()
  {
    nsCOMPtr<nsIRunnable> runnable = new EnterMTARunnable();
    nsresult rv = NS_NewNamedThread("COM MTA",
                                    getter_AddRefs(mThread), runnable);
    NS_WARN_IF(NS_FAILED(rv));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  ~BackgroundMTAData()
  {
    if (mThread) {
      mThread->Dispatch(NS_NewRunnableFunction(&::CoUninitialize),
                        NS_DISPATCH_NORMAL);
      mThread->Shutdown();
    }
  }

  nsCOMPtr<nsIThread> GetThread() const
  {
    return mThread;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

} // anonymous namespace

static mozilla::StaticAutoPtr<BackgroundMTAData> sMTAData;

namespace mozilla {
namespace mscom {

/* static */ nsCOMPtr<nsIThread>
EnsureMTA::GetMTAThread()
{
  if (!sMTAData) {
    sMTAData = new BackgroundMTAData();
    ClearOnShutdown(&sMTAData, ShutdownPhase::ShutdownThreads);
  }
  return sMTAData->GetThread();
}

} // namespace mscom
} // namespace mozilla

