/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXMLContentBuilder.h"
#include "nsISupportsArray.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIContent.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAtom.h"
#include "nsContentCID.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);


class nsXMLContentBuilder : public nsIXMLContentBuilder
{
protected:
  friend nsresult NS_NewXMLContentBuilder(nsIXMLContentBuilder** aResult);
  
  nsXMLContentBuilder();
  ~nsXMLContentBuilder();
  
public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIXMLContentBuilder interface
  NS_DECL_NSIXMLCONTENTBUILDER

private:
  void EnsureDoc();
  
  nsCOMPtr<nsIContent> mTop;
  nsCOMPtr<nsIContent> mCurrent;
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsINameSpaceManager> mNamespaceManager;
  PRInt32 mNamespaceId;
};

//----------------------------------------------------------------------
// implementation:

nsXMLContentBuilder::nsXMLContentBuilder()
    : mNamespaceId(kNameSpaceID_None)
{
#ifdef DEBUG
//  printf("nsXMLContentBuilder CTOR\n");
#endif

  mNamespaceManager = do_GetService(NS_NAMESPACEMANAGER_CONTRACTID);
}

nsXMLContentBuilder::~nsXMLContentBuilder()
{
#ifdef DEBUG
//  printf("~nsXMLContentBuilder\n");
#endif
}

nsresult
NS_NewXMLContentBuilder(nsIXMLContentBuilder** aResult)
{
  nsXMLContentBuilder* result = new nsXMLContentBuilder();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS1(nsXMLContentBuilder, nsIXMLContentBuilder);

//----------------------------------------------------------------------
// nsIXMLContentBuilder implementation

/* void clear (in nsIDOMElement root); */
NS_IMETHODIMP nsXMLContentBuilder::Clear(nsIDOMElement *root)
{
  mCurrent = root;
  mTop = root;
  if (mNamespaceId != kNameSpaceID_None) {
    mNamespaceId = kNameSpaceID_None;
  }
  return NS_OK;
}

/* void setDocument (in nsIDOMDocument doc); */
NS_IMETHODIMP nsXMLContentBuilder::SetDocument(nsIDOMDocument *doc)
{
  mDocument = do_QueryInterface(doc);
#ifdef DEBUG
  if (!mDocument)
    NS_WARNING("null document in nsXMLContentBuilder::SetDocument");
#endif
  return NS_OK;
}

/* void setElementNamespace (in AString ns); */
NS_IMETHODIMP nsXMLContentBuilder::SetElementNamespace(const nsAString & ns)
{
  mNamespaceManager->RegisterNameSpace(ns, mNamespaceId);
  return NS_OK;
}

/* void beginElement (in AString tagname); */
NS_IMETHODIMP nsXMLContentBuilder::BeginElement(const nsAString & tagname)
{
  nsCOMPtr<nsIContent> node;
  {
    EnsureDoc();
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(tagname);
    mDocument->CreateElem(nameAtom, nsnull, mNamespaceId, PR_FALSE, getter_AddRefs(node));
  }
  if (!node) {
    NS_ERROR("could not create node");
    return NS_ERROR_FAILURE;
  }

  // ok, we created a content element. now either append it to our
  // top-level array or to the current element.
  if (!mCurrent) {
    if (mTop) {
      NS_ERROR("Building of multi-rooted trees not supported");
      return NS_ERROR_FAILURE;
    }
    mTop = node;
    mCurrent = mTop;
  }
  else {    
    mCurrent->AppendChildTo(node, PR_TRUE, PR_TRUE);
    mCurrent = node;
  }
  
  return NS_OK;
}

/* void endElement (); */
NS_IMETHODIMP nsXMLContentBuilder::EndElement()
{
  NS_ASSERTION(mCurrent, "unbalanced begin/endelement");
  mCurrent = mCurrent->GetParent();
  return NS_OK;
}

/* void attrib (in AString name, in AString value); */
NS_IMETHODIMP nsXMLContentBuilder::Attrib(const nsAString & name, const nsAString & value)
{
  NS_ASSERTION(mCurrent, "can't set attrib w/o open element");
  nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(name);
  mCurrent->SetAttr(0, nameAtom, value, PR_TRUE);
  return NS_OK;
}

/* void textNode (in AString text); */
NS_IMETHODIMP nsXMLContentBuilder::TextNode(const nsAString & text)
{
  EnsureDoc();
  NS_ASSERTION(mCurrent, "can't append textnode w/o open element");
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mDocument);
  NS_ASSERTION(domDoc, "no dom document");

  nsCOMPtr<nsIDOMText> textNode;
  domDoc->CreateTextNode(text, getter_AddRefs(textNode));
  NS_ASSERTION(textNode, "Failed to create text node");
  nsCOMPtr<nsIContent> textContent = do_QueryInterface(textNode);
  mCurrent->AppendChildTo(textContent, PR_TRUE, PR_TRUE);
  return NS_OK;
}

/* readonly attribute nsIDOMElement root; */
NS_IMETHODIMP nsXMLContentBuilder::GetRoot(nsIDOMElement * *aRoot)
{
  if (!mTop) {
    *aRoot = nsnull;
    return NS_OK;
  }
  return mTop->QueryInterface(nsIDOMElement::GetIID(), (void**)aRoot);
}

/* readonly attribute nsIDOMElement current; */
NS_IMETHODIMP nsXMLContentBuilder::GetCurrent(nsIDOMElement * *aCurrent)
{
  if (!mCurrent) {
    *aCurrent = nsnull;
    return NS_OK;
  }  
  return mCurrent->QueryInterface(nsIDOMElement::GetIID(), (void**)aCurrent);
}

//----------------------------------------------------------------------
// implementation helpers

void nsXMLContentBuilder::EnsureDoc()
{
  if (!mDocument) {
    mDocument = do_CreateInstance(kXMLDocumentCID);
    // XXX should probably do some doc initialization here, such as
    // setting the base url
  }
  NS_ASSERTION(mDocument, "null doc");
}
