/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This file provides the implementation for xul popup listener which
  tracks xul popups and context menus
 */

#include "nsXULPopupListener.h"
#include "XULButtonElement.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsXULPopupManager.h"
#include "nsIScriptContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsServiceManagerUtils.h"
#include "nsLayoutUtils.h"
#include "mozilla/ReflowInput.h"
#include "nsIObjectLoadingContent.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"  // for Event
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/MouseEventBinding.h"

// for event firing in context menus
#include "nsPresContext.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"
#include "nsViewManager.h"
#include "nsError.h"

using namespace mozilla;
using namespace mozilla::dom;

// on win32 and os/2, context menus come up on mouse up. On other platforms,
// they appear on mouse down. Certain bits of code care about this difference.
#if defined(XP_WIN)
#  define NS_CONTEXT_MENU_IS_MOUSEUP 1
#endif

nsXULPopupListener::nsXULPopupListener(mozilla::dom::Element* aElement,
                                       bool aIsContext)
    : mElement(aElement), mPopupContent(nullptr), mIsContext(aIsContext) {}

nsXULPopupListener::~nsXULPopupListener(void) { ClosePopup(); }

NS_IMPL_CYCLE_COLLECTION(nsXULPopupListener, mElement, mPopupContent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULPopupListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULPopupListener)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsXULPopupListener)
  // If the owner, mElement, can be skipped, so can we.
  if (tmp->mElement) {
    return mozilla::dom::FragmentOrElement::CanSkip(tmp->mElement, true);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsXULPopupListener)
  if (tmp->mElement) {
    return mozilla::dom::FragmentOrElement::CanSkipInCC(tmp->mElement);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsXULPopupListener)
  if (tmp->mElement) {
    return mozilla::dom::FragmentOrElement::CanSkipThis(tmp->mElement);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULPopupListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////
// nsIDOMEventListener

nsresult nsXULPopupListener::HandleEvent(Event* aEvent) {
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (!((eventType.EqualsLiteral("mousedown") && !mIsContext) ||
        (eventType.EqualsLiteral("contextmenu") && mIsContext)))
    return NS_OK;

  MouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (!mouseEvent) {
    // non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Get the node that was clicked on.
  nsCOMPtr<nsIContent> targetContent =
      nsIContent::FromEventTargetOrNull(mouseEvent->GetTarget());
  if (!targetContent) {
    return NS_OK;
  }

  if (nsIContent* content =
          nsIContent::FromEventTargetOrNull(mouseEvent->GetOriginalTarget())) {
    if (EventStateManager::IsTopLevelRemoteTarget(content)) {
      return NS_OK;
    }
  }

  bool preventDefault = mouseEvent->DefaultPrevented();
  if (preventDefault && mIsContext) {
    // Someone called preventDefault on a context menu.
    // Let's make sure they are allowed to do so.
    bool eventEnabled =
        Preferences::GetBool("dom.event.contextmenu.enabled", true);
    if (!eventEnabled) {
      // The user wants his contextmenus.  Let's make sure that this is a
      // website and not chrome since there could be places in chrome which
      // don't want contextmenus.
      if (!targetContent->NodePrincipal()->IsSystemPrincipal()) {
        // This isn't chrome.  Cancel the preventDefault() and
        // let the event go forth.
        preventDefault = false;
      }
    }
  }

  if (preventDefault) {
    // someone called preventDefault. bail.
    return NS_OK;
  }

  // prevent popups on menu and menuitems as they handle their own popups
  // This was added for bug 96920.
  // If a menu item child was clicked on that leads to a popup needing
  // to show, we know (guaranteed) that we're dealing with a menu or
  // submenu of an already-showing popup.  We don't need to do anything at all.
  if (!mIsContext &&
      targetContent->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menuitem)) {
    return NS_OK;
  }

  if (!mIsContext && mouseEvent->Button() != 0) {
    // Only open popups when the left mouse button is down.
    return NS_OK;
  }

  // Open the popup. LaunchPopup will call StopPropagation and PreventDefault
  // in the right situations.
  LaunchPopup(mouseEvent);

  return NS_OK;
}

// ClosePopup
//
// Do everything needed to shut down the popup.
//
// NOTE: This routine is safe to call even if the popup is already closed.
//
void nsXULPopupListener::ClosePopup() {
  if (mPopupContent) {
    // this is called when the listener is going away, so make sure that the
    // popup is hidden. Use asynchronous hiding just to be safe so we don't
    // fire events during destruction.
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      pm->HidePopup(mPopupContent,
                    {HidePopupOption::DeselectMenu, HidePopupOption::Async});
    mPopupContent = nullptr;  // release the popup
  }
}  // ClosePopup

static already_AddRefed<Element> GetImmediateChild(nsIContent* aContent,
                                                   nsAtom* aTag) {
  for (nsIContent* child = aContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsXULElement(aTag)) {
      RefPtr<Element> ret = child->AsElement();
      return ret.forget();
    }
  }

  return nullptr;
}

//
// LaunchPopup
//
// Given the element on which the event was triggered and the mouse locations in
// Client and widget coordinates, popup a new window showing the appropriate
// content.
//
// aTargetContent is the target of the mouse event aEvent that triggered the
// popup. mElement is the element that the popup menu is attached to.
// aTargetContent may be equal to mElement or it may be a descendant.
//
// This looks for an attribute on |mElement| of the appropriate popup type
// (popup, context) and uses that attribute's value as an ID for
// the popup content in the document.
//
nsresult nsXULPopupListener::LaunchPopup(MouseEvent* aEvent) {
  nsresult rv = NS_OK;

  nsAutoString identifier;
  nsAtom* type = mIsContext ? nsGkAtoms::context : nsGkAtoms::popup;
  bool hasPopupAttr = mElement->GetAttr(type, identifier);

  if (identifier.IsEmpty()) {
    hasPopupAttr =
        mElement->GetAttr(mIsContext ? nsGkAtoms::contextmenu : nsGkAtoms::menu,
                          identifier) ||
        hasPopupAttr;
  }

  if (hasPopupAttr) {
    aEvent->StopPropagation();
    aEvent->PreventDefault();
  }

  if (identifier.IsEmpty()) return rv;

  // Try to find the popup content and the document.
  nsCOMPtr<Document> document = mElement->GetComposedDoc();
  if (!document) {
    NS_WARNING("No document!");
    return NS_ERROR_FAILURE;
  }

  // Handle the _child case for popups and context menus
  RefPtr<Element> popup;
  if (identifier.EqualsLiteral("_child")) {
    popup = GetImmediateChild(mElement, nsGkAtoms::menupopup);
  } else if (!mElement->IsInUncomposedDoc() ||
             !(popup = document->GetElementById(identifier))) {
    // XXXsmaug Should we try to use ShadowRoot::GetElementById in case
    //          mElement is in shadow DOM?
    //
    // Use getElementById to obtain the popup content and gracefully fail if
    // we didn't find any popup content in the document.
    NS_WARNING("GetElementById had some kind of spasm.");
    return rv;
  }

  // return if no popup was found or the popup is the element itself.
  if (!popup || popup == mElement) {
    return NS_OK;
  }

  // Submenus can't be used as context menus or popups, bug 288763.
  // Similar code also in nsXULTooltipListener::GetTooltipFor.
  if (auto* button = XULButtonElement::FromNodeOrNull(popup->GetParent())) {
    if (button->IsMenu()) {
      return NS_OK;
    }
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) return NS_OK;

  // For left-clicks, if the popup has an position attribute, or both the
  // popupanchor and popupalign attributes are used, anchor the popup to the
  // element, otherwise just open it at the screen position where the mouse
  // was clicked. Context menus always open at the mouse position.
  mPopupContent = popup;
  if (!mIsContext && (mPopupContent->HasAttr(nsGkAtoms::position) ||
                      (mPopupContent->HasAttr(nsGkAtoms::popupanchor) &&
                       mPopupContent->HasAttr(nsGkAtoms::popupalign)))) {
    pm->ShowPopup(mPopupContent, mElement, u""_ns, 0, 0, false, true, false,
                  aEvent);
  } else {
    CSSIntPoint pos = aEvent->ScreenPoint(CallerType::System);
    pm->ShowPopupAtScreen(mPopupContent, pos.x, pos.y, mIsContext, aEvent);
  }

  return NS_OK;
}
