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
 */



#include <limits.h>
#include "CBrowserContainer.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowser.h"
#include "nsIDOMNamedNodeMap.h"

#include "prprf.h" // for PR_snprintf
#include "nsFileSpec.h" // for nsAutoCString

#include "dom_util.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_BEOS)

#define WC_ITOA(intVal, buf, radix) sprintf(buf, "%d", intVal)
#else
#define WC_ITOA(intVal, buf, radix) itoa(intVal, buf, radix)
#endif


CBrowserContainer::CBrowserContainer(nsIWebBrowser *pOwner, JNIEnv *env,
                                     WebShellInitContext *yourInitContext) :
    m_pOwner(pOwner), mJNIEnv(env), mInitContext(yourInitContext), 
    mDocTarget(nsnull), mMouseTarget(nsnull), mDomEventTarget(nsnull), 
    inverseDepth(-1), properties(nsnull), currentDOMEvent(nsnull)
{
  	NS_INIT_REFCNT();
    // initialize the string constants (including properties keys)
    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants(env);
    }
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
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
    if (properties) {
        ::util_DeleteGlobalRef(env, properties);
    }
    properties = nsnull;
    currentDOMEvent = nsnull;
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CBrowserContainer)
NS_IMPL_RELEASE(CBrowserContainer)

NS_INTERFACE_MAP_BEGIN(CBrowserContainer)
  //  	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
	NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
	NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
	NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
	NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
	NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
	NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
	NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
	NS_INTERFACE_MAP_ENTRY(nsIPrompt)
	NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
	NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
	NS_INTERFACE_MAP_ENTRY(wcIBrowserContainer)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CBrowserContainer::GetInterface(const nsIID & uuid, void * *result)
{
	const nsIID &iid = NS_GET_IID(nsIPrompt);
	if (memcmp(&uuid, &iid, sizeof(nsIID)) == 0)
	{
		*result = (nsIPrompt *) this;
		AddRef();
		return NS_OK;
	}
    return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIPrompt

/* void alert (in wstring text); */
NS_IMETHODIMP CBrowserContainer::Alert(const PRUnichar *dialogTitle, 
                                       const PRUnichar *text)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean confirm (in wstring text); */
NS_IMETHODIMP CBrowserContainer::Confirm(const PRUnichar *dialogTitle,
                                         const PRUnichar *text, 
                                         PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean confirmCheck (in wstring text, in wstring checkMsg, out boolean checkValue); */
NS_IMETHODIMP CBrowserContainer::ConfirmCheck(const PRUnichar *dialogTitle,
                                              const PRUnichar *text, 
                                              const PRUnichar *checkMsg, 
                                              PRBool *checkValue, 
                                              PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean prompt (in wstring text, in wstring defaultText, out wstring result); */
NS_IMETHODIMP CBrowserContainer::Prompt(const PRUnichar *dialogTitle,
                                        const PRUnichar *text, 
                                        const PRUnichar* passwordRealm,
                                        const PRUnichar* defaultText,
                                        PRUnichar **result, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptUsernameAndPassword (in wstring text, out wstring user, out wstring pwd); */
NS_IMETHODIMP CBrowserContainer::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                                           const PRUnichar *text, const PRUnichar *passwordRealm, PRBool persistPassword, 
                                                           PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptPassword (in wstring text, in wstring title, out wstring pwd); */
NS_IMETHODIMP CBrowserContainer::PromptPassword(const PRUnichar *dialogTitle,
                                                const PRUnichar *text, 
                                                const PRUnichar* passwordRealm,
                                                PRBool persistPassword, 
                                                PRUnichar **pwd, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean select (in wstring inDialogTitle, in wstring inMsg, in PRUint32 inCount, [array, size_is (inCount)] in wstring inList, out long outSelection); */
NS_IMETHODIMP CBrowserContainer::Select(const PRUnichar *inDialogTitle, const PRUnichar *inMsg, PRUint32 inCount, const PRUnichar **inList, PRInt32 *outSelection, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void universalDialog (in wstring inTitleMessage, in wstring inDialogTitle, in wstring inMsg, in wstring inCheckboxMsg, in wstring inButton0Text, in wstring inButton1Text, in wstring inButton2Text, in wstring inButton3Text, in wstring inEditfield1Msg, in wstring inEditfield2Msg, inout wstring inoutEditfield1Value, inout wstring inoutEditfield2Value, in wstring inIConURL, inout boolean inoutCheckboxState, in PRInt32 inNumberButtons, in PRInt32 inNumberEditfields, in PRInt32 inEditField1Password, out PRInt32 outButtonPressed); */
NS_IMETHODIMP CBrowserContainer::UniversalDialog(const PRUnichar *inTitleMessage, const PRUnichar *inDialogTitle, const PRUnichar *inMsg, const PRUnichar *inCheckboxMsg, const PRUnichar *inButton0Text, const PRUnichar *inButton1Text, const PRUnichar *inButton2Text, const PRUnichar *inButton3Text, const PRUnichar *inEditfield1Msg, const PRUnichar *inEditfield2Msg, PRUnichar **inoutEditfield1Value, PRUnichar **inoutEditfield2Value, const PRUnichar *inIConURL, PRBool *inoutCheckboxState, PRInt32 inNumberButtons, PRInt32 inNumberEditfields, PRInt32 inEditField1Password, PRInt32 *outButtonPressed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

/* void onProgressChange (in nsIChannel channel, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP CBrowserContainer::OnProgressChange(nsIChannel *channel, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
  //	NG_TRACE(_T("CBrowserContainer::OnProgressChange(...)\n"));
	
	long nProgress = curTotalProgress;
	long nProgressMax = maxTotalProgress;

	if (nProgress == 0)
	{
	}
	if (nProgressMax == 0)
	{
		nProgressMax = LONG_MAX;
	}
	if (nProgress > nProgressMax)
	{
		nProgress = nProgressMax; // Progress complete
	}

    //PENDING (Ashu)
    // Code to indicate Status Change goes here

    return NS_OK;
}


/* void onChildProgressChange (in nsIChannel channel, in long curChildProgress, in long maxChildProgress); */
NS_IMETHODIMP CBrowserContainer::OnChildProgressChange(nsIChannel *channel, PRInt32 curChildProgress, PRInt32 maxChildProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void onStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP CBrowserContainer::OnStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{

    return NS_OK;
}


/* void onChildStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP CBrowserContainer::OnChildStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP CBrowserContainer::OnLocationChange(nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

/* void onStartURIOpen (in nsIURI aURI, in string aWindowTarget, out boolean aAbortOpen); */
NS_IMETHODIMP CBrowserContainer::OnStartURIOpen(nsIURI *pURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getProtocolHandler (in nsIURI aURI, out nsIProtocolHandler aProtocolHandler); */
NS_IMETHODIMP CBrowserContainer::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void doContent (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, in nsIChannel aOpenedChannel, out nsIStreamListener aContentHandler, out boolean aAbortProcess); */
NS_IMETHODIMP CBrowserContainer::DoContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, nsIChannel *aOpenedChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean isPreferred (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, out string aDesiredContentType); */
NS_IMETHODIMP CBrowserContainer::IsPreferred(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean canHandleContent (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, out string aDesiredContentType); */
NS_IMETHODIMP CBrowserContainer::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, char **aDesiredContentType, PRBool *_retval)
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
// nsIDocShellTreeOwner


NS_IMETHODIMP
CBrowserContainer::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
	return NS_ERROR_FAILURE;

}


NS_IMETHODIMP
CBrowserContainer::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
	return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
	NS_ERROR("Haven't Implemented this yet");
	*aShell = nsnull;
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::SizeShellTo(nsIDocShellTreeItem* aShell,
   PRInt32 aCX, PRInt32 aCY)
{
	// Ignore request to be resized
	return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::ShowModal()
{
	// Ignore request to be shown modally
	return NS_OK;
}


NS_IMETHODIMP CBrowserContainer::GetNewWindow(PRInt32 aChromeFlags, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{

    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIBaseWindow

NS_IMETHODIMP 
CBrowserContainer::InitWindow(nativeWindow parentNativeWindow, nsIWidget * parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::Create(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::Destroy(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetPosition(PRInt32 x, PRInt32 y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetPosition(PRInt32 *x, PRInt32 *y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetSize(PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::Repaint(PRBool force)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetParentWidget(nsIWidget * *aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetParentWidget(nsIWidget * aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetVisibility(PRBool *aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetVisibility(PRBool aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetMainWidget(nsIWidget * *aMainWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetFocus(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::FocusAvailable(nsIBaseWindow *aCurrentFocus, PRBool *aTookFocus)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::GetTitle(PRUnichar * *aTitle)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CBrowserContainer::SetTitle(const PRUnichar * aTitle)
{
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CBrowserContainer::SetJSStatus(const PRUnichar *status)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::SetJSDefaultStatus(const PRUnichar *status)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::SetOverLink(const PRUnichar *link)
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
CBrowserContainer::GetChromeMask(PRUint32 *aChromeMask)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::SetChromeMask(PRUint32 aChromeMask)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::GetNewBrowser(PRUint32 chromeMask, nsIWebBrowser **_retval)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CBrowserContainer::FindNamedBrowserItem(const PRUnichar *aName, nsIDocShellTreeItem **_retval)
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


///////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver implementation


NS_IMETHODIMP
CBrowserContainer::OnStartRequest(nsIChannel* aChannel, nsISupports* aContext)
{

	return NS_OK;
}


NS_IMETHODIMP
CBrowserContainer::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext, nsresult aStatus, const PRUnichar* aMsg)
{
	return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocumentLoaderObserver implementation 


NS_IMETHODIMP
CBrowserContainer::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL,
                                       const char* aCommand)
{
    // remove the old mouse listener for the old document
    if (mDomEventTarget) {
        nsCAutoString eType("mouseover");
        PRUnichar *eTypeUni = eType.ToNewUnicode();
        mDomEventTarget->RemoveEventListener(eTypeUni, this, PR_FALSE);
        nsCRT::free(eTypeUni);
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
    char *urlStr = nsnull;
    jobject urlJStr = nsnull;
    if (nsnull != aURL) {

        // IMPORTANT: do not use initContext->env here since it comes
        // from another thread.  Use JNU_GetEnv instead.

        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
        
        aURL->GetSpec(&urlStr);
        if (nsnull != urlStr) {
            urlJStr = (jobject) ::util_NewStringUTF(env, urlStr);
            ::Recycle(urlStr);
        }
    }  

    // maskValues array comes from ../src_share/jni_util.h
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[START_DOCUMENT_LOAD_EVENT_MASK], 
                         urlJStr);
    

    if (urlJStr) {
        ::util_DeleteStringUTF(mInitContext->env, (jstring) urlJStr);
    }

	return NS_OK; 
} 


// we need this to fire the document complete 
NS_IMETHODIMP
CBrowserContainer::OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel *aChannel, nsresult aStatus)
{
    if (nsnull != loader) {
        if (mInitContext->currentDocument = dom_getDocumentFromLoader(loader)){
            // if we have a mouse listener
            if (mMouseTarget) {
                // turn the currentDocument into an nsIDOMEventTarget
                mDomEventTarget = 
                    do_QueryInterface(mInitContext->currentDocument);
                // if successful
                if (mDomEventTarget) {
                    nsCAutoString eType("mouseover");
                    PRUnichar *eTypeUni = eType.ToNewUnicode();
                    mDomEventTarget->AddEventListener(eTypeUni, this, 
                                                      PR_FALSE);
                    nsCRT::free(eTypeUni);
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


NS_IMETHODIMP
CBrowserContainer::OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel)
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
                         DocumentLoader_maskValues[START_URL_LOAD_EVENT_MASK], 
                         nsnull);
    
	return NS_OK; 
} 


NS_IMETHODIMP
CBrowserContainer::OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel, PRUint32 aProgress, PRUint32 aProgressMax)
{ 
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnProgressURLLoad\n"));
    }
#endif

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mDocTarget, 
                         DOCUMENT_LOAD_LISTENER_CLASSNAME,
                         DocumentLoader_maskValues[PROGRESS_URL_LOAD_EVENT_MASK], 
                         nsnull);
    

	return NS_OK;
} 


NS_IMETHODIMP
CBrowserContainer::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                   nsIChannel* channel, nsString& aMsg)
{ 

	//NOTE: This appears to get fired for each individual item on a page, indicating the status of that item.
    if (!mDocTarget) {
        return NS_OK;
    }
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 4, 
               ("CBrowserContainer: OnStatusURLLoad\n"));
    }
#endif
    int length = aMsg.Length();

    // IMPORTANT: do not use initContext->env here since it comes
    // from another thread.  Use JNU_GetEnv instead.
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
    jstring statusMessage = ::util_NewString(env, (const jchar *) 
                                             aMsg.GetUnicode(), length);
    
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


NS_IMETHODIMP
CBrowserContainer::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
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


nsresult
CBrowserContainer::HandleEvent(nsIDOMEvent* aEvent)
{
	return NS_OK; 
}


nsresult
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


nsresult
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

nsresult
CBrowserContainer::MouseClick(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
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


nsresult
CBrowserContainer::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
    if (!mMouseTarget) {
        return NS_OK;
    }
    PR_ASSERT(nsnull != aMouseEvent);
    
    getPropertiesFromEvent(aMouseEvent);

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
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


nsresult
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


nsresult
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

NS_IMETHODIMP CBrowserContainer::RemoveMouseListener()
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }


    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2), mMouseTarget);
    mMouseTarget = nsnull;
    return rv;
}

NS_IMETHODIMP CBrowserContainer::RemoveDocumentLoadListener()
{
    nsresult rv = NS_OK;
    if (!mInitContext->initComplete) {
        return NS_ERROR_FAILURE;
    }

    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2), mDocTarget);
    mDocTarget = nsnull;
    return rv;
}



NS_IMETHODIMP CBrowserContainer::RemoveAllListeners()
{
    if (mDomEventTarget) {
        nsCAutoString eType("mouseover");
        PRUnichar *eTypeUni = eType.ToNewUnicode();
        mDomEventTarget->RemoveEventListener(eTypeUni, this, PR_FALSE);
        nsCRT::free(eTypeUni);
        mDomEventTarget = nsnull;
    }   

    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2), mDocTarget);
    ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2), mMouseTarget);

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
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);

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
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
    
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
    const PRUint32 depthStrLen = 20;
    //    char depthStr[depthStrLen];
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);

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
            nsAutoCString nodeInfoCStr(nodeName);
            PR_LOG(prLogModuleInfo, 4, ("%s", (const char *)nodeInfoCStr));
        }
        
        rv = currentNode->GetNodeValue(nodeInfo);
        if (NS_FAILED(rv)) {
            return rv;
        }
        //        nodeValue = nodeDepth;
        //        nodeValue += nodeInfo;
        nodeValue = nodeInfo;
        
        if (prLogModuleInfo) {
            nsAutoCString nodeInfoCStr(nodeValue);
            PR_LOG(prLogModuleInfo, 4, ("%s", (const char *)nodeInfoCStr));
        }
        
        jNodeName = ::util_NewString(env, nodeName.GetUnicode(), 
                                     nodeName.Length());
        jNodeValue = ::util_NewString(env, nodeValue.GetUnicode(), 
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
                nsAutoCString nodeInfoCStr(nodeName);
                PR_LOG(prLogModuleInfo, 4, 
                       ("attribute[%d], %s", i, (const char *)nodeInfoCStr));
            }
            
            rv = currentNode->GetNodeValue(nodeInfo);
            if (NS_FAILED(rv)) {
                return rv;
            }
            //            nodeValue = nodeDepth;
            //            nodeValue += nodeInfo;
            nodeValue = nodeInfo;
            
            if (prLogModuleInfo) {
                nsAutoCString nodeInfoCStr(nodeValue);
                PR_LOG(prLogModuleInfo, 4, 
                       ("attribute[%d] %s", i,(const char *)nodeInfoCStr));
            }
            jNodeName = ::util_NewString(env, nodeName.GetUnicode(), 
                                         nodeName.Length());
            jNodeValue = ::util_NewString(env, nodeValue.GetUnicode(), 
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


