/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Aaron Leventhal.
 * Portionsk  created by Aaron Leventhal are Copyright (C) 2001
 * Aaron Leventhal. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under
 * the MPL, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the
 * GPL. If you do not delete the provisions above, a recipient may use
 * your version of this file under either the MPL or the GPL.
 *
 * Contributor(s):
 */

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIGenericFactory.h"
#include "nsIWebProgress.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEventTarget.h"
#include "nsIPref.h"

#include "nsIRegistry.h"
#include "nsString.h"

#include "nsIDOMNode.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsICaret.h"

// Header for this class
#include "nsAccessProxy.h"

// #define NS_DEBUG_ACCESS_BUILTIN 1


////////////////////////////////////////////////////////////////////////


NS_IMPL_ISUPPORTS4(nsAccessProxy, nsIObserver, nsISupportsWeakReference, nsIWebProgressListener, nsIDOMEventListener)

nsAccessProxy* nsAccessProxy::mInstance = nsnull;

nsAccessProxy::nsAccessProxy()
{
  NS_INIT_REFCNT();
}

nsAccessProxy::~nsAccessProxy()
{
}

nsAccessProxy *nsAccessProxy::GetInstance()
{
  if (mInstance == nsnull) {
    mInstance = new nsAccessProxy();
    // Will be released in the module destructor
    NS_IF_ADDREF(mInstance);
  }

  NS_IF_ADDREF(mInstance);
  return mInstance;
}

void nsAccessProxy::ReleaseInstance()
{
  NS_IF_RELEASE(nsAccessProxy::mInstance);
}


NS_IMETHODIMP nsAccessProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  nsresult rv;

  //////// Get Type of Event into a string called eventName ///////
  nsAutoString eventNameStr;
  rv=aEvent->GetType(eventNameStr);
  if (NS_FAILED(rv))
    return rv;
  // Print event name and styles debugging messages
  #ifdef NS_DEBUG_ACCESS_BUILTIN
  printf("\n==== %s event occurred ====\n",NS_ConvertUCS2toUTF8(eventNameStr).get());
  #endif

  ////////// Get Target Node - place in document where event was fired ////////////
  nsCOMPtr<nsIDOMEventTarget> targetNode;
  rv = aEvent->GetOriginalTarget(getter_AddRefs(targetNode));

  if (NS_FAILED(rv))
    return rv;
  if (!targetNode)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(targetNode);
  if (!domNode)
    return NS_OK;

  // get the Document and PresShell
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIDocument> doc;
  domNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (domDoc) {
    doc = do_QueryInterface(domDoc);
    if (doc && doc->GetNumberOfShells()>0) {
      doc->GetShellAt(0, getter_AddRefs(presShell));
    }
  }
  //return  NS_OK;
  /*
  if (presShell && eventNameStr.EqualsWithConversion("click")) {
    nsCOMPtr<nsIFrameSelection> frameSelection;
    presShell->GetFrameSelection(getter_AddRefs(frameSelection));
    if (!frameSelection)
      return NS_OK;
    nsCOMPtr<nsISelection> domSelection;
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL,
                            getter_AddRefs(domSelection));
    if (!domSelection)
      return NS_OK;
    nsCOMPtr<nsIDOMNode> focusDomNode;
    domSelection->GetAnchorNode(getter_AddRefs(focusDomNode));
    if (focusDomNode) domNode=focusDomNode;
    // first, tell the caret which selection to use
    nsCOMPtr<nsICaret> caret;
    presShell->GetCaret(getter_AddRefs(caret));
    if (!caret) return NS_OK;
    caret->SetCaretDOMSelection(domSelection);
    // tell the pres shell to enable the caret, rather than settings its visibility directly.
    // this way the presShell's idea of caret visibility is maintained.
    nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(presShell);
    if (!selCon) return NS_ERROR_NO_INTERFACE;
    selCon->SetCaretEnabled(PR_TRUE);
    caret->SetCaretVisible(PR_TRUE);
  }
  */

  return NS_OK;
}


// This method gets called on application startup
NS_IMETHODIMP nsAccessProxy::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *aData) 
{
  static PRBool accessProxyInstalled;

  nsresult rv = NS_OK;
  nsDependentString aTopicString(aTopic);

  if (accessProxyInstalled && aTopicString.Equals(NS_LITERAL_STRING(NS_XPCOM_SHUTDOWN_OBSERVER_ID)))
    return Release();

  if (!accessProxyInstalled && aTopicString.Equals(NS_LITERAL_STRING(APPSTARTUP_CATEGORY))) {
    accessProxyInstalled = PR_TRUE; // Set to TRUE even for failure cases - we don't want to try more than once
    nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
    rv = NS_ERROR_FAILURE;
    if (progress) {
      rv = progress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,this));
      if (NS_SUCCEEDED(rv))
        AddRef();
    }
     // install xpcom shutdown observer
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIObserverService> observerService(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv)) 
        rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    }
  }
  return rv;
}


NS_IMETHODIMP nsAccessProxy::OnStateChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
{
/* PRInt32 aStateFlags ...
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

  if ((aStateFlags & (STATE_STOP|STATE_START)) && (aStateFlags & STATE_IS_DOCUMENT)) {
    // Test for built in text to speech or braille display usage preference
    // If so, attach event handlers to window. If not, don't.
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    nsXPIDLCString textToSpeechEngine, brailleDisplayEngine;
    if (prefs) {
      prefs->CopyCharPref("accessibility.usetexttospeech", getter_Copies(textToSpeechEngine));
      prefs->CopyCharPref("accessibility.usebrailledisplay", getter_Copies(brailleDisplayEngine));
    }

    if ((textToSpeechEngine && *textToSpeechEngine) || (brailleDisplayEngine && *brailleDisplayEngine)) {  
      // Yes, prefs say we will need handlers for this 
      nsCOMPtr<nsIDOMWindow> domWindow;
      aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));

      if (domWindow) {
        nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(domWindow);
        nsCOMPtr<nsIDOMWindowInternal> windowInternal = do_QueryInterface(domWindow);
        nsCOMPtr<nsIDOMWindowInternal> opener;
        if (windowInternal)
          windowInternal->GetOpener(getter_AddRefs(opener));
        if (eventTarget && opener) {
          eventTarget->AddEventListener(NS_LITERAL_STRING("keyup"), this, PR_FALSE);
          eventTarget->AddEventListener(NS_LITERAL_STRING("keypress"), this, PR_FALSE);
          eventTarget->AddEventListener(NS_LITERAL_STRING("focus"), this, PR_FALSE);
          eventTarget->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
          eventTarget->AddEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE); // for debugging
        }
      }
    }
  }

  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsAccessProxy::OnProgressChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
  PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  // We can use this to report the percentage done
  #ifdef NS_DEBUG_ACCESS_BUILTIN
  // printf ("** nsAccessProxy ** OnProgressChange ** \n");
  #endif
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsAccessProxy::OnLocationChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsIURI *location)
{
  // Load has been verified, it will occur, about to commence
  #ifdef NS_DEBUG_ACCESS_BUILTIN
  // printf ("** nsAccessProxy ** OnLocationChange ** \n");
  #endif
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsAccessProxy::OnStatusChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  // Status bar has changed
  #ifdef NS_DEBUG_ACCESS_BUILTIN
  // printf ("** nsAccessProxy ** OnStatusChange ** \n");
  #endif
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP nsAccessProxy::OnSecurityChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 state)
{
  // Security setting has changed
  #ifdef NS_DEBUG_ACCESS_BUILTIN
  printf ("** nsAccessProxy ** OnSecurityChange ** \n");
  #endif
  return NS_OK;
}

