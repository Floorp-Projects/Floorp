/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsLineBox_h___
#define nsLineBox_h___

#include "nsVoidArray.h"
#include "nsPlaceholderFrame.h"
#include "nsILineIterator.h"
#include "nsISizeOfHandler.h"

class nsISpaceManager;
class nsLineBox;
class nsFloaterCache;
class nsFloaterCacheList;
class nsFloaterCacheFreeList;

// State cached after reflowing a floater. This state is used during
// incremental reflow when we avoid reflowing a floater.
class nsFloaterCache {
public:
  nsFloaterCache();
#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsFloaterCache();
#else
  ~nsFloaterCache() { }
#endif

  nsFloaterCache* Next() const { return mNext; }

  nsPlaceholderFrame* mPlaceholder;     // nsPlaceholderFrame

  // This will be true if the floater was placed on the current line
  // instead of below the current line.
  PRBool mIsCurrentLineFloater;

  nsMargin mMargins;                    // computed margins

  nsMargin mOffsets;                    // computed offsets (relative pos)

  // Region in the spacemanager impacted by this floater; the
  // coordinates are relative to the containing block frame. The
  // region includes the margins around the floater, but doesn't
  // include the relative offsets.
  nsRect mRegion;

  // Combined area for the floater. This will not include the margins
  // for the floater. Like mRegion, the coordinates are relative to
  // the containing block frame.
  nsRect mCombinedArea;

protected:
  nsFloaterCache* mNext;

  friend class nsFloaterCacheList;
  friend class nsFloaterCacheFreeList;
};

//----------------------------------------

class nsFloaterCacheList {
public:
  nsFloaterCacheList() : mHead(nsnull) { }
  ~nsFloaterCacheList();

  PRBool IsEmpty() const {
    return nsnull == mHead;
  }

  PRBool NotEmpty() const {
    return nsnull != mHead;
  }

  nsFloaterCache* Head() const {
    return mHead;
  }

  nsFloaterCache* Tail() const;

  nsFloaterCache* Find(nsIFrame* aOutOfFlowFrame);

  void Remove(nsFloaterCache* aElement);

  void Append(nsFloaterCacheFreeList& aList);

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  nsFloaterCache* mHead;

  friend class nsFloaterCacheFreeList;
};

//---------------------------------------

class nsFloaterCacheFreeList : public nsFloaterCacheList {
public:
  nsFloaterCacheFreeList() : mTail(nsnull) { }
  ~nsFloaterCacheFreeList() { }

  // Steal away aList's nsFloaterCache objects and put them on this
  // free-list.
  void Append(nsFloaterCacheList& aList);

  void Append(nsFloaterCache* aFloaterCache);

  // Allocate a new nsFloaterCache object
  nsFloaterCache* Alloc();

protected:
  nsFloaterCache* mTail;

  friend class nsFloaterCacheList;
};

//----------------------------------------------------------------------

#define LINE_MAX_BREAK_TYPE  ((1 << 4) - 1)
#define LINE_MAX_CHILD_COUNT ((1 << 20) - 1)

#if NS_STYLE_CLEAR_LAST_VALUE > 15
need to rearrange the mBits bitfield;
#endif

// Funtion to create a line box
nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
                         PRInt32 aCount, PRBool aIsBlock);

/**
 * The nsLineBox class represents a horizontal line of frames. It contains
 * enough state to support incremental reflow of the frames, event handling
 * for the frames, and rendering of the frames.
 */
class nsLineBox {
private:
  nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock);
  ~nsLineBox();
  
  // Overloaded new operator. Uses an arena (which comes from the presShell)
  // to perform the allocation.
  void* operator new(size_t sz, nsIPresShell* aPresShell);
  void operator delete(void* aPtr, size_t sz);

public:
  // Use these two functions to allocate and destroy line boxes
  friend nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
                                  PRInt32 aCount, PRBool aIsBlock);

  void Destroy(nsIPresShell* aPresShell);

  // mBlock bit
  PRBool IsBlock() const {
    return mFlags.mBlock;
  }
  PRBool IsInline() const {
    return 0 == mFlags.mBlock;
  }

  // mDirty bit
  void MarkDirty() {
    mFlags.mDirty = 1;
  }
  void ClearDirty() {
    mFlags.mDirty = 0;
  }
  PRBool IsDirty() const {
    return mFlags.mDirty;
  }

  // mImpactedByFloater bit
  void SetLineIsImpactedByFloater(PRBool aValue) {
    NS_ASSERTION((PR_FALSE==aValue || PR_TRUE==aValue), "somebody is playing fast and loose with bools and bits!");
    mFlags.mImpactedByFloater = aValue;
  }
  PRBool IsImpactedByFloater() const {
    return mFlags.mImpactedByFloater;
  }

  // mTrimmed bit
  void SetTrimmed(PRBool aOn) {
    NS_ASSERTION((PR_FALSE==aOn || PR_TRUE==aOn), "somebody is playing fast and loose with bools and bits!");
    mFlags.mTrimmed = aOn;
  }
  PRBool IsTrimmed() const {
    return mFlags.mTrimmed;
  }

  // mHasPercentageChild bit
  void SetHasPercentageChild(PRBool aOn) {
    NS_ASSERTION((PR_FALSE==aOn || PR_TRUE==aOn), "somebody is playing fast and loose with bools and bits!");
    mFlags.mHasPercentageChild = aOn;
  }
  PRBool HasPercentageChild() const {
    return mFlags.mHasPercentageChild;
  }

  // mLineWrapped bit
  void SetLineWrapped(PRBool aOn) {
    NS_ASSERTION((PR_FALSE==aOn || PR_TRUE==aOn), "somebody is playing fast and loose with bools and bits!");
    mFlags.mLineWrapped = aOn;
  }
  PRBool IsLineWrapped() const {
    return mFlags.mLineWrapped;
  }

  // mLineWrapped bit
  void SetForceInvalidate(PRBool aOn) {
    NS_ASSERTION((PR_FALSE==aOn || PR_TRUE==aOn), "somebody is playing fast and loose with bools and bits!");
    mFlags.mForceInvalidate = aOn;
  }
  PRBool IsForceInvalidate() const {
    return mFlags.mForceInvalidate;
  }

  // mResizeReflowOptimizationDisabled bit
  void DisableResizeReflowOptimization() {
    mFlags.mResizeReflowOptimizationDisabled = PR_TRUE;
  }
  void EnableResizeReflowOptimization() {
    mFlags.mResizeReflowOptimizationDisabled = PR_FALSE;
  }
  PRBool ResizeReflowOptimizationDisabled() const {
    return mFlags.mResizeReflowOptimizationDisabled;
  }
  
  // mChildCount value
  PRInt32 GetChildCount() const {
    return (PRInt32) mFlags.mChildCount;
  }
  void SetChildCount(PRInt32 aNewCount) {
    if (NS_WARN_IF_FALSE(aNewCount >= 0, "negative child count")) {
      aNewCount = 0;
    }
    if (aNewCount > LINE_MAX_CHILD_COUNT) {
      aNewCount = LINE_MAX_CHILD_COUNT;
    }
    mFlags.mChildCount = aNewCount;
  }

  // mBreakType value
  PRBool HasBreak() const {
    return NS_STYLE_CLEAR_NONE != mFlags.mBreakType;
  }
  void SetBreakType(PRUint8 aBreakType) {
    NS_WARN_IF_FALSE(aBreakType <= LINE_MAX_BREAK_TYPE, "bad break type");
    mFlags.mBreakType = aBreakType;
  }
  PRUint8 GetBreakType() const {
    return mFlags.mBreakType;
  }

  // mCarriedOutBottomMargin value
  nscoord GetCarriedOutBottomMargin() const;
  void SetCarriedOutBottomMargin(nscoord aValue);

  // mFloaters
  PRBool HasFloaters() const {
    return (IsInline() && mInlineData) && mInlineData->mFloaters.NotEmpty();
  }
  nsFloaterCache* GetFirstFloater();
  void FreeFloaters(nsFloaterCacheFreeList& aFreeList);
  void AppendFloaters(nsFloaterCacheFreeList& aFreeList);
  PRBool RemoveFloater(nsIFrame* aFrame);
  void RemoveFloatersFromSpaceManager(nsISpaceManager* aSpaceManager);

  // Combined area
  void SetCombinedArea(const nsRect& aCombinedArea);
  void GetCombinedArea(nsRect* aResult);
  PRBool CombinedAreaIntersects(const nsRect& aDamageRect) {
    nsRect* ca = (mData ? &mData->mCombinedArea : &mBounds);
    return !((ca->YMost() <= aDamageRect.y) ||
             (ca->y >= aDamageRect.YMost()));
  }

  void SlideBy(nscoord aDY) {
    mBounds.y += aDY;
    if (mData) {
      mData->mCombinedArea.y += aDY;
    }
  }

  //----------------------------------------

  nscoord GetHeight() const {
    return mBounds.height;
  }

  static void DeleteLineList(nsIPresContext* aPresContext, nsLineBox* aLine);

  static nsLineBox* LastLine(nsLineBox* aLine);

  static nsLineBox* FindLineContaining(nsLineBox* aLine, nsIFrame* aFrame,
                                       PRInt32* aFrameIndexInLine);

#ifdef DEBUG
  void List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
#endif

  nsIFrame* LastChild() const;

  PRBool IsLastChild(nsIFrame* aFrame) const;

  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  PRInt32 IndexOf(nsIFrame* aFrame) const;

  PRBool Contains(nsIFrame* aFrame) const {
    return IndexOf(aFrame) >= 0;
  }

#ifdef DEBUG
  nsIAtom* SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
  static PRInt32 GetCtorCount();
  static PRInt32 ListLength(nsLineBox* aLine);
#endif

  nsIFrame* mFirstChild;
  nsLineBox* mNext;

  nsRect mBounds;
  nscoord mMaxElementWidth;  // width part of max-element-size
  nscoord mMaximumWidth;     // maximum width (needed for incremental reflow of tables)

  struct FlagBits {
    PRUint32 mDirty : 1;
    PRUint32 mBlock : 1;
    PRUint32 mImpactedByFloater : 1;
    PRUint32 mTrimmed : 1;
    PRUint32 mHasPercentageChild : 1;
    PRUint32 mLineWrapped: 1;
    PRUint32 mForceInvalidate: 1;                   // default 0 = means this line handles it's own invalidation.  1 = always invalidate this entire line
	  PRUint32 mResizeReflowOptimizationDisabled: 1;  // default 0 = means that the opt potentially applies to this line. 1 = never skip reflowing this line for a resize reflow
    PRUint32 mBreakType : 4;

    PRUint32 mChildCount : 20;
  };

  struct ExtraData {
    ExtraData(const nsRect& aBounds) : mCombinedArea(aBounds) {
    }
    nsRect mCombinedArea;
  };

  struct ExtraBlockData : public ExtraData {
    ExtraBlockData(const nsRect& aBounds) : ExtraData(aBounds) {
      mCarriedOutBottomMargin = 0;
    }
    nscoord mCarriedOutBottomMargin;
  };

  struct ExtraInlineData : public ExtraData {
    ExtraInlineData(const nsRect& aBounds) : ExtraData(aBounds) {
    }
    nsFloaterCacheList mFloaters;
  };

protected:
  union {
    PRUint32 mAllFlags;
    FlagBits mFlags;
  };

  union {
    ExtraData* mData;
    ExtraBlockData* mBlockData;
    ExtraInlineData* mInlineData;
  };

  void Cleanup();
  void MaybeFreeData();
};

//----------------------------------------------------------------------

class nsLineIterator : public nsILineIteratorNavigator {
public:
  nsLineIterator();
  virtual ~nsLineIterator();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetNumLines(PRInt32* aResult);
  NS_IMETHOD GetDirection(PRBool* aIsRightToLeft);
  NS_IMETHOD GetLine(PRInt32 aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds,
                     PRUint32* aLineFlags);
  NS_IMETHOD FindLineContaining(nsIFrame* aFrame,
                                PRInt32* aLineNumberResult);
  NS_IMETHOD FindLineAt(nscoord aY,
                        PRInt32* aLineNumberResult);
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
#ifdef IBMBIDI
                         PRBool aCouldBeReordered,
#endif
                         nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame,
                         PRBool* aXIsAfterLastFrame);

  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber);
#ifdef IBMBIDI
  NS_IMETHOD CheckLineOrder(PRInt32                  aLine,
                            PRBool                   *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual);
#endif
  nsresult Init(nsLineBox* aLines, PRBool aRightToLeft);

protected:
  PRInt32 NumLines() const {
    return mNumLines;
  }

  nsLineBox* CurrentLine() {
    return mLines[mIndex];
  }

  nsLineBox* PrevLine() {
    if (0 == mIndex) {
      return nsnull;
    }
    return mLines[--mIndex];
  }

  nsLineBox* NextLine() {
    if (mIndex >= mNumLines - 1) {
      return nsnull;
    }
    return mLines[++mIndex];
  }

  nsLineBox* LineAt(PRInt32 aIndex) {
    if ((aIndex < 0) || (aIndex >= mNumLines)) {
      return nsnull;
    }
    return mLines[aIndex];
  }

  nsLineBox** mLines;
  PRInt32 mIndex;
  PRInt32 mNumLines;
  PRPackedBool mRightToLeft;
};

#endif /* nsLineBox_h___ */
