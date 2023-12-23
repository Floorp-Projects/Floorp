/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSFRAMETRAVERSAL_H
#define NSFRAMETRAVERSAL_H

#include "mozilla/Attributes.h"
#include "nsIFrameTraversal.h"

class nsIFrame;

class nsFrameIterator {
  NS_INLINE_DECL_REFCOUNTING(nsFrameIterator)

 public:
  void First();
  void Next();
  nsIFrame* CurrentItem();
  bool IsDone();

  void Last();
  void Prev();

  inline nsIFrame* Traverse(bool aForward) {
    if (aForward) {
      Next();
    } else {
      Prev();
    }
    return CurrentItem();
  };

  nsFrameIterator(nsPresContext* aPresContext, nsIFrame* aStart,
                  nsIteratorType aType, bool aLockScroll, bool aFollowOOFs,
                  bool aSkipPopupChecks, nsIFrame* aLimiter);

 protected:
  virtual ~nsFrameIterator() = default;

  void SetCurrent(nsIFrame* aFrame) { mCurrent = aFrame; }
  nsIFrame* GetCurrent() { return mCurrent; }
  nsIFrame* GetStart() { return mStart; }
  nsIFrame* GetLast() { return mLast; }
  void SetLast(nsIFrame* aFrame) { mLast = aFrame; }
  int8_t GetOffEdge() { return mOffEdge; }
  void SetOffEdge(int8_t aOffEdge) { mOffEdge = aOffEdge; }

  /*
   Our own versions of the standard frame tree navigation
   methods, which, if the iterator is following out-of-flows,
   apply the following rules for placeholder frames:

   - If a frame HAS a placeholder frame, getting its parent
   gets the placeholder's parent.

   - If a frame's first child or next/prev sibling IS a
   placeholder frame, then we instead return the real frame.

   - If a frame HAS a placeholder frame, getting its next/prev
   sibling gets the placeholder frame's next/prev sibling.

   These are all applied recursively to support multiple levels of
   placeholders.
   */

  nsIFrame* GetParentFrame(nsIFrame* aFrame);
  // like GetParentFrame but returns null once a popup frame is reached
  nsIFrame* GetParentFrameNotPopup(nsIFrame* aFrame);

  nsIFrame* GetFirstChild(nsIFrame* aFrame);
  nsIFrame* GetLastChild(nsIFrame* aFrame);

  nsIFrame* GetNextSibling(nsIFrame* aFrame);
  nsIFrame* GetPrevSibling(nsIFrame* aFrame);

  /*
   These methods are overridden by the bidi visual iterator to have the
   semantics of "get first child in visual order", "get last child in visual
   order", "get next sibling in visual order" and "get previous sibling in
   visual order".
  */

  virtual nsIFrame* GetFirstChildInner(nsIFrame* aFrame);
  virtual nsIFrame* GetLastChildInner(nsIFrame* aFrame);

  virtual nsIFrame* GetNextSiblingInner(nsIFrame* aFrame);
  virtual nsIFrame* GetPrevSiblingInner(nsIFrame* aFrame);

  /**
   * Return the placeholder frame for aFrame if it has one, otherwise return
   * aFrame itself.
   */
  nsIFrame* GetPlaceholderFrame(nsIFrame* aFrame);
  bool IsPopupFrame(nsIFrame* aFrame);

  bool IsInvokerOpenPopoverFrame(nsIFrame* aFrame);

  nsPresContext* const mPresContext;
  const bool mLockScroll;
  const bool mFollowOOFs;
  const bool mSkipPopupChecks;
  const nsIteratorType mType;

 private:
  nsIFrame* const mStart;
  nsIFrame* mCurrent;
  nsIFrame* mLast;  // the last one that was in current;
  nsIFrame* mLimiter;
  int8_t mOffEdge;  // 0= no -1 to far prev, 1 to far next;
};

nsresult NS_NewFrameTraversal(nsFrameIterator** aEnumerator,
                              nsPresContext* aPresContext, nsIFrame* aStart,
                              nsIteratorType aType, bool aVisual,
                              bool aLockInScrollView, bool aFollowOOFs,
                              bool aSkipPopupChecks,
                              nsIFrame* aLimiter = nullptr);

nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);

class nsFrameTraversal final : public nsIFrameTraversal {
 public:
  nsFrameTraversal();

  NS_DECL_ISUPPORTS

  NS_IMETHOD NewFrameTraversal(nsFrameIterator** aEnumerator,
                               nsPresContext* aPresContext, nsIFrame* aStart,
                               int32_t aType, bool aVisual,
                               bool aLockInScrollView, bool aFollowOOFs,
                               bool aSkipPopupChecks,
                               nsIFrame* aLimiter = nullptr) override;

 protected:
  virtual ~nsFrameTraversal();
};

#endif  // NSFRAMETRAVERSAL_H
