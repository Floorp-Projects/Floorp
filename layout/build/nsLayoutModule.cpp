/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"

#include "nsLayoutCID.h"
#include "nsIAutoCopy.h"
#include "nsIBoxObject.h"
#include "nsICSSFrameConstructor.h"
#include "nsIFrameTraversal.h"
#include "nsIFrameUtil.h"
#include "nsILayoutDebugger.h"
#include "nsILayoutHistoryState.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPresState.h"
#include "nsIPrintContext.h"
#include "nsTextTransformer.h"

#include "nsXBLAtoms.h"     // to addref/release table
#include "nsHTMLAtoms.h"    // to addref/release table
#include "nsLayoutAtoms.h"  // to addref/release table
#include "nsCSSKeywords.h"  // to addref/release table
#include "nsCSSProps.h"     // to addref/release table
#include "nsCSSAtoms.h"     // to addref/release table
#include "nsColorNames.h"   // to addref/release table
#include "nsCSSFrameConstructor.h"
#include "nsSpaceManager.h"

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#include "nsRepeatService.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#endif

//MathML Mod - RBS
#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#endif

// SVG
#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#endif


//----------------------------------------------------------------------

// Perform our one-time intialization for this module
static PRBool gInitialized = PR_FALSE;

PR_STATIC_CALLBACK(nsresult)
Initialize(nsIModule* self)
{
  NS_PRECONDITION(!gInitialized, "module already initialized");
  if (gInitialized)
    return NS_OK;

  gInitialized = PR_TRUE;
    
  // Register all of our atoms once
  nsCSSAtoms::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsHTMLAtoms::AddRefAtoms();
  nsXBLAtoms::AddRefAtoms();
  nsLayoutAtoms::AddRefAtoms();
#ifdef INCLUDE_XUL
  nsXULAtoms::AddRefAtoms();
#endif
//MathML Mod - RBS
#ifdef MOZ_MATHML
  nsMathMLOperators::AddRefTable();
  nsMathMLAtoms::AddRefAtoms();
#endif

// SVG
#ifdef MOZ_SVG
  nsSVGAtoms::AddRefAtoms();
#endif

  nsCSSFrameConstructor::InitGlobals();

  return nsTextTransformer::Initialize();
}

// Shutdown this module, releasing all of the module resources
PR_STATIC_CALLBACK(void)
Shutdown(nsIModule* self)
{
  NS_PRECONDITION(gInitialized, "module not initialized");
  if (! gInitialized)
    return;

  gInitialized = PR_FALSE;

#ifdef INCLUDE_XUL
  nsRepeatService::Shutdown();
  nsSprocketLayout::Shutdown();
  nsStackLayout::Shutdown();
#endif

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsCSSAtoms::ReleaseAtoms();
  nsHTMLAtoms::ReleaseAtoms();
  nsXBLAtoms::ReleaseAtoms();
  nsLayoutAtoms::ReleaseAtoms();

#ifdef INCLUDE_XUL
  nsXULAtoms::ReleaseAtoms();
#endif  

//MathML Mod - RBS
#ifdef MOZ_MATHML
  nsMathMLOperators::ReleaseTable();
  nsMathMLAtoms::ReleaseAtoms();
#endif

// SVG
#ifdef MOZ_SVG
  nsSVGAtoms::ReleaseAtoms();
#endif

  nsCSSFrameConstructor::ReleaseGlobals();

  nsTextTransformer::Shutdown();

  nsSpaceManager::Shutdown();
}

#ifdef NS_DEBUG
extern nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);
extern nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
#endif

extern nsresult NS_NewBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewTreeBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewScrollBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewMenuBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewEditorBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewPopupBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewBrowserBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewIFrameBoxObject(nsIBoxObject** aResult);
extern nsresult NS_NewOutlinerBoxObject(nsIBoxObject** aResult);
extern nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);
extern nsresult NS_CreateCSSFrameConstructor(nsICSSFrameConstructor** aResult);
extern nsresult NS_NewLayoutHistoryState(nsILayoutHistoryState** aResult);
extern nsresult NS_NewAutoCopyService(nsIAutoCopyService** aResult);

#define MAKE_CTOR(ctor_, iface_, func_)                   \
static NS_IMETHODIMP                                      \
ctor_(nsISupports* aOuter, REFNSIID aIID, void** aResult) \
{                                                         \
  *aResult = nsnull;                                      \
  if (aOuter)                                             \
    return NS_ERROR_NO_AGGREGATION;                       \
  iface_* inst;                                           \
  nsresult rv = func_(&inst);                             \
  if (NS_SUCCEEDED(rv)) {                                 \
    rv = inst->QueryInterface(aIID, aResult);             \
    NS_RELEASE(inst);                                     \
  }                                                       \
  return rv;                                              \
}
  
#ifdef DEBUG
MAKE_CTOR(CreateNewFrameUtil,           nsIFrameUtil,           NS_NewFrameUtil)
MAKE_CTOR(CreateNewLayoutDebugger,      nsILayoutDebugger,      NS_NewLayoutDebugger)
#endif

MAKE_CTOR(CreateNewPrintPreviewContext, nsIPresContext,         NS_NewPrintPreviewContext)
MAKE_CTOR(CreateNewCSSFrameConstructor, nsICSSFrameConstructor, NS_CreateCSSFrameConstructor)
MAKE_CTOR(CreateNewFrameTraversal,      nsIFrameTraversal,      NS_CreateFrameTraversal)
MAKE_CTOR(CreateNewLayoutHistoryState,  nsILayoutHistoryState,  NS_NewLayoutHistoryState)
MAKE_CTOR(CreateNewPresShell,           nsIPresShell,           NS_NewPresShell)
MAKE_CTOR(CreateNewPresState,           nsIPresState,           NS_NewPresState)
MAKE_CTOR(CreateNewGalleyContext,       nsIPresContext,         NS_NewGalleyContext)
MAKE_CTOR(CreateNewPrintContext,        nsIPrintContext,        NS_NewPrintContext)
MAKE_CTOR(CreateNewBoxObject,           nsIBoxObject,           NS_NewBoxObject)
MAKE_CTOR(CreateNewTreeBoxObject,       nsIBoxObject,           NS_NewTreeBoxObject)
MAKE_CTOR(CreateNewMenuBoxObject,       nsIBoxObject,           NS_NewMenuBoxObject)
MAKE_CTOR(CreateNewPopupBoxObject,      nsIBoxObject,           NS_NewPopupBoxObject)
MAKE_CTOR(CreateNewBrowserBoxObject,    nsIBoxObject,           NS_NewBrowserBoxObject)
MAKE_CTOR(CreateNewEditorBoxObject,     nsIBoxObject,           NS_NewEditorBoxObject)
MAKE_CTOR(CreateNewIFrameBoxObject,     nsIBoxObject,           NS_NewIFrameBoxObject)
MAKE_CTOR(CreateNewScrollBoxObject,     nsIBoxObject,           NS_NewScrollBoxObject)
MAKE_CTOR(CreateNewOutlinerBoxObject,   nsIBoxObject,           NS_NewOutlinerBoxObject)
MAKE_CTOR(CreateNewAutoCopyService,     nsIAutoCopyService,     NS_NewAutoCopyService)

// The list of components we register
static nsModuleComponentInfo gComponents[] = {
#ifdef DEBUG
  { "Frame utility",
    NS_FRAME_UTIL_CID,
    nsnull,
    CreateNewFrameUtil },

  { "Layout debugger",
    NS_LAYOUT_DEBUGGER_CID,
    nsnull,
    CreateNewLayoutDebugger },
#endif

  { "Print preview context",
    NS_PRINT_PREVIEW_CONTEXT_CID,
    nsnull,
    CreateNewPrintPreviewContext },

  { "CSS Frame Constructor",
    NS_CSSFRAMECONSTRUCTOR_CID,
    nsnull,
    CreateNewCSSFrameConstructor },

  { "Frame Traversal",
    NS_FRAMETRAVERSAL_CID,
    nsnull,
    CreateNewFrameTraversal },

  { "Layout History State",
    NS_LAYOUT_HISTORY_STATE_CID,
    nsnull,
    CreateNewLayoutHistoryState },

  // XXX ick
  { "Presentation shell",
    NS_PRESSHELL_CID,
    nsnull,
    CreateNewPresShell },

  { "Presentation state",
    NS_PRESSTATE_CID,
    nsnull,
    CreateNewPresState },

  { "Galley context",
    NS_GALLEYCONTEXT_CID,
    nsnull,
    CreateNewGalleyContext },

  { "Print context",
    NS_PRINTCONTEXT_CID,
    nsnull,
    CreateNewPrintContext },
  // XXX end ick

  { "XUL Box Object",
    NS_BOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject;1",
    CreateNewBoxObject },

  { "XUL Tree Box Object",
    NS_TREEBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-tree;1",
    CreateNewTreeBoxObject },

  { "XUL Menu Box Object",
    NS_MENUBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-menu;1",
    CreateNewMenuBoxObject },

  { "XUL Popup Box Object",
    NS_POPUPBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-popup;1",
    CreateNewPopupBoxObject },

  { "XUL Browser Box Object",
    NS_BROWSERBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-browser;1",
    CreateNewBrowserBoxObject },

  { "XUL Editor Box Object",
    NS_EDITORBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-editor;1",
    CreateNewEditorBoxObject },

  { "XUL Iframe Object",
    NS_IFRAMEBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-iframe;1",
    CreateNewIFrameBoxObject },

  { "XUL ScrollBox Object",
    NS_SCROLLBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-scrollbox;1",
    CreateNewScrollBoxObject },

  { "XUL Outliner Box Object",
    NS_OUTLINERBOXOBJECT_CID,
    "@mozilla.org/layout/xul-boxobject-outliner;1",
    CreateNewOutlinerBoxObject  },

  { "AutoCopy Service",
    NS_AUTOCOPYSERVICE_CID,
    "@mozilla.org/autocopy;1",
    CreateNewAutoCopyService }
};

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsLayoutModule, gComponents, Initialize, Shutdown)
