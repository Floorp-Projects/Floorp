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
}


CWebShellContainer::~CWebShellContainer()
{
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CWebShellContainer)
NS_IMPL_RELEASE(CWebShellContainer)
NS_IMPL_QUERY_HEAD(CWebShellContainer)
	NS_IMPL_QUERY_BODY(nsIBrowserWindow)
	NS_IMPL_QUERY_BODY(nsIStreamObserver)
	NS_IMPL_QUERY_BODY(nsIDocumentLoaderObserver)
	NS_IMPL_QUERY_BODY(nsIWebShellContainer)
NS_IMPL_QUERY_TAIL(nsIStreamObserver)


///////////////////////////////////////////////////////////////////////////////
// nsIBrowserWindow implementation

NS_IMETHODIMP
CWebShellContainer::Init(nsIAppShell* aAppShell, const nsRect& aBounds, PRUint32 aChromeMask, PRBool aAllowPlugins)
{
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::MoveTo(PRInt32 aX, PRInt32 aY)
{
	NG_TRACE_METHOD(CWebShellContainer::MoveTo);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
	NG_TRACE_METHOD(CWebShellContainer::SizeTo);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::SizeWindowTo(PRInt32 aWidth, PRInt32 aHeight,
                                 PRBool aWidthTransient, PRBool aHeightTransient)
{
	NG_TRACE_METHOD(CWebShellContainer::SizeWindowTo);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::SizeContentTo(PRInt32 aWidth, PRInt32 aHeight)
{
	NG_TRACE_METHOD(CWebShellContainer::SizeContentTo);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetContentBounds(nsRect& aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetContentBounds);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetBounds(nsRect& aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetBounds);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetWindowBounds(nsRect& aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetWindowBounds);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::IsIntrinsicallySized(PRBool& aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::IsIntrinsicallySized);
    aResult = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::ShowAfterCreation()
{
  return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::Show()
{
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::Hide()
{
	NG_TRACE_METHOD(CWebShellContainer::Hide);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::Close()
{
	NG_TRACE_METHOD(CWebShellContainer::Close);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::ShowModally(PRBool aPrepare)
{
	NG_TRACE_METHOD(CWebShellContainer::ShowModally);
	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::SetChrome(PRUint32 aNewChromeMask)
{
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetChrome(PRUint32& aChromeMaskResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetChrome);
	aChromeMaskResult = 0;
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::SetTitle(const PRUnichar* aTitle)
{
	//Fire a title change event
	USES_CONVERSION;
	LPOLESTR pszConvertedLocationName = W2OLE(const_cast<PRUnichar *>(aTitle));
	BSTR __RPC_FAR LocationName = SysAllocString(pszConvertedLocationName);
	m_pEvents1->Fire_TitleChange(LocationName);
	m_pEvents2->Fire_TitleChange(LocationName);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetTitle(PRUnichar** aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetTitle);
	*aResult = nsnull;
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::SetStatus(const PRUnichar* aStatus)
{

	NG_TRACE_METHOD(CWebShellContainer::SetStatus);
	
	//Gets fired on mouse over link events.
	BSTR bstrStatus = SysAllocString(W2OLE((PRUnichar *) aStatus));
	m_pEvents1->Fire_StatusTextChange(bstrStatus);
	m_pEvents2->Fire_StatusTextChange(bstrStatus);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetStatus(const PRUnichar** aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetStatus);
	*aResult = nsnull;
	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::SetDefaultStatus(const PRUnichar* aStatus)
{
	NG_TRACE_METHOD(CWebShellContainer::SetDefaultStatus);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetDefaultStatus(const PRUnichar** aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetDefaultStatus);
	*aResult = nsnull;
	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::SetProgress(PRInt32 aProgress, PRInt32 aProgressMax)
{
	NG_TRACE_METHOD(CWebShellContainer::SetProgress);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::ShowMenuBar(PRBool aShow)
{
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetWebShell(nsIWebShell*& aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetWebShell);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::GetContentWebShell(nsIWebShell **aResult)
{
	NG_TRACE_METHOD(CWebShellContainer::GetContentWebShell);
	*aResult = nsnull;
	return NS_OK;
}



///////////////////////////////////////////////////////////////////////////////
// nsIWebShellContainer implementation


NS_IMETHODIMP
CWebShellContainer::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::WillLoadURL(..., \"%s\", %d)\n"), W2T(aURL), (int) aReason);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::BeginLoadURL(..., \"%s\")\n"), W2T(aURL));

	// Setup the post data
	CComVariant vPostDataRef;
	CComVariant vPostData;
	vPostDataRef.vt = VT_BYREF | VT_VARIANT;
	vPostDataRef.pvarVal = &vPostData;
	// TODO get the post data passed in via the original call to Navigate()

	// Fire a BeforeNavigate event
	OLECHAR *pszURL = W2OLE((WCHAR *)aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	BSTR bstrTargetFrameName = NULL;
	BSTR bstrHeaders = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	long lFlags = 0;

	m_pEvents1->Fire_BeforeNavigate(bstrURL, lFlags, bstrTargetFrameName, &vPostDataRef, bstrHeaders, &bCancel);

	// Fire a BeforeNavigate2 event
	CComVariant vURL(bstrURL);
	CComVariant vFlags(lFlags);
	CComVariant vTargetFrameName(bstrTargetFrameName);
	CComVariant vHeaders(bstrHeaders);

	m_pEvents2->Fire_BeforeNavigate2(m_pOwner, &vURL, &vFlags, &vTargetFrameName, &vPostDataRef, &vHeaders, &bCancel);

	SysFreeString(bstrURL);
	SysFreeString(bstrTargetFrameName);
	SysFreeString(bstrHeaders);

	if (bCancel == VARIANT_TRUE)
	{
		return NS_ERROR_ABORT;
	}
	else
	{
		m_pOwner->m_bBusy = TRUE;
	}

	//NOTE:	The IE control fires a DownloadBegin after the first BeforeNavigate.  It then fires a 
	//		DownloadComplete after then engine has made it's initial connection to the server.  It
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

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::ProgressLoadURL(..., \"%s\", %d, %d)\n"), W2T(aURL), (int) aProgress, (int) aProgressMax);
	
	long nProgress = aProgress;
	long nProgressMax = aProgressMax;

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


NS_IMETHODIMP
CWebShellContainer::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::EndLoadURL(..., \"%s\", %d)\n"), W2T(aURL), (int) aStatus);

	// Fire a NavigateComplete event
	OLECHAR *pszURL = W2OLE((WCHAR *) aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	m_pEvents1->Fire_NavigateComplete(bstrURL);

	// Fire a NavigateComplete2 event
	CComVariant vURL(bstrURL);
	m_pEvents2->Fire_NavigateComplete2(m_pOwner, &vURL);

	// Fire the new NavigateForward state
	VARIANT_BOOL bEnableForward = VARIANT_FALSE;
	if ( m_pOwner->m_pIWebShell->CanForward() == NS_OK )
	{
		bEnableForward = VARIANT_TRUE;
	}
	m_pEvents2->Fire_CommandStateChange(CSC_NAVIGATEFORWARD, bEnableForward);

	// Fire the new NavigateBack state
	VARIANT_BOOL bEnableBack = VARIANT_FALSE;
	if ( m_pOwner->m_pIWebShell->CanBack() == NS_OK )
	{
		bEnableBack = VARIANT_TRUE;
	}
	m_pEvents2->Fire_CommandStateChange(CSC_NAVIGATEBACK, bEnableBack);

	m_pOwner->m_bBusy = FALSE;
	SysFreeString(bstrURL);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::NewWebShell(PRUint32 aChromeMask, PRBool aVisible, nsIWebShell *&aNewWebShell)
{
	NG_TRACE_METHOD(CWebShellContainer::NewWebShell);
	nsresult rv = NS_ERROR_OUT_OF_MEMORY;
	return rv;
}


NS_IMETHODIMP
CWebShellContainer::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::FindWebShellWithName(\"%s\", ...)\n"), W2T(aName));

	nsresult rv = NS_OK; 

	// Zero result (in case we fail). 
	aResult = nsnull;  

	if (m_pOwner->m_pIWebShell != NULL)
	{ 
		rv = m_pOwner->m_pIWebShell->FindChildWithName(aName, aResult); 
	} 

	return rv; 
}


NS_IMETHODIMP
CWebShellContainer::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
	NG_TRACE_METHOD(CWebShellContainer::FocusAvailable);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::ContentShellAdded(nsIWebShell* aWebShell, nsIContent* frameNode)
{
	NG_TRACE_METHOD(CWebShellContainer::ContentShellAdded);
	nsresult rv = NS_OK;
	return rv;
}


NS_IMETHODIMP
CWebShellContainer::CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup)
{
	NG_TRACE_METHOD(CWebShellContainer::CreatePopup);
	HMENU hMenu = ::CreatePopupMenu();
    *outPopup = NULL;
	InsertMenu(hMenu, 0, MF_BYPOSITION, 1, _T("TODO"));
	TrackPopupMenu(hMenu, TPM_LEFTALIGN, aXPos, aYPos, NULL, NULL, NULL);
	return NS_OK;
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
CWebShellContainer::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand)
{
	char* wString = nsnull;    
	aURL->GetPath(&wString);
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStartDocumentLoad(..., %s, \"%s\")\n"),A2CT(wString), A2CT(aCommand));

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
    nsIURI* uri = nsnull;

    aChannel->GetURI(&uri);
    if (uri) {
      uri->GetSpec(&aString);
    }
	if (aString == NULL)
	{
		return NS_ERROR_NULL_POINTER;
	}

	USES_CONVERSION;
	BSTR bstrURL = SysAllocString(A2OLE((CHAR *) aString)); 
		
	delete [] aString; // clean up. 

	// Fire a DocumentComplete event
	CComVariant vURL(bstrURL);
	m_pEvents2->Fire_DocumentComplete(m_pOwner, &vURL);
	SysFreeString(bstrURL);

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

