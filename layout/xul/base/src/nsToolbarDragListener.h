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
 
#ifndef nsToolbarDragListener_h__
#define nsToolbarDragListener_h__

#include "nsIDOMDragListener.h"
#include "nsCoord.h"


class nsToolbarFrame;
class nsIPresContext;
class nsIDOMEvent;


class nsToolbarDragListener : public nsIDOMDragListener
{
public:

    // default ctor and dtor
  nsToolbarDragListener ( nsToolbarFrame* inToolbar, nsIPresContext* inPresContext );
  virtual ~nsToolbarDragListener();

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

    // Figure out which child item mouse is over. |outIndex| is the index of the item the object 
    // should be dropped _before_. Therefore if the item should be dropped at the end, the index 
    // will be greater than the number of items in the list. |outOnChild| is true if the item
    // is a container and the drop would be "on" that item.
  void ItemMouseIsOver(nsIDOMEvent* aDragEvent, nscoord* outXLoc, PRUint32* outIndex, PRBool* outOnChild);

  nsToolbarFrame * mToolbar;         // toolbar owns me, don't be circular
  nsIPresContext * mPresContext;     // weak reference
  PRInt32          mCurrentDropLoc;

}; // class nsToolbarDragListener


#endif
