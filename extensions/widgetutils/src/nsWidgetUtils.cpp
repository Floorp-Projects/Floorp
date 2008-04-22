/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla's Element Optimizeing extension.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com> (original author)
 *   Brad Lassey <blassey@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCURILoader.h"
#include "nsICategoryManager.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMNSElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDocument.h"
#include "nsIGenericFactory.h"
#include "nsIObserver.h"
#include "nsIPref.h"
#include "nsIPresShell.h"
#include "nsIStyleSheetService.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIWindowWatcher.h"
#include "nsNetUtil.h"
#include "nsRect.h"
#include "nsStringGlue.h"
#include "nsWeakReference.h"
#include "nsIWebBrowser.h"
#include "nsIObserverService.h"
#include "nsIDOMEventTarget.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindow.h"
//#include ".h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMCompositionListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIView.h"
#include "nsGUIEvent.h"
#include "nsIViewManager.h"

const int MIN_INT =((int) (1 << (sizeof(int) * 8 - 1)));

static int g_lastX=MIN_INT;
static int g_lastY=MIN_INT;

#define EM_MULT 16.
#define NS_FRAME_HAS_RELATIVE_SIZE 0x01000000
#define NS_FRAME_HAS_OPTIMIZEDVIEW 0x02000000

// TODO auto reload nsWidgetUtils in C.
class nsWidgetUtils : public nsIObserver,
                      public nsIDOMMouseMotionListener,
                      public nsIDOMMouseListener,
                      public nsSupportsWeakReference
{
public:
  nsWidgetUtils();
  virtual ~nsWidgetUtils();
  nsresult Init(void);
  nsresult GetDOMWindowByNode(nsIDOMNode *aNode, nsIDOMWindow * *aDOMWindow);

  void RemoveWindowListeners(nsIDOMWindow *aDOMWin);
  void GetChromeEventHandler(nsIDOMWindow *aDOMWin, nsIDOMEventTarget **aChromeTarget);
  void AttachWindowListeners(nsIDOMWindow *aDOMWin);

  // nsIDOMMouseMotionListener
  NS_IMETHOD MouseMove(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD HandleEvent(nsIDOMEvent* aDOMEvent);

  // nsIDOMMouseListener
  NS_IMETHOD MouseDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aDOMEvent);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

nsWidgetUtils::nsWidgetUtils()
{
  Init();
}

NS_IMETHODIMP
nsWidgetUtils::Init()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  rv = obsSvc->AddObserver(this, "domwindowopened", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "domwindowclosed", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
}

NS_IMETHODIMP
nsWidgetUtils::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::MouseDown(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;

  ((nsIDOMMouseEvent*)mouseEvent)->GetScreenX(&g_lastX);
  ((nsIDOMMouseEvent*)mouseEvent)->GetScreenY(&g_lastY);
  // Return TRUE from your signal handler to mark the event as consumed.
  // PRBool return_val = PR_FALSE;
  // if (return_val) {
  //   aDOMEvent->StopPropagation();
  //   aDOMEvent->PreventDefault();
  // }
  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::MouseUp(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  g_lastX = MIN_INT;
  g_lastY = MIN_INT;

  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::MouseClick(nsIDOMEvent* aDOMEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::MouseDblClick(nsIDOMEvent* aDOMEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::MouseOver(nsIDOMEvent* aDOMEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::MouseOut(nsIDOMEvent* aDOMEvent)
{
  return NS_OK;
}

nsresult
nsWidgetUtils::GetDOMWindowByNode(nsIDOMNode *aNode, nsIDOMWindow * *aDOMWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDOMDocument> nodeDoc;
  rv = aNode->GetOwnerDocument(getter_AddRefs(nodeDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface(nodeDoc, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMAbstractView> absView;
  NS_ENSURE_SUCCESS(rv, rv);
  rv = docView->GetDefaultView(getter_AddRefs(absView));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(absView, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  *aDOMWindow = window;
  NS_IF_ADDREF(*aDOMWindow);
  return rv;
}

NS_IMETHODIMP
nsWidgetUtils::MouseMove(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  int x, y;
  ((nsIDOMMouseEvent*)mouseEvent)->GetScreenX(&x);
  ((nsIDOMMouseEvent*)mouseEvent)->GetScreenY(&y);

  int dx = g_lastX - x;
  int dy = g_lastY - y;
  if(g_lastX == MIN_INT || g_lastY == MIN_INT)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> eventNode;
  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  aDOMEvent->GetTarget(getter_AddRefs(eventTarget));
  if (!eventTarget)
    return NS_OK;
  eventNode = do_QueryInterface(eventTarget);
  if (!eventNode)
    return NS_OK;
  nsCOMPtr<nsIDOMWindow> domWindow;
  GetDOMWindowByNode(eventNode, getter_AddRefs(domWindow));
  if (!domWindow)
    return NS_OK;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  doc = do_QueryInterface(domDoc);
  if (!doc) return NS_OK;
  // the only case where there could be more shells in printpreview
  nsIPresShell *shell = doc->GetPrimaryShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);
  nsIViewManager* viewManager = shell->GetViewManager();
  NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);
  nsIView* rootView = nsnull;
  viewManager->GetRootView(rootView);
  NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);
  nsEventStatus statusX;
  nsMouseScrollEvent scrollEventX(PR_TRUE, NS_MOUSE_SCROLL, rootView->GetWidget());
  scrollEventX.delta = dx;
  scrollEventX.scrollFlags = nsMouseScrollEvent::kIsHorizontal | nsMouseScrollEvent::kIsPixels;
  viewManager->DispatchEvent(&scrollEventX, &statusX);
  if(statusX != nsEventStatus_eIgnore ){
    g_lastX = x;
  }

  nsEventStatus statusY;
  nsMouseScrollEvent scrollEventY(PR_TRUE, NS_MOUSE_SCROLL, rootView->GetWidget());
  scrollEventY.delta = dy;
  scrollEventY.scrollFlags = nsMouseScrollEvent::kIsVertical | nsMouseScrollEvent::kIsPixels;
  viewManager->DispatchEvent(&scrollEventY, &statusY);
  if(statusY != nsEventStatus_eIgnore ){
    g_lastY = y;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::DragMove(nsIDOMEvent* aDOMEvent)
{
  return NS_OK;
}

void
nsWidgetUtils::GetChromeEventHandler(nsIDOMWindow *aDOMWin,
                                 nsIDOMEventTarget **aChromeTarget)
{
    nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(aDOMWin));
    nsPIDOMEventTarget* chromeEventHandler = nsnull;
    if (privateDOMWindow) {
        chromeEventHandler = privateDOMWindow->GetChromeEventHandler();
    }

    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

    *aChromeTarget = target;
    NS_IF_ADDREF(*aChromeTarget);
}

void
nsWidgetUtils::RemoveWindowListeners(nsIDOMWindow *aDOMWin)
{
    nsresult rv;
    nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
    GetChromeEventHandler(aDOMWin, getter_AddRefs(chromeEventHandler));
    if (!chromeEventHandler) {
        return;
    }

    // Use capturing, otherwise the normal find next will get activated when ours should
    nsCOMPtr<nsPIDOMEventTarget> piTarget(do_QueryInterface(chromeEventHandler));

    // Remove DOM Text listener for IME text events
    rv = piTarget->RemoveEventListenerByIID(static_cast<nsIDOMMouseListener*>(this),
                                            NS_GET_IID(nsIDOMMouseListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add Mouse Motion listener\n");
        return;
    }
    rv = piTarget->RemoveEventListenerByIID(static_cast<nsIDOMMouseMotionListener*>(this),
                                            NS_GET_IID(nsIDOMMouseMotionListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add Mouse Motion listener\n");
        return;
    }
}

void
nsWidgetUtils::AttachWindowListeners(nsIDOMWindow *aDOMWin)
{
    nsresult rv;
    nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
    GetChromeEventHandler(aDOMWin, getter_AddRefs(chromeEventHandler));
    if (!chromeEventHandler) {
        return;
    }

    // Use capturing, otherwise the normal find next will get activated when ours should
    nsCOMPtr<nsPIDOMEventTarget> piTarget(do_QueryInterface(chromeEventHandler));

    // Attach menu listeners, this will help us ignore keystrokes meant for menus
    rv = piTarget->AddEventListenerByIID(static_cast<nsIDOMMouseListener*>(this),
                                         NS_GET_IID(nsIDOMMouseListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add Mouse Motion listener\n");
        return;
    }
    rv = piTarget->AddEventListenerByIID(static_cast<nsIDOMMouseMotionListener*>(this),
                                         NS_GET_IID(nsIDOMMouseMotionListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add Mouse Motion listener\n");
        return;
    }
}

nsWidgetUtils::~nsWidgetUtils()
{
}

NS_IMPL_ISUPPORTS4(nsWidgetUtils, nsIObserver, nsIDOMMouseMotionListener, nsIDOMMouseListener, nsISupportsWeakReference)

NS_IMETHODIMP
nsWidgetUtils::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsresult rv;
  if (!strcmp(aTopic,"domwindowopened")) 
  {
    nsCOMPtr<nsIDOMWindow> chromeWindow = do_QueryInterface(aSubject);
    if (chromeWindow)
      AttachWindowListeners(chromeWindow);
    return NS_OK;
  }

  if (!strcmp(aTopic,"domwindowclosed")) 
  {
    nsCOMPtr<nsIDOMWindow> chromeWindow = do_QueryInterface(aSubject);
    RemoveWindowListeners(chromeWindow);
    return NS_OK;
  }

  return NS_OK;
}

//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------

#define WidgetUtils_CID \
{  0x0ced17b6, 0x96ed, 0x4030, \
{0xa1, 0x34, 0x77, 0xcb, 0x66, 0x10, 0xa8, 0xf6} }

#define WidgetUtils_ContractID "@mozilla.org/extensions/widgetutils;1"

static NS_METHOD WidgetUtilsRegistration(nsIComponentManager *aCompMgr,
                                         nsIFile *aPath,
                                         const char *registryLocation,
                                         const char *componentType,
                                         const nsModuleComponentInfo *info)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servman = do_QueryInterface((nsISupports*)aCompMgr, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsICategoryManager> catman;
    servman->GetServiceByContractID(NS_CATEGORYMANAGER_CONTRACTID,
                                    NS_GET_IID(nsICategoryManager),
                                    getter_AddRefs(catman));

    if (NS_FAILED(rv))
        return rv;

    char* previous = nsnull;
    rv = catman->AddCategoryEntry("app-startup",
                                  "WidgetUtils",
                                  WidgetUtils_ContractID,
                                  PR_TRUE,
                                  PR_TRUE,
                                  &previous);
    if (previous)
        nsMemory::Free(previous);

    return rv;
}

static NS_METHOD WidgetUtilsUnregistration(nsIComponentManager *aCompMgr,
                                           nsIFile *aPath,
                                           const char *registryLocation,
                                           const nsModuleComponentInfo *info)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servman = do_QueryInterface((nsISupports*)aCompMgr, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsICategoryManager> catman;
    servman->GetServiceByContractID(NS_CATEGORYMANAGER_CONTRACTID,
                                    NS_GET_IID(nsICategoryManager),
                                    getter_AddRefs(catman));

    if (NS_FAILED(rv))
        return rv;

    rv = catman->DeleteCategoryEntry("app-startup",
                                     "WidgetUtils",
                                     PR_TRUE);

    return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWidgetUtils)

  static const nsModuleComponentInfo components[] =
{
  { "nsWidgetUtilsService",
    WidgetUtils_CID,
    WidgetUtils_ContractID,
    nsWidgetUtilsConstructor,
    WidgetUtilsRegistration,
    WidgetUtilsUnregistration
  }
};

NS_IMPL_NSGETMODULE(nsWidgetUtilsModule, components)
