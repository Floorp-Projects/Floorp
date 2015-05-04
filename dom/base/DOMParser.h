/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMParser_h_
#define mozilla_dom_DOMParser_h_

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMParser.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/DOMParserBinding.h"
#include "mozilla/dom/TypedArray.h"

class nsIDocument;

namespace mozilla {
namespace dom {

class DOMParser final : public nsIDOMParser,
                        public nsSupportsWeakReference,
                        public nsWrapperCache
{
  typedef mozilla::dom::GlobalObject GlobalObject;

  virtual ~DOMParser();

public: 
  DOMParser();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(DOMParser,
                                                         nsIDOMParser)

  // nsIDOMParser
  NS_DECL_NSIDOMPARSER

  // WebIDL API
  static already_AddRefed<DOMParser>
  Constructor(const GlobalObject& aOwner,
              mozilla::ErrorResult& rv);

  static already_AddRefed<DOMParser>
  Constructor(const GlobalObject& aOwner,
              nsIPrincipal* aPrincipal, nsIURI* aDocumentURI, nsIURI* aBaseURI,
              mozilla::ErrorResult& rv);

  already_AddRefed<nsIDocument>
  ParseFromString(const nsAString& aStr, mozilla::dom::SupportedType aType,
                  mozilla::ErrorResult& rv);

  already_AddRefed<nsIDocument>
  ParseFromBuffer(const mozilla::dom::Sequence<uint8_t>& aBuf,
                  uint32_t aBufLen, mozilla::dom::SupportedType aType,
                  mozilla::ErrorResult& rv);

  already_AddRefed<nsIDocument>
  ParseFromBuffer(const mozilla::dom::Uint8Array& aBuf, uint32_t aBufLen,
                  mozilla::dom::SupportedType aType,
                  mozilla::ErrorResult& rv);

  already_AddRefed<nsIDocument>
  ParseFromStream(nsIInputStream* aStream, const nsAString& aCharset,
                  int32_t aContentLength, mozilla::dom::SupportedType aType,
                  mozilla::ErrorResult& rv);

  void Init(nsIPrincipal* aPrincipal, nsIURI* aDocumentURI,
            nsIURI* aBaseURI, mozilla::ErrorResult& rv);

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::DOMParserBinding::Wrap(aCx, this, aGivenProto);
  }

private:
  explicit DOMParser(nsISupports* aOwner) : mOwner(aOwner), mAttemptedInit(false)
  {
    MOZ_ASSERT(aOwner);
  }

  nsresult InitInternal(nsISupports* aOwner, nsIPrincipal* prin,
                        nsIURI* documentURI, nsIURI* baseURI);

  nsresult SetUpDocument(DocumentFlavor aFlavor, nsIDOMDocument** aResult);

  // Helper for ParseFromString
  nsresult ParseFromString(const nsAString& str, const char *contentType,
                           nsIDOMDocument **aResult);

  class AttemptedInitMarker {
  public:
    explicit AttemptedInitMarker(bool* aAttemptedInit) :
      mAttemptedInit(aAttemptedInit)
    {}

    ~AttemptedInitMarker() {
      *mAttemptedInit = true;
    }

  private:
    bool* mAttemptedInit;
  };

  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mOriginalPrincipal;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
  nsWeakPtr mScriptHandlingObject;
  
  bool mAttemptedInit;
};

} // namespace dom
} // namespace mozilla

#endif
