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

/********************************************************************************************************
 
     Wrapper class for various parsing URL parsing routines...
 
*********************************************************************************************************/

#ifndef _nsMimeURLUtils_h__
#define _nsMimeURLUtils_h__

#include "nsIMimeURLUtils.h" 

class nsMimeURLUtils: public nsIMimeURLUtils { 
public: 
    nsMimeURLUtils();
    virtual ~nsMimeURLUtils();

    /* this macro defines QueryInterface, AddRef and Release for this class */
	  NS_DECL_ISUPPORTS

    static const nsIID& IID(void) { static nsIID iid = NS_IMIME_URLUTILS_IID; return iid; }

    NS_IMETHOD    URLType(const char *URL, PRInt32  *retType);

    NS_IMETHOD    ReduceURL (char *url, char **retURL);

    NS_IMETHOD    ScanForURLs(const char *input, int32 input_size,
                              char *output, int output_size, PRBool urls_only);

    NS_IMETHOD    MakeAbsoluteURL(char * absolute_url, char * relative_url, char **retURL);
}; 


/* this function will be used by the factory to generate an RFC-822 Parser....*/
extern nsresult NS_NewMimeURLUtils(nsIMimeURLUtils ** aInstancePtrResult);

#endif /* _nsMimeURLUtils_h__ */
