/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsOutlookMail_h___
#define nsOutlookMail_h___

#include "nsISupportsArray.h"
#include "nsIFileSpec.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsOutlookCompose.h"
#include "MapiApi.h"
#include "MapiMessage.h"

class nsIAddrDatabase;
class nsIImportFieldMap;

class nsOutlookMail {
public:
    nsOutlookMail();
    ~nsOutlookMail();

	nsresult GetMailFolders( nsISupportsArray **pArray);
	nsresult GetAddressBooks( nsISupportsArray **pArray);
	nsresult ImportMailbox( PRUint32 *pDoneSoFar, PRBool *pAbort, PRInt32 index, const PRUnichar *pName, nsIFileSpec *pDest, PRInt32 *pMsgCount);
	nsresult ImportAddresses( PRUint32 *pCount, PRUint32 *pTotal, const PRUnichar *pName, PRUint32 id, nsIAddrDatabase *pDb, nsString& errors);


private:
	void	OpenMessageStore( CMapiFolder *pNextFolder);
	BOOL	WriteData( nsIFileSpec *pDest, const char *pData, PRInt32 len);
	BOOL	WriteMessage( nsIFileSpec *pDest, CMapiMessage *pMsg, int& attachCount, BOOL *pTerminate);
	BOOL	WriteStr( nsIFileSpec *pDest, const char *pStr);
	BOOL	WriteWithoutFrom( nsIFileSpec *pDest, const char * pData, int len, BOOL checkStart);
	BOOL	WriteMimeMsgHeader( nsIFileSpec *pDest, CMapiMessage *pMsg);
	BOOL	WriteMimeBoundary( nsIFileSpec *pDest, CMapiMessage *pMsg, BOOL terminate);

	nsresult	DeleteFile( nsIFileSpec *pSpec);
	void		EmptyAttachments( void);
	void		BuildAttachments( CMapiMessage& msg, int count);
	void		DumpAttachments( void);

	PRBool		IsAddressBookNameUnique( nsString& name, nsString& list);
	void		MakeAddressBookNameUnique( nsString& name, nsString& list);
	void		SanitizeValue( nsString& val);
	void		SplitString( nsString& val1, nsString& val2);
	PRBool		BuildCard( const PRUnichar *pName, nsIAddrDatabase *pDb, nsIMdbRow *newRow, LPMAPIPROP pUser, nsIImportFieldMap *pFieldMap);

private:
	PRBool				m_gotFolders;
	PRBool				m_gotAddresses;
	PRBool				m_haveMapi;
	CMapiApi			m_mapi;
	CMapiFolderList		m_folderList;
	CMapiFolderList		m_addressList;
	CMapiFolderList		m_storeList;
	LPMDB				m_lpMdb;
	nsVoidArray			m_attachments;
};

#endif /* nsOutlookMail_h___ */
