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

#ifndef __GtkMozEmbedChrome_h
#define __GtkMozEmbedChrome_h

// needed for the ever helpful nsCOMPtr
#include <nsCOMPtr.h>

// we will implement these interfaces
#include "nsIGtkEmbed.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserSiteWindow.h"
#include "nsIURIContentListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInputStream.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrompt.h"
#include "nsICommonDialogs.h"
#include "nsIDOMWindowInternal.h"
#include "nsITooltipListener.h"
#include "nsIBaseWindow.h"

// utility classes
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsVoidArray.h"

// include our gtk stuff here
#include "gdksuperwin.h"

class GtkMozEmbedChrome : public nsIGtkEmbed,
                          public nsIWebBrowserChrome,
                          public nsIWebBrowserSiteWindow,
                          public nsIURIContentListener,
                          public nsIDocShellTreeOwner,
                          public nsIInterfaceRequestor,
                          public nsIPrompt,
                          public nsIBaseWindow,
                          public nsITooltipListener

{
public:

  GtkMozEmbedChrome();
  virtual ~GtkMozEmbedChrome();

  // nsIGtkEmbed

  NS_IMETHOD Init                         (GtkWidget *aOwningWidget);
  NS_IMETHOD SetEmbedListener             (GtkEmbedListener *aListener);
  NS_IMETHOD GetLinkMessage               (char **retval);
  NS_IMETHOD GetJSStatus                  (char **retval);
  NS_IMETHOD GetTitleChar                 (char **retval);
  NS_IMETHOD OpenStream                   (const char *aBaseURI, const char *aContentType);
  NS_IMETHOD AppendToStream               (const char *aData, gint32 aLen);
  NS_IMETHOD CloseStream                  (void);
  NS_IMETHOD EnsureCommonDialogs          (void);
  NS_IMETHOD GetDOMWindowInternal         (nsIDOMWindowInternal **aWindow);
     
  NS_DECL_ISUPPORTS

  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_NSIWEBBROWSERCHROME

  NS_DECL_NSIURICONTENTLISTENER

  NS_DECL_NSIDOCSHELLTREEOWNER

  NS_DECL_NSIWEBBROWSERSITEWINDOW

  NS_DECL_NSIPROMPT

  NS_DECL_NSITOOLTIPLISTENER

  // missing methods for nsIBaseWindow
  NS_IMETHOD InitWindow(nativeWindow parentNativeWindow,
			nsIWidget * parentWidget,
			PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy);
  NS_IMETHOD Create(void);
  NS_IMETHOD Repaint(PRBool force);
  NS_IMETHOD GetParentWidget(nsIWidget * *aParentWidget);
  NS_IMETHOD SetParentWidget(nsIWidget * aParentWidget);
  NS_IMETHOD GetParentNativeWindow(nativeWindow *aParentNativeWindow);
  NS_IMETHOD SetParentNativeWindow(nativeWindow aParentNativeWindow);
  NS_IMETHOD GetVisibility(PRBool *aVisibility);
  NS_IMETHOD SetVisibility(PRBool aVisibility);
  NS_IMETHOD GetMainWidget(nsIWidget * *aMainWidget);
  NS_IMETHOD FocusAvailable(nsIBaseWindow *aCurrentFocus, PRBool *aTookFocus);

private:
  GtkWidget                 *mOwningGtkWidget;
  nsCOMPtr<nsIWebBrowser>    mWebBrowser;
  nsRect                     mBounds;
  PRBool                     mVisibility;
  nsXPIDLCString             mLinkMessage;
  nsXPIDLCString             mJSStatus;
  nsXPIDLCString             mTitle;
  nsString                   mTitleUnicode;
  PRUint32                   mChromeMask;
  static nsVoidArray        *sBrowsers;
  nsCOMPtr<nsIInputStream>   mStream;
  nsCOMPtr<nsILoadGroup>     mLoadGroup;
  nsCOMPtr<nsIChannel>       mChannel;
  nsCOMPtr<nsIContentViewer> mContentViewer;
  nsCOMPtr<nsIStreamListener> mStreamListener;
  PRUint32                   mOffset;
  PRBool                     mDoingStream;
  GtkEmbedListener          *mChromeListener;
  nsIDocShellTreeItem       *mContentShell;
  nsCOMPtr<nsICommonDialogs> mCommonDialogs;
};

#endif /* __GtkMozEmbedChrome_h */

