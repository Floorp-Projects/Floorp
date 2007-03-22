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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Eric D Vaughan <evaughan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsFrameManager.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsIWidget.h"
#include "nsIRenderingContext.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsITheme.h"
#include "nsIServiceManager.h"
#include "nsIBoxLayout.h"

#ifdef DEBUG_LAYOUT
PRInt32 gIndent = 0;
#endif

#ifdef DEBUG_LAYOUT
void
nsBoxAddIndents()
{
    for(PRInt32 i=0; i < gIndent; i++)
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
    sprintf(addr, "[@%p] ", NS_STATIC_CAST(void*, this));

    aResult.AppendASCII(addr);
    aResult.Append(name);
    aResult.AppendLiteral(" ");

    nsIContent* content = GetContent();

    // add on all the set attributes
    if (content) {
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
      nsCOMPtr<nsIDOMNamedNodeMap> namedMap;

      node->GetAttributes(getter_AddRefs(namedMap));
      PRUint32 length;
      namedMap->GetLength(&length);

      nsCOMPtr<nsIDOMNode> attribute;
      for (PRUint32 i = 0; i < length; ++i)
      {
        namedMap->Item(i, getter_AddRefs(attribute));
        nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(attribute));
        attr->GetName(name);
        nsAutoString value;
        attr->GetValue(value);
        AppendAttribute(name, value, aResult);
      }
    }
}

NS_IMETHODIMP
nsBox::DumpBox(FILE* aFile)
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
      SetDebug(aState, PR_TRUE);
    else
      SetDebug(aState, PR_FALSE);
  } else if (mState & NS_STATE_IS_ROOT) {
    SetDebug(aState, gDebug);
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
nsBox::BeginLayout(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT 

  nsBoxAddIndents();
  printf("Layout: ");
  DumpBox(stdout);
  printf("\n");
  gIndent++;
#endif

  // mark ourselves as dirty so no child under us
  // can post an incremental layout.
  // XXXldb Is this still needed?
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

  if (GetStateBits() & NS_FRAME_IS_DIRTY)
  {
    // If the parent is dirty, all the children are dirty (nsHTMLReflowState
    // does this too).
    nsIFrame* box;
    for (box = GetChildBox(); box; box = box->GetNextBox())
      box->AddStateBits(NS_FRAME_IS_DIRTY);
  }


#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsBox::DoLayout(nsBoxLayoutState& aState)
{
  return NS_OK;
}

nsresult
nsBox::EndLayout(nsBoxLayoutState& aState)
{

  #ifdef DEBUG_LAYOUT
      --gIndent;
  #endif

  return SyncLayout(aState);
}

PRBool nsBox::gGotTheme = PR_FALSE;
nsITheme* nsBox::gTheme = nsnull;

nsBox::nsBox()
{
  MOZ_COUNT_CTOR(nsBox);
  //mX = 0;
  //mY = 0;
  if (!gGotTheme) {
    gGotTheme = PR_TRUE;
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
  gGotTheme = PR_FALSE;
  NS_IF_RELEASE(gTheme);
}

NS_IMETHODIMP
nsBox::RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)
{
  return NS_OK;
}

nsresult
nsIFrame::GetClientRect(nsRect& aClientRect)
{
  aClientRect = mRect;
  aClientRect.MoveTo(0,0);

  nsMargin borderPadding;
  GetBorderAndPadding(borderPadding);

  aClientRect.Deflate(borderPadding);

  if (aClientRect.width < 0)
     aClientRect.width = 0;

  if (aClientRect.height < 0)
     aClientRect.height = 0;

 // NS_ASSERTION(aClientRect.width >=0 && aClientRect.height >= 0, "Content Size < 0");

  return NS_OK;
}

void
nsBox::SetBounds(nsBoxLayoutState& aState, const nsRect& aRect, PRBool aRemoveOverflowArea)
{
    NS_BOX_ASSERTION(this, aRect.width >=0 && aRect.height >= 0, "SetBounds Size < 0");

    nsRect rect(mRect);

    PRUint32 flags = 0;
    GetLayoutFlags(flags);

    PRUint32 stateFlags = aState.LayoutFlags();

    flags |= stateFlags;

    if (flags & NS_FRAME_NO_MOVE_FRAME)
      SetSize(nsSize(aRect.width, aRect.height));
    else
      SetRect(aRect);

    // Nuke the overflow area. The caller is responsible for restoring
    // it if necessary.
    if (aRemoveOverflowArea && (GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN)) {
      // remove the previously stored overflow area
      GetPresContext()->PropertyTable()->
        DeleteProperty(this, nsGkAtoms::overflowAreaProperty);
      RemoveStateBits(NS_FRAME_OUTSIDE_CHILDREN);
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

void
nsBox::GetLayoutFlags(PRUint32& aFlags)
{
  aFlags = 0;
}


NS_IMETHODIMP
nsIFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0, 0, 0, 0);
  nsresult rv = GetBorder(aBorderAndPadding);
  if (NS_FAILED(rv))
    return rv;

  nsMargin padding;
  rv = GetPadding(padding);
  if (NS_FAILED(rv))
    return rv;

  aBorderAndPadding += padding;

  return rv;
}

NS_IMETHODIMP
nsBox::GetBorder(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
    
  const nsStyleDisplay* disp = GetStyleDisplay();
  if (disp->mAppearance && gTheme) {
    // Go to the theme for the border.
    nsPresContext *context = GetPresContext();
    if (gTheme->ThemeSupportsWidget(context, this, disp->mAppearance)) {
      nsMargin margin(0, 0, 0, 0);
      gTheme->GetWidgetBorder(context->DeviceContext(), this,
                              disp->mAppearance, &margin);
      aMargin.top = context->DevPixelsToAppUnits(margin.top);
      aMargin.right = context->DevPixelsToAppUnits(margin.right);
      aMargin.bottom = context->DevPixelsToAppUnits(margin.bottom);
      aMargin.left = context->DevPixelsToAppUnits(margin.left);
      return NS_OK;
    }
  }

  aMargin = GetStyleBorder()->GetBorder();

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetPadding(nsMargin& aMargin)
{
  const nsStyleDisplay *disp = GetStyleDisplay();
  if (disp->mAppearance && gTheme) {
    // Go to the theme for the padding.
    nsPresContext *context = GetPresContext();
    if (gTheme->ThemeSupportsWidget(context, this, disp->mAppearance)) {
      nsMargin margin(0, 0, 0, 0);
      PRBool useThemePadding;

      useThemePadding = gTheme->GetWidgetPadding(context->DeviceContext(),
                                                 this, disp->mAppearance,
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
  GetStylePadding()->GetPadding(aMargin);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMargin(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
  GetStyleMargin()->GetMargin(aMargin);

  return NS_OK;
}

void
nsBox::SizeNeedsRecalc(nsSize& aSize)
{
  aSize.width  = -1;
  aSize.height = -1;
}

void
nsBox::CoordNeedsRecalc(PRInt32& aFlex)
{
  aFlex = -1;
}

PRBool
nsBox::DoesNeedRecalc(const nsSize& aSize)
{
  return (aSize.width == -1 || aSize.height == -1);
}

PRBool
nsBox::DoesNeedRecalc(nscoord aCoord)
{
  return (aCoord == -1);
}

PRBool
nsBox::GetWasCollapsed(nsBoxLayoutState& aState)
{
  return (GetStateBits() & NS_STATE_IS_COLLAPSED) != 0;
}

void
nsBox::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aCollapsed)
{
  if (aCollapsed)
     AddStateBits(NS_STATE_IS_COLLAPSED);
  else
     RemoveStateBits(NS_STATE_IS_COLLAPSED);
}

NS_IMETHODIMP
nsBox::SetLayoutManager(nsIBoxLayout* aLayout)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetLayoutManager(nsIBoxLayout** aLayout)
{
  *aLayout = nsnull;
  return NS_OK;
}


nsSize
nsBox::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize pref(0,0);
  DISPLAY_PREF_SIZE(this, pref);

  if (IsCollapsed(aState))
    return pref;

  AddBorderAndPadding(pref);
  nsIBox::AddCSSPrefSize(aState, this, pref);

  nsSize minSize = GetMinSize(aState);
  nsSize maxSize = GetMaxSize(aState);
  BoundsCheck(minSize, pref, maxSize);

  return pref;
}

nsSize
nsBox::GetMinSize(nsBoxLayoutState& aState)
{
  nsSize min(0,0);
  DISPLAY_MIN_SIZE(this, min);

  if (IsCollapsed(aState))
    return min;

  AddBorderAndPadding(min);
  nsIBox::AddCSSMinSize(aState, this, min);
  return min;
}

nsSize
nsBox::GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState)
{
  return nsSize(0, 0);
}

nsSize
nsBox::GetMaxSize(nsBoxLayoutState& aState)
{
  nsSize max(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, max);

  if (IsCollapsed(aState))
    return max;

  AddBorderAndPadding(max);
  nsIBox::AddCSSMaxSize(aState, this, max);
  return max;
}

nscoord
nsBox::GetFlex(nsBoxLayoutState& aState)
{
  nscoord flex = 0;

  GetDefaultFlex(flex);
  nsIBox::AddCSSFlex(aState, this, flex);

  return flex;
}

PRUint32
nsIFrame::GetOrdinal(nsBoxLayoutState& aState)
{
  PRUint32 ordinal = DEFAULT_ORDINAL_GROUP;
  nsIBox::AddCSSOrdinal(aState, this, ordinal);

  return ordinal;
}

nscoord
nsBox::GetBoxAscent(nsBoxLayoutState& aState)
{
  if (IsCollapsed(aState))
    return 0;

  return GetPrefSize(aState).height;
}

PRBool
nsBox::IsCollapsed(nsBoxLayoutState& aState)
{
  PRBool collapsed = PR_FALSE;
  nsIBox::AddCSSCollapsed(aState, this, collapsed);

  return collapsed;
}

nsresult
nsIFrame::Layout(nsBoxLayoutState& aState)
{
  nsBox *box = NS_STATIC_CAST(nsBox*, this);
  DISPLAY_LAYOUT(box);

  box->BeginLayout(aState);

  box->DoLayout(aState);

  box->EndLayout(aState);

  return NS_OK;
}

PRBool
nsBox::DoesClipChildren()
{
  const nsStyleDisplay* display = GetStyleDisplay();
  NS_ASSERTION((display->mOverflowY == NS_STYLE_OVERFLOW_CLIP) ==
               (display->mOverflowX == NS_STYLE_OVERFLOW_CLIP),
               "If one overflow is clip, the other should be too");
  return display->mOverflowX == NS_STYLE_OVERFLOW_CLIP;
}

nsresult
nsBox::SyncLayout(nsBoxLayoutState& aState)
{
  /*
  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed) {
    CollapseChild(aState, this, PR_TRUE);
    return NS_OK;
  }
  */
  

  if (GetStateBits() & NS_FRAME_IS_DIRTY)
     Redraw(aState);

  RemoveStateBits(NS_FRAME_HAS_DIRTY_CHILDREN | NS_FRAME_IS_DIRTY
                  | NS_FRAME_FIRST_REFLOW | NS_FRAME_IN_REFLOW);

  nsPresContext* presContext = aState.PresContext();

  PRUint32 flags = 0;
  GetLayoutFlags(flags);

  PRUint32 stateFlags = aState.LayoutFlags();

  flags |= stateFlags;

  nsRect rect(nsPoint(0, 0), GetSize());

  if (ComputesOwnOverflowArea()) {
    nsRect* overflow = GetOverflowAreaProperty();
    if (overflow)
      rect = *overflow;

  } else {
    if (!DoesClipChildren()) {
      // See if our child frames caused us to overflow after being laid
      // out. If so, store the overflow area.  This normally can't happen
      // in XUL, but it can happen with the CSS 'outline' property and
      // possibly with other exotic stuff (e.g. relatively positioned
      // frames in HTML inside XUL).
      nsIFrame* box = GetChildBox();
      while (box) {
        nsRect* overflowArea = box->GetOverflowAreaProperty();
        nsRect bounds = overflowArea ? *overflowArea + box->GetPosition()
                                     : box->GetRect();
        rect.UnionRect(rect, bounds);

        box = box->GetNextBox();
      }
    }

    FinishAndStoreOverflow(&rect, GetSize());
  }

  nsIView* view = GetView();
  if (view) {
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    nsHTMLContainerFrame::SyncFrameViewAfterReflow(
                             presContext, 
                             this,
                             view,
                             &rect,
                             flags);
  } 

  if (IsBoxFrame())
    mState &= ~(NS_STATE_STYLE_CHANGE);

  return NS_OK;
}

nsresult
nsIFrame::Redraw(nsBoxLayoutState& aState,
                 const nsRect*   aDamageRect,
                 PRBool          aImmediate)
{
  if (aState.PaintingDisabled())
    return NS_OK;

  nsRect damageRect(0,0,0,0);
  if (aDamageRect)
    damageRect = *aDamageRect;
  else
    damageRect = GetOverflowRect();

  Invalidate(damageRect, aImmediate);

  return NS_OK;
}

PRBool 
nsIBox::AddCSSPrefSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{
    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;

    // add in the css min, max, pref
    const nsStylePosition* position = aBox->GetStylePosition();

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.width = position->mWidth.GetCoordValue();
        widthSet = PR_TRUE;
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.height = position->mHeight.GetCoordValue();     
        heightSet = PR_TRUE;
    }
    
    nsIContent* content = aBox->GetContent();
    // ignore 'height' and 'width' attributes if the actual element is not XUL
    // For example, we might be magic XUL frames whose primary content is an HTML
    // <select>
    if (content && content->IsNodeOfType(nsINode::eXUL)) {
        nsPresContext* presContext = aState.PresContext();

        nsAutoString value;
        PRInt32 error;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::width, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            aSize.width =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            widthSet = PR_TRUE;
        }

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::height, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            aSize.height =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            heightSet = PR_TRUE;
        }
    }

    return (widthSet && heightSet);
}


PRBool 
nsIBox::AddCSSMinSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{

    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;
    PRBool canOverride = PR_TRUE;

    // See if a native theme wants to supply a minimum size.
    const nsStyleDisplay* display = aBox->GetStyleDisplay();
    if (display->mAppearance) {
      nsITheme *theme = aState.PresContext()->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aState.PresContext(), aBox, display->mAppearance)) {
        nsSize size;
        nsIRenderingContext* rendContext = aState.GetRenderingContext();
        if (rendContext) {
          theme->GetMinimumWidgetSize(rendContext, aBox,
                                      display->mAppearance, &size, &canOverride);
          if (size.width) {
            aSize.width = aState.PresContext()->DevPixelsToAppUnits(size.width);
            widthSet = PR_TRUE;
          }
          if (size.height) {
            aSize.height = aState.PresContext()->DevPixelsToAppUnits(size.height);
            heightSet = PR_TRUE;
          }
        }
      }
    }

    // add in the css min, max, pref
    const nsStylePosition* position = aBox->GetStylePosition();

    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min && (!widthSet || (min > aSize.width && canOverride))) {
           aSize.width = min;
           widthSet = PR_TRUE;
        }
    } else if (position->mMinWidth.GetUnit() == eStyleUnit_Percent) {
        float min = position->mMinWidth.GetPercentValue();
        NS_ASSERTION(min == 0.0f, "Non-zero percentage values not currently supported");
        aSize.width = 0;
        widthSet = PR_TRUE;
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min && (!heightSet || (min > aSize.height && canOverride))) {
           aSize.height = min;
           heightSet = PR_TRUE;
        }
    } else if (position->mMinHeight.GetUnit() == eStyleUnit_Percent) {
        float min = position->mMinHeight.GetPercentValue();
        NS_ASSERTION(min == 0.0f, "Non-zero percentage values not currently supported");
        aSize.height = 0;
        heightSet = PR_TRUE;
    }

    nsIContent* content = aBox->GetContent();
    if (content) {
        nsPresContext* presContext = aState.PresContext();

        nsAutoString value;
        PRInt32 error;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::minwidth, value);
        if (!value.IsEmpty())
        {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            if (val > aSize.width)
              aSize.width = val;
            widthSet = PR_TRUE;
        }

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::minheight, value);
        if (!value.IsEmpty())
        {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            if (val > aSize.height)
              aSize.height = val;

            heightSet = PR_TRUE;
        }
    }

    return (widthSet && heightSet);
}

PRBool 
nsIBox::AddCSSMaxSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{  

    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;

    // add in the css min, max, pref
    const nsStylePosition* position = aBox->GetStylePosition();

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.width = max;
        widthSet = PR_TRUE;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.height = max;
        heightSet = PR_TRUE;
    }

    nsIContent* content = aBox->GetContent();
    if (content) {
        nsPresContext* presContext = aState.PresContext();

        nsAutoString value;
        PRInt32 error;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::maxwidth, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            aSize.width = val;
            widthSet = PR_TRUE;
        }

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::maxheight, value);
        if (!value.IsEmpty()) {
            value.Trim("%");

            nscoord val =
              nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
            aSize.height = val;

            heightSet = PR_TRUE;
        }
    }

    return (widthSet || heightSet);
}

PRBool 
nsIBox::AddCSSFlex(nsBoxLayoutState& aState, nsIBox* aBox, nscoord& aFlex)
{
    PRBool flexSet = PR_FALSE;

    // get the flexibility
    nsIContent* content = aBox->GetContent();
    if (content) {
        PRInt32 error;
        nsAutoString value;

        content->GetAttr(kNameSpaceID_None, nsGkAtoms::flex, value);
        if (!value.IsEmpty()) {
            value.Trim("%");
            aFlex = value.ToInteger(&error);
            flexSet = PR_TRUE;
        }
        else {
          // No attribute value.  Check CSS.
          const nsStyleXUL* boxInfo = aBox->GetStyleXUL();
          if (boxInfo->mBoxFlex > 0.0f) {
            // The flex was defined in CSS.
            aFlex = (nscoord)boxInfo->mBoxFlex;
            flexSet = PR_TRUE;
          }
        }
    }

    if (aFlex < 0)
      aFlex = 0;
    if (aFlex >= nscoord_MAX)
      aFlex = nscoord_MAX - 1;

    return flexSet;
}

PRBool 
nsIBox::AddCSSCollapsed(nsBoxLayoutState& aState, nsIBox* aBox, PRBool& aCollapsed)
{
  aCollapsed = aBox->GetStyleVisibility()->mVisible ==
               NS_STYLE_VISIBILITY_COLLAPSE;
  return PR_TRUE;
}

PRBool 
nsIBox::AddCSSOrdinal(nsBoxLayoutState& aState, nsIBox* aBox, PRUint32& aOrdinal)
{
  PRBool ordinalSet = PR_FALSE;
  
  // get the flexibility
  nsIContent* content = aBox->GetContent();
  if (content) {
    PRInt32 error;
    nsAutoString value;

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::ordinal, value);
    if (!value.IsEmpty()) {
      aOrdinal = value.ToInteger(&error);
      ordinalSet = PR_TRUE;
    }
    else {
      // No attribute value.  Check CSS.
      const nsStyleXUL* boxInfo = aBox->GetStyleXUL();
      if (boxInfo->mBoxOrdinal > 1) {
        // The ordinal group was defined in CSS.
        aOrdinal = (nscoord)boxInfo->mBoxOrdinal;
        ordinalSet = PR_TRUE;
      }
    }
  }

  return ordinalSet;
}

void
nsBox::AddBorderAndPadding(nsSize& aSize)
{
  AddBorderAndPadding(this, aSize);
}

void
nsBox::AddMargin(nsSize& aSize)
{
  AddMargin(this, aSize);
}

void
nsBox::AddBorderAndPadding(nsIBox* aBox, nsSize& aSize)
{
  nsMargin borderPadding(0,0,0,0);
  aBox->GetBorderAndPadding(borderPadding);
  AddMargin(aSize, borderPadding);
}

void
nsBox::AddMargin(nsIBox* aChild, nsSize& aSize)
{
  nsMargin margin(0,0,0,0);
  aChild->GetMargin(margin);
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

void
nsBox::BoundsCheck(nscoord& aMin, nscoord& aPref, nscoord& aMax)
{
   if (aMax < aMin)
       aMax = aMin;

   if (aPref > aMax)
       aPref = aMax;

   if (aPref < aMin)
       aPref = aMin;
}

void
nsBox::BoundsCheckMinMax(nsSize& aMinSize, nsSize& aMaxSize)
{
  if (aMaxSize.width < aMinSize.width) {
    aMaxSize.width = aMinSize.width;
  }

  if (aMaxSize.height < aMinSize.height)
    aMaxSize.height = aMinSize.height;
}

void
nsBox::BoundsCheck(nsSize& aMinSize, nsSize& aPrefSize, nsSize& aMaxSize)
{
   BoundsCheck(aMinSize.width, aPrefSize.width, aMaxSize.width);
   BoundsCheck(aMinSize.height, aPrefSize.height, aMaxSize.height);
}

#ifdef DEBUG_LAYOUT
nsresult
nsBox::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBox::GetDebugBoxAt( const nsPoint& aPoint,
                      nsIBox**     aBox)
{
  nsRect thisRect(nsPoint(0,0), GetSize());
  if (!thisRect.Contains(aPoint))
    return NS_ERROR_FAILURE;

  nsIBox* child = GetChildBox();
  nsIBox* hit = nsnull;

  *aBox = nsnull;
  while (nsnull != child) {
    nsresult rv = child->GetDebugBoxAt(aPoint - child->GetOffsetTo(this), &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aBox = hit;
    }
    child = child->GetNextBox();
  }

  // found a child
  if (*aBox) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsBox::GetDebug(PRBool& aDebug)
{
  aDebug = PR_FALSE;
  return NS_OK;
}

#endif

PRBool
nsBox::GetMouseThrough() const
{
  if (mParent && mParent->IsBoxFrame())
    return mParent->GetMouseThrough();

  return PR_FALSE;
}

PRBool
nsBox::GetDefaultFlex(PRInt32& aFlex) 
{ 
  aFlex = 0; 
  return PR_TRUE; 
}
