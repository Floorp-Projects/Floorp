/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDocument.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "nsIDOMSVGElement.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;

DOMCI_NODE_DATA(SVGDocument, SVGDocument)

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

NS_INTERFACE_TABLE_HEAD(SVGDocument)
  NS_INTERFACE_TABLE_INHERITED1(SVGDocument,
                                nsIDOMSVGDocument)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGDocument)
NS_INTERFACE_MAP_END_INHERITING(XMLDocument)

NS_IMPL_ADDREF_INHERITED(SVGDocument, XMLDocument)
NS_IMPL_RELEASE_INHERITED(SVGDocument, XMLDocument)

//----------------------------------------------------------------------
// nsIDOMSVGDocument methods:

/* readonly attribute DOMString domain; */
NS_IMETHODIMP
SVGDocument::GetDomain(nsAString& aDomain)
{
  SetDOMStringToNull(aDomain);

  if (mDocumentURI) {
    nsAutoCString domain;
    nsresult rv = mDocumentURI->GetHost(domain);
    if (domain.IsEmpty() || NS_FAILED(rv))
      return rv;
    CopyUTF8toUTF16(domain, aDomain);
  }

  return NS_OK;
}

/* readonly attribute SVGSVGElement rootElement; */
NS_IMETHODIMP
SVGDocument::GetRootElement(nsIDOMSVGElement** aRootElement)
{
  *aRootElement = nullptr;
  Element* root = nsDocument::GetRootElement();

  return root ? CallQueryInterface(root, aRootElement) : NS_OK;
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

  *aInstancePtrResult = doc.forget().get();
  return rv;
}
