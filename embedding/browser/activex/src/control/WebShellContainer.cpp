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
 * Contributor(s): 
 */

#include "stdafx.h"

#include <limits.h>
#include <shlobj.h>

#include "WebShellContainer.h"

CWebShellContainer::CWebShellContainer(CMozillaBrowser *pOwner)
{
	NS_INIT_REFCNT();
	m_pOwner = pOwner;
	m_pEvents1 = m_pOwner;
	m_pEvents2 = m_pOwner;
	m_pCurrentURI = nsnull;
}


CWebShellContainer::~CWebShellContainer()
{
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation


NS_IMPL_ADDREF(CWebShellContainer)
NS_IMPL_RELEASE(CWebShellContainer)

NS_INTERFACE_MAP_BEGIN(CWebShellContainer)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
	NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
	NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
	NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
	NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
	NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
	NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CWebShellContainer::GetInterface(const nsIID & uuid, void * *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

/* void onProgressChange (in nsIChannel channel, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP CWebShellContainer::OnProgressChange(nsIChannel *channel, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
	NG_TRACE(_T("CWebShellContainer::OnProgressChange(...)\n"));
	
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


/* void onChildProgressChange (in nsIChannel channel, in long curChildProgress, in long maxChildProgress); */
NS_IMETHODIMP CWebShellContainer::OnChildProgressChange(nsIChannel *channel, PRInt32 curChildProgress, PRInt32 maxChildProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void onStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP CWebShellContainer::OnStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{
	NG_TRACE(_T("CWebShellContainer::OnStatusChange(...)\n"));

	if (progressStatusFlags & nsIWebProgress::flag_net_start)
	{
		// TODO 
	}

	if (progressStatusFlags & nsIWebProgress::flag_net_stop)
	{
		NG_ASSERT(m_pCurrentURI);
		nsXPIDLCString aURI;
		m_pCurrentURI->GetSpec(getter_Copies(aURI));

		// Fire a NavigateComplete event
		USES_CONVERSION;
		BSTR bstrURI = SysAllocString(A2OLE((const char *) aURI));
		m_pEvents1->Fire_NavigateComplete(bstrURI);

		// Fire a NavigateComplete2 event
		CComVariant vURI(bstrURI);
		m_pEvents2->Fire_NavigateComplete2(m_pOwner, &vURI);

		// Cleanup
		SysFreeString(bstrURI);

		nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(m_pOwner->m_pIWebBrowser));

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

		m_pOwner->m_bBusy = FALSE;

		NS_RELEASE(m_pCurrentURI);
	}

    return NS_OK;
}


/* void onChildStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP CWebShellContainer::OnChildStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP CWebShellContainer::OnLocationChange(nsIURI *location)
{
//	nsXPIDLCString aPath;
//	location->GetPath(getter_Copies(aPath));
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

/* void onStartURIOpen (in nsIURI aURI, in string aWindowTarget, out boolean aAbortOpen); */
NS_IMETHODIMP CWebShellContainer::OnStartURIOpen(nsIURI *pURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStartURIOpen(...)\n"));

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
		m_pOwner->m_bBusy = TRUE;
	}

	//NOTE:	The IE control fires a DownloadBegin after the first BeforeNavigate.  It then fires a 
	//		DownloadComplete after the engine has made it's initial connection to the server.  It
	//		then fires a second DownloadBegin/DownloadComplete pair around the loading of everything
	//		on the page.  These events get fired out of CWebShellContainer::StartDocumentLoad() and
	//		CWebShellContainer::EndDocumentLoad().
	//		We don't appear to distinguish between the initial connection to the server and the
	//		actual transfer of data.  Firing these events here simulates, appeasing applications
	//		that are expecting that initial pair.

	m_pEvents1->Fire_DownloadBegin();
	m_pEvents2->Fire_DownloadBegin();
	m_pEvents1->Fire_DownloadComplete();
	m_pEvents2->Fire_DownloadComplete();

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getProtocolHandler (in nsIURI aURI, out nsIProtocolHandler aProtocolHandler); */
NS_IMETHODIMP CWebShellContainer::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void doContent (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, in nsIChannel aOpenedChannel, out nsIStreamListener aContentHandler, out boolean aAbortProcess); */
NS_IMETHODIMP CWebShellContainer::DoContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, nsIChannel *aOpenedChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean isPreferred (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, out string aDesiredContentType); */
NS_IMETHODIMP CWebShellContainer::IsPreferred(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean canHandleContent (in string aContentType, in nsURILoadCommand aCommand, in string aWindowTarget, out string aDesiredContentType); */
NS_IMETHODIMP CWebShellContainer::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsISupports loadCookie; */
NS_IMETHODIMP CWebShellContainer::GetLoadCookie(nsISupports * *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CWebShellContainer::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP CWebShellContainer::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CWebShellContainer::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocShellTreeOwner


NS_IMETHODIMP
CWebShellContainer::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
	nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(m_pOwner->m_pIWebBrowser));
	NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
	return docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome*, this), aFoundItem);
}


NS_IMETHODIMP
CWebShellContainer::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
	NS_ERROR("Haven't Implemented this yet");
	*aShell = nsnull;
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::SizeShellTo(nsIDocShellTreeItem* aShell,
   PRInt32 aCX, PRInt32 aCY)
{
	// Ignore request to be resized
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::ShowModal()
{
	// Ignore request to be shown modally
	return NS_OK;
}


NS_IMETHODIMP CWebShellContainer::GetNewWindow(PRInt32 aChromeFlags, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{
	NS_ERROR("Haven't Implemented this yet");
	*aDocShellTreeItem = nsnull;
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIBaseWindow

NS_IMETHODIMP 
CWebShellContainer::InitWindow(nativeWindow parentNativeWindow, nsIWidget * parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::Create(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::Destroy(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetPosition(PRInt32 x, PRInt32 y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetPosition(PRInt32 *x, PRInt32 *y)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetSize(PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::Repaint(PRBool force)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetParentWidget(nsIWidget * *aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetParentWidget(nsIWidget * aParentWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetVisibility(PRBool *aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetVisibility(PRBool aVisibility)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetMainWidget(nsIWidget * *aMainWidget)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetFocus(void)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::FocusAvailable(nsIBaseWindow *aCurrentFocus, PRBool *aTookFocus)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::GetTitle(PRUnichar * *aTitle)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebShellContainer::SetTitle(const PRUnichar * aTitle)
{
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CWebShellContainer::SetJSStatus(const PRUnichar *status)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::SetJSDefaultStatus(const PRUnichar *status)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::SetOverLink(const PRUnichar *link)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::GetChromeMask(PRUint32 *aChromeMask)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::SetChromeMask(PRUint32 aChromeMask)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::GetNewBrowser(PRUint32 chromeMask, nsIWebBrowser **_retval)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::FindNamedBrowserItem(const PRUnichar *aName, nsIDocShellTreeItem **_retval)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebShellContainer::ShowAsModal(void)
{
	return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver implementation


NS_IMETHODIMP
CWebShellContainer::OnStartRequest(nsIChannel* aChannel, nsISupports* aContext)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStartRequest(...)\n"));

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext, nsresult aStatus, const PRUnichar* aMsg)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStopRequest(..., %d, \"%s\")\n"), (int) aStatus, W2T((PRUnichar *) aMsg));

	// Fire a DownloadComplete event
	m_pEvents1->Fire_DownloadComplete();
	m_pEvents2->Fire_DownloadComplete();

	return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocumentLoaderObserver implementation 


NS_IMETHODIMP
CWebShellContainer::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* pURI, const char* aCommand)
{
	nsXPIDLCString aURI;
	pURI->GetSpec(getter_Copies(aURI));
	NG_TRACE(_T("CWebShellContainer::OnStartDocumentLoad(..., %s, \"%s\")\n"), A2CT(aURI), A2CT(aCommand));

	//Fire a DownloadBegin
	m_pEvents1->Fire_DownloadBegin();
	m_pEvents2->Fire_DownloadBegin();

	return NS_OK; 
} 


// we need this to fire the document complete 
NS_IMETHODIMP
CWebShellContainer::OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel *aChannel, nsresult aStatus)
{
	NG_TRACE(_T("CWebShellContainer::OnEndDocumentLoad(...,  \"\")\n"));

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

	return NS_OK; 
} 


NS_IMETHODIMP
CWebShellContainer::OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel)
{ 
	NG_TRACE(_T("CWebShellContainer::OnStartURLLoad(...,  \"\")\n"));

	//NOTE: This appears to get fired once for each individual item on a page.

	return NS_OK; 
} 


NS_IMETHODIMP
CWebShellContainer::OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel, PRUint32 aProgress, PRUint32 aProgressMax)
{ 
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnProgress(..., \"%d\", \"%d\")\n"), (int) aProgress, (int) aProgressMax);

	return NS_OK;
} 


NS_IMETHODIMP
CWebShellContainer::OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* aChannel, nsString& aMsg)
{ 
	NG_TRACE(_T("CWebShellContainer::OnStatusURLLoad(...,  \"\")\n"));
	
	//NOTE: This appears to get fired for each individual item on a page, indicating the status of that item.
	BSTR bstrStatus = SysAllocString(W2OLE((PRUnichar *) aMsg.GetUnicode()));
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);
	
	return NS_OK; 
} 


NS_IMETHODIMP
CWebShellContainer::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
{
	NG_TRACE(_T("CWebShellContainer::OnEndURLLoad(...,  \"\")\n"));

	//NOTE: This appears to get fired once for each individual item on a page.

	return NS_OK; 
} 

