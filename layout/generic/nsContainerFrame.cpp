/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Elika J. Etemad ("fantasai") <fantasai@inkedblade.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* base class #1 for rendering objects that have child lists */

#include "nsContainerFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsHTMLContainerFrame.h"
#include "nsFrameManager.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsCSSRendering.h"
#include "nsTransform2D.h"
#include "nsRegion.h"
#include "nsLayoutErrors.h"
#include "nsDisplayList.h"
#include "nsContentErrors.h"
#include "nsListControlFrame.h"
#include "nsIBaseWindow.h"
#include "nsThemeConstants.h"
#include "nsCSSFrameConstructor.h"
#include "nsThemeConstants.h"
#include "mozilla/dom/Element.h"

#ifdef NS_DEBUG
#undef NOISY
#else
#undef NOISY
#endif

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_FRAMEARENA_HELPERS(nsContainerFrame)

nsContainerFrame::~nsContainerFrame()
{
}

NS_QUERYFRAME_HEAD(nsContainerFrame)
  NS_QUERYFRAME_ENTRY(nsContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSplittableFrame)

NS_IMETHODIMP
nsContainerFrame::Init(nsIContent* aContent,
                       nsIFrame*   aParent,
                       nsIFrame*   aPrevInFlow)
{
  nsresult rv = nsSplittableFrame::Init(aContent, aParent, aPrevInFlow);
  if (aPrevInFlow) {
    // Make sure we copy bits from our prev-in-flow that will affect
    // us. A continuation for a container frame needs to know if it
    // has a child with a view so that we'll properly reposition it.
    if (aPrevInFlow->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)
      AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }
  return rv;
}

NS_IMETHODIMP
nsContainerFrame::SetInitialChildList(ChildListID  aListID,
                                      nsFrameList& aChildList)
{
  nsresult  result;
  if (mFrames.NotEmpty()) {
    // We already have child frames which means we've already been
    // initialized
    NS_NOTREACHED("unexpected second call to SetInitialChildList");
    result = NS_ERROR_UNEXPECTED;
  } else if (aListID != kPrincipalList) {
    // All we know about is the principal child list.
    NS_NOTREACHED("unknown frame list");
    result = NS_ERROR_INVALID_ARG;
  } else {
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aChildList);
#endif
    mFrames.SetFrames(aChildList);
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
nsContainerFrame::AppendFrames(ChildListID  aListID,
                               nsFrameList& aFrameList)
{
  if (aListID != kPrincipalList) {
#ifdef IBMBIDI
    if (aListID != kNoReflowPrincipalList)
#endif
    {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }
  if (aFrameList.NotEmpty()) {
    mFrames.AppendFrames(this, aFrameList);

    // Ask the parent frame to reflow me.
#ifdef IBMBIDI
    if (aListID == kPrincipalList)
#endif
    {
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::InsertFrames(ChildListID aListID,
                               nsIFrame* aPrevFrame,
                               nsFrameList& aFrameList)
{
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if (aListID != kPrincipalList) {
#ifdef IBMBIDI
    if (aListID != kNoReflowPrincipalList)
#endif
    {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }
  if (aFrameList.NotEmpty()) {
    // Insert frames after aPrevFrame
    mFrames.InsertFrames(this, aPrevFrame, aFrameList);

#ifdef IBMBIDI
    if (aListID == kPrincipalList)
#endif
    {
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::RemoveFrame(ChildListID aListID,
                              nsIFrame* aOldFrame)
{
  if (aListID != kPrincipalList) {
#ifdef IBMBIDI
    if (kNoReflowPrincipalList != aListID)
#endif
    {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (aOldFrame) {
    // Loop and destroy the frame and all of its continuations.
    // If the frame we are removing is a brFrame, we need a reflow so
    // the line the brFrame was on can attempt to pull up any frames
    // that can fit from lines below it.
    bool generateReflowCommand = true;
#ifdef IBMBIDI
    if (kNoReflowPrincipalList == aListID) {
      generateReflowCommand = PR_FALSE;
    }
#endif
    nsContainerFrame* parent = static_cast<nsContainerFrame*>(aOldFrame->GetParent());
    while (aOldFrame) {
      // When the parent is an inline frame we have a simple task - just
      // remove the frame from its parents list and generate a reflow
      // command.
      nsIFrame* oldFrameNextContinuation = aOldFrame->GetNextContinuation();
      //XXXfr probably should use StealFrame here. I'm not sure if we need to
      //      check the overflow lists atm, but we'll need a prescontext lookup
      //      for overflow containers once we can split abspos elements with
      //      inline containing blocks.
      if (parent == this) {
        if (!parent->mFrames.DestroyFrameIfPresent(aOldFrame)) {
          // Try to remove it from our overflow list, if we have one.
          // The simplest way is to reuse StealFrame.
          StealFrame(PresContext(), aOldFrame, PR_TRUE);
          aOldFrame->Destroy();
        }
      } else {
        // This recursive call takes care of all continuations after aOldFrame,
        // so we don't need to loop anymore.
        parent->RemoveFrame(kPrincipalList, aOldFrame);
        break;
      }
      aOldFrame = oldFrameNextContinuation;
      if (aOldFrame) {
        parent = static_cast<nsContainerFrame*>(aOldFrame->GetParent());
      }
    }

    if (generateReflowCommand) {
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  return NS_OK;
}

void
nsContainerFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Prevent event dispatch during destruction
  if (HasView()) {
    GetView()->SetClientData(nsnull);
  }

  // Delete the primary child list
  mFrames.DestroyFramesFrom(aDestructRoot);

  // Destroy auxiliary frame lists
  nsPresContext* prescontext = PresContext();

  DestroyOverflowList(prescontext, aDestructRoot);

  if (IsFrameOfType(nsIFrame::eCanContainOverflowContainers)) {
    nsFrameList* frameList =
      RemovePropTableFrames(prescontext, OverflowContainersProperty());
    if (frameList)
      frameList->DestroyFrom(aDestructRoot);

    frameList = RemovePropTableFrames(prescontext,
                                      ExcessOverflowContainersProperty());
    if (frameList)
      frameList->DestroyFrom(aDestructRoot);
  }

  // Destroy the frame and remove the flow pointers
  nsSplittableFrame::DestroyFrom(aDestructRoot);
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

nsFrameList
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
      nsFrameList* list =
        GetPropTableFrames(PresContext(), OverflowContainersProperty());
      return list ? *list : nsFrameList::EmptyList();
    }
    case kExcessOverflowContainersList: {
      nsFrameList* list =
        GetPropTableFrames(PresContext(), ExcessOverflowContainersProperty());
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
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

NS_IMETHODIMP
nsContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  return BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists);
}

nsresult
nsContainerFrame::BuildDisplayListForNonBlockChildren(nsDisplayListBuilder*   aBuilder,
                                                      const nsRect&           aDirtyRect,
                                                      const nsDisplayListSet& aLists,
                                                      PRUint32                aFlags)
{
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background directly onto the content list
  nsDisplayListSet set(aLists, aLists.Content());
  // The children should be in content order
  while (kid) {
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set, aFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

/* virtual */ void
nsContainerFrame::ChildIsDirty(nsIFrame* aChild)
{
  AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
}

bool
nsContainerFrame::IsLeaf() const
{
  return PR_FALSE;
}

bool
nsContainerFrame::PeekOffsetNoAmount(bool aForward, PRInt32* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return PR_FALSE;
}

bool
nsContainerFrame::PeekOffsetCharacter(bool aForward, PRInt32* aOffset,
                                      bool aRespectClusters)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return PR_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Helper member functions

static nsresult
ReparentFrameViewTo(nsIFrame*       aFrame,
                    nsIViewManager* aViewManager,
                    nsIView*        aNewParentView,
                    nsIView*        aOldParentView)
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
    nsIView* view = aFrame->GetView();
    // Verify that the current parent view is what we think it is
    //nsIView*  parentView;
    //NS_ASSERTION(parentView == aOldParentView, "unexpected parent view");

    aViewManager->RemoveChild(view);
    
    // The view will remember the Z-order and other attributes that have been set on it.
    nsIView* insertBefore = nsLayoutUtils::FindSiblingViewFor(aNewParentView, aFrame);
    aViewManager->InsertChild(aNewParentView, view, insertBefore, insertBefore != nsnull);
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

nsresult
nsContainerFrame::CreateViewForFrame(nsIFrame* aFrame,
                                     bool aForce)
{
  if (aFrame->HasView()) {
    return NS_OK;
  }

  // If we don't yet have a view, see if we need a view
  if (!aForce && !aFrame->NeedsView()) {
    // don't need a view
    return NS_OK;
  }

  nsIView* parentView = aFrame->GetParent()->GetClosestView();
  NS_ASSERTION(parentView, "no parent with view");

  nsIViewManager* viewManager = parentView->GetViewManager();
  NS_ASSERTION(viewManager, "null view manager");

  // Create a view
  nsIView* view = viewManager->CreateView(aFrame->GetRect(), parentView);
  if (!view)
    return NS_ERROR_OUT_OF_MEMORY;

  SyncFrameViewProperties(aFrame->PresContext(), aFrame, nsnull, view);

  nsIView* insertBefore = nsLayoutUtils::FindSiblingViewFor(parentView, aFrame);
  // we insert this view 'above' the insertBefore view, unless insertBefore is null,
  // in which case we want to call with aAbove == PR_FALSE to insert at the beginning
  // in document order
  viewManager->InsertChild(parentView, view, insertBefore, insertBefore != nsnull);

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
               ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p view=%p",
                aFrame));
  return NS_OK;
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

  nsIView* view = aKidFrame->GetView();
  nsIViewManager* vm = view->GetViewManager();
  nsPoint pt;
  nsIView* ancestorView = parentFrame->GetClosestView(&pt);

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
nsContainerFrame::ReparentFrameView(nsPresContext* aPresContext,
                                    nsIFrame*       aChildFrame,
                                    nsIFrame*       aOldParentFrame,
                                    nsIFrame*       aNewParentFrame)
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
  nsIView* oldParentView = aOldParentFrame->GetClosestView();
  nsIView* newParentView = aNewParentFrame->GetClosestView();
  
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
nsContainerFrame::ReparentFrameViewList(nsPresContext*     aPresContext,
                                        const nsFrameList& aChildFrameList,
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
  nsIView* oldParentView = aOldParentFrame->GetClosestView();
  nsIView* newParentView = aNewParentFrame->GetClosestView();
  
  // See if the old parent frame and the new parent frame are in the
  // same view sub-hierarchy. If they are then we don't have to do
  // anything
  if (oldParentView != newParentView) {
    nsIViewManager* viewManager = oldParentView->GetViewManager();

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
    return nsnull;

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  return mainWidget;
}

static bool
IsTopLevelWidget(nsIWidget* aWidget)
{
  nsWindowType windowType;
  aWidget->GetWindowType(windowType);
  return windowType == eWindowType_toplevel ||
         windowType == eWindowType_dialog ||
         windowType == eWindowType_sheet;
  // popups aren't toplevel so they're not handled here
}

void
nsContainerFrame::SyncWindowProperties(nsPresContext*       aPresContext,
                                       nsIFrame*            aFrame,
                                       nsIView*             aView)
{
#ifdef MOZ_XUL
  if (!aView || !nsCSSRendering::IsCanvasFrame(aFrame) || !aView->HasWidget())
    return;

  nsIWidget* windowWidget = GetPresContextContainerWidget(aPresContext);
  if (!windowWidget || !IsTopLevelWidget(windowWidget))
    return;

  nsIViewManager* vm = aView->GetViewManager();
  nsIView* rootView = vm->GetRootView();

  if (aView != rootView)
    return;

  Element* rootElement = aPresContext->Document()->GetRootElement();
  if (!rootElement || !rootElement->IsXUL()) {
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

  nsTransparencyMode mode = nsLayoutUtils::GetFrameTransparency(aFrame, rootFrame);
  nsIWidget* viewWidget = aView->GetWidget();
  viewWidget->SetTransparencyMode(mode);
  windowWidget->SetWindowShadowStyle(rootFrame->GetStyleUIReset()->mWindowShadow);
#endif
}

void
nsContainerFrame::SyncFrameViewAfterReflow(nsPresContext* aPresContext,
                                           nsIFrame*       aFrame,
                                           nsIView*        aView,
                                           const nsRect&   aVisualOverflowArea,
                                           PRUint32        aFlags)
{
  if (!aView) {
    return;
  }

  // Make sure the view is sized and positioned correctly
  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aFrame);
  }

  if (0 == (aFlags & NS_FRAME_NO_SIZE_VIEW)) {
    nsIViewManager* vm = aView->GetViewManager();

    vm->ResizeView(aView, aVisualOverflowArea, PR_TRUE);
  }
}

void
nsContainerFrame::SyncFrameViewProperties(nsPresContext*  aPresContext,
                                          nsIFrame*        aFrame,
                                          nsStyleContext*  aStyleContext,
                                          nsIView*         aView,
                                          PRUint32         aFlags)
{
  NS_ASSERTION(!aStyleContext || aFrame->GetStyleContext() == aStyleContext,
               "Wrong style context for frame?");

  if (!aView) {
    return;
  }

  nsIViewManager* vm = aView->GetViewManager();

  if (nsnull == aStyleContext) {
    aStyleContext = aFrame->GetStyleContext();
  }

  // Make sure visibility is correct. This only affects nsSubdocumentFrame.
  if (0 == (aFlags & NS_FRAME_NO_VISIBILITY) &&
      !aFrame->SupportsVisibilityHidden()) {
    // See if the view should be hidden or visible
    vm->SetViewVisibility(aView,
        aStyleContext->GetStyleVisibility()->IsVisible()
            ? nsViewVisibility_kShow : nsViewVisibility_kHide);
  }

  // See if the frame is being relatively positioned or absolutely
  // positioned
  bool isPositioned = aStyleContext->GetStyleDisplay()->IsPositioned();

  PRInt32 zIndex = 0;
  bool    autoZIndex = false;

  if (!isPositioned) {
    autoZIndex = PR_TRUE;
  } else {
    // Make sure z-index is correct
    const nsStylePosition* position = aStyleContext->GetStylePosition();

    if (position->mZIndex.GetUnit() == eStyleUnit_Integer) {
      zIndex = position->mZIndex.GetIntValue();
    } else if (position->mZIndex.GetUnit() == eStyleUnit_Auto) {
      autoZIndex = PR_TRUE;
    }
  }

  vm->SetViewZIndex(aView, autoZIndex, zIndex, isPositioned);
}

static nscoord GetCoord(const nsStyleCoord& aCoord, nscoord aIfNotCoord)
{
  if (aCoord.ConvertsToLength()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
  }
  return aIfNotCoord;
}

void
nsContainerFrame::DoInlineIntrinsicWidth(nsRenderingContext *aRenderingContext,
                                         InlineIntrinsicWidthData *aData,
                                         nsLayoutUtils::IntrinsicWidthType aType)
{
  if (GetPrevInFlow())
    return; // Already added.

  NS_PRECONDITION(aType == nsLayoutUtils::MIN_WIDTH ||
                  aType == nsLayoutUtils::PREF_WIDTH, "bad type");

  mozilla::css::Side startSide, endSide;
  if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR) {
    startSide = NS_SIDE_LEFT;
    endSide = NS_SIDE_RIGHT;
  } else {
    startSide = NS_SIDE_RIGHT;
    endSide = NS_SIDE_LEFT;
  }

  const nsStylePadding *stylePadding = GetStylePadding();
  const nsStyleBorder *styleBorder = GetStyleBorder();
  const nsStyleMargin *styleMargin = GetStyleMargin();

  // This goes at the beginning no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the startSide border is always on the
  // first line.
  // This frame is a first-in-flow, but it might have a previous bidi
  // continuation, in which case that continuation should handle the startSide
  // border.
  if (!GetPrevContinuation()) {
    aData->currentLine +=
      // clamp negative calc() to 0
      NS_MAX(GetCoord(stylePadding->mPadding.Get(startSide), 0), 0) +
      styleBorder->GetActualBorderWidth(startSide) +
      GetCoord(styleMargin->mMargin.Get(startSide), 0);
  }

  const nsLineList_iterator* savedLine = aData->line;
  nsIFrame* const savedLineContainer = aData->lineContainer;

  nsContainerFrame *lastInFlow;
  for (nsContainerFrame *nif = this; nif;
       nif = static_cast<nsContainerFrame*>(nif->GetNextInFlow())) {
    for (nsIFrame *kid = nif->mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {
      if (aType == nsLayoutUtils::MIN_WIDTH)
        kid->AddInlineMinWidth(aRenderingContext,
                               static_cast<InlineMinWidthData*>(aData));
      else
        kid->AddInlinePrefWidth(aRenderingContext,
                                static_cast<InlinePrefWidthData*>(aData));
    }

    // After we advance to our next-in-flow, the stored line and line container
    // may no longer be correct. Just forget them.
    aData->line = nsnull;
    aData->lineContainer = nsnull;

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
  if (!lastInFlow->GetNextContinuation()) {
    aData->currentLine +=
      // clamp negative calc() to 0
      NS_MAX(GetCoord(stylePadding->mPadding.Get(endSide), 0), 0) +
      styleBorder->GetActualBorderWidth(endSide) +
      GetCoord(styleMargin->mMargin.Get(endSide), 0);
  }
}

/* virtual */ nsSize
nsContainerFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                  nsSize aCBSize, nscoord aAvailableWidth,
                                  nsSize aMargin, nsSize aBorder,
                                  nsSize aPadding, bool aShrinkWrap)
{
  nsSize result(0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  nscoord availBased = aAvailableWidth - aMargin.width - aBorder.width -
                       aPadding.width;
  // replaced elements always shrink-wrap
  if (aShrinkWrap || IsFrameOfType(eReplaced)) {
    // don't bother setting it if the result won't be used
    if (GetStylePosition()->mWidth.GetUnit() == eStyleUnit_Auto) {
      result.width = ShrinkWidthToFit(aRenderingContext, availBased);
    }
  } else {
    result.width = availBased;
  }
  return result;
}

/**
 * Invokes the WillReflow() function, positions the frame and its view (if
 * requested), and then calls Reflow(). If the reflow succeeds and the child
 * frame is complete, deletes any next-in-flows using DeleteNextInFlowChild()
 */
nsresult
nsContainerFrame::ReflowChild(nsIFrame*                aKidFrame,
                              nsPresContext*           aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nscoord                  aX,
                              nscoord                  aY,
                              PRUint32                 aFlags,
                              nsReflowStatus&          aStatus,
                              nsOverflowContinuationTracker* aTracker)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");

  nsresult  result;

  // Send the WillReflow() notification, and position the child frame
  // and its view if requested
  aKidFrame->WillReflow(aPresContext);

  if (NS_FRAME_NO_MOVE_FRAME != (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    if ((aFlags & NS_FRAME_INVALIDATE_ON_MOVE) &&
        !(aKidFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
        aKidFrame->GetPosition() != nsPoint(aX, aY)) {
      aKidFrame->InvalidateFrameSubtree();
    }
    aKidFrame->SetPosition(nsPoint(aX, aY));
  }

  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aKidFrame);
  }

  // Reflow the child frame
  result = aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowState,
                             aStatus);

  // If the reflow was successful and the child frame is complete, delete any
  // next-in-flows
  if (NS_SUCCEEDED(result) && NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow = aKidFrame->GetNextInFlow();
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      if (aTracker) aTracker->Finish(aKidFrame);
      static_cast<nsContainerFrame*>(kidNextInFlow->GetParent())
        ->DeleteNextInFlowChild(aPresContext, kidNextInFlow, PR_TRUE);
    }
  }
  return result;
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
  // has a popupList and during layout will call nsMenuPopupFrame::AdjustView.
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
nsresult
nsContainerFrame::FinishReflowChild(nsIFrame*                  aKidFrame,
                                    nsPresContext*             aPresContext,
                                    const nsHTMLReflowState*   aReflowState,
                                    const nsHTMLReflowMetrics& aDesiredSize,
                                    nscoord                    aX,
                                    nscoord                    aY,
                                    PRUint32                   aFlags)
{
  nsPoint curOrigin = aKidFrame->GetPosition();
  nsRect  bounds(aX, aY, aDesiredSize.width, aDesiredSize.height);

  aKidFrame->SetRect(bounds);

  if (aKidFrame->HasView()) {
    nsIView* view = aKidFrame->GetView();
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

    // We also need to redraw everything associated with the frame
    // because if the frame's Reflow issued any invalidates, then they
    // will be at the wrong offset ... note that this includes
    // invalidates issued against the frame's children, so we need to
    // invalidate the overflow area too.
    aKidFrame->Invalidate(aDesiredSize.VisualOverflow());
  }

  return aKidFrame->DidReflow(aPresContext, aReflowState, NS_FRAME_REFLOW_FINISHED);
}

nsresult
nsContainerFrame::ReflowOverflowContainerChildren(nsPresContext*           aPresContext,
                                                  const nsHTMLReflowState& aReflowState,
                                                  nsOverflowAreas&         aOverflowRects,
                                                  PRUint32                 aFlags,
                                                  nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aPresContext, "null pointer");
  nsresult rv = NS_OK;

  nsFrameList* overflowContainers =
               GetPropTableFrames(aPresContext,
                                  OverflowContainersProperty());

  NS_ASSERTION(!(overflowContainers && GetPrevInFlow()
                 && static_cast<nsContainerFrame*>(GetPrevInFlow())
                      ->GetPropTableFrames(aPresContext,
                          ExcessOverflowContainersProperty())),
               "conflicting overflow containers lists");

  if (!overflowContainers) {
    // Drain excess from previnflow
    nsContainerFrame* prev = (nsContainerFrame*) GetPrevInFlow();
    if (prev) {
      nsFrameList* excessFrames =
        prev->RemovePropTableFrames(aPresContext,
                                    ExcessOverflowContainersProperty());
      if (excessFrames) {
        excessFrames->ApplySetParent(this);
        nsContainerFrame::ReparentFrameViewList(aPresContext, *excessFrames,
                                                prev, this);
        overflowContainers = excessFrames;
        rv = SetPropTableFrames(aPresContext, overflowContainers,
                                OverflowContainersProperty());
        if (NS_FAILED(rv)) {
          excessFrames->DestroyFrames();
          delete excessFrames;
          return rv;
        }
      }
    }
  }

  if (!overflowContainers)
    return NS_OK; // nothing to reflow

  nsOverflowContinuationTracker tracker(aPresContext, this, PR_FALSE, PR_FALSE);
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
      nsRect prevRect = prevInFlow->GetRect();

      // Initialize reflow params
      nsSize availSpace(prevRect.width, aReflowState.availableHeight);
      nsHTMLReflowMetrics desiredSize;
      nsHTMLReflowState frameState(aPresContext, aReflowState,
                                   frame, availSpace);
      nsReflowStatus frameStatus = NS_FRAME_COMPLETE;

      // Cache old bounds
      nsRect oldRect = frame->GetRect();
      nsRect oldOverflow = frame->GetVisualOverflowRect();

      // Reflow
      rv = ReflowChild(frame, aPresContext, desiredSize, frameState,
                       prevRect.x, 0, aFlags, frameStatus, &tracker);
      NS_ENSURE_SUCCESS(rv, rv);
      //XXXfr Do we need to override any shrinkwrap effects here?
      // e.g. desiredSize.width = prevRect.width;
      rv = FinishReflowChild(frame, aPresContext, &frameState, desiredSize,
                             prevRect.x, 0, aFlags);
      NS_ENSURE_SUCCESS(rv, rv);

      // Invalidate if there was a position or size change
      nsRect rect = frame->GetRect();
      if (!rect.IsEqualInterior(oldRect)) {
        nsRect dirtyRect = oldOverflow;
        dirtyRect.MoveBy(oldRect.x, oldRect.y);
        Invalidate(dirtyRect);

        dirtyRect = frame->GetVisualOverflowRect();
        dirtyRect.MoveBy(rect.x, rect.y);
        Invalidate(dirtyRect);
      }

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
          rv = aPresContext->PresShell()->FrameConstructor()->
                 CreateContinuingFrame(aPresContext, frame, this, &nif);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        else if (!(nif->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          // used to be a normal next-in-flow; steal it from the child list
          rv = static_cast<nsContainerFrame*>(nif->GetParent())
                 ->StealFrame(aPresContext, nif);
          NS_ENSURE_SUCCESS(rv, rv);
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
      if (aReflowState.mFloatManager)
        nsBlockFrame::RecoverFloatsFor(frame, *aReflowState.mFloatManager);
    }
    ConsiderChildOverflow(aOverflowRects, frame);
  }

  return NS_OK;
}

void
nsContainerFrame::DisplayOverflowContainers(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsFrameList* overflowconts =
    GetPropTableFrames(PresContext(), OverflowContainersProperty());
  if (overflowconts) {
    for (nsIFrame* frame = overflowconts->FirstChild(); frame;
         frame = frame->GetNextSibling()) {
      BuildDisplayListForChild(aBuilder, frame, aDirtyRect, aLists);
    }
  }
}

nsresult
nsContainerFrame::StealFrame(nsPresContext* aPresContext,
                             nsIFrame*      aChild,
                             bool           aForceNormal)
{
  bool removed = true;
  if ((aChild->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)
      && !aForceNormal) {
    // Try removing from the overflow container list
    if (!RemovePropTableFrame(aPresContext, aChild,
                              OverflowContainersProperty())) {
      // It must be in the excess overflow container list
      removed = RemovePropTableFrame(aPresContext, aChild,
                                     ExcessOverflowContainersProperty());
    }
  }
  else {
    if (!mFrames.RemoveFrameIfPresent(aChild)) {
      removed = PR_FALSE;
      // We didn't find the child in the parent's principal child list.
      // Maybe it's on the overflow list?
      nsFrameList* frameList = GetOverflowFrames();
      if (frameList) {
        removed = frameList->RemoveFrameIfPresent(aChild);
        if (frameList->IsEmpty()) {
          DestroyOverflowList(aPresContext, nsnull);
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

void
nsContainerFrame::DestroyOverflowList(nsPresContext* aPresContext,
                                      nsIFrame*      aDestructRoot)
{
  nsFrameList* list =
    RemovePropTableFrames(aPresContext, OverflowProperty());
  if (list) {
    if (aDestructRoot)
      list->DestroyFrom(aDestructRoot);
    else
      list->Destroy();
  }
}

/**
 * Remove and delete aNextInFlow and its next-in-flows. Updates the sibling and flow
 * pointers
 */
void
nsContainerFrame::DeleteNextInFlowChild(nsPresContext* aPresContext,
                                        nsIFrame*      aNextInFlow,
                                        bool           aDeletingEmptyFrames)
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
    for (PRInt32 i = frames.Length() - 1; i >= 0; --i) {
      nsIFrame* delFrame = frames.ElementAt(i);
      static_cast<nsContainerFrame*>(delFrame->GetParent())
        ->DeleteNextInFlowChild(aPresContext, delFrame, aDeletingEmptyFrames);
    }
  }

  aNextInFlow->InvalidateFrameSubtree();

  // Take the next-in-flow out of the parent's child list
#ifdef DEBUG
  nsresult rv =
#endif
    StealFrame(aPresContext, aNextInFlow);
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
nsContainerFrame::SetOverflowFrames(nsPresContext* aPresContext,
                                    const nsFrameList& aOverflowFrames)
{
  NS_PRECONDITION(aOverflowFrames.NotEmpty(), "Shouldn't be called");
  nsFrameList* newList = new nsFrameList(aOverflowFrames);

  aPresContext->PropertyTable()->Set(this, OverflowProperty(), newList);
}

nsFrameList*
nsContainerFrame::GetPropTableFrames(nsPresContext*                 aPresContext,
                                     const FramePropertyDescriptor* aProperty) const
{
  FramePropertyTable* propTable = aPresContext->PropertyTable();
  return static_cast<nsFrameList*>(propTable->Get(this, aProperty));
}

nsFrameList*
nsContainerFrame::RemovePropTableFrames(nsPresContext*                 aPresContext,
                                        const FramePropertyDescriptor* aProperty)
{
  FramePropertyTable* propTable = aPresContext->PropertyTable();
  return static_cast<nsFrameList*>(propTable->Remove(this, aProperty));
}

bool
nsContainerFrame::RemovePropTableFrame(nsPresContext*                 aPresContext,
                                       nsIFrame*                      aFrame,
                                       const FramePropertyDescriptor* aProperty)
{
  nsFrameList* frameList = RemovePropTableFrames(aPresContext, aProperty);
  if (!frameList) {
    // No such list
    return PR_FALSE;
  }
  if (!frameList->RemoveFrameIfPresent(aFrame)) {
    // Found list, but it doesn't have the frame. Put list back.
    SetPropTableFrames(aPresContext, frameList, aProperty);
    return PR_FALSE;
  }

  if (frameList->IsEmpty()) {
    // Removed frame and now list is empty. Delete it.
    delete frameList;
  }
  else {
    // Removed frame, but list not empty. Put it back.
    SetPropTableFrames(aPresContext, frameList, aProperty);
  }
  return PR_TRUE;
}

nsresult
nsContainerFrame::SetPropTableFrames(nsPresContext*                 aPresContext,
                                     nsFrameList*                   aFrameList,
                                     const FramePropertyDescriptor* aProperty)
{
  NS_PRECONDITION(aPresContext && aProperty && aFrameList, "null ptr");
  NS_PRECONDITION(
    (aProperty != nsContainerFrame::OverflowContainersProperty() &&
     aProperty != nsContainerFrame::ExcessOverflowContainersProperty()) ||
    IsFrameOfType(nsIFrame::eCanContainOverflowContainers),
    "this type of frame can't have overflow containers");
  aPresContext->PropertyTable()->Set(this, aProperty, aFrameList);
  return NS_OK;
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
nsContainerFrame::PushChildren(nsPresContext* aPresContext,
                               nsIFrame*       aFromChild,
                               nsIFrame*       aPrevSibling)
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
      nsContainerFrame::ReparentFrameView(aPresContext, f, this, nextInFlow);
    }
    nextInFlow->mFrames.InsertFrames(nextInFlow, nsnull, tail);
  }
  else {
    // Add the frames to our overflow list
    SetOverflowFrames(aPresContext, tail);
  }
}

/**
 * Moves any frames on the overflow lists (the prev-in-flow's overflow list and
 * the receiver's overflow list) to the child list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @return  PR_TRUE if any frames were moved and PR_FALSE otherwise
 */
bool
nsContainerFrame::MoveOverflowToChildList(nsPresContext* aPresContext)
{
  bool result = false;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)GetPrevInFlow();
  if (nsnull != prevInFlow) {
    nsAutoPtr<nsFrameList> prevOverflowFrames(prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Tables are special; they can have repeated header/footer
      // frames on mFrames at this point.
      NS_ASSERTION(mFrames.IsEmpty() || GetType() == nsGkAtoms::tableFrame,
                   "bad overflow list");
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(aPresContext,
                                              *prevOverflowFrames,
                                              prevInFlow, this);
      mFrames.AppendFrames(this, *prevOverflowFrames);
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsAutoPtr<nsFrameList> overflowFrames(StealOverflowFrames());
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nsnull, *overflowFrames);
    result = PR_TRUE;
  }
  return result;
}

nsOverflowContinuationTracker::nsOverflowContinuationTracker(nsPresContext*    aPresContext,
                                                             nsContainerFrame* aFrame,
                                                             bool              aWalkOOFFrames,
                                                             bool              aSkipOverflowContainerChildren)
  : mOverflowContList(nsnull),
    mPrevOverflowCont(nsnull),
    mSentry(nsnull),
    mParent(aFrame),
    mSkipOverflowContainerChildren(aSkipOverflowContainerChildren),
    mWalkOOFFrames(aWalkOOFFrames)
{
  NS_PRECONDITION(aFrame, "null frame pointer");
  nsContainerFrame* next = static_cast<nsContainerFrame*>
                             (aFrame->GetNextInFlow());
  if (next) {
    mOverflowContList = next->GetPropTableFrames(aPresContext,
      nsContainerFrame::OverflowContainersProperty());
    if (mOverflowContList) {
      mParent = next;
      SetUpListWalker();
    }
  }
  if (!mOverflowContList) {
    mOverflowContList = mParent->GetPropTableFrames(aPresContext,
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
            : nsnull;
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
  bool convertedToOverflowContainer = false;
  nsPresContext* presContext = aOverflowCont->PresContext();
  if (!mSentry || aOverflowCont != mSentry->GetNextInFlow()) {
    // Not in our list, so we need to add it
    if (aOverflowCont->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
      // aOverflowCont is in some other overflow container list,
      // steal it first
      NS_ASSERTION(!(mOverflowContList &&
                     mOverflowContList->ContainsFrame(aOverflowCont)),
                   "overflow containers out of order");
      rv = static_cast<nsContainerFrame*>(aOverflowCont->GetParent())
             ->StealFrame(presContext, aOverflowCont);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      aOverflowCont->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      convertedToOverflowContainer = PR_TRUE;
    }
    if (!mOverflowContList) {
      mOverflowContList = new nsFrameList();
      rv = mParent->SetPropTableFrames(presContext, mOverflowContList,
        nsContainerFrame::ExcessOverflowContainersProperty());
      NS_ENSURE_SUCCESS(rv, rv);
      SetUpListWalker();
    }
    if (aOverflowCont->GetParent() != mParent) {
      nsContainerFrame::ReparentFrameView(presContext, aOverflowCont,
                                          aOverflowCont->GetParent(),
                                          mParent);
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

  if (convertedToOverflowContainer) {
    // Convert all non-overflow-container continuations of aOverflowCont
    // into overflow containers and move them to our overflow
    // tracker. This preserves the invariant that the next-continuations
    // of an overflow container are also overflow containers.
    nsIFrame* f = aOverflowCont->GetNextContinuation();
    if (f && !(f->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
      nsContainerFrame* parent = static_cast<nsContainerFrame*>(f->GetParent());
      rv = parent->StealFrame(presContext, f);
      NS_ENSURE_SUCCESS(rv, rv);
      Insert(f, aReflowStatus);
    }
  }
  return rv;
}

void
nsOverflowContinuationTracker::Finish(nsIFrame* aChild)
{
  NS_PRECONDITION(aChild, "null ptr");
  NS_PRECONDITION(aChild->GetNextInFlow(),
                "supposed to call Finish *before* deleting next-in-flow!");

  for (nsIFrame* f = aChild; f; f = f->GetNextInFlow()) {
    // Make sure we drop all references if the only frame
    // in the overflow containers list is about to be destroyed
    if (mOverflowContList &&
        mOverflowContList->FirstChild() == f->GetNextInFlow() &&
        !f->GetNextInFlow()->GetNextSibling()) {
      mOverflowContList = nsnull;
      mPrevOverflowCont = nsnull;
      mSentry = nsnull;
      mParent = static_cast<nsContainerFrame*>(f->GetParent());
      break;
    }
    if (f == mSentry) {
      // Step past aChild
      nsIFrame* prevOverflowCont = mPrevOverflowCont;
      StepForward();
      if (mPrevOverflowCont == f->GetNextInFlow()) {
        // Pull mPrevOverflowChild back to aChild's prevSibling:
        // aChild will be removed from our list by our caller
        mPrevOverflowCont = prevOverflowCont;
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

#ifdef NS_DEBUG
NS_IMETHODIMP
nsContainerFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", static_cast<void*>(mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", static_cast<void*>(GetView()));
  }
  if (GetNextSibling()) {
    fprintf(out, " next=%p", static_cast<void*>(GetNextSibling()));
  }
  if (nsnull != GetPrevContinuation()) {
    fprintf(out, " prev-continuation=%p", static_cast<void*>(GetPrevContinuation()));
  }
  if (nsnull != GetNextContinuation()) {
    fprintf(out, " next-continuation=%p", static_cast<void*>(GetNextContinuation()));
  }
  void* IBsibling = Properties().Get(IBSplitSpecialSibling());
  if (IBsibling) {
    fprintf(out, " IBSplitSpecialSibling=%p", IBsibling);
  }
  void* IBprevsibling = Properties().Get(IBSplitSpecialPrevSibling());
  if (IBprevsibling) {
    fprintf(out, " IBSplitSpecialPrevSibling=%p", IBprevsibling);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%016llx]", mState);
  }
  fprintf(out, " [content=%p]", static_cast<void*>(mContent));
  nsContainerFrame* f = const_cast<nsContainerFrame*>(this);
  if (f->HasOverflowAreas()) {
    nsRect overflowArea = f->GetVisualOverflowRect();
    fprintf(out, " [vis-overflow=%d,%d,%d,%d]", overflowArea.x, overflowArea.y,
            overflowArea.width, overflowArea.height);
    overflowArea = f->GetScrollableOverflowRect();
    fprintf(out, " [scr-overflow=%d,%d,%d,%d]", overflowArea.x, overflowArea.y,
            overflowArea.width, overflowArea.height);
  }
  fprintf(out, " [sc=%p]", static_cast<void*>(mStyleContext));
  nsIAtom* pseudoTag = mStyleContext->GetPseudo();
  if (pseudoTag) {
    nsAutoString atomString;
    pseudoTag->ToString(atomString);
    fprintf(out, " pst=%s",
            NS_LossyConvertUTF16toASCII(atomString).get());
  }

  // Output the children
  bool outputOneList = false;
  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    if (outputOneList) {
      IndentBy(out, aIndent);
    }
    outputOneList = PR_TRUE;
    fputs(mozilla::layout::ChildListName(lists.CurrentID()), out);
    fputs("<\n", out);
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* kid = childFrames.get();
      // Verify the child frame's parent frame pointer is correct
      NS_ASSERTION(kid->GetParent() == this, "bad parent frame pointer");

      // Have the child frame list
      kid->List(out, aIndent + 1);
    }
    IndentBy(out, aIndent);
    fputs(">\n", out);
  }

  if (!outputOneList) {
    fputs("<>\n", out);
  }

  return NS_OK;
}
#endif
