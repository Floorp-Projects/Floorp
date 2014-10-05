/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Response_h
#define mozilla_dom_Response_h

#include "nsWrapperCache.h"
#include "nsISupportsImpl.h"

#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/UnionTypes.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Headers;
class Promise;

class Response MOZ_FINAL : public nsISupports
                         , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Response)

public:
  Response(nsISupports* aOwner);

  JSObject*
  WrapObject(JSContext* aCx)
  {
    return ResponseBinding::Wrap(aCx, this);
  }

  ResponseType
  Type() const
  {
    return ResponseType::Error;
  }

  void
  GetUrl(DOMString& aUrl) const
  {
    aUrl.AsAString() = EmptyString();
  }

  uint16_t
  Status() const
  {
    return 400;
  }

  void
  GetStatusText(nsCString& aStatusText) const
  {
    aStatusText = EmptyCString();
  }

  Headers*
  Headers_() const { return mHeaders; }

  static already_AddRefed<Response>
  Error(const GlobalObject& aGlobal);

  static already_AddRefed<Response>
  Redirect(const GlobalObject& aGlobal, const nsAString& aUrl, uint16_t aStatus);

  static already_AddRefed<Response>
  Constructor(const GlobalObject& aGlobal,
              const Optional<ArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams>& aBody,
              const ResponseInit& aInit, ErrorResult& rv);

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  already_AddRefed<Response>
  Clone();

  already_AddRefed<Promise>
  ArrayBuffer(ErrorResult& aRv);

  already_AddRefed<Promise>
  Blob(ErrorResult& aRv);

  already_AddRefed<Promise>
  Json(ErrorResult& aRv);

  already_AddRefed<Promise>
  Text(ErrorResult& aRv);

  bool
  BodyUsed();
private:
  ~Response();

  nsCOMPtr<nsISupports> mOwner;
  nsRefPtr<Headers> mHeaders;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Response_h
