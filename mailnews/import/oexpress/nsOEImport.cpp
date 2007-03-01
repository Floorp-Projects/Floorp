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

  Outlook Express (Win32) import mail and addressbook interfaces

*/
#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsOEImport.h"
#include "nsIMemory.h"
#include "nsOEScanBoxes.h"
#include "nsIImportService.h"
#include "nsIImportMail.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportGeneric.h"
#include "nsOEMailbox.h"
#include "nsIImportAddressBooks.h"
#include "nsIImportABDescriptor.h"
#include "nsIImportFieldMap.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "WabObject.h"
#include "nsOEAddressIterator.h"
#include "nsIOutputStream.h"
#include "nsOE5File.h"
#include "nsIAddrDatabase.h"
#include "nsOESettings.h"
#include "nsTextFormatter.h"
#include "nsOEStringBundle.h"
#include "nsIStringBundle.h"
#include "nsUnicharUtils.h"

#include "OEDebugLog.h"

static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);
PRLogModuleInfo *OELOGMODULE = nsnull;

class ImportOEMailImpl : public nsIImportMail
{
public:
    ImportOEMailImpl();
    virtual ~ImportOEMailImpl();

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
	static void	ReportSuccess( nsString& name, PRInt32 count, nsString *pStream);
	static void ReportError( PRInt32 errorNum, nsString& name, nsString *pStream);
	static void	AddLinebreak( nsString *pStream);
	static void	SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess);

private:
	PRUint32	m_bytesDone;
};


class ImportOEAddressImpl : public nsIImportAddressBooks
{
public:
    ImportOEAddressImpl();
    virtual ~ImportOEAddressImpl();

	static nsresult Create(nsIImportAddressBooks** aImport);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIImportAddressBooks interface
    
	/* PRBool GetSupportsMultiple (); */
	NS_IMETHOD GetSupportsMultiple(PRBool *_retval) { *_retval = PR_FALSE; return( NS_OK);}
	
	/* PRBool GetAutoFind (out wstring description); */
	NS_IMETHOD GetAutoFind(PRUnichar **description, PRBool *_retval);
	
	/* PRBool GetNeedsFieldMap ( nsIFileSpec *location); */
	NS_IMETHOD GetNeedsFieldMap(nsIFileSpec *pLoc, PRBool *_retval) { *_retval = PR_FALSE; return( NS_OK);}
	
	/* void GetDefaultLocation (out nsIFileSpec location, out boolean found, out boolean userVerify); */
	NS_IMETHOD GetDefaultLocation(nsIFileSpec **location, PRBool *found, PRBool *userVerify)
		{ return( NS_ERROR_FAILURE);}
	
	/* nsISupportsArray FindAddressBooks (in nsIFileSpec location); */
	NS_IMETHOD FindAddressBooks(nsIFileSpec *location, nsISupportsArray **_retval);
	
	/* nsISupports InitFieldMap(nsIFileSpec *location, nsIImportFieldMap *fieldMap); */
	NS_IMETHOD InitFieldMap(nsIImportFieldMap *fieldMap)
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
	CWAB *	m_pWab;
	int		m_doneSoFar;
};
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////


nsOEImport::nsOEImport()
{
  // Init logging module.
  if (!OELOGMODULE)
    OELOGMODULE = PR_NewLogModule("IMPORT");
	IMPORT_LOG0( "nsOEImport Module Created\n");

	nsOEStringBundle::GetStringBundle();
}


nsOEImport::~nsOEImport()
{

	IMPORT_LOG0( "nsOEImport Module Deleted\n");

}



NS_IMPL_ISUPPORTS1(nsOEImport, nsIImportModule)


NS_IMETHODIMP nsOEImport::GetName( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	// nsString	title = "Outlook Express";
	// *name = ToNewUnicode(title);
	*name = nsOEStringBundle::GetStringByID( OEIMPORT_NAME);
		
    return NS_OK;
}

NS_IMETHODIMP nsOEImport::GetDescription( PRUnichar **name)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    if (! name)
        return NS_ERROR_NULL_POINTER;

	// nsString	desc = "Outlook Express mail and address books";
	// *name = ToNewUnicode(desc);
	*name = nsOEStringBundle::GetStringByID( OEIMPORT_DESCRIPTION);
		
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


NS_IMETHODIMP nsOEImport::GetSupportsUpgrade( PRBool *pUpgrade)
{
    NS_PRECONDITION(pUpgrade != nsnull, "null ptr");
    if (! pUpgrade)
        return NS_ERROR_NULL_POINTER;
       
	*pUpgrade = PR_TRUE;
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
		rv = ImportOEMailImpl::Create( &pMail);
		if (NS_SUCCEEDED( rv)) {
			nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
			if (NS_SUCCEEDED( rv)) {
				rv = impSvc->CreateNewGenericMail( &pGeneric);
				if (NS_SUCCEEDED( rv)) {
					pGeneric->SetData( "mailInterface", pMail);
					nsString name;
					nsOEStringBundle::GetStringByID( OEIMPORT_NAME, name);
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
		rv = ImportOEAddressImpl::Create( &pAddress);
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
		rv = nsOESettings::Create( &pSettings);
		if (NS_SUCCEEDED( rv)) {
			pSettings->QueryInterface( kISupportsIID, (void **)ppInterface);
		}
		NS_IF_RELEASE( pSettings);
		return( rv);
	}
		
	return( NS_ERROR_NOT_AVAILABLE);
}

/////////////////////////////////////////////////////////////////////////////////
nsresult ImportOEMailImpl::Create(nsIImportMail** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new ImportOEMailImpl();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

ImportOEMailImpl::ImportOEMailImpl()
{
}


ImportOEMailImpl::~ImportOEMailImpl()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(ImportOEMailImpl, nsIImportMail)

NS_IMETHODIMP ImportOEMailImpl::TranslateFolderName(const nsAString & aFolderName, nsAString & _retval)
{
    if (aFolderName.Equals(NS_LITERAL_STRING("Deleted Items"), nsCaseInsensitiveStringComparator()))
        _retval = NS_LITERAL_STRING(kDestTrashFolderName);
    else if (aFolderName.Equals(NS_LITERAL_STRING("Sent Items"), nsCaseInsensitiveStringComparator()))
        _retval = NS_LITERAL_STRING(kDestSentFolderName);
    else if (aFolderName.Equals(NS_LITERAL_STRING("Outbox"), nsCaseInsensitiveStringComparator()))
        _retval = NS_LITERAL_STRING(kDestUnsentMessagesFolderName);
    else
        _retval = aFolderName;

    return NS_OK;
}

NS_IMETHODIMP ImportOEMailImpl::GetDefaultLocation( nsIFileSpec **ppLoc, PRBool *found, PRBool *userVerify)
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


NS_IMETHODIMP ImportOEMailImpl::FindMailboxes( nsIFileSpec *pLoc, nsISupportsArray **ppArray)
{
    NS_PRECONDITION(pLoc != nsnull, "null ptr");
    NS_PRECONDITION(ppArray != nsnull, "null ptr");
    if (!pLoc || !ppArray)
        return NS_ERROR_NULL_POINTER;
	
	PRBool exists = PR_FALSE;
	nsresult rv = pLoc->Exists( &exists);
	if (NS_FAILED( rv) || !exists)
		return( NS_ERROR_FAILURE);

	nsOEScanBoxes	scan;
	
	if (!scan.GetMailboxes( pLoc, ppArray))
		*ppArray = nsnull;
	
	return( NS_OK);
}

void ImportOEMailImpl::AddLinebreak( nsString *pStream)
{
	if (pStream)
		pStream->Append( PRUnichar(nsCRT::LF));
}

void ImportOEMailImpl::ReportSuccess( nsString& name, PRInt32 count, nsString *pStream)
{
	if (!pStream)
		return;
	// load the success string
	nsIStringBundle *pBundle = nsOEStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsOEStringBundle::GetStringByID( OEIMPORT_MAILBOX_SUCCESS, pBundle);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get(), count);
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsOEStringBundle::FreeString( pFmt);
	AddLinebreak( pStream);
	NS_IF_RELEASE( pBundle);
}

void ImportOEMailImpl::ReportError( PRInt32 errorNum, nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the error string
	nsIStringBundle *pBundle = nsOEStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsOEStringBundle::GetStringByID( errorNum, pBundle);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get());
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsOEStringBundle::FreeString( pFmt);
	AddLinebreak( pStream);
	NS_IF_RELEASE( pBundle);
}


void ImportOEMailImpl::SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess)
{
	if (pError)
		*pError = ToNewUnicode(error);
	if (pSuccess)
		*pSuccess = ToNewUnicode(success);
}

NS_IMETHODIMP ImportOEMailImpl::ImportMailbox(	nsIImportMailboxDescriptor *pSource, 
                                              nsIFileSpec *pDestination, 
                                              PRUnichar **pErrorLog,
                                              PRUnichar **pSuccessLog,
                                              PRBool *fatalError)
{
  NS_PRECONDITION(pSource != nsnull, "null ptr");
  NS_PRECONDITION(pDestination != nsnull, "null ptr");
  NS_PRECONDITION(fatalError != nsnull, "null ptr");
  
  nsCOMPtr<nsIStringBundle>	bundle( dont_AddRef( nsOEStringBundle::GetStringBundleProxy()));
  
  nsString	success;
  nsString	error;
  if (!pSource || !pDestination || !fatalError) {
    nsOEStringBundle::GetStringByID( OEIMPORT_MAILBOX_BADPARAM, error, bundle);
    if (fatalError)
      *fatalError = PR_TRUE;
    SetLogs( success, error, pErrorLog, pSuccessLog);
    return NS_ERROR_NULL_POINTER;
  }
  
  PRBool		abort = PR_FALSE;
  nsString	name;
  nsXPIDLString pName;
  if (NS_SUCCEEDED( pSource->GetDisplayName( getter_Copies(pName)))) {
    name = pName;
  }
  
  PRUint32 mailSize = 0;
  pSource->GetSize( &mailSize);
  if (mailSize == 0) {
    ReportSuccess( name, 0, &success);
    SetLogs( success, error, pErrorLog, pSuccessLog);
    return( NS_OK);
  }
  
  nsIFileSpec	*	inFile;
  if (NS_FAILED( pSource->GetFileSpec( &inFile))) {
    ReportError( OEIMPORT_MAILBOX_BADSOURCEFILE, name, &error);
    SetLogs( success, error, pErrorLog, pSuccessLog);		
    return( NS_ERROR_FAILURE);
  }
  
  nsXPIDLCString pPath; 
  inFile->GetNativePath(getter_Copies(pPath));
  IMPORT_LOG1( "Importing Outlook Express mailbox: %s\n", pPath.get());
  
  m_bytesDone = 0;
  PRUint32	msgCount = 0;
  nsresult rv;
  if (nsOE5File::IsLocalMailFile( inFile)) {
    IMPORT_LOG1( "Importing OE5 mailbox: %S!\n", name.get());
    rv = nsOE5File::ImportMailbox( &m_bytesDone, &abort, name, inFile, pDestination, &msgCount);
  }
  else {
    if (CImportMailbox::ImportMailbox( &m_bytesDone, &abort, name, inFile, pDestination, &msgCount)) {
    		rv = NS_OK;
    }
    else {
    		rv = NS_ERROR_FAILURE;
    }
  }
  
  inFile->Release();
  
  if (NS_SUCCEEDED( rv)) {
    ReportSuccess( name, msgCount, &success);
  }
  else {
    ReportError( OEIMPORT_MAILBOX_CONVERTERROR, name, &error);
  }
  
  SetLogs( success, error, pErrorLog, pSuccessLog);
  
  return( rv);
}


NS_IMETHODIMP ImportOEMailImpl::GetImportProgress( PRUint32 *pDoneSoFar) 
{ 
    NS_PRECONDITION(pDoneSoFar != nsnull, "null ptr");
    if (! pDoneSoFar)
        return NS_ERROR_NULL_POINTER;
	
	*pDoneSoFar = m_bytesDone;
	return( NS_OK);
}



nsresult ImportOEAddressImpl::Create(nsIImportAddressBooks** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new ImportOEAddressImpl();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

ImportOEAddressImpl::ImportOEAddressImpl()
{
	m_pWab = nsnull;
}


ImportOEAddressImpl::~ImportOEAddressImpl()
{
	if (m_pWab)
		delete m_pWab;
}



NS_IMPL_THREADSAFE_ISUPPORTS1(ImportOEAddressImpl, nsIImportAddressBooks)

	
NS_IMETHODIMP ImportOEAddressImpl::GetAutoFind(PRUnichar **description, PRBool *_retval)
{
    NS_PRECONDITION(description != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! description || !_retval)
        return NS_ERROR_NULL_POINTER;
    
    *_retval = PR_TRUE;
    nsString str; str.Append(nsOEStringBundle::GetStringByID(OEIMPORT_AUTOFIND));
    *description = ToNewUnicode(str);
    
    return( NS_OK);
}

	
	
NS_IMETHODIMP ImportOEAddressImpl::FindAddressBooks(nsIFileSpec *location, nsISupportsArray **_retval)
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
	nsString					str; str.Append(nsOEStringBundle::GetStringByID( OEIMPORT_DEFAULT_NAME));
	
	if (m_pWab->Loaded()) {
		// create a new nsIImportABDescriptor and add it to the array
		nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
		if (NS_SUCCEEDED( rv)) {
			rv = impSvc->CreateNewABDescriptor( &pID);
			if (NS_SUCCEEDED( rv)) {
				pID->SetIdentifier( 0x4F453334);
				pID->SetRef( 1);
				pID->SetSize( 100);
				pID->SetPreferredName(str);
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

	
	
NS_IMETHODIMP ImportOEAddressImpl::ImportAddressBook(	nsIImportABDescriptor *source, 
													nsIAddrDatabase *	destination, 
													nsIImportFieldMap *	fieldMap, 
													PRBool isAddrLocHome, 
													PRUnichar **		errorLog,
													PRUnichar **		successLog,
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

  nsCOMPtr<nsIStringBundle>	bundle( dont_AddRef( nsOEStringBundle::GetStringBundleProxy()));
  nsString	success;
  nsString	error;
  if (!source || !destination || !fatalError)
  {
    nsOEStringBundle::GetStringByID( OEIMPORT_ADDRESS_BADPARAM, error, bundle);
    if (fatalError)
      *fatalError = PR_TRUE;
    ImportOEMailImpl::SetLogs( success, error, errorLog, successLog);
    return NS_ERROR_NULL_POINTER;
  }

  m_doneSoFar = 0;
  nsOEAddressIterator * pIter = new nsOEAddressIterator( m_pWab, destination);
  HRESULT hr = m_pWab->IterateWABContents( pIter, &m_doneSoFar);
  delete pIter;

  nsString	name;
  if (SUCCEEDED(hr))
  {
    if (NS_SUCCEEDED(source->GetPreferredName(name)))
    {
      ReportSuccess( name, &success);
    }
  }
  else
    ImportOEMailImpl::ReportError( OEIMPORT_ADDRESS_CONVERTERROR, name, &error);

  ImportOEMailImpl::SetLogs( success, error, errorLog, successLog);
    
  nsresult rv = destination->Commit(nsAddrDBCommitType::kLargeCommit);
	return rv;
}

	
NS_IMETHODIMP ImportOEAddressImpl::GetImportProgress(PRUint32 *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;
	
	*_retval = (PRUint32) m_doneSoFar;
	return( NS_OK);
}

void ImportOEAddressImpl::ReportSuccess( nsString& name, nsString *pStream)
{
  if (!pStream)
    return;
  // load the success string
  nsIStringBundle *pBundle = nsOEStringBundle::GetStringBundleProxy();
  PRUnichar *pFmt = nsOEStringBundle::GetStringByID( OEIMPORT_ADDRESS_SUCCESS, pBundle);
  PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get());
  pStream->Append( pText);
  nsTextFormatter::smprintf_free( pText);
  nsOEStringBundle::FreeString( pFmt);
  ImportOEMailImpl::AddLinebreak( pStream);
  NS_IF_RELEASE( pBundle);
}
