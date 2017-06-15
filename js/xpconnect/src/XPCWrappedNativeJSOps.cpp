/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript JSClasses and JSOps for our Wrapped Native JS Objects. */

#include "xpcprivate.h"
#include "xpc_make_class.h"
#include "jsprf.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Preferences.h"
#include "nsIAddonInterposition.h"
#include "AddonWrapper.h"
#include "js/Class.h"

using namespace mozilla;
using namespace JS;

/***************************************************************************/

// All of the exceptions thrown into JS from this file go through here.
// That makes this a nice place to set a breakpoint.

static bool Throw(nsresult errNum, JSContext* cx)
{
    XPCThrower::Throw(errNum, cx);
    return false;
}

// Handy macro used in many callback stub below.

#define THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper)                          \
    PR_BEGIN_MACRO                                                            \
    if (!wrapper)                                                             \
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);                    \
    if (!wrapper->IsValid())                                                  \
        return Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);                     \
    PR_END_MACRO

/***************************************************************************/

static bool
ToStringGuts(XPCCallContext& ccx)
{
    UniqueChars sz;
    XPCWrappedNative* wrapper = ccx.GetWrapper();

    if (wrapper)
        sz.reset(wrapper->ToString(ccx.GetTearOff()));
    else
        sz = JS_smprintf("[xpconnect wrapped native prototype]");

    if (!sz) {
        JS_ReportOutOfMemory(ccx);
        return false;
    }

    JSString* str = JS_NewStringCopyZ(ccx, sz.get());
    if (!str)
        return false;

    ccx.SetRetVal(JS::StringValue(str));
    return true;
}

/***************************************************************************/

static bool
XPC_WN_Shared_ToString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    XPCCallContext ccx(cx, obj);
    if (!ccx.IsValid())
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);
    ccx.SetName(ccx.GetContext()->GetStringID(XPCJSContext::IDX_TO_STRING));
    ccx.SetArgsAndResultPtr(args.length(), args.array(), vp);
    return ToStringGuts(ccx);
}

static bool
XPC_WN_Shared_ToSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    static const char empty[] = "({})";
    JSString* str = JS_NewStringCopyN(cx, empty, sizeof(empty)-1);
    if (!str)
        return false;
    args.rval().setString(str);

    return true;
}

static bool
XPC_WN_Shared_toPrimitive(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);
    if (!JS_ValueToObject(cx, args.thisv(), &obj))
        return false;
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    JSType hint;
    if (!GetFirstArgumentAsTypeHint(cx, args, &hint))
        return false;

    if (hint == JSTYPE_NUMBER) {
        args.rval().set(JS_GetNaNValue(cx));
        return true;
    }

    MOZ_ASSERT(hint == JSTYPE_STRING || hint == JSTYPE_UNDEFINED);
    ccx.SetName(ccx.GetContext()->GetStringID(XPCJSContext::IDX_TO_STRING));
    ccx.SetArgsAndResultPtr(0, nullptr, args.rval().address());

    XPCNativeMember* member = ccx.GetMember();
    if (member && member->IsMethod()) {
        if (!XPCWrappedNative::CallMethod(ccx))
            return false;

        if (args.rval().isPrimitive())
            return true;
    }

    // else...
    return ToStringGuts(ccx);
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
    RootedObject obj(ccx);
    nsCOMPtr<nsIXPConnectWrappedJS>
        underware = do_QueryInterface(wrapper->GetIdentityObject());
    if (underware) {
        RootedObject mainObj(ccx, underware->GetJSObject());
        if (mainObj) {
            RootedId id(ccx, ccx.GetContext()->
                            GetStringID(XPCJSContext::IDX_WRAPPED_JSOBJECT));

            JSAutoCompartment ac(ccx, mainObj);

            RootedValue val(ccx);
            if (JS_GetPropertyById(ccx, mainObj, id, &val) &&
                !val.isPrimitive()) {
                obj = val.toObjectOrNull();
            }
        }
    }
    return obj;
}

// This is the getter native function we use to handle 'wrappedJSObject' for
// double wrapped JSObjects.

static bool
XPC_WN_DoubleWrappedGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    MOZ_ASSERT(JS_TypeOfValue(cx, args.calleev()) == JSTYPE_FUNCTION, "bad function");

    RootedObject realObject(cx, GetDoubleWrappedJSObject(ccx, wrapper));
    if (!realObject) {
        // This is pretty unexpected at this point. The object originally
        // responded to this get property call and now gives no object.
        // XXX Should this throw something at the caller?
        args.rval().setNull();
        return true;
    }

    // It is a double wrapped object. This should really never appear in
    // content these days, but addons still do it - see bug 965921.
    if (MOZ_UNLIKELY(!nsContentUtils::IsSystemCaller(cx))) {
        JS_ReportErrorASCII(cx, "Attempt to use .wrappedJSObject in untrusted code");
        return false;
    }
    args.rval().setObject(*realObject);
    return JS_WrapValue(cx, args.rval());
}

/***************************************************************************/

// This is our shared function to define properties on our JSObjects.

/*
 * NOTE:
 * We *never* set the tearoff names (e.g. nsIFoo) as JS_ENUMERATE.
 * We *never* set toString or toSource as JS_ENUMERATE.
 */

static bool
DefinePropertyIfFound(XPCCallContext& ccx,
                      HandleObject obj,
                      HandleId idArg,
                      XPCNativeSet* set,
                      XPCNativeInterface* ifaceArg,
                      XPCNativeMember* member,
                      XPCWrappedNativeScope* scope,
                      bool reflectToStringAndToSource,
                      XPCWrappedNative* wrapperToReflectInterfaceNames,
                      XPCWrappedNative* wrapperToReflectDoubleWrap,
                      nsIXPCScriptable* scr,
                      unsigned propFlags,
                      bool* resolved)
{
    RootedId id(ccx, idArg);
    RefPtr<XPCNativeInterface> iface = ifaceArg;
    XPCJSContext* xpccx = ccx.GetContext();
    bool found;
    const char* name;

    propFlags |= JSPROP_RESOLVING;

    if (set) {
        if (iface)
            found = true;
        else
            found = set->FindMember(id, &member, &iface);
    } else
        found = (nullptr != (member = iface->FindMember(id)));

    if (!found) {
        if (reflectToStringAndToSource) {
            JSNative call;
            uint32_t flags = 0;

            if (scr) {
                nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(scr);

                if (classInfo) {
                    nsresult rv = classInfo->GetFlags(&flags);
                    if (NS_FAILED(rv))
                        return Throw(rv, ccx);
                }
            }

            bool overwriteToString = !(flags & nsIClassInfo::DOM_OBJECT)
                || Preferences::GetBool("dom.XPCToStringForDOMClasses", false);

            if(id == xpccx->GetStringID(XPCJSContext::IDX_TO_STRING)
                && overwriteToString)
            {
                call = XPC_WN_Shared_ToString;
                name = xpccx->GetStringName(XPCJSContext::IDX_TO_STRING);
            } else if (id == xpccx->GetStringID(XPCJSContext::IDX_TO_SOURCE)) {
                call = XPC_WN_Shared_ToSource;
                name = xpccx->GetStringName(XPCJSContext::IDX_TO_SOURCE);
            } else if (id == SYMBOL_TO_JSID(
                               JS::GetWellKnownSymbol(ccx, JS::SymbolCode::toPrimitive)))
            {
                call = XPC_WN_Shared_toPrimitive;
                name = "[Symbol.toPrimitive]";
            } else {
                call = nullptr;
            }

            if (call) {
                RootedFunction fun(ccx, JS_NewFunction(ccx, call, 0, 0, name));
                if (!fun) {
                    JS_ReportOutOfMemory(ccx);
                    return false;
                }

                AutoResolveName arn(ccx, id);
                if (resolved)
                    *resolved = true;
                RootedObject value(ccx, JS_GetFunctionObject(fun));
                return JS_DefinePropertyById(ccx, obj, id, value,
                                             propFlags & ~JSPROP_ENUMERATE);
            }
        }
        // This *might* be a tearoff name that is not yet part of our
        // set. Let's lookup the name and see if it is the name of an
        // interface. Then we'll see if the object actually *does* this
        // interface and add a tearoff as necessary.

        if (wrapperToReflectInterfaceNames) {
            JSAutoByteString name;
            RefPtr<XPCNativeInterface> iface2;
            XPCWrappedNativeTearOff* to;
            RootedObject jso(ccx);
            nsresult rv = NS_OK;

            if (JSID_IS_STRING(id) &&
                name.encodeLatin1(ccx, JSID_TO_STRING(id)) &&
                (iface2 = XPCNativeInterface::GetNewOrUsed(name.ptr())) &&
                nullptr != (to = wrapperToReflectInterfaceNames->
                           FindTearOff(iface2, true, &rv)) &&
                nullptr != (jso = to->GetJSObject()))

            {
                AutoResolveName arn(ccx, id);
                if (resolved)
                    *resolved = true;
                return JS_DefinePropertyById(ccx, obj, id, jso,
                                             propFlags & ~JSPROP_ENUMERATE);
            } else if (NS_FAILED(rv) && rv != NS_ERROR_NO_INTERFACE) {
                return Throw(rv, ccx);
            }
        }

        // This *might* be a double wrapped JSObject
        if (wrapperToReflectDoubleWrap &&
            id == xpccx->GetStringID(XPCJSContext::IDX_WRAPPED_JSOBJECT) &&
            GetDoubleWrappedJSObject(ccx, wrapperToReflectDoubleWrap)) {
            // We build and add a getter function.
            // A security check is done on a per-get basis.

            JSFunction* fun;

            id = xpccx->GetStringID(XPCJSContext::IDX_WRAPPED_JSOBJECT);
            name = xpccx->GetStringName(XPCJSContext::IDX_WRAPPED_JSOBJECT);

            fun = JS_NewFunction(ccx, XPC_WN_DoubleWrappedGetter,
                                 0, 0, name);

            if (!fun)
                return false;

            RootedObject funobj(ccx, JS_GetFunctionObject(fun));
            if (!funobj)
                return false;

            propFlags |= JSPROP_GETTER | JSPROP_SHARED;
            propFlags &= ~JSPROP_ENUMERATE;

            AutoResolveName arn(ccx, id);
            if (resolved)
                *resolved = true;
            return JS_DefinePropertyById(ccx, obj, id, UndefinedHandleValue, propFlags,
                                         JS_DATA_TO_FUNC_PTR(JSNative, funobj.get()),
                                         nullptr);
        }

        if (resolved)
            *resolved = false;
        return true;
    }

    if (!member) {
        if (wrapperToReflectInterfaceNames) {
            XPCWrappedNativeTearOff* to =
              wrapperToReflectInterfaceNames->FindTearOff(iface, true);

            if (!to)
                return false;
            RootedObject jso(ccx, to->GetJSObject());
            if (!jso)
                return false;

            AutoResolveName arn(ccx, id);
            if (resolved)
                *resolved = true;
            return JS_DefinePropertyById(ccx, obj, id, jso,
                                         propFlags & ~JSPROP_ENUMERATE);
        }
        if (resolved)
            *resolved = false;
        return true;
    }

    if (member->IsConstant()) {
        RootedValue val(ccx);
        AutoResolveName arn(ccx, id);
        if (resolved)
            *resolved = true;
        return member->GetConstantValue(ccx, iface, val.address()) &&
               JS_DefinePropertyById(ccx, obj, id, val, propFlags);
    }

    if (scope->HasInterposition()) {
        Rooted<PropertyDescriptor> desc(ccx);
        if (!xpc::InterposeProperty(ccx, obj, iface->GetIID(), id, &desc))
            return false;

        if (desc.object()) {
            AutoResolveName arn(ccx, id);
            if (resolved)
                *resolved = true;
            desc.attributesRef() |= JSPROP_RESOLVING;
            return JS_DefinePropertyById(ccx, obj, id, desc);
        }
    }

    if (id == xpccx->GetStringID(XPCJSContext::IDX_TO_STRING) ||
        id == xpccx->GetStringID(XPCJSContext::IDX_TO_SOURCE) ||
        (scr &&
         scr->DontEnumQueryInterface() &&
         id == xpccx->GetStringID(XPCJSContext::IDX_QUERY_INTERFACE)))
        propFlags &= ~JSPROP_ENUMERATE;

    RootedValue funval(ccx);
    if (!member->NewFunctionObject(ccx, iface, obj, funval.address()))
        return false;

    if (member->IsMethod()) {
        AutoResolveName arn(ccx, id);
        if (resolved)
            *resolved = true;
        return JS_DefinePropertyById(ccx, obj, id, funval, propFlags);
    }

    // else...

    MOZ_ASSERT(member->IsAttribute(), "way broken!");

    propFlags |= JSPROP_GETTER | JSPROP_SHARED;
    propFlags &= ~JSPROP_READONLY;
    JSObject* funobj = funval.toObjectOrNull();
    JSNative getter = JS_DATA_TO_FUNC_PTR(JSNative, funobj);
    JSNative setter;
    if (member->IsWritableAttribute()) {
        propFlags |= JSPROP_SETTER;
        setter = JS_DATA_TO_FUNC_PTR(JSNative, funobj);
    } else {
        setter = nullptr;
    }

    AutoResolveName arn(ccx, id);
    if (resolved)
        *resolved = true;

    return JS_DefinePropertyById(ccx, obj, id, UndefinedHandleValue, propFlags, getter, setter);
}

/***************************************************************************/
/***************************************************************************/

static bool
XPC_WN_OnlyIWrite_AddPropertyStub(JSContext* cx, HandleObject obj, HandleId id, HandleValue v)
{
    XPCCallContext ccx(cx, obj, nullptr, id);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    // Allow only XPConnect to add/set the property
    if (ccx.GetResolveName() == id)
        return true;

    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

bool
XPC_WN_CannotModifyPropertyStub(JSContext* cx, HandleObject obj, HandleId id,
                                HandleValue v)
{
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

bool
XPC_WN_CannotDeletePropertyStub(JSContext* cx, HandleObject obj, HandleId id,
                                ObjectOpResult& result)
{
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

bool
XPC_WN_CannotModifySetPropertyStub(JSContext* cx, HandleObject obj, HandleId id,
                                   MutableHandleValue vp, ObjectOpResult& result)
{
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

bool
XPC_WN_Shared_Enumerate(JSContext* cx, HandleObject obj)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    // Since we aren't going to enumerate tearoff names and the prototype
    // handles non-mutated members, we can do this potential short-circuit.
    if (!wrapper->HasMutatedSet())
        return true;

    XPCNativeSet* set = wrapper->GetSet();
    XPCNativeSet* protoSet = wrapper->HasProto() ?
                                wrapper->GetProto()->GetSet() : nullptr;

    uint16_t interface_count = set->GetInterfaceCount();
    XPCNativeInterface** interfaceArray = set->GetInterfaceArray();
    for (uint16_t i = 0; i < interface_count; i++) {
        XPCNativeInterface* iface = interfaceArray[i];
        uint16_t member_count = iface->GetMemberCount();
        for (uint16_t k = 0; k < member_count; k++) {
            XPCNativeMember* member = iface->GetMemberAt(k);
            jsid name = member->GetName();

            // Skip if this member is going to come from the proto.
            uint16_t index;
            if (protoSet &&
                protoSet->FindMember(name, nullptr, &index) && index == i)
                continue;
            if (!xpc_ForcePropertyResolve(cx, obj, name))
                return false;
        }
    }
    return true;
}

/***************************************************************************/

enum WNHelperType {
    WN_NOHELPER,
    WN_HELPER
};

static void
WrappedNativeFinalize(js::FreeOp* fop, JSObject* obj, WNHelperType helperType)
{
    const js::Class* clazz = js::GetObjectClass(obj);
    if (clazz->flags & JSCLASS_DOM_GLOBAL) {
        mozilla::dom::DestroyProtoAndIfaceCache(obj);
    }
    nsISupports* p = static_cast<nsISupports*>(xpc_GetJSPrivate(obj));
    if (!p)
        return;

    XPCWrappedNative* wrapper = static_cast<XPCWrappedNative*>(p);
    if (helperType == WN_HELPER)
        wrapper->GetScriptable()->Finalize(wrapper, js::CastToJSFreeOp(fop), obj);
    wrapper->FlatJSObjectFinalized();
}

static void
WrappedNativeObjectMoved(JSObject* obj, const JSObject* old)
{
    nsISupports* p = static_cast<nsISupports*>(xpc_GetJSPrivate(obj));
    if (!p)
        return;

    XPCWrappedNative* wrapper = static_cast<XPCWrappedNative*>(p);
    wrapper->FlatJSObjectMoved(obj, old);
}

void
XPC_WN_NoHelper_Finalize(js::FreeOp* fop, JSObject* obj)
{
    WrappedNativeFinalize(fop, obj, WN_NOHELPER);
}

/*
 * General comment about XPConnect tracing: Given a C++ object |wrapper| and its
 * corresponding JS object |obj|, calling |wrapper->TraceSelf| will ask the JS
 * engine to mark |obj|. Eventually, this will lead to the trace hook being
 * called for |obj|. The trace hook should call |wrapper->TraceInside|, which
 * should mark any JS objects held by |wrapper| as members.
 */

/* static */ void
XPCWrappedNative::Trace(JSTracer* trc, JSObject* obj)
{
    const js::Class* clazz = js::GetObjectClass(obj);
    if (clazz->flags & JSCLASS_DOM_GLOBAL) {
        mozilla::dom::TraceProtoAndIfaceCache(trc, obj);
    }
    MOZ_ASSERT(IS_WN_CLASS(clazz));

    XPCWrappedNative* wrapper = XPCWrappedNative::Get(obj);
    if (wrapper && wrapper->IsValid())
        wrapper->TraceInside(trc);
}

void
XPCWrappedNative_Trace(JSTracer* trc, JSObject* obj)
{
    XPCWrappedNative::Trace(trc, obj);
}

static bool
XPC_WN_NoHelper_Resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    XPCCallContext ccx(cx, obj, nullptr, id);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeSet* set = ccx.GetSet();
    if (!set)
        return true;

    // Don't resolve properties that are on our prototype.
    if (ccx.GetInterface() && !ccx.GetStaticMemberIsLocal())
        return true;

    return DefinePropertyIfFound(ccx, obj, id,
                                 set, nullptr, nullptr, wrapper->GetScope(),
                                 true, wrapper, wrapper, nullptr,
                                 JSPROP_ENUMERATE |
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT,
                                 resolvedp);
}

static const js::ClassOps XPC_WN_NoHelper_JSClassOps = {
    XPC_WN_OnlyIWrite_AddPropertyStub, // addProperty
    XPC_WN_CannotDeletePropertyStub,   // delProperty
    nullptr,                           // getProperty
    nullptr,                           // setProperty
    XPC_WN_Shared_Enumerate,           // enumerate
    nullptr,                           // newEnumerate
    XPC_WN_NoHelper_Resolve,           // resolve
    nullptr,                           // mayResolve
    XPC_WN_NoHelper_Finalize,          // finalize
    nullptr,                           // call
    nullptr,                           // construct
    nullptr,                           // hasInstance
    XPCWrappedNative::Trace,           // trace
};

const js::ClassExtension XPC_WN_JSClassExtension = {
    nullptr, // weakmapKeyDelegateOp
    WrappedNativeObjectMoved
};

const js::Class XPC_WN_NoHelper_JSClass = {
    "XPCWrappedNative_NoHelper",
    XPC_WRAPPER_FLAGS |
    JSCLASS_IS_WRAPPED_NATIVE |
    JSCLASS_PRIVATE_IS_NSISUPPORTS |
    JSCLASS_FOREGROUND_FINALIZE,
    &XPC_WN_NoHelper_JSClassOps,
    JS_NULL_CLASS_SPEC,
    &XPC_WN_JSClassExtension,
    JS_NULL_OBJECT_OPS
};


/***************************************************************************/

bool
XPC_WN_MaybeResolvingPropertyStub(JSContext* cx, HandleObject obj, HandleId id, HandleValue v)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    if (ccx.GetResolvingWrapper() == wrapper)
        return true;
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

bool
XPC_WN_MaybeResolvingSetPropertyStub(JSContext* cx, HandleObject obj, HandleId id,
                                     MutableHandleValue vp, ObjectOpResult& result)
{
    result.succeed();
    return XPC_WN_MaybeResolvingPropertyStub(cx, obj, id, vp);
}

bool
XPC_WN_MaybeResolvingDeletePropertyStub(JSContext* cx, HandleObject obj, HandleId id,
                                        ObjectOpResult& result)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    if (ccx.GetResolvingWrapper() == wrapper) {
        return result.succeed();
    }
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

// macro fun!
#define PRE_HELPER_STUB                                                       \
    JSObject* unwrapped = js::CheckedUnwrap(obj, false);                      \
    if (!unwrapped) {                                                         \
        JS_ReportErrorASCII(cx, "Permission denied to operate on object.");   \
        return false;                                                         \
    }                                                                         \
    if (!IS_WN_REFLECTOR(unwrapped)) {                                        \
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);                    \
    }                                                                         \
    XPCWrappedNative* wrapper = XPCWrappedNative::Get(unwrapped);             \
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);                             \
    bool retval = true;                                                       \
    nsresult rv = wrapper->GetScriptable()->

#define POST_HELPER_STUB                                                      \
    if (NS_FAILED(rv))                                                        \
        return Throw(rv, cx);                                                 \
    return retval;

#define POST_HELPER_STUB_WITH_OBJECTOPRESULT(failMethod)                      \
    if (NS_FAILED(rv))                                                        \
        return Throw(rv, cx);                                                 \
    return retval ? result.succeed() : result.failMethod();

bool
XPC_WN_Helper_GetProperty(JSContext* cx, HandleObject obj, HandleId id,
                          MutableHandleValue vp)
{
    PRE_HELPER_STUB
    GetProperty(wrapper, cx, obj, id, vp.address(), &retval);
    POST_HELPER_STUB
}

bool
XPC_WN_Helper_SetProperty(JSContext* cx, HandleObject obj, HandleId id,
                          MutableHandleValue vp, ObjectOpResult& result)
{
    PRE_HELPER_STUB
    SetProperty(wrapper, cx, obj, id, vp.address(), &retval);
    POST_HELPER_STUB_WITH_OBJECTOPRESULT(failReadOnly)
}

bool
XPC_WN_Helper_Call(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    // N.B. we want obj to be the callee, not JS_THIS(cx, vp)
    RootedObject obj(cx, &args.callee());

    XPCCallContext ccx(cx, obj, nullptr, JSID_VOIDHANDLE, args.length(),
                       args.array(), args.rval().address());
    if (!ccx.IsValid())
        return false;

    PRE_HELPER_STUB
    Call(wrapper, cx, obj, args, &retval);
    POST_HELPER_STUB
}

bool
XPC_WN_Helper_Construct(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    RootedObject obj(cx, &args.callee());
    if (!obj)
        return false;

    XPCCallContext ccx(cx, obj, nullptr, JSID_VOIDHANDLE, args.length(),
                       args.array(), args.rval().address());
    if (!ccx.IsValid())
        return false;

    PRE_HELPER_STUB
    Construct(wrapper, cx, obj, args, &retval);
    POST_HELPER_STUB
}

bool
XPC_WN_Helper_HasInstance(JSContext* cx, HandleObject obj, MutableHandleValue valp, bool* bp)
{
    bool retval2;
    PRE_HELPER_STUB
    HasInstance(wrapper, cx, obj, valp, &retval2, &retval);
    *bp = retval2;
    POST_HELPER_STUB
}

void
XPC_WN_Helper_Finalize(js::FreeOp* fop, JSObject* obj)
{
    WrappedNativeFinalize(fop, obj, WN_HELPER);
}

bool
XPC_WN_Helper_Resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    nsresult rv = NS_OK;
    bool retval = true;
    bool resolved = false;
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    RootedId old(cx, ccx.SetResolveName(id));

    nsCOMPtr<nsIXPCScriptable> scr = wrapper->GetScriptable();
    if (scr && scr->WantResolve()) {
        XPCWrappedNative* oldResolvingWrapper;
        bool allowPropMods = scr->AllowPropModsDuringResolve();

        if (allowPropMods)
            oldResolvingWrapper = ccx.SetResolvingWrapper(wrapper);

        rv = scr->Resolve(wrapper, cx, obj, id, &resolved, &retval);

        if (allowPropMods)
            (void)ccx.SetResolvingWrapper(oldResolvingWrapper);
    }

    old = ccx.SetResolveName(old);
    MOZ_ASSERT(old == id, "bad nest");

    if (NS_FAILED(rv)) {
        return Throw(rv, cx);
    }

    if (resolved) {
        *resolvedp = true;
    } else if (wrapper->HasMutatedSet()) {
        // We are here if scriptable did not resolve this property and
        // it *might* be in the instance set but not the proto set.

        XPCNativeSet* set = wrapper->GetSet();
        XPCNativeSet* protoSet = wrapper->HasProto() ?
                                    wrapper->GetProto()->GetSet() : nullptr;
        XPCNativeMember* member = nullptr;
        RefPtr<XPCNativeInterface> iface;
        bool IsLocal = false;

        if (set->FindMember(id, &member, &iface, protoSet, &IsLocal) &&
            IsLocal) {

            XPCWrappedNative* wrapperForInterfaceNames =
                (scr && scr->DontReflectInterfaceNames()) ? nullptr : wrapper;

            XPCWrappedNative* oldResolvingWrapper =
                ccx.SetResolvingWrapper(wrapper);
            retval = DefinePropertyIfFound(ccx, obj, id,
                                           set, iface, member,
                                           wrapper->GetScope(),
                                           false,
                                           wrapperForInterfaceNames,
                                           nullptr, scr,
                                           JSPROP_ENUMERATE, resolvedp);
            (void)ccx.SetResolvingWrapper(oldResolvingWrapper);
        }
    }

    return retval;
}

bool
XPC_WN_Helper_Enumerate(JSContext* cx, HandleObject obj)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    nsCOMPtr<nsIXPCScriptable> scr = wrapper->GetScriptable();
    if (!scr || !scr->WantEnumerate())
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    if (!XPC_WN_Shared_Enumerate(cx, obj))
        return false;

    bool retval = true;
    nsresult rv = scr->Enumerate(wrapper, cx, obj, &retval);
    if (NS_FAILED(rv))
        return Throw(rv, cx);
    return retval;
}

/***************************************************************************/

bool
XPC_WN_NewEnumerate(JSContext* cx, HandleObject obj, AutoIdVector& properties,
                    bool enumerableOnly)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    nsCOMPtr<nsIXPCScriptable> scr = wrapper->GetScriptable();
    if (!scr || !scr->WantNewEnumerate())
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    if (!XPC_WN_Shared_Enumerate(cx, obj))
        return false;

    bool retval = true;
    nsresult rv = scr->NewEnumerate(wrapper, cx, obj, properties, &retval);
    if (NS_FAILED(rv))
        return Throw(rv, cx);
    return retval;
}

/***************************************************************************/
/***************************************************************************/

// Compatibility hack.
//
// XPConnect used to do all sorts of funny tricks to find the "correct"
// |this| object for a given method (often to the detriment of proper
// call/apply). When these tricks were removed, a fair amount of chrome
// code broke, because it was relying on being able to grab methods off
// some XPCOM object (like the nsITelemetry service) and invoke them without
// a proper |this|. So, if it's quite clear that we're in this situation and
// about to use a |this| argument that just won't work, fix things up.
//
// This hack is only useful for getters/setters if someone sets an XPCOM object
// as the prototype for a vanilla JS object and expects the XPCOM attributes to
// work on the derived object, which we really don't want to support. But we
// handle it anyway, for now, to minimize regression risk on an already-risky
// landing.
//
// This hack is mainly useful for the NoHelper JSClass. We also fix up
// Components.utils because it implements nsIXPCScriptable (giving it a custom
// JSClass) but not nsIClassInfo (which would put the methods on a prototype).

#define IS_NOHELPER_CLASS(clasp) (clasp == &XPC_WN_NoHelper_JSClass)
#define IS_CU_CLASS(clasp) (clasp->name[0] == 'n' && !strcmp(clasp->name, "nsXPCComponents_Utils"))

MOZ_ALWAYS_INLINE JSObject*
FixUpThisIfBroken(JSObject* obj, JSObject* funobj)
{
    if (funobj) {
        JSObject* parentObj =
            &js::GetFunctionNativeReserved(funobj,
                                           XPC_FUNCTION_PARENT_OBJECT_SLOT).toObject();
        const js::Class* parentClass = js::GetObjectClass(parentObj);
        if (MOZ_UNLIKELY((IS_NOHELPER_CLASS(parentClass) || IS_CU_CLASS(parentClass)) &&
                         (js::GetObjectClass(obj) != parentClass)))
        {
            return parentObj;
        }
    }
    return obj;
}

bool
XPC_WN_CallMethod(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    MOZ_ASSERT(JS_TypeOfValue(cx, args.calleev()) == JSTYPE_FUNCTION, "bad function");
    RootedObject funobj(cx, &args.callee());

    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    obj = FixUpThisIfBroken(obj, funobj);
    XPCCallContext ccx(cx, obj, funobj, JSID_VOIDHANDLE, args.length(),
                       args.array(), vp);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    RefPtr<XPCNativeInterface> iface;
    XPCNativeMember*    member;

    if (!XPCNativeMember::GetCallInfo(funobj, &iface, &member))
        return Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx);
    ccx.SetCallInfo(iface, member, false);
    return XPCWrappedNative::CallMethod(ccx);
}

bool
XPC_WN_GetterSetter(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    MOZ_ASSERT(JS_TypeOfValue(cx, args.calleev()) == JSTYPE_FUNCTION, "bad function");
    RootedObject funobj(cx, &args.callee());

    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    obj = FixUpThisIfBroken(obj, funobj);
    XPCCallContext ccx(cx, obj, funobj, JSID_VOIDHANDLE, args.length(),
                       args.array(), vp);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    RefPtr<XPCNativeInterface> iface;
    XPCNativeMember*    member;

    if (!XPCNativeMember::GetCallInfo(funobj, &iface, &member))
        return Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx);

    if (args.length() != 0 && member->IsWritableAttribute()) {
        ccx.SetCallInfo(iface, member, true);
        bool retval = XPCWrappedNative::SetAttribute(ccx);
        if (retval)
            args.rval().set(args[0]);
        return retval;
    }
    // else...

    ccx.SetCallInfo(iface, member, false);
    return XPCWrappedNative::GetAttribute(ccx);
}

/***************************************************************************/

static bool
XPC_WN_Shared_Proto_Enumerate(JSContext* cx, HandleObject obj)
{
    MOZ_ASSERT(js::GetObjectClass(obj) == &XPC_WN_ModsAllowed_Proto_JSClass ||
               js::GetObjectClass(obj) == &XPC_WN_NoMods_Proto_JSClass,
               "bad proto");
    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCNativeSet* set = self->GetSet();
    if (!set)
        return false;

    XPCCallContext ccx(cx);
    if (!ccx.IsValid())
        return false;

    uint16_t interface_count = set->GetInterfaceCount();
    XPCNativeInterface** interfaceArray = set->GetInterfaceArray();
    for (uint16_t i = 0; i < interface_count; i++) {
        XPCNativeInterface* iface = interfaceArray[i];
        uint16_t member_count = iface->GetMemberCount();

        for (uint16_t k = 0; k < member_count; k++) {
            if (!xpc_ForcePropertyResolve(cx, obj, iface->GetMemberAt(k)->GetName()))
                return false;
        }
    }

    return true;
}

static void
XPC_WN_Shared_Proto_Finalize(js::FreeOp* fop, JSObject* obj)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p = (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (p)
        p->JSProtoObjectFinalized(fop, obj);
}

static void
XPC_WN_Shared_Proto_ObjectMoved(JSObject* obj, const JSObject* old)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p = (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (p)
        p->JSProtoObjectMoved(obj, old);
}

static void
XPC_WN_Shared_Proto_Trace(JSTracer* trc, JSObject* obj)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (p)
        p->TraceInside(trc);
}

/*****************************************************/

static bool
XPC_WN_ModsAllowed_Proto_Resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvep)
{
    MOZ_ASSERT(js::GetObjectClass(obj) == &XPC_WN_ModsAllowed_Proto_JSClass,
               "bad proto");

    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCCallContext ccx(cx);
    if (!ccx.IsValid())
        return false;

    nsCOMPtr<nsIXPCScriptable> scr = self->GetScriptable();
    return DefinePropertyIfFound(ccx, obj, id,
                                 self->GetSet(), nullptr, nullptr,
                                 self->GetScope(),
                                 true, nullptr, nullptr, scr,
                                 JSPROP_ENUMERATE, resolvep);
}

static const js::ClassOps XPC_WN_ModsAllowed_Proto_JSClassOps = {
    nullptr,                            // addProperty
    nullptr,                            // delProperty
    nullptr,                            // getProperty
    nullptr,                            // setProperty
    XPC_WN_Shared_Proto_Enumerate,      // enumerate
    nullptr,                            // newEnumerate
    XPC_WN_ModsAllowed_Proto_Resolve,   // resolve
    nullptr,                            // mayResolve
    XPC_WN_Shared_Proto_Finalize,       // finalize
    nullptr,                            // call
    nullptr,                            // construct
    nullptr,                            // hasInstance
    XPC_WN_Shared_Proto_Trace,          // trace
};

static const js::ClassExtension XPC_WN_Shared_Proto_ClassExtension = {
    nullptr,    /* weakmapKeyDelegateOp */
    XPC_WN_Shared_Proto_ObjectMoved
};

const js::Class XPC_WN_ModsAllowed_Proto_JSClass = {
    "XPC_WN_ModsAllowed_Proto_JSClass",
    XPC_WRAPPER_FLAGS,
    &XPC_WN_ModsAllowed_Proto_JSClassOps,
    JS_NULL_CLASS_SPEC,
    &XPC_WN_Shared_Proto_ClassExtension,
    JS_NULL_OBJECT_OPS
};

/***************************************************************************/

static bool
XPC_WN_OnlyIWrite_Proto_AddPropertyStub(JSContext* cx, HandleObject obj, HandleId id,
                                        HandleValue v)
{
    MOZ_ASSERT(js::GetObjectClass(obj) == &XPC_WN_NoMods_Proto_JSClass,
               "bad proto");

    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCCallContext ccx(cx);
    if (!ccx.IsValid())
        return false;

    // Allow XPConnect to add the property only
    if (ccx.GetResolveName() == id)
        return true;

    return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);
}

static bool
XPC_WN_NoMods_Proto_Resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    MOZ_ASSERT(js::GetObjectClass(obj) == &XPC_WN_NoMods_Proto_JSClass,
               "bad proto");

    XPCWrappedNativeProto* self =
        (XPCWrappedNativeProto*) xpc_GetJSPrivate(obj);
    if (!self)
        return false;

    XPCCallContext ccx(cx);
    if (!ccx.IsValid())
        return false;

    nsCOMPtr<nsIXPCScriptable> scr = self->GetScriptable();

    return DefinePropertyIfFound(ccx, obj, id,
                                 self->GetSet(), nullptr, nullptr,
                                 self->GetScope(),
                                 true, nullptr, nullptr, scr,
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT |
                                 JSPROP_ENUMERATE, resolvedp);
}

static const js::ClassOps XPC_WN_NoMods_Proto_JSClassOps = {
    XPC_WN_OnlyIWrite_Proto_AddPropertyStub,   // addProperty
    XPC_WN_CannotDeletePropertyStub,           // delProperty
    nullptr,                                   // getProperty
    nullptr,                                   // setProperty
    XPC_WN_Shared_Proto_Enumerate,             // enumerate
    nullptr,                                   // newEnumerate
    XPC_WN_NoMods_Proto_Resolve,               // resolve
    nullptr,                                   // mayResolve
    XPC_WN_Shared_Proto_Finalize,              // finalize
    nullptr,                                   // call
    nullptr,                                   // construct
    nullptr,                                   // hasInstance
    XPC_WN_Shared_Proto_Trace,                 // trace
};

const js::Class XPC_WN_NoMods_Proto_JSClass = {
    "XPC_WN_NoMods_Proto_JSClass",
    XPC_WRAPPER_FLAGS,
    &XPC_WN_NoMods_Proto_JSClassOps,
    JS_NULL_CLASS_SPEC,
    &XPC_WN_Shared_Proto_ClassExtension,
    JS_NULL_OBJECT_OPS
};

/***************************************************************************/

static bool
XPC_WN_TearOff_Enumerate(JSContext* cx, HandleObject obj)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCWrappedNativeTearOff* to = ccx.GetTearOff();
    XPCNativeInterface* iface;

    if (!to || nullptr == (iface = to->GetInterface()))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    uint16_t member_count = iface->GetMemberCount();
    for (uint16_t k = 0; k < member_count; k++) {
        if (!xpc_ForcePropertyResolve(cx, obj, iface->GetMemberAt(k)->GetName()))
            return false;
    }

    return true;
}

static bool
XPC_WN_TearOff_Resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    XPCCallContext ccx(cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCWrappedNativeTearOff* to = ccx.GetTearOff();
    XPCNativeInterface* iface;

    if (!to || nullptr == (iface = to->GetInterface()))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    return DefinePropertyIfFound(ccx, obj, id, nullptr, iface, nullptr,
                                 wrapper->GetScope(),
                                 true, nullptr, nullptr, nullptr,
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT |
                                 JSPROP_ENUMERATE, resolvedp);
}

static void
XPC_WN_TearOff_Finalize(js::FreeOp* fop, JSObject* obj)
{
    XPCWrappedNativeTearOff* p = (XPCWrappedNativeTearOff*)
        xpc_GetJSPrivate(obj);
    if (!p)
        return;
    p->JSObjectFinalized();
}

static void
XPC_WN_TearOff_ObjectMoved(JSObject* obj, const JSObject* old)
{
    XPCWrappedNativeTearOff* p = (XPCWrappedNativeTearOff*)
        xpc_GetJSPrivate(obj);
    if (!p)
        return;
    p->JSObjectMoved(obj, old);
}

// Make sure XPC_WRAPPER_FLAGS has no reserved slots, so our
// XPC_WN_TEAROFF_RESERVED_SLOTS value is OK.

static_assert(((XPC_WRAPPER_FLAGS >> JSCLASS_RESERVED_SLOTS_SHIFT) &
               JSCLASS_RESERVED_SLOTS_MASK) == 0,
              "XPC_WRAPPER_FLAGS should not include any reserved slots");

static const js::ClassOps XPC_WN_Tearoff_JSClassOps = {
    XPC_WN_OnlyIWrite_AddPropertyStub,  // addProperty
    XPC_WN_CannotDeletePropertyStub,    // delProperty
    nullptr,                            // getProperty
    nullptr,                            // setProperty
    XPC_WN_TearOff_Enumerate,           // enumerate
    nullptr,                            // newEnumerate
    XPC_WN_TearOff_Resolve,             // resolve
    nullptr,                            // mayResolve
    XPC_WN_TearOff_Finalize,            // finalize
    nullptr,                            // call
    nullptr,                            // construct
    nullptr,                            // hasInstance
    nullptr,                            // trace
};

static const js::ClassExtension XPC_WN_Tearoff_JSClassExtension = {
    nullptr,                            // weakmapKeyDelegateOp
    XPC_WN_TearOff_ObjectMoved
};

const js::Class XPC_WN_Tearoff_JSClass = {
    "WrappedNative_TearOff",
    XPC_WRAPPER_FLAGS |
    JSCLASS_HAS_RESERVED_SLOTS(XPC_WN_TEAROFF_RESERVED_SLOTS),
    &XPC_WN_Tearoff_JSClassOps,
    JS_NULL_CLASS_SPEC,
    &XPC_WN_Tearoff_JSClassExtension
};
