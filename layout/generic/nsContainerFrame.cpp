/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class #1 for rendering objects that have child lists */

#include "nsContainerFrame.h"

#include "nsAbsoluteContainingBlock.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsStyleConsts.h"
#include "nsView.h"
#include "nsIPresShell.h"
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
#include "mozilla/AutoRestore.h"
#include "nsIFrameInlines.h"
#include "nsPrintfCString.h"
#include <algorithm>

#ifdef DEBUG
#undef NOISY
#else
#undef NOISY
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

NS_IMPL_FRAMEARENA_HELPERS(nsContainerFrame)

nsContainerFrame::~nsContainerFrame()
{
}

NS_QUERYFRAME_HEAD(nsContainerFrame)
  NS_QUERYFRAME_ENTRY(nsContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSplittableFrame)

void
nsContainerFrame::Init(nsIContent*       aContent,
                       nsContainerFrame* aParent,
                       nsIFrame*         aPrevInFlow)
{
  nsSplittableFrame::Init(aContent, aParent, aPrevInFlow);
  if (aPrevInFlow) {
    // Make sure we copy bits from our prev-in-flow that will affect
    // us. A continuation for a container frame needs to know if it
    // has a child with a view so that we'll properly reposition it.
    if (aPrevInFlow->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)
      AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }
}

void
nsContainerFrame::SetInitialChildList(ChildListID  aListID,
                                      nsFrameList& aChildList)
{
  MOZ_ASSERT(mFrames.IsEmpty(),
             "unexpected second call to SetInitialChildList");
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  mFrames.SetFrames(aChildList);
}

void
nsContainerFrame::AppendFrames(ChildListID  aListID,
                               nsFrameList& aFrameList)
{
  MOZ_ASSERT(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
             "unexpected child list");

  if (MOZ_UNLIKELY(aFrameList.IsEmpty())) {
    return;
  }

  DrainSelfOverflowList(); // ensure the last frame is in mFrames
  mFrames.AppendFrames(this, aFrameList);

  if (aListID != kNoReflowPrincipalList) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
  }
}

void
nsContainerFrame::InsertFrames(ChildListID aListID,
                               nsIFrame* aPrevFrame,
                               nsFrameList& aFrameList)
{
  MOZ_ASSERT(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
             "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if (MOZ_UNLIKELY(aFrameList.IsEmpty())) {
    return;
  }

  DrainSelfOverflowList(); // ensure aPrevFrame is in mFrames
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  if (aListID != kNoReflowPrincipalList) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
  }
}

void
nsContainerFrame::RemoveFrame(ChildListID aListID,
                              nsIFrame* aOldFrame)
{
  MOZ_ASSERT(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
             "unexpected child list");

  // Loop and destroy aOldFrame and all of its continuations.
  // Request a reflow on the parent frames involved unless we were explicitly
  // told not to (kNoReflowPrincipalList).
  bool generateReflowCommand = true;
  if (kNoReflowPrincipalList == aListID) {
    generateReflowCommand = false;
  }
  nsIPresShell* shell = PresContext()->PresShell();
  nsContainerFrame* lastParent = nullptr;
  while (aOldFrame) {
    nsIFrame* oldFrameNextContinuation = aOldFrame->GetNextContinuation();
    nsContainerFrame* parent = aOldFrame->GetParent();
    // Please note that 'parent' may not actually be where 'aOldFrame' lives.
    // We really MUST use StealFrame() and nothing else here.
    // @see nsInlineFrame::StealFrame for details.
    parent->StealFrame(aOldFrame, true);
    aOldFrame->Destroy();
    aOldFrame = oldFrameNextContinuation;
    if (parent != lastParent && generateReflowCommand) {
      shell->FrameNeedsReflow(parent, nsIPresShell::eTreeChange,
                              NS_FRAME_HAS_DIRTY_CHILDREN);
      lastParent = parent;
    }
  }
}

void
nsContainerFrame::DestroyAbsoluteFrames(nsIFrame* aDestructRoot)
{
  if (IsAbsoluteContainer()) {
    GetAbsoluteContainingBlock()->DestroyFrames(this, aDestructRoot);
    MarkAsNotAbsoluteContainingBlock();
  }
}

void
nsContainerFrame::SafelyDestroyFrameListProp(nsIFrame* aDestructRoot,
                                             nsIPresShell* aPresShell,
                                             FramePropertyTable* aPropTable,
                                             const FramePropertyDescriptor* aProp)
{
  // Note that the last frame can be removed through another route and thus
  // delete the property -- that's why we fetch the property again before
  // removing each frame rather than fetching it once and iterating the list.
  while (nsFrameList* frameList =
           static_cast<nsFrameList*>(aPropTable->Get(this, aProp))) {
    nsIFrame* frame = frameList->RemoveFirstChild();
    if (MOZ_LIKELY(frame)) {
      frame->DestroyFrom(aDestructRoot);
    } else {
      aPropTable->Remove(this, aProp);
      frameList->Delete(aPresShell);
      return;
    }
  }
}

void
nsContainerFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Prevent event dispatch during destruction.
  if (HasView()) {
    GetView()->SetFrame(nullptr);
  }

  DestroyAbsoluteFrames(aDestructRoot);

  // Destroy frames on the principal child list.
  mFrames.DestroyFramesFrom(aDestructRoot);

  // Destroy frames on the auxiliary frame lists and delete the lists.
  nsPresContext* pc = PresContext();
  nsIPresShell* shell = pc->PresShell();
  FramePropertyTable* props = pc->PropertyTable();
  SafelyDestroyFrameListProp(aDestructRoot, shell, props, OverflowProperty());

  MOZ_ASSERT(IsFrameOfType(nsIFrame::eCanContainOverflowContainers) ||
             !(props->Get(this, nsContainerFrame::OverflowContainersProperty()) ||
               props->Get(this, nsContainerFrame::ExcessOverflowContainersProperty())),
             "this type of frame should't have overflow containers");

  SafelyDestroyFrameListProp(aDestructRoot, shell, props,
                             OverflowContainersProperty());
  SafelyDestroyFrameListProp(aDestructRoot, shell, props,
                             ExcessOverflowContainersProperty());

  nsSplittableFrame::DestroyFrom(aDestructRoot);
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

const nsFrameList&
nsContainerFrame::GetChildList(ChildListID aListID) const
{
  // We only know about the principal child list and the overflow lists.
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
    default:
      return nsSplittableFrame::GetChildList(aListID);
  }
}

static void AppendIfNonempty(const nsIFrame* aFrame,
                            FramePropertyTable* aPropTable,
                            const FramePropertyDescriptor* aProperty,
                            nsTArray<nsIFrame::ChildList>* aLists,
                            nsIFrame::ChildListID aListID)
{
  nsFrameList* list = static_cast<nsFrameList*>(
    aPropTable->Get(aFrame, aProperty));
  if (list) {
    list->AppendIfNonempty(aLists, aListID);
  }
}

void
nsContainerFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  mFrames.AppendIfNonempty(aLists, kPrincipalList);
  FramePropertyTable* propTable = PresContext()->PropertyTable();
  ::AppendIfNonempty(this, propTable, OverflowProperty(),
                     aLists, kOverflowList);
  if (IsFrameOfType(nsIFrame::eCanContainOverflowContainers)) {
    ::AppendIfNonempty(this, propTable, OverflowContainersProperty(),
                       aLists, kOverflowContainersList);
    ::AppendIfNonempty(this, propTable, ExcessOverflowContainersProperty(),
                       aLists, kExcessOverflowContainersList);
  }
  nsSplittableFrame::GetChildLists(aLists);
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

void
nsContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists);
}

void
nsContainerFrame::BuildDisplayListForNonBlockChildren(nsDisplayListBuilder*   aBuilder,
                                                      const nsRect&           aDirtyRect,
                                                      const nsDisplayListSet& aLists,
                                                      uint32_t                aFlags)
{
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background directly onto the content list
  nsDisplayListSet set(aLists, aLists.Content());
  // The children should be in content order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set, aFlags);
    kid = kid->GetNextSibling();
  }
}

/* virtual */ void
nsContainerFrame::ChildIsDirty(nsIFrame* aChild)
{
  NS_ASSERTION(NS_SUBTREE_DIRTY(aChild), "child isn't actually dirty");

  AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
}

bool
nsContainerFrame::IsLeaf() const
{
  return false;
}

nsIFrame::FrameSearchResult
nsContainerFrame::PeekOffsetNoAmount(bool aForward, int32_t* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return CONTINUE_EMPTY;
}

nsIFrame::FrameSearchResult
nsContainerFrame::PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                      bool aRespectClusters)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return CONTINUE_EMPTY;
}

/////////////////////////////////////////////////////////////////////////////
// Helper member functions

static nsresult
ReparentFrameViewTo(nsIFrame*       aFrame,
                    nsViewManager* aViewManager,
                    nsView*        aNewParentView,
                    nsView*        aOldParentView)
{

  // XXX What to do about placeholder views for "position: fixed" elements?
  // They should be reparented too.

  // Does aFrame have a view?
  if (aFrame->HasView()) {
#ifdef MOZ_XUL
    if (aFrame->GetType() == nsGkAtoms::menuPopupFrame) {
      // This view must be parented by the root view, don't reparent it.
      return NS_OK;
    }
#endif
    nsView* view = aFrame->GetView();
    // Verify that the current parent view is what we think it is
    //nsView*  parentView;
    //NS_ASSERTION(parentView == aOldParentView, "unexpected parent view");

    aViewManager->RemoveChild(view);
    
    // The view will remember the Z-order and other attributes that have been set on it.
    nsView* insertBefore = nsLayoutUtils::FindSiblingViewFor(aNewParentView, aFrame);
    aViewManager->InsertChild(aNewParentView, view, insertBefore, insertBefore != nullptr);
  } else {
    nsIFrame::ChildListIterator lists(aFrame);
    for (; !lists.IsDone(); lists.Next()) {
      // Iterate the child frames, and check each child frame to see if it has
      // a view
      nsFrameList::Enumerator childFrames(lists.CurrentList());
      for (; !childFrames.AtEnd(); childFrames.Next()) {
        ReparentFrameViewTo(childFrames.get(), aViewManager,
                            aNewParentView, aOldParentView);
      }
    }
  }

  return NS_OK;
}

void
nsContainerFrame::CreateViewForFrame(nsIFrame* aFrame,
                                     bool aForce)
{
  if (aFrame->HasView()) {
    return;
  }

  // If we don't yet have a view, see if we need a view
  if (!aForce && !aFrame->NeedsView()) {
    // don't need a view
    return;
  }

  nsView* parentView = aFrame->GetParent()->GetClosestView();
  NS_ASSERTION(parentView, "no parent with view");

  nsViewManager* viewManager = parentView->GetViewManager();
  NS_ASSERTION(viewManager, "null view manager");

  // Create a view
  nsView* view = viewManager->CreateView(aFrame->GetRect(), parentView);

  SyncFrameViewProperties(aFrame->PresContext(), aFrame, nullptr, view);

  nsView* insertBefore = nsLayoutUtils::FindSiblingViewFor(parentView, aFrame);
  // we insert this view 'above' the insertBefore view, unless insertBefore is null,
  // in which case we want to call with aAbove == false to insert at the beginning
  // in document order
  viewManager->InsertChild(parentView, view, insertBefore, insertBefore != nullptr);

  // REVIEW: Don't create a widget for fixed-pos elements anymore.
  // ComputeRepaintRegionForCopy will calculate the right area to repaint
  // when we scroll.
  // Reparent views on any child frames (or their descendants) to this
  // view. We can just call ReparentFrameViewTo on this frame because
  // we know this frame has no view, so it will crawl the children. Also,
  // we know that any descendants with views must have 'parentView' as their
  // parent view.
  ReparentFrameViewTo(aFrame, viewManager, view, parentView);

  // Remember our view
  aFrame->SetView(view);

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
               ("nsContainerFrame::CreateViewForFrame: frame=%p view=%p",
                aFrame));
}

/**
 * Position the view associated with |aKidFrame|, if there is one. A
 * container frame should call this method after positioning a frame,
 * but before |Reflow|.
 */
void
nsContainerFrame::PositionFrameView(nsIFrame* aKidFrame)
{
  nsIFrame* parentFrame = aKidFrame->GetParent();
  if (!aKidFrame->HasView() || !parentFrame)
    return;

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

nsresult
nsContainerFrame::ReparentFrameView(nsIFrame* aChildFrame,
                                    nsIFrame* aOldParentFrame,
                                    nsIFrame* aNewParentFrame)
{
  NS_PRECONDITION(aChildFrame, "null child frame pointer");
  NS_PRECONDITION(aOldParentFrame, "null old parent frame pointer");
  NS_PRECONDITION(aNewParentFrame, "null new parent frame pointer");
  NS_PRECONDITION(aOldParentFrame != aNewParentFrame, "same old and new parent frame");

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
    return ReparentFrameViewTo(aChildFrame, oldParentView->GetViewManager(), newParentView,
                               oldParentView);
  }

  return NS_OK;
}

nsresult
nsContainerFrame::ReparentFrameViewList(const nsFrameList& aChildFrameList,
                                        nsIFrame*          aOldParentFrame,
                                        nsIFrame*          aNewParentFrame)
{
  NS_PRECONDITION(aChildFrameList.NotEmpty(), "empty child frame list");
  NS_PRECONDITION(aOldParentFrame, "null old parent frame pointer");
  NS_PRECONDITION(aNewParentFrame, "null new parent frame pointer");
  NS_PRECONDITION(aOldParentFrame != aNewParentFrame, "same old and new parent frame");

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
      ReparentFrameViewTo(e.get(), viewManager, newParentView, oldParentView);
    }
  }

  return NS_OK;
}

static nsIWidget*
GetPresContextContainerWidget(nsPresContext* aPresContext)
{
  nsCOMPtr<nsISupports> container = aPresContext->Document()->GetContainer();
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
  if (!baseWindow)
    return nullptr;

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  return mainWidget;
}

static bool
IsTopLevelWidget(nsIWidget* aWidget)
{
  nsWindowType windowType = aWidget->WindowType();
  return windowType == eWindowType_toplevel ||
         windowType == eWindowType_dialog ||
         windowType == eWindowType_sheet;
  // popups aren't toplevel so they're not handled here
}

void
nsContainerFrame::SyncWindowProperties(nsPresContext*       aPresContext,
                                       nsIFrame*            aFrame,
                                       nsView*              aView,
                                       nsRenderingContext*  aRC,
                                       uint32_t             aFlags)
{
#ifdef MOZ_XUL
  if (!aView || !nsCSSRendering::IsCanvasFrame(aFrame) || !aView->HasWidget())
    return;

  nsCOMPtr<nsIWidget> windowWidget = GetPresContextContainerWidget(aPresContext);
  if (!windowWidget || !IsTopLevelWidget(windowWidget))
    return;

  nsViewManager* vm = aView->GetViewManager();
  nsView* rootView = vm->GetRootView();

  if (aView != rootView)
    return;

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

  nsIFrame *rootFrame = aPresContext->PresShell()->FrameConstructor()->GetRootElementStyleFrame();
  if (!rootFrame)
    return;

  if (aFlags & SET_ASYNC) {
    aView->SetNeedsWindowPropertiesSync();
    return;
  }

  nsRefPtr<nsPresContext> kungFuDeathGrip(aPresContext);
  nsWeakFrame weak(rootFrame);

  nsTransparencyMode mode = nsLayoutUtils::GetFrameTransparency(aFrame, rootFrame);
  int32_t shadow = rootFrame->StyleUIReset()->mWindowShadow;
  nsCOMPtr<nsIWidget> viewWidget = aView->GetWidget();
  viewWidget->SetTransparencyMode(mode);
  windowWidget->SetWindowShadowStyle(shadow);

  if (!aRC)
    return;

  if (!weak.IsAlive()) {
    return;
  }

  nsBoxLayoutState aState(aPresContext, aRC);
  nsSize minSize = rootFrame->GetMinSize(aState);
  nsSize maxSize = rootFrame->GetMaxSize(aState);

  SetSizeConstraints(aPresContext, windowWidget, minSize, maxSize);
#endif
}

void nsContainerFrame::SetSizeConstraints(nsPresContext* aPresContext,
                                          nsIWidget* aWidget,
                                          const nsSize& aMinSize,
                                          const nsSize& aMaxSize)
{
  LayoutDeviceIntSize devMinSize(aPresContext->AppUnitsToDevPixels(aMinSize.width),
                                 aPresContext->AppUnitsToDevPixels(aMinSize.height));
  LayoutDeviceIntSize devMaxSize(aMaxSize.width == NS_INTRINSICSIZE ? NS_MAXSIZE :
                                 aPresContext->AppUnitsToDevPixels(aMaxSize.width),
                                 aMaxSize.height == NS_INTRINSICSIZE ? NS_MAXSIZE :
                                 aPresContext->AppUnitsToDevPixels(aMaxSize.height));

  // MinSize has a priority over MaxSize
  if (devMinSize.width > devMaxSize.width)
    devMaxSize.width = devMinSize.width;
  if (devMinSize.height > devMaxSize.height)
    devMaxSize.height = devMinSize.height;

  widget::SizeConstraints constraints(devMinSize, devMaxSize);

  // The sizes are in inner window sizes, so convert them into outer window sizes.
  // Use a size of (200, 200) as only the difference between the inner and outer
  // size is needed.
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

void
nsContainerFrame::SyncFrameViewAfterReflow(nsPresContext* aPresContext,
                                           nsIFrame*       aFrame,
                                           nsView*        aView,
                                           const nsRect&   aVisualOverflowArea,
                                           uint32_t        aFlags)
{
  if (!aView) {
    return;
  }

  // Make sure the view is sized and positioned correctly
  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aFrame);
  }

  if (0 == (aFlags & NS_FRAME_NO_SIZE_VIEW)) {
    nsViewManager* vm = aView->GetViewManager();

    vm->ResizeView(aView, aVisualOverflowArea, true);
  }
}

void
nsContainerFrame::SyncFrameViewProperties(nsPresContext*  aPresContext,
                                          nsIFrame*        aFrame,
                                          nsStyleContext*  aStyleContext,
                                          nsView*         aView,
                                          uint32_t         aFlags)
{
  NS_ASSERTION(!aStyleContext || aFrame->StyleContext() == aStyleContext,
               "Wrong style context for frame?");

  if (!aView) {
    return;
  }

  nsViewManager* vm = aView->GetViewManager();

  if (nullptr == aStyleContext) {
    aStyleContext = aFrame->StyleContext();
  }

  // Make sure visibility is correct. This only affects nsSubdocumentFrame.
  if (0 == (aFlags & NS_FRAME_NO_VISIBILITY) &&
      !aFrame->SupportsVisibilityHidden()) {
    // See if the view should be hidden or visible
    vm->SetViewVisibility(aView,
        aStyleContext->StyleVisibility()->IsVisible()
            ? nsViewVisibility_kShow : nsViewVisibility_kHide);
  }

  int32_t zIndex = 0;
  bool    autoZIndex = false;

  if (aFrame->IsAbsPosContaininingBlock()) {
    // Make sure z-index is correct
    const nsStylePosition* position = aStyleContext->StylePosition();

    if (position->mZIndex.GetUnit() == eStyleUnit_Integer) {
      zIndex = position->mZIndex.GetIntValue();
    } else if (position->mZIndex.GetUnit() == eStyleUnit_Auto) {
      autoZIndex = true;
    }
  } else {
    autoZIndex = true;
  }

  vm->SetViewZIndex(aView, autoZIndex, zIndex);
}

static nscoord GetCoord(const nsStyleCoord& aCoord, nscoord aIfNotCoord)
{
  if (aCoord.ConvertsToLength()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
  }
  return aIfNotCoord;
}

void
nsContainerFrame::DoInlineIntrinsicISize(nsRenderingContext *aRenderingContext,
                                         InlineIntrinsicISizeData *aData,
                                         nsLayoutUtils::IntrinsicISizeType aType)
{
  if (GetPrevInFlow())
    return; // Already added.

  NS_PRECONDITION(aType == nsLayoutUtils::MIN_ISIZE ||
                  aType == nsLayoutUtils::PREF_ISIZE, "bad type");

  WritingMode wm = GetWritingMode();
  mozilla::css::Side startSide =
    wm.PhysicalSideForInlineAxis(eLogicalEdgeStart);
  mozilla::css::Side endSide =
    wm.PhysicalSideForInlineAxis(eLogicalEdgeEnd);

  const nsStylePadding *stylePadding = StylePadding();
  const nsStyleBorder *styleBorder = StyleBorder();
  const nsStyleMargin *styleMargin = StyleMargin();

  // This goes at the beginning no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the startSide border is always on the
  // first line.
  // This frame is a first-in-flow, but it might have a previous bidi
  // continuation, in which case that continuation should handle the startSide
  // border.
  // For box-decoration-break:clone we setup clonePBM = startPBM + endPBM and
  // add that to each line.  For box-decoration-break:slice clonePBM is zero.
  nscoord clonePBM = 0; // PBM = PaddingBorderMargin
  const bool sliceBreak =
    styleBorder->mBoxDecorationBreak == NS_STYLE_BOX_DECORATION_BREAK_SLICE;
  if (!GetPrevContinuation()) {
    nscoord startPBM =
      // clamp negative calc() to 0
      std::max(GetCoord(stylePadding->mPadding.Get(startSide), 0), 0) +
      styleBorder->GetComputedBorderWidth(startSide) +
      GetCoord(styleMargin->mMargin.Get(startSide), 0);
    if (MOZ_LIKELY(sliceBreak)) {
      aData->currentLine += startPBM;
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
  }

  const nsLineList_iterator* savedLine = aData->line;
  nsIFrame* const savedLineContainer = aData->lineContainer;

  nsContainerFrame *lastInFlow;
  for (nsContainerFrame *nif = this; nif;
       nif = static_cast<nsContainerFrame*>(nif->GetNextInFlow())) {
    if (aData->currentLine == 0) {
      aData->currentLine = clonePBM;
    }
    for (nsIFrame *kid = nif->mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {
      if (aType == nsLayoutUtils::MIN_ISIZE)
        kid->AddInlineMinISize(aRenderingContext,
                               static_cast<InlineMinISizeData*>(aData));
      else
        kid->AddInlinePrefISize(aRenderingContext,
                                static_cast<InlinePrefISizeData*>(aData));
    }

    // After we advance to our next-in-flow, the stored line and line container
    // may no longer be correct. Just forget them.
    aData->line = nullptr;
    aData->lineContainer = nullptr;

    lastInFlow = nif;
  }

  aData->line = savedLine;
  aData->lineContainer = savedLineContainer;

  // This goes at the end no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the endSide border is always on the
  // last line.
  // We reached the last-in-flow, but it might have a next bidi
  // continuation, in which case that continuation should handle
  // the endSide border.
  if (MOZ_LIKELY(!lastInFlow->GetNextContinuation() && sliceBreak)) {
    aData->currentLine += endPBM;
  }
}

/* virtual */
LogicalSize
nsContainerFrame::ComputeAutoSize(nsRenderingContext* aRenderingContext,
                                  WritingMode aWM,
                                  const LogicalSize& aCBSize,
                                  nscoord aAvailableISize,
                                  const LogicalSize& aMargin,
                                  const LogicalSize& aBorder,
                                  const LogicalSize& aPadding,
                                  bool aShrinkWrap)
{
  LogicalSize result(aWM, 0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  nscoord availBased = aAvailableISize - aMargin.ISize(aWM) -
                       aBorder.ISize(aWM) - aPadding.ISize(aWM);
  // replaced elements always shrink-wrap
  if (aShrinkWrap || IsFrameOfType(eReplaced)) {
    // don't bother setting it if the result won't be used
    if (StylePosition()->ISize(aWM).GetUnit() == eStyleUnit_Auto) {
      result.ISize(aWM) = ShrinkWidthToFit(aRenderingContext, availBased);
    }
  } else {
    result.ISize(aWM) = availBased;
  }

  if (IsTableCaption()) {
    // If we're a container for font size inflation, then shrink
    // wrapping inside of us should not apply font size inflation.
    AutoMaybeDisableFontInflation an(this);

    // XXX todo: make this aware of vertical writing modes
    uint8_t captionSide = StyleTableBorder()->mCaptionSide;
    if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
        captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
      result.ISize(aWM) = GetMinISize(aRenderingContext);
    } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
               captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
      // The outer frame constrains our available width to the width of
      // the table.  Grow if our min-width is bigger than that, but not
      // larger than the containing block width.  (It would really be nice
      // to transmit that information another way, so we could grow up to
      // the table's available width, but that's harder.)
      nscoord min = GetMinISize(aRenderingContext);
      if (min > aCBSize.ISize(aWM)) {
        min = aCBSize.ISize(aWM);
      }
      if (min > result.ISize(aWM)) {
        result.ISize(aWM) = min;
      }
    }
  }
  return result;
}

void
nsContainerFrame::ReflowChild(nsIFrame*                aKidFrame,
                              nsPresContext*           aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              const WritingMode&       aWM,
                              const LogicalPoint&      aPos,
                              nscoord                  aContainerWidth,
                              uint32_t                 aFlags,
                              nsReflowStatus&          aStatus,
                              nsOverflowContinuationTracker* aTracker)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");
  if (aWM.IsVerticalRL() || (!aWM.IsVertical() && !aWM.IsBidiLTR())) {
    NS_ASSERTION(aContainerWidth != NS_UNCONSTRAINEDSIZE,
                 "ReflowChild with unconstrained container width!");
  }

  // Position the child frame and its view if requested.
  if (NS_FRAME_NO_MOVE_FRAME != (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    aKidFrame->SetPosition(aWM, aPos, aContainerWidth);
  }

  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aKidFrame);
  }

  // Reflow the child frame
  aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // If the child frame is complete, delete any next-in-flows,
  // but only if the NO_DELETE_NEXT_IN_FLOW flag isn't set.
  if (NS_FRAME_IS_FULLY_COMPLETE(aStatus) &&
      !(aFlags & NS_FRAME_NO_DELETE_NEXT_IN_FLOW_CHILD)) {
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

//XXX temporary: hold on to a copy of the old physical version of
//    ReflowChild so that we can convert callers incrementally.
void
nsContainerFrame::ReflowChild(nsIFrame*                aKidFrame,
                              nsPresContext*           aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nscoord                  aX,
                              nscoord                  aY,
                              uint32_t                 aFlags,
                              nsReflowStatus&          aStatus,
                              nsOverflowContinuationTracker* aTracker)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");

  // Position the child frame and its view if requested.
  if (NS_FRAME_NO_MOVE_FRAME != (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    aKidFrame->SetPosition(nsPoint(aX, aY));
  }

  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aKidFrame);
  }

  // Reflow the child frame
  aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // If the child frame is complete, delete any next-in-flows,
  // but only if the NO_DELETE_NEXT_IN_FLOW flag isn't set.
  if (NS_FRAME_IS_FULLY_COMPLETE(aStatus) &&
      !(aFlags & NS_FRAME_NO_DELETE_NEXT_IN_FLOW_CHILD)) {
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
void
nsContainerFrame::PositionChildViews(nsIFrame* aFrame)
{
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
 * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aX and aY are ignored in this
 *    case. Also implies NS_FRAME_NO_MOVE_VIEW
 * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
 *    don't want to automatically sync the frame and view
 * NS_FRAME_NO_SIZE_VIEW - don't size the frame's view
 */
void
nsContainerFrame::FinishReflowChild(nsIFrame*                  aKidFrame,
                                    nsPresContext*             aPresContext,
                                    const nsHTMLReflowMetrics& aDesiredSize,
                                    const nsHTMLReflowState*   aReflowState,
                                    const WritingMode&         aWM,
                                    const LogicalPoint&        aPos,
                                    nscoord                    aContainerWidth,
                                    uint32_t                   aFlags)
{
  if (aWM.IsVerticalRL() || (!aWM.IsVertical() && !aWM.IsBidiLTR())) {
    NS_ASSERTION(aContainerWidth != NS_UNCONSTRAINEDSIZE,
                 "FinishReflowChild with unconstrained container width!");
  }

  nsPoint curOrigin = aKidFrame->GetPosition();
  WritingMode outerWM = aDesiredSize.GetWritingMode();
  LogicalSize convertedSize = aDesiredSize.Size(outerWM).ConvertTo(aWM,
                                                                   outerWM);

  if (NS_FRAME_NO_MOVE_FRAME != (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    aKidFrame->SetRect(aWM, LogicalRect(aWM, aPos, convertedSize),
                       aContainerWidth);
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
  if (!(aFlags & NS_FRAME_NO_MOVE_VIEW) && curOrigin != newOrigin) {
    if (!aKidFrame->HasView()) {
      // If the frame has moved, then we need to make sure any child views are
      // correctly positioned
      PositionChildViews(aKidFrame);
    }
  }

  aKidFrame->DidReflow(aPresContext, aReflowState, nsDidReflowStatus::FINISHED);
}

//XXX temporary: hold on to a copy of the old physical version of
//    FinishReflowChild so that we can convert callers incrementally.
void
nsContainerFrame::FinishReflowChild(nsIFrame*                  aKidFrame,
                                    nsPresContext*             aPresContext,
                                    const nsHTMLReflowMetrics& aDesiredSize,
                                    const nsHTMLReflowState*   aReflowState,
                                    nscoord                    aX,
                                    nscoord                    aY,
                                    uint32_t                   aFlags)
{
  nsPoint curOrigin = aKidFrame->GetPosition();

  if (NS_FRAME_NO_MOVE_FRAME != (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    aKidFrame->SetRect(nsRect(aX, aY, aDesiredSize.Width(), aDesiredSize.Height()));
  } else {
    aKidFrame->SetSize(nsSize(aDesiredSize.Width(), aDesiredSize.Height()));
  }

  if (aKidFrame->HasView()) {
    nsView* view = aKidFrame->GetView();
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                             aDesiredSize.VisualOverflow(), aFlags);
  }

  if (!(aFlags & NS_FRAME_NO_MOVE_VIEW) &&
      (curOrigin.x != aX || curOrigin.y != aY)) {
    if (!aKidFrame->HasView()) {
      // If the frame has moved, then we need to make sure any child views are
      // correctly positioned
      PositionChildViews(aKidFrame);
    }
  }

  aKidFrame->DidReflow(aPresContext, aReflowState, nsDidReflowStatus::FINISHED);
}

void
nsContainerFrame::ReflowOverflowContainerChildren(nsPresContext*           aPresContext,
                                                  const nsHTMLReflowState& aReflowState,
                                                  nsOverflowAreas&         aOverflowRects,
                                                  uint32_t                 aFlags,
                                                  nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aPresContext, "null pointer");

  nsFrameList* overflowContainers =
               GetPropTableFrames(OverflowContainersProperty());

  NS_ASSERTION(!(overflowContainers && GetPrevInFlow()
                 && static_cast<nsContainerFrame*>(GetPrevInFlow())
                      ->GetPropTableFrames(ExcessOverflowContainersProperty())),
               "conflicting overflow containers lists");

  if (!overflowContainers) {
    // Drain excess from previnflow
    nsContainerFrame* prev = (nsContainerFrame*) GetPrevInFlow();
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
  // present if our next-in-flow hasn't been reflown yet.
  nsFrameList* selfExcessOCFrames =
    RemovePropTableFrames(ExcessOverflowContainersProperty());
  if (selfExcessOCFrames) {
    if (overflowContainers) {
      overflowContainers->AppendFrames(nullptr, *selfExcessOCFrames);
      selfExcessOCFrames->Delete(aPresContext->PresShell());
    } else {
      overflowContainers = selfExcessOCFrames;
      SetPropTableFrames(overflowContainers, OverflowContainersProperty());
    }
  }
  if (!overflowContainers) {
    return; // nothing to reflow
  }

  nsOverflowContinuationTracker tracker(this, false, false);
  bool shouldReflowAllKids = aReflowState.ShouldReflowAllKids();

  for (nsIFrame* frame = overflowContainers->FirstChild(); frame;
       frame = frame->GetNextSibling()) {
    if (frame->GetPrevInFlow()->GetParent() != GetPrevInFlow()) {
      // frame's prevInFlow has moved, skip reflowing this frame;
      // it will get reflowed once it's been placed
      continue;
    }
    // If the available vertical height has changed, we need to reflow
    // even if the frame isn't dirty.
    if (shouldReflowAllKids || NS_SUBTREE_DIRTY(frame)) {
      // Get prev-in-flow
      nsIFrame* prevInFlow = frame->GetPrevInFlow();
      NS_ASSERTION(prevInFlow,
                   "overflow container frame must have a prev-in-flow");
      NS_ASSERTION(frame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER,
                   "overflow container frame must have overflow container bit set");
      WritingMode wm = frame->GetWritingMode();
      nscoord containerWidth = aReflowState.AvailableSize(wm).Width(wm);
      LogicalRect prevRect = prevInFlow->GetLogicalRect(wm, containerWidth);

      // Initialize reflow params
      LogicalSize availSpace(wm, prevRect.ISize(wm),
                             aReflowState.AvailableSize(wm).BSize(wm));
      nsHTMLReflowMetrics desiredSize(aReflowState);
      nsHTMLReflowState frameState(aPresContext, aReflowState,
                                   frame, availSpace);
      nsReflowStatus frameStatus;

      // Reflow
      LogicalPoint pos(wm, prevRect.IStart(wm), 0);
      ReflowChild(frame, aPresContext, desiredSize, frameState,
                  wm, pos, containerWidth, aFlags, frameStatus, &tracker);
      //XXXfr Do we need to override any shrinkwrap effects here?
      // e.g. desiredSize.Width() = prevRect.width;
      FinishReflowChild(frame, aPresContext, desiredSize, &frameState,
                        wm, pos, containerWidth, aFlags);

      // Handle continuations
      if (!NS_FRAME_IS_FULLY_COMPLETE(frameStatus)) {
        if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
          // Abspos frames can't cause their parent to be incomplete,
          // only overflow incomplete.
          NS_FRAME_SET_OVERFLOW_INCOMPLETE(frameStatus);
        }
        else {
          NS_ASSERTION(NS_FRAME_IS_COMPLETE(frameStatus),
                       "overflow container frames can't be incomplete, only overflow-incomplete");
        }

        // Acquire a next-in-flow, creating it if necessary
        nsIFrame* nif = frame->GetNextInFlow();
        if (!nif) {
          NS_ASSERTION(frameStatus & NS_FRAME_REFLOW_NEXTINFLOW,
                       "Someone forgot a REFLOW_NEXTINFLOW flag");
          nif = aPresContext->PresShell()->FrameConstructor()->
            CreateContinuingFrame(aPresContext, frame, this);
        }
        else if (!(nif->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          // used to be a normal next-in-flow; steal it from the child list
          nsresult rv = nif->GetParent()->StealFrame(nif);
          if (NS_FAILED(rv)) {
            return;
          }
        }

        tracker.Insert(nif, frameStatus);
      }
      NS_MergeReflowStatusInto(&aStatus, frameStatus);
      // At this point it would be nice to assert !frame->GetOverflowRect().IsEmpty(),
      // but we have some unsplittable frames that, when taller than
      // availableHeight will push zero-height content into a next-in-flow.
    }
    else {
      tracker.Skip(frame, aStatus);
      if (aReflowState.mFloatManager) {
        nsBlockFrame::RecoverFloatsFor(frame, *aReflowState.mFloatManager,
                                       aReflowState.GetWritingMode(),
                                       aReflowState.ComputedWidth());
      }
    }
    ConsiderChildOverflow(aOverflowRects, frame);
  }
}

void
nsContainerFrame::DisplayOverflowContainers(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsFrameList* overflowconts = GetPropTableFrames(OverflowContainersProperty());
  if (overflowconts) {
    for (nsIFrame* frame = overflowconts->FirstChild(); frame;
         frame = frame->GetNextSibling()) {
      BuildDisplayListForChild(aBuilder, frame, aDirtyRect, aLists);
    }
  }
}

static bool
TryRemoveFrame(nsIFrame* aFrame, FramePropertyTable* aPropTable,
               const FramePropertyDescriptor* aProp, nsIFrame* aChildToRemove)
{
  nsFrameList* list = static_cast<nsFrameList*>(aPropTable->Get(aFrame, aProp));
  if (list && list->StartRemoveFrame(aChildToRemove)) {
    // aChildToRemove *may* have been removed from this list.
    if (list->IsEmpty()) {
      aPropTable->Remove(aFrame, aProp);
      list->Delete(aFrame->PresContext()->PresShell());
    }
    return true;
  }
  return false;
}

nsresult
nsContainerFrame::StealFrame(nsIFrame* aChild,
                             bool      aForceNormal)
{
#ifdef DEBUG
  if (!mFrames.ContainsFrame(aChild)) {
    nsFrameList* list = GetOverflowFrames();
    if (!list || !list->ContainsFrame(aChild)) {
      FramePropertyTable* propTable = PresContext()->PropertyTable();
      list = static_cast<nsFrameList*>(
               propTable->Get(this, OverflowContainersProperty()));
      if (!list || !list->ContainsFrame(aChild)) {
        list = static_cast<nsFrameList*>(
                 propTable->Get(this, ExcessOverflowContainersProperty()));
        MOZ_ASSERT(list && list->ContainsFrame(aChild), "aChild isn't our child"
                   " or on a frame list not supported by StealFrame");
      }
    }
  }
#endif

  bool removed;
  if ((aChild->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)
      && !aForceNormal) {
    FramePropertyTable* propTable = PresContext()->PropertyTable();
    // Try removing from the overflow container list.
    removed = ::TryRemoveFrame(this, propTable, OverflowContainersProperty(),
                               aChild);
    if (!removed) {
      // It must be in the excess overflow container list.
      removed = ::TryRemoveFrame(this, propTable,
                                 ExcessOverflowContainersProperty(),
                                 aChild);
    }
  } else {
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

  NS_POSTCONDITION(removed, "StealFrame: can't find aChild");
  return removed ? NS_OK : NS_ERROR_UNEXPECTED;
}

nsFrameList
nsContainerFrame::StealFramesAfter(nsIFrame* aChild)
{
  NS_ASSERTION(!aChild ||
               !(aChild->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER),
               "StealFramesAfter doesn't handle overflow containers");
  NS_ASSERTION(GetType() != nsGkAtoms::blockFrame, "unexpected call");

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
nsIFrame*
nsContainerFrame::CreateNextInFlow(nsIFrame* aFrame)
{
  NS_PRECONDITION(GetType() != nsGkAtoms::blockFrame,
                  "you should have called nsBlockFrame::CreateContinuationFor instead");
  NS_PRECONDITION(mFrames.ContainsFrame(aFrame), "expected an in-flow child frame");

  nsPresContext* pc = PresContext();
  nsIFrame* nextInFlow = aFrame->GetNextInFlow();
  if (nullptr == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our child list.
    nextInFlow = pc->PresShell()->FrameConstructor()->
      CreateContinuingFrame(pc, aFrame, this);
    mFrames.InsertFrame(nullptr, aFrame, nextInFlow);

    NS_FRAME_LOG(NS_FRAME_TRACE_NEW_FRAMES,
       ("nsContainerFrame::CreateNextInFlow: frame=%p nextInFlow=%p",
        aFrame, nextInFlow));

    return nextInFlow;
  }
  return nullptr;
}

/**
 * Remove and delete aNextInFlow and its next-in-flows. Updates the sibling and flow
 * pointers
 */
void
nsContainerFrame::DeleteNextInFlowChild(nsIFrame* aNextInFlow,
                                        bool      aDeletingEmptyFrames)
{
#ifdef DEBUG
  nsIFrame* prevInFlow = aNextInFlow->GetPrevInFlow();
#endif
  NS_PRECONDITION(prevInFlow, "bad prev-in-flow");

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  // Do this in a loop so we don't overflow the stack for frames
  // with very many next-in-flows
  nsIFrame* nextNextInFlow = aNextInFlow->GetNextInFlow();
  if (nextNextInFlow) {
    nsAutoTArray<nsIFrame*, 8> frames;
    for (nsIFrame* f = nextNextInFlow; f; f = f->GetNextInFlow()) {
      frames.AppendElement(f);
    }
    for (int32_t i = frames.Length() - 1; i >= 0; --i) {
      nsIFrame* delFrame = frames.ElementAt(i);
      delFrame->GetParent()->
        DeleteNextInFlowChild(delFrame, aDeletingEmptyFrames);
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

  NS_POSTCONDITION(!prevInFlow->GetNextInFlow(), "non null next-in-flow");
}

/**
 * Set the frames on the overflow list
 */
void
nsContainerFrame::SetOverflowFrames(const nsFrameList& aOverflowFrames)
{
  NS_PRECONDITION(aOverflowFrames.NotEmpty(), "Shouldn't be called");

  nsPresContext* pc = PresContext();
  nsFrameList* newList = new (pc->PresShell()) nsFrameList(aOverflowFrames);

  pc->PropertyTable()->Set(this, OverflowProperty(), newList);
}

nsFrameList*
nsContainerFrame::GetPropTableFrames(const FramePropertyDescriptor* aProperty) const
{
  FramePropertyTable* propTable = PresContext()->PropertyTable();
  return static_cast<nsFrameList*>(propTable->Get(this, aProperty));
}

nsFrameList*
nsContainerFrame::RemovePropTableFrames(const FramePropertyDescriptor* aProperty)
{
  FramePropertyTable* propTable = PresContext()->PropertyTable();
  return static_cast<nsFrameList*>(propTable->Remove(this, aProperty));
}

void
nsContainerFrame::SetPropTableFrames(nsFrameList*                   aFrameList,
                                     const FramePropertyDescriptor* aProperty)
{
  NS_PRECONDITION(aProperty && aFrameList, "null ptr");
  NS_PRECONDITION(
    (aProperty != nsContainerFrame::OverflowContainersProperty() &&
     aProperty != nsContainerFrame::ExcessOverflowContainersProperty()) ||
    IsFrameOfType(nsIFrame::eCanContainOverflowContainers),
    "this type of frame can't have overflow containers");
  MOZ_ASSERT(!GetPropTableFrames(aProperty));
  PresContext()->PropertyTable()->Set(this, aProperty, aFrameList);
}

/**
 * Push aFromChild and its next siblings to the next-in-flow. Change the
 * geometric parent of each frame that's pushed. If there is no next-in-flow
 * the frames are placed on the overflow list (and the geometric parent is
 * left unchanged).
 *
 * Updates the next-in-flow's child count. Does <b>not</b> update the
 * pusher's child count.
 *
 * @param   aFromChild the first child frame to push. It is disconnected from
 *            aPrevSibling
 * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
 *            an error to push a parent's first child frame
 */
void
nsContainerFrame::PushChildren(nsIFrame* aFromChild,
                               nsIFrame* aPrevSibling)
{
  NS_PRECONDITION(aFromChild, "null pointer");
  NS_PRECONDITION(aPrevSibling, "pushing first child");
  NS_PRECONDITION(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

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
  }
  else {
    // Add the frames to our overflow list
    SetOverflowFrames(tail);
  }
}

/**
 * Moves any frames on the overflow lists (the prev-in-flow's overflow list and
 * the receiver's overflow list) to the child list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @return  true if any frames were moved and false otherwise
 */
bool
nsContainerFrame::MoveOverflowToChildList()
{
  bool result = false;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)GetPrevInFlow();
  if (nullptr != prevInFlow) {
    AutoFrameListPtr prevOverflowFrames(PresContext(),
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Tables are special; they can have repeated header/footer
      // frames on mFrames at this point.
      NS_ASSERTION(mFrames.IsEmpty() || GetType() == nsGkAtoms::tableFrame,
                   "bad overflow list");
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(*prevOverflowFrames,
                                              prevInFlow, this);
      mFrames.AppendFrames(this, *prevOverflowFrames);
      result = true;
    }
  }

  // It's also possible that we have an overflow list for ourselves.
  return DrainSelfOverflowList() || result;
}

bool
nsContainerFrame::DrainSelfOverflowList()
{
  AutoFrameListPtr overflowFrames(PresContext(), StealOverflowFrames());
  if (overflowFrames) {
    mFrames.AppendFrames(nullptr, *overflowFrames);
    return true;
  }
  return false;
}

nsIFrame*
nsContainerFrame::GetNextInFlowChild(ContinuationTraversingState& aState,
                                     bool* aIsInOverflow)
{
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

nsIFrame*
nsContainerFrame::PullNextInFlowChild(ContinuationTraversingState& aState)
{
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

bool
nsContainerFrame::ResolvedOrientationIsVertical()
{
  uint8_t orient = StyleDisplay()->mOrient;
  switch (orient) {
    case NS_STYLE_ORIENT_HORIZONTAL:
      return false;
    case NS_STYLE_ORIENT_VERTICAL:
      return true;
    case NS_STYLE_ORIENT_INLINE:
      return GetWritingMode().IsVertical();
    case NS_STYLE_ORIENT_BLOCK:
      return !GetWritingMode().IsVertical();
  }
  NS_NOTREACHED("unexpected -moz-orient value");
  return false;
}

nsOverflowContinuationTracker::nsOverflowContinuationTracker(nsContainerFrame* aFrame,
                                                             bool              aWalkOOFFrames,
                                                             bool              aSkipOverflowContainerChildren)
  : mOverflowContList(nullptr),
    mPrevOverflowCont(nullptr),
    mSentry(nullptr),
    mParent(aFrame),
    mSkipOverflowContainerChildren(aSkipOverflowContainerChildren),
    mWalkOOFFrames(aWalkOOFFrames)
{
  NS_PRECONDITION(aFrame, "null frame pointer");
  SetupOverflowContList();
}

void
nsOverflowContinuationTracker::SetupOverflowContList()
{
  NS_PRECONDITION(mParent, "null frame pointer");
  NS_PRECONDITION(!mOverflowContList, "already have list");
  nsContainerFrame* nif =
    static_cast<nsContainerFrame*>(mParent->GetNextInFlow());
  if (nif) {
    mOverflowContList = nif->GetPropTableFrames(
      nsContainerFrame::OverflowContainersProperty());
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
void
nsOverflowContinuationTracker::SetUpListWalker()
{
  NS_ASSERTION(!mSentry && !mPrevOverflowCont,
               "forgot to reset mSentry or mPrevOverflowCont");
  if (mOverflowContList) {
    nsIFrame* cur = mOverflowContList->FirstChild();
    if (mSkipOverflowContainerChildren) {
      while (cur && (cur->GetPrevInFlow()->GetStateBits()
                     & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
        mPrevOverflowCont = cur;
        cur = cur->GetNextSibling();
      }
      while (cur && (!(cur->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
                     == mWalkOOFFrames)) {
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
void
nsOverflowContinuationTracker::StepForward()
{
  NS_PRECONDITION(mOverflowContList, "null list");

  // Step forward
  if (mPrevOverflowCont) {
    mPrevOverflowCont = mPrevOverflowCont->GetNextSibling();
  }
  else {
    mPrevOverflowCont = mOverflowContList->FirstChild();
  }

  // Skip over oof or non-oof frames as appropriate
  if (mSkipOverflowContainerChildren) {
    nsIFrame* cur = mPrevOverflowCont->GetNextSibling();
    while (cur && (!(cur->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
                   == mWalkOOFFrames)) {
      mPrevOverflowCont = cur;
      cur = cur->GetNextSibling();
    }
  }

  // Set up the sentry
  mSentry = (mPrevOverflowCont->GetNextSibling())
            ? mPrevOverflowCont->GetNextSibling()->GetPrevInFlow()
            : nullptr;
}

nsresult
nsOverflowContinuationTracker::Insert(nsIFrame*       aOverflowCont,
                                      nsReflowStatus& aReflowStatus)
{
  NS_PRECONDITION(aOverflowCont, "null frame pointer");
  NS_PRECONDITION(!mSkipOverflowContainerChildren || mWalkOOFFrames ==
                  !!(aOverflowCont->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
                  "shouldn't insert frame that doesn't match walker type");
  NS_PRECONDITION(aOverflowCont->GetPrevInFlow(),
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
    }
    else {
      aOverflowCont->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
    }
    if (!mOverflowContList) {
      mOverflowContList = new (presContext->PresShell()) nsFrameList();
      mParent->SetPropTableFrames(mOverflowContList,
        nsContainerFrame::ExcessOverflowContainersProperty());
      SetUpListWalker();
    }
    if (aOverflowCont->GetParent() != mParent) {
      nsContainerFrame::ReparentFrameView(aOverflowCont,
                                          aOverflowCont->GetParent(),
                                          mParent);
      reparented = true;
    }

    // If aOverflowCont has a prev/next-in-flow that might be in
    // mOverflowContList we need to find it and insert after/before it to
    // maintain the order amongst next-in-flows in this list.
    nsIFrame* pif = aOverflowCont->GetPrevInFlow();
    nsIFrame* nif = aOverflowCont->GetNextInFlow();
    if ((pif && pif->GetParent() == mParent && pif != mPrevOverflowCont) ||
        (nif && nif->GetParent() == mParent && mPrevOverflowCont)) {
      for (nsFrameList::Enumerator e(*mOverflowContList); !e.AtEnd(); e.Next()) {
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
    aReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
  }

  // If we need to reflow it, mark it dirty
  if (aReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW)
    aOverflowCont->AddStateBits(NS_FRAME_IS_DIRTY);

  // It's in our list, just step forward
  StepForward();
  NS_ASSERTION(mPrevOverflowCont == aOverflowCont ||
               (mSkipOverflowContainerChildren &&
                (mPrevOverflowCont->GetStateBits() & NS_FRAME_OUT_OF_FLOW) !=
                (aOverflowCont->GetStateBits() & NS_FRAME_OUT_OF_FLOW)),
              "OverflowContTracker in unexpected state");

  if (addToList) {
    // Convert all non-overflow-container continuations of aOverflowCont
    // into overflow containers and move them to our overflow
    // tracker. This preserves the invariant that the next-continuations
    // of an overflow container are also overflow containers.
    nsIFrame* f = aOverflowCont->GetNextContinuation();
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

void
nsOverflowContinuationTracker::BeginFinish(nsIFrame* aChild)
{
  NS_PRECONDITION(aChild, "null ptr");
  NS_PRECONDITION(aChild->GetNextInFlow(),
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

void
nsOverflowContinuationTracker::EndFinish(nsIFrame* aChild)
{
  if (!mOverflowContList) {
    return;
  }
  // Forget mOverflowContList if it was deleted.
  nsPresContext* pc = aChild->PresContext();
  FramePropertyTable* propTable = pc->PropertyTable();
  nsFrameList* eoc = static_cast<nsFrameList*>(propTable->Get(mParent,
                       nsContainerFrame::ExcessOverflowContainersProperty()));
  if (eoc != mOverflowContList) {
    nsFrameList* oc = static_cast<nsFrameList*>(propTable->Get(mParent,
                        nsContainerFrame::OverflowContainersProperty()));
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
void
nsContainerFrame::List(FILE* out, const char* aPrefix, uint32_t aFlags) const
{
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
      str += nsPrintfCString("%s %p ", mozilla::layout::ChildListName(lists.CurrentID()),
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
#endif
