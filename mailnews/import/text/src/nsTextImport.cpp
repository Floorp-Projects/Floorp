/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Mark Banner <mark@standard8.demon.co.uk>
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

  Text import addressbook interfaces

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
#include "nsIComponentManager.h"
#include "nsTextImport.h"
#include "nsIMemory.h"
#include "nsIImportGeneric.h"
#include "nsIImportAddressBooks.h"
#include "nsIImportABDescriptor.h"
#include "nsIImportFieldMap.h"
#include "nsIOutputStream.h"
#include "nsIAddrDatabase.h"
#include "nsIAbLDIFService.h"
#include "nsAbBaseCID.h"
#include "nsTextFormatter.h"
#include "nsTextStringBundle.h"
#include "nsIStringBundle.h"
#include "nsTextAddress.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsXPIDLString.h"
#include "nsProxiedService.h"
#include "TextDebugLog.h"
#include "nsIFileSpec.h"
#include "nsNetUtil.h"

static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);
PRLogModuleInfo* TEXTIMPORTLOGMODULE;

class ImportAddressImpl : public nsIImportAddressBooks
{
public:
    ImportAddressImpl();

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
	
	/* nsISupports InitFieldMap(nsIImportFieldMap fieldMap); */
	NS_IMETHOD InitFieldMap(nsIImportFieldMap *fieldMap);
	
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
 
	NS_IMETHOD GetSampleData( PRInt32 index, PRBool *pFound, PRUnichar **pStr);

	NS_IMETHOD SetSampleLocation( nsIFileSpec *);

private:
	void	ClearSampleFile( void);
	void	SaveFieldMap( nsIImportFieldMap *pMap);

	static void	ReportSuccess( nsString& name, nsString *pStream);
	static void SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess);
	static void ReportError( PRInt32 errorNum, nsString& name, nsString *pStream);
	static void	SanitizeSampleData( nsCString& val);

private:
  nsTextAddress m_text;
  PRBool m_haveDelim;
  nsCOMPtr<nsILocalFile> m_fileLoc;
  char m_delim;
  PRUint32 m_bytesImported;
};


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


nsTextImport::nsTextImport()
{
  // Init logging module.
  if (!TEXTIMPORTLOGMODULE)
    TEXTIMPORTLOGMODULE = PR_NewLogModule("IMPORT");
	IMPORT_LOG0( "nsTextImport Module Created\n");

	nsTextStringBundle::GetStringBundle();
}


nsTextImport::~nsTextImport()
{

	IMPORT_LOG0( "nsTextImport Module Deleted\n");

}



NS_IMPL_ISUPPORTS1(nsTextImport, nsIImportModule)


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
	m_haveDelim = PR_FALSE;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(ImportAddressImpl, nsIImportAddressBooks)

	
NS_IMETHODIMP ImportAddressImpl::GetAutoFind(PRUnichar **addrDescription, PRBool *_retval)
{
    NS_PRECONDITION(addrDescription != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! addrDescription || !_retval)
        return NS_ERROR_NULL_POINTER;
    
    nsString	str;
    *_retval = PR_FALSE;
    nsTextStringBundle::GetStringByID( TEXTIMPORT_ADDRESS_NAME, str);
    *addrDescription = ToNewUnicode(str);
    
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
	
	ClearSampleFile();

	*ppArray = nsnull;
	PRBool exists = PR_FALSE;
	nsresult rv = pLoc->Exists( &exists);
	if (NS_FAILED( rv) || !exists)
		return( NS_ERROR_FAILURE);
	
	PRBool isFile = PR_FALSE;
	rv = pLoc->IsFile( &isFile);
	if (NS_FAILED( rv) || !isFile)
		return( NS_ERROR_FAILURE);

  // XXX this should just be passed in as an nsIFile.
  nsFileSpec spec;
  rv = pLoc->GetFileSpec(&spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> fileLocation;
  rv = NS_FileSpecToIFile(&spec, getter_AddRefs(fileLocation));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = m_text.DetermineDelim(fileLocation);
	
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error determining delimitter\n");
		return( rv);
	}
	m_haveDelim = PR_TRUE;
	m_delim = m_text.GetDelim();
	
	m_fileLoc = fileLocation;

	/* Build an address book descriptor based on the file passed in! */
	nsCOMPtr<nsISupportsArray>	array;
	rv = NS_NewISupportsArray( getter_AddRefs( array));
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray\n");
		return( rv);
	}
		
	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed to obtain the import service\n");
		return( rv);
	}
	
	nsXPIDLCString pName;
	rv = pLoc->GetLeafName(getter_Copies(pName));
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Failed getting leaf name of file\n");
		return( rv);
	}

	// for get unicode leafname.  If it uses nsILocalFile interface,
	// these codes do not need due to nsILocalFile->GetUnicodeLeafName()
	nsString	name;
	rv = impSvc->SystemStringToUnicode((const char*) pName, name);
	if (NS_FAILED(rv))
		name.AssignWithConversion((const char*) pName);

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
    desc->SetPreferredName(name);
		desc->SetSize( sz);
    desc->SetAbFile(m_fileLoc);
		rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
		array->AppendElement( pInterface);
		pInterface->Release();
	}
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error creating address book descriptor for text import\n");
	}		
	else {
		rv = array->QueryInterface( NS_GET_IID(nsISupportsArray), (void **) ppArray);
	}

	return( rv);
}

	

void ImportAddressImpl::ReportSuccess( nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the success string
	nsIStringBundle *pBundle = nsTextStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsTextStringBundle::GetStringByID( TEXTIMPORT_ADDRESS_SUCCESS, pBundle);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get());
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsTextStringBundle::FreeString( pFmt);
	pStream->Append( PRUnichar(nsCRT::LF));
	NS_IF_RELEASE( pBundle);
}

void ImportAddressImpl::ReportError( PRInt32 errorNum, nsString& name, nsString *pStream)
{
	if (!pStream)
		return;
	// load the error string
	nsIStringBundle *pBundle = nsTextStringBundle::GetStringBundleProxy();
	PRUnichar *pFmt = nsTextStringBundle::GetStringByID( errorNum, pBundle);
	PRUnichar *pText = nsTextFormatter::smprintf( pFmt, name.get());
	pStream->Append( pText);
	nsTextFormatter::smprintf_free( pText);
	nsTextStringBundle::FreeString( pFmt);
	pStream->Append( PRUnichar(nsCRT::LF));
	NS_IF_RELEASE( pBundle);
}

void ImportAddressImpl::SetLogs( nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess)
{
	if (pError)
		*pError = ToNewUnicode(error);
	if (pSuccess)
		*pSuccess = ToNewUnicode(success);
}


NS_IMETHODIMP ImportAddressImpl::ImportAddressBook(	nsIImportABDescriptor *pSource, 
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
	
	nsCOMPtr<nsIStringBundle>	bundle( dont_AddRef( nsTextStringBundle::GetStringBundleProxy()));
	m_bytesImported = 0;

	nsString	success;
	nsString	error;
    if (!pSource || !pDestination || !fatalError) {
		IMPORT_LOG0( "*** Bad param passed to text address import\n");
		nsTextStringBundle::GetStringByID( TEXTIMPORT_ADDRESS_BADPARAM, error, bundle);
		if (fatalError)
			*fatalError = PR_TRUE;
		SetLogs( success, error, pErrorLog, pSuccessLog);
	    return( NS_ERROR_NULL_POINTER);
	}
   
	ClearSampleFile();

  PRBool addrAbort = PR_FALSE;
  nsString name;
  pSource->GetPreferredName(name);
    
	PRUint32 addressSize = 0;
	pSource->GetSize( &addressSize);
	if (addressSize == 0) {
		IMPORT_LOG0( "Address book size is 0, skipping import.\n");
		ReportSuccess( name, &success);
		SetLogs( success, error, pErrorLog, pSuccessLog);
		return NS_OK;
	}

  nsCOMPtr<nsIFile> inFile;
  if (NS_FAILED(pSource->GetAbFile(getter_AddRefs(inFile)))) {
		ReportError( TEXTIMPORT_ADDRESS_BADSOURCEFILE, name, &error);
		SetLogs( success, error, pErrorLog, pSuccessLog);		
    return NS_ERROR_FAILURE;
  }

	PRBool	isLDIF = PR_FALSE;
  nsresult rv;
    nsCOMPtr<nsIAbLDIFService> ldifService = do_GetService(NS_ABLDIFSERVICE_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv)) {
      rv = ldifService->IsLDIFFile(inFile, &isLDIF);
      if (NS_FAILED(rv)) {
        IMPORT_LOG0( "*** Error reading address file\n");
      }
    }

	if (NS_FAILED( rv)) {
		ReportError( TEXTIMPORT_ADDRESS_CONVERTERROR, name, &error);
		SetLogs( success, error, pErrorLog, pSuccessLog);
		return( rv);
	}
	
    if (isLDIF) {
        if (ldifService)
          rv = ldifService->ImportLDIFFile(pDestination, inFile, PR_FALSE, &m_bytesImported);
        else
          return NS_ERROR_FAILURE;
	}
	else {	
    rv = m_text.ImportAddresses( &addrAbort, name.get(), inFile, pDestination, fieldMap, error, &m_bytesImported);
		SaveFieldMap( fieldMap);
	}

	if (NS_SUCCEEDED( rv) && error.IsEmpty()) {
		ReportSuccess( name, &success);
	}
	else {
		ReportError( TEXTIMPORT_ADDRESS_CONVERTERROR, name, &error);
	}

	SetLogs( success, error, pErrorLog, pSuccessLog);

	IMPORT_LOG0( "*** Text address import done\n");
  return rv;
}


NS_IMETHODIMP ImportAddressImpl::GetImportProgress(PRUint32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
	
	*_retval = m_bytesImported;
	return NS_OK;
}


NS_IMETHODIMP ImportAddressImpl::GetNeedsFieldMap(nsIFileSpec *location, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG_POINTER(location);

	*_retval = PR_TRUE;
	PRBool	exists = PR_FALSE;
	PRBool	isFile = PR_FALSE;

	nsresult rv = location->Exists( &exists);
	rv = location->IsFile( &isFile);
	
	if (!exists || !isFile)
		return( NS_ERROR_FAILURE);

  nsXPIDLCString pPath; 
  location->GetNativePath(getter_Copies(pPath));

  nsCOMPtr<nsILocalFile> inFile;
  rv = NS_NewNativeLocalFile(pPath, PR_TRUE, getter_AddRefs(inFile));
  NS_ENSURE_SUCCESS(rv, rv);

	PRBool	isLDIF = PR_FALSE;
    nsCOMPtr<nsIAbLDIFService> ldifService = do_GetService(NS_ABLDIFSERVICE_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv))
      rv = ldifService->IsLDIFFile(inFile, &isLDIF);

	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error determining if file is of type LDIF\n");
		return( rv);
	}
	
	if (isLDIF)
		*_retval = PR_FALSE;
	
	return( NS_OK);
}

void ImportAddressImpl::SanitizeSampleData( nsCString& val)
{
	// remove any line-feeds...
	val.ReplaceSubstring( "\x0D\x0A", ", ");
	val.ReplaceChar( 13, ',');
	val.ReplaceChar( 10, ',');
}

NS_IMETHODIMP ImportAddressImpl::GetSampleData( PRInt32 index, PRBool *pFound, PRUnichar **pStr)
{
    NS_PRECONDITION(pFound != nsnull, "null ptr");
    NS_PRECONDITION(pStr != nsnull, "null ptr");
	if (!pFound || !pStr)
		return( NS_ERROR_NULL_POINTER);
	
	if (!m_fileLoc) {
		IMPORT_LOG0( "*** Error, called GetSampleData before SetSampleLocation\n");
		return( NS_ERROR_FAILURE);
	}

	nsresult	rv;
	*pStr = nsnull;
	PRUnichar	term = 0;

	if (!m_haveDelim) {
    rv = m_text.DetermineDelim(m_fileLoc);
    NS_ENSURE_SUCCESS(rv, rv);
		m_haveDelim = PR_TRUE;
		m_delim = m_text.GetDelim();
	}
	
  PRBool fileExists;
  rv = m_fileLoc->Exists(&fileExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!fileExists) {
    *pFound = PR_FALSE;
    *pStr = nsCRT::strdup(&term);
    return NS_OK;
  }
	
  nsXPIDLCString line;
	
	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));

	rv = nsTextAddress::ReadRecordNumber(m_fileLoc, line, m_delim, index);
	if (NS_SUCCEEDED( rv)) {
		nsString	str;
		nsCString	field;
		nsString	uField;
		PRInt32		fNum = 0;
		while (nsTextAddress::GetField(line.get(), line.Length(), fNum, field, m_delim)) {
			if (fNum)
				str.AppendLiteral("\n");
			SanitizeSampleData( field);
			if (impSvc)
				impSvc->SystemStringToUnicode( field.get(), uField);
			else
				uField.AssignWithConversion( field.get());

			str.Append( uField);
			fNum++;
			field.Truncate();
		}

		*pStr = nsCRT::strdup( str.get());
		*pFound = PR_TRUE;

		/* IMPORT_LOG1( "Sample data: %S\n", str.get()); */
	}
	else {
		*pFound = PR_FALSE;
		*pStr = nsCRT::strdup( &term);
	}

	return NS_OK;
}

NS_IMETHODIMP ImportAddressImpl::SetSampleLocation(nsIFileSpec *pLocation)
{
  NS_ENSURE_ARG_POINTER(pLocation);

  // XXX this should just be passed in as an nsIFile.
  nsFileSpec spec;
  nsresult rv = pLocation->GetFileSpec(&spec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_FileSpecToIFile(&spec, getter_AddRefs(m_fileLoc));
  NS_ENSURE_SUCCESS(rv, rv);

	m_haveDelim = PR_FALSE;

	return NS_OK;
}

void ImportAddressImpl::ClearSampleFile( void)
{
  m_fileLoc = nsnull;
  m_haveDelim = PR_FALSE;
}

NS_IMETHODIMP ImportAddressImpl::InitFieldMap(nsIImportFieldMap *fieldMap)
{
	// Let's remember the last one the user used!
	// This should be normal for someone importing multiple times, it's usually
	// from the same file format.
	
	nsresult rv;
	nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
	if (NS_SUCCEEDED( rv)) {
		nsXPIDLCString	prefStr;
		rv = prefs->GetCharPref( "mailnews.import.text.fieldmap", getter_Copies(prefStr));
		if (NS_SUCCEEDED( rv)) {
			const char *pStr = (const char *)prefStr;
			if (pStr) {
				fieldMap->SetFieldMapSize( 0);
				long		fNum;
				PRBool		active;
				long		fIndex = 0;
				while (*pStr) {
					while (*pStr && (*pStr != '+') && (*pStr != '-'))
						pStr++;
					if (*pStr == '+')
						active = PR_TRUE;
					else if (*pStr == '-')
						active = PR_FALSE;
					else
						break;
					fNum = 0;
					while (*pStr && ((*pStr < '0') || (*pStr > '9')))
						pStr++;
					if (!(*pStr))
						break;
					while (*pStr && (*pStr >= '0') && (*pStr <= '9')) {
						fNum *= 10;
						fNum += (*pStr - '0');
						pStr++;
					}
					while (*pStr && (*pStr != ','))
						pStr++;
					if (*pStr == ',')
						pStr++;
					fieldMap->SetFieldMap( -1, fNum);
					fieldMap->SetFieldActive( fIndex, active);
					fIndex++;
				}
				if (!fIndex) {
					int num;
					fieldMap->GetNumMozFields( &num);
					fieldMap->DefaultFieldMap( num);
				}
			}
		}

        // Now also get the last used skip first record value.
        PRBool skipFirstRecord = PR_FALSE;
        rv = prefs->GetBoolPref("mailnews.import.text.skipfirstrecord", &skipFirstRecord);
        if (NS_SUCCEEDED(rv))
        {
          fieldMap->SetSkipFirstRecord(skipFirstRecord);
        }
	}

	return( NS_OK);
}


void ImportAddressImpl::SaveFieldMap( nsIImportFieldMap *pMap)
{
	if (!pMap)
		return;

	int			size;
	int			index;
	PRBool		active;
	nsCString	str;

	pMap->GetMapSize( &size);
	for (long i = 0; i < size; i++) {
		index = i;
		active = PR_FALSE;
		pMap->GetFieldMap( i, &index);
		pMap->GetFieldActive( i, &active);
		if (active)
			str.Append( '+');
		else
			str.Append( '-');

		str.AppendInt( index);
		str.Append( ',');
	}

	PRBool	done = PR_FALSE;
	nsresult rv;

	nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));

	if (NS_SUCCEEDED( rv)) {
		nsXPIDLCString	prefStr;
		rv = prefs->GetCharPref( "mailnews.import.text.fieldmap", getter_Copies(prefStr));
		if (NS_SUCCEEDED( rv)) {
			if (str.Equals(prefStr))
				done = PR_TRUE;
		}
		if (!done) {
			rv = prefs->SetCharPref( "mailnews.import.text.fieldmap", str.get());
		}
	}

    // Now also save last used skip first record value.
    PRBool skipFirstRecord = PR_FALSE;
    rv = pMap->GetSkipFirstRecord(&skipFirstRecord);
    if (NS_SUCCEEDED(rv))
    {
      prefs->SetBoolPref("mailnews.import.text.skipfirstrecord", skipFirstRecord);
    }
}
