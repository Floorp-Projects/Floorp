/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsEditorController_h__
#define nsEditorController_h__

#define NS_EDITORCONTROLLER_CID \
{ 0x26fb965c, 0x9de6, 0x11d3, { 0xbc, 0xcc, 0x0, 0x60, 0xb0, 0xfc, 0x76, 0xbd } }


#include "nsIController.h"
#include "nsIEditorController.h"
#include "nsIControllerCommand.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsHashtable.h"
#include "nsString.h"
#include "nsWeakPtr.h"

class nsIEditor;


// the editor controller is used for both text widgets, and basic text editing
// commands in composer. The refCon that gets passed to its commands is an nsIEditor.

class nsEditorController :  public nsIController,
                            public nsIEditorController,
                            public nsIInterfaceRequestor
{
public:

          nsEditorController();
  virtual ~nsEditorController();

  // nsISupports
  NS_DECL_ISUPPORTS
    
  // nsIController
  NS_DECL_NSICONTROLLER

  /** init the controller */
  NS_IMETHOD Init(nsISupports *aCommandRefCon);

  /** Set the cookie that is passed to commands
   */
  NS_IMETHOD SetCommandRefCon(nsISupports *aCommandRefCon);

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR
  
protected:

  /** return PR_TRUE if the editor associated with mContent is enabled */
  PRBool   IsEnabled();

protected:

   //if editor is null then look to mContent. this is for dual use of window and content
   //attached controller.
   nsISupports *mCommandRefCon;
   
   nsCOMPtr<nsIControllerCommandManager> mCommandManager;     // our reference to the command manager
   
private:

  static nsresult GetEditorCommandManager(nsIControllerCommandManager* *outCommandManager);
  static nsresult RegisterEditorCommands(nsIControllerCommandManager* inCommandManager);
  
  // the singleton command manager
  static nsWeakPtr sEditorCommandManager;       // common editor (i.e. text widget) commands
   
};

#endif /* nsEditorController_h__ */

