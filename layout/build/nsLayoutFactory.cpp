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
#include "nsITextContent.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIDOMSelection.h"
#include "nsIFrameUtil.h"

#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsICSSParser.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsIDOMRange.h"
#include "nsIContentIterator.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIEventListenerManager.h"
#include "nsILayoutDebugger.h"
#include "nsIHTMLElementFactory.h"
#include "nsIDocumentEncoder.h"
#include "nsCOMPtr.h"
#include "nsIFrameSelection.h"

#include "nsCSSKeywords.h"  // to addref/release table
#include "nsCSSProps.h"  // to addref/release table
#include "nsCSSAtoms.h"  // to addref/release table
#include "nsColorNames.h"  // to addref/release table

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#endif
#include "nsLayoutAtoms.h"

class nsIDocumentLoaderFactory;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kCSSParserCID,     NS_CSSPARSER_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID, NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID, NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kCSSLoaderCID, NS_CSS_LOADER_CID);
static NS_DEFINE_IID(kHTMLImageElementCID, NS_HTMLIMAGEELEMENT_CID);
static NS_DEFINE_IID(kHTMLOptionElementCID, NS_HTMLOPTIONELEMENT_CID);

static NS_DEFINE_CID(kSelectionCID, NS_SELECTION_CID);
static NS_DEFINE_IID(kFrameSelectionCID, NS_FRAMESELECTION_CID);
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);
static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);

static NS_DEFINE_CID(kPresShellCID,  NS_PRESSHELL_CID);
static NS_DEFINE_CID(kTextNodeCID,   NS_TEXTNODE_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,  NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kFrameUtilCID,  NS_FRAME_UTIL_CID);
static NS_DEFINE_CID(kEventListenerManagerCID, NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kPrintPreviewContextCID, NS_PRINT_PREVIEW_CONTEXT_CID);

static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
static NS_DEFINE_CID(kLayoutDebuggerCID, NS_LAYOUT_DEBUGGER_CID);
static NS_DEFINE_CID(kHTMLElementFactoryCID, NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kTextEncoderCID, NS_TEXT_ENCODER_CID);

extern nsresult NS_NewRangeList(nsIFrameSelection** aResult);
extern nsresult NS_NewRange(nsIDOMRange** aResult);
extern nsresult NS_NewContentIterator(nsIContentIterator** aResult);
extern nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aResult);
extern nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);

extern nsresult NS_NewLayoutDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
extern nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
extern nsresult NS_NewHTMLElementFactory(nsIHTMLElementFactory** aResult);
extern nsresult NS_NewHTMLEncoder(nsIDocumentEncoder** aResult);
extern nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult);

////////////////////////////////////////////////////////////

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
  nsCSSAtoms::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsHTMLAtoms::AddRefAtoms();
#ifdef INCLUDE_XUL
  nsXULAtoms::AddRefAtoms();
#endif
}   

nsLayoutFactory::~nsLayoutFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsCSSAtoms::ReleaseAtoms();
  nsHTMLAtoms::ReleaseAtoms();
#ifdef INCLUDE_XUL
  nsXULAtoms::ReleaseAtoms();
#endif
}   

nsresult
nsLayoutFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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

nsrefcnt
nsLayoutFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt
nsLayoutFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

#ifdef DEBUG
#define LOG_NEW_FAILURE(_msg,_ec)                                           \
  printf("nsLayoutFactory::CreateInstance failed for %s: error=%d(0x%x)\n", \
         _msg, _ec, _ec)
#else
#define LOG_NEW_FAILURE(_msg,_ec)
#endif

nsresult
nsLayoutFactory::CreateInstance(nsISupports *aOuter,  
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
  if (mClassID.Equals(kHTMLDocumentCID)) {
    res = NS_NewHTMLDocument((nsIDocument **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLDocument", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kXMLDocumentCID)) {
    res = NS_NewXMLDocument((nsIDocument **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewXMLDocument", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kImageDocumentCID)) {
    res = NS_NewImageDocument((nsIDocument **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewImageDocument", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
#if 1
// XXX replace these with nsIHTMLElementFactory calls
  else if (mClassID.Equals(kHTMLImageElementCID)) {
    res = NS_NewHTMLImageElement((nsIHTMLContent**)&inst, nsHTMLAtoms::img);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLImageElement", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kHTMLOptionElementCID)) {
    res = NS_NewHTMLOptionElement((nsIHTMLContent**)&inst, nsHTMLAtoms::option);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLOptionElement", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
// XXX why the heck is this exported???? bad bad bad bad
  else if (mClassID.Equals(kPresShellCID)) {
    res = NS_NewPresShell((nsIPresShell**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPresShell", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
#endif
  else if (mClassID.Equals(kFrameSelectionCID)) {
    res = NS_NewRangeList((nsIFrameSelection**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewRangeList", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kRangeCID)) {
    res = NS_NewRange((nsIDOMRange **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewRange", res);
      return res;
    }
    refCounted = PR_TRUE;
  } 
  else if (mClassID.Equals(kContentIteratorCID)) {
    res = NS_NewContentIterator((nsIContentIterator **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewContentIterator", res);
      return res;
    }
    refCounted = PR_TRUE;
  } 
  else if (mClassID.Equals(kSubtreeIteratorCID)) {
    res = NS_NewContentSubtreeIterator((nsIContentIterator **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewContentSubtreeIterator", res);
      return res;
    }
    refCounted = PR_TRUE;
  } 
  else if (mClassID.Equals(kCSSParserCID)) {
    res = NS_NewCSSParser((nsICSSParser**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewCSSParser", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kHTMLStyleSheetCID)) {
    res = NS_NewHTMLStyleSheet((nsIHTMLStyleSheet**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLStyleSheet", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kHTMLCSSStyleSheetCID)) {
    res = NS_NewHTMLCSSStyleSheet((nsIHTMLCSSStyleSheet**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLCSSStyleSheet", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kCSSLoaderCID)) {
    res = NS_NewCSSLoader((nsICSSLoader**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewCSSLoader", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kTextNodeCID)) {
    res = NS_NewTextNode((nsIContent**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewTextNode", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kNameSpaceManagerCID)) {
    res = NS_NewNameSpaceManager((nsINameSpaceManager**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewNameSpaceManager", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kFrameUtilCID)) {
    res = NS_NewFrameUtil((nsIFrameUtil**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewFrameUtil", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kEventListenerManagerCID)) {
    res = NS_NewEventListenerManager((nsIEventListenerManager**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewEventListenerManager", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kPrintPreviewContextCID)) {
    res = NS_NewPrintPreviewContext((nsIPresContext**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPrintPreviewContext", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kLayoutDocumentLoaderFactoryCID)) {
    res = NS_NewLayoutDocumentLoaderFactory((nsIDocumentLoaderFactory**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewLayoutDocumentLoaderFactory", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kLayoutDebuggerCID)) {
    res = NS_NewLayoutDebugger((nsILayoutDebugger**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewLayoutDebugger", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kHTMLElementFactoryCID)) {
    res = NS_NewHTMLElementFactory((nsIHTMLElementFactory**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLElementFactory", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else if (mClassID.Equals(kTextEncoderCID)) {
    res = NS_NewTextEncoder((nsIDocumentEncoder**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewTextEncoder", res);
      return res;
    }
    refCounted = PR_TRUE;
  }
  else {
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
    LOG_NEW_FAILURE("final QueryInterface", res);
    delete inst;  
  }  

  return res;  
}  

nsresult nsLayoutFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////


static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_IID(kIScriptNameSpaceManagerIID, NS_ISCRIPTNAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);

class LayoutScriptNameSet : public nsIScriptExternalNameSet {
public:
  LayoutScriptNameSet();
  virtual ~LayoutScriptNameSet();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
  NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

LayoutScriptNameSet::LayoutScriptNameSet()
{
  NS_INIT_REFCNT();
}

LayoutScriptNameSet::~LayoutScriptNameSet()
{
}

NS_IMPL_ISUPPORTS(LayoutScriptNameSet, kIScriptExternalNameSetIID);

NS_IMETHODIMP 
LayoutScriptNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  return NS_OK;
}

NS_IMETHODIMP
LayoutScriptNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
  nsresult result = NS_OK;
  nsIScriptNameSpaceManager* manager;

  result = aScriptContext->GetNameSpaceManager(&manager);
  if (NS_OK == result) {
    result = manager->RegisterGlobalName("HTMLImageElement",
                                         kHTMLImageElementCID,
                                         PR_TRUE);
    if (NS_FAILED(result)) {
      NS_RELEASE(manager);
      return result;
    }

    result = manager->RegisterGlobalName("HTMLOptionElement",
                                         kHTMLOptionElementCID,
                                         PR_TRUE);
    if (NS_FAILED(result)) {
      NS_RELEASE(manager);
      return result;
    }
        
    NS_RELEASE(manager);
  }
  
  return result;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

static nsIScriptNameSetRegistry *gRegistry;

// return the proper factory to the caller
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_LAYOUT nsresult 
NSGetFactory_LAYOUT_DLL(nsISupports* serviceMgr,
                        const nsCID &aClass,
                        const char *aClassName,
                        const char *aProgID,
                        nsIFactory **aFactory)
#else
extern "C" NS_LAYOUT nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
#endif
{
  if (nsnull == gRegistry) {
    nsresult result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                                   kIScriptNameSetRegistryIID,
                                                   (nsISupports **)&gRegistry);
    if (NS_OK == result) {
      LayoutScriptNameSet* nameSet = new LayoutScriptNameSet();
      gRegistry->AddExternalNameSet(nameSet);
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

extern nsresult NS_RegisterDocumentFactories(nsIComponentManager* aCM,
                                             const char* aPath);
extern nsresult NS_UnregisterDocumentFactories(nsIComponentManager* aCM,
                                               const char* aPath);

#ifdef DEBUG
#define LOG_REGISTER_FAILURE(_msg,_ec) \
  printf("RegisterComponent failed for %s: error=%d(0x%x)\n", _msg, _ec, _ec)
#else
#define LOG_REGISTER_FAILURE(_msg,_ec)
#endif

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
  printf("*** Registering html library\n");
  nsresult rv = NS_OK;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsIComponentManager* cm;
  rv = servMgr->GetService(kComponentManagerCID,
                           nsIComponentManager::GetIID(),
                           (nsISupports**)&cm);
  if (NS_FAILED(rv)) {
    return rv;
  }

  do {
    rv = NS_RegisterDocumentFactories(cm, aPath);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("document factories", rv);
      break;
    }
// XXX duh: replace this with an array and a loop!
    rv = cm->RegisterComponent(kHTMLDocumentCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kHTMLDocumentCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kXMLDocumentCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kXMLDocumentCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kImageDocumentCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kImageDocumentCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kNameSpaceManagerCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kNameSpaceManagerCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kEventListenerManagerCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kEventListenerManagerCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kCSSParserCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kCSSParserCID", rv);
      break;
    }
#if 1
    rv = cm->RegisterComponent(kHTMLImageElementCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kHTMLImageElementCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kHTMLOptionElementCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kHTMLImageElementCID", rv);
      break;
    }
// XXX why the heck is this exported???? bad bad bad bad
    rv = cm->RegisterComponent(kPresShellCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kPresShellCID", rv);
      break;
    }
#endif
    rv = cm->RegisterComponent(kHTMLStyleSheetCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kHTMLStyleSheetCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kHTMLCSSStyleSheetCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kHTMLCSSStyleSheetCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kCSSLoaderCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kCSSLoaderCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kTextNodeCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kTextNodeCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kSelectionCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kSelectionCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kRangeCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kRangeCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kFrameSelectionCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kFrameSelection", rv);
      break;
    }
    rv = cm->RegisterComponent(kContentIteratorCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kContentIteratorCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kSubtreeIteratorCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kSubtreeIteratorCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kFrameUtilCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kFrameUtilCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kPrintPreviewContextCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kPrintPreviewContextCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kLayoutDebuggerCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kLayoutDebuggerCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kHTMLElementFactoryCID, NULL, NULL, aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE("kHTMLElementFactoryCID", rv);
      break;
    }
    rv = cm->RegisterComponent(kTextEncoderCID, "HTML document encoder",
                               NS_DOC_ENCODER_PROGID_BASE "text/html",
                               aPath,
                               PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE(NS_DOC_ENCODER_PROGID_BASE "text/html", rv);
      break;
    }
 
    rv = cm->RegisterComponent(kTextEncoderCID, "Plaintext document encoder",
                               NS_DOC_ENCODER_PROGID_BASE "text/plain",
                               aPath,
                               PR_TRUE, PR_TRUE);
   if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE(NS_DOC_ENCODER_PROGID_BASE "text/plain", rv);
      break;
    }    

   rv = cm->RegisterComponent(kTextEncoderCID, "XIF document encoder",
                               NS_DOC_ENCODER_PROGID_BASE "text/xif",
                               aPath,
                               PR_TRUE, PR_TRUE);
   if (NS_FAILED(rv)) {
      LOG_REGISTER_FAILURE(NS_DOC_ENCODER_PROGID_BASE "text/xif", rv);
      break;
    }    

   break;
  } while (PR_FALSE);

  servMgr->ReleaseService(kComponentManagerCID, cm);
  return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  printf("*** Unregistering html library\n");

  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsIComponentManager* cm;
  rv = servMgr->GetService(kComponentManagerCID,
                           nsIComponentManager::GetIID(),
                           (nsISupports**)&cm);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Save return value during debugging, both otherwise discard it
  // because there is no good reason for an unregister to fail!
  rv = NS_UnregisterDocumentFactories(cm, aPath);
  rv = cm->UnregisterComponent(kHTMLDocumentCID, aPath);
  rv = cm->UnregisterComponent(kXMLDocumentCID, aPath);
  rv = cm->UnregisterComponent(kImageDocumentCID, aPath);
  rv = cm->UnregisterComponent(kNameSpaceManagerCID, aPath);
  rv = cm->UnregisterComponent(kEventListenerManagerCID, aPath);
  rv = cm->UnregisterComponent(kCSSParserCID, aPath);
  rv = cm->UnregisterComponent(kHTMLStyleSheetCID, aPath);
  rv = cm->UnregisterComponent(kHTMLCSSStyleSheetCID, aPath);
  rv = cm->UnregisterComponent(kCSSLoaderCID, aPath);
  rv = cm->UnregisterComponent(kTextNodeCID, aPath);
  rv = cm->UnregisterComponent(kSelectionCID, aPath);
  rv = cm->UnregisterComponent(kRangeCID, aPath);
  rv = cm->UnregisterComponent(kFrameSelectionCID, aPath);
  rv = cm->UnregisterComponent(kContentIteratorCID, aPath);
  rv = cm->UnregisterComponent(kSubtreeIteratorCID, aPath);
  rv = cm->UnregisterComponent(kFrameUtilCID, aPath);
  rv = cm->UnregisterComponent(kLayoutDebuggerCID, aPath);
//rv = cm->UnregisterComponent(kHTMLEncoderCID, aPath);
  rv = cm->UnregisterComponent(kTextEncoderCID, aPath);

// XXX why the heck are these exported???? bad bad bad bad
#if 1
  rv = cm->UnregisterComponent(kPresShellCID, aPath);
  rv = cm->UnregisterComponent(kHTMLImageElementCID, aPath);
  rv = cm->UnregisterComponent(kHTMLOptionElementCID, aPath);
#endif

  servMgr->ReleaseService(kComponentManagerCID, cm);

  return NS_OK;
}
