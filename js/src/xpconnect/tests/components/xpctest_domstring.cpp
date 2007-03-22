/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
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

#include "nsISupports.h"
#include "nsString.h"
#include "xpctest_domstring.h"
#include "xpctest_private.h"

class xpcTestDOMString : public nsIXPCTestDOMString {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTDOMSTRING
    xpcTestDOMString();
    virtual ~xpcTestDOMString();
private:
    nsString mStr;
};

NS_IMPL_ISUPPORTS1(xpcTestDOMString, nsIXPCTestDOMString)

xpcTestDOMString::xpcTestDOMString()
{
    NS_ADDREF_THIS();
}

xpcTestDOMString::~xpcTestDOMString()
{
}

NS_IMETHODIMP
xpcTestDOMString::HereHaveADOMString(const nsAString &str)
{
    // assignment will share buffer if possible
    mStr = str;
    return NS_OK;
}

NS_IMETHODIMP
xpcTestDOMString::DontKeepThisOne(const nsAString &str)
{
    nsCString c; c.AssignWithConversion(str);
    fprintf(stderr, "xpcTestDOMString::DontKeepThisOne: \"%s\"\n", c.get());
    return NS_OK;
}

NS_IMETHODIMP
xpcTestDOMString::GiveDOMStringTo(nsIXPCTestDOMString *recv)
{
    NS_NAMED_LITERAL_STRING(myString, "A DOM String, Just For You");
    return recv->HereHaveADOMString(myString);
}

NS_IMETHODIMP
xpcTestDOMString::PassDOMStringThroughTo(const nsAString &str,
                                         nsIXPCTestDOMString *recv)
{
    return recv->HereHaveADOMString(str);
}


NS_IMETHODIMP
xpctest::ConstructXPCTestDOMString(nsISupports *aOuter, REFNSIID aIID,
                                   void **aResult)
{
    nsresult rv;
    NS_ASSERTION(!aOuter, "no aggregation");
    xpcTestDOMString *obj = new xpcTestDOMString();
    if (obj)
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

