/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                          public nsSupportsWeakReference

{
public:
  typedef nsTArray<nsCOMPtr<nsIObserver> > ObserverList;

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
  nsClassHashtable<nsCharPtrHashKey, ObserverList> mObserversTable;

  nsIDOMWindow*         mWindow;      // weak ptr. The window should always outlive us
};


#endif // nsCommandManager_h__
