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
#include "nsISelectionController.h"
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

// Header for this class
#include "nsTypeAheadFind.h"

// When true, this prevents listener callbacks from resetting us during typeahead find processing
PRBool nsTypeAheadFind::gIsFindingText = PR_FALSE; 

////////////////////////////////////////////////////////////////////////


NS_INTERFACE_MAP_BEGIN(nsTypeAheadFind)
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


nsTypeAheadFind::nsTypeAheadFind(): 
  mLinksOnlyPref(PR_FALSE), mLinksOnly(PR_FALSE), mIsTypeAheadOn(PR_FALSE), 
  mCaretBrowsingOn(PR_FALSE), mIsRepeatingSameChar(PR_FALSE), mKeepSelectionOnCancel(PR_FALSE),
  mTimeoutLength(0),
  mFind(do_CreateInstance(NS_FIND_CONTRACTID)), 
  mFindService(do_GetService("@mozilla.org/find/find_service;1"))
{
  NS_INIT_REFCNT();

#ifdef DEBUG
  // There should only ever be one instance of us
  static gInstanceCount;
  ++gInstanceCount;
  NS_ASSERTION(gInstanceCount == 1, "There should be only 1 instance of nsTypeAheadFind, someone is creating more than 1.");
#endif

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs) {
    prefs->RegisterCallback("accessibility.typeaheadfind", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    prefs->RegisterCallback("accessibility.typeaheadfind.linksonly", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    prefs->RegisterCallback("accessibility.typeaheadfind.timeout", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    prefs->RegisterCallback("accessibility.browsewithcaret", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    AddRef();
  }

  nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(kStringBundleServiceCID));
  if (stringBundleService)
    stringBundleService->CreateBundle(TYPEAHEADFIND_BUNDLE_URL, getter_AddRefs(mStringBundle));  
}


nsTypeAheadFind::~nsTypeAheadFind()
{
  RemoveCurrentSelectionListener();
  RemoveCurrentScrollPositionListener();
  RemoveCurrentKeypressListener();
  RemoveCurrentWindowFocusListener();

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (prefs) {
    prefs->UnregisterCallback("accessibility.typeaheadfind", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    prefs->UnregisterCallback("accessibility.typeaheadfind.linksonly", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    prefs->UnregisterCallback("accessibility.typeaheadfind.timeout", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
    prefs->UnregisterCallback("accessibility.browsewithcaret", (PrefChangedFunc)nsTypeAheadFind::TypeAheadFindPrefCallback, (void*)this);
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

int PR_CALLBACK nsTypeAheadFind::TypeAheadFindPrefCallback(const char* aPrefName, void* instance_data)
{
  nsTypeAheadFind* typeAheadFind= NS_STATIC_CAST(nsTypeAheadFind*, instance_data);
  NS_ASSERTION(typeAheadFind, "bad instance data");

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));

  if (!nsCRT::strcasecmp(aPrefName,"accessibility.typeaheadfind")) {
    nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
    if (progress) {
      PRBool wasTypeAheadOn = typeAheadFind->mIsTypeAheadOn;
      prefs->GetBoolPref("accessibility.typeaheadfind", &typeAheadFind->mIsTypeAheadOn);
      if (!typeAheadFind->mIsTypeAheadOn) {
        typeAheadFind->RemoveCurrentSelectionListener();
        typeAheadFind->RemoveCurrentScrollPositionListener();
        typeAheadFind->RemoveCurrentKeypressListener();
        typeAheadFind->RemoveCurrentWindowFocusListener();
        progress->RemoveProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, typeAheadFind));
      }
      else if (typeAheadFind->mIsTypeAheadOn != wasTypeAheadOn)
        progress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, typeAheadFind), nsIWebProgress::NOTIFY_STATE_DOCUMENT);
    }
  }
  if (!nsCRT::strcasecmp(aPrefName,"accessibility.typeaheadfind.linksonly"))
    prefs->GetBoolPref("accessibility.typeaheadfind.linksonly", &typeAheadFind->mLinksOnlyPref);
  if (!nsCRT::strcasecmp(aPrefName,"accessibility.typeaheadfind.timeout"))
    prefs->GetIntPref("accessibility.typeaheadfind.timeout", &typeAheadFind->mTimeoutLength);
  if (!nsCRT::strcasecmp(aPrefName,"accessibility.browsewithcaret"))
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
 * STATE_START, STATE_REDIRECTING, STATE_TRANSFERRING,
 * STATE_NEGOTIATING, STATE_STOP

 * ===== Where did it occur? =====
 * STATE_IS_REQUEST, STATE_IS_DOCUMENT, STATE_IS_NETWORK, STATE_IS_WINDOW

 * ===== Security info =====
 * STATE_IS_INSECURE, STATE_IS_BROKEN, STATE_IS_SECURE, STATE_SECURE_HIGH
 * STATE_SECURE_MED, STATE_SECURE_LOW
 *
 */

  if (!mFind || (aStateFlags & STATE_IS_DOCUMENT|STATE_TRANSFERRING) != (STATE_IS_DOCUMENT|STATE_TRANSFERRING))
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow)
    return NS_OK;
  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
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

  nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(domWindow));
  if (eventTarget) {
    RemoveCurrentWindowFocusListener();
    AttachNewWindowFocusListener(eventTarget);
  }

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
  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(domEventTarget));
  if (!domNode || gIsFindingText)
    return NS_OK;  // prevents listener callbacks from resetting us during typeahead find processing
  CancelFind();

  nsCOMPtr<nsIDOMDocument> domDoc;
  domNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    domDoc = do_QueryInterface(domNode);
  nsCOMPtr<nsIDocument> targetDoc(do_QueryInterface(domDoc));
  if (!targetDoc)
    return NS_OK;

  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
  targetDoc->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
  nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(ourGlobal));
  nsCOMPtr<nsIDOMEventTarget> rootTarget(do_QueryInterface(domWin));

  if (!rootTarget || domWin == mCurrentWindow)
    return NS_OK;

  // Focus event -- possibly trigger keypress event listening
  nsCOMPtr<nsIPresShell> presShell;
  targetDoc->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell)
    return NS_OK;

  RemoveCurrentSelectionListener();
  RemoveCurrentScrollPositionListener();
  RemoveCurrentKeypressListener();

  mCurrentWindow = domWin;
  mWeakShell = do_GetWeakReference(presShell);

  PRInt16 isEditor;
  presShell->GetSelectionFlags(&isEditor);
  if (isEditor == nsISelectionDisplay::DISPLAY_ALL) {
    // This is an editor window, we don't want to listen to any of its events
    RemoveCurrentWindowFocusListener();
    return NS_OK;
  }

  // Add scroll position and selection listeners, so we can cancel current find when user navigates
  GetSelection(presShell, nsnull, getter_AddRefs(mDocSelection)); // cache for reuse
  AttachNewScrollPositionListener(presShell);
  AttachNewSelectionListener();

  // If focusing on new window, add keypress listener to new window and remove old keypress listener
  AttachNewKeypressListener(rootTarget);

  return NS_OK;
}


NS_IMETHODIMP nsTypeAheadFind::Blur(nsIDOMEvent* aEvent) 
{ 
  if (gIsFindingText)
    return NS_OK;  // prevents listener callbacks from resetting us during typeahead find processing
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  aEvent->GetTarget(getter_AddRefs(domEventTarget));
  nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(domEventTarget));
  if (domWindow) {
    RemoveCurrentSelectionListener();
    RemoveCurrentScrollPositionListener();
    RemoveCurrentKeypressListener();
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
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  aEvent->GetTarget(getter_AddRefs(domEventTarget));

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

  if ((isAlt && !isShift) || isCtrl || isMeta)
    return NS_OK;  // Ignore most modified keys, but alt+shift may be used for entering foreign chars

  // ---------- Get document/presshell/prescontext/selection/etc. ------------

  nsCOMPtr<nsIDOMDocument> domDoc;
  targetNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    return NS_OK;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_OK;
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (!presShell)
    return NS_OK;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return NS_OK;

  nsCOMPtr<nsISelection> localSelection;
  GetSelection(presShell, targetNode, getter_AddRefs(localSelection));  

  if (localSelection != mDocSelection || !mDocSelection)
    return NS_OK;   // Different selection for this node/frame. Must be in editable control.

  // ------------- Escape pressed ---------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE) {
    // Escape accomplishes 2 things: 
    // 1. it is a way for the user to deselect with the keyboard
    mDocSelection->CollapseToStart();
    // 2. it is a way for the user to cancel incremental find with visual feedback (the selection disappears)
    if (mTypeAheadBuffer.IsEmpty())
      return NS_OK;
    aEvent->PreventDefault(); // If Escape is normally used for a command, don't do it
    CancelFind();
    return NS_OK;
  }

  PRBool isFirstVisiblePreferred = PR_FALSE;
  PRBool isBackspace = PR_FALSE;  // When backspace is pressed
  PRBool isLinksOnly = mLinksOnly;

  // ----------- Back space ----------------------
  if (keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
    if (mTypeAheadBuffer.IsEmpty())
      return NS_OK;
    aEvent->PreventDefault(); // If back space is normally used for a command, don't do it
    if (mTypeAheadBuffer.Length() == 1) {
      if (mStartFindRange) {
        mDocSelection->RemoveAllRanges();
        mDocSelection->AddRange(mStartFindRange);
      }
      mDocSelection->CollapseToStart();
      CancelFind();
      return NS_OK;
    }
    mTypeAheadBuffer = Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length() - 1);
    isBackspace = PR_TRUE;
  }
  // ----------- Printable characters --------------
  else {
    PRUnichar uniChar = ToLowerCase(NS_STATIC_CAST(PRUnichar, charCode));
    PRBool bufferLength =mTypeAheadBuffer.Length();
    if (uniChar < ' ')
      return NS_OK; // not a printable character
    if (uniChar == ' ') {
      if (bufferLength == 0)
        return NS_OK; // We ignore space only if it's the first character
      else 
        aEvent->PreventDefault(); // Use the space if it's not the first character, but don't page down
    }
    if (bufferLength > 0 && mTypeAheadBuffer.First() == uniChar) {
      if (mTypeAheadBuffer.Length() == 1 && mTypeAheadBuffer.First() == uniChar)
        isLinksOnly = mIsRepeatingSameChar = PR_TRUE;  // only look for links when using repeated character method
    }
    else if (mIsRepeatingSameChar) {
      isLinksOnly = mLinksOnlyPref;
      mIsRepeatingSameChar = PR_FALSE;
    }
    if (bufferLength == 0 && (uniChar == '`' || uniChar=='\'' || uniChar=='\"')) {
      mLinksOnly = !mLinksOnly; // If you type quote, you can search for all text - it reverses the setting
      return NS_OK;
    }
    mTypeAheadBuffer += uniChar;
    if (bufferLength == 0) {
      // --------- Initialize find -----------
      // If you can see the selection (either not collapsed or through caret browsing), start there
      // Otherwise we're going to start at the first visible element
      PRBool isSelectionCollapsed;
      mDocSelection->GetIsCollapsed(&isSelectionCollapsed);
      isFirstVisiblePreferred = !mCaretBrowsingOn && isSelectionCollapsed;
    }
  }

  FindItNow(isLinksOnly, isFirstVisiblePreferred, isBackspace);
  return NS_OK;
}

void nsTypeAheadFind::FindItNow(PRBool aIsLinksOnly, PRBool aIsFirstVisiblePreferred, PRBool aIsBackspace)
{
  gIsFindingText = PR_TRUE; // prevents listener callbacks from resetting us during typeahead find processing

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (!presShell)
    return;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return;

  // ----------- Set search options ---------------

  mFind->SetFindBackwards(PR_FALSE);
  mFind->SetCaseSensitive(PR_FALSE);
  mFind->SetWordBreaker(nsnull);

  // ------------ Get ranges ready ----------------
  nsCOMPtr<nsIDOMRange> searchRange(do_CreateInstance(kRangeCID)), selectionRange,
    returnRange, startPointRange(do_CreateInstance(kRangeCID)), endPointRange(do_CreateInstance(kRangeCID));

  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return;
  nsCOMPtr<nsIContent> rootContent;
  doc->GetRootContent(getter_AddRefs(rootContent));
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootContent));
  if (!rootNode)
    return;
  PRInt32 childCount;
  if (NS_FAILED(rootContent->ChildCount(childCount)))
    return;
 
  searchRange->SetStart(rootNode, 0);
  searchRange->SetEnd(rootNode, childCount);
  endPointRange->SetStart(rootNode, childCount);
  endPointRange->SetEnd(rootNode, childCount);

  mDocSelection->GetRangeAt(0, getter_AddRefs(selectionRange));
  if (!selectionRange || aIsFirstVisiblePreferred) {
    nsCOMPtr<nsIDOMRange> rootRange(do_CreateInstance(kRangeCID));
    rootRange->SetStart(rootNode, 0);
    rootRange->SetEnd(rootNode, 0);
    // Ensure visible range, move forward if necessary
    IsRangeVisible(presShell, presContext, rootRange, aIsFirstVisiblePreferred, getter_AddRefs(startPointRange));
  }
  else if (aIsBackspace && mStartFindRange)  // when backspace is pressed
    mStartFindRange->CloneRange(getter_AddRefs(startPointRange));
  else {
    PRInt32 startOffset;
    nsCOMPtr<nsIDOMNode> startNode;
    if (mIsRepeatingSameChar) {
      selectionRange->GetEndContainer(getter_AddRefs(startNode));
      selectionRange->GetEndOffset(&startOffset);
    }
    else {
      selectionRange->GetStartContainer(getter_AddRefs(startNode));
      selectionRange->GetStartOffset(&startOffset);
    }
    if (!startNode) 
      startNode = rootNode;
    startPointRange->SetStart(startNode, startOffset);
    startPointRange->SetEnd(startNode, startOffset);
  }

  PRBool hasWrapped = PR_FALSE;

  // -------- Find text loop -----------

  nsAutoString findBuffer;
  if (mIsRepeatingSameChar)
    findBuffer = mTypeAheadBuffer.First();
  else 
    findBuffer = PromiseFlatString(mTypeAheadBuffer);

  do {
    mFind->Find(findBuffer.get(), searchRange, 
                startPointRange, endPointRange, getter_AddRefs(returnRange));
    if (!returnRange) {
      if (!hasWrapped || aIsFirstVisiblePreferred) {
        searchRange->SetStart(rootNode, 0);
        searchRange->SetEnd(rootNode, childCount);
        startPointRange->SetStart(rootNode, 0);
        startPointRange->SetEnd(rootNode, 0);
        if (aIsFirstVisiblePreferred)
          aIsFirstVisiblePreferred = PR_FALSE;
        else if (aIsFirstVisiblePreferred)
          break;  // No need to wrap, started at beginning
        else
          hasWrapped = PR_TRUE;
        continue;
      }
      if (mTypeAheadBuffer.First() == mTypeAheadBuffer.CharAt(1) &&
          mTypeAheadBuffer.Last() != mTypeAheadBuffer.First()) {
        // The aardvark rule, if they repeat the same character and then change
        // first find exactly what they typed, if not there start over with new char
        mTypeAheadBuffer = mTypeAheadBuffer.Last();
        FindItNow(PR_TRUE, aIsFirstVisiblePreferred, aIsBackspace);
        return;
      }
      // ----- Nothing found -----
      nsCOMPtr<nsISound> soundInterface(do_CreateInstance("@mozilla.org/sound;1"));
      DisplayStatus(PR_FALSE, PR_FALSE); // Display failure status
      if (soundInterface)
        soundInterface->Beep();
      // Remove bad character from buffer, so we can continue typing from last matched character
      mTypeAheadBuffer = Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length() - 1);
      break;
    }

    // ------- Test resulting found range for success conditions ------
    PRBool isInsideLink, isStartingLink, isInsideViewPort = PR_TRUE;
    RangeStartsInsideLink(returnRange, presShell, &isInsideLink, &isStartingLink);

    if (!IsRangeVisible(presShell, presContext, returnRange, aIsFirstVisiblePreferred, getter_AddRefs(startPointRange)) ||
        (mIsRepeatingSameChar && !isStartingLink) || (aIsLinksOnly && !isInsideLink)) {
      // ------ Failure ------
      // Start find again from here 
      startPointRange->Collapse(PR_FALSE);  // collapse to end
      continue;
    }

    // ------ Success! -------
    mKeepSelectionOnCancel = PR_FALSE;
    mDocSelection->RemoveAllRanges();
    mDocSelection->AddRange(returnRange);
    nsCOMPtr<nsIEventStateManager> esm;
    presContext->GetEventStateManager(getter_AddRefs(esm));
    if (esm) {
      PRBool isSelectionWithFocus;
      esm->MoveFocusToCaret(PR_TRUE, &isSelectionWithFocus);
    }
    DisplayStatus(PR_TRUE, PR_FALSE); // display success status

    // ------- Store current find string for regular find usage: find-next or find dialog text field ------
    if (mFindService) {
#ifdef RESTORE_SEARCH
      if (mTypeAheadBuffer.Length() == 1 && mOldSearchString.IsEmpty()) {
        mFindService->GetSearchString(mOldSearchString);
        mFindService->GetWrapFind(&mOldWrapSetting);
        mFindService->GetMatchCase(&mOldCaseSetting);
        mFindService->GetEntireWord(&mOldWordSetting);
        mFindService->GetFindBackwards(&mOldDirectionSetting);
      }
#endif
      mFindService->SetSearchString(mTypeAheadBuffer);
      mFindService->SetWrapFind(PR_TRUE);
      mFindService->SetMatchCase(PR_FALSE);
      mFindService->SetEntireWord(PR_FALSE);
      mFindService->SetFindBackwards(PR_FALSE);
    }

    // ------- If accessibility.typeaheadfind.timeout is set, cancel find after specified # milliseconds
    if (mTimeoutLength) {
      if (!mTimer)
        mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (mTimer)
        mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), mTimeoutLength);
    }
    break;
  }
  while (PR_TRUE);

  // ----------- Clean up ----------------

  if (mTypeAheadBuffer.Length() == 1) {  // If first letter, store where the first find succeeded (mStartFindRange)
    mStartFindRange = nsnull;
    nsCOMPtr<nsIDOMRange> startFindRange;
    mDocSelection->GetRangeAt(0, getter_AddRefs(startFindRange));
    startFindRange->CloneRange(getter_AddRefs(mStartFindRange));
  }

  gIsFindingText = PR_FALSE;
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


// ------- Helper Methods ---------------

void nsTypeAheadFind::CancelFind()
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
    DisplayStatus(PR_FALSE, PR_TRUE); // Clear status
    if (!mKeepSelectionOnCancel)
      mDocSelection->CollapseToStart();
#ifdef RESTORE_SEARCH
    // Restore old find settings
    // not working to well, because timer cancels find
    // which is returns Ctrl+G to old find
    if (mFindService && !mOldSearchString.IsEmpty()) {
      mFindService->SetSearchString(mOldSearchString);
      mFindService->SetWrapFind(mOldWrapSetting);
      mFindService->SetMatchCase(mOldCaseSetting);
      mFindService->SetEntireWord(mOldWordSetting);
      mFindService->SetFindBackwards(mOldDirectionSetting);
    }
#endif
  }
  mLinksOnly = mLinksOnlyPref;

  // These will be initialized to their true values after the first character is typed
  mCaretBrowsingOn = PR_FALSE;
  mIsRepeatingSameChar = PR_FALSE;
  mStartFindRange = nsnull;
}


void nsTypeAheadFind::RemoveCurrentScrollPositionListener()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  nsCOMPtr<nsIViewManager> vm;

  if (presShell)
    presShell->GetViewManager(getter_AddRefs(vm));

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (scrollableView)
    scrollableView->RemoveScrollPositionListener(NS_STATIC_CAST(nsIScrollPositionListener *, this));
  mWeakShell = nsnull;
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
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mDocSelection));
  if (selPrivate)
    selPrivate->RemoveSelectionListener(this); // remove us if we are a listener
  mDocSelection = nsnull;
}


void nsTypeAheadFind::AttachNewSelectionListener()
{
  // Only add selection listener if caret browsing is on
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mDocSelection));
  if (selPrivate)
    selPrivate->AddSelectionListener(this);
}


void nsTypeAheadFind::RemoveCurrentKeypressListener()
{
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mCurrentWindow));
  if (target)
    target->RemoveEventListener(NS_LITERAL_STRING("keypress"), NS_STATIC_CAST(nsIDOMKeyListener*, this), PR_FALSE);
}


void nsTypeAheadFind::AttachNewKeypressListener(nsIDOMEventTarget *aTarget)
{
  aTarget->AddEventListener(NS_LITERAL_STRING("keypress"),  NS_STATIC_CAST(nsIDOMKeyListener*, this), PR_FALSE);
}


void nsTypeAheadFind::RemoveCurrentWindowFocusListener()
{
  // Remove focus listener, and unload listener as well, since it's sole purpose was to remove focus listener
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mCurrentWindow));
  if (target) {
    target->RemoveEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_FALSE);
    target->RemoveEventListener(NS_LITERAL_STRING("unload"), NS_STATIC_CAST(nsIDOMLoadListener*, this), PR_FALSE);
  }
  mCurrentWindow = nsnull;
}


void nsTypeAheadFind::AttachNewWindowFocusListener(nsIDOMEventTarget *aTarget)
{
  // Add focus listener, and unload listener, which will remove focus listener when the window's life ends
  aTarget->AddEventListener(NS_LITERAL_STRING("focus"),  NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_FALSE);
  aTarget->AddEventListener(NS_LITERAL_STRING("unload"),  NS_STATIC_CAST(nsIDOMLoadListener*, this), PR_FALSE);
}


void nsTypeAheadFind::GetSelection(nsIPresShell *aPresShell, 
                                   nsIDOMNode *aCurrentNode, nsISelection **aDomSel)
{
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
    nsCOMPtr<nsISelectionController> selCon;
    frame->GetSelectionController(presContext, getter_AddRefs(selCon));
    if (selCon) {
      nsCOMPtr<nsISelection> domSel;
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, aDomSel);
    }
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


void nsTypeAheadFind::DisplayStatus(PRBool aSuccess, PRBool aClearStatus)
{
  // Cache mBrowserChrome, used for showing status messages
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
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
      nsAutoString closeQuoteString;
      GetTranslatedString(NS_LITERAL_STRING("closequote"), closeQuoteString);
      statusString += mTypeAheadBuffer + closeQuoteString;
    }
  }
  browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, PromiseFlatString(statusString).get());
}
