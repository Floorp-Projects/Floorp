/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsNativeScrollbarFrame.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsBoxLayoutState.h"
#include "nsComponentManagerUtils.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsINativeScrollbar.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollbarMediator.h"


//
// NS_NewNativeScrollbarFrame
//
// Creates a new scrollbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewNativeScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsNativeScrollbarFrame* it = new (aPresShell) nsNativeScrollbarFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewNativeScrollbarFrame


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsNativeScrollbarFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


nsNativeScrollbarFrame::nsNativeScrollbarFrame(nsIPresShell* aShell)
  : nsBoxFrame(aShell), mScrollbarNeedsContent(PR_TRUE)
{

}

nsNativeScrollbarFrame::~nsNativeScrollbarFrame ( )
{
  // frame is going away, unhook the native scrollbar from
  // the content node just to be safe about lifetime issues
  nsCOMPtr<nsINativeScrollbar> scrollbar ( do_QueryInterface(mScrollbar) );
  if ( scrollbar )
    scrollbar->SetContent(nsnull, nsnull);
}


//
// Init
//
// Pass along to our parent, but also create the native widget that
// we wrap. 
//
NS_IMETHODIMP
nsNativeScrollbarFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                               nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // create the native widget that we're wrapping, initialize and show it.
  // Don't worry about sizing it, we'll do that later when we're reflowed.
  mScrollbar = do_CreateInstance("@mozilla.org/widget/nativescrollbar;1", &rv);
  NS_ASSERTION(mScrollbar, "Couldn't create native scrollbar!");
  if ( mScrollbar ) {
    nsCOMPtr<nsIWidget> parentWidget;
    GetWindow(aPresContext, getter_AddRefs(parentWidget));
    
    nsCOMPtr<nsIDeviceContext> devContext;
    aPresContext->GetDeviceContext(getter_AddRefs(devContext));
    nsRect bounds(0,0,0,0);
    rv = mScrollbar->Create(parentWidget, bounds, nsnull, devContext);
    mScrollbar->Show(PR_TRUE);
    
    // defer telling the scrollbar about the mediator and the content
    // node until its first reflow since not everything has been set
    // by this point.
    mScrollbarNeedsContent = PR_TRUE;
  }
  
  return rv;
}


//
// FindScrollbar
//
// Walk up the parent frame tree and find the content node of the frame
// with the tag "scrollbar". This is the content node that the GFX Scroll Frame
// is watching for attribute changes.
//
nsresult
nsNativeScrollbarFrame::FindScrollbar(nsIFrame* start, nsIFrame** outFrame, nsIContent** outContent)
{
  *outContent = nsnull;
  *outFrame = nsnull;
  
  while ( start ) {
    start->GetParent(&start);
    if ( start ) {
      // get the content node
      nsCOMPtr<nsIContent> currContent;  
      start->GetContent(getter_AddRefs(currContent));

      nsCOMPtr<nsIAtom> atom;
      if (currContent && currContent->GetTag(*getter_AddRefs(atom)) == NS_OK && atom.get() == nsXULAtoms::scrollbar) {
        *outContent = currContent.get();
        *outFrame = start;
        NS_IF_ADDREF(*outContent);
        return NS_OK;
      }
    }
  }

  return NS_OK;
}


//
// AttributeChanged
//
// We inherit changes to certain attributes from the parent's content node. These
// occur when gecko changes the values of the scrollbar or scrolls the content area
// by some means other than our scrollbar (keyboard, scrollwheel, etc). Update
// our native scrollbar with the correct values.
//
NS_IMETHODIMP
nsNativeScrollbarFrame::AttributeChanged(nsIPresContext* aPresContext, nsIContent* aChild,
                                           PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aModType, 
                                           PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aModType, aHint);
  
  if ( aAttribute == nsXULAtoms::curpos ||  aAttribute == nsXULAtoms::maxpos || 
         aAttribute == nsXULAtoms::pageincrement || aAttribute == nsXULAtoms::increment ) {
    nsAutoString valueStr;
    aChild->GetAttr(aNameSpaceID, aAttribute, valueStr);
    PRInt32 value = atoi(NS_LossyConvertUCS2toASCII(valueStr).get());
    if ( value < 0 )
      value = 1;          // just be safe and sanity check, scrollbar expects unsigned

    nsCOMPtr<nsINativeScrollbar> scrollbar ( do_QueryInterface(mScrollbar) );
    if ( scrollbar ) {
      if ( aAttribute == nsXULAtoms::curpos )
        scrollbar->SetPosition(value);
      else if ( aAttribute == nsXULAtoms::maxpos )
        scrollbar->SetMaxRange(value);
      else if ( aAttribute == nsXULAtoms::pageincrement )   // poorly named, actually the height of the visible view area
        scrollbar->SetViewSize(value);
     else if ( aAttribute == nsXULAtoms::increment )
        scrollbar->SetLineIncrement(value);
    }
  }

  return rv;
}


//
// GetPrefSize
//
// Ask our native widget what dimensions it wants to be, convert them
// back to twips, and tell gecko.
//
NS_IMETHODIMP
nsNativeScrollbarFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  float p2t = 0.0;
  aState.GetPresContext()->GetPixelsToTwips(&p2t);
  
  PRInt32 narrowDimension = 0;
  nsCOMPtr<nsINativeScrollbar> native ( do_QueryInterface(mScrollbar) );
  native->GetNarrowSize(&narrowDimension);
  
  if ( IsVertical() )
    aSize.width = narrowDimension * p2t;
  else
    aSize.height = narrowDimension * p2t;
    
  return NS_OK;
}


//
// EndLayout
//
// Called when the box system is done moving us around. Move our associated widget
// to match the bounds of the frame and, if necessary, finish initializing the
// widget.
//
NS_IMETHODIMP
nsNativeScrollbarFrame::EndLayout(nsBoxLayoutState& aState)
{
  nsresult rv = nsBoxFrame::EndLayout(aState);

  nsRect adjustedRect;
  ConvertToWidgetCoordinates(aState.GetPresContext(), mRect, adjustedRect);       // also converts twips->pixels
  mScrollbar->Resize(adjustedRect.x, adjustedRect.y, adjustedRect.width,
                          adjustedRect.height, PR_TRUE);

  // By now, we have both the content node for the scrollbar and the associated
  // scrollbar mediator (for outliner, if applicable). Hook up the scrollbar to
  // gecko
  if ( mScrollbarNeedsContent ) {
    nsCOMPtr<nsIContent> scrollbarContent;
    nsIFrame* scrollbarFrame = nsnull;
    FindScrollbar(this, &scrollbarFrame, getter_AddRefs(scrollbarContent));
    nsCOMPtr<nsIScrollbarMediator> mediator;
    nsCOMPtr<nsIScrollbarFrame> sb(do_QueryInterface(scrollbarFrame));
    if (sb) {
      sb->GetScrollbarMediator(getter_AddRefs(mediator));
      nsCOMPtr<nsINativeScrollbar> scrollbar(do_QueryInterface(mScrollbar));
      if ( scrollbar ) {
        scrollbar->SetContent(scrollbarContent, mediator);
        mScrollbarNeedsContent = PR_FALSE;
      }
    }     
  }
  
  return rv;
}


//
// ConvertToWidgetCoordinates
//
// Given a rect in frame coordinates (also in twips), convert to a rect (in pixels) that
// is relative to the widget containing this frame. We have to go from the frame to the
// closest view, then from the view to the closest widget.
//
void
nsNativeScrollbarFrame::ConvertToWidgetCoordinates(nsIPresContext* inPresContext, const nsRect & inRect, 
                                                    nsRect & outAdjustedRect)
{
  // Find offset from our view
	nsIView *containingView = nsnull;
	nsPoint	viewOffset(0,0);
	GetOffsetFromView(inPresContext, viewOffset, &containingView);
  NS_ASSERTION(containingView, "No containing view!");
  if ( !containingView )
    return;

  // get the widget associated with the containing view. The offsets we get back
  // are in twips.
  nsCOMPtr<nsIWidget>	aWidget;
  nscoord widgetOffsetX = 0, widgetOffsetY = 0;
  containingView->GetOffsetFromWidget ( &widgetOffsetX, &widgetOffsetY, *getter_AddRefs(aWidget) );
  if (aWidget) {
		float t2p = 1.0;
		inPresContext->GetTwipsToPixels(&t2p);

    // GetOffsetFromWidget() actually returns the _parent's_ offset from its widget, so we
    // still have to add in the offset to |containingView|'s parent ourselves.
    nscoord viewOffsetToParentX = 0, viewOffsetToParentY = 0;
    containingView->GetPosition ( &viewOffsetToParentX, &viewOffsetToParentY );

    // Shift our offset point by offset into our view, the view's offset to its parent,
    // and the parent's offset to the closest widget. Recall that everything that came from
    // the view system is in twips and must be converted.
    nsPoint widgetOffset(0,0);                                
    widgetOffset.MoveBy (NSTwipsToIntPixels(widgetOffsetX + viewOffsetToParentX + viewOffset.x, t2p),
                            NSTwipsToIntPixels(widgetOffsetY + viewOffsetToParentY + viewOffset.y, t2p) );

    outAdjustedRect.x = NSTwipsToIntPixels(inRect.x,t2p);
    outAdjustedRect.y = NSTwipsToIntPixels(inRect.y,t2p);
    outAdjustedRect.MoveBy(widgetOffset.x, widgetOffset.y);
    outAdjustedRect.SizeTo(NSTwipsToIntPixels(inRect.width,t2p), NSTwipsToIntPixels(inRect.height,t2p));
  }

} // ConvertToWidgetCoordinates

