/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameList_h___
#define nsFrameList_h___

#include <stdio.h> /* for FILE* */
#include "nsDebug.h"
#include "nsTArrayForwardDeclare.h"
#include "mozilla/ReverseIterator.h"

#if defined(DEBUG) || defined(MOZ_DUMP_PAINTING)
// DEBUG_FRAME_DUMP enables nsIFrame::List and related methods.
// You can also define this in a non-DEBUG build if you need frame dumps.
#define DEBUG_FRAME_DUMP 1
#endif

class nsContainerFrame;
class nsIFrame;
class nsIPresShell;
class nsPresContext;

namespace mozilla {
namespace layout {
  class FrameChildList;
  enum FrameChildListID {
      // The individual concrete child lists.
      kPrincipalList                = 0x1,
      kPopupList                    = 0x2,
      kCaptionList                  = 0x4,
      kColGroupList                 = 0x8,
      kSelectPopupList              = 0x10,
      kAbsoluteList                 = 0x20,
      kFixedList                    = 0x40,
      kOverflowList                 = 0x80,
      kOverflowContainersList       = 0x100,
      kExcessOverflowContainersList = 0x200,
      kOverflowOutOfFlowList        = 0x400,
      kFloatList                    = 0x800,
      kBulletList                   = 0x1000,
      kPushedFloatsList             = 0x2000,
      kBackdropList                 = 0x4000,
      // A special alias for kPrincipalList that suppress the reflow request that
      // is normally done when manipulating child lists.
      kNoReflowPrincipalList        = 0x8000
  };
} // namespace layout
} // namespace mozilla

// Uncomment this to enable expensive frame-list integrity checking
// #define DEBUG_FRAME_LIST

/**
 * A class for managing a list of frames.
 */
class nsFrameList {
public:
  nsFrameList() :
    mFirstChild(nullptr), mLastChild(nullptr)
  {
  }

  nsFrameList(nsIFrame* aFirstFrame, nsIFrame* aLastFrame) :
    mFirstChild(aFirstFrame), mLastChild(aLastFrame)
  {
    VerifyList();
  }

  nsFrameList(const nsFrameList& aOther) :
    mFirstChild(aOther.mFirstChild), mLastChild(aOther.mLastChild)
  {
  }

  /**
   * Infallibly allocate a nsFrameList from the shell arena.
   */
  void* operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW;

  /**
   * Deallocate this list that was allocated from the shell arena.
   * The list is required to be empty.
   */
  void Delete(nsIPresShell* aPresShell);

  /**
   * For each frame in this list: remove it from the list then call
   * Destroy() on it.
   */
  void DestroyFrames();

  /**
   * For each frame in this list: remove it from the list then call
   * DestroyFrom(aDestructRoot) on it.
   */
  void DestroyFramesFrom(nsIFrame* aDestructRoot);

  void Clear() { mFirstChild = mLastChild = nullptr; }

  void SetFrames(nsIFrame* aFrameList);

  void SetFrames(nsFrameList& aFrameList) {
    NS_PRECONDITION(!mFirstChild, "Losing frames");

    mFirstChild = aFrameList.FirstChild();
    mLastChild = aFrameList.LastChild();
    aFrameList.Clear();
  }

  class Slice;

  /**
   * Append aFrameList to this list.  If aParent is not null,
   * reparents the newly added frames.  Clears out aFrameList and
   * returns a list slice represening the newly-appended frames.
   */
  Slice AppendFrames(nsContainerFrame* aParent, nsFrameList& aFrameList) {
    return InsertFrames(aParent, LastChild(), aFrameList);
  }


  /**
   * Append aFrame to this list.  If aParent is not null,
   * reparents the newly added frame.
   */
  void AppendFrame(nsContainerFrame* aParent, nsIFrame* aFrame) {
    nsFrameList temp(aFrame, aFrame);
    AppendFrames(aParent, temp);
  }

  /**
   * Take aFrame out of the frame list. This also disconnects aFrame
   * from the sibling list. The frame must be non-null and present on
   * this list.
   */
  void RemoveFrame(nsIFrame* aFrame);

  /**
   * Take the frames after aAfterFrame out of the frame list.  If
   * aAfterFrame is null, removes the entire list.
   * @param aAfterFrame a frame in this list, or null
   * @return the removed frames, if any
   */
  nsFrameList RemoveFramesAfter(nsIFrame* aAfterFrame);

  /**
   * Take the first frame (if any) out of the frame list.
   * @return the first child, or nullptr if the list is empty
   */
  nsIFrame* RemoveFirstChild();

  /**
   * The following two functions are intended to be used in concert for
   * removing a frame from its frame list when the set of possible frame
   * lists is known in advance, but the exact frame list is unknown.
   * aFrame must be non-null.
   * Example use:
   *   bool removed = frameList1.StartRemoveFrame(aFrame) ||
   *                  frameList2.ContinueRemoveFrame(aFrame) ||
   *                  frameList3.ContinueRemoveFrame(aFrame);
   *   MOZ_ASSERT(removed);
   *
   * @note One of the frame lists MUST contain aFrame, if it's on some other
   *       frame list then the example above will likely lead to crashes.
   * This function is O(1).
   * @return true iff aFrame was removed from /some/ list, not necessarily
   *         this one.  If it was removed from a different list then it is
   *         guaranteed that that list is still non-empty.
   * (this method is implemented in nsIFrame.h to be able to inline)
   */
  inline bool StartRemoveFrame(nsIFrame* aFrame);

  /**
   * Precondition: StartRemoveFrame MUST be called before this.
   * This function is O(1).
   * @see StartRemoveFrame
   * @return true iff aFrame was removed from this list
   * (this method is implemented in nsIFrame.h to be able to inline)
   */
  inline bool ContinueRemoveFrame(nsIFrame* aFrame);

  /**
   * Take aFrame out of the frame list and then destroy it.
   * The frame must be non-null and present on this list.
   */
  void DestroyFrame(nsIFrame* aFrame);

  /**
   * Insert aFrame right after aPrevSibling, or prepend it to this
   * list if aPrevSibling is null. If aParent is not null, also
   * reparents newly-added frame. Note that this method always
   * sets the frame's nextSibling pointer.
   */
  void InsertFrame(nsContainerFrame* aParent, nsIFrame* aPrevSibling,
                   nsIFrame* aFrame) {
    nsFrameList temp(aFrame, aFrame);
    InsertFrames(aParent, aPrevSibling, temp);
  }


  /**
   * Inserts aFrameList into this list after aPrevSibling (at the beginning if
   * aPrevSibling is null).  If aParent is not null, reparents the newly added
   * frames.  Clears out aFrameList and returns a list slice representing the
   * newly-inserted frames.
   */
  Slice InsertFrames(nsContainerFrame* aParent, nsIFrame* aPrevSibling,
                     nsFrameList& aFrameList);

  class FrameLinkEnumerator;

  /**
   * Split this frame list such that all the frames before the link pointed to
   * by aLink end up in the returned list, while the remaining frames stay in
   * this list.  After this call, aLink points to the beginning of this list.
   */
  nsFrameList ExtractHead(FrameLinkEnumerator& aLink);

  /**
   * Split this frame list such that all the frames coming after the link
   * pointed to by aLink end up in the returned list, while the frames before
   * that link stay in this list.  After this call, aLink is at end.
   */
  nsFrameList ExtractTail(FrameLinkEnumerator& aLink);

  nsIFrame* FirstChild() const {
    return mFirstChild;
  }

  nsIFrame* LastChild() const {
    return mLastChild;
  }

  nsIFrame* FrameAt(int32_t aIndex) const;
  int32_t IndexOf(nsIFrame* aFrame) const;

  bool IsEmpty() const {
    return nullptr == mFirstChild;
  }

  bool NotEmpty() const {
    return nullptr != mFirstChild;
  }

  bool ContainsFrame(const nsIFrame* aFrame) const;

  int32_t GetLength() const;

  /**
   * If this frame list has only one frame, return that frame.
   * Otherwise, return null.
   */
  nsIFrame* OnlyChild() const {
    if (FirstChild() == LastChild()) {
      return FirstChild();
    }
    return nullptr;
  }

  /**
   * Call SetParent(aParent) for each frame in this list.
   * @param aParent the new parent frame, must be non-null
   */
  void ApplySetParent(nsContainerFrame* aParent) const;

  /**
   * If this frame list is non-empty then append it to aLists as the
   * aListID child list.
   * (this method is implemented in FrameChildList.h for dependency reasons)
   */
  inline void AppendIfNonempty(nsTArray<mozilla::layout::FrameChildList>* aLists,
                               mozilla::layout::FrameChildListID aListID) const;

  /**
   * Return the frame before this frame in visual order (after Bidi reordering).
   * If aFrame is null, return the last frame in visual order.
   */
  nsIFrame* GetPrevVisualFor(nsIFrame* aFrame) const;

  /**
   * Return the frame after this frame in visual order (after Bidi reordering).
   * If aFrame is null, return the first frame in visual order.
   */
  nsIFrame* GetNextVisualFor(nsIFrame* aFrame) const;

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out) const;
#endif

  static inline const nsFrameList& EmptyList();

  class Enumerator;

  /**
   * A class representing a slice of a frame list.
   */
  class Slice {
    friend class Enumerator;

  public:
    // Implicit on purpose, so that we can easily create enumerators from
    // nsFrameList via this impicit constructor.
    MOZ_IMPLICIT Slice(const nsFrameList& aList) :
#ifdef DEBUG
      mList(aList),
#endif
      mStart(aList.FirstChild()),
      mEnd(nullptr)
    {}

    Slice(const nsFrameList& aList, nsIFrame* aStart, nsIFrame* aEnd) :
#ifdef DEBUG
      mList(aList),
#endif
      mStart(aStart),
      mEnd(aEnd)
    {}

    Slice(const Slice& aOther) :
#ifdef DEBUG
      mList(aOther.mList),
#endif
      mStart(aOther.mStart),
      mEnd(aOther.mEnd)
    {}

  private:
#ifdef DEBUG
    const nsFrameList& mList;
#endif
    nsIFrame* const mStart; // our starting frame
    const nsIFrame* const mEnd; // The first frame that is NOT in the slice.
                                // May be null.
  };

  class Enumerator {
  public:
    explicit Enumerator(const Slice& aSlice) :
#ifdef DEBUG
      mSlice(aSlice),
#endif
      mFrame(aSlice.mStart),
      mEnd(aSlice.mEnd)
    {}

    Enumerator(const Enumerator& aOther) :
#ifdef DEBUG
      mSlice(aOther.mSlice),
#endif
      mFrame(aOther.mFrame),
      mEnd(aOther.mEnd)
    {}

    bool AtEnd() const {
      // Can't just check mEnd, because some table code goes and destroys the
      // tail of the frame list (including mEnd!) while iterating over the
      // frame list.
      return !mFrame || mFrame == mEnd;
    }

    /* Next() needs to know about nsIFrame, and nsIFrame will need to
       know about nsFrameList methods, so in order to inline this put
       the implementation in nsIFrame.h */
    inline void Next();

    /**
     * Get the current frame we're pointing to.  Do not call this on an
     * iterator that is at end!
     */
    nsIFrame* get() const {
      NS_PRECONDITION(!AtEnd(), "Enumerator is at end");
      return mFrame;
    }

    /**
     * Get an enumerator that is just like this one, but not limited in terms of
     * the part of the list it will traverse.
     */
    Enumerator GetUnlimitedEnumerator() const {
      return Enumerator(*this, nullptr);
    }

#ifdef DEBUG
    const nsFrameList& List() const { return mSlice.mList; }
#endif

  protected:
    Enumerator(const Enumerator& aOther, const nsIFrame* const aNewEnd):
#ifdef DEBUG
      mSlice(aOther.mSlice),
#endif
      mFrame(aOther.mFrame),
      mEnd(aNewEnd)
    {}

#ifdef DEBUG
    /* Has to be an object, not a reference, since the slice could
       well be a temporary constructed from an nsFrameList */
    const Slice mSlice;
#endif
    nsIFrame* mFrame; // our current frame.
    const nsIFrame* const mEnd; // The first frame we should NOT enumerate.
                                // May be null.
  };

  /**
   * A class that can be used to enumerate links between frames.  When created
   * from an nsFrameList, it points to the "link" immediately before the first
   * frame.  It can then be advanced until it points to the "link" immediately
   * after the last frame.  At any position, PrevFrame() and NextFrame() are
   * the frames before and after the given link.  This means PrevFrame() is
   * null when the enumerator is at the beginning of the list and NextFrame()
   * is null when it's AtEnd().
   */
  class FrameLinkEnumerator : private Enumerator {
  public:
    friend class nsFrameList;

    explicit FrameLinkEnumerator(const nsFrameList& aList) :
      Enumerator(aList),
      mPrev(nullptr)
    {}

    FrameLinkEnumerator(const FrameLinkEnumerator& aOther) :
      Enumerator(aOther),
      mPrev(aOther.mPrev)
    {}

    /* This constructor needs to know about nsIFrame, and nsIFrame will need to
       know about nsFrameList methods, so in order to inline this put
       the implementation in nsIFrame.h */
    inline FrameLinkEnumerator(const nsFrameList& aList, nsIFrame* aPrevFrame);

    void operator=(const FrameLinkEnumerator& aOther) {
      NS_PRECONDITION(&List() == &aOther.List(), "Different lists?");
      mFrame = aOther.mFrame;
      mPrev = aOther.mPrev;
    }

    inline void Next();

    bool AtEnd() const { return Enumerator::AtEnd(); }

    nsIFrame* PrevFrame() const { return mPrev; }
    nsIFrame* NextFrame() const { return mFrame; }

  protected:
    nsIFrame* mPrev;
  };

  class Iterator
  {
  public:
    Iterator(const nsFrameList& aList, nsIFrame* aCurrent)
      : mList(aList)
      , mCurrent(aCurrent)
    {}

    Iterator(const Iterator& aOther)
      : mList(aOther.mList)
      , mCurrent(aOther.mCurrent)
    {}

    nsIFrame* operator*() const { return mCurrent; }

    // The operators need to know about nsIFrame, hence the
    // implementations are in nsIFrame.h
    Iterator& operator++();
    Iterator& operator--();

    Iterator operator++(int) { auto ret = *this; ++*this; return ret; }
    Iterator operator--(int) { auto ret = *this; --*this; return ret; }

    friend bool operator==(const Iterator& aIter1, const Iterator& aIter2);
    friend bool operator!=(const Iterator& aIter1, const Iterator& aIter2);

  private:
    const nsFrameList& mList;
    nsIFrame* mCurrent;
  };

  typedef Iterator iterator;
  typedef Iterator const_iterator;
  typedef mozilla::ReverseIterator<Iterator> reverse_iterator;
  typedef mozilla::ReverseIterator<Iterator> const_reverse_iterator;

  iterator begin() const { return iterator(*this, mFirstChild); }
  const_iterator cbegin() const { return begin(); }
  iterator end() const { return iterator(*this, nullptr); }
  const_iterator cend() const { return end(); }
  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return rbegin(); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return rend(); }

private:
  void operator delete(void*) = delete;

#ifdef DEBUG_FRAME_LIST
  void VerifyList() const;
#else
  void VerifyList() const {}
#endif

protected:
  /**
   * Disconnect aFrame from its siblings.  This must only be called if aFrame
   * is NOT the first or last sibling, because otherwise its nsFrameList will
   * have a stale mFirst/LastChild pointer.  This precondition is asserted.
   * This function is O(1).
   */
  static void UnhookFrameFromSiblings(nsIFrame* aFrame);

  nsIFrame* mFirstChild;
  nsIFrame* mLastChild;
};

inline bool
operator==(const nsFrameList::Iterator& aIter1,
           const nsFrameList::Iterator& aIter2)
{
  MOZ_ASSERT(&aIter1.mList == &aIter2.mList,
             "must not compare iterator from different list");
  return aIter1.mCurrent == aIter2.mCurrent;
}

inline bool
operator!=(const nsFrameList::Iterator& aIter1,
           const nsFrameList::Iterator& aIter2)
{
  MOZ_ASSERT(&aIter1.mList == &aIter2.mList,
             "Must not compare iterator from different list");
  return aIter1.mCurrent != aIter2.mCurrent;
}

namespace mozilla {
namespace layout {

/**
 * Simple "auto_ptr" for nsFrameLists allocated from the shell arena.
 * The frame list given to the constructor will be deallocated (if non-null)
 * in the destructor.  The frame list must then be empty.
 */
class AutoFrameListPtr {
public:
  AutoFrameListPtr(nsPresContext* aPresContext, nsFrameList* aFrameList)
    : mPresContext(aPresContext), mFrameList(aFrameList) {}
  ~AutoFrameListPtr();
  operator nsFrameList*() const { return mFrameList; }
  nsFrameList* operator->() const { return mFrameList; }
private:
  nsPresContext* mPresContext;
  nsFrameList* mFrameList;
};

namespace detail {
union AlignedFrameListBytes {
  void* ptr;
  char bytes[sizeof(nsFrameList)];
};
extern const AlignedFrameListBytes gEmptyFrameListBytes;
} // namespace detail

} // namespace layout
} // namespace mozilla

/* static */ inline const nsFrameList&
nsFrameList::EmptyList()
{
  return *reinterpret_cast<const nsFrameList*>(&mozilla::layout::detail::gEmptyFrameListBytes);
}

#endif /* nsFrameList_h___ */
