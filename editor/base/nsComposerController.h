/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsComposerController_h__
#define nsComposerController_h__

#define NS_COMPOSERCONTROLLER_CID \
{ 0x50e95301, 0x17a8, 0x11d4, { 0x9f, 0x7e, 0xdd, 0x53, 0x0d, 0x5f, 0x05, 0x7c } }

#include "nsString.h"
#include "nsIEditorController.h"
#include "nsIController.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIControllerCommand.h"
#include "nsWeakPtr.h"

// the editor controller is used for composer only (and other HTML compose
// areas). The refCon that gets passed to its commands is an nsIEditorShell.

class nsComposerController : public nsIController,
                             public nsIEditorController,
                             public nsIInterfaceRequestor
{
public:

          nsComposerController();
  virtual ~nsComposerController();

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

   //if editor is null then look to mContent. this is for dual use of window and content
   //attached controller.
   nsISupports *mCommandRefCon;
   
   nsCOMPtr<nsIControllerCommandManager> mCommandManager;     // our reference to the command manager
   
private:

  static nsresult GetComposerCommandManager(nsIControllerCommandManager* *outCommandManager);
  static nsresult RegisterComposerCommands(nsIControllerCommandManager* inCommandManager);

  // the singleton command manager
  static nsWeakPtr sComposerCommandManager;       // common composer commands
   
};

#endif /* nsComposerController_h__ */
