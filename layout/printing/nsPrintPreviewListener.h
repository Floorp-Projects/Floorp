/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#ifndef nsPrintPreviewListener_h__
#define nsPrintPreviewListener_h__

// Interfaces needed to be included
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
// Helper Classes
#include "nsIDOMEventReceiver.h"
#include "nsCOMPtr.h"

#define REG_NONE_LISTENER        0x00
#define REG_CONTEXTMENU_LISTENER 0x01
#define REG_KEY_LISTENER         0x02
#define REG_MOUSE_LISTENER       0x04
#define REG_MOUSEMOTION_LISTENER 0x08

//
// class nsPrintPreviewListener
//
// The class that listens to the chrome events and tells the embedding
// chrome to show context menus, as appropriate. Handles registering itself
// with the DOM with AddChromeListeners() and removing itself with
// RemoveChromeListeners().
//
class nsPrintPreviewListener : public nsIDOMContextMenuListener,
                                public nsIDOMKeyListener,
                                public nsIDOMMouseListener,
                                public nsIDOMMouseMotionListener

{
public:
  NS_DECL_ISUPPORTS
  
  nsPrintPreviewListener(nsIDOMEventReceiver* aEVRec);
  virtual ~nsPrintPreviewListener()
  {
  }

  // nsIDOMContextMenuListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent)        {	return NS_OK; }
  NS_IMETHOD ContextMenu (nsIDOMEvent* aEvent)       { aEvent->PreventDefault(); return NS_OK; }

  // nsIDOMKeyListener
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

  // nsIDOMMouseListener
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent)     { aMouseEvent->StopPropagation();aMouseEvent->PreventDefault(); return NS_OK; }
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent)       { aMouseEvent->StopPropagation();aMouseEvent->PreventDefault(); return NS_OK; }
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent)    { aMouseEvent->StopPropagation();aMouseEvent->PreventDefault(); return NS_OK; }
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { aMouseEvent->StopPropagation();aMouseEvent->PreventDefault(); return NS_OK; }
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent)     { aMouseEvent->StopPropagation();aMouseEvent->PreventDefault(); return NS_OK; }
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent)      { aMouseEvent->StopPropagation();aMouseEvent->PreventDefault(); return NS_OK; }

  // nsIDOMMouseMotionListener
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent)      { return NS_OK; };
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent)     { aMouseEvent->PreventDefault(); return NS_OK; }

  // Add/remove the relevant listeners, based on what interfaces
  // the embedding chrome implements.
  nsresult AddListeners();
  nsresult RemoveListeners();

private:

  nsCOMPtr<nsIDOMEventReceiver> mEventReceiver;
  PRInt8 mRegFlags;

}; // class nsPrintPreviewListener



#endif /* nsPrintPreviewListener_h__ */
