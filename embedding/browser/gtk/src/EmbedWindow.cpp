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
 */

#include <nsCWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIDocShellTreeItem.h>
#include "nsIWidget.h"
#include "nsReadableUtils.h"

#include "EmbedWindow.h"
#include "EmbedPrivate.h"
#include "EmbedPrompter.h"

GtkWidget *EmbedWindow::sTipWindow = nsnull;

EmbedWindow::EmbedWindow(void)
{
  NS_INIT_REFCNT();
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
  
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(mWebBrowser);
  item->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  return NS_OK;
}

nsresult
EmbedWindow::CreateWindow(void)
{
  nsresult rv;
  GtkWidget *ownerAsWidget = GTK_WIDGET(mOwner->mOwningWidget);

  // Get the base window interface for the web browser object and
  // create the window.
  mBaseWindow = do_QueryInterface(mWebBrowser);
  rv = mBaseWindow->InitWindow(GTK_WIDGET(mOwner->mOwningWidget),
			       nsnull,
			       0, 0, 
			       ownerAsWidget->allocation.width,
			       ownerAsWidget->allocation.height);
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
      gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		      moz_embed_signals[JS_STATUS]);
    }
    break;
  case STATUS_SCRIPT_DEFAULT:
    // Gee, that's nice.
    break;
  case STATUS_LINK:
    {
      mLinkMessage = aStatus;
      gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		      moz_embed_signals[LINK_MESSAGE]);
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
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DESTROY_BROWSER]);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[SIZE_TO], aCX, aCY);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ShowAsModal(void)
{
  mIsModal = PR_TRUE;
  GtkWidget *toplevel;
  toplevel = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
  gtk_grab_add(toplevel);
  gtk_main();
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
    GtkWidget *toplevel;
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(mOwner->mOwningWidget));
    gtk_grab_remove(toplevel);
    mIsModal = PR_FALSE;
    gtk_main_quit();
  }
  return NS_OK;
}

// nsIWebBrowserChromeFocus

NS_IMETHODIMP
EmbedWindow::FocusNextElement()
{
  GtkWidget* parent = GTK_WIDGET(mOwner->mOwningWidget)->parent;

  if (GTK_IS_CONTAINER(parent))
    gtk_container_focus(GTK_CONTAINER(parent),
                        GTK_DIR_TAB_FORWARD);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::FocusPrevElement()
{
  GtkWidget* parent = GTK_WIDGET(mOwner->mOwningWidget)->parent;

  if (GTK_IS_CONTAINER(parent))
    gtk_container_focus(GTK_CONTAINER(parent),
                        GTK_DIR_TAB_BACKWARD);
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
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[TITLE]);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetSiteWindow(void **aSiteWindow)
{
  GtkWidget *ownerAsWidget (GTK_WIDGET(mOwner->mOwningWidget));
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

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[VISIBILITY],
		  aVisibility);
  return NS_OK;
}

// nsITooltipListener

NS_IMETHODIMP
EmbedWindow::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords,
			   const PRUnichar *aTipText)
{
  
  nsAutoString tipText ( aTipText );
  const char* tipString = ToNewCString(tipText);

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

  gtk_widget_popup(sTipWindow, aXCoords + root_x, aYCoords + root_y);
  
  nsMemory::Free( (void*)tipString );

  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::OnHideTooltip(void)
{
  if (sTipWindow)
    gtk_widget_destroy(sTipWindow);
  sTipWindow = NULL;
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
