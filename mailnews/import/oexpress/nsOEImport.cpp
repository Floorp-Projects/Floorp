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

  A sample of XPConnect. This file contains an implementation of
  nsISample.

*/
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsOEImport.h"
#include "nsIAllocator.h"
#include "nsOEScanBoxes.h"
#include "nsIImportService.h"
#include "nsIImportMail.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportGeneric.h"
#include "nsOEMailbox.h"
#include "nsIImportAddressBooks.h"
#include "nsIImportABDescriptor.h"
#include "WabObject.h"
#include "nsOEAddressIterator.h"
#include "nsIOutputStream.h"
#include "nsOE5File.h"
#include "nsIAddrDatabase.h"

#include "OEDebugLog.h"


static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);



class ImportMailImpl : public nsIImportMail
{
public:
    ImportMailImpl();
    virtual ~ImportMailImpl();

	static nsresult Create(nsIImportMail** aImport);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIImportmail interface
  
	/* void GetDefaultLocation (out nsIFileSpec location, out boolean found, out boolean userVerify); */
	NS_IMETHOD GetDefaultLocation(nsIFileSpec **location, PRBool *found, PRBool *userVerify);
	
	/* nsISupportsArray FindMailboxes (in nsIFileSpec location); */
	NS_IMETHOD FindMailboxes(nsIFileSpec *location, nsISupportsArray **_retval);
	
	/* void ImportMailbox (in nsIImportMailboxDescriptor source, in nsIFileSpec destination, out boolean fatalError); */
	NS_IMETHOD ImportMailbox(nsIImportMailboxDescriptor *source, nsIFileSpec *destination, 
								nsIOutputStream *pErrorLog, nsIOutputStream *pSuccessLog, PRBool *fatalError);
	
	/* unsigned long GetImportProgress (); */
	NS_IMETHOD GetImportProgress(PRUint32 *_retval);
	
	/* wstring GetErrorString (); */
	NS_IMETHOD GetErrorString(PRUnichar **_retval);

private:
};

class ImportAddressImpl : public nsIImportAddressBooks
{
public:
    ImportAddressImpl();
    virtual ~ImportAddressImpl();

	static nsresult Create(nsIImportAddressBooks** aImport);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIImportAddressBooks interface
    
	/* PRBool GetSupportsMultiple (); */
	NS_IMETHOD GetSupportsMultiple(PRBool *_retval) { *_retval = PR_FALSE; return( NS_OK);}
	
	/* PRBool GetAutoFind (out wstring description); */
	NS_IMETHOD GetAutoFind(PRUnichar **description, PRBool *_retval);
	
	/* PRBool GetNeedsFieldMap (); */
	NS_IMETHOD GetNeedsFieldMap(PRBool *_retval) { *_retval = PR_FALSE; return( NS_OK);}
	
	/* void GetDefaultLocation (out nsIFileSpec location, out boolean found, out boolean userVerify); */
	NS_IMETHOD GetDefaultLocation(nsIFileSpec **location, PRBool *found, PRBool *userVerify)
		{ return( NS_ERROR_FAILURE);}
	
	/* nsISupportsArray FindAddressBooks (in nsIFileSpec location); */
	NS_IMETHOD FindAddressBooks(nsIFileSpec *location, nsISupportsArray **_retval);
	
	/* nsISupports GetFieldMap (in nsIImportABDescriptor source); */
	NS_IMETHOD GetFieldMap(nsIImportABDescriptor *source, nsISupports **_retval)
		{ return( NS_ERROR_FAILURE); }
	
	/* void ImportAddressBook (in nsIImportABDescriptor source, in nsISupports destination, in nsISupports fieldMap, out boolean fatalError); */
	NS_IMETHOD ImportAddressBook(	nsIImportABDescriptor *source, 
									nsIAddrDatabase *	destination, 
									nsISupports *		fieldMap, 
									nsIOutputStream *	errorLog,
									nsIOutputStream *	successLog,
									PRBool *			fatalError);
	
	/* unsigned long GetImportProgress (); */
	NS_IMETHOD GetImportProgress(PRUint32 *_retval);
 

private:
	CWAB *	m_pWab;
};
////////////////////////////////////////////////////////////////////////

nsresult NS_NewOEImport(nsIImportModule** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new nsOEImport();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


nsOEImport::nsOEImport()
{
    NS_INIT_REFCNT();

	IMPORT_LOG0( "nsOEImport Module Created\n");

}


nsOEImport::~nsOEImport()
{

	IMPORT_LOG0( "nsOEImport Module Deleted\n");

}



NS_IMPL_ISUPPORTS(nsOEImport, nsIImportModule::GetIID());


NS_IMETHODIMP nsOEImport::GetName( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	nsString	title = "Outlook Express";
	*name = title.ToNewUnicode();
		
    return NS_OK;
}

NS_IMETHODIMP nsOEImport::GetDescription( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	nsString	desc = "Outlook Express mail and address books";
	*name = desc.ToNewUnicode();
		
    return NS_OK;
}

NS_IMETHODIMP nsOEImport::GetSupports( char **supports)
{
    NS_PRECONDITION(supports != nsnull, "null ptr");
    if (! supports)
        return NS_ERROR_NULL_POINTER;
       
	*supports = nsCRT::strdup( kOESupportsString);
	return( NS_OK);
}

NS_IMETHODIMP nsOEImport::GetImportInterface( const char *pImportType, nsISupports **ppInterface)
{
    NS_PRECONDITION(pImportType != nsnull, "null ptr");
    if (! pImportType)
        return NS_ERROR_NULL_POINTER;
    NS_PRECONDITION(ppInterface != nsnull, "null ptr");
    if (! ppInterface)
        return NS_ERROR_NULL_POINTER;

	*ppInterface = nsnull;
	nsresult	rv;
	if (!nsCRT::strcmp( pImportType, "mail")) {
		// create the nsIImportMail interface and return it!
		nsIImportMail *	pMail = nsnull;
		nsIImportGeneric *pGeneric = nsnull;
		rv = ImportMailImpl::Create( &pMail);
		if (NS_SUCCEEDED( rv)) {
			NS_WITH_SERVICE( nsIImportService, impSvc, kImportServiceCID, &rv);
			if (NS_SUCCEEDED( rv)) {
				rv = impSvc->CreateNewGenericMail( &pGeneric);
				if (NS_SUCCEEDED( rv)) {
					pGeneric->SetData( "mailInterface", pMail);
					rv = pGeneric->QueryInterface( kISupportsIID, (void **)ppInterface);
				}
			}
		}
		NS_IF_RELEASE( pMail);
		NS_IF_RELEASE( pGeneric);
		return( rv);
	}
	
	if (!nsCRT::strcmp( pImportType, "addressbook")) {
		// create the nsIImportMail interface and return it!
		nsIImportAddressBooks *	pAddress = nsnull;
		nsIImportGeneric *		pGeneric = nsnull;
		rv = ImportAddressImpl::Create( &pAddress);
		if (NS_SUCCEEDED( rv)) {
			NS_WITH_SERVICE( nsIImportService, impSvc, kImportServiceCID, &rv);
			if (NS_SUCCEEDED( rv)) {
				rv = impSvc->CreateNewGenericAddressBooks( &pGeneric);
				if (NS_SUCCEEDED( rv)) {
					pGeneric->SetData( "addressInterface", pAddress);
					rv = pGeneric->QueryInterface( kISupportsIID, (void **)ppInterface);
				}
			}
		}
		NS_IF_RELEASE( pAddress);
		NS_IF_RELEASE( pGeneric);
		return( rv);
	}
	
	return( NS_ERROR_NOT_AVAILABLE);
}

/////////////////////////////////////////////////////////////////////////////////
nsresult ImportMailImpl::Create(nsIImportMail** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new ImportMailImpl();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

ImportMailImpl::ImportMailImpl()
{
    NS_INIT_REFCNT();
}


ImportMailImpl::~ImportMailImpl()
{
}



NS_IMPL_ISUPPORTS(ImportMailImpl, nsIImportMail::GetIID());

NS_IMETHODIMP ImportMailImpl::GetDefaultLocation( nsIFileSpec **ppLoc, PRBool *found, PRBool *userVerify)
{
    NS_PRECONDITION(ppLoc != nsnull, "null ptr");
    NS_PRECONDITION(found != nsnull, "null ptr");
    NS_PRECONDITION(userVerify != nsnull, "null ptr");
    if (!ppLoc || !found || !userVerify)
        return NS_ERROR_NULL_POINTER;

	// use scanboxes to find the location.
	nsresult	rv;
	nsIFileSpec *	spec;
	if (NS_FAILED( rv = NS_NewFileSpec( &spec)))
		return( rv);
	
	if (nsOEScanBoxes::FindMail( spec)) {
		*found = PR_TRUE;
		*ppLoc = spec;
	}
	else {
		*found = PR_FALSE;
		*ppLoc = nsnull;
		spec->Release();
	}
	*userVerify = PR_TRUE;
	return( NS_OK);
}


NS_IMETHODIMP ImportMailImpl::FindMailboxes( nsIFileSpec *pLoc, nsISupportsArray **ppArray)
{
    NS_PRECONDITION(pLoc != nsnull, "null ptr");
    NS_PRECONDITION(ppArray != nsnull, "null ptr");
    if (!pLoc || !ppArray)
        return NS_ERROR_NULL_POINTER;
	
	nsOEScanBoxes	scan;
	
	if (!scan.GetMailboxes( pLoc, ppArray))
		*ppArray = nsnull;
	
	return( NS_OK);
}


NS_IMETHODIMP ImportMailImpl::ImportMailbox(	nsIImportMailboxDescriptor *pSource, 
												nsIFileSpec *pDestination, 
												nsIOutputStream *pErrorLog,
												nsIOutputStream *pSuccessLog,
												PRBool *fatalError)
{
    NS_PRECONDITION(pSource != nsnull, "null ptr");
    NS_PRECONDITION(pDestination != nsnull, "null ptr");
    NS_PRECONDITION(fatalError != nsnull, "null ptr");
    if (!pSource || !pDestination || !fatalError)
        return NS_ERROR_NULL_POINTER;
      
    PRBool		abort = PR_FALSE;
    nsString	name;
    PRUnichar *	pName;
    if (NS_SUCCEEDED( pSource->GetDisplayName( &pName))) {
    	name = pName;
    	nsCRT::free( pName);
    }
    
	PRUint32 mailSize = 0;
	pSource->GetSize( &mailSize);
	if (mailSize == 0) {
		return( NS_OK);
	}

    nsIFileSpec	*	inFile;
    if (NS_FAILED( pSource->GetFileSpec( &inFile))) {
    	return( NS_ERROR_FAILURE);
    }

#ifdef IMPORT_DEBUG
	char *pPath;
	inFile->GetNativePath( &pPath);    
	IMPORT_LOG1( "Impot mailbox: %s\n", pPath);
	nsCRT::free( pPath);
#endif
    
    nsresult rv;
	if (nsOE5File::IsLocalMailFile( inFile)) {
		IMPORT_LOG1( "Importing OE5 mailbox: %S!\n", name.GetUnicode());
		rv = nsOE5File::ImportMailbox( &abort, name, inFile, pDestination);
	}
	else {
		if (CImportMailbox::ImportMailbox( &abort, name, inFile, pDestination)) {
    		rv = NS_OK;
		}
		else {
    		rv = NS_ERROR_FAILURE;
		}
	}
	    
    inFile->Release();
    
    return( rv);
}


NS_IMETHODIMP ImportMailImpl::GetImportProgress( PRUint32 *pDoneSoFar) 
{ 
    NS_PRECONDITION(pDoneSoFar != nsnull, "null ptr");
    if (! pDoneSoFar)
        return NS_ERROR_NULL_POINTER;
	
	// TLR: FIXME: Figure our how to update this from the import
	// of the current mailbox.
	*pDoneSoFar = 0;
	return( NS_OK);
}

NS_IMETHODIMP ImportMailImpl::GetErrorString( PRUnichar **pDisplayError) 
{ 
    NS_PRECONDITION(pDisplayError != nsnull, "null ptr");
    if (! pDisplayError)
        return NS_ERROR_NULL_POINTER;
	
	*pDisplayError = nsnull;
	return( NS_ERROR_FAILURE);
}



nsresult ImportAddressImpl::Create(nsIImportAddressBooks** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new ImportAddressImpl();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

ImportAddressImpl::ImportAddressImpl()
{
    NS_INIT_REFCNT();
	m_pWab = nsnull;
}


ImportAddressImpl::~ImportAddressImpl()
{
	if (m_pWab)
		delete m_pWab;
}



NS_IMPL_ISUPPORTS(ImportAddressImpl, nsIImportAddressBooks::GetIID());

	
NS_IMETHODIMP ImportAddressImpl::GetAutoFind(PRUnichar **description, PRBool *_retval)
{
    NS_PRECONDITION(description != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! description || !_retval)
        return NS_ERROR_NULL_POINTER;
    
    *_retval = PR_TRUE;
    nsString str = "Outlook Express address book (windows address book)";
    *description = str.ToNewUnicode();
    
    return( NS_OK);
}

	
	
NS_IMETHODIMP ImportAddressImpl::FindAddressBooks(nsIFileSpec *location, nsISupportsArray **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_NewISupportsArray( _retval);
	if (NS_FAILED( rv)) {
		return( rv);
	}
	// Make sure we can load up the windows address book...
	
	rv = NS_ERROR_FAILURE;
	
	if (m_pWab)
		delete m_pWab;
	m_pWab = new CWAB( nsnull);
	
	nsIImportABDescriptor *		pID;
	nsISupports *				pInterface;
	nsString					str = "Outlook Express Address Book";
	
	if (m_pWab->Loaded()) {
		// create a new nsIImportABDescriptor and add it to the array
		NS_WITH_SERVICE( nsIImportService, impSvc, kImportServiceCID, &rv);
		if (NS_SUCCEEDED( rv)) {
			rv = impSvc->CreateNewABDescriptor( &pID);
			if (NS_SUCCEEDED( rv)) {
				pID->SetIdentifier( 0x4F453334);
				pID->SetRef( 1);
				pID->SetSize( 100);
				pID->SetPreferredName( str.GetUnicode());
				rv = pID->QueryInterface( kISupportsIID, (void **) &pInterface);
				(*_retval)->AppendElement( pInterface);
				pInterface->Release();
				pID->Release();
			}
		}
	}
	
	if (NS_FAILED( rv)) {
		delete m_pWab;
		m_pWab = nsnull;
	}
	
	
	return( NS_OK);
}

	
	
NS_IMETHODIMP ImportAddressImpl::ImportAddressBook(	nsIImportABDescriptor *source, 
													nsIAddrDatabase *	destination, 
													nsISupports *		fieldMap, 
													nsIOutputStream *	errorLog,
													nsIOutputStream *	successLog,
													PRBool *			fatalError)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    // NS_PRECONDITION(destination != nsnull, "null ptr");
    // NS_PRECONDITION(fieldMap != nsnull, "null ptr");
    NS_PRECONDITION(fatalError != nsnull, "null ptr");
    if (!source || !fatalError)
        return NS_ERROR_NULL_POINTER;

	// we assume it is our one and only address book.
	if (!m_pWab) {
		IMPORT_LOG0( "Wab not loaded in ImportAddressBook call\n");
		return( NS_ERROR_FAILURE);
	}
    
    IMPORT_LOG0( "IMPORTING OUTLOOK EXPRESS ADDRESS BOOK\n");
    
    nsOEAddressIterator * pIter = new nsOEAddressIterator( m_pWab, destination);
    m_pWab->IterateWABContents( pIter);
    delete pIter;
    
	return( NS_OK);
}

	
NS_IMETHODIMP ImportAddressImpl::GetImportProgress(PRUint32 *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;
	
	*_retval = 0;
	return( NS_OK);
}

