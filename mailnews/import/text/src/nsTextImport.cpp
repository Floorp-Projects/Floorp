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

  Text import addressbook interfaces

*/
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsComponentManagerUtils.h"
#include "nsTextImport.h"
#include "nsIAllocator.h"
#include "nsIImportGeneric.h"
#include "nsIImportAddressBooks.h"
#include "nsIImportABDescriptor.h"
#include "nsIImportFieldMap.h"
#include "nsIOutputStream.h"
#include "nsIAddrDatabase.h"
#include "nsTextFormater.h"
#include "nsTextStringBundle.h"
#include "nsTextAddress.h"


#include "TextDebugLog.h"


static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kISupportsArrayIID,	NS_ISUPPORTSARRAY_IID);



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
	
	/* PRBool GetNeedsFieldMap (nsIFileSpec *location); */
	NS_IMETHOD GetNeedsFieldMap(nsIFileSpec *location, PRBool *_retval);
	
	/* void GetDefaultLocation (out nsIFileSpec location, out boolean found, out boolean userVerify); */
	NS_IMETHOD GetDefaultLocation(nsIFileSpec **location, PRBool *found, PRBool *userVerify);
	
	/* nsISupportsArray FindAddressBooks (in nsIFileSpec location); */
	NS_IMETHOD FindAddressBooks(nsIFileSpec *location, nsISupportsArray **_retval);
	
	/* nsISupports InitFieldMap(nsIFileSpec location, nsIImportFieldMap fieldMap); */
	NS_IMETHOD InitFieldMap(nsIFileSpec *location, nsIImportFieldMap *fieldMap)
		{ return( NS_OK); }
	
	/* void ImportAddressBook (in nsIImportABDescriptor source, in nsISupports destination, in nsISupports fieldMap, out boolean fatalError); */
	NS_IMETHOD ImportAddressBook(	nsIImportABDescriptor *source, 
									nsIAddrDatabase *	destination, 
									nsIImportFieldMap *	fieldMap, 
									PRUnichar **		errorLog,
									PRUnichar **		successLog,
									PRBool *			fatalError);
	
	/* unsigned long GetImportProgress (); */
	NS_IMETHOD GetImportProgress(PRUint32 *_retval);
 
private:
	static void	ReportSuccess( nsString& name, nsString *pStream);
	static void SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess);
	static void ReportError( PRInt32 errorNum, nsString& name, nsString *pStream);

private:
	nsTextAddress	m_text;
};


////////////////////////////////////////////////////////////////////////

nsresult NS_NewTextImport(nsIImportModule** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new nsTextImport();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


nsTextImport::nsTextImport()
{
    NS_INIT_REFCNT();

	IMPORT_LOG0( "nsTextImport Module Created\n");

}


nsTextImport::~nsTextImport()
{

	IMPORT_LOG0( "nsTextImport Module Deleted\n");

}



NS_IMPL_ISUPPORTS(nsTextImport, nsIImportModule::GetIID());


NS_IMETHODIMP nsTextImport::GetName( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	*name = nsTextStringBundle::GetStringByID( TEXTIMPORT_NAME);
		
    return NS_OK;
}

NS_IMETHODIMP nsTextImport::GetDescription( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	*name = nsTextStringBundle::GetStringByID( TEXTIMPORT_DESCRIPTION);
		
    return NS_OK;
}

NS_IMETHODIMP nsTextImport::GetSupports( char **supports)
{
    NS_PRECONDITION(supports != nsnull, "null ptr");
    if (! supports)
        return NS_ERROR_NULL_POINTER;
       
	*supports = nsCRT::strdup( kTextSupportsString);
	return( NS_OK);
}

NS_IMETHODIMP nsTextImport::GetSupportsUpgrade( PRBool *pUpgrade)
{
    NS_PRECONDITION(pUpgrade != nsnull, "null ptr");
    if (! pUpgrade)
        return NS_ERROR_NULL_POINTER;
       
	*pUpgrade = PR_FALSE;
	return( NS_OK);
}


NS_IMETHODIMP nsTextImport::GetImportInterface( const char *pImportType, nsISupports **ppInterface)
{
    NS_PRECONDITION(pImportType != nsnull, "null ptr");
    if (! pImportType)
        return NS_ERROR_NULL_POINTER;
    NS_PRECONDITION(ppInterface != nsnull, "null ptr");
    if (! ppInterface)
        return NS_ERROR_NULL_POINTER;

	*ppInterface = nsnull;
	nsresult	rv;
	
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
}


ImportAddressImpl::~ImportAddressImpl()
{
}



NS_IMPL_ISUPPORTS(ImportAddressImpl, nsIImportAddressBooks::GetIID());

	
NS_IMETHODIMP ImportAddressImpl::GetAutoFind(PRUnichar **description, PRBool *_retval)
{
    NS_PRECONDITION(description != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! description || !_retval)
        return NS_ERROR_NULL_POINTER;
    
    nsString	str;
    *_retval = PR_FALSE;
    nsTextStringBundle::GetStringByID( TEXTIMPORT_ADDRESS_NAME, str);
    *description = str.ToNewUnicode();
    
    return( NS_OK);
}


NS_IMETHODIMP ImportAddressImpl::GetDefaultLocation(nsIFileSpec **ppLoc, PRBool *found, PRBool *userVerify)
{
    NS_PRECONDITION(found != nsnull, "null ptr");
    NS_PRECONDITION(ppLoc != nsnull, "null ptr");
    NS_PRECONDITION(userVerify != nsnull, "null ptr");
    if (! found || !userVerify || !ppLoc)
        return NS_ERROR_NULL_POINTER;
    
	*ppLoc = nsnull;
	*found = PR_FALSE;
	*userVerify = PR_TRUE;
	
	return( NS_OK);    
}


	
NS_IMETHODIMP ImportAddressImpl::FindAddressBooks(nsIFileSpec *pLoc, nsISupportsArray **ppArray)
{
    NS_PRECONDITION(pLoc != nsnull, "null ptr");
    NS_PRECONDITION(ppArray != nsnull, "null ptr");
    if (!pLoc || !ppArray)
        return NS_ERROR_NULL_POINTER;
	
	*ppArray = nsnull;
	PRBool exists = PR_FALSE;
	nsresult rv = pLoc->Exists( &exists);
	if (NS_FAILED( rv) || !exists)
		return( NS_ERROR_FAILURE);
	
	PRBool isFile = PR_FALSE;
	rv = pLoc->IsFile( &isFile);
	if (NS_FAILED( rv) || !isFile)
		return( NS_ERROR_FAILURE);

	rv = m_text.DetermineDelim( pLoc);

	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error determining delimitter\n");
		return( rv);
	}


	/* Build an address book descriptor based on the file passed in! */
	nsCOMPtr<nsISupportsArray>	array;
	rv = NS_NewISupportsArray( getter_AddRefs( array));
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray\n");
		return( rv);
	}
		
	NS_WITH_SERVICE( nsIImportService, impSvc, kImportServiceCID, &rv);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed to obtain the import service\n");
		return( rv);
	}
	
	char *pName = nsnull;
	rv = pLoc->GetLeafName( &pName);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed getting leaf name of file\n");
		return( rv);
	}
	nsString	name = pName;
	PRInt32		idx = name.RFindChar( '.');
	if ((idx != -1) && (idx > 0) && ((name.Length() - idx - 1) < 5)) {
		nsString t;
		name.Left( t, idx);
		name = t;
	}

	nsCOMPtr<nsIImportABDescriptor>	desc;
	nsISupports *					pInterface;

	rv = impSvc->CreateNewABDescriptor( getter_AddRefs( desc));
	if (NS_SUCCEEDED( rv)) {
		PRUint32 sz = 0;
		pLoc->GetFileSize( &sz);	
		desc->SetPreferredName( name.GetUnicode());
		desc->SetSize( sz);
		nsIFileSpec *pSpec = nsnull;
		desc->GetFileSpec( &pSpec);
		if (pSpec) {
			pSpec->FromFileSpec( pLoc);
			NS_RELEASE( pSpec);
		}
		rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
		array->AppendElement( pInterface);
		pInterface->Release();
	}
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error creating address book descriptor for text import\n");
	}		
	else {
		rv = array->QueryInterface( kISupportsArrayIID, (void **) ppArray);
	}

	return( rv);
}

	

void ImportAddressImpl::ReportSuccess( nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the success string
	PRUnichar *pText = nsTextStringBundle::GetStringByID( TEXTIMPORT_ADDRESS_SUCCESS);
	pStream->Append( pText);
	nsTextStringBundle::FreeString( pText);
	pStream->Append( NS_LINEBREAK);
}

void ImportAddressImpl::ReportError( PRInt32 errorNum, nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the error string
	PRUnichar *pFmt = nsTextStringBundle::GetStringByID( errorNum);
	PRUnichar *pText = nsTextFormater::smprintf( pFmt, name.GetUnicode());
	pStream->Append( pText);
	nsTextFormater::smprintf_free( pText);
	nsTextStringBundle::FreeString( pFmt);
	pStream->Append( NS_LINEBREAK);
}

void ImportAddressImpl::SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess)
{
	if (pError)
		*pError = error.ToNewUnicode();
	if (pSuccess)
		*pSuccess = success.ToNewUnicode();
}


NS_IMETHODIMP ImportAddressImpl::ImportAddressBook(	nsIImportABDescriptor *pSource, 
													nsIAddrDatabase *	pDestination, 
													nsIImportFieldMap *	fieldMap, 
													PRUnichar **		pErrorLog,
													PRUnichar **		pSuccessLog,
													PRBool *			fatalError)
{
    NS_PRECONDITION(pSource != nsnull, "null ptr");
    NS_PRECONDITION(pDestination != nsnull, "null ptr");
    NS_PRECONDITION(fatalError != nsnull, "null ptr");

	nsString	success;
	nsString	error;
    if (!pSource || !pDestination || !fatalError) {
		IMPORT_LOG0( "*** Bad param passed to text address import\n");
		nsTextStringBundle::GetStringByID( TEXTIMPORT_ADDRESS_BADPARAM, error);
		if (fatalError)
			*fatalError = PR_TRUE;
		SetLogs( success, error, pErrorLog, pSuccessLog);
	    return( NS_ERROR_NULL_POINTER);
	}
    
    PRBool		abort = PR_FALSE;
    nsString	name;
    PRUnichar *	pName;
    if (NS_SUCCEEDED( pSource->GetPreferredName( &pName))) {
    	name = pName;
    	nsCRT::free( pName);
    }
    
	PRUint32 addressSize = 0;
	pSource->GetSize( &addressSize);
	if (addressSize == 0) {
		IMPORT_LOG0( "Address book size is 0, skipping import.\n");
		ReportSuccess( name, &success);
		SetLogs( success, error, pErrorLog, pSuccessLog);
		return( NS_OK);
	}

    
	nsIFileSpec	*	inFile;
    if (NS_FAILED( pSource->GetFileSpec( &inFile))) {
		ReportError( TEXTIMPORT_ADDRESS_BADSOURCEFILE, name, &error);
		SetLogs( success, error, pErrorLog, pSuccessLog);		
    	return( NS_ERROR_FAILURE);
    }

#ifdef IMPORT_DEBUG
	char *pPath;
	inFile->GetNativePath( &pPath);    
	IMPORT_LOG1( "Import address book: %s\n", pPath);
	nsCRT::free( pPath);
#endif
	
    nsresult rv = NS_OK;
	PRBool	isLDIF = PR_FALSE;
	rv = nsTextAddress::IsLDIFFile( inFile, &isLDIF);
	if (NS_FAILED( rv)) {
		inFile->Release();
		ReportError( TEXTIMPORT_ADDRESS_CONVERTERROR, name, &error);
		SetLogs( success, error, pErrorLog, pSuccessLog);
		return( rv);
	}
	
    if (isLDIF) {
		// This get's tricky, the database really requires the thing
		// to have an .ldi extension so if it doesn't we may need to
		// copy the file to a temp file with the correct name, then
		// import it!
		rv = m_text.ImportLDIF( &abort, name.GetUnicode(), inFile, pDestination, error);
	}
	else {	
		rv = m_text.ImportAddresses( &abort, name.GetUnicode(), inFile, pDestination, fieldMap, error);
	}

    inFile->Release();
	    
    
	if (NS_SUCCEEDED( rv) && error.IsEmpty()) {
		ReportSuccess( name, &success);
	}
	else {
		ReportError( TEXTIMPORT_ADDRESS_CONVERTERROR, name, &error);
	}

	SetLogs( success, error, pErrorLog, pSuccessLog);

	IMPORT_LOG0( "*** Text address import done\n");

    return( rv);
}


NS_IMETHODIMP ImportAddressImpl::GetImportProgress(PRUint32 *_retval)
{
	return( NS_OK);
}


NS_IMETHODIMP ImportAddressImpl::GetNeedsFieldMap(nsIFileSpec *location, PRBool *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    NS_PRECONDITION(location != nsnull, "null ptr");
	if (!location || !_retval)
		return( NS_ERROR_NULL_POINTER);

	*_retval = PR_TRUE;
	PRBool	exists = PR_FALSE;
	PRBool	isFile = PR_FALSE;

	nsresult rv = location->Exists( &exists);
	rv = location->IsFile( &isFile);
	
	if (!exists || !isFile)
		return( NS_ERROR_FAILURE);

	PRBool	isLDIF = PR_FALSE;
	rv = nsTextAddress::IsLDIFFile( location, &isLDIF);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error determining if file is of type LDIF\n");
		return( rv);
	}
	
	if (isLDIF)
		*_retval = PR_FALSE;
	
	return( NS_OK);
}
