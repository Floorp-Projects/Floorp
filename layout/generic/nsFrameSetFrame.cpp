/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for HTML <frameset> elements */

#include "nsFrameSetFrame.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/Likely.h"

#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsLeafFrame.h"
#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsStyleContext.h"
#include "nsHTMLParts.h"
#include "nsRenderingContext.h"
#include "nsIDOMMutationEvent.h"
#include "nsNameSpaceManager.h"
#include "nsCSSAnonBoxes.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "mozilla/dom/Element.h"
#include "nsDisplayList.h"
#include "nsNodeUtils.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/HTMLFrameSetElement.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "nsSubDocumentFrame.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

// masks for mEdgeVisibility
#define LEFT_VIS   0x0001
#define RIGHT_VIS  0x0002
#define TOP_VIS    0x0004
#define BOTTOM_VIS 0x0008
#define ALL_VIS    0x000F
#define NONE_VIS   0x0000

/*******************************************************************************
 * nsFramesetDrag
 ******************************************************************************/
nsFramesetDrag::nsFramesetDrag()
{
  UnSet();
}

void nsFramesetDrag::Reset(bool                 aVertical,
                           int32_t              aIndex,
                           int32_t              aChange,
                           nsHTMLFramesetFrame* aSource)
{
  mVertical = aVertical;
  mIndex    = aIndex;
  mChange   = aChange;
  mSource   = aSource;
}

void nsFramesetDrag::UnSet()
{
  mVertical = true;
  mIndex    = -1;
  mChange   = 0;
  mSource   = nullptr;
}

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
class nsHTMLFramesetBorderFrame : public nsLeafFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual nsresult GetCursor(const nsPoint&    aPoint,
                             nsIFrame::Cursor& aCursor) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) override;

  bool GetVisibility() { return mVisibility; }
  void SetVisibility(bool aVisibility);
  void SetColor(nscolor aColor);

  void PaintBorder(DrawTarget* aDrawTarget, nsPoint aPt);

protected:
  nsHTMLFramesetBorderFrame(nsStyleContext* aContext, int32_t aWidth, bool aVertical, bool aVisible);
  virtual ~nsHTMLFramesetBorderFrame();
  virtual nscoord GetIntrinsicISize() override;
  virtual nscoord GetIntrinsicBSize() override;

  // the prev and next neighbors are indexes into the row (for a horizontal border) or col (for
  // a vertical border) of nsHTMLFramesetFrames or nsHTMLFrames
  int32_t mPrevNeighbor;
  int32_t mNextNeighbor;
  nscolor mColor;
  int32_t mWidth;
  bool mVertical;
  bool mVisibility;
  bool mCanResize;
  friend class nsHTMLFramesetFrame;
};
/*******************************************************************************
 * nsHTMLFramesetBlankFrame
 ******************************************************************************/
class nsHTMLFramesetBlankFrame : public nsLeafFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsHTMLFramesetBlankFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("FramesetBlank"), aResult);
  }
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) override;

protected:
  explicit nsHTMLFramesetBlankFrame(nsStyleContext* aContext) : nsLeafFrame(aContext) {}
  virtual ~nsHTMLFramesetBlankFrame();
  virtual nscoord GetIntrinsicISize() override;
  virtual nscoord GetIntrinsicBSize() override;

  friend class nsHTMLFramesetFrame;
  friend class nsHTMLFrameset;
};

/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
bool    nsHTMLFramesetFrame::gDragInProgress = false;
#define DEFAULT_BORDER_WIDTH_PX 6

nsHTMLFramesetFrame::nsHTMLFramesetFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
{
  mNumRows             = 0;
  mNumCols             = 0;
  mEdgeVisibility      = 0;
  mParentFrameborder   = eFrameborder_Yes; // default
  mParentBorderWidth   = -1; // default not set
  mParentBorderColor   = NO_COLOR; // default not set
  mFirstDragPoint.x     = mFirstDragPoint.y = 0;
  mMinDrag             = nsPresContext::CSSPixelsToAppUnits(2);
  mNonBorderChildCount = 0;
  mNonBlankChildCount  = 0;
  mDragger             = nullptr;
  mChildCount          = 0;
  mTopLevelFrameset    = nullptr;
  mEdgeColors.Set(NO_COLOR);
}

nsHTMLFramesetFrame::~nsHTMLFramesetFrame()
{
}

NS_QUERYFRAME_HEAD(nsHTMLFramesetFrame)
  NS_QUERYFRAME_ENTRY(nsHTMLFramesetFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
nsHTMLFramesetFrame::Init(nsIContent*       aContent,
                          nsContainerFrame* aParent,
                          nsIFrame*         aPrevInFlow)
{
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
  // find the highest ancestor that is a frameset
  nsIFrame* parentFrame = GetParent();
  mTopLevelFrameset = this;
  while (parentFrame) {
    nsHTMLFramesetFrame* frameset = do_QueryFrame(parentFrame);
    if (frameset) {
      mTopLevelFrameset = frameset;
      parentFrame = parentFrame->GetParent();
    } else {
      break;
    }
  }

  nsPresContext* presContext = PresContext();
  nsIPresShell* shell = presContext->PresShell();

  nsFrameborder  frameborder = GetFrameBorder();
  int32_t borderWidth = GetBorderWidth(presContext, false);
  nscolor borderColor = GetBorderColor();

  // Get the rows= cols= data
  HTMLFrameSetElement* ourContent = HTMLFrameSetElement::FromContent(mContent);
  NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
  const nsFramesetSpec* rowSpecs = nullptr;
  const nsFramesetSpec* colSpecs = nullptr;
  // GetRowSpec and GetColSpec can fail, but when they do they set
  // mNumRows and mNumCols respectively to 0, so we deal with it fine.
  ourContent->GetRowSpec(&mNumRows, &rowSpecs);
  ourContent->GetColSpec(&mNumCols, &colSpecs);

  // Maximum value of mNumRows and mNumCols is NS_MAX_FRAMESET_SPEC_COUNT
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < UINT_MAX / sizeof(nscoord));
  mRowSizes  = MakeUnique<nscoord[]>(mNumRows);
  mColSizes  = MakeUnique<nscoord[]>(mNumCols);

  // Ensure we can't overflow numCells
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < INT32_MAX / NS_MAX_FRAMESET_SPEC_COUNT);
  int32_t numCells = mNumRows*mNumCols;

  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < UINT_MAX / sizeof(nsHTMLFramesetBorderFrame*));
  mVerBorders    = MakeUnique<nsHTMLFramesetBorderFrame*[]>(mNumCols);  // 1 more than number of ver borders

  for (int verX  = 0; verX < mNumCols; verX++)
    mVerBorders[verX]    = nullptr;

  mHorBorders    = MakeUnique<nsHTMLFramesetBorderFrame*[]>(mNumRows);  // 1 more than number of hor borders

  for (int horX = 0; horX < mNumRows; horX++)
    mHorBorders[horX]    = nullptr;

  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT
                   < UINT_MAX / sizeof(int32_t) / NS_MAX_FRAMESET_SPEC_COUNT);
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT
                   < UINT_MAX / sizeof(nsFrameborder) / NS_MAX_FRAMESET_SPEC_COUNT);
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT
                   < UINT_MAX / sizeof(nsBorderColor) / NS_MAX_FRAMESET_SPEC_COUNT);
  mChildFrameborder  = MakeUnique<nsFrameborder[]>(numCells);
  mChildBorderColors  = MakeUnique<nsBorderColor[]>(numCells);

  // create the children frames; skip content which isn't <frameset> or <frame>
  mChildCount = 0; // number of <frame> or <frameset> children
  nsIFrame* frame;

  // number of any type of children
  uint32_t numChildren = mContent->GetChildCount();

  for (uint32_t childX = 0; childX < numChildren; childX++) {
    if (mChildCount == numCells) { // we have more <frame> or <frameset> than cells
      // Clear the lazy bits in the remaining children.  Also clear
      // the restyle flags, like nsCSSFrameConstructor::ProcessChildren does.
      for (uint32_t i = childX; i < numChildren; i++) {
        nsIContent *child = mContent->GetChildAt(i);
        child->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME);
        if (child->IsElement()) {
          child->UnsetFlags(ELEMENT_ALL_RESTYLE_FLAGS);
        }
      }
      break;
    }
    nsIContent *child = mContent->GetChildAt(childX);
    child->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME);
    // Also clear the restyle flags in the child like
    // nsCSSFrameConstructor::ProcessChildren does.
    if (child->IsElement()) {
      child->UnsetFlags(ELEMENT_ALL_RESTYLE_FLAGS);
    }

    // IMPORTANT: This must match the conditions in
    // nsCSSFrameConstructor::ContentAppended/Inserted/Removed
    if (!child->IsHTMLElement())
      continue;

    if (child->IsAnyOfHTMLElements(nsGkAtoms::frameset, nsGkAtoms::frame)) {
      RefPtr<nsStyleContext> kidSC;

      kidSC = shell->StyleSet()->ResolveStyleFor(child->AsElement(),
                                                 mStyleContext);
      if (child->IsHTMLElement(nsGkAtoms::frameset)) {
        frame = NS_NewHTMLFramesetFrame(shell, kidSC);

        nsHTMLFramesetFrame* childFrame = (nsHTMLFramesetFrame*)frame;
        childFrame->SetParentFrameborder(frameborder);
        childFrame->SetParentBorderWidth(borderWidth);
        childFrame->SetParentBorderColor(borderColor);
        frame->Init(child, this, nullptr);

        mChildBorderColors[mChildCount].Set(childFrame->GetBorderColor());
      } else { // frame
        frame = NS_NewSubDocumentFrame(shell, kidSC);

        frame->Init(child, this, nullptr);

        mChildFrameborder[mChildCount] = GetFrameBorder(child);
        mChildBorderColors[mChildCount].Set(GetBorderColor(child));
      }
      child->SetPrimaryFrame(frame);

      mFrames.AppendFrame(nullptr, frame);

      mChildCount++;
    }
  }

  mNonBlankChildCount = mChildCount;
  // add blank frames for frameset cells that had no content provided
  for (int blankX = mChildCount; blankX < numCells; blankX++) {
    RefPtr<nsStyleContext> pseudoStyleContext;
    pseudoStyleContext = shell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::framesetBlank, mStyleContext);

    // XXX the blank frame is using the content of its parent - at some point it
    // should just have null content, if we support that
    nsHTMLFramesetBlankFrame* blankFrame = new (shell) nsHTMLFramesetBlankFrame(pseudoStyleContext);

    blankFrame->Init(mContent, this, nullptr);

    mFrames.AppendFrame(nullptr, blankFrame);

    mChildBorderColors[mChildCount].Set(NO_COLOR);
    mChildCount++;
  }

  mNonBorderChildCount = mChildCount;
}

void
nsHTMLFramesetFrame::SetInitialChildList(ChildListID  aListID,
                                         nsFrameList& aChildList)
{
  // We do this weirdness where we create our child frames in Init().  On the
  // other hand, we're going to get a SetInitialChildList() with an empty list
  // and null list name after the frame constructor is done creating us.  So
  // just ignore that call.
  if (aListID == kPrincipalList && aChildList.IsEmpty()) {
    return;
  }

  nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

// XXX should this try to allocate twips based on an even pixel boundary?
void nsHTMLFramesetFrame::Scale(nscoord  aDesired,
                                int32_t  aNumIndicies,
                                int32_t* aIndicies,
                                int32_t  aNumItems,
                                int32_t* aItems)
{
  int32_t actual = 0;
  int32_t i, j;
  // get the actual total
  for (i = 0; i < aNumIndicies; i++) {
    j = aIndicies[i];
    actual += aItems[j];
  }

  if (actual > 0) {
    float factor = (float)aDesired / (float)actual;
    actual = 0;
    // scale the items up or down
    for (i = 0; i < aNumIndicies; i++) {
      j = aIndicies[i];
      aItems[j] = NSToCoordRound((float)aItems[j] * factor);
      actual += aItems[j];
    }
  } else if (aNumIndicies != 0) {
    // All the specs say zero width, but we have to fill up space
    // somehow.  Distribute it equally.
    nscoord width = NSToCoordRound((float)aDesired / (float)aNumIndicies);
    actual = width * aNumIndicies;
    for (i = 0; i < aNumIndicies; i++) {
      aItems[aIndicies[i]] = width;
    }
  }

  if (aNumIndicies > 0 && aDesired != actual) {
    int32_t unit = (aDesired > actual) ? 1 : -1;
    for (i=0; (i < aNumIndicies) && (aDesired != actual); i++) {
      j = aIndicies[i];
      if (j < aNumItems) {
        aItems[j] += unit;
        actual += unit;
      }
    }
  }
}


/**
  * Translate the rows/cols specs into an array of integer sizes for
  * each cell in the frameset. Sizes are allocated based on the priorities of the
  * specifier - fixed sizes have the highest priority, percentage sizes have the next
  * highest priority and relative sizes have the lowest.
  */
void nsHTMLFramesetFrame::CalculateRowCol(nsPresContext*        aPresContext,
                                          nscoord               aSize,
                                          int32_t               aNumSpecs,
                                          const nsFramesetSpec* aSpecs,
                                          nscoord*              aValues)
{
  // aNumSpecs maximum value is NS_MAX_FRAMESET_SPEC_COUNT
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < UINT_MAX / sizeof(int32_t));

  int32_t  fixedTotal = 0;
  int32_t  numFixed = 0;
  auto fixed = MakeUnique<int32_t[]>(aNumSpecs);
  int32_t  numPercent = 0;
  auto percent = MakeUnique<int32_t[]>(aNumSpecs);
  int32_t  relativeSums = 0;
  int32_t  numRelative = 0;
  auto relative = MakeUnique<int32_t[]>(aNumSpecs);

  if (MOZ_UNLIKELY(!fixed || !percent || !relative)) {
    return; // NS_ERROR_OUT_OF_MEMORY
  }

  int32_t i, j;

  // initialize the fixed, percent, relative indices, allocate the fixed sizes and zero the others
  for (i = 0; i < aNumSpecs; i++) {
    aValues[i] = 0;
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aValues[i] = nsPresContext::CSSPixelsToAppUnits(aSpecs[i].mValue);
        fixedTotal += aValues[i];
        fixed[numFixed] = i;
        numFixed++;
        break;
      case eFramesetUnit_Percent:
        percent[numPercent] = i;
        numPercent++;
        break;
      case eFramesetUnit_Relative:
        relative[numRelative] = i;
        numRelative++;
        relativeSums += aSpecs[i].mValue;
        break;
    }
  }

  // scale the fixed sizes if they total too much (or too little and there aren't any percent or relative)
  if ((fixedTotal > aSize) || ((fixedTotal < aSize) && (0 == numPercent) && (0 == numRelative))) {
    Scale(aSize, numFixed, fixed.get(), aNumSpecs, aValues);
    return;
  }

  int32_t percentMax = aSize - fixedTotal;
  int32_t percentTotal = 0;
  // allocate the percentage sizes from what is left over from the fixed allocation
  for (i = 0; i < numPercent; i++) {
    j = percent[i];
    aValues[j] = NSToCoordRound((float)aSpecs[j].mValue * (float)aSize / 100.0f);
    percentTotal += aValues[j];
  }

  // scale the percent sizes if they total too much (or too little and there aren't any relative)
  if ((percentTotal > percentMax) || ((percentTotal < percentMax) && (0 == numRelative))) {
    Scale(percentMax, numPercent, percent.get(), aNumSpecs, aValues);
    return;
  }

  int32_t relativeMax = percentMax - percentTotal;
  int32_t relativeTotal = 0;
  // allocate the relative sizes from what is left over from the percent allocation
  for (i = 0; i < numRelative; i++) {
    j = relative[i];
    aValues[j] = NSToCoordRound((float)aSpecs[j].mValue * (float)relativeMax / (float)relativeSums);
    relativeTotal += aValues[j];
  }

  // scale the relative sizes if they take up too much or too little
  if (relativeTotal != relativeMax) {
    Scale(relativeMax, numRelative, relative.get(), aNumSpecs, aValues);
  }
}


/**
  * Translate the rows/cols integer sizes into an array of specs for
  * each cell in the frameset.  Reverse of CalculateRowCol() behaviour.
  * This allows us to maintain the user size info through reflows.
  */
void nsHTMLFramesetFrame::GenerateRowCol(nsPresContext*        aPresContext,
                                         nscoord               aSize,
                                         int32_t               aNumSpecs,
                                         const nsFramesetSpec* aSpecs,
                                         nscoord*              aValues,
                                         nsString&             aNewAttr)
{
  int32_t i;

  for (i = 0; i < aNumSpecs; i++) {
    if (!aNewAttr.IsEmpty())
      aNewAttr.Append(char16_t(','));

    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aNewAttr.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(aValues[i]));
        break;
      case eFramesetUnit_Percent: // XXX Only accurate to 1%, need 1 pixel
      case eFramesetUnit_Relative:
        // Add 0.5 to the percentage to make rounding work right.
        aNewAttr.AppendInt(uint32_t((100.0*aValues[i])/aSize + 0.5));
        aNewAttr.Append(char16_t('%'));
        break;
    }
  }
}

int32_t nsHTMLFramesetFrame::GetBorderWidth(nsPresContext* aPresContext,
                                            bool aTakeForcingIntoAccount)
{
  nsFrameborder frameborder = GetFrameBorder();
  if (frameborder == eFrameborder_No) {
    return 0;
  }
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::border);
    if (attr) {
      int32_t intVal = 0;
      if (attr->Type() == nsAttrValue::eInteger) {
        intVal = attr->GetIntegerValue();
        if (intVal < 0) {
          intVal = 0;
        }
      }

      return nsPresContext::CSSPixelsToAppUnits(intVal);
    }
  }

  if (mParentBorderWidth >= 0) {
    return mParentBorderWidth;
  }

  return nsPresContext::CSSPixelsToAppUnits(DEFAULT_BORDER_WIDTH_PX);
}

void
nsHTMLFramesetFrame::GetDesiredSize(nsPresContext*           aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics&     aDesiredSize)
{
  WritingMode wm = aReflowState.GetWritingMode();
  LogicalSize desiredSize(wm);
  nsHTMLFramesetFrame* framesetParent = do_QueryFrame(GetParent());
  if (nullptr == framesetParent) {
    if (aPresContext->IsPaginated()) {
      // XXX This needs to be changed when framesets paginate properly
      desiredSize.ISize(wm) = aReflowState.AvailableISize();
      desiredSize.BSize(wm) = aReflowState.AvailableBSize();
    } else {
      LogicalSize area(wm, aPresContext->GetVisibleArea().Size());

      desiredSize.ISize(wm) = area.ISize(wm);
      desiredSize.BSize(wm) = area.BSize(wm);
    }
  } else {
    LogicalSize size(wm);
    framesetParent->GetSizeOfChild(this, wm, size);
    desiredSize.ISize(wm) = size.ISize(wm);
    desiredSize.BSize(wm) = size.BSize(wm);
  }
  aDesiredSize.SetSize(wm, desiredSize);
}

// only valid for non border children
void nsHTMLFramesetFrame::GetSizeOfChildAt(int32_t  aIndexInParent,
                                           WritingMode aWM,
                                           LogicalSize&  aSize,
                                           nsIntPoint& aCellIndex)
{
  int32_t row = aIndexInParent / mNumCols;
  int32_t col = aIndexInParent - (row * mNumCols); // remainder from dividing index by mNumCols
  if ((row < mNumRows) && (col < mNumCols)) {
    aSize.ISize(aWM) = mColSizes[col];
    aSize.BSize(aWM) = mRowSizes[row];
    aCellIndex.x = col;
    aCellIndex.y = row;
  } else {
    aSize.SizeTo(aWM, 0, 0);
    aCellIndex.x = aCellIndex.y = 0;
  }
}

// only valid for non border children
void nsHTMLFramesetFrame::GetSizeOfChild(nsIFrame* aChild,
                                         WritingMode aWM,
                                         LogicalSize& aSize)
{
  // Reflow only creates children frames for <frameset> and <frame> content.
  // this assumption is used here
  int i = 0;
  for (nsIFrame* child : mFrames) {
    if (aChild == child) {
      nsIntPoint ignore;
      GetSizeOfChildAt(i, aWM, aSize, ignore);
      return;
    }
    i++;
  }
  aSize.SizeTo(aWM, 0, 0);
}


nsresult nsHTMLFramesetFrame::HandleEvent(nsPresContext* aPresContext,
                                           WidgetGUIEvent* aEvent,
                                           nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (mDragger) {
    // the nsFramesetBorderFrame has captured NS_MOUSE_DOWN
    switch (aEvent->mMessage) {
      case eMouseMove:
        MouseDrag(aPresContext, aEvent);
	      break;
      case eMouseUp:
        if (aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
          EndMouseDrag(aPresContext);
        }
	      break;
      default:
        break;
    }
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  } else {
    *aEventStatus = nsEventStatus_eIgnore;
  }
  return NS_OK;
}

nsresult
nsHTMLFramesetFrame::GetCursor(const nsPoint&    aPoint,
                               nsIFrame::Cursor& aCursor)
{
  if (mDragger) {
    aCursor.mCursor = (mDragger->mVertical) ? NS_STYLE_CURSOR_EW_RESIZE : NS_STYLE_CURSOR_NS_RESIZE;
  } else {
    aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
  }
  return NS_OK;
}

void
nsHTMLFramesetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                      const nsRect&           aDirtyRect,
                                      const nsDisplayListSet& aLists)
{
  BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);

  if (mDragger && aBuilder->IsForEventDelivery()) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayEventReceiver(aBuilder, this));
  }
}

void
nsHTMLFramesetFrame::ReflowPlaceChild(nsIFrame*                aChild,
                                      nsPresContext*           aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsPoint&                 aOffset,
                                      nsSize&                  aSize,
                                      nsIntPoint*              aCellIndex)
{
  // reflow the child
  nsHTMLReflowState reflowState(aPresContext, aReflowState, aChild,
                                LogicalSize(aChild->GetWritingMode(), aSize));
  reflowState.SetComputedWidth(std::max(0, aSize.width - reflowState.ComputedPhysicalBorderPadding().LeftRight()));
  reflowState.SetComputedHeight(std::max(0, aSize.height - reflowState.ComputedPhysicalBorderPadding().TopBottom()));
  nsHTMLReflowMetrics metrics(aReflowState);
  metrics.Width() = aSize.width;
  metrics.Height() = aSize.height;
  nsReflowStatus status;

  ReflowChild(aChild, aPresContext, metrics, reflowState, aOffset.x,
              aOffset.y, 0, status);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");

  // Place and size the child
  metrics.Width() = aSize.width;
  metrics.Height() = aSize.height;
  FinishReflowChild(aChild, aPresContext, metrics, nullptr, aOffset.x, aOffset.y, 0);
}

static
nsFrameborder GetFrameBorderHelper(nsGenericHTMLElement* aContent)
{
  if (nullptr != aContent) {
    const nsAttrValue* attr = aContent->GetParsedAttr(nsGkAtoms::frameborder);
    if (attr && attr->Type() == nsAttrValue::eEnum) {
      switch (attr->GetEnumValue())
      {
        case NS_STYLE_FRAME_YES:
        case NS_STYLE_FRAME_1:
          return eFrameborder_Yes;

        case NS_STYLE_FRAME_NO:
        case NS_STYLE_FRAME_0:
          return eFrameborder_No;
      }
    }
  }
  return eFrameborder_Notset;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder()
{
  nsFrameborder result = eFrameborder_Notset;
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    result = GetFrameBorderHelper(content);
  }
  if (eFrameborder_Notset == result) {
    return mParentFrameborder;
  }
  return result;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder(nsIContent* aContent)
{
  nsFrameborder result = eFrameborder_Notset;

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(aContent);

  if (content) {
    result = GetFrameBorderHelper(content);
  }
  if (eFrameborder_Notset == result) {
    return GetFrameBorder();
  }
  return result;
}

nscolor nsHTMLFramesetFrame::GetBorderColor()
{
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::bordercolor);
    if (attr) {
      nscolor color;
      if (attr->GetColorValue(color)) {
        return color;
      }
    }
  }

  return mParentBorderColor;
}

nscolor nsHTMLFramesetFrame::GetBorderColor(nsIContent* aContent)
{
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(aContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::bordercolor);
    if (attr) {
      nscolor color;
      if (attr->GetColorValue(color)) {
        return color;
      }
    }
  }
  return GetBorderColor();
}

void
nsHTMLFramesetFrame::Reflow(nsPresContext*           aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  nsIPresShell *shell = aPresContext->PresShell();
  nsStyleSet *styleSet = shell->StyleSet();

  GetParent()->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);

  //printf("FramesetFrame2::Reflow %X (%d,%d) \n", this, aReflowState.AvailableWidth(), aReflowState.AvailableHeight());
  // Always get the size so that the caller knows how big we are
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);

  nscoord width  = (aDesiredSize.Width() <= aReflowState.AvailableWidth())
    ? aDesiredSize.Width() : aReflowState.AvailableWidth();
  nscoord height = (aDesiredSize.Height() <= aReflowState.AvailableHeight())
    ? aDesiredSize.Height() : aReflowState.AvailableHeight();

  // We might be reflowed more than once with NS_FRAME_FIRST_REFLOW;
  // that's allowed.  (Though it will only happen for misuse of frameset
  // that includes it within other content.)  So measure firstTime by
  // what we care about, which is whether we've processed the data we
  // process below if firstTime is true.
  MOZ_ASSERT(!mChildFrameborder == !mChildBorderColors);
  bool firstTime = !!mChildFrameborder;

  // subtract out the width of all of the potential borders. There are
  // only borders between <frame>s. There are none on the edges (e.g the
  // leftmost <frame> has no left border).
  int32_t borderWidth = GetBorderWidth(aPresContext, true);

  width  -= (mNumCols - 1) * borderWidth;
  if (width < 0) width = 0;

  height -= (mNumRows - 1) * borderWidth;
  if (height < 0) height = 0;

  HTMLFrameSetElement* ourContent = HTMLFrameSetElement::FromContent(mContent);
  NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
  const nsFramesetSpec* rowSpecs = nullptr;
  const nsFramesetSpec* colSpecs = nullptr;
  int32_t rows = 0;
  int32_t cols = 0;
  ourContent->GetRowSpec(&rows, &rowSpecs);
  ourContent->GetColSpec(&cols, &colSpecs);
  // If the number of cols or rows has changed, the frame for the frameset
  // will be re-created.
  if (mNumRows != rows || mNumCols != cols) {
    aStatus = NS_FRAME_COMPLETE;
    mDrag.UnSet();
    NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
    return;
  }

  CalculateRowCol(aPresContext, width, mNumCols, colSpecs, mColSizes.get());
  CalculateRowCol(aPresContext, height, mNumRows, rowSpecs, mRowSizes.get());

  UniquePtr<bool[]>  verBordersVis; // vertical borders visibility
  UniquePtr<nscolor[]> verBorderColors;
  UniquePtr<bool[]>  horBordersVis; // horizontal borders visibility
  UniquePtr<nscolor[]> horBorderColors;
  nscolor                 borderColor = GetBorderColor();
  nsFrameborder           frameborder = GetFrameBorder();

  if (firstTime) {
    // Check for overflow in memory allocations using mNumCols and mNumRows
    // which have a maxium value of NS_MAX_FRAMESET_SPEC_COUNT.
    PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < UINT_MAX / sizeof(bool));
    PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < UINT_MAX / sizeof(nscolor));

    verBordersVis = MakeUnique<bool[]>(mNumCols);
    verBorderColors = MakeUnique<nscolor[]>(mNumCols);
    for (int verX  = 0; verX < mNumCols; verX++) {
      verBordersVis[verX] = false;
      verBorderColors[verX] = NO_COLOR;
    }

    horBordersVis = MakeUnique<bool[]>(mNumRows);
    horBorderColors = MakeUnique<nscolor[]>(mNumRows);
    for (int horX = 0; horX < mNumRows; horX++) {
      horBordersVis[horX] = false;
      horBorderColors[horX] = NO_COLOR;
    }
  }

  // reflow the children
  int32_t lastRow = 0;
  int32_t lastCol = 0;
  int32_t borderChildX = mNonBorderChildCount; // index of border children
  nsHTMLFramesetBorderFrame* borderFrame = nullptr;
  nsPoint offset(0,0);
  nsSize size, lastSize;
  WritingMode wm = GetWritingMode();
  LogicalSize logicalSize(wm);
  nsIFrame* child = mFrames.FirstChild();

  for (int32_t childX = 0; childX < mNonBorderChildCount; childX++) {
    nsIntPoint cellIndex;
    GetSizeOfChildAt(childX, wm, logicalSize, cellIndex);
    size = logicalSize.GetPhysicalSize(wm);

    if (lastRow != cellIndex.y) {  // changed to next row
      offset.x = 0;
      offset.y += lastSize.height;
      if (firstTime) { // create horizontal border

        RefPtr<nsStyleContext> pseudoStyleContext;
        pseudoStyleContext = styleSet->
          ResolveAnonymousBoxStyle(nsCSSAnonBoxes::horizontalFramesetBorder,
                                   mStyleContext);

        borderFrame = new (shell) nsHTMLFramesetBorderFrame(pseudoStyleContext,
                                                            borderWidth,
                                                            false,
                                                            false);
        borderFrame->Init(mContent, this, nullptr);
        mChildCount++;
        mFrames.AppendFrame(nullptr, borderFrame);
        mHorBorders[cellIndex.y-1] = borderFrame;
        // set the neighbors for determining drag boundaries
        borderFrame->mPrevNeighbor = lastRow;
        borderFrame->mNextNeighbor = cellIndex.y;
      } else {
        borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
        borderFrame->mWidth = borderWidth;
        borderChildX++;
      }
      nsSize borderSize(aDesiredSize.Width(), borderWidth);
      ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
      borderFrame = nullptr;
      offset.y += borderWidth;
    } else {
      if (cellIndex.x > 0) {  // moved to next col in same row
        if (0 == cellIndex.y) { // in 1st row
          if (firstTime) { // create vertical border

            RefPtr<nsStyleContext> pseudoStyleContext;
            pseudoStyleContext = styleSet->
              ResolveAnonymousBoxStyle(nsCSSAnonBoxes::verticalFramesetBorder,
                                       mStyleContext);

            borderFrame = new (shell) nsHTMLFramesetBorderFrame(pseudoStyleContext,
                                                                borderWidth,
                                                                true,
                                                                false);
            borderFrame->Init(mContent, this, nullptr);
            mChildCount++;
            mFrames.AppendFrame(nullptr, borderFrame);
            mVerBorders[cellIndex.x-1] = borderFrame;
            // set the neighbors for determining drag boundaries
            borderFrame->mPrevNeighbor = lastCol;
            borderFrame->mNextNeighbor = cellIndex.x;
          } else {
            borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
            borderFrame->mWidth = borderWidth;
            borderChildX++;
          }
          nsSize borderSize(borderWidth, aDesiredSize.Height());
          ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
          borderFrame = nullptr;
        }
        offset.x += borderWidth;
      }
    }

    ReflowPlaceChild(child, aPresContext, aReflowState, offset, size, &cellIndex);

    if (firstTime) {
      int32_t childVis;
      nsHTMLFramesetFrame* framesetFrame = do_QueryFrame(child);
      nsSubDocumentFrame* subdocFrame;
      if (framesetFrame) {
        childVis = framesetFrame->mEdgeVisibility;
        mChildBorderColors[childX] = framesetFrame->mEdgeColors;
      } else if ((subdocFrame = do_QueryFrame(child))) {
        if (eFrameborder_Yes == mChildFrameborder[childX]) {
          childVis = ALL_VIS;
        } else if (eFrameborder_No == mChildFrameborder[childX]) {
          childVis = NONE_VIS;
        } else {  // notset
          childVis = (eFrameborder_No == frameborder) ? NONE_VIS : ALL_VIS;
        }
      } else {  // blank
        DebugOnly<nsHTMLFramesetBlankFrame*> blank;
        MOZ_ASSERT(blank = do_QueryFrame(child), "unexpected child frame type");
        childVis = NONE_VIS;
      }
      nsBorderColor childColors = mChildBorderColors[childX];
      // set the visibility, color of our edge borders based on children
      if (0 == cellIndex.x) {
        if (!(mEdgeVisibility & LEFT_VIS)) {
          mEdgeVisibility |= (LEFT_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mLeft) {
          mEdgeColors.mLeft = childColors.mLeft;
        }
      }
      if (0 == cellIndex.y) {
        if (!(mEdgeVisibility & TOP_VIS)) {
          mEdgeVisibility |= (TOP_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mTop) {
          mEdgeColors.mTop = childColors.mTop;
        }
      }
      if (mNumCols-1 == cellIndex.x) {
        if (!(mEdgeVisibility & RIGHT_VIS)) {
          mEdgeVisibility |= (RIGHT_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mRight) {
          mEdgeColors.mRight = childColors.mRight;
        }
      }
      if (mNumRows-1 == cellIndex.y) {
        if (!(mEdgeVisibility & BOTTOM_VIS)) {
          mEdgeVisibility |= (BOTTOM_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mBottom) {
          mEdgeColors.mBottom = childColors.mBottom;
        }
      }
      // set the visibility of borders that the child may affect
      if (childVis & RIGHT_VIS) {
        verBordersVis[cellIndex.x] = true;
      }
      if (childVis & BOTTOM_VIS) {
        horBordersVis[cellIndex.y] = true;
      }
      if ((cellIndex.x > 0) && (childVis & LEFT_VIS)) {
        verBordersVis[cellIndex.x-1] = true;
      }
      if ((cellIndex.y > 0) && (childVis & TOP_VIS)) {
        horBordersVis[cellIndex.y-1] = true;
      }
      // set the colors of borders that the child may affect
      if (NO_COLOR == verBorderColors[cellIndex.x]) {
        verBorderColors[cellIndex.x] = mChildBorderColors[childX].mRight;
      }
      if (NO_COLOR == horBorderColors[cellIndex.y]) {
        horBorderColors[cellIndex.y] = mChildBorderColors[childX].mBottom;
      }
      if ((cellIndex.x > 0) && (NO_COLOR == verBorderColors[cellIndex.x-1])) {
        verBorderColors[cellIndex.x-1] = mChildBorderColors[childX].mLeft;
      }
      if ((cellIndex.y > 0) && (NO_COLOR == horBorderColors[cellIndex.y-1])) {
        horBorderColors[cellIndex.y-1] = mChildBorderColors[childX].mTop;
      }
    }
    lastRow  = cellIndex.y;
    lastCol  = cellIndex.x;
    lastSize = size;
    offset.x += size.width;
    child = child->GetNextSibling();
  }

  if (firstTime) {
    nscolor childColor;
    // set the visibility, color, mouse sensitivity of borders
    for (int verX = 0; verX < mNumCols-1; verX++) {
      if (mVerBorders[verX]) {
        mVerBorders[verX]->SetVisibility(verBordersVis[verX]);
        SetBorderResize(mVerBorders[verX]);
        childColor = (NO_COLOR == verBorderColors[verX]) ? borderColor : verBorderColors[verX];
        mVerBorders[verX]->SetColor(childColor);
      }
    }
    for (int horX = 0; horX < mNumRows-1; horX++) {
      if (mHorBorders[horX]) {
        mHorBorders[horX]->SetVisibility(horBordersVis[horX]);
        SetBorderResize(mHorBorders[horX]);
        childColor = (NO_COLOR == horBorderColors[horX]) ? borderColor : horBorderColors[horX];
        mHorBorders[horX]->SetColor(childColor);
      }
    }

    mChildFrameborder.reset();
    mChildBorderColors.reset();
  }

  aStatus = NS_FRAME_COMPLETE;
  mDrag.UnSet();

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

nsIAtom*
nsHTMLFramesetFrame::GetType() const
{
  return nsGkAtoms::frameSetFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsHTMLFramesetFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Frameset"), aResult);
}
#endif

bool
nsHTMLFramesetFrame::IsLeaf() const
{
  // We handle constructing our kids manually
  return true;
}

bool
nsHTMLFramesetFrame::CanResize(bool aVertical,
                               bool aLeft)
{
  int32_t childX;
  int32_t startX;
  if (aVertical) {
    startX = (aLeft) ? 0 : mNumCols-1;
    for (childX = startX; childX < mNonBorderChildCount; childX += mNumCols) {
      if (!CanChildResize(aVertical, aLeft, childX)) {
        return false;
      }
    }
  } else {
    startX = (aLeft) ? 0 : (mNumRows - 1) * mNumCols;
    int32_t endX = startX + mNumCols;
    for (childX = startX; childX < endX; childX++) {
      if (!CanChildResize(aVertical, aLeft, childX)) {
        return false;
      }
    }
  }
  return true;
}

bool
nsHTMLFramesetFrame::GetNoResize(nsIFrame* aChildFrame)
{
  nsIContent* content = aChildFrame->GetContent();

  return content && content->HasAttr(kNameSpaceID_None, nsGkAtoms::noresize);
}

bool
nsHTMLFramesetFrame::CanChildResize(bool aVertical, bool aLeft, int32_t aChildX)
{
  nsIFrame* child = mFrames.FrameAt(aChildX);
  nsHTMLFramesetFrame* frameset = do_QueryFrame(child);
  return frameset ? frameset->CanResize(aVertical, aLeft) : !GetNoResize(child);
}

// This calculates and sets the resizability of all border frames

void
nsHTMLFramesetFrame::RecalculateBorderResize()
{
  if (!mContent) {
    return;
  }

  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT < INT32_MAX / NS_MAX_FRAMESET_SPEC_COUNT);
  PR_STATIC_ASSERT(NS_MAX_FRAMESET_SPEC_COUNT
                   < UINT_MAX / sizeof(int32_t) / NS_MAX_FRAMESET_SPEC_COUNT);
  // set the visibility and mouse sensitivity of borders
  int32_t verX;
  for (verX = 0; verX < mNumCols-1; verX++) {
    if (mVerBorders[verX]) {
      mVerBorders[verX]->mCanResize = true;
      SetBorderResize(mVerBorders[verX]);
    }
  }
  int32_t horX;
  for (horX = 0; horX < mNumRows-1; horX++) {
    if (mHorBorders[horX]) {
      mHorBorders[horX]->mCanResize = true;
      SetBorderResize(mHorBorders[horX]);
    }
  }
}

void
nsHTMLFramesetFrame::SetBorderResize(nsHTMLFramesetBorderFrame* aBorderFrame)
{
  if (aBorderFrame->mVertical) {
    for (int rowX = 0; rowX < mNumRows; rowX++) {
      int32_t childX = aBorderFrame->mPrevNeighbor + (rowX * mNumCols);
      if (!CanChildResize(true, false, childX) ||
          !CanChildResize(true, true, childX+1)) {
        aBorderFrame->mCanResize = false;
      }
    }
  } else {
    int32_t childX = aBorderFrame->mPrevNeighbor * mNumCols;
    int32_t endX   = childX + mNumCols;
    for (; childX < endX; childX++) {
      if (!CanChildResize(false, false, childX)) {
        aBorderFrame->mCanResize = false;
      }
    }
    endX = endX + mNumCols;
    for (; childX < endX; childX++) {
      if (!CanChildResize(false, true, childX)) {
        aBorderFrame->mCanResize = false;
      }
    }
  }
}

void
nsHTMLFramesetFrame::StartMouseDrag(nsPresContext*             aPresContext,
                                    nsHTMLFramesetBorderFrame* aBorder,
                                    WidgetGUIEvent*            aEvent)
{
#if 0
  int32_t index;
  IndexOf(aBorder, index);
  NS_ASSERTION((nullptr != aBorder) && (index >= 0), "invalid dragger");
#endif

  nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);

  mDragger = aBorder;

  mFirstDragPoint = aEvent->refPoint;

  // Store the original frame sizes
  if (mDragger->mVertical) {
    mPrevNeighborOrigSize = mColSizes[mDragger->mPrevNeighbor];
    mNextNeighborOrigSize = mColSizes[mDragger->mNextNeighbor];
  } else {
    mPrevNeighborOrigSize = mRowSizes[mDragger->mPrevNeighbor];
    mNextNeighborOrigSize = mRowSizes[mDragger->mNextNeighbor];
  }

  gDragInProgress = true;
}


void
nsHTMLFramesetFrame::MouseDrag(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent)
{
  // if the capture ended, reset the drag state
  if (nsIPresShell::GetCapturingContent() != GetContent()) {
    mDragger = nullptr;
    gDragInProgress = false;
    return;
  }

  int32_t change; // measured positive from left-to-right or top-to-bottom
  nsWeakFrame weakFrame(this);
  if (mDragger->mVertical) {
    change = aPresContext->DevPixelsToAppUnits(aEvent->refPoint.x - mFirstDragPoint.x);
    if (change > mNextNeighborOrigSize - mMinDrag) {
      change = mNextNeighborOrigSize - mMinDrag;
    } else if (change <= mMinDrag - mPrevNeighborOrigSize) {
      change = mMinDrag - mPrevNeighborOrigSize;
    }
    mColSizes[mDragger->mPrevNeighbor] = mPrevNeighborOrigSize + change;
    mColSizes[mDragger->mNextNeighbor] = mNextNeighborOrigSize - change;

    if (change != 0) {
      // Recompute the specs from the new sizes.
      nscoord width = mRect.width - (mNumCols - 1) * GetBorderWidth(aPresContext, true);
      HTMLFrameSetElement* ourContent = HTMLFrameSetElement::FromContent(mContent);
      NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
      const nsFramesetSpec* colSpecs = nullptr;
      ourContent->GetColSpec(&mNumCols, &colSpecs);
      nsAutoString newColAttr;
      GenerateRowCol(aPresContext, width, mNumCols, colSpecs, mColSizes.get(),
                     newColAttr);
      // Setting the attr will trigger a reflow
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::cols, newColAttr, true);
    }
  } else {
    change = aPresContext->DevPixelsToAppUnits(aEvent->refPoint.y - mFirstDragPoint.y);
    if (change > mNextNeighborOrigSize - mMinDrag) {
      change = mNextNeighborOrigSize - mMinDrag;
    } else if (change <= mMinDrag - mPrevNeighborOrigSize) {
      change = mMinDrag - mPrevNeighborOrigSize;
    }
    mRowSizes[mDragger->mPrevNeighbor] = mPrevNeighborOrigSize + change;
    mRowSizes[mDragger->mNextNeighbor] = mNextNeighborOrigSize - change;

    if (change != 0) {
      // Recompute the specs from the new sizes.
      nscoord height = mRect.height - (mNumRows - 1) * GetBorderWidth(aPresContext, true);
      HTMLFrameSetElement* ourContent = HTMLFrameSetElement::FromContent(mContent);
      NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
      const nsFramesetSpec* rowSpecs = nullptr;
      ourContent->GetRowSpec(&mNumRows, &rowSpecs);
      nsAutoString newRowAttr;
      GenerateRowCol(aPresContext, height, mNumRows, rowSpecs, mRowSizes.get(),
                     newRowAttr);
      // Setting the attr will trigger a reflow
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::rows, newRowAttr, true);
    }
  }

  ENSURE_TRUE(weakFrame.IsAlive());
  if (change != 0) {
    mDrag.Reset(mDragger->mVertical, mDragger->mPrevNeighbor, change, this);
  }
}

void
nsHTMLFramesetFrame::EndMouseDrag(nsPresContext* aPresContext)
{
  nsIPresShell::SetCapturingContent(nullptr, 0);
  mDragger = nullptr;
  gDragInProgress = false;
}

nsIFrame*
NS_NewHTMLFramesetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
#ifdef DEBUG
  const nsStyleDisplay* disp = aContext->StyleDisplay();
  NS_ASSERTION(!disp->IsAbsolutelyPositionedStyle() && !disp->IsFloatingStyle(),
               "Framesets should not be positioned and should not float");
#endif

  return new (aPresShell) nsHTMLFramesetFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLFramesetFrame)

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
nsHTMLFramesetBorderFrame::nsHTMLFramesetBorderFrame(nsStyleContext* aContext,
                                                     int32_t aWidth,
                                                     bool    aVertical,
                                                     bool    aVisibility)
  : nsLeafFrame(aContext), mWidth(aWidth), mVertical(aVertical), mVisibility(aVisibility)
{
   mCanResize    = true;
   mColor        = NO_COLOR;
   mPrevNeighbor = 0;
   mNextNeighbor = 0;
}

nsHTMLFramesetBorderFrame::~nsHTMLFramesetBorderFrame()
{
  //printf("nsHTMLFramesetBorderFrame destructor %p \n", this);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLFramesetBorderFrame)

nscoord nsHTMLFramesetBorderFrame::GetIntrinsicISize()
{
  // No intrinsic width
  return 0;
}

nscoord nsHTMLFramesetBorderFrame::GetIntrinsicBSize()
{
  // No intrinsic height
  return 0;
}

void nsHTMLFramesetBorderFrame::SetVisibility(bool aVisibility)
{
  mVisibility = aVisibility;
}

void nsHTMLFramesetBorderFrame::SetColor(nscolor aColor)
{
  mColor = aColor;
}


void
nsHTMLFramesetBorderFrame::Reflow(nsPresContext*           aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetBorderFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Override Reflow(), since we don't want to deal with what our
  // computed values are.
  SizeToAvailSize(aReflowState, aDesiredSize);

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  aStatus = NS_FRAME_COMPLETE;
}

class nsDisplayFramesetBorder : public nsDisplayItem {
public:
  nsDisplayFramesetBorder(nsDisplayListBuilder* aBuilder,
                          nsHTMLFramesetBorderFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFramesetBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFramesetBorder() {
    MOZ_COUNT_DTOR(nsDisplayFramesetBorder);
  }
#endif

  // REVIEW: see old GetFrameForPoint
  // Receives events in its bounds
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("FramesetBorder", TYPE_FRAMESET_BORDER)
};

void nsDisplayFramesetBorder::Paint(nsDisplayListBuilder* aBuilder,
                                    nsRenderingContext* aCtx)
{
  static_cast<nsHTMLFramesetBorderFrame*>(mFrame)->
    PaintBorder(aCtx->GetDrawTarget(), ToReferenceFrame());
}

void
nsHTMLFramesetBorderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayFramesetBorder(aBuilder, this));
}

void nsHTMLFramesetBorderFrame::PaintBorder(DrawTarget* aDrawTarget,
                                            nsPoint aPt)
{
  nscoord widthInPixels = nsPresContext::AppUnitsToIntCSSPixels(mWidth);
  nscoord pixelWidth    = nsPresContext::CSSPixelsToAppUnits(1);

  if (widthInPixels <= 0)
    return;

  ColorPattern bgColor(ToDeviceColor(
                 LookAndFeel::GetColor(LookAndFeel::eColorID_WidgetBackground,
                                       NS_RGB(200, 200, 200))));

  ColorPattern fgColor(ToDeviceColor(
                 LookAndFeel::GetColor(LookAndFeel::eColorID_WidgetForeground,
                                       NS_RGB(0, 0, 0))));

  ColorPattern hltColor(ToDeviceColor(
                 LookAndFeel::GetColor(LookAndFeel::eColorID_Widget3DHighlight,
                                       NS_RGB(255, 255, 255))));

  ColorPattern sdwColor(ToDeviceColor(
                 LookAndFeel::GetColor(LookAndFeel::eColorID_Widget3DShadow,
                                       NS_RGB(128, 128, 128))));

  ColorPattern color(ToDeviceColor(NS_RGB(255, 255, 255))); // default to white
  if (mVisibility) {
    color = (NO_COLOR == mColor) ? bgColor :
                                   ColorPattern(ToDeviceColor(mColor));
  }

  int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();

  Point toRefFrame = NSPointToPoint(aPt, appUnitsPerDevPixel);

  AutoRestoreTransform autoRestoreTransform(aDrawTarget);
  aDrawTarget->SetTransform(
    aDrawTarget->GetTransform().PreTranslate(toRefFrame));

  nsPoint start(0, 0);
  nsPoint end = mVertical ? nsPoint(0, mRect.height) : nsPoint(mRect.width, 0);

  // draw grey or white first
  for (int i = 0; i < widthInPixels; i++) {
    StrokeLineWithSnapping(start, end, appUnitsPerDevPixel, *aDrawTarget,
                           color);
    if (mVertical) {
      start.x += pixelWidth;
      end.x =  start.x;
    } else {
      start.y += pixelWidth;
      end.y =  start.y;
    }
  }

  if (!mVisibility)
    return;

  if (widthInPixels >= 5) {
    start.x = (mVertical) ? pixelWidth : 0;
    start.y = (mVertical) ? 0 : pixelWidth;
    end.x   = (mVertical) ? start.x : mRect.width;
    end.y   = (mVertical) ? mRect.height : start.y;
    StrokeLineWithSnapping(start, end, appUnitsPerDevPixel, *aDrawTarget,
                           hltColor);
  }

  if (widthInPixels >= 2) {
    start.x = (mVertical) ? mRect.width - (2 * pixelWidth) : 0;
    start.y = (mVertical) ? 0 : mRect.height - (2 * pixelWidth);
    end.x   = (mVertical) ? start.x : mRect.width;
    end.y   = (mVertical) ? mRect.height : start.y;
    StrokeLineWithSnapping(start, end, appUnitsPerDevPixel, *aDrawTarget,
                           sdwColor);
  }

  if (widthInPixels >= 1) {
    start.x = (mVertical) ? mRect.width - pixelWidth : 0;
    start.y = (mVertical) ? 0 : mRect.height - pixelWidth;
    end.x   = (mVertical) ? start.x : mRect.width;
    end.y   = (mVertical) ? mRect.height : start.y;
    StrokeLineWithSnapping(start, end, appUnitsPerDevPixel, *aDrawTarget,
                           fgColor);
  }
}


nsresult
nsHTMLFramesetBorderFrame::HandleEvent(nsPresContext* aPresContext,
                                       WidgetGUIEvent* aEvent,
                                       nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eIgnore;

  //XXX Mouse setting logic removed.  The remaining logic should also move.
  if (!mCanResize) {
    return NS_OK;
  }

  if (aEvent->mMessage == eMouseDown &&
      aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
    nsHTMLFramesetFrame* parentFrame = do_QueryFrame(GetParent());
    if (parentFrame) {
      parentFrame->StartMouseDrag(aPresContext, this, aEvent);
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
    }
  }
  return NS_OK;
}

nsresult
nsHTMLFramesetBorderFrame::GetCursor(const nsPoint&    aPoint,
                                     nsIFrame::Cursor& aCursor)
{
  if (!mCanResize) {
    aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
  } else {
    aCursor.mCursor = (mVertical) ? NS_STYLE_CURSOR_EW_RESIZE : NS_STYLE_CURSOR_NS_RESIZE;
  }
  return NS_OK;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsHTMLFramesetBorderFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FramesetBorder"), aResult);
}
#endif

/*******************************************************************************
 * nsHTMLFramesetBlankFrame
 ******************************************************************************/

NS_QUERYFRAME_HEAD(nsHTMLFramesetBlankFrame)
  NS_QUERYFRAME_ENTRY(nsHTMLFramesetBlankFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLFramesetBlankFrame)

nsHTMLFramesetBlankFrame::~nsHTMLFramesetBlankFrame()
{
  //printf("nsHTMLFramesetBlankFrame destructor %p \n", this);
}

nscoord nsHTMLFramesetBlankFrame::GetIntrinsicISize()
{
  // No intrinsic width
  return 0;
}

nscoord nsHTMLFramesetBlankFrame::GetIntrinsicBSize()
{
  // No intrinsic height
  return 0;
}

void
nsHTMLFramesetBlankFrame::Reflow(nsPresContext*           aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetBlankFrame");

  // Override Reflow(), since we don't want to deal with what our
  // computed values are.
  SizeToAvailSize(aReflowState, aDesiredSize);

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  aStatus = NS_FRAME_COMPLETE;
}

class nsDisplayFramesetBlank : public nsDisplayItem {
public:
  nsDisplayFramesetBlank(nsDisplayListBuilder* aBuilder,
                         nsIFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFramesetBlank);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFramesetBlank() {
    MOZ_COUNT_DTOR(nsDisplayFramesetBlank);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("FramesetBlank", TYPE_FRAMESET_BLANK)
};

void nsDisplayFramesetBlank::Paint(nsDisplayListBuilder* aBuilder,
                                   nsRenderingContext* aCtx)
{
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect rect =
    NSRectToSnappedRect(mVisibleRect, appUnitsPerDevPixel, *drawTarget);
  ColorPattern white(ToDeviceColor(Color(1.f, 1.f, 1.f, 1.f)));
  drawTarget->FillRect(rect, white);
}

void
nsHTMLFramesetBlankFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists)
{
  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayFramesetBlank(aBuilder, this));
}
