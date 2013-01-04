/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCURILoader.h"
#include "nsICategoryManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLIFrameElement.h"
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
#include "nsIDOMWheelEvent.h"
#include "nsView.h"
#include "nsGUIEvent.h"
#include "nsIViewManager.h"
#include "nsIContentPolicy.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContent.h"
#include "nsITimer.h"

using namespace mozilla;

const int MIN_INT =((int) (1 << (sizeof(int) * 8 - 1)));

static int g_lastX=MIN_INT;
static int g_lastY=MIN_INT;
static int32_t g_panning = 0;
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
  bool IsXULNode(nsIDOMNode *aNode, uint32_t *aType = 0);
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

  rv = obsSvc->AddObserver(this, "domwindowopened", false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "domwindowclosed", false);
  NS_ENSURE_SUCCESS(rv, rv);
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
}

nsresult
nsWidgetUtils::UpdateFromEvent(nsIDOMEvent *aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;

  mouseEvent->GetScreenX(&g_lastX);
  mouseEvent->GetScreenY(&g_lastY);

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIDOMNode> mNode;
  nsCOMPtr<nsIDOMNode> mOrigNode;

  uint32_t type = 0;
  bool isXul = false;
  {
    nsCOMPtr<nsIDOMEventTarget> eventOrigTarget;
    aDOMEvent->GetOriginalTarget(getter_AddRefs(eventOrigTarget));
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
  g_is_scrollable = false;
  // Return TRUE from your signal handler to mark the event as consumed.
  if (NS_FAILED(UpdateFromEvent(aDOMEvent)))
    return NS_OK;
  g_is_scrollable = true;
  if (g_is_scrollable) {
     aDOMEvent->StopPropagation();
     aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

/* static */ void
nsWidgetUtils::StopPanningCallback(nsITimer *timer, void *closure)
{
  g_panning = false;
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
  g_is_scrollable = false;
  if (g_panning) {
     aDOMEvent->StopPropagation();
     aDOMEvent->PreventDefault();
     nsresult rv;
     if (mTimer) {
       rv = mTimer->InitWithFuncCallback(nsWidgetUtils::StopPanningCallback,
                                        nullptr, 500, nsITimer::TYPE_ONE_SHOT);
       if (NS_SUCCEEDED(rv))
         return NS_OK;
     }
     g_panning = false;
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

  nsView* aView = mViewManager->GetRootView();
  if (!aView)
    if (NS_FAILED(UpdateFromEvent(aDOMEvent)))
      return NS_OK;

  nsEventStatus status;
  widget::WheelEvent wheelEvent(true, NS_WHEEL_WHEEL, mWidget);
  wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;
  wheelEvent.deltaX = wheelEvent.lineOrPageDeltaX = dx;
  wheelEvent.deltaY = wheelEvent.lineOrPageDeltaY = dy;
  mViewManager->DispatchEvent(&wheelEvent, aView, &status);
  if (status != nsEventStatus_eIgnore) {
    if (dx > 5 || dy > 5) {
      g_panning = true;
    }
    g_lastX = x;
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
nsWidgetUtils::ShouldLoad(uint32_t          aContentType,
                          nsIURI           *aContentLocation,
                          nsIURI           *aRequestingLocation,
                          nsISupports      *aRequestingContext,
                          const nsACString &aMimeGuess,
                          nsISupports      *aExtra,
                          int16_t          *aDecision)
{
    *aDecision = nsIContentPolicy::ACCEPT;
    nsresult rv;

    if (aContentType != nsIContentPolicy::TYPE_DOCUMENT)
        return NS_OK;

    // we can't do anything without this
    if (!aContentLocation)
        return NS_OK;

    nsAutoCString scheme;
    rv = aContentLocation->GetScheme(scheme);
    nsAutoCString lscheme;
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
nsWidgetUtils::ShouldProcess(uint32_t          aContentType,
                             nsIURI           *aContentLocation,
                             nsIURI           *aRequestingLocation,
                             nsISupports      *aRequestingContext,
                             const nsACString &aMimeGuess,
                             nsISupports      *aExtra,
                             int16_t          *aDecision)
{
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
}

bool
nsWidgetUtils::IsXULNode(nsIDOMNode *aNode, uint32_t *aType)
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
    *aType = false; // Magic
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
    nsIDOMEventTarget* chromeEventHandler = nullptr;
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
                                            this, false);
    chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("mouseup"),
                                            this, false);
    chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                            this, false);
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
                                         false, false);
    chromeEventHandler->AddEventListener(NS_LITERAL_STRING("mouseup"), this,
                                         false, false);
    chromeEventHandler->AddEventListener(NS_LITERAL_STRING("mousemove"), this,
                                         false, false);
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

    char* previous = nullptr;
    rv = catman->AddCategoryEntry("app-startup",
                                  "WidgetUtils",
                                  WidgetUtils_ContractID,
                                  true,
                                  true,
                                  &previous);
    if (previous)
        nsMemory::Free(previous);
    rv = catman->AddCategoryEntry("content-policy",
                                  "WidgetUtils",
                                  WidgetUtils_ContractID,
                                  true,
                                  true,
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
                                     true);
    rv = catman->DeleteCategoryEntry("content-policy",
                                     "WidgetUtils",
                                     true);

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
