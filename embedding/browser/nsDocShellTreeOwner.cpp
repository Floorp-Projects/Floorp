/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsDocShellTreeOwner.h"
#include "nsWebBrowser.h"

// Helper Classes
#include "nsStyleCoord.h"
#include "nsSize.h"
#include "nsHTMLReflowState.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsIAtom.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/LookAndFeel.h"

// Interfaces needed to be included
#include "nsPresContext.h"
#include "nsIContextMenuListener.h"
#include "nsIContextMenuListener2.h"
#include "nsITooltipListener.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"
#include "Link.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGTitleElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIImageLoadingContent.h"
#include "nsIWebNavigation.h"
#include "nsIDOMHTMLElement.h"
#include "nsIPresShell.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIDOMWindowCollection.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsRect.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIContent.h"
#include "imgIContainer.h"
#include "nsContextMenuInfo.h"
#include "nsPresContext.h"
#include "nsViewManager.h"
#include "nsView.h"
#include "nsIDOMDragEvent.h"
#include "nsIConstraintValidation.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()

using namespace mozilla;
using namespace mozilla::dom;

//
// GetEventReceiver
//
// A helper routine that navigates the tricky path from a |nsWebBrowser| to
// a |EventTarget| via the window root and chrome event handler.
//
static nsresult
GetDOMEventTarget(nsWebBrowser* inBrowser, EventTarget** aTarget)
{
  NS_ENSURE_ARG_POINTER(inBrowser);

  nsCOMPtr<nsIDOMWindow> domWindow;
  inBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
  NS_ENSURE_TRUE(domWindowPrivate, NS_ERROR_FAILURE);
  nsPIDOMWindow *rootWindow = domWindowPrivate->GetPrivateRoot();
  NS_ENSURE_TRUE(rootWindow, NS_ERROR_FAILURE);
  nsCOMPtr<EventTarget> target =
    rootWindow->GetChromeEventHandler();
  NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);
  target.forget(aTarget);

  return NS_OK;
}


//*****************************************************************************
//***    nsDocShellTreeOwner: Object Management
//*****************************************************************************

nsDocShellTreeOwner::nsDocShellTreeOwner() :
   mWebBrowser(nullptr), 
   mTreeOwner(nullptr),
   mPrimaryContentShell(nullptr),
   mWebBrowserChrome(nullptr),
   mOwnerWin(nullptr),
   mOwnerRequestor(nullptr),
   mChromeTooltipListener(nullptr),
   mChromeContextMenuListener(nullptr)
{
}

nsDocShellTreeOwner::~nsDocShellTreeOwner()
{
  RemoveChromeListeners();
}

//*****************************************************************************
// nsDocShellTreeOwner::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDocShellTreeOwner)
NS_IMPL_RELEASE(nsDocShellTreeOwner)

NS_INTERFACE_MAP_BEGIN(nsDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
    NS_INTERFACE_MAP_ENTRY(nsICDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDocShellTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP
nsDocShellTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);

  if(NS_SUCCEEDED(QueryInterface(aIID, aSink)))
    return NS_OK;

  if (aIID.Equals(NS_GET_IID(nsIWebBrowserChromeFocus))) {
    if (mWebBrowserChromeWeak != nullptr)
      return mWebBrowserChromeWeak->QueryReferent(aIID, aSink);
    return mOwnerWin->QueryInterface(aIID, aSink);
  }

  if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
    nsIPrompt *prompt;
    EnsurePrompter();
    prompt = mPrompter;
    if (prompt) {
      NS_ADDREF(prompt);
      *aSink = prompt;
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    nsIAuthPrompt *prompt;
    EnsureAuthPrompter();
    prompt = mAuthPrompter;
    if (prompt) {
      NS_ADDREF(prompt);
      *aSink = prompt;
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  nsCOMPtr<nsIInterfaceRequestor> req = GetOwnerRequestor();
  if (req)
    return req->GetInterface(aIID, aSink);

  return NS_NOINTERFACE;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP
nsDocShellTreeOwner::FindItemWithName(const char16_t* aName,
                                      nsIDocShellTreeItem* aRequestor,
                                      nsIDocShellTreeItem* aOriginalRequestor,
                                      nsIDocShellTreeItem** aFoundItem)
{
  NS_ENSURE_ARG(aName);
  NS_ENSURE_ARG_POINTER(aFoundItem);
  *aFoundItem = nullptr; // if we don't find one, we return NS_OK and a null result 
  nsresult rv;

  nsAutoString name(aName);

  if (!mWebBrowser)
    return NS_OK; // stymied

  /* special cases */
  if(name.IsEmpty())
    return NS_OK;
  if(name.LowerCaseEqualsLiteral("_blank"))
    return NS_OK;
  // _main is an IE target which should be case-insensitive but isn't
  // see bug 217886 for details
  // XXXbz what if our browser isn't targetable?  We need to handle that somehow.
  if(name.LowerCaseEqualsLiteral("_content") || name.EqualsLiteral("_main")) {
    *aFoundItem = mWebBrowser->mDocShell;
    NS_IF_ADDREF(*aFoundItem);
    return NS_OK;
  }

  if (!SameCOMIdentity(aRequestor, mWebBrowser->mDocShell)) {
    // This isn't a request coming up from our kid, so check with said kid
    nsISupports* thisSupports = static_cast<nsIDocShellTreeOwner*>(this);
    rv = mWebBrowser->mDocShell->FindItemWithName(aName, thisSupports,
                                                  aOriginalRequestor, aFoundItem);
    if (NS_FAILED(rv) || *aFoundItem) {
      return rv;
    }
  }

  // next, if we have a parent and it isn't the requestor, ask it
  if(mTreeOwner) {
    nsCOMPtr<nsIDocShellTreeOwner> reqAsTreeOwner(do_QueryInterface(aRequestor));
    if (mTreeOwner != reqAsTreeOwner)
      return mTreeOwner->FindItemWithName(aName, mWebBrowser->mDocShell,
                                          aOriginalRequestor, aFoundItem);
    return NS_OK;
  }

  // finally, failing everything else, search all windows
  return FindItemWithNameAcrossWindows(aName, aRequestor, aOriginalRequestor,
                                       aFoundItem);
}

nsresult
nsDocShellTreeOwner::FindItemWithNameAcrossWindows(const char16_t* aName,
                                                   nsIDocShellTreeItem* aRequestor,
                                                   nsIDocShellTreeItem* aOriginalRequestor,
                                                   nsIDocShellTreeItem** aFoundItem)
{
  // search for the item across the list of top-level windows
  nsCOMPtr<nsPIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!wwatch)
    return NS_OK;

  return wwatch->FindItemWithName(aName, aRequestor, aOriginalRequestor,
                                  aFoundItem);
}

void
nsDocShellTreeOwner::EnsurePrompter()
{
  if (mPrompter)
    return;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (wwatch && mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow)
      wwatch->GetNewPrompter(domWindow, getter_AddRefs(mPrompter));
  }
}

void
nsDocShellTreeOwner::EnsureAuthPrompter()
{
  if (mAuthPrompter)
    return;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (wwatch && mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow)
      wwatch->GetNewAuthPrompter(domWindow, getter_AddRefs(mAuthPrompter));
  }
}

void
nsDocShellTreeOwner::AddToWatcher()
{
  if (mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsCOMPtr<nsPIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      if (wwatch) {
        nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome = GetWebBrowserChrome();
        if (webBrowserChrome)
          wwatch->AddWindow(domWindow, webBrowserChrome);
      }
    }
  }
}

void
nsDocShellTreeOwner::RemoveFromWatcher()
{
  if (mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsCOMPtr<nsPIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      if (wwatch)
        wwatch->RemoveWindow(domWindow);
    }
  }
}


NS_IMETHODIMP
nsDocShellTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                       bool aPrimary, bool aTargetable,
                                       const nsAString& aID)
{
   if(mTreeOwner)
      return mTreeOwner->ContentShellAdded(aContentShell, aPrimary,
                                           aTargetable, aID);

   if (aPrimary)
      mPrimaryContentShell = aContentShell;
   return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::ContentShellRemoved(nsIDocShellTreeItem* aContentShell)
{
  if(mTreeOwner)
    return mTreeOwner->ContentShellRemoved(aContentShell);

  if(mPrimaryContentShell == aContentShell)
    mPrimaryContentShell = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ENSURE_ARG_POINTER(aShell);

   if (mTreeOwner)
       return mTreeOwner->GetPrimaryContentShell(aShell);

   *aShell = (mPrimaryContentShell ? mPrimaryContentShell : mWebBrowser->mDocShell);
   NS_IF_ADDREF(*aShell);

   return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
                                 int32_t aCX, int32_t aCY)
{
   nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome = GetWebBrowserChrome();

   NS_ENSURE_STATE(mTreeOwner || webBrowserChrome);

   if(mTreeOwner)
      return mTreeOwner->SizeShellTo(aShellItem, aCX, aCY);

   if(aShellItem == mWebBrowser->mDocShell)
      return webBrowserChrome->SizeBrowserTo(aCX, aCY);

   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(aShellItem));
   NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMDocument> domDocument;
   webNav->GetDocument(getter_AddRefs(domDocument));
   NS_ENSURE_TRUE(domDocument, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMElement> domElement;
   domDocument->GetDocumentElement(getter_AddRefs(domElement));
   NS_ENSURE_TRUE(domElement, NS_ERROR_FAILURE);

   // Set the preferred Size
   //XXX
   NS_ERROR("Implement this");
   /*
   Set the preferred size on the aShellItem.
   */

   nsRefPtr<nsPresContext> presContext;
   mWebBrowser->mDocShell->GetPresContext(getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   nsIPresShell *presShell = presContext->GetPresShell();
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE,
      NS_UNCONSTRAINEDSIZE), NS_ERROR_FAILURE);
   
   nsRect shellArea = presContext->GetVisibleArea();

   int32_t browserCX = presContext->AppUnitsToDevPixels(shellArea.width);
   int32_t browserCY = presContext->AppUnitsToDevPixels(shellArea.height);

   return webBrowserChrome->SizeBrowserTo(browserCX, browserCY);
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetPersistence(bool aPersistPosition,
                                    bool aPersistSize,
                                    bool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPersistence(bool* aPersistPosition,
                                    bool* aPersistSize,
                                    bool* aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetTargetableShellCount(uint32_t* aResult)
{
  if(mTreeOwner) {
    mTreeOwner->GetTargetableShellCount(aResult);
  } else {
    *aResult = 0;
  }

  return NS_OK;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIBaseWindow
//*****************************************************************************   


NS_IMETHODIMP
nsDocShellTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
                                nsIWidget* aParentWidget, int32_t aX,
                                int32_t aY, int32_t aCX, int32_t aCY)   
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::Create()
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::Destroy()
{
  nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome = GetWebBrowserChrome();
  if (webBrowserChrome)
  {
    return webBrowserChrome->DestroyBrowserWindow();
  }

  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetUnscaledDevicePixelsPerCSSPixel(double *aScale)
{
  if (mWebBrowser) {
    return mWebBrowser->GetUnscaledDevicePixelsPerCSSPixel(aScale);
  }

  *aScale = 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetPosition(int32_t aX, int32_t aY)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                   aX, aY, 0, 0);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPosition(int32_t* aX, int32_t* aY)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                   aX, aY, nullptr, nullptr);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetSize(int32_t aCX, int32_t aCY, bool aRepaint)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER,
                                   0, 0, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetSize(int32_t* aCX, int32_t* aCY)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER,
                                   nullptr, nullptr, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetPositionAndSize(int32_t aX, int32_t aY, int32_t aCX,
                                        int32_t aCY, bool aRepaint)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER |
                                   nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                   aX, aY, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aCX,
                                        int32_t* aCY)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER |
                                   nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                   aX, aY, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::Repaint(bool aForce)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->GetSiteWindow(aParentNativeWindow);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetNativeHandle(nsAString& aNativeHandle)
{
  // the nativeHandle should be accessed from nsIXULWindow
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetVisibility(bool* aVisibility)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->GetVisibility(aVisibility);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetVisibility(bool aVisibility)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->SetVisibility(aVisibility);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetEnabled(bool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = true;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetEnabled(bool aEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetFocus()
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->SetFocus();
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetTitle(char16_t** aTitle)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->GetTitle(aTitle);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetTitle(const char16_t* aTitle)
{
  nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin = GetOwnerWin();
  if (ownerWin)
  {
    return ownerWin->SetTitle(aTitle);
  }
  return NS_ERROR_NULL_POINTER;
}


//*****************************************************************************
// nsDocShellTreeOwner::nsIWebProgressListener
//*****************************************************************************   


NS_IMETHODIMP
nsDocShellTreeOwner::OnProgressChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest,
                                      int32_t aCurSelfProgress,
                                      int32_t aMaxSelfProgress, 
                                      int32_t aCurTotalProgress,
                                      int32_t aMaxTotalProgress)
{
    // In the absence of DOM document creation event, this method is the
    // most convenient place to install the mouse listener on the
    // DOM document.
    return AddChromeListeners();
}

NS_IMETHODIMP
nsDocShellTreeOwner::OnStateChange(nsIWebProgress* aProgress,
                                   nsIRequest* aRequest,
                                   uint32_t aProgressStateFlags,
                                   nsresult aStatus)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::OnLocationChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      nsIURI* aURI,
                                      uint32_t aFlags)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsDocShellTreeOwner::OnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsresult aStatus,
                                    const char16_t* aMessage)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsDocShellTreeOwner::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                      nsIRequest *aRequest, 
                                      uint32_t state)
{
    return NS_OK;
}


//*****************************************************************************
// nsDocShellTreeOwner: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDocShellTreeOwner: Accessors
//*****************************************************************************   

void
nsDocShellTreeOwner::WebBrowser(nsWebBrowser* aWebBrowser)
{
  if ( !aWebBrowser )
    RemoveChromeListeners();
  if (aWebBrowser != mWebBrowser) {
    mPrompter = 0;
    mAuthPrompter = 0;
  }

  mWebBrowser = aWebBrowser;
}

nsWebBrowser *
nsDocShellTreeOwner::WebBrowser()
{
   return mWebBrowser;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner)
{ 
  if(aTreeOwner) {
    nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome(do_GetInterface(aTreeOwner));
    NS_ENSURE_TRUE(webBrowserChrome, NS_ERROR_INVALID_ARG);
    NS_ENSURE_SUCCESS(SetWebBrowserChrome(webBrowserChrome), NS_ERROR_INVALID_ARG);
    mTreeOwner = aTreeOwner;
  }
  else {
    mTreeOwner = nullptr;
    nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome = GetWebBrowserChrome();
    if (!webBrowserChrome)
      NS_ENSURE_SUCCESS(SetWebBrowserChrome(nullptr), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetWebBrowserChrome(nsIWebBrowserChrome* aWebBrowserChrome)
{
  if(!aWebBrowserChrome) {
    mWebBrowserChrome = nullptr;
    mOwnerWin = nullptr;
    mOwnerRequestor = nullptr;
    mWebBrowserChromeWeak = 0;
  } else {
    nsCOMPtr<nsISupportsWeakReference> supportsweak =
                                           do_QueryInterface(aWebBrowserChrome);
    if (supportsweak) {
      supportsweak->GetWeakReference(getter_AddRefs(mWebBrowserChromeWeak));
    } else {
      nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin(do_QueryInterface(aWebBrowserChrome));
      nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(aWebBrowserChrome));

      // it's ok for ownerWin or requestor to be null.
      mWebBrowserChrome = aWebBrowserChrome;
      mOwnerWin = ownerWin;
      mOwnerRequestor = requestor;
    }
  }
  return NS_OK;
}


//
// AddChromeListeners
//
// Hook up things to the chrome like context menus and tooltips, if the chrome
// has implemented the right interfaces.
//
NS_IMETHODIMP
nsDocShellTreeOwner::AddChromeListeners()
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome = GetWebBrowserChrome();
  if (!webBrowserChrome)
    return NS_ERROR_FAILURE;

  // install tooltips
  if ( !mChromeTooltipListener ) { 
    nsCOMPtr<nsITooltipListener>
                           tooltipListener(do_QueryInterface(webBrowserChrome));
    if ( tooltipListener ) {
      mChromeTooltipListener = new ChromeTooltipListener(mWebBrowser,
                                                         webBrowserChrome);
      if ( mChromeTooltipListener ) {
        NS_ADDREF(mChromeTooltipListener);
        rv = mChromeTooltipListener->AddChromeListeners();
      }
      else
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  
  // install context menus
  if ( !mChromeContextMenuListener ) {
    nsCOMPtr<nsIContextMenuListener2>
                          contextListener2(do_QueryInterface(webBrowserChrome));
    nsCOMPtr<nsIContextMenuListener>
                           contextListener(do_QueryInterface(webBrowserChrome));
    if ( contextListener2 || contextListener ) {
      mChromeContextMenuListener =
                   new ChromeContextMenuListener(mWebBrowser, webBrowserChrome);
      if ( mChromeContextMenuListener ) {
        NS_ADDREF(mChromeContextMenuListener);
        rv = mChromeContextMenuListener->AddChromeListeners();
      }
      else
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // register dragover and drop event listeners with the listener manager
  nsCOMPtr<EventTarget> target;
  GetDOMEventTarget(mWebBrowser, getter_AddRefs(target));

  EventListenerManager* elmP = target->GetOrCreateListenerManager();
  if (elmP) {
    elmP->AddEventListenerByType(this, NS_LITERAL_STRING("dragover"),
                                 TrustedEventsAtSystemGroupBubble());
    elmP->AddEventListenerByType(this, NS_LITERAL_STRING("drop"),
                                 TrustedEventsAtSystemGroupBubble());
  }

  return rv;

} // AddChromeListeners


NS_IMETHODIMP
nsDocShellTreeOwner::RemoveChromeListeners()
{
  if ( mChromeTooltipListener ) {
    mChromeTooltipListener->RemoveChromeListeners();
    NS_RELEASE(mChromeTooltipListener);
  }
  if ( mChromeContextMenuListener ) {
    mChromeContextMenuListener->RemoveChromeListeners();
    NS_RELEASE(mChromeContextMenuListener);
  }

  nsCOMPtr<EventTarget> piTarget;
  GetDOMEventTarget(mWebBrowser, getter_AddRefs(piTarget));
  if (!piTarget)
    return NS_OK;

  EventListenerManager* elmP = piTarget->GetOrCreateListenerManager();
  if (elmP)
  {
    elmP->RemoveEventListenerByType(this, NS_LITERAL_STRING("dragover"),
                                    TrustedEventsAtSystemGroupBubble());
    elmP->RemoveEventListenerByType(this, NS_LITERAL_STRING("drop"),
                                    TrustedEventsAtSystemGroupBubble());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(aEvent);
  NS_ENSURE_TRUE(dragEvent, NS_ERROR_INVALID_ARG);

  bool defaultPrevented;
  aEvent->GetDefaultPrevented(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  nsCOMPtr<nsIDroppedLinkHandler> handler = do_GetService("@mozilla.org/content/dropped-link-handler;1");
  if (handler) {
    nsAutoString eventType;
    aEvent->GetType(eventType);
    if (eventType.EqualsLiteral("dragover")) {
      bool canDropLink;
      handler->CanDropLink(dragEvent, false, &canDropLink);
      if (canDropLink)
        aEvent->PreventDefault();
    }
    else if (eventType.EqualsLiteral("drop")) {
      nsIWebNavigation* webnav = static_cast<nsIWebNavigation *>(mWebBrowser);

      nsAutoString link, name;
      if (webnav && NS_SUCCEEDED(handler->DropLink(dragEvent, link, false, name))) {
        if (!link.IsEmpty()) {
          webnav->LoadURI(link.get(), 0, nullptr, nullptr, nullptr);
        }
      }
      else {
        aEvent->StopPropagation();
        aEvent->PreventDefault();
      }
    }
  }

  return NS_OK;
}

already_AddRefed<nsIWebBrowserChrome>
nsDocShellTreeOwner::GetWebBrowserChrome()
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  if (mWebBrowserChromeWeak) {
    chrome = do_QueryReferent(mWebBrowserChromeWeak);
  } else if (mWebBrowserChrome) {
    chrome = mWebBrowserChrome;
  }
  return chrome.forget();
}

already_AddRefed<nsIEmbeddingSiteWindow>
nsDocShellTreeOwner::GetOwnerWin()
{
  nsCOMPtr<nsIEmbeddingSiteWindow> win;
  if (mWebBrowserChromeWeak) {
    win = do_QueryReferent(mWebBrowserChromeWeak);
  } else if (mOwnerWin) {
    win = mOwnerWin;
  }
  return win.forget();
}

already_AddRefed<nsIInterfaceRequestor>
nsDocShellTreeOwner::GetOwnerRequestor()
{
  nsCOMPtr<nsIInterfaceRequestor> req;
  if (mWebBrowserChromeWeak) {
    req = do_QueryReferent(mWebBrowserChromeWeak);
  } else if (mOwnerRequestor) {
    req = mOwnerRequestor;
  }
  return req.forget();
}


///////////////////////////////////////////////////////////////////////////////
// DefaultTooltipTextProvider

class DefaultTooltipTextProvider MOZ_FINAL : public nsITooltipTextProvider
{
public:
    DefaultTooltipTextProvider();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITOOLTIPTEXTPROVIDER

protected:
    ~DefaultTooltipTextProvider() {}

    nsCOMPtr<nsIAtom>   mTag_dialog;
    nsCOMPtr<nsIAtom>   mTag_dialogheader;
    nsCOMPtr<nsIAtom>   mTag_window;
};

NS_IMPL_ISUPPORTS(DefaultTooltipTextProvider, nsITooltipTextProvider)

DefaultTooltipTextProvider::DefaultTooltipTextProvider()
{
    // There are certain element types which we don't want to use
    // as tool tip text. 
    mTag_dialog       = do_GetAtom("dialog");
    mTag_dialogheader = do_GetAtom("dialogheader");
    mTag_window       = do_GetAtom("window");   
}

//
// UseSVGTitle
//
// A helper routine that determines whether we're still interested
// in SVG titles. We need to stop at the SVG root element that
// has a document node parent
//
static bool
UseSVGTitle(nsIDOMElement *currElement)
{
  nsCOMPtr<dom::Element> element(do_QueryInterface(currElement));
  if (!element || !element->IsSVG() || !element->GetParentNode())
    return false;

  return element->GetParentNode()->NodeType() != nsIDOMNode::DOCUMENT_NODE;
}

/* void getNodeText (in nsIDOMNode aNode, out wstring aText); */
NS_IMETHODIMP
DefaultTooltipTextProvider::GetNodeText(nsIDOMNode *aNode, char16_t **aText,
                                        bool *_retval)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aText);

  nsString outText;

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);

  bool lookingForSVGTitle = true;
  bool found = false;
  nsCOMPtr<nsIDOMNode> current ( aNode );

  // If the element implement the constraint validation API and has no title,
  // show the validation message, if any.
  nsCOMPtr<nsIConstraintValidation> cvElement = do_QueryInterface(current);
  if (cvElement) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(cvElement);
    nsCOMPtr<nsIAtom> titleAtom = do_GetAtom("title");

    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(content);
    bool formHasNoValidate = false;
    mozilla::dom::Element* form = formControl->GetFormElement();
    if (form) {
      nsCOMPtr<nsIAtom> noValidateAtom = do_GetAtom("novalidate");
      formHasNoValidate = form->HasAttr(kNameSpaceID_None, noValidateAtom);
    }

    if (!content->HasAttr(kNameSpaceID_None, titleAtom) &&
        !formHasNoValidate) {
      cvElement->GetValidationMessage(outText);
      found = !outText.IsEmpty();
    }
  }

  while ( !found && current ) {
    nsCOMPtr<nsIDOMElement> currElement ( do_QueryInterface(current) );
    if ( currElement ) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(currElement));
      if (content) {
        nsIAtom *tagAtom = content->Tag();
        if (tagAtom != mTag_dialog &&
            tagAtom != mTag_dialogheader &&
            tagAtom != mTag_window) {
          // first try the normal title attribute...
          currElement->GetAttribute(NS_LITERAL_STRING("title"), outText);
          if ( outText.Length() )
            found = true;
          else {
            // ...ok, that didn't work, try it in the XLink namespace
            NS_NAMED_LITERAL_STRING(xlinkNS, "http://www.w3.org/1999/xlink");
            nsCOMPtr<mozilla::dom::Link> linkContent(do_QueryInterface(currElement));
            if (linkContent) {
              nsCOMPtr<nsIURI> uri(linkContent->GetURIExternal());
              if (uri) {
                currElement->GetAttributeNS(NS_LITERAL_STRING("http://www.w3.org/1999/xlink"), NS_LITERAL_STRING("title"), outText);
                if ( outText.Length() )
                  found = true;
              }
            }
            else {
              if (lookingForSVGTitle) {
                lookingForSVGTitle = UseSVGTitle(currElement);
              }
              if (lookingForSVGTitle) {
                nsINodeList* childNodes = node->ChildNodes();
                uint32_t childNodeCount = childNodes->Length();
                for (uint32_t i = 0; i < childNodeCount; i++) {
                  nsIContent* child = childNodes->Item(i);
                  if (child->IsSVG(nsGkAtoms::title)) {
                    static_cast<dom::SVGTitleElement*>(child)->GetTextContent(outText);
                    if ( outText.Length() )
                      found = true;
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }

    // not found here, walk up to the parent and keep trying
    if ( !found ) {
      nsCOMPtr<nsIDOMNode> temp ( current );
      temp->GetParentNode(getter_AddRefs(current));
    }
  } // while not found

  *_retval = found;
  *aText = (found) ? ToNewUnicode(outText) : nullptr;

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(ChromeTooltipListener, nsIDOMEventListener)

//
// ChromeTooltipListener ctor
//

ChromeTooltipListener::ChromeTooltipListener(nsWebBrowser* inBrowser,
                                             nsIWebBrowserChrome* inChrome) 
  : mWebBrowser(inBrowser), mWebBrowserChrome(inChrome),
     mTooltipListenerInstalled(false),
     mMouseClientX(0), mMouseClientY(0),
     mShowingTooltip(false), mTooltipShownOnce(false)
{
  mTooltipTextProvider = do_GetService(NS_TOOLTIPTEXTPROVIDER_CONTRACTID);
  if (!mTooltipTextProvider) {
    nsISupports *pProvider = (nsISupports *) new DefaultTooltipTextProvider;
    mTooltipTextProvider = do_QueryInterface(pProvider);
  }
} // ctor


//
// ChromeTooltipListener dtor
//
ChromeTooltipListener::~ChromeTooltipListener()
{

} // dtor


//
// AddChromeListeners
//
// Hook up things to the chrome like context menus and tooltips, if the chrome
// has implemented the right interfaces.
//
NS_IMETHODIMP
ChromeTooltipListener::AddChromeListeners()
{
  if (!mEventTarget)
    GetDOMEventTarget(mWebBrowser, getter_AddRefs(mEventTarget));

  // Register the appropriate events for tooltips, but only if
  // the embedding chrome cares.
  nsresult rv = NS_OK;
  nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
  if ( tooltipListener && !mTooltipListenerInstalled ) {
    rv = AddTooltipListener();
    if ( NS_FAILED(rv) )
      return rv;
  }

  return rv;

} // AddChromeListeners


//
// AddTooltipListener
//
// Subscribe to the events that will allow us to track tooltips. We need "mouse" for mouseExit,
// "mouse motion" for mouseMove, and "key" for keyDown. As we add the listeners, keep track
// of how many succeed so we can clean up correctly in Release().
//
NS_IMETHODIMP
ChromeTooltipListener::AddTooltipListener()
{
  if (mEventTarget) {
    nsresult rv = mEventTarget->AddEventListener(NS_LITERAL_STRING("keydown"),
                                                 this, false, false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), this,
                                        false, false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->AddEventListener(NS_LITERAL_STRING("mouseout"), this,
                                        false, false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->AddEventListener(NS_LITERAL_STRING("mousemove"), this,
                                        false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    mTooltipListenerInstalled = true;
  }

  return NS_OK;
}


//
// RemoveChromeListeners
//
// Unsubscribe from the various things we've hooked up to the window root.
//
NS_IMETHODIMP
ChromeTooltipListener::RemoveChromeListeners ( )
{
  HideTooltip();

  if ( mTooltipListenerInstalled )
    RemoveTooltipListener();
  
  mEventTarget = nullptr;
  
  // it really doesn't matter if these fail...
  return NS_OK;
  
} // RemoveChromeTooltipListeners



//
// RemoveTooltipListener
//
// Unsubscribe from all the various tooltip events that we were listening to
//
NS_IMETHODIMP 
ChromeTooltipListener::RemoveTooltipListener()
{
  if (mEventTarget) {
    nsresult rv =
      mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), this,
                                        false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"),
                                           this, false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this,
                                           false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                           this, false);
    NS_ENSURE_SUCCESS(rv, rv);

    mTooltipListenerInstalled = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
ChromeTooltipListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.EqualsLiteral("keydown") ||
      eventType.EqualsLiteral("mousedown")) {
    return HideTooltip();
  }
  else if (eventType.EqualsLiteral("mouseout")) {
    // Reset flag so that tooltip will display on the next MouseMove
    mTooltipShownOnce = false;
    return HideTooltip();
  }
  else if (eventType.EqualsLiteral("mousemove")) {
    return MouseMove(aEvent);
  }

  NS_ERROR("Unexpected event type");
  return NS_OK;
}

//
// MouseMove
//
// If we're a tooltip, fire off a timer to see if a tooltip should be shown. If the
// timer fires, we cache the node in |mPossibleTooltipNode|.
//
nsresult
ChromeTooltipListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent)
    return NS_OK;

  // stash the coordinates of the event so that we can still get back to it from within the 
  // timer callback. On win32, we'll get a MouseMove event even when a popup goes away --
  // even when the mouse doesn't change position! To get around this, we make sure the
  // mouse has really moved before proceeding.
  int32_t newMouseX, newMouseY;
  mouseEvent->GetClientX(&newMouseX);
  mouseEvent->GetClientY(&newMouseY);
  if ( mMouseClientX == newMouseX && mMouseClientY == newMouseY )
    return NS_OK;

  // Filter out minor mouse movements.
  if (mShowingTooltip &&
      (abs(mMouseClientX - newMouseX) <= kTooltipMouseMoveTolerance) &&
      (abs(mMouseClientY - newMouseY) <= kTooltipMouseMoveTolerance))
    return NS_OK;

  mMouseClientX = newMouseX; mMouseClientY = newMouseY;
  mouseEvent->GetScreenX(&mMouseScreenX);
  mouseEvent->GetScreenY(&mMouseScreenY);

  if ( mTooltipTimer ) {
    mTooltipTimer->Cancel();
  }

  if (!mShowingTooltip && !mTooltipShownOnce) {
    mTooltipTimer = do_CreateInstance("@mozilla.org/timer;1");
    if ( mTooltipTimer ) {
      nsCOMPtr<EventTarget> eventTarget = aMouseEvent->InternalDOMEvent()->GetTarget();
      if ( eventTarget )
        mPossibleTooltipNode = do_QueryInterface(eventTarget);
      if ( mPossibleTooltipNode ) {
        nsresult rv =
          mTooltipTimer->InitWithFuncCallback(sTooltipCallback, this,
            LookAndFeel::GetInt(LookAndFeel::eIntID_TooltipDelay, 500),
            nsITimer::TYPE_ONE_SHOT);
        if (NS_FAILED(rv))
          mPossibleTooltipNode = nullptr;
      }
    }
    else
      NS_WARNING ( "Could not create a timer for tooltip tracking" );
  }
  else {
    mTooltipShownOnce = true;
    return HideTooltip();
  }

  return NS_OK;

} // MouseMove


//
// ShowTooltip
//
// Tell the registered chrome that they should show the tooltip
//
NS_IMETHODIMP
ChromeTooltipListener::ShowTooltip(int32_t inXCoords, int32_t inYCoords,
                                   const nsAString & inTipText)
{
  nsresult rv = NS_OK;
  
  // do the work to call the client
  nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
  if ( tooltipListener ) {
    rv = tooltipListener->OnShowTooltip ( inXCoords, inYCoords, PromiseFlatString(inTipText).get() ); 
    if ( NS_SUCCEEDED(rv) )
      mShowingTooltip = true;
  }

  return rv;
  
} // ShowTooltip


//
// HideTooltip
//
// Tell the registered chrome that they should rollup the tooltip
// NOTE: This routine is safe to call even if the popup is already closed.
//
NS_IMETHODIMP
ChromeTooltipListener::HideTooltip()
{
  nsresult rv = NS_OK;
  
  // shut down the relevant timers
  if ( mTooltipTimer ) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nullptr;
    // release tooltip target
    mPossibleTooltipNode = nullptr;
  }

  // if we're showing the tip, tell the chrome to hide it
  if ( mShowingTooltip ) {
    nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
    if ( tooltipListener ) {
      rv = tooltipListener->OnHideTooltip ( );
      if ( NS_SUCCEEDED(rv) )
        mShowingTooltip = false;
    }
  }

  return rv;
  
} // HideTooltip


//
// sTooltipCallback
//
// A timer callback, fired when the mouse has hovered inside of a frame for the 
// appropriate amount of time. Getting to this point means that we should show the
// tooltip, but only after we determine there is an appropriate TITLE element.
//
// This relies on certain things being cached into the |aChromeTooltipListener| object passed to
// us by the timer:
//   -- the x/y coordinates of the mouse      (mMouseClientY, mMouseClientX)
//   -- the dom node the user hovered over    (mPossibleTooltipNode)
//
void
ChromeTooltipListener::sTooltipCallback(nsITimer *aTimer,
                                        void *aChromeTooltipListener)
{
  ChromeTooltipListener* self = static_cast<ChromeTooltipListener*>
                                           (aChromeTooltipListener);
  if ( self && self->mPossibleTooltipNode ){
    // The actual coordinates we want to put the tooltip at are relative to the
    // toplevel docshell of our mWebBrowser.  We know what the screen
    // coordinates of the mouse event were, which means we just need the screen
    // coordinates of the docshell.  Unfortunately, there is no good way to
    // find those short of groveling for the presentation in that docshell and
    // finding the screen coords of its toplevel widget...
    nsCOMPtr<nsIDocShell> docShell =
      do_GetInterface(static_cast<nsIWebBrowser*>(self->mWebBrowser));
    nsCOMPtr<nsIPresShell> shell;
    if (docShell) {
      shell = docShell->GetPresShell();
    }

    nsIWidget* widget = nullptr;
    if (shell) {
      nsViewManager* vm = shell->GetViewManager();
      if (vm) {
        nsView* view = vm->GetRootView();
        if (view) {
          nsPoint offset;
          widget = view->GetNearestWidget(&offset);
        }
      }
    }

    if (!widget) {
      // release tooltip target if there is one, NO MATTER WHAT
      self->mPossibleTooltipNode = nullptr;
      return;
    }

    // if there is text associated with the node, show the tip and fire
    // off a timer to auto-hide it.

    nsXPIDLString tooltipText;
    if (self->mTooltipTextProvider) {
      bool textFound = false;

      self->mTooltipTextProvider->GetNodeText(
          self->mPossibleTooltipNode, getter_Copies(tooltipText), &textFound);
      
      if (textFound) {
        nsString tipText(tooltipText);
        nsIntPoint screenDot = widget->WidgetToScreenOffset();
        self->ShowTooltip (self->mMouseScreenX - screenDot.x,
                           self->mMouseScreenY - screenDot.y,
                           tipText);
      }
    }
    
    // release tooltip target if there is one, NO MATTER WHAT
    self->mPossibleTooltipNode = nullptr;
  } // if "self" data valid
  
} // sTooltipCallback


NS_IMPL_ISUPPORTS(ChromeContextMenuListener, nsIDOMEventListener)


//
// ChromeTooltipListener ctor
//
ChromeContextMenuListener::ChromeContextMenuListener(nsWebBrowser* inBrowser, nsIWebBrowserChrome* inChrome ) 
  : mContextMenuListenerInstalled(false),
    mWebBrowser(inBrowser),
    mWebBrowserChrome(inChrome)
{
} // ctor


//
// ChromeTooltipListener dtor
//
ChromeContextMenuListener::~ChromeContextMenuListener()
{
} // dtor


//
// AddContextMenuListener
//
// Subscribe to the events that will allow us to track context menus. Bascially, this
// is just the context-menu DOM event.
//
NS_IMETHODIMP
ChromeContextMenuListener::AddContextMenuListener()
{
  if (mEventTarget) {
    nsresult rv =
      mEventTarget->AddEventListener(NS_LITERAL_STRING("contextmenu"), this,
                                     false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    mContextMenuListenerInstalled = true;
  }

  return NS_OK;
}


//
// RemoveContextMenuListener
//
// Unsubscribe from all the various context menu events that we were listening to. 
//
NS_IMETHODIMP 
ChromeContextMenuListener::RemoveContextMenuListener()
{
  if (mEventTarget) {
    nsresult rv =
      mEventTarget->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this,
                                        false);
    NS_ENSURE_SUCCESS(rv, rv);

    mContextMenuListenerInstalled = false;
  }

  return NS_OK;
}


//
// AddChromeListeners
//
// Hook up things to the chrome like context menus and tooltips, if the chrome
// has implemented the right interfaces.
//
NS_IMETHODIMP
ChromeContextMenuListener::AddChromeListeners()
{
  if (!mEventTarget)
    GetDOMEventTarget(mWebBrowser, getter_AddRefs(mEventTarget));

  // Register the appropriate events for context menus, but only if
  // the embedding chrome cares.
  nsresult rv = NS_OK;

  nsCOMPtr<nsIContextMenuListener2> contextListener2 ( do_QueryInterface(mWebBrowserChrome) );
  nsCOMPtr<nsIContextMenuListener> contextListener ( do_QueryInterface(mWebBrowserChrome) );
  if ( (contextListener || contextListener2) && !mContextMenuListenerInstalled )
    rv = AddContextMenuListener();

  return rv;

} // AddChromeListeners


//
// RemoveChromeListeners
//
// Unsubscribe from the various things we've hooked up to the window root.
//
NS_IMETHODIMP
ChromeContextMenuListener::RemoveChromeListeners()
{
  if ( mContextMenuListenerInstalled )
    RemoveContextMenuListener();
  
  mEventTarget = nullptr;
  
  // it really doesn't matter if these fail...
  return NS_OK;
  
} // RemoveChromeTooltipListeners



//
// ContextMenu
//
// We're on call to show the context menu. Dig around in the DOM to
// find the type of object we're dealing with and notify the front
// end chrome.
//
NS_IMETHODIMP
ChromeContextMenuListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_UNEXPECTED);

  bool isDefaultPrevented = false;
  aMouseEvent->GetDefaultPrevented(&isDefaultPrevented);
  if (isDefaultPrevented) {
    return NS_OK;
  }

  nsCOMPtr<EventTarget> targetNode = aMouseEvent->InternalDOMEvent()->GetTarget();
  if (!targetNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> targetDOMnode;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(targetNode);
  if (!node)
    return NS_OK;

  // Stop the context menu event going to other windows (bug 78396)
  aMouseEvent->PreventDefault();

  // If the listener is a nsIContextMenuListener2, create the info object
  nsCOMPtr<nsIContextMenuListener2> menuListener2(do_QueryInterface(mWebBrowserChrome));
  nsContextMenuInfo *menuInfoImpl = nullptr;
  nsCOMPtr<nsIContextMenuInfo> menuInfo;
  if (menuListener2) {
    menuInfoImpl = new nsContextMenuInfo;
    menuInfo = menuInfoImpl; 
  }

  uint32_t flags = nsIContextMenuListener::CONTEXT_NONE;
  uint32_t flags2 = nsIContextMenuListener2::CONTEXT_NONE;

  // XXX test for selected text

  uint16_t nodeType;
  nsresult res = node->GetNodeType(&nodeType);
  NS_ENSURE_SUCCESS(res, res);

  // First, checks for nodes that never have children.
  if (nodeType == nsIDOMNode::ELEMENT_NODE) {
    nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(node));
    if (content) {
      nsCOMPtr<nsIURI> imgUri;
      content->GetCurrentURI(getter_AddRefs(imgUri));
      if (imgUri) {
        flags |= nsIContextMenuListener::CONTEXT_IMAGE;
        flags2 |= nsIContextMenuListener2::CONTEXT_IMAGE;
        targetDOMnode = node;
      }
    }

    nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(node));
    if (formControl) {
      if (formControl->GetType() == NS_FORM_TEXTAREA) {
        flags |= nsIContextMenuListener::CONTEXT_TEXT;
        flags2 |= nsIContextMenuListener2::CONTEXT_TEXT;
        targetDOMnode = node;
      } else {
        nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(formControl));
        if (inputElement) {
          flags |= nsIContextMenuListener::CONTEXT_INPUT;
          flags2 |= nsIContextMenuListener2::CONTEXT_INPUT;

          if (menuListener2) {
            if (formControl->IsSingleLineTextControl(false)) {
              flags2 |= nsIContextMenuListener2::CONTEXT_TEXT;
            }
          }

          targetDOMnode = node;
        }
      }
    }

    // always consume events for plugins and Java who may throw their
    // own context menus but not for image objects.  Document objects
    // will never be targets or ancestors of targets, so that's OK.
    nsCOMPtr<nsIDOMHTMLObjectElement> objectElement;
    if (!(flags & nsIContextMenuListener::CONTEXT_IMAGE))
      objectElement = do_QueryInterface(node);
    nsCOMPtr<nsIDOMHTMLEmbedElement> embedElement(do_QueryInterface(node));
    nsCOMPtr<nsIDOMHTMLAppletElement> appletElement(do_QueryInterface(node));

    if (objectElement || embedElement || appletElement)
      return NS_OK;
  }

  // Bubble out, looking for items of interest
  do {
    uint16_t nodeType;
    res = node->GetNodeType(&nodeType);
    NS_ENSURE_SUCCESS(res, res);

    if (nodeType == nsIDOMNode::ELEMENT_NODE) {

      // Test if the element has an associated link
      nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node));

      bool hasAttr = false;
      res = element->HasAttribute(NS_LITERAL_STRING("href"), &hasAttr);

      if (NS_SUCCEEDED(res) && hasAttr)
      {
        flags |= nsIContextMenuListener::CONTEXT_LINK;
        flags2 |= nsIContextMenuListener2::CONTEXT_LINK;
        if (!targetDOMnode)
          targetDOMnode = node;
        if (menuInfoImpl)
          menuInfoImpl->SetAssociatedLink(node);
        break; // exit do-while
      }
    }

    // walk-up-the-tree
    nsCOMPtr<nsIDOMNode> parentNode;
    node->GetParentNode(getter_AddRefs(parentNode));
    node = parentNode;
  } while (node);

  if (!flags && !flags2) {
    // We found nothing of interest so far, check if we
    // have at least an html document.
    nsCOMPtr<nsIDOMDocument> document;
    node = do_QueryInterface(targetNode);
    node->GetOwnerDocument(getter_AddRefs(document));
    nsCOMPtr<nsIDOMHTMLDocument> htmlDocument(do_QueryInterface(document));
    if (htmlDocument) {
      flags |= nsIContextMenuListener::CONTEXT_DOCUMENT;
      flags2 |= nsIContextMenuListener2::CONTEXT_DOCUMENT;
      targetDOMnode = node;
      if (!(flags & nsIContextMenuListener::CONTEXT_IMAGE)) {
        // check if this is a background image that the user was trying to click on
        // and if the listener is ready for that (only nsIContextMenuListener2 and up)
        if (menuInfoImpl && menuInfoImpl->HasBackgroundImage(targetDOMnode)) {
          flags2 |= nsIContextMenuListener2::CONTEXT_BACKGROUND_IMAGE;
          // For the embedder to get the correct background image 
          // targetDOMnode must point to the original node. 
          targetDOMnode = do_QueryInterface(targetNode);
        }
      }
    }
  }

  // we need to cache the event target into the focus controller's popupNode
  // so we can get at it later from command code, etc.:

  // get the dom window
  nsCOMPtr<nsIDOMWindow> win;
  res = mWebBrowser->GetContentDOMWindow(getter_AddRefs(win));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(win));
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
  if (root) {
    // set the window root's popup node to the event target
    root->SetPopupNode(targetDOMnode);
  }

  // Tell the listener all about the event
  if ( menuListener2 ) {
    menuInfoImpl->SetMouseEvent(aMouseEvent);
    menuInfoImpl->SetDOMNode(targetDOMnode);
    menuListener2->OnShowContextMenu(flags2, menuInfo);
  }
  else {
    nsCOMPtr<nsIContextMenuListener> menuListener(do_QueryInterface(mWebBrowserChrome));
    if ( menuListener )
      menuListener->OnShowContextMenu(flags, aMouseEvent, targetDOMnode);
  }

  return NS_OK;

} // MouseDown
