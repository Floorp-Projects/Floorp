/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDocument.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "nsIDOMSVGElement.h"
#include "mozilla/dom/Element.h"
#include "nsSVGElement.h"
#include "mozilla/dom/SVGDocumentBinding.h"

using namespace mozilla::dom;

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// Implementation

SVGDocument::SVGDocument()
{
}

SVGDocument::~SVGDocument()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ISUPPORTS_INHERITED1(SVGDocument, XMLDocument, nsIDOMSVGDocument)

//----------------------------------------------------------------------
// nsIDOMSVGDocument methods:

/* readonly attribute DOMString domain; */
NS_IMETHODIMP
SVGDocument::GetDomain(nsAString& aDomain)
{
  ErrorResult rv;
  GetDomain(aDomain, rv);
  return rv.ErrorCode();
}

void
SVGDocument::GetDomain(nsAString& aDomain, ErrorResult& aRv)
{
  SetDOMStringToNull(aDomain);

  if (mDocumentURI) {
    nsAutoCString domain;
    nsresult rv = mDocumentURI->GetHost(domain);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
    if (domain.IsEmpty()) {
      return;
    }
    CopyUTF8toUTF16(domain, aDomain);
  }
}

/* readonly attribute SVGSVGElement rootElement; */
NS_IMETHODIMP
SVGDocument::GetRootElement(nsIDOMSVGElement** aRootElement)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMSVGElement> retval = GetRootElement(rv);
  retval.forget(aRootElement);
  return rv.ErrorCode();
}

nsSVGElement*
SVGDocument::GetRootElement(ErrorResult& aRv)
{
  Element* root = nsDocument::GetRootElement();
  if (!root) {
    return nullptr;
  }
  if (!root->IsSVG()) {
    aRv.Throw(NS_NOINTERFACE);
    return nullptr;
  }
  return static_cast<nsSVGElement*>(root);
}

nsresult
SVGDocument::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  nsRefPtr<SVGDocument> clone = new SVGDocument();
  NS_ENSURE_TRUE(clone, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = CloneDocHelper(clone.get());
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(clone.get(), aResult);
}

JSObject*
SVGDocument::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  JSObject* obj = SVGDocumentBinding::Wrap(aCx, aScope, this);
  if (obj && !PostCreateWrapper(aCx, obj)) {
    return nullptr;
  }
  return obj;
}

} // namespace dom
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult)
{
  nsRefPtr<SVGDocument> doc = new SVGDocument();

  nsresult rv = doc->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  doc.forget(aInstancePtrResult);
  return rv;
}
