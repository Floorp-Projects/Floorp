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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h" // for pre-compiled headers...
#include "nsUrlListenerManager.h"

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

nsUrlListenerManager::nsUrlListenerManager()
{
	NS_INIT_REFCNT();
	// create a new isupports array to store our listeners in...
	NS_NewISupportsArray(&m_listeners);

}

nsUrlListenerManager::~nsUrlListenerManager()
{
	if (m_listeners)
	{
		PRUint32 count = m_listeners->Count();

		for (int i = count - 1; i >= 0; i--)
			m_listeners->RemoveElementAt(i);

		NS_RELEASE(m_listeners);
	}
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsUrlListenerManager, nsIUrlListenerManager::GetIID());

nsresult nsUrlListenerManager::RegisterListener(nsIUrlListener * aUrlListener)
{
	if (m_listeners && aUrlListener)
		m_listeners->AppendElement(aUrlListener);

	return NS_OK;
}

nsresult nsUrlListenerManager::UnRegisterListener(nsIUrlListener * aUrlListener)
{
	if (m_listeners && aUrlListener)
		m_listeners->RemoveElement(aUrlListener);
	return NS_OK;
}

nsresult nsUrlListenerManager::BroadcastChange(nsIURL * aUrl, nsUrlNotifyType notification, nsresult aErrorCode)
{
	NS_PRECONDITION(aUrl, "we shouldn't get OnStartRunningUrl for the url listener manager without a url...");
	nsIUrlListener * listener = nsnull;
	nsresult rv = NS_OK;

	if (m_listeners && aUrl)
	{
		// enumerate over all url listeners...(Start at the end and work our way down)
		for (PRUint32 i = m_listeners->Count(); i > 0; i--)
		{
			nsISupports *supports = m_listeners->ElementAt(i-1);
			if(supports)
			{
				if(NS_SUCCEEDED(rv = supports->QueryInterface(nsIUrlListener::GetIID(), (void**)&listener)))
				{
					if (listener)
					{
						if (notification == nsUrlNotifyStartRunning)
							listener->OnStartRunningUrl(aUrl);
						else if (notification == nsUrlNotifyStopRunning)
							listener->OnStopRunningUrl(aUrl, aErrorCode);
						NS_RELEASE(listener);
					}
				}

				NS_RELEASE(supports);
			} // if we have a valid element in the array

		} // for each listener
	} // if m_listeners && aUrl

	return rv;
}

nsresult nsUrlListenerManager::OnStartRunningUrl(nsIMsgMailNewsUrl * aUrl)
{
	return BroadcastChange(aUrl, nsUrlNotifyStartRunning, 0);
}

nsresult nsUrlListenerManager::OnStopRunningUrl(nsIMsgMailNewsUrl * aUrl, nsresult aErrorCode)
{
	return BroadcastChange(aUrl, nsUrlNotifyStopRunning, aErrorCode);
}

