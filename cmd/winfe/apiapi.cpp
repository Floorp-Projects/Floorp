/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "stdafx.h"
#include "apiapi.h"	    // get all global interfaces

class CAPIList {
protected:
	CAPIList * m_pNext;
	CAPIList * m_pPrevious;
	void  * m_pData;
public:
	CAPIList(void * pData = NULL);
	~CAPIList();
	long elements(void);
	CAPIList * Add(void * element);
	CAPIList * Remove(void * element);
	void * GetAt (int element);
	inline CAPIList * GetNext(void) {
      return m_pNext; 
      }
	inline CAPIList * GetPrevious(void) { 
      return m_pPrevious; 
      }
	inline CAPIList * SetNext(CAPIList * pElement) {
		CAPIList * pNext = m_pNext;
		m_pNext = pElement;
		return pNext;
		}
	inline CAPIList * SetPrevious(CAPIList * pElement) {
		CAPIList * pPrevious = m_pPrevious;
		m_pPrevious = pElement;
		return pPrevious;
		}
	inline void * GetData(void) { return m_pData; }
};

CAPIList::CAPIList(void * pData)
{
	m_pNext = NULL;
	m_pPrevious = NULL;
	SetNext(this);
	SetPrevious(this);
	m_pData = pData;
}

CAPIList::~CAPIList()
{
	assert(m_pNext);
	assert(m_pPrevious);
	GetPrevious()->SetNext(GetNext());
	GetNext()->SetPrevious(GetPrevious());
}

CAPIList * CAPIList::Add(void * element)
{
	CAPIList * pNode = new CAPIList(element);
	CAPIList * pOld = SetNext(pNode);
	pNode->SetNext(pOld);
	pOld->SetPrevious(pNode);
	pNode->SetPrevious(this);
	return pNode;
}

long CAPIList::elements(void)
{
	CAPIList * pNode = this;
	long lcount = 0;
	do {
		lcount++;
		pNode = pNode->GetNext();
	} while (pNode != this);
	return lcount;
}

CAPIList * CAPIList::Remove(void * element)
{
	BOOL found = FALSE;
	CAPIList * pNode = this;
	do {
		if (pNode->GetData() == element) {
			found = TRUE;
			break;
		}
		pNode = pNode->GetNext();	
	} while (pNode != this);

	if (found) {
		if (GetNext() == this) {
			delete this;
			return NULL;
		}
		else {
			CAPIList * ptr = pNode->GetPrevious();
			delete pNode;
			return ptr;
		}
	}					
	return this;
}

void * CAPIList::GetAt ( int element )
{
	CAPIList * pNode = this;
	do {
		if (!element)
			return pNode->GetData();
		element--;
		pNode = pNode->GetNext();
	} while (pNode != this);
	return NULL;
}

// the API repository implements the APIAPI interface and maintains
// the list of APIs, their IDs, and reference counts.

class ApiEntry {
protected:
    LPUNKNOWN m_pUnk;
    APISIGNATURE m_apiSig;
public:
    ApiEntry(LPUNKNOWN pUnk = NULL, APISIGNATURE apiSig = NULL ) {
        m_pUnk =pUnk;
        m_apiSig = apiSig;
    }
    inline LPUNKNOWN GetUnk() { return m_pUnk; }
    inline APISIGNATURE GetSig() { return m_apiSig; }
    inline void SetSig(APISIGNATURE apiSig) { m_apiSig = apiSig; }
    inline void SetUnk(LPUNKNOWN pUnk) { m_pUnk = pUnk; }
};

class ClassFactory {						// class factory entries
public:
   LPCLASSFACTORY	m_pClassFactory;	// class instance creation factory
   char * m_pszClassName;				// name of class
   BOOL m_bIsLoaded;  					// is class factory loaded?
   char * m_pszModuleName;				// module containing class factory
   HMODULE m_hModule;					// factory dll instance
   CAPIList  * m_ApiList; 					// list of api instances

	ClassFactory (char * pszClassName, LPCLASSFACTORY pFactory, char * pszModule = 0 );
   ~ClassFactory();

    LPUNKNOWN CreateClassInstance(LPUNKNOWN pUnkOuter, APISIGNATURE apiSig = NULL);      
    LPUNKNOWN GetFirst(void);
    LPUNKNOWN GetNext(LPUNKNOWN);
    void Insert(LPUNKNOWN pUnk,APISIGNATURE apiSig = NULL);
    void Remove(LPUNKNOWN);
    LPUNKNOWN LocateBySignature(APISIGNATURE);
	BOOL AttemptToLoad(void);
};

void ClassFactory::Insert(
    LPUNKNOWN pUnk,
    APISIGNATURE apiSig
    )
{
	if (!m_ApiList) {
		m_ApiList = new CAPIList((void*)(new ApiEntry(pUnk,apiSig)));
    }
	else {
		m_ApiList = m_ApiList->Add((void*)(new ApiEntry(pUnk,apiSig)));
    }
}

void ClassFactory::Remove(LPUNKNOWN pUnk)
{
	if (m_ApiList) {
        ApiEntry * pEntry;
        for (int i = 0; i < m_ApiList->elements(); i++) {
            pEntry = (ApiEntry *)m_ApiList->GetAt(i);
            assert(pEntry);
            if (pEntry->GetUnk() == pUnk) {
		        m_ApiList = m_ApiList->Remove((void*)pEntry);
                delete pEntry;
                return;
            }
        }
    }
}

LPUNKNOWN ClassFactory::GetFirst(void)
{
	if (!m_ApiList)
		return 0;

	if (!m_ApiList->elements())
		return 0;

	return ((ApiEntry *)(m_ApiList->GetAt(0)))->GetUnk();
}

LPUNKNOWN ClassFactory::GetNext(LPUNKNOWN pUnk)
{
	if (m_ApiList) {
		for (int i = 0; i < m_ApiList->elements() - 1; i++)
   			if (((ApiEntry*)m_ApiList->GetAt(i))->GetUnk() == (void*)pUnk)
      			return ((ApiEntry*)m_ApiList->GetAt(i+1))->GetUnk();
	}
   return 0;
}

LPUNKNOWN ClassFactory::LocateBySignature(APISIGNATURE apiSig)
{
    if (!apiSig || !m_ApiList)
        return NULL;
    ApiEntry * pEntry;
    for (int i = 0; i < m_ApiList->elements(); i++) {
        pEntry = (ApiEntry*)m_ApiList->GetAt(i);
        if (APISIGCMP(pEntry->GetSig(),apiSig))
            return pEntry->GetUnk();
    }
    return NULL;
}

ClassFactory::ClassFactory(char * pszClassName, LPCLASSFACTORY pFactory, char * pszModule)
{
	assert(pszClassName);
	m_pClassFactory = pFactory;
	m_pszClassName = strdup(pszClassName);
	m_hModule = NULL;
	m_ApiList = NULL;
	if (pszModule != 0) {
   		m_pszModuleName = strdup(pszModule);
		m_bIsLoaded = FALSE;
	}
	else {
   		m_bIsLoaded = TRUE;
		m_pszModuleName = NULL;
	}
}

ClassFactory::~ClassFactory()
{
   if (m_pszClassName != NULL)
		free (m_pszClassName);
   if (m_pszModuleName != NULL)
      free (m_pszModuleName);
   if (m_hModule != NULL)
      FreeLibrary(m_hModule);

   int elements;
   elements = m_ApiList ? (int)m_ApiList->elements( ): 0;
   while ( elements ) {
       ApiEntry * pEntry = (ApiEntry *)m_ApiList->GetAt(elements - 1);
       assert(pEntry);
       LPUNKNOWN entry = (LPUNKNOWN)pEntry->GetUnk();
       // FIXME release objects here!
       if (entry)
        while (entry->Release())
         	;
       delete pEntry;
       elements--;
   }
}

LPUNKNOWN ClassFactory::CreateClassInstance(
    LPUNKNOWN pUnkOuter, 
    APISIGNATURE apiSig)
{
   assert(m_pClassFactory);
   LPUNKNOWN pUnknown;
   if (m_pClassFactory->CreateInstance( 
   	pUnkOuter, IID_IUnknown, (LPVOID*)&pUnknown) == NOERROR)
      	Insert(pUnknown,apiSig);
   return pUnknown;
}

class ClassRepository	:   public CGenericFactory,
                            public IApiapi {
protected:
    ULONG m_ulRefCount;
	CAPIList * m_ClassList; 
	// lookup attempts to find an API entry based on id
	ClassFactory * Lookup (char * pszClassName);   
    ClassFactory * GetFirst(void);
    ClassFactory * GetNext(ClassFactory *);
    void Remove(ClassFactory*);
   
public:

	ClassRepository ( );
    ~ClassRepository ( );

	// IClassFactory Interface
	STDMETHODIMP			CreateInstance(LPUNKNOWN,REFIID,LPVOID*);

	// IApiapi interface
	LPUNKNOWN GetFirstInstance(APICLASS lpszClassName);
    LPUNKNOWN GetNextInstance(APICLASS lpszClassName, LPUNKNOWN pUnk );
	void * GetFirstApiInstance( REFIID refiid, LPUNKNOWN * ppUnk);
	int RemoveInstance( LPUNKNOWN pUnk );
	LPUNKNOWN CreateClassInstance (
        APICLASS lpszClassName,LPUNKNOWN pUnkOuter,APISIGNATURE apiSig);
	int RegisterClassFactory (
   	    APICLASS lpszClassName, LPCLASSFACTORY pClassFactory, char * szModuleName = 0);
};

ClassRepository::ClassRepository ( )
{
    m_ClassList = NULL;
    RegisterClassFactory(APICLASS_APIAPI,(LPCLASSFACTORY)this);
}

ClassRepository::~ClassRepository ( )
{
	if (m_ClassList) {
		int elements = (int)m_ClassList->elements( );
		while ( elements ) {
			elements--;
		}
	}
}

STDMETHODIMP ClassRepository::CreateInstance(
	LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj)
{
	*ppvObj = NULL;
	ClassRepository * pRepository = new ClassRepository;
	return pRepository->QueryInterface(refiid,ppvObj);   
}

int ClassRepository::RemoveInstance( LPUNKNOWN pUnk )
{
	if (m_ClassList) {
		int elements = (int) m_ClassList->elements();
		int i;
		for ( i = 0; i < elements; i++ ) {	
			ClassFactory * entry = (ClassFactory *)m_ClassList->GetAt(i);
            entry->Remove(pUnk);
		}
	}
	return 0;
}	

ClassFactory * ClassRepository::GetFirst(void)
{
	if (!m_ClassList)
		return 0;

	if (!m_ClassList->elements())
   		return 0;
	return (ClassFactory *)m_ClassList->GetAt(0);
}

ClassFactory * ClassRepository::GetNext(ClassFactory * pFactory)
{
	if (m_ClassList) {
		for (int i = 0; i < m_ClassList->elements() - 1; i++)
			if (m_ClassList->GetAt(i) == (void*)pFactory)
      			return (ClassFactory *)m_ClassList->GetAt(i+1);
	}
   return 0;
}

void ClassRepository::Remove(ClassFactory * pFactory)
{
	if (m_ClassList)
		m_ClassList = m_ClassList->Remove((void*)pFactory);
}

LPUNKNOWN ClassRepository::CreateClassInstance(
    APICLASS lpszClassName,
    LPUNKNOWN pUnkOuter,
    APISIGNATURE apiSig )
{
    ClassFactory * pFactory = Lookup(lpszClassName);
	if (pFactory) {
        if (apiSig) {
            LPUNKNOWN pUnk;
            if ((pUnk = pFactory->LocateBySignature(apiSig)) != NULL) 
                return pUnk;
        }
   		return pFactory->CreateClassInstance(pUnkOuter,apiSig);
    }
      
    return NULL;
}
   
BOOL ClassFactory::AttemptToLoad(void)
{
	if ((!m_pszModuleName) || m_bIsLoaded)
		return FALSE;
    // attempt to load the interface module (DLL)
	m_hModule = LoadLibrary (m_pszModuleName);
	if (m_hModule > (HMODULE)32) {
       // The class factory initializer is the first routine in the DLL.
		APIBOOTER fpApiInitialize = 
			(APIBOOTER)GetProcAddress(m_hModule,MAKEINTRESOURCE(1));
      // if we got a valid initializer, then call it with the 
      // class name.  This was the module can initialize one at 
      // a time if it likes.
		if (fpApiInitialize != NULL ) {
			m_bIsLoaded = TRUE;
			if ((*fpApiInitialize)(m_pszClassName))
				return TRUE;
			}
			else
				m_bIsLoaded = FALSE;
	}
    else {
        // couldn't load the module
        char szMessage[1024];
        sprintf ( szMessage, "Cannot load module: %.800s\nClass factory %800s", 
        		m_pszModuleName, m_pszClassName);
        MessageBox ( 0, szMessage, "Interface Error", MB_OK | MB_ICONEXCLAMATION );
    }
	return FALSE;
}

ClassFactory * ClassRepository::Lookup ( char * pszClassName )
{
	// get the number of stored elements
	if (m_ClassList) {
		int elements = (int) m_ClassList->elements();
		int i;

		// look up the api			
		for ( i = 0; i < elements; i++ )	{
			ClassFactory * entry = (ClassFactory *)m_ClassList->GetAt(i);
			if (!stricmp(entry->m_pszClassName, pszClassName)) {
				if (!entry->m_bIsLoaded)
					if (entry->AttemptToLoad())
						return entry;
					else
						return 0;
				return entry;		
			}
		}
	}
	return 0;
}

int ClassRepository::RegisterClassFactory ( 
	char * pszClassName,
   LPCLASSFACTORY pClassFactory,
   char * szModuleName )
{
	ClassFactory * pClassEntry;
	if ((pClassEntry = Lookup (pszClassName)) != 0) {
		pClassEntry->m_pClassFactory = pClassFactory;
		return 1;
	}
	else {
		pClassEntry = new ClassFactory ( pszClassName, pClassFactory, szModuleName);
		if (!m_ClassList)
			m_ClassList = new CAPIList((void*)pClassEntry);
		else
			m_ClassList = m_ClassList->Add ((void*)pClassEntry );
	}

	return 0;
}

void * ClassRepository::GetFirstApiInstance( REFIID refiid, LPUNKNOWN * ppUnk)
{
	void * pSomeInterface;
	*ppUnk = 0;
	ClassFactory * pClass = GetFirst();
	while (pClass) {
		LPUNKNOWN pUnk = pClass->GetFirst();
		while (pUnk) {
			if (pUnk->QueryInterface(refiid,(LPVOID*)&pSomeInterface) == NOERROR) {
				*ppUnk = pUnk;
				return pSomeInterface;
				}
			pUnk = pClass->GetNext(pUnk);
		}
		pClass = GetNext(pClass);
	}
	return NULL;
}

// function looks up an API by id and returns a void pointer which in
// turn can be cast to the requested interface type.  This is done 
// for you by the Api<id,interf> template.

LPUNKNOWN ClassRepository::GetFirstInstance ( char * pszClassName )
{
	if (m_ClassList) {
		for (int i = 0; i < m_ClassList->elements(); i++) {
			ClassFactory * pClassFactory = (ClassFactory *)m_ClassList->GetAt(i);
			if (!stricmp(pClassFactory->m_pszClassName, pszClassName)) {
				if (pClassFactory->m_ApiList)
					return ((ApiEntry*)pClassFactory->m_ApiList->GetAt(0))->GetUnk();
				return NULL;
			}
		}
	}
	return NULL;
}

LPUNKNOWN ClassRepository::GetNextInstance ( char * pszClassName, LPUNKNOWN pUnk )
{
	if (m_ClassList) {
		for (int i = 0; i < m_ClassList->elements(); i++) {
			ClassFactory * pClassFactory = (ClassFactory *)m_ClassList->GetAt(i);
			if (!stricmp(pClassFactory->m_pszClassName, pszClassName)) {
                CAPIList * pList = pClassFactory->m_ApiList;
    			if (pList)
                for (int j = 0; j<pList->elements()-1; j++)
                    if (((ApiEntry*)pList->GetAt(j))->GetUnk() == pUnk)
					    return ((ApiEntry*) pClassFactory->m_ApiList->GetAt(j+1))->GetUnk();
				return NULL;
			}
		}
	}
	return NULL;
}


// The apiapi object MUST be constructed first, this pragma ensures that
// the static constructor will occur before any other static construction
// happens.              
#pragma warning( disable : 4074 )
#pragma init_seg(compiler)
static ClassRepository classRepository;

// global function gets the ApiApi from the static repository
// From here, all registered APIs can be retrieved and set

API_PUBLIC(LPAPIAPI) GetAPIAPI()
{
	return (LPAPIAPI) &classRepository;
}

// Generic Objects
// Generic Factory


STDMETHODIMP CGenericFactory::QueryInterface(REFIID refiid, LPVOID * ppv)
{
    *ppv = NULL;
    if (IsEqualIID(refiid,IID_IUnknown))
        *ppv = (LPUNKNOWN) this;
    else if (IsEqualIID(refiid,IID_IClassFactory))
        *ppv = (LPCLASSFACTORY) this;

    if (*ppv != NULL) {
   	    AddRef();
        return NOERROR;
    }
            
    return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CGenericFactory::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CGenericFactory::Release(void)
{
    ULONG ulRef;
    ulRef = --m_ulRefCount;
    if (m_ulRefCount == 0) ;
//	    delete this;   	
	return ulRef;   	
}

STDMETHODIMP CGenericFactory::CreateInstance(
	LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj)
{
    return NULL;
    // User Implemented       
}

STDMETHODIMP CGenericFactory::LockServer(BOOL fLock)
{
	return NOERROR;
}

// Generic Object

STDMETHODIMP CGenericObject::QueryInterface(REFIID refiid, LPVOID * ppv)
{
    return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CGenericObject::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CGenericObject::Release(void)
{
    ULONG ulRef;
    ulRef = --m_ulRefCount;
    if (m_ulRefCount == 0) {
        classRepository.RemoveInstance(this);
	    delete this;   	
    }
	return ulRef;   	
}

