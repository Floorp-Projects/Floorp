/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsContainerFrame.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIDOMNode.h"
#include "nsIDOMMozNamedAttrMap.h"
#include "nsIDOMAttr.h"
#include "nsITheme.h"
#include "nsIServiceManager.h"
#include "nsBoxLayout.h"
#include "FrameLayerBuilder.h"
#include <algorithm>

using namespace mozilla;

#ifdef DEBUG_LAYOUT
int32_t gIndent = 0;
#endif

#ifdef DEBUG_LAYOUT
void
nsBoxAddIndents()
{
    for(int32_t i=0; i < gIndent; i++)
    {
        printf(" ");
    }
}
#endif

#ifdef DEBUG_LAYOUT
void
nsBox::AppendAttribute(const nsAutoString& aAttribute, const nsAutoString& aValue, nsAutoString& aResult)
{
   aResult.Append(aAttribute);
   aResult.AppendLiteral("='");
   aResult.Append(aValue);
   aResult.AppendLiteral("' ");
}

void
nsBox::ListBox(nsAutoString& aResult)
{
    nsAutoString name;
    GetBoxName(name);

    char addr[100];
    sprintf(addr, "[@%p] ", static_cast<void*>(this));

    aResult.AppendASCII(addr);
    aResult.Append(name);
    aResult.Append(' ');

    nsIContent* content = GetContent();

    // add on all the set attributes
    if (content) {
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
      nsCOMPtr<nsIDOMMozNamedAttrMap> namedMap;

      node->GetAttributes(getter_AddRefs(namedMap));
      uint32_t length;
      namedMap->GetLength(&length);

      nsCOMPtr<nsIDOMAttr> attribute;
      for (uint32_t i = 0; i < length; ++i)
      {
        namedMap->Item(i, getter_AddRefs(attribute));
        attribute->GetName(name);
        nsAutoString value;
        attribute->GetValue(value);
        AppendAttribute(name, value, aResult);
      }
    }
}

nsresult
nsBox::XULDumpBox(FILE* aFile)
{
  nsAutoString s;
  ListBox(s);
  fprintf(aFile, "%s", NS_LossyConvertUTF16toASCII(s).get());
  return NS_OK;
}

void
nsBox::PropagateDebug(nsBoxLayoutState& aState)
{
  // propagate debug information
  if (mState & NS_STATE_DEBUG_WAS_SET) {
    if (mState & NS_STATE_SET_TO_DEBUG)
      SetXULDebug(aState, true);
    else
      SetXULDebug(aState, false);
  } else if (mState & NS_STATE_IS_ROOT) {
    SetXULDebug(aState, gDebug);
  }
}
#endif

#ifdef DEBUG_LAYOUT
void
nsBox::GetBoxName(nsAutoString& aName)
{
  aName.AssignLiteral("Box");
}
#endif

nsresult
nsBox::BeginXULLayout(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT 

  nsBoxAddIndents();
  printf("XULLayout: ");
  XULDumpBox(stdout);
  printf("\n");
  gIndent++;
#endif

  // mark ourselves as dirty so no child under us
  // can post an incremental layout.
  // XXXldb Is this still needed?
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

  if (GetStateBits() & NS_FRAME_IS_DIRTY)
  {
    // If the parent is dirty, all the children are dirty (ReflowInput
    // does this too).
    nsIFrame* box;
    for (box = GetChildXULBox(this); box; box = GetNextXULBox(box))
      box->AddStateBits(NS_FRAME_IS_DIRTY);
  }

  // Another copy-over from ReflowInput.
  // Since we are in reflow, we don't need to store these properties anymore.
  FrameProperties props = Properties();
  props.Delete(UsedBorderProperty());
  props.Delete(UsedPaddingProperty());
  props.Delete(UsedMarginProperty());

#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsBox::DoXULLayout(nsBoxLayoutState& aState)
{
  return NS_OK;
}

nsresult
nsBox::EndXULLayout(nsBoxLayoutState& aState)
{

  #ifdef DEBUG_LAYOUT
      --gIndent;
  #endif

  return SyncLayout(aState);
}

bool nsBox::gGotTheme = false;
nsITheme* nsBox::gTheme = nullptr;

nsBox::nsBox(LayoutFrameType aType)
  : nsIFrame(aType)
{
  MOZ_COUNT_CTOR(nsBox);
  //mX = 0;
  //mY = 0;
  if (!gGotTheme) {
    gGotTheme = true;
    CallGetService("@mozilla.org/chrome/chrome-native-theme;1", &gTheme);
  }
}

nsBox::~nsBox()
{
  // NOTE:  This currently doesn't get called for |nsBoxToBlockAdaptor|
  // objects, so don't rely on putting anything here.
  MOZ_COUNT_DTOR(nsBox);
}

/* static */ void
nsBox::Shutdown()
{
  gGotTheme = false;
  NS_IF_RELEASE(gTheme);
}

nsresult
nsBox::XULRelayoutChildAtOrdinal(nsIFrame* aChild)
{
  return NS_OK;
}

nsresult
nsIFrame::GetXULClientRect(nsRect& aClientRect)
{
  aClientRect = mRect;
  aClientRect.MoveTo(0,0);

  nsMargin borderPadding;
  GetXULBorderAndPadding(borderPadding);

  aClientRect.Deflate(borderPadding);

  if (aClientRect.width < 0)
     aClientRect.width = 0;

  if (aClientRect.height < 0)
     aClientRect.height = 0;

 // NS_ASSERTION(aClientRect.width >=0 && aClientRect.height >= 0, "Content Size < 0");

  return NS_OK;
}

void
nsBox::SetXULBounds(nsBoxLayoutState& aState, const nsRect& aRect, bool aRemoveOverflowAreas)
{
    NS_BOX_ASSERTION(this, aRect.width >=0 && aRect.height >= 0, "SetXULBounds Size < 0");

    nsRect rect(mRect);

    uint32_t flags = GetXULLayoutFlags();

    uint32_t stateFlags = aState.LayoutFlags();

    flags |= stateFlags;

    if ((flags & NS_FRAME_NO_MOVE_FRAME) == NS_FRAME_NO_MOVE_FRAME)
      SetSize(aRect.Size());
    else
      SetRect(aRect);

    // Nuke the overflow area. The caller is responsible for restoring
    // it if necessary.
    if (aRemoveOverflowAreas) {
      // remove the previously stored overflow area
      ClearOverflowRects();
    }

    if (!(flags & NS_FRAME_NO_MOVE_VIEW))
    {
      nsContainerFrame::PositionFrameView(this);
      if ((rect.x != aRect.x) || (rect.y != aRect.y))
        nsContainerFrame::PositionChildViews(this);
    }
  

   /*  
    // only if the origin changed
    if ((rect.x != aRect.x) || (rect.y != aRect.y))  {
      if (frame->HasView()) {
        nsContainerFrame::PositionFrameView(presContext, frame,
                                            frame->GetView());
      } else {
        nsContainerFrame::PositionChildViews(presContext, frame);
      }
    }
    */
}

nsresult
nsIFrame::GetXULBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0, 0, 0, 0);
  nsresult rv = GetXULBorder(aBorderAndPadding);
  if (NS_FAILED(rv))
    return rv;

  nsMargin padding;
  rv = GetXULPadding(padding);
  if (NS_FAILED(rv))
    return rv;

  aBorderAndPadding += padding;

  return rv;
}

nsresult
nsBox::GetXULBorder(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
    
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->UsedAppearance() && gTheme) {
    // Go to the theme for the border.
    nsPresContext *context = PresContext();
    if (gTheme->ThemeSupportsWidget(context, this, disp->UsedAppearance())) {
      nsIntMargin margin(0, 0, 0, 0);
      gTheme->GetWidgetBorder(context->DeviceContext(), this,
                              disp->UsedAppearance(), &margin);
      aMargin.top = context->DevPixelsToAppUnits(margin.top);
      aMargin.right = context->DevPixelsToAppUnits(margin.right);
      aMargin.bottom = context->DevPixelsToAppUnits(margin.bottom);
      aMargin.left = context->DevPixelsToAppUnits(margin.left);
      return NS_OK;
    }
  }

  aMargin = StyleBorder()->GetComputedBorder();

  return NS_OK;
}

nsresult
nsBox::GetXULPadding(nsMargin& aMargin)
{
  const nsStyleDisplay *disp = StyleDisplay();
  if (disp->UsedAppearance() && gTheme) {
    // Go to the theme for the padding.
    nsPresContext *context = PresContext();
    if (gTheme->ThemeSupportsWidget(context, this, disp->UsedAppearance())) {
      nsIntMargin margin(0, 0, 0, 0);
      bool useThemePadding;

      useThemePadding = gTheme->GetWidgetPadding(context->DeviceContext(),
                                                 this, disp->UsedAppearance(),
                                                 &margin);
      if (useThemePadding) {
        aMargin.top = context->DevPixelsToAppUnits(margin.top);
        aMargin.right = context->DevPixelsToAppUnits(margin.right);
        aMargin.bottom = context->DevPixelsToAppUnits(margin.bottom);
        aMargin.left = context->DevPixelsToAppUnits(margin.left);
        return NS_OK;
      }
    }
  }

  aMargin.SizeTo(0,0,0,0);
  StylePadding()->GetPadding(aMargin);

  return NS_OK;
}

nsresult
nsBox::GetXULMargin(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
  StyleMargin()->GetMargin(aMargin);

  return NS_OK;
}

void
nsBox::SizeNeedsRecalc(nsSize& aSize)
{
  aSize.width  = -1;
  aSize.height = -1;
}

void
nsBox::CoordNeedsRecalc(nscoord& aFlex)
{
  aFlex = -1;
}

bool
nsBox::DoesNeedRecalc(const nsSize& aSize)
{
  return (aSize.width == -1 || aSize.height == -1);
}

bool
nsBox::DoesNeedRecalc(nscoord aCoord)
{
  return (aCoord == -1);
}

nsSize
nsBox::GetXULPrefSize(nsBoxLayoutState& aState)
{
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsSize pref(0,0);
  DISPLAY_PREF_SIZE(this, pref);

  if (IsXULCollapsed())
    return pref;

  AddBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, pref, widthSet, heightSet);

  nsSize minSize = GetXULMinSize(aState);
  nsSize maxSize = GetXULMaxSize(aState);
  return BoundsCheck(minSize, pref, maxSize);
}

nsSize
nsBox::GetXULMinSize(nsBoxLayoutState& aState)
{
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsSize min(0,0);
  DISPLAY_MIN_SIZE(this, min);

  if (IsXULCollapsed())
    return min;

  AddBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(aState, this, min, widthSet, heightSet);
  return min;
}

nsSize
nsBox::GetXULMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState)
{
  return nsSize(0, 0);
}

nsSize
nsBox::GetXULMaxSize(nsBoxLayoutState& aState)
{
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsSize maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, maxSize);

  if (IsXULCollapsed())
    return maxSize;

  AddBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddXULMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

nscoord
nsBox::GetXULFlex()
{
  nscoord flex = 0;

  nsIFrame::AddXULFlex(this, flex);

  return flex;
}

uint32_t
nsIFrame::GetXULOrdinal()
{
  uint32_t ordinal = StyleXUL()->mBoxOrdinal;

  // When present, attribute value overrides CSS.
  nsIContent* content = GetContent();
  if (content && content->IsXULElement()) {
    nsresult error;
    nsAutoString value;

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::ordinal, value);
    if (!value.IsEmpty()) {
      ordinal = value.ToInteger(&error);
    }
  }

  return ordinal;
}

nscoord
nsBox::GetXULBoxAscent(nsBoxLayoutState& aState)
{
  if (IsXULCollapsed())
    return 0;

  return GetXULPrefSize(aState).height;
}

bool
nsBox::IsXULCollapsed()
{
  return StyleVisibility()->mVisible == NS_STYLE_VISIBILITY_COLLAPSE;
}

nsresult
nsIFrame::XULLayout(nsBoxLayoutState& aState)
{
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsBox *box = static_cast<nsBox*>(this);
  DISPLAY_LAYOUT(box);

  box->BeginXULLayout(aState);

  box->DoXULLayout(aState);

  box->EndXULLayout(aState);

  return NS_OK;
}

bool
nsBox::DoesClipChildren()
{
  const nsStyleDisplay* display = StyleDisplay();
  NS_ASSERTION((display->mOverflowY == NS_STYLE_OVERFLOW_CLIP) ==
               (display->mOverflowX == NS_STYLE_OVERFLOW_CLIP),
               "If one overflow is clip, the other should be too");
  return display->mOverflowX == NS_STYLE_OVERFLOW_CLIP;
}

nsresult
nsBox::SyncLayout(nsBoxLayoutState& aState)
{
  /*
  if (IsXULCollapsed()) {
    CollapseChild(aState, this, true);
    return NS_OK;
  }
  */
  

  if (GetStateBits() & NS_FRAME_IS_DIRTY)
     XULRedraw(aState);

  RemoveStateBits(NS_FRAME_HAS_DIRTY_CHILDREN | NS_FRAME_IS_DIRTY
                  | NS_FRAME_FIRST_REFLOW | NS_FRAME_IN_REFLOW);

  nsPresContext* presContext = aState.PresContext();

  uint32_t flags = GetXULLayoutFlags();

  uint32_t stateFlags = aState.LayoutFlags();

  flags |= stateFlags;

  nsRect visualOverflow;

  if (ComputesOwnOverflowArea()) {
    visualOverflow = GetVisualOverflowRect();
  }
  else {
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

nsresult
nsIFrame::XULRedraw(nsBoxLayoutState& aState)
{
  if (aState.PaintingDisabled())
    return NS_OK;

  // nsStackLayout, at least, expects us to repaint descendants even
  // if a damage rect is provided
  InvalidateFrameSubtree();

  return NS_OK;
}

bool
nsIFrame::AddXULPrefSize(nsIFrame* aBox, nsSize& aSize, bool &aWidthSet, bool &aHeightSet)
{
    aWidthSet = false;
    aHeightSet = false;

    // add in the css min, max, pref
    const nsStylePosition* position = aBox->StylePosition();

    // see if the width or height was specifically set
    // XXX Handle eStyleUnit_Enumerated?
    // (Handling the eStyleUnit_Enumerated types requires
    // GetXULPrefSize/GetXULMinSize methods that don't consider
    // (min-/max-/)(width/height) properties.)
    const nsStyleCoord &width = position->mWidth;
    if (width.GetUnit() == eStyleUnit_Coord) {
        aSize.width = width.GetCoordValue();
        aWidthSet = true;
    } else if (width.IsCalcUnit()) {
        if (!width.CalcHasPercent()) {
            // pass 0 for percentage basis since we know there are no %s
            aSize.width = nsRuleNode::ComputeComputedCalc(width, 0);
            if (aSize.width < 0)
                aSize.width = 0;
            aWidthSet = true;
        }
    }

    const nsStyleCoord &height = position->mHeight;
    if (height.GetUnit() == eStyleUnit_Coord) {
        aSize.height = height.GetCoordValue();
        aHeightSet = true;
    } else if (height.IsCalcUnit()) {
        if (!height.CalcHasPercent()) {
            // pass 0 for percentage basis since we know there are no %s
            aSize.height = nsRuleNode::ComputeComputedCalc(height, 0);
            if (aSize.height < 0)
                aSize.height = 0;
            aHeightSet = true;
        }
    }

    nsIContent* content = aBox->GetContent();
    // ignore 'height' and 'width' attributes if the actual element is not XUL
    // For example, we might be magic XUL frames whose primary content is an HTML
    // <select>
    if (content && content->IsXULElement()) {
        nsAutoString value;
        nsresult error;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::width, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            aSize.width =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            aWidthSet = true;
        }

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::height, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            aSize.height =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            aHeightSet = true;
        }
    }

    return (aWidthSet && aHeightSet);
}


bool
nsIFrame::AddXULMinSize(nsBoxLayoutState& aState, nsIFrame* aBox, nsSize& aSize,
                      bool &aWidthSet, bool &aHeightSet)
{
    aWidthSet = false;
    aHeightSet = false;

    bool canOverride = true;

    // See if a native theme wants to supply a minimum size.
    const nsStyleDisplay* display = aBox->StyleDisplay();
    if (display->UsedAppearance()) {
      nsITheme *theme = aState.PresContext()->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aState.PresContext(), aBox, display->UsedAppearance())) {
        LayoutDeviceIntSize size;
        theme->GetMinimumWidgetSize(aState.PresContext(), aBox,
                                    display->UsedAppearance(), &size, &canOverride);
        if (size.width) {
          aSize.width = aState.PresContext()->DevPixelsToAppUnits(size.width);
          aWidthSet = true;
        }
        if (size.height) {
          aSize.height = aState.PresContext()->DevPixelsToAppUnits(size.height);
          aHeightSet = true;
        }
      }
    }

    // add in the css min, max, pref
    const nsStylePosition* position = aBox->StylePosition();

    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 (as a coord) means not set.
    const nsStyleCoord &minWidth = position->mMinWidth;
    if ((minWidth.GetUnit() == eStyleUnit_Coord &&
         minWidth.GetCoordValue() != 0) ||
        (minWidth.IsCalcUnit() && !minWidth.CalcHasPercent())) {
        nscoord min = nsRuleNode::ComputeCoordPercentCalc(minWidth, 0);
        if (!aWidthSet || (min > aSize.width && canOverride)) {
           aSize.width = min;
           aWidthSet = true;
        }
    } else if (minWidth.GetUnit() == eStyleUnit_Percent) {
        NS_ASSERTION(minWidth.GetPercentValue() == 0.0f,
          "Non-zero percentage values not currently supported");
        aSize.width = 0;
        aWidthSet = true; // FIXME: should we really do this for
                             // nonzero values?
    }
    // XXX Handle eStyleUnit_Enumerated?
    // (Handling the eStyleUnit_Enumerated types requires
    // GetXULPrefSize/GetXULMinSize methods that don't consider
    // (min-/max-/)(width/height) properties.
    // calc() with percentage is treated like '0' (unset)

    const nsStyleCoord &minHeight = position->mMinHeight;
    if ((minHeight.GetUnit() == eStyleUnit_Coord &&
         minHeight.GetCoordValue() != 0) ||
        (minHeight.IsCalcUnit() && !minHeight.CalcHasPercent())) {
        nscoord min = nsRuleNode::ComputeCoordPercentCalc(minHeight, 0);
        if (!aHeightSet || (min > aSize.height && canOverride)) {
           aSize.height = min;
           aHeightSet = true;
        }
    } else if (minHeight.GetUnit() == eStyleUnit_Percent) {
        NS_ASSERTION(position->mMinHeight.GetPercentValue() == 0.0f,
          "Non-zero percentage values not currently supported");
        aSize.height = 0;
        aHeightSet = true; // FIXME: should we really do this for
                              // nonzero values?
    }
    // calc() with percentage is treated like '0' (unset)

    nsIContent* content = aBox->GetContent();
    if (content && content->IsXULElement()) {
        nsAutoString value;
        nsresult error;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::minwidth, value);
        if (!value.IsEmpty())
        {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            if (val > aSize.width)
              aSize.width = val;
            aWidthSet = true;
        }

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::minheight, value);
        if (!value.IsEmpty())
        {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            if (val > aSize.height)
              aSize.height = val;

            aHeightSet = true;
        }
    }

    return (aWidthSet && aHeightSet);
}

bool
nsIFrame::AddXULMaxSize(nsIFrame* aBox, nsSize& aSize, bool &aWidthSet, bool &aHeightSet)
{
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
    const nsStyleCoord maxWidth = position->mMaxWidth;
    if (maxWidth.ConvertsToLength()) {
        aSize.width = nsRuleNode::ComputeCoordPercentCalc(maxWidth, 0);
        aWidthSet = true;
    }
    // percentages and calc() with percentages are treated like 'none'

    const nsStyleCoord &maxHeight = position->mMaxHeight;
    if (maxHeight.ConvertsToLength()) {
        aSize.height = nsRuleNode::ComputeCoordPercentCalc(maxHeight, 0);
        aHeightSet = true;
    }
    // percentages and calc() with percentages are treated like 'none'

    nsIContent* content = aBox->GetContent();
    if (content && content->IsXULElement()) {
        nsAutoString value;
        nsresult error;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::maxwidth, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            aSize.width = val;
            aWidthSet = true;
        }

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::maxheight, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            aSize.height = val;

            aHeightSet = true;
        }
    }

    return (aWidthSet || aHeightSet);
}

bool
nsIFrame::AddXULFlex(nsIFrame* aBox, nscoord& aFlex)
{
    bool flexSet = false;

    // get the flexibility
    aFlex = aBox->StyleXUL()->mBoxFlex;

    // attribute value overrides CSS
    nsIContent* content = aBox->GetContent();
    if (content && content->IsXULElement()) {
        nsresult error;
        nsAutoString value;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::flex, value);
        if (!value.IsEmpty()) {
            value.Trim("%");
            aFlex = value.ToInteger(&error);
            flexSet = true;
        }
    }

    if (aFlex < 0)
      aFlex = 0;
    if (aFlex >= nscoord_MAX)
      aFlex = nscoord_MAX - 1;

    return flexSet || aFlex > 0;
}

void
nsBox::AddBorderAndPadding(nsSize& aSize)
{
  AddBorderAndPadding(this, aSize);
}

void
nsBox::AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize)
{
  nsMargin borderPadding(0,0,0,0);
  aBox->GetXULBorderAndPadding(borderPadding);
  AddMargin(aSize, borderPadding);
}

void
nsBox::AddMargin(nsIFrame* aChild, nsSize& aSize)
{
  nsMargin margin(0,0,0,0);
  aChild->GetXULMargin(margin);
  AddMargin(aSize, margin);
}

void
nsBox::AddMargin(nsSize& aSize, const nsMargin& aMargin)
{
  if (aSize.width != NS_INTRINSICSIZE)
    aSize.width += aMargin.left + aMargin.right;

  if (aSize.height != NS_INTRINSICSIZE)
     aSize.height += aMargin.top + aMargin.bottom;
}

nscoord
nsBox::BoundsCheck(nscoord aMin, nscoord aPref, nscoord aMax)
{
   if (aPref > aMax)
       aPref = aMax;

   if (aPref < aMin)
       aPref = aMin;

   return aPref;
}

nsSize
nsBox::BoundsCheckMinMax(const nsSize& aMinSize, const nsSize& aMaxSize)
{
  return nsSize(std::max(aMaxSize.width, aMinSize.width),
                std::max(aMaxSize.height, aMinSize.height));
}

nsSize
nsBox::BoundsCheck(const nsSize& aMinSize, const nsSize& aPrefSize, const nsSize& aMaxSize)
{
  return nsSize(BoundsCheck(aMinSize.width, aPrefSize.width, aMaxSize.width),
                BoundsCheck(aMinSize.height, aPrefSize.height, aMaxSize.height));
}

/*static*/ nsIFrame*
nsBox::GetChildXULBox(const nsIFrame* aFrame)
{
  // box layout ends at box-wrapped frames, so don't allow these frames
  // to report child boxes.
  return aFrame->IsXULBoxFrame() ? aFrame->PrincipalChildList().FirstChild() : nullptr;
}

/*static*/ nsIFrame*
nsBox::GetNextXULBox(const nsIFrame* aFrame)
{
  return aFrame->GetParent() &&
    aFrame->GetParent()->IsXULBoxFrame() ? aFrame->GetNextSibling() : nullptr;
}

/*static*/ nsIFrame*
nsBox::GetParentXULBox(const nsIFrame* aFrame)
{
  return aFrame->GetParent() &&
    aFrame->GetParent()->IsXULBoxFrame() ? aFrame->GetParent() : nullptr;
}

#ifdef DEBUG_LAYOUT
nsresult
nsBox::SetXULDebug(nsBoxLayoutState& aState, bool aDebug)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBox::GetDebugBoxAt( const nsPoint& aPoint,
                      nsIFrame**     aBox)
{
  nsRect thisRect(nsPoint(0,0), GetSize());
  if (!thisRect.Contains(aPoint))
    return NS_ERROR_FAILURE;

  nsIFrame* child = nsBox::GetChildXULBox(this);
  nsIFrame* hit = nullptr;

  *aBox = nullptr;
  while (nullptr != child) {
    nsresult rv = child->GetDebugBoxAt(aPoint - child->GetOffsetTo(this), &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aBox = hit;
    }
    child = GetNextXULBox(child);
  }

  // found a child
  if (*aBox) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


nsresult
nsBox::GetXULDebug(bool& aDebug)
{
  aDebug = false;
  return NS_OK;
}

#endif
