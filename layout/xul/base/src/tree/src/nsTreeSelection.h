/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Hyatt <hyatt@mozilla.org> (Original Author)
 *   Brian Ryner <bryner@brianryner.com>
 *   Jan Varga <varga@ku.sk>
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

#ifndef nsTreeSelection_h__
#define nsTreeSelection_h__

#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsITimer.h"
#include "nsCycleCollectionParticipant.h"

class nsITreeBoxObject;
struct nsTreeRange;

class nsTreeSelection : public nsINativeTreeSelection
{
public:
  nsTreeSelection(nsITreeBoxObject* aTree);
  ~nsTreeSelection();
   
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsTreeSelection)
  NS_DECL_NSITREESELECTION

  // nsINativeTreeSelection: Untrusted code can use us
  NS_IMETHOD EnsureNative() { return NS_OK; }

  friend struct nsTreeRange;

protected:
  nsresult FireOnSelectHandler();
  static void SelectCallback(nsITimer *aTimer, void *aClosure);

protected:
  // Members
  nsCOMPtr<nsITreeBoxObject> mTree; // The tree will hold on to us through the view and let go when it dies.

  bool mSuppressed; // Whether or not we should be firing onselect events.
  PRInt32 mCurrentIndex; // The item to draw the rect around. The last one clicked, etc.
  nsCOMPtr<nsITreeColumn> mCurrentColumn;
  PRInt32 mShiftSelectPivot; // Used when multiple SHIFT+selects are performed to pivot on.

  nsTreeRange* mFirstRange; // Our list of ranges.

  nsCOMPtr<nsITimer> mSelectTimer;
};

nsresult
NS_NewTreeSelection(nsITreeBoxObject* aTree, nsITreeSelection** aResult);

#endif
