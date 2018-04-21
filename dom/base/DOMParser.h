/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMParser_h_
#define mozilla_dom_DOMParser_h_

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Span.h"
#include "mozilla/dom/DOMParserBinding.h"
#include "mozilla/dom/TypedArray.h"

class nsIDocument;
class nsIGlobalObject;

namespace mozilla {
namespace dom {

class DOMParser final : public nsISupports,
                        public nsWrapperCache
{
  typedef mozilla::dom::GlobalObject GlobalObject;

  virtual ~DOMParser();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMParser)

  // WebIDL API
  static already_AddRefed<DOMParser>
  Constructor(const GlobalObject& aOwner,
              mozilla::ErrorResult& rv);

  already_AddRefed<nsIDocument>
  ParseFromString(const nsAString& aStr, SupportedType aType, ErrorResult& aRv);

  // Sequence converts to Span, so we can use this overload for both
  // the Sequence case and our internal uses.
  already_AddRefed<nsIDocument>
  ParseFromBuffer(Span<const uint8_t> aBuf, SupportedType aType,
                  ErrorResult& aRv);

  already_AddRefed<nsIDocument>
  ParseFromBuffer(const Uint8Array& aBuf, SupportedType aType,
                  ErrorResult& aRv);

  already_AddRefed<nsIDocument>
  ParseFromStream(nsIInputStream* aStream, const nsAString& aCharset,
                  int32_t aContentLength, SupportedType aType,
                  ErrorResult& aRv);

  void
  ForceEnableXULXBL()
  {
    mForceEnableXULXBL = true;
  }

  nsIGlobalObject* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::DOMParserBinding::Wrap(aCx, this, aGivenProto);
  }

  // A way to create a non-global-associated DOMParser from C++.
  static already_AddRefed<DOMParser> CreateWithoutGlobal(ErrorResult& aRv);

private:
  DOMParser(nsIGlobalObject* aOwner, nsIPrincipal* aDocPrincipal,
            nsIURI* aDocumentURI, nsIURI* aBaseURI);

  already_AddRefed<nsIDocument> SetUpDocument(DocumentFlavor aFlavor,
                                              ErrorResult& aRv);

  nsCOMPtr<nsIGlobalObject> mOwner;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;

  bool mForceEnableXULXBL;
};

} // namespace dom
} // namespace mozilla

#endif
