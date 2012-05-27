/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessible_h__
#define mozilla_a11y_RootAccessible_h__

#include "nsCaretAccessible.h"
#include "DocAccessibleWrap.h"


#include "nsHashtable.h"
#include "nsCaretAccessible.h"
#include "nsIDocument.h"
#include "nsIDOMEventListener.h"

class nsXULTreeAccessible;
class Relation;

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

  // nsAccessible
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName);
  virtual Relation RelationByType(PRUint32 aType);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // RootAccessible
  nsCaretAccessible* GetCaretAccessible();

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
  void HandlePopupShownEvent(nsAccessible* aAccessible);

  /*
   * Process "popuphiding" event. Used by HandleEvent().
   */
  void HandlePopupHidingEvent(nsINode* aNode);

#ifdef MOZ_XUL
    void HandleTreeRowCountChangedEvent(nsIDOMEvent* aEvent,
                                        nsXULTreeAccessible* aAccessible);
    void HandleTreeInvalidatedEvent(nsIDOMEvent* aEvent,
                                    nsXULTreeAccessible* aAccessible);

    PRUint32 GetChromeFlags();
#endif

    nsRefPtr<nsCaretAccessible> mCaretAccessible;
};

} // namespace a11y
} // namespace mozilla

inline mozilla::a11y::RootAccessible*
nsAccessible::AsRoot()
{
  return mFlags & eRootAccessible ?
    static_cast<mozilla::a11y::RootAccessible*>(this) : nsnull;
}

#endif
