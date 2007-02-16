/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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

#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsSpaceManager.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsIServiceManager.h"
#include "nsIBoxLayout.h"
#include "nsSprocketLayout.h"
#include "nsIDocument.h"
#include "nsIScrollableFrame.h"
#include "nsWidgetsCID.h"
#include "nsCSSAnonBoxes.h"
#include "nsIScrollableView.h"
#include "nsHTMLContainerFrame.h"
#include "nsIWidget.h"
#include "nsIEventStateManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsITheme.h"
#include "nsTransform2D.h"
#include "nsIEventStateManager.h"
#include "nsEventDispatcher.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"

// Needed for Print Preview
#include "nsIDocument.h"
#include "nsIURI.h"


static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

//define DEBUG_REDRAW

#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8

//#define TEST_SANITY

#ifdef DEBUG_rods
//#define DO_NOISY_REFLOW
#endif

#ifdef DEBUG_LAYOUT
PRBool nsBoxFrame::gDebug = PR_FALSE;
nsIBox* nsBoxFrame::mDebugChild = nsnull;
#endif

nsIFrame*
NS_NewBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
{
  return new (aPresShell) nsBoxFrame(aPresShell, aContext, aIsRoot, aLayoutManager);
} // NS_NewBoxFrame

nsBoxFrame::nsBoxFrame(nsIPresShell* aPresShell,
                       nsStyleContext* aContext,
                       PRBool aIsRoot,
                       nsIBoxLayout* aLayoutManager) :
  nsContainerFrame(aContext),
  mMouseThrough(unset)
{
  mState |= NS_FRAME_IS_BOX;
  mState |= NS_STATE_IS_HORIZONTAL;
  mState |= NS_STATE_AUTO_STRETCH;

  if (aIsRoot) 
     mState |= NS_STATE_IS_ROOT;

  mValign = vAlign_Top;
  mHalign = hAlign_Left;
  
  // if no layout manager specified us the static sprocket layout
  nsCOMPtr<nsIBoxLayout> layout = aLayoutManager;

  if (layout == nsnull) {
    NS_NewSprocketLayout(aPresShell, layout);
  }

  SetLayoutManager(layout);
}

nsBoxFrame::~nsBoxFrame()
{
}

NS_IMETHODIMP
nsBoxFrame::GetVAlign(Valignment& aAlign)
{
   aAlign = mValign;
   return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetHAlign(Halignment& aAlign)
{
   aAlign = mHalign;
   return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::SetInitialChildList(nsIAtom*        aListName,
                                nsIFrame*       aChildList)
{
  nsresult r = nsContainerFrame::SetInitialChildList(aListName, aChildList);
  if (r == NS_OK) {
    // initialize our list of infos.
    nsBoxLayoutState state(GetPresContext());
    CheckBoxOrder(state);
    if (mLayoutManager)
      mLayoutManager->ChildrenSet(this, state, mFrames.FirstChild());
  } else {
    NS_WARNING("Warning add child failed!!\n");
  }

  return r;
}

NS_IMETHODIMP
nsBoxFrame::DidSetStyleContext()
{
  // The values that CacheAttributes() computes depend on our style,
  // so we need to recompute them here...
  CacheAttributes();

  return NS_OK;
}

/**
 * Initialize us. This is a good time to get the alignment of the box
 */
NS_IMETHODIMP
nsBoxFrame::Init(nsIContent*      aContent,
                 nsIFrame*        aParent,
                 nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  MarkIntrinsicWidthsDirty();

  // see if we need a widget
  if (aParent && aParent->IsBoxFrame()) {
    if (aParent->ChildrenMustHaveWidgets()) {
        nsHTMLContainerFrame::CreateViewForFrame(this, nsnull, PR_TRUE);

        nsIView* view = GetView();
        if (!view->HasWidget())
           view->CreateWidget(kWidgetCID);   
    }
  }

  CacheAttributes();

#ifdef DEBUG_LAYOUT
    // if we are root and this
  if (mState & NS_STATE_IS_ROOT) 
      GetDebugPref(GetPresContext());
#endif

  mMouseThrough = unset;

  UpdateMouseThrough();

  // register access key
  rv = RegUnregAccessKey(PR_TRUE);

  return rv;
}

void nsBoxFrame::UpdateMouseThrough()
{
  if (mContent) {
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::never, &nsGkAtoms::always, nsnull};
    static const eMouseThrough values[] = {never, always};
    PRInt32 index = mContent->FindAttrValueIn(kNameSpaceID_None,
        nsGkAtoms::mousethrough, strings, eCaseMatters);
    if (index >= 0) {
      mMouseThrough = values[index];
    }
  }
}

PRBool
nsBoxFrame::GetMouseThrough() const
{
  switch(mMouseThrough)
  {
    case always:
      return PR_TRUE;
    case never:
      return PR_FALSE;
    case unset:
      if (mParent && mParent->IsBoxFrame())
        return mParent->GetMouseThrough();
  }

  return PR_FALSE;
}

void
nsBoxFrame::CacheAttributes()
{
  /*
  printf("Caching: ");
  DumpBox(stdout);
  printf("\n");
   */

  mValign = vAlign_Top;
  mHalign = hAlign_Left;

  PRBool orient = PR_FALSE;
  GetInitialOrientation(orient); 
  if (orient)
    mState |= NS_STATE_IS_HORIZONTAL;
  else
    mState &= ~NS_STATE_IS_HORIZONTAL;

  PRBool normal = PR_TRUE;
  GetInitialDirection(normal); 
  if (normal)
    mState |= NS_STATE_IS_DIRECTION_NORMAL;
  else
    mState &= ~NS_STATE_IS_DIRECTION_NORMAL;

  GetInitialVAlignment(mValign);
  GetInitialHAlignment(mHalign);
  
  PRBool equalSize = PR_FALSE;
  GetInitialEqualSize(equalSize); 
  if (equalSize)
        mState |= NS_STATE_EQUAL_SIZE;
    else
        mState &= ~NS_STATE_EQUAL_SIZE;

  PRBool autostretch = mState & NS_STATE_AUTO_STRETCH;
  GetInitialAutoStretch(autostretch);
  if (autostretch)
        mState |= NS_STATE_AUTO_STRETCH;
     else
        mState &= ~NS_STATE_AUTO_STRETCH;


#ifdef DEBUG_LAYOUT
  PRBool debug = mState & NS_STATE_SET_TO_DEBUG;
  PRBool debugSet = GetInitialDebug(debug); 
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
PRBool
nsBoxFrame::GetInitialDebug(PRBool& aDebug)
{
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
    return PR_FALSE;

  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_false, &nsGkAtoms::_true, nsnull};
  PRInt32 index = content->FindAttrValueIn(kNameSpaceID_None,
      nsGkAtoms::debug, strings, eCaseMatters);
  if (index >= 0) {
    aDebug = index == 1;
    return PR_TRUE;
  }

  return PR_FALSE;
}
#endif

PRBool
nsBoxFrame::GetInitialHAlignment(nsBoxFrame::Halignment& aHalign)
{
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));
  if (!content)
    return PR_FALSE;

  // XXXdwh Everything inside this if statement is deprecated code.
  static nsIContent::AttrValuesArray alignStrings[] =
    {&nsGkAtoms::left, &nsGkAtoms::right, nsnull};
  static const Halignment alignValues[] = {hAlign_Left, hAlign_Right};
  PRInt32 index = content->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::align,
      alignStrings, eCaseMatters);
  if (index >= 0) {
    aHalign = alignValues[index];
    return PR_TRUE;
  }
      
  // Now that the deprecated stuff is out of the way, we move on to check the appropriate 
  // attribute.  For horizontal boxes, we are checking the PACK attribute.  For vertical boxes
  // we are checking the ALIGN attribute.
  nsIAtom* attrName = IsHorizontal() ? nsGkAtoms::pack : nsGkAtoms::align;
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::start, &nsGkAtoms::center, &nsGkAtoms::end, nsnull};
  static const Halignment values[] =
    {hAlign_Left/*not used*/, hAlign_Left, hAlign_Center, hAlign_Right};
  index = content->FindAttrValueIn(kNameSpaceID_None, attrName,
      strings, eCaseMatters);

  if (index == nsIContent::ATTR_VALUE_NO_MATCH) {
    // The attr was present but had a nonsensical value. Revert to the default.
    return PR_FALSE;
  }
  if (index > 0) {    
    aHalign = values[index];
    return PR_TRUE;
  }

  // Now that we've checked for the attribute it's time to check CSS.  For 
  // horizontal boxes we're checking PACK.  For vertical boxes we are checking
  // ALIGN.
  const nsStyleXUL* boxInfo = GetStyleXUL();
  if (IsHorizontal()) {
    switch (boxInfo->mBoxPack) {
      case NS_STYLE_BOX_PACK_START:
        aHalign = nsBoxFrame::hAlign_Left;
        return PR_TRUE;
      case NS_STYLE_BOX_PACK_CENTER:
        aHalign = nsBoxFrame::hAlign_Center;
        return PR_TRUE;
      case NS_STYLE_BOX_PACK_END:
        aHalign = nsBoxFrame::hAlign_Right;
        return PR_TRUE;
      default: // Nonsensical value. Just bail.
        return PR_FALSE;
    }
  }
  else {
    switch (boxInfo->mBoxAlign) {
      case NS_STYLE_BOX_ALIGN_START:
        aHalign = nsBoxFrame::hAlign_Left;
        return PR_TRUE;
      case NS_STYLE_BOX_ALIGN_CENTER:
        aHalign = nsBoxFrame::hAlign_Center;
        return PR_TRUE;
      case NS_STYLE_BOX_ALIGN_END:
        aHalign = nsBoxFrame::hAlign_Right;
        return PR_TRUE;
      default: // Nonsensical value. Just bail.
        return PR_FALSE;
    }
  }

  return PR_FALSE;
}

PRBool
nsBoxFrame::GetInitialVAlignment(nsBoxFrame::Valignment& aValign)
{
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));
  if (!content)
    return PR_FALSE;

  static nsIContent::AttrValuesArray valignStrings[] =
    {&nsGkAtoms::top, &nsGkAtoms::baseline, &nsGkAtoms::middle, &nsGkAtoms::bottom, nsnull};
  static const Valignment valignValues[] =
    {vAlign_Top, vAlign_BaseLine, vAlign_Middle, vAlign_Bottom};
  PRInt32 index = content->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::valign,
      valignStrings, eCaseMatters);
  if (index >= 0) {
    aValign = valignValues[index];
    return PR_TRUE;
  }

  // Now that the deprecated stuff is out of the way, we move on to check the appropriate 
  // attribute.  For horizontal boxes, we are checking the ALIGN attribute.  For vertical boxes
  // we are checking the PACK attribute.
  nsIAtom* attrName = IsHorizontal() ? nsGkAtoms::align : nsGkAtoms::pack;
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::start, &nsGkAtoms::center,
     &nsGkAtoms::baseline, &nsGkAtoms::end, nsnull};
  static const Valignment values[] =
    {vAlign_Top/*not used*/, vAlign_Top, vAlign_Middle, vAlign_BaseLine, vAlign_Bottom};
  index = content->FindAttrValueIn(kNameSpaceID_None, attrName,
      strings, eCaseMatters);
  if (index == nsIContent::ATTR_VALUE_NO_MATCH) {
    // The attr was present but had a nonsensical value. Revert to the default.
    return PR_FALSE;
  }
  if (index > 0) {
    aValign = values[index];
    return PR_TRUE;
  }

  // Now that we've checked for the attribute it's time to check CSS.  For 
  // horizontal boxes we're checking ALIGN.  For vertical boxes we are checking
  // PACK.
  const nsStyleXUL* boxInfo = GetStyleXUL();
  if (IsHorizontal()) {
    switch (boxInfo->mBoxAlign) {
      case NS_STYLE_BOX_ALIGN_START:
        aValign = nsBoxFrame::vAlign_Top;
        return PR_TRUE;
      case NS_STYLE_BOX_ALIGN_CENTER:
        aValign = nsBoxFrame::vAlign_Middle;
        return PR_TRUE;
      case NS_STYLE_BOX_ALIGN_BASELINE:
        aValign = nsBoxFrame::vAlign_BaseLine;
        return PR_TRUE;
      case NS_STYLE_BOX_ALIGN_END:
        aValign = nsBoxFrame::vAlign_Bottom;
        return PR_TRUE;
      default: // Nonsensical value. Just bail.
        return PR_FALSE;
    }
  }
  else {
    switch (boxInfo->mBoxPack) {
      case NS_STYLE_BOX_PACK_START:
        aValign = nsBoxFrame::vAlign_Top;
        return PR_TRUE;
      case NS_STYLE_BOX_PACK_CENTER:
        aValign = nsBoxFrame::vAlign_Middle;
        return PR_TRUE;
      case NS_STYLE_BOX_PACK_END:
        aValign = nsBoxFrame::vAlign_Bottom;
        return PR_TRUE;
      default: // Nonsensical value. Just bail.
        return PR_FALSE;
    }
  }

  return PR_FALSE;
}

void
nsBoxFrame::GetInitialOrientation(PRBool& aIsHorizontal)
{
 // see if we are a vertical or horizontal box.
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
    return;

  // Check the style system first.
  const nsStyleXUL* boxInfo = GetStyleXUL();
  if (boxInfo->mBoxOrient == NS_STYLE_BOX_ORIENT_HORIZONTAL)
    aIsHorizontal = PR_TRUE;
  else 
    aIsHorizontal = PR_FALSE;

  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::vertical, &nsGkAtoms::horizontal, nsnull};
  PRInt32 index = content->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::orient,
      strings, eCaseMatters);
  if (index >= 0) {
    aIsHorizontal = index == 1;
  }
}

void
nsBoxFrame::GetInitialDirection(PRBool& aIsNormal)
{
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
    return;

  if (IsHorizontal()) {
    // For horizontal boxes only, we initialize our value based off the CSS 'direction' property.
    // This means that BiDI users will end up with horizontally inverted chrome.
    aIsNormal = (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR); // If text runs RTL then so do we.
  }
  else
    aIsNormal = PR_TRUE; // Assume a normal direction in the vertical case.

  // Now check the style system to see if we should invert aIsNormal.
  const nsStyleXUL* boxInfo = GetStyleXUL();
  if (boxInfo->mBoxDirection == NS_STYLE_BOX_DIRECTION_REVERSE)
    aIsNormal = !aIsNormal; // Invert our direction.
  
  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::reverse, &nsGkAtoms::ltr, &nsGkAtoms::rtl, nsnull};
  PRInt32 index = content->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::dir,
      strings, eCaseMatters);
  if (index >= 0) {
    PRPackedBool values[] = {!aIsNormal, PR_TRUE, PR_FALSE};
    aIsNormal = values[index];
  }
}

/* Returns true if it was set.
 */
PRBool
nsBoxFrame::GetInitialEqualSize(PRBool& aEqualSize)
{
 // see if we are a vertical or horizontal box.
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
     return PR_FALSE;
  
  if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::equalsize,
                           nsGkAtoms::always, eCaseMatters)) {
    aEqualSize = PR_TRUE;
    return PR_TRUE;
  } 

  return PR_FALSE;
}

/* Returns true if it was set.
 */
PRBool
nsBoxFrame::GetInitialAutoStretch(PRBool& aStretch)
{
  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
     return PR_FALSE;
  
  // Check the align attribute.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::stretch, nsnull};
  PRInt32 index = content->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::align,
      strings, eCaseMatters);
  if (index != nsIContent::ATTR_MISSING && index != 0) {
    aStretch = index == 1;
    return PR_TRUE;
  }

  // Check the CSS box-align property.
  const nsStyleXUL* boxInfo = GetStyleXUL();
  aStretch = (boxInfo->mBoxAlign == NS_STYLE_BOX_ALIGN_STRETCH);

  return PR_TRUE;
}

NS_IMETHODIMP
nsBoxFrame::DidReflow(nsPresContext*           aPresContext,
                      const nsHTMLReflowState*  aReflowState,
                      nsDidReflowStatus         aStatus)
{
  nsFrameState preserveBits =
    mState & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
  nsresult rv = nsFrame::DidReflow(aPresContext, aReflowState, aStatus);
  mState |= preserveBits;
  return rv;
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
nsBoxFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsBoxLayoutState state(GetPresContext(), aRenderingContext);
  nsSize minSize = GetMinSize(state);

  // GetMinSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetMinSize added, which is the
  // result of GetBorderAndPadding.
  nsMargin bp;
  GetBorderAndPadding(bp);

  result = minSize.width - bp.LeftRight();

  return result;
}

/* virtual */ nscoord
nsBoxFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsBoxLayoutState state(GetPresContext(), aRenderingContext);
  nsSize prefSize = GetPrefSize(state);

  // GetPrefSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetPrefSize added, which is the
  // result of GetBorderAndPadding.
  nsMargin bp;
  GetBorderAndPadding(bp);

  result = prefSize.width - bp.LeftRight();

  return result;
}

NS_IMETHODIMP
nsBoxFrame::Reflow(nsPresContext*          aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   const nsHTMLReflowState& aReflowState,
                   nsReflowStatus&          aStatus)
{
  // If you make changes to this method, please keep nsLeafBoxFrame::Reflow
  // in sync, if the changes are applicable there.

  DO_GLOBAL_REFLOW_COUNT("nsBoxFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(aReflowState.ComputedWidth() >=0 && aReflowState.ComputedWidth() >= 0, "Computed Size < 0");

#ifdef DO_NOISY_REFLOW
  printf("\n-------------Starting BoxFrame Reflow ----------------------------\n");
  printf("%p ** nsBF::Reflow %d ", this, myCounter++);
  
  printSize("AW", aReflowState.availableWidth);
  printSize("AH", aReflowState.availableHeight);
  printSize("CW", aReflowState.ComputedWidth());
  printSize("CH", aReflowState.mComputedHeight);

  printf(" *\n");

#endif

  aStatus = NS_FRAME_COMPLETE;

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowState.rendContext);

  nsSize computedSize(aReflowState.ComputedWidth(),aReflowState.mComputedHeight);

  nsMargin m;
  m = aReflowState.mComputedBorderPadding;
  // GetBorderAndPadding(m);

  nsSize prefSize(0,0);

  // if we are told to layout intrinsic then get our preferred size.
  NS_ASSERTION(computedSize.width != NS_INTRINSICSIZE,
               "computed width should always be computed");
  if (computedSize.height == NS_INTRINSICSIZE) {
    prefSize = GetPrefSize(state);
    nsSize minSize = GetMinSize(state);
    nsSize maxSize = GetMaxSize(state);
    BoundsCheck(minSize, prefSize, maxSize);
  }

  // get our desiredSize
  computedSize.width += m.left + m.right;

  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) {
    computedSize.height = prefSize.height;
  } else {
    computedSize.height += m.top + m.bottom;
  }

  // handle reflow state min and max sizes

  if (computedSize.width > aReflowState.mComputedMaxWidth)
    computedSize.width = aReflowState.mComputedMaxWidth;

  if (computedSize.height > aReflowState.mComputedMaxHeight)
    computedSize.height = aReflowState.mComputedMaxHeight;

  if (computedSize.width < aReflowState.mComputedMinWidth)
    computedSize.width = aReflowState.mComputedMinWidth;

  if (computedSize.height < aReflowState.mComputedMinHeight)
    computedSize.height = aReflowState.mComputedMinHeight;

  nsRect r(mRect.x, mRect.y, computedSize.width, computedSize.height);

  SetBounds(state, r);
 
  // layout our children
  Layout(state);
  
  // ok our child could have gotten bigger. So lets get its bounds
  
  // get the ascent
  nscoord ascent = mRect.height;

  // getting the ascent could be a lot of work. Don't get it if
  // we are the root. The viewport doesn't care about it.
  if (!(mState & NS_STATE_IS_ROOT)) {
    ascent = GetBoxAscent(state);
  }

  aDesiredSize.width  = mRect.width;
  aDesiredSize.height = mRect.height;
  aDesiredSize.ascent = ascent;

  // NS_FRAME_OUTSIDE_CHILDREN is set in SetBounds() above
  if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
    nsRect* overflowArea = GetOverflowAreaProperty();
    NS_ASSERTION(overflowArea, "Failed to set overflow area property");
    aDesiredSize.mOverflowArea = *overflowArea;
  } else {
    aDesiredSize.mOverflowArea = nsRect(nsPoint(0, 0), GetSize());
  }

#ifdef DO_NOISY_REFLOW
  {
    printf("%p ** nsBF(done) W:%d H:%d  ", this, aDesiredSize.width, aDesiredSize.height);

    if (maxElementSize) {
      printf("MW:%d\n", *maxElementWidth); 
    } else {
      printf("MW:?\n"); 
    }

  }
#endif

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsSize
nsBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(0,0);
  DISPLAY_PREF_SIZE(this, size);
  if (!DoesNeedRecalc(mPrefSize)) {
     size = mPrefSize;
     return size;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsCollapsed(aBoxLayoutState))
    return size;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSPrefSize(aBoxLayoutState, this, size))
  {
    if (mLayoutManager) {
      mLayoutManager->GetPrefSize(this, aBoxLayoutState, size);
      nsIBox::AddCSSPrefSize(aBoxLayoutState, this, size);
    } else
      size = nsBox::GetPrefSize(aBoxLayoutState);
  }

  nsSize minSize = GetMinSize(aBoxLayoutState);
  nsSize maxSize = GetMaxSize(aBoxLayoutState);
  BoundsCheck(minSize, size, maxSize);
  mPrefSize = size;
 
  return size;
}

nscoord
nsBoxFrame::GetBoxAscent(nsBoxLayoutState& aBoxLayoutState)
{
  if (!DoesNeedRecalc(mAscent))
     return mAscent;

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsCollapsed(aBoxLayoutState))
    return 0;

  if (mLayoutManager)
    mLayoutManager->GetAscent(this, aBoxLayoutState, mAscent);
  else
    mAscent = nsBox::GetBoxAscent(aBoxLayoutState);

  return mAscent;
}

nsSize
nsBoxFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(0,0);
  DISPLAY_MIN_SIZE(this, size);
  if (!DoesNeedRecalc(mMinSize)) {
    size = mMinSize;
    return size;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsCollapsed(aBoxLayoutState))
    return size;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSMinSize(aBoxLayoutState, this, size))
  {
    if (mLayoutManager) {
      mLayoutManager->GetMinSize(this, aBoxLayoutState, size);
      nsIBox::AddCSSMinSize(aBoxLayoutState, this, size);
    } else {
      size = nsBox::GetMinSize(aBoxLayoutState);
    }
  }
  
  mMinSize = size;

  return size;
}

nsSize
nsBoxFrame::GetMaxSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, size);
  if (!DoesNeedRecalc(mMaxSize)) {
    size = mMaxSize;
    return size;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  if (IsCollapsed(aBoxLayoutState))
    return size;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSMaxSize(aBoxLayoutState, this, size))
  {
    if (mLayoutManager) {
      mLayoutManager->GetMaxSize(this, aBoxLayoutState, size);
      nsIBox::AddCSSMaxSize(aBoxLayoutState, this, size);
    } else {
      size = nsBox::GetMaxSize(aBoxLayoutState);
    }
  }

  mMaxSize = size;

  return size;
}

nscoord
nsBoxFrame::GetFlex(nsBoxLayoutState& aBoxLayoutState)
{
  if (!DoesNeedRecalc(mFlex))
     return mFlex;

  mFlex = nsBox::GetFlex(aBoxLayoutState);

  return mFlex;
}

/**
 * If subclassing please subclass this method not layout. 
 * layout will call this method.
 */
NS_IMETHODIMP
nsBoxFrame::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 oldFlags = aState.LayoutFlags();
  aState.SetLayoutFlags(0);

  nsresult rv = NS_OK;
  if (mLayoutManager)
    rv = mLayoutManager->Layout(this, aState);

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

void
nsBoxFrame::Destroy()
{
  // unregister access key
  RegUnregAccessKey(PR_FALSE);

  // clean up the container box's layout manager and child boxes
  SetLayoutManager(nsnull);

  nsContainerFrame::Destroy();
} 

#ifdef DEBUG_LAYOUT
NS_IMETHODIMP
nsBoxFrame::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
  // see if our state matches the given debug state
  PRBool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  PRBool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
     if (aDebug) {
         mState |= NS_STATE_CURRENTLY_IN_DEBUG;
     } else {
         mState &= ~NS_STATE_CURRENTLY_IN_DEBUG;
     }
 
     SetDebugOnChildList(aState, mFirstChild, aDebug);

    MarkIntrinsicWidthsDirty();
  }

  return NS_OK;
}
#endif

/* virtual */ void
nsBoxFrame::MarkIntrinsicWidthsDirty()
{
  SizeNeedsRecalc(mPrefSize);
  SizeNeedsRecalc(mMinSize);
  SizeNeedsRecalc(mMaxSize);
  CoordNeedsRecalc(mFlex);
  CoordNeedsRecalc(mAscent);

  if (mLayoutManager) {
    nsBoxLayoutState state(GetPresContext());
    mLayoutManager->IntrinsicWidthsDirty(this, state);
  }

  // Don't call base class method, since everything it does is within an
  // IsBoxWrapped check.
}

NS_IMETHODIMP
nsBoxFrame::RemoveFrame(nsIAtom*        aListName,
                        nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(!aListName, "We don't support out-of-flow kids");
  nsPresContext* presContext = GetPresContext();
  nsBoxLayoutState state(presContext);

  // remove the child frame
  mFrames.RemoveFrame(aOldFrame);

  // notify the layout manager
  if (mLayoutManager)
    mLayoutManager->ChildrenRemoved(this, state, aOldFrame);

  // destroy the child frame
  aOldFrame->Destroy();

  // mark us dirty and generate a reflow command
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;
  GetPresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::InsertFrames(nsIAtom*        aListName,
                         nsIFrame*       aPrevFrame,
                         nsIFrame*       aFrameList)
{
   NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
                "inserting after sibling frame with different parent");
   NS_PRECONDITION(!aListName, "We don't support out-of-flow kids");
   nsBoxLayoutState state(GetPresContext());

   // insert the child frames
   mFrames.InsertFrames(this, aPrevFrame, aFrameList);

   // notify the layout manager
   if (mLayoutManager)
     mLayoutManager->ChildrenInserted(this, state, aPrevFrame, aFrameList);

#ifdef DEBUG_LAYOUT
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFrames.FirstChild(), PR_TRUE);
#endif

   mState |= NS_FRAME_HAS_DIRTY_CHILDREN;
   GetPresContext()->PresShell()->
     FrameNeedsReflow(this, nsIPresShell::eTreeChange);
   return NS_OK;
}


NS_IMETHODIMP
nsBoxFrame::AppendFrames(nsIAtom*        aListName,
                         nsIFrame*       aFrameList)
{
   NS_PRECONDITION(!aListName, "We don't support out-of-flow kids");
   nsBoxLayoutState state(GetPresContext());

   // append the new frames
   mFrames.AppendFrames(this, aFrameList);

   // notify the layout manager
   if (mLayoutManager)
     mLayoutManager->ChildrenAppended(this, state, aFrameList);

#ifdef DEBUG_LAYOUT
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFrames.FirstChild(), PR_TRUE);
#endif

   if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
     mState |= NS_FRAME_HAS_DIRTY_CHILDREN;
     GetPresContext()->PresShell()->
       FrameNeedsReflow(this, nsIPresShell::eTreeChange);
   }
   return NS_OK;
}



NS_IMETHODIMP
nsBoxFrame::AttributeChanged(PRInt32 aNameSpaceID,
                             nsIAtom* aAttribute,
                             PRInt32 aModType)
{
  nsresult rv = nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                                   aModType);

  // Ignore 'width', 'height', 'screenX', 'screenY' and 'sizemode' on a
  // <window>.
  nsIAtom *tag = mContent->Tag();
  if ((tag == nsGkAtoms::window ||
       tag == nsGkAtoms::page ||
       tag == nsGkAtoms::dialog ||
       tag == nsGkAtoms::wizard) &&
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

      PRBool orient = PR_TRUE;
      GetInitialOrientation(orient); 
      if (orient)
        mState |= NS_STATE_IS_HORIZONTAL;
      else
        mState &= ~NS_STATE_IS_HORIZONTAL;

      PRBool normal = PR_TRUE;
      GetInitialDirection(normal);
      if (normal)
        mState |= NS_STATE_IS_DIRECTION_NORMAL;
      else
        mState &= ~NS_STATE_IS_DIRECTION_NORMAL;

      GetInitialVAlignment(mValign);
      GetInitialHAlignment(mHalign);

      PRBool equalSize = PR_FALSE;
      GetInitialEqualSize(equalSize); 
      if (equalSize)
        mState |= NS_STATE_EQUAL_SIZE;
      else
        mState &= ~NS_STATE_EQUAL_SIZE;

#ifdef DEBUG_LAYOUT
      PRBool debug = mState & NS_STATE_SET_TO_DEBUG;
      PRBool debugSet = GetInitialDebug(debug);
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

      PRBool autostretch = mState & NS_STATE_AUTO_STRETCH;
      GetInitialAutoStretch(autostretch);
      if (autostretch)
        mState |= NS_STATE_AUTO_STRETCH;
      else
        mState &= ~NS_STATE_AUTO_STRETCH;
    }
    else if (aAttribute == nsGkAtoms::left ||
             aAttribute == nsGkAtoms::top) {
      mState &= ~NS_STATE_STACK_NOT_POSITIONED;
    }
    else if (aAttribute == nsGkAtoms::mousethrough) {
      UpdateMouseThrough();
    }

    mState |= NS_FRAME_IS_DIRTY;
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange);
  }
  else if (aAttribute == nsGkAtoms::ordinal) {
    nsBoxLayoutState state(GetPresContext());

    nsIFrame* frameToMove = this;
    if (GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      GetPresContext()->PresShell()->GetPlaceholderFrameFor(this,
                                                            &frameToMove);
      NS_ASSERTION(frameToMove, "Out of flow without placeholder?");
    }
    
    nsIBox* parent;
    frameToMove->GetParentBox(&parent);
    // If our parent is not a box, there's not much we can do... but in that
    // case our ordinal doesn't matter anyway, so that's ok.
    if (parent) {
      parent->RelayoutChildAtOrdinal(state, frameToMove);
      mState |= NS_FRAME_IS_DIRTY;
      // XXXldb Should this instead be a tree change on the child or parent?
      GetPresContext()->PresShell()->
        FrameNeedsReflow(frameToMove, nsIPresShell::eStyleChange);
    }
  }
  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  else if (aAttribute == nsGkAtoms::accesskey) {
    RegUnregAccessKey(PR_TRUE);
  }

  return rv;
}

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::GetDebugPref(nsPresContext* aPresContext)
{
    gDebug = nsContentUtils::GetBoolPref("xul.debug.box");
}

class nsDisplayXULDebug : public nsDisplayItem {
public:
  nsDisplayXULDebug(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULDebug);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULDebug() {
    MOZ_COUNT_DTOR(nsDisplayXULDebug);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) {
    NS_STATIC_CAST(nsBoxFrame*, mFrame)->
      DisplayDebugInfoFor(this, aPt - aBuilder->ToReferenceFrame(mFrame));
    return PR_TRUE;
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("ComboboxFocus")
};

void
nsDisplayXULDebug::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsBoxFrame*, mFrame)->
    PaintXULDebugOverlay(*aCtx, aBuilder->ToReferenceFrame(mFrame));
}

static void
PaintXULDebugBackground(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                        const nsRect& aDirtyRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsBoxFrame*, aFrame)->PaintXULDebugBackground(*aCtx, aPt);
}
#endif

nsresult
nsBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                             const nsRect&           aDirtyRect,
                             const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_LAYOUT
  // REVIEW: From GetFrameForPoint
  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
    rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(this, PaintXULDebugBackground, "XULDebugBackground"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aLists.Outlines()->AppendNewToTop(new (aBuilder)
        nsDisplayXULDebug(this));
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  rv = BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  // see if we have to draw a selection frame around this container
  return DisplaySelectionOverlay(aBuilder, aLists);
}

NS_IMETHODIMP
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
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
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
nsBoxFrame::PaintXULDebugBackground(nsIRenderingContext& aRenderingContext,
                                    nsPoint aPt)
{
  nsMargin border;
  GetBorder(border);

  nsMargin debugBorder;
  nsMargin debugMargin;
  nsMargin debugPadding;

  PRBool isHorizontal = IsHorizontal();

  GetDebugBorder(debugBorder);
  PixelMarginToTwips(GetPresContext(), debugBorder);

  GetDebugMargin(debugMargin);
  PixelMarginToTwips(GetPresContext(), debugMargin);

  GetDebugPadding(debugPadding);
  PixelMarginToTwips(GetPresContext(), debugPadding);

  nsRect inner(mRect);
  inner.MoveTo(aPt);
  inner.Deflate(debugMargin);
  inner.Deflate(border);
  //nsRect borderRect(inner);

  nscolor color;
  if (isHorizontal) {
    color = NS_RGB(0,0,255);
  } else {
    color = NS_RGB(255,0,0);
  }

  aRenderingContext.SetColor(color);

  //left
  nsRect r(inner);
  r.width = debugBorder.left;
  aRenderingContext.FillRect(r);

  // top
  r = inner;
  r.height = debugBorder.top;
  aRenderingContext.FillRect(r);

  //right
  r = inner;
  r.x = r.x + r.width - debugBorder.right;
  r.width = debugBorder.right;
  aRenderingContext.FillRect(r);

  //bottom
  r = inner;
  r.y = r.y + r.height - debugBorder.bottom;
  r.height = debugBorder.bottom;
  aRenderingContext.FillRect(r);

  
  // if we have dirty children or we are dirty 
  // place a green border around us.
  if (GetStateBits & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) {
     nsRect dirtyr(inner);
     aRenderingContext.SetColor(NS_RGB(0,255,0));
     aRenderingContext.DrawRect(dirtyr);
     aRenderingContext.SetColor(color);
  }
}

void
nsBoxFrame::PaintXULDebugOverlay(nsIRenderingContext& aRenderingContext,
                                 nsPoint aPt)
  nsMargin border;
  GetBorder(border);

  nsMargin debugMargin;
  GetDebugMargin(debugMargin);
  PixelMarginToTwips(GetPresContext(), debugMargin);

  nsRect inner(mRect);
  inner.MoveTo(aPt);
  inner.Deflate(debugMargin);
  inner.Deflate(border);

  nscoord onePixel = GetPresContext()->IntScaledPixelsToTwips(1);

  GetChildBox(&kid);
  while (nsnull != kid) {
    PRBool isHorizontal = IsHorizontal();

    nscoord x, y, borderSize, spacerSize;
    
    nsRect cr(kid->mRect);
    nsMargin margin;
    kid->GetMargin(margin);
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

    nsBoxLayoutState state(GetPresContext());
    nscoord flex = kid->GetFlex(state);

    if (!kid->IsCollapsed(state)) {
      aRenderingContext.SetColor(NS_RGB(255,255,255));

      if (isHorizontal) 
          borderSize = cr.width;
      else 
          borderSize = cr.height;
    
      DrawSpacer(GetPresContext(), aRenderingContext, isHorizontal, flex, x, y, borderSize, spacerSize);
    }

    kid->GetNextBox(&kid);
  }
}
#endif

NS_IMETHODIMP_(nsrefcnt) 
nsBoxFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsBoxFrame::Release(void)
{
    return NS_OK;
}

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::GetBoxName(nsAutoString& aName)
{
   GetFrameName(aName);
}
#endif

#ifdef DEBUG
NS_IMETHODIMP
nsBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Box"), aResult);
}
#endif

nsIAtom*
nsBoxFrame::GetType() const
{
  return nsGkAtoms::boxFrame;
}

PRBool
nsBoxFrame::IsFrameOfType(PRUint32 aFlags) const
{
  // This is bogus, but it's what we've always done.
  // (Given that we're replaced, we need to say we're a replaced element
  // that contains a block so nsHTMLReflowState doesn't tell us to be
  // NS_INTRINSICSIZE wide.)
  return !(aFlags & ~(eReplaced | eReplacedContainsBlock));
}

#ifdef DEBUG_LAYOUT
NS_IMETHODIMP
nsBoxFrame::GetDebug(PRBool& aDebug)
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
nsIBox*
nsBoxFrame::GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor)
{
  if (aFrame && !aFrame->IsBoxFrame())
    aIsAdaptor = PR_TRUE;

  return aFrame;
}

// REVIEW: GetCursor had debug-only event dumping code. I have replaced it
// with instrumentation in nsDisplayXULDebug.
nsresult 
nsBoxFrame::GetContentOf(nsIContent** aContent)
{
    // If we don't have a content node find a parent that does.
    nsIFrame *frame = this;
    while (frame) {
      *aContent = frame->GetContent();
      if (*aContent) {
        NS_ADDREF(*aContent);
        return NS_OK;
      }

      frame = frame->GetParent();
    }

    *aContent = nsnull;
    return NS_OK;
}

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::DrawLine(nsIRenderingContext& aRenderingContext, PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2)
{
    if (aHorizontal)
       aRenderingContext.DrawLine(x1,y1,x2,y2);
    else
       aRenderingContext.DrawLine(y1,x1,y2,x2);
}

void
nsBoxFrame::FillRect(nsIRenderingContext& aRenderingContext, PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height)
{
    if (aHorizontal)
       aRenderingContext.FillRect(x,y,width,height);
    else
       aRenderingContext.FillRect(y,x,height,width);
}

void 
nsBoxFrame::DrawSpacer(nsPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize)
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
            DrawLine(aRenderingContext, aHorizontal, x,y + spacerSize/2, x + size, y + spacerSize/2);
        } else {
            for (int i=0; i < coils; i++)
            {
                   DrawLine(aRenderingContext, aHorizontal, offset, center+halfSpacer, offset+halfCoilSize, center-halfSpacer);
                   DrawLine(aRenderingContext, aHorizontal, offset+halfCoilSize, center-halfSpacer, offset+coilSize, center+halfSpacer);

                   offset += coilSize;
            }
        }

        FillRect(aRenderingContext, aHorizontal, x + size - spacerSize/2, y, spacerSize/2, spacerSize);
        FillRect(aRenderingContext, aHorizontal, x, y, spacerSize/2, spacerSize);

        //DrawKnob(aPresContext, aRenderingContext, x + size - spacerSize, y, spacerSize);
}

void
nsBoxFrame::GetDebugBorder(nsMargin& aInset)
{
    aInset.SizeTo(2,2,2,2);

    if (IsHorizontal()) 
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

NS_IMETHODIMP 
nsBoxFrame::GetInset(nsMargin& margin)
{
  margin.SizeTo(0,0,0,0);

  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
    nsPresContext* presContext = GetPresContext();
    nsMargin debugMargin(0,0,0,0);
    nsMargin debugBorder(0,0,0,0);
    nsMargin debugPadding(0,0,0,0);
    GetDebugBorder(debugBorder);
    PixelMarginToTwips(presContext, debugBorder);
    GetDebugMargin(debugMargin);
    PixelMarginToTwips(presContext, debugMargin);
    GetDebugMargin(debugPadding);
    PixelMarginToTwips(presContext, debugPadding);
    margin += debugBorder;
    margin += debugMargin;
    margin += debugPadding;
  }

  return NS_OK;
}
#endif

void 
nsBoxFrame::PixelMarginToTwips(nsPresContext* aPresContext, nsMargin& aMarginPixels)
{
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  aMarginPixels.left   *= onePixel;
  aMarginPixels.right  *= onePixel;
  aMarginPixels.top    *= onePixel;
  aMarginPixels.bottom *= onePixel;
}


#ifdef DEBUG_LAYOUT
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
nsBoxFrame::GetValue(nsPresContext* aPresContext, PRInt32 a, PRInt32 b, char* ch) 
{
    if (a == NS_INTRINSICSIZE)
      sprintf(ch, "%d[SET]", b);             
    else
      sprintf(ch, "%d", a);             
}

nsresult
nsBoxFrame::DisplayDebugInfoFor(nsIBox*  aBox,
                                nsPoint& aPoint)
{
    nsBoxLayoutState state(GetPresContext());

    nscoord x = aPoint.x;
    nscoord y = aPoint.y;

    // get the area inside our border but not our debug margins.
    nsRect insideBorder(aBox->mRect);
    insideBorder.MoveTo(0,0):
    nsMargin border(0,0,0,0);
    aBox->GetBorderAndPadding(border);
    insideBorder.Deflate(border);

    PRBool isHorizontal = IsHorizontal();

    if (!insideBorder.Contains(nsPoint(x,y)))
        return NS_ERROR_FAILURE;

    //printf("%%%%%% inside box %%%%%%%\n");

    int count = 0;
    nsIBox* child = nsnull;
    aBox->GetChildBox(&child);

    nsMargin m;
    nsMargin m2;
    GetDebugBorder(m);
    PixelMarginToTwips(aPresContext, m);

    GetDebugMargin(m2);
    PixelMarginToTwips(aPresContext, m2);

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
                      DumpBox(stdout);
                      printf("\n");
                    }

                    if (child->GetContent()) {
                        printf("child #%d: ", count);
                        child->DumpBox(stdout);
                        printf("\n");
                    }

                    mDebugChild = child;

                    nsSize prefSizeCSS(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                    nsSize minSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                    nsSize maxSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                    nscoord flexCSS = NS_INTRINSICSIZE;

                    nsIBox::AddCSSPrefSize(state, child, prefSizeCSS);
                    nsIBox::AddCSSMinSize (state, child, minSizeCSS);
                    nsIBox::AddCSSMaxSize (state, child, maxSizeCSS);
                    nsIBox::AddCSSFlex    (state, child, flexCSS);

                    nsSize prefSize = child->GetPrefSize(state);
                    nsSize minSize = child->GetMinSize(state);
                    nsSize maxSize = child->GetMaxSize(state);
                    nscoord flexSize = child->GetFlex(state);
                    nscoord ascentSize = child->GetBoxAscent(state);

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

          child->GetNextBox(&child);
          count++;
        }
    } else {
    }

    mDebugChild = nsnull;

    return NS_OK;
}

void
nsBoxFrame::SetDebugOnChildList(nsBoxLayoutState& aState, nsIBox* aChild, PRBool aDebug)
{
    nsIBox* child = nsnull;
     GetChildBox(&child);
     while (child)
     {
        child->SetDebug(aState, aDebug);
        child->GetNextBox(&child);
     }
}
#endif

nsresult
nsBoxFrame::GetFrameSizeWithMargin(nsIBox* aBox, nsSize& aSize)
{
  nsRect rect(aBox->GetRect());
  nsMargin margin(0,0,0,0);
  aBox->GetMargin(margin);
  rect.Inflate(margin);
  aSize.width = rect.width;
  aSize.height = rect.height;
  return NS_OK;
}

/**
 * Boxed don't support fixed positionioning of their children.
 */
nsresult
nsBoxFrame::CreateViewForFrame(nsPresContext*  aPresContext,
                               nsIFrame*        aFrame,
                               nsStyleContext*  aStyleContext,
                               PRBool           aForce)
{
  // If we don't yet have a view, see if we need a view
  if (!aFrame->HasView()) {
    PRInt32 zIndex = 0;
    PRBool  autoZIndex = PR_FALSE;
    PRBool  fixedBackgroundAttachment = PR_FALSE;

    const nsStyleBackground* bg;
    PRBool isCanvas;
    PRBool hasBG =
        nsCSSRendering::FindBackground(aPresContext, aFrame, &bg, &isCanvas);
    const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();

    if (disp->mOpacity != 1.0f) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsBoxFrame::CreateViewForFrame: frame=%p opacity=%g",
         aFrame, disp->mOpacity));
      aForce = PR_TRUE;
    }

    // See if the frame has a fixed background attachment
    if (hasBG && bg->HasFixedBackground()) {
      aForce = PR_TRUE;
      fixedBackgroundAttachment = PR_TRUE;
    }
    
    // See if the frame is a scrolled frame
    if (!aForce) {
      if (aStyleContext->GetPseudoType() == nsCSSAnonBoxes::scrolledContent) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsBoxFrame::CreateViewForFrame: scrolled frame=%p", aFrame));
        aForce = PR_TRUE;
      }
    }

    if (aForce) {
      // Create a view
      nsIFrame* parent = aFrame->GetAncestorWithView();
      NS_ASSERTION(parent, "GetAncestorWithView failed");
      nsIView* parentView = parent->GetView();
      NS_ASSERTION(parentView, "no parent with view");
      nsIViewManager* viewManager = parentView->GetViewManager();
      NS_ASSERTION(nsnull != viewManager, "null view manager");

      // Create a view
      nsIView *view = viewManager->CreateView(aFrame->GetRect(), parentView);
      if (view) {
        // If the frame has a fixed background attachment, then indicate that the
        // view's contents should be repainted and not bitblt'd
        if (fixedBackgroundAttachment) {
          viewManager->SetViewBitBltEnabled(view, PR_FALSE);
        }
        
        // Insert the view into the view hierarchy. If the parent view is a
        // scrolling view we need to do this differently
        nsIScrollableView*  scrollingView = parentView->ToScrollableView();
        if (scrollingView) {
          scrollingView->SetScrolledView(view);
        } else {
          viewManager->SetViewZIndex(view, autoZIndex, zIndex);
          // XXX put view last in document order until we can do better
          viewManager->InsertChild(parentView, view, nsnull, PR_TRUE);
        }

        // See if the view should be hidden
        PRBool  viewIsVisible = PR_TRUE;
        PRBool  viewHasTransparentContent =
            !isCanvas &&
            (!hasBG ||
             (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT));

        const nsStyleVisibility* vis = aStyleContext->GetStyleVisibility();
        if (NS_STYLE_VISIBILITY_COLLAPSE == vis->mVisible) {
          viewIsVisible = PR_FALSE;
        }
        else if (NS_STYLE_VISIBILITY_HIDDEN == vis->mVisible) {
          // If it has a widget, hide the view because the widget can't deal with it
          if (view->HasWidget()) {
            viewIsVisible = PR_FALSE;
          }
          else {
            // If it's a container element, then leave the view visible, but
            // mark it as having transparent content. The reason we need to
            // do this is that child elements can override their parent's
            // hidden visibility and be visible anyway.
            //
            // Because this function is called before processing the content
            // object's child elements, we can't tell if it's a leaf by looking
            // at whether the frame has any child frames
            nsIContent* content = aFrame->GetContent();

            if (content && content->IsNodeOfType(nsINode::eELEMENT)) {
              // The view needs to be visible, but marked as having transparent
              // content
              viewHasTransparentContent = PR_TRUE;
            } else {
              // Go ahead and hide the view
              viewIsVisible = PR_FALSE;
            }
          }
        }

        if (viewIsVisible) {
          if (viewHasTransparentContent) {
            viewManager->SetViewContentTransparency(view, PR_TRUE);
          }

        } else {
          viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
        }

        viewManager->SetViewOpacity(view, disp->mOpacity);
      }

      // Remember our view
      aFrame->SetView(view);

      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsBoxFrame::CreateViewForFrame: frame=%p view=%p",
         aFrame));
      if (!view)
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

// If you make changes to this function, check its counterparts
// in nsTextBoxFrame and nsAreaFrame
nsresult
nsBoxFrame::RegUnregAccessKey(PRBool aDoReg)
{
  // if we have no content, we can't do anything
  if (!mContent)
    return NS_ERROR_FAILURE;

  // find out what type of element this is
  nsIAtom *atom = mContent->Tag();

  // only support accesskeys for the following elements
  if (atom != nsGkAtoms::button &&
      atom != nsGkAtoms::toolbarbutton &&
      atom != nsGkAtoms::checkbox &&
      atom != nsGkAtoms::textbox &&
      atom != nsGkAtoms::tab &&
      atom != nsGkAtoms::radio) {
    return NS_OK;
  }

  nsAutoString accessKey;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);

  if (accessKey.IsEmpty())
    return NS_OK;

  // With a valid PresContext we can get the ESM 
  // and register the access key
  nsIEventStateManager *esm = GetPresContext()->EventStateManager();

  nsresult rv;

  PRUint32 key = accessKey.First();
  if (aDoReg)
    rv = esm->RegisterAccessKey(mContent, key);
  else
    rv = esm->UnregisterAccessKey(mContent, key);

  return rv;
}

void
nsBoxFrame::FireDOMEventSynch(const nsAString& aDOMEventName, nsIContent *aContent)
{
  // XXX This will be deprecated, because it is not good to fire synchronous DOM events
  // from layout. It's better to use nsFrame::FireDOMEvent() which is asynchronous.
  nsPresContext *presContext = GetPresContext();
  nsIContent *content = aContent ? aContent : mContent;
  if (content && presContext) {
    // Fire a DOM event
    nsCOMPtr<nsIDOMEvent> event;
    if (NS_SUCCEEDED(nsEventDispatcher::CreateEvent(presContext, nsnull,
                                                    NS_LITERAL_STRING("Events"),
                                                    getter_AddRefs(event)))) {
      event->InitEvent(aDOMEventName, PR_TRUE, PR_TRUE);

      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
      privateEvent->SetTrusted(PR_TRUE);

      nsEventDispatcher::DispatchDOMEvent(content, nsnull, event,
                                          presContext, nsnull);
    }
  }
}

void 
nsBoxFrame::CheckBoxOrder(nsBoxLayoutState& aState)
{
  // Run through our list of children and check whether we
  // need to sort them.  Count up the children at the same
  // time, since we're already traversing the list.
  PRBool orderBoxes = PR_FALSE;
  PRInt32 childCount = 0;
  nsIFrame *child = mFrames.FirstChild();

  while (child) {
    ++childCount;

    PRUint32 ordinal = child->GetOrdinal(aState);
    if (ordinal != DEFAULT_ORDINAL_GROUP)
      orderBoxes = PR_TRUE;

    child = child->GetNextSibling();
  }

  if (!orderBoxes || childCount < 2)
    return;

  // Turn the child list into an array for sorting.
  nsIFrame** boxes = new nsIFrame*[childCount];
  nsIFrame* box = mFrames.FirstChild();
  nsIFrame** boxPtr = boxes;
  while (box) {
    *boxPtr++ = box;
    box = box->GetNextSibling();
  }

  // sort the array by ordinal group, selection sort
  // XXX this could use a more efficient sort
  PRInt32 i, j, min;
  PRUint32 minOrd, jOrd;
  for(i = 0; i < childCount; i++) {
    min = i;
    minOrd = boxes[min]->GetOrdinal(aState);
    for(j = i + 1; j < childCount; j++) {
      jOrd = boxes[j]->GetOrdinal(aState);
      if (jOrd < minOrd) {
        min = j;
        minOrd = jOrd;
      }
    }
    box = boxes[min];
    boxes[min] = boxes[i];
    boxes[i] = box;
  }

  // turn the array back into linked list, with first and last cached
  mFrames.SetFrames(boxes[0]);
  for (i = 0; i < childCount - 1; ++i)
    boxes[i]->SetNextSibling(boxes[i+1]);

  boxes[childCount-1]->SetNextSibling(nsnull);
  delete [] boxes;
}

NS_IMETHODIMP
nsBoxFrame::SetLayoutManager(nsIBoxLayout* aLayout)
{
  mLayoutManager = aLayout;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetLayoutManager(nsIBoxLayout** aLayout)
{
  *aLayout = mLayoutManager;
  NS_IF_ADDREF(*aLayout);
  return NS_OK;
}

nsresult
nsBoxFrame::LayoutChildAt(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect)
{
  // get the current rect
  nsRect oldRect(aBox->GetRect());
  aBox->SetBounds(aState, aRect);

  PRBool layout = (aBox->GetStateBits() & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0;
  
  if (layout || (oldRect.width != aRect.width || oldRect.height != aRect.height))  {
    return aBox->Layout(aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)
{
  PRUint32 ord = aChild->GetOrdinal(aState);
  
  nsIFrame *child = mFrames.FirstChild();
  nsIFrame *curPrevSib = nsnull, *newPrevSib = nsnull;
  PRBool foundPrevSib = PR_FALSE, foundNewPrevSib = PR_FALSE;

  while (child) {
    if (child == aChild)
      foundPrevSib = PR_TRUE;
    else if (!foundPrevSib)
      curPrevSib = child;

    PRUint32 ordCmp = child->GetOrdinal(aState);
    if (ord < ordCmp)
      foundNewPrevSib = PR_TRUE;
    else if (!foundNewPrevSib && child != aChild)
      newPrevSib = child;

    child->GetNextBox(&child);
  }

  NS_ASSERTION(foundPrevSib, "aChild not in frame list?");

  if (curPrevSib == newPrevSib) {
    // This box is not moving.
    return NS_OK;
  }

  // Take aChild out of its old position in the child list.
  if (curPrevSib)
    curPrevSib->SetNextSibling(aChild->GetNextSibling());
  else
    mFrames.SetFrames(aChild->GetNextSibling());

  nsIBox* newNextSib;
  if (newPrevSib) {
    // insert |aChild| between |newPrevSib| and its next sibling
    newNextSib = newPrevSib->GetNextSibling();
    newPrevSib->SetNextSibling(aChild);
  } else {
    // no |newPrevSib| found, so this box will become |mFirstChild|
    newNextSib = mFrames.FirstChild();
    mFrames.SetFrames(aChild);
  }

  aChild->SetNextSibling(newNextSib);

  return NS_OK;
}

PRBool
nsBoxFrame::GetWasCollapsed(nsBoxLayoutState& aState)
{
  return nsBox::GetWasCollapsed(aState);
}

void
nsBoxFrame::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aWas)
{
  nsBox::SetWasCollapsed(aState, aWas);
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
 * Note that turning a hit on a descendant element into nsnull, so events
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
class nsDisplayXULEventRedirector : public nsDisplayWrapList {
public:
  nsDisplayXULEventRedirector(nsIFrame* aFrame, nsDisplayItem* aItem,
                              nsIFrame* aTargetFrame)
    : nsDisplayWrapList(aFrame, aItem), mTargetFrame(aTargetFrame) {}
  nsDisplayXULEventRedirector(nsIFrame* aFrame, nsDisplayList* aList,
                              nsIFrame* aTargetFrame)
    : nsDisplayWrapList(aFrame, aList), mTargetFrame(aTargetFrame) {}
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt);
  NS_DISPLAY_DECL_NAME("XULEventRedirector")
private:
  nsIFrame* mTargetFrame;
};

nsIFrame* nsDisplayXULEventRedirector::HitTest(nsDisplayListBuilder* aBuilder,
    nsPoint aPt)
{
  nsIFrame* frame = mList.HitTest(aBuilder, aPt);
  if (!frame)
    return nsnull;

  for (nsIContent* content = frame->GetContent();
       content && content != mTargetFrame->GetContent();
       content = content->GetParent()) {
    if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::allowevents,
                             nsGkAtoms::_true, eCaseMatters)) {
      // Events are allowed on 'frame', so let it go.
      return frame;
    }
  }
  // Treat it as a hit on the target frame itself.
  return mTargetFrame;
}

class nsXULEventRedirectorWrapper : public nsDisplayWrapper
{
public:
  nsXULEventRedirectorWrapper(nsIFrame* aTargetFrame)
      : mTargetFrame(aTargetFrame) {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    return new (aBuilder)
        nsDisplayXULEventRedirector(aFrame, aList, mTargetFrame);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    return new (aBuilder)
        nsDisplayXULEventRedirector(aItem->GetUnderlyingFrame(), aItem,
                                    mTargetFrame);
  }
private:
  nsIFrame* mTargetFrame;
};

nsresult
nsBoxFrame::WrapListsInRedirector(nsDisplayListBuilder*   aBuilder,
                                  const nsDisplayListSet& aIn,
                                  const nsDisplayListSet& aOut)
{
  nsXULEventRedirectorWrapper wrapper(this);
  return wrapper.WrapLists(aBuilder, this, aIn, aOut);
}
