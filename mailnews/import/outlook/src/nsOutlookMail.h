/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsOutlookMail_h___
#define nsOutlookMail_h___

#include "nsISupportsArray.h"
#include "nsIFileSpec.h"

#include "MapiApi.h"
#include "MapiMessage.h"

class nsOutlookMail {
public:
    nsOutlookMail();
    ~nsOutlookMail();

	nsresult GetMailFolders( nsISupportsArray **pArray);
	nsresult ImportMailbox( PRBool *pAbort, PRInt32 index, const PRUnichar *pName, nsIFileSpec *pDest, PRInt32 *pMsgCount);


private:
	void	OpenMessageStore( CMapiFolder *pNextFolder);
	BOOL	WriteData( nsIFileSpec *pDest, const char *pData, PRInt32 len);
	BOOL	WriteMessage( nsIFileSpec *pDest, CMapiMessage *pMsg, int& attachCount, BOOL *pTerminate);
	BOOL	WriteStr( nsIFileSpec *pDest, const char *pStr);
	BOOL	WriteWithoutFrom( nsIFileSpec *pDest, const char * pData, int len, BOOL checkStart);
	BOOL	WriteMimeMsgHeader( nsIFileSpec *pDest, CMapiMessage *pMsg);
	BOOL	WriteMimeBoundary( nsIFileSpec *pDest, CMapiMessage *pMsg, BOOL terminate);
	PRBool	WriteAttachment( nsIFileSpec *pDest, CMapiMessage *pMsg);

private:
	PRBool				m_gotFolders;
	PRBool				m_haveMapi;
	CMapiApi			m_mapi;
	CMapiFolderList		m_folderList;
	CMapiFolderList		m_storeList;
	LPMDB				m_lpMdb;
};

#endif /* nsOutlookMail_h___ */
