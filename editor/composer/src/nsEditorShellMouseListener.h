/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef editorShellMouseLisenter_h__
#define editorShellMouseLisenter_h__

#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseListener.h"
#include "nsIEditorShell.h"

class nsString;

class nsEditorShellMouseListener : public nsIDOMMouseListener,
                                   public nsSupportsWeakReference 
{
public:
  /** default constructor
   */
  nsEditorShellMouseListener();
  /** default destructor
   */
  virtual ~nsEditorShellMouseListener();

  /** SetEditorShell gives an address to the editorShell that will be accessed
   *  @param aEditorShell the editorShell this listener calls for editing operations
   */
  void SetEditorShell(nsIEditorShell *aEditorShell){mEditorShell = aEditorShell;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of mouseevent handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
/*END implementations of mouseevent handler interface*/

protected:
  nsIEditorShell*   mEditorShell; // weak reference
};

/** factory for the mouse listener
 */
extern nsresult NS_NewEditorShellMouseListener(nsIDOMEventListener ** aInstancePtrResult, nsIEditorShell *aEditorShell);

#endif //editorShellMouseLisenter_h__

