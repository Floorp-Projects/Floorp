/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsMarkupDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"

class nsIHTMLStyleSheet;
class nsContentList;
class nsIContentViewerContainer;
class nsIParser;

class nsHTMLDocument : public nsMarkupDocument, public nsIHTMLDocument, public nsIDOMHTMLDocument {
public:
  nsHTMLDocument();
  virtual ~nsHTMLDocument();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD StartDocumentLoad(nsIURL* aUrl, 
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener** aDocListener);

  NS_IMETHOD EndLoad();

  NS_IMETHOD SetTitle(const nsString& aTitle);

  NS_IMETHOD AddImageMap(nsIImageMap* aMap);

  NS_IMETHOD GetImageMap(const nsString& aMapName, nsIImageMap** aResult);

  // nsIDOMDocument interface
  NS_IMETHOD    GetMasterDoc(nsIDOMDocument **aDocument)
  { return nsDocument::GetMasterDoc(aDocument); }
  NS_IMETHOD    GetDocumentType(nsIDOMDocumentType** aDocumentType);
  NS_IMETHOD    GetProlog(nsIDOMNodeList** aProlog)
  { return nsDocument::GetProlog(aProlog); }
  NS_IMETHOD    GetEpilog(nsIDOMNodeList** aEpilog)
  { return nsDocument::GetEpilog(aEpilog); }
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement)
  { return nsDocument::GetDocumentElement(aDocumentElement); }
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
  { return nsDocument::CreateDocumentFragment(aReturn); }
  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn)
  { return nsDocument::CreateComment(aData, aReturn); }
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)
  { return nsDocument::CreateProcessingInstruction(aTarget, aData, aReturn); }
  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMNode* aValue, nsIDOMAttribute** aReturn)
  { return nsDocument::CreateAttribute(aName, aValue, aReturn); }
  NS_IMETHOD    CreateElement(const nsString& aTagName, 
                              nsIDOMNamedNodeMap* aAttributes, 
                              nsIDOMElement** aReturn);
  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn);
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn)
  { return nsDocument::GetElementsByTagName(aTagname, aReturn); }

  // nsIDOMNode interface
  NS_IMETHOD    GetNodeName(nsString& aNodeName)
  { return nsDocument::GetNodeName(aNodeName); }
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue)
  { return nsDocument::GetNodeValue(aNodeValue); }
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue)
  { return nsDocument::SetNodeValue(aNodeValue); }
  NS_IMETHOD    GetNodeType(PRInt32* aNodeType)
  { return nsDocument::GetNodeType(aNodeType); }
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)
  { return nsDocument::GetParentNode(aParentNode); }
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)
  { return nsDocument::GetChildNodes(aChildNodes); }
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes)
  { return nsDocument::GetHasChildNodes(aHasChildNodes); }
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)
  { return nsDocument::GetFirstChild(aFirstChild); }
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)
  { return nsDocument::GetLastChild(aLastChild); }
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)
  { return nsDocument::GetPreviousSibling(aPreviousSibling); }
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)
  { return nsDocument::GetNextSibling(aNextSibling); }
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)
  { return nsDocument::GetAttributes(aAttributes); }
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
  { return nsDocument::InsertBefore(aNewChild, aRefChild, aReturn); }
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  { return nsDocument::ReplaceChild(aNewChild, aOldChild, aReturn); }
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  { return nsDocument::RemoveChild(aOldChild, aReturn); }
  NS_IMETHOD    CloneNode(nsIDOMNode** aReturn)
  { return nsDocument::CloneNode(aReturn); }
  NS_IMETHOD    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)
  { return nsDocument::Equals(aNode, aDeep, aReturn); }

  // nsIDOMHTMLDocument interface
  NS_IMETHOD    GetTitle(nsString& aTitle);
  NS_IMETHOD    GetReferrer(nsString& aReferrer);
  NS_IMETHOD    GetFileSize(nsString& aFileSize);
  NS_IMETHOD    GetFileCreatedDate(nsString& aFileCreatedDate);
  NS_IMETHOD    GetFileModifiedDate(nsString& aFileModifiedDate);
  NS_IMETHOD    GetFileUpdatedDate(nsString& aFileUpdatedDate);
  NS_IMETHOD    GetDomain(nsString& aDomain);
  NS_IMETHOD    GetURL(nsString& aURL);
  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody);
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody);
  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages);
  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets);
  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks);
  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms);
  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors);
  NS_IMETHOD    GetCookie(nsString& aCookie);
  NS_IMETHOD    SetCookie(const nsString& aCookie);
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc);
  NS_IMETHOD    Close();
  NS_IMETHOD    Write(JSContext *cx, jsval *argv, PRUint32 argc);
  NS_IMETHOD    Writeln(JSContext *cx, jsval *argv, PRUint32 argc);
  NS_IMETHOD    GetElementById(const nsString& aElementId, nsIDOMElement** aReturn);
  NS_IMETHOD    GetElementsByName(const nsString& aElementName, nsIDOMNodeList** aReturn);

  // From nsIScriptObjectOwner interface, implemented by nsDocument
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

protected:
  virtual void AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet);
  static PRBool MatchLinks(nsIContent *aContent);
  static PRBool MatchAnchors(nsIContent *aContent);

  nsIHTMLStyleSheet* mAttrStyleSheet;
  nsVoidArray mImageMaps;

  nsContentList *mImages;
  nsContentList *mApplets;
  nsContentList *mEmbeds;
  nsContentList *mLinks;
  nsContentList *mAnchors;

  nsIParser *mParser;
};

#endif /* nsHTMLDocument_h___ */
