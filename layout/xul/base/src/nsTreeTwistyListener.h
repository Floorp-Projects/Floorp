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
#ifndef nsTreeTwistyListener_h__
#define nsTreeTwistyListener_h__

#include "nsIDOMMouseListener.h"
#include "nsIDOMEventReceiver.h"

class nsIPresContext;

/** editor Implementation of the DragListener interface
 */
class nsTreeTwistyListener : public nsIDOMMouseListener
{
public:
  /** default constructor
   */
  nsTreeTwistyListener();
  /** default destructor
   */
  virtual ~nsTreeTwistyListener();
   
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  
  NS_DECL_ISUPPORTS

protected:
};


#endif
