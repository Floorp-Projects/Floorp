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
#include "nsIFormManager.h"
#include "nsRepository.h"
#include "nsParserCIID.h"

// Find/Serach Includes
#include "nsISelection.h"
#include "nsSelectionRange.h"
#include "nsSelectionPoint.h"
const PRInt32 kForward  = 0;
const PRInt32 kBackward = 1;



//#define rickgdebug 1
#ifdef rickgdebug
#include "nsHTMLContentSinkStream.h"
#endif

static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNSHTMLDocumentIID, NS_IDOMNSHTMLDOCUMENT_IID);

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
  mForms = nsnull;
  mNamedItems = nsnull;
  mParser = nsnull;
  nsHTMLAtoms::AddrefAtoms();

  // Find/Search Init
  mSearchStr             = nsnull;
  mLastBlockSearchOffset = 0;
  mAdjustToEnd           = PR_FALSE;
  mShouldMatchCase       = PR_FALSE;
  mIsPreTag              = PR_FALSE;

  mHoldBlockContent      = nsnull;
  mCurrentBlockContent   = nsnull;

  // These will be converted to a nsDeque
  mStackInx    = 0;
  mParentStack = (nsIContent**) new PRUint32[32];
  mChildStack  = (nsIContent**) new PRUint32[32];


}

nsHTMLDocument::~nsHTMLDocument()
{
  // XXX Temporary code till forms become real content
  PRInt32 i;
  for (i = 0; i < mTempForms.Count(); i++) {
    nsIFormManager *form = (nsIFormManager *)mTempForms.ElementAt(i);
    if (nsnull != form) {
      NS_RELEASE(form);
    }
  }
  DeleteNamedItems();
  NS_IF_RELEASE(mImages);
  NS_IF_RELEASE(mApplets);
  NS_IF_RELEASE(mEmbeds);
  NS_IF_RELEASE(mLinks);
  NS_IF_RELEASE(mAnchors);
  NS_IF_RELEASE(mForms);
  NS_IF_RELEASE(mAttrStyleSheet);
  NS_IF_RELEASE(mParser);
  for (i = 0; i < mImageMaps.Count(); i++) {
    nsIImageMap*  map = (nsIImageMap*)mImageMaps.ElementAt(i);

    NS_RELEASE(map);
  }
// XXX don't bother doing this until the dll is unloaded???
//  nsHTMLAtoms::ReleaseAtoms();

  // These will be converted to a nsDeque
  delete[] mParentStack;
  delete[] mChildStack;

  delete mSearchStr;
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
  if (aIID.Equals(kIDOMNSHTMLDocumentIID)) {
    AddRef();
    *aInstancePtr = (void**) (nsIDOMNSHTMLDocument *)this;
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

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  rv = NSRepository::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&mParser);

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

// XXX Temporary form methods. Forms will soon become actual content
// elements. For now, the document keeps a list of them.
NS_IMETHODIMP 
nsHTMLDocument::AddForm(nsIFormManager *aForm)
{
  NS_PRECONDITION(nsnull != aForm, "null ptr");
  if (nsnull == aForm) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mTempForms.AppendElement(aForm)) {
    NS_ADDREF(aForm);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP_(PRInt32) 
nsHTMLDocument::GetFormCount() const
{
  return mTempForms.Count();
}
  
NS_IMETHODIMP 
nsHTMLDocument::GetFormAt(PRInt32 aIndex, nsIFormManager **aForm) const
{
  *aForm = (nsIFormManager *)mTempForms.ElementAt(aIndex);

  if (nsnull != *aForm) {
    NS_ADDREF(*aForm);
    return NS_OK;
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
  nsIAtom *name;
  aContent->GetTag(name);
  static nsAutoString area("AREA"), anchor("A");
  nsAutoString attr;
  PRBool result = PR_FALSE;
  
  if ((nsnull != name) && 
      (area.EqualsIgnoreCase(name) || anchor.EqualsIgnoreCase(name)) &&
      (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute("HREF", attr))) {
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

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

// XXX Temporary till form becomes real content

class nsTempFormContentList : public nsContentList {
public:
  nsTempFormContentList(nsHTMLDocument *aDocument);
  ~nsTempFormContentList();
};

nsTempFormContentList::nsTempFormContentList(nsHTMLDocument *aDocument) : nsContentList(aDocument)
{
  PRInt32 i, count = aDocument->GetFormCount();
  for (i=0; i < count; i++) {
    nsIFormManager *form;
    
    if (NS_OK == aDocument->GetFormAt(i, &form)) {
      nsIContent *content;
      if (NS_OK == form->QueryInterface(kIContentIID, (void **)&content)) {
        Add(content);
        NS_RELEASE(content);
      }
      NS_RELEASE(form);
    }
  }
}

nsTempFormContentList::~nsTempFormContentList()
{
  mDocument = nsnull;
}

NS_IMETHODIMP    
nsHTMLDocument::GetForms(nsIDOMHTMLCollection** aForms)
{
  if (nsnull == mForms) {
    mForms = new nsTempFormContentList(this);
    if (nsnull == mForms) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mForms);
  }

  *aForms = (nsIDOMHTMLCollection *)mForms;
  NS_ADDREF(mForms);

  return NS_OK;
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

PRBool
nsHTMLDocument::MatchAnchors(nsIContent *aContent)
{
  nsIAtom *name;
  aContent->GetTag(name);
  static nsAutoString anchor("A");
  nsAutoString attr;
  PRBool result = PR_FALSE;
  
  if ((nsnull != name) && 
      anchor.EqualsIgnoreCase(name) &&
      (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute("NAME", attr))) {
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

nsIContent *
nsHTMLDocument::MatchName(nsIContent *aContent, const nsString& aName)
{
  static nsAutoString name("NAME"), id("ID");
  nsAutoString value;
  nsIContent *result = nsnull;

  if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(id, value)) &&
      aName.Equals(value)) {
    return aContent;
  }
  else if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(name, value)) &&
           aName.Equals(value)) {
    return aContent;
  }
  
  PRInt32 i, count;
  aContent->ChildCount(count);
  for (i = 0; i < count && result == nsnull; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchName(child, aName);
    NS_RELEASE(child);
  }  

  return result;
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementById(const nsString& aElementId, nsIDOMElement** aReturn)
{
  nsIContent *content;

  // XXX For now, we do a brute force search of the content tree.
  // We should come up with a more efficient solution.
  content = MatchName(mRootContent, aElementId);

  if (nsnull != content) {
    return content->QueryInterface(kIDOMElementIID, (void **)aReturn);
  }
  else {
    *aReturn = nsnull;
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLDocument::GetElementsByName(const nsString& aElementName, nsIDOMNodeList** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetAlinkColor(nsString& aAlinkColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::SetAlinkColor(const nsString& aAlinkColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLinkColor(nsString& aLinkColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::SetLinkColor(const nsString& aLinkColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetVlinkColor(nsString& aVlinkColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::SetVlinkColor(const nsString& aVlinkColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetBgColor(nsString& aBgColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::SetBgColor(const nsString& aBgColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetFgColor(nsString& aFgColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::SetFgColor(const nsString& aFgColor)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLastModified(nsString& aLastModified)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetEmbeds(nsIDOMHTMLCollection** aEmbeds)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetLayers(nsIDOMHTMLCollection** aLayers)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetPlugins(nsIDOMHTMLCollection** aPlugins)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLDocument::GetSelection(nsString& aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRIntn 
nsHTMLDocument::RemoveStrings(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;

  delete [] str;
  return HT_ENUMERATE_REMOVE;
}

void
nsHTMLDocument::DeleteNamedItems()
{
  if (nsnull != mNamedItems) {
    PL_HashTableEnumerateEntries(mNamedItems, RemoveStrings, nsnull);
    PL_HashTableDestroy(mNamedItems);
    mNamedItems = nsnull;
  }
}

void
nsHTMLDocument::RegisterNamedItems(nsIContent *aContent, PRBool aInForm)
{
  static nsAutoString name("NAME");
  nsAutoString value;
  nsIAtom *tag;
  aContent->GetTag(tag);
  PRBool inForm;

  // Only the content types reflected in Level 0 with a NAME
  // attribute are registered. Images and forms always get 
  // reflected up to the document. Applets and embeds only go
  // to the closest container (which could be a form).
  if ((tag == nsHTMLAtoms::img) || (tag == nsHTMLAtoms::form) ||
      (!aInForm && ((tag == nsHTMLAtoms::applet) || 
                    (tag == nsHTMLAtoms::embed)))) {
    if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(name, value)) {
      char *nameStr = value.ToNewCString();
      PL_HashTableAdd(mNamedItems, nameStr, aContent);
    }
  }

  inForm = aInForm || (tag == nsHTMLAtoms::form);
  NS_IF_RELEASE(tag);
  
  PRInt32 i, count;
  aContent->ChildCount(count);
  for (i = 0; i < count; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    RegisterNamedItems(child, inForm);
    NS_RELEASE(child);
  }  
}

NS_IMETHODIMP    
nsHTMLDocument::NamedItem(const nsString& aName, nsIDOMElement** aReturn)
{
  static nsAutoString name("NAME");
  nsresult result = NS_OK;
  nsIContent *content;

  if (nsnull == mNamedItems) {
    mNamedItems = PL_NewHashTable(10, PL_HashString, PL_CompareStrings, 
                                  PL_CompareValues, nsnull, nsnull);
    RegisterNamedItems(mRootContent, PR_FALSE);
    // XXX Need to do this until forms become real content
    PRInt32 i, count = mTempForms.Count();
    for (i = 0; i < count; i++) {
      nsIFormManager *form = (nsIFormManager *)mTempForms.ElementAt(i);
      if (NS_OK == form->QueryInterface(kIContentIID, (void **)&content)) {
        nsAutoString value;
        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(name, value)) {
          char *nameStr = value.ToNewCString();
          PL_HashTableAdd(mNamedItems, nameStr, content);
        }
        NS_RELEASE(content);
      }
    }
  }

  char *str = aName.ToNewCString();

  content = (nsIContent *)PL_HashTableLookup(mNamedItems, str);
  if (nsnull != content) {
    result = content->QueryInterface(kIDOMElementIID, (void **)aReturn);
  }
  else {
    *aReturn = nsnull;
  }

  delete [] str;
  return result;
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


//----------------------------
PRBool IsInline(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case  eHTMLTag_a:
    case  eHTMLTag_address:
    case  eHTMLTag_big:
    case  eHTMLTag_blink:
    case  eHTMLTag_b:
    case  eHTMLTag_br:
    case  eHTMLTag_cite:
    case  eHTMLTag_code:
    case  eHTMLTag_dfn:
    case  eHTMLTag_em:
    case  eHTMLTag_font:
    case  eHTMLTag_img:
    case  eHTMLTag_i:
    case  eHTMLTag_kbd:
    case  eHTMLTag_keygen:
    case  eHTMLTag_nobr:
    case  eHTMLTag_samp:
    case  eHTMLTag_small:
    case  eHTMLTag_spacer:
    case  eHTMLTag_span:      
    case  eHTMLTag_strike:
    case  eHTMLTag_strong:
    case  eHTMLTag_sub:
    case  eHTMLTag_sup:
    case  eHTMLTag_td:
    case  eHTMLTag_textarea:
    case  eHTMLTag_tt:
    case  eHTMLTag_var:
    case  eHTMLTag_wbr:
           
      result = PR_TRUE;
      break;

    default:
      break;

  }
  return result;
}

//----------------------------
PRBool IsBlockLevel(eHTMLTags aTag, PRBool &isPreTag) 
{
  isPreTag = (aTag == eHTMLTag_pre);

  return !IsInline(aTag);
}

//----------------------------
class SubText {
public:
  nsIContent * mContent;
  PRInt32      mOffset;
  PRInt32      mLength;
};

//----------------------------
class BlockText {
  nsString   mText;
  SubText  * mSubTexts[512];
  PRInt32    mNumSubTexts;
public:
  BlockText() : mNumSubTexts(0) {}
  virtual ~BlockText();

  char * GetText()             { return mText.ToNewCString(); }
  const nsString & GetNSString() { return mText; }

  void AddSubText(nsIContent * aContent, nsString & aStr);
  PRInt32 GetStartEnd(PRInt32 anIndex, PRInt32 aLength,
                       nsIContent ** aStartContent, PRInt32 & aStartOffset,
                       nsIContent ** aEndContent,   PRInt32 & aEndOffset,
                       PRInt32       aDirection);
};

//----------------------------
BlockText::~BlockText() 
{
  PRInt32 i;
  for (i=0;i<mNumSubTexts;i++) {
    delete mSubTexts[i];
  }
}

//----------------------------
void BlockText::AddSubText(nsIContent * aContent, nsString & aStr) 
{
  SubText * subTxt = new SubText();
  subTxt->mContent = aContent;
  subTxt->mOffset  = mText.Length();
  subTxt->mLength  = aStr.Length();
  mText.Append(aStr);
  //DEBUG printf("Adding Str to Block [%s]\n", aStr.ToNewCString());
  //DEBUG printf("Block [%s]\n", mText.ToNewCString());
  mSubTexts[mNumSubTexts++] = subTxt;
}

//-----------------------------------------------
PRInt32 BlockText::GetStartEnd(PRInt32 anIndex, PRInt32 aLength,
                               nsIContent ** aStartContent, PRInt32 & aStartOffset,
                               nsIContent ** aEndContent,   PRInt32 & aEndOffset,
                               PRInt32       aDirection)
{
  // Debug Start
  char * cStr = mText.ToNewCString();
  if (strstr(cStr, "blue text")) {
    int x = 0;
  }
  // Debug End

  PRInt32 i      = 0;
  PRInt32 offset = 0;
  PRInt32 endPos = anIndex + aLength;
  while (anIndex > mSubTexts[i]->mOffset+mSubTexts[i]->mLength) {
    i++;
  }
  aStartOffset  = anIndex - mSubTexts[i]->mOffset;
  *aStartContent = mSubTexts[i]->mContent;
  if (endPos <= mSubTexts[i]->mOffset+mSubTexts[i]->mLength) {
    aEndOffset = aStartOffset + aLength;
    *aEndContent = *aStartContent;
  } else {

    while (endPos > mSubTexts[i]->mOffset+mSubTexts[i]->mLength) {
      i++;
    }
    aEndOffset   = endPos - mSubTexts[i]->mOffset;
    *aEndContent = mSubTexts[i]->mContent;
  }

  if (aDirection == kBackward) {
    endPos -= aLength;
  }
  return endPos;

}

//-----------------------------------------------
PRBool nsHTMLDocument::SearchBlock(BlockText & aBlockText, 
                                   nsString  & aStr) 
{
  PRBool found = PR_FALSE;

  mLastBlockSearchOffset = (mCurrentBlockContent == mHoldBlockContent? mLastBlockSearchOffset : 0);

  char * searchStr;
  char * contentStr;

  if (!mShouldMatchCase) {
    nsString searchTxt(aStr);
    nsString blockTxt(aBlockText.GetNSString());

    searchTxt.ToLowerCase();
    blockTxt.ToLowerCase();
    searchStr  = searchTxt.ToNewCString();
    contentStr = blockTxt.ToNewCString();
  } else {
    searchStr  = aStr.ToNewCString();
    contentStr = aBlockText.GetText();
  }

  char * adjustedContent;
  char * str = nsnull;

  if (mSearchDirection == kForward) {
    adjustedContent = contentStr + mLastBlockSearchOffset;
    str = strstr(adjustedContent, searchStr);
  } else {
    adjustedContent = contentStr;
    size_t adjLen;
    if (mAdjustToEnd) {
      adjLen = strlen(adjustedContent);
    } else {
      adjLen = mLastBlockSearchOffset;
    }
    size_t srchLen = strlen(searchStr);
    if (srchLen > adjLen) {
      str = nsnull;
    } else if (adjLen == srchLen && !strncmp(adjustedContent, searchStr, srchLen)) {
      str = adjustedContent;
    } else {
      str = adjustedContent + adjLen - srchLen;
      while (strncmp(str, searchStr, srchLen)) {
        str--;
        if (str < adjustedContent) {
          str = nsnull;
          break;
        }
      }
    }
  }

  if (str) {
    PRInt32 inx = str - contentStr;

    nsIContent * startContent;
    PRInt32      startOffset;
    nsIContent * endContent;
    PRInt32      endOffset;

    mLastBlockSearchOffset      = aBlockText.GetStartEnd(inx, aStr.Length(), &startContent, startOffset, &endContent, endOffset, mSearchDirection);
    mHoldBlockContent           = mCurrentBlockContent;
    nsSelectionRange * range    = mSelection->GetRange();
    nsSelectionPoint * startPnt = range->GetStartPoint();
    nsSelectionPoint * endPnt   = range->GetEndPoint();
    startPnt->SetPoint(startContent, startOffset, PR_TRUE);
    endPnt->SetPoint(endContent, endOffset, PR_FALSE);

    found = PR_TRUE;
  }

  delete[] searchStr;
  delete[] contentStr;

  return found;
}

////////////////////////////////////////////////////
// Check to see if a Content node is a block tag
////////////////////////////////////////////////////
PRBool nsHTMLDocument::ContentIsBlock(nsIContent * aContent) 
{
  PRBool    isBlock = PR_FALSE;
  nsIAtom * atom;
  aContent->GetTag(atom);
  if (atom != nsnull) {
    nsString str;
    atom->ToString(str);
    char * cStr = str.ToNewCString();
    isBlock = IsBlockLevel(NS_TagToEnum(cStr), mIsPreTag);
    delete[] cStr;
    NS_RELEASE(atom);
  }
  return isBlock;
}


/////////////////////////////////////////////
// This function moves up the parent hierarchy 
// looking for a parent that is a "block"
/////////////////////////////////////////////
nsIContent * nsHTMLDocument::FindBlockParent(nsIContent * aContent, 
                                             PRBool       aSkipThisContent)
{
  // Clear up stack and release content nodes on it
  PRInt32 i;
  for (i=0;i<mStackInx;i++) {
    NS_RELEASE(mParentStack[i]);
    NS_RELEASE(mChildStack[i]);
  }
  mStackInx = 0;
  nsIContent * parent;
  aContent->GetParent(parent);
  nsIContent * child;

  if (parent == nsnull) {
    return nsnull;
  }
  NS_ADDREF(aContent);

  // This method enables the param "aContent" be part of the "path"
  // on the stack as the it llok for the block parent.
  //
  // There are times whne we don't want to include the aContent
  // as part of the path so we skip to its next sibling. If it is
  // the last sibling then we jump up to it's parent.
  if (aSkipThisContent) {
    child = aContent;
    PRBool done = PR_FALSE;
    while (!done) {
      PRInt32 inx, numKids;
      parent->IndexOf(child, inx);
      inx += (mSearchDirection == kForward?1:-1);
      parent->ChildCount(numKids);
      if (inx < 0 || inx >= numKids) {
        NS_RELEASE(child);
        child  = parent;
        child->GetParent(parent);
        if (parent == nsnull) {
          NS_RELEASE(child);
          return nsnull;
        }
      } else {
        NS_RELEASE(child);
        parent->ChildAt(inx, child);
        done = PR_TRUE;
      }
    }
  } else {
    child = aContent;
  }

  // This travels up through the parents lloking for the parent who
  // is a block tag. We place the child/parent pairs onto a stack 
  // so we know what nodes to skip as we work our way back down into
  // the block
  do {

    NS_ADDREF(parent);
    NS_ADDREF(child);
    mParentStack[mStackInx]  = parent;
    mChildStack[mStackInx++] = child;

    if (ContentIsBlock(parent)) {
      break;
    }

    nsIContent * oldChild = child;
    child  = parent;
    child->GetParent(parent);
    NS_RELEASE(oldChild);
  } while (parent != nsnull);

  NS_RELEASE(child);
  return parent;
}

//-----------------------------------------------
PRBool nsHTMLDocument::BuildBlockFromContent(nsIContent   * aContent,
                                             BlockText    & aBlockText)
{
  // First check to see if it is a new Block node
  // If it is then we need to check the current block text (aBlockText)
  // to see if it holds the search string
  if (ContentIsBlock(aContent)) {

    // Search current block of text
    if (SearchBlock(aBlockText, *mSearchStr)) {
      return PR_TRUE;
    }

    // Start new search here on down with a new block
    BlockText blockText;
    if (!BuildBlockTraversing(aContent, blockText)) {
      // down inside the search string wasn't found check the full block text 
      // for the search text
      if (SearchBlock(blockText, *mSearchStr)) {
        return PR_TRUE;
      }
    } else {
      // search text was found down inside, so leave
      return PR_TRUE;
    }

  } else {
    return BuildBlockTraversing(aContent, aBlockText);
  }
  return PR_FALSE;
}

//----------------------------------
// This function moves down through
// the hiearchy and build the block of text
//-----------------------------------------
PRBool nsHTMLDocument::BuildBlockTraversing(nsIContent   * aParent,
                                            BlockText    & aBlockText) 
{
  nsIAtom * atom;
  aParent->GetTag(atom);
  if (atom != nsnull) {
    PRInt32 numKids;
    aParent->ChildCount(numKids);
    if (numKids > 0) {
      PRInt32 i;
      if (mSearchDirection == kForward) {
        for (i=0;i<numKids;i++) {
          nsIContent * child;
          aParent->ChildAt(i, child);
          if (BuildBlockFromContent(child, aBlockText)) {
            NS_IF_RELEASE(child);
            return PR_TRUE;
          }
          NS_RELEASE(child);
        }
      } else { // Backward
        for (i=numKids-1;i>=0;i--) {
          nsIContent * child;
          aParent->ChildAt(i, child);
          if (BuildBlockFromContent(child, aBlockText)) {
            NS_IF_RELEASE(child);
            return PR_TRUE;
          }
          NS_RELEASE(child);
        }
      }
    }
    NS_RELEASE(atom);
  } else {
    static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
    nsIDOMText* textContent;
    nsresult rv = aParent->QueryInterface(kIDOMTextIID,(void **)&textContent);
    if (NS_OK == rv) {
      nsString stringBuf;
      textContent->GetData(stringBuf);
      aBlockText.AddSubText(aParent, stringBuf);
      NS_RELEASE(textContent);
    }
  }
  return PR_FALSE;
}

//----------------------------------
// This function moves down through
// the hiearchy and build the block of text
//-----------------------------------------
PRBool nsHTMLDocument::BuildBlockFromStack(nsIContent * aParent,
                                           BlockText  & aBlockText,
                                           PRInt32      aStackInx) 
{
  nsIContent * stackParent = mParentStack[aStackInx];
  nsIContent * stackChild  = mChildStack[aStackInx];

  PRInt32      inx;
  PRInt32      j;
  aParent->IndexOf(stackChild, inx);

  // Forward
  if (mSearchDirection == kForward) {
    PRInt32 numKids;
    aParent->ChildCount(numKids);
    for (j=inx;j<numKids;j++) {
      nsIContent * child;
      aParent->ChildAt(j, child);
      if (child == stackChild && aStackInx < mStackInx) {
        if (BuildBlockFromStack(child, aBlockText, aStackInx+1)) {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        }
      } else {
        if (!BuildBlockTraversing(child, aBlockText)) {
          if (SearchBlock(aBlockText, *mSearchStr)) {
            NS_IF_RELEASE(child);
            return PR_TRUE;
          }
        } else {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        }
      }
      NS_RELEASE(child);
    }
  } else { // Backward
    for (j=inx;j>=0;j--) {
      nsIContent * child;
      aParent->ChildAt(j, child);
      if (child == stackChild && aStackInx < mStackInx) {
        if (BuildBlockFromStack(child, aBlockText, aStackInx+1)) {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        }
      } else {
        if (!BuildBlockTraversing(child, aBlockText)) {
          if (SearchBlock(aBlockText, *mSearchStr)) {
            NS_IF_RELEASE(child);
            return PR_TRUE;
          }
        } else {
         NS_IF_RELEASE(child);
         return PR_TRUE;
        }
      }
      NS_RELEASE(child);
    }

  }
  return PR_FALSE;

}

//----------------------------------
// This function moves down through
// the hiearchy and build the block of text
//-----------------------------------------
PRBool nsHTMLDocument::BuildBlock(nsIContent * aParent,
                                  BlockText  & aBlockText) 
{
  nsIContent * stackParent = mParentStack[0];
  nsIContent * stackChild  = mChildStack[0];
  PRInt32      inx;
  PRInt32      j;
  stackParent->IndexOf(stackChild, inx);

  if (mSearchDirection == kForward) {
    PRInt32 numKids;
    stackParent->ChildCount(numKids);
    for (j=inx;j<numKids;j++) {
      nsIContent * child;
      stackParent->ChildAt(j, child);
      if (child == stackChild && mStackInx > 1) {
        if (BuildBlockFromStack(child, aBlockText, 1)) {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        }
      } else {
        if (BuildBlockTraversing(child, aBlockText)) {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        } else {
          if (SearchBlock(aBlockText, *mSearchStr)) {
            NS_IF_RELEASE(child);
            return PR_TRUE;
          }
        }
      }
      NS_IF_RELEASE(child);
    }
  } else {
    for (j=inx;j>=0;j--) {
      BlockText blockText;
      nsIContent * child;
      stackParent->ChildAt(j, child);
      if (child == stackChild && mStackInx > 1) {
        if (BuildBlockFromStack(child, blockText, 1)) {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        }
      } else {
        if (BuildBlockTraversing(child, blockText)) {
          NS_IF_RELEASE(child);
          return PR_TRUE;
        } else {
          if (SearchBlock(blockText, *mSearchStr)) {
            NS_IF_RELEASE(child);
            return PR_TRUE;
          }
        }
      }
      mAdjustToEnd = PR_TRUE;
      NS_IF_RELEASE(child);
    }
  }
  return PR_FALSE;
}

/**
  * Finds text in content
 */
NS_IMETHODIMP nsHTMLDocument::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  aIsFound         = PR_FALSE;
  mShouldMatchCase = aMatchCase;

  if (mSearchStr == nsnull) {
    mSearchStr = new nsString(aSearchStr);
  } else {
    mSearchStr->SetLength(0);
    mSearchStr->Append(aSearchStr);
  }

  nsIContent * start = nsnull;
  nsIContent * end   = nsnull;
  nsIContent * body  = nsnull;

  nsString bodyStr("BODY");
  PRInt32 i, n;
  mRootContent->ChildCount(n);
  for (i=0;i<n;i++) {
    nsIContent * child;
    mRootContent->ChildAt(i, child);
    PRBool isSynthetic;
    child->IsSynthetic(isSynthetic);
    if (!isSynthetic) {
      nsIAtom * atom;
      child->GetTag(atom);/* XXX leak */
      if (bodyStr.EqualsIgnoreCase(atom)) {
        body = child;
        break;
      }

    }
    NS_RELEASE(child);
  }

  if (body == nsnull) {
    return NS_OK;
  }

  start = body;
  NS_ADDREF(body);
  // Find Very first Piece of Content
  PRInt32 snc;
  start->ChildCount(snc);
  while (snc > 0) {
    nsIContent * child = start;
    child->ChildAt(0, start);
    NS_RELEASE(child);
  }

  end = body;
  NS_ADDREF(body);
  // Last piece of Content
  PRInt32 count;
  end->ChildCount(count);
  while (count > 0) {
    nsIContent * child = end;
    child->ChildAt(count-1, end);
    NS_RELEASE(child);
    end->ChildCount(count);
  }

  nsSelectionRange * range        = mSelection->GetRange();
  nsIContent       * startContent = range->GetStartContent();
  nsIContent       * endContent   = range->GetEndContent();
  nsSelectionPoint * startPnt     = range->GetStartPoint();
  nsSelectionPoint * endPnt       = range->GetEndPoint();

  mSearchDirection = aSearchDown? kForward:kBackward;
  nsIContent * searchContent;

  if (endContent == nsnull && mSearchDirection == kForward) {
    searchContent = (mSearchDirection == kForward ? body:end);
    //NS_ADDREF(searchContent);
    BlockText blockText;
    if (!BuildBlockTraversing(searchContent, blockText)) {
      if (SearchBlock(blockText, *mSearchStr)) {
        aIsFound = PR_TRUE;
      }
    }
    //NS_RELEASE(searchContent);
  } else {
    searchContent          = (mSearchDirection == kForward ? endContent:startContent);
    mLastBlockSearchOffset = (mSearchDirection == kForward ? endPnt->GetOffset():startPnt->GetOffset());
    mAdjustToEnd = PR_FALSE;
    if (mSearchDirection == kBackward && endContent == nsnull) {
      searchContent = end;
      mAdjustToEnd  = PR_TRUE;
    }
    nsIContent * blockContent = FindBlockParent(searchContent); // this ref counts blockContent
    while (blockContent != nsnull) {

      BlockText blockText;
      if (BuildBlock(blockContent, blockText)) {
        aIsFound = PR_TRUE;
        break;
      }

      mLastBlockSearchOffset = 0;
      mAdjustToEnd           = PR_TRUE;

      NS_RELEASE(blockContent);
      blockContent = FindBlockParent(blockContent, PR_TRUE);
    }

    // Clear up stack and release content nodes on it
    PRInt32 i;
    for (i=0;i<mStackInx;i++) {
      NS_RELEASE(mParentStack[i]);
      NS_RELEASE(mChildStack[i]);
    }
  }

  NS_IF_RELEASE(startContent);
  NS_IF_RELEASE(endContent);
  NS_IF_RELEASE(body);
  NS_IF_RELEASE(start);
  NS_IF_RELEASE(end);

  SetDisplaySelection(PR_TRUE); 

  return NS_OK;
}

