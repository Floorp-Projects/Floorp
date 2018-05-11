/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMImplementation.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/dom/DOMImplementationBinding.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "mozilla/dom/DocumentType.h"
#include "nsTextNode.h"

namespace mozilla {
namespace dom {

// QueryInterface implementation for DOMImplementation
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMImplementation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMImplementation, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMImplementation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMImplementation)

JSObject*
DOMImplementation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMImplementationBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DocumentType>
DOMImplementation::CreateDocumentType(const nsAString& aQualifiedName,
                                      const nsAString& aPublicId,
                                      const nsAString& aSystemId,
                                      ErrorResult& aRv)
{
  if (!mOwner) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  aRv = nsContentUtils::CheckQName(aQualifiedName);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<nsAtom> name = NS_Atomize(aQualifiedName);
  if (!name) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // Indicate that there is no internal subset (not just an empty one)
  RefPtr<DocumentType> docType =
    NS_NewDOMDocumentType(mOwner->NodeInfoManager(), name, aPublicId,
                          aSystemId, VoidString());
  return docType.forget();
}

nsresult
DOMImplementation::CreateDocument(const nsAString& aNamespaceURI,
                                  const nsAString& aQualifiedName,
                                  DocumentType* aDoctype,
                                  nsIDocument** aDocument)
{
  *aDocument = nullptr;

  nsresult rv;
  if (!aQualifiedName.IsEmpty()) {
    const nsString& qName = PromiseFlatString(aQualifiedName);
    const char16_t *colon;
    rv = nsContentUtils::CheckQName(qName, true, &colon);
    NS_ENSURE_SUCCESS(rv, rv);

    if (colon &&
        (DOMStringIsNull(aNamespaceURI) ||
         (Substring(qName.get(), colon).EqualsLiteral("xml") &&
          !aNamespaceURI.EqualsLiteral("http://www.w3.org/XML/1998/namespace")))) {
      return NS_ERROR_DOM_NAMESPACE_ERR;
    }
  }

  nsCOMPtr<nsIGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptObject);

  NS_ENSURE_STATE(!mScriptObject || scriptHandlingObject);

  nsCOMPtr<nsIDocument> doc;

  rv = NS_NewDOMDocument(getter_AddRefs(doc),
                         aNamespaceURI, aQualifiedName, aDoctype,
                         mDocumentURI, mBaseURI,
                         mOwner->NodePrincipal(),
                         true, scriptHandlingObject,
                         DocumentFlavorLegacyGuess);
  NS_ENSURE_SUCCESS(rv, rv);

  // When DOMImplementation's createDocument method is invoked with
  // namespace set to HTML Namespace use the registry of the associated
  // document to the new instance.

  if (aNamespaceURI.EqualsLiteral("http://www.w3.org/1999/xhtml")) {
    doc->SetContentType(NS_LITERAL_STRING("application/xhtml+xml"));
  } else if (aNamespaceURI.EqualsLiteral("http://www.w3.org/2000/svg")) {
    doc->SetContentType(NS_LITERAL_STRING("image/svg+xml"));
  } else {
    doc->SetContentType(NS_LITERAL_STRING("application/xml"));
  }

  doc->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);

  doc.forget(aDocument);
  return NS_OK;
}

already_AddRefed<nsIDocument>
DOMImplementation::CreateDocument(const nsAString& aNamespaceURI,
                                  const nsAString& aQualifiedName,
                                  DocumentType* aDoctype,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsIDocument> document;
  aRv = CreateDocument(aNamespaceURI, aQualifiedName, aDoctype,
                       getter_AddRefs(document));
  return document.forget();
}

nsresult
DOMImplementation::CreateHTMLDocument(const nsAString& aTitle,
                                      nsIDocument** aDocument)
{
  *aDocument = nullptr;

  NS_ENSURE_STATE(mOwner);

  // Indicate that there is no internal subset (not just an empty one)
  RefPtr<DocumentType> doctype =
    NS_NewDOMDocumentType(mOwner->NodeInfoManager(),
                          nsGkAtoms::html, // aName
                          EmptyString(), // aPublicId
                          EmptyString(), // aSystemId
                          VoidString()); // aInternalSubset

  nsCOMPtr<nsIGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptObject);

  NS_ENSURE_STATE(!mScriptObject || scriptHandlingObject);

  nsCOMPtr<nsIDocument> doc;
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(doc),
                                  EmptyString(), EmptyString(),
                                  doctype, mDocumentURI, mBaseURI,
                                  mOwner->NodePrincipal(),
                                  true, scriptHandlingObject,
                                  DocumentFlavorLegacyGuess);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Element> root = doc->CreateElem(NS_LITERAL_STRING("html"), nullptr,
                                           kNameSpaceID_XHTML);
  rv = doc->AppendChildTo(root, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Element> head = doc->CreateElem(NS_LITERAL_STRING("head"), nullptr,
                                           kNameSpaceID_XHTML);
  rv = root->AppendChildTo(head, false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!DOMStringIsNull(aTitle)) {
    nsCOMPtr<Element> title = doc->CreateElem(NS_LITERAL_STRING("title"),
                                              nullptr, kNameSpaceID_XHTML);
    rv = head->AppendChildTo(title, false);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<nsTextNode> titleText = new nsTextNode(doc->NodeInfoManager());
    rv = titleText->SetText(aTitle, false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = title->AppendChildTo(titleText, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<Element> body = doc->CreateElem(NS_LITERAL_STRING("body"), nullptr,
                                           kNameSpaceID_XHTML);
  rv = root->AppendChildTo(body, false);
  NS_ENSURE_SUCCESS(rv, rv);

  doc->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);

  doc.forget(aDocument);
  return NS_OK;
}

already_AddRefed<nsIDocument>
DOMImplementation::CreateHTMLDocument(const Optional<nsAString>& aTitle,
                                      ErrorResult& aRv)
{
  nsCOMPtr<nsIDocument> document;
  aRv = CreateHTMLDocument(aTitle.WasPassed() ? aTitle.Value() : VoidString(),
                           getter_AddRefs(document));
  return document.forget();
}

} // namespace dom
} // namespace mozilla
