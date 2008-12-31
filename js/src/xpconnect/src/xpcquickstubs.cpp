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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
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

#include "jsapi.h"
#include "jsobj.h"
#include "jsstr.h"
#include "jscntxt.h"  /* for error messages */
#include "nsCOMPtr.h"
#include "xpcprivate.h"
#include "xpcinlines.h"
#include "xpcquickstubs.h"
#include "XPCWrapper.h"
#include "XPCNativeWrapper.h"

static const xpc_qsHashEntry *
LookupEntry(PRUint32 tableSize, const xpc_qsHashEntry *table, const nsID &iid)
{
    size_t i;
    const xpc_qsHashEntry *p;

    i = iid.m0 % tableSize;
    do
    {
        p = table + i;
        if(p->iid.Equals(iid))
            return p;
        i = p->chain;
    } while(i != XPC_QS_NULL_INDEX);
    return nsnull;
}

static const xpc_qsHashEntry *
LookupInterfaceOrAncestor(PRUint32 tableSize, const xpc_qsHashEntry *table,
                          const nsID &iid)
{
    const xpc_qsHashEntry *p = LookupEntry(tableSize, table, iid);
    if(!p)
    {
        /*
         * On a miss, we have to search for every interface the object
         * supports, including ancestors.
         */
        nsCOMPtr<nsIInterfaceInfo> info;
        if(NS_FAILED(nsXPConnect::GetXPConnect()->GetInfoForIID(
                          &iid, getter_AddRefs(info))))
            return nsnull;

        nsIID *piid;
        for(;;)
        {
            nsCOMPtr<nsIInterfaceInfo> parent;
            if(NS_FAILED(info->GetParent(getter_AddRefs(parent))) ||
               !parent ||
               NS_FAILED(parent->GetInterfaceIID(&piid)))
            {
                break;
            }
            p = LookupEntry(tableSize, table, *piid);
            if(p)
                break;
            info.swap(parent);
        }
    }
    return p;
}

JSBool
xpc_qsDefineQuickStubs(JSContext *cx, JSObject *proto, uintN flags,
                       PRUint32 ifacec, const nsIID **interfaces,
                       PRUint32 tableSize, const xpc_qsHashEntry *table)
{
    /*
     * Walk interfaces in reverse order to behave like XPConnect when a
     * feature is defined in more than one of the interfaces.
     *
     * XPCNativeSet::FindMethod returns the first matching feature it finds,
     * searching the interfaces forward.  Here, definitions toward the
     * front of 'interfaces' overwrite those toward the back.
     */
    for(uint32 i = ifacec; i-- != 0;)
    {
        const nsID &iid = *interfaces[i];
        const xpc_qsHashEntry *entry =
            LookupInterfaceOrAncestor(tableSize, table, iid);

        if(entry)
        {
            for(;;)
            {
                // Define quick stubs for attributes.
                const xpc_qsPropertySpec *ps = entry->properties;
                if(ps)
                {
                    for(; ps->name; ps++)
                    {
                        if(!JS_DefineProperty(cx, proto, ps->name, JSVAL_VOID,
                                              ps->getter, ps->setter,
                                              flags | JSPROP_SHARED))
                            return JS_FALSE;
                    }
                }

                // Define quick stubs for methods.
                const xpc_qsFunctionSpec *fs = entry->functions;
                if(fs)
                {
                    for(; fs->name; fs++)
                    {
                        if(!JS_DefineFunction(
                               cx, proto, fs->name,
                               reinterpret_cast<JSNative>(fs->native),
                               fs->arity, flags | JSFUN_FAST_NATIVE))
                            return JS_FALSE;
                    }
                }

                // Next.
                size_t j = entry->parentInterface;
                if(j == XPC_QS_NULL_INDEX)
                    break;
                entry = table + j;
            }
        }
    }
    return JS_TRUE;
}

JSBool
xpc_qsThrow(JSContext *cx, nsresult rv)
{
    XPCThrower::Throw(rv, cx);
    return JS_FALSE;
}

/**
 * Get the interface name and member name (for error messages).
 *
 * We could instead have each quick stub pass its name to the error-handling
 * functions, as that name is statically known.  But that would be redundant;
 * the information is handy at runtime anyway.  Also, this code often produces
 * a more specific error message, e.g. "[nsIDOMHTMLDocument.appendChild]"
 * rather than "[nsIDOMNode.appendChild]".
 */
static void
GetMemberInfo(JSObject *obj,
              jsval memberId,
              const char **ifaceName,
              const char **memberName)
{
    // Get the interface name.  From DefinePropertyIfFound (in
    // xpcwrappednativejsops.cpp) and XPCThrower::Verbosify.
    //
    // We could instead make the quick stub could pass in its interface name,
    // but this code often produces a more specific error message, e.g.
    *ifaceName = "Unknown";

    NS_ASSERTION(IS_WRAPPER_CLASS(STOBJ_GET_CLASS(obj)) ||
                 STOBJ_GET_CLASS(obj) == &XPC_WN_Tearoff_JSClass,
                 "obj must be an XPCWrappedNative");
    XPCWrappedNative *wrapper = (XPCWrappedNative *) STOBJ_GET_PRIVATE(obj);
    XPCWrappedNativeProto *proto = wrapper->GetProto();
    if(proto)
    {
        XPCNativeSet *set = proto->GetSet();
        if(set)
        {
            XPCNativeMember *member;
            XPCNativeInterface *iface;

            if(set->FindMember(memberId, &member, &iface))
                *ifaceName = iface->GetNameString();
        }
    }

    *memberName = (JSVAL_IS_STRING(memberId)
                   ? JS_GetStringBytes(JSVAL_TO_STRING(memberId))
                   : "unknown");
}

static void
GetMethodInfo(JSContext *cx,
              jsval *vp,
              const char **ifaceName,
              const char **memberName)
{
    JSObject *funobj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    NS_ASSERTION(JS_ObjectIsFunction(cx, funobj),
                 "JSFastNative callee should be Function object");
    JSString *str = JS_GetFunctionId((JSFunction *) JS_GetPrivate(cx, funobj));
    jsval methodId = str ? STRING_TO_JSVAL(str) : JSVAL_NULL;

    GetMemberInfo(JSVAL_TO_OBJECT(vp[1]), methodId, ifaceName, memberName);
}

static JSBool
ThrowCallFailed(JSContext *cx, nsresult rv,
                const char *ifaceName, const char *memberName)
{
    // From XPCThrower::ThrowBadResult.
    char* sz;
    const char* format;
    const char* name;

    /*
     *  If there is a pending exception when the native call returns and
     *  it has the same error result as returned by the native call, then
     *  the native call may be passing through an error from a previous JS
     *  call. So we'll just throw that exception into our JS.
     */
    if(XPCThrower::CheckForPendingException(rv, cx))
        return JS_FALSE;

    // else...

    if(!nsXPCException::NameAndFormatForNSResult(
            NS_ERROR_XPC_NATIVE_RETURNED_FAILURE, nsnull, &format) ||
        !format)
    {
        format = "";
    }

    if(nsXPCException::NameAndFormatForNSResult(rv, &name, nsnull)
        && name)
    {
        sz = JS_smprintf("%s 0x%x (%s) [%s.%s]",
                         format, rv, name, ifaceName, memberName);
    }
    else
    {
        sz = JS_smprintf("%s 0x%x [%s.%s]",
                         format, rv, ifaceName, memberName);
    }

    XPCThrower::BuildAndThrowException(cx, rv, sz);

    if(sz)
        JS_smprintf_free(sz);

    return JS_FALSE;
}

JSBool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv, JSObject *obj,
                              jsval memberId)
{
    const char *ifaceName, *memberName;
    GetMemberInfo(obj, memberId, &ifaceName, &memberName);
    return ThrowCallFailed(cx, rv, ifaceName, memberName);
}

JSBool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv, jsval *vp)
{
    const char *ifaceName, *memberName;
    GetMethodInfo(cx, vp, &ifaceName, &memberName);
    return ThrowCallFailed(cx, rv, ifaceName, memberName);
}

JSBool
xpc_qsThrowMethodFailedWithCcx(XPCCallContext &ccx, nsresult rv)
{
    ThrowBadResult(rv, ccx);
    return JS_FALSE;
}

static void
ThrowBadArg(JSContext *cx, nsresult rv,
            const char *ifaceName, const char *memberName, uintN paramnum)
{
    // From XPCThrower::ThrowBadParam.
    char* sz;
    const char* format;

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";

    sz = JS_smprintf("%s arg %u [%s.%s]",
                     format, (unsigned int) paramnum, ifaceName, memberName);

    XPCThrower::BuildAndThrowException(cx, rv, sz);

    if(sz)
        JS_smprintf_free(sz);
}

void
xpc_qsThrowBadArg(JSContext *cx, nsresult rv, jsval *vp, uintN paramnum)
{
    const char *ifaceName, *memberName;
    GetMethodInfo(cx, vp, &ifaceName, &memberName);
    ThrowBadArg(cx, rv, ifaceName, memberName, paramnum);
}

void
xpc_qsThrowBadArgWithCcx(XPCCallContext &ccx, nsresult rv, uintN paramnum)
{
    XPCThrower::ThrowBadParam(rv, paramnum, ccx);
}

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv,
                          JSObject *obj, jsval propId)
{
    const char *ifaceName, *memberName;
    GetMemberInfo(obj, propId, &ifaceName, &memberName);
    ThrowBadArg(cx, rv, ifaceName, memberName, 0);
}

xpc_qsDOMString::xpc_qsDOMString(JSContext *cx, jsval *pval)
{
    // From the T_DOMSTRING case in XPCConvert::JSData2Native.
    typedef implementation_type::char_traits traits;
    jsval v;
    JSString *s;
    const jschar *chars;
    size_t len;

    v = *pval;
    if(JSVAL_IS_STRING(v))
    {
        s = JSVAL_TO_STRING(v);
    }
    else
    {
        if(JSVAL_IS_NULL(v))
        {
            (new(mBuf) implementation_type(
                traits::sEmptyBuffer, PRUint32(0)))->SetIsVoid(PR_TRUE);
            mValid = JS_TRUE;
            return;
        }

        s = JS_ValueToString(cx, v);
        if(!s)
        {
            mValid = JS_FALSE;
            return;
        }
        *pval = STRING_TO_JSVAL(s);  // Root the new string.
    }

    len = JS_GetStringLength(s);
    chars = (len == 0 ? traits::sEmptyBuffer : JS_GetStringChars(s));
    new(mBuf) implementation_type(chars, len);
    mValid = JS_TRUE;
}

xpc_qsAString::xpc_qsAString(JSContext *cx, jsval *pval)
{
    // From the T_ASTRING case in XPCConvert::JSData2Native.
    typedef implementation_type::char_traits traits;
    jsval v;
    JSString *s;
    const jschar *chars;
    size_t len;

    v = *pval;
    if(JSVAL_IS_STRING(v))
    {
        s = JSVAL_TO_STRING(v);
    }
    else
    {
        if(JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        {
            (new(mBuf) implementation_type(
                traits::sEmptyBuffer, PRUint32(0)))->SetIsVoid(PR_TRUE);
            mValid = JS_TRUE;
            return;
        }

        s = JS_ValueToString(cx, v);
        if(!s)
        {
            mValid = JS_FALSE;
            return;
        }
        *pval = STRING_TO_JSVAL(s);  // Root the new string.
    }

    len = JS_GetStringLength(s);
    chars = (len == 0 ? traits::sEmptyBuffer : JS_GetStringChars(s));
    new(mBuf) implementation_type(chars, len);
    mValid = JS_TRUE;
}

xpc_qsACString::xpc_qsACString(JSContext *cx, jsval *pval)
{
    // From the T_CSTRING case in XPCConvert::JSData2Native.
    jsval v;
    JSString *s;

    v = *pval;
    if(JSVAL_IS_STRING(v))
    {
        s = JSVAL_TO_STRING(v);
    }
    else
    {
        if(JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        {
            (new(mBuf) implementation_type())->SetIsVoid(PR_TRUE);
            mValid = JS_TRUE;
            return;
        }

        s = JS_ValueToString(cx, v);
        if(!s)
        {
            mValid = JS_FALSE;
            return;
        }
        *pval = STRING_TO_JSVAL(s);  // Root the new string.
    }

    const char *bytes = JS_GetStringBytes(s);
    size_t len = JS_GetStringLength(s);
    new(mBuf) implementation_type(bytes, len);
    mValid = JS_TRUE;
}

static nsresult
getNativeFromWrapper(XPCWrappedNative *wrapper,
                     const nsIID &iid,
                     void **ppThis,
                     nsISupports **pThisRef,
                     jsval *vp)
{
    nsISupports *idobj = wrapper->GetIdentityObject();

    // Try using the QITableEntry to avoid the extra AddRef and Release.
    QITableEntry* entries = wrapper->GetOffsets();
    if(entries)
    {
        for(QITableEntry* e = entries; e->iid; e++)
        {
            if(e->iid->Equals(iid))
            {
                *ppThis = (char*) idobj + e->offset - entries[0].offset;
                *vp = OBJECT_TO_JSVAL(wrapper->GetFlatJSObject());
                *pThisRef = nsnull;
                return NS_OK;
            }
        }
    }

    nsresult rv = idobj->QueryInterface(iid, ppThis);
    *pThisRef = static_cast<nsISupports*>(*ppThis);
    if(NS_SUCCEEDED(rv))
        *vp = OBJECT_TO_JSVAL(wrapper->GetFlatJSObject());
    return rv;
}

JSBool
xpc_qsUnwrapThisImpl(JSContext *cx,
                     JSObject *obj,
                     const nsIID &iid,
                     void **ppThis,
                     nsISupports **pThisRef,
                     jsval *vp)
{
    // From XPCWrappedNative::GetWrappedNativeOfJSObject.
    //
    // Usually IS_WRAPPER_CLASS is true the first time through the while loop,
    // and the QueryInterface then succeeds.

    NS_ASSERTION(obj, "this == null");

    JSObject *cur = obj;
    while(cur)
    {
        JSClass *clazz;
        XPCWrappedNative *wrapper;
        nsresult rv;

        clazz = STOBJ_GET_CLASS(cur);
        if(IS_WRAPPER_CLASS(clazz))
        {
            wrapper = (XPCWrappedNative*) xpc_GetJSPrivate(cur);
            NS_ASSERTION(wrapper, "XPCWN wrapping nothing");
        }
        else if(clazz == &XPC_WN_Tearoff_JSClass)
        {
            wrapper = (XPCWrappedNative*) xpc_GetJSPrivate(STOBJ_GET_PARENT(cur));
            NS_ASSERTION(wrapper, "XPCWN wrapping nothing");
        }
        else if(clazz == &sXPC_XOW_JSClass.base)
        {
            JSObject *unsafeObj = XPCWrapper::Unwrap(cx, cur);
            if(unsafeObj)
            {
                cur = unsafeObj;
                continue;
            }

            // This goto is a bug, dutifully copied from
            // XPCWrappedNative::GetWrappedNativeOfJSObject.
            goto next;
        }
        else if(XPCNativeWrapper::IsNativeWrapperClass(clazz))
        {
            wrapper = XPCNativeWrapper::GetWrappedNative(cur);
            NS_ASSERTION(wrapper, "XPCNativeWrapper wrapping nothing");
        }
        else if(IsXPCSafeJSObjectWrapperClass(clazz))
        {
            cur = STOBJ_GET_PARENT(cur);
            NS_ASSERTION(cur, "SJOW wrapping nothing");
            continue;
        }
        else {
            goto next;
        }

        rv = getNativeFromWrapper(wrapper, iid, ppThis, pThisRef, vp);
        if(NS_SUCCEEDED(rv))
            return JS_TRUE;
        if(rv != NS_ERROR_NO_INTERFACE)
            return xpc_qsThrow(cx, rv);

    next:
        cur = STOBJ_GET_PROTO(cur);
    }

    // If we didn't find a wrapper using the given obj, try again with obj's
    // outer object, if it's got one.

    JSClass *clazz = STOBJ_GET_CLASS(obj);

    if((clazz->flags & JSCLASS_IS_EXTENDED) &&
        ((JSExtendedClass*)clazz)->outerObject)
    {
        JSObject *outer = ((JSExtendedClass*)clazz)->outerObject(cx, obj);

        // Protect against infinite recursion through XOWs.
        JSObject *unsafeObj;
        clazz = STOBJ_GET_CLASS(outer);
        if(clazz == &sXPC_XOW_JSClass.base &&
           (unsafeObj = XPCWrapper::Unwrap(cx, outer)))
        {
            outer = unsafeObj;
        }

        if(outer && outer != obj)
            return xpc_qsUnwrapThisImpl(cx, outer, iid, ppThis, pThisRef, vp);
    }

    *pThisRef = nsnull;
    return xpc_qsThrow(cx, NS_ERROR_XPC_BAD_OP_ON_WN_PROTO);
}

JSBool
xpc_qsUnwrapThisFromCcxImpl(XPCCallContext &ccx,
                            const nsIID &iid,
                            void **ppThis,
                            nsISupports **pThisRef,
                            jsval *vp)
{
    XPCWrappedNative *wrapper = ccx.GetWrapper();
    if(!wrapper)
        return xpc_qsThrow(ccx.GetJSContext(), NS_ERROR_XPC_BAD_OP_ON_WN_PROTO);
    if(!wrapper->IsValid())
        return xpc_qsThrow(ccx.GetJSContext(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

    nsresult rv = getNativeFromWrapper(wrapper, iid, ppThis, pThisRef, vp);
    if(NS_FAILED(rv))
        return xpc_qsThrow(ccx.GetJSContext(), rv);
    return JS_TRUE;
}

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx,
                    jsval v,
                    const nsIID &iid,
                    void **ppArg)
{
    // From XPCConvert::JSData2Native
    if(JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v))
        return NS_OK;

    if(!JSVAL_IS_OBJECT(v))
    {
        return ((JSVAL_IS_INT(v) && JSVAL_TO_INT(v) == 0)
                ? NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL
                : NS_ERROR_XPC_BAD_CONVERT_JS);
    }
    JSObject *src = JSVAL_TO_OBJECT(v);

    // From XPCConvert::JSObject2NativeInterface
    XPCWrappedNative* wrappedNative =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, src);
    nsISupports *iface;
    if(wrappedNative)
    {
        iface = wrappedNative->GetIdentityObject();
        if(NS_FAILED(iface->QueryInterface(iid, ppArg)))
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        return NS_OK;
    }
    // else...
    // Slow path.

    // XXX E4X breaks the world. Don't try wrapping E4X objects!
    // This hack can be removed (or changed accordingly) when the
    // DOM <-> E4X bindings are complete, see bug 270553
    if(JS_TypeOfValue(cx, OBJECT_TO_JSVAL(src)) == JSTYPE_XML)
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // Does the JSObject have 'nsISupportness'?
    // XXX hmm, I wonder if this matters anymore with no
    // oldstyle DOM objects around.
    if(XPCConvert::GetISupportsFromJSObject(src, &iface))
    {
        if(!iface || NS_FAILED(iface->QueryInterface(iid, ppArg)))
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        return NS_OK;
    }

    // Create the ccx needed for quick stubs.
    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    nsXPCWrappedJS *wrapper;
    nsresult rv =
        nsXPCWrappedJS::GetNewOrUsed(ccx, src, iid, nsnull, &wrapper);
    if(NS_FAILED(rv) || !wrapper)
        return rv;

    // We need to go through the QueryInterface logic to make this return
    // the right thing for the various 'special' interfaces; e.g.
    // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
    // there is an outer to avoid nasty recursion.
    rv = wrapper->QueryInterface(iid, ppArg);
    NS_RELEASE(wrapper);
    return rv;
}

JSBool
xpc_qsJsvalToCharStr(JSContext *cx, jsval *pval, char **pstr)
{
    jsval v = *pval;
    JSString *str;

    if(JSVAL_IS_STRING(v))
    {
        str = JSVAL_TO_STRING(v);
    }
    else if(JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v))
    {
        *pstr = NULL;
        return JS_TRUE;
    }
    else
    {
        if(!(str = JS_ValueToString(cx, v)))
            return JS_FALSE;
        *pval = STRING_TO_JSVAL(str);  // Root the new string.
    }

    *pstr = JS_GetStringBytes(str);
    return JS_TRUE;
}

JSBool
xpc_qsJsvalToWcharStr(JSContext *cx, jsval *pval, PRUnichar **pstr)
{
    jsval v = *pval;
    JSString *str;

    if(JSVAL_IS_STRING(v))
    {
        str = JSVAL_TO_STRING(v);
    }
    else if(JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v))
    {
        *pstr = NULL;
        return JS_TRUE;
    }
    else
    {
        if(!(str = JS_ValueToString(cx, v)))
            return JS_FALSE;
        *pval = STRING_TO_JSVAL(str);  // Root the new string.
    }

    *pstr = JS_GetStringChars(str);
    return JS_TRUE;
}

JSBool
xpc_qsStringToJsval(JSContext *cx, const nsAString &str, jsval *rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if(str.IsVoid())
    {
        *rval = JSVAL_NULL;
        return JS_TRUE;
    }

    JSString *jsstr = XPCStringConvert::ReadableToJSString(cx, str);
    if(!jsstr)
        return JS_FALSE;
    *rval = STRING_TO_JSVAL(jsstr);
    return JS_TRUE;
}

JSBool
xpc_qsXPCOMObjectToJsval(XPCCallContext &ccx, nsISupports *p,
                         nsWrapperCache *cache, XPCNativeInterface *iface,
                         jsval *rval)
{
    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // This is one of the slowest things quick stubs do.

    JSObject *scope = ccx.GetCurrentJSObject();
    NS_ASSERTION(scope, "bad ccx");

    if(!iface)
        return xpc_qsThrow(ccx, NS_ERROR_XPC_BAD_CONVERT_NATIVE);

    // XXX The OBJ_IS_NOT_GLOBAL here is not really right. In
    // fact, this code is depending on the fact that the
    // global object will not have been collected, and
    // therefore this NativeInterface2JSObject will not end up
    // creating a new XPCNativeScriptableShared.
    nsresult rv;
    if(!XPCConvert::NativeInterface2JSObject(ccx, rval, nsnull, p, nsnull,
                                             iface, cache, scope, PR_TRUE,
                                             OBJ_IS_NOT_GLOBAL, &rv))
    {
        // I can't tell if NativeInterface2JSObject throws JS exceptions
        // or not.  This is a sloppy stab at the right semantics; the
        // method really ought to be fixed to behave consistently.
        if(!JS_IsExceptionPending(ccx))
            xpc_qsThrow(ccx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
        return JS_FALSE;
    }

#ifdef DEBUG
    JSObject* jsobj = JSVAL_TO_OBJECT(*rval);
    if(jsobj && !STOBJ_GET_PARENT(jsobj))
        NS_ASSERTION(STOBJ_GET_CLASS(jsobj)->flags & JSCLASS_IS_GLOBAL,
                     "Why did we recreate this wrapper?");
#endif

    return JS_TRUE;
}

JSBool
xpc_qsVariantToJsval(XPCCallContext &ccx,
                     nsIVariant *p,
                     uintN paramNum,
                     jsval *rval)
{
    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // Error handling is in XPCWrappedNative::CallMethod.
    if(p)
    {
        nsresult rv;
        JSBool ok = XPCVariant::VariantDataToJS(ccx, p,
                                                ccx.GetCurrentJSObject(),
                                                &rv, rval);
        if (!ok)
            XPCThrower::ThrowBadParam(rv, 0, ccx);
        return ok;
    }
    *rval = JSVAL_NULL;
    return JS_TRUE;
}

JSBool
xpc_qsReadOnlySetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_GETTER_ONLY, NULL);
    return JS_FALSE;
}

#ifdef DEBUG
void
xpc_qsAssertContextOK(JSContext *cx)
{
    XPCPerThreadData *thread = XPCPerThreadData::GetData(cx);
    XPCJSContextStack* stack = thread->GetJSContextStack();

    JSContext* topJSContext = nsnull;
    nsresult rv = stack->Peek(&topJSContext);
    NS_ASSERTION(NS_SUCCEEDED(rv), "XPCJSContextStack::Peek failed");

    // This is what we're actually trying to assert here.
    NS_ASSERTION(cx == topJSContext, "wrong context on XPCJSContextStack!");

    NS_ASSERTION(XPCPerThreadData::IsMainThread(cx),
                 "XPConnect quick stub called on non-main thread");
}
#endif
