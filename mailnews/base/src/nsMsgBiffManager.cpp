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
#include "nsCRT.h"

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgBiffManager(const nsIID& iid, void **result)
{
	nsMsgBiffManager *biffManager = new nsMsgBiffManager();
	if(!biffManager)
		return NS_ERROR_OUT_OF_MEMORY;
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
    if (iid.Equals(nsCOMTypeInfo<nsIMsgBiffManager>::GetIID()) ||
        iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *result = NS_STATIC_CAST(nsIMsgBiffManager*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	else if(iid.Equals(nsCOMTypeInfo<nsITimerCallback>::GetIID()))
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

	mBiffTimer = nsnull;
	mBiffArray = new nsVoidArray();
}

nsMsgBiffManager::~nsMsgBiffManager()
{
	if (mBiffTimer) {
		mBiffTimer->Cancel();
	}
	NS_IF_RELEASE(mBiffTimer);

	PRInt32 count = mBiffArray->Count();
    PRInt32 i;
	for(i=0; i < count; i++)
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
		nsBiffEntry *biffEntry = new nsBiffEntry;
		if(!biffEntry)
			return NS_ERROR_OUT_OF_MEMORY;
		biffEntry->server = server;
		nsTime currentTime;
		rv = SetNextBiffTime(biffEntry, currentTime);
		if(NS_FAILED(rv))
			return rv;

		AddBiffEntry(biffEntry);
		SetupNextBiff();
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgBiffManager::RemoveServerBiff(nsIMsgIncomingServer *server)
{
	PRInt32 pos = FindServer(server);
	if(pos != -1)
	{
		mBiffArray->RemoveElementAt(pos);
		//Need to release mBiffArray's ref on server.
		NS_IF_RELEASE(server);
	}

	//Should probably reset biff time if this was the server that gets biffed next.
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
	PerformBiff();		
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
	PRInt32 i;
	PRInt32 count = mBiffArray->Count();
	for(i = 0; i < count; i++)
	{
		nsBiffEntry *current = (nsBiffEntry*)mBiffArray->ElementAt(i);
		if(biffEntry->nextBiffTime < current->nextBiffTime)
			mBiffArray->InsertElementAt(biffEntry, i);

	}
	mBiffArray->InsertElementAt(biffEntry, i);
	return NS_OK;
}

nsresult nsMsgBiffManager::SetNextBiffTime(nsBiffEntry *biffEntry, nsTime startTime)
{
	nsresult rv;
	nsIMsgIncomingServer *server = biffEntry->server;

	if(!server)
		return NS_ERROR_FAILURE;

	PRInt32 biffInterval;
	rv = server->GetBiffMinutes(&biffInterval);
	if(NS_FAILED(rv))
		return rv;
	//Add 60 secs/minute in microseconds to current time. biffEntry->nextBiffTime's
	//constructor makes it the current time.
	biffEntry->nextBiffTime = startTime;
	biffEntry->nextBiffTime += (60000000 * biffInterval);
	return NS_OK;
}

nsresult nsMsgBiffManager::SetupNextBiff()
{
	
	if(mBiffArray->Count() > 0)
	{
		
		//Get the next biff entry
		nsBiffEntry *biffEntry = (nsBiffEntry*)mBiffArray->ElementAt(0);
		nsTime currentTime;
		nsInt64 biffDelay;
		nsInt64 ms(1000);
		if(currentTime > biffEntry->nextBiffTime)
			biffDelay = 1;
		else
			biffDelay = biffEntry->nextBiffTime - currentTime;
		//Convert biffDelay into milliseconds
		nsInt64 timeInMS = biffDelay / ms;
		PRUint32 timeInMSUint32 = (PRUint32)timeInMS;
		//Can't currently reset a timer when it's in the process of
		//calling Notify. So, just release the timer here and create a new one.
		if(mBiffTimer)
		{
			mBiffTimer->Cancel();
			NS_RELEASE(mBiffTimer);
		}
		NS_NewTimer(&mBiffTimer);
		mBiffTimer->Init(this, timeInMSUint32);
		
	}
	return NS_OK;
}

//This is the function that does a biff on all of the servers whose time it is to biff.
nsresult nsMsgBiffManager::PerformBiff()
{
	nsTime currentTime;
	for(PRInt32 i = 0; i < mBiffArray->Count(); i++)
	{
		nsBiffEntry *current = (nsBiffEntry*)mBiffArray->ElementAt(i);
		if(current->nextBiffTime < currentTime)
		{
			PRBool serverBusy = PR_FALSE;
			char *password = nsnull;
			// we don't want to prompt the user for password UI so pass in false to
			// the server->GetPassword method. If we don't already know the passsword then 
			// we just won't biff this server
			current->server->GetPassword(PR_FALSE, &password);
			current->server->GetServerBusy(&serverBusy);

			//Make sure we're logged on before doing a biff
			// and make sure the server isn't already in the middle of downloading new messages
			if(!serverBusy && password && (nsCRT::strcmp(password, "") != 0))
				current->server->PerformBiff();
			mBiffArray->RemoveElementAt(i);
			i--; //Because we removed it we need to look at the one that just moved up.
			SetNextBiffTime(current, currentTime);
			AddBiffEntry(current);
		}
		else
			//since we're in biff order, there's no reason to keep checking
			break;
	}
	SetupNextBiff();
	return NS_OK;
}
