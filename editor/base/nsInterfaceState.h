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
 * The Original Code is mozilla.org code.
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


#ifndef nsInterfaceState_h__
#define nsInterfaceState_h__


#include "nsISelectionListener.h"
#include "nsIDocumentStateListener.h"
#include "nsITransactionListener.h"
#include "nsIWebShell.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"

class nsIHTMLEditor;
class nsIDOMDocument;

// class responsible for communicating changes in local state back to the UI.
// This is currently somewhat tied to a given XUL UI implementation

class nsInterfaceState : public nsISelectionListener,
                         public nsIDocumentStateListener,
                         public nsITransactionListener,
                         public nsITimerCallback
{
public:

                nsInterfaceState();
  virtual       ~nsInterfaceState();
  
  NS_DECL_ISUPPORTS
  
  NS_IMETHOD    Init(nsIHTMLEditor* aEditor, nsIDOMDocument *aChromeDoc);

  // nsISelectionListener interface
  NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason);

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

protected:

  enum {
    eStateUninitialized   = -1,
    eStateOff             = PR_FALSE,
    eStateOn              = PR_TRUE
  };
  
  PRBool        SelectionIsCollapsed();
  nsresult      UpdateDirtyState(PRBool aNowDirty);  
  nsresult      CallUpdateCommands(const nsAString& aCommand);
  nsresult      UpdateUndoCommands(nsITransactionManager *aManager);
  nsresult      PrimeUpdateTimer();
  void          TimerCallback();

  // this class should not hold references to the editor or editorShell. Doing
  // so would result in cirular reference chains.
  
  nsIHTMLEditor*   mEditor;		 // the HTML editor
  nsIDOMDocument*  mChromeDoc;  // XUL document for the chrome area

  nsIDOMWindowInternal*       mDOMWindow;   // nsIDOMWindowInternal used for calling UpdateCommands
  
  nsCOMPtr<nsITimer>  mUpdateTimer;
    
  PRInt8        mDirtyState;  
  PRInt8        mSelectionCollapsed;  
  PRPackedBool  mFirstDoOfFirstUndo;
  PRInt32       mBatchDepth;
    
};

extern "C" nsresult NS_NewInterfaceState(nsIHTMLEditor* aEditor, nsIDOMDocument* aChromeDoc, nsISelectionListener** aInstancePtrResult);

#endif // nsInterfaceState_h__
