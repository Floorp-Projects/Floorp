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
#include "nsCWebBrowser.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "prlog.h"

// this is a define to make sure that we don't call certain function
// before the object has been properly initialized

#define CHECK_IS_INIT()                                \
        PR_BEGIN_MACRO                                 \
          if (!mIsInitialized)                         \
            return NS_ERROR_NOT_INITIALIZED;           \
        PR_END_MACRO

static PRLogModuleInfo *mozEmbedLm = NULL;

// constructor and destructor
GtkMozEmbedChrome::GtkMozEmbedChrome()
{
  NS_INIT_REFCNT();
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
  mBounds.x         = 0;
  mBounds.y         = 0;
  mBounds.width     = 0;
  mBounds.height    = 0;
  mVisibility       = PR_FALSE;
  mLinkMessage      = NULL;
  mJSStatus         = NULL;
  mLocation         = NULL;
  mTitle            = NULL;
  if (!mozEmbedLm)
    mozEmbedLm = PR_NewLogModule("GtkMozEmbedChrome");
}

GtkMozEmbedChrome::~GtkMozEmbedChrome()
{
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
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

// nsIGtkEmbed interface

NS_IMETHODIMP GtkMozEmbedChrome::Init(GtkWidget *aOwningWidget)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::Init\n"));
  mOwningGtkWidget = aOwningWidget;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetNewBrowserCallback(GtkMozEmbedChromeCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetNewBrowserCallback\n"));
  mNewBrowserCB = aCallback;
  mNewBrowserCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetDestroyCallback(GtkMozEmbedDestroyCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetDestroyCallback\n"));
  mDestroyCB = aCallback;
  mDestroyCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetVisibilityCallback(GtkMozEmbedVisibilityCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetVisibilityCallback\n"));
  mVisibilityCB = aCallback;
  mVisibilityCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetLinkChangeCallback (GtkMozEmbedLinkCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetLinkChangeCallback\n"));
  mLinkCB = aCallback;
  mLinkCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetJSStatusChangeCallback (GtkMozEmbedJSStatusCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetJSStatusChangeCallback\n"));
  mJSStatusCB = aCallback;
  mJSStatusCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetLocationChangeCallback (GtkMozEmbedLocationCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetLocationChangeCallback\n"));
  mLocationCB = aCallback;
  mLocationCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetTitleChangeCallback (GtkMozEmbedTitleCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetTitleChangeCB\n"));
  mTitleCB = aCallback;
  mTitleCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetProgressCallback (GtkMozEmbedProgressCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetProgressCallback\n"));
  mProgressCB = aCallback;
  mProgressCBData = aData;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetNetCallback (GtkMozEmbedNetCB *aCallback, void *aData)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetNetCallback\n"));
  mNetCB = aCallback;
  mNetCBData = aData;
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
    *retval = nsCRT::strdup(mLinkMessage);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetLocation (char **retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetLocation\n"));
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
  // let any listeners know that the status has changed
  if (mJSStatusCB)
    mJSStatusCB(mJSStatusCBData);
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
  // if there are any listeners, let them know about the change
  if (mLinkCB)
    mLinkCB(mLinkCBData);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetWebBrowser\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetWebBrowser\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetChromeMask(PRUint32 *aChromeMask)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetChromeMask\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetChromeMask(PRUint32 aChromeMask)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SetChromeMask\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetNewBrowser(PRUint32 chromeMask, 
					       nsIWebBrowser **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::GetNewBrowser\n"));
  NS_ENSURE_STATE(mNewBrowserCB);
  if (mNewBrowserCB)
    return mNewBrowserCB(chromeMask, _retval, mNewBrowserCBData);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GtkMozEmbedChrome::FindNamedBrowserItem(const PRUnichar *aName, 
						      nsIDocShellTreeItem **_retval)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::FindNamedBrowserItem\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::SizeBrowserTo\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::ShowAsModal(void)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::ShowAsModal\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIURIContentListener

NS_IMETHODIMP GtkMozEmbedChrome::OnStartURIOpen(nsIURI *aURI, const char *aWindowTarget, PRBool *aAbortOpen)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnStartURIOpen\n"));
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

// nsIWebProgressListener

NS_IMETHODIMP GtkMozEmbedChrome::OnProgressChange(nsIChannel *channel, PRInt32 curSelfProgress,
						  PRInt32 maxSelfProgress, PRInt32 curTotalProgress,
						  PRInt32 maxTotalProgress)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnProgressChange\n"));
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("maxTotalProgress is %d and curTotalProgress is %d\n",
				    maxTotalProgress, curTotalProgress));
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
    mProgressCB(mProgressCBData, maxTotalProgress, curTotalProgress);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::OnChildProgressChange(nsIChannel *channel, PRInt32 curChildProgress,
						       PRInt32 maxChildProgress)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnChildProgressChange\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::OnStatusChange(nsIChannel *channel, PRInt32 aStatus)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnStatusChange\n"));
  if (aStatus & nsIWebProgress::flag_net_start)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_start\n"));
  if (aStatus & nsIWebProgress::flag_net_dns)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_dns\n"));
  if (aStatus & nsIWebProgress::flag_net_connecting)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_connecting\n"));
  if (aStatus & nsIWebProgress::flag_net_redirecting)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_redirecting\n"));
  if (aStatus & nsIWebProgress::flag_net_negotiating)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_negotiating\n"));
  if (aStatus & nsIWebProgress::flag_net_transferring)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_transferring\n"));
  if (aStatus & nsIWebProgress::flag_net_failedDNS)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_failedDNS\n"));
  if (aStatus & nsIWebProgress::flag_net_failedConnect)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_failedConnect\n"));
  if (aStatus & nsIWebProgress::flag_net_failedTransfer)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_failedTransfer\n"));
  if (aStatus & nsIWebProgress::flag_net_failedTimeout)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_failedTimeout\n"));
  if (aStatus & nsIWebProgress::flag_net_userCancelled)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_net_userCancelled\n"));
  if (aStatus & nsIWebProgress::flag_win_start)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_win_start\n"));
  if (aStatus & nsIWebProgress::flag_win_stop)
    PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("flag_win_stop\n"));

  // if we have a callback registered, call it
  if (mNetCB)
    mNetCB(mNetCBData, aStatus);
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedChrome::OnChildStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnChildStatusChange\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::OnLocationChange(nsIURI *aLocation)
{
  PR_LOG(mozEmbedLm, PR_LOG_DEBUG, ("GtkMozEmbedChrome::OnLocationChange\n"));
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
  return NS_ERROR_NOT_IMPLEMENTED;
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
  if (mVisibilityCB)
    mVisibilityCB(aVisibility, mVisibilityCBData);
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
  // if there's a callback, call it to let it know that the title has
  // changed.
  if (mTitleCB)
    mTitleCB(mTitleCBData);
  return NS_OK;
}



