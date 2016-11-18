/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLClassifierChild_h
#define mozilla_dom_URLClassifierChild_h

#include "mozilla/dom/PURLClassifierChild.h"
#include "nsIURIClassifier.h"

namespace mozilla {
namespace dom {

class URLClassifierChild : public PURLClassifierChild
{
 public:
  URLClassifierChild() = default;

  void SetCallback(nsIURIClassifierCallback* aCallback)
  {
    mCallback = aCallback;
  }
  mozilla::ipc::IPCResult Recv__delete__(const MaybeResult& aResult) override;

 private:
  ~URLClassifierChild() = default;

  nsCOMPtr<nsIURIClassifierCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_URLClassifierChild_h
