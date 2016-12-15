/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLClassifierParent_h
#define mozilla_dom_URLClassifierParent_h

#include "mozilla/dom/PURLClassifierParent.h"
#include "nsIURIClassifier.h"

namespace mozilla {
namespace dom {

class URLClassifierParent : public nsIURIClassifierCallback,
                            public PURLClassifierParent
{
 public:
  URLClassifierParent() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURICLASSIFIERCALLBACK

  mozilla::ipc::IPCResult StartClassify(nsIPrincipal* aPrincipal,
                                        bool aUseTrackingProtection,
                                        bool* aSuccess);
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~URLClassifierParent() = default;

  bool mIPCOpen = true;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_URLClassifierParent_h
