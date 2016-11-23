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
  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID);
  if (!uriClassifier) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = uriClassifier->Classify(aPrincipal, aUseTrackingProtection,
                                        this, aSuccess);
  if (NS_FAILED(rv) || !*aSuccess) {
    // We treat the case where we fail to classify and the case where the
    // classifier returns successfully but doesn't perform a lookup as the
    // classification not yielding any results, so we just kill the child actor
    // without ever calling out callback in both cases.
    // This means that code using this in the child process will only get a hit
    // on its callback if some classification actually happens.
    Unused << Send__delete__(this, void_t());
  }
  return IPC_OK();
}

nsresult
URLClassifierParent::OnClassifyComplete(nsresult aRv)
{
  Unused << Send__delete__(this, aRv);
  return NS_OK;
}

void
URLClassifierParent::ActorDestroy(ActorDestroyReason aWhy)
{
}
