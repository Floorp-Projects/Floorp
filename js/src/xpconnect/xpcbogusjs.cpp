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

/* Temporary JSAPI interface related stuff. */

#include "xpcprivate.h"

NS_IMPL_ISUPPORTS(nsJSContext, NS_IJSCONTEXT_IID)

nsJSContext::nsJSContext(JSContext* cx)
    : mJSContext(cx)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}        

nsresult
nsJSContext::GetNative(JSContext** cx)
{
    NS_PRECONDITION(cx,"bad param");
    *cx = mJSContext;
    return NS_OK;
}        

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsJSObject, NS_IJSOBJECT_IID)

nsJSObject::nsJSObject(nsIJSContext* aJSContext, JSObject* jsobj)
    : mJSContext(aJSContext),
      mJSObject(jsobj)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    NS_ADDREF(mJSContext);
    JSContext* cx;
    if(NS_SUCCEEDED(mJSContext->GetNative(&cx)))
        JS_AddRoot(cx, &mJSObject);

}        

nsJSObject::~nsJSObject()
{
    JSContext* cx;
    if(NS_SUCCEEDED(mJSContext->GetNative(&cx)))
        JS_RemoveRoot(cx, &mJSObject);
    NS_RELEASE(mJSContext);
}        

nsresult
nsJSObject::GetNative(JSObject** jsobj)
{
    NS_PRECONDITION(jsobj,"bad param");
    *jsobj = mJSObject;
    return NS_OK;
}        

/***************************************************************************/

XPC_PUBLIC_API(nsIJSContext*)
XPC_NewJSContext(JSContext* cx)
{
    return new nsJSContext(cx);
}        

XPC_PUBLIC_API(nsIJSObject*)
XPC_NewJSObject(nsIJSContext* aJSContext, JSObject* jsobj)
{
    return new nsJSObject(aJSContext, jsobj);
}        

