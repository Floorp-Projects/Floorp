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


#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsIEnumerator.h"
#include "nsIImportModule.h"
#include "nsIImportService.h"
#include "nsImportMailboxDescriptor.h"
#include "nsImportABDescriptor.h"
#include "nsIImportGeneric.h"
#include "plstr.h"
#include "prmem.h"
#include "ImportDebug.h"


static NS_DEFINE_CID(kComponentManagerCID, 	NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kRegistryCID,			NS_REGISTRY_CID);
static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_IID(kImportModuleIID,		NS_IIMPORTMODULE_IID);


static nsIImportService *	gImportService = nsnull;


class nsImportModuleList;

class nsImportService : public nsIImportService
{
public:

	nsImportService();
	virtual ~nsImportService();
	
	NS_DECL_ISUPPORTS

	/* void DiscoverModules (); */
	NS_IMETHOD DiscoverModules(void);
	
	/* long GetModuleCount (in string filter); */
	NS_IMETHOD GetModuleCount(const char *filter, PRInt32 *_retval);
	
	/* void GetModuleInfo (in string filter, in long index, out string name, out string description); */
	NS_IMETHOD GetModuleInfo(const char *filter, PRInt32 index, PRUnichar **name, PRUnichar **description);
	
	/* wstring GetModuleName (in string filter, in long index); */
	NS_IMETHOD GetModuleName(const char *filter, PRInt32 index, PRUnichar **_retval);
	
	/* wstring GetModuleDescription (in string filter, in long index); */
	NS_IMETHOD GetModuleDescription(const char *filter, PRInt32 index, PRUnichar **_retval);

	/* nsIImportModule GetModule (in string filter, in long index); */
	NS_IMETHOD GetModule(const char *filter, PRInt32 index, nsIImportModule **_retval);
	
	/* nsIImportModule GetModuleWithCID (in nsCIDRef cid); */
	NS_IMETHOD GetModuleWithCID(const nsCID & cid, nsIImportModule **_retval);
	

	/* nsIImportMailboxDescriptor CreateNewMailboxDescriptor (); */
	NS_IMETHOD CreateNewMailboxDescriptor(nsIImportMailboxDescriptor **_retval);

	/* nsIImportABDescriptor CreateNewABDescriptor (); */
	NS_IMETHOD CreateNewABDescriptor(nsIImportABDescriptor **_retval);

	/* nsIImportGeneric CreateNewGenericMail (); */
	NS_IMETHOD CreateNewGenericMail(nsIImportGeneric **_retval);

	/* nsIImportGeneric CreateNewGenericAddressBooks (); */
	NS_IMETHOD CreateNewGenericAddressBooks(nsIImportGeneric **_retval);


private:
	nsresult	LoadModuleInfo( char *pClsId, const char *pSupports);
	nsresult	DoDiscover( void);
	nsresult	GetImportRegKey( nsIRegistry *reg, nsIRegistry::Key *pKey);
	nsresult	GetImportModulesRegKey( nsIRegistry *reg, nsIRegistry::Key *pKey);

private:
	nsImportModuleList *	m_pModules;
	PRBool					m_didDiscovery;	
};

// extern nsresult NS_NewImportService(nsIImportService** aImportService);

class ImportModuleDesc {
public:
	ImportModuleDesc() { m_pSupports = nsnull; m_pModule = nsnull;}
	~ImportModuleDesc() { if (m_pSupports != nsnull) nsCRT::free( m_pSupports);
						  ReleaseModule();	}
	
	void	SetCID( const nsCID& cid) { m_cid = cid;}
	void	SetName( const PRUnichar *pName) { m_name = pName;}
	void	SetDescription( const PRUnichar *pDesc) { m_description = pDesc;}
	void	SetSupports( const char *pSupports);
	
	nsCID			GetCID( void) { return( m_cid);}
	const PRUnichar *GetName( void) { return( m_name.GetUnicode());}
	const PRUnichar *GetDescription( void) { return( m_description.GetUnicode());}
	const char *	GetSupports( void) { return( m_pSupports);}
	
	nsIImportModule *	GetModule( PRBool keepLoaded = PR_FALSE); // Adds ref
	void				ReleaseModule( void);
	
private:
	nsCID				m_cid;
	nsString			m_name;
	nsString			m_description;
	char *				m_pSupports;
	nsIImportModule *	m_pModule;
};

class nsImportModuleList {
public:
	nsImportModuleList() { m_pList = nsnull; m_alloc = 0; m_count = 0;}
	~nsImportModuleList() { ClearList(); }

	void	AddModule( const nsCID& cid, const char *pSupports, const PRUnichar *pName, const PRUnichar *pDesc);

	void	ClearList( void);
	
	PRInt32	GetCount( void) { return( m_count);}
	
	ImportModuleDesc *	GetModuleDesc( PRInt32 idx) 
		{ if ((idx < 0) || (idx >= m_count)) return( nsnull); else return( m_pList[idx]);}
	
private:
	
private:
	ImportModuleDesc **	m_pList;
	PRInt32				m_alloc;
	PRInt32				m_count;
};

////////////////////////////////////////////////////////////////////////

nsresult NS_NewImportService(nsIImportService** aImportService)
{
    NS_PRECONDITION(aImportService != nsnull, "null ptr");
    if (! aImportService)
        return NS_ERROR_NULL_POINTER;
	
	if (!gImportService) {	
    	gImportService = new nsImportService();
    	if (! gImportService)
        	return NS_ERROR_OUT_OF_MEMORY;
	}       

    NS_ADDREF( gImportService);
    
    *aImportService = gImportService;
    
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


nsImportService::nsImportService() : m_pModules( nsnull)
{
    NS_INIT_REFCNT();

	IMPORT_LOG0( "* nsImport Service Created\n");

	m_didDiscovery = PR_FALSE;
}


nsImportService::~nsImportService()
{
    if (m_pModules != nsnull)
        delete m_pModules;

	IMPORT_LOG0( "* nsImport Service Deleted\n");

}



NS_IMPL_ISUPPORTS(nsImportService, nsIImportService::GetIID());


NS_IMETHODIMP nsImportService::DiscoverModules( void)
{
	m_didDiscovery = PR_FALSE;
	return( DoDiscover());
}

NS_IMETHODIMP nsImportService::CreateNewMailboxDescriptor( nsIImportMailboxDescriptor **_retval)
{
  nsresult rv;
  rv = nsImportMailboxDescriptor::Create( nsnull, nsIImportMailboxDescriptor::GetIID(), (void**)_retval);
  return rv;
}

NS_IMETHODIMP nsImportService::CreateNewABDescriptor(nsIImportABDescriptor **_retval)
{
  nsresult rv;
  rv = nsImportABDescriptor::Create( nsnull, nsIImportABDescriptor::GetIID(), (void**)_retval);
  return rv;
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
	
	if (m_pModules != nsnull)
		*_retval = m_pModules->GetCount();
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

NS_IMETHODIMP nsImportService::GetModuleInfo( const char *filter, PRInt32 index, PRUnichar **name, PRUnichar **description)
{
    NS_PRECONDITION(name != nsnull, "null ptr");
    NS_PRECONDITION(description != nsnull, "null ptr");
    if (!name || !description)
        return NS_ERROR_NULL_POINTER;
    
    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *pDesc;
	pDesc = m_pModules->GetModuleDesc( index);
	if (!pDesc) 
		return( NS_ERROR_FAILURE);	
	
	*name = nsCRT::strdup( pDesc->GetName());
	*description = nsCRT::strdup( pDesc->GetDescription());
	return( NS_OK);
}

NS_IMETHODIMP nsImportService::GetModuleName(const char *filter, PRInt32 index, PRUnichar **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
    
    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *pDesc;
	pDesc = m_pModules->GetModuleDesc( index);
	if (!pDesc) 
		return( NS_ERROR_FAILURE);	
	
	*_retval = nsCRT::strdup( pDesc->GetName());
	return( NS_OK);
}


NS_IMETHODIMP nsImportService::GetModuleDescription(const char *filter, PRInt32 index, PRUnichar **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
    
    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *pDesc;
	pDesc = m_pModules->GetModuleDesc( index);
	if (!pDesc) 
		return( NS_ERROR_FAILURE);	
	
	*_retval = nsCRT::strdup( pDesc->GetDescription());
	return( NS_OK);
}



NS_IMETHODIMP nsImportService::GetModule( const char *filter, PRInt32 index, nsIImportModule **_retval)
{
    DoDiscover();
    if (!m_pModules)
		return( NS_ERROR_FAILURE);
	
	if ((index < 0) || (index >= m_pModules->GetCount()))
		return( NS_ERROR_FAILURE);
	
	ImportModuleDesc *pDesc;
	pDesc = m_pModules->GetModuleDesc( index);
	if (!pDesc) 
		return( NS_ERROR_FAILURE);
	
	*_retval = pDesc->GetModule();
	if (! (*_retval))
		return( NS_ERROR_FAILURE);
	return( NS_OK);
}




nsresult nsImportService::GetImportRegKey( nsIRegistry *reg, nsIRegistry::Key *pKey)
{
	nsIRegistry::Key	nScapeKey;

	nsresult rv = reg->GetSubtree( nsIRegistry::Common, "Netscape", &nScapeKey);
	if (NS_FAILED(rv)) {
		rv = reg->AddSubtree( nsIRegistry::Common, "Netscape", &nScapeKey);
	}
	if (NS_FAILED( rv))
		return( rv);

	rv = reg->GetSubtree( nScapeKey, "Import", pKey);
	if (NS_FAILED( rv)) {
		rv = reg->AddSubtree( nScapeKey, "Import", pKey);
	}
		
	return( rv);
}

nsresult nsImportService::GetImportModulesRegKey( nsIRegistry *reg, nsIRegistry::Key *pKey)
{
	nsIRegistry::Key	iKey;
	nsresult rv = GetImportRegKey( reg, &iKey);
	if (NS_FAILED( rv))
		return( rv);

	rv = reg->GetSubtree( iKey, "Modules", pKey);
	if (NS_FAILED( rv)) {
		rv = reg->AddSubtree( iKey, "Modules", pKey);
	}

	return( rv);
}


nsresult nsImportService::DoDiscover( void)
{	
	if (m_didDiscovery)
		return( NS_OK);
	
	if (m_pModules != nsnull)
		m_pModules->ClearList();
		    
    nsresult rv;
	    
    
   	NS_WITH_SERVICE( nsIRegistry, reg, kRegistryCID, &rv);
   	if (NS_FAILED(rv)) return rv;
   	
   	rv = reg->OpenDefault();
   	if (NS_FAILED(rv)) return( rv);
   		    	
	nsIRegistry::Key	modulesKey;
	rv = GetImportModulesRegKey( reg, &modulesKey);
   	if (NS_FAILED(rv)) return( rv);
    
    // enumerate the modules key
    
    nsIEnumerator *	enumerator;
    rv = reg->EnumerateSubtrees( modulesKey, &enumerator);	
	if (NS_FAILED( rv)) return( rv);


	// Go to beginning...
	char *pNodeName;
	nsIID nodeIID = NS_IREGISTRYNODE_IID;
	rv = enumerator->First();
	while ( NS_SUCCEEDED(rv) && !enumerator->IsDone()) { 
		nsISupports *base;
		rv = enumerator->CurrentItem( &base );		
		if (NS_SUCCEEDED( rv)) {
			// Get specific interface.
			nsIRegistryNode *node;
			rv = base->QueryInterface( nodeIID, (void**)&node);
		    if (NS_SUCCEEDED(rv)) {
				// Get node name.
				pNodeName = nsnull;
				rv = node->GetName( &pNodeName);	
				if (NS_SUCCEEDED( rv) && (pNodeName != nsnull)) {
					nsIRegistry::Key 	key;
					rv = reg->GetSubtree( modulesKey, pNodeName, &key);
					PR_Free( pNodeName);
					if (NS_SUCCEEDED( rv)) {
						// get the info we need for this module
						char *pClsId = nsnull;
						rv = reg->GetString( key, "CLSID", &pClsId);
						if (NS_SUCCEEDED( rv) && (pClsId != nsnull)) {
							char *pSupports = nsnull;
							rv = reg->GetString( key, "Supports", &pSupports);
							if (NS_SUCCEEDED( rv) && (pSupports != nsnull)) {
								LoadModuleInfo( pClsId, pSupports);
								PR_Free( pSupports);
							}
							PR_Free( pClsId);
						}	
					}
				}
				node->Release();
			}
			base->Release();
		}
		if (NS_SUCCEEDED( rv))
			rv = enumerator->Next();
	}
	
	m_didDiscovery = PR_TRUE;
	
    return NS_OK;
}

nsresult nsImportService::LoadModuleInfo( char *pClsId, const char *pSupports)
{
	if (m_pModules == nsnull)
		m_pModules = new nsImportModuleList();
		
	// load the component and get all of the info we need from it....
	// then call AddModule
	nsresult	rv;
   	NS_WITH_SERVICE( nsIComponentManager, compMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;
	
	nsCID				clsId;
	clsId.Parse( pClsId);
	nsIImportModule *	module;
	rv = compMgr->CreateInstance( clsId, nsnull, kImportModuleIID, (void **) &module);
	if (NS_FAILED(rv)) return rv;
	
	nsString	title;	
	nsString	description;
	PRUnichar *	pName;
	rv = module->GetName( &pName);
	if (NS_SUCCEEDED( rv)) {
		title = pName;
		delete [] pName;
	}
	else
		title = "Unknown";
		
	rv = module->GetDescription( &pName);
	if (NS_SUCCEEDED( rv)) {
		description = pName;
		delete [] pName;
	}
	else
		description = "Unknown description";
	
	// call the module to get the info we need
	m_pModules->AddModule( clsId, pSupports, title.GetUnicode(), description.GetUnicode());
	
	module->Release();
	
	return NS_OK;
}


void ImportModuleDesc::SetSupports( const char *pSupports)
{
	if (m_pSupports != nsnull)
		nsCRT::free( m_pSupports);
	m_pSupports = nsCRT::strdup( pSupports);
}

nsIImportModule *ImportModuleDesc::GetModule( PRBool keepLoaded)
{
	if (m_pModule) {
		m_pModule->AddRef();
		return( m_pModule);
	}
	
	nsresult	rv;
   	NS_WITH_SERVICE( nsIComponentManager, compMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return nsnull;
	
	rv = compMgr->CreateInstance( m_cid, nsnull, kImportModuleIID, (void **) &m_pModule);
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


void nsImportModuleList::ClearList( void)
{
	if (m_pList != nsnull) {
		for (int i = 0; i < m_count; i++) {
			if (m_pList[i] != nsnull)
				delete m_pList[i];	
			m_pList[i] = nsnull;
		}
		m_count = 0;
		nsAllocator::Free( m_pList);
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
		nsCRT::memset( m_pList, 0, sizeof( ImportModuleDesc *) * m_alloc);
	}
	
	if (m_count == m_alloc) {
		ImportModuleDesc **pList = new ImportModuleDesc *[m_alloc + 10];
		nsCRT::memset( &(pList[m_alloc]), 0, sizeof( ImportModuleDesc *) * 10);
		nsCRT::memcpy( pList, m_pList, sizeof( ImportModuleDesc *) * m_alloc);
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
	
	IMPORT_LOG3( "* nsImportService registered import module: %S, %S, %s\n", pName, pDesc, pSupports);
}
