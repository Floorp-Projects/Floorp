/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The Components.Sandbox object.
 */

#include "AccessCheck.h"
#include "jsfriendapi.h"
#include "js/Proxy.h"
#include "js/StructuredClone.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsNullPrincipal.h"
#include "nsPrincipal.h"
#include "nsXMLHttpRequest.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "XrayWrapper.h"
#include "Crypto.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/CSSBinding.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/RTCIdentityProviderRegistrar.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/UnionConversions.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/URLSearchParamsBinding.h"

using namespace mozilla;
using namespace JS;
using namespace js;
using namespace xpc;

using mozilla::dom::DestroyProtoAndIfaceCache;
using mozilla::dom::indexedDB::IndexedDatabaseManager;

NS_IMPL_CYCLE_COLLECTION_CLASS(SandboxPrivate)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SandboxPrivate)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SandboxPrivate)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(SandboxPrivate)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SandboxPrivate)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SandboxPrivate)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SandboxPrivate)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

const char kScriptSecurityManagerContractID[] = NS_SCRIPTSECURITYMANAGER_CONTRACTID;

class nsXPCComponents_utils_Sandbox : public nsIXPCComponents_utils_Sandbox,
                                      public nsIXPCScriptable
{
public:
    // Aren't macros nice?
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_UTILS_SANDBOX
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCComponents_utils_Sandbox();

private:
    virtual ~nsXPCComponents_utils_Sandbox();

    static nsresult CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                    JSContext* cx, HandleObject obj,
                                    const CallArgs& args, bool* _retval);
};

already_AddRefed<nsIXPCComponents_utils_Sandbox>
xpc::NewSandboxConstructor()
{
    nsCOMPtr<nsIXPCComponents_utils_Sandbox> sbConstructor =
        new nsXPCComponents_utils_Sandbox();
    return sbConstructor.forget();
}

static bool
SandboxDump(JSContext* cx, unsigned argc, jsval* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0)
        return true;

    RootedString str(cx, ToString(cx, args[0]));
    if (!str)
        return false;

    JSAutoByteString utf8str;
    char* cstr = utf8str.encodeUtf8(cx, str);
    if (!cstr)
        return false;

#if defined(XP_MACOSX)
    // Be nice and convert all \r to \n.
    char* c = cstr;
    char* cEnd = cstr + strlen(cstr);
    while (c < cEnd) {
        if (*c == '\r')
            *c = '\n';
        c++;
    }
#endif
#ifdef ANDROID
    __android_log_write(ANDROID_LOG_INFO, "GeckoDump", cstr);
#endif

    fputs(cstr, stdout);
    fflush(stdout);
    args.rval().setBoolean(true);
    return true;
}

static bool
SandboxDebug(JSContext* cx, unsigned argc, jsval* vp)
{
#ifdef DEBUG
    return SandboxDump(cx, argc, vp);
#else
    return true;
#endif
}

static bool
SandboxImport(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args[0].isPrimitive()) {
        XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);
        return false;
    }

    RootedString funname(cx);
    if (args.length() > 1) {
        // Use the second parameter as the function name.
        funname = ToString(cx, args[1]);
        if (!funname)
            return false;
    } else {
        // NB: funobj must only be used to get the JSFunction out.
        RootedObject funobj(cx, &args[0].toObject());
        if (js::IsProxy(funobj)) {
            funobj = XPCWrapper::UnsafeUnwrapSecurityWrapper(funobj);
        }

        JSAutoCompartment ac(cx, funobj);

        RootedValue funval(cx, ObjectValue(*funobj));
        JSFunction* fun = JS_ValueToFunction(cx, funval);
        if (!fun) {
            XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);
            return false;
        }

        // Use the actual function name as the name.
        funname = JS_GetFunctionId(fun);
        if (!funname) {
            XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);
            return false;
        }
    }

    RootedId id(cx);
    if (!JS_StringToId(cx, funname, &id))
        return false;

    // We need to resolve the this object, because this function is used
    // unbound and should still work and act on the original sandbox.
    RootedObject thisObject(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisObject) {
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
        return false;
    }
    if (!JS_SetPropertyById(cx, thisObject, id, args[0]))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
SandboxCreateCrypto(JSContext* cx, JS::HandleObject obj)
{
    MOZ_ASSERT(JS_IsGlobalObject(obj));

    nsIGlobalObject* native = xpc::NativeGlobal(obj);
    MOZ_ASSERT(native);

    dom::Crypto* crypto = new dom::Crypto();
    crypto->Init(native);
    JS::RootedObject wrapped(cx, crypto->WrapObject(cx, nullptr));
    return JS_DefineProperty(cx, obj, "crypto", wrapped, JSPROP_ENUMERATE);
}

static bool
SandboxCreateRTCIdentityProvider(JSContext* cx, JS::HandleObject obj)
{
    MOZ_ASSERT(JS_IsGlobalObject(obj));

    nsCOMPtr<nsIGlobalObject> nativeGlobal = xpc::NativeGlobal(obj);
    MOZ_ASSERT(nativeGlobal);

    dom::RTCIdentityProviderRegistrar* registrar =
            new dom::RTCIdentityProviderRegistrar(nativeGlobal);
    JS::RootedObject wrapped(cx, registrar->WrapObject(cx, nullptr));
    return JS_DefineProperty(cx, obj, "rtcIdentityProvider", wrapped, JSPROP_ENUMERATE);
}

static bool
SetFetchRequestFromValue(JSContext *cx, RequestOrUSVString& request,
                         const MutableHandleValue& requestOrUrl)
{
    RequestOrUSVStringArgument requestHolder(request);
    bool noMatch = true;
    if (requestOrUrl.isObject() &&
        !requestHolder.TrySetToRequest(cx, requestOrUrl, noMatch, false)) {
        return false;
    }
    if (noMatch &&
        !requestHolder.TrySetToUSVString(cx, requestOrUrl, noMatch)) {
        return false;
    }
    if (noMatch) {
        return false;
    }
    return true;
}

static bool
SandboxFetch(JSContext* cx, JS::HandleObject scope, const CallArgs& args)
{
    if (args.length() < 1) {
        JS_ReportError(cx, "fetch requires at least 1 argument");
        return false;
    }

    RequestOrUSVString request;
    if (!SetFetchRequestFromValue(cx, request, args[0])) {
        JS_ReportError(cx, "fetch requires a string or Request in argument 1");
        return false;
    }
    RootedDictionary<dom::RequestInit> options(cx);
    if (!options.Init(cx, args.hasDefined(1) ? args[1] : JS::NullHandleValue,
                      "Argument 2 of fetch", false)) {
        return false;
    }
    nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(scope);
    if (!global) {
        return false;
    }
    ErrorResult rv;
    nsRefPtr<dom::Promise> response =
        FetchRequest(global, Constify(request), Constify(options), rv);
    rv.WouldReportJSException();
    if (rv.Failed()) {
        return ThrowMethodFailedWithDetails(cx, rv, "Sandbox", "fetch");
    }
    if (!GetOrCreateDOMReflector(cx, scope, response, args.rval())) {
        return false;
    }
    return true;
}

static bool SandboxFetchPromise(JSContext* cx, unsigned argc, jsval* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());
    RootedObject scope(cx, JS::CurrentGlobalOrNull(cx));
    if (SandboxFetch(cx, scope, args)) {
        return true;
    }
    return ConvertExceptionToPromise(cx, scope, args.rval());
}


static bool
SandboxCreateFetch(JSContext* cx, HandleObject obj)
{
    MOZ_ASSERT(JS_IsGlobalObject(obj));

    return JS_DefineFunction(cx, obj, "fetch", SandboxFetchPromise, 2, 0) &&
        dom::RequestBinding::GetConstructorObject(cx, obj) &&
        dom::ResponseBinding::GetConstructorObject(cx, obj) &&
        dom::HeadersBinding::GetConstructorObject(cx, obj);
}

static bool
SandboxIsProxy(JSContext* cx, unsigned argc, jsval* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportError(cx, "Function requires at least 1 argument");
        return false;
    }
    if (!args[0].isObject()) {
        args.rval().setBoolean(false);
        return true;
    }

    RootedObject obj(cx, &args[0].toObject());
    obj = js::CheckedUnwrap(obj);
    NS_ENSURE_TRUE(obj, false);

    args.rval().setBoolean(js::IsScriptedProxy(obj));
    return true;
}

/*
 * Expected type of the arguments and the return value:
 * function exportFunction(function funToExport,
 *                         object targetScope,
 *                         [optional] object options)
 */
static bool
SandboxExportFunction(JSContext* cx, unsigned argc, jsval* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportError(cx, "Function requires at least 2 arguments");
        return false;
    }

    RootedValue options(cx, args.length() > 2 ? args[2] : UndefinedValue());
    return ExportFunction(cx, args[0], args[1], options, args.rval());
}

static bool
SandboxCreateObjectIn(JSContext* cx, unsigned argc, jsval* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportError(cx, "Function requires at least 1 argument");
        return false;
    }

    RootedObject optionsObj(cx);
    bool calledWithOptions = args.length() > 1;
    if (calledWithOptions) {
        if (!args[1].isObject()) {
            JS_ReportError(cx, "Expected the 2nd argument (options) to be an object");
            return false;
        }
        optionsObj = &args[1].toObject();
    }

    CreateObjectInOptions options(cx, optionsObj);
    if (calledWithOptions && !options.Parse())
        return false;

    return xpc::CreateObjectIn(cx, args[0], options, args.rval());
}

static bool
SandboxCloneInto(JSContext* cx, unsigned argc, jsval* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportError(cx, "Function requires at least 2 arguments");
        return false;
    }

    RootedValue options(cx, args.length() > 2 ? args[2] : UndefinedValue());
    return xpc::CloneInto(cx, args[0], args[1], options, args.rval());
}

static bool
sandbox_enumerate(JSContext* cx, HandleObject obj)
{
    return JS_EnumerateStandardClasses(cx, obj);
}

static bool
sandbox_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    return JS_ResolveStandardClass(cx, obj, id, resolvedp);
}

static void
sandbox_finalize(js::FreeOp* fop, JSObject* obj)
{
    nsIScriptObjectPrincipal* sop =
        static_cast<nsIScriptObjectPrincipal*>(xpc_GetJSPrivate(obj));
    if (!sop) {
        // sop can be null if CreateSandboxObject fails in the middle.
        return;
    }

    static_cast<SandboxPrivate*>(sop)->ForgetGlobalObject();
    NS_RELEASE(sop);
    DestroyProtoAndIfaceCache(obj);
}

static void
sandbox_moved(JSObject* obj, const JSObject* old)
{
    // Note that this hook can be called before the private pointer is set. In
    // this case the SandboxPrivate will not exist yet, so there is nothing to
    // do.
    nsIScriptObjectPrincipal* sop =
        static_cast<nsIScriptObjectPrincipal*>(xpc_GetJSPrivate(obj));
    if (sop)
        static_cast<SandboxPrivate*>(sop)->ObjectMoved(obj, old);
}

static bool
sandbox_convert(JSContext* cx, HandleObject obj, JSType type, MutableHandleValue vp)
{
    if (type == JSTYPE_OBJECT) {
        vp.setObject(*obj);
        return true;
    }

    return OrdinaryToPrimitive(cx, obj, type, vp);
}

static bool
writeToProto_setProperty(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                         JS::MutableHandleValue vp, JS::ObjectOpResult& result)
{
    RootedObject proto(cx);
    if (!JS_GetPrototype(cx, obj, &proto))
        return false;

    RootedValue receiver(cx, ObjectValue(*proto));
    return JS_ForwardSetPropertyTo(cx, proto, id, vp, receiver, result);
}

static bool
writeToProto_getProperty(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                    JS::MutableHandleValue vp)
{
    RootedObject proto(cx);
    if (!JS_GetPrototype(cx, obj, &proto))
        return false;

    return JS_GetPropertyById(cx, proto, id, vp);
}

struct AutoSkipPropertyMirroring
{
    explicit AutoSkipPropertyMirroring(CompartmentPrivate* priv) : priv(priv) {
        MOZ_ASSERT(!priv->skipWriteToGlobalPrototype);
        priv->skipWriteToGlobalPrototype = true;
    }
    ~AutoSkipPropertyMirroring() {
        MOZ_ASSERT(priv->skipWriteToGlobalPrototype);
        priv->skipWriteToGlobalPrototype = false;
    }

  private:
    CompartmentPrivate* priv;
};

// This hook handles the case when writeToGlobalPrototype is set on the
// sandbox. This flag asks that any properties defined on the sandbox global
// also be defined on the sandbox global's prototype. Whenever one of these
// properties is changed (on either side), the change should be reflected on
// both sides. We use this functionality to create sandboxes that are
// essentially "sub-globals" of another global. This is useful for running
// add-ons in a separate compartment while still giving them access to the
// chrome window.
static bool
sandbox_addProperty(JSContext* cx, HandleObject obj, HandleId id, HandleValue v)
{
    CompartmentPrivate* priv = CompartmentPrivate::Get(obj);
    MOZ_ASSERT(priv->writeToGlobalPrototype);

    // Whenever JS_EnumerateStandardClasses is called (by sandbox_enumerate for
    // example), it defines the "undefined" property, even if it's already
    // defined. We don't want to do anything in that case.
    if (id == XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_UNDEFINED))
        return true;

    // Avoid recursively triggering sandbox_addProperty in the
    // JS_DefinePropertyById call below.
    if (priv->skipWriteToGlobalPrototype)
        return true;

    AutoSkipPropertyMirroring askip(priv);

    RootedObject proto(cx);
    if (!JS_GetPrototype(cx, obj, &proto))
        return false;

    // After bug 1015790 is fixed, we should be able to remove this unwrapping.
    RootedObject unwrappedProto(cx, js::UncheckedUnwrap(proto, /* stopAtOuter = */ false));

    Rooted<JSPropertyDescriptor> pd(cx);
    if (!JS_GetPropertyDescriptorById(cx, proto, id, &pd))
        return false;

    // This is a little icky. If the property exists and is not configurable,
    // then JS_CopyPropertyFrom will throw an exception when we try to do a
    // normal assignment since it will think we're trying to remove the
    // non-configurability. So we do JS_SetPropertyById in that case.
    //
    // However, in the case of |const x = 3|, we get called once for
    // JSOP_DEFCONST and once for JSOP_SETCONST. The first one creates the
    // property as readonly and configurable. The second one changes the
    // attributes to readonly and not configurable. If we use JS_SetPropertyById
    // for the second call, it will throw an exception because the property is
    // readonly. We have to use JS_CopyPropertyFrom since it ignores the
    // readonly attribute (as it calls JSObject::defineProperty). See bug
    // 1019181.
    if (pd.object() && !pd.configurable()) {
        if (!JS_SetPropertyById(cx, proto, id, v))
            return false;
    } else {
        if (!JS_CopyPropertyFrom(cx, id, unwrappedProto, obj,
                                 MakeNonConfigurableIntoConfigurable))
            return false;
    }

    if (!JS_GetPropertyDescriptorById(cx, obj, id, &pd))
        return false;
    unsigned attrs = pd.attributes() & ~(JSPROP_GETTER | JSPROP_SETTER);
    if (!JS_DefinePropertyById(cx, obj, id, v,
                               attrs | JSPROP_PROPOP_ACCESSORS | JSPROP_REDEFINE_NONCONFIGURABLE,
                               JS_PROPERTYOP_GETTER(writeToProto_getProperty),
                               JS_PROPERTYOP_SETTER(writeToProto_setProperty)))
        return false;

    return true;
}

#define XPCONNECT_SANDBOX_CLASS_METADATA_SLOT (XPCONNECT_GLOBAL_EXTRA_SLOT_OFFSET)

static const js::Class SandboxClass = {
    "Sandbox",
    XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(1),
    nullptr, nullptr, nullptr, nullptr,
    sandbox_enumerate, sandbox_resolve,
    nullptr,        /* mayResolve */
    sandbox_convert,  sandbox_finalize,
    nullptr, nullptr, nullptr, JS_GlobalObjectTraceHook,
    JS_NULL_CLASS_SPEC,
    {
      nullptr,      /* outerObject */
      nullptr,      /* innerObject */
      false,        /* isWrappedNative */
      nullptr,      /* weakmapKeyDelegateOp */
      sandbox_moved /* objectMovedOp */
    },
    JS_NULL_OBJECT_OPS
};

// Note to whomever comes here to remove addProperty hooks: billm has promised
// to do the work for this class.
static const js::Class SandboxWriteToProtoClass = {
    "Sandbox",
    XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(1),
    sandbox_addProperty, nullptr, nullptr, nullptr,
    sandbox_enumerate, sandbox_resolve,
    nullptr,        /* mayResolve */
    sandbox_convert,  sandbox_finalize,
    nullptr, nullptr, nullptr, JS_GlobalObjectTraceHook,
    JS_NULL_CLASS_SPEC,
    {
      nullptr,      /* outerObject */
      nullptr,      /* innerObject */
      false,        /* isWrappedNative */
      nullptr,      /* weakmapKeyDelegateOp */
      sandbox_moved /* objectMovedOp */
    },
    JS_NULL_OBJECT_OPS
};

static const JSFunctionSpec SandboxFunctions[] = {
    JS_FS("dump",    SandboxDump,    1,0),
    JS_FS("debug",   SandboxDebug,   1,0),
    JS_FS("importFunction", SandboxImport, 1,0),
    JS_FS_END
};

bool
xpc::IsSandbox(JSObject* obj)
{
    const Class* clasp = GetObjectClass(obj);
    return clasp == &SandboxClass || clasp == &SandboxWriteToProtoClass;
}

/***************************************************************************/
nsXPCComponents_utils_Sandbox::nsXPCComponents_utils_Sandbox()
{
}

nsXPCComponents_utils_Sandbox::~nsXPCComponents_utils_Sandbox()
{
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_utils_Sandbox)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_utils_Sandbox)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_utils_Sandbox)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsXPCComponents_utils_Sandbox)
NS_IMPL_RELEASE(nsXPCComponents_utils_Sandbox)

// We use the nsIXPScriptable macros to generate lots of stuff for us.
#define XPC_MAP_CLASSNAME           nsXPCComponents_utils_Sandbox
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_utils_Sandbox"
#define                             XPC_MAP_WANT_CALL
#define                             XPC_MAP_WANT_CONSTRUCT
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This #undef's the above. */

const xpc::SandboxProxyHandler xpc::sandboxProxyHandler;

bool
xpc::IsSandboxPrototypeProxy(JSObject* obj)
{
    return js::IsProxy(obj) &&
           js::GetProxyHandler(obj) == &xpc::sandboxProxyHandler;
}

bool
xpc::SandboxCallableProxyHandler::call(JSContext* cx, JS::Handle<JSObject*> proxy,
                                       const JS::CallArgs& args) const
{
    // We forward the call to our underlying callable.

    // Get our SandboxProxyHandler proxy.
    RootedObject sandboxProxy(cx, getSandboxProxy(proxy));
    MOZ_ASSERT(js::IsProxy(sandboxProxy) &&
               js::GetProxyHandler(sandboxProxy) == &xpc::sandboxProxyHandler);

    // The global of the sandboxProxy is the sandbox global, and the
    // target object is the original proto.
    RootedObject sandboxGlobal(cx,
      js::GetGlobalForObjectCrossCompartment(sandboxProxy));
    MOZ_ASSERT(IsSandbox(sandboxGlobal));

    // If our this object is the sandbox global, we call with this set to the
    // original proto instead.
    //
    // There are two different ways we can compute |this|. If we use
    // JS_THIS_VALUE, we'll get the bonafide |this| value as passed by the
    // caller, which may be undefined if a global function was invoked without
    // an explicit invocant. If we use JS_THIS or JS_THIS_OBJECT, the |this|
    // in |vp| will be coerced to the global, which is not the correct
    // behavior in ES5 strict mode. And we have no way to compute strictness
    // here.
    //
    // The naive approach is simply to use JS_THIS_VALUE here. If |this| was
    // explicit, we can remap it appropriately. If it was implicit, then we
    // leave it as undefined, and let the callee sort it out. Since the callee
    // is generally in the same compartment as its global (eg the Window's
    // compartment, not the Sandbox's), the callee will generally compute the
    // correct |this|.
    //
    // However, this breaks down in the Xray case. If the sandboxPrototype
    // is an Xray wrapper, then we'll end up reifying the native methods in
    // the Sandbox's scope, which means that they'll compute |this| to be the
    // Sandbox, breaking old-style XPC_WN_CallMethod methods.
    //
    // Luckily, the intent of Xrays is to provide a vanilla view of a foreign
    // DOM interface, which means that we don't care about script-enacted
    // strictness in the prototype's home compartment. Indeed, since DOM
    // methods are always non-strict, we can just assume non-strict semantics
    // if the sandboxPrototype is an Xray Wrapper, which lets us appropriately
    // remap |this|.
    bool isXray = WrapperFactory::IsXrayWrapper(sandboxProxy);
    RootedValue thisVal(cx, isXray ? args.computeThis(cx) : args.thisv());
    if (thisVal == ObjectValue(*sandboxGlobal)) {
        thisVal = ObjectValue(*js::GetProxyTargetObject(sandboxProxy));
    }

    RootedValue func(cx, js::GetProxyPrivate(proxy));
    return JS::Call(cx, thisVal, func, args, args.rval());
}

const xpc::SandboxCallableProxyHandler xpc::sandboxCallableProxyHandler;

/*
 * Wrap a callable such that if we're called with oldThisObj as the
 * "this" we will instead call it with newThisObj as the this.
 */
static JSObject*
WrapCallable(JSContext* cx, HandleObject callable, HandleObject sandboxProtoProxy)
{
    MOZ_ASSERT(JS::IsCallable(callable));
    // Our proxy is wrapping the callable.  So we need to use the
    // callable as the private.  We put the given sandboxProtoProxy in
    // an extra slot, and our call() hook depends on that.
    MOZ_ASSERT(js::IsProxy(sandboxProtoProxy) &&
               js::GetProxyHandler(sandboxProtoProxy) ==
                 &xpc::sandboxProxyHandler);

    RootedValue priv(cx, ObjectValue(*callable));
    JSObject* obj = js::NewProxyObject(cx, &xpc::sandboxCallableProxyHandler,
                                       priv, nullptr);
    if (obj) {
        js::SetProxyExtra(obj, SandboxCallableProxyHandler::SandboxProxySlot,
                          ObjectValue(*sandboxProtoProxy));
    }

    return obj;
}

template<typename Op>
bool WrapAccessorFunction(JSContext* cx, Op& op, JSPropertyDescriptor* desc,
                          unsigned attrFlag, HandleObject sandboxProtoProxy)
{
    if (!op) {
        return true;
    }

    if (!(desc->attrs & attrFlag)) {
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
        return false;
    }

    RootedObject func(cx, JS_FUNC_TO_DATA_PTR(JSObject*, op));
    func = WrapCallable(cx, func, sandboxProtoProxy);
    if (!func)
        return false;
    op = JS_DATA_TO_FUNC_PTR(Op, func.get());
    return true;
}

bool
xpc::SandboxProxyHandler::getPropertyDescriptor(JSContext* cx,
                                                JS::Handle<JSObject*> proxy,
                                                JS::Handle<jsid> id,
                                                JS::MutableHandle<JSPropertyDescriptor> desc) const
{
    JS::RootedObject obj(cx, wrappedObject(proxy));

    MOZ_ASSERT(js::GetObjectCompartment(obj) == js::GetObjectCompartment(proxy));
    if (!JS_GetPropertyDescriptorById(cx, obj, id, desc))
        return false;

    if (!desc.object())
        return true; // No property, nothing to do

    // Now fix up the getter/setter/value as needed to be bound to desc->obj.
    if (!WrapAccessorFunction(cx, desc.getter(), desc.address(),
                              JSPROP_GETTER, proxy))
        return false;
    if (!WrapAccessorFunction(cx, desc.setter(), desc.address(),
                              JSPROP_SETTER, proxy))
        return false;
    if (desc.value().isObject()) {
        RootedObject val (cx, &desc.value().toObject());
        if (JS::IsCallable(val)) {
            val = WrapCallable(cx, val, proxy);
            if (!val)
                return false;
            desc.value().setObject(*val);
        }
    }

    return true;
}

bool
xpc::SandboxProxyHandler::getOwnPropertyDescriptor(JSContext* cx,
                                                   JS::Handle<JSObject*> proxy,
                                                   JS::Handle<jsid> id,
                                                   JS::MutableHandle<JSPropertyDescriptor> desc)
                                                   const
{
    if (!getPropertyDescriptor(cx, proxy, id, desc))
        return false;

    if (desc.object() != wrappedObject(proxy))
        desc.object().set(nullptr);

    return true;
}

/*
 * Reuse the BaseProxyHandler versions of the derived traps that are implemented
 * in terms of the fundamental traps.
 */

bool
xpc::SandboxProxyHandler::has(JSContext* cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id, bool* bp) const
{
    return BaseProxyHandler::has(cx, proxy, id, bp);
}
bool
xpc::SandboxProxyHandler::hasOwn(JSContext* cx, JS::Handle<JSObject*> proxy,
                                 JS::Handle<jsid> id, bool* bp) const
{
    return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
}

bool
xpc::SandboxProxyHandler::get(JSContext* cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<JSObject*> receiver,
                              JS::Handle<jsid> id,
                              JS::MutableHandle<Value> vp) const
{
    return BaseProxyHandler::get(cx, proxy, receiver, id, vp);
}

bool
xpc::SandboxProxyHandler::set(JSContext* cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id,
                              JS::Handle<Value> v,
                              JS::Handle<Value> receiver,
                              JS::ObjectOpResult& result) const
{
    return BaseProxyHandler::set(cx, proxy, id, v, receiver, result);
}

bool
xpc::SandboxProxyHandler::getOwnEnumerablePropertyKeys(JSContext* cx,
                                                       JS::Handle<JSObject*> proxy,
                                                       AutoIdVector& props) const
{
    return BaseProxyHandler::getOwnEnumerablePropertyKeys(cx, proxy, props);
}

bool
xpc::SandboxProxyHandler::enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                                    JS::MutableHandle<JSObject*> objp) const
{
    return BaseProxyHandler::enumerate(cx, proxy, objp);
}

bool
xpc::GlobalProperties::Parse(JSContext* cx, JS::HandleObject obj)
{
    MOZ_ASSERT(JS_IsArrayObject(cx, obj));

    uint32_t length;
    bool ok = JS_GetArrayLength(cx, obj, &length);
    NS_ENSURE_TRUE(ok, false);
    for (uint32_t i = 0; i < length; i++) {
        RootedValue nameValue(cx);
        ok = JS_GetElement(cx, obj, i, &nameValue);
        NS_ENSURE_TRUE(ok, false);
        if (!nameValue.isString()) {
            JS_ReportError(cx, "Property names must be strings");
            return false;
        }
        JSAutoByteString name(cx, nameValue.toString());
        NS_ENSURE_TRUE(name, false);
        if (!strcmp(name.ptr(), "CSS")) {
            CSS = true;
        } else if (!strcmp(name.ptr(), "indexedDB")) {
            indexedDB = true;
        } else if (!strcmp(name.ptr(), "XMLHttpRequest")) {
            XMLHttpRequest = true;
        } else if (!strcmp(name.ptr(), "TextEncoder")) {
            TextEncoder = true;
        } else if (!strcmp(name.ptr(), "TextDecoder")) {
            TextDecoder = true;
        } else if (!strcmp(name.ptr(), "URL")) {
            URL = true;
        } else if (!strcmp(name.ptr(), "URLSearchParams")) {
            URLSearchParams = true;
        } else if (!strcmp(name.ptr(), "atob")) {
            atob = true;
        } else if (!strcmp(name.ptr(), "btoa")) {
            btoa = true;
        } else if (!strcmp(name.ptr(), "Blob")) {
            Blob = true;
        } else if (!strcmp(name.ptr(), "File")) {
            File = true;
        } else if (!strcmp(name.ptr(), "crypto")) {
            crypto = true;
        } else if (!strcmp(name.ptr(), "rtcIdentityProvider")) {
            rtcIdentityProvider = true;
        } else if (!strcmp(name.ptr(), "fetch")) {
            fetch = true;
        } else {
            JS_ReportError(cx, "Unknown property name: %s", name.ptr());
            return false;
        }
    }
    return true;
}

bool
xpc::GlobalProperties::Define(JSContext* cx, JS::HandleObject obj)
{
    if (CSS && !dom::CSSBinding::GetConstructorObject(cx, obj))
        return false;

    if (indexedDB &&
        !IndexedDatabaseManager::DefineIndexedDB(cx, obj))
        return false;

    if (XMLHttpRequest &&
        !dom::XMLHttpRequestBinding::GetConstructorObject(cx, obj))
        return false;

    if (TextEncoder &&
        !dom::TextEncoderBinding::GetConstructorObject(cx, obj))
        return false;

    if (TextDecoder &&
        !dom::TextDecoderBinding::GetConstructorObject(cx, obj))
        return false;

    if (URL &&
        !dom::URLBinding::GetConstructorObject(cx, obj))
        return false;

    if (URLSearchParams &&
        !dom::URLSearchParamsBinding::GetConstructorObject(cx, obj))
        return false;

    if (atob &&
        !JS_DefineFunction(cx, obj, "atob", Atob, 1, 0))
        return false;

    if (btoa &&
        !JS_DefineFunction(cx, obj, "btoa", Btoa, 1, 0))
        return false;

    if (Blob &&
        !dom::BlobBinding::GetConstructorObject(cx, obj))
        return false;

    if (File &&
        !dom::FileBinding::GetConstructorObject(cx, obj))
        return false;

    if (crypto && !SandboxCreateCrypto(cx, obj))
        return false;

    if (rtcIdentityProvider && !SandboxCreateRTCIdentityProvider(cx, obj))
        return false;

    if (fetch && !SandboxCreateFetch(cx, obj))
        return false;

    return true;
}

nsresult
xpc::CreateSandboxObject(JSContext* cx, MutableHandleValue vp, nsISupports* prinOrSop,
                         SandboxOptions& options)
{
    // Create the sandbox global object
    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(prinOrSop);
    if (!principal) {
        nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(prinOrSop);
        if (sop) {
            principal = sop->GetPrincipal();
        } else {
            nsRefPtr<nsNullPrincipal> nullPrin = nsNullPrincipal::Create();
            NS_ENSURE_TRUE(nullPrin, NS_ERROR_FAILURE);
            principal = nullPrin;
        }
    }
    MOZ_ASSERT(principal);

    JS::CompartmentOptions compartmentOptions;
    if (options.sameZoneAs)
        compartmentOptions.setSameZoneAs(js::UncheckedUnwrap(options.sameZoneAs));
    else if (options.freshZone)
        compartmentOptions.setZone(JS::FreshZone);
    else
        compartmentOptions.setZone(JS::SystemZone);

    compartmentOptions.setInvisibleToDebugger(options.invisibleToDebugger)
                      .setDiscardSource(options.discardSource)
                      .setTrace(TraceXPCGlobal);

    // Try to figure out any addon this sandbox should be associated with.
    // The addon could have been passed in directly, as part of the metadata,
    // or by being constructed from an addon's code.
    JSAddonId* addonId = nullptr;
    if (options.addonId) {
        addonId = JS::NewAddonId(cx, options.addonId);
        NS_ENSURE_TRUE(addonId, NS_ERROR_FAILURE);
    } else if (JSObject* obj = JS::CurrentGlobalOrNull(cx)) {
        if (JSAddonId* id = JS::AddonIdOfObject(obj))
            addonId = id;
    }

    compartmentOptions.setAddonId(addonId);

    const Class* clasp = options.writeToGlobalPrototype
                       ? &SandboxWriteToProtoClass
                       : &SandboxClass;

    RootedObject sandbox(cx, xpc::CreateGlobalObject(cx, js::Jsvalify(clasp),
                                                     principal, compartmentOptions));
    if (!sandbox)
        return NS_ERROR_FAILURE;

    CompartmentPrivate::Get(sandbox)->writeToGlobalPrototype =
      options.writeToGlobalPrototype;

    // Set up the wantXrays flag, which indicates whether xrays are desired even
    // for same-origin access.
    //
    // This flag has historically been ignored for chrome sandboxes due to
    // quirks in the wrapping implementation that have now been removed. Indeed,
    // same-origin Xrays for chrome->chrome access seems a bit superfluous.
    // Arguably we should just flip the default for chrome and still honor the
    // flag, but such a change would break code in subtle ways for minimal
    // benefit. So we just switch it off here.
    CompartmentPrivate::Get(sandbox)->wantXrays =
      AccessCheck::isChrome(sandbox) ? false : options.wantXrays;

    {
        JSAutoCompartment ac(cx, sandbox);

        if (options.proto) {
            bool ok = JS_WrapObject(cx, &options.proto);
            if (!ok)
                return NS_ERROR_XPC_UNEXPECTED;

            // Now check what sort of thing we've got in |proto|
            JSObject* unwrappedProto = js::CheckedUnwrap(options.proto, false);
            if (!unwrappedProto) {
                JS_ReportError(cx, "Sandbox must subsume sandboxPrototype");
                return NS_ERROR_INVALID_ARG;
            }
            const js::Class* unwrappedClass = js::GetObjectClass(unwrappedProto);
            if (IS_WN_CLASS(unwrappedClass) ||
                mozilla::dom::IsDOMClass(Jsvalify(unwrappedClass))) {
                // Wrap it up in a proxy that will do the right thing in terms
                // of this-binding for methods.
                RootedValue priv(cx, ObjectValue(*options.proto));
                options.proto = js::NewProxyObject(cx, &xpc::sandboxProxyHandler,
                                                   priv, nullptr);
                if (!options.proto)
                    return NS_ERROR_OUT_OF_MEMORY;
            }

            ok = JS_SetPrototype(cx, sandbox, options.proto);
            if (!ok)
                return NS_ERROR_XPC_UNEXPECTED;
        }

        nsCOMPtr<nsIScriptObjectPrincipal> sbp =
            new SandboxPrivate(principal, sandbox);

        // Pass on ownership of sbp to |sandbox|.
        JS_SetPrivate(sandbox, sbp.forget().take());

        // Don't try to mirror the properties that are set below.
        AutoSkipPropertyMirroring askip(CompartmentPrivate::Get(sandbox));

        bool allowComponents = principal == nsXPConnect::SystemPrincipal() ||
                               nsContentUtils::IsExpandedPrincipal(principal);
        if (options.wantComponents && allowComponents &&
            !ObjectScope(sandbox)->AttachComponentsObject(cx))
            return NS_ERROR_XPC_UNEXPECTED;

        if (!XPCNativeWrapper::AttachNewConstructorObject(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;

        if (!JS_DefineFunctions(cx, sandbox, SandboxFunctions))
            return NS_ERROR_XPC_UNEXPECTED;

        if (options.wantExportHelpers &&
            (!JS_DefineFunction(cx, sandbox, "exportFunction", SandboxExportFunction, 3, 0) ||
             !JS_DefineFunction(cx, sandbox, "createObjectIn", SandboxCreateObjectIn, 2, 0) ||
             !JS_DefineFunction(cx, sandbox, "cloneInto", SandboxCloneInto, 3, 0) ||
             !JS_DefineFunction(cx, sandbox, "isProxy", SandboxIsProxy, 1, 0)))
            return NS_ERROR_XPC_UNEXPECTED;

        if (!options.globalProperties.Define(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;

        // Promise is supposed to be part of ES, and therefore should appear on
        // every global.
        if (!dom::PromiseBinding::GetConstructorObject(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;

        // Resolve standard classes eagerly to avoid triggering mirroring hooks for them.
        if (options.writeToGlobalPrototype && !JS_EnumerateStandardClasses(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;
    }

    // We handle the case where the context isn't in a compartment for the
    // benefit of InitSingletonScopes.
    vp.setObject(*sandbox);
    if (js::GetContextCompartment(cx) && !JS_WrapValue(cx, vp))
        return NS_ERROR_UNEXPECTED;

    // Set the location information for the new global, so that tools like
    // about:memory may use that information
    xpc::SetLocationForGlobal(sandbox, options.sandboxName);

    xpc::SetSandboxMetadata(cx, sandbox, options.metadata);

    JS_FireOnNewGlobalObject(cx, sandbox);

    return NS_OK;
}

/* bool call(in nsIXPConnectWrappedNative wrapper,
 *           in JSContextPtr cx,
 *           in JSObjectPtr obj,
 *           in uint32_t argc,
 *           in JSValPtr argv,
 *           in JSValPtr vp);
 */
NS_IMETHODIMP
nsXPCComponents_utils_Sandbox::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                                    JSObject* objArg, const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

/* bool construct(in nsIXPConnectWrappedNative wrapper,
 *                in JSContextPtr cx,
 *                in JSObjectPtr obj,
 *                in uint32_t argc,
 *                in JSValPtr argv,
 *                in JSValPtr vp);
 */
NS_IMETHODIMP
nsXPCComponents_utils_Sandbox::Construct(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                                         JSObject* objArg, const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

/*
 * For sandbox constructor the first argument can be a URI string in which case
 * we use the related Codebase Principal for the sandbox.
 */
bool
ParsePrincipal(JSContext* cx, HandleString codebase, nsIPrincipal** principal)
{
    MOZ_ASSERT(principal);
    MOZ_ASSERT(codebase);
    nsCOMPtr<nsIURI> uri;
    nsAutoJSString codebaseStr;
    NS_ENSURE_TRUE(codebaseStr.init(cx, codebase), false);
    nsresult rv = NS_NewURI(getter_AddRefs(uri), codebaseStr);
    if (NS_FAILED(rv)) {
        JS_ReportError(cx, "Creating URI from string failed");
        return false;
    }

    nsCOMPtr<nsIScriptSecurityManager> secman =
        do_GetService(kScriptSecurityManagerContractID);
    NS_ENSURE_TRUE(secman, false);

    // We could allow passing in the app-id and browser-element info to the
    // sandbox constructor. But creating a sandbox based on a string is a
    // deprecated API so no need to add features to it.
    rv = secman->GetNoAppCodebasePrincipal(uri, principal);
    if (NS_FAILED(rv) || !*principal) {
        JS_ReportError(cx, "Creating Principal from URI failed");
        return false;
    }
    return true;
}

/*
 * For sandbox constructor the first argument can be a principal object or
 * a script object principal (Document, Window).
 */
static bool
GetPrincipalOrSOP(JSContext* cx, HandleObject from, nsISupports** out)
{
    MOZ_ASSERT(out);
    *out = nullptr;

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    nsISupports* native = xpc->GetNativeOfWrapper(cx, from);

    if (nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(native)) {
        sop.forget(out);
        return true;
    }

    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(native);
    principal.forget(out);
    NS_ENSURE_TRUE(*out, false);

    return true;
}

/*
 * The first parameter of the sandbox constructor might be an array of principals, either in string
 * format or actual objects (see GetPrincipalOrSOP)
 */
static bool
GetExpandedPrincipal(JSContext* cx, HandleObject arrayObj, nsIExpandedPrincipal** out)
{
    MOZ_ASSERT(out);
    uint32_t length;

    if (!JS_IsArrayObject(cx, arrayObj) ||
        !JS_GetArrayLength(cx, arrayObj, &length) ||
        !length)
    {
        // We need a whitelist of principals or uri strings to create an
        // expanded principal, if we got an empty array or something else
        // report error.
        JS_ReportError(cx, "Expected an array of URI strings");
        return false;
    }

    nsTArray< nsCOMPtr<nsIPrincipal> > allowedDomains(length);
    allowedDomains.SetLength(length);

    for (uint32_t i = 0; i < length; ++i) {
        RootedValue allowed(cx);
        if (!JS_GetElement(cx, arrayObj, i, &allowed))
            return false;

        nsresult rv;
        nsCOMPtr<nsIPrincipal> principal;
        if (allowed.isString()) {
            // In case of string let's try to fetch a codebase principal from it.
            RootedString str(cx, allowed.toString());
            if (!ParsePrincipal(cx, str, getter_AddRefs(principal)))
                return false;

        } else if (allowed.isObject()) {
            // In case of object let's see if it's a Principal or a ScriptObjectPrincipal.
            nsCOMPtr<nsISupports> prinOrSop;
            RootedObject obj(cx, &allowed.toObject());
            if (!GetPrincipalOrSOP(cx, obj, getter_AddRefs(prinOrSop)))
                return false;

            nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(prinOrSop));
            principal = do_QueryInterface(prinOrSop);
            if (sop)
                principal = sop->GetPrincipal();
        }
        NS_ENSURE_TRUE(principal, false);

        // We do not allow ExpandedPrincipals to contain any system principals.
        bool isSystem;
        rv = nsXPConnect::SecurityManager()->IsSystemPrincipal(principal, &isSystem);
        NS_ENSURE_SUCCESS(rv, false);
        if (isSystem) {
            JS_ReportError(cx, "System principal is not allowed in an expanded principal");
            return false;
        }
        allowedDomains[i] = principal;
  }

  nsCOMPtr<nsIExpandedPrincipal> result = new nsExpandedPrincipal(allowedDomains);
  result.forget(out);
  return true;
}

/*
 * Helper that tries to get a property from the options object.
 */
bool
OptionsBase::ParseValue(const char* name, MutableHandleValue prop, bool* aFound)
{
    bool found;
    bool ok = JS_HasProperty(mCx, mObject, name, &found);
    NS_ENSURE_TRUE(ok, false);

    if (aFound)
        *aFound = found;

    if (!found)
        return true;

    return JS_GetProperty(mCx, mObject, name, prop);
}

/*
 * Helper that tries to get a boolean property from the options object.
 */
bool
OptionsBase::ParseBoolean(const char* name, bool* prop)
{
    MOZ_ASSERT(prop);
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue(name, &value, &found);
    NS_ENSURE_TRUE(ok, false);

    if (!found)
        return true;

    if (!value.isBoolean()) {
        JS_ReportError(mCx, "Expected a boolean value for property %s", name);
        return false;
    }

    *prop = value.toBoolean();
    return true;
}

/*
 * Helper that tries to get an object property from the options object.
 */
bool
OptionsBase::ParseObject(const char* name, MutableHandleObject prop)
{
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue(name, &value, &found);
    NS_ENSURE_TRUE(ok, false);

    if (!found)
        return true;

    if (!value.isObject()) {
        JS_ReportError(mCx, "Expected an object value for property %s", name);
        return false;
    }
    prop.set(&value.toObject());
    return true;
}

/*
 * Helper that tries to get an object property from the options object.
 */
bool
OptionsBase::ParseJSString(const char* name, MutableHandleString prop)
{
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue(name, &value, &found);
    NS_ENSURE_TRUE(ok, false);

    if (!found)
        return true;

    if (!value.isString()) {
        JS_ReportError(mCx, "Expected a string value for property %s", name);
        return false;
    }
    prop.set(value.toString());
    return true;
}

/*
 * Helper that tries to get a string property from the options object.
 */
bool
OptionsBase::ParseString(const char* name, nsCString& prop)
{
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue(name, &value, &found);
    NS_ENSURE_TRUE(ok, false);

    if (!found)
        return true;

    if (!value.isString()) {
        JS_ReportError(mCx, "Expected a string value for property %s", name);
        return false;
    }

    char* tmp = JS_EncodeString(mCx, value.toString());
    NS_ENSURE_TRUE(tmp, false);
    prop.Assign(tmp, strlen(tmp));
    js_free(tmp);
    return true;
}

/*
 * Helper that tries to get a string property from the options object.
 */
bool
OptionsBase::ParseString(const char* name, nsString& prop)
{
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue(name, &value, &found);
    NS_ENSURE_TRUE(ok, false);

    if (!found)
        return true;

    if (!value.isString()) {
        JS_ReportError(mCx, "Expected a string value for property %s", name);
        return false;
    }

    nsAutoJSString strVal;
    if (!strVal.init(mCx, value.toString()))
        return false;

    prop = strVal;
    return true;
}

/*
 * Helper that tries to get jsid property from the options object.
 */
bool
OptionsBase::ParseId(const char* name, MutableHandleId prop)
{
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue(name, &value, &found);
    NS_ENSURE_TRUE(ok, false);

    if (!found)
        return true;

    return JS_ValueToId(mCx, value, prop);
}

/*
 * Helper that tries to get a list of DOM constructors and other helpers from the options object.
 */
bool
SandboxOptions::ParseGlobalProperties()
{
    RootedValue value(mCx);
    bool found;
    bool ok = ParseValue("wantGlobalProperties", &value, &found);
    NS_ENSURE_TRUE(ok, false);
    if (!found)
        return true;

    if (!value.isObject()) {
        JS_ReportError(mCx, "Expected an array value for wantGlobalProperties");
        return false;
    }

    RootedObject ctors(mCx, &value.toObject());
    if (!JS_IsArrayObject(mCx, ctors)) {
        JS_ReportError(mCx, "Expected an array value for wantGlobalProperties");
        return false;
    }

    return globalProperties.Parse(mCx, ctors);
}

/*
 * Helper that parsing the sandbox options object (from) and sets the fields of the incoming options struct (options).
 */
bool
SandboxOptions::Parse()
{
    bool ok = ParseObject("sandboxPrototype", &proto) &&
              ParseBoolean("wantXrays", &wantXrays) &&
              ParseBoolean("wantComponents", &wantComponents) &&
              ParseBoolean("wantExportHelpers", &wantExportHelpers) &&
              ParseString("sandboxName", sandboxName) &&
              ParseObject("sameZoneAs", &sameZoneAs) &&
              ParseBoolean("freshZone", &freshZone) &&
              ParseBoolean("invisibleToDebugger", &invisibleToDebugger) &&
              ParseBoolean("discardSource", &discardSource) &&
              ParseJSString("addonId", &addonId) &&
              ParseBoolean("writeToGlobalPrototype", &writeToGlobalPrototype) &&
              ParseGlobalProperties() &&
              ParseValue("metadata", &metadata);
    if (!ok)
        return false;

    if (freshZone && sameZoneAs) {
        JS_ReportError(mCx, "Cannot use both sameZoneAs and freshZone");
        return false;
    }

    return true;
}

static nsresult
AssembleSandboxMemoryReporterName(JSContext* cx, nsCString& sandboxName)
{
    // Use a default name when the caller did not provide a sandboxName.
    if (sandboxName.IsEmpty())
        sandboxName = NS_LITERAL_CSTRING("[anonymous sandbox]");

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    // Get the xpconnect native call context.
    nsAXPCNativeCallContext* cc = nullptr;
    xpc->GetCurrentNativeCallContext(&cc);
    NS_ENSURE_TRUE(cc, NS_ERROR_INVALID_ARG);

    // Get the current source info from xpc.
    nsCOMPtr<nsIStackFrame> frame;
    xpc->GetCurrentJSStack(getter_AddRefs(frame));

    // Append the caller's location information.
    if (frame) {
        nsString location;
        int32_t lineNumber = 0;
        frame->GetFilename(location);
        frame->GetLineNumber(&lineNumber);

        sandboxName.AppendLiteral(" (from: ");
        sandboxName.Append(NS_ConvertUTF16toUTF8(location));
        sandboxName.Append(':');
        sandboxName.AppendInt(lineNumber);
        sandboxName.Append(')');
    }

    return NS_OK;
}

// static
nsresult
nsXPCComponents_utils_Sandbox::CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                               JSContext* cx, HandleObject obj,
                                               const CallArgs& args, bool* _retval)
{
    if (args.length() < 1)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);

    nsresult rv;
    bool ok = false;

    // Make sure to set up principals on the sandbox before initing classes.
    nsCOMPtr<nsIPrincipal> principal;
    nsCOMPtr<nsIExpandedPrincipal> expanded;
    nsCOMPtr<nsISupports> prinOrSop;

    if (args[0].isString()) {
        RootedString str(cx, args[0].toString());
        ok = ParsePrincipal(cx, str, getter_AddRefs(principal));
        prinOrSop = principal;
    } else if (args[0].isObject()) {
        RootedObject obj(cx, &args[0].toObject());
        if (JS_IsArrayObject(cx, obj)) {
            ok = GetExpandedPrincipal(cx, obj, getter_AddRefs(expanded));
            prinOrSop = expanded;
        } else {
            ok = GetPrincipalOrSOP(cx, obj, getter_AddRefs(prinOrSop));
        }
    } else if (args[0].isNull()) {
        // Null means that we just pass prinOrSop = nullptr, and get an
        // nsNullPrincipal.
        ok = true;
    }

    if (!ok)
        return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);

    bool calledWithOptions = args.length() > 1;
    if (calledWithOptions && !args[1].isObject())
        return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);

    RootedObject optionsObject(cx, calledWithOptions ? &args[1].toObject()
                                                     : nullptr);

    SandboxOptions options(cx, optionsObject);
    if (calledWithOptions && !options.Parse())
        return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);

    if (NS_FAILED(AssembleSandboxMemoryReporterName(cx, options.sandboxName)))
        return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);

    if (options.metadata.isNullOrUndefined()) {
        // If the caller is running in a sandbox, inherit.
        RootedObject callerGlobal(cx, CurrentGlobalOrNull(cx));
        if (IsSandbox(callerGlobal)) {
            rv = GetSandboxMetadata(cx, callerGlobal, &options.metadata);
            if (NS_WARN_IF(NS_FAILED(rv)))
                return rv;
        }
    }

    rv = CreateSandboxObject(cx, args.rval(), prinOrSop, options);

    if (NS_FAILED(rv))
        return ThrowAndFail(rv, cx, _retval);

    // We have this crazy behavior where wantXrays=false also implies that the
    // returned sandbox is implicitly waived. We've stopped advertising it, but
    // keep supporting it for now.
    if (!options.wantXrays && !xpc::WrapperFactory::WaiveXrayAndWrap(cx, args.rval()))
        return NS_ERROR_UNEXPECTED;

    *_retval = true;
    return NS_OK;
}

nsresult
xpc::EvalInSandbox(JSContext* cx, HandleObject sandboxArg, const nsAString& source,
                   const nsACString& filename, int32_t lineNo,
                   JSVersion jsVersion, MutableHandleValue rval)
{
    JS_AbortIfWrongThread(JS_GetRuntime(cx));
    rval.set(UndefinedValue());

    bool waiveXray = xpc::WrapperFactory::HasWaiveXrayFlag(sandboxArg);
    RootedObject sandbox(cx, js::CheckedUnwrap(sandboxArg));
    if (!sandbox || !IsSandbox(sandbox)) {
        return NS_ERROR_INVALID_ARG;
    }

    nsIScriptObjectPrincipal* sop =
        static_cast<nsIScriptObjectPrincipal*>(xpc_GetJSPrivate(sandbox));
    MOZ_ASSERT(sop, "Invalid sandbox passed");
    SandboxPrivate* priv = static_cast<SandboxPrivate*>(sop);
    nsCOMPtr<nsIPrincipal> prin = sop->GetPrincipal();
    NS_ENSURE_TRUE(prin, NS_ERROR_FAILURE);

    nsAutoCString filenameBuf;
    if (!filename.IsVoid() && filename.Length() != 0) {
        filenameBuf.Assign(filename);
    } else {
        // Default to the spec of the principal.
        nsJSPrincipals::get(prin)->GetScriptLocation(filenameBuf);
        lineNo = 1;
    }

    // We create a separate cx to do the sandbox evaluation. Scope it.
    RootedValue v(cx, UndefinedValue());
    RootedValue exn(cx, UndefinedValue());
    bool ok = true;
    {
        // We're about to evaluate script, so make an AutoEntryScript.
        // This is clearly Gecko-specific and not in any spec.
        mozilla::dom::AutoEntryScript aes(priv, "XPConnect sandbox evaluation");
        JSContext* sandcx = aes.cx();
        AutoSaveContextOptions savedOptions(sandcx);
        JS::ContextOptionsRef(sandcx).setDontReportUncaught(true);
        JSAutoCompartment ac(sandcx, sandbox);

        JS::CompileOptions options(sandcx);
        options.setFileAndLine(filenameBuf.get(), lineNo)
               .setVersion(jsVersion);
        MOZ_ASSERT(JS_IsGlobalObject(sandbox));
        ok = JS::Evaluate(sandcx, options,
                          PromiseFlatString(source).get(), source.Length(), &v);

        // If the sandbox threw an exception, grab it off the context.
        if (JS_GetPendingException(sandcx, &exn)) {
            MOZ_ASSERT(!ok);
            JS_ClearPendingException(sandcx);
        }
    }

    //
    // Alright, we're back on the caller's cx. If an error occured, try to
    // wrap and set the exception. Otherwise, wrap the return value.
    //

    if (!ok) {
        // If we end up without an exception, it was probably due to OOM along
        // the way, in which case we thow. Otherwise, wrap it.
        if (exn.isUndefined() || !JS_WrapValue(cx, &exn))
            return NS_ERROR_OUT_OF_MEMORY;

        // Set the exception on our caller's cx.
        JS_SetPendingException(cx, exn);
        return NS_ERROR_FAILURE;
    }

    // Transitively apply Xray waivers if |sb| was waived.
    if (waiveXray) {
        ok = xpc::WrapperFactory::WaiveXrayAndWrap(cx, &v);
    } else {
        ok = JS_WrapValue(cx, &v);
    }
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

    // Whew!
    rval.set(v);
    return NS_OK;
}

nsresult
xpc::GetSandboxAddonId(JSContext* cx, HandleObject sandbox, MutableHandleValue rval)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsSandbox(sandbox));

    JSAddonId* id = JS::AddonIdOfObject(sandbox);
    if (!id) {
        rval.setNull();
        return NS_OK;
    }

    JS::RootedValue idStr(cx, StringValue(JS::StringOfAddonId(id)));
    if (!JS_WrapValue(cx, &idStr))
        return NS_ERROR_UNEXPECTED;

    rval.set(idStr);
    return NS_OK;
}

nsresult
xpc::GetSandboxMetadata(JSContext* cx, HandleObject sandbox, MutableHandleValue rval)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsSandbox(sandbox));

    RootedValue metadata(cx);
    {
      JSAutoCompartment ac(cx, sandbox);
      metadata = JS_GetReservedSlot(sandbox, XPCONNECT_SANDBOX_CLASS_METADATA_SLOT);
    }

    if (!JS_WrapValue(cx, &metadata))
        return NS_ERROR_UNEXPECTED;

    rval.set(metadata);
    return NS_OK;
}

nsresult
xpc::SetSandboxMetadata(JSContext* cx, HandleObject sandbox, HandleValue metadataArg)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsSandbox(sandbox));

    RootedValue metadata(cx);

    JSAutoCompartment ac(cx, sandbox);
    if (!JS_StructuredClone(cx, metadataArg, &metadata, nullptr, nullptr))
        return NS_ERROR_UNEXPECTED;

    JS_SetReservedSlot(sandbox, XPCONNECT_SANDBOX_CLASS_METADATA_SLOT, metadata);

    return NS_OK;
}
