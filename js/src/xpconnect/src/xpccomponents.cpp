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

/* The "Components" xpcom objects for JavaScript. */

#include "xpcprivate.h"

/***************************************************************************/

class InterfacesScriptable : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    InterfacesScriptable();
    virtual ~InterfacesScriptable();
private:
    void CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary);
 };

/**********************************************/

InterfacesScriptable::InterfacesScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

InterfacesScriptable::~InterfacesScriptable() {}

static NS_DEFINE_IID(kInterfacesScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(InterfacesScriptable, kInterfacesScriptableIID);

XPC_IMPLEMENT_FORWARD_CREATE(InterfacesScriptable);
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(InterfacesScriptable);
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_ENUMERATE(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_CALL(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_FINALIZE(InterfacesScriptable);

NS_IMETHODIMP
InterfacesScriptable::LookupProperty(JSContext *cx, JSObject *obj,
                                     jsid id,
                                     JSObject **objp, JSProperty **propp,
                                     nsIXPConnectWrappedNative* wrapper,
                                     nsIXPCScriptable* arbitrary,
                                     JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                              NULL, retval)) && *retval)
        return NS_OK;
    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     NULL, retval);
}

NS_IMETHODIMP
InterfacesScriptable::GetProperty(JSContext *cx, JSObject *obj,
                                  jsid id, jsval *vp,
                                  nsIXPConnectWrappedNative* wrapper,
                                  nsIXPCScriptable* arbitrary,
                                  JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->GetProperty(cx, obj, id, vp, wrapper,
                                              NULL, retval)) && *retval &&
                                              *vp != JSVAL_VOID)
        return NS_OK;

    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, NULL, retval);
}


void
InterfacesScriptable::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                                    nsIXPConnectWrappedNative* wrapper,
                                    nsIXPCScriptable* arbitrary)
{
    jsval idval;
    const char* property_name = NULL;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != NULL)
    {
        nsJSIID* nsid = nsJSIID::NewID(property_name);
        nsIXPConnectWrappedNative* nsid_wrapper;
        if(nsid)
        {
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(xpc)
            {
                if(NS_SUCCEEDED(xpc->WrapNative(cx, nsid,
                                                nsIJSIID::GetIID(),
                                                &nsid_wrapper)))
                {
                    JSObject* idobj;
                    if(NS_SUCCEEDED(nsid_wrapper->GetJSObject(&idobj)))
                    {
                        JSBool retval;
                        jsval val = OBJECT_TO_JSVAL(idobj);
                        arbitrary->SetProperty(cx, obj, id, &val, wrapper,
                                               NULL, &retval);
                    }
                    NS_RELEASE(nsid_wrapper);
                }
                NS_RELEASE(xpc);
            }
        }
    }
}


/***************************************************************************/

class nsXPCInterfaces : public nsIXPCInterfaces
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS;

public:
    nsXPCInterfaces();
    virtual ~nsXPCInterfaces();
private:
    InterfacesScriptable* mScriptable;
};

nsXPCInterfaces::nsXPCInterfaces()
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
    mScriptable = new InterfacesScriptable();
}

nsXPCInterfaces::~nsXPCInterfaces()
{
    NS_RELEASE(mScriptable);
}

NS_IMPL_ADDREF(nsXPCInterfaces)
NS_IMPL_RELEASE(nsXPCInterfaces)
NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(nsXPCInterfaces, mScriptable)

/***************************************************************************/
/***************************************************************************/
class ClassesScriptable : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    ClassesScriptable();
    virtual ~ClassesScriptable();
private:
    void CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary);
 };

/**********************************************/

ClassesScriptable::ClassesScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

ClassesScriptable::~ClassesScriptable() {}

static NS_DEFINE_IID(kClassesScriptableIID, NS_IXPCCLASSES_IID);
NS_IMPL_ISUPPORTS(ClassesScriptable, kClassesScriptableIID);

XPC_IMPLEMENT_FORWARD_CREATE(ClassesScriptable);
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(ClassesScriptable);
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(ClassesScriptable);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(ClassesScriptable);
XPC_IMPLEMENT_FORWARD_ENUMERATE(ClassesScriptable);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_CALL(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(ClassesScriptable);
XPC_IMPLEMENT_FORWARD_FINALIZE(ClassesScriptable);

NS_IMETHODIMP
ClassesScriptable::LookupProperty(JSContext *cx, JSObject *obj,
                                  jsid id,
                                  JSObject **objp, JSProperty **propp,
                                  nsIXPConnectWrappedNative* wrapper,
                                  nsIXPCScriptable* arbitrary,
                                  JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                              NULL, retval)) && *retval)
        return NS_OK;
    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     NULL, retval);
}

NS_IMETHODIMP
ClassesScriptable::GetProperty(JSContext *cx, JSObject *obj,
                               jsid id, jsval *vp,
                               nsIXPConnectWrappedNative* wrapper,
                               nsIXPCScriptable* arbitrary,
                               JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->GetProperty(cx, obj, id, vp, wrapper,
                                           NULL, retval)) && *retval &&
                                           *vp != JSVAL_VOID)
        return NS_OK;

    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, NULL, retval);
}


void
ClassesScriptable::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                                 nsIXPConnectWrappedNative* wrapper,
                                 nsIXPCScriptable* arbitrary)
{
    jsval idval;
    const char* property_name = NULL;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != NULL)
    {
        nsJSCID* nsid = nsJSCID::NewID(property_name);
        nsIXPConnectWrappedNative* nsid_wrapper;
        if(nsid)
        {
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(xpc)
            {
                if(NS_SUCCEEDED(xpc->WrapNative(cx, nsid,
                                                nsIJSCID::GetIID(),
                                                &nsid_wrapper)))
                {
                    JSObject* idobj;
                    if(NS_SUCCEEDED(nsid_wrapper->GetJSObject(&idobj)))
                    {
                        JSBool retval;
                        jsval val = OBJECT_TO_JSVAL(idobj);
                        arbitrary->SetProperty(cx, obj, id, &val, wrapper,
                                               NULL, &retval);
                    }
                    NS_RELEASE(nsid_wrapper);
                }
                NS_RELEASE(xpc);
            }
        }
    }
}

/***************************************************************************/

class nsXPCClasses : public nsIXPCClasses
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS;

public:
    nsXPCClasses();
    virtual ~nsXPCClasses();
private:
    ClassesScriptable* mScriptable;
};

nsXPCClasses::nsXPCClasses()
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
    mScriptable = new ClassesScriptable();
}

nsXPCClasses::~nsXPCClasses()
{
    NS_RELEASE(mScriptable);
}

NS_IMPL_ADDREF(nsXPCClasses)
NS_IMPL_RELEASE(nsXPCClasses)
NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(nsXPCClasses, mScriptable)

/***************************************************************************/
/***************************************************************************/
static NS_DEFINE_IID(kComponentsIID, NS_IXPCCOMPONENTS_IID);
NS_IMPL_QUERY_INTERFACE(nsXPCComponents, kComponentsIID)
NS_IMPL_ADDREF(nsXPCComponents)
NS_IMPL_RELEASE(nsXPCComponents)

nsXPCComponents::nsXPCComponents()
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
    mInterfaces = new nsXPCInterfaces();
    mClasses = new nsXPCClasses();
}

nsXPCComponents::~nsXPCComponents()
{
    if(mInterfaces)
        NS_RELEASE(mInterfaces);
    if(mClasses)
        NS_RELEASE(mClasses);
}

NS_IMETHODIMP
nsXPCComponents::GetInterfaces(nsIXPCInterfaces * *aInterfaces)
{
    if(mInterfaces)
    {
        NS_ADDREF(mInterfaces);
        *aInterfaces = mInterfaces;
        return NS_OK;
    }
    *aInterfaces = NULL;
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsXPCComponents::GetClasses(nsIXPCClasses * *aClasses)
{
    if(mClasses)
    {
        NS_ADDREF(mClasses);
        *aClasses = mClasses;
        return NS_OK;
    }
    *aClasses = NULL;
    return NS_ERROR_UNEXPECTED;
}

/***************************************************************************/

XPC_PUBLIC_API(nsIXPCComponents*)
XPC_GetXPConnectComponentsObject()
{
    return new nsXPCComponents();
}

