/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
            return NULL;
        }
    }

    myStack = (nsDeque*) PR_GetThreadPrivate(index);
    if(!myStack)
    {
        if(NULL != (myStack = new nsDeque(nsnull)))
        {
            if(PR_FAILURE == PR_SetThreadPrivate(index, myStack))
            {
                NS_ASSERTION(0, "PR_SetThreadPrivate failed!");
                delete myStack;
                myStack = NULL;
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

nsXPCThreadJSContextStackImpl::nsXPCThreadJSContextStackImpl()
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
}

nsXPCThreadJSContextStackImpl::~nsXPCThreadJSContextStackImpl() {}

static NS_DEFINE_IID(knsXPCThreadJSContextStackImplIID, NS_IJSCONTEXTSTACK_IID);
NS_IMPL_ISUPPORTS(nsXPCThreadJSContextStackImpl, knsXPCThreadJSContextStackImplIID);

//static 
nsXPCThreadJSContextStackImpl* 
nsXPCThreadJSContextStackImpl::GetSingleton()
{
    static nsXPCThreadJSContextStackImpl* singleton = NULL;
    if(!singleton)
        singleton = new nsXPCThreadJSContextStackImpl();
    if(singleton)
        NS_ADDREF(singleton);
    return singleton;
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

    if(myStack->GetSize() > 0)
        *_retval = (JSContext*) myStack->Peek();
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
