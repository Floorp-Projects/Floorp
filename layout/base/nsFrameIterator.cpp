/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameIterator.h"
#include "nsIFrame.h"
#include "nsGkAtoms.h"
#include "nsPlaceholderFrame.h"

nsFrameIterator::nsFrameIterator(nsPresContext* aPresContext, nsIFrame *aStart,
                                 nsIteratorType aType, PRUint32 aFlags)
  : mPresContext(aPresContext)
  , mOffEdge(0)
  , mType(aType)
{
  mFollowOOFs = (aFlags & FLAG_FOLLOW_OUT_OF_FLOW) != 0;
  mLockScroll = (aFlags & FLAG_LOCK_SCROLL) != 0;
  mVisual = (aFlags & FLAG_VISUAL) != 0;
  if (mFollowOOFs && aStart)
    aStart = nsPlaceholderFrame::GetRealFrameFor(aStart);
  setStart(aStart);
  setCurrent(aStart);
  setLast(aStart);
}

nsIFrame*
nsFrameIterator::CurrentItem()
{
  return mOffEdge ? nullptr : mCurrent;
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
            (mLockScroll && result->GetType() == nsGkAtoms::scrollFrame)) {
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
            (mLockScroll && result->GetType() == nsGkAtoms::scrollFrame)) {
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
  if (mLockScroll && result && result->GetType() == nsGkAtoms::scrollFrame)
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
  if (mLockScroll && result && result->GetType() == nsGkAtoms::scrollFrame)
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
  if (mVisual) {
    return aFrame->PrincipalChildList().GetNextVisualFor(nullptr);
  }
  return aFrame->GetFirstPrincipalChild();
}

nsIFrame*
nsFrameIterator::GetLastChildInner(nsIFrame* aFrame) {
  if (mVisual) {
    return aFrame->PrincipalChildList().GetPrevVisualFor(nullptr);
  }
  return aFrame->PrincipalChildList().LastChild();
}

nsIFrame*
nsFrameIterator::GetNextSiblingInner(nsIFrame* aFrame) {
  if (mVisual) {
    nsIFrame* parent = GetParentFrame(aFrame);
    if (!parent)
      return nullptr;
    return parent->PrincipalChildList().GetNextVisualFor(aFrame);
  }
  return aFrame->GetNextSibling();
}

nsIFrame*
nsFrameIterator::GetPrevSiblingInner(nsIFrame* aFrame) {
  if (mVisual) {
    nsIFrame* parent = GetParentFrame(aFrame);
    if (!parent)
      return nullptr;
    return parent->PrincipalChildList().GetPrevVisualFor(aFrame);
  }
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

