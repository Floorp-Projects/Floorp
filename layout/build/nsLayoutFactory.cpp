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
#include "nscore.h"
#include "nslayout.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsLayoutCID.h"
#include "nsIDocument.h"
#include "nsIHTMLContent.h"
#include "nsICollection.h"
#include "nsIPresShell.h"
#include "nsISelection.h"

#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsDOMCID.h"
#include "nsIDOMNativeObjectRegistry.h"
#include "nsIServiceManager.h"
#include "nsICSSParser.h"
#include "nsIHTMLStyleSheet.h"
#include "nsICollection.h"
#include "nsIDOMRange.h"


static NS_DEFINE_IID(kCHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kCXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kCImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kCCSSParserCID,     NS_CSSPARSER_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID, NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_IID(kCHTMLImageElementFactoryCID, NS_HTMLIMAGEELEMENTFACTORY_CID);
static NS_DEFINE_IID(kIDOMHTMLImageElementFactoryIID, NS_IDOMHTMLIMAGEELEMENTFACTORY_IID);
static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);
static NS_DEFINE_IID(kCRangeListCID, NS_RANGELIST_CID);
static NS_DEFINE_IID(kCRangeCID,     NS_RANGE_CID);
static NS_DEFINE_CID(kPresShellCID,  NS_PRESSHELL_CID);
static NS_DEFINE_CID(kTextNodeCID,   NS_TEXTNODE_CID);
static NS_DEFINE_CID(kSelectionCID,  NS_SELECTION_CID);


nsresult NS_NewRangeList(nsICollection **);
nsresult NS_NewRange(nsIDOMRange **);



class HTMLImageElementFactory : public nsIDOMHTMLImageElementFactory {
public:
  HTMLImageElementFactory();
  ~HTMLImageElementFactory();
  
  NS_DECL_ISUPPORTS
  
  NS_IMETHOD   CreateInstance(nsIDOMHTMLImageElement **aReturn);
};

HTMLImageElementFactory::HTMLImageElementFactory()
{
  NS_INIT_REFCNT();
}

HTMLImageElementFactory::~HTMLImageElementFactory()
{
}

NS_IMPL_ISUPPORTS(HTMLImageElementFactory, kIDOMHTMLImageElementFactoryIID);

NS_IMETHODIMP   
HTMLImageElementFactory::CreateInstance(nsIDOMHTMLImageElement **aReturn)
{
  nsIHTMLContent *content;

  nsresult rv = NS_NewHTMLImageElement(&content, nsHTMLAtoms::img);
  if (NS_OK != rv) {
    return rv;
  }
  rv = content->QueryInterface(kIDOMHTMLImageElementIID, (void **)aReturn);
  NS_RELEASE(content);
  return rv;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kDOMNativeObjectRegistryCID, NS_DOM_NATIVE_OBJECT_REGISTRY_CID);
static NS_DEFINE_IID(kIDOMNativeObjectRegistryIID, NS_IDOM_NATIVE_OBJECT_REGISTRY_IID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

class nsLayoutFactory : public nsIFactory
{   
  public:   
    // nsISupports methods   
    NS_IMETHOD QueryInterface(const nsIID &aIID,    
                              void **aResult);   
    NS_IMETHOD_(nsrefcnt) AddRef(void);   
    NS_IMETHOD_(nsrefcnt) Release(void);   

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsLayoutFactory(const nsCID &aClass);   

  protected:
    virtual ~nsLayoutFactory();   

  private:   
    nsrefcnt  mRefCnt;   
    nsCID     mClassID;
};   

nsLayoutFactory::nsLayoutFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsLayoutFactory::~nsLayoutFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsLayoutFactory::QueryInterface(const nsIID &aIID,   
                                      void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

nsrefcnt nsLayoutFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt nsLayoutFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult nsLayoutFactory::CreateInstance(nsISupports *aOuter,  
                                         const nsIID &aIID,  
                                         void **aResult)  
{  
  nsresult res;
  PRBool refCounted = PR_TRUE;

  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  // XXX ClassID check happens here
  if (mClassID.Equals(kCHTMLDocumentCID)) {
    res = NS_NewHTMLDocument((nsIDocument **)&inst);
    if (!NS_SUCCEEDED(res)) {
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCXMLDocumentCID)) {
    res = NS_NewXMLDocument((nsIDocument **)&inst);
    if (!NS_SUCCEEDED(res)) {
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCImageDocumentCID)) {
    res = NS_NewImageDocument((nsIDocument **)&inst);
    if (!NS_SUCCEEDED(res)) {
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCHTMLImageElementFactoryCID)) {
    inst = new HTMLImageElementFactory();
    refCounted = PR_FALSE;
  }
  else if (mClassID.Equals(kPresShellCID)) {
    res = NS_NewPresShell((nsIPresShell**) &inst);
    if (NS_FAILED(res)) {
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCRangeListCID)) {
    res = NS_NewRangeList((nsICollection **)&inst);
    if (!NS_SUCCEEDED(res)) {
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCRangeCID)) {
    res = NS_NewRange((nsIDOMRange **)&inst);
    if (!NS_SUCCEEDED(res)) {
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCCSSParserCID)) {
    // XXX this should really be factored into a style-specific DLL so
    // that all the HTML, generic layout, and style stuff isn't munged
    // together.
    if (NS_FAILED(res = NS_NewCSSParser((nsICSSParser**)&inst)))
      return res;
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kHTMLStyleSheetCID)) {
    // XXX ibid
    if (NS_FAILED(res = NS_NewHTMLStyleSheet((nsIHTMLStyleSheet**)&inst)))
      return res;
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kTextNodeCID)) {
    // XXX ibid
    if (NS_FAILED(res = NS_NewTextNode((nsIHTMLContent**) &inst)))
      return res;
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kSelectionCID)) {
    if (NS_FAILED(res = NS_NewSelection((nsISelection**) &inst)))
      return res;
    refCounted = PR_TRUE;
  }
  else 
  {
    return NS_NOINTERFACE;
  }
  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  res = inst->QueryInterface(aIID, aResult);

  if (refCounted) {
    NS_RELEASE(inst);
  }
  else if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsLayoutFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

class NativeObjectRegistryManager {
public:
  static nsIDOMNativeObjectRegistry *gRegistry;
  
  NativeObjectRegistryManager() {};
  ~NativeObjectRegistryManager() {
    if (gRegistry) {
      nsServiceManager::ReleaseService(kDOMNativeObjectRegistryCID,
                                       gRegistry);
      gRegistry = nsnull;
    }
  }
};

nsIDOMNativeObjectRegistry *NativeObjectRegistryManager::gRegistry = nsnull;
static NativeObjectRegistryManager gManager;

// return the proper factory to the caller
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_LAYOUT nsresult NSGetFactory_LAYOUT_DLL(const nsCID &aClass, nsIFactory **aFactory)
#else
extern "C" NS_LAYOUT nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
#endif
{
  if (nsnull == NativeObjectRegistryManager::gRegistry) {
    nsresult result = nsServiceManager::GetService(kDOMNativeObjectRegistryCID,
                                                   kIDOMNativeObjectRegistryIID,
                                                   (nsISupports **)&NativeObjectRegistryManager::gRegistry);
    if (NS_OK == result) {
      NativeObjectRegistryManager::gRegistry->RegisterFactory("HTMLImageElement", kCHTMLImageElementFactoryCID);
    }
  }

  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsLayoutFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
