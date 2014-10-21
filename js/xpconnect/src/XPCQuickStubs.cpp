/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "nsCOMPtr.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XPCInlines.h"
#include "XPCQuickStubs.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Exceptions.h"

using namespace mozilla;
using namespace JS;

extern const char* xpc_qsStringTable;

static const xpc_qsHashEntry *
LookupEntry(uint32_t tableSize, const xpc_qsHashEntry *table, const nsID &iid)
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
    return nullptr;
}

static const xpc_qsHashEntry *
LookupInterfaceOrAncestor(uint32_t tableSize, const xpc_qsHashEntry *table,
                          const nsID &iid)
{
    const xpc_qsHashEntry *entry = LookupEntry(tableSize, table, iid);
    if (!entry) {
        /*
         * On a miss, we have to search for every interface the object
         * supports, including ancestors.
         */
        nsCOMPtr<nsIInterfaceInfo> info;
        if (NS_FAILED(nsXPConnect::XPConnect()->GetInfoForIID(&iid, getter_AddRefs(info))))
            return nullptr;

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

static MOZ_ALWAYS_INLINE bool
HasBitInInterfacesBitmap(JSObject *obj, uint32_t interfaceBit)
{
    MOZ_ASSERT(IS_WN_REFLECTOR(obj), "Not a wrapper?");

    const XPCWrappedNativeJSClass *clasp =
      (const XPCWrappedNativeJSClass*)js::GetObjectClass(obj);
    return (clasp->interfacesBitmap & (1 << interfaceBit)) != 0;
}

bool
xpc_qsDefineQuickStubs(JSContext *cx, JSObject *protoArg, unsigned flags,
                       uint32_t ifacec, const nsIID **interfaces,
                       uint32_t tableSize, const xpc_qsHashEntry *table,
                       const xpc_qsPropertySpec *propspecs,
                       const xpc_qsFunctionSpec *funcspecs,
                       const char *stringTable)
{
    /*
     * Walk interfaces in reverse order to behave like XPConnect when a
     * feature is defined in more than one of the interfaces.
     *
     * XPCNativeSet::FindMethod returns the first matching feature it finds,
     * searching the interfaces forward.  Here, definitions toward the
     * front of 'interfaces' overwrite those toward the back.
     */
    RootedObject proto(cx, protoArg);
    for (uint32_t i = ifacec; i-- != 0;) {
        const nsID &iid = *interfaces[i];
        const xpc_qsHashEntry *entry =
            LookupInterfaceOrAncestor(tableSize, table, iid);

        if (entry) {
            for (;;) {
                // Define quick stubs for attributes.
                const xpc_qsPropertySpec *ps = propspecs + entry->prop_index;
                const xpc_qsPropertySpec *ps_end = ps + entry->n_props;
                for ( ; ps < ps_end; ++ps) {
                    if (!JS_DefineProperty(cx, proto,
                                           stringTable + ps->name_index,
                                           JS::UndefinedHandleValue,
                                           flags | JSPROP_SHARED | JSPROP_NATIVE_ACCESSORS,
                                           (JSPropertyOp)ps->getter,
                                           (JSStrictPropertyOp)ps->setter))
                        return false;
                }

                // Define quick stubs for methods.
                const xpc_qsFunctionSpec *fs = funcspecs + entry->func_index;
                const xpc_qsFunctionSpec *fs_end = fs + entry->n_funcs;
                for ( ; fs < fs_end; ++fs) {
                    if (!JS_DefineFunction(cx, proto,
                                           stringTable + fs->name_index,
                                           reinterpret_cast<JSNative>(fs->native),
                                           fs->arity, flags))
                        return false;
                }

                if (entry->newBindingProperties) {
                    if (entry->newBindingProperties->regular) {
                        mozilla::dom::DefineWebIDLBindingPropertiesOnXPCObject(cx, proto, entry->newBindingProperties->regular);
                    }
                    if (entry->newBindingProperties->chromeOnly &&
                        xpc::AccessCheck::isChrome(js::GetContextCompartment(cx))) {
                        mozilla::dom::DefineWebIDLBindingPropertiesOnXPCObject(cx, proto, entry->newBindingProperties->chromeOnly);
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

    return true;
}

bool
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
    *ifaceName = "Unknown";

    // Don't try to generate a useful name if there are security wrappers,
    // because it isn't worth the risk of something going wrong just to generate
    // an error message. Instead, only handle the simple case where we have the
    // reflector in hand.
    if (IS_WN_REFLECTOR(obj)) {
        XPCWrappedNative *wrapper = XPCWrappedNative::Get(obj);
        XPCWrappedNativeProto *proto = wrapper->GetProto();
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
}

static void
GetMethodInfo(JSContext *cx, jsval *vp, const char **ifaceNamep, jsid *memberIdp)
{
    CallReceiver call = CallReceiverFromVp(vp);
    RootedObject funobj(cx, &call.callee());
    MOZ_ASSERT(JS_ObjectIsFunction(cx, funobj),
               "JSNative callee should be Function object");
    RootedString str(cx, JS_GetFunctionId(JS_GetObjectFunction(funobj)));
    RootedId methodId(cx, str ? INTERNED_STRING_TO_JSID(cx, str) : JSID_VOID);
    GetMemberInfo(&call.thisv().toObject(), methodId, ifaceNamep);
    *memberIdp = methodId;
}

static bool
ThrowCallFailed(JSContext *cx, nsresult rv,
                const char *ifaceName, HandleId memberId, const char *memberName)
{
    /* Only one of memberId or memberName should be given. */
    MOZ_ASSERT(JSID_IS_VOID(memberId) != !memberName);

    // From XPCThrower::ThrowBadResult.
    char* sz;
    const char* format;
    const char* name;

    // If the cx already has a pending exception, just throw that.
    //
    // We used to check here to make sure the exception matched rv (whatever
    // that means). But this meant that we'd be calling into JSAPI below with
    // a pending exception, which isn't really kosher. The first exception thrown
    // should generally take precedence anyway.
    if (JS_IsExceptionPending(cx))
        return false;

    // else...

    if (!nsXPCException::NameAndFormatForNSResult(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE, nullptr, &format) ||
        !format) {
        format = "";
    }

    JSAutoByteString memberNameBytes;
    if (!memberName) {
        memberName = JSID_IS_STRING(memberId)
                     ? memberNameBytes.encodeLatin1(cx, JSID_TO_STRING(memberId))
                     : "unknown";
    }
    if (nsXPCException::NameAndFormatForNSResult(rv, &name, nullptr)
        && name) {
        sz = JS_smprintf("%s 0x%x (%s) [%s.%s]",
                         format, rv, name, ifaceName, memberName);
    } else {
        sz = JS_smprintf("%s 0x%x [%s.%s]",
                         format, rv, ifaceName, memberName);
    }

    dom::Throw(cx, rv, sz);

    if (sz)
        JS_smprintf_free(sz);

    return false;
}

bool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv, JSObject *obj,
                              jsid memberIdArg)
{
    RootedId memberId(cx, memberIdArg);
    const char *ifaceName;
    GetMemberInfo(obj, memberId, &ifaceName);
    return ThrowCallFailed(cx, rv, ifaceName, memberId, nullptr);
}

bool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv, JSObject *objArg,
                              const char* memberName)
{
    RootedObject obj(cx, objArg);
    JSString *str = JS_InternString(cx, memberName);
    if (!str) {
        return false;
    }
    return xpc_qsThrowGetterSetterFailed(cx, rv, obj,
                                         INTERNED_STRING_TO_JSID(cx, str));
}

bool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv, JSObject *obj,
                              uint16_t memberIndex)
{
    return xpc_qsThrowGetterSetterFailed(cx, rv, obj,
                                         xpc_qsStringTable + memberIndex);
}

bool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv, jsval *vp)
{
    const char *ifaceName;
    RootedId memberId(cx);
    GetMethodInfo(cx, vp, &ifaceName, memberId.address());
    return ThrowCallFailed(cx, rv, ifaceName, memberId, nullptr);
}

static void
ThrowBadArg(JSContext *cx, nsresult rv, const char *ifaceName,
            jsid memberId, const char *memberName, unsigned paramnum)
{
    /* Only one memberId or memberName should be given. */
    MOZ_ASSERT(JSID_IS_VOID(memberId) != !memberName);

    // From XPCThrower::ThrowBadParam.
    char* sz;
    const char* format;

    if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &format))
        format = "";

    JSAutoByteString memberNameBytes;
    if (!memberName) {
        memberName = JSID_IS_STRING(memberId)
                     ? memberNameBytes.encodeLatin1(cx, JSID_TO_STRING(memberId))
                     : "unknown";
    }
    sz = JS_smprintf("%s arg %u [%s.%s]",
                     format, (unsigned int) paramnum, ifaceName, memberName);

    dom::Throw(cx, rv, sz);

    if (sz)
        JS_smprintf_free(sz);
}

void
xpc_qsThrowBadArg(JSContext *cx, nsresult rv, jsval *vp, unsigned paramnum)
{
    const char *ifaceName;
    RootedId memberId(cx);
    GetMethodInfo(cx, vp, &ifaceName, memberId.address());
    ThrowBadArg(cx, rv, ifaceName, memberId, nullptr, paramnum);
}

void
xpc_qsThrowBadArgWithCcx(XPCCallContext &ccx, nsresult rv, unsigned paramnum)
{
    XPCThrower::ThrowBadParam(rv, paramnum, ccx);
}

void
xpc_qsThrowBadArgWithDetails(JSContext *cx, nsresult rv, unsigned paramnum,
                             const char *ifaceName, const char *memberName)
{
    ThrowBadArg(cx, rv, ifaceName, JSID_VOID, memberName, paramnum);
}

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv,
                          JSObject *obj, jsid propIdArg)
{
    RootedId propId(cx, propIdArg);
    const char *ifaceName;
    GetMemberInfo(obj, propId, &ifaceName);
    ThrowBadArg(cx, rv, ifaceName, propId, nullptr, 0);
}

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv,
                          JSObject *objArg, const char* propName)
{
    RootedObject obj(cx, objArg);
    JSString *str = JS_InternString(cx, propName);
    if (!str) {
        return;
    }
    xpc_qsThrowBadSetterValue(cx, rv, obj, INTERNED_STRING_TO_JSID(cx, str));
}

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv, JSObject *obj,
                          uint16_t name_index)
{
    xpc_qsThrowBadSetterValue(cx, rv, obj, xpc_qsStringTable + name_index);
}

bool
xpc_qsGetterOnlyNativeStub(JSContext *cx, unsigned argc, jsval *vp)
{
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT |
                                        JSREPORT_STRICT_MODE_ERROR,
                                        js_GetErrorMessage, nullptr,
                                        JSMSG_GETTER_ONLY);
}

xpc_qsDOMString::xpc_qsDOMString(JSContext *cx, HandleValue v,
                                 MutableHandleValue pval, bool notpassed,
                                 StringificationBehavior nullBehavior,
                                 StringificationBehavior undefinedBehavior)
{
    typedef implementation_type::char_traits traits;
    // From the T_DOMSTRING case in XPCConvert::JSData2Native.
    JSString *s = InitOrStringify<traits>(cx, v,
                                          pval, notpassed,
                                          nullBehavior,
                                          undefinedBehavior);
    if (!s)
        return;

    nsAutoString *str = new(mBuf) implementation_type();

    mValid = AssignJSString(cx, *str, s);
}

xpc_qsACString::xpc_qsACString(JSContext *cx, HandleValue v,
                               MutableHandleValue pval, bool notpassed,
                               StringificationBehavior nullBehavior,
                               StringificationBehavior undefinedBehavior)
{
    typedef implementation_type::char_traits traits;
    // From the T_CSTRING case in XPCConvert::JSData2Native.
    JSString *s = InitOrStringify<traits>(cx, v,
                                          pval, notpassed,
                                          nullBehavior,
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

xpc_qsAUTF8String::xpc_qsAUTF8String(JSContext *cx, HandleValue v, MutableHandleValue pval, bool notpassed)
{
    typedef nsCharTraits<char16_t> traits;
    // From the T_UTF8STRING  case in XPCConvert::JSData2Native.
    JSString *s = InitOrStringify<traits>(cx, v, pval, notpassed, eNull, eNull);
    if (!s)
        return;

    nsAutoJSString str;
    if (!str.init(cx, s)) {
        mValid = false;
        return;
    }

    new(mBuf) implementation_type(str);
    mValid = true;
}

static nsresult
getNative(nsISupports *idobj,
          HandleObject obj,
          const nsIID &iid,
          void **ppThis,
          nsISupports **pThisRef,
          jsval *vp)
{
    nsresult rv = idobj->QueryInterface(iid, ppThis);
    *pThisRef = static_cast<nsISupports*>(*ppThis);
    if (NS_SUCCEEDED(rv))
        *vp = OBJECT_TO_JSVAL(obj);
    return rv;
}

static inline nsresult
getNativeFromWrapper(JSContext *cx,
                     XPCWrappedNative *wrapper,
                     const nsIID &iid,
                     void **ppThis,
                     nsISupports **pThisRef,
                     jsval *vp)
{
    RootedObject obj(cx, wrapper->GetFlatJSObject());
    return getNative(wrapper->GetIdentityObject(), obj, iid, ppThis, pThisRef,
                     vp);
}


nsresult
getWrapper(JSContext *cx,
           JSObject *obj,
           XPCWrappedNative **wrapper,
           JSObject **cur,
           XPCWrappedNativeTearOff **tearoff)
{
    // We can have at most three layers in need of unwrapping here:
    // * A (possible) security wrapper
    // * A (possible) Xray waiver
    // * A (possible) outer window
    //
    // If we pass stopAtOuter == false, we can handle all three with one call
    // to js::CheckedUnwrap.
    if (js::IsWrapper(obj)) {
        JSObject* inner = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);

        // The safe unwrap might have failed if we encountered an object that
        // we're not allowed to unwrap. If it didn't fail though, we should be
        // done with wrappers.
        if (!inner)
            return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
        MOZ_ASSERT(!js::IsWrapper(inner));

        obj = inner;
    }

    // Start with sane values.
    *wrapper = nullptr;
    *cur = nullptr;
    *tearoff = nullptr;

    if (dom::IsDOMObject(obj)) {
        *cur = obj;

        return NS_OK;
    }

    // Handle tearoffs.
    //
    // If |obj| is of the tearoff class, that means we're dealing with a JS
    // object reflection of a particular interface (ie, |foo.nsIBar|). These
    // JS objects are parented to their wrapper, so we snag the tearoff object
    // along the way (if desired), and then set |obj| to its parent.
    const js::Class* clasp = js::GetObjectClass(obj);
    if (clasp == &XPC_WN_Tearoff_JSClass) {
        *tearoff = (XPCWrappedNativeTearOff*) js::GetObjectPrivate(obj);
        obj = js::GetObjectParent(obj);
    }

    // If we've got a WN, store things the way callers expect. Otherwise, leave
    // things null and return.
    if (IS_WN_CLASS(clasp))
        *wrapper = XPCWrappedNative::Get(obj);

    return NS_OK;
}

nsresult
castNative(JSContext *cx,
           XPCWrappedNative *wrapper,
           JSObject *curArg,
           XPCWrappedNativeTearOff *tearoff,
           const nsIID &iid,
           void **ppThis,
           nsISupports **pThisRef,
           MutableHandleValue vp)
{
    RootedObject cur(cx, curArg);
    if (wrapper) {
        nsresult rv = getNativeFromWrapper(cx,wrapper, iid, ppThis, pThisRef,
                                           vp.address());

        if (rv != NS_ERROR_NO_INTERFACE)
            return rv;
    } else if (cur) {
        nsISupports *native;
        if (!(native = mozilla::dom::UnwrapDOMObjectToISupports(cur))) {
            *pThisRef = nullptr;
            return NS_ERROR_ILLEGAL_VALUE;
        }

        if (NS_SUCCEEDED(getNative(native, cur, iid, ppThis, pThisRef, vp.address()))) {
            return NS_OK;
        }
    }

    *pThisRef = nullptr;
    return NS_ERROR_XPC_BAD_OP_ON_WN_PROTO;
}

nsISupports*
castNativeFromWrapper(JSContext *cx,
                      JSObject *obj,
                      uint32_t interfaceBit,
                      uint32_t protoID,
                      int32_t protoDepth,
                      nsISupports **pRef,
                      MutableHandleValue pVal,
                      nsresult *rv)
{
    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *cur;

    if (IS_WN_REFLECTOR(obj)) {
        cur = obj;
        wrapper = XPCWrappedNative::Get(obj);
        tearoff = nullptr;
    } else {
        *rv = getWrapper(cx, obj, &wrapper, &cur, &tearoff);
        if (NS_FAILED(*rv))
            return nullptr;
    }

    nsISupports *native;
    if (wrapper) {
        native = wrapper->GetIdentityObject();
        cur = wrapper->GetFlatJSObject();
        if (!native || !HasBitInInterfacesBitmap(cur, interfaceBit)) {
            native = nullptr;
        }
    } else if (cur && protoDepth >= 0) {
        const mozilla::dom::DOMJSClass* domClass =
            mozilla::dom::GetDOMClass(cur);
        native = mozilla::dom::UnwrapDOMObject<nsISupports>(cur);
        if (native &&
            (uint32_t)domClass->mInterfaceChain[protoDepth] != protoID) {
            native = nullptr;
        }
    } else {
        native = nullptr;
    }

    if (native) {
        *pRef = nullptr;
        pVal.setObjectOrNull(cur);
        *rv = NS_OK;
    } else {
        *rv = NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    return native;
}

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx,
                    HandleValue v,
                    const nsIID &iid,
                    void **ppArg,
                    nsISupports **ppArgRef,
                    MutableHandleValue vp)
{
    nsresult rv;
    RootedObject src(cx, xpc_qsUnwrapObj(v, ppArgRef, &rv));
    if (!src) {
        *ppArg = nullptr;

        return rv;
    }

    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *obj2;
    rv = getWrapper(cx, src, &wrapper, &obj2, &tearoff);
    NS_ENSURE_SUCCESS(rv, rv);

    if (wrapper || obj2) {
        if (NS_FAILED(castNative(cx, wrapper, obj2, tearoff, iid, ppArg,
                                 ppArgRef, vp)))
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        return NS_OK;
    }
    // else...
    // Slow path.

    // Try to unwrap a slim wrapper.
    nsISupports *iface;
    if (XPCConvert::GetISupportsFromJSObject(src, &iface)) {
        if (!iface || NS_FAILED(iface->QueryInterface(iid, ppArg))) {
            *ppArgRef = nullptr;
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        }

        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        return NS_OK;
    }

    // Create the ccx needed for quick stubs.
    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid()) {
        *ppArgRef = nullptr;
        return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    nsRefPtr<nsXPCWrappedJS> wrappedJS;
    rv = nsXPCWrappedJS::GetNewOrUsed(src, iid, getter_AddRefs(wrappedJS));
    if (NS_FAILED(rv) || !wrappedJS) {
        *ppArgRef = nullptr;
        return rv;
    }

    // We need to go through the QueryInterface logic to make this return
    // the right thing for the various 'special' interfaces; e.g.
    // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
    // there is an outer to avoid nasty recursion.
    rv = wrappedJS->QueryInterface(iid, ppArg);
    if (NS_SUCCEEDED(rv)) {
        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        vp.setObjectOrNull(wrappedJS->GetJSObject());
    }
    return rv;
}

bool
xpc_qsJsvalToCharStr(JSContext *cx, HandleValue v, JSAutoByteString *bytes)
{
    MOZ_ASSERT(!bytes->ptr());

    if (v.isNullOrUndefined())
      return true;

    JSString *str = ToString(cx, v);
    if (!str)
      return false;
    return !!bytes->encodeLatin1(cx, str);
}

namespace xpc {

bool
NonVoidStringToJsval(JSContext *cx, nsAString &str, MutableHandleValue rval)
{
    nsStringBuffer* sharedBuffer;
    if (!XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer, rval))
      return false;

    if (sharedBuffer) {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return true;
}

} // namespace xpc

bool
xpc_qsXPCOMObjectToJsval(JSContext *cx, qsObjectHelper &aHelper,
                         const nsIID *iid, XPCNativeInterface **iface,
                         MutableHandleValue rval)
{
    NS_PRECONDITION(iface, "Who did that and why?");

    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // This is one of the slowest things quick stubs do.

    nsresult rv;
    if (!XPCConvert::NativeInterface2JSObject(rval, nullptr,
                                              aHelper, iid, iface,
                                              true, &rv)) {
        // I can't tell if NativeInterface2JSObject throws JS exceptions
        // or not.  This is a sloppy stab at the right semantics; the
        // method really ought to be fixed to behave consistently.
        if (!JS_IsExceptionPending(cx))
            xpc_qsThrow(cx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
        return false;
    }

#ifdef DEBUG
    JSObject* jsobj = rval.toObjectOrNull();
    if (jsobj && !js::GetObjectParent(jsobj))
        MOZ_ASSERT(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                   "Why did we recreate this wrapper?");
#endif

    return true;
}

bool
xpc_qsVariantToJsval(JSContext *aCx,
                     nsIVariant *p,
                     MutableHandleValue rval)
{
    // From the T_INTERFACE case in XPCConvert::NativeData2JS.
    // Error handling is in XPCWrappedNative::CallMethod.
    if (p) {
        nsresult rv;
        bool ok = XPCVariant::VariantDataToJS(p, &rv, rval);
        if (!ok)
            xpc_qsThrow(aCx, rv);
        return ok;
    }
    rval.setNull();
    return true;
}

#ifdef DEBUG
void
xpc_qsAssertContextOK(JSContext *cx)
{
    XPCJSContextStack* stack = XPCJSRuntime::Get()->GetJSContextStack();

    JSContext *topJSContext = stack->Peek();

    // This is what we're actually trying to assert here.
    MOZ_ASSERT(cx == topJSContext, "wrong context on XPCJSContextStack!");
}
#endif
