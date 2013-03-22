/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessible_h__
#define mozilla_a11y_RootAccessible_h__

#include "HyperTextAccessible.h"
#include "DocAccessibleWrap.h"

#include "nsIDOMEventListener.h"

class nsIDocument;

namespace mozilla {
namespace a11y {

class RootAccessible : public DocAccessibleWrap,
                       public nsIDOMEventListener
{
  NS_DECL_ISUPPORTS_INHERITED

public:
  RootAccessible(nsIDocument* aDocument, nsIContent* aRootContent,
                 nsIPresShell* aPresShell);
  virtual ~RootAccessible();

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName);
  virtual Relation RelationByType(uint32_t aType);
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();

  // RootAccessible

  /**
   * Notify that the sub document presshell was activated.
   */
  virtual void DocumentActivated(DocAccessible* aDocument);

protected:

  /**
   * Add/remove DOM event listeners.
   */
  virtual nsresult AddEventListeners();
  virtual nsresult RemoveEventListeners();

  /**
   * Process the DOM event.
   */
  void ProcessDOMEvent(nsIDOMEvent* aEvent);

  /**
   * Process "popupshown" event. Used by HandleEvent().
   */
  void HandlePopupShownEvent(Accessible* aAccessible);

  /*
   * Process "popuphiding" event. Used by HandleEvent().
   */
  void HandlePopupHidingEvent(nsINode* aNode);

#ifdef MOZ_XUL
    void HandleTreeRowCountChangedEvent(nsIDOMEvent* aEvent,
                                        XULTreeAccessible* aAccessible);
    void HandleTreeInvalidatedEvent(nsIDOMEvent* aEvent,
                                    XULTreeAccessible* aAccessible);

    uint32_t GetChromeFlags();
#endif
};

inline RootAccessible*
Accessible::AsRoot()
{
  return IsRoot() ? static_cast<mozilla::a11y::RootAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif
