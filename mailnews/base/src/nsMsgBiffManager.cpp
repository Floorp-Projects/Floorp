/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMsgBiffManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
//There's no GetIID for this.
static NS_DEFINE_IID(kITimerCallbackIID, NS_ITIMERCALLBACK_IID);

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgBiffManager(const nsIID& iid, void **result)
{
	nsMsgBiffManager *biffManager = new nsMsgBiffManager();
	return biffManager->QueryInterface(iid, result);
}

NS_END_EXTERN_C

NS_IMPL_ADDREF(nsMsgBiffManager)
NS_IMPL_RELEASE(nsMsgBiffManager)

NS_IMETHODIMP
nsMsgBiffManager::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIMsgBiffManager::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIMsgBiffManager*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	else if(iid.Equals(kITimerCallbackIID))
	{
		*result = NS_STATIC_CAST(nsITimerCallback*, this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}
nsMsgBiffManager::nsMsgBiffManager()
{
	NS_INIT_REFCNT();

	NS_NewTimer(&mBiffTimer);

	mBiffArray = new nsVoidArray();
}

nsMsgBiffManager::~nsMsgBiffManager()
{
	mBiffTimer->Cancel();
	NS_IF_RELEASE(mBiffTimer);

	PRInt32 count = mBiffArray->Count();
	for(PRInt32 i; i < count; i++)
	{
		nsBiffEntry *biffEntry = (nsBiffEntry*)mBiffArray->ElementAt(i);
		delete biffEntry;
	}
	delete mBiffArray;

}

NS_IMETHODIMP nsMsgBiffManager::AddServerBiff(nsIMsgIncomingServer *server)
{
	PRInt32 serverIndex = FindServer(server);
	nsresult rv;
	//Only add it if it hasn't been added already.
	if(serverIndex == -1)
	{
		PRInt32 biffInterval;
		nsBiffEntry *biffEntry = new nsBiffEntry;
		if(!biffEntry)
			return NS_ERROR_OUT_OF_MEMORY;
		biffEntry->server = server;
		rv = server->GetBiffMinutes(&biffInterval);
		if(NS_FAILED(rv))
			return rv;
		//Add 60 secs/minute in microseconds to current time. biffEntry->nextBiffTime's
		//constructor makes it the current time.
		biffEntry->nextBiffTime += (60000000 * biffInterval);
		AddBiffEntry(biffEntry);
		SetupNextBiff();
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgBiffManager::RemoveServerBiff(nsIMsgIncomingServer *server)
{
	return NS_OK;
}


NS_IMETHODIMP nsMsgBiffManager::ForceBiff(nsIMsgIncomingServer *server)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgBiffManager::ForceBiffAll()
{
	return NS_OK;
}

void nsMsgBiffManager::Notify(nsITimer *timer)
{
	
}

PRInt32 nsMsgBiffManager::FindServer(nsIMsgIncomingServer *server)
{
	PRInt32 count = mBiffArray->Count();
	for(PRInt32 i = 0; i < count; i++)
	{
		nsBiffEntry *biffEntry = (nsBiffEntry*)mBiffArray->ElementAt(i);
		if(server == biffEntry->server.get())
			return i;
	}
	return -1;
}

nsresult nsMsgBiffManager::AddBiffEntry(nsBiffEntry *biffEntry)
{
	PRInt32 count = mBiffArray->Count();
	PRInt32 i;
	for(i = 0; i < count; i++)
	{
		nsBiffEntry *current = (nsBiffEntry*)mBiffArray->ElementAt(i);
		if(biffEntry->nextBiffTime < current->nextBiffTime)
			mBiffArray->InsertElementAt(biffEntry, i);

	}
	mBiffArray->InsertElementAt(biffEntry, i);
	return NS_OK;
}

nsresult nsMsgBiffManager::SetupNextBiff()
{
	if(mBiffArray->Count() > 0)
	{
		//Get the next biff entry
		nsBiffEntry *biffEntry = (nsBiffEntry*)mBiffArray->ElementAt(0);
		nsTime currentTime;
		nsTime biffDelay;

		if(currentTime > biffEntry->nextBiffTime)
			biffDelay = 0;
		else
			biffDelay = biffEntry->nextBiffTime - currentTime;
		mBiffTimer->Cancel();
		//Convert biffDelay into milliseconds
		PRInt64 timeInMS;
		PRUint32 timeInMSUint32;

		LL_DIV(timeInMS, biffDelay, 1000);
		LL_L2UI(timeInMSUint32, timeInMS);
		mBiffTimer->Init(this, timeInMSUint32);
	}
	return NS_OK;
}

