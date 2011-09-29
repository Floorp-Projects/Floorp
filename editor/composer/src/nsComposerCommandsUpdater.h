/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser   <sfraser@netscape.com>
 *   Michael Judge  <mjudge@netscape.com>
 *   Charles Manske <cmanske@netscape.com>
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




#ifndef nsComposerCommandsUpdater_h__
#define nsComposerCommandsUpdater_h__

#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsWeakPtr.h"
#include "nsPICommandUpdater.h"

#include "nsISelectionListener.h"
#include "nsIDocumentStateListener.h"
#include "nsITransactionListener.h"

class nsIDocShell;
class nsITransactionManager;

class nsComposerCommandsUpdater : public nsISelectionListener,
                                  public nsIDocumentStateListener,
                                  public nsITransactionListener,
                                  public nsITimerCallback
{
public:

                                  nsComposerCommandsUpdater();
  virtual                         ~nsComposerCommandsUpdater();

  // nsISupports
  NS_DECL_ISUPPORTS
  
  // nsISelectionListener
  NS_DECL_NSISELECTIONLISTENER
  
  // nsIDocumentStateListener
  NS_DECL_NSIDOCUMENTSTATELISTENER

  // nsITimerCallback interfaces
  NS_DECL_NSITIMERCALLBACK

  /** nsITransactionListener interfaces
    */  
  NS_IMETHOD WillDo(nsITransactionManager *aManager, nsITransaction *aTransaction, bool *aInterrupt);
  NS_IMETHOD DidDo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aDoResult);
  NS_IMETHOD WillUndo(nsITransactionManager *aManager, nsITransaction *aTransaction, bool *aInterrupt);
  NS_IMETHOD DidUndo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aUndoResult);
  NS_IMETHOD WillRedo(nsITransactionManager *aManager, nsITransaction *aTransaction, bool *aInterrupt);
  NS_IMETHOD DidRedo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aRedoResult);
  NS_IMETHOD WillBeginBatch(nsITransactionManager *aManager, bool *aInterrupt);
  NS_IMETHOD DidBeginBatch(nsITransactionManager *aManager, nsresult aResult);
  NS_IMETHOD WillEndBatch(nsITransactionManager *aManager, bool *aInterrupt);
  NS_IMETHOD DidEndBatch(nsITransactionManager *aManager, nsresult aResult);
  NS_IMETHOD WillMerge(nsITransactionManager *aManager, nsITransaction *aTopTransaction,
                       nsITransaction *aTransactionToMerge, bool *aInterrupt);
  NS_IMETHOD DidMerge(nsITransactionManager *aManager, nsITransaction *aTopTransaction,
                      nsITransaction *aTransactionToMerge,
                      bool aDidMerge, nsresult aMergeResult);


  nsresult   Init(nsIDOMWindow* aDOMWindow);

protected:

  enum {
    eStateUninitialized   = -1,
    eStateOff             = PR_FALSE,
    eStateOn              = PR_TRUE
  };
  
  bool          SelectionIsCollapsed();
  nsresult      UpdateDirtyState(bool aNowDirty);  
  nsresult      UpdateOneCommand(const char* aCommand);
  nsresult      UpdateCommandGroup(const nsAString& aCommandGroup);

  already_AddRefed<nsPICommandUpdater> GetCommandUpdater();
  
  nsresult      PrimeUpdateTimer();
  void          TimerCallback();
  nsCOMPtr<nsITimer>  mUpdateTimer;

  nsWeakPtr     mDOMWindow;
  nsWeakPtr     mDocShell;
  PRInt8        mDirtyState;  
  PRInt8        mSelectionCollapsed;  
  bool          mFirstDoOfFirstUndo;
    

};

extern "C" nsresult NS_NewComposerCommandsUpdater(nsISelectionListener** aInstancePtrResult);


#endif // nsComposerCommandsUpdater_h__
