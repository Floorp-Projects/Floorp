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

/* All the XPCScriptable public interfaces. */

#ifndef nsIXPCScriptable_h___
#define nsIXPCScriptable_h___

#include "nsISupports.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

/***************************************************************************/

// forward declaration
class nsIXPCScriptable;

#define XPC_DECLARE_IXPCSCRIPTABLE \
public: \
    NS_IMETHOD Create(JSContext *cx, JSObject *obj,                         \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      nsIXPCScriptable* arbitrary) COND_PURE ;              \
    NS_IMETHOD LookupProperty(JSContext *cx, JSObject *obj, jsid id,        \
                              JSObject **objp, JSProperty **propp,          \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval) COND_PURE ;                   \
    NS_IMETHOD DefineProperty(JSContext *cx, JSObject *obj,                 \
                              jsid id, jsval value,                         \
                              JSPropertyOp getter, JSPropertyOp setter,     \
                              uintN attrs, JSProperty **propp,              \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval) COND_PURE ;                   \
    NS_IMETHOD GetProperty(JSContext *cx, JSObject *obj,                    \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval) COND_PURE ;                      \
    NS_IMETHOD SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp,\
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval) COND_PURE ;                      \
    NS_IMETHOD GetAttributes(JSContext *cx, JSObject *obj, jsid id,         \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval) COND_PURE ;                    \
    NS_IMETHOD SetAttributes(JSContext *cx, JSObject *obj, jsid id,         \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval) COND_PURE ;                    \
    NS_IMETHOD DeleteProperty(JSContext *cx, JSObject *obj,                 \
                              jsid id, jsval *vp,                           \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval) COND_PURE ;                   \
    NS_IMETHOD DefaultValue(JSContext *cx, JSObject *obj,                   \
                            JSType type, jsval *vp,                         \
                            nsIXPConnectWrappedNative* wrapper,             \
                            nsIXPCScriptable* arbitrary,                    \
                            JSBool* retval) COND_PURE ;                     \
    NS_IMETHOD Enumerate(JSContext *cx, JSObject *obj,                      \
                         JSIterateOp enum_op,                               \
                         jsval *statep, jsid *idp,                          \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval) COND_PURE ;                        \
    NS_IMETHOD CheckAccess(JSContext *cx, JSObject *obj, jsid id,           \
                           JSAccessMode mode, jsval *vp, uintN *attrsp,     \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval) COND_PURE ;                      \
    NS_IMETHOD Call(JSContext *cx, JSObject *obj,                           \
                    uintN argc, jsval *argv,                                \
                    jsval *rval,                                            \
                    nsIXPConnectWrappedNative* wrapper,                     \
                    nsIXPCScriptable* arbitrary,                            \
                    JSBool* retval) COND_PURE ;                             \
    NS_IMETHOD Construct(JSContext *cx, JSObject *obj,                      \
                         uintN argc, jsval *argv,                           \
                         jsval *rval,                                       \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval) COND_PURE ;                        \
    NS_IMETHOD Finalize(JSContext *cx, JSObject *obj,                       \
                        nsIXPConnectWrappedNative* wrapper,                 \
                        nsIXPCScriptable* arbitrary) COND_PURE /*;*/


// {8A9C85F0-BA3A-11d2-982D-006008962422}
#define NS_IXPCSCRIPTABLE_IID \
{ 0x8a9c85f0, 0xba3a, 0x11d2, \
    { 0x98, 0x2d, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }
class nsIXPCScriptable : public nsISupports
{
public:
  static const nsIID& IID()
    {static nsIID iid = NS_IXPCSCRIPTABLE_IID; return iid;}
#define COND_PURE = 0
    XPC_DECLARE_IXPCSCRIPTABLE;
#undef COND_PURE
#define COND_PURE
};

// macro test...
// XPC_DECLARE_IXPCSCRIPTABLE;

/***************************************************************************/

#define XPC_IMPLEMENT_FORWARD_CREATE(_class) \
    NS_IMETHODIMP _class::Create(JSContext *cx, JSObject *obj,              \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      nsIXPCScriptable* arbitrary)                          \
    {return arbitrary->Create(cx, obj, wrapper, NULL);}

#define XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(_class) \
    NS_IMETHODIMP _class::LookupProperty(JSContext *cx, JSObject *obj,      \
                              jsid id,                                      \
                              JSObject **objp, JSProperty **propp,          \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,    \
                                      NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(_class) \
    NS_IMETHODIMP _class::DefineProperty(JSContext *cx, JSObject *obj,      \
                              jsid id, jsval value,                         \
                              JSPropertyOp getter, JSPropertyOp setter,     \
                              uintN attrs, JSProperty **propp,              \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {return arbitrary->DefineProperty(cx, obj, id, value, getter, setter,   \
                                 attrs, propp, wrapper, NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_GETPROPERTY(_class) \
    NS_IMETHODIMP _class::GetProperty(JSContext *cx, JSObject *obj,         \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return arbitrary->GetProperty(cx, obj, id, vp, wrapper, NULL, retval);}


#define XPC_IMPLEMENT_FORWARD_SETPROPERTY(_class) \
    NS_IMETHODIMP _class::SetProperty(JSContext *cx, JSObject *obj,         \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return arbitrary->SetProperty(cx, obj, id, vp, wrapper, NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(_class) \
    NS_IMETHODIMP _class::GetAttributes(JSContext *cx, JSObject *obj,       \
                             jsid id,                                       \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval)                                \
    {return arbitrary->GetAttributes(cx, obj, id, prop, attrsp, wrapper,    \
                                     NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(_class) \
    NS_IMETHODIMP _class::SetAttributes(JSContext *cx, JSObject *obj,       \
                             jsid id,                                       \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval)                                \
    {return arbitrary->SetAttributes(cx, obj, id, prop, attrsp, wrapper,    \
                                     NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(_class) \
    NS_IMETHODIMP _class::DeleteProperty(JSContext *cx, JSObject *obj,      \
                              jsid id, jsval *vp,                           \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {return arbitrary->DeleteProperty(cx, obj, id, vp, wrapper,             \
                                      NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(_class) \
    NS_IMETHODIMP _class::DefaultValue(JSContext *cx, JSObject *obj,        \
                            JSType type, jsval *vp,                         \
                            nsIXPConnectWrappedNative* wrapper,             \
                            nsIXPCScriptable* arbitrary,                    \
                            JSBool* retval)                                 \
    {return arbitrary->DefaultValue(cx, obj, type, vp, wrapper,             \
                                    NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_ENUMERATE(_class) \
    NS_IMETHODIMP _class::Enumerate(JSContext *cx, JSObject *obj,           \
                         JSIterateOp enum_op,                               \
                         jsval *statep, jsid *idp,                          \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval)                                    \
    {return arbitrary->Enumerate(cx, obj, enum_op, statep, idp, wrapper,    \
                                 NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_CHECKACCESS(_class) \
    NS_IMETHODIMP _class::CheckAccess(JSContext *cx, JSObject *obj,         \
                           jsid id,                                         \
                           JSAccessMode mode, jsval *vp, uintN *attrsp,     \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return arbitrary->CheckAccess(cx, obj, id, mode, vp, attrsp,           \
                              wrapper, NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_CALL(_class) \
    NS_IMETHODIMP _class::Call(JSContext *cx, JSObject *obj,                \
                    uintN argc, jsval *argv,                                \
                    jsval *rval,                                            \
                    nsIXPConnectWrappedNative* wrapper,                     \
                    nsIXPCScriptable* arbitrary,                            \
                    JSBool* retval)                                         \
    {return arbitrary->Call(cx, obj, argc, argv, rval, wrapper,             \
                            NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_CONSTRUCT(_class) \
    NS_IMETHODIMP _class::Construct(JSContext *cx, JSObject *obj,           \
                         uintN argc, jsval *argv,                           \
                         jsval *rval,                                       \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval)                                    \
    {return arbitrary->Construct(cx, obj, argc, argv, rval, wrapper,        \
                                 NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_FINALIZE(_class) \
    NS_IMETHODIMP _class::Finalize(JSContext *cx, JSObject *obj,            \
                        nsIXPConnectWrappedNative* wrapper,                 \
                        nsIXPCScriptable* arbitrary)                        \
    {return arbitrary->Finalize(cx, obj, wrapper, NULL);}

#define XPC_IMPLEMENT_FORWARD_IXPCSCRIPTABLE(_class) \
    XPC_IMPLEMENT_FORWARD_CREATE(_class) \
    XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(_class) \
    XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(_class) \
    XPC_IMPLEMENT_FORWARD_GETPROPERTY(_class) \
    XPC_IMPLEMENT_FORWARD_SETPROPERTY(_class) \
    XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(_class) \
    XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(_class) \
    XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(_class) \
    XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(_class) \
    XPC_IMPLEMENT_FORWARD_ENUMERATE(_class) \
    XPC_IMPLEMENT_FORWARD_CHECKACCESS(_class) \
    XPC_IMPLEMENT_FORWARD_CALL(_class) \
    XPC_IMPLEMENT_FORWARD_CONSTRUCT(_class) \
    XPC_IMPLEMENT_FORWARD_FINALIZE(_class)

// macro test...

//class foo : public nsIXPCScriptable
//{
//    XPC_DECLARE_IXPCSCRIPTABLE;
//};
//XPC_IMPLEMENT_FORWARD_IXPCSCRIPTABLE(foo)

/***************************************************************************/

#endif /* nsIXPCScriptable_h___ */
