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

#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsIWebProgress.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIEditorDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSEvent.h"
#include "nsIPref.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsFrameTraversal.h"
#include "nsIDOMDocument.h"
#include "nsIEventStateManager.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsILink.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"

#include "nsICaret.h"
#include "nsIFocusController.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDocShellTreeItem.h"
#include "nsISound.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsXULAtoms.h"
#include "nsINameSpaceManager.h"

// Header for this class
#include "nsTypeAheadFind.h"

// When true, this prevents listener callbacks from resetting us during typeahead find processing
PRBool nsTypeAheadFind::gIsFindingText = PR_FALSE; 

////////////////////////////////////////////////////////////////////////


NS_INTERFACE_MAP_BEGIN(nsTypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsITypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) 
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTypeAheadFind);
NS_IMPL_RELEASE(nsTypeAheadFind);

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

nsTypeAheadFind* nsTypeAheadFind::mInstance = nsnull;
PRInt32 nsTypeAheadFind::gAccelKey = -1;  // magic value of -1 indicates unitialized state


nsTypeAheadFind::nsTypeAheadFind(): 
  mLinksOnlyPref(PR_FALSE), mLinksOnly(PR_FALSE), mIsTypeAheadOn(PR_FALSE), 
  mCaretBrowsingOn(PR_FALSE), mLiteralTextSearchOnly(PR_FALSE), mKeepSelectionOnCancel(PR_FALSE), 
  mDontTryExactMatch(PR_FALSE), mRepeatingMode(eRepeatingNone), mTimeoutLength(0),
  mFindService(do_GetService("@mozilla.org/find/find_service;1"))
{
  NS_INIT_REFCNT();

#ifdef DEBUG
  // There should only ever be one instance of us
  static PRInt32 gInstanceCount;
  ++gInstanceCount;
  NS_ASSERTION(gInstanceCount == 1, "There should be only 1 instance of nsTypeAheadFind, someone is creating more than 1.");
#endif

  if (!mFindService || NS_FAILED(NS_NewISupportsArray(getter_AddRefs(mManualFindWindows))))
    return;

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs) 
    return;
  
  mFind = do_CreateInstance(NS_FIND_CONTRACTID);
  if (!mFind)
    return;  // mFind is always null when we are not correctly initialized
  
  AddRef();

  // ----------- Set search options ---------------
  mFind->SetCaseSensitive(PR_FALSE);
  mFind->SetWordBreaker(nsnull);

  // ----------- Get initial preferences ----------
  TypeAheadFindPrefsReset("", NS_STATIC_CAST(void*, this));

  // ----------- Listen to prefs ------------------
  prefs->RegisterCallback("accessibility.typeaheadfind", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefsReset, NS_STATIC_CAST(void*, this));
  prefs->RegisterCallback("accessibility.browsewithcaret", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefsReset, NS_STATIC_CAST(void*, this));

  // ----------- Get accel key --------------------
  prefs->GetIntPref("ui.key.accelKey", &gAccelKey);
}


nsTypeAheadFind::~nsTypeAheadFind()
{
  RemoveCurrentSelectionListener();
  RemoveCurrentScrollPositionListener();
  RemoveCurrentKeypressListener();
  RemoveCurrentWindowFocusListener();

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs) {
    prefs->UnregisterCallback("accessibility.typeaheadfind", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefsReset, (void*)this);
    prefs->UnregisterCallback("accessibility.browsewithcaret", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefsReset, (void*)this);
  }
}

nsTypeAheadFind *nsTypeAheadFind::GetInstance()
{
  if (!mInstance)
    mInstance = new nsTypeAheadFind();
  // Will be released in the module destructor
  NS_IF_ADDREF(mInstance);
  return mInstance;
}


void nsTypeAheadFind::ReleaseInstance()
{
  NS_IF_RELEASE(nsTypeAheadFind::mInstance);
}


// ------- Pref Callbacks (2) ---------------

int PR_CALLBACK nsTypeAheadFind::TypeAheadFindPrefsReset(const char* aPrefName, void* instance_data)
{
  nsTypeAheadFind* typeAheadFind= NS_STATIC_CAST(nsTypeAheadFind*, instance_data);
  NS_ASSERTION(typeAheadFind, "bad instance data");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs)
    return 0;

  PRBool wasTypeAheadOn = typeAheadFind->mIsTypeAheadOn;
  prefs->GetBoolPref("accessibility.typeaheadfind", &typeAheadFind->mIsTypeAheadOn);
  if (typeAheadFind->mIsTypeAheadOn != wasTypeAheadOn) {
    nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
    if (progress) {
      if (!typeAheadFind->mIsTypeAheadOn) {
        typeAheadFind->CancelFind();
        typeAheadFind->RemoveCurrentSelectionListener();
        typeAheadFind->RemoveCurrentScrollPositionListener();
        typeAheadFind->RemoveCurrentKeypressListener();
        typeAheadFind->RemoveCurrentWindowFocusListener();
        progress->RemoveProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, typeAheadFind));
      }
      else {
        progress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, typeAheadFind), nsIWebProgress::NOTIFY_STATE_DOCUMENT);  
        // Initialize string bundle
        nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(kStringBundleServiceCID));
        if (stringBundleService)
          stringBundleService->CreateBundle(TYPEAHEADFIND_BUNDLE_URL, getter_AddRefs(typeAheadFind->mStringBundle));  
      }
    }
  }
  prefs->GetBoolPref("accessibility.typeaheadfind.linksonly", &typeAheadFind->mLinksOnlyPref);
  prefs->GetIntPref("accessibility.typeaheadfind.timeout", &typeAheadFind->mTimeoutLength);
  prefs->GetBoolPref("accessibility.browsewithcaret", &typeAheadFind->mCaretBrowsingOn);

  return 0;  // PREF_OK
}


// ------- nsITimer Methods (1) ---------------

void nsTypeAheadFind::Notify(nsITimer *timer)
{
  CancelFind();
}

// ------- nsIWebProgressListener Methods (5) ---------------

NS_IMETHODIMP nsTypeAheadFind::OnStateChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
/* PRUint32 aStateFlags ...
 *
 * ===== What has happened =====	
 * STATE_START=1, STATE_REDIRECTING=2, STATE_TRANSFERRING=4,
 * STATE_NEGOTIATING=8, STATE_STOP=0x10
 *
 * ===== Where did it occur? =====
 * STATE_IS_REQUEST=0x1000, STATE_IS_DOCUMENT=0x2000, STATE_IS_NETWORK=0x4000, STATE_IS_WINDOW=0x8000
 */

  if ((aStateFlags & (STATE_IS_DOCUMENT|STATE_TRANSFERRING)) != (STATE_IS_DOCUMENT|STATE_TRANSFERRING) || !mFind)
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(domWindow));
  if (!domWindow || mManualFindWindows->IndexOf(windowSupports) >= 0)
    return NS_OK;
  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc)), parentDoc;
  if (!doc)
    return NS_OK;
  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell)
    return NS_OK;

  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return NS_OK;

  // Don't listen for events on chrome windows
  nsCOMPtr<nsISupports> pcContainer;
  presContext->GetContainer(getter_AddRefs(pcContainer));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
  PRInt32 itemType = nsIDocShellTreeItem::typeChrome;
  if (treeItem) 
    treeItem->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeChrome) 
    return NS_OK;

  // Check for editor or message pane
  nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(treeItem));
  if (editorDocShell) {
    PRBool isEditable;
    editorDocShell->GetEditable(&isEditable);
    if (isEditable)
      return NS_OK;
  }
  // XXX the manual id check for the mailnews message pane looks like a hack, but is necessary.
  // We conflict with mailnews single key shortcuts like "n" for next unread message.
  // We need this manual check in Mozilla 1.0 and Netscape 7.0 
  // because people will be using XPI's to install us there, so we have to do our check from here
  // rather than create a new interface or attribute on the browser element.
  // XXX We will remove this and use autotypeaheadfind="false" in future trunk builds
  doc->GetParentDocument(getter_AddRefs(parentDoc));
  if (parentDoc) {
    nsCOMPtr<nsIContent> browserElementContent;
    parentDoc->FindContentForSubDocument(doc, getter_AddRefs(browserElementContent)); // content for <browser>
    nsCOMPtr<nsIDOMElement> browserElement(do_QueryInterface(browserElementContent));
    if (browserElement) {
      nsAutoString id, tagName, autoFind;
      browserElement->GetLocalName(tagName);
      browserElement->GetAttribute(NS_LITERAL_STRING("id"), id);
      browserElement->GetAttribute(NS_LITERAL_STRING("autotypeaheadfind"), autoFind);
      if (tagName.EqualsWithConversion("editor") || id.EqualsWithConversion("messagepane") ||
          autoFind.EqualsWithConversion("false")) {
        SetAutoStart(domWindow, PR_FALSE);
        return NS_OK;
      }
    }
  }

  nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(domWindow));
  if (eventTarget)
    AttachNewWindowFocusListener(eventTarget);

  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::OnProgressChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
  PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  // We can use this to report the percentage done
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::OnLocationChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsIURI *location)
{
  // Load has been verified, it will occur, about to commence
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::OnStatusChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  // Status bar has changed
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::OnSecurityChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 state)
{
  // Security setting has changed
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


  
// ------- nsIDOMFocusListener Methods (2) ---------------

NS_IMETHODIMP nsTypeAheadFind::Focus(nsIDOMEvent* aEvent) 
{
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  aEvent->GetTarget(getter_AddRefs(domEventTarget));
  return HandleFocusInternal(domEventTarget);
}

nsresult nsTypeAheadFind::HandleFocusInternal(nsIDOMEventTarget *aDOMEventTarget)
{
  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(aDOMEventTarget));
  if (!domNode)
    return NS_OK;  
  if (!gIsFindingText) // prevents listener callbacks from resetting us during typeahead find processing
    CancelFind();

  nsCOMPtr<nsIDOMDocument> domDoc;
  domNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    domDoc = do_QueryInterface(domNode);
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_OK;
  nsCOMPtr<nsIDOMEventTarget> docTarget(do_QueryInterface(domDoc));

  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
  doc->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
  nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(ourGlobal));
  nsCOMPtr<nsIDOMEventTarget> rootTarget(do_QueryInterface(domWin));

  if (!rootTarget || (domWin == mFocusedWindow && docTarget != aDOMEventTarget))
    return NS_OK;  // Return early for elements focused within currently focused  document

  // Focus event in a new doc -- trigger keypress event listening
  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell)
    return NS_OK;

  RemoveCurrentSelectionListener();
  RemoveCurrentScrollPositionListener();
  RemoveCurrentKeypressListener();

  mFocusedWindow = domWin;
  mFocusedWeakShell = do_GetWeakReference(presShell);

  // Add scroll position and selection listeners, so we can cancel current find when user navigates
  GetSelection(presShell, nsnull, getter_AddRefs(mFocusedDocSelCon),
               getter_AddRefs(mFocusedDocSelection)); // cache for reuse
  AttachNewScrollPositionListener(presShell);
  AttachNewSelectionListener();

  // If focusing on new window, add keypress listener to new window and remove old keypress listener
  AttachNewKeypressListener(rootTarget);

  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::Blur(nsIDOMEvent* aEvent) 
{ 
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  aEvent->GetTarget(getter_AddRefs(domEventTarget));
  nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(domEventTarget));
  if (domWindow) {
    RemoveCurrentSelectionListener();
    RemoveCurrentScrollPositionListener();
    RemoveCurrentKeypressListener();
    CancelFind();
    mFocusedWindow = nsnull;
  }

  return NS_OK; 
}


// ------- nsIDOMEventListener Methods (1) ---------------

NS_IMETHODIMP nsTypeAheadFind::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK; 
}


// ------- nsIDOMLoadListener Methods (4) ---------------

NS_IMETHODIMP nsTypeAheadFind::Load(nsIDOMEvent* aEvent) { return NS_OK; } 

NS_IMETHODIMP nsTypeAheadFind::Unload(nsIDOMEvent* aEvent) 
{
  // Remove focus listener, if there was one, and unload listener.
  // These are the two listeners we need to remove here.
  // If the current window was focused, then the blur event
  // is used to remove the keypress, selection and scroll position listeners,
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  aEvent->GetTarget(getter_AddRefs(domEventTarget));

  if (domEventTarget) {
    domEventTarget->RemoveEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_FALSE);
    domEventTarget->RemoveEventListener(NS_LITERAL_STRING("unload"), NS_STATIC_CAST(nsIDOMLoadListener*, this), PR_FALSE);
    nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(domEventTarget));
    if (domWin) {  // Remove from list of manual find windows, because window pointer is no longer value
      SetAutoStart(domWin, PR_TRUE); 
      if (mFocusedWindow == domWin)
        mFocusedWindow = nsnull;
    }
  }

  return NS_OK; 
}

NS_IMETHODIMP nsTypeAheadFind::Abort(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsTypeAheadFind::Error(nsIDOMEvent* aEvent) { return NS_OK; }


// ------- nsIDOMKeyListener Methods (3) ---------------

NS_IMETHODIMP nsTypeAheadFind::KeyDown(nsIDOMEvent* aEvent) { return NS_OK; } 

NS_IMETHODIMP nsTypeAheadFind::KeyUp(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsTypeAheadFind::KeyPress(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNSEvent> nsEvent(do_QueryInterface(aEvent));
  if (!nsEvent)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  nsEvent->GetOriginalTarget(getter_AddRefs(domEventTarget));

  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(domEventTarget));
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));

  // ---------- First analyze keystroke, exit early if possible --------------

  PRUint32 keyCode, charCode;
  PRBool isShift, isCtrl, isAlt, isMeta;
  if (!keyEvent || !targetNode ||
      NS_FAILED(keyEvent->GetKeyCode(&keyCode)) ||
      NS_FAILED(keyEvent->GetCharCode(&charCode)) ||
      NS_FAILED(keyEvent->GetShiftKey(&isShift)) ||
      NS_FAILED(keyEvent->GetCtrlKey(&isCtrl)) ||
      NS_FAILED(keyEvent->GetAltKey(&isAlt)) ||
      NS_FAILED(keyEvent->GetMetaKey(&isMeta)))
    return NS_ERROR_FAILURE;

  if ((charCode == 'g' || charCode=='G') && !mTypeAheadBuffer.IsEmpty() && isAlt + isMeta + isCtrl == 1 && (
     (nsTypeAheadFind::gAccelKey == nsIDOMKeyEvent::DOM_VK_CONTROL && isCtrl) || 
     (nsTypeAheadFind::gAccelKey == nsIDOMKeyEvent::DOM_VK_ALT     && isAlt ) || 
     (nsTypeAheadFind::gAccelKey == nsIDOMKeyEvent::DOM_VK_META    && isMeta))) {
    // We steal Accel+G (find next) and Accel+Shift+G (find prev), avoid early return
    if (mRepeatingMode == eRepeatingChar)
      mTypeAheadBuffer = mTypeAheadBuffer.First();
    mRepeatingMode = (charCode=='G')? eRepeatingReverse: eRepeatingForward;
    mLiteralTextSearchOnly = PR_TRUE;
  }
  else if ((isAlt && !isShift) || isCtrl || isMeta)
    return NS_OK;  // Ignore most modified keys, but alt+shift may be used for entering foreign chars
  else if (mRepeatingMode == eRepeatingForward || mRepeatingMode == eRepeatingReverse)
    CancelFind();

  // ---------- Get document/presshell/prescontext/selection/etc. ------------

  nsCOMPtr<nsIDOMDocument> domDoc;
  targetNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    return NS_OK;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_OK;
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
  if (!presShell)
    return NS_OK;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return NS_OK;

  nsCOMPtr<nsISelection> localSelection;
  nsCOMPtr<nsISelectionController> localSelCon;
  GetSelection(presShell, targetNode, getter_AddRefs(localSelCon), getter_AddRefs(localSelection));  

  if (localSelection != mFocusedDocSelection || !mFocusedDocSelection)
    return NS_OK;   // Different selection for this node/frame. Must be in editable control.

  PRBool isFirstVisiblePreferred = PR_FALSE;
  PRBool isBackspace = PR_FALSE;  // When backspace is pressed
  PRBool isLinksOnly = mLinksOnly;

  // ------------- Escape pressed ---------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE) {
    // Escape accomplishes 2 things: 
    // 1. it is a way for the user to deselect with the keyboard
    mFocusedDocSelection->CollapseToStart();
    // 2. it is a way for the user to cancel incremental find with visual feedback (the selection disappears)
    if (mTypeAheadBuffer.IsEmpty())
      return NS_OK;
    aEvent->PreventDefault(); // If Escape is normally used for a command, don't do it
    CancelFind();
    return NS_OK;
  }
  // ----------- Back space ----------------------
  else if (keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
    if (mTypeAheadBuffer.IsEmpty())
      return NS_OK;
    if (mTypeAheadBuffer.Length() == 1) {
      if (mStartFindRange) {
        mFocusedDocSelection->RemoveAllRanges();
        mFocusedDocSelection->AddRange(mStartFindRange);
      }
      mFocusedDocSelection->CollapseToStart();
      CancelFind();
      return NS_OK;
    }
    mTypeAheadBuffer = Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length() - 1);
    isBackspace = PR_TRUE;
    mDontTryExactMatch = PR_FALSE;
  }
  // ----------- Printable characters --------------
  else if (mRepeatingMode != eRepeatingForward && mRepeatingMode != eRepeatingReverse) {
    PRUnichar uniChar = ToLowerCase(NS_STATIC_CAST(PRUnichar, charCode));
    PRInt32 bufferLength = mTypeAheadBuffer.Length();
    if (uniChar < ' ')
      return NS_OK; // not a printable character
    if (uniChar == ' ') {
      if (bufferLength == 0)
        return NS_OK; // We ignore space only if it's the first character
      else 
        aEvent->PreventDefault(); // Use the space if it's not the first character, but don't page down
    }
    if (bufferLength > 0) {
      if (mTypeAheadBuffer.First() != uniChar)
        mRepeatingMode = eRepeatingNone;
      else if (bufferLength == 1)
        mRepeatingMode = eRepeatingChar;
    }
    if (bufferLength == 0)
      if (uniChar == '`' || uniChar=='\'' || uniChar=='\"') {
        mLinksOnly = PR_TRUE; // If you type quote, it starts a links only search
        return NS_OK;
      }
      else if (uniChar == '/') {
        mLinksOnly = PR_FALSE; // If you type / it starts a search for all text 
        mLiteralTextSearchOnly = PR_TRUE;  // Repeated characters will not search for links
        return NS_OK;
      }
    mTypeAheadBuffer += uniChar;
    if (bufferLength == 0) {
      // --------- Initialize find -----------
      // If you can see the selection (either not collapsed or through caret browsing), start there
      // Otherwise we're going to start at the first visible element
      PRBool isSelectionCollapsed;
      mFocusedDocSelection->GetIsCollapsed(&isSelectionCollapsed);
      isFirstVisiblePreferred = !mCaretBrowsingOn && isSelectionCollapsed;
    }
  }

  gIsFindingText = PR_TRUE; // prevents listener callbacks from resetting us during typeahead find processing

  nsresult rv = NS_ERROR_FAILURE;

  // ----------- Set search options ---------------
  mFind->SetFindBackwards(mRepeatingMode == eRepeatingReverse);

  if (!mDontTryExactMatch)    // Regular find, not repeated char find
    rv = FindItNow(PR_FALSE, mLinksOnly, isFirstVisiblePreferred, isBackspace); // Prefer to find exact match
#ifndef NO_LINK_CYCLE_ON_SAME_CHAR
  if (NS_FAILED(rv) && !mLiteralTextSearchOnly && mRepeatingMode == eRepeatingChar) {
    mDontTryExactMatch = PR_TRUE;  // Repeated character find mode
    rv = FindItNow(PR_TRUE, PR_TRUE, isFirstVisiblePreferred, isBackspace);
  }
#endif

  aEvent->PreventDefault(); // Prevent normal processing of this keystroke
  aEvent->StopPropagation();

  // ------- If accessibility.typeaheadfind.timeout is set, cancel find after specified # milliseconds
  if (mTimeoutLength) {
    if (!mTimer)
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mTimer)
      mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), mTimeoutLength);
  }

  gIsFindingText = PR_FALSE;

  if (NS_SUCCEEDED(rv)) {
    // ------- Success!!! ---------------------------------------------------------------------------------
    mKeepSelectionOnCancel = !isLinksOnly;   // Next time CancelFind() is called, selection will be collapsed

    // ------- Store current find string for regular find usage: find-next or find dialog text field ------

    if (mTypeAheadBuffer.Length() == 1) {  // If first letter, store where the first find succeeded (mStartFindRange)
      mStartFindRange = nsnull;
      nsCOMPtr<nsIDOMRange> startFindRange;
      mFocusedDocSelection->GetRangeAt(0, getter_AddRefs(startFindRange));
      if (startFindRange)
        startFindRange->CloneRange(getter_AddRefs(mStartFindRange));
    }
  }
  else {
    // ----- Nothing found -----
    nsCOMPtr<nsISound> soundInterface(do_CreateInstance("@mozilla.org/sound;1"));
    DisplayStatus(PR_FALSE, nsnull, PR_FALSE); // Display failure status
    if (soundInterface)
      soundInterface->Beep();
    // Remove bad character from buffer, so we can continue typing from last matched character
#ifdef DONT_ADD_CHAR_IF_NOT_FOUND
    if (mTypeAheadBuffer.Length == 1)  // If first character is bad, flush it away anyway
#endif
      mTypeAheadBuffer = Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length() - 1);
  }

  return NS_OK;
}


nsresult nsTypeAheadFind::FindItNow(PRBool aIsRepeatingSameChar, PRBool aIsLinksOnly, PRBool aIsFirstVisiblePreferred, PRBool aIsBackspace)
{
  nsCOMPtr<nsIPresShell> presShell, startingPresShell(do_QueryReferent(mFocusedWeakShell));
  if (aIsBackspace && mStartFindRange) { // when backspace is pressed, start from where first char was found
    nsCOMPtr<nsIDOMNode> startNode;
    mStartFindRange->GetStartContainer(getter_AddRefs(startNode));
    if (startNode) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      startNode->GetOwnerDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
      if (doc) 
        doc->GetShellAt(0, getter_AddRefs(presShell));
    }
  }
  if (!presShell)   
    presShell = startingPresShell;  // this is the current document
  if (!presShell)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> currentContainer, startingContainer;
  presContext->GetContainer(getter_AddRefs(startingContainer));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(startingContainer)), rootContentTreeItem;
  nsCOMPtr<nsIDocShell> currentDocShell, startingDocShell(do_QueryInterface(startingContainer));

  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(rootContentTreeItem));
  nsCOMPtr<nsIDocShell> rootContentDocShell(do_QueryInterface(rootContentTreeItem));
  if (!rootContentDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent, nsIDocShell::ENUMERATE_FORWARDS,
                                             getter_AddRefs(docShellEnumerator));

  // Default: can start at the current document
  currentContainer = startingContainer = do_QueryInterface(rootContentDocShell);

  // Iterate up to current shell, if there's more than 1 that we're dealing with
  PRBool hasMoreDocShells;
  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells)) && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    currentDocShell = do_QueryInterface(currentContainer);
    if (!currentDocShell || currentDocShell == startingDocShell || aIsFirstVisiblePreferred)
      break;
  }

  // ------------ Get ranges ready ----------------
  nsCOMPtr<nsIDOMRange> searchRange, startPointRange, endPointRange, returnRange;
  if (NS_FAILED(GetSearchContainers(currentContainer, aIsRepeatingSameChar, aIsFirstVisiblePreferred, PR_TRUE,
                                    getter_AddRefs(presShell), getter_AddRefs(presContext), 
                                    getter_AddRefs(searchRange), getter_AddRefs(startPointRange), 
                                    getter_AddRefs(endPointRange))))
    return NS_ERROR_FAILURE;

  if (aIsBackspace && mStartFindRange && startingDocShell == currentDocShell) {
    // when backspace is pressed, start where first char was found
    mStartFindRange->CloneRange(getter_AddRefs(startPointRange)); 
  }

  PRInt32 rangeCompareResult = 0;
  startPointRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START , searchRange, &rangeCompareResult);
  PRBool hasWrapped = (rangeCompareResult <= 0); // No need to wrap find in doc if starting at beginning

  nsAutoString findBuffer;
  if (aIsRepeatingSameChar)
    findBuffer = mTypeAheadBuffer.First();
  else 
    findBuffer = PromiseFlatString(mTypeAheadBuffer);

  while (PR_TRUE) {    // ----- Outer while loop: go through all docs -----
    while (PR_TRUE) {         // Inner while loop: go through a single document -------
      mFind->Find(findBuffer.get(), searchRange, 
                  startPointRange, endPointRange, getter_AddRefs(returnRange));
      if (!returnRange)
        break;
      // ------- Test resulting found range for success conditions ------
      PRBool isInsideLink, isStartingLink;
      RangeStartsInsideLink(returnRange, presShell, &isInsideLink, &isStartingLink);

      if (!IsRangeVisible(presShell, presContext, returnRange, aIsFirstVisiblePreferred, getter_AddRefs(startPointRange)) ||
          (aIsRepeatingSameChar && !isStartingLink) || (aIsLinksOnly && !isInsideLink)) {
        // ------ Failure ------
        // Start find again from here 
        returnRange->CloneRange(getter_AddRefs(startPointRange));
        startPointRange->Collapse(mRepeatingMode == eRepeatingReverse);  // collapse to end
        continue;
      }

      // ------ Success! -------
      // Make sure new document is selected
      if (presShell != startingPresShell) {
        // We are in a new document
        mFocusedDocSelection->CollapseToStart(); // hide old doc's selection
        nsCOMPtr<nsIDocument> doc;
        presShell->GetDocument(getter_AddRefs(doc));
        if (!doc)
          return NS_ERROR_FAILURE;
        mFocusedWeakShell = do_GetWeakReference(presShell);
        nsCOMPtr<nsIContent> docContent;
        doc->GetRootContent(getter_AddRefs(docContent));
        if (docContent)
          docContent->SetFocus(presContext);
      }

      // Select the found text and focus it
      mFocusedDocSelection->RemoveAllRanges();
      mFocusedDocSelection->AddRange(returnRange);
      mFocusedDocSelCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, 
                                                 nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
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
    // ================= end-inner-while: go through a single document ==================

    // ---------- Nothing found yet -------------
    do {
      if (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells)) && hasMoreDocShells) {
        docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
        if (NS_FAILED(GetSearchContainers(currentContainer, aIsRepeatingSameChar, 
                                          aIsFirstVisiblePreferred, PR_FALSE,
                                          getter_AddRefs(presShell), getter_AddRefs(presContext), 
                                          getter_AddRefs(searchRange), getter_AddRefs(startPointRange), 
                                          getter_AddRefs(endPointRange))))
          return NS_ERROR_FAILURE;
        currentDocShell = do_QueryInterface(currentContainer);
        if (currentDocShell)
          break;
      }
      // Reached last doc shell, loop around back to first doc shell
      rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent, nsIDocShell::ENUMERATE_FORWARDS,
                                                getter_AddRefs(docShellEnumerator));
    }
    while (docShellEnumerator);

    if (currentDocShell != startingDocShell)
      continue;  // Try next document

    // Finished searching through docshells:
    // If aFirstVisiblePreferred == PR_TRUE, we may need to go through all docshells twice
    // Once to look for visible matches, the second time for any match
    if (!hasWrapped || aIsFirstVisiblePreferred) {
      aIsFirstVisiblePreferred = PR_FALSE;
      hasWrapped = PR_TRUE;
      continue;  // Go through all docs again
    }

    if (aIsRepeatingSameChar && 
        mTypeAheadBuffer.Length() > 1 && mTypeAheadBuffer.First() == mTypeAheadBuffer.CharAt(1) &&
        mTypeAheadBuffer.Last() != mTypeAheadBuffer.First()) {
      // If they repeat the same character and then change, such as aaaab
      // start over with new char as a repeated char find
      mTypeAheadBuffer = mTypeAheadBuffer.Last();
      return FindItNow(PR_TRUE, PR_TRUE, aIsFirstVisiblePreferred, aIsBackspace);
    }
    
    // ------------- Failed --------------
    break;
  }   // end-outer-while: go through all docs

  return NS_ERROR_FAILURE;
}


nsresult nsTypeAheadFind::GetSearchContainers(nsISupports *aContainer, PRBool aIsRepeatingSameChar, 
                                              PRBool aIsFirstVisiblePreferred, PRBool aCanUseDocSelection,
                                              nsIPresShell **aPresShell, nsIPresContext **aPresContext,
                                              nsIDOMRange **aSearchRange, nsIDOMRange **aStartPointRange, 
                                              nsIDOMRange **aEndPointRange)
{
  *aSearchRange = nsnull;
  *aStartPointRange = nsnull;
  *aEndPointRange = nsnull;
  *aPresShell = nsnull;
  *aPresContext = nsnull;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
  if (!docShell)
    return NS_ERROR_FAILURE;

  docShell->GetPresShell(aPresShell);
  docShell->GetPresContext(aPresContext);
  if (!*aPresShell || !*aPresContext)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  (*aPresShell)->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIContent> rootContent;
  doc->GetRootContent(getter_AddRefs(rootContent));
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootContent));
  if (!rootNode)
    return NS_ERROR_FAILURE;
  PRInt32 childCount;
  if (NS_FAILED(rootContent->ChildCount(childCount)))
    return NS_ERROR_FAILURE;
 
  nsCOMPtr<nsIDOMRange> searchRange(do_CreateInstance(kRangeCID));
  nsCOMPtr<nsIDOMRange> startPointRange(do_CreateInstance(kRangeCID));
  nsCOMPtr<nsIDOMRange> endPointRange(do_CreateInstance(kRangeCID));

  searchRange->SelectNodeContents(rootNode);
  endPointRange->SetStart(rootNode, childCount);
  endPointRange->SetEnd(rootNode, childCount);

  // Consider current selection as null if it's not in the currently focused document
  nsCOMPtr<nsIDOMRange> currentSelectionRange;
  nsCOMPtr<nsIPresShell> selectionPresShell = do_QueryReferent(mFocusedWeakShell);
  if (selectionPresShell == *aPresShell && aCanUseDocSelection) 
    mFocusedDocSelection->GetRangeAt(0, getter_AddRefs(currentSelectionRange));

  if (!currentSelectionRange || aIsFirstVisiblePreferred) {
    // Ensure visible range, move forward if necessary
    IsRangeVisible(*aPresShell, *aPresContext, searchRange, aIsFirstVisiblePreferred, getter_AddRefs(startPointRange));
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
    if (!startNode) 
      startNode = rootNode;
    startPointRange->SetStart(startNode, startOffset);
    startPointRange->Collapse(PR_TRUE); // collapse to start
  }

  NS_ADDREF(*aSearchRange = searchRange);
  NS_ADDREF(*aStartPointRange = startPointRange);
  NS_ADDREF(*aEndPointRange = endPointRange);
  return NS_OK;
}


void nsTypeAheadFind::RangeStartsInsideLink(nsIDOMRange *aRange, nsIPresShell *aPresShell,
                                            PRBool *aIsInsideLink, PRBool *aIsStartingLink)
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
  origContent = startContent;

  if (NS_SUCCEEDED(startContent->CanContainChildren(canContainChildren)) &&
      canContainChildren) {
    startContent->ChildAt(startOffset, *getter_AddRefs(childContent));
    if (childContent)
      startContent = childContent;
  }
  else if (startOffset > 0) {
    nsCOMPtr<nsITextContent> textContent(do_QueryInterface(startContent));
    if (textContent) {
      // look for non whitespace character before start offset 
      const nsTextFragment *textFrag;
      textContent->GetText(&textFrag);
      for (PRInt32 index = 0; index < startOffset; index++)
        if (!XP_IS_SPACE(textFrag->CharAt(index))) {
          *aIsStartingLink = PR_FALSE;  // not at start of a node
          break;
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
      nsCOMPtr<nsITextContent> textContent(do_QueryInterface(parentsFirstChild));
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


NS_IMETHODIMP nsTypeAheadFind::ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY)
{
  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::ScrollPositionDidChange(nsIScrollableView *aScrollableView, nscoord aX, nscoord aY)
{
  if (!gIsFindingText)
    CancelFind();
  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason)
{
  if (gIsFindingText)
    return NS_OK;
  PRBool isSelectionCollapsed;
  aSel->GetIsCollapsed(&isSelectionCollapsed);

  if (!isSelectionCollapsed)
    mKeepSelectionOnCancel = PR_TRUE;
  else if (mCaretBrowsingOn && aSel) {
    // Cancel find if they moved the caret in browse with caret mode
    CancelFind();
  }
  return NS_OK;
}


// ---------------- nsITypeAheadFind --------------------

NS_IMETHODIMP nsTypeAheadFind::GetIsActive(PRBool *aIsActive)
{
  *aIsActive = !mTypeAheadBuffer.IsEmpty();
  return NS_OK;
}


/*
 * Start new type ahead find manually
 */

NS_IMETHODIMP nsTypeAheadFind::StartNewFind(nsIDOMWindow *aWindow, PRBool aLinksOnly)
{
  if (!mFind)
    return NS_ERROR_FAILURE;  // Type Ahead Find not correctly initialized

  nsCOMPtr<nsIDOMWindowInternal> windowInternal(do_QueryInterface(aWindow));
  if (!windowInternal)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(aWindow));
  if (!eventTarget)
    return NS_ERROR_FAILURE;

  RemoveCurrentWindowFocusListener();
  AttachNewWindowFocusListener(eventTarget);

  windowInternal->Focus();  
  if (mFocusedWindow != aWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aWindow->GetDocument(getter_AddRefs(domDoc));
    eventTarget = do_QueryInterface(domDoc);
    if (eventTarget)
      HandleFocusInternal(eventTarget);  // This routine will set up the keypress listener
  }
  mLinksOnly = aLinksOnly;

  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::SetAutoStart(nsIDOMWindow *aWindow, PRBool aAutoStartOn)
{
  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(aWindow));
  PRInt32 index = mManualFindWindows->IndexOf(windowSupports);

  if (aAutoStartOn) {
    if (index >= 0)
      mManualFindWindows->RemoveElementAt(index);  // Remove from list of windows requiring manual find
  }
  else if (index < 0)  // Should be in list of windows requiring manual find
    mManualFindWindows->InsertElementAt(windowSupports, 0);
  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::GetAutoStart(nsIDOMWindow *aWindow, PRBool *aIsAutoStartOn)
{
  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(aWindow));
  *aIsAutoStartOn = mManualFindWindows->IndexOf(windowSupports) < 0; // If not stored in manual find windows list

  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::CancelFind()
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
    if (!mKeepSelectionOnCancel && presShell)
      mFocusedDocSelection->CollapseToStart();
  }
  mLinksOnly = mLinksOnlyPref;

  // These will be initialized to their true values after the first character is typed
  mCaretBrowsingOn = PR_FALSE;
  mRepeatingMode = eRepeatingNone;
  mLiteralTextSearchOnly = PR_FALSE;
  mDontTryExactMatch = PR_FALSE;
  mStartFindRange = nsnull;

  nsCOMPtr<nsISupports> windowSupports(do_QueryInterface(mFocusedWindow));
  if (mManualFindWindows->IndexOf(windowSupports) >= 0) {
    SetAutoStart(mFocusedWindow, PR_FALSE);
    RemoveCurrentKeypressListener();
    RemoveCurrentWindowFocusListener();
    RemoveCurrentSelectionListener();
    RemoveCurrentScrollPositionListener();
  }

  return NS_OK;
}


// ------- Helper Methods ---------------

void nsTypeAheadFind::RemoveCurrentScrollPositionListener()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
  nsCOMPtr<nsIViewManager> vm;

  if (presShell)
    presShell->GetViewManager(getter_AddRefs(vm));

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (scrollableView)
    scrollableView->RemoveScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, this));
  mFocusedWeakShell = nsnull;
}


void nsTypeAheadFind::AttachNewScrollPositionListener(nsIPresShell *aPresShell)
{
  nsCOMPtr<nsIViewManager> vm;
  if (aPresShell)
    aPresShell->GetViewManager(getter_AddRefs(vm));
  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);
  if (scrollableView)
    scrollableView->AddScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, this));
}


void nsTypeAheadFind::RemoveCurrentSelectionListener()
{
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mFocusedDocSelection));
  if (selPrivate)
    selPrivate->RemoveSelectionListener(this); // remove us if we are a listener
  mFocusedDocSelection = nsnull;
}


void nsTypeAheadFind::AttachNewSelectionListener()
{
  // Only add selection listener if caret browsing is on
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mFocusedDocSelection));
  if (selPrivate)
    selPrivate->AddSelectionListener(this);
}


void nsTypeAheadFind::RemoveCurrentKeypressListener()
{
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mFocusedWindow));
  if (target)
    target->RemoveEventListener(NS_LITERAL_STRING("keypress"), NS_STATIC_CAST(nsIDOMKeyListener*, this), PR_TRUE);
}


void nsTypeAheadFind::AttachNewKeypressListener(nsIDOMEventTarget *aTarget)
{
  aTarget->AddEventListener(NS_LITERAL_STRING("keypress"),  NS_STATIC_CAST(nsIDOMKeyListener*, this), PR_TRUE);
}


void nsTypeAheadFind::RemoveCurrentWindowFocusListener()
{
  // Remove focus listener, and unload listener as well, since it's sole purpose was to remove focus listener
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mFocusedWindow));
  if (target) {
    target->RemoveEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_FALSE);
    target->RemoveEventListener(NS_LITERAL_STRING("unload"), NS_STATIC_CAST(nsIDOMLoadListener*, this), PR_FALSE);
  }
  mFocusedWindow = nsnull;
}


void nsTypeAheadFind::AttachNewWindowFocusListener(nsIDOMEventTarget *aTarget)
{
  // Add focus listener, and unload listener, which will remove focus listener when the window's life ends
  aTarget->AddEventListener(NS_LITERAL_STRING("focus"),  NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_FALSE);
  aTarget->AddEventListener(NS_LITERAL_STRING("unload"),  NS_STATIC_CAST(nsIDOMLoadListener*, this), PR_FALSE);
}


void nsTypeAheadFind::GetSelection(nsIPresShell *aPresShell, nsIDOMNode *aCurrentNode, 
                                   nsISelectionController **aSelCon, nsISelection **aDomSel)
{
  // if aCurrentNode is nsnull, get selection for document
  *aDomSel = nsnull;

  nsCOMPtr<nsIPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));

  nsIFrame *frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (!content)
    aPresShell->GetRootFrame(&frame);
  else 
    aPresShell->GetPrimaryFrameFor(content, &frame);

  if (presContext && frame) {
    frame->GetSelectionController(presContext, aSelCon);
    if (*aSelCon)
      (*aSelCon)->GetSelection(nsISelectionController::SELECTION_NORMAL, aDomSel);
  }
}


PRBool nsTypeAheadFind::IsRangeVisible(nsIPresShell *aPresShell, nsIPresContext *aPresContext,
                                      nsIDOMRange *aRange, PRBool aMustBeInViewPort, nsIDOMRange **aFirstVisibleRange) 
{
  // We need to know if at least a kMinPixels around the object is visible
  // Otherwise it will be marked STATE_OFFSCREEN and STATE_INVISIBLE

  aRange->CloneRange(aFirstVisibleRange);;
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content) 
    return PR_FALSE;

  nsIFrame *frame = nsnull;
  aPresShell->GetPrimaryFrameFor(content, &frame);
  if (!frame)   // No frame! Not visible then.
    return PR_FALSE; 

  // ---- We have a frame ----
  if (!aMustBeInViewPort)
    return PR_TRUE;  //  Don't need it to be on screen, just in rendering tree

  // Set up the variables we need, return true if we can't get at them all
  const PRUint16 kMinPixels  = 12;

  nsCOMPtr<nsIViewManager> viewManager;
  aPresShell->GetViewManager(getter_AddRefs(viewManager));
  if (!viewManager)
    return PR_TRUE;

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate AccGetBounds, because that is more expensive 
  // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough indicator
  nsRect relFrameRect;
  nsIView *containingView = nsnull;
  nsPoint frameOffset;
  frame->GetRect(relFrameRect);
  frame->GetOffsetFromView(aPresContext, frameOffset, &containingView);
  if (!containingView)
    return PR_FALSE;  // no view -- not visible
  relFrameRect.x = frameOffset.x;
  relFrameRect.y = frameOffset.y;

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  PRBool isVisible = PR_FALSE;
  viewManager->IsRectVisible(containingView, relFrameRect, NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)), &isVisible);

  if (isVisible)
    return PR_TRUE;

  // We know that the target range isn't usable because it's not in the view port
  // Move range forward to first visible point, this speeds us up in long documents
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID));
  if (trav)
    trav->NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF, aPresContext, frame);
  if (!frameTraversal)
    return PR_FALSE;

  PRBool isFirstVisible = PR_FALSE;
  while (!isFirstVisible) {
    frameTraversal->Next();
    nsISupports* currentItem;
    frameTraversal->CurrentItem(&currentItem);
    frame = NS_STATIC_CAST(nsIFrame*, currentItem);
    if (!frame)
      return PR_FALSE;
    frame->GetRect(relFrameRect);  
    frame->GetOffsetFromView(aPresContext, frameOffset, &containingView);
    if (containingView) {
      relFrameRect.x = frameOffset.x;
      relFrameRect.y = frameOffset.y;
      viewManager->IsRectVisible(containingView, relFrameRect, NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)), &isFirstVisible);
    }
  }
  if (frame) {
    nsCOMPtr<nsIContent> firstVisibleContent;
    frame->GetContent(getter_AddRefs(firstVisibleContent));
    nsCOMPtr<nsIDOMNode> firstVisibleNode(do_QueryInterface(firstVisibleContent));
    if (firstVisibleNode) {
      (*aFirstVisibleRange)->SelectNode(firstVisibleNode);
      (*aFirstVisibleRange)->Collapse(PR_TRUE);  // Collapse to start
    }
  }

  return PR_FALSE;
}


nsresult nsTypeAheadFind::GetTranslatedString(const nsAString& aKey, nsAString& aStringOut)
{
  nsXPIDLString xsValue;

  if (!mStringBundle || 
    NS_FAILED(mStringBundle->GetStringFromName(PromiseFlatString(aKey).get(), getter_Copies(xsValue)))) 
    return NS_ERROR_FAILURE;

  aStringOut.Assign(xsValue);
  return NS_OK;
}


void nsTypeAheadFind::DisplayStatus(PRBool aSuccess, nsIContent *aFocusedContent, PRBool aClearStatus)
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mFocusedWeakShell));
  if (!presShell)
    return;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return;

  nsCOMPtr<nsISupports> pcContainer;
  presContext->GetContainer(getter_AddRefs(pcContainer));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
  if (!treeItem)
    return;
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner)
    return;
  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  if (!browserChrome)
    return;

  nsAutoString statusString;
  if (aClearStatus)
    GetTranslatedString(NS_LITERAL_STRING("stopfind"), statusString);
  else {
    if (NS_SUCCEEDED(GetTranslatedString(mLinksOnly? (aSuccess? NS_LITERAL_STRING("linkfound"): NS_LITERAL_STRING("linknotfound")): 
       (aSuccess? NS_LITERAL_STRING("textfound"): NS_LITERAL_STRING("textnotfound")), statusString))) {
      nsAutoString closeQuoteString, urlString;
      GetTranslatedString(NS_LITERAL_STRING("closequote"), closeQuoteString);
      statusString += mTypeAheadBuffer + closeQuoteString;
#ifdef ADD_TYPEAHEADFIND_URL_TO_STATUS
      nsCOMPtr<nsIDOMNode> focusedNode(do_QueryInterface(aFocusedContent));
      if (focusedNode)
        presShell->GetLinkLocation(focusedNode, urlString);
      if (! urlString.IsEmpty()) {   // Add URL in parenthesis
        nsAutoString openParenString, closeParenString;
        GetTranslatedString(NS_LITERAL_STRING("openparen"), openParenString);
        GetTranslatedString(NS_LITERAL_STRING("closeparen"), closeParenString);
        statusString += NS_LITERAL_STRING("   ")  + openParenString + urlString + closeParenString;
      }
#endif
    }
  }
  browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, PromiseFlatString(statusString).get());
}



