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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h" // for pre-compiled headers...
#include "nsUrlListenerManager.h"

nsUrlListenerManager::nsUrlListenerManager() :
    mRefCnt(0),
    m_listeners(nsnull)
{
	NS_INIT_REFCNT();
	// create a new isupports array to store our listeners in...
	m_listeners = new nsVoidArray();
}

nsUrlListenerManager::~nsUrlListenerManager()
{
	if (m_listeners)
	{
		PRUint32 count = m_listeners->Count();

		for (int i = count - 1; i >= 0; i--)
			m_listeners->RemoveElementAt(i);
	}

	delete m_listeners;
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsUrlListenerManager, nsCOMTypeInfo<nsIUrlListenerManager>::GetIID());

nsresult nsUrlListenerManager::RegisterListener(nsIUrlListener * aUrlListener)
{
	if (m_listeners && aUrlListener)
		m_listeners->AppendElement((void *) aUrlListener);

	return NS_OK;
}

nsresult nsUrlListenerManager::UnRegisterListener(nsIUrlListener * aUrlListener)
{
	if (m_listeners && aUrlListener)
		m_listeners->RemoveElement((void *) aUrlListener);
	return NS_OK;
}

nsresult nsUrlListenerManager::BroadcastChange(nsIURI * aUrl, nsUrlNotifyType notification, nsresult aErrorCode)
{
	NS_PRECONDITION(aUrl, "we shouldn't get OnStartRunningUrl for the url listener manager without a url...");
	nsresult rv = NS_OK;

	if (m_listeners && aUrl)
	{
		// enumerate over all url listeners...(Start at the end and work our way down)
		nsIUrlListener * listener = nsnull;
		
		for (PRUint32 i = m_listeners->Count(); i > 0; i--)
		{
			listener = (nsIUrlListener *) m_listeners->ElementAt(i-1);
			if (listener)
			{
				if (notification == nsUrlNotifyStartRunning)
					listener->OnStartRunningUrl(aUrl);
				else if (notification == nsUrlNotifyStopRunning)
					listener->OnStopRunningUrl(aUrl, aErrorCode);
			}

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

