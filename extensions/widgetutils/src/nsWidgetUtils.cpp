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
 *   Ms2ger <ms2ger@gmail.com>
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
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMNSElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDocument.h"
#include "nsIGenericFactory.h"
#include "nsIObserver.h"
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
#include "nsIDOMCompositionListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIView.h"
#include "nsGUIEvent.h"
#include "nsIViewManager.h"
#include "nsIContentPolicy.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContent.h"
#include "nsITimer.h"

const int MIN_INT =((int) (1 << (sizeof(int) * 8 - 1)));

static int g_lastX=MIN_INT;
static int g_lastY=MIN_INT;
static PRInt32 g_panning = 0;
static bool g_is_scrollable = false;

#define EM_MULT 16.
#define NS_FRAME_HAS_RELATIVE_SIZE 0x01000000
#define NS_FRAME_HAS_OPTIMIZEDVIEW 0x02000000
#define BEHAVIOR_ACCEPT nsIPermissionManager::ALLOW_ACTION
#define BEHAVIOR_REJECT nsIPermissionManager::DENY_ACTION
#define BEHAVIOR_NOFOREIGN 3
#define NUMBER_OF_TYPES 13

// TODO auto reload nsWidgetUtils in C.
class nsWidgetUtils : public nsIObserver,
                      public nsIDOMEventListener,
                      public nsIContentPolicy,
                      public nsSupportsWeakReference
{
public:
  nsWidgetUtils();
  virtual ~nsWidgetUtils();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER 
  NS_DECL_NSICONTENTPOLICY

private:
  nsresult Init(void);
  void RemoveWindowListeners(nsIDOMWindow *aDOMWin);
  void GetChromeEventHandler(nsIDOMWindow *aDOMWin, nsIDOMEventTarget **aChromeTarget);
  void AttachWindowListeners(nsIDOMWindow *aDOMWin);
  bool IsXULNode(nsIDOMNode *aNode, PRUint32 *aType = 0);
  nsresult GetDOMWindowByNode(nsIDOMNode *aNode, nsIDOMWindow * *aDOMWindow);
  nsresult UpdateFromEvent(nsIDOMEvent *aDOMEvent);
  nsresult MouseDown(nsIDOMEvent* aDOMEvent);
  nsresult MouseUp(nsIDOMEvent* aDOMEvent);
  nsresult MouseMove(nsIDOMEvent* aDOMEvent);

  static void StopPanningCallback(nsITimer *timer, void *closure);

  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsITimer> mTimer;
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
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
}

nsresult
nsWidgetUtils::UpdateFromEvent(nsIDOMEvent *aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;

  ((nsIDOMMouseEvent*)mouseEvent)->GetScreenX(&g_lastX);
  ((nsIDOMMouseEvent*)mouseEvent)->GetScreenY(&g_lastY);

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIDOMNode> mNode;
  nsCOMPtr<nsIDOMNode> mOrigNode;

  PRUint32 type = 0;
  bool isXul = false;
  {
    nsCOMPtr <nsIDOMNSEvent> aEvent = do_QueryInterface(aDOMEvent);
    nsCOMPtr<nsIDOMEventTarget> eventOrigTarget;
    if (aEvent)
      aEvent->GetOriginalTarget(getter_AddRefs(eventOrigTarget));
    if (eventOrigTarget)
      mOrigNode = do_QueryInterface(eventOrigTarget);
    isXul = IsXULNode(mOrigNode, &type);

  }
  if (isXul)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  aDOMEvent->GetTarget(getter_AddRefs(eventTarget));
  if (eventTarget)
    mNode = do_QueryInterface(eventTarget);

  if (!mNode)
    return NS_OK;

  GetDOMWindowByNode(mNode, getter_AddRefs(mWindow));
  if (!mWindow)
    return NS_OK;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDOMDocument> domDoc;
  mWindow->GetDocument(getter_AddRefs(domDoc));
  doc = do_QueryInterface(domDoc);
  if (!doc) return NS_OK;
  // the only case where there could be more shells in printpreview
  nsIPresShell *shell = doc->GetShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);
  mViewManager = shell->GetViewManager();
  NS_ENSURE_TRUE(mViewManager, NS_ERROR_FAILURE);
  mViewManager->GetRootWidget(getter_AddRefs(mWidget));
  NS_ENSURE_TRUE(mWidget, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
nsWidgetUtils::MouseDown(nsIDOMEvent* aDOMEvent)
{
  g_is_scrollable = PR_FALSE;
  // Return TRUE from your signal handler to mark the event as consumed.
  if (NS_FAILED(UpdateFromEvent(aDOMEvent)))
    return NS_OK;
  g_is_scrollable = PR_TRUE;
  if (g_is_scrollable) {
     aDOMEvent->StopPropagation();
     aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

/* static */ void
nsWidgetUtils::StopPanningCallback(nsITimer *timer, void *closure)
{
  g_panning = PR_FALSE;
}

nsresult
nsWidgetUtils::MouseUp(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  g_lastX = MIN_INT;
  g_lastY = MIN_INT;
  g_is_scrollable = PR_FALSE;
  if (g_panning) {
     aDOMEvent->StopPropagation();
     aDOMEvent->PreventDefault();
     nsresult rv;
     if (mTimer) {
       rv = mTimer->InitWithFuncCallback(nsWidgetUtils::StopPanningCallback,
                                        nsnull, 500, nsITimer::TYPE_ONE_SHOT);
       if (NS_SUCCEEDED(rv))
         return NS_OK;
     }
     g_panning = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsWidgetUtils::MouseMove(nsIDOMEvent* aDOMEvent)
{
  if (!g_is_scrollable) return NS_OK;

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

  nsIView* aView = mViewManager->GetRootView();
  if (!aView)
    if (NS_FAILED(UpdateFromEvent(aDOMEvent)))
      return NS_OK;

  nsEventStatus statusX;
  nsMouseScrollEvent scrollEventX(PR_TRUE, NS_MOUSE_SCROLL, mWidget);
  scrollEventX.delta = dx;
  scrollEventX.scrollFlags = nsMouseScrollEvent::kIsHorizontal | nsMouseScrollEvent::kHasPixels;
  mViewManager->DispatchEvent(&scrollEventX, aView, &statusX);
  if(statusX != nsEventStatus_eIgnore ){
    if (dx > 5)
      g_panning = PR_TRUE;
    g_lastX = x;
  }

  nsEventStatus statusY;
  nsMouseScrollEvent scrollEventY(PR_TRUE, NS_MOUSE_SCROLL, mWidget);
  scrollEventY.delta = dy;
  scrollEventY.scrollFlags = nsMouseScrollEvent::kIsVertical | nsMouseScrollEvent::kHasPixels;
  mViewManager->DispatchEvent(&scrollEventY, aView, &statusY);
  if(statusY != nsEventStatus_eIgnore ){
    if (dy > 5)
      g_panning = PR_TRUE;
    g_lastY = y;
  }
  if (g_panning) {
     aDOMEvent->StopPropagation();
     aDOMEvent->PreventDefault();
  }

  return NS_OK;
}

// nsIContentPolicy Implementation
NS_IMETHODIMP
nsWidgetUtils::ShouldLoad(PRUint32          aContentType,
                          nsIURI           *aContentLocation,
                          nsIURI           *aRequestingLocation,
                          nsISupports      *aRequestingContext,
                          const nsACString &aMimeGuess,
                          nsISupports      *aExtra,
                          PRInt16          *aDecision)
{
    *aDecision = nsIContentPolicy::ACCEPT;
    nsresult rv;

    if (aContentType != nsIContentPolicy::TYPE_DOCUMENT)
        return NS_OK;

    // we can't do anything without this
    if (!aContentLocation)
        return NS_OK;

    nsCAutoString scheme;
    rv = aContentLocation->GetScheme(scheme);
    nsCAutoString lscheme;
    ToLowerCase(scheme, lscheme);
    if (!lscheme.EqualsLiteral("ftp") &&
        !lscheme.EqualsLiteral("http") &&
        !lscheme.EqualsLiteral("https"))
        return NS_OK;
    if (g_panning > 0)
      *aDecision = nsIContentPolicy::REJECT_REQUEST;
    return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.EqualsLiteral("mousedown")) {
    return MouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("mouseup")) {
    return MouseUp(aEvent);
  }
  if (eventType.EqualsLiteral("mousemove")) {
    return MouseMove(aEvent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWidgetUtils::ShouldProcess(PRUint32          aContentType,
                             nsIURI           *aContentLocation,
                             nsIURI           *aRequestingLocation,
                             nsISupports      *aRequestingContext,
                             const nsACString &aMimeGuess,
                             nsISupports      *aExtra,
                             PRInt16          *aDecision)
{
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
}

bool
nsWidgetUtils::IsXULNode(nsIDOMNode *aNode, PRUint32 *aType)
{
  bool retval = false;
  if (!aNode) return retval;

  nsString sorigNode;
  aNode->GetNodeName(sorigNode);
  if (sorigNode.EqualsLiteral("#document"))
    return retval;
  retval = StringBeginsWith(sorigNode, NS_LITERAL_STRING("xul:"));

  if (!aType) return retval;

  if (sorigNode.EqualsLiteral("xul:thumb")
      || sorigNode.EqualsLiteral("xul:vbox")
      || sorigNode.EqualsLiteral("xul:spacer"))
    *aType = PR_FALSE; // Magic
  else if (sorigNode.EqualsLiteral("xul:slider"))
    *aType = 2; // Magic
  else if (sorigNode.EqualsLiteral("xul:scrollbarbutton"))
    *aType = 3; // Magic

  return retval;
}

nsresult
nsWidgetUtils::GetDOMWindowByNode(nsIDOMNode* aNode, nsIDOMWindow** aDOMWindow)
{
  nsCOMPtr<nsIDOMDocument> nodeDoc;
  nsresult rv = aNode->GetOwnerDocument(getter_AddRefs(nodeDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(nodeDoc, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMWindow> window;
  rv = nodeDoc->GetDefaultView(getter_AddRefs(window));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);
  window.forget(aDOMWindow);
  return rv;
}

void
nsWidgetUtils::GetChromeEventHandler(nsIDOMWindow *aDOMWin,
                                     nsIDOMEventTarget **aChromeTarget)
{
    nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(aDOMWin));
    nsIDOMEventTarget* chromeEventHandler = nsnull;
    if (privateDOMWindow) {
        chromeEventHandler = privateDOMWindow->GetChromeEventHandler();
    }

    NS_IF_ADDREF(*aChromeTarget = chromeEventHandler);
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

    // Remove DOM Text listener for IME text events
    chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("mousedown"),
                                            this, PR_FALSE);
    chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("mouseup"),
                                            this, PR_FALSE);
    chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                            this, PR_FALSE);
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

    // Attach menu listeners, this will help us ignore keystrokes meant for menus
    chromeEventHandler->AddEventListener(NS_LITERAL_STRING("mousedown"), this,
                                         PR_FALSE, PR_FALSE);
    chromeEventHandler->AddEventListener(NS_LITERAL_STRING("mouseup"), this,
                                         PR_FALSE, PR_FALSE);
    chromeEventHandler->AddEventListener(NS_LITERAL_STRING("mousemove"), this,
                                         PR_FALSE, PR_FALSE);
}

nsWidgetUtils::~nsWidgetUtils()
{
}

NS_IMPL_ISUPPORTS4(nsWidgetUtils,
                   nsIObserver,
                   nsIDOMEventListener,
                   nsIContentPolicy,
                   nsISupportsWeakReference)

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
    rv = catman->AddCategoryEntry("content-policy",
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
    rv = catman->DeleteCategoryEntry("content-policy",
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
