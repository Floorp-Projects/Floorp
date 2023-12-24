/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameTraversal.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "nsFrameList.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsContainerFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PopoverData.h"

using namespace mozilla;
using namespace mozilla::dom;

// Bidi visual iterator
class nsVisualIterator : public nsFrameIterator {
 public:
  nsVisualIterator(nsPresContext* aPresContext, nsIFrame* aStart, Type aType,
                   bool aLockInScrollView, bool aFollowOOFs,
                   bool aSkipPopupChecks, nsIFrame* aLimiter)
      : nsFrameIterator(aPresContext, aStart, aType, aLockInScrollView,
                        aFollowOOFs, aSkipPopupChecks, aLimiter) {}

 protected:
  nsIFrame* GetFirstChildInner(nsIFrame* aFrame) override;
  nsIFrame* GetLastChildInner(nsIFrame* aFrame) override;

  nsIFrame* GetNextSiblingInner(nsIFrame* aFrame) override;
  nsIFrame* GetPrevSiblingInner(nsIFrame* aFrame) override;
};

/************IMPLEMENTATIONS**************/

// static
already_AddRefed<nsFrameIterator> nsFrameIterator::Create(
    nsPresContext* aPresContext, nsIFrame* aStart, Type aType, bool aVisual,
    bool aLockInScrollView, bool aFollowOOFs, bool aSkipPopupChecks,
    nsIFrame* aLimiter) {
  MOZ_ASSERT(aStart);

  if (aFollowOOFs) {
    aStart = nsPlaceholderFrame::GetRealFrameFor(aStart);
  }

  RefPtr<nsFrameIterator> iterator;
  if (aVisual) {
    iterator =
        new nsVisualIterator(aPresContext, aStart, aType, aLockInScrollView,
                             aFollowOOFs, aSkipPopupChecks, aLimiter);
  } else {
    iterator =
        new nsFrameIterator(aPresContext, aStart, aType, aLockInScrollView,
                            aFollowOOFs, aSkipPopupChecks, aLimiter);
  }
  return iterator.forget();
}

nsFrameIterator::nsFrameIterator(nsPresContext* aPresContext, nsIFrame* aStart,
                                 Type aType, bool aLockInScrollView,
                                 bool aFollowOOFs, bool aSkipPopupChecks,
                                 nsIFrame* aLimiter)
    : mPresContext(aPresContext),
      mLockScroll(aLockInScrollView),
      mFollowOOFs(aFollowOOFs),
      mSkipPopupChecks(aSkipPopupChecks),
      mType(aType),
      mStart(aStart),
      mCurrent(aStart),
      mLast(aStart),
      mLimiter(aLimiter),
      mOffEdge(0) {
  MOZ_ASSERT(!aFollowOOFs || !aStart->IsPlaceholderFrame(),
             "Caller should have resolved placeholder frame");
}

nsIFrame* nsFrameIterator::CurrentItem() {
  if (mOffEdge) return nullptr;

  return mCurrent;
}

bool nsFrameIterator::IsDone() { return mOffEdge != 0; }

void nsFrameIterator::First() { mCurrent = mStart; }

static bool IsRootFrame(nsIFrame* aFrame) { return aFrame->IsCanvasFrame(); }

void nsFrameIterator::Last() {
  nsIFrame* result;
  nsIFrame* parent = GetCurrent();
  // If the current frame is a popup, don't move farther up the tree.
  // Otherwise, get the nearest root frame or popup.
  if (mSkipPopupChecks || !parent->IsMenuPopupFrame()) {
    while (!IsRootFrame(parent) && (result = GetParentFrameNotPopup(parent)))
      parent = result;
  }

  while ((result = GetLastChild(parent))) {
    parent = result;
  }

  SetCurrent(parent);
  if (!parent) SetOffEdge(1);
}

void nsFrameIterator::Next() {
  // recursive-oid method to get next frame
  nsIFrame* result = nullptr;
  nsIFrame* parent = GetCurrent();
  if (!parent) parent = GetLast();

  if (mType == Type::Leaf) {
    // Drill down to first leaf
    while ((result = GetFirstChild(parent))) {
      parent = result;
    }
  } else if (mType == Type::PreOrder) {
    result = GetFirstChild(parent);
    if (result) parent = result;
  }

  if (parent != GetCurrent()) {
    result = parent;
  } else {
    while (parent) {
      result = GetNextSibling(parent);
      if (result) {
        if (mType != Type::PreOrder) {
          parent = result;
          while ((result = GetFirstChild(parent))) {
            parent = result;
          }
          result = parent;
        }
        break;
      }
      result = GetParentFrameNotPopup(parent);
      if (!result || IsRootFrame(result) ||
          (mLockScroll && result->IsScrollFrame())) {
        result = nullptr;
        break;
      }
      if (mType == Type::PostOrder) {
        break;
      }
      parent = result;
    }
  }

  SetCurrent(result);
  if (!result) {
    SetOffEdge(1);
    SetLast(parent);
  }
}

void nsFrameIterator::Prev() {
  // recursive-oid method to get prev frame
  nsIFrame* result = nullptr;
  nsIFrame* parent = GetCurrent();
  if (!parent) parent = GetLast();

  if (mType == Type::Leaf) {
    // Drill down to last leaf
    while ((result = GetLastChild(parent))) {
      parent = result;
    }
  } else if (mType == Type::PostOrder) {
    result = GetLastChild(parent);
    if (result) parent = result;
  }

  if (parent != GetCurrent()) {
    result = parent;
  } else {
    while (parent) {
      result = GetPrevSibling(parent);
      if (result) {
        if (mType != Type::PostOrder) {
          parent = result;
          while ((result = GetLastChild(parent))) {
            parent = result;
          }
          result = parent;
        }
        break;
      }
      result = GetParentFrameNotPopup(parent);
      if (!result || IsRootFrame(result) ||
          (mLockScroll && result->IsScrollFrame())) {
        result = nullptr;
        break;
      }
      if (mType == Type::PreOrder) {
        break;
      }
      parent = result;
    }
  }

  SetCurrent(result);
  if (!result) {
    SetOffEdge(-1);
    SetLast(parent);
  }
}

nsIFrame* nsFrameIterator::GetParentFrame(nsIFrame* aFrame) {
  if (mFollowOOFs) aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame == mLimiter) return nullptr;
  if (aFrame) return aFrame->GetParent();

  return nullptr;
}

nsIFrame* nsFrameIterator::GetParentFrameNotPopup(nsIFrame* aFrame) {
  if (mFollowOOFs) aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame == mLimiter) return nullptr;
  if (aFrame) {
    nsIFrame* parent = aFrame->GetParent();
    if (!IsPopupFrame(parent)) return parent;
  }

  return nullptr;
}

nsIFrame* nsFrameIterator::GetFirstChild(nsIFrame* aFrame) {
  nsIFrame* result = GetFirstChildInner(aFrame);
  if (mLockScroll && result && result->IsScrollFrame()) return nullptr;
  if (result && mFollowOOFs) {
    result = nsPlaceholderFrame::GetRealFrameFor(result);

    if (IsPopupFrame(result) || IsInvokerOpenPopoverFrame(result)) {
      result = GetNextSibling(result);
    }
  }

  return result;
}

nsIFrame* nsFrameIterator::GetLastChild(nsIFrame* aFrame) {
  nsIFrame* result = GetLastChildInner(aFrame);
  if (mLockScroll && result && result->IsScrollFrame()) return nullptr;
  if (result && mFollowOOFs) {
    result = nsPlaceholderFrame::GetRealFrameFor(result);

    if (IsPopupFrame(result) || IsInvokerOpenPopoverFrame(result)) {
      result = GetPrevSibling(result);
    }
  }

  return result;
}

nsIFrame* nsFrameIterator::GetNextSibling(nsIFrame* aFrame) {
  nsIFrame* result = nullptr;
  if (mFollowOOFs) aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame == mLimiter) return nullptr;
  if (aFrame) {
    result = GetNextSiblingInner(aFrame);
    if (result && mFollowOOFs) {
      result = nsPlaceholderFrame::GetRealFrameFor(result);
      if (IsPopupFrame(result) || IsInvokerOpenPopoverFrame(result)) {
        result = GetNextSibling(result);
      }
    }
  }

  return result;
}

nsIFrame* nsFrameIterator::GetPrevSibling(nsIFrame* aFrame) {
  nsIFrame* result = nullptr;
  if (mFollowOOFs) aFrame = GetPlaceholderFrame(aFrame);
  if (aFrame == mLimiter) return nullptr;
  if (aFrame) {
    result = GetPrevSiblingInner(aFrame);
    if (result && mFollowOOFs) {
      result = nsPlaceholderFrame::GetRealFrameFor(result);
      if (IsPopupFrame(result) || IsInvokerOpenPopoverFrame(result)) {
        result = GetPrevSibling(result);
      }
    }
  }

  return result;
}

nsIFrame* nsFrameIterator::GetFirstChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().FirstChild();
}

nsIFrame* nsFrameIterator::GetLastChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().LastChild();
}

nsIFrame* nsFrameIterator::GetNextSiblingInner(nsIFrame* aFrame) {
  return aFrame->GetNextSibling();
}

nsIFrame* nsFrameIterator::GetPrevSiblingInner(nsIFrame* aFrame) {
  return aFrame->GetPrevSibling();
}

nsIFrame* nsFrameIterator::GetPlaceholderFrame(nsIFrame* aFrame) {
  if (MOZ_LIKELY(!aFrame || !aFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW))) {
    return aFrame;
  }
  nsIFrame* placeholder = aFrame->GetPlaceholderFrame();
  return placeholder ? placeholder : aFrame;
}

bool nsFrameIterator::IsPopupFrame(nsIFrame* aFrame) {
  // If skipping popup checks, pretend this isn't one.
  if (mSkipPopupChecks) {
    return false;
  }
  return aFrame && aFrame->IsMenuPopupFrame();
}

bool nsFrameIterator::IsInvokerOpenPopoverFrame(nsIFrame* aFrame) {
  if (const nsIContent* currentContent = aFrame->GetContent()) {
    if (const auto* popover = Element::FromNode(currentContent)) {
      return popover && popover->IsPopoverOpen() &&
             popover->GetPopoverData()->GetInvoker();
    }
  }
  return false;
}

// nsVisualIterator implementation

nsIFrame* nsVisualIterator::GetFirstChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().GetNextVisualFor(nullptr);
}

nsIFrame* nsVisualIterator::GetLastChildInner(nsIFrame* aFrame) {
  return aFrame->PrincipalChildList().GetPrevVisualFor(nullptr);
}

nsIFrame* nsVisualIterator::GetNextSiblingInner(nsIFrame* aFrame) {
  nsIFrame* parent = GetParentFrame(aFrame);
  if (!parent) return nullptr;
  return parent->PrincipalChildList().GetNextVisualFor(aFrame);
}

nsIFrame* nsVisualIterator::GetPrevSiblingInner(nsIFrame* aFrame) {
  nsIFrame* parent = GetParentFrame(aFrame);
  if (!parent) return nullptr;
  return parent->PrincipalChildList().GetPrevVisualFor(aFrame);
}
