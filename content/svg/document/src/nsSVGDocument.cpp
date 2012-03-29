/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Bradley Baetz.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@cs.mcgill.ca> (original author)
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
nsSVGDocument::Clone(nsNodeInfo *aNodeInfo, nsINode **aResult) const
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
