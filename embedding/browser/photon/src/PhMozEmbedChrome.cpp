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
 *	 Brian Edmond <briane@qnx.com>
 */

#include "PhMozEmbedChrome.h"
#include "PhMozEmbedStream.h"
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

nsVoidArray *PhMozEmbedChrome::sBrowsers = NULL;

// constructor and destructor
PhMozEmbedChrome::PhMozEmbedChrome()
{
  NS_INIT_REFCNT();
  mOwningPhWidget  = nsnull;
  mWebBrowser       = nsnull;
  mBounds.x         = 0;
  mBounds.y         = 0;
  mBounds.width     = 0;
  mBounds.height    = 0;
  mVisibility       = PR_FALSE;
  mLinkMessage      = NULL;
  mJSStatus         = NULL;
  mTitle            = NULL;
  mChromeMask       = 0;
  mOffset           = 0;
  mDoingStream      = PR_FALSE;
  mChromeListener   = 0;
  mContentShell     = 0;
  if (!mozEmbedLm)
    mozEmbedLm = PR_NewLogModule("PhMozEmbedChrome");
  if (!sBrowsers)
    sBrowsers = new nsVoidArray();
  sBrowsers->AppendElement((void *)this);
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::PhMozEmbedChrome %p\n", this));
}

PhMozEmbedChrome::~PhMozEmbedChrome()
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::~PhMozEmbedChrome %p\n", this));
  sBrowsers->RemoveElement((void *)this);
}

// nsISupports interface

NS_IMPL_ADDREF(PhMozEmbedChrome)
NS_IMPL_RELEASE(PhMozEmbedChrome)

NS_INTERFACE_MAP_BEGIN(PhMozEmbedChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPhEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIPhEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

// nsIPhEmbed interface

NS_IMETHODIMP PhMozEmbedChrome::Init(PtWidget_t *aOwningWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::Init\n"));
  mOwningPhWidget = aOwningWidget;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SetEmbedListener(PhEmbedListener *aListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetEmbedListener\n"));
  // This listener isn't a refcnted object.  It's assumed that the
  // listener is the owner and will be destroyed after this object is.
  mChromeListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetLinkMessage (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetLinkMessage\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mLinkMessage)
    *retval = nsCRT::strdup(mLinkMessage);
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetJSStatus (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetJSStatus\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mJSStatus)
    *retval = nsCRT::strdup(mJSStatus);
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetTitleChar (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetTitleChar\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mTitle)
    *retval = nsCRT::strdup(mTitle);
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::OpenStream (const char *aBaseURI, const char *aContentType)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_ARG_POINTER(aContentType);

  nsresult rv = NS_OK;
  PhMozEmbedStream *newStream = nsnull;

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
  newStream = new PhMozEmbedStream();
  if (!newStream)
    return NS_ERROR_OUT_OF_MEMORY;
  // we own this
  NS_ADDREF(newStream);
  // initialize it
  newStream->Init();
  // QI it to the right interface
  mStream = do_QueryInterface(newStream);
  if (!mStream)
    return NS_ERROR_FAILURE;
  // ok, now we're just using it for an nsIInputStream interface so
  // release our second reference to it.
  NS_RELEASE(newStream);
  
  // get our hands on the primary content area of that docshell
  // get the browser as an item
  nsCOMPtr<nsIDocShellTreeItem> browserAsItem;
  browserAsItem = do_QueryInterface(mWebBrowser);
  // get the tree owner for that item
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  // get the primary content shell as an item
  nsCOMPtr<nsIDocShellTreeItem> contentItem;
  treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
  // QI that back to a docshell
  docShell = do_QueryInterface(contentItem);
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
  docLoaderContractID += "view;1?type=";
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

NS_IMETHODIMP PhMozEmbedChrome::AppendToStream (const char *aData, int32 aLen)
{
  nsIInputStream *inputStream;
  inputStream = mStream.get();
  PhMozEmbedStream *embedStream = (PhMozEmbedStream *)inputStream;
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

NS_IMETHODIMP PhMozEmbedChrome::CloseStream (void)
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

NS_IMETHODIMP PhMozEmbedChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetInterface\n"));
  return QueryInterface(aIID, aInstancePtr);
}

// nsIWebBrowserChrome interface

NS_IMETHODIMP PhMozEmbedChrome::SetStatus(PRUint32 aType, const PRUnichar *status)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetStatus\n"));

  switch (aType)
    {
    case STATUS_SCRIPT:
      {
	nsString jsStatusString(status);
	mJSStatus = jsStatusString.ToNewCString();
	PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("js status is %s\n", (const char *)mJSStatus));
	// let our chrome listener know that the JS message has changed.
	if (mChromeListener)
	  mChromeListener->Message(PhEmbedListener::MessageJSStatus, mJSStatus);
      }
      break;
    case STATUS_SCRIPT_DEFAULT:
      // NOT IMPLEMENTED
      break;
    case STATUS_LINK:
      {
	nsString linkMessageString(status);
	mLinkMessage = linkMessageString.ToNewCString();
	PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("message is %s\n", (const char *)mLinkMessage));
	// notify the chrome listener that the link message has changed
	if (mChromeListener)
	  mChromeListener->Message(PhEmbedListener::MessageLink, mLinkMessage);
	return NS_OK;
      }
    }
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetWebBrowser\n"));

  NS_ENSURE_ARG_POINTER(aWebBrowser);
  *aWebBrowser = mWebBrowser;
  NS_IF_ADDREF(*aWebBrowser);

  return NS_OK;

}

NS_IMETHODIMP PhMozEmbedChrome::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetWebBrowser\n"));

  NS_ENSURE_ARG_POINTER(aWebBrowser);
  mWebBrowser = aWebBrowser;

  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetChromeFlags(PRUint32 *aChromeFlags)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetChromeFlags\n"));
  NS_ENSURE_ARG_POINTER(aChromeFlags);
  *aChromeFlags = mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SetChromeFlags(PRUint32 aChromeFlags)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetChromeFlags\n"));
  mChromeMask = aChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::CreateBrowserWindow(PRUint32 chromeMask, 
     PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nsIWebBrowser **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetNewBrowser\n"));
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP PhMozEmbedChrome::FindNamedBrowserItem(const PRUnichar *aName, 
						      nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::FindNamedBrowserItem\n"));
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  PRInt32 i = 0;
  PRInt32 numBrowsers = sBrowsers->Count();

  for (i = 0; i < numBrowsers; i++)
  {
    PhMozEmbedChrome *chrome = (PhMozEmbedChrome *)sBrowsers->ElementAt(i);
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

NS_IMETHODIMP PhMozEmbedChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SizeBrowserTo %d %d\n", aCX, aCY));
  if (mChromeListener)
    mChromeListener->SizeTo(aCX, aCY);
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::ShowAsModal(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::ShowAsModal\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::ExitModalEventLoop(nsresult aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::ExitModalLoop\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIURIContentListener

NS_IMETHODIMP PhMozEmbedChrome::OnStartURIOpen(nsIURI *aURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::OnStartURIOpen\n"));
  NS_ENSURE_ARG_POINTER(aAbortOpen);
  NS_ENSURE_ARG_POINTER(aURI);
  char *specString = NULL;
  nsCAutoString autoString;
  nsresult rv;

  rv = aURI->GetSpec(&specString);
  if (NS_FAILED(rv))
    return rv;
  
  autoString = specString;

  if (mChromeListener)
  {
    *aAbortOpen = mChromeListener->StartOpen(autoString);
    return NS_OK;
  }
  else
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetProtocolHandler\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::DoContent(const char *aContentType, nsURILoadCommand aCommand,
					   const char *aWindowTarget, nsIChannel *aOpenedChannel,
					   nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::DoContent\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::IsPreferred(const char *aContentType, nsURILoadCommand aCommand,
					     const char *aWindowTarget, char **aDesiredContentType,
					     PRBool *aCanHandleContent)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::IsPreferred\n"));
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

NS_IMETHODIMP PhMozEmbedChrome::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand,
						  const char *aWindowTarget, char **aDesiredContentType,
						  PRBool *_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::CanHandleContent\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetLoadCookie(nsISupports * *aLoadCookie)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetLoadCookie\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::SetLoadCookie(nsISupports * aLoadCookie)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetLoadCookie\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetParentContentListener\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetParentContentListener\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDocShellTreeOwner interface

NS_IMETHODIMP PhMozEmbedChrome::FindItemWithName(const PRUnichar *aName,
						  nsIDocShellTreeItem *aRequestor,
						  nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::FindItemWithName\n"));
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  PRInt32 i = 0;
  PRInt32 numBrowsers = sBrowsers->Count();

  for (i = 0; i < numBrowsers; i++)
  {
    PhMozEmbedChrome *chrome = (PhMozEmbedChrome *)sBrowsers->ElementAt(i);
    nsCOMPtr<nsIWebBrowser> webBrowser;
    NS_ENSURE_SUCCESS(chrome->GetWebBrowser(getter_AddRefs(webBrowser)),
		      NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(webBrowser));
    NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

    docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome *, this), _retval);
    if (*_retval)
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::ContentShellAdded(nsIDocShellTreeItem *aContentShell,
						   PRBool aPrimary,
						   const PRUnichar *aID)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::ContentShellAdded\n"));
  if (aPrimary)
    mContentShell = aContentShell;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetPrimaryContentShell(nsIDocShellTreeItem **aPrimaryContentShell)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetPrimaryContentShell\n"));
  NS_IF_ADDREF(mContentShell);
  *aPrimaryContentShell = mContentShell;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SizeShellTo(nsIDocShellTreeItem *shell,
					     PRInt32 cx, PRInt32 cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SizeShellTo\n"));
  if (mChromeListener)
    mChromeListener->SizeTo(cx, cy);
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::ShowModal(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::ShowModal\n"));
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::ExitModalLoop(nsresult aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::ExitModalLoop\n"));
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetNewWindow(PRInt32 aChromeFlags,
					      nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetNewWindow\n"));
  if (mChromeListener)
    return mChromeListener->NewBrowser(aChromeFlags, _retval);
  else
    return NS_ERROR_FAILURE;
}

// nsIBaseWindow interface

NS_IMETHODIMP PhMozEmbedChrome::InitWindow(nativeWindow parentNativeWindow,
					    nsIWidget * parentWidget, 
					    PRInt32 x, PRInt32 y,
					    PRInt32 cx, PRInt32 cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::InitWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::Create(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::Create\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::Destroy(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::Destory\n"));
  if (mChromeListener)
    mChromeListener->Destroy();
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SetPosition(PRInt32 x, PRInt32 y)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetPosition\n"));
  mBounds.x = x;
  mBounds.y = y;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetPosition(PRInt32 *x, PRInt32 *y)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetPosition\n"));
  NS_ENSURE_ARG_POINTER(x);
  NS_ENSURE_ARG_POINTER(y);
  *x = mBounds.x;
  *y = mBounds.y;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetSize\n"));
  mBounds.width = cx;
  mBounds.height = cy;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetSize(PRInt32 *cx, PRInt32 *cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetSize\n"));
  NS_ENSURE_ARG_POINTER(cx);
  NS_ENSURE_ARG_POINTER(cy);
  *cx = mBounds.width;
  *cy = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SetPositionAndSize(PRInt32 x, PRInt32 y,
						    PRInt32 cx, PRInt32 cy,
						    PRBool fRepaint)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetPositionAndSize %d %d %d %d\n",
				    x, y, cx, cy));
  mBounds.x = x;
  mBounds.y = y;
  mBounds.width = cx;
  mBounds.height = cy;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetPositionAndSize(PRInt32 *x, PRInt32 *y,
						    PRInt32 *cx, PRInt32 *cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetPositionAndSize %d %d %d %d\n",
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

NS_IMETHODIMP PhMozEmbedChrome::Repaint(PRBool force)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedCHrome::Repaint\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetParentWidget(nsIWidget * *aParentWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetParentWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::SetParentWidget(nsIWidget * aParentWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetParentWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetParentNativeWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP PhMozEmbedChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetParentNativeWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetVisibility(PRBool *aVisibility)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetVisibility\n"));
  NS_ENSURE_ARG_POINTER(aVisibility);
  *aVisibility = mVisibility;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::SetVisibility(PRBool aVisibility)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetVisibility for %p\n", this));
  if (mChromeListener)
    mChromeListener->Visibility(aVisibility);
  mVisibility = aVisibility;
  return NS_OK;
}

NS_IMETHODIMP PhMozEmbedChrome::GetMainWidget(nsIWidget * *aMainWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetMainWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::SetFocus(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetFocus\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::FocusAvailable(nsIBaseWindow *aCurrentFocus, 
						PRBool *aTookFocus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::FocusAvailable\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP PhMozEmbedChrome::GetTitle(PRUnichar * *aTitle)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::GetTitle\n"));
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = nsnull;
  if (mTitle)
    *aTitle = mTitleUnicode.ToNewUnicode();
  return NS_OK;
}
 
NS_IMETHODIMP PhMozEmbedChrome::SetTitle(const PRUnichar * aTitle)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("PhMozEmbedChrome::SetTitle\n"));
  nsString newTitleString(aTitle);
  mTitleUnicode = aTitle;
  mTitle = newTitleString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("title is %s\n", (const char *)mTitle));
  // let the listener know that the title has changed
  if (mChromeListener)
    mChromeListener->Message(PhEmbedListener::MessageTitle, mTitle);
  return NS_OK;
}

NS_IMETHODIMP
PhMozEmbedChrome::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                  PRBool aPersistCX, PRBool aPersistCY,
                                  PRBool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PhMozEmbedChrome::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                  PRBool* aPersistCX, PRBool* aPersistCY,
                                  PRBool* aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
