/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h"    // precompiled header...
#include "mimehdrs.h"
#include "nsIMimeHeaders.h"

class nsMimeHeaders : public nsIMimeHeaders
{
 public: 
 	nsMimeHeaders();
 	virtual ~nsMimeHeaders();
 
	 /* this macro defines QueryInterface, AddRef and Release for this class */
	 NS_DECL_ISUPPORTS
	 
	 NS_DECL_NSIMIMEHEADERS

private:
	MimeHeaders	*	mHeaders;
};


nsMimeHeaders::nsMimeHeaders() :
	mHeaders(nsnull)
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMimeHeaders::~nsMimeHeaders()
{
	if (mHeaders)
		MimeHeaders_free(mHeaders);
}

NS_IMPL_ISUPPORTS(nsMimeHeaders, nsCOMTypeInfo<nsIMimeHeaders>::GetIID())

nsresult nsMimeHeaders::Initialize(const char * aAllHeaders)
{
	/* just in case we want to reuse the object, cleanup...*/
	if (mHeaders)
		MimeHeaders_free(mHeaders);

	if (mHeaders = MimeHeaders_new())
		return MimeHeaders_parse_line(aAllHeaders, nsCRT::strlen(aAllHeaders), mHeaders);
	
	return NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsMimeHeaders::ExtractHeader(const char *headerName, PRBool getAllOfThem, char **_retval)
{
	if (! mHeaders)
		return NS_ERROR_NOT_INITIALIZED;
	
	*_retval = MimeHeaders_get(mHeaders, headerName, PR_FALSE, getAllOfThem);
	return NS_OK;
}

/* 
 * This function will be used by the factory to generate an 
 * mime headers class object....
 */
nsresult NS_NewMimeHeaders(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	//nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMimeHeaders *obj = new nsMimeHeaders();
		if (obj)
			return obj->QueryInterface(aIID, (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}
