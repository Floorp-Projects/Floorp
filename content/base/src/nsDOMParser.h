/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMParser_h_
#define nsDOMParser_h_

#include "nsIDOMParser.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsWeakReference.h"
#include "nsIJSNativeInitializer.h"
#include "nsIDocument.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/DOMParserBinding.h"
#include "mozilla/dom/TypedArray.h"

class nsIInputStream;

class nsDOMParser MOZ_FINAL : public nsIDOMParser,
                              public nsIDOMParserJS,
                              public nsIJSNativeInitializer,
                              public nsSupportsWeakReference,
                              public nsWrapperCache
{
public: 
  nsDOMParser();
  virtual ~nsDOMParser();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMParser,
                                                         nsIDOMParser)

  // nsIDOMParser
  NS_DECL_NSIDOMPARSER

  // nsIDOMParserJS
  NS_DECL_NSIDOMPARSERJS

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                        uint32_t argc, jsval *argv);

  // WebIDL API
  static already_AddRefed<nsDOMParser>
  Constructor(nsISupports* aOwner, mozilla::ErrorResult& rv)
  {
    nsRefPtr<nsDOMParser> domParser = new nsDOMParser(aOwner);
    rv = domParser->Initialize(aOwner, nullptr, nullptr, 0, nullptr);
    if (rv.Failed()) {
      return nullptr;
    }
    return domParser.forget();
  }

  static already_AddRefed<nsDOMParser>
  Constructor(nsISupports* aOwner, nsIPrincipal* aPrincipal,
              nsIURI* aDocumentURI, nsIURI* aBaseURI,
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
            nsIURI* aBaseURI, mozilla::ErrorResult& rv)
  {
    rv = Init(aPrincipal, aDocumentURI, aBaseURI);
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope,
                               bool* aTriedToWrap) MOZ_OVERRIDE
  {
    return mozilla::dom::DOMParserBinding::Wrap(aCx, aScope, this,
                                                aTriedToWrap);
  }

private:
  nsDOMParser(nsISupports* aOwner) : mOwner(aOwner), mAttemptedInit(false)
  {
    MOZ_ASSERT(aOwner);
    SetIsDOMBinding();
  }

  nsresult InitInternal(nsISupports* aOwner, nsIPrincipal* prin,
                        nsIURI* documentURI, nsIURI* baseURI);

  nsresult SetUpDocument(DocumentFlavor aFlavor, nsIDOMDocument** aResult);

  class AttemptedInitMarker {
  public:
    AttemptedInitMarker(bool* aAttemptedInit) :
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

#endif
