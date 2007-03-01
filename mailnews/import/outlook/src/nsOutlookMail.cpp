/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
  Outlook mail import
*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsIImportFieldMap.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportABDescriptor.h"
#include "nsIImportMimeEncode.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsOutlookStringBundle.h"
#include "nsABBaseCID.h"
#include "nsIAbCard.h"
#include "nsIAddrDatabase.h"
#include "mdb.h"
#include "OutlookDebugLog.h"
#include "nsOutlookMail.h"
#include "nsUnicharUtils.h"
#include "nsMsgUtils.h"
#include "nsIOutputStream.h"

static NS_DEFINE_CID(kImportMimeEncodeCID,	NS_IMPORTMIMEENCODE_CID);
static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);

/* ------------ Address book stuff ----------------- */
typedef struct {
	PRInt32		mozField;
	PRInt32		multiLine;
	ULONG		mapiTag;
} MAPIFields;

/*
	Fields in MAPI, not in Mozilla
	PR_OFFICE_LOCATION
	FIX - PR_BIRTHDAY - stored as PT_SYSTIME - FIX to extract for moz address book birthday
	PR_DISPLAY_NAME_PREFIX - Mr., Mrs. Dr., etc.
	PR_SPOUSE_NAME
	PR_GENDER - integer, not text
	FIX - PR_CONTACT_EMAIL_ADDRESSES - multiuline strings for email addresses, needs
		parsing to get secondary email address for mozilla
*/

#define kIsMultiLine	-2
#define	kNoMultiLine	-1

static MAPIFields	gMapiFields[] = {
	{ 35, kIsMultiLine, PR_BODY},
	{ 6, kNoMultiLine, PR_BUSINESS_TELEPHONE_NUMBER},
	{ 7, kNoMultiLine, PR_HOME_TELEPHONE_NUMBER},
	{ 25, kNoMultiLine, PR_COMPANY_NAME},
	{ 23, kNoMultiLine, PR_TITLE},
	{ 10, kNoMultiLine, PR_CELLULAR_TELEPHONE_NUMBER},
	{ 9, kNoMultiLine, PR_PAGER_TELEPHONE_NUMBER},
	{ 8, kNoMultiLine, PR_BUSINESS_FAX_NUMBER},
	{ 8, kNoMultiLine, PR_HOME_FAX_NUMBER},
	{ 22, kNoMultiLine, PR_COUNTRY},
	{ 19, kNoMultiLine, PR_LOCALITY},
	{ 20, kNoMultiLine, PR_STATE_OR_PROVINCE},
	{ 17, 18, PR_STREET_ADDRESS},
	{ 21, kNoMultiLine, PR_POSTAL_CODE},
	{ 27, kNoMultiLine, PR_PERSONAL_HOME_PAGE},
	{ 26, kNoMultiLine, PR_BUSINESS_HOME_PAGE},
	{ 13, kNoMultiLine, PR_HOME_ADDRESS_CITY},
	{ 16, kNoMultiLine, PR_HOME_ADDRESS_COUNTRY},
	{ 15, kNoMultiLine, PR_HOME_ADDRESS_POSTAL_CODE},
	{ 14, kNoMultiLine, PR_HOME_ADDRESS_STATE_OR_PROVINCE},
	{ 11, 12, PR_HOME_ADDRESS_STREET},
	{ 24, kNoMultiLine, PR_DEPARTMENT_NAME}
};
/* ---------------------------------------------------- */


#define	kCopyBufferSize		(16 * 1024)

// The email address in Outlook Contacts doesn't have a named 
// property,  we need to use this mapi name ID to access the email 
// The MAPINAMEID for email address has ulKind=MNID_ID
// Outlook stores each email address in two IDs,  32899/32900 for Email1
// 32915/32916 for Email2, 32931/32932 for Email3
// Current we use OUTLOOK_EMAIL1_MAPI_ID1 for primary email
// OUTLOOK_EMAIL2_MAPI_ID1 for secondary email
#define	OUTLOOK_EMAIL1_MAPI_ID1 32899    
#define	OUTLOOK_EMAIL1_MAPI_ID2 32900    
#define	OUTLOOK_EMAIL2_MAPI_ID1 32915   
#define	OUTLOOK_EMAIL2_MAPI_ID2 32916   
#define	OUTLOOK_EMAIL3_MAPI_ID1 32931    
#define	OUTLOOK_EMAIL3_MAPI_ID2 32932  

nsOutlookMail::nsOutlookMail()
{
	m_gotAddresses = PR_FALSE;
	m_gotFolders = PR_FALSE;
	m_haveMapi = CMapiApi::LoadMapi();
	m_lpMdb = NULL;
}

nsOutlookMail::~nsOutlookMail()
{
	EmptyAttachments();
}

nsresult nsOutlookMail::GetMailFolders( nsISupportsArray **pArray)
{
	if (!m_haveMapi) {
		IMPORT_LOG0( "GetMailFolders called before Mapi is initialized\n");
		return( NS_ERROR_FAILURE);
	}

	nsresult rv = NS_NewISupportsArray( pArray);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray for the mail folder list\n");
		return( rv);
	}

	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
	if (NS_FAILED( rv))
		return( rv);

	m_gotFolders = PR_TRUE;

	m_folderList.ClearAll();

	m_mapi.Initialize();
	m_mapi.LogOn();

	if (m_storeList.GetSize() == 0)
		m_mapi.IterateStores( m_storeList);

	int i = 0;
	CMapiFolder *pFolder;
	if (m_storeList.GetSize() > 1) {
		while ((pFolder = m_storeList.GetItem( i))) {
			CMapiFolder *pItem = new CMapiFolder( pFolder);
			pItem->SetDepth( 1);
			m_folderList.AddItem( pItem);
			if (!m_mapi.GetStoreFolders( pItem->GetCBEntryID(), pItem->GetEntryID(), m_folderList, 2)) {
				IMPORT_LOG1( "GetStoreFolders for index %d failed.\n", i);
			}
			i++;
		}
	}
	else {
		if ((pFolder = m_storeList.GetItem( i))) {
			if (!m_mapi.GetStoreFolders( pFolder->GetCBEntryID(), pFolder->GetEntryID(), m_folderList, 1)) {
				IMPORT_LOG1( "GetStoreFolders for index %d failed.\n", i);
			}
		}
	}
	
	// Create the mailbox descriptors for the list of folders
	nsIImportMailboxDescriptor *	pID;
	nsISupports *					pInterface;
	nsString						name;
	nsString						uniName;

	for (i = 0; i < m_folderList.GetSize(); i++) {
		pFolder = m_folderList.GetItem( i);
		rv = impSvc->CreateNewMailboxDescriptor( &pID);
		if (NS_SUCCEEDED( rv)) {
			pID->SetDepth( pFolder->GetDepth());
			pID->SetIdentifier( i);

			pFolder->GetDisplayName( name);
			pID->SetDisplayName(name.get());

			pID->SetSize( 1000);
			rv = pID->QueryInterface( kISupportsIID, (void **) &pInterface);
			(*pArray)->AppendElement( pInterface);
			pInterface->Release();
			pID->Release();
		}
	}
	
	return( NS_OK);	
}

PRBool nsOutlookMail::IsAddressBookNameUnique( nsString& name, nsString& list)
{
	nsString		usedName;
	usedName.AppendLiteral("[");
	usedName.Append( name);
	usedName.AppendLiteral("],");

	return( list.Find( usedName) == -1);
}

void nsOutlookMail::MakeAddressBookNameUnique( nsString& name, nsString& list)
{
	nsString		newName;
	int				idx = 1;

	newName = name;
	while (!IsAddressBookNameUnique( newName, list)) {
		newName = name;
		newName.Append(PRUnichar(' '));
		newName.AppendInt( (PRInt32) idx);
		idx++;
	}
	
	name = newName;
	list.AppendLiteral("[");
	list.Append( name);
	list.AppendLiteral("],");
}

nsresult nsOutlookMail::GetAddressBooks( nsISupportsArray **pArray)
{
	if (!m_haveMapi) {
		IMPORT_LOG0( "GetAddressBooks called before Mapi is initialized\n");
		return( NS_ERROR_FAILURE);
	}

	nsresult rv = NS_NewISupportsArray( pArray);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray for the address book list\n");
		return( rv);
	}

	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
	if (NS_FAILED( rv))
		return( rv);

	m_gotAddresses = PR_TRUE;
	
	m_addressList.ClearAll();
	m_mapi.Initialize();
	m_mapi.LogOn();
	if (m_storeList.GetSize() == 0)
		m_mapi.IterateStores( m_storeList);

	int i = 0;
	CMapiFolder *pFolder;
	if (m_storeList.GetSize() > 1) {
		while ((pFolder = m_storeList.GetItem( i))) {
			CMapiFolder *pItem = new CMapiFolder( pFolder);
			pItem->SetDepth( 1);
			m_addressList.AddItem( pItem);
			if (!m_mapi.GetStoreAddressFolders( pItem->GetCBEntryID(), pItem->GetEntryID(), m_addressList)) {
				IMPORT_LOG1( "GetStoreAddressFolders for index %d failed.\n", i);
			}
			i++;
		}
	}
	else {
		if ((pFolder = m_storeList.GetItem( i))) {
			if (!m_mapi.GetStoreAddressFolders( pFolder->GetCBEntryID(), pFolder->GetEntryID(), m_addressList)) {
				IMPORT_LOG1( "GetStoreFolders for index %d failed.\n", i);
			}
		}
	}
	
	// Create the mailbox descriptors for the list of folders
	nsIImportABDescriptor *			pID;
	nsISupports *					pInterface;
	nsString						name;
	nsString						list;

	for (i = 0; i < m_addressList.GetSize(); i++) {
		pFolder = m_addressList.GetItem( i);
		if (!pFolder->IsStore()) {
			rv = impSvc->CreateNewABDescriptor( &pID);
			if (NS_SUCCEEDED( rv)) {
				pID->SetIdentifier( i);
				pFolder->GetDisplayName( name);
				MakeAddressBookNameUnique( name, list);
				pID->SetPreferredName(name);
				pID->SetSize( 100);
				rv = pID->QueryInterface( kISupportsIID, (void **) &pInterface);
				(*pArray)->AppendElement( pInterface);
				pInterface->Release();
				pID->Release();
			}
		}
	}
	
	return( NS_OK);	
}



void nsOutlookMail::OpenMessageStore( CMapiFolder *pNextFolder)
{	
	// Open the store specified
	if (pNextFolder->IsStore()) {
		if (!m_mapi.OpenStore( pNextFolder->GetCBEntryID(), pNextFolder->GetEntryID(), &m_lpMdb)) {
			m_lpMdb = NULL;
			IMPORT_LOG0( "CMapiApi::OpenStore failed\n");
		}
		
		return;
	}
	
	// Check to see if we should open the one and only store
	if (!m_lpMdb) {
		if (m_storeList.GetSize() == 1) {
			CMapiFolder * pFolder = m_storeList.GetItem( 0);
			if (pFolder) {
				if (!m_mapi.OpenStore( pFolder->GetCBEntryID(), pFolder->GetEntryID(), &m_lpMdb)) {
					m_lpMdb = NULL;
					IMPORT_LOG0( "CMapiApi::OpenStore failed\n");
				}
			}
			else {
				IMPORT_LOG0( "Error retrieving the one & only message store\n");
			}
		}
		else {
			IMPORT_LOG0( "*** Error importing a folder without a valid message store\n");
		}
	}
}

void nsOutlookMail::SetDefaultContentType(CMapiMessage &msg, nsCString &cType)
{
  cType.Truncate();

  // MAPI doesn't seem to return the entire body data (ie, multiple parts) for
  // content type "multipart/alternative", instead it only returns the body data 
  // for a particular part. For this reason we'll need to set the content type
  // here. Same thing when conten type is not being set at all.
  if (msg.GetMimeContentLen())
  {
    // If content type is not multipart/alternative or mixed or related, return.
    // for a multipart alternative with attachments, we get multipart mixed!
    if (nsCRT::strcasecmp(msg.GetMimeContent(), "multipart/alternative")
      && nsCRT::strcasecmp(msg.GetMimeContent(), "multipart/mixed")
      && nsCRT::strcasecmp(msg.GetMimeContent(), "multipart/related"))
      return;

    // For multipart/alternative, if no body or boundary,
    // or boundary is found in body then return.
    const char *body = msg.GetBody();
    const char *boundary = msg.GetMimeBoundary();
    if (!body || !boundary || strstr(body, boundary))
      return;
  }
  
  // Now default the content type to text/html or text/plain
  // depending whether or not the body data is html.
  cType = msg.BodyIsHtml() ? "text/html" : "text/plain";
}

nsresult nsOutlookMail::ImportMailbox( PRUint32 *pDoneSoFar, PRBool *pAbort, PRInt32 index, const PRUnichar *pName, nsIFileSpec *pDest, PRInt32 *pMsgCount)
{
	if ((index < 0) || (index >= m_folderList.GetSize())) {
		IMPORT_LOG0( "*** Bad mailbox identifier, unable to import\n");
		*pAbort = PR_TRUE;
		return( NS_ERROR_FAILURE);
	}
	
	PRInt32		dummyMsgCount = 0;
	if (pMsgCount)
		*pMsgCount = 0;
	else
		pMsgCount = &dummyMsgCount;

	CMapiFolder *pFolder = m_folderList.GetItem( index);
	OpenMessageStore( pFolder);
	if (!m_lpMdb) {
		IMPORT_LOG1( "*** Unable to obtain mapi message store for mailbox: %S\n", pName);
		return( NS_ERROR_FAILURE);
	}
	
	if (pFolder->IsStore())
		return( NS_OK);
	
	nsCOMPtr<nsIFileSpec>	compositionFile;
	nsresult	rv;
	if (NS_FAILED( rv = NS_NewFileSpec( getter_AddRefs( compositionFile)))) {
		return( rv);
	}
	
	nsOutlookCompose		compose;
	SimpleBufferTonyRCopiedTwice			copy;

	copy.Allocate( kCopyBufferSize);

	// now what?
	CMapiFolderContents		contents( m_lpMdb, pFolder->GetCBEntryID(), pFolder->GetEntryID());

	BOOL		done = FALSE;
	ULONG		cbEid;
	LPENTRYID	lpEid;
	ULONG		oType;
	LPMESSAGE	lpMsg = nsnull;
	int			attachCount;
	ULONG		totalCount;
	PRFloat64	doneCalc;
	nsCString	fromLine;
	int			fromLen;
	PRBool		lostAttach = PR_FALSE;

	while (!done) {
		if (!contents.GetNext( &cbEid, &lpEid, &oType, &done)) {
			IMPORT_LOG1( "*** Error iterating mailbox: %S\n", pName);
			return( NS_ERROR_FAILURE);
		}
		
		totalCount = contents.GetCount();
		doneCalc = *pMsgCount;
		doneCalc /= totalCount;
		doneCalc *= 1000;
		if (pDoneSoFar) {
			*pDoneSoFar = (PRUint32) doneCalc;
			if (*pDoneSoFar > 1000)
				*pDoneSoFar = 1000;
		}

		if (!done && (oType == MAPI_MESSAGE)) {
			if (!m_mapi.OpenMdbEntry( m_lpMdb, cbEid, lpEid, (LPUNKNOWN *) &lpMsg)) {
				IMPORT_LOG1( "*** Error opening messages in mailbox: %S\n", pName);
				return( NS_ERROR_FAILURE);
			}
			
			CMapiMessage	msg( lpMsg);
	
			BOOL bResult = msg.FetchHeaders();
			if (bResult)
				bResult = msg.FetchBody();
			if (bResult)
				fromLine = msg.GetFromLine( fromLen);

			attachCount = msg.CountAttachments();
			BuildAttachments( msg, attachCount);

			if (!bResult) {
				IMPORT_LOG1( "*** Error reading message from mailbox: %S\n", pName);
				return( NS_ERROR_FAILURE);
			}
						
			// --------------------------------------------------------------
			compose.SetBody( msg.GetBody());

      // Need to convert all headers to unicode (for i18n).
      // Init header here since 'composes' is used for all msgs.
      compose.SetHeaders("");
      nsCOMPtr<nsIImportService>	impSvc = do_GetService(NS_IMPORTSERVICE_CONTRACTID);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get import service");
      if (NS_SUCCEEDED(rv))
      {
        nsAutoString newheader;
        nsCAutoString tempCStr(msg.GetHeaders(), msg.GetHeaderLen());
        rv = impSvc->SystemStringToUnicode(tempCStr.get(), newheader);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to convert headers to utf8");
        if (NS_SUCCEEDED(rv))
          compose.SetHeaders(NS_ConvertUTF16toUTF8(newheader).get());
      }

			compose.SetAttachments( &m_attachments);

      // See if it's a drafts folder. Outlook doesn't allow drafts
      // folder to be configured so it's ok to hard code it here.
      nsAutoString folderName(pName);
      nsMsgDeliverMode mode = nsIMsgSend::nsMsgDeliverNow;
      mode = nsIMsgSend::nsMsgSaveAsDraft;
      if ( folderName.LowerCaseEqualsLiteral("drafts") )
        mode = nsIMsgSend::nsMsgSaveAsDraft;
			
			/*
				If I can't get no headers,
				I can't get no satisfaction
			*/
			if (msg.GetHeaderLen()) {
        nsCAutoString cType;
        SetDefaultContentType(msg, cType);
				rv = compose.SendTheMessage( compositionFile, mode, cType);
				if (NS_SUCCEEDED( rv)) {
					rv = compose.CopyComposedMessage( fromLine, compositionFile, pDest, copy);
					DeleteFile( compositionFile);
					if (NS_FAILED( rv)) {
						IMPORT_LOG0( "*** Error copying composed message to destination mailbox\n");
						return( rv);
					}
          (*pMsgCount)++;
				}
			}
			else
				rv = NS_OK;

      // The following code to write msg to folder when compose.SendTheMessage() fails is commented
      // out for now because the code doesn't handle attachments and users will complain anyway so
      // until we fix the code to handle all kinds of msgs correctly we should not even make users
      // think that all msgs are imported ok. This will also help users to identify which msgs are
      // not imported and help to debug the problem.
#if 0
			if (NS_FAILED( rv)) {
				
				/* NS_PRECONDITION( FALSE, "Manual breakpoint"); */

				IMPORT_LOG1( "Message #%d failed.\n", (int) (*pMsgCount));
				DumpAttachments();
	
				// --------------------------------------------------------------

				// This is the OLD way of writing out the message which uses
				// all kinds of crufty old crap for attachments.
				// Since we now use Compose to send attachments, 
				// this is only fallback error stuff.

				// Attachments get lost.		

				if (attachCount) {
					lostAttach = PR_TRUE;
					attachCount = 0;
				}

				BOOL needsTerminate = FALSE;
				if (!WriteMessage( pDest, &msg, attachCount, &needsTerminate)) {
					IMPORT_LOG0( "*** Error writing message\n");
					*pAbort = PR_TRUE;
					return( NS_ERROR_FAILURE);
				}
				
				if (needsTerminate) {
					if (!WriteMimeBoundary( pDest, &msg, TRUE)) {
						IMPORT_LOG0( "*** Error writing message mime boundary\n");
						*pAbort = PR_TRUE;
						return( NS_ERROR_FAILURE);
					}
				}
			}
#endif

			// Just for YUCKS, let's try an extra endline
			WriteData( pDest, "\x0D\x0A", 2);
		}
	}

	return( NS_OK);
}

BOOL nsOutlookMail::WriteMessage( nsIFileSpec *pDest, CMapiMessage *pMsg, int& attachCount, BOOL *pTerminate)
{
	BOOL		bResult = TRUE;
	const char *pData;
	int			len;
	BOOL		checkStart = FALSE;

	*pTerminate = FALSE;

	pData = pMsg->GetFromLine( len);
	if (pData) {
		bResult = WriteData( pDest, pData, len);
		checkStart = TRUE;
	}

  nsCOMPtr<nsIOutputStream> outStream;
  pDest->GetOutputStream (getter_AddRefs (outStream));
  
	pData = pMsg->GetHeaders( len);
	if (pData && len) {
    if (checkStart)
      bResult = (EscapeFromSpaceLine(outStream, (char *)pData, pData+len) == NS_OK);
    else
      bResult = (EscapeFromSpaceLine(outStream, (char *)(pData+1), pData+len-1) == NS_OK);
	}

	// Do we need to add any mime headers???
	//	Is the message multipart?
	//		If so, then we are OK, but need to make sure we add mime
	//		header info to the body of the message
	//	If not AND we have attachments, then we need to add mime headers.
	
	BOOL needsMimeHeaders = pMsg->IsMultipart();
	if (!needsMimeHeaders && attachCount) {
		// if the message already has mime headers
		// that aren't multipart then we are in trouble!
		// in that case, ditch the attachments...  alternatively, we could
		// massage the headers and replace the existing mime headers
		// with our own but I think this case is likely not to occur.
		if (pMsg->HasContentHeader())
			attachCount = 0;
		else {
			if (bResult)
				bResult = WriteMimeMsgHeader( pDest, pMsg);
			needsMimeHeaders = TRUE;
		}
	}

	if (bResult)
		bResult = WriteStr( pDest, "\x0D\x0A");
	
	if (needsMimeHeaders) {
		if (bResult)
			bResult = WriteStr( pDest, "This is a MIME formatted message.\x0D\x0A");
		if (bResult)
			bResult = WriteStr( pDest, "\x0D\x0A");
		if (bResult)
			bResult = WriteMimeBoundary( pDest, pMsg, FALSE);
		if (pMsg->BodyIsHtml()) {
			if (bResult)
				bResult = WriteStr( pDest, "Content-type: text/html\x0D\x0A");
		}
		else {
			if (bResult)
				bResult = WriteStr( pDest, "Content-type: text/plain\x0D\x0A");
		}

		if (bResult)
			bResult = WriteStr( pDest, "\x0D\x0A");
	}


	pData = pMsg->GetBody( len);
	if (pData && len) {
		if (bResult)
			bResult = (EscapeFromSpaceLine(outStream, (char *)pData, pData+len) == NS_OK);
		if ((len < 2) || (pData[len - 1] != 0x0A) || (pData[len - 2] != 0x0D))
			bResult = WriteStr( pDest, "\x0D\x0A");
	}

	*pTerminate = needsMimeHeaders;

	return( bResult);
}


BOOL nsOutlookMail::WriteData( nsIFileSpec *pDest, const char *pData, PRInt32 len)
{	
	PRInt32		written;
	nsresult rv = pDest->Write( pData, len, &written);
	if (NS_FAILED( rv) || (written != len))
		return( FALSE);
	return( TRUE);
}

BOOL nsOutlookMail::WriteStr( nsIFileSpec *pDest, const char *pStr)
{	
	PRInt32		written;
	PRInt32		len = strlen( pStr);

	nsresult rv = pDest->Write( pStr, len, &written);
	if (NS_FAILED( rv) || (written != len))
		return( FALSE);
	return( TRUE);
}

BOOL nsOutlookMail::WriteMimeMsgHeader( nsIFileSpec *pDest, CMapiMessage *pMsg)
{
	BOOL	bResult = TRUE;
	if (!pMsg->HasMimeVersion())
		bResult = WriteStr( pDest, "MIME-Version: 1.0\x0D\x0A");
	pMsg->GenerateBoundary();
	if (bResult)
		bResult = WriteStr( pDest, "Content-type: multipart/mixed; boundary=\"");
	if (bResult)
		bResult = WriteStr( pDest, pMsg->GetMimeBoundary());
	if (bResult)
		bResult = WriteStr( pDest, "\"\x0D\x0A");

	return( bResult);
}

BOOL nsOutlookMail::WriteMimeBoundary( nsIFileSpec *pDest, CMapiMessage *pMsg, BOOL terminate)
{
	BOOL	bResult = WriteStr( pDest, "--");
	if (bResult)
		bResult = WriteStr( pDest, pMsg->GetMimeBoundary());
	if (terminate && bResult)
		bResult = WriteStr( pDest, "--");
	if (bResult)
		bResult = WriteStr( pDest, "\x0D\x0A");

	return( bResult);
}


/*
PRBool nsOutlookMail::WriteAttachment( nsIFileSpec *pDest, CMapiMessage *pMsg)
{
	nsCOMPtr<nsIFileSpec> pSpec;
	nsresult rv = NS_NewFileSpec( getter_AddRefs( pSpec));
	if (NS_FAILED( rv) || !pSpec) {
		IMPORT_LOG0( "*** Error creating file spec for attachment\n");
		return( PR_FALSE);
	}

	if (pMsg->GetAttachFileLoc( pSpec)) {
		PRBool	isFile = PR_FALSE;
		PRBool	exists = PR_FALSE;
		pSpec->Exists( &exists);
		pSpec->IsFile( &isFile);

		if (!exists || !isFile) {
			IMPORT_LOG0( "Attachment file does not exist\n");
			return( PR_TRUE);
		}
	}
	else {
		IMPORT_LOG0( "Attachment not processed, unable to obtain file\n");
		return( PR_TRUE);
	}

	// Set up headers...
	BOOL bResult = WriteMimeBoundary( pDest, pMsg, FALSE);
	// Now set up the encoder object

	if (bResult) {
		nsCOMPtr<nsIImportMimeEncode> encoder = do_CreateInstance( kImportMimeEncodeCID, &rv);
		if (NS_FAILED( rv)) {
			IMPORT_LOG0( "*** Error creating mime encoder\n");
			return( PR_FALSE);
		}

		encoder->Initialize( pSpec, pDest, pMsg->GetFileName(), pMsg->GetMimeType());
		encoder->DoEncoding( &bResult);
	}

	return( bResult);
}
*/






nsresult nsOutlookMail::DeleteFile( nsIFileSpec *pSpec)
{
	PRBool		result;
	nsresult	rv = NS_OK;

	result = PR_FALSE;
	pSpec->IsStreamOpen( &result);
	if (result)
		pSpec->CloseStream();
	result = PR_FALSE;
	pSpec->Exists( &result);
	if (result) {
		result = PR_FALSE;
		pSpec->IsFile( &result);
		if (result) {
			nsFileSpec	spec;
			rv = pSpec->GetFileSpec( &spec);
			if (NS_SUCCEEDED( rv))
				spec.Delete( PR_FALSE);
		}
	}

	return( rv);
}

void nsOutlookMail::EmptyAttachments( void)
{
	PRBool	exists;
	PRBool	isFile;
	PRInt32 max = m_attachments.Count();
	OutlookAttachment *	pAttach;
	for (PRInt32 i = 0; i < max; i++) {
		pAttach = (OutlookAttachment *) m_attachments.ElementAt( i);
		if (pAttach) {
			if (pAttach->pAttachment) {
				exists = PR_FALSE;
				isFile = PR_FALSE;
				pAttach->pAttachment->Exists( &exists);
				if (exists)
					pAttach->pAttachment->IsFile( &isFile);
				if (exists && isFile)
					DeleteFile( pAttach->pAttachment);
				NS_RELEASE( pAttach->pAttachment);
			}
			nsCRT::free( pAttach->description);
			nsCRT::free( pAttach->mimeType);
			delete pAttach;
		}
	}

	m_attachments.Clear();
}

void nsOutlookMail::BuildAttachments( CMapiMessage& msg, int count)
{
	EmptyAttachments();
	if (count) {
		nsIFileSpec *	pSpec;
		nsresult rv;
		for (int i = 0; i < count; i++) {
			if (!msg.GetAttachmentInfo( i)) {
				IMPORT_LOG1( "*** Error getting attachment info for #%d\n", i);
			}

			pSpec = nsnull;
			rv = NS_NewFileSpec( &pSpec);
			if (NS_FAILED( rv) || !pSpec) {
				IMPORT_LOG0( "*** Error creating file spec for attachment\n");
			}
			else {
				if (msg.GetAttachFileLoc( pSpec)) {
					PRBool	isFile = PR_FALSE;
					PRBool	exists = PR_FALSE;
					pSpec->Exists( &exists);
					pSpec->IsFile( &isFile);

					if (!exists || !isFile) {
						IMPORT_LOG0( "Attachment file does not exist\n");
						NS_RELEASE( pSpec);
					}
					else {
						// We have a file spec, now get the other info
						OutlookAttachment *a = new OutlookAttachment;
						a->mimeType = nsCRT::strdup( msg.GetMimeType());
            // Init description here so that we cacn tell
            // if defaul tattacchment is needed later.
            a->description = nsnull;  

            const char *fileName = msg.GetFileName();
            if (fileName && fileName[0]) {
              // Convert description to unicode.
              nsCOMPtr<nsIImportService>	impSvc = do_GetService(NS_IMPORTSERVICE_CONTRACTID);
              NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get import service");
              if (NS_SUCCEEDED(rv)) {
                nsAutoString description;
                rv = impSvc->SystemStringToUnicode(msg.GetFileName(), description);
                NS_ASSERTION(NS_SUCCEEDED(rv), "failed to convert system string to unicode");
                if (NS_SUCCEEDED(rv))
                  a->description = ToNewUTF8String(description);
              }
            }

            // If no description use "Attachment i" format.
            if (!a->description) {
              nsCAutoString	str("Attachment ");
							str.AppendInt( (PRInt32) i);
							a->description = ToNewCString( str);
						}

						a->pAttachment = pSpec;
						m_attachments.AppendElement( a);
					}
				}
				else {
					NS_RELEASE( pSpec);
				}
			}
		}
	}
}

void nsOutlookMail::DumpAttachments( void)
{
#ifdef IMPORT_DEBUG
	PRInt32		count = 0;
	count = m_attachments.Count();
	if (!count) {
		IMPORT_LOG0( "*** No Attachments\n");
		return;
	}
	IMPORT_LOG1( "#%d attachments\n", (int) count);

	OutlookAttachment *	pAttach;
	
	for (PRInt32 i = 0; i < count; i++) {
		IMPORT_LOG1( "\tAttachment #%d ---------------\n", (int) i);
		pAttach = (OutlookAttachment *) m_attachments.ElementAt( i);
		if (pAttach->mimeType)
			IMPORT_LOG1( "\t\tMime type: %s\n", pAttach->mimeType);
		if (pAttach->description)
			IMPORT_LOG1( "\t\tDescription: %s\n", pAttach->description);
		if (pAttach->pAttachment) {
			nsXPIDLCString	path;
			pAttach->pAttachment->GetNativePath( getter_Copies( path));
			IMPORT_LOG1( "\t\tFile: %s\n", (const char *)path);
		}
	}
#endif
}

nsresult nsOutlookMail::ImportAddresses( PRUint32 *pCount, PRUint32 *pTotal, const PRUnichar *pName, PRUint32 id, nsIAddrDatabase *pDb, nsString& errors)
{
	if (id >= (PRUint32)(m_addressList.GetSize())) {
		IMPORT_LOG0( "*** Bad address identifier, unable to import\n");
		return( NS_ERROR_FAILURE);
	}
	
	PRUint32	dummyCount = 0;
	if (pCount)
		*pCount = 0;
	else
		pCount = &dummyCount;

	CMapiFolder *pFolder;
	if (id > 0) {
		PRInt32 idx = (PRInt32) id;
		idx--;
		while (idx >= 0) {
			pFolder = m_addressList.GetItem( idx);
			if (pFolder->IsStore()) {
				OpenMessageStore( pFolder);
				break;
			}
			idx--;
		}
	}

	pFolder = m_addressList.GetItem( id);
	OpenMessageStore( pFolder);
	if (!m_lpMdb) {
		IMPORT_LOG1( "*** Unable to obtain mapi message store for address book: %S\n", pName);
		return( NS_ERROR_FAILURE);
	}
	
	if (pFolder->IsStore())
		return( NS_OK);
	
	nsresult	rv;

	nsCOMPtr<nsIImportFieldMap>		pFieldMap;

	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
	if (NS_SUCCEEDED( rv)) {
		rv = impSvc->CreateNewFieldMap( getter_AddRefs( pFieldMap));
	}

	CMapiFolderContents		contents( m_lpMdb, pFolder->GetCBEntryID(), pFolder->GetEntryID());

	BOOL			done = FALSE;
	ULONG			cbEid;
	LPENTRYID		lpEid;
	ULONG			oType;
	LPMESSAGE		lpMsg;
	nsCString		type;
	LPSPropValue	pVal;
	nsString		subject;

	while (!done) {
		(*pCount)++;

		if (!contents.GetNext( &cbEid, &lpEid, &oType, &done)) {
			IMPORT_LOG1( "*** Error iterating address book: %S\n", pName);
			return( NS_ERROR_FAILURE);
		}
		
		if (pTotal && (*pTotal == 0))
			*pTotal = contents.GetCount();

		if (!done && (oType == MAPI_MESSAGE)) {
			if (!m_mapi.OpenMdbEntry( m_lpMdb, cbEid, lpEid, (LPUNKNOWN *) &lpMsg)) {
				IMPORT_LOG1( "*** Error opening messages in mailbox: %S\n", pName);
				return( NS_ERROR_FAILURE);
			}

			// Get the PR_MESSAGE_CLASS attribute,
			// ensure that it is IPM.Contact
			pVal = m_mapi.GetMapiProperty( lpMsg, PR_MESSAGE_CLASS);
			if (pVal) {
				type.Truncate( 0);
				m_mapi.GetStringFromProp( pVal, type);
				if (type.EqualsLiteral("IPM.Contact")) {
					// This is a contact, add it to the address book!
					subject.Truncate( 0);
					pVal = m_mapi.GetMapiProperty( lpMsg, PR_SUBJECT);
					if (pVal)
						m_mapi.GetStringFromProp( pVal, subject);
					
					nsIMdbRow* newRow = nsnull;
					pDb->GetNewRow( &newRow); 
					// FIXME: Check with Candice about releasing the newRow if it
					// isn't added to the database.  Candice's code in nsAddressBook
					// never releases it but that doesn't seem right to me!
					if (newRow) {
						if (BuildCard( subject.get(), pDb, newRow, lpMsg, pFieldMap)) {
							pDb->AddCardRowToDB( newRow);
						}
					}
				}
        else if (type.EqualsLiteral("IPM.DistList"))
        {
          // This is a list/group, add it to the address book!
          subject.Truncate( 0);
          pVal = m_mapi.GetMapiProperty( lpMsg, PR_SUBJECT);
          if (pVal)
            m_mapi.GetStringFromProp( pVal, subject);
          CreateList(subject.get(), pDb, lpMsg, pFieldMap);
        }
			}			

			lpMsg->Release();
		}
	}


  rv = pDb->Commit(nsAddrDBCommitType::kLargeCommit);
	return rv;
}
nsresult nsOutlookMail::CreateList( const PRUnichar * pName,
                                   nsIAddrDatabase *pDb,
                                   LPMAPIPROP pUserList,
                                   nsIImportFieldMap *pFieldMap)
{
  // If no name provided then we're done.
  if (!pName || !(*pName))
    return NS_OK;
  
  nsresult rv = NS_ERROR_FAILURE;
  // Make sure we have db to work with.
  if (!pDb)
    return rv;
  
  nsCOMPtr <nsIMdbRow> newListRow;
  rv = pDb->GetNewListRow(getter_AddRefs(newListRow));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString column;
  column.AssignWithConversion(pName);
  rv = pDb->AddListName(newListRow, column.get());
  NS_ENSURE_SUCCESS(rv, rv);
  
  HRESULT             hr;
  LPSPropValue aValue = NULL ;
  ULONG aValueCount = 0 ;
  
  LPSPropTagArray properties = NULL ;
  m_mapi.MAPIAllocateBuffer(CbNewSPropTagArray(1),
    (void **)&properties) ;
  properties->cValues = 1;
  properties->aulPropTag [0] = m_mapi.GetEmailPropertyTag(pUserList, 0x8054);
  hr = pUserList->GetProps(properties, 0, &aValueCount, &aValue) ;
  
  SBinaryArray *sa=(SBinaryArray *)&aValue->Value.bin;
  if (!sa || !sa->lpbin)
    return NS_ERROR_NULL_POINTER;
  
  LPENTRYID    lpEid;
  ULONG        cbEid;
  PRInt32        idx;
  LPMESSAGE        lpMsg;
  nsCString        type;
  LPSPropValue    pVal;
  nsString        subject;
  PRUint32 total;
  
  
  total=sa->cValues;
  for (idx = 0;idx < sa->cValues ;idx++)
  {
    lpEid= (LPENTRYID) sa->lpbin[idx].lpb;
    cbEid = sa->lpbin[idx].cb;
    
    
    if (!m_mapi.OpenEntry(cbEid, lpEid, (LPUNKNOWN *) &lpMsg))
    {
      
      IMPORT_LOG1( "*** Error opening messages in mailbox: %S\n", pName);
      return( NS_ERROR_FAILURE);
    }
    // This is a contact, add it to the address book!
    subject.Truncate( 0);
    pVal = m_mapi.GetMapiProperty( lpMsg, PR_SUBJECT);
    if (pVal)
      m_mapi.GetStringFromProp( pVal, subject);
    
    nsCOMPtr <nsIMdbRow> newRow;
    nsCOMPtr <nsIMdbRow> oldRow;
    pDb->GetNewRow( getter_AddRefs(newRow));
    if (newRow) {
      if (BuildCard( subject.get(), pDb, newRow, lpMsg, pFieldMap)) 
      {
        nsCOMPtr <nsIAbCard> userCard;
        nsCOMPtr <nsIAbCard> newCard;
        userCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        pDb->InitCardFromRow(userCard,newRow);
        
        //add card to db
        PRBool bl=PR_FALSE;
        pDb->FindRowByCard(userCard,getter_AddRefs(oldRow));
        if (oldRow)
        {
          newRow = oldRow;
        }
        else
        {
          pDb->AddCardRowToDB( newRow);
        }
        
        //add card list
        pDb->AddListCardColumnsToRow(userCard,
          newListRow,idx+1,
          getter_AddRefs(newCard),PR_TRUE);
        
        
      }
    }
    
    
  }
  
  rv = pDb->AddCardRowToDB(newListRow);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = pDb->SetListAddressTotal(newListRow, total);
  rv = pDb->AddListDirNode(newListRow);
  return rv;
}


void nsOutlookMail::SanitizeValue( nsString& val)
{
	val.ReplaceSubstring(NS_LITERAL_STRING("\x0D\x0A").get(),
                         NS_LITERAL_STRING(", ").get());
	val.ReplaceChar( 13, ',');
	val.ReplaceChar( 10, ',');
}

void nsOutlookMail::SplitString( nsString& val1, nsString& val2)
{
	nsString	temp;

	// Find the last line if there is more than one!
	PRInt32 idx = val1.RFind( "\x0D\x0A");
	PRInt32	cnt = 2;
	if (idx == -1) {
		cnt = 1;
		idx = val1.RFindChar( 13);
	}
	if (idx == -1)
		idx= val1.RFindChar( 10);
	if (idx != -1) {
		val1.Right( val2, val1.Length() - idx - cnt);
		val1.Left( temp, idx);
		val1 = temp;
		SanitizeValue( val1);
	}
}

PRBool nsOutlookMail::BuildCard( const PRUnichar *pName, nsIAddrDatabase *pDb, nsIMdbRow *newRow, LPMAPIPROP pUser, nsIImportFieldMap *pFieldMap)
{
	
	nsString		lastName;
	nsString		firstName;
	nsString		eMail;
	nsString		nickName;
	nsString		middleName;
	nsString		secondEMail;
  ULONG       emailTag;

	LPSPropValue	pProp = m_mapi.GetMapiProperty( pUser, PR_EMAIL_ADDRESS);
	if (!pProp) {
    emailTag = m_mapi.GetEmailPropertyTag(pUser, OUTLOOK_EMAIL1_MAPI_ID1);
    if (emailTag) {
	    pProp = m_mapi.GetMapiProperty( pUser, emailTag);
    }
  }
	if (pProp) {
		m_mapi.GetStringFromProp( pProp, eMail);
		SanitizeValue( eMail);
	}

  // for secondary email
  emailTag = m_mapi.GetEmailPropertyTag(pUser, OUTLOOK_EMAIL2_MAPI_ID1);
  if (emailTag) {
	  pProp = m_mapi.GetMapiProperty( pUser, emailTag);
	  if (pProp) {
		  m_mapi.GetStringFromProp( pProp, secondEMail);
		  SanitizeValue( secondEMail);
	  }
  }

	pProp = m_mapi.GetMapiProperty( pUser, PR_GIVEN_NAME);
	if (pProp) {
		m_mapi.GetStringFromProp( pProp, firstName);
		SanitizeValue( firstName);
	}
	pProp = m_mapi.GetMapiProperty( pUser, PR_SURNAME);
	if (pProp) {
		m_mapi.GetStringFromProp( pProp, lastName);
		SanitizeValue( lastName);
	}
	pProp = m_mapi.GetMapiProperty( pUser, PR_MIDDLE_NAME);
	if (pProp) {
		m_mapi.GetStringFromProp( pProp, middleName);
		SanitizeValue( middleName);
	}
	pProp = m_mapi.GetMapiProperty( pUser, PR_NICKNAME);
	if (pProp) {
		m_mapi.GetStringFromProp( pProp, nickName);
		SanitizeValue( nickName);
	}
	if (firstName.IsEmpty() && lastName.IsEmpty()) {
		firstName = pName;
	}

	nsString	displayName;
	pProp = m_mapi.GetMapiProperty( pUser, PR_DISPLAY_NAME);
	if (pProp) {
		m_mapi.GetStringFromProp( pProp, displayName);
		SanitizeValue( displayName);
	}
	if (displayName.IsEmpty()) {
		if (firstName.IsEmpty())
			displayName = pName;
		else {
			displayName = firstName;
			if (!middleName.IsEmpty()) {
				displayName.Append( PRUnichar(' '));
				displayName.Append( middleName);
			}
			if (!lastName.IsEmpty()) {
				displayName.Append( PRUnichar(' '));
				displayName.Append( lastName);
			}
		}
	}
	
	// We now have the required fields
	// write them out followed by any optional fields!
	if (!displayName.IsEmpty()) {
		pDb->AddDisplayName( newRow, NS_ConvertUTF16toUTF8(displayName).get());
	}
	if (!firstName.IsEmpty()) {
		pDb->AddFirstName( newRow, NS_ConvertUTF16toUTF8(firstName).get());
	}
	if (!lastName.IsEmpty()) {
		pDb->AddLastName( newRow, NS_ConvertUTF16toUTF8(lastName).get());
	}
	if (!nickName.IsEmpty()) {
		pDb->AddNickName( newRow, NS_ConvertUTF16toUTF8(nickName).get());
	}
	if (!eMail.IsEmpty()) {
		pDb->AddPrimaryEmail( newRow, NS_ConvertUTF16toUTF8(eMail).get());
	}
	if (!secondEMail.IsEmpty()) {
		pDb->Add2ndEmail( newRow, NS_ConvertUTF16toUTF8(secondEMail).get());
	}

	// Do all of the extra fields!

	nsString	value;
	nsString	line2;

	if (pFieldMap) {
		int max = sizeof( gMapiFields) / sizeof( MAPIFields);
		for (int i = 0; i < max; i++) {
			pProp = m_mapi.GetMapiProperty( pUser, gMapiFields[i].mapiTag);
			if (pProp) {
				m_mapi.GetStringFromProp( pProp, value);
				if (!value.IsEmpty()) {
					if (gMapiFields[i].multiLine == kNoMultiLine) {
						SanitizeValue( value);
						pFieldMap->SetFieldValue( pDb, newRow, gMapiFields[i].mozField, value.get());
					}
					else if (gMapiFields[i].multiLine == kIsMultiLine) {
						pFieldMap->SetFieldValue( pDb, newRow, gMapiFields[i].mozField, value.get());
					}
					else {
						line2.Truncate();
						SplitString( value, line2);
						if (!value.IsEmpty())
							pFieldMap->SetFieldValue( pDb, newRow, gMapiFields[i].mozField, value.get());
						if (!line2.IsEmpty())
							pFieldMap->SetFieldValue( pDb, newRow, gMapiFields[i].multiLine, line2.get());
					}
				}
			}
		}
	}


	return( PR_TRUE);
}
