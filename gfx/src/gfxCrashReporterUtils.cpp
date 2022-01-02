/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxCrashReporterUtils.h"
#include <string.h>                  // for strcmp
#include "mozilla/Assertions.h"      // for MOZ_ASSERT_HELPER2
#include "mozilla/SchedulerGroup.h"  // for SchedulerGroup
#include "mozilla/Services.h"        // for GetObserverService
#include "mozilla/StaticMutex.h"
#include "mozilla/mozalloc.h"    // for operator new, etc
#include "mozilla/RefPtr.h"      // for RefPtr
#include "MainThreadUtils.h"     // for NS_IsMainThread
#include "nsCOMPtr.h"            // for nsCOMPtr
#include "nsError.h"             // for NS_OK, NS_FAILED, nsresult
#include "nsExceptionHandler.h"  // for AppendAppNotesToCrashReport
#include "nsID.h"
#include "nsIObserver.h"         // for nsIObserver, etc
#include "nsIObserverService.h"  // for nsIObserverService
#include "nsIRunnable.h"         // for nsIRunnable
#include "nsISupports.h"
#include "nsTArray.h"  // for nsTArray
#include "nscore.h"    // for NS_IMETHOD, NS_IMETHODIMP, etc

namespace mozilla {

static nsTArray<nsCString>* gFeaturesAlreadyReported = nullptr;
static StaticMutex gFeaturesAlreadyReportedMutex;

class ObserverToDestroyFeaturesAlreadyReported final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ObserverToDestroyFeaturesAlreadyReported() = default;

 private:
  virtual ~ObserverToDestroyFeaturesAlreadyReported() = default;
};

NS_IMPL_ISUPPORTS(ObserverToDestroyFeaturesAlreadyReported, nsIObserver)

NS_IMETHODIMP
ObserverToDestroyFeaturesAlreadyReported::Observe(nsISupports* aSubject,
                                                  const char* aTopic,
                                                  const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    StaticMutexAutoLock al(gFeaturesAlreadyReportedMutex);
    if (gFeaturesAlreadyReported) {
      delete gFeaturesAlreadyReported;
      gFeaturesAlreadyReported = nullptr;
    }
  }
  return NS_OK;
}

class RegisterObserverRunnable : public Runnable {
 public:
  RegisterObserverRunnable() : Runnable("RegisterObserverRunnable") {}
  NS_IMETHOD Run() override {
    // LeakLog made me do this. Basically, I just wanted
    // gFeaturesAlreadyReported to be a static nsTArray<nsCString>, and LeakLog
    // was complaining about leaks like this:
    //    leaked 1 instance of nsTArray_base with size 8 bytes
    //    leaked 7 instances of nsStringBuffer with size 8 bytes each (56 bytes
    //    total)
    // So this is a work-around using a pointer, and using a nsIObserver to
    // deallocate on xpcom shutdown. Yay for fighting bloat.
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService) return NS_OK;
    RefPtr<ObserverToDestroyFeaturesAlreadyReported> observer =
        new ObserverToDestroyFeaturesAlreadyReported;
    observerService->AddObserver(observer, "xpcom-shutdown", false);
    return NS_OK;
  }
};

class AppendAppNotesRunnable : public Runnable {
 public:
  explicit AppendAppNotesRunnable(const nsACString& aFeatureStr)
      : Runnable("AppendAppNotesRunnable"), mFeatureString(aFeatureStr) {}

  NS_IMETHOD Run() override {
    CrashReporter::AppendAppNotesToCrashReport(mFeatureString);
    return NS_OK;
  }

 private:
  nsAutoCString mFeatureString;
};

void ScopedGfxFeatureReporter::WriteAppNote(char statusChar,
                                            int32_t statusNumber) {
  StaticMutexAutoLock al(gFeaturesAlreadyReportedMutex);

  if (!gFeaturesAlreadyReported) {
    gFeaturesAlreadyReported = new nsTArray<nsCString>;
    nsCOMPtr<nsIRunnable> r = new RegisterObserverRunnable();
    SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  }

  nsAutoCString featureString;
  if (statusNumber == 0) {
    featureString.AppendPrintf("%s%c ", mFeature, statusChar);
  } else {
    featureString.AppendPrintf("%s%c%d ", mFeature, statusChar, statusNumber);
  }

  if (!gFeaturesAlreadyReported->Contains(featureString)) {
    gFeaturesAlreadyReported->AppendElement(featureString);
    AppNote(featureString);
  }
}

void ScopedGfxFeatureReporter::AppNote(const nsACString& aMessage) {
  if (NS_IsMainThread()) {
    CrashReporter::AppendAppNotesToCrashReport(aMessage);
  } else {
    nsCOMPtr<nsIRunnable> r = new AppendAppNotesRunnable(aMessage);
    SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  }
}

}  // end namespace mozilla
