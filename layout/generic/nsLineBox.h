/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
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
#ifndef nsLineBox_h___
#define nsLineBox_h___

#include "nsVoidArray.h"
#include "nsPlaceholderFrame.h"
#include "nsILineIterator.h"

class nsSpaceManager;
class nsLineBox;
class nsFloatCache;
class nsFloatCacheList;
class nsFloatCacheFreeList;

// State cached after reflowing a float. This state is used during
// incremental reflow when we avoid reflowing a float.
class nsFloatCache {
public:
  nsFloatCache();
#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsFloatCache();
#else
  ~nsFloatCache() { }
#endif

  nsFloatCache* Next() const { return mNext; }

  nsPlaceholderFrame* mPlaceholder;     // nsPlaceholderFrame

  // This will be true if the float was placed on the current line
  // instead of below the current line.
  PRBool mIsCurrentLineFloat;

  nsMargin mMargins;                    // computed margins

  nsMargin mOffsets;                    // computed offsets (relative pos)

  // Region in the spacemanager impacted by this float; the
  // coordinates are relative to the containing block frame. The
  // region includes the margins around the float, but doesn't
  // include the relative offsets.
  nsRect mRegion;

  // Combined area for the float. This will not include the margins
  // for the float. Like mRegion, the coordinates are relative to
  // the containing block frame.
  nsRect mCombinedArea;

  // The float's max-element-width.
  nscoord mMaxElementWidth;

protected:
  nsFloatCache* mNext;

  friend class nsFloatCacheList;
  friend class nsFloatCacheFreeList;
};

//----------------------------------------

class nsFloatCacheList {
public:
  nsFloatCacheList() : mHead(nsnull) { }
  ~nsFloatCacheList();

  PRBool IsEmpty() const {
    return nsnull == mHead;
  }

  PRBool NotEmpty() const {
    return nsnull != mHead;
  }

  nsFloatCache* Head() const {
    return mHead;
  }

  nsFloatCache* Tail() const;

  nsFloatCache* Find(nsIFrame* aOutOfFlowFrame);

  void Remove(nsFloatCache* aElement);

  void Append(nsFloatCacheFreeList& aList);

protected:
  nsFloatCache* mHead;

  friend class nsFloatCacheFreeList;
};

//---------------------------------------

class nsFloatCacheFreeList : public nsFloatCacheList {
public:
  nsFloatCacheFreeList() : mTail(nsnull) { }
  ~nsFloatCacheFreeList() { }

  // Steal away aList's nsFloatCache objects and put them on this
  // free-list.
  void Append(nsFloatCacheList& aList);

  void Append(nsFloatCache* aFloatCache);

  // Allocate a new nsFloatCache object
  nsFloatCache* Alloc();

protected:
  nsFloatCache* mTail;

  friend class nsFloatCacheList;
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

class nsLineList;

// don't use the following names outside of this file.  Instead, use
// nsLineList::iterator, etc.  These are just here to allow them to
// be specified as parameters to methods of nsLineBox.
class nsLineList_iterator;
class nsLineList_const_iterator;
class nsLineList_reverse_iterator;
class nsLineList_const_reverse_iterator;

/**
 * Users must have the class that is to be part of the list inherit
 * from nsLineLink.  If they want to be efficient, it should be the
 * first base class.  (This was originally nsCLink in a templatized
 * nsCList, but it's still useful separately.)
 */

class nsLineLink {

  public:
    friend class nsLineList;
    friend class nsLineList_iterator;
    friend class nsLineList_reverse_iterator;
    friend class nsLineList_const_iterator;
    friend class nsLineList_const_reverse_iterator;

  private:
    nsLineLink *_mNext; // or head
    nsLineLink *_mPrev; // or tail

};


/**
 * The nsLineBox class represents a horizontal line of frames. It contains
 * enough state to support incremental reflow of the frames, event handling
 * for the frames, and rendering of the frames.
 */
class nsLineBox : public nsLineLink {
private:
  nsLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock);
  ~nsLineBox();
  
  // Overloaded new operator. Uses an arena (which comes from the presShell)
  // to perform the allocation.
  void* operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW;
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

  // mPreviousMarginDirty bit
  void MarkPreviousMarginDirty() {
    mFlags.mPreviousMarginDirty = 1;
  }
  void ClearPreviousMarginDirty() {
    mFlags.mPreviousMarginDirty = 0;
  }
  PRBool IsPreviousMarginDirty() const {
    return mFlags.mPreviousMarginDirty;
  }

  // mImpactedByFloat bit
  void SetLineIsImpactedByFloat(PRBool aValue) {
    NS_ASSERTION((PR_FALSE==aValue || PR_TRUE==aValue), "somebody is playing fast and loose with bools and bits!");
    mFlags.mImpactedByFloat = aValue;
  }
  PRBool IsImpactedByFloat() const {
    return mFlags.mImpactedByFloat;
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
    if (aNewCount < 0) {
      NS_WARNING("negative child count");
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
  nsCollapsingMargin GetCarriedOutBottomMargin() const;
  void SetCarriedOutBottomMargin(nsCollapsingMargin aValue);

  // mFloats
  PRBool HasFloats() const {
    return (IsInline() && mInlineData) && mInlineData->mFloats.NotEmpty();
  }
  nsFloatCache* GetFirstFloat();
  void FreeFloats(nsFloatCacheFreeList& aFreeList);
  void AppendFloats(nsFloatCacheFreeList& aFreeList);
  PRBool RemoveFloat(nsIFrame* aFrame);

  // Combined area is the area of the line that should influence the
  // overflow area of its parent block.  The combined area should be
  // used for painting-related things, but should never be used for
  // layout (except for handling of 'overflow').
  void SetCombinedArea(const nsRect& aCombinedArea);
  nsRect GetCombinedArea() {
    return mData ? mData->mCombinedArea : mBounds;
  }
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

  /**
   * The ascent (distance from top to baseline) of the linebox is the
   * ascent of the anonymous inline box (for which we don't actually
   * create a frame) that wraps all the consecutive inline children of a
   * block.
   *
   * This is currently unused for block lines.
   */
  nscoord GetAscent() const { return mAscent; }
  void SetAscent(nscoord aAscent) { mAscent = aAscent; }

  nscoord GetHeight() const {
    return mBounds.height;
  }

  static void DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines);

  // search from beginning to end
  // XXX Should switch to API below
  static nsLineBox* FindLineContaining(nsLineList& aLines, nsIFrame* aFrame,
                                       PRInt32* aFrameIndexInLine);

  // search from end to beginning of [aBegin, aEnd)
  // Returns PR_TRUE if it found the line and PR_FALSE if not.
  // Moves aEnd as it searches so that aEnd points to the resulting line.
  static PRBool RFindLineContaining(nsIFrame* aFrame,
                                    const nsLineList_iterator& aBegin,
                                    nsLineList_iterator& aEnd,
                                    PRInt32* aFrameIndexInLine);

#ifdef DEBUG
  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  void List(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
#endif

  nsIFrame* LastChild() const;

  PRBool IsLastChild(nsIFrame* aFrame) const;

  PRInt32 IndexOf(nsIFrame* aFrame) const;

  PRBool Contains(nsIFrame* aFrame) const {
    return IndexOf(aFrame) >= 0;
  }

  // whether the line box is "logically" empty (just like nsIFrame::IsEmpty)
  PRBool IsEmpty() const;

#ifdef DEBUG
  static PRInt32 GetCtorCount();
#endif

  nsIFrame* mFirstChild;

  nsRect mBounds;
  nscoord mMaxElementWidth;  // width part of max-element-size
  nscoord mMaximumWidth;     // maximum width (needed for incremental reflow of tables)

  struct FlagBits {
    PRUint32 mDirty : 1;
    PRUint32 mPreviousMarginDirty : 1;
    PRUint32 mBlock : 1;
    PRUint32 mImpactedByFloat : 1;
    PRUint32 mHasPercentageChild : 1;
    PRUint32 mLineWrapped: 1;
    PRUint32 mResizeReflowOptimizationDisabled: 1;  // default 0 = means that the opt potentially applies to this line. 1 = never skip reflowing this line for a resize reflow
    PRUint32 mBreakType : 4;

    PRUint32 mChildCount : 21;
  };

  struct ExtraData {
    ExtraData(const nsRect& aBounds) : mCombinedArea(aBounds) {
    }
    nsRect mCombinedArea;
  };

  struct ExtraBlockData : public ExtraData {
    ExtraBlockData(const nsRect& aBounds)
      : ExtraData(aBounds),
        mCarriedOutBottomMargin()
    {
    }
    nsCollapsingMargin mCarriedOutBottomMargin;
  };

  struct ExtraInlineData : public ExtraData {
    ExtraInlineData(const nsRect& aBounds) : ExtraData(aBounds) {
    }
    nsFloatCacheList mFloats;
  };

protected:
  nscoord mAscent;           // see |SetAscent| / |GetAscent|
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

#ifdef DEBUG
#define NS_LINELIST_DEBUG_PASS_END
#else
#undef NS_LINELIST_DEBUG_PASS_END
#endif

/**
 * A linked list type where the items in the list must inherit from
 * a link type to fuse allocations.
 *
 * API heavily based on the |list| class in the C++ standard.
 */
 
class nsLineList_iterator {
  public:
    friend class nsLineList;
    friend class nsLineList_reverse_iterator;
    friend class nsLineList_const_iterator;
    friend class nsLineList_const_reverse_iterator;

    typedef nsLineList_iterator         iterator_self_type;
    typedef nsLineList_reverse_iterator iterator_reverse_type;

    typedef nsLineBox&                  reference;
    typedef const nsLineBox&            const_reference;

    typedef nsLineBox*                  pointer;
    typedef const nsLineBox*            const_pointer;

    typedef PRUint32                    size_type;
    typedef PRInt32                     difference_type;

    typedef nsLineLink                  link_type;

    // Auto generated default constructor OK.
    // Auto generated copy-constructor OK.

    inline iterator_self_type&
        operator=(const iterator_self_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_reverse_type& aOther);

    iterator_self_type& operator++()
    {
      mCurrent = mCurrent->_mNext;
      return *this;
    }

    iterator_self_type operator++(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mNext;
      return rv;
    }

    iterator_self_type& operator--()
    {
      mCurrent = mCurrent->_mPrev;
      return *this;
    }

    iterator_self_type operator--(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mPrev;
      return rv;
    }

    reference operator*()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return *NS_STATIC_CAST(pointer, mCurrent);
    }

    pointer operator->()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(pointer, mCurrent);
    }

    pointer get()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(pointer, mCurrent);
    }

    operator pointer()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(pointer, mCurrent);
    }

    const_reference operator*() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return *NS_STATIC_CAST(const_pointer, mCurrent);
    }

    const_pointer operator->() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }
#endif /* !__MWERKS__ */

    iterator_self_type next()
    {
      iterator_self_type copy(*this);
      return ++copy;
    }

    const iterator_self_type next() const
    {
      iterator_self_type copy(*this);
      return ++copy;
    }

    iterator_self_type prev()
    {
      iterator_self_type copy(*this);
      return --copy;
    }

    const iterator_self_type prev() const
    {
      iterator_self_type copy(*this);
      return --copy;
    }

    // Passing by value rather than by reference and reference to const
    // to keep AIX happy.
    // XXX Should add assertions that |mListLink| is the same for both.
    PRBool operator==(const iterator_self_type aOther) const
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther) const
        { return mCurrent != aOther.mCurrent; }
    PRBool operator==(const iterator_self_type aOther)
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther)
        { return mCurrent != aOther.mCurrent; }

  private:
    link_type *mCurrent;
#ifdef NS_LINELIST_DEBUG_PASS_END
    link_type *mListLink; // the list's link, i.e., the end
#endif
};

class nsLineList_reverse_iterator {

  public:

    friend class nsLineList;
    friend class nsLineList_iterator;
    friend class nsLineList_const_iterator;
    friend class nsLineList_const_reverse_iterator;

    typedef nsLineList_reverse_iterator iterator_self_type;
    typedef nsLineList_iterator         iterator_reverse_type;

    typedef nsLineBox&                  reference;
    typedef const nsLineBox&            const_reference;

    typedef nsLineBox*                  pointer;
    typedef const nsLineBox*            const_pointer;

    typedef PRUint32                    size_type;
    typedef PRInt32                     difference_type;

    typedef nsLineLink                  link_type;

    // Auto generated default constructor OK.
    // Auto generated copy-constructor OK.

    inline iterator_self_type&
        operator=(const iterator_reverse_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_self_type& aOther);

    iterator_self_type& operator++()
    {
      mCurrent = mCurrent->_mPrev;
      return *this;
    }

    iterator_self_type operator++(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mPrev;
      return rv;
    }

    iterator_self_type& operator--()
    {
      mCurrent = mCurrent->_mNext;
      return *this;
    }

    iterator_self_type operator--(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mNext;
      return rv;
    }

    reference operator*()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return *NS_STATIC_CAST(pointer, mCurrent);
    }

    pointer operator->()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(pointer, mCurrent);
    }

    pointer get()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(pointer, mCurrent);
    }

    operator pointer()
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(pointer, mCurrent);
    }

    const_reference operator*() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return *NS_STATIC_CAST(const_pointer, mCurrent);
    }

    const_pointer operator->() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }
#endif /* !__MWERKS__ */

    // Passing by value rather than by reference and reference to const
    // to keep AIX happy.
    PRBool operator==(const iterator_self_type aOther) const
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther) const
        { return mCurrent != aOther.mCurrent; }
    PRBool operator==(const iterator_self_type aOther)
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther)
        { return mCurrent != aOther.mCurrent; }

  private:
    link_type *mCurrent;
#ifdef NS_LINELIST_DEBUG_PASS_END
    link_type *mListLink; // the list's link, i.e., the end
#endif
};

class nsLineList_const_iterator {
  public:

    friend class nsLineList;
    friend class nsLineList_iterator;
    friend class nsLineList_reverse_iterator;
    friend class nsLineList_const_reverse_iterator;

    typedef nsLineList_const_iterator           iterator_self_type;
    typedef nsLineList_const_reverse_iterator   iterator_reverse_type;
    typedef nsLineList_iterator                 iterator_nonconst_type;
    typedef nsLineList_reverse_iterator         iterator_nonconst_reverse_type;

    typedef nsLineBox&                  reference;
    typedef const nsLineBox&            const_reference;

    typedef nsLineBox*                  pointer;
    typedef const nsLineBox*            const_pointer;

    typedef PRUint32                    size_type;
    typedef PRInt32                     difference_type;

    typedef nsLineLink                  link_type;

    // Auto generated default constructor OK.
    // Auto generated copy-constructor OK.

    inline iterator_self_type&
        operator=(const iterator_nonconst_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_nonconst_reverse_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_self_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_reverse_type& aOther);

    iterator_self_type& operator++()
    {
      mCurrent = mCurrent->_mNext;
      return *this;
    }

    iterator_self_type operator++(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mNext;
      return rv;
    }

    iterator_self_type& operator--()
    {
      mCurrent = mCurrent->_mPrev;
      return *this;
    }

    iterator_self_type operator--(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mPrev;
      return rv;
    }

    const_reference operator*() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return *NS_STATIC_CAST(const_pointer, mCurrent);
    }

    const_pointer operator->() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }

    const_pointer get() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }
#endif /* !__MWERKS__ */

    const iterator_self_type next() const
    {
      iterator_self_type copy(*this);
      return ++copy;
    }

    const iterator_self_type prev() const
    {
      iterator_self_type copy(*this);
      return --copy;
    }

    // Passing by value rather than by reference and reference to const
    // to keep AIX happy.
    PRBool operator==(const iterator_self_type aOther) const
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther) const
        { return mCurrent != aOther.mCurrent; }
    PRBool operator==(const iterator_self_type aOther)
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther)
        { return mCurrent != aOther.mCurrent; }

  private:
    const link_type *mCurrent;
#ifdef NS_LINELIST_DEBUG_PASS_END
    const link_type *mListLink; // the list's link, i.e., the end
#endif
};

class nsLineList_const_reverse_iterator {
  public:

    friend class nsLineList;
    friend class nsLineList_iterator;
    friend class nsLineList_reverse_iterator;
    friend class nsLineList_const_iterator;

    typedef nsLineList_const_reverse_iterator   iterator_self_type;
    typedef nsLineList_const_iterator           iterator_reverse_type;
    typedef nsLineList_iterator                 iterator_nonconst_reverse_type;
    typedef nsLineList_reverse_iterator         iterator_nonconst_type;

    typedef nsLineBox&                  reference;
    typedef const nsLineBox&            const_reference;

    typedef nsLineBox*                  pointer;
    typedef const nsLineBox*            const_pointer;

    typedef PRUint32                    size_type;
    typedef PRInt32                     difference_type;

    typedef nsLineLink                  link_type;

    // Auto generated default constructor OK.
    // Auto generated copy-constructor OK.

    inline iterator_self_type&
        operator=(const iterator_nonconst_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_nonconst_reverse_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_self_type& aOther);
    inline iterator_self_type&
        operator=(const iterator_reverse_type& aOther);

    iterator_self_type& operator++()
    {
      mCurrent = mCurrent->_mPrev;
      return *this;
    }

    iterator_self_type operator++(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mPrev;
      return rv;
    }

    iterator_self_type& operator--()
    {
      mCurrent = mCurrent->_mNext;
      return *this;
    }

    iterator_self_type operator--(int)
    {
      iterator_self_type rv(*this);
      mCurrent = mCurrent->_mNext;
      return rv;
    }

    const_reference operator*() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return *NS_STATIC_CAST(const_pointer, mCurrent);
    }

    const_pointer operator->() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }

    const_pointer get() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
#ifdef NS_LINELIST_DEBUG_PASS_END
      NS_ASSERTION(mCurrent != mListLink, "running past end");
#endif
      return NS_STATIC_CAST(const_pointer, mCurrent);
    }
#endif /* !__MWERKS__ */

    // Passing by value rather than by reference and reference to const
    // to keep AIX happy.
    PRBool operator==(const iterator_self_type aOther) const
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther) const
        { return mCurrent != aOther.mCurrent; }
    PRBool operator==(const iterator_self_type aOther)
        { return mCurrent == aOther.mCurrent; }
    PRBool operator!=(const iterator_self_type aOther)
        { return mCurrent != aOther.mCurrent; }

//private:
    const link_type *mCurrent;
#ifdef NS_LINELIST_DEBUG_PASS_END
    const link_type *mListLink; // the list's link, i.e., the end
#endif
};

class nsLineList {

  public:

  friend class nsLineList_iterator;
  friend class nsLineList_reverse_iterator;
  friend class nsLineList_const_iterator;
  friend class nsLineList_const_reverse_iterator;

  typedef PRUint32                    size_type;
  typedef PRInt32                     difference_type;

  typedef nsLineLink                  link_type;

  private:
    link_type mLink;

  public:
    typedef nsLineList                  self_type;

    typedef nsLineBox&                  reference;
    typedef const nsLineBox&            const_reference;

    typedef nsLineBox*                  pointer;
    typedef const nsLineBox*            const_pointer;

    typedef nsLineList_iterator         iterator;
    typedef nsLineList_reverse_iterator reverse_iterator;
    typedef nsLineList_const_iterator   const_iterator;
    typedef nsLineList_const_reverse_iterator const_reverse_iterator;

    nsLineList()
    {
      clear();
    }

    const_iterator begin() const
    {
      const_iterator rv;
      rv.mCurrent = mLink._mNext;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    iterator begin()
    {
      iterator rv;
      rv.mCurrent = mLink._mNext;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    iterator begin(nsLineBox* aLine)
    {
      iterator rv;
      rv.mCurrent = aLine;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    const_iterator end() const
    {
      const_iterator rv;
      rv.mCurrent = &mLink;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    iterator end()
    {
      iterator rv;
      rv.mCurrent = &mLink;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    const_reverse_iterator rbegin() const
    {
      const_reverse_iterator rv;
      rv.mCurrent = mLink._mPrev;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    reverse_iterator rbegin()
    {
      reverse_iterator rv;
      rv.mCurrent = mLink._mPrev;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    const_reverse_iterator rend() const
    {
      const_reverse_iterator rv;
      rv.mCurrent = &mLink;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    reverse_iterator rend()
    {
      reverse_iterator rv;
      rv.mCurrent = &mLink;
#ifdef NS_LINELIST_DEBUG_PASS_END
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    PRBool empty() const
    {
      return mLink._mNext == &mLink;
    }

    // NOTE: O(N).
    size_type size() const
    {
      size_type count = 0;
      for (const link_type *cur = mLink._mNext;
           cur != &mLink;
           cur = cur->_mNext)
      {
        ++count;
      }
      return count;
    }

    pointer front()
    {
      NS_ASSERTION(!empty(), "no element to return");
      return NS_STATIC_CAST(pointer, mLink._mNext);
    }

    const_pointer front() const
    {
      NS_ASSERTION(!empty(), "no element to return");
      return NS_STATIC_CAST(const_pointer, mLink._mNext);
    }

    pointer back()
    {
      NS_ASSERTION(!empty(), "no element to return");
      return NS_STATIC_CAST(pointer, mLink._mPrev);
    }

    const_pointer back() const
    {
      NS_ASSERTION(!empty(), "no element to return");
      return NS_STATIC_CAST(const_pointer, mLink._mPrev);
    }

    void push_front(pointer aNew)
    {
      aNew->_mNext = mLink._mNext;
      mLink._mNext->_mPrev = aNew;
      aNew->_mPrev = &mLink;
      mLink._mNext = aNew;
    }

    void pop_front()
        // NOTE: leaves dangling next/prev pointers
    {
      NS_ASSERTION(!empty(), "no element to pop");
      link_type *newFirst = mLink._mNext->_mNext;
      newFirst->_mPrev = &mLink;
      // mLink._mNext->_mNext = nsnull;
      // mLink._mNext->_mPrev = nsnull;
      mLink._mNext = newFirst;
    }

    void push_back(pointer aNew)
    {
      aNew->_mPrev = mLink._mPrev;
      mLink._mPrev->_mNext = aNew;
      aNew->_mNext = &mLink;
      mLink._mPrev = aNew;
    }

    void pop_back()
        // NOTE: leaves dangling next/prev pointers
    {
      NS_ASSERTION(!empty(), "no element to pop");
      link_type *newLast = mLink._mPrev->_mPrev;
      newLast->_mNext = &mLink;
      // mLink._mPrev->_mPrev = nsnull;
      // mLink._mPrev->_mNext = nsnull;
      mLink._mPrev = newLast;
    }

    // inserts x before position
    iterator before_insert(iterator position, pointer x)
    {
      // use |mCurrent| to prevent DEBUG_PASS_END assertions
      x->_mPrev = position.mCurrent->_mPrev;
      x->_mNext = position.mCurrent;
      position.mCurrent->_mPrev->_mNext = x;
      position.mCurrent->_mPrev = x;
      return --position;
    }

    // inserts x after position
    iterator after_insert(iterator position, pointer x)
    {
      // use |mCurrent| to prevent DEBUG_PASS_END assertions
      x->_mNext = position.mCurrent->_mNext;
      x->_mPrev = position.mCurrent;
      position.mCurrent->_mNext->_mPrev = x;
      position.mCurrent->_mNext = x;
      return ++position;
    }

    // returns iterator pointing to after the element
    iterator erase(iterator position)
        // NOTE: leaves dangling next/prev pointers
    {
      position->_mPrev->_mNext = position->_mNext;
      position->_mNext->_mPrev = position->_mPrev;
      return ++position;
    }

    void swap(self_type& y)
    {
      link_type tmp(y.mLink);
      y.mLink = mLink;
      mLink = tmp;
    }

    void clear()
        // NOTE:  leaves dangling next/prev pointers
    {
      mLink._mNext = &mLink;
      mLink._mPrev = &mLink;
    }

    // inserts the conts of x before position and makes x empty
    void splice(iterator position, self_type& x)
    {
      // use |mCurrent| to prevent DEBUG_PASS_END assertions
      position.mCurrent->_mPrev->_mNext = x.mLink._mNext;
      x.mLink._mNext->_mPrev = position.mCurrent->_mPrev;
      x.mLink._mPrev->_mNext = position.mCurrent;
      position.mCurrent->_mPrev = x.mLink._mPrev;
      x.clear();
    }

    // Inserts element *i from list x before position and removes
    // it from x.
    void splice(iterator position, self_type& x, iterator i)
    {
      NS_ASSERTION(!x.empty(), "Can't insert from empty list.");
      NS_ASSERTION(position != i && position.mCurrent != i->_mNext,
                   "We don't check for this case.");

      // remove from |x|
      i->_mPrev->_mNext = i->_mNext;
      i->_mNext->_mPrev = i->_mPrev;

      // use |mCurrent| to prevent DEBUG_PASS_END assertions
      // link into |this|, before-side
      i->_mPrev = position.mCurrent->_mPrev;
      position.mCurrent->_mPrev->_mNext = i.get();

      // link into |this|, after-side
      i->_mNext = position.mCurrent;
      position.mCurrent->_mPrev = i.get();
    }

    // Inserts elements in [|first|, |last|), which are in |x|,
    // into |this| before |position| and removes them from |x|.
    void splice(iterator position, self_type& x, iterator first,
                iterator last)
    {
      NS_ASSERTION(!x.empty(), "Can't insert from empty list.");

      if (first == last)
        return;

      --last; // so we now want to move [first, last]
      // remove from |x|
      first->_mPrev->_mNext = last->_mNext;
      last->_mNext->_mPrev = first->_mPrev;

      // use |mCurrent| to prevent DEBUG_PASS_END assertions
      // link into |this|, before-side
      first->_mPrev = position.mCurrent->_mPrev;
      position.mCurrent->_mPrev->_mNext = first.get();

      // link into |this|, after-side
      last->_mNext = position.mCurrent;
      position.mCurrent->_mPrev = last.get();
    }

};


// Many of these implementations of operator= don't work yet.  I don't
// know why.

#ifdef NS_LINELIST_DEBUG_PASS_END

  // NOTE: ASSIGN_FROM is meant to be used *only* as the entire body
  // of a function and therefore lacks PR_{BEGIN,END}_MACRO
#define ASSIGN_FROM(other_)          \
  mCurrent = other_.mCurrent;        \
  mListLink = other_.mListLink;      \
  return *this;

#else /* !NS_LINELIST_DEBUG_PASS_END */

#define ASSIGN_FROM(other_)          \
  mCurrent = other_.mCurrent;        \
  return *this;

#endif /* !NS_LINELIST_DEBUG_PASS_END */

inline
nsLineList_iterator&
nsLineList_iterator::operator=(const nsLineList_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_iterator&
nsLineList_iterator::operator=(const nsLineList_reverse_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_reverse_iterator&
nsLineList_reverse_iterator::operator=(const nsLineList_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_reverse_iterator&
nsLineList_reverse_iterator::operator=(const nsLineList_reverse_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_iterator&
nsLineList_const_iterator::operator=(const nsLineList_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_iterator&
nsLineList_const_iterator::operator=(const nsLineList_reverse_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_iterator&
nsLineList_const_iterator::operator=(const nsLineList_const_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_iterator&
nsLineList_const_iterator::operator=(const nsLineList_const_reverse_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_reverse_iterator&
nsLineList_const_reverse_iterator::operator=(const nsLineList_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_reverse_iterator&
nsLineList_const_reverse_iterator::operator=(const nsLineList_reverse_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_reverse_iterator&
nsLineList_const_reverse_iterator::operator=(const nsLineList_const_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}

inline
nsLineList_const_reverse_iterator&
nsLineList_const_reverse_iterator::operator=(const nsLineList_const_reverse_iterator& aOther)
{
  ASSIGN_FROM(aOther)
}


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
  nsresult Init(nsLineList& aLines, PRBool aRightToLeft);

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
