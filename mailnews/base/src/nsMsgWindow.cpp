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

#include "nsMsgWindow.h"



NS_IMPL_ISUPPORTS(nsMsgWindow, nsCOMTypeInfo<nsIMsgWindow>::GetIID())

nsMsgWindow::nsMsgWindow()
{
	NS_INIT_REFCNT();

}

nsMsgWindow::~nsMsgWindow()
{

}

nsresult nsMsgWindow::Init()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetStatusFeedback(nsIMsgStatusFeedback * *aStatusFeedback)
{
	if(!aStatusFeedback)
		return NS_ERROR_NULL_POINTER;

	*aStatusFeedback = mStatusFeedback;
	NS_IF_ADDREF(*aStatusFeedback);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetStatusFeedback(nsIMsgStatusFeedback * aStatusFeedback)
{
	mStatusFeedback = aStatusFeedback;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetTransactionManager(nsITransactionManager * *aTransactionManager)
{
	if(!aTransactionManager)
		return NS_ERROR_NULL_POINTER;

	*aTransactionManager = mTransactionManager;
	NS_IF_ADDREF(*aTransactionManager);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetTransactionManager(nsITransactionManager * aTransactionManager)
{
	mTransactionManager = aTransactionManager;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetMessageView(nsIMessageView * *aMessageView)
{
	if(!aMessageView)
		return NS_ERROR_NULL_POINTER;

	*aMessageView = mMessageView;
	NS_IF_ADDREF(*aMessageView);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetMessageView(nsIMessageView * aMessageView)
{
	mMessageView = aMessageView;
	return NS_OK;
}
