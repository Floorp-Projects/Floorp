/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// NOTE: alphabetically ordered
#include "nsAccessibilityService.h"
#include "nsAccessibleEventData.h"
#include "nsCaretAccessible.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIAccessibleCaret.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScrollableView.h"
#include "nsIServiceManager.h"
#include "nsIViewManager.h"
#include "nsLayoutAtoms.h"
#include "nsPIDOMWindow.h"
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
#include "nsIDOMHTMLInputElement.h"
#include "nsAccessibleEventData.h"
#include "nsIDOMDocument.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibleHyperText.h"
#endif

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFormListener)
NS_INTERFACE_MAP_END_INHERITING(nsDocAccessible)

NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsDocAccessible)
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsDocAccessible)


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsDocAccessibleWrap(aDOMNode, aShell), 
  mAccService(do_GetService("@mozilla.org/accessibilityService;1"))
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
}

// helpers
/* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetParent(nsIAccessible * *aParent) 
{ 
  *aParent = nsnull;
  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetRole(PRUint32 *aRole) 
{ 
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  *aRole = ROLE_PANE;

  // If it's a <dialog>, use ROLE_DIALOG instead
  nsIContent *rootContent = mDocument->GetRootContent();
  if (rootContent) {
    nsCOMPtr<nsIDOMElement> rootElement(do_QueryInterface(rootContent));
    if (rootElement) {
      nsAutoString name;
      rootElement->GetLocalName(name);
      if (name.EqualsLiteral("dialog")) 
        *aRole = ROLE_DIALOG;
    }
  }

  return NS_OK;
}

void
nsRootAccessible::GetChromeEventHandler(nsIDOMEventTarget **aChromeTarget)
{
  nsCOMPtr<nsIDOMWindow> domWin;
  GetWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(domWin));
  nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
  if (privateDOMWindow) {
    chromeEventHandler = privateDOMWindow->GetChromeEventHandler();
  }

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

  *aChromeTarget = target;
  NS_IF_ADDREF(*aChromeTarget);
}

nsresult nsRootAccessible::AddEventListeners()
{
  // use AddEventListener from the nsIDOMEventTarget interface
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  if (target) { 
    // capture DOM focus events 
    nsresult rv = target->AddEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // capture Form change events 
    rv = target->AddEventListener(NS_LITERAL_STRING("select"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // capture ValueChange events (fired whenever value changes, immediately after, whether focus moves or not)
    rv = target->AddEventListener(NS_LITERAL_STRING("ValueChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // add ourself as a CheckboxStateChange listener (custom event fired in nsHTMLInputElement.cpp)
    rv = target->AddEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

    // add ourself as a RadioStateChange Listener ( custom event fired in in nsHTMLInputElement.cpp  & radio.xml)
    rv = target->AddEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
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
  }

  GetChromeEventHandler(getter_AddRefs(target));
  NS_ASSERTION(target, "No chrome event handler for document");
  if (target) {   
    // onunload doesn't fire unless we use chrome event handler for target
    target->AddEventListener(NS_LITERAL_STRING("unload"), 
                             NS_STATIC_CAST(nsIDOMXULListener*, this), 
                             PR_TRUE);
  }

  if (!mCaretAccessible)
    mCaretAccessible = new nsCaretAccessible(mDOMNode, mWeakShell, this);

  return nsDocAccessible::AddEventListeners();
}

nsresult nsRootAccessible::RemoveEventListeners()
{
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  if (target) { 
    target->RemoveEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("select"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("ValueChange"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("popupshowing"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("popuphiding"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuItemActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
  }

  GetChromeEventHandler(getter_AddRefs(target));
  if (target) {
    target->RemoveEventListener(NS_LITERAL_STRING("unload"), 
                                NS_STATIC_CAST(nsIDOMXULListener*, this), 
                                PR_TRUE);
  }

  if (mCaretAccessible) {
    mCaretAccessible->RemoveSelectionListener();
    mCaretAccessible = nsnull;
  }

  mAccService = nsnull;

  return nsDocAccessible::RemoveEventListeners();
}

NS_IMETHODIMP nsRootAccessible::GetCaretAccessible(nsIAccessible **aCaretAccessible)
{
  *aCaretAccessible = nsnull;
  if (mCaretAccessible) {
    CallQueryInterface(mCaretAccessible, aCaretAccessible);
  }

  return NS_OK;
}

void nsRootAccessible::FireAccessibleFocusEvent(nsIAccessible *focusAccessible, nsIDOMNode *focusNode)
{
  if (focusAccessible && focusNode && gLastFocusedNode != focusNode) {
    nsCOMPtr<nsPIAccessible> privateFocusAcc(do_QueryInterface(focusAccessible));
    privateFocusAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, focusAccessible, nsnull);
    NS_IF_RELEASE(gLastFocusedNode);
    PRUint32 role = ROLE_NOTHING;
    focusAccessible->GetRole(&role);
    if (role != ROLE_MENUITEM && role != ROLE_LISTITEM) {
      // It must report all focus events on menu and list items
      gLastFocusedNode = focusNode;
      NS_ADDREF(gLastFocusedNode);
    }
    if (mCaretAccessible)
      mCaretAccessible->AttachNewSelectionListener(focusNode);
  }
}

void nsRootAccessible::GetEventShell(nsIDOMNode *aNode, nsIPresShell **aEventShell)
{
  // XXX aaronl - this is not ideal.
  // We could avoid this whole section and the fallible 
  // doc->GetShellAt(0) by putting the event handler
  // on nsDocAccessible instead.
  // The disadvantage would be that we would be seeing some events
  // for inner documents that we don't care about.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aNode->GetOwnerDocument(getter_AddRefs(domDocument));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDocument));
  if (!doc) {   // This is necessary when the node is the document node
    doc = do_QueryInterface(aNode);
  }
  if (doc) {
    *aEventShell = doc->GetShellAt(0);
    NS_IF_ADDREF(*aEventShell);
  }
}

// --------------- nsIDOMEventListener Methods (3) ------------------------

NS_IMETHODIMP nsRootAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  // optionTargetNode is set to current option for HTML selects
  nsCOMPtr<nsIDOMNode> targetNode, optionTargetNode; 
  GetTargetNode(aEvent, getter_AddRefs(targetNode));
  if (!targetNode)
    return NS_ERROR_FAILURE;

  nsAutoString eventType;
  aEvent->GetType(eventType);
#ifdef DEBUG_aleventhal
  // Very useful for debugging, please leave this here.
  if (eventType.EqualsIgnoreCase("DOMMenuItemActive")) {
    printf("debugging events");
  }
#endif

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

  nsCOMPtr<nsIPresShell> eventShell;
  GetEventShell(targetNode, getter_AddRefs(eventShell));

#ifdef MOZ_ACCESSIBILITY_ATK
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement(do_QueryInterface(targetNode));
  if (anchorElement) {
    nsCOMPtr<nsIDOMNode> blockNode;
    // For ATK, we don't create any individual object for hyperlink, use its parent who has block frame instead
    if (NS_SUCCEEDED(nsAccessible::GetParentBlockNode(eventShell, targetNode, getter_AddRefs(blockNode))))
      targetNode = blockNode;
  }
#endif

  nsCOMPtr<nsIAccessible> accessible;
  if (!eventShell ||
      NS_FAILED(mAccService->GetAccessibleInShell(targetNode, eventShell,
                                                  getter_AddRefs(accessible))))
    return NS_OK;
  
#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item
  PRInt32 treeIndex = -1;
  nsCOMPtr<nsITreeBoxObject> treeBox;
  nsCOMPtr<nsIAccessible> treeItemAccessible;
  nsXULTreeAccessible::GetTreeBoxObject(targetNode, getter_AddRefs(treeBox));
  if (treeBox) {
    nsCOMPtr<nsITreeView> view;
    treeBox->GetView(getter_AddRefs(view));
    if (view) {
      nsCOMPtr<nsITreeSelection> selection;
      view->GetSelection(getter_AddRefs(selection));
      if (selection) {
        selection->GetCurrentIndex(&treeIndex);
        if (treeIndex >= 0) {
          // XXX todo Kyle - fix bug 201922 so that tree is responsible for keeping track
          // of it's own accessibles. Then we'll ask the tree so we can reuse
          // the accessibles already created.
          nsCOMPtr<nsIWeakReference> weakEventShell(do_GetWeakReference(eventShell));
          treeItemAccessible = new nsXULTreeitemAccessible(accessible, targetNode, 
                                                           weakEventShell, treeIndex);
          if (!treeItemAccessible)
            return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }
#endif

  nsCOMPtr<nsPIAccessible> privAcc(do_QueryInterface(accessible));

#ifndef MOZ_ACCESSIBILITY_ATK
#ifdef MOZ_XUL
  // tree event
  if (treeItemAccessible && (eventType.EqualsIgnoreCase("DOMMenuItemActive") ||
      eventType.EqualsIgnoreCase("select"))) {
    privAcc = do_QueryInterface(treeItemAccessible);
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                              treeItemAccessible, nsnull);
    return NS_OK;
  }
#endif
  
  if (eventType.EqualsIgnoreCase("unload")) {
    nsCOMPtr<nsPIAccessibleDocument> privateAccDoc = 
      do_QueryInterface(accessible);
    if (privateAccDoc) {
      privateAccDoc->Destroy();
    }
  }
  else if (eventType.EqualsIgnoreCase("focus") || 
           eventType.EqualsIgnoreCase("DOMMenuItemActive")) { 
    if (optionTargetNode &&
        NS_SUCCEEDED(mAccService->GetAccessibleInShell(optionTargetNode, eventShell,
                                                       getter_AddRefs(accessible)))) {
      if (eventType.EqualsIgnoreCase("focus")) {
        nsCOMPtr<nsIAccessible> selectAccessible;
        mAccService->GetAccessibleInShell(targetNode, eventShell,
                                          getter_AddRefs(selectAccessible));
        if (selectAccessible) {
          FireAccessibleFocusEvent(selectAccessible, targetNode);
        }
      }
      FireAccessibleFocusEvent(accessible, optionTargetNode);
    }
    else
      FireAccessibleFocusEvent(accessible, targetNode);
  }
  else if (eventType.EqualsIgnoreCase("ValueChange")) { 
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, 
                              accessible, nsnull);
  }
  else if (eventType.EqualsIgnoreCase("CheckboxStateChange")) { 
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                              accessible, nsnull);
  }
  else if (eventType.EqualsIgnoreCase("RadioStateChange") ) {
    // first the XUL radio buttons
    if (targetNode &&
        NS_SUCCEEDED(mAccService->GetAccessibleInShell(targetNode, eventShell,
                                                       getter_AddRefs(accessible)))) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                                accessible, nsnull);
      FireAccessibleFocusEvent(accessible, targetNode);
    }
    else { // for the html radio buttons -- apparently the focus code just works. :-)
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                                accessible, nsnull);
    }
  }
  else if (eventType.EqualsIgnoreCase("DOMMenuBarActive")) 
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUSTART, accessible, nsnull);
  else if (eventType.EqualsIgnoreCase("DOMMenuBarInactive")) {
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUEND, accessible, nsnull);
    GetFocusedChild(getter_AddRefs(accessible));
    if (accessible) {
      accessible->GetDOMNode(getter_AddRefs(targetNode));
      FireAccessibleFocusEvent(accessible, targetNode);
    }
  }
  else {
    // Menu popup events
    PRUint32 menuEvent = 0;
    if (eventType.EqualsIgnoreCase("popupshowing"))
      menuEvent = nsIAccessibleEvent::EVENT_MENUPOPUPSTART;
    else if (eventType.EqualsIgnoreCase("popuphiding"))
      menuEvent = nsIAccessibleEvent::EVENT_MENUPOPUPEND;
    if (menuEvent) {
      PRUint32 role = ROLE_NOTHING;
      accessible->GetRole(&role);
      if (role == ROLE_MENUPOPUP)
        privAcc->FireToolkitEvent(menuEvent, accessible, nsnull);
    }
  }
#else
  AtkStateChange stateData;
  if (eventType.EqualsIgnoreCase("unload")) {
    nsCOMPtr<nsPIAccessibleDocument> privateAccDoc = 
      do_QueryInterface(accessible);
    if (privateAccDoc) {
      privateAccDoc->Destroy();
    }
  }
  else if (eventType.EqualsIgnoreCase("focus") || 
      eventType.EqualsIgnoreCase("DOMMenuItemActive")) {
    if (treeItemAccessible) { // use focused treeitem
      privAcc = do_QueryInterface(treeItemAccessible);
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                                treeItemAccessible, nsnull);
    }
    else if (anchorElement) {
      nsCOMPtr<nsIAccessibleHyperText> hyperText(do_QueryInterface(accessible));
      if (hyperText) {
        PRInt32 selectedLink;
        hyperText->GetSelectedLinkIndex(&selectedLink);
        privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_ATK_LINK_SELECTED, accessible, &selectedLink);
      }
    }
    else if (optionTargetNode && // use focused option
        NS_SUCCEEDED(mAccService->GetAccessibleInShell(optionTargetNode, eventShell,
                                                       getter_AddRefs(accessible))))
      FireAccessibleFocusEvent(accessible, optionTargetNode);
    else
      FireAccessibleFocusEvent(accessible, targetNode);
  }
  else if (eventType.EqualsIgnoreCase("select")) {
    if (treeBox && treeIndex >= 0) { // it's a XUL <tree>
      // use EVENT_FOCUS instead of EVENT_ATK_SELECTION_CHANGE
      privAcc = do_QueryInterface(treeItemAccessible);
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                                treeItemAccessible, nsnull);
      }
  }
#if 0
  // XXX todo: value change events for ATK are done with 
  // AtkPropertyChange, PROP_VALUE. Need the old and new value.
  // Not sure how we'll get the old value.
  else if (eventType.EqualsIgnoreCase("ValueChange")) { 
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, 
                              accessible, nsnull);
  }
#endif
  else if (eventType.EqualsIgnoreCase("CheckboxStateChange") || // it's a XUL <checkbox>
           eventType.EqualsIgnoreCase("RadioStateChange")) { // it's a XUL <radio>
    accessible->GetState(&stateData.state);
    stateData.enable = (stateData.state & STATE_CHECKED) != 0;
    stateData.state = STATE_CHECKED;
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, accessible, &stateData);
    if (eventType.EqualsIgnoreCase("RadioStateChange")) {
      FireAccessibleFocusEvent(accessible, targetNode);
    }
  }
  else if (eventType.EqualsIgnoreCase("popupshowing")) {
    FireAccessibleFocusEvent(accessible, targetNode);
  }
  else if (eventType.EqualsIgnoreCase("popuphiding")) {
    //FireAccessibleFocusEvent(accessible, targetNode);
  }
#endif
  return NS_OK;
}

void nsRootAccessible::GetTargetNode(nsIDOMEvent *aEvent, nsIDOMNode **aTargetNode)
{
  *aTargetNode = nsnull;

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  if (nsevent) {
    nsCOMPtr<nsIDOMEventTarget> domEventTarget;
    nsevent->GetOriginalTarget(getter_AddRefs(domEventTarget));
    nsCOMPtr<nsIContent> content(do_QueryInterface(domEventTarget));
    if (!content || content->IsContentOfType(nsIContent::eHTML)) {
      // Kind of a hack. If we're on an HTML element we want to make sure that it wasn't
      // inserted via XBL. In that case we want the "original explicit event target".
      // The difference is not 100% clear from the API docs, 
      // but this combination gets the following important cases correct:
      // 1. Inserted <dialog> buttons like OK, Cancel, Help.
      // 2. XUL menulists and comboboxes.
      // 3. The focused radio button in a group.
      nsevent->GetExplicitOriginalTarget(getter_AddRefs(domEventTarget));
    }
    if (domEventTarget) {
      CallQueryInterface(domEventTarget, aTargetNode);
    }
  }
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
  // deleted text, new text, change in selection for list/combo boxes
  // this may be the event that we have the individual Accessible objects
  // handle themselves -- have list/combos figure out the change in selection
  // have textareas and inputs fire a change of state etc...
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

NS_IMETHODIMP nsRootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::~nsAccessNode()
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }
  mCaretAccessible = nsnull;
  mAccService = nsnull;

  return nsDocAccessibleWrap::Shutdown();
}

