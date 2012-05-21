/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript JSClasses and JSOps for our Wrapped Native JS Objects. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/BindingUtils.h"

/***************************************************************************/

// All of the exceptions thrown into JS from this file go through here.
// That makes this a nice place to set a breakpoint.

static JSBool Throw(unsigned errNum, JSContext* cx)
{
    XPCThrower::Throw(errNum, cx);
    return false;
}

// Handy macro used in many callback stub below.

#define MORPH_SLIM_WRAPPER(cx, obj)                                           \
    PR_BEGIN_MACRO                                                            \
    SLIM_LOG_WILL_MORPH(cx, obj);                                             \
    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))                   \
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);                    \
    PR_END_MACRO

#define THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper)                          \
    PR_BEGIN_MACRO                                                            \
    if (!wrapper)                                                             \
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);                    \
    if (!wrapper->IsValid())                                                  \
        return Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);                     \
    PR_END_MACRO

/***************************************************************************/

static JSBool
ToStringGuts(XPCCallContext& ccx)
{
    char* sz;
    XPCWrappedNative* wrapper = ccx.GetWrapper();

    if (wrapper)
        sz = wrapper->ToString(ccx, ccx.GetTearOff());
    else
        sz = JS_smprintf("[xpconnect wrapped native prototype]");

    if (!sz) {
        JS_ReportOutOfMemory(ccx);
        return false;
    }

    JSString* str = JS_NewStringCopyZ(ccx, sz);
    JS_smprintf_free(sz);
    if (!str)
        return false;

    ccx.SetRetVal(STRING_TO_JSVAL(str));
    return true;
}

/***************************************************************************/

static JSBool
XPC_WN_Shared_ToString(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    if (IS_SLIM_WRAPPER(obj)) {
        XPCNativeScriptableInfo *si =
            GetSlimWrapperProto(obj)->GetScriptableInfo();
#ifdef DEBUG
#  define FMT_ADDR " @ 0x%p"
#  define FMT_STR(str) str
#  define PARAM_ADDR(w) , w
#else
#  define FMT_ADDR ""
#  define FMT_STR(str)
#  define PARAM_ADDR(w)
#endif
        char *sz = JS_smprintf("[object %s" FMT_ADDR FMT_STR(" (native") FMT_ADDR FMT_STR(")") "]", si->GetJSClass()->name PARAM_ADDR(obj) PARAM_ADDR(xpc_GetJSPrivate(obj)));
        if (!sz)
            return false;

        JSString* str = JS_NewStringCopyZ(cx, sz);
        JS_smprintf_free(sz);
        if (!str)
            return false;

        *vp = STRING_TO_JSVAL(str);

        return true;
    }

    XPCCallContext ccx(JS_CALLER, cx, obj);
    ccx.SetName(ccx.GetRuntime()->GetStringID(XPCJSRuntime::IDX_TO_STRING));
    ccx.SetArgsAndResultPtr(argc, JS_ARGV(cx, vp), vp);
    return ToStringGuts(ccx);
}

static JSBool
XPC_WN_Shared_ToSource(JSContext *cx, unsigned argc, jsval *vp)
{
    static const char empty[] = "({})";
    JSString *str = JS_NewStringCopyN(cx, empty, sizeof(empty)-1);
    if (!str)
        return false;
    *vp = STRING_TO_JSVAL(str);

    return true;
}

/***************************************************************************/

// A "double wrapped object" is a user JSObject that has been wrapped as a
// wrappedJS in order to be used by native code and then re-wrapped by a
// wrappedNative wrapper to be used by JS code. One might think of it as:
//    wrappedNative(wrappedJS(underlying_JSObject))
// This is done (as opposed to just unwrapping the wrapped JS and automatically
// returning the underlying JSObject) so that JS callers will see what looks
// Like any other xpcom object - and be limited to use its interfaces.
//
// See the comment preceding nsIXPCWrappedJSObjectGetter in nsIXPConnect.idl.

static JSObject*
GetDoubleWrappedJSObject(XPCCallContext& ccx, XPCWrappedNative* wrapper)
{
    JSObject* obj = nsnull;
    nsCOMPtr<nsIXPConnectWrappedJS>
        underware = do_QueryInterface(wrapper->GetIdentityObject());
    if (underware) {
        JSObject* mainObj = nsnull;
        if (NS_SUCCEEDED(underware->GetJSObject(&mainObj)) && mainObj) {
            jsid id = ccx.GetRuntime()->
                    GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);

            JSAutoEnterCompartment ac;
            if (!ac.enter(ccx, mainObj))
                return NULL;

            jsval val;
            if (JS_GetPropertyById(ccx, mainObj, id, &val) &&
                !JSVAL_IS_PRIMITIVE(val)) {
                obj = JSVAL_TO_OBJECT(val);
            }
        }
    }
    return obj;
}

// This is the getter native function we use to handle 'wrappedJSObject' for
// double wrapped JSObjects.

static JSBool
XPC_WN_DoubleWrappedGetter(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    NS_ASSERTION(JS_TypeOfValue(cx, JS_CALLEE(cx, vp)) == JSTYPE_FUNCTION, "bad function");

    JSObject* realObject = GetDoubleWrappedJSObject(ccx, wrapper);
    if (!realObject) {
        // This is pretty unexpected at this point. The object originally
        // responded to this get property call and now gives no object.
        // XXX Should this throw something at the caller?
        *vp = JSVAL_NULL;
        return true;
    }

    // It is a double wrapped object. Figure out if the caller
    // is allowed to see it.

    nsIXPCSecurityManager* sm;
    XPCContext* xpcc = ccx.GetXPCContext();

    sm = xpcc->GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_GET_PROPERTY);
    if (sm) {
        AutoMarkingNativeInterfacePtr iface(ccx);
        iface = XPCNativeInterface::
                    GetNewOrUsed(ccx, &NS_GET_IID(nsIXPCWrappedJSObjectGetter));

        if (iface) {
            jsid id = ccx.GetRuntime()->
                        GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);

            ccx.SetCallInfo(iface, iface->GetMemberAt(1), false);
            if (NS_FAILED(sm->
                          CanAccess(nsIXPCSecurityManager::ACCESS_GET_PROPERTY,
                                    &ccx, ccx,
                                    ccx.GetFlattenedJSObject(),
                                    wrapper->GetIdentityObject(),
                                    wrapper->GetClassInfo(), id,
                                    wrapper->GetSecurityInfoAddr()))) {
                // The SecurityManager should have set an exception.
                return false;
            }
        }
    }
    *vp = OBJECT_TO_JSVAL(realObject);
    return JS_WrapValue(cx, vp);
}

/***************************************************************************/

// This is our shared function to define properties on our JSObjects.

/*
 * NOTE:
 * We *never* set the tearoff names (e.g. nsIFoo) as JS_ENUMERATE.
 * We *never* set toString or toSource as JS_ENUMERATE.
 */

static JSBool
DefinePropertyIfFound(XPCCallContext& ccx,
                      JSObject *obj, jsid id,
                      XPCNativeSet* set,
                      XPCNativeInterface* iface,
                      XPCNativeMember* member,
                      XPCWrappedNativeScope* scope,
                      JSBool reflectToStringAndToSource,
                      XPCWrappedNative* wrapperToReflectInterfaceNames,
                      XPCWrappedNative* wrapperToReflectDoubleWrap,
                      XPCNativeScriptableInfo* scriptableInfo,
                      unsigned propFlags,
                      JSBool* resolved)
{
    XPCJSRuntime* rt = ccx.GetRuntime();
    JSBool found;
    const char* name;

    if (set) {
        if (iface)
            found = true;
        else
            found = set->FindMember(id, &member, &iface);
    } else
        found = (nsnull != (member = iface->FindMember(id)));

    if (!found) {
        if (reflectToStringAndToSource) {
            JSNative call;

            if (id == rt->GetStringID(XPCJSRuntime::IDX_TO_STRING)) {
                call = XPC_WN_Shared_ToString;
                name = rt->GetStringName(XPCJSRuntime::IDX_TO_STRING);
                id   = rt->GetStringID(XPCJSRuntime::IDX_TO_STRING);
            } else if (id == rt->GetStringID(XPCJSRuntime::IDX_TO_SOURCE)) {
                call = XPC_WN_Shared_ToSource;
                name = rt->GetStringName(XPCJSRuntime::IDX_TO_SOURCE);
                id   = rt->GetStringID(XPCJSRuntime::IDX_TO_SOURCE);
            }

            else
                call = nsnull;

            if (call) {
                JSFunction* fun = JS_NewFunction(ccx, call, 0, 0, obj, name);
                if (!fun) {
                    JS_ReportOutOfMemory(ccx);
                    return false;
                }

                AutoResolveName arn(ccx, id);
                if (resolved)
                    *resolved = true;
                return JS_DefinePropertyById(ccx, obj, id,
                                             OBJECT_TO_JSVAL(JS_GetFunctionObject(fun)),
                                             nsnull, nsnull,
                                             propFlags & ~JSPROP_ENUMERATE);
            }
        }
        // This *might* be a tearoff name that is not yet part of our
        // set. Let's lookup the name and see if it is the name of an
        // interface. Then we'll see if the object actually *does* this
        // interface and add a tearoff as necessary.

        if (wrapperToReflectInterfaceNames) {
            JSAutoByteString name;
            AutoMarkingNativeInterfacePtr iface2(ccx);
            XPCWrappedNativeTearOff* to;
            JSObject* jso;
            nsresult rv = NS_OK;

            if (JSID_IS_STRING(id) &&
                name.encode(ccx, JSID_TO_STRING(id)) &&
                (iface2 = XPCNativeInterface::GetNewOrUsed(ccx, name.ptr()), iface2) &&
                nsnull != (to = wrapperToReflectInterfaceNames->
                           FindTearOff(ccx, iface2, true, &rv)) &&
                nsnull != (jso = to->GetJSObject()))

            {
                AutoResolveName arn(ccx, id);
                if (resolved)
                    *resolved = true;
                return JS_DefinePropertyById(ccx, obj, id, OBJECT_TO_JSVAL(jso),
                                             nsnull, nsnull,
                                             propFlags & ~JSPROP_ENUMERATE);
            } else if (NS_FAILED(rv) && rv != NS_ERROR_NO_INTERFACE) {
                return Throw(rv, ccx);
            }
        }

        // This *might* be a double wrapped JSObject
        if (wrapperToReflectDoubleWrap &&
            id == rt->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT) &&
            GetDoubleWrappedJSObject(ccx, wrapperToReflectDoubleWrap)) {
            // We build and add a getter function.
            // A security check is done on a per-get basis.

            JSFunction* fun;

            id = rt->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);
            name = rt->GetStringName(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);

            fun = JS_NewFunction(ccx, XPC_WN_DoubleWrappedGetter,
                                 0, 0, obj, name);

            if (!fun)
                return false;

            JSObject* funobj = JS_GetFunctionObject(fun);
            if (!funobj)
                return false;

            propFlags |= JSPROP_GETTER;
            propFlags &= ~JSPROP_ENUMERATE;

            AutoResolveName arn(ccx, id);
            if (resolved)
                *resolved = true;
            return JS_DefinePropertyById(ccx, obj, id, JSVAL_VOID,
                                         JS_DATA_TO_FUNC_PTR(JSPropertyOp,
                                                             funobj),
                                         nsnull, propFlags);
        }

        if (resolved)
            *resolved = false;
        return true;
    }

    if (!member) {
        if (wrapperToReflectInterfaceNames) {
            XPCWrappedNativeTearOff* to =
              wrapperToReflectInterfaceNames->FindTearOff(ccx, iface, true);

            if (!to)
                return false;
            JSObject* jso = to->GetJSObject();
            if (!jso)
                return false;

            AutoResolveName arn(ccx, id);
            if (resolved)
                *resolved = true;
            return JS_DefinePropertyById(ccx, obj, id, OBJECT_TO_JSVAL(jso),
                                         nsnull, nsnull,
                                         propFlags & ~JSPROP_ENUMERATE);
        }
        if (resolved)
            *resolved = false;
        return true;
    }

    if (member->IsConstant()) {
        jsval val;
        AutoResolveName arn(ccx, id);
        if (resolved)
            *resolved = true;
        return member->GetConstantValue(ccx, iface, &val) &&
               JS_DefinePropertyById(ccx, obj, id, val, nsnull, nsnull,
                                     propFlags);
    }

    if (id == rt->GetStringID(XPCJSRuntime::IDX_TO_STRING) ||
        id == rt->GetStringID(XPCJSRuntime::IDX_TO_SOURCE) ||
        (scriptableInfo &&
         scriptableInfo->GetFlags().DontEnumQueryInterface() &&
         id == rt->GetStringID(XPCJSRuntime::IDX_QUERY_INTERFACE)))
        propFlags &= ~JSPROP_ENUMERATE;

    jsval funval;
    if (!member->NewFunctionObject(ccx, iface, obj, &funval))
        return false;

    // protect funobj until it is actually attached
    AUTO_MARK_JSVAL(ccx, funval);

#ifdef off_DEBUG_jband
    {
        static int cloneCount = 0;
        if (!(++cloneCount%10))
            printf("<><><> %d cloned functions created\n", cloneCount);
    }
#endif

    if (member->IsMethod()) {
        AutoResolveName arn(ccx, id);
        if (resolved)
            *resolved = true;
        return JS_DefinePropertyById(ccx, obj, id, funval, nsnull, nsnull,
                                     propFlags);
    }

    // else...

    NS_ASSERTION(member->IsAttribute(), "way broken!");

    propFlags |= JSPROP_GETTER | JSPROP_SHARED;
    JSObject* funobj = JSVAL_TO_OBJECT(funval);
    JSPropertyOp getter = JS_DATA_TO_FUNC_PTR(JSPropertyOp, funobj);
    JSStrictPropertyOp setter;
    if (member->IsWritableAttribute()) {
        propFlags |= JSPROP_SETTER;
        propFlags &= ~JSPROP_READONLY;
        setter = JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, funobj);
    } else {
        setter = js_GetterOnlyPropertyStub;
    }

    AutoResolveName arn(ccx, id);
    if (resolved)
        *resolved = true;

    return JS_DefinePropertyById(ccx, obj, id, JSVAL_VOID, getter, setter,
                                 propFlags);
}

/***************************************************************************/
/***************************************************************************/

static JSBool
XPC_WN_OnlyIWrite_AddPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull, id);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    // Allow only XPConnect to add/set the property
    if (ccx.GetResolveName() == id)
        return true;

    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

static JSBool
XPC_WN_OnlyIWrite_SetPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict, jsval *vp)
{
    return XPC_WN_OnlyIWrite_AddPropertyStub(cx, obj, id, vp);
}

static JSBool
XPC_WN_CannotModifyPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

static JSBool
XPC_WN_CannotModifyStrictPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict,
                                      jsval *vp)
{
    return XPC_WN_CannotModifyPropertyStub(cx, obj, id, vp);
}

static JSBool
XPC_WN_Shared_Convert(JSContext *cx, JSHandleObject obj, JSType type, jsval *vp)
{
    if (type == JSTYPE_OBJECT) {
        *vp = OBJECT_TO_JSVAL(obj);
        return true;
    }

    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    switch (type) {
        case JSTYPE_FUNCTION:
            {
                if (!ccx.GetTearOff()) {
                    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
                    if (si && (si->GetFlags().WantCall() ||
                               si->GetFlags().WantConstruct())) {
                        *vp = OBJECT_TO_JSVAL(obj);
                        return true;
                    }
                }
            }
            return Throw(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN, cx);
        case JSTYPE_NUMBER:
            *vp = JS_GetNaNValue(cx);
            return true;
        case JSTYPE_BOOLEAN:
            *vp = JSVAL_TRUE;
            return true;
        case JSTYPE_VOID:
        case JSTYPE_STRING:
        {
            ccx.SetName(ccx.GetRuntime()->GetStringID(XPCJSRuntime::IDX_TO_STRING));
            ccx.SetArgsAndResultPtr(0, nsnull, vp);

            XPCNativeMember* member = ccx.GetMember();
            if (member && member->IsMethod()) {
                if (!XPCWrappedNative::CallMethod(ccx))
                    return false;

                if (JSVAL_IS_PRIMITIVE(*vp))
                    return true;
            }

            // else...
            return ToStringGuts(ccx);
        }
        default:
            NS_ERROR("bad type in conversion");
            return false;
    }
    NS_NOTREACHED("huh?");
    return false;
}

static JSBool
XPC_WN_Shared_Enumerate(JSContext *cx, JSHandleObject obj)
{
    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    // Since we aren't going to enumerate tearoff names and the prototype
    // handles non-mutated members, we can do this potential short-circuit.
    if (!wrapper->HasMutatedSet())
        return true;

    XPCNativeSet* set = wrapper->GetSet();
    XPCNativeSet* protoSet = wrapper->HasProto() ?
                                wrapper->GetProto()->GetSet() : nsnull;

    PRUint16 interface_count = set->GetInterfaceCount();
    XPCNativeInterface** interfaceArray = set->GetInterfaceArray();
    for (PRUint16 i = 0; i < interface_count; i++) {
        XPCNativeInterface* iface = interfaceArray[i];
        PRUint16 member_count = iface->GetMemberCount();
        for (PRUint16 k = 0; k < member_count; k++) {
            XPCNativeMember* member = iface->GetMemberAt(k);
            jsid name = member->GetName();

            // Skip if this member is going to come from the proto.
            PRUint16 index;
            if (protoSet &&
                protoSet->FindMember(name, nsnull, &index) && index == i)
                continue;
            if (!xpc_ForcePropertyResolve(cx, obj, name))
                return false;
        }
    }
    return true;
}

/***************************************************************************/

#ifdef DEBUG_slimwrappers
static PRUint32 sFinalizedSlimWrappers;
#endif

enum WNHelperType {
    WN_NOHELPER,
    WN_HELPER
};

static void
WrappedNativeFinalize(js::FreeOp *fop, JSObject *obj, WNHelperType helperType)
{
    js::Class* clazz = js::GetObjectClass(obj);
    if (clazz->flags & JSCLASS_DOM_GLOBAL) {
        mozilla::dom::DestroyProtoOrIfaceCache(obj);
    }
    nsISupports* p = static_cast<nsISupports*>(xpc_GetJSPrivate(obj));
    if (!p)
        return;

    if (IS_SLIM_WRAPPER_OBJECT(obj)) {
        SLIM_LOG(("----- %i finalized slim wrapper (%p, %p)\n",
                  ++sFinalizedSlimWrappers, obj, p));

        nsWrapperCache* cache;
        CallQueryInterface(p, &cache);
        cache->ClearWrapper();

        XPCJSRuntime *rt = nsXPConnect::GetRuntimeInstance();
        MOZ_ASSERT(rt, "XPCJSRuntime should exist during a GC.");
        rt->DeferredRelease(p);
        return;
    }

    XPCWrappedNative* wrapper = static_cast<XPCWrappedNative*>(p);
    if (helperType == WN_HELPER)
        wrapper->GetScriptableCallback()->Finalize(wrapper, js::CastToJSFreeOp(fop), obj);
    wrapper->FlatJSObjectFinalized();
}

static void
XPC_WN_NoHelper_Finalize(js::FreeOp *fop, JSObject *obj)
{
    WrappedNativeFinalize(fop, obj, WN_NOHELPER);
}

static void
TraceScopeJSObjects(JSTracer *trc, XPCWrappedNativeScope* scope)
{
    scope->TraceSelf(trc);
}

static void
TraceForValidWrapper(JSTracer *trc, XPCWrappedNative* wrapper)
{
    // NOTE: It might be nice to also do the wrapper->Mark() call here too
    // when we are called during the marking phase of JS GC to mark the
    // wrapper's and wrapper's proto's interface sets.
    //
    // We currently do that in the GC callback code. The reason we don't do that
    // here is because the bits used in that marking do unpleasant things to the
    // member counts in the interface and interface set objects. Those counts
    // are used in the DealWithDyingGCThings calls that are part of this JS GC
    // marking phase. By doing these calls later during our GC callback we
    // avoid that problem. Arguably this could be changed. But it ain't broke.
    //
    // However, we do need to call the wrapper's TraceJS so that
    // it can be sure that its (potentially shared) JSClass is traced. The
    // danger is that a live wrapper might not be in a wrapper map and thus
    // won't be fully marked in the GC callback. This can happen if there is
    // a security exception during wrapper creation or if during wrapper
    // creation it is determined that the wrapper is not needed. In those cases
    // the wrapper can never actually be used from JS code - so resources like
    // the interface set will never be accessed. But the JS engine will still
    // need to use the JSClass. So, some marking is required for protection.

    wrapper->TraceJS(trc);

    TraceScopeJSObjects(trc, wrapper->GetScope());
}

static void
MarkWrappedNative(JSTracer *trc, JSObject *obj)
{
    js::Class* clazz = js::GetObjectClass(obj);
    if (clazz->flags & JSCLASS_DOM_GLOBAL) {
        mozilla::dom::TraceProtoOrIfaceCache(trc, obj);
    }

    JSObject *obj2;

    // Pass null for the first JSContext* parameter  to skip any security
    // checks and to avoid potential state change there.
    XPCWrappedNative* wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(nsnull, obj, nsnull, &obj2);

    if (wrapper) {
        if (wrapper->IsValid())
             TraceForValidWrapper(trc, wrapper);
    } else if (obj2) {
        GetSlimWrapperProto(obj2)->TraceJS(trc);
    }
}

static void
XPC_WN_NoHelper_Trace(JSTracer *trc, JSObject *obj)
{
    MarkWrappedNative(trc, obj);
}

static JSBool
XPC_WN_NoHelper_Resolve(JSContext *cx, JSHandleObject obj, JSHandleId id)
{
    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull, id);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeSet* set = ccx.GetSet();
    if (!set)
        return true;

    // Don't resolve properties that are on our prototype.
    if (ccx.GetInterface() && !ccx.GetStaticMemberIsLocal())
        return true;

    return DefinePropertyIfFound(ccx, obj, id,
                                 set, nsnull, nsnull, wrapper->GetScope(),
                                 true, wrapper, wrapper, nsnull,
                                 JSPROP_ENUMERATE |
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT, nsnull);
}

nsISupports *
XPC_GetIdentityObject(JSContext *cx, JSObject *obj)
{
    XPCWrappedNative *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

    return wrapper ? wrapper->GetIdentityObject() : nsnull;
}

JSBool
XPC_WN_Equality(JSContext *cx, JSHandleObject obj, const jsval *valp, JSBool *bp)
{
    jsval v = *valp;
    *bp = false;

    JSObject *obj2;
    XPCWrappedNative *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj, nsnull, &obj2);
    if (obj2) {
        *bp = !JSVAL_IS_PRIMITIVE(v) && (JSVAL_TO_OBJECT(v) == obj2);

        return true;
    }

    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if (si && si->GetFlags().WantEquality()) {
        bool res;
        nsresult rv = si->GetCallback()->Equality(wrapper, cx, obj, v, &res);
        if (NS_FAILED(rv))
            return Throw(rv, cx);
        *bp = res;
    } else if (!JSVAL_IS_PRIMITIVE(v)) {
        JSObject *other = JSVAL_TO_OBJECT(v);

        *bp = (obj == other ||
               XPC_GetIdentityObject(cx, obj) ==
               XPC_GetIdentityObject(cx, other));
    }

    return true;
}

static JSObject *
XPC_WN_OuterObject(JSContext *cx, JSHandleObject obj_)
{
    JSObject *obj = obj_;

    XPCWrappedNative *wrapper =
        static_cast<XPCWrappedNative *>(js::GetObjectPrivate(obj));
    if (!wrapper) {
        Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

        return nsnull;
    }

    if (!wrapper->IsValid()) {
        Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);

        return nsnull;
    }

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if (si && si->GetFlags().WantOuterObject()) {
        JSObject *newThis;
        nsresult rv =
            si->GetCallback()->OuterObject(wrapper, cx, obj, &newThis);

        if (NS_FAILED(rv)) {
            Throw(rv, cx);

            return nsnull;
        }

        obj = newThis;
    }

    return obj;
}

XPCWrappedNativeJSClass XPC_WN_NoHelper_JSClass = {
  { // base
    "XPCWrappedNative_NoHelper",    // name;
    WRAPPER_SLOTS |
    JSCLASS_PRIVATE_IS_NSISUPPORTS, // flags

    /* Mandatory non-null function pointer members. */
    XPC_WN_OnlyIWrite_AddPropertyStub, // addProperty
    XPC_WN_CannotModifyPropertyStub,   // delProperty
    JS_PropertyStub,                   // getProperty
    XPC_WN_OnlyIWrite_SetPropertyStub, // setProperty

    XPC_WN_Shared_Enumerate,           // enumerate
    XPC_WN_NoHelper_Resolve,           // resolve
    XPC_WN_Shared_Convert,             // convert
    XPC_WN_NoHelper_Finalize,          // finalize

    /* Optionally non-null members start here. */
    nsnull,                         // checkAccess
    nsnull,                         // call
    nsnull,                         // construct
    nsnull,                         // hasInstance
    XPC_WN_NoHelper_Trace,          // trace

    // ClassExtension
    {
        XPC_WN_Equality,
        nsnull, // outerObject
        nsnull, // innerObject
        nsnull, // iteratorObject
        nsnull, // unused
        true,   // isWrappedNative
    },

    // ObjectOps
    {
        nsnull, // lookupGeneric
        nsnull, // lookupProperty
        nsnull, // lookupElement
        nsnull, // lookupSpecial
        nsnull, // defineGeneric
        nsnull, // defineProperty
        nsnull, // defineElement
        nsnull, // defineSpecial
        nsnull, // getGeneric
        nsnull, // getProperty
        nsnull, // getElement
        nsnull, // getElementIfPresent
        nsnull, // getSpecial
        nsnull, // setGeneric
        nsnull, // setProperty
        nsnull, // setElement
        nsnull, // setSpecial
        nsnull, // getGenericAttributes
        nsnull, // getAttributes
        nsnull, // getElementAttributes
        nsnull, // getSpecialAttributes
        nsnull, // setGenericAttributes
        nsnull, // setAttributes
        nsnull, // setElementAttributes
        nsnull, // setSpecialAttributes
        nsnull, // deleteProperty
        nsnull, // deleteElement
        nsnull, // deleteSpecial
        XPC_WN_JSOp_Enumerate,
        XPC_WN_JSOp_TypeOf_Object,
        XPC_WN_JSOp_ThisObject,
        XPC_WN_JSOp_Clear
    }
  },
  0 // interfacesBitmap
};


/***************************************************************************/

static JSBool
XPC_WN_MaybeResolvingPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    if (ccx.GetResolvingWrapper() == wrapper)
        return true;
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

static JSBool
XPC_WN_MaybeResolvingStrictPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict,
                                        jsval *vp)
{
    return XPC_WN_MaybeResolvingPropertyStub(cx, obj, id, vp);
}

// macro fun!
#define PRE_HELPER_STUB_NO_SLIM                                               \
    XPCWrappedNative* wrapper =                                               \
        XPCWrappedNative::GetAndMorphWrappedNativeOfJSObject(cx, obj);        \
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);                             \
    bool retval = true;                                                       \
    nsresult rv = wrapper->GetScriptableCallback()->

#define PRE_HELPER_STUB                                                       \
    XPCWrappedNative* wrapper;                                                \
    nsIXPCScriptable* si;                                                     \
    if (IS_SLIM_WRAPPER(obj)) {                                               \
        wrapper = nsnull;                                                     \
        si = GetSlimWrapperProto(obj)->GetScriptableInfo()->GetCallback();    \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        wrapper = XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);      \
        THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);                         \
        si = wrapper->GetScriptableCallback();                                \
    }                                                                         \
    bool retval = true;                                                       \
    nsresult rv = si->

#define POST_HELPER_STUB                                                      \
    if (NS_FAILED(rv))                                                        \
        return Throw(rv, cx);                                                 \
    return retval;

static JSBool
XPC_WN_Helper_AddProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    PRE_HELPER_STUB
    AddProperty(wrapper, cx, obj, id, vp, &retval);
    POST_HELPER_STUB
}

static JSBool
XPC_WN_Helper_DelProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    PRE_HELPER_STUB
    DelProperty(wrapper, cx, obj, id, vp, &retval);
    POST_HELPER_STUB
}

JSBool
XPC_WN_Helper_GetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    PRE_HELPER_STUB
    GetProperty(wrapper, cx, obj, id, vp, &retval);
    POST_HELPER_STUB
}

JSBool
XPC_WN_Helper_SetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict, jsval *vp)
{
    PRE_HELPER_STUB
    SetProperty(wrapper, cx, obj, id, vp, &retval);
    POST_HELPER_STUB
}

static JSBool
XPC_WN_Helper_Convert(JSContext *cx, JSHandleObject obj, JSType type, jsval *vp)
{
    SLIM_LOG_WILL_MORPH(cx, obj);
    PRE_HELPER_STUB_NO_SLIM
    Convert(wrapper, cx, obj, type, vp, &retval);
    POST_HELPER_STUB
}

static JSBool
XPC_WN_Helper_CheckAccess(JSContext *cx, JSHandleObject obj, JSHandleId id,
                          JSAccessMode mode, jsval *vp)
{
    PRE_HELPER_STUB
    CheckAccess(wrapper, cx, obj, id, mode, vp, &retval);
    POST_HELPER_STUB
}

static JSBool
XPC_WN_Helper_Call(JSContext *cx, unsigned argc, jsval *vp)
{
    // N.B. we want obj to be the callee, not JS_THIS(cx, vp)
    JSObject *obj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull, JSID_VOID,
                       argc, JS_ARGV(cx, vp), vp);
    if (!ccx.IsValid())
        return false;

    JS_ASSERT(obj == ccx.GetFlattenedJSObject());

    SLIM_LOG_WILL_MORPH(cx, obj);
    PRE_HELPER_STUB_NO_SLIM
    Call(wrapper, cx, obj, argc, JS_ARGV(cx, vp), vp, &retval);
    POST_HELPER_STUB
}

static JSBool
XPC_WN_Helper_Construct(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    if (!obj)
        return false;

    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull, JSID_VOID,
                       argc, JS_ARGV(cx, vp), vp);
    if (!ccx.IsValid())
        return false;

    JS_ASSERT(obj == ccx.GetFlattenedJSObject());

    SLIM_LOG_WILL_MORPH(cx, obj);
    PRE_HELPER_STUB_NO_SLIM
    Construct(wrapper, cx, obj, argc, JS_ARGV(cx, vp), vp, &retval);
    POST_HELPER_STUB
}

static JSBool
XPC_WN_Helper_HasInstance(JSContext *cx, JSHandleObject obj, const jsval *valp, JSBool *bp)
{
    SLIM_LOG_WILL_MORPH(cx, obj);
    bool retval2;
    PRE_HELPER_STUB_NO_SLIM
    HasInstance(wrapper, cx, obj, *valp, &retval2, &retval);
    *bp = retval2;
    POST_HELPER_STUB
}

static void
XPC_WN_Helper_Finalize(js::FreeOp *fop, JSObject *obj)
{
    WrappedNativeFinalize(fop, obj, WN_HELPER);
}

static JSBool
XPC_WN_Helper_NewResolve(JSContext *cx, JSHandleObject obj, JSHandleId id, unsigned flags,
                         JSObject **objp)
{
    nsresult rv = NS_OK;
    bool retval = true;
    JSObject* obj2FromScriptable = nsnull;
    if (IS_SLIM_WRAPPER(obj)) {
        XPCNativeScriptableInfo *si =
            GetSlimWrapperProto(obj)->GetScriptableInfo();
        if (!si->GetFlags().WantNewResolve())
            return retval;

        NS_ASSERTION(si->GetFlags().AllowPropModsToPrototype() &&
                     !si->GetFlags().AllowPropModsDuringResolve(),
                     "We don't support these flags for slim wrappers!");

        rv = si->GetCallback()->NewResolve(nsnull, cx, obj, id, flags,
                                           &obj2FromScriptable, &retval);
        if (NS_FAILED(rv))
            return Throw(rv, cx);

        if (obj2FromScriptable)
            *objp = obj2FromScriptable;

        return retval;
    }

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    jsid old = ccx.SetResolveName(id);

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if (si && si->GetFlags().WantNewResolve()) {
        XPCWrappedNative* oldResolvingWrapper;
        JSBool allowPropMods = si->GetFlags().AllowPropModsDuringResolve();

        if (allowPropMods)
            oldResolvingWrapper = ccx.SetResolvingWrapper(wrapper);

        rv = si->GetCallback()->NewResolve(wrapper, cx, obj, id, flags,
                                           &obj2FromScriptable, &retval);

        if (allowPropMods)
            (void)ccx.SetResolvingWrapper(oldResolvingWrapper);
    }

    old = ccx.SetResolveName(old);
    NS_ASSERTION(old == id, "bad nest");

    if (NS_FAILED(rv)) {
        return Throw(rv, cx);
    }

    if (obj2FromScriptable) {
        *objp = obj2FromScriptable;
    } else if (wrapper->HasMutatedSet()) {
        // We are here if scriptable did not resolve this property and
        // it *might* be in the instance set but not the proto set.

        XPCNativeSet* set = wrapper->GetSet();
        XPCNativeSet* protoSet = wrapper->HasProto() ?
                                    wrapper->GetProto()->GetSet() : nsnull;
        XPCNativeMember* member;
        XPCNativeInterface* iface;
        JSBool IsLocal;

        if (set->FindMember(id, &member, &iface, protoSet, &IsLocal) &&
            IsLocal) {
            XPCWrappedNative* oldResolvingWrapper;

            XPCNativeScriptableFlags siFlags(0);
            if (si)
                siFlags = si->GetFlags();

            unsigned enumFlag =
                siFlags.DontEnumStaticProps() ? 0 : JSPROP_ENUMERATE;

            XPCWrappedNative* wrapperForInterfaceNames =
                siFlags.DontReflectInterfaceNames() ? nsnull : wrapper;

            JSBool resolved;
            oldResolvingWrapper = ccx.SetResolvingWrapper(wrapper);
            retval = DefinePropertyIfFound(ccx, obj, id,
                                           set, iface, member,
                                           wrapper->GetScope(),
                                           false,
                                           wrapperForInterfaceNames,
                                           nsnull, si,
                                           enumFlag, &resolved);
            (void)ccx.SetResolvingWrapper(oldResolvingWrapper);
            if (retval && resolved)
                *objp = obj;
        }
    }

    return retval;
}

/***************************************************************************/

/*
    Here are the enumerator cases:

    set jsclass enumerate to stub (unless noted otherwise)

    if ( helper wants new enumerate )
        if ( DONT_ENUM_STATICS )
            forward to scriptable enumerate
        else
            if ( set not mutated )
                forward to scriptable enumerate
            else
                call shared enumerate
                forward to scriptable enumerate
    else if ( helper wants old enumerate )
        use this JSOp
        if ( DONT_ENUM_STATICS )
            call scriptable enumerate
            call stub
        else
            if ( set not mutated )
                call scriptable enumerate
                call stub
            else
                call shared enumerate
                call scriptable enumerate
                call stub

    else //... if ( helper wants NO enumerate )
        if ( DONT_ENUM_STATICS )
            use enumerate stub - don't use this JSOp thing at all
        else
            do shared enumerate - don't use this JSOp thing at all
*/

JSBool
XPC_WN_JSOp_Enumerate(JSContext *cx, JSHandleObject obj, JSIterateOp enum_op,
                      jsval *statep, jsid *idp)
{
    js::Class *clazz = js::GetObjectClass(obj);
    if (!IS_WRAPPER_CLASS(clazz) || clazz == &XPC_WN_NoHelper_JSClass.base) {
        // obj must be a prototype object or a wrapper w/o a
        // helper. Short circuit this call to the default
        // implementation.

        return JS_EnumerateState(cx, obj, enum_op, statep, idp);
    }

    MORPH_SLIM_WRAPPER(cx, obj);

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if (!si)
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    bool retval = true;
    nsresult rv;

    if (si->GetFlags().WantNewEnumerate()) {
        if (((enum_op == JSENUMERATE_INIT &&
              !si->GetFlags().DontEnumStaticProps()) ||
             enum_op == JSENUMERATE_INIT_ALL) &&
            wrapper->HasMutatedSet() &&
            !XPC_WN_Shared_Enumerate(cx, obj)) {
            *statep = JSVAL_NULL;
            return false;
        }

        // XXX Might we really need to wrap this call and *also* call
        // js_ObjectOps.enumerate ???

        rv = si->GetCallback()->
            NewEnumerate(wrapper, cx, obj, enum_op, statep, idp, &retval);

        if ((enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL) &&
            (NS_FAILED(rv) || !retval)) {
            *statep = JSVAL_NULL;
        }

        if (NS_FAILED(rv))
            return Throw(rv, cx);
        return retval;
    }

    if (si->GetFlags().WantEnumerate()) {
        if (enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL) {
            if ((enum_op == JSENUMERATE_INIT_ALL ||
                 !si->GetFlags().DontEnumStaticProps()) &&
                wrapper->HasMutatedSet() &&
                !XPC_WN_Shared_Enumerate(cx, obj)) {
                *statep = JSVAL_NULL;
                return false;
            }
            rv = si->GetCallback()->
                Enumerate(wrapper, cx, obj, &retval);

            if (NS_FAILED(rv) || !retval)
                *statep = JSVAL_NULL;

            if (NS_FAILED(rv))
                return Throw(rv, cx);
            if (!retval)
                return false;
            // Then fall through and call the default implementation...
        }
    }

    // else call js_ObjectOps.enumerate...

    return JS_EnumerateState(cx, obj, enum_op, statep, idp);
}

JSType
XPC_WN_JSOp_TypeOf_Object(JSContext *cx, JSHandleObject obj)
{
    return JSTYPE_OBJECT;
}

JSType
XPC_WN_JSOp_TypeOf_Function(JSContext *cx, JSHandleObject obj)
{
    return JSTYPE_FUNCTION;
}

void
XPC_WN_JSOp_Clear(JSContext *cx, JSHandleObject obj)
{
    // XXX Clear XrayWrappers?
}

namespace {

NS_STACK_CLASS class AutoPopJSContext
{
public:
  AutoPopJSContext(XPCJSContextStack *stack)
  : mCx(nsnull), mStack(stack)
  {
      NS_ASSERTION(stack, "Null stack!");
  }

  ~AutoPopJSContext()
  {
      if (mCx)
          mStack->Pop();
  }

  void PushIfNotTop(JSContext *cx)
  {
      NS_ASSERTION(cx, "Null context!");
      NS_ASSERTION(!mCx, "This class is only meant to be used once!");

      JSContext *cxTop = mStack->Peek();

      if (cxTop != cx && mStack->Push(cx))
          mCx = cx;
  }

private:
  JSContext *mCx;
  XPCJSContextStack *mStack;
};

} // namespace

JSObject*
XPC_WN_JSOp_ThisObject(JSContext *cx, JSHandleObject obj)
{
    // None of the wrappers we could potentially hand out are threadsafe so
    // just hand out the given object.
    if (!XPCPerThreadData::IsMainThread(cx))
        return obj;

    return JS_ObjectToOuterObject(cx, obj);
}

/***************************************************************************/

// static
XPCNativeScriptableInfo*
XPCNativeScriptableInfo::Construct(XPCCallContext& ccx,
                                   const XPCNativeScriptableCreateInfo* sci)
{
    NS_ASSERTION(sci, "bad param");
    NS_ASSERTION(sci->GetCallback(), "bad param");

    XPCNativeScriptableInfo* newObj =
        new XPCNativeScriptableInfo(sci->GetCallback());
    if (!newObj)
        return nsnull;

    char* name = nsnull;
    if (NS_FAILED(sci->GetCallback()->GetClassName(&name)) || !name) {
        delete newObj;
        return nsnull;
    }

    JSBool success;

    XPCJSRuntime* rt = ccx.GetRuntime();
    XPCNativeScriptableSharedMap* map = rt->GetNativeScriptableSharedMap();
    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        success = map->GetNewOrUsed(sci->GetFlags(), name,
                                    sci->GetInterfacesBitmap(), newObj);
    }

    if (!success) {
        delete newObj;
        return nsnull;
    }

    return newObj;
}

void
XPCNativeScriptableShared::PopulateJSClass()
{
    NS_ASSERTION(mJSClass.base.name, "bad state!");

    mJSClass.base.flags = WRAPPER_SLOTS |
                          JSCLASS_PRIVATE_IS_NSISUPPORTS |
                          JSCLASS_NEW_RESOLVE;

    if (mFlags.IsGlobalObject())
        mJSClass.base.flags |= XPCONNECT_GLOBAL_FLAGS;

    JSPropertyOp addProperty;
    if (mFlags.WantAddProperty())
        addProperty = XPC_WN_Helper_AddProperty;
    else if (mFlags.UseJSStubForAddProperty())
        addProperty = JS_PropertyStub;
    else if (mFlags.AllowPropModsDuringResolve())
        addProperty = XPC_WN_MaybeResolvingPropertyStub;
    else
        addProperty = XPC_WN_CannotModifyPropertyStub;
    mJSClass.base.addProperty = addProperty;

    JSPropertyOp delProperty;
    if (mFlags.WantDelProperty())
        delProperty = XPC_WN_Helper_DelProperty;
    else if (mFlags.UseJSStubForDelProperty())
        delProperty = JS_PropertyStub;
    else if (mFlags.AllowPropModsDuringResolve())
        delProperty = XPC_WN_MaybeResolvingPropertyStub;
    else
        delProperty = XPC_WN_CannotModifyPropertyStub;
    mJSClass.base.delProperty = delProperty;

    if (mFlags.WantGetProperty())
        mJSClass.base.getProperty = XPC_WN_Helper_GetProperty;
    else
        mJSClass.base.getProperty = JS_PropertyStub;

    JSStrictPropertyOp setProperty;
    if (mFlags.WantSetProperty())
        setProperty = XPC_WN_Helper_SetProperty;
    else if (mFlags.UseJSStubForSetProperty())
        setProperty = JS_StrictPropertyStub;
    else if (mFlags.AllowPropModsDuringResolve())
        setProperty = XPC_WN_MaybeResolvingStrictPropertyStub;
    else
        setProperty = XPC_WN_CannotModifyStrictPropertyStub;
    mJSClass.base.setProperty = setProperty;

    // We figure out most of the enumerate strategy at call time.

    if (mFlags.WantNewEnumerate() || mFlags.WantEnumerate() ||
        mFlags.DontEnumStaticProps())
        mJSClass.base.enumerate = JS_EnumerateStub;
    else
        mJSClass.base.enumerate = XPC_WN_Shared_Enumerate;

    // We have to figure out resolve strategy at call time
    mJSClass.base.resolve = (JSResolveOp) XPC_WN_Helper_NewResolve;

    if (mFlags.WantConvert())
        mJSClass.base.convert = XPC_WN_Helper_Convert;
    else
        mJSClass.base.convert = XPC_WN_Shared_Convert;

    if (mFlags.WantFinalize())
        mJSClass.base.finalize = XPC_WN_Helper_Finalize;
    else
        mJSClass.base.finalize = XPC_WN_NoHelper_Finalize;

    // We let the rest default to nsnull unless the helper wants them...
    if (mFlags.WantCheckAccess())
        mJSClass.base.checkAccess = XPC_WN_Helper_CheckAccess;

    // Note that we *must* set the ObjectOps (even for the cases were it does
    // not do much) because with these dynamically generated JSClasses, the
    // code in XPCWrappedNative::GetWrappedNativeOfJSObject() needs to look
    // for that these callback pointers in order to identify that a given
    // JSObject represents a wrapper.
    js::ObjectOps *ops = &mJSClass.base.ops;
    ops->enumerate = XPC_WN_JSOp_Enumerate;
    ops->clear = XPC_WN_JSOp_Clear;
    ops->thisObject = XPC_WN_JSOp_ThisObject;

    if (mFlags.WantCall() || mFlags.WantConstruct()) {
        ops->typeOf = XPC_WN_JSOp_TypeOf_Function;
        if (mFlags.WantCall())
            mJSClass.base.call = XPC_WN_Helper_Call;
        if (mFlags.WantConstruct())
            mJSClass.base.construct = XPC_WN_Helper_Construct;
    } else {
        ops->typeOf = XPC_WN_JSOp_TypeOf_Object;
    }

    if (mFlags.UseStubEqualityHook()) {
        NS_ASSERTION(!mFlags.WantEquality(),
                     "If you want an Equality callback, you can't use a stub "
                     "equality hook");
        mJSClass.base.ext.equality = nsnull;
    } else {
        mJSClass.base.ext.equality = XPC_WN_Equality;
    }

    if (mFlags.WantHasInstance())
        mJSClass.base.hasInstance = XPC_WN_Helper_HasInstance;

    mJSClass.base.trace = XPC_WN_NoHelper_Trace;

    if (mFlags.WantOuterObject())
        mJSClass.base.ext.outerObject = XPC_WN_OuterObject;

    if (!(mFlags & nsIXPCScriptable::WANT_OUTER_OBJECT))
        mCanBeSlim = true;

    mJSClass.base.ext.isWrappedNative = true;
}

/***************************************************************************/
/***************************************************************************/

JSBool
XPC_WN_CallMethod(JSContext *cx, unsigned argc, jsval *vp)
{
    NS_ASSERTION(JS_TypeOfValue(cx, JS_CALLEE(cx, vp)) == JSTYPE_FUNCTION, "bad function");
    JSObject* funobj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

    JSObject* obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

#ifdef DEBUG_slimwrappers
    {
        JSFunction* fun = funobj->getFunctionPrivate();
        JSString *funid = JS_GetFunctionId(fun);
        JSAutoByteString bytes;
        const char *funname = !funid ? "" : bytes.encode(cx, funid) ? bytes.ptr() : "<error>";
        SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, funname);
    }
#endif
    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    XPCCallContext ccx(JS_CALLER, cx, obj, funobj, JSID_VOID, argc, JS_ARGV(cx, vp), vp);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeInterface* iface;
    XPCNativeMember*    member;

    if (!XPCNativeMember::GetCallInfo(ccx, funobj, &iface, &member))
        return Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx);
    ccx.SetCallInfo(iface, member, false);
    return XPCWrappedNative::CallMethod(ccx);
}

JSBool
XPC_WN_GetterSetter(JSContext *cx, unsigned argc, jsval *vp)
{
    NS_ASSERTION(JS_TypeOfValue(cx, JS_CALLEE(cx, vp)) == JSTYPE_FUNCTION, "bad function");
    JSObject* funobj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

    JSObject* obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

#ifdef DEBUG_slimwrappers
    {
        const char* funname = nsnull;
        JSAutoByteString bytes;
        if (JS_TypeOfValue(cx, JS_CALLEE(cx, vp)) == JSTYPE_FUNCTION) {
            JSString *funid = JS_GetFunctionId(funobj->getFunctionPrivate());
            funname = !funid ? "" : bytes.encode(cx, funid) ? bytes.ptr() : "<error>";
        }
        SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, funname);
    }
#endif
    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    XPCCallContext ccx(JS_CALLER, cx, obj, funobj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeInterface* iface;
    XPCNativeMember*    member;

    if (!XPCNativeMember::GetCallInfo(ccx, funobj, &iface, &member))
        return Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx);

    ccx.SetArgsAndResultPtr(argc, JS_ARGV(cx, vp), vp);
    if (argc && member->IsWritableAttribute()) {
        ccx.SetCallInfo(iface, member, true);
        JSBool retval = XPCWrappedNative::SetAttribute(ccx);
        if (retval)
            *vp = JS_ARGV(cx, vp)[0];
        return retval;
    }
    // else...

    ccx.SetCallInfo(iface, member, false);
    return XPCWrappedNative::GetAttribute(ccx);
}

/***************************************************************************/

static JSBool
XPC_WN_Shared_Proto_Enumerate(JSContext *cx, JSHandleObject obj)
{
    NS_ASSERTION(js::GetObjectClass(obj) == &XPC_WN_ModsAllowed_WithCall_Proto_JSClass ||
                 js::GetObjectClass(obj) == &XPC_WN_ModsAllowed_NoCall_Proto_JSClass ||
                 js::GetObjectClass(obj) == &XPC_WN_NoMods_WithCall_Proto_JSClass ||
                 js::GetObjectClass(obj) == &XPC_WN_NoMods_NoCall_Proto_JSClass,
                 "bad proto");
    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    if (self->GetScriptableInfo() &&
        self->GetScriptableInfo()->GetFlags().DontEnumStaticProps())
        return true;

    XPCNativeSet* set = self->GetSet();
    if (!set)
        return false;

    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid())
        return false;
    ccx.SetScopeForNewJSObjects(obj);

    PRUint16 interface_count = set->GetInterfaceCount();
    XPCNativeInterface** interfaceArray = set->GetInterfaceArray();
    for (PRUint16 i = 0; i < interface_count; i++) {
        XPCNativeInterface* iface = interfaceArray[i];
        PRUint16 member_count = iface->GetMemberCount();

        for (PRUint16 k = 0; k < member_count; k++) {
            if (!xpc_ForcePropertyResolve(cx, obj, iface->GetMemberAt(k)->GetName()))
                return false;
        }
    }

    return true;
}

static void
XPC_WN_Shared_Proto_Finalize(js::FreeOp *fop, JSObject *obj)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p = (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (p)
        p->JSProtoObjectFinalized(fop, obj);
}

static void
XPC_WN_Shared_Proto_Trace(JSTracer *trc, JSObject *obj)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (p)
        TraceScopeJSObjects(trc, p->GetScope());
}

/*****************************************************/

static JSBool
XPC_WN_ModsAllowed_Proto_Resolve(JSContext *cx, JSHandleObject obj, JSHandleId id)
{
    NS_ASSERTION(js::GetObjectClass(obj) == &XPC_WN_ModsAllowed_WithCall_Proto_JSClass ||
                 js::GetObjectClass(obj) == &XPC_WN_ModsAllowed_NoCall_Proto_JSClass,
                 "bad proto");

    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid())
        return false;
    ccx.SetScopeForNewJSObjects(obj);

    XPCNativeScriptableInfo* si = self->GetScriptableInfo();
    unsigned enumFlag = (si && si->GetFlags().DontEnumStaticProps()) ?
                                                0 : JSPROP_ENUMERATE;

    return DefinePropertyIfFound(ccx, obj, id,
                                 self->GetSet(), nsnull, nsnull,
                                 self->GetScope(),
                                 true, nsnull, nsnull, si,
                                 enumFlag, nsnull);
}

js::Class XPC_WN_ModsAllowed_WithCall_Proto_JSClass = {
    "XPC_WN_ModsAllowed_WithCall_Proto_JSClass", // name;
    WRAPPER_SLOTS, // flags;

    /* Mandatory non-null function pointer members. */
    JS_PropertyStub,                // addProperty;
    JS_PropertyStub,                // delProperty;
    JS_PropertyStub,                // getProperty;
    JS_StrictPropertyStub,          // setProperty;
    XPC_WN_Shared_Proto_Enumerate,  // enumerate;
    XPC_WN_ModsAllowed_Proto_Resolve, // resolve;
    JS_ConvertStub,                 // convert;
    XPC_WN_Shared_Proto_Finalize,   // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // hasInstance;
    XPC_WN_Shared_Proto_Trace,      // trace;

    JS_NULL_CLASS_EXT,
    XPC_WN_WithCall_ObjectOps
};

js::Class XPC_WN_ModsAllowed_NoCall_Proto_JSClass = {
    "XPC_WN_ModsAllowed_NoCall_Proto_JSClass", // name;
    WRAPPER_SLOTS,                  // flags;

    /* Mandatory non-null function pointer members. */
    JS_PropertyStub,                // addProperty;
    JS_PropertyStub,                // delProperty;
    JS_PropertyStub,                // getProperty;
    JS_StrictPropertyStub,          // setProperty;
    XPC_WN_Shared_Proto_Enumerate,  // enumerate;
    XPC_WN_ModsAllowed_Proto_Resolve, // resolve;
    JS_ConvertStub,                 // convert;
    XPC_WN_Shared_Proto_Finalize,   // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // hasInstance;
    XPC_WN_Shared_Proto_Trace,      // trace;

    JS_NULL_CLASS_EXT,
    XPC_WN_NoCall_ObjectOps
};

/***************************************************************************/

static JSBool
XPC_WN_OnlyIWrite_Proto_AddPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    NS_ASSERTION(js::GetObjectClass(obj) == &XPC_WN_NoMods_WithCall_Proto_JSClass ||
                 js::GetObjectClass(obj) == &XPC_WN_NoMods_NoCall_Proto_JSClass,
                 "bad proto");

    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid())
        return false;
    ccx.SetScopeForNewJSObjects(obj);

    // Allow XPConnect to add the property only
    if (ccx.GetResolveName() == id)
        return true;

    return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);
}

static JSBool
XPC_WN_OnlyIWrite_Proto_SetPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict,
                                        jsval *vp)
{
    return XPC_WN_OnlyIWrite_Proto_AddPropertyStub(cx, obj, id, vp);
}

static JSBool
XPC_WN_NoMods_Proto_Resolve(JSContext *cx, JSHandleObject obj, JSHandleId id)
{
    NS_ASSERTION(js::GetObjectClass(obj) == &XPC_WN_NoMods_WithCall_Proto_JSClass ||
                 js::GetObjectClass(obj) == &XPC_WN_NoMods_NoCall_Proto_JSClass,
                 "bad proto");

    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid())
        return false;
    ccx.SetScopeForNewJSObjects(obj);

    XPCNativeScriptableInfo* si = self->GetScriptableInfo();
    unsigned enumFlag = (si && si->GetFlags().DontEnumStaticProps()) ?
                                                0 : JSPROP_ENUMERATE;

    return DefinePropertyIfFound(ccx, obj, id,
                                 self->GetSet(), nsnull, nsnull,
                                 self->GetScope(),
                                 true, nsnull, nsnull, si,
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT |
                                 enumFlag, nsnull);
}

js::Class XPC_WN_NoMods_WithCall_Proto_JSClass = {
    "XPC_WN_NoMods_WithCall_Proto_JSClass",    // name;
    WRAPPER_SLOTS,                             // flags;

    /* Mandatory non-null function pointer members. */
    XPC_WN_OnlyIWrite_Proto_AddPropertyStub,   // addProperty;
     XPC_WN_CannotModifyPropertyStub,          // delProperty;
    JS_PropertyStub,                           // getProperty;
    XPC_WN_OnlyIWrite_Proto_SetPropertyStub,   // setProperty;
    XPC_WN_Shared_Proto_Enumerate,             // enumerate;
    XPC_WN_NoMods_Proto_Resolve,               // resolve;
    JS_ConvertStub,                            // convert;
    XPC_WN_Shared_Proto_Finalize,              // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // hasInstance;
    XPC_WN_Shared_Proto_Trace,      // trace;

    JS_NULL_CLASS_EXT,
    XPC_WN_WithCall_ObjectOps
};

js::Class XPC_WN_NoMods_NoCall_Proto_JSClass = {
    "XPC_WN_NoMods_NoCall_Proto_JSClass",      // name;
    WRAPPER_SLOTS,                             // flags;

    /* Mandatory non-null function pointer members. */
    XPC_WN_OnlyIWrite_Proto_AddPropertyStub,   // addProperty;
    XPC_WN_CannotModifyPropertyStub,           // delProperty;
    JS_PropertyStub,                           // getProperty;
    XPC_WN_OnlyIWrite_Proto_SetPropertyStub,   // setProperty;
    XPC_WN_Shared_Proto_Enumerate,             // enumerate;
    XPC_WN_NoMods_Proto_Resolve,               // resolve;
    JS_ConvertStub,                            // convert;
    XPC_WN_Shared_Proto_Finalize,              // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // hasInstance;
    XPC_WN_Shared_Proto_Trace,      // trace;

    JS_NULL_CLASS_EXT,
    XPC_WN_NoCall_ObjectOps
};

/***************************************************************************/

static JSBool
XPC_WN_TearOff_Enumerate(JSContext *cx, JSHandleObject obj)
{
    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCWrappedNativeTearOff* to = ccx.GetTearOff();
    XPCNativeInterface* iface;

    if (!to || nsnull == (iface = to->GetInterface()))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    PRUint16 member_count = iface->GetMemberCount();
    for (PRUint16 k = 0; k < member_count; k++) {
        if (!xpc_ForcePropertyResolve(cx, obj, iface->GetMemberAt(k)->GetName()))
            return false;
    }

    return true;
}

static JSBool
XPC_WN_TearOff_Resolve(JSContext *cx, JSHandleObject obj, JSHandleId id)
{
    MORPH_SLIM_WRAPPER(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCWrappedNativeTearOff* to = ccx.GetTearOff();
    XPCNativeInterface* iface;

    if (!to || nsnull == (iface = to->GetInterface()))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    return DefinePropertyIfFound(ccx, obj, id, nsnull, iface, nsnull,
                                 wrapper->GetScope(),
                                 true, nsnull, nsnull, nsnull,
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT |
                                 JSPROP_ENUMERATE, nsnull);
}

static void
XPC_WN_TearOff_Finalize(js::FreeOp *fop, JSObject *obj)
{
    XPCWrappedNativeTearOff* p = (XPCWrappedNativeTearOff*)
        xpc_GetJSPrivate(obj);
    if (!p)
        return;
    p->JSObjectFinalized();
}

js::Class XPC_WN_Tearoff_JSClass = {
    "WrappedNative_TearOff",                   // name;
    WRAPPER_SLOTS,                             // flags;

    XPC_WN_OnlyIWrite_AddPropertyStub,         // addProperty;
    XPC_WN_CannotModifyPropertyStub,           // delProperty;
    JS_PropertyStub,                           // getProperty;
    XPC_WN_OnlyIWrite_SetPropertyStub,         // setProperty;
    XPC_WN_TearOff_Enumerate,                  // enumerate;
    XPC_WN_TearOff_Resolve,                    // resolve;
    XPC_WN_Shared_Convert,                     // convert;
    XPC_WN_TearOff_Finalize                    // finalize;
};
