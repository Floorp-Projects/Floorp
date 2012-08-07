/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSFRAMEITERATOR_H
#define NSFRAMEITERATOR_H

#include "prtypes.h"

class nsPresContext;
class nsIFrame;

enum nsIteratorType {
  eLeaf,
  ePreOrder,
  ePostOrder
};

class nsFrameIterator
{
public:
  nsFrameIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                  nsIteratorType aType, PRUint32 aFlags);

  ~nsFrameIterator() {}

  void First();
  void Next();
  nsIFrame* CurrentItem();
  bool IsDone();

  void Last();
  void Prev();

  enum FrameIteratorFlags {
    FLAG_NONE = 0,
    FLAG_LOCK_SCROLL = 1 << 1,
    FLAG_FOLLOW_OUT_OF_FLOW = 1 << 2,
    FLAG_VISUAL = 1 << 3
  };
protected:
  void      setCurrent(nsIFrame *aFrame){mCurrent = aFrame;}
  nsIFrame *getCurrent(){return mCurrent;}
  void      setStart(nsIFrame *aFrame){mStart = aFrame;}
  nsIFrame *getStart(){return mStart;}
  nsIFrame *getLast(){return mLast;}
  void      setLast(nsIFrame *aFrame){mLast = aFrame;}
  PRInt8    getOffEdge(){return mOffEdge;}
  void      setOffEdge(PRInt8 aOffEdge){mOffEdge = aOffEdge;}
  void      SetLockInScrollView(bool aLockScroll){mLockScroll = aLockScroll;}

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
   These methods are different in visual mode to have the
   semantics of "get first child in visual order", "get last child in visual
   order", "get next sibling in visual order" and "get previous sibling in visual
   order".
  */

  nsIFrame* GetFirstChildInner(nsIFrame* aFrame);
  nsIFrame* GetLastChildInner(nsIFrame* aFrame);

  nsIFrame* GetNextSiblingInner(nsIFrame* aFrame);
  nsIFrame* GetPrevSiblingInner(nsIFrame* aFrame);

  nsIFrame* GetPlaceholderFrame(nsIFrame* aFrame);
  bool      IsPopupFrame(nsIFrame* aFrame);

  nsPresContext* mPresContext;
  nsIFrame *mStart;
  nsIFrame *mCurrent;
  nsIFrame *mLast; //the last one that was in current;
  PRInt8    mOffEdge; //0= no -1 to far prev, 1 to far next;
  nsIteratorType mType;
  bool mLockScroll;
  bool mFollowOOFs;
  bool mVisual;
};

#endif //NSFRAMEITERATOR_H
