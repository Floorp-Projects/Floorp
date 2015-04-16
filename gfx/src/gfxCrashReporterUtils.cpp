/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxCrashReporterUtils.h"

#if defined(MOZ_CRASHREPORTER)
#define MOZ_GFXFEATUREREPORTER 1
#endif

#ifdef MOZ_GFXFEATUREREPORTER
#include "gfxCrashReporterUtils.h"
#include <string.h>                     // for strcmp
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/StaticMutex.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsError.h"                    // for NS_OK, NS_FAILED, nsresult
#include "nsExceptionHandler.h"         // for AppendAppNotesToCrashReport
#include "nsID.h"
#include "nsIEventTarget.h"             // for NS_DISPATCH_NORMAL
#include "nsIObserver.h"                // for nsIObserver, etc
#include "nsIObserverService.h"         // for nsIObserverService
#include "nsIRunnable.h"                // for nsIRunnable
#include "nsISupports.h"
#include "nsString.h"               // for nsAutoCString, nsCString, etc
#include "nsTArray.h"                   // for nsTArray
#include "nsThreadUtils.h"              // for NS_DispatchToMainThread, etc
#include "nscore.h"                     // for NS_IMETHOD, NS_IMETHODIMP, etc

namespace mozilla {

static nsTArray<nsCString> *gFeaturesAlreadyReported = nullptr;
static StaticMutex gFeaturesAlreadyReportedMutex;

class ObserverToDestroyFeaturesAlreadyReported final : public nsIObserver
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ObserverToDestroyFeaturesAlreadyReported() {}
private:
  virtual ~ObserverToDestroyFeaturesAlreadyReported() {}
};

NS_IMPL_ISUPPORTS(ObserverToDestroyFeaturesAlreadyReported,
                  nsIObserver)

NS_IMETHODIMP
ObserverToDestroyFeaturesAlreadyReported::Observe(nsISupports* aSubject,
                                                  const char* aTopic,
                                                  const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    StaticMutexAutoLock al(gFeaturesAlreadyReportedMutex);
    if (gFeaturesAlreadyReported) {
      delete gFeaturesAlreadyReported;
      gFeaturesAlreadyReported = nullptr;
    }
  }
  return NS_OK;
}

class RegisterObserverRunnable : public nsRunnable {
public:
  NS_IMETHOD Run() override {
    // LeakLog made me do this. Basically, I just wanted gFeaturesAlreadyReported to be a static nsTArray<nsCString>,
    // and LeakLog was complaining about leaks like this:
    //    leaked 1 instance of nsTArray_base with size 8 bytes
    //    leaked 7 instances of nsStringBuffer with size 8 bytes each (56 bytes total)
    // So this is a work-around using a pointer, and using a nsIObserver to deallocate on xpcom shutdown.
    // Yay for fighting bloat.
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (!observerService)
      return NS_OK;
    nsRefPtr<ObserverToDestroyFeaturesAlreadyReported> observer = new ObserverToDestroyFeaturesAlreadyReported;
    observerService->AddObserver(observer, "xpcom-shutdown", false);
    return NS_OK;
  }
};

void
ScopedGfxFeatureReporter::WriteAppNote(char statusChar)
{
  StaticMutexAutoLock al(gFeaturesAlreadyReportedMutex);

  if (!gFeaturesAlreadyReported) {
    gFeaturesAlreadyReported = new nsTArray<nsCString>;
    nsCOMPtr<nsIRunnable> r = new RegisterObserverRunnable();
    NS_DispatchToMainThread(r);
  }

  nsAutoCString featureString;
  featureString.AppendPrintf("%s%c ",
                             mFeature,
                             mStatusChar);

  if (!gFeaturesAlreadyReported->Contains(featureString)) {
    gFeaturesAlreadyReported->AppendElement(featureString);
    CrashReporter::AppendAppNotesToCrashReport(featureString);
  }
}

} // end namespace mozilla

#else

namespace mozilla {
void ScopedGfxFeatureReporter::WriteAppNote(char) {}
}

#endif
