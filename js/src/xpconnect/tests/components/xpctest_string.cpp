/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* implement nsIXPCTestString for testing. */

#include "xpctest_private.h"

class xpcstringtest : public nsIXPCTestString
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTSTRING

    xpcstringtest();
    virtual ~xpcstringtest();
};

xpcstringtest::xpcstringtest()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

xpcstringtest::~xpcstringtest()
{
}

static NS_DEFINE_IID(kxpcstringtestIID, NS_IXPCTESTSTRING_IID);
NS_IMPL_ISUPPORTS(xpcstringtest, kxpcstringtestIID);

/* string GetStringA (); */
NS_IMETHODIMP
xpcstringtest::GetStringA(char **_retval)
{
    const char myResult[] = "result of xpcstringtest::GetStringA";

    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = (char*) nsAllocator::Clone(myResult,
                                          sizeof(char)*(strlen(myResult)+1));
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* void GetStringB (out string s); */
NS_IMETHODIMP
xpcstringtest::GetStringB(char **s)
{
    const char myResult[] = "result of xpcstringtest::GetStringB";

    if(!s)
        return NS_ERROR_NULL_POINTER;

    *s = (char*) nsAllocator::Clone(myResult,
                                    sizeof(char)*(strlen(myResult)+1));

    return *s ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


/* void GetStringC ([shared, retval] out string s); */
NS_IMETHODIMP
xpcstringtest::GetStringC(const char **s)
{
    static const char myResult[] = "result of xpcstringtest::GetStringC";
    if(!s)
        return NS_ERROR_NULL_POINTER;
    *s = myResult;
    return NS_OK;
}

// quick and dirty!!!
static PRUnichar* GetTestWString(int* size)
{
    static PRUnichar* sWStr;            
    static char str[] = "This is part of a long string... ";
    static const int slen = (sizeof(str)-1)/sizeof(char);
    static const int rep = 1;
    static const int space = (slen*rep*sizeof(PRUnichar))+sizeof(PRUnichar);

    if(!sWStr)
    {
        sWStr = (PRUnichar*) nsAllocator::Alloc(space);
        if(sWStr)
        {
            PRUnichar* p = sWStr;
            for(int k = 0; k < rep; k++)
                for (int i = 0; i < slen; i++)
                    *(p++) = (PRUnichar) str[i];
        *p = 0;        
        }
    }
    if(size)
        *size = space;
    return sWStr;
}        

/* void GetWStringCopied ([retval] out wstring s); */
NS_IMETHODIMP xpcstringtest::GetWStringCopied(PRUnichar **s)
{
    if(!s)
        return NS_ERROR_NULL_POINTER;

    int size;
    PRUnichar* str = GetTestWString(&size);
    if(!str)
        return NS_ERROR_OUT_OF_MEMORY;

    *s = (PRUnichar*) nsAllocator::Clone(str, size);
    return *s ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}        

/* void GetWStringShared ([shared, retval] out wstring s); */
NS_IMETHODIMP xpcstringtest::GetWStringShared(const PRUnichar **s)
{
    if(!s)
        return NS_ERROR_NULL_POINTER;
    *s = GetTestWString(nsnull);
    return NS_OK;
}        

/***************************************************************************/

// static
NS_IMETHODIMP
xpctest::ConstructStringTest(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpcstringtest* obj = new xpcstringtest();

    if(obj)
    {
        rv = obj->QueryInterface(aIID, aResult);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
        NS_RELEASE(obj);
    }
    else
    {
        *aResult = nsnull;
        rv = NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}
/***************************************************************************/




