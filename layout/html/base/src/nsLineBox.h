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
#ifndef nsLineBox_h___
#define nsLineBox_h___

#include "nsVoidArray.h"
#include "nsPlaceholderFrame.h"

// bits in nsLineBox.mFlags
#define LINE_IS_DIRTY                 0x1
#define LINE_IS_BLOCK                 0x2
#define LINE_NEED_DID_REFLOW          0x8
#define LINE_TOP_MARGIN_IS_AUTO       0x10
#define LINE_BOTTOM_MARGIN_IS_AUTO    0x20
#define LINE_OUTSIDE_CHILDREN         0x40
#define LINE_ISA_EMPTY_LINE           0x80

class nsISpaceManager;
class nsLineBox;

/**
 * The nsLineBox class represents a horizontal line of frames. It contains
 * enough state to support incremental reflow of the frames, event handling
 * for the frames, and rendering of the frames.
 */
class nsLineBox {
public:

  nscoord GetCarriedOutBottomMargin() const {
    return mCarriedOutBottomMargin;
  }

  nscoord GetHeight() const { return mBounds.height; }

#ifdef NS_DEBUG
  PRBool CheckIsBlock() const;
#endif

  PRBool IsEmptyLine() const {
    return 0 != (mState & LINE_ISA_EMPTY_LINE);
  }

  void SetIsEmptyLine(PRBool aSetting) {
    if (aSetting)
      mState |= aSetting;
    else
      mState &= ~aSetting;
  }

  //----------------------------------------------------------------------
  // XXX old junk
  nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRUint16 flags);

  ~nsLineBox();

  static void DeleteLineList(nsIPresContext& aPresContext, nsLineBox* aLine);

  static nsLineBox* LastLine(nsLineBox* aLine);

  static nsLineBox* FindLineContaining(nsLineBox* aLine, nsIFrame* aFrame);

  void List(FILE* out, PRInt32 aIndent) const;

  PRInt32 ChildCount() const {
    return PRInt32(mChildCount);
  }

  nsIFrame* LastChild() const;

  PRBool IsLastChild(nsIFrame* aFrame) const;

  PRBool IsBlock() const {
    return 0 != (LINE_IS_BLOCK & mState);
  }

  void SetIsBlock() {
    mState |= LINE_IS_BLOCK;
  }

  void ClearIsBlock() {
    mState &= ~LINE_IS_BLOCK;
  }

  void SetIsBlock(PRBool aValue) {
    if (aValue) {
      SetIsBlock();
    }
    else {
      ClearIsBlock();
    }
  }

  void MarkDirty() {
    mState |= LINE_IS_DIRTY;
  }

  void ClearDirty() {
    mState &= ~LINE_IS_DIRTY;
  }

  void SetNeedDidReflow() {
    mState |= LINE_NEED_DID_REFLOW;
  }

  void ClearNeedDidReflow() {
    mState &= ~LINE_NEED_DID_REFLOW;
  }

  PRBool NeedsDidReflow() {
    return 0 != (LINE_NEED_DID_REFLOW & mState);
  }

  PRBool IsDirty() const {
    return 0 != (LINE_IS_DIRTY & mState);
  }

#ifdef XXX_need_line_outside_children
  void SetOutsideChildren() {
    mState |= LINE_OUTSIDE_CHILDREN;
  }

  void ClearOutsideChildren() {
    mState &= ~LINE_OUTSIDE_CHILDREN;
  }

  PRBool OutsideChildren() const {
    return 0 != (LINE_OUTSIDE_CHILDREN & mState);
  }
#endif

  PRUint16 GetState() const { return mState; }

  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  PRBool Contains(nsIFrame* aFrame) const;

  void UnplaceFloaters(nsISpaceManager* aSpaceManager);

#ifdef NS_DEBUG
  void Verify();
#endif

//XXX protected:
  nsIFrame* mFirstChild;
  PRUint16 mChildCount;
  PRUint16 mState;
  PRUint8 mBreakType;
  nsRect mBounds;
  nsRect mCombinedArea;
  nscoord mCarriedOutBottomMargin;/* XXX switch to 16 bits */
  nsVoidArray* mFloaters;
  nsLineBox* mNext;

protected:
};

#endif /* nsLineBox_h___ */
