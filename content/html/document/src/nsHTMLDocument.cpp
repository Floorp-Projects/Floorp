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
#include "nsHTMLDocument.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h" 
#include "nsIImageMap.h"
#include "nsIHTMLContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIPostToServer.h"  
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIContentViewerContainer.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "CNavDTD.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentList.h"
#include "nsINetService.h"

//#define rickgdebug 1
#ifdef rickgdebug
#include "nsHTMLContentSinkStream.h"
#endif

static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);

NS_LAYOUT nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult)
{
  nsHTMLDocument* doc = new nsHTMLDocument();
  return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
}

nsHTMLDocument::nsHTMLDocument()
  : nsMarkupDocument(),
    mAttrStyleSheet(nsnull)
{
  mImages = nsnull;
  mApplets = nsnull;
  mEmbeds = nsnull;
  mLinks = nsnull;
  mAnchors = nsnull;
  mParser = nsnull;
  nsHTMLAtoms::AddrefAtoms();
}

nsHTMLDocument::~nsHTMLDocument()
{
  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mAttrStyleSheet);
  NS_IF_RELEASE(mParser);
  nsHTMLAtoms::ReleaseAtoms();
}

NS_IMETHODIMP nsHTMLDocument::QueryInterface(REFNSIID aIID,
                                             void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIHTMLDocumentIID)) {
    AddRef();
    *aInstancePtr = (void**) (nsIHTMLDocument *)this;
    return NS_OK;
  }
  if (aIID.Equals(kIDOMHTMLDocumentIID)) {
    AddRef();
    *aInstancePtr = (void**) (nsIDOMHTMLDocument *)this;
    return NS_OK;
  }
  return nsDocument::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsHTMLDocument::AddRef()
{
  return nsDocument::AddRef();
}

nsrefcnt nsHTMLDocument::Release()
{
  return nsDocument::Release();
}

NS_IMETHODIMP
nsHTMLDocument::StartDocumentLoad(nsIURL *aURL, 
                                  nsIContentViewerContainer* aContainer,
                                  nsIStreamListener **aDocListener)
{
  nsresult rv;
  nsIWebShell* webShell;

  // Delete references to style sheets - this should be done in superclass...
  PRInt32 index = mStyleSheets.Count();
  while (--index >= 0) {
    nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
    NS_RELEASE(sheet);
  }
  mStyleSheets.Clear();

  NS_IF_RELEASE(mAttrStyleSheet);
  NS_IF_RELEASE(mDocumentURL);
  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }

  mDocumentURL = aURL;
  NS_ADDREF(aURL);

  rv = NS_NewParser(&mParser);
  if (NS_OK == rv) {
    nsIHTMLContentSink* sink;

#ifdef rickgdebug
    rv = NS_New_HTML_ContentSinkStream(&sink);
#else
    aContainer->QueryInterface(kIWebShellIID, (void**)&webShell);
    rv = NS_NewHTMLContentSink(&sink, this, aURL, webShell);
    NS_IF_RELEASE(webShell);
#endif

    if (NS_OK == rv) {
      nsIHTMLCSSStyleSheet* styleAttrSheet;
      if (NS_OK == NS_NewHTMLCSSStyleSheet(&styleAttrSheet, aURL)) {
        AddStyleSheet(styleAttrSheet); // tell the world about our new style sheet
        NS_RELEASE(styleAttrSheet);
      }
      if (NS_OK == NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL)) {
        AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
      }

      // Set the parser as the stream listener for the document loader...
      static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
      rv = mParser->QueryInterface(kIStreamListenerIID, (void**)aDocListener);

      if (NS_OK == rv) {

        //The following lines were added by Rick.
        //These perform "dynamic" DTD registration, allowing
        //the caller total control over process, and decoupling
        //parser from any given grammar.

        nsIDTD* theDTD=0;
        NS_NewNavHTMLDTD(&theDTD);
        mParser->RegisterDTD(theDTD);

        mParser->SetContentSink(sink);
        mParser->Parse(aURL);
      }
      NS_RELEASE(sink);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::EndLoad()
{
  NS_IF_RELEASE(mParser);
  return nsDocument::EndLoad();
}

static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

NS_IMETHODIMP nsHTMLDocument::SetTitle(const nsString& aTitle)
{
  if (nsnull == mDocumentTitle) {
    mDocumentTitle = new nsString(aTitle);
  }
  else {
    *mDocumentTitle = aTitle;
  }

  // Pass on to any interested containers
  PRInt32 i, n = mPresShells.Count();
  for (i = 0; i < n; i++) {
    nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(i);
    nsIPresContext* cx = shell->GetPresContext();
    nsISupports* container;
    if (NS_OK == cx->GetContainer(&container)) {
      if (nsnull != container) {
        nsIWebShell* ws = nsnull;
        container->QueryInterface(kIWebShellIID, (void**) &ws);
        if (nsnull != ws) {
          ws->SetTitle(aTitle);
          NS_RELEASE(ws);
        }
        NS_RELEASE(container);
      }
    }
    NS_RELEASE(cx);
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocument::AddImageMap(nsIImageMap* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mImageMaps.AppendElement(aMap)) {
    NS_ADDREF(aMap);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsHTMLDocument::GetImageMap(const nsString& aMapName,
                                          nsIImageMap** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsAutoString name;
  PRInt32 i, n = mImageMaps.Count();
  for (i = 0; i < n; i++) {
    nsIImageMap* map = (nsIImageMap*) mImageMaps.ElementAt(i);
    if (NS_OK == map->GetName(name)) {
      if (name.EqualsIgnoreCase(aMapName)) {
        *aResult = map;
        NS_ADDREF(map);
        return NS_OK;
      }
    }
  }

  return 1;/* XXX NS_NOT_FOUND */
}

NS_IMETHODIMP nsHTMLDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mAttrStyleSheet;
  if (nsnull == mAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  return NS_OK;
}


void nsHTMLDocument::AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet)
{
  if ((nsnull != mAttrStyleSheet) && (aSheet != mAttrStyleSheet)) {
    aSet->InsertDocStyleSheetBefore(aSheet, mAttrStyleSheet);
  }
  else {
    aSet->AppendDocStyleSheet(aSheet);
  }
}

//
// nsIDOMDocument interface implementation
//
NS_IMETHODIMP    
nsHTMLDocument::CreateElement(const nsString& aTagName, 
                              nsIDOMNamedNodeMap* aAttributes, 
                              nsIDOMElement** aReturn)
{
  nsIHTMLContent* content;
  nsresult rv = NS_CreateHTMLElement(&content, aTagName);
  if (NS_OK != rv) {
    return rv;
  }
  rv = content->QueryInterface(kIDOMElementIID, (void**)aReturn);
  return rv;
}

NS_IMETHODIMP
nsHTMLDocument::CreateTextNode(const nsString& aData, nsIDOMText** aTextNode)
{
  nsIHTMLContent* text = nsnull;
  nsresult        rv = NS_NewHTMLText(&text, aData, aData.Length());

  if (NS_OK == rv) {
    rv = text->QueryInterface(kIDOMTextIID, (void**)aTextNode);
  }

  return rv;
}

NS_IMETHODIMP    
nsHTMLDocument::GetDocumentType(nsIDOMDocumentType** aDocumentType)
{
  // There's no document type for a HTML document
  *aDocumentType = nsnull;
  return NS_OK;
}

//
// nsIDOMHTMLDocument interface implementation
//
NS_IMETHODIMP
nsHTMLDocument::GetTitle(nsString& aTitle)
{
  if (nsnull != mDocumentTitle) {
    aTitle.SetString(*mDocumentTitle);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetReferrer(nsString& aReferrer)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFileSize(nsString& aFileSize)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFileCreatedDate(nsString& aFileCreatedDate)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFileModifiedDate(nsString& aFileModifiedDate)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFileUpdatedDate(nsString& aFileUpdatedDate)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetDomain(nsString& aDomain)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetURL(nsString& aURL)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetBody(nsIDOMHTMLElement** aBody)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::SetBody(nsIDOMHTMLElement* aBody)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetImages(nsIDOMHTMLCollection** aImages)
{
  if (nsnull == mImages) {
    mImages = new nsContentList(this, "IMG");
    if (nsnull == mImages) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mImages);
  }

  *aImages = (nsIDOMHTMLCollection *)mImages;
  NS_ADDREF(mImages);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetApplets(nsIDOMHTMLCollection** aApplets)
{
  if (nsnull == mApplets) {
    mApplets = new nsContentList(this, "APPLET");
    if (nsnull == mApplets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mApplets);
  }

  *aApplets = (nsIDOMHTMLCollection *)mImages;
  NS_ADDREF(mImages);

  return NS_OK;
}

PRBool
nsHTMLDocument::MatchLinks(nsIContent *aContent)
{
  nsIAtom *name = aContent->GetTag();
  static nsAutoString area("AREA"), anchor("A");
  nsAutoString attr;
  PRBool result = PR_FALSE;
  
  if ((nsnull != name) && 
      (area.EqualsIgnoreCase(name) || anchor.EqualsIgnoreCase(name)) &&
      (eContentAttr_HasValue == aContent->GetAttribute("HREF", attr))) {
      result = PR_TRUE;
  }

  NS_IF_RELEASE(name);
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLinks(nsIDOMHTMLCollection** aLinks)
{
  if (nsnull == mLinks) {
    mLinks = new nsContentList(this, MatchLinks);
    if (nsnull == mLinks) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mLinks);
  }

  *aLinks = (nsIDOMHTMLCollection *)mLinks;
  NS_ADDREF(mLinks);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetForms(nsIDOMHTMLCollection** aForms)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRBool
nsHTMLDocument::MatchAnchors(nsIContent *aContent)
{
  nsIAtom *name = aContent->GetTag();
  static nsAutoString anchor("A");
  nsAutoString attr;
  PRBool result = PR_FALSE;
  
  if ((nsnull != name) && 
      anchor.EqualsIgnoreCase(name) &&
      (eContentAttr_HasValue == aContent->GetAttribute("NAME", attr))) {
      result = PR_TRUE;
  }

  NS_IF_RELEASE(name);
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::GetAnchors(nsIDOMHTMLCollection** aAnchors)
{
  if (nsnull == mAnchors) {
    mAnchors = new nsContentList(this, MatchAnchors);
    if (nsnull == mAnchors) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mAnchors);
  }

  *aAnchors = (nsIDOMHTMLCollection *)mAnchors;
  NS_ADDREF(mAnchors);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetCookie(nsString& aCookie)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {

    res = service->GetCookieString(mDocumentURL, aCookie);

    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLDocument::SetCookie(const nsString& aCookie)
{
  nsINetService *service;
  nsresult res = NS_OK;
  
  res = NS_NewINetService(&service, nsnull);
  if ((NS_OK == res) && (nsnull != service)) {

    res = service->SetCookieString(mDocumentURL, aCookie);

    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLDocument::Open(JSContext *cx, jsval *argv, PRUint32 argc)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::Close()
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::Write(JSContext *cx, jsval *argv, PRUint32 argc)
{
  nsresult result = NS_OK;

  // XXX Right now, we only deal with inline document.writes
  if (nsnull == mParser) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  
  if (argc > 0) {
    PRUint32 index;
    for (index = 0; index < argc; index++) {
      nsAutoString str;
      JSString *jsstring = JS_ValueToString(cx, argv[index]);
      
      if (nsnull != jsstring) {
        str.SetString(JS_GetStringChars(jsstring));
      }
      else {
        str.SetString("");   // Should this really be null?? 
      }
      
      result = mParser->Parse(str, PR_TRUE);
      if (NS_OK != result) {
        return result;
      }
    }
  }
  
  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::Writeln(JSContext *cx, jsval *argv, PRUint32 argc)
{
  nsAutoString newLine("\n");
  nsresult result;

  // XXX Right now, we only deal with inline document.writes
  if (nsnull == mParser) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  
  result = Write(cx, argv, argc);
  if (NS_OK == result) {
    result = mParser->Parse(newLine, PR_TRUE);
  }
  
  return result;
}


NS_IMETHODIMP    
nsHTMLDocument::GetElementById(const nsString& aElementId, nsIDOMElement** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementsByName(const nsString& aElementName, nsIDOMNodeList** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsHTMLDocument::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLDocument(aContext, this, (nsISupports *)global, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

