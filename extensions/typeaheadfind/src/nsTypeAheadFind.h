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
#include "nsIDOMKeyListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMCompositionListener.h"
#include "nsIScrollPositionListener.h"
#include "nsISelectionListener.h"
#include "nsISelectionController.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIFocusController.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsUnicharUtils.h"
#include "nsIFindService.h"
#include "nsIFind.h"
#include "nsIWebBrowserFind.h"
#include "nsWeakReference.h"
#include "nsIAppStartupNotifier.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsITypeAheadFind.h"
#include "nsIStringBundle.h"
#include "nsISupportsArray.h"
#include "nsISound.h"

#define TYPEAHEADFIND_BUNDLE_URL \
        "chrome://global/locale/typeaheadfind.properties"
#define TYPEAHEADFIND_NOTFOUND_WAV_URL \
        "chrome://global/content/notfound.wav"

enum {
  eRepeatingNone,
  eRepeatingChar,
  eRepeatingCharReverse,
  eRepeatingForward,
  eRepeatingReverse
}; 

const int kMaxBadCharsBeforeCancel = 3;

class nsTypeAheadFind : public nsITypeAheadFind,
                        public nsIDOMKeyListener,
                        public nsIDOMTextListener,
                        public nsIDOMCompositionListener,
                        public nsIObserver,
                        public nsIScrollPositionListener,
                        public nsISelectionListener,
                        public nsITimerCallback,
                        public nsSupportsWeakReference
{
public:
  nsTypeAheadFind();
  virtual ~nsTypeAheadFind();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_TYPEAHEADFIND_CID);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITYPEAHEADFIND
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSISELECTIONLISTENER

  // ----- nsIDOMKeyListener ----------------------------
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

  // ----- nsIDOMTextListener ----------------------------
  NS_IMETHOD HandleText(nsIDOMEvent* aTextEvent);

  // ----- nsIDOMCompositionListener ----------------------------
  NS_IMETHOD HandleStartComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD HandleEndComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD HandleQueryComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD HandleQueryReconversion(nsIDOMEvent* aCompositionEvent);

  // ----- nsIScrollPositionListener --------------------
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, 
                                      nscoord aX, nscoord aY);
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, 
                                     nscoord aX, nscoord aY);

  // ----- nsITimerCallback -----------------------------
  NS_DECL_NSITIMERCALLBACK

  static nsTypeAheadFind *GetInstance();
  static void ReleaseInstance(void);
  static PRBool IsTargetContentOkay(nsIContent *aContent);

protected:
  nsresult PrefsReset();

  // Helper methods
  nsresult HandleChar(PRUnichar aChar);
  PRBool HandleBackspace();
  void SaveFind();
  void PlayNotFoundSound();
  void GetTopContentPresShell(nsIDocShellTreeItem *aTreeItem, 
                              nsIPresShell **aPresShell);
  void GetStartWindow(nsIDOMWindow *aWindow, nsIDOMWindow **aStartWindow);
  nsresult GetWebBrowserFind(nsIDOMWindow *aDOMWin,
                             nsIWebBrowserFind **aWebBrowserFind);
  void StartTimeout();
  nsresult Init();
  void Shutdown();
  nsresult UseInWindow(nsIDOMWindow *aDomWin);
  void SetSelectionLook(nsIPresShell *aPresShell, PRBool aChangeColor, 
                        PRBool aEnabled);
  void ResetGlobalAutoStart(PRBool aAutoStart);
  void AttachDocListeners(nsIPresShell *aPresShell);
  void RemoveDocListeners();
  void AttachWindowListeners(nsIDOMWindow *aDOMWin);
  void RemoveWindowListeners(nsIDOMWindow *aDOMWin);
  void GetChromeEventHandler(nsIDOMWindow *aDOMWin, 
                             nsIDOMEventTarget **aChromeTarget);

  void RangeStartsInsideLink(nsIDOMRange *aRange, nsIPresShell *aPresShell, 
                             PRBool *aIsInsideLink, PRBool *aIsStartingLink);

  nsresult GetTargetIfTypeAheadOkay(nsIDOMEvent *aEvent, 
                                    nsIContent **aTargetContent, 
                                    nsIPresShell **aTargetPresShell);

  // Get selection and selection controller for current pres shell
  void GetSelection(nsIPresShell *aPresShell, nsISelectionController **aSelCon, 
                    nsISelection **aDomSel);
  PRBool IsRangeVisible(nsIPresShell *aPresShell, nsPresContext *aPresContext,
                         nsIDOMRange *aRange, PRBool aMustBeVisible, 
                         PRBool aGetTopVisibleLeaf,
                         nsIDOMRange **aNewRange);
  nsresult FindItNow(nsIPresShell *aPresShell, PRBool aIsRepeatingSameChar, 
                     PRBool aIsLinksOnly, PRBool aIsFirstVisiblePreferred);
  nsresult GetSearchContainers(nsISupports *aContainer, 
                               PRBool aIsRepeatingSameChar,
                               PRBool aIsFirstVisiblePreferred, 
                               PRBool aCanUseDocSelection,
                               nsIPresShell **aPresShell, 
                               nsPresContext **aPresContext);
  void DisplayStatus(PRBool aSuccess, nsIContent *aFocusedContent, 
                     PRBool aClearStatus, const PRUnichar *aText = nsnull);
  nsresult GetTranslatedString(const nsAString& aKey, nsAString& aStringOut);

  // Used by GetInstance and ReleaseInstance
  static nsTypeAheadFind *sInstance;

  // Current find state
  nsString mTypeAheadBuffer;
  nsString mFindNextBuffer;
  nsString mIMEString;

  nsCString mNotFoundSoundURL;

  // PRBool's are used instead of PRPackedBool's where the address of the
  // boolean variable is getting passed into a method. For example:
  // GetBoolPref("accessibility.typeaheadfind.linksonly", &mLinksOnlyPref);
  PRBool mIsFindAllowedInWindow;
  PRBool mAutoStartPref;
  PRBool mLinksOnlyPref;
  PRBool mStartLinksOnlyPref;
  PRPackedBool mLinksOnly;
  PRBool mIsTypeAheadOn;
  PRBool mCaretBrowsingOn;
  PRPackedBool mLiteralTextSearchOnly;
  PRPackedBool mDontTryExactMatch;
  // mAllTheSame Char starts out PR_TRUE, becomes false when 
  // at least 2 different chars typed
  PRPackedBool mAllTheSameChar;
  // mLinksOnlyManuallySet = PR_TRUE when the user has already 
  // typed / or '. This allows the next / or ' to get searched for.
  PRPackedBool mLinksOnlyManuallySet;
  // mIsFindingText = PR_TRUE when we need to prevent listener callbacks 
  // from resetting us during typeahead find processing
  PRPackedBool mIsFindingText; 
  PRPackedBool mIsMenuBarActive;
  PRPackedBool mIsMenuPopupActive;
  PRPackedBool mIsFirstVisiblePreferred;
  PRPackedBool mIsIMETypeAheadActive;
  PRPackedBool mIsBackspaceProtectOn; // from accidentally going back in history
  PRInt32 mBadKeysSinceMatch;
  PRUnichar mLastBadChar; // if taf automatically overwrites an unfound character
  PRInt32 mRepeatingMode;
  PRInt32 mTimeoutLength; // time in ms before find is automatically cancelled

  // Sound is played asynchronously on some platforms.
  // If we destroy mSoundInterface before sound has played, it won't play
  nsCOMPtr<nsISound> mSoundInterface;
  PRBool mIsSoundInitialized;
  
  static PRInt32 sAccelKey;  // magic value of -1 indicates unitialized state

  // where selection was when user started the find
  nsCOMPtr<nsIDOMRange> mStartFindRange;
  nsCOMPtr<nsIDOMRange> mSearchRange;
  nsCOMPtr<nsIDOMRange> mStartPointRange;
  nsCOMPtr<nsIDOMRange> mEndPointRange;

  // Cached useful interfaces
  nsCOMPtr<nsIFind> mFind;
  nsCOMPtr<nsIFindService> mFindService;
  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIFocusController> mFocusController;

  // The focused content window that we're listening to and it's cached objects
  nsCOMPtr<nsISelection> mFocusedDocSelection;
  nsCOMPtr<nsISelectionController> mFocusedDocSelCon;
  nsCOMPtr<nsIDOMWindow> mFocusedWindow;
  nsCOMPtr<nsIWeakReference> mFocusedWeakShell;

  // Windows where typeaheadfind doesn't auto start as the user types
  nsCOMPtr<nsISupportsArray> mManualFindWindows;
};


class nsTypeAheadController : public nsIController
{
public:
  nsTypeAheadController(nsIFocusController *aFocusController);
  virtual ~nsTypeAheadController();
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLER

private:
  nsCOMPtr<nsIFocusController> mFocusController;
  nsresult EnsureContentWindow(nsIDOMWindowInternal *aFocusedWin,
                               nsIDOMWindow **aStartContentWin);
};
