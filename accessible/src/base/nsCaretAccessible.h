/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsCaretAccessible_h__
#define __nsCaretAccessible_h__

#include "nsBaseWidgetAccessible.h"
#include "nsIWeakReference.h"
#include "nsIDOMNode.h"
#include "nsIAccessibleCaret.h"
#include "nsISelectionListener.h"
#include "nsRect.h"

class nsRootAccessible;

/*
 * This special accessibility class is for the caret, which is really the currently focused selection.
 * There is only 1 visible caret per top level window (nsRootAccessible),
 * However, there may be several visible selections.
 *
 * The important selections are the one owned by each document, and the one in the currently focused control.
 *
 * The caret accesible does not exist within the normal accessible tree; it lives in a different world.
 * In MSAA, it is retrieved with via the WM_GETOBJECT message with lParam = OBJID_CARET, 
 * (as opposed to the root accessible tree for a window which is retrieved with OBJID_CLIENT)
 * In ATK, the caret accessible is never exposed as an accessible object, but is used to fire
 * caret move and selection change events.
 *
 * The caret accessible is owned by the nsRootAccessible for the top level window that it's in.
 * The nsRootAccessible needs to tell the nsCaretAccessible about focus changes via
 * setControlSelectionListener().
 * Each nsDocAccessible needs to tell the nsCaretAccessible owned by the root to
 * listen for selection events via addDocSelectionListener() and then needs to remove the 
 * selection listener when the doc goes away via removeDocSelectionListener().
 */

class nsCaretAccessible : public nsLeafAccessible, public nsIAccessibleCaret, public nsISelectionListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsCaretAccessible(nsIDOMNode* aDocumentNode, nsIWeakReference* aShell, nsRootAccessible *aRootAccessible);

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetParent(nsIAccessible **_retval);
  NS_IMETHOD GetRole(PRUint32 *_retval);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  NS_IMETHOD GetNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetPreviousSibling(nsIAccessible **_retval);

  /* ----- nsIAccessibleCaret ------ */
  NS_DECL_NSIACCESSIBLECARET
  
  /* ----- nsISelectionListener ---- */
  NS_DECL_NSISELECTIONLISTENER

  /* ----- nsIAccessNode ----- */
  NS_IMETHOD Init()
  {
#ifdef DEBUG_A11Y
    mIsInitialized = PR_TRUE;
#endif
    return NS_OK;
  }
  NS_IMETHOD Shutdown();

private:
  // The currently focused control -- never a document.
  // We listen to selection for one control at a time (the focused one)
  // Document selection is handled separately via additional listeners on all active documents
  // The current control is set via SetControlSelectionListener()
  nsCOMPtr<nsIDOMNode> mCurrentControl;  // Selection controller for the currently focused control
  nsCOMPtr<nsIWeakReference> mCurrentControlSelection;

  // Info for the the last selection event
  // If it was on a control, then mLastUsedSelection == mCurrentControlSelection
  // Otherwise, it's for a document where the selection changed
  nsCOMPtr<nsIWeakReference> mLastUsedSelection; // Weak ref to nsISelection
  nsCOMPtr<nsIDOMNode> mLastNodeWithCaret;
  PRInt32 mLastCaretOffset;
  already_AddRefed<nsICaret> GetLastCaret(nsRect *aRect, PRBool *aIsVisible);

  nsRootAccessible *mRootAccessible;
};

#endif
