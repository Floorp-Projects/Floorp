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

#ifndef __MozEmbedChrome_h
#define __MozEmbedChrome_h

// needed for the ever helpful nsCOMPtr
#include <nsCOMPtr.h>

// we will implement these interfaces
#include "nsIPhEmbed.h"
#include "nsIWebBrowserChrome.h"
#include "nsIBaseWindow.h"
#include "nsIURIContentListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInputStream.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"

// utility classes
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsVoidArray.h"

class MozEmbedChrome : public nsIPhEmbed,
                          public nsIWebBrowserChrome,
                          public nsIBaseWindow,
                          public nsIURIContentListener,
                          public nsIWebProgressListener,
                          public nsIInterfaceRequestor
{
public:

  MozEmbedChrome();
  virtual ~MozEmbedChrome();

  // nsIPhEmbed

  NS_IMETHOD Init                         (PtWidget_t *aOwningWidget);
  NS_IMETHOD SetNewBrowserCallback        (MozEmbedChromeCB *aCallback, void *aData);
  NS_IMETHOD SetDestroyCallback           (MozEmbedDestroyCB *aCallback, void *aData);
  NS_IMETHOD SetVisibilityCallback        (MozEmbedVisibilityCB *aCallback, void *aData);
  NS_IMETHOD SetLinkChangeCallback        (MozEmbedLinkCB *aCallback, void *aData);
  NS_IMETHOD SetJSStatusChangeCallback    (MozEmbedJSStatusCB *aCallback, void *aData);
  NS_IMETHOD SetLocationChangeCallback    (MozEmbedLocationCB *aCallback, void *aData);
  NS_IMETHOD SetTitleChangeCallback       (MozEmbedTitleCB *aCallback, void *aData);
  NS_IMETHOD SetProgressCallback          (MozEmbedProgressCB *aCallback, void *aData);
  NS_IMETHOD SetNetCallback               (MozEmbedNetCB *aCallback, void *aData);
  NS_IMETHOD SetStartOpenCallback         (MozEmbedStartOpenCB *aCallback, void *aData);
  NS_IMETHOD GetLinkMessage               (char **retval);
  NS_IMETHOD GetJSStatus                  (char **retval);
  NS_IMETHOD GetLocation                  (char **retval);
  NS_IMETHOD GetTitleChar                 (char **retval);
  NS_IMETHOD OpenStream                   (const char *aBaseURI, const char *aContentType);
  NS_IMETHOD AppendToStream               (const char *aData,int32 aLen);
  NS_IMETHOD CloseStream                  (void);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_NSIWEBBROWSERCHROME

  NS_DECL_NSIURICONTENTLISTENER

  NS_DECL_NSIWEBPROGRESSLISTENER

  NS_DECL_NSIBASEWINDOW

private:
  PtWidget_t                 *mOwningWidget;
  nsCOMPtr<nsIWebBrowser>    mWebBrowser;
  MozEmbedChromeCB       *mNewBrowserCB;
  void                      *mNewBrowserCBData;
  MozEmbedDestroyCB      *mDestroyCB;
  void                      *mDestroyCBData;
  MozEmbedVisibilityCB   *mVisibilityCB;
  void                      *mVisibilityCBData;
  MozEmbedLinkCB         *mLinkCB;
  void                      *mLinkCBData;
  MozEmbedJSStatusCB     *mJSStatusCB;
  void                      *mJSStatusCBData;
  MozEmbedLocationCB     *mLocationCB;
  void                      *mLocationCBData;
  MozEmbedTitleCB        *mTitleCB;
  void                      *mTitleCBData;
  MozEmbedProgressCB     *mProgressCB;
  void                      *mProgressCBData;
  MozEmbedNetCB          *mNetCB;
  void                      *mNetCBData;
  MozEmbedStartOpenCB    *mOpenCB;
  void                      *mOpenCBData;
  nsRect                     mBounds;
  PRBool                     mVisibility;
  nsXPIDLCString             mLinkMessage;
  nsXPIDLCString             mJSStatus;
  nsXPIDLCString             mLocation;
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
};

#endif /* __MozEmbedChrome_h */

