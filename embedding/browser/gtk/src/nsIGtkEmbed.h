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
#include "nsIWebProgress.h"
#include "nsIRequest.h"
#include "nsIDocShellTreeItem.h"

#define NS_IGTKEMBED_IID_STR "ebe19ea4-1dd1-11b2-bc20-8e8105516b2f"

#define NS_IGTKEMBED_IID \
 {0xebe19ea4, 0x1dd1, 0x11b2, \
   { 0xbc, 0x20, 0x8e, 0x81, 0x05, 0x51, 0x6b, 0x2f }}

class GtkEmbedListener
{
 public:
  enum GtkEmbedListenerMessageType
  {
    MessageLink = 0,
    MessageJSStatus,
    MessageTitle
  };
  virtual nsresult NewBrowser(PRUint32 aChromeFlags,
			      nsIDocShellTreeItem **_retval) = 0;
  virtual void     Destroy(void) = 0;
  virtual void     Visibility(PRBool aVisibility) = 0;
  virtual void     Message(GtkEmbedListenerMessageType aType,
			   const char *aMessage) = 0;
  virtual PRBool   StartOpen(const char *aURI) = 0;
  virtual void     SizeTo(PRInt32 width, PRInt32 height) = 0;
};

class nsIGtkEmbed : public nsISupports
{
public:
  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IGTKEMBED_IID)
  
  NS_IMETHOD Init                         (GtkWidget *aOwningWidget) = 0;
  NS_IMETHOD SetEmbedListener             (GtkEmbedListener *aListener) = 0;
  NS_IMETHOD GetLinkMessage               (char **retval) = 0;
  NS_IMETHOD GetJSStatus                  (char **retval) = 0;
  NS_IMETHOD GetTitleChar                 (char **retval) = 0;
  NS_IMETHOD OpenStream                   (const char *aBaseURI, const char *aContentType) = 0;
  NS_IMETHOD AppendToStream               (const char *aData, gint32 aLen) = 0;
  NS_IMETHOD CloseStream                  (void) = 0;
};

#endif /* __nsIGtkEmbed_h__ */
