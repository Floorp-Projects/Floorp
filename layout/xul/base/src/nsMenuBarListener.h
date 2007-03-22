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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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
#ifndef nsMenuBarListener_h__
#define nsMenuBarListener_h__

#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMEventReceiver.h"

class nsMenuBarFrame;
class nsPresContext;
class nsIDOMKeyEvent;

/** editor Implementation of the DragListener interface
 */
class nsMenuBarListener : public nsIDOMKeyListener, public nsIDOMFocusListener, public nsIDOMMouseListener
{
public:
  /** default constructor
   */
  nsMenuBarListener(nsMenuBarFrame* aMenuBar);
  /** default destructor
   */
  virtual ~nsMenuBarListener();
   
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  
  NS_IMETHOD KeyUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aMouseEvent);
  
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
  
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);

  static nsresult GetMenuAccessKey(PRInt32* aAccessKey);
  
  NS_DECL_ISUPPORTS

  static PRBool IsAccessKeyPressed(nsIDOMKeyEvent* event);

protected:
  static void InitAccessKey();

  static PRUint32 GetModifiers(nsIDOMKeyEvent* event);

  nsMenuBarFrame* mMenuBarFrame; // The menu bar object.
  PRBool mAccessKeyDown;         // Whether or not the ALT key is currently down.
  static PRBool mAccessKeyFocuses; // Does the access key by itself focus the menubar?
  static PRInt32 mAccessKey;     // See nsIDOMKeyEvent.h for sample values
  static PRUint32 mAccessKeyMask;// Modifier mask for the access key
};


#endif
