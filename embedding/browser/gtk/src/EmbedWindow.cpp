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

#include <nsCWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIDocShellTreeItem.h>

#include "EmbedWindow.h"
#include "EmbedPrivate.h"

GtkWidget *EmbedWindow::sTipWindow = nsnull;

EmbedWindow::EmbedWindow(void)
{
  NS_INIT_REFCNT();
  mOwner       = nsnull;
  mChromeMask  = 0;
}

EmbedWindow::~EmbedWindow(void)
{
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
}

// nsISupports

NS_IMPL_ADDREF(EmbedWindow)
NS_IMPL_RELEASE(EmbedWindow)

NS_INTERFACE_MAP_BEGIN(EmbedWindow)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
  NS_INTERFACE_MAP_ENTRY(nsIPrompt)
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
  *aChromeFlags = mChromeMask;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetChromeFlags(PRUint32 aChromeFlags)
{
  mChromeMask = aChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::CreateBrowserWindow(PRUint32 aChromeFlags, 
				 PRInt32 aX, PRInt32 aY,
				 PRInt32 aCX, PRInt32 aCY,
				 nsIWebBrowser **_retval)
{
  GtkMozEmbed *newEmbed = nsnull;

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[NEW_WINDOW],
		  &newEmbed, (guint)aChromeFlags);

  if (!newEmbed)
    return NS_ERROR_FAILURE;

  // The window _must_ be realized before we pass it back to the
  // function that created it. Functions that create new windows
  // will do things like GetDocShell() and the widget has to be
  // realized before that can happen.
  gtk_widget_realize(GTK_WIDGET(newEmbed));

  EmbedPrivate *newEmbedPrivate = NS_STATIC_CAST(EmbedPrivate *,
						 newEmbed->data);

  newEmbedPrivate->mWindow->GetWebBrowser(_retval);
  
  if (*_retval)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;

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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::IsWindowModal(PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::ExitModalEventLoop(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::SetPersistence(PRBool aPersistX, PRBool aPersistY,
			    PRBool aPersistCX, PRBool aPersistCY,
			    PRBool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::GetPersistence(PRBool *aPersistX, PRBool *aPersistY,
			    PRBool *aPersistCX, PRBool *aPersistCY,
			    PRBool *aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIWebBrowserSiteWindow

NS_IMETHODIMP
EmbedWindow::Destroy(void)
{
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DESTROY_BROWSER]);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetPosition(PRInt32 aX, PRInt32 aY)
{
  return mBaseWindow->SetPosition(aX, aY);
}

NS_IMETHODIMP
EmbedWindow::GetPosition(PRInt32 *aX, PRInt32 *aY)
{
  return mBaseWindow->GetPosition(aX, aY);
}

NS_IMETHODIMP
EmbedWindow::SetSize(PRInt32 aX, PRInt32 aY, PRBool aRepaint)
{
  return mBaseWindow->SetSize(aX, aY, aRepaint);
}

NS_IMETHODIMP
EmbedWindow::GetSize(PRInt32 *aX, PRInt32 *aY)
{
  return mBaseWindow->GetSize(aX, aY);
}

NS_IMETHODIMP
EmbedWindow::SetPositionAndSize(PRInt32 aX, PRInt32 aY,
				PRInt32 aWidth, PRInt32 aHeight,
				PRBool aRepaint)
{
  return mBaseWindow->SetPositionAndSize(aX, aY, aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
EmbedWindow::GetPositionAndSize(PRInt32 *aX, PRInt32 *aY,
				PRInt32 *aWidth, PRInt32 *aHeight)
{
  return mBaseWindow->GetPositionAndSize(aX, aY, aWidth, aHeight);
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
  *aTitle = mTitle.ToNewUnicode();
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsITooltipListener

NS_IMETHODIMP
EmbedWindow::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords,
			   const PRUnichar *aTipText)
{
  
  nsAutoString tipText ( aTipText );
  const char* tipString = tipText.ToNewCString();

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
  // set the coords for the widget
  gtk_widget_set_uposition(sTipWindow, aXCoords + root_x,
			   aYCoords + root_y);

  // and show it.
  gtk_widget_show_all(sTipWindow);
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

// nsIPrompt

NS_IMETHODIMP
EmbedWindow::Alert(const PRUnichar *aDialogTitle, const PRUnichar *aText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::AlertCheck(const PRUnichar *aDialogTitle,
			const PRUnichar *aText,
			const PRUnichar *aCheckMsg, PRBool *aCheckValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::Confirm(const PRUnichar *aDialogTitle, const PRUnichar *aText,
		     PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::ConfirmCheck(const PRUnichar *aDialogTitle,
			  const PRUnichar *aText, const PRUnichar *aCheckMsg,
			  PRBool *aCheckValue, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::Prompt(const PRUnichar *aDialogTitle, const PRUnichar *aText,
		    const PRUnichar *aPasswordRealm, PRUint32 aSavePassword,
		    const PRUnichar *aDefaultText, PRUnichar **result,
		    PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::PromptUsernameAndPassword(const PRUnichar *aDialogTitle,
				       const PRUnichar *aText,
				       const PRUnichar *aPasswordRealm,
				       PRUint32 aSavePassword,
				       PRUnichar **aUser, PRUnichar **aPwd,
				       PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::PromptPassword(const PRUnichar *aDialogTitle, 
			    const PRUnichar *aText,
			    const PRUnichar *aPasswordRealm,
			    PRUint32 aSavePassword, PRUnichar **aPwd,
			    PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::Select(const PRUnichar *aDialogTitle, const PRUnichar *aText,
		    PRUint32 aCount, const PRUnichar **aSelectList,
		    PRInt32 *aOutSelection, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::UniversalDialog(const PRUnichar *titleMessage,
			     const PRUnichar *dialogTitle,
			     const PRUnichar *text,
			     const PRUnichar *checkboxMsg,
			     const PRUnichar *button0Text,
			     const PRUnichar *button1Text,
			     const PRUnichar *button2Text,
			     const PRUnichar *button3Text,
			     const PRUnichar *editfield1Msg,
			     const PRUnichar *editfield2Msg, 
			     PRUnichar **editfield1Value,
			     PRUnichar **editfield2Value,
			     const PRUnichar *iconURL,
			     PRBool *checkboxState, 
			     PRInt32 numberButtons,
			     PRInt32 numberEditfields, 
			     PRInt32 editField1Password,
			     PRInt32 *buttonPressed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
EmbedWindow::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  return QueryInterface(aIID, aInstancePtr);
}
