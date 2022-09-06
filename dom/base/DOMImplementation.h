/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMImplementation_h
#define mozilla_dom_DOMImplementation_h

#include "nsWrapperCache.h"

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURI.h"
#include "nsIWeakReferenceUtils.h"
#include "nsString.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class Document;
class DocumentType;
template <typename T>
class Optional;

class DOMImplementation final : public nsISupports, public nsWrapperCache {
  ~DOMImplementation();

 public:
  DOMImplementation(Document* aOwner, nsIGlobalObject* aScriptObject,
                    nsIURI* aDocumentURI, nsIURI* aBaseURI);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMImplementation)

  Document* GetParentObject() const { return mOwner; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool HasFeature() { return true; }

  already_AddRefed<DocumentType> CreateDocumentType(
      const nsAString& aQualifiedName, const nsAString& aPublicId,
      const nsAString& aSystemId, ErrorResult& aRv);

  already_AddRefed<Document> CreateDocument(const nsAString& aNamespaceURI,
                                            const nsAString& aQualifiedName,
                                            DocumentType* aDoctype,
                                            ErrorResult& aRv);

  already_AddRefed<Document> CreateHTMLDocument(
      const Optional<nsAString>& aTitle, ErrorResult& aRv);

 private:
  nsresult CreateDocument(const nsAString& aNamespaceURI,
                          const nsAString& aQualifiedName,
                          DocumentType* aDoctype, Document** aDocument);
  nsresult CreateHTMLDocument(const nsAString& aTitle, Document** aDocument);

  nsCOMPtr<Document> mOwner;
  nsWeakPtr mScriptObject;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMImplementation_h
