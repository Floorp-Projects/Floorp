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

/* implement nsIXPCTestNoisy for testing. */

#include "xpctest_private.h"

class xpctestNoisy : public nsIXPCTestNoisy
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTNOISY

    xpctestNoisy();
    virtual ~xpctestNoisy();
private:
    static int sID;
    static int sCount;
    int        mID;
};

int xpctestNoisy::sID = 0;
int xpctestNoisy::sCount = 0;


NS_IMETHODIMP_(nsrefcnt) xpctestNoisy::AddRef(void)
{
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, __FILE__, __LINE__);
  printf("Noisy %d - incremented refcount to %d\n", mID, mRefCnt);
  return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) xpctestNoisy::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  --mRefCnt;
  printf("Noisy %d - decremented refcount to %d\n", mID, mRefCnt);
  NS_LOG_RELEASE(this, mRefCnt, __FILE__, __LINE__);
  if (mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

NS_IMETHODIMP
xpctestNoisy::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIXPCTestNoisy::GetIID()) ||
        iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *result = NS_STATIC_CAST(nsIXPCTestNoisy*, this);
        printf("Noisy %d - QueryInterface called and succeeding\n", mID);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        printf("Noisy %d - QueryInterface for interface I don't do\n", mID);
        return NS_NOINTERFACE;
    }
}

xpctestNoisy::xpctestNoisy()
    : mID(++sID)
{
    sCount++;
    NS_INIT_REFCNT();
    printf("Noisy %d - Created, %d total\n", mID, sCount);
    NS_ADDREF_THIS();
}

xpctestNoisy::~xpctestNoisy()
{
    sCount--;
    printf("Noisy %d - Destroyed, %d total\n", mID, sCount);
}

NS_IMETHODIMP xpctestNoisy::Squawk()
{
    printf("Noisy %d - Squawk called\n", mID);
    return NS_OK;
}

/***************************************************************************/

// static
NS_IMETHODIMP
xpctest::ConstructNoisy(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpctestNoisy* obj = new xpctestNoisy();

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




