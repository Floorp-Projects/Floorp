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

#include "EmbedWindowCreator.h"
#include "EmbedPrivate.h"
#include "EmbedWindow.h"

// in order to create orphaned windows
#include "gtkmozembedprivate.h"

EmbedWindowCreator::EmbedWindowCreator(void)
{
}

EmbedWindowCreator::~EmbedWindowCreator()
{
}

NS_IMPL_ISUPPORTS1(EmbedWindowCreator, nsIWindowCreator)

NS_IMETHODIMP
EmbedWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
				       PRUint32 aChromeFlags,
				       nsIWebBrowserChrome **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  GtkMozEmbed *newEmbed = nsnull;

  // No parent?  Ask via the singleton object instead.
  if (!aParent) {
    gtk_moz_embed_single_create_window(&newEmbed,
				       (guint)aChromeFlags);
  }
  else {
    // Find the EmbedPrivate object for this web browser chrome object.
    EmbedPrivate *embedPrivate = EmbedPrivate::FindPrivateForBrowser(aParent);
    
    if (!embedPrivate)
      return NS_ERROR_FAILURE;
    
    gtk_signal_emit(GTK_OBJECT(embedPrivate->mOwningWidget),
		    moz_embed_signals[NEW_WINDOW],
		    &newEmbed, (guint)aChromeFlags);
    
  }

  // check to make sure that we made a new window
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

  *_retval = NS_STATIC_CAST(nsIWebBrowserChrome *,
  			    (newEmbedPrivate->mWindow));
  
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
