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
#include "EmbedPrompter.h"

GtkWidget *EmbedWindow::sTipWindow = nsnull;

EmbedWindow::EmbedWindow(void)
{
  NS_INIT_REFCNT();
  mOwner       = nsnull;
  mVisibility  = PR_FALSE;
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
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
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

  // set the chrome flag on the new window if it's a chrome open
  if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
    newEmbedPrivate->mIsChrome = PR_TRUE;

  newEmbedPrivate->mWindow->GetWebBrowser(_retval);
  
  if (*_retval)
    return NS_OK;

  return NS_ERROR_FAILURE;

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
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  prompter.Create(EmbedPrompter::TYPE_ALERT);
  prompter.Run();
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::AlertCheck(const PRUnichar *aDialogTitle,
			const PRUnichar *aText,
			const PRUnichar *aCheckMsg, PRBool *aCheckValue)
{
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  prompter.SetCheckMessage(aCheckMsg);
  prompter.SetCheckValue(*aCheckValue);
  prompter.Create(EmbedPrompter::TYPE_ALERT_CHECK);
  prompter.Run();
  prompter.GetCheckValue(aCheckValue);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::Confirm(const PRUnichar *aDialogTitle, const PRUnichar *aText,
		     PRBool *_retval)
{
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  prompter.Create(EmbedPrompter::TYPE_CONFIRM);
  prompter.Run();
  prompter.GetConfirmValue(_retval);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ConfirmCheck(const PRUnichar *aDialogTitle,
			  const PRUnichar *aText, const PRUnichar *aCheckMsg,
			  PRBool *aCheckValue, PRBool *_retval)
{
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  prompter.SetCheckMessage(aCheckMsg);
  prompter.SetCheckValue(*aCheckValue);
  prompter.Create(EmbedPrompter::TYPE_CONFIRM);
  prompter.Run();
  prompter.GetConfirmValue(_retval);
  if (*_retval)
    prompter.GetCheckValue(aCheckValue);
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ConfirmEx(const PRUnichar *dialogTitle, const PRUnichar *text,
                       PRUint32 button0And1Flags, const PRUnichar *button2Title,
                       const PRUnichar *checkMsg, PRBool *checkValue,
                       PRInt32 *buttonPressed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedWindow::Prompt(const PRUnichar *aDialogTitle, const PRUnichar *aText,
		    PRUnichar **result,
		    const PRUnichar *aCheckMsg, PRBool *aCheckValue,
		    PRBool *_retval)
{
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  if (result && *result)
    prompter.SetTextValue(*result);
  if (aCheckValue) {
    prompter.SetCheckValue(*aCheckValue);
    if (aCheckMsg)
      prompter.SetCheckMessage(aCheckMsg);
    else
      prompter.SetCheckMessage(NS_LITERAL_STRING("Save This Value").get());
  }  
  prompter.Create(EmbedPrompter::TYPE_PROMPT);
  prompter.Run();
  prompter.GetConfirmValue(_retval);
  if (*_retval) {
    if (result && *result) {
      nsMemory::Free(*result);
      *result = nsnull;
    }
    prompter.GetTextValue(result);
    if (aCheckValue)
      prompter.GetCheckValue(aCheckValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::PromptUsernameAndPassword(const PRUnichar *aDialogTitle,
				       const PRUnichar *aText,
				       PRUnichar **aUser, PRUnichar **aPwd,
				       const PRUnichar *aCheckMsg, PRBool *aCheckValue,
				       PRBool *_retval)
{
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  if (aUser && *aUser)
    prompter.SetUser(*aUser);
  if (aPwd && *aPwd)
    prompter.SetPassword(*aPwd);
  if (aCheckValue) {
    prompter.SetCheckValue(*aCheckValue);
    if (aCheckMsg)
      prompter.SetCheckMessage(aCheckMsg);
    else
      prompter.SetCheckMessage(NS_LITERAL_STRING("Save These Values").get());
  }      
  prompter.Create(EmbedPrompter::TYPE_PROMPT_USER_PASS);
  prompter.Run();
  prompter.GetConfirmValue(_retval);
  if (*_retval) {
    if (aUser && *aUser) {
      nsMemory::Free(*aUser);
      *aUser = nsnull;
    }
    prompter.GetUser(aUser);
    if (aPwd && *aPwd) {
      nsMemory::Free(*aPwd);
      *aPwd = nsnull;
    }
    prompter.GetPassword(aPwd);
    if (aCheckValue)
      prompter.GetCheckValue(aCheckValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::PromptPassword(const PRUnichar *aDialogTitle, 
			    const PRUnichar *aText, PRUnichar **aPwd,
			    const PRUnichar *aCheckMsg, PRBool *aCheckValue,
			    PRBool *_retval)
{
  EmbedPrompter prompter;
  prompter.SetTitle(aDialogTitle);
  prompter.SetMessageText(aText);
  if (aPwd && *aPwd)
    prompter.SetPassword(*aPwd);
  if (aCheckValue) {
    prompter.SetCheckValue(*aCheckValue);
    if (aCheckMsg)
      prompter.SetCheckMessage(aCheckMsg);
    else
      prompter.SetCheckMessage(NS_LITERAL_STRING("Save Password").get());
  }        
  prompter.Create(EmbedPrompter::TYPE_PROMPT_PASS);
  prompter.Run();
  prompter.GetConfirmValue(_retval);
  if (*_retval) {
    if (aPwd && *aPwd) {
      nsMemory::Free(*aPwd);
      *aPwd = nsnull;
    }
    prompter.GetPassword(aPwd);
    if (aCheckValue)
      prompter.GetCheckValue(aCheckValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::Select(const PRUnichar *aDialogTitle, const PRUnichar *aText,
		    PRUint32 aCount, const PRUnichar **aSelectList,
		    PRInt32 *aOutSelection, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
EmbedWindow::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  return QueryInterface(aIID, aInstancePtr);
}
