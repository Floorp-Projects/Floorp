/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

// How boxes layout
// ----------------
// Boxes layout a bit differently than html. html does a bottom up layout. Where boxes do a top down.
// 1) First thing a box does it goes out and askes each child for its min, max, and preferred sizes.
// 2) It then adds them up to determine its size.
// 3) If the box was asked to layout it self intrinically it will layout its children at their preferred size
//    otherwise it will layout the child at the size it was told to. It will squeeze or stretch its children if 
//    Necessary.
//
// However there is a catch. Some html components like block frames can not determine their preferred size. 
// this is their size if they were laid out intrinsically. So the box will flow the child to determine this can
// cache the value.

// Boxes and Incremental Reflow
// ----------------------------
// Boxes layout out top down by adding up their children's min, max, and preferred sizes. Only problem is if a incremental
// reflow occurs. The preferred size of a child deep in the hierarchy could change. And this could change
// any number of syblings around the box. Basically any children in the reflow chain must have their caches cleared
// so when asked for there current size they can relayout themselves. 

#include "nsBoxFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsBoxLayoutState.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/Move.h"
#include "nsStyleContext.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsHTMLParts.h"
#include "nsViewManager.h"
#include "nsView.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIServiceManager.h"
#include "nsBoxLayout.h"
#include "nsSprocketLayout.h"
#include "nsIScrollableFrame.h"
#include "nsWidgetsCID.h"
#include "nsCSSAnonBoxes.h"
#include "nsContainerFrame.h"
#include "nsIDOMElement.h"
#include "nsITheme.h"
#include "nsTransform2D.h"
#include "mozilla/EventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsDisplayList.h"
#include "mozilla/Preferences.h"
#include "nsThemeConstants.h"
#include "nsLayoutUtils.h"
#include "nsSliderFrame.h"
#include <algorithm>

// Needed for Print Preview
#include "nsIURI.h"

#include "mozilla/TouchEvents.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

//define DEBUG_REDRAW

#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8

//#define TEST_SANITY

#ifdef DEBUG_rods
//#define DO_NOISY_REFLOW
#endif

#ifdef DEBUG_LAYOUT
bool nsBoxFrame::gDebug = false;
nsIFrame* nsBoxFrame::mDebugChild = nullptr;
#endif

nsIFrame*
NS_NewBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot, nsBoxLayout* aLayoutManager)
{
  return new (aPresShell) nsBoxFrame(aContext, aIsRoot, aLayoutManager);
}

nsIFrame*
NS_NewBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsBoxFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsBoxFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsBoxFrame)
  NS_QUERYFRAME_ENTRY(nsBoxFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)
#endif

nsBoxFrame::nsBoxFrame(nsStyleContext* aContext,
                       LayoutFrameType aType,
                       bool aIsRoot,
                       nsBoxLayout* aLayoutManager)
  : nsContainerFrame(aContext, aType)
  , mFlex(0)
  , mAscent(0)
{
  mState |= NS_STATE_IS_HORIZONTAL;
  mState |= NS_STATE_AUTO_STRETCH;

  if (aIsRoot)
     mState |= NS_STATE_IS_ROOT;

  mValign = vAlign_Top;
  mHalign = hAlign_Left;

  // if no layout manager specified us the static sprocket layout
  nsCOMPtr<nsBoxLayout> layout = aLayoutManager;

  if (layout == nullptr) {
    NS_NewSprocketLayout(layout);
  }

  SetXULLayoutManager(layout);
}

nsBoxFrame::~nsBoxFrame()
{
}

void
nsBoxFrame::SetInitialChildList(ChildListID     aListID,
                                nsFrameList&    aChildList)
{
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  if (aListID == kPrincipalList) {
    // initialize our list of infos.
    nsBoxLayoutState state(PresContext());
    CheckBoxOrder();
    if (mLayoutManager)
      mLayoutManager->ChildrenSet(this, state, mFrames.FirstChild());
  }
}

/* virtual */ void
nsBoxFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  // The values that CacheAttributes() computes depend on our style,
  // so we need to recompute them here...
  CacheAttributes();
}

/**
 * Initialize us. This is a good time to get the alignment of the box
 */
void
nsBoxFrame::Init(nsIContent*       aContent,
                 nsContainerFrame* aParent,
                 nsIFrame*         aPrevInFlow)
{
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  MarkIntrinsicISizesDirty();

  CacheAttributes();

#ifdef DEBUG_LAYOUT
    // if we are root and this
  if (mState & NS_STATE_IS_ROOT) {
    GetDebugPref();
  }
#endif

  UpdateMouseThrough();

  // register access key
  RegUnregAccessKey(true);
}

void nsBoxFrame::UpdateMouseThrough()
{
  if (mContent) {
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::never, &nsGkAtoms::always, nullptr};
    switch (mContent->FindAttrValueIn(kNameSpaceID_None,
              nsGkAtoms::mousethrough, strings, eCaseMatters)) {
      case 0: AddStateBits(NS_FRAME_MOUSE_THROUGH_NEVER); break;
      case 1: AddStateBits(NS_FRAME_MOUSE_THROUGH_ALWAYS); break;
      case 2: {
        RemoveStateBits(NS_FRAME_MOUSE_THROUGH_ALWAYS);
        RemoveStateBits(NS_FRAME_MOUSE_THROUGH_NEVER);
        break;
      }
    }
  }
}

void
nsBoxFrame::CacheAttributes()
{
  /*
  printf("Caching: ");
  XULDumpBox(stdout);
  printf("\n");
   */

  mValign = vAlign_Top;
  mHalign = hAlign_Left;

  bool orient = false;
  GetInitialOrientation(orient); 
  if (orient)
    mState |= NS_STATE_IS_HORIZONTAL;
  else
    mState &= ~NS_STATE_IS_HORIZONTAL;

  bool normal = true;
  GetInitialDirection(normal); 
  if (normal)
    mState |= NS_STATE_IS_DIRECTION_NORMAL;
  else
    mState &= ~NS_STATE_IS_DIRECTION_NORMAL;

  GetInitialVAlignment(mValign);
  GetInitialHAlignment(mHalign);
  
  bool equalSize = false;
  GetInitialEqualSize(equalSize); 
  if (equalSize)
        mState |= NS_STATE_EQUAL_SIZE;
    else
        mState &= ~NS_STATE_EQUAL_SIZE;

  bool autostretch = !!(mState & NS_STATE_AUTO_STRETCH);
  GetInitialAutoStretch(autostretch);
  if (autostretch)
        mState |= NS_STATE_AUTO_STRETCH;
     else
        mState &= ~NS_STATE_AUTO_STRETCH;


#ifdef DEBUG_LAYOUT
  bool debug = mState & NS_STATE_SET_TO_DEBUG;
  bool debugSet = GetInitialDebug(debug); 
  if (debugSet) {
        mState |= NS_STATE_DEBUG_WAS_SET;
        if (debug)
            mState |= NS_STATE_SET_TO_DEBUG;
        else
            mState &= ~NS_STATE_SET_TO_DEBUG;
  } else {
        mState &= ~NS_STATE_DEBUG_WAS_SET;
  }
#endif
}

#ifdef DEBUG_LAYOUT
bool
nsBoxFrame::GetInitialDebug(bool& aDebug)
{
  if (!GetContent())
    return false;

  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_false, &nsGkAtoms::_true, nullptr};
  int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None,
      nsGkAtoms::debug, strings, eCaseMatters);
  if (index >= 0) {
    aDebug = index == 1;
    return true;
  }

  return false;
}
#endif

bool
nsBoxFrame::GetInitialHAlignment(nsBoxFrame::Halignment& aHalign)
{
  if (!GetContent())
    return false;

  // XXXdwh Everything inside this if statement is deprecated code.
  static nsIContent::AttrValuesArray alignStrings[] =
    {&nsGkAtoms::left, &nsGkAtoms::right, nullptr};
  static const Halignment alignValues[] = {hAlign_Left, hAlign_Right};
  int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::align,
      alignStrings, eCaseMatters);
  if (index >= 0) {
    aHalign = alignValues[index];
    return true;
  }
      
  // Now that the deprecated stuff is out of the way, we move on to check the appropriate 
  // attribute.  For horizontal boxes, we are checking the PACK attribute.  For vertical boxes
  // we are checking the ALIGN attribute.
  nsIAtom* attrName = IsXULHorizontal() ? nsGkAtoms::pack : nsGkAtoms::align;
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::start, &nsGkAtoms::center, &nsGkAtoms::end, nullptr};
  static const Halignment values[] =
    {hAlign_Left/*not used*/, hAlign_Left, hAlign_Center, hAlign_Right};
  index = GetContent()->FindAttrValueIn(kNameSpaceID_None, attrName,
      strings, eCaseMatters);

  if (index == nsIContent::ATTR_VALUE_NO_MATCH) {
    // The attr was present but had a nonsensical value. Revert to the default.
    return false;
  }
  if (index > 0) {    
    aHalign = values[index];
    return true;
  }

  // Now that we've checked for the attribute it's time to check CSS.  For 
  // horizontal boxes we're checking PACK.  For vertical boxes we are checking
  // ALIGN.
  const nsStyleXUL* boxInfo = StyleXUL();
  if (IsXULHorizontal()) {
    switch (boxInfo->mBoxPack) {
      case StyleBoxPack::Start:
        aHalign = nsBoxFrame::hAlign_Left;
        return true;
      case StyleBoxPack::Center:
        aHalign = nsBoxFrame::hAlign_Center;
        return true;
      case StyleBoxPack::End:
        aHalign = nsBoxFrame::hAlign_Right;
        return true;
      default: // Nonsensical value. Just bail.
        return false;
    }
  }
  else {
    switch (boxInfo->mBoxAlign) {
      case StyleBoxAlign::Start:
        aHalign = nsBoxFrame::hAlign_Left;
        return true;
      case StyleBoxAlign::Center:
        aHalign = nsBoxFrame::hAlign_Center;
        return true;
      case StyleBoxAlign::End:
        aHalign = nsBoxFrame::hAlign_Right;
        return true;
      default: // Nonsensical value. Just bail.
        return false;
    }
  }

  return false;
}

bool
nsBoxFrame::GetInitialVAlignment(nsBoxFrame::Valignment& aValign)
{
  if (!GetContent())
    return false;

  static nsIContent::AttrValuesArray valignStrings[] =
    {&nsGkAtoms::top, &nsGkAtoms::baseline, &nsGkAtoms::middle, &nsGkAtoms::bottom, nullptr};
  static const Valignment valignValues[] =
    {vAlign_Top, vAlign_BaseLine, vAlign_Middle, vAlign_Bottom};
  int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::valign,
      valignStrings, eCaseMatters);
  if (index >= 0) {
    aValign = valignValues[index];
    return true;
  }

  // Now that the deprecated stuff is out of the way, we move on to check the appropriate 
  // attribute.  For horizontal boxes, we are checking the ALIGN attribute.  For vertical boxes
  // we are checking the PACK attribute.
  nsIAtom* attrName = IsXULHorizontal() ? nsGkAtoms::align : nsGkAtoms::pack;
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::start, &nsGkAtoms::center,
     &nsGkAtoms::baseline, &nsGkAtoms::end, nullptr};
  static const Valignment values[] =
    {vAlign_Top/*not used*/, vAlign_Top, vAlign_Middle, vAlign_BaseLine, vAlign_Bottom};
  index = GetContent()->FindAttrValueIn(kNameSpaceID_None, attrName,
      strings, eCaseMatters);
  if (index == nsIContent::ATTR_VALUE_NO_MATCH) {
    // The attr was present but had a nonsensical value. Revert to the default.
    return false;
  }
  if (index > 0) {
    aValign = values[index];
    return true;
  }

  // Now that we've checked for the attribute it's time to check CSS.  For 
  // horizontal boxes we're checking ALIGN.  For vertical boxes we are checking
  // PACK.
  const nsStyleXUL* boxInfo = StyleXUL();
  if (IsXULHorizontal()) {
    switch (boxInfo->mBoxAlign) {
      case StyleBoxAlign::Start:
        aValign = nsBoxFrame::vAlign_Top;
        return true;
      case StyleBoxAlign::Center:
        aValign = nsBoxFrame::vAlign_Middle;
        return true;
      case StyleBoxAlign::Baseline:
        aValign = nsBoxFrame::vAlign_BaseLine;
        return true;
      case StyleBoxAlign::End:
        aValign = nsBoxFrame::vAlign_Bottom;
        return true;
      default: // Nonsensical value. Just bail.
        return false;
    }
  }
  else {
    switch (boxInfo->mBoxPack) {
      case StyleBoxPack::Start:
        aValign = nsBoxFrame::vAlign_Top;
        return true;
      case StyleBoxPack::Center:
        aValign = nsBoxFrame::vAlign_Middle;
        return true;
      case StyleBoxPack::End:
        aValign = nsBoxFrame::vAlign_Bottom;
        return true;
      default: // Nonsensical value. Just bail.
        return false;
    }
  }

  return false;
}

void
nsBoxFrame::GetInitialOrientation(bool& aIsHorizontal)
{
 // see if we are a vertical or horizontal box.
  if (!GetContent())
    return;

  // Check the style system first.
  const nsStyleXUL* boxInfo = StyleXUL();
  if (boxInfo->mBoxOrient == StyleBoxOrient::Horizontal) {
    aIsHorizontal = true;
  } else {
    aIsHorizontal = false;
  }

  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::vertical, &nsGkAtoms::horizontal, nullptr};
  int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::orient,
      strings, eCaseMatters);
  if (index >= 0) {
    aIsHorizontal = index == 1;
  }
}

void
nsBoxFrame::GetInitialDirection(bool& aIsNormal)
{
  if (!GetContent())
    return;

  if (IsXULHorizontal()) {
    // For horizontal boxes only, we initialize our value based off the CSS 'direction' property.
    // This means that BiDI users will end up with horizontally inverted chrome.
    aIsNormal = (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR); // If text runs RTL then so do we.
  }
  else
    aIsNormal = true; // Assume a normal direction in the vertical case.

  // Now check the style system to see if we should invert aIsNormal.
  const nsStyleXUL* boxInfo = StyleXUL();
  if (boxInfo->mBoxDirection == StyleBoxDirection::Reverse) {
    aIsNormal = !aIsNormal; // Invert our direction.
  }

  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  if (IsXULHorizontal()) {
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::reverse, &nsGkAtoms::ltr, &nsGkAtoms::rtl, nullptr};
    int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::dir,
        strings, eCaseMatters);
    if (index >= 0) {
      bool values[] = {!aIsNormal, true, false};
      aIsNormal = values[index];
    }
  } else if (GetContent()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                       nsGkAtoms::reverse, eCaseMatters)) {
    aIsNormal = !aIsNormal;
  }
}

/* Returns true if it was set.
 */
bool
nsBoxFrame::GetInitialEqualSize(bool& aEqualSize)
{
 // see if we are a vertical or horizontal box.
  if (!GetContent())
     return false;

  if (GetContent()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::equalsize,
                           nsGkAtoms::always, eCaseMatters)) {
    aEqualSize = true;
    return true;
  }

  return false;
}

/* Returns true if it was set.
 */
bool
nsBoxFrame::GetInitialAutoStretch(bool& aStretch)
{
  if (!GetContent())
     return false;
  
  // Check the align attribute.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::stretch, nullptr};
  int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::align,
      strings, eCaseMatters);
  if (index != nsIContent::ATTR_MISSING && index != 0) {
    aStretch = index == 1;
    return true;
  }

  // Check the CSS box-align property.
  const nsStyleXUL* boxInfo = StyleXUL();
  aStretch = (boxInfo->mBoxAlign == StyleBoxAlign::Stretch);

  return true;
}

void
nsBoxFrame::DidReflow(nsPresContext*           aPresContext,
                      const ReflowInput*  aReflowInput,
                      nsDidReflowStatus         aStatus)
{
  nsFrameState preserveBits =
    mState & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
  nsFrame::DidReflow(aPresContext, aReflowInput, aStatus);
  mState |= preserveBits;
}

bool
nsBoxFrame::HonorPrintBackgroundSettings()
{
  return (!mContent || !mContent->IsInNativeAnonymousSubtree()) &&
    nsContainerFrame::HonorPrintBackgroundSettings();
}

#ifdef DO_NOISY_REFLOW
static int myCounter = 0;
static void printSize(char * aDesc, nscoord aSize) 
{
  printf(" %s: ", aDesc);
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    printf("UC");
  } else {
    printf("%d", aSize);
  }
}
#endif

/* virtual */ nscoord
nsBoxFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsBoxLayoutState state(PresContext(), aRenderingContext);
  nsSize minSize = GetXULMinSize(state);

  // GetXULMinSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetXULMinSize added, which is the
  // result of GetXULBorderAndPadding.
  nsMargin bp;
  GetXULBorderAndPadding(bp);

  result = minSize.width - bp.LeftRight();
  result = std::max(result, 0);

  return result;
}

/* virtual */ nscoord
nsBoxFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsBoxLayoutState state(PresContext(), aRenderingContext);
  nsSize prefSize = GetXULPrefSize(state);

  // GetXULPrefSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetXULPrefSize added, which is the
  // result of GetXULBorderAndPadding.
  nsMargin bp;
  GetXULBorderAndPadding(bp);

  result = prefSize.width - bp.LeftRight();
  result = std::max(result, 0);

  return result;
}

void
nsBoxFrame::Reflow(nsPresContext*          aPresContext,
                   ReflowOutput&     aDesiredSize,
                   const ReflowInput& aReflowInput,
                   nsReflowStatus&          aStatus)
{
  MarkInReflow();
  // If you make changes to this method, please keep nsLeafBoxFrame::Reflow
  // in sync, if the changes are applicable there.

  DO_GLOBAL_REFLOW_COUNT("nsBoxFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  NS_ASSERTION(aReflowInput.ComputedWidth() >=0 &&
               aReflowInput.ComputedHeight() >= 0, "Computed Size < 0");

#ifdef DO_NOISY_REFLOW
  printf("\n-------------Starting BoxFrame Reflow ----------------------------\n");
  printf("%p ** nsBF::Reflow %d ", this, myCounter++);
  
  printSize("AW", aReflowInput.AvailableWidth());
  printSize("AH", aReflowInput.AvailableHeight());
  printSize("CW", aReflowInput.ComputedWidth());
  printSize("CH", aReflowInput.ComputedHeight());

  printf(" *\n");

#endif

  aStatus.Reset();

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowInput.mRenderingContext,
                         &aReflowInput, aReflowInput.mReflowDepth);

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize computedSize(wm, aReflowInput.ComputedISize(),
                           aReflowInput.ComputedBSize());

  LogicalMargin m = aReflowInput.ComputedLogicalBorderPadding();
  // GetXULBorderAndPadding(m);

  LogicalSize prefSize(wm);

  // if we are told to layout intrinsic then get our preferred size.
  NS_ASSERTION(computedSize.ISize(wm) != NS_INTRINSICSIZE,
               "computed inline size should always be computed");
  if (computedSize.BSize(wm) == NS_INTRINSICSIZE) {
    nsSize physicalPrefSize = GetXULPrefSize(state);
    nsSize minSize = GetXULMinSize(state);
    nsSize maxSize = GetXULMaxSize(state);
    // XXXbz isn't GetXULPrefSize supposed to bounds-check for us?
    physicalPrefSize = BoundsCheck(minSize, physicalPrefSize, maxSize);
    prefSize = LogicalSize(wm, physicalPrefSize);
  }

  // get our desiredSize
  computedSize.ISize(wm) += m.IStart(wm) + m.IEnd(wm);

  if (aReflowInput.ComputedBSize() == NS_INTRINSICSIZE) {
    computedSize.BSize(wm) = prefSize.BSize(wm);
    // prefSize is border-box but min/max constraints are content-box.
    nscoord blockDirBorderPadding =
      aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm);
    nscoord contentBSize = computedSize.BSize(wm) - blockDirBorderPadding;
    // Note: contentHeight might be negative, but that's OK because min-height
    // is never negative.
    computedSize.BSize(wm) = aReflowInput.ApplyMinMaxHeight(contentBSize) +
                             blockDirBorderPadding;
  } else {
    computedSize.BSize(wm) += m.BStart(wm) + m.BEnd(wm);
  }

  nsSize physicalSize = computedSize.GetPhysicalSize(wm);
  nsRect r(mRect.x, mRect.y, physicalSize.width, physicalSize.height);

  SetXULBounds(state, r);
 
  // layout our children
  XULLayout(state);
  
  // ok our child could have gotten bigger. So lets get its bounds
  
  // get the ascent
  LogicalSize boxSize = GetLogicalSize(wm);
  nscoord ascent = boxSize.BSize(wm);

  // getting the ascent could be a lot of work. Don't get it if
  // we are the root. The viewport doesn't care about it.
  if (!(mState & NS_STATE_IS_ROOT)) {
    ascent = GetXULBoxAscent(state);
  }

  aDesiredSize.SetSize(wm, boxSize);
  aDesiredSize.SetBlockStartAscent(ascent);

  aDesiredSize.mOverflowAreas = GetOverflowAreas();

#ifdef DO_NOISY_REFLOW
  {
    printf("%p ** nsBF(done) W:%d H:%d  ", this, aDesiredSize.Width(), aDesiredSize.Height());

    if (maxElementSize) {
      printf("MW:%d\n", *maxElementWidth); 
    } else {
      printf("MW:?\n"); 
    }

  }
#endif

  ReflowAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

nsSize
nsBoxFrame::GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState)
{
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize size(0,0);
  DISPLAY_PREF_SIZE(this, size);
  if (!DoesNeedRecalc(mPrefSize)) {
     return mPrefSize;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsXULCollapsed())
    return size;

  // if the size was not completely redefined in CSS then ask our children
  bool widthSet, heightSet;
  if (!nsIFrame::AddXULPrefSize(this, size, widthSet, heightSet))
  {
    if (mLayoutManager) {
      nsSize layoutSize = mLayoutManager->GetXULPrefSize(this, aBoxLayoutState);
      if (!widthSet)
        size.width = layoutSize.width;
      if (!heightSet)
        size.height = layoutSize.height;
    }
    else {
      size = nsBox::GetXULPrefSize(aBoxLayoutState);
    }
  }

  nsSize minSize = GetXULMinSize(aBoxLayoutState);
  nsSize maxSize = GetXULMaxSize(aBoxLayoutState);
  mPrefSize = BoundsCheck(minSize, size, maxSize);
 
  return mPrefSize;
}

nscoord
nsBoxFrame::GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState)
{
  if (!DoesNeedRecalc(mAscent))
     return mAscent;

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsXULCollapsed())
    return 0;

  if (mLayoutManager)
    mAscent = mLayoutManager->GetAscent(this, aBoxLayoutState);
  else
    mAscent = nsBox::GetXULBoxAscent(aBoxLayoutState);

  return mAscent;
}

nsSize
nsBoxFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize size(0,0);
  DISPLAY_MIN_SIZE(this, size);
  if (!DoesNeedRecalc(mMinSize)) {
    return mMinSize;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsXULCollapsed())
    return size;

  // if the size was not completely redefined in CSS then ask our children
  bool widthSet, heightSet;
  if (!nsIFrame::AddXULMinSize(aBoxLayoutState, this, size, widthSet, heightSet))
  {
    if (mLayoutManager) {
      nsSize layoutSize = mLayoutManager->GetXULMinSize(this, aBoxLayoutState);
      if (!widthSet)
        size.width = layoutSize.width;
      if (!heightSet)
        size.height = layoutSize.height;
    }
    else {
      size = nsBox::GetXULMinSize(aBoxLayoutState);
    }
  }
  
  mMinSize = size;

  return size;
}

nsSize
nsBoxFrame::GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState)
{
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize size(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, size);
  if (!DoesNeedRecalc(mMaxSize)) {
    return mMaxSize;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsXULCollapsed())
    return size;

  // if the size was not completely redefined in CSS then ask our children
  bool widthSet, heightSet;
  if (!nsIFrame::AddXULMaxSize(this, size, widthSet, heightSet))
  {
    if (mLayoutManager) {
      nsSize layoutSize = mLayoutManager->GetXULMaxSize(this, aBoxLayoutState);
      if (!widthSet)
        size.width = layoutSize.width;
      if (!heightSet)
        size.height = layoutSize.height;
    }
    else {
      size = nsBox::GetXULMaxSize(aBoxLayoutState);
    }
  }

  mMaxSize = size;

  return size;
}

nscoord
nsBoxFrame::GetXULFlex()
{
  if (!DoesNeedRecalc(mFlex))
     return mFlex;

  mFlex = nsBox::GetXULFlex();

  return mFlex;
}

/**
 * If subclassing please subclass this method not layout. 
 * layout will call this method.
 */
NS_IMETHODIMP
nsBoxFrame::DoXULLayout(nsBoxLayoutState& aState)
{
  uint32_t oldFlags = aState.LayoutFlags();
  aState.SetLayoutFlags(0);

  nsresult rv = NS_OK;
  if (mLayoutManager) {
    CoordNeedsRecalc(mAscent);
    rv = mLayoutManager->XULLayout(this, aState);
  }

  aState.SetLayoutFlags(oldFlags);

  if (HasAbsolutelyPositionedChildren()) {
    // Set up a |reflowInput| to pass into ReflowAbsoluteFrames
    WritingMode wm = GetWritingMode();
    ReflowInput reflowInput(aState.PresContext(), this,
                                  aState.GetRenderingContext(),
                                  LogicalSize(wm, GetLogicalSize().ISize(wm),
                                              NS_UNCONSTRAINEDSIZE));

    // Set up a |desiredSize| to pass into ReflowAbsoluteFrames
    ReflowOutput desiredSize(reflowInput);
    desiredSize.Width() = mRect.width;
    desiredSize.Height() = mRect.height;

    // get the ascent (cribbed from ::Reflow)
    nscoord ascent = mRect.height;

    // getting the ascent could be a lot of work. Don't get it if
    // we are the root. The viewport doesn't care about it.
    if (!(mState & NS_STATE_IS_ROOT)) {
      ascent = GetXULBoxAscent(aState);
    }
    desiredSize.SetBlockStartAscent(ascent);
    desiredSize.mOverflowAreas = GetOverflowAreas();

    AddStateBits(NS_FRAME_IN_REFLOW);
    // Set up a |reflowStatus| to pass into ReflowAbsoluteFrames
    // (just a dummy value; hopefully that's OK)
    nsReflowStatus reflowStatus;
    ReflowAbsoluteFrames(aState.PresContext(), desiredSize,
                         reflowInput, reflowStatus);
    RemoveStateBits(NS_FRAME_IN_REFLOW);
  }

  return rv;
}

void
nsBoxFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // unregister access key
  RegUnregAccessKey(false);

  // clean up the container box's layout manager and child boxes
  SetXULLayoutManager(nullptr);

  nsContainerFrame::DestroyFrom(aDestructRoot);
} 

#ifdef DEBUG_LAYOUT
nsresult
nsBoxFrame::SetXULDebug(nsBoxLayoutState& aState, bool aDebug)
{
  // see if our state matches the given debug state
  bool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  bool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
     if (aDebug) {
         mState |= NS_STATE_CURRENTLY_IN_DEBUG;
     } else {
         mState &= ~NS_STATE_CURRENTLY_IN_DEBUG;
     }
 
     SetDebugOnChildList(aState, mFirstChild, aDebug);

    MarkIntrinsicISizesDirty();
  }

  return NS_OK;
}
#endif

/* virtual */ void
nsBoxFrame::MarkIntrinsicISizesDirty()
{
  SizeNeedsRecalc(mPrefSize);
  SizeNeedsRecalc(mMinSize);
  SizeNeedsRecalc(mMaxSize);
  CoordNeedsRecalc(mFlex);
  CoordNeedsRecalc(mAscent);

  if (mLayoutManager) {
    nsBoxLayoutState state(PresContext());
    mLayoutManager->IntrinsicISizesDirty(this, state);
  }

  // Don't call base class method, since everything it does is within an
  // IsXULBoxWrapped check.
}

void
nsBoxFrame::RemoveFrame(ChildListID     aListID,
                        nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(aListID == kPrincipalList, "We don't support out-of-flow kids");
  nsPresContext* presContext = PresContext();
  nsBoxLayoutState state(presContext);

  // remove the child frame
  mFrames.RemoveFrame(aOldFrame);

  // notify the layout manager
  if (mLayoutManager)
    mLayoutManager->ChildrenRemoved(this, state, aOldFrame);

  // destroy the child frame
  aOldFrame->Destroy();

  // mark us dirty and generate a reflow command
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsBoxFrame::InsertFrames(ChildListID     aListID,
                         nsIFrame*       aPrevFrame,
                         nsFrameList&    aFrameList)
{
   NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
                "inserting after sibling frame with different parent");
   NS_ASSERTION(!aPrevFrame || mFrames.ContainsFrame(aPrevFrame),
                "inserting after sibling frame not in our child list");
   NS_PRECONDITION(aListID == kPrincipalList, "We don't support out-of-flow kids");
   nsBoxLayoutState state(PresContext());

   // insert the child frames
   const nsFrameList::Slice& newFrames =
     mFrames.InsertFrames(this, aPrevFrame, aFrameList);

   // notify the layout manager
   if (mLayoutManager)
     mLayoutManager->ChildrenInserted(this, state, aPrevFrame, newFrames);

   // Make sure to check box order _after_ notifying the layout
   // manager; otherwise the slice we give the layout manager will
   // just be bogus.  If the layout manager cares about the order, we
   // just lose.
   CheckBoxOrder();

#ifdef DEBUG_LAYOUT
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFrames.FirstChild(), true);
#endif

   PresContext()->PresShell()->
     FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                      NS_FRAME_HAS_DIRTY_CHILDREN);
}


void
nsBoxFrame::AppendFrames(ChildListID     aListID,
                         nsFrameList&    aFrameList)
{
   NS_PRECONDITION(aListID == kPrincipalList, "We don't support out-of-flow kids");
   nsBoxLayoutState state(PresContext());

   // append the new frames
   const nsFrameList::Slice& newFrames = mFrames.AppendFrames(this, aFrameList);

   // notify the layout manager
   if (mLayoutManager)
     mLayoutManager->ChildrenAppended(this, state, newFrames);

   // Make sure to check box order _after_ notifying the layout
   // manager; otherwise the slice we give the layout manager will
   // just be bogus.  If the layout manager cares about the order, we
   // just lose.
   CheckBoxOrder();

#ifdef DEBUG_LAYOUT
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFrames.FirstChild(), true);
#endif

   // XXXbz why is this NS_FRAME_FIRST_REFLOW check here?
   if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
     PresContext()->PresShell()->
       FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                        NS_FRAME_HAS_DIRTY_CHILDREN);
   }
}

/* virtual */ nsContainerFrame*
nsBoxFrame::GetContentInsertionFrame()
{
  if (GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK)
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  return nsContainerFrame::GetContentInsertionFrame();
}

nsresult
nsBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                             nsIAtom* aAttribute,
                             int32_t aModType)
{
  nsresult rv = nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                                   aModType);

  // Ignore 'width', 'height', 'screenX', 'screenY' and 'sizemode' on a
  // <window>.
  if (mContent->IsAnyOfXULElements(nsGkAtoms::window,
                                   nsGkAtoms::page,
                                   nsGkAtoms::dialog,
                                   nsGkAtoms::wizard) &&
      (nsGkAtoms::width == aAttribute ||
       nsGkAtoms::height == aAttribute ||
       nsGkAtoms::screenX == aAttribute ||
       nsGkAtoms::screenY == aAttribute ||
       nsGkAtoms::sizemode == aAttribute)) {
    return rv;
  }

  if (aAttribute == nsGkAtoms::width       ||
      aAttribute == nsGkAtoms::height      ||
      aAttribute == nsGkAtoms::align       ||
      aAttribute == nsGkAtoms::valign      ||
      aAttribute == nsGkAtoms::left        ||
      aAttribute == nsGkAtoms::top         ||
      aAttribute == nsGkAtoms::right        ||
      aAttribute == nsGkAtoms::bottom       ||
      aAttribute == nsGkAtoms::start        ||
      aAttribute == nsGkAtoms::end          ||
      aAttribute == nsGkAtoms::minwidth     ||
      aAttribute == nsGkAtoms::maxwidth     ||
      aAttribute == nsGkAtoms::minheight    ||
      aAttribute == nsGkAtoms::maxheight    ||
      aAttribute == nsGkAtoms::flex         ||
      aAttribute == nsGkAtoms::orient       ||
      aAttribute == nsGkAtoms::pack         ||
      aAttribute == nsGkAtoms::dir          ||
      aAttribute == nsGkAtoms::mousethrough ||
      aAttribute == nsGkAtoms::equalsize) {

    if (aAttribute == nsGkAtoms::align  ||
        aAttribute == nsGkAtoms::valign ||
        aAttribute == nsGkAtoms::orient  ||
        aAttribute == nsGkAtoms::pack    ||
#ifdef DEBUG_LAYOUT
        aAttribute == nsGkAtoms::debug   ||
#endif
        aAttribute == nsGkAtoms::dir) {

      mValign = nsBoxFrame::vAlign_Top;
      mHalign = nsBoxFrame::hAlign_Left;

      bool orient = true;
      GetInitialOrientation(orient); 
      if (orient)
        mState |= NS_STATE_IS_HORIZONTAL;
      else
        mState &= ~NS_STATE_IS_HORIZONTAL;

      bool normal = true;
      GetInitialDirection(normal);
      if (normal)
        mState |= NS_STATE_IS_DIRECTION_NORMAL;
      else
        mState &= ~NS_STATE_IS_DIRECTION_NORMAL;

      GetInitialVAlignment(mValign);
      GetInitialHAlignment(mHalign);

      bool equalSize = false;
      GetInitialEqualSize(equalSize); 
      if (equalSize)
        mState |= NS_STATE_EQUAL_SIZE;
      else
        mState &= ~NS_STATE_EQUAL_SIZE;

#ifdef DEBUG_LAYOUT
      bool debug = mState & NS_STATE_SET_TO_DEBUG;
      bool debugSet = GetInitialDebug(debug);
      if (debugSet) {
        mState |= NS_STATE_DEBUG_WAS_SET;

        if (debug)
          mState |= NS_STATE_SET_TO_DEBUG;
        else
          mState &= ~NS_STATE_SET_TO_DEBUG;
      } else {
        mState &= ~NS_STATE_DEBUG_WAS_SET;
      }
#endif

      bool autostretch = !!(mState & NS_STATE_AUTO_STRETCH);
      GetInitialAutoStretch(autostretch);
      if (autostretch)
        mState |= NS_STATE_AUTO_STRETCH;
      else
        mState &= ~NS_STATE_AUTO_STRETCH;
    }
    else if (aAttribute == nsGkAtoms::left ||
             aAttribute == nsGkAtoms::top ||
             aAttribute == nsGkAtoms::right ||
             aAttribute == nsGkAtoms::bottom ||
             aAttribute == nsGkAtoms::start ||
             aAttribute == nsGkAtoms::end) {
      mState &= ~NS_STATE_STACK_NOT_POSITIONED;
    }
    else if (aAttribute == nsGkAtoms::mousethrough) {
      UpdateMouseThrough();
    }

    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }
  else if (aAttribute == nsGkAtoms::ordinal) {
    nsIFrame* parent = GetParentXULBox(this);
    // If our parent is not a box, there's not much we can do... but in that
    // case our ordinal doesn't matter anyway, so that's ok.
    // Also don't bother with popup frames since they are kept on the 
    // kPopupList and XULRelayoutChildAtOrdinal() only handles
    // principal children.
    if (parent && !(GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
        StyleDisplay()->mDisplay != mozilla::StyleDisplay::MozPopup) {
      parent->XULRelayoutChildAtOrdinal(this);
      // XXXldb Should this instead be a tree change on the child or parent?
      PresContext()->PresShell()->
        FrameNeedsReflow(parent, nsIPresShell::eStyleChange,
                         NS_FRAME_IS_DIRTY);
    }
  }
  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  else if (aAttribute == nsGkAtoms::accesskey) {
    RegUnregAccessKey(true);
  }
  else if (aAttribute == nsGkAtoms::rows &&
           mContent->IsXULElement(nsGkAtoms::tree)) {
    // Reflow ourselves and all our children if "rows" changes, since
    // nsTreeBodyFrame's layout reads this from its parent (this frame).
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }

  return rv;
}

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::GetDebugPref()
{
  gDebug = Preferences::GetBool("xul.debug.box");
}

class nsDisplayXULDebug : public nsDisplayItem {
public:
  nsDisplayXULDebug(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULDebug);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULDebug() {
    MOZ_COUNT_DTOR(nsDisplayXULDebug);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, nsRect aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
    nsPoint rectCenter(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2);
    static_cast<nsBoxFrame*>(mFrame)->
      DisplayDebugInfoFor(this, rectCenter - ToReferenceFrame());
    aOutFrames->AppendElement(this);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("XULDebug", TYPE_XUL_DEBUG)
};

void
nsDisplayXULDebug::Paint(nsDisplayListBuilder* aBuilder,
                         nsRenderingContext* aCtx)
{
  static_cast<nsBoxFrame*>(mFrame)->
    PaintXULDebugOverlay(*aCtx->GetDrawTarget(), ToReferenceFrame());
}

static void
PaintXULDebugBackground(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                        const nsRect& aDirtyRect, nsPoint aPt)
{
  static_cast<nsBoxFrame*>(aFrame)->PaintXULDebugBackground(aDrawTarget, aPt);
}
#endif

void
nsBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                             const nsRect&           aDirtyRect,
                             const nsDisplayListSet& aLists)
{
  bool forceLayer = false;

  if (GetContent()->IsXULElement()) {
    // forcelayer is only supported on XUL elements with box layout
    if (GetContent()->HasAttr(kNameSpaceID_None, nsGkAtoms::layer)) {
      forceLayer = true;
    }
    // Check for frames that are marked as a part of the region used
    // in calculating glass margins on Windows.
    const nsStyleDisplay* styles = StyleDisplay();
    if (styles && styles->UsedAppearance() == NS_THEME_WIN_EXCLUDE_GLASS) {
      aBuilder->AddWindowExcludeGlassRegion(
          nsRect(aBuilder->ToReferenceFrame(this), GetSize()));
    }
  }

  nsDisplayListCollection tempLists;
  const nsDisplayListSet& destination = forceLayer ? tempLists : aLists;

  DisplayBorderBackgroundOutline(aBuilder, destination);

#ifdef DEBUG_LAYOUT
  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
    destination.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(aBuilder, this, PaintXULDebugBackground,
                       "XULDebugBackground"));
    destination.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayXULDebug(aBuilder, this));
  }
#endif

  Maybe<nsDisplayListBuilder::AutoContainerASRTracker> contASRTracker;
  if (forceLayer) {
    contASRTracker.emplace(aBuilder);
  }

  BuildDisplayListForChildren(aBuilder, aDirtyRect, destination);

  // see if we have to draw a selection frame around this container
  DisplaySelectionOverlay(aBuilder, destination.Content());

  if (forceLayer) {
    // This is a bit of a hack. Collect up all descendant display items
    // and merge them into a single Content() list. This can cause us
    // to violate CSS stacking order, but forceLayer is a magic
    // XUL-only extension anyway.
    nsDisplayList masterList;
    masterList.AppendToTop(tempLists.BorderBackground());
    masterList.AppendToTop(tempLists.BlockBorderBackgrounds());
    masterList.AppendToTop(tempLists.Floats());
    masterList.AppendToTop(tempLists.Content());
    masterList.AppendToTop(tempLists.PositionedDescendants());
    masterList.AppendToTop(tempLists.Outlines());

    const ActiveScrolledRoot* ownLayerASR = contASRTracker->GetContainerASR();

    DisplayListClipState::AutoSaveRestore ownLayerClipState(aBuilder);
    ownLayerClipState.ClearUpToASR(ownLayerASR);

    // Wrap the list to make it its own layer
    aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayOwnLayer(aBuilder, this, &masterList, ownLayerASR));
  }
}

void
nsBoxFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                        const nsRect&           aDirtyRect,
                                        const nsDisplayListSet& aLists)
{
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background onto the BlockBorderBackgrounds list
  // to emulate the existing two-layer XUL painting scheme.
  nsDisplayListSet set(aLists, aLists.BlockBorderBackgrounds());
  // The children should be in the right order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set);
    kid = kid->GetNextSibling();
  }
}

// REVIEW: PaintChildren did a few things none of which are a big deal
// anymore:
// * Paint some debugging rects for this frame.
// This is done by nsDisplayXULDebugBackground, which goes in the
// BorderBackground() layer so it isn't clipped by OVERFLOW_CLIP.
// * Apply OVERFLOW_CLIP to the children.
// This is now in nsFrame::BuildDisplayListForStackingContext/Child.
// * Actually paint the children.
// Moved to BuildDisplayList.
// * Paint per-kid debug information.
// This is done by nsDisplayXULDebug, which is in the Outlines()
// layer so it goes on top. This means it is not clipped by OVERFLOW_CLIP,
// whereas it did used to respect OVERFLOW_CLIP, but too bad.
#ifdef DEBUG_LAYOUT
void
nsBoxFrame::PaintXULDebugBackground(DrawTarget* aDrawTarget, nsPoint aPt)
{
  nsMargin border;
  GetXULBorder(border);

  nsMargin debugBorder;
  nsMargin debugMargin;
  nsMargin debugPadding;

  bool isHorizontal = IsXULHorizontal();

  GetDebugBorder(debugBorder);
  PixelMarginToTwips(debugBorder);

  GetDebugMargin(debugMargin);
  PixelMarginToTwips(debugMargin);

  GetDebugPadding(debugPadding);
  PixelMarginToTwips(debugPadding);

  nsRect inner(mRect);
  inner.MoveTo(aPt);
  inner.Deflate(debugMargin);
  inner.Deflate(border);
  //nsRect borderRect(inner);

  int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();

  ColorPattern color(ToDeviceColor(isHorizontal ? Color(0.f, 0.f, 1.f, 1.f) :
                                                  Color(1.f, 0.f, 0.f, 1.f)));

  //left
  nsRect r(inner);
  r.width = debugBorder.left;
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), color);

  // top
  r = inner;
  r.height = debugBorder.top;
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), color);

  //right
  r = inner;
  r.x = r.x + r.width - debugBorder.right;
  r.width = debugBorder.right;
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), color);

  //bottom
  r = inner;
  r.y = r.y + r.height - debugBorder.bottom;
  r.height = debugBorder.bottom;
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), color);

  // If we have dirty children or we are dirty place a green border around us.
  if (NS_SUBTREE_DIRTY(this)) {
    nsRect dirty(inner);
    ColorPattern green(ToDeviceColor(Color(0.f, 1.f, 0.f, 1.f)));
    aDrawTarget->StrokeRect(NSRectToRect(dirty, appUnitsPerDevPixel), green);
  }
}

void
nsBoxFrame::PaintXULDebugOverlay(DrawTarget& aDrawTarget, nsPoint aPt)
{
  nsMargin border;
  GetXULBorder(border);

  nsMargin debugMargin;
  GetDebugMargin(debugMargin);
  PixelMarginToTwips(debugMargin);

  nsRect inner(mRect);
  inner.MoveTo(aPt);
  inner.Deflate(debugMargin);
  inner.Deflate(border);

  nscoord onePixel = GetPresContext()->IntScaledPixelsToTwips(1);

  kid = nsBox::GetChildXULBox(this);
  while (nullptr != kid) {
    bool isHorizontal = IsXULHorizontal();

    nscoord x, y, borderSize, spacerSize;
    
    nsRect cr(kid->mRect);
    nsMargin margin;
    kid->GetXULMargin(margin);
    cr.Inflate(margin);
    
    if (isHorizontal) 
    {
        cr.y = inner.y;
        x = cr.x;
        y = cr.y + onePixel;
        spacerSize = debugBorder.top - onePixel*4;
    } else {
        cr.x = inner.x;
        x = cr.y;
        y = cr.x + onePixel;
        spacerSize = debugBorder.left - onePixel*4;
    }

    nscoord flex = kid->GetXULFlex();

    if (!kid->IsXULCollapsed()) {
      if (isHorizontal) 
          borderSize = cr.width;
      else 
          borderSize = cr.height;

      DrawSpacer(GetPresContext(), aDrawTarget, isHorizontal, flex, x, y, borderSize, spacerSize);
    }

    kid = GetNextXULBox(kid);
  }
}
#endif

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::GetBoxName(nsAutoString& aName)
{
   GetFrameName(aName);
}
#endif

#ifdef DEBUG_FRAME_DUMP
nsresult
nsBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Box"), aResult);
}
#endif

#ifdef DEBUG_LAYOUT
nsresult
nsBoxFrame::GetXULDebug(bool& aDebug)
{
  aDebug = (mState & NS_STATE_CURRENTLY_IN_DEBUG);
  return NS_OK;
}
#endif

// REVIEW: nsBoxFrame::GetFrameForPoint is a problem because of 'mousethrough'
// attribute support. Here's how it works:
// * For each child frame F, we determine the target frame T(F) by recursively
// invoking GetFrameForPoint on the child
// * Let F' be the last child frame such that T(F') doesn't have mousethrough.
// If F' exists, return T(F')
// * Otherwise let F'' be the first child frame such that T(F'') is non-null.
// If F'' exists, return T(F'')
// * Otherwise return this frame, if this frame contains the point
// * Otherwise return null
// It's not clear how this should work for more complex z-ordering situations.
// The basic principle seems to be that if a frame F has a descendant
// 'mousethrough' frame that includes the target position, then F
// will not receive events (unless it overrides GetFrameForPoint).
// A 'mousethrough' frame will only receive an event if, after applying that rule,
// all eligible frames are 'mousethrough'; the bottom-most inner-most 'mousethrough'
// frame is then chosen (the first eligible frame reached in a
// traversal of the frame tree --- pre/post is irrelevant since ancestors
// of the mousethrough frames can't be eligible).
// IMHO this is very bogus and adds a great deal of complexity for something
// that is very rarely used. So I'm redefining 'mousethrough' to the following:
// a frame with mousethrough is transparent to mouse events. This is compatible
// with the way 'mousethrough' is used in Seamonkey's navigator.xul and
// Firefox's browser.xul. The only other place it's used is in the 'expander'
// XBL binding, which in our tree is only used by Thunderbird SMIME Advanced
// Preferences, and I can't figure out what that does, so I'll have to test it.
// If it's broken I'll probably just change the binding to use it more sensibly.
// This new behaviour is implemented in nsDisplayList::HitTest. 
// REVIEW: This debug-box stuff is annoying. I'm just going to put debug boxes
// in the outline layer and avoid GetDebugBoxAt.

// REVIEW: GetCursor had debug-only event dumping code. I have replaced it
// with instrumentation in nsDisplayXULDebug.

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::DrawLine(DrawTarget& aDrawTarget, bool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2)
{
    nsPoint p1(x1, y1);
    nsPoint p2(x2, y2);
    if (!aHorizontal) {
      Swap(p1.x, p1.y);
      Swap(p2.x, p2.y);
    }
    ColorPattern white(ToDeviceColor(Color(1.f, 1.f, 1.f, 1.f)));
    StrokeLineWithSnapping(p1, p2, PresContext()->AppUnitsPerDevPixel(),
                           aDrawTarget, color);
}

void
nsBoxFrame::FillRect(DrawTarget& aDrawTarget, bool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height)
{
    Rect rect = NSRectToSnappedRect(aHorizontal ? nsRect(x, y, width, height) :
                                                  nsRect(y, x, height, width),
                                    PresContext()->AppUnitsPerDevPixel(),
                                    aDrawTarget);
    ColorPattern white(ToDeviceColor(Color(1.f, 1.f, 1.f, 1.f)));
    aDrawTarget.FillRect(rect, white);
}

void 
nsBoxFrame::DrawSpacer(nsPresContext* aPresContext, DrawTarget& aDrawTarget,
                       bool aHorizontal, int32_t flex, nscoord x, nscoord y,
                       nscoord size, nscoord spacerSize)
{    
         nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);

     // if we do draw the coils
        int distance = 0;
        int center = 0;
        int offset = 0;
        int coilSize = COIL_SIZE*onePixel;
        int halfSpacer = spacerSize/2;

        distance = size;
        center = y + halfSpacer;
        offset = x;

        int coils = distance/coilSize;

        int halfCoilSize = coilSize/2;

        if (flex == 0) {
            DrawLine(aDrawTarget, aHorizontal, x,y + spacerSize/2, x + size, y + spacerSize/2);
        } else {
            for (int i=0; i < coils; i++)
            {
                   DrawLine(aDrawTarget, aHorizontal, offset, center+halfSpacer, offset+halfCoilSize, center-halfSpacer);
                   DrawLine(aDrawTarget, aHorizontal, offset+halfCoilSize, center-halfSpacer, offset+coilSize, center+halfSpacer);

                   offset += coilSize;
            }
        }

        FillRect(aDrawTarget, aHorizontal, x + size - spacerSize/2, y, spacerSize/2, spacerSize);
        FillRect(aDrawTarget, aHorizontal, x, y, spacerSize/2, spacerSize);
}

void
nsBoxFrame::GetDebugBorder(nsMargin& aInset)
{
    aInset.SizeTo(2,2,2,2);

    if (IsXULHorizontal()) 
       aInset.top = 10;
    else 
       aInset.left = 10;
}

void
nsBoxFrame::GetDebugMargin(nsMargin& aInset)
{
    aInset.SizeTo(2,2,2,2);
}

void
nsBoxFrame::GetDebugPadding(nsMargin& aPadding)
{
    aPadding.SizeTo(2,2,2,2);
}

void 
nsBoxFrame::PixelMarginToTwips(nsMargin& aMarginPixels)
{
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  aMarginPixels.left   *= onePixel;
  aMarginPixels.right  *= onePixel;
  aMarginPixels.top    *= onePixel;
  aMarginPixels.bottom *= onePixel;
}

void
nsBoxFrame::GetValue(nsPresContext* aPresContext, const nsSize& a, const nsSize& b, char* ch) 
{
    float p2t = aPresContext->ScaledPixelsToTwips();

    char width[100];
    char height[100];
    
    if (a.width == NS_INTRINSICSIZE)
        sprintf(width,"%s","INF");
    else
        sprintf(width,"%d", nscoord(a.width/*/p2t*/));
    
    if (a.height == NS_INTRINSICSIZE)
        sprintf(height,"%s","INF");
    else 
        sprintf(height,"%d", nscoord(a.height/*/p2t*/));
    

    sprintf(ch, "(%s%s, %s%s)", width, (b.width != NS_INTRINSICSIZE ? "[SET]" : ""),
                    height, (b.height != NS_INTRINSICSIZE ? "[SET]" : ""));

}

void
nsBoxFrame::GetValue(nsPresContext* aPresContext, int32_t a, int32_t b, char* ch) 
{
    if (a == NS_INTRINSICSIZE)
      sprintf(ch, "%d[SET]", b);             
    else
      sprintf(ch, "%d", a);             
}

nsresult
nsBoxFrame::DisplayDebugInfoFor(nsIFrame*  aBox,
                                nsPoint& aPoint)
{
    nsBoxLayoutState state(GetPresContext());

    nscoord x = aPoint.x;
    nscoord y = aPoint.y;

    // get the area inside our border but not our debug margins.
    nsRect insideBorder(aBox->mRect);
    insideBorder.MoveTo(0,0):
    nsMargin border(0,0,0,0);
    aBox->GetXULBorderAndPadding(border);
    insideBorder.Deflate(border);

    bool isHorizontal = IsXULHorizontal();

    if (!insideBorder.Contains(nsPoint(x,y)))
        return NS_ERROR_FAILURE;

    //printf("%%%%%% inside box %%%%%%%\n");

    int count = 0;
    nsIFrame* child = nsBox::GetChildXULBox(aBox);

    nsMargin m;
    nsMargin m2;
    GetDebugBorder(m);
    PixelMarginToTwips(m);

    GetDebugMargin(m2);
    PixelMarginToTwips(m2);

    m += m2;

    if ((isHorizontal && y < insideBorder.y + m.top) ||
        (!isHorizontal && x < insideBorder.x + m.left)) {
        //printf("**** inside debug border *******\n");
        while (child) 
        {
            const nsRect& r = child->mRect;

            // if we are not in the child. But in the spacer above the child.
            if ((isHorizontal && x >= r.x && x < r.x + r.width) ||
                (!isHorizontal && y >= r.y && y < r.y + r.height)) {
                aCursor = NS_STYLE_CURSOR_POINTER;
                   // found it but we already showed it.
                    if (mDebugChild == child)
                        return NS_OK;

                    if (aBox->GetContent()) {
                      printf("---------------\n");
                      XULDumpBox(stdout);
                      printf("\n");
                    }

                    if (child->GetContent()) {
                        printf("child #%d: ", count);
                        child->XULDumpBox(stdout);
                        printf("\n");
                    }

                    mDebugChild = child;

                    nsSize prefSizeCSS(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                    nsSize minSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                    nsSize maxSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                    nscoord flexCSS = NS_INTRINSICSIZE;

                    bool widthSet, heightSet;
                    nsIFrame::AddXULPrefSize(child, prefSizeCSS, widthSet, heightSet);
                    nsIFrame::AddXULMinSize (state, child, minSizeCSS, widthSet, heightSet);
                    nsIFrame::AddXULMaxSize (child, maxSizeCSS, widthSet, heightSet);
                    nsIFrame::AddXULFlex    (child, flexCSS);

                    nsSize prefSize = child->GetXULPrefSize(state);
                    nsSize minSize = child->GetXULMinSize(state);
                    nsSize maxSize = child->GetXULMaxSize(state);
                    nscoord flexSize = child->GetXULFlex();
                    nscoord ascentSize = child->GetXULBoxAscent(state);

                    char min[100];
                    char pref[100];
                    char max[100];
                    char calc[100];
                    char flex[100];
                    char ascent[100];
                  
                    nsSize actualSize;
                    GetFrameSizeWithMargin(child, actualSize);
                    nsSize actualSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);

                    GetValue(aPresContext, minSize,  minSizeCSS, min);
                    GetValue(aPresContext, prefSize, prefSizeCSS, pref);
                    GetValue(aPresContext, maxSize,  maxSizeCSS, max);
                    GetValue(aPresContext, actualSize, actualSizeCSS, calc);
                    GetValue(aPresContext, flexSize,  flexCSS, flex);
                    GetValue(aPresContext, ascentSize,  NS_INTRINSICSIZE, ascent);


                    printf("min%s, pref%s, max%s, actual%s, flex=%s, ascent=%s\n\n", 
                        min,
                        pref,
                        max,
                        calc,
                        flex,
                        ascent
                    );

                    return NS_OK;   
            }

          child = GetNextXULBox(child);
          count++;
        }
    } else {
    }

    mDebugChild = nullptr;

    return NS_OK;
}

void
nsBoxFrame::SetDebugOnChildList(nsBoxLayoutState& aState, nsIFrame* aChild, bool aDebug)
{
    nsIFrame* child = nsBox::GetChildXULBox(this);
     while (child)
     {
        child->SetXULDebug(aState, aDebug);
        child = GetNextXULBox(child);
     }
}

nsresult
nsBoxFrame::GetFrameSizeWithMargin(nsIFrame* aBox, nsSize& aSize)
{
  nsRect rect(aBox->GetRect());
  nsMargin margin(0,0,0,0);
  aBox->GetXULMargin(margin);
  rect.Inflate(margin);
  aSize.width = rect.width;
  aSize.height = rect.height;
  return NS_OK;
}
#endif

// If you make changes to this function, check its counterparts
// in nsTextBoxFrame and nsXULLabelFrame
void
nsBoxFrame::RegUnregAccessKey(bool aDoReg)
{
  MOZ_ASSERT(mContent);

  // only support accesskeys for the following elements
  if (!mContent->IsAnyOfXULElements(nsGkAtoms::button,
                                    nsGkAtoms::toolbarbutton,
                                    nsGkAtoms::checkbox,
                                    nsGkAtoms::textbox,
                                    nsGkAtoms::tab,
                                    nsGkAtoms::radio)) {
    return;
  }

  nsAutoString accessKey;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);

  if (accessKey.IsEmpty())
    return;

  // With a valid PresContext we can get the ESM 
  // and register the access key
  EventStateManager* esm = PresContext()->EventStateManager();

  uint32_t key = accessKey.First();
  if (aDoReg)
    esm->RegisterAccessKey(mContent, key);
  else
    esm->UnregisterAccessKey(mContent, key);
}

bool
nsBoxFrame::SupportsOrdinalsInChildren()
{
  return true;
}

// Helper less-than-or-equal function, used in CheckBoxOrder() as a
// template-parameter for the sorting functions.
bool
IsBoxOrdinalLEQ(nsIFrame* aFrame1,
                nsIFrame* aFrame2)
{
  // If we've got a placeholder frame, use its out-of-flow frame's ordinal val.
  nsIFrame* aRealFrame1 = nsPlaceholderFrame::GetRealFrameFor(aFrame1);
  nsIFrame* aRealFrame2 = nsPlaceholderFrame::GetRealFrameFor(aFrame2);
  return aRealFrame1->GetXULOrdinal() <= aRealFrame2->GetXULOrdinal();
}

void 
nsBoxFrame::CheckBoxOrder()
{
  if (SupportsOrdinalsInChildren() &&
      !nsIFrame::IsFrameListSorted<IsBoxOrdinalLEQ>(mFrames)) {
    nsIFrame::SortFrameList<IsBoxOrdinalLEQ>(mFrames);
  }
}

nsresult
nsBoxFrame::LayoutChildAt(nsBoxLayoutState& aState, nsIFrame* aBox, const nsRect& aRect)
{
  // get the current rect
  nsRect oldRect(aBox->GetRect());
  aBox->SetXULBounds(aState, aRect);

  bool layout = NS_SUBTREE_DIRTY(aBox);
  
  if (layout || (oldRect.width != aRect.width || oldRect.height != aRect.height))  {
    return aBox->XULLayout(aState);
  }

  return NS_OK;
}

nsresult
nsBoxFrame::XULRelayoutChildAtOrdinal(nsIFrame* aChild)
{
  if (!SupportsOrdinalsInChildren())
    return NS_OK;

  uint32_t ord = aChild->GetXULOrdinal();
  
  nsIFrame* child = mFrames.FirstChild();
  nsIFrame* newPrevSib = nullptr;

  while (child) {
    if (ord < child->GetXULOrdinal()) {
      break;
    }

    if (child != aChild) {
      newPrevSib = child;
    }

    child = GetNextXULBox(child);
  }

  if (aChild->GetPrevSibling() == newPrevSib) {
    // This box is not moving.
    return NS_OK;
  }

  // Take |aChild| out of its old position in the child list.
  mFrames.RemoveFrame(aChild);

  // Insert it after |newPrevSib| or at the start if it's null.
  mFrames.InsertFrame(nullptr, newPrevSib, aChild);

  return NS_OK;
}

/**
 * This wrapper class lets us redirect mouse hits from descendant frames
 * of a menu to the menu itself, if they didn't specify 'allowevents'.
 * 
 * The wrapper simply turns a hit on a descendant element
 * into a hit on the menu itself, unless there is an element between the target
 * and the menu with the "allowevents" attribute.
 * 
 * This is used by nsMenuFrame and nsTreeColFrame.
 * 
 * Note that turning a hit on a descendant element into nullptr, so events
 * could fall through to the menu background, might be an appealing simplification
 * but it would mean slightly strange behaviour in some cases, because grabber
 * wrappers can be created for many individual lists and items, so the exact
 * fallthrough behaviour would be complex. E.g. an element with "allowevents"
 * on top of the Content() list could receive the event even if it was covered
 * by a PositionedDescenants() element without "allowevents". It is best to
 * never convert a non-null hit into null.
 */
// REVIEW: This is roughly of what nsMenuFrame::GetFrameForPoint used to do.
// I've made 'allowevents' affect child elements because that seems the only
// reasonable thing to do.
class nsDisplayXULEventRedirector final : public nsDisplayWrapList
{
public:
  nsDisplayXULEventRedirector(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame, nsDisplayItem* aItem,
                              nsIFrame* aTargetFrame)
    : nsDisplayWrapList(aBuilder, aFrame, aItem), mTargetFrame(aTargetFrame) {}
  nsDisplayXULEventRedirector(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame, nsDisplayList* aList,
                              nsIFrame* aTargetFrame)
    : nsDisplayWrapList(aBuilder, aFrame, aList), mTargetFrame(aTargetFrame) {}
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }
  NS_DISPLAY_DECL_NAME("XULEventRedirector", TYPE_XUL_EVENT_REDIRECTOR)
private:
  nsIFrame* mTargetFrame;
};

void nsDisplayXULEventRedirector::HitTest(nsDisplayListBuilder* aBuilder,
    const nsRect& aRect, HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsTArray<nsIFrame*> outFrames;
  mList.HitTest(aBuilder, aRect, aState, &outFrames);

  bool topMostAdded = false;
  uint32_t localLength = outFrames.Length();

  for (uint32_t i = 0; i < localLength; i++) {

    for (nsIContent* content = outFrames.ElementAt(i)->GetContent();
         content && content != mTargetFrame->GetContent();
         content = content->GetParent()) {
      if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::allowevents,
                               nsGkAtoms::_true, eCaseMatters)) {
        // Events are allowed on 'frame', so let it go.
        aOutFrames->AppendElement(outFrames.ElementAt(i));
        topMostAdded = true;
      }
    }

    // If there was no hit on the topmost frame or its ancestors,
    // add the target frame itself as the first candidate (see bug 562554).
    if (!topMostAdded) {
      topMostAdded = true;
      aOutFrames->AppendElement(mTargetFrame);
    }
  }
}

class nsXULEventRedirectorWrapper final : public nsDisplayWrapper
{
public:
  explicit nsXULEventRedirectorWrapper(nsIFrame* aTargetFrame)
      : mTargetFrame(aTargetFrame) {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame,
                                  nsDisplayList* aList) override {
    return new (aBuilder)
        nsDisplayXULEventRedirector(aBuilder, aFrame, aList, mTargetFrame);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) override {
    return new (aBuilder)
        nsDisplayXULEventRedirector(aBuilder, aItem->Frame(), aItem,
                                    mTargetFrame);
  }
private:
  nsIFrame* mTargetFrame;
};

void
nsBoxFrame::WrapListsInRedirector(nsDisplayListBuilder*   aBuilder,
                                  const nsDisplayListSet& aIn,
                                  const nsDisplayListSet& aOut)
{
  nsXULEventRedirectorWrapper wrapper(this);
  wrapper.WrapLists(aBuilder, this, aIn, aOut);
}

bool
nsBoxFrame::GetEventPoint(WidgetGUIEvent* aEvent, nsPoint &aPoint) {
  LayoutDeviceIntPoint refPoint;
  bool res = GetEventPoint(aEvent, refPoint);
  aPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(
    aEvent, refPoint, this);
  return res;
}

bool
nsBoxFrame::GetEventPoint(WidgetGUIEvent* aEvent, LayoutDeviceIntPoint& aPoint) {
  NS_ENSURE_TRUE(aEvent, false);

  WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
  if (touchEvent) {
    // return false if there is more than one touch on the page, or if
    // we can't find a touch point
    if (touchEvent->mTouches.Length() != 1) {
      return false;
    }

    dom::Touch* touch = touchEvent->mTouches.SafeElementAt(0);
    if (!touch) {
      return false;
    }
    aPoint = touch->mRefPoint;
  } else {
    aPoint = aEvent->mRefPoint;
  }
  return true;
}
