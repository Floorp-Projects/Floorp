/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nslayout.h"
#include "nsLayoutModule.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsLayoutCID.h"
#include "nsIDocument.h"
#include "nsIHTMLContent.h"
#include "nsITextContent.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsISelection.h"
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
#include "nsIElementFactory.h"
#include "nsIDocumentEncoder.h"
#include "nsCOMPtr.h"
#include "nsIFrameSelection.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIPrivateDOMImplementation.h"

#include "nsIXBLService.h"
#include "nsIBindingManager.h"

#include "nsIBoxObject.h"

#include "nsIAutoCopy.h"
#include "nsContentPolicyUtils.h"
#include "nsIXIFConverter.h"

#include "nsINodeInfo.h"
#include "nsIComputedDOMStyle.h"

class nsIDocumentLoaderFactory;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID, NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kCSSParserCID,     NS_CSSPARSER_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID, NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID, NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kCSSLoaderCID, NS_CSS_LOADER_CID);
static NS_DEFINE_IID(kHTMLImageElementCID, NS_HTMLIMAGEELEMENT_CID);
static NS_DEFINE_IID(kHTMLOptionElementCID, NS_HTMLOPTIONELEMENT_CID);

static NS_DEFINE_CID(kSelectionCID, NS_SELECTION_CID);
static NS_DEFINE_IID(kFrameSelectionCID, NS_FRAMESELECTION_CID);
static NS_DEFINE_IID(kDOMSelectionCID, NS_DOMSELECTION_CID);

static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);
static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kGeneratedContentIteratorCID, NS_GENERATEDCONTENTITERATOR_CID);
static NS_DEFINE_IID(kGeneratedSubtreeIteratorCID, NS_GENERATEDSUBTREEITERATOR_CID);
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

static NS_DEFINE_CID(kXBLServiceCID, NS_XBLSERVICE_CID);
static NS_DEFINE_CID(kBindingManagerCID, NS_BINDINGMANAGER_CID);

static NS_DEFINE_CID(kBoxObjectCID, NS_BOXOBJECT_CID);
static NS_DEFINE_CID(kTreeBoxObjectCID, NS_TREEBOXOBJECT_CID);
static NS_DEFINE_CID(kScrollBoxObjectCID, NS_SCROLLBOXOBJECT_CID);
static NS_DEFINE_CID(kMenuBoxObjectCID, NS_MENUBOXOBJECT_CID);
static NS_DEFINE_CID(kPopupSetBoxObjectCID, NS_POPUPSETBOXOBJECT_CID);
static NS_DEFINE_CID(kBrowserBoxObjectCID, NS_BROWSERBOXOBJECT_CID);
static NS_DEFINE_CID(kEditorBoxObjectCID, NS_EDITORBOXOBJECT_CID);
static NS_DEFINE_CID(kIFrameBoxObjectCID, NS_IFRAMEBOXOBJECT_CID);

static NS_DEFINE_CID(kDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kNodeInfoManagerCID, NS_NODEINFOMANAGER_CID);
static NS_DEFINE_CID(kAutoCopyServiceCID, NS_AUTOCOPYSERVICE_CID);
static NS_DEFINE_CID(kContentPolicyCID, NS_CONTENTPOLICY_CID);
static NS_DEFINE_CID(kXIFConverterCID, NS_XIFCONVERTER_CID);
static NS_DEFINE_CID(kComputedDOMStyleCID, NS_COMPUTEDDOMSTYLE_CID);


extern nsresult NS_NewSelection(nsIFrameSelection** aResult);
extern nsresult NS_NewDomSelection(nsISelection** aResult);
extern nsresult NS_NewRange(nsIDOMRange** aResult);
extern nsresult NS_NewContentIterator(nsIContentIterator** aResult);
extern nsresult NS_NewGenRegularIterator(nsIContentIterator** aResult);
extern nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aResult);
extern nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);

extern nsresult NS_NewLayoutDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
#ifdef NS_DEBUG
extern nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);
extern nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
#endif
extern nsresult NS_NewHTMLElementFactory(nsIElementFactory** aResult);
extern nsresult NS_NewXMLElementFactory(nsIElementFactory** aResult);

extern nsresult NS_NewHTMLEncoder(nsIDocumentEncoder** aResult);
extern nsresult NS_NewTextEncoder(nsIDocumentEncoder** aResult);

extern nsresult NS_NewXBLService(nsIXBLService** aResult);

extern nsresult NS_NewBindingManager(nsIBindingManager** aResult);

extern nsresult NS_NewBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewTreeBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewScrollBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewMenuBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewEditorBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewPopupSetBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewBrowserBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewIFrameBoxObject(nsIBoxObject** aResult);

extern nsresult NS_NewNodeInfoManager(nsINodeInfoManager** aResult);

extern nsresult NS_NewAutoCopyService(nsIAutoCopyService** aResult);
extern nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);

extern nsresult NS_NewXIFConverter(nsIXIFConverter** aResult);

//----------------------------------------------------------------------

nsLayoutFactory::nsLayoutFactory(const nsCID &aClass)
{
  NS_INIT_ISUPPORTS();
  mClassID = aClass;
#if 0
  char* cs = aClass.ToString();
  printf("+++ Creating layout factory for %s\n", cs);
  nsCRT::free(cs);
#endif
}

nsLayoutFactory::~nsLayoutFactory()
{
#if 0
  char* cs = mClassID.ToString();
  printf("+++ Destroying layout factory for %s\n", cs);
  nsCRT::free(cs);
#endif
}

NS_IMPL_ISUPPORTS(nsLayoutFactory, NS_GET_IID(nsIFactory))

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

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsISupports *inst = nsnull;

  // XXX ClassID check happens here
  if (mClassID.Equals(kHTMLDocumentCID)) {
    res = NS_NewHTMLDocument((nsIDocument **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLDocument", res);
      return res;
    }
  }
  else if (mClassID.Equals(kXMLDocumentCID)) {
    res = NS_NewXMLDocument((nsIDocument **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewXMLDocument", res);
      return res;
    }
  }
  else if (mClassID.Equals(kImageDocumentCID)) {
    res = NS_NewImageDocument((nsIDocument **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewImageDocument", res);
      return res;
    }
  }
#if 1
// XXX replace these with nsIElementFactory calls
  else if (mClassID.Equals(kHTMLImageElementCID)) {
    // Note! NS_NewHTMLImageElement is special cased to handle a null nodeinfo
    res = NS_NewHTMLImageElement((nsIHTMLContent**)&inst, nsnull);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLImageElement", res);
      return res;
    }
  }
  else if (mClassID.Equals(kHTMLOptionElementCID)) {
    // Note! NS_NewHTMLOptionElement is special cased to handle a null nodeinfo
    res = NS_NewHTMLOptionElement((nsIHTMLContent**)&inst, nsnull);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLOptionElement", res);
      return res;
    }
  }
// XXX why the heck is this exported???? bad bad bad bad
  else if (mClassID.Equals(kPresShellCID)) {
    res = NS_NewPresShell((nsIPresShell**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPresShell", res);
      return res;
    }
  }
#endif
  else if (mClassID.Equals(kFrameSelectionCID)) {
    res = NS_NewSelection((nsIFrameSelection**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewSelection", res);
      return res;
    }
  }
  else if (mClassID.Equals(kDOMSelectionCID)) {
    res = NS_NewDomSelection((nsISelection**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewDomSelection", res);
      return res;
    }
  }
  else if (mClassID.Equals(kRangeCID)) {
    res = NS_NewRange((nsIDOMRange **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewRange", res);
      return res;
    }
  }
  else if (mClassID.Equals(kContentIteratorCID)) {
    res = NS_NewContentIterator((nsIContentIterator **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewContentIterator", res);
      return res;
    }
  }
  else if (mClassID.Equals(kGeneratedContentIteratorCID)) {
    res = NS_NewGenRegularIterator((nsIContentIterator **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewGenRegularIterator", res);
      return res;
    }
  }
  else if (mClassID.Equals(kSubtreeIteratorCID)) {
    res = NS_NewContentSubtreeIterator((nsIContentIterator **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewContentSubtreeIterator", res);
      return res;
    }
  }
  else if (mClassID.Equals(kGeneratedSubtreeIteratorCID)) {
    res = NS_NewGenSubtreeIterator((nsIContentIterator **)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewGenSubtreeIterator", res);
      return res;
    }
  }
  else if (mClassID.Equals(kCSSParserCID)) {
    res = NS_NewCSSParser((nsICSSParser**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewCSSParser", res);
      return res;
    }
  }
  else if (mClassID.Equals(kHTMLStyleSheetCID)) {
    res = NS_NewHTMLStyleSheet((nsIHTMLStyleSheet**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLStyleSheet", res);
      return res;
    }
  }
  else if (mClassID.Equals(kHTMLCSSStyleSheetCID)) {
    res = NS_NewHTMLCSSStyleSheet((nsIHTMLCSSStyleSheet**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLCSSStyleSheet", res);
      return res;
    }
  }
  else if (mClassID.Equals(kCSSLoaderCID)) {
    res = NS_NewCSSLoader((nsICSSLoader**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewCSSLoader", res);
      return res;
    }
  }
  else if (mClassID.Equals(kTextNodeCID)) {
    res = NS_NewTextNode((nsIContent**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewTextNode", res);
      return res;
    }
  }
  else if (mClassID.Equals(kNameSpaceManagerCID)) {
    res = NS_NewNameSpaceManager((nsINameSpaceManager**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewNameSpaceManager", res);
      return res;
    }
  }
  else if (mClassID.Equals(kEventListenerManagerCID)) {
    res = NS_NewEventListenerManager((nsIEventListenerManager**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewEventListenerManager", res);
      return res;
    }
  }
  else if (mClassID.Equals(kPrintPreviewContextCID)) {
    res = NS_NewPrintPreviewContext((nsIPresContext**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPrintPreviewContext", res);
      return res;
    }
  }
  else if (mClassID.Equals(kLayoutDocumentLoaderFactoryCID)) {
    res = NS_NewLayoutDocumentLoaderFactory((nsIDocumentLoaderFactory**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewLayoutDocumentLoaderFactory", res);
      return res;
    }
  }
#ifdef DEBUG
  else if (mClassID.Equals(kFrameUtilCID)) {
    res = NS_NewFrameUtil((nsIFrameUtil**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewFrameUtil", res);
      return res;
    }
  }
  else if (mClassID.Equals(kLayoutDebuggerCID)) {
    res = NS_NewLayoutDebugger((nsILayoutDebugger**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewLayoutDebugger", res);
      return res;
    }
  }
#endif
  else if (mClassID.Equals(kHTMLElementFactoryCID)) {
    res = NS_NewHTMLElementFactory((nsIElementFactory**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewHTMLElementFactory", res);
      return res;
    }
  }
  else if (mClassID.Equals(kXMLElementFactoryCID)) {
    res = NS_NewXMLElementFactory((nsIElementFactory**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewXMLElementFactory", res);
      return res;
    }
  }
  else if (mClassID.Equals(kTextEncoderCID)) {
    res = NS_NewTextEncoder((nsIDocumentEncoder**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewTextEncoder", res);
      return res;
    }
  }
  else if (mClassID.Equals(kXBLServiceCID)) {
    res = NS_NewXBLService((nsIXBLService**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewXBLService", res);
      return res;
    }
  }
  else if (mClassID.Equals(kBindingManagerCID)) {
    res = NS_NewBindingManager((nsIBindingManager**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewBindingManager", res);
      return res;
    }
  }
  else if (mClassID.Equals(kBoxObjectCID)) {
    res = NS_NewBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kMenuBoxObjectCID)) {
    res = NS_NewMenuBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewMenuBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kTreeBoxObjectCID)) {
    res = NS_NewTreeBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewTreeBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kPopupSetBoxObjectCID)) {
    res = NS_NewPopupSetBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPopupSetBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kBrowserBoxObjectCID)) {
    res = NS_NewBrowserBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewBrowserBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kEditorBoxObjectCID)) {
    res = NS_NewEditorBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewEditorBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kIFrameBoxObjectCID)) {
    res = NS_NewIFrameBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewIFrameBoxObject", res);
      return res;
    }
  }
  else if (mClassID.Equals(kNodeInfoManagerCID)) {
    res = NS_NewNodeInfoManager((nsINodeInfoManager**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewNodeInfoManager", res);
      return res;
    }
  }
  else if (mClassID.Equals(kAutoCopyServiceCID)) {
    res = NS_NewAutoCopyService((nsIAutoCopyService**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewAutoCopyService", res);
      return res;
    }
  }
  else if (mClassID.Equals(kXIFConverterCID)) {
    res = NS_NewXIFConverter((nsIXIFConverter**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewAutoCopyService", res);
      return res;
    }
  }  
  else if (mClassID.Equals(kDOMImplementationCID)) {
    res = NS_NewDOMImplementation((nsIDOMDOMImplementation**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewDOMImplementation", res);
      return res;
    }
  }
  else if (mClassID.Equals(kContentPolicyCID)) {
    res = NS_NewContentPolicy((nsIContentPolicy**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewContentPolicy", res);
      return res;
    }
  }
  else if (mClassID.Equals(kComputedDOMStyleCID)) {
    res = NS_NewComputedDOMStyle((nsIComputedDOMStyle**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewComputedDOMStyle", res);
      return res;
    }
  }
  else if (mClassID.Equals(kScrollBoxObjectCID)) {
    res = NS_NewScrollBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewScrollBoxObject", res);
      return res;
    }
  }
  else {
    return NS_NOINTERFACE;
  }

  if (NS_FAILED(res)) return res;

  res = inst->QueryInterface(aIID, aResult);

  NS_RELEASE(inst);

  if (NS_FAILED(res)) {
    // We didn't get the right interface, so clean up
    LOG_NEW_FAILURE("final QueryInterface", res);
  }

  return res;
}

nsresult nsLayoutFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}
