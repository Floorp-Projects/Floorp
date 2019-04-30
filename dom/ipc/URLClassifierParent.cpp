/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLClassifierParent.h"
#include "nsComponentManagerUtils.h"
#include "nsIUrlClassifierFeature.h"
#include "nsNetCID.h"
#include "mozilla/net/UrlClassifierFeatureResult.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;

/////////////////////////////////////////////////////////////////////
// URLClassifierParent.

NS_IMPL_ISUPPORTS(URLClassifierParent, nsIURIClassifierCallback)

mozilla::ipc::IPCResult URLClassifierParent::StartClassify(
    nsIPrincipal* aPrincipal, bool* aSuccess) {
  *aSuccess = false;
  nsresult rv = NS_OK;
  // Note that in safe mode, the URL classifier service isn't available, so we
  // should handle the service not being present gracefully.
  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = uriClassifier->Classify(aPrincipal, nullptr, this, aSuccess);
  }
  if (NS_FAILED(rv) || !*aSuccess) {
    // We treat the case where we fail to classify and the case where the
    // classifier returns successfully but doesn't perform a lookup as the
    // classification not yielding any results, so we just kill the child actor
    // without ever calling out callback in both cases.
    // This means that code using this in the child process will only get a hit
    // on its callback if some classification actually happens.
    *aSuccess = false;
    ClassificationFailed();
  }
  return IPC_OK();
}

/////////////////////////////////////////////////////////////////////
// URLClassifierLocalParent.

namespace {

// This class implements a nsIUrlClassifierFeature on the parent side, starting
// from an IPC data struct.
class IPCFeature final : public nsIUrlClassifierFeature {
 public:
  NS_DECL_ISUPPORTS

  IPCFeature(nsIURI* aURI, const IPCURLClassifierFeature& aFeature)
      : mURI(aURI), mIPCFeature(aFeature) {}

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName = mIPCFeature.featureName();
    return NS_OK;
  }

  NS_IMETHOD
  GetTables(nsIUrlClassifierFeature::listType,
            nsTArray<nsCString>& aTables) override {
    aTables.AppendElements(mIPCFeature.tables());
    return NS_OK;
  }

  NS_IMETHOD
  HasTable(const nsACString& aTable, nsIUrlClassifierFeature::listType,
           bool* aResult) override {
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mIPCFeature.tables().Contains(aTable);
    return NS_OK;
  }

  NS_IMETHOD
  HasHostInPreferences(const nsACString& aHost,
                       nsIUrlClassifierFeature::listType,
                       nsACString& aTableName, bool* aResult) override {
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = false;
    return NS_OK;
  }

  NS_IMETHOD
  GetSkipHostList(nsACString& aList) override {
    aList = mIPCFeature.skipHostList();
    return NS_OK;
  }

  NS_IMETHOD
  ProcessChannel(nsIChannel* aChannel, const nsTArray<nsCString>& aList,
                 const nsTArray<nsCString>& aHashes,
                 bool* aShouldContinue) override {
    NS_ENSURE_ARG_POINTER(aShouldContinue);
    *aShouldContinue = true;

    // Nothing to do here.
    return NS_OK;
  }

  NS_IMETHOD
  GetURIByListType(nsIChannel* aChannel,
                   nsIUrlClassifierFeature::listType aListType,
                   nsIURI** aURI) override {
    NS_ENSURE_ARG_POINTER(aURI);

    // This method should not be called, but we have a URI, let's return it.
    nsCOMPtr<nsIURI> uri = mURI;
    uri.forget(aURI);
    return NS_OK;
  }

 private:
  ~IPCFeature() = default;

  nsCOMPtr<nsIURI> mURI;
  IPCURLClassifierFeature mIPCFeature;
};

NS_IMPL_ISUPPORTS(IPCFeature, nsIUrlClassifierFeature)

}  // namespace

NS_IMPL_ISUPPORTS(URLClassifierLocalParent, nsIUrlClassifierFeatureCallback)

mozilla::ipc::IPCResult URLClassifierLocalParent::StartClassify(
    nsIURI* aURI, const nsTArray<IPCURLClassifierFeature>& aFeatures) {
  MOZ_ASSERT(aURI);

  nsresult rv = NS_OK;
  // Note that in safe mode, the URL classifier service isn't available, so we
  // should handle the service not being present gracefully.
  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    OnClassifyComplete(nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>());
    return IPC_OK();
  }

  nsTArray<RefPtr<nsIUrlClassifierFeature>> features;
  for (const IPCURLClassifierFeature& feature : aFeatures) {
    features.AppendElement(new IPCFeature(aURI, feature));
  }

  // Doesn't matter if we pass blacklist, whitelist or any other list.
  // IPCFeature returns always the same values.
  rv = uriClassifier->AsyncClassifyLocalWithFeatures(
      aURI, features, nsIUrlClassifierFeature::blacklist, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    OnClassifyComplete(nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>());
    return IPC_OK();
  }

  return IPC_OK();
}

NS_IMETHODIMP
URLClassifierLocalParent::OnClassifyComplete(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aResults) {
  if (mIPCOpen) {
    nsTArray<URLClassifierLocalResult> ipcResults;
    for (nsIUrlClassifierFeatureResult* result : aResults) {
      URLClassifierLocalResult* ipcResult = ipcResults.AppendElement();

      net::UrlClassifierFeatureResult* r =
          static_cast<net::UrlClassifierFeatureResult*>(result);

      ipcResult->uri() = r->URI();
      r->Feature()->GetName(ipcResult->featureName());
      ipcResult->matchingList() = r->List();
    }

    Unused << Send__delete__(this, ipcResults);
  }
  return NS_OK;
}
