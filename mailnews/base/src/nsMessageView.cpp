/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsMessageView.h"

NS_BEGIN_EXTERN_C

nsresult
NS_NewMessageView(const nsIID& iid, void **result)
{
	nsMessageView *messageView = new nsMessageView();
	if(!messageView)
		return NS_ERROR_OUT_OF_MEMORY;
	nsresult rv = messageView->Init();
	if(NS_FAILED(rv))
	{
		delete messageView;
		return rv;
	}

	return messageView->QueryInterface(iid, result);
}

NS_END_EXTERN_C

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

