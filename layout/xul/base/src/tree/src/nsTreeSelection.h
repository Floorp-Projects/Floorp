/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Brian Ryner <bryner@brianryner.com>
 *   Jan Varga <varga@nixcorp.com>
 */

#ifndef nsTreeSelection_h__
#define nsTreeSelection_h__

#include "nsITreeSelection.h"
#include "nsITimer.h"

class nsITreeBoxObject;
struct nsTreeRange;

class nsTreeSelection : public nsITreeSelection
{
public:
  nsTreeSelection(nsITreeBoxObject* aTree);
  ~nsTreeSelection();
   
  NS_DECL_ISUPPORTS
  NS_DECL_NSITREESELECTION

  friend struct nsTreeRange;

protected:
  nsresult FireOnSelectHandler();
  static void SelectCallback(nsITimer *aTimer, void *aClosure);

protected:
  // Members
  nsITreeBoxObject* mTree; // [Weak]. The tree will hold on to us through the view and let go when it dies.

  PRBool mSuppressed; // Whether or not we should be firing onselect events.
  PRInt32 mCurrentIndex; // The item to draw the rect around. The last one clicked, etc.
  PRInt32 mShiftSelectPivot; // Used when multiple SHIFT+selects are performed to pivot on.

  nsTreeRange* mFirstRange; // Our list of ranges.

  nsCOMPtr<nsITimer> mSelectTimer;
};

nsresult
NS_NewTreeSelection(nsITreeBoxObject* aTree, nsITreeSelection** aResult);

#endif
