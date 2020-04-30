/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLClassifierChild_h
#define mozilla_dom_URLClassifierChild_h

#include "mozilla/dom/PURLClassifierChild.h"
#include "mozilla/dom/PURLClassifierLocalChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/UrlClassifierFeatureResult.h"
#include "nsIURIClassifier.h"
#include "nsIUrlClassifierFeature.h"

namespace mozilla {
namespace dom {

class URLClassifierChild : public PURLClassifierChild {
 public:
  void SetCallback(nsIURIClassifierCallback* aCallback) {
    mCallback = aCallback;
  }

  mozilla::ipc::IPCResult Recv__delete__(const Maybe<ClassifierInfo>& aInfo,
                                         const nsresult& aResult) {
    MOZ_ASSERT(mCallback);
    if (aInfo.isSome()) {
      mCallback->OnClassifyComplete(aResult, aInfo.ref().list(),
                                    aInfo.ref().provider(),
                                    aInfo.ref().fullhash());
    }
    return IPC_OK();
  }

 private:
  nsCOMPtr<nsIURIClassifierCallback> mCallback;
};

class URLClassifierLocalChild : public PURLClassifierLocalChild {
 public:
  void SetFeaturesAndCallback(
      const nsTArray<RefPtr<nsIUrlClassifierFeature>>& aFeatures,
      nsIUrlClassifierFeatureCallback* aCallback) {
    mCallback = aCallback;
    mFeatures = aFeatures.Clone();
  }

  mozilla::ipc::IPCResult Recv__delete__(
      nsTArray<URLClassifierLocalResult>&& aResults) {
    nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> finalResults;

    nsTArray<URLClassifierLocalResult> results = std::move(aResults);
    for (URLClassifierLocalResult& result : results) {
      for (nsIUrlClassifierFeature* feature : mFeatures) {
        nsAutoCString name;
        nsresult rv = feature->GetName(name);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          continue;
        }

        if (result.featureName() != name) {
          continue;
        }

        RefPtr<nsIURI> uri = result.uri();
        if (NS_WARN_IF(!uri)) {
          continue;
        }

        RefPtr<net::UrlClassifierFeatureResult> r =
            new net::UrlClassifierFeatureResult(uri, feature,
                                                result.matchingList());
        finalResults.AppendElement(r);
        break;
      }
    }

    mCallback->OnClassifyComplete(finalResults);
    return IPC_OK();
  }

 private:
  nsCOMPtr<nsIUrlClassifierFeatureCallback> mCallback;
  nsTArray<RefPtr<nsIUrlClassifierFeature>> mFeatures;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_URLClassifierChild_h
