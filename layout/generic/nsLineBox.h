/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of one line within a block frame, a CSS line box */

#ifndef nsLineBox_h___
#define nsLineBox_h___

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#include "nsILineIterator.h"
#include "nsIFrame.h"
#include <algorithm>

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
#define LINE_MAX_CHILD_COUNT INT32_MAX

/**
 * Function to create a line box and initialize it with a single frame.
 * The allocation is infallible.
 * If the frame was moved from another line then you're responsible
 * for notifying that line using NoteFrameRemoved().  Alternatively,
 * it's better to use the next function that does that for you in an
 * optimal way.
 */
nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
                         bool aIsBlock);
/**
 * Function to create a line box and initialize it with aCount frames
 * that are currently on aFromLine.  The allocation is infallible.
 */
nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsLineBox* aFromLine,
                         nsIFrame* aFrame, int32_t aCount);

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
class nsLineBox final : public nsLineLink {
private:
  nsLineBox(nsIFrame* aFrame, int32_t aCount, bool aIsBlock);
  ~nsLineBox();
  
  // Infallible overloaded new operator. Uses an arena (which comes from the
  // presShell) to perform the allocation.
  void* operator new(size_t sz, nsIPresShell* aPresShell);
  void operator delete(void* aPtr, size_t sz) = delete;

public:
  // Use these functions to allocate and destroy line boxes
  friend nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsIFrame* aFrame,
                                  bool aIsBlock);
  friend nsLineBox* NS_NewLineBox(nsIPresShell* aPresShell, nsLineBox* aFromLine,
                                  nsIFrame* aFrame, int32_t aCount);
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
  static const uint32_t kMinChildCountForHashtable = 200;

  /**
   * Take ownership of aFromLine's hash table and remove the frames that
   * stay on aFromLine from it, i.e. aFromLineNewCount frames starting with
   * mFirstChild.  This method is used to optimize moving a large number
   * of frames from one line to the next.
   */
  void StealHashTableFrom(nsLineBox* aFromLine, uint32_t aFromLineNewCount);

  /**
   * Does the equivalent of this->NoteFrameAdded and aFromLine->NoteFrameRemoved
   * for each frame on this line, but in a optimized way.
   */
  void NoteFramesMovedFrom(nsLineBox* aFromLine);

  void SwitchToHashtable()
  {
    MOZ_ASSERT(!mFlags.mHasHashedFrames);
    uint32_t count = GetChildCount();
    mFlags.mHasHashedFrames = 1;
    uint32_t minLength = std::max(kMinChildCountForHashtable,
                                  uint32_t(PLDHashTable::kDefaultInitialLength));
    mFrames = new nsTHashtable< nsPtrHashKey<nsIFrame> >(std::max(count, minLength));
    for (nsIFrame* f = mFirstChild; count-- > 0; f = f->GetNextSibling()) {
      mFrames->PutEntry(f);
    }
  }
  void SwitchToCounter() {
    MOZ_ASSERT(mFlags.mHasHashedFrames);
    uint32_t count = GetChildCount();
    delete mFrames;
    mFlags.mHasHashedFrames = 0;
    mChildCount = count;
  }

public:
  int32_t GetChildCount() const {
    return MOZ_UNLIKELY(mFlags.mHasHashedFrames) ? mFrames->Count() : mChildCount;
  }

  /**
   * Register that aFrame is now on this line.
   */
  void NoteFrameAdded(nsIFrame* aFrame) {
    if (MOZ_UNLIKELY(mFlags.mHasHashedFrames)) {
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
    if (MOZ_UNLIKELY(mFlags.mHasHashedFrames)) {
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
  void SetBreakTypeBefore(uint8_t aBreakType) {
    NS_ASSERTION(IsBlock(), "Only blocks have break-before");
    NS_ASSERTION(aBreakType == NS_STYLE_CLEAR_NONE ||
                 aBreakType == NS_STYLE_CLEAR_LEFT ||
                 aBreakType == NS_STYLE_CLEAR_RIGHT ||
                 aBreakType == NS_STYLE_CLEAR_BOTH,
                 "Only float break types are allowed before a line");
    mFlags.mBreakType = aBreakType;
  }
  uint8_t GetBreakTypeBefore() const {
    return IsBlock() ? mFlags.mBreakType : NS_STYLE_CLEAR_NONE;
  }

  bool HasBreakAfter() const {
    return !IsBlock() && NS_STYLE_CLEAR_NONE != mFlags.mBreakType;
  }
  void SetBreakTypeAfter(uint8_t aBreakType) {
    NS_ASSERTION(!IsBlock(), "Only inlines have break-after");
    NS_ASSERTION(aBreakType <= LINE_MAX_BREAK_TYPE, "bad break type");
    mFlags.mBreakType = aBreakType;
  }
  bool HasFloatBreakAfter() const {
    return !IsBlock() && (NS_STYLE_CLEAR_LEFT == mFlags.mBreakType ||
                          NS_STYLE_CLEAR_RIGHT == mFlags.mBreakType ||
                          NS_STYLE_CLEAR_BOTH == mFlags.mBreakType);
  }
  uint8_t GetBreakTypeAfter() const {
    return !IsBlock() ? mFlags.mBreakType : NS_STYLE_CLEAR_NONE;
  }

  // mCarriedOutBEndMargin value
  nsCollapsingMargin GetCarriedOutBEndMargin() const;
  // Returns true if the margin changed
  bool SetCarriedOutBEndMargin(nsCollapsingMargin aValue);

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
  mozilla::LogicalRect GetOverflowArea(nsOverflowType aType,
                                       mozilla::WritingMode aWM,
                                       const nsSize& aContainerSize)
  {
    return mozilla::LogicalRect(aWM, GetOverflowArea(aType), aContainerSize);
  }
  nsRect GetOverflowArea(nsOverflowType aType) {
    return mData ? mData->mOverflowAreas.Overflow(aType) : GetPhysicalBounds();
  }
  nsOverflowAreas GetOverflowAreas() {
    if (mData) {
      return mData->mOverflowAreas;
    }
    nsRect bounds = GetPhysicalBounds();
    return nsOverflowAreas(bounds, bounds);
  }
  nsRect GetVisualOverflowArea()
    { return GetOverflowArea(eVisualOverflow); }
  nsRect GetScrollableOverflowArea()
    { return GetOverflowArea(eScrollableOverflow); }

  void SlideBy(nscoord aDBCoord, const nsSize& aContainerSize) {
    NS_ASSERTION(aContainerSize == mContainerSize ||
                 mContainerSize == nsSize(-1, -1),
                 "container size doesn't match");
    mContainerSize = aContainerSize;
    mBounds.BStart(mWritingMode) += aDBCoord;
    if (mData) {
      // Use a null containerSize to convert vector from logical to physical.
      const nsSize nullContainerSize;
      nsPoint physicalDelta =
        mozilla::LogicalPoint(mWritingMode, 0, aDBCoord).
          GetPhysicalPoint(mWritingMode, nullContainerSize);
      NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
        mData->mOverflowAreas.Overflow(otype) += physicalDelta;
      }
    }
  }

  // Container-size for the line is changing (and therefore if writing mode
  // was vertical-rl, the line will move physically; this is like SlideBy,
  // but it is the container size instead of the line's own logical coord
  // that is changing.
  nsSize UpdateContainerSize(const nsSize aNewContainerSize)
  {
    NS_ASSERTION(mContainerSize != nsSize(-1, -1), "container size not set");
    nsSize delta = mContainerSize - aNewContainerSize;
    mContainerSize = aNewContainerSize;
    // this has a physical-coordinate effect only in vertical-rl mode
    if (mWritingMode.IsVerticalRL() && mData) {
      nsPoint physicalDelta(-delta.width, 0);
      NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
        mData->mOverflowAreas.Overflow(otype) += physicalDelta;
      }
    }
    return delta;
  }

  void IndentBy(nscoord aDICoord, const nsSize& aContainerSize) {
    NS_ASSERTION(aContainerSize == mContainerSize ||
                 mContainerSize == nsSize(-1, -1),
                 "container size doesn't match");
    mContainerSize = aContainerSize;
    mBounds.IStart(mWritingMode) += aDICoord;
  }

  void ExpandBy(nscoord aDISize, const nsSize& aContainerSize) {
    NS_ASSERTION(aContainerSize == mContainerSize ||
                 mContainerSize == nsSize(-1, -1),
                 "container size doesn't match");
    mContainerSize = aContainerSize;
    mBounds.ISize(mWritingMode) += aDISize;
  }

  /**
   * The logical ascent (distance from block-start to baseline) of the
   * linebox is the logical ascent of the anonymous inline box (for
   * which we don't actually create a frame) that wraps all the
   * consecutive inline children of a block.
   *
   * This is currently unused for block lines.
   */
  nscoord GetLogicalAscent() const { return mAscent; }
  void SetLogicalAscent(nscoord aAscent) { mAscent = aAscent; }

  nscoord BStart() const {
    return mBounds.BStart(mWritingMode);
  }
  nscoord BSize() const {
    return mBounds.BSize(mWritingMode);
  }
  nscoord BEnd() const {
    return mBounds.BEnd(mWritingMode);
  }
  nscoord IStart() const {
    return mBounds.IStart(mWritingMode);
  }
  nscoord ISize() const {
    return mBounds.ISize(mWritingMode);
  }
  nscoord IEnd() const {
    return mBounds.IEnd(mWritingMode);
  }
  void SetBoundsEmpty() {
    mBounds.IStart(mWritingMode) = 0;
    mBounds.ISize(mWritingMode) = 0;
    mBounds.BStart(mWritingMode) = 0;
    mBounds.BSize(mWritingMode) = 0;
  }

  static void DeleteLineList(nsPresContext* aPresContext, nsLineList& aLines,
                             nsIFrame* aDestructRoot, nsFrameList* aFrames);

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
                                    int32_t* aFrameIndexInLine);

#ifdef DEBUG_FRAME_DUMP
  char* StateToString(char* aBuf, int32_t aBufSize) const;

  void List(FILE* out, int32_t aIndent, uint32_t aFlags = 0) const;
  void List(FILE* out = stderr, const char* aPrefix = "", uint32_t aFlags = 0) const;
  nsIFrame* LastChild() const;
#endif

private:
  int32_t IndexOf(nsIFrame* aFrame) const;
public:

  bool Contains(nsIFrame* aFrame) const {
    return MOZ_UNLIKELY(mFlags.mHasHashedFrames) ? mFrames->Contains(aFrame)
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
  static int32_t GetCtorCount();
#endif

  nsIFrame* mFirstChild;

  mozilla::WritingMode mWritingMode;

  // Physical size. Use only for physical <-> logical coordinate conversion.
  nsSize mContainerSize;

 private:
  mozilla::LogicalRect mBounds;

 public:
  const mozilla::LogicalRect& GetBounds() { return mBounds; }
  nsRect GetPhysicalBounds() const
  {
    if (mBounds.IsAllZero()) {
      return nsRect(0, 0, 0, 0);
    }

    NS_ASSERTION(mContainerSize != nsSize(-1, -1),
                 "mContainerSize not initialized");
    return mBounds.GetPhysicalRect(mWritingMode, mContainerSize);
  }
  void SetBounds(mozilla::WritingMode aWritingMode,
                 nscoord aIStart, nscoord aBStart,
                 nscoord aISize, nscoord aBSize,
                 const nsSize& aContainerSize)
  {
    mWritingMode = aWritingMode;
    mContainerSize = aContainerSize;
    mBounds = mozilla::LogicalRect(aWritingMode, aIStart, aBStart,
                                   aISize, aBSize);
  }
  void SetBounds(mozilla::WritingMode aWritingMode,
                 nsRect aRect, const nsSize& aContainerSize)
  {
    mWritingMode = aWritingMode;
    mContainerSize = aContainerSize;
    mBounds = mozilla::LogicalRect(aWritingMode, aRect, aContainerSize);
  }

  // mFlags.mHasHashedFrames says which one to use
  union {
    nsTHashtable< nsPtrHashKey<nsIFrame> >* mFrames;
    uint32_t mChildCount;
  };

  struct FlagBits {
    uint32_t mDirty : 1;
    uint32_t mPreviousMarginDirty : 1;
    uint32_t mHasClearance : 1;
    uint32_t mBlock : 1;
    uint32_t mImpactedByFloat : 1;
    uint32_t mLineWrapped: 1;
    uint32_t mInvalidateTextRuns : 1;
    uint32_t mResizeReflowOptimizationDisabled: 1;  // default 0 = means that the opt potentially applies to this line. 1 = never skip reflowing this line for a resize reflow
    uint32_t mEmptyCacheValid: 1;
    uint32_t mEmptyCacheState: 1;
    // mHasBullet indicates that this is an inline line whose block's
    // bullet is adjacent to this line and non-empty.
    uint32_t mHasBullet : 1;
    // Indicates that this line *may* have a placeholder for a float
    // that was pushed to a later column or page.
    uint32_t mHadFloatPushed : 1;
    uint32_t mHasHashedFrames: 1;
    uint32_t mBreakType : 4;
  };

  struct ExtraData {
    explicit ExtraData(const nsRect& aBounds) : mOverflowAreas(aBounds, aBounds) {
    }
    nsOverflowAreas mOverflowAreas;
  };

  struct ExtraBlockData : public ExtraData {
    explicit ExtraBlockData(const nsRect& aBounds)
      : ExtraData(aBounds),
        mCarriedOutBEndMargin()
    {
    }
    nsCollapsingMargin mCarriedOutBEndMargin;
  };

  struct ExtraInlineData : public ExtraData {
    explicit ExtraInlineData(const nsRect& aBounds) : ExtraData(aBounds) {
    }
    nsFloatCacheList mFloats;
  };

protected:
  nscoord mAscent;           // see |SetAscent| / |GetAscent|
  union {
    uint32_t mAllFlags;
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

    typedef uint32_t                    size_type;
    typedef int32_t                     difference_type;

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
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return *static_cast<pointer>(mCurrent);
    }

    pointer operator->()
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    pointer get()
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    operator pointer()
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    const_reference operator*() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
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
      MOZ_ASSERT(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther) const
    {
      MOZ_ASSERT(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent != aOther.mCurrent;
    }
    bool operator==(const iterator_self_type aOther)
    {
      MOZ_ASSERT(mListLink == aOther.mListLink, "comparing iterators over different lists");
      return mCurrent == aOther.mCurrent;
    }
    bool operator!=(const iterator_self_type aOther)
    {
      MOZ_ASSERT(mListLink == aOther.mListLink, "comparing iterators over different lists");
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

    typedef uint32_t                    size_type;
    typedef int32_t                     difference_type;

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
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return *static_cast<pointer>(mCurrent);
    }

    pointer operator->()
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    pointer get()
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    operator pointer()
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<pointer>(mCurrent);
    }

    const_reference operator*() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
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

    typedef uint32_t                    size_type;
    typedef int32_t                     difference_type;

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
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

    const_pointer get() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
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

    typedef uint32_t                    size_type;
    typedef int32_t                     difference_type;

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
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return *static_cast<const_pointer>(mCurrent);
    }

    const_pointer operator->() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

    const_pointer get() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
      return static_cast<const_pointer>(mCurrent);
    }

#ifndef __MWERKS__
    operator const_pointer() const
    {
      MOZ_ASSERT(mCurrent != mListLink, "running past end");
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

  typedef uint32_t                    size_type;
  typedef int32_t                     difference_type;

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

class nsLineIterator final : public nsILineIterator
{
public:
  nsLineIterator();
  ~nsLineIterator();

  virtual void DisposeLineIterator() override;

  virtual int32_t GetNumLines() override;
  virtual bool GetDirection() override;
  NS_IMETHOD GetLine(int32_t aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     int32_t* aNumFramesOnLine,
                     nsRect& aLineBounds) override;
  virtual int32_t FindLineContaining(nsIFrame* aFrame, int32_t aStartLine = 0) override;
  NS_IMETHOD FindFrameAt(int32_t aLineNumber,
                         nsPoint aPos,
                         nsIFrame** aFrameFound,
                         bool* aPosIsBeforeFirstFrame,
                         bool* aPosIsAfterLastFrame) override;

  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, int32_t aLineNumber) override;
  NS_IMETHOD CheckLineOrder(int32_t                  aLine,
                            bool                     *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual) override;
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

  nsLineBox* LineAt(int32_t aIndex) {
    if ((aIndex < 0) || (aIndex >= mNumLines)) {
      return nullptr;
    }
    return mLines[aIndex];
  }

  nsLineBox** mLines;
  int32_t mIndex;
  int32_t mNumLines;
  bool mRightToLeft;
};

#endif /* nsLineBox_h___ */
