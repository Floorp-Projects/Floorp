/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 * Contributors:    
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMEventListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIWebProgressListener.h"
#include "nsIScrollPositionListener.h"
#include "nsISelectionListener.h"
#include "nsITimerCallback.h"
#include "nsITimer.h"
#include "nsUnicharUtils.h"
#include "nsIFindService.h"
#include "nsIFind.h"
#include "nsWeakReference.h"
#include "nsIAppStartupNotifier.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMWindow.h"
#include "nsITypeAheadFind.h"
#include "nsIStringBundle.h"

#define TYPEAHEADFIND_BUNDLE_URL "chrome://typeaheadfind/locale/typeaheadfind.properties"

class nsTypeAheadFind : public nsIDOMFocusListener,
                        public nsIDOMKeyListener,
                        public nsIDOMLoadListener,
                        public nsIWebProgressListener,
                        public nsIScrollPositionListener,
                        public nsISelectionListener,
                        public nsITimerCallback,
                        public nsSupportsWeakReference
{
public:
  nsTypeAheadFind();
  virtual ~nsTypeAheadFind();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_TYPEAHEADFIND_CID);

  NS_DECL_ISUPPORTS  // This macro expands into declaration of nsISupports interface
  NS_DECL_NSIWEBPROGRESSLISTENER

  // ----- nsIDOMEventListener --------------------------
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // ----- nsIDOMFocusListener --------------------------
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);

  // ----- nsIDOMKeyListener ----------------------------
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

  // ----- nsIDOMLoadListener ----------------------------
  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);

  // ----- nsIScrollPositionListener --------------------
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY);
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY);

  // ----- nsISelectionListener -------------------------
  NS_IMETHOD NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason);

  // ----- nsITimerCallback ------------------------------------
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  static nsTypeAheadFind *GetInstance();
  static void ReleaseInstance(void);

protected:
  static int PR_CALLBACK TypeAheadFindPrefsReset(const char* aPrefName, void* instance_data);

  // Helper methods
  void AttachNewSelectionListener();
  void RemoveCurrentSelectionListener();
  void AttachNewScrollPositionListener(nsIPresShell *aPresShell);
  void RemoveCurrentScrollPositionListener();
  void AttachNewKeypressListener(nsIDOMEventTarget *aTarget);
  void RemoveCurrentKeypressListener();
  void AttachNewWindowFocusListener(nsIDOMEventTarget *aTarget);
  void RemoveCurrentWindowFocusListener();

  void RangeStartsInsideLink(nsIDOMRange *aRange, nsIPresShell *aPresShell, 
                             PRBool *aIsInsideLink, PRBool *aIsStartingLink);
  void GetSelection(nsIPresShell *aPresShell,   // If aCurrentNode is nsnull, gets selection for document
                    nsIDOMNode *aCurrentNode, nsISelection **aDomSel);
  void CancelFind();
  PRBool IsRangeVisible(nsIPresShell *aPresShell, nsIPresContext *aPresContext, 
                         nsIDOMRange *aRange, PRBool aMustBeVisible, nsIDOMRange **aNewRange);
  nsresult FindItNow(PRBool aIsLinksOnly, PRBool aIsFirstVisiblePreferred, PRBool aIsBackspace);
  nsresult GetSearchContainers(nsISupports *aContainer, PRBool aIsFirstVisiblePreferred, PRBool aCanUseDocSelection,
                               nsIPresShell **aPresShell, nsIPresContext **aPresContext,
                               nsIDOMRange **aSearchRange, nsIDOMRange **aStartRange, nsIDOMRange **aEndRange);
  void DisplayStatus(PRBool aSuccess, PRBool aClearStatus);
  nsresult GetTranslatedString(const nsAString& aKey, nsAString& aStringOut);

  // Used by GetInstance and ReleaseInstance
  static nsTypeAheadFind *mInstance;

  // Current find state
  nsString mTypeAheadBuffer;
  PRBool mLinksOnlyPref, mLinksOnly, mErrorBeepsPref, mIsTypeAheadOn;
  PRBool mCaretBrowsingOn;
  PRBool mIsRepeatingSameChar;
  PRBool mKeepSelectionOnCancel;
  PRInt32 mTimeoutLength; // Amount of time before find is automatically cancelled
  static PRBool gIsFindingText; // this flag prevents side effects from listener callbacks while selecting/focusing found text
  nsCOMPtr<nsIDOMRange> mStartFindRange;  // where selection was when user started the find

#ifdef RESTORE_SEARCH
  // Old nsIFindService state
  nsString mOldSearchString;
  PRBool mOldWrapSetting, mOldCaseSetting, mOldWordSetting, mOldDirectionSetting;
#endif

  // Cached useful interfaces
  nsCOMPtr<nsIFind> mFind;
  nsCOMPtr<nsIFindService> mFindService;
  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsITimer> mTimer;

  // The focused content window that we're listening to and it's cached objects
  nsCOMPtr<nsISelection> mFocusedDocSelection;
  nsCOMPtr<nsIDOMWindow> mFocusedWindow;
  nsCOMPtr<nsIWeakReference> mFocusedWeakShell;
};
