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
#include "nsIPresState.h"
#include "nsIPresContext.h"
#include "nsIPrintContext.h"
#include "nsIFrameUtil.h"

#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIDOMRange.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIEventListenerManager.h"
#include "nsILayoutDebugger.h"
#include "nsIElementFactory.h"
#include "nsIDocumentEncoder.h"
#include "nsCOMPtr.h"

#include "nsIBoxObject.h"

#include "nsIAutoCopy.h"

#include "nsINodeInfo.h"
#include "nsIFrameTraversal.h"
#include "nsICSSFrameConstructor.h"

class nsIDocumentLoaderFactory;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

static NS_DEFINE_CID(kPresShellCID,  NS_PRESSHELL_CID);
static NS_DEFINE_CID(kPresStateCID,  NS_PRESSTATE_CID);
static NS_DEFINE_CID(kGalleyContextCID,  NS_GALLEYCONTEXT_CID);
static NS_DEFINE_CID(kPrintContextCID,  NS_PRINTCONTEXT_CID);
static NS_DEFINE_CID(kTextNodeCID,   NS_TEXTNODE_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,  NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kFrameUtilCID,  NS_FRAME_UTIL_CID);
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);
static NS_DEFINE_CID(kCSSFrameConstructorCID, NS_CSSFRAMECONSTRUCTOR_CID);
static NS_DEFINE_CID(kPrintPreviewContextCID, NS_PRINT_PREVIEW_CONTEXT_CID);

static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
static NS_DEFINE_CID(kLayoutDebuggerCID, NS_LAYOUT_DEBUGGER_CID);

static NS_DEFINE_CID(kBoxObjectCID, NS_BOXOBJECT_CID);
static NS_DEFINE_CID(kTreeBoxObjectCID, NS_TREEBOXOBJECT_CID);
static NS_DEFINE_CID(kScrollBoxObjectCID, NS_SCROLLBOXOBJECT_CID);
static NS_DEFINE_CID(kMenuBoxObjectCID, NS_MENUBOXOBJECT_CID);
static NS_DEFINE_CID(kPopupSetBoxObjectCID, NS_POPUPSETBOXOBJECT_CID);
static NS_DEFINE_CID(kBrowserBoxObjectCID, NS_BROWSERBOXOBJECT_CID);
static NS_DEFINE_CID(kEditorBoxObjectCID, NS_EDITORBOXOBJECT_CID);
static NS_DEFINE_CID(kIFrameBoxObjectCID, NS_IFRAMEBOXOBJECT_CID);
static NS_DEFINE_CID(kOutlinerBoxObjectCID, NS_OUTLINERBOXOBJECT_CID);

static NS_DEFINE_CID(kAutoCopyServiceCID, NS_AUTOCOPYSERVICE_CID);

extern nsresult NS_NewLayoutDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
#ifdef NS_DEBUG
extern nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);
extern nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
#endif

extern nsresult NS_NewBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewTreeBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewScrollBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewMenuBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewEditorBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewPopupSetBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewBrowserBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewIFrameBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewOutlinerBoxObject(nsIBoxObject** aResult);

extern nsresult NS_NewAutoCopyService(nsIAutoCopyService** aResult);

extern nsresult NS_NewGalleyContext(nsIPresContext** aResult);

extern nsresult NS_NewPresShell(nsIPresShell** aResult);
extern nsresult NS_NewPresState(nsIPresState** aResult);
extern nsresult NS_NewPrintContext(nsIPrintContext** aResult);

extern nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);
extern nsresult NS_CreateCSSFrameConstructor(nsICSSFrameConstructor** aResult);

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
#if 1
// XXX replace these with nsIElementFactory calls
// XXX why the heck is this exported???? bad bad bad bad
  if (mClassID.Equals(kPresShellCID)) {
    res = NS_NewPresShell((nsIPresShell**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPresShell", res);
      return res;
    }
  }
#endif
  else if (mClassID.Equals(kPresStateCID)) {
    res = NS_NewPresState((nsIPresState**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPresState", res);
      return res;
    }
  }
  else if (mClassID.Equals(kFrameTraversalCID)) {
    res = NS_CreateFrameTraversal((nsIFrameTraversal**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_CreateFrameTraversal", res);
      return res;
    }
  }
  else if (mClassID.Equals(kCSSFrameConstructorCID)) {
    res = NS_CreateCSSFrameConstructor((nsICSSFrameConstructor**)&inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_CreateCSSFrameConstructor", res);
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
  else if (mClassID.Equals(kPrintContextCID)) {
    res = NS_NewPrintContext((nsIPrintContext**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewPrintContext", res);
      return res;
    }
  }
  else if (mClassID.Equals(kGalleyContextCID)) {
    res = NS_NewGalleyContext((nsIPresContext**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewGalleyContext", res);
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
  else if (mClassID.Equals(kOutlinerBoxObjectCID)) {
    res = NS_NewOutlinerBoxObject((nsIBoxObject**) &inst);
    if (NS_FAILED(res)) {
      LOG_NEW_FAILURE("NS_NewOutlinerBoxObject", res);
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
