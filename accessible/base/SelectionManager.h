/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_SelectionManager_h__
#define mozilla_a11y_SelectionManager_h__

#include "nsIFrame.h"
#include "nsISelectionListener.h"
#include "mozilla/WeakPtr.h"

class nsIPresShell;

namespace mozilla {

namespace dom {
class Element;
class Selection;
}

namespace a11y {

class AccEvent;
class HyperTextAccessible;

/**
 * This special accessibility class is for the caret and selection management.
 * There is only 1 visible caret per top level window. However, there may be
 * several visible selections.
 *
 * The important selections are the one owned by each document, and the one in
 * the currently focused control.
 *
 * On Windows this class is used to move an invisible system caret that
 * shadows the Mozilla caret. Windows will also automatically map this to
 * the MSAA caret accessible object (via OBJID_CARET) (as opposed to the root
 * accessible tree for a window which is retrieved with OBJID_CLIENT).
 *
 * For ATK and IAccessible2, this class is used to fire caret move and
 * selection change events.
 */

struct SelData;

class SelectionManager : public nsISelectionListener
{
public:
  // nsISupports
  // implemented by derived nsAccessibilityService

  // nsISelectionListener
  NS_DECL_NSISELECTIONLISTENER

  // SelectionManager
  void Shutdown() { ClearControlSelectionListener(); }

  /**
   * Listen to selection events on the focused control.
   *
   * Note: only one control's selection events are listened to at a time. This
   * will remove the previous control's selection listener.
   */
  void SetControlSelectionListener(dom::Element* aFocusedElm);

  /**
   * Stop listening to selection events on the control.
   */
  void ClearControlSelectionListener();

  /**
   * Listen to selection events on the document.
   */
  void AddDocSelectionListener(nsIPresShell* aPresShell);

  /**
   * Stop listening to selection events for a given document
   */
  void RemoveDocSelectionListener(nsIPresShell* aShell);

  /**
   * Process delayed event, results in caret move and text selection change
   * events.
   */
  void ProcessTextSelChangeEvent(AccEvent* aEvent);

  /**
   * Gets the current caret offset/hypertext accessible pair. If there is no
   * current pair, then returns -1 for the offset and a nullptr for the
   * accessible.
   */
  inline HyperTextAccessible* AccessibleWithCaret(int32_t* aCaret)
  {
    if (aCaret)
      *aCaret = mCaretOffset;

    return mAccWithCaret;
  }

  /**
   * Update caret offset when it doesn't go through a caret move event.
   */
  inline void UpdateCaretOffset(HyperTextAccessible* aItem, int32_t aOffset)
  {
    mAccWithCaret = aItem;
    mCaretOffset = aOffset;
  }

  inline void ResetCaretOffset()
  {
    mCaretOffset = -1;
    mAccWithCaret = nullptr;
  }

protected:

  SelectionManager();

  /**
   * Process DOM selection change. Fire selection and caret move events.
   */
  void ProcessSelectionChanged(SelData* aSelData);

private:
  // Currently focused control.
  int32_t mCaretOffset;
  HyperTextAccessible* mAccWithCaret;
  WeakPtr<dom::Selection> mCurrCtrlNormalSel;
  WeakPtr<dom::Selection> mCurrCtrlSpellSel;
};

} // namespace a11y
} // namespace mozilla

#endif
