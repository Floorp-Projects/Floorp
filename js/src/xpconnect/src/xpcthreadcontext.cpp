/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Implement global service to track stack of JSContext per thread. */

#include "xpcprivate.h"

JS_STATIC_DLL_CALLBACK(void)
xpc_StackDtorCB(void* ptr)
{
    nsDeque* myStack = (nsDeque*) ptr;
    if(myStack)
        delete myStack;
}

static nsDeque*
GetMyStack()
{
#define BAD_TLS_INDEX ((PRUintn) -1)
    nsDeque* myStack;
    static PRUintn index = BAD_TLS_INDEX;
    if(index == BAD_TLS_INDEX)
    {
        if(PR_FAILURE == PR_NewThreadPrivateIndex(&index, xpc_StackDtorCB))
        {
            NS_ASSERTION(0, "PR_NewThreadPrivateIndex failed!");
            return nsnull;
        }
    }

    myStack = (nsDeque*) PR_GetThreadPrivate(index);
    if(!myStack)
    {
        if(nsnull != (myStack = new nsDeque(nsnull)))
        {
            if(PR_FAILURE == PR_SetThreadPrivate(index, myStack))
            {
                NS_ASSERTION(0, "PR_SetThreadPrivate failed!");
                delete myStack;
                myStack = nsnull;
            }
        }
        else
        {
            NS_ASSERTION(0, "new nsDeque failed!");
        }
    }
    return myStack;
}


/***************************************************************************/

/*
* This object holds state that we don't want to lose!
*
* The plan is that once created this object never goes away. We do an
* intentional extra addref at construction to keep it around even if no one
* is using it.
*/

NS_IMPL_ISUPPORTS1(nsXPCThreadJSContextStackImpl, nsIJSContextStack)

static nsXPCThreadJSContextStackImpl* gXPCThreadJSContextStack = nsnull;

nsXPCThreadJSContextStackImpl::nsXPCThreadJSContextStackImpl()
{
    NS_INIT_ISUPPORTS();
}

nsXPCThreadJSContextStackImpl::~nsXPCThreadJSContextStackImpl()
{
    gXPCThreadJSContextStack = nsnull;
}

//static
nsXPCThreadJSContextStackImpl*
nsXPCThreadJSContextStackImpl::GetSingleton()
{
    if(!gXPCThreadJSContextStack)
    {
        gXPCThreadJSContextStack = new nsXPCThreadJSContextStackImpl();
        if(gXPCThreadJSContextStack)
        {
            // hold an extra reference to lock it down    
            NS_ADDREF(gXPCThreadJSContextStack);
        }
    }
    NS_IF_ADDREF(gXPCThreadJSContextStack);
    return gXPCThreadJSContextStack;
}

void
nsXPCThreadJSContextStackImpl::FreeSingleton()
{
    nsXPCThreadJSContextStackImpl* tcs = gXPCThreadJSContextStack;
    if (tcs) {
        nsrefcnt cnt;
        NS_RELEASE2(tcs, cnt);
#ifdef XPC_DUMP_AT_SHUTDOWN
        if (0 != cnt) {
            printf("*** dangling reference to nsXPCThreadJSContextStackImpl: refcnt=%d\n", cnt);
        }
#endif
    }
}

/* readonly attribute PRInt32 Count; */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::GetCount(PRInt32 *aCount)
{
    if(!aCount)
        return NS_ERROR_NULL_POINTER;

    nsDeque* myStack = GetMyStack();

    if(!myStack)
    {
        *aCount = 0;
        return NS_ERROR_FAILURE;
    }

    *aCount = myStack->GetSize();
    return NS_OK;
}

/* JSContext Peek (); */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::Peek(JSContext * *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    nsDeque* myStack = GetMyStack();

    if(!myStack)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;
    }

    PRInt32 size = myStack->GetSize();
    if(size > 0)
        *_retval = (JSContext*) myStack->ObjectAt(size - 1);
    else
        *_retval = nsnull;
    return NS_OK;
}

/* JSContext Pop (); */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::Pop(JSContext * *_retval)
{
    nsDeque* myStack = GetMyStack();

    if(!myStack)
    {
        if(_retval)
            *_retval = nsnull;
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(myStack->GetSize() > 0, "ThreadJSContextStack underflow");

    if(_retval)
        *_retval = (JSContext*) myStack->Pop();
    else
        myStack->Pop();
    return NS_OK;
}

/* void Push (in JSContext cx); */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::Push(JSContext * cx)
{
    nsDeque* myStack = GetMyStack();

    if(!myStack)
        return NS_ERROR_FAILURE;

    myStack->Push(cx);
    return NS_OK;
}
