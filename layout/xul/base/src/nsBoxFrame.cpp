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
// this is their size if they were layed out intrinsically. So the box will flow the child to determine this can
// cache the value.

// Boxes and Incremental Reflow
// ----------------------------
// Boxes layout out top down by adding up their childrens min, max, and preferred sizes. Only problem is if a incremental
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
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsSpaceManager.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsIServiceManager.h"
#include "nsBoxToBlockAdaptor.h"
#include "nsIBoxLayout.h"
#include "nsSprocketLayout.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
#include "nsIScrollableFrame.h"
#include "nsWidgetsCID.h"
#include "nsLayoutAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsViewsCID.h"
#include "nsIScrollableView.h"
#include "nsHTMLContainerFrame.h"
#include "nsIWidget.h"
#include "nsIEventStateManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsITheme.h"
#include "nsTransform2D.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsContentUtils.h"

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

nsresult
NS_NewBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBoxFrame* it = new (aPresShell) nsBoxFrame(aPresShell, aIsRoot, aLayoutManager);

  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
  
} // NS_NewBoxFrame

nsBoxFrame::nsBoxFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsContainerBox(aPresShell)
{
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

  NeedsRecalc();
}

nsBoxFrame::~nsBoxFrame()
{
}

NS_IMETHODIMP nsBoxFrame::SetParent(const nsIFrame* aParent)
{
  nsresult rv = nsContainerFrame::SetParent(aParent);

  // our box parent can only be a box. Make sure its a box and set it
  // if its not a box then its nsnull

  // cast away const so we can call QueryInterface.
  nsIFrame* parent = (nsIFrame*)aParent;

  // don't use com ptr. Frames don't support ADDREF or RELEASE;
  nsIBox* boxParent = nsnull;

  if (parent)
     parent->QueryInterface(NS_GET_IID(nsIBox), (void**)&boxParent);

  nsBox::SetParentBox(boxParent);

  return rv;
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
nsBoxFrame::SetInitialChildList(nsPresContext* aPresContext,
                                nsIAtom*        aListName,
                                nsIFrame*       aChildList)
{
  SanityCheck(mFrames);

  nsresult r = nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (r == NS_OK) {
    // initialize our list of infos.
    nsBoxLayoutState state(aPresContext->PresShell());
    InitChildren(state, aChildList);
    CheckFrameOrder();
  } else {
    NS_WARNING("Warning add child failed!!\n");
  }

  SanityCheck(mFrames);

  return r;
}

PRBool 
nsBoxFrame::IsHorizontal() const
{
   return mState & NS_STATE_IS_HORIZONTAL;
}

PRBool 
nsBoxFrame::IsNormalDirection() const
{
   return mState & NS_STATE_IS_DIRECTION_NORMAL;
}

/**
 * Initialize us. This is a good time to get the alignment of the box
 */
NS_IMETHODIMP
nsBoxFrame::Init(nsPresContext*  aPresContext,
                 nsIContent*      aContent,
                 nsIFrame*        aParent,
                 nsStyleContext*  aContext,
                 nsIFrame*        aPrevInFlow)
{
  SetParent(aParent);

  mPresContext = aPresContext;

  nsresult  rv = nsContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // see if we need a widget. Get our parent. Querty interface the parent we are given. 
  nsIBox *parent;
  if (aParent && NS_SUCCEEDED(CallQueryInterface(aParent, &parent))) {
    PRBool needsWidget = PR_FALSE;
    parent->ChildrenMustHaveWidgets(needsWidget);
    if (needsWidget) {
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
      GetDebugPref(aPresContext);
#endif

  mMouseThrough = unset;

  UpdateMouseThrough();

  // register access key
  rv = RegUnregAccessKey(aPresContext, PR_TRUE);

  return rv;
}

void nsBoxFrame::UpdateMouseThrough()
{
  if (mContent) {
    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::mousethrough, value)) {
        if (value.EqualsLiteral("never"))
          mMouseThrough = never;
        else if (value.EqualsLiteral("always"))
          mMouseThrough = always;
      
    }
  }
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
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
    return PR_FALSE;


  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::debug, value)) {
      if (value.EqualsLiteral("true")) {
          aDebug = PR_TRUE;
          return PR_TRUE;
      } else if (value.EqualsLiteral("false")) {
          aDebug = PR_FALSE;
          return PR_TRUE;
      }
  }

  return PR_FALSE;
}
#endif

PRBool
nsBoxFrame::GetInitialHAlignment(nsBoxFrame::Halignment& aHalign)
{
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));
  if (!content)
    return PR_FALSE;

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::align, value)) {
    // XXXdwh Everything inside this if statement is deprecated code.
    if (value.EqualsLiteral("left")) {
        aHalign = nsBoxFrame::hAlign_Left;
        return PR_TRUE;
    } else if (value.EqualsLiteral("right")) {
        aHalign = nsBoxFrame::hAlign_Right;
        return PR_TRUE;
    }
  }
      
  // Now that the deprecated stuff is out of the way, we move on to check the appropriate 
  // attribute.  For horizontal boxes, we are checking the PACK attribute.  For vertical boxes
  // we are checking the ALIGN attribute.
  nsresult res;
  if (IsHorizontal())
    res = content->GetAttr(kNameSpaceID_None, nsXULAtoms::pack, value);
  else res = content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::align, value);
  if (res == NS_CONTENT_ATTR_HAS_VALUE) {
    if (value.EqualsLiteral("start")) {
        aHalign = nsBoxFrame::hAlign_Left;
        return PR_TRUE;
    } else if (value.EqualsLiteral("center")) {
        aHalign = nsBoxFrame::hAlign_Center;
        return PR_TRUE;
    } else if (value.EqualsLiteral("end")) {
        aHalign = nsBoxFrame::hAlign_Right;
        return PR_TRUE;
    }

    // The attr was present but had a nonsensical value. Revert to the default.
    return PR_FALSE;
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

  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));
  if (!content)
    return PR_FALSE;

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::valign, value)) {
    if (value.EqualsLiteral("top")) {
        aValign = nsBoxFrame::vAlign_Top;
        return PR_TRUE;
    } else if (value.EqualsLiteral("baseline")) {
        aValign = nsBoxFrame::vAlign_BaseLine;
        return PR_TRUE;
    } else if (value.EqualsLiteral("middle")) {
        aValign = nsBoxFrame::vAlign_Middle;
        return PR_TRUE;
    } else if (value.EqualsLiteral("bottom")) {
        aValign = nsBoxFrame::vAlign_Bottom;
        return PR_TRUE;
    }
  }

  // Now that the deprecated stuff is out of the way, we move on to check the appropriate 
  // attribute.  For horizontal boxes, we are checking the ALIGN attribute.  For vertical boxes
  // we are checking the PACK attribute.
  nsresult res;
  if (IsHorizontal())
    res = content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::align, value);
  else res = content->GetAttr(kNameSpaceID_None, nsXULAtoms::pack, value);
  if (res == NS_CONTENT_ATTR_HAS_VALUE) {
    if (value.EqualsLiteral("start")) {
        aValign = nsBoxFrame::vAlign_Top;
        return PR_TRUE;
    } else if (value.EqualsLiteral("center")) {
        aValign = nsBoxFrame::vAlign_Middle;
        return PR_TRUE;
    } else if (value.EqualsLiteral("baseline")) {
        aValign = nsBoxFrame::vAlign_BaseLine;
        return PR_TRUE;
    } else if (value.EqualsLiteral("end")) {
        aValign = nsBoxFrame::vAlign_Bottom;
        return PR_TRUE;
    }
    // The attr was present but had a nonsensical value. Revert to the default.
    return PR_FALSE;
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

/* Returns true if it was set.
 */
void
nsBoxFrame::GetInitialOrientation(PRBool& aIsHorizontal)
{
 // see if we are a vertical or horizontal box.
  nsAutoString value;

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
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::orient, value)) {
    if (value.EqualsLiteral("vertical"))
      aIsHorizontal = PR_FALSE;
    else if (value.EqualsLiteral("horizontal"))
     aIsHorizontal = PR_TRUE;
  }
}

void
nsBoxFrame::GetInitialDirection(PRBool& aIsNormal)
{
  nsAutoString value;
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
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::dir, value)) {
    if (value.EqualsLiteral("reverse"))
      aIsNormal = !aIsNormal; // Invert our direction.
    else if (value.EqualsLiteral("ltr"))
      aIsNormal = PR_TRUE;
    else if (value.EqualsLiteral("rtl"))
      aIsNormal = PR_FALSE;
  }
}

/* Returns true if it was set.
 */
PRBool
nsBoxFrame::GetInitialEqualSize(PRBool& aEqualSize)
{
 // see if we are a vertical or horizontal box.
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
     return PR_FALSE;

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::equalsize, value))
  {
      if (value.EqualsLiteral("always")) {
         aEqualSize = PR_TRUE;
         return PR_TRUE;
      }
  } 

  return PR_FALSE;
}

/* Returns true if it was set.
 */
PRBool
nsBoxFrame::GetInitialAutoStretch(PRBool& aStretch)
{
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
     return PR_FALSE;
  
  // Check the align attribute.
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::align, value)) {
    aStretch = value.EqualsLiteral("stretch");
    return PR_TRUE;
  }

  // Check the CSS box-align property.
  const nsStyleXUL* boxInfo = GetStyleXUL();
  aStretch = (boxInfo->mBoxAlign == NS_STYLE_BOX_ALIGN_STRETCH);

  return PR_TRUE;
}


NS_IMETHODIMP
nsBoxFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
   // if we receive a ReflowDirtyChild it is because there is an HTML frame 
   // just inside us. So must find the adaptor that contains the child and
   // tell it that things are dirty.
   nsCOMPtr<nsPresContext> context;
   aPresShell->GetPresContext(getter_AddRefs(context));
   nsBoxLayoutState state(context);

   nsIBox* box = nsnull;
   GetChildBox(&box);
   while (box)
   {
     nsIFrame* frame = nsnull;
     box->GetFrame(&frame);
     if (frame == aChild) {
       box->MarkDirty(state);
       return RelayoutDirtyChild(state, box);
     }

     box->GetNextBox(&box);
   }

   NS_ERROR("Could not find an adaptor!");
   return NS_OK;
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

/**
  * Returns PR_TRUE when the reflow reason is "Initial" and doing Print Preview
  *         when returning PR_FALSE aIsChrome's value is indeterminate
  * aIsChrome - Returns PR_TRUE when document is chrome, otherwise PR_FALSE
  */
PRBool
nsBoxFrame::IsInitialReflowForPrintPreview(nsBoxLayoutState& aState, 
                                           PRBool& aIsChrome)
{
  aIsChrome = PR_FALSE;
  const nsHTMLReflowState* reflowState = aState.GetReflowState();
  if (reflowState->reason == eReflowReason_Initial) {
    // See if we are doing Print Preview
    if (aState.PresContext()->Type() == nsPresContext::eContext_PrintPreview) {
      // Now, get the current URI to see if we doing chrome
      nsIPresShell *presShell = aState.PresShell();
      if (!presShell) return PR_FALSE;
      nsIDocument *doc = presShell->GetDocument();
      if (!doc) return PR_FALSE;
      nsIURI *uri = doc->GetDocumentURI();
      if (!uri) return PR_FALSE;
      uri->SchemeIs("chrome", &aIsChrome);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsBoxFrame::Reflow(nsPresContext*          aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   const nsHTMLReflowState& aReflowState,
                   nsReflowStatus&          aStatus)
{
  // If you make changes to this method, please keep nsLeafBoxFrame::Reflow
  // in sync, if the changes are applicable there.

  DO_GLOBAL_REFLOW_COUNT("nsBoxFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(aReflowState.mComputedWidth >=0 && aReflowState.mComputedHeight >= 0, "Computed Size < 0");

#ifdef DO_NOISY_REFLOW
  printf("\n-------------Starting BoxFrame Reflow ----------------------------\n");
  printf("%p ** nsBF::Reflow %d R: ", this, myCounter++);
  switch (aReflowState.reason) {
    case eReflowReason_Initial:
      printf("Ini");break;
    case eReflowReason_Incremental:
      printf("Inc");break;
    case eReflowReason_Resize:
      printf("Rsz");break;
    case eReflowReason_StyleChange:
      printf("Sty");break;
    case eReflowReason_Dirty:
      printf("Drt ");
      break;
    default:printf("<unknown>%d", aReflowState.reason);break;
  }
  
  printSize("AW", aReflowState.availableWidth);
  printSize("AH", aReflowState.availableHeight);
  printSize("CW", aReflowState.mComputedWidth);
  printSize("CH", aReflowState.mComputedHeight);

  printf(" *\n");

#endif

  aStatus = NS_FRAME_COMPLETE;

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowState, aDesiredSize);

  // coelesce reflows if we are root.
  state.HandleReflow(this);
  
  nsSize computedSize(aReflowState.mComputedWidth,aReflowState.mComputedHeight);

  nsMargin m;
  m = aReflowState.mComputedBorderPadding;
  // GetBorderAndPadding(m);

  nsSize prefSize(0,0);

  // if we are told to layout intrinic then get our preferred size.
  if (computedSize.width == NS_INTRINSICSIZE || computedSize.height == NS_INTRINSICSIZE) {
     nsSize minSize(0,0);
     nsSize maxSize(0,0);
     GetPrefSize(state, prefSize);
     GetMinSize(state,  minSize);
     GetMaxSize(state,  maxSize);
     BoundsCheck(minSize, prefSize, maxSize);
  }

  // get our desiredSize
  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE) {
    computedSize.width = prefSize.width;
  } else {
    computedSize.width += m.left + m.right;
  }

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
  GetBounds(r);
  
  // get the ascent
  nscoord ascent = r.height;

  // getting the ascent could be a lot of work. Don't get it if
  // we are the root. The viewport doesn't care about it.
  if (!(mState & NS_STATE_IS_ROOT)) {
    // Only call GetAscent when not doing Initial reflow while in PP
    // or when it is Initial reflow while in PP and a chrome doc
    // If called again with initial reflow it crashes because the 
    // frames are fully constructed (I think).
    PRBool isChrome;
    PRBool isInitialPP = IsInitialReflowForPrintPreview(state, isChrome);
    if (!isInitialPP || (isInitialPP && isChrome)) {
      GetAscent(state, ascent);
    }
  }

  aDesiredSize.width  = r.width;
  aDesiredSize.height = r.height;
  aDesiredSize.ascent = ascent;
  aDesiredSize.descent = r.height - ascent;

  // NS_FRAME_OUTSIDE_CHILDREN is set in SetBounds() above
  if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
    nsRect* overflowArea = GetOverflowAreaProperty();
    NS_ASSERTION(overflowArea, "Failed to set overflow area property");
    aDesiredSize.mOverflowArea = *overflowArea;
  }

  // max sure the max element size reflects
  // our min width
  nscoord* maxElementWidth = state.GetMaxElementWidth();
  if (maxElementWidth)
  {
     nsSize minSize(0,0);
     GetMinSize(state,  minSize);

     if (mRect.width > minSize.width &&
         aReflowState.mComputedWidth == NS_INTRINSICSIZE)
       *maxElementWidth = minSize.width;
     else
       *maxElementWidth = mRect.width;
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

NS_IMETHODIMP
nsBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mPrefSize)) {
     aSize = mPrefSize;
     return NS_OK;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  nsresult rv = NS_OK;
  rv = nsContainerBox::GetPrefSize(aBoxLayoutState, mPrefSize);

  aSize = mPrefSize;
 
  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)
{
  if (!DoesNeedRecalc(mAscent)) {
     aAscent = mAscent;
     return NS_OK;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  nsresult rv = NS_OK;
  rv = nsContainerBox::GetAscent(aBoxLayoutState, mAscent);

  aAscent = mAscent;
 
  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mMinSize)) {
     aSize = mMinSize;
     return NS_OK;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  nsresult rv = NS_OK;

  mMinSize.SizeTo(0,0);
  rv = nsContainerBox::GetMinSize(aBoxLayoutState, mMinSize);
  
  aSize = mMinSize;

  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mMaxSize)) {
     aSize = mMaxSize;
     return NS_OK;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aBoxLayoutState);
#endif

  nsresult rv = NS_OK;

  mMaxSize.SizeTo(0,0);
  nsContainerBox::GetMaxSize(aBoxLayoutState, mMaxSize);
  
  aSize = mMaxSize;

  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetFlex(nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex)
{
  if (!DoesNeedRecalc(mFlex)) {
     aFlex = mFlex;
     return NS_OK;
  }

  nsresult rv = NS_OK;

  mFlex = 0;
  rv = nsContainerBox::GetFlex(aBoxLayoutState, mFlex);
  aFlex = mFlex;

  return rv;
}

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::PropagateDebug(nsBoxLayoutState& aState)
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

NS_IMETHODIMP
nsBoxFrame::BeginLayout(nsBoxLayoutState& aState)
{

  nsresult rv = nsContainerBox::BeginLayout(aState);

  // mark ourselves as dirty so no child under us 
  // can post an incremental layout.
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  return rv;
}

/**
 * If subclassing please subclass this method not layout. 
 * layout will call this method.
 */
NS_IMETHODIMP
nsBoxFrame::DoLayout(nsBoxLayoutState& aState)
{
  return nsContainerBox::DoLayout(aState);
}

NS_IMETHODIMP
nsBoxFrame::Destroy(nsPresContext* aPresContext)
{
  // unregister access key
  RegUnregAccessKey(aPresContext, PR_FALSE);

  // clean up the container box's layout manager and child boxes
  nsBoxLayoutState state(aPresContext);
  nsContainerBox::Destroy(state);

  return nsContainerFrame::Destroy(aPresContext);
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

     NeedsRecalc();
  }

  return NS_OK;
}
#endif

NS_IMETHODIMP
nsBoxFrame::NeedsRecalc()
{
  SizeNeedsRecalc(mPrefSize);
  SizeNeedsRecalc(mMinSize);
  SizeNeedsRecalc(mMaxSize);
  CoordNeedsRecalc(mFlex);
  CoordNeedsRecalc(mAscent);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::RemoveFrame(nsPresContext* aPresContext,
                        nsIPresShell&   aPresShell,
                        nsIAtom*        aListName,
                        nsIFrame*       aOldFrame)
{
  SanityCheck(mFrames);

  // remove child from our info list
  nsBoxLayoutState state(aPresContext);
  Remove(state, aOldFrame);

  // remove the child frame
  mFrames.DestroyFrame(aPresContext, aOldFrame);

  SanityCheck(mFrames);

  // mark us dirty and generate a reflow command
  MarkDirtyChildren(state);
  MarkDirty(state);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::InsertFrames(nsPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aPrevFrame,
                         nsIFrame*       aFrameList)
{
   SanityCheck(mFrames);

   nsIBox* prevBox = GetBox(aPrevFrame);
   if (prevBox == nsnull && aPrevFrame != nsnull) {
#ifdef DEBUG
     printf("Warning prev sibling is not in our list!!!\n");
#endif
     aPrevFrame = nsnull;
   }

   // insert the frames to our info list
   nsBoxLayoutState state(aPresContext);
   Insert(state, aPrevFrame, aFrameList);
    
   // insert the frames in out regular frame list
   mFrames.InsertFrames(this, aPrevFrame, aFrameList);

#ifdef DEBUG_LAYOUT
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFirstChild, PR_TRUE);
#endif

   CheckFrameOrder();
   SanityCheck(mFrames);

   // mark us dirty and generate a reflow command
   MarkDirtyChildren(state);
   MarkDirty(state);
   return NS_OK;
}


NS_IMETHODIMP
nsBoxFrame::AppendFrames(nsPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aFrameList)
{
   SanityCheck(mFrames);

    // append them after
   nsBoxLayoutState state(aPresContext);
   Append(state,aFrameList);

   // append in regular frames
   mFrames.AppendFrames(this, aFrameList); 

#ifdef DEBUG_LAYOUT
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFirstChild, PR_TRUE);
#endif

   CheckFrameOrder();
   SanityCheck(mFrames);

   MarkDirtyChildren(state);
   MarkDirty(state);
   return NS_OK;
}



NS_IMETHODIMP
nsBoxFrame::AttributeChanged(nsPresContext* aPresContext,
                             nsIContent* aChild,
                             PRInt32 aNameSpaceID,
                             nsIAtom* aAttribute,
                             PRInt32 aModType)
{
  nsresult rv = nsContainerFrame::AttributeChanged(aPresContext, aChild,
                                                   aNameSpaceID, aAttribute,
                                                   aModType);

  // Ignore 'width', 'height', 'screenX', 'screenY' and 'sizemode' on a
  // <window>.
  nsIAtom *tag = mContent->Tag();
  if ((tag == nsXULAtoms::window ||
       tag == nsXULAtoms::page ||
       tag == nsXULAtoms::dialog ||
       tag == nsXULAtoms::wizard) &&
      (nsXULAtoms::width == aAttribute ||
       nsXULAtoms::height == aAttribute ||
       nsXULAtoms::screenX == aAttribute ||
       nsXULAtoms::screenY == aAttribute ||
       nsXULAtoms::sizemode == aAttribute)) {
    return rv;
  }

  if (aAttribute == nsHTMLAtoms::width       ||
      aAttribute == nsHTMLAtoms::height      ||
      aAttribute == nsHTMLAtoms::align       ||
      aAttribute == nsHTMLAtoms::valign      ||
      aAttribute == nsHTMLAtoms::left        ||
      aAttribute == nsHTMLAtoms::top         ||
      aAttribute == nsXULAtoms::flex         ||
      aAttribute == nsXULAtoms::orient       ||
      aAttribute == nsXULAtoms::pack         ||
      aAttribute == nsXULAtoms::dir          ||
      aAttribute == nsXULAtoms::mousethrough ||
      aAttribute == nsXULAtoms::equalsize) {

    if (aAttribute == nsHTMLAtoms::align  ||
        aAttribute == nsHTMLAtoms::valign ||
        aAttribute == nsXULAtoms::orient  ||
        aAttribute == nsXULAtoms::pack    ||
#ifdef DEBUG_LAYOUT
        aAttribute == nsXULAtoms::debug   ||
#endif
        aAttribute == nsXULAtoms::dir) {

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
    else if (aAttribute == nsHTMLAtoms::left ||
             aAttribute == nsHTMLAtoms::top) {
      mState &= ~NS_STATE_STACK_NOT_POSITIONED;
    }
    else if (aAttribute == nsXULAtoms::mousethrough) {
      UpdateMouseThrough();
    }
  }
  else if (aAttribute == nsXULAtoms::ordinal) {
    nsBoxLayoutState state(aPresContext->PresShell());
    
    nsIBox* parent;
    GetParentBox(&parent);
    parent->RelayoutChildAtOrdinal(state, this);
    nsIFrame* parentFrame;
    parent->GetFrame(&parentFrame);
    nsBoxFrame* parentBoxFrame = (nsBoxFrame*) parentFrame;
    if (parentBoxFrame)
      parentBoxFrame->CheckFrameOrder();
    parent->MarkDirty(state);
  }
  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  else if (aAttribute == nsXULAtoms::accesskey) {
    RegUnregAccessKey(aPresContext, PR_TRUE);
  }

  nsBoxLayoutState state(aPresContext);
  // XXX This causes us to reflow for any attribute change (e.g.,
  // flipping through menus).
  MarkDirty(state);

  return rv;
}

NS_IMETHODIMP 
nsBoxFrame::GetInset(nsMargin& margin)
{
  margin.SizeTo(0,0,0,0);

#ifdef DEBUG_LAYOUT
  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
     nsMargin debugMargin(0,0,0,0);
     nsMargin debugBorder(0,0,0,0);
     nsMargin debugPadding(0,0,0,0);
     GetDebugBorder(debugBorder);
     PixelMarginToTwips(mPresContext, debugBorder);
     GetDebugMargin(debugMargin);
     PixelMarginToTwips(mPresContext, debugMargin);
     GetDebugMargin(debugPadding);
     PixelMarginToTwips(mPresContext, debugPadding);
     margin += debugBorder;
     margin += debugMargin;
     margin += debugPadding;
  }
#endif

  return NS_OK;
}

#ifdef DEBUG_COELESCED
static PRInt32 StyleCoelesced = 0;
#endif

PRBool
nsBoxFrame::HasStyleChange()
{
  return mState & NS_STATE_STYLE_CHANGE;
}

void
nsBoxFrame::SetStyleChangeFlag(PRBool aDirty)
{
  nsBox::SetStyleChangeFlag(aDirty);

  if (aDirty)
     mState |= (NS_STATE_STYLE_CHANGE);
  else 
     mState &= ~NS_STATE_STYLE_CHANGE;
}

nsresult
nsBoxFrame::SyncLayout(nsBoxLayoutState& aState)
{
  nsresult rv = nsBox::SyncLayout(aState);
  mState &= ~(NS_STATE_STYLE_CHANGE);
  return rv;
}

void 
nsBoxFrame::CheckFrameOrder()
{
  if (mOrderBoxes) {
    // synchronize the frame order with the box order by simply walking
    // the box list and linking each frame as its box is linked
    nsIBox* box = mFirstChild;
    nsIFrame* frame1;
    box->GetFrame(&frame1);
    
    nsIBox* box2;
    nsIFrame* frame;
    nsIFrame* frame2;
    while (box) {
      box->GetNextBox(&box2);
      box->GetFrame(&frame);
      if (box2)
        box2->GetFrame(&frame2);
      else
        frame2 = nsnull;
      frame->SetNextSibling(frame2);
      box = box2;
    }
    
    mFrames.SetFrames(frame1);
  }
}

#ifdef DEBUG_LAYOUT
void
nsBoxFrame::GetDebugPref(nsPresContext* aPresContext)
{
    gDebug = nsContentUtils::GetBoolPref("xul.debug.box");
}
#endif

NS_IMETHODIMP
nsBoxFrame::Paint(nsPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsFramePaintLayer    aWhichLayer,
                  PRUint32             aFlags)
{
  // if collapsed nothing is drawn
  if (GetStyleVisibility()->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
    return NS_OK;

  if (NS_FRAME_IS_UNFLOWABLE & mState) {
    return NS_OK;
  }

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    PaintSelf(aPresContext, aRenderingContext, aDirtyRect, 0, PR_FALSE);
  }

  if (GetType() == nsLayoutAtoms::rootFrame) {
    // We are wrapping the root frame of a XUL document. We
    // need to check the pres shell to find out if painting is locked
    // down (because we're still in the early stages of document
    // and frame construction.  If painting is locked down, then we
    // do not paint our children.  
    PRBool paintingSuppressed = PR_FALSE;
    aPresContext->PresShell()->IsPaintingSuppressed(&paintingSuppressed);
    if (paintingSuppressed)
      return NS_OK;
  }

  // Now paint the kids. Note that child elements have the opportunity to
  // override the visibility property and display even if their parent is
  // hidden.  Don't paint our children if the theme object is a leaf.
  const nsStyleDisplay* display = GetStyleDisplay();
  if (!(display->mAppearance && nsBox::gTheme && 
        gTheme->ThemeSupportsWidget(aPresContext, this, display->mAppearance) &&
        !gTheme->WidgetIsContainer(display->mAppearance)))
    PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  // see if we have to draw a selection frame around this container
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

/**
 * Redefined to handle collapsed as well as removing unneeded crap having to
 * do with frame state and overlapping that only applied to HTML not XUL
 */
void
nsBoxFrame::PaintChild(nsPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsIFrame*            aFrame,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  // if collapsed don't paint the child.
  if (aFrame->GetStyleVisibility()->mVisible == NS_STYLE_VISIBILITY_COLLAPSE)
     return;

  if (!aFrame->HasView()) {
    nsRect kidRect = aFrame->GetRect();
 
    nsRect damageArea;
    PRBool overlap;
    // Compute the intersection of the dirty rect and the childs
    // rect (both are in our coordinate space). This limits the
    // damageArea to just the portion that intersects the childs
    // rect.
    overlap = damageArea.IntersectRect(aDirtyRect, kidRect); 

    if (overlap) {
      // Translate damage area into the kids coordinate
      // system. Translate rendering context into the kids
      // coordinate system.
      damageArea.x -= kidRect.x;
      damageArea.y -= kidRect.y;

      // Save the transformation matrix's translation components.
      float xMatrix;
      float yMatrix;
      nsTransform2D *theTransform; 
      aRenderingContext.GetCurrentTransform(theTransform);
      NS_ASSERTION(theTransform != nsnull, "The rendering context transform is null");
      theTransform->GetTranslation(&xMatrix, &yMatrix);

      aRenderingContext.Translate(kidRect.x, kidRect.y);
     
      // Paint the kid
      aFrame->Paint(aPresContext, aRenderingContext, damageArea, aWhichLayer);

      // Restore the transformation matrix's translation components.
      theTransform->SetTranslation(xMatrix, yMatrix);
    }
  }
}

void
nsBoxFrame::PaintChildren(nsPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  nsMargin border;
  nsRect inner;

  GetBorder(border);

#ifdef DEBUG_LAYOUT
  nsMargin debugBorder;
  nsMargin debugMargin;
  nsMargin debugPadding;

  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
  {
        PRBool isHorizontal = IsHorizontal();

        GetDebugBorder(debugBorder);
        PixelMarginToTwips(aPresContext, debugBorder);

        GetDebugMargin(debugMargin);
        PixelMarginToTwips(aPresContext, debugMargin);

        GetDebugPadding(debugPadding);
        PixelMarginToTwips(aPresContext, debugPadding);

        GetContentRect(inner);
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
        PRBool dirty = PR_FALSE;
        IsDirty(dirty);
        PRBool dirtyc = PR_FALSE;
        HasDirtyChildren(dirtyc);

        if (dirty || dirtyc) {
           IsDirty(dirty);
           HasDirtyChildren(dirty);

           nsRect dirtyr(inner);
           aRenderingContext.SetColor(NS_RGB(0,255,0));
           aRenderingContext.DrawRect(dirtyr);
           aRenderingContext.SetColor(color);
        }
  }
#endif

  const nsStyleDisplay* disp = GetStyleDisplay();

  // Child elements have the opportunity to override the visibility property
  // of their parent and display even if the parent is hidden

  nsRect r(0,0,mRect.width, mRect.height);
  PRBool hasClipped = PR_FALSE;
  
  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_CLIP == disp->mOverflow) {
    nsMargin im(0,0,0,0);
    GetInset(im);
    r.Deflate(im);
    r.Deflate(border);    
  }

  nsIBox* kid = nsnull;
  GetChildBox(&kid);
  while (nsnull != kid) {
    nsIFrame* frame = nsnull;
    kid->GetFrame(&frame);

    if (!hasClipped && NS_STYLE_OVERFLOW_CLIP == disp->mOverflow) {
        // if we haven't already clipped and we should
        // check to see if the child is in out bounds. If not then
        // we begin clipping.
        nsRect cr(0,0,0,0);
        kid->GetBounds(cr);
    
        // if our rect does not contain the childs then begin clipping
        if (!r.Contains(cr)) {
            aRenderingContext.PushState();
            aRenderingContext.SetClipRect(r, nsClipCombine_kIntersect);
            hasClipped = PR_TRUE;
        }
    }

    PaintChild(aPresContext, aRenderingContext, aDirtyRect, frame, aWhichLayer);

    kid->GetNextBox(&kid);
  }

  if (hasClipped)
    aRenderingContext.PopState();

#ifdef DEBUG_LAYOUT
  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
  {
    nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);
    GetContentRect(r);

    if (NS_STYLE_OVERFLOW_CLIP == disp->mOverflow) {
      GetDebugMargin(debugMargin);
      PixelMarginToTwips(aPresContext, debugMargin);
      r.Deflate(debugMargin);
    }

    hasClipped = PR_FALSE;

    GetChildBox(&kid);
    while (nsnull != kid) {
         if (!hasClipped && NS_STYLE_OVERFLOW_CLIP == disp->mOverflow) {
            // if we haven't already clipped and we should
            // check to see if the child is in out bounds. If not then
            // we begin clipping.
            nsRect cr(0,0,0,0);
            kid->GetBounds(cr);
    
            // if our rect does not contain the childs then begin clipping
            if (!r.Contains(cr)) {
                aRenderingContext.PushState();
                aRenderingContext.SetClipRect(r, nsClipCombine_kIntersect);
                hasClipped = PR_TRUE;
            }
        }
        PRBool isHorizontal = IsHorizontal();

        nscoord x, y, borderSize, spacerSize;
        
        nsRect cr(0,0,0,0);
        kid->GetBounds(cr);
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

        nsBoxLayoutState state(aPresContext);
        nscoord flex = 0;
        kid->GetFlex(state, flex);

        
        PRBool isCollapsed = PR_FALSE;
        kid->IsCollapsed(state, isCollapsed);

        if (!isCollapsed) {
          aRenderingContext.SetColor(NS_RGB(255,255,255));

          if (isHorizontal) 
              borderSize = cr.width;
          else 
              borderSize = cr.height;
        
          DrawSpacer(aPresContext, aRenderingContext, isHorizontal, flex, x, y, borderSize, spacerSize);
        }

        kid->GetNextBox(&kid);
    }

    if (hasClipped)
       aRenderingContext.PopState();
  }
#endif
}

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

NS_INTERFACE_MAP_BEGIN(nsBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsIBox)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBox)
NS_INTERFACE_MAP_END_INHERITING(nsContainerFrame)

NS_IMETHODIMP
nsBoxFrame::GetFrame(nsIFrame** aFrame)
{
  *aFrame = this;  
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
  return nsLayoutAtoms::boxFrame;
}

#ifdef DEBUG_LAYOUT
NS_IMETHODIMP
nsBoxFrame::GetDebug(PRBool& aDebug)
{
  aDebug = (mState & NS_STATE_CURRENTLY_IN_DEBUG);
  return NS_OK;
}
#endif

NS_IMETHODIMP  
nsBoxFrame::GetFrameForPoint(nsPresContext*   aPresContext,
                             const nsPoint&    aPoint, 
                             nsFramePaintLayer aWhichLayer,    
                             nsIFrame**        aFrame)
{   
  if (!mRect.Contains(aPoint))
    return NS_ERROR_FAILURE;

  const nsStyleVisibility* vis = GetStyleVisibility();
  if (vis->mVisible == NS_STYLE_VISIBILITY_COLLAPSE)
    return NS_ERROR_FAILURE;

  nsIView* view = nsnull;
  nsPoint originOffset;
  GetOriginToViewOffset(aPresContext, originOffset, &view);

#ifdef DEBUG_LAYOUT
  // get the debug frame.
  if (view || (mState & NS_STATE_IS_ROOT))
  {
    nsIBox* box = nsnull;
    if (NS_SUCCEEDED(GetDebugBoxAt(aPoint, &box)) && box)
    {
      PRBool isDebug = PR_FALSE;
      box->GetDebug(isDebug);
      if (isDebug) {
        nsIFrame* frame = nsnull;
        box->GetFrame(&frame);
        *aFrame = frame;
        return NS_OK;
      }
    }
  }
#endif

  nsIFrame *hit = nsnull;
  nsPoint tmp;

  *aFrame = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);

  if (view)
    tmp += originOffset;

  nsIBox* kid = nsnull;
  GetChildBox(&kid);
  while (nsnull != kid) {
    nsIFrame* frame = nsnull;
    kid->GetFrame(&frame);
    GetFrameForPointChild(aPresContext, tmp, aWhichLayer, frame, hit != nsnull, &hit);
    kid->GetNextBox(&kid);
  }
  if (hit)
    *aFrame = hit;

  if (*aFrame) {
    return NS_OK;
  }

  // if no kids were hit then select us
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND && vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/* virtual */ nsresult
nsBoxFrame::GetFrameForPointChild(nsPresContext*   aPresContext,
                                  const nsPoint&    aPoint,
                                  nsFramePaintLayer aWhichLayer,    
                                  nsIFrame*         aChild,
                                  PRBool            aCheckMouseThrough,
                                  nsIFrame**        aFrame)
{
  nsIFrame *hit = nsnull;
  nsresult rv =
    aChild->GetFrameForPoint(aPresContext, aPoint, aWhichLayer, &hit);

  if (NS_SUCCEEDED(rv) && hit) {
    rv = NS_ERROR_FAILURE;
    if (!aCheckMouseThrough) {
      *aFrame = hit;
      rv = NS_OK;
    }
    else
    {
      // If we had a lower frame for this point, check whether hit's box has
      // mouse through.  If so, stick with the lower frame that we found.
      PRBool isAdaptor = PR_FALSE;
      nsIBox *box = GetBoxForFrame(hit, isAdaptor);
      if (box) {
        PRBool mouseThrough = PR_FALSE;
        box->GetMouseThrough(mouseThrough);
        // if the child says it can never mouse though ignore it. 
        if (!mouseThrough) {
          *aFrame = hit;
          rv = NS_OK;
        }
      }
    }
  }
  return rv;
}

nsIBox*
nsBoxFrame::GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor)
{
  if (aFrame == nsnull)
    return nsnull;

  nsIBox* ibox = nsnull;
  if (NS_FAILED(aFrame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox))) {
    aIsAdaptor = PR_TRUE;

    // if we hit a non box. Find the box in out last container
    // and clear its cache.
    nsIBox* parentBox = nsnull;
    if (NS_FAILED(aFrame->GetParent()->
                  QueryInterface(NS_GET_IID(nsIBox), (void**)&parentBox)))
       return nsnull;

    if (parentBox) {
      nsIBox* start = nsnull;
      parentBox->GetChildBox(&start);
      while (start) {
        nsIFrame* frame = nsnull;
        start->GetFrame(&frame);
        if (frame == aFrame) {
          ibox = start;
          break;
        }

        start->GetNextBox(&start);
      }
    }
  } 

  return ibox;
}

/*
NS_IMETHODIMP
nsBoxFrame::GetMouseThrough(PRBool& aMouseThrough)
{
   const nsStyleBackground* color = GetStyleBackground();
   PRBool transparentBG = NS_STYLE_BG_COLOR_TRANSPARENT ==
                         (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);

   if (!transparentBG)
      aMouseThrough = never;
   else
      return nsBox::GetMouseThrough(aMouseThrough);

   return NS_OK;
}
*/




NS_IMETHODIMP
nsBoxFrame::GetCursor(nsPresContext* aPresContext,
                      nsPoint&        aPoint,
                      PRInt32&        aCursor)
{
  /*
    #ifdef NS_DEBUG
    printf("Get Cursor: ");
                            nsFrame::ListTag(stdout, this);
    printf("\n");
                            
    #endif
 */

    nsPoint newPoint;
    TranslateEventCoords(aPresContext, aPoint, newPoint);
    
#ifdef DEBUG_LAYOUT
    // if we are in debug and we are in the debug area
    // return our own cursor and dump the debug information.
    if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
    {
          nsresult rv = DisplayDebugInfoFor(this, aPresContext, newPoint, aCursor);
          if (rv == NS_OK)
             return rv;
    }
#endif

    nsresult rv = nsContainerFrame::GetCursor(aPresContext, aPoint, aCursor);

    return rv;
}


//XXX the event come's in in view relative coords, but really should
//be in frame relative coords by the time it hits our frame.

// Translate an point that is relative to our view (or a containing
// view) into a localized pixel coordinate that is relative to the
// content area of this frame (inside the border+padding).
void
nsBoxFrame::TranslateEventCoords(nsPresContext* aPresContext,
                                 const nsPoint& aPoint,
                                 nsPoint& aResult)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  // If we have a view then the event coordinates are already relative
  // to this frame; otherwise we have to adjust the coordinates
  // appropriately.
  if (!HasView()) {
    nsPoint offset;
    nsIView* view;
    GetOffsetFromView(aPresContext, offset, &view);
    if (nsnull != view) {
      x -= offset.x;
      y -= offset.y;
    }
  }

  aResult.x = x;
  aResult.y = y;
 
}


nsresult 
nsBoxFrame::GetContentOf(nsIContent** aContent)
{
    // If we don't have a content node find a parent that does.
    nsIFrame* frame;
    GetFrame(&frame);

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

/*
nsresult
nsBoxFrame::PaintDebug(nsIBox*              aBox, 
                       nsPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer)

{
    
        PRBool isHorizontal = IsHorizontal();

        nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);

        nsMargin debugBorder(0,0,0,0);
        nsMargin debugMargin(0,0,0,0);
        nsMargin debugPadding(0,0,0,0);

        GetDebugBorder(debugBorder);
        PixelMarginToTwips(aPresContext, debugBorder);

        GetDebugMargin(debugMargin);
        PixelMarginToTwips(aPresContext, debugMargin);

        GetDebugPadding(debugPadding);
        PixelMarginToTwips(aPresContext, debugPadding);

        nsRect inner(0,0,0,0);
        aBox->GetContentRect(inner);

        inner.Deflate(debugMargin);

        //nsRect borderRect(inner);

        nscolor color;
        if (isHorizontal) {
          color = NS_RGB(0,0,255);
        } else {
          color = NS_RGB(255,0,0);
        }
        
        //left
        aRenderingContext.SetColor(color);
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
        PRBool dirty = PR_FALSE;
        IsDirty(dirty);
        PRBool dirtyc = PR_FALSE;
        HasDirtyChildren(dirty);

        if (dirty || dirtyc) {
           nsRect dirtyr(inner);
           aRenderingContext.SetColor(NS_RGB(0,255,0));
           aRenderingContext.DrawRect(dirtyr);
        }

        // paint the spacers.
        nscoord x, y, borderSize, spacerSize;
        
        aRenderingContext.SetColor(NS_RGB(255,255,255));
        
        if (isHorizontal) 
        {
            x = inner.x;
            y = inner.y + onePixel;
            x += debugBorder.left;
            spacerSize = debugBorder.top - onePixel*4;
        } else {
            x = inner.y;
            y = inner.x + onePixel;
            x += debugBorder.top;
            spacerSize = debugBorder.left - onePixel*4;
        }

        nsIBox* box = nsnull;
        aBox->GetChildBox(&box);
        nsBoxLayoutState state(aPresContext);

        while (box) {
          nsSize size;
          GetFrameSizeWithMargin(box, size);
          PRBool isCollapsed = PR_FALSE;
          box->IsCollapsed(state, isCollapsed);

          if (!isCollapsed) {
                if (isHorizontal) 
                    borderSize = size.width;
                else 
                    borderSize = size.height;

             

                nscoord flex = 0;
                box->GetFlex(state, flex);

                DrawSpacer(aPresContext, aRenderingContext, isHorizontal, flex, x, y, borderSize, spacerSize);
                x += borderSize;
            }
            box->GetNextBox(&box);
        }

        return NS_OK;
}
*/

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

#ifdef DEBUG_LAYOUT
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
#endif

void 
nsBoxFrame::PixelMarginToTwips(nsPresContext* aPresContext, nsMargin& aMarginPixels)
{
  nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);
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
nsBoxFrame::DisplayDebugInfoFor(nsIBox*         aBox,
                                nsPresContext* aPresContext,
                                nsPoint&        aPoint,
                                PRInt32&        aCursor)
{
    nsBoxLayoutState state(aPresContext);

    nscoord x = aPoint.x;
    nscoord y = aPoint.y;

    nsIFrame* ourFrame = nsnull;
    aBox->GetFrame(&ourFrame);

    // get the area inside our border but not our debug margins.
    nsRect insideBorder;
    aBox->GetContentRect(insideBorder);
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
               nsRect r;
               child->GetBounds(r);
               nsIFrame* childFrame = nsnull;
               child->GetFrame(&childFrame);

                // if we are not in the child. But in the spacer above the child.
                if ((isHorizontal && x >= r.x && x < r.x + r.width) ||
                    (!isHorizontal && y >= r.y && y < r.y + r.height)) {
                    aCursor = NS_STYLE_CURSOR_POINTER;
                       // found it but we already showed it.
                        if (mDebugChild == child)
                            return NS_OK;

                        if (ourFrame->GetContent()) {
                          printf("---------------\n");
                          DumpBox(stdout);
                          printf("\n");
                        }

                        if (childFrame->GetContent()) {
                            printf("child #%d: ", count);
                            child->DumpBox(stdout);
                            printf("\n");
                        }

                        mDebugChild = child;

                        nsSize prefSizeCSS(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                        nsSize minSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                        nsSize maxSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                        nscoord flexCSS = NS_INTRINSICSIZE;

                        nsSize prefSize(0, 0);
                        nsSize minSize (0, 0);
                        nsSize maxSize (NS_INTRINSICSIZE, NS_INTRINSICSIZE);
                        nscoord flexSize = 0;
                        nscoord ascentSize = 0;


                        nsIBox::AddCSSPrefSize(state, child, prefSizeCSS);
                        nsIBox::AddCSSMinSize (state, child, minSizeCSS);
                        nsIBox::AddCSSMaxSize (state, child, maxSizeCSS);
                        nsIBox::AddCSSFlex    (state, child, flexCSS);

                        child->GetPrefSize(state, prefSize);
                        child->GetMinSize(state, minSize);
                        child->GetMaxSize(state, maxSize);
                        child->GetFlex(state, flexSize);
                        child->GetAscent(state, ascentSize);

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
#endif

nsresult
nsBoxFrame::GetFrameSizeWithMargin(nsIBox* aBox, nsSize& aSize)
{
  nsRect rect(0,0,0,0);
  aBox->GetBounds(rect);
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
    if (hasBG && NS_STYLE_BG_ATTACHMENT_FIXED == bg->mBackgroundAttachment) {
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

      // Create a view
      static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
      nsIView *view;
      nsresult result = CallCreateInstance(kViewCID, &view);
      if (NS_SUCCEEDED(result)) {
        nsIViewManager* viewManager = parentView->GetViewManager();
        NS_ASSERTION(nsnull != viewManager, "null view manager");

        // Initialize the view
        view->Init(viewManager, aFrame->GetRect(), parentView);

        // If the frame has a fixed background attachment, then indicate that the
        // view's contents should be repainted and not bitblt'd
        if (fixedBackgroundAttachment) {
          viewManager->SetViewBitBltEnabled(view, PR_FALSE);
        }
        
        // Insert the view into the view hierarchy. If the parent view is a
        // scrolling view we need to do this differently
        nsIScrollableView*  scrollingView;
        if (NS_SUCCEEDED(CallQueryInterface(parentView, &scrollingView))) {
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

            if (content && content->IsContentOfType(nsIContent::eELEMENT)) {
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
      return result;
    }
  }
  return NS_OK;
}

// If you make changes to this function, check its counterparts
// in nsTextBoxFrame and nsAreaFrame
nsresult
nsBoxFrame::RegUnregAccessKey(nsPresContext* aPresContext, PRBool aDoReg)
{
  // if we have no content, we can't do anything
  if (!mContent)
    return NS_ERROR_FAILURE;

  // find out what type of element this is
  nsIAtom *atom = mContent->Tag();

  // only support accesskeys for the following elements
  if (atom != nsXULAtoms::button &&
      atom != nsXULAtoms::toolbarbutton &&
      atom != nsXULAtoms::checkbox &&
      atom != nsXULAtoms::textbox &&
      atom != nsXULAtoms::tab &&
      atom != nsXULAtoms::radio) {
    return NS_OK;
  }

  nsAutoString accessKey;
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::accesskey, accessKey);

  if (accessKey.IsEmpty())
    return NS_OK;

  // With a valid PresContext we can get the ESM 
  // and register the access key
  nsIEventStateManager *esm = aPresContext->EventStateManager();

  nsresult rv;

  PRUint32 key = accessKey.First();
  if (aDoReg)
    rv = esm->RegisterAccessKey(mContent, key);
  else
    rv = esm->UnregisterAccessKey(mContent, key);

  return rv;
}


void
nsBoxFrame::FireDOMEvent(nsPresContext *aPresContext, const nsAString& aDOMEventName)
{
  if (mContent) {
    // Fire a DOM event for the title change.
    nsCOMPtr<nsIDOMEvent> event;
    nsCOMPtr<nsIEventListenerManager> manager;
    mContent->GetListenerManager(getter_AddRefs(manager));
    if (manager &&
        NS_SUCCEEDED(manager->CreateEvent(aPresContext, nsnull, NS_LITERAL_STRING("Events"), getter_AddRefs(event)))) {
      event->InitEvent(aDOMEventName, PR_TRUE, PR_TRUE);
      PRBool noDefault;
      aPresContext->EventStateManager()->DispatchNewEvent(mContent, event,
                                                          &noDefault);
    }
  }
}

