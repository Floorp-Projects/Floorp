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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* implement nsIChild for testing. */

#include "xpctest_private.h"

#define USE_MI 0

#if USE_MI
/***************************************************************************/

class xpctestOther : public nsIXPCTestOther
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTOTHER

    xpctestOther();
};

class xpctestChild : public nsIXPCTestChild, public xpctestOther
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIXPCTESTPARENT
    NS_DECL_NSIXPCTESTCHILD

    xpctestChild();
};

NS_IMPL_ISUPPORTS1(xpctestOther, nsIXPCTestOther);

xpctestOther::xpctestOther()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP xpctestOther::Method3(PRInt16 i, PRInt16 j, PRInt16 k)
{
    printf("Method3 called on inherited other\n");
    return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED1(xpctestChild, xpctestOther, nsIXPCTestChild)

xpctestChild::xpctestChild()
{
}

NS_IMETHODIMP xpctestChild::Method1(PRInt16 i)
{
    printf("Method1 called on child\n");
    return NS_OK;
}

NS_IMETHODIMP xpctestChild::Method1a(nsIXPCTestParent *foo)
{
    printf("Method1a called on child\n");
    return NS_OK;
}


NS_IMETHODIMP xpctestChild::Method2(PRInt16 i, PRInt16 j)
{
    printf("Method2 called on child\n");
    return NS_OK;
}

#if 0
class xpctestParent : public nsIXPCTestParent
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTPARENT

    xpctestParent();
};


class xpctestChild : public xpctestParent, public nsIXPCTestChild
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTCHILD

    xpctestChild();
};

NS_IMETHODIMP xpctestParent::Method1(PRInt16 i)
{
    printf("Method1 called on parent via child\n");
    return NS_OK;
}
NS_IMETHODIMP xpctestParent::Method1a(nsIXPCTestParent *foo)
{
    printf("Method1a called on parent via child\n");
    return NS_OK;
}

#endif

/***************************************************************************/
#else
/***************************************************************************/
class xpctestChild : public nsIXPCTestChild
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTPARENT
    NS_DECL_NSIXPCTESTCHILD

    xpctestChild();
};

NS_IMPL_ADDREF(xpctestChild);
NS_IMPL_RELEASE(xpctestChild);

NS_IMETHODIMP
xpctestChild::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(NS_GET_IID(nsIXPCTestChild)) ||
        iid.Equals(NS_GET_IID(nsIXPCTestParent)) ||
        iid.Equals(NS_GET_IID(nsISupports))) {
        *result = NS_STATIC_CAST(nsIXPCTestChild*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}

xpctestChild::xpctestChild()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP xpctestChild::Method1(PRInt16 i)
{
    printf("Method1 called on child\n");
    return NS_OK;
}

NS_IMETHODIMP xpctestChild::Method1a(nsIXPCTestParent *foo)
{
    printf("Method1a called on child\n");
    return NS_OK;
}

NS_IMETHODIMP xpctestChild::Method2(PRInt16 i, PRInt16 j)
{
    printf("Method2 called on child\n");
    return NS_OK;
}
#endif
/***************************************************************************/

// static
NS_IMETHODIMP
xpctest::ConstructChild(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpctestChild* obj = new xpctestChild();
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




