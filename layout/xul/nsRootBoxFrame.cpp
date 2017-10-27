/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLParts.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsBoxFrame.h"
#include "nsStackLayout.h"
#include "nsIRootBox.h"
#include "nsIContent.h"
#include "nsXULTooltipListener.h"
#include "nsFrameManager.h"
#include "mozilla/BasicEvents.h"

using namespace mozilla;

// Interface IDs

//#define DEBUG_REFLOW

// static
nsIRootBox*
nsIRootBox::GetRootBox(nsIPresShell* aShell)
{
  if (!aShell) {
    return nullptr;
  }
  nsIFrame* rootFrame = aShell->FrameManager()->GetRootFrame();
  if (!rootFrame) {
    return nullptr;
  }

  if (rootFrame) {
    rootFrame = rootFrame->PrincipalChildList().FirstChild();
  }

  nsIRootBox* rootBox = do_QueryFrame(rootFrame);
  return rootBox;
}

class nsRootBoxFrame final : public nsBoxFrame, public nsIRootBox
{
public:

  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  explicit nsRootBoxFrame(nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsRootBoxFrame)

  virtual nsPopupSetFrame* GetPopupSetFrame() override;
  virtual void SetPopupSetFrame(nsPopupSetFrame* aPopupSet) override;
  virtual nsIContent* GetDefaultTooltip() override;
  virtual void SetDefaultTooltip(nsIContent* aTooltip) override;
  virtual nsresult AddTooltipSupport(nsIContent* aNode) override;
  virtual nsresult RemoveTooltipSupport(nsIContent* aNode) override;

  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual void Reflow(nsPresContext*          aPresContext,
                          ReflowOutput&     aDesiredSize,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus&          aStatus) override;
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  nsPopupSetFrame* mPopupSetFrame;

protected:
  nsIContent* mDefaultTooltip;
};

//----------------------------------------------------------------------

nsContainerFrame*
NS_NewRootBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsRootBoxFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsRootBoxFrame)

nsRootBoxFrame::nsRootBoxFrame(nsStyleContext* aContext)
  : nsBoxFrame(aContext, kClassID, true)
  , mPopupSetFrame(nullptr)
  , mDefaultTooltip(nullptr)
{
  nsCOMPtr<nsBoxLayout> layout;
  NS_NewStackLayout(layout);
  SetXULLayoutManager(layout);
}

void
nsRootBoxFrame::AppendFrames(ChildListID     aListID,
                             nsFrameList&    aFrameList)
{
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list ID");
  MOZ_ASSERT(mFrames.IsEmpty(), "already have a child frame");
  nsBoxFrame::AppendFrames(aListID, aFrameList);
}

void
nsRootBoxFrame::InsertFrames(ChildListID     aListID,
                             nsIFrame*       aPrevFrame,
                             nsFrameList&    aFrameList)
{
  // Because we only support a single child frame inserting is the same
  // as appending.
  MOZ_ASSERT(!aPrevFrame, "unexpected previous sibling frame");
  AppendFrames(aListID, aFrameList);
}

void
nsRootBoxFrame::RemoveFrame(ChildListID     aListID,
                            nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list ID");
  if (aOldFrame == mFrames.FirstChild()) {
    nsBoxFrame::RemoveFrame(aListID, aOldFrame);
  } else {
    MOZ_CRASH("unknown aOldFrame");
  }
}

#ifdef DEBUG_REFLOW
int32_t gReflows = 0;
#endif

void
nsRootBoxFrame::Reflow(nsPresContext*           aPresContext,
                       ReflowOutput&     aDesiredSize,
                       const ReflowInput& aReflowInput,
                       nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRootBoxFrame");
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

#ifdef DEBUG_REFLOW
  gReflows++;
  printf("----Reflow %d----\n", gReflows);
#endif
  return nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);
}

void
nsRootBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                 const nsDisplayListSet& aLists)
{
  if (mContent && mContent->GetProperty(nsGkAtoms::DisplayPortMargins)) {
    // The XUL document's root element may have displayport margins set in
    // ChromeProcessController::InitializeRoot, and we should to supply the
    // base rect.
    nsRect displayPortBase =
      aBuilder->GetVisibleRect().Intersect(nsRect(nsPoint(0, 0), GetSize()));
    nsLayoutUtils::SetDisplayPortBase(mContent, displayPortBase);
  }

  // root boxes don't need a debug border/outline or a selection overlay...
  // They *may* have a background propagated to them, so force creation
  // of a background display list element.
  DisplayBorderBackgroundOutline(aBuilder, aLists, true);

  BuildDisplayListForChildren(aBuilder, aLists);
}

nsresult
nsRootBoxFrame::HandleEvent(nsPresContext* aPresContext,
                            WidgetGUIEvent* aEvent,
                            nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  if (aEvent->mMessage == eMouseUp) {
    nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

nsPopupSetFrame*
nsRootBoxFrame::GetPopupSetFrame()
{
  return mPopupSetFrame;
}

void
nsRootBoxFrame::SetPopupSetFrame(nsPopupSetFrame* aPopupSet)
{
  // Under normal conditions this should only be called once.  However,
  // if something triggers ReconstructDocElementHierarchy, we will
  // destroy this frame's child (the nsDocElementBoxFrame), but not this
  // frame.  This will cause the popupset to remove itself by calling
  // |SetPopupSetFrame(nullptr)|, and then we'll be able to accept a new
  // popupset.  Since the anonymous content is associated with the
  // nsDocElementBoxFrame, we'll get a new popupset when the new doc
  // element box frame is created.
  if (!mPopupSetFrame || !aPopupSet) {
    mPopupSetFrame = aPopupSet;
  } else {
    NS_NOTREACHED("Popup set is already defined! Only 1 allowed.");
  }
}

nsIContent*
nsRootBoxFrame::GetDefaultTooltip()
{
  return mDefaultTooltip;
}

void
nsRootBoxFrame::SetDefaultTooltip(nsIContent* aTooltip)
{
  mDefaultTooltip = aTooltip;
}

nsresult
nsRootBoxFrame::AddTooltipSupport(nsIContent* aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsXULTooltipListener *listener = nsXULTooltipListener::GetInstance();
  if (!listener)
    return NS_ERROR_OUT_OF_MEMORY;

  return listener->AddTooltipSupport(aNode);
}

nsresult
nsRootBoxFrame::RemoveTooltipSupport(nsIContent* aNode)
{
  // XXjh yuck, I'll have to implement a way to get at
  // the tooltip listener for a given node to make
  // this work.  Not crucial, we aren't removing
  // tooltips from any nodes in the app just yet.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_QUERYFRAME_HEAD(nsRootBoxFrame)
  NS_QUERYFRAME_ENTRY(nsIRootBox)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRootBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RootBox"), aResult);
}
#endif
