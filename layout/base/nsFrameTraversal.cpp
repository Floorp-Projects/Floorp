/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "nsFrameTraversal.h"
#include "nsFrameList.h"
#include "nsPlaceholderFrame.h"
#include "nsContainerFrame.h"


class nsFrameIterator : public nsIFrameEnumerator
{
public:
  typedef nsIFrame::ChildListID ChildListID;

  NS_DECL_ISUPPORTS

  virtual void First() override;
  virtual void Next() override;
  virtual nsIFrame* CurrentItem() override;
  virtual bool IsDone() override;

  virtual void Last() override;
  virtual void Prev() override;

  nsFrameIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                  nsIteratorType aType, bool aLockScroll, bool aFollowOOFs,
                  bool aSkipPopupChecks);

protected:
  virtual ~nsFrameIterator() {}

  void      setCurrent(nsIFrame *aFrame){mCurrent = aFrame;}
  nsIFrame *getCurrent(){return mCurrent;}
  nsIFrame *getStart(){return mStart;}
  nsIFrame *getLast(){return mLast;}
  void      setLast(nsIFrame *aFrame){mLast = aFrame;}
  int8_t    getOffEdge(){return mOffEdge;}
  void      setOffEdge(int8_t aOffEdge){mOffEdge = aOffEdge;}

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
   order", "get next sibling in visual order" and "get previous sibling in visual
   order".
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
  bool      IsPopupFrame(nsIFrame* aFrame);

  nsPresContext* const mPresContext;
  const bool mLockScroll;
  const bool mFollowOOFs;
  const bool mSkipPopupChecks;
  const nsIteratorType mType;

private:
  nsIFrame* const mStart;
  nsIFrame* mCurrent;
  nsIFrame* mLast; //the last one that was in current;
  int8_t    mOffEdge; //0= no -1 to far prev, 1 to far next;
};



// Bidi visual iterator
class nsVisualIterator: public nsFrameIterator
{
public:
  nsVisualIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                   nsIteratorType aType, bool aLockScroll,
                   bool aFollowOOFs, bool aSkipPopupChecks) :
  nsFrameIterator(aPresContext, aStart, aType, aLockScroll, aFollowOOFs, aSkipPopupChecks) {}

protected:
  nsIFrame* GetFirstChildInner(nsIFrame* aFrame) override;
  nsIFrame* GetLastChildInner(nsIFrame* aFrame) override;  
  
  nsIFrame* GetNextSiblingInner(nsIFrame* aFrame) override;
  nsIFrame* GetPrevSiblingInner(nsIFrame* aFrame) override;  
};

/************IMPLEMENTATIONS**************/

nsresult
NS_CreateFrameTraversal(nsIFrameTraversal** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsIFrameTraversal> t = new nsFrameTraversal();
  t.forget(aResult);

  return NS_OK;
}

nsresult
NS_NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                     nsPresContext* aPresContext,
                     nsIFrame *aStart,
                     nsIteratorType aType,
                     bool aVisual,
                     bool aLockInScrollView,
                     bool aFollowOOFs,
                     bool aSkipPopupChecks)
{
  if (!aEnumerator || !aStart)
    return NS_ERROR_NULL_POINTER;

  if (aFollowOOFs) {
    aStart = nsPlaceholderFrame::GetRealFrameFor(aStart);
  }

  nsCOMPtr<nsIFrameEnumerator> trav;
  if (aVisual) {
    trav = new nsVisualIterator(aPresContext, aStart, aType,
                                aLockInScrollView, aFollowOOFs, aSkipPopupChecks);
  } else {
    trav = new nsFrameIterator(aPresContext, aStart, aType,
                               aLockInScrollView, aFollowOOFs, aSkipPopupChecks);
  }
  trav.forget(aEnumerator);
  return NS_OK;
}


nsFrameTraversal::nsFrameTraversal()
{
}

nsFrameTraversal::~nsFrameTraversal()
{
}

NS_IMPL_ISUPPORTS(nsFrameTraversal,nsIFrameTraversal)

NS_IMETHODIMP 
 nsFrameTraversal::NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                                     nsPresContext* aPresContext,
                                     nsIFrame *aStart,
                                     int32_t aType,
                                     bool aVisual,
                                     bool aLockInScrollView,
                                     bool aFollowOOFs,
                                     bool aSkipPopupChecks)
{
  return NS_NewFrameTraversal(aEnumerator, aPresContext, aStart,
                              static_cast<nsIteratorType>(aType),
                              aVisual, aLockInScrollView, aFollowOOFs, aSkipPopupChecks);  
}

// nsFrameIterator implementation

NS_IMPL_ISUPPORTS(nsFrameIterator, nsIFrameEnumerator)

nsFrameIterator::nsFrameIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                                 nsIteratorType aType, bool aLockInScrollView,
                                 bool aFollowOOFs, bool aSkipPopupChecks)
: mPresContext(aPresContext),
  mLockScroll(aLockInScrollView),
  mFollowOOFs(aFollowOOFs),
  mSkipPopupChecks(aSkipPopupChecks),
  mType(aType),
  mStart(aStart),
  mCurrent(aStart),
  mLast(aStart),
  mOffEdge(0)
{
  MOZ_ASSERT(!aFollowOOFs || !aStart->IsPlaceholderFrame(),
             "Caller should have resolved placeholder frame");
}



nsIFrame*
nsFrameIterator::CurrentItem()
{
  if (mOffEdge)
    return nullptr;

  return mCurrent;
}



bool
nsFrameIterator::IsDone()
{
  return mOffEdge != 0;
}

void
nsFrameIterator::First()
{
  mCurrent = mStart;
}

static bool
IsRootFrame(nsIFrame* aFrame)
{
  return aFrame->IsCanvasFrame() || aFrame->IsRootFrame();
}

void
nsFrameIterator::Last()
{
  nsIFrame* result;
  nsIFrame* parent = getCurrent();
  // If the current frame is a popup, don't move farther up the tree.
  // Otherwise, get the nearest root frame or popup.
  if (mSkipPopupChecks || !parent->IsMenuPopupFrame()) {
    while (!IsRootFrame(parent) && (result = GetParentFrameNotPopup(parent)))
      parent = result;
  }

  while ((result = GetLastChild(parent))) {
    parent = result;
  }

  setCurrent(parent);
  if (!parent)
    setOffEdge(1);
}

void
nsFrameIterator::Next()
{
  // recursive-oid method to get next frame
  nsIFrame *result = nullptr;
  nsIFrame *parent = getCurrent();
  if (!parent)
    parent = getLast();

  if (mType == eLeaf) {
    // Drill down to first leaf
    while ((result = GetFirstChild(parent))) {
      parent = result;
    }
  } else if (mType == ePreOrder) {
    result = GetFirstChild(parent);
    if (result)
      parent = result;
  }

  if (parent != getCurrent()) {
    result = parent;
  } else {
    while (parent) {
      result = GetNextSibling(parent);
      if (result) {
        if (mType != ePreOrder) {
          parent = result;
          while ((result = GetFirstChild(parent))) {
            parent = result;
          }
          result = parent;
        }
        break;
      }
      else {
        result = GetParentFrameNotPopup(parent);
        if (!result || IsRootFrame(result) ||
            (mLockScroll && result->IsScrollFrame())) {
          result = nullptr;
          break;
        }
        if (mType == ePostOrder)
          break;
        parent = result;
      }
    }
  }

  setCurrent(result);
  if (!result) {
    setOffEdge(1);
    setLast(parent);
  }
}

void
nsFrameIterator::Prev()
{
  // recursive-oid method to get prev frame
  nsIFrame *result = nullptr;
  nsIFrame *parent = getCurrent();
  if (!parent)
    parent = getLast();

  if (mType == eLeaf) {
    // Drill down to last leaf
    while ((result = GetLastChild(parent))) {
      parent = result;
    }
  } else if (mType == ePostOrder) {
    result = GetLastChild(parent);
    if (result)
      parent = result;
  }
  
  if (parent != getCurrent()) {
    result = parent;
  } else {
    while (parent) {
      result = GetPrevSibling(parent);
      if (result) {
        if (mType != ePostOrder) {
          parent = result;
          while ((result = GetLastChild(parent))) {
            parent = result;
          }
          result = parent;
        }
        break;
      } else {
        result = GetParentFrameNotPopup(parent);
        if (!result || IsRootFrame(result) ||
            (mLockScroll && result->IsScrollFrame())) {
          result = nullptr;
          break;
        }
        if (mType == ePreOrder)
          break;
        parent = result;
      }
    }
  }

  setCurrent(result);
  if (!result) {
    setOffEdge(-1);
    setLast(parent);
  }
}

nsIFrame*
nsFrameIterator::GetParentFrame(nsIFrame* aFrame)
{
  if (mFollowOOFs)
    aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame)
    return aFrame->GetParent();
  
  return nullptr;
}

nsIFrame*
nsFrameIterator::GetParentFrameNotPopup(nsIFrame* aFrame)
{
  if (mFollowOOFs)
    aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame) {
    nsIFrame* parent = aFrame->GetParent();
    if (!IsPopupFrame(parent))
      return parent;
  }
    
  return nullptr;
}

nsIFrame*
nsFrameIterator::GetFirstChild(nsIFrame* aFrame)
{
  nsIFrame* result = GetFirstChildInner(aFrame);
  if (mLockScroll && result && result->IsScrollFrame())
    return nullptr;
  if (result && mFollowOOFs) {
    result = nsPlaceholderFrame::GetRealFrameFor(result);
    
    if (IsPopupFrame(result))
      result = GetNextSibling(result);
  }
  return result;
}

nsIFrame*
nsFrameIterator::GetLastChild(nsIFrame* aFrame)
{
  nsIFrame* result = GetLastChildInner(aFrame);
  if (mLockScroll && result && result->IsScrollFrame())
    return nullptr;
  if (result && mFollowOOFs) {
    result = nsPlaceholderFrame::GetRealFrameFor(result);
    
    if (IsPopupFrame(result))
      result = GetPrevSibling(result);
  }
  return result;
}

nsIFrame*
nsFrameIterator::GetNextSibling(nsIFrame* aFrame)
{
  nsIFrame* result = nullptr;
  if (mFollowOOFs)
    aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame) {
    result = GetNextSiblingInner(aFrame);
    if (result && mFollowOOFs)
      result = nsPlaceholderFrame::GetRealFrameFor(result);
  }

  if (mFollowOOFs && IsPopupFrame(result))
    result = GetNextSibling(result);

  return result;
}

nsIFrame*
nsFrameIterator::GetPrevSibling(nsIFrame* aFrame)
{
  nsIFrame* result = nullptr;
  if (mFollowOOFs)
    aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame) {
    result = GetPrevSiblingInner(aFrame);
    if (result && mFollowOOFs)
      result = nsPlaceholderFrame::GetRealFrameFor(result);
  }

  if (mFollowOOFs && IsPopupFrame(result))
    result = GetPrevSibling(result);

  return result;
}

nsIFrame*
nsFrameIterator::GetFirstChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().FirstChild();
}

nsIFrame*
nsFrameIterator::GetLastChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().LastChild();
}

nsIFrame*
nsFrameIterator::GetNextSiblingInner(nsIFrame* aFrame) {
  return aFrame->GetNextSibling();
}

nsIFrame*
nsFrameIterator::GetPrevSiblingInner(nsIFrame* aFrame) {
  return aFrame->GetPrevSibling();
}


nsIFrame*
nsFrameIterator::GetPlaceholderFrame(nsIFrame* aFrame)
{
  if (MOZ_LIKELY(!aFrame || !aFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW))) {
    return aFrame;
  }
  nsIFrame* placeholder =
    aFrame->PresContext()->PresShell()->GetPlaceholderFrameFor(aFrame);
  return placeholder ? placeholder : aFrame;
}

bool
nsFrameIterator::IsPopupFrame(nsIFrame* aFrame)
{
  // If skipping popup checks, pretend this isn't one.
  if (mSkipPopupChecks) {
    return false;
  }

  return (aFrame &&
          aFrame->StyleDisplay()->mDisplay == StyleDisplay::MozPopup);
}

// nsVisualIterator implementation

nsIFrame*
nsVisualIterator::GetFirstChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().GetNextVisualFor(nullptr);
}

nsIFrame*
nsVisualIterator::GetLastChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().GetPrevVisualFor(nullptr);
}

nsIFrame*
nsVisualIterator::GetNextSiblingInner(nsIFrame* aFrame) {
  nsIFrame* parent = GetParentFrame(aFrame);
  if (!parent)
    return nullptr;
  return parent->PrincipalChildList().GetNextVisualFor(aFrame);
}

nsIFrame*
nsVisualIterator::GetPrevSiblingInner(nsIFrame* aFrame) {
  nsIFrame* parent = GetParentFrame(aFrame);
  if (!parent)
    return nullptr;
  return parent->PrincipalChildList().GetPrevVisualFor(aFrame);
}
