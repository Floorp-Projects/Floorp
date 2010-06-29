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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
  NS_LOG_ADDREF(this, mRefCnt, "xpctestNoisy", sizeof(*this));
  printf("Noisy %d - incremented refcount to %d\n", mID, mRefCnt.get());
  return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) xpctestNoisy::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  --mRefCnt;
  printf("Noisy %d - decremented refcount to %d\n", mID, mRefCnt.get());
  NS_LOG_RELEASE(this, mRefCnt, "xpctestNoisy");
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

    if (iid.Equals(NS_GET_IID(nsIXPCTestNoisy)) ||
        iid.Equals(NS_GET_IID(nsISupports))) {
        *result = static_cast<nsIXPCTestNoisy*>(this);
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




