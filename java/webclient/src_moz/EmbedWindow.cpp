/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Christopher Blizzard
 */

#include <nsCWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIDocShellTreeItem.h>
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindow.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMNode.h"
#include "nsIWidget.h"
#include "nsReadableUtils.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerEdit.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIDocShellLoadInfo.h"
#include "nsNetUtil.h" // for NS_NewPostDataStream

#include "NativeBrowserControl.h"
#include "EmbedWindow.h"

#include "jni_util.h"

#include "nsCRT.h"

EmbedWindow::EmbedWindow(void)
{
  mOwner       = nsnull;
  mVisibility  = PR_FALSE;
  mIsModal     = PR_FALSE;
}

EmbedWindow::~EmbedWindow(void)
{
    ExitModalEventLoop(PR_FALSE);
}

nsresult
EmbedWindow::Init(NativeBrowserControl *aOwner)
{
  // save our owner for later
  mOwner = aOwner;

  // create our nsIWebBrowser object and set up some basic defaults.
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;

  mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, this));
  
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(mWebBrowser);
  item->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  return NS_OK;
}

nsresult
EmbedWindow::CreateWindow_(PRUint32 width, PRUint32 height)
{
    nsresult rv;
#ifdef XP_UNIX
    PR_ASSERT(PR_FALSE);
    GtkWidget *ownerAsWidget (GTK_WIDGET(mOwner->parentHWnd));
    width = ownerAsWidget->allocation.width;
    height = ownerAsWidget->allocation.height;
#else 
    HWND ownerAsWidget = mOwner->parentHWnd;
#endif
    
    // Get the base window interface for the web browser object and
    // create the window.
    mBaseWindow = do_QueryInterface(mWebBrowser);
    rv = mBaseWindow->InitWindow(ownerAsWidget,
                                 nsnull,
                                 0, 0, 
                                 width, height);
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

  if (mBaseWindow) { 
      mBaseWindow->Destroy();
  }
  mBaseWindow = 0;
  mWebBrowser = 0;
}

nsresult
EmbedWindow::SelectAll()
{
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebBrowser);
    if (!docShell) {
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIContentViewer> contentViewer;
    nsresult rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
    if (!contentViewer) {
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIContentViewerEdit> contentViewerEdit(do_QueryInterface(contentViewer));
    if (!contentViewerEdit) {
        return NS_ERROR_FAILURE;
    }
    
    rv = contentViewerEdit->SelectAll();
    return rv;
}

nsresult 
EmbedWindow::GetSelection(JNIEnv *env, jobject mSelection)
{
    nsresult rv = NS_ERROR_FAILURE;
    
    // Get the DOM window
    nsCOMPtr<nsIDOMWindow> domWindow;
    rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (NS_FAILED(rv) || !domWindow)  {
        return rv;
    }
    
    // Get the selection object of the DOM window
    nsCOMPtr<nsISelection> selection;
    rv = domWindow->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(rv) || !selection)  {
        return rv;
    }

    // Get the range count
    PRInt32 rangeCount;
    rv = selection->GetRangeCount(&rangeCount);
    if (NS_FAILED(rv) || rangeCount == 0)  {
        return rv;
    }

    // Get the actual selection string
    PRUnichar *selectionStr;
    rv = selection->ToString(&selectionStr);
    if (NS_FAILED(rv))  {
        return rv;
    }
    
    jstring string =
        env->NewString((jchar*)selectionStr, nsCRT::strlen(selectionStr));
    // string is now GC'd by Java
    nsMemory::Free((void *) selectionStr);
    
    // Get the first range object of the selection object
    nsCOMPtr<nsIDOMRange> range;
    rv = selection->GetRangeAt(0, getter_AddRefs(range));
    if (NS_FAILED(rv) || !range)  {
        return rv;
    }
    
    // Get the properties of the range object (startContainer,
    // startOffset, endContainer, endOffset)
    PRInt32 startOffset;
    PRInt32 endOffset;
    nsCOMPtr<nsIDOMNode> startContainer;
    nsCOMPtr<nsIDOMNode> endContainer;

    // start container
    rv = range->GetStartContainer(getter_AddRefs(startContainer));
    if (NS_FAILED(rv))  {
        return rv;
    }
    
    // end container
    rv = range->GetEndContainer(getter_AddRefs(endContainer));
    if (NS_FAILED(rv))  {
        return rv;
    }
    
    // start offset
    rv = range->GetStartOffset(&startOffset);
    if (NS_FAILED(rv))  {
        return rv;
    }
    
    // end offset
    rv = range->GetEndOffset(&endOffset);
    if (NS_FAILED(rv))  {
        return rv;
    }
    
    // get a handle on to acutal (java) Node representing the start
    // and end containers
    jlong node1Long = nsnull;
    jlong node2Long = nsnull;
    
    nsCOMPtr<nsIDOMNode> node1Ptr(do_QueryInterface(startContainer));
    nsCOMPtr<nsIDOMNode> node2Ptr(do_QueryInterface(endContainer));

    if (nsnull == (node1Long = (jlong)node1Ptr.get())) {
        return NS_ERROR_NULL_POINTER;
    }
    if (nsnull == (node2Long = (jlong)node2Ptr.get())) {
        return NS_ERROR_NULL_POINTER;
    }
    
    jclass clazz = nsnull;
    jmethodID mid = nsnull;
    
    if (nsnull == (clazz = ::util_FindClass(env,
                                            "org/mozilla/dom/DOMAccessor"))) {
        return NS_ERROR_NULL_POINTER;
    }
    if (nsnull == (mid = env->GetStaticMethodID(clazz, "getNodeByHandle",
                                                 "(J)Lorg/w3c/dom/Node;"))) {
        return NS_ERROR_NULL_POINTER;
    }
    
    jobject node1 = (jobject) ((void *)::util_CallStaticObjectMethodlongArg(env, clazz, mid, node1Long));
    jobject node2 = (jobject) ((void *)::util_CallStaticObjectMethodlongArg(env, clazz, mid, node2Long));
    
    // prepare the (java) Selection object that is to be returned.
    if (nsnull == (clazz = ::util_FindClass(env, "org/mozilla/webclient/Selection"))) {
        return NS_ERROR_NULL_POINTER;
    }
    
    if (nsnull == (mid = env->GetMethodID(clazz, "init",
                                           "(Ljava/lang/String;Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;II)V"))) {
        return NS_ERROR_NULL_POINTER;
    }
    
    env->CallVoidMethod(mSelection, mid,
                         string, node1, node2,
                         (jint)startOffset, (jint)endOffset);
    
    return NS_OK;
}

nsresult 
EmbedWindow::LoadStream(nsIInputStream *aStream, nsIURI * aURI,
                        const nsACString &aContentType,
                        const nsACString &aContentCharset,
                        nsIDocShellLoadInfo * aLoadInfo) 
{
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebBrowser);
    if (!docShell) {
        return NS_ERROR_FAILURE;
    }
    return docShell->LoadStream(aStream, aURI, aContentType, aContentCharset,
                                aLoadInfo);
}

nsresult 
EmbedWindow::Post(nsIURI *absoluteURL,
                  const PRUnichar *target,
                  PRInt32 targetLength,
                  PRInt32 postDataLength,
                  const char *postData,  
                  PRInt32 postHeadersLength,
                  const char *postHeaders)
{
    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebBrowser);
    nsCOMPtr<nsIDocShellLoadInfo> docShellLoadInfo = nsnull;
    if (!docShell) {
        return rv;
    }

    rv = docShell->CreateLoadInfo(getter_AddRefs(docShellLoadInfo));

    if (NS_FAILED(rv)) {
        return rv;
    }
    
    docShellLoadInfo->SetReferrer(absoluteURL);
    docShellLoadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormalReplace);
    docShellLoadInfo->SetInheritOwner(PR_TRUE);
    docShellLoadInfo->SetTarget(target);

    // create the streams
    nsCOMPtr<nsIInputStream> postDataStream = nsnull;
    nsCOMPtr<nsIInputStream> headersDataStream = nsnull;
    nsCOMPtr<nsIInputStream> inputStream = nsnull;
    
    if (postData) {
        nsCAutoString postDataStr(postData);
        NS_NewPostDataStream(getter_AddRefs(postDataStream),
                             PR_FALSE,
                             postDataStr, 0);
    }
    
    if (postHeaders) {
        NS_NewByteInputStream(getter_AddRefs(inputStream),
                              postHeaders, postHeadersLength);
        if (inputStream) {
            headersDataStream = do_QueryInterface(inputStream, &rv);
        }
        
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    docShellLoadInfo->SetPostDataStream(postDataStream);
    docShellLoadInfo->SetHeadersStream(headersDataStream);

    rv = docShell->LoadURI(absoluteURL, docShellLoadInfo, 
                           nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
    
    return rv;
}

nsresult
EmbedWindow::AddWebBrowserListener(nsIWeakReference *aListener, 
                                   const nsIID & aIID)
{
    return mWebBrowser->AddWebBrowserListener(aListener, aIID);
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
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

// nsIWebBrowserChrome

NS_IMETHODIMP
EmbedWindow::SetStatus(PRUint32 aStatusType, const PRUnichar *aStatus)
{
  switch (aStatusType) {
  case STATUS_SCRIPT: 
    {
      mJSStatus = aStatus;
      // call back to the client if necessary
    }
    break;
  case STATUS_SCRIPT_DEFAULT:
    // Gee, that's nice.
    break;
  case STATUS_LINK:
    {
      mLinkMessage = aStatus;
      // call back to the client if necessary
    }
    break;
  }
  return NS_OK;
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
  // mark the owner as destroyed so it won't emit events anymore.
  mOwner->mIsDestroyed = PR_TRUE;
  // call back to the client if necessary
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
    // PENDING: call back to Java to set the size of the canvas
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ShowAsModal(void)
{
  mIsModal = PR_TRUE;
  /********* PENDING(edburns): implement for win32
  GtkWidget *toplevel;
  toplevel = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
  gtk_grab_add(toplevel);
  gtk_main();
  ************/
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::IsWindowModal(PRBool *_retval)
{
  *_retval = mIsModal;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ExitModalEventLoop(nsresult aStatus)
{
  if (mIsModal) {
  /********* PENDING(edburns): implement for win32
    GtkWidget *toplevel;
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
    gtk_grab_remove(toplevel);
    mIsModal = PR_FALSE;
    gtk_main_quit();
  ***************/
  }
  return NS_OK;
}

// nsIWebBrowserChromeFocus

NS_IMETHODIMP
EmbedWindow::FocusNextElement()
{
  /********* PENDING(edburns): implement for win32
#ifdef MOZ_WIDGET_GTK
  GtkWidget* parent = GTK_WIDGET(mOwner->mOwningWidget)->parent;

  if (GTK_IS_CONTAINER(parent))
    gtk_container_focus(GTK_CONTAINER(parent),
                        GTK_DIR_TAB_FORWARD);
#endif

#ifdef MOZ_WIDGET_GTK2
  GtkWidget *toplevel;
  toplevel = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
  if (!GTK_WIDGET_TOPLEVEL(toplevel))
    return NS_OK;

  g_signal_emit_by_name(G_OBJECT(toplevel), "move_focus",
			GTK_DIR_TAB_FORWARD);
#endif
  ***************/

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::FocusPrevElement()
{
  /********* PENDING(edburns): implement for win32
#ifdef MOZ_WIDGET_GTK
  GtkWidget* parent = GTK_WIDGET(mOwner->mOwningWidget)->parent;

  if (GTK_IS_CONTAINER(parent))
    gtk_container_focus(GTK_CONTAINER(parent),
                        GTK_DIR_TAB_BACKWARD);
#endif

#ifdef MOZ_WIDGET_GTK2
  GtkWidget *toplevel;
  toplevel = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
  if (!GTK_WIDGET_TOPLEVEL(toplevel))
    return NS_OK;

  g_signal_emit_by_name(G_OBJECT(toplevel), "move_focus",
			GTK_DIR_TAB_BACKWARD);
#endif
  ***************/

  return NS_OK;
}

// nsIEmbeddingSiteWindow

NS_IMETHODIMP
EmbedWindow::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY,
			   PRInt32 aCX, PRInt32 aCY)
{
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
		 nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) {
    return mBaseWindow->SetPositionAndSize(aX, aY, aCX, aCY, PR_TRUE);
  }
  else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    return mBaseWindow->SetPosition(aX, aY);
  }
  else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
		     nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
    return mBaseWindow->SetSize(aCX, aCY, PR_TRUE);
  }
  return NS_ERROR_INVALID_ARG;
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
  mTitle = aTitle;
  /********* PENDING(edburns): implement for win32
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[TITLE]);
  ***************/

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetSiteWindow(void **aSiteWindow)
{
#ifdef XP_UNIX
    GtkWidget *ownerAsWidget (GTK_WIDGET(mOwner->parentHWnd));
#else 
    HWND ownerAsWidget = mOwner->parentHWnd;
#endif

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
  // We always set the visibility so that if it's chrome and we finish
  // the load we know that we have to show the window.
  mVisibility = aVisibility;

  // if this is a chrome window and the chrome hasn't finished loading
  // yet then don't show the window yet.
  if (mOwner->mIsChrome && !mOwner->mChromeLoaded)
    return NS_OK;

  /********* PENDING(edburns): implement for win32
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[VISIBILITY],
		  aVisibility);
  ***************/

  return NS_OK;
}

// nsITooltipListener

NS_IMETHODIMP
EmbedWindow::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords,
			   const PRUnichar *aTipText)
{
  nsAutoString tipText ( aTipText );

  /********* PENDING(edburns): implement for win32
#ifdef MOZ_WIDGET_GTK
  const char* tipString = ToNewCString(tipText);
#endif

#ifdef MOZ_WIDGET_GTK2
  const char* tipString = ToNewUTF8String(tipText);
#endif

  if (sTipWindow)
    gtk_widget_destroy(sTipWindow);
  
  // get the root origin for this content window
  nsCOMPtr<nsIWidget> mainWidget;
  mBaseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  GdkWindow *window;
  window = NS_STATIC_CAST(GdkWindow *,
			  mainWidget->GetNativeData(NS_NATIVE_WINDOW));
  gint root_x, root_y;
  gdk_window_get_origin(window, &root_x, &root_y);

  // XXX work around until I can get pink to figure out why
  // tooltips vanish if they show up right at the origin of the
  // cursor.
  root_y += 10;
  
  sTipWindow = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_set_app_paintable(sTipWindow, TRUE);
  gtk_window_set_policy(GTK_WINDOW(sTipWindow), FALSE, FALSE, TRUE);
  // needed to get colors + fonts etc correctly
  gtk_widget_set_name(sTipWindow, "gtk-tooltips");
  
  // set up the popup window as a transient of the widget.
  GtkWidget *toplevel_window;
  toplevel_window = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
  if (!GTK_WINDOW(toplevel_window)) {
    NS_ERROR("no gtk window in hierarchy!\n");
    return NS_ERROR_FAILURE;
  }
  gtk_window_set_transient_for(GTK_WINDOW(sTipWindow),
			       GTK_WINDOW(toplevel_window));
  
  // realize the widget
  gtk_widget_realize(sTipWindow);

  // set up the label for the tooltip
  GtkWidget *label = gtk_label_new(tipString);
  // wrap automatically
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_container_add(GTK_CONTAINER(sTipWindow), label);
  gtk_container_set_border_width(GTK_CONTAINER(sTipWindow), 3);
  // set the coords for the widget
  gtk_widget_set_uposition(sTipWindow, aXCoords + root_x,
			   aYCoords + root_y);

  // and show it.
  gtk_widget_show_all(sTipWindow);

  // draw tooltip style border around the text
  gtk_paint_flat_box(sTipWindow->style, sTipWindow->window,
           GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
           NULL, GTK_WIDGET(sTipWindow), "tooltip",
           0, 0,
           sTipWindow->allocation.width, sTipWindow->allocation.height);

#ifdef MOZ_WIDGET_GTK
  gtk_widget_popup(sTipWindow, aXCoords + root_x, aYCoords + root_y);
#endif 
  
  nsMemory::Free( (void*)tipString );
  ***************/

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::OnHideTooltip(void)
{
  /********* PENDING(edburns): implement for win32
  if (sTipWindow)
    gtk_widget_destroy(sTipWindow);
  sTipWindow = NULL;
  ***************/
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
