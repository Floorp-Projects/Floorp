/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "MozEmbedChrome.h"
#include "MozEmbedStream.h"
#include "nsCWebBrowser.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIDocShellTreeItem.h"
#include "nsCRT.h"
#include "prlog.h"
#include "prprf.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocShell.h"

static NS_DEFINE_CID(kSimpleURICID,            NS_SIMPLEURI_CID);

// this is a define to make sure that we don't call certain function
// before the object has been properly initialized

#define CHECK_IS_INIT()                                \
        PR_BEGIN_MACRO                                 \
          if (!mIsInitialized)                         \
            return NS_ERROR_NOT_INITIALIZED;           \
        PR_END_MACRO

static PRLogModuleInfo *mozEmbedLm = NULL;

nsVoidArray *MozEmbedChrome::sBrowsers = NULL;

// constructor and destructor
MozEmbedChrome::MozEmbedChrome()
{
  NS_INIT_REFCNT();
  mOwningWidget  = nsnull;
  mWebBrowser       = nsnull;
  mNewBrowserCB     = nsnull;
  mNewBrowserCBData = nsnull;
  mDestroyCB        = nsnull;
  mDestroyCBData    = nsnull;
  mVisibilityCB     = nsnull;
  mVisibilityCBData = nsnull;
  mLinkCB           = nsnull;
  mLinkCBData       = nsnull;
  mJSStatusCB       = nsnull;
  mJSStatusCBData   = nsnull;
  mLocationCB       = nsnull;
  mLocationCBData   = nsnull;
  mTitleCB          = nsnull;
  mTitleCBData      = nsnull;
  mProgressCB       = nsnull;
  mProgressCBData   = nsnull;
  mNetCB            = nsnull;
  mNetCBData        = nsnull;
  mOpenCB           = nsnull;
  mOpenCBData       = nsnull;
  mBounds.x         = 0;
  mBounds.y         = 0;
  mBounds.width     = 0;
  mBounds.height    = 0;
  mVisibility       = PR_FALSE;
  mLinkMessage      = NULL;
  mJSStatus         = NULL;
  mLocation         = NULL;
  mTitle            = NULL;
  mChromeMask       = 0;
  mOffset           = 0;
  mDoingStream      = PR_FALSE;
  if (!mozEmbedLm)
    mozEmbedLm = PR_NewLogModule("MozEmbedChrome");
  if (!sBrowsers)
    sBrowsers = new nsVoidArray();
  sBrowsers->AppendElement((void *)this);
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::MozEmbedChrome %p\n", this));
}

MozEmbedChrome::~MozEmbedChrome()
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::~MozEmbedChrome %p\n", this));
  sBrowsers->RemoveElement((void *)this);
}

// nsISupports interface

NS_IMPL_ADDREF(MozEmbedChrome)
NS_IMPL_RELEASE(MozEmbedChrome)

NS_INTERFACE_MAP_BEGIN(MozEmbedChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPhEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIPhEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

// nsIPhEmbed interface

NS_IMETHODIMP MozEmbedChrome::Init(PtWidget_t *aOwningWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::Init\n"));
  mOwningWidget = aOwningWidget;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetNewBrowserCallback(MozEmbedChromeCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetNewBrowserCallback\n"));
  mNewBrowserCB = aCallback;
  mNewBrowserCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetDestroyCallback(MozEmbedDestroyCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetDestroyCallback\n"));
  mDestroyCB = aCallback;
  mDestroyCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetVisibilityCallback(MozEmbedVisibilityCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetVisibilityCallback\n"));
  mVisibilityCB = aCallback;
  mVisibilityCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetLinkChangeCallback (MozEmbedLinkCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetLinkChangeCallback\n"));
  mLinkCB = aCallback;
  mLinkCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetJSStatusChangeCallback (MozEmbedJSStatusCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetJSStatusChangeCallback\n"));
  mJSStatusCB = aCallback;
  mJSStatusCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetLocationChangeCallback (MozEmbedLocationCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetLocationChangeCallback\n"));
  mLocationCB = aCallback;
  mLocationCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetTitleChangeCallback (MozEmbedTitleCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetTitleChangeCB\n"));
  mTitleCB = aCallback;
  mTitleCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetProgressCallback (MozEmbedProgressCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetProgressCallback\n"));
  mProgressCB = aCallback;
  mProgressCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetNetCallback (MozEmbedNetCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetNetCallback\n"));
  mNetCB = aCallback;
  mNetCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetStartOpenCallback (MozEmbedStartOpenCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetStartOpenCallback\n"));
  mOpenCB = aCallback;
  mOpenCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetLinkMessage (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetLinkMessage\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mLinkMessage)
    *retval = nsCRT::strdup(mLinkMessage);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetJSStatus (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetJSStatus\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mJSStatus)
    *retval = nsCRT::strdup(mLinkMessage);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetLocation (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetLocation\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mLocation)
    *retval = nsCRT::strdup(mLocation);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetTitleChar (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetTitleChar\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mTitle)
    *retval = nsCRT::strdup(mTitle);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::OpenStream (const char *aBaseURI, const char *aContentType)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_ARG_POINTER(aContentType);

  nsresult rv = NS_OK;
  MozEmbedStream *newStream = nsnull;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;
  nsCOMPtr<nsIDocShell> docShell;
  nsCOMPtr<nsIContentViewerContainer> viewerContainer;
  nsCOMPtr<nsIContentViewer> contentViewer;
  nsCOMPtr<nsIURI> uri;
  nsCAutoString docLoaderContractID;
  nsCAutoString spec(aBaseURI);
  
  // check to see if we need to close the current stream
  if (mDoingStream)
    CloseStream();
  mDoingStream = PR_TRUE;
  // create our new stream object
  newStream = new MozEmbedStream();
  if (!newStream)
    return NS_ERROR_OUT_OF_MEMORY;
  // we own this
  NS_ADDREF(newStream);
  // QI it to the right interface
  mStream = do_QueryInterface(newStream);
  if (!mStream)
    return NS_ERROR_FAILURE;
  // ok, now we're just using it for an nsIInputStream interface so
  // release our second reference to it.
  NS_RELEASE(newStream);
  
  // get our hands on the docShell
  mWebBrowser->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_ERROR_FAILURE;

  // QI that to a content viewer container
  viewerContainer = do_QueryInterface(docShell);
  if (!viewerContainer)
    return NS_ERROR_FAILURE;

  // create a new uri object
  uri = do_CreateInstance(kSimpleURICID, &rv);
  if (NS_FAILED(rv))
    return rv;
  rv = uri->SetSpec(spec.GetBuffer());
  if (NS_FAILED(rv))
    return rv;

  // create a new load group
  rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), nsnull);
  if (NS_FAILED(rv))
    return rv;
  
  // create a new input stream channel
  rv = NS_NewInputStreamChannel(getter_AddRefs(mChannel), uri, mStream, aContentType,
				1024 /* len */);
  if (NS_FAILED(rv))
    return rv;

  // set the channel's load group
  rv = mChannel->SetLoadGroup(mLoadGroup);
  if (NS_FAILED(rv))
    return rv;

  // find a document loader for this command plus content type
  // combination

  docLoaderContractID  = NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX;
  docLoaderContractID += "view";
  docLoaderContractID += "/";
  docLoaderContractID += aContentType;

  docLoaderFactory = do_CreateInstance(docLoaderContractID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // ok, create an instance of the content viewer for that command and
  // mime type
  rv = docLoaderFactory->CreateInstance("view",
					mChannel,
					mLoadGroup,
					aContentType,
					viewerContainer,
					nsnull,
					getter_AddRefs(mStreamListener),
					getter_AddRefs(contentViewer));
					
  if (NS_FAILED(rv))
    return rv;

  // set the container viewer container for this content view
  rv = contentViewer->SetContainer(viewerContainer);
  if (NS_FAILED(rv))
    return rv;

  // embed this sucker.
  rv = viewerContainer->Embed(contentViewer, "view", nsnull);
  if (NS_FAILED(rv))
    return rv;

  // start our request
  rv = mStreamListener->OnStartRequest(mChannel, NULL);
  if (NS_FAILED(rv))
    return rv;
  
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::AppendToStream (const char *aData, int32 aLen)
{
  MozEmbedStream *embedStream = (MozEmbedStream *)mStream.get();
  nsresult rv;
  NS_ENSURE_STATE(mDoingStream);
  rv = embedStream->Append(aData, aLen);
  if (NS_FAILED(rv))
    return rv;
  rv = mStreamListener->OnDataAvailable(mChannel,
					NULL,
					mStream,
					mOffset, /* offset */
					aLen); /* len */
  mOffset += aLen;
  if (NS_FAILED(rv))
    return rv;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::CloseStream (void)
{
  nsresult rv;
  NS_ENSURE_STATE(mDoingStream);
  mDoingStream = PR_FALSE;
  rv = mStreamListener->OnStopRequest(mChannel,
				      NULL,
				      NS_OK,
				      NULL);
  if (NS_FAILED(rv))
    return rv;
  mStream = nsnull;
  mLoadGroup = nsnull;
  mChannel = nsnull;
  mStreamListener = nsnull;
  mContentViewer = nsnull;
  mOffset = 0;
  return NS_OK;
}

// nsIInterfaceRequestor interface

NS_IMETHODIMP MozEmbedChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetInterface\n"));
  return QueryInterface(aIID, aInstancePtr);
}

// nsIWebBrowserChrome interface

NS_IMETHODIMP MozEmbedChrome::SetJSStatus(const PRUnichar *status)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetJSStatus\n"));
  nsString jsStatusString(status);
  mJSStatus = jsStatusString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("js status is %s\n", (const char *)mJSStatus));
  // let any listeners know that the status has changed
  if (mJSStatusCB)
    mJSStatusCB(mJSStatusCBData);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetJSDefaultStatus(const PRUnichar *status)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetJSDefaultStatus\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::SetOverLink(const PRUnichar *link)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetOverLink\n"));
  nsString linkMessageString(link);
  mLinkMessage = linkMessageString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("message is %s\n", (const char *)mLinkMessage));
  // if there are any listeners, let them know about the change
  if (mLinkCB)
    mLinkCB(mLinkCBData);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetWebBrowser\n"));

  NS_ENSURE_ARG_POINTER(aWebBrowser);
  *aWebBrowser = mWebBrowser;
  NS_IF_ADDREF(*aWebBrowser);

  return NS_OK;

}

NS_IMETHODIMP MozEmbedChrome::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetWebBrowser\n"));

  NS_ENSURE_ARG_POINTER(aWebBrowser);
  mWebBrowser = aWebBrowser;

  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetChromeMask(PRUint32 *aChromeMask)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetChromeMask\n"));
  NS_ENSURE_ARG_POINTER(aChromeMask);
  *aChromeMask = mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetChromeMask(PRUint32 aChromeMask)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetChromeMask\n"));
  mChromeMask = aChromeMask;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetNewBrowser(PRUint32 chromeMask, 
					       nsIWebBrowser **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetNewBrowser\n"));
  if (mNewBrowserCB)
    return mNewBrowserCB(chromeMask, _retval, mNewBrowserCBData);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozEmbedChrome::FindNamedBrowserItem(const PRUnichar *aName, 
						      nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::FindNamedBrowserItem\n"));
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  PRInt32 i = 0;
  PRInt32 numBrowsers = sBrowsers->Count();

  for (i = 0; i < numBrowsers; i++)
  {
    MozEmbedChrome *chrome = (MozEmbedChrome *)sBrowsers->ElementAt(i);
    nsCOMPtr<nsIWebBrowser> webBrowser;
    NS_ENSURE_SUCCESS(chrome->GetWebBrowser(getter_AddRefs(webBrowser)), NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(webBrowser));
    NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

    docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome *, this), _retval);
    if (*_retval)
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SizeBrowserTo\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::ShowAsModal(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::ShowAsModal\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::ExitModalEventLoop(nsresult aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ExitModalLoop\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIURIContentListener

NS_IMETHODIMP MozEmbedChrome::OnStartURIOpen(nsIURI *aURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::OnStartURIOpen\n"));
  NS_ENSURE_ARG_POINTER(aAbortOpen);
  NS_ENSURE_ARG_POINTER(aURI);
  char *specString = NULL;
  nsCAutoString autoString;
  nsresult rv;

  rv = aURI->GetSpec(&specString);
  if (NS_FAILED(rv))
    return rv;
  
  autoString = specString;

  if (mOpenCB)
  {
    *aAbortOpen = mOpenCB(autoString, mOpenCBData);
    return NS_OK;
  }
  else
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetProtocolHandler\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::DoContent(const char *aContentType, nsURILoadCommand aCommand,
					   const char *aWindowTarget, nsIChannel *aOpenedChannel,
					   nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::DoContent\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::IsPreferred(const char *aContentType, nsURILoadCommand aCommand,
					     const char *aWindowTarget, char **aDesiredContentType,
					     PRBool *aCanHandleContent)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::IsPreferred\n"));
  NS_ENSURE_ARG_POINTER(aCanHandleContent);
  if (aContentType)
  {
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("checking content type %s\n", aContentType));
    if (nsCRT::strcasecmp(aContentType,  "text/html") == 0
        || nsCRT::strcasecmp(aContentType, "text/xul") == 0
        || nsCRT::strcasecmp(aContentType, "text/rdf") == 0 
        || nsCRT::strcasecmp(aContentType, "text/xml") == 0
        || nsCRT::strcasecmp(aContentType, "text/css") == 0
        || nsCRT::strcasecmp(aContentType, "image/gif") == 0
        || nsCRT::strcasecmp(aContentType, "image/jpeg") == 0
        || nsCRT::strcasecmp(aContentType, "image/png") == 0
        || nsCRT::strcasecmp(aContentType, "image/tiff") == 0
        || nsCRT::strcasecmp(aContentType, "application/http-index-format") == 0)
      *aCanHandleContent = PR_TRUE;
  }
  else
    *aCanHandleContent = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand,
						  const char *aWindowTarget, char **aDesiredContentType,
						  PRBool *_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::CanHandleContent\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetLoadCookie(nsISupports * *aLoadCookie)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetLoadCookie\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::SetLoadCookie(nsISupports * aLoadCookie)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetLoadCookie\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetParentContentListener\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetParentContentListener\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIWebProgressListener

NS_IMETHODIMP MozEmbedChrome::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                                  PRInt32 curSelfProgress,
						  PRInt32 maxSelfProgress, PRInt32 curTotalProgress,
						  PRInt32 maxTotalProgress)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::OnProgressChange\n"));
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("curTotalProgress is %d and maxTotalProgress is %d\n",
				    curTotalProgress, maxTotalProgress));
  if (maxTotalProgress >= 0)
  {
    PRUint32 percentage = (curTotalProgress * 100) / maxTotalProgress;
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("%d%% percent\n", percentage));
  }
  else
  {
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("Unknown percent\n"));
  }
  
  // call our callback if it's been registered
  if (mProgressCB)
    mProgressCB(mProgressCBData, curTotalProgress, maxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRInt32 aStateFlags, nsresult aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::OnStateChange\n"));
  if (aStateFlags & nsIWebProgressListener::flag_start)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_start\n"));
  if (aStateFlags & nsIWebProgressListener::flag_redirecting)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_redirecting\n"));
  if (aStateFlags & nsIWebProgressListener::flag_negotiating)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_negotiating\n"));
  if (aStateFlags & nsIWebProgressListener::flag_transferring)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_transferring\n"));
  if (aStateFlags & nsIWebProgressListener::flag_stop)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_stop\n"));
  if (aStateFlags & nsIWebProgressListener::flag_is_request)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_is_request\n"));
  if (aStateFlags & nsIWebProgressListener::flag_is_document)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_is_document\n"));
  if (aStateFlags & nsIWebProgressListener::flag_is_network)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_is_network\n"));
  if (aStateFlags & nsIWebProgressListener::flag_is_window)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_is_window\n"));

  // if we have a callback registered, call it
  if (mNetCB)
    mNetCB(mNetCBData, aStateFlags, aStatus);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                               nsIRequest* aRequest,
                                               nsIURI *aLocation)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::OnLocationChange\n"));
  char *newURIString = NULL;
  NS_ENSURE_ARG_POINTER(aLocation);
  aLocation->GetSpec(&newURIString);
  mLocation = newURIString;
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("new location is %s\n", (const char *)mLocation));
  // if there are any listeners let them know about the location change
  if (mLocationCB)
    mLocationCB(mLocationCBData);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
MozEmbedChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest,
                               nsresult aStatus,
                               const PRUnichar* aMessage)
{
    return NS_OK;
}



NS_IMETHODIMP 
MozEmbedChrome::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIBaseWindow interface

NS_IMETHODIMP MozEmbedChrome::InitWindow(nativeWindow parentNativeWindow,
					    nsIWidget * parentWidget, 
					    PRInt32 x, PRInt32 y,
					    PRInt32 cx, PRInt32 cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::InitWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::Create(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::Create\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::Destroy(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::Destory\n"));
  if (mDestroyCB)
    mDestroyCB(mDestroyCBData);
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetPosition(PRInt32 x, PRInt32 y)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetPosition\n"));
  mBounds.x = x;
  mBounds.y = y;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetPosition(PRInt32 *x, PRInt32 *y)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetPosition\n"));
  NS_ENSURE_ARG_POINTER(x);
  NS_ENSURE_ARG_POINTER(y);
  *x = mBounds.x;
  *y = mBounds.y;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetSize\n"));
  mBounds.width = cx;
  mBounds.height = cy;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetSize(PRInt32 *cx, PRInt32 *cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetSize\n"));
  NS_ENSURE_ARG_POINTER(cx);
  NS_ENSURE_ARG_POINTER(cy);
  *cx = mBounds.width;
  *cy = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetPositionAndSize(PRInt32 x, PRInt32 y,
						    PRInt32 cx, PRInt32 cy,
						    PRBool fRepaint)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetPositionAndSize %d %d %d %d\n",
				    x, y, cx, cy));
  mBounds.x = x;
  mBounds.y = y;
  mBounds.width = cx;
  mBounds.height = cy;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetPositionAndSize(PRInt32 *x, PRInt32 *y,
						    PRInt32 *cx, PRInt32 *cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetPositionAndSize %d %d %d %d\n",
				    mBounds.x, mBounds.y, mBounds.width, mBounds.height));
  NS_ENSURE_ARG_POINTER(x);
  NS_ENSURE_ARG_POINTER(y);
  NS_ENSURE_ARG_POINTER(cx);
  NS_ENSURE_ARG_POINTER(cy);
  *x = mBounds.x;
  *y = mBounds.y;
  *cx = mBounds.width;
  *cy = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::Repaint(PRBool force)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedCHrome::Repaint\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetParentWidget(nsIWidget * *aParentWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetParentWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::SetParentWidget(nsIWidget * aParentWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetParentWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetParentNativeWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP MozEmbedChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetParentNativeWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetVisibility(PRBool *aVisibility)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetVisibility\n"));
  NS_ENSURE_ARG_POINTER(aVisibility);
  *aVisibility = mVisibility;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::SetVisibility(PRBool aVisibility)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetVisibility for %p\n", this));
  if (mVisibilityCB)
    mVisibilityCB(aVisibility, mVisibilityCBData);
  mVisibility = aVisibility;
  return NS_OK;
}

NS_IMETHODIMP MozEmbedChrome::GetMainWidget(nsIWidget * *aMainWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetMainWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::SetFocus(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetFocus\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::FocusAvailable(nsIBaseWindow *aCurrentFocus, 
						PRBool *aTookFocus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::FocusAvailable\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MozEmbedChrome::GetTitle(PRUnichar * *aTitle)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::GetTitle\n"));
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = nsnull;
  if (mTitle)
    *aTitle = mTitleUnicode.ToNewUnicode();
  return NS_OK;
}
 
NS_IMETHODIMP MozEmbedChrome::SetTitle(const PRUnichar * aTitle)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("MozEmbedChrome::SetTitle\n"));
  nsString newTitleString(aTitle);
  mTitleUnicode = aTitle;
  mTitle = newTitleString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("title is %s\n", (const char *)mTitle));
  // if there's a callback, call it to let it know that the title has
  // changed.
  if (mTitleCB)
    mTitleCB(mTitleCBData);
  return NS_OK;
}

NS_IMETHODIMP
MozEmbedChrome::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                  PRBool aPersistCX, PRBool aPersistCY,
                                  PRBool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MozEmbedChrome::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                  PRBool* aPersistCX, PRBool* aPersistCY,
                                  PRBool* aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
