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

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIEditorDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIChromeEventHandler.h"
#include "nsIFocusController.h"
#include "nsIDOMNSEvent.h"
#include "nsIPref.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsFrameTraversal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIEventStateManager.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectElement.h"
#include "nsILink.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsILookAndFeel.h"

#include "nsICaret.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISound.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsIWindowWatcher.h"
#include "nsIObserverService.h"
#include "nsLayoutAtoms.h"

#include "nsIPrivateTextEvent.h"
#include "nsIPrivateCompositionEvent.h"
#include "nsGUIEvent.h"
#include "nsIDOMEventReceiver.h"

// Header for this class
#include "nsTypeAheadFind.h"

////////////////////////////////////////////////////////////////////////


NS_INTERFACE_MAP_BEGIN(nsTypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsITypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTextListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCompositionListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTypeAheadFind);
NS_IMPL_RELEASE(nsTypeAheadFind);

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);
static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

nsTypeAheadFind* nsTypeAheadFind::sInstance = nsnull;
PRInt32 nsTypeAheadFind::sAccelKey = -1;  // magic value of -1 when unitialized


nsTypeAheadFind::nsTypeAheadFind():
  mIsFindAllowedInWindow(PR_FALSE),
  mLinksOnlyPref(PR_FALSE), mStartLinksOnlyPref(PR_FALSE),
  mLinksOnly(PR_FALSE), mIsTypeAheadOn(PR_FALSE), mCaretBrowsingOn(PR_FALSE),
  mLiteralTextSearchOnly(PR_FALSE), mDontTryExactMatch(PR_FALSE),
  mLinksOnlyManuallySet(PR_FALSE), mIsFindingText(PR_FALSE),
  mIsMenuBarActive(PR_FALSE), mIsMenuPopupActive(PR_FALSE),
  mIsFirstVisiblePreferred(PR_FALSE), mIsIMETypeAheadActive(PR_FALSE),
  mBadKeysSinceMatch(0),
  mRepeatingMode(eRepeatingNone), mTimeoutLength(0)
{
  NS_INIT_ISUPPORTS();

#ifdef DEBUG
  // There should only ever be one instance of us
  static PRInt32 gInstanceCount;
  ++gInstanceCount;
  NS_ASSERTION(gInstanceCount == 1,
    "There should be only 1 instance of nsTypeAheadFind!");
#endif
}


nsTypeAheadFind::~nsTypeAheadFind()
{
  RemoveDocListeners();
  mTimer = nsnull;

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs) {
    prefs->UnregisterCallback("accessibility.typeaheadfind",
                              nsTypeAheadFind::PrefsReset, this);
    prefs->UnregisterCallback("accessibility.browsewithcaret",
                              nsTypeAheadFind::PrefsReset, this);
  }
}

nsresult
nsTypeAheadFind::Init()
{
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(mManualFindWindows));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  mSearchRange = do_CreateInstance(kRangeCID);
  mStartPointRange = do_CreateInstance(kRangeCID);
  mEndPointRange = do_CreateInstance(kRangeCID);
  mFind = do_CreateInstance(NS_FIND_CONTRACTID);
  if (!prefs || !mSearchRange || !mStartPointRange || !mEndPointRange ||
      !mFind) {
    return NS_ERROR_FAILURE;
  }

  // ----------- Listen to prefs ------------------
  rv = prefs->RegisterCallback("accessibility.typeaheadfind",
                               (PrefChangedFunc)nsTypeAheadFind::PrefsReset,
                               NS_STATIC_CAST(void*, this));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefs->RegisterCallback("accessibility.browsewithcaret",
                               (PrefChangedFunc)nsTypeAheadFind::PrefsReset,
                               NS_STATIC_CAST(void*, this));
  NS_ENSURE_SUCCESS(rv, rv);


  // ----------- Get accel key --------------------
  rv = prefs->GetIntPref("ui.key.accelKey", &sAccelKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // ----------- Get initial preferences ----------
  PrefsReset("", NS_STATIC_CAST(void*, this));

  // ----------- Set search options ---------------
  mFind->SetCaseSensitive(PR_FALSE);
  mFind->SetWordBreaker(nsnull);

  return rv;
}

nsTypeAheadFind *
nsTypeAheadFind::GetInstance()
{
  if (!sInstance) {
    sInstance = new nsTypeAheadFind();
    if (!sInstance)
      return nsnull;

    NS_ADDREF(sInstance);  // addref for sInstance global

    if (NS_FAILED(sInstance->Init())) {
      NS_RELEASE(sInstance);

      return nsnull;
    }
  }

  NS_ADDREF(sInstance);   // addref for the getter

  return sInstance;
}


void
nsTypeAheadFind::ReleaseInstance()
{
  NS_IF_RELEASE(sInstance);
}


// ------- Pref Callbacks (2) ---------------

int PR_CALLBACK
nsTypeAheadFind::PrefsReset(const char* aPrefName, void* instance_data)
{
  nsTypeAheadFind* typeAheadFind =
    NS_STATIC_CAST(nsTypeAheadFind*, instance_data);
  NS_ASSERTION(typeAheadFind, "bad instance data");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs) {
    return 0;
  }

  PRBool wasTypeAheadOn = typeAheadFind->mIsTypeAheadOn;

  prefs->GetBoolPref("accessibility.typeaheadfind",
                     &typeAheadFind->mIsTypeAheadOn);

  if (typeAheadFind->mIsTypeAheadOn != wasTypeAheadOn) {
    if (!typeAheadFind->mIsTypeAheadOn) {
      typeAheadFind->CancelFind();
    }
    else if (!typeAheadFind->mStringBundle) {
      // Get ready to watch windows
      nsresult rv;
      nsCOMPtr<nsIWindowWatcher> windowWatcher =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      windowWatcher->RegisterNotification(typeAheadFind);

      // Initialize string bundle
      nsCOMPtr<nsIStringBundleService> stringBundleService =
        do_GetService(kStringBundleServiceCID);

      if (stringBundleService)
        stringBundleService->CreateBundle(TYPEAHEADFIND_BUNDLE_URL,
                                          getter_AddRefs(typeAheadFind->mStringBundle));

      // Observe find again commands. We'll handle them if we were the last find
      nsCOMPtr<nsIObserverService> observerService = 
        do_GetService("@mozilla.org/observer-service;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      observerService->AddObserver(typeAheadFind, "nsWebBrowserFind_FindAgain", PR_TRUE);
    }
  }

  prefs->GetBoolPref("accessibility.typeaheadfind.linksonly",
                     &typeAheadFind->mLinksOnlyPref);

  prefs->GetBoolPref("accessibility.typeaheadfind.startlinksonly",
                     &typeAheadFind->mStartLinksOnlyPref);

  prefs->GetIntPref("accessibility.typeaheadfind.timeout",
                     &typeAheadFind->mTimeoutLength);

  prefs->GetBoolPref("accessibility.browsewithcaret",
                     &typeAheadFind->mCaretBrowsingOn);

  return 0;  // PREF_OK
}


// ------- nsITimer Methods (1) ---------------

NS_IMETHODIMP
nsTypeAheadFind::Notify(nsITimer *timer)
{
  CancelFind();
  return NS_OK;
}

// ----------- nsIObserver Methods (1) -------------------

NS_IMETHODIMP
nsTypeAheadFind::Observe(nsISupports *aSubject, const char *aTopic,
                         const PRUnichar *aData)
{
  PRBool isOpening;
  if (!nsCRT::strcmp(aTopic,"domwindowopened")) {
    isOpening = PR_TRUE;
  }
  else if (!nsCRT::strcmp(aTopic,"domwindowclosed")) {
    isOpening = PR_FALSE;
  }
  else if (!nsCRT::strcmp(aTopic,"nsWebBrowserFind_FindAgain")) {
    // A find next command wants to be executed.
    // We might want to handle it. If we do, return true in didExecute.
    nsCOMPtr<nsISupportsPRBool> didExecute(do_QueryInterface(aSubject));
    return FindNext(NS_LITERAL_STRING("up").Equals(aData), didExecute);
  }
  else {
    return NS_OK;
  }

  // When a window closes, we have to remove it and all of its subwindows
  // from mManualFindWindows so that we don't leak.
  // Eek, lots of work for such a simple thing.
  nsCOMPtr<nsIInterfaceRequestor> ifreq(do_QueryInterface(aSubject));
  NS_ENSURE_TRUE(ifreq, NS_OK);

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(ifreq));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  NS_ENSURE_TRUE(docShell, NS_OK);

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  docShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeAll,
                                  nsIDocShell::ENUMERATE_FORWARDS,
                                  getter_AddRefs(docShellEnumerator));

  // Iterate through shells to get windows
  // Eek! docshell -> presshell -> doc -> script global object -> window
  PRBool hasMoreDocShells;
  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
         && hasMoreDocShells) {
    nsCOMPtr<nsISupports> container;
    docShellEnumerator->GetNext(getter_AddRefs(container));
    nsCOMPtr<nsIInterfaceRequestor> ifreq(do_QueryInterface(container));

    if (ifreq) {
      nsCOMPtr<nsIDOMWindow> domWin(do_GetInterface(ifreq));
      if (isOpening) {
        AttachWindowListeners(domWin);
      }
      else {
        nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(domWin));

        if (windowSupports) {
          PRInt32 index = mManualFindWindows->IndexOf(windowSupports);

          if (index >= 0) {
            mManualFindWindows->RemoveElementAt(index);
          }
        }
      }
    }
  }

  return NS_OK;
}


nsresult
nsTypeAheadFind::UseInWindow(nsIDOMWindow *aDOMWin)
{
  // Set member variables and listeners up for new window and doc

  mFindNextBuffer.Truncate();
  CancelFind();

  nsCOMPtr<nsIDOMDocument> domDoc;
  aDOMWin->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

  if (!doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));

  if (!presShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> oldPresShell(do_QueryReferent(mFocusedWeakShell));

  if (!oldPresShell || oldPresShell != presShell) {
    CancelFind();
  } else if (presShell == oldPresShell) {
    // Same window, no need to reattach listeners

    return NS_OK;
  }

  RemoveDocListeners();

  mIsFindAllowedInWindow = PR_TRUE;
  mFocusedWindow = aDOMWin;
  mFocusedWeakShell = do_GetWeakReference(presShell);

  // Add scroll position and selection listeners, so we can cancel
  // current find when user navigates
  GetSelection(presShell, getter_AddRefs(mFocusedDocSelCon),
               getter_AddRefs(mFocusedDocSelection)); // cache for reuse
  AttachDocListeners(presShell);

  return NS_OK;
}


// ------- nsIDOMEventListener Methods (1) ---------------

NS_IMETHODIMP
nsTypeAheadFind::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.Equals(NS_LITERAL_STRING("DOMMenuBarActive"))) {
    mIsMenuBarActive = PR_TRUE;
  }
  else if (eventType.Equals(NS_LITERAL_STRING("DOMMenuBarInactive"))) {
    mIsMenuBarActive = PR_FALSE;
  }
  else if (eventType.Equals(NS_LITERAL_STRING("popupshown"))) {
    mIsMenuPopupActive = PR_TRUE;
  }
  else if (eventType.Equals(NS_LITERAL_STRING("popuphidden"))) {
    mIsMenuPopupActive = PR_FALSE;
  }

  return NS_OK;
}


// ------- nsIDOMKeyListener Methods (3) ---------------

NS_IMETHODIMP
nsTypeAheadFind::KeyDown(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::KeyUp(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::KeyPress(nsIDOMEvent* aEvent)
{
  if (!mIsTypeAheadOn || mIsMenuBarActive || mIsMenuPopupActive) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> targetContent;
  nsCOMPtr<nsIPresShell> targetPresShell;
  GetTargetIfTypeAheadOkay(aEvent, getter_AddRefs(targetContent), 
    getter_AddRefs(targetPresShell));
  if (!targetContent || !targetPresShell)
    return NS_OK;

  PRUint32 keyCode(0), charCode;
  PRBool isShift(PR_FALSE), isCtrl(PR_FALSE), isAlt(PR_FALSE), isMeta(PR_FALSE);
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));

  // ---------- Analyze keystroke, exit early if possible --------------

  if (!keyEvent ||
      NS_FAILED(keyEvent->GetKeyCode(&keyCode)) ||
      NS_FAILED(keyEvent->GetCharCode(&charCode)) ||
      NS_FAILED(keyEvent->GetShiftKey(&isShift)) ||
      NS_FAILED(keyEvent->GetCtrlKey(&isCtrl)) ||
      NS_FAILED(keyEvent->GetAltKey(&isAlt)) ||
      NS_FAILED(keyEvent->GetMetaKey(&isMeta))) {
    return NS_ERROR_FAILURE;
  }

  // ---------- Check the keystroke --------------------------------
  if ((isAlt && !isShift) || isCtrl || isMeta) {
    // Ignore most modified keys, but alt+shift may be used for
    // entering foreign chars.

    return NS_OK;
  }

  // ------------- Escape pressed ---------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE) {
    // Escape accomplishes 2 things:
    // 1. it is a way for the user to deselect with the keyboard
    // 2. it is a way for the user to cancel incremental find with
    //    visual feedback
    if (!mTypeAheadBuffer.IsEmpty()) {
      // If Escape is normally used for a command, don't do it
      aEvent->PreventDefault();
      CancelFind();
    }
    mFocusedDocSelection->CollapseToStart();

    return NS_OK;
  }

  // ---------- PreventDefault check ---------------
  // If a web page wants to use printable character keys,
  // they have to use evt.preventDefault() after they get the key
  nsCOMPtr<nsIDOMNSUIEvent> uiEvent(do_QueryInterface(aEvent));
  PRBool preventDefault;
  uiEvent->GetPreventDefault(&preventDefault);
  if (preventDefault) {
    return NS_OK;
  }

  // ----------- Back space -------------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
    if (HandleBackspace()) {
      aEvent->PreventDefault(); // Prevent normal processing of this keystroke
    }

    return NS_OK;
  }
  
  // ----------- Other non-printable keys -----------
  // We ignore space only if it's the first character
  // Function keys, etc. exit here
  if (keyCode || charCode < ' ' || 
      (charCode == ' ' && mTypeAheadBuffer.IsEmpty())) {
    return NS_OK;
  }

  aEvent->StopPropagation();  // We're using this key, no one else should

  return HandleChar(charCode);
}


PRBool
nsTypeAheadFind::HandleBackspace()
{
  // In normal type ahead find, remove a printable char from 
  // mTypeAheadBuffer, then search for buffer contents
  // Or, in repeated char find, go backwards

  // ---------- No chars in string ------------
  if (mTypeAheadBuffer.IsEmpty() || !mStartFindRange) {
    if (mRepeatingMode == eRepeatingChar || 
      mRepeatingMode == eRepeatingCharReverse) {
      // Backspace to find previous repeated char
      mTypeAheadBuffer = mFindNextBuffer;
      mFocusedDocSelection->GetRangeAt(0, getter_AddRefs(mStartFindRange));
    }
    else {
      return PR_FALSE;  // No find string to backspace in!
    }
  }

  // ---------- Only 1 char in string ------------
  if (mTypeAheadBuffer.Length() == 1 && 
      mRepeatingMode != eRepeatingCharReverse) {
    if (mStartFindRange) {
      mFocusedDocSelection->RemoveAllRanges();
      mFocusedDocSelection->AddRange(mStartFindRange);
    }

    mFocusedDocSelection->CollapseToStart();
    CancelFind();

    return PR_TRUE;
  }

  // ---------- Multiple chars in string ----------
  PRBool findBackwards = PR_FALSE;
  if (mBadKeysSinceMatch > 0) {
    // Nothing to remove when a backspace is hit after a bad key
    // because bad keys aren't added to the buffer
    mBadKeysSinceMatch = 0;
  }
  else if (mRepeatingMode == eRepeatingChar ||
           mRepeatingMode == eRepeatingCharReverse) {
    // Backspace in repeating char mode is like 
    mRepeatingMode = eRepeatingCharReverse;
    findBackwards = PR_TRUE;
  }
  else {
    mTypeAheadBuffer.Truncate(mTypeAheadBuffer.Length() - 1);
  }

  mDontTryExactMatch = PR_FALSE;

  // ---------- Get new find start ------------------
  nsCOMPtr<nsIPresShell> presShell;
  if (!findBackwards) {
    // For backspace, start from where first char was found
    // unless in backspacing after repeating char mode
    nsCOMPtr<nsIDOMNode> startNode;
    mStartFindRange->GetStartContainer(getter_AddRefs(startNode));
    if (startNode) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      startNode->GetOwnerDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
      if (doc) {
        doc->GetShellAt(0, getter_AddRefs(presShell));
      }
    }
    if (!presShell) {
      return PR_FALSE;
    }
    // Set the selection to the where the first character was found
    // so that find starts from there
    mIsFindingText = PR_TRUE; // so selection won't call CancelFind()
    GetSelection(presShell, getter_AddRefs(mFocusedDocSelCon), 
      getter_AddRefs(mFocusedDocSelection));
    nsCOMPtr<nsIDOMRange> startFindRange = do_CreateInstance(kRangeCID);
    mStartFindRange->CloneRange(getter_AddRefs(startFindRange));
    mFocusedDocSelection->RemoveAllRanges();
    mFocusedDocSelection->AddRange(startFindRange);
    mStartFindRange = startFindRange;
  }

  // ----------- Perform the find ------------------
  mIsFindingText = PR_TRUE; // so selection won't call CancelFind()
  if (NS_FAILED(FindItNow(presShell, findBackwards, mLinksOnly, PR_FALSE))) {
    DisplayStatus(PR_FALSE, nsnull, PR_FALSE); // Display failure status
  }
  mIsFindingText = PR_FALSE;

  SaveFind();

  return PR_TRUE;   // Backspace handled
}


nsresult
nsTypeAheadFind::HandleChar(PRUnichar aChar)
{
  // Add a printable char to mTypeAheadBuffer, then search for buffer contents

  // ------------ Million keys protection -------------
  if (mBadKeysSinceMatch >= kMaxBadCharsBeforeCancel) {
    // If they're just quickly mashing keys onto the keyboard, stop searching
    // until typeahead find is canceled via timeout or another normal means
    StartTimeout();  // Timeout from last bad key (this one)
    DisplayStatus(PR_FALSE, nsnull, PR_TRUE); // Status message to say find stopped
    return NS_ERROR_FAILURE;
  }

  aChar = ToLowerCase(NS_STATIC_CAST(PRUnichar, aChar));
  PRInt32 bufferLength = mTypeAheadBuffer.Length();

  // --------- No new chars after find again ----------
  if (mRepeatingMode == eRepeatingForward ||
      mRepeatingMode == eRepeatingReverse) {
    // Once Accel+[shift]+G or [shift]+F3 has been used once,
    // new typing will start a new find
    CancelFind();
    bufferLength = 0;
    mRepeatingMode = eRepeatingNone;
  }
  // --------- New char in repeated char mode ---------
  else if ((mRepeatingMode == eRepeatingChar ||
           mRepeatingMode == eRepeatingCharReverse) && 
           bufferLength > 1 && aChar != mTypeAheadBuffer.First()) {
    // If they repeat the same character and then change, such as aaaab
    // start over with new char as a repeated char find
    mTypeAheadBuffer = aChar;
  }
  // ------- Set repeating mode ---------
  else if (bufferLength > 0) {
    if (mTypeAheadBuffer.First() != aChar) {
      mRepeatingMode = eRepeatingNone;
    } else if (bufferLength == 1) {
      mRepeatingMode = eRepeatingChar;
    }
  }

  // ---- Check for prefix chars that change type of find ----
  if (bufferLength == 0 && !mLinksOnlyManuallySet) {
    if (aChar == '`' || aChar =='\'' || aChar =='\"') {
      // mLinksOnlyManuallySet = PR_TRUE when the user has already 
      // typed / or '. This allows the next / or ' to get searched for.
      mLinksOnlyManuallySet = PR_TRUE;

      // If you type quote, it starts a links only search
      mLinksOnly = PR_TRUE;

      return NS_OK;
    }

    if (aChar == '/') {
      // If you type / it starts a search for all text
      mLinksOnly = PR_FALSE;

      // In text search, repeated characters will not search for links
      mLiteralTextSearchOnly = PR_TRUE;

      // mLinksOnlyManuallySet = PR_TRUE when the user has already 
      // typed / or '. This allows the next / or ' to get searched for.
      mLinksOnlyManuallySet = PR_TRUE;

      return NS_OK;
    }
  }

  mTypeAheadBuffer += aChar;    // Add the char!

  // --------- Initialize find if 1st char ----------
  if (bufferLength == 0) {
    if (!mLinksOnlyManuallySet) {
      // Reset links only to default, if not manually set
      // by the user via ' or / keypress at beginning
      mLinksOnly = mLinksOnlyPref;
    }

    mRepeatingMode = eRepeatingNone;

    // If you can see the selection (not collapsed or thru caret browsing),
    // or if already focused on a page element, start there.
    // Otherwise we're going to start at the first visible element
    PRBool isSelectionCollapsed;
    mFocusedDocSelection->GetIsCollapsed(&isSelectionCollapsed);

    // If true, we will scan from top left of visible area
    // If false, we will scan from start of selection
    mIsFirstVisiblePreferred = !mCaretBrowsingOn && isSelectionCollapsed;
    if (mIsFirstVisiblePreferred) {
      // Get focused content from esm. If it's null, the document is focused.
      // If not, make sure the selection is in sync with the focus, so we can 
      // start our search from there.
      nsCOMPtr<nsIContent> focusedContent;
      nsCOMPtr<nsIPresContext> presContext;
      nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
      NS_ENSURE_TRUE(presShell, NS_OK);
      presShell->GetPresContext(getter_AddRefs(presContext));
      NS_ENSURE_TRUE(presContext, NS_OK);
      nsCOMPtr<nsIEventStateManager> esm;
      presContext->GetEventStateManager(getter_AddRefs(esm));
      esm->GetFocusedContent(getter_AddRefs(focusedContent));
      if (focusedContent) {
        mIsFindingText = PR_TRUE; // prevent selection listener from calling CancelFind()
        esm->MoveCaretToFocus();
        mIsFindingText = PR_FALSE;
        mIsFirstVisiblePreferred = PR_FALSE;
      }
    }
  }

  // ----------- Find the text! ---------------------
  mIsFindingText = PR_TRUE; // prevent selection listener from calling CancelFind()

  nsresult rv = NS_ERROR_FAILURE;

  if (mBadKeysSinceMatch <= 1) {   // Don't even try if the last key was already bad
    if (!mDontTryExactMatch) {
      // Regular find, not repeated char find

      // Prefer to find exact match
      rv = FindItNow(nsnull, PR_FALSE, mLinksOnly, mIsFirstVisiblePreferred);
    }

#ifndef NO_LINK_CYCLE_ON_SAME_CHAR
    if (NS_FAILED(rv) && !mLiteralTextSearchOnly &&
        mRepeatingMode == eRepeatingChar) {
      mDontTryExactMatch = PR_TRUE;  // Repeated character find mode
      rv = FindItNow(nsnull, PR_TRUE, PR_TRUE, mIsFirstVisiblePreferred);
    }
#endif
  }

  // ---------Handle success or failure ---------------
  mIsFindingText = PR_FALSE;
  if (NS_SUCCEEDED(rv)) {
    if (mTypeAheadBuffer.Length() == 1) {
      // If first letter, store where the first find succeeded
      // (mStartFindRange)

      mStartFindRange = nsnull;
      nsCOMPtr<nsIDOMRange> startFindRange;
      mFocusedDocSelection->GetRangeAt(0, getter_AddRefs(startFindRange));

      if (startFindRange) {
        startFindRange->CloneRange(getter_AddRefs(mStartFindRange));
      }
    }
  }
  else {
    DisplayStatus(PR_FALSE, nsnull, PR_FALSE); // Display failure status
    mRepeatingMode = eRepeatingNone;

    ++mBadKeysSinceMatch;
    // Error beep (don't been when backspace is pressed, they're 
    // trying to correct the mistake!)
    nsCOMPtr<nsISound> soundInterface =
      do_CreateInstance("@mozilla.org/sound;1");
    if (soundInterface) {
      soundInterface->Beep();
    }
    // Remove bad character from buffer, so we can continue typing from
    // last matched character
    if (mTypeAheadBuffer.Length() >= 1) {
      mTypeAheadBuffer.Truncate(mTypeAheadBuffer.Length() - 1);
    }
  }

  SaveFind();

  return NS_OK;
}


void
nsTypeAheadFind::SaveFind()
{
  // Store find string for find-next
  mFindNextBuffer = mTypeAheadBuffer;

  nsCOMPtr<nsIWebBrowserFind> webBrowserFind;
  GetWebBrowserFind(getter_AddRefs(webBrowserFind));
  if (webBrowserFind) {
    webBrowserFind->SetSearchString(PromiseFlatString(mTypeAheadBuffer).get());
  }
  
  if (!mFindService) {
    mFindService = do_GetService("@mozilla.org/find/find_service;1");
  }
  if (mFindService) {
    mFindService->SetSearchString(mTypeAheadBuffer);
  }

  // --- If accessibility.typeaheadfind.timeout is set,
  //     cancel find after specified # milliseconds ---
  StartTimeout();
}


NS_IMETHODIMP
nsTypeAheadFind::HandleText(nsIDOMEvent* aTextEvent)
{
  // This is called multiple times in the middle of an 
  // IME composition

  if (!mIsIMETypeAheadActive) {
    return NS_OK;
  }

  // ------- Check if Type Ahead can occur here -------------
  // (and if it can, get the target content and document)
  nsCOMPtr<nsIContent> targetContent;
  nsCOMPtr<nsIPresShell> targetPresShell;
  GetTargetIfTypeAheadOkay(aTextEvent, getter_AddRefs(targetContent), 
    getter_AddRefs(targetPresShell));
  if (!targetContent || !targetPresShell) {
    mIsIMETypeAheadActive = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrivateTextEvent> textEvent(do_QueryInterface(aTextEvent));
  if (!textEvent)
    return NS_OK;

  textEvent->GetText(mIMEString);
  if (mIMEString.IsEmpty())
    return NS_OK;

  // show the candidate char/word in the status bar
  DisplayStatus(PR_FALSE, nsnull, PR_FALSE, mIMEString.get());

  // --------- Position the IME window --------------
  // XXX - what do we do with this, is it even necessary?
  //       should we position it in a consistent place?
  nsTextEventReply *textEventReply;
  textEvent->GetEventReply(&textEventReply);

  nsCOMPtr<nsICaret> caret;
  targetPresShell->GetCaret(getter_AddRefs(caret));
  NS_ENSURE_TRUE(caret, NS_ERROR_FAILURE);

  // Reset caret coordinates, so that IM window can move with us
  caret->GetCaretCoordinates(nsICaret::eIMECoordinates, mFocusedDocSelection,
    &(textEventReply->mCursorPosition), &(textEventReply->mCursorIsCollapsed), nsnull);

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::HandleStartComposition(nsIDOMEvent* aCompositionEvent)
{
  // This is called once at the start of an IME composition

  mIsIMETypeAheadActive = PR_TRUE;

  if (!mIsTypeAheadOn || mIsMenuBarActive || mIsMenuPopupActive) {
    mIsIMETypeAheadActive = PR_FALSE;
    return NS_OK;
  }

  // Pause the cancellation timer until IME is finished
  // HandleChar() will start it again
  if (mTimer) {
    mTimer->Cancel();
  }

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::HandleEndComposition(nsIDOMEvent* aCompositionEvent)
{
  // This is called once at the end of an IME composition

  if (!mIsIMETypeAheadActive) {
    return PR_FALSE;
  }

  // -------- Find the composed chars one at a time ---------
  nsReadingIterator<PRUnichar> iter;
  nsReadingIterator<PRUnichar> iterEnd;

  mIMEString.BeginReading(iter);
  mIMEString.EndReading(iterEnd);

  // Handle the characters one at a time
  while (iter != iterEnd) {
    if (NS_FAILED(HandleChar(*iter))) {
      // Character not found, exit loop early
      break;
    }
    ++iter;
  }

  mIMEString.Truncate(); // To be safe, so that find won't happen twice

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::HandleQueryComposition(nsIDOMEvent* aCompositionEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::HandleQueryReconversion(nsIDOMEvent* aCompositionEvent)
{
  return NS_OK;
}


nsresult
nsTypeAheadFind::FindItNow(nsIPresShell *aPresShell,
                           PRBool aIsRepeatingSameChar, PRBool aIsLinksOnly,
                           PRBool aIsFirstVisiblePreferred)
{
  nsCOMPtr<nsIPresShell> presShell(aPresShell);
  nsCOMPtr<nsIPresShell> startingPresShell =
    do_QueryReferent(mFocusedWeakShell);

  if (!presShell) {
    presShell = startingPresShell;  // this is the current document

    if (!presShell) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));

  if (!presContext) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> currentContainer, startingContainer;
  presContext->GetContainer(getter_AddRefs(startingContainer));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(startingContainer));
  nsCOMPtr<nsIDocShellTreeItem> rootContentTreeItem;
  nsCOMPtr<nsIDocShell> currentDocShell;
  nsCOMPtr<nsIDocShell> startingDocShell(do_QueryInterface(startingContainer));

  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(rootContentTreeItem));
  nsCOMPtr<nsIDocShell> rootContentDocShell =
    do_QueryInterface(rootContentTreeItem);

  if (!rootContentDocShell) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                             nsIDocShell::ENUMERATE_FORWARDS,
                                             getter_AddRefs(docShellEnumerator));

  // Default: can start at the current document
  currentContainer = startingContainer =
    do_QueryInterface(rootContentDocShell);

  // Iterate up to current shell, if there's more than 1 that we're
  // dealing with
  PRBool hasMoreDocShells;

  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
         && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    currentDocShell = do_QueryInterface(currentContainer);
    if (!currentDocShell || currentDocShell == startingDocShell ||
        aIsFirstVisiblePreferred) {
      break;
    }
  }

  // ------------ Get ranges ready ----------------
  nsCOMPtr<nsIDOMRange> returnRange;
  if (NS_FAILED(GetSearchContainers(currentContainer, aIsRepeatingSameChar,
                                    aIsFirstVisiblePreferred, 
                                    !aIsFirstVisiblePreferred || mStartFindRange,
                                    getter_AddRefs(presShell),
                                    getter_AddRefs(presContext)))) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 rangeCompareResult = 0;
  mStartPointRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START,
                                          mSearchRange, &rangeCompareResult);
  // No need to wrap find in doc if starting at beginning
  PRBool hasWrapped = (rangeCompareResult <= 0);

  nsAutoString findBuffer;
  if (aIsRepeatingSameChar) {
    findBuffer = mTypeAheadBuffer.First();
  } else {
    findBuffer = PromiseFlatString(mTypeAheadBuffer);
  }

  if (findBuffer.IsEmpty())
    return NS_ERROR_FAILURE;

  mFind->SetFindBackwards(mRepeatingMode == eRepeatingCharReverse ||
    mRepeatingMode == eRepeatingReverse);

  while (PR_TRUE) {    // ----- Outer while loop: go through all docs -----
    while (PR_TRUE) {  // === Inner while loop: go through a single doc ===
      mFind->Find(findBuffer.get(), mSearchRange, mStartPointRange,
                  mEndPointRange, getter_AddRefs(returnRange));
      if (!returnRange) {
        break;  // Nothing found in this doc, go to outer loop (try next doc)
      }

      // ------- Test resulting found range for success conditions ------
      PRBool isInsideLink = PR_FALSE, isStartingLink = PR_FALSE;

      if (aIsLinksOnly) {
        // Don't check if inside link when searching all text

        RangeStartsInsideLink(returnRange, presShell, &isInsideLink,
                              &isStartingLink);
      }

      if (!IsRangeVisible(presShell, presContext, returnRange,
                          aIsFirstVisiblePreferred, PR_FALSE,
                          getter_AddRefs(mStartPointRange)) ||
          (aIsRepeatingSameChar && !isStartingLink) ||
          (aIsLinksOnly && !isInsideLink) ||
          (mStartLinksOnlyPref && aIsLinksOnly && !isStartingLink)) {
        // ------ Failure ------
        // Start find again from here
        returnRange->CloneRange(getter_AddRefs(mStartPointRange));

        // Collapse to end
        mStartPointRange->Collapse(mRepeatingMode == eRepeatingReverse || 
          mRepeatingMode == eRepeatingCharReverse);

        continue;
      }

      // ------ Success! -------
      // Make sure new document is selected
      if (presShell != startingPresShell) {
        // We are in a new document (because of frames/iframes)
        mFocusedDocSelection->CollapseToStart(); // Hide old doc's selection
        SetSelectionLook(startingPresShell, PR_FALSE, PR_FALSE); // hide caret

        nsCOMPtr<nsIDocument> doc;
        presShell->GetDocument(getter_AddRefs(doc));

        if (!doc) {
          return NS_ERROR_FAILURE;
        }

        mFocusedWeakShell = do_GetWeakReference(presShell);
        nsCOMPtr<nsIContent> docContent;
        doc->GetRootContent(getter_AddRefs(docContent));
        if (docContent) {
          docContent->SetFocus(presContext);
        }
        // Get selection controller and selection for new frame/iframe
        GetSelection(presShell, getter_AddRefs(mFocusedDocSelCon), 
                     getter_AddRefs(mFocusedDocSelection));
      }

      // Select the found text and focus it
      mFocusedDocSelection->RemoveAllRanges();
      mFocusedDocSelection->AddRange(returnRange);
      mFocusedDocSelCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                                 nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
      SetSelectionLook(presShell, PR_TRUE, mRepeatingMode != eRepeatingForward 
                                           && mRepeatingMode != eRepeatingReverse);

      nsCOMPtr<nsIEventStateManager> esm;
      presContext->GetEventStateManager(getter_AddRefs(esm));
      nsCOMPtr<nsIContent> focusedContent;
      if (esm) {
        PRBool isSelectionWithFocus;
        esm->MoveFocusToCaret(PR_TRUE, &isSelectionWithFocus);
        esm->GetFocusedContent(getter_AddRefs(focusedContent));
      }

      DisplayStatus(PR_TRUE, focusedContent, PR_FALSE);

      mBadKeysSinceMatch = 0;

      return NS_OK;
    }

    // ======= end-inner-while (go through a single document) ==========

    // ---------- Nothing found yet, try next document  -------------
    do {
      // ==== Second inner loop - get another while  ====
      if (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
          && hasMoreDocShells) {
        docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
        NS_ASSERTION(currentContainer, "HasMoreElements lied to us!");
        if (NS_FAILED(GetSearchContainers(currentContainer,
                                          aIsRepeatingSameChar,
                                          aIsFirstVisiblePreferred, PR_FALSE,
                                          getter_AddRefs(presShell),
                                          getter_AddRefs(presContext)))) {
          return NS_ERROR_FAILURE;
        }

        currentDocShell = do_QueryInterface(currentContainer);

        if (currentDocShell) {
          break;
        }
      }

      // Reached last doc shell, loop around back to first doc shell
      rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                                 nsIDocShell::ENUMERATE_FORWARDS,
                                                 getter_AddRefs(docShellEnumerator));
    } while (docShellEnumerator);  // ==== end second inner while  ===

    if (currentDocShell != startingDocShell) {
      continue;  // Try next document
    }

    // Finished searching through docshells:
    // If aFirstVisiblePreferred == PR_TRUE, we may need to go through all
    // docshells twice -once to look for visible matches, the second time
    // for any match
    if (!hasWrapped || aIsFirstVisiblePreferred) {
      aIsFirstVisiblePreferred = PR_FALSE;
      hasWrapped = PR_TRUE;

      continue;  // Go through all docs again
    }

    // ------------- Failed --------------
    break;
  }   // end-outer-while: go through all docs

  return NS_ERROR_FAILURE;
}


nsresult
nsTypeAheadFind::GetSearchContainers(nsISupports *aContainer,
                                     PRBool aIsRepeatingSameChar,
                                     PRBool aIsFirstVisiblePreferred,
                                     PRBool aCanUseDocSelection,
                                     nsIPresShell **aPresShell,
                                     nsIPresContext **aPresContext)
{
  *aPresShell = nsnull;
  *aPresContext = nsnull;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
  if (!docShell) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresContext> presContext;
  nsCOMPtr<nsIPresShell> presShell;

  docShell->GetPresShell(getter_AddRefs(presShell));
  docShell->GetPresContext(getter_AddRefs(presContext));

  if (!presShell || !presContext) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));

  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> rootContent;
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(doc));
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLElement> bodyEl;
    htmlDoc->GetBody(getter_AddRefs(bodyEl));
    rootContent = do_QueryInterface(bodyEl);
  }
  if (!rootContent) {
    doc->GetRootContent(getter_AddRefs(rootContent));
  }
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootContent));

  if (!rootNode) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 childCount;

  if (NS_FAILED(rootContent->ChildCount(childCount))) {
    return NS_ERROR_FAILURE;
  }

  mSearchRange->SelectNodeContents(rootNode);

  mEndPointRange->SetEnd(rootNode, childCount);
  mEndPointRange->Collapse(PR_FALSE); // collapse to end

  // Consider current selection as null if
  // it's not in the currently focused document
  nsCOMPtr<nsIDOMRange> currentSelectionRange;
  nsCOMPtr<nsIPresShell> selectionPresShell =
    do_QueryReferent(mFocusedWeakShell);

  if (aCanUseDocSelection && selectionPresShell == presShell) {
    mFocusedDocSelection->GetRangeAt(0, getter_AddRefs(currentSelectionRange));
  }

  if (!currentSelectionRange) {
    // Ensure visible range, move forward if necessary
    // This uses ignores the return value, but usese the side effect of
    // IsRangeVisible. It returns the first visible range after searchRange
    IsRangeVisible(presShell, presContext, mSearchRange, 
                   aIsFirstVisiblePreferred, PR_TRUE, 
                   getter_AddRefs(mStartPointRange));
  }
  else {
    PRInt32 startOffset;
    nsCOMPtr<nsIDOMNode> startNode;
    if ((aIsRepeatingSameChar && mRepeatingMode != eRepeatingCharReverse) || 
        mRepeatingMode == eRepeatingForward) {
      currentSelectionRange->GetEndContainer(getter_AddRefs(startNode));
      currentSelectionRange->GetEndOffset(&startOffset);
    }
    else {
      currentSelectionRange->GetStartContainer(getter_AddRefs(startNode));
      currentSelectionRange->GetStartOffset(&startOffset);
    }
    if (!startNode) {
      startNode = rootNode;
    }

    // We need to set the start point this way, other methods haven't worked
    mStartPointRange->SelectNode(startNode);
    mStartPointRange->SetStart(startNode, startOffset);
    mStartPointRange->Collapse(PR_TRUE); // collapse to start
  }

  *aPresShell = presShell;
  NS_ADDREF(*aPresShell);

  *aPresContext = presContext;
  NS_ADDREF(*aPresContext);

  return NS_OK;
}


void
nsTypeAheadFind::RangeStartsInsideLink(nsIDOMRange *aRange,
                                       nsIPresShell *aPresShell,
                                       PRBool *aIsInsideLink,
                                       PRBool *aIsStartingLink)
{
  *aIsInsideLink = PR_FALSE;
  *aIsStartingLink = PR_TRUE;

  // ------- Get nsIContent to test -------
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIContent> startContent, origContent;
  aRange->GetStartContainer(getter_AddRefs(startNode));
  PRInt32 startOffset;
  aRange->GetStartOffset(&startOffset);

  nsCOMPtr<nsIContent> childContent;
  PRBool canContainChildren;

  startContent = do_QueryInterface(startNode);
  if (!startContent) {
    NS_NOTREACHED("startContent should never be null");
    return;
  }
  origContent = startContent;

  if (NS_SUCCEEDED(startContent->CanContainChildren(canContainChildren)) &&
      canContainChildren) {
    startContent->ChildAt(startOffset, *getter_AddRefs(childContent));
    if (childContent) {
      startContent = childContent;
    }
  }
  else if (startOffset > 0) {
    nsCOMPtr<nsITextContent> textContent(do_QueryInterface(startContent));

    if (textContent) {
      // look for non whitespace character before start offset
      const nsTextFragment *textFrag;
      textContent->GetText(&textFrag);

      for (PRInt32 index = 0; index < startOffset; index++) {
        if (!XP_IS_SPACE(textFrag->CharAt(index))) {
          *aIsStartingLink = PR_FALSE;  // not at start of a node

          break;
        }
      }
    }
  }

  // ------- Check to see if inside link ---------

  // We now have the correct start node for the range
  // Search for links, starting with startNode, and going up parent chain

  nsCOMPtr<nsIAtom> tag, hrefAtom(do_GetAtom("href"));
  nsCOMPtr<nsIAtom> typeAtom(do_GetAtom("type"));

  while (PR_TRUE) {
    // Keep testing while textContent is equal to something,
    // eventually we'll run out of ancestors

    if (startContent->IsContentOfType(nsIContent::eHTML)) {
      nsCOMPtr<nsILink> link(do_QueryInterface(startContent));
      if (link) {
        // Check to see if inside HTML link
        *aIsInsideLink = startContent->HasAttr(kNameSpaceID_None, hrefAtom);
        return;
      }
    }
    else {
      // Any xml element can be an xlink
      *aIsInsideLink = startContent->HasAttr(kNameSpaceID_XLink, hrefAtom);
      if (*aIsInsideLink) {
        nsAutoString xlinkType;
        startContent->GetAttr(kNameSpaceID_XLink, typeAtom, xlinkType);
        if (!xlinkType.Equals(NS_LITERAL_STRING("simple"))) {
          *aIsInsideLink = PR_FALSE;  // Xlink must be type="simple"
        }

        return;
      }
    }

    // Get the parent
    nsCOMPtr<nsIContent> parent, parentsFirstChild;
    startContent->GetParent(*getter_AddRefs(parent));
    if (parent) {
      parent->ChildAt(0, *getter_AddRefs(parentsFirstChild));
      nsCOMPtr<nsITextContent> textContent =
        do_QueryInterface(parentsFirstChild);

      if (textContent) {
        // We don't want to look at a whitespace-only first child
        PRBool isOnlyWhitespace;
        textContent->IsOnlyWhitespace(&isOnlyWhitespace);
        if (isOnlyWhitespace)
          parent->ChildAt(1, *getter_AddRefs(parentsFirstChild));
      }

      if (parentsFirstChild != startContent) {
        // startContent wasn't a first child, so we conclude that
        // if this is inside a link, it's not at the beginning of it
        *aIsStartingLink = PR_FALSE;
      }

      startContent = parent;
    }
    else
      break;
  }

  *aIsStartingLink = PR_FALSE;
}


NS_IMETHODIMP
nsTypeAheadFind::ScrollPositionWillChange(nsIScrollableView *aView,
                                          nscoord aX, nscoord aY)
{
  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::ScrollPositionDidChange(nsIScrollableView *aScrollableView,
                                         nscoord aX, nscoord aY)
{
  if (!mIsFindingText)
    CancelFind();

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::NotifySelectionChanged(nsIDOMDocument *aDoc,
                                        nsISelection *aSel, short aReason)
{
  if (!mIsFindingText) {
    if (mRepeatingMode != eRepeatingNone) {
      // Selection had changed color for Type Ahead Find's version of Accel+G
      // We change it back when the selection changes from someone else
      nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
      SetSelectionLook(presShell, PR_FALSE, PR_FALSE);
    }
    CancelFind();
  }

  return NS_OK;
}


// ---------------- nsITypeAheadFind --------------------

NS_IMETHODIMP
nsTypeAheadFind::FindNext(PRBool aFindBackwards, nsISupportsPRBool *aDidExecute)
{
  NS_ENSURE_TRUE(aDidExecute, NS_ERROR_FAILURE);

  aDidExecute->SetData(PR_FALSE);
  if (!mIsFindAllowedInWindow || mFindNextBuffer.IsEmpty() || !mFocusedWindow) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> privateWindow(do_QueryInterface(mFocusedWindow));
  NS_ENSURE_TRUE(privateWindow, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFocusController> focusController;
  privateWindow->GetRootFocusController(getter_AddRefs(focusController));
  NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindowInternal> callerWinInternal;
  focusController->GetFocusedWindow(getter_AddRefs(callerWinInternal));
  nsCOMPtr<nsIDOMWindow> callerWin(do_QueryInterface(callerWinInternal));
  NS_ENSURE_TRUE(callerWin, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> typeAheadPresShell(do_QueryReferent(mFocusedWeakShell));
  NS_ENSURE_TRUE(typeAheadPresShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> callerDomDoc;
  callerWin->GetDocument(getter_AddRefs(callerDomDoc));
  nsCOMPtr<nsIDocument> callerDoc(do_QueryInterface(callerDomDoc));
  NS_ENSURE_TRUE(callerDoc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> callerPresShell;
  callerDoc->GetShellAt(0, getter_AddRefs(callerPresShell));
  NS_ENSURE_TRUE(callerPresShell, NS_ERROR_FAILURE);

  if (callerWin != mFocusedWindow || callerPresShell != typeAheadPresShell) {
    // This means typeaheadfind is active in a different window or doc
    // So it's not appropriate to find next for the current window
    mFindNextBuffer.Truncate();
    return NS_OK;
  }

  nsCOMPtr<nsIWebBrowserFind> webBrowserFind;
  GetWebBrowserFind(getter_AddRefs(webBrowserFind));
  NS_ENSURE_TRUE(webBrowserFind, NS_ERROR_FAILURE);

  nsXPIDLString webBrowserFindString;
  if (webBrowserFind) {
    webBrowserFind->GetSearchString(getter_Copies(webBrowserFindString));
    if (!webBrowserFindString.Equals(mFindNextBuffer)) {
      // If they're not equal, then the find dialog was used last,
      // not typeaheadfind. Typeaheadfind applies to the last find,
      // so we should let nsIWebBrowserFind::FindNext() do it.
      mFindNextBuffer.Truncate();
      return NS_OK;
    }

  }

  /* -------------------------------------------------------
   * Typeaheadfind is active in the currently focused window,
   * so do the find next operation now
   */

  aDidExecute->SetData(PR_TRUE);

  if (mBadKeysSinceMatch > 0) {
    // We know it will fail, so just return
    return NS_OK;
  }

  mTypeAheadBuffer = mFindNextBuffer;
  PRBool repeatingSameChar = PR_FALSE;

  if (mRepeatingMode == eRepeatingChar || 
      mRepeatingMode == eRepeatingCharReverse) {
    mRepeatingMode = aFindBackwards? eRepeatingCharReverse: eRepeatingChar;
    repeatingSameChar = PR_TRUE;
  }
  else {
    mRepeatingMode = aFindBackwards? eRepeatingReverse: eRepeatingForward;
  }
  mLiteralTextSearchOnly = PR_TRUE;

  mIsFindingText = PR_TRUE; // prevent our listeners from calling CancelFind()

  if (NS_FAILED(FindItNow(nsnull, repeatingSameChar, mLinksOnly, PR_FALSE))) {
    DisplayStatus(PR_FALSE, nsnull, PR_FALSE); // Display failure status
    mRepeatingMode = eRepeatingNone;
  }

  mTypeAheadBuffer.Truncate(); // Find buffer is now in mFindNextBuffer
  StartTimeout();
  mIsFindingText = PR_FALSE;

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::GetIsActive(PRBool *aIsActive)
{
  *aIsActive = !mTypeAheadBuffer.IsEmpty();

  return NS_OK;
}


/*
 * Start new type ahead find manually
 */

NS_IMETHODIMP
nsTypeAheadFind::StartNewFind(nsIDOMWindow *aWindow, PRBool aLinksOnly)
{
  if (!mFind)
    return NS_ERROR_FAILURE;  // Type Ahead Find not correctly initialized

  nsCOMPtr<nsIDOMWindowInternal> windowInternal(do_QueryInterface(aWindow));
  if (!windowInternal)
    return NS_ERROR_FAILURE;

  AttachWindowListeners(aWindow);
  if (mFocusedWindow != aWindow) {
    // This routine will set up the keypress and other listeners
    UseInWindow(aWindow);
  }
  mLinksOnly = aLinksOnly;

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::SetAutoStart(nsIDOMWindow *aDOMWin, PRBool aAutoStartOn)
{
  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(aDOMWin));
  PRInt32 index = mManualFindWindows->IndexOf(windowSupports);

  if (aAutoStartOn) {
    if (index >= 0) {
      // Remove from list of windows requiring manual find
      mManualFindWindows->RemoveElementAt(index);
    }

    AttachWindowListeners(aDOMWin);
  }
  else {  // Should be in list of windows requiring manual find
    if (aDOMWin == mFocusedWindow) {
      CancelFind();
    }

    RemoveWindowListeners(aDOMWin);

    if (index < 0) {  // Should be in list of windows requiring manual find
      mManualFindWindows->InsertElementAt(windowSupports, 0);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::GetAutoStart(nsIDOMWindow *aDOMWin, PRBool *aIsAutoStartOn)
{
  *aIsAutoStartOn = PR_FALSE;

  nsCOMPtr<nsIInterfaceRequestor> ifreq(do_QueryInterface(aDOMWin));
  NS_ENSURE_TRUE(ifreq, NS_OK);

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(ifreq));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(webNav));
  NS_ENSURE_TRUE(treeItem, NS_OK);

  // Don't listen for events on chrome windows
  PRInt32 itemType = nsIDocShellTreeItem::typeChrome;
  if (treeItem) {
    treeItem->GetItemType(&itemType);
  }

  if (itemType == nsIDocShellTreeItem::typeChrome) {
    return NS_OK;
  }

  // Check for editor or message pane
  nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(treeItem));
  if (editorDocShell) {
    PRBool isEditable;
    editorDocShell->GetEditable(&isEditable);

    if (isEditable) {
      return NS_OK;
    }
  }
  // XXX the manual id check for the mailnews message pane looks like a hack,
  // but is necessary. We conflict with mailnews single key shortcuts like "n"
  // for next unread message.
  // We need this manual check in Mozilla 1.0 and Netscape 7.0
  // because people will be using XPI's to install us there, so we have to
  // do our check from here rather than create a new interface or attribute on
  // the browser element.
  // XXX We'll use autotypeaheadfind="false" in future trunk builds

  nsCOMPtr<nsIDOMDocument> domDoc;
  aDOMWin->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(doc, NS_OK);

  nsCOMPtr<nsIDocument> parentDoc;
  doc->GetParentDocument(getter_AddRefs(parentDoc));
  if (parentDoc) {
    nsCOMPtr<nsIContent> browserElContent;
    // get content for <browser>
    parentDoc->FindContentForSubDocument(doc,
                                         getter_AddRefs(browserElContent));
    nsCOMPtr<nsIDOMElement> browserElement =
      do_QueryInterface(browserElContent);

    if (browserElement) {
      nsAutoString id, tagName, autoFind;
      browserElement->GetLocalName(tagName);
      browserElement->GetAttribute(NS_LITERAL_STRING("id"), id);
      browserElement->GetAttribute(NS_LITERAL_STRING("autotypeaheadfind"),
                                   autoFind);
      if (tagName.EqualsWithConversion("editor") ||
          id.EqualsWithConversion("messagepane") ||
          autoFind.EqualsWithConversion("false")) {
        return NS_OK;
      }
    }
  }

  // Is this window stored in manual find windows list?
  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(aDOMWin));
  *aIsAutoStartOn = mManualFindWindows->IndexOf(windowSupports) < 0;

  return NS_OK;
}


NS_IMETHODIMP
nsTypeAheadFind::CancelFind()
{
  // Stop current find if:
  //   1. Escape pressed
  //   2. Selection is moved/changed
  //   3. User clicks in window (if it changes the selection)
  //   4. Window scrolls
  //   5. User tabs (this can move the selection)
  //   6. Timer expires

  if (!mTypeAheadBuffer.IsEmpty() || mRepeatingMode != eRepeatingNone) {
    mTypeAheadBuffer.Truncate();
    DisplayStatus(PR_FALSE, nsnull, PR_TRUE); // Clear status
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
    SetSelectionLook(presShell, PR_FALSE, PR_FALSE);
  }

  // This is set to true if the user types / (all text) or ' (links only) first
  mLinksOnlyManuallySet = PR_FALSE;

  // These will be initialized to their true values after
  // the first character is typed
  mCaretBrowsingOn = PR_FALSE;
  mLiteralTextSearchOnly = PR_FALSE;
  mDontTryExactMatch = PR_FALSE;
  mStartFindRange = nsnull;
  mBadKeysSinceMatch = 0;

  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(mFocusedWindow));
  if (mManualFindWindows->IndexOf(windowSupports) >= 0) {
    RemoveDocListeners();
    RemoveWindowListeners(mFocusedWindow);
    mIsFindAllowedInWindow = PR_FALSE;
  }

  return NS_OK;
}


// ------- Helper Methods ---------------

nsresult
nsTypeAheadFind::GetWebBrowserFind(nsIWebBrowserFind **aWebBrowserFind)
{
  *aWebBrowserFind = nsnull;

  nsCOMPtr<nsIInterfaceRequestor> ifreq(do_QueryInterface(mFocusedWindow));
  NS_ENSURE_TRUE(ifreq, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(ifreq));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWebBrowserFind> webBrowserFind(do_GetInterface(docShell));
  NS_ENSURE_TRUE(webBrowserFind, NS_ERROR_FAILURE);

  NS_ADDREF(*aWebBrowserFind = webBrowserFind);

  return NS_OK;
}


void
nsTypeAheadFind::StartTimeout()
{
  if (mTimeoutLength) {
    if (!mTimer) {
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
    }

    if (mTimer) {
      mTimer->InitWithCallback(this, mTimeoutLength, nsITimer::TYPE_ONE_SHOT);
    }
  }
}

void
nsTypeAheadFind::SetSelectionLook(nsIPresShell *aPresShell, 
                                  PRBool aChangeColor, 
                                  PRBool aEnabled)
{
  if (!aPresShell || !mFocusedDocSelCon)
    return;

  // Show caret when type ahead find is on
  // Also paint selection bright (typeaheadfind on) or normal
  // (typeaheadfind off)

  if (aChangeColor) {
    mFocusedDocSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ATTENTION);
  } else {
    mFocusedDocSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  }

  mFocusedDocSelCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);

  if (mCaretBrowsingOn) {
    return;  // Leave caret visibility as it is
  }

  nsCOMPtr<nsICaret> caret;
  aPresShell->GetCaret(getter_AddRefs(caret));
  nsCOMPtr<nsILookAndFeel> lookNFeel(do_GetService(kLookAndFeelCID));
  if (!caret || !lookNFeel) {
    return;
  }

  if (aEnabled) {
    // Set caret visible so that it's obvious we're in a live mode
    caret->SetCaretDOMSelection(mFocusedDocSelection);
    caret->SetVisibilityDuringSelection(PR_TRUE);
    caret->SetCaretVisible(PR_TRUE);
    mFocusedDocSelCon->SetCaretEnabled(PR_TRUE);
    PRInt32 pixelWidth = 1;
    lookNFeel->GetMetric(nsILookAndFeel::eMetric_MultiLineCaretWidth,
                         pixelWidth);
    caret->SetCaretWidth(pixelWidth);
  }
  else {
    PRInt32 isCaretVisibleDuringSelection = 0;
    lookNFeel->GetMetric(nsILookAndFeel::eMetric_ShowCaretDuringSelection,
                         isCaretVisibleDuringSelection);
    caret->SetVisibilityDuringSelection(isCaretVisibleDuringSelection != 0);
    nsCOMPtr<nsISelection> caretDomSelection;
    caret->GetCaretDOMSelection(getter_AddRefs(caretDomSelection));
    if (mFocusedDocSelection == caretDomSelection)  {
      mFocusedDocSelCon->SetCaretEnabled(isCaretVisibleDuringSelection != 0);
    }
  }
}


void
nsTypeAheadFind::RemoveDocListeners()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
  nsCOMPtr<nsIViewManager> vm;

  if (presShell) {
    presShell->GetViewManager(getter_AddRefs(vm));
  }

  nsIScrollableView* scrollableView = nsnull;
  if (vm) {
    vm->GetRootScrollableView(&scrollableView);
  }

  if (scrollableView) {
    scrollableView->RemoveScrollPositionListener(this);
  }

  mFocusedWeakShell = nsnull;

  // Remove selection listener
  nsCOMPtr<nsISelectionPrivate> selPrivate =
    do_QueryInterface(mFocusedDocSelection);

  if (selPrivate) {
    selPrivate->RemoveSelectionListener(this); // remove us if we're a listener
  }

  mFocusedDocSelection = nsnull;
}


void
nsTypeAheadFind::AttachDocListeners(nsIPresShell *aPresShell)
{
  if (!aPresShell) {
    return;
  }

  nsCOMPtr<nsIViewManager> vm;
  aPresShell->GetViewManager(getter_AddRefs(vm));
  if (!vm) {
    return;
  }

  nsIScrollableView* scrollableView = nsnull;
  vm->GetRootScrollableView(&scrollableView);
  if (!scrollableView) {
    return;
  }

  scrollableView->AddScrollPositionListener(this);

  // Attach selection listener
  nsCOMPtr<nsISelectionPrivate> selPrivate =
    do_QueryInterface(mFocusedDocSelection);

  if (selPrivate) {
    selPrivate->AddSelectionListener(this);
  }
}


void
nsTypeAheadFind::RemoveWindowListeners(nsIDOMWindow *aDOMWin)
{
  nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
  GetChromeEventHandler(aDOMWin, getter_AddRefs(chromeEventHandler));
  if (!chromeEventHandler) {
    return;
  }

  // Use capturing, otherwise the normal find next will get activated when ours should
  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("keypress"),
                                          NS_STATIC_CAST(nsIDOMKeyListener*, this),
                                          PR_FALSE);

  if (aDOMWin == mFocusedWindow) {
    mFocusedWindow = nsnull;
  }

  // Remove menu listeners
  nsIDOMEventListener *genericEventListener = 
    NS_STATIC_CAST(nsIDOMEventListener*, NS_STATIC_CAST(nsIDOMKeyListener*, this));

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("popupshown"), 
                                          genericEventListener, 
                                          PR_TRUE);

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("popuphidden"), 
                                          genericEventListener, 
                                          PR_TRUE);

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), 
                                          genericEventListener, 
                                          PR_TRUE);

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), 
                                          genericEventListener, 
                                          PR_TRUE);

  // Remove DOM Text listener for IME text events
  nsCOMPtr<nsIDOMEventReceiver> chromeEventReceiver = 
    do_QueryInterface(chromeEventHandler);
  chromeEventReceiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMTextListener*, this), 
    NS_GET_IID(nsIDOMTextListener));
  chromeEventReceiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMCompositionListener*, this), 
    NS_GET_IID(nsIDOMCompositionListener));
}


void
nsTypeAheadFind::AttachWindowListeners(nsIDOMWindow *aDOMWin)
{
  nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
  GetChromeEventHandler(aDOMWin, getter_AddRefs(chromeEventHandler));
  if (!chromeEventHandler) {
    return;
  }

  // Use capturing, otherwise the normal find next will get activated when ours should
  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("keypress"),
                                       NS_STATIC_CAST(nsIDOMKeyListener*, this),
                                       PR_FALSE);

  // Attach menu listeners, this will help us ignore keystrokes meant for menus
  nsIDOMEventListener *genericEventListener = 
    NS_STATIC_CAST(nsIDOMEventListener*, NS_STATIC_CAST(nsIDOMKeyListener*, this));

  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("popupshown"), 
                                       genericEventListener, 
                                       PR_TRUE);

  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("popuphidden"), 
                                        genericEventListener, 
                                       PR_TRUE);

  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), 
                                       genericEventListener, 
                                       PR_TRUE);
  
  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), 
                                       genericEventListener, 
                                       PR_TRUE);

  // Add DOM Text listener for IME text events
  nsCOMPtr<nsIDOMEventReceiver> chromeEventReceiver =
    do_QueryInterface(chromeEventHandler);
  chromeEventReceiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMTextListener*, this), 
    NS_GET_IID(nsIDOMTextListener));
  chromeEventReceiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMCompositionListener*, this), 
    NS_GET_IID(nsIDOMCompositionListener));
}


void
nsTypeAheadFind::GetChromeEventHandler(nsIDOMWindow *aDOMWin,
                                       nsIDOMEventTarget **aChromeTarget)
{
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(aDOMWin));
  nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
  if (privateDOMWindow) {
    privateDOMWindow->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
  }

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

  *aChromeTarget = target;
  NS_IF_ADDREF(*aChromeTarget);
}


nsresult
nsTypeAheadFind::GetTargetIfTypeAheadOkay(nsIDOMEvent *aEvent, 
                                          nsIContent **aTargetContent,
                                          nsIPresShell **aTargetPresShell)
{
  *aTargetContent = nsnull;
  *aTargetPresShell = nsnull;

  nsCOMPtr<nsIDOMNSEvent> nsEvent(do_QueryInterface(aEvent));
  if (!nsEvent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  nsEvent->GetOriginalTarget(getter_AddRefs(domEventTarget));

  nsCOMPtr<nsIContent> targetContent(do_QueryInterface(domEventTarget));

  // ---- Exit early if in form controls that can be typed in ---------

  if (!targetContent) {
    return NS_OK;
  }

  if (targetContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {
    nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(targetContent));
    PRInt32 controlType;
    formControl->GetType(&controlType);
    if (controlType == NS_FORM_SELECT || 
        controlType == NS_FORM_TEXTAREA ||
        controlType == NS_FORM_INPUT_TEXT ||
        controlType == NS_FORM_INPUT_PASSWORD ||
        controlType == NS_FORM_INPUT_FILE) {
      // Don't steal keys from these form controls 
      // - selects have their own incremental find for options
      // - text fields need to allow typing
      if (!mTypeAheadBuffer.IsEmpty()) {
        CancelFind();
      }
      return NS_OK;
    }
  }
  else if (targetContent->IsContentOfType(nsIContent::eHTML)) {
    // Test for isindex, a deprecated kind of text field. We're using a string 
    // compare because <isindex> is not considered a form control, so it does 
    // not support nsIFormControl or eHTML_FORM_CONTROL, and it's not worth 
    // having a table of atoms just for it. Instead, we're paying for 1 extra 
    // string compare per keystroke, which isn't too bad.
    nsCOMPtr<nsIAtom> targetTagAtom;
    targetContent->GetTag(*getter_AddRefs(targetTagAtom));
    nsAutoString targetTagString;
    targetTagAtom->ToString(targetTagString);
    if (targetTagString.Equals(NS_LITERAL_STRING("isindex"))) {
      if (!mTypeAheadBuffer.IsEmpty())  {
        CancelFind();
      }
      return NS_OK;
    }
  }

  NS_ADDREF(*aTargetContent = targetContent);

  // ---------- Is the keytroke in a new window? -------------------

  nsCOMPtr<nsIDocument> doc;
  if (NS_FAILED(targetContent->GetDocument(*getter_AddRefs(doc))) || !doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
  doc->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
  nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(ourGlobal));

  if (domWin != mFocusedWindow) {
    GetAutoStart(domWin, &mIsFindAllowedInWindow);
    if (mIsFindAllowedInWindow) {
      UseInWindow(domWin);
    }
    mFocusedWindow = domWin;
  }
  if (!mIsFindAllowedInWindow) {
    return NS_OK;
  }


  // ---------- Get presshell -----------

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell) {
    return NS_OK;
  }
  nsCOMPtr<nsIPresShell> lastShell(do_QueryReferent(mFocusedWeakShell));
  if (lastShell != presShell) {
    // Same window, but a new document, so start fresh
    UseInWindow(domWin);
  }

  NS_ADDREF(*aTargetPresShell = presShell);  
  return NS_OK;
}


void
nsTypeAheadFind::GetSelection(nsIPresShell *aPresShell,
                              nsISelectionController **aSelCon,
                              nsISelection **aDOMSel)
{
  // if aCurrentNode is nsnull, get selection for document
  *aDOMSel = nsnull;

  nsCOMPtr<nsIPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));

  nsIFrame *frame = nsnull;
  aPresShell->GetRootFrame(&frame);

  if (presContext && frame) {
    frame->GetSelectionController(presContext, aSelCon);
    if (*aSelCon) {
      (*aSelCon)->GetSelection(nsISelectionController::SELECTION_NORMAL,
                               aDOMSel);
    }
  }
}


PRBool
nsTypeAheadFind::IsRangeVisible(nsIPresShell *aPresShell,
                                nsIPresContext *aPresContext,
                                nsIDOMRange *aRange, PRBool aMustBeInViewPort,
                                PRBool aGetTopVisibleLeaf,
                                nsIDOMRange **aFirstVisibleRange)
{
  // We need to know if the range start is visible.
  // Otherwise, return a the first visible range start 
  // in aFirstVisibleRange

  aRange->CloneRange(aFirstVisibleRange);
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content) {
    return PR_FALSE;
  }

  nsIFrame *frame = nsnull;
  aPresShell->GetPrimaryFrameFor(content, &frame);
  if (!frame) {
    // No frame! Not visible then.

    return PR_FALSE;
  }

  // ---- We have a frame ----
  if (!aMustBeInViewPort) {
    //  Don't need it to be on screen, just in rendering tree

    return PR_TRUE;
  }

  // Get the next in flow frame that contains the range start
  PRInt32 startRangeOffset, startFrameOffset, endFrameOffset;
  aRange->GetStartOffset(&startRangeOffset);
  while (PR_TRUE) {
    frame->GetOffsets(startFrameOffset, endFrameOffset);
    if (startRangeOffset < endFrameOffset) {
      break;
    }
    nsIFrame *nextInFlowFrame = nsnull;
    frame->GetNextInFlow(&nextInFlowFrame);
    if (nextInFlowFrame) {
      frame = nextInFlowFrame;
    }
    else {
      break;
    }
  }

  // Set up the variables we need, return true if we can't get at them all
  const PRUint16 kMinPixels  = 12;

  nsCOMPtr<nsIViewManager> viewManager;
  aPresShell->GetViewManager(getter_AddRefs(viewManager));
  if (!viewManager) {
    return PR_TRUE;
  }

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate AccGetBounds, because that is
  // more expensive and the STATE_OFFSCREEN flag that this is used
  // for only needs to be a rough indicator
  nsRect relFrameRect;
  nsIView *containingView = nsnull;
  nsPoint frameOffset;
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nsRectVisibility rectVisibility = nsRectVisibility_kAboveViewport;

  if (!aGetTopVisibleLeaf) {
    frame->GetRect(relFrameRect);
    frame->GetOffsetFromView(aPresContext, frameOffset, &containingView);
    if (!containingView) {
      // no view -- not visible

      return PR_FALSE;
    }

    relFrameRect.x = frameOffset.x;
    relFrameRect.y = frameOffset.y;

    viewManager->GetRectVisibility(containingView, relFrameRect,
                                   NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)),
                                   &rectVisibility);

    if (rectVisibility != nsRectVisibility_kAboveViewport &&
        rectVisibility != nsRectVisibility_kZeroAreaRect) {
      return PR_TRUE;
    }
  }

  // We know that the target range isn't usable because it's not in the
  // view port. Move range forward to first visible point,
  // this speeds us up a lot in long documents
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID));
  if (trav) {
    trav->NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF,
                            aPresContext, frame);
  }

  if (!frameTraversal) {
    return PR_FALSE;
  }

  while (rectVisibility == nsRectVisibility_kAboveViewport ||
         rectVisibility == nsRectVisibility_kZeroAreaRect) {
    frameTraversal->Next();
    nsISupports* currentItem;
    frameTraversal->CurrentItem(&currentItem);
    frame = NS_STATIC_CAST(nsIFrame*, currentItem);
    if (!frame) {
      return PR_FALSE;
    }

    frame->GetRect(relFrameRect);
    frame->GetOffsetFromView(aPresContext, frameOffset, &containingView);
    if (containingView) {
      relFrameRect.x = frameOffset.x;
      relFrameRect.y = frameOffset.y;
      viewManager->GetRectVisibility(containingView, relFrameRect,
                                     NS_STATIC_CAST(PRUint16,
                                                    (kMinPixels * p2t)),
                                     &rectVisibility);
    }
  }

  if (frame) {
    nsCOMPtr<nsIContent> firstVisibleContent;
    frame->GetContent(getter_AddRefs(firstVisibleContent));
    nsCOMPtr<nsIDOMNode> firstVisibleNode =
      do_QueryInterface(firstVisibleContent);

    if (firstVisibleNode) {
      (*aFirstVisibleRange)->SelectNode(firstVisibleNode);
      frame->GetOffsets(startFrameOffset, endFrameOffset);
      (*aFirstVisibleRange)->SetStart(firstVisibleNode, startFrameOffset);
      (*aFirstVisibleRange)->Collapse(PR_TRUE);  // Collapse to start
    }
  }

  return PR_FALSE;
}


nsresult
nsTypeAheadFind::GetTranslatedString(const nsAString& aKey,
                                     nsAString& aStringOut)
{
  nsXPIDLString xsValue;

  if (!mStringBundle ||
    NS_FAILED(mStringBundle->GetStringFromName(PromiseFlatString(aKey).get(),
                                               getter_Copies(xsValue)))) {
    return NS_ERROR_FAILURE;
  }

  aStringOut.Assign(xsValue);

  return NS_OK;
}


void
nsTypeAheadFind::DisplayStatus(PRBool aSuccess, nsIContent *aFocusedContent,
                               PRBool aClearStatus, const PRUnichar *aText)
{
  // pres shell -> pres context -> container -> tree item ->
  // tree owner -> browser chrome

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
  if (!presShell) {
    return;
  }

  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return;
  }

  nsCOMPtr<nsISupports> pcContainer;
  presContext->GetContainer(getter_AddRefs(pcContainer));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
  if (!treeItem) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) {
    return;
  }

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  if (!browserChrome) {
    return;
  }

  nsAutoString statusString;
  if (aText) 
    statusString = aText;
  else {
    if (aClearStatus) {
      GetTranslatedString(NS_LITERAL_STRING("stopfind"), statusString);
    } else {
      nsAutoString key;

      if (mLinksOnly) {
        key.Assign(NS_LITERAL_STRING("link"));
      } else {
        key.Assign(NS_LITERAL_STRING("text"));
      }

      if (!aSuccess) {
        key.Append(NS_LITERAL_STRING("not"));
      }

      key.Append(NS_LITERAL_STRING("found"));

      if (NS_SUCCEEDED(GetTranslatedString(key, statusString))) {
        nsAutoString closeQuoteString, urlString;
        GetTranslatedString(NS_LITERAL_STRING("closequote"), closeQuoteString);
        statusString += mTypeAheadBuffer + closeQuoteString;
        nsCOMPtr<nsIDOMNode> focusedNode(do_QueryInterface(aFocusedContent));
        if (focusedNode) {
          presShell->GetLinkLocation(focusedNode, urlString);
        }

        if (!urlString.IsEmpty()) {   // Add URL in parenthesis
          nsAutoString openParenString, closeParenString;
          GetTranslatedString(NS_LITERAL_STRING("openparen"), openParenString);
          GetTranslatedString(NS_LITERAL_STRING("closeparen"), closeParenString);
          statusString += NS_LITERAL_STRING("   ") + openParenString
                          + urlString + closeParenString;
        }
      }
    }
  }

  browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK,
                           PromiseFlatString(statusString).get());
}
