/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  
  EmbedPrivate *newEmbedPrivate = static_cast<EmbedPrivate *>(newEmbed->data);

  // set the chrome flag on the new window if it's a chrome open
  if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
    newEmbedPrivate->mIsChrome = PR_TRUE;

  *_retval = static_cast<nsIWebBrowserChrome *>(newEmbedPrivate->mWindow);
  
  if (*_retval) {
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
