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
 *   Kathleen Brade <brade@netscape.com>
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

#ifndef nsCommandManager_h__
#define nsCommandManager_h__


#include "nsString.h"
#include "nsClassHashtable.h"
#include "nsWeakReference.h"

#include "nsICommandManager.h"
#include "nsPICommandUpdater.h"
#include "nsCycleCollectionParticipant.h"

class nsIController;
template<class E> class nsCOMArray;


class nsCommandManager :  public nsICommandManager,
                          public nsPICommandUpdater,
                       // public nsISecurityCheckedComponent,
                          public nsSupportsWeakReference

{
public:
                        nsCommandManager();
  virtual               ~nsCommandManager();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsCommandManager, nsICommandManager)
  
  // nsICommandManager
  NS_DECL_NSICOMMANDMANAGER
  
  // nsPICommandUpdater
  NS_DECL_NSPICOMMANDUPDATER


protected:


  nsresult  IsCallerChrome(bool *aIsCallerChrome);
  nsresult  GetControllerForCommand(const char * aCommand,
                                    nsIDOMWindow *aDirectedToThisWindow,
                                    nsIController** outController);


protected:

  nsClassHashtable<nsCharPtrHashKey, nsCOMArray<nsIObserver> > mObserversTable;

  nsIDOMWindow*         mWindow;      // weak ptr. The window should always outlive us
};


#endif // nsCommandManager_h__
