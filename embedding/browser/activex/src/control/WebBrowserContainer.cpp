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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */

#include "stdafx.h"

#include <limits.h>
#include <shlobj.h>

#include "WebBrowserContainer.h"

CWebBrowserContainer::CWebBrowserContainer(CMozillaBrowser *pOwner)
{
	NS_INIT_REFCNT();
	m_pOwner = pOwner;
	m_pEvents1 = m_pOwner;
	m_pEvents2 = m_pOwner;
	m_pCurrentURI = nsnull;
}


CWebBrowserContainer::~CWebBrowserContainer()
{
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CWebBrowserContainer)
NS_IMPL_RELEASE(CWebBrowserContainer)

NS_INTERFACE_MAP_BEGIN(CWebBrowserContainer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
	NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
	NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
	NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
	NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
	NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
	NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
	NS_INTERFACE_MAP_ENTRY(nsIPrompt)
    NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CWebBrowserContainer::GetInterface(const nsIID & uuid, void * *result)
{
	const nsIID &iid = NS_GET_IID(nsIPrompt);
	if (memcmp(&uuid, &iid, sizeof(nsIID)) == 0)
	{
		*result = (nsIPrompt *) this;
		AddRef();
		return NS_OK;
	}
    return QueryInterface(uuid, result);
}


///////////////////////////////////////////////////////////////////////////////
// nsIContextMenuListener

NS_IMETHODIMP CWebBrowserContainer::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    m_pOwner->ShowContextMenu(aContextFlags);
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPrompt

/* void alert (in wstring text); */
NS_IMETHODIMP CWebBrowserContainer::Alert(const PRUnichar* dialogTitle, const PRUnichar *text)
{
	USES_CONVERSION;
	m_pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);
    return NS_OK;
}

/* boolean confirm (in wstring text); */
NS_IMETHODIMP CWebBrowserContainer::Confirm(const PRUnichar* dialogTitle, const PRUnichar *text, PRBool *_retval)
{
	USES_CONVERSION;
	int nAnswer = m_pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_YESNO | MB_ICONQUESTION);
	*_retval = (nAnswer == IDYES) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}

/* boolean confirmCheck (in wstring text, in wstring checkMsg, out boolean checkValue); */
NS_IMETHODIMP CWebBrowserContainer::ConfirmCheck(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
	USES_CONVERSION;

	// TODO show dialog with check box

	int nAnswer = m_pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_YESNO | MB_ICONQUESTION);
	*_retval = (nAnswer == IDYES) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}

/* boolean prompt (in wstring text, in wstring defaultText, out wstring result); */
NS_IMETHODIMP CWebBrowserContainer::Prompt(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar* passwordRealm,
                                           PRUint32 savePassword, const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
    // TODO show dialog with entry field
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptUsernameAndPassword (in wstring text, out wstring user, out wstring pwd); */
NS_IMETHODIMP CWebBrowserContainer::PromptUsernameAndPassword(const PRUnichar* dialogTitle, const PRUnichar *text, 
                                                              const PRUnichar* passwordRealm, PRUint32 savePassword,
                                                              PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
    // TODO show dialog with entry field and password field
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptPassword (in wstring text, in wstring title, out wstring pwd); */
NS_IMETHODIMP CWebBrowserContainer::PromptPassword(const PRUnichar* dialogTitle, const PRUnichar *text, 
                                                   const PRUnichar* passwordRealm, PRUint32 savePassword,
                                                   PRUnichar **pwd, PRBool *_retval)
{
    // TODO show dialog with password field
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean select (in wstring inDialogTitle, in wstring inMsg, in PRUint32 inCount, [array, size_is (inCount)] in wstring inList, out long outSelection); */
NS_IMETHODIMP CWebBrowserContainer::Select(const PRUnichar *inDialogTitle, const PRUnichar *inMsg, PRUint32 inCount, const PRUnichar **inList, PRInt32 *outSelection, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void universalDialog (in wstring inTitleMessage, in wstring inDialogTitle, in wstring inMsg, in wstring inCheckboxMsg, in wstring inButton0Text, in wstring inButton1Text, in wstring inButton2Text, in wstring inButton3Text, in wstring inEditfield1Msg, in wstring inEditfield2Msg, inout wstring inoutEditfield1Value, inout wstring inoutEditfield2Value, in wstring inIConURL, inout boolean inoutCheckboxState, in PRInt32 inNumberButtons, in PRInt32 inNumberEditfields, in PRInt32 inEditField1Password, out PRInt32 outButtonPressed); */
NS_IMETHODIMP CWebBrowserContainer::UniversalDialog(const PRUnichar *inTitleMessage, const PRUnichar *inDialogTitle, const PRUnichar *inMsg, const PRUnichar *inCheckboxMsg, const PRUnichar *inButton0Text, const PRUnichar *inButton1Text, const PRUnichar *inButton2Text, const PRUnichar *inButton3Text, const PRUnichar *inEditfield1Msg, const PRUnichar *inEditfield2Msg, PRUnichar **inoutEditfield1Value, PRUnichar **inoutEditfield2Value, const PRUnichar *inIConURL, PRBool *inoutCheckboxState, PRInt32 inNumberButtons, PRInt32 inNumberEditfields, PRInt32 inEditField1Password, PRInt32 *outButtonPressed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

/* void onProgressChange (in nsIWebProgress aProgress, in nsIRequest aRequest, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP CWebBrowserContainer::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
	NG_TRACE(_T("CWebBrowserContainer::OnProgressChange(...)\n"));
	
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

	m_pEvents1->Fire_ProgressChange(nProgress, nProgressMax);
	m_pEvents2->Fire_ProgressChange(nProgress, nProgressMax);

    return NS_OK;
}




/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest request, in long progressStateFlags, in unsinged long aStatus); */
NS_IMETHODIMP CWebBrowserContainer::OnStateChange(nsIWebProgress* aWebProgress, nsIRequest *aRequest, PRInt32 progressStateFlags, nsresult aStatus)
{
	NG_TRACE(_T("CWebBrowserContainer::OnStateChange(...)\n"));

    if (progressStateFlags & flag_is_network)
    {

    	if (progressStateFlags &  flag_start)
	    {
    		// TODO 
    	}

    	if (progressStateFlags & flag_stop)
    	{
	    	nsXPIDLCString aURI;
		    if (m_pCurrentURI)
    		{
    			m_pCurrentURI->GetSpec(getter_Copies(aURI));
    		}

    		// Fire a NavigateComplete event
    		USES_CONVERSION;
    		BSTR bstrURI = SysAllocString(A2OLE((const char *) aURI));
    		m_pEvents1->Fire_NavigateComplete(bstrURI);

    		// Fire a NavigateComplete2 event
    		CComVariant vURI(bstrURI);
    		m_pEvents2->Fire_NavigateComplete2(m_pOwner, &vURI);

    		// Cleanup
    		SysFreeString(bstrURI);

    		nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(m_pOwner->mWebBrowser));

    		// Fire the new NavigateForward state
    		VARIANT_BOOL bEnableForward = VARIANT_FALSE;
    		PRBool aCanGoForward = PR_FALSE;
    		webNav->GetCanGoForward(&aCanGoForward);
    		if (aCanGoForward == PR_TRUE)
    		{
    			bEnableForward = VARIANT_TRUE;
    		}
    		m_pEvents2->Fire_CommandStateChange(CSC_NAVIGATEFORWARD, bEnableForward);

    		// Fire the new NavigateBack state
    		VARIANT_BOOL bEnableBack = VARIANT_FALSE;
    		PRBool aCanGoBack = PR_FALSE;
    		webNav->GetCanGoBack(&aCanGoBack);
    		if (aCanGoBack == PR_TRUE)
    		{
    			bEnableBack = VARIANT_TRUE;
    		}
    		m_pEvents2->Fire_CommandStateChange(CSC_NAVIGATEBACK, bEnableBack);

    		m_pOwner->mBusyFlag = FALSE;

    		if (m_pCurrentURI)
    		{
    			NS_RELEASE(m_pCurrentURI);
    		}
    	}
    }

    return NS_OK;
}


/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP CWebBrowserContainer::OnLocationChange(nsIWebProgress* aWebProgress,
                                                     nsIRequest* aRequest,
                                                     nsIURI *location)
{
//	nsXPIDLCString aPath;
//	location->GetPath(getter_Copies(aPath));
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
CWebBrowserContainer::OnStatusChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     nsresult aStatus,
                                     const PRUnichar* aMessage)
{
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

/* void onStartURIOpen (in nsIURI aURI, in string aWindowTarget, out boolean aAbortOpen); */
NS_IMETHODIMP CWebBrowserContainer::OnStartURIOpen(nsIURI *pURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebBrowserContainer::OnStartURIOpen(...)\n"));

	if (m_pCurrentURI)
	{
		NS_RELEASE(m_pCurrentURI);
	}
	m_pCurrentURI = pURI;
	NG_ASSERT(m_pCurrentURI);
	m_pCurrentURI->AddRef();

	nsXPIDLCString aURI;
	m_pCurrentURI->GetSpec(getter_Copies(aURI));

	// Setup the post data
	CComVariant vPostDataRef;
	CComVariant vPostData;
	vPostDataRef.vt = VT_BYREF | VT_VARIANT;
	vPostDataRef.pvarVal = &vPostData;
	// TODO get the post data passed in via the original call to Navigate()

	// Fire a BeforeNavigate event
	BSTR bstrURI = SysAllocString(A2OLE((const char *)aURI));
	BSTR bstrTargetFrameName = NULL;
	BSTR bstrHeaders = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	long lFlags = 0;

	m_pEvents1->Fire_BeforeNavigate(bstrURI, lFlags, bstrTargetFrameName, &vPostDataRef, bstrHeaders, &bCancel);

	// Fire a BeforeNavigate2 event
	CComVariant vURI(bstrURI);
	CComVariant vFlags(lFlags);
	CComVariant vTargetFrameName(bstrTargetFrameName);
	CComVariant vHeaders(bstrHeaders);

	m_pEvents2->Fire_BeforeNavigate2(m_pOwner, &vURI, &vFlags, &vTargetFrameName, &vPostDataRef, &vHeaders, &bCancel);

	// Cleanup
	SysFreeString(bstrURI);
	SysFreeString(bstrTargetFrameName);
	SysFreeString(bstrHeaders);

	if (bCancel == VARIANT_TRUE)
	{
		*aAbortOpen = PR_TRUE;
		return NS_ERROR_ABORT;
	}
	else
	{
		m_pOwner->mBusyFlag = TRUE;
	}

	//NOTE:	The IE control fires a DownloadBegin after the first BeforeNavigate.
    //      It then fires a DownloadComplete after the engine has made it's
    //      initial connection to the server.  It then fires a second
    //      DownloadBegin/DownloadComplete pair around the loading of
    //      everything on the page.  These events get fired out of
    //      CWebBrowserContainer::StartDocumentLoad() and
    //      CWebBrowserContainer::EndDocumentLoad().
	//		We don't appear to distinguish between the initial connection to
    //      the server and the actual transfer of data.  Firing these events
    //      here simulates, appeasing applications that are expecting that
    //      initial pair.

	m_pEvents1->Fire_DownloadBegin();
	m_pEvents2->Fire_DownloadBegin();
	m_pEvents1->Fire_DownloadComplete();
	m_pEvents2->Fire_DownloadComplete();

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getProtocolHandler (in nsIURI aURI, out nsIProtocolHandler aProtocolHandler); */
NS_IMETHODIMP CWebBrowserContainer::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void doContent (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, in nsIChannel aOpenedChannel, out nsIStreamListener aContentHandler, out boolean aAbortProcess); */
NS_IMETHODIMP CWebBrowserContainer::DoContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, nsIChannel *aOpenedChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean isPreferred (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, out string aDesiredContentType); */
NS_IMETHODIMP CWebBrowserContainer::IsPreferred(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean canHandleContent (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, out string aDesiredContentType); */
NS_IMETHODIMP CWebBrowserContainer::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsISupports loadCookie; */
NS_IMETHODIMP CWebBrowserContainer::GetLoadCookie(nsISupports * *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CWebBrowserContainer::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP CWebBrowserContainer::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CWebBrowserContainer::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocShellTreeOwner


NS_IMETHODIMP
CWebBrowserContainer::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
	return NS_ERROR_FAILURE;
/*
	nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(m_pOwner->mWebBrowser));
	NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
	return docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome*, this), aFoundItem);
	*/
}

static nsIDocShellTreeItem* contentShell = nsnull; 
NS_IMETHODIMP
CWebBrowserContainer::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
    if (aPrimary)
        contentShell = aContentShell;
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
	// NS_ERROR("Haven't Implemented this yet");
    NS_IF_ADDREF(contentShell);
	*aShell = contentShell;
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::SizeShellTo(nsIDocShellTreeItem* aShell,
   PRInt32 aCX, PRInt32 aCY)
{
	// Ignore request to be resized
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::ShowModal()
{
	// Ignore request to be shown modally
	return NS_OK;
}

NS_IMETHODIMP
CWebBrowserContainer::ExitModalLoop(nsresult aStatus)
{
	// Ignore request to exit modal loop
	return NS_OK;
}

NS_IMETHODIMP CWebBrowserContainer::GetNewWindow(PRInt32 aChromeFlags, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{
	IDispatch *pDispNew = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	
	// Test if the event sink can give us a new window to navigate into
	m_pEvents2->Fire_NewWindow2(&pDispNew, &bCancel);

	if ((bCancel == VARIANT_FALSE) && pDispNew)
	{
		CMozillaBrowser *pBrowser = (CMozillaBrowser *) pDispNew;

		nsIDocShell *docShell;
		pBrowser->mWebBrowser->GetDocShell(&docShell);
		docShell->QueryInterface(NS_GET_IID(nsIDocShellTreeItem), (void **) aDocShellTreeItem);
		docShell->Release();
		pDispNew->Release();
		return NS_OK;
	}

	*aDocShellTreeItem = nsnull;
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIBaseWindow

NS_IMETHODIMP 
CWebBrowserContainer::InitWindow(nativeWindow parentNativeWindow, nsIWidget * parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::Create(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::Destroy(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetPosition(PRInt32 x, PRInt32 y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetPosition(PRInt32 *x, PRInt32 *y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetSize(PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::Repaint(PRBool force)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetParentWidget(nsIWidget * *aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetParentWidget(nsIWidget * aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetVisibility(PRBool *aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetVisibility(PRBool aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetMainWidget(nsIWidget * *aMainWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetFocus(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::FocusAvailable(nsIBaseWindow *aCurrentFocus, PRBool *aTookFocus)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetTitle(PRUnichar * *aTitle)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetTitle(const PRUnichar * aTitle)
{
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CWebBrowserContainer::SetJSStatus(const PRUnichar *status)
{
	//Fire a StatusTextChange event
	BSTR bstrStatus = SysAllocString(status);
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);
    SysFreeString(bstrStatus);
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::SetJSDefaultStatus(const PRUnichar *status)
{
	//Fire a StatusTextChange event
	BSTR bstrStatus = SysAllocString(status);
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);
    SysFreeString(bstrStatus);
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::SetOverLink(const PRUnichar *link)
{
	//Fire a StatusTextChange event
	BSTR bstrStatus = SysAllocString(link);
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);
    SysFreeString(bstrStatus);
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::GetChromeMask(PRUint32 *aChromeMask)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SetChromeMask(PRUint32 aChromeMask)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::GetNewBrowser(PRUint32 chromeMask, nsIWebBrowser **_retval)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::FindNamedBrowserItem(const PRUnichar *aName, nsIDocShellTreeItem **_retval)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::ShowAsModal(void)
{
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebBrowserContainer::ExitModalEventLoop(nsresult aStatus)
{
	// Ignore request to exit modal loop
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                     PRBool aPersistCX, PRBool aPersistCY,
                                     PRBool aPersistSizeMode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CWebBrowserContainer::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                     PRBool* aPersistCX, PRBool* aPersistCY,
                                     PRBool* aPersistSizeMode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver implementation


NS_IMETHODIMP
CWebBrowserContainer::OnStartRequest(nsIChannel* aChannel, nsISupports* aContext)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebBrowserContainer::OnStartRequest(...)\n"));

	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext, nsresult aStatus, const PRUnichar* aMsg)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebBrowserContainer::OnStopRequest(..., %d, \"%s\")\n"), (int) aStatus, W2T((PRUnichar *) aMsg));

	// Fire a DownloadComplete event
	m_pEvents1->Fire_DownloadComplete();
	m_pEvents2->Fire_DownloadComplete();

	return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocumentLoaderObserver implementation 


NS_IMETHODIMP
CWebBrowserContainer::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* pURI, const char* aCommand)
{
	nsXPIDLCString aURI;
	pURI->GetSpec(getter_Copies(aURI));
	NG_TRACE(_T("CWebBrowserContainer::OnStartDocumentLoad(..., %s, \"%s\")\n"), A2CT(aURI), A2CT(aCommand));

	//Fire a DownloadBegin
	m_pEvents1->Fire_DownloadBegin();
	m_pEvents2->Fire_DownloadBegin();

	return NS_OK; 
} 


// we need this to fire the document complete 
NS_IMETHODIMP
CWebBrowserContainer::OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel *aChannel, nsresult aStatus)
{
	NG_TRACE(_T("CWebBrowserContainer::OnEndDocumentLoad(...,  \"\")\n"));

    if (m_pOwner->mIERootDocument)
    {
        // allow to keep old document around
        m_pOwner->mIERootDocument->Release();
        m_pOwner->mIERootDocument = NULL;
    }

	//Fire a DownloadComplete
	m_pEvents1->Fire_DownloadComplete();
	m_pEvents2->Fire_DownloadComplete();

	char* aString = nsnull;    
    nsIURI* pURI = nsnull;

    aChannel->GetURI(&pURI);
	if (pURI == nsnull)
	{
		return NS_ERROR_NULL_POINTER;
	}
	nsXPIDLCString aURI;
	pURI->GetSpec(getter_Copies(aURI));
	NS_RELEASE(pURI);

	USES_CONVERSION;
	BSTR bstrURI = SysAllocString(A2OLE((const char *) aURI)); 
		
	// Fire a DocumentComplete event
	CComVariant vURI(bstrURI);
	m_pEvents2->Fire_DocumentComplete(m_pOwner, &vURI);
	SysFreeString(bstrURI);

	//Fire a StatusTextChange event
	BSTR bstrStatus = SysAllocString(A2OLE((CHAR *) "Done"));
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);
    SysFreeString(bstrStatus);

	return NS_OK; 
} 


NS_IMETHODIMP
CWebBrowserContainer::OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel)
{ 
	NG_TRACE(_T("CWebBrowserContainer::OnStartURLLoad(...,  \"\")\n"));

	//NOTE: This appears to get fired once for each individual item on a page.

	return NS_OK; 
} 


NS_IMETHODIMP
CWebBrowserContainer::OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel, PRUint32 aProgress, PRUint32 aProgressMax)
{ 
	USES_CONVERSION;
	NG_TRACE(_T("CWebBrowserContainer::OnProgress(..., \"%d\", \"%d\")\n"), (int) aProgress, (int) aProgressMax);

	return NS_OK;
} 


NS_IMETHODIMP
CWebBrowserContainer::OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel, nsString& aMsg)
{ 
	NG_TRACE(_T("CWebBrowserContainer::OnStatusURLLoad(...,  \"\")\n"));
	
	//NOTE: This appears to get fired for each individual item on a page, indicating the status of that item.
	BSTR bstrStatus = SysAllocString(W2OLE((PRUnichar *) aMsg.GetUnicode()));
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);
    SysFreeString(bstrStatus);
	
	return NS_OK; 
} 


NS_IMETHODIMP
CWebBrowserContainer::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
{
	NG_TRACE(_T("CWebBrowserContainer::OnEndURLLoad(...,  \"\")\n"));

	//NOTE: This appears to get fired once for each individual item on a page.

	return NS_OK; 
} 


