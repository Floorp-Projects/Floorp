/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

#include "mozilla/Util.h"

#include "jsapi.h"
#include "jscntxt.h"  /* for error messages */
#include "nsCOMPtr.h"
#include "xpcprivate.h"
#include "XPCInlines.h"
#include "XPCQuickStubs.h"
#include "XPCWrapper.h"

using namespace mozilla;

static inline QITableEntry *
GetOffsets(nsISupports *identity, XPCWrappedNativeProto* proto)
{
    QITableEntry* offsets = proto ? proto->GetOffsets() : nsnull;
    if (!offsets) {
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
        if (p->iid.Equals(iid))
            return p;
        i = p->chain;
    } while (i != XPC_QS_NULL_INDEX);
    return nsnull;
}

static const xpc_qsHashEntry *
LookupInterfaceOrAncestor(PRUint32 tableSize, const xpc_qsHashEntry *table,
                          const nsID &iid)
{
    const xpc_qsHashEntry *entry = LookupEntry(tableSize, table, iid);
    if (!entry) {
        /*
         * On a miss, we have to search for every interface the object
         * supports, including ancestors.
         */
        nsCOMPtr<nsIInterfaceInfo> info;
        if (NS_FAILED(nsXPConnect::GetXPConnect()->GetInfoForIID(&iid, getter_AddRefs(info))))
            return nsnull;

        const nsIID *piid;
        for (;;) {
            nsCOMPtr<nsIInterfaceInfo> parent;
            if (NS_FAILED(info->GetParent(getter_AddRefs(parent))) ||
                !parent ||
                NS_FAILED(parent->GetIIDShared(&piid))) {
                break;
            }
            entry = LookupEntry(tableSize, table, *piid);
            if (entry)
                break;
            info.swap(parent);
        }
    }
    return entry;
}

// Apply |op| to |obj|, |id|, and |vp|. If |op| is a setter, treat the assignment as lenient.
template<typename Op>
static inline JSBool ApplyPropertyOp(JSContext *cx, Op op, JSObject *obj, jsid id, jsval *vp);

template<>
inline JSBool
ApplyPropertyOp<JSPropertyOp>(JSContext *cx, JSPropertyOp op, JSObject *obj, jsid id, jsval *vp)
{
    return op(cx, obj, id, vp);
}

template<>
inline JSBool
ApplyPropertyOp<JSStrictPropertyOp>(JSContext *cx, JSStrictPropertyOp op, JSObject *obj,
                                    jsid id, jsval *vp)
{
    return op(cx, obj, id, true, vp);
}

template<typename Op>
static JSBool
PropertyOpForwarder(JSContext *cx, uintN argc, jsval *vp)
{
    // Layout:
    //   this = our this
    //   property op to call = callee reserved slot 0
    //   name of the property = callee reserved slot 1

    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    jsval v = js::GetFunctionNativeReserved(callee, 0);

    JSObject *ptrobj = JSVAL_TO_OBJECT(v);
    Op *popp = static_cast<Op *>(JS_GetPrivate(cx, ptrobj));

    v = js::GetFunctionNativeReserved(callee, 1);

    jsval argval = (argc > 0) ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    jsid id;
    if (!JS_ValueToId(cx, argval, &id))
        return false;
    JS_SET_RVAL(cx, vp, argval);
    return ApplyPropertyOp<Op>(cx, *popp, obj, id, vp);
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
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, PointerFinalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

template<typename Op>
static JSObject *
GeneratePropertyOp(JSContext *cx, JSObject *obj, jsid id, uintN argc, Op pop)
{
    // The JS engine provides two reserved slots on function objects for
    // XPConnect to use. Use them to stick the necessary info here.
    JSFunction *fun =
        js::NewFunctionByIdWithReserved(cx, PropertyOpForwarder<Op>, argc, 0, obj, id);
    if (!fun)
        return false;

    JSObject *funobj = JS_GetFunctionObject(fun);

    js::AutoObjectRooter tvr(cx, funobj);

    // Unfortunately, we cannot guarantee that Op is aligned. Use a
    // second object to work around this.
    JSObject *ptrobj = JS_NewObject(cx, &PointerHolderClass, nsnull, funobj);
    if (!ptrobj)
        return false;
    Op *popp = new Op;
    if (!popp)
        return false;
    *popp = pop;
    JS_SetPrivate(cx, ptrobj, popp);

    js::SetFunctionNativeReserved(funobj, 0, OBJECT_TO_JSVAL(ptrobj));
    js::SetFunctionNativeReserved(funobj, 1, js::IdToJsval(id));
    return funobj;
}

static JSBool
ReifyPropertyOps(JSContext *cx, JSObject *obj, jsid id, uintN orig_attrs,
                 JSPropertyOp getter, JSStrictPropertyOp setter,
                 JSObject **getterobjp, JSObject **setterobjp)
{
    // Generate both getter and setter and stash them in the prototype.
    jsval roots[2] = { JSVAL_NULL, JSVAL_NULL };
    js::AutoArrayRooter tvr(cx, ArrayLength(roots), roots);

    uintN attrs = JSPROP_SHARED | (orig_attrs & JSPROP_ENUMERATE);
    JSObject *getterobj;
    if (getter) {
        getterobj = GeneratePropertyOp(cx, obj, id, 0, getter);
        if (!getterobj)
            return false;
        roots[0] = OBJECT_TO_JSVAL(getterobj);
        attrs |= JSPROP_GETTER;
    } else
        getterobj = nsnull;

    JSObject *setterobj;
    if (setter) {
        setterobj = GeneratePropertyOp(cx, obj, id, 1, setter);
        if (!setterobj)
            return false;
        roots[1] = OBJECT_TO_JSVAL(setterobj);
        attrs |= JSPROP_SETTER;
    } else
        setterobj = nsnull;

    if (getterobjp)
        *getterobjp = getterobj;
    if (setterobjp)
        *setterobjp = setterobj;
    return JS_DefinePropertyById(cx, obj, id, JSVAL_VOID,
                                 JS_DATA_TO_FUNC_PTR(JSPropertyOp, getterobj),
                                 JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, setterobj),
                                 attrs);
}

static JSBool
LookupGetterOrSetter(JSContext *cx, JSBool wantGetter, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    if (argc == 0) {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    jsval idval = JS_ARGV(cx, vp)[0];
    jsid id;
    JSPropertyDescriptor desc;
    if (!JS_ValueToId(cx, idval, &id) ||
        !JS_GetPropertyDescriptorById(cx, obj, id, JSRESOLVE_QUALIFIED, &desc))
        return false;

    // No property at all means no getters or setters possible.
    if (!desc.obj) {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }

    // Inline obj_lookup[GS]etter here.
    if (wantGetter) {
        if (desc.attrs & JSPROP_GETTER) {
            JS_SET_RVAL(cx, vp,
                        OBJECT_TO_JSVAL(JS_FUNC_TO_DATA_PTR(JSObject *, desc.getter)));
            return true;
        }
    } else {
        if (desc.attrs & JSPROP_SETTER) {
            JS_SET_RVAL(cx, vp,
                        OBJECT_TO_JSVAL(JS_FUNC_TO_DATA_PTR(JSObject *, desc.setter)));
            return true;
        }
    }

    // Since XPConnect doesn't use JSPropertyOps in any other contexts,
    // ensuring that we have an XPConnect prototype object ensures that
    // we are only going to expose quickstubbed properties to script.
    // Also be careful not to overwrite existing properties!

    if (!JSID_IS_STRING(id) ||
        !IS_PROTO_CLASS(js::GetObjectClass(desc.obj)) ||
        (desc.attrs & (JSPROP_GETTER | JSPROP_SETTER)) ||
        !(desc.getter || desc.setter) ||
        desc.setter == js::GetObjectJSClass(desc.obj)->setProperty) {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }

    JSObject *getterobj, *setterobj;
    if (!ReifyPropertyOps(cx, desc.obj, id, desc.attrs, desc.getter, desc.setter,
                          &getterobj, &setterobj)) {
        return false;
    }

    JSObject *wantedobj = wantGetter ? getterobj : setterobj;
    jsval v = wantedobj ? OBJECT_TO_JSVAL(wantedobj) : JSVAL_VOID;
    JS_SET_RVAL(cx, vp, v);
    return true;
}

static JSBool
SharedLookupGetter(JSContext *cx, uintN argc, jsval *vp)
{
    return LookupGetterOrSetter(cx, true, argc, vp);
}

static JSBool
SharedLookupSetter(JSContext *cx, uintN argc, jsval *vp)
{
    return LookupGetterOrSetter(cx, false, argc, vp);
}

static JSBool
DefineGetterOrSetter(JSContext *cx, uintN argc, JSBool wantGetter, jsval *vp)
{
    uintN attrs;
    JSBool found;
    JSPropertyOp getter;
    JSStrictPropertyOp setter;
    JSObject *obj2;
    jsval v;
    jsid id;

    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;
    JSNative forward = wantGetter ? js::obj_defineGetter : js::obj_defineSetter;
    jsval idval = (argc >= 1) ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    if (!JSVAL_IS_STRING(idval))
        return forward(cx, argc, vp);

    if (!JS_ValueToId(cx, idval, &id) ||
        !JS_LookupPropertyWithFlagsById(cx, obj, id,
                                        JSRESOLVE_QUALIFIED, &obj2, &v) ||
        (obj2 &&
         !JS_GetPropertyAttrsGetterAndSetterById(cx, obj2, id, &attrs,
                                                 &found, &getter, &setter)))
        return false;

    // The property didn't exist, already has a getter or setter, or is not
    // our property, then just forward now.
    if (!obj2 ||
        (attrs & (JSPROP_GETTER | JSPROP_SETTER)) ||
        !(getter || setter) ||
        !IS_PROTO_CLASS(js::GetObjectClass(obj2)))
        return forward(cx, argc, vp);

    // Reify the getter and setter...
    if (!ReifyPropertyOps(cx, obj2, id, attrs, getter, setter, nsnull, nsnull))
        return false;

    return forward(cx, argc, vp);
}

static JSBool
SharedDefineGetter(JSContext *cx, uintN argc, jsval *vp)
{
    return DefineGetterOrSetter(cx, argc, true, vp);
}

static JSBool
SharedDefineSetter(JSContext *cx, uintN argc, jsval *vp)
{
    return DefineGetterOrSetter(cx, argc, false, vp);
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
    bool definedProperty = false;
    for (uint32 i = ifacec; i-- != 0;) {
        const nsID &iid = *interfaces[i];
        const xpc_qsHashEntry *entry =
            LookupInterfaceOrAncestor(tableSize, table, iid);

        if (entry) {
            for (;;) {
                // Define quick stubs for attributes.
                const xpc_qsPropertySpec *ps = entry->properties;
                if (ps) {
                    for (; ps->name; ps++) {
                        definedProperty = true;
                        if (!JS_DefineProperty(cx, proto, ps->name, JSVAL_VOID,
                                               ps->getter, ps->setter,
                                               flags | JSPROP_SHARED))
                            return false;
                    }
                }

                // Define quick stubs for methods.
                const xpc_qsFunctionSpec *fs = entry->functions;
                if (fs) {
                    for (; fs->name; fs++) {
                        if (!JS_DefineFunction(cx, proto, fs->name,
                                               reinterpret_cast<JSNative>(fs->native),
                                               fs->arity, flags))
                            return false;
                    }
                }

                // Next.
                size_t j = entry->parentInterface;
                if (j == XPC_QS_NULL_INDEX)
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

    if (definedProperty && !JS_DefineFunctions(cx, proto, getterfns))
        return false;

    return true;
}

JSBool
xpc_qsThrow(JSContext *cx, nsresult rv)
{
    XPCThrower::Throw(rv, cx);
    return false;
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
GetMemberInfo(JSObject *obj, jsid memberId, const char **ifaceName)
{
    // Get the interface name.  From DefinePropertyIfFound (in
    // xpcwrappednativejsops.cpp) and XPCThrower::Verbosify.
    //
    // We could instead make the quick stub could pass in its interface name,
    // but this code often produces a more specific error message, e.g.
    *ifaceName = "Unknown";

    NS_ASSERTION(IS_WRAPPER_CLASS(js::GetObjectClass(obj)) ||
                 js::GetObjectClass(obj) == &XPC_WN_Tearoff_JSClass,
                 "obj must be a wrapper");
    XPCWrappedNativeProto *proto;
    if (IS_SLIM_WRAPPER(obj)) {
        proto = GetSlimWrapperProto(obj);
    } else {
        XPCWrappedNative *wrapper = (XPCWrappedNative *) js::GetObjectPrivate(obj);
        proto = wrapper->GetProto();
    }
    if (proto) {
        XPCNativeSet *set = proto->GetSet();
        if (set) {
            XPCNativeMember *member;
            XPCNativeInterface *iface;

            if (set->FindMember(memberId, &member, &iface))
                *ifaceName = iface->GetNameString();
        }
    }
}

static void
GetMethodInfo(JSContext *cx, jsval *vp, const char **ifaceNamep, jsid *memberIdp)
{
    JSObject *funobj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    NS_ASSERTION(JS_ObjectIsFunction(cx, funobj),
                 "JSNative callee should be Function object");
    JSString *str = JS_GetFunctionId(JS_GetObjectFunction(funobj));
    jsid methodId = str ? INTERNED_STRING_TO_JSID(cx, str) : JSID_VOID;
    GetMemberInfo(JSVAL_TO_OBJECT(vp[1]), methodId, ifaceNamep);
    *memberIdp = methodId;
}

static JSBool
ThrowCallFailed(JSContext *cx, nsresult rv,
                const char *ifaceName, jsid memberId, const char *memberName)
{
    /* Only one of memberId or memberName should be given. */
    JS_ASSERT(JSID_IS_VOID(memberId) != !memberName);

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
    if (XPCThrower::CheckForPendingException(rv, cx))
        return false;

    // else...

    if (!nsXPCException::NameAndFormatForNSResult(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE, nsnull, &format) ||
        !format) {
        format = "";
    }

    JSAutoByteString memberNameBytes;
    if (!memberName) {
        memberName = JSID_IS_STRING(memberId)
                     ? memberNameBytes.encode(cx, JSID_TO_STRING(memberId))
                     : "unknown";
    }
    if (nsXPCException::NameAndFormatForNSResult(rv, &name, nsnull)
        && name) {
        sz = JS_smprintf("%s 0x%x (%s) [%s.%s]",
                         format, rv, name, ifaceName, memberName);
    } else {
        sz = JS_smprintf("%s 0x%x [%s.%s]",
                         format, rv, ifaceName, memberName);
    }

    XPCThrower::BuildAndThrowException(cx, rv, sz);

    if (sz)
        JS_smprintf_free(sz);

    return false;
}

JSBool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv, JSObject *obj,
                              jsid memberId)
{
    const char *ifaceName;
    GetMemberInfo(obj, memberId, &ifaceName);
    return ThrowCallFailed(cx, rv, ifaceName, memberId, NULL);
}

JSBool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv, jsval *vp)
{
    const char *ifaceName;
    jsid memberId;
    GetMethodInfo(cx, vp, &ifaceName, &memberId);
    return ThrowCallFailed(cx, rv, ifaceName, memberId, NULL);
}

JSBool
xpc_qsThrowMethodFailedWithCcx(XPCCallContext &ccx, nsresult rv)
{
    ThrowBadResult(rv, ccx);
    return false;
}

void
xpc_qsThrowMethodFailedWithDetails(JSContext *cx, nsresult rv,
                                   const char *ifaceName,
                                   const char *memberName)
{
    ThrowCallFailed(cx, rv, ifaceName, JSID_VOID, memberName);
}

static void
ThrowBadArg(JSContext *cx, nsresult rv, const char *ifaceName,
            jsid memberId, const char *memberName, uintN paramnum)
{
    /* Only one memberId or memberName should be given. */
    JS_ASSERT(JSID_IS_VOID(memberId) != !memberName);

    // From XPCThrower::ThrowBadParam.
    char* sz;
    const char* format;

    if (!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";

    JSAutoByteString memberNameBytes;
    if (!memberName) {
        memberName = JSID_IS_STRING(memberId)
                     ? memberNameBytes.encode(cx, JSID_TO_STRING(memberId))
                     : "unknown";
    }
    sz = JS_smprintf("%s arg %u [%s.%s]",
                     format, (unsigned int) paramnum, ifaceName, memberName);

    XPCThrower::BuildAndThrowException(cx, rv, sz);

    if (sz)
        JS_smprintf_free(sz);
}

void
xpc_qsThrowBadArg(JSContext *cx, nsresult rv, jsval *vp, uintN paramnum)
{
    const char *ifaceName;
    jsid memberId;
    GetMethodInfo(cx, vp, &ifaceName, &memberId);
    ThrowBadArg(cx, rv, ifaceName, memberId, NULL, paramnum);
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
    ThrowBadArg(cx, rv, ifaceName, JSID_VOID, memberName, paramnum);
}

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv,
                          JSObject *obj, jsid propId)
{
    const char *ifaceName;
    GetMemberInfo(obj, propId, &ifaceName);
    ThrowBadArg(cx, rv, ifaceName, propId, NULL, 0);
}

JSBool
xpc_qsGetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
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
    typedef implementation_type::char_traits traits;
    // From the T_DOMSTRING case in XPCConvert::JSData2Native.
    JSString *s = InitOrStringify<traits>(cx, v, pval, nullBehavior,
                                          undefinedBehavior);
    if (!s)
        return;

    size_t len;
    const jschar *chars = JS_GetStringCharsZAndLength(cx, s, &len);
    if (!chars) {
        mValid = false;
        return;
    }

    new(mBuf) implementation_type(chars, len);
    mValid = true;
}

xpc_qsACString::xpc_qsACString(JSContext *cx, jsval v, jsval *pval,
                               StringificationBehavior nullBehavior,
                               StringificationBehavior undefinedBehavior)
{
    typedef implementation_type::char_traits traits;
    // From the T_CSTRING case in XPCConvert::JSData2Native.
    JSString *s = InitOrStringify<traits>(cx, v, pval, nullBehavior,
                                          undefinedBehavior);
    if (!s)
        return;

    size_t len = JS_GetStringEncodingLength(cx, s);
    if (len == size_t(-1)) {
        mValid = false;
        return;
    }

    JSAutoByteString bytes(cx, s);
    if (!bytes) {
        mValid = false;
        return;
    }

    new(mBuf) implementation_type(bytes.ptr(), len);
    mValid = true;
}

xpc_qsAUTF8String::xpc_qsAUTF8String(JSContext *cx, jsval v, jsval *pval)
{
    typedef nsCharTraits<PRUnichar> traits;
    // From the T_UTF8STRING  case in XPCConvert::JSData2Native.
    JSString *s = InitOrStringify<traits>(cx, v, pval, eNull, eNull);
    if (!s)
        return;

    size_t len;
    const PRUnichar *chars = JS_GetStringCharsZAndLength(cx, s, &len);
    if (!chars) {
        mValid = false;
        return;
    }

    new(mBuf) implementation_type(chars, len);
    mValid = true;
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
    if (entries) {
        for (QITableEntry* e = entries; e->iid; e++) {
            if (e->iid->Equals(iid)) {
                *ppThis = (char*) idobj + e->offset - entries[0].offset;
                *vp = OBJECT_TO_JSVAL(obj);
                *pThisRef = nsnull;
                return NS_OK;
            }
        }
    }

    nsresult rv = idobj->QueryInterface(iid, ppThis);
    *pThisRef = static_cast<nsISupports*>(*ppThis);
    if (NS_SUCCEEDED(rv))
        *vp = OBJECT_TO_JSVAL(obj);
    return rv;
}

inline nsresult
getNativeFromWrapper(JSContext *cx,
                     XPCWrappedNative *wrapper,
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
    if (XPCWrapper::IsSecurityWrapper(obj) &&
        !(obj = XPCWrapper::Unwrap(cx, obj))) {
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
    if (wrapper) {
        nsresult rv = getNativeFromWrapper(cx,wrapper, iid, ppThis, pThisRef,
                                           vp);

        if (lccx && NS_SUCCEEDED(rv))
            lccx->SetWrapper(wrapper, tearoff);

        if (rv != NS_ERROR_NO_INTERFACE)
            return rv;
    } else if (cur) {
        nsISupports *native;
        QITableEntry *entries;
        if (IS_SLIM_WRAPPER(cur)) {
            native = static_cast<nsISupports*>(xpc_GetJSPrivate(cur));
            entries = GetOffsetsFromSlimWrapper(cur);
        } else {
            NS_ABORT_IF_FALSE(mozilla::dom::binding::instanceIsProxy(cur),
                              "what kind of wrapper is this?");
            native = static_cast<nsISupports*>(js::GetProxyPrivate(cur).toPrivate());
            entries = nsnull;
        }

        if (NS_SUCCEEDED(getNative(native, entries, cur, iid, ppThis, pThisRef, vp))) {
            if (lccx) {
                // This only matters for unwrapping of this objects, so we
                // shouldn't end up here for the new DOM bindings.
                NS_ABORT_IF_FALSE(IS_SLIM_WRAPPER(cur),
                                  "what kind of wrapper is this?");
                lccx->SetWrapper(cur);
            }

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
    if (!native)
        return xpc_qsThrow(ccx.GetJSContext(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

    nsresult rv = getNative(native, GetOffsets(native, ccx.GetProto()),
                            ccx.GetFlattenedJSObject(), iid, ppThis, pThisRef,
                            vp);
    if (NS_FAILED(rv))
        return xpc_qsThrow(ccx.GetJSContext(), rv);
    return true;
}

JSObject*
xpc_qsUnwrapObj(jsval v, nsISupports **ppArgRef, nsresult *rv)
{
    if (JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v)) {
        *ppArgRef = nsnull;
        *rv = NS_OK;
        return nsnull;
    }

    if (!JSVAL_IS_OBJECT(v)) {
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
    if (!src) {
        *ppArg = nsnull;

        return rv;
    }

    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *obj2;
    if (mozilla::dom::binding::instanceIsProxy(src)) {
        wrapper = nsnull;
        obj2 = src;
    } else {
        rv = getWrapper(cx, src, nsnull, &wrapper, &obj2, &tearoff);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (wrapper || obj2) {
        if (NS_FAILED(castNative(cx, wrapper, obj2, tearoff, iid, ppArg,
                                 ppArgRef, vp, nsnull)))
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        return NS_OK;
    }
    // else...
    // Slow path.

    // XXX E4X breaks the world. Don't try wrapping E4X objects!
    // This hack can be removed (or changed accordingly) when the
    // DOM <-> E4X bindings are complete, see bug 270553
    if (JS_TypeOfValue(cx, OBJECT_TO_JSVAL(src)) == JSTYPE_XML) {
        *ppArgRef = nsnull;
        return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    // Try to unwrap a slim wrapper.
    nsISupports *iface;
    if (XPCConvert::GetISupportsFromJSObject(src, &iface)) {
        if (!iface || NS_FAILED(iface->QueryInterface(iid, ppArg))) {
            *ppArgRef = nsnull;
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        }

        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        return NS_OK;
    }

    // Create the ccx needed for quick stubs.
    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid()) {
        *ppArgRef = nsnull;
        return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    nsRefPtr<nsXPCWrappedJS> wrappedJS;
    rv = nsXPCWrappedJS::GetNewOrUsed(ccx, src, iid, nsnull,
                                      getter_AddRefs(wrappedJS));
    if (NS_FAILED(rv) || !wrappedJS) {
        *ppArgRef = nsnull;
        return rv;
    }

    // We need to go through the QueryInterface logic to make this return
    // the right thing for the various 'special' interfaces; e.g.
    // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
    // there is an outer to avoid nasty recursion.
    rv = wrappedJS->QueryInterface(iid, ppArg);
    if (NS_SUCCEEDED(rv)) {
        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        *vp = OBJECT_TO_JSVAL(wrappedJS->GetJSObject());
    }
    return rv;
}

JSBool
xpc_qsJsvalToCharStr(JSContext *cx, jsval v, JSAutoByteString *bytes)
{
    JSString *str;

    JS_ASSERT(!bytes->ptr());
    if (JSVAL_IS_STRING(v)) {
        str = JSVAL_TO_STRING(v);
    } else if (JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v)) {
        return true;
    } else {
        if (!(str = JS_ValueToString(cx, v)))
            return false;
    }
    return !!bytes->encode(cx, str);
}

JSBool
xpc_qsJsvalToWcharStr(JSContext *cx, jsval v, jsval *pval, const PRUnichar **pstr)
{
    JSString *str;

    if (JSVAL_IS_STRING(v)) {
        str = JSVAL_TO_STRING(v);
    } else if (JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v)) {
        *pstr = NULL;
        return true;
    } else {
        if (!(str = JS_ValueToString(cx, v)))
            return false;
        *pval = STRING_TO_JSVAL(str);  // Root the new string.
    }

    const jschar *chars = JS_GetStringCharsZ(cx, str);
    if (!chars)
        return false;

    *pstr = static_cast<const PRUnichar *>(chars);
    return true;
}

JSBool
xpc_qsStringToJsval(JSContext *cx, nsString &str, jsval *rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if (str.IsVoid()) {
        *rval = JSVAL_NULL;
        return true;
    }

    nsStringBuffer* sharedBuffer;
    jsval jsstr = XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer);
    if (JSVAL_IS_NULL(jsstr))
        return false;
    *rval = jsstr;
    if (sharedBuffer) {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return true;
}

JSBool
xpc_qsStringToJsstring(JSContext *cx, nsString &str, JSString **rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if (str.IsVoid()) {
        *rval = nsnull;
        return true;
    }

    nsStringBuffer* sharedBuffer;
    jsval jsstr = XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer);
    if (JSVAL_IS_NULL(jsstr))
        return false;
    *rval = JSVAL_TO_STRING(jsstr);
    if (sharedBuffer) {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return true;
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
    if (!XPCConvert::NativeInterface2JSObject(lccx, rval, nsnull,
                                              aHelper, iid, iface,
                                              true, OBJ_IS_NOT_GLOBAL, &rv)) {
        // I can't tell if NativeInterface2JSObject throws JS exceptions
        // or not.  This is a sloppy stab at the right semantics; the
        // method really ought to be fixed to behave consistently.
        if (!JS_IsExceptionPending(cx))
            xpc_qsThrow(cx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
        return false;
    }

#ifdef DEBUG
    JSObject* jsobj = JSVAL_TO_OBJECT(*rval);
    if (jsobj && !js::GetObjectParent(jsobj))
        NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                     "Why did we recreate this wrapper?");
#endif

    return true;
}

JSBool
xpc_qsVariantToJsval(XPCLazyCallContext &lccx,
                     nsIVariant *p,
                     jsval *rval)
{
    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // Error handling is in XPCWrappedNative::CallMethod.
    if (p) {
        nsresult rv;
        JSBool ok = XPCVariant::VariantDataToJS(lccx, p, &rv, rval);
        if (!ok)
            xpc_qsThrow(lccx.GetJSContext(), rv);
        return ok;
    }
    *rval = JSVAL_NULL;
    return true;
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
