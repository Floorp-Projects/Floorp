/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessible_h__
#define mozilla_a11y_RootAccessible_h__

#include "HyperTextAccessible.h"
#include "DocAccessibleWrap.h"

#include "nsIDOMEventListener.h"

namespace mozilla {

class PresShell;

namespace a11y {

/**
 * The node at a root of the accessibility tree. This node originated in the
 * current process. If this is the parent process, RootAccessible is the
 * Accessible for the top-level window. If this is a content process,
 * RootAccessible is a top-level content document in this process, which is
 * either a tab document or an out-of-process iframe.
 */
class RootAccessible : public DocAccessibleWrap, public nsIDOMEventListener {
  NS_DECL_ISUPPORTS_INHERITED

 public:
  RootAccessible(dom::Document* aDocument, PresShell* aPresShell);

  // nsIDOMEventListener
  NS_DECL_NSIDOMEVENTLISTENER

  // LocalAccessible
  virtual void Shutdown() override;
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName) const override;
  virtual Relation RelationByType(RelationType aType) const override;
  virtual uint64_t NativeState() const override;

  // RootAccessible

  /**
   * Notify that the sub document presshell was activated.
   */
  virtual void DocumentActivated(DocAccessible* aDocument);

  /**
   * Return the primary remote top level document if any.
   */
  RemoteAccessible* GetPrimaryRemoteTopLevelContentDoc() const;

 protected:
  virtual ~RootAccessible();

  /**
   * Add/remove DOM event listeners.
   */
  virtual nsresult AddEventListeners() override;
  virtual nsresult RemoveEventListeners() override;

  /**
   * Process the DOM event.
   */
  void ProcessDOMEvent(dom::Event* aDOMEvent, nsINode* aTarget);

  /**
   * Process "popupshown" event. Used by HandleEvent().
   */
  void HandlePopupShownEvent(LocalAccessible* aAccessible);

  /*
   * Process "popuphiding" event. Used by HandleEvent().
   */
  void HandlePopupHidingEvent(nsINode* aNode);

  void HandleTreeRowCountChangedEvent(dom::Event* aEvent,
                                      XULTreeAccessible* aAccessible);
  void HandleTreeInvalidatedEvent(dom::Event* aEvent,
                                  XULTreeAccessible* aAccessible);

  uint32_t GetChromeFlags() const;
};

inline RootAccessible* LocalAccessible::AsRoot() {
  return IsRoot() ? static_cast<mozilla::a11y::RootAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
