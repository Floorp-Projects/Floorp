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
#include "nsXULAtoms.h"
#include "nsBoxLayoutState.h"
#include "nsComponentManagerUtils.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsINativeScrollbar.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsWidgetsCID.h"
#include "nsINameSpaceManager.h"

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
    scrollbar->SetContent(nsnull, nsnull, nsnull);
}


//
// Init
//
// Pass along to our parent, but also create the native widget that we wrap. 
//
NS_IMETHODIMP
nsNativeScrollbarFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                               nsIFrame* aParent, nsStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // create a view for this frame and then associate the view with the native
  // scrollbar widget. The net result of this is that the view will automatically
  // be resized and moved for us when things reflow, and the widget will follow
  // suit. We don't have to lift a finger!
  static NS_DEFINE_IID(kScrollbarCID,  NS_NATIVESCROLLBAR_CID);
  if ( NS_SUCCEEDED(CreateViewForFrame(aPresContext, this, aContext, PR_TRUE)) ) {
    nsIView* myView = GetView();
    if ( myView ) {
      nsWidgetInitData widgetData;
      if ( NS_SUCCEEDED(myView->CreateWidget(kScrollbarCID, &widgetData, nsnull)) ) {
        mScrollbar = myView->GetWidget();
        if (mScrollbar) {
          mScrollbar->Show(PR_TRUE);
          mScrollbar->Enable(PR_TRUE);

          // defer telling the scrollbar about the mediator and the content
          // node until its first reflow since not everything has been set
          // by this point.
          mScrollbarNeedsContent = PR_TRUE;
        } else {
          NS_WARNING("Couldn't create native scrollbar!");
          return NS_ERROR_FAILURE;
        }
      }
    }
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
nsNativeScrollbarFrame::FindScrollbar(nsIFrame* start, nsIFrame** outFrame,
                                      nsIContent** outContent)
{
  *outContent = nsnull;
  *outFrame = nsnull;
  
  while ( start ) {
    start = start->GetParent();
    if ( start ) {
      // get the content node
      nsIContent* currContent = start->GetContent();

      if (currContent && currContent->Tag() == nsXULAtoms::scrollbar) {
        *outContent = currContent;
        *outFrame = start;
        NS_ADDREF(*outContent);
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeScrollbarFrame::Reflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  // nsGfxScrollFrame may have told us to shrink to nothing. If so, make sure our
  // desired size agrees.
  if (aReflowState.availableWidth == 0) {
    aDesiredSize.width = 0;
  }
  if (aReflowState.availableHeight == 0) {
    aDesiredSize.height = 0;
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
nsNativeScrollbarFrame::AttributeChanged(nsIPresContext* aPresContext,
                                         nsIContent* aChild,
                                         PRInt32 aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                             aNameSpaceID, aAttribute,
                                             aModType);
  
  if (  aAttribute == nsXULAtoms::curpos ||
        aAttribute == nsXULAtoms::maxpos || 
        aAttribute == nsXULAtoms::pageincrement ||
        aAttribute == nsXULAtoms::increment ) {
    nsAutoString valueStr;
    aChild->GetAttr(aNameSpaceID, aAttribute, valueStr);
    
    PRInt32 error;
    PRInt32 value = valueStr.ToInteger(&error);
    if (value < 0)
      value = 1;          // just be safe and sanity check, scrollbar expects unsigned

    nsCOMPtr<nsINativeScrollbar> scrollbar(do_QueryInterface(mScrollbar));
    if (scrollbar) {
      if (aAttribute == nsXULAtoms::maxpos) {
        // bounds check it
        PRUint32 maxValue = (PRUint32)value;
        PRUint32 current;
        scrollbar->GetPosition(&current);
        if (current > maxValue)
        {
          PRInt32 oldPosition = (PRInt32)current;
          PRInt32 curPosition = maxValue;
        
          nsCOMPtr<nsIContent> scrollbarContent;
          nsIFrame* sbFrame = nsnull;
          FindScrollbar(this, &sbFrame, getter_AddRefs(scrollbarContent));
          nsCOMPtr<nsIScrollbarFrame> scrollbarFrame(do_QueryInterface(sbFrame));
          if (scrollbarFrame) {
            nsCOMPtr<nsIScrollbarMediator> mediator;
            scrollbarFrame->GetScrollbarMediator(getter_AddRefs(mediator));
            if (mediator)
              mediator->PositionChanged(scrollbarFrame, oldPosition, /* inout */ curPosition);
          }

          nsAutoString currentStr;
          currentStr.AppendInt(curPosition);
          scrollbarContent->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, currentStr, PR_TRUE);
        }
      }
      
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
  p2t = aState.PresContext()->PixelsToTwips();
  
  PRInt32 narrowDimension = 0;
  nsCOMPtr<nsINativeScrollbar> native ( do_QueryInterface(mScrollbar) );
  if ( !native ) return NS_ERROR_FAILURE;  
  native->GetNarrowSize(&narrowDimension);
  
  if ( IsVertical() )
    aSize.width = nscoord(narrowDimension * p2t);
  else
    aSize.height = nscoord(narrowDimension * p2t);
  
  // By now, we have both the content node for the scrollbar and the associated
  // scrollbar mediator (for outliner, if applicable). Hook up the scrollbar to
  // gecko
  Hookup();
    
  return NS_OK;
}


//
// Hookup
//
// Connect our widget to the content node and/or scrolling mediator. This needs
// to be called late enough in the game where everything is ready. Calling it too
// early can lead to situations where the mediator hasn't yet been hooked up to the
// scrollbar frame
//
void
nsNativeScrollbarFrame::Hookup()
{
  if (!mScrollbarNeedsContent)
    return;

  nsCOMPtr<nsIContent> scrollbarContent;
  nsIFrame* scrollbarFrame = nsnull;
  FindScrollbar(this, &scrollbarFrame, getter_AddRefs(scrollbarContent));

  nsCOMPtr<nsIScrollbarMediator> mediator;
  nsCOMPtr<nsIScrollbarFrame> sb(do_QueryInterface(scrollbarFrame));
  if (!sb) {
    NS_WARNING("ScrollbarFrame doesn't implement nsIScrollbarFrame");
    return;
  }

  sb->GetScrollbarMediator(getter_AddRefs(mediator));
  nsCOMPtr<nsINativeScrollbar> scrollbar(do_QueryInterface(mScrollbar));
  if (!mScrollbar) {
    NS_WARNING("Native scrollbar widget doesn't implement nsINativeScrollbar");
    return;
  }

  scrollbar->SetContent(scrollbarContent, sb, mediator);
  mScrollbarNeedsContent = PR_FALSE;

  if (!scrollbarContent)
    return;

  // Check to see if the curpos attribute is already present on the content
  // node. If so, notify the scrollbar.

  nsAutoString value;
  scrollbarContent->GetAttr(kNameSpaceID_None, nsXULAtoms::curpos, value);

  PRInt32 error;
  PRUint32 curpos = value.ToInteger(&error);
  if (!curpos || error)
    return;

  scrollbar->SetPosition(curpos);
}

