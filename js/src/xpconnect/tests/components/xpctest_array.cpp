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
private:
    nsIXPCTestArray* mReceiver;
};

xpcarraytest::xpcarraytest()
    : mReceiver(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

xpcarraytest::~xpcarraytest()
{
    NS_IF_RELEASE(mReceiver);
}

NS_IMPL_ISUPPORTS1(xpcarraytest, nsIXPCTestArray);

NS_IMETHODIMP xpcarraytest::SetReceiver(nsIXPCTestArray* aReceiver)
{
    NS_IF_ADDREF(aReceiver);
    NS_IF_RELEASE(mReceiver);
    mReceiver = aReceiver;
    return NS_OK;
}


/* void PrintIntegerArray (in PRUint32 count, [array, size_is (count)] in PRInt32 valueArray); */
NS_IMETHODIMP
xpcarraytest::PrintIntegerArray(PRUint32 count, PRInt32 *valueArray)
{
    if(mReceiver)
        return mReceiver->PrintIntegerArray(count, valueArray);
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
    if(mReceiver)
        return mReceiver->PrintStringArray(count, valueArray);
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
    if(mReceiver)
        return mReceiver->MultiplyEachItemInIntegerArray(val, count, valueArray);
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
    if(mReceiver)
        return mReceiver->MultiplyEachItemInIntegerArrayAndAppend(val, count, valueArray);
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

/* void CallEchoMethodOnEachInArray (inout nsIIDPtr uuid, inout PRUint32 count, [array, size_is (count), iid_is (uuid)] inout nsQIResult result); */
NS_IMETHODIMP
xpcarraytest::CallEchoMethodOnEachInArray(nsIID * *uuid, PRUint32 *count, void * **result)
{
    NS_ENSURE_ARG_POINTER(uuid);
    NS_ENSURE_ARG_POINTER(count);
    NS_ENSURE_ARG_POINTER(result);

    if(mReceiver)
        return mReceiver->CallEchoMethodOnEachInArray(uuid, count, result);

    // check that this is the expected type
    if(!(*uuid)->Equals(NS_GET_IID(nsIEcho)))
        return NS_ERROR_FAILURE;

    // call each and release
    nsIEcho** ifaceArray = (nsIEcho**) *result;
    for(PRUint32 i = 0; i < *count; i++)
    {
        ifaceArray[i]->SendOneString("print this from C++");
        NS_RELEASE(ifaceArray[i]);
    }

    // cleanup
    nsAllocator::Free(*uuid);
    nsAllocator::Free(*result);

    // set up to hand over array of 'this'

    *uuid = (nsIID*) nsAllocator::Clone(&NS_GET_IID(nsIXPCTestArray), 
                                        sizeof(nsIID));

    nsISupports** outArray = (nsISupports**) 
            nsAllocator::Alloc(2 * sizeof(nsISupports*));

    outArray[0] = outArray[1] = this;    
    NS_ADDREF(this);
    NS_ADDREF(this);
    *result = (void**) outArray;

    *count = 2;

    return NS_OK;
}        

/* void DoubleStringArray (inout PRUint32 count, [array, size_is (count)] inout string valueArray); */
NS_IMETHODIMP
xpcarraytest::DoubleStringArray(PRUint32 *count, char ***valueArray)
{
    NS_ENSURE_ARG_POINTER(valueArray);
    if(mReceiver)
        return mReceiver->DoubleStringArray(count, valueArray);
    if(!count || !*count)
        return NS_OK;

    char** outArray = (char**) nsAllocator::Alloc(*count * 2 * sizeof(char*));
    if(!outArray)
        return NS_ERROR_OUT_OF_MEMORY;
    
    char** p = *valueArray;
    for(PRUint32 i = 0; i < *count; i++)
    {
        int len = strlen(p[i]);
        outArray[i*2] = (char*)nsAllocator::Alloc(((len * 2)+1) * sizeof(char));                        
        outArray[(i*2)+1] = (char*)nsAllocator::Alloc(((len * 2)+1) * sizeof(char));                        

        for(int k = 0; k < len; k++)
        {
            outArray[i*2][k*2] = outArray[i*2][(k*2)+1] =
            outArray[(i*2)+1][k*2] = outArray[(i*2)+1][(k*2)+1] = p[i][k];
        }
        outArray[i*2][len*2] = outArray[(i*2)+1][len*2] = '\0';
        nsAllocator::Free(p[i]);
    }

    nsAllocator::Free(p);
    *valueArray = outArray; 
    *count = *count * 2;
    return NS_OK;
}        

/* void ReverseStringArray (in PRUint32 count, [array, size_is (count)] inout string valueArray); */
NS_IMETHODIMP
xpcarraytest::ReverseStringArray(PRUint32 count, char ***valueArray)
{
    NS_ENSURE_ARG_POINTER(valueArray);
    if(mReceiver)
        return mReceiver->ReverseStringArray(count, valueArray);
    if(!count)
        return NS_OK;

    char** p = *valueArray;
    for(PRUint32 i = 0; i < count/2; i++)
    {
        char* temp = p[i];
        p[i] = p[count-1-i];
        p[count-1-i] = temp;
    }
    return NS_OK;
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




