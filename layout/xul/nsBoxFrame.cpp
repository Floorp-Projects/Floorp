/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
// Boxes layout a bit differently than html. html does a bottom up layout. Where
// boxes do a top down.
//
// 1) First thing a box does it goes out and askes each child for its min, max,
//    and preferred sizes.
//
// 2) It then adds them up to determine its size.
//
// 3) If the box was asked to layout it self intrinically it will layout its
//    children at their preferred size otherwise it will layout the child at
//    the size it was told to. It will squeeze or stretch its children if
//    Necessary.
//
// However there is a catch. Some html components like block frames can not
// determine their preferred size. this is their size if they were laid out
// intrinsically. So the box will flow the child to determine this can cache the
// value.

// Boxes and Incremental Reflow
// ----------------------------
// Boxes layout out top down by adding up their children's min, max, and
// preferred sizes. Only problem is if a incremental reflow occurs. The
// preferred size of a child deep in the hierarchy could change. And this could
// change any number of syblings around the box. Basically any children in the
// reflow chain must have their caches cleared so when asked for there current
// size they can relayout themselves.

#include "nsBoxFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsBoxLayoutState.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/Move.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/PresShell.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsHTMLParts.h"
#include "nsViewManager.h"
#include "nsView.h"
#include "nsCSSRendering.h"
#include "nsIServiceManager.h"
#include "nsBoxLayout.h"
#include "nsSprocketLayout.h"
#include "nsIScrollableFrame.h"
#include "nsWidgetsCID.h"
#include "nsCSSAnonBoxes.h"
#include "nsContainerFrame.h"
#include "nsITheme.h"
#include "nsTransform2D.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsDisplayList.h"
#include "mozilla/Preferences.h"
#include "nsStyleConsts.h"
#include "nsLayoutUtils.h"
#include "nsSliderFrame.h"
#include <algorithm>

// Needed for Print Preview
#include "nsIURI.h"

#include "mozilla/TouchEvents.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

nsIFrame* NS_NewBoxFrame(PresShell* aPresShell, ComputedStyle* aStyle,
                         bool aIsRoot, nsBoxLayout* aLayoutManager) {
  return new (aPresShell)
      nsBoxFrame(aStyle, aPresShell->GetPresContext(), nsBoxFrame::kClassID,
                 aIsRoot, aLayoutManager);
}

nsIFrame* NS_NewBoxFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsBoxFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsBoxFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsBoxFrame)
  NS_QUERYFRAME_ENTRY(nsBoxFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)
#endif

nsBoxFrame::nsBoxFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                       ClassID aID, bool aIsRoot, nsBoxLayout* aLayoutManager)
    : nsContainerFrame(aStyle, aPresContext, aID), mFlex(0), mAscent(0) {
  AddStateBits(NS_STATE_IS_HORIZONTAL | NS_STATE_AUTO_STRETCH);

  if (aIsRoot) AddStateBits(NS_STATE_IS_ROOT);

  mValign = vAlign_Top;
  mHalign = hAlign_Left;

  // if no layout manager specified us the static sprocket layout
  nsCOMPtr<nsBoxLayout> layout = aLayoutManager;

  if (layout == nullptr) {
    NS_NewSprocketLayout(layout);
  }

  SetXULLayoutManager(layout);
}

nsBoxFrame::~nsBoxFrame() {}

void nsBoxFrame::SetInitialChildList(ChildListID aListID,
                                     nsFrameList& aChildList) {
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  if (aListID == kPrincipalList) {
    // initialize our list of infos.
    nsBoxLayoutState state(PresContext());
    CheckBoxOrder();
    if (mLayoutManager)
      mLayoutManager->ChildrenSet(this, state, mFrames.FirstChild());
  }
}

/* virtual */
void nsBoxFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  nsContainerFrame::DidSetComputedStyle(aOldComputedStyle);

  // The values that CacheAttributes() computes depend on our style,
  // so we need to recompute them here...
  CacheAttributes();
}

/**
 * Initialize us. This is a good time to get the alignment of the box
 */
void nsBoxFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                      nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  MarkIntrinsicISizesDirty();

  CacheAttributes();

  UpdateMouseThrough();

  // register access key
  RegUnregAccessKey(true);
}

void nsBoxFrame::UpdateMouseThrough() {
  static Element::AttrValuesArray strings[] = {nsGkAtoms::never,
                                               nsGkAtoms::always, nullptr};
  switch (mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::mousethrough, strings, eCaseMatters)) {
    case 0:
      AddStateBits(NS_FRAME_MOUSE_THROUGH_NEVER);
      break;
    case 1:
      AddStateBits(NS_FRAME_MOUSE_THROUGH_ALWAYS);
      break;
    case 2: {
      RemoveStateBits(NS_FRAME_MOUSE_THROUGH_ALWAYS);
      RemoveStateBits(NS_FRAME_MOUSE_THROUGH_NEVER);
      break;
    }
  }
}

void nsBoxFrame::CacheAttributes() {
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
    AddStateBits(NS_STATE_IS_HORIZONTAL);
  else
    RemoveStateBits(NS_STATE_IS_HORIZONTAL);

  bool normal = true;
  GetInitialDirection(normal);
  if (normal)
    AddStateBits(NS_STATE_IS_DIRECTION_NORMAL);
  else
    RemoveStateBits(NS_STATE_IS_DIRECTION_NORMAL);

  GetInitialVAlignment(mValign);
  GetInitialHAlignment(mHalign);

  bool equalSize = false;
  GetInitialEqualSize(equalSize);
  if (equalSize)
    AddStateBits(NS_STATE_EQUAL_SIZE);
  else
    RemoveStateBits(NS_STATE_EQUAL_SIZE);

  bool autostretch = !!(mState & NS_STATE_AUTO_STRETCH);
  GetInitialAutoStretch(autostretch);
  if (autostretch)
    AddStateBits(NS_STATE_AUTO_STRETCH);
  else
    RemoveStateBits(NS_STATE_AUTO_STRETCH);
}

bool nsBoxFrame::GetInitialHAlignment(nsBoxFrame::Halignment& aHalign) {
  if (!GetContent() || !GetContent()->IsElement()) return false;

  Element* element = GetContent()->AsElement();
  // XXXdwh Everything inside this if statement is deprecated code.
  static Element::AttrValuesArray alignStrings[] = {nsGkAtoms::left,
                                                    nsGkAtoms::right, nullptr};
  static const Halignment alignValues[] = {hAlign_Left, hAlign_Right};
  int32_t index = element->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::align,
                                           alignStrings, eCaseMatters);
  if (index >= 0) {
    aHalign = alignValues[index];
    return true;
  }

  // Now that the deprecated stuff is out of the way, we move on to check the
  // appropriate attribute.  For horizontal boxes, we are checking the PACK
  // attribute.  For vertical boxes we are checking the ALIGN attribute.
  nsAtom* attrName = IsXULHorizontal() ? nsGkAtoms::pack : nsGkAtoms::align;
  static Element::AttrValuesArray strings[] = {
      nsGkAtoms::_empty, nsGkAtoms::start, nsGkAtoms::center, nsGkAtoms::end,
      nullptr};
  static const Halignment values[] = {hAlign_Left /*not used*/, hAlign_Left,
                                      hAlign_Center, hAlign_Right};
  index = element->FindAttrValueIn(kNameSpaceID_None, attrName, strings,
                                   eCaseMatters);

  if (index == Element::ATTR_VALUE_NO_MATCH) {
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
      default:  // Nonsensical value. Just bail.
        return false;
    }
  } else {
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
      default:  // Nonsensical value. Just bail.
        return false;
    }
  }

  return false;
}

bool nsBoxFrame::GetInitialVAlignment(nsBoxFrame::Valignment& aValign) {
  if (!GetContent() || !GetContent()->IsElement()) return false;

  Element* element = GetContent()->AsElement();

  static Element::AttrValuesArray valignStrings[] = {
      nsGkAtoms::top, nsGkAtoms::baseline, nsGkAtoms::middle, nsGkAtoms::bottom,
      nullptr};
  static const Valignment valignValues[] = {vAlign_Top, vAlign_BaseLine,
                                            vAlign_Middle, vAlign_Bottom};
  int32_t index = element->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::valign,
                                           valignStrings, eCaseMatters);
  if (index >= 0) {
    aValign = valignValues[index];
    return true;
  }

  // Now that the deprecated stuff is out of the way, we move on to check the
  // appropriate attribute.  For horizontal boxes, we are checking the ALIGN
  // attribute.  For vertical boxes we are checking the PACK attribute.
  nsAtom* attrName = IsXULHorizontal() ? nsGkAtoms::align : nsGkAtoms::pack;
  static Element::AttrValuesArray strings[] = {
      nsGkAtoms::_empty,   nsGkAtoms::start, nsGkAtoms::center,
      nsGkAtoms::baseline, nsGkAtoms::end,   nullptr};
  static const Valignment values[] = {vAlign_Top /*not used*/, vAlign_Top,
                                      vAlign_Middle, vAlign_BaseLine,
                                      vAlign_Bottom};
  index = element->FindAttrValueIn(kNameSpaceID_None, attrName, strings,
                                   eCaseMatters);
  if (index == Element::ATTR_VALUE_NO_MATCH) {
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
      default:  // Nonsensical value. Just bail.
        return false;
    }
  } else {
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
      default:  // Nonsensical value. Just bail.
        return false;
    }
  }

  return false;
}

void nsBoxFrame::GetInitialOrientation(bool& aIsHorizontal) {
  // see if we are a vertical or horizontal box.
  if (!GetContent()) return;

  // Check the style system first.
  const nsStyleXUL* boxInfo = StyleXUL();
  if (boxInfo->mBoxOrient == StyleBoxOrient::Horizontal) {
    aIsHorizontal = true;
  } else {
    aIsHorizontal = false;
  }

  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  if (!GetContent()->IsElement()) return;

  static Element::AttrValuesArray strings[] = {nsGkAtoms::vertical,
                                               nsGkAtoms::horizontal, nullptr};
  int32_t index = GetContent()->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::orient, strings, eCaseMatters);
  if (index >= 0) {
    aIsHorizontal = index == 1;
  }
}

void nsBoxFrame::GetInitialDirection(bool& aIsNormal) {
  if (!GetContent()) return;

  if (IsXULHorizontal()) {
    // For horizontal boxes only, we initialize our value based off the CSS
    // 'direction' property. This means that BiDI users will end up with
    // horizontally inverted chrome.
    aIsNormal = (StyleVisibility()->mDirection ==
                 NS_STYLE_DIRECTION_LTR);  // If text runs RTL then so do we.
  } else
    aIsNormal = true;  // Assume a normal direction in the vertical case.

  // Now check the style system to see if we should invert aIsNormal.
  const nsStyleXUL* boxInfo = StyleXUL();
  if (boxInfo->mBoxDirection == StyleBoxDirection::Reverse) {
    aIsNormal = !aIsNormal;  // Invert our direction.
  }

  if (!GetContent()->IsElement()) {
    return;
  }

  Element* element = GetContent()->AsElement();

  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  if (IsXULHorizontal()) {
    static Element::AttrValuesArray strings[] = {
        nsGkAtoms::reverse, nsGkAtoms::ltr, nsGkAtoms::rtl, nullptr};
    int32_t index = element->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::dir,
                                             strings, eCaseMatters);
    if (index >= 0) {
      bool values[] = {!aIsNormal, true, false};
      aIsNormal = values[index];
    }
  } else if (element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                  nsGkAtoms::reverse, eCaseMatters)) {
    aIsNormal = !aIsNormal;
  }
}

/* Returns true if it was set.
 */
bool nsBoxFrame::GetInitialEqualSize(bool& aEqualSize) {
  // see if we are a vertical or horizontal box.
  if (!GetContent() || !GetContent()->IsElement()) return false;

  if (GetContent()->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::equalsize,
                                             nsGkAtoms::always, eCaseMatters)) {
    aEqualSize = true;
    return true;
  }

  return false;
}

/* Returns true if it was set.
 */
bool nsBoxFrame::GetInitialAutoStretch(bool& aStretch) {
  if (!GetContent()) return false;

  // Check the align attribute.
  if (GetContent()->IsElement()) {
    static Element::AttrValuesArray strings[] = {nsGkAtoms::_empty,
                                                 nsGkAtoms::stretch, nullptr};
    int32_t index = GetContent()->AsElement()->FindAttrValueIn(
        kNameSpaceID_None, nsGkAtoms::align, strings, eCaseMatters);
    if (index != Element::ATTR_MISSING && index != 0) {
      aStretch = index == 1;
      return true;
    }
  }

  // Check the CSS box-align property.
  const nsStyleXUL* boxInfo = StyleXUL();
  aStretch = (boxInfo->mBoxAlign == StyleBoxAlign::Stretch);

  return true;
}

void nsBoxFrame::DidReflow(nsPresContext* aPresContext,
                           const ReflowInput* aReflowInput) {
  nsFrameState preserveBits =
      mState & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
  nsFrame::DidReflow(aPresContext, aReflowInput);
  AddStateBits(preserveBits);
  if (preserveBits & NS_FRAME_IS_DIRTY) {
    this->MarkSubtreeDirty();
  }
}

bool nsBoxFrame::HonorPrintBackgroundSettings() {
  return !mContent->IsInNativeAnonymousSubtree() &&
         nsContainerFrame::HonorPrintBackgroundSettings();
}

#ifdef DO_NOISY_REFLOW
static int myCounter = 0;
static void printSize(char* aDesc, nscoord aSize) {
  printf(" %s: ", aDesc);
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    printf("UC");
  } else {
    printf("%d", aSize);
  }
}
#endif

/* virtual */
nscoord nsBoxFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  nsBoxLayoutState state(PresContext(), aRenderingContext);
  nsSize minSize = GetXULMinSize(state);

  // GetXULMinSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow input's border and padding, we
  // actually just want to subtract what GetXULMinSize added, which is the
  // result of GetXULBorderAndPadding.
  nsMargin bp;
  GetXULBorderAndPadding(bp);

  result = minSize.width - bp.LeftRight();
  result = std::max(result, 0);

  return result;
}

/* virtual */
nscoord nsBoxFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  nsBoxLayoutState state(PresContext(), aRenderingContext);
  nsSize prefSize = GetXULPrefSize(state);

  // GetXULPrefSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow input's border and padding, we
  // actually just want to subtract what GetXULPrefSize added, which is the
  // result of GetXULBorderAndPadding.
  nsMargin bp;
  GetXULBorderAndPadding(bp);

  result = prefSize.width - bp.LeftRight();
  result = std::max(result, 0);

  return result;
}

void nsBoxFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                        const ReflowInput& aReflowInput,
                        nsReflowStatus& aStatus) {
  MarkInReflow();
  // If you make changes to this method, please keep nsLeafBoxFrame::Reflow
  // in sync, if the changes are applicable there.

  DO_GLOBAL_REFLOW_COUNT("nsBoxFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  NS_ASSERTION(
      aReflowInput.ComputedWidth() >= 0 && aReflowInput.ComputedHeight() >= 0,
      "Computed Size < 0");

#ifdef DO_NOISY_REFLOW
  printf(
      "\n-------------Starting BoxFrame Reflow ----------------------------\n");
  printf("%p ** nsBF::Reflow %d ", this, myCounter++);

  printSize("AW", aReflowInput.AvailableWidth());
  printSize("AH", aReflowInput.AvailableHeight());
  printSize("CW", aReflowInput.ComputedWidth());
  printSize("CH", aReflowInput.ComputedHeight());

  printf(" *\n");

#endif

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowInput.mRenderingContext,
                         &aReflowInput, aReflowInput.mReflowDepth);

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize computedSize = aReflowInput.ComputedSize();

  LogicalMargin m = aReflowInput.ComputedLogicalBorderPadding();
  // GetXULBorderAndPadding(m);

  LogicalSize prefSize(wm);

  // if we are told to layout intrinsic then get our preferred size.
  NS_ASSERTION(computedSize.ISize(wm) != NS_UNCONSTRAINEDSIZE,
               "computed inline size should always be computed");
  if (computedSize.BSize(wm) == NS_UNCONSTRAINEDSIZE) {
    nsSize physicalPrefSize = GetXULPrefSize(state);
    nsSize minSize = GetXULMinSize(state);
    nsSize maxSize = GetXULMaxSize(state);
    // XXXbz isn't GetXULPrefSize supposed to bounds-check for us?
    physicalPrefSize = BoundsCheck(minSize, physicalPrefSize, maxSize);
    prefSize = LogicalSize(wm, physicalPrefSize);
  }

  // get our desiredSize
  computedSize.ISize(wm) += m.IStart(wm) + m.IEnd(wm);

  if (aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE) {
    computedSize.BSize(wm) = prefSize.BSize(wm);
    // prefSize is border-box but min/max constraints are content-box.
    nscoord blockDirBorderPadding =
        aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm);
    nscoord contentBSize = computedSize.BSize(wm) - blockDirBorderPadding;
    // Note: contentHeight might be negative, but that's OK because min-height
    // is never negative.
    computedSize.BSize(wm) =
        aReflowInput.ApplyMinMaxHeight(contentBSize) + blockDirBorderPadding;
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
    printf("%p ** nsBF(done) W:%d H:%d  ", this, aDesiredSize.Width(),
           aDesiredSize.Height());

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

nsSize nsBoxFrame::GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) {
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize size(0, 0);
  DISPLAY_PREF_SIZE(this, size);
  if (!DoesNeedRecalc(mPrefSize)) {
    size = mPrefSize;
    return size;
  }

  if (IsXULCollapsed()) return size;

  // if the size was not completely redefined in CSS then ask our children
  bool widthSet, heightSet;
  if (!nsIFrame::AddXULPrefSize(this, size, widthSet, heightSet)) {
    if (mLayoutManager) {
      nsSize layoutSize = mLayoutManager->GetXULPrefSize(this, aBoxLayoutState);
      if (!widthSet) size.width = layoutSize.width;
      if (!heightSet) size.height = layoutSize.height;
    } else {
      size = nsBox::GetXULPrefSize(aBoxLayoutState);
    }
  }

  nsSize minSize = GetXULMinSize(aBoxLayoutState);
  nsSize maxSize = GetXULMaxSize(aBoxLayoutState);
  mPrefSize = BoundsCheck(minSize, size, maxSize);

  return mPrefSize;
}

nscoord nsBoxFrame::GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) {
  if (!DoesNeedRecalc(mAscent)) return mAscent;

  if (IsXULCollapsed()) return 0;

  if (mLayoutManager)
    mAscent = mLayoutManager->GetAscent(this, aBoxLayoutState);
  else
    mAscent = nsBox::GetXULBoxAscent(aBoxLayoutState);

  return mAscent;
}

nsSize nsBoxFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) {
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize size(0, 0);
  DISPLAY_MIN_SIZE(this, size);
  if (!DoesNeedRecalc(mMinSize)) {
    size = mMinSize;
    return size;
  }

  if (IsXULCollapsed()) return size;

  // if the size was not completely redefined in CSS then ask our children
  bool widthSet, heightSet;
  if (!nsIFrame::AddXULMinSize(aBoxLayoutState, this, size, widthSet,
                               heightSet)) {
    if (mLayoutManager) {
      nsSize layoutSize = mLayoutManager->GetXULMinSize(this, aBoxLayoutState);
      if (!widthSet) size.width = layoutSize.width;
      if (!heightSet) size.height = layoutSize.height;
    } else {
      size = nsBox::GetXULMinSize(aBoxLayoutState);
    }
  }

  mMinSize = size;

  return size;
}

nsSize nsBoxFrame::GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) {
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize size(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  DISPLAY_MAX_SIZE(this, size);
  if (!DoesNeedRecalc(mMaxSize)) {
    size = mMaxSize;
    return size;
  }

  if (IsXULCollapsed()) return size;

  // if the size was not completely redefined in CSS then ask our children
  bool widthSet, heightSet;
  if (!nsIFrame::AddXULMaxSize(this, size, widthSet, heightSet)) {
    if (mLayoutManager) {
      nsSize layoutSize = mLayoutManager->GetXULMaxSize(this, aBoxLayoutState);
      if (!widthSet) size.width = layoutSize.width;
      if (!heightSet) size.height = layoutSize.height;
    } else {
      size = nsBox::GetXULMaxSize(aBoxLayoutState);
    }
  }

  mMaxSize = size;

  return size;
}

nscoord nsBoxFrame::GetXULFlex() {
  if (!DoesNeedRecalc(mFlex)) return mFlex;

  mFlex = nsBox::GetXULFlex();

  return mFlex;
}

/**
 * If subclassing please subclass this method not layout.
 * layout will call this method.
 */
NS_IMETHODIMP
nsBoxFrame::DoXULLayout(nsBoxLayoutState& aState) {
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
    ReflowInput reflowInput(
        aState.PresContext(), this, aState.GetRenderingContext(),
        LogicalSize(wm, GetLogicalSize().ISize(wm), NS_UNCONSTRAINEDSIZE));

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
    ReflowAbsoluteFrames(aState.PresContext(), desiredSize, reflowInput,
                         reflowStatus);
    RemoveStateBits(NS_FRAME_IN_REFLOW);
  }

  return rv;
}

void nsBoxFrame::DestroyFrom(nsIFrame* aDestructRoot,
                             PostDestroyData& aPostDestroyData) {
  // unregister access key
  RegUnregAccessKey(false);

  // clean up the container box's layout manager and child boxes
  SetXULLayoutManager(nullptr);

  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

/* virtual */
void nsBoxFrame::MarkIntrinsicISizesDirty() {
  SizeNeedsRecalc(mPrefSize);
  SizeNeedsRecalc(mMinSize);
  SizeNeedsRecalc(mMaxSize);
  CoordNeedsRecalc(mFlex);
  CoordNeedsRecalc(mAscent);

  if (mLayoutManager) {
    nsBoxLayoutState state(PresContext());
    mLayoutManager->IntrinsicISizesDirty(this, state);
  }

  nsContainerFrame::MarkIntrinsicISizesDirty();
}

void nsBoxFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  MOZ_ASSERT(aListID == kPrincipalList, "We don't support out-of-flow kids");

  nsPresContext* presContext = PresContext();
  nsBoxLayoutState state(presContext);

  // remove the child frame
  mFrames.RemoveFrame(aOldFrame);

  // notify the layout manager
  if (mLayoutManager) mLayoutManager->ChildrenRemoved(this, state, aOldFrame);

  // destroy the child frame
  aOldFrame->Destroy();

  // mark us dirty and generate a reflow command
  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
}

void nsBoxFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                              nsFrameList& aFrameList) {
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  NS_ASSERTION(!aPrevFrame || mFrames.ContainsFrame(aPrevFrame),
               "inserting after sibling frame not in our child list");
  MOZ_ASSERT(aListID == kPrincipalList, "We don't support out-of-flow kids");

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

  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
}

void nsBoxFrame::AppendFrames(ChildListID aListID, nsFrameList& aFrameList) {
  MOZ_ASSERT(aListID == kPrincipalList, "We don't support out-of-flow kids");

  nsBoxLayoutState state(PresContext());

  // append the new frames
  const nsFrameList::Slice& newFrames = mFrames.AppendFrames(this, aFrameList);

  // notify the layout manager
  if (mLayoutManager) mLayoutManager->ChildrenAppended(this, state, newFrames);

  // Make sure to check box order _after_ notifying the layout
  // manager; otherwise the slice we give the layout manager will
  // just be bogus.  If the layout manager cares about the order, we
  // just lose.
  CheckBoxOrder();

  // XXXbz why is this NS_FRAME_FIRST_REFLOW check here?
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
  }
}

/* virtual */
nsContainerFrame* nsBoxFrame::GetContentInsertionFrame() {
  if (GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK)
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  return nsContainerFrame::GetContentInsertionFrame();
}

nsresult nsBoxFrame::AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                      int32_t aModType) {
  nsresult rv =
      nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  // Ignore 'width', 'height', 'screenX', 'screenY' and 'sizemode' on a
  // <window>.
  if (mContent->IsAnyOfXULElements(nsGkAtoms::window, nsGkAtoms::page,
                                   nsGkAtoms::dialog, nsGkAtoms::wizard) &&
      (nsGkAtoms::width == aAttribute || nsGkAtoms::height == aAttribute ||
       nsGkAtoms::screenX == aAttribute || nsGkAtoms::screenY == aAttribute ||
       nsGkAtoms::sizemode == aAttribute)) {
    return rv;
  }

  if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
      aAttribute == nsGkAtoms::align || aAttribute == nsGkAtoms::valign ||
      aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top ||
      aAttribute == nsGkAtoms::right || aAttribute == nsGkAtoms::bottom ||
      aAttribute == nsGkAtoms::start || aAttribute == nsGkAtoms::end ||
      aAttribute == nsGkAtoms::minwidth || aAttribute == nsGkAtoms::maxwidth ||
      aAttribute == nsGkAtoms::minheight ||
      aAttribute == nsGkAtoms::maxheight || aAttribute == nsGkAtoms::flex ||
      aAttribute == nsGkAtoms::orient || aAttribute == nsGkAtoms::pack ||
      aAttribute == nsGkAtoms::dir || aAttribute == nsGkAtoms::mousethrough ||
      aAttribute == nsGkAtoms::equalsize) {
    if (aAttribute == nsGkAtoms::align || aAttribute == nsGkAtoms::valign ||
        aAttribute == nsGkAtoms::orient || aAttribute == nsGkAtoms::pack ||
        aAttribute == nsGkAtoms::dir) {
      mValign = nsBoxFrame::vAlign_Top;
      mHalign = nsBoxFrame::hAlign_Left;

      bool orient = true;
      GetInitialOrientation(orient);
      if (orient)
        AddStateBits(NS_STATE_IS_HORIZONTAL);
      else
        RemoveStateBits(NS_STATE_IS_HORIZONTAL);

      bool normal = true;
      GetInitialDirection(normal);
      if (normal)
        AddStateBits(NS_STATE_IS_DIRECTION_NORMAL);
      else
        RemoveStateBits(NS_STATE_IS_DIRECTION_NORMAL);

      GetInitialVAlignment(mValign);
      GetInitialHAlignment(mHalign);

      bool equalSize = false;
      GetInitialEqualSize(equalSize);
      if (equalSize)
        AddStateBits(NS_STATE_EQUAL_SIZE);
      else
        RemoveStateBits(NS_STATE_EQUAL_SIZE);

      bool autostretch = !!(mState & NS_STATE_AUTO_STRETCH);
      GetInitialAutoStretch(autostretch);
      if (autostretch)
        AddStateBits(NS_STATE_AUTO_STRETCH);
      else
        RemoveStateBits(NS_STATE_AUTO_STRETCH);
    } else if (aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top ||
               aAttribute == nsGkAtoms::right ||
               aAttribute == nsGkAtoms::bottom ||
               aAttribute == nsGkAtoms::start || aAttribute == nsGkAtoms::end) {
      RemoveStateBits(NS_STATE_STACK_NOT_POSITIONED);
    } else if (aAttribute == nsGkAtoms::mousethrough) {
      UpdateMouseThrough();
    }

    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  } else if (aAttribute == nsGkAtoms::ordinal) {
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
      PresShell()->FrameNeedsReflow(parent, IntrinsicDirty::StyleChange,
                                    NS_FRAME_IS_DIRTY);
    }
  }
  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  else if (aAttribute == nsGkAtoms::accesskey) {
    RegUnregAccessKey(true);
  } else if (aAttribute == nsGkAtoms::rows &&
             mContent->IsXULElement(nsGkAtoms::tree)) {
    // Reflow ourselves and all our children if "rows" changes, since
    // nsTreeBodyFrame's layout reads this from its parent (this frame).
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  }

  return rv;
}

void nsBoxFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                  const nsDisplayListSet& aLists) {
  bool forceLayer = false;
  // We check the renderroot attribute here in nsBoxFrame for lack of a better
  // place. This roughly mirrors the pre-existing "layer" attribute. In the
  // long term we may want to add a specific element in which we can wrap
  // alternate renderroot content, but we're electing to not go down that
  // rabbit hole today.
  wr::RenderRoot renderRoot =
      gfxUtils::GetRenderRootForFrame(this).valueOr(wr::RenderRoot::Default);

  if (GetContent()->IsXULElement()) {
    // forcelayer is only supported on XUL elements with box layout
    if (GetContent()->AsElement()->HasAttr(kNameSpaceID_None,
                                           nsGkAtoms::layer)) {
      forceLayer = true;
    }
    // Check for frames that are marked as a part of the region used
    // in calculating glass margins on Windows.
    const nsStyleDisplay* styles = StyleDisplay();
    if (styles && styles->mAppearance == StyleAppearance::MozWinExcludeGlass) {
      aBuilder->AddWindowExcludeGlassRegion(
          this, nsRect(aBuilder->ToReferenceFrame(this), GetSize()));
    }
  }

  nsDisplayListCollection tempLists(aBuilder);
  const nsDisplayListSet& destination =
      (forceLayer || renderRoot != wr::RenderRoot::Default) ? tempLists
                                                            : aLists;

  DisplayBorderBackgroundOutline(aBuilder, destination);

  Maybe<nsDisplayListBuilder::AutoContainerASRTracker> contASRTracker;
  if (forceLayer || renderRoot != wr::RenderRoot::Default) {
    contASRTracker.emplace(aBuilder);
  }

  BuildDisplayListForChildren(aBuilder, destination);

  // see if we have to draw a selection frame around this container
  DisplaySelectionOverlay(aBuilder, destination.Content());

  if (forceLayer || renderRoot != wr::RenderRoot::Default) {
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

    if (forceLayer) {
      MOZ_ASSERT(renderRoot == wr::RenderRoot::Default);
      // Wrap the list to make it its own layer
      aLists.Content()->AppendNewToTop<nsDisplayOwnLayer>(
          aBuilder, this, &masterList, ownLayerASR,
          nsDisplayOwnLayerFlags::None, mozilla::layers::ScrollbarData{}, true,
          true);
    } else {
      MOZ_ASSERT(!XRE_IsContentProcess());
      aLists.Content()->AppendNewToTop<nsDisplayRenderRoot>(
          aBuilder, this, &masterList, ownLayerASR, renderRoot);
    }
  }
}

void nsBoxFrame::BuildDisplayListForChildren(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayListSet& aLists) {
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background onto the BlockBorderBackgrounds list
  // to emulate the existing two-layer XUL painting scheme.
  nsDisplayListSet set(aLists, aLists.BlockBorderBackgrounds());
  // The children should be in the right order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, set);
    kid = kid->GetNextSibling();
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsBoxFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("Box"), aResult);
}
#endif

// If you make changes to this function, check its counterparts
// in nsTextBoxFrame and nsXULLabelFrame
void nsBoxFrame::RegUnregAccessKey(bool aDoReg) {
  MOZ_ASSERT(mContent);

  // only support accesskeys for the following elements
  if (!mContent->IsAnyOfXULElements(nsGkAtoms::button, nsGkAtoms::toolbarbutton,
                                    nsGkAtoms::checkbox, nsGkAtoms::textbox,
                                    nsGkAtoms::tab, nsGkAtoms::radio)) {
    return;
  }

  nsAutoString accessKey;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                 accessKey);

  if (accessKey.IsEmpty()) return;

  // With a valid PresContext we can get the ESM
  // and register the access key
  EventStateManager* esm = PresContext()->EventStateManager();

  uint32_t key = accessKey.First();
  if (aDoReg)
    esm->RegisterAccessKey(mContent->AsElement(), key);
  else
    esm->UnregisterAccessKey(mContent->AsElement(), key);
}

void nsBoxFrame::AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) {
  if (GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK) {
    aResult.AppendElement(OwnedAnonBox(PrincipalChildList().FirstChild()));
  }
}

// Helper less-than-or-equal function, used in CheckBoxOrder() as a
// template-parameter for the sorting functions.
static bool IsBoxOrdinalLEQ(nsIFrame* aFrame1, nsIFrame* aFrame2) {
  // If we've got a placeholder frame, use its out-of-flow frame's ordinal val.
  nsIFrame* aRealFrame1 = nsPlaceholderFrame::GetRealFrameFor(aFrame1);
  nsIFrame* aRealFrame2 = nsPlaceholderFrame::GetRealFrameFor(aFrame2);
  return aRealFrame1->GetXULOrdinal() <= aRealFrame2->GetXULOrdinal();
}

void nsBoxFrame::CheckBoxOrder() {
  if (!nsIFrame::IsFrameListSorted<IsBoxOrdinalLEQ>(mFrames)) {
    nsIFrame::SortFrameList<IsBoxOrdinalLEQ>(mFrames);
  }
}

nsresult nsBoxFrame::LayoutChildAt(nsBoxLayoutState& aState, nsIFrame* aBox,
                                   const nsRect& aRect) {
  // get the current rect
  nsRect oldRect(aBox->GetRect());
  aBox->SetXULBounds(aState, aRect);

  bool layout = NS_SUBTREE_DIRTY(aBox);

  if (layout ||
      (oldRect.width != aRect.width || oldRect.height != aRect.height)) {
    return aBox->XULLayout(aState);
  }

  return NS_OK;
}

nsresult nsBoxFrame::XULRelayoutChildAtOrdinal(nsIFrame* aChild) {
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
 * could fall through to the menu background, might be an appealing
 * simplification but it would mean slightly strange behaviour in some cases,
 * because grabber wrappers can be created for many individual lists and items,
 * so the exact fallthrough behaviour would be complex. E.g. an element with
 * "allowevents" on top of the Content() list could receive the event even if it
 * was covered by a PositionedDescenants() element without "allowevents". It is
 * best to never convert a non-null hit into null.
 */
// REVIEW: This is roughly of what nsMenuFrame::GetFrameForPoint used to do.
// I've made 'allowevents' affect child elements because that seems the only
// reasonable thing to do.
class nsDisplayXULEventRedirector final : public nsDisplayWrapList {
 public:
  nsDisplayXULEventRedirector(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsDisplayItem* aItem, nsIFrame* aTargetFrame)
      : nsDisplayWrapList(aBuilder, aFrame, aItem),
        mTargetFrame(aTargetFrame) {}
  nsDisplayXULEventRedirector(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsDisplayList* aList, nsIFrame* aTargetFrame)
      : nsDisplayWrapList(aBuilder, aFrame, aList),
        mTargetFrame(aTargetFrame) {}
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }
  NS_DISPLAY_DECL_NAME("XULEventRedirector", TYPE_XUL_EVENT_REDIRECTOR)
 private:
  nsIFrame* mTargetFrame;
};

void nsDisplayXULEventRedirector::HitTest(nsDisplayListBuilder* aBuilder,
                                          const nsRect& aRect,
                                          HitTestState* aState,
                                          nsTArray<nsIFrame*>* aOutFrames) {
  nsTArray<nsIFrame*> outFrames;
  mList.HitTest(aBuilder, aRect, aState, &outFrames);

  bool topMostAdded = false;
  uint32_t localLength = outFrames.Length();

  for (uint32_t i = 0; i < localLength; i++) {
    for (nsIContent* content = outFrames.ElementAt(i)->GetContent();
         content && content != mTargetFrame->GetContent();
         content = content->GetParent()) {
      if (!content->IsElement() ||
          !content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::allowevents,
                                             nsGkAtoms::_true, eCaseMatters)) {
        continue;
      }

      // Events are allowed on 'frame', so let it go.
      aOutFrames->AppendElement(outFrames.ElementAt(i));
      topMostAdded = true;
    }

    // If there was no hit on the topmost frame or its ancestors,
    // add the target frame itself as the first candidate (see bug 562554).
    if (!topMostAdded) {
      topMostAdded = true;
      aOutFrames->AppendElement(mTargetFrame);
    }
  }
}

class nsXULEventRedirectorWrapper final : public nsDisplayWrapper {
 public:
  explicit nsXULEventRedirectorWrapper(nsIFrame* aTargetFrame)
      : mTargetFrame(aTargetFrame) {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame,
                                  nsDisplayList* aList) override {
    return MakeDisplayItem<nsDisplayXULEventRedirector>(aBuilder, aFrame, aList,
                                                        mTargetFrame);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) override {
    return MakeDisplayItem<nsDisplayXULEventRedirector>(
        aBuilder, aItem->Frame(), aItem, mTargetFrame);
  }

 private:
  nsIFrame* mTargetFrame;
};

void nsBoxFrame::WrapListsInRedirector(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aIn,
                                       const nsDisplayListSet& aOut) {
  nsXULEventRedirectorWrapper wrapper(this);
  wrapper.WrapLists(aBuilder, this, aIn, aOut);
}

bool nsBoxFrame::GetEventPoint(WidgetGUIEvent* aEvent, nsPoint& aPoint) {
  LayoutDeviceIntPoint refPoint;
  bool res = GetEventPoint(aEvent, refPoint);
  aPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, refPoint, this);
  return res;
}

bool nsBoxFrame::GetEventPoint(WidgetGUIEvent* aEvent,
                               LayoutDeviceIntPoint& aPoint) {
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
