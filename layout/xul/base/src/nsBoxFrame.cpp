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
// so when asked for there current size they can relayout themselves. Thats where the Dirty method comes in. This
// moves down the reflow chain until it reaches something that is not a box marking each child as being dirty.

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

#define CONSTANT 0
//#define DEBUG_REFLOW
//define DEBUG_REDRAW
#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8

//#define TEST_SANITY

/**
 * Only created when the box is in debug mode
 */
class nsBoxDebug
{
public:

  nsBoxDebug(nsIPresContext* aPresContext, nsBoxFrame* aBoxFrame);

  static void AddRef(nsIPresContext* aPresContext, nsBoxFrame* aBox);
  static void Release(nsIPresContext* aPresContext);
  static void GetPref(nsIPresContext* aPresContext);
    
  void GetValue(nsIPresContext* aPresContext, const nsSize& a, const nsSize& b, char* value);
  void GetValue(nsIPresContext* aPresContext, PRInt32 a, PRInt32 b, char* value);
  void DrawSpring(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord springSize);
  void PaintSprings(nsIPresContext* aPresContext,  nsBoxFrame* aBoxFrame,  nsIRenderingContext& aRenderingContext, const nsRect& aDirtyRect);
  void DrawLine(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
  void FillRect(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);

  PRBool DisplayDebugInfoFor(nsIPresContext* aPresContext,
                             nsBoxFrame* aBoxFrame,
                                      nsPoint&        aPoint,
                                      PRInt32&        aCursor);

  nsCOMPtr<nsIStyleContext> mHorizontalDebugStyle;
  nsCOMPtr<nsIStyleContext> mVerticalDebugStyle;
  PRInt32 mRefCount;
  nsIFrame* mDebugChild;

  static PRBool gDebug;
  static nsBoxDebug* gInstance;
};

PRBool nsBoxDebug::gDebug = PR_FALSE;
nsBoxDebug* nsBoxDebug::gInstance = nsnull;

class nsInfoListImpl: public nsInfoList
{
public:
    void* operator new(size_t sz, nsIPresShell* aPresShell);
    void operator delete(void* aPtr, size_t sz);
    void Recycle(nsIPresShell* aShell);

    nsInfoListImpl();
    virtual ~nsInfoListImpl();
    virtual nsCalculatedBoxInfo* GetFirst();
    virtual nsCalculatedBoxInfo* GetLast();
    virtual PRInt32 GetCount();
    virtual nsresult SetDebug(nsIPresContext* aPresContext, PRBool aDebug);

    void Clear(nsIPresShell* aShell);
    PRInt32 CreateInfosFor(nsIPresShell* aShell, nsIFrame* aList, nsCalculatedBoxInfo*& first, nsCalculatedBoxInfo*& last);
    void RemoveAfter(nsIPresShell* aShell, nsCalculatedBoxInfo* aPrev);
    void Remove(nsIPresShell* aShell, nsIFrame* aChild);
    void Prepend(nsIPresShell* aShell, nsIFrame* aList);
    void Append(nsIPresShell* aShell, nsIFrame* aList);
    void Insert(nsIPresShell* aShell, nsIFrame* aPrevFrame, nsIFrame* aList);
    void InsertAfter(nsIPresShell* aShell, nsCalculatedBoxInfo* aPrev, nsIFrame* aList);
    void Init(nsIPresShell* aShell, nsIFrame* aList);
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
    void* operator new(size_t sz, nsIPresShell* aPresShell);
    void operator delete(void* aPtr, size_t sz);
    virtual void Recycle(nsIPresShell* aShell);
};

/**
 * The boxes private implementation
 */
class nsBoxFrameInner
{
public:
  nsBoxFrameInner(nsIPresShell* aPresShell, nsBoxFrame* aThis)
  {
    mOuter = aThis;
    mInfoList = new (aPresShell) nsInfoListImpl();

  }

  ~nsBoxFrameInner()
  {
  }

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
  static void* Allocate(size_t sz, nsIPresShell* aPresShell);
  static void Free(void* aPtr, size_t sz);
  static void RecycleFreedMemory(nsIPresShell* aPresShell, void* mem);

  void GetDebugInset(nsMargin& inset);
  void AdjustChildren(nsIPresContext* aPresContext, nsBoxFrame* aBox);
  nsresult GetContentOf(nsIFrame* aFrame, nsIContent** aContent);
  void SanityCheck();

  nsresult FlowChildren(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsIFrame*&               aIncrementalChild,
                     nsRect& aRect,
                     nsSize& aActualSize,
                     nscoord& aMaxAscent);

  nsresult FlowChildAt(nsIFrame* frame, 
                     nsIPresContext* aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsCalculatedBoxInfo&     aInfo,
                     nscoord                  aX,
                     nscoord                  aY,
                     PRBool                   aMoveFrame,
                     nsIFrame*&               aIncrementalChild,
                     PRBool& needsRedraw,
                     const nsString& aReason);

   void PlaceChild(nsIPresContext* aPresContext, nsIFrame* aFrame, nscoord aX, nscoord aY);

   void TranslateEventCoords(nsIPresContext* aPresContext,
                                    const nsPoint& aPoint,
                                    nsPoint& aResult);

    static void InvalidateCachesTo(nsIFrame* aTargetFrame);
    static void InvalidateAllCachesBelow(nsIPresContext* aPresContext, nsIFrame* aTargetFrame);
    static PRBool AdjustTargetToScope(nsIFrame* aParent, nsIFrame*& aTargetFrame);

    PRBool GetInitialDebug(PRBool& aDebug);

    nsBoxFrame::Halignment GetHAlign();
    nsBoxFrame::Valignment GetVAlign();
    void InitializeReflowState(nsIPresContext* aPresContext,  const nsCalculatedBoxInfo& aInfo, nsHTMLReflowState& aReflowState, const nsHTMLReflowState& aParentReflowState, nsIFrame* aFrame, const nsSize& aAvailSize, nsReflowReason reason);
    void CreateResizedArray(PRBool*& aResized, PRInt32 aCount);


#ifdef DEBUG_REFLOW
    void MakeReason(nsIFrame* aFrame, const nsString& aText, nsString& aReason);
    void AddIndents();
#endif

    nsCOMPtr<nsISpaceManager> mSpaceManager; // We own this [OWNER].
    nsBoxFrame* mOuter;
    nsInfoListImpl* mInfoList;

    // XXX make these flags!
    nscoord mInnerSize;

    nsBoxFrame::Valignment mValign;
    nsBoxFrame::Halignment mHalign;

#ifdef DEBUG_REFLOW
    PRInt32 reflowCount;
#endif

};

#ifdef DEBUG_REFLOW

PRInt32 gIndent = 0;
PRInt32 gReflows = 0;

#endif

#define GET_WIDTH(size) (IsHorizontal() ? size.width : size.height)
#define GET_HEIGHT(size) (IsHorizontal() ? size.height : size.width)

#define SET_WIDTH(size, coord)  if (IsHorizontal()) { (size).width  = (coord); } else { (size).height = (coord); }
#define SET_HEIGHT(size, coord) if (IsHorizontal()) { (size).height = (coord); } else { (size).width  = (coord); }

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

nsBoxFrame::nsBoxFrame(nsIPresShell* aPresShell, PRBool aIsRoot)
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

#ifdef DEBUG_REFLOW
  mInner->reflowCount = 100;
#endif
}

void* 
nsBoxFrameInner::operator new(size_t sz, nsIPresShell* aPresShell)
{
    return nsBoxFrameInner::Allocate(sz,aPresShell);
}

void 
nsBoxFrameInner::Recycle(nsIPresShell* aPresShell)
{
  // recyle out info list.
  mInfoList->Recycle(aPresShell);
  delete this;
  nsBoxFrameInner::RecycleFreedMemory(aPresShell, this);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsBoxFrameInner::operator delete(void* aPtr, size_t sz)
{
    nsBoxFrameInner::Free(aPtr, sz);
}

void* 
nsBoxFrameInner::Allocate(size_t sz, nsIPresShell* aPresShell)
{
  // Check the recycle list first.
  void* result = nsnull;
  aPresShell->AllocateFrame(sz, &result);
  
  if (result) {
    nsCRT::zero(result, sz);
  }

  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsBoxFrameInner::Free(void* aPtr, size_t sz)
{
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.

  // Stash the size of the object in the first four bytes of the
  // freed up memory.  The Destroy method can then use this information
  // to recycle the object.
  size_t* szPtr = (size_t*)aPtr;
  *szPtr = sz;
}


nsBoxFrame::~nsBoxFrame()
{
  NS_ASSERTION(mInner == nsnull,"Error Destroy was never called on this Frame!!!");
}


NS_IMETHODIMP
nsBoxFrame::InvalidateCache(nsIFrame* aChild)
{
   nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

   // if the child is null then blow away all caches. Otherwise just blow away the 
   // specified kids cache.
   if (aChild == nsnull) {
       while(info != nsnull) {
            info->mFlags |= NS_FRAME_BOX_NEEDS_RECALC;
            info = info->next;
        }
   } else {
       while(info != nsnull) {
            if (info->frame == aChild) {
            info->mFlags |= NS_FRAME_BOX_NEEDS_RECALC;
                return NS_OK;
            }
            info = info->next;
        }
   }

   return NS_OK;
}


NS_IMETHODIMP
nsBoxFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  mInner->SanityCheck();

  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  // initialize our list of infos.
  mInner->mInfoList->Init(shell, aChildList);

  mInner->SanityCheck();

  return r;
}


PRBool nsBoxFrame::IsHorizontal() const
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
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

 

  nsSpaceManager* spaceManager = new nsSpaceManager(this);
  mInner->mSpaceManager = spaceManager;

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
      nsBoxDebug::GetPref(aPresContext);

  return rv;
}

PRBool
nsBoxFrame::GetDefaultFlex(PRInt32& aFlex)
{
    aFlex = 0;
    return PR_TRUE;
}

PRBool
nsBoxFrameInner::GetInitialDebug(PRBool& aDebug)
{
  nsString value;

  nsCOMPtr<nsIContent> content;
  GetContentOf(mOuter, getter_AddRefs(content));

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
  nsString value;

  nsCOMPtr<nsIContent> content;
  mInner->GetContentOf(this, getter_AddRefs(content));

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

  nsString value;

  nsCOMPtr<nsIContent> content;
  mInner->GetContentOf(this, getter_AddRefs(content));

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
  nsString value;

  nsCOMPtr<nsIContent> content;
  mInner->GetContentOf(this, getter_AddRefs(content));

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
  nsString value;

  nsCOMPtr<nsIContent> content;
  mInner->GetContentOf(this, getter_AddRefs(content));

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
        //aSize.prefWidthIntrinsic = PR_FALSE;
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.prefSize.height = position->mHeight.GetCoordValue();     
        //aSize.prefHeightIntrinsic = PR_FALSE;
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
    aFrame->GetContent(getter_AddRefs(content));

    if (content) {
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

    // make sure pref size is inside min and max values.
    if (aSize.prefSize.width < aSize.minSize.width)
        aSize.prefSize.width = aSize.minSize.width;

    if (aSize.prefSize.height < aSize.minSize.height)
        aSize.prefSize.height = aSize.minSize.height;

    if (aSize.prefSize.width > aSize.maxSize.width)
        aSize.prefSize.width = aSize.maxSize.width;

    if (aSize.prefSize.height > aSize.maxSize.height)
        aSize.prefSize.height = aSize.maxSize.height;


    // if we are not flexible then our min, max, and pref sizes are all the same.
  //  if (aSize.flex == 0) {
  //      SET_WIDTH(aSize.minSize, GET_WIDTH(aSize.prefSize));
   ///     SET_WIDTH(aSize.maxSize, GET_WIDTH(aSize.prefSize));
   /// }

}

/**
 * Given a frame gets its box info. If it does not have a box info then it will merely
 * get the normally defined min, pref, max stuff.
 *
 */
nsresult
nsBoxFrame::GetChildBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame* aFrame, nsCalculatedBoxInfo& aSize)
{
#ifdef DEBUG_REFLOW
    gIndent++;
#endif

  nsIFrame* incrementalChild;
  if (aReflowState.reason == eReflowReason_Incremental) {
      // then get the child we need to flow incrementally
      aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
  }

  aSize.Clear();

  // see if the frame Innerements IBox interface
  
  // since frames are not refCounted, don't use nsCOMPtr with them
  // if it does ask it for its BoxSize and we are done
  nsIBox* ibox = nsnull;
  if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) {
      if (aReflowState.reason == eReflowReason_Incremental) {
          // then get the child we need to flow incrementally
          if (incrementalChild == aFrame) {
              aReflowState.reflowCommand->GetNext(incrementalChild);
              // dirty the incremental child
                nsFrameState childState;
                aFrame->GetFrameState(&childState);
                childState |= NS_FRAME_IS_DIRTY;
                aFrame->SetFrameState(childState);
          }
      }
     ibox->GetBoxInfo(aPresContext, aReflowState, aSize); 
     aSize.mFlags |= NS_FRAME_IS_BOX;
     // add in the border, padding, width, min, max
     GetRedefinedMinPrefMax(aPresContext, aFrame, aSize);

#ifdef DEBUG_REFLOW
     gIndent--;
#endif
     return NS_OK;
  }   

  // so the child is not a box. So we will have to be creative. We need to get its min, max, and preferred
  // sizes. Min and Max are easy you get them for CSS. But preferred size? First we will see if they are set in 
  // css. If not we will have to flow the child intrinically.

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

        nsHTMLReflowState state(aReflowState);
        state.availableWidth = NS_INTRINSICSIZE;
        state.availableHeight = NS_INTRINSICSIZE;
        //state.reason = eReflowReason_Resize;

        nsIFrame* incrementalChild = nsnull;

        mInner->FlowChildAt(aFrame, aPresContext, desiredSize, state, status, aSize, 0, 0, PR_FALSE, incrementalChild, redraw, reason);

        // remove margin and border
        desiredSize.height -= (total.top + total.bottom);
        desiredSize.width -= (total.left + total.right);

        // get the size returned and the it as the preferredsize.
        aSize.prefSize.width = desiredSize.width;
        aSize.prefSize.height = desiredSize.height;
        aSize.ascent = desiredSize.ascent;
    
#ifdef DEBUG_REFLOW
           mInner->AddIndents();
           printf("pref-size: (%d,%d)\n", aSize.prefSize.width, aSize.prefSize.height);
#endif

    //    aSize.prefSize.width = 0;
      //  aSize.prefSize.height = 0;
      
    // redefine anything depending on css
    GetRedefinedMinPrefMax(aPresContext, aFrame, aSize);
  }

#ifdef DEBUG_REFLOW
    gIndent--;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
    InvalidateCache(aChild);

    // if we are not dirty mark ourselves dirty and tell our parent we are dirty too.
    if (!(mState & NS_FRAME_IS_DIRTY)) {      
      // Mark yourself as dirty
      mState |= NS_FRAME_IS_DIRTY;
      return mParent->ReflowDirtyChild(aPresShell, this);
    }

    return NS_OK;
}


/*
void
nsIBox::HandleRootBoxReflow(nsIPresContext* aPresContext, 
                            nsIFrame* aBox, 
                            const nsHTMLReflowState& aReflowState) 
{
 // if its an incremental reflow
  if ( aReflowState.reason == eReflowReason_Incremental ) {

    nsIReflowCommand::ReflowType  reflowType;
    aReflowState.reflowCommand->GetType(reflowType);

    // See if it's targeted at us
    nsIFrame* targetFrame;    
    aReflowState.reflowCommand->GetTarget(targetFrame);

    if (aBox != targetFrame) {
           // clear the caches down the chain
            nsBoxFrameInner::InvalidateCachesTo(targetFrame);

           // if the type is style changed then make sure we 
           // invalidate all caches under the target
           if (reflowType == nsIReflowCommand::StyleChanged)
               nsBoxFrameInner::InvalidateAllCachesBelow(aPresContext, targetFrame);
     }
  } 
}
*/


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

#ifdef DEBUG_REFLOW
    gIndent++;
#endif


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
    nsIFrame* incrementalChild = nsnull;

    // update our debug state if it needs it.
    // see if the debug was set. If it was then propagate it
    // if it wasn't and we are root then get it from the global pref.
    if (mState & NS_STATE_DEBUG_WAS_SET) {
        if (mState & NS_STATE_SET_TO_DEBUG)
            SetDebug(aPresContext, PR_TRUE);
        else
            SetDebug(aPresContext, PR_FALSE);
    } else if (mState & NS_STATE_IS_ROOT) {
        SetDebug(aPresContext, nsBoxDebug::gDebug);
    }

    if ( aReflowState.reason == eReflowReason_Incremental ) {
        nsIReflowCommand::ReflowType  reflowType;
        aReflowState.reflowCommand->GetType(reflowType);

        // See if it's targeted at us
        nsIFrame* targetFrame;    
        aReflowState.reflowCommand->GetTarget(targetFrame);

        if (this == targetFrame) {
          // if we are the target see what the type was
          // and generate a normal non incremental reflow.
          switch (reflowType) {

              case nsIReflowCommand::StyleChanged: 
              {
                nsHTMLReflowState newState(aReflowState);
                newState.reason = eReflowReason_StyleChange;
                newState.reflowCommand = nsnull;
                #ifdef DEBUG_REFLOW
                  gIndent--;
                #endif
                return Reflow(aPresContext, aDesiredSize, newState, aStatus);
              }
              break;

              // if its a dirty type then reflow us with a dirty reflow
              case nsIReflowCommand::ReflowDirty: 
              {
                nsHTMLReflowState newState(aReflowState);
                newState.reason = eReflowReason_Dirty;
                newState.reflowCommand = nsnull;
                #ifdef DEBUG_REFLOW
                  gIndent--;
                #endif
                return Reflow(aPresContext, aDesiredSize, newState, aStatus);
              }
              break;

              default:
                NS_ASSERTION(PR_FALSE, "unexpected reflow command type");
          } 
          
        } else {
            /*
            // ok we are not the target, see if our parent is a box. If its not then we 
            // are the first box so its our responsibility
            // to blow away the caches for each child in the incremental 
            // reflow chain. only mark those children whose parents are boxes
            if (mState & NS_STATE_IS_ROOT) {
               HandleRootBoxReflow(aPresContext, this, aReflowState);
            }            
        */

        }

        // then get the child we need to flow incrementally
        //aReflowState.reflowCommand->GetNext(incrementalChild);
    }   


#ifdef DEBUG_REFLOW
    if (mState & NS_STATE_IS_ROOT)
       printf("--------REFLOW %d--------\n", gReflows++);
#endif




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

  //nsMargin debugInset(0,0,0,0);
  //mInner->GetDebugInset(debugInset);
  //inset += debugInset;

  rect.Deflate(inset);

  //-----------------------------------------------------------------------------------
  //------------------------- figure our our children's sizes  -------------------------
  //-----------------------------------------------------------------------------------

  // now that we know our child's min, max, pref sizes. Stretch our children out to fit into our size.
  // this will calculate each of our childs sizes.
  InvalidateChildren();

  nscoord maxAscent;

  LayoutChildrenInRect(rect, maxAscent);


  //-----------------------------------------------------------------------------------
  //------------------------- flow all the children -----------------------------------
  //-----------------------------------------------------------------------------------

    // get the layout rect.
  nsRect layoutRect = rect;

  // set up our actual size
  layoutRect.Inflate(inset);

  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE && layoutRect.width < aReflowState.mComputedWidth)
    layoutRect.width = aReflowState.mComputedWidth;
 
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE && layoutRect.height < aReflowState.mComputedHeight)
    layoutRect.height = aReflowState.mComputedHeight;

  nsRect actualRect(layoutRect);
  actualRect.Deflate(inset);

  nsSize actualSize(actualRect.width, actualRect.height);

  // flow each child at the new sizes we have calculated. With the actual size we are giving them to layout our and the
  // size of the rect they were computed to layout in.
  mInner->FlowChildren(aPresContext, aDesiredSize, aReflowState, aStatus, incrementalChild, rect, actualSize, maxAscent);

  //-----------------------------------------------------------------------------------
  //------------------------- Add our border and insets in ----------------------------
  //-----------------------------------------------------------------------------------

  
  // flow children may have changed the rect so lets use it.
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
  
#if 0
ListTag(stdout); printf(": reflow done\n");
#endif

  mInner->SanityCheck();

  
#ifdef DEBUG_REFLOW
  gIndent--;
#endif

  return NS_OK;
}

void
nsBoxFrameInner::InvalidateCachesTo(nsIFrame* aTargetFrame)
{
    // walk up the tree from the target frame to the root invalidating each childs cached layout infomation
    
    nsIFrame* parent;
    aTargetFrame->GetParent(&parent);
    if (parent == nsnull)
       return;


    InvalidateCachesTo(parent);

    // only mark those children whose parents are boxes
    nsIBox* ibox = nsnull;
    if (NS_SUCCEEDED(parent->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox) {
       ibox->InvalidateCache(aTargetFrame);
    }
}


void
nsBoxFrameInner::InvalidateAllCachesBelow(nsIPresContext* aPresContext, nsIFrame* aTargetFrame)
{
    // walk the whole tree under the target frame and clear each childs cached layout information
    // but make sure we only mark children whose parent is a box.
    nsIBox* ibox = nsnull;
    if (NS_FAILED(aTargetFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox))) {
        return;
    }

    ibox->InvalidateCache(nsnull);

    nsIFrame* child = nsnull;
    aTargetFrame->FirstChild(aPresContext, nsnull, &child);

    while (nsnull != child) 
    {
         InvalidateAllCachesBelow(aPresContext, child);
         nsresult rv = child->GetNextSibling(&child);
         NS_ASSERTION(rv == NS_OK,"failed to get next child");
    }   
}

void
nsBoxFrame::ComputeChildsNextPosition(nsCalculatedBoxInfo* aInfo, nscoord& aCurX, nscoord& aCurY, nscoord& aNextX, nscoord& aNextY, const nsSize& aCurrentChildSize, const nsRect& aBoxRect, nscoord aMaxAscent)
{
    if (mState & NS_STATE_IS_HORIZONTAL) {
        aNextX = aCurX + aCurrentChildSize.width;

        if (mState & NS_STATE_AUTO_STRETCH) {
               aCurY = aBoxRect.y;
        } else {
            switch (mInner->mValign) 
            {
               case nsBoxFrame::vAlign_BaseLine:
                   aCurY = aBoxRect.y + (aMaxAscent - aInfo->ascent);
               break;

               case nsBoxFrame::vAlign_Top:
                   aCurY = aBoxRect.y;
                   break;
               case nsBoxFrame::vAlign_Middle:
                   aCurY = aBoxRect.y + (aBoxRect.height/2 - aCurrentChildSize.height/2);
                   break;
               case nsBoxFrame::vAlign_Bottom:
                   aCurY = aBoxRect.y + aBoxRect.height - aCurrentChildSize.height;
                   break;
            }
        }
    } else {
        aNextY = aCurY + aCurrentChildSize.height;
        if (mState & NS_STATE_AUTO_STRETCH) {
                   aCurX = aBoxRect.x;
        } else {
            switch (mInner->mHalign) 
            {
               case nsBoxFrame::hAlign_Left:
                   aCurX = aBoxRect.x;
                   break;
               case nsBoxFrame::hAlign_Center:
                   aCurX = aBoxRect.x + (aBoxRect.width/2 - aCurrentChildSize.width/2);
                   break;
               case nsBoxFrame::hAlign_Right:
                   aCurX = aBoxRect.x + aBoxRect.width - aCurrentChildSize.width;
                   break;
            }
        }
    }
}

/**
 * When all the childrens positions have been calculated and layed out. Flow each child
 * at its not size.
 */
nsresult
nsBoxFrameInner::FlowChildren(nsIPresContext* aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsIFrame*&               aIncrementalChild,
                     nsRect& aRect,
                     nsSize& aActualSize,
                     nscoord& aMaxAscent)
{

      // ------- set the childs positions ---------

  PRBool redraw = PR_FALSE;

  PRBool finished;
  nscoord passes = 0;
  nscoord changedIndex = -1;
  nsAutoString reason;
  nsAutoString nextReason;
  PRInt32 infoCount = mInfoList->GetCount();
  PRBool* resized = nsnull;
 
  // ----------------------
  // Flow all children 
  // ----------------------

  // ok what we want to do if flow each child at the location given in the spring.
  // unfortunately after flowing a child it might get bigger. We have not control over this
  // so it the child gets bigger or smaller than we expected we will have to do a 2nd, 3rd, 4th pass to 
  // adjust. When we adjust make sure the new reason is resize.

  changedIndex = -1;

  passes = 0;
  do 
  { 
    #ifdef DEBUG_REFLOW
      if (passes > 0) {
           AddIndents();
           printf("ChildResized doing pass: %d\n", passes);
      }
    #endif 

    finished = PR_TRUE;
    nscoord count = 0;
    nsCalculatedBoxInfo* info = mInfoList->GetFirst();

    nscoord x = aRect.x;
    nscoord y = aRect.y;

    if (!(mOuter->mState & NS_STATE_AUTO_STRETCH)) {
        if (mOuter->mState & NS_STATE_IS_HORIZONTAL) {
           switch(mHalign) {
              case nsBoxFrame::hAlign_Left:
                 x = aRect.x;
              break;

              case nsBoxFrame::hAlign_Center:
                 x = aRect.x + (aActualSize.width - aRect.width)/2;
              break;

              case nsBoxFrame::hAlign_Right:
                 x = aRect.x + (aActualSize.width - aRect.width);
              break;
           }
        } else {
            switch(mValign) {
              case nsBoxFrame::vAlign_Top:
              case nsBoxFrame::vAlign_BaseLine:
                 y = aRect.y;
              break;

              case nsBoxFrame::vAlign_Middle:
                 y = aRect.y + (aActualSize.height - aRect.height)/2;
              break;

              case nsBoxFrame::vAlign_Bottom:
                 y = aRect.y + (aActualSize.height - aRect.height);
              break;
            }
        }
    }

    while (nsnull != info) 
    {    

       nsIFrame* childFrame = info->frame;

       NS_ASSERTION(info, "ERROR not info object!!");

        // if we reached the index that changed we are done.
       //if (count == changedIndex)
       //    break;

       // make collapsed children not show up
        if (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED) {
          
          if (aReflowState.reason == eReflowReason_Initial)
	      {
                FlowChildAt(childFrame, aPresContext, aDesiredSize, aReflowState, aStatus, *info, x, y, PR_TRUE, aIncrementalChild, redraw, reason);

                // if the child got bigger then adjust our rect and all the children.
                mOuter->ChildResized(childFrame, aDesiredSize, aRect, aMaxAscent, *info, resized, infoCount, changedIndex, finished, count, nextReason);
	      }

          // ok if we are collapsed make sure the view and all its children
          // are collapsed as well.

           nsRect rect(0,0,0,0);
           childFrame->GetRect(rect);
           if (rect.width > 0 || rect.height > 0) 
           {
              childFrame->SizeTo(aPresContext, 0,0);       
              mOuter->CollapseChild(aPresContext, childFrame, PR_TRUE);
           } 

          /*
          nsRect rect(0,0,0,0);
          childFrame->GetRect(rect);
          if (rect.width > 0 || rect.height > 0) {
              nsIView* view;
              childFrame->GetView(aPresContext, &view);
             if (view) {
                nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, childFrame,
                                                           view, nsnull);
              } else {
                // Re-position any child frame views
                nsContainerFrame::PositionChildViews(aPresContext, childFrame);
              }
          }
          */

        } else {

          nscoord nextX = x;
          nscoord nextY = y;

          mOuter->ComputeChildsNextPosition(info, x, y, nextX, nextY, info->calculatedSize, aRect, aMaxAscent);

          // reflow if the child needs it or we are on a second pass
          FlowChildAt(childFrame, aPresContext, aDesiredSize, aReflowState, aStatus, *info, x, y, PR_TRUE, aIncrementalChild, redraw, reason);
 
          // if the child got bigger then adjust our rect and all the children.
          mOuter->ChildResized(childFrame, aDesiredSize, aRect, aMaxAscent, *info, resized, infoCount, changedIndex, finished, count, nextReason);

          /*
#ifdef DEBUG_REFLOW

          if (!finished) {
              AddIndents();
              printf("**** Child ");
              nsFrame::ListTag(stdout, childFrame);
              printf(" resized!******\n");
              AddIndents();
              char ch[100];
              nextReason.ToCString(ch,100);
              printf("because (%s)\n", ch);
          }
#endif
          */

          // always do 1 pass for now.
          //finished = PR_TRUE;
        
          // if the child resized then recompute it position.
          if (!finished) {
              mOuter->ComputeChildsNextPosition(info, x, y, nextX, nextY, nsSize(aDesiredSize.width, aDesiredSize.height), aRect, aMaxAscent);
          }

          x = nextX;
          y = nextY;

        }

        info = info->next;
        count++;
    }

    // ok if something go bigger and we need to do another pass then
    // the second pass will be reflow resize
    reason = nextReason;

    // if we get over 10 passes something probably when wrong.
    passes++;
    if (passes > 5) {
//      mOuter->NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
  //               ("bug"));
    }

    //NS_ASSERTION(passes <= 10,"Error infinte loop too many passes");
    if (passes > 10) {
       break;
    }


  } while (PR_FALSE == finished);


  // if the rect inside us changes size. Mainly if it gets smaller redraw.
  // this will make use draw when children have removed.
  nscoord newInnerSize;
  if (mOuter->mState & NS_STATE_IS_HORIZONTAL)
      newInnerSize = aRect.width;
  else
      newInnerSize = aRect.height;

  if (mInnerSize != newInnerSize)
  {
      mInnerSize = newInnerSize;
      redraw = PR_TRUE;
  }
      
      // redraw things if needed.
  if (redraw) {
#ifdef DEBUG_REDRAW
      mInner->ListTag(stdout);
      printf("is being redrawn\n");
#endif
    mOuter->Invalidate(aPresContext, nsRect(0,0,mOuter->mRect.width, mOuter->mRect.height), PR_FALSE);
  }

  if (resized)
     delete[] resized;

  return NS_OK;
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

#ifdef DEBUG_REFLOW
void
nsBoxFrameInner::MakeReason(nsIFrame* aFrame, const nsString& aText, nsString& aReason)
{
    nsAutoString type;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(aFrame->QueryInterface(nsIFrameDebug::GetIID(), (void**)&frameDebug))) {
      frameDebug->GetFrameName(type);
    }

    char address[100];
    sprintf(address, "@%p", aFrame);

    aReason = "(";
    aReason += type;
    aReason += address;
    aReason += " ";
    aReason += aText;
    aReason += ")";
}
#endif

void
nsBoxFrameInner::CreateResizedArray(PRBool*& aResized, PRInt32 aCount)
{
  aResized = new PRBool[aCount];

  for (int i=0; i < aCount; i++) 
      aResized[i] = PR_FALSE;
}

void
nsBoxFrame::ChildResized(nsIFrame* aFrame, nsHTMLReflowMetrics& aDesiredSize, nsRect& aRect, nscoord& aMaxAscent, nsCalculatedBoxInfo& aInfo, PRBool*& aResized, PRInt32 aInfoCount, nscoord& aChangedIndex, PRBool& aFinished, nscoord aIndex, nsString& aReason)
{
  if (mState & NS_STATE_IS_HORIZONTAL) {
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
            LayoutChildrenInRect(aRect, aMaxAscent);
#ifdef DEBUG_REFLOW
            mInner->MakeReason(aFrame, "height got bigger", aReason);
#endif 

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

            if (aResized == nsnull)
                mInner->CreateResizedArray(aResized, aInfoCount); 

            // our index resized
            aResized[aIndex] = PR_TRUE;

            // if the width changed. mark our child as being resized
            nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
            PRInt32 infoCount = 0;
            while(info) {
              if (aResized[infoCount])
                 info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
              else
                 info->mFlags &= ~NS_FRAME_BOX_SIZE_VALID;


              info = info->next;
              infoCount++;
            }

            LayoutChildrenInRect(aRect, aMaxAscent);
            aFinished = PR_FALSE;
            aChangedIndex = aIndex;
#ifdef DEBUG_REFLOW
            mInner->MakeReason(aFrame, "width got bigger", aReason);
#endif 

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
            if (aResized == nsnull)
                mInner->CreateResizedArray(aResized, aInfoCount); 

            PRInt32 infoCount = mInner->mInfoList->GetCount();
            for (int i=0; i < infoCount; i++)
               aResized[i] = PR_FALSE;

            aFinished = PR_FALSE;
            aChangedIndex = aIndex;

            // relayout everything
            InvalidateChildren();
            LayoutChildrenInRect(aRect, aMaxAscent);
#ifdef DEBUG_REFLOW
            mInner->MakeReason(aFrame, "width got bigger", aReason);
#endif 
            
      } else if (aDesiredSize.height > aInfo.calculatedSize.height) {
            // our width now becomes the new size
            aInfo.calculatedSize.height = aDesiredSize.height;

            InvalidateChildren();

            if (aResized == nsnull)
                mInner->CreateResizedArray(aResized, aInfoCount); 

            // our index resized
            aResized[aIndex] = PR_TRUE;

            // if the width changed. mark our child as being resized
            nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
            PRInt32 infoCount = 0;
            while(info) {
              if (aResized[infoCount])
                 info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
              else
                 info->mFlags &= ~NS_FRAME_BOX_SIZE_VALID;

              info = info->next;
              infoCount++;
            }

            LayoutChildrenInRect(aRect, aMaxAscent);
            aFinished = PR_FALSE;
            aChangedIndex = aIndex;
#ifdef DEBUG_REFLOW
            mInner->MakeReason(aFrame, "height got bigger", aReason);
#endif 
            
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

      // Trees have to collapse their scrollbars manually, since you can't
      // get to the scrollbar via the normal frame list.
      nsISelfScrollingFrame* treeFrame;
      if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsISelfScrollingFrame), (void**)&treeFrame)) && treeFrame) {
        // Tell the tree frame to collapse its scrollbar.
        treeFrame->CollapseScrollbar(aPresContext, hide);
      }
      
      // collapse the child
      nsIFrame* child = nsnull;
      frame->FirstChild(aPresContext, nsnull, &child);

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

  /*
  nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

  while (info) 
  {
    // make collapsed children not show up
    if (info->collapsed) {
       nsRect rect(0,0,0,0);
       info->frame->GetRect(rect);
       if (rect.width == 0 && rect.height == 0)
           return;

        CollapseChild(aPresContext, info->frame, PR_TRUE);
    } 

    info = info->next;
  }
  */
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
    if (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED) {
       nsRect rect(0,0,0,0);
       childFrame->GetRect(rect);
       if (rect.width > 0 || rect.height > 0) {
          childFrame->SizeTo(aPresContext, 0,0);
          CollapseChild(aPresContext, childFrame, PR_TRUE);
       }
    } else {
      const nsStyleSpacing* spacing;
      rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) spacing);

      nsMargin margin(0,0,0,0);
      spacing->GetMargin(margin);

      if (mState & NS_STATE_IS_HORIZONTAL) {
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
      if (mState & NS_STATE_IS_HORIZONTAL)
        x += margin.right;
      else
        y += margin.bottom;
     
      if (mState & NS_STATE_IS_HORIZONTAL) {
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

void
nsBoxFrameInner::PlaceChild(nsIPresContext* aPresContext, nsIFrame* aFrame, nscoord aX, nscoord aY)
{
      nsPoint curOrigin;
      aFrame->GetOrigin(curOrigin);

      // only if the origin changed
    if ((curOrigin.x != aX) || (curOrigin.y != aY)) {
        aFrame->MoveTo(aPresContext, aX, aY);

        nsIView*  view;
        aFrame->GetView(aPresContext, &view);
        if (view) {
            nsContainerFrame::PositionFrameView(aPresContext, aFrame, view);
        } else
            nsContainerFrame::PositionChildViews(aPresContext, aFrame);
    }
}


void
nsBoxFrameInner::InitializeReflowState(nsIPresContext* aPresContext, const nsCalculatedBoxInfo& aInfo, nsHTMLReflowState& aReflowState, const nsHTMLReflowState& aParentReflowState, nsIFrame* aFrame, const nsSize& aAvailSize, nsReflowReason aReason)
{
      aFrame->GetStyleData(eStyleStruct_Position,
                          (const nsStyleStruct*&)aReflowState.mStylePosition);
      aFrame->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&)aReflowState.mStyleDisplay);
      aFrame->GetStyleData(eStyleStruct_Spacing,
                          (const nsStyleStruct*&)aReflowState.mStyleSpacing);

        aReflowState.mReflowDepth = aParentReflowState.mReflowDepth + 1;
        aReflowState.parentReflowState = &aParentReflowState;
        aReflowState.frame = aFrame;
        aReflowState.reason = aReason;
        aReflowState.reflowCommand = aReflowState.reflowCommand;
        aReflowState.availableWidth = aAvailSize.width;
        aReflowState.availableHeight = aAvailSize.height;

        aReflowState.rendContext = aParentReflowState.rendContext;
        aReflowState.mSpaceManager = aParentReflowState.mSpaceManager;
        aReflowState.mLineLayout = aParentReflowState.mLineLayout;
        aReflowState.isTopOfPage = aParentReflowState.isTopOfPage;

        // if we are not a box then to the regular html thing.
        // otherwise do the efficient thing for boxes.
       // if (!(aInfo.mFlags & NS_FRAME_IS_BOX)) {
       //     aReflowState.Init(aPresContext);
       /// } else {
            aReflowState.mComputedMargin.SizeTo(0,0,0,0);
            aReflowState.mStyleSpacing->GetMargin(aReflowState.mComputedMargin);
            aReflowState.mComputedBorderPadding.SizeTo(0,0,0,0);
            aReflowState.mStyleSpacing->GetBorderPadding(aReflowState.mComputedBorderPadding);
            aReflowState.mComputedPadding.SizeTo(0,0,0,0);
            aReflowState.mStyleSpacing->GetPadding(aReflowState.mComputedPadding);
            aReflowState.mComputedOffsets.SizeTo(0, 0, 0, 0);
            aReflowState.mComputedMinWidth = aReflowState.mComputedMinHeight = 0;
            aReflowState.mComputedMaxWidth = aReflowState.mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;
            aReflowState.mCompactMarginWidth = 0;
            aReflowState.mAlignCharOffset = 0;
            aReflowState.mUseAlignCharOffset = 0;
            aReflowState.mFrameType = NS_CSS_FRAME_TYPE_BLOCK;
        //}
}

/**
 * Flow an individual child. Special args:
 * count: the spring that will be used to lay out the child
 * incrementalChild: If incremental reflow this is the child that need to be reflowed.
 *                   when we finally do reflow the child we will set the child to null
 */

nsresult
nsBoxFrameInner::FlowChildAt(nsIFrame* childFrame, 
                             nsIPresContext* aPresContext,
                             nsHTMLReflowMetrics&     desiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus,
                             nsCalculatedBoxInfo&     aInfo,
                             nscoord                  aX,
                             nscoord                  aY,
                             PRBool                   aMoveFrame,
                             nsIFrame*&               aIncrementalChild,
                             PRBool& aRedraw,
                             const nsString& aReason)
{
   //   if (mOuter->mState & NS_STATE_CURRENTLY_IN_DEBUG)
    //      printf("in debug\n");

      aStatus = NS_FRAME_COMPLETE;
      PRBool needsReflow = PR_FALSE;
      nsReflowReason reason = aReflowState.reason;

      nsFrameState childState;
      aInfo.frame->GetFrameState(&childState);

      if (childState & NS_FRAME_FIRST_REFLOW) {
          if (reason != eReflowReason_Initial)
          {
              // if incremental then make sure we send a initial reflow first.
              if (reason == eReflowReason_Incremental) {
                  nsHTMLReflowState state(aReflowState);
                  state.reason = eReflowReason_Initial;
                  state.reflowCommand = nsnull;
                  FlowChildAt(childFrame, aPresContext, desiredSize, state, aStatus, aInfo, aX, aY, aMoveFrame, aIncrementalChild, aRedraw, aReason);
              } else {
                  // convert to initial if not incremental.
                  reason = eReflowReason_Initial;
              }

          }
      } else if (reason == eReflowReason_Initial) {
          reason = eReflowReason_Resize;
      }
       
      switch(reason)
      {
         // if the child we are reflowing is the child we popped off the incremental 
         // reflow chain then we need to reflow it no matter what.
         // if its not the child we got from the reflow chain then this child needs reflow
         // because as a side effect of the incremental child changing aize andit needs to be resized.
         // This will happen with a lot when a box that contains 2 children with different flexabilities
         // if on child gets bigger the other is affected because it is proprotional to the first.
         // so it might need to be resized. But we don't need to reflow it. If it is already the
         // needed size then we will do nothing. 
         case eReflowReason_Incremental: {

            // if incremental see if the next child in the chain is the child. If so then
            // we will just let it go down. If not then convert it to a dirty. It will get converted back when 
            // needed in the case just below this one.
            nsIFrame* incrementalChild = nsnull;
            aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
            if (incrementalChild == aInfo.frame) {
                needsReflow = PR_TRUE;
                aReflowState.reflowCommand->GetNext(incrementalChild);
            } else {
                // create a new state. Make sure we don't null out the
                // reflow command. This is how we will tell it was a converted incremental.
                // We will use this later to convert back to a incremental.
                nsHTMLReflowState state(aReflowState);
                state.reason = eReflowReason_Dirty;
                return FlowChildAt(childFrame, aPresContext, desiredSize, state, aStatus, aInfo, aX, aY, aMoveFrame, aIncrementalChild, aRedraw, aReason);
            }          
         } break;
         
         // if its dirty then see if the child we want to reflow is dirty. If it is then
         // mark it as needing to be reflowed.
         case eReflowReason_Dirty: {

             // sometimes incrementals are converted to dirty. This is done in the case just above this. So lets check
             // to see if this was converted. If it was it will still have a reflow state.
             if (aReflowState.reflowCommand) {
                // it was converted so lets see if the next child is this one. If it is then convert it back and
                // pass it down.
                nsIFrame* incrementalChild = nsnull;
                aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
                if (incrementalChild == aInfo.frame) {
                     nsHTMLReflowState state(aReflowState);
                     state.reason = eReflowReason_Incremental;
                     return FlowChildAt(childFrame, aPresContext, desiredSize, state, aStatus, aInfo, aX, aY, aMoveFrame, aIncrementalChild, aRedraw, aReason);
                } 
              }

              // get the frame state to see if it needs reflow
              needsReflow = (childState & NS_FRAME_IS_DIRTY) || (childState & NS_FRAME_HAS_DIRTY_CHILDREN);

         } break;

         // if the a resize reflow then it doesn't need to be reflowed. Only if the size is different
         // from the new size would we actually do a reflow
         case eReflowReason_Resize:
             needsReflow = PR_FALSE;
         break;

         // if its an initial reflow we must place the child.
         // otherwise we might think it was already placed when it wasn't
         case eReflowReason_Initial:
             aMoveFrame = PR_TRUE;
             needsReflow = PR_TRUE;
         break;

         default:
             needsReflow = PR_TRUE;
             
      }

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
      if (!needsReflow) {
          
          if (aInfo.calculatedSize.width != NS_INTRINSICSIZE && aInfo.calculatedSize.height != NS_INTRINSICSIZE) {
          
              // if the new calculated size has a 0 width or a 0 height
              if ((currentRect.width == 0 || currentRect.height == 0) && (aInfo.calculatedSize.width == 0 || aInfo.calculatedSize.height == 0)) {
                   needsReflow = PR_FALSE;
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
                      needsReflow = PR_FALSE;
                else
                      needsReflow = PR_TRUE;
       
              }
          } else {
              // if the width or height are intrinsic alway reflow because
              // we don't know what it should be.
             needsReflow = PR_TRUE;
          }
      }
                             
      // ok now reflow the child into the springs calculated space
      if (needsReflow) {

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

 #ifdef DEBUG_REFLOW

   char* reflowReasonString;

    switch(reason) 
    {
        case eReflowReason_Initial:
          reflowReasonString = "initial";
          break;

        case eReflowReason_Resize:
          reflowReasonString = "resize";
          break;
        case eReflowReason_Dirty:
          reflowReasonString = "dirty";
          break;
        case eReflowReason_StyleChange:
          reflowReasonString = "stylechange";
          break;
        case eReflowReason_Incremental:
          reflowReasonString = "incremental";
          break;
        default:
          reflowReasonString = "unknown";
          break;
    }

    AddIndents();
    nsFrame::ListTag(stdout, childFrame);
    char ch[100];
    aReason.ToCString(ch,100);

    printf(" reason=%s %s",reflowReasonString,ch);
#endif

    // only frames that implement nsIBox seem to be able to handle a reason of Dirty. For everyone else
    // send down a resize.
    if (!(aInfo.mFlags & NS_FRAME_IS_BOX) && reason == eReflowReason_Dirty)
        reason = eReflowReason_Resize;

        if (!(aInfo.mFlags & NS_FRAME_IS_BOX)) {
            nsHTMLReflowState   reflowState(aPresContext, aReflowState, childFrame, nsSize(size.width, NS_INTRINSICSIZE));
            reflowState.reason = reason;

            if (size.height != NS_INTRINSICSIZE)
                size.height -= (border.top + border.bottom);

            if (size.width != NS_INTRINSICSIZE)
                size.width -= (border.left + border.right);

            reflowState.mComputedWidth = size.width;
            reflowState.mComputedHeight = size.height;
#ifdef DEBUG_REFLOW
    printf(" Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
#endif

          // place the child and reflow
            childFrame->WillReflow(aPresContext);

            if (aMoveFrame) {
                  PlaceChild(aPresContext, childFrame, aX + margin.left, aY + margin.top);
            }

            childFrame->Reflow(aPresContext, desiredSize, reflowState, aStatus);
        } else {
            nsHTMLReflowState reflowState(aReflowState);
            InitializeReflowState(aPresContext, aInfo, reflowState, aReflowState, childFrame, nsSize(size.width, NS_INTRINSICSIZE), reason);

            if (size.height != NS_INTRINSICSIZE)
                size.height -= (border.top + border.bottom);

            if (size.width != NS_INTRINSICSIZE)
                size.width -= (border.left + border.right);

            reflowState.mComputedWidth = size.width;
            reflowState.mComputedHeight = size.height;

#ifdef DEBUG_REFLOW
    printf(" Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
#endif

          // place the child and reflow
            childFrame->WillReflow(aPresContext);

            if (aMoveFrame) {
                  PlaceChild(aPresContext, childFrame, aX + margin.left, aY + margin.top);
            }

            childFrame->Reflow(aPresContext, desiredSize, reflowState, aStatus);
        }

        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

        nsFrameState  kidState;
        childFrame->GetFrameState(&kidState);

       // printf("width: %d, height: %d\n", desiredSize.mCombinedArea.width, desiredSize.mCombinedArea.height);

        // XXX EDV hack to fix bug in block
        if (reason != eReflowReason_Initial) {
            if (kidState & NS_FRAME_OUTSIDE_CHILDREN) {
                 desiredSize.width = desiredSize.mOverflowArea.width;
                 desiredSize.height = desiredSize.mOverflowArea.height;
            }
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
 
        nsContainerFrame::FinishReflowChild(childFrame, aPresContext, desiredSize, aX + margin.left, aY + margin.top, NS_FRAME_NO_MOVE_FRAME);

        // Stub out desiredSize.maxElementSize so that when go out of
        // scope, nothing bad happens!
        //desiredSize.maxElementSize = nsnull;
      
      } else {
          if (aMoveFrame) {
               PlaceChild(aPresContext, childFrame, aX + margin.left, aY + margin.top);
          }
      }    
      

      // add the margin back in. The child should add its border automatically
      desiredSize.height += (margin.top + margin.bottom);
      desiredSize.width += (margin.left + margin.right);

#ifdef DEBUG_REFLOW
      if (aInfo.calculatedSize.height != NS_INTRINSICSIZE && desiredSize.height != aInfo.calculatedSize.height)
      {
              AddIndents();
              printf("**** Child ");
              nsFrame::ListTag(stdout, childFrame);
              printf(" got taller!******\n");
             
      }
      if (aInfo.calculatedSize.width != NS_INTRINSICSIZE && desiredSize.width != aInfo.calculatedSize.width)
      {
              AddIndents();
              printf("**** Child ");
              nsFrame::ListTag(stdout, childFrame);
              printf(" got wider!******\n");
             
      }
#endif

      if (aInfo.calculatedSize.width == NS_INTRINSICSIZE)
         aInfo.calculatedSize.width = desiredSize.width;

      if (aInfo.calculatedSize.height == NS_INTRINSICSIZE)
         aInfo.calculatedSize.height = desiredSize.height;

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
  const nscoord& awidth  = (mState & NS_STATE_IS_HORIZONTAL) ? a.width  : a.height;
  const nscoord& aheight = (mState & NS_STATE_IS_HORIZONTAL) ? a.height : a.width;
  nscoord& bwidth  = (mState & NS_STATE_IS_HORIZONTAL) ? b.width  : b.height;
  nscoord& bheight = (mState & NS_STATE_IS_HORIZONTAL) ? b.height : b.width;

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
    mInner->GetDebugInset(margin);
}


void
nsBoxFrame::InvalidateChildren()
{
    nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
    while(info) {
        info->mFlags &= ~NS_FRAME_BOX_SIZE_VALID;
        info = info->next;
    }
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


void
nsBoxFrame::LayoutChildrenInRect(nsRect& aGivenSize, nscoord& aMaxAscent)
{  
      nsCalculatedBoxInfo* first = mInner->mInfoList->GetFirst();

      if (!first)
          return;

      PRInt32 sizeRemaining;
       
      if (mState & NS_STATE_IS_HORIZONTAL)
          sizeRemaining = aGivenSize.width;
      else
          sizeRemaining = aGivenSize.height;

      PRInt32 springConstantsRemaining = 0;

      nsCalculatedBoxInfo* info = first;

      while(info) { 
          // ignore collapsed children
          if (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED) {
              info = info->next;
              continue;
          }

          // figure out the direction of the box and get the correct value either the width or height
          nscoord pref = GET_WIDTH(info->prefSize);

          nscoord min  = GET_WIDTH(info->minSize);
        
          if (mState & NS_STATE_AUTO_STRETCH)
          {
              // stretch
              nscoord h = GET_HEIGHT(aGivenSize);
              nscoord max1 = GET_HEIGHT(info->maxSize);
              nscoord min1 = GET_HEIGHT(info->minSize);
              if (h < min1)
                  h = min1;
              else if (h > max1) 
                  h = max1;

              SET_HEIGHT(info->calculatedSize, h);
          } else {
              // go to preferred size
              nscoord h = GET_HEIGHT(info->prefSize);
              nscoord s = GET_HEIGHT(aGivenSize);
              if (h > s)
                  h = s;

              nscoord max1 = GET_HEIGHT(info->maxSize);
              nscoord min1 = GET_HEIGHT(info->minSize);
              if (h < min1)
                  h = min1;
              else if (h > max1) 
                  h = max1;

              SET_HEIGHT(info->calculatedSize, h);
          }

          if (pref < min) {
              pref = min;
              SET_WIDTH(info->prefSize, min);
          }

          if (info->mFlags & NS_FRAME_BOX_SIZE_VALID) { 
             sizeRemaining -= GET_WIDTH(info->calculatedSize);
          } else {
            if (info->flex == 0)
            {
              info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
              SET_WIDTH(info->calculatedSize, pref);
            }
            sizeRemaining -= pref;
            springConstantsRemaining += info->flex;
          } 

          info = info->next;
      }

      nscoord& sz = GET_WIDTH(aGivenSize);
      if (sz == NS_INTRINSICSIZE) {
          sz = 0;
          info = first;
          while(info) {

              // ignore collapsed springs
              if (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED) {
                 info = info->next;
                 continue;
              }

              nscoord pref = GET_WIDTH(info->prefSize);

              if (!(info->mFlags & NS_FRAME_BOX_SIZE_VALID)) 
              {
                // set the calculated size to be the preferred size
                SET_WIDTH(info->calculatedSize, pref);
                info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
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
              if (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED) {
                 info = info->next;
                 continue;
              }

              nscoord pref = GET_WIDTH(info->prefSize);
              nscoord max  = GET_WIDTH(info->maxSize);
              nscoord min  = GET_WIDTH(info->minSize);
     
              if (!(info->mFlags & NS_FRAME_BOX_SIZE_VALID)) {
                  PRInt32 newSize = pref + (sizeRemaining*info->flex/springConstantsRemaining);
                  if (newSize<=min) {
                      SET_WIDTH(info->calculatedSize, min);
                      springConstantsRemaining -= info->flex;
                      sizeRemaining += pref;
                      sizeRemaining -= min;
 
                      info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
                      limit = PR_TRUE;
                  }
                  else if (newSize>=max) {
                      SET_WIDTH(info->calculatedSize, max);
                      springConstantsRemaining -= info->flex;
                      sizeRemaining += pref;
                      sizeRemaining -= max;
                      info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
                      limit = PR_TRUE;
                  }
              }
              info = info->next;
          }
      }

         nscoord& s = GET_WIDTH(aGivenSize);
         s = 0;
         aMaxAscent = 0;

        info = first;
        while(info) {

             // ignore collapsed springs
            if (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED) {
                 info = info->next;
                 continue;
            }

             nscoord pref = GET_WIDTH(info->prefSize);
  
            if (!(info->mFlags & NS_FRAME_BOX_SIZE_VALID)) {
                if (springConstantsRemaining == 0) { 
                    SET_WIDTH(info->calculatedSize, pref);
                }
                else {
                    SET_WIDTH(info->calculatedSize, pref + info->flex*sizeRemaining/springConstantsRemaining);
                }

                info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
            }

            s += GET_WIDTH(info->calculatedSize);

            if (info->ascent > aMaxAscent)
                aMaxAscent = info->ascent;

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

// Marks the frame as dirty and generates an incremental reflow
// command targeted at this frame
nsresult
nsBoxFrame::GenerateDirtyReflowCommand(nsIPresContext* aPresContext,
                                       nsIPresShell&   aPresShell)
{
  if (mState & NS_FRAME_IS_DIRTY)      
       return NS_OK;

  // ask out parent to dirty things.
  mState |= NS_FRAME_IS_DIRTY;
  return mParent->ReflowDirtyChild(&aPresShell, this);
}

NS_IMETHODIMP
nsBoxFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
    mInner->SanityCheck();

    // remove child from our info list
    mInner->mInfoList->Remove(&aPresShell, aOldFrame);

    // remove the child frame
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    mInner->SanityCheck();

    // mark us dirty and generate a reflow command
    return GenerateDirtyReflowCommand(aPresContext, aPresShell);
}

NS_IMETHODIMP
nsBoxFrame::Destroy(nsIPresContext* aPresContext)
{
// if we are root remove 1 from the debug count.
  if (mState & NS_STATE_IS_ROOT)
      nsBoxDebug::GetPref(aPresContext);
      
  if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
      nsBoxDebug::Release(aPresContext);

  // recycle the Inner via the shell's arena.
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  mInner->Recycle(shell);
  mInner = nsnull;

  return nsHTMLContainerFrame::Destroy(aPresContext);
} 

void
nsBoxFrameInner::RecycleFreedMemory(nsIPresShell* aShell, void* aMem)
{
  size_t* sz = (size_t*)aMem;
  aShell->FreeFrame(*sz, aMem);
}

NS_IMETHODIMP
nsBoxFrame::SetDebug(nsIPresContext* aPresContext, PRBool aDebug)
{
  // see if our state matches the given debug state
  PRBool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  PRBool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
     if (aDebug) {
         mState |= NS_STATE_CURRENTLY_IN_DEBUG;
         nsBoxDebug::AddRef(aPresContext, this);
     } else {
         mState &= ~NS_STATE_CURRENTLY_IN_DEBUG;
         nsBoxDebug::Release(aPresContext);
     }

     return mInner->mInfoList->SetDebug(aPresContext, aDebug);
  }

  return NS_OK;
}

void
nsBoxDebug::GetPref(nsIPresContext* aPresContext)
{
    gDebug = PR_FALSE;
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_PROGID));
    if (pref) {
	    pref->GetBoolPref("xul.debug.box", &gDebug);
    }
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
   mInner->mInfoList->Insert(&aPresShell, aPrevFrame, aFrameList);

   // insert the frames in out regular frame list
   mFrames.InsertFrames(this, aPrevFrame, aFrameList);

   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       mInner->mInfoList->SetDebug(aPresContext, PR_TRUE);

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
   mInner->mInfoList->Append(&aPresShell,aFrameList);

   // append in regular frames
   mFrames.AppendFrames(this, aFrameList); 
   
   // if we are in debug make sure our children are in debug as well.
   if (mState & NS_STATE_CURRENTLY_IN_DEBUG)
       mInner->mInfoList->SetDebug(aPresContext, PR_TRUE);

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
        GenerateDirtyReflowCommand(aPresContext, *shell);
    }
  
  return rv;
}

/**
 * Goes though each child asking for its size to determine our size. Returns our box size minus our border.
 * This method is defined in nsIBox interface.
 */
/**
 * Goes though each child asking for its size to determine our size. Returns our box size minus our border.
 * This method is defined in nsIBox interface.
 */
NS_IMETHODIMP
nsBoxFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
   nsIFrame* incrementalChild = nsnull;

   if (aReflowState.reason == eReflowReason_Incremental) {
      // then get the child we need to flow incrementally
      aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
   }

   //nsMargin debugInset(0,0,0,0);
   //mInner->GetDebugInset(debugInset);

   nsresult rv;

   aSize.Clear();
   GetDefaultFlex(aSize.flex);
 
   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

       nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();

       while (info) 
       {  
          // see if the child is collapsed
          const nsStyleDisplay* disp;
          info->frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));

          PRBool collapsed = (info->mFlags & NS_FRAME_BOX_IS_COLLAPSED);
          PRBool visibilityCollapsed = disp->mVisible == NS_STYLE_VISIBILITY_COLLAPSE;
          PRBool collapsedChanged = (!collapsed && visibilityCollapsed) || (collapsed && !visibilityCollapsed);

        // if a child needs recalculation then ask it for its size. Otherwise
        // just use the size we already have.
        if ((info->mFlags & NS_FRAME_BOX_NEEDS_RECALC) || info->frame == incrementalChild || collapsedChanged)
        {
            if (visibilityCollapsed) {
                nsFrameState childState;
                info->frame->GetFrameState(&childState);
                childState |= NS_FRAME_IS_DIRTY;
                info->frame->SetFrameState(childState);
            }

            if (!visibilityCollapsed || aReflowState.reflowCommand != nsnull) {
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
            info->mFlags &= ~NS_FRAME_BOX_NEEDS_RECALC;

            if (visibilityCollapsed) {
                 info->mFlags |= NS_FRAME_BOX_IS_COLLAPSED;
            }
        } 

        if (!visibilityCollapsed) 
           AddChildSize(aSize, *info);

        // if horizontal get the largest child's ascent
        if (mState & NS_STATE_IS_HORIZONTAL) {
            if (info->ascent > aSize.ascent)
                aSize.ascent = info->ascent;
        } else {
            // if vertical then get the last child's ascent.
            if (info->next)
               aSize.ascent += info->prefSize.height;
            else
               aSize.ascent += info->ascent;
        }

        info = info->next;
       }
   
  // add the insets into our size. This is merely some extra space subclasses like toolbars
  // can place around us. Toolbars use it to place extra control like grippies around things.
  nsMargin inset(0,0,0,0);
  GetInset(inset);

 // inset += debugInset;

  nsSize in(inset.left+inset.right,inset.top+inset.bottom);
  aSize.minSize += in;
  aSize.prefSize += in;

  /*
  // make sure we can see the debug info
  if (aSize.maxSize.width < debugInset.left + debugInset.right)
      aSize.maxSize.width = debugInset.left + debugInset.right;

  if (aSize.maxSize.height < debugInset.top + debugInset.bottom)
      aSize.maxSize.height = debugInset.top + debugInset.bottom;
  */

  aSize.ascent += inset.top;
  //aSize.ascent += debugInset.top;

  return rv;
}

void
nsBoxFrame::AddChildSize(nsBoxInfo& aInfo, nsBoxInfo& aChildInfo)
{
    // if the child is not flexible then its min, max, is the same as its pref.
    if (aChildInfo.flex == 0) {

        nsSize min(aChildInfo.minSize);
        nsSize max(aChildInfo.maxSize);
        nsSize pref(aChildInfo.prefSize);

        SET_WIDTH(min, GET_WIDTH(pref));
        SET_WIDTH(max, GET_WIDTH(pref));

        AddSize(min, aInfo.minSize,  PR_FALSE);
        AddSize(max, aInfo.maxSize,  PR_TRUE);
        AddSize(pref, aInfo.prefSize, PR_FALSE);
    } else {
        // now that we know our child's min, max, pref sizes figure OUR size from them.
        AddSize(aChildInfo.minSize,  aInfo.minSize,  PR_FALSE);
        AddSize(aChildInfo.maxSize,  aInfo.maxSize,  PR_TRUE);
        AddSize(aChildInfo.prefSize, aInfo.prefSize, PR_FALSE);
    }
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

  // if we are visible then tell our superclass to paint
  nsresult r = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
        if (mState & NS_STATE_CURRENTLY_IN_DEBUG) {
            nsBoxDebug::gInstance->PaintSprings(aPresContext, this, aRenderingContext, aDirtyRect);
        }
  }
 
  
  /*
#ifdef DEBUG_REFLOW
  if (mInner->reflowCount == gViewPortReflowCount) {
     if (!mInner->mResized)
        aRenderingContext.SetColor(NS_RGB(255,0,0));
     else
        aRenderingContext.SetColor(NS_RGB(0,255,0));

     aRenderingContext.DrawRect(nsRect(0,0,mRect.width, mRect.height));
  }
#endif
  */
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
nsBoxFrame::PaintChildren(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer)
{
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
    //nsMargin dm(0,0,0,0);
    //mInner->GetDebugInset(dm);
    nsMargin im(0,0,0,0);
    GetInset(im);
    nsMargin border(0,0,0,0);
    const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
    spacing->GetBorderPadding(border);
    r.Deflate(im);
    //r.Deflate(dm);
    r.Deflate(border);    
  }

  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    if (!hasClipped && NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
        // if we haven't already clipped and we should
        // check to see if the child is in out bounds. If not then
        // we begin clipping.
        nsRect cr(0,0,0,0);
        kid->GetRect(cr);
    
        // if our rect does not contain the childs then begin clipping
        if (!r.Contains(cr)) {
            aRenderingContext.PushState();
            aRenderingContext.SetClipRect(r,
                                          nsClipCombine_kIntersect, clipState);
            hasClipped = PR_TRUE;
        }
    }

    PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, aWhichLayer);
    kid->GetNextSibling(&kid);
  }

  if (hasClipped) {
    aRenderingContext.PopState(clipState);
  }
}

void 
nsBoxDebug::PaintSprings(nsIPresContext* aPresContext, nsBoxFrame* aBoxFrame,
                         nsIRenderingContext& aRenderingContext, const nsRect& aDirtyRect)
{
    
        PRBool isHorizontal = aBoxFrame->mState & NS_STATE_IS_HORIZONTAL;

        // remove our border
        const nsStyleSpacing* spacing;
        aBoxFrame->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&) spacing);

        nsMargin border(0,0,0,0);
        spacing->GetBorderPadding(border);

        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

        nsIStyleContext* debugStyle;
        if (aBoxFrame->mState & NS_STATE_IS_HORIZONTAL)
            debugStyle = mHorizontalDebugStyle;
        else
            debugStyle = mVerticalDebugStyle;

        if (debugStyle == nsnull)
            return;

        const nsStyleSpacing* debugSpacing =
        (const nsStyleSpacing*)debugStyle->GetStyleData(eStyleStruct_Spacing);
        const nsStyleColor* debugColor =
        (const nsStyleColor*)debugStyle->GetStyleData(eStyleStruct_Color);

        nsMargin margin(0,0,0,0);
        debugSpacing->GetMargin(margin);

        border += margin;

        nsRect inner(0,0,aBoxFrame->mRect.width, aBoxFrame->mRect.height);
        inner.Deflate(border);

        // paint our debug border
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aBoxFrame,
                                    aDirtyRect, inner, *debugSpacing, debugStyle, 0);

        // get the debug border dimensions
        nsMargin debugBorder(0,0,0,0);
        debugSpacing->GetBorderPadding(debugBorder);


        // paint the springs.
        nscoord x, y, borderSize, springSize;
        

        aRenderingContext.SetColor(debugColor->mColor);
        

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

        nsCalculatedBoxInfo* info = aBoxFrame->GetInfoList()->GetFirst();
        while (info) {
            nsSize& size = info->calculatedSize;
            if (!(info->mFlags & NS_FRAME_BOX_IS_COLLAPSED)) {
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

                DrawSpring(aPresContext, aRenderingContext, isHorizontal, info->flex, x, y, borderSize, springSize);
                x += borderSize;
            }
            info = info->next;
        }
}

void
nsBoxDebug::DrawLine(nsIRenderingContext& aRenderingContext, PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2)
{
    if (aHorizontal)
       aRenderingContext.DrawLine(x1,y1,x2,y2);
    else
       aRenderingContext.DrawLine(y1,x1,y2,x2);
}

void
nsBoxDebug::FillRect(nsIRenderingContext& aRenderingContext, PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height)
{
    if (aHorizontal)
       aRenderingContext.FillRect(x,y,width,height);
    else
       aRenderingContext.FillRect(y,x,height,width);
}

void 
nsBoxDebug::DrawSpring(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord springSize)
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
nsBoxDebug::AddRef(nsIPresContext* aPresContext, nsBoxFrame* aBoxFrame)
{
    if (!gInstance) {
        gInstance = new nsBoxDebug(aPresContext, aBoxFrame);
    }

    gInstance->mRefCount++;
}

void
nsBoxDebug::Release(nsIPresContext* aPresContext)
{
   NS_ASSERTION(gInstance && gInstance->mRefCount > 0, "nsBoxDebug Realsed too may times!!");

   gInstance->mRefCount--;
   if (gInstance->mRefCount == 0) {
       delete gInstance;
       gInstance = nsnull;
   } 
}

nsBoxDebug::nsBoxDebug(nsIPresContext* aPresContext, nsBoxFrame* aBoxFrame)
{
    nsCOMPtr<nsIContent> content;
    aBoxFrame->mInner->GetContentOf(aBoxFrame, getter_AddRefs(content));

	nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-horizontal-box-debug")) );
	aPresContext->ProbePseudoStyleContextFor(content, atom, aBoxFrame->mStyleContext,
										  PR_FALSE,
										  getter_AddRefs(mHorizontalDebugStyle));

  	atom = getter_AddRefs(NS_NewAtom(":-moz-vertical-box-debug"));
	aPresContext->ProbePseudoStyleContextFor(content, atom, aBoxFrame->mStyleContext,
										  PR_FALSE,
										  getter_AddRefs(mVerticalDebugStyle));

    mDebugChild = nsnull;
    mRefCount = 0;
}

void
nsBoxFrameInner::GetDebugInset(nsMargin& inset)
{
    inset.SizeTo(0,0,0,0);

    if (mOuter->mState & NS_STATE_CURRENTLY_IN_DEBUG) 
    {
        nsIStyleContext* style;
        if (mOuter->mState & NS_STATE_IS_HORIZONTAL)
            style = nsBoxDebug::gInstance->mHorizontalDebugStyle;
        else
            style = nsBoxDebug::gInstance->mVerticalDebugStyle;

        if (style == nsnull)
            return;

        const nsStyleSpacing* debugSpacing =
        (const nsStyleSpacing*)style->GetStyleData(eStyleStruct_Spacing);

        debugSpacing->GetBorderPadding(inset);
        nsMargin margin(0,0,0,0);
        debugSpacing->GetMargin(margin);
        inset += margin;
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
nsBoxFrame::GetFrameName(nsString& aResult) const
{
    nsCOMPtr<nsIContent> content;
    nsIFrame* frame = (nsIFrame*)this;
    mInner->GetContentOf(frame, getter_AddRefs(content));

    nsString id;
    content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);

    aResult = "Box[id=";
    aResult.Append(id);
    aResult.Append("]");
    return NS_OK;
}

void* 
nsCalculatedBoxInfoImpl::operator new(size_t sz, nsIPresShell* aPresShell)
{
    return nsBoxFrameInner::Allocate(sz,aPresShell);
}

void 
nsCalculatedBoxInfo::Recycle(nsIPresShell* aShell) 
{
}

void 
nsCalculatedBoxInfoImpl::Recycle(nsIPresShell* aShell) 
{
   delete this;
   nsBoxFrameInner::RecycleFreedMemory(aShell, this);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsCalculatedBoxInfoImpl::operator delete(void* aPtr, size_t sz)
{
    nsBoxFrameInner::Free(aPtr, sz);
}

//static PRInt32 gBoxInfoCount = 0;

nsCalculatedBoxInfoImpl::nsCalculatedBoxInfoImpl(nsIFrame* aFrame)
{
   //gBoxInfoCount++;
  // printf("created Info=%d\n",gBoxInfoCount);

    mFlags = NS_FRAME_BOX_NEEDS_RECALC;

    next = nsnull;
    calculatedSize.width = 0;
    calculatedSize.height = 0;
    frame = aFrame;
    prefSize.width = 0;
    prefSize.height = 0;
    minSize.width = 0;
    minSize.height = 0;
    ascent = 0;
    flex = 0;
    maxSize.width = NS_INTRINSICSIZE;
    maxSize.height = NS_INTRINSICSIZE;
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

    mFlags = NS_FRAME_BOX_NEEDS_RECALC;

    calculatedSize.width = 0;
    calculatedSize.height = 0;

    //prefWidthIntrinsic = PR_TRUE;
    //prefHeightIntrinsic = PR_TRUE;

}

NS_IMETHODIMP  
nsBoxFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                             const nsPoint& aPoint, 
                             nsIFrame**     aFrame)
{   
    return nsHTMLContainerFrame::GetFrameForPoint(aPresContext, aPoint, aFrame);

    /*
  nsRect r(0,0,mRect.width, mRect.height);

  // if it is not inside us fail
  if (!r.Contains(aPoint)) {
      return NS_ERROR_FAILURE;
  }

  // is it inside our border, padding, and debugborder or insets?
  //nsMargin dm(0,0,0,0);
  //mInner->GetDebugInset(dm);
  nsMargin im(0,0,0,0);
  GetInset(im);
  nsMargin border(0,0,0,0);
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
  mStyleContext->GetStyleData(eStyleStruct_Spacing);
  spacing->GetBorderPadding(border);
  r.Deflate(im);
  //r.Deflate(dm);
  r.Deflate(border);    

  // no? Then it must be in our border so return us.
  if (!r.Contains(aPoint)) {
      *aFrame = this;
      return NS_OK;
  }

  // ok lets look throught the children
  nsCalculatedBoxInfo* info = mInner->mInfoList->GetFirst();
  nsRect kidRect;
  while(info)
  {
    info->frame->GetRect(kidRect);

    // Do a quick check and see if the child frame contains the point
    if (kidRect.Contains(aPoint)) {
        nsPoint tmp;
        tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
        return info->frame->GetFrameForPoint(aPresContext, tmp, aFrame);
    }

    info = info->next;
  }

  const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

  PRBool        transparentBG = NS_STYLE_BG_COLOR_TRANSPARENT ==
                                (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);

  PRBool backgroundImage = (color->mBackgroundImage.Length() > 0);

  if (!transparentBG || backgroundImage)
  {
      *aFrame = this;
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
  */
}


void
nsBoxDebug::GetValue(nsIPresContext* aPresContext, const nsSize& a, const nsSize& b, char* ch) 
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
    

    sprintf(ch, "(%s%s, %s%s)", width, (b.width != NS_INTRINSICSIZE ? "[CSS]" : ""),
                    height, (b.height != NS_INTRINSICSIZE ? "[CSS]" : ""));

}

void
nsBoxDebug::GetValue(nsIPresContext* aPresContext, PRInt32 a, PRInt32 b, char* ch) 
{
    sprintf(ch, "(%d)", a);             
}

PRBool
nsBoxDebug::DisplayDebugInfoFor(nsIPresContext* aPresContext,
                                nsBoxFrame* aBoxFrame,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
    nscoord x = aPoint.x;
    nscoord y = aPoint.y;

    // get the area inside our border.
    nsRect insideBorder(0,0,aBoxFrame->mRect.width, aBoxFrame->mRect.height);

    const nsStyleSpacing* spacing;
    nsresult rv = aBoxFrame->GetStyleData(eStyleStruct_Spacing,
               (const nsStyleStruct*&) spacing);

    NS_ASSERTION(rv == NS_OK,"failed to get spacing");
    if (NS_FAILED(rv))
     return rv;

    nsMargin border;
    spacing->GetBorderPadding(border);

    insideBorder.Deflate(border);

    PRBool isHorizontal = aBoxFrame->mState & NS_STATE_IS_HORIZONTAL;

    if (!insideBorder.Contains(nsPoint(x,y)))
        return NS_OK;

        //printf("%%%%%% inside box %%%%%%%\n");

        int count = 0;
        nsCalculatedBoxInfo* info = aBoxFrame->GetInfoList()->GetFirst();

        nsMargin m;
        aBoxFrame->mInner->GetDebugInset(m);

        if ((isHorizontal && y < insideBorder.y + m.top) ||
            (!isHorizontal && x < insideBorder.x + m.left)) {
            //printf("**** inside debug border *******\n");
            while (info) 
            {    
                nsIFrame* childFrame = info->frame;
                nsRect r;
                childFrame->GetRect(r);

                // if we are not in the child. But in the spring above the child.
                if ((isHorizontal && x >= r.x && x < r.x + r.width) ||
                    (!isHorizontal && y >= r.y && y < r.y + r.height)) {
                    aCursor = NS_STYLE_CURSOR_POINTER;
                       // found it but we already showed it.
                        if (mDebugChild == childFrame)
                            return PR_TRUE;


                            nsCOMPtr<nsIContent> content;
                            aBoxFrame->mInner->GetContentOf(aBoxFrame, getter_AddRefs(content));

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

                            printf("----- ");
               
    #ifdef NS_DEBUG
                            nsFrame::ListTag(stdout, aBoxFrame);
    #endif

                            printf(" Tag='%s', id='%s' class='%s'---------------\n", tagValue, idValue, kClassValue);
                        

                        childFrame->GetContent(getter_AddRefs(content));

                        if (content) {

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
                        rv = childFrame->GetStyleData(eStyleStruct_Position,
                                      (const nsStyleStruct*&) position);

                        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get position");
                        if (NS_FAILED(rv))
                            return PR_TRUE;

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

                        if (content) {
                            nsAutoString value;
                            PRInt32 error;
                            
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

                        char min[100];
                        char pref[100];
                        char max[100];
                        char calc[100];
                        char flex[100];

                        /*
                        nsSize c(info->calculatedSize);
                        if (c.width != NS_INTRINSICSIZE)
                            c.width -= inset.left + inset.right;

                       if (c.height != NS_INTRINSICSIZE)
                            c.height -= inset.left + inset.right;
                 
                        */

                        GetValue(aPresContext, info->minSize,  aSize.minSize, min);
                        GetValue(aPresContext, info->prefSize, aSize.prefSize, pref);
                        GetValue(aPresContext, info->maxSize, aSize.maxSize, max);
                        GetValue(aPresContext, info->calculatedSize,  aSize.calculatedSize, calc);
                        GetValue(aPresContext, info->flex,  aSize.flex, flex);


                        printf("min%s, pref%s, max%s, actual%s, flex=%s\n\n", 
                            min,
                            pref,
                            max,
                            calc,
                            flex
                        );

                        return PR_TRUE;   
                }

              info = info->next;
              count++;
            }
        } else {
                mDebugChild = nsnull;
        }

        return PR_FALSE;
}

NS_IMETHODIMP
nsBoxFrame::GetCursor(nsIPresContext* aPresContext,
                           nsPoint&        aPoint,
                           PRInt32&        aCursor)
{
    nsPoint newPoint;
    mInner->TranslateEventCoords(aPresContext, aPoint, newPoint);

    /*
    nsCOMPtr<nsIContent> content;
    mInner->GetContentOf(this, getter_AddRefs(content));

    nsString id;
    content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);
    char idValue[100];
    id.ToCString(idValue,100);
*/

   /// printf("----------Box id = %s-----------\n", idValue);
   // printf("x=%d, r.x=%d r.x + r.width=%d\n",newPoint.x, or.x, or.x + or.width);
   // printf("y=%d, r.y=%d r.y + r.height=%d\n",newPoint.y, or.y, or.y + or.height);

    //aCursor = NS_STYLE_CURSOR_POINTER;

    
    // if we are in debug and we are in the debug area
    // return our own cursor and dump the debug information.
    if (mState & NS_STATE_CURRENTLY_IN_DEBUG) 
    {
        if (nsBoxDebug::gInstance->DisplayDebugInfoFor(aPresContext, this, newPoint, aCursor)) 
            return NS_OK;
    }
    
    return nsHTMLContainerFrame::GetCursor(aPresContext, aPoint, aCursor);
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

  /*
  const nsStyleSpacing* spacing;
        nsresult rv = mOuter->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

  nsMargin m(0,0,0,0);
  spacing->GetBorderPadding(m);

  // Subtract out border and padding here so that the coordinates are
  // now relative to the content area of this frame.
  x -= m.left + m.right;
  y -= m.top + m.bottom;
*/

  aResult.x = x;
  aResult.y = y;
 
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

void* 
nsInfoListImpl::operator new(size_t sz, nsIPresShell* aPresShell)
{
    return nsBoxFrameInner::Allocate(sz,aPresShell);
}

void 
nsInfoListImpl::Recycle(nsIPresShell* aShell) 
{
   // recycle all out box infos
   Clear(aShell);

   delete this;
   nsBoxFrameInner::RecycleFreedMemory(aShell, this);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsInfoListImpl::operator delete(void* aPtr, size_t sz)
{
    nsBoxFrameInner::Free(aPtr, sz);
}

nsInfoListImpl::nsInfoListImpl():mFirst(nsnull),mLast(nsnull),mCount(0)
{

}

nsInfoListImpl::~nsInfoListImpl() 
{
}

nsresult
nsInfoListImpl::SetDebug(nsIPresContext* aPresContext, PRBool aDebug)
{
    nsCalculatedBoxInfo* info = GetFirst();
    while (info)
    {
      nsIBox* ibox = nsnull;
      if (NS_SUCCEEDED(info->frame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) {
          ibox->SetDebug(aPresContext, aDebug);
      }

      // make sure we recalculate our children's sizes
      info->mFlags |= NS_FRAME_BOX_NEEDS_RECALC;

      info = info->next;
    }


    return NS_OK;
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
nsInfoListImpl::Clear(nsIPresShell* aShell)
{
   nsCalculatedBoxInfo* info = mFirst;
   while(info) {
      nsCalculatedBoxInfo* it = info;
      info = info->next;
      it->Recycle(aShell);
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
nsInfoListImpl::CreateInfosFor(nsIPresShell* aPresShell, nsIFrame* aList, nsCalculatedBoxInfo*& first, nsCalculatedBoxInfo*& last)
{
    PRInt32 count = 0;
    if (aList) {
       first = new (aPresShell) nsCalculatedBoxInfoImpl(aList);
       count++;
       last = first;
       aList->GetNextSibling(&aList);
       while(aList) {
         last->next = new (aPresShell) nsCalculatedBoxInfoImpl(aList);
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
nsInfoListImpl::Remove(nsIPresShell* aShell, nsIFrame* aFrame)
{
    // get the info before the frame
    nsCalculatedBoxInfo* prevInfo = GetPrevious(aFrame);
    RemoveAfter(aShell, prevInfo);        
}

void
nsInfoListImpl::Insert(nsIPresShell* aShell, nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
   nsCalculatedBoxInfo* prevInfo = GetInfo(aPrevFrame);

   // find the frame before this one
   // if no previous frame then we are inserting in front
   if (prevInfo == nsnull) {
       // prepend them
       Prepend(aShell, aFrameList);
   } else {
       // insert insert after previous info
       InsertAfter(aShell, prevInfo, aFrameList);
   }
}

void
nsInfoListImpl::RemoveAfter(nsIPresShell* aPresShell, nsCalculatedBoxInfo* aPrevious)
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

    toDelete->Recycle(aPresShell);
    mCount--;
}

void
nsInfoListImpl::Prepend(nsIPresShell* aPresShell, nsIFrame* aList)
{
    nsCalculatedBoxInfo* first;
    nsCalculatedBoxInfo* last;
    mCount += CreateInfosFor(aPresShell, aList, first, last);
    if (!mFirst)
        mFirst = mLast = first;
    else {
        last->next = mFirst;
        mFirst = first;
    }
}

void
nsInfoListImpl::Append(nsIPresShell* aPresShell, nsIFrame* aList)
{
    nsCalculatedBoxInfo* first;
    nsCalculatedBoxInfo* last;
    mCount += CreateInfosFor(aPresShell, aList, first, last);
    if (!mFirst) 
        mFirst = first;
    else 
        mLast->next = first;
    
    mLast = last;
}

void 
nsInfoListImpl::InsertAfter(nsIPresShell* aPresShell, nsCalculatedBoxInfo* aPrev, nsIFrame* aList)
{
    nsCalculatedBoxInfo* first;
    nsCalculatedBoxInfo* last;
    mCount += CreateInfosFor(aPresShell, aList, first, last);
    last->next = aPrev->next;
    aPrev->next = first;
    if (aPrev == mLast)
        mLast = last;
}

void 
nsInfoListImpl::Init(nsIPresShell* aPresShell, nsIFrame* aList)
{
    Clear(aPresShell);
    mCount += CreateInfosFor(aPresShell, aList, mFirst, mLast);
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

nsBoxInfo::nsBoxInfo():prefSize(0,0), minSize(0,0), maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE), flex(0)
{ 
}

void 
nsBoxInfo::Clear()
{
      prefSize.width = 0;
      prefSize.height = 0;

      minSize.width = 0;
      minSize.height = 0;

      ascent = 0;

      flex = 0;

      maxSize.width = NS_INTRINSICSIZE;
      maxSize.height = NS_INTRINSICSIZE;
}

nsBoxInfo::~nsBoxInfo()
{
}










