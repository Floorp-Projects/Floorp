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

#ifndef nsTreeItemDragCapturer_h__
#define nsTreeItemDragCapturer_h__

#include "nsIDOMDragListener.h"
#include "nsCoord.h"


class nsIPresContext;
class nsIDOMEvent;
class nsIFrame;
class nsXULTreeGroupFrame;
class nsPoint;
class nsRect;

class nsTreeItemDragCapturer : public nsIDOMDragListener
{
public:

    // special values for the Y value of the drop location.
  enum { 
    kNoDropLoc = -2,            // unset, no drop feedback to be drawn.
    kContainerDropLoc = -1      // a container Y is meaningless, but indicates feedback is still needed
  } ;
  
    // default ctor and dtor
  nsTreeItemDragCapturer ( nsXULTreeGroupFrame* inTreeItem, nsIPresContext* inPresContext );
  virtual ~nsTreeItemDragCapturer();

    // interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

    // nsIDOMDragListener
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  virtual nsresult DragEnter(nsIDOMEvent* aDragEvent);
  virtual nsresult DragOver(nsIDOMEvent* aDragEvent);
  virtual nsresult DragExit(nsIDOMEvent* aDragEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aDragEvent);
  virtual nsresult DragGesture(nsIDOMEvent* aDragEvent);

protected:

    // Compute where the drop feedback should be drawn and the relative position to the current
    // row on a drop. |outBefore| indicates if the drop will go before or after this row. 
    // |outDropOnMe| is true if the row has children (it's a folder) and the mouse is in the 
    // middle region of the row height. If this is the case, |outBefore| is meaningless. 
  void ComputeDropPosition(nsIDOMEvent* aDragEvent, nscoord* outYLoc, PRBool* outBefore, PRBool* outDropOnMe);

    // Take the mouse coordinates from the DOM event and convert them into frame-relative
    // coordinates.
  void ConvertEventCoordsToRowCoords ( nsIDOMEvent* inDragEvent, nsPoint* outCoords,
                                          nsRect* outRowRect ) ;
  
    // Since there are so many nested tree items, we have to weed out when the event is not
    // really for us.
  PRBool IsEventTargetMyTreeItem ( nsIDOMEvent* inEvent ) ;
  
  nsXULTreeGroupFrame* mTreeItem;    // rowGroup owns me, don't be circular
  nsIPresContext*  mPresContext;     // weak reference
  PRInt32          mCurrentDropLoc;

}; // class nsTreeItemDragCapturer


#endif
