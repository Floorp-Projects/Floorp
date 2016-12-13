/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLClassifierParent.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(URLClassifierParent, nsIURIClassifierCallback)

mozilla::ipc::IPCResult
URLClassifierParent::StartClassify(nsIPrincipal* aPrincipal,
                                   bool aUseTrackingProtection,
                                   bool* aSuccess)
{
  mStartClassifyLog = 1;
  if (*aSuccess) {
    mStartClassifyLog |= 32;
  }
  nsresult rv = NS_OK;
  // Note that in safe mode, the URL classifier service isn't available, so we
  // should handle the service not being present gracefully.
  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    mStartClassifyLog |= 2;
    rv = uriClassifier->Classify(aPrincipal, aUseTrackingProtection,
                                 this, aSuccess);
    if (NS_SUCCEEDED(rv)) {
      mStartClassifyLog |= 4;
    }
    if (*aSuccess) {
      mStartClassifyLog |= 8;
    }
  }
  if (NS_FAILED(rv) || !*aSuccess) {
    mStartClassifyLog |= 16;
    // We treat the case where we fail to classify and the case where the
    // classifier returns successfully but doesn't perform a lookup as the
    // classification not yielding any results, so we just kill the child actor
    // without ever calling out callback in both cases.
    // This means that code using this in the child process will only get a hit
    // on its callback if some classification actually happens.
    Unused << Send__delete__(this, void_t());
  }
  mStartClassifyRv = rv;
  return IPC_OK();
}

nsresult
URLClassifierParent::OnClassifyComplete(nsresult aRv)
{
#ifdef MOZ_CRASHREPORTER
  if (mStartClassifyLog & 16) {
    // We have deleted the actor previously, therefore the Send__delete__() call
    // below will crash.  Annotate the crash reports with a log of what's
    // happened in StartClassify().
    nsAutoCString log;
    log.AppendPrintf("%d", mStartClassifyLog);
    nsAutoCString rv;
    rv.AppendPrintf("0x%x", mStartClassifyRv);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("URLClassifierParentLog"),
                                       log);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("URLClassifierParentRv"),
                                       rv);
  }
#endif
  Unused << Send__delete__(this, aRv);
  return NS_OK;
}

void
URLClassifierParent::ActorDestroy(ActorDestroyReason aWhy)
{
}
