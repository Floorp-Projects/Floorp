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

#ifndef __EmbedWindow_h
#define __EmbedWindow_h

#include <nsString.h>
#include <nsIWebBrowserChrome.h>
#include <nsIWebBrowserChromeFocus.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsITooltipListener.h>
#include <nsISupports.h>
#include <nsIWebBrowser.h>
#include <nsIBaseWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsCOMPtr.h>
#include "nsString.h"

#include <gtk/gtk.h>

class EmbedPrivate;

class EmbedWindow : public nsIWebBrowserChrome,
		    public nsIWebBrowserChromeFocus,
                    public nsIEmbeddingSiteWindow,
                    public nsITooltipListener,
		    public nsIInterfaceRequestor
{

 public:

  EmbedWindow();
  virtual ~EmbedWindow();

  nsresult Init            (EmbedPrivate *aOwner);
  nsresult CreateWindow    (void);
  void     ReleaseChildren (void);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBBROWSERCHROME

  NS_DECL_NSIWEBBROWSERCHROMEFOCUS

  NS_DECL_NSIEMBEDDINGSITEWINDOW

  NS_DECL_NSITOOLTIPLISTENER

  NS_DECL_NSIINTERFACEREQUESTOR

  nsString                 mTitle;
  nsString                 mJSStatus;
  nsString                 mLinkMessage;

  nsCOMPtr<nsIBaseWindow>  mBaseWindow; // [OWNER]

private:

  EmbedPrivate            *mOwner;
  nsCOMPtr<nsIWebBrowser>  mWebBrowser; // [OWNER]
  static GtkWidget        *sTipWindow;
  PRBool                   mVisibility;
  PRBool                   mIsModal;

};
  

#endif /* __EmbedWindow_h */
