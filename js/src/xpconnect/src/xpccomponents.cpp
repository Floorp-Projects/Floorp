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

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

/***************************************************************************/

class nsXPCInterfaces : public nsIXPCInterfaces, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCINTERFACES
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCInterfaces();
    virtual ~nsXPCInterfaces();

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

nsXPCInterfaces::nsXPCInterfaces()
{
    NS_INIT_ISUPPORTS();
}

nsXPCInterfaces::~nsXPCInterfaces()
{
    // empty
}

NS_IMPL_ISUPPORTS2(nsXPCInterfaces, nsIXPCInterfaces, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCInterfaces)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCInterfaces)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCInterfaces)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCInterfaces)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsXPCInterfaces)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCInterfaces)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCInterfaces)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCInterfaces)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCInterfaces)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCInterfaces)

NS_IMETHODIMP
nsXPCInterfaces::GetFlags(JSContext *cx, JSObject *obj,
                          nsIXPConnectWrappedNative* wrapper,
                          JSUint32* flagsp,
                          nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCInterfaces::LookupProperty(JSContext *cx, JSObject *obj,
                                jsid id,
                                JSObject **objp, JSProperty **propp,
                                nsIXPConnectWrappedNative* wrapper,
                                nsIXPCScriptable* arbitrary,
                                JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                              nsnull, retval)) && *retval)
        return NS_OK;
    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     nsnull, retval);
}

NS_IMETHODIMP
nsXPCInterfaces::GetProperty(JSContext *cx, JSObject *obj,
                             jsid id, jsval *vp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->GetProperty(cx, obj, id, vp, wrapper,
                                           nsnull, retval)) && *retval &&
                                           *vp != JSVAL_VOID)
        return NS_OK;

    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
}


NS_IMETHODIMP
nsXPCInterfaces::Enumerate(JSContext *cx, JSObject *obj,
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
nsXPCInterfaces::FillCache(JSContext *cx, JSObject *obj,
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

    if(NS_FAILED(iim->EnumerateInterfaces(&Interfaces)))
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
    NS_IF_RELEASE(Interfaces);
    NS_IF_RELEASE(iim);
}

void
nsXPCInterfaces::RealizeInterface(JSContext *cx, JSObject *obj,
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

        if(nsnull != (jstrid = JS_InternString(cx, iface_name)) &&
           JS_ValueToId(cx, STRING_TO_JSVAL(jstrid), &id))
        {
            CacheDynaProp(cx, obj, id, wrapper, arbitrary);
        }
    }
}

void
nsXPCInterfaces::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                               nsIXPConnectWrappedNative* wrapper,
                               nsIXPCScriptable* arbitrary)
{
    jsval idval;
    const char* property_name = nsnull;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != nsnull &&
       property_name[0] != '{') // we only allow interfaces by name here
    {
        nsJSIID* nsid = nsJSIID::NewID(property_name);
        nsIXPConnectWrappedNative* nsid_wrapper;
        if(nsid)
        {
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(xpc)
            {
                if(NS_SUCCEEDED(xpc->WrapNative(cx, NS_STATIC_CAST(nsIJSID*,nsid),
                                                NS_GET_IID(nsIJSIID),
                                                &nsid_wrapper)))
                {
                    JSObject* idobj;
                    if(NS_SUCCEEDED(nsid_wrapper->GetJSObject(&idobj)))
                    {
                        JSBool retval;
                        jsval val = OBJECT_TO_JSVAL(idobj);
                        arbitrary->SetProperty(cx, obj, id, &val, wrapper,
                                               nsnull, &retval);
                    }
                    NS_RELEASE(nsid_wrapper);
                }
                else
                {
                    NS_RELEASE(nsid);
                }
                NS_RELEASE(xpc);
            }
        }
    }
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCClasses : public nsIXPCClasses, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCLASSES
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCClasses();
    virtual ~nsXPCClasses();

private:

    void RealizeClass(JSContext *cx, JSObject *obj, const char *class_name,
                      nsIXPConnectWrappedNative* wrapper,
                      nsIXPCScriptable* arbitrary);

    void CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary);

    void FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary);
};

nsXPCClasses::nsXPCClasses()
{
    NS_INIT_ISUPPORTS();
}

nsXPCClasses::~nsXPCClasses()
{
    // empty
}

NS_IMPL_ISUPPORTS2(nsXPCClasses, nsIXPCClasses, nsIXPCScriptable)

XPC_IMPLEMENT_FORWARD_CREATE(nsXPCClasses)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCClasses)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCClasses)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCClasses)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsXPCClasses)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCClasses)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCClasses)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCClasses)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCClasses)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCClasses)


NS_IMETHODIMP
nsXPCClasses::GetFlags(JSContext *cx, JSObject *obj,
                       nsIXPConnectWrappedNative* wrapper,
                       JSUint32* flagsp,
                       nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}


NS_IMETHODIMP
nsXPCClasses::LookupProperty(JSContext *cx, JSObject *obj,
                             jsid id,
                             JSObject **objp, JSProperty **propp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                              nsnull, retval)) && *retval)
        return NS_OK;
    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     nsnull, retval);
}

NS_IMETHODIMP
nsXPCClasses::GetProperty(JSContext *cx, JSObject *obj,
                          jsid id, jsval *vp,
                          nsIXPConnectWrappedNative* wrapper,
                          nsIXPCScriptable* arbitrary,
                          JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->GetProperty(cx, obj, id, vp, wrapper,
                                           nsnull, retval)) && *retval &&
                                           *vp != JSVAL_VOID)
        return NS_OK;

    CacheDynaProp(cx, obj, id, wrapper, arbitrary);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
}



NS_IMETHODIMP
nsXPCClasses::Enumerate(JSContext *cx, JSObject *obj,
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

/* enumerate the known progids, adding a property for each new one */
void
nsXPCClasses::FillCache(JSContext *cx, JSObject *obj,
                        nsIXPConnectWrappedNative *wrapper,
                        nsIXPCScriptable *arbitrary)
{

    nsIEnumerator*      Classes;
    nsISupports*        is_Class;
    nsISupportsString*  ClassNameHolder;
    nsresult            rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if(NS_FAILED(rv))
        return;

    if(NS_FAILED(rv = compMgr->EnumerateProgIDs(&Classes)))
        return;

    if(!NS_FAILED(rv = Classes->First()))
    {
        do
        {
            if(NS_FAILED(rv = Classes->CurrentItem(&is_Class))|| !is_Class)
            {
                Classes->Next();
                continue;
            }

            rv = is_Class->QueryInterface(NS_GET_IID(nsISupportsString),
                                          (void **)&ClassNameHolder);
            if(!NS_FAILED(rv))
            {
                char *class_name;
                if(NS_SUCCEEDED(ClassNameHolder->GetData(&class_name)))
                {
                    RealizeClass(cx, obj, class_name, wrapper, arbitrary);
                    nsAllocator::Free(class_name);
                }
                NS_RELEASE(ClassNameHolder);
            }
            else
                break;

            NS_RELEASE(is_Class);
            Classes->Next();

        } while(!NS_FAILED(Classes->IsDone()));
    }
    NS_RELEASE(Classes);
}

void
nsXPCClasses::RealizeClass(JSContext *cx, JSObject *obj,
                           const char *class_name,
                           nsIXPConnectWrappedNative* wrapper,
                           nsIXPCScriptable* arbitrary)
{
    jsval prop;

    if(!JS_LookupProperty(cx, obj, class_name, &prop) ||
       JSVAL_IS_PRIMITIVE(prop))
    {
        jsid id;
        JSString *jstrid;

        if(nsnull != (jstrid = JS_InternString(cx, class_name)) &&
           JS_ValueToId(cx, STRING_TO_JSVAL(jstrid), &id))
        {
            CacheDynaProp(cx, obj, id, wrapper, arbitrary);
        }
    }
}

void
nsXPCClasses::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                            nsIXPConnectWrappedNative* wrapper,
                            nsIXPCScriptable* arbitrary)
{
    jsval idval;
    const char* property_name = nsnull;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != nsnull &&
       property_name[0] != '{') // we only allow progids here
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
                                               nsnull, &retval);
                    }
                    NS_RELEASE(nsid_wrapper);
                }
                else
                {
                    NS_RELEASE(nsid);
                }
                NS_RELEASE(xpc);
            }
        }
    }
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCClassesByID : public nsIXPCClassesByID, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCLASSESBYID
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCClassesByID();
    virtual ~nsXPCClassesByID();

private:

    void RealizeClass(JSContext *cx, JSObject *obj, const char *class_name,
                      nsIXPConnectWrappedNative* wrapper,
                      nsIXPCScriptable* arbitrary,
                      JSBool knownToBeRegistered);

    void CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary,
                       JSBool knownToBeRegistered);

    void FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary);
};

nsXPCClassesByID::nsXPCClassesByID()
{
    NS_INIT_ISUPPORTS();
}

nsXPCClassesByID::~nsXPCClassesByID()
{
    // empty
}

NS_IMPL_ISUPPORTS2(nsXPCClassesByID, nsIXPCClassesByID, nsIXPCScriptable)

XPC_IMPLEMENT_FORWARD_CREATE(nsXPCClassesByID)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCClassesByID)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCClassesByID)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCClassesByID)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsXPCClassesByID)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCClassesByID)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCClassesByID)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCClassesByID)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCClassesByID)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCClassesByID)


NS_IMETHODIMP
nsXPCClassesByID::GetFlags(JSContext *cx, JSObject *obj,
                           nsIXPConnectWrappedNative* wrapper,
                           JSUint32* flagsp,
                           nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCClassesByID::LookupProperty(JSContext *cx, JSObject *obj,
                                 jsid id,
                                 JSObject **objp, JSProperty **propp,
                                 nsIXPConnectWrappedNative* wrapper,
                                 nsIXPCScriptable* arbitrary,
                                 JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                              nsnull, retval)) && *retval)
        return NS_OK;
    CacheDynaProp(cx, obj, id, wrapper, arbitrary, JS_FALSE);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     nsnull, retval);
}

NS_IMETHODIMP
nsXPCClassesByID::GetProperty(JSContext *cx, JSObject *obj,
                              jsid id, jsval *vp,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval)
{
    if(NS_SUCCEEDED(arbitrary->GetProperty(cx, obj, id, vp, wrapper,
                                           nsnull, retval)) && *retval &&
                                           *vp != JSVAL_VOID)
        return NS_OK;

    CacheDynaProp(cx, obj, id, wrapper, arbitrary, JS_FALSE);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
}



NS_IMETHODIMP
nsXPCClassesByID::Enumerate(JSContext *cx, JSObject *obj,
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

/* enumerate the known progids, adding a property for each new one */
void
nsXPCClassesByID::FillCache(JSContext *cx, JSObject *obj,
                            nsIXPConnectWrappedNative *wrapper,
                            nsIXPCScriptable *arbitrary)
{

    nsIEnumerator*      Classes;
    nsISupports*        is_Class;
    nsISupportsID*      ClassIDHolder;
    nsresult            rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if(NS_FAILED(rv))
        return;

    if(NS_FAILED(rv = compMgr->EnumerateCLSIDs(&Classes)))
        return;

    if(!NS_FAILED(rv = Classes->First()))
    {
        do
        {
            if(NS_FAILED(rv = Classes->CurrentItem(&is_Class))|| !is_Class)
            {
                Classes->Next();
                continue;
            }

            rv = is_Class->QueryInterface(NS_GET_IID(nsISupportsID),
                                          (void **)&ClassIDHolder);
            if(!NS_FAILED(rv))
            {
                nsID* class_id;
                if(NS_SUCCEEDED(ClassIDHolder->GetData(&class_id)))
                {
                    char* class_name;
                    if(nsnull != (class_name = class_id->ToString()))
                    {
                       RealizeClass(cx, obj, class_name, wrapper,
                                    arbitrary, JS_TRUE);
                       delete [] class_name;
                    }
                    nsAllocator::Free(class_id);
                }
                NS_RELEASE(ClassIDHolder);
            }
            else
                break;

            NS_RELEASE(is_Class);
            Classes->Next();

        } while(!NS_FAILED(Classes->IsDone()));
    }
    NS_RELEASE(Classes);
}

void
nsXPCClassesByID::RealizeClass(JSContext *cx, JSObject *obj,
                               const char *class_name,
                               nsIXPConnectWrappedNative* wrapper,
                               nsIXPCScriptable* arbitrary,
                               JSBool knownToBeRegistered)
{
    jsval prop;

    if(!JS_LookupProperty(cx, obj, class_name, &prop) ||
       JSVAL_IS_PRIMITIVE(prop))
    {
        jsid id;
        JSString *jstrid;

        if(nsnull != (jstrid = JS_InternString(cx, class_name)) &&
           JS_ValueToId(cx, STRING_TO_JSVAL(jstrid), &id))
        {
            CacheDynaProp(cx, obj, id, wrapper, arbitrary, knownToBeRegistered);
        }
    }
}

static PRBool
IsCanonicalFormOfRegisteredCLSID(const char* str)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if(NS_FAILED(rv))
        return PR_FALSE;

    char* copy = (char*)nsAllocator::Clone(str, (strlen(str)+1)*sizeof(char));
    if(!copy)
        return PR_FALSE;

    nsID id;
    PRBool idIsOK = id.Parse(copy);
    nsAllocator::Free(copy);
    if(!idIsOK)
        return PR_FALSE;

    PRBool registered;
    if(NS_SUCCEEDED(compMgr->IsRegistered(id, &registered)))
        return registered;
    return PR_FALSE;
}

void
nsXPCClassesByID::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                                nsIXPConnectWrappedNative* wrapper,
                                nsIXPCScriptable* arbitrary,
                                JSBool knownToBeRegistered)
{
    jsval idval;
    const char* property_name = nsnull;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != nsnull &&
       property_name[0] == '{') // we only allow canonical CLSIDs here
    {
        // in this case we are responsible for verifying that the
        // component manager has this CLSID registered.
        if(!knownToBeRegistered)
            if(!IsCanonicalFormOfRegisteredCLSID(property_name))
                return;

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
                                               nsnull, &retval);
                    }
                    NS_RELEASE(nsid_wrapper);
                }
                else
                {
                    NS_RELEASE(nsid);
                }
                NS_RELEASE(xpc);
            }
        }
    }
}

/***************************************************************************/

// Currently the possible results do not change at runtime, so they are only
// cached once (unlike ProgIDs, CLSIDs, and IIDs)

class nsXPCResults : public nsIXPCResults, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCRESULTS
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCResults();
    virtual ~nsXPCResults();

private:
    void FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary);

private:
    JSBool mCacheFilled;
};

nsXPCResults::nsXPCResults()
    :   mCacheFilled(JS_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPCResults::~nsXPCResults()
{
    // empty
}

void
nsXPCResults::FillCache(JSContext *cx, JSObject *obj,
                        nsIXPConnectWrappedNative *wrapper,
                        nsIXPCScriptable *arbitrary)
{
    nsresult rv;
    const char* name;
    void* iterp = nsnull;

    while(nsXPCException::IterateNSResults(&rv, &name, nsnull, &iterp))
    {
        jsid id;
        JSString *jstrid;
        jsval val;
        JSBool retval;

        if(!(jstrid = JS_InternString(cx, name)) ||
           !JS_ValueToId(cx, STRING_TO_JSVAL(jstrid), &id) ||
           !JS_NewNumberValue(cx, (jsdouble)rv, &val) ||
           NS_FAILED(arbitrary->SetProperty(cx, obj, id, &val, wrapper, nsnull, &retval)) ||
           !retval)
        {
            JS_ReportOutOfMemory(cx);
            return;
        }
    }

    mCacheFilled = JS_TRUE;
    return;
}

NS_IMPL_ISUPPORTS2(nsXPCResults, nsIXPCResults, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCResults)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCResults)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCResults)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCResults)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCResults)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCResults)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCResults)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCResults)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCResults)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsXPCResults)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCResults)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCResults)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCResults)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCResults)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCResults)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCResults)

NS_IMETHODIMP
nsXPCResults::GetFlags(JSContext *cx, JSObject *obj,
                       nsIXPConnectWrappedNative* wrapper,
                       JSUint32* flagsp,
                       nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCResults::LookupProperty(JSContext *cx, JSObject *obj,
                             jsid id,
                             JSObject **objp, JSProperty **propp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
    if(!mCacheFilled)
        FillCache(cx, obj, wrapper, arbitrary);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     nsnull, retval);
}

NS_IMETHODIMP
nsXPCResults::GetProperty(JSContext *cx, JSObject *obj,
                          jsid id, jsval *vp,
                          nsIXPConnectWrappedNative* wrapper,
                          nsIXPCScriptable* arbitrary,
                          JSBool* retval)
{
    if(!mCacheFilled)
        FillCache(cx, obj, wrapper, arbitrary);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
}


NS_IMETHODIMP
nsXPCResults::Enumerate(JSContext *cx, JSObject *obj,
                        JSIterateOp enum_op,
                        jsval *statep, jsid *idp,
                        nsIXPConnectWrappedNative *wrapper,
                        nsIXPCScriptable *arbitrary,
                        JSBool *retval)
{
    if(enum_op == JSENUMERATE_INIT && !mCacheFilled)
        FillCache(cx, obj, wrapper, arbitrary);

    return arbitrary->Enumerate(cx, obj, enum_op, statep, idp, wrapper,
                                arbitrary, retval);
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsXPCComponents, nsIXPCComponents, nsIXPCScriptable)

nsXPCComponents::nsXPCComponents()
    :   mInterfaces(nsnull),
        mClasses(nsnull),
        mClassesByID(nsnull),
        mResults(nsnull),
        mCreating(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents::~nsXPCComponents()
{
    NS_IF_RELEASE(mInterfaces);
    NS_IF_RELEASE(mClasses);
    NS_IF_RELEASE(mClassesByID);
    NS_IF_RELEASE(mResults);
}

#define XPC_IMPL_GET_OBJ_METHOD(_n) \
NS_IMETHODIMP nsXPCComponents::Get##_n(nsIXPC##_n * *a##_n) { \
    NS_ENSURE_ARG_POINTER(a##_n); \
    if(!m##_n) { \
        if(!(m##_n = new nsXPC##_n())) { \
            *a##_n = nsnull; \
            return NS_ERROR_OUT_OF_MEMORY; \
        } \
        NS_ADDREF(m##_n); \
    } \
    NS_ADDREF(m##_n); \
    *a##_n = m##_n; \
    return NS_OK; \
}

XPC_IMPL_GET_OBJ_METHOD(Interfaces);
XPC_IMPL_GET_OBJ_METHOD(Classes);
XPC_IMPL_GET_OBJ_METHOD(ClassesByID);
XPC_IMPL_GET_OBJ_METHOD(Results);

#undef XPC_IMPL_GET_OBJ_METHOD

NS_IMETHODIMP
nsXPCComponents::GetStack(nsIJSStackFrameLocation * *aStack)
{
    return nsXPConnect::GetXPConnect()->GetCurrentJSStack(aStack);
}

/**********************************************/

// XPC_IMPLEMENT_FORWARD_CREATE(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents)
// XPC_IMPLEMENT_FORWARD_SETPROPERTY(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents)

/*
* Arbitrary properties are not generally Settable. However, during creation
* we want to use JS to attach stuff, so we temporarily enable SetProperty
* by setting the mCreating flag.
*/

NS_IMETHODIMP
nsXPCComponents::Create(JSContext *cx, JSObject *obj,
                        nsIXPConnectWrappedNative* wrapper,
                        nsIXPCScriptable* arbitrary)
{
    // NOTE: this script is evaluated where the Components object is the
    // current 'this'.

    static const char source[] =
        "this.ID = function(str) {\n"
        "  var clazz = Components.classes['nsID'];\n"
        "  var iface = Components.interfaces['nsIJSID'];\n"
        "  var bad = \"Components.ID requires a nsID as input; e.g. \'{c9fc3520-4f93-11d3-9894-006008962422}\'\";\n"
        "  if(arguments.length != 1)\n"
        "    throw bad;\n"
        "  /* let's see if our param is an nsID object */\n"
        "  try{\n"
        "    if((str instanceof Object) &&\n"
        "      typeof(str.QueryInterface) != 'undefined') {\n"
        "      var other_id = str.QueryInterface(iface);\n"
        "      str = other_id.number;\n"
        "    }\n"
        "  } catch(e) {}\n"
        "  var id = clazz.createInstance(iface);\n"
        "  try{\n"
        "    id.initialize(str);\n"
        "  } catch(e) {\n"
        "    throw bad;\n"
        "  }\n"
        "  return id;\n"
        "};\n"
        "\n"
        "this.ID.toString = function() {\n"
        "  return '\\nfunction ID() {\\n    [JavaScript code]\\n}\\n';\n"
        "};\n"
        "\n"
        "this.Exception = function(str, code, stack, data) {\n"
        "  var clazz = Components.classes['nsXPCException'];\n"
        "  var iface = Components.interfaces['nsIXPCException'];\n"
        "  var exception = clazz.createInstance(iface);\n"
        "  /* all fall through */\n"
        "  switch(arguments.length){\n"
        "    case 0: str = 'exception';\n"
        "    case 1: code = Components.results.NS_ERROR_FAILURE;\n"
        "    case 2: stack = Components.stack.caller;\n"
        "    case 3: data = null;\n"
        "  }\n"
        "  exception.initialize(str, code, null, stack, data);\n"
        "  return exception;\n"
        "};\n"
        "\n"
        "this.Exception.toString = function() {\n"
        "  return '\\nfunction Exception() {\\n    [JavaScript code]\\n}\\n';\n"
        "};\n"
        ;

    jsval ignored;
    mCreating = PR_TRUE;
    JS_EvaluateScript(cx, obj, source, sizeof(source) - sizeof(char),
                      "Components_init_script", 1, &ignored);
    mCreating = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::GetProperty(JSContext *cx, JSObject *obj,
                             jsid id, jsval *vp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    if(xpcc)
    {
        PRBool doResult = JS_FALSE;
        nsresult res;
        if(xpcc->GetStringID(XPCContext::IDX_LAST_RESULT) == id)
        {
            res = xpcc->GetLastResult();
            doResult = JS_TRUE;
        }
        else if(xpcc->GetStringID(XPCContext::IDX_RETURN_CODE) == id)
        {
            res = xpcc->GetPendingResult();
            doResult = JS_TRUE;
        }

        if(doResult)
        {
            if(!JS_NewNumberValue(cx, (jsdouble) res, vp))
            {
                JS_ReportOutOfMemory(cx);
                return NS_ERROR_OUT_OF_MEMORY;
            }
            *retval = JS_TRUE;
            return NS_OK;
        }
    }
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
}

NS_IMETHODIMP
nsXPCComponents::SetProperty(JSContext *cx, JSObject *obj,
                             jsid id, jsval *vp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    if(xpcc)
    {
        if(xpcc->GetStringID(XPCContext::IDX_RETURN_CODE) == id)
        {
            nsresult rv;
            if(JS_ValueToECMAUint32(cx, *vp, (uint32*)&rv))
            {
                xpcc->SetPendingResult(rv);
                xpcc->SetLastResult(rv);
            }
            // XXX even if it fails we are silent?
            *retval = JS_TRUE;
            return NS_OK;
        }
    }

    if(mCreating)
        return arbitrary->SetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
    *retval = JS_TRUE;
    return NS_OK;
}

