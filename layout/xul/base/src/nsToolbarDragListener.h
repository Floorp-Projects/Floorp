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
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMEventReceiver.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nscoord.h"


class nsToolbarFrame;
class nsIPresContext;

/** editor Implementation of the DragListener interface
 */
class nsToolbarDragListener : public nsIDOMDragListener, public nsIDOMMouseListener, public nsIDOMMouseMotionListener
{
public:
  /** default constructor
   */
  nsToolbarDragListener();
  /** default destructor
   */
  virtual ~nsToolbarDragListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetToolbar(nsToolbarFrame *aToolbar){mToolbar = aToolbar;}
  void SetPresContext(nsIPresContext *aPresContext){mPresContext = aPresContext;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of dragevent handler interface*/
    virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
public:
  virtual nsresult DragEnter(nsIDOMEvent* aDragEvent);
  virtual nsresult DragOver(nsIDOMEvent* aDragEvent);
  virtual nsresult DragExit(nsIDOMEvent* aDragEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aDragEvent);
/*END implementations of dragevent handler interface*/

/*BEGIN implementations of nsIDOMMouseMotionListener interface*/
public:
  virtual nsresult MouseMove(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragMove(nsIDOMEvent* aMouseEvent);
/*END implementations of nsIDOMMouseMotionListener interface*/

/*BEGIN implementations of nsIDOMMouseListener interface*/
public:
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);
/*END implementations of nsIDOMMouseListener interface*/

protected:
  PRBool IsOnToolbarItem(nsIDOMEvent* aDragEvent, nscoord& aXLoc);

  nsToolbarFrame * mToolbar;
  nsIPresContext * mPresContext;
  PRInt32          mCurrentDropLoc;

  PRBool           mMouseDown;
  PRBool           mMouseDrag;

};


#endif