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

#include "nsSplitterFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIXMLContent.h"
#include "nsIPresContext.h"
#include "nsDocument.h"
#include "nsINameSpaceManager.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsWidgetsCID.h"
#include "nsBoxLayoutState.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"

#define REAL_TIME_DRAG


const PRInt32 kMaxZ = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32


class nsSplitterInfo {
public:
    nscoord min;
    nscoord max;
    nscoord current;
    nscoord changed;
    nsIBox* child;
    PRInt32 flex;
    PRInt32 index;
};

class nsSplitterFrameInner : public nsIDOMMouseListener, public nsIDOMMouseMotionListener {

public:

  NS_DECL_ISUPPORTS

  nsSplitterFrameInner(nsSplitterFrame* aSplitter)
  {
    NS_INIT_REFCNT();
    mOuter = aSplitter;
    mPressed = PR_FALSE;
  }
  virtual ~nsSplitterFrameInner();

  // mouse listener
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) { return MouseMove(aMouseEvent); }
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  // mouse motion listener
  virtual nsresult MouseMove(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  void MouseDrag(nsIPresContext* aPresContext, nsGUIEvent* aEvent);
  void MouseUp(nsIPresContext* aPresContext, nsGUIEvent* aEvent);

  void AdjustChildren(nsIPresContext* aPresContext);
  void AdjustChildren(nsIPresContext* aPresContext, nsSplitterInfo* aChildInfos, PRInt32 aCount, PRBool aIsHorizontal);

  void AddRemoveSpace(nscoord aDiff,
                    nsSplitterInfo* aChildInfos,
                    PRInt32 aCount,
                    PRInt32& aSpaceLeft);

  void ResizeChildTo(nsIPresContext* aPresContext,
                   nscoord& aDiff, 
                   nsSplitterInfo* aChildrenBeforeInfos, 
                   nsSplitterInfo* aChildrenAfterInfos, 
                   PRInt32 aChildrenBeforeCount, 
                   PRInt32 aChildrenAfterCount, 
                   PRBool aBounded);

  void UpdateState();

  void AddListener(nsIPresContext* aPresContext);
  void RemoveListener();

  enum ResizeType { Closest, Farthest, Grow };
  enum State { Open, Collapsed, Dragging };
  enum CollapseDirection { Before, After, None };

  ResizeType GetResizeBefore();
  ResizeType GetResizeAfter();
  State GetState();

  //nsresult CaptureMouse(nsIPresContext* aPresContext, PRBool aGrabMouseEvents);
  //PRBool IsMouseCaptured(nsIPresContext* aPresContext);
  void Reverse(nsSplitterInfo*& aIndexes, PRInt32 aCount);
  CollapseDirection GetCollapseDirection();

  void MoveSplitterBy(nsIPresContext* aPresContext, nscoord aDiff);
  void EnsureOrient();
  void SetPreferredSize(nsBoxLayoutState& aState, nsIBox* aChildBox, nscoord aOnePixel, PRBool aIsHorizontal, nscoord* aSize);

  nsSplitterFrame* mOuter;
  PRBool mDidDrag;
  nscoord mDragStartPx;
  nscoord mCurrentPos;
  nsIBox* mParentBox;
  PRBool mPressed;
  nsSplitterInfo* mChildInfosBefore;
  nsSplitterInfo* mChildInfosAfter;
  PRInt32 mChildInfosBeforeCount;
  PRInt32 mChildInfosAfterCount;
  State mState;
  nscoord mSplitterPos;
  nscoord mSplitterViewPos;
  PRBool mDragging;

};


NS_IMPL_ISUPPORTS2(nsSplitterFrameInner, nsIDOMMouseListener, nsIDOMMouseMotionListener)

nsSplitterFrameInner::ResizeType
nsSplitterFrameInner::GetResizeBefore()
{
    nsCOMPtr<nsIContent> content;  
    mOuter->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::resizebefore, value);
    if (value.EqualsIgnoreCase("farthest"))
      return Farthest;
    else
      return Closest;
}

nsSplitterFrameInner::~nsSplitterFrameInner() 
{
  delete[] mChildInfosBefore;
  delete[] mChildInfosAfter;
}

nsSplitterFrameInner::ResizeType
nsSplitterFrameInner::GetResizeAfter()
{
    nsCOMPtr<nsIContent> content;  
    mOuter->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::resizeafter, value);
    if (value.EqualsIgnoreCase("farthest"))
      return Farthest;
    else if (value.EqualsIgnoreCase("grow"))
      return Grow;
    else 
      return Closest;
}

nsSplitterFrameInner::State
nsSplitterFrameInner::GetState()
{
    nsCOMPtr<nsIContent> content;  
    mOuter->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::state, value);
    if (value.EqualsIgnoreCase("dragging"))
      return Dragging;
    else if (value.EqualsIgnoreCase("collapsed"))
      return Collapsed;
	else 
      return Open;

}

//
// NS_NewSplitterFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewSplitterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSplitterFrame* it = new (aPresShell) nsSplitterFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSplitterFrame

nsSplitterFrame::nsSplitterFrame(nsIPresShell* aPresShell):nsBoxFrame(aPresShell)
{
  mInner = new nsSplitterFrameInner(this);
  mInner->AddRef();
  mInner->mChildInfosAfter = nsnull;
  mInner->mChildInfosBefore = nsnull;
  mInner->mState = nsSplitterFrameInner::Open;
  mInner->mDragging = PR_FALSE;
}

nsSplitterFrame::~nsSplitterFrame()
{
  mInner->RemoveListener();
  mInner->Release();
}


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsSplitterFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);

/**
 * Anonymous interface
 */
NS_IMETHODIMP
nsSplitterFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                        nsISupportsArray& aAnonymousChildren)
{
  // if not content the create some anonymous content
  PRInt32 count = 0;
  mContent->ChildCount(count); 

  // create a grippy if we have no children and teh collapse attribute is before or after.
  if (count == 0) 
  {
    nsSplitterFrameInner::CollapseDirection d = mInner->GetCollapseDirection();
    if (d != nsSplitterFrameInner::None)
    {
        // create a spring
        nsCOMPtr<nsIContent> content;
        NS_CreateAnonymousNode(mContent, nsXULAtoms::spring, nsXULAtoms::nameSpaceID, content);
        content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, NS_ConvertASCIItoUCS2("100%"), PR_FALSE);
        aAnonymousChildren.AppendElement(content);

        // a grippy
        NS_CreateAnonymousNode(mContent, nsXULAtoms::grippy, nsXULAtoms::nameSpaceID, content);
        aAnonymousChildren.AppendElement(content);

        // create a spring
        NS_CreateAnonymousNode(mContent, nsXULAtoms::spring, nsXULAtoms::nameSpaceID, content);
        content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, NS_ConvertASCIItoUCS2("100%"), PR_FALSE);
        aAnonymousChildren.AppendElement(content);
     }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::SetDocumentForAnonymousContent(nsIDocument* aDocument,
                                                PRBool aDeep,
                                                PRBool aCompileEventHandlers)
{
  // XXX WRITE ME
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::GetCursor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
   return nsBoxFrame::GetCursor(aPresContext, aPoint, aCursor);

   /*
   if (IsHorizontal())
      aCursor = NS_STYLE_CURSOR_N_RESIZE;
   else
      aCursor = NS_STYLE_CURSOR_W_RESIZE;
   
   return NS_OK;
   */
}

NS_IMETHODIMP
nsSplitterFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);
  // if the alignment changed. Let the grippy know
  if (aAttribute == nsHTMLAtoms::align) {
     // tell the slider its attribute changed so it can 
     // update itself
     nsIFrame* grippy = nsnull;
     nsScrollbarButtonFrame::GetChildWithTag(aPresContext, nsXULAtoms::grippy, this, grippy);
     if (grippy)
        grippy->AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  } else if (aAttribute == nsXULAtoms::state) {
        mInner->UpdateState();
  }

  return rv;
}

/**
 * Initialize us. If we are in a box get our alignment so we know what direction we are
 */
NS_IMETHODIMP
nsSplitterFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // XXX Hack because we need the pres context in some of the event handling functions...
  mPresContext = aPresContext; 

  nsHTMLContainerFrame::CreateViewForFrame(aPresContext,this,aContext,nsnull,PR_TRUE);
  nsIView* view;
  GetView(aPresContext, &view);

#if defined(REAL_TIME_DRAG) || defined(XP_UNIX)
    view->SetContentTransparency(PR_TRUE);
    view->SetZIndex(kMaxZ);
#else
     // currently this only works on win32 and mac
     static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);
     nsCOMPtr<nsIViewManager> viewManager;
     view->GetViewManager(*getter_AddRefs(viewManager));
     viewManager->SetViewContentTransparency(view, PR_TRUE);
     viewManager->SetViewZIndex(view, kMaxZ);

     // Need to have a widget to appear on top of other widgets.
     view->CreateWidget(kCChildCID);
#endif

  mInner->mState = nsSplitterFrameInner::Open;
  mInner->AddListener(aPresContext);
  mInner->mParentBox = nsnull;
  return rv;
}

NS_IMETHODIMP
nsSplitterFrame::DoLayout(nsBoxLayoutState& aState)
{
  nsIFrame* frame;
  GetFrame(&frame);

  nsFrameState childState;
  frame->GetFrameState(&childState);

  if (childState & NS_FRAME_FIRST_REFLOW) 
  {
    GetParentBox(&mInner->mParentBox);
    mInner->UpdateState();
  }

  return nsBoxFrame::DoLayout(aState);
}


PRBool
nsSplitterFrame::GetInitialOrientation(PRBool& aIsHorizontal)
{
 // find the box we are in
  nsIFrame* box = nsnull;
  nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::box, this, box);

  // if no box get the window because it is a box.
  if (box == nsnull)
      nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::window, this, box);

  // see if the box is horizontal or vertical we are the opposite
  if (box) {
     aIsHorizontal = !((nsBoxFrame*)box)->IsHorizontal();
     return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsSplitterFrame::HandlePress(nsIPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleMultiplePress(nsIPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleDrag(nsIPresContext* aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus*  aEventStatus)
{
   return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleRelease(nsIPresContext* aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP  nsSplitterFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                             const nsPoint& aPoint, 
                                             nsFramePaintLayer aWhichLayer,    
                                             nsIFrame**     aFrame)
{   
  if ((aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND))
    return NS_ERROR_FAILURE;

  // if the mouse is captured always return us as the frame.
  if (mInner->mDragging)
  {
    // XXX It's probably better not to check visibility here, right?
    *aFrame = this;
    return NS_OK;
  } else {

      if (!mRect.Contains(aPoint))
         return NS_ERROR_FAILURE;

      nsresult rv = nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
      if (rv == NS_ERROR_FAILURE) {
        *aFrame = this;
        rv = NS_OK;
      } 

      return rv;
  }
}

NS_IMETHODIMP
nsSplitterFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{

    switch (aEvent->message) {
    case NS_MOUSE_MOVE: 
         mInner->MouseDrag(aPresContext, aEvent);
    break;
  
    case NS_MOUSE_LEFT_BUTTON_UP:
         mInner->MouseUp(aPresContext, aEvent);     
    break;
  }

  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void
nsSplitterFrameInner::MouseUp(nsIPresContext* aPresContext, nsGUIEvent* aEvent)
{
      if (mDragging) {
          AdjustChildren(aPresContext);
          AddListener(aPresContext);
          mOuter->CaptureMouse(aPresContext, PR_FALSE);
          mDragging = PR_FALSE;
          State newState = GetState(); 
          // if the state is dragging then make it Open.
          if (newState == Dragging)
              mOuter->mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, nsAutoString(), PR_TRUE);

          mPressed = PR_FALSE;

          //printf("MouseUp\n");

      }
}

void
nsSplitterFrameInner::MouseDrag(nsIPresContext* aPresContext, nsGUIEvent* aEvent)
{
        if (mDragging) {

          //printf("Dragging\n");

          PRBool isHorizontal = !mOuter->IsHorizontal();
           // convert coord to pixels
          nscoord pos = isHorizontal ? aEvent->point.x : aEvent->point.y;

           // mDragStartPx is in pixels and is in our client areas coordinate system. 
           // so we need to first convert it so twips and then get it into our coordinate system.

           // convert start to twips
           nscoord startpx = mDragStartPx;
              
           float p2t;
           aPresContext->GetScaledPixelsToTwips(&p2t);
           nscoord onePixel = NSIntPixelsToTwips(1, p2t);
           nscoord start = startpx*onePixel;

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
                nsresult result = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
                if (NS_SUCCEEDED(result)) {
                    nscoord xoff = 0;
                    nscoord yoff = 0;
                    scrollingView->GetScrollPosition(xoff, yoff);
                    isHorizontal ? start += xoff : start += yoff;         
                }
              }
       
             nsRect r;
             parent->GetRect(r);
             isHorizontal ? start -= r.x : start -= r.y;
             parent->GetParent(&parent);
           }

           // take our current position and substract the start location
           pos -= start;

           //printf("Diff=%d\n", pos);
           
           ResizeType resizeAfter  = GetResizeAfter();

           PRBool bounded;

           if (resizeAfter == nsSplitterFrameInner::Grow)
               bounded = PR_FALSE;
           else 
               bounded = PR_TRUE;

		   int i;
           for (i=0; i < mChildInfosBeforeCount; i++) 
               mChildInfosBefore[i].changed = mChildInfosBefore[i].current;
           
           for (i=0; i < mChildInfosAfterCount; i++) 
               mChildInfosAfter[i].changed = mChildInfosAfter[i].current;

            nscoord oldPos = pos;

            ResizeChildTo(aPresContext, pos, mChildInfosBefore, mChildInfosAfter, mChildInfosBeforeCount, mChildInfosAfterCount, bounded);

            State currentState = GetState();
            CollapseDirection dir = GetCollapseDirection();

            // if we are in a collapsed position
            if ((oldPos > 0 && oldPos > pos && dir == After) || (oldPos < 0 && oldPos < pos && dir == Before))
            {
                // and we are not collapsed then collapse
                if (currentState == Dragging) {
                    if (oldPos > 0 && oldPos > pos)
                    {
                        //printf("Collapse right\n");
                        if (GetCollapseDirection() == After) 
                        {
                             mOuter->mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, NS_ConvertASCIItoUCS2("collapsed"), PR_TRUE);
                           
                        }

                    } else if (oldPos < 0 && oldPos < pos)
                    {
                        //printf("Collapse left\n");
                        if (GetCollapseDirection() == Before) 
                        {
                          mOuter->mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, NS_ConvertASCIItoUCS2("collapsed"), PR_TRUE);
                         
                        }
                    }
                }
            } else {
                // if we are not in a collapsed position and we are not dragging make sure
                // we are dragging.
                if (currentState != Dragging)
                  mOuter->mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, NS_ConvertASCIItoUCS2("dragging"), PR_TRUE);

#ifdef REAL_TIME_DRAG
                AdjustChildren(aPresContext);
#else
                MoveSplitterBy(aPresContext, pos);
#endif

            }
            



           // printf("----- resize ----- ");
            /*
            for (i=0; i < mChildInfosBeforeCount; i++) 
               printf("before, index=%d, current=%d, changed=%d\n", mChildInfosBefore[i].index, mChildInfosBefore[i].current, mChildInfosBefore[i].changed);
            for (i=0; i < mChildInfosAfterCount; i++) 
               printf("after, index=%d, current=%d, changed=%d\n", mChildInfosAfter[i].index, mChildInfosAfter[i].current, mChildInfosAfter[i].changed);
            */

            /*
            nsCOMPtr<nsIPresShell> shell;
            aPresContext->GetShell(getter_AddRefs(shell));

            nsCOMPtr<nsIReflowCommand> reflowCmd;
            nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), mOuter->mParent,
                                                  nsIReflowCommand::StyleChanged);
            if (NS_SUCCEEDED(rv)) 
              shell->AppendReflowCommand(reflowCmd);
           
           nsCOMPtr<nsIPresShell> shell;
           aPresContext->GetShell(getter_AddRefs(shell));

           mOuter->mState |= NS_FRAME_IS_DIRTY;
           mOuter->mParent->ReflowDirtyChild(shell, mOuter);
*/
           mDidDrag = PR_TRUE;
    }
}

void
nsSplitterFrameInner::AddListener(nsIPresContext* aPresContext)
{
  nsIFrame* thumbFrame = nsnull;
  mOuter->FirstChild(aPresContext, nsnull,&thumbFrame);
  nsCOMPtr<nsIContent> content;
  mOuter->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,this), NS_GET_IID(nsIDOMMouseListener));
  reciever->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,this), NS_GET_IID(nsIDOMMouseMotionListener));
}

void
nsSplitterFrameInner::RemoveListener()
{
  nsCOMPtr<nsIContent> content;
  mOuter->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,this),NS_GET_IID(nsIDOMMouseListener));
  reciever->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,this),NS_GET_IID(nsIDOMMouseMotionListener));
}

/*
nsresult
nsSplitterFrameInner :: CaptureMouse(nsIPresContext* aPresContext, PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  mOuter->GetView(aPresContext, &view);
  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;
  //nsCOMPtr<nsIWidget> widget;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));
    //view->GetWidget(*getter_AddRefs(widget));
    if (viewMan) {
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
      //  if (widget)
        //   widget->CaptureMouse(PR_TRUE);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
       // if (widget)
         //  widget->CaptureMouse(PR_FALSE);
      }
    }
  }

  return NS_OK;
}


PRBool
nsSplitterFrameInner :: IsMouseCaptured(nsIPresContext* aPresContext)
{
    // get its view
  nsIView* view = nsnull;
  mOuter->GetView(aPresContext, &view);
  nsCOMPtr<nsIViewManager> viewMan;
  
  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
        nsIView* grabbingView;
        viewMan->GetMouseEventGrabber(grabbingView);
        if (grabbingView == view)
          return PR_TRUE;
    }
  }

  return PR_FALSE;
}
*/

nsresult
nsSplitterFrameInner::MouseUp(nsIDOMEvent* aMouseEvent)
{  
  mPressed = PR_FALSE;
  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseDown(nsIDOMEvent* aMouseEvent)
{  
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));

  PRUint16 button = 0;
  mouseEvent->GetButton(&button);

  // only if left button
  if (button != 1)
     return NS_OK;

  nsBoxLayoutState state(mOuter->mPresContext);
  mCurrentPos = 0;
  mPressed = PR_TRUE;

  mDidDrag = PR_FALSE;

  mOuter->GetParentBox(&mParentBox);

  // get our index
  nscoord childIndex = nsFrameNavigator::IndexOf(mOuter->mPresContext, mParentBox, mOuter);
  PRInt32 childCount = nsFrameNavigator::CountFrames(mOuter->mPresContext, mParentBox);

  // if its 0 or the last index then stop right here.
  if (childIndex == 0 || childIndex == childCount-1) {
    mPressed = PR_FALSE;
    return NS_OK;
  }

	EnsureOrient();
  PRBool isHorizontal = !mOuter->IsHorizontal();

  ResizeType resizeBefore = GetResizeBefore();
  ResizeType resizeAfter  = GetResizeAfter();

  delete[] mChildInfosBefore;
  delete[] mChildInfosAfter;

  mChildInfosBefore = new nsSplitterInfo[childCount];
  mChildInfosAfter  = new nsSplitterInfo[childCount];

  // create info 2 lists. One of the children before us and one after.
  PRInt32 count = 0;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;

  nsIBox* childBox = nsnull;
  mParentBox->GetChildBox(&childBox); 

  while (nsnull != childBox) 
  { 
    nsIFrame* childFrame = nsnull;
    childBox->GetFrame(&childFrame);

    nsCOMPtr<nsIContent> content;
    childFrame->GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIAtom> atom;
    nsresult rv;
    NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);

    if (NS_SUCCEEDED(rv) && xblService) {
      PRInt32 dummy;
      xblService->ResolveTag(content, &dummy, getter_AddRefs(atom));
    } else
      content->GetTag(*getter_AddRefs(atom));

    // skip over any splitters
    if (atom.get() != nsXULAtoms::splitter) { 
        nsSize prefSize(0,0);
        nsSize minSize(0,0);
        nsSize maxSize(0,0);
        nscoord flex = 0;

        childBox->GetPrefSize(state, prefSize);
        childBox->GetMinSize(state, minSize);
        childBox->GetMaxSize(state, maxSize);
        nsBox::BoundsCheck(minSize, prefSize, maxSize);

        mOuter->AddMargin(childBox, minSize);
        mOuter->AddMargin(childBox, prefSize);
        mOuter->AddMargin(childBox, maxSize);

        childBox->GetFlex(state, flex);

        nsMargin margin(0,0,0,0);
        childBox->GetMargin(margin);
        nsRect r(0,0,0,0);
        childBox->GetBounds(r);
        r.Inflate(margin);

        if (count < childIndex) {
            mChildInfosBefore[mChildInfosBeforeCount].child   = childBox;
            mChildInfosBefore[mChildInfosBeforeCount].min     = isHorizontal ? minSize.width : minSize.height;
            mChildInfosBefore[mChildInfosBeforeCount].max     = isHorizontal ? maxSize.width : maxSize.height;
            mChildInfosBefore[mChildInfosBeforeCount].current = isHorizontal ? r.width : r.height;
            mChildInfosBefore[mChildInfosBeforeCount].flex    = flex;
            mChildInfosBefore[mChildInfosBeforeCount].index   = count;
            mChildInfosBefore[mChildInfosBeforeCount].changed = mChildInfosBefore[mChildInfosBeforeCount].current;
            mChildInfosBeforeCount++;
        } else if (count > childIndex) {
            mChildInfosAfter[mChildInfosAfterCount].child   = childBox;
            mChildInfosAfter[mChildInfosAfterCount].min     = isHorizontal ? minSize.width : minSize.height;
            mChildInfosAfter[mChildInfosAfterCount].max     = isHorizontal ? maxSize.width : maxSize.height;
            mChildInfosAfter[mChildInfosAfterCount].current = isHorizontal ? r.width : r.height;
            mChildInfosAfter[mChildInfosAfterCount].flex    = flex;
            mChildInfosAfter[mChildInfosAfterCount].index   = count;
            mChildInfosAfter[mChildInfosAfterCount].changed   = mChildInfosAfter[mChildInfosAfterCount].current;
            mChildInfosAfterCount++;
        } 
    }
    
    rv = childBox->GetNextBox(&childBox);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  // if the resizebefore is closest we must reverse the list because the first child in the list
  // is the Farthest we want the first child to be the closest.
  if (resizeBefore == Closest)
     Reverse(mChildInfosBefore, mChildInfosBeforeCount);

  // if the resizeafter is the Farthest we must reverse the list because the first child in the list
  // is the closest we want the first child to be the Farthest.
  if (resizeAfter == Farthest)
     Reverse(mChildInfosAfter, mChildInfosAfterCount);

  // grow only applys to the children after. If grow is set then no space should be taken out of any children after
  // us. To do this we just set the size of that list to be 0.
  if (resizeAfter == Grow)
     mChildInfosAfterCount = 0;

  nsRect vr(0,0,0,0);
  nsIView *v;
  mOuter->GetView(mOuter->mPresContext, &v);
  v->GetBounds(vr);

  PRInt32 c = 0;
  if (isHorizontal) {
     mouseEvent->GetClientX(&c);
     mSplitterPos = mOuter->mRect.x;
     mSplitterViewPos = vr.x;
  } else {
     mouseEvent->GetClientY(&c);
     mSplitterPos = mOuter->mRect.y;
     mSplitterViewPos = vr.y;
  }

  mDragStartPx = c;
    
  //printf("Pressed mDragStartPx=%d\n",mDragStartPx);

  return NS_OK;
}

nsresult
nsSplitterFrameInner::MouseMove(nsIDOMEvent* aMouseEvent)
{  
  //printf("Mouse move\n");

  if (!mPressed)
      return NS_OK;
  
  if (mDragging)
    return NS_OK;

  mOuter->mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, NS_ConvertASCIItoUCS2("dragging"), PR_TRUE);

  RemoveListener();
  mOuter->CaptureMouse(mOuter->mPresContext, PR_TRUE);
  mDragging = PR_TRUE;

  return NS_OK;
}

void
nsSplitterFrameInner::Reverse(nsSplitterInfo*& aChildInfos, PRInt32 aCount)
{
    nsSplitterInfo* infos = new nsSplitterInfo[aCount];

    for (int i=0; i < aCount; i++)
       infos[i] = aChildInfos[aCount - 1 - i];

    delete[] aChildInfos;
    aChildInfos = infos;
}

nsSplitterFrameInner::CollapseDirection
nsSplitterFrameInner::GetCollapseDirection()
{
    nsString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mOuter->mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::collapse, value))
    {
     if (value.EqualsIgnoreCase("before"))
         return Before;
     else if (value.EqualsIgnoreCase("after"))
         return After;
     else 
       return None;
    } else {
        return None;
    }
}

void
nsSplitterFrameInner::UpdateState()
{
  // State Transitions:
  //   Open      -> Dragging
  //   Open      -> Collapsed
  //   Collapsed -> Open
  //   Collapsed -> Dragging
  //   Dragging  -> Open
  //   Dragging  -> Collapsed (auto collapse)

  State newState = GetState(); 

  if (newState == mState) {
    // No change.
	  return;
  }

    CollapseDirection direction = GetCollapseDirection();
    if (direction != None) {
      nsIBox* splitter = mOuter;
      // Find the splitter's immediate sibling.
      nsIBox* splitterSibling =
        nsFrameNavigator::GetChildBeforeAfter(mOuter->mPresContext, splitter,
                                              (direction == Before));
      if (splitterSibling) {
        nsIFrame* splitterSiblingFrame = nsnull;
        splitterSibling->GetFrame(&splitterSiblingFrame);
        nsCOMPtr<nsIContent> sibling;
        if (NS_SUCCEEDED(splitterSiblingFrame->GetContent(getter_AddRefs(sibling)))
            && sibling) {
          if (mState == Collapsed) {
            // Collapsed -> Open
            // Collapsed -> Dragging
            sibling->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::collapsed,
                                    PR_TRUE);
          } else if ((mState == Open || mState == Dragging) 
                     && newState == Collapsed) {
            // Open -> Collapsed
            // Dragging -> Collapsed
            sibling->SetAttribute(kNameSpaceID_None, nsXULAtoms::collapsed,
                                  NS_ConvertASCIItoUCS2("true"), PR_TRUE);
          }
        }
      }
    }
    mState = newState;
  
}

void
nsSplitterFrameInner::EnsureOrient()
{
  nsIFrame* frame = nsnull;
  mParentBox->GetFrame(&frame);
  nsFrameState state;
  frame->GetFrameState(&state);

  PRBool isHorizontal = !(state & NS_STATE_IS_HORIZONTAL);
  if (isHorizontal)
      mOuter->mState |= NS_STATE_IS_HORIZONTAL;
  else
      mOuter->mState &= ~NS_STATE_IS_HORIZONTAL;
}

void
nsSplitterFrameInner::AdjustChildren(nsIPresContext* aPresContext)
{
  EnsureOrient();
  PRBool isHorizontal = !mOuter->IsHorizontal();

  AdjustChildren(aPresContext, mChildInfosBefore, mChildInfosBeforeCount, isHorizontal);
  AdjustChildren(aPresContext, mChildInfosAfter, mChildInfosAfterCount, isHorizontal);
   
  
   // printf("----- Posting Dirty -----\n");

   
#ifdef REAL_TIME_DRAG
//    nsBoxLayoutState state(aPresContext);
  //  state.SetLayoutReason(nsBoxLayoutState::Resize);
    //mParentBox->Layout(state);
  /*
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));

    shell->EnterReflowLock();
    shell->ProcessReflowCommands(PR_TRUE);
    shell->ExitReflowLock(PR_FALSE);
*/

        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        nsIFrame* frame = nsnull;
        mParentBox->GetFrame(&frame);

        /*
        shell->EnterReflowLock();
        nsRect                bounds;
        mParentBox->GetBounds(bounds);
        nsSize                maxSize(bounds.width, bounds.height);
        nsIRenderingContext*  rcx = nsnull;
        nsresult rv = shell->CreateRenderingContext(frame, &rcx);
        nsHTMLReflowState reflowState(aPresContext, frame,
                                      eReflowReason_Resize, rcx, maxSize);
        nsBoxLayoutState state(aPresContext, reflowState);
        mParentBox->Layout(state);
        shell->ExitReflowLock(PR_TRUE);
        */

        shell->FlushPendingNotifications();

        nsCOMPtr<nsIViewManager> viewManager;
        nsIView* view = nsnull;
        frame->GetView(aPresContext, &view);

        nsRect damageRect(0,0,0,0);
        mParentBox->GetContentRect(damageRect);

        if (view) {
          view->GetViewManager(*getter_AddRefs(viewManager));
          viewManager->UpdateView(view, damageRect, NS_VMREFRESH_IMMEDIATE);
    
        } else {
          nsRect    rect(damageRect);
          nsPoint   offset;
  
          frame->GetOffsetFromView(aPresContext, offset, &view);
          NS_ASSERTION(nsnull != view, "no view");
          rect += offset;
          view->GetViewManager(*getter_AddRefs(viewManager));
          viewManager->UpdateView(view, rect, NS_VMREFRESH_IMMEDIATE);
        }

#else 
    //mOuter->mState |= NS_FRAME_IS_DIRTY;
    //mOuter->mParent->ReflowDirtyChild(shell, mOuter->mParent);
    nsBoxLayoutState state(aPresContext);
    mOuter->MarkDirty(state);
#endif

}

void
nsSplitterFrameInner::AdjustChildren(nsIPresContext* aPresContext, nsSplitterInfo* aChildInfos, PRInt32 aCount, PRBool aIsHorizontal)
{
    ///printf("------- AdjustChildren------\n");

    nsBoxLayoutState state(aPresContext);

    nsCOMPtr<nsIPresShell> shell;
    state.GetPresShell(getter_AddRefs(shell));

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    // first set all the widths.
    nsIBox* child = nsnull;
    mOuter->GetChildBox(&child);
    while(child)
    {
      SetPreferredSize(state, child, onePixel, aIsHorizontal, nsnull);
      child->GetNextBox(&child);
    }

    // now set our changed widths.
    for (int i=0; i < aCount; i++) 
    {
        nscoord   pref       = aChildInfos[i].changed;
        nsIBox* childBox     = aChildInfos[i].child;

        SetPreferredSize(state, childBox, onePixel, aIsHorizontal, &pref);
    }
}

void
nsSplitterFrameInner::SetPreferredSize(nsBoxLayoutState& aState, nsIBox* aChildBox, nscoord aOnePixel, PRBool aIsHorizontal, nscoord* aSize)
{
  //printf("current=%d, pref=%d", current/onePixel, pref/onePixel);
 
  nscoord current = 0;

  nsRect rect(0,0,0,0);
  aChildBox->GetBounds(rect);
  if (aIsHorizontal) 
    current = rect.width;
  else
    current = rect.height;

  nscoord pref = 0;

  if (!aSize)
  {
    if (aIsHorizontal) 
      pref = rect.width;
    else
      pref = rect.height;
  } else {
    pref = *aSize;
  }

  nsMargin margin(0,0,0,0);
  aChildBox->GetMargin(margin);

  nsCOMPtr<nsIAtom> attribute;

  if (aIsHorizontal) {
	  pref -= (margin.left + margin.right);
	  attribute = nsHTMLAtoms::width;
  } else {
	  pref -= (margin.top + margin.bottom);
	  attribute = nsHTMLAtoms::height;
  }

  nsIFrame* childFrame = nsnull;
  aChildBox->GetFrame(&childFrame);

  nsCOMPtr<nsIContent> content;
  childFrame->GetContent(getter_AddRefs(content));

  // set its preferred size.
  char ch[50];
  sprintf(ch,"%d",pref/aOnePixel);
  nsAutoString oldValue;
  content->GetAttribute(kNameSpaceID_None, attribute, oldValue);
  if (oldValue.EqualsWithConversion(ch))
     return;

  content->SetAttribute(kNameSpaceID_None, attribute, NS_ConvertASCIItoUCS2(ch), PR_FALSE);
#ifndef REAL_TIME_DRAG
  aChildBox->MarkDirty(aState);
#else
  aChildBox->MarkDirty(aState);
#endif

  //printf("\n");
}


void 
nsSplitterFrameInner::AddRemoveSpace(nscoord aDiff,
                                    nsSplitterInfo* aChildInfos,
                                    PRInt32 aCount,
                                    PRInt32& aSpaceLeft)
{
  aSpaceLeft = 0;

  for (int i=0; i < aCount; i++) {   
    nscoord min    = aChildInfos[i].min;
    nscoord max    = aChildInfos[i].max;
    nscoord& c     = aChildInfos[i].changed;

    // figure our how much space to add or remove
    if (c + aDiff < min) {
     aDiff += (c - min);
     c = min;
    } else if (c + aDiff > max) {
     aDiff -= (max - c);
     c = max;
    } else {
     c += aDiff;
     aDiff = 0;
    }

    // there is not space left? We are done
    if (aDiff == 0)
       break;
  }

  aSpaceLeft = aDiff;
}

#define PR_ABS(x) (x < 0 ? -x : x)

/**
 * Ok if we want to resize a child we will know the actual size in pixels we want it to be.
 * This is not the preferred size. But they only way we can change a child is my manipulating its
 * preferred size. So give the actual pixel size this return method will return figure out the preferred
 * size and set it.
 */

void
nsSplitterFrameInner::ResizeChildTo(nsIPresContext* aPresContext,
                                   nscoord& aDiff, 
                                   nsSplitterInfo* aChildrenBeforeInfos, 
                                   nsSplitterInfo* aChildrenAfterInfos, 
                                   PRInt32 aChildrenBeforeCount, 
                                   PRInt32 aChildrenAfterCount, 
                                   PRBool aBounded)
{ 
  nscoord spaceLeft;
  AddRemoveSpace(aDiff, aChildrenBeforeInfos,aChildrenBeforeCount,spaceLeft);

  // if there is any space left over remove it from the dif we were originally given
  aDiff -= spaceLeft;
  AddRemoveSpace(-aDiff, aChildrenAfterInfos,aChildrenAfterCount,spaceLeft);

  if (spaceLeft != 0) {
    if (aBounded) {
       aDiff += spaceLeft;
       AddRemoveSpace(spaceLeft, aChildrenBeforeInfos,aChildrenBeforeCount,spaceLeft);
    } else {
      spaceLeft = 0;
    }
  }
}


void
nsSplitterFrameInner::MoveSplitterBy(nsIPresContext* aPresContext, nscoord aDiff)
{
 const nsRect& r = mOuter->mRect;
  nsRect vr;
  nsCOMPtr<nsIViewManager> vm;
  nsIView *v;
  mOuter->GetView(aPresContext, &v);
  v->GetViewManager(*getter_AddRefs(vm));
  v->GetBounds(vr);
  nsRect invalid;
  EnsureOrient();
  PRBool isHorizontal = !mOuter->IsHorizontal();
  if (isHorizontal) {
      mOuter->MoveTo(aPresContext, mSplitterPos + aDiff, r.y);
      vm->MoveViewTo(v, mSplitterViewPos + aDiff, vr.y);
      invalid.UnionRect(r,mOuter->mRect);
  } else {
      mOuter->MoveTo(aPresContext, r.x, mSplitterPos + aDiff);
      vm->MoveViewTo(v, vr.x, mSplitterViewPos + aDiff);
      invalid.UnionRect(r,mOuter->mRect);
  }

  // redraw immediately only what changed. This is animation so 
  // it must be immediate.
  nsBoxLayoutState state(aPresContext);
  mParentBox->Redraw(state, &invalid, PR_TRUE);
}



