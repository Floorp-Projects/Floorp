/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class #1 for rendering objects that have child lists */

#include "nsContainerFrame.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLSummaryElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsFlexContainerFrame.h"
#include "nsFrameSelection.h"
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsStyleConsts.h"
#include "nsView.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsViewManager.h"
#include "nsIWidget.h"
#include "nsCSSRendering.h"
#include "nsError.h"
#include "nsDisplayList.h"
#include "nsIBaseWindow.h"
#include "nsBoxLayoutState.h"
#include "nsCSSFrameConstructor.h"
#include "nsBlockFrame.h"
#include "nsBulletFrame.h"
#include "nsPlaceholderFrame.h"
#include "mozilla/AutoRestore.h"
#include "nsIFrameInlines.h"
#include "nsPrintfCString.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

using mozilla::gfx::ColorPattern;
using mozilla::gfx::DeviceColor;
using mozilla::gfx::Rect;
using mozilla::gfx::sRGBColor;
using mozilla::gfx::ToDeviceColor;

nsContainerFrame::~nsContainerFrame() = default;

NS_QUERYFRAME_HEAD(nsContainerFrame)
  NS_QUERYFRAME_ENTRY(nsContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSplittableFrame)

void nsContainerFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsSplittableFrame::Init(aContent, aParent, aPrevInFlow);
  if (aPrevInFlow) {
    // Make sure we copy bits from our prev-in-flow that will affect
    // us. A continuation for a container frame needs to know if it
    // has a child with a view so that we'll properly reposition it.
    if (aPrevInFlow->HasAnyStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW)) {
      AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
    }
  }
}

void nsContainerFrame::SetInitialChildList(ChildListID aListID,
                                           nsFrameList& aChildList) {
#ifdef DEBUG
  nsIFrame::VerifyDirtyBitSet(aChildList);
  for (nsIFrame* f : aChildList) {
    MOZ_ASSERT(f->GetParent() == this, "Unexpected parent");
  }
#endif
  if (aListID == kPrincipalList) {
    MOZ_ASSERT(mFrames.IsEmpty(),
               "unexpected second call to SetInitialChildList");
    mFrames.SetFrames(aChildList);
  } else if (aListID == kBackdropList) {
    MOZ_ASSERT(StyleDisplay()->mTopLayer != StyleTopLayer::None,
               "Only top layer frames should have backdrop");
    MOZ_ASSERT(HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
               "Top layer frames should be out-of-flow");
    MOZ_ASSERT(!GetProperty(BackdropProperty()),
               "We shouldn't have setup backdrop frame list before");
#ifdef DEBUG
    {
      nsIFrame* placeholder = aChildList.FirstChild();
      MOZ_ASSERT(aChildList.OnlyChild(), "Should have only one backdrop");
      MOZ_ASSERT(placeholder->IsPlaceholderFrame(),
                 "The frame to be stored should be a placeholder");
      MOZ_ASSERT(static_cast<nsPlaceholderFrame*>(placeholder)
                     ->GetOutOfFlowFrame()
                     ->IsBackdropFrame(),
                 "The placeholder should points to a backdrop frame");
    }
#endif
    nsFrameList* list = new (PresShell()) nsFrameList(aChildList);
    SetProperty(BackdropProperty(), list);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected child list");
  }
}

void nsContainerFrame::AppendFrames(ChildListID aListID,
                                    nsFrameList& aFrameList) {
  MOZ_ASSERT(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
             "unexpected child list");

  if (MOZ_UNLIKELY(aFrameList.IsEmpty())) {
    return;
  }

  DrainSelfOverflowList();  // ensure the last frame is in mFrames
  mFrames.AppendFrames(this, aFrameList);

  if (aListID != kNoReflowPrincipalList) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
  }
}

void nsContainerFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                                    const nsLineList::iterator* aPrevFrameLine,
                                    nsFrameList& aFrameList) {
  MOZ_ASSERT(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
             "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if (MOZ_UNLIKELY(aFrameList.IsEmpty())) {
    return;
  }

  DrainSelfOverflowList();  // ensure aPrevFrame is in mFrames
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  if (aListID != kNoReflowPrincipalList) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
  }
}

void nsContainerFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  MOZ_ASSERT(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
             "unexpected child list");

  AutoTArray<nsIFrame*, 10> continuations;
  {
    nsIFrame* continuation = aOldFrame;
    while (continuation) {
      continuations.AppendElement(continuation);
      continuation = continuation->GetNextContinuation();
    }
  }

  mozilla::PresShell* presShell = PresShell();
  nsContainerFrame* lastParent = nullptr;

  // Loop and destroy aOldFrame and all of its continuations.
  //
  // Request a reflow on the parent frames involved unless we were explicitly
  // told not to (kNoReflowPrincipalList).
  const bool generateReflowCommand = (kNoReflowPrincipalList != aListID);
  for (nsIFrame* continuation : Reversed(continuations)) {
    nsContainerFrame* parent = continuation->GetParent();

    // Please note that 'parent' may not actually be where 'continuation' lives.
    // We really MUST use StealFrame() and nothing else here.
    // @see nsInlineFrame::StealFrame for details.
    parent->StealFrame(continuation);
    continuation->Destroy();
    if (generateReflowCommand && parent != lastParent) {
      presShell->FrameNeedsReflow(parent, IntrinsicDirty::TreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
      lastParent = parent;
    }
  }
}

void nsContainerFrame::DestroyAbsoluteFrames(
    nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) {
  if (IsAbsoluteContainer()) {
    GetAbsoluteContainingBlock()->DestroyFrames(this, aDestructRoot,
                                                aPostDestroyData);
    MarkAsNotAbsoluteContainingBlock();
  }
}

void nsContainerFrame::SafelyDestroyFrameListProp(
    nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData,
    mozilla::PresShell* aPresShell, FrameListPropertyDescriptor aProp) {
  // Note that the last frame can be removed through another route and thus
  // delete the property -- that's why we fetch the property again before
  // removing each frame rather than fetching it once and iterating the list.
  while (nsFrameList* frameList = GetProperty(aProp)) {
    nsIFrame* frame = frameList->RemoveFirstChild();
    if (MOZ_LIKELY(frame)) {
      frame->DestroyFrom(aDestructRoot, aPostDestroyData);
    } else {
      Unused << TakeProperty(aProp);
      frameList->Delete(aPresShell);
      return;
    }
  }
}

void nsContainerFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                   PostDestroyData& aPostDestroyData) {
  // Prevent event dispatch during destruction.
  if (HasView()) {
    GetView()->SetFrame(nullptr);
  }

  DestroyAbsoluteFrames(aDestructRoot, aPostDestroyData);

  // Destroy frames on the principal child list.
  mFrames.DestroyFramesFrom(aDestructRoot, aPostDestroyData);

  // If we have any IB split siblings, clear their references to us.
  if (HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT)) {
    // Delete previous sibling's reference to me.
    if (nsIFrame* prevSib = GetProperty(nsIFrame::IBSplitPrevSibling())) {
      NS_WARNING_ASSERTION(
          this == prevSib->GetProperty(nsIFrame::IBSplitSibling()),
          "IB sibling chain is inconsistent");
      prevSib->RemoveProperty(nsIFrame::IBSplitSibling());
    }

    // Delete next sibling's reference to me.
    if (nsIFrame* nextSib = GetProperty(nsIFrame::IBSplitSibling())) {
      NS_WARNING_ASSERTION(
          this == nextSib->GetProperty(nsIFrame::IBSplitPrevSibling()),
          "IB sibling chain is inconsistent");
      nextSib->RemoveProperty(nsIFrame::IBSplitPrevSibling());
    }

#ifdef DEBUG
    // This is just so we can assert it's not set in nsIFrame::DestroyFrom.
    RemoveStateBits(NS_FRAME_PART_OF_IBSPLIT);
#endif
  }

  if (MOZ_UNLIKELY(!mProperties.IsEmpty())) {
    using T = mozilla::FrameProperties::UntypedDescriptor;
    bool hasO = false, hasOC = false, hasEOC = false, hasBackdrop = false;
    mProperties.ForEach([&](const T& aProp, void*) {
      if (aProp == OverflowProperty()) {
        hasO = true;
      } else if (aProp == OverflowContainersProperty()) {
        hasOC = true;
      } else if (aProp == ExcessOverflowContainersProperty()) {
        hasEOC = true;
      } else if (aProp == BackdropProperty()) {
        hasBackdrop = true;
      }
      return true;
    });

    // Destroy frames on the auxiliary frame lists and delete the lists.
    nsPresContext* pc = PresContext();
    mozilla::PresShell* presShell = pc->PresShell();
    if (hasO) {
      SafelyDestroyFrameListProp(aDestructRoot, aPostDestroyData, presShell,
                                 OverflowProperty());
    }

    MOZ_ASSERT(
        IsFrameOfType(eCanContainOverflowContainers) || !(hasOC || hasEOC),
        "this type of frame shouldn't have overflow containers");
    if (hasOC) {
      SafelyDestroyFrameListProp(aDestructRoot, aPostDestroyData, presShell,
                                 OverflowContainersProperty());
    }
    if (hasEOC) {
      SafelyDestroyFrameListProp(aDestructRoot, aPostDestroyData, presShell,
                                 ExcessOverflowContainersProperty());
    }

    MOZ_ASSERT(!GetProperty(BackdropProperty()) ||
                   StyleDisplay()->mTopLayer != StyleTopLayer::None,
               "only top layer frame may have backdrop");
    if (hasBackdrop) {
      SafelyDestroyFrameListProp(aDestructRoot, aPostDestroyData, presShell,
                                 BackdropProperty());
    }
  }

  nsSplittableFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

const nsFrameList& nsContainerFrame::GetChildList(ChildListID aListID) const {
  // We only know about the principal child list, the overflow lists,
  // and the backdrop list.
  switch (aListID) {
    case kPrincipalList:
      return mFrames;
    case kOverflowList: {
      nsFrameList* list = GetOverflowFrames();
      return list ? *list : nsFrameList::EmptyList();
    }
    case kOverflowContainersList: {
      nsFrameList* list = GetOverflowContainers();
      return list ? *list : nsFrameList::EmptyList();
    }
    case kExcessOverflowContainersList: {
      nsFrameList* list = GetExcessOverflowContainers();
      return list ? *list : nsFrameList::EmptyList();
    }
    case kBackdropList: {
      nsFrameList* list = GetProperty(BackdropProperty());
      return list ? *list : nsFrameList::EmptyList();
    }
    default:
      return nsSplittableFrame::GetChildList(aListID);
  }
}

void nsContainerFrame::GetChildLists(nsTArray<ChildList>* aLists) const {
  mFrames.AppendIfNonempty(aLists, kPrincipalList);

  using T = mozilla::FrameProperties::UntypedDescriptor;
  mProperties.ForEach([this, aLists](const T& aProp, void* aValue) {
    typedef const nsFrameList* L;
    if (aProp == OverflowProperty()) {
      L(aValue)->AppendIfNonempty(aLists, kOverflowList);
    } else if (aProp == OverflowContainersProperty()) {
      MOZ_ASSERT(IsFrameOfType(nsIFrame::eCanContainOverflowContainers),
                 "found unexpected OverflowContainersProperty");
      Unused << this;  // silence clang -Wunused-lambda-capture in opt builds
      L(aValue)->AppendIfNonempty(aLists, kOverflowContainersList);
    } else if (aProp == ExcessOverflowContainersProperty()) {
      MOZ_ASSERT(IsFrameOfType(nsIFrame::eCanContainOverflowContainers),
                 "found unexpected ExcessOverflowContainersProperty");
      Unused << this;  // silence clang -Wunused-lambda-capture in opt builds
      L(aValue)->AppendIfNonempty(aLists, kExcessOverflowContainersList);
    } else if (aProp == BackdropProperty()) {
      L(aValue)->AppendIfNonempty(aLists, kBackdropList);
    }
    return true;
  });

  nsSplittableFrame::GetChildLists(aLists);
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

void nsContainerFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayListSet& aLists) {
  DisplayBorderBackgroundOutline(aBuilder, aLists);
  BuildDisplayListForNonBlockChildren(aBuilder, aLists);
}

void nsContainerFrame::BuildDisplayListForNonBlockChildren(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists,
    DisplayChildFlags aFlags) {
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background directly onto the content list
  nsDisplayListSet set(aLists, aLists.Content());
  // The children should be in content order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, set, aFlags);
    kid = kid->GetNextSibling();
  }
}

class nsDisplaySelectionOverlay : public nsPaintedDisplayItem {
 public:
  /**
   * @param aSelectionValue nsISelectionController::getDisplaySelection.
   */
  nsDisplaySelectionOverlay(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            int16_t aSelectionValue)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mSelectionValue(aSelectionValue) {
    MOZ_COUNT_CTOR(nsDisplaySelectionOverlay);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplaySelectionOverlay)

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  NS_DISPLAY_DECL_NAME("SelectionOverlay", TYPE_SELECTION_OVERLAY)
 private:
  DeviceColor ComputeColor() const;

  static DeviceColor ComputeColorFromSelectionStyle(ComputedStyle&);
  static DeviceColor ApplyTransparencyIfNecessary(nscolor);

  // nsISelectionController::getDisplaySelection.
  int16_t mSelectionValue;
};

DeviceColor nsDisplaySelectionOverlay::ApplyTransparencyIfNecessary(
    nscolor aColor) {
  // If it has already alpha, leave it like that.
  if (NS_GET_A(aColor) != 255) {
    return ToDeviceColor(aColor);
  }

  // NOTE(emilio): Blink and WebKit do something slightly different here, and
  // blend the color with white instead, both for overlays and text backgrounds.
  auto color = sRGBColor::FromABGR(aColor);
  color.a = 0.5;
  return ToDeviceColor(color);
}

DeviceColor nsDisplaySelectionOverlay::ComputeColorFromSelectionStyle(
    ComputedStyle& aStyle) {
  return ApplyTransparencyIfNecessary(
      aStyle.GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor));
}

DeviceColor nsDisplaySelectionOverlay::ComputeColor() const {
  LookAndFeel::ColorID colorID;
  if (RefPtr<ComputedStyle> style =
          mFrame->ComputeSelectionStyle(mSelectionValue)) {
    return ComputeColorFromSelectionStyle(*style);
  }
  if (mSelectionValue == nsISelectionController::SELECTION_ON) {
    colorID = LookAndFeel::ColorID::TextSelectBackground;
  } else if (mSelectionValue == nsISelectionController::SELECTION_ATTENTION) {
    colorID = LookAndFeel::ColorID::TextSelectBackgroundAttention;
  } else {
    colorID = LookAndFeel::ColorID::TextSelectBackgroundDisabled;
  }

  return ApplyTransparencyIfNecessary(
      LookAndFeel::GetColor(colorID, NS_RGB(255, 255, 255)));
}

void nsDisplaySelectionOverlay::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx) {
  DrawTarget& aDrawTarget = *aCtx->GetDrawTarget();
  ColorPattern color(ComputeColor());

  nsIntRect pxRect = GetPaintRect().ToOutsidePixels(
      mFrame->PresContext()->AppUnitsPerDevPixel());
  Rect rect(pxRect.x, pxRect.y, pxRect.width, pxRect.height);
  MaybeSnapToDevicePixels(rect, aDrawTarget, true);

  aDrawTarget.FillRect(rect, color);
}

bool nsDisplaySelectionOverlay::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::LayoutRect bounds = wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(
      nsRect(ToReferenceFrame(), Frame()->GetSize()),
      mFrame->PresContext()->AppUnitsPerDevPixel()));
  aBuilder.PushRect(bounds, bounds, !BackfaceIsHidden(),
                    wr::ToColorF(ComputeColor()));
  return true;
}

void nsContainerFrame::DisplaySelectionOverlay(nsDisplayListBuilder* aBuilder,
                                               nsDisplayList* aList,
                                               uint16_t aContentType) {
  if (!IsSelected() || !IsVisibleForPainting()) {
    return;
  }

  int16_t displaySelection = PresShell()->GetSelectionFlags();
  if (!(displaySelection & aContentType)) {
    return;
  }

  const nsFrameSelection* frameSelection = GetConstFrameSelection();
  int16_t selectionValue = frameSelection->GetDisplaySelection();

  if (selectionValue <= nsISelectionController::SELECTION_HIDDEN) {
    return;  // selection is hidden or off
  }

  nsIContent* newContent = mContent->GetParent();

  // check to see if we are anonymous content
  int32_t offset = 0;
  if (newContent) {
    // XXXbz there has GOT to be a better way of determining this!
    offset = newContent->ComputeIndexOf(mContent);
  }

  // look up to see what selection(s) are on this frame
  UniquePtr<SelectionDetails> details =
      frameSelection->LookUpSelection(newContent, offset, 1, false);
  if (!details) {
    return;
  }

  bool normal = false;
  for (SelectionDetails* sd = details.get(); sd; sd = sd->mNext.get()) {
    if (sd->mSelectionType == SelectionType::eNormal) {
      normal = true;
    }
  }

  if (!normal && aContentType == nsISelectionDisplay::DISPLAY_IMAGES) {
    // Don't overlay an image if it's not in the primary selection.
    return;
  }

  aList->AppendNewToTop<nsDisplaySelectionOverlay>(aBuilder, this,
                                                   selectionValue);
}

/* virtual */
void nsContainerFrame::ChildIsDirty(nsIFrame* aChild) {
  NS_ASSERTION(aChild->IsSubtreeDirty(), "child isn't actually dirty");

  AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
}

nsIFrame::FrameSearchResult nsContainerFrame::PeekOffsetNoAmount(
    bool aForward, int32_t* aOffset) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return CONTINUE_EMPTY;
}

nsIFrame::FrameSearchResult nsContainerFrame::PeekOffsetCharacter(
    bool aForward, int32_t* aOffset, PeekOffsetCharacterOptions aOptions) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return CONTINUE_EMPTY;
}

/////////////////////////////////////////////////////////////////////////////
// Helper member functions

/**
 * Position the view associated with |aKidFrame|, if there is one. A
 * container frame should call this method after positioning a frame,
 * but before |Reflow|.
 */
void nsContainerFrame::PositionFrameView(nsIFrame* aKidFrame) {
  nsIFrame* parentFrame = aKidFrame->GetParent();
  if (!aKidFrame->HasView() || !parentFrame) return;

  nsView* view = aKidFrame->GetView();
  nsViewManager* vm = view->GetViewManager();
  nsPoint pt;
  nsView* ancestorView = parentFrame->GetClosestView(&pt);

  if (ancestorView != view->GetParent()) {
    NS_ASSERTION(ancestorView == view->GetParent()->GetParent(),
                 "Allowed only one anonymous view between frames");
    // parentFrame is responsible for positioning aKidFrame's view
    // explicitly
    return;
  }

  pt += aKidFrame->GetPosition();
  vm->MoveViewTo(view, pt.x, pt.y);
}

nsresult nsContainerFrame::ReparentFrameView(nsIFrame* aChildFrame,
                                             nsIFrame* aOldParentFrame,
                                             nsIFrame* aNewParentFrame) {
  MOZ_ASSERT(aChildFrame, "null child frame pointer");
  MOZ_ASSERT(aOldParentFrame, "null old parent frame pointer");
  MOZ_ASSERT(aNewParentFrame, "null new parent frame pointer");
  MOZ_ASSERT(aOldParentFrame != aNewParentFrame,
             "same old and new parent frame");

  // See if either the old parent frame or the new parent frame have a view
  while (!aOldParentFrame->HasView() && !aNewParentFrame->HasView()) {
    // Walk up both the old parent frame and the new parent frame nodes
    // stopping when we either find a common parent or views for one
    // or both of the frames.
    //
    // This works well in the common case where we push/pull and the old parent
    // frame and the new parent frame are part of the same flow. They will
    // typically be the same distance (height wise) from the
    aOldParentFrame = aOldParentFrame->GetParent();
    aNewParentFrame = aNewParentFrame->GetParent();

    // We should never walk all the way to the root frame without finding
    // a view
    NS_ASSERTION(aOldParentFrame && aNewParentFrame, "didn't find view");

    // See if we reached a common ancestor
    if (aOldParentFrame == aNewParentFrame) {
      break;
    }
  }

  // See if we found a common parent frame
  if (aOldParentFrame == aNewParentFrame) {
    // We found a common parent and there are no views between the old parent
    // and the common parent or the new parent frame and the common parent.
    // Because neither the old parent frame nor the new parent frame have views,
    // then any child views don't need reparenting
    return NS_OK;
  }

  // We found views for one or both of the ancestor frames before we
  // found a common ancestor.
  nsView* oldParentView = aOldParentFrame->GetClosestView();
  nsView* newParentView = aNewParentFrame->GetClosestView();

  // See if the old parent frame and the new parent frame are in the
  // same view sub-hierarchy. If they are then we don't have to do
  // anything
  if (oldParentView != newParentView) {
    // They're not so we need to reparent any child views
    aChildFrame->ReparentFrameViewTo(oldParentView->GetViewManager(),
                                     newParentView, oldParentView);
  }

  return NS_OK;
}

void nsContainerFrame::ReparentFrameViewList(const nsFrameList& aChildFrameList,
                                             nsIFrame* aOldParentFrame,
                                             nsIFrame* aNewParentFrame) {
  MOZ_ASSERT(aChildFrameList.NotEmpty(), "empty child frame list");
  MOZ_ASSERT(aOldParentFrame, "null old parent frame pointer");
  MOZ_ASSERT(aNewParentFrame, "null new parent frame pointer");
  MOZ_ASSERT(aOldParentFrame != aNewParentFrame,
             "same old and new parent frame");

  // See if either the old parent frame or the new parent frame have a view
  while (!aOldParentFrame->HasView() && !aNewParentFrame->HasView()) {
    // Walk up both the old parent frame and the new parent frame nodes
    // stopping when we either find a common parent or views for one
    // or both of the frames.
    //
    // This works well in the common case where we push/pull and the old parent
    // frame and the new parent frame are part of the same flow. They will
    // typically be the same distance (height wise) from the
    aOldParentFrame = aOldParentFrame->GetParent();
    aNewParentFrame = aNewParentFrame->GetParent();

    // We should never walk all the way to the root frame without finding
    // a view
    NS_ASSERTION(aOldParentFrame && aNewParentFrame, "didn't find view");

    // See if we reached a common ancestor
    if (aOldParentFrame == aNewParentFrame) {
      break;
    }
  }

  // See if we found a common parent frame
  if (aOldParentFrame == aNewParentFrame) {
    // We found a common parent and there are no views between the old parent
    // and the common parent or the new parent frame and the common parent.
    // Because neither the old parent frame nor the new parent frame have views,
    // then any child views don't need reparenting
    return;
  }

  // We found views for one or both of the ancestor frames before we
  // found a common ancestor.
  nsView* oldParentView = aOldParentFrame->GetClosestView();
  nsView* newParentView = aNewParentFrame->GetClosestView();

  // See if the old parent frame and the new parent frame are in the
  // same view sub-hierarchy. If they are then we don't have to do
  // anything
  if (oldParentView != newParentView) {
    nsViewManager* viewManager = oldParentView->GetViewManager();

    // They're not so we need to reparent any child views
    for (nsFrameList::Enumerator e(aChildFrameList); !e.AtEnd(); e.Next()) {
      e.get()->ReparentFrameViewTo(viewManager, newParentView, oldParentView);
    }
  }
}

void nsContainerFrame::ReparentFrame(nsIFrame* aFrame,
                                     nsContainerFrame* aOldParent,
                                     nsContainerFrame* aNewParent) {
  NS_ASSERTION(aOldParent == aFrame->GetParent(),
               "Parent not consistent with expectations");

  aFrame->SetParent(aNewParent);

  // When pushing and pulling frames we need to check for whether any
  // views need to be reparented
  ReparentFrameView(aFrame, aOldParent, aNewParent);
}

void nsContainerFrame::ReparentFrames(nsFrameList& aFrameList,
                                      nsContainerFrame* aOldParent,
                                      nsContainerFrame* aNewParent) {
  for (auto* f : aFrameList) {
    ReparentFrame(f, aOldParent, aNewParent);
  }
}

static nsIWidget* GetPresContextContainerWidget(nsPresContext* aPresContext) {
  nsCOMPtr<nsISupports> container = aPresContext->Document()->GetContainer();
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
  if (!baseWindow) return nullptr;

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  return mainWidget;
}

static bool IsTopLevelWidget(nsIWidget* aWidget) {
  nsWindowType windowType = aWidget->WindowType();
  return windowType == eWindowType_toplevel ||
         windowType == eWindowType_dialog || windowType == eWindowType_popup ||
         windowType == eWindowType_sheet;
}

void nsContainerFrame::SyncWindowProperties(nsPresContext* aPresContext,
                                            nsIFrame* aFrame, nsView* aView,
                                            gfxContext* aRC, uint32_t aFlags) {
#ifdef MOZ_XUL
  if (!aView || !nsCSSRendering::IsCanvasFrame(aFrame) || !aView->HasWidget())
    return;

  nsCOMPtr<nsIWidget> windowWidget =
      GetPresContextContainerWidget(aPresContext);
  if (!windowWidget || !IsTopLevelWidget(windowWidget)) return;

  nsViewManager* vm = aView->GetViewManager();
  nsView* rootView = vm->GetRootView();

  if (aView != rootView) return;

  Element* rootElement = aPresContext->Document()->GetRootElement();
  if (!rootElement) {
    return;
  }

  nsIFrame* rootFrame =
      aPresContext->PresShell()->FrameConstructor()->GetRootElementStyleFrame();
  if (!rootFrame) return;

  if (aFlags & SET_ASYNC) {
    aView->SetNeedsWindowPropertiesSync();
    return;
  }

  RefPtr<nsPresContext> kungFuDeathGrip(aPresContext);
  AutoWeakFrame weak(rootFrame);

  if (!aPresContext->PresShell()->GetRootScrollFrame()) {
    // Scrollframes use native widgets which don't work well with
    // translucent windows, at least in Windows XP. So if the document
    // has a root scrollrame it's useless to try to make it transparent,
    // we'll just get something broken.
    // We can change this to allow translucent toplevel HTML documents
    // (e.g. to do something like Dashboard widgets), once we
    // have broad support for translucent scrolled documents, but be
    // careful because apparently some Firefox extensions expect
    // openDialog("something.html") to produce an opaque window
    // even if the HTML doesn't have a background-color set.
    nsTransparencyMode mode =
        nsLayoutUtils::GetFrameTransparency(aFrame, rootFrame);
    StyleWindowShadow shadow = rootFrame->StyleUIReset()->mWindowShadow;
    nsCOMPtr<nsIWidget> viewWidget = aView->GetWidget();
    viewWidget->SetTransparencyMode(mode);
    windowWidget->SetWindowShadowStyle(shadow);
  }

  if (!aRC) return;

  if (!weak.IsAlive()) {
    return;
  }

  nsSize minSize(0, 0);
  nsSize maxSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  if (rootElement->IsXULElement()) {
    nsBoxLayoutState aState(aPresContext, aRC);
    minSize = rootFrame->GetXULMinSize(aState);
    maxSize = rootFrame->GetXULMaxSize(aState);
  } else {
    auto* pos = rootFrame->StylePosition();
    if (pos->mMinWidth.ConvertsToLength()) {
      minSize.width = pos->mMinWidth.ToLength();
    }
    if (pos->mMinHeight.ConvertsToLength()) {
      minSize.height = pos->mMinHeight.ToLength();
    }
    if (pos->mMaxWidth.ConvertsToLength()) {
      maxSize.width = pos->mMaxWidth.ToLength();
    }
    if (pos->mMaxHeight.ConvertsToLength()) {
      maxSize.height = pos->mMaxHeight.ToLength();
    }
  }
  SetSizeConstraints(aPresContext, windowWidget, minSize, maxSize);
#endif
}

void nsContainerFrame::SetSizeConstraints(nsPresContext* aPresContext,
                                          nsIWidget* aWidget,
                                          const nsSize& aMinSize,
                                          const nsSize& aMaxSize) {
  LayoutDeviceIntSize devMinSize(
      aPresContext->AppUnitsToDevPixels(aMinSize.width),
      aPresContext->AppUnitsToDevPixels(aMinSize.height));
  LayoutDeviceIntSize devMaxSize(
      aMaxSize.width == NS_UNCONSTRAINEDSIZE
          ? NS_MAXSIZE
          : aPresContext->AppUnitsToDevPixels(aMaxSize.width),
      aMaxSize.height == NS_UNCONSTRAINEDSIZE
          ? NS_MAXSIZE
          : aPresContext->AppUnitsToDevPixels(aMaxSize.height));

  // MinSize has a priority over MaxSize
  if (devMinSize.width > devMaxSize.width) devMaxSize.width = devMinSize.width;
  if (devMinSize.height > devMaxSize.height)
    devMaxSize.height = devMinSize.height;

  widget::SizeConstraints constraints(devMinSize, devMaxSize);

  // The sizes are in inner window sizes, so convert them into outer window
  // sizes. Use a size of (200, 200) as only the difference between the inner
  // and outer size is needed.
  LayoutDeviceIntSize windowSize =
      aWidget->ClientToWindowSize(LayoutDeviceIntSize(200, 200));
  if (constraints.mMinSize.width)
    constraints.mMinSize.width += windowSize.width - 200;
  if (constraints.mMinSize.height)
    constraints.mMinSize.height += windowSize.height - 200;
  if (constraints.mMaxSize.width != NS_MAXSIZE)
    constraints.mMaxSize.width += windowSize.width - 200;
  if (constraints.mMaxSize.height != NS_MAXSIZE)
    constraints.mMaxSize.height += windowSize.height - 200;

  aWidget->SetSizeConstraints(constraints);
}

void nsContainerFrame::SyncFrameViewAfterReflow(nsPresContext* aPresContext,
                                                nsIFrame* aFrame, nsView* aView,
                                                const nsRect& aInkOverflowArea,
                                                ReflowChildFlags aFlags) {
  if (!aView) {
    return;
  }

  // Make sure the view is sized and positioned correctly
  if (!(aFlags & ReflowChildFlags::NoMoveView)) {
    PositionFrameView(aFrame);
  }

  if (!(aFlags & ReflowChildFlags::NoSizeView)) {
    nsViewManager* vm = aView->GetViewManager();

    vm->ResizeView(aView, aInkOverflowArea, true);
  }
}

static nscoord GetCoord(const LengthPercentage& aCoord, nscoord aIfNotCoord) {
  if (aCoord.ConvertsToLength()) {
    return aCoord.ToLength();
  }
  return aIfNotCoord;
}

static nscoord GetCoord(const LengthPercentageOrAuto& aCoord,
                        nscoord aIfNotCoord) {
  if (aCoord.IsAuto()) {
    return aIfNotCoord;
  }
  return GetCoord(aCoord.AsLengthPercentage(), aIfNotCoord);
}

void nsContainerFrame::DoInlineIntrinsicISize(gfxContext* aRenderingContext,
                                              InlineIntrinsicISizeData* aData,
                                              IntrinsicISizeType aType) {
  if (GetPrevInFlow()) return;  // Already added.

  WritingMode wm = GetWritingMode();
  mozilla::Side startSide = wm.PhysicalSideForInlineAxis(eLogicalEdgeStart);
  mozilla::Side endSide = wm.PhysicalSideForInlineAxis(eLogicalEdgeEnd);

  const nsStylePadding* stylePadding = StylePadding();
  const nsStyleBorder* styleBorder = StyleBorder();
  const nsStyleMargin* styleMargin = StyleMargin();

  // This goes at the beginning no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the startSide border is always on the
  // first line.
  // This frame is a first-in-flow, but it might have a previous bidi
  // continuation, in which case that continuation should handle the startSide
  // border.
  // For box-decoration-break:clone we setup clonePBM = startPBM + endPBM and
  // add that to each line.  For box-decoration-break:slice clonePBM is zero.
  nscoord clonePBM = 0;  // PBM = PaddingBorderMargin
  const bool sliceBreak =
      styleBorder->mBoxDecorationBreak == StyleBoxDecorationBreak::Slice;
  if (!GetPrevContinuation() || MOZ_UNLIKELY(!sliceBreak)) {
    nscoord startPBM =
        // clamp negative calc() to 0
        std::max(GetCoord(stylePadding->mPadding.Get(startSide), 0), 0) +
        styleBorder->GetComputedBorderWidth(startSide) +
        GetCoord(styleMargin->mMargin.Get(startSide), 0);
    if (MOZ_LIKELY(sliceBreak)) {
      aData->mCurrentLine += startPBM;
    } else {
      clonePBM = startPBM;
    }
  }

  nscoord endPBM =
      // clamp negative calc() to 0
      std::max(GetCoord(stylePadding->mPadding.Get(endSide), 0), 0) +
      styleBorder->GetComputedBorderWidth(endSide) +
      GetCoord(styleMargin->mMargin.Get(endSide), 0);
  if (MOZ_UNLIKELY(!sliceBreak)) {
    clonePBM += endPBM;
    aData->mCurrentLine += clonePBM;
  }

  const nsLineList_iterator* savedLine = aData->mLine;
  nsIFrame* const savedLineContainer = aData->LineContainer();

  nsContainerFrame* lastInFlow;
  for (nsContainerFrame* nif = this; nif;
       nif = static_cast<nsContainerFrame*>(nif->GetNextInFlow())) {
    if (aData->mCurrentLine == 0) {
      aData->mCurrentLine = clonePBM;
    }
    for (nsIFrame* kid : nif->mFrames) {
      if (aType == IntrinsicISizeType::MinISize) {
        kid->AddInlineMinISize(aRenderingContext,
                               static_cast<InlineMinISizeData*>(aData));
      } else {
        kid->AddInlinePrefISize(aRenderingContext,
                                static_cast<InlinePrefISizeData*>(aData));
      }
    }

    // After we advance to our next-in-flow, the stored line and line container
    // may no longer be correct. Just forget them.
    aData->mLine = nullptr;
    aData->SetLineContainer(nullptr);

    lastInFlow = nif;
  }

  aData->mLine = savedLine;
  aData->SetLineContainer(savedLineContainer);

  // This goes at the end no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the endSide border is always on the
  // last line.
  // We reached the last-in-flow, but it might have a next bidi
  // continuation, in which case that continuation should handle
  // the endSide border.
  if (MOZ_LIKELY(!lastInFlow->GetNextContinuation() && sliceBreak)) {
    aData->mCurrentLine += endPBM;
  }
}

/* virtual */
LogicalSize nsContainerFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const mozilla::LogicalSize& aBorderPadding,
    const StyleSizeOverrides& aSizeOverrides, ComputeSizeFlags aFlags) {
  LogicalSize result(aWM, 0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  nscoord availBased =
      aAvailableISize - aMargin.ISize(aWM) - aBorderPadding.ISize(aWM);
  // replaced elements always shrink-wrap
  if (aFlags.contains(ComputeSizeFlag::ShrinkWrap) ||
      IsFrameOfType(eReplaced)) {
    // Only bother computing our 'auto' ISize if the result will be used.
    const auto& styleISize = aSizeOverrides.mStyleISize
                                 ? *aSizeOverrides.mStyleISize
                                 : StylePosition()->ISize(aWM);
    if (styleISize.IsAuto()) {
      result.ISize(aWM) =
          ShrinkWidthToFit(aRenderingContext, availBased, aFlags);
    }
  } else {
    result.ISize(aWM) = availBased;
  }

  if (IsTableCaption()) {
    // If we're a container for font size inflation, then shrink
    // wrapping inside of us should not apply font size inflation.
    AutoMaybeDisableFontInflation an(this);

    WritingMode tableWM = GetParent()->GetWritingMode();
    StyleCaptionSide captionSide = StyleTableBorder()->mCaptionSide;

    if (aWM.IsOrthogonalTo(tableWM)) {
      if (captionSide == StyleCaptionSide::Top ||
          captionSide == StyleCaptionSide::TopOutside ||
          captionSide == StyleCaptionSide::Bottom ||
          captionSide == StyleCaptionSide::BottomOutside) {
        // For an orthogonal caption on a block-dir side of the table,
        // shrink-wrap to min-isize.
        result.ISize(aWM) = GetMinISize(aRenderingContext);
      } else {
        // An orthogonal caption on an inline-dir side of the table
        // is constrained to the containing block.
        nscoord pref = GetPrefISize(aRenderingContext);
        if (pref > aCBSize.ISize(aWM)) {
          pref = aCBSize.ISize(aWM);
        }
        if (pref < result.ISize(aWM)) {
          result.ISize(aWM) = pref;
        }
      }
    } else {
      if (captionSide == StyleCaptionSide::Left ||
          captionSide == StyleCaptionSide::Right) {
        result.ISize(aWM) = GetMinISize(aRenderingContext);
      } else if (captionSide == StyleCaptionSide::Top ||
                 captionSide == StyleCaptionSide::Bottom) {
        // The outer frame constrains our available isize to the isize of
        // the table.  Grow if our min-isize is bigger than that, but not
        // larger than the containing block isize.  (It would really be nice
        // to transmit that information another way, so we could grow up to
        // the table's available isize, but that's harder.)
        nscoord min = GetMinISize(aRenderingContext);
        if (min > aCBSize.ISize(aWM)) {
          min = aCBSize.ISize(aWM);
        }
        if (min > result.ISize(aWM)) {
          result.ISize(aWM) = min;
        }
      }
    }
  }
  return result;
}

void nsContainerFrame::ReflowChild(
    nsIFrame* aKidFrame, nsPresContext* aPresContext,
    ReflowOutput& aDesiredSize, const ReflowInput& aReflowInput,
    const WritingMode& aWM, const LogicalPoint& aPos,
    const nsSize& aContainerSize, ReflowChildFlags aFlags,
    nsReflowStatus& aStatus, nsOverflowContinuationTracker* aTracker) {
  MOZ_ASSERT(aReflowInput.mFrame == aKidFrame, "bad reflow input");
  if (aWM.IsPhysicalRTL()) {
    NS_ASSERTION(aContainerSize.width != NS_UNCONSTRAINEDSIZE,
                 "ReflowChild with unconstrained container width!");
  }
  MOZ_ASSERT(aDesiredSize.InkOverflow() == nsRect(0, 0, 0, 0) &&
                 aDesiredSize.ScrollableOverflow() == nsRect(0, 0, 0, 0),
             "please reset the overflow areas before calling ReflowChild");

  // Position the child frame and its view if requested.
  if (ReflowChildFlags::NoMoveFrame !=
      (aFlags & ReflowChildFlags::NoMoveFrame)) {
    aKidFrame->SetPosition(aWM, aPos, aContainerSize);
  }

  if (!(aFlags & ReflowChildFlags::NoMoveView)) {
    PositionFrameView(aKidFrame);
    PositionChildViews(aKidFrame);
  }

  // Reflow the child frame
  aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);

  // If the child frame is complete, delete any next-in-flows,
  // but only if the NoDeleteNextInFlowChild flag isn't set.
  if (!aStatus.IsInlineBreakBefore() && aStatus.IsFullyComplete() &&
      !(aFlags & ReflowChildFlags::NoDeleteNextInFlowChild)) {
    nsIFrame* kidNextInFlow = aKidFrame->GetNextInFlow();
    if (kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsOverflowContinuationTracker::AutoFinish fini(aTracker, aKidFrame);
      kidNextInFlow->GetParent()->DeleteNextInFlowChild(kidNextInFlow, true);
    }
  }
}

// XXX temporary: hold on to a copy of the old physical version of
//    ReflowChild so that we can convert callers incrementally.
void nsContainerFrame::ReflowChild(nsIFrame* aKidFrame,
                                   nsPresContext* aPresContext,
                                   ReflowOutput& aDesiredSize,
                                   const ReflowInput& aReflowInput, nscoord aX,
                                   nscoord aY, ReflowChildFlags aFlags,
                                   nsReflowStatus& aStatus,
                                   nsOverflowContinuationTracker* aTracker) {
  MOZ_ASSERT(aReflowInput.mFrame == aKidFrame, "bad reflow input");

  // Position the child frame and its view if requested.
  if (ReflowChildFlags::NoMoveFrame !=
      (aFlags & ReflowChildFlags::NoMoveFrame)) {
    aKidFrame->SetPosition(nsPoint(aX, aY));
  }

  if (!(aFlags & ReflowChildFlags::NoMoveView)) {
    PositionFrameView(aKidFrame);
    PositionChildViews(aKidFrame);
  }

  // Reflow the child frame
  aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);

  // If the child frame is complete, delete any next-in-flows,
  // but only if the NoDeleteNextInFlowChild flag isn't set.
  if (aStatus.IsFullyComplete() &&
      !(aFlags & ReflowChildFlags::NoDeleteNextInFlowChild)) {
    nsIFrame* kidNextInFlow = aKidFrame->GetNextInFlow();
    if (kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsOverflowContinuationTracker::AutoFinish fini(aTracker, aKidFrame);
      kidNextInFlow->GetParent()->DeleteNextInFlowChild(kidNextInFlow, true);
    }
  }
}

/**
 * Position the views of |aFrame|'s descendants. A container frame
 * should call this method if it moves a frame after |Reflow|.
 */
void nsContainerFrame::PositionChildViews(nsIFrame* aFrame) {
  if (!aFrame->HasAnyStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  // Recursively walk aFrame's child frames.
  // Process the additional child lists, but skip the popup list as the
  // view for popups is managed by the parent. Currently only nsMenuFrame
  // and nsPopupSetFrame have a popupList and during layout will adjust the
  // view manually to position the popup.
  for (const auto& [list, listID] : aFrame->ChildLists()) {
    if (listID == kPopupList) {
      continue;
    }
    for (nsIFrame* childFrame : list) {
      // Position the frame's view (if it has one) otherwise recursively
      // process its children
      if (childFrame->HasView()) {
        PositionFrameView(childFrame);
      } else {
        PositionChildViews(childFrame);
      }
    }
  }
}

/**
 * De-optimize function to work around a VC2017 15.5+ compiler bug:
 * https://bugzil.la/1424281#c12
 */
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_AMD64)
#  pragma optimize("g", off)
#endif
void nsContainerFrame::FinishReflowChild(
    nsIFrame* aKidFrame, nsPresContext* aPresContext,
    const ReflowOutput& aDesiredSize, const ReflowInput* aReflowInput,
    const WritingMode& aWM, const LogicalPoint& aPos,
    const nsSize& aContainerSize, nsIFrame::ReflowChildFlags aFlags) {
  MOZ_ASSERT(!aReflowInput || aReflowInput->mFrame == aKidFrame);
  MOZ_ASSERT(aReflowInput || aKidFrame->IsFrameOfType(eMathML) ||
                 aKidFrame->IsTableCellFrame(),
             "aReflowInput should be passed in almost all cases");

  if (aWM.IsPhysicalRTL()) {
    NS_ASSERTION(aContainerSize.width != NS_UNCONSTRAINEDSIZE,
                 "FinishReflowChild with unconstrained container width!");
  }

  nsPoint curOrigin = aKidFrame->GetPosition();
  const LogicalSize convertedSize = aDesiredSize.Size(aWM);
  LogicalPoint pos(aPos);

  if (aFlags & ReflowChildFlags::ApplyRelativePositioning) {
    MOZ_ASSERT(aReflowInput, "caller must have passed reflow input");
    // ApplyRelativePositioning in right-to-left writing modes needs to know
    // the updated frame width to set the normal position correctly.
    aKidFrame->SetSize(aWM, convertedSize);

    const LogicalMargin offsets = aReflowInput->ComputedLogicalOffsets(aWM);
    ReflowInput::ApplyRelativePositioning(aKidFrame, aWM, offsets, &pos,
                                          aContainerSize);
  }

  if (ReflowChildFlags::NoMoveFrame !=
      (aFlags & ReflowChildFlags::NoMoveFrame)) {
    aKidFrame->SetRect(aWM, LogicalRect(aWM, pos, convertedSize),
                       aContainerSize);
  } else {
    aKidFrame->SetSize(aWM, convertedSize);
  }

  if (aKidFrame->HasView()) {
    nsView* view = aKidFrame->GetView();
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                             aDesiredSize.InkOverflow(), aFlags);
  }

  nsPoint newOrigin = aKidFrame->GetPosition();
  if (!(aFlags & ReflowChildFlags::NoMoveView) && curOrigin != newOrigin) {
    if (!aKidFrame->HasView()) {
      // If the frame has moved, then we need to make sure any child views are
      // correctly positioned
      PositionChildViews(aKidFrame);
    }
  }

  aKidFrame->DidReflow(aPresContext, aReflowInput);
}
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_AMD64)
#  pragma optimize("", on)
#endif

// XXX temporary: hold on to a copy of the old physical version of
//    FinishReflowChild so that we can convert callers incrementally.
void nsContainerFrame::FinishReflowChild(nsIFrame* aKidFrame,
                                         nsPresContext* aPresContext,
                                         const ReflowOutput& aDesiredSize,
                                         const ReflowInput* aReflowInput,
                                         nscoord aX, nscoord aY,
                                         ReflowChildFlags aFlags) {
  MOZ_ASSERT(!(aFlags & ReflowChildFlags::ApplyRelativePositioning),
             "only the logical version supports ApplyRelativePositioning "
             "since ApplyRelativePositioning requires the container size");

  nsPoint curOrigin = aKidFrame->GetPosition();
  nsPoint pos(aX, aY);
  nsSize size(aDesiredSize.PhysicalSize());

  if (ReflowChildFlags::NoMoveFrame !=
      (aFlags & ReflowChildFlags::NoMoveFrame)) {
    aKidFrame->SetRect(nsRect(pos, size));
  } else {
    aKidFrame->SetSize(size);
  }

  if (aKidFrame->HasView()) {
    nsView* view = aKidFrame->GetView();
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                             aDesiredSize.InkOverflow(), aFlags);
  }

  if (!(aFlags & ReflowChildFlags::NoMoveView) && curOrigin != pos) {
    if (!aKidFrame->HasView()) {
      // If the frame has moved, then we need to make sure any child views are
      // correctly positioned
      PositionChildViews(aKidFrame);
    }
  }

  aKidFrame->DidReflow(aPresContext, aReflowInput);
}

void nsContainerFrame::ReflowOverflowContainerChildren(
    nsPresContext* aPresContext, const ReflowInput& aReflowInput,
    OverflowAreas& aOverflowRects, ReflowChildFlags aFlags,
    nsReflowStatus& aStatus, ChildFrameMerger aMergeFunc,
    Maybe<nsSize> aContainerSize) {
  MOZ_ASSERT(aPresContext, "null pointer");

  nsFrameList* overflowContainers =
      DrainExcessOverflowContainersList(aMergeFunc);
  if (!overflowContainers) {
    return;  // nothing to reflow
  }

  nsOverflowContinuationTracker tracker(this, false, false);
  bool shouldReflowAllKids = aReflowInput.ShouldReflowAllKids();

  for (nsIFrame* frame : *overflowContainers) {
    if (frame->GetPrevInFlow()->GetParent() != GetPrevInFlow()) {
      // frame's prevInFlow has moved, skip reflowing this frame;
      // it will get reflowed once it's been placed
      if (GetNextInFlow()) {
        // We report OverflowIncomplete status in this case to avoid our parent
        // deleting our next-in-flows which might destroy non-empty frames.
        nsReflowStatus status;
        status.SetOverflowIncomplete();
        aStatus.MergeCompletionStatusFrom(status);
      }
      continue;
    }

    auto ScrollableOverflowExceedsAvailableBSize =
        [this, &aReflowInput](nsIFrame* aFrame) {
          if (aReflowInput.AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
            return false;
          }
          const auto parentWM = GetWritingMode();
          const nscoord scrollableOverflowRectBEnd =
              LogicalRect(parentWM,
                          aFrame->ScrollableOverflowRectRelativeToParent(),
                          GetSize())
                  .BEnd(parentWM);
          return scrollableOverflowRectBEnd > aReflowInput.AvailableBSize();
        };

    // If the available block-size has changed, or the existing scrollable
    // overflow's block-end exceeds it, we need to reflow even if the frame
    // isn't dirty.
    if (shouldReflowAllKids || frame->IsSubtreeDirty() ||
        ScrollableOverflowExceedsAvailableBSize(frame)) {
      // Get prev-in-flow
      nsIFrame* prevInFlow = frame->GetPrevInFlow();
      NS_ASSERTION(prevInFlow,
                   "overflow container frame must have a prev-in-flow");
      NS_ASSERTION(
          frame->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER),
          "overflow container frame must have overflow container bit set");
      WritingMode wm = frame->GetWritingMode();
      nsSize containerSize = aContainerSize.valueOr(
          aReflowInput.AvailableSize(wm).GetPhysicalSize(wm));
      LogicalRect prevRect = prevInFlow->GetLogicalRect(wm, containerSize);

      // Initialize reflow params
      LogicalSize availSpace(wm, prevRect.ISize(wm),
                             aReflowInput.AvailableSize(wm).BSize(wm));
      ReflowOutput desiredSize(aReflowInput);
      ReflowInput frameState(aPresContext, aReflowInput, frame, availSpace);
      nsReflowStatus frameStatus;

      // Reflow
      LogicalPoint pos(wm, prevRect.IStart(wm), 0);
      ReflowChild(frame, aPresContext, desiredSize, frameState, wm, pos,
                  containerSize, aFlags, frameStatus, &tracker);
      // XXXfr Do we need to override any shrinkwrap effects here?
      // e.g. desiredSize.Width() = prevRect.width;
      FinishReflowChild(frame, aPresContext, desiredSize, &frameState, wm, pos,
                        containerSize, aFlags);

      // Handle continuations
      if (!frameStatus.IsFullyComplete()) {
        if (frame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
          // Abspos frames can't cause their parent to be incomplete,
          // only overflow incomplete.
          frameStatus.SetOverflowIncomplete();
        } else {
          NS_ASSERTION(frameStatus.IsComplete(),
                       "overflow container frames can't be incomplete, only "
                       "overflow-incomplete");
        }

        // Acquire a next-in-flow, creating it if necessary
        nsIFrame* nif = frame->GetNextInFlow();
        if (!nif) {
          NS_ASSERTION(frameStatus.NextInFlowNeedsReflow(),
                       "Someone forgot a NextInFlowNeedsReflow flag");
          nif = PresShell()->FrameConstructor()->CreateContinuingFrame(frame,
                                                                       this);
        } else if (!nif->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          // used to be a normal next-in-flow; steal it from the child list
          nif->GetParent()->StealFrame(nif);
        }

        tracker.Insert(nif, frameStatus);
      }
      aStatus.MergeCompletionStatusFrom(frameStatus);
      // At this point it would be nice to assert
      // !frame->GetOverflowRect().IsEmpty(), but we have some unsplittable
      // frames that, when taller than availableHeight will push zero-height
      // content into a next-in-flow.
    } else {
      tracker.Skip(frame, aStatus);
      if (aReflowInput.mFloatManager) {
        nsBlockFrame::RecoverFloatsFor(frame, *aReflowInput.mFloatManager,
                                       aReflowInput.GetWritingMode(),
                                       aReflowInput.ComputedPhysicalSize());
      }
    }
    ConsiderChildOverflow(aOverflowRects, frame);
  }
}

void nsContainerFrame::DisplayOverflowContainers(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) {
  nsFrameList* overflowconts = GetOverflowContainers();
  if (overflowconts) {
    for (nsIFrame* frame : *overflowconts) {
      BuildDisplayListForChild(aBuilder, frame, aLists);
    }
  }
}

bool nsContainerFrame::TryRemoveFrame(FrameListPropertyDescriptor aProp,
                                      nsIFrame* aChildToRemove) {
  nsFrameList* list = GetProperty(aProp);
  if (list && list->StartRemoveFrame(aChildToRemove)) {
    // aChildToRemove *may* have been removed from this list.
    if (list->IsEmpty()) {
      Unused << TakeProperty(aProp);
      list->Delete(PresShell());
    }
    return true;
  }
  return false;
}

bool nsContainerFrame::MaybeStealOverflowContainerFrame(nsIFrame* aChild) {
  bool removed = false;
  if (MOZ_UNLIKELY(aChild->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER))) {
    // Try removing from the overflow container list.
    removed = TryRemoveFrame(OverflowContainersProperty(), aChild);
    if (!removed) {
      // It might be in the excess overflow container list.
      removed = TryRemoveFrame(ExcessOverflowContainersProperty(), aChild);
    }
  }
  return removed;
}

void nsContainerFrame::StealFrame(nsIFrame* aChild) {
#ifdef DEBUG
  if (!mFrames.ContainsFrame(aChild)) {
    nsFrameList* list = GetOverflowFrames();
    if (!list || !list->ContainsFrame(aChild)) {
      list = GetOverflowContainers();
      if (!list || !list->ContainsFrame(aChild)) {
        list = GetExcessOverflowContainers();
        MOZ_ASSERT(list && list->ContainsFrame(aChild),
                   "aChild isn't our child"
                   " or on a frame list not supported by StealFrame");
      }
    }
  }
#endif

  if (MaybeStealOverflowContainerFrame(aChild)) {
    return;
  }

  // NOTE nsColumnSetFrame and nsCanvasFrame have their overflow containers
  // on the normal lists so we might get here also if the frame bit
  // NS_FRAME_IS_OVERFLOW_CONTAINER is set.
  if (mFrames.StartRemoveFrame(aChild)) {
    return;
  }

  // We didn't find the child in our principal child list.
  // Maybe it's on the overflow list?
  nsFrameList* frameList = GetOverflowFrames();
  if (frameList && frameList->ContinueRemoveFrame(aChild)) {
    if (frameList->IsEmpty()) {
      DestroyOverflowList();
    }
    return;
  }

  MOZ_ASSERT_UNREACHABLE("StealFrame: can't find aChild");
}

nsFrameList nsContainerFrame::StealFramesAfter(nsIFrame* aChild) {
  NS_ASSERTION(
      !aChild || !aChild->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER),
      "StealFramesAfter doesn't handle overflow containers");
  NS_ASSERTION(!IsBlockFrame(), "unexpected call");

  if (!aChild) {
    nsFrameList copy(mFrames);
    mFrames.Clear();
    return copy;
  }

  for (nsFrameList::FrameLinkEnumerator iter(mFrames); !iter.AtEnd();
       iter.Next()) {
    if (iter.PrevFrame() == aChild) {
      return mFrames.ExtractTail(iter);
    }
  }

  // We didn't find the child in the principal child list.
  // Maybe it's on the overflow list?
  nsFrameList* overflowFrames = GetOverflowFrames();
  if (overflowFrames) {
    for (nsFrameList::FrameLinkEnumerator iter(*overflowFrames); !iter.AtEnd();
         iter.Next()) {
      if (iter.PrevFrame() == aChild) {
        return overflowFrames->ExtractTail(iter);
      }
    }
  }

  NS_ERROR("StealFramesAfter: can't find aChild");
  return nsFrameList::EmptyList();
}

/*
 * Create a next-in-flow for aFrame. Will return the newly created
 * frame <b>if and only if</b> a new frame is created; otherwise
 * nullptr is returned.
 */
nsIFrame* nsContainerFrame::CreateNextInFlow(nsIFrame* aFrame) {
  MOZ_ASSERT(
      !IsBlockFrame(),
      "you should have called nsBlockFrame::CreateContinuationFor instead");
  MOZ_ASSERT(mFrames.ContainsFrame(aFrame), "expected an in-flow child frame");

  nsIFrame* nextInFlow = aFrame->GetNextInFlow();
  if (nullptr == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our child list.
    nextInFlow =
        PresShell()->FrameConstructor()->CreateContinuingFrame(aFrame, this);
    mFrames.InsertFrame(nullptr, aFrame, nextInFlow);

    NS_FRAME_LOG(NS_FRAME_TRACE_NEW_FRAMES,
                 ("nsContainerFrame::CreateNextInFlow: frame=%p nextInFlow=%p",
                  aFrame, nextInFlow));

    return nextInFlow;
  }
  return nullptr;
}

/**
 * Remove and delete aNextInFlow and its next-in-flows. Updates the sibling and
 * flow pointers
 */
void nsContainerFrame::DeleteNextInFlowChild(nsIFrame* aNextInFlow,
                                             bool aDeletingEmptyFrames) {
#ifdef DEBUG
  nsIFrame* prevInFlow = aNextInFlow->GetPrevInFlow();
#endif
  MOZ_ASSERT(prevInFlow, "bad prev-in-flow");

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  // Do this in a loop so we don't overflow the stack for frames
  // with very many next-in-flows
  nsIFrame* nextNextInFlow = aNextInFlow->GetNextInFlow();
  if (nextNextInFlow) {
    AutoTArray<nsIFrame*, 8> frames;
    for (nsIFrame* f = nextNextInFlow; f; f = f->GetNextInFlow()) {
      frames.AppendElement(f);
    }
    for (nsIFrame* delFrame : Reversed(frames)) {
      delFrame->GetParent()->DeleteNextInFlowChild(delFrame,
                                                   aDeletingEmptyFrames);
    }
  }

  // Take the next-in-flow out of the parent's child list
  StealFrame(aNextInFlow);

#ifdef DEBUG
  if (aDeletingEmptyFrames) {
    nsLayoutUtils::AssertTreeOnlyEmptyNextInFlows(aNextInFlow);
  }
#endif

  // Delete the next-in-flow frame and its descendants. This will also
  // remove it from its next-in-flow/prev-in-flow chain.
  aNextInFlow->Destroy();

  MOZ_ASSERT(!prevInFlow->GetNextInFlow(), "non null next-in-flow");
}

void nsContainerFrame::PushChildrenToOverflow(nsIFrame* aFromChild,
                                              nsIFrame* aPrevSibling) {
  MOZ_ASSERT(aFromChild, "null pointer");
  MOZ_ASSERT(aPrevSibling, "pushing first child");
  MOZ_ASSERT(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

  // Add the frames to our overflow list (let our next in flow drain
  // our overflow list when it is ready)
  SetOverflowFrames(mFrames.RemoveFramesAfter(aPrevSibling));
}

void nsContainerFrame::PushChildren(nsIFrame* aFromChild,
                                    nsIFrame* aPrevSibling) {
  MOZ_ASSERT(aFromChild, "null pointer");
  MOZ_ASSERT(aPrevSibling, "pushing first child");
  MOZ_ASSERT(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

  // Disconnect aFromChild from its previous sibling
  nsFrameList tail = mFrames.RemoveFramesAfter(aPrevSibling);

  nsContainerFrame* nextInFlow =
      static_cast<nsContainerFrame*>(GetNextInFlow());
  if (nextInFlow) {
    // XXX This is not a very good thing to do. If it gets removed
    // then remove the copy of this routine that doesn't do this from
    // nsInlineFrame.
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    for (nsIFrame* f = aFromChild; f; f = f->GetNextSibling()) {
      nsContainerFrame::ReparentFrameView(f, this, nextInFlow);
    }
    nextInFlow->mFrames.InsertFrames(nextInFlow, nullptr, tail);
  } else {
    // Add the frames to our overflow list
    SetOverflowFrames(std::move(tail));
  }
}

bool nsContainerFrame::PushIncompleteChildren(
    const FrameHashtable& aPushedItems, const FrameHashtable& aIncompleteItems,
    const FrameHashtable& aOverflowIncompleteItems) {
  MOZ_ASSERT(IsFlexOrGridContainer(),
             "Only Grid / Flex containers can call this!");

  if (aPushedItems.IsEmpty() && aIncompleteItems.IsEmpty() &&
      aOverflowIncompleteItems.IsEmpty()) {
    return false;
  }

  // Iterate the children in normal document order and append them (or a NIF)
  // to one of the following frame lists according to their status.
  nsFrameList pushedList;
  nsFrameList incompleteList;
  nsFrameList overflowIncompleteList;
  auto* fc = PresShell()->FrameConstructor();
  for (nsIFrame* child = GetChildList(kPrincipalList).FirstChild(); child;) {
    MOZ_ASSERT((aPushedItems.Contains(child) ? 1 : 0) +
                       (aIncompleteItems.Contains(child) ? 1 : 0) +
                       (aOverflowIncompleteItems.Contains(child) ? 1 : 0) <=
                   1,
               "child should only be in one of these sets");
    // Save the next-sibling so we can continue the loop if |child| is moved.
    nsIFrame* next = child->GetNextSibling();
    if (aPushedItems.Contains(child)) {
      MOZ_ASSERT(child->GetParent() == this);
      StealFrame(child);
      pushedList.AppendFrame(nullptr, child);
    } else if (aIncompleteItems.Contains(child)) {
      nsIFrame* childNIF = child->GetNextInFlow();
      if (!childNIF) {
        childNIF = fc->CreateContinuingFrame(child, this);
        incompleteList.AppendFrame(nullptr, childNIF);
      } else {
        auto* parent = childNIF->GetParent();
        MOZ_ASSERT(parent != this || !mFrames.ContainsFrame(childNIF),
                   "child's NIF shouldn't be in the same principal list");
        // If child's existing NIF is an overflow container, convert it to an
        // actual NIF, since now |child| has non-overflow stuff to give it.
        // Or, if it's further away then our next-in-flow, then pull it up.
        if (childNIF->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER) ||
            (parent != this && parent != GetNextInFlow())) {
          parent->StealFrame(childNIF);
          childNIF->RemoveStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
          if (parent == this) {
            incompleteList.AppendFrame(nullptr, childNIF);
          } else {
            // If childNIF already lives on the next fragment, then we
            // don't need to reparent it, since we know it's destined to end
            // up there anyway.  Just move it to its parent's overflow list.
            if (parent == GetNextInFlow()) {
              nsFrameList toMove(childNIF, childNIF);
              parent->MergeSortedOverflow(toMove);
            } else {
              ReparentFrame(childNIF, parent, this);
              incompleteList.AppendFrame(nullptr, childNIF);
            }
          }
        }
      }
    } else if (aOverflowIncompleteItems.Contains(child)) {
      nsIFrame* childNIF = child->GetNextInFlow();
      if (!childNIF) {
        childNIF = fc->CreateContinuingFrame(child, this);
        childNIF->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
        overflowIncompleteList.AppendFrame(nullptr, childNIF);
      } else {
        DebugOnly<nsContainerFrame*> lastParent = this;
        auto* nif = static_cast<nsContainerFrame*>(GetNextInFlow());
        // If child has any non-overflow-container NIFs, convert them to
        // overflow containers, since that's all |child| needs now.
        while (childNIF &&
               !childNIF->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          auto* parent = childNIF->GetParent();
          parent->StealFrame(childNIF);
          childNIF->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
          if (parent == this) {
            overflowIncompleteList.AppendFrame(nullptr, childNIF);
          } else {
            if (!nif || parent == nif) {
              nsFrameList toMove(childNIF, childNIF);
              parent->MergeSortedExcessOverflowContainers(toMove);
            } else {
              ReparentFrame(childNIF, parent, nif);
              nsFrameList toMove(childNIF, childNIF);
              nif->MergeSortedExcessOverflowContainers(toMove);
            }
            // We only need to reparent the first childNIF (or not at all if
            // its parent is our NIF).
            nif = nullptr;
          }
          lastParent = parent;
          childNIF = childNIF->GetNextInFlow();
        }
      }
    }
    child = next;
  }

  // Merge the results into our respective overflow child lists.
  if (!pushedList.IsEmpty()) {
    MergeSortedOverflow(pushedList);
  }
  if (!incompleteList.IsEmpty()) {
    MergeSortedOverflow(incompleteList);
  }
  if (!overflowIncompleteList.IsEmpty()) {
    // If our next-in-flow already has overflow containers list, merge the
    // overflowIncompleteList into that list. Otherwise, merge it into our
    // excess overflow containers list, to be drained by our next-in-flow.
    auto* nif = static_cast<nsContainerFrame*>(GetNextInFlow());
    nsFrameList* oc = nif ? nif->GetOverflowContainers() : nullptr;
    if (oc) {
      ReparentFrames(overflowIncompleteList, this, nif);
      MergeSortedFrameLists(*oc, overflowIncompleteList, GetContent());
    } else {
      MergeSortedExcessOverflowContainers(overflowIncompleteList);
    }
  }
  return true;
}

void nsContainerFrame::NormalizeChildLists() {
  MOZ_ASSERT(IsFlexOrGridContainer(),
             "Only Flex / Grid containers can call this!");

  // Note: the following description uses grid container as an example. Flex
  // container is similar.
  //
  // First we gather child frames we should include in our reflow/placement,
  // i.e. overflowed children from our prev-in-flow, and pushed first-in-flow
  // children (that might now fit). It's important to note that these children
  // can be in arbitrary order vis-a-vis the current children in our lists.
  // E.g. grid items in the document order: A, B, C may be placed in the rows
  // 3, 2, 1.  Assume each row goes in a separate grid container fragment,
  // and we reflow the second fragment.  Now if C (in fragment 1) overflows,
  // we can't just prepend it to our mFrames like we usually do because that
  // would violate the document order invariant that other code depends on.
  // Similarly if we pull up child A (from fragment 3) we can't just append
  // that for the same reason.  Instead, we must sort these children into
  // our child lists.  (The sorting is trivial given that both lists are
  // already fully sorted individually - it's just a merge.)
  //
  // The invariants that we maintain are that each grid container child list
  // is sorted in the normal document order at all times, but that children
  // in different grid container continuations may be in arbitrary order.

  const auto didPushItemsBit = IsFlexContainerFrame()
                                   ? NS_STATE_FLEX_DID_PUSH_ITEMS
                                   : NS_STATE_GRID_DID_PUSH_ITEMS;
  const auto hasChildNifBit = IsFlexContainerFrame()
                                  ? NS_STATE_FLEX_HAS_CHILD_NIFS
                                  : NS_STATE_GRID_HAS_CHILD_NIFS;

  auto* prevInFlow = static_cast<nsContainerFrame*>(GetPrevInFlow());
  // Merge overflow frames from our prev-in-flow into our principal child list.
  if (prevInFlow) {
    AutoFrameListPtr overflow(PresContext(), prevInFlow->StealOverflowFrames());
    if (overflow) {
      ReparentFrames(*overflow, prevInFlow, this);
      MergeSortedFrameLists(mFrames, *overflow, GetContent());

      // Move trailing next-in-flows into our overflow list.
      nsFrameList continuations;
      for (nsIFrame* f = mFrames.FirstChild(); f;) {
        nsIFrame* next = f->GetNextSibling();
        nsIFrame* pif = f->GetPrevInFlow();
        if (pif && pif->GetParent() == this) {
          mFrames.RemoveFrame(f);
          continuations.AppendFrame(nullptr, f);
        }
        f = next;
      }
      MergeSortedOverflow(continuations);

      // Move prev-in-flow's excess overflow containers list into our own
      // overflow containers list. If we already have an excess overflow
      // containers list, any child in that list which doesn't have a
      // prev-in-flow in this frame is also merged into our overflow container
      // list.
      nsFrameList* overflowContainers =
          DrainExcessOverflowContainersList(MergeSortedFrameListsFor);

      // Move trailing OC next-in-flows into our excess overflow containers
      // list.
      if (overflowContainers) {
        nsFrameList moveToEOC;
        for (nsIFrame* f = overflowContainers->FirstChild(); f;) {
          nsIFrame* next = f->GetNextSibling();
          nsIFrame* pif = f->GetPrevInFlow();
          if (pif && pif->GetParent() == this) {
            overflowContainers->RemoveFrame(f);
            moveToEOC.AppendFrame(nullptr, f);
          }
          f = next;
        }
        if (overflowContainers->IsEmpty()) {
          DestroyOverflowContainers();
        }
        MergeSortedExcessOverflowContainers(moveToEOC);
      }
    }
  }

  // For each item in aItems, pull up its next-in-flow (if any), and reparent it
  // to our next-in-flow, unless its parent is already ourselves or our
  // next-in-flow (to avoid leaving a hole there).
  auto PullItemsNextInFlow = [this](const nsFrameList& aItems) {
    auto* firstNIF = static_cast<nsContainerFrame*>(GetNextInFlow());
    if (!firstNIF) {
      return;
    }
    nsFrameList childNIFs;
    nsFrameList childOCNIFs;
    for (auto* child : aItems) {
      if (auto* childNIF = child->GetNextInFlow()) {
        if (auto* parent = childNIF->GetParent();
            parent != this && parent != firstNIF) {
          parent->StealFrame(childNIF);
          ReparentFrame(childNIF, parent, firstNIF);
          if (childNIF->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER)) {
            childOCNIFs.AppendFrame(nullptr, childNIF);
          } else {
            childNIFs.AppendFrame(nullptr, childNIF);
          }
        }
      }
    }
    // Merge aItems' NIFs into our NIF's respective overflow child lists.
    firstNIF->MergeSortedOverflow(childNIFs);
    firstNIF->MergeSortedExcessOverflowContainers(childOCNIFs);
  };

  // Merge our own overflow frames into our principal child list,
  // except those that are a next-in-flow for one of our items.
  DebugOnly<bool> foundOwnPushedChild = false;
  {
    nsFrameList* ourOverflow = GetOverflowFrames();
    if (ourOverflow) {
      nsFrameList items;
      for (nsIFrame* f = ourOverflow->FirstChild(); f;) {
        nsIFrame* next = f->GetNextSibling();
        nsIFrame* pif = f->GetPrevInFlow();
        if (!pif || pif->GetParent() != this) {
          MOZ_ASSERT(f->GetParent() == this);
          ourOverflow->RemoveFrame(f);
          items.AppendFrame(nullptr, f);
          if (!pif) {
            foundOwnPushedChild = true;
          }
        }
        f = next;
      }

      if (ourOverflow->IsEmpty()) {
        DestroyOverflowList();
        ourOverflow = nullptr;
      }
      if (items.NotEmpty()) {
        PullItemsNextInFlow(items);
      }
      MergeSortedFrameLists(mFrames, items, GetContent());
    }
  }

  // Push any child next-in-flows in our principal list to OverflowList.
  if (HasAnyStateBits(hasChildNifBit)) {
    nsFrameList framesToPush;
    nsIFrame* firstChild = mFrames.FirstChild();
    // Note that we potentially modify our mFrames list as we go.
    for (auto* child = firstChild; child; child = child->GetNextSibling()) {
      if (auto* childNIF = child->GetNextInFlow()) {
        if (childNIF->GetParent() == this) {
          for (auto* c = child->GetNextSibling(); c; c = c->GetNextSibling()) {
            if (c == childNIF) {
              // child's next-in-flow is in our principal child list, push it.
              mFrames.RemoveFrame(childNIF);
              framesToPush.AppendFrame(nullptr, childNIF);
              break;
            }
          }
        }
      }
    }
    if (!framesToPush.IsEmpty()) {
      MergeSortedOverflow(framesToPush);
    }
    RemoveStateBits(hasChildNifBit);
  }

  // Pull up any first-in-flow children we might have pushed.
  if (HasAnyStateBits(didPushItemsBit)) {
    RemoveStateBits(didPushItemsBit);
    nsFrameList items;
    auto* nif = static_cast<nsContainerFrame*>(GetNextInFlow());
    DebugOnly<bool> nifNeedPushedItem = false;
    while (nif) {
      nsFrameList nifItems;
      for (nsIFrame* nifChild = nif->GetChildList(kPrincipalList).FirstChild();
           nifChild;) {
        nsIFrame* next = nifChild->GetNextSibling();
        if (!nifChild->GetPrevInFlow()) {
          nif->StealFrame(nifChild);
          ReparentFrame(nifChild, nif, this);
          nifItems.AppendFrame(nullptr, nifChild);
          nifNeedPushedItem = false;
        }
        nifChild = next;
      }
      MergeSortedFrameLists(items, nifItems, GetContent());

      if (!nif->HasAnyStateBits(didPushItemsBit)) {
        MOZ_ASSERT(!nifNeedPushedItem || mDidPushItemsBitMayLie,
                   "The state bit stored in didPushItemsBit lied!");
        break;
      }
      nifNeedPushedItem = true;

      for (nsIFrame* nifChild = nif->GetChildList(kOverflowList).FirstChild();
           nifChild;) {
        nsIFrame* next = nifChild->GetNextSibling();
        if (!nifChild->GetPrevInFlow()) {
          nif->StealFrame(nifChild);
          ReparentFrame(nifChild, nif, this);
          nifItems.AppendFrame(nullptr, nifChild);
          nifNeedPushedItem = false;
        }
        nifChild = next;
      }
      MergeSortedFrameLists(items, nifItems, GetContent());

      nif->RemoveStateBits(didPushItemsBit);
      nif = static_cast<nsContainerFrame*>(nif->GetNextInFlow());
      MOZ_ASSERT(nif || !nifNeedPushedItem || mDidPushItemsBitMayLie,
                 "The state bit stored in didPushItemsBit lied!");
    }

    if (!items.IsEmpty()) {
      PullItemsNextInFlow(items);
    }

    MOZ_ASSERT(
        foundOwnPushedChild || !items.IsEmpty() || mDidPushItemsBitMayLie,
        "The state bit stored in didPushItemsBit lied!");
    MergeSortedFrameLists(mFrames, items, GetContent());
  }
}

void nsContainerFrame::NoteNewChildren(ChildListID aListID,
                                       const nsFrameList& aFrameList) {
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");
  MOZ_ASSERT(IsFlexOrGridContainer(),
             "Only Flex / Grid containers can call this!");

  mozilla::PresShell* presShell = PresShell();
  const auto didPushItemsBit = IsFlexContainerFrame()
                                   ? NS_STATE_FLEX_DID_PUSH_ITEMS
                                   : NS_STATE_GRID_DID_PUSH_ITEMS;
  for (auto* pif = GetPrevInFlow(); pif; pif = pif->GetPrevInFlow()) {
    pif->AddStateBits(didPushItemsBit);
    presShell->FrameNeedsReflow(pif, IntrinsicDirty::TreeChange,
                                NS_FRAME_IS_DIRTY);
  }
}

bool nsContainerFrame::MoveOverflowToChildList() {
  bool result = false;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)GetPrevInFlow();
  if (nullptr != prevInFlow) {
    AutoFrameListPtr prevOverflowFrames(PresContext(),
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Tables are special; they can have repeated header/footer
      // frames on mFrames at this point.
      NS_ASSERTION(mFrames.IsEmpty() || IsTableFrame(), "bad overflow list");
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(*prevOverflowFrames, prevInFlow,
                                              this);
      mFrames.AppendFrames(this, *prevOverflowFrames);
      result = true;
    }
  }

  // It's also possible that we have an overflow list for ourselves.
  return DrainSelfOverflowList() || result;
}

void nsContainerFrame::MergeSortedOverflow(nsFrameList& aList) {
  if (aList.IsEmpty()) {
    return;
  }
  MOZ_ASSERT(
      !aList.FirstChild()->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER),
      "this is the wrong list to put this child frame");
  MOZ_ASSERT(aList.FirstChild()->GetParent() == this);
  nsFrameList* overflow = GetOverflowFrames();
  if (overflow) {
    MergeSortedFrameLists(*overflow, aList, GetContent());
  } else {
    SetOverflowFrames(std::move(aList));
  }
}

void nsContainerFrame::MergeSortedExcessOverflowContainers(nsFrameList& aList) {
  if (aList.IsEmpty()) {
    return;
  }
  MOZ_ASSERT(
      aList.FirstChild()->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER),
      "this is the wrong list to put this child frame");
  MOZ_ASSERT(aList.FirstChild()->GetParent() == this);
  if (nsFrameList* eoc = GetExcessOverflowContainers()) {
    MergeSortedFrameLists(*eoc, aList, GetContent());
  } else {
    SetExcessOverflowContainers(std::move(aList));
  }
}

nsIFrame* nsContainerFrame::GetFirstNonAnonBoxInSubtree(nsIFrame* aFrame) {
  while (aFrame) {
    // If aFrame isn't an anonymous container, or it's text or such, then it'll
    // do.
    if (!aFrame->Style()->IsAnonBox() ||
        nsCSSAnonBoxes::IsNonElement(aFrame->Style()->GetPseudoType())) {
      break;
    }

    // Otherwise, descend to its first child and repeat.

    // SPECIAL CASE: if we're dealing with an anonymous table, then it might
    // be wrapping something non-anonymous in its caption or col-group lists
    // (instead of its principal child list), so we have to look there.
    // (Note: For anonymous tables that have a non-anon cell *and* a non-anon
    // column, we'll always return the column. This is fine; we're really just
    // looking for a handle to *anything* with a meaningful content node inside
    // the table, for use in DOM comparisons to things outside of the table.)
    if (MOZ_UNLIKELY(aFrame->IsTableWrapperFrame())) {
      nsIFrame* captionDescendant = GetFirstNonAnonBoxInSubtree(
          aFrame->GetChildList(kCaptionList).FirstChild());
      if (captionDescendant) {
        return captionDescendant;
      }
    } else if (MOZ_UNLIKELY(aFrame->IsTableFrame())) {
      nsIFrame* colgroupDescendant = GetFirstNonAnonBoxInSubtree(
          aFrame->GetChildList(kColGroupList).FirstChild());
      if (colgroupDescendant) {
        return colgroupDescendant;
      }
    }

    // USUAL CASE: Descend to the first child in principal list.
    aFrame = aFrame->PrincipalChildList().FirstChild();
  }
  return aFrame;
}

/**
 * Is aFrame1 a prev-continuation of aFrame2?
 */
static bool IsPrevContinuationOf(nsIFrame* aFrame1, nsIFrame* aFrame2) {
  nsIFrame* prev = aFrame2;
  while ((prev = prev->GetPrevContinuation())) {
    if (prev == aFrame1) {
      return true;
    }
  }
  return false;
}

void nsContainerFrame::MergeSortedFrameLists(nsFrameList& aDest,
                                             nsFrameList& aSrc,
                                             nsIContent* aCommonAncestor) {
  // Returns a frame whose DOM node can be used for the purpose of ordering
  // aFrame among its sibling frames by DOM position. If aFrame is
  // non-anonymous, this just returns aFrame itself. Otherwise, this returns the
  // first non-anonymous descendant in aFrame's continuation chain.
  auto FrameForDOMPositionComparison = [](nsIFrame* aFrame) {
    if (!aFrame->Style()->IsAnonBox()) {
      // The usual case.
      return aFrame;
    }

    // Walk the continuation chain from the start, and return the first
    // non-anonymous descendant that we find.
    for (nsIFrame* f = aFrame->FirstContinuation(); f;
         f = f->GetNextContinuation()) {
      if (nsIFrame* nonAnonBox = GetFirstNonAnonBoxInSubtree(f)) {
        return nonAnonBox;
      }
    }

    MOZ_ASSERT_UNREACHABLE(
        "Why is there no non-anonymous descendants in the continuation chain?");
    return aFrame;
  };

  nsIFrame* dest = aDest.FirstChild();
  for (nsIFrame* src = aSrc.FirstChild(); src;) {
    if (!dest) {
      aDest.AppendFrames(nullptr, aSrc);
      break;
    }
    nsIContent* srcContent = FrameForDOMPositionComparison(src)->GetContent();
    nsIContent* destContent = FrameForDOMPositionComparison(dest)->GetContent();
    int32_t result = nsLayoutUtils::CompareTreePosition(srcContent, destContent,
                                                        aCommonAncestor);
    if (MOZ_UNLIKELY(result == 0)) {
      // NOTE: we get here when comparing ::before/::after for the same element.
      if (MOZ_UNLIKELY(srcContent->IsGeneratedContentContainerForBefore())) {
        if (MOZ_LIKELY(!destContent->IsGeneratedContentContainerForBefore()) ||
            ::IsPrevContinuationOf(src, dest)) {
          result = -1;
        }
      } else if (MOZ_UNLIKELY(
                     srcContent->IsGeneratedContentContainerForAfter())) {
        if (MOZ_UNLIKELY(destContent->IsGeneratedContentContainerForAfter()) &&
            ::IsPrevContinuationOf(src, dest)) {
          result = -1;
        }
      } else if (::IsPrevContinuationOf(src, dest)) {
        result = -1;
      }
    }
    if (result < 0) {
      // src should come before dest
      nsIFrame* next = src->GetNextSibling();
      aSrc.RemoveFrame(src);
      aDest.InsertFrame(nullptr, dest->GetPrevSibling(), src);
      src = next;
    } else {
      dest = dest->GetNextSibling();
    }
  }
  MOZ_ASSERT(aSrc.IsEmpty());
}

bool nsContainerFrame::MoveInlineOverflowToChildList(nsIFrame* aLineContainer) {
  MOZ_ASSERT(aLineContainer,
             "Must have line container for moving inline overflows");

  bool result = false;

  // Check for an overflow list with our prev-in-flow
  if (auto prevInFlow = static_cast<nsContainerFrame*>(GetPrevInFlow())) {
    AutoFrameListPtr prevOverflowFrames(PresContext(),
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // We may need to reparent floats from prev-in-flow to our line
      // container if the container has prev continuation.
      if (aLineContainer->GetPrevContinuation()) {
        ReparentFloatsForInlineChild(aLineContainer,
                                     prevOverflowFrames->FirstChild(), true);
      }
      // When pushing and pulling frames we need to check for whether
      // any views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(*prevOverflowFrames, prevInFlow,
                                              this);
      // Prepend overflow frames to the list.
      mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
      result = true;
    }
  }

  // It's also possible that we have overflow list for ourselves.
  return DrainSelfOverflowList() || result;
}

bool nsContainerFrame::DrainSelfOverflowList() {
  AutoFrameListPtr overflowFrames(PresContext(), StealOverflowFrames());
  if (overflowFrames) {
    mFrames.AppendFrames(nullptr, *overflowFrames);
    return true;
  }
  return false;
}

bool nsContainerFrame::DrainAndMergeSelfOverflowList() {
  MOZ_ASSERT(IsFlexOrGridContainer(),
             "Only Flex / Grid containers can call this!");

  // Unlike nsContainerFrame::DrainSelfOverflowList, flex or grid containers
  // need to merge these lists so that the resulting mFrames is in document
  // content order.
  // NOTE: nsContainerFrame::AppendFrames/InsertFrames calls this method and
  // there are also direct calls from the fctor (FindAppendPrevSibling).
  AutoFrameListPtr overflowFrames(PresContext(), StealOverflowFrames());
  if (overflowFrames) {
    MergeSortedFrameLists(mFrames, *overflowFrames, GetContent());
    // We set a frame bit to push them again in Reflow() to avoid creating
    // multiple flex / grid items per flex / grid container fragment for the
    // same content.
    AddStateBits(IsFlexContainerFrame() ? NS_STATE_FLEX_HAS_CHILD_NIFS
                                        : NS_STATE_GRID_HAS_CHILD_NIFS);
    return true;
  }
  return false;
}

nsFrameList* nsContainerFrame::DrainExcessOverflowContainersList(
    ChildFrameMerger aMergeFunc) {
  nsFrameList* overflowContainers = GetOverflowContainers();

  // Drain excess overflow containers from our prev-in-flow.
  if (auto* prev = static_cast<nsContainerFrame*>(GetPrevInFlow())) {
    AutoFrameListPtr excessFrames(PresContext(),
                                  prev->StealExcessOverflowContainers());
    if (excessFrames) {
      excessFrames->ApplySetParent(this);
      nsContainerFrame::ReparentFrameViewList(*excessFrames, prev, this);
      if (overflowContainers) {
        // The default merge function is AppendFrames, so we use excessFrames as
        // the destination and then assign the result to overflowContainers.
        aMergeFunc(*excessFrames, *overflowContainers, this);
        *overflowContainers = std::move(*excessFrames);
      } else {
        overflowContainers = SetOverflowContainers(std::move(*excessFrames));
      }
    }
  }

  // Our own excess overflow containers from a previous reflow can still be
  // present if our next-in-flow hasn't been reflown yet.  Move any children
  // from it that don't have a continuation in this frame to the
  // OverflowContainers list.
  AutoFrameListPtr selfExcessOCFrames(PresContext(),
                                      StealExcessOverflowContainers());
  if (selfExcessOCFrames) {
    nsFrameList toMove;
    auto child = selfExcessOCFrames->FirstChild();
    while (child) {
      auto next = child->GetNextSibling();
      MOZ_ASSERT(child->GetPrevInFlow(),
                 "ExcessOverflowContainers frames must be continuations");
      if (child->GetPrevInFlow()->GetParent() != this) {
        selfExcessOCFrames->RemoveFrame(child);
        toMove.AppendFrame(nullptr, child);
      }
      child = next;
    }

    // If there's any remaining excess overflow containers, put them back.
    if (selfExcessOCFrames->NotEmpty()) {
      SetExcessOverflowContainers(std::move(*selfExcessOCFrames));
    }

    if (toMove.NotEmpty()) {
      if (overflowContainers) {
        aMergeFunc(*overflowContainers, toMove, this);
      } else {
        overflowContainers = SetOverflowContainers(std::move(toMove));
      }
    }
  }

  return overflowContainers;
}

nsIFrame* nsContainerFrame::GetNextInFlowChild(
    ContinuationTraversingState& aState, bool* aIsInOverflow) {
  nsContainerFrame*& nextInFlow = aState.mNextInFlow;
  while (nextInFlow) {
    // See if there is any frame in the container
    nsIFrame* frame = nextInFlow->mFrames.FirstChild();
    if (frame) {
      if (aIsInOverflow) {
        *aIsInOverflow = false;
      }
      return frame;
    }
    // No frames in the principal list, try its overflow list
    nsFrameList* overflowFrames = nextInFlow->GetOverflowFrames();
    if (overflowFrames) {
      if (aIsInOverflow) {
        *aIsInOverflow = true;
      }
      return overflowFrames->FirstChild();
    }
    nextInFlow = static_cast<nsContainerFrame*>(nextInFlow->GetNextInFlow());
  }
  return nullptr;
}

nsIFrame* nsContainerFrame::PullNextInFlowChild(
    ContinuationTraversingState& aState) {
  bool isInOverflow;
  nsIFrame* frame = GetNextInFlowChild(aState, &isInOverflow);
  if (frame) {
    nsContainerFrame* nextInFlow = aState.mNextInFlow;
    if (isInOverflow) {
      nsFrameList* overflowFrames = nextInFlow->GetOverflowFrames();
      overflowFrames->RemoveFirstChild();
      if (overflowFrames->IsEmpty()) {
        nextInFlow->DestroyOverflowList();
      }
    } else {
      nextInFlow->mFrames.RemoveFirstChild();
    }

    // Move the frame to the principal frame list of this container
    mFrames.AppendFrame(this, frame);
    // AppendFrame has reparented the frame, we need
    // to reparent the frame view then.
    nsContainerFrame::ReparentFrameView(frame, nextInFlow, this);
  }
  return frame;
}

/* static */
void nsContainerFrame::ReparentFloatsForInlineChild(nsIFrame* aOurLineContainer,
                                                    nsIFrame* aFrame,
                                                    bool aReparentSiblings) {
  // XXXbz this would be better if it took a nsFrameList or a frame
  // list slice....
  NS_ASSERTION(aOurLineContainer->GetNextContinuation() ||
                   aOurLineContainer->GetPrevContinuation(),
               "Don't call this when we have no continuation, it's a waste");
  if (!aFrame) {
    NS_ASSERTION(aReparentSiblings, "Why did we get called?");
    return;
  }

  nsBlockFrame* frameBlock = nsLayoutUtils::GetFloatContainingBlock(aFrame);
  if (!frameBlock || frameBlock == aOurLineContainer) {
    return;
  }

  nsBlockFrame* ourBlock = do_QueryFrame(aOurLineContainer);
  NS_ASSERTION(ourBlock, "Not a block, but broke vertically?");

  while (true) {
    ourBlock->ReparentFloats(aFrame, frameBlock, false);

    if (!aReparentSiblings) return;
    nsIFrame* next = aFrame->GetNextSibling();
    if (!next) return;
    if (next->GetParent() == aFrame->GetParent()) {
      aFrame = next;
      continue;
    }
    // This is paranoid and will hardly ever get hit ... but we can't actually
    // trust that the frames in the sibling chain all have the same parent,
    // because lazy reparenting may be going on. If we find a different
    // parent we need to redo our analysis.
    ReparentFloatsForInlineChild(aOurLineContainer, next, aReparentSiblings);
    return;
  }
}

bool nsContainerFrame::ResolvedOrientationIsVertical() {
  StyleOrient orient = StyleDisplay()->mOrient;
  switch (orient) {
    case StyleOrient::Horizontal:
      return false;
    case StyleOrient::Vertical:
      return true;
    case StyleOrient::Inline:
      return GetWritingMode().IsVertical();
    case StyleOrient::Block:
      return !GetWritingMode().IsVertical();
  }
  MOZ_ASSERT_UNREACHABLE("unexpected -moz-orient value");
  return false;
}

LogicalSize nsContainerFrame::ComputeSizeWithIntrinsicDimensions(
    gfxContext* aRenderingContext, WritingMode aWM,
    const IntrinsicSize& aIntrinsicSize, const AspectRatio& aAspectRatio,
    const LogicalSize& aCBSize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  const nsStylePosition* stylePos = StylePosition();
  const auto& styleISize = aSizeOverrides.mStyleISize
                               ? *aSizeOverrides.mStyleISize
                               : stylePos->ISize(aWM);
  const auto& styleBSize = aSizeOverrides.mStyleBSize
                               ? *aSizeOverrides.mStyleBSize
                               : stylePos->BSize(aWM);
  const auto& aspectRatio =
      aSizeOverrides.mAspectRatio ? *aSizeOverrides.mAspectRatio : aAspectRatio;

  auto* parentFrame = GetParent();
  const bool isGridItem = IsGridItem();
  const bool isFlexItem =
      IsFlexItem() &&
      !parentFrame->HasAnyStateBits(NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX);
  // This variable only gets set (and used) if isFlexItem is true.  It
  // indicates which axis (in this frame's own WM) corresponds to its
  // flex container's main axis.
  LogicalAxis flexMainAxis =
      eLogicalAxisInline;  // (init to make valgrind happy)

  if (isFlexItem) {
    flexMainAxis = nsFlexContainerFrame::IsItemInlineAxisMainAxis(this)
                       ? eLogicalAxisInline
                       : eLogicalAxisBlock;
  }

  // Handle intrinsic sizes and their interaction with
  // {min-,max-,}{width,height} according to the rules in
  // http://www.w3.org/TR/CSS21/visudet.html#min-max-widths

  // Note: throughout the following section of the function, I avoid
  // a * (b / c) because of its reduced accuracy relative to a * b / c
  // or (a * b) / c (which are equivalent).

  const bool isAutoISize = styleISize.IsAuto();
  const bool isAutoBSize =
      nsLayoutUtils::IsAutoBSize(styleBSize, aCBSize.BSize(aWM)) ||
      aFlags.contains(ComputeSizeFlag::UseAutoBSize);

  const auto boxSizingAdjust = stylePos->mBoxSizing == StyleBoxSizing::Border
                                   ? aBorderPadding
                                   : LogicalSize(aWM);
  const nscoord boxSizingToMarginEdgeISize = aMargin.ISize(aWM) +
                                             aBorderPadding.ISize(aWM) -
                                             boxSizingAdjust.ISize(aWM);

  nscoord iSize, minISize, maxISize, bSize, minBSize, maxBSize;
  enum class Stretch {
    // stretch to fill the CB (preserving intrinsic ratio) in the relevant axis
    StretchPreservingRatio,  // XXX not used yet
    // stretch to fill the CB in the relevant axis
    Stretch,
    // no stretching in the relevant axis
    NoStretch,
  };
  // just to avoid having to type these out everywhere:
  const auto eStretchPreservingRatio = Stretch::StretchPreservingRatio;
  const auto eStretch = Stretch::Stretch;
  const auto eNoStretch = Stretch::NoStretch;

  Stretch stretchI = eNoStretch;  // stretch behavior in the inline axis
  Stretch stretchB = eNoStretch;  // stretch behavior in the block axis

  const bool isOrthogonal = aWM.IsOrthogonalTo(parentFrame->GetWritingMode());
  const bool isVertical = aWM.IsVertical();
  const LogicalSize fallbackIntrinsicSize(aWM, kFallbackIntrinsicSize);
  const auto& isizeCoord =
      isVertical ? aIntrinsicSize.height : aIntrinsicSize.width;
  const bool hasIntrinsicISize = isizeCoord.isSome();
  nscoord intrinsicISize = std::max(0, isizeCoord.valueOr(0));

  const auto& bsizeCoord =
      isVertical ? aIntrinsicSize.width : aIntrinsicSize.height;
  const bool hasIntrinsicBSize = bsizeCoord.isSome();
  nscoord intrinsicBSize = std::max(0, bsizeCoord.valueOr(0));

  if (!isAutoISize) {
    iSize = ComputeISizeValue(aRenderingContext, aWM, aCBSize, boxSizingAdjust,
                              boxSizingToMarginEdgeISize, styleISize, aFlags)
                .mISize;
  } else if (MOZ_UNLIKELY(isGridItem) &&
             !parentFrame->IsMasonry(isOrthogonal ? eLogicalAxisBlock
                                                  : eLogicalAxisInline)) {
    MOZ_ASSERT(!IsTrueOverflowContainer());
    // 'auto' inline-size for grid-level box - apply 'stretch' as needed:
    auto cbSize = aCBSize.ISize(aWM);
    if (cbSize != NS_UNCONSTRAINEDSIZE) {
      if (!StyleMargin()->HasInlineAxisAuto(aWM)) {
        auto inlineAxisAlignment =
            isOrthogonal ? stylePos->UsedAlignSelf(GetParent()->Style())._0
                         : stylePos->UsedJustifySelf(GetParent()->Style())._0;
        if (inlineAxisAlignment == StyleAlignFlags::STRETCH) {
          stretchI = eStretch;
        }
      }
      if (stretchI != eNoStretch ||
          aFlags.contains(ComputeSizeFlag::IClampMarginBoxMinSize)) {
        iSize = std::max(nscoord(0), cbSize - aBorderPadding.ISize(aWM) -
                                         aMargin.ISize(aWM));
      }
    } else {
      // Reset this flag to avoid applying the clamping below.
      aFlags -= ComputeSizeFlag::IClampMarginBoxMinSize;
    }
  }

  const auto& maxISizeCoord = stylePos->MaxISize(aWM);

  if (!maxISizeCoord.IsNone() &&
      !(isFlexItem && flexMainAxis == eLogicalAxisInline)) {
    maxISize =
        ComputeISizeValue(aRenderingContext, aWM, aCBSize, boxSizingAdjust,
                          boxSizingToMarginEdgeISize, maxISizeCoord, aFlags)
            .mISize;
  } else {
    maxISize = nscoord_MAX;
  }

  // NOTE: Flex items ignore their min & max sizing properties in their
  // flex container's main-axis.  (Those properties get applied later in
  // the flexbox algorithm.)

  const auto& minISizeCoord = stylePos->MinISize(aWM);

  if (!minISizeCoord.IsAuto() &&
      !(isFlexItem && flexMainAxis == eLogicalAxisInline)) {
    minISize =
        ComputeISizeValue(aRenderingContext, aWM, aCBSize, boxSizingAdjust,
                          boxSizingToMarginEdgeISize, minISizeCoord, aFlags)
            .mISize;
  } else {
    // Treat "min-width: auto" as 0.
    // NOTE: Technically, "auto" is supposed to behave like "min-content" on
    // flex items. However, we don't need to worry about that here, because
    // flex items' min-sizes are intentionally ignored until the flex
    // container explicitly considers them during space distribution.
    minISize = 0;
  }

  if (!isAutoBSize) {
    bSize = nsLayoutUtils::ComputeBSizeValue(aCBSize.BSize(aWM),
                                             boxSizingAdjust.BSize(aWM),
                                             styleBSize.AsLengthPercentage());
  } else if (MOZ_UNLIKELY(isGridItem) &&
             !parentFrame->IsMasonry(isOrthogonal ? eLogicalAxisInline
                                                  : eLogicalAxisBlock)) {
    MOZ_ASSERT(!IsTrueOverflowContainer());
    // 'auto' block-size for grid-level box - apply 'stretch' as needed:
    auto cbSize = aCBSize.BSize(aWM);
    if (cbSize != NS_UNCONSTRAINEDSIZE) {
      if (!StyleMargin()->HasBlockAxisAuto(aWM)) {
        auto blockAxisAlignment =
            !isOrthogonal ? stylePos->UsedAlignSelf(GetParent()->Style())._0
                          : stylePos->UsedJustifySelf(GetParent()->Style())._0;
        if (blockAxisAlignment == StyleAlignFlags::STRETCH) {
          stretchB = eStretch;
        }
      }
      if (stretchB != eNoStretch ||
          aFlags.contains(ComputeSizeFlag::BClampMarginBoxMinSize)) {
        bSize = std::max(nscoord(0), cbSize - aBorderPadding.BSize(aWM) -
                                         aMargin.BSize(aWM));
      }
    } else {
      // Reset this flag to avoid applying the clamping below.
      aFlags -= ComputeSizeFlag::BClampMarginBoxMinSize;
    }
  }

  const auto& maxBSizeCoord = stylePos->MaxBSize(aWM);

  if (!nsLayoutUtils::IsAutoBSize(maxBSizeCoord, aCBSize.BSize(aWM)) &&
      !(isFlexItem && flexMainAxis == eLogicalAxisBlock)) {
    maxBSize = nsLayoutUtils::ComputeBSizeValue(
        aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
        maxBSizeCoord.AsLengthPercentage());
  } else {
    maxBSize = nscoord_MAX;
  }

  const auto& minBSizeCoord = stylePos->MinBSize(aWM);

  if (!nsLayoutUtils::IsAutoBSize(minBSizeCoord, aCBSize.BSize(aWM)) &&
      !(isFlexItem && flexMainAxis == eLogicalAxisBlock)) {
    minBSize = nsLayoutUtils::ComputeBSizeValue(
        aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
        minBSizeCoord.AsLengthPercentage());
  } else {
    minBSize = 0;
  }

  NS_ASSERTION(aCBSize.ISize(aWM) != NS_UNCONSTRAINEDSIZE,
               "Our containing block must not have unconstrained inline-size!");

  // Now calculate the used values for iSize and bSize:
  if (isAutoISize) {
    if (isAutoBSize) {
      // 'auto' iSize, 'auto' bSize

      // Get tentative values - CSS 2.1 sections 10.3.2 and 10.6.2:

      nscoord tentISize, tentBSize;

      if (hasIntrinsicISize) {
        tentISize = intrinsicISize;
      } else if (hasIntrinsicBSize && aspectRatio) {
        tentISize = aspectRatio.ComputeRatioDependentSize(
            LogicalAxis::eLogicalAxisInline, aWM, intrinsicBSize,
            boxSizingAdjust);
      } else if (aspectRatio) {
        tentISize =
            aCBSize.ISize(aWM) - boxSizingToMarginEdgeISize;  // XXX scrollbar?
        if (tentISize < 0) {
          tentISize = 0;
        }
      } else {
        tentISize = fallbackIntrinsicSize.ISize(aWM);
      }

      // If we need to clamp the inline size to fit the CB, we use the 'stretch'
      // or 'normal' codepath.  We use the ratio-preserving 'normal' codepath
      // unless we have 'stretch' in the other axis.
      if (aFlags.contains(ComputeSizeFlag::IClampMarginBoxMinSize) &&
          stretchI != eStretch && tentISize > iSize) {
        stretchI = (stretchB == eStretch ? eStretch : eStretchPreservingRatio);
      }

      if (hasIntrinsicBSize) {
        tentBSize = intrinsicBSize;
      } else if (aspectRatio) {
        tentBSize = aspectRatio.ComputeRatioDependentSize(
            LogicalAxis::eLogicalAxisBlock, aWM, tentISize, boxSizingAdjust);
      } else {
        tentBSize = fallbackIntrinsicSize.BSize(aWM);
      }

      // (ditto the comment about clamping the inline size above)
      if (aFlags.contains(ComputeSizeFlag::BClampMarginBoxMinSize) &&
          stretchB != eStretch && tentBSize > bSize) {
        stretchB = (stretchI == eStretch ? eStretch : eStretchPreservingRatio);
      }

      if (stretchI == eStretch) {
        tentISize = iSize;  // * / 'stretch'
        if (stretchB == eStretch) {
          tentBSize = bSize;  // 'stretch' / 'stretch'
        } else if (stretchB == eStretchPreservingRatio && aspectRatio) {
          // 'normal' / 'stretch'
          tentBSize = aspectRatio.ComputeRatioDependentSize(
              LogicalAxis::eLogicalAxisBlock, aWM, iSize, boxSizingAdjust);
        }
      } else if (stretchB == eStretch) {
        tentBSize = bSize;  // 'stretch' / * (except 'stretch')
        if (stretchI == eStretchPreservingRatio && aspectRatio) {
          // 'stretch' / 'normal'
          tentISize = aspectRatio.ComputeRatioDependentSize(
              LogicalAxis::eLogicalAxisInline, aWM, bSize, boxSizingAdjust);
        }
      } else if (stretchI == eStretchPreservingRatio && aspectRatio) {
        tentISize = iSize;  // * (except 'stretch') / 'normal'
        tentBSize = aspectRatio.ComputeRatioDependentSize(
            LogicalAxis::eLogicalAxisBlock, aWM, iSize, boxSizingAdjust);
        if (stretchB == eStretchPreservingRatio && tentBSize > bSize) {
          // Stretch within the CB size with preserved intrinsic ratio.
          tentBSize = bSize;  // 'normal' / 'normal'
          tentISize = aspectRatio.ComputeRatioDependentSize(
              LogicalAxis::eLogicalAxisInline, aWM, bSize, boxSizingAdjust);
        }
      } else if (stretchB == eStretchPreservingRatio && aspectRatio) {
        tentBSize = bSize;  // 'normal' / * (except 'normal' and 'stretch')
        tentISize = aspectRatio.ComputeRatioDependentSize(
            LogicalAxis::eLogicalAxisInline, aWM, bSize, boxSizingAdjust);
      }

      // ComputeAutoSizeWithIntrinsicDimensions preserves the ratio when
      // applying the min/max-size.  We don't want that when we have 'stretch'
      // in either axis because tentISize/tentBSize is likely not according to
      // ratio now.
      if (aspectRatio && stretchI != eStretch && stretchB != eStretch) {
        nsSize autoSize = nsLayoutUtils::ComputeAutoSizeWithIntrinsicDimensions(
            minISize, minBSize, maxISize, maxBSize, tentISize, tentBSize);
        // The nsSize that ComputeAutoSizeWithIntrinsicDimensions returns will
        // actually contain logical values if the parameters passed to it were
        // logical coordinates, so we do NOT perform a physical-to-logical
        // conversion here, but just assign the fields directly to our result.
        iSize = autoSize.width;
        bSize = autoSize.height;
      } else {
        // Not honoring an intrinsic ratio: clamp the dimensions independently.
        iSize = NS_CSS_MINMAX(tentISize, minISize, maxISize);
        bSize = NS_CSS_MINMAX(tentBSize, minBSize, maxBSize);
      }
    } else {
      // 'auto' iSize, non-'auto' bSize
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);
      if (stretchI != eStretch) {
        if (aspectRatio) {
          iSize = aspectRatio.ComputeRatioDependentSize(
              LogicalAxis::eLogicalAxisInline, aWM, bSize, boxSizingAdjust);
        } else if (hasIntrinsicISize) {
          if (!(aFlags.contains(ComputeSizeFlag::IClampMarginBoxMinSize) &&
                intrinsicISize > iSize)) {
            iSize = intrinsicISize;
          }  // else - leave iSize as is to fill the CB
        } else {
          iSize = fallbackIntrinsicSize.ISize(aWM);
        }
      }  // else - leave iSize as is to fill the CB
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
    }
  } else {
    if (isAutoBSize) {
      // non-'auto' iSize, 'auto' bSize
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
      if (stretchB != eStretch) {
        if (aspectRatio) {
          bSize = aspectRatio.ComputeRatioDependentSize(
              LogicalAxis::eLogicalAxisBlock, aWM, iSize, boxSizingAdjust);
        } else if (hasIntrinsicBSize) {
          if (!(aFlags.contains(ComputeSizeFlag::BClampMarginBoxMinSize) &&
                intrinsicBSize > bSize)) {
            bSize = intrinsicBSize;
          }  // else - leave bSize as is to fill the CB
        } else {
          bSize = fallbackIntrinsicSize.BSize(aWM);
        }
      }  // else - leave bSize as is to fill the CB
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);

    } else {
      // non-'auto' iSize, non-'auto' bSize
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);
    }
  }

  return LogicalSize(aWM, iSize, bSize);
}

nsRect nsContainerFrame::ComputeSimpleTightBounds(
    DrawTarget* aDrawTarget) const {
  if (StyleOutline()->ShouldPaintOutline() || StyleBorder()->HasBorder() ||
      !StyleBackground()->IsTransparent(this) ||
      StyleDisplay()->HasAppearance()) {
    // Not necessarily tight, due to clipping, negative
    // outline-offset, and lots of other issues, but that's OK
    return InkOverflowRect();
  }

  nsRect r(0, 0, 0, 0);
  for (const auto& childLists : ChildLists()) {
    for (nsIFrame* child : childLists.mList) {
      r.UnionRect(
          r, child->ComputeTightBounds(aDrawTarget) + child->GetPosition());
    }
  }
  return r;
}

void nsContainerFrame::PushDirtyBitToAbsoluteFrames() {
  if (!HasAnyStateBits(NS_FRAME_IS_DIRTY)) {
    return;  // No dirty bit to push.
  }
  if (!HasAbsolutelyPositionedChildren()) {
    return;  // No absolute children to push to.
  }
  GetAbsoluteContainingBlock()->MarkAllFramesDirty();
}

// Define the MAX_FRAME_DEPTH to be the ContentSink's MAX_REFLOW_DEPTH plus
// 4 for the frames above the document's frames:
//  the Viewport, GFXScroll, ScrollPort, and Canvas
#define MAX_FRAME_DEPTH (MAX_REFLOW_DEPTH + 4)

bool nsContainerFrame::IsFrameTreeTooDeep(const ReflowInput& aReflowInput,
                                          ReflowOutput& aMetrics,
                                          nsReflowStatus& aStatus) {
  if (aReflowInput.mReflowDepth > MAX_FRAME_DEPTH) {
    NS_WARNING("frame tree too deep; setting zero size and returning");
    AddStateBits(NS_FRAME_TOO_DEEP_IN_FRAME_TREE);
    ClearOverflowRects();
    aMetrics.ClearSize();
    aMetrics.SetBlockStartAscent(0);
    aMetrics.mCarriedOutBEndMargin.Zero();
    aMetrics.mOverflowAreas.Clear();

    aStatus.Reset();
    if (GetNextInFlow()) {
      // Reflow depth might vary between reflows, so we might have
      // successfully reflowed and split this frame before.  If so, we
      // shouldn't delete its continuations.
      aStatus.SetIncomplete();
    }

    return true;
  }
  RemoveStateBits(NS_FRAME_TOO_DEEP_IN_FRAME_TREE);
  return false;
}

bool nsContainerFrame::ShouldAvoidBreakInside(
    const ReflowInput& aReflowInput) const {
  const auto* disp = StyleDisplay();
  return !aReflowInput.mFlags.mIsTopOfPage &&
         StyleBreakWithin::Avoid == disp->mBreakInside &&
         !IsAbsolutelyPositioned(disp) && !GetPrevInFlow();
}

void nsContainerFrame::ConsiderChildOverflow(OverflowAreas& aOverflowAreas,
                                             nsIFrame* aChildFrame) {
  if (StyleDisplay()->IsContainLayout() &&
      IsFrameOfType(eSupportsContainLayoutAndPaint)) {
    // If we have layout containment and are not a non-atomic, inline-level
    // principal box, we should only consider our child's visual (ink) overflow,
    // leaving the scrollable regions of the parent unaffected.
    // Note: scrollable overflow is a subset of ink overflow,
    // so this has the same affect as unioning the child's visual and
    // scrollable overflow with the parent's ink overflow.
    nsRect childVisual = aChildFrame->InkOverflowRect();
    OverflowAreas combined = OverflowAreas(childVisual, nsRect());
    aOverflowAreas.UnionWith(combined + aChildFrame->GetPosition());
  } else {
    aOverflowAreas.UnionWith(aChildFrame->GetOverflowAreasRelativeToParent());
  }
}

StyleAlignFlags nsContainerFrame::CSSAlignmentForAbsPosChild(
    const ReflowInput& aChildRI, LogicalAxis aLogicalAxis) const {
  MOZ_ASSERT(aChildRI.mFrame->IsAbsolutelyPositioned(),
             "This method should only be called for abspos children");
  NS_ERROR(
      "Child classes that use css box alignment for abspos children "
      "should provide their own implementation of this method!");

  // In the unexpected/unlikely event that this implementation gets invoked,
  // just use "start" alignment.
  return StyleAlignFlags::START;
}

nsOverflowContinuationTracker::nsOverflowContinuationTracker(
    nsContainerFrame* aFrame, bool aWalkOOFFrames,
    bool aSkipOverflowContainerChildren)
    : mOverflowContList(nullptr),
      mPrevOverflowCont(nullptr),
      mSentry(nullptr),
      mParent(aFrame),
      mSkipOverflowContainerChildren(aSkipOverflowContainerChildren),
      mWalkOOFFrames(aWalkOOFFrames) {
  MOZ_ASSERT(aFrame, "null frame pointer");
  SetupOverflowContList();
}

void nsOverflowContinuationTracker::SetupOverflowContList() {
  MOZ_ASSERT(mParent, "null frame pointer");
  MOZ_ASSERT(!mOverflowContList, "already have list");
  nsContainerFrame* nif =
      static_cast<nsContainerFrame*>(mParent->GetNextInFlow());
  if (nif) {
    mOverflowContList = nif->GetOverflowContainers();
    if (mOverflowContList) {
      mParent = nif;
      SetUpListWalker();
    }
  }
  if (!mOverflowContList) {
    mOverflowContList = mParent->GetExcessOverflowContainers();
    if (mOverflowContList) {
      SetUpListWalker();
    }
  }
}

/**
 * Helper function to walk past overflow continuations whose prev-in-flow
 * isn't a normal child and to set mSentry and mPrevOverflowCont correctly.
 */
void nsOverflowContinuationTracker::SetUpListWalker() {
  NS_ASSERTION(!mSentry && !mPrevOverflowCont,
               "forgot to reset mSentry or mPrevOverflowCont");
  if (mOverflowContList) {
    nsIFrame* cur = mOverflowContList->FirstChild();
    if (mSkipOverflowContainerChildren) {
      while (cur && cur->GetPrevInFlow()->HasAnyStateBits(
                        NS_FRAME_IS_OVERFLOW_CONTAINER)) {
        mPrevOverflowCont = cur;
        cur = cur->GetNextSibling();
      }
      while (cur &&
             (cur->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) != mWalkOOFFrames)) {
        mPrevOverflowCont = cur;
        cur = cur->GetNextSibling();
      }
    }
    if (cur) {
      mSentry = cur->GetPrevInFlow();
    }
  }
}

/**
 * Helper function to step forward through the overflow continuations list.
 * Sets mSentry and mPrevOverflowCont, skipping over OOF or non-OOF frames
 * as appropriate. May only be called when we have already set up an
 * mOverflowContList; mOverflowContList cannot be null.
 */
void nsOverflowContinuationTracker::StepForward() {
  MOZ_ASSERT(mOverflowContList, "null list");

  // Step forward
  if (mPrevOverflowCont) {
    mPrevOverflowCont = mPrevOverflowCont->GetNextSibling();
  } else {
    mPrevOverflowCont = mOverflowContList->FirstChild();
  }

  // Skip over oof or non-oof frames as appropriate
  if (mSkipOverflowContainerChildren) {
    nsIFrame* cur = mPrevOverflowCont->GetNextSibling();
    while (cur &&
           (cur->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) != mWalkOOFFrames)) {
      mPrevOverflowCont = cur;
      cur = cur->GetNextSibling();
    }
  }

  // Set up the sentry
  mSentry = (mPrevOverflowCont->GetNextSibling())
                ? mPrevOverflowCont->GetNextSibling()->GetPrevInFlow()
                : nullptr;
}

nsresult nsOverflowContinuationTracker::Insert(nsIFrame* aOverflowCont,
                                               nsReflowStatus& aReflowStatus) {
  MOZ_ASSERT(aOverflowCont, "null frame pointer");
  MOZ_ASSERT(!mSkipOverflowContainerChildren ||
                 mWalkOOFFrames ==
                     aOverflowCont->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
             "shouldn't insert frame that doesn't match walker type");
  MOZ_ASSERT(aOverflowCont->GetPrevInFlow(),
             "overflow containers must have a prev-in-flow");

  nsresult rv = NS_OK;
  bool reparented = false;
  nsPresContext* presContext = aOverflowCont->PresContext();
  bool addToList = !mSentry || aOverflowCont != mSentry->GetNextInFlow();

  // If we have a list and aOverflowCont is already in it then don't try to
  // add it again.
  if (addToList && aOverflowCont->GetParent() == mParent &&
      aOverflowCont->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER) &&
      mOverflowContList && mOverflowContList->ContainsFrame(aOverflowCont)) {
    addToList = false;
    mPrevOverflowCont = aOverflowCont->GetPrevSibling();
  }

  if (addToList) {
    if (aOverflowCont->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER)) {
      // aOverflowCont is in some other overflow container list,
      // steal it first
      NS_ASSERTION(!(mOverflowContList &&
                     mOverflowContList->ContainsFrame(aOverflowCont)),
                   "overflow containers out of order");
      aOverflowCont->GetParent()->StealFrame(aOverflowCont);
    } else {
      aOverflowCont->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
    }
    if (!mOverflowContList) {
      // Note: We don't use SetExcessOverflowContainers() since it requires
      // setting a non-empty list. It's OK to manually set an empty list to
      // ExcessOverflowContainersProperty() because we are going to insert
      // aOverflowCont to mOverflowContList below, which guarantees an nonempty
      // list in ExcessOverflowContainersProperty().
      mOverflowContList = new (presContext->PresShell()) nsFrameList();
      mParent->SetProperty(nsContainerFrame::ExcessOverflowContainersProperty(),
                           mOverflowContList);
      SetUpListWalker();
    }
    if (aOverflowCont->GetParent() != mParent) {
      nsContainerFrame::ReparentFrameView(aOverflowCont,
                                          aOverflowCont->GetParent(), mParent);
      reparented = true;
    }

    // If aOverflowCont has a prev/next-in-flow that might be in
    // mOverflowContList we need to find it and insert after/before it to
    // maintain the order amongst next-in-flows in this list.
    nsIFrame* pif = aOverflowCont->GetPrevInFlow();
    nsIFrame* nif = aOverflowCont->GetNextInFlow();
    if ((pif && pif->GetParent() == mParent && pif != mPrevOverflowCont) ||
        (nif && nif->GetParent() == mParent && mPrevOverflowCont)) {
      for (nsFrameList::Enumerator e(*mOverflowContList); !e.AtEnd();
           e.Next()) {
        nsIFrame* f = e.get();
        if (f == pif) {
          mPrevOverflowCont = pif;
          break;
        }
        if (f == nif) {
          mPrevOverflowCont = f->GetPrevSibling();
          break;
        }
      }
    }

    mOverflowContList->InsertFrame(mParent, mPrevOverflowCont, aOverflowCont);
    aReflowStatus.SetNextInFlowNeedsReflow();
  }

  // If we need to reflow it, mark it dirty
  if (aReflowStatus.NextInFlowNeedsReflow()) {
    aOverflowCont->MarkSubtreeDirty();
  }

  // It's in our list, just step forward
  StepForward();
  NS_ASSERTION(mPrevOverflowCont == aOverflowCont ||
                   (mSkipOverflowContainerChildren &&
                    mPrevOverflowCont->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) !=
                        aOverflowCont->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)),
               "OverflowContTracker in unexpected state");

  if (addToList) {
    // Convert all non-overflow-container next-in-flows of aOverflowCont
    // into overflow containers and move them to our overflow
    // tracker. This preserves the invariant that the next-in-flows
    // of an overflow container are also overflow containers.
    nsIFrame* f = aOverflowCont->GetNextInFlow();
    if (f && (!f->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER) ||
              (!reparented && f->GetParent() == mParent) ||
              (reparented && f->GetParent() != mParent))) {
      if (!f->HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER)) {
        f->GetParent()->StealFrame(f);
      }
      Insert(f, aReflowStatus);
    }
  }
  return rv;
}

void nsOverflowContinuationTracker::BeginFinish(nsIFrame* aChild) {
  MOZ_ASSERT(aChild, "null ptr");
  MOZ_ASSERT(aChild->GetNextInFlow(),
             "supposed to call Finish *before* deleting next-in-flow!");

  for (nsIFrame* f = aChild; f; f = f->GetNextInFlow()) {
    // We'll update these in EndFinish after the next-in-flows are gone.
    if (f == mPrevOverflowCont) {
      mSentry = nullptr;
      mPrevOverflowCont = nullptr;
      break;
    }
    if (f == mSentry) {
      mSentry = nullptr;
      break;
    }
  }
}

void nsOverflowContinuationTracker::EndFinish(nsIFrame* aChild) {
  if (!mOverflowContList) {
    return;
  }
  // Forget mOverflowContList if it was deleted.
  nsFrameList* eoc = mParent->GetExcessOverflowContainers();
  if (eoc != mOverflowContList) {
    nsFrameList* oc = mParent->GetOverflowContainers();
    if (oc != mOverflowContList) {
      // mOverflowContList was deleted
      mPrevOverflowCont = nullptr;
      mSentry = nullptr;
      mParent = aChild->GetParent();
      mOverflowContList = nullptr;
      SetupOverflowContList();
      return;
    }
  }
  // The list survived, update mSentry if needed.
  if (!mSentry) {
    if (!mPrevOverflowCont) {
      SetUpListWalker();
    } else {
      mozilla::AutoRestore<nsIFrame*> saved(mPrevOverflowCont);
      // step backward to make StepForward() use our current mPrevOverflowCont
      mPrevOverflowCont = mPrevOverflowCont->GetPrevSibling();
      StepForward();
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

#ifdef DEBUG
void nsContainerFrame::SanityCheckChildListsBeforeReflow() const {
  MOZ_ASSERT(IsFlexOrGridContainer(),
             "Only Flex / Grid containers can call this!");

  const auto didPushItemsBit = IsFlexContainerFrame()
                                   ? NS_STATE_FLEX_DID_PUSH_ITEMS
                                   : NS_STATE_GRID_DID_PUSH_ITEMS;
  ChildListIDs absLists = {kAbsoluteList, kFixedList, kOverflowContainersList,
                           kExcessOverflowContainersList};
  ChildListIDs itemLists = {kPrincipalList, kOverflowList};
  for (const nsIFrame* f = this; f; f = f->GetNextInFlow()) {
    MOZ_ASSERT(!f->HasAnyStateBits(didPushItemsBit),
               "At start of reflow, we should've pulled items back from all "
               "NIFs and cleared the state bit stored in didPushItemsBit in "
               "the process.");
    for (const auto& [list, listID] : f->ChildLists()) {
      if (!itemLists.contains(listID)) {
        MOZ_ASSERT(absLists.contains(listID) || listID == kBackdropList,
                   "unexpected non-empty child list");
        continue;
      }
      for (const auto* child : list) {
        MOZ_ASSERT(f == this || child->GetPrevInFlow(),
                   "all pushed items must be pulled up before reflow");
      }
    }
  }
  // If we have a prev-in-flow, each of its children's next-in-flow
  // should be one of our children or be null.
  const auto* pif = static_cast<nsContainerFrame*>(GetPrevInFlow());
  if (pif) {
    const nsFrameList* oc = GetOverflowContainers();
    const nsFrameList* eoc = GetExcessOverflowContainers();
    const nsFrameList* pifEOC = pif->GetExcessOverflowContainers();
    for (const nsIFrame* child : pif->GetChildList(kPrincipalList)) {
      const nsIFrame* childNIF = child->GetNextInFlow();
      MOZ_ASSERT(!childNIF || mFrames.ContainsFrame(childNIF) ||
                 (pifEOC && pifEOC->ContainsFrame(childNIF)) ||
                 (oc && oc->ContainsFrame(childNIF)) ||
                 (eoc && eoc->ContainsFrame(childNIF)));
    }
  }
}

void nsContainerFrame::SetDidPushItemsBitIfNeeded(ChildListID aListID,
                                                  nsIFrame* aOldFrame) {
  MOZ_ASSERT(IsFlexOrGridContainer(),
             "Only Flex / Grid containers can call this!");

  // Note that kPrincipalList doesn't mean aOldFrame must be on that list.
  // It can also be on kOverflowList, in which case it might be a pushed
  // item, and if it's the only pushed item our DID_PUSH_ITEMS bit will lie.
  if (aListID == kPrincipalList && !aOldFrame->GetPrevInFlow()) {
    // Since the bit may lie, set the mDidPushItemsBitMayLie value to true for
    // ourself and for all our prev-in-flows.
    nsContainerFrame* frameThatMayLie = this;
    do {
      frameThatMayLie->mDidPushItemsBitMayLie = true;
      frameThatMayLie =
          static_cast<nsContainerFrame*>(frameThatMayLie->GetPrevInFlow());
    } while (frameThatMayLie);
  }
}
#endif

#ifdef DEBUG_FRAME_DUMP
void nsContainerFrame::List(FILE* out, const char* aPrefix,
                            ListFlags aFlags) const {
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);
  ExtraContainerFrameInfo(str);

  // Output the frame name and various fields.
  fprintf_stderr(out, "%s <\n", str.get());

  const nsCString pfx = nsCString(aPrefix) + "  "_ns;

  // Output principal child list separately since we want to omit its
  // name and address.
  for (nsIFrame* kid : PrincipalChildList()) {
    kid->List(out, pfx.get(), aFlags);
  }

  // Output rest of the child lists.
  const ChildListIDs skippedListIDs = {kPrincipalList};
  ListChildLists(out, pfx.get(), aFlags, skippedListIDs);

  fprintf_stderr(out, "%s>\n", aPrefix);
}

void nsContainerFrame::ListWithMatchedRules(FILE* out,
                                            const char* aPrefix) const {
  fprintf_stderr(out, "%s%s\n", aPrefix, ListTag().get());

  nsCString rulePrefix;
  rulePrefix += aPrefix;
  rulePrefix += "    ";
  ListMatchedRules(out, rulePrefix.get());

  nsCString childPrefix;
  childPrefix += aPrefix;
  childPrefix += "  ";

  for (const auto& childList : ChildLists()) {
    for (const nsIFrame* kid : childList.mList) {
      kid->ListWithMatchedRules(out, childPrefix.get());
    }
  }
}

void nsContainerFrame::ListChildLists(FILE* aOut, const char* aPrefix,
                                      ListFlags aFlags,
                                      ChildListIDs aSkippedListIDs) const {
  const nsCString nestedPfx = nsCString(aPrefix) + "  "_ns;

  for (const auto& [list, listID] : ChildLists()) {
    if (aSkippedListIDs.contains(listID)) {
      continue;
    }

    // Use nsPrintfCString so that %p don't output prefix "0x". This is
    // consistent with nsIFrame::ListTag().
    const nsPrintfCString str("%s%s@%p <\n", aPrefix, ChildListName(listID),
                              &GetChildList(listID));
    fprintf_stderr(aOut, "%s", str.get());

    for (nsIFrame* kid : list) {
      // Verify the child frame's parent frame pointer is correct.
      NS_ASSERTION(kid->GetParent() == this, "Bad parent frame pointer!");
      kid->List(aOut, nestedPfx.get(), aFlags);
    }
    fprintf_stderr(aOut, "%s>\n", aPrefix);
  }
}

void nsContainerFrame::ExtraContainerFrameInfo(nsACString& aTo) const {
  (void)aTo;
}

#endif
