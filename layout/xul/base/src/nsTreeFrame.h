/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsTableFrame.h"
#include "nsVoidArray.h"

class nsTreeCellFrame;

class nsTreeFrame : public nsTableFrame
{
public:
  friend nsresult NS_NewTreeFrame(nsIFrame*& aNewFrame);

  void SetSelection(nsIPresContext& presContext, nsTreeCellFrame* pFrame);
  void ClearSelection(nsIPresContext& presContext);
  void ToggleSelection(nsIPresContext& presContext, nsTreeCellFrame* pFrame);
  void RangedSelection(nsIPresContext& aPresContext, nsTreeCellFrame* pEndFrame);

  void MoveUp(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveDown(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveLeft(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveRight(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
//  using nsTableFrame::MoveTo;    // don't hide inherited::Move
  void MoveTo(nsIPresContext& aPresContext, PRInt32 row, PRInt32 col, nsTreeCellFrame* pFrame);
     // NOTE: CWPro4 still reports a warning about hiding inherited virtual functions.
     // Compiler bug, ignore it.
     
protected:
  nsTreeFrame();
  virtual ~nsTreeFrame();

protected: // Data Members
	nsVoidArray mSelectedItems; // The selected cell frames.
    
}; // class nsTableFrame
