/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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
#include "nsDocument.h"
#include "nsINameSpaceManager.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"

const PRInt32 kMaxZ = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32


static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);

class nsSplitterInfo {
public:
    nscoord min;
    nscoord max;
    nscoord current;
    nscoord changed;
    nsIFrame* child;
    float flex;
    PRInt32 index;
};

class nsSplitterFrameImpl : public nsIDOMMouseListener, public nsIDOMMouseMotionListener {

public:

  NS_DECL_ISUPPORTS

  nsSplitterFrameImpl(nsSplitterFrame* aSplitter)
  {
    mSplitter = aSplitter;
    mRefCnt = 0;
    mPressed = PR_FALSE;
  }

  // mouse listener
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  // mouse motion listener
  virtual nsresult MouseMove(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  void MouseDrag(nsIPresContext& aPresContext, nsGUIEvent* aEvent);

  void AdjustChildren(nsIPresContext& aPresContext);
  void AdjustChildren(nsIPresContext& aPresContext, nsSplitterInfo* aChildInfos, PRInt32 aCount, PRBool aIsHorizontal);

  void AddRemoveSpace(nscoord aDiff,
                    nsSplitterInfo* aChildInfos,
                    PRInt32 aCount,
                    PRInt32& aSpaceLeft);

  void ResizeChildTo(nscoord& aDiff, 
                   nsSplitterInfo* aChildrenBeforeInfos, 
                   nsSplitterInfo* aChildrenAfterInfos, 
                   PRInt32 aChildrenBeforeCount, 
                   PRInt32 aChildrenAfterCount, 
                   PRBool aBounded);

  void UpdateState();

  void AddListener();
  void RemoveListener();

  enum ResizeType { Closest, Farest, Grow };
  enum State { Open, Collapsed, Dragging };

  ResizeType GetResizeBefore();
  ResizeType GetResizeAfter();
  State GetState();

  nsresult CaptureMouse(PRBool aGrabMouseEvents);
  PRBool IsMouseCaptured();
  void Reverse(nsSplitterInfo*& aIndexes, PRInt32 aCount);

  nsSplitterFrame* mSplitter;
  PRBool mDidDrag;
  nscoord mDragStartPx;
  nscoord mCurrentPos;
  nsBoxFrame* mParentBox;
  PRBool mPressed;
  nsSplitterInfo* mChildInfosBefore;
  nsSplitterInfo* mChildInfosAfter;
  PRInt32 mChildInfosBeforeCount;
  PRInt32 mChildInfosAfterCount;
  State mState;
  nsString mCollapsedChildStyle;
  nsCOMPtr<nsIContent> mCollapsedChild;
  nscoord mSplitterPos;

};


static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
NS_IMPL_ISUPPORTS2(nsSplitterFrameImpl, nsIDOMMouseListener, nsIDOMMouseMotionListener)

nsSplitterFrameImpl::ResizeType
nsSplitterFrameImpl::GetResizeBefore()
{
    nsCOMPtr<nsIContent> content;  
    mSplitter->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::resizebefore, value);
    if (value.EqualsIgnoreCase("farest"))
      return Farest;
    else
      return Closest;
}

nsSplitterFrameImpl::ResizeType
nsSplitterFrameImpl::GetResizeAfter()
{
    nsCOMPtr<nsIContent> content;  
    mSplitter->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::resizeafter, value);
    if (value.EqualsIgnoreCase("farest"))
      return Farest;
    else if (value.EqualsIgnoreCase("grow"))
      return Grow;
    else 
      return Closest;
}

nsSplitterFrameImpl::State
nsSplitterFrameImpl::GetState()
{
    nsCOMPtr<nsIContent> content;  
    mSplitter->GetContent(getter_AddRefs(content));

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
NS_NewSplitterFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSplitterFrame* it = new nsSplitterFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSplitterFrame

nsSplitterFrame::nsSplitterFrame()
{
  mImpl = new nsSplitterFrameImpl(this);
  mImpl->AddRef();
  mImpl->mChildInfosAfter = nsnull;
  mImpl->mChildInfosBefore = nsnull;
  mImpl->mState = nsSplitterFrameImpl::Open;
}

nsSplitterFrame::~nsSplitterFrame()
{
  mImpl->Release();
}

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);

/**
 * Anonymous interface
 */
NS_IMETHODIMP
nsSplitterFrame::CreateAnonymousContent(nsISupportsArray& aAnonymousChildren)
{
  // if not content the create some anonymous content
  PRInt32 count = 0;
  mContent->ChildCount(count); 

  if (count == 0) {

    nsCOMPtr<nsIContent> content;
    NS_CreateAnonymousNode(mContent, nsXULAtoms::spring, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_FALSE);
    aAnonymousChildren.AppendElement(content);

    // a grippy
    NS_CreateAnonymousNode(mContent, nsXULAtoms::grippy, nsXULAtoms::nameSpaceID, content);
    aAnonymousChildren.AppendElement(content);

    // create a spring
    NS_CreateAnonymousNode(mContent, nsXULAtoms::spring, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_FALSE);
    aAnonymousChildren.AppendElement(content);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);
  // if the alignment changed. Let the grippy know
  if (aAttribute == nsHTMLAtoms::align) {
     // tell the slider its attribute changed so it can 
     // update itself
     nsIFrame* grippy = nsnull;
     nsScrollbarButtonFrame::GetChildWithTag(nsXULAtoms::grippy, this, grippy);
     if (grippy)
        grippy->AttributeChanged(aPresContext, aChild, aAttribute, aHint);
  } else if (aAttribute == nsXULAtoms::state) {
        mImpl->UpdateState();
  }

  return rv;
}

/**
 * Initialize us. If we are in a box get our alignment so we know what direction we are
 */
NS_IMETHODIMP
nsSplitterFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // find the box we are in
  nsIFrame* box = nsnull;
  nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::box, this, box);

  // if no box get the window because it is a box.
  if (box == nsnull)
      nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::window, this, box);

  // see if the box is horizontal or vertical
  if (box) {
    nsCOMPtr<nsIContent> content;  
    box->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);
    if (value.EqualsIgnoreCase("vertical"))
      mHorizontal = PR_TRUE;
    else 
      mHorizontal = PR_FALSE;
  }

  nsHTMLContainerFrame::CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  nsIView* view;
  GetView(&view);
  view->SetContentTransparency(PR_TRUE);
  view->SetZIndex(kMaxZ);

  mImpl->AddListener();

  return rv;
}

NS_IMETHODIMP 
nsSplitterFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP
nsSplitterFrame::HandlePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleMultiplePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleDrag(nsIPresContext& aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus&  aEventStatus)
{
   return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::HandleRelease(nsIPresContext& aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP  nsSplitterFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                             nsIFrame**     aFrame)
{   
  // if the mouse is captured always return us as the frame.
  if (mImpl->IsMouseCaptured())
  {
    *aFrame = this;
    return NS_OK;
  } else 
    return nsBoxFrame::GetFrameForPoint(aPoint, aFrame);
}

NS_IMETHODIMP
nsSplitterFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
    switch (aEvent->message) {
    case NS_MOUSE_MOVE: 
         mImpl->MouseDrag(aPresContext, aEvent);
    break;
  
/*
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
    case NS_MOUSE_LEFT_BUTTON_DOWN:
        mImpl->mDidDrag = PR_FALSE;
        mImpl->CaptureMouse(PR_TRUE);
    break;
*/

    case NS_MOUSE_RIGHT_BUTTON_UP:
    case NS_MOUSE_LEFT_BUTTON_UP:
      // add our listener back 
      // and pass the event down to the child if we did not
      // drag

      mImpl->AdjustChildren(aPresContext);
      mImpl->AddListener();
      mImpl->CaptureMouse(PR_FALSE);
      mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, "", PR_TRUE);
      mImpl->mPressed = PR_FALSE;
      
      if (!mImpl->mDidDrag) {
      //  nsIView* view = nsnull;
      //  GetView(&view);
      //  nsCOMPtr<nsIViewManager> viewMan;
      //  view->GetViewManager(*getter_AddRefs(viewMan));

     //  return viewMan->HandleEvent(aEvent, aEventStatus);
      }
      
     
    break;
  }

  return nsHTMLContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void
nsSplitterFrameImpl::MouseDrag(nsIPresContext& aPresContext, nsGUIEvent* aEvent)
{
        if (IsMouseCaptured()) {
          PRBool isHorizontal = !mSplitter->IsHorizontal();
           // convert coord to pixels
          nscoord pos = isHorizontal ? aEvent->point.x : aEvent->point.y;

           // mDragStartPx is in pixels and is in our client areas coordinate system. 
           // so we need to first convert it so twips and then get it into our coordinate system.

           // convert start to twips
           nscoord startpx = mDragStartPx;
              
           float p2t;
           aPresContext.GetScaledPixelsToTwips(&p2t);
           nscoord onePixel = NSIntPixelsToTwips(1, p2t);
           nscoord start = startpx*onePixel;

           // get it into our coordintate system by subtracting our parents offsets.
           nsIFrame* parent = mSplitter;
           while(parent != nsnull)
           {
              // if we hit a scrollable view make sure we take into account
              // how much we are scrolled.
              nsIScrollableView* scrollingView;
              nsIView*           view;
              parent->GetView(&view);
              if (view) {
                nsresult result = view->QueryInterface(kScrollViewIID, (void**)&scrollingView);
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

           printf("Diff=%d\n", pos);
           
           ResizeType resizeAfter  = GetResizeAfter();

           PRBool bounded;

           if (resizeAfter == nsSplitterFrameImpl::Grow)
               bounded = PR_FALSE;
           else 
               bounded = PR_TRUE;

		   // nscoord diff = pos - mCurrentPos;
           int i;
           for (i=0; i < mChildInfosBeforeCount; i++) 
               mChildInfosBefore[i].changed = mChildInfosBefore[i].current;
           
           for (i=0; i < mChildInfosAfterCount; i++) 
               mChildInfosAfter[i].changed = mChildInfosAfter[i].current;

            ResizeChildTo(pos, mChildInfosBefore, mChildInfosAfter, mChildInfosBeforeCount, mChildInfosAfterCount, bounded);

			//mCurrentPos = diff + mCurrentPos;

            printf("----- resize ----- ");
            for (i=0; i < mChildInfosBeforeCount; i++) 
               printf("before, index=%d, current=%d, changed=%d\n", mChildInfosBefore[i].index, mChildInfosBefore[i].current, mChildInfosBefore[i].changed);
            for (i=0; i < mChildInfosAfterCount; i++) 
               printf("after, index=%d, current=%d, changed=%d\n", mChildInfosAfter[i].index, mChildInfosAfter[i].current, mChildInfosAfter[i].changed);


            nsCOMPtr<nsIPresShell> shell;
            aPresContext.GetShell(getter_AddRefs(shell));

            nsCOMPtr<nsIReflowCommand> reflowCmd;
            nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), mSplitter->mParent,
                                                  nsIReflowCommand::StyleChanged);
            if (NS_SUCCEEDED(rv)) 
              shell->AppendReflowCommand(reflowCmd);

           mDidDrag = PR_TRUE;
    }
}

void
nsSplitterFrameImpl::AddListener()
{
  nsIFrame* thumbFrame = nsnull;
  mSplitter->FirstChild(nsnull,&thumbFrame);
  nsCOMPtr<nsIContent> content;
  mSplitter->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,this), kIDOMMouseListenerIID);
  reciever->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,this), kIDOMMouseMotionListenerIID);
}

void
nsSplitterFrameImpl::RemoveListener()
{
  nsCOMPtr<nsIContent> content;
  mSplitter->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,this),kIDOMMouseListenerIID);
  reciever->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,this),kIDOMMouseMotionListenerIID);
}

nsresult
nsSplitterFrameImpl :: CaptureMouse(PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  mSplitter->GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
      }
    }
  }

  return NS_OK;
}

PRBool
nsSplitterFrameImpl :: IsMouseCaptured()
{
    // get its view
  nsIView* view = nsnull;
  mSplitter->GetView(&view);
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

nsresult
nsSplitterFrameImpl::MouseUp(nsIDOMEvent* aMouseEvent)
{  
  printf("Released\n");
  mPressed = PR_FALSE;
  return NS_OK;
}

nsresult
nsSplitterFrameImpl::MouseDown(nsIDOMEvent* aMouseEvent)
{  

  mCurrentPos = 0;
  mPressed = PR_TRUE;

  mDidDrag = PR_FALSE;

  nsIFrame* parent = nsnull;
  mSplitter->GetParent(&parent);
  mParentBox = (nsBoxFrame*)parent;

  // get our index
  nscoord childIndex = nsFrameNavigator::IndexOf(mParentBox, mSplitter);
  PRInt32 childCount = nsFrameNavigator::CountFrames(mParentBox);

  // if its 0 or the last index then stop right here.
  if (childIndex == 0 || childIndex == childCount-1) {
    mPressed = PR_FALSE;
    return NS_OK;
  }

  PRBool isHorizontal = mParentBox->IsHorizontal();

  ResizeType resizeBefore = GetResizeBefore();
  ResizeType resizeAfter  = GetResizeAfter();

  delete mChildInfosBefore;
  delete mChildInfosAfter;

  mChildInfosBefore = new nsSplitterInfo[childCount];
  mChildInfosAfter  = new nsSplitterInfo[childCount];

  // create info 2 lists. One of the children before us and one after.
  PRInt32 count = 0;
  mChildInfosBeforeCount = 0;
  mChildInfosAfterCount = 0;

  nsIFrame* childFrame = nsnull;
  mParentBox->FirstChild(nsnull, &childFrame); 

  while (nsnull != childFrame) 
  { 
    nsCOMPtr<nsIContent> content;
    childFrame->GetContent(getter_AddRefs(content));
    nsIAtom* atom;
    content->GetTag(atom);

    // skip over any splitters
    if (atom != nsXULAtoms::splitter) { 
        nsBoxInfo info;
        mParentBox->GetChildBoxInfo(count, info);

        const nsStyleSpacing* spacing;
        nsresult rv = childFrame->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

        NS_ASSERTION(rv == NS_OK,"failed to get spacing info");

        nsMargin margin(0,0,0,0);
        spacing->GetMargin(margin);
        nsRect r(0,0,0,0);
        childFrame->GetRect(r);
        r.Inflate(margin);

        if (count < childIndex) {
            mChildInfosBefore[mChildInfosBeforeCount].child   = childFrame;
            mChildInfosBefore[mChildInfosBeforeCount].min     = isHorizontal ? info.minSize.width : info.minSize.height;
            mChildInfosBefore[mChildInfosBeforeCount].max     = isHorizontal ? info.maxSize.width : info.maxSize.height;
            mChildInfosBefore[mChildInfosBeforeCount].current = isHorizontal ? r.width : r.height;
            mChildInfosBefore[mChildInfosBeforeCount].flex    = info.flex;
            mChildInfosBefore[mChildInfosBeforeCount].index   = count;
            mChildInfosBeforeCount++;
        } else if (count > childIndex) {
            mChildInfosAfter[mChildInfosAfterCount].child   = childFrame;
            mChildInfosAfter[mChildInfosAfterCount].min     = isHorizontal ? info.minSize.width : info.minSize.height;
            mChildInfosAfter[mChildInfosAfterCount].max     = isHorizontal ? info.maxSize.width : info.maxSize.height;
            mChildInfosAfter[mChildInfosAfterCount].current = isHorizontal ? r.width : r.height;
            mChildInfosAfter[mChildInfosAfterCount].flex    = info.flex;
            mChildInfosAfter[mChildInfosAfterCount].index   = count;
            mChildInfosAfterCount++;
        } 
    }
    
    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  // if the resizebefore is closest we must reverse the list because the first child in the list
  // is the farest we want the first child to be the closest.
  if (resizeBefore == Closest)
     Reverse(mChildInfosBefore, mChildInfosBeforeCount);

  // if the resizeafter is the farest we must reverse the list because the first child in the list
  // is the closest we want the first child to be the farest.
  if (resizeAfter == Farest)
     Reverse(mChildInfosAfter, mChildInfosAfterCount);

  // grow only applys to the children after. If grow is set then no space should be taken out of any children after
  // us. To do this we just set the size of that list to be 0.
  if (resizeAfter == Grow)
     mChildInfosAfterCount = 0;

  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(aMouseEvent));

  PRInt32 c = 0;
  if (isHorizontal) {
     uiEvent->GetClientX(&c);
     mSplitterPos = mSplitter->mRect.x;
  } else {
     uiEvent->GetClientY(&c);
     mSplitterPos = mSplitter->mRect.y;
  }

  mDragStartPx = c;
    
  printf("Pressed mDragStartPx=%d\n",mDragStartPx);

  return NS_OK;
}

nsresult
nsSplitterFrameImpl::MouseMove(nsIDOMEvent* aMouseEvent)
{  
  printf("Mouse move\n");

  if (!mPressed)
      return NS_OK;
  
  if (IsMouseCaptured())
    return NS_OK;

  mSplitter->mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::state, "dragging", PR_TRUE);

  RemoveListener();
  CaptureMouse(PR_TRUE);

  return NS_OK;
}

void
nsSplitterFrameImpl::Reverse(nsSplitterInfo*& aChildInfos, PRInt32 aCount)
{
    nsSplitterInfo* infos = new nsSplitterInfo[aCount];

    for (int i=0; i < aCount; i++)
       infos[i] = aChildInfos[aCount - 1 - i];

    delete aChildInfos;
    aChildInfos = infos;
}

void
nsSplitterFrameImpl::UpdateState()
{
  State newState = GetState(); 

  if (newState == mState)
	  return;

   nsString style;

  if (mState == Collapsed) {
    style = mCollapsedChildStyle;
  } else {
    // when clicked see if we are in a splitter. 
    nsIFrame* splitter = mSplitter;

    // get the splitters content node
    nsCOMPtr<nsIContent> content;
    splitter->GetContent(getter_AddRefs(content));

    // get the collapse attribute. If the attribute is not set collapse
    // the child before otherwise collapse the child after
    PRBool before = PR_TRUE;
    nsString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::collapse, value))
    {
     if (value=="after")
       before = PR_FALSE;
    }

    // find the child just in the box just before the splitter. If we are not currently collapsed then
    // then get the childs style attribute and store it. Then set the child style attribute to be display none.
    // if we are already collapsed then set the child's style back to our stored value.
    nsIFrame* child = nsFrameNavigator::GetChildBeforeAfter(splitter,before);
    if (child == nsnull)
      return;

    child->GetContent(getter_AddRefs(mCollapsedChild));

    style = "visibility: collapse";
    mCollapsedChildStyle = "";
    mCollapsedChild->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::style, mCollapsedChildStyle);
  }

  mCollapsedChild->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::style, style, PR_TRUE);

  mState = newState;
}

void
nsSplitterFrameImpl::AdjustChildren(nsIPresContext& aPresContext)
{
  PRBool isHorizontal = mParentBox->IsHorizontal();
  AdjustChildren(aPresContext, mChildInfosBefore, mChildInfosBeforeCount, isHorizontal);
  AdjustChildren(aPresContext, mChildInfosAfter, mChildInfosAfterCount, isHorizontal);
   
  nsCOMPtr<nsIPresShell> shell;
  aPresContext.GetShell(getter_AddRefs(shell));
  
  nsCOMPtr<nsIReflowCommand> reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), mParentBox,
                                        nsIReflowCommand::StyleChanged);
  if (NS_SUCCEEDED(rv)) 
    shell->AppendReflowCommand(reflowCmd);
}

void
nsSplitterFrameImpl::AdjustChildren(nsIPresContext& aPresContext, nsSplitterInfo* aChildInfos, PRInt32 aCount, PRBool aIsHorizontal)
{
    printf("------- AdjustChildren------\n");

    for (int i=0; i < aCount; i++) 
    {
        nscoord   pref       = aChildInfos[i].changed;
        nsIFrame* childFrame = aChildInfos[i].child;
        PRInt32 index        = aChildInfos[i].index;
        nsresult rv;

        const nsStyleSpacing* spacing;
        rv = childFrame->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

        NS_ASSERTION(rv == NS_OK,"failed to get spacing info");

        nsMargin margin(0,0,0,0);
        spacing->GetMargin(margin);
        nsMargin border(0,0,0,0);
        spacing->GetBorderPadding(border);
        margin += border;

        nsIAtom* attribute;

        if (aIsHorizontal) {
	        pref -= (margin.left + margin.right);
	        attribute = nsHTMLAtoms::width;
        } else {
	        pref -= (margin.top + margin.bottom);
	        attribute = nsHTMLAtoms::height;
        }

        float p2t;
        aPresContext.GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

        nsCOMPtr<nsIContent> content;
        childFrame->GetContent(getter_AddRefs(content));

        // set its preferred size.
        char ch[50];
        sprintf(ch,"%d",pref/onePixel);
        printf("index=%d, pref=%s\n", i, ch);
        content->SetAttribute(kNameSpaceID_None, attribute, ch, PR_FALSE);
        mParentBox->SetChildNeedsRecalc(index, PR_TRUE);
    }
}


void 
nsSplitterFrameImpl::AddRemoveSpace(nscoord aDiff,
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
nsSplitterFrameImpl::ResizeChildTo(nscoord& aDiff, 
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

  const nsRect& r = mSplitter->mRect;

  nsRect invalid(0,0,0,0);
  if (mParentBox->IsHorizontal()) {
      mSplitter->MoveTo(mSplitterPos + aDiff, r.y);
      invalid.width = r.width + PR_ABS(aDiff);
      invalid.x = PR_MAX(mSplitterPos,mSplitterPos + aDiff);
  } else {
      mSplitter->MoveTo(r.x, mSplitterPos + aDiff);
      invalid.height = r.height + PR_ABS(aDiff);
      invalid.y = PR_MAX(mSplitterPos, mSplitterPos + aDiff);
  }

  // redraw immediately only what changed. This is animation so 
  // it must be immediate.
  mParentBox->Invalidate(invalid, PR_TRUE);
}

