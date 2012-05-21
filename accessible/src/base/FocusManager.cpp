/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FocusManager.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "RootAccessible.h"

#include "nsEventStateManager.h"
#include "nsFocusManager.h"

namespace dom = mozilla::dom;
using namespace mozilla::a11y;

FocusManager::FocusManager()
{
}

FocusManager::~FocusManager()
{
}

nsAccessible*
FocusManager::FocusedAccessible() const
{
  if (mActiveItem)
    return mActiveItem;

  nsINode* focusedNode = FocusedDOMNode();
  if (focusedNode) {
    nsDocAccessible* doc = 
      GetAccService()->GetDocAccessible(focusedNode->OwnerDoc());
    return doc ? doc->GetAccessibleOrContainer(focusedNode) : nsnull;
  }

  return nsnull;
}

bool
FocusManager::IsFocused(const nsAccessible* aAccessible) const
{
  if (mActiveItem)
    return mActiveItem == aAccessible;

  nsINode* focusedNode = FocusedDOMNode();
  if (focusedNode) {
    // XXX: Before getting an accessible for node having a DOM focus make sure
    // they belong to the same document because it can trigger unwanted document
    // accessible creation for temporary about:blank document. Without this
    // peculiarity we would end up with plain implementation based on
    // FocusedAccessible() method call. Make sure this issue is fixed in
    // bug 638465.
    if (focusedNode->OwnerDoc() == aAccessible->GetNode()->OwnerDoc()) {
      nsDocAccessible* doc = 
        GetAccService()->GetDocAccessible(focusedNode->OwnerDoc());
      return aAccessible ==
	(doc ? doc->GetAccessibleOrContainer(focusedNode) : nsnull);
    }
  }
  return false;
}

bool
FocusManager::IsFocusWithin(const nsAccessible* aContainer) const
{
  nsAccessible* child = FocusedAccessible();
  while (child) {
    if (child == aContainer)
      return true;

    child = child->Parent();
  }
  return false;
}

FocusManager::FocusDisposition
FocusManager::IsInOrContainsFocus(const nsAccessible* aAccessible) const
{
  nsAccessible* focus = FocusedAccessible();
  if (!focus)
    return eNone;

  // If focused.
  if (focus == aAccessible)
    return eFocused;

  // If contains the focus.
  nsAccessible* child = focus->Parent();
  while (child) {
    if (child == aAccessible)
      return eContainsFocus;

    child = child->Parent();
  }

  // If contained by focus.
  child = aAccessible->Parent();
  while (child) {
    if (child == focus)
      return eContainedByFocus;

    child = child->Parent();
  }

  return eNone;
}

void
FocusManager::NotifyOfDOMFocus(nsISupports* aTarget)
{
  A11YDEBUG_FOCUS_NOTIFICATION_SUPPORTSTARGET("DOM focus", "DOM focus target",
                                              aTarget)

  mActiveItem = nsnull;

  nsCOMPtr<nsINode> targetNode(do_QueryInterface(aTarget));
  if (targetNode) {
    nsDocAccessible* document =
      GetAccService()->GetDocAccessible(targetNode->OwnerDoc());
    if (document) {
      // Set selection listener for focused element.
      if (targetNode->IsElement()) {
        RootAccessible* root = document->RootAccessible();
        nsCaretAccessible* caretAcc = root->GetCaretAccessible();
        caretAcc->SetControlSelectionListener(targetNode->AsElement());
      }

      document->HandleNotification<FocusManager, nsINode>
        (this, &FocusManager::ProcessDOMFocus, targetNode);
    }
  }
}

void
FocusManager::NotifyOfDOMBlur(nsISupports* aTarget)
{
  A11YDEBUG_FOCUS_NOTIFICATION_SUPPORTSTARGET("DOM blur", "DOM blur target",
                                              aTarget)

  mActiveItem = nsnull;

  // If DOM document stays focused then fire accessible focus event to process
  // the case when no element within this DOM document will be focused.
  nsCOMPtr<nsINode> targetNode(do_QueryInterface(aTarget));
  if (targetNode && targetNode->OwnerDoc() == FocusedDOMDocument()) {
    nsIDocument* DOMDoc = targetNode->OwnerDoc();
    nsDocAccessible* document =
      GetAccService()->GetDocAccessible(DOMDoc);
    if (document) {
      document->HandleNotification<FocusManager, nsINode>
        (this, &FocusManager::ProcessDOMFocus, DOMDoc);
    }
  }
}

void
FocusManager::ActiveItemChanged(nsAccessible* aItem, bool aCheckIfActive)
{
  A11YDEBUG_FOCUS_NOTIFICATION_ACCTARGET("active item changed",
                                         "Active item", aItem)

  // Nothing changed, happens for XUL trees and HTML selects.
  if (aItem && aItem == mActiveItem)
    return;

  mActiveItem = nsnull;

  if (aItem && aCheckIfActive) {
    nsAccessible* widget = aItem->ContainerWidget();
    A11YDEBUG_FOCUS_LOG_WIDGET("Active item widget", widget)
    if (!widget || !widget->IsActiveWidget() || !widget->AreItemsOperable())
      return;
  }
  mActiveItem = aItem;

  // If active item is changed then fire accessible focus event on it, otherwise
  // if there's no an active item then fire focus event to accessible having
  // DOM focus.
  nsAccessible* target = FocusedAccessible();
  if (target)
    DispatchFocusEvent(target->Document(), target);
}

void
FocusManager::ForceFocusEvent()
{
  nsINode* focusedNode = FocusedDOMNode();
  if (focusedNode) {
    nsDocAccessible* document =
      GetAccService()->GetDocAccessible(focusedNode->OwnerDoc());
    if (document) {
      document->HandleNotification<FocusManager, nsINode>
        (this, &FocusManager::ProcessDOMFocus, focusedNode);
    }
  }
}

void
FocusManager::DispatchFocusEvent(nsDocAccessible* aDocument,
                                 nsAccessible* aTarget)
{
  NS_PRECONDITION(aDocument, "No document for focused accessible!");
  if (aDocument) {
    nsRefPtr<AccEvent> event =
      new AccEvent(nsIAccessibleEvent::EVENT_FOCUS, aTarget,
                   eAutoDetect, AccEvent::eCoalesceOfSameType);
    aDocument->FireDelayedAccessibleEvent(event);

    A11YDEBUG_FOCUS_LOG_ACCTARGET("Focus notification", aTarget)
  }
}

void
FocusManager::ProcessDOMFocus(nsINode* aTarget)
{
  A11YDEBUG_FOCUS_NOTIFICATION_DOMTARGET("Process DOM focus",
                                         "Notification target", aTarget)

  nsDocAccessible* document =
    GetAccService()->GetDocAccessible(aTarget->OwnerDoc());

  nsAccessible* target = document->GetAccessibleOrContainer(aTarget);
  if (target && document) {
    // Check if still focused. Otherwise we can end up with storing the active
    // item for control that isn't focused anymore.
    nsAccessible* DOMFocus =
      document->GetAccessibleOrContainer(FocusedDOMNode());
    if (target != DOMFocus)
      return;

    nsAccessible* activeItem = target->CurrentItem();
    if (activeItem) {
      mActiveItem = activeItem;
      target = activeItem;
    }

    DispatchFocusEvent(document, target);
  }
}

void
FocusManager::ProcessFocusEvent(AccEvent* aEvent)
{
  NS_PRECONDITION(aEvent->GetEventType() == nsIAccessibleEvent::EVENT_FOCUS,
                  "Focus event is expected!");

  EIsFromUserInput fromUserInputFlag = aEvent->IsFromUserInput() ?
    eFromUserInput : eNoUserInput;

  // Emit focus event if event target is the active item. Otherwise then check
  // if it's still focused and then update active item and emit focus event.
  nsAccessible* target = aEvent->GetAccessible();
  if (target != mActiveItem) {

    // Check if still focused. Otherwise we can end up with storing the active
    // item for control that isn't focused anymore.
    nsDocAccessible* document = aEvent->GetDocAccessible();
    nsAccessible* DOMFocus = document->GetAccessibleOrContainer(FocusedDOMNode());

    if (target != DOMFocus)
      return;

    nsAccessible* activeItem = target->CurrentItem();
    if (activeItem) {
      mActiveItem = activeItem;
      target = activeItem;
    }
  }

  // Fire menu start/end events for ARIA menus.
  if (target->ARIARole() == roles::MENUITEM) {
    // The focus was moved into menu.
    nsAccessible* ARIAMenubar =
      nsAccUtils::GetAncestorWithRole(target, roles::MENUBAR);

    if (ARIAMenubar != mActiveARIAMenubar) {
      // Leaving ARIA menu. Fire menu_end event on current menubar.
      if (mActiveARIAMenubar) {
        nsRefPtr<AccEvent> menuEndEvent =
          new AccEvent(nsIAccessibleEvent::EVENT_MENU_END, mActiveARIAMenubar,
                       fromUserInputFlag);
        nsEventShell::FireEvent(menuEndEvent);
      }

      mActiveARIAMenubar = ARIAMenubar;

      // Entering ARIA menu. Fire menu_start event.
      if (mActiveARIAMenubar) {
        nsRefPtr<AccEvent> menuStartEvent =
          new AccEvent(nsIAccessibleEvent::EVENT_MENU_START,
                       mActiveARIAMenubar, fromUserInputFlag);
        nsEventShell::FireEvent(menuStartEvent);
      }
    }
  } else if (mActiveARIAMenubar) {
    // Focus left a menu. Fire menu_end event.
    nsRefPtr<AccEvent> menuEndEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_MENU_END, mActiveARIAMenubar,
                   fromUserInputFlag);
    nsEventShell::FireEvent(menuEndEvent);

    mActiveARIAMenubar = nsnull;
  }

  A11YDEBUG_FOCUS_NOTIFICATION_ACCTARGET("FIRE FOCUS EVENT", "Focus target",
                                         target)

  nsRefPtr<AccEvent> focusEvent =
    new AccEvent(nsIAccessibleEvent::EVENT_FOCUS, target, fromUserInputFlag);
  nsEventShell::FireEvent(focusEvent);

  // Fire scrolling_start event when the document receives the focus if it has
  // an anchor jump. If an accessible within the document receive the focus
  // then null out the anchor jump because it no longer applies.
  nsDocAccessible* targetDocument = target->Document();
  nsAccessible* anchorJump = targetDocument->AnchorJump();
  if (anchorJump) {
    if (target == targetDocument) {
      // XXX: bug 625699, note in some cases the node could go away before we
      // we receive focus event, for example if the node is removed from DOM.
      nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_SCROLLING_START,
                              anchorJump, fromUserInputFlag);
    }
    targetDocument->SetAnchorJump(nsnull);
  }
}

nsINode*
FocusManager::FocusedDOMNode() const
{
  nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
  nsIContent* focusedElm = DOMFocusManager->GetFocusedContent();

  // No focus on remote target elements like xul:browser having DOM focus and
  // residing in chrome process because it means an element in content process
  // keeps the focus.
  if (focusedElm) {
    if (nsEventStateManager::IsRemoteTarget(focusedElm))
      return nsnull;
    return focusedElm;
  }

  // Otherwise the focus can be on DOM document.
  nsCOMPtr<nsIDOMWindow> focusedWnd;
  DOMFocusManager->GetFocusedWindow(getter_AddRefs(focusedWnd));
  if (focusedWnd) {
    nsCOMPtr<nsIDOMDocument> DOMDoc;
    focusedWnd->GetDocument(getter_AddRefs(DOMDoc));
    nsCOMPtr<nsIDocument> DOMDocNode(do_QueryInterface(DOMDoc));
    return DOMDocNode;
  }
  return nsnull;
}

nsIDocument*
FocusManager::FocusedDOMDocument() const
{
  nsINode* focusedNode = FocusedDOMNode();
  return focusedNode ? focusedNode->OwnerDoc() : nsnull;
}
