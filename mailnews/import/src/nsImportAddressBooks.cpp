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

#include "prthread.h"
#include "prprf.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIImportAddressBooks.h"
#include "nsIImportGeneric.h"
#include "nsIOutputStream.h"
#include "nsIImportABDescriptor.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIURL.h"

#include "nsIFileSpec.h"
#include "nsIFileLocator.h"

#include "nsIProfile.h"
#include "nsIAddrDatabase.h"
#include "nsIAddrBookSession.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsAbBaseCID.h"
#include "nsIAbDirectory.h"
#include "ImportDebug.h"

static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kAbDirectoryCID, NS_ABDIRECTORYRESOURCE_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_IID(kIStandardUrlIID, NS_IURL_IID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static const char *kDirectoryDataSourceRoot = "abdirectory://";
static const char *kCardDataSourceRoot = "abcard://";

////////////////////////////////////////////////////////////////////////

PR_STATIC_CALLBACK( void) ImportAddressThread( void *stuff);


class AddressThreadData;

class nsImportGenericAddressBooks : public nsIImportGeneric
{
public:

	nsImportGenericAddressBooks();
	virtual ~nsImportGenericAddressBooks();
	
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
	void	GetDefaultLocation( void);
	void	GetDefaultBooks( void);

private:
	nsIImportAddressBooks *		m_pInterface;
	nsISupportsArray *			m_pBooks;
	nsIFileSpec *				m_pLocation;
	PRBool						m_autoFind;
	PRUnichar *					m_description;
	PRBool						m_gotLocation;
	PRBool						m_found;
	PRBool						m_userVerify;
	nsIOutputStream *			m_pSuccessLog;
	nsIOutputStream *			m_pErrorLog;
	PRUint32					m_totalSize;
	PRBool						m_doImport;
	AddressThreadData *			m_pThreadData;
	char *						m_pDestinationUri;
};

class AddressThreadData {
public:
	PRBool						driverAlive;
	PRBool						threadAlive;
	PRBool						abort;
	PRBool						fatalError;
	PRUint32					currentTotal;
	PRUint32					currentSize;
	nsISupportsArray *			books;
	nsIImportAddressBooks *		addressImport;
	nsIOutputStream *			successLog;
	nsIOutputStream *			errorLog;
	char *						pDestinationUri;

	AddressThreadData();
	~AddressThreadData();
	void DriverDelete();
	void ThreadDelete();
	void DriverAbort();
};


nsresult NS_NewGenericAddressBooks(nsIImportGeneric** aImportGeneric)
{
    NS_PRECONDITION(aImportGeneric != nsnull, "null ptr");
    if (! aImportGeneric)
        return NS_ERROR_NULL_POINTER;
	
	nsImportGenericAddressBooks *pGen = new nsImportGenericAddressBooks();

	if (pGen == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF( pGen);
	nsresult rv = pGen->QueryInterface( nsIImportGeneric::GetIID(), (void **)aImportGeneric);
	NS_RELEASE( pGen);
    
    return( rv);
}

nsImportGenericAddressBooks::nsImportGenericAddressBooks()
{
    NS_INIT_REFCNT();
	m_pInterface = nsnull;
	m_pBooks = nsnull;
	m_pSuccessLog = nsnull;
	m_pErrorLog = nsnull;
	m_totalSize = 0;
	m_doImport = PR_FALSE;
	m_pThreadData = nsnull;
	m_pDestinationUri = nsnull;

	m_pLocation = nsnull;
	m_autoFind = PR_FALSE;
	m_description = nsnull;
	m_gotLocation = PR_FALSE;
	m_found = PR_FALSE;
	m_userVerify = PR_FALSE;
}


nsImportGenericAddressBooks::~nsImportGenericAddressBooks()
{
	if (m_pThreadData) {
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}
	
	if (m_pDestinationUri)
		nsCRT::free( m_pDestinationUri);
	
	if (m_description)
		nsCRT::free( m_description);

	NS_IF_RELEASE( m_pLocation);
	NS_IF_RELEASE( m_pInterface);
	NS_IF_RELEASE( m_pBooks);
	NS_IF_RELEASE( m_pSuccessLog);
	NS_IF_RELEASE( m_pErrorLog);
}



NS_IMPL_ISUPPORTS(nsImportGenericAddressBooks, nsIImportGeneric::GetIID());


NS_IMETHODIMP nsImportGenericAddressBooks::GetNextXUL(nsIDOMWindow *wizardWindow, nsIDOMWindow *currentPage, PRInt32 currentPageId, PRInt32 *newPageId, char **_retval)
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
		*_retval = nsCRT::strdup( "iw-simpleaddress.xul");
		*newPageId = 1;
		return( NS_OK);
	}
	else {
		*_retval = nsnull;
		*newPageId = 0;
		return( NS_OK);
	}
}


NS_IMETHODIMP nsImportGenericAddressBooks::GetData(const char *dataId, nsISupports **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!_retval)
		return NS_ERROR_NULL_POINTER;
	
	nsresult	rv;
	*_retval = nsnull;
	if (!nsCRT::strcasecmp( dataId, "addressInterface")) {
		*_retval = m_pInterface;
		NS_IF_ADDREF( m_pInterface);
	}
	
	if (!nsCRT::strcasecmp( dataId, "addressLocation")) {
		if (!m_pLocation)
			GetDefaultLocation();
		*_retval = m_pLocation;
		NS_IF_ADDREF( m_pLocation);
	}
	
	if (!nsCRT::strcasecmp( dataId, "addressBooks")) {
		if (!m_pLocation)
			GetDefaultLocation();
		if (!m_pBooks)
			GetDefaultBooks();
		*_retval = m_pBooks;
		NS_IF_ADDREF( m_pBooks);
	}
	
	if (!nsCRT::strcasecmp( dataId, "addressDestination")) {
		if (m_pDestinationUri) {
			nsCOMPtr<nsIURL>	url;
			rv = nsComponentManager::CreateInstance( kStandardUrlCID, nsnull, kIStandardUrlIID, getter_AddRefs( url));
			if (NS_SUCCEEDED( rv)) {
				url->SetSpec( m_pDestinationUri);
				*_retval = url;
				NS_IF_ADDREF( *_retval);
			}
		}
	}


	return( NS_OK);
}

NS_IMETHODIMP nsImportGenericAddressBooks::SetData( const char *dataId, nsISupports *item)
{
	NS_PRECONDITION(dataId != nsnull, "null ptr");
    if (!dataId)
        return NS_ERROR_NULL_POINTER;

	if (!nsCRT::strcasecmp( dataId, "addressInterface")) {
		NS_IF_RELEASE( m_pInterface);
		if (item)
			item->QueryInterface( nsIImportAddressBooks::GetIID(), (void **) &m_pInterface);
	}
	if (!nsCRT::strcasecmp( dataId, "addressBooks")) {
		NS_IF_RELEASE( m_pBooks);
		if (item)
			item->QueryInterface( nsISupportsArray::GetIID(), (void **) &m_pBooks);
	}
	
	if (!nsCRT::strcasecmp( dataId, "addressLocation")) {
		NS_IF_RELEASE( m_pLocation);
		if (item)
			item->QueryInterface( nsIFileSpec::GetIID(), (void **) &m_pLocation);
	}

	if (!nsCRT::strcasecmp( dataId, "addressDestination")) {
		if (item) {
			nsCOMPtr<nsIURL> url;
			item->QueryInterface( kIStandardUrlIID, getter_AddRefs( url));
			if (url) {
				if (m_pDestinationUri)
					nsCRT::free( m_pDestinationUri);
				m_pDestinationUri = nsnull;
				url->GetSpec( &m_pDestinationUri);
			}
		}
	}


	return( NS_OK);
}

NS_IMETHODIMP nsImportGenericAddressBooks::GetStatus( const char *statusKind, PRInt32 *_retval)
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
	
	if (!nsCRT::strcasecmp( statusKind, "autoFind")) {
		GetDefaultLocation();
		*_retval = (PRInt32) m_autoFind;
	}

	if (!nsCRT::strcasecmp( statusKind, "supportsMultiple")) {
		PRBool		multi = PR_FALSE;
		if (m_pInterface)
			m_pInterface->GetSupportsMultiple( &multi);
		*_retval = (PRInt32) multi;
	}


	return( NS_OK);
}

void nsImportGenericAddressBooks::GetDefaultLocation( void)
{
	if (!m_pInterface)
		return;
	
	if ((m_pLocation && m_gotLocation) || m_autoFind)
		return;
	
	if (m_description)
		nsCRT::free( m_description);
	m_description = nsnull;
	nsresult rv = m_pInterface->GetAutoFind( &m_description, &m_autoFind);
	m_gotLocation = PR_TRUE;
	if (m_autoFind) {
		m_found = PR_TRUE;
		m_userVerify = PR_FALSE;
		return;
	}

	nsIFileSpec *	pLoc = nsnull;
	rv = m_pInterface->GetDefaultLocation( &pLoc, &m_found, &m_userVerify);
	if (!m_pLocation)
		m_pLocation = pLoc;
	else {
		NS_IF_RELEASE( pLoc);
	}
}

void nsImportGenericAddressBooks::GetDefaultBooks( void)
{
	if (!m_pInterface || m_pBooks)
		return;
	
	if (!m_pLocation && !m_autoFind)
		return;

	nsresult rv = m_pInterface->FindAddressBooks( m_pLocation, &m_pBooks);
}


NS_IMETHODIMP nsImportGenericAddressBooks::WantsProgress(PRBool *_retval)
{
	NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
	
	if (m_pThreadData) {
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}

	GetDefaultLocation();
	GetDefaultBooks();

	PRUint32		totalSize = 0;
	PRBool			result = PR_FALSE;

	if (m_pBooks) {
		PRUint32		count = 0;
		nsresult 		rv = m_pBooks->Count( &count);
		nsISupports *	pSupports;
		PRUint32		i;
		PRBool			import;
		PRUint32		size;
		
		for (i = 0; i < count; i++) {
			pSupports = m_pBooks->ElementAt( i);
			if (pSupports) {
				nsCOMPtr<nsISupports> interface( dont_AddRef( pSupports));
				nsCOMPtr<nsIImportABDescriptor> book( do_QueryInterface( pSupports));
				if (book) {
					import = PR_FALSE;
					size = 0;
					rv = book->GetImport( &import);
					if (import) {
						rv = book->GetSize( &size);
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


NS_IMETHODIMP nsImportGenericAddressBooks::BeginImport(nsIOutputStream *successLog, nsIOutputStream *errorLog, PRBool *_retval)
{
	NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;

	if (!m_doImport) {
		*_retval = PR_TRUE;
		return( NS_OK);		
	}
	
	if (!m_pInterface || !m_pBooks) {
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
	m_pThreadData = new AddressThreadData();
	m_pThreadData->books = m_pBooks;
	NS_ADDREF( m_pBooks);
	m_pThreadData->addressImport = m_pInterface;
	NS_ADDREF( m_pInterface);
	m_pThreadData->errorLog = m_pErrorLog;
	NS_IF_ADDREF( m_pErrorLog);
	m_pThreadData->successLog = m_pSuccessLog;
	NS_IF_ADDREF( m_pSuccessLog);
	if (m_pDestinationUri)
		m_pThreadData->pDestinationUri = nsCRT::strdup( m_pDestinationUri);
				
	PRThread *pThread = PR_CreateThread( PR_USER_THREAD, &ImportAddressThread, m_pThreadData, 
									PR_PRIORITY_NORMAL, 
									PR_LOCAL_THREAD, 
									PR_UNJOINABLE_THREAD,
									0);
	if (!pThread) {
		m_pThreadData->ThreadDelete();
		m_pThreadData->DriverDelete();
		m_pThreadData = nsnull;
		*_retval = PR_FALSE;
	}
	else
		*_retval = PR_TRUE;
					
	return( NS_OK);

}


NS_IMETHODIMP nsImportGenericAddressBooks::ContinueImport(PRBool *_retval)
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


NS_IMETHODIMP nsImportGenericAddressBooks::GetProgress(PRInt32 *_retval)
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


NS_IMETHODIMP nsImportGenericAddressBooks::CancelImport(void)
{
	if (m_pThreadData) {
		m_pThreadData->abort = PR_TRUE;
		m_pThreadData->DriverAbort();
		m_pThreadData = nsnull;
	}

	return( NS_OK);
}


AddressThreadData::AddressThreadData()
{
	fatalError = PR_FALSE;
	driverAlive = PR_TRUE;
	threadAlive = PR_TRUE;
	abort = PR_FALSE;
	currentTotal = 0;
	currentSize = 0;
	books = nsnull;
	addressImport = nsnull;
	successLog = nsnull;
	errorLog = nsnull;
	pDestinationUri = nsnull;
}

AddressThreadData::~AddressThreadData()
{
	if (pDestinationUri)
		nsCRT::free( pDestinationUri);

	NS_IF_RELEASE( books);
	NS_IF_RELEASE( addressImport);
	NS_IF_RELEASE( errorLog);
	NS_IF_RELEASE( successLog);
}

void AddressThreadData::DriverDelete( void)
{
	driverAlive = PR_FALSE;
	if (!driverAlive && !threadAlive)
		delete this;
}

void AddressThreadData::ThreadDelete()
{
	threadAlive = PR_FALSE;
	if (!driverAlive && !threadAlive)
		delete this;
}

void AddressThreadData::DriverAbort()
{
	if (abort && !threadAlive) {
		// FIXME: Do whatever is necessary to abort what has already been imported!
	}
	else
		abort = PR_TRUE;
	DriverDelete();
}


nsIAddrDatabase *GetAddressBookFromUri( const char *pUri)
{
	return( nsnull);
}

nsIAddrDatabase *GetAddressBook( const PRUnichar *name, PRBool makeNew)
{
	nsresult			rv = NS_OK;
	nsString			theName = name;

	if (!makeNew) {
		// FIXME: How do I get the list of address books and look for a
		// specific name.  Major bogosity!
		// For now, assume we didn't find anything with that name
	}
	
	nsIAddrDatabase *	pDatabase = nsnull;

	/* Get the profile directory */
	// Note to Candice: This should return an nsIFileSpec, not a nsFileSpec
	nsFileSpec *	dbPath = nsnull;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if (NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	if (dbPath) {
		// Create a new address book file - we don't care what the file
		// name is, as long as it's unique
		(*dbPath) += "impab.mab";
		(*dbPath).MakeUnique();
		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);
		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open( dbPath, PR_TRUE, &pDatabase, PR_TRUE);
	}
	
	if (pDatabase) {
		// We made a database, add it to the UI?!?!?!?!?!?!
		// This is major bogosity again!  Why doesn't the address book
		// just handle this properly for me?  Uggggg...
		
		NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
		if (NS_SUCCEEDED(rv)) {
			nsCOMPtr<nsIRDFResource> parentResource;
			char *parentUri = PR_smprintf( "%s", kDirectoryDataSourceRoot);
			rv = rdfService->GetResource( parentUri, getter_AddRefs(parentResource));
			nsCOMPtr<nsIAbDirectory> parentDir = do_QueryInterface(parentResource);
			if (parentUri)
				PR_smprintf_free(parentUri);
			if (parentDir) {
				char *pName = theName.ToNewCString();
				char *fileName = (*dbPath).GetLeafName();
				parentDir->CreateNewDirectory( pName, fileName);
				nsCRT::free( pName);
				nsCRT::free( fileName);
			}
		}
	}

	return( pDatabase);
	
	/*
	NS_WITH_SERVICE(nsIAbDirectory, directoryFactory, kAbDirectoryCID, &rv);
	if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Unable to get nsIAbDirectory service.\n");
		return( nsnull);
	}
	
	nsString	sName = name;
	char *		cName = sName.ToNewCString();
	int			nLen = nsCRT::strlen( cName) + 10;
	char *		nName = new char[nLen];
	char *		pUri = nsnull;

	nsCOMPtr<nsIEnumerator>	nodes;
	directoryFactory->GetChildNodes( getter_AddRefs( nodes));
	if (nodes) {
		nsISupports *	pSupports = nsnull;
		int				cnt = 1;
		rv = nodes->First();
		while(NS_SUCCEEDED( rv)) {
			pSupports = nsnull;
			rv = nodes->CurrentItem( &pSupports);
			if (NS_SUCCEEDED( rv))
				rv = nodes->Next();
			if (pSupports) {
				nsCOMPtr<nsISupports> iFace( dont_AddRef( pSupports));
				nsCOMPtr<nsIAbDirectory> dir( do_QueryInterface( pSupports));
				if (dir) {
					char *dirName = nsnull;
					dir->GetDirName( &dirName);
					if (dirName && !nsCRT::strcasecmp( dirName, cName)) {
						if (!makeNew) {
							delete [] nName;
							nsCRT::free( cName);
							dir->GetDirUri( &pUri);
							return( pUri);							
						}

						rv = nodes->First();
						nsCRT::free( cName);
						cName = sName.ToNewCString();
						PR_snprintf( nName, nLen, "%s-%d", cName, cnt);
						cnt++;
						nsCRT::free( cName);
						cName = nsCRT::strdup( nName); 
					}
				}
			}
		}	
	}
	
	delete [] nName;


	nsCOMPtr<nsIAbDirectory>	newDir;
	rv = directoryFactory->CreateNewDirectory( cName, nsnull, getter_AddRefs( newDir));
	nsCRT::free( cName);
	if (NS_SUCCEEDED( rv) && newDir) {
		newDir->GetDirUri( &pUri);
	}
	
	return( pUri);
	*/
}

PR_STATIC_CALLBACK( void) ImportAddressThread( void *stuff)
{
	IMPORT_LOG0( "In Begin ImportAddressThread\n");

	AddressThreadData *pData = (AddressThreadData *)stuff;
	PRUint32	count = 0;
	nsresult 	rv = pData->books->Count( &count);
	
	nsISupports *				pSupports;
	PRUint32					i;
	PRBool						import;
	PRUint32					size;
	nsCOMPtr<nsIAddrDatabase>	destDB( getter_AddRefs( GetAddressBookFromUri( pData->pDestinationUri)));
	nsIAddrDatabase *			pDestDB = nsnull;

	for (i = 0; (i < count) && !(pData->abort); i++) {
		pSupports = pData->books->ElementAt( i);
		if (pSupports) {
			nsCOMPtr<nsISupports> interface( dont_AddRef( pSupports));
			nsCOMPtr<nsIImportABDescriptor> book( do_QueryInterface( pSupports));
			if (book) {
				import = PR_FALSE;
				size = 0;
				rv = book->GetImport( &import);
				if (import)
					rv = book->GetSize( &size);
				
				if (size && import) {
					PRUnichar *pName;
					book->GetPreferredName( &pName);
					if (destDB) {
						destDB->QueryInterface( nsIAddrDatabase::GetIID(), (void **)&pDestDB);
					}
					else {
						pDestDB = GetAddressBook( pName, PR_TRUE);
					}

					nsCRT::free( pName);

					PRBool fatalError = PR_FALSE;
					pData->currentSize = size;
					if (pDestDB) {
						rv = pData->addressImport->ImportAddressBook(	book, 
																	pDestDB, // destination
																	nsnull, // fieldmap
																	pData->errorLog,
																	pData->successLog,
																	&fatalError);
					}

					pData->currentSize = 0;
					pData->currentTotal += size;
					
					NS_IF_RELEASE( pDestDB);

					if (fatalError) {
						pData->fatalError = PR_TRUE;
						break;
					}
				}
			}
		}
	}
	
	if (pData->abort || pData->fatalError) {
		// FIXME: do what is necessary to get rid of what has been imported so far.
		// Nothing if we went into an existing address book!  Otherwise, delete
		// the ones we created?
	}

	pData->ThreadDelete();	
}
