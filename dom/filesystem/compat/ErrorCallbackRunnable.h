/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ErrorCallbackRunnable_h
#define mozilla_dom_ErrorCallbackRunnable_h

#include "mozilla/dom/DOMError.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

class ErrorCallbackRunnable final : public Runnable
{
public:
  explicit ErrorCallbackRunnable(nsIGlobalObject* aGlobalObject,
                                 ErrorCallback* aCallback)
    : mGlobal(aGlobalObject)
    , mCallback(aCallback)
  {
    MOZ_ASSERT(aGlobalObject);
    MOZ_ASSERT(aCallback);
  }

  NS_IMETHOD
  Run() override
  {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
    if (NS_WARN_IF(!window)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<DOMError> error =
      new DOMError(window, NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    mCallback->HandleEvent(*error);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ErrorCallback> mCallback;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ErrorCallbackRunnable_h
