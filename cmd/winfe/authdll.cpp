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
/* ****************************************************************************
 *
 */
#include "stdafx.h"
#include <windows.h>
#include <ole2.h>
#include "DAIntf.h"
#include "xp.h"

#include <initguid.h>
#include "DAIntf.h"


/* ****************************************************************************
 *
 */
class	CDALoader
{
	IDAProcess *m_pIDAProcess;

public:
	
	CDALoader(void);
	~CDALoader(void);

	IDAProcess *Load(void);

	IDAProcess *Interface(void) { return m_pIDAProcess; }
};


/* ****************************************************************************
 *
 */
CDALoader::CDALoader(void)
	:	m_pIDAProcess(NULL)
{
}


/* ****************************************************************************
 *
 */
CDALoader::~CDALoader(void)
{
	if	(m_pIDAProcess)
	{
		m_pIDAProcess->Release();

		CoUninitialize();
	}
}


/* ****************************************************************************
 *
 */
IDAProcess *	CDALoader::Load(void)
{
	static tried_once = FALSE;
			   

	if	(m_pIDAProcess)
	{
		return	m_pIDAProcess;
	}
			   
	if(tried_once)
		return NULL;
	else
		tried_once = TRUE;

	if	(SUCCEEDED(CoInitialize(NULL)))
	{
		if	(SUCCEEDED(CoCreateInstance(CLSID_DA,
										NULL,
										CLSCTX_INPROC_SERVER,
										IID_IDAProcess,
										(LPVOID *)&m_pIDAProcess)))
		{
			return	m_pIDAProcess;
		}

		CoUninitialize();
	}

	return	NULL;
}


/* ****************************************************************************
 *
 */
PRIVATE XP_List * 	address_auth_list = 0;
static	CDALoader	g_DA;

/* ****************************************************************************
 *
 */
class	CDAPacket	:	public	IDAPacket
{
	LONG	m_cRef;
	char *  m_pPassedInHeader;

public:
	CDAPacket(char *pHeader);
	~CDAPacket(void);

	char *	m_pNewHeaders;

    // IUnknown methods.
    STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);
	STDMETHOD(QueryInterface)(THIS_
							  REFIID riid,
							  LPVOID *ppvObject);

    // IDAPacket methods.
    STDMETHOD(AddNameValue)(THIS_
                            const TCHAR *pszName, 
                            const TCHAR *pszValue);

    STDMETHOD(FindValue)(THIS_ 
                         const TCHAR *pszName,
                         const TCHAR *pszSubName,
                         TCHAR *      pszValue,
                         DWORD        cbValue)          const;

    STDMETHOD(ReplaceNameValue)(THIS_
                                const TCHAR *pszName, 
                                const TCHAR *pszValue);
};

/* ****************************************************************************
 *
 */
struct address_auth_assoc {
	char *address;
	CDAPacket *auth;
};


/* ****************************************************************************
 *
 */
CDAPacket::CDAPacket(char *pPassedInHeader)
	:	m_cRef(1),
		m_pNewHeaders(NULL)
{
	m_pPassedInHeader = pPassedInHeader;
}


/* ****************************************************************************
 *
 */
CDAPacket::~CDAPacket(void)
{
	if(m_pNewHeaders)
		XP_FREE(m_pNewHeaders);
}


/* ****************************************************************************
 *
 */
STDMETHODIMP_(ULONG)	CDAPacket::AddRef(void)
{
#ifdef WIN32
	return	InterlockedIncrement(&m_cRef);
#else
	return	++m_cRef;
#endif
}


/* ****************************************************************************
 *
 */
STDMETHODIMP_(ULONG)	CDAPacket::Release(void)
{
#ifdef WIN32
	return	InterlockedDecrement(&m_cRef);
#else
	return	--m_cRef;
#endif
}


/* ****************************************************************************
 *
 */
STDMETHODIMP	CDAPacket::QueryInterface
(
	REFIID 		riid,
	LPVOID *	ppvObject
)
{
	if	(!ppvObject)
	{
		return	ResultFromScode(E_INVALIDARG);
	}

	*ppvObject = NULL;

	if	(riid == IID_IDAPacket)
	{
		*ppvObject = (IDAPacket *)this;
	}	else	if	(riid == IID_IUnknown)
	{
		*ppvObject = (IUnknown *)this;
	}
	else
	{
		return	ResultFromScode(E_NOINTERFACE);
	}

	AddRef();

	return	NOERROR;
}


/* ****************************************************************************
 *
 */
STDMETHODIMP	CDAPacket::AddNameValue
(
	const TCHAR *pszName, 
    const TCHAR *pszValue
)
{
	if	(!pszName || !pszValue)
	{
		return	ResultFromScode(E_FAIL);
	}

	if(m_pNewHeaders)
	  {
	  	char * tmp = m_pNewHeaders;
	  	m_pNewHeaders = PR_smprintf("%s%s: %s"CRLF, 
										tmp ? tmp : "",
							 			pszName, 
							 			pszValue);
		XP_FREE(tmp);
	  }
	else
	  {
	  	m_pNewHeaders = PR_smprintf("%s: %s"CRLF, 
								 		pszName, 
							 			pszValue);
	  }

	return	NOERROR;
}


/* ****************************************************************************
 *
 */
STDMETHODIMP	CDAPacket::FindValue
(
	const TCHAR *pszName,
    const TCHAR *pszSubName,
    TCHAR *      pszValue,
    DWORD        cbValue
)	const
{
	
	#define WWW_AUTHENTICATE "WWW-Authenticate"

	if(strncasecomp(pszName, WWW_AUTHENTICATE, sizeof(WWW_AUTHENTICATE) - 1))
		return ResultFromScode(E_FAIL);  /* not a valid name */

	if(pszSubName == NULL)
	  {
		strncpy(pszValue, "Remote-Passphrase", CASTSIZE_T(cbValue));
		pszValue[cbValue-1] = '\0';
		return (NOERROR);
	  }

	char *token;

	while((token = strcasestr(m_pPassedInHeader, pszSubName)))
	  {
	  	/* look for an immediate equal and then a quote
		 */
		char *cp = token;
		char *end_of_value;

		cp += XP_STRLEN(pszSubName);

		while(isspace(*cp)) cp++;

		if(*cp != '=')
			continue;
			
		while(isspace(*cp)) cp++;

		if(*++cp != '"')
			continue;

		end_of_value = strchr(++cp, '"');

		if(!end_of_value)
		  return ResultFromScode(E_FAIL);

		*end_of_value = '\0';

		if(end_of_value - cp > (ptrdiff_t)cbValue)
		  {
		  	*end_of_value = '"';
			return ResultFromScode(E_FAIL);
		  }

		XP_STRCPY(pszValue, cp);

		*end_of_value = '"';

		return (NOERROR);
	  }

	return ResultFromScode(E_FAIL);
}


/* ****************************************************************************
 *
 */
STDMETHODIMP	CDAPacket::ReplaceNameValue
(
	const TCHAR *pszName, 
    const TCHAR *pszValue
)
{
	return ResultFromScode(E_NOTIMPL);
}


/* ****************************************************************************
 *
 */
PRIVATE	char *GetMethod(URL_Struct *URL_s)
{
	if(URL_s->method == URL_POST_METHOD)
		return	XP_STRDUP("POST");
	else if(URL_s->method == URL_HEAD_METHOD)
		return	XP_STRDUP("HEAD");
	else 
		return	XP_STRDUP("GET");
}


/* ****************************************************************************
 *
 */
extern "C" int
WFE_DoCompuserveAuthenticate(MWContext *context,
							 URL_Struct *URL_s, 
							 char *authenticate_header_value)
{
	struct address_auth_assoc * assoc_obj = 0;
	struct address_auth_assoc * cur_assoc_ptr;
	CDAPacket *auth;
	XP_List *list_ptr;
	char *host;
	HRESULT	status;

	// Load the DA OLE server if it hasn't already been loaded.
	IDAProcess *pIDAProcess = g_DA.Interface();

	if	(!pIDAProcess)
	{
		return NET_AUTH_FAILED_DISPLAY_DOCUMENT;
	}

	if(!address_auth_list)
	  {
		address_auth_list = XP_ListNew();
		if(!address_auth_list)
		return NET_AUTH_FAILED_DISPLAY_DOCUMENT;
	  }

	/* search for an existing association */
	list_ptr = address_auth_list;
	while((cur_assoc_ptr = (address_auth_assoc *)XP_ListNextObject(list_ptr)))
	  {
	  	if(!XP_STRCMP(cur_assoc_ptr->address, URL_s->address))
		  {
			assoc_obj = cur_assoc_ptr;
	  		break;
	  	  }
	  }

	if(assoc_obj)
	  {
	  	XP_ListRemoveObject(address_auth_list, assoc_obj);
	  	delete assoc_obj->auth;
		XP_FREE(assoc_obj->address);
		XP_FREE(assoc_obj);	  	
	  }

	if(URL_s->server_status != 200
		&& URL_s->server_status != 401)
		return(FALSE);

	
	auth = new CDAPacket(authenticate_header_value);
												 
	auth->m_pNewHeaders = XP_STRDUP("Extension: Security/Remote-Passphrase"CRLF);

	host = NET_ParseURL(URL_s->address, GET_HOST_PART);

	if(URL_s->server_status == 401)	
	  {
		struct SDAAuthData auth_data_struct;
		char  username[256] = "";
		char  password[256] = "";
		char  realm[256] = "";
		char * path = NET_ParseURL(URL_s->address, GET_PATH_PART | GET_SEARCH_PART);
			
		auth_data_struct.pIDAPacketIn	= auth;
		auth_data_struct.pszHost 		= host;
		auth_data_struct.pszURI			= path;
		auth_data_struct.pszMethod		= GetMethod(URL_s);
		auth_data_struct.pIDAPacketOut	= auth;
		auth_data_struct.bShowDialog 	= 1;
		auth_data_struct.hParent 		= NULL;
		auth_data_struct.pszUsername 	= username;
		auth_data_struct.wMaxUsername 	= sizeof(username) - 1;
		auth_data_struct.pszRealmname 	= realm;
		auth_data_struct.pszPassword 	= password;
		auth_data_struct.wMaxPassword 	= sizeof(password) - 1;
		auth_data_struct.pIDAPassword 	= NULL;

		status = pIDAProcess->On401Authenticate(&auth_data_struct);

		if(status == NOERROR)
		  {
			assoc_obj = XP_NEW(struct address_auth_assoc);

			if(!assoc_obj)
				return NET_AUTH_FAILED_DISPLAY_DOCUMENT;

			assoc_obj->auth = auth;
			assoc_obj->address = XP_STRDUP(URL_s->address);
			XP_ListAddObject(address_auth_list, assoc_obj);

			XP_FREE(host);
			return(NET_RETRY_WITH_AUTH);
		  }
	  }	
	else
	  {
		status = pIDAProcess->On200Authenticate(host, auth); 
	  }
							
	XP_FREE(host);
	delete(auth);

	if(status != NOERROR)
		return(NET_AUTH_FAILED_DONT_DISPLAY);
	else
		return(NET_AUTH_SUCCEEDED);
}


/* ****************************************************************************
 *
 */
extern "C" char *
WFE_BuildCompuserveAuthString(URL_Struct *URL_s)
{
	XP_List * list_ptr;
	struct address_auth_assoc *cur_assoc_ptr;
	struct address_auth_assoc *assoc_obj = NULL;
	static char *rv=NULL;  /* malloc and free on successive calls */
	
	IDAProcess *pIDAProcess = g_DA.Load();

	if(pIDAProcess)
	{
		/* search for an existing association */
		list_ptr = address_auth_list;
		while((cur_assoc_ptr = (address_auth_assoc *)XP_ListNextObject(list_ptr)))
		  {
			if(!XP_STRCMP(cur_assoc_ptr->address, URL_s->address))
				assoc_obj = cur_assoc_ptr;
		  }
	
		if(assoc_obj)
		  {
			/* since we found it in the assoc list then
			 * we have gotten a 401 and are about to
			 * send another request.  Send the
			 * header.
			 */
			return(assoc_obj->auth->m_pNewHeaders);
		  }
		
		/* if we didn't find it in the assoc list then
		 * call the cheat routine to see if we need
		 * to send any headers
		 */	
	
		if(rv)
		  {
			XP_FREE(rv);
			rv = NULL;
		  }
	
		HRESULT		status 	= ResultFromScode(E_FAIL);
		char *		host 	= NET_ParseURL(URL_s->address, GET_HOST_PART);
		char *		path	= NET_ParseURL(URL_s->address, GET_PATH_PART | GET_SEARCH_PART);
		char *		method  = GetMethod(URL_s);

		if(host && path && method)
		{
			CDAPacket   daPacket("");   
	
			status = pIDAProcess->Cheat(host, path, method, &daPacket);
	
			rv = XP_STRDUP(daPacket.m_pNewHeaders);
		}

		if(host)
			XP_FREE(host);
		if(path)
			XP_FREE(path);
		if(method)
			XP_FREE(method);
	}
					
	return(rv);
}
