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
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

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
#include "nsIPresShell.h"
#include "nsGenericHTMLElement.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"

#define CONSTANT 0
#define DEBUG_REFLOW 0
#define DEBUG_REDRAW 0
#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8

#define TEST_SANITY

/**
 * Only created when the box is in debug mode
 */
class nsBoxDebugInner
{
public:

  nsBoxDebugInner(nsBoxFrame* aThis)
  {
    mOuter = aThis;
  }
  virtual ~nsBoxDebugInner() {}

public:
    nsCOMPtr<nsIStyleContext> mHorizontalDebugStyle;
    nsCOMPtr<nsIStyleContext> mVerticalDebugStyle;
    nsBoxFrame* mOuter;

    float mP2t;
    static nsIFrame* mDebugChild;
    void GetValue(const nsSize& a, const nsSize& b, char* value);
    void GetValue(PRInt32 a, PRInt32 b, char* value);
    void DrawSpring( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord springSize);
    void PaintSprings(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, const nsRect& aDirtyRect);
    void DrawLine(nsIRenderingContext& aRenderingContext, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(nsIRenderingContext& aRenderingContext, nscoord x, nscoord y, nscoord width, nscoord height);

    nsresult DisplayDebugInfoFor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor);

};

class nsInfoListImpl: public nsInfoList
{
public:
    nsInfoListImpl();
    virtual ~nsInfoListImpl();
    virtual nsCalculatedBoxInfo* GetFirst();
    virtual nsCalculatedBoxInfo* GetLast();
    virtual PRInt32 GetCount();

    void Clear();
    PRInt32 CreateInfosFor(nsIFrame* aList, nsCalculatedBoxInfo*& first, nsCalculatedBoxInfo*& last);
    void RemoveAfter(nsCalculatedBoxInfo* aPrev);
    void Remove(nsIFrame* aChild);
    void Prepend(nsIFrame* aList);
    void Append(nsIFrame* aList);
    void Insert(nsIFrame* aPrevFrame, nsIFrame* aList);
    void InsertAfter(nsCalculatedBoxInfo* aPrev, nsIFrame* aList);
    void Init(nsIFrame* aList);
    nsCalculatedBoxInfo* GetPrevious(nsIFrame* aChild);
    nsCalculatedBoxInfo* GetInfo(nsIFrame* aChild);
    void SanityCheck(nsFrameList& aFrameList);


    nsCalculatedBoxInfo* mFirst;
    nsCalculatedBoxInfo* mLast;
    PRInt32 mCount;
};

class nsCalculatedBoxInfoImpl: public nsCalculatedBoxInfo
{
public:
    nsCalculatedBoxInfoImpl(nsIFrame* aFrame);
    virtual ~nsCalculatedBoxInfoImpl();
    nsCalculatedBoxInfoImpl(const nsBoxInfo& aInfo);
    virtual void Clear();
};

/**
 * The boxes private implementation
 */
class nsBoxFrameInner
{
public:
  nsBoxFrameInner(nsBoxFrame* aThis)
  {
    mOuter = aThis;
    mDebugInner = nsnull;
    mInfoList = new nsInfoListImpl();

  }

  ~nsBoxFrameInner()
  {
      delete mDebugInner;
      delete mInfoList;
  }

  void GetDebugInset(nsMargin& inset);
  void AdjustChildren(nsIPresContext* aPresContext, nsBoxFrame* aBox);
  void UpdatePseudoElements(nsIPresContext* aPresContext);
  nsresult GetContentOf(nsIFrame* aFrame, nsIContent** aContent);
  void SanityCheck();

    nsCOMPtr<nsISpaceManager> mSpaceManager; // We own this [OWNER].
    PRUint32 mFlags;
    nsBoxFrame* mOuter;
    nsBoxDebugInner* mDebugInner;
    nsInfoListImpl* mInfoList;
    PRBool mHorizontal;

};

nsIFrame* nsBoxDebugInner::mDebugChild = nsnull;

nsresult
NS_NewBoxFrame ( nsIFrame** aNewFrame, PRUint32 aFlags )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBoxFrame* it = new nsBoxFrame(aFlags);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewBoxFrame

nsBoxFrame::nsBoxFrame(PRUint32 aFlags)
{
  mInner = new nsBoxFrameInner(this);

  // if not otherwise specified boxes by default are horizontal.
  mInner->mHorizontal = PR_TRUE;
  mInner->mFlags = aFlags;
}

nsBoxFrame::~nsBoxFrame()
{
  delete mInner;
}

NS_IMETHODIMP
nsBoxFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  mInner->SanityCheck();

  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  
  // initialize our list of infos.
  mInner->mInfoList->Init(aChildList);

  mInner->SanityCheck();

  return r;
}


PRBool nsBoxFrame::IsHorizontal() const
{
   return mInner->mHorizontal;
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
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

 

  nsSpaceManager* spaceManager = new nsSpaceManager(this);
  mInner->mSpaceManager = spaceManager;
  mInner->UpdatePseudoElements(aPresContext);
  mInner->mHorizontal = GetInitialAlignment();
  return rv;
}

/* Returns true if the box is horizontal and false if the box is vertical
 */
PRBool
nsBoxFrame::GetInitialAlignment()
{
 // see if we are a vertical or horizontal box.
  nsString value;

  nsCOMPtr<nsIContent> content;
  mInner->GetContentOf(this, getter_AddRefs(content));

  content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);
  if (value.EqualsIgnoreCase("vertical"))
    return PR_FALSE;
  else 
    return PR_TRUE;
}

nsInfoList* 
nsBoxFrame::GetInfoList()
{
   return mInner->mInfoList;
}




void
nsBoxFrame::GetInnerRect(nsRect& aInner)
{
    const nsStyleSpacing* spacing;

    GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    nsMargin border(0,0,0,0);
    spacing->GetBorderPadding(border);

    aInner = mRect;
    aInner.x = 0;
    aInner.y = 0;
    aInner.Deflate(border);

    border.SizeTo(0,0,0,0);
    mInner->GetDebugInset(border);
    aInner.Deflate(border);
}

/** 
 * Looks at the given frame and sees if its redefined preferred, min, or max sizes
 * if so it used those instead. Currently it gets its values from css
 */
void 
nsBoxFrame::GetRedefinedMinPrefMax(nsIPresContext*  aPresContext, nsIFrame* aFrame, nsCalculatedBoxInfo& aSize)
{
  // add in the css min, max, pref
    const nsStylePosition* position;
    aFrame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.prefSize.width = position->mWidth.GetCoordValue();
        aSize.prefWidthIntrinsic = PR_FALSE;
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.prefSize.height = position->mHeight.GetCoordValue();     
        aSize.prefHeightIntrinsic = PR_FALSE;
    }
    
    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min != 0)
           aSize.minSize.width = min;
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min != 0)
           aSize.minSize.height = min;
    }

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.maxSize.width = max;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.maxSize.height = max;
    }

    // get the flexibility
    nsCOMPtr<nsIContent> content;
    mInner->GetContentOf(aFrame, getter_AddRefs(content));

    PRInt32 error;
    nsAutoString value;

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
    {
        value.Trim("%");
        aSize.flex = value.ToInteger(&error);
    }

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, value))
    {
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);

        value.Trim("%");

        aSize.prefSize.width = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
    }

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::height, value))
    {
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);

        value.Trim("%");

        aSize.prefSize.height = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
    }
}

/**
 * Given a frame gets its box info. If it does not have a box info then it will merely
 * get the normally defined min, pref, max stuff.
 *
 */
nsresult
nsBoxFrame::GetChildBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame* aFrame, nsCalculatedBoxInfo& aSize)
{
  aSize.Clear();

  // see if the frame Innerements IBox interface
  
  // since frames are not refCounted, don't use nsCOMPtr with them
  //nsCOMPtr<nsIBox> ibox = do_QueryInterface(aFrame);

  // if it does ask it for its BoxSize and we are done
  nsIBox* ibox;
  if (NS_SUCCEEDED(aFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox) {
     ibox->GetBoxInfo(aPresContext, aReflowState, aSize); 
     // add in the border, padding, width, min, max
     GetRedefinedMinPrefMax(aPresContext, aFrame, aSize);
     return NS_OK;
  }   

 // start the preferred size as intrinsic
  aSize.prefSize.width = NS_INTRINSICSIZE;
  aSize.prefSize.height = NS_INTRINSICSIZE;

  // redefine anything depending on css
  GetRedefinedMinPrefMax(aPresContext, aFrame, aSize);

  // if we are still intrinsically sized the flow to get the size otherwise
  // we are done.
  if (aSize.prefSize.width == NS_INTRINSICSIZE || aSize.prefSize.height == NS_INTRINSICSIZE)
  {  
        // subtract out the childs margin and border 
        const nsStyleSpacing* spacing;
        nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&) spacing);

        NS_ASSERTION(rv == NS_OK,"failed to get spacing");
        if (NS_FAILED(rv))
             return rv;

        nsMargin margin(0,0,0,0);;
        spacing->GetMargin(margin);
        nsMargin border(0,0,0,0);
        spacing->GetBorderPadding(border);
        nsMargin total = margin + border;

        // add in childs margin and border
        if (aSize.prefSize.width != NS_INTRINSICSIZE)
            aSize.prefSize.width += (total.left + total.right);

        if (aSize.prefSize.height != NS_INTRINSICSIZE)
            aSize.prefSize.height += (total.top + total.bottom);

        // flow child at preferred size
        nsHTMLReflowMetrics desiredSize(nsnull);

        aSize.calculatedSize = aSize.prefSize;

        nsReflowStatus status;
        PRBool redraw;
        nsString reason("To get pref size");
        FlowChildAt(aFrame, aPresContext, desiredSize, aReflowState, status, aSize, redraw, reason);

        // remove margin and border
        desiredSize.height -= (total.top + total.bottom);
        desiredSize.width -= (total.left + total.right);

        // get the size returned and the it as the preferredsize.
        aSize.prefSize.width = desiredSize.width;
        aSize.prefSize.height = desiredSize.height;
    

    // redefine anything depending on css
    GetRedefinedMinPrefMax(aPresContext, aFrame, aSize);
  }

  return NS_OK;
}

/**
 * Ok what we want to do here is get all the children, figure out
 * their flexibility, preferred, min, max sizes and then stretch or
 * shrink them to fit in the given space.
 *
 * So we will have 3 passes. 
 * 1) get our min,max,preferred size.
 * 2) flow all our children to fit into the size we are given layout in
 * 3) move all the children to the right locations.
 */
NS_IMETHODIMP
nsBoxFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{

  mInner->SanityCheck();

  // If we have a space manager, then set it in the reflow state
  if (mInner->mSpaceManager) {
    // Modify the reflow state and set the space manager
    nsHTMLReflowState&  reflowState = (nsHTMLReflowState&)aReflowState;
    reflowState.mSpaceManager = mInner->mSpaceManager;

    // Clear the spacemanager's regions.
    mInner->mSpaceManager->ClearRegions();
  }

  //--------------------------------------------------------------------
  //-------------- figure out the rect we need to fit into -------------
  //--------------------------------------------------------------------

  // this is the size of our box. Remember to subtract our our border. The size we are given
  // does not include it. So we have to adjust our rect accordingly.

  nscoord x = aReflowState.mComputedBorderPadding.left;
  nscoord y = aReflowState.mComputedBorderPadding.top;

  nsRect rect(x,y,aReflowState.mComputedWidth,aReflowState.mComputedHeight);
 
  //---------------------------------------------------------
  //------- handle incremental reflow --------------------
  //---------------------------------------------------------

  // if there is incremental we need to tell all the boxes below to blow away the
  // cached values for the children in the reflow list
  nsIFrame* incrementalChild = nsnull;
  if ( aReflowState.reason == eReflowReason_Incremental ) {
    nsIFrame* targetFrame;    
    // See if it's targeted at us
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      // if it has redraw us
      Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);
    } else {
      // otherwise dirty our children
      Dirty(aPresContext, aReflowState,incrementalChild);
    }
  } 
#if 0
ListTag(stdout);
printf(": begin reflow reason=%s", 
       aReflowState.reason == eReflowReason_Incremental ? "incremental" : "other");
if (incrementalChild) { printf(" frame="); nsFrame::ListTag(stdout, incrementalChild); }
printf("\n");
#endif

  //------------------------------------------------------------------------------------------------
  //------- Figure out what our box size is. This will calculate our children's sizes as well ------
  //------------------------------------------------------------------------------------------------

  // get our size. This returns a boxSize that contains our min, max, pref sizes. It also
  // calculates all of our children sizes as well. It does not include our border we will have to include that 
  // later
  nsBoxInfo ourSize;
  GetBoxInfo(aPresContext, aReflowState, ourSize);

  //------------------------------------------------------------------------------------------------
  //------- Make sure the space we need to layout into adhears to our min, max, pref sizes    ------
  //------------------------------------------------------------------------------------------------

  BoundsCheck(ourSize, rect);
 
  // subtract out the insets. Insets are so subclasses like toolbars can wedge controls in and around the 
  // box. GetBoxInfo automatically adds them in. But we want to know the size we need to layout our children 
  // in so lets subtract them our for now.
  nsMargin inset(0,0,0,0);
  GetInset(inset);

  nsMargin debugInset(0,0,0,0);
  mInner->GetDebugInset(debugInset);
  inset += debugInset;

  rect.Deflate(inset);

  //-----------------------------------------------------------------------------------
  //------------------------- figure our our children's sizes  -------------------------
  //-----------------------------------------------------------------------------------

  // now that we know our child's min, max, pref sizes. Stretch our children out to fit into our size.
  // this will calculate each of our childs sizes.
  InvalidateChildren();
  LayoutChildrenInRect(rect);

  //-----------------------------------------------------------------------------------
  //------------------------- flow all the children -----------------------------------
  //-----------------------------------------------------------------------------------

  // flow each child at the new sizes we have calculated.
  FlowChildren(aPresContext, aDesiredSize, aReflowState, aStatus, rect);

  //-----------------------------------------------------------------------------------
  //------------------------- Adjust each childs x, y location-------------------------
  //-----------------------------------------------------------------------------------
   // set the x,y locations of each of our children. Taking into acount their margins, our border,
  // and insets.
  PlaceChildren(aPresContext,rect);

  //-----------------------------------------------------------------------------------
  //------------------------- Add our border and insets in ----------------------------
  //-----------------------------------------------------------------------------------

  rect.Inflate(inset);

  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE && rect.width < aReflowState.mComputedWidth)
    rect.width = aReflowState.mComputedWidth;
 
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE && rect.height < aReflowState.mComputedHeight)
    rect.height = aReflowState.mComputedHeight;
 
  rect.Inflate(aReflowState.mComputedBorderPadding);

  // the rect might have gotten bigger so recalc ourSize
  aDesiredSize.width = rect.width;
  aDesiredSize.height = rect.height;

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
 
  aStatus = NS_FRAME_COMPLETE;
  
  nsRect damageArea(0,0,0,0);
  damageArea.y = 0;
  damageArea.height = aDesiredSize.height;
  damageArea.width = aDesiredSize.width;

#if 0
ListTag(stdout); printf(": reflow done\n");
#endif

  mInner->SanityCheck();

  return NS_OK;
}


/**
 * When all the childrens positions have been calculated and layed out. Flow each child
 * at its not size.
 */
nsresult
nsBoxFrame::FlowChildren(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsRect& rect)
{
  PRBool redraw = PR_FALSE;

  //-----------------------------------
  // first pass flow all fixed children
  //-----------------------------------

  PRBool finished;
  nscoord passes = 0;
  nscoord changedIndex = -1;
  //nscoord count = 0;
  nsAutoString reason("initial");
  nsAutoString nextReason("initial");
  PRInt32 infoCount = mInner->mInfoList->GetCount();
  PRBool* resized = new PRBool[infoCount];

  for (int i=0; i < infoCount; i++) 
      resized[i] = PR_FALSE;
 
  // ----------------------
  // Flow all children 
  // ----------------------

  // ok what we want to do if flow each child at the location given in the spring.
  // unfortunately after flowing a child it might get bigger. We have not control over this
  // so it the child gets bigger or smaller than we expected we will have to do a 2nd, 3rd, 4th pass to 
  // adjust.

  changedIndex = -1;
  InvalidateChildren();
  LayoutChildrenInRect(rect);
 
  passes = 0;
  do 
  {
    finished = PR_TRUE;
    nscoord count = 0;
    nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
    while (nsnull != info) 
    {    
        nsIFrame* childFrame = info->frame;

        NS_ASSERTION(info, "ERROR not info object!!");

        // if we reached the index that changed we are done.
        if (count == changedIndex)
            break;

        if (!info->collapsed)
        {
        // reflow if the child needs it or we are on a second pass
          FlowChildAt(childFrame, aPresContext, aDesiredSize, aReflowState, aStatus, *info, redraw, reason);
 
          // if the child got bigger then adjust our rect and all the children.
          ChildResized(childFrame, aDesiredSize, rect, *info, resized, changedIndex, finished, count, nextReason);
        }
      
        
      info = info->next;
      count++;
      reason = nextReason;
    }

    // if we get over 10 passes something probably when wrong.
    passes++;
    if (passes > 5) {
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("bug"));
    }

    //NS_ASSERTION(passes <= 10,"Error infinte loop too many passes");
    if (passes > 10) {
       break;
    }


  } while (PR_FALSE == finished);

  // redraw things if needed.
  if (redraw) {
#if DEBUG_REDRAW
      ListTag(stdout);
      printf("is being redrawn\n");
#endif
    Invalidate(aPresContext, nsRect(0,0,mRect.width, mRect.height), PR_FALSE);
  }

  delete[] resized;

  return NS_OK;
}

void
nsBoxFrame::ChildResized(nsIFrame* aFrame, nsHTMLReflowMetrics& aDesiredSize, nsRect& aRect, nsCalculatedBoxInfo& aInfo, PRBool* aResized, nscoord& aChangedIndex, PRBool& aFinished, nscoord aIndex, nsString& aReason)
{
  if (mInner->mHorizontal) {
      // if we are a horizontal box see if the child will fit inside us.
      if ( aDesiredSize.height > aRect.height) {
            // if we are a horizontal box and the the child it bigger than our height

            // ok if the height changed then we need to reflow everyone but us at the new height
            // so we will set the changed index to be us. And signal that we need a new pass.

            aRect.height = aDesiredSize.height;

            // remember we do not need to clear the resized list because changing the height of a horizontal box
            // will not affect the width of any of its children because block flow left to right, top to bottom. Just trust me
            // on this one.
            aFinished = PR_FALSE;
            aChangedIndex = aIndex;

            // relayout everything
            InvalidateChildren();
            LayoutChildrenInRect(aRect);
            aReason = "child's height got bigger";
      } else if (aDesiredSize.width > aInfo.calculatedSize.width) {
            // if the child is wider than we anticipated. This can happend for children that we were not able to get a
            // take on their min width. Like text, or tables.

            // because things flow from left to right top to bottom we know that
            // if we get wider that we can set the min size. This will only work
            // for width not height. Height must always be recalculated!

            // however we must see whether the min size was set in css.
            // if it was then this size should not override it.

            // add in the css min, max, pref
            const nsStylePosition* position;
            aFrame->GetStyleData(eStyleStruct_Position,
                          (const nsStyleStruct*&) position);
    

            // same for min size. Unfortunately min size is always set to 0. So for now
            // we will assume 0 means not set.
            if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
                nscoord min = position->mMinWidth.GetCoordValue();
                if (min != 0)
                   return;
            }

            aInfo.minSize.width = aDesiredSize.width;

            // our width now becomes the new size
            aInfo.calculatedSize.width = aDesiredSize.width;

            InvalidateChildren();

            // our index resized
            aResized[aIndex] = PR_TRUE;

            // if the width changed. mark our child as being resized
            nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
            PRInt32 infoCount = 0;
            while(info) {
              info->sizeValid = aResized[infoCount];
              info = info->next;
              infoCount++;
            }

            LayoutChildrenInRect(aRect);
            aFinished = PR_FALSE;
            aChangedIndex = aIndex;
            aReason = "child's width got bigger";
      }
  } else {
     if ( aDesiredSize.width > aRect.width) {
            // ok if the height changed then we need to reflow everyone but us at the new height
            // so we will set the changed index to be us. And signal that we need a new pass.
            aRect.width = aDesiredSize.width;

            // add in the css min, max, pref
            const nsStylePosition* position;
            aFrame->GetStyleData(eStyleStruct_Position,
                          (const nsStyleStruct*&) position);
    
            // same for min size. Unfortunately min size is always set to 0. So for now
            // we will assume 0 means not set.
            if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
                nscoord min = position->mMinWidth.GetCoordValue();
                if (min != 0)
                   return;
            }

            // because things flow from left to right top to bottom we know that
            // if we get wider that we can set the min size. This will only work
            // for width not height. Height must always be recalculated!
            aInfo.minSize.width = aDesiredSize.width;

            // if the width changed then clear out the resized list
            // but only do this if we are vertical box. On a horizontal box increasing the height will not change the
            // width of its children.
            PRInt32 infoCount = mInner->mInfoList->GetCount();
            for (int i=0; i < infoCount; i++)
               aResized[i] = PR_FALSE;

            aFinished = PR_FALSE;
            aChangedIndex = aIndex;

            // relayout everything
            InvalidateChildren();
            LayoutChildrenInRect(aRect);
            aReason = "child's height got bigger";
      } else if (aDesiredSize.height > aInfo.calculatedSize.height) {
            // our width now becomes the new size
            aInfo.calculatedSize.height = aDesiredSize.height;

            InvalidateChildren();

            // our index resized
            aResized[aIndex] = PR_TRUE;

            // if the width changed. mark our child as being resized
            nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
            PRInt32 infoCount = 0;
            while(info) {
              info->sizeValid = aResized[infoCount];
              info = info->next;
              infoCount++;
            }

            LayoutChildrenInRect(aRect);
            aFinished = PR_FALSE;
            aChangedIndex = aIndex;
            aReason = "child's width got bigger";
      }
  }
}

void 
nsBoxFrame::CollapseChild(nsIPresContext* aPresContext, nsIFrame* frame, PRBool hide)
{
    // shrink the view
      nsIView* view = nsnull;
      frame->GetView(aPresContext, &view);

      // if we find a view stop right here. All views under it
      // will be clipped.
      if (view) {
         nsViewVisibility v;
         view->GetVisibility(v);
         nsCOMPtr<nsIWidget> widget;
         view->GetWidget(*getter_AddRefs(widget));
         if (hide) {
             view->SetVisibility(nsViewVisibility_kHide);
         } else {
             view->SetVisibility(nsViewVisibility_kShow);
         }
         if (widget) {

           return;
         }
      }
    
      // collapse the child
      nsIFrame* child = nsnull;
      frame->FirstChild(nsnull, &child);

      while (nsnull != child) 
      {
         CollapseChild(aPresContext, child, hide);
         nsresult rv = child->GetNextSibling(&child);
         NS_ASSERTION(rv == NS_OK,"failed to get next child");
      }
}

NS_IMETHODIMP
nsBoxFrame::DidReflow(nsIPresContext* aPresContext,
                      nsDidReflowStatus aStatus)
{
  nsresult rv = nsHTMLContainerFrame::DidReflow(aPresContext, aStatus);
  NS_ASSERTION(rv == NS_OK,"DidReflow failed");

  nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

  while (info) 
  {
    // make collapsed children not show up
    if (info->collapsed) {
        CollapseChild(aPresContext, info->frame, PR_TRUE);
    } 

    info = info->next;
  }

  return rv;
}

/**
 * Given the boxes rect. Set the x,y locations of all its children. Taking into account
 * their margins
 */
nsresult
nsBoxFrame::PlaceChildren(nsIPresContext* aPresContext, nsRect& boxRect)
{
  // ------- set the childs positions ---------
  nscoord x = boxRect.x;
  nscoord y = boxRect.y;

  nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

  while (info) 
  {
    nsIFrame* childFrame = info->frame;

    nsresult rv;

    // make collapsed children not show up
    if (info->collapsed) {
       nsRect rect(0,0,0,0);
       childFrame->GetRect(rect);
       if (rect.width > 0 || rect.height > 0) {
          childFrame->SizeTo(aPresContext, 0,0);
       }
    } else {
      const nsStyleSpacing* spacing;
      rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) spacing);

      nsMargin margin(0,0,0,0);
      spacing->GetMargin(margin);

      if (mInner->mHorizontal) {
        x += margin.left;
        y = boxRect.y + margin.top;
      } else {
        y += margin.top;
        x = boxRect.x + margin.left;
      }

      nsRect rect;
      nsIView*  view;
      childFrame->GetRect(rect);
      rect.x = x;
      rect.y = y;
      childFrame->SetRect(aPresContext, rect);
      childFrame->GetView(aPresContext, &view);
      // XXX Because we didn't position the frame or its view when reflowing
      // it we must re-position all child views. This isn't optimal, and if
      // we knew its final position, it would be better to position the frame
      // and its view when doing the reflow...
      if (view) {
        nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, childFrame,
                                                   view, nsnull);
      } else {
        // Re-position any child frame views
        nsContainerFrame::PositionChildViews(aPresContext, childFrame);
      }

      // add in the right margin
      if (mInner->mHorizontal)
        x += margin.right;
      else
        y += margin.bottom;
     
      if (mInner->mHorizontal) {
        x += rect.width;
        //width += rect.width + margin.left + margin.right;
      } else {
        y += rect.height;
        //height += rect.height + margin.top + margin.bottom;
      }
    }

    info = info->next;
  }

  return NS_OK;
}


/**
 * Flow an individual child. Special args:
 * count: the spring that will be used to lay out the child
 * incrementalChild: If incremental reflow this is the child that need to be reflowed.
 *                   when we finally do reflow the child we will set the child to null
 */

nsresult
nsBoxFrame::FlowChildAt(nsIFrame* childFrame, 
                     nsIPresContext* aPresContext,
                     nsHTMLReflowMetrics&     desiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsCalculatedBoxInfo&     aInfo,
                     PRBool& aRedraw,
                     nsString& aReason)
{
      aStatus = NS_FRAME_COMPLETE;
      nsReflowReason reason = aReflowState.reason;
      PRBool shouldReflow = PR_TRUE;

      // if the reason is incremental and the child is not marked as incremental. Then relow the child
      // as a resize instead.
      if (aInfo.isIncremental)
          reason = eReflowReason_Incremental;
      else if (reason == eReflowReason_Incremental)
          reason = eReflowReason_Resize;
      
      // subtract out the childs margin and border 
      const nsStyleSpacing* spacing;
      nsresult rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) spacing);

      nsMargin margin(0,0,0,0);
      spacing->GetMargin(margin);

      // get the current size of the child
      nsRect currentRect(0,0,0,0);
      childFrame->GetRect(currentRect);

      // if we don't need a reflow then 
      // lets see if we are already that size. Yes? then don't even reflow. We are done.
      if (!aInfo.needsReflow && aInfo.calculatedSize.width != NS_INTRINSICSIZE && aInfo.calculatedSize.height != NS_INTRINSICSIZE) {

          // if the new calculated size has a 0 width or a 0 height
          if ((currentRect.width == 0 || currentRect.height == 0) && (aInfo.calculatedSize.width == 0 || aInfo.calculatedSize.height == 0)) {
               shouldReflow = PR_FALSE;
               desiredSize.width = aInfo.calculatedSize.width - (margin.left + margin.right);
               desiredSize.height = aInfo.calculatedSize.height - (margin.top + margin.bottom);
               childFrame->SizeTo(aPresContext, desiredSize.width, desiredSize.height);
          } else {
            desiredSize.width = currentRect.width;
            desiredSize.height = currentRect.height;

            // remove the margin. The rect of our child does not include it but our calculated size does.
            nscoord calcWidth = aInfo.calculatedSize.width - (margin.left + margin.right);
            nscoord calcHeight = aInfo.calculatedSize.height - (margin.top + margin.bottom);

            // don't reflow if we are already the right size
            if (currentRect.width == calcWidth && currentRect.height == calcHeight)
                  shouldReflow = PR_FALSE;
          }
      }      

      // ok now reflow the child into the springs calculated space
      if (shouldReflow) {

        nsMargin border(0,0,0,0);
        spacing->GetBorderPadding(border);

        const nsStylePosition* position;
        rv = childFrame->GetStyleData(eStyleStruct_Position,
                       (const nsStyleStruct*&) position);

        desiredSize.width = 0;
        desiredSize.height = 0;

        nsSize size(aInfo.calculatedSize.width, aInfo.calculatedSize.height);

        /*
        // lets also look at our intrinsic flag. This flag is to make things like HR work.
        // hr is funny if you flow it intrinsically you will get a size that is the height of
        // the current font size. But if you then flow the hr with a computed height of what was returned the
        // hr will be stretched out to fit. So basically the hr lays itself out differently depending 
        // on if you use intrinsic or or computed size. So to fix this we follow this policy. If any child
        // does not Innerement nsIBox then we set this flag. Then on a flow if we decide to flow at the preferred width
        // we flow it with a intrinsic width. This goes for height as well.
        if (aInfo.prefWidthIntrinsic && size.width == aInfo.prefSize.width) 
           size.width = NS_INTRINSICSIZE;

        if (aInfo.prefHeightIntrinsic && size.height == aInfo.prefSize.height) 
           size.height = NS_INTRINSICSIZE;
        */

        // only subrtact margin
        if (size.height != NS_INTRINSICSIZE)
            size.height -= (margin.top + margin.bottom);

        if (size.width != NS_INTRINSICSIZE)
            size.width -= (margin.left + margin.right);

        // create a reflow state to tell our child to flow at the given size.
        nsHTMLReflowState   reflowState(aPresContext, aReflowState, childFrame, nsSize(size.width, NS_INTRINSICSIZE));
        reflowState.reason = reason;

        if (size.height != NS_INTRINSICSIZE)
            size.height -= (border.top + border.bottom);

        if (size.width != NS_INTRINSICSIZE)
            size.width -= (border.left + border.right);

        reflowState.mComputedWidth = size.width;
        reflowState.mComputedHeight = size.height;

    //    nsSize maxElementSize(0, 0);
      //  desiredSize.maxElementSize = &maxElementSize;
        
#if DEBUG_REFLOW
  ListTag(stdout); 
  if (reason == eReflowReason_Incremental && aInfo.isIncremental) 
     printf(": INCREMENTALLY reflowing ");
  else
     printf(": reflowing ");

   nsFrame::ListTag(stdout, childFrame);
   char ch[100];
   aReason.ToCString(ch,100);
   printf("because (%s)\n", ch);
#endif
        // do the flow
        // Note that we don't position the frame (or its view) now. If we knew
        // where we were going to place the child, then it would be more
        // efficient to position it now...
        childFrame->WillReflow(aPresContext);
        childFrame->Reflow(aPresContext, desiredSize, reflowState, aStatus);

        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

        nsFrameState  kidState;
        childFrame->GetFrameState(&kidState);

       // printf("width: %d, height: %d\n", desiredSize.mCombinedArea.width, desiredSize.mCombinedArea.height);

        if (kidState & NS_FRAME_OUTSIDE_CHILDREN) {
             desiredSize.width = desiredSize.mCombinedArea.width;
             desiredSize.height = desiredSize.mCombinedArea.height;
        }

        
//        if (maxElementSize.width > desiredSize.width)
  //          desiredSize.width = maxElementSize.width;

        PRBool changedSize = PR_FALSE;

        if (currentRect.width != desiredSize.width || currentRect.height != desiredSize.height)
           changedSize = PR_TRUE;
        
        // if the child got bigger then make sure the new size in our min max range
        if (changedSize) {
          
          // redraw if we changed size.
          aRedraw = PR_TRUE;

          if (aInfo.maxSize.width != NS_INTRINSICSIZE && desiredSize.width > aInfo.maxSize.width - (margin.left + margin.right))
              desiredSize.width = aInfo.maxSize.width - (margin.left + margin.right);

          // if the child was bigger than anticipated and there was a min size set thennn
          if (aInfo.calculatedSize.width != NS_INTRINSICSIZE && position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
             nscoord min = position->mMinWidth.GetCoordValue();
             if (min != 0)
                 desiredSize.width = aInfo.calculatedSize.width - (margin.left + margin.right);
          }

          if (aInfo.maxSize.height != NS_INTRINSICSIZE && desiredSize.height > aInfo.maxSize.height - (margin.top + margin.bottom))
              desiredSize.height = aInfo.maxSize.height - (margin.top + margin.bottom);

          // if a min size was set we will always get the desired height
          if (aInfo.calculatedSize.height != NS_INTRINSICSIZE && position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
             nscoord min = position->mMinHeight.GetCoordValue();
             if (min != 0)
                 desiredSize.height = aInfo.calculatedSize.height - (margin.top + margin.bottom);
          }

        }

        // set the rect and size the view (if it has one).. Don't position the view
        // and sync its properties (like opacity) until later when we know its final
        // position
        childFrame->SizeTo(aPresContext, desiredSize.width, desiredSize.height);
        nsIView*  view;
        childFrame->GetView(aPresContext, &view);
        if (view) {
          nsIViewManager  *vm;

          view->GetViewManager(vm);
          vm->ResizeView(view, desiredSize.width, desiredSize.height);
          NS_RELEASE(vm);
        }
        childFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);

        // Stub out desiredSize.maxElementSize so that when go out of
        // scope, nothing bad happens!
        desiredSize.maxElementSize = nsnull;

        // clear out the incremental child, so that we don't flow it incrementally again
        if (reason == eReflowReason_Incremental && aInfo.isIncremental) 
          aInfo.isIncremental = PR_FALSE;
      
      }
      // add the margin back in. The child should add its border automatically
      desiredSize.height += (margin.top + margin.bottom);
      desiredSize.width += (margin.left + margin.right);

      aInfo.needsReflow = PR_FALSE;

      return NS_OK;
}

/**
 * Given a box info object and a rect. Make sure the rect is not too small to layout the box and
 * not to big either.
 */
void
nsBoxFrame::BoundsCheck(const nsBoxInfo& aBoxInfo, nsRect& aRect)
{ 
  // if we are bieng flowed at our intrinsic width or height then set our width
  // to the biggest child.
  if (aRect.height == NS_INTRINSICSIZE ) 
      aRect.height = aBoxInfo.prefSize.height;

  if (aRect.width == NS_INTRINSICSIZE ) 
      aRect.width = aBoxInfo.prefSize.width;

  // make sure the available size is no bigger than the max size
  if (aRect.height > aBoxInfo.maxSize.height)
     aRect.height = aBoxInfo.maxSize.height;

  if (aRect.width > aBoxInfo.maxSize.width)
     aRect.width = aBoxInfo.maxSize.width;

  // make sure the available size is at least as big as the min size
  if (aRect.height < aBoxInfo.minSize.height)
     aRect.height = aBoxInfo.minSize.height;

  if (aRect.width < aBoxInfo.minSize.width)
     aRect.width = aBoxInfo.minSize.width;
     
}

/**
 * Ok when calculating a boxes size such as its min size we need to look at its children to figure it out.
 * But this isn't as easy as just adding up its childs min sizes. If the box is horizontal then we need to 
 * add up each child's min width but our min height should be the childs largest min height. This needs to 
 * be done for preferred size and max size as well. Of course for our max size we need to pick the smallest
 * max size. So this method facilitates the calculation. Just give it 2 sizes and a flag to ask whether is is
 * looking for the largest or smallest value (max needs smallest) and it will set the second value.
 */
void
nsBoxFrame::AddSize(const nsSize& a, nsSize& b, PRBool largest)
{

  // depending on the dimension switch either the width or the height component. 
  const nscoord& awidth  = mInner->mHorizontal ? a.width  : a.height;
  const nscoord& aheight = mInner->mHorizontal ? a.height : a.width;
  nscoord& bwidth  = mInner->mHorizontal ? b.width  : b.height;
  nscoord& bheight = mInner->mHorizontal ? b.height : b.width;

  // add up the widths make sure we check for intrinsic.
  if (bwidth != NS_INTRINSICSIZE) // if we are already intrinsic we are done
  {
    // otherwise if what we are adding is intrinsic then we just become instrinsic and we are done
    if (awidth == NS_INTRINSICSIZE)
       bwidth = NS_INTRINSICSIZE;
    else // add it on
       bwidth += awidth;
  }
  
  // store the largest or smallest height
  if ((largest && aheight > bheight) || (!largest && bheight < aheight)) 
      bheight = aheight;
}


void 
nsBoxFrame::GetInset(nsMargin& margin)
{
}

#define GET_WIDTH(size) (IsHorizontal() ? size.width : size.height)
#define GET_HEIGHT(size) (IsHorizontal() ? size.height : size.width)

void
nsBoxFrame::InvalidateChildren()
{
    nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
    while(info) {
        info->sizeValid = PR_FALSE;
        info = info->next;
    }
}

void
nsBoxFrame::LayoutChildrenInRect(nsRect& size)
{
      nsCalculatedBoxInfo* first = mInner->mInfoList->GetFirst();

      if (!first)
          return;

      PRInt32 sizeRemaining;
       
      if (mInner->mHorizontal)
          sizeRemaining = size.width;
      else
          sizeRemaining = size.height;

      PRInt32 springConstantsRemaining = 0;

      nsCalculatedBoxInfo* info = first;

      while(info) { 
          // ignore collapsed children
          if (info->collapsed) {
              info = info->next;
              continue;
          }

          // figure out the direction of the box and get the correct value either the width or height
          nscoord pref = GET_WIDTH(info->prefSize);
          // nscoord& max  = GET_WIDTH(spring.maxSize); // Not used.
          nscoord min  = GET_WIDTH(info->minSize);
         
          GET_HEIGHT(info->calculatedSize) = GET_HEIGHT(size);
        
          if (pref < min) {
              pref = min;
              GET_WIDTH(info->prefSize) = min;
          }

          if (info->sizeValid) { 
             sizeRemaining -= GET_WIDTH(info->calculatedSize);
          } else {
            if (info->flex == 0)
            {
              info->sizeValid = PR_TRUE;
              GET_WIDTH(info->calculatedSize) = pref;
            }
            sizeRemaining -= pref;
            springConstantsRemaining += info->flex;
          } 

          info = info->next;
      }

      nscoord& sz = GET_WIDTH(size);
      if (sz == NS_INTRINSICSIZE) {
          sz = 0;
          info = first;
          while(info) {

              // ignore collapsed springs
              if (info->collapsed) {
                 info = info->next;
                 continue;
              }

              nscoord pref = GET_WIDTH(info->prefSize);

              if (!info->sizeValid) 
              {
                // set the calculated size to be the preferred size
                GET_WIDTH(info->calculatedSize) = pref;
                info->sizeValid = PR_TRUE;
              }

              // changed the size returned to reflect
              sz += GET_WIDTH(info->calculatedSize);

              info = info->next;

          }
          return;
      }

      PRBool limit = PR_TRUE;
      for (int pass=1; PR_TRUE == limit; pass++) {
          limit = PR_FALSE;
          info = first;
          while(info) {
              // ignore collapsed springs
              if (info->collapsed) {
                 info = info->next;
                 continue;
              }

              nscoord pref = GET_WIDTH(info->prefSize);
              nscoord max  = GET_WIDTH(info->maxSize);
              nscoord min  = GET_WIDTH(info->minSize);
              nscoord calculated = GET_WIDTH(info->calculatedSize);
     
              if (info->sizeValid==PR_FALSE) {
                  PRInt32 newSize = pref + (sizeRemaining*info->flex/springConstantsRemaining);
                  if (newSize<=min) {
                      GET_WIDTH(info->calculatedSize) = min;
                      springConstantsRemaining -= info->flex;
                      sizeRemaining += pref;
                      sizeRemaining -= calculated;
 
                      info->sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
                  else if (newSize>=max) {
                      GET_WIDTH(info->calculatedSize) = max;
                      springConstantsRemaining -= info->flex;
                      sizeRemaining += pref;
                      sizeRemaining -= calculated;
                      info->sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
              }
              info = info->next;
          }
      }

      //PRInt32 stretchFactor = (springConstantsRemaining == 0) ? 0 : sizeRemaining/springConstantsRemaining;

        nscoord& s = GET_WIDTH(size);
        s = 0;
        info = first;
        while(info) {

             // ignore collapsed springs
            if (info->collapsed) {
                 info = info->next;
                 continue;
            }

             nscoord pref = GET_WIDTH(info->prefSize);
  
            if (info->sizeValid==PR_FALSE) {
                if (springConstantsRemaining == 0) 
                    GET_WIDTH(info->calculatedSize) = pref;
                else
                    GET_WIDTH(info->calculatedSize) = pref + info->flex*sizeRemaining/springConstantsRemaining;

                info->sizeValid = PR_TRUE;
            }

            s += GET_WIDTH(info->calculatedSize);
            info = info->next;
        }
}


void 
nsBoxFrame::GetChildBoxInfo(PRInt32 aIndex, nsBoxInfo& aSize)
{
    PRInt32 infoCount = mInner->mInfoList->GetCount();

    NS_ASSERTION(aIndex >= 0 && aIndex < infoCount,"Index out of bounds!!");

    nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

    for (PRInt32 i=0; i< infoCount; i++) {
        if (i == aIndex)
            break;
        info = info->next;
    }

    aSize = *info;
}

void 
nsBoxFrame::SetChildNeedsRecalc(PRInt32 aIndex, PRBool aRecalc)
{
    PRInt32 infoCount = mInner->mInfoList->GetCount();

    NS_ASSERTION(aIndex >= 0 && aIndex < infoCount,"Index out of bounds!!");
    nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

    for (PRInt32 i=0; i< infoCount; i++) {
         if (i == aIndex)
            break;
        info = info->next;
    }

    info->needsRecalc = PR_TRUE;
}


// Marks the frame as dirty and generates an incremental reflow
// command targeted at this frame
nsresult
nsBoxFrame::GenerateDirtyReflowCommand(nsIPresContext* aPresContext,
                                       nsIPresShell&   aPresShell)
{
  nsCOMPtr<nsIReflowCommand>  reflowCmd;
  nsresult                    rv;
  
  mState |= NS_FRAME_IS_DIRTY;
  rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), this,
                               nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell.AppendReflowCommand(reflowCmd);
  }

  return rv;
}

NS_IMETHODIMP
nsBoxFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
    mInner->SanityCheck();

    // remove child from our info list
    mInner->mInfoList->Remove(aOldFrame);

    // remove the child frame
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    mInner->SanityCheck();

    // mark us dirty and generate a reflow command
    return GenerateDirtyReflowCommand(aPresContext, aPresShell);
}

NS_IMETHODIMP
nsBoxFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
   mInner->SanityCheck();

   // insert the frames to our info list
   mInner->mInfoList->Insert(aPrevFrame, aFrameList);

   // insert the frames in out regular frame list
   mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

   mInner->SanityCheck();

   // mark us dirty and generate a reflow command
   return GenerateDirtyReflowCommand(aPresContext, aPresShell);
   
}


NS_IMETHODIMP
nsBoxFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
   mInner->SanityCheck();

    // append them after
   mInner->mInfoList->Append(aFrameList);

   // append in regular frames
   mFrames.AppendFrames(nsnull, aFrameList); 
   
   mInner->SanityCheck();

   // mark us dirty and generate a reflow command
   return GenerateDirtyReflowCommand(aPresContext, aPresShell);
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

  if (NS_OK != rv) {
    return rv;
  }

  return NS_OK;
}

/**
 * Goes though each child asking for its size to determine our size. Returns our box size minus our border.
 * This method is defined in nsIBox interface.
 */
NS_IMETHODIMP
nsBoxFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
   nsresult rv;

   aSize.Clear();
 
   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

       nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

       while (info) 
       {  
        // if a child needs recalculation then ask it for its size. Otherwise
        // just use the size we already have.
        if (info->needsRecalc)
        {
          // see if the child is collapsed
          const nsStyleDisplay* disp;
          info->frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));

          // if collapsed then the child will have no size
          if (disp->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
             info->collapsed = PR_TRUE;
          else {
            // get the size of the child. This is the min, max, preferred, and spring constant
            // it does not include its border.
            rv = GetChildBoxInfo(aPresContext, aReflowState, info->frame, *info);
            NS_ASSERTION(rv == NS_OK,"failed to child box info");
            if (NS_FAILED(rv))
             return rv;

            // add in the child's margin and border/padding if there is one.
            const nsStyleSpacing* spacing;
            rv = info->frame->GetStyleData(eStyleStruct_Spacing,
                                          (const nsStyleStruct*&) spacing);

            NS_ASSERTION(rv == NS_OK,"failed to get spacing info");
            if (NS_FAILED(rv))
               return rv;

            nsMargin margin(0,0,0,0);
            spacing->GetMargin(margin);
            nsSize m(margin.left+margin.right,margin.top+margin.bottom);
            info->minSize += m;
            info->prefSize += m;
            if (info->maxSize.width != NS_INTRINSICSIZE)
               info->maxSize.width += m.width;

            if (info->maxSize.height != NS_INTRINSICSIZE)
               info->maxSize.height += m.height;

            spacing->GetBorderPadding(margin);
            nsSize b(margin.left+margin.right,margin.top+margin.bottom);
            info->minSize += b;
            info->prefSize += b;
            if (info->maxSize.width != NS_INTRINSICSIZE)
               info->maxSize.width += b.width;

            if (info->maxSize.height != NS_INTRINSICSIZE)
               info->maxSize.height += b.height;
          }

          // ok we don't need to calc this guy again
          info->needsRecalc = PR_FALSE;
        } 

        AddChildSize(aSize, *info);

        info = info->next;
       }
   
  // add the insets into our size. This is merely some extra space subclasses like toolbars
  // can place around us. Toolbars use it to place extra control like grippies around things.
  nsMargin inset(0,0,0,0);
  GetInset(inset);

  nsMargin debugInset(0,0,0,0);
  mInner->GetDebugInset(debugInset);
  inset += debugInset;

  nsSize in(inset.left+inset.right,inset.top+inset.bottom);
  aSize.minSize += in;
  aSize.prefSize += in;

  return rv;
}

void
nsBoxFrame::AddChildSize(nsBoxInfo& aInfo, nsBoxInfo& aChildInfo)
{
    // now that we know our child's min, max, pref sizes figure OUR size from them.
    AddSize(aChildInfo.minSize,  aInfo.minSize,  PR_FALSE);
    AddSize(aChildInfo.maxSize,  aInfo.maxSize,  PR_TRUE);
    AddSize(aChildInfo.prefSize, aInfo.prefSize, PR_FALSE);
}

/**
 * Boxes work differently that regular HTML elements. Each box knows if it needs to be reflowed or not
 * So when a box gets an incremental reflow. It runs down all the children and marks them for reflow. If it
 * Reaches a child that is not a box then it marks that child as incremental so when it is flowed next it 
 * will be flowed incrementally.
 */
NS_IMETHODIMP
nsBoxFrame::Dirty(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame*& incrementalChild)
{
  nsIFrame* targetFrame = nsnull;
  aReflowState.reflowCommand->GetTarget(targetFrame);
  if (this == targetFrame) {
    // if it has redraw us if we are the target
    Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);
  }

  incrementalChild = nsnull;
  nsresult rv = NS_OK;

  // Dirty any children that need it.
  nsIFrame* frame;
  aReflowState.reflowCommand->GetNext(frame);

  nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

  while (info) 
  {
    if (info->frame == frame) {
        // clear the spring so it is recalculated on the flow
        info->Clear();
        // can't use nsCOMPtr on non-refcounted things like frames
        nsIBox* ibox;
        if (NS_SUCCEEDED(info->frame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox)
            ibox->Dirty(aPresContext, aReflowState, incrementalChild);
        else 
            incrementalChild = frame;

        if (incrementalChild == info->frame) 
            info->isIncremental = PR_TRUE;
        
        break;
    }

    info = info->next;
  }

  return rv;
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
  if (!disp->mVisible) 
	   return NS_OK;  

  // if we are visible then tell our superclass to paint
  nsresult r = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
        if (mInner->mDebugInner) {
            mInner->mDebugInner->PaintSprings(aPresContext, aRenderingContext, aDirtyRect);
        }
  }
 

  return r;
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
nsBoxDebugInner::PaintSprings(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, const nsRect& aDirtyRect)
{
    
        // remove our border
        const nsStyleSpacing* spacing;
        mOuter->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&) spacing);

        nsMargin border(0,0,0,0);
        spacing->GetBorderPadding(border);

        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

        nsIStyleContext* debugStyle;
        if (mOuter->mInner->mHorizontal)
            debugStyle = mHorizontalDebugStyle;
        else
            debugStyle = mVerticalDebugStyle;

        const nsStyleSpacing* debugSpacing =
        (const nsStyleSpacing*)debugStyle->GetStyleData(eStyleStruct_Spacing);
        const nsStyleColor* debugColor =
        (const nsStyleColor*)debugStyle->GetStyleData(eStyleStruct_Color);

        nsMargin margin(0,0,0,0);
        debugSpacing->GetMargin(margin);

        border += margin;

        nsRect inner(0,0,mOuter->mRect.width, mOuter->mRect.height);
        inner.Deflate(border);

        // paint our debug border
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mOuter,
                                    aDirtyRect, inner, *debugSpacing, debugStyle, 0);

        // get the debug border dimensions
        nsMargin debugBorder(0,0,0,0);
        debugSpacing->GetBorderPadding(debugBorder);


        // paint the springs.
        nscoord x, y, borderSize, springSize;
        

        aRenderingContext.SetColor(debugColor->mColor);
        

        if (mOuter->mInner->mHorizontal) 
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

        nsCalculatedBoxInfo* info = mOuter->GetInfoList()->GetFirst();
        while (info) {
            nsSize& size = info->calculatedSize;
            if (!info->collapsed) {
                if (mOuter->mInner->mHorizontal) 
                    borderSize = size.width;
                else 
                    borderSize = size.height;

                DrawSpring(aPresContext, aRenderingContext, info->flex, x, y, borderSize, springSize);
                x += borderSize;
            }
            info = info->next;
        }
}

void
nsBoxDebugInner::DrawLine(nsIRenderingContext& aRenderingContext, nscoord x1, nscoord y1, nscoord x2, nscoord y2)
{
    if (mOuter->mInner->mHorizontal)
       aRenderingContext.DrawLine(x1,y1,x2,y2);
    else
       aRenderingContext.DrawLine(y1,x1,y2,x2);
}

void
nsBoxDebugInner::FillRect(nsIRenderingContext& aRenderingContext, nscoord x, nscoord y, nscoord width, nscoord height)
{
    if (mOuter->mInner->mHorizontal)
       aRenderingContext.FillRect(x,y,width,height);
    else
       aRenderingContext.FillRect(y,x,height,width);
}

void 
nsBoxDebugInner::DrawSpring(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord springSize)
{
        // PRBool h = mOuter->mHorizontal;
    
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
            DrawLine(aRenderingContext, x,y + springSize/2, x + size, y + springSize/2);
        } else {
            for (int i=0; i < coils; i++)
            {
                   DrawLine(aRenderingContext, offset, center+halfSpring, offset+halfCoilSize, center-halfSpring);
                   DrawLine(aRenderingContext, offset+halfCoilSize, center-halfSpring, offset+coilSize, center+halfSpring);

                   offset += coilSize;
            }
        }

        FillRect(aRenderingContext, x + size - springSize/2, y, springSize/2, springSize);
        FillRect(aRenderingContext, x, y, springSize/2, springSize);

        //DrawKnob(aPresContext, aRenderingContext, x + size - springSize, y, springSize);
}

void
nsBoxFrameInner::UpdatePseudoElements(nsIPresContext* aPresContext) 
{
    nsCOMPtr<nsIStyleContext> hs;
    nsCOMPtr<nsIStyleContext> vs;

    nsCOMPtr<nsIContent> content;
    GetContentOf(mOuter, getter_AddRefs(content));

	nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-horizontal-box-debug")) );
	aPresContext->ProbePseudoStyleContextFor(content, atom, mOuter->mStyleContext,
										  PR_FALSE,
										  getter_AddRefs(hs));

  	atom = getter_AddRefs(NS_NewAtom(":-moz-vertical-box-debug"));
	aPresContext->ProbePseudoStyleContextFor(content, atom, mOuter->mStyleContext,
										  PR_FALSE,
										  getter_AddRefs(vs));

    if (hs && vs) {
        if (!mDebugInner) {
            mDebugInner = new nsBoxDebugInner(mOuter);
        }
        mDebugInner->mHorizontalDebugStyle = hs;
        mDebugInner->mVerticalDebugStyle = vs;
        aPresContext->GetScaledPixelsToTwips(&mDebugInner->mP2t);
    } else {
        if (mDebugInner) 
        {
            delete mDebugInner;
            mDebugInner = nsnull;
        }
    }
}


void
nsBoxFrameInner::GetDebugInset(nsMargin& inset)
{
    inset.SizeTo(0,0,0,0);

    if (mDebugInner) 
    {
        nsIStyleContext* style;
        if (mOuter->mInner->mHorizontal)
            style = mDebugInner->mHorizontalDebugStyle;
        else
            style = mDebugInner->mVerticalDebugStyle;

        const nsStyleSpacing* debugSpacing =
        (const nsStyleSpacing*)style->GetStyleData(eStyleStruct_Spacing);

        debugSpacing->GetBorderPadding(inset);
        nsMargin margin(0,0,0,0);
        debugSpacing->GetMargin(margin);
        inset += margin;
    }
}

NS_IMETHODIMP nsBoxFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIBoxIID)) {                                         
    *aInstancePtr = (void*)(nsIBox*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                     
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

NS_IMETHODIMP
nsBoxFrame::GetFrameName(nsString& aResult) const
{
  aResult = "Box";
  return NS_OK;
}

//static PRInt32 gBoxInfoCount = 0;

nsCalculatedBoxInfoImpl::nsCalculatedBoxInfoImpl(nsIFrame* aFrame)
{
   //gBoxInfoCount++;
  // printf("created Info=%d\n",gBoxInfoCount);

    next = nsnull;
    needsReflow = PR_TRUE;
    needsRecalc = PR_TRUE;
    collapsed = PR_FALSE;
    calculatedSize.width = 0;
    calculatedSize.height = 0;
    sizeValid = PR_FALSE;
    isIncremental = PR_FALSE;
    frame = aFrame;
    prefWidthIntrinsic = PR_TRUE;
    prefHeightIntrinsic = PR_TRUE;
}

nsCalculatedBoxInfoImpl::~nsCalculatedBoxInfoImpl()
{
 //  gBoxInfoCount--;
  // printf("deleted Info=%d\n",gBoxInfoCount);
}

void 
nsCalculatedBoxInfoImpl::Clear()
{       
    nsBoxInfo::Clear();

    needsReflow = PR_TRUE;
    needsRecalc = PR_TRUE;
    collapsed = PR_FALSE;

    calculatedSize.width = 0;
    calculatedSize.height = 0;

    sizeValid = PR_FALSE;

   prefWidthIntrinsic = PR_TRUE;
   prefHeightIntrinsic = PR_TRUE;

}

NS_IMETHODIMP  
nsBoxFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                             const nsPoint& aPoint, 
                             nsIFrame**     aFrame)
{   
    nsresult rv = nsHTMLContainerFrame::GetFrameForPoint(aPresContext, aPoint, aFrame);
    return rv;
}

void
nsBoxDebugInner::GetValue(const nsSize& a, const nsSize& b, char* ch) 
{
    char width[100];
    char height[100];
    
    if (a.width == NS_INTRINSICSIZE)
        sprintf(width,"%s","INF");
    else 
        sprintf(width,"%d", nscoord(a.width/mP2t));
    
    if (a.height == NS_INTRINSICSIZE)
        sprintf(height,"%s","INF");
    else 
        sprintf(height,"%d", nscoord(a.height/mP2t));
    

    sprintf(ch, "(%s%s, %s%s)", width, (b.width != NS_INTRINSICSIZE ? "[CSS]" : ""),
                    height, (b.height != NS_INTRINSICSIZE ? "[CSS]" : ""));

}

void
nsBoxDebugInner::GetValue(PRInt32 a, PRInt32 b, char* ch) 
{
    sprintf(ch, "(%d)", a);             
}

nsresult
nsBoxDebugInner::DisplayDebugInfoFor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
    nscoord x = aPoint.x;
    nscoord y = aPoint.y;

     /*
    // get it into our coordintate system by subtracting our parents offsets.
    nsIFrame* parent = mOuter;
    while(parent != nsnull)
    {
      // if we hit a scrollable view make sure we take into account
      // how much we are scrolled.
      nsIScrollableView* scrollingView;
      nsIView*           view;
      parent->GetView(aPresContext, &view);
      if (view) {
        nsresult result = view->QueryInterface(kScrollViewIID, (void**)&scrollingView);
        if (NS_SUCCEEDED(result)) {
            nscoord xoff = 0;
            nscoord yoff = 0;
            scrollingView->GetScrollPosition(xoff, yoff);
            x += xoff;
            y += yoff;         
        }
      }

     nsRect cr;
     parent->GetRect(cr);
     x -= cr.x;
     y -= cr.y;
     parent->GetParent(&parent);
    }
    */

    nsRect r(0,0,mOuter->mRect.width, mOuter->mRect.height);
    PRBool isHorizontal = mOuter->mInner->mHorizontal;

    if (!r.Contains(nsPoint(x,y)))
        return NS_OK;

        int count = 0;
        nsCalculatedBoxInfo* info = mOuter->GetInfoList()->GetFirst();

        while (info) 
        {    
            nsIFrame* childFrame = info->frame;
            childFrame->GetRect(r);

            // if we are not in the child. But in the spring above the child.
            if (((isHorizontal && x >= r.x && x < r.x + r.width && y < r.y) ||
                (!isHorizontal && y >= r.y && y < r.y + r.height && x < r.x))) {
                //printf("y=%d, r.y=%d\n",y,r.y);
                aCursor = NS_STYLE_CURSOR_POINTER;
                   // found it but we already showed it.
                    if (mDebugChild == childFrame)
                        return NS_OK;

                    nsCOMPtr<nsIContent> content;
                    mOuter->mInner->GetContentOf(childFrame, getter_AddRefs(content));

                    nsString id;
                    content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);
                    char idValue[100];
                    id.ToCString(idValue,100);
                       
                    nsString kClass;
                    content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::kClass, kClass);
                    char kClassValue[100];
                    kClass.ToCString(kClassValue,100);

                    nsCOMPtr<nsIAtom> tag;
                    content->GetTag(*getter_AddRefs(tag));
                    nsString tagString;
                    tag->ToString(tagString);
                    char tagValue[100];
                    tagString.ToCString(tagValue,100);

                    printf("--------------------\n");
                    printf("Tag='%s', id='%s' class='%s'\n", tagValue, idValue, kClassValue);

                    mDebugChild = childFrame;
                    nsCalculatedBoxInfoImpl aSize(childFrame);
                    aSize.prefSize.width = NS_INTRINSICSIZE;
                    aSize.prefSize.height = NS_INTRINSICSIZE;

                    aSize.minSize.width = NS_INTRINSICSIZE;
                    aSize.minSize.height = NS_INTRINSICSIZE;

                    aSize.maxSize.width = NS_INTRINSICSIZE;
                    aSize.maxSize.height = NS_INTRINSICSIZE;

                    aSize.calculatedSize.width = NS_INTRINSICSIZE;
                    aSize.calculatedSize.height = NS_INTRINSICSIZE;

                    aSize.flex = -1;

                  // add in the css min, max, pref
                    const nsStylePosition* position;
                    nsresult rv = childFrame->GetStyleData(eStyleStruct_Position,
                                  (const nsStyleStruct*&) position);

                    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get position");
                    if (NS_FAILED(rv))
                        return rv;

                    // see if the width or height was specifically set
                    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
                        aSize.prefSize.width = position->mWidth.GetCoordValue();
                    }

                    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
                        aSize.prefSize.height = position->mHeight.GetCoordValue();     
                    }
    
                    // same for min size. Unfortunately min size is always set to 0. So for now
                    // we will assume 0 means not set.
                    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
                        nscoord min = position->mMinWidth.GetCoordValue();
                        if (min != 0)
                           aSize.minSize.width = min;
                    }

                    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
                        nscoord min = position->mMinHeight.GetCoordValue();
                        if (min != 0)
                           aSize.minSize.height = min;
                    }

                    // and max
                    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
                        nscoord max = position->mMaxWidth.GetCoordValue();
                        aSize.maxSize.width = max;
                    }

                    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
                        nscoord max = position->mMaxHeight.GetCoordValue();
                        aSize.maxSize.height = max;
                    }

                    PRInt32 error;
                    nsAutoString value;

                    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
                    {
                        value.Trim("%");
                        aSize.flex = value.ToInteger(&error);
                    }

                    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, value))
                    {
                        float p2t;
                        aPresContext->GetScaledPixelsToTwips(&p2t);

                        value.Trim("%");

                        aSize.prefSize.width = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
                    }

                    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::height, value))
                    {
                        float p2t;
                        aPresContext->GetScaledPixelsToTwips(&p2t);

                        value.Trim("%");

                        aSize.prefSize.height = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
                    }

           

                    char min[100];
                    char pref[100];
                    char max[100];
                    char calc[100];
                    char flex[100];

                    GetValue(info->minSize,  aSize.minSize, min);
                    GetValue(info->prefSize, aSize.prefSize, pref);
                    GetValue(info->maxSize, aSize.maxSize, max);
                    GetValue(info->calculatedSize,  aSize.calculatedSize, calc);
                    GetValue(info->flex,  aSize.flex, flex);


                    printf("min%s, pref%s, max%s, actual%s, flex=%s\n", 
                        min,
                        pref,
                        max,
                        calc,
                        flex
                    );
                break;   
            }

          info = info->next;
          count++;
        }
        return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetCursor(nsIPresContext* aPresContext,
                           nsPoint&        aPoint,
                           PRInt32&        aCursor)
{
    nsresult rv = nsHTMLContainerFrame::GetCursor(aPresContext, aPoint, aCursor);

    if (mInner->mDebugInner)
        mInner->mDebugInner->DisplayDebugInfoFor(aPresContext, aPoint, aCursor);

    return rv;
}

nsresult 
nsBoxFrameInner::GetContentOf(nsIFrame* aFrame, nsIContent** aContent)
{
    // If we don't have a content node find a parent that does.
    while(aFrame != nsnull) {
        aFrame->GetContent(aContent);
        if (*aContent != nsnull)
            return NS_OK;

        aFrame->GetParent(&aFrame);
    }

    NS_ERROR("Can't find a parent with a content node");
    return NS_OK;
}

void
nsBoxFrameInner::SanityCheck()
{ 
#ifdef TEST_SANITY
   mInfoList->SanityCheck(mOuter->mFrames);
#endif
}

nsInfoListImpl::nsInfoListImpl():mFirst(nsnull),mLast(nsnull),mCount(0)
{

}

nsInfoListImpl::~nsInfoListImpl() 
{
  Clear();
}

nsCalculatedBoxInfo* 
nsInfoListImpl::GetFirst()
{
    return mFirst;
}

nsCalculatedBoxInfo* 
nsInfoListImpl::GetLast()
{
    return mLast;
}

void
nsInfoListImpl::Clear()
{
   nsCalculatedBoxInfo* info = mFirst;
   while(info) {
      nsCalculatedBoxInfo* it = info;
      info = info->next;
      delete it;
   }
   mFirst = nsnull;
   mLast = nsnull;
   mCount = 0;
}

PRInt32 
nsInfoListImpl::GetCount()
{
   return mCount;
}

PRInt32 
nsInfoListImpl::CreateInfosFor(nsIFrame* aList, nsCalculatedBoxInfo*& first, nsCalculatedBoxInfo*& last)
{
    PRInt32 count = 0;
    if (aList) {
       first = new nsCalculatedBoxInfoImpl(aList);
       count++;
       last = first;
       aList->GetNextSibling(&aList);
       while(aList) {
         last->next = new nsCalculatedBoxInfoImpl(aList);
         count++;
         aList->GetNextSibling(&aList);       
         last = last->next;
       }
    }

    return count;
}

nsCalculatedBoxInfo* 
nsInfoListImpl::GetPrevious(nsIFrame* aFrame)
{
    if (aFrame == nsnull)
        return nsnull;

   // find the frame to remove
    nsCalculatedBoxInfo* info = mFirst;
    nsCalculatedBoxInfo* prev = nsnull;
    while (info) 
    {        
      if (info->frame == aFrame) {
          return prev;
      }

      prev = info;
      info = info->next;
    }

    return nsnull;
}

nsCalculatedBoxInfo* 
nsInfoListImpl::GetInfo(nsIFrame* aFrame)
{
    if (aFrame == nsnull)
        return nsnull;

   // find the frame to remove
    nsCalculatedBoxInfo* info = mFirst;
    while (info) 
    {        
      if (info->frame == aFrame) {
          return info;
      }

      info = info->next;
    }

    return nsnull;
}

void
nsInfoListImpl::Remove(nsIFrame* aFrame)
{
    // get the info before the frame
    nsCalculatedBoxInfo* prevInfo = GetPrevious(aFrame);
    RemoveAfter(prevInfo);        
}

void
nsInfoListImpl::Insert(nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
   nsCalculatedBoxInfo* prevInfo = GetInfo(aPrevFrame);

   // find the frame before this one
   // if no previous frame then we are inserting in front
   if (prevInfo == nsnull) {
       // prepend them
       Prepend(aFrameList);
   } else {
       // insert insert after previous info
       InsertAfter(prevInfo, aFrameList);
   }
}

void
nsInfoListImpl::RemoveAfter(nsCalculatedBoxInfo* aPrevious)
{
    nsCalculatedBoxInfo* toDelete = nsnull;

    if (aPrevious == nsnull)
    {
        NS_ASSERTION(mFirst,"Can't find first child");
        toDelete = mFirst;
        if (mLast == mFirst)
            mLast = mFirst->next;
        mFirst = mFirst->next;
    } else {
        toDelete = aPrevious->next;
        NS_ASSERTION(toDelete,"Can't find child to delete");
        aPrevious->next = toDelete->next;
        if (mLast == toDelete)
            mLast = aPrevious;
    }

    delete toDelete;
    mCount--;
}

void
nsInfoListImpl::Prepend(nsIFrame* aList)
{
    nsCalculatedBoxInfo* first;
    nsCalculatedBoxInfo* last;
    mCount += CreateInfosFor(aList, first, last);
    if (!mFirst)
        mFirst = mLast = first;
    else {
        last->next = mFirst;
        mFirst = first;
    }
}

void
nsInfoListImpl::Append(nsIFrame* aList)
{
    nsCalculatedBoxInfo* first;
    nsCalculatedBoxInfo* last;
    mCount += CreateInfosFor(aList, first, last);
    if (!mFirst) 
        mFirst = first;
    else 
        mLast->next = first;
    
    mLast = last;
}

void 
nsInfoListImpl::InsertAfter(nsCalculatedBoxInfo* aPrev, nsIFrame* aList)
{
    nsCalculatedBoxInfo* first;
    nsCalculatedBoxInfo* last;
    mCount += CreateInfosFor(aList, first, last);
    last->next = aPrev->next;
    aPrev->next = first;
    if (aPrev == mLast)
        mLast = last;
}

void 
nsInfoListImpl::Init(nsIFrame* aList)
{
    Clear();
    mCount += CreateInfosFor(aList, mFirst, mLast);
}

void
nsInfoListImpl::SanityCheck(nsFrameList& aFrameList)
{
    // make sure the length match
    PRInt32 length = aFrameList.GetLength();
    NS_ASSERTION(length == mCount,"nsBox::ERROR!! Box info list count does not match frame count!!");

    // make sure last makes sense
    NS_ASSERTION(mLast == nsnull || mLast->next == nsnull,"nsBox::ERROR!! The last child is not really the last!!!");
    nsIFrame* child = aFrameList.FirstChild();
    nsCalculatedBoxInfo* info = mFirst;
    PRInt32 count = 0;
    while(child)
    {
       NS_ASSERTION(count <= mCount,"too many children!!!");
       NS_ASSERTION(info->frame == child,"nsBox::ERROR!! box info list and child info lists don't match!!!"); 
       info = info->next;
       child->GetNextSibling(&child);
       count++;
    }
}


// ---- box info ------

nsBoxInfo::nsBoxInfo():prefSize(0,0), minSize(0,0), flex(0), maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE)
{ 
}

void 
nsBoxInfo::Clear()
{
      prefSize.width = 0;
      prefSize.height = 0;

      minSize.width = 0;
      minSize.height = 0;

      flex = 0;

      maxSize.width = NS_INTRINSICSIZE;
      maxSize.height = NS_INTRINSICSIZE;
}

nsBoxInfo::~nsBoxInfo()
{
}








