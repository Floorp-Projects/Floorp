/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * Contributor(s): Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                 Ed Burns <edburns@acm.org>
 *                 Jason Mawdsley <jason@macadamian.com>
 *                 Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 *                 Kyle Yuan <kyle.yuan@sun.com>
 */

#include <limits.h>
#include "CBrowserContainer.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFocus.h"
#include "nsIRequest.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPromptService.h"

#include "prprf.h" // for PR_snprintf
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsFileSpec.h" // for nsAutoCString

#include "dom_util.h"

#include "PromptActionEvents.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

jobject gPromptProperties = nsnull;

PRInt32 CBrowserContainer::mInstanceCount = 0;

CBrowserContainer::CBrowserContainer(nsIWebBrowser *pOwner, JNIEnv *env,
                                     WebShellInitContext *yourInitContext) :
    m_pOwner(pOwner), mJNIEnv(env), mInitContext(yourInitContext),
    mDocTarget(nsnull), mMouseTarget(nsnull), mPrompt(nsnull),
    mDomEventTarget(nsnull), inverseDepth(-1),
    properties(nsnull), currentDOMEvent(nsnull)
{
    // initialize the string constants (including properties keys)
    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants();
    }
    mInstanceCount++;
}


CBrowserContainer::~CBrowserContainer()
{
    m_pOwner = nsnull;
    mJNIEnv = nsnull;
    mInitContext = nsnull;
    mDocTarget = nsnull;
    mMouseTarget = nsnull;
    mDomEventTarget = nsnull;
    inverseDepth = -1;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    if (properties) {
        ::util_DeleteGlobalRef(env, properties);
    }
    properties = nsnull;
    currentDOMEvent = nsnull;
    mInstanceCount--;
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CBrowserContainer)
NS_IMPL_RELEASE(CBrowserContainer)

NS_INTERFACE_MAP_BEGIN(CBrowserContainer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
    NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
    NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
    NS_INTERFACE_MAP_ENTRY(wcIBrowserContainer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CBrowserContainer::GetInterface(const nsIID & uuid, void * *result)
{
    if (uuid.Equals(NS_GET_IID(nsIDOMWindow)))
    {
        if (m_pOwner)
            return m_pOwner->GetContentDOMWindow((nsIDOMWindow **) result);
        return NS_ERROR_NOT_INITIALIZED;
    }

    return QueryInterface(uuid, result);
}

///////////////////////////////////////////////////////////////////////////////
// nsIEmbeddingSiteWindow

NS_IMETHODIMP CBrowserContainer::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    printf ("SetDimensions\n");
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    printf ("GetDimensions\n");
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::GetSiteWindow(void** aSiteWindow)
{
  if (!aSiteWindow)
    return NS_ERROR_NULL_POINTER;

  *aSiteWindow = NS_REINTERPRET_CAST(void *, this);
  return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::SetFocus()
{
    printf ("SetFocus\n");
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::GetTitle(PRUnichar** aTitle)
{
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::SetTitle(const PRUnichar* aTitle)
{
    // PENDING(kyle): set the browser window title here
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::GetVisibility(PRBool *aVisibility)
{
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::SetVisibility(PRBool aVisibility)
{
    printf ("SetVisibility\n");
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP CBrowserContainer::OnStateChange(nsIWebProgress *aWebProgress,
                                               nsIRequest *aRequest,
                                               PRUint32 aStateFlags,
                                               PRUint32 aStatus)
{
    nsCAutoString name;
    nsAutoString uname;
    nsCOMPtr<nsIDOMWindow> domWin;
    nsresult rv;

    if (NS_FAILED(rv = aRequest->GetName(name))) {
        return rv;
    }

    uname.AssignWithConversion(name.get());

    //
    // document states
    //
    if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_DOCUMENT)) {
        if (NS_FAILED(rv =aWebProgress->GetDOMWindow(getter_AddRefs(domWin)))||
            !domWin) {
            return rv;
        }

        domWin->GetDocument(getter_AddRefs(mInitContext->currentDocument));
        doStartDocumentLoad(uname.get());
    }
    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_DOCUMENT)) {
        doEndDocumentLoad(aWebProgress);
    }
    //
    // request states
    //
    if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_REQUEST)) {
        doStartURLLoad(uname.get());
    }
    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_REQUEST)) {
        doEndURLLoad(uname.get());
    }
    if (aStateFlags & STATE_REDIRECTING) {
        printf("debug: edburns: STATE_REDIRECTING\n");
    }
    if (aStateFlags & STATE_TRANSFERRING) {
        printf("debug: edburns: STATE_TRANSFERRING\n");
    }
    if (aStateFlags & STATE_NEGOTIATING) {
        printf("debug: edburns: STATE_NEGOTIATING\n");
    }
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::OnSecurityChange(nsIWebProgress *aWebProgress,
                                                  nsIRequest *aRequest,
                                                  PRUint32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProgressChange (in nsIChannel channel, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP CBrowserContainer::OnProgressChange(nsIWebProgress *aWebProgress,
                                                  nsIRequest *aRequest,
                                                  PRInt32 aCurSelfProgress,
                                                  PRInt32 aMaxSelfProgress,
                                                  PRInt32 curTotalProgress,
                                                  PRInt32 maxTotalProgress)
{
    PRInt32 percentComplete = 0;
    nsCAutoString name;
    nsAutoString autoName;
    jobject msgJStr = nsnull;
    nsresult rv;

    if (!mDocTarget) {
        return NS_OK;
    }

    // PENDING(edburns): Allow per fetch progress reporting.  Right now
    // we only have coarse grained support.
    if (maxTotalProgress) {
        percentComplete = curTotalProgress / maxTotalProgress;
    }

#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnProgressURLLoad\n"));
    }
#endif

    if (NS_FAILED(rv = aRequest->GetName(name))) {
        return rv;
    }
    autoName.AssignWithConversion(name.get());
    // build up the string to be sent
    autoName.AppendWithConversion(" ");
    autoName.AppendInt(percentComplete);
    autoName.AppendWithConversion("%");


    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    msgJStr = (jobject) ::util_NewString(env, autoName.get(),
                                         autoName.Length());

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread, mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[PROGRESS_URL_LOAD_EVENT_MASK],
                         msgJStr);

    if (msgJStr) {
        ::util_DeleteString(mInitContext->env, (jstring) msgJStr);
    }


    return NS_OK;
}

/* void onStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP CBrowserContainer::OnStatusChange(nsIWebProgress *aWebProgress,
                                                nsIRequest *aRequest,
                                                nsresult aStatus,
                                                const PRUnichar *aMessage)
{
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnStatusURLLoad\n"));
    }
#endif
    nsAutoString aMsg(aMessage);
    int length = aMsg.Length();

    // IMPORTANT: do not use initContext->env here since it comes
    // from another thread.  Use JNU_GetEnv instead.

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jstring statusMessage = ::util_NewString(env, (const jchar *)
                                             aMsg.get(), length);

    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread,
                         mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[STATUS_URL_LOAD_EVENT_MASK],
                         (jobject) statusMessage);

    if (statusMessage) {
        ::util_DeleteString(mInitContext->env, statusMessage);
    }

    return NS_OK;
}

/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP CBrowserContainer::OnLocationChange(nsIWebProgress *aWebProgress,
                                                  nsIRequest *aRequest,
                                                  nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
// Helper methods for our nsIWebProgressListener implementation
//

nsresult JNICALL
CBrowserContainer::doStartDocumentLoad(const PRUnichar *aDocumentName)
{
    // remove the old mouse listener for the old document
    if (mDomEventTarget) {
        mDomEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseover"),
                                             this, PR_FALSE);
        mDomEventTarget = nsnull;
    }

    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnStartDocumentLoad\n"));
    }
#endif
    jobject urlJStr = nsnull;
    if (nsnull != aDocumentName) {

        // IMPORTANT: do not use initContext->env here since it comes
        // from another thread.  Use JNU_GetEnv instead.

        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

        urlJStr = (jobject) ::util_NewString(env, aDocumentName,
                                             nsCRT::strlen(aDocumentName));
    }

    // maskValues array comes from ../src_share/jni_util.h
    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread, mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[START_DOCUMENT_LOAD_EVENT_MASK],
                         urlJStr);


    if (urlJStr) {
        ::util_DeleteString(mInitContext->env, (jstring) urlJStr);
    }

    return NS_OK;
}

// we need this to fire the document complete
nsresult JNICALL
CBrowserContainer::doEndDocumentLoad(nsIWebProgress *aWebProgress)
{

    nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(mInitContext->webBrowser));
    focus->Activate();

    nsCOMPtr<nsIDOMWindow> domWin;

    if (nsnull != aWebProgress) {
        if (NS_SUCCEEDED(aWebProgress->GetDOMWindow(getter_AddRefs(domWin)))
            && domWin) {
            if (NS_SUCCEEDED(domWin->GetDocument(getter_AddRefs(mInitContext->currentDocument)))) {
                // if we have a mouse listener
                if (mMouseTarget) {
                    // turn the currentDocument into an nsIDOMEventTarget
                    mDomEventTarget =
                        do_QueryInterface(mInitContext->currentDocument);
                    // if successful
                    if (mDomEventTarget) {
                        mDomEventTarget->AddEventListener(NS_LITERAL_STRING("mouseover"),
                                                          this, PR_FALSE);
                    }
                }
            }
        }
    }
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnEndDocumentLoad\n"));
    }
#endif

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread, mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[END_DOCUMENT_LOAD_EVENT_MASK],
                         nsnull);


    return NS_OK;
}

nsresult JNICALL
CBrowserContainer::doStartURLLoad(const PRUnichar *aDocumentName)
{
    //NOTE: This appears to get fired once for each individual item on a page.
    if (!mDocTarget) {
        return NS_OK;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnStartURLLoad\n"));
    }
#endif
    nsAutoString nameAutoStr(aDocumentName);
    jstring nameJStr = ::util_NewString(env, (const jchar *)
                                             nameAutoStr.get(),
                                             nameAutoStr.Length());

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread, mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[START_URL_LOAD_EVENT_MASK],
                         nameJStr);

    if (nameJStr) {
        ::util_DeleteString(mInitContext->env, nameJStr);
    }

    return NS_OK;
}

nsresult JNICALL
CBrowserContainer::doEndURLLoad(const PRUnichar *aDocumentName)
{
    //NOTE: This appears to get fired once for each individual item on a page.
    if (!mDocTarget) {
        return NS_OK;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnStartURLLoad\n"));
    }
#endif
    nsAutoString nameAutoStr(aDocumentName);
    jstring nameJStr = ::util_NewString(env, (const jchar *)
                                             nameAutoStr.get(),
                                             nameAutoStr.Length());

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread, mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK],
                         nameJStr);

    if (nameJStr) {
        ::util_DeleteString(mInitContext->env, nameJStr);
    }

    return NS_OK;
}



///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

NS_IMETHODIMP CBrowserContainer::OnStartURIOpen(nsIURI *aURI, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserContainer::DoContent(const char *aContentType, PRBool aIsContentPreferred, nsIRequest *aRequest, nsIStreamListener **aContentHandler, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::IsPreferred(const char *aContentType, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::CanHandleContent(const char *aContentType, PRBool aIsContentPreferred, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsISupports loadCookie; */
NS_IMETHODIMP CBrowserContainer::GetLoadCookie(nsISupports * *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP CBrowserContainer::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CBrowserContainer::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CBrowserContainer::DestroyBrowserWindow()
{
    PR_ASSERT(PR_FALSE);
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::IsWindowModal(PRBool *retVal)
{
    if (!retVal) {
        return NS_ERROR_NULL_POINTER;
    }
    *retVal = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
CBrowserContainer::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::GetChromeFlags(PRUint32 *aChromeFlags)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::SetChromeFlags(PRUint32 aChromeFlags)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::ShowAsModal(void)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CBrowserContainer::ExitModalEventLoop(nsresult aStatus)
{
    return NS_ERROR_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDocumentLoaderObserver implementation
#if 0

// PENDING(edburns): when you figure out how to do this, implement it
// with nsIWebProgressListener.

NS_IMETHODIMP
CBrowserContainer::OnStartURLLoad(nsIDocumentLoader* loader,
                                  nsIRequest* aRequest)
{
}

NS_IMETHODIMP
CBrowserContainer::OnEndURLLoad(nsIDocumentLoader* loader, nsIRequest* request, nsresult aStatus)
{
  //NOTE: This appears to get fired once for each individual item on a page.
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4,
               ("CBrowserContainer: OnStartURLLoad\n"));
    }
#endif

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread, mDocTarget,
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK], nsnull);

    return NS_OK;
}

#endif

NS_IMETHODIMP
CBrowserContainer::HandleEvent(nsIDOMEvent* aEvent)
{
    return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::MouseDown(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread,
                         mMouseTarget,
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_DOWN_EVENT_MASK],
                         properties);
    return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::MouseUp(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread,
                         mMouseTarget,
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_UP_EVENT_MASK],
                         properties);
    return NS_OK;
}

NS_IMETHODIMP
CBrowserContainer::MouseClick(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_StoreIntoPropertiesObject(env, properties,  CLICK_COUNT_KEY,
                                     ONE_VALUE, (jobject)
                                     &(mInitContext->shareContext));

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread,
                         mMouseTarget,
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_CLICK_EVENT_MASK],
                         properties);
    return NS_OK;
}

NS_IMETHODIMP
CBrowserContainer::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    ::util_StoreIntoPropertiesObject(env, properties,  CLICK_COUNT_KEY,
                                     TWO_VALUE, (jobject)
                                     &(mInitContext->shareContext));

    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread,
                         mMouseTarget,
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_DOUBLE_CLICK_EVENT_MASK],
                         properties);
    return NS_OK;
}

NS_IMETHODIMP
CBrowserContainer::MouseOver(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread,
                         mMouseTarget,
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_OVER_EVENT_MASK],
                         properties);
    return NS_OK;
}

NS_IMETHODIMP
CBrowserContainer::MouseOut(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);

    getPropertiesFromEvent(aMouseEvent);
    util_SendEventToJava(mInitContext->env,
                         mInitContext->nativeEventThread,
                         mMouseTarget,
                         MOUSE_LISTENER_CLASSNAME,
                         DOMMouseListener_maskValues[MOUSE_OUT_EVENT_MASK],
                         properties);
    return NS_OK;
}

//
// From wcIBrowserContainer
//


NS_IMETHODIMP CBrowserContainer::AddMouseListener(jobject target)
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    if (-1 == DOMMouseListener_maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/WCMouseEvent",
                                                DOMMouseListener_maskNames,
                                                DOMMouseListener_maskValues);
    }
    mMouseTarget = target;

    return rv;
}

NS_IMETHODIMP CBrowserContainer::AddDocumentLoadListener(jobject target)
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    if (-1 == DocumentLoader_maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/DocumentLoadEvent",
                                                DocumentLoader_maskNames,
                                                DocumentLoader_maskValues);
    }
    mDocTarget = target;

    return rv;
}

NS_IMETHODIMP CBrowserContainer::SetPrompt(jobject target)
{
    nsresult rv = NS_OK;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (mPrompt) {
        ::util_DeleteGlobalRef(env, mPrompt);
        mPrompt = nsnull;
    }
    if (nsnull == (mPrompt = ::util_NewGlobalRef(env, target))) {
        ::util_ThrowExceptionToJava(env, "Exception: Navigation.nativeSetPrompt(): can't create NewGlobalRef\n\tfor argument");
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}


NS_IMETHODIMP CBrowserContainer::RemoveMouseListener()
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }


    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mMouseTarget);
    mMouseTarget = nsnull;
    return rv;
}

NS_IMETHODIMP CBrowserContainer::RemoveDocumentLoadListener()
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mDocTarget);
    mDocTarget = nsnull;
    return rv;
}

NS_IMETHODIMP CBrowserContainer::GetInstanceCount(PRInt32 *outCount)
{
    PR_ASSERT(outCount);
    *outCount = mInstanceCount;
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::RemoveAllListeners()
{
    if (mDomEventTarget) {
        mDomEventTarget->RemoveEventListener(NS_LITERAL_STRING("mouseover"),
                                             this, PR_FALSE);
        mDomEventTarget = nsnull;
    }

    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mDocTarget);
    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), mMouseTarget);

    mMouseTarget = nsnull;
    mDocTarget = nsnull;

    return NS_OK;
}


//
// Methods from nsIWebShellContainer
//

NS_IMETHODIMP CBrowserContainer::WillLoadURL(nsIWebShell* aShell,
                                             const PRUnichar* aURL,
                                             nsLoadType aReason)
{
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::BeginLoadURL(nsIWebShell* aShell,
                                              const PRUnichar* aURL)
{
    return NS_OK;
}

NS_IMETHODIMP CBrowserContainer::EndLoadURL(nsIWebShell* aShell,
                                            const PRUnichar* aURL,
                                            nsresult aStatus)
{
    return NS_OK;
}

jobject JNICALL CBrowserContainer::getPropertiesFromEvent(nsIDOMEvent *event)
{
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    nsCOMPtr<nsIDOMNode> currentNode;
    nsCOMPtr<nsIDOMEvent> aMouseEvent = event;
    nsresult rv = NS_OK;;

    rv = aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    if (NS_FAILED(rv)) {
        return properties;
    }
    currentNode = do_QueryInterface(eventTarget);
    if (!currentNode) {
        return properties;
    }
    inverseDepth = 0;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (properties) {
        util_ClearPropertiesObject(env, properties, (jobject)
                                   &(mInitContext->shareContext));
    }
    else {
        if (!(properties =
              util_CreatePropertiesObject(env, (jobject)
                                          &(mInitContext->shareContext)))) {
            return properties;
        }
    }
    dom_iterateToRoot(currentNode, CBrowserContainer::takeActionOnNode,
                      (void *)this);
    addMouseEventDataToProperties(aMouseEvent);

    return properties;
}

void JNICALL CBrowserContainer::addMouseEventDataToProperties(nsIDOMEvent *aMouseEvent)
{
    // if the initialization failed, don't modify the properties
    if (!properties || !util_StringConstantsAreInitialized()) {
        return;
    }
    nsresult rv;

    // Add modifiers, keys, mouse buttons, etc, to the properties table
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent;

    rv = aMouseEvent->QueryInterface(nsIDOMMouseEvent::GetIID(),
                                     getter_AddRefs(mouseEvent));
    if (NS_FAILED(rv)) {
        return;
    }

    PRInt32 intVal;
    PRUint16 int16Val;
    PRBool boolVal;
    char buf[20];
    jstring strVal;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    // PENDING(edburns): perhaps use a macro to speed this up?
    rv = mouseEvent->GetScreenX(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, SCREEN_X_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetScreenY(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, SCREEN_Y_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetClientX(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, CLIENT_X_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetClientY(&intVal);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(intVal, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, CLIENT_Y_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    int16Val = 0;
    rv = mouseEvent->GetButton(&int16Val);
    if (NS_SUCCEEDED(rv)) {
        WC_ITOA(int16Val, buf, 10);
        strVal = ::util_NewStringUTF(env, buf);
        ::util_StoreIntoPropertiesObject(env, properties, BUTTON_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetAltKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, ALT_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetCtrlKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, CTRL_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetShiftKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, SHIFT_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }

    rv = mouseEvent->GetMetaKey(&boolVal);
    if (NS_SUCCEEDED(rv)) {
        strVal = boolVal ? (jstring) TRUE_VALUE : (jstring) FALSE_VALUE;
        ::util_StoreIntoPropertiesObject(env, properties, META_KEY,
                                         (jobject) strVal,
                                         (jobject)
                                         &(mInitContext->shareContext));
    }
}

nsresult JNICALL CBrowserContainer::takeActionOnNode(nsCOMPtr<nsIDOMNode> currentNode,
                                                        void *myObject)
{
    nsresult rv = NS_OK;
    nsString nodeInfo, nodeName, nodeValue; //, nodeDepth;
    jstring jNodeName, jNodeValue;
    PRUint32 depth = 0;
    CBrowserContainer *curThis = nsnull;
    //const PRUint32 depthStrLen = 20;
    //    char depthStr[depthStrLen];
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    PR_ASSERT(nsnull != myObject);
    curThis = (CBrowserContainer *) myObject;
    depth = curThis->inverseDepth++;

    if (!(curThis->properties)) {
        return rv;
    }
    // encode the depth of the node
    //    PR_snprintf(depthStr, depthStrLen, "depthFromLeaf:%d", depth);
    //    nodeDepth = (const char *) depthStr;

    // Store the name and the value of this node

    {
        // get the name and prepend the depth
        rv = currentNode->GetNodeName(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeName = nodeDepth;
        //        nodeName += nodeInfo;
        nodeName = nodeInfo;

        if (prLogModuleInfo) {
            char * nodeInfoCStr = ToNewCString(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", nodeInfoCStr));
            nsMemory::Free(nodeInfoCStr);
        }

        rv = currentNode->GetNodeValue(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeValue = nodeDepth;
        //        nodeValue += nodeInfo;
        nodeValue = nodeInfo;

        if (prLogModuleInfo) {
            char * nodeInfoCStr = ToNewCString(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", (const char *)nodeInfoCStr));
            nsMemory::Free(nodeInfoCStr);
        }

        jNodeName = ::util_NewString(env, nodeName.get(),
                                     nodeName.Length());
        jNodeValue = ::util_NewString(env, nodeValue.get(),
                                      nodeValue.Length());

        util_StoreIntoPropertiesObject(env, (jobject) curThis->properties,
                                       (jobject) jNodeName,
                                       (jobject) jNodeValue,
                                       (jobject)
                                       &(curThis->mInitContext->shareContext));
        if (jNodeName) {
            ::util_DeleteString(env, jNodeName);
        }
        if (jNodeValue) {
            ::util_DeleteString(env, jNodeValue);
        }
    } // end of Storing the name and value of this node

    // store any attributes of this node
    {
        nsCOMPtr<nsIDOMNamedNodeMap> nodeMap;
        rv = currentNode->GetAttributes(getter_AddRefs(nodeMap));
        if (NS_FAILED(rv) || !nodeMap) {
            return rv;
        }
        PRUint32 length, i;
        rv = nodeMap->GetLength(&length);
        if (NS_FAILED(rv)) {
            return rv;
        }
        for (i = 0; i < length; i++) {
            rv = nodeMap->Item(i, getter_AddRefs(currentNode));

            if (nsnull == currentNode) {
                return rv;
            }

            rv = currentNode->GetNodeName(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeName = nodeDepth;
            //            nodeName += nodeInfo;
            nodeName = nodeInfo;

            if (prLogModuleInfo) {
                char * nodeInfoCStr = ToNewCString(nodeName);
                PR_LOG(prLogModuleInfo, 4,
                       ("attribute[%d], %s", i, (const char *)nodeInfoCStr));
                nsMemory::Free(nodeInfoCStr);
            }

            rv = currentNode->GetNodeValue(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeValue = nodeDepth;
            //            nodeValue += nodeInfo;
            nodeValue = nodeInfo;

            if (prLogModuleInfo) {
                char * nodeInfoCStr = ToNewCString(nodeName);
                PR_LOG(prLogModuleInfo, 4,
                       ("attribute[%d] %s", i,(const char *)nodeInfoCStr));
                nsMemory::Free(nodeInfoCStr);
            }
            jNodeName = ::util_NewString(env, nodeName.get(),
                                         nodeName.Length());
            jNodeValue = ::util_NewString(env, nodeValue.get(),
                                          nodeValue.Length());

            util_StoreIntoPropertiesObject(env, (jobject) curThis->properties,
                                           (jobject) jNodeName,
                                           (jobject) jNodeValue,
                                           (jobject)
                                           &(curThis->mInitContext->shareContext));
            if (jNodeName) {
                ::util_DeleteString(env, jNodeName);
            }
            if (jNodeValue) {
                ::util_DeleteString(env, jNodeValue);
            }
        }
    } // end of storing the attributes

    return rv;
}

//
// Local functions
//

