/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1999.
 * Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Mike Pinkerton
 */

#include "nsTreeItemDragCapturer.h"
#include "nsTreeRowGroupFrame.h"

#include "nsCOMPtr.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"
#include "nsIEventStateManager.h"
#include "nsISupportsPrimitives.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"


NS_IMPL_ADDREF(nsTreeItemDragCapturer)
NS_IMPL_RELEASE(nsTreeItemDragCapturer)
NS_IMPL_QUERY_INTERFACE2(nsTreeItemDragCapturer, nsIDOMEventListener, nsIDOMDragListener )


//
// nsTreeItemDragCapturer ctor
//
// Init member variables. We can't really do much of anything important here because
// any subframes might not be totally intialized yet, or in the hash table
//
nsTreeItemDragCapturer :: nsTreeItemDragCapturer ( nsTreeRowGroupFrame* inGroup, nsIPresContext* inPresContext )
  : mTreeItem(inGroup), mPresContext(inPresContext), mCurrentDropLoc(kNoDropLoc)
{
  NS_INIT_REFCNT();
  
  // we really need this all over the place. just be safe that we have it.
  NS_ASSERTION ( mPresContext, "no pres context set on tree drag listener" );
  NS_ASSERTION ( mTreeItem, "tree item invalid" );
 
} // nsTreeItemDragCapturer ctor


//
// nsTreeItemDragCapturer dtor
//
// Cleanup.
//
nsTreeItemDragCapturer::~nsTreeItemDragCapturer() 
{
}


////////////////////////////////////////////////////////////////////////
nsresult
nsTreeItemDragCapturer::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsTreeItemDragCapturer::DragGesture(nsIDOMEvent* aDragEvent)
{
  // this code should all be in JS.
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsTreeItemDragCapturer::DragEnter(nsIDOMEvent* aDragEvent)
{
  // We don't need to do anything special here. If anything does need to be done, 
  // the code should all be in JS.
  return NS_OK;
}


//
// ComputeDropPosition
//
// Compute where the drop feedback should be drawn and the relative position to the current
// row on a drop. |outBefore| indicates if the drop will go before or after this row. 
// |outDropOnMe| is true if the row has children (it's a folder) and the mouse is in the 
// middle region of the row height. If this is the case, |outBefore| is meaningless. 
// 
void
nsTreeItemDragCapturer :: ComputeDropPosition ( nsIDOMEvent* aDragEvent, nscoord* outYLoc,
                                                      PRBool* outBefore, PRBool* outDropOnMe )
{
  *outYLoc = 0;
  *outBefore = PR_FALSE;
  *outDropOnMe = PR_FALSE;

  //
  // Get the mouse coordinates from the DOM event, but they will be in the 
  // window/widget coordinate system. We must first get them into the frame-relative
  // coordinate system. Yuck.
  //
  
  // get mouse coordinates and translate them into twips
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aDragEvent));
  PRInt32 x,y = 0;
  mouseEvent->GetClientX(&x);
  mouseEvent->GetClientY(&y);
  float p2t;
  mPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  nscoord xp       = NSIntPixelsToTwips(x, p2t);
  nscoord yp       = NSIntPixelsToTwips(y, p2t);
  
  // get the rect of the row (not the tree item) that the mouse is over. This is
  // where we need to start computing things from.
  nsRect rowRect;
  nsIFrame* rowFrame;
  mTreeItem->FirstChild(mPresContext, nsnull, &rowFrame);
  NS_ASSERTION ( rowFrame, "couldn't get rowGroup's row frame" );
  rowFrame->GetRect(rowRect);
   
  // compute the offset to top level in twips
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  PRInt32 frameOffsetX = 0, frameOffsetY = 0;
  nsIFrame* curr = rowFrame;
  curr->GetParent(&curr);
  while ( curr ) {
    nsPoint origin;
    curr->GetOrigin(origin);      // in twips    
    frameOffsetX += origin.x;     // build the offset incrementally
    frameOffsetY += origin.y;    
    curr->GetParent(&curr);       // moving up the chain
  } // until we reach the top  

  // subtract the offset from the mouse coord to put it into row relative coordinates.
  xp -= frameOffsetX;
  yp -= frameOffsetY;
  nsPoint pnt(xp, yp);

  // now check if item is a container
  PRBool isContainer = PR_FALSE;
  nsCOMPtr<nsIContent> treeItemContent;
  mTreeItem->GetContent ( getter_AddRefs(treeItemContent) );
  nsCOMPtr<nsIDOMElement> treeItemNode ( do_QueryInterface(treeItemContent) );
  if ( treeItemNode ) {
    nsAutoString value;
    treeItemNode->GetAttribute(nsAutoString("container"), value);  // can't use an atom here =(
    isContainer = value.Equals("true");
  }
  else
    NS_WARNING("Not a DOM element");

  // if we have a container, the area is broken up into 3 pieces (top, middle, bottom). If
  // it isn't it's only broken up into two (top and bottom)
  if ( isContainer ) {
    if (pnt.y <= (rowRect.y + (rowRect.height / 4))) {
       *outBefore = PR_TRUE;
       *outYLoc = 0;
    } 
    else if (pnt.y >= (rowRect.y + PRInt32(float(rowRect.height) *0.75))) {
      *outBefore = PR_FALSE;
      *outYLoc = rowRect.y + rowRect.height - onePixel;
    } 
    else {
      // we're on the container
      *outDropOnMe = PR_TRUE;
      *outYLoc = kContainerDropLoc;
    }
  } // if row is a container
  else {
    if (pnt.y <= (rowRect.y + (rowRect.height / 2))) {
      *outBefore = PR_TRUE;
      *outYLoc = 0;
    }
    else
      *outYLoc = rowRect.y + rowRect.height - onePixel;
  } // else is not a container

} // ComputeDropPosition


//
// DragOver
//
// The mouse has moved over the tree while a drag is happening. We really just want to
// "annotate" the tree item with the current drop location. We don't want to make any judgement
// as this stage as to whether or not the drag should be accepted or draw any feedback. 
//
nsresult
nsTreeItemDragCapturer::DragOver(nsIDOMEvent* aDragEvent)
{
  // if we are not the target of the event, bail.
  if ( ! IsEventTargetMyTreeItem(aDragEvent) )
    return NS_OK; 

  // Check to see if the drop feedback should be drawn above, below, or on
  // the row (if a container).
  nscoord yLoc = 0;
  PRBool onMe = PR_FALSE;
  PRBool beforeMe = PR_FALSE;
  ComputeDropPosition(aDragEvent, &yLoc, &beforeMe, &onMe);
  if ( yLoc != mCurrentDropLoc ) {
  
    // stash the new location in the treeItem's content. Note that the tree drawing code doesn't
    // care at all about "dd-droplocation", only the coordinate so there is no need to send the
    // AttributeChanged() about that attribute.
    nsCOMPtr<nsIContent> content;
    mTreeItem->GetContent ( getter_AddRefs(content) );
    if ( content ) {
      char buffer[10];

      	// need the cast, because on some platforms, PR[U]int32 != long, but we're using "%ld"
      sprintf(buffer, "%d", yLoc);
      content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddDropLocationCoord, buffer, PR_TRUE );
      content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddDropLocation, beforeMe ? "true" : "false", PR_FALSE );
      content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddDropOn, onMe ? "true" : "false", PR_TRUE );
    }
    
    // cache the current drop location
    mCurrentDropLoc = yLoc;
  }
  
  // NS_OK means event is NOT consumed. We want to make sure JS gets this so it
  // can determine if the drag is allowed.
  return NS_OK; 
}


////////////////////////////////////////////////////////////////////////
nsresult
nsTreeItemDragCapturer::DragExit(nsIDOMEvent* aDragEvent)
{
  // if we are not the target of the event, bail.
  if ( !IsEventTargetMyTreeItem(aDragEvent) )
    return NS_OK;

  nsCOMPtr<nsIContent> content;
  mTreeItem->GetContent ( getter_AddRefs(content) );
  NS_ASSERTION ( content, "can't get content node, we're in trouble" );
  
  if ( content ) {
    // tell the treeItem to not do any more drop feedback. Note that the tree code doesn't
    // care at all about "dd-droplocation", only the coordinate so there is no need to send the
    // AttributeChanged() about that attribute.
    char buffer[10];
    sprintf(buffer, "%d", kNoDropLoc);
    content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddDropLocationCoord, buffer, PR_TRUE );
    content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddDropLocation, "false", PR_TRUE );
    content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddDropOn, "false", PR_TRUE );
    content->SetAttribute ( kNameSpaceID_None, nsXULAtoms::ddTriggerRepaintRestore, "1", PR_TRUE );  
  }
  
  // cache the current drop location
  mCurrentDropLoc = kNoDropLoc;

  return NS_OK; // don't consume event
}



////////////////////////////////////////////////////////////////////////
nsresult
nsTreeItemDragCapturer::DragDrop(nsIDOMEvent* aMouseEvent)
{
  // this code should all be in JS.
  return NS_OK;
}


//
// IsEventTargetMyTreeItem
//
// Since the tree has many nested row groups we will definately see this event several
// times on the way down, but we only care when it gets to the bottom of the chain.
//
PRBool
nsTreeItemDragCapturer :: IsEventTargetMyTreeItem ( nsIDOMEvent* inEvent )
{
  NS_ASSERTION ( inEvent, "null drag event passed to me" );
  nsresult retVal = PR_FALSE;
  
  // get dom node for our associated treeItem frame
  nsCOMPtr<nsIContent> myContent;
  mTreeItem->GetContent ( getter_AddRefs(myContent) );
  nsCOMPtr<nsIDOMNode> myDomNode ( do_QueryInterface(myContent) );

  // get the treeItem associated with the target. Remember that the tree cell is the
  // actual target, so we have to go up two levels to get to its treeItem.
  nsCOMPtr<nsIDOMNode> targetCell;
  inEvent->GetTarget ( getter_AddRefs(targetCell) );
  nsCOMPtr<nsIDOMNode> targetRow;
  nsCOMPtr<nsIDOMNode> targetTreeItem;
  if ( targetCell ) {
    targetCell->GetParentNode(getter_AddRefs(targetRow)); 
    if ( targetRow ) {
      targetRow->GetParentNode(getter_AddRefs(targetTreeItem));
      // the critical comparison. are 
      if ( myDomNode == targetTreeItem )
        retVal = PR_TRUE;
    }
  }

  return retVal;

} // IsEventTargetMyTreeItem
