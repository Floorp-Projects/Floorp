/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxCrashReporterUtils.h"

#if defined(MOZ_CRASHREPORTER)
#define MOZ_GFXFEATUREREPORTER 1
#endif

#ifdef MOZ_GFXFEATUREREPORTER
#include "nsExceptionHandler.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"

namespace mozilla {

static nsTArray<nsCString> *gFeaturesAlreadyReported = nullptr;

class ObserverToDestroyFeaturesAlreadyReported : public nsIObserver
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ObserverToDestroyFeaturesAlreadyReported() {}
  virtual ~ObserverToDestroyFeaturesAlreadyReported() {}
};

NS_IMPL_ISUPPORTS1(ObserverToDestroyFeaturesAlreadyReported,
                   nsIObserver)

NS_IMETHODIMP
ObserverToDestroyFeaturesAlreadyReported::Observe(nsISupports* aSubject,
                                                  const char* aTopic,
                                                  const PRUnichar* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    if (gFeaturesAlreadyReported) {
      delete gFeaturesAlreadyReported;
      gFeaturesAlreadyReported = nullptr;
    }
  }
  return NS_OK;
}

class ScopedGfxFeatureReporter::AppNoteWritingRunnable : public nsRunnable {
public:
  AppNoteWritingRunnable(char aStatusChar, const char *aFeature) :
    mStatusChar(aStatusChar), mFeature(aFeature) {}
  NS_IMETHOD Run() { 
    // LeakLog made me do this. Basically, I just wanted gFeaturesAlreadyReported to be a static nsTArray<nsCString>,
    // and LeakLog was complaining about leaks like this:
    //    leaked 1 instance of nsTArray_base with size 8 bytes
    //    leaked 7 instances of nsStringBuffer with size 8 bytes each (56 bytes total)
    // So this is a work-around using a pointer, and using a nsIObserver to deallocate on xpcom shutdown.
    // Yay for fighting bloat.
    if (!gFeaturesAlreadyReported) {
      nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
      if (!observerService)
        return NS_OK;
      nsRefPtr<ObserverToDestroyFeaturesAlreadyReported> observer = new ObserverToDestroyFeaturesAlreadyReported;
      nsresult rv = observerService->AddObserver(observer, "xpcom-shutdown", false);
      if (NS_FAILED(rv)) {
        observer = nullptr;
        return NS_OK;
      }
      gFeaturesAlreadyReported = new nsTArray<nsCString>;
    }

    nsAutoCString featureString;
    featureString.AppendPrintf("%s%c ",
                               mFeature,
                               mStatusChar);

    if (!gFeaturesAlreadyReported->Contains(featureString)) {
      gFeaturesAlreadyReported->AppendElement(featureString);
      CrashReporter::AppendAppNotesToCrashReport(featureString);
    }
    return NS_OK;
  }
private:
  char mStatusChar;
  const char *mFeature;
};

void
ScopedGfxFeatureReporter::WriteAppNote(char statusChar)
{
  nsCOMPtr<nsIRunnable> r = new AppNoteWritingRunnable(statusChar, mFeature);
  NS_DispatchToMainThread(r.get(), NS_DISPATCH_NORMAL);
}

} // end namespace mozilla

#else

namespace mozilla {
void ScopedGfxFeatureReporter::WriteAppNote(char) {}
}

#endif
