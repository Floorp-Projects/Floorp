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

#ifndef nsDocShellLoadInfo_h__
#define nsDocShellLoadInfo_h__


// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIDocShellLoadInfo.h"
#include "nsIURI.h"
#include "nsISHEntry.h"

class nsDocShellLoadInfo : public nsIDocShellLoadInfo
{
public:
   nsDocShellLoadInfo();

   NS_DECL_ISUPPORTS
   NS_DECL_NSIDOCSHELLLOADINFO

protected:
   virtual ~nsDocShellLoadInfo();

protected:
   nsCOMPtr<nsIURI>                 mReferrer;
   nsCOMPtr<nsISupports>            mOwner;
   PRBool                           mInheritOwner;
   nsDocShellInfoLoadType           mLoadType;
   nsCOMPtr<nsISHEntry>             mSHEntry;
};

#endif /* nsDocShellLoadInfo_h__ */
