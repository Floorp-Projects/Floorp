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

class xpcarraytest : public nsIXPCTestArray
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTARRAY

    xpcarraytest();
    virtual ~xpcarraytest();
};

xpcarraytest::xpcarraytest()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

xpcarraytest::~xpcarraytest()
{
}

NS_IMPL_ISUPPORTS1(xpcarraytest, nsIXPCTestArray);

/* void PrintIntegerArray (in PRUint32 count, [array, size_is (count)] in PRInt32 valueArray); */
NS_IMETHODIMP
xpcarraytest::PrintIntegerArray(PRUint32 count, PRInt32 *valueArray)
{
    if(valueArray && count)
    {
        for(PRUint32 i = 0; i < count; i++)
            printf("%d%s", valueArray[i], i == count -1 ? "\n" : ",");
    }
    else
        printf("empty array\n");

    return NS_OK;
}

/* void PrintStringArray (in PRUint32 count, [array, size_is (count)] in string valueArray); */
NS_IMETHODIMP
xpcarraytest::PrintStringArray(PRUint32 count, const char **valueArray)
{
    if(valueArray && count)
    {
        for(PRUint32 i = 0; i < count; i++)
            printf("\"%s\"%s", valueArray[i], i == count -1 ? "\n" : ",");
    }
    else
        printf("empty array\n");

    return NS_OK;
}

/* void MultiplyEachItemInIntegerArray (in PRInt32 val, in PRUint32 count, [array, size_is (count)] inout PRInt32 valueArray); */
NS_IMETHODIMP
xpcarraytest::MultiplyEachItemInIntegerArray(PRInt32 val, PRUint32 count, PRInt32 **valueArray)
{
    PRInt32* a;
    if(valueArray && count && nsnull != (a = *valueArray))
    {
        for(PRUint32 i = 0; i < count; i++)
            a[i] *= val;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}        

/* void MultiplyEachItemInIntegerArrayAndAppend (in PRInt32 val, inout PRUint32 count, [array, size_is (count)] inout PRInt32 valueArray); */
NS_IMETHODIMP
xpcarraytest::MultiplyEachItemInIntegerArrayAndAppend(PRInt32 val, PRUint32 *count, PRInt32 **valueArray)
{
    PRInt32* in;
    PRUint32 in_count;
    if(valueArray && count && 0 != (in_count = *count) && nsnull != (in = *valueArray))
    {
        PRInt32* out = 
            (PRInt32*) nsAllocator::Alloc(in_count * 2 * sizeof(PRUint32));

        if(!out)
            return NS_ERROR_OUT_OF_MEMORY;

        for(PRUint32 i = 0; i < in_count; i++)
        {
            out[i*2]   = in[i];
            out[i*2+1] = in[i] * val;
        }
        nsAllocator::Free(in);
        *valueArray = out;
        *count = in_count * 2;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}        

/***************************************************************************/

// static
NS_IMETHODIMP
xpctest::ConstructArrayTest(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpcarraytest* obj = new xpcarraytest();

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




