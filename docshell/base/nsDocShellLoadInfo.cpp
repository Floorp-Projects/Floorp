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
 *   Viswanath Ramachandran <vishy@netscape.com>
 */

// Local Includes
#include "nsDocShellLoadInfo.h"
#include "nsReadableUtils.h"

//*****************************************************************************
//***    nsDocShellLoadInfo: Object Management
//*****************************************************************************

nsDocShellLoadInfo::nsDocShellLoadInfo()
{
   mLoadType = nsIDocShellLoadInfo::loadNormal;
   mInheritOwner = PR_FALSE;
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

NS_IMETHODIMP nsDocShellLoadInfo::GetOwner(nsISupports** aOwner)
{
   NS_ENSURE_ARG_POINTER(aOwner);

   *aOwner = mOwner;
   NS_IF_ADDREF(*aOwner);
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetOwner(nsISupports* aOwner)
{
   mOwner = aOwner;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetInheritOwner(PRBool* aInheritOwner)
{
   NS_ENSURE_ARG_POINTER(aInheritOwner);

   *aInheritOwner = mInheritOwner;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetInheritOwner(PRBool aInheritOwner)
{
   mInheritOwner = aInheritOwner;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetLoadType(nsDocShellInfoLoadType * aLoadType)
{
   NS_ENSURE_ARG_POINTER(aLoadType);

   *aLoadType = mLoadType;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetLoadType(nsDocShellInfoLoadType aLoadType)
{
   mLoadType = aLoadType;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetSHEntry(nsISHEntry** aSHEntry)
{
   NS_ENSURE_ARG_POINTER(aSHEntry);

   *aSHEntry = mSHEntry;
   NS_IF_ADDREF(*aSHEntry);
   return NS_OK;
}
 
NS_IMETHODIMP nsDocShellLoadInfo::SetSHEntry(nsISHEntry* aSHEntry)
{
   mSHEntry = aSHEntry;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetTarget(PRUnichar** aTarget)
{
   NS_ENSURE_ARG_POINTER(aTarget);

   *aTarget = ToNewUnicode(mTarget);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetTarget(const PRUnichar* aTarget)
{
   mTarget.Assign(aTarget);
   return NS_OK;
}


NS_IMETHODIMP
nsDocShellLoadInfo::GetPostDataStream(nsIInputStream **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mPostDataStream;

  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsDocShellLoadInfo::SetPostDataStream(nsIInputStream *aStream)
{
  mPostDataStream = aStream;
  return NS_OK;
}

/* attribute nsIInputStream headersStream; */
NS_IMETHODIMP nsDocShellLoadInfo::GetHeadersStream(nsIInputStream * *aHeadersStream)
{
  NS_ENSURE_ARG_POINTER(aHeadersStream);
  *aHeadersStream = mHeadersStream;
  NS_IF_ADDREF(*aHeadersStream);
  return NS_OK;
}
NS_IMETHODIMP nsDocShellLoadInfo::SetHeadersStream(nsIInputStream * aHeadersStream)
{
  mHeadersStream = aHeadersStream;
  return NS_OK;
}

//*****************************************************************************
// nsDocShellLoadInfo: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDocShellLoadInfo: Accessors
//*****************************************************************************   
