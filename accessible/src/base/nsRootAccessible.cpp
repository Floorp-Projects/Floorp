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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#include "nsIAccessible.h"
#include "nsRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsReadableUtils.h"
#include "nsILink.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIXULDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsCURILoader.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsXULTreeAccessible.h"
#include "nsITreeSelection.h"
#include "nsAccessibilityService.h"
#include "nsISelectionPrivate.h"
#include "nsICaret.h"
#include "nsIAccessibleCaret.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsAccessibleEventData.h"

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) 
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener) 
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFormListener)
NS_INTERFACE_MAP_END_INHERITING(nsAccessible)


NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsAccessible);
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsAccessible);

nsIDOMNode * nsRootAccessible::gLastFocusedNode = 0; // Strong reference

PRUint32 nsRootAccessible::gInstanceCount = 0;

//#define DEBUG_LEAKS 1 // aaronl debug


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIWeakReference* aShell):nsAccessible(nsnull,aShell), 
  nsDocAccessibleMixin(aShell), mListener(nsnull), 
  mAccService(do_GetService("@mozilla.org/accessibilityService;1")),
  mBusy(eBusyStateUninitialized), mScrollPositionChangedTicks(0), mScrollablePresShells(nsnull)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (shell) {
    shell->GetDocument(getter_AddRefs(mDocument));
    mDOMNode = do_QueryInterface(mDocument);
  }
  ++gInstanceCount;
#ifdef DEBUG_LEAKS
  printf("=====> %d nsRootAccessible's %x\n", gInstanceCount, (PRUint32)this); 
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
  if (--gInstanceCount == 0) 
    NS_IF_RELEASE(gLastFocusedNode);
#ifdef DEBUG_LEAKS
  printf("======> %d nsRootAccessible's %x\n", gInstanceCount, (PRUint32)this); 
#endif

  RemoveAccessibleEventListener();
  if (mScrollablePresShells)
    delete mScrollablePresShells;
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsRootAccessible::GetAccName(nsAString& aAccName) 
{ 
  return GetTitle(aAccName);
}

// helpers
nsIFrame* nsRootAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));

  nsIFrame* root = nsnull;
  if (shell) 
    shell->GetRootFrame(&root);

  return root;
}

void nsRootAccessible::GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  *aRelativeFrame = GetFrame();
  if (*aRelativeFrame)
    (*aRelativeFrame)->GetRect(aBounds);
}

/* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = nsnull;
  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetAccRole(PRUint32 *aAccRole) 
{ 
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
    *aAccRole = 0;
    return NS_ERROR_FAILURE;
  }

  /*
  // Commenting this out for now.
  // It was requested that we always use MSAA ROLE_PANE objects instead of client objects.
  // However, it might be asked that we put client objects back.

  nsCOMPtr<nsIPresContext> context; 
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsISupports> container;
  context->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem, docTreeItem(do_QueryInterface(container));
    if (docTreeItem) {
      docTreeItem->GetSameTypeParent(getter_AddRefs(parentTreeItem));
      // Basically, if this docshell has a parent of the same type, it's a frame
      if (parentTreeItem) {
        *aAccRole = ROLE_PANE;
        return NS_OK;
      }
    }
  }

  *aAccRole = ROLE_CLIENT;
  */

  *aAccRole = ROLE_PANE;

  // If it's a <dialog>, use ROLE_DIALOG instead
  nsCOMPtr<nsIContent> rootContent;
  mDocument->GetRootContent(getter_AddRefs(rootContent));
  if (rootContent) {
    nsCOMPtr<nsIDOMElement> rootElement(do_QueryInterface(rootContent));
    if (rootElement) {
      nsAutoString name;
      rootElement->GetLocalName(name);
      if (name.Equals(NS_LITERAL_STRING("dialog"))) 
        *aAccRole = ROLE_DIALOG;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::GetAccState(PRUint32 *aAccState)
{
  *aAccState = STATE_FOCUSABLE;
  if (mBusy == eBusyStateLoading)
    *aAccState |= STATE_BUSY;
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::GetAccValue(nsAString& aAccValue)
{
  return GetURL(aAccValue);
}


NS_IMETHODIMP nsRootAccessible::Notify(nsITimer *timer)
{
  if (mScrollPositionChangedTicks) {
    if (++mScrollPositionChangedTicks > 2) {
      // Whenever scroll position changes, mScrollPositionChangeTicks gets reset to 1
      // We only want to fire accessibilty scroll event when scrolling stops or pauses
      // Therefore, we wait for no scroll events to occur between 2 ticks of this timer
      // That indicates a pause in scrolling, so we fire the accessibilty scroll event
      nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mLastScrolledPresShell));
      if (mListener && presShell) {
        nsCOMPtr<nsIAccessible> docAccessible;
        if (mPresShell == mLastScrolledPresShell)
          docAccessible = this;  // Fast way, don't need to create new accessible to fire event on the root
        else {
          // Need to create accessible for document that had scrolling occur in it
          nsCOMPtr<nsIDocument> doc;
          presShell->GetDocument(getter_AddRefs(doc));
          nsCOMPtr<nsIDOMNode> docNode(do_QueryInterface(doc));
          mAccService->GetAccessibleFor(docNode, getter_AddRefs(docAccessible));
        }

        if (docAccessible)
          mListener->HandleEvent(nsIAccessibleEventListener::EVENT_SCROLLINGEND, docAccessible, nsnull);
      }
      mScrollPositionChangedTicks = 0;
      mLastScrolledPresShell = nsnull;
    }
  }
  return NS_OK;
}
  
void nsRootAccessible::AddScrollListener(nsIPresShell *aPresShell)
{
  nsCOMPtr<nsIViewManager> vm;

  if (aPresShell)
    aPresShell->GetViewManager(getter_AddRefs(vm));

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (!scrollableView)
    return;

  if (!mScrollablePresShells)
    mScrollablePresShells = new nsSupportsHashtable(SCROLL_HASH_START_SIZE, PR_TRUE);  // PR_TRUE = thread safe hash table

  if (mScrollablePresShells) {
    nsISupportsKey key(scrollableView);
    nsCOMPtr<nsISupports> supports(dont_AddRef(NS_STATIC_CAST(nsISupports*, mScrollablePresShells->Get(&key))));
    if (supports)  // This scroll view is already being listened to, remove it. We will re-add it w/ current pres shell
      RemoveScrollListener(aPresShell);

    // Add to hash table, so we can retrieve correct pres shell later
    nsCOMPtr<nsIWeakReference> weakShell;
    weakShell = do_GetWeakReference(aPresShell);
    mScrollablePresShells->Put(&key, weakShell);

    // Add listener
    scrollableView->AddScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, this));
  }
}

void nsRootAccessible::RemoveScrollListener(nsIPresShell *aPresShell)
{
  nsCOMPtr<nsIViewManager> vm;

  if (aPresShell)
    aPresShell->GetViewManager(getter_AddRefs(vm));

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (scrollableView)
    scrollableView->RemoveScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, this));

  if (mScrollablePresShells) {
    nsISupportsKey key(scrollableView);
    nsCOMPtr<nsISupports> supp;
    mScrollablePresShells->Remove(&key, getter_AddRefs(supp));
  }
}

PRBool PR_CALLBACK
RemoveScrollListenerEnum(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(NS_STATIC_CAST(nsIWeakReference*, aData)));

  // If presShell gone, then our nsIScrollPositionListener will also already be gone
  if (presShell) {
    nsCOMPtr<nsIViewManager> vm;
    if (presShell)
      presShell->GetViewManager(getter_AddRefs(vm));

    nsIScrollableView* scrollableView = nsnull;
    if (vm)
      vm->GetRootScrollableView(&scrollableView);

    if (scrollableView)
      scrollableView->RemoveScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, aClosure));
  }

  return PR_TRUE;
}

NS_IMETHODIMP nsRootAccessible::ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::ScrollPositionDidChange(nsIScrollableView *aScrollableView, nscoord aX, nscoord aY)
{
  if (mListener) {
    if (!mScrollablePresShells)
      return NS_OK;
    nsISupportsKey key(aScrollableView);
    nsCOMPtr<nsIWeakReference> weakShell(dont_AddRef(NS_STATIC_CAST(nsIWeakReference*, mScrollablePresShells->Get(&key))));
    if (!weakShell)
      return NS_OK;
    if (mTimer && (mBusy != eBusyStateDone || (mScrollPositionChangedTicks && mLastScrolledPresShell != weakShell))) {
      // We need to say finish up our current time work and start a new timer
      // either because we haven't yet fired our finished loading event, or 
      // a scroll event from another pres shell is still waiting to be fired
      Notify(mTimer);
      mTimer->Cancel();
    }
    // Start new timer, if the timer cycles at least 1 full cycle without more scroll position changes, 
    // then the ::Notify() method will fire the accessibility event for scroll position changes
    nsresult rv;
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      const PRUint32 kScrollPosCheckWait = 50;
      mTimer->InitWithCallback(NS_STATIC_CAST(nsITimerCallback*, this), kScrollPosCheckWait, nsITimer::TYPE_REPEATING_SLACK);
    }
    mScrollPositionChangedTicks = 1;
    mLastScrolledPresShell = weakShell;
  }
  return NS_OK;
}

/* void addAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsRootAccessible::AddAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
  NS_ASSERTION(aListener, "Trying to add a null listener!");
  if (mListener)
    return NS_OK;

  mListener =  aListener;

  // use AddEventListener from the nsIDOMEventTarget interface
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  if (target) { 
    // capture DOM focus events 
    nsresult rv = target->AddEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // capture Form change events 
    rv = target->AddEventListener(NS_LITERAL_STRING("select"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // add ourself as a CheckboxStateChange listener (custom event fired in nsHTMLInputElement.cpp)
    rv = target->AddEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // add ourself as a RadioStateChange Listener ( custom event fired in in nsHTMLInputElement.cpp  & radio.xml)
    rv = target->AddEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    rv = target->AddEventListener(NS_LITERAL_STRING("ListitemStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    rv = target->AddEventListener(NS_LITERAL_STRING("popupshowing"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    rv = target->AddEventListener(NS_LITERAL_STRING("popuphiding"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuItemActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
    
    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
    
    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
    
    // Set up web progress listener - we need to know when page loading is finished
    // That way we can send the STATE_CHANGE events for the MSAA root "pane" object (ROLE_PANE),
    // and change the STATE_BUSY bit flag
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mPresShell));
    if (presShell) {
      nsCOMPtr<nsIPresContext> context; 
      presShell->GetPresContext(getter_AddRefs(context));
      if (context) { 
        nsCOMPtr<nsISupports> container; context->GetContainer(getter_AddRefs(container));
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
        if (docShell) {
          mWebProgress = do_GetInterface(docShell);
          mWebProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_LOCATION | 
                                                  nsIWebProgress::NOTIFY_STATE_DOCUMENT);
        }
      }
    }
    NS_ASSERTION(mWebProgress, "Could not get nsIWebProgress for nsRootAccessible");
  }

  if (!mCaretAccessible && mListener)
    mAccService->CreateCaretAccessible(mDOMNode, mListener, getter_AddRefs(mCaretAccessible));

  return NS_OK;
}

/* void removeAccessibleEventListener (); */
NS_IMETHODIMP nsRootAccessible::RemoveAccessibleEventListener()
{
  if (mListener) {
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
    if (target) { 
      target->RemoveEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("select"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("popupshowing"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("popuphiding"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuItemActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("ListitemStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    }
 
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nsnull;
    }

    if (mWebProgress) {
      mWebProgress->RemoveProgressListener(this);
      mWebProgress = nsnull;
    }

    if (mScrollablePresShells)
      mScrollablePresShells->Enumerate(RemoveScrollListenerEnum, NS_STATIC_CAST(nsIScrollPositionListener*, this));

    mListener = nsnull;
  }

  if (mCaretAccessible) {
    mCaretAccessible->RemoveSelectionListener();
    mCaretAccessible = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::GetCaretAccessible(nsIAccessibleCaret **aCaretAccessible)
{
  *aCaretAccessible = mCaretAccessible;
  NS_IF_ADDREF(*aCaretAccessible);
  return NS_OK;
}

void nsRootAccessible::FireAccessibleFocusEvent(nsIAccessible *focusAccessible, nsIDOMNode *focusNode)
{
  if (focusAccessible && focusNode && gLastFocusedNode != focusNode) {
    mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, focusAccessible, nsnull);
    NS_IF_RELEASE(gLastFocusedNode);
    gLastFocusedNode = focusNode;
    NS_ADDREF(gLastFocusedNode);
    if (mCaretAccessible)
      mCaretAccessible->AttachNewSelectionListener(focusNode);
  }
}

// --------------- nsIDOMEventListener Methods (3) ------------------------

NS_IMETHODIMP nsRootAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  if (mListener) {
    // optionTargetNode is set to current option for HTML selects
    nsCOMPtr<nsIDOMNode> targetNode, optionTargetNode; 
    nsresult rv = GetTargetNode(aEvent, targetNode);
    if (NS_FAILED(rv))
      return rv;

    // Check to see if it's a select element. If so, need the currently focused option
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(targetNode));
    if (selectElement)     // ----- Target Node is an HTML <select> element ------
      nsHTMLSelectOptionAccessible::GetFocusedOptionNode(targetNode, optionTargetNode);

    // for focus events on Radio Groups we give the focus to the selected button
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl(do_QueryInterface(targetNode));
    if (selectControl) {
      nsCOMPtr<nsIDOMXULSelectControlItemElement> selectItem;
      selectControl->GetSelectedItem(getter_AddRefs(selectItem));
      optionTargetNode = do_QueryInterface(selectItem);
    }

    nsCOMPtr<nsIAccessible> accessible;
    if (NS_FAILED(mAccService->GetAccessibleFor(targetNode, getter_AddRefs(accessible))))
      return NS_OK;

    // If it's a tree element, need the currently selected item
    PRInt32 treeIndex = -1;
    nsCOMPtr<nsITreeBoxObject> treeBox;
    nsCOMPtr<nsIAccessible> treeItemAccessible;
    nsXULTreeAccessible::GetTreeBoxObject(targetNode, getter_AddRefs(treeBox));
    if (treeBox) {
      nsCOMPtr<nsITreeSelection> selection;
      treeBox->GetSelection(getter_AddRefs(selection));
      if (selection) {
        selection->GetCurrentIndex(&treeIndex);
        if (treeIndex >= 0) {
          nsCOMPtr<nsIWeakReference> weakShell;
          nsAccessibilityService::GetShellFromNode(targetNode, getter_AddRefs(weakShell));
          treeItemAccessible = new nsXULTreeitemAccessible(accessible, targetNode, weakShell, treeIndex);
          if (!treeItemAccessible)
            return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }

    nsAutoString eventType;
    aEvent->GetType(eventType);

#ifndef MOZ_ACCESSIBILITY_ATK
    // tree event
    if (treeItemAccessible && 
        (eventType.EqualsIgnoreCase("DOMMenuItemActive") || eventType.EqualsIgnoreCase("select"))) {
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, treeItemAccessible, nsnull);
      return NS_OK;
    }

    if (eventType.EqualsIgnoreCase("focus") || eventType.EqualsIgnoreCase("DOMMenuItemActive")) { 
      if (optionTargetNode &&
          NS_SUCCEEDED(mAccService->GetAccessibleFor(optionTargetNode, getter_AddRefs(accessible)))) {
        FireAccessibleFocusEvent(accessible, optionTargetNode);
      }
      else
        FireAccessibleFocusEvent(accessible, targetNode);
    }
    else if (eventType.EqualsIgnoreCase("ListitemStateChange")) {
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
      FireAccessibleFocusEvent(accessible, optionTargetNode);
    }
    else if (eventType.EqualsIgnoreCase("CheckboxStateChange")) { 
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
    }
    else if (eventType.EqualsIgnoreCase("RadioStateChange") ) {
      // first the XUL radio buttons
      if (targetNode &&
          NS_SUCCEEDED(mAccService->GetAccessibleFor(targetNode, getter_AddRefs(accessible)))) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
        FireAccessibleFocusEvent(accessible, targetNode);
      }
      else { // for the html radio buttons -- apparently the focus code just works. :-)
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
      }
    }
    else if (eventType.EqualsIgnoreCase("popupshowing")) 
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_MENUPOPUPSTART, accessible, nsnull);
    else if (eventType.EqualsIgnoreCase("popuphiding")) 
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_MENUPOPUPEND, accessible, nsnull);
    else if (eventType.EqualsIgnoreCase("DOMMenuBarActive")) 
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_MENUSTART, accessible, nsnull);
    else if (eventType.EqualsIgnoreCase("DOMMenuBarInactive")) {
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_MENUEND, accessible, nsnull);
      GetAccFocused(getter_AddRefs(accessible));
      if (accessible) {
        accessible->AccGetDOMNode(getter_AddRefs(targetNode));
        FireAccessibleFocusEvent(accessible, targetNode);
      }
    }
#else
    AtkStateChange stateData;
    AtkTextChange textData;
    if (eventType.EqualsIgnoreCase("focus") || eventType.EqualsIgnoreCase("DOMMenuItemActive")) {
      if (treeItemAccessible) // use focused treeitem
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, treeItemAccessible, nsnull);
      else if (optionTargetNode && // use focused option
          NS_SUCCEEDED(mAccService->GetAccessibleFor(optionTargetNode, getter_AddRefs(accessible))))
        FireAccessibleFocusEvent(accessible, optionTargetNode);
      else
        FireAccessibleFocusEvent(accessible, targetNode);
    }
    else if (eventType.EqualsIgnoreCase("change")) {
      if (selectElement) // it's a HTML <select>
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_ATK_SELECTION_CHANGE, accessible, nsnull);
      else {
        nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(targetNode));
        if (inputElement) { // it's a HTML <input>
          accessible->GetAccState(&stateData.state);
          stateData.enable = (stateData.state & STATE_CHECKED) != 0;
          stateData.state = STATE_CHECKED;
          mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, (AccessibleEventData*)&stateData);
        }
      }
    }
    else if (eventType.EqualsIgnoreCase("select")) {
      if (selectControl) // it's a XUL <listbox>
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_ATK_SELECTION_CHANGE, accessible, nsnull);
      else if (treeBox && treeIndex >= 0) // it's a XUL <tree>
        // use EVENT_FOCUS instead of EVENT_ATK_SELECTION_CHANGE
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, treeItemAccessible, nsnull);
    }
    else if (eventType.EqualsIgnoreCase("input")) {
      // XXX kyle.yuan@sun.com future work, put correct values for text change data
      textData.start = 0;
      textData.length = 0;
      textData.add = PR_TRUE;
      nsAutoString accName;
      accessible->GetAccValue(accName);
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_ATK_TEXT_CHANGE, accessible, (AccessibleEventData*)&textData);
    }
    else if (eventType.EqualsIgnoreCase("ListitemStateChange")) // it's a XUL <listbox>
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, accessible, nsnull);
    else if (eventType.EqualsIgnoreCase("CheckboxStateChange") || // it's a XUL <checkbox>
             eventType.EqualsIgnoreCase("RadioStateChange")) { // it's a XUL <radio>
      accessible->GetAccState(&stateData.state);
      stateData.enable = (stateData.state & STATE_CHECKED) != 0;
      stateData.state = STATE_CHECKED;
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, (AccessibleEventData*)&stateData);
    }
    else if (eventType.EqualsIgnoreCase("popupshowing")) 
      FireAccessibleFocusEvent(accessible, targetNode);
    else if (eventType.EqualsIgnoreCase("popuphiding")) 
      FireAccessibleFocusEvent(accessible, targetNode);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::GetTargetNode(nsIDOMEvent *aEvent, nsCOMPtr<nsIDOMNode>& aTargetNode)
{
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  if (nsevent) {
    nsevent->GetOriginalTarget(getter_AddRefs(domEventTarget));
  }

  nsresult rv;
  aTargetNode = do_QueryInterface(domEventTarget, &rv);

  return rv;
}

// ------- nsIDOMFocusListener Methods (1) -------------

NS_IMETHODIMP nsRootAccessible::Focus(nsIDOMEvent* aEvent) 
{ 
  return HandleEvent(aEvent);
}

NS_IMETHODIMP nsRootAccessible::Blur(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

// ------- nsIDOMFormListener Methods (5) -------------

NS_IMETHODIMP nsRootAccessible::Submit(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsRootAccessible::Reset(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsRootAccessible::Change(nsIDOMEvent* aEvent)
{
  // get change events when the form elements changes its state, checked->not,
  //  deleted text, new text, change in selection for list/combo boxes
  // this may be the event that we have the individual Accessible objects
  //  handle themselves -- have list/combos figure out the change in selection
  //  have textareas and inputs fire a change of state etc...
#ifdef MOZ_ACCESSIBILITY_ATK
  return HandleEvent(aEvent);
#else
  return NS_OK;   // Ignore form change events in MSAA
#endif
}

// gets Select events when text is selected in a textarea or input
NS_IMETHODIMP nsRootAccessible::Select(nsIDOMEvent* aEvent) 
{
  return HandleEvent(aEvent);
}

// gets Input events when text is entered or deleted in a textarea or input
NS_IMETHODIMP nsRootAccessible::Input(nsIDOMEvent* aEvent) 
{ 
#ifndef MOZ_ACCESSIBILITY_ATK
  return NS_OK; 
#else
  return HandleEvent(aEvent);
#endif
}

// ------- nsIDOMXULListener Methods (8) ---------------

NS_IMETHODIMP nsRootAccessible::PopupShowing(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::PopupShown(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::PopupHiding(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::PopupHidden(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Close(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Command(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Broadcast(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::CommandUpdate(nsIDOMEvent* aEvent) { return NS_OK; }

// ------- nsIAccessibleDocument Methods (5) ---------------

NS_IMETHODIMP nsRootAccessible::GetURL(nsAString& aURL)
{
  return nsDocAccessibleMixin::GetURL(aURL);
}

NS_IMETHODIMP nsRootAccessible::GetTitle(nsAString& aTitle)
{
  return nsDocAccessibleMixin::GetTitle(aTitle);
}

NS_IMETHODIMP nsRootAccessible::GetMimeType(nsAString& aMimeType)
{
  return nsDocAccessibleMixin::GetMimeType(aMimeType);
}

NS_IMETHODIMP nsRootAccessible::GetDocType(nsAString& aDocType)
{
  return nsDocAccessibleMixin::GetDocType(aDocType);
}

NS_IMETHODIMP nsRootAccessible::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAString& aNameSpaceURI)
{
  return nsDocAccessibleMixin::GetNameSpaceURIForID(aNameSpaceID, aNameSpaceURI);
}

NS_IMETHODIMP nsRootAccessible::GetDocument(nsIDocument **doc)
{
  return nsDocAccessibleMixin::GetDocument(doc);
}

NS_IMETHODIMP nsRootAccessible::OnStateChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  if ((aStateFlags & STATE_IS_DOCUMENT) && (aStateFlags & STATE_STOP)) {
    // Doc is ready!
    if (mBusy != eBusyStateDone) {
      mBusy = eBusyStateDone;
#ifndef MOZ_ACCESSIBILITY_ATK
      if (mListener)
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, this, nsnull);
#endif
    }

    // Set up scroll position listener
    nsCOMPtr<nsIDOMWindow> domWin;
    aWebProgress->GetDOMWindow(getter_AddRefs(domWin));
    if (domWin) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      domWin->GetDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
      if (doc) {
        nsCOMPtr<nsIPresShell> presShell;
        doc->GetShellAt(0, getter_AddRefs(presShell));
        AddScrollListener(presShell);
      }
    }
  }

  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsRootAccessible::OnProgressChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
  PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsRootAccessible::OnLocationChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsIURI *location)
{
  // Load has been verified, it will occur, about to commence
  if (mBusy != eBusyStateLoading) {
    mBusy = eBusyStateLoading; 
#ifndef MOZ_ACCESSIBILITY_ATK
    if (mListener)
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, this, nsnull);
#else
    if (mListener) {
      AtkChildrenChange childrenData;
      childrenData.index = -1;
      childrenData.child = 0;
      childrenData.add = PR_FALSE;
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_REORDER , this, (AccessibleEventData*)&childrenData);
    }
#endif

    // Document is going away, remove its scroll position listener
    nsCOMPtr<nsIDOMWindow> domWin;
    aWebProgress->GetDOMWindow(getter_AddRefs(domWin));
    if (domWin) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      domWin->GetDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
      if (doc) {
        nsCOMPtr<nsIPresShell> presShell;
        doc->GetShellAt(0, getter_AddRefs(presShell));
        RemoveScrollListener(presShell);
      }
    }
  }

  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsRootAccessible::OnStatusChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsRootAccessible::OnSecurityChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

nsDocAccessibleMixin::nsDocAccessibleMixin(nsIDocument *aDoc):mDocument(aDoc)
{
}

nsDocAccessibleMixin::nsDocAccessibleMixin(nsIWeakReference *aPresShell)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aPresShell));
  if (shell)
    shell->GetDocument(getter_AddRefs(mDocument));
}

nsDocAccessibleMixin::~nsDocAccessibleMixin()
{
}

NS_IMETHODIMP nsDocAccessibleMixin::GetURL(nsAString& aURL)
{ 
  nsCOMPtr<nsIPresShell> presShell;
  mDocument->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell;
  GetDocShellFromPS(presShell, getter_AddRefs(docShell));

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(docShell));
  nsCAutoString theURL;
  if (webNav) {
    nsCOMPtr<nsIURI> pURI;
    webNav->GetCurrentURI(getter_AddRefs(pURI));
    if (pURI) 
      pURI->GetSpec(theURL);
  }
  //XXXaaronl Need to use CopyUTF8toUCS2(nsDependentCString(theURL), aURL); when it's written
  aURL.Assign(NS_ConvertUTF8toUCS2(theURL)); 
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetTitle(nsAString& aTitle)
{
  // This doesn't leak - we don't own the const pointer that's returned
  aTitle = *(mDocument->GetDocumentTitle());
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetMimeType(nsAString& aMimeType)
{
  nsCOMPtr<nsIDOMNSDocument> domnsDocument(do_QueryInterface(mDocument));
  if (domnsDocument) {
    return domnsDocument->GetContentType(aMimeType);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetDocType(nsAString& aDocType)
{
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocumentType> docType;

  if (xulDoc) {
    aDocType = NS_LITERAL_STRING("window"); // doctype not implemented for XUL at time of writing - causes assertion
    return NS_OK;
  }
  else if (domDoc && NS_SUCCEEDED(domDoc->GetDoctype(getter_AddRefs(docType))) && docType) {
    return docType->GetName(aDocType);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAString& aNameSpaceURI)
{
  if (mDocument) {
    nsCOMPtr<nsINameSpaceManager> nameSpaceManager;
    if (NS_SUCCEEDED(mDocument->GetNameSpaceManager(*getter_AddRefs(nameSpaceManager)))) 
      return nameSpaceManager->GetNameSpaceURI(aNameSpaceID, aNameSpaceURI);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetDocument(nsIDocument **doc)
{
  *doc = mDocument;
  if (mDocument) {
    NS_IF_ADDREF(*doc);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsDocAccessibleMixin::GetDocShellFromPS(nsIPresShell* aPresShell, nsIDocShell** aDocShell)
{
  *aDocShell = nsnull;
  if (aPresShell) {
    nsCOMPtr<nsIDocument> doc;
    aPresShell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIScriptGlobalObject> scriptObj;
      doc->GetScriptGlobalObject(getter_AddRefs(scriptObj));
      if (scriptObj) {
        scriptObj->GetDocShell(aDocShell);
        if (*aDocShell)
          return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetCaretAccessible(nsIAccessibleCaret **aCaretAccessible)
{
  // Caret only owned by top level window's document
  *aCaretAccessible = nsnull;
  return NS_OK;
}

