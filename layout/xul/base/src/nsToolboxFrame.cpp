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
// Mike Pinkerton
// Netscape Communications
//
// Significant portions of the collapse/expanding code donated by Chris Lattner
// (sabre@skylab.org). Thanks Chris!
//
// See documentation in associated header file
//

#include "nsToolboxFrame.h"
#include "nsToolbarFrame.h" // needed for MIME definitions

#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsIHTMLReflow.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsINameSpaceManager.h"

#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,  NS_IDOMEVENTRECEIVER_IID);

#include "nsISupportsArray.h"


class nsToolboxFrame::DragListenerDelegate : public nsIDOMDragListener
{
protected:
  nsToolboxFrame* mFrame;

public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent)
  {
    return mFrame ? mFrame->HandleEvent(aEvent) : NS_OK;
  }
  
  virtual nsresult DragGesture(nsIDOMEvent* aEvent)
  {
    return mFrame ? mFrame->DragGesture(aEvent) : NS_OK;
  }

  // nsIDOMDragListener interface
  virtual nsresult DragEnter(nsIDOMEvent* aMouseEvent)
  {
    return mFrame ? mFrame->DragEnter(aMouseEvent) : NS_OK;
  }

  virtual nsresult DragOver(nsIDOMEvent* aMouseEvent)
  {
    return mFrame ? mFrame->DragOver(aMouseEvent) : NS_OK;
  }

  virtual nsresult DragExit(nsIDOMEvent* aMouseEvent)
  {
    return mFrame ? mFrame->DragExit(aMouseEvent) : NS_OK;
  }

  virtual nsresult DragDrop(nsIDOMEvent* aMouseEvent)
  {
    return mFrame ? mFrame->DragDrop(aMouseEvent) : NS_OK;
  }

  // Implementation methods
  DragListenerDelegate(nsToolboxFrame* aFrame) : mFrame(aFrame)
  {
    NS_INIT_REFCNT();
  }

  virtual ~DragListenerDelegate() {}

  void NotifyFrameDestroyed() { mFrame = nsnull; }
};


NS_IMPL_ADDREF(nsToolboxFrame::DragListenerDelegate);
NS_IMPL_RELEASE(nsToolboxFrame::DragListenerDelegate);


NS_IMETHODIMP
nsToolboxFrame::DragListenerDelegate::QueryInterface(REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMDragListener>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsIDOMEventListener>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aResult = NS_STATIC_CAST(nsIDOMDragListener*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }
}



//
// NS_NewToolboxFrame
//
// Creates a new toolbox frame and returns it in |aNewFrame|
//
nsresult
NS_NewToolboxFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsToolboxFrame* it = new nsToolboxFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  //it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewToolboxFrame


//
// nsToolboxFrame cntr
//
// Init, if necessary
//
nsToolboxFrame :: nsToolboxFrame ( )
  : mSumOfToolbarHeights(0), mNumToolbars(0), mGrippyHilighted(kNoGrippyHilighted), mDragListenerDelegate(nsnull),
      kCollapsedAtom(dont_AddRef( NS_NewAtom("collapsed"))), 
      kHiddenAtom(dont_AddRef( NS_NewAtom("hidden")))
{
  // we start off vertical
  mHorizontal = PR_FALSE;
}


//
// nsToolboxFrame dstr
//
// Cleanup, if necessary
//
nsToolboxFrame :: ~nsToolboxFrame ( )
{
  if (mDragListenerDelegate) {
    mDragListenerDelegate->NotifyFrameDestroyed();
    NS_RELEASE(mDragListenerDelegate);
  }
	//еее walk mGrippies and delete elements
}


//
// RefreshStyleContext
//
// Not exactly sure what this does ;)
//
void
nsToolboxFrame :: RefreshStyleContext(nsIPresContext* aPresContext,
                                            nsIAtom *         aNewContentPseudo,
                                            nsCOMPtr<nsIStyleContext>* aCurrentStyle,
                                            nsIContent *      aContent,
                                            nsIStyleContext*  aParentStyle)
{
  nsIStyleContext* newStyleContext;
  aPresContext->ProbePseudoStyleContextFor(aContent,
                                           aNewContentPseudo,
                                           aParentStyle,
                                           PR_FALSE,
                                           &newStyleContext);
  if (newStyleContext != aCurrentStyle->get())
    *aCurrentStyle = dont_QueryInterface(newStyleContext);
    
} // RefreshStyleContext


//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsToolboxFrame :: ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                          PRInt32 aParentChange, nsStyleChangeList* aChangeList,
                                          PRInt32* aLocalChange)
{
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = nsBoxFrame::ReResolveStyleContext(aPresContext, aParentContext, 
                                                            aParentChange, aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {
     UpdateStyles(aPresContext);
  }
  
  return rv;
  
} // ReResolveStyleContext

NS_IMETHODIMP
nsToolboxFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  UpdateStyles(&aPresContext);

  // Register the delegate as a drag listener.
  mDragListenerDelegate = new DragListenerDelegate(this);
  if (! mDragListenerDelegate)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(mDragListenerDelegate);

  nsCOMPtr<nsIContent> content;
  rv = GetContent(getter_AddRefs(content));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  if (NS_OK == reciever->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMDragListener*, mDragListenerDelegate), nsIDOMDragListener::GetIID())) {
    printf("Toolbar registered as Drag Listener\n");
  }

  return rv;
}

void
nsToolboxFrame::UpdateStyles(nsIPresContext*  aPresContext)
{
    nsCOMPtr<nsIAtom> grippyRolloverPseudo ( dont_AddRef(NS_NewAtom(":toolbox-rollover")) );
    RefreshStyleContext(aPresContext, grippyRolloverPseudo, &mGrippyRolloverStyle, mContent, mStyleContext);

    nsCOMPtr<nsIAtom> grippyNormalPseudo ( dont_AddRef(NS_NewAtom(":toolbox-normal")) );
    RefreshStyleContext(aPresContext, grippyNormalPseudo, &mGrippyNormalStyle, mContent, mStyleContext);
}


//
// Paint
//
// Paint our background and border like normal frames, but before we draw the
// children, draw our grippies for each toolbar.
//
NS_IMETHODIMP
nsToolboxFrame :: Paint ( nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  // if we aren't visible then we are done.
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
  mStyleContext->GetStyleData(eStyleStruct_Display);

  if (!disp->mVisible) 
	   return NS_OK;  

  // take care of bg painting, borders and children
  nsresult retVal = nsBoxFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );

  // now draw what makes us special
  DrawGrippies ( aPresContext, aRenderingContext );
  
  return retVal;
  
} // Paint


//
// DrawGrippies
//
// Redraws all the grippies in the toolbox by iterating over each toolbar in the DOM 
// and figuring out how to draw the grippies based on size/visibility information
// 
void
nsToolboxFrame :: DrawGrippies (  nsIPresContext& aPresContext, nsIRenderingContext & aRenderingContext ) const
{
  for ( PRInt32 i = 0; i < mGrippies.Count(); ++i ) {
    TabInfo* currGrippy = NS_STATIC_CAST(TabInfo*, mGrippies[i]);
    
    PRBool hilight = (mGrippyHilighted == i) ? PR_TRUE : PR_FALSE;
    DrawGrippy ( aPresContext, aRenderingContext,  currGrippy->mBoundingRect, hilight );   
  } // for each child

} // DrawGrippies


//
// DrawGrippy
//
// Draw a single grippy in the given rectangle, either with or without rollover feedback.
//
void
nsToolboxFrame :: DrawGrippy (  nsIPresContext& aPresContext, nsIRenderingContext & aRenderingContext,
                                  const nsRect & aBoundingRect, PRBool aDrawHilighted ) const
{
  nsCOMPtr<nsIStyleContext> style ( aDrawHilighted ? mGrippyRolloverStyle : mGrippyNormalStyle ) ;
  if ( !mGrippyRolloverStyle ) {
    #ifdef NS_DEBUG
    printf("nsToolboxFrame::DrawGrippy() -- style context null, css file not loaded correctly??\n");
    #endif
    return;   // something must be seriously wrong
  }

  const nsStyleColor*   grippyColor   = (const nsStyleColor*)style->GetStyleData(eStyleStruct_Color);
  const nsStyleSpacing* grippySpacing = (const nsStyleSpacing*)style->GetStyleData(eStyleStruct_Spacing);
//  const nsStyleFont*    grippyFont    = (const nsStyleFont*)style->GetStyleData(eStyleStruct_Font);

  nsToolboxFrame* nonConstSelf = NS_CONST_CAST(nsToolboxFrame*, this);
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, nonConstSelf,
                                    aBoundingRect, aBoundingRect, *grippyColor, *grippySpacing, 0, 0);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, nonConstSelf,
                                aBoundingRect, aBoundingRect, *grippySpacing, style, 0);

} // DrawGrippy

NS_IMETHODIMP 
nsToolboxFrame :: Reflow(nsIPresContext&          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  // compute amount (in twips) each toolbar will be offset from the right because of 
  // the grippy
  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  nscoord grippyWidth = kGrippyWidthInPixels * onePixel;   // remember to leave room for the grippy on the right
  nscoord collapsedGrippyHeight = kCollapsedGrippyHeightInPixels * onePixel;
  nscoord collapsedGrippyWidth  = kCollapsedGrippyWidthInPixels  * onePixel;

  const PRBool isHorz = IsHorizontal();

  // Leave room at the side for the grippies of visible toolbars. Make a margin 
  // of the appropriate dimensions.
  mInset = nsMargin(0,0,0,0);
  if ( isHorz )     // Set margins so we have a place for uncollapsed grippies
     mInset.top = grippyWidth;
  else
     mInset.left = grippyWidth;

  // Save old tabs so we can make newly collapsed bars as wide as they WERE tall. We will
  // release the grippies in mGrippies at the very end when we dispose of |oldGrippies|
  nsVoidArray oldGrippies, emptyList;
  oldGrippies = mGrippies;
  mGrippies = emptyList;

  
  // ----- Calculate a new set of grippy states...

 
  // iterate over each content node to see if we can find one with the "collapsed"
  // attribute set. If so, we have a collapsed toolbar.
  nsAutoString value;
  int numCollapsedGrippies = 0;
  nscoord grippyPos = 0;

  // Get the first child of the toolbox content node
  unsigned int contentCounter = 0;
  nsCOMPtr<nsIContent> childContent;
  nsresult errCode = mContent->ChildAt(contentCounter, *getter_AddRefs(childContent));
  NS_ASSERTION(errCode == NS_OK,"failed to get first child");    
  
  // iterate over each content node to see if we can find one with the "collapsed"
  // attribute set. If so, we have a collapsed toolbar.
  while ( childContent ) {      
    // is this bar collapsed?
    value = "";
    childContent->GetAttribute(kNameSpaceID_None, kCollapsedAtom, value);

    if (value == "true") {  // The bar is collapsed!
      nscoord grippyWidth;
      nscoord grippyHeight;

      if ( isHorz ) {
        grippyWidth = collapsedGrippyHeight;
        grippyHeight = collapsedGrippyWidth;
      } else {
        grippyWidth = collapsedGrippyWidth;
        grippyHeight = collapsedGrippyHeight;
      }

      TabInfo* oldGrippy = FindGrippyForToolbar ( oldGrippies, childContent );
      if ( oldGrippy ) {  // Inherit the old size...
        if ( isHorz ) {     // If laying out children horizontally...
          if ( oldGrippy->mCollapsed)    // Did it used to be collapsed? 
            grippyHeight = oldGrippy->mBoundingRect.height;  // Copy old width
          else
            grippyHeight = oldGrippy->mBoundingRect.width; // Else copy old height
        } else {            // If laying out children vertically...
          if ( oldGrippy->mCollapsed )    // Did it used to be collapsed? 
            grippyWidth = oldGrippy->mBoundingRect.width;  // Copy old width
          else                       
            grippyWidth = oldGrippy->mBoundingRect.height; // Else copy old height
        }
      }

      if ( isHorz ) {
        mGrippies.AppendElement( new TabInfo(childContent, PR_TRUE, 
                                               nsRect(0, grippyPos, grippyWidth, grippyHeight)) );
        grippyPos += grippyHeight;
      } else {
        mGrippies.AppendElement( new TabInfo(childContent, PR_TRUE, 
                                               nsRect(grippyPos, 0, grippyWidth, grippyHeight)) );
        grippyPos += grippyWidth;
      }

      ++numCollapsedGrippies;
    } else                // The bar is NOT collapsed!!
      mGrippies.AppendElement( new TabInfo(childContent, PR_FALSE) );
    
    // next!
    ++contentCounter;
    errCode = mContent->ChildAt(contentCounter, *getter_AddRefs(childContent));
    NS_ASSERTION(errCode == NS_OK,"failed to get next child");    
  }

  // if there are any collapsed bars, we need to leave room at the bottom of
  // the box for the grippies. Adjust the margins before we reflow the box.
  if ( numCollapsedGrippies ) {
    if ( isHorz )
       mInset.left = collapsedGrippyHeight;
    else
       mInset.bottom = collapsedGrippyHeight;
  }
  
  
  // ===== Reflow things =====
  errCode = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  NS_ASSERTION(errCode == NS_OK,"box reflow failed");


  // -----set all the grippy locations-----

  
  // iterate over all visible toolbar frames, moving the associated grippy
  // next to the toolbar
  mNumToolbars = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while ( childFrame ) {    
    // get the childs rect and figure out the grippy size
    nsCOMPtr<nsIContent> childContent;
    childFrame->GetContent(getter_AddRefs(childContent));

    nsRect grippyRect;
    childFrame->GetRect(grippyRect);

    if ( isHorz ) {
      grippyRect.y = 0;
      grippyRect.height = grippyWidth;
    } else {
      grippyRect.x = 0;
      grippyRect.width = grippyWidth;
    }

    TabInfo *grippyInfo = FindGrippyForToolbar(mGrippies, childContent);
    NS_ASSERTION(grippyInfo != 0, "Grippy Info Struct dissapeared!");
    NS_ASSERTION(grippyInfo->mCollapsed == 0, "Collapsed toolbar has frame!");

    // Set the location of the grippy to the left...
    grippyInfo->SetBounds(grippyRect);

    errCode = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(errCode == NS_OK, "failed to get next child");
    mNumToolbars++;
  }

  // now move collapsed grippies to the bottom
  for ( PRInt32 i = 0; i < mGrippies.Count(); ++i ) {
    TabInfo* currGrippy = NS_STATIC_CAST(TabInfo*, mGrippies[i]);
    if (currGrippy->mCollapsed) {
      // remember we are just inverting the coord system here so in a
      // horzontal toolbox our height is our width. Thats why we just use
      // height here on both x and y coords.
      if ( isHorz )
        currGrippy->mBoundingRect.x = aDesiredSize.width - collapsedGrippyHeight;
      else
        currGrippy->mBoundingRect.y = aDesiredSize.height - collapsedGrippyHeight;
    }
  }
  
  // make sure we now dispose of the old grippies since we have allocated
  // new ones.
  ClearGrippyList ( oldGrippies );

  return errCode;
} // Reflow


//
// FindGrippyForToolbar
//
// Utility routine to scan through the grippy list looking for one in the list
// associated with the given content object (which is the toolbar's content object).
//
// Will return nsnull if it cannot find the toolbar.
//
nsToolboxFrame::TabInfo*
nsToolboxFrame :: FindGrippyForToolbar ( nsVoidArray & inList, const nsIContent* inContent ) const
{
  for ( PRInt32 i = 0; i < inList.Count(); ++i ) {
    TabInfo* currGrippy = NS_STATIC_CAST(TabInfo*, inList[i]);
    if ( currGrippy->mToolbar == inContent )
      return currGrippy;
  }
  return nsnull;

} // FindGrippyForToolbar


//
// ClearGrippyList
//
// Since we are assuming the array "owns" the grippies once they go in there,
// we need to make sure that when the list goes away that they are cleaned up. The
// nsVoidArray does not know how to do this, so we need to do it ourselves.
//
void
nsToolboxFrame :: ClearGrippyList ( nsVoidArray & inList )
{
  for ( PRInt32 i = 0; i < inList.Count(); ++i ) {
    TabInfo* currGrippy = NS_STATIC_CAST(TabInfo*, inList[i]);
	delete currGrippy;
  }
  
} // ClearGrippyList


//
// GetInset
//
// Our Reflow() method computes a margin for the grippies and for collased grippies (if
// any). Return this pre-computed margin when asked by the box.
//
void
nsToolboxFrame::GetInset(nsMargin& margin)
{
   margin = mInset;
}


//
// GetFrameForPoint
//
// Override to process events in our own frame
//
NS_IMETHODIMP
nsToolboxFrame :: GetFrameForPoint(const nsPoint& aPoint, 
                                  nsIFrame**     aFrame)
{
  nsresult retVal = nsHTMLContainerFrame::GetFrameForPoint(aPoint, aFrame);

  // returning NS_OK means that we tell the frame finding code that we have something
  // and to stop looking elsewhere for a frame.
  if ( aFrame && *aFrame == this )
    retVal = NS_OK;
  else if ( retVal != NS_OK ) {
    *aFrame = this;
    retVal = NS_OK;
  }
     
  return retVal;
  
} // GetFrameForPoint


//
// HandleEvent
//
// 
NS_IMETHODIMP
nsToolboxFrame :: HandleEvent ( nsIPresContext& aPresContext, 
                                   nsGUIEvent*     aEvent,
                                   nsEventStatus&  aEventStatus)
{
  if ( !aEvent )
    return nsEventStatus_eIgnore;
 
  switch ( aEvent->message ) {

    case NS_MOUSE_LEFT_CLICK:
      // BUG 3752 aEvent->point is invalid here.
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      OnMouseLeftClick ( aEvent->point );
      break;
    
    case NS_MOUSE_MOVE:
      OnMouseMove ( aEvent->point );
      break;
      
    case NS_MOUSE_EXIT:
      OnMouseExit ( );
      break;

    case NS_DRAGDROP_ENTER: 
      // show drop feedback 
      break; 

    case NS_DRAGDROP_OVER: 
      break; 

    case NS_DRAGDROP_EXIT: 
      // remove drop feedback 
      break; 

    case NS_DRAGDROP_DROP: 
      // do drop coolness stuff 
      break; 

    default:
      break;

  } // case of which event

  return nsEventStatus_eIgnore;
  
} // HandleEvent


//
// ConvertToLocalPoint
//
// Given a point in the coordinate system of the parent view, convert to a point in the
// frame's local coordinate system.
// 
void
nsToolboxFrame :: ConvertToLocalPoint ( nsPoint & ioPoint )
{
  nsIView* view = nsnull;             // note: |view| not AddRef'd
  nsPoint offset; 
  if ( GetOffsetFromView(offset, &view) == NS_OK )
    ioPoint -= offset;

} // ConvertToLocalPoint


//
// OnMouseMove
//
// Handle mouse move events for hilighting and unhilighting the grippies. |aMouseLoc|
// is not in local frame coordinates.
//
void
nsToolboxFrame :: OnMouseMove ( nsPoint & aMouseLoc )
{
  nsPoint localMouseLoc = aMouseLoc;
  ConvertToLocalPoint ( localMouseLoc );
    
  for ( int i = 0; i < mGrippies.Count(); ++i ) {
    TabInfo* currGrippy = NS_STATIC_CAST(TabInfo*, mGrippies[i]);
    if ( currGrippy->mBoundingRect.Contains(localMouseLoc) ) {
      if ( i != mGrippyHilighted ) {
        // unhilight the old one
        if ( mGrippyHilighted != kNoGrippyHilighted ) {
          TabInfo* hilightedGrippy = NS_STATIC_CAST(TabInfo*, mGrippies[mGrippyHilighted]);
          Invalidate ( hilightedGrippy->mBoundingRect, PR_FALSE );
	    }
	    		
        // hilight the new one and remember it
        mGrippyHilighted = i;
        Invalidate ( currGrippy->mBoundingRect, PR_FALSE );
      } // if in a new tab
    }
  } // for each toolbar

} // OnMouseMove


//
// OnMouseLeftClick
//
// Check if a click is in a grippy and expand/collapse appropriately. |aMouseLoc|
// is not in local frame coordinates.
//
void
nsToolboxFrame :: OnMouseLeftClick ( nsPoint & aMouseLoc )
{
  nsPoint localMouseLoc = aMouseLoc;
  ConvertToLocalPoint ( localMouseLoc );
  
  for ( int i = 0; i < mGrippies.Count(); ++i ) {
    TabInfo* currGrippy = NS_STATIC_CAST(TabInfo*, mGrippies[i]);			
    if ( currGrippy->mBoundingRect.Contains(localMouseLoc) ) {
      if ( currGrippy->mCollapsed )
        ExpandToolbar ( *currGrippy );
      else
        CollapseToolbar ( *currGrippy );
			
      // don't keep repeating this process since toolbars have now be
      // relaid out and a new toolbar may be under the current mouse
      // location!
      break;
    }
  }
	
} // OnMouseLeftClick


//
// OnMouseExit
//
// Update the grippies that may have been hilighted while the mouse was within the
// manager.
//
void
nsToolboxFrame :: OnMouseExit ( )
{
	if ( mGrippyHilighted != kNoGrippyHilighted ) {
	    TabInfo* hilightedGrippy = NS_STATIC_CAST(TabInfo*, mGrippies[mGrippyHilighted]);
		Invalidate ( hilightedGrippy->mBoundingRect, PR_FALSE );
		mGrippyHilighted = kNoGrippyHilighted;
	}

} // OnMouseExit



//
// CollapseToolbar
//
// Given the tab that was clicked on, collapse its corresponding toolbar. This
// assumes that the tab is expanded.
//
void
nsToolboxFrame :: CollapseToolbar ( TabInfo & inTab ) 
{
  if ( inTab.mToolbar ) {
    #ifdef NS_DEBUG
      printf("CollapseToolbar:: collapsing\n");
    #endif
    nsresult errCode = inTab.mToolbar->SetAttribute ( kNameSpaceID_None, kCollapsedAtom, "true", PR_TRUE );
    #ifdef NS_DEBUG
    if ( errCode )
      printf("Problem setting collapsed attribute while collapsing toolbar\n");
    #endif
  }
   
} // CollapseToolbar


//
// ExpandToolbar
//
// Given the collapsed (horizontal) tab that was clicked on, expand its
// corresponding toolbar. This assumes the tab is collapsed.
//
void
nsToolboxFrame :: ExpandToolbar ( TabInfo & inTab ) 
{
//  nsresult errCode = mContent->UnsetAttribute ( kNameSpaceID_None, kCollapsedAtom, PR_TRUE );
  nsresult errCode = inTab.mToolbar->SetAttribute ( kNameSpaceID_None, kCollapsedAtom, "false", PR_TRUE );
  #ifdef NS_DEBUG
  if ( errCode )
    printf("Problem clearing collapsed attribute while expanding toolbar\n");
  #endif

} // ExpandToolbar


////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::HandleEvent(nsIDOMEvent* aEvent)
{
  //printf("nsToolbarDragListener::HandleEvent\n");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragEnter(nsIDOMEvent* aDragEvent)
{
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    nsAutoString toolbarFlavor(TOOLBAR_MIME);
    if (dragSession && (NS_OK == dragSession->IsDataFlavorSupported(&toolbarFlavor))) {
      dragSession->SetCanDrop(PR_TRUE);
      rv = NS_ERROR_BASE; // consume event
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } else {
    rv = NS_OK;
  }
  return rv; 
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragOver(nsIDOMEvent* aDragEvent)
{
  // now tell the drag session whether we can drop here
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                           nsIDragService::GetIID(),
                                           (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
    nsAutoString toolbarFlavor(TOOLBAR_MIME);
    if (dragSession && NS_OK == dragSession->IsDataFlavorSupported(&toolbarFlavor)) {

      // Right here you need to figure out where the mouse is 
      // and whether you can drop here

      dragSession->SetCanDrop(PR_TRUE);
      rv = NS_ERROR_BASE; // consume event
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  }

  // NS_OK means event is NOT consumed
  return rv; 
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragExit(nsIDOMEvent* aDragEvent)
{
  return NS_ERROR_BASE; // consumes event
}



////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragDrop(nsIDOMEvent* aMouseEvent)
{
  // String for doing paste
  nsString stuffToPaste;

  // Create drag service for getting state of drag
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
  
    if (dragSession) {

      // Create transferable for getting the drag data
      nsCOMPtr<nsITransferable> trans;
      rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), 
                                              (void**) getter_AddRefs(trans));
      if ( NS_SUCCEEDED(rv) && trans ) {
        // Add the text Flavor to the transferable, 
        // because that is the only type of data we are
        // looking for at the moment.
        nsAutoString toolbarMime (TOOLBAR_MIME);
        trans->AddDataFlavor(&toolbarMime);
        //trans->AddDataFlavor(mImageDataFlavor);

        // Fill the transferable with data for each drag item in succession
        PRUint32 numItems = 0; 
        if (NS_SUCCEEDED(dragSession->GetNumDropItems(&numItems))) { 

          //printf("Num Drop Items %d\n", numItems); 

          PRUint32 i; 
          for (i=0;i<numItems;++i) {
            if (NS_SUCCEEDED(dragSession->GetData(trans, i))) { 
 
              // Get the string data out of the transferable
              // Note: the transferable owns the pointer to the data
              char *str = 0;
              PRUint32 len;
              trans->GetAnyTransferData(&toolbarMime, (void **)&str, &len);

              // If the string was not empty then paste it in
              if (str) {
                char buf[256];
                strncpy(buf, str, len);
                buf[len] = 0;
                printf("Dropped: %s\n", buf);
                stuffToPaste.SetString(str, len);
                dragSession->SetCanDrop(PR_TRUE);
              }
            }
          } // foreach drag item
        }
      } // if valid transferable
    } // if valid drag session
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } // if valid drag service

  return NS_ERROR_BASE; // consume the event;
}


