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

// APIAPI.H  This header file containes the main api repository
// interface.  Use the APIAPI to register interfaces and retrieve
// registered interfaces.

#ifndef __APIAPI_H
#define __APIAPI_H

#ifdef _WIN32
#include "objbase.h"
#else
#include <memory.h>
#include "compobj.h"
#endif
#include "assert.h"

#define APIID(id,x)		DEFINE_GUID(id,0x30611040 + x,0x8c7a,0x11cf,0xac,0x4c,0x44,0x45,0x53,0x54,0x00,0x00);

APIID(IID_IApiapi,0);		// base id

typedef enum {
	ApiPrivate = 0x0,
	ApiPublic 
} APIEXPORT;

typedef int (*APIBOOTER)(char *);
typedef char * APICLASS;
typedef void * APISIGNATURE;

#define APISIGCMP(a,b) ((a)==(b))

#define	APICLASS_APIAPI		"APIAPI"

class IApiapi {
public:
	virtual LPUNKNOWN GetFirstInstance ( 
        APICLASS lpszClassName
		) = 0;

    virtual LPUNKNOWN GetNextInstance (
        APICLASS lpszClassName,
        LPUNKNOWN pUnk
        ) = 0;

	virtual void * GetFirstApiInstance ( 
        REFIID refiid,
        LPUNKNOWN * ppUnk 
		) = 0;

	virtual int RemoveInstance(
		LPUNKNOWN pUnk
		) = 0;

	virtual LPUNKNOWN CreateClassInstance (
   		APICLASS lpszClassName,
        LPUNKNOWN pUnkOuter = NULL,
        APISIGNATURE apiSig = 0
		) = 0;

	virtual int RegisterClassFactory (
   		APICLASS lpszClassName,
   		LPCLASSFACTORY pUnknown,
		char * szModuleName = 0
		) = 0;
};

typedef IApiapi * LPAPIAPI;

#ifdef WIN32
	#ifdef __BORLANDC__
   	#define API_PUBLIC(__x)		__x
	#else      
		#define API_PUBLIC(__x)		_declspec(dllexport) __x
   #endif
#else
	#define	API_PUBLIC(__x)		__x _cdecl _export _loadds
#endif

extern "C" {
	API_PUBLIC(LPAPIAPI) GetAPIAPI(void);
}

class ApiApi {
public:
	IApiapi * m_Apiapi;
	ApiApi ( ) 
	{ 
   	  m_Apiapi = GetAPIAPI();
      assert(m_Apiapi);
	}
	IApiapi * operator -> ( )
	{
		return m_Apiapi;
	}
};

class ApiPtr {
protected:
    LPUNKNOWN m_pUnknown;
    void * m_pSomeInterface;
public:
    ApiPtr ( REFIID apiid, LPUNKNOWN pUnknown = 0 )
    {  
		if (!pUnknown) {
			LPAPIAPI  pApiapi = GetAPIAPI();
			assert(pApiapi);
			m_pSomeInterface = pApiapi->GetFirstApiInstance(apiid,&m_pUnknown);
			m_pUnknown = NULL;
		}						  
		else {
			m_pUnknown = pUnknown;
			assert(pUnknown->QueryInterface(apiid,(LPVOID*)&m_pSomeInterface)==NOERROR);
		}

    }
    ~ApiPtr ( )
    {
		if (m_pUnknown)
    		m_pUnknown->Release();
    }
    void * GetAPI ( ) 
    {
        return m_pSomeInterface;
    }
};        

#define APIPTRDEF(apiid,i,v,unk) ApiPtr v##_ptr(apiid,unk); \
                        i * v = (i *)v##_ptr.GetAPI();

#define ApiApiPtr(v) LPAPIAPI v = GetAPIAPI()
#define DECLARE_FACTORY(f) static f __##f

// Generic Classes

class CGenericObject    :   public  IUnknown
{
protected:
	ULONG       m_ulRefCount;
   
public:
    CGenericObject ( ) {
        m_ulRefCount = 0;
    }

	virtual ~CGenericObject() {}

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)    Release(void);
};

class CGenericFactory   :   public  IClassFactory
{
protected:
	ULONG m_ulRefCount;
   
public:
    CGenericFactory ( ) {
        m_ulRefCount = 0;
    }

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)    Release(void);
   
	// IClassFactory Interface
	STDMETHODIMP			CreateInstance(LPUNKNOWN,REFIID,LPVOID*);
	STDMETHODIMP			LockServer(BOOL);

};

#endif
