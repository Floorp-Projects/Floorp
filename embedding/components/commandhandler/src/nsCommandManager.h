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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#ifndef nsCommandManager_h__
#define nsCommandManager_h__


#include "nsString.h"
#include "nsHashtable.h"
#include "nsWeakReference.h"

#include "nsICommandManager.h"
#include "nsPICommandUpdater.h"

class nsIController;


// {64edb481-0c04-11d5-a73c-e964b968b0bc}
#define NS_COMMAND_MANAGER_CID \
{ 0x64edb481, 0x0c04, 0x11d5, { 0xa7, 0x3c, 0xe9, 0x64, 0xb9, 0x68, 0xb0, 0xbc } }

#define NS_COMMAND_MANAGER_CONTRACTID \
 "@mozilla.org/embedcomp/command-manager;1"


class nsCommandManager :  public nsICommandManager,
                          public nsPICommandUpdater,
                          public nsSupportsWeakReference

{
public:
                        nsCommandManager();
  virtual               ~nsCommandManager();

  // nsISupports
  NS_DECL_ISUPPORTS
  
  // nsICommandManager
  NS_DECL_NSICOMMANDMANAGER
  
  // nsPICommandUpdater
  NS_DECL_NSPICOMMANDUPDATER


protected:


  nsresult  GetControllerForCommand(const nsAReadableString& aCommand, nsIController** outController);


protected:

  nsSupportsHashtable   mCommandObserversTable;   // hash table of nsIObservers, keyed by command name

  nsIDOMWindow*         mWindow;      // weak ptr. The window should always outlive us
};


#endif // nsCommandManager_h__
