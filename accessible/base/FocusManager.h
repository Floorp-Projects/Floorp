/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_FocusManager_h_
#define mozilla_a11y_FocusManager_h_

#include "mozilla/RefPtr.h"

class nsINode;
class nsISupports;

namespace mozilla {
namespace dom {
class Document;
}

namespace a11y {

class AccEvent;
class LocalAccessible;
class DocAccessible;

/**
 * Manage the accessible focus. Used to fire and process accessible events.
 */
class FocusManager {
 public:
  virtual ~FocusManager();

  /**
   * Return a focused accessible.
   */
  LocalAccessible* FocusedAccessible() const;

  /**
   * Return true if given accessible is focused.
   */
  bool IsFocused(const LocalAccessible* aAccessible) const;

  /**
   * Return true if the given accessible is an active item, i.e. an item that
   * is current within the active widget.
   */
  inline bool IsActiveItem(const LocalAccessible* aAccessible) {
    return aAccessible == mActiveItem;
  }

  /**
   * Return DOM node having DOM focus.
   */
  nsINode* FocusedDOMNode() const;

  /**
   * Return true if given DOM node has DOM focus.
   */
  inline bool HasDOMFocus(const nsINode* aNode) const {
    return aNode == FocusedDOMNode();
  }

  /**
   * Return true if focused accessible is within the given container.
   */
  bool IsFocusWithin(const LocalAccessible* aContainer) const;

  /**
   * Return whether the given accessible is focused or contains the focus or
   * contained by focused accessible.
   */
  enum FocusDisposition { eNone, eFocused, eContainsFocus, eContainedByFocus };
  FocusDisposition IsInOrContainsFocus(
      const LocalAccessible* aAccessible) const;

  /**
   * Return true if the given accessible was the last accessible focused.
   * This is useful to detect the case where the last focused accessible was
   * removed before something else was focused. This can happen in one of two
   * ways:
   * 1. The DOM focus was removed. DOM doesn't fire a blur event when this
   *    happens; see bug 559561.
   * 2. The accessibility focus was an active item (e.g. aria-activedescendant)
   *    and that item was removed.
   */
  bool WasLastFocused(const LocalAccessible* aAccessible) const;

  //////////////////////////////////////////////////////////////////////////////
  // Focus notifications and processing (don't use until you know what you do).

  /**
   * Called when DOM focus event is fired.
   */
  void NotifyOfDOMFocus(nsISupports* aTarget);

  /**
   * Called when DOM blur event is fired.
   */
  void NotifyOfDOMBlur(nsISupports* aTarget);

  /**
   * Called when active item is changed. Note: must be called when accessible
   * tree is up to date.
   */
  void ActiveItemChanged(LocalAccessible* aItem, bool aCheckIfActive = true);

  /**
   * Dispatch delayed focus event for the current focus accessible.
   */
  void ForceFocusEvent();

  /**
   * Dispatch delayed focus event for the given target.
   */
  void DispatchFocusEvent(DocAccessible* aDocument, LocalAccessible* aTarget);

  /**
   * Process DOM focus notification.
   */
  void ProcessDOMFocus(nsINode* aTarget);

  /**
   * Process the delayed accessible event.
   * do.
   */
  void ProcessFocusEvent(AccEvent* aEvent);

 protected:
  FocusManager();

 private:
  FocusManager(const FocusManager&);
  FocusManager& operator=(const FocusManager&);

  /**
   * Return DOM document having DOM focus.
   */
  dom::Document* FocusedDOMDocument() const;

 private:
  RefPtr<LocalAccessible> mActiveItem;
  RefPtr<LocalAccessible> mLastFocus;
  RefPtr<LocalAccessible> mActiveARIAMenubar;
};

}  // namespace a11y
}  // namespace mozilla

#endif
