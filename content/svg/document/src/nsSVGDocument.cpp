/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGDocument.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "nsIDOMSVGSVGElement.h"
#include "mozilla/dom/Element.h"
#include "nsGenericElement.h"

using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsSVGDocument::nsSVGDocument()
{
}

nsSVGDocument::~nsSVGDocument()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

DOMCI_NODE_DATA(SVGDocument, nsSVGDocument)

NS_INTERFACE_TABLE_HEAD(nsSVGDocument)
  NS_INTERFACE_TABLE_INHERITED1(nsSVGDocument,
                                nsIDOMSVGDocument)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGDocument)
NS_INTERFACE_MAP_END_INHERITING(nsXMLDocument)

NS_IMPL_ADDREF_INHERITED(nsSVGDocument, nsXMLDocument)
NS_IMPL_RELEASE_INHERITED(nsSVGDocument, nsXMLDocument)

//----------------------------------------------------------------------
// nsIDOMSVGDocument methods:

/* readonly attribute DOMString domain; */
NS_IMETHODIMP
nsSVGDocument::GetDomain(nsAString& aDomain)
{
  SetDOMStringToNull(aDomain);

  if (mDocumentURI) {
    nsCAutoString domain;
    nsresult rv = mDocumentURI->GetHost(domain);
    if (domain.IsEmpty() || NS_FAILED(rv))
      return rv;
    CopyUTF8toUTF16(domain, aDomain);
  }

  return NS_OK;
}

/* readonly attribute DOMString URL; */
NS_IMETHODIMP
nsSVGDocument::GetURL(nsAString& aURL)
{
  SetDOMStringToNull(aURL);

  if (mDocumentURI) {
    nsCAutoString url;
    nsresult rv = mDocumentURI->GetSpec(url);
    if (url.IsEmpty() || NS_FAILED(rv))
      return rv;
    CopyUTF8toUTF16(url, aURL);
  }

  return NS_OK;
}

/* readonly attribute SVGSVGElement rootElement; */
NS_IMETHODIMP
nsSVGDocument::GetRootElement(nsIDOMSVGSVGElement** aRootElement)
{
  *aRootElement = nsnull;
  Element* root = nsDocument::GetRootElement();

  return root ? CallQueryInterface(root, aRootElement) : NS_OK;
}

nsresult
nsSVGDocument::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  nsRefPtr<nsSVGDocument> clone = new nsSVGDocument();
  NS_ENSURE_TRUE(clone, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = CloneDocHelper(clone.get());
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(clone.get(), aResult);
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult)
{
  *aInstancePtrResult = nsnull;
  nsSVGDocument* doc = new nsSVGDocument();

  if (!doc)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
    return rv;
  }

  *aInstancePtrResult = doc;
  return rv;
}
