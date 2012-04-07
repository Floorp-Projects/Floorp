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

NS_IMETHODIMP XPC_MAP_CLASSNAME::GetScriptableFlags(PRUint32 *aFlags)
{
    *aFlags =
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
    return NS_OK;
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
