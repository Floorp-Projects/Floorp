/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
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

  Eudora import mail and addressbook interfaces

*/
#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsIComponentManager.h"
#include "nsEudoraImport.h"
#include "nsIMemory.h"
#include "nsIImportService.h"
#include "nsIImportMail.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportGeneric.h"
#include "nsIImportAddressBooks.h"
#include "nsIImportABDescriptor.h"
#include "nsIImportSettings.h"
#include "nsIImportFieldMap.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIOutputStream.h"
#include "nsIAddrDatabase.h"
#include "nsTextFormatter.h"
#include "nsEudoraStringBundle.h"
#include "nsIStringBundle.h"
#include "nsEudoraSettings.h"
#include "nsReadableUtils.h"


#if defined(XP_WIN) || defined(XP_OS2)
#include "nsEudoraWin32.h"
#endif
#if defined(XP_MAC) || defined(XP_MACOSX)
#include "nsEudoraMac.h"
#endif

#include "EudoraDebugLog.h"

static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);
PRLogModuleInfo *EUDORALOGMODULE = nsnull;

class ImportEudoraMailImpl : public nsIImportMail
{
public:
    ImportEudoraMailImpl();
    virtual ~ImportEudoraMailImpl();

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
								PRUnichar **pErrorLog, PRUnichar **pSuccessLog, PRBool *fatalError);
	
	/* unsigned long GetImportProgress (); */
	NS_IMETHOD GetImportProgress(PRUint32 *_retval);
	
    NS_IMETHOD TranslateFolderName(const nsAString & aFolderName, nsAString & _retval);
	
public:
	static void	AddLinebreak( nsString *pStream);
	static void	SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess);
	static void ReportError( PRInt32 errorNum, nsString& name, nsString *pStream);


private:
	static void	ReportSuccess( nsString& name, PRInt32 count, nsString *pStream);

private:
#if defined(XP_WIN) || defined(XP_OS2)
	nsEudoraWin32	m_eudora;
#endif
#if defined(XP_MAC) || defined(XP_MACOSX)
	nsEudoraMac		m_eudora;
#endif
	PRUint32		m_bytes;
};


class ImportEudoraAddressImpl : public nsIImportAddressBooks
{
public:
    ImportEudoraAddressImpl();
    virtual ~ImportEudoraAddressImpl();

	static nsresult Create(nsIImportAddressBooks** aImport);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIImportAddressBooks interface
    
	/* PRBool GetSupportsMultiple (); */
	NS_IMETHOD GetSupportsMultiple(PRBool *_retval) { *_retval = PR_TRUE; return( NS_OK);}
	
	/* PRBool GetAutoFind (out wstring description); */
	NS_IMETHOD GetAutoFind(PRUnichar **description, PRBool *_retval);
	
	/* PRBool GetNeedsFieldMap (nsIFileSpec location); */
	NS_IMETHOD GetNeedsFieldMap(nsIFileSpec *location, PRBool *_retval) { *_retval = PR_FALSE; return( NS_OK);}
	
	/* void GetDefaultLocation (out nsIFileSpec location, out boolean found, out boolean userVerify); */
	NS_IMETHOD GetDefaultLocation(nsIFileSpec **location, PRBool *found, PRBool *userVerify);
	
	/* nsISupportsArray FindAddressBooks (in nsIFileSpec location); */
	NS_IMETHOD FindAddressBooks(nsIFileSpec *location, nsISupportsArray **_retval);
	
	/* nsISupports GetFieldMap (in nsIImportABDescriptor source); */
	NS_IMETHOD InitFieldMap(nsIFileSpec *location, nsIImportFieldMap *fieldMap)
		{ return( NS_ERROR_FAILURE); }
	
	/* void ImportAddressBook (in nsIImportABDescriptor source, in nsIAddrDatabase destination, in nsIImportFieldMap fieldMap, in boolean isAddrLocHome, out wstring errorLog, out wstring successLog, out boolean fatalError); */
	NS_IMETHOD ImportAddressBook(	nsIImportABDescriptor *source, 
									nsIAddrDatabase *	destination, 
									nsIImportFieldMap *	fieldMap, 
									PRBool isAddrLocHome, 
									PRUnichar **		errorLog,
									PRUnichar **		successLog,
									PRBool *			fatalError);
	
	/* unsigned long GetImportProgress (); */
	NS_IMETHOD GetImportProgress(PRUint32 *_retval);
 
	NS_IMETHOD GetSampleData( PRInt32 index, PRBool *pFound, PRUnichar **pStr)
		{ return( NS_ERROR_FAILURE);}

	NS_IMETHOD SetSampleLocation( nsIFileSpec *) { return( NS_OK); }

private:
	static void	ReportSuccess( nsString& name, nsString *pStream);

private:
#if defined(XP_WIN) || defined(XP_OS2)
	nsEudoraWin32	m_eudora;
#endif
#if defined(XP_MAC) || defined(XP_MACOSX)
	nsEudoraMac		m_eudora;
#endif
	PRUint32		m_bytes;
};

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


nsEudoraImport::nsEudoraImport()
{
  // Init logging module.
  if (!EUDORALOGMODULE)
    EUDORALOGMODULE = PR_NewLogModule("IMPORT");
	IMPORT_LOG0( "nsEudoraImport Module Created\n");

	nsEudoraStringBundle::GetStringBundle();
}


nsEudoraImport::~nsEudoraImport()
{

	IMPORT_LOG0( "nsEudoraImport Module Deleted\n");

}



NS_IMPL_ISUPPORTS1(nsEudoraImport, nsIImportModule)


NS_IMETHODIMP nsEudoraImport::GetName( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	// nsString	title = "Outlook Express";
	// *name = ToNewUnicode(title);
	*name = nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NAME);
		
    return NS_OK;
}

NS_IMETHODIMP nsEudoraImport::GetDescription( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	// nsString	desc = "Outlook Express mail and address books";
	// *name = ToNewUnicode(desc);
	*name = nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_DESCRIPTION);
		
    return NS_OK;
}

NS_IMETHODIMP nsEudoraImport::GetSupports( char **supports)
{
    NS_PRECONDITION(supports != nsnull, "null ptr");
    if (! supports)
        return NS_ERROR_NULL_POINTER;
       
	*supports = nsCRT::strdup( kEudoraSupportsString);
	return( NS_OK);
}

NS_IMETHODIMP nsEudoraImport::GetSupportsUpgrade( PRBool *pUpgrade)
{
    NS_PRECONDITION(pUpgrade != nsnull, "null ptr");
    if (! pUpgrade)
        return NS_ERROR_NULL_POINTER;
       
	*pUpgrade = PR_TRUE;
	return( NS_OK);
}


NS_IMETHODIMP nsEudoraImport::GetImportInterface( const char *pImportType, nsISupports **ppInterface)
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
		rv = ImportEudoraMailImpl::Create( &pMail);
		if (NS_SUCCEEDED( rv)) {
			nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
			if (NS_SUCCEEDED( rv)) {
				rv = impSvc->CreateNewGenericMail( &pGeneric);
				if (NS_SUCCEEDED( rv)) {
					pGeneric->SetData( "mailInterface", pMail);
					nsString name;
					nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NAME, name);
					nsCOMPtr<nsISupportsString> nameString (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
					if (NS_SUCCEEDED(rv)) {
						nameString->SetData(name);
						pGeneric->SetData( "name", nameString);
						rv = pGeneric->QueryInterface( kISupportsIID, (void **)ppInterface);
					}
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
		rv = ImportEudoraAddressImpl::Create( &pAddress);
		if (NS_SUCCEEDED( rv)) {
			nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
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
	
	if (!nsCRT::strcmp( pImportType, "settings")) {
		nsIImportSettings *pSettings = nsnull;
		rv = nsEudoraSettings::Create( &pSettings);
		if (NS_SUCCEEDED( rv)) {
			pSettings->QueryInterface( kISupportsIID, (void **)ppInterface);
		}
		NS_IF_RELEASE( pSettings);
		return( rv);
	}
		
	return( NS_ERROR_NOT_AVAILABLE);
}

/////////////////////////////////////////////////////////////////////////////////
nsresult ImportEudoraMailImpl::Create(nsIImportMail** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new ImportEudoraMailImpl();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

ImportEudoraMailImpl::ImportEudoraMailImpl()
{
}


ImportEudoraMailImpl::~ImportEudoraMailImpl()
{
}



NS_IMPL_THREADSAFE_ISUPPORTS1(ImportEudoraMailImpl, nsIImportMail)

NS_IMETHODIMP ImportEudoraMailImpl::GetDefaultLocation( nsIFileSpec **ppLoc, PRBool *found, PRBool *userVerify)
{
    NS_PRECONDITION(ppLoc != nsnull, "null ptr");
    NS_PRECONDITION(found != nsnull, "null ptr");
    NS_PRECONDITION(userVerify != nsnull, "null ptr");
    if (!ppLoc || !found || !userVerify)
        return NS_ERROR_NULL_POINTER;
	
	*ppLoc = nsnull;

	nsresult	rv;
	nsIFileSpec *	spec;
	if (NS_FAILED( rv = NS_NewFileSpec( &spec)))
		return( rv);
	
	*found = m_eudora.FindMailFolder( spec);
	
	if (!*found) {
		NS_RELEASE( spec);
	}
	else {
		*ppLoc = spec;
	}

	*userVerify = PR_TRUE;

	return( NS_OK);
}


NS_IMETHODIMP ImportEudoraMailImpl::FindMailboxes( nsIFileSpec *pLoc, nsISupportsArray **ppArray)
{
    NS_PRECONDITION(pLoc != nsnull, "null ptr");
    NS_PRECONDITION(ppArray != nsnull, "null ptr");
    if (!pLoc || !ppArray)
        return NS_ERROR_NULL_POINTER;
	
	PRBool exists = PR_FALSE;
	nsresult rv = pLoc->Exists( &exists);
	if (NS_FAILED( rv) || !exists)
		return( NS_ERROR_FAILURE);

	rv = m_eudora.FindMailboxes( pLoc, ppArray);
	if (NS_FAILED( rv) && *ppArray) {
		NS_RELEASE( *ppArray);
		*ppArray = nsnull;
	}
	
	return( rv);
}

void ImportEudoraMailImpl::AddLinebreak( nsString *pStream)
{
	if (pStream)
		pStream->Append( PRUnichar(nsCRT::LF));
}

void ImportEudoraMailImpl::ReportSuccess( nsString& name, PRInt32 count, nsString *pStream)
{
	if (!pStream)
		return;
	// load the success string
	nsIStringBundle *pBundle = nsEudoraStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_MAILBOX_SUCCESS, pBundle);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get(), count);
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsEudoraStringBundle::FreeString( pFmt);
	AddLinebreak( pStream);
	NS_IF_RELEASE( pBundle);
}

void ImportEudoraMailImpl::ReportError( PRInt32 errorNum, nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the error string
	nsIStringBundle *pBundle = nsEudoraStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsEudoraStringBundle::GetStringByID( errorNum);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get());
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsEudoraStringBundle::FreeString( pFmt);
	AddLinebreak( pStream);
	NS_IF_RELEASE( pBundle);
}


void ImportEudoraMailImpl::SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess)
{
	if (pError)
		*pError = ToNewUnicode(error);
	if (pSuccess)
		*pSuccess = ToNewUnicode(success);
}

NS_IMETHODIMP ImportEudoraMailImpl::ImportMailbox(	nsIImportMailboxDescriptor *pSource, 
												nsIFileSpec *pDestination, 
												PRUnichar **pErrorLog,
												PRUnichar **pSuccessLog,
												PRBool *fatalError)
{
    NS_PRECONDITION(pSource != nsnull, "null ptr");
    NS_PRECONDITION(pDestination != nsnull, "null ptr");
    NS_PRECONDITION(fatalError != nsnull, "null ptr");


	nsCOMPtr<nsIStringBundle> bundle( dont_AddRef( nsEudoraStringBundle::GetStringBundleProxy()));
	
	nsString	success;
	nsString	error;
    if (!pSource || !pDestination || !fatalError) {
		IMPORT_LOG0( "*** Bad param passed to eudora mailbox import\n");
		nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_MAILBOX_BADPARAM, error, bundle);
		if (fatalError)
			*fatalError = PR_TRUE;
		SetLogs( success, error, pErrorLog, pSuccessLog);
	    return NS_ERROR_NULL_POINTER;
	}
      
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
		IMPORT_LOG0( "Mailbox size is 0, skipping mailbox.\n");
		ReportSuccess( name, 0, &success);
		SetLogs( success, error, pErrorLog, pSuccessLog);
		return( NS_OK);
	}

    
	nsIFileSpec	*	inFile;
    if (NS_FAILED( pSource->GetFileSpec( &inFile))) {
		ReportError( EUDORAIMPORT_MAILBOX_BADSOURCEFILE, name, &error);
		SetLogs( success, error, pErrorLog, pSuccessLog);		
    	return( NS_ERROR_FAILURE);
    }

#ifdef IMPORT_DEBUG
	char *pPath;
	inFile->GetNativePath( &pPath);    
	IMPORT_LOG1( "Import mailbox: %s\n", pPath);
	nsCRT::free( pPath);
#endif
	
	    
	PRInt32	msgCount = 0;
    nsresult rv = NS_OK;
	
	m_bytes = 0;
	rv = m_eudora.ImportMailbox( &m_bytes, &abort, name.get(), inFile, pDestination, &msgCount);

    inFile->Release();

	    
    
	if (NS_SUCCEEDED( rv)) {
		ReportSuccess( name, msgCount, &success);
	}
	else {
		ReportError( EUDORAIMPORT_MAILBOX_CONVERTERROR, name, &error);
	}

	SetLogs( success, error, pErrorLog, pSuccessLog);

	IMPORT_LOG0( "*** Returning from eudora mailbox import\n");

    return( rv);
}


NS_IMETHODIMP ImportEudoraMailImpl::GetImportProgress( PRUint32 *pDoneSoFar) 
{ 
    NS_PRECONDITION(pDoneSoFar != nsnull, "null ptr");
    if (! pDoneSoFar)
        return NS_ERROR_NULL_POINTER;
	
	*pDoneSoFar = m_bytes;
	return( NS_OK);
}


NS_IMETHODIMP ImportEudoraMailImpl::TranslateFolderName(const nsAString & aFolderName, nsAString & _retval)
{
    _retval = aFolderName; 
    return NS_OK;
}

nsresult ImportEudoraAddressImpl::Create(nsIImportAddressBooks** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new ImportEudoraAddressImpl();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

ImportEudoraAddressImpl::ImportEudoraAddressImpl()
{
}


ImportEudoraAddressImpl::~ImportEudoraAddressImpl()
{
}



NS_IMPL_THREADSAFE_ISUPPORTS1(ImportEudoraAddressImpl, nsIImportAddressBooks)

	
NS_IMETHODIMP ImportEudoraAddressImpl::GetAutoFind(PRUnichar **description, PRBool *_retval)
{
    NS_PRECONDITION(description != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! description || !_retval)
        return NS_ERROR_NULL_POINTER;
    
    nsString	str;
    *_retval = PR_FALSE;
    nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NICKNAMES_NAME, str);
    *description = ToNewUnicode(str);
    
    return( NS_OK);
}


NS_IMETHODIMP ImportEudoraAddressImpl::GetDefaultLocation(nsIFileSpec **ppLoc, PRBool *found, PRBool *userVerify)
{
    NS_PRECONDITION(found != nsnull, "null ptr");
    NS_PRECONDITION(ppLoc != nsnull, "null ptr");
    NS_PRECONDITION(userVerify != nsnull, "null ptr");
    if (! found || !userVerify || !ppLoc)
        return NS_ERROR_NULL_POINTER;
    
	*ppLoc = nsnull;
	nsresult		rv;
	nsIFileSpec *	spec;
	if (NS_FAILED( rv = NS_NewFileSpec( &spec)))
		return( rv);
	
	*found = m_eudora.FindAddressFolder( spec);
	if (!(*found)) {
		NS_IF_RELEASE( spec);
	}
	else {
		*ppLoc = spec;
	}
	*userVerify = PR_TRUE;
	
	return( NS_OK);    
}


	
NS_IMETHODIMP ImportEudoraAddressImpl::FindAddressBooks(nsIFileSpec *pLoc, nsISupportsArray **ppArray)
{
    NS_PRECONDITION(pLoc != nsnull, "null ptr");
    NS_PRECONDITION(ppArray != nsnull, "null ptr");
    if (!pLoc || !ppArray)
        return NS_ERROR_NULL_POINTER;
	
	PRBool exists = PR_FALSE;
	nsresult rv = pLoc->Exists( &exists);
	if (NS_FAILED( rv) || !exists)
		return( NS_ERROR_FAILURE);

	rv = m_eudora.FindAddressBooks( pLoc, ppArray);
	if (NS_FAILED( rv) && *ppArray) {
		NS_RELEASE( *ppArray);
		*ppArray = nsnull;
	}
	
	return( rv);
}

	

void ImportEudoraAddressImpl::ReportSuccess( nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the success string
	nsIStringBundle *pBundle = nsEudoraStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_ADDRESS_SUCCESS, pBundle);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get());
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsEudoraStringBundle::FreeString( pFmt);
	ImportEudoraMailImpl::AddLinebreak( pStream);
	NS_IF_RELEASE( pBundle);
}


NS_IMETHODIMP ImportEudoraAddressImpl::ImportAddressBook(	nsIImportABDescriptor *pSource, 
													nsIAddrDatabase *	pDestination, 
													nsIImportFieldMap *	fieldMap, 
								PRBool isAddrLocHome, 
													PRUnichar **		pErrorLog,
													PRUnichar **		pSuccessLog,
													PRBool *			fatalError)
{
    NS_PRECONDITION(pSource != nsnull, "null ptr");
    NS_PRECONDITION(pDestination != nsnull, "null ptr");
    NS_PRECONDITION(fatalError != nsnull, "null ptr");
	
	nsCOMPtr<nsIStringBundle> bundle( dont_AddRef( nsEudoraStringBundle::GetStringBundleProxy()));

	nsString	success;
	nsString	error;
    if (!pSource || !pDestination || !fatalError) {
		IMPORT_LOG0( "*** Bad param passed to eudora address import\n");
		nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_ADDRESS_BADPARAM, error, bundle);
		if (fatalError)
			*fatalError = PR_TRUE;
		ImportEudoraMailImpl::SetLogs( success, error, pErrorLog, pSuccessLog);
	    return NS_ERROR_NULL_POINTER;
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
		IMPORT_LOG0( "Address book size is 0, skipping mailbox.\n");
		ReportSuccess( name, &success);
		ImportEudoraMailImpl::SetLogs( success, error, pErrorLog, pSuccessLog);
		return( NS_OK);
	}

    
	nsIFileSpec	*	inFile;
    if (NS_FAILED( pSource->GetFileSpec( &inFile))) {
		ImportEudoraMailImpl::ReportError( EUDORAIMPORT_ADDRESS_BADSOURCEFILE, name, &error);
		ImportEudoraMailImpl::SetLogs( success, error, pErrorLog, pSuccessLog);		
    	return( NS_ERROR_FAILURE);
    }

#ifdef IMPORT_DEBUG
	char *pPath;
	inFile->GetNativePath( &pPath);    
	IMPORT_LOG1( "Import address book: %s\n", pPath);
	nsCRT::free( pPath);
#endif
	
	    
    nsresult rv = NS_OK;
	
	m_bytes = 0;
	rv = m_eudora.ImportAddresses( &m_bytes, &abort, name.get(), inFile, pDestination, error);

    inFile->Release();

	    
    
	if (NS_SUCCEEDED( rv) && error.IsEmpty()) {
		ReportSuccess( name, &success);
	}
	else {
		ImportEudoraMailImpl::ReportError( EUDORAIMPORT_ADDRESS_CONVERTERROR, name, &error);
	}

	ImportEudoraMailImpl::SetLogs( success, error, pErrorLog, pSuccessLog);

	IMPORT_LOG0( "*** Returning from eudora address import\n");

    return( rv);
}

	
NS_IMETHODIMP ImportEudoraAddressImpl::GetImportProgress(PRUint32 *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!_retval)
		return( NS_ERROR_NULL_POINTER);
	
	*_retval = m_bytes;

	return( NS_OK);
}



