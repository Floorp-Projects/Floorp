/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of one line within a block frame, a CSS line box */

#ifndef nsLineBox_h___
#define nsLineBox_h___

#include "mozilla/Attributes.h"

#include "nsILineIterator.h"
#include "nsIFrame.h"

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

  nsIFrame* mFloat;                     // floating frame

protected:
  nsFloatCache* mNext;

  friend class nsFloatCacheList;
  friend class nsFloatCacheFreeList;
};

//----------------------------------------

class nsFloatCacheList {
public:
#ifdef NS_BUILD_REFCNT_LOGGING
  nsFloatCacheList();
#else
  nsFloatCacheList() : mHead(nullptr) { }
#endif
  ~nsFloatCacheList();

  bool IsEmpty() const {
    return nullptr == mHead;
  }

  bool NotEmpty() const {
    return nullptr != mHead;
  }

  nsFloatCache* Head() const {
    return mHead;
  }

  nsFloatCache* Tail() const;

  void DeleteAll();

  nsFloatCache* Find(nsIFrame* aOutOfFlowFrame);

  // Remove a nsFloatCache from this list.  Deleting this nsFloatCache
  // becomes the caller's responsibility.
  void Remove(nsFloatCache* aElement) { RemoveAndReturnPrev(aElement); }
  
  // Steal away aList's nsFloatCache objects and put them in this
  // list.  aList must not be empty.
  void Append(nsFloatCacheFreeList& aList);

protected:
  nsFloatCache* mHead;

  // Remove a nsFloatCache from this list.  Deleting this nsFloatCache
  // becomes the caller's responsibility. Returns the nsFloatCache that was
  // before aElement, or nullptr if aElement was the first.
  nsFloatCache* RemoveAndReturnPrev(nsFloatCache* aElement);
  
  friend class nsFloatCacheFreeList;
};

//---------------------------------------
// Like nsFloatCacheList, but with fast access to the tail

class nsFloatCacheFreeList : private nsFloatCacheList {
public:
#ifdef NS_BUILD_REFCNT_LOGGING
  nsFloatCacheFreeList();
  ~nsFloatCacheFreeList();
#else
  nsFloatCacheFreeList() : mTail(nullptr) { }
  ~nsFloatCacheFreeList() { }
#endif

  // Reimplement trivial functions
  bool IsEmpty() const {
    return nullptr == mHead;
  }

  nsFloatCache* Head() const {
    return mHead;
  }

  nsFloatCache* Tail() const {
    return mTail;
  }
  
  bool NotEmpty() const {
    return nullptr != mHead;
  }

  void DeleteAll();

  // Steal away aList's nsFloatCache objects and put them on this
  // free-list.  aList must not be empty.
  void Append(nsFloatCacheList& aList);

  void Append(nsFloatCache* aFloatCache);

  void Remove(nsFloatCache* aElement);

  // Remove an nsFloatCache object from this list and return it, or create
  // a new one if this one is empty; Set its mFloat to aFloat.
  nsFloatCache* Alloc(nsIFrame* aFloat);
  
protected:
  nsFloatCache* mTail;

  friend class nsFloatCacheList;
};

//----------------------------------------------------------------------

#define LINE_MAX_BREAK_TYPE  ((1 << 4) - 1)
#define LINE_MAX_CHILD_COUNT PR_INT32_MAX

#if NS_STYLE_CLEAR_LAST_VALUE > 15
need to rearrange the mBits bitfield;
#endif

/**
 * Function to create a line box and initialize it with a single frame.
 * If the frame was moved from another line then you're responsible
 * for notifying that line using NoteFrameRemoved().  Alternatively,
 * it's better to use the next function that does that for you in an
 * optimal way.
 */
nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
                         bool aIsBlock);
/**
 * Function to create a line box and initialize it with aCount frames
 * that are currently on aFromLine.
 */
nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsLineBox* aFromLine,
                         nsIFrame* aFrame, PRInt32 aCount);

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
class nsLineBox MOZ_FINAL : public nsLineLink {
private:
  nsLineBox(nsIFrame* aFrame, PRInt32 aCount, bool aIsBlock);
  ~nsLineBox();
  
  // Overloaded new operator. Uses an arena (which comes from the presShell)
  // to perform the allocation.
  void* operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW;
  void operator delete(void* aPtr, size_t sz) MOZ_DELETE;

public:
  // Use these functions to allocate and destroy line boxes
  friend nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
                                  bool aIsBlock);
  friend nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsLineBox* aFromLine,
                                  nsIFrame* aFrame, PRInt32 aCount);
  void Destroy(nsIPresShell* aPresShell);

  // mBlock bit
  bool IsBlock() const {
    return mFlags.mBlock;
  }
  bool IsInline() const {
    return 0 == mFlags.mBlock;
  }

  // mDirty bit
  void MarkDirty() {
    mFlags.mDirty = 1;
  }
  void ClearDirty() {
    mFlags.mDirty = 0;
  }
  bool IsDirty() const {
    return mFlags.mDirty;
  }

  // mPreviousMarginDirty bit
  void MarkPreviousMarginDirty() {
    mFlags.mPreviousMarginDirty = 1;
  }
  void ClearPreviousMarginDirty() {
    mFlags.mPreviousMarginDirty = 0;
  }
  bool IsPreviousMarginDirty() const {
    return mFlags.mPreviousMarginDirty;
  }

  // mHasClearance bit
  void SetHasClearance() {
    mFlags.mHasClearance = 1;
  }
  void ClearHasClearance() {
    mFlags.mHasClearance = 0;
  }
  bool HasClearance() const {
    return mFlags.mHasClearance;
  }

  // mImpactedByFloat bit
  void SetLineIsImpactedByFloat(bool aValue) {
    mFlags.mImpactedByFloat = aValue;
  }
  bool IsImpactedByFloat() const {
    return mFlags.mImpactedByFloat;
  }

  // mLineWrapped bit
  void SetLineWrapped(bool aOn) {
    mFlags.mLineWrapped = aOn;
  }
  bool IsLineWrapped() const {
    return mFlags.mLineWrapped;
  }

  // mInvalidateTextRuns bit
  void SetInvalidateTextRuns(bool aOn) {
    mFlags.mInvalidateTextRuns = aOn;
  }
  bool GetInvalidateTextRuns() const {
    return mFlags.mInvalidateTextRuns;
  }

  // mResizeReflowOptimizationDisabled bit
  void DisableResizeReflowOptimization() {
    mFlags.mResizeReflowOptimizationDisabled = true;
  }
  void EnableResizeReflowOptimization() {
    mFlags.mResizeReflowOptimizationDisabled = false;
  }
  bool ResizeReflowOptimizationDisabled() const {
    return mFlags.mResizeReflowOptimizationDisabled;
  }

  // mHasBullet bit
  void SetHasBullet() {
    mFlags.mHasBullet = true;
    InvalidateCachedIsEmpty();
  }
  void ClearHasBullet() {
    mFlags.mHasBullet = false;
    InvalidateCachedIsEmpty();
  }
  bool HasBullet() const {
    return mFlags.mHasBullet;
  }

  // mHadFloatPushed bit
  void SetHadFloatPushed() {
    mFlags.mHadFloatPushed = true;
  }
  void ClearHadFloatPushed() {
    mFlags.mHadFloatPushed = false;
  }
  bool HadFloatPushed() const {
    return mFlags.mHadFloatPushed;
  }

private:
  // Add a hash table for fast lookup when the line has more frames than this.
  static const PRUint32 kMinChildCountForHashtable = 200;

  /**
   * Take ownership of aFromLine's hash table and remove the frames that
   * stay on aFromLine from it, i.e. aFromLineNewCount frames starting with
   * mFirstChild.  This method is used to optimize moving a large number
   * of frames from one line to the next.
   */
  void StealHashTableFrom(nsLineBox* aFromLine, PRUint32 aFromLineNewCount);

  /**
   * Does the equivalent of this->NoteFrameAdded and aFromLine->NoteFrameRemoved
   * for each frame on this line, but in a optimized way.
   */
  void NoteFramesMovedFrom(nsLineBox* aFromLine);

  void SwitchToHashtable()
  {
    MOZ_ASSERT(!mFlags.mHasHashedFrames);
    PRUint32 count = GetChildCount();
    mFrames = new nsTHashtable< nsPtrHashKey<nsIFrame> >();
    mFlags.mHasHashedFrames = 1;
    PRUint32 minSize =
      NS_MAX(kMinChildCountForHashtable, PRUint32(PL_DHASH_MIN_SIZE));
    mFrames->Init(NS_MAX(count, minSize));
    for (nsIFrame* f = mFirstChild; count-- > 0; f = f->GetNextSibling()) {
      mFrames->PutEntry(f);
    }
  }
  void SwitchToCounter() {
    MOZ_ASSERT(mFlags.mHasHashedFrames);
    PRUint32 count = GetChildCount();
    delete mFrames;
    mFlags.mHasHashedFrames = 0;
    mChildCount = count;
  }

public:
  PRInt32 GetChildCount() const {
    return NS_UNLIKELY(mFlags.mHasHashedFrames) ? mFrames->Count() : mChildCount;
  }

  /**
   * Register that aFrame is now on this line.
   */
  void NoteFrameAdded(nsIFrame* aFrame) {
    if (NS_UNLIKELY(mFlags.mHasHashedFrames)) {
      mFrames->PutEntry(aFrame);
    } else {
      if (++mChildCount >= kMinChildCountForHashtable) {
        SwitchToHashtable();
      }
    }
  }

  /**
   * Register that aFrame is not on this line anymore.
   */
  void NoteFrameRemoved(nsIFrame* aFrame) {
    MOZ_ASSERT(GetChildCount() > 0);
    if (NS_UNLIKELY(mFlags.mHasHashedFrames)) {
      mFrames->RemoveEntry(aFrame);
      if (mFrames->Count() < kMinChildCountForHashtable) {
        SwitchToCounter();
      }
    } else {
      --mChildCount;
    }
  }

  // mBreakType value
  // Break information is applied *before* the line if the line is a block,
  // or *after* the line if the line is an inline. Confusing, I know, but
  // using different names should help.
  bool HasBreakBefore() const {
    return IsBlock() && NS_STYLE_CLEAR_NONE != mFlags.mBreakType;
  }
  void SetBreakTypeBefore(PRUint8 aBreakType) {
    NS_ASSERTION(IsBlock(), "Only blocks have break-before");
    NS_ASSERTION(aBreakType <= NS_STYLE_CLEAR_LEFT_AND_RIGHT,
                 "Only float break types are allowed before a line");
    mFlags.mBreakType = aBreakType;
  }
  PRUint8 GetBreakTypeBefore() const {
    return IsBlock() ? mFlags.mBreakType : NS_STYLE_CLEAR_NONE;
  }

  bool HasBreakAfter() const {
    return !IsBlock() && NS_STYLE_CLEAR_NONE != mFlags.mBreakType;
  }
  void SetBreakTypeAfter(PRUint8 aBreakType) {
    NS_ASSERTION(!IsBlock(), "Only inlines have break-after");
    NS_ASSERTION(aBreakType <= LINE_MAX_BREAK_TYPE, "bad break type");
    mFlags.mBreakType = aBreakType;
  }
  bool HasFloatBreakAfter() const {
    return !IsBlock() && (NS_STYLE_CLEAR_LEFT == mFlags.mBreakType ||
                          NS_STYLE_CLEAR_RIGHT == mFlags.mBreakType ||
                          NS_STYLE_CLEAR_LEFT_AND_RIGHT == mFlags.mBreakType);
  }
  PRUint8 GetBreakTypeAfter() const {
    return !IsBlock() ? mFlags.mBreakType : NS_STYLE_CLEAR_NONE;
  }

  // mCarriedOutBottomMargin value
  nsCollapsingMargin GetCarriedOutBottomMargin() const;
  // Returns true if the margin changed
  bool SetCarriedOutBottomMargin(nsCollapsingMargin aValue);

  // mFloats
  bool HasFloats() const {
    return (IsInline() && mInlineData) && mInlineData->mFloats.NotEmpty();
  }
  nsFloatCache* GetFirstFloat();
  void FreeFloats(nsFloatCacheFreeList& aFreeList);
  void AppendFloats(nsFloatCacheFreeList& aFreeList);
  bool RemoveFloat(nsIFrame* aFrame);

  // Combined area is the area of the line that should influence the
  // overflow area of its parent block.  The combined area should be
  // used for painting-related things, but should never be used for
  // layout (except for handling of 'overflow').
  void SetOverflowAreas(const nsOverflowAreas& aOverflowAreas);
  nsRect GetOverflowArea(nsOverflowType aType) {
    return mData ? mData->mOverflowAreas.Overflow(aType) : mBounds;
  }
  nsOverflowAreas GetOverflowAreas() {
    if (mData) {
      return mData->mOverflowAreas;
    }
    return nsOverflowAreas(mBounds, mBounds);
  }
  nsRect GetVisualOverflowArea()
    { return GetOverflowArea(eVisualOverflow); }
  nsRect GetScrollableOverflowArea()
    { return GetOverflowArea(eScrollableOverflow); }

  void SlideBy(nscoord aDY) {
    mBounds.y += aDY;
    if (mData) {
      NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
        mData->mOverflowAreas.Overflow(otype).y += aDY;
      }
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

  static void DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines,
                             nsIFrame* aDestructRoot);

  // search from end to beginning of [aBegin, aEnd)
  // Returns true if it found the line and false if not.
  // Moves aEnd as it searches so that aEnd points to the resulting line.
  // aLastFrameBeforeEnd is the last frame before aEnd (so if aEnd is
  // the end of the line list, it's just the last frame in the frame
  // list).
  static bool RFindLineContaining(nsIFrame* aFrame,
                                    const nsLineList_iterator& aBegin,
                                    nsLineList_iterator& aEnd,
                                    nsIFrame* aLastFrameBeforeEnd,
                                    PRInt32* aFrameIndexInLine);

#ifdef DEBUG
  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  void List(FILE* out, PRInt32 aIndent) const;
  nsIFrame* LastChild() const;
#endif

private:
  PRInt32 IndexOf(nsIFrame* aFrame) const;
public:

  bool Contains(nsIFrame* aFrame) const {
    return NS_UNLIKELY(mFlags.mHasHashedFrames) ? mFrames->Contains(aFrame)
                                                : IndexOf(aFrame) >= 0;
  }

  // whether the line box is "logically" empty (just like nsIFrame::IsEmpty)
  bool IsEmpty() const;

  // Call this only while in Reflow() for the block the line belongs
  // to, only between reflowing the line (or sliding it, if we skip
  // reflowing it) and the end of reflowing the block.
  bool CachedIsEmpty();

  void InvalidateCachedIsEmpty() {
    mFlags.mEmptyCacheValid = false;
  }

  // For debugging purposes
  bool IsValidCachedIsEmpty() {
    return mFlags.mEmptyCacheValid;
  }

#ifdef DEBUG
  static PRInt32 GetCtorCount();
#endif

  nsIFrame* mFirstChild;

  nsRect mBounds;

  // mFlags.mHasHashedFrames says which one to use
  union {
    nsTHashtable< nsPtrHashKey<nsIFrame> >* mFrames;
    PRUint32 mChildCount;
  };

  struct FlagBits {
    PRUint32 mDirty : 1;
    PRUint32 mPreviousMarginDirty : 1;
    PRUint32 mHasClearance : 1;
    PRUint32 mBlock : 1;
    PRUint32 mImpactedByFloat : 1;
    PRUint32 mLineWrapped: 1;
    PRUint32 mInvalidateTextRuns : 1;
    PRUint32 mResizeReflowOptimizationDisabled: 1;  // default 0 = means that the opt potentially applies to this line. 1 = never skip reflowing this line for a resize reflow
    PRUint32 mEmptyCacheValid: 1;
    PRUint32 mEmptyCacheState: 1;
    // mHasBullet indicates that this is an inline line whose block's
    // bullet is adjacent to this line and non-empty.
    PRUint32 mHasBullet : 1;
    // Indicates that this line *may* have a placeholder for a float
    // that was pushed to a later column or page.
    PRUint32 mHadFloatPushed : 1;
    PRUint32 mHasHashedFrames: 1;
    PRUint32 mBreakType : 4;
  };

  struct ExtraData {
    ExtraData(const nsRect& aBounds) : mOverflowAreas(aBounds, aBounds) {
    }
    nsOverflowAreas mOverflowAreas;
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

#ifdef DEBUG
    nsLineList_iterator() { memset(&mCurrent, 0xcd, sizeof(mCurrent)); }
#else
    // Auto generated default constructor OK.
#endif
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
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return *static_cast<pointer>(mCurrent);
    }

    pointer operator->()
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    pointer get()
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    operator pointer()
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    const_reference operator*() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
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
    bool operator==(const iterator_self_type aOther) const
    {
      NS_ABORT_IF_FALSE(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther) const
    {
      NS_ABORT_IF_FALSE(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }
    bool operator==(const iterator_self_type aOther)
    {
      NS_ABORT_IF_FALSE(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther)
    {
      NS_ABORT_IF_FALSE(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }

  private:
    link_type *mCurrent;
#ifdef DEBUG
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

#ifdef DEBUG
    nsLineList_reverse_iterator() { memset(&mCurrent, 0xcd, sizeof(mCurrent)); }
#else
    // Auto generated default constructor OK.
#endif
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
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return *static_cast<pointer>(mCurrent);
    }

    pointer operator->()
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    pointer get()
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    operator pointer()
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    const_reference operator*() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }
#endif /* !__MWERKS__ */

    // Passing by value rather than by reference and reference to const
    // to keep AIX happy.
    bool operator==(const iterator_self_type aOther) const
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther) const
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }
    bool operator==(const iterator_self_type aOther)
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther)
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }

  private:
    link_type *mCurrent;
#ifdef DEBUG
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

#ifdef DEBUG
    nsLineList_const_iterator() { memset(&mCurrent, 0xcd, sizeof(mCurrent)); }
#else
    // Auto generated default constructor OK.
#endif
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
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

    const_pointer get() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
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
    bool operator==(const iterator_self_type aOther) const
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther) const
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }
    bool operator==(const iterator_self_type aOther)
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther)
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }

  private:
    const link_type *mCurrent;
#ifdef DEBUG
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

#ifdef DEBUG
    nsLineList_const_reverse_iterator() { memset(&mCurrent, 0xcd, sizeof(mCurrent)); }
#else
    // Auto generated default constructor OK.
#endif
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
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

    const_pointer get() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      NS_ABORT_IF_FALSE(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }
#endif /* !__MWERKS__ */

    // Passing by value rather than by reference and reference to const
    // to keep AIX happy.
    bool operator==(const iterator_self_type aOther) const
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther) const
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }
    bool operator==(const iterator_self_type aOther)
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther)
    {
      NS_ASSERTION(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }

//private:
    const link_type *mCurrent;
#ifdef DEBUG
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
      MOZ_COUNT_CTOR(nsLineList);
      clear();
    }

    ~nsLineList()
    {
      MOZ_COUNT_DTOR(nsLineList);
    }

    const_iterator begin() const
    {
      const_iterator rv;
      rv.mCurrent = mLink._mNext;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    iterator begin()
    {
      iterator rv;
      rv.mCurrent = mLink._mNext;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    iterator begin(nsLineBox* aLine)
    {
      iterator rv;
      rv.mCurrent = aLine;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    const_iterator end() const
    {
      const_iterator rv;
      rv.mCurrent = &mLink;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    iterator end()
    {
      iterator rv;
      rv.mCurrent = &mLink;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    const_reverse_iterator rbegin() const
    {
      const_reverse_iterator rv;
      rv.mCurrent = mLink._mPrev;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    reverse_iterator rbegin()
    {
      reverse_iterator rv;
      rv.mCurrent = mLink._mPrev;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    reverse_iterator rbegin(nsLineBox* aLine)
    {
      reverse_iterator rv;
      rv.mCurrent = aLine;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    const_reverse_iterator rend() const
    {
      const_reverse_iterator rv;
      rv.mCurrent = &mLink;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    reverse_iterator rend()
    {
      reverse_iterator rv;
      rv.mCurrent = &mLink;
#ifdef DEBUG
      rv.mListLink = &mLink;
#endif
      return rv;
    }

    bool empty() const
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
      return static_cast<pointer>(mLink._mNext);
    }

    const_pointer front() const
    {
      NS_ASSERTION(!empty(), "no element to return");
      return static_cast<const_pointer>(mLink._mNext);
    }

    pointer back()
    {
      NS_ASSERTION(!empty(), "no element to return");
      return static_cast<pointer>(mLink._mPrev);
    }

    const_pointer back() const
    {
      NS_ASSERTION(!empty(), "no element to return");
      return static_cast<const_pointer>(mLink._mPrev);
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
      // mLink._mNext->_mNext = nullptr;
      // mLink._mNext->_mPrev = nullptr;
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
      // mLink._mPrev->_mPrev = nullptr;
      // mLink._mPrev->_mNext = nullptr;
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

      if (!empty()) {
        mLink._mNext->_mPrev = &mLink;
        mLink._mPrev->_mNext = &mLink;
      }

      if (!y.empty()) {
        y.mLink._mNext->_mPrev = &y.mLink;
        y.mLink._mPrev->_mNext = &y.mLink;
      }
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

#ifdef DEBUG

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

class nsLineIterator MOZ_FINAL : public nsILineIterator
{
public:
  nsLineIterator();
  ~nsLineIterator();

  virtual void DisposeLineIterator();

  virtual PRInt32 GetNumLines();
  virtual bool GetDirection();
  NS_IMETHOD GetLine(PRInt32 aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds,
                     PRUint32* aLineFlags);
  virtual PRInt32 FindLineContaining(nsIFrame* aFrame, PRInt32 aStartLine = 0);
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
                         nsIFrame** aFrameFound,
                         bool* aXIsBeforeFirstFrame,
                         bool* aXIsAfterLastFrame);

  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber);
#ifdef IBMBIDI
  NS_IMETHOD CheckLineOrder(PRInt32                  aLine,
                            bool                     *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual);
#endif
  nsresult Init(nsLineList& aLines, bool aRightToLeft);

private:
  nsLineBox* PrevLine() {
    if (0 == mIndex) {
      return nullptr;
    }
    return mLines[--mIndex];
  }

  nsLineBox* NextLine() {
    if (mIndex >= mNumLines - 1) {
      return nullptr;
    }
    return mLines[++mIndex];
  }

  nsLineBox* LineAt(PRInt32 aIndex) {
    if ((aIndex < 0) || (aIndex >= mNumLines)) {
      return nullptr;
    }
    return mLines[aIndex];
  }

  nsLineBox** mLines;
  PRInt32 mIndex;
  PRInt32 mNumLines;
  bool mRightToLeft;
};

#endif /* nsLineBox_h___ */
