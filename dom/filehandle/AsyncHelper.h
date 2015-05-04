/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AsyncHelper_h
#define mozilla_dom_AsyncHelper_h

#include "nsCOMPtr.h"
#include "nsIRequest.h"
#include "nsIRunnable.h"

class nsIRequestObserver;

namespace mozilla {
namespace dom {

/**
 * Must be subclassed. The subclass must implement DoStreamWork.
 * Async operations that are not supported in necko (truncate, flush, etc.)
 * should use this helper. Call AsyncWork to invoke the operation.
 */
class AsyncHelper : public nsIRunnable,
                    public nsIRequest
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIREQUEST

  nsresult
  AsyncWork(nsIRequestObserver* aObserver, nsISupports* aCtxt);

protected:
  explicit AsyncHelper(nsISupports* aStream)
  : mStream(aStream),
    mStatus(NS_OK)
  { }

  virtual ~AsyncHelper()
  { }

  virtual nsresult
  DoStreamWork(nsISupports* aStream) = 0;

private:
  nsCOMPtr<nsISupports> mStream;
  nsCOMPtr<nsIRequestObserver> mObserver;

  nsresult mStatus;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AsyncHelper_h
