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

// Local Includes
#include "nsDocShellLoadInfo.h"

//*****************************************************************************
//***    nsDocShellLoadInfo: Object Management
//*****************************************************************************

nsDocShellLoadInfo::nsDocShellLoadInfo() : mReplaceSessionHistorySlot(PR_FALSE)
{
	NS_INIT_REFCNT();
}

nsDocShellLoadInfo::~nsDocShellLoadInfo()
{
}

//*****************************************************************************
// nsDocShellLoadInfo::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDocShellLoadInfo)
NS_IMPL_RELEASE(nsDocShellLoadInfo)

NS_INTERFACE_MAP_BEGIN(nsDocShellLoadInfo)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellLoadInfo)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellLoadInfo)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDocShellLoadInfo::nsIDocShellLoadInfo
//*****************************************************************************   

NS_IMETHODIMP nsDocShellLoadInfo::GetReferrer(nsIURI** aReferrer)
{
   NS_ENSURE_ARG_POINTER(aReferrer);

   *aReferrer = mReferrer;
   NS_IF_ADDREF(*aReferrer);
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetReferrer(nsIURI* aReferrer)
{
   mReferrer = aReferrer;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetReplaceSessionHistorySlot(PRBool* aReplace)
{
   NS_ENSURE_ARG_POINTER(aReplace);

   *aReplace = mReplaceSessionHistorySlot;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetReplaceSessionHistorySlot(PRBool aReplace)
{
   mReplaceSessionHistorySlot = aReplace;
   return NS_OK;
}

//*****************************************************************************
// nsDocShellLoadInfo: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDocShellLoadInfo: Accessors
//*****************************************************************************   
