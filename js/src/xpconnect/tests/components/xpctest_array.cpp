/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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

    // a test that forces a QI to some other arbitrary type.
    if(mReceiver)
    {
        nsCOMPtr<nsIEcho> echo = do_QueryInterface(mReceiver);
    }

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
            (PRInt32*) nsMemory::Alloc(in_count * 2 * sizeof(PRUint32));

        if(!out)
            return NS_ERROR_OUT_OF_MEMORY;

        for(PRUint32 i = 0; i < in_count; i++)
        {
            out[i*2]   = in[i];
            out[i*2+1] = in[i] * val;
        }
        nsMemory::Free(in);
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
    nsMemory::Free(*uuid);
    nsMemory::Free(*result);

    // set up to hand over array of 'this'

    *uuid = (nsIID*) nsMemory::Clone(&NS_GET_IID(nsIXPCTestArray), 
                                        sizeof(nsIID));

    nsISupports** outArray = (nsISupports**) 
            nsMemory::Alloc(2 * sizeof(nsISupports*));

    outArray[0] = outArray[1] = this;    
    NS_ADDREF(this);
    NS_ADDREF(this);
    *result = (void**) outArray;

    *count = 2;

    return NS_OK;
}        

/* void CallEchoMethodOnEachInArray2 (inout PRUint32 count, [array, size_is (count)] inout nsIEcho result); */
NS_IMETHODIMP
xpcarraytest::CallEchoMethodOnEachInArray2(PRUint32 *count, nsIEcho ***result)
{
    NS_ENSURE_ARG_POINTER(count);
    NS_ENSURE_ARG_POINTER(result);

    if(mReceiver)
        return mReceiver->CallEchoMethodOnEachInArray2(count, result);

    // call each and release
    nsIEcho** ifaceArray =  *result;
    for(PRUint32 i = 0; i < *count; i++)
    {
        ifaceArray[i]->SendOneString("print this from C++");
        NS_RELEASE(ifaceArray[i]);
    }

    // cleanup
    nsMemory::Free(*result);

    // setup return
    *count = 0;
    *result = nsnull;
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

    char** outArray = (char**) nsMemory::Alloc(*count * 2 * sizeof(char*));
    if(!outArray)
        return NS_ERROR_OUT_OF_MEMORY;
    
    char** p = *valueArray;
    for(PRUint32 i = 0; i < *count; i++)
    {
        int len = strlen(p[i]);
        outArray[i*2] = (char*)nsMemory::Alloc(((len * 2)+1) * sizeof(char));                        
        outArray[(i*2)+1] = (char*)nsMemory::Alloc(((len * 2)+1) * sizeof(char));                        

        for(int k = 0; k < len; k++)
        {
            outArray[i*2][k*2] = outArray[i*2][(k*2)+1] =
            outArray[(i*2)+1][k*2] = outArray[(i*2)+1][(k*2)+1] = p[i][k];
        }
        outArray[i*2][len*2] = outArray[(i*2)+1][len*2] = '\0';
        nsMemory::Free(p[i]);
    }

    nsMemory::Free(p);
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

/* void PrintStringWithSize (in PRUint32 count, [size_is (count)] in string str); */
NS_IMETHODIMP
xpcarraytest::PrintStringWithSize(PRUint32 count, const char *str)
{
    if(mReceiver)
        return mReceiver->PrintStringWithSize(count, str);
    printf("\"%s\" : %d\n", str, count);
    return NS_OK;
}        

/* void DoubleString (inout PRUint32 count, [size_is (count)] inout string str); */
NS_IMETHODIMP
xpcarraytest::DoubleString(PRUint32 *count, char **str)
{
    NS_ENSURE_ARG_POINTER(str);
    if(mReceiver)
        return mReceiver->DoubleString(count, str);
    if(!count || !*count)
        return NS_OK;

    char* out = (char*) nsMemory::Alloc(((*count * 2)+1) * sizeof(char));                        
    if(!out)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 k;
    for(k = 0; k < *count; k++)
        out[k*2] = out[(k*2)+1] = (*str)[k];
    out[k*2] = '\0';
    nsMemory::Free(*str);
    *str = out; 
    *count = *count * 2;
    return NS_OK;
}        

/* void GetStrings (out PRUint32 count, [array, size_is (count), retval] out string str); */
NS_IMETHODIMP 
xpcarraytest::GetStrings(PRUint32 *count, char ***str)
{
    const static char *strings[] = {"one", "two", "three", "four"};
    const static PRUint32 scount = sizeof(strings)/sizeof(strings[0]);

    if(mReceiver)
        return mReceiver->GetStrings(count, str);

    char** out = (char**) nsMemory::Alloc(scount * sizeof(char*));
    if(!out)
        return NS_ERROR_OUT_OF_MEMORY;
    for(PRUint32 i = 0; i < scount; ++i)
    {
        out[i] = (char*) nsMemory::Clone(strings[i], strlen(strings[i])+1);
        // failure unlikely, leakage foolishly tolerated in this test case
        if(!out[i])
            return NS_ERROR_OUT_OF_MEMORY;
    }

    *count = scount;
    *str = out;
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




