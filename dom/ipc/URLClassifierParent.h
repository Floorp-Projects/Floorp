/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLClassifierParent_h
#define mozilla_dom_URLClassifierParent_h

#include "mozilla/dom/PURLClassifierParent.h"
#include "mozilla/dom/PURLClassifierLocalParent.h"
#include "nsIURIClassifier.h"

namespace mozilla {
namespace dom {

template<typename BaseProtocol>
class URLClassifierParentBase : public nsIURIClassifierCallback,
                                public BaseProtocol
{
public:
  // nsIURIClassifierCallback.
  NS_IMETHOD OnClassifyComplete(nsresult aErrorCode,
                                const nsACString& aList,
                                const nsACString& aProvider,
                                const nsACString& aFullHash) override
  {
    if (mIPCOpen) {
      ClassifierInfo info = ClassifierInfo(nsCString(aList),
                                           nsCString(aProvider),
                                           nsCString(aFullHash));
      Unused << BaseProtocol::Send__delete__(this, info, aErrorCode);
    }
    return NS_OK;
  }

  // Custom.
  void ClassificationFailed()
  {
    if (mIPCOpen) {
      Unused << BaseProtocol::Send__delete__(this, void_t(), NS_ERROR_FAILURE);
    }
  }

protected:
  ~URLClassifierParentBase() = default;
  bool mIPCOpen = true;
};

//////////////////////////////////////////////////////////////
// URLClassifierParent

class URLClassifierParent : public URLClassifierParentBase<PURLClassifierParent>
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  mozilla::ipc::IPCResult StartClassify(nsIPrincipal* aPrincipal,
                                        bool aUseTrackingProtection,
                                        bool* aSuccess);
private:
  ~URLClassifierParent() = default;

  // Override PURLClassifierParent::ActorDestroy. We seem to unable to
  // override from the base template class.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

//////////////////////////////////////////////////////////////
// URLClassifierLocalParent

class URLClassifierLocalParent : public URLClassifierParentBase<PURLClassifierLocalParent>
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  mozilla::ipc::IPCResult StartClassify(nsIURI* aURI, const nsACString& aTables);

private:
  ~URLClassifierLocalParent() = default;

  // Override PURLClassifierParent::ActorDestroy.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_URLClassifierParent_h
