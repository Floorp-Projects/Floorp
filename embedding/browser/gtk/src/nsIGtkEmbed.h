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

#ifndef __nsIGtkEmbed_h__
#define __nsIGtkEmbed_h__

#include <gtk/gtk.h>
#include "nsIWebBrowser.h"

#define NS_IGTKEMBED_IID_STR "ebe19ea4-1dd1-11b2-bc20-8e8105516b2f"

#define NS_IGTKEMBED_IID \
 {0xebe19ea4, 0x1dd1, 0x11b2, \
   { 0xbc, 0x20, 0x8e, 0x81, 0x05, 0x51, 0x6b, 0x2f }}

typedef nsresult (GtkMozEmbedChromeCB)     (PRUint32 chromeMask, nsIWebBrowser **_retval, void *aData);
typedef void     (GtkMozEmbedDestroyCB)    (void *aData);
typedef void     (GtkMozEmbedVisibilityCB) (PRBool aVisibility, void *aData);

class nsIGtkEmbed : public nsISupports
{
public:
  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IGTKEMBED_IID)
  
  NS_IMETHOD Init                         (GtkWidget *aOwningWidget) = 0;
  NS_IMETHOD SetNewBrowserCallback        (GtkMozEmbedChromeCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetDestroyCallback           (GtkMozEmbedDestroyCB *aCallback, void *aData) = 0;
  NS_IMETHOD SetVisibilityCallback        (GtkMozEmbedVisibilityCB *aCallback, void *aData) = 0;
};

#endif /* __nsIGtkEmbed_h__ */
