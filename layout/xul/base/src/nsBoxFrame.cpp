/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

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
#include "nsGenericHTMLElement.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsISelfScrollingFrame.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsBoxToBlockAdaptor.h"
#include "nsIBoxLayout.h"
#include "nsSprocketLayout.h"

#define CONSTANT 0
//#define DEBUG_REFLOW
//define DEBUG_REDRAW
#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8

//#define TEST_SANITY

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

    nsresult PaintDebug(nsIBox* aBox, 
                        nsIPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nsFramePaintLayer aWhichLayer);

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
    void DrawSpring(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord springSize);
    void DrawLine(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);

    nsBoxFrame::Halignment GetHAlign();
    nsBoxFrame::Valignment GetVAlign();

    nsBoxFrame* mOuter;
    nscoord mInnerSize;

    nsBoxFrame::Valignment mValign;
    nsBoxFrame::Halignment mHalign;
    
    nsSize mPrefSize;
    nsSize mMinSize;
    nsSize mMaxSize;
    nscoord mFlex;
    nscoord mAscent;
    nsIPresContext* mPresContext;

    static PRBool gDebug;
    static nsIBox* mDebugChild;


#ifdef DEBUG_REFLOW
    PRInt32 reflowCount;
#endif

};

PRBool nsBoxFrameInner::gDebug = PR_FALSE;
nsIBox* nsBoxFrameInner::mDebugChild = nsnull;

#ifdef DEBUG_REFLOW

PRInt32 gIndent = 0;
PRInt32 gReflows = 0;

#endif

nsresult
NS_NewBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBoxFrame* it = new (aPresShell) nsBoxFrame(aPresShell, aIsRoot);

  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewBoxFrame

nsBoxFrame::nsBoxFrame(nsIPresShell* aPresShell, PRBool aIsRoot):nsContainerBox(aPresShell)
{
  mInner = new (aPresShell) nsBoxFrameInner(aPresShell, this);

  // if not otherwise specified boxes by default are horizontal.
  mState |= NS_STATE_IS_HORIZONTAL;
  mState |= NS_STATE_AUTO_STRETCH;

  if (aIsRoot) 
     mState |= NS_STATE_IS_ROOT;

  mInner->mValign = nsBoxFrame::vAlign_Top;
  mInner->mHalign = nsBoxFrame::hAlign_Left;
  mInner->mInnerSize = 0;

  NeedsRecalc();

  nsSprocketLayout* layout = new nsSprocketLayout(aPresShell);
  SetLayoutManager(layout);

#ifdef DEBUG_REFLOW
  mInner->reflowCount = 100;
#endif
}

nsBoxFrame::~nsBoxFrame()
{
  NS_ASSERTION(mInner == nsnull,"Error Destroy was never called on this Frame!!!");
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

  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (r == NS_OK) {
    // initialize our list of infos.
    InitChildren(shell, aChildList);
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
  mInner->mPresContext = aPresContext;

  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  
  mInner->mValign = nsBoxFrame::vAlign_Top;
  mInner->mHalign = nsBoxFrame::hAlign_Left;

  GetInitialVAlignment(mInner->mValign);
  GetInitialHAlignment(mInner->mHalign);
  
  PRBool orient = mState & NS_STATE_IS_HORIZONTAL;
  GetInitialOrientation(orient); 
  if (orient)
        mState |= NS_STATE_IS_HORIZONTAL;
    else
        mState &= ~NS_STATE_IS_HORIZONTAL;


  PRBool autostretch = mState & NS_STATE_AUTO_STRETCH;
  GetInitialAutoStretch(autostretch);
  if (autostretch)
        mState |= NS_STATE_AUTO_STRETCH;
     else
        mState &= ~NS_STATE_AUTO_STRETCH;


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

    // if we are root and this
  if (mState & NS_STATE_IS_ROOT) 
      mInner->GetDebugPref(aPresContext);


  mMouseThrough = always;

  if (mContent) {
    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::mousethrough, value)) {
        if (value.EqualsIgnoreCase("never")) 
            mMouseThrough = never;
        else if (value.EqualsIgnoreCase("always")) 
            mMouseThrough = always;
      
    }
  }

  return rv;
}

PRBool
nsBoxFrameInner::GetInitialDebug(PRBool& aDebug)
{
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  mOuter->GetContentOf(getter_AddRefs(content));

  if (!content)
    return PR_FALSE;


  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::debug, value)) {
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

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value)) {
      if (value.EqualsIgnoreCase("left")) {
          aHalign = nsBoxFrame::hAlign_Left;
          return PR_TRUE;
      } else if (value.EqualsIgnoreCase("center")) {
          aHalign = nsBoxFrame::hAlign_Center;
          return PR_TRUE;
      } else if (value.EqualsIgnoreCase("right")) {
          aHalign = nsBoxFrame::hAlign_Right;
          return PR_TRUE;
      }
  }

            // look at vertical alignment
      const nsStyleText* textStyle =
        (const nsStyleText*)mStyleContext->GetStyleData(eStyleStruct_Text);

        switch (textStyle->mTextAlign) 
         {

                case NS_STYLE_TEXT_ALIGN_RIGHT:
                    aHalign = nsBoxFrame::hAlign_Right;
                    return PR_TRUE;
                break;

                case NS_STYLE_TEXT_ALIGN_CENTER:
                    aHalign = nsBoxFrame::hAlign_Center;
                    return PR_TRUE;
                break;

                default:
                    aHalign = nsBoxFrame::hAlign_Left;
                    return PR_TRUE;
                break;
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

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::valign, value)) {
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

  
             // look at vertical alignment
      const nsStyleText* textStyle =
        (const nsStyleText*)mStyleContext->GetStyleData(eStyleStruct_Text);

      if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {

         PRInt32 type = textStyle->mVerticalAlign.GetIntValue();

         switch (type) 
         {
                case NS_STYLE_VERTICAL_ALIGN_BASELINE:
                    aValign = nsBoxFrame::vAlign_BaseLine;
                    return PR_TRUE;
                break;

                case NS_STYLE_VERTICAL_ALIGN_TOP:
                    aValign = nsBoxFrame::vAlign_Top;
                    return PR_TRUE;
                break;

                case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
                    aValign = nsBoxFrame::vAlign_Bottom;
                    return PR_TRUE;
                break;

                case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
                    aValign = nsBoxFrame::vAlign_Middle;
                    return PR_TRUE;
                break;
         }
      }
  

  return PR_FALSE;
}

/* Returns true if it was set.
 */
PRBool
nsBoxFrame::GetInitialOrientation(PRBool& aIsHorizontal)
{
 // see if we are a vertical or horizontal box.
  nsAutoString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(getter_AddRefs(content));

  if (!content)
     return PR_FALSE;

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::orient, value))
  {
      if (value.EqualsIgnoreCase("vertical")) {
         aIsHorizontal = PR_FALSE;
         return PR_TRUE;
      } else if (value.EqualsIgnoreCase("horizontal")) {
         aIsHorizontal = PR_TRUE;
         return PR_TRUE;
      }
  } else {
      // depricated, use align
      if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value)) {
          if (value.EqualsIgnoreCase("vertical")) {
             aIsHorizontal = PR_FALSE;
             return PR_TRUE;
          } else if (value.EqualsIgnoreCase("horizontal")) {
             aIsHorizontal = PR_TRUE;
             return PR_TRUE;
          }
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

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::autostretch, value))
  {
      if (value.EqualsIgnoreCase("never")) {
         aStretch = PR_FALSE;
         return PR_TRUE;
      } else if (value.EqualsIgnoreCase("always")) {
         aStretch = PR_TRUE;
         return PR_TRUE;
      }
  } 

  return PR_FALSE;
}


NS_IMETHODIMP
nsBoxFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
    // if we are not dirty mark ourselves dirty and tell our parent we are dirty too.
    if (!(mState & NS_FRAME_HAS_DIRTY_CHILDREN)) {      
      // Mark yourself as dirty and needing to be recalculated
      mState |= NS_FRAME_HAS_DIRTY_CHILDREN;
      NeedsRecalc();
      return mParent->ReflowDirtyChild(aPresShell, this);
    }

    return NS_OK;
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

NS_IMETHODIMP
nsBoxFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  #ifdef DEBUG_REFLOW
      gIndent++;
  #endif

  NS_ASSERTION(aReflowState.mComputedWidth >=0 && aReflowState.mComputedHeight >= 0, "Computed Size < 0");

  aStatus = NS_FRAME_COMPLETE;

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowState);

  // coelesce reflows if we are root.
  if (state.HandleReflow(this, (mState & NS_STATE_IS_ROOT))) {
     aDesiredSize.width  = 10;
     aDesiredSize.height = 10;
     aDesiredSize.ascent = 0;
     aDesiredSize.descent = 0;
     return NS_OK;
  }

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
     nsSize minSize;
     nsSize maxSize;
     GetPrefSize(state, prefSize);
     GetMinSize(state,  minSize);
     GetMaxSize(state,  maxSize);
     BoundsCheck(minSize, prefSize, maxSize);
  }

  // get our desiredSize
  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE)
     computedSize.width = prefSize.width;
  else
     computedSize.width += m.left + m.right;

  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE || aReflowState.mComputedHeight == 0)
     computedSize.height = prefSize.height;
  else
     computedSize.height += m.top + m.bottom;

  nsRect r(0,0,computedSize.width, computedSize.height);
  r.Inflate(m);
  r.x = mRect.x;
  r.y = mRect.y;

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

  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mInner->mPrefSize)) {
     aSize = mInner->mPrefSize;
     return NS_OK;
  }

  PropagateDebug(aBoxLayoutState);

  nsresult rv = NS_OK;
  rv = nsContainerBox::GetPrefSize(aBoxLayoutState, mInner->mPrefSize);

  aSize = mInner->mPrefSize;
 
  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)
{
  if (!DoesNeedRecalc(mInner->mAscent)) {
     aAscent = mInner->mAscent;
     return NS_OK;
  }

  PropagateDebug(aBoxLayoutState);

  nsresult rv = NS_OK;
  rv = nsContainerBox::GetAscent(aBoxLayoutState, mInner->mAscent);

  aAscent = mInner->mAscent;
 
  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mInner->mMinSize)) {
     aSize = mInner->mMinSize;
     return NS_OK;
  }

  PropagateDebug(aBoxLayoutState);

  nsresult rv = NS_OK;

  mInner->mMinSize.SizeTo(0,0);
  rv = nsContainerBox::GetMinSize(aBoxLayoutState, mInner->mMinSize);
  
  aSize = mInner->mMinSize;

  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mInner->mMaxSize)) {
     aSize = mInner->mMaxSize;
     return NS_OK;
  }

  PropagateDebug(aBoxLayoutState);

  nsresult rv = NS_OK;

  mInner->mMaxSize.SizeTo(0,0);
  nsContainerBox::GetMaxSize(aBoxLayoutState, mInner->mMaxSize);
  
  aSize = mInner->mMaxSize;

  return rv;
}

NS_IMETHODIMP
nsBoxFrame::GetFlex(nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex)
{
  if (!DoesNeedRecalc(mInner->mFlex)) {
     aFlex = mInner->mFlex;
     return NS_OK;
  }

  nsresult rv = NS_OK;

  mInner->mFlex = 0;
  rv = nsContainerBox::GetFlex(aBoxLayoutState, mInner->mFlex);
  aFlex = mInner->mFlex;

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
nsBoxFrame::Layout(nsBoxLayoutState& aState)
{
  PropagateDebug(aState);
  return nsContainerBox::Layout(aState);
}


#ifdef DEBUG_REFLOW
void
nsBoxFrameInner::AddIndents()
{
    for(PRInt32 i=0; i < gIndent; i++)
    {
        printf(" ");
    }
}
#endif

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
      
  // recycle the Inner via the shell's arena.
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  mInner->Recycle(shell);
  mInner = nsnull;

  return nsHTMLContainerFrame::Destroy(aPresContext);
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
  SizeNeedsRecalc(mInner->mPrefSize);
  SizeNeedsRecalc(mInner->mMinSize);
  SizeNeedsRecalc(mInner->mMaxSize);
  CoordNeedsRecalc(mInner->mFlex);
  CoordNeedsRecalc(mInner->mAscent);
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
  Remove(&aPresShell, aOldFrame);

  // remove the child frame
  mFrames.DestroyFrame(aPresContext, aOldFrame);

  SanityCheck(mFrames);

  // mark us dirty and generate a reflow command
  nsBoxLayoutState state(aPresContext);
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
     printf("Warning prev sibling is not in our list!!!");
     aPrevFrame = nsnull;
   }

   // insert the frames to our info list
   Insert(&aPresShell, aPrevFrame, aFrameList);

   // insert the frames in out regular frame list
   mFrames.InsertFrames(this, aPrevFrame, aFrameList);

   nsBoxLayoutState state(aPresContext);

   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFirstChild, PR_TRUE);

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
   Append(&aPresShell,aFrameList);

   // append in regular frames
   mFrames.AppendFrames(this, aFrameList); 

   nsBoxLayoutState state(aPresContext);

   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       SetDebugOnChildList(state, mFirstChild, PR_TRUE);

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
                               PRInt32 aHint)
{
    nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);

    if (aAttribute == nsHTMLAtoms::width ||
        aAttribute == nsHTMLAtoms::height ||
        aAttribute == nsHTMLAtoms::align  ||
        aAttribute == nsHTMLAtoms::valign ||
        aAttribute == nsXULAtoms::flex ||
        aAttribute == nsXULAtoms::orient ||
        aAttribute == nsXULAtoms::autostretch) {

        if (aAttribute == nsXULAtoms::orient || aAttribute == nsXULAtoms::debug || aAttribute == nsHTMLAtoms::align || aAttribute == nsHTMLAtoms::valign) {
          mInner->mValign = nsBoxFrame::vAlign_Top;
          mInner->mHalign = nsBoxFrame::hAlign_Left;

          GetInitialVAlignment(mInner->mValign);
          GetInitialHAlignment(mInner->mHalign);
  
          PRBool orient = mState & NS_STATE_IS_HORIZONTAL;
          GetInitialOrientation(orient); 
          if (orient)
                mState |= NS_STATE_IS_HORIZONTAL;
            else
                mState &= ~NS_STATE_IS_HORIZONTAL;
   
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


NS_IMETHODIMP
nsBoxFrame :: Paint ( nsIPresContext* aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect& aDirtyRect,
                      nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
  mStyleContext->GetStyleData(eStyleStruct_Display);

  // if we aren't visible then we are done.
  if (!disp->IsVisibleOrCollapsed()) 
	   return NS_OK;  

  if (disp->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
   return NS_OK;

  // if we are visible then tell our superclass to paint
  nsresult r = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);

  /*
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
        if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
          mInner->PaintDebug(this, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
        }
  }
  */
 
  return r;
}

void
nsBoxFrameInner::GetDebugPref(nsIPresContext* aPresContext)
{
    gDebug = PR_FALSE;
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_PROGID));
    if (pref) {
	    pref->GetBoolPref("xul.debug.box", &gDebug);
    }
}

// Paint one child frame
void
nsBoxFrame::PaintChild(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer)
{
      const nsStyleDisplay* disp;
      aFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));

      // if collapsed don't paint the child.
      if (disp->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
         return;

      nsHTMLContainerFrame::PaintChild(aPresContext, aRenderingContext, aDirtyRect, aFrame, aWhichLayer);
}

void
nsBoxFrame::PaintChildren(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer)
{
  nsMargin debugBorder;
  nsMargin debugMargin;
  nsMargin debugPadding;
  nsMargin border;
  nscoord onePixel;
  nsRect inner;

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
        GetBorder(border);

        inner.Deflate(border);
        nsRect borderRect(inner);

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
    nsBoxLayoutState state(aPresContext);
    GetInset(im);
    nsMargin border(0,0,0,0);
    const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
    spacing->GetBorderPadding(border);
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

        nscoord x, y, borderSize, springSize;
        
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
            springSize = debugBorder.top - onePixel*4;
        } else {
            cr.x = inner.x;
            x = cr.y;
            y = cr.x + onePixel;
            springSize = debugBorder.left - onePixel*4;
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
        
          mInner->DrawSpring(aPresContext, aRenderingContext, isHorizontal, flex, x, y, borderSize, springSize);
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
NS_INTERFACE_MAP_END_INHERITING(nsHTMLContainerFrame)

NS_IMETHODIMP
nsBoxFrame::GetFrame(nsIFrame** aFrame)
{
  *aFrame = this;  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetFrameName(nsString& aResult) const
{
    nsCOMPtr<nsIContent> content;
    nsBoxFrame* box = (nsBoxFrame*)this;
    box->GetContentOf(getter_AddRefs(content));
    nsAutoString id;

    if (content)
       content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);

    aResult = "Box[id=";
    aResult.Append(id);
    aResult.Append("]");
    return NS_OK;
}

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
  if (!mRect.Contains(aPoint))
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


  if (mMouseThrough == never)
  {
     *aFrame = this;
     return NS_OK;
  }

  // This won't work.
  nsresult rv = GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, PR_FALSE, aFrame);    

  /*
  nsRect r(0,0,mRect.width, mRect.height);

  // if it is not inside us fail
  if (!r.Contains(aPoint)) {
      return NS_ERROR_FAILURE;
  }

  // is it inside our border, padding, and debugborder or insets?
  nsMargin im(0,0,0,0);
  nsBoxLayoutState state(aPresContext);
  GetInset(im);
  nsMargin border(0,0,0,0);
  GetBorderAndPadding(border);
  r.Deflate(im);
  r.Deflate(border);    

  // no? Then it must be in our border so return us.
  if (!r.Contains(aPoint)) {
      *aFrame = this;
      return NS_OK;
  }

  // ok lets look throught the children
  *aFrame = nsnull;
  nsIBox* child = nsnull;
  GetChildBox(&child);
  nsPoint tmp;
  nsIFrame *frame = nsnull, *hit = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
  while(child)
  {
    child->GetFrame(&frame);
    nsresult rv = frame->GetFrameForPoint(aPresContext, tmp, aWhichLayer, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
     *aFrame = hit;
    }

    child->GetNextBox(&child);
  }

  // found it.
  if (hit)
    return NS_OK;

  */

  if (rv != NS_ERROR_FAILURE)
     return rv;

  // see if it is in our border, padding, or inset
  nsRect r(mRect);
  nsMargin m;
  GetInset(m);
  r.Deflate(m);
  GetBorderAndPadding(m);
  r.Deflate(m);
  if (!r.Contains(aPoint)) {
    *aFrame = this;
    return NS_OK;
  }

  if (mMouseThrough == sometimes)
  {
     *aFrame = this;
     return NS_OK;
  }

  const nsStyleColor* color = (const nsStyleColor*)
  mStyleContext->GetStyleData(eStyleStruct_Color);
  PRBool transparentBG = NS_STYLE_BG_COLOR_TRANSPARENT ==
                        (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);

  if (!transparentBG)
  {
     *aFrame = this;
     return NS_OK;
  }

  return NS_ERROR_FAILURE;
}



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

    nsresult rv = nsHTMLContainerFrame::GetCursor(aPresContext, aPoint, aCursor);

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
  mOuter->ClearChildren(aPresShell);

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

        nsRect borderRect(inner);

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

        // paint the springs.
        nscoord x, y, borderSize, springSize;
        
        aRenderingContext.SetColor(NS_RGB(255,255,255));
        
        if (isHorizontal) 
        {
            x = inner.x;
            y = inner.y + onePixel;
            x += debugBorder.left;
            springSize = debugBorder.top - onePixel*4;
        } else {
            x = inner.y;
            y = inner.x + onePixel;
            x += debugBorder.top;
            springSize = debugBorder.left - onePixel*4;
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

                /*
                if (mDebugChild == info->frame) 
                {
                    aRenderingContext.SetColor(NS_RGB(0,255,0));
                    if (mOuter->mInner->mHorizontal) 
                        aRenderingContext.FillRect(x, inner.y, size.width, debugBorder.top);
                    else
                        aRenderingContext.FillRect(inner.x, x, size.height, debugBorder.left);
                    aRenderingContext.SetColor(debugColor->mColor);
                }
                */

                nscoord flex = 0;
                box->GetFlex(state, flex);

                DrawSpring(aPresContext, aRenderingContext, isHorizontal, flex, x, y, borderSize, springSize);
                x += borderSize;
            }
            box->GetNextBox(&box);
        }

        return NS_OK;
}


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
nsBoxFrameInner::DrawSpring(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord springSize)
{    
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

     // if we do draw the coils
        int distance = 0;
        int center = 0;
        int offset = 0;
        int coilSize = COIL_SIZE*onePixel;
        int halfSpring = springSize/2;

        distance = size;
        center = y + halfSpring;
        offset = x;

        int coils = distance/coilSize;

        int halfCoilSize = coilSize/2;

        if (flex == 0) {
            DrawLine(aRenderingContext, aHorizontal, x,y + springSize/2, x + size, y + springSize/2);
        } else {
            for (int i=0; i < coils; i++)
            {
                   DrawLine(aRenderingContext, aHorizontal, offset, center+halfSpring, offset+halfCoilSize, center-halfSpring);
                   DrawLine(aRenderingContext, aHorizontal, offset+halfCoilSize, center-halfSpring, offset+coilSize, center+halfSpring);

                   offset += coilSize;
            }
        }

        FillRect(aRenderingContext, aHorizontal, x + size - springSize/2, y, springSize/2, springSize);
        FillRect(aRenderingContext, aHorizontal, x, y, springSize/2, springSize);

        //DrawKnob(aPresContext, aRenderingContext, x + size - springSize, y, springSize);
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

                // if we are not in the child. But in the spring above the child.
                if ((isHorizontal && x >= r.x && x < r.x + r.width) ||
                    (!isHorizontal && y >= r.y && y < r.y + r.height)) {
                    aCursor = NS_STYLE_CURSOR_POINTER;
                       // found it but we already showed it.
                        if (mDebugChild == child)
                            return NS_OK;


                            nsCOMPtr<nsIContent> content;
                            ourFrame->GetContent(getter_AddRefs(content));

                            nsAutoString id;
                            nsAutoString kClass;
                            nsCOMPtr<nsIAtom> tag;
                            nsAutoString tagString;
                            char tagValue[100];
                            char kClassValue[100];
                            char idValue[100];

                            if (content) {
                              content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);
                              id.ToCString(idValue,100);
                       
                              content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::kClass, kClass);
                              kClass.ToCString(kClassValue,100);

                              content->GetTag(*getter_AddRefs(tag));
                              tag->ToString(tagString);
                              tagString.ToCString(tagValue,100);

                              printf("----- ");
               
      #ifdef NS_DEBUG
                              nsFrame::ListTag(stdout, ourFrame);
      #endif

                              printf(" Tag='%s', id='%s' class='%s'---------------\n", tagValue, idValue, kClassValue);
                        
                            }

                        childFrame->GetContent(getter_AddRefs(content));

                        if (content) {
                            id = "";
                            kClass = "";
                            tagString = "";

                            content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);
                            id.ToCString(idValue,100);
                       
                            content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::kClass, kClass);
                            kClass.ToCString(kClassValue,100);

                            content->GetTag(*getter_AddRefs(tag));
                            tag->ToString(tagString);
                            tagString.ToCString(tagValue,100);

                            printf("child #%d: ", count);
#ifdef NS_DEBUG
                            nsFrame::ListTag(stdout, childFrame);                            
#endif
                            printf(" Tag='%s', id='%s' class='%s'\n", tagValue, idValue, kClassValue);

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


                        nsIBox::AddCSSPrefSize(state, child, prefSizeCSS);
                        nsIBox::AddCSSMinSize (state, child, minSizeCSS);
                        nsIBox::AddCSSMaxSize (state, child, maxSizeCSS);
                        nsIBox::AddCSSFlex    (state, child, flexCSS);

                        child->GetPrefSize(state, prefSize);
                        child->GetMinSize(state, minSize);
                        child->GetMaxSize(state, maxSize);
                        child->GetFlex(state, flexSize);

                        char min[100];
                        char pref[100];
                        char max[100];
                        char calc[100];
                        char flex[100];
                      
                        nsSize actualSize;
                        GetFrameSizeWithMargin(child, actualSize);
                        nsSize actualSizeCSS (NS_INTRINSICSIZE, NS_INTRINSICSIZE);

                        GetValue(aPresContext, minSize,  minSizeCSS, min);
                        GetValue(aPresContext, prefSize, prefSizeCSS, pref);
                        GetValue(aPresContext, maxSize,  maxSizeCSS, max);
                        GetValue(aPresContext, actualSize, actualSizeCSS, calc);
                        GetValue(aPresContext, flexSize,  flexCSS, flex);


                        printf("min%s, pref%s, max%s, actual%s, flex=%s\n\n", 
                            min,
                            pref,
                            max,
                            calc,
                            flex
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