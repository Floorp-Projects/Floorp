/*
 * vim:ts=2:et:sw=2
 *
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
 *   Brian Edmond <briane@qnx.com>
 */
#include <nsIDocShell.h>
#include <nsIWebProgress.h>
#include <nsIURI.h>
#include "nsIWidget.h"

// for do_GetInterface
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIWebBrowserPersist.h>
// for do_CreateInstance
#include <nsIComponentManager.h>
#include "nsIWebProgressListener.h"
#include "nsIDOMHTMLImageElement.h"

#include <nsCWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIDocShellTreeItem.h>
#include <nsILocalFile.h>
#include <nsString.h>
#include "nsReadableUtils.h"

#include "EmbedWindow.h"
#include "EmbedPrivate.h"

#include "PtMozilla.h"

PtWidget_t *EmbedWindow::sTipWindow = nsnull;

EmbedWindow::EmbedWindow(void)
{
	NS_INIT_ISUPPORTS();
  mOwner       = nsnull;
  mVisibility  = PR_FALSE;
  mIsModal     = PR_FALSE;
}

EmbedWindow::~EmbedWindow(void)
{
  ExitModalEventLoop(PR_FALSE);
}

nsresult
EmbedWindow::Init(EmbedPrivate *aOwner)
{
  // save our owner for later
  mOwner = aOwner;

  // create our nsIWebBrowser object and set up some basic defaults.
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;

  mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, this));
  
	// get the doc shel for cut/copy/paste
	//mRootDocShell = do_GetInterface(mWebBrowser);

  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(mWebBrowser);
  item->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  return NS_OK;
}

nsresult
EmbedWindow::CreateWindow(void)
{
  nsresult rv;
  PtWidget_t *ownerAsWidget = (PtWidget_t *)(mOwner->mOwningWidget);

  // Get the base window interface for the web browser object and
  // create the window.
  mBaseWindow = do_QueryInterface(mWebBrowser);
  rv = mBaseWindow->InitWindow(ownerAsWidget,
			       nsnull,
			       0, 0, 
			       ownerAsWidget->area.size.w,
			       ownerAsWidget->area.size.h);
  if (NS_FAILED(rv))
    return rv;

  rv = mBaseWindow->Create();
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

void
EmbedWindow::ReleaseChildren(void)
{
  ExitModalEventLoop(PR_FALSE);
    
  mBaseWindow->Destroy();
  mBaseWindow = 0;
  mWebBrowser = 0;
}

// nsISupports

NS_IMPL_ADDREF(EmbedWindow)
NS_IMPL_RELEASE(EmbedWindow)

NS_INTERFACE_MAP_BEGIN(EmbedWindow)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
  NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
  NS_INTERFACE_MAP_ENTRY(nsIDNSListener)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

// nsIWebBrowserChrome

NS_IMETHODIMP
EmbedWindow::SetStatus(PRUint32 aStatusType, const PRUnichar *aStatus)
{
  PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaInfoCb_t 	info;
  nsAutoString Text ( aStatus );
  int       type = 0;

  switch (aStatusType)
  {
      case STATUS_SCRIPT:
          type = Pt_MOZ_INFO_JSSTATUS;
          break;
      case STATUS_SCRIPT_DEFAULT:
          return NS_OK;
          break;
      case STATUS_LINK:
          type = Pt_MOZ_INFO_LINK;
          break;
      default:
          return NS_OK;
          break;
  }

  if (!moz->info_cb)
    return NS_OK;

  memset(&cbinfo, 0, sizeof(cbinfo));
  cbinfo.cbdata = &info;
  cbinfo.reason = Pt_CB_MOZ_INFO;
  cb = moz->info_cb;

  info.type = type;
  info.status = 0;
  const char* status = ToNewCString(Text);
  info.data = (char *)status;
  PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

  nsMemory::Free( (void*)status );

  return NS_OK;
}

int
EmbedWindow::SaveAs(char *fname, char *dirname)
{
	nsresult rv;

	nsCOMPtr<nsIWebBrowserPersist> persist(do_GetInterface(mWebBrowser, &rv));
	if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILocalFile> file;
  NS_NewNativeLocalFile(nsDependentCString(fname), PR_TRUE, getter_AddRefs(file));

	/* the nsIWebBrowserPersist wants a parent directory where to download images and other items related to the document */
	/* we have to provide a directory, otherwise the current SaveDocument is marked as not finished and the next one is aborted */
	nsCOMPtr<nsILocalFile> parentDirAsLocal;
	rv = NS_NewNativeLocalFile(nsDependentCString( dirname ), PR_TRUE, getter_AddRefs(parentDirAsLocal));
	if (NS_FAILED(rv)) return rv;

	PRUint32 flags;
	persist->GetPersistFlags( &flags );
	if( !(flags & nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES ) ) persist->SetPersistFlags( nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES );

  persist->SaveDocument(nsnull, file, parentDirAsLocal, nsnull, 0, 0);
  return 0;
}

NS_IMETHODIMP
EmbedWindow::GetWebBrowser(nsIWebBrowser **aWebBrowser)
{
  *aWebBrowser = mWebBrowser;
  NS_IF_ADDREF(*aWebBrowser);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetWebBrowser(nsIWebBrowser *aWebBrowser)
{
  mWebBrowser = aWebBrowser;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetChromeFlags(PRUint32 *aChromeFlags)
{
  *aChromeFlags = mOwner->mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetChromeFlags(PRUint32 aChromeFlags)
{
  mOwner->mChromeMask = aChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::DestroyBrowserWindow(void)
{
  PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
  PtCallbackList_t  *cb;
  PtCallbackInfo_t  cbinfo;

  if (!moz->destroy_cb)
      return NS_OK;

  cb = moz->destroy_cb;
  memset(&cbinfo, 0, sizeof(cbinfo));
  cbinfo.reason = Pt_CB_MOZ_DESTROY;
  PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
	PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
	PtCallbackList_t  *cb;
	PtCallbackInfo_t  cbinfo;
	PtMozillaNewAreaCb_t resize;

	if (!moz->resize_cb)
		return NS_OK;

	cb = moz->resize_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_NEW_AREA;
	resize.flags = Pt_MOZ_NEW_AREA_SET_SIZE;
	resize.area.size.w = aCX;
	resize.area.size.h = aCY;
	cbinfo.cbdata = &resize;
	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ShowAsModal(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedWindow::IsWindowModal(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ExitModalEventLoop(nsresult aStatus)
{
  return NS_OK;
}

// nsIWebBrowserChromeFocus

NS_IMETHODIMP
EmbedWindow::FocusNextElement()
{
  PtContainerFocusNext(mOwner->mOwningWidget, NULL);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::FocusPrevElement()
{
  PtContainerFocusPrev(mOwner->mOwningWidget, NULL);
  return NS_OK;
}

// nsIEmbeddingSiteWindow

NS_IMETHODIMP
EmbedWindow::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY,
			   PRInt32 aCX, PRInt32 aCY)
{
  PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
  PtCallbackList_t  *cb;
  PtCallbackInfo_t  cbinfo;
  PtMozillaNewAreaCb_t  resize;

  nsresult  rv = NS_ERROR_INVALID_ARG;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
     nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) {
    rv =  mBaseWindow->SetPositionAndSize(aX, aY, aCX, aCY, PR_TRUE);
  }
  else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    rv =  mBaseWindow->SetPosition(aX, aY);
  }
  else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
         nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
    rv =  mBaseWindow->SetSize(aCX, aCY, PR_TRUE);
  }
	return rv;

  if (!moz->resize_cb)
      return NS_OK;

  cb = moz->resize_cb;
  memset(&cbinfo, 0, sizeof(cbinfo));
  cbinfo.reason = Pt_CB_MOZ_NEW_AREA;
  cbinfo.cbdata = &resize;

  memset(&resize, 0, sizeof(PtMozillaNewAreaCb_t));

  if( aCX==0 && aCY==0 )
    resize.flags = Pt_MOZ_NEW_AREA_SET_POSITION;
  else 
    resize.flags = Pt_MOZ_NEW_AREA_SET_AREA;

  resize.area.pos.x = aX;
  resize.area.pos.y = aY;
  resize.area.size.w = aCX;
  resize.area.size.h = aCY;

  PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetDimensions(PRUint32 aFlags, PRInt32 *aX,
			   PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY)
{
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
		 nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) {
    return mBaseWindow->GetPositionAndSize(aX, aY, aCX, aCY);
  }
  else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    return mBaseWindow->GetPosition(aX, aY);
  }
  else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
		     nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
    return mBaseWindow->GetSize(aCX, aCY);
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
EmbedWindow::SetFocus(void)
{
  // XXX might have to do more here.
  return mBaseWindow->SetFocus();
}

NS_IMETHODIMP
EmbedWindow::GetTitle(PRUnichar **aTitle)
{
  *aTitle = ToNewUnicode(mTitle);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetTitle(const PRUnichar *aTitle)
{
  PtMozillaWidget_t   *moz = (PtMozillaWidget_t *)mOwner->mOwningWidget;
  PtCallbackList_t  *cb;
  PtCallbackInfo_t  cbinfo;
  PtMozillaInfoCb_t   info;
  nsString mTitleString(aTitle);
  char *str;

  mTitle = aTitle;

  if (!moz->info_cb)
    return NS_OK;

  memset(&cbinfo, 0, sizeof(cbinfo));
  cbinfo.cbdata = &info;
  cbinfo.reason = Pt_CB_MOZ_INFO;
  cb = moz->info_cb;

  info.type = Pt_MOZ_INFO_TITLE;
  info.status = 0;

	/* see if the title is empty */
	if( mTitleString.Length() == 0 ) {
		if( moz->EmbedRef->mURI.Length() > 0 )
			str = ToNewCString( moz->EmbedRef->mURI );
		else {
			str = (char*) nsMemory::Alloc( 2 );
			str[0] = ' '; str[1] = 0;
			}
		}
  else str = ToNewCString(mTitleString);

  info.data = str;
  PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

  nsMemory::Free( (void*)str );

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetSiteWindow(void **aSiteWindow)
{
  PtWidget_t *ownerAsWidget = (PtWidget_t *)(mOwner->mOwningWidget);
  *aSiteWindow = NS_STATIC_CAST(void *, ownerAsWidget);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetVisibility(PRBool *aVisibility)
{
  *aVisibility = mVisibility;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetVisibility(PRBool aVisibility)
{
  mVisibility = aVisibility;
  return NS_OK;
}

// nsITooltipListener

NS_IMETHODIMP
EmbedWindow::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords,
			   const PRUnichar *aTipText)
{
  nsAutoString tipText ( aTipText );
  const char* tipString = ToNewCString(tipText), *font = "TextFont08";
  PtArg_t args[10];
  PhRect_t extent;
  PhDim_t dim;
  PhPoint_t pos = {0, 0};
  int n = 0, w, h;

  if (sTipWindow)
    PtDestroyWidget(sTipWindow);
  
  // get the root origin for this content window
  nsCOMPtr<nsIWidget> mainWidget;
  mBaseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  PtWidget_t *window;
  window = NS_STATIC_CAST(PtWidget_t *, mainWidget->GetNativeData(NS_NATIVE_WINDOW));

  PgExtentText(&extent, &pos, font, tipString, 0);
  w = extent.lr.x - extent.ul.x + 1;
  h = extent.lr.y - extent.ul.y + 1;

  n = 0;
  pos.x = aXCoords;
  pos.y = aYCoords + 10; /* we add 10 so that we don't position it right under the mouse */
	dim.w = w + 6; dim.h = h + 6;
  PtSetArg(&args[n++], Pt_ARG_POS, &pos, 0);
  PtSetArg(&args[n++], Pt_ARG_DIM, &dim, 0);
	PtSetArg( &args[n++], Pt_ARG_REGION_OPAQUE,   Ph_EV_EXPOSE, Ph_EV_EXPOSE);
  sTipWindow = PtCreateWidget(PtRegion, Pt_NO_PARENT, n, args);

  n = 0;
  pos.x = pos.y = 0;
	dim.w = w; dim.h = h;
  PtSetArg(&args[n++], Pt_ARG_POS, &pos, 0);
  PtSetArg(&args[n++], Pt_ARG_DIM, &dim, 0);
  PtSetArg(&args[n++], Pt_ARG_FLAGS, Pt_HIGHLIGHTED, -1 );
  PtSetArg(&args[n++], Pt_ARG_FILL_COLOR, 0xfeffb1, 0);
  PtSetArg(&args[n++], Pt_ARG_TEXT_FONT, font, 0);
  PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, tipString, 0);
  PtSetArg(&args[n++], Pt_ARG_BASIC_FLAGS, Pt_STATIC_GRADIENT | Pt_TOP_OUTLINE | Pt_LEFT_OUTLINE |
      Pt_RIGHT_OUTLINE | Pt_BOTTOM_OUTLINE, -1 );
  PtCreateWidget(PtLabel, sTipWindow, n, args);

  // realize the widget
  PtRealizeWidget(sTipWindow);

  nsMemory::Free( (void*)tipString );

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::OnHideTooltip(void)
{
  if (sTipWindow)
    PtDestroyWidget(sTipWindow);
  sTipWindow = NULL;
  return NS_OK;
}


// nsIDNSListener

#if 0

/* void OnStartLookup (in nsISupports ctxt, in string hostname); */
NS_IMETHODIMP EmbedWindow::OnStartLookup(nsISupports *ctxt, const char *hostname)
{
    PtMozillaWidget_t       *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
    PtCallbackList_t        *cb = NULL;
    PtCallbackInfo_t        cbinfo;
    PtMozillaNetStateCb_t   state;
    cbinfo.reason = Pt_CB_MOZ_NET_STATE;
    cbinfo.cbdata = &state;
    state.flags = 0;
    state.status = 0;
    state.url = (char *)hostname;
    char *statusMessage = "Resolving host name:";
    state.message = statusMessage;
    if( ( cb = moz->net_state_cb ) )
        PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

    return NS_OK;
}

/* [noscript] void OnFound (in nsISupports ctxt, in string hostname, in nsHostEntStar entry); */
NS_IMETHODIMP EmbedWindow::OnFound(nsISupports *ctxt, const char *hostname, nsHostEnt * entry)
{
    PtMozillaWidget_t       *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
    PtCallbackList_t        *cb = NULL;
    PtCallbackInfo_t        cbinfo;
    PtMozillaNetStateCb_t   state;

    cbinfo.reason = Pt_CB_MOZ_NET_STATE;
    cbinfo.cbdata = &state;
    state.flags = 0;
    state.status = 0;
    state.url = (char *)hostname;
    char *statusMessage = "Opening connection:";
    state.message = statusMessage;
    if( ( cb = moz->net_state_cb ) )
        PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

    return NS_OK;
}
#endif

/* void OnStopLookup (in nsISupports ctxt, in string hostname, in nsresult status); */
NS_IMETHODIMP EmbedWindow::OnLookupComplete(nsIDNSRequest *aRequest, nsIDNSRecord *aRecord, nsresult aStatus)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIContextMenuListener

NS_IMETHODIMP EmbedWindow::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
    PtCallbackList_t    *cb;
    PtCallbackInfo_t    cbinfo;
    PtMozillaContextCb_t    cmenu;

    if (!moz->context_cb)
        return NS_OK;

    cb = moz->context_cb;
    memset(&cbinfo, 0, sizeof(cbinfo));
    cbinfo.reason = Pt_CB_MOZ_CONTEXT;
    cbinfo.cbdata = &cmenu;

    memset(&cmenu, 0, sizeof(PtMozillaContextCb_t));
    if (aContextFlags & CONTEXT_NONE)
        cmenu.flags |= Pt_MOZ_CONTEXT_NONE;
    else if (aContextFlags & CONTEXT_LINK)
        cmenu.flags |= Pt_MOZ_CONTEXT_LINK;
    else if (aContextFlags & CONTEXT_IMAGE)
        cmenu.flags |= Pt_MOZ_CONTEXT_IMAGE;
    else if (aContextFlags & CONTEXT_DOCUMENT)
        cmenu.flags |= Pt_MOZ_CONTEXT_DOCUMENT;
    else if (aContextFlags & CONTEXT_TEXT)
        cmenu.flags |= Pt_MOZ_CONTEXT_TEXT;
    else if (aContextFlags & CONTEXT_INPUT)
        cmenu.flags |= Pt_MOZ_CONTEXT_INPUT;

    nsCOMPtr<nsIDOMMouseEvent> mouseEvent (do_QueryInterface( aEvent ));
    if(!mouseEvent) return NS_OK;
    mouseEvent->GetScreenX( &cmenu.x );
    mouseEvent->GetScreenY( &cmenu.y );

    PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

    if( aContextFlags & CONTEXT_IMAGE )
    {
    		/* store the url we clicked on */
    		nsAutoString rightClickUrl;

        // Get the IMG SRC
        nsresult rv = NS_OK;
        nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(aNode, &rv));
        if(NS_FAILED(rv)) return NS_OK;

        rv = imgElement->GetSrc(rightClickUrl);

    		if( moz->rightClickUrl_image ) free( moz->rightClickUrl_image );

        if(NS_FAILED(rv))  moz->rightClickUrl_image = NULL;
    		else moz->rightClickUrl_image = ToNewCString(rightClickUrl);
    }

		if( aContextFlags & CONTEXT_LINK )
    {
				/* CONTEXT_IMAGE|CONTEXT_LINK is set for an <IMG>  with an <A> as an ancestor */
				if( aContextFlags & CONTEXT_IMAGE ) {
					nsIDOMNode *parent;
					aNode->GetParentNode( &parent );
					if( parent ) aNode = parent;
					}

    		/* store the url we clicked on */
    		nsAutoString rightClickUrl;

        nsresult rv = NS_OK;
        nsCOMPtr<nsIDOMHTMLAnchorElement> linkElement(do_QueryInterface(aNode, &rv));

        if(NS_FAILED(rv)) return NS_OK;

        // Note that this string is in UCS2 format
        rv = linkElement->GetHref( rightClickUrl );

				if( moz->rightClickUrl_link ) free( moz->rightClickUrl_link );

        if(NS_FAILED(rv)) moz->rightClickUrl_link = NULL;
				else moz->rightClickUrl_link = ToNewCString(rightClickUrl);
    }

    return NS_OK;
}


// nsIInterfaceRequestor

NS_IMETHODIMP
EmbedWindow::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  nsresult rv;
  
  rv = QueryInterface(aIID, aInstancePtr);

  // pass it up to the web browser object
  if (NS_FAILED(rv) || !*aInstancePtr) {
    nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(mWebBrowser);
    return ir->GetInterface(aIID, aInstancePtr);
  }

  return rv;
}


