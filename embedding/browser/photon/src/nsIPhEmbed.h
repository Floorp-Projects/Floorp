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

#ifndef __nsIPhEmbed_h__
#define __nsIPhEmbed_h__

#include <Pt.h>
#include "nsIWebBrowser.h"

#define NS_IGTKEMBED_IID_STR "ebe19ea4-1dd1-11b2-bc20-8e8105516b2f"

#define NS_IGTKEMBED_IID \
 {0xebe19ea4, 0x1dd1, 0x11b2, \
   { 0xbc, 0x20, 0x8e, 0x81, 0x05, 0x51, 0x6b, 0x2f }}

typedef nsresult (MozEmbedChromeCB)          (PRUint32 chromeMask, nsIWebBrowser **_retval, void *aData);
typedef void     (MozEmbedDestroyCB)         (void *aData);
typedef void     (MozEmbedVisibilityCB)      (PRBool aVisibility, void *aData);
typedef void     (MozEmbedLinkCB)            (void *aData);
typedef void     (MozEmbedJSStatusCB)        (void *aData);
typedef void     (MozEmbedLocationCB)        (void *aData);
typedef void     (MozEmbedTitleCB)           (void *aData);
typedef void     (MozEmbedProgressCB)        (void *aData, PRInt32 aProgressTotal,
						 PRInt32 aProgressCurrent);
typedef void     (MozEmbedNetCB)             (void *aData, PRInt32 aFlags, nsresult aStatus);
typedef PRBool   (MozEmbedStartOpenCB)       (const char *aURI, void *aData);

class nsIPhEmbed : public nsISupports
{
public:
  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IGTKEMBED_IID)
  
  NS_IMETHOD Init                         (PtWidget_t *aOwningWidget) = 0;
  NS_IMETHOD SetNewBrowserCallback        (MozEmbedChromeCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetDestroyCallback           (MozEmbedDestroyCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetVisibilityCallback        (MozEmbedVisibilityCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetLinkChangeCallback        (MozEmbedLinkCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetJSStatusChangeCallback    (MozEmbedJSStatusCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetLocationChangeCallback    (MozEmbedLocationCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetTitleChangeCallback       (MozEmbedTitleCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetProgressCallback          (MozEmbedProgressCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetNetCallback               (MozEmbedNetCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetStartOpenCallback         (MozEmbedStartOpenCB *aCallback, void *aData) = 0;
  NS_IMETHOD GetLinkMessage               (char **retval) = 0;
  NS_IMETHOD GetJSStatus                  (char **retval) = 0;
  NS_IMETHOD GetLocation                  (char **retval) = 0;
  NS_IMETHOD GetTitleChar                 (char **retval) = 0;
  NS_IMETHOD OpenStream                   (const char *aBaseURI, const char *aContentType) = 0;
  NS_IMETHOD AppendToStream               (const char *aData, int32 aLen) = 0;
  NS_IMETHOD CloseStream                  (void) = 0;
};

#endif /* __nsIPhEmbed_h__ */
