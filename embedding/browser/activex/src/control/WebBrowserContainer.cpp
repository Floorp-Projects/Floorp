/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Adam Lock <adamlock@netscape.com>
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

#include "stdafx.h"

#include <limits.h>
#include <shlobj.h>

#include "WebBrowserContainer.h"

#include "nsICategoryManager.h"
#include "nsReadableUtils.h"

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
    NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
//    NS_INTERFACE_MAP_ENTRY(nsICommandHandler)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CWebBrowserContainer::GetInterface(const nsIID & uuid, void * *result)
{
    return QueryInterface(uuid, result);
}


///////////////////////////////////////////////////////////////////////////////
// nsIContextMenuListener

NS_IMETHODIMP CWebBrowserContainer::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    m_pOwner->ShowContextMenu(aContextFlags, aEvent, aNode);
    return NS_OK;
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
    nsresult rv = NS_OK;

    NG_TRACE(_T("CWebBrowserContainer::OnStateChange(...)\n"));

    // determine whether or not the document load has started or stopped.
    if (progressStateFlags & STATE_IS_DOCUMENT)
    {
        if (progressStateFlags & STATE_START)
        {
            NG_TRACE(_T("CWebBrowserContainer::OnStateChange->Doc Start(...,  \"\")\n"));

            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
            if (channel)
            {
                nsCOMPtr<nsIURI> uri;
                rv = channel->GetURI(getter_AddRefs(uri));
                if (NS_SUCCEEDED(rv))
                {
                    nsXPIDLCString aURI;
                    uri->GetSpec(getter_Copies(aURI));
                    NG_TRACE(_T("CWebBrowserContainer::OnStateChange->Doc Start(..., %s, \"\")\n"), A2CT(aURI));
                }
            }

            //Fire a DownloadBegin
            m_pEvents1->Fire_DownloadBegin();
            m_pEvents2->Fire_DownloadBegin();
        }
        else if (progressStateFlags & STATE_STOP)
        {
            NG_TRACE(_T("CWebBrowserContainer::OnStateChange->Doc Stop(...,  \"\")\n"));

            if (m_pOwner->mIERootDocument)
            {
                // allow to keep old document around
                m_pOwner->mIERootDocument->Release();
                m_pOwner->mIERootDocument = NULL;
            }

            //Fire a DownloadComplete
            m_pEvents1->Fire_DownloadComplete();
            m_pEvents2->Fire_DownloadComplete();

            nsCOMPtr<nsIURI> pURI;

            nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(aRequest);
            if (!aChannel) return NS_ERROR_NULL_POINTER;

            rv = aChannel->GetURI(getter_AddRefs(pURI));
            if (NS_FAILED(rv)) return rv;

            nsXPIDLCString aURI;
            rv = pURI->GetSpec(getter_Copies(aURI));
            if (NS_FAILED(rv)) return rv;

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
        }

    }

    if (progressStateFlags & STATE_IS_NETWORK)
    {

        if (progressStateFlags & STATE_START)
        {
            // TODO 
        }

        if (progressStateFlags & STATE_STOP)
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
//    nsXPIDLCString aPath;
//    location->GetPath(getter_Copies(aPath));
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
CWebBrowserContainer::OnStatusChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     nsresult aStatus,
                                     const PRUnichar* aMessage)
{
    NG_TRACE(_T("CWebBrowserContainer::OnStatusChange(...,  \"\")\n"));

    BSTR bstrStatus = SysAllocString(W2OLE((PRUnichar *) aMessage));
    m_pEvents1->Fire_StatusTextChange(bstrStatus);
    m_pEvents2->Fire_StatusTextChange(bstrStatus);
    SysFreeString(bstrStatus);

    return NS_OK;
}

NS_IMETHODIMP 
CWebBrowserContainer::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                       nsIRequest *aRequest, 
                                       PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

/* void onStartURIOpen (in nsIURI aURI, out boolean aAbortOpen); */
NS_IMETHODIMP CWebBrowserContainer::OnStartURIOpen(nsIURI *pURI, PRBool *aAbortOpen)
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

    //NOTE:    The IE control fires a DownloadBegin after the first BeforeNavigate.
    //      It then fires a DownloadComplete after the engine has made it's
    //      initial connection to the server.  It then fires a second
    //      DownloadBegin/DownloadComplete pair around the loading of
    //      everything on the page.  These events get fired out of
    //      CWebBrowserContainer::StartDocumentLoad() and
    //      CWebBrowserContainer::EndDocumentLoad().
    //        We don't appear to distinguish between the initial connection to
    //      the server and the actual transfer of data.  Firing these events
    //      here simulates, appeasing applications that are expecting that
    //      initial pair.

    m_pEvents1->Fire_DownloadBegin();
    m_pEvents2->Fire_DownloadBegin();
    m_pEvents1->Fire_DownloadComplete();
    m_pEvents2->Fire_DownloadComplete();

    return NS_OK;
}

/* void doContent (in string aContentType, in nsURILoadCommand aCommand, in nsIRequest request, out nsIStreamListener aContentHandler, out boolean aAbortProcess); */
NS_IMETHODIMP CWebBrowserContainer::DoContent(const char *aContentType, nsURILoadCommand aCommand, nsIRequest *request, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean isPreferred (in string aContentType, in nsURILoadCommand aCommand, out string aDesiredContentType); */
NS_IMETHODIMP CWebBrowserContainer::IsPreferred(const char *aContentType, nsURILoadCommand aCommand, char **aDesiredContentType, PRBool *_retval)
{
    return CanHandleContent(aContentType, aCommand, aDesiredContentType, _retval);
}


/* boolean canHandleContent (in string aContentType, in nsURILoadCommand aCommand, out string aDesiredContentType); */
NS_IMETHODIMP CWebBrowserContainer::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand, char **aDesiredContentType, PRBool *_retval)
{
    if (aContentType)
    {
        nsCOMPtr<nsICategoryManager> catMgr;
        nsresult rv;
        catMgr = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
        nsXPIDLCString value;
        rv = catMgr->GetCategoryEntry("Gecko-Content-Viewers",
            aContentType, 
            getter_Copies(value));

        // If the category manager can't find what we're looking for
        // it returns NS_ERROR_NOT_AVAILABLE, we don't wanto propagate
        // that to the caller since it's really not a failure

        if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)
            return rv;

        if (value && *value)
            *_retval = PR_TRUE;
    }
    return NS_OK;
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
// nsIEmbeddingSiteWindow

NS_IMETHODIMP 
CWebBrowserContainer::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetSiteWindow(void **aParentNativeWindow)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetFocus(void)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
CWebBrowserContainer::GetTitle(PRUnichar * *aTitle)
{
    NG_ASSERT_POINTER(aTitle, PRUnichar **);
    if (!aTitle)
        return E_INVALIDARG;

    *aTitle = ToNewUnicode(m_sTitle);

    return NS_OK;
}


NS_IMETHODIMP 
CWebBrowserContainer::SetTitle(const PRUnichar * aTitle)
{
    NG_ASSERT_POINTER(aTitle, PRUnichar *);
    if (!aTitle)
        return E_INVALIDARG;

    m_sTitle = aTitle;
    // Fire a TitleChange event
    BSTR bstrTitle = SysAllocString(aTitle);
    m_pEvents1->Fire_TitleChange(bstrTitle);
    m_pEvents2->Fire_TitleChange(bstrTitle);
    SysFreeString(bstrTitle);

    return NS_OK;
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


///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChromeFocus implementation

NS_IMETHODIMP
CWebBrowserContainer::FocusNextElement()
{
    ATLTRACE(_T("CWebBrowserContainer::FocusNextElement()\n"));
    m_pOwner->NextDlgControl();
    return NS_OK;
}

NS_IMETHODIMP
CWebBrowserContainer::FocusPrevElement()
{
    ATLTRACE(_T("CWebBrowserContainer::FocusPrevElement()\n"));
    m_pOwner->PrevDlgControl();
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CWebBrowserContainer::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
    //Fire a StatusTextChange event
    BSTR bstrStatus = SysAllocString(status);
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
CWebBrowserContainer::GetChromeFlags(PRUint32 *aChromeFlags)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SetChromeFlags(PRUint32 aChromeFlags)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::CreateBrowserWindow(PRUint32 chromeFlags,  PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nsIWebBrowser **_retval)
{
      IDispatch *pDispNew = NULL;
      VARIANT_BOOL bCancel = VARIANT_FALSE;

    *_retval = nsnull;

      // Test if the event sink can give us a new window to navigate into
      m_pEvents2->Fire_NewWindow2(&pDispNew, &bCancel);

    if ((bCancel == VARIANT_FALSE) && pDispNew)
      {
        CComQIPtr<IMozControlBridge, &IID_IMozControlBridge> cpBridge = pDispNew;
        if (cpBridge)
        {
            nsIWebBrowser *browser = nsnull;
            cpBridge->GetWebBrowser((void **) &browser);
            if (browser)
            {
                *_retval = browser;
                return NS_OK;
            }
        }
        pDispNew->Release();
    }

    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::DestroyBrowserWindow(void)
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
CWebBrowserContainer::IsWindowModal(PRBool *_retval)
{
    // we're not
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
CWebBrowserContainer::ExitModalEventLoop(nsresult aStatus)
{
    // Ignore request to exit modal loop
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver implementation


NS_IMETHODIMP
CWebBrowserContainer::OnStartRequest(nsIRequest *request, nsISupports* aContext)
{
    USES_CONVERSION;
    NG_TRACE(_T("CWebBrowserContainer::OnStartRequest(...)\n"));

    return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::OnStopRequest(nsIRequest *request, nsISupports* aContext, nsresult aStatus)
{
    USES_CONVERSION;
    NG_TRACE(_T("CWebBrowserContainer::OnStopRequest(..., %d)\n"), (int) aStatus);

    // Fire a DownloadComplete event
    m_pEvents1->Fire_DownloadComplete();
    m_pEvents2->Fire_DownloadComplete();

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsICommandHandler implementation

/* void do (in string aCommand, in string aStatus); */
NS_IMETHODIMP CWebBrowserContainer::Exec(const char *aCommand, const char *aStatus, char **aResult)
{
    return NS_OK;
}

/* void query (in string aCommand, in string aStatus); */
NS_IMETHODIMP CWebBrowserContainer::Query(const char *aCommand, const char *aStatus, char **aResult)
{
    return NS_OK;
}
