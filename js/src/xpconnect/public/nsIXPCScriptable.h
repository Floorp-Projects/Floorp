/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* All the XPCScriptable public interfaces. */

#ifndef nsIXPCScriptable_h___
#define nsIXPCScriptable_h___

#include "nsIXPConnect.h"
#include "jsapi.h"

/*
* A preliminary document (written before implementation) explaining the ideas
* behind this interface can be can be found at:
* http://www.mozilla.org/scriptable/scriptable-proposal.html
*
* nsIXPCScriptable is an interface that an object being wrapped by XPConnect
* can expose to support dynamic properites. XPConnect will QueryInterface
* wrapped xpcom objects to find out if they expose this interface. If an object
* does support this interface then XPConnect will pass through calls from JS
* to the methods of the nsIXPCScriptable under certain circumstances. Calls are
* passed through when JS code attempts to access properties on the wrapper
* which are not declared as attributes or methods in the xpidl that defines the
* interface. Calls will also be passed through for operation not normally
* supported on wrapped xpcom objects; e.g. 'call' and 'construct'.
*
* The methods of nsIXPCScriptable map directly onto the JSObjectOps 'interface'
* of the JavaScript api. In addition to the params passed by JavaScript the
* methods are also passed a pointer to the current wrapper and a pointer to
* the default implementation of the methods called the 'arbitrary' object.
*
* The methods of nsIXPCScriptable can be implemented to override default
* behavior or the calls can be forwarded to the 'arbitrary' object. Macros
* are declared below to make it easy to imlement objects which forward some or
* all of the method calls to the 'arbitrary' object.
*
* nsIXPCScriptable is meant to be called by XPConnect only. All the
* methods of this interface take params which identify the wrapper and
* therefore from which the wrapped object can be found. The methods are in
* effect virtually 'static' methods. For simplicity's sake we've decided to
* break COM identiry rules for this interface and say that its QueryInterface
* need not follow the normal rules of returning the same object as the
* QueryInterface of the object which returns this interface. Thus, the same
* nsIXPCScriptable object could be used by multiple objects if that suits you.
* In fact the 'arbitrary' object passed to this interface by XPConnect in all
* the methods is a singleton in XPConnect.
*/

/***************************************************************************/

// forward declaration
class nsIXPCScriptable;

// bit flags used in result of GetFlags call
#define XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS    (1 << 0)


#define XPC_DECLARE_IXPCSCRIPTABLE \
public: \
    NS_IMETHOD Create(JSContext *cx, JSObject *obj,                         \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      nsIXPCScriptable* arbitrary) COND_PURE ;              \
    NS_IMETHOD GetFlags(JSContext *cx, JSObject *obj,                       \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      JSUint32* flagsp,                                     \
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
    NS_IMETHOD HasInstance(JSContext *cx, JSObject *obj,                    \
                           jsval v, JSBool *bp,                             \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval) COND_PURE ;                      \
    NS_IMETHOD Finalize(JSContext *cx, JSObject *obj,                       \
                        nsIXPConnectWrappedNative* wrapper,                 \
                        nsIXPCScriptable* arbitrary) COND_PURE ;


// {8A9C85F0-BA3A-11d2-982D-006008962422}
#define NS_IXPCSCRIPTABLE_IID \
{ 0x8a9c85f0, 0xba3a, 0x11d2, \
    { 0x98, 0x2d, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }
class nsIXPCScriptable : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCSCRIPTABLE_IID)
#define COND_PURE = 0
    XPC_DECLARE_IXPCSCRIPTABLE
#undef COND_PURE
#define COND_PURE
};

// macro test...
// XPC_DECLARE_IXPCSCRIPTABLE

/***************************************************************************/

#define XPC_IMPLEMENT_FORWARD_CREATE(_class) \
    NS_IMETHODIMP _class::Create(JSContext *cx, JSObject *obj,              \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      nsIXPCScriptable* arbitrary)                          \
    {return arbitrary->Create(cx, obj, wrapper, NULL);}

#define XPC_IMPLEMENT_FORWARD_GETFLAGS(_class) \
    NS_IMETHODIMP _class::GetFlags(JSContext *cx, JSObject *obj,            \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      JSUint32* flagsp,                                     \
                      nsIXPCScriptable* arbitrary)                          \
    {return arbitrary->GetFlags(cx, obj, wrapper, flagsp, NULL);}

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

#define XPC_IMPLEMENT_FORWARD_HASINSTANCE(_class) \
    NS_IMETHODIMP _class::HasInstance(JSContext *cx, JSObject *obj,         \
                           jsval v, JSBool *bp,                             \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return arbitrary->HasInstance(cx, obj, v, bp, wrapper,                 \
                                 NULL, retval);}

#define XPC_IMPLEMENT_FORWARD_FINALIZE(_class) \
    NS_IMETHODIMP _class::Finalize(JSContext *cx, JSObject *obj,            \
                        nsIXPConnectWrappedNative* wrapper,                 \
                        nsIXPCScriptable* arbitrary)                        \
    /* XPConnect does the finalization on the wrapper itself anyway */      \
    {return arbitrary->Finalize(cx, obj, wrapper, NULL);}

/***************************************************************************/

#define XPC_IMPLEMENT_IGNORE_CREATE(_class) \
    NS_IMETHODIMP _class::Create(JSContext *cx, JSObject *obj,              \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      nsIXPCScriptable* arbitrary)                          \
    {return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_GETFLAGS(_class) \
    NS_IMETHODIMP _class::GetFlags(JSContext *cx, JSObject *obj,            \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      JSUint32* flagsp,                                     \
                      nsIXPCScriptable* arbitrary)                          \
    {*flagsp = 0; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(_class) \
    NS_IMETHODIMP _class::LookupProperty(JSContext *cx, JSObject *obj,      \
                              jsid id,                                      \
                              JSObject **objp, JSProperty **propp,          \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {*objp = NULL; *propp = NULL; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(_class) \
    NS_IMETHODIMP _class::DefineProperty(JSContext *cx, JSObject *obj,      \
                              jsid id, jsval value,                         \
                              JSPropertyOp getter, JSPropertyOp setter,     \
                              uintN attrs, JSProperty **propp,              \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {if(propp)*propp = NULL; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_GETPROPERTY(_class) \
    NS_IMETHODIMP _class::GetProperty(JSContext *cx, JSObject *obj,         \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {*vp = JSVAL_VOID; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_SETPROPERTY(_class) \
    NS_IMETHODIMP _class::SetProperty(JSContext *cx, JSObject *obj,         \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {*retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(_class) \
    NS_IMETHODIMP _class::GetAttributes(JSContext *cx, JSObject *obj,       \
                             jsid id,                                       \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval)                                \
    {*attrsp = 0; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(_class) \
    NS_IMETHODIMP _class::SetAttributes(JSContext *cx, JSObject *obj,       \
                             jsid id,                                       \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval)                                \
    {*retval = JS_FALSE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(_class) \
    NS_IMETHODIMP _class::DeleteProperty(JSContext *cx, JSObject *obj,      \
                              jsid id, jsval *vp,                           \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {*retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_DEFAULTVALUE(_class) \
    NS_IMETHODIMP _class::DefaultValue(JSContext *cx, JSObject *obj,        \
                            JSType type, jsval *vp,                         \
                            nsIXPConnectWrappedNative* wrapper,             \
                            nsIXPCScriptable* arbitrary,                    \
                            JSBool* retval)                                 \
    {*retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_ENUMERATE(_class) \
    NS_IMETHODIMP _class::Enumerate(JSContext *cx, JSObject *obj,           \
                         JSIterateOp enum_op,                               \
                         jsval *statep, jsid *idp,                          \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval)                                    \
    {*retval = JS_FALSE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_CHECKACCESS(_class) \
    NS_IMETHODIMP _class::CheckAccess(JSContext *cx, JSObject *obj,         \
                           jsid id,                                         \
                           JSAccessMode mode, jsval *vp, uintN *attrsp,     \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {*retval = JS_FALSE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_CALL(_class) \
    NS_IMETHODIMP _class::Call(JSContext *cx, JSObject *obj,                \
                    uintN argc, jsval *argv,                                \
                    jsval *rval,                                            \
                    nsIXPConnectWrappedNative* wrapper,                     \
                    nsIXPCScriptable* arbitrary,                            \
                    JSBool* retval)                                         \
    {*rval = JSVAL_NULL; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_CONSTRUCT(_class) \
    NS_IMETHODIMP _class::Construct(JSContext *cx, JSObject *obj,           \
                         uintN argc, jsval *argv,                           \
                         jsval *rval,                                       \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval)                                    \
    {*rval = JSVAL_NULL; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_HASINSTANCE(_class) \
    NS_IMETHODIMP _class::HasInstance(JSContext *cx, JSObject *obj,         \
                           jsval v, JSBool *bp,                             \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {*bp = JS_FALSE; *retval = JS_TRUE; return NS_OK;}

#define XPC_IMPLEMENT_IGNORE_FINALIZE(_class) \
    NS_IMETHODIMP _class::Finalize(JSContext *cx, JSObject *obj,            \
                        nsIXPConnectWrappedNative* wrapper,                 \
                        nsIXPCScriptable* arbitrary)                        \
    /* XPConnect does the finalization on the wrapper itself anyway */      \
    {return NS_OK;}

/***************************************************************************/

#define XPC_IMPLEMENT_FAIL_CREATE(_class, _code) \
    NS_IMETHODIMP _class::Create(JSContext *cx, JSObject *obj,              \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      nsIXPCScriptable* arbitrary)                          \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_GETFLAGS(_class, _code) \
    NS_IMETHODIMP _class::GetFlags(JSContext *cx, JSObject *obj,            \
                      nsIXPConnectWrappedNative* wrapper,                   \
                      JSUint32* flagsp,                                     \
                      nsIXPCScriptable* arbitrary)                          \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_LOOKUPPROPERTY(_class, _code) \
    NS_IMETHODIMP _class::LookupProperty(JSContext *cx, JSObject *obj,      \
                              jsid id,                                      \
                              JSObject **objp, JSProperty **propp,          \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_DEFINEPROPERTY(_class, _code) \
    NS_IMETHODIMP _class::DefineProperty(JSContext *cx, JSObject *obj,      \
                              jsid id, jsval value,                         \
                              JSPropertyOp getter, JSPropertyOp setter,     \
                              uintN attrs, JSProperty **propp,              \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_GETPROPERTY(_class, _code) \
    NS_IMETHODIMP _class::GetProperty(JSContext *cx, JSObject *obj,         \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_SETPROPERTY(_class, _code) \
    NS_IMETHODIMP _class::SetProperty(JSContext *cx, JSObject *obj,         \
                           jsid id, jsval *vp,                              \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_GETATTRIBUTES(_class, _code) \
    NS_IMETHODIMP _class::GetAttributes(JSContext *cx, JSObject *obj,       \
                             jsid id,                                       \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval)                                \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_SETATTRIBUTES(_class, _code) \
    NS_IMETHODIMP _class::SetAttributes(JSContext *cx, JSObject *obj,       \
                             jsid id,                                       \
                             JSProperty *prop, uintN *attrsp,               \
                             nsIXPConnectWrappedNative* wrapper,            \
                             nsIXPCScriptable* arbitrary,                   \
                             JSBool* retval)                                \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_DELETEPROPERTY(_class, _code) \
    NS_IMETHODIMP _class::DeleteProperty(JSContext *cx, JSObject *obj,      \
                              jsid id, jsval *vp,                           \
                              nsIXPConnectWrappedNative* wrapper,           \
                              nsIXPCScriptable* arbitrary,                  \
                              JSBool* retval)                               \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_DEFAULTVALUE(_class, _code) \
    NS_IMETHODIMP _class::DefaultValue(JSContext *cx, JSObject *obj,        \
                            JSType type, jsval *vp,                         \
                            nsIXPConnectWrappedNative* wrapper,             \
                            nsIXPCScriptable* arbitrary,                    \
                            JSBool* retval)                                 \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_ENUMERATE(_class, _code) \
    NS_IMETHODIMP _class::Enumerate(JSContext *cx, JSObject *obj,           \
                         JSIterateOp enum_op,                               \
                         jsval *statep, jsid *idp,                          \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval)                                    \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_CHECKACCESS(_class, _code) \
    NS_IMETHODIMP _class::CheckAccess(JSContext *cx, JSObject *obj,         \
                           jsid id,                                         \
                           JSAccessMode mode, jsval *vp, uintN *attrsp,     \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_CALL(_class, _code) \
    NS_IMETHODIMP _class::Call(JSContext *cx, JSObject *obj,                \
                    uintN argc, jsval *argv,                                \
                    jsval *rval,                                            \
                    nsIXPConnectWrappedNative* wrapper,                     \
                    nsIXPCScriptable* arbitrary,                            \
                    JSBool* retval)                                         \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_CONSTRUCT(_class, _code) \
    NS_IMETHODIMP _class::Construct(JSContext *cx, JSObject *obj,           \
                         uintN argc, jsval *argv,                           \
                         jsval *rval,                                       \
                         nsIXPConnectWrappedNative* wrapper,                \
                         nsIXPCScriptable* arbitrary,                       \
                         JSBool* retval)                                    \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_HASINSTANCE(_class, _code) \
    NS_IMETHODIMP _class::HasInstance(JSContext *cx, JSObject *obj,         \
                           jsval v, JSBool *bp,                             \
                           nsIXPConnectWrappedNative* wrapper,              \
                           nsIXPCScriptable* arbitrary,                     \
                           JSBool* retval)                                  \
    {return _code;}

#define XPC_IMPLEMENT_FAIL_FINALIZE(_class, _code) \
    NS_IMETHODIMP _class::Finalize(JSContext *cx, JSObject *obj,            \
                        nsIXPConnectWrappedNative* wrapper,                 \
                        nsIXPCScriptable* arbitrary)                        \
    /* XPConnect does the finalization on the wrapper itself anyway */      \
    {return _code;}

/***************************************************************************/

#define XPC_IMPLEMENT_FORWARD_IXPCSCRIPTABLE(_class)    \
    XPC_IMPLEMENT_FORWARD_CREATE(_class)                \
    XPC_IMPLEMENT_FORWARD_GETFLAGS(_class)              \
    XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(_class)        \
    XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(_class)        \
    XPC_IMPLEMENT_FORWARD_GETPROPERTY(_class)           \
    XPC_IMPLEMENT_FORWARD_SETPROPERTY(_class)           \
    XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(_class)         \
    XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(_class)         \
    XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(_class)        \
    XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(_class)          \
    XPC_IMPLEMENT_FORWARD_ENUMERATE(_class)             \
    XPC_IMPLEMENT_FORWARD_CHECKACCESS(_class)           \
    XPC_IMPLEMENT_FORWARD_CALL(_class)                  \
    XPC_IMPLEMENT_FORWARD_CONSTRUCT(_class)             \
    XPC_IMPLEMENT_FORWARD_HASINSTANCE(_class)           \
    XPC_IMPLEMENT_FORWARD_FINALIZE(_class)

#define XPC_IMPLEMENT_IGNORE_IXPCSCRIPTABLE(_class)     \
    XPC_IMPLEMENT_IGNORE_CREATE(_class)                 \
    XPC_IMPLEMENT_IGNORE_GETFLAGS(_class)               \
    XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(_class)         \
    XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(_class)         \
    XPC_IMPLEMENT_IGNORE_GETPROPERTY(_class)            \
    XPC_IMPLEMENT_IGNORE_SETPROPERTY(_class)            \
    XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(_class)          \
    XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(_class)          \
    XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(_class)         \
    XPC_IMPLEMENT_IGNORE_DEFAULTVALUE(_class)           \
    XPC_IMPLEMENT_IGNORE_ENUMERATE(_class)              \
    XPC_IMPLEMENT_IGNORE_CHECKACCESS(_class)            \
    XPC_IMPLEMENT_IGNORE_CALL(_class)                   \
    XPC_IMPLEMENT_IGNORE_CONSTRUCT(_class)              \
    XPC_IMPLEMENT_IGNORE_HASINSTANCE(_class)            \
    XPC_IMPLEMENT_IGNORE_FINALIZE(_class)

#define XPC_IMPLEMENT_FAIL_IXPCSCRIPTABLE(_class,_code) \
    XPC_IMPLEMENT_FAIL_CREATE(_class,_code)             \
    XPC_IMPLEMENT_FAIL_GETFLAGS(_class,_code)           \
    XPC_IMPLEMENT_FAIL_LOOKUPPROPERTY(_class,_code)     \
    XPC_IMPLEMENT_FAIL_DEFINEPROPERTY(_class,_code)     \
    XPC_IMPLEMENT_FAIL_GETPROPERTY(_class,_code)        \
    XPC_IMPLEMENT_FAIL_SETPROPERTY(_class,_code)        \
    XPC_IMPLEMENT_FAIL_GETATTRIBUTES(_class,_code)      \
    XPC_IMPLEMENT_FAIL_SETATTRIBUTES(_class,_code)      \
    XPC_IMPLEMENT_FAIL_DELETEPROPERTY(_class,_code)     \
    XPC_IMPLEMENT_FAIL_DEFAULTVALUE(_class,_code)       \
    XPC_IMPLEMENT_FAIL_ENUMERATE(_class,_code)          \
    XPC_IMPLEMENT_FAIL_CHECKACCESS(_class,_code)        \
    XPC_IMPLEMENT_FAIL_CALL(_class,_code)               \
    XPC_IMPLEMENT_FAIL_CONSTRUCT(_class,_code)          \
    XPC_IMPLEMENT_FAIL_HASINSTANCE(_class,_code)        \
    XPC_IMPLEMENT_FAIL_FINALIZE(_class,_code)


// macro test...

//class foo : public nsIXPCScriptable
//{
//    XPC_DECLARE_IXPCSCRIPTABLE
//};
//XPC_IMPLEMENT_FORWARD_IXPCSCRIPTABLE(foo)

/***************************************************************************/

#endif /* nsIXPCScriptable_h___ */
