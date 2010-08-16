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
#include "jsstr.h"
#include "jscntxt.h"  /* for error messages */
#include "nsCOMPtr.h"
#include "xpcprivate.h"
#include "xpcinlines.h"
#include "xpcquickstubs.h"
#include "XPCWrapper.h"
#include "XPCNativeWrapper.h"

static inline QITableEntry *
GetOffsets(nsISupports *identity, XPCWrappedNativeProto* proto)
{
    QITableEntry* offsets = proto ? proto->GetOffsets() : nsnull;
    if(!offsets)
    {
        static NS_DEFINE_IID(kThisPtrOffsetsSID, NS_THISPTROFFSETS_SID);
        identity->QueryInterface(kThisPtrOffsetsSID, (void**)&offsets);
    }
    return offsets;
}

static inline QITableEntry *
GetOffsetsFromSlimWrapper(JSObject *obj)
{
    NS_ASSERTION(IS_SLIM_WRAPPER(obj), "What kind of object is this?");
    return GetOffsets(static_cast<nsISupports*>(xpc_GetJSPrivate(obj)),
                      GetSlimWrapperProto(obj));
}

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
    const xpc_qsHashEntry *entry = LookupEntry(tableSize, table, iid);
    if(!entry)
    {
        /*
         * On a miss, we have to search for every interface the object
         * supports, including ancestors.
         */
        nsCOMPtr<nsIInterfaceInfo> info;
        if(NS_FAILED(nsXPConnect::GetXPConnect()->GetInfoForIID(
                          &iid, getter_AddRefs(info))))
            return nsnull;

        const nsIID *piid;
        for(;;)
        {
            nsCOMPtr<nsIInterfaceInfo> parent;
            if(NS_FAILED(info->GetParent(getter_AddRefs(parent))) ||
               !parent ||
               NS_FAILED(parent->GetIIDShared(&piid)))
            {
                break;
            }
            entry = LookupEntry(tableSize, table, *piid);
            if(entry)
                break;
            info.swap(parent);
        }
    }
    return entry;
}

static JSBool
PropertyOpForwarder(JSContext *cx, uintN argc, jsval *vp)
{
    // Layout:
    //   this = our this
    //   property op to call = callee reserved slot 0
    //   name of the property = callee reserved slot 1

    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    jsval v;

    if(!JS_GetReservedSlot(cx, callee, 0, &v))
        return JS_FALSE;
    JSObject *ptrobj = JSVAL_TO_OBJECT(v);
    JSPropertyOp *popp = static_cast<JSPropertyOp *>(JS_GetPrivate(cx, ptrobj));

    if(!JS_GetReservedSlot(cx, callee, 1, &v))
        return JS_FALSE;

    jsval argval = (argc > 0) ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    jsid id;
    if (!JS_ValueToId(cx, argval, &id))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, argval);
    return (*popp)(cx, obj, id, vp);
}

static void
PointerFinalize(JSContext *cx, JSObject *obj)
{
    JSPropertyOp *popp = static_cast<JSPropertyOp *>(JS_GetPrivate(cx, obj));
    delete popp;
}

static JSClass
PointerHolderClass = {
    "Pointer", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, PointerFinalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSObject *
GeneratePropertyOp(JSContext *cx, JSObject *obj, jsval idval, uintN argc,
                   const char *name, JSPropertyOp pop)
{
    // The JS engine provides two reserved slots on function objects for
    // XPConnect to use. Use them to stick the necessary info here.
    JSFunction *fun =
        JS_NewFunction(cx, reinterpret_cast<JSNative>(PropertyOpForwarder),
                       argc, 0, obj, name);
    if(!fun)
        return JS_FALSE;

    JSObject *funobj = JS_GetFunctionObject(fun);

    js::AutoObjectRooter tvr(cx, funobj);

    // Unfortunately, we cannot guarantee that JSPropertyOp is aligned. Use a
    // second object to work around this.
    JSObject *ptrobj = JS_NewObject(cx, &PointerHolderClass, nsnull, funobj);
    if(!ptrobj)
        return JS_FALSE;
    JSPropertyOp *popp = new JSPropertyOp;
    if(!popp)
        return JS_FALSE;
    *popp = pop;
    JS_SetPrivate(cx, ptrobj, popp);

    JS_SetReservedSlot(cx, funobj, 0, OBJECT_TO_JSVAL(ptrobj));
    JS_SetReservedSlot(cx, funobj, 1, idval);
    return funobj;
}

static JSBool
ReifyPropertyOps(JSContext *cx, JSObject *obj, jsval idval, jsid interned_id,
                 const char *name, JSPropertyOp getter, JSPropertyOp setter,
                 JSObject **getterobjp, JSObject **setterobjp)
{
    // Generate both getter and setter and stash them in the prototype.
    jsval roots[2] = { JSVAL_NULL, JSVAL_NULL };
    js::AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(roots), roots);

    uintN attrs = JSPROP_SHARED;
    JSObject *getterobj;
    if(getter)
    {
        getterobj = GeneratePropertyOp(cx, obj, idval, 0, name, getter);
        if(!getterobj)
            return JS_FALSE;
        roots[0] = OBJECT_TO_JSVAL(getterobj);
        attrs |= JSPROP_GETTER;
    }
    else
        getterobj = nsnull;

    JSObject *setterobj;
    if (setter)
    {
        setterobj = GeneratePropertyOp(cx, obj, idval, 1, name, setter);
        if(!setterobj)
            return JS_FALSE;
        roots[1] = OBJECT_TO_JSVAL(setterobj);
        attrs |= JSPROP_SETTER;
    }
    else
        setterobj = nsnull;

    if(getterobjp)
        *getterobjp = getterobj;
    if(setterobjp)
        *setterobjp = setterobj;
    return JS_DefinePropertyById(cx, obj, interned_id, JSVAL_VOID,
                                 JS_DATA_TO_FUNC_PTR(JSPropertyOp, getterobj),
                                 JS_DATA_TO_FUNC_PTR(JSPropertyOp, setterobj),
                                 attrs);
}

static JSBool
LookupGetterOrSetter(JSContext *cx, JSBool wantGetter, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    if(argc == 0)
    {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return JS_TRUE;
    }

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if(!obj)
        return JS_FALSE;

    jsval idval = JS_ARGV(cx, vp)[0];
    jsid interned_id;
    JSPropertyDescriptor desc;
    if(!JS_ValueToId(cx, idval, &interned_id) ||
       !JS_GetPropertyDescriptorById(cx, obj, interned_id, JSRESOLVE_QUALIFIED, &desc))
        return JS_FALSE;

    // No property at all means no getters or setters possible.
    if(!desc.obj)
    {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return JS_TRUE;
    }

    // Inline obj_lookup[GS]etter here.
    if(wantGetter)
    {
        if(desc.attrs & JSPROP_GETTER)
        {
            JS_SET_RVAL(cx, vp,
                        OBJECT_TO_JSVAL(JS_FUNC_TO_DATA_PTR(JSObject *, desc.getter)));
            return JS_TRUE;
        }
    }
    else
    {
        if(desc.attrs & JSPROP_SETTER)
        {
            JS_SET_RVAL(cx, vp,
                        OBJECT_TO_JSVAL(JS_FUNC_TO_DATA_PTR(JSObject *, desc.setter)));
            return JS_TRUE;
        }
    }

    // Since XPConnect doesn't use JSPropertyOps in any other contexts,
    // ensuring that we have an XPConnect prototype object ensures that
    // we are only going to expose quickstubbed properties to script.
    // Also be careful not to overwrite existing properties!

    const char *name = JSVAL_IS_STRING(idval)
                       ? JS_GetStringBytes(JSVAL_TO_STRING(idval))
                       : nsnull;
    if(!name ||
       !IS_PROTO_CLASS(desc.obj->getClass()) ||
       (desc.attrs & (JSPROP_GETTER | JSPROP_SETTER)) ||
       !(desc.getter || desc.setter) ||
       desc.setter == desc.obj->getJSClass()->setProperty)
    {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return JS_TRUE;
    }

    JSObject *getterobj, *setterobj;
    if(!ReifyPropertyOps(cx, obj, idval, interned_id, name,
                         desc.getter, desc.setter, &getterobj, &setterobj))
        return JS_FALSE;

    JSObject *wantedobj = wantGetter ? getterobj : setterobj;
    jsval v = wantedobj ? OBJECT_TO_JSVAL(wantedobj) : JSVAL_VOID;
    JS_SET_RVAL(cx, vp, v);
    return JS_TRUE;
}

static JSBool
SharedLookupGetter(JSContext *cx, uintN argc, jsval *vp)
{
    return LookupGetterOrSetter(cx, PR_TRUE, argc, vp);
}

static JSBool
SharedLookupSetter(JSContext *cx, uintN argc, jsval *vp)
{
    return LookupGetterOrSetter(cx, PR_FALSE, argc, vp);
}

static JSBool
DefineGetterOrSetter(JSContext *cx, uintN argc, JSBool wantGetter, jsval *vp)
{
    uintN attrs;
    JSBool found;
    JSPropertyOp getter, setter;
    JSObject *obj2;
    jsval v;
    jsid interned_id;

    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    JSNative forward = wantGetter ? Jsvalify(js_obj_defineGetter)
                                  : Jsvalify(js_obj_defineSetter);
    jsval id = (argc >= 1) ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    if(!JSVAL_IS_STRING(id))
        return forward(cx, argc, vp);
    JSString *str = JSVAL_TO_STRING(id);

    const char *name = JS_GetStringBytes(str);
    if(!JS_ValueToId(cx, id, &interned_id) ||
       !JS_LookupPropertyWithFlagsById(cx, obj, interned_id,
                                       JSRESOLVE_QUALIFIED, &obj2, &v) ||
       (obj2 &&
        !JS_GetPropertyAttrsGetterAndSetterById(cx, obj2, interned_id, &attrs,
                                                &found, &getter, &setter)))
        return JS_FALSE;

    // The property didn't exist, already has a getter or setter, or is not
    // our property, then just forward now.
    if(!obj2 ||
       (attrs & (JSPROP_GETTER | JSPROP_SETTER)) ||
       !(getter || setter) ||
       !IS_PROTO_CLASS(obj2->getClass()))
        return forward(cx, argc, vp);

    // Reify the getter and setter...
    if(!ReifyPropertyOps(cx, obj, id, interned_id, name, getter, setter,
                         nsnull, nsnull))
        return JS_FALSE;

    return forward(cx, argc, vp);
}

static JSBool
SharedDefineGetter(JSContext *cx, uintN argc, jsval *vp)
{
    return DefineGetterOrSetter(cx, argc, PR_TRUE, vp);
}

static JSBool
SharedDefineSetter(JSContext *cx, uintN argc, jsval *vp)
{
    return DefineGetterOrSetter(cx, argc, PR_FALSE, vp);
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
    PRBool definedProperty = PR_FALSE;
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
                        definedProperty = PR_TRUE;
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
                               fs->arity, flags))
                            return JS_FALSE;
                    }
                }

                const xpc_qsTraceableSpec *ts = entry->traceables;
                if(ts)
                {
                    for(; ts->name; ts++)
                    {
                        if(!JS_DefineFunction(
                               cx, proto, ts->name, ts->native, ts->arity,
                               flags | JSFUN_STUB_GSOPS | JSFUN_TRCINFO))
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

    static JSFunctionSpec getterfns[] = {
        JS_FN("__lookupGetter__", SharedLookupGetter, 1, 0),
        JS_FN("__lookupSetter__", SharedLookupSetter, 1, 0),
        JS_FN("__defineGetter__", SharedDefineGetter, 2, 0),
        JS_FN("__defineSetter__", SharedDefineSetter, 2, 0),
        JS_FS_END
    };

    if(definedProperty && !JS_DefineFunctions(cx, proto, getterfns))
        return JS_FALSE;

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
              jsid memberId,
              const char **ifaceName,
              const char **memberName)
{
    // Get the interface name.  From DefinePropertyIfFound (in
    // xpcwrappednativejsops.cpp) and XPCThrower::Verbosify.
    //
    // We could instead make the quick stub could pass in its interface name,
    // but this code often produces a more specific error message, e.g.
    *ifaceName = "Unknown";

    NS_ASSERTION(IS_WRAPPER_CLASS(obj->getClass()) ||
                 obj->getClass() == &XPC_WN_Tearoff_JSClass,
                 "obj must be a wrapper");
    XPCWrappedNativeProto *proto;
    if(IS_SLIM_WRAPPER(obj))
    {
        proto = GetSlimWrapperProto(obj);
    }
    else
    {
        XPCWrappedNative *wrapper = (XPCWrappedNative *) obj->getPrivate();
        proto = wrapper->GetProto();
    }
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

    *memberName = (JSID_IS_STRING(memberId)
                   ? JS_GetStringBytes(JSID_TO_STRING(memberId))
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
                 "JSNative callee should be Function object");
    JSString *str = JS_GetFunctionId((JSFunction *) JS_GetPrivate(cx, funobj));
    jsid methodId = str ? INTERNED_STRING_TO_JSID(str) : JSID_VOID;

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
                              jsid memberId)
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

void
xpc_qsThrowMethodFailedWithDetails(JSContext *cx, nsresult rv,
                                   const char *ifaceName,
                                   const char *memberName)
{
    ThrowCallFailed(cx, rv, ifaceName, memberName);
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
xpc_qsThrowBadArgWithDetails(JSContext *cx, nsresult rv, uintN paramnum,
                             const char *ifaceName, const char *memberName)
{
    ThrowBadArg(cx, rv, ifaceName, memberName, paramnum);
}

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv,
                          JSObject *obj, jsid propId)
{
    const char *ifaceName, *memberName;
    GetMemberInfo(obj, propId, &ifaceName, &memberName);
    ThrowBadArg(cx, rv, ifaceName, memberName, 0);
}

JSBool
xpc_qsGetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT |
                                        JSREPORT_STRICT_MODE_ERROR,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_GETTER_ONLY);
}

xpc_qsDOMString::xpc_qsDOMString(JSContext *cx, jsval v, jsval *pval,
                                 StringificationBehavior nullBehavior,
                                 StringificationBehavior undefinedBehavior)
{
    // From the T_DOMSTRING case in XPCConvert::JSData2Native.
    typedef implementation_type::char_traits traits;
    JSString *s;
    const PRUnichar *chars;
    size_t len;

    if(JSVAL_IS_STRING(v))
    {
        s = JSVAL_TO_STRING(v);
    }
    else
    {
        StringificationBehavior behavior = eStringify;
        if(JSVAL_IS_NULL(v))
        {
            behavior = nullBehavior;
        }
        else if(JSVAL_IS_VOID(v))
        {
            behavior = undefinedBehavior;
        }

        // If pval is null, that means the argument was optional and
        // not passed; turn those into void strings if they're
        // supposed to be stringified.
        if (behavior != eStringify || !pval)
        {
            // Here behavior == eStringify implies !pval, so both eNull and
            // eStringify should end up with void strings.
            (new(mBuf) implementation_type(
                traits::sEmptyBuffer, PRUint32(0)))->SetIsVoid(behavior != eEmpty);
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

    len = s->length();
    chars = (len == 0 ? traits::sEmptyBuffer :
                        reinterpret_cast<const PRUnichar*>(JS_GetStringChars(s)));
    new(mBuf) implementation_type(chars, len);
    mValid = JS_TRUE;
}

xpc_qsACString::xpc_qsACString(JSContext *cx, jsval v, jsval *pval)
{
    // From the T_CSTRING case in XPCConvert::JSData2Native.
    JSString *s;

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
    size_t len = s->length();
    new(mBuf) implementation_type(bytes, len);
    mValid = JS_TRUE;
}

static nsresult
getNative(nsISupports *idobj,
          QITableEntry* entries,
          JSObject *obj,
          const nsIID &iid,
          void **ppThis,
          nsISupports **pThisRef,
          jsval *vp)
{
    // Try using the QITableEntry to avoid the extra AddRef and Release.
    if(entries)
    {
        for(QITableEntry* e = entries; e->iid; e++)
        {
            if(e->iid->Equals(iid))
            {
                *ppThis = (char*) idobj + e->offset - entries[0].offset;
                *vp = OBJECT_TO_JSVAL(obj);
                *pThisRef = nsnull;
                return NS_OK;
            }
        }
    }

    nsresult rv = idobj->QueryInterface(iid, ppThis);
    *pThisRef = static_cast<nsISupports*>(*ppThis);
    if(NS_SUCCEEDED(rv))
        *vp = OBJECT_TO_JSVAL(obj);
    return rv;
}

inline nsresult
getNativeFromWrapper(XPCWrappedNative *wrapper,
                     const nsIID &iid,
                     void **ppThis,
                     nsISupports **pThisRef,
                     jsval *vp)
{
    return getNative(wrapper->GetIdentityObject(), wrapper->GetOffsets(),
                     wrapper->GetFlatJSObject(), iid, ppThis, pThisRef, vp);
}


nsresult
getWrapper(JSContext *cx,
           JSObject *obj,
           JSObject *callee,
           XPCWrappedNative **wrapper,
           JSObject **cur,
           XPCWrappedNativeTearOff **tearoff)
{
    if(XPCWrapper::IsSecurityWrapper(obj) &&
       !(obj = XPCWrapper::Unwrap(cx, obj)))
    {
        return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }

    *cur = obj;
    *tearoff = nsnull;

    *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj, callee, cur,
                                                     tearoff);

    return NS_OK;
}

nsresult
castNative(JSContext *cx,
           XPCWrappedNative *wrapper,
           JSObject *cur,
           XPCWrappedNativeTearOff *tearoff,
           const nsIID &iid,
           void **ppThis,
           nsISupports **pThisRef,
           jsval *vp,
           XPCLazyCallContext *lccx)
{
    if(wrapper)
    {
        nsresult rv = getNativeFromWrapper(wrapper, iid, ppThis, pThisRef, vp);

        if(lccx && NS_SUCCEEDED(rv))
            lccx->SetWrapper(wrapper, tearoff);

        if(rv != NS_ERROR_NO_INTERFACE)
            return rv;
    }
    else if(cur)
    {
        nsISupports *native = static_cast<nsISupports*>(xpc_GetJSPrivate(cur));
        if(NS_SUCCEEDED(getNative(native, GetOffsetsFromSlimWrapper(cur),
                                  cur, iid, ppThis, pThisRef, vp)))
        {
            if(lccx)
                lccx->SetWrapper(cur);

            return NS_OK;
        }
    }

    *pThisRef = nsnull;
    return NS_ERROR_XPC_BAD_OP_ON_WN_PROTO;
}

JSBool
xpc_qsUnwrapThisFromCcxImpl(XPCCallContext &ccx,
                            const nsIID &iid,
                            void **ppThis,
                            nsISupports **pThisRef,
                            jsval *vp)
{
    nsISupports *native = ccx.GetIdentityObject();
    if(!native)
        return xpc_qsThrow(ccx.GetJSContext(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

    nsresult rv = getNative(native, GetOffsets(native, ccx.GetProto()),
                            ccx.GetFlattenedJSObject(), iid, ppThis, pThisRef,
                            vp);
    if(NS_FAILED(rv))
        return xpc_qsThrow(ccx.GetJSContext(), rv);
    return JS_TRUE;
}

JSObject*
xpc_qsUnwrapObj(jsval v, nsISupports **ppArgRef, nsresult *rv)
{
    if(JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v))
    {
        *ppArgRef = nsnull;
        *rv = NS_OK;
        return nsnull;
    }

    if(!JSVAL_IS_OBJECT(v))
    {
        *ppArgRef = nsnull;
        *rv = ((JSVAL_IS_INT(v) && JSVAL_TO_INT(v) == 0)
              ? NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL
              : NS_ERROR_XPC_BAD_CONVERT_JS);
        return nsnull;
    }

    *rv = NS_OK;
    return JSVAL_TO_OBJECT(v);
}

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx,
                    jsval v,
                    const nsIID &iid,
                    void **ppArg,
                    nsISupports **ppArgRef,
                    jsval *vp)
{
    nsresult rv;
    JSObject *src = xpc_qsUnwrapObj(v, ppArgRef, &rv);
    if(!src)
    {
        *ppArg = nsnull;

        return rv;
    }

    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *obj2;
    rv = getWrapper(cx, src, nsnull, &wrapper, &obj2, &tearoff);
    NS_ENSURE_SUCCESS(rv, rv);

    if(wrapper || obj2)
    {
        if(NS_FAILED(castNative(cx, wrapper, obj2, tearoff, iid, ppArg,
                                ppArgRef, vp, nsnull)))
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        return NS_OK;
    }
    // else...
    // Slow path.

    // XXX E4X breaks the world. Don't try wrapping E4X objects!
    // This hack can be removed (or changed accordingly) when the
    // DOM <-> E4X bindings are complete, see bug 270553
    if(JS_TypeOfValue(cx, OBJECT_TO_JSVAL(src)) == JSTYPE_XML)
    {
        *ppArgRef = nsnull;
        return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    // Try to unwrap a slim wrapper.
    nsISupports *iface;
    if(XPCConvert::GetISupportsFromJSObject(src, &iface))
    {
        if(!iface || NS_FAILED(iface->QueryInterface(iid, ppArg)))
        {
            *ppArgRef = nsnull;
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        }

        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        return NS_OK;
    }

    // Create the ccx needed for quick stubs.
    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
    {
        *ppArgRef = nsnull;
        return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    nsRefPtr<nsXPCWrappedJS> wrappedJS;
    rv = nsXPCWrappedJS::GetNewOrUsed(ccx, src, iid, nsnull,
                                      getter_AddRefs(wrappedJS));
    if(NS_FAILED(rv) || !wrappedJS)
    {
        *ppArgRef = nsnull;
        return rv;
    }

    // We need to go through the QueryInterface logic to make this return
    // the right thing for the various 'special' interfaces; e.g.
    // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
    // there is an outer to avoid nasty recursion.
    rv = wrappedJS->QueryInterface(iid, ppArg);
    if(NS_SUCCEEDED(rv))
    {
        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        *vp = OBJECT_TO_JSVAL(wrappedJS->GetJSObject());
    }
    return rv;
}

JSBool
xpc_qsJsvalToCharStr(JSContext *cx, jsval v, jsval *pval, char **pstr)
{
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
xpc_qsJsvalToWcharStr(JSContext *cx, jsval v, jsval *pval, PRUnichar **pstr)
{
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

    // XXXbz this is casting away constness too...  That seems like a bad idea.
    *pstr = (PRUnichar*)JS_GetStringChars(str);
    return JS_TRUE;
}

JSBool
xpc_qsStringToJsval(JSContext *cx, nsString &str, jsval *rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if(str.IsVoid())
    {
        *rval = JSVAL_NULL;
        return JS_TRUE;
    }

    nsStringBuffer* sharedBuffer;
    jsval jsstr = XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer);
    if (JSVAL_IS_NULL(jsstr))
        return JS_FALSE;
    *rval = jsstr;
    if (sharedBuffer)
    {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return JS_TRUE;
}

JSBool
xpc_qsStringToJsstring(JSContext *cx, nsString &str, JSString **rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if(str.IsVoid())
    {
        *rval = nsnull;
        return JS_TRUE;
    }

    nsStringBuffer* sharedBuffer;
    jsval jsstr = XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer);
    if(JSVAL_IS_NULL(jsstr))
        return JS_FALSE;
    *rval = JSVAL_TO_STRING(jsstr);
    if (sharedBuffer)
    {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return JS_TRUE;
}

JSBool
xpc_qsXPCOMObjectToJsval(XPCLazyCallContext &lccx, qsObjectHelper &aHelper,
                         const nsIID *iid, XPCNativeInterface **iface,
                         jsval *rval)
{
    NS_PRECONDITION(iface, "Who did that and why?");

    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // This is one of the slowest things quick stubs do.

    JSContext *cx = lccx.GetJSContext();

    // XXX The OBJ_IS_NOT_GLOBAL here is not really right. In
    // fact, this code is depending on the fact that the
    // global object will not have been collected, and
    // therefore this NativeInterface2JSObject will not end up
    // creating a new XPCNativeScriptableShared.

    nsresult rv;
    if(!XPCConvert::NativeInterface2JSObject(lccx, rval, nsnull,
                                             aHelper, iid, iface,
                                             lccx.GetCurrentJSObject(), PR_TRUE,
                                             OBJ_IS_NOT_GLOBAL, &rv))
    {
        // I can't tell if NativeInterface2JSObject throws JS exceptions
        // or not.  This is a sloppy stab at the right semantics; the
        // method really ought to be fixed to behave consistently.
        if(!JS_IsExceptionPending(cx))
            xpc_qsThrow(cx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
        return JS_FALSE;
    }

#ifdef DEBUG
    JSObject* jsobj = JSVAL_TO_OBJECT(*rval);
    if(jsobj && !jsobj->getParent())
        NS_ASSERTION(jsobj->getClass()->flags & JSCLASS_IS_GLOBAL,
                     "Why did we recreate this wrapper?");
#endif

    return JS_TRUE;
}

JSBool
xpc_qsVariantToJsval(XPCLazyCallContext &lccx,
                     nsIVariant *p,
                     jsval *rval)
{
    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // Error handling is in XPCWrappedNative::CallMethod.
    if(p)
    {
        nsresult rv;
        JSBool ok = XPCVariant::VariantDataToJS(lccx, p,
                                                lccx.GetCurrentJSObject(),
                                                &rv, rval);
        if (!ok)
            xpc_qsThrow(lccx.GetJSContext(), rv);
        return ok;
    }
    *rval = JSVAL_NULL;
    return JS_TRUE;
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
