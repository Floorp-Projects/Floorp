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
 * Contributor(s): 
 *   Simon Fraser <sfraser@netscape.com>
 */




#ifndef nsComposerCommandsUpdater_h__
#define nsComposerCommandsUpdater_h__

#include "nsString.h"
#include "nsITimer.h"

#include "nsISelectionListener.h"
#include "nsIDocumentStateListener.h"
#include "nsITransactionListener.h"
#include "nsITimerCallback.h"


class nsIEditor;
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
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  /** nsITransactionListener interfaces
    */  
  NS_IMETHOD WillDo(nsITransactionManager *aManager, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidDo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aDoResult);
  NS_IMETHOD WillUndo(nsITransactionManager *aManager, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidUndo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aUndoResult);
  NS_IMETHOD WillRedo(nsITransactionManager *aManager, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidRedo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aRedoResult);
  NS_IMETHOD WillBeginBatch(nsITransactionManager *aManager, PRBool *aInterrupt);
  NS_IMETHOD DidBeginBatch(nsITransactionManager *aManager, nsresult aResult);
  NS_IMETHOD WillEndBatch(nsITransactionManager *aManager, PRBool *aInterrupt);
  NS_IMETHOD DidEndBatch(nsITransactionManager *aManager, nsresult aResult);
  NS_IMETHOD WillMerge(nsITransactionManager *aManager, nsITransaction *aTopTransaction,
                       nsITransaction *aTransactionToMerge, PRBool *aInterrupt);
  NS_IMETHOD DidMerge(nsITransactionManager *aManager, nsITransaction *aTopTransaction,
                      nsITransaction *aTransactionToMerge,
                      PRBool aDidMerge, nsresult aMergeResult);


  nsresult      SetEditor(nsIEditor* aEditor);

protected:

  enum {
    eStateUninitialized   = -1,
    eStateOff             = PR_FALSE,
    eStateOn              = PR_TRUE
  };
  
  PRBool        SelectionIsCollapsed();
  nsresult      UpdateDirtyState(PRBool aNowDirty);  
  nsresult      CallUpdateCommands(const nsAReadableString& aCommand);
  
  nsresult      PrimeUpdateTimer();
  void          TimerCallback();

  
  // this class should not hold references to the editor or editorShell. Doing
  // so would result in cirular reference chains.
  
  nsIEditor*   	mEditor;		 // the HTML editor

  nsIDocShell*  mDocShell;
  
  nsCOMPtr<nsITimer>  mUpdateTimer;
    
  PRInt8        mDirtyState;  
  PRInt8        mSelectionCollapsed;  
  PRPackedBool  mFirstDoOfFirstUndo;
    

};

extern "C" nsresult NS_NewComposerCommandsUpdater(nsIEditor* aEditor, nsISelectionListener** aInstancePtrResult);


#endif // nsComposerCommandsUpdater_h__
