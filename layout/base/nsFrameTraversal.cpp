/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "nsFrameTraversal.h"
#include "nsFrameList.h"
#include "nsPlaceholderFrame.h"


class nsFrameIterator : public nsIFrameEnumerator
{
public:
  typedef nsIFrame::ChildListID ChildListID;

  NS_DECL_ISUPPORTS

  virtual void First();
  virtual void Next();
  virtual nsIFrame* CurrentItem();
  virtual bool IsDone();

  virtual void Last();
  virtual void Prev();

  nsFrameIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                  nsIteratorType aType, bool aLockScroll, bool aFollowOOFs);

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
   These methods are overridden by the bidi visual iterator to have the
   semantics of "get first child in visual order", "get last child in visual
   order", "get next sibling in visual order" and "get previous sibling in visual
   order".
  */
  
  virtual nsIFrame* GetFirstChildInner(nsIFrame* aFrame);
  virtual nsIFrame* GetLastChildInner(nsIFrame* aFrame);  

  virtual nsIFrame* GetNextSiblingInner(nsIFrame* aFrame);
  virtual nsIFrame* GetPrevSiblingInner(nsIFrame* aFrame);

  nsIFrame* GetPlaceholderFrame(nsIFrame* aFrame);
  bool      IsPopupFrame(nsIFrame* aFrame);

  nsPresContext* mPresContext;
  bool mLockScroll;
  bool mFollowOOFs;
  nsIteratorType mType;

private:
  nsIFrame *mStart;
  nsIFrame *mCurrent;
  nsIFrame *mLast; //the last one that was in current;
  PRInt8    mOffEdge; //0= no -1 to far prev, 1 to far next;
};



// Bidi visual iterator
class nsVisualIterator: public nsFrameIterator
{
public:
  nsVisualIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                   nsIteratorType aType, bool aLockScroll, bool aFollowOOFs) :
  nsFrameIterator(aPresContext, aStart, aType, aLockScroll, aFollowOOFs) {}

protected:
  nsIFrame* GetFirstChildInner(nsIFrame* aFrame);
  nsIFrame* GetLastChildInner(nsIFrame* aFrame);  
  
  nsIFrame* GetNextSiblingInner(nsIFrame* aFrame);
  nsIFrame* GetPrevSiblingInner(nsIFrame* aFrame);  
};

/************IMPLEMENTATIONS**************/

nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  nsCOMPtr<nsIFrameTraversal> t(new nsFrameTraversal());

  *aResult = t;
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult
NS_NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                     nsPresContext* aPresContext,
                     nsIFrame *aStart,
                     nsIteratorType aType,
                     bool aVisual,
                     bool aLockInScrollView,
                     bool aFollowOOFs)
{
  if (!aEnumerator || !aStart)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIFrameEnumerator> trav;
  if (aVisual) {
    trav = new nsVisualIterator(aPresContext, aStart, aType,
                                aLockInScrollView, aFollowOOFs);
  } else {
    trav = new nsFrameIterator(aPresContext, aStart, aType,
                               aLockInScrollView, aFollowOOFs);
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

NS_IMPL_ISUPPORTS1(nsFrameTraversal,nsIFrameTraversal)

NS_IMETHODIMP 
 nsFrameTraversal::NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                                     nsPresContext* aPresContext,
                                     nsIFrame *aStart,
                                     PRInt32 aType,
                                     bool aVisual,
                                     bool aLockInScrollView,
                                     bool aFollowOOFs)
{
  return NS_NewFrameTraversal(aEnumerator, aPresContext, aStart,
                              static_cast<nsIteratorType>(aType),
                              aVisual, aLockInScrollView, aFollowOOFs);  
}

// nsFrameIterator implementation

NS_IMPL_ISUPPORTS1(nsFrameIterator, nsIFrameEnumerator)

nsFrameIterator::nsFrameIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                                 nsIteratorType aType, bool aLockInScrollView,
                                 bool aFollowOOFs)
{
  mOffEdge = 0;
  mPresContext = aPresContext;
  if (aFollowOOFs && aStart)
    aStart = nsPlaceholderFrame::GetRealFrameFor(aStart);
  setStart(aStart);
  setCurrent(aStart);
  setLast(aStart);
  mType = aType;
  SetLockInScrollView(aLockInScrollView);
  mFollowOOFs = aFollowOOFs;
}



nsIFrame*
nsFrameIterator::CurrentItem()
{
  if (mOffEdge)
    return nsnull;

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
  nsIAtom* atom = aFrame->GetType();
  return (atom == nsGkAtoms::canvasFrame) ||
         (atom == nsGkAtoms::rootFrame);
}

void
nsFrameIterator::Last()
{
  nsIFrame* result;
  nsIFrame* parent = getCurrent();
  // If the current frame is a popup, don't move farther up the tree.
  // Otherwise, get the nearest root frame or popup.
  if (parent->GetType() != nsGkAtoms::menuPopupFrame) {
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
  nsIFrame *result = nsnull;
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
            (mLockScroll && result->GetType() == nsGkAtoms::scrollFrame)) {
          result = nsnull;
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
  nsIFrame *result = nsnull;
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
            (mLockScroll && result->GetType() == nsGkAtoms::scrollFrame)) {
          result = nsnull;
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
  
  return nsnull;
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
    
  return nsnull;
}

nsIFrame*
nsFrameIterator::GetFirstChild(nsIFrame* aFrame)
{
  nsIFrame* result = GetFirstChildInner(aFrame);
  if (mLockScroll && result && result->GetType() == nsGkAtoms::scrollFrame)
    return nsnull;
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
  if (mLockScroll && result && result->GetType() == nsGkAtoms::scrollFrame)
    return nsnull;
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
  nsIFrame* result = nsnull;
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
  nsIFrame* result = nsnull;
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
  return aFrame->GetFirstPrincipalChild();
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
  nsIFrame* result = aFrame;
  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    nsIFrame* placeholder = presShell->GetPlaceholderFrameFor(aFrame);
    if (placeholder)
      result = placeholder;
  }

  if (result != aFrame)
    result = GetPlaceholderFrame(result);

  return result;
}

bool
nsFrameIterator::IsPopupFrame(nsIFrame* aFrame)
{
  return (aFrame &&
          aFrame->GetStyleDisplay()->mDisplay == NS_STYLE_DISPLAY_POPUP);
}

// nsVisualIterator implementation

nsIFrame*
nsVisualIterator::GetFirstChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().GetNextVisualFor(nsnull);
}

nsIFrame*
nsVisualIterator::GetLastChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().GetPrevVisualFor(nsnull);
}

nsIFrame*
nsVisualIterator::GetNextSiblingInner(nsIFrame* aFrame) {
  nsIFrame* parent = GetParentFrame(aFrame);
  if (!parent)
    return nsnull;
  return parent->PrincipalChildList().GetNextVisualFor(aFrame);
}

nsIFrame*
nsVisualIterator::GetPrevSiblingInner(nsIFrame* aFrame) {
  nsIFrame* parent = GetParentFrame(aFrame);
  if (!parent)
    return nsnull;
  return parent->PrincipalChildList().GetPrevVisualFor(aFrame);
}
