/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_SelectionManager_h__
#define mozilla_a11y_SelectionManager_h__

#include "nsIFrame.h"
#include "nsISelectionListener.h"

class nsIPresShell;

namespace mozilla {

namespace dom {
class Element;
}

namespace a11y {

class AccEvent;

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

protected:
  /**
   * Process DOM selection change. Fire selection and caret move events.
   */
  void ProcessSelectionChanged(nsISelection* aSelection);

private:
  // Currently focused control.
  nsWeakFrame mCurrCtrlFrame;
};

} // namespace a11y
} // namespace mozilla

#endif
