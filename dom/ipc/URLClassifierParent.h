/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLClassifierParent_h
#define mozilla_dom_URLClassifierParent_h

#include "mozilla/dom/PContent.h"
#include "mozilla/dom/PURLClassifierParent.h"
#include "mozilla/dom/PURLClassifierLocalParent.h"
#include "nsIURIClassifier.h"
#include "nsIUrlClassifierFeature.h"

namespace mozilla {
namespace dom {

class IPCURLClassifierFeature;

//////////////////////////////////////////////////////////////
// URLClassifierParent

class URLClassifierParent : public nsIURIClassifierCallback,
                            public PURLClassifierParent {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  mozilla::ipc::IPCResult StartClassify(nsIPrincipal* aPrincipal,
                                        bool* aSuccess);

  // nsIURIClassifierCallback.
  NS_IMETHOD OnClassifyComplete(nsresult aErrorCode, const nsACString& aList,
                                const nsACString& aProvider,
                                const nsACString& aFullHash) override {
    if (mIPCOpen) {
      ClassifierInfo info = ClassifierInfo(
          nsCString(aList), nsCString(aProvider), nsCString(aFullHash));
      Unused << Send__delete__(this, Some(info), aErrorCode);
    }
    return NS_OK;
  }

  // Custom.
  void ClassificationFailed() {
    if (mIPCOpen) {
      Unused << Send__delete__(this, Nothing(), NS_ERROR_FAILURE);
    }
  }

 private:
  ~URLClassifierParent() = default;

  // Override PURLClassifierParent::ActorDestroy. We seem to unable to
  // override from the base template class.
  void ActorDestroy(ActorDestroyReason aWhy) override { mIPCOpen = false; }

  bool mIPCOpen = true;
};

//////////////////////////////////////////////////////////////
// URLClassifierLocalParent

class URLClassifierLocalParent : public nsIUrlClassifierFeatureCallback,
                                 public PURLClassifierLocalParent {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  mozilla::ipc::IPCResult StartClassify(
      nsIURI* aURI, const nsTArray<IPCURLClassifierFeature>& aFeatureNames);

  // nsIUrlClassifierFeatureCallback.
  NS_IMETHOD
  OnClassifyComplete(
      const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aResults) override;

 private:
  ~URLClassifierLocalParent() = default;

  // Override PURLClassifierLocalParent::ActorDestroy.
  void ActorDestroy(ActorDestroyReason aWhy) override { mIPCOpen = false; }

  bool mIPCOpen = true;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_URLClassifierParent_h
