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
#include "stdio.h"
#include "mimecom.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsMimeObjectClassAccess.h"

/* 
 * These macros are used to define a class IID for our component. 
 */
static NS_DEFINE_IID(kIMimeObjectClassAccess, NS_IMIME_OBJECT_CLASS_ACCESS_IID);

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewMimeObjectClassAccess(nsIMimeObjectClassAccess ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMimeObjectClassAccess *obj = new nsMimeObjectClassAccess();
		if (obj)
			return obj->QueryInterface(kIMimeObjectClassAccess, (void**) aInstancePtrResult);
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
NS_IMPL_ADDREF(nsMimeObjectClassAccess)
NS_IMPL_RELEASE(nsMimeObjectClassAccess)
NS_IMPL_QUERY_INTERFACE(nsMimeObjectClassAccess, kIMimeObjectClassAccess); /* we need to pass in the interface ID of this interface */

/*
 * nsMimeObjectClassAccess definitions....
 */

/* 
 * Inherited methods for nsMimeObjectClassAccess
 */
nsMimeObjectClassAccess::nsMimeObjectClassAccess()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMimeObjectClassAccess::~nsMimeObjectClassAccess()
{
}

nsresult
nsMimeObjectClassAccess::MimeObjectWrite(void *mimeObject, 
                                char *data, 
                                PRInt32 length, 
                                PRBool user_visible_p)
{
  int rc = XPCOM_MimeObject_write(mimeObject, data, length, user_visible_p);
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeInlineTextClass(void **ptr)
{
  *ptr = XPCOM_GetmimeInlineTextClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeLeafClass(void **ptr)
{
  *ptr = XPCOM_GetmimeLeafClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeObjectClass(void **ptr)
{
  *ptr = XPCOM_GetmimeObjectClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeContainerClass(void **ptr)
{
  *ptr = XPCOM_GetmimeContainerClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeMultipartClass(void **ptr)
{
  *ptr = XPCOM_GetmimeMultipartClass();
  return NS_OK;
}

nsresult 
nsMimeObjectClassAccess::GetmimeMultipartSignedClass(void **ptr)
{
  *ptr = XPCOM_GetmimeMultipartSignedClass();
  return NS_OK;
}
