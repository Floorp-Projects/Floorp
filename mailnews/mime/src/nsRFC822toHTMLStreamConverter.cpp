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
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIStreamConverter.h"
#include "nsRFC822toHTMLStreamConverter.h"

/* 
 * These macros are used to define a class IID for our component. Our 
 * object currently supports the nsRFC822toHTMLStreamConverter so we want 
 * to define constants for these two interfaces 
 */
static NS_DEFINE_IID(kIStreamConverter, NS_ISTREAM_CONVERTER_IID);

/* 
 * This function will be used by the factory to generate an 
 * RFC822 Converter....
 */
nsresult NS_NewRFC822HTMLConverter(nsIStreamConverter** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsRFC822toHTMLStreamConverter *converter = new nsRFC822toHTMLStreamConverter();
		if (converter)
			return converter->QueryInterface(kIStreamConverter, (void**) aInstancePtrResult);
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
NS_IMPL_ADDREF(nsRFC822toHTMLStreamConverter)
NS_IMPL_RELEASE(nsRFC822toHTMLStreamConverter)
NS_IMPL_QUERY_INTERFACE(nsRFC822toHTMLStreamConverter, kIStreamConverter); /* we need to pass in the interface ID of this interface */

/*
 * nsRFC822toHTMLStreamConverter definitions....
 */

/* 
 * Inherited methods for nsIStreamConverter 
 */
nsRFC822toHTMLStreamConverter::nsRFC822toHTMLStreamConverter()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsRFC822toHTMLStreamConverter::~nsRFC822toHTMLStreamConverter()
{
}

#include "stdio.h"
/* 
 * Inherited methods for nsIStreamListener 
 */
nsresult nsRFC822toHTMLStreamConverter::SetOutputStream(nsIOutputStream *) 
{
  /* 
  return mime_some_fun_call();
   */
  int x = 1;

  printf("Setting that output stream!\n");
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::SetOutputListener(nsIStreamListener *)
{
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::OnStartBinding(nsIURL *,const char *)
{
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::OnProgress(nsIURL *,unsigned int,unsigned int) 
{
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::OnStatus(nsIURL *,const unsigned short *)
{
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::OnStopBinding(nsIURL *,unsigned int,const unsigned short *) 
{
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::GetBindInfo(nsIURL *,struct nsStreamBindingInfo *)
{
  return NS_OK;
}

nsresult nsRFC822toHTMLStreamConverter::OnDataAvailable(nsIURL *, nsIInputStream *,unsigned int)
{
  return NS_OK;
}

