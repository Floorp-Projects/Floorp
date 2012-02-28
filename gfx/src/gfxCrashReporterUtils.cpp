/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxCrashReporterUtils.h"

#if defined(MOZ_CRASHREPORTER)
#define MOZ_GFXFEATUREREPORTER 1
#endif

#ifdef MOZ_GFXFEATUREREPORTER
#include "nsExceptionHandler.h"
#include "nsString.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"

namespace mozilla {

static nsTArray<nsCString> *gFeaturesAlreadyReported = nsnull;

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
      gFeaturesAlreadyReported = nsnull;
    }
  }
  return NS_OK;
}


void
ScopedGfxFeatureReporter::WriteAppNote(char statusChar)
{
  // LeakLog made me do this. Basically, I just wanted gFeaturesAlreadyReported to be a static nsTArray<nsCString>,
  // and LeakLog was complaining about leaks like this:
  //    leaked 1 instance of nsTArray_base with size 8 bytes
  //    leaked 7 instances of nsStringBuffer with size 8 bytes each (56 bytes total)
  // So this is a work-around using a pointer, and using a nsIObserver to deallocate on xpcom shutdown.
  // Yay for fighting bloat.
  if (!gFeaturesAlreadyReported) {
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (!observerService)
      return;
    nsRefPtr<ObserverToDestroyFeaturesAlreadyReported> observer = new ObserverToDestroyFeaturesAlreadyReported;
    nsresult rv = observerService->AddObserver(observer, "xpcom-shutdown", false);
    if (NS_FAILED(rv)) {
      observer = nsnull;
      return;
    }
    gFeaturesAlreadyReported = new nsTArray<nsCString>;
  }

  nsCAutoString featureString;
  featureString.AppendPrintf("%s%c ",
                             mFeature,
                             statusChar);

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
