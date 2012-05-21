/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsCaretAccessible_h__
#define __nsCaretAccessible_h__

#include "NotificationController.h"
#include "nsHyperTextAccessible.h"

#include "nsISelectionListener.h"

/*
 * This special accessibility class is for the caret, which is really the currently focused selection.
 * There is only 1 visible caret per top level window (RootAccessible),
 * However, there may be several visible selections.
 *
 * The important selections are the one owned by each document, and the one in the currently focused control.
 *
 * The caret accessible is no longer an accessible object in its own right.
 * On Windows it is used to move an invisible system caret that shadows the Mozilla caret. Windows will
 * also automatically map this to the MSAA caret accessible object (via OBJID_CARET).
 * (as opposed to the root accessible tree for a window which is retrieved with OBJID_CLIENT)
 * For ATK and Iaccessible2, the caret accessible is used to fire
 * caret move and selection change events.
 *
 * The caret accessible is owned by the RootAccessible for the top level window that it's in.
 * The RootAccessible needs to tell the nsCaretAccessible about focus changes via
 * setControlSelectionListener().
 * Each nsDocAccessible needs to tell the nsCaretAccessible owned by the root to
 * listen for selection events via addDocSelectionListener() and then needs to remove the 
 * selection listener when the doc goes away via removeDocSelectionListener().
 */

class nsCaretAccessible : public nsISelectionListener
{
public:
  NS_DECL_ISUPPORTS

  nsCaretAccessible(mozilla::a11y::RootAccessible* aRootAccessible);
  virtual ~nsCaretAccessible();
  void Shutdown();

  /* ----- nsISelectionListener ---- */
  NS_DECL_NSISELECTIONLISTENER

  /**
   * Listen to selection events on the focused control.
   * Only one control's selection events are listened to at a time, per top-level window.
   * This will remove the previous control's selection listener.
   * It will fail if aFocusedNode is a document node -- document selection must be listened
   * to via AddDocSelectionListener().
   * @param aFocusedNode   The node for the focused control
   */
  nsresult SetControlSelectionListener(nsIContent *aCurrentNode);

  /**
   * Stop listening to selection events for any control.
   * This does not have to be called explicitly in Shutdown() procedures,
   * because the nsCaretAccessible implementation guarantees that.
   */
  nsresult ClearControlSelectionListener();

  /**
   * Start listening to selection events for a given document
   * More than one document's selection events can be listened to
   * at the same time, by a given nsCaretAccessible
   * @param aShell   PresShell for document to listen to selection events from.
   */
  nsresult AddDocSelectionListener(nsIPresShell *aShell);

  /**
   * Stop listening to selection events for a given document
   * If the document goes away, this method needs to be called for 
   * that document by the owner of the caret. We use presShell because
   * instead of document because it is more direct than getting it from
   * the document, and in any case it is unavailable from the doc after a pagehide.
   * @param aShell   PresShell for document to no longer listen to selection events from.
   */
  nsresult RemoveDocSelectionListener(nsIPresShell *aShell);

  nsIntRect GetCaretRect(nsIWidget **aOutWidget);

protected:
  /**
   * Process DOM selection change. Fire selection and caret move events.
   */
  void ProcessSelectionChanged(nsISelection* aSelection);

  /**
   * Process normal selection change and fire caret move event.
   */
  void NormalSelectionChanged(nsISelection* aSelection);

  /**
   * Process spellcheck selection change and fire text attribute changed event
   * for invalid text attribute.
   */
  void SpellcheckSelectionChanged(nsISelection* aSelection);

  /**
   * Return selection controller for the given node.
   */
  already_AddRefed<nsISelectionController>
    GetSelectionControllerForNode(nsIContent *aNode);

private:
  // The currently focused control -- never a document.
  // We listen to selection for one control at a time (the focused one)
  // Document selection is handled separately via additional listeners on all active documents
  // The current control is set via SetControlSelectionListener()

  // Currently focused control.
  nsCOMPtr<nsIContent> mCurrentControl;

  // Info for the the last selection event.
  // If it was on a control, then its control's selection. Otherwise, it's for
  // a document where the selection changed.
  nsCOMPtr<nsIWeakReference> mLastUsedSelection; // Weak ref to nsISelection
  nsRefPtr<nsHyperTextAccessible> mLastTextAccessible;
  PRInt32 mLastCaretOffset;

  mozilla::a11y::RootAccessible* mRootAccessible;
};

#endif
