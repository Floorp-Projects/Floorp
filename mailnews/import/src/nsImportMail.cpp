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

/*
	TODO: 
		* Fix the the name of the imported folder to reflect the module
		name rather than just "Imported Mail"

		* Fix the imported folder name to only call GenerateUnique if
		necessary (that way the first import won't have a zero after the
		name!).

		* Test Cancel to make sure it works!

		* Figure out how to clean-up when an import is cancelled and
		we do NOT own the destination folder.  

		* Do the right thing when creating a folder that already exists.  Should
		we create a new folder or append to the one that we found already existing?

		* Fix the default destination to create an account if it cannot find
		a suitable account to import into.

		* Determine if I need to "lock" mailboxes for import?
*/


#include "prthread.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIImportMail.h"
#include "nsIImportGeneric.h"
#include "nsIOutputStream.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsCRT.h"
#include "nsString.h"

#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsSpecialSystemDirectory.h"

#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsPOP3IncomingServer.h"
#include "nsIProfile.h"
#include "nsIMsgFolder.h"

#include "ImportDebug.h"

static NS_DEFINE_IID(kIPOP3IncomingServerIID, NS_IPOP3INCOMINGSERVER_IID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

////////////////////////////////////////////////////////////////////////

PR_STATIC_CALLBACK( void) ImportMailThread( void *stuff);


class ImportThreadData;

class nsImportGenericMail : public nsIImportGeneric
{
public:

	nsImportGenericMail();
	virtual ~nsImportGenericMail();
	
	NS_DECL_ISUPPORTS

	/* string GetNextXUL (in nsIDOMWindow wizardWindow, in nsIDOMWindow currentPage, in long currentPageId, out long newPageId); */
	NS_IMETHOD GetNextXUL(nsIDOMWindow *wizardWindow, nsIDOMWindow *currentPage, PRInt32 currentPageId, PRInt32 *newPageId, char **_retval);

	/* nsISupports GetData (in string dataId); */
	NS_IMETHOD GetData(const char *dataId, nsISupports **_retval);

	NS_IMETHOD SetData(const char *dataId, nsISupports *pData);

	NS_IMETHOD GetStatus( const char *statusKind, PRInt32 *_retval);

	/* boolean WantsProgress (); */
	NS_IMETHOD WantsProgress(PRBool *_retval);

	/* boolean BeginImport (in nsIOutputStream successLog, in nsIOutputStream errorLog); */
	NS_IMETHOD BeginImport(nsIOutputStream *successLog, nsIOutputStream *errorLog, PRBool *_retval);

	/* boolean ContinueImport (); */
	NS_IMETHOD ContinueImport(PRBool *_retval);

	/* long GetProgress (); */
	NS_IMETHOD GetProgress(PRInt32 *_retval);

	/* void CancelImport (); */
	NS_IMETHOD CancelImport(void);

private:
	PRBool	GetAccount( nsIFileSpec **ppSpec);
	PRBool	FindAccount( nsIMsgFolder **ppFolder);
	void	GetDefaultMailboxes( void);
	void	GetDefaultLocation( void);
	void	GetDefaultDestination( void);

private:
	nsIMsgFolder *		m_pDestFolder;
	PRBool				m_deleteDestFolder;
	nsIFileSpec *		m_pSrcLocation;
	PRBool				m_gotLocation;
	PRBool				m_found;
	PRBool				m_userVerify;
	nsIImportMail *		m_pInterface;
	nsISupportsArray *	m_pMailboxes;
	nsIOutputStream *	m_pSuccessLog;
	nsIOutputStream *	m_pErrorLog;
	PRUint32			m_totalSize;
	PRBool				m_doImport;
	ImportThreadData *	m_pThreadData;
};

class ImportThreadData {
public:
	PRBool					driverAlive;
	PRBool					threadAlive;
	PRBool					abort;
	PRBool					fatalError;
	PRUint32				currentTotal;
	PRUint32				currentSize;
	nsIMsgFolder *			destRoot;
	PRBool					ownsDestRoot;
	nsISupportsArray *		boxes;
	nsIImportMail *			mailImport;
	nsIOutputStream *		successLog;
	nsIOutputStream *		errorLog;

	ImportThreadData();
	~ImportThreadData();
	void DriverDelete();
	void ThreadDelete();
	void DriverAbort();
};


nsresult NS_NewGenericMail(nsIImportGeneric** aImportGeneric)
{
    NS_PRECONDITION(aImportGeneric != nsnull, "null ptr");
    if (! aImportGeneric)
        return NS_ERROR_NULL_POINTER;
	
	nsImportGenericMail *pGen = new nsImportGenericMail();

	if (pGen == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF( pGen);
	nsresult rv = pGen->QueryInterface( nsIImportGeneric::GetIID(), (void **)aImportGeneric);
	NS_RELEASE( pGen);
    
    return( rv);
}

nsImportGenericMail::nsImportGenericMail()
{
    NS_INIT_REFCNT();
	m_pSrcLocation = nsnull;
	m_found = PR_FALSE;
	m_userVerify = PR_FALSE;
	m_gotLocation = PR_FALSE;
	m_pInterface = nsnull;
	m_pMailboxes = nsnull;
	m_pSuccessLog = nsnull;
	m_pErrorLog = nsnull;
	m_totalSize = 0;
	m_doImport = PR_FALSE;
	m_pThreadData = nsnull;

	m_pDestFolder = nsnull;
	m_deleteDestFolder = PR_FALSE;
}


nsImportGenericMail::~nsImportGenericMail()
{
	if (m_pThreadData) {
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}
	
	NS_IF_RELEASE( m_pDestFolder);
	NS_IF_RELEASE( m_pSrcLocation);
	NS_IF_RELEASE( m_pInterface);
	NS_IF_RELEASE( m_pMailboxes);
	NS_IF_RELEASE( m_pSuccessLog);
	NS_IF_RELEASE( m_pErrorLog);
}



NS_IMPL_ISUPPORTS(nsImportGenericMail, nsIImportGeneric::GetIID());


NS_IMETHODIMP nsImportGenericMail::GetNextXUL(nsIDOMWindow *wizardWindow, nsIDOMWindow *currentPage, PRInt32 currentPageId, PRInt32 *newPageId, char **_retval)
{
    NS_PRECONDITION(newPageId != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!newPageId || !_retval)
        return NS_ERROR_NULL_POINTER;

	/*
		Only 1 page required, the list of mailboxes!
	*/
	if (currentPageId == 0) {
		// return the simple mail interface...
		*_retval = nsCRT::strdup( "iw-simplemail.xul");
		*newPageId = 1;
		return( NS_OK);
	}
	else {
		*_retval = nsnull;
		*newPageId = 0;
		return( NS_OK);
	}
}


NS_IMETHODIMP nsImportGenericMail::GetData(const char *dataId, nsISupports **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!_retval)
		return NS_ERROR_NULL_POINTER;
	
	*_retval = nsnull;
	if (!nsCRT::strcasecmp( dataId, "mailInterface")) {
		*_retval = m_pInterface;
		NS_IF_ADDREF( m_pInterface);
	}
	
	if (!nsCRT::strcasecmp( dataId, "mailBoxes")) {
		if (!m_pMailboxes)
			GetDefaultMailboxes();
		*_retval = m_pMailboxes;
		NS_IF_ADDREF( m_pMailboxes);
	}

	if (!nsCRT::strcasecmp( dataId, "mailLocation")) {
		if (!m_pSrcLocation)
			GetDefaultLocation();
		*_retval = m_pSrcLocation;
		NS_IF_ADDREF( m_pSrcLocation);
	}
	
	if (!nsCRT::strcasecmp( dataId, "mailDestination")) {
		if (!m_pDestFolder)
			GetDefaultDestination();
		*_retval = m_pDestFolder;
		NS_IF_ADDREF( m_pDestFolder);
	}

	return( NS_OK);
}

NS_IMETHODIMP nsImportGenericMail::SetData( const char *dataId, nsISupports *item)
{
	NS_PRECONDITION(dataId != nsnull, "null ptr");
    if (!dataId)
        return NS_ERROR_NULL_POINTER;

	if (!nsCRT::strcasecmp( dataId, "mailInterface")) {
		NS_IF_RELEASE( m_pInterface);
		if (item)
			item->QueryInterface( nsIImportMail::GetIID(), (void **) &m_pInterface);
	}
	if (!nsCRT::strcasecmp( dataId, "mailBoxes")) {
		NS_IF_RELEASE( m_pMailboxes);
		if (item)
			item->QueryInterface( nsISupportsArray::GetIID(), (void **) &m_pMailboxes);
	}
	
	if (!nsCRT::strcasecmp( dataId, "mailLocation")) {
		NS_IF_RELEASE( m_pMailboxes);
		NS_IF_RELEASE( m_pSrcLocation);
		if (item)
			item->QueryInterface( nsIFileSpec::GetIID(), (void **) &m_pSrcLocation);
	}
	
	if (!nsCRT::strcasecmp( dataId, "mailDestination")) {
		NS_IF_RELEASE( m_pDestFolder);
		if (item)
			item->QueryInterface( nsIMsgFolder::GetIID(), (void **) &m_pDestFolder);
		m_deleteDestFolder = PR_FALSE;
	}
	
	return( NS_OK);
}

NS_IMETHODIMP nsImportGenericMail::GetStatus( const char *statusKind, PRInt32 *_retval)
{
	NS_PRECONDITION(statusKind != nsnull, "null ptr");
	NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!statusKind || !_retval)
		return( NS_ERROR_NULL_POINTER);
	
	*_retval = 0;

	if (!nsCRT::strcasecmp( statusKind, "isInstalled")) {
		GetDefaultLocation();
		*_retval = (PRInt32) m_found;
	}

	if (!nsCRT::strcasecmp( statusKind, "canUserSetLocation")) {
		GetDefaultLocation();
		*_retval = (PRInt32) m_userVerify;
	}

	return( NS_OK);
}


void nsImportGenericMail::GetDefaultLocation( void)
{
	if (!m_pInterface)
		return;
	
	if (m_pSrcLocation && m_gotLocation)
		return;

	m_gotLocation = PR_TRUE;

	nsIFileSpec *	pLoc = nsnull;
	nsresult rv = m_pInterface->GetDefaultLocation( &pLoc, &m_found, &m_userVerify);
	if (!m_pSrcLocation)
		m_pSrcLocation = pLoc;
	else {
		NS_IF_RELEASE( pLoc);
	}
}

void nsImportGenericMail::GetDefaultMailboxes( void)
{
	if (!m_pInterface || m_pMailboxes || !m_pSrcLocation)
		return;
	
	nsresult rv = m_pInterface->FindMailboxes( m_pSrcLocation, &m_pMailboxes);
}

void nsImportGenericMail::GetDefaultDestination( void)
{
	if (m_pDestFolder)
		return;
	nsIMsgFolder *	rootFolder;
	m_deleteDestFolder = PR_FALSE;
	if (FindAccount( &rootFolder)) {
		// create the sub folder for our import.
		char *pName = nsnull;
		rootFolder->GenerateUniqueSubfolderName( "Imported Mail", nsnull, &pName);
		if (pName) {
			IMPORT_LOG1( "* Creating folder for importing mail: %s\n", pName);
			rootFolder->CreateSubfolder( pName);
			nsCOMPtr<nsISupports> subFolder;
			rootFolder->GetChildNamed( pName, getter_AddRefs( subFolder));
			if (subFolder) {
				subFolder->QueryInterface( nsIMsgFolder::GetIID(), (void **) &m_pDestFolder);
				if (m_pDestFolder)
					m_deleteDestFolder = PR_TRUE;
			}
		}
		rootFolder->Release();
	}
	if (!m_pDestFolder) {
		IMPORT_LOG0( "*** FAILED to create a default import destination\n");
	}
}

NS_IMETHODIMP nsImportGenericMail::WantsProgress(PRBool *_retval)
{
	NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
	
	if (m_pThreadData) {
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}

	if (!m_pMailboxes) {
		GetDefaultLocation();
		GetDefaultMailboxes();
	}

	if (!m_pDestFolder) {
		GetDefaultDestination();
	}

	PRUint32		totalSize = 0;
	PRBool			result = PR_FALSE;

	if (m_pMailboxes) {
		nsISupports *	pSupports;
		PRUint32		i;
		PRBool			import;
		PRUint32		count = 0;
		nsresult		rv;
		PRUint32		size;

		rv = m_pMailboxes->Count( &count);
		
		for (i = 0; i < count; i++) {
			pSupports = m_pMailboxes->ElementAt( i);
			if (pSupports) {
				nsCOMPtr<nsISupports> interface( dont_AddRef( pSupports));
				nsCOMPtr<nsIImportMailboxDescriptor> box( do_QueryInterface( pSupports));
				if (box) {
					import = PR_FALSE;
					size = 0;
					rv = box->GetImport( &import);
					if (import) {
						rv = box->GetSize( &size);
						result = PR_TRUE;
					}
					totalSize += size;
				}
			}
		}

		m_totalSize = totalSize;
	}
	
	m_doImport = result;

	*_retval = result;	

	return( NS_OK);
}


NS_IMETHODIMP nsImportGenericMail::BeginImport(nsIOutputStream *successLog, nsIOutputStream *errorLog, PRBool *_retval)
{
	NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;

	if (!m_doImport) {
		*_retval = PR_TRUE;
		return( NS_OK);		
	}
	
	if (!m_pInterface || !m_pMailboxes || !m_pDestFolder) {
		IMPORT_LOG0( "*** Something is not set properly, interface, mailboxes, or destination.\n");
		*_retval = PR_FALSE;
		return( NS_OK);
	}

	if (m_pThreadData) {
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}

	NS_IF_RELEASE( m_pSuccessLog);
	NS_IF_RELEASE( m_pErrorLog);
	m_pSuccessLog = successLog;
	m_pErrorLog = errorLog;
	NS_IF_ADDREF( m_pSuccessLog);
	NS_IF_ADDREF( m_pErrorLog);
	
	
	// kick off the thread to do the import!!!!
	m_pThreadData = new ImportThreadData();
	m_pThreadData->boxes = m_pMailboxes;
	NS_ADDREF( m_pMailboxes);
	m_pThreadData->mailImport = m_pInterface;
	NS_ADDREF( m_pInterface);
	m_pThreadData->errorLog = m_pErrorLog;
	NS_IF_ADDREF( m_pErrorLog);
	m_pThreadData->successLog = m_pSuccessLog;
	NS_IF_ADDREF( m_pSuccessLog);
	
	m_pThreadData->ownsDestRoot = m_deleteDestFolder;
	m_pThreadData->destRoot = m_pDestFolder;
	NS_IF_ADDREF( m_pDestFolder);


	PRThread *pThread = PR_CreateThread( PR_USER_THREAD, &ImportMailThread, m_pThreadData, 
									PR_PRIORITY_NORMAL, 
									PR_LOCAL_THREAD, 
									PR_UNJOINABLE_THREAD,
									0);
	if (!pThread) {
		m_pThreadData->ThreadDelete();
		m_pThreadData->abort = PR_TRUE;
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
		*_retval = PR_FALSE;
	}
	else
		*_retval = PR_TRUE;
					
	return( NS_OK);

}


NS_IMETHODIMP nsImportGenericMail::ContinueImport(PRBool *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
	
	*_retval = PR_TRUE;
	if (m_pThreadData) {
		if (m_pThreadData->fatalError)
			*_retval = PR_FALSE;
	}

	return( NS_OK);
}


NS_IMETHODIMP nsImportGenericMail::GetProgress(PRInt32 *_retval)
{
	// This returns the progress from the the currently
	// running import mail or import address book thread.
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;

	if (!m_pThreadData || !(m_pThreadData->threadAlive)) {
		*_retval = 100;				
		return( NS_OK);	
	}
	
	PRUint32 sz = 0;
	if (m_pThreadData->currentSize && m_pInterface) {
		if (NS_FAILED( m_pInterface->GetImportProgress( &sz)))
			sz = 0;
	}
	
	if (m_totalSize)
		*_retval = ((m_pThreadData->currentTotal + sz) * 100) / m_totalSize;
	else
		*_retval = 0;
		
	return( NS_OK);
}


NS_IMETHODIMP nsImportGenericMail::CancelImport(void)
{
	if (m_pThreadData) {
		m_pThreadData->abort = PR_TRUE;
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}

	return( NS_OK);
}


ImportThreadData::ImportThreadData()
{
	fatalError = PR_FALSE;
	driverAlive = PR_TRUE;
	threadAlive = PR_TRUE;
	abort = PR_FALSE;
	currentTotal = 0;
	currentSize = 0;
	destRoot = nsnull;
	ownsDestRoot = PR_FALSE;
	boxes = nsnull;
	mailImport = nsnull;
	successLog = nsnull;
	errorLog = nsnull;
}

ImportThreadData::~ImportThreadData()
{
	NS_IF_RELEASE( destRoot);
	NS_IF_RELEASE( boxes);
	NS_IF_RELEASE( mailImport);
	NS_IF_RELEASE( errorLog);
	NS_IF_RELEASE( successLog);
}

void ImportThreadData::DriverDelete( void)
{
	driverAlive = PR_FALSE;
	if (!driverAlive && !threadAlive)
		delete this;
}

void ImportThreadData::ThreadDelete()
{
	threadAlive = PR_FALSE;
	if (!driverAlive && !threadAlive)
		delete this;
}

void ImportThreadData::DriverAbort()
{
	if (abort && !threadAlive && destRoot) {
		if (ownsDestRoot) {
			destRoot->RecursiveDelete( PR_TRUE);
		}
		else {
			// FIXME: just delete the stuff we created?
		}
	}
	else
		abort = PR_TRUE;
	DriverDelete();
}



PR_STATIC_CALLBACK( void)
ImportMailThread( void *stuff)
{
	IMPORT_LOG0( "In Begin ImporMailThread\n");

	ImportThreadData *pData = (ImportThreadData *)stuff;
	
	IMPORT_LOG0( "In ImportMailThread!!!!!!!\n");
		
	nsresult rv = NS_OK;

	nsCOMPtr<nsIMsgFolder>  	destRoot( pData->destRoot);		
				
	PRUint32	count = 0;
	rv = pData->boxes->Count( &count);
	
	nsISupports *	pSupports;
	PRUint32		i;
	PRBool			import;
	PRUint32		size;	
	PRUint32		depth = 1;
	PRUint32		newDepth;
	nsString		lastName;
	PRUnichar *		pName;
	char *			pStr;
	
	nsCOMPtr<nsIMsgFolder>  	curFolder( destRoot);

	nsCOMPtr<nsIMsgFolder>		newFolder;
	nsCOMPtr<nsIFileSpec>  		outBox;
	nsCOMPtr<nsISupports>		subFolder;
	PRBool						exists;

	for (i = 0; (i < count) && !(pData->abort); i++) {
		pSupports = pData->boxes->ElementAt( i);
		if (pSupports) {
			nsCOMPtr<nsISupports> iFace( dont_AddRef( pSupports));
			nsCOMPtr<nsIImportMailboxDescriptor> box( do_QueryInterface( pSupports));
			if (box) {
				import = PR_FALSE;
				size = 0;
				rv = box->GetImport( &import);
				if (import)
					rv = box->GetSize( &size);
				rv = box->GetDepth( &newDepth);
				if (newDepth > depth) {
					char *	pStr = lastName.ToNewCString();
					IMPORT_LOG1( "* Finding folder for child named: %s\n", pStr);
					rv = curFolder->GetChildNamed( pStr, getter_AddRefs( subFolder));
					nsCRT::free( pStr);
					curFolder = do_QueryInterface( subFolder);
				}
				else if (newDepth < depth) {
					while (newDepth < depth) {
						nsCOMPtr<nsIFolder> parFolder;
						curFolder->GetParent( getter_AddRefs( parFolder));
						curFolder = do_QueryInterface( parFolder);
						depth--;
					}
				}
				depth = newDepth;
				pName = nsnull;
				box->GetDisplayName( &pName);
				if (pName) {
					lastName = pName;
					nsCRT::free( pName);
				}
				else
					lastName = "Unknown!";
				
				pStr = lastName.ToNewCString();
				exists = PR_FALSE;
				rv = curFolder->ContainsChildNamed( pStr, &exists);
				if (exists) {
					char *pName = nsnull;
					curFolder->GenerateUniqueSubfolderName( pStr, nsnull, &pName);
					if (pName) {
						nsCRT::free( pStr);
						pStr = pName;
					}
				}
				lastName = pStr;
				
				IMPORT_LOG1( "* Creating new import folder: %s\n", pStr);

				rv = curFolder->CreateSubfolder( pStr);
				rv = curFolder->GetChildNamed( pStr, getter_AddRefs( subFolder));
				newFolder = do_QueryInterface( subFolder);
				newFolder->GetPath( getter_AddRefs( outBox));
				
				nsCRT::free( pStr);

				if (size && import) {
					PRBool fatalError = PR_FALSE;
					pData->currentSize = size;
					rv = pData->mailImport->ImportMailbox( box, outBox, pData->errorLog, pData->successLog, &fatalError);
					pData->currentSize = 0;
					pData->currentTotal += size;
					if (fatalError) {
						pData->fatalError = PR_TRUE;
						break;
					}
				}
			}
		}
	}
	
	if (pData->abort || pData->fatalError) {
		if (pData->ownsDestRoot) {
			destRoot->RecursiveDelete( PR_TRUE);
		}
		else {
			// FIXME: just delete the stuff we created?
		}
	}

	pData->ThreadDelete();	
}


PRBool nsImportGenericMail::GetAccount( nsIFileSpec **ppSpec)
{
	nsresult	rv;
	
	*ppSpec = nsnull;

    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Failed to create a mail session!\n");
		return( PR_FALSE);
	}
	
	nsCOMPtr<nsIMsgAccountManager> accMgr;

	rv = mailSession->GetAccountManager( getter_AddRefs( accMgr));
	
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed to get account manager\n");
		return( PR_FALSE);
	}

	nsCOMPtr<nsIMsgIdentity> identity;

	rv = mailSession->GetCurrentIdentity( getter_AddRefs( identity));

	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed to get current identity\n");
		return( PR_FALSE);
	}	

	// Create a new account for the import?
	
	nsCOMPtr<nsIMsgIncomingServer> server;

	rv = nsComponentManager::CreateInstance("component://netscape/messenger/server&type=pop3",
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgIncomingServer>::GetIID(),
                                          getter_AddRefs(server));

	server->SetKey( "importedPOP1");
	server->SetType( "pop3");
	
	nsString	prettyName = "Imported Mail";
	server->SetPrettyName( (PRUnichar *) prettyName.GetUnicode());
	server->SetHostName( "imported.mail.noserver");
	server->SetUsername( "ImportTest");

	nsFileSpec profileDir;
  
	NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
	if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Unable to get profile service.\n");
		return( PR_FALSE);
	}
  
	rv = profile->GetCurrentProfileDir(&profileDir);
	if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Unable to get profile directory\n");
		return( PR_FALSE);
	}

    nsCOMPtr<nsIFileSpec>	mailDir;
    nsFileSpec				dir(profileDir);
    PRBool					dirExists;
    
    // turn profileDir into the mail dir.
    dir += "Mail";
    if (!dir.Exists()) {
      dir.CreateDir();
    }
    dir += "imported.mail";
     
    rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs( mailDir));
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Unable to create mail dir file spec\n");
		return( PR_FALSE);
	}
    
    rv = mailDir->Exists(&dirExists);
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Failed to see if mail dir exists\n");
		return( PR_FALSE);
	}
    
    if (!dirExists) {
		mailDir->CreateDir();
    }
    
    char *str = nsnull;
    mailDir->GetNativePath( &str);
    
    if (str && *str) {
		server->SetLocalPath( str);
		IMPORT_LOG1( "New account mail dir: %s\n", str);
		nsCRT::free( str);
	}
	
	// create a new account with the server and identity.
	nsCOMPtr<nsIMsgAccount>	account;
	rv = accMgr->GetAccount( "ImportedMailAccount", getter_AddRefs( account));
	if (NS_FAILED( rv)) {
		rv = accMgr->CreateAccount( getter_AddRefs( account));
		if (NS_FAILED( rv)) {
			IMPORT_LOG0( "*** Error creating new account\n");
			return( PR_FALSE);
		}
		
		rv = account->SetKey( "ImportedMailAccount");

		rv = account->SetIncomingServer( server);	
	}
		

	rv = mailDir->QueryInterface( nsIFileSpec::GetIID(), (void **) ppSpec);
	
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Unable to get nsIFileSpec interface from nsIFileSpec!\n");
		return( PR_FALSE);
	}

	return( PR_TRUE);
}

PRBool nsImportGenericMail::FindAccount( nsIMsgFolder **ppFolder)
{
	nsresult	rv;
	
	*ppFolder = nsnull;

    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Failed to create a mail session!\n");
		return( PR_FALSE);
	}
	
	nsCOMPtr<nsIMsgAccountManager> accMgr;

	rv = mailSession->GetAccountManager( getter_AddRefs( accMgr));
	
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed to get account manager\n");
		return( PR_FALSE);
	}
	
	/*
		First check the default account to see if it has a local mail
		store.  If it does then use this account to import the mail.
	*/
	char *							pServerType = nsnull;
	nsCOMPtr<nsIMsgAccount>			defAcc;
	nsCOMPtr<nsIMsgIncomingServer>	localServer;
	nsCOMPtr<nsIFolder>				rootFolder;
	rv = accMgr->GetDefaultAccount( getter_AddRefs( defAcc));
	if (NS_SUCCEEDED( rv) && (defAcc != nsnull)) {
		rv = defAcc->GetIncomingServer( getter_AddRefs( localServer));
		if (NS_SUCCEEDED( rv) && (localServer != nsnull)) {
			rv = localServer->GetType( &pServerType);
			if (NS_SUCCEEDED( rv) && pServerType &&
				(!nsCRT::strcasecmp( pServerType, "none") ||
				 !nsCRT::strcasecmp( pServerType, "POP3"))) {
				nsCRT::free( pServerType);
				rv = localServer->GetRootFolder( getter_AddRefs( rootFolder));
				if (NS_SUCCEEDED( rv) && (rootFolder != nsnull)) {
					rv = rootFolder->QueryInterface( nsIMsgFolder::GetIID(), (void **)ppFolder);
					if (NS_SUCCEEDED( rv))
						return( PR_TRUE);
				}
			}
			if (pServerType) {
				nsCRT::free( pServerType);
				pServerType = nsnull;
			}
		}
	}

	
	/*
		Either an error occurred or the default account does not exist or
		the default account is not of type "none" or "POP3".  Look for the
		first account that we can use.
	*/

	nsCOMPtr<nsISupportsArray> accounts;
	rv = accMgr->GetAccounts( getter_AddRefs( accounts));
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed to get the accounts from the account manager\n");
		return( PR_FALSE);
	}
	
	PRUint32	cnt = 0;
	accounts->Count( &cnt);
	nsISupports *	pSupports;
	for (PRUint32 i = 0; i < cnt; i++) {
		rv = accounts->GetElementAt( i, &pSupports);
		if (NS_SUCCEEDED( rv) && pSupports) {
			nsCOMPtr<nsISupports>		iFace( dont_AddRef( pSupports));
			nsCOMPtr<nsIMsgAccount>		acc( do_QueryInterface( pSupports));
			if (acc) {
				nsCOMPtr<nsIMsgIncomingServer> server;
				rv = acc->GetIncomingServer( getter_AddRefs( server));
				if (NS_SUCCEEDED( rv) && (server != nsnull)) {
					char *pType = nsnull;
					rv = server->GetType( &pType);
					if (pType) {
						if (!nsCRT::strcasecmp( pType, "none") ||
							!nsCRT::strcasecmp( pType, "POP3")) {
							nsCRT::free( pType);
							rv = server->GetRootFolder( getter_AddRefs( rootFolder));
							if (NS_SUCCEEDED( rv) && (rootFolder != nsnull)) {   
								rv = rootFolder->QueryInterface( nsIMsgFolder::GetIID(), (void **) ppFolder);
								if (NS_SUCCEEDED( rv))
									return( PR_TRUE);
							}
						}
						else {
							nsCRT::free( pType);
						}
					}
				}
			}
		}
	}
	
	return( PR_FALSE);
		
}

