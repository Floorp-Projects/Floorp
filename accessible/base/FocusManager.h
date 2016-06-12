/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_FocusManager_h_
#define mozilla_a11y_FocusManager_h_

class nsINode;
class nsIDocument;
class nsISupports;

namespace mozilla {
namespace a11y {

class AccEvent;
class Accessible;
class DocAccessible;

/**
 * Manage the accessible focus. Used to fire and process accessible events.
 */
class FocusManager
{
public:
  virtual ~FocusManager();

  /**
   * Return a focused accessible.
   */
  Accessible* FocusedAccessible() const;

  /**
   * Return true if given accessible is focused.
   */
  bool IsFocused(const Accessible* aAccessible) const;

  /**
   * Return true if the given accessible is an active item, i.e. an item that
   * is current within the active widget.
   */
  inline bool IsActiveItem(const Accessible* aAccessible)
    { return aAccessible == mActiveItem; }

  /**
   * Return true if given DOM node has DOM focus.
   */
  inline bool HasDOMFocus(const nsINode* aNode) const
    { return aNode == FocusedDOMNode(); }

  /**
   * Return true if focused accessible is within the given container.
   */
  bool IsFocusWithin(const Accessible* aContainer) const;

  /**
   * Return whether the given accessible is focused or contains the focus or
   * contained by focused accessible.
   */
  enum FocusDisposition {
    eNone,
    eFocused,
    eContainsFocus,
    eContainedByFocus
  };
  FocusDisposition IsInOrContainsFocus(const Accessible* aAccessible) const;

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
  void ActiveItemChanged(Accessible* aItem, bool aCheckIfActive = true);

  /**
   * Dispatch delayed focus event for the current focus accessible.
   */
  void ForceFocusEvent();

  /**
   * Dispatch delayed focus event for the given target.
   */
  void DispatchFocusEvent(DocAccessible* aDocument, Accessible* aTarget);

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
  FocusManager& operator =(const FocusManager&);

  /**
   * Return DOM node having DOM focus.
   */
  nsINode* FocusedDOMNode() const;

  /**
   * Return DOM document having DOM focus.
   */
  nsIDocument* FocusedDOMDocument() const;

private:
  RefPtr<Accessible> mActiveItem;
  RefPtr<Accessible> mActiveARIAMenubar;
};

} // namespace a11y
} // namespace mozilla

#endif
