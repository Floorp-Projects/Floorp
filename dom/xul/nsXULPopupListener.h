/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the popup listener implementation for popup menus and context menus.
 */

#ifndef nsXULPopupListener_h___
#define nsXULPopupListener_h___

#include "nsCOMPtr.h"

#include "nsIDOMEventListener.h"
#include "nsCycleCollectionParticipant.h"

class nsIContent;

namespace mozilla {
namespace dom {
class Element;
class MouseEvent;
}  // namespace dom
}  // namespace mozilla

class nsXULPopupListener : public nsIDOMEventListener {
 public:
  // aElement is the element that the popup is attached to. If aIsContext is
  // false, the popup opens on left click on aElement or a descendant. If
  // aIsContext is true, the popup is a context menu which opens on a
  // context menu event.
  nsXULPopupListener(mozilla::dom::Element* aElement, bool aIsContext);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS(nsXULPopupListener)
  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  virtual ~nsXULPopupListener(void);

  // open the popup. aEvent is the event that triggered the popup such as
  // a mouse click and aTargetContent is the target of this event.
  virtual nsresult LaunchPopup(mozilla::dom::MouseEvent* aEvent);

  // close the popup when the listener goes away
  virtual void ClosePopup();

 private:
#ifndef NS_CONTEXT_MENU_IS_MOUSEUP
  // When a context menu is opened, focus the target of the contextmenu event.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  FireFocusOnTargetContent(nsIContent* aTargetContent, bool aIsTouch);
#endif

  // |mElement| is the node to which this listener is attached.
  RefPtr<mozilla::dom::Element> mElement;

  // The popup that is getting shown on top of mElement.
  RefPtr<mozilla::dom::Element> mPopupContent;

  // true if a context popup
  bool mIsContext;
};

#endif  // nsXULPopupListener_h___
