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

    void RealizeInterface(JSContext *cx, JSObject *obj, const char *iface_name,
                          nsIXPConnectWrappedNative* wrapper,
                          nsIXPCScriptable* arbitrary);

    void CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary);

    void FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary);

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

XPC_IMPLEMENT_IGNORE_CREATE(InterfacesScriptable);
// XPC_IMPLEMENT_IGNORE_GETFLAGS(InterfacesScriptable);
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(InterfacesScriptable);
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(InterfacesScriptable);
// XPC_IMPLEMENT_FORWARD_ENUMERATE(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_CALL(InterfacesScriptable);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(InterfacesScriptable);
XPC_IMPLEMENT_FORWARD_FINALIZE(InterfacesScriptable);

NS_IMETHODIMP
InterfacesScriptable::GetFlags(JSContext *cx, JSObject *obj,
                               nsIXPConnectWrappedNative* wrapper,
                               JSUint32* flagsp,
                               nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

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


NS_IMETHODIMP
InterfacesScriptable::Enumerate(JSContext *cx, JSObject *obj,
                                JSIterateOp enum_op,
                                jsval *statep, jsid *idp,
                                nsIXPConnectWrappedNative *wrapper,
                                nsIXPCScriptable *arbitrary,
                                JSBool *retval)
{
    if(enum_op == JSENUMERATE_INIT)
        FillCache(cx, obj, wrapper, arbitrary);

    return arbitrary->Enumerate(cx, obj, enum_op, statep, idp, wrapper,
                                arbitrary, retval);
}

/* enumerate the known interfaces, adding a property for each new one */
void
InterfacesScriptable::FillCache(JSContext *cx, JSObject *obj,
                                nsIXPConnectWrappedNative *wrapper,
                                nsIXPCScriptable *arbitrary)
{
    nsIInterfaceInfoManager*  iim = nsnull;
    nsIEnumerator*            Interfaces = nsnull;
    nsISupports*              is_Interface;
    nsIInterfaceInfo*         Interface;
    nsresult                  rv;

    if(!(iim = XPTI_GetInterfaceInfoManager()))
    {
        NS_ASSERTION(0,"failed to get the InterfaceInfoManager");
        goto done;
    }

    if(NS_FAILED(iim->GetInterfaceEnumerator(&Interfaces)))
    {
        NS_ASSERTION(0,"failed to get interface enumeration");
        goto done;
    }

    if(NS_FAILED(rv = Interfaces->First()))
    {
        NS_ASSERTION(0,"failed to go to first item in interface enumeration");
        goto done;
    }

    do
    {
        if(NS_FAILED(rv = Interfaces->CurrentItem(&is_Interface)))
        {
            /* maybe something should be done,
             * debugging info at least? */
            Interfaces->Next();
            continue;
        }

        rv = is_Interface->
            QueryInterface(nsCOMTypeInfo<nsIInterfaceInfo>::GetIID(),
                           (void **)&Interface);

        if(!NS_FAILED(rv))
        {
            char *interface_name;
            Interface->GetName(&interface_name);
            RealizeInterface(cx, obj, interface_name, wrapper, arbitrary);
            nsAllocator::Free(interface_name);
            NS_RELEASE(Interface);
        }
        else
        {
            /* that would be odd */
        }

        NS_RELEASE(is_Interface);
        Interfaces->Next();

    } while (!NS_FAILED(Interfaces->IsDone()));

done:
    if(Interfaces)
        NS_RELEASE(Interfaces);
    if(iim)
        NS_RELEASE(iim);
}

void
InterfacesScriptable::RealizeInterface(JSContext *cx, JSObject *obj,
                                       const char *iface_name,
                                       nsIXPConnectWrappedNative* wrapper,
                                       nsIXPCScriptable* arbitrary)
{
    jsval prop;

    if(!JS_LookupProperty(cx, obj, iface_name, &prop) ||
       JSVAL_IS_PRIMITIVE(prop))
    {
        jsid id;
        JSString *jstrid;

        if(NULL != (jstrid = JS_InternString(cx, iface_name)) &&
           JS_ValueToId(cx, STRING_TO_JSVAL(jstrid), &id))
        {
            CacheDynaProp(cx, obj, id, wrapper, arbitrary);
        }
    }
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
    NS_DECL_ISUPPORTS

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
    if(mScriptable)
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

static NS_DEFINE_IID(kClassesScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(ClassesScriptable, kClassesScriptableIID);

XPC_IMPLEMENT_FORWARD_CREATE(ClassesScriptable);
XPC_IMPLEMENT_IGNORE_GETFLAGS(ClassesScriptable);
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
    NS_DECL_ISUPPORTS

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
class ComponentsScriptable : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    ComponentsScriptable();
    virtual ~ComponentsScriptable();
 };

/**********************************************/

ComponentsScriptable::ComponentsScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

ComponentsScriptable::~ComponentsScriptable() {}

static NS_DEFINE_IID(kComponentsScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(ComponentsScriptable, kComponentsScriptableIID)

XPC_IMPLEMENT_FORWARD_CREATE(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_GETFLAGS(ComponentsScriptable);
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(ComponentsScriptable);
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(ComponentsScriptable);
XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(ComponentsScriptable);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(ComponentsScriptable);
XPC_IMPLEMENT_FORWARD_ENUMERATE(ComponentsScriptable);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_CALL(ComponentsScriptable);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(ComponentsScriptable);
XPC_IMPLEMENT_FORWARD_FINALIZE(ComponentsScriptable);

NS_IMETHODIMP
ComponentsScriptable::GetProperty(JSContext *cx, JSObject *obj,
                               jsid id, jsval *vp,
                               nsIXPConnectWrappedNative* wrapper,
                               nsIXPCScriptable* arbitrary,
                               JSBool* retval)
{
    *retval = JS_TRUE;

    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    if(xpcc && xpcc->GetStringID(XPCContext::IDX_LAST_RESULT) == id)
    {
        if(JS_NewDoubleValue(cx, (jsdouble) (PRInt32)xpcc->GetLastResult(), vp))
            return NS_OK;
    }
    *vp = JSVAL_VOID;
    return NS_OK;
}

/***************************************************************************/
static NS_DEFINE_IID(kComponentsIID, NS_IXPCCOMPONENTS_IID);
NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(nsXPCComponents, mScriptable)
NS_IMPL_ADDREF(nsXPCComponents)
NS_IMPL_RELEASE(nsXPCComponents)

nsXPCComponents::nsXPCComponents()
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
    mInterfaces = new nsXPCInterfaces();
    mClasses = new nsXPCClasses();
    mScriptable = new ComponentsScriptable();
}

nsXPCComponents::~nsXPCComponents()
{
    if(mInterfaces)
        NS_RELEASE(mInterfaces);
    if(mClasses)
        NS_RELEASE(mClasses);
    if(mScriptable)
        NS_RELEASE(mScriptable);
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

NS_IMETHODIMP
nsXPCComponents::GetStack(nsIJSStackFrameLocation * *aStack)
{
    return nsXPConnect::GetXPConnect()->GetCurrentJSStack(aStack);
}
