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
#include "nsIBaseWindow.h"
#include "nsIInterfaceRequestor.h"

// include our gtk stuff here
#include "gdksuperwin.h"

class GtkMozEmbedChrome : public nsIGtkEmbed,
                          public nsIWebBrowserChrome,
                          public nsIBaseWindow,
                          public nsIInterfaceRequestor
{
public:

  GtkMozEmbedChrome();
  virtual ~GtkMozEmbedChrome();

  // nsIGtkEmbed
  NS_IMETHOD Init(GdkWindow *aParentWindow);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_NSIWEBBROWSERCHROME

  NS_DECL_NSIBASEWINDOW

private:
  
};

#endif /* __GtkMozEmbedChrome_h */

