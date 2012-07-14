/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef XPC_MAP_CLASSNAME
#error "Must #define XPC_MAP_CLASSNAME before #including xpc_map_end.h"
#endif

#ifndef XPC_MAP_QUOTED_CLASSNAME
#error "Must #define XPC_MAP_QUOTED_CLASSNAME before #including xpc_map_end.h"
#endif

/**************************************************************/

NS_IMETHODIMP XPC_MAP_CLASSNAME::GetClassName(char * *aClassName)
{
    static const char sName[] = XPC_MAP_QUOTED_CLASSNAME;
    *aClassName = (char*) nsMemory::Clone(sName, sizeof(sName));
    return NS_OK;
}

/**************************************************************/

// virtual
PRUint32
XPC_MAP_CLASSNAME::GetScriptableFlags()
{
    return
#ifdef XPC_MAP_WANT_PRECREATE
    nsIXPCScriptable::WANT_PRECREATE |
#endif
#ifdef XPC_MAP_WANT_CREATE
    nsIXPCScriptable::WANT_CREATE |
#endif
#ifdef XPC_MAP_WANT_POSTCREATE
    nsIXPCScriptable::WANT_POSTCREATE |
#endif
#ifdef XPC_MAP_WANT_ADDPROPERTY
    nsIXPCScriptable::WANT_ADDPROPERTY |
#endif
#ifdef XPC_MAP_WANT_DELPROPERTY
    nsIXPCScriptable::WANT_DELPROPERTY |
#endif
#ifdef XPC_MAP_WANT_GETPROPERTY
    nsIXPCScriptable::WANT_GETPROPERTY |
#endif
#ifdef XPC_MAP_WANT_SETPROPERTY
    nsIXPCScriptable::WANT_SETPROPERTY |
#endif
#ifdef XPC_MAP_WANT_ENUMERATE
    nsIXPCScriptable::WANT_ENUMERATE |
#endif
#ifdef XPC_MAP_WANT_NEWENUMERATE
    nsIXPCScriptable::WANT_NEWENUMERATE |
#endif
#ifdef XPC_MAP_WANT_NEWRESOLVE
    nsIXPCScriptable::WANT_NEWRESOLVE |
#endif
#ifdef XPC_MAP_WANT_CONVERT
    nsIXPCScriptable::WANT_CONVERT |
#endif
#ifdef XPC_MAP_WANT_FINALIZE
    nsIXPCScriptable::WANT_FINALIZE |
#endif
#ifdef XPC_MAP_WANT_CHECKACCESS
    nsIXPCScriptable::WANT_CHECKACCESS |
#endif
#ifdef XPC_MAP_WANT_CALL
    nsIXPCScriptable::WANT_CALL |
#endif
#ifdef XPC_MAP_WANT_CONSTRUCT
    nsIXPCScriptable::WANT_CONSTRUCT |
#endif
#ifdef XPC_MAP_WANT_HASINSTANCE
    nsIXPCScriptable::WANT_HASINSTANCE |
#endif
#ifdef XPC_MAP_WANT_EQUALITY
    nsIXPCScriptable::WANT_EQUALITY |
#endif
#ifdef XPC_MAP_WANT_OUTER_OBJECT
    nsIXPCScriptable::WANT_OUTER_OBJECT |
#endif
#ifdef XPC_MAP_FLAGS
    XPC_MAP_FLAGS |
#endif
    0;
}

/**************************************************************/

#ifndef XPC_MAP_WANT_PRECREATE
/* void preCreate (in nsISupports nativeObj, in JSContextPtr cx, in JSObjectPtr globalObj, out JSObjectPtr parentObj); */
NS_IMETHODIMP XPC_MAP_CLASSNAME::PreCreate(nsISupports *nativeObj, JSContext * cx, JSObject * globalObj, JSObject * *parentObj)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_CREATE
NS_IMETHODIMP XPC_MAP_CLASSNAME::Create(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_POSTCREATE
NS_IMETHODIMP XPC_MAP_CLASSNAME::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}

NS_IMETHODIMP XPC_MAP_CLASSNAME::PostTransplant(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_ADDPROPERTY
NS_IMETHODIMP XPC_MAP_CLASSNAME::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, jsid id, jsval * vp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_DELPROPERTY
NS_IMETHODIMP XPC_MAP_CLASSNAME::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, jsid id, jsval * vp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_GETPROPERTY
NS_IMETHODIMP XPC_MAP_CLASSNAME::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, jsid id, jsval * vp, bool *_retval)
    {NS_WARNING("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_SETPROPERTY
NS_IMETHODIMP XPC_MAP_CLASSNAME::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, jsid id, jsval * vp, bool *_retval)
    {NS_WARNING("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_NEWENUMERATE
NS_IMETHODIMP XPC_MAP_CLASSNAME::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 enum_op, jsval * statep, jsid * idp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_ENUMERATE
NS_IMETHODIMP XPC_MAP_CLASSNAME::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_NEWRESOLVE
NS_IMETHODIMP XPC_MAP_CLASSNAME::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, jsid id, PRUint32 flags, JSObject * *objp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_CONVERT
NS_IMETHODIMP XPC_MAP_CLASSNAME::Convert(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 type, jsval * vp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_FINALIZE
NS_IMETHODIMP XPC_MAP_CLASSNAME::Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp * fop, JSObject * obj)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_CHECKACCESS
NS_IMETHODIMP XPC_MAP_CLASSNAME::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, jsid id, PRUint32 mode, jsval * vp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_CALL
NS_IMETHODIMP XPC_MAP_CLASSNAME::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_CONSTRUCT
NS_IMETHODIMP XPC_MAP_CLASSNAME::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_HASINSTANCE
NS_IMETHODIMP XPC_MAP_CLASSNAME::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, const jsval &val, bool *bp, bool *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_EQUALITY
NS_IMETHODIMP XPC_MAP_CLASSNAME::Equality(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, const jsval &val, bool *bp)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_OUTER_OBJECT
NS_IMETHODIMP XPC_MAP_CLASSNAME::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, JSObject * *_retval)
    {NS_ERROR("never called"); return NS_ERROR_NOT_IMPLEMENTED;}
#endif

#ifndef XPC_MAP_WANT_POST_CREATE_PROTOTYPE
NS_IMETHODIMP XPC_MAP_CLASSNAME::PostCreatePrototype(JSContext *cx, JSObject *proto)
    {return NS_OK;}
#endif

/**************************************************************/

#undef XPC_MAP_CLASSNAME
#undef XPC_MAP_QUOTED_CLASSNAME

#ifdef XPC_MAP_WANT_PRECREATE
#undef XPC_MAP_WANT_PRECREATE
#endif

#ifdef XPC_MAP_WANT_CREATE
#undef XPC_MAP_WANT_CREATE
#endif

#ifdef XPC_MAP_WANT_POSTCREATE
#undef XPC_MAP_WANT_POSTCREATE
#endif

#ifdef XPC_MAP_WANT_ADDPROPERTY
#undef XPC_MAP_WANT_ADDPROPERTY
#endif

#ifdef XPC_MAP_WANT_DELPROPERTY
#undef XPC_MAP_WANT_DELPROPERTY
#endif

#ifdef XPC_MAP_WANT_GETPROPERTY
#undef XPC_MAP_WANT_GETPROPERTY
#endif

#ifdef XPC_MAP_WANT_SETPROPERTY
#undef XPC_MAP_WANT_SETPROPERTY
#endif

#ifdef XPC_MAP_WANT_ENUMERATE
#undef XPC_MAP_WANT_ENUMERATE
#endif

#ifdef XPC_MAP_WANT_NEWENUMERATE
#undef XPC_MAP_WANT_NEWENUMERATE
#endif

#ifdef XPC_MAP_WANT_NEWRESOLVE
#undef XPC_MAP_WANT_NEWRESOLVE
#endif

#ifdef XPC_MAP_WANT_CONVERT
#undef XPC_MAP_WANT_CONVERT
#endif

#ifdef XPC_MAP_WANT_FINALIZE
#undef XPC_MAP_WANT_FINALIZE
#endif

#ifdef XPC_MAP_WANT_CHECKACCESS
#undef XPC_MAP_WANT_CHECKACCESS
#endif

#ifdef XPC_MAP_WANT_CALL
#undef XPC_MAP_WANT_CALL
#endif

#ifdef XPC_MAP_WANT_CONSTRUCT
#undef XPC_MAP_WANT_CONSTRUCT
#endif

#ifdef XPC_MAP_WANT_HASINSTANCE
#undef XPC_MAP_WANT_HASINSTANCE
#endif

#ifdef XPC_MAP_WANT_EQUALITY
#undef XPC_MAP_WANT_EQUALITY
#endif

#ifdef XPC_MAP_WANT_OUTER_OBJECT
#undef XPC_MAP_WANT_OUTER_OBJECT
#endif

#ifdef XPC_MAP_WANT_POST_CREATE_PROTOTYPE
#undef XPC_MAP_WANT_POST_CREATE_PROTOTYPE
#endif

#ifdef XPC_MAP_FLAGS
#undef XPC_MAP_FLAGS
#endif
