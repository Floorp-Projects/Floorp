/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Seth Spitzer <sspitzer@netscape.com>
 */

#include "nsNewsDownloadDialogArgs.h"
#include "nsCRT.h"

nsNewsDownloadDialogArgs::nsNewsDownloadDialogArgs()
{
    NS_INIT_REFCNT();
    
    mGroupName = "";
    mArticleCount = 0;
    mServerKey = "";
    mHitOK = PR_FALSE;
    mDownloadAll = PR_FALSE;
}

nsNewsDownloadDialogArgs::~nsNewsDownloadDialogArgs()
{
}

NS_IMPL_ISUPPORTS1(nsNewsDownloadDialogArgs, nsINewsDownloadDialogArgs)

NS_IMETHODIMP nsNewsDownloadDialogArgs::GetGroupName(char * *aGroupName)
{
    NS_ENSURE_ARG_POINTER(aGroupName);

    *aGroupName = nsCRT::strdup((const char *)mGroupName);

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::SetGroupName(const char * aGroupName)
{
    NS_ENSURE_ARG_POINTER(aGroupName);

    mGroupName = aGroupName;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::GetArticleCount(PRInt32 *aArticleCount)
{
    NS_ENSURE_ARG_POINTER(aArticleCount);

    *aArticleCount = mArticleCount;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::SetArticleCount(PRInt32 aArticleCount)
{
    mArticleCount = aArticleCount;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::GetServerKey(char * *aServerKey)
{
    NS_ENSURE_ARG_POINTER(aServerKey);
    
    *aServerKey = nsCRT::strdup((const char *)mServerKey);
    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::SetServerKey(const char * aServerKey)
{
    NS_ENSURE_ARG_POINTER(aServerKey);

    mServerKey = aServerKey;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::GetHitOK(PRBool *aHitOK)
{
    NS_ENSURE_ARG_POINTER(aHitOK);
    
    *aHitOK = mHitOK;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::SetHitOK(PRBool aHitOK)
{
    mHitOK = aHitOK;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::GetDownloadAll(PRBool *aDownloadAll)
{
    NS_ENSURE_ARG_POINTER(aDownloadAll);

    *aDownloadAll = mDownloadAll;

    return NS_OK;
}
NS_IMETHODIMP nsNewsDownloadDialogArgs::SetDownloadAll(PRBool aDownloadAll)
{
    mDownloadAll = aDownloadAll;

    return NS_OK;
}
