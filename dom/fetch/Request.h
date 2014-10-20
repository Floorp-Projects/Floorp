/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Request_h
#define mozilla_dom_Request_h

#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/InternalRequest.h"
// Required here due to certain WebIDL enums/classes being declared in both
// files.
#include "mozilla/dom/RequestBinding.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Headers;
class InternalHeaders;
class Promise;
class RequestOrScalarValueString;

class Request MOZ_FINAL : public nsISupports
                        , public nsWrapperCache
                        , public FetchBody<Request>
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Request)

public:
  Request(nsIGlobalObject* aOwner, InternalRequest* aRequest);

  JSObject*
  WrapObject(JSContext* aCx)
  {
    return RequestBinding::Wrap(aCx, this);
  }

  void
  GetUrl(DOMString& aUrl) const
  {
    aUrl.AsAString() = NS_ConvertUTF8toUTF16(mRequest->mURL);
  }

  void
  GetMethod(nsCString& aMethod) const
  {
    aMethod = mRequest->mMethod;
  }

  RequestMode
  Mode() const
  {
    return mRequest->mMode;
  }

  RequestCredentials
  Credentials() const
  {
    return mRequest->mCredentialsMode;
  }

  void
  GetReferrer(DOMString& aReferrer) const
  {
    if (mRequest->ReferrerIsNone()) {
      aReferrer.AsAString() = EmptyString();
      return;
    }

    // FIXME(nsm): Spec doesn't say what to do if referrer is client.
    aReferrer.AsAString() = NS_ConvertUTF8toUTF16(mRequest->mReferrerURL);
  }

  InternalHeaders*
  GetInternalHeaders() const
  {
    return mRequest->Headers();
  }

  Headers* Headers_();

  void
  GetBody(nsIInputStream** aStream) { return mRequest->GetBody(aStream); }

  static already_AddRefed<Request>
  Constructor(const GlobalObject& aGlobal, const RequestOrScalarValueString& aInput,
              const RequestInit& aInit, ErrorResult& rv);

  nsIGlobalObject* GetParentObject() const
  {
    return mOwner;
  }

  already_AddRefed<Request>
  Clone() const;

  already_AddRefed<InternalRequest>
  GetInternalRequest();
private:
  ~Request();

  nsCOMPtr<nsIGlobalObject> mOwner;
  nsRefPtr<InternalRequest> mRequest;
  // Lazily created.
  nsRefPtr<Headers> mHeaders;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Request_h
