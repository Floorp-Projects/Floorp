/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "nsIFrame.h"

#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsDOMAttributeMap.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsContainerFrame.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsITheme.h"
#include "nsBoxLayout.h"
#include "FrameLayerBuilder.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include <algorithm>

using namespace mozilla;

nsresult nsIFrame::BeginXULLayout(nsBoxLayoutState& aState) {
  // mark ourselves as dirty so no child under us
  // can post an incremental layout.
  // XXXldb Is this still needed?
  AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    // If the parent is dirty, all the children are dirty (ReflowInput
    // does this too).
    nsIFrame* box;
    for (box = GetChildXULBox(this); box; box = GetNextXULBox(box))
      box->MarkSubtreeDirty();
  }

  // Another copy-over from ReflowInput.
  // Since we are in reflow, we don't need to store these properties anymore.
  RemoveProperty(UsedBorderProperty());
  RemoveProperty(UsedPaddingProperty());
  RemoveProperty(UsedMarginProperty());

  return NS_OK;
}

nsresult nsIFrame::EndXULLayout(nsBoxLayoutState& aState) {
  return SyncXULLayout(aState);
}

nsresult nsIFrame::XULRelayoutChildAtOrdinal(nsIFrame* aChild) { return NS_OK; }

nsresult nsIFrame::GetXULClientRect(nsRect& aClientRect) {
  aClientRect = mRect;
  aClientRect.MoveTo(0, 0);

  nsMargin borderPadding;
  GetXULBorderAndPadding(borderPadding);

  aClientRect.Deflate(borderPadding);

  if (aClientRect.width < 0) aClientRect.width = 0;

  if (aClientRect.height < 0) aClientRect.height = 0;

  return NS_OK;
}

void nsIFrame::SetXULBounds(nsBoxLayoutState& aState, const nsRect& aRect,
                            bool aRemoveOverflowAreas) {
  nsRect rect(mRect);

  ReflowChildFlags flags = GetXULLayoutFlags() | aState.LayoutFlags();

  if ((flags & ReflowChildFlags::NoMoveFrame) ==
      ReflowChildFlags::NoMoveFrame) {
    SetSize(aRect.Size());
  } else {
    SetRect(aRect);
  }

  // Nuke the overflow area. The caller is responsible for restoring
  // it if necessary.
  if (aRemoveOverflowAreas) {
    // remove the previously stored overflow area
    ClearOverflowRects();
  }

  if (!(flags & ReflowChildFlags::NoMoveView)) {
    nsContainerFrame::PositionFrameView(this);
    if ((rect.x != aRect.x) || (rect.y != aRect.y))
      nsContainerFrame::PositionChildViews(this);
  }
}

nsresult nsIFrame::GetXULBorderAndPadding(nsMargin& aBorderAndPadding) {
  aBorderAndPadding.SizeTo(0, 0, 0, 0);
  nsresult rv = GetXULBorder(aBorderAndPadding);
  if (NS_FAILED(rv)) return rv;

  nsMargin padding;
  rv = GetXULPadding(padding);
  if (NS_FAILED(rv)) return rv;

  aBorderAndPadding += padding;

  return rv;
}

nsresult nsIFrame::GetXULBorder(nsMargin& aBorder) {
  aBorder.SizeTo(0, 0, 0, 0);

  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->HasAppearance()) {
    // Go to the theme for the border.
    nsPresContext* pc = PresContext();
    nsITheme* theme = pc->Theme();
    if (theme->ThemeSupportsWidget(pc, this, disp->mAppearance)) {
      LayoutDeviceIntMargin margin =
          theme->GetWidgetBorder(pc->DeviceContext(), this, disp->mAppearance);
      aBorder =
          LayoutDevicePixel::ToAppUnits(margin, pc->AppUnitsPerDevPixel());
      return NS_OK;
    }
  }

  aBorder = StyleBorder()->GetComputedBorder();

  return NS_OK;
}

nsresult nsIFrame::GetXULPadding(nsMargin& aBorderAndPadding) {
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->HasAppearance()) {
    // Go to the theme for the padding.
    nsPresContext* pc = PresContext();
    nsITheme* theme = pc->Theme();
    if (theme->ThemeSupportsWidget(pc, this, disp->mAppearance)) {
      LayoutDeviceIntMargin padding;
      bool useThemePadding = theme->GetWidgetPadding(
          pc->DeviceContext(), this, disp->mAppearance, &padding);
      if (useThemePadding) {
        aBorderAndPadding =
            LayoutDevicePixel::ToAppUnits(padding, pc->AppUnitsPerDevPixel());
        return NS_OK;
      }
    }
  }

  aBorderAndPadding.SizeTo(0, 0, 0, 0);
  StylePadding()->GetPadding(aBorderAndPadding);

  return NS_OK;
}

nsresult nsIFrame::GetXULMargin(nsMargin& aMargin) {
  aMargin.SizeTo(0, 0, 0, 0);
  StyleMargin()->GetMargin(aMargin);

  return NS_OK;
}

void nsIFrame::XULSizeNeedsRecalc(nsSize& aSize) {
  aSize.width = -1;
  aSize.height = -1;
}

void nsIFrame::XULCoordNeedsRecalc(nscoord& aCoord) { aCoord = -1; }

bool nsIFrame::XULNeedsRecalc(const nsSize& aSize) {
  return (aSize.width == -1 || aSize.height == -1);
}

bool nsIFrame::XULNeedsRecalc(nscoord aCoord) { return (aCoord == -1); }

nsSize nsIFrame::GetUncachedXULPrefSize(nsBoxLayoutState& aBoxLayoutState) {
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize pref(0, 0);
  DISPLAY_PREF_SIZE(this, pref);

  if (IsXULCollapsed()) {
    return pref;
  }

  AddXULBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, pref, widthSet, heightSet);

  nsSize minSize = GetXULMinSize(aBoxLayoutState);
  nsSize maxSize = GetXULMaxSize(aBoxLayoutState);
  return XULBoundsCheck(minSize, pref, maxSize);
}

nsSize nsIFrame::GetUncachedXULMinSize(nsBoxLayoutState& aBoxLayoutState) {
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize min(0, 0);
  DISPLAY_MIN_SIZE(this, min);

  if (IsXULCollapsed()) {
    return min;
  }

  AddXULBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(this, min, widthSet, heightSet);
  return min;
}

nsSize nsIFrame::GetXULMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) {
  return nsSize(0, 0);
}

nsSize nsIFrame::GetUncachedXULMaxSize(nsBoxLayoutState& aBoxLayoutState) {
  NS_ASSERTION(aBoxLayoutState.GetRenderingContext(),
               "must have rendering context");

  nsSize maxSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  DISPLAY_MAX_SIZE(this, maxSize);

  if (IsXULCollapsed()) {
    return maxSize;
  }

  AddXULBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddXULMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

bool nsIFrame::IsXULCollapsed() {
  return StyleVisibility()->mVisible == StyleVisibility::Collapse;
}

nsresult nsIFrame::XULLayout(nsBoxLayoutState& aState) {
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsIFrame* box = static_cast<nsIFrame*>(this);
  DISPLAY_LAYOUT(box);

  box->BeginXULLayout(aState);

  box->DoXULLayout(aState);

  box->EndXULLayout(aState);

  return NS_OK;
}

bool nsIFrame::DoesClipChildren() {
  const nsStyleDisplay* display = StyleDisplay();
  NS_ASSERTION(
      (display->mOverflowY == StyleOverflow::MozHiddenUnscrollable) ==
          (display->mOverflowX == StyleOverflow::MozHiddenUnscrollable),
      "If one overflow is -moz-hidden-unscrollable, the other should be too");
  return display->mOverflowX == StyleOverflow::MozHiddenUnscrollable;
}

nsresult nsIFrame::SyncXULLayout(nsBoxLayoutState& aBoxLayoutState) {
  /*
  if (IsXULCollapsed()) {
    CollapseChild(aBoxLayoutState, this, true);
    return NS_OK;
  }
  */

  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    XULRedraw(aBoxLayoutState);
  }

  RemoveStateBits(NS_FRAME_HAS_DIRTY_CHILDREN | NS_FRAME_IS_DIRTY |
                  NS_FRAME_FIRST_REFLOW | NS_FRAME_IN_REFLOW);

  nsPresContext* presContext = aBoxLayoutState.PresContext();

  ReflowChildFlags flags = GetXULLayoutFlags() | aBoxLayoutState.LayoutFlags();

  nsRect visualOverflow;

  if (XULComputesOwnOverflowArea()) {
    visualOverflow = GetVisualOverflowRect();
  } else {
    nsRect rect(nsPoint(0, 0), GetSize());
    nsOverflowAreas overflowAreas(rect, rect);
    if (!DoesClipChildren() && !IsXULCollapsed()) {
      // See if our child frames caused us to overflow after being laid
      // out. If so, store the overflow area.  This normally can't happen
      // in XUL, but it can happen with the CSS 'outline' property and
      // possibly with other exotic stuff (e.g. relatively positioned
      // frames in HTML inside XUL).
      nsLayoutUtils::UnionChildOverflow(this, overflowAreas);
    }

    FinishAndStoreOverflow(overflowAreas, GetSize());
    visualOverflow = overflowAreas.VisualOverflow();
  }

  nsView* view = GetView();
  if (view) {
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    nsContainerFrame::SyncFrameViewAfterReflow(presContext, this, view,
                                               visualOverflow, flags);
  }

  return NS_OK;
}

nsresult nsIFrame::XULRedraw(nsBoxLayoutState& aState) {
  if (aState.PaintingDisabled()) return NS_OK;

  // nsStackLayout, at least, expects us to repaint descendants even
  // if a damage rect is provided
  InvalidateFrameSubtree();

  return NS_OK;
}

bool nsIFrame::AddXULPrefSize(nsIFrame* aBox, nsSize& aSize, bool& aWidthSet,
                              bool& aHeightSet) {
  aWidthSet = false;
  aHeightSet = false;

  // add in the css min, max, pref
  const nsStylePosition* position = aBox->StylePosition();

  // see if the width or height was specifically set
  // XXX Handle eStyleUnit_Enumerated?
  // (Handling the eStyleUnit_Enumerated types requires
  // GetXULPrefSize/GetXULMinSize methods that don't consider
  // (min-/max-/)(width/height) properties.)
  const auto& width = position->mWidth;
  if (width.ConvertsToLength()) {
    aSize.width = std::max(0, width.ToLength());
    aWidthSet = true;
  }

  const auto& height = position->mHeight;
  if (height.ConvertsToLength()) {
    aSize.height = std::max(0, height.ToLength());
    aHeightSet = true;
  }

  nsIContent* content = aBox->GetContent();
  // ignore 'height' and 'width' attributes if the actual element is not XUL
  // For example, we might be magic XUL frames whose primary content is an HTML
  // <select>
  if (content && content->IsXULElement()) {
    nsAutoString value;
    nsresult error;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::width, value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      aSize.width = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aWidthSet = true;
    }

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::height, value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      aSize.height =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aHeightSet = true;
    }
  }

  return (aWidthSet && aHeightSet);
}

// This returns the scrollbar width we want to use when either native
// theme is disabled, or the native theme claims that it doesn't support
// scrollbar.
static nscoord GetScrollbarWidthNoTheme(nsIFrame* aBox) {
  ComputedStyle* scrollbarStyle = nsLayoutUtils::StyleForScrollbar(aBox);
  switch (scrollbarStyle->StyleUIReset()->mScrollbarWidth) {
    default:
    case StyleScrollbarWidth::Auto:
      return 12 * AppUnitsPerCSSPixel();
    case StyleScrollbarWidth::Thin:
      return 6 * AppUnitsPerCSSPixel();
    case StyleScrollbarWidth::None:
      return 0;
  }
}

bool nsIFrame::AddXULMinSize(nsIFrame* aBox, nsSize& aSize, bool& aWidthSet,
                             bool& aHeightSet) {
  aWidthSet = false;
  aHeightSet = false;

  bool canOverride = true;

  nsPresContext* pc = aBox->PresContext();

  // See if a native theme wants to supply a minimum size.
  const nsStyleDisplay* display = aBox->StyleDisplay();
  if (display->HasAppearance()) {
    nsITheme* theme = pc->Theme();
    if (theme->ThemeSupportsWidget(pc, aBox, display->mAppearance)) {
      LayoutDeviceIntSize size;
      theme->GetMinimumWidgetSize(pc, aBox, display->mAppearance, &size,
                                  &canOverride);
      if (size.width) {
        aSize.width = pc->DevPixelsToAppUnits(size.width);
        aWidthSet = true;
      }
      if (size.height) {
        aSize.height = pc->DevPixelsToAppUnits(size.height);
        aHeightSet = true;
      }
    } else {
      switch (display->mAppearance) {
        case StyleAppearance::ScrollbarVertical:
          aSize.width = GetScrollbarWidthNoTheme(aBox);
          aWidthSet = true;
          break;
        case StyleAppearance::ScrollbarHorizontal:
          aSize.height = GetScrollbarWidthNoTheme(aBox);
          aHeightSet = true;
          break;
        default:
          break;
      }
    }
  }

  // add in the css min, max, pref
  const nsStylePosition* position = aBox->StylePosition();
  const auto& minWidth = position->mMinWidth;
  if (minWidth.ConvertsToLength()) {
    nscoord min = minWidth.ToLength();
    if (!aWidthSet || (min > aSize.width && canOverride)) {
      aSize.width = min;
      aWidthSet = true;
    }
  } else if (minWidth.ConvertsToPercentage()) {
    NS_ASSERTION(minWidth.ToPercentage() == 0.0f,
                 "Non-zero percentage values not currently supported");
    aSize.width = 0;
    aWidthSet = true;  // FIXME: should we really do this for
                       // nonzero values?
  }
  // XXX Handle ExtremumLength?
  // (Handling them  requires GetXULPrefSize/GetXULMinSize methods that don't
  // consider (min-/max-/)(width/height) properties.
  // calc() with percentage is treated like '0' (unset)

  const auto& minHeight = position->mMinHeight;
  if (minHeight.ConvertsToLength()) {
    nscoord min = minHeight.ToLength();
    if (!aHeightSet || (min > aSize.height && canOverride)) {
      aSize.height = min;
      aHeightSet = true;
    }
  } else if (minHeight.ConvertsToPercentage()) {
    NS_ASSERTION(position->mMinHeight.ToPercentage() == 0.0f,
                 "Non-zero percentage values not currently supported");
    aSize.height = 0;
    aHeightSet = true;  // FIXME: should we really do this for
                        // nonzero values?
  }
  // calc() with percentage is treated like '0' (unset)

  nsIContent* content = aBox->GetContent();
  if (content && content->IsXULElement()) {
    nsAutoString value;
    nsresult error;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::minwidth,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      if (val > aSize.width) aSize.width = val;
      aWidthSet = true;
    }

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::minheight,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      if (val > aSize.height) aSize.height = val;

      aHeightSet = true;
    }
  }

  return (aWidthSet && aHeightSet);
}

bool nsIFrame::AddXULMaxSize(nsIFrame* aBox, nsSize& aSize, bool& aWidthSet,
                             bool& aHeightSet) {
  aWidthSet = false;
  aHeightSet = false;

  // add in the css min, max, pref
  const nsStylePosition* position = aBox->StylePosition();

  // and max
  // see if the width or height was specifically set
  // XXX Handle eStyleUnit_Enumerated?
  // (Handling the eStyleUnit_Enumerated types requires
  // GetXULPrefSize/GetXULMinSize methods that don't consider
  // (min-/max-/)(width/height) properties.)
  const auto& maxWidth = position->mMaxWidth;
  if (maxWidth.ConvertsToLength()) {
    aSize.width = maxWidth.ToLength();
    aWidthSet = true;
  }
  // percentages and calc() with percentages are treated like 'none'

  const auto& maxHeight = position->mMaxHeight;
  if (maxHeight.ConvertsToLength()) {
    aSize.height = maxHeight.ToLength();
    aHeightSet = true;
  }
  // percentages and calc() with percentages are treated like 'none'

  nsIContent* content = aBox->GetContent();
  if (content && content->IsXULElement()) {
    nsAutoString value;
    nsresult error;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::maxwidth,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aSize.width = val;
      aWidthSet = true;
    }

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::maxheight,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aSize.height = val;

      aHeightSet = true;
    }
  }

  return (aWidthSet || aHeightSet);
}

bool nsIFrame::AddXULFlex(nsIFrame* aBox, nscoord& aFlex) {
  bool flexSet = false;

  // get the flexibility
  aFlex = aBox->StyleXUL()->mBoxFlex;

  // attribute value overrides CSS
  nsIContent* content = aBox->GetContent();
  if (content && content->IsXULElement()) {
    nsresult error;
    nsAutoString value;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::flex, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      aFlex = value.ToInteger(&error);
      flexSet = true;
    }
  }

  if (aFlex < 0) aFlex = 0;
  if (aFlex >= nscoord_MAX) aFlex = nscoord_MAX - 1;

  return flexSet || aFlex > 0;
}

void nsIFrame::AddXULBorderAndPadding(nsSize& aSize) {
  AddXULBorderAndPadding(this, aSize);
}

void nsIFrame::AddXULBorderAndPadding(nsIFrame* aBox, nsSize& aSize) {
  nsMargin borderPadding(0, 0, 0, 0);
  aBox->GetXULBorderAndPadding(borderPadding);
  AddXULMargin(aSize, borderPadding);
}

void nsIFrame::AddXULMargin(nsIFrame* aChild, nsSize& aSize) {
  nsMargin margin(0, 0, 0, 0);
  aChild->GetXULMargin(margin);
  AddXULMargin(aSize, margin);
}

void nsIFrame::AddXULMargin(nsSize& aSize, const nsMargin& aMargin) {
  if (aSize.width != NS_UNCONSTRAINEDSIZE)
    aSize.width += aMargin.left + aMargin.right;

  if (aSize.height != NS_UNCONSTRAINEDSIZE)
    aSize.height += aMargin.top + aMargin.bottom;
}

nscoord nsIFrame::XULBoundsCheck(nscoord aMin, nscoord aPref, nscoord aMax) {
  if (aPref > aMax) aPref = aMax;

  if (aPref < aMin) aPref = aMin;

  return aPref;
}

nsSize nsIFrame::XULBoundsCheckMinMax(const nsSize& aMinSize,
                                      const nsSize& aMaxSize) {
  return nsSize(std::max(aMaxSize.width, aMinSize.width),
                std::max(aMaxSize.height, aMinSize.height));
}

nsSize nsIFrame::XULBoundsCheck(const nsSize& aMinSize, const nsSize& aPrefSize,
                                const nsSize& aMaxSize) {
  return nsSize(
      XULBoundsCheck(aMinSize.width, aPrefSize.width, aMaxSize.width),
      XULBoundsCheck(aMinSize.height, aPrefSize.height, aMaxSize.height));
}

/*static*/
nsIFrame* nsIFrame::GetChildXULBox(const nsIFrame* aFrame) {
  // box layout ends at box-wrapped frames, so don't allow these frames
  // to report child boxes.
  return aFrame->IsXULBoxFrame() ? aFrame->PrincipalChildList().FirstChild()
                                 : nullptr;
}

/*static*/
nsIFrame* nsIFrame::GetNextXULBox(const nsIFrame* aFrame) {
  return aFrame->GetParent() && aFrame->GetParent()->IsXULBoxFrame()
             ? aFrame->GetNextSibling()
             : nullptr;
}

/*static*/
nsIFrame* nsIFrame::GetParentXULBox(const nsIFrame* aFrame) {
  return aFrame->GetParent() && aFrame->GetParent()->IsXULBoxFrame()
             ? aFrame->GetParent()
             : nullptr;
}
