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
#include "nsMsgImapCID.h"
#include "nsImapMailFolder.h"
#include "nsImapMoveCoalescer.h"
#include "nsMsgKeySet.h"
#include "nsImapService.h"

static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);

nsImapMoveCoalescer::nsImapMoveCoalescer(nsImapMailFolder *sourceFolder)
{
	m_sourceFolder = sourceFolder;
	if (sourceFolder)
		NS_ADDREF(sourceFolder);
}

nsImapMoveCoalescer::~nsImapMoveCoalescer()
{
	NS_IF_RELEASE(m_sourceFolder);
	for (PRInt32 i = 0; i < m_sourceKeySets.Count(); i++)
	{
		nsMsgKeySet *keys = (nsMsgKeySet *) m_sourceKeySets.ElementAt(i);
		delete keys;
	}
}

nsresult nsImapMoveCoalescer::AddMove(nsIMsgFolder *folder, nsMsgKey key)
{
	if (!m_destFolders)
        NS_NewISupportsArray(getter_AddRefs(m_destFolders));
	if (m_destFolders)
	{
		PRInt32 folderIndex = m_destFolders->IndexOf(folder);
		nsMsgKeySet *keysToAdd;
		if (folderIndex >= 0)
		{
			keysToAdd = (nsMsgKeySet *) m_sourceKeySets.ElementAt(folderIndex);
		}
		else
		{
			m_destFolders->AppendElement(folder);
			keysToAdd = nsMsgKeySet::Create();
			if (!keysToAdd)
				return NS_ERROR_OUT_OF_MEMORY;

			m_sourceKeySets.AppendElement(keysToAdd);
		}
		if (keysToAdd)
			keysToAdd->Add(key);
		return NS_OK;
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;

}

nsresult nsImapMoveCoalescer::PlaybackMoves(nsIEventQueue *eventQueue)
{
	PRUint32 numFolders;
	nsresult rv = NS_OK;

	if (!m_destFolders)
		return NS_OK;	// nothing to do.

	m_destFolders->Count(&numFolders);
	for (PRUint32 i = 0; i < numFolders; i++)
	{
		nsCOMPtr <nsISupports> destSupports = getter_AddRefs(m_destFolders->ElementAt(i));
		nsCOMPtr <nsIMsgFolder> destFolder(do_QueryInterface(destSupports));
        NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
        if (NS_SUCCEEDED(rv) && imapService)
		{
			nsMsgKeySet *keysToAdd = (nsMsgKeySet *) m_sourceKeySets.ElementAt(i);
			if (keysToAdd)
			{
				char *messageIds = keysToAdd->Output();
				if (messageIds)
				{
					nsCOMPtr <nsISupports> sourceSupports = do_QueryInterface((nsIMsgImapMailFolder *) m_sourceFolder, &rv);
					nsCOMPtr <nsIUrlListener> urlListener(do_QueryInterface(sourceSupports));
					rv = imapService->OnlineMessageCopy(eventQueue,
													m_sourceFolder, messageIds,
													destFolder, PR_TRUE, PR_TRUE,
													urlListener, nsnull);
					delete [] messageIds;
				}
				else
				{
					rv = NS_ERROR_OUT_OF_MEMORY;
					break;
				}
			}
		}
	}
	return rv;
}

