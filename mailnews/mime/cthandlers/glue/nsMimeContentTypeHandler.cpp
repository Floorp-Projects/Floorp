/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "stdio.h"
#include "nscore.h"
#include "plstr.h"
#include "prtypes.h"
//#include "mimecth.h"
#include "nsMimeContentTypeHandler.h"

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewMimeContentTypeHandler(nsIMimeContentTypeHandler ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMimeContentTypeHandler *obj = new nsMimeContentTypeHandler();
		if (obj)
			return obj->QueryInterface(nsIMimeContentTypeHandler::GetIID(), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeContentTypeHandler)
NS_IMPL_RELEASE(nsMimeContentTypeHandler)
NS_IMPL_QUERY_INTERFACE(nsMimeContentTypeHandler, nsIMimeContentTypeHandler::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsIMimeEmitter definitions....
 */
nsMimeContentTypeHandler::nsMimeContentTypeHandler()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();

}

nsMimeContentTypeHandler::~nsMimeContentTypeHandler(void)
{
}

extern "C" char             *MIME_GetContentType(void);
extern "C" MimeObjectClass  *MIME_CreateContentTypeHandlerClass(const char *content_type, 
                                                                contentTypeHandlerInitStruct *initStruct);

// Get the content type if necessary
nsresult
nsMimeContentTypeHandler::GetContentType(char **contentType)
{
  *contentType = MIME_GetContentType();
  return NS_OK;
}

// Set the output stream for processed data.
nsresult
nsMimeContentTypeHandler::CreateContentTypeHandlerClass(const char *content_type, 
                                                contentTypeHandlerInitStruct *initStruct, 
                                                MimeObjectClass **objClass)
{
  *objClass = MIME_CreateContentTypeHandlerClass(content_type, initStruct);
	if (!*objClass)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
  else
    return NS_OK;
}
