/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef nsIDOMDragListener_h__
#define nsIDOMDragListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

/*
 * The listener for drag events.
 *
 * The reason for two events for the same operation are for compatibility
 * between the WHAT-WG drag and drop spec and existing XUL code.
 */
#define NS_IDOMDRAGLISTENER_IID \
{ /* 1A107271-1E26-419A-BCF1-0A4CF7A66B45 */ \
0x1a107271, 0x1e26, 0x419a, \
{0xbc, 0xf1, 0x0a, 0x4c, 0xf7, 0xa6, 0x6b, 0x45} }



class nsIDOMDragListener : public nsIDOMEventListener {

public:

   NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOMDRAGLISTENER_IID)

  /**
  * The dragenter event is fired when the mouse is moved from one node onto
  * another. The target is the node that the mouse is moved onto and the
  * related target is the node that the mouse left.
  *
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD DragEnter(nsIDOMEvent* aMouseEvent) = 0;

  /**
  * The dragover event is fired at regular intervals (several times per second)
  * while a drag is occuring. The target of this event is the node that the
  * mouse is over.
  *
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD DragOver(nsIDOMEvent* aMouseEvent) = 0;

  /**
  * The dragleave event is fired when the mouse leaves a node for another
  * node. The dragexit event is fired immediately afterwards which will
  * call this method. The target is the node that the mouse left and the
  * related target is the node that the mouse is entering. A dragenter
  * event will be fired on the node that the mouse is entering after both
  * the dragleave and dragexit event are fired.
  *
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD DragExit(nsIDOMEvent* aMouseEvent) = 0;

  /**
   * The drop event will be fired on the node that the mouse is over once
   * the drag is complete. The dragdrop event will be fired immediately
   * afterwards which will call this method.
   *
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD DragDrop(nsIDOMEvent* aMouseEvent) = 0;
  
  /**
   * When the user begins a drag by pressing the mouse button and moving the
   * mouse slightly, a dragstart event will be fired. Afterwards a draggesture
   * event will be fired which will call this method.
   *
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD DragGesture(nsIDOMEvent* aMouseEvent) = 0;

  /**
   * The dragend event is fired when a drag is finished, whether the data was
   * dropped successfully or whether the drag was cancelled. The target of
   * this event is the source node of the drag.
   *
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD DragEnd(nsIDOMEvent* aMouseEvent) = 0;

  /**
   * The drag event is fired just before a dragover event is fired. The target
   * of this event is the source node of the drag.
   *
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD Drag(nsIDOMEvent* aMouseEvent) = 0;

  // these methods are for compatibility 
  NS_IMETHOD DragStart(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD DragLeave(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD Drop(nsIDOMEvent* aMouseEvent) { return NS_OK; }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMDragListener, NS_IDOMDRAGLISTENER_IID)

#endif // nsIDOMDragListener_h__
