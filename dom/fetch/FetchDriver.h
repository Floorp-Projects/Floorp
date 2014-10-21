/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchDriver_h
#define mozilla_dom_FetchDriver_h

#include "nsIStreamListener.h"
#include "nsRefPtr.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class InternalRequest;
class InternalResponse;

class FetchDriverObserver
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchDriverObserver);
  virtual void OnResponseAvailable(InternalResponse* aResponse) = 0;

protected:
  virtual ~FetchDriverObserver()
  { };
};

class FetchDriver MOZ_FINAL
{
  NS_INLINE_DECL_REFCOUNTING(FetchDriver)
public:
  explicit FetchDriver(InternalRequest* aRequest);
  NS_IMETHOD Fetch(FetchDriverObserver* aObserver);

private:
  nsRefPtr<InternalRequest> mRequest;
  nsRefPtr<FetchDriverObserver> mObserver;
  uint32_t mFetchRecursionCount;

  FetchDriver() MOZ_DELETE;
  FetchDriver(const FetchDriver&) MOZ_DELETE;
  FetchDriver& operator=(const FetchDriver&) MOZ_DELETE;
  ~FetchDriver();

  nsresult Fetch(bool aCORSFlag);
  nsresult ContinueFetch(bool aCORSFlag);
  nsresult BasicFetch();
  nsresult FailWithNetworkError();
  nsresult BeginResponse(InternalResponse* aResponse);
  nsresult SucceedWithResponse();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FetchDriver_h
