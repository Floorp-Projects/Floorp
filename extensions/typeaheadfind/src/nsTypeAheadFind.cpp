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
#include "nsIChromeEventHandler.h"
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
  mFindService = do_GetService("@mozilla.org/find/find_service;1");
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
  // Focus events on windows are important
  nsCOMPtr<nsIDOMDocument> domDoc;
  aDOMWin->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

  if (!doc) {
    return NS_OK;
  }

  // Focus event in a new doc -- trigger keypress event listening
  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));

  if (!presShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> oldPresShell(do_QueryReferent(mFocusedWeakShell));

  if (!oldPresShell || oldPresShell != presShell) {
    CancelFind();
  } else if (presShell == oldPresShell) {
    // Focus on same window, no need to reattach listeners

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
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));

  // ---------- Analyze keystroke, exit early if possible --------------

  PRUint32 keyCode, charCode;
  PRBool isShift, isCtrl, isAlt, isMeta;
  if (!keyEvent || !targetContent ||
      NS_FAILED(keyEvent->GetKeyCode(&keyCode)) ||
      NS_FAILED(keyEvent->GetCharCode(&charCode)) ||
      NS_FAILED(keyEvent->GetShiftKey(&isShift)) ||
      NS_FAILED(keyEvent->GetCtrlKey(&isCtrl)) ||
      NS_FAILED(keyEvent->GetAltKey(&isAlt)) ||
      NS_FAILED(keyEvent->GetMetaKey(&isMeta))) {
    return NS_ERROR_FAILURE;
  }

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

  if (mBadKeysSinceMatch >= kMaxBadCharsBeforeCancel) {
    // If they're just quickly mashing keys onto the keyboard, stop searching
    // until typeahead find is canceled via timeout or another normal means
    if (mTimer) {
      mTimer->Init(this, mTimeoutLength, nsITimer::TYPE_ONE_SHOT);  // Timeout from last bad key (this one)
    }
    DisplayStatus(PR_FALSE, nsnull, PR_TRUE); // Status message to say find stopped
    return NS_OK;
  }

  // ---------- Check the keystroke --------------------------------
  if (((charCode == 'g' || charCode=='G') && isAlt + isMeta + isCtrl == 1 && (
      (nsTypeAheadFind::sAccelKey == nsIDOMKeyEvent::DOM_VK_CONTROL && isCtrl) ||
      (nsTypeAheadFind::sAccelKey == nsIDOMKeyEvent::DOM_VK_ALT     && isAlt ) ||
      (nsTypeAheadFind::sAccelKey == nsIDOMKeyEvent::DOM_VK_META    && isMeta))) ||
#ifdef XP_MAC
     // We don't use F3 for find next on Macintosh, function keys are user
     // definable there
     ) {
#else
    (keyCode == nsIDOMKeyEvent::DOM_VK_F3 && !isCtrl && !isMeta && !isAlt)) {
#endif
    // We steal Accel+G, F3 (find next) and Accel+Shift+G, Shift+F3
    // (find prev), avoid early return, so we can use our find loop

    // If the global find string is different, that means it's been changed
    // directly by the user, from the last type ahead find string, via the 
    // find dialog. In that case, let the normal find-next happen
    nsAutoString globalFindString;
    mFindService->GetSearchString(globalFindString);
    if (globalFindString != mFindNextBuffer)
      return NS_OK;
    mTypeAheadBuffer = mFindNextBuffer;

    if (mRepeatingMode == eRepeatingChar)
      mTypeAheadBuffer = mTypeAheadBuffer.First();
    mRepeatingMode = (charCode=='G' || isShift)?
      eRepeatingReverse: eRepeatingForward;
    mLiteralTextSearchOnly = PR_TRUE;
    aEvent->PreventDefault(); // Prevent normal processing of this keystroke
  }
  else if ((isAlt && !isShift) || isCtrl || isMeta) {
    // Ignore most modified keys, but alt+shift may be used for
    // entering foreign chars.
    CancelFind();

    return NS_OK;
  }
  else if (mRepeatingMode == eRepeatingForward ||
           mRepeatingMode == eRepeatingReverse) {
    // Once Accel+[shift]+G has been used once,
    // new typing will start a new find
    CancelFind();
  }


  PRBool isFirstVisiblePreferred = PR_FALSE;
  PRBool isBackspace = PR_FALSE;  // When backspace is pressed

  // ------------- Escape pressed ---------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE) {
    // Escape accomplishes 2 things:
    // 1. it is a way for the user to deselect with the keyboard
    // 2. it is a way for the user to cancel incremental find with
    //    visual feedback
    mFocusedDocSelection->CollapseToStart();
    if (mTypeAheadBuffer.IsEmpty()) {
      return NS_OK;
    }

    // If Escape is normally used for a command, don't do it
    aEvent->PreventDefault();
    CancelFind();

    return NS_OK;
  }

  // ----------- Back space ----------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
    if (mTypeAheadBuffer.IsEmpty()) {
      return NS_OK;
    }

    if (mTypeAheadBuffer.Length() == 1) {
      if (mStartFindRange) {
        mFocusedDocSelection->RemoveAllRanges();
        mFocusedDocSelection->AddRange(mStartFindRange);
      }

      mFocusedDocSelection->CollapseToStart();
      CancelFind();
      aEvent->PreventDefault(); // Prevent normal processing of this keystroke

      return NS_OK;
    }

    if (--mBadKeysSinceMatch < 0)
      mBadKeysSinceMatch = 0;
    mTypeAheadBuffer = Substring(mTypeAheadBuffer, 0,
                                 mTypeAheadBuffer.Length() - 1);
    isBackspace = PR_TRUE;
    mDontTryExactMatch = PR_FALSE;
  }
  // ----------- Printable characters --------------
  else if (mRepeatingMode != eRepeatingForward &&
           mRepeatingMode != eRepeatingReverse) {
    PRUnichar uniChar = ToLowerCase(NS_STATIC_CAST(PRUnichar, charCode));

    if (uniChar < ' ') {
      return NS_OK; // Not a printable char
    }

    PRInt32 bufferLength = mTypeAheadBuffer.Length();

    if (uniChar == ' ' && bufferLength == 0) {
      return NS_OK; // We ignore space only if it's the first character
    }

    if (bufferLength > 0) {
      if (mTypeAheadBuffer.First() != uniChar) {
        mRepeatingMode = eRepeatingNone;
      } else if (bufferLength == 1) {
        mRepeatingMode = eRepeatingChar;
      }
    }

    if (bufferLength == 0 && !mLinksOnlyManuallySet) {
      if (uniChar == '`' || uniChar=='\'' || uniChar=='\"') {
        // mLinksOnlyManuallySet = PR_TRUE when the user has already 
        // typed / or '. This allows the next / or ' to get searched for.
        mLinksOnlyManuallySet = PR_TRUE;

        // If you type quote, it starts a links only search
        mLinksOnly = PR_TRUE;

        return NS_OK;
      }

      if (uniChar == '/') {
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

    mTypeAheadBuffer += uniChar;

    if (bufferLength == 0) {
      // --------- Initialize find -----------
      // If you can see the selection (not collapsed or thru caret browsing),
      // or if already focused on a page element, start there.
      // Otherwise we're going to start at the first visible element
      PRBool isSelectionCollapsed;
      mFocusedDocSelection->GetIsCollapsed(&isSelectionCollapsed);

      // If true, we will scan from top left of visible area
      // If false, we will scan from start of selection
      isFirstVisiblePreferred = !mCaretBrowsingOn && isSelectionCollapsed;
      if (isFirstVisiblePreferred) {
        // Get focused content from esm. If it's null, the document is focused.
        // If not, make sure the selection is in sync with the focus, so we can 
        // start our search from there.
        nsCOMPtr<nsIContent> focusedContent;
        nsCOMPtr<nsIPresContext> presContext;
        presShell->GetPresContext(getter_AddRefs(presContext));
        if (!presContext) {
          return NS_OK;
        }
        nsCOMPtr<nsIEventStateManager> esm;
        presContext->GetEventStateManager(getter_AddRefs(esm));
        esm->GetFocusedContent(getter_AddRefs(focusedContent));
        if (focusedContent) {
          esm->MoveCaretToFocus();
          isFirstVisiblePreferred = PR_FALSE;
        }
      }
    }
  }

  mIsFindingText = PR_TRUE; // prevent our listeners from calling CancelFind()

  nsresult rv = NS_ERROR_FAILURE;

  // ----------- Set search options ---------------
  mFind->SetFindBackwards(mRepeatingMode == eRepeatingReverse);

  if (mBadKeysSinceMatch == 0) {   // Don't even try if the last key was already bad
    if (!mDontTryExactMatch) {
      // Regular find, not repeated char find

      // Prefer to find exact match
      rv = FindItNow(PR_FALSE, mLinksOnly, isFirstVisiblePreferred, isBackspace);
    }

#ifndef NO_LINK_CYCLE_ON_SAME_CHAR
    if (NS_FAILED(rv) && !mLiteralTextSearchOnly &&
        mRepeatingMode == eRepeatingChar) {
      mDontTryExactMatch = PR_TRUE;  // Repeated character find mode
      rv = FindItNow(PR_TRUE, PR_TRUE, isFirstVisiblePreferred, isBackspace);
    }
#endif
  }

  aEvent->StopPropagation();  // We're using this key, no one else should

  // --- If accessibility.typeaheadfind.timeout is set,
  //     cancel find after specified # milliseconds ---
  if (mTimeoutLength) {
    if (!mTimer) {
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
    }

    if (mTimer) {
      mTimer->InitWithCallback(this, mTimeoutLength, nsITimer::TYPE_ONE_SHOT);
    }
  }

  mIsFindingText = PR_FALSE;

  if (NS_SUCCEEDED(rv)) {
    // ------- Success!!! -----------------------------------------------------
    // ------- Store current find string for regular find usage: find-next
    //         or find dialog text field ---------
    mBadKeysSinceMatch = 0;
    mFindNextBuffer = mTypeAheadBuffer;
    mFindService->SetSearchString(mFindNextBuffer);
    if (mRepeatingMode == eRepeatingForward || 
        mRepeatingMode == eRepeatingReverse)
      mTypeAheadBuffer.Truncate();

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
    if (!isBackspace)
      ++mBadKeysSinceMatch;
    // ----- Nothing found -----
    DisplayStatus(PR_FALSE, nsnull, PR_FALSE); // Display failure status

    mRepeatingMode = eRepeatingNone;
    if (!isBackspace) {
      // Error beep (don't been when backspace is pressed, they're 
      // trying to correct the mistake!)
      nsCOMPtr<nsISound> soundInterface =
        do_CreateInstance("@mozilla.org/sound;1");
      if (soundInterface) {
        soundInterface->Beep();
      }
    }

    // Remove bad character from buffer, so we can continue typing from
    // last matched character

    // If first character is bad, flush it away anyway
#ifdef TYPEAHEADFIND_REMOVE_ALL_BAD_KEYS
    // Remove all bad characters
    if (mTypeAheadBuffer.Length() >= 1 && !isBackspace) {
#else
    // Remove bad *first* characters only
    if (mTypeAheadBuffer.Length() == 1 && !isBackspace) {
#endif
      // Notice if () in #ifdef above!
      mTypeAheadBuffer = Substring(mTypeAheadBuffer, 0,
                                   mTypeAheadBuffer.Length() - 1);
    }
  }

  return NS_OK;
}


nsresult
nsTypeAheadFind::FindItNow(PRBool aIsRepeatingSameChar, PRBool aIsLinksOnly,
                           PRBool aIsFirstVisiblePreferred,
                           PRBool aIsBackspace)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIPresShell> startingPresShell =
    do_QueryReferent(mFocusedWeakShell);

  // When backspace is pressed, start from where first char was found
  if (aIsBackspace && mStartFindRange) {
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
  }

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
                                    aIsFirstVisiblePreferred, !aIsFirstVisiblePreferred,
                                    getter_AddRefs(presShell),
                                    getter_AddRefs(presContext)))) {
    return NS_ERROR_FAILURE;
  }

  if (aIsBackspace && mStartFindRange && startingDocShell == currentDocShell) {
    // when backspace is pressed, start where first char was found
    mStartFindRange->CloneRange(getter_AddRefs(mStartPointRange));
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
                          aIsFirstVisiblePreferred,
                          getter_AddRefs(mStartPointRange)) ||
          (aIsRepeatingSameChar && !isStartingLink) ||
          (aIsLinksOnly && !isInsideLink) ||
          (mStartLinksOnlyPref && aIsLinksOnly && !isStartingLink)) {
        // ------ Failure ------
        // Start find again from here
        returnRange->CloneRange(getter_AddRefs(mStartPointRange));

        // Collapse to end
        mStartPointRange->Collapse(mRepeatingMode == eRepeatingReverse);

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

    if (aIsRepeatingSameChar && mTypeAheadBuffer.Length() > 1 &&
        mTypeAheadBuffer.First() == mTypeAheadBuffer.CharAt(1) &&
        mTypeAheadBuffer.Last() != mTypeAheadBuffer.First()) {
      // If they repeat the same character and then change, such as aaaab
      // start over with new char as a repeated char find
      mTypeAheadBuffer = mTypeAheadBuffer.Last();

      return FindItNow(PR_TRUE, PR_TRUE, aIsFirstVisiblePreferred,
                       aIsBackspace);
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

  if (!currentSelectionRange || aIsFirstVisiblePreferred) {
    // Ensure visible range, move forward if necessary
    // This uses ignores the return value, but usese the side effect of
    // IsRangeVisible. It returns the first visible range after searchRange
    IsRangeVisible(presShell, presContext, mSearchRange,
                   aIsFirstVisiblePreferred, getter_AddRefs(mStartPointRange));
  }
  else {
    PRInt32 startOffset;
    nsCOMPtr<nsIDOMNode> startNode;
    if (aIsRepeatingSameChar || mRepeatingMode == eRepeatingForward) {
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

    mStartPointRange->SetEnd(startNode, startOffset);
    mStartPointRange->Collapse(PR_FALSE); // collapse to end
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

  nsCOMPtr<nsIAtom> tag;

  while (PR_TRUE) {
    // Keep testing while textContent is equal to something,
    // eventually we'll run out of ancestors

    nsCOMPtr<nsILink> link(do_QueryInterface(startContent));

    if (link) {
      *aIsInsideLink = PR_TRUE;

      return;
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
    CancelFind();
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
  //   2. Window switched
  //   3. User clicks in window
  //   4. Selection is moved/changed
  //   5. Window scrolls
  //   6. Focus is moved

  if (!mTypeAheadBuffer.IsEmpty()) {
    mTypeAheadBuffer.Truncate();
    DisplayStatus(PR_FALSE, nsnull, PR_TRUE); // Clear status
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
    SetSelectionLook(presShell, PR_FALSE, PR_FALSE);
  }
  mLinksOnly = mLinksOnlyPref;
  mLinksOnlyManuallySet = PR_FALSE;

  // These will be initialized to their true values after
  // the first character is typed
  mCaretBrowsingOn = PR_FALSE;
  mRepeatingMode = eRepeatingNone;
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
    caret->SetCaretVisible(isCaretVisibleDuringSelection != 0);
    mFocusedDocSelCon->SetCaretEnabled(isCaretVisibleDuringSelection != 0);
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
  if (chromeEventHandler) {
    // Use capturing, otherwise the normal find next will get activated when ours should
    chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("keypress"),
                                            NS_STATIC_CAST(nsIDOMKeyListener*, this),
                                            PR_TRUE);
  }

  if (aDOMWin == mFocusedWindow) {
    mFocusedWindow = nsnull;
  }

  // Remove menu listeners
  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("popupshown"), 
                                          NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                          PR_TRUE);

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("popuphidden"), 
                                          NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                          PR_TRUE);

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), 
                                          NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                          PR_TRUE);

  chromeEventHandler->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), 
                                          NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                          PR_TRUE);
}


void
nsTypeAheadFind::AttachWindowListeners(nsIDOMWindow *aDOMWin)
{
  nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
  GetChromeEventHandler(aDOMWin, getter_AddRefs(chromeEventHandler));
  if (chromeEventHandler)
    // Use capturing, otherwise the normal find next will get activated when ours should
    chromeEventHandler->AddEventListener(NS_LITERAL_STRING("keypress"),
                                         NS_STATIC_CAST(nsIDOMKeyListener*, this),
                                         PR_TRUE);
  // Attach menu listeners, this will help us ignore keystrokes meant for menus
  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("popupshown"), 
                                       NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                       PR_TRUE);

  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("popuphidden"), 
                                       NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                       PR_TRUE);

  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), 
                                       NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                       PR_TRUE);
  
  chromeEventHandler->AddEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), 
                                       NS_STATIC_CAST(nsIDOMEventListener*, this), 
                                       PR_TRUE);
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
  frame->GetRect(relFrameRect);
  frame->GetOffsetFromView(aPresContext, frameOffset, &containingView);
  if (!containingView) {
    // no view -- not visible

    return PR_FALSE;
  }

  relFrameRect.x = frameOffset.x;
  relFrameRect.y = frameOffset.y;

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nsRectVisibility rectVisibility;
  viewManager->GetRectVisibility(containingView, relFrameRect,
                                 NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)),
                                 &rectVisibility);

  if (rectVisibility != nsRectVisibility_kAboveViewport &&
      rectVisibility != nsRectVisibility_kZeroAreaRect) {
    return PR_TRUE;
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

  while (rectVisibility == nsRectVisibility_kAboveViewport &&
         rectVisibility != nsRectVisibility_kZeroAreaRect) {
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
                               PRBool aClearStatus)
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

  browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK,
                            PromiseFlatString(statusString).get());
}
