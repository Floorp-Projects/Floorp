/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsWBURIContentListener_h__
#define nsWBURIContentListener_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIURIContentListener.h"

class nsWebBrowser;

class nsWBURIContentListener : public nsIURIContentListener
{
friend class nsWebBrowser;
public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIURICONTENTLISTENER

protected:
   nsWBURIContentListener();
   virtual ~nsWBURIContentListener();

   void WebBrowser(nsWebBrowser* aWebBrowser);
   nsWebBrowser* WebBrowser();

protected:
   nsWebBrowser*           mWebBrowser;

   nsIURIContentListener*  mParentContentListener;  // Weak Reference
};

#endif /* nsWBURIContentListener_h__ */
