/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIReflowCommand.h"
#include "nsIContent.h"
#include "nsSpaceManager.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsBoxToBlockAdaptor.h"
#include "nsIBoxLayout.h"
#include "nsSprocketLayout.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
#include "nsIScrollableFrame.h"
#include "nsWidgetsCID.h"
#include "nsLayoutAtoms.h"
#include "nsViewsCID.h"
#include "nsIScrollableView.h"
#include "nsHTMLContainerFrame.h"
#include "nsIWidget.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);

//define DEBUG_REDRAW

#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8

//#define TEST_SANITY

#ifdef DEBUG_rods
//#define DO_NOISY_REFLOW
#endif

/**
 * The boxes private implementation
 */
class nsBoxFrameInner
{
public:
  nsBoxFrameInner(nsIPresShell* aPresShell, nsBoxFrame* aThis)
  {
    mOuter = aThis;
  }

  ~nsBoxFrameInner()
  {
  }

  //----- Sibling/Child ----

  nsresult SetDebug(nsIPresContext* aPresContext, PRBool aDebug);

  void Recycle(nsIPresShell* aPresShell);

  // Overloaded new operator. Initializes the memory to 0 and relies on an arena
  // (which comes from the presShell) to perform the allocation.
  void* operator new(size_t sz, nsIPresShell* aPresShell);

  // Overridden to prevent the global delete from being called, since the memory
  // came out of an nsIArena instead of the global delete operator's heap.  
  // XXX Would like to make this private some day, but our UNIX compilers can't 
  // deal with it.
  void operator delete(void* aPtr, size_t sz);

  // helper methods


   void TranslateEventCoords(nsIPresContext* aPresContext,
                                    const nsPoint& aPoint,
                                    nsPoint& aResult);

    static PRBool AdjustTargetToScope(nsIFrame* aParent, nsIFrame*& aTargetFrame);


    PRBool GetInitialDebug(PRBool& aDebug);

    void GetDebugPref(nsIPresContext* aPresContext);

    /*
    nsresult PaintDebug(nsIBox* aBox, 
                        nsIPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nsFramePaintLayer aWhichLayer);
*/

    nsresult DisplayDebugInfoFor(nsIBox* aBox, 
                                 nsIPresContext* aPresContext,
                                 nsPoint&        aPoint,
                                 PRInt32&        aCursor);

    nsresult GetFrameSizeWithMargin(nsIBox* aBox, nsSize& aSize);

    void GetDebugBorder(nsMargin& aInset);
    void GetDebugPadding(nsMargin& aInset);
    void GetDebugMargin(nsMargin& aInset);
    void PixelMarginToTwips(nsIPresContext* aPresContext, nsMargin& aMarginPixels);

    void GetValue(nsIPresContext* aPresContext, const nsSize& a, const nsSize& b, char* value);
    void GetValue(nsIPresContext* aPresContext, PRInt32 a, PRInt32 b, char* value);
    void DrawSpacer(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize);
    void DrawLine(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);

    void CacheAttributes();

    nsIBox* GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor);

    nsBoxFrame::Halignment GetHAlign();
    nsBoxFrame::Valignment GetVAlign();

    // instance variables.
    nsBoxFrame* mOuter;

    nsBoxFrame::Valignment mValign;
    nsBoxFrame::Halignment mHalign;

    PRBool mAttributesCached;
    
    nsIPresContext* mPresContext;

    static PRBool gDebug;
    static nsIBox* mDebugChild;

};

PRBool nsBoxFrameInner::gDebug = PR_FALSE;
nsIBox* nsBoxFrameInner::mDebugChild = nsnull;

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
  mInner = new (aPresShell) nsBoxFrameInner(aPresShell, this);

  mInner->mAttributesCached = PR_FALSE;

  mState |= NS_STATE_IS_HORIZONTAL;
  mState |= NS_STATE_AUTO_STRETCH;

  if (aIsRoot) 
     mState |= NS_STATE_IS_ROOT;

  mInner->mValign = nsBoxFrame::vAlign_Top;
  mInner->mHalign = nsBoxFrame::hAlign_Left;
  
  NeedsRecalc();

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
  NS_ASSERTION(mInner == nsnull,"Error Destroy was never called on this Frame!!!");
}

NS_IMETHODIMP nsBoxFrame::SetParent(const nsIFrame* aParent)
{
  // our box parent can only be a box. Make sure its a box and set it
  // if its not a box then its nsnull

  // cast away const so we can call QueryInterface.
  nsIFrame* parent = (nsIFrame*)aParent;

  // don't use com ptr. Frames don't support ADDREF or RELEASE;
  nsIBox* boxParent = nsnull;

  if (parent)
     parent->QueryInterface(NS_GET_IID(nsIBox), (void**)&boxParent);

  return nsBox::SetParentBox(boxParent);
}


NS_IMETHODIMP
nsBoxFrame::GetVAlign(Valignment& aAlign)
{
   aAlign = mInner->mValign;
   return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetHAlign(Halignment& aAlign)
{
   aAlign = mInner->mHalign;
   return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  SanityCheck(mFrames);

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  nsresult r = nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (r == NS_OK) {
    // initialize our list of infos.
    nsBoxLayoutState state(shell);
    InitChildren(state, aChildList);
    CheckFrameOrder();
  } else {
    printf("Warning add child failed!!\n");
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
nsBoxFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  SetParent(aParent);

  mInner->mPresContext = aPresContext;

  nsresult  rv = nsContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // see if we need a widget. Get our parent. Querty interface the parent we are given. 
  nsCOMPtr<nsIBox> parent (do_QueryInterface(aParent));

  if (parent) {
    PRBool needsWidget = PR_FALSE;
    parent->ChildrenMustHaveWidgets(needsWidget);
    if (needsWidget) {
        nsIView* view = nsnull;
        GetView(aPresContext, &view);

        if (!view) {
           nsHTMLContainerFrame::CreateViewForFrame(aPresContext,this,mStyleContext,nsnull,PR_TRUE); 
           GetView(aPresContext, &view);
        }

        nsIWidget* widget;
        view->GetWidget(widget);

        if (!widget)
           view->CreateWidget(kWidgetCID);   
    }
  }

  mInner->CacheAttributes();

    // if we are root and this
  if (mState & NS_STATE_IS_ROOT) 
      mInner->GetDebugPref(aPresContext);


  mMouseThrough = unset;

  if (mContent) {
    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::mousethrough, value)) {
        if (value.EqualsIgnoreCase("never")) 
            mMouseThrough = never;
        else if (value.EqualsIgnoreCase("always")) 
            mMouseThrough = always;
      
    }
  }

  return rv;
}

void
nsBoxFrameInner::CacheAttributes()
{
  /*
  printf("Caching: ");
  mOuter->DumpBox(stdout);
  printf("\n");
   */

  mValign = nsBoxFrame::vAlign_Top;
  mHalign = nsBoxFrame::hAlign_Left;

  PRBool orient = PR_FALSE;
  mOuter->GetInitialOrientation(orient); 
  if (orient)
    mOuter->mState |= NS_STATE_IS_HORIZONTAL;
  else
    mOuter->mState &= ~NS_STATE_IS_HORIZONTAL;

  PRBool normal = PR_TRUE;
  mOuter->GetInitialDirection(normal); 
  if (normal)
    mOuter->mState |= NS_STATE_IS_DIRECTION_NORMAL;
  else
    mOuter->mState &= ~NS_STATE_IS_DIRECTION_NORMAL;

  mOuter->GetInitialVAlignment(mValign);
  mOuter->GetInitialHAlignment(mHalign);
  
  PRBool equalSize = PR_FALSE;
  mOuter->GetInitialEqualSize(equalSize); 
  if (equalSize)
        mOuter->mState |= NS_STATE_EQUAL_SIZE;
    else
        mOuter->mState &= ~NS_STATE_EQUAL_SIZE;

  PRBool autostretch = mOuter->mState & NS_STATE_AUTO_STRETCH;
  mOuter->GetInitialAutoStretch(autostretch);
  if (autostretch)
        mOuter->mState |= NS_STATE_AUTO_STRETCH;
     else
        mOuter->mState &= ~NS_STATE_AUTO_STRETCH;


  PRBool debug = mOuter->mState & NS_STATE_SET_TO_DEBUG;
  PRBool debugSet = GetInitialDebug(debug); 
  if (debugSet) {
        mOuter->mState |= NS_STATE_DEBUG_WAS_SET;
        if (debug)
            mOuter->mState |= NS_STATE_SET_TO_DEBUG;
        else
            mOuter->mState &= ~NS_STATE_SET_TO_DEBUG;
  } else {
        mOuter->mState &= ~NS_STATE_DEBUG_WAS_SET;
  }
}

PRBool
nsBoxFrameInner::GetInitialDebug(PRBool& aDebug)
{
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  mOuter->GetContentOf(getter_AddRefs(content));

  if (!content)
    return PR_FALSE;


  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::debug, value)) {
      if (value.EqualsIgnoreCase("true")) {
          aDebug = PR_TRUE;
          return PR_TRUE;
      } else if (value.EqualsIgnoreCase("false")) {
          aDebug = PR_FALSE;
          return PR_TRUE;
      }
  }

  return PR_FALSE;
}

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
    if (value.EqualsIgnoreCase("left")) {
        aHalign = nsBoxFrame::hAlign_Left;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("right")) {
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
    if (value.EqualsIgnoreCase("start")) {
        aHalign = nsBoxFrame::hAlign_Left;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("center")) {
        aHalign = nsBoxFrame::hAlign_Center;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("end")) {
        aHalign = nsBoxFrame::hAlign_Right;
        return PR_TRUE;
    }

    // The attr was present but had a nonsensical value. Revert to the default.
    return PR_FALSE;
  }

  // Now that we've checked for the attribute it's time to check CSS.  For 
  // horizontal boxes we're checking PACK.  For vertical boxes we are checking
  // ALIGN.
  const nsStyleXUL* boxInfo = (const nsStyleXUL*)mStyleContext->GetStyleData(eStyleStruct_XUL);
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
    if (value.EqualsIgnoreCase("top")) {
        aValign = nsBoxFrame::vAlign_Top;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("baseline")) {
        aValign = nsBoxFrame::vAlign_BaseLine;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("middle")) {
        aValign = nsBoxFrame::vAlign_Middle;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("bottom")) {
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
    if (value.EqualsIgnoreCase("start")) {
        aValign = nsBoxFrame::vAlign_Top;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("center")) {
        aValign = nsBoxFrame::vAlign_Middle;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("baseline")) {
        aValign = nsBoxFrame::vAlign_BaseLine;
        return PR_TRUE;
    } else if (value.EqualsIgnoreCase("end")) {
        aValign = nsBoxFrame::vAlign_Bottom;
        return PR_TRUE;
    }
    // The attr was present but had a nonsensical value. Revert to the default.
    return PR_FALSE;
  }

  // Now that we've checked for the attribute it's time to check CSS.  For 
  // horizontal boxes we're checking ALIGN.  For vertical boxes we are checking
  // PACK.
  const nsStyleXUL* boxInfo = (const nsStyleXUL*)mStyleContext->GetStyleData(eStyleStruct_XUL);
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
  const nsStyleXUL* boxInfo;
  GetStyleData(eStyleStruct_XUL,
               (const nsStyleStruct*&)boxInfo);
  if (boxInfo->mBoxOrient == NS_STYLE_BOX_ORIENT_HORIZONTAL)
    aIsHorizontal = PR_TRUE;
  else 
    aIsHorizontal = PR_FALSE;

  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::orient, value)) {
    if (value.EqualsIgnoreCase("vertical"))
      aIsHorizontal = PR_FALSE;
    else if (value.EqualsIgnoreCase("horizontal"))
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
    const nsStyleVisibility* vis;
    GetStyleData(eStyleStruct_Visibility,
                 (const nsStyleStruct*&)vis);
    aIsNormal = (vis->mDirection == NS_STYLE_DIRECTION_LTR); // If text runs RTL then so do we.
  }
  else
    aIsNormal = PR_TRUE; // Assume a normal direction in the vertical case.

  // Now check the style system to see if we should invert aIsNormal.
  const nsStyleXUL* boxInfo;
  GetStyleData(eStyleStruct_XUL,
               (const nsStyleStruct*&)boxInfo);
  if (boxInfo->mBoxDirection == NS_STYLE_BOX_DIRECTION_REVERSE)
    aIsNormal = !aIsNormal; // Invert our direction.
  
  // Now see if we have an attribute.  The attribute overrides
  // the style system value.
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::dir, value)) {
    if (value.EqualsIgnoreCase("reverse"))
      aIsNormal = !aIsNormal; // Invert our direction.
    else if (value.EqualsIgnoreCase("ltr"))
      aIsNormal = PR_TRUE;
    else if (value.EqualsIgnoreCase("rtl"))
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
      if (value.EqualsIgnoreCase("always")) {
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

  PRBool autostretchSet = PR_FALSE;
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::autostretch, value))
  {
      if (value.EqualsIgnoreCase("never")) {
         aStretch = PR_FALSE;
         autostretchSet = PR_TRUE;
         return PR_TRUE;
      } else if (value.EqualsIgnoreCase("always")) {
         aStretch = PR_TRUE;
         autostretchSet = PR_TRUE;
         return PR_TRUE;
      }
  }

  if (!autostretchSet) {
    // Check the align attribute.
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::align, value)) {
      aStretch = value.EqualsIgnoreCase("stretch");
      return PR_TRUE;
    }

    // Check the CSS box-align property.
    const nsStyleXUL* boxInfo;
    GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)boxInfo);
    aStretch = (boxInfo->mBoxAlign == NS_STYLE_BOX_ALIGN_STRETCH);
  }

  return PR_TRUE;
}


NS_IMETHODIMP
nsBoxFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
   nsCOMPtr<nsIPresContext> context;
   aPresShell->GetPresContext(getter_AddRefs(context));
   nsBoxLayoutState state(context);
   return RelayoutDirtyChild(state, this);
}

NS_IMETHODIMP
nsBoxFrame::DidReflow(nsIPresContext* aPresContext,
                   nsDidReflowStatus aStatus)
{
  PRBool isDirty = mState & NS_FRAME_IS_DIRTY;
  PRBool hasDirtyChildren = mState & NS_FRAME_HAS_DIRTY_CHILDREN;
  nsresult rv = nsFrame::DidReflow(aPresContext, aStatus);
  if (isDirty)
    mState |= NS_FRAME_IS_DIRTY;

  if (hasDirtyChildren)
    mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

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

NS_IMETHODIMP
nsBoxFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsBoxFrame", aReflowState.reason);

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

  //GetBorderAndPadding(m);

  // this happens sometimes. So lets handle it gracefully.
  if (aReflowState.mComputedHeight == 0) {
    nsSize minSize(0,0);
    GetMinSize(state, minSize);
    computedSize.height = minSize.height - m.top - m.bottom;
  }

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
  if (!(mState & NS_STATE_IS_ROOT)) 
     GetAscent(state, ascent);

  aDesiredSize.width  = r.width;
  aDesiredSize.height = r.height;
  aDesiredSize.ascent = ascent;
  aDesiredSize.descent = 0;

  // max sure the max element size reflects
  // our min width
  nsSize* maxElementSize = nsnull;
  state.GetMaxElementSize(&maxElementSize);
  if (maxElementSize)
  {
     nsSize minSize(0,0);
     GetMinSize(state,  minSize);

     if (mRect.width > minSize.width) {
       if (aReflowState.mComputedWidth == NS_INTRINSICSIZE) {
         maxElementSize->width = minSize.width;
       } else {
         maxElementSize->width = mRect.width;
       }
     } else {
        maxElementSize->width = mRect.width;
     }

     if (mRect.height > minSize.height) {
       if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) {
         maxElementSize->height = minSize.height;
       } else {
         maxElementSize->height = mRect.height;
       }
     } else {
        maxElementSize->height = mRect.height;
     }
  }
#ifdef DO_NOISY_REFLOW
  {
    printf("%p ** nsBF(done) W:%d H:%d  ", this, aDesiredSize.width, aDesiredSize.height);

    if (maxElementSize) {
      printf("MW:%d MH:%d\n", maxElementSize->width, maxElementSize->height); 
    } else {
      printf("MW:? MH:?\n"); 
    }

  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mPrefSize)) {
     aSize = mPrefSize;
     return NS_OK;
  }

  PropagateDebug(aBoxLayoutState);

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

  PropagateDebug(aBoxLayoutState);

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

  PropagateDebug(aBoxLayoutState);

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

  /*
    // @@@ hack to fix bug in xbl where it won't set flex -EDV
  if ((mState & NS_FRAME_FIRST_REFLOW) && !mInner->mAttributesCached) {
    mInner->CacheAttributes();
    mInner->mAttributesCached = PR_TRUE;
  }
  */

  PropagateDebug(aBoxLayoutState);

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
    SetDebug(aState, nsBoxFrameInner::gDebug);
  }
}

NS_IMETHODIMP
nsBoxFrame::BeginLayout(nsBoxLayoutState& aState)
{

  nsresult rv = nsContainerBox::BeginLayout(aState);

  // mark ourselves as dirty so no child under us 
  // can post an incremental layout.
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;
  PropagateDebug(aState);


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

nsBoxFrame::Valignment
nsBoxFrameInner::GetVAlign()
{
   return mValign;
}

nsBoxFrame::Halignment
nsBoxFrameInner::GetHAlign()
{
    return mHalign;
}

NS_IMETHODIMP
nsBoxFrame::Destroy(nsIPresContext* aPresContext)
{
// if we are root remove 1 from the debug count.
  if (mState & NS_STATE_IS_ROOT)
      mInner->GetDebugPref(aPresContext);

  SetLayoutManager(nsnull);

  // recycle the Inner via the shell's arena.
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  mInner->Recycle(shell);
  mInner = nsnull;

  return nsContainerFrame::Destroy(aPresContext);
} 

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

NS_IMETHODIMP
nsBoxFrame::NeedsRecalc()
{
  if (mInner) {
    SizeNeedsRecalc(mPrefSize);
    SizeNeedsRecalc(mMinSize);
    SizeNeedsRecalc(mMaxSize);
    CoordNeedsRecalc(mFlex);
    CoordNeedsRecalc(mAscent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
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
nsBoxFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
   SanityCheck(mFrames);

   nsIBox* prevBox = GetBox(aPrevFrame);
   if (prevBox == nsnull && aPrevFrame != nsnull) {
     printf("Warning prev sibling is not in our list!!!\n");
     aPrevFrame = nsnull;
   }

   // insert the frames to our info list
   nsBoxLayoutState state(aPresContext);
   Insert(state, aPrevFrame, aFrameList);
    
   // insert the frames in out regular frame list
   mFrames.InsertFrames(this, aPrevFrame, aFrameList);

   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFirstChild, PR_TRUE);

   CheckFrameOrder();
   SanityCheck(mFrames);

   // mark us dirty and generate a reflow command
   MarkDirtyChildren(state);
   MarkDirty(state);
   return NS_OK;
}


NS_IMETHODIMP
nsBoxFrame::AppendFrames(nsIPresContext* aPresContext,
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

   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFirstChild, PR_TRUE);

   CheckFrameOrder();
   SanityCheck(mFrames);

   MarkDirtyChildren(state);
   MarkDirty(state);
   return NS_OK;
}



NS_IMETHODIMP
nsBoxFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType, 
                               PRInt32 aHint)
{
    nsresult rv = nsContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aModType, aHint);

    if (aAttribute == nsXULAtoms::ordinal) {
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      nsBoxLayoutState state(shell);
      
      nsIBox* parent;
      GetParentBox(&parent);
      parent->RelayoutChildAtOrdinal(state, this);
      nsIFrame* parentFrame;
      parent->GetFrame(&parentFrame);
      nsBoxFrame* parentBoxFrame = (nsBoxFrame*) parentFrame;
      if (parentBoxFrame)
        parentBoxFrame->CheckFrameOrder();
      parent->MarkDirty(state);
    } else if (aAttribute == nsHTMLAtoms::width ||
        aAttribute == nsHTMLAtoms::height ||
        aAttribute == nsHTMLAtoms::align  ||
        aAttribute == nsHTMLAtoms::valign ||
        aAttribute == nsHTMLAtoms::left ||
        aAttribute == nsHTMLAtoms::top ||
        aAttribute == nsXULAtoms::flex ||
        aAttribute == nsXULAtoms::orient ||
        aAttribute == nsXULAtoms::pack ||
        aAttribute == nsXULAtoms::dir ||
        aAttribute == nsXULAtoms::equalsize ||
        aAttribute == nsXULAtoms::autostretch) {

       if (aAttribute == nsXULAtoms::orient || aAttribute == nsXULAtoms::dir || 
           aAttribute == nsXULAtoms::debug || aAttribute == nsHTMLAtoms::align || 
           aAttribute == nsHTMLAtoms::valign || aAttribute == nsXULAtoms::pack) {
          mInner->mValign = nsBoxFrame::vAlign_Top;
          mInner->mHalign = nsBoxFrame::hAlign_Left;

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

          GetInitialVAlignment(mInner->mValign);
          GetInitialHAlignment(mInner->mHalign);
  
          PRBool equalSize = PR_FALSE;
          GetInitialEqualSize(equalSize); 
          if (equalSize)
                mState |= NS_STATE_EQUAL_SIZE;
            else
                mState &= ~NS_STATE_EQUAL_SIZE;
 
          PRBool debug = mState & NS_STATE_SET_TO_DEBUG;
          PRBool debugSet = mInner->GetInitialDebug(debug); 
          if (debugSet) {
                mState |= NS_STATE_DEBUG_WAS_SET;
                if (debug)
                    mState |= NS_STATE_SET_TO_DEBUG;
                else
                    mState &= ~NS_STATE_SET_TO_DEBUG;
          } else {
                mState &= ~NS_STATE_DEBUG_WAS_SET;
          }


          PRBool autostretch = mState & NS_STATE_AUTO_STRETCH;
          GetInitialAutoStretch(autostretch);
          if (autostretch)
                mState |= NS_STATE_AUTO_STRETCH;
             else
                mState &= ~NS_STATE_AUTO_STRETCH;
        }

        if (aAttribute == nsHTMLAtoms::left || aAttribute == nsHTMLAtoms::top)
          mState &= ~NS_STATE_STACK_NOT_POSITIONED;
      
        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        nsBoxLayoutState state(aPresContext);
        MarkDirty(state);
    }
  
  return rv;
}

NS_IMETHODIMP 
nsBoxFrame::GetInset(nsMargin& margin)
{
  margin.SizeTo(0,0,0,0);

  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
     nsMargin debugMargin(0,0,0,0);
     nsMargin debugBorder(0,0,0,0);
     nsMargin debugPadding(0,0,0,0);
     mInner->GetDebugBorder(debugBorder);
     mInner->PixelMarginToTwips(mInner->mPresContext, debugBorder);
     mInner->GetDebugMargin(debugMargin);
     mInner->PixelMarginToTwips(mInner->mPresContext, debugMargin);
     mInner->GetDebugMargin(debugPadding);
     mInner->PixelMarginToTwips(mInner->mPresContext, debugPadding);
     margin += debugBorder;
     margin += debugMargin;
     margin += debugPadding;
  }

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

void
nsBoxFrameInner::GetDebugPref(nsIPresContext* aPresContext)
{
    gDebug = PR_FALSE;
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
    if (pref) {
	    pref->GetBoolPref("xul.debug.box", &gDebug);
    }
}

#include "nsIMonument.h"

NS_IMETHODIMP
nsBoxFrame::Paint(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsFramePaintLayer    aWhichLayer,
                  PRUint32             aFlags)
{
  const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);

  // if collapsed nothing is drawn
  if (vis->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
    return NS_OK;

  if (NS_FRAME_IS_UNFLOWABLE & mState) {
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> frameType;
  GetFrameType(getter_AddRefs(frameType));

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    if (vis->IsVisible() && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleBackground* color = (const nsStyleBackground*)
        mStyleContext->GetStyleData(eStyleStruct_Background);
      const nsStyleBorder* border = (const nsStyleBorder*)
        mStyleContext->GetStyleData(eStyleStruct_Border);
      const nsStyleOutline* outline = (const nsStyleOutline*)
        mStyleContext->GetStyleData(eStyleStruct_Outline);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *border, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, mStyleContext, skipSides);
      nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, *outline, mStyleContext, 0);
      
      // The sole purpose of this is to trigger display
      //  of the selection window for Named Anchors,
      //  which don't have any children and normally don't
      //  have any size, but in Editor we use CSS to display
      //  an image to represent this "hidden" element.
      if (!mFrames.FirstChild())
      {
        nsFrame::Paint(aPresContext,
                       aRenderingContext, aDirtyRect, aWhichLayer);
      }
    }
  }

  if (frameType.get() == nsLayoutAtoms::rootFrame) {
    // We are wrapping the root frame of a XUL document. We
    // need to check the pres shell to find out if painting is locked
    // down (because we're still in the early stages of document
    // and frame construction.  If painting is locked down, then we
    // do not paint our children.  
    PRBool paintingSuppressed = PR_FALSE;
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    shell->IsPaintingSuppressed(&paintingSuppressed);
    if (paintingSuppressed)
      return NS_OK;
  }

  // Now paint the kids. Note that child elements have the opportunity to
  // override the visibility property and display even if their parent is
  // hidden

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  // see if we have to draw a selection frame around this container
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

/**
 * Redefined to handle collapsed as well as removing unneeded crap having to
 * do with frame state and overlapping that only applied to HTML not XUL
 */
void
nsBoxFrame::PaintChild(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsIFrame*            aFrame,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  const nsStyleVisibility* vis;
  aFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)vis));

  // if collapsed don't paint the child.
  if (vis->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
     return;

  nsIView *pView;
  aFrame->GetView(aPresContext, &pView);
  if (nsnull == pView) {
    nsRect kidRect;
    aFrame->GetRect(kidRect);
 
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
      aRenderingContext.Translate(kidRect.x, kidRect.y);

      // Paint the kid
      aFrame->Paint(aPresContext, aRenderingContext, damageArea, aWhichLayer);
      // don't use PushState and PopState, because they're slow
      aRenderingContext.Translate(-kidRect.x, -kidRect.y);
    }
  }
}

void
nsBoxFrame::PaintChildren(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  nsMargin debugBorder;
  nsMargin debugMargin;
  nsMargin debugPadding;
  nsMargin border;
  nscoord onePixel;
  nsRect inner;

  GetBorder(border);

  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
  {
        PRBool isHorizontal = IsHorizontal();

        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        onePixel = NSIntPixelsToTwips(1, p2t);

        mInner->GetDebugBorder(debugBorder);
        mInner->PixelMarginToTwips(aPresContext, debugBorder);

        mInner->GetDebugMargin(debugMargin);
        mInner->PixelMarginToTwips(aPresContext, debugMargin);

        mInner->GetDebugPadding(debugPadding);
        mInner->PixelMarginToTwips(aPresContext, debugPadding);

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


  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // Child elements have the opportunity to override the visibility property
  // of their parent and display even if the parent is hidden
  PRBool clipState;

  nsRect r(0,0,mRect.width, mRect.height);
  PRBool hasClipped = PR_FALSE;
  
  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
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

    if (!hasClipped && NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
        // if we haven't already clipped and we should
        // check to see if the child is in out bounds. If not then
        // we begin clipping.
        nsRect cr(0,0,0,0);
        kid->GetBounds(cr);
    
        // if our rect does not contain the childs then begin clipping
        if (!r.Contains(cr)) {
            aRenderingContext.PushState();
            aRenderingContext.SetClipRect(r,
                                          nsClipCombine_kIntersect, clipState);
            hasClipped = PR_TRUE;
        }
    }

    PaintChild(aPresContext, aRenderingContext, aDirtyRect, frame, aWhichLayer);

    kid->GetNextBox(&kid);
  }

  if (hasClipped) {
    aRenderingContext.PopState(clipState);
  }

  if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
  {
    GetContentRect(r);

    if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
      mInner->GetDebugMargin(debugMargin);
      mInner->PixelMarginToTwips(aPresContext, debugMargin);
      r.Deflate(debugMargin);
    }

    hasClipped = PR_FALSE;

    GetChildBox(&kid);
    while (nsnull != kid) {
         if (!hasClipped && NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
            // if we haven't already clipped and we should
            // check to see if the child is in out bounds. If not then
            // we begin clipping.
            nsRect cr(0,0,0,0);
            kid->GetBounds(cr);
    
            // if our rect does not contain the childs then begin clipping
            if (!r.Contains(cr)) {
                aRenderingContext.PushState();
                aRenderingContext.SetClipRect(r,
                                              nsClipCombine_kIntersect, clipState);
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
        
          mInner->DrawSpacer(aPresContext, aRenderingContext, isHorizontal, flex, x, y, borderSize, spacerSize);
        }

        kid->GetNextBox(&kid);
    }

    if (hasClipped) {
       aRenderingContext.PopState(clipState);
    }
  }
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

void
nsBoxFrame::GetBoxName(nsAutoString& aName)
{
#ifdef DEBUG
   GetFrameName(aName);
#endif
}

#ifdef DEBUG
NS_IMETHODIMP
nsBoxFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Box", aResult);
}
#endif

NS_IMETHODIMP
nsBoxFrame::GetDebug(PRBool& aDebug)
{
  aDebug = (mState & NS_STATE_CURRENTLY_IN_DEBUG);
  return NS_OK;
}

NS_IMETHODIMP  
nsBoxFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                             const nsPoint& aPoint, 
                             nsFramePaintLayer aWhichLayer,    
                             nsIFrame**     aFrame)
{   

  if ((aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND))
    return NS_ERROR_FAILURE;

  if (!mRect.Contains(aPoint))
    return NS_ERROR_FAILURE;

  const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
  if (vis->mVisible == NS_STYLE_VISIBILITY_COLLAPSE)
    return NS_ERROR_FAILURE;

  nsIView* view = nsnull;
  GetView(aPresContext, &view);

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

  nsIFrame *kid, *hit = nsnull;
  nsPoint tmp;

  FirstChild(aPresContext, nsnull, &kid);
  *aFrame = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
  while (nsnull != kid) {
    // have we hit a child before
    PRBool haveKid = (hit != nsnull);
    nsresult rv = kid->GetFrameForPoint(aPresContext, tmp, aWhichLayer, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      if (!haveKid)
         *aFrame = hit;
      else
      {
        // if the kid had a child before see if this child has mouse
        // though. 
        PRBool isAdaptor = PR_FALSE;
        nsCOMPtr<nsIBox> box = mInner->GetBoxForFrame(hit, isAdaptor);
        if (box) {
          PRBool mouseThrough = PR_FALSE;
          box->GetMouseThrough(mouseThrough);
          // if the child says it can never mouse though ignore it.
          if (!mouseThrough)
              *aFrame = hit;
        }
      }
    }

    kid->GetNextSibling(&kid);
  }

  if (*aFrame) {
    return NS_OK;
  }

  // if no kids were hit then select us
  if (vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsIBox*
nsBoxFrameInner::GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor)
{
  if (aFrame == nsnull)
    return nsnull;

  nsIBox* ibox = nsnull;
  if (NS_FAILED(aFrame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox))) {
    aIsAdaptor = PR_TRUE;

    // if we hit a non box. Find the box in out last container
    // and clear its cache.
    nsIFrame* parent = nsnull;
    aFrame->GetParent(&parent);
    nsIBox* parentBox = nsnull;
    if (NS_FAILED(parent->QueryInterface(NS_GET_IID(nsIBox), (void**)&parentBox))) 
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
   const nsStyleColor* color = (const nsStyleColor*)
   mStyleContext->GetStyleData(eStyleStruct_Color);
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
nsBoxFrame::GetCursor(nsIPresContext* aPresContext,
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
    mInner->TranslateEventCoords(aPresContext, aPoint, newPoint);
    
    // if we are in debug and we are in the debug area
    // return our own cursor and dump the debug information.
    if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
    {
          nsresult rv = mInner->DisplayDebugInfoFor(this, aPresContext, newPoint, aCursor);
          if (rv == NS_OK)
             return rv;
    }

    nsresult rv = nsContainerFrame::GetCursor(aPresContext, aPoint, aCursor);

    return rv;
}


//XXX the event come's in in view relative coords, but really should
//be in frame relative coords by the time it hits our frame.

// Translate an point that is relative to our view (or a containing
// view) into a localized pixel coordinate that is relative to the
// content area of this frame (inside the border+padding).
void
nsBoxFrameInner::TranslateEventCoords(nsIPresContext* aPresContext,
                                      const nsPoint& aPoint,
                                      nsPoint& aResult)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  // If we have a view then the event coordinates are already relative
  // to this frame; otherwise we have to adjust the coordinates
  // appropriately.
  nsIView* view;
  mOuter->GetView(aPresContext, &view);
  if (nsnull == view) {
    nsPoint offset;
    mOuter->GetOffsetFromView(aPresContext, offset, &view);
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

    while(frame != nsnull) {
       
      frame->GetContent(aContent);
        if (*aContent != nsnull)
            return NS_OK;

        frame->GetParent(&frame);
    }

    return NS_OK;
}



void* 
nsBoxFrameInner::operator new(size_t sz, nsIPresShell* aPresShell)
{
    return nsBoxLayoutState::Allocate(sz,aPresShell);
}

void 
nsBoxFrameInner::Recycle(nsIPresShell* aPresShell)
{
  nsBoxLayoutState state(aPresShell);
  mOuter->ClearChildren(state);

  delete this;
  nsBoxLayoutState::RecycleFreedMemory(aPresShell, this);
}


// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsBoxFrameInner::operator delete(void* aPtr, size_t sz)
{
    nsBoxLayoutState::Free(aPtr, sz);
}

/*

nsresult
nsBoxFrameInner::PaintDebug(nsIBox* aBox, 
                        nsIPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nsFramePaintLayer aWhichLayer)

{
    
        PRBool isHorizontal = mOuter->IsHorizontal();

        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

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
        mOuter->IsDirty(dirty);
        PRBool dirtyc = PR_FALSE;
        mOuter->HasDirtyChildren(dirty);

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
nsBoxFrameInner::DrawLine(nsIRenderingContext& aRenderingContext, PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2)
{
    if (aHorizontal)
       aRenderingContext.DrawLine(x1,y1,x2,y2);
    else
       aRenderingContext.DrawLine(y1,x1,y2,x2);
}

void
nsBoxFrameInner::FillRect(nsIRenderingContext& aRenderingContext, PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height)
{
    if (aHorizontal)
       aRenderingContext.FillRect(x,y,width,height);
    else
       aRenderingContext.FillRect(y,x,height,width);
}

void 
nsBoxFrameInner::DrawSpacer(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize)
{    
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

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
nsBoxFrameInner::GetDebugBorder(nsMargin& aInset)
{
    aInset.SizeTo(2,2,2,2);

    if (mOuter->IsHorizontal()) 
       aInset.top = 10;
    else 
       aInset.left = 10;
}

void
nsBoxFrameInner::GetDebugMargin(nsMargin& aInset)
{
    aInset.SizeTo(2,2,2,2);
}

void
nsBoxFrameInner::GetDebugPadding(nsMargin& aPadding)
{
    aPadding.SizeTo(2,2,2,2);
}


void 
nsBoxFrameInner::PixelMarginToTwips(nsIPresContext* aPresContext, nsMargin& aMarginPixels)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  aMarginPixels.left   *= onePixel;
  aMarginPixels.right  *= onePixel;
  aMarginPixels.top    *= onePixel;
  aMarginPixels.bottom *= onePixel;
}


void
nsBoxFrameInner::GetValue(nsIPresContext* aPresContext, const nsSize& a, const nsSize& b, char* ch) 
{
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);

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
nsBoxFrameInner::GetValue(nsIPresContext* aPresContext, PRInt32 a, PRInt32 b, char* ch) 
{
    if (a == NS_INTRINSICSIZE)
      sprintf(ch, "%d[SET]", b);             
    else
      sprintf(ch, "%d", a);             
}

nsresult
nsBoxFrameInner::DisplayDebugInfoFor(nsIBox* aBox,
                                     nsIPresContext* aPresContext,
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

    PRBool isHorizontal = mOuter->IsHorizontal();

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

                            nsCOMPtr<nsIContent> content;
                            ourFrame->GetContent(getter_AddRefs(content));

                            if (content) {                             
                              printf("---------------\n");
                              mOuter->DumpBox(stdout);
                              printf("\n");
                            }

                        childFrame->GetContent(getter_AddRefs(content));

                        if (content) {
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

nsresult
nsBoxFrameInner::GetFrameSizeWithMargin(nsIBox* aBox, nsSize& aSize)
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
nsBoxFrame::CreateViewForFrame(nsIPresContext* aPresContext,
                             nsIFrame* aFrame,
                             nsIStyleContext* aStyleContext,
                             PRBool aForce)
{
  nsIView* view;
  aFrame->GetView(aPresContext, &view);
  // If we don't yet have a view, see if we need a view
  if (nsnull == view) {
    PRInt32 zIndex = 0;
    PRBool  autoZIndex = PR_FALSE;
    PRBool  fixedBackgroundAttachment = PR_FALSE;

    const nsStyleBackground* bg = (const nsStyleBackground*)
      aStyleContext->GetStyleData(eStyleStruct_Background);
    const nsStyleVisibility* vis = (const nsStyleVisibility*)
      aStyleContext->GetStyleData(eStyleStruct_Visibility);

    if (vis->mOpacity != 1.0f) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsBoxFrame::CreateViewForFrame: frame=%p opacity=%g",
         aFrame, vis->mOpacity));
      aForce = PR_TRUE;
    }

    // See if the frame has a fixed background attachment
    if (NS_STYLE_BG_ATTACHMENT_FIXED == bg->mBackgroundAttachment) {
      aForce = PR_TRUE;
      fixedBackgroundAttachment = PR_TRUE;
    }
    
    // See if the frame is a scrolled frame
    if (!aForce) {
      nsIAtom*  pseudoTag;
      aStyleContext->GetPseudoType(pseudoTag);
      if (pseudoTag == nsLayoutAtoms::scrolledContentPseudo) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsBoxFrame::CreateViewForFrame: scrolled frame=%p", aFrame));
        aForce = PR_TRUE;
      }
      NS_IF_RELEASE(pseudoTag);
    }

    if (aForce) {
      // Create a view
      nsIFrame* parent;

      aFrame->GetParentWithView(aPresContext, &parent);
      NS_ASSERTION(parent, "GetParentWithView failed");
      nsIView* parentView;
   
      parent->GetView(aPresContext, &parentView);
      NS_ASSERTION(parentView, "no parent with view");

      // Create a view
      static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

      nsresult result = nsComponentManager::CreateInstance(kViewCID, 
                                                     nsnull, 
                                                     NS_GET_IID(nsIView), 
                                                     (void **)&view);
      if (NS_OK == result) {
        nsIViewManager* viewManager;
        parentView->GetViewManager(viewManager);
        NS_ASSERTION(nsnull != viewManager, "null view manager");

        // Initialize the view
        nsRect bounds;
        aFrame->GetRect(bounds);
        view->Init(viewManager, bounds, parentView);

        // If the frame has a fixed background attachment, then indicate that the
        // view's contents should be repainted and not bitblt'd
        if (fixedBackgroundAttachment) {
          PRUint32  viewFlags;
          view->GetViewFlags(&viewFlags);
          view->SetViewFlags(viewFlags | NS_VIEW_PUBLIC_FLAG_DONT_BITBLT);
        }
        
        // Insert the view into the view hierarchy. If the parent view is a
        // scrolling view we need to do this differently
        nsIScrollableView*  scrollingView;
        if (NS_SUCCEEDED(parentView->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
          scrollingView->SetScrolledView(view);
        } else {
          viewManager->InsertChild(parentView, view, zIndex);

          if (autoZIndex) {
            viewManager->SetViewAutoZIndex(view, PR_TRUE);
          }
        }

        // See if the view should be hidden
        PRBool  viewIsVisible = PR_TRUE;
        PRBool  viewHasTransparentContent = (bg->mBackgroundFlags &
                  NS_STYLE_BG_COLOR_TRANSPARENT) == NS_STYLE_BG_COLOR_TRANSPARENT;

        if (NS_STYLE_VISIBILITY_COLLAPSE == vis->mVisible) {
          viewIsVisible = PR_FALSE;
        }
        else if (NS_STYLE_VISIBILITY_HIDDEN == vis->mVisible) {
          // If it has a widget, hide the view because the widget can't deal with it
          nsIWidget* widget = nsnull;
          view->GetWidget(widget);
          if (widget) {
            viewIsVisible = PR_FALSE;
            NS_RELEASE(widget);
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
            nsCOMPtr<nsIContent> content;
            PRBool      result = PR_FALSE;

            aFrame->GetContent(getter_AddRefs(content));
            if (content) {
              content->CanContainChildren(result);
            }

            if (result) {
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
          view->SetVisibility(nsViewVisibility_kHide);
        }

        viewManager->SetViewOpacity(view, vis->mOpacity);
        NS_RELEASE(viewManager);
      }

      // Remember our view
      aFrame->SetView(aPresContext, view);

      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsBoxFrame::CreateViewForFrame: frame=%p view=%p",
         aFrame));
      return result;
    }
  }
  return NS_OK;
}


