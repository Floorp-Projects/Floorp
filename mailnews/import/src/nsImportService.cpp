/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"

#include "nsCRT.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsIEnumerator.h"
#include "nsIImportModule.h"
#include "nsIImportService.h"
#include "nsImportMailboxDescriptor.h"
#include "nsImportABDescriptor.h"
#include "nsIImportGeneric.h"
#include "nsImportFieldMap.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "plstr.h"
#include "prmem.h"
#include "ImportDebug.h"
#include "nsImportService.h"
#include "nsImportStringBundle.h"

PRLogModuleInfo *IMPORTLOGMODULE = nsnull;

static nsIImportService *	gImportService = nsnull;
static const char *	kWhitespace = "\b\t\r\n ";


////////////////////////////////////////////////////////////////////////


nsImportService::nsImportService() : m_pModules( nsnull)
{
	// Init logging module.
  if (!IMPORTLOGMODULE)
    IMPORTLOGMODULE = PR_NewLogModule("IMPORT");
  IMPORT_LOG0( "* nsImport Service Created\n");

	m_didDiscovery = PR_FALSE;
	m_pDecoder = nsnull;
	m_pEncoder = nsnull;

  nsresult rv = nsImportStringBundle::GetStringBundle(IMPORT_MSGS_URL, getter_AddRefs(m_stringBundle));
  if (NS_FAILED(rv))
    IMPORT_LOG0("Failed to get string bundle for Importing Mail");
}


nsImportService::~nsImportService()
{
	NS_IF_RELEASE(m_pDecoder);
	NS_IF_RELEASE(m_pEncoder);

	gImportService = nsnull;

    if (m_pModules != nsnull)
        delete m_pModules;

	IMPORT_LOG0( "* nsImport Service Deleted\n");
}



NS_IMPL_THREADSAFE_ISUPPORTS1(nsImportService, nsIImportService)


NS_IMETHODIMP nsImportService::DiscoverModules( void)
{
	m_didDiscovery = PR_FALSE;
	return( DoDiscover());
}

NS_IMETHODIMP nsImportService::CreateNewFieldMap( nsIImportFieldMap **_retval)
{
  return nsImportFieldMap::Create(m_stringBundle, nsnull, NS_GET_IID(nsIImportFieldMap), (void**)_retval);
}

NS_IMETHODIMP nsImportService::CreateNewMailboxDescriptor( nsIImportMailboxDescriptor **_retval)
{
  return nsImportMailboxDescriptor::Create(nsnull, NS_GET_IID(nsIImportMailboxDescriptor), (void**)_retval);
}

NS_IMETHODIMP nsImportService::CreateNewABDescriptor(nsIImportABDescriptor **_retval)
{
  return nsImportABDescriptor::Create(nsnull, NS_GET_IID(nsIImportABDescriptor), (void**)_retval);
}

extern nsresult NS_NewGenericMail(nsIImportGeneric** aImportGeneric);

NS_IMETHODIMP nsImportService::CreateNewGenericMail(nsIImportGeneric **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

	return( NS_NewGenericMail( _retval));
}

extern nsresult NS_NewGenericAddressBooks(nsIImportGeneric** aImportGeneric);

NS_IMETHODIMP nsImportService::CreateNewGenericAddressBooks(nsIImportGeneric **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

	return( NS_NewGenericAddressBooks( _retval));
}


NS_IMETHODIMP nsImportService::GetModuleCount( const char *filter, PRInt32 *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (! _retval)
        return NS_ERROR_NULL_POINTER;

	DoDiscover();
	
	if (m_pModules != nsnull) {
		ImportModuleDesc *	pDesc;
		PRInt32	count = 0;
		for (PRInt32 i = 0; i < m_pModules->GetCount(); i++) {
			pDesc = m_pModules->GetModuleDesc( i);
			if (pDesc->SupportsThings( filter))
				count++;
		}
		*_retval = count;
	}
	else
		*_retval = 0;
		
	return( NS_OK);
}
	
NS_IMETHODIMP nsImportService::GetModuleWithCID( const nsCID& cid, nsIImportModule **ppModule)
{
    NS_PRECONDITION(ppModule != nsnull, "null ptr");
    if (! ppModule)
        return NS_ERROR_NULL_POINTER;
	
	*ppModule = nsnull;
	nsresult rv = DoDiscover();
	if (NS_FAILED( rv)) return( rv);
	if (m_pModules == nsnull) return( NS_ERROR_FAILURE);
	PRInt32	cnt = m_pModules->GetCount();
	ImportModuleDesc *pDesc;
	for (PRInt32 i = 0; i < cnt; i++) {
		pDesc = m_pModules->GetModuleDesc( i);
		if (!pDesc) return( NS_ERROR_FAILURE);
		if (pDesc->GetCID().Equals( cid)) {
			*ppModule = pDesc->GetModule();
			
			IMPORT_LOG0( "* nsImportService::GetSpecificModule - attempted to load module\n");
			
			if (*ppModule == nsnull)
				return( NS_ERROR_FAILURE);
			else
				return( NS_OK);
		}
	}
	
	IMPORT_LOG0( "* nsImportService::GetSpecificModule - module not found\n");
	
	return( NS_ERROR_NOT_AVAILABLE);
}

NS_IMETHODIMP nsImportService::GetModuleInfo( const char *filter, PRInt32 index, PRUnichar **name, PRUnichar **moduleDescription)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    NS_PRECONDITION(moduleDescription != nsnull, "null ptr");
    if (!name || !moduleDescription)
        return NS_ERROR_NULL_POINTER;
    
	*name = nsnull;
	*moduleDescription = nsnull;

    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *	pDesc;
	PRInt32	count = 0;
	for (PRInt32 i = 0; i < m_pModules->GetCount(); i++) {
		pDesc = m_pModules->GetModuleDesc( i);
		if (pDesc->SupportsThings( filter)) {
			if (count == index) {
				*name = nsCRT::strdup( pDesc->GetName());
				*moduleDescription = nsCRT::strdup( pDesc->GetDescription());
				return( NS_OK);
			}
			else
				count++;
		}
	}

	return( NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsImportService::GetModuleName(const char *filter, PRInt32 index, PRUnichar **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
	
	*_retval = nsnull;
	    
    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *	pDesc;
	PRInt32	count = 0;
	for (PRInt32 i = 0; i < m_pModules->GetCount(); i++) {
		pDesc = m_pModules->GetModuleDesc( i);
		if (pDesc->SupportsThings( filter)) {
			if (count == index) {
				*_retval = nsCRT::strdup( pDesc->GetName());
				return( NS_OK);
			}
			else
				count++;
		}
	}
	
	return( NS_ERROR_FAILURE);
}


NS_IMETHODIMP nsImportService::GetModuleDescription(const char *filter, PRInt32 index, PRUnichar **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
    
	*_retval = nsnull;

    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *	pDesc;
	PRInt32	count = 0;
	for (PRInt32 i = 0; i < m_pModules->GetCount(); i++) {
		pDesc = m_pModules->GetModuleDesc( i);
		if (pDesc->SupportsThings( filter)) {
			if (count == index) {
				*_retval = nsCRT::strdup( pDesc->GetDescription());
				return( NS_OK);
			}
			else
				count++;
		}
	}
	
	return( NS_ERROR_FAILURE);
}



NS_IMETHODIMP nsImportService::GetModule( const char *filter, PRInt32 index, nsIImportModule **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
	*_retval = nsnull;

    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
		
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *	pDesc;
	PRInt32	count = 0;
	for (PRInt32 i = 0; i < m_pModules->GetCount(); i++) {
		pDesc = m_pModules->GetModuleDesc( i);
		if (pDesc->SupportsThings( filter)) {
			if (count == index) {
				*_retval = pDesc->GetModule();
				break;
			}
			else
				count++;
		}
	}
	if (! (*_retval))
		return( NS_ERROR_FAILURE);

	return( NS_OK);
}


nsresult nsImportService::DoDiscover( void)
{	
	if (m_didDiscovery)
		return( NS_OK);
	
	if (m_pModules != nsnull)
		m_pModules->ClearList();
		    
    nsresult rv;
	
	nsCOMPtr<nsICategoryManager> catMan = do_GetService( NS_CATEGORYMANAGER_CONTRACTID, &rv);
	if (NS_FAILED( rv)) return( rv);
    
	nsCOMPtr<nsISimpleEnumerator> e;
	rv = catMan->EnumerateCategory( "mailnewsimport", getter_AddRefs( e));
	if (NS_FAILED( rv)) return( rv);
	nsCOMPtr<nsISupportsCString> contractid;
	rv = e->GetNext( getter_AddRefs( contractid));
	while (NS_SUCCEEDED( rv) && contractid) {
		nsXPIDLCString	contractIdStr;
		contractid->ToString( getter_Copies( contractIdStr));
		nsXPIDLCString	supportsStr;
		rv = catMan->GetCategoryEntry( "mailnewsimport", contractIdStr, getter_Copies( supportsStr));
		if (NS_SUCCEEDED( rv)) {
			LoadModuleInfo( contractIdStr, supportsStr);
		}
		rv = e->GetNext( getter_AddRefs( contractid));
	}

	m_didDiscovery = PR_TRUE;
	
    return NS_OK;
}

nsresult nsImportService::LoadModuleInfo( const char *pClsId, const char *pSupports)
{
	if (!pClsId || !pSupports)
		return( NS_OK);

	if (m_pModules == nsnull)
		m_pModules = new nsImportModuleList();
		
	// load the component and get all of the info we need from it....
	// then call AddModule
	nsresult	rv;

	nsCID				clsId;
	clsId.Parse( pClsId);
	nsIImportModule *	module;
	rv = CallCreateInstance( clsId, &module);
	if (NS_FAILED(rv)) return rv;
	
	nsString	theTitle;	
	nsString	theDescription;
	PRUnichar *	pName;
	rv = module->GetName( &pName);
	if (NS_SUCCEEDED( rv)) {
		theTitle = pName;
                nsMemory::Free(pName);
	}
	else
		theTitle.AssignLiteral("Unknown");
		
	rv = module->GetDescription( &pName);
	if (NS_SUCCEEDED( rv)) {
		theDescription = pName;
                nsMemory::Free(pName);
	}
	else
		theDescription.AssignLiteral("Unknown description");
	
	// call the module to get the info we need
	m_pModules->AddModule( clsId, pSupports, theTitle.get(), theDescription.get());
	
	module->Release();
	
	return NS_OK;
}


nsIImportModule *ImportModuleDesc::GetModule( PRBool keepLoaded)
{
	if (m_pModule) {
		m_pModule->AddRef();
		return( m_pModule);
	}
	
	nsresult	rv;
	rv = CallCreateInstance( m_cid, &m_pModule);
	if (NS_FAILED(rv)) {
		m_pModule = nsnull;
		return nsnull;
	}
	
	if (keepLoaded) {
		m_pModule->AddRef();
		return( m_pModule);
	}
	else {
		nsIImportModule *pModule = m_pModule;
		m_pModule = nsnull;
		return( pModule);
	}
}

void ImportModuleDesc::ReleaseModule( void)
{
	if (m_pModule) {
		m_pModule->Release();
		m_pModule = nsnull;
	}
}

PRBool ImportModuleDesc::SupportsThings( const char *pThings)
{
	if (!pThings)
		return( PR_TRUE);
	if (!(*pThings))
		return( PR_TRUE);

	nsCString	thing(pThings);
	nsCString	item;
	PRInt32		idx;
	
	while ((idx = thing.FindChar( ',')) != -1) {
		thing.Left( item, idx);
		item.Trim( kWhitespace);
		ToLowerCase(item);
		if (item.Length() && (m_supports.Find( item) == -1))
			return( PR_FALSE);
		thing.Right( item, thing.Length() - idx - 1);
		thing = item;
	}
	thing.Trim( kWhitespace);
	ToLowerCase(thing);
	if (thing.Length() && (m_supports.Find( thing) == -1))
		return( PR_FALSE);
	
	return( PR_TRUE);
}

void nsImportModuleList::ClearList( void)
{
	if (m_pList != nsnull) {
		for (int i = 0; i < m_count; i++) {
			if (m_pList[i] != nsnull)
				delete m_pList[i];	
			m_pList[i] = nsnull;
		}
		m_count = 0;
		delete [] m_pList;
		m_pList = nsnull;
		m_alloc = 0;
	}
	
}

void nsImportModuleList::AddModule( const nsCID& cid, const char *pSupports, const PRUnichar *pName, const PRUnichar *pDesc)
{
	if (m_pList == nsnull) {
		m_alloc = 10;
		m_pList = new ImportModuleDesc *[m_alloc];
		m_count = 0;
		memset( m_pList, 0, sizeof( ImportModuleDesc *) * m_alloc);
	}
	
	if (m_count == m_alloc) {
		ImportModuleDesc **pList = new ImportModuleDesc *[m_alloc + 10];
		memset( &(pList[m_alloc]), 0, sizeof( ImportModuleDesc *) * 10);
		memcpy( pList, m_pList, sizeof( ImportModuleDesc *) * m_alloc);
    for(int i = 0; i < m_count; i++)
      delete m_pList[i];
    delete [] m_pList;
		m_pList = pList;
		m_alloc += 10;
	}
	
	m_pList[m_count] = new ImportModuleDesc();
	m_pList[m_count]->SetCID( cid);
	m_pList[m_count]->SetSupports( pSupports);
	m_pList[m_count]->SetName( pName);
	m_pList[m_count]->SetDescription( pDesc);
	
	m_count++;
#ifdef IMPORT_DEBUG
	nsCString 	name; name.AssignWithConversion( pName);
	nsCString	desc; desc.AssignWithConversion( pDesc);
	IMPORT_LOG3( "* nsImportService registered import module: %s, %s, %s\n", name.get(), desc.get(), pSupports);
#endif
}
