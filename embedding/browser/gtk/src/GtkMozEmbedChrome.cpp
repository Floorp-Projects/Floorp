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

#include "GtkMozEmbedChrome.h"
#include "GtkMozEmbedStream.h"
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

nsVoidArray *GtkMozEmbedChrome::sBrowsers = NULL;

// constructor and destructor
GtkMozEmbedChrome::GtkMozEmbedChrome()
{
  NS_INIT_REFCNT();
  mOwningGtkWidget  = nsnull;
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
    mozEmbedLm = PR_NewLogModule("GtkMozEmbedChrome");
  if (!sBrowsers)
    sBrowsers = new nsVoidArray();
  sBrowsers->AppendElement((void *)this);
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GtkMozEmbedChrome %p\n", this));
}

GtkMozEmbedChrome::~GtkMozEmbedChrome()
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::~GtkMozEmbedChrome %p\n", this));
  sBrowsers->RemoveElement((void *)this);
}

// nsISupports interface

NS_IMPL_ADDREF(GtkMozEmbedChrome)
NS_IMPL_RELEASE(GtkMozEmbedChrome)

NS_INTERFACE_MAP_BEGIN(GtkMozEmbedChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGtkEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIGtkEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

// nsIGtkEmbed interface

NS_IMETHODIMP GtkMozEmbedChrome::Init(GtkWidget *aOwningWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::Init\n"));
  mOwningGtkWidget = aOwningWidget;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetEmbedListener(GtkEmbedListener *aListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetEmbedListener\n"));
  // This listener isn't a refcnted object.  It's assumed that the
  // listener is the owner and will be destroyed after this object is.
  mChromeListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetLinkMessage (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetLinkMessage\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mLinkMessage)
    *retval = nsCRT::strdup(mLinkMessage);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetJSStatus (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetJSStatus\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mJSStatus)
    *retval = nsCRT::strdup(mJSStatus);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetTitleChar (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetTitleChar\n"));
  NS_ENSURE_ARG_POINTER(retval);
  *retval = NULL;
  if (mTitle)
    *retval = nsCRT::strdup(mTitle);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::OpenStream (const char *aBaseURI, const char *aContentType)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_ARG_POINTER(aContentType);

  nsresult rv = NS_OK;
  GtkMozEmbedStream *newStream = nsnull;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;
  nsCOMPtr<nsIDocShell> docShell;
  nsCOMPtr<nsIContentViewerContainer> viewerContainer;
  nsCOMPtr<nsIContentViewer> contentViewer;
  nsCOMPtr<nsIURI> uri;
  nsCAutoString docLoaderProgID;
  nsCAutoString spec(aBaseURI);
  
  // check to see if we need to close the current stream
  if (mDoingStream)
    CloseStream();
  mDoingStream = PR_TRUE;
  // create our new stream object
  newStream = new GtkMozEmbedStream();
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

  docLoaderProgID  = NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX;
  docLoaderProgID += "view";
  docLoaderProgID += "/";
  docLoaderProgID += aContentType;

  docLoaderFactory = do_CreateInstance(docLoaderProgID, &rv);
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

NS_IMETHODIMP GtkMozEmbedChrome::AppendToStream (const char *aData, gint32 aLen)
{
  nsIInputStream *inputStream;
  inputStream = mStream.get();
  GtkMozEmbedStream *embedStream = (GtkMozEmbedStream *)inputStream;
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

NS_IMETHODIMP GtkMozEmbedChrome::CloseStream (void)
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

NS_IMETHODIMP GtkMozEmbedChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetInterface\n"));
  return QueryInterface(aIID, aInstancePtr);
}

// nsIWebBrowserChrome interface

NS_IMETHODIMP GtkMozEmbedChrome::SetJSStatus(const PRUnichar *status)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetJSStatus\n"));
  nsString jsStatusString(status);
  mJSStatus = jsStatusString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("js status is %s\n", (const char *)mJSStatus));
  // let our chrome listener know that the JS message has changed.
  if (mChromeListener)
    mChromeListener->Message(GtkEmbedListener::MessageJSStatus, mJSStatus);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetJSDefaultStatus(const PRUnichar *status)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetJSDefaultStatus\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetOverLink(const PRUnichar *link)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetOverLink\n"));
  nsString linkMessageString(link);
  mLinkMessage = linkMessageString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("message is %s\n", (const char *)mLinkMessage));
  // notify the chrome listener that the link message has changed
  if (mChromeListener)
    mChromeListener->Message(GtkEmbedListener::MessageLink, mLinkMessage);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetWebBrowser\n"));

  NS_ENSURE_ARG_POINTER(aWebBrowser);
  *aWebBrowser = mWebBrowser;
  NS_IF_ADDREF(*aWebBrowser);

  return NS_OK;

}

NS_IMETHODIMP GtkMozEmbedChrome::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetWebBrowser\n"));

  NS_ENSURE_ARG_POINTER(aWebBrowser);
  mWebBrowser = aWebBrowser;

  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetChromeMask(PRUint32 *aChromeMask)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetChromeMask\n"));
  NS_ENSURE_ARG_POINTER(aChromeMask);
  *aChromeMask = mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetChromeMask(PRUint32 aChromeMask)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetChromeMask\n"));
  mChromeMask = aChromeMask;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetNewBrowser(PRUint32 chromeMask, 
					       nsIWebBrowser **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetNewBrowser\n"));
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GtkMozEmbedChrome::FindNamedBrowserItem(const PRUnichar *aName, 
						      nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::FindNamedBrowserItem\n"));
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  PRInt32 i = 0;
  PRInt32 numBrowsers = sBrowsers->Count();

  for (i = 0; i < numBrowsers; i++)
  {
    GtkMozEmbedChrome *chrome = (GtkMozEmbedChrome *)sBrowsers->ElementAt(i);
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

NS_IMETHODIMP GtkMozEmbedChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SizeBrowserTo %d %d\n", aCX, aCY));
  if (mChromeListener)
    mChromeListener->SizeTo(aCX, aCY);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::ShowAsModal(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ShowAsModal\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::ExitModalEventLoop(nsresult aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ExitModalLoop\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIURIContentListener

NS_IMETHODIMP GtkMozEmbedChrome::OnStartURIOpen(nsIURI *aURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnStartURIOpen\n"));
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

NS_IMETHODIMP GtkMozEmbedChrome::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetProtocolHandler\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::DoContent(const char *aContentType, nsURILoadCommand aCommand,
					   const char *aWindowTarget, nsIChannel *aOpenedChannel,
					   nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::DoContent\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::IsPreferred(const char *aContentType, nsURILoadCommand aCommand,
					     const char *aWindowTarget, char **aDesiredContentType,
					     PRBool *aCanHandleContent)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::IsPreferred\n"));
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

NS_IMETHODIMP GtkMozEmbedChrome::CanHandleContent(const char *aContentType, nsURILoadCommand aCommand,
						  const char *aWindowTarget, char **aDesiredContentType,
						  PRBool *_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::CanHandleContent\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetLoadCookie(nsISupports * *aLoadCookie)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetLoadCookie\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetLoadCookie(nsISupports * aLoadCookie)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetLoadCookie\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetParentContentListener\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetParentContentListener\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDocShellTreeOwner interface

NS_IMETHODIMP GtkMozEmbedChrome::FindItemWithName(const PRUnichar *aName,
						  nsIDocShellTreeItem *aRequestor,
						  nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::FindItemWithName\n"));
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  PRInt32 i = 0;
  PRInt32 numBrowsers = sBrowsers->Count();

  for (i = 0; i < numBrowsers; i++)
  {
    GtkMozEmbedChrome *chrome = (GtkMozEmbedChrome *)sBrowsers->ElementAt(i);
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

NS_IMETHODIMP GtkMozEmbedChrome::ContentShellAdded(nsIDocShellTreeItem *aContentShell,
						   PRBool aPrimary,
						   const PRUnichar *aID)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ContentShellAdded\n"));
  if (aPrimary)
    mContentShell = aContentShell;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetPrimaryContentShell(nsIDocShellTreeItem **aPrimaryContentShell)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetPrimaryContentShell\n"));
  NS_IF_ADDREF(mContentShell);
  *aPrimaryContentShell = mContentShell;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SizeShellTo(nsIDocShellTreeItem *shell,
					     PRInt32 cx, PRInt32 cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SizeShellTo\n"));
  if (mChromeListener)
    mChromeListener->SizeTo(cx, cy);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::ShowModal(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ShowModal\n"));
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::ExitModalLoop(nsresult aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ExitModalLoop\n"));
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetNewWindow(PRInt32 aChromeFlags,
					      nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetNewWindow\n"));
  if (mChromeListener)
    return mChromeListener->NewBrowser(aChromeFlags, _retval);
  else
    return NS_ERROR_FAILURE;
}

// nsIBaseWindow interface

NS_IMETHODIMP GtkMozEmbedChrome::InitWindow(nativeWindow parentNativeWindow,
					    nsIWidget * parentWidget, 
					    PRInt32 x, PRInt32 y,
					    PRInt32 cx, PRInt32 cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::InitWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::Create(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::Create\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::Destroy(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::Destory\n"));
  if (mChromeListener)
    mChromeListener->Destroy();
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetPosition(PRInt32 x, PRInt32 y)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetPosition\n"));
  mBounds.x = x;
  mBounds.y = y;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetPosition(PRInt32 *x, PRInt32 *y)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetPosition\n"));
  NS_ENSURE_ARG_POINTER(x);
  NS_ENSURE_ARG_POINTER(y);
  *x = mBounds.x;
  *y = mBounds.y;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetSize\n"));
  mBounds.width = cx;
  mBounds.height = cy;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetSize(PRInt32 *cx, PRInt32 *cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetSize\n"));
  NS_ENSURE_ARG_POINTER(cx);
  NS_ENSURE_ARG_POINTER(cy);
  *cx = mBounds.width;
  *cy = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetPositionAndSize(PRInt32 x, PRInt32 y,
						    PRInt32 cx, PRInt32 cy,
						    PRBool fRepaint)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetPositionAndSize %d %d %d %d\n",
				    x, y, cx, cy));
  mBounds.x = x;
  mBounds.y = y;
  mBounds.width = cx;
  mBounds.height = cy;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetPositionAndSize(PRInt32 *x, PRInt32 *y,
						    PRInt32 *cx, PRInt32 *cy)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetPositionAndSize %d %d %d %d\n",
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

NS_IMETHODIMP GtkMozEmbedChrome::Repaint(PRBool force)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedCHrome::Repaint\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetParentWidget(nsIWidget * *aParentWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetParentWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetParentWidget(nsIWidget * aParentWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetParentWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetParentNativeWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP GtkMozEmbedChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetParentNativeWindow\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetVisibility(PRBool *aVisibility)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetVisibility\n"));
  NS_ENSURE_ARG_POINTER(aVisibility);
  *aVisibility = mVisibility;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetVisibility(PRBool aVisibility)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetVisibility for %p\n", this));
  if (mChromeListener)
    mChromeListener->Visibility(aVisibility);
  mVisibility = aVisibility;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetMainWidget(nsIWidget * *aMainWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetMainWidget\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetFocus(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetFocus\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::FocusAvailable(nsIBaseWindow *aCurrentFocus, 
						PRBool *aTookFocus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::FocusAvailable\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetTitle(PRUnichar * *aTitle)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetTitle\n"));
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = nsnull;
  if (mTitle)
    *aTitle = mTitleUnicode.ToNewUnicode();
  return NS_OK;
}
 
NS_IMETHODIMP GtkMozEmbedChrome::SetTitle(const PRUnichar * aTitle)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetTitle\n"));
  nsString newTitleString(aTitle);
  mTitleUnicode = aTitle;
  mTitle = newTitleString.ToNewCString();
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("title is %s\n", (const char *)mTitle));
  // let the listener know that the title has changed
  if (mChromeListener)
    mChromeListener->Message(GtkEmbedListener::MessageTitle, mTitle);
  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedChrome::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                  PRBool aPersistCX, PRBool aPersistCY,
                                  PRBool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GtkMozEmbedChrome::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                  PRBool* aPersistCX, PRBool* aPersistCY,
                                  PRBool* aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
