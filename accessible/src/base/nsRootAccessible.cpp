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

// NOTE: alphabetically ordered
#include "nsAccessibilityService.h"
#include "nsAccessibleEventData.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIAccessibleCaret.h"
#include "nsIAccessibleHyperText.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScrollableView.h"
#include "nsIServiceManager.h"
#include "nsIViewManager.h"
#include "nsLayoutAtoms.h"
#include "nsReadableUtils.h"
#include "nsRootAccessible.h"
#ifdef MOZ_XUL
#include "nsXULTreeAccessible.h"
#include "nsITreeSelection.h"
#include "nsIXULDocument.h"
#endif
#include "nsAccessibilityService.h"
#include "nsISelectionPrivate.h"
#include "nsICaret.h"
#include "nsIAccessibleCaret.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsAccessibleEventData.h"

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) 
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener) 
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFormListener)
NS_INTERFACE_MAP_END_INHERITING(nsDocAccessible)


NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsDocAccessible);
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsDocAccessible);

nsIDOMNode * nsRootAccessible::gLastFocusedNode = 0; // Strong reference
PRUint32 nsRootAccessible::gActiveRootAccessibles = 0;

//#define DEBUG_LEAKS 1 // aaronl debug


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsDocAccessibleWrap(aDOMNode, aShell), 
  mListener(nsnull), 
  mScrollWatchTimer(nsnull), mDocLoadTimer(nsnull), mWebProgress(nsnull),
  mAccService(do_GetService("@mozilla.org/accessibilityService;1")),
  mBusy(eBusyStateUninitialized), mIsNewDocument(PR_FALSE), 
  mScrollPositionChangedTicks(0)
{
  ++gActiveRootAccessibles;

#ifdef DEBUG_LEAKS
  printf("=====> %d nsRootAccessible's %x\n", gActiveRootAccessibles, (PRUint32)this); 
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
#ifdef DEBUG_LEAKS
  printf("======> %d nsRootAccessible's %x\n", gActiveRootAccessibles, (PRUint32)this); 
#endif
}

/* attribute wstring accName; */
NS_IMETHODIMP nsRootAccessible::GetAccName(nsAString& aAccName) 
{ 
  return GetTitle(aAccName);
}

// helpers
/* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = nsnull;
  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetAccRole(PRUint32 *aAccRole) 
{ 
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

void nsRootAccessible::DocLoadCallback(nsITimer *aTimer, void *aClosure)
{
  // Doc has finished loading, fire "load finished" event
  // This path is only used if the doc was already finished loading
  // when the RootAccessible was created. 
  // Otherwise, ::OnStateChange() fires the event when doc is loaded.

  nsRootAccessible *rootAcc = NS_REINTERPRET_CAST(nsRootAccessible*, aClosure);
  if (rootAcc)
    rootAcc->FireDocLoadFinished();
}

void nsRootAccessible::ScrollTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsRootAccessible *rootAcc = NS_REINTERPRET_CAST(nsRootAccessible*, aClosure);

  if (rootAcc && rootAcc->mScrollPositionChangedTicks && 
      ++rootAcc->mScrollPositionChangedTicks > 2) {
    // Whenever scroll position changes, mScrollPositionChangeTicks gets reset to 1
    // We only want to fire accessibilty scroll event when scrolling stops or pauses
    // Therefore, we wait for no scroll events to occur between 2 ticks of this timer
    // That indicates a pause in scrolling, so we fire the accessibilty scroll event
    rootAcc->HandleEvent(nsIAccessibleEventListener::EVENT_SCROLLINGEND, rootAcc, nsnull);
    rootAcc->mScrollPositionChangedTicks = 0;
    rootAcc->mScrollWatchTimer->Cancel();
    rootAcc->mScrollWatchTimer = nsnull;
  }
}

void nsRootAccessible::AddScrollListener(nsIPresShell *aPresShell)
{
  nsCOMPtr<nsIViewManager> vm;

  if (aPresShell)
    aPresShell->GetViewManager(getter_AddRefs(vm));

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (scrollableView)
    scrollableView->AddScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, this));
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
}

NS_IMETHODIMP nsRootAccessible::ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::ScrollPositionDidChange(nsIScrollableView *aScrollableView, nscoord aX, nscoord aY)
{
  if (mListener) {
    // Start new timer, if the timer cycles at least 1 full cycle without more scroll position changes, 
    // then the ::Notify() method will fire the accessibility event for scroll position changes
    const PRUint32 kScrollPosCheckWait = 50;
    if (mScrollWatchTimer) {
      mScrollWatchTimer->SetDelay(kScrollPosCheckWait);  // Create new timer, to avoid leaks
    }
    else {
      mScrollWatchTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (mScrollWatchTimer) {
        mScrollWatchTimer->InitWithFuncCallback(ScrollTimerCallback, this,
                                                kScrollPosCheckWait, 
                                                nsITimer::TYPE_REPEATING_SLACK);
      }
    }
    mScrollPositionChangedTicks = 1;
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
    rv = target->AddEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // add ourself as a RadioStateChange Listener ( custom event fired in in nsHTMLInputElement.cpp  & radio.xml)
    rv = target->AddEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
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
    
    AddContentDocListeners();
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
      target->RemoveEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("ListitemStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("popupshowing"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("popuphiding"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuItemActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
      target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    }
 
    if (mScrollWatchTimer) {
      mScrollWatchTimer->Cancel();
      mScrollWatchTimer = nsnull;
    }

    if (mDocLoadTimer) {
      mDocLoadTimer->Cancel();
      mDocLoadTimer = nsnull;
    }

    RemoveContentDocListeners();

    mListener = nsnull;
  }

  if (mCaretAccessible) {
    mCaretAccessible->RemoveSelectionListener();
    mCaretAccessible = nsnull;
  }

  mAccService = nsnull;

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
    HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, focusAccessible, nsnull);
    NS_IF_RELEASE(gLastFocusedNode);
    PRUint32 role = ROLE_NOTHING;
    focusAccessible->GetAccRole(&role);
    if (role == ROLE_MENUITEM || role == ROLE_LISTITEM)
      gLastFocusedNode = nsnull; // This makes it report all focus events on menu and list items
    else {
      gLastFocusedNode = focusNode;
      NS_IF_ADDREF(gLastFocusedNode);
    }
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
    GetTargetNode(aEvent, getter_AddRefs(targetNode));
    if (!targetNode)
      return NS_ERROR_FAILURE;

    // Check to see if it's a select element. If so, need the currently focused option
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(targetNode));
    if (selectElement)     // ----- Target Node is an HTML <select> element ------
      nsHTMLSelectOptionAccessible::GetFocusedOptionNode(targetNode, getter_AddRefs(optionTargetNode));

    // for focus events on Radio Groups we give the focus to the selected button
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl(do_QueryInterface(targetNode));
    if (selectControl) {
      nsCOMPtr<nsIDOMXULSelectControlItemElement> selectItem;
      selectControl->GetSelectedItem(getter_AddRefs(selectItem));
      optionTargetNode = do_QueryInterface(selectItem);
    }

#ifdef MOZ_ACCESSIBILITY_ATK
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement(do_QueryInterface(targetNode));
    if (anchorElement) {
      nsCOMPtr<nsIDOMNode> blockNode;
      // For ATK, we don't create any individual object for hyperlink, use its parent who has block frame instead
      nsAccessible::GetParentBlockNode(targetNode, getter_AddRefs(blockNode));
      targetNode = blockNode;
    }
#endif

    nsCOMPtr<nsIAccessible> accessible;
    if (targetNode == mDOMNode) {
      QueryInterface(NS_GET_IID(nsIAccessible), getter_AddRefs(accessible));
    }
    else if (NS_FAILED(mAccService->GetAccessibleFor(targetNode, getter_AddRefs(accessible))))
      return NS_OK;

#ifdef MOZ_XUL
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
#endif

    nsAutoString eventType;
    aEvent->GetType(eventType);

#ifndef MOZ_ACCESSIBILITY_ATK
#ifdef MOZ_XUL
    // tree event
    if (treeItemAccessible && 
        (eventType.EqualsIgnoreCase("DOMMenuItemActive") || eventType.EqualsIgnoreCase("select"))) {
      HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, treeItemAccessible, nsnull);
      return NS_OK;
    }
#endif

    if (eventType.EqualsIgnoreCase("focus") || eventType.EqualsIgnoreCase("DOMMenuItemActive")) { 
      if (optionTargetNode &&
          NS_SUCCEEDED(mAccService->GetAccessibleFor(optionTargetNode, getter_AddRefs(accessible)))) {
        if (eventType.EqualsIgnoreCase("focus")) {
          nsCOMPtr<nsIAccessible> selectAccessible;
          mAccService->GetAccessibleFor(targetNode, getter_AddRefs(selectAccessible));
          if (selectAccessible) {
            FireAccessibleFocusEvent(selectAccessible, targetNode);
          }
        }
        FireAccessibleFocusEvent(accessible, optionTargetNode);
      }
      else
        FireAccessibleFocusEvent(accessible, targetNode);
    }
    else if (eventType.EqualsIgnoreCase("ListitemStateChange")) {
      HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
      FireAccessibleFocusEvent(accessible, optionTargetNode);
    }
    else if (eventType.EqualsIgnoreCase("CheckboxStateChange")) { 
      HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
    }
    else if (eventType.EqualsIgnoreCase("RadioStateChange") ) {
      // first the XUL radio buttons
      if (targetNode &&
          NS_SUCCEEDED(mAccService->GetAccessibleFor(targetNode, getter_AddRefs(accessible)))) {
        HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
        FireAccessibleFocusEvent(accessible, targetNode);
      }
      else { // for the html radio buttons -- apparently the focus code just works. :-)
        HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, nsnull);
      }
    }
    else if (eventType.EqualsIgnoreCase("DOMMenuBarActive")) 
      HandleEvent(nsIAccessibleEventListener::EVENT_MENUSTART, accessible, nsnull);
    else if (eventType.EqualsIgnoreCase("DOMMenuBarInactive")) {
      HandleEvent(nsIAccessibleEventListener::EVENT_MENUEND, accessible, nsnull);
      GetAccFocused(getter_AddRefs(accessible));
      if (accessible) {
        accessible->AccGetDOMNode(getter_AddRefs(targetNode));
        FireAccessibleFocusEvent(accessible, targetNode);
      }
    }
    else {
      // Menu popup events
      PRUint32 menuEvent = 0;
      if (eventType.EqualsIgnoreCase("popupshowing"))
        menuEvent = nsIAccessibleEventListener::EVENT_MENUPOPUPSTART;
      else if (eventType.EqualsIgnoreCase("popuphiding"))
        menuEvent = nsIAccessibleEventListener::EVENT_MENUPOPUPEND;
      if (menuEvent) {
        PRUint32 role = ROLE_NOTHING;
        accessible->GetAccRole(&role);
        if (role == ROLE_MENUPOPUP)
          HandleEvent(menuEvent, accessible, nsnull);
      }
    }
#else
    AtkStateChange stateData;
    if (eventType.EqualsIgnoreCase("focus") || eventType.EqualsIgnoreCase("DOMMenuItemActive")) {
      if (treeItemAccessible) // use focused treeitem
        HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, treeItemAccessible, nsnull);
      else if (anchorElement) {
        nsCOMPtr<nsIAccessibleHyperText> hyperText(do_QueryInterface(accessible));
        if (hyperText) {
          PRInt32 selectedLink;
          hyperText->GetSelectedLinkIndex(&selectedLink);
          HandleEvent(nsIAccessibleEventListener::EVENT_ATK_LINK_SELECTED, accessible, &selectedLink);
        }
      }
      else if (optionTargetNode && // use focused option
          NS_SUCCEEDED(mAccService->GetAccessibleFor(optionTargetNode, getter_AddRefs(accessible))))
        FireAccessibleFocusEvent(accessible, optionTargetNode);
      else
        FireAccessibleFocusEvent(accessible, targetNode);
    }
    else if (eventType.EqualsIgnoreCase("select")) {
      if (treeBox && treeIndex >= 0) // it's a XUL <tree>
        // use EVENT_FOCUS instead of EVENT_ATK_SELECTION_CHANGE
        HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, treeItemAccessible, nsnull);
    }
    else if (eventType.EqualsIgnoreCase("ListitemStateChange")) // it's a XUL <listbox>
      HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, accessible, nsnull);
    else if (eventType.EqualsIgnoreCase("CheckboxStateChange") || // it's a XUL <checkbox>
             eventType.EqualsIgnoreCase("RadioStateChange")) { // it's a XUL <radio>
      accessible->GetAccState(&stateData.state);
      stateData.enable = (stateData.state & STATE_CHECKED) != 0;
      stateData.state = STATE_CHECKED;
      HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, accessible, &stateData);
      if (eventType.EqualsIgnoreCase("RadioStateChange")) {
        FireAccessibleFocusEvent(accessible, targetNode);
      }
    }
    else if (eventType.EqualsIgnoreCase("popupshowing")) 
      FireAccessibleFocusEvent(accessible, targetNode);
    else if (eventType.EqualsIgnoreCase("popuphiding")) 
      FireAccessibleFocusEvent(accessible, targetNode);
#endif
  }
  return NS_OK;
}

void nsRootAccessible::GetTargetNode(nsIDOMEvent *aEvent, nsIDOMNode **aTargetNode)
{
  *aTargetNode = nsnull;

  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  if (nsevent) {
    nsevent->GetOriginalTarget(getter_AddRefs(domEventTarget));
    nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(domEventTarget));
    NS_IF_ADDREF(*aTargetNode = targetNode);
  }
}

NS_IMETHODIMP nsRootAccessible::HandleEvent(PRUint32 aEvent, nsIAccessible *aTarget, void * aData)
{
  if (mListener)
    mListener->HandleEvent(aEvent, aTarget, aData);
  return NS_OK;
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
  return NS_OK;   // Ignore form change events in MSAA
}

// gets Select events when text is selected in a textarea or input
NS_IMETHODIMP nsRootAccessible::Select(nsIDOMEvent* aEvent) 
{
  return HandleEvent(aEvent);
}

// gets Input events when text is entered or deleted in a textarea or input
NS_IMETHODIMP nsRootAccessible::Input(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
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

void nsRootAccessible::RemoveContentDocListeners()
{
  // Remove listeners associated with content documents

  // Remove web progress listener
  if (mWebProgress) {
    mWebProgress->RemoveProgressListener(this);
    mWebProgress = nsnull;
  }

  // Remove scroll position listener
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mPresShell));
  RemoveScrollListener(presShell);
}

void nsRootAccessible::AddContentDocListeners()
{
  // 1) Set up scroll position listener
  // 2) Set up web progress listener - we need to know 
  //    when page loading is finished
  //    That way we can send the STATE_CHANGE events for 
  //    the MSAA root "pane" object (ROLE_PANE),
  //    and change the STATE_BUSY bit flag
  //    Do this only for top level content documents

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mPresShell));
  if (!presShell)
    return;

  AddScrollListener(presShell);

  nsCOMPtr<nsISupports> container;
  mDocument->GetContainer(getter_AddRefs(container));

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(container));
  if (!docShellTreeItem)
    return;

  // Make sure we're a content docshell
  // We don't want to listen to chrome progress
  PRInt32 itemType;
  docShellTreeItem->GetItemType(&itemType);

  if (itemType != nsIDocShellTreeItem::typeContent)
    return;

  // Make sure we're the top content doc shell 
  // We don't want to listen to iframe progress
  nsCOMPtr<nsIDocShellTreeItem> topOfContentTree;
  docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(topOfContentTree));
  if (topOfContentTree != docShellTreeItem)
    return;
  
  nsCOMPtr<nsIPresContext> context; 
  presShell->GetPresContext(getter_AddRefs(context));
  if (!context)
    return;

  mWebProgress = do_GetInterface(docShellTreeItem);
  if (!mWebProgress)
    return;

  mWebProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_LOCATION | 
                                    nsIWebProgress::NOTIFY_STATE_DOCUMENT);


  mIsNewDocument = PR_TRUE;
  mBusy = eBusyStateLoading;
  PRBool isLoading;

  mWebProgress->GetIsLoadingDocument(&isLoading);
  if (!isLoading) {
    // If already loaded, fire "done loading" event after short timeout
    // If we fired the event here, we'd get reentrancy problems
    // Otherwise it will be fired from OnStateChange when the load is done
    mDocLoadTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mDocLoadTimer) {
      mDocLoadTimer->InitWithFuncCallback(DocLoadCallback, this, 1,
                                          nsITimer::TYPE_ONE_SHOT);
    }
  }
}

void nsRootAccessible::FireDocLoadFinished()
{
  if (mIsNewDocument) {
    mIsNewDocument = PR_FALSE;

    if (mBusy != eBusyStateDone) {
      mBusy = eBusyStateDone;
#ifndef MOZ_ACCESSIBILITY_ATK
      HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, this, nsnull);
#endif
    }
  }
}

NS_IMETHODIMP nsRootAccessible::OnStateChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  if ((aStateFlags & STATE_IS_DOCUMENT) && (aStateFlags & STATE_STOP))
    FireDocLoadFinished();   // Doc is ready!

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

  // We won't fire a "doc finished loading" event on this nsRootAccessible 
  // Instead we fire that on the new nsRootAccessible that is created for the new doc
  mIsNewDocument = PR_FALSE;   // We're a doc that's going away

  if (mBusy != eBusyStateLoading) {
    mBusy = eBusyStateLoading; 
    // Fire a "new doc has started to load" event
#ifndef MOZ_ACCESSIBILITY_ATK
    HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, this, nsnull);
#else
    AtkChildrenChange childrenData;
    childrenData.index = -1;
    childrenData.child = 0;
    childrenData.add = PR_FALSE;
    HandleEvent(nsIAccessibleEventListener::EVENT_REORDER , this, &childrenData);
#endif
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


NS_IMETHODIMP nsRootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::~nsAccessNode()
  if (mDOMNode) {
    RemoveAccessibleEventListener();
    if (--gActiveRootAccessibles == 0)   // Last root accessible. 
      ShutdownAll();
  }

  return nsDocAccessibleWrap::Shutdown();
}

void nsRootAccessible::ShutdownAll()
{
  // Turn accessibility support off and destroy all objects rather 
  // than waiting until shutdown otherwise there will be crashes when 
  // releases occur in modules that are no longer loaded

  NS_IF_RELEASE(gLastFocusedNode);
  NS_IF_RELEASE(nsAccessible::gStringBundle);
  NS_IF_RELEASE(nsAccessible::gKeyStringBundle);

  // Shutdown accessibility service
  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  if (accService) {
    accService->Shutdown();
  }
}

