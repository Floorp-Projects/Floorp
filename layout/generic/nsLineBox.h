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
#include "nsILineIterator.h"
#include "nsISizeOfHandler.h"

class nsISpaceManager;
class nsLineBox;

//----------------------------------------------------------------------

class nsFloaterCache;
class nsFloaterCacheList;
class nsFloaterCacheFreeList;

// State cached after reflowing a floater. This state is used during
// incremental reflow when we avoid reflowing a floater.
class nsFloaterCache {
public:
  nsFloaterCache();
#ifdef DEBUG
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

/**
 * The nsLineBox class represents a horizontal line of frames. It contains
 * enough state to support incremental reflow of the frames, event handling
 * for the frames, and rendering of the frames.
 */
class nsLineBox {
public:
  nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock);
  ~nsLineBox();

  PRBool IsBlock() const {
    return mFlags.mBlock;
  }

  PRBool IsInline() const {
    return 0 == mFlags.mBlock;
  }

  // XXX Turn into a bit-field to simplify this code
  void SetTrimmed(PRBool aOn) {
    mFlags.mTrimmed = aOn;
  }

  PRBool IsTrimmed() const {
    return mFlags.mTrimmed;
  }

  PRBool HasBreak() const {
    return NS_STYLE_CLEAR_NONE != mFlags.mBreakType;
  }

  void SetBreakType(PRUint8 aBreakType) {
    mFlags.mBreakType = aBreakType;
  }

  PRUint8 GetBreakType() const {
    return mFlags.mBreakType;
  }

  nscoord GetCarriedOutBottomMargin() const {
    return mCarriedOutBottomMargin;
  }

  nscoord GetHeight() const { return mBounds.height; }

  //----------------------------------------------------------------------
  // XXX old junk


  static void DeleteLineList(nsIPresContext& aPresContext, nsLineBox* aLine);

  static nsLineBox* LastLine(nsLineBox* aLine);

  static nsLineBox* FindLineContaining(nsLineBox* aLine, nsIFrame* aFrame,
                                       PRInt32* aFrameIndexInLine);

  void List(FILE* out, PRInt32 aIndent) const;

  PRInt32 ChildCount() const {
    return PRInt32(mChildCount);
  }

  nsIFrame* LastChild() const;

  PRBool IsLastChild(nsIFrame* aFrame) const;

  void SetIsBlock(PRBool aValue) {
    mFlags.mBlock = aValue;
  }

  void SetLineIsImpactedByFloater(PRBool aValue) {
    mFlags.mImpactedByFloater = aValue;
  }

  PRBool IsImpactedByFloater() const {
    return mFlags.mImpactedByFloater;
  }

  void MarkDirty() {
    mFlags.mDirty = 1;
  }

  void ClearDirty() {
    mFlags.mDirty = 0;
  }

  PRBool IsDirty() const {
    return mFlags.mDirty;
  }

  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  PRInt32 IndexOf(nsIFrame* aFrame) const;

  PRBool Contains(nsIFrame* aFrame) const {
    return IndexOf(aFrame) >= 0;
  }

#ifdef NS_DEBUG
  PRBool CheckIsBlock() const;
#endif

#ifdef DEBUG
  PRBool SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

  nsIFrame* mFirstChild;
  PRUint16 mChildCount;
  nsRect mBounds;
  nsRect mCombinedArea;
  nscoord mCarriedOutBottomMargin;/* XXX switch to 16 bits */
  nsFloaterCacheList mFloaters;
  nsLineBox* mNext;
  nscoord mMaxElementWidth;  // width part of max-element-size

  struct FlagBits {
    PRUint32 mDirty : 1;
    PRUint32 mBlock : 1;
    PRUint32 mImpactedByFloater : 1;
    PRUint32 mTrimmed : 1;

    PRUint32 reserved : 20;

    PRUint32 mBreakType : 8;
  };

protected:
  union {
    PRUint32 mAllFlags;
    FlagBits mFlags;
  };
};

//----------------------------------------------------------------------

class nsLineIterator : public nsILineIterator {
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
                         nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame,
                         PRBool* aXIsAfterLastFrame);

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
  PRBool mRightToLeft;
};

#endif /* nsLineBox_h___ */
