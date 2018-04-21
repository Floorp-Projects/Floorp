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

  already_AddRefed<nsIDocument>
  ParseFromString(const nsAString& aStr, SupportedType aType, ErrorResult& aRv);

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

  void
  ForceEnableXULXBL()
  {
    mForceEnableXULXBL = true;
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::DOMParserBinding::Wrap(aCx, this, aGivenProto);
  }

private:
  explicit DOMParser(nsISupports* aOwner)
    : mOwner(aOwner)
    , mAttemptedInit(false)
    , mForceEnableXULXBL(false)
  {
    MOZ_ASSERT(aOwner);
  }

  /**
   * Initialize the principal and document and base URIs that the parser should
   * use for documents it creates.  If this is not called, then a null
   * principal and its URI will be used.  When creating a DOMParser via the JS
   * constructor, this will be called automatically.  This method may only be
   * called once.  If this method fails, all following parse attempts will
   * fail.
   *
   * @param principal The principal to use for documents we create.
   *                  If this is null, a codebase principal will be created
   *                  based on documentURI; in that case the documentURI must
   *                  be non-null.
   * @param documentURI The documentURI to use for the documents we create.
   *                    If null, the principal's URI will be used;
   *                    in that case, the principal must be non-null and its
   *                    URI must be non-null.
   * @param baseURI The baseURI to use for the documents we create.
   *                If null, the documentURI will be used.
   * @param scriptObject The object from which the context for event handling
   *                     can be got.
   */
  nsresult Init(nsIPrincipal* aPrincipal, nsIURI* aDocumentURI,
                nsIURI* aBaseURI, nsIGlobalObject* aSriptObjet);


  nsresult InitInternal(nsISupports* aOwner, nsIPrincipal* prin,
                        nsIURI* documentURI, nsIURI* baseURI);

  nsresult SetUpDocument(DocumentFlavor aFlavor, nsIDocument** aResult);

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
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
  nsWeakPtr mScriptHandlingObject;

  bool mAttemptedInit;
  bool mForceEnableXULXBL;
};

} // namespace dom
} // namespace mozilla

#endif
