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
#include "nsIDOMClassInfo.h"
#include "nsContentUtils.h"
#include "nsIHttpChannel.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsCOMPtr.h"

NS_INTERFACE_MAP_BEGIN(nsSVGDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGDocument)
NS_INTERFACE_MAP_END_INHERITING(nsXMLDocument)

NS_IMPL_ADDREF_INHERITED(nsSVGDocument, nsXMLDocument)
NS_IMPL_RELEASE_INHERITED(nsSVGDocument, nsXMLDocument)

nsSVGDocument::nsSVGDocument() {

}

nsSVGDocument::~nsSVGDocument() {

}

// nsIDOMSVGDocument

NS_IMETHODIMP
nsSVGDocument::GetTitle(nsAString& aTitle) {
  return nsXMLDocument::GetTitle(aTitle);
}

NS_IMETHODIMP
nsSVGDocument::GetReferrer(nsAString& aReferrer) {
  return nsDocument::GetReferrer(aReferrer);
}

NS_IMETHODIMP
nsSVGDocument::GetDomain(nsAString& aDomain) {
  nsCAutoString domain;

  if (mDocumentURI) {
    nsresult rv = mDocumentURI->GetHost(domain);
    if (NS_FAILED(rv)) return rv;
  }

  CopyUTF8toUTF16(domain, aDomain);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGDocument::GetURL(nsAString& aURL) {
  nsCAutoString url;

  if (mDocumentURI) {
    nsresult rv = mDocumentURI->GetSpec(url);
    if (NS_FAILED(rv)) return rv;    
  }

  CopyUTF8toUTF16(url, aURL);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGDocument::GetRootElement(nsIDOMSVGSVGElement** aRootElement) {
  nsCOMPtr<nsIDOMSVGSVGElement> root = do_QueryInterface(mRootContent);
  *aRootElement = root;
  NS_IF_ADDREF(*aRootElement);
  return NS_OK;
}

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult)
{
  nsSVGDocument* doc = new nsSVGDocument();

  NS_ENSURE_TRUE(doc, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
    return rv;
  }

  *aInstancePtrResult = doc;
  return rv;
}
