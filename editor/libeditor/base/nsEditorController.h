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

#define NS_EDITORCONTROLLER_CID \
{ 0x26fb965c, 0x9de6, 0x11d3, { 0xbc, 0xcc, 0x0, 0x60, 0xb0, 0xfc, 0x76, 0xbd } }

#define NS_COMPOSERCONTROLLER_CID \
{ 0x50e95301, 0x17a8, 0x11d4, { 0x9f, 0x7e, 0xdd, 0x53, 0x0d, 0x5f, 0x05, 0x7c } }


#include "nsIController.h"
#include "nsIEditorController.h"
#include "nsIControllerCommand.h"
#include "nsIInterfaceRequestor.h"

#include "nsHashtable.h"
#include "nsString.h"
#include "nsWeakPtr.h"

class nsIEditor;
class nsIEditorShell;
class nsBaseCommand;


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

  /* register a command */
  nsresult RegisterEditorCommand(nsBaseCommand* aCommand);
  
protected:

   //if editor is null then look to mContent. this is for dual use of window and content
   //attached controller.
   nsISupports *mCommandRefCon;
   
   nsCOMPtr<nsIControllerCommandManager> mCommandManager;     // our reference to the command manager
   

protected:

  static nsresult RegisterOneCommand(const PRUnichar* aCommandName, nsIControllerCommandManager *inCommandManager, nsBaseCommand* aCommand);

private:

  static nsresult GetEditorCommandManager(nsIControllerCommandManager* *outCommandManager);
  static nsresult RegisterEditorCommands(nsIControllerCommandManager* inCommandManager);
  
  // the singleton command manager
  static nsWeakPtr sEditorCommandManager;       // common editor (i.e. text widget) commands
   
};



// the editor controller is used for composer only (and other HTML compose
// areas). The refCon that gets passed to its commands is an nsIEditorShell.

class nsComposerController : public nsEditorController
{
public:

          nsComposerController();
  virtual ~nsComposerController();

  /** init the controller */
  NS_IMETHOD Init(nsISupports *aCommandRefCon);

protected:

private:

  static nsresult GetComposerCommandManager(nsIControllerCommandManager* *outCommandManager);
  static nsresult RegisterComposerCommands(nsIControllerCommandManager* inCommandManager);

  // the singleton command manager
  static nsWeakPtr sComposerCommandManager;     // composer-specific commands (lots of them)
};
