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

/* The "Components" xpcom objects for JavaScript. */

#include "xpcprivate.h"

/***************************************************************************/
// stuff used by all

static nsresult ThrowAndFail(uintN errNum, JSContext* cx, JSBool* retval)
{
    nsXPConnect::GetJSThrower()->ThrowException(errNum, cx);
    *retval = JS_FALSE;
    return NS_OK;
}        

static JSBool
JSValIsInterfaceOfType(JSContext *cx, jsval v, REFNSIID iid)
{
    nsCOMPtr<nsIXPConnect> xpc;
    nsCOMPtr<nsIXPConnectWrappedNative> wn;
    nsCOMPtr<nsISupports> sup;
    nsISupports* iface;
    if(!JSVAL_IS_PRIMITIVE(v) &&
       nsnull != (xpc = dont_AddRef(
           NS_STATIC_CAST(nsIXPConnect*,nsXPConnect::GetXPConnect()))) &&
       NS_SUCCEEDED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(v),
                            getter_AddRefs(wn))) && wn &&
       NS_SUCCEEDED(wn->GetNative(getter_AddRefs(sup))) && sup &&
       NS_SUCCEEDED(sup->QueryInterface(iid, (void**)&iface)) && iface)
    {
        NS_RELEASE(iface);
        return JS_TRUE;
    }
    return JS_FALSE;
}

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

/***************************************************************************/

class nsXPCComponents_Interfaces : public nsIXPCComponents_Interfaces, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_INTERFACES
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCComponents_Interfaces();
    virtual ~nsXPCComponents_Interfaces();

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

nsXPCComponents_Interfaces::nsXPCComponents_Interfaces()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Interfaces::~nsXPCComponents_Interfaces()
{
    // empty
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_Interfaces, nsIXPCComponents_Interfaces, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCComponents_Interfaces)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_Interfaces)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_Interfaces)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_Interfaces, NS_ERROR_FAILURE)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_Interfaces)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_Interfaces)

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetFlags(JSContext *cx, JSObject *obj,
                          nsIXPConnectWrappedNative* wrapper,
                          JSUint32* flagsp,
                          nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::LookupProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Interfaces::GetProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Interfaces::Enumerate(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Interfaces::FillCache(JSContext *cx, JSObject *obj,
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
            QueryInterface(NS_GET_IID(nsIInterfaceInfo),
                           (void **)&Interface);

        if(!NS_FAILED(rv))
        {
            char *interface_name;
            Interface->GetName(&interface_name);
            RealizeInterface(cx, obj, interface_name, wrapper, arbitrary);
            nsMemory::Free(interface_name);
            NS_RELEASE(Interface);
        }
        else
        {
            /* that would be odd */
        }

        NS_RELEASE(is_Interface);
        Interfaces->Next();

    } while (NS_COMFALSE == Interfaces->IsDone());

done:
    NS_IF_RELEASE(Interfaces);
    NS_IF_RELEASE(iim);
}

void
nsXPCComponents_Interfaces::RealizeInterface(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Interfaces::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                               nsIXPConnectWrappedNative* wrapper,
                               nsIXPCScriptable* arbitrary)
{
    jsval idval;
    const char* property_name = nsnull;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != nsnull &&
       property_name[0] != '{') // we only allow interfaces by name here
    {
        nsCOMPtr<nsIJSIID> nsid = 
            dont_AddRef(NS_STATIC_CAST(nsIJSIID*,nsJSIID::NewID(property_name)));
        if(nsid)
        {
            nsCOMPtr<nsXPConnect> xpc = dont_AddRef(nsXPConnect::GetXPConnect());
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                NS_STATIC_CAST(nsIJSIID*,nsid),
                                                NS_GET_IID(nsIJSIID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        JSBool retval;
                        jsval val = OBJECT_TO_JSVAL(idobj);
                        arbitrary->SetProperty(cx, obj, id, &val, wrapper,
                                               nsnull, &retval);
                    }
                }
            }
        }
    }
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_Classes : public nsIXPCComponents_Classes, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CLASSES
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCComponents_Classes();
    virtual ~nsXPCComponents_Classes();

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

nsXPCComponents_Classes::nsXPCComponents_Classes()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Classes::~nsXPCComponents_Classes()
{
    // empty
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_Classes, nsIXPCComponents_Classes, nsIXPCScriptable)

XPC_IMPLEMENT_FORWARD_CREATE(nsXPCComponents_Classes)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_Classes)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_Classes)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_Classes)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_Classes, NS_ERROR_FAILURE)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_Classes)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_Classes)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_Classes)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_Classes)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_Classes)


NS_IMETHODIMP
nsXPCComponents_Classes::GetFlags(JSContext *cx, JSObject *obj,
                       nsIXPConnectWrappedNative* wrapper,
                       JSUint32* flagsp,
                       nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}


NS_IMETHODIMP
nsXPCComponents_Classes::LookupProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Classes::GetProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Classes::Enumerate(JSContext *cx, JSObject *obj,
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

/* enumerate the known contractids, adding a property for each new one */
void
nsXPCComponents_Classes::FillCache(JSContext *cx, JSObject *obj,
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

    if(NS_FAILED(rv = compMgr->EnumerateContractIDs(&Classes)))
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
                    nsMemory::Free(class_name);
                }
                NS_RELEASE(ClassNameHolder);
            }
            else
                break;

            NS_RELEASE(is_Class);
            Classes->Next();

        } while (NS_COMFALSE == Classes->IsDone());
    }
    NS_RELEASE(Classes);
}

void
nsXPCComponents_Classes::RealizeClass(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Classes::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
                            nsIXPConnectWrappedNative* wrapper,
                            nsIXPCScriptable* arbitrary)
{
    jsval idval;
    const char* property_name = nsnull;

    if(JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
       (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != nsnull &&
       property_name[0] != '{') // we only allow contractids here
    {
        nsCOMPtr<nsIJSCID> nsid = 
            dont_AddRef(NS_STATIC_CAST(nsIJSCID*,nsJSCID::NewID(property_name)));
        if(nsid)
        {
            nsCOMPtr<nsXPConnect> xpc = dont_AddRef(nsXPConnect::GetXPConnect());
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                NS_STATIC_CAST(nsIJSCID*,nsid),
                                                NS_GET_IID(nsIJSCID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        JSBool retval;
                        jsval val = OBJECT_TO_JSVAL(idobj);
                        arbitrary->SetProperty(cx, obj, id, &val, wrapper,
                                               nsnull, &retval);
                    }
                }
            }
        }
    }
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_ClassesByID : public nsIXPCComponents_ClassesByID, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CLASSESBYID
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCComponents_ClassesByID();
    virtual ~nsXPCComponents_ClassesByID();

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

nsXPCComponents_ClassesByID::nsXPCComponents_ClassesByID()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_ClassesByID::~nsXPCComponents_ClassesByID()
{
    // empty
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_ClassesByID, nsIXPCComponents_ClassesByID, nsIXPCScriptable)

XPC_IMPLEMENT_FORWARD_CREATE(nsXPCComponents_ClassesByID)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_ClassesByID)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_ClassesByID)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_ClassesByID, NS_ERROR_FAILURE)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_ClassesByID)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_ClassesByID)


NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetFlags(JSContext *cx, JSObject *obj,
                           nsIXPConnectWrappedNative* wrapper,
                           JSUint32* flagsp,
                           nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::LookupProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_ClassesByID::GetProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_ClassesByID::Enumerate(JSContext *cx, JSObject *obj,
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

/* enumerate the known contractids, adding a property for each new one */
void
nsXPCComponents_ClassesByID::FillCache(JSContext *cx, JSObject *obj,
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
                    nsMemory::Free(class_id);
                }
                NS_RELEASE(ClassIDHolder);
            }
            else
                break;

            NS_RELEASE(is_Class);
            Classes->Next();

        } while (NS_COMFALSE == Classes->IsDone());
    }
    NS_RELEASE(Classes);
}

void
nsXPCComponents_ClassesByID::RealizeClass(JSContext *cx, JSObject *obj,
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

    char* copy = (char*)nsMemory::Clone(str, (strlen(str)+1)*sizeof(char));
    if(!copy)
        return PR_FALSE;

    nsID id;
    PRBool idIsOK = id.Parse(copy);
    nsMemory::Free(copy);
    if(!idIsOK)
        return PR_FALSE;

    PRBool registered;
    if(NS_SUCCEEDED(compMgr->IsRegistered(id, &registered)))
        return registered;
    return PR_FALSE;
}

void
nsXPCComponents_ClassesByID::CacheDynaProp(JSContext *cx, JSObject *obj, jsid id,
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

        nsCOMPtr<nsIJSCID> nsid = 
            dont_AddRef(NS_STATIC_CAST(nsIJSCID*,nsJSCID::NewID(property_name)));
        if(nsid)
        {
            nsCOMPtr<nsXPConnect> xpc = dont_AddRef(nsXPConnect::GetXPConnect());
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                NS_STATIC_CAST(nsIJSCID*,nsid),
                                                NS_GET_IID(nsIJSCID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        JSBool retval;
                        jsval val = OBJECT_TO_JSVAL(idobj);
                        arbitrary->SetProperty(cx, obj, id, &val, wrapper,
                                               nsnull, &retval);
                    }
                }
            }
        }
    }
}

/***************************************************************************/

// Currently the possible results do not change at runtime, so they are only
// cached once (unlike ContractIDs, CLSIDs, and IIDs)

class nsXPCComponents_Results : public nsIXPCComponents_Results, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_RESULTS
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCComponents_Results();
    virtual ~nsXPCComponents_Results();

private:
    void FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary);

private:
    JSBool mCacheFilled;
};

nsXPCComponents_Results::nsXPCComponents_Results()
    :   mCacheFilled(JS_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Results::~nsXPCComponents_Results()
{
    // empty
}

void
nsXPCComponents_Results::FillCache(JSContext *cx, JSObject *obj,
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

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_Results, nsIXPCComponents_Results, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCComponents_Results)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_Results)
// XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_Results)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_Results)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_Results, NS_ERROR_FAILURE)
// XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_Results)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_Results)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_Results)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_Results)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_Results)

NS_IMETHODIMP
nsXPCComponents_Results::GetFlags(JSContext *cx, JSObject *obj,
                       nsIXPConnectWrappedNative* wrapper,
                       JSUint32* flagsp,
                       nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::LookupProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Results::GetProperty(JSContext *cx, JSObject *obj,
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
nsXPCComponents_Results::Enumerate(JSContext *cx, JSObject *obj,
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
// JavaScript Constructor for nsIJSID objects (Components.ID)

class nsXPCComponents_ID : public nsIXPCComponents_ID, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_ID
    XPC_DECLARE_IXPCSCRIPTABLE


public:
    nsXPCComponents_ID();
    virtual ~nsXPCComponents_ID();

private:
    NS_METHOD CallOrConstruct(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv,
                              jsval *rval,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval);
};

nsXPCComponents_ID::nsXPCComponents_ID()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_ID::~nsXPCComponents_ID()
{
    // empty
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_ID, nsIXPCComponents_ID, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCComponents_ID)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_ID)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_ID)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_ID)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_ID)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_ID)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_ID)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_ID)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_ID)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_ID, NS_ERROR_FAILURE)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_ID)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_ID)
// XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_ID)
// XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_ID)
// XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_ID)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_ID)

NS_IMETHODIMP
nsXPCComponents_ID::Call(JSContext *cx, JSObject *obj,
                         uintN argc, jsval *argv,
                         jsval *rval,
                         nsIXPConnectWrappedNative* wrapper,
                         nsIXPCScriptable* arbitrary,
                         JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_IMETHODIMP
nsXPCComponents_ID::Construct(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv,
                              jsval *rval,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_METHOD
nsXPCComponents_ID::CallOrConstruct(JSContext *cx, JSObject *obj,
                                    uintN argc, jsval *argv,
                                    jsval *rval,
                                    nsIXPConnectWrappedNative* wrapper,
                                    nsIXPCScriptable* arbitrary,
                                    JSBool* retval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);

    // make sure we have at least one arg
    
    if(!argc)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, retval);
    
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    if(!xpcc)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);

    // Do the security check if necessary
    
    nsIXPCSecurityManager* sm =
            xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, nsJSID::GetCID())))
    {
        // the security manager vetoed. It should have set an exception.
        *retval = JS_FALSE;
        return NS_OK;
    }
    
    // convert the first argument into a string and see if it looks like an id

    JSString* jsstr;
    const char* str;
    nsID id;

    if(!(jsstr = JS_ValueToString(cx, argv[0])) ||
       !(str = JS_GetStringBytes(jsstr)) ||
       ! id.Parse(str))
    {
        return ThrowAndFail(NS_ERROR_XPC_BAD_ID_STRING, cx, retval);
    }

    // make the new object and return it.

    JSObject* newobj = xpc_NewIDObject(cx, obj, id);

    if(rval)
        *rval = OBJECT_TO_JSVAL(newobj);
    
    *retval = JS_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::HasInstance(JSContext *cx, JSObject *obj,                    
                                jsval v, JSBool *bp,                             
                                nsIXPConnectWrappedNative* wrapper,              
                                nsIXPCScriptable* arbitrary,                     
                                JSBool* retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, v, NS_GET_IID(nsIJSID));
    *retval = JS_TRUE;
    return NS_OK;
}                            

/***************************************************************************/
// JavaScript Constructor for nsIXPCException objects (Components.Exception)

class nsXPCComponents_Exception : public nsIXPCComponents_Exception, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_EXCEPTION
    XPC_DECLARE_IXPCSCRIPTABLE


public:
    nsXPCComponents_Exception();
    virtual ~nsXPCComponents_Exception();

private:
    NS_METHOD CallOrConstruct(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv,
                              jsval *rval,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval);
};

nsXPCComponents_Exception::nsXPCComponents_Exception()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Exception::~nsXPCComponents_Exception()
{
    // empty
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_Exception, nsIXPCComponents_Exception, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCComponents_Exception)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_Exception)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_Exception)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_Exception)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_Exception)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_Exception)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_Exception)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_Exception)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_Exception)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_Exception, NS_ERROR_FAILURE)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_Exception)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_Exception)
// XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_Exception)
// XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_Exception)
// XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_Exception)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_Exception)

NS_IMETHODIMP
nsXPCComponents_Exception::Call(JSContext *cx, JSObject *obj,
                                uintN argc, jsval *argv,
                                jsval *rval,
                                nsIXPConnectWrappedNative* wrapper,
                                nsIXPCScriptable* arbitrary,
                                JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_IMETHODIMP
nsXPCComponents_Exception::Construct(JSContext *cx, JSObject *obj,
                                     uintN argc, jsval *argv,
                                     jsval *rval,
                                     nsIXPConnectWrappedNative* wrapper,
                                     nsIXPCScriptable* arbitrary,
                                     JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_METHOD
nsXPCComponents_Exception::CallOrConstruct(JSContext *cx, JSObject *obj,
                                           uintN argc, jsval *argv,
                                           jsval *rval,
                                           nsIXPConnectWrappedNative* wrapper,
                                           nsIXPCScriptable* arbitrary,
                                           JSBool* retval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);

    nsCOMPtr<nsIXPConnect> xpc = 
        dont_AddRef(NS_STATIC_CAST(nsIXPConnect*,nsXPConnect::GetXPConnect()));
    XPCContext* xpcc = nsXPConnect::GetContext(cx);

    if(!xpc || !xpcc)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);

    // Do the security check if necessary
    
    nsIXPCSecurityManager* sm =
            xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, nsXPCException::GetCID())))
    {
        // the security manager vetoed. It should have set an exception.
        *retval = JS_FALSE;
        return NS_OK;
    }

    // initialization params for the exception object we will create
    const char*                       eMsg = "exception";
    nsresult                          eResult = NS_ERROR_FAILURE;
    nsCOMPtr<nsIJSStackFrameLocation> eStack;
    nsCOMPtr<nsISupports>             eData;

    // all params are optional - grab any passed in
    switch(argc)
    {
        default:    // more than 4 - ignore extra
            // ...fall through...
        case 4:     // argv[3] is object for eData
            if(JSVAL_IS_NULL(argv[3]))
            {
                // do nothing, leave eData as null     
            }
            else
            {
                if(JSVAL_IS_PRIMITIVE(argv[3]) ||
                   NS_FAILED(xpc->WrapJS(cx, JSVAL_TO_OBJECT(argv[3]),
                                         NS_GET_IID(nsISupports),
                                         (void**)getter_AddRefs(eData))))
                    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
            }
            // ...fall through...
        case 3:     // argv[2] is object for eStack
            if(JSVAL_IS_NULL(argv[2]))
            {
                // do nothing, leave eStack as null     
            }
            else
            {
                if(JSVAL_IS_PRIMITIVE(argv[2]) ||
                   NS_FAILED(xpc->WrapJS(cx, JSVAL_TO_OBJECT(argv[2]),
                                         NS_GET_IID(nsIJSStackFrameLocation),
                                         (void**)getter_AddRefs(eStack))))
                    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
            }
            // fall through...
        case 2:     // argv[1] is nsresult for eResult
            if(!JS_ValueToECMAInt32(cx, argv[1], (int32*) &eResult))
                return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
            // ...fall through...
        case 1:     // argv[0] is string for eMsg
            {
                JSString* str = JS_ValueToString(cx, argv[0]);
                if(!str || !(eMsg = JS_GetStringBytes(str)))
                    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
            }
            // ...fall through...
        case 0: // this case required so that 'default' does not include zero.
            ;   // -- do nothing --
    }

    nsCOMPtr<nsIXPCException> e = 
        dont_AddRef(
            NS_STATIC_CAST(nsIXPCException*,
                nsXPCException::NewException(eMsg, eResult, eStack, eData)));
    if(!e)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    JSObject* newObj = nsnull;

    if(NS_FAILED(xpc->WrapNative(cx, obj, e, NS_GET_IID(nsIXPCException), 
                 getter_AddRefs(holder))) || !holder ||
       NS_FAILED(holder->GetJSObject(&newObj)) || !newObj)
    {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, retval);
    }

    if(rval)
        *rval = OBJECT_TO_JSVAL(newObj);
    
    *retval = JS_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::HasInstance(JSContext *cx, JSObject *obj,                    
                                       jsval v, JSBool *bp,                             
                                       nsIXPConnectWrappedNative* wrapper,              
                                       nsIXPCScriptable* arbitrary,                     
                                       JSBool* retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, v, NS_GET_IID(nsIXPCException));
    *retval = JS_TRUE;
    return NS_OK;
}                            

/***************************************************************************/
// This class is for the thing returned by "new Component.Constructor".

// XXXjband we use this CID for security check, but security system can't see
// it since it has no registed factory. Security really kicks in when we try 
// to build a wrapper around an instance.

// {B4A95150-E25A-11d3-8F61-0010A4E73D9A}
#define NS_XPCCONSTRUCTOR_CID  \
{ 0xb4a95150, 0xe25a, 0x11d3, \
    { 0x8f, 0x61, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a } }

class nsXPCConstructor : public nsIXPCConstructor, public nsIXPCScriptable
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XPCCONSTRUCTOR_CID)
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCONSTRUCTOR
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCConstructor(); // not implemented
    nsXPCConstructor(nsIJSCID* aClassID,
                     nsIJSIID* aInterfaceID,
                     const char* aInitializer);
    virtual ~nsXPCConstructor();

private:
    NS_METHOD CallOrConstruct(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv,
                              jsval *rval,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval);
private:
    nsIJSCID* mClassID;
    nsIJSIID* mInterfaceID;
    char*     mInitializer;
};

nsXPCConstructor::nsXPCConstructor(nsIJSCID* aClassID,
                                   nsIJSIID* aInterfaceID,
                                   const char* aInitializer)
{
    NS_INIT_ISUPPORTS();
    NS_IF_ADDREF(mClassID = aClassID);
    NS_IF_ADDREF(mInterfaceID = aInterfaceID);
    mInitializer = aInitializer ? 
        (char*) nsMemory::Clone(aInitializer, strlen(aInitializer)+1) :
        nsnull;
}

nsXPCConstructor::~nsXPCConstructor()
{
    NS_IF_RELEASE(mClassID);
    NS_IF_RELEASE(mInterfaceID);
    if(mInitializer) 
        nsMemory::Free(mInitializer);
}

/* readonly attribute nsIJSCID classID; */
NS_IMETHODIMP 
nsXPCConstructor::GetClassID(nsIJSCID * *aClassID)
{
    NS_IF_ADDREF(*aClassID = mClassID);
    return NS_OK;        
}        

/* readonly attribute nsIJSIID interfaceID; */
NS_IMETHODIMP 
nsXPCConstructor::GetInterfaceID(nsIJSIID * *aInterfaceID)
{
    NS_IF_ADDREF(*aInterfaceID = mInterfaceID);
    return NS_OK;        
}        

/* readonly attribute string initializer; */
NS_IMETHODIMP 
nsXPCConstructor::GetInitializer(char * *aInitializer)
{
    XPC_STRING_GETTER_BODY(aInitializer, mInitializer);
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCConstructor, nsIXPCConstructor, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCConstructor)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCConstructor)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCConstructor)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCConstructor)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCConstructor)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCConstructor)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCConstructor)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCConstructor)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCConstructor)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCConstructor, NS_ERROR_FAILURE)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCConstructor)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCConstructor)
// XPC_IMPLEMENT_IGNORE_CALL(nsXPCConstructor)
// XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCConstructor)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCConstructor)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCConstructor)


NS_IMETHODIMP
nsXPCConstructor::Call(JSContext *cx, JSObject *obj,
                       uintN argc, jsval *argv,
                       jsval *rval,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary,
                       JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_IMETHODIMP
nsXPCConstructor::Construct(JSContext *cx, JSObject *obj,
                            uintN argc, jsval *argv,
                            jsval *rval,
                            nsIXPConnectWrappedNative* wrapper,
                            nsIXPCScriptable* arbitrary,
                            JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_METHOD
nsXPCConstructor::CallOrConstruct(JSContext *cx, JSObject *obj,
                                  uintN argc, jsval *argv,
                                  jsval *rval,
                                  nsIXPConnectWrappedNative* wrapper,
                                  nsIXPCScriptable* arbitrary,
                                  JSBool* retval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);

    nsCOMPtr<nsIXPConnect> xpc = 
        dont_AddRef(NS_STATIC_CAST(nsIXPConnect*,nsXPConnect::GetXPConnect()));
    if(!xpc)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);

    // security check not required because we are going to call through the
    // code which is reflected into JS which will do that for us later.

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    JSObject* cidObj;
    JSObject* iidObj;

    if(NS_FAILED(xpc->WrapNative(cx, obj, mClassID, NS_GET_IID(nsIJSCID), 
                 getter_AddRefs(holder))) || !holder ||
       NS_FAILED(holder->GetJSObject(&cidObj)) || !cidObj ||
       NS_FAILED(xpc->WrapNative(cx, obj, mInterfaceID, NS_GET_IID(nsIJSIID), 
                 getter_AddRefs(holder))) || !holder ||
       NS_FAILED(holder->GetJSObject(&iidObj)) || !iidObj)
    {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, retval);
    }
    
    jsval ctorArgs[1] = {OBJECT_TO_JSVAL(iidObj)};
    jsval val;

    if(!JS_CallFunctionName(cx, cidObj, "createInstance", 1, ctorArgs, &val) ||
       JSVAL_IS_PRIMITIVE(val))
    {
        // createInstance will have thrown an exception
        *retval = JS_FALSE;
        return NS_OK;
    }

    // root the result
    if(rval)
        *rval = val;

    // call initializer method if supplied
    if(mInitializer)
    {
        JSObject* newObj = JSVAL_TO_OBJECT(val);
        jsval fun;
        jsval ignored;

        // first check existence of function property for better error reporting
        if(!JS_GetProperty(cx, newObj, mInitializer, &fun) ||
           JSVAL_IS_PRIMITIVE(fun))
        {
            return ThrowAndFail(NS_ERROR_XPC_BAD_INITIALIZER_NAME, cx, retval);
        }

        if(!JS_CallFunctionValue(cx, newObj, fun, argc, argv, &ignored))
        {
            // function should have thrown an exception
            *retval = JS_FALSE;
            return NS_OK;
        }
    }
    
    *retval = JS_TRUE;
    return NS_OK;
}

/*******************************************************/
// JavaScript Constructor for nsIXPCConstructor objects (Components.Constructor)

class nsXPCComponents_Constructor : public nsIXPCComponents_Constructor, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CONSTRUCTOR
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCComponents_Constructor();
    virtual ~nsXPCComponents_Constructor();

private:
    NS_METHOD CallOrConstruct(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv,
                              jsval *rval,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval);
};

nsXPCComponents_Constructor::nsXPCComponents_Constructor()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Constructor::~nsXPCComponents_Constructor()
{
    // empty
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents_Constructor, nsIXPCComponents_Constructor, nsIXPCScriptable)

XPC_IMPLEMENT_IGNORE_CREATE(nsXPCComponents_Constructor)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents_Constructor)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents_Constructor)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents_Constructor)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents_Constructor)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsXPCComponents_Constructor)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsXPCComponents_Constructor)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents_Constructor)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents_Constructor)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents_Constructor, NS_ERROR_FAILURE)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents_Constructor)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents_Constructor)
// XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents_Constructor)
// XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents_Constructor)
// XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents_Constructor)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents_Constructor)

NS_IMETHODIMP
nsXPCComponents_Constructor::Call(JSContext *cx, JSObject *obj,
                                uintN argc, jsval *argv,
                                jsval *rval,
                                nsIXPConnectWrappedNative* wrapper,
                                nsIXPCScriptable* arbitrary,
                                JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_IMETHODIMP
nsXPCComponents_Constructor::Construct(JSContext *cx, JSObject *obj,
                                     uintN argc, jsval *argv,
                                     jsval *rval,
                                     nsIXPConnectWrappedNative* wrapper,
                                     nsIXPCScriptable* arbitrary,
                                     JSBool* retval)
{
    return CallOrConstruct(cx, obj, argc, argv, rval, wrapper, arbitrary, retval);
}

NS_METHOD
nsXPCComponents_Constructor::CallOrConstruct(JSContext *cx, JSObject *obj,
                                           uintN argc, jsval *argv,
                                           jsval *rval,
                                           nsIXPConnectWrappedNative* wrapper,
                                           nsIXPCScriptable* arbitrary,
                                           JSBool* retval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);

    // make sure we have at least one arg
    
    if(!argc)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, retval);

    // get the various other object pointers we need

    nsCOMPtr<nsIXPConnect> xpc = 
        dont_AddRef(NS_STATIC_CAST(nsIXPConnect*,nsXPConnect::GetXPConnect()));
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    nsXPCWrappedNativeScope* scope =  
        nsXPCWrappedNativeScope::FindInJSObjectScope(xpcc, obj);
    nsXPCComponents* comp;

    if(!xpc || !xpcc || !scope || !(comp = scope->GetComponents()))
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);

    // Do the security check if necessary
    
    nsIXPCSecurityManager* sm =
            xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, nsXPCConstructor::GetCID())))
    {
        // the security manager vetoed. It should have set an exception.
        *retval = JS_FALSE;
        return NS_OK;
    }

    // initialization params for the Constructor object we will create
    nsCOMPtr<nsIJSCID> cClassID;
    nsCOMPtr<nsIJSIID> cInterfaceID;
    const char*        cInitializer = nsnull;

    if(argc >= 3)
    {
        // argv[2] is an initializer function or property name
        JSString* str = JS_ValueToString(cx, argv[2]);
        if(!str || !(cInitializer = JS_GetStringBytes(str)))
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
    }

    if(argc >= 2)
    {
        // argv[1] is an iid name string
        // XXXjband support passing "Components.interfaces.foo"?

        nsCOMPtr<nsIXPCComponents_Interfaces> ifaces;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        JSObject* ifacesObj = nsnull;

        // we do the lookup by asking the Components.interfaces object
        // for the property with this name - i.e. we let its caching of these
        // nsIJSIID objects work for us.

        if(NS_FAILED(comp->GetInterfaces(getter_AddRefs(ifaces))) ||
           NS_FAILED(xpc->WrapNative(cx, obj, ifaces, 
                                     NS_GET_IID(nsIXPCComponents_Interfaces), 
                                     getter_AddRefs(holder))) || !holder ||
           NS_FAILED(holder->GetJSObject(&ifacesObj)) || !ifacesObj)
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);
        }

        JSString* str = JS_ValueToString(cx, argv[1]);
        if(!str)
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
        
        jsval val;
        if(!JS_GetProperty(cx, ifacesObj, JS_GetStringBytes(str), &val) ||
           JSVAL_IS_PRIMITIVE(val))
        {
            return ThrowAndFail(NS_ERROR_XPC_BAD_IID, cx, retval);
        }

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        nsCOMPtr<nsISupports> sup;
        if(NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                                getter_AddRefs(wn))) || !wn ||
           NS_FAILED(wn->GetNative(getter_AddRefs(sup))) || !sup ||
           !(cInterfaceID = do_QueryInterface(sup)))
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);
        }
    }
    else
    {
        cInterfaceID = 
            dont_AddRef(
                NS_STATIC_CAST(nsIJSIID*, nsJSIID::NewID("nsISupports")));
        if(!cInterfaceID)
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);
    }

    // a new scope to avoid warnings about shadowed names
    {
        // argv[0] is a contractid name string
        // XXXjband support passing "Components.classes.foo"?

        // we do the lookup by asking the Components.classes object
        // for the property with this name - i.e. we let its caching of these
        // nsIJSCID objects work for us.


        nsCOMPtr<nsIXPCComponents_Classes> classes;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        JSObject* classesObj = nsnull;

        if(NS_FAILED(comp->GetClasses(getter_AddRefs(classes))) ||
           NS_FAILED(xpc->WrapNative(cx, obj, classes, 
                                     NS_GET_IID(nsIXPCComponents_Classes), 
                                     getter_AddRefs(holder))) || !holder ||
           NS_FAILED(holder->GetJSObject(&classesObj)) || !classesObj)
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);
        }

        JSString* str = JS_ValueToString(cx, argv[0]);
        if(!str)
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, retval);
        
        jsval val;
        if(!JS_GetProperty(cx, classesObj, JS_GetStringBytes(str), &val) ||
           JSVAL_IS_PRIMITIVE(val))
        {
            return ThrowAndFail(NS_ERROR_XPC_BAD_CID, cx, retval);
        }

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        nsCOMPtr<nsISupports> sup;
        if(NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                                getter_AddRefs(wn))) || !wn ||
           NS_FAILED(wn->GetNative(getter_AddRefs(sup))) || !sup ||
           !(cClassID = do_QueryInterface(sup)))
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);
        }
    }

    nsCOMPtr<nsIXPCConstructor> ctor = 
        NS_STATIC_CAST(nsIXPCConstructor*, 
            new nsXPCConstructor(cClassID, cInterfaceID, cInitializer));
    if(!ctor)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, retval);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder2;
    JSObject* newObj = nsnull;

    if(NS_FAILED(xpc->WrapNative(cx, obj, ctor, NS_GET_IID(nsIXPCConstructor), 
                 getter_AddRefs(holder2))) || !holder2 ||
       NS_FAILED(holder2->GetJSObject(&newObj)) || !newObj)
    {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, retval);
    }

    if(rval)
        *rval = OBJECT_TO_JSVAL(newObj);
    
    *retval = JS_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::HasInstance(JSContext *cx, JSObject *obj,                    
                                       jsval v, JSBool *bp,                             
                                       nsIXPConnectWrappedNative* wrapper,              
                                       nsIXPCScriptable* arbitrary,                     
                                       JSBool* retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, v, NS_GET_IID(nsIXPCConstructor));
    *retval = JS_TRUE;
    return NS_OK;
}                            

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// XXXjband We ought to cache the wrapper in the object's slots rather than
// re-wrapping on demand

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCComponents, nsIXPCComponents, nsIXPCScriptable)

nsXPCComponents::nsXPCComponents()
    :   mInterfaces(nsnull),
        mClasses(nsnull),
        mClassesByID(nsnull),
        mResults(nsnull),
        mID(nsnull),
        mException(nsnull),
        mConstructor(nsnull),
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
    NS_IF_RELEASE(mID);
    NS_IF_RELEASE(mException);
    NS_IF_RELEASE(mConstructor);
}

/*******************************************/
#define XPC_IMPL_GET_OBJ_METHOD(_n) \
NS_IMETHODIMP nsXPCComponents::Get##_n(nsIXPCComponents_##_n * *a##_n) { \
    NS_ENSURE_ARG_POINTER(a##_n); \
    if(!m##_n) { \
        if(!(m##_n = new nsXPCComponents_##_n())) { \
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
XPC_IMPL_GET_OBJ_METHOD(ID);
XPC_IMPL_GET_OBJ_METHOD(Exception);
XPC_IMPL_GET_OBJ_METHOD(Constructor);

#undef XPC_IMPL_GET_OBJ_METHOD
/*******************************************/

NS_IMETHODIMP
nsXPCComponents::GetStack(nsIJSStackFrameLocation * *aStack)
{
    nsresult rv;
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(!xpc)
        return NS_ERROR_FAILURE;
    rv = xpc->GetCurrentJSStack(aStack);
    NS_RELEASE(xpc);
    return rv;
}

NS_IMETHODIMP
nsXPCComponents::GetManager(nsIComponentManager * *aManager)
{
    NS_ENSURE_ARG_POINTER(aManager);

    nsIComponentManager* cm;
    if(NS_FAILED(NS_GetGlobalComponentManager(&cm)))
    {
        *aManager = nsnull;
        return NS_ERROR_FAILURE;    
    }
    NS_IF_ADDREF(cm);
    *aManager = cm;
                
    return NS_OK;
}

/**********************************************/

XPC_IMPLEMENT_FORWARD_CREATE(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsXPCComponents)
// XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsXPCComponents)
// XPC_IMPLEMENT_FORWARD_SETPROPERTY(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsXPCComponents)
XPC_IMPLEMENT_FAIL_DEFAULTVALUE(nsXPCComponents, NS_ERROR_FAILURE)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_CALL(nsXPCComponents)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsXPCComponents)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsXPCComponents)

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
        XPCJSRuntime* rt = xpcc->GetRuntime();
        if(rt->GetStringID(XPCJSRuntime::IDX_LAST_RESULT) == id)
        {
            res = xpcc->GetLastResult();
            doResult = JS_TRUE;
        }
        else if(rt->GetStringID(XPCJSRuntime::IDX_RETURN_CODE) == id)
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
        XPCJSRuntime* rt = xpcc->GetRuntime();
        if(rt->GetStringID(XPCJSRuntime::IDX_RETURN_CODE) == id)
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

// static 
JSBool
nsXPCComponents::AttachNewComponentsObject(XPCContext* xpcc, 
                                           JSObject* aGlobal)
{
    if(!xpcc || !aGlobal)
        return JS_FALSE;

    JSBool success = JS_FALSE;
    nsXPCComponents* components = nsnull;
    nsXPCWrappedNativeScope* scope = nsnull;
    nsXPCWrappedNative* wrapper = nsnull;

    components = new nsXPCComponents();
    if(components)
        NS_ADDREF(components);
    else
        goto done;        
    
    scope = new nsXPCWrappedNativeScope(xpcc, components);
    if(scope)
        NS_ADDREF(scope);
    else
        goto done;        

    if(!scope->IsValid())
        goto done;        

    // objects init'd OK. 

    wrapper = nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc, scope, aGlobal, 
                                NS_STATIC_CAST(nsIXPCComponents*,components),
                                NS_GET_IID(nsIXPCComponents), nsnull);
    if(wrapper)
    {
        JSObject* obj;
        jsid id = xpcc->GetRuntime()->GetStringID(XPCJSRuntime::IDX_COMPONENTS);
        success = NS_SUCCEEDED(wrapper->GetJSObject(&obj)) &&
                  OBJ_DEFINE_PROPERTY(xpcc->GetJSContext(),
                                      aGlobal, id, OBJECT_TO_JSVAL(obj),
                                      nsnull, nsnull,
                                      JSPROP_PERMANENT |
                                      JSPROP_READONLY |
                                      JSPROP_ENUMERATE,
                                      nsnull);
    }            
done:
    NS_IF_RELEASE(components);
    NS_IF_RELEASE(scope);
    NS_IF_RELEASE(wrapper);
    return success;
}

