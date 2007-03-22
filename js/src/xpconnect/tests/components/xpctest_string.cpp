/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    NS_ADDREF_THIS();
}

xpcstringtest::~xpcstringtest()
{
}

NS_IMPL_ISUPPORTS1(xpcstringtest, nsIXPCTestString)

/* string GetStringA (); */
NS_IMETHODIMP
xpcstringtest::GetStringA(char **_retval)
{
    const char myResult[] = "result of xpcstringtest::GetStringA";

    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = (char*) nsMemory::Clone(myResult,
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

    *s = (char*) nsMemory::Clone(myResult,
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
        sWStr = (PRUnichar*) nsMemory::Alloc(space);
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

    *s = (PRUnichar*) nsMemory::Clone(str, size);
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




