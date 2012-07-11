/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXMLContentBuilder.h"
#include "nsINameSpaceManager.h"
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
#include "nsContentUtils.h"
#include "mozilla/Attributes.h"

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

class nsXMLContentBuilder MOZ_FINAL : public nsIXMLContentBuilder
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

NS_IMPL_ISUPPORTS1(nsXMLContentBuilder, nsIXMLContentBuilder)

//----------------------------------------------------------------------
// nsIXMLContentBuilder implementation

/* void clear (in nsIDOMElement root); */
NS_IMETHODIMP nsXMLContentBuilder::Clear(nsIDOMElement *root)
{
  mCurrent = do_QueryInterface(root);
  mTop = do_QueryInterface(root);
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
  nsContentUtils::NameSpaceManager()->RegisterNameSpace(ns, mNamespaceId);
  return NS_OK;
}

/* void beginElement (in AString tagname); */
NS_IMETHODIMP nsXMLContentBuilder::BeginElement(const nsAString & tagname)
{
  nsCOMPtr<nsIContent> node;
  {
    EnsureDoc();
    mDocument->CreateElem(tagname, nsnull, mNamespaceId, getter_AddRefs(node));
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
    mCurrent->AppendChildTo(node, true);
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
  mCurrent->SetAttr(0, nameAtom, value, true);
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
  mCurrent->AppendChildTo(textContent, true);
  return NS_OK;
}

/* readonly attribute nsIDOMElement root; */
NS_IMETHODIMP nsXMLContentBuilder::GetRoot(nsIDOMElement * *aRoot)
{
  if (!mTop) {
    *aRoot = nsnull;
    return NS_OK;
  }
  return CallQueryInterface(mTop, aRoot);
}

/* readonly attribute nsIDOMElement current; */
NS_IMETHODIMP nsXMLContentBuilder::GetCurrent(nsIDOMElement * *aCurrent)
{
  if (!mCurrent) {
    *aCurrent = nsnull;
    return NS_OK;
  }  
  return CallQueryInterface(mCurrent, aCurrent);
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
