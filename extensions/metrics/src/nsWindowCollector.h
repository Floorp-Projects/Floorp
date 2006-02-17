/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWindowCollector_h_
#define nsWindowCollector_h_

#include "nsIObserver.h"
#include "nsDataHashtable.h"

class nsIDOMWindow;

// This file defines the window collector class, which monitors window
// creation, opening, closing, and destruction, and records the events into
// the metrics service. This information provides additional context for loads
// into each window.
//
// The window collector also manages the list of assigned window ids, which
// are shared by all interested collectors.
//
// Event descriptions:
//
// <windowcreate/>: logged on new window creation.
// This window can correspond to a toplevel window or to a subframe.
// Attributes:
//   id: The id of the new window (uint16)
//   parent: The id of the window's parent (uint16)
//   chrome: Set to true if the window has a chrome docshell (boolean)
//
// <windowopen/>: logged when window.open() is called.
// This will be logged immediately following <windowcreate/> when a toplevel
// window is opened.  It will never be logged for subframe windows.
// Attributes:
//   id: The id of the opened window (uint16)
//   opener: The id of the window's opener (uint16)
//
// <windowclose/>: logged when a toplevel window is closed.
// Attributes:
//   id: The id of the closed window (uint16)
//
// <windowdestroy/>: logged when a window is destroyed.
// Attributes:
//   id: The id of the destroyed window (uint16).

class nsWindowCollector : public nsIObserver
{
 public:
  nsWindowCollector() : mNextWindowID(0) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Get the window id for the given DOMWindow.  If a window id has not
  // yet been assigned for the window, the next id will be used.
  static PRUint16 GetWindowID(nsIDOMWindow *window);

  // Creates the collector singleton and registers for notifications.
  static nsresult Startup();
  // Destroys the collector singleton.
  static void Shutdown();

 private:
  ~nsWindowCollector();

  // Object intiliazation, to be called by Startup()
  nsresult Init();

  // Window to incrementing-id map.  The keys are nsIDOMWindow*.
  nsDataHashtable<nsVoidPtrHashKey, PRUint16> mWindowMap;

  PRUint16 mNextWindowID;
};

#endif // nsWindowCollector_h_
