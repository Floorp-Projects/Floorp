/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Iterator class for frame lists that respect CSS "order" during layout */

#ifndef mozilla_CSSOrderAwareFrameIterator_h
#define mozilla_CSSOrderAwareFrameIterator_h

#include <algorithm>
#include "nsFrameList.h"
#include "nsIFrame.h"
#include "mozilla/Maybe.h"
#include "mozilla/Assertions.h"

#if defined(__clang__) && __clang_major__ == 3 && __clang_minor__ <= 9
#  define CLANG_CRASH_BUG 1
#endif

namespace mozilla {

/**
 * CSSOrderAwareFrameIteratorT is a base class for iterators that traverse
 * child frame lists in a way that respects their CSS "order" property.
 *   https://drafts.csswg.org/css-flexbox-1/#order-property
 * This class isn't meant to be directly used; instead, use its specializations
 * CSSOrderAwareFrameIterator and ReverseCSSOrderAwareFrameIterator.
 *
 * Client code can use a CSSOrderAwareFrameIterator to traverse lower-"order"
 * frames before higher-"order" ones (as required for correct flex/grid
 * layout), without modifying the frames' actual ordering within the frame
 * tree. Any frames with equal "order" values will be traversed consecutively,
 * in frametree order (which is generally equivalent to DOM order).
 *
 * By default, the iterator will skip past placeholder frames during
 * iteration. You can adjust this behavior via the ChildFilter constructor arg.
 *
 * By default, the iterator will use the frames' CSS "order" property to
 * determine its traversal order. However, it can be customized to instead use
 * the (prefixed) legacy "box-ordinal-group" CSS property instead, as part of
 * emulating "display:-webkit-box" containers. This behavior can be customized
 * using the OrderingProperty constructor arg.
 *
 * A few notes on performance:
 *  - If you're iterating multiple times in a row, it's a good idea to reuse
 * the same iterator (calling Reset() to start each new iteration), rather than
 * instantiating a new one each time.
 *  - If you have foreknowledge of the list's orderedness, you can save some
 * time by passing eKnownOrdered or eKnownUnordered to the constructor (which
 * will skip some checks during construction).
 *
 * Warning: if the given frame list changes, it makes the iterator invalid and
 * bad things will happen if it's used further.
 */
template <typename Iterator>
class CSSOrderAwareFrameIteratorT {
 public:
  enum OrderState { eUnknownOrder, eKnownOrdered, eKnownUnordered };
  enum ChildFilter { eSkipPlaceholders, eIncludeAll };
  enum OrderingProperty {
    eUseOrder,           // Default behavior: use "order".
    eUseBoxOrdinalGroup  // Legacy behavior: use prefixed "box-ordinal-group".
  };
  CSSOrderAwareFrameIteratorT(nsIFrame* aContainer,
                              nsIFrame::ChildListID aListID,
                              ChildFilter aFilter = eSkipPlaceholders,
                              OrderState aState = eUnknownOrder,
                              OrderingProperty aOrderProp = eUseOrder)
      : mChildren(aContainer->GetChildList(aListID)),
        mArrayIndex(0),
        mItemIndex(0),
        mSkipPlaceholders(aFilter == eSkipPlaceholders)
#ifdef DEBUG
        ,
        mContainer(aContainer),
        mListID(aListID)
#endif
  {
    MOZ_ASSERT(aContainer->IsFlexOrGridContainer(),
               "Only use this iterator in a container that honors 'order'");

    size_t count = 0;
    bool isOrdered = aState != eKnownUnordered;
    if (aState == eUnknownOrder) {
      auto maxOrder = std::numeric_limits<int32_t>::min();
      for (auto* child : mChildren) {
        ++count;

        int32_t order = aOrderProp == eUseBoxOrdinalGroup
                            ? child->StyleXUL()->mBoxOrdinal
                            : child->StylePosition()->mOrder;

        if (order < maxOrder) {
          isOrdered = false;
          break;
        }
        maxOrder = order;
      }
    }
    if (isOrdered) {
      mIter.emplace(begin(mChildren));
      mIterEnd.emplace(end(mChildren));
    } else {
      count *= 2;  // XXX somewhat arbitrary estimate for now...
      mArray.emplace(count);
      for (Iterator i(begin(mChildren)), iEnd(end(mChildren)); i != iEnd; ++i) {
        mArray->AppendElement(*i);
      }
      auto comparator = (aOrderProp == eUseBoxOrdinalGroup)
                            ? CSSBoxOrdinalGroupComparator
                            : CSSOrderComparator;

      // XXX replace this with nsTArray::StableSort when bug 1147091 is fixed.
      std::stable_sort(mArray->begin(), mArray->end(), comparator);
    }

    if (mSkipPlaceholders) {
      SkipPlaceholders();
    }
  }
  ~CSSOrderAwareFrameIteratorT() {
    MOZ_ASSERT(IsForward() == mItemCount.isNothing());
  }

  bool IsForward() const;
  Iterator begin(const nsFrameList& aList);
  Iterator end(const nsFrameList& aList);

  nsIFrame* operator*() const {
    MOZ_ASSERT(!AtEnd());
    if (mIter.isSome()) {
      return **mIter;
    }
    return (*mArray)[mArrayIndex];
  }

  /**
   * Return the child index of the current item, placeholders not counted.
   * It's forbidden to call this method when the current frame is placeholder.
   */
  size_t ItemIndex() const {
    MOZ_ASSERT(!AtEnd());
    MOZ_ASSERT(!(**this)->IsPlaceholderFrame(),
               "MUST not call this when at a placeholder");
    MOZ_ASSERT(IsForward() || mItemIndex < *mItemCount,
               "Returning an out-of-range mItemIndex...");
    return mItemIndex;
  }

  void SetItemCount(size_t aItemCount) {
#ifndef CLANG_CRASH_BUG
    MOZ_ASSERT(mIter.isSome() || aItemCount <= mArray->Length(),
               "item count mismatch");
#endif
    mItemCount.emplace(aItemCount);
    // Note: it's OK if mItemIndex underflows -- ItemIndex()
    // will not be called unless there is at least one item.
    mItemIndex = IsForward() ? 0 : *mItemCount - 1;
  }

  /**
   * Skip over placeholder children.
   */
  void SkipPlaceholders() {
    if (mIter.isSome()) {
      for (; *mIter != *mIterEnd; ++*mIter) {
        nsIFrame* child = **mIter;
        if (!child->IsPlaceholderFrame()) {
          return;
        }
      }
    } else {
      for (; mArrayIndex < mArray->Length(); ++mArrayIndex) {
        nsIFrame* child = (*mArray)[mArrayIndex];
        if (!child->IsPlaceholderFrame()) {
          return;
        }
      }
    }
  }

  bool AtEnd() const {
#ifndef CLANG_CRASH_BUG
    // Clang 3.6.2 crashes when compiling this assertion:
    MOZ_ASSERT(mIter.isSome() || mArrayIndex <= mArray->Length());
#endif
    return mIter ? (*mIter == *mIterEnd) : mArrayIndex >= mArray->Length();
  }

  void Next() {
#ifdef DEBUG
    MOZ_ASSERT(!AtEnd());
    nsFrameList list = mContainer->GetChildList(mListID);
    MOZ_ASSERT(list.FirstChild() == mChildren.FirstChild() &&
                   list.LastChild() == mChildren.LastChild(),
               "the list of child frames must not change while iterating!");
#endif
    if (mSkipPlaceholders || !(**this)->IsPlaceholderFrame()) {
      IsForward() ? ++mItemIndex : --mItemIndex;
    }
    if (mIter.isSome()) {
      ++*mIter;
    } else {
      ++mArrayIndex;
    }
    if (mSkipPlaceholders) {
      SkipPlaceholders();
    }
  }

  void Reset(ChildFilter aFilter = eSkipPlaceholders) {
    if (mIter.isSome()) {
      mIter.reset();
      mIter.emplace(begin(mChildren));
      mIterEnd.reset();
      mIterEnd.emplace(end(mChildren));
    } else {
      mArrayIndex = 0;
    }
    mItemIndex = IsForward() ? 0 : *mItemCount - 1;
    mSkipPlaceholders = aFilter == eSkipPlaceholders;
    if (mSkipPlaceholders) {
      SkipPlaceholders();
    }
  }

  bool IsValid() const { return mIter.isSome() || mArray.isSome(); }

  void Invalidate() {
    mIter.reset();
    mArray.reset();
    mozWritePoison(&mChildren, sizeof(mChildren));
  }

  bool ItemsAreAlreadyInOrder() const { return mIter.isSome(); }

  static bool CSSOrderComparator(nsIFrame* const& a, nsIFrame* const& b);
  static bool CSSBoxOrdinalGroupComparator(nsIFrame* const& a,
                                           nsIFrame* const& b);

 private:
  nsFrameList mChildren;
  // Used if child list is already in ascending 'order'.
  Maybe<Iterator> mIter;
  Maybe<Iterator> mIterEnd;
  // Used if child list is *not* in ascending 'order'.
  // This array is pre-sorted in reverse order for a reverse iterator.
  Maybe<nsTArray<nsIFrame*>> mArray;
  size_t mArrayIndex;
  // The index of the current item (placeholders excluded).
  size_t mItemIndex;
  // The number of items (placeholders excluded).
  // It's only initialized and used in a reverse iterator.
  Maybe<size_t> mItemCount;
  // Skip placeholder children in the iteration?
  bool mSkipPlaceholders;
#ifdef DEBUG
  nsIFrame* mContainer;
  nsIFrame::ChildListID mListID;
#endif
};

typedef CSSOrderAwareFrameIteratorT<nsFrameList::iterator>
    CSSOrderAwareFrameIterator;
typedef CSSOrderAwareFrameIteratorT<nsFrameList::reverse_iterator>
    ReverseCSSOrderAwareFrameIterator;

}  // namespace mozilla

#endif  // mozilla_CSSOrderAwareFrameIterator_h
