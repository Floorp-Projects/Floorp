/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h" // for pre-compiled headers...
#include "nsUrlListenerManager.h"

nsUrlListenerManager::nsUrlListenerManager() :
    mRefCnt(0),
    m_listeners(nsnull)
{
	NS_INIT_REFCNT();
	// create a new isupports array to store our listeners in...
  NS_NewISupportsArray(getter_AddRefs(m_listeners));
}

nsUrlListenerManager::~nsUrlListenerManager()
{
  ReleaseListeners();
}

void nsUrlListenerManager::ReleaseListeners()
{
  if(m_listeners)
	{
		PRUint32 count;
        nsresult rv = m_listeners->Count(&count);
        NS_ASSERTION(NS_SUCCEEDED(rv), "m_listeners->Count() failed");
        if (NS_FAILED(rv)) return;
        
		for (int i = count - 1; i >= 0; i--)
			m_listeners->RemoveElementAt(i);
	}
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUrlListenerManager, nsIUrlListenerManager)

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

nsresult nsUrlListenerManager::BroadcastChange(nsIURI * aUrl, nsUrlNotifyType notification, nsresult aErrorCode)
{
	NS_PRECONDITION(aUrl, "we shouldn't get OnStartRunningUrl for the url listener manager without a url...");
	nsresult rv = NS_OK;

	if (m_listeners && aUrl)
	{
		// enumerate over all url listeners...(Start at the end and work our way down)
		nsCOMPtr<nsIUrlListener> listener;
    nsCOMPtr<nsISupports> aSupports;
		PRUint32 index;
    m_listeners->Count(&index);
		for (; index > 0; index--)
		{
      m_listeners->GetElementAt(index-1, getter_AddRefs(aSupports)); 
			listener = do_QueryInterface(aSupports);

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
	nsresult rv = BroadcastChange(aUrl, nsUrlNotifyStopRunning, aErrorCode);
  // in order to prevent circular references, after we issue on stop running url, 
  // go through and release all of our listeners...
  ReleaseListeners();
  return rv;
}

