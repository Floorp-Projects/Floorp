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

/* implement nsIEcho for testing. */

#include "xpctest_private.h"

class xpctestEcho : public nsIEcho
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD SetReceiver(nsIEcho* aReceiver);
    NS_IMETHOD SendOneString(const char* str);
    NS_IMETHOD In2OutOneInt(int input, int* output);
    NS_IMETHOD In2OutAddTwoInts(int input1,
                                int input2,
                                int* output1,
                                int* output2,
                                int* result);
    NS_IMETHOD In2OutOneString(const char* input, char** output);
    NS_IMETHOD SimpleCallNoEcho();
    NS_IMETHOD SendManyTypes(PRUint8              p1,
                             PRInt16             p2,
                             PRInt32             p3,
                             PRInt64             p4,
                             PRUint8              p5,
                             PRUint16            p6,
                             PRUint32            p7,
                             PRUint64            p8,
                             float             p9,
                             double            p10,
                             PRBool            p11,
                             char              p12,
                             PRUint16            p13,
                             nsID*             p14,
                             const char*       p15,
                             const PRUnichar*  p16);
    NS_IMETHOD SendInOutManyTypes(PRUint8*    p1,
                                  PRInt16*   p2,
                                  PRInt32*   p3,
                                  PRInt64*   p4,
                                  PRUint8*    p5,
                                  PRUint16*  p6,
                                  PRUint32*  p7,
                                  PRUint64*  p8,
                                  float*   p9,
                                  double*  p10,
                                  PRBool*  p11,
                                  char*    p12,
                                  PRUint16*  p13,
                                  nsID**   p14,
                                  char**   p15,
                                  PRUint16** p16);
    NS_IMETHOD MethodWithNative(int p1, void* p2);

    NS_IMETHOD ReturnCode(int code);

    NS_IMETHOD FailInJSTest(int fail);

    /* void SharedTest ([shared, retval] out string str); */
    NS_IMETHOD SharedString(char **str);

    /* void ReturnCode_NS_OK (); */
    NS_IMETHOD ReturnCode_NS_OK();

    /* void ReturnCode_NS_COMFALSE (); */
    NS_IMETHOD ReturnCode_NS_COMFALSE();

    /* void ReturnCode_NS_ERROR_NULL_POINTER (); */
    NS_IMETHOD ReturnCode_NS_ERROR_NULL_POINTER();

    /* void ReturnCode_NS_ERROR_UNEXPECTED (); */
    NS_IMETHOD ReturnCode_NS_ERROR_UNEXPECTED();

    /* void ReturnCode_NS_ERROR_OUT_OF_MEMORY (); */
    NS_IMETHOD ReturnCode_NS_ERROR_OUT_OF_MEMORY();


    xpctestEcho();
private:
    nsIEcho* mReceiver;
};

/***************************************************************************/

static NS_DEFINE_IID(kxpctestEchoIID, NS_IECHO_IID);
NS_IMPL_ISUPPORTS(xpctestEcho, kxpctestEchoIID);

xpctestEcho::xpctestEcho()
    : mReceiver(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP xpctestEcho::SetReceiver(nsIEcho* aReceiver)
{
    if(mReceiver)
        NS_RELEASE(mReceiver);
    mReceiver = aReceiver;
    if(mReceiver)
        NS_ADDREF(mReceiver);
    return NS_OK;
}

NS_IMETHODIMP xpctestEcho::SendOneString(const char* str)
{
    if(mReceiver)
        return mReceiver->SendOneString(str);
    return NS_OK;
}

NS_IMETHODIMP xpctestEcho::In2OutOneInt(int input, int* output)
{
    *output = input;
    return NS_OK;
}

NS_IMETHODIMP xpctestEcho::In2OutAddTwoInts(int input1,
                                       int input2,
                                       int* output1,
                                       int* output2,
                                       int* result)
{
    *output1 = input1;
    *output2 = input2;
    *result = input1+input2;
    return NS_OK;
}

NS_IMETHODIMP xpctestEcho::In2OutOneString(const char* input, char** output)
{
    char* p;
    int len;
    if(input && output &&
       (NULL != (p = (char*)nsAllocator::Alloc(len=strlen(input)+1))))
    {
        memcpy(p, input, len);
        *output = p;
        return NS_OK;
    }
    if(output)
        *output = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP xpctestEcho::SimpleCallNoEcho()
{
    return NS_OK;
}

NS_IMETHODIMP
xpctestEcho::SendManyTypes(PRUint8              p1,
                      PRInt16             p2,
                      PRInt32             p3,
                      PRInt64             p4,
                      PRUint8              p5,
                      PRUint16            p6,
                      PRUint32            p7,
                      PRUint64            p8,
                      float             p9,
                      double            p10,
                      PRBool            p11,
                      char              p12,
                      PRUint16            p13,
                      nsID*             p14,
                      const char*       p15,
                      const PRUnichar*  p16)
{
    if(mReceiver)
        return mReceiver->SendManyTypes(p1, p2, p3, p4, p5, p6, p7, p8, p9,
                                        p10, p11, p12, p13, p14, p15, p16);
    return NS_OK;
}

NS_IMETHODIMP
xpctestEcho::SendInOutManyTypes(PRUint8*    p1,
                           PRInt16*   p2,
                           PRInt32*   p3,
                           PRInt64*   p4,
                           PRUint8*    p5,
                           PRUint16*  p6,
                           PRUint32*  p7,
                           PRUint64*  p8,
                           float*   p9,
                           double*  p10,
                           PRBool*  p11,
                           char*    p12,
                           PRUint16*  p13,
                           nsID**   p14,
                           char**   p15,
                           PRUint16** p16)
{
    if(mReceiver)
        return mReceiver->SendInOutManyTypes(p1, p2, p3, p4, p5, p6, p7, p8, p9,
                                             p10, p11, p12, p13, p14, p15, p16);
    return NS_OK;
}

NS_IMETHODIMP
xpctestEcho::MethodWithNative(int p1, void* p2)
{
    return NS_OK;
}

NS_IMETHODIMP
xpctestEcho::ReturnCode(int code)
{
    return (nsresult) code;
}

NS_IMETHODIMP
xpctestEcho::FailInJSTest(int fail)
{
    if(mReceiver)
        return mReceiver->FailInJSTest(fail);
    return NS_OK;
}

NS_IMETHODIMP
xpctestEcho::SharedString(char **str)
{
    *str = "a static string";
/*
    // to do non-shared we clone the string:
    char buf[] = "a static string";
    int len;
    *str = (char*)nsAllocator::Alloc(len=strlen(buf)+1);
    memcpy(*str, buf, len);
*/
    return NS_OK;
}

NS_IMETHODIMP
xpctestEcho::ReturnCode_NS_OK()
{return NS_OK;}

NS_IMETHODIMP
xpctestEcho::ReturnCode_NS_COMFALSE()
{return NS_COMFALSE;}

NS_IMETHODIMP
xpctestEcho::ReturnCode_NS_ERROR_NULL_POINTER()
{return NS_ERROR_NULL_POINTER;}

NS_IMETHODIMP
xpctestEcho::ReturnCode_NS_ERROR_UNEXPECTED()
{return NS_ERROR_UNEXPECTED;}

NS_IMETHODIMP
xpctestEcho::ReturnCode_NS_ERROR_OUT_OF_MEMORY()
{return NS_ERROR_OUT_OF_MEMORY;}

/***************************************************************************/

// static 
NS_IMETHODIMP 
xpctest::ConstructEcho(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpctestEcho* obj = new xpctestEcho();
    rv = obj->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(obj);
    return rv;
}        
