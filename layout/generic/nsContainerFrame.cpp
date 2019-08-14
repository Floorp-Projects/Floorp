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
#include "nsAbsoluteContainingBlock.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsFlexContainerFrame.h"
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
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

nsContainerFrame::~nsContainerFrame() {}

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
    if (aPrevInFlow->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)
      AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }
}

void nsContainerFrame::SetInitialChildList(ChildListID aListID,
                                           nsFrameList& aChildList) {
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
  for (nsIFrame* f : aChildList) {
    MOZ_ASSERT(f->GetParent() == this, "Unexpected parent");
  }
#endif
  if (aListID == kPrincipalList) {
    MOZ_ASSERT(mFrames.IsEmpty(),
               "unexpected second call to SetInitialChildList");
    mFrames.SetFrames(aChildList);
  } else if (aListID == kBackdropList) {
    MOZ_ASSERT(StyleDisplay()->mTopLayer != NS_STYLE_TOP_LAYER_NONE,
               "Only top layer frames should have backdrop");
    MOZ_ASSERT(GetStateBits() & NS_FRAME_OUT_OF_FLOW,
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
      RemoveProperty(aProp);
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
    nsIFrame* prevSib = GetProperty(nsIFrame::IBSplitPrevSibling());
    if (prevSib) {
      NS_WARNING_ASSERTION(
          this == prevSib->GetProperty(nsIFrame::IBSplitSibling()),
          "IB sibling chain is inconsistent");
      prevSib->DeleteProperty(nsIFrame::IBSplitSibling());
    }

    // Delete next sibling's reference to me.
    nsIFrame* nextSib = GetProperty(nsIFrame::IBSplitSibling());
    if (nextSib) {
      NS_WARNING_ASSERTION(
          this == nextSib->GetProperty(nsIFrame::IBSplitPrevSibling()),
          "IB sibling chain is inconsistent");
      nextSib->DeleteProperty(nsIFrame::IBSplitPrevSibling());
    }

#ifdef DEBUG
    // This is just so we can assert it's not set in nsFrame::DestroyFrom.
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
                   StyleDisplay()->mTopLayer != NS_STYLE_TOP_LAYER_NONE,
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
      nsFrameList* list = GetPropTableFrames(OverflowContainersProperty());
      return list ? *list : nsFrameList::EmptyList();
    }
    case kExcessOverflowContainersList: {
      nsFrameList* list =
          GetPropTableFrames(ExcessOverflowContainersProperty());
      return list ? *list : nsFrameList::EmptyList();
    }
    case kBackdropList: {
      nsFrameList* list = GetPropTableFrames(BackdropProperty());
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
    uint32_t aFlags) {
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background directly onto the content list
  nsDisplayListSet set(aLists, aLists.Content());
  // The children should be in content order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, set, aFlags);
    kid = kid->GetNextSibling();
  }
}

/* virtual */
void nsContainerFrame::ChildIsDirty(nsIFrame* aChild) {
  NS_ASSERTION(NS_SUBTREE_DIRTY(aChild), "child isn't actually dirty");

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

nsresult nsContainerFrame::ReparentFrameViewList(
    const nsFrameList& aChildFrameList, nsIFrame* aOldParentFrame,
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
    nsViewManager* viewManager = oldParentView->GetViewManager();

    // They're not so we need to reparent any child views
    for (nsFrameList::Enumerator e(aChildFrameList); !e.AtEnd(); e.Next()) {
      e.get()->ReparentFrameViewTo(viewManager, newParentView, oldParentView);
    }
  }

  return NS_OK;
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
  if (!rootElement || !rootElement->IsXULElement()) {
    // Scrollframes use native widgets which don't work well with
    // translucent windows, at least in Windows XP. So if the document
    // has a root scrollrame it's useless to try to make it transparent,
    // we'll just get something broken.
    // nsCSSFrameConstructor::ConstructRootFrame constructs root
    // scrollframes whenever the root element is not a XUL element, so
    // we test for that here. We can't just call
    // presShell->GetRootScrollFrame() since that might not have
    // been constructed yet.
    // We can change this to allow translucent toplevel HTML documents
    // (e.g. to do something like Dashboard widgets), once we
    // have broad support for translucent scrolled documents, but be
    // careful because apparently some Firefox extensions expect
    // openDialog("something.html") to produce an opaque window
    // even if the HTML doesn't have a background-color set.
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

  nsTransparencyMode mode =
      nsLayoutUtils::GetFrameTransparency(aFrame, rootFrame);
  int32_t shadow = rootFrame->StyleUIReset()->mWindowShadow;
  nsCOMPtr<nsIWidget> viewWidget = aView->GetWidget();
  viewWidget->SetTransparencyMode(mode);
  windowWidget->SetWindowShadowStyle(shadow);

  if (!aRC) return;

  if (!weak.IsAlive()) {
    return;
  }

  nsBoxLayoutState aState(aPresContext, aRC);
  nsSize minSize = rootFrame->GetXULMinSize(aState);
  nsSize maxSize = rootFrame->GetXULMaxSize(aState);

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

void nsContainerFrame::SyncFrameViewAfterReflow(
    nsPresContext* aPresContext, nsIFrame* aFrame, nsView* aView,
    const nsRect& aVisualOverflowArea, ReflowChildFlags aFlags) {
  if (!aView) {
    return;
  }

  // Make sure the view is sized and positioned correctly
  if (!(aFlags & ReflowChildFlags::NoMoveView)) {
    PositionFrameView(aFrame);
  }

  if (!(aFlags & ReflowChildFlags::NoSizeView)) {
    nsViewManager* vm = aView->GetViewManager();

    vm->ResizeView(aView, aVisualOverflowArea, true);
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

void nsContainerFrame::DoInlineIntrinsicISize(
    gfxContext* aRenderingContext, InlineIntrinsicISizeData* aData,
    nsLayoutUtils::IntrinsicISizeType aType) {
  if (GetPrevInFlow()) return;  // Already added.

  MOZ_ASSERT(
      aType == nsLayoutUtils::MIN_ISIZE || aType == nsLayoutUtils::PREF_ISIZE,
      "bad type");

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
      if (aType == nsLayoutUtils::MIN_ISIZE)
        kid->AddInlineMinISize(aRenderingContext,
                               static_cast<InlineMinISizeData*>(aData));
      else
        kid->AddInlinePrefISize(aRenderingContext,
                                static_cast<InlinePrefISizeData*>(aData));
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
    const LogicalSize& aBorder, const LogicalSize& aPadding,
    ComputeSizeFlags aFlags) {
  LogicalSize result(aWM, 0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  nscoord availBased = aAvailableISize - aMargin.ISize(aWM) -
                       aBorder.ISize(aWM) - aPadding.ISize(aWM);
  // replaced elements always shrink-wrap
  if ((aFlags & ComputeSizeFlags::eShrinkWrap) || IsFrameOfType(eReplaced)) {
    // Only bother computing our 'auto' ISize if the result will be used.
    // It'll be used under two scenarios:
    // - If our ISize property is itself 'auto'.
    // - If we're using flex-basis in place of our ISize property (i.e. we're a
    // flex item with our inline axis being the main axis), AND we have
    // flex-basis:content.
    const nsStylePosition* pos = StylePosition();
    if (pos->ISize(aWM).IsAuto() ||
        (pos->mFlexBasis.IsContent() && IsFlexItem() &&
         nsFlexContainerFrame::IsItemInlineAxisMainAxis(this))) {
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
    uint8_t captionSide = StyleTableBorder()->mCaptionSide;

    if (aWM.IsOrthogonalTo(tableWM)) {
      if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
          captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
          captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
          captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE) {
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
      if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
          captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
        result.ISize(aWM) = GetMinISize(aRenderingContext);
      } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
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
  if (aWM.IsVerticalRL() || (!aWM.IsVertical() && !aWM.IsBidiLTR())) {
    NS_ASSERTION(aContainerSize.width != NS_UNCONSTRAINEDSIZE,
                 "ReflowChild with unconstrained container width!");
  }
  MOZ_ASSERT(aDesiredSize.VisualOverflow() == nsRect(0, 0, 0, 0) &&
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
  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  // Recursively walk aFrame's child frames.
  // Process the additional child lists, but skip the popup list as the
  // view for popups is managed by the parent. Currently only nsMenuFrame
  // and nsPopupSetFrame have a popupList and during layout will adjust the
  // view manually to position the popup.
  ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    if (lists.CurrentID() == kPopupList) {
      continue;
    }
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      // Position the frame's view (if it has one) otherwise recursively
      // process its children
      nsIFrame* childFrame = childFrames.get();
      if (childFrame->HasView()) {
        PositionFrameView(childFrame);
      } else {
        PositionChildViews(childFrame);
      }
    }
  }
}

/**
 * The second half of frame reflow. Does the following:
 * - sets the frame's bounds
 * - sizes and positions (if requested) the frame's view. If the frame's final
 *   position differs from the current position and the frame itself does not
 *   have a view, then any child frames with views are positioned so they stay
 *   in sync
 * - sets the view's visibility, opacity, content transparency, and clip
 * - invoked the DidReflow() function
 *
 * Flags:
 * ReflowChildFlags::NoMoveFrame - don't move the frame. aX and aY are ignored
 *    in this case. Also implies ReflowChildFlags::NoMoveView
 * ReflowChildFlags::NoMoveView - don't position the frame's view. Set this if
 *    you don't want to automatically sync the frame and view
 * ReflowChildFlags::NoSizeView - don't size the frame's view
 */

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

  if (aWM.IsVerticalRL() || (!aWM.IsVertical() && !aWM.IsBidiLTR())) {
    NS_ASSERTION(aContainerSize.width != NS_UNCONSTRAINEDSIZE,
                 "FinishReflowChild with unconstrained container width!");
  }

  nsPoint curOrigin = aKidFrame->GetPosition();
  WritingMode outerWM = aDesiredSize.GetWritingMode();
  LogicalSize convertedSize =
      aDesiredSize.Size(outerWM).ConvertTo(aWM, outerWM);
  LogicalPoint pos(aPos);

  if (aFlags & ReflowChildFlags::ApplyRelativePositioning) {
    MOZ_ASSERT(aReflowInput, "caller must have passed reflow input");
    // ApplyRelativePositioning in right-to-left writing modes needs to know
    // the updated frame width to set the normal position correctly.
    aKidFrame->SetSize(aWM, convertedSize);
    aReflowInput->ApplyRelativePositioning(&pos, aContainerSize);
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
                             aDesiredSize.VisualOverflow(), aFlags);
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
                             aDesiredSize.VisualOverflow(), aFlags);
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
    nsOverflowAreas& aOverflowRects, ReflowChildFlags aFlags,
    nsReflowStatus& aStatus, ChildFrameMerger aMergeFunc) {
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
    // If the available vertical height has changed, we need to reflow
    // even if the frame isn't dirty.
    if (shouldReflowAllKids || NS_SUBTREE_DIRTY(frame)) {
      // Get prev-in-flow
      nsIFrame* prevInFlow = frame->GetPrevInFlow();
      NS_ASSERTION(prevInFlow,
                   "overflow container frame must have a prev-in-flow");
      NS_ASSERTION(
          frame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER,
          "overflow container frame must have overflow container bit set");
      WritingMode wm = frame->GetWritingMode();
      nsSize containerSize = aReflowInput.AvailableSize(wm).GetPhysicalSize(wm);
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
        if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
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
          nif = aPresContext->PresShell()
                    ->FrameConstructor()
                    ->CreateContinuingFrame(aPresContext, frame, this);
        } else if (!(nif->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          // used to be a normal next-in-flow; steal it from the child list
          nsresult rv = nif->GetParent()->StealFrame(nif);
          if (NS_FAILED(rv)) {
            return;
          }
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
  nsFrameList* overflowconts = GetPropTableFrames(OverflowContainersProperty());
  if (overflowconts) {
    for (nsIFrame* frame : *overflowconts) {
      BuildDisplayListForChild(aBuilder, frame, aLists);
    }
  }
}

static bool TryRemoveFrame(nsIFrame* aFrame,
                           nsContainerFrame::FrameListPropertyDescriptor aProp,
                           nsIFrame* aChildToRemove) {
  nsFrameList* list = aFrame->GetProperty(aProp);
  if (list && list->StartRemoveFrame(aChildToRemove)) {
    // aChildToRemove *may* have been removed from this list.
    if (list->IsEmpty()) {
      aFrame->RemoveProperty(aProp);
      list->Delete(aFrame->PresShell());
    }
    return true;
  }
  return false;
}

bool nsContainerFrame::MaybeStealOverflowContainerFrame(nsIFrame* aChild) {
  bool removed = false;
  if (MOZ_UNLIKELY(aChild->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
    // Try removing from the overflow container list.
    removed = ::TryRemoveFrame(this, OverflowContainersProperty(), aChild);
    if (!removed) {
      // It might be in the excess overflow container list.
      removed =
          ::TryRemoveFrame(this, ExcessOverflowContainersProperty(), aChild);
    }
  }
  return removed;
}

nsresult nsContainerFrame::StealFrame(nsIFrame* aChild) {
#ifdef DEBUG
  if (!mFrames.ContainsFrame(aChild)) {
    nsFrameList* list = GetOverflowFrames();
    if (!list || !list->ContainsFrame(aChild)) {
      list = GetProperty(OverflowContainersProperty());
      if (!list || !list->ContainsFrame(aChild)) {
        list = GetProperty(ExcessOverflowContainersProperty());
        MOZ_ASSERT(list && list->ContainsFrame(aChild),
                   "aChild isn't our child"
                   " or on a frame list not supported by StealFrame");
      }
    }
  }
#endif

  bool removed = MaybeStealOverflowContainerFrame(aChild);
  if (!removed) {
    // NOTE nsColumnSetFrame and nsCanvasFrame have their overflow containers
    // on the normal lists so we might get here also if the frame bit
    // NS_FRAME_IS_OVERFLOW_CONTAINER is set.
    removed = mFrames.StartRemoveFrame(aChild);
    if (!removed) {
      // We didn't find the child in our principal child list.
      // Maybe it's on the overflow list?
      nsFrameList* frameList = GetOverflowFrames();
      if (frameList) {
        removed = frameList->ContinueRemoveFrame(aChild);
        if (frameList->IsEmpty()) {
          DestroyOverflowList();
        }
      }
    }
  }

  MOZ_ASSERT(removed, "StealFrame: can't find aChild");
  return removed ? NS_OK : NS_ERROR_UNEXPECTED;
}

nsFrameList nsContainerFrame::StealFramesAfter(nsIFrame* aChild) {
  NS_ASSERTION(
      !aChild || !(aChild->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER),
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

  nsPresContext* pc = PresContext();
  nsIFrame* nextInFlow = aFrame->GetNextInFlow();
  if (nullptr == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our child list.
    nextInFlow = pc->PresShell()->FrameConstructor()->CreateContinuingFrame(
        pc, aFrame, this);
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
  DebugOnly<nsresult> rv = StealFrame(aNextInFlow);
  NS_ASSERTION(NS_SUCCEEDED(rv), "StealFrame failure");

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

/**
 * Set the frames on the overflow list
 */
void nsContainerFrame::SetOverflowFrames(const nsFrameList& aOverflowFrames) {
  MOZ_ASSERT(aOverflowFrames.NotEmpty(), "Shouldn't be called");

  nsPresContext* pc = PresContext();
  nsFrameList* newList = new (pc->PresShell()) nsFrameList(aOverflowFrames);

  SetProperty(OverflowProperty(), newList);
}

nsFrameList* nsContainerFrame::GetPropTableFrames(
    FrameListPropertyDescriptor aProperty) const {
  return GetProperty(aProperty);
}

nsFrameList* nsContainerFrame::RemovePropTableFrames(
    FrameListPropertyDescriptor aProperty) {
  return RemoveProperty(aProperty);
}

void nsContainerFrame::SetPropTableFrames(
    nsFrameList* aFrameList, FrameListPropertyDescriptor aProperty) {
  MOZ_ASSERT(aProperty && aFrameList, "null ptr");
  MOZ_ASSERT(
      (aProperty != nsContainerFrame::OverflowContainersProperty() &&
       aProperty != nsContainerFrame::ExcessOverflowContainersProperty()) ||
          IsFrameOfType(nsIFrame::eCanContainOverflowContainers),
      "this type of frame can't have overflow containers");
  MOZ_ASSERT(!GetPropTableFrames(aProperty));
  SetProperty(aProperty, aFrameList);
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
    SetOverflowFrames(tail);
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

nsFrameList* nsContainerFrame::DrainExcessOverflowContainersList(
    ChildFrameMerger aMergeFunc) {
  nsFrameList* overflowContainers =
      GetPropTableFrames(OverflowContainersProperty());

  NS_ASSERTION(!(overflowContainers && GetPrevInFlow() &&
                 static_cast<nsContainerFrame*>(GetPrevInFlow())
                     ->GetPropTableFrames(ExcessOverflowContainersProperty())),
               "conflicting overflow containers lists");

  if (!overflowContainers) {
    // Drain excess from previnflow
    nsContainerFrame* prev = (nsContainerFrame*)GetPrevInFlow();
    if (prev) {
      nsFrameList* excessFrames =
          prev->RemovePropTableFrames(ExcessOverflowContainersProperty());
      if (excessFrames) {
        excessFrames->ApplySetParent(this);
        nsContainerFrame::ReparentFrameViewList(*excessFrames, prev, this);
        overflowContainers = excessFrames;
        SetPropTableFrames(overflowContainers, OverflowContainersProperty());
      }
    }
  }

  // Our own excess overflow containers from a previous reflow can still be
  // present if our next-in-flow hasn't been reflown yet.  Move any children
  // from it that don't have a continuation in this frame to the
  // OverflowContainers list.
  nsFrameList* selfExcessOCFrames =
      RemovePropTableFrames(ExcessOverflowContainersProperty());
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
    if (toMove.IsEmpty()) {
      SetPropTableFrames(selfExcessOCFrames,
                         ExcessOverflowContainersProperty());
    } else if (overflowContainers) {
      aMergeFunc(*overflowContainers, toMove, this);
      if (selfExcessOCFrames->IsEmpty()) {
        selfExcessOCFrames->Delete(PresShell());
      } else {
        SetPropTableFrames(selfExcessOCFrames,
                           ExcessOverflowContainersProperty());
      }
    } else {
      if (selfExcessOCFrames->IsEmpty()) {
        *selfExcessOCFrames = toMove;
        overflowContainers = selfExcessOCFrames;
      } else {
        SetPropTableFrames(selfExcessOCFrames,
                           ExcessOverflowContainersProperty());
        auto shell = PresShell();
        overflowContainers = new (shell) nsFrameList(toMove);
      }
      SetPropTableFrames(overflowContainers, OverflowContainersProperty());
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

uint16_t nsContainerFrame::CSSAlignmentForAbsPosChild(
    const ReflowInput& aChildRI, LogicalAxis aLogicalAxis) const {
  MOZ_ASSERT(aChildRI.mFrame->IsAbsolutelyPositioned(),
             "This method should only be called for abspos children");
  NS_ERROR(
      "Child classes that use css box alignment for abspos children "
      "should provide their own implementation of this method!");

  // In the unexpected/unlikely event that this implementation gets invoked,
  // just use "start" alignment.
  return NS_STYLE_ALIGN_START;
}

#ifdef ACCESSIBILITY
void nsContainerFrame::GetSpokenMarkerText(nsAString& aText) const {
  const nsStyleList* myList = StyleList();
  if (myList->GetListStyleImage()) {
    char16_t kDiscCharacter = 0x2022;
    aText.Assign(kDiscCharacter);
    aText.Append(' ');
  } else {
    nsIContent* markerPseudo = nsLayoutUtils::GetMarkerPseudo(GetContent());
    if (nsIFrame* marker = markerPseudo->GetPrimaryFrame()) {
      if (nsBulletFrame* bullet = do_QueryFrame(marker)) {
        bullet->GetSpokenText(aText);
      } else {
        ErrorResult err;
        markerPseudo->GetTextContent(aText, err);
        if (err.Failed()) {
          aText.Truncate();
        }
      }
    } else {
      aText.Truncate();
    }
  }
}
#endif

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
    mOverflowContList =
        nif->GetPropTableFrames(nsContainerFrame::OverflowContainersProperty());
    if (mOverflowContList) {
      mParent = nif;
      SetUpListWalker();
    }
  }
  if (!mOverflowContList) {
    mOverflowContList = mParent->GetPropTableFrames(
        nsContainerFrame::ExcessOverflowContainersProperty());
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
      while (cur && (cur->GetPrevInFlow()->GetStateBits() &
                     NS_FRAME_IS_OVERFLOW_CONTAINER)) {
        mPrevOverflowCont = cur;
        cur = cur->GetNextSibling();
      }
      while (cur && (!(cur->GetStateBits() & NS_FRAME_OUT_OF_FLOW) ==
                     mWalkOOFFrames)) {
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
           (!(cur->GetStateBits() & NS_FRAME_OUT_OF_FLOW) == mWalkOOFFrames)) {
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
                     !!(aOverflowCont->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
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
      (aOverflowCont->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) &&
      mOverflowContList && mOverflowContList->ContainsFrame(aOverflowCont)) {
    addToList = false;
    mPrevOverflowCont = aOverflowCont->GetPrevSibling();
  }

  if (addToList) {
    if (aOverflowCont->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
      // aOverflowCont is in some other overflow container list,
      // steal it first
      NS_ASSERTION(!(mOverflowContList &&
                     mOverflowContList->ContainsFrame(aOverflowCont)),
                   "overflow containers out of order");
      rv = aOverflowCont->GetParent()->StealFrame(aOverflowCont);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      aOverflowCont->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
    }
    if (!mOverflowContList) {
      mOverflowContList = new (presContext->PresShell()) nsFrameList();
      mParent->SetPropTableFrames(
          mOverflowContList,
          nsContainerFrame::ExcessOverflowContainersProperty());
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
  NS_ASSERTION(
      mPrevOverflowCont == aOverflowCont ||
          (mSkipOverflowContainerChildren &&
           (mPrevOverflowCont->GetStateBits() & NS_FRAME_OUT_OF_FLOW) !=
               (aOverflowCont->GetStateBits() & NS_FRAME_OUT_OF_FLOW)),
      "OverflowContTracker in unexpected state");

  if (addToList) {
    // Convert all non-overflow-container next-in-flows of aOverflowCont
    // into overflow containers and move them to our overflow
    // tracker. This preserves the invariant that the next-in-flows
    // of an overflow container are also overflow containers.
    nsIFrame* f = aOverflowCont->GetNextInFlow();
    if (f && (!(f->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) ||
              (!reparented && f->GetParent() == mParent) ||
              (reparented && f->GetParent() != mParent))) {
      if (!(f->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
        rv = f->GetParent()->StealFrame(f);
        NS_ENSURE_SUCCESS(rv, rv);
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
  nsFrameList* eoc = mParent->GetProperty(
      nsContainerFrame::ExcessOverflowContainersProperty());
  if (eoc != mOverflowContList) {
    nsFrameList* oc = static_cast<nsFrameList*>(
        mParent->GetProperty(nsContainerFrame::OverflowContainersProperty()));
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

#ifdef DEBUG_FRAME_DUMP
void nsContainerFrame::List(FILE* out, const char* aPrefix,
                            uint32_t aFlags) const {
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  // Output the children
  bool outputOneList = false;
  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    if (outputOneList) {
      str += aPrefix;
    }
    if (lists.CurrentID() != kPrincipalList) {
      if (!outputOneList) {
        str += "\n";
        str += aPrefix;
      }
      str += nsPrintfCString("%s %p ",
                             mozilla::layout::ChildListName(lists.CurrentID()),
                             &GetChildList(lists.CurrentID()));
    }
    fprintf_stderr(out, "%s<\n", str.get());
    str = "";
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* kid = childFrames.get();
      // Verify the child frame's parent frame pointer is correct
      NS_ASSERTION(kid->GetParent() == this, "bad parent frame pointer");

      // Have the child frame list
      nsCString pfx(aPrefix);
      pfx += "  ";
      kid->List(out, pfx.get(), aFlags);
    }
    fprintf_stderr(out, "%s>\n", aPrefix);
    outputOneList = true;
  }

  if (!outputOneList) {
    fprintf_stderr(out, "%s<>\n", str.get());
  }
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

  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* kid = childFrames.get();
      kid->ListWithMatchedRules(out, childPrefix.get());
    }
  }
}
#endif
