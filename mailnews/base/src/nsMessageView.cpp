/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMessageView.h"



NS_IMPL_ISUPPORTS(nsMessageView, nsCOMTypeInfo<nsIMessageView>::GetIID())

nsMessageView::nsMessageView()
{
	NS_INIT_REFCNT();
	mShowThreads = PR_FALSE;
	mViewType = nsIMessageView::eShowAll;

}

nsMessageView::~nsMessageView()
{

}

nsresult nsMessageView::Init()
{
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::GetViewType(PRUint32 *aViewType)
{
	if(!aViewType)
		return NS_ERROR_NULL_POINTER;

	*aViewType = mViewType;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::SetViewType(PRUint32 aViewType)
{
	mViewType = aViewType;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::GetShowThreads(PRBool *aShowThreads)
{
	if(!aShowThreads)
		return NS_ERROR_NULL_POINTER;

	*aShowThreads = mShowThreads;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::SetShowThreads(PRBool aShowThreads)
{
	mShowThreads = aShowThreads;
	return NS_OK;
}

