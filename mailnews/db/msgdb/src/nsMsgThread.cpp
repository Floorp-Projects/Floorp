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

#include "msgCore.h"
#include "nsMsgThread.h"
#include "nsMsgDatabase.h"


NS_IMPL_ISUPPORTS(nsMsgThread, nsMsgThread::GetIID())

nsMsgThread::nsMsgThread()
{
	m_threadKey = nsMsgKey_None; 
	m_numChildren = 0;		
	m_numNewChildren = 0;	
	m_flags = 0;
	NS_INIT_REFCNT();
}

nsMsgThread::~nsMsgThread()
{
}

NS_IMETHODIMP nsMsgThread::GetFlags(PRUint32 *result)
{
	nsresult ret = NS_OK;

	return ret;
}

NS_IMETHODIMP nsMsgThread::SetFlags(PRUint32 flags)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetNumChildren(PRUint32 *result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetNumUnreadChildren (PRUint32 *result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::AddChild(nsIMessage *child, PRBool threadInThread)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChildAt(PRUint32 index, nsIMessage **result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChild(nsMsgKey msgKey, nsIMessage **result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChildHdrAt(PRUint32 index, nsIMessage **result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::RemoveChildAt(PRUint32 index)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::RemoveChild(nsMsgKey msgKey)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::MarkChildRead(PRBool bRead)
{
	nsresult ret = NS_OK;

	return ret;
}


