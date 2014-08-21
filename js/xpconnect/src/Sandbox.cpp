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
#include "jsproxy.h"
#include "js/OldDebugAPI.h"
#include "js/StructuredClone.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsGlobalWindow.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsPrincipal.h"
#include "nsXMLHttpRequest.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "XPCWrapper.h"
#include "XrayWrapper.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/CSSBinding.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/URLBinding.h"

using namespace mozilla;
using namespace JS;
using namespace js;
using namespace xpc;

using mozilla::dom::DestroyProtoAndIfaceCache;
using mozilla::dom::indexedDB::IndexedDatabaseManager;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(SandboxPrivate)
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
    virtual ~nsXPCComponents_utils_Sandbox();

private:
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                    JSContext *cx, HandleObject obj,
                                    const CallArgs &args, bool *_retval);
};

already_AddRefed<nsIXPCComponents_utils_Sandbox>
xpc::NewSandboxConstructor()
{
    nsCOMPtr<nsIXPCComponents_utils_Sandbox> sbConstructor =
        new nsXPCComponents_utils_Sandbox();
    return sbConstructor.forget();
}

static bool
SandboxDump(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0)
        return true;

    RootedString str(cx, ToString(cx, args[0]));
    if (!str)
        return false;

    size_t length;
    const jschar *chars = JS_GetStringCharsZAndLength(cx, str, &length);
    if (!chars)
        return false;

    nsDependentString wstr(chars, length);
    char *cstr = ToNewUTF8String(wstr);
    if (!cstr)
        return false;

#if defined(XP_MACOSX)
    // Be nice and convert all \r to \n.
    char *c = cstr, *cEnd = cstr + strlen(cstr);
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
    NS_Free(cstr);
    args.rval().setBoolean(true);
    return true;
}

static bool
SandboxDebug(JSContext *cx, unsigned argc, jsval *vp)
{
#ifdef DEBUG
    return SandboxDump(cx, argc, vp);
#else
    return true;
#endif
}

static bool
SandboxImport(JSContext *cx, unsigned argc, Value *vp)
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
        JSFunction *fun = JS_ValueToFunction(cx, funval);
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
CreateXMLHttpRequest(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    MOZ_ASSERT(global);

    nsIScriptObjectPrincipal *sop =
        static_cast<nsIScriptObjectPrincipal *>(xpc_GetJSPrivate(global));
    nsCOMPtr<nsIGlobalObject> iglobal = do_QueryInterface(sop);

    nsCOMPtr<nsIXMLHttpRequest> xhr = new nsXMLHttpRequest();
    nsresult rv = xhr->Init(nsContentUtils::SubjectPrincipal(), nullptr,
                            iglobal, nullptr);
    if (NS_FAILED(rv))
        return false;

    rv = nsContentUtils::WrapNative(cx, xhr, args.rval());
    if (NS_FAILED(rv))
        return false;

    return true;
}

static bool
IsProxy(JSContext *cx, unsigned argc, jsval *vp)
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

namespace xpc {

bool
ExportFunction(JSContext *cx, HandleValue vfunction, HandleValue vscope, HandleValue voptions,
               MutableHandleValue rval)
{
    bool hasOptions = !voptions.isUndefined();
    if (!vscope.isObject() || !vfunction.isObject() || (hasOptions && !voptions.isObject())) {
        JS_ReportError(cx, "Invalid argument");
        return false;
    }

    RootedObject funObj(cx, &vfunction.toObject());
    RootedObject targetScope(cx, &vscope.toObject());
    ExportOptions options(cx, hasOptions ? &voptions.toObject() : nullptr);
    if (hasOptions && !options.Parse())
        return false;

    // We can only export functions to scopes those are transparent for us,
    // so if there is a security wrapper around targetScope we must throw.
    targetScope = CheckedUnwrap(targetScope);
    if (!targetScope) {
        JS_ReportError(cx, "Permission denied to export function into scope");
        return false;
    }

    if (js::IsScriptedProxy(targetScope)) {
        JS_ReportError(cx, "Defining property on proxy object is not allowed");
        return false;
    }

    {
        // We need to operate in the target scope from here on, let's enter
        // its compartment.
        JSAutoCompartment ac(cx, targetScope);

        // Unwrapping to see if we have a callable.
        funObj = UncheckedUnwrap(funObj);
        if (!JS_ObjectIsCallable(cx, funObj)) {
            JS_ReportError(cx, "First argument must be a function");
            return false;
        }

        RootedId id(cx, options.defineAs);
        if (JSID_IS_VOID(id)) {
            // If there wasn't any function name specified,
            // copy the name from the function being imported.
            JSFunction *fun = JS_GetObjectFunction(funObj);
            RootedString funName(cx, JS_GetFunctionId(fun));
            if (!funName)
                funName = JS_InternString(cx, "");

            if (!JS_StringToId(cx, funName, &id))
                return false;
        }
        MOZ_ASSERT(JSID_IS_STRING(id));

        // The function forwarder will live in the target compartment. Since
        // this function will be referenced from its private slot, to avoid a
        // GC hazard, we must wrap it to the same compartment.
        if (!JS_WrapObject(cx, &funObj))
            return false;

        // And now, let's create the forwarder function in the target compartment
        // for the function the be exported.
        if (!NewFunctionForwarder(cx, id, funObj, /* doclone = */ true, rval)) {
            JS_ReportError(cx, "Exporting function failed");
            return false;
        }

        // We have the forwarder function in the target compartment. If
        // defineAs was set, we also need to define it as a property on
        // the target.
        if (!JSID_IS_VOID(options.defineAs)) {
            if (!JS_DefinePropertyById(cx, targetScope, id, rval, JSPROP_ENUMERATE,
                                       JS_PropertyStub, JS_StrictPropertyStub)) {
                return false;
            }
        }
    }

    // Finally we have to re-wrap the exported function back to the caller compartment.
    if (!JS_WrapValue(cx, rval))
        return false;

    return true;
}

/*
 * Expected type of the arguments and the return value:
 * function exportFunction(function funToExport,
 *                         object targetScope,
 *                         [optional] object options)
 */
static bool
ExportFunction(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportError(cx, "Function requires at least 2 arguments");
        return false;
    }

    RootedValue options(cx, args.length() > 2 ? args[2] : UndefinedValue());
    return ExportFunction(cx, args[0], args[1], options, args.rval());
}
} /* namespace xpc */

static bool
GetFilenameAndLineNumber(JSContext *cx, nsACString &filename, unsigned &lineno)
{
    JS::AutoFilename scriptFilename;
    if (JS::DescribeScriptedCaller(cx, &scriptFilename, &lineno)) {
        if (const char *cfilename = scriptFilename.get()) {
            filename.Assign(nsDependentCString(cfilename));
            return true;
        }
    }
    return false;
}

bool
xpc::IsReflector(JSObject *obj)
{
    return IS_WN_REFLECTOR(obj) || dom::IsDOMObject(obj);
}

enum ForwarderCloneTags {
    SCTAG_BASE = JS_SCTAG_USER_MIN,
    SCTAG_REFLECTOR
};

static JSObject *
CloneNonReflectorsRead(JSContext *cx, JSStructuredCloneReader *reader, uint32_t tag,
                       uint32_t data, void *closure)
{
    MOZ_ASSERT(closure, "Null pointer!");
    AutoObjectVector *reflectors = static_cast<AutoObjectVector *>(closure);
    if (tag == SCTAG_REFLECTOR) {
        MOZ_ASSERT(!data);

        size_t idx;
        if (JS_ReadBytes(reader, &idx, sizeof(size_t))) {
            RootedObject reflector(cx, (*reflectors)[idx]);
            MOZ_ASSERT(reflector, "No object pointer?");
            MOZ_ASSERT(IsReflector(reflector), "Object pointer must be a reflector!");

            if (!JS_WrapObject(cx, &reflector))
                return nullptr;
            MOZ_ASSERT(WrapperFactory::IsXrayWrapper(reflector) ||
                       IsReflector(reflector));

            return reflector;
        }
    }

    JS_ReportError(cx, "CloneNonReflectorsRead error");
    return nullptr;
}

static bool
CloneNonReflectorsWrite(JSContext *cx, JSStructuredCloneWriter *writer,
                        Handle<JSObject *> obj, void *closure)
{
    MOZ_ASSERT(closure, "Null pointer!");

    // We need to maintain a list of reflectors to make sure all these objects
    // are properly rooter. Only their indices will be serialized.
    AutoObjectVector *reflectors = static_cast<AutoObjectVector *>(closure);
    if (IsReflector(obj)) {
        if (!reflectors->append(obj))
            return false;

        size_t idx = reflectors->length()-1;
        if (JS_WriteUint32Pair(writer, SCTAG_REFLECTOR, 0) &&
            JS_WriteBytes(writer, &idx, sizeof(size_t))) {
            return true;
        }
    }

    JS_ReportError(cx, "CloneNonReflectorsWrite error");
    return false;
}

static const JSStructuredCloneCallbacks gForwarderStructuredCloneCallbacks = {
    CloneNonReflectorsRead,
    CloneNonReflectorsWrite,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

/*
 * This is a special structured cloning, that clones only non-reflectors.
 * The function assumes the cx is already entered the compartment we want
 * to clone to, and that if val is an object is from the compartment we
 * clone from.
 */
static bool
CloneNonReflectors(JSContext *cx, MutableHandleValue val)
{
    JSAutoStructuredCloneBuffer buffer;
    AutoObjectVector rootedReflectors(cx);
    {
        // For parsing val we have to enter its compartment.
        // (unless it's a primitive)
        Maybe<JSAutoCompartment> ac;
        if (val.isObject()) {
            ac.construct(cx, &val.toObject());
        } else if (val.isString() && !JS_WrapValue(cx, val)) {
            return false;
        }

        if (!buffer.write(cx, val,
            &gForwarderStructuredCloneCallbacks,
            &rootedReflectors))
        {
            return false;
        }
    }

    // Now recreate the clones in the target compartment.
    if (!buffer.read(cx, val,
        &gForwarderStructuredCloneCallbacks,
        &rootedReflectors))
    {
        return false;
    }

    return true;
}

namespace xpc {

bool
EvalInWindow(JSContext *cx, const nsAString &source, HandleObject scope, MutableHandleValue rval)
{
    // If we cannot unwrap we must not eval in it.
    RootedObject targetScope(cx, CheckedUnwrap(scope));
    if (!targetScope) {
        JS_ReportError(cx, "Permission denied to eval in target scope");
        return false;
    }

    // Make sure that we have a window object.
    RootedObject inner(cx, CheckedUnwrap(targetScope, /* stopAtOuter = */ false));
    nsCOMPtr<nsIGlobalObject> global;
    nsCOMPtr<nsPIDOMWindow> window;
    if (!JS_IsGlobalObject(inner) ||
        !(global = GetNativeForGlobal(inner)) ||
        !(window = do_QueryInterface(global)))
    {
        JS_ReportError(cx, "Second argument must be a window");
        return false;
    }

    nsCOMPtr<nsIScriptContext> context =
        (static_cast<nsGlobalWindow*>(window.get()))->GetScriptContext();
    if (!context) {
        JS_ReportError(cx, "Script context needed");
        return false;
    }

    nsCString filename;
    unsigned lineNo;
    if (!GetFilenameAndLineNumber(cx, filename, lineNo)) {
        // Default values for non-scripted callers.
        filename.AssignLiteral("Unknown");
        lineNo = 0;
    }

    RootedObject cxGlobal(cx, JS::CurrentGlobalOrNull(cx));
    {
        // CompileOptions must be created from the context
        // we will execute this script in.
        JSContext *wndCx = context->GetNativeContext();
        AutoCxPusher pusher(wndCx);
        JS::CompileOptions compileOptions(wndCx);
        compileOptions.setFileAndLine(filename.get(), lineNo);

        // We don't want the JS engine to automatically report
        // uncaught exceptions.
        nsJSUtils::EvaluateOptions evaluateOptions;
        evaluateOptions.setReportUncaught(false);

        nsresult rv = nsJSUtils::EvaluateString(wndCx,
                                                source,
                                                targetScope,
                                                compileOptions,
                                                evaluateOptions,
                                                rval);

        if (NS_FAILED(rv)) {
            // If there was an exception we get it as a return value, if
            // the evaluation failed for some other reason, then a default
            // exception is raised.
            MOZ_ASSERT(!JS_IsExceptionPending(wndCx),
                       "Exception should be delivered as return value.");
            if (rval.isUndefined()) {
                MOZ_ASSERT(rv == NS_ERROR_OUT_OF_MEMORY);
                return false;
            }

            // If there was an exception thrown we should set it
            // on the calling context.
            RootedValue exn(wndCx, rval);
            // First we should reset the return value.
            rval.set(UndefinedValue());

            // Then clone the exception.
            JSAutoCompartment ac(wndCx, cxGlobal);
            if (CloneNonReflectors(wndCx, &exn))
                js::SetPendingExceptionCrossContext(cx, exn);

            return false;
        }
    }

    // Let's clone the return value back to the callers compartment.
    if (!CloneNonReflectors(cx, rval)) {
        rval.set(UndefinedValue());
        return false;
    }

    return true;
}

/*
 * Expected type of the arguments:
 * value evalInWindow(string script,
 *                    object window)
 */
static bool
EvalInWindow(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportError(cx, "Function requires two arguments");
        return false;
    }

    if (!args[0].isString() || !args[1].isObject()) {
        JS_ReportError(cx, "Invalid arguments");
        return false;
    }

    RootedString srcString(cx, args[0].toString());
    RootedObject targetScope(cx, &args[1].toObject());

    nsDependentJSString srcDepString;
    if (!srcDepString.init(cx, srcString)) {
        JS_ReportError(cx, "Source string is invalid");
        return false;
    }

    return EvalInWindow(cx, srcDepString, targetScope, args.rval());
}

static bool
CreateObjectIn(JSContext *cx, unsigned argc, jsval *vp)
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
CloneInto(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportError(cx, "Function requires at least 2 arguments");
        return false;
    }

    RootedValue options(cx, args.length() > 2 ? args[2] : UndefinedValue());
    return xpc::CloneInto(cx, args[0], args[1], options, args.rval());
}

} /* namespace xpc */

static bool
sandbox_enumerate(JSContext *cx, HandleObject obj)
{
    return JS_EnumerateStandardClasses(cx, obj);
}

static bool
sandbox_resolve(JSContext *cx, HandleObject obj, HandleId id)
{
    bool resolved;
    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

static void
sandbox_finalize(JSFreeOp *fop, JSObject *obj)
{
    nsIScriptObjectPrincipal *sop =
        static_cast<nsIScriptObjectPrincipal *>(xpc_GetJSPrivate(obj));
    if (!sop) {
        // sop can be null if CreateSandboxObject fails in the middle.
        return;
    }

    static_cast<SandboxPrivate *>(sop)->ForgetGlobalObject();
    NS_RELEASE(sop);
    DestroyProtoAndIfaceCache(obj);
}

static bool
sandbox_convert(JSContext *cx, HandleObject obj, JSType type, MutableHandleValue vp)
{
    if (type == JSTYPE_OBJECT) {
        vp.set(OBJECT_TO_JSVAL(obj));
        return true;
    }

    return JS_ConvertStub(cx, obj, type, vp);
}

#define XPCONNECT_SANDBOX_CLASS_METADATA_SLOT (XPCONNECT_GLOBAL_EXTRA_SLOT_OFFSET)

static const JSClass SandboxClass = {
    "Sandbox",
    XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(1),
    JS_PropertyStub,   JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    sandbox_enumerate, sandbox_resolve, sandbox_convert,  sandbox_finalize,
    nullptr, nullptr, nullptr, JS_GlobalObjectTraceHook
};

static const JSFunctionSpec SandboxFunctions[] = {
    JS_FS("dump",    SandboxDump,    1,0),
    JS_FS("debug",   SandboxDebug,   1,0),
    JS_FS("importFunction", SandboxImport, 1,0),
    JS_FS_END
};

bool
xpc::IsSandbox(JSObject *obj)
{
    return GetObjectJSClass(obj) == &SandboxClass;
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

xpc::SandboxProxyHandler xpc::sandboxProxyHandler;

bool
xpc::IsSandboxPrototypeProxy(JSObject *obj)
{
    return js::IsProxy(obj) &&
           js::GetProxyHandler(obj) == &xpc::sandboxProxyHandler;
}

bool
xpc::SandboxCallableProxyHandler::call(JSContext *cx, JS::Handle<JSObject*> proxy,
                                       const JS::CallArgs &args)
{
    // We forward the call to our underlying callable.

    // The parent of our proxy is the SandboxProxyHandler proxy
    RootedObject sandboxProxy(cx, JS_GetParent(proxy));
    MOZ_ASSERT(js::IsProxy(sandboxProxy) &&
               js::GetProxyHandler(sandboxProxy) == &xpc::sandboxProxyHandler);

    // The parent of the sandboxProxy is the sandbox global, and the
    // target object is the original proto.
    RootedObject sandboxGlobal(cx, JS_GetParent(sandboxProxy));
    MOZ_ASSERT(js::GetObjectJSClass(sandboxGlobal) == &SandboxClass);

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

xpc::SandboxCallableProxyHandler xpc::sandboxCallableProxyHandler;

/*
 * Wrap a callable such that if we're called with oldThisObj as the
 * "this" we will instead call it with newThisObj as the this.
 */
static JSObject*
WrapCallable(JSContext *cx, JSObject *callable, JSObject *sandboxProtoProxy)
{
    MOZ_ASSERT(JS_ObjectIsCallable(cx, callable));
    // Our proxy is wrapping the callable.  So we need to use the
    // callable as the private.  We use the given sandboxProtoProxy as
    // the parent, and our call() hook depends on that.
    MOZ_ASSERT(js::IsProxy(sandboxProtoProxy) &&
               js::GetProxyHandler(sandboxProtoProxy) ==
                 &xpc::sandboxProxyHandler);

    RootedValue priv(cx, ObjectValue(*callable));
    js::ProxyOptions options;
    options.selectDefaultClass(true);
    return js::NewProxyObject(cx, &xpc::sandboxCallableProxyHandler,
                              priv, nullptr,
                              sandboxProtoProxy, options);
}

template<typename Op>
bool BindPropertyOp(JSContext *cx, Op &op, JSPropertyDescriptor *desc, HandleId id,
                    unsigned attrFlag, HandleObject sandboxProtoProxy)
{
    if (!op) {
        return true;
    }

    RootedObject func(cx);
    if (desc->attrs & attrFlag) {
        // Already an object
        func = JS_FUNC_TO_DATA_PTR(JSObject *, op);
    } else {
        // We have an actual property op.  For getters, we use 0
        // args, for setters we use 1 arg.
        uint32_t args = (attrFlag == JSPROP_GETTER) ? 0 : 1;
        RootedObject obj(cx, desc->obj);
        func = GeneratePropertyOp(cx, obj, id, args, op);
        if (!func)
            return false;
    }
    func = WrapCallable(cx, func, sandboxProtoProxy);
    if (!func)
        return false;
    op = JS_DATA_TO_FUNC_PTR(Op, func.get());
    desc->attrs |= attrFlag;
    return true;
}

extern bool
XPC_WN_Helper_GetProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp);
extern bool
XPC_WN_Helper_SetProperty(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp);

bool
xpc::SandboxProxyHandler::getPropertyDescriptor(JSContext *cx,
                                                JS::Handle<JSObject*> proxy,
                                                JS::Handle<jsid> id,
                                                JS::MutableHandle<JSPropertyDescriptor> desc)
{
    JS::RootedObject obj(cx, wrappedObject(proxy));

    MOZ_ASSERT(js::GetObjectCompartment(obj) == js::GetObjectCompartment(proxy));
    if (!JS_GetPropertyDescriptorById(cx, obj, id, desc))
        return false;

    if (!desc.object())
        return true; // No property, nothing to do

    // Now fix up the getter/setter/value as needed to be bound to desc->obj
    // Don't mess with holder_get and holder_set, though, because those rely on
    // the "vp is prefilled with the value in the slot" behavior that property
    // ops can in theory rely on, but our property op forwarder doesn't know how
    // to make that happen.  Since we really only need to rebind the DOM methods
    // here, not rebindings holder_get and holder_set is OK.
    //
    // Similarly, don't mess with XPC_WN_Helper_GetProperty and
    // XPC_WN_Helper_SetProperty, for the same reasons: that could confuse our
    // access to expandos when we're not doing Xrays.
    if (desc.getter() != xpc::holder_get &&
        desc.getter() != XPC_WN_Helper_GetProperty &&
        !BindPropertyOp(cx, desc.getter(), desc.address(), id, JSPROP_GETTER, proxy))
        return false;
    if (desc.setter() != xpc::holder_set &&
        desc.setter() != XPC_WN_Helper_SetProperty &&
        !BindPropertyOp(cx, desc.setter(), desc.address(), id, JSPROP_SETTER, proxy))
        return false;
    if (desc.value().isObject()) {
        JSObject* val = &desc.value().toObject();
        if (JS_ObjectIsCallable(cx, val)) {
            val = WrapCallable(cx, val, proxy);
            if (!val)
                return false;
            desc.value().setObject(*val);
        }
    }

    return true;
}

bool
xpc::SandboxProxyHandler::getOwnPropertyDescriptor(JSContext *cx,
                                                   JS::Handle<JSObject*> proxy,
                                                   JS::Handle<jsid> id,
                                                   JS::MutableHandle<JSPropertyDescriptor> desc)
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
xpc::SandboxProxyHandler::has(JSContext *cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id, bool *bp)
{
    return BaseProxyHandler::has(cx, proxy, id, bp);
}
bool
xpc::SandboxProxyHandler::hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
                                 JS::Handle<jsid> id, bool *bp)
{
    return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
}

bool
xpc::SandboxProxyHandler::get(JSContext *cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<JSObject*> receiver,
                              JS::Handle<jsid> id,
                              JS::MutableHandle<Value> vp)
{
    return BaseProxyHandler::get(cx, proxy, receiver, id, vp);
}

bool
xpc::SandboxProxyHandler::set(JSContext *cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<JSObject*> receiver,
                              JS::Handle<jsid> id,
                              bool strict,
                              JS::MutableHandle<Value> vp)
{
    return BaseProxyHandler::set(cx, proxy, receiver, id, strict, vp);
}

bool
xpc::SandboxProxyHandler::keys(JSContext *cx, JS::Handle<JSObject*> proxy,
                               AutoIdVector &props)
{
    return BaseProxyHandler::keys(cx, proxy, props);
}

bool
xpc::SandboxProxyHandler::iterate(JSContext *cx, JS::Handle<JSObject*> proxy,
                                  unsigned flags, JS::MutableHandle<Value> vp)
{
    return BaseProxyHandler::iterate(cx, proxy, flags, vp);
}

bool
xpc::GlobalProperties::Parse(JSContext *cx, JS::HandleObject obj)
{
    MOZ_ASSERT(JS_IsArrayObject(cx, obj));

    uint32_t length;
    bool ok = JS_GetArrayLength(cx, obj, &length);
    NS_ENSURE_TRUE(ok, false);
    bool promise = Promise;
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
        if (promise && !strcmp(name.ptr(), "-Promise")) {
            Promise = false;
        } else if (!strcmp(name.ptr(), "CSS")) {
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
        } else if (!strcmp(name.ptr(), "atob")) {
            atob = true;
        } else if (!strcmp(name.ptr(), "btoa")) {
            btoa = true;
        } else {
            JS_ReportError(cx, "Unknown property name: %s", name.ptr());
            return false;
        }
    }
    return true;
}

bool
xpc::GlobalProperties::Define(JSContext *cx, JS::HandleObject obj)
{
    if (CSS && !dom::CSSBinding::GetConstructorObject(cx, obj))
        return false;

    if (Promise && !dom::PromiseBinding::GetConstructorObject(cx, obj))
        return false;

    if (indexedDB && AccessCheck::isChrome(obj) &&
        !IndexedDatabaseManager::DefineIndexedDB(cx, obj))
        return false;

    if (XMLHttpRequest &&
        !JS_DefineFunction(cx, obj, "XMLHttpRequest", CreateXMLHttpRequest, 0, JSFUN_CONSTRUCTOR))
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

    if (atob &&
        !JS_DefineFunction(cx, obj, "atob", Atob, 1, 0))
        return false;

    if (btoa &&
        !JS_DefineFunction(cx, obj, "btoa", Btoa, 1, 0))
        return false;

    return true;
}

nsresult
xpc::CreateSandboxObject(JSContext *cx, MutableHandleValue vp, nsISupports *prinOrSop,
                         SandboxOptions& options)
{
    // Create the sandbox global object
    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(prinOrSop);
    if (!principal) {
        nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(prinOrSop);
        if (sop) {
            principal = sop->GetPrincipal();
        } else {
            principal = do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
            MOZ_ASSERT(NS_FAILED(rv) || principal, "Bad return from do_CreateInstance");

            if (!principal || NS_FAILED(rv)) {
                if (NS_SUCCEEDED(rv)) {
                    rv = NS_ERROR_FAILURE;
                }

                return rv;
            }
        }
        MOZ_ASSERT(principal);
    }

    JS::CompartmentOptions compartmentOptions;
    if (options.sameZoneAs)
        compartmentOptions.setSameZoneAs(js::UncheckedUnwrap(options.sameZoneAs));
    else
        compartmentOptions.setZone(JS::SystemZone);

    compartmentOptions.setInvisibleToDebugger(options.invisibleToDebugger)
                      .setDiscardSource(options.discardSource)
                      .setTrace(TraceXPCGlobal);

    RootedObject sandbox(cx, xpc::CreateGlobalObject(cx, &SandboxClass,
                                                     principal, compartmentOptions));
    if (!sandbox)
        return NS_ERROR_FAILURE;

    // Set up the wantXrays flag, which indicates whether xrays are desired even
    // for same-origin access.
    //
    // This flag has historically been ignored for chrome sandboxes due to
    // quirks in the wrapping implementation that have now been removed. Indeed,
    // same-origin Xrays for chrome->chrome access seems a bit superfluous.
    // Arguably we should just flip the default for chrome and still honor the
    // flag, but such a change would break code in subtle ways for minimal
    // benefit. So we just switch it off here.
    xpc::GetCompartmentPrivate(sandbox)->wantXrays =
      AccessCheck::isChrome(sandbox) ? false : options.wantXrays;

    {
        JSAutoCompartment ac(cx, sandbox);

        if (options.proto) {
            bool ok = JS_WrapObject(cx, &options.proto);
            if (!ok)
                return NS_ERROR_XPC_UNEXPECTED;

            if (xpc::WrapperFactory::IsXrayWrapper(options.proto) && !options.wantXrays) {
                RootedValue v(cx, ObjectValue(*options.proto));
                if (!xpc::WrapperFactory::WaiveXrayAndWrap(cx, &v))
                    return NS_ERROR_FAILURE;
                options.proto = &v.toObject();
            }

            // Now check what sort of thing we've got in |proto|
            JSObject *unwrappedProto = js::UncheckedUnwrap(options.proto, false);
            const js::Class *unwrappedClass = js::GetObjectClass(unwrappedProto);
            if (IS_WN_CLASS(unwrappedClass) ||
                mozilla::dom::IsDOMClass(Jsvalify(unwrappedClass))) {
                // Wrap it up in a proxy that will do the right thing in terms
                // of this-binding for methods.
                RootedValue priv(cx, ObjectValue(*options.proto));
                options.proto = js::NewProxyObject(cx, &xpc::sandboxProxyHandler,
                                                   priv, nullptr, sandbox);
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

        bool allowComponents = nsContentUtils::IsSystemPrincipal(principal) ||
                               nsContentUtils::IsExpandedPrincipal(principal);
        if (options.wantComponents && allowComponents &&
            !GetObjectScope(sandbox)->AttachComponentsObject(cx))
            return NS_ERROR_XPC_UNEXPECTED;

        if (!XPCNativeWrapper::AttachNewConstructorObject(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;

        if (!JS_DefineFunctions(cx, sandbox, SandboxFunctions))
            return NS_ERROR_XPC_UNEXPECTED;

        if (options.wantExportHelpers &&
            (!JS_DefineFunction(cx, sandbox, "exportFunction", ExportFunction, 3, 0) ||
             !JS_DefineFunction(cx, sandbox, "evalInWindow", EvalInWindow, 2, 0) ||
             !JS_DefineFunction(cx, sandbox, "createObjectIn", CreateObjectIn, 2, 0) ||
             !JS_DefineFunction(cx, sandbox, "cloneInto", CloneInto, 3, 0) ||
             !JS_DefineFunction(cx, sandbox, "isProxy", IsProxy, 1, 0)))
            return NS_ERROR_XPC_UNEXPECTED;

        if (!options.globalProperties.Define(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;
    }


    // We have this crazy behavior where wantXrays=false also implies that the
    // returned sandbox is implicitly waived. We've stopped advertising it, but
    // keep supporting it for now.
    vp.setObject(*sandbox);
    if (options.wantXrays && !JS_WrapValue(cx, vp))
        return NS_ERROR_UNEXPECTED;
    if (!options.wantXrays && !xpc::WrapperFactory::WaiveXrayAndWrap(cx, vp))
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
nsXPCComponents_utils_Sandbox::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                                    JSObject *objArg, const CallArgs &args, bool *_retval)
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
nsXPCComponents_utils_Sandbox::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                                         JSObject *objArg, const CallArgs &args, bool *_retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

/*
 * For sandbox constructor the first argument can be a URI string in which case
 * we use the related Codebase Principal for the sandbox.
 */
bool
ParsePrincipal(JSContext *cx, HandleString codebase, nsIPrincipal **principal)
{
    MOZ_ASSERT(principal);
    MOZ_ASSERT(codebase);
    nsCOMPtr<nsIURI> uri;
    nsDependentJSString codebaseStr;
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
GetPrincipalOrSOP(JSContext *cx, HandleObject from, nsISupports **out)
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
GetExpandedPrincipal(JSContext *cx, HandleObject arrayObj, nsIExpandedPrincipal **out)
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
OptionsBase::ParseValue(const char *name, MutableHandleValue prop, bool *aFound)
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
OptionsBase::ParseBoolean(const char *name, bool *prop)
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
OptionsBase::ParseObject(const char *name, MutableHandleObject prop)
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
 * Helper that tries to get a string property from the options object.
 */
bool
OptionsBase::ParseString(const char *name, nsCString &prop)
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

    char *tmp = JS_EncodeString(mCx, value.toString());
    NS_ENSURE_TRUE(tmp, false);
    prop.Adopt(tmp, strlen(tmp));
    return true;
}

/*
 * Helper that tries to get a string property from the options object.
 */
bool
OptionsBase::ParseString(const char *name, nsString &prop)
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

    nsDependentJSString strVal;
    strVal.init(mCx, value.toString());
    prop = strVal;
    return true;
}

/*
 * Helper that tries to get jsid property from the options object.
 */
bool
OptionsBase::ParseId(const char *name, MutableHandleId prop)
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
    return ParseObject("sandboxPrototype", &proto) &&
           ParseBoolean("wantXrays", &wantXrays) &&
           ParseBoolean("wantComponents", &wantComponents) &&
           ParseBoolean("wantExportHelpers", &wantExportHelpers) &&
           ParseString("sandboxName", sandboxName) &&
           ParseObject("sameZoneAs", &sameZoneAs) &&
           ParseBoolean("invisibleToDebugger", &invisibleToDebugger) &&
           ParseBoolean("discardSource", &discardSource) &&
           ParseGlobalProperties() &&
           ParseValue("metadata", &metadata);
}

static nsresult
AssembleSandboxMemoryReporterName(JSContext *cx, nsCString &sandboxName)
{
    // Use a default name when the caller did not provide a sandboxName.
    if (sandboxName.IsEmpty())
        sandboxName = NS_LITERAL_CSTRING("[anonymous sandbox]");

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    // Get the xpconnect native call context.
    nsAXPCNativeCallContext *cc = nullptr;
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
nsXPCComponents_utils_Sandbox::CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                               JSContext *cx, HandleObject obj,
                                               const CallArgs &args, bool *_retval)
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

    *_retval = true;
    return NS_OK;
}

class ContextHolder : public nsIScriptObjectPrincipal
{
public:
    ContextHolder(JSContext *aOuterCx, HandleObject aSandbox, nsIPrincipal *aPrincipal);
    virtual ~ContextHolder();

    JSContext * GetJSContext()
    {
        return mJSContext;
    }

    nsIPrincipal * GetPrincipal() { return mPrincipal; }

    NS_DECL_ISUPPORTS

private:
    JSContext* mJSContext;
    nsCOMPtr<nsIPrincipal> mPrincipal;
};

NS_IMPL_ISUPPORTS(ContextHolder, nsIScriptObjectPrincipal)

ContextHolder::ContextHolder(JSContext *aOuterCx,
                             HandleObject aSandbox,
                             nsIPrincipal *aPrincipal)
    : mJSContext(JS_NewContext(JS_GetRuntime(aOuterCx), 1024)),
      mPrincipal(aPrincipal)
{
    if (mJSContext) {
        bool isChrome;
        DebugOnly<nsresult> rv = nsXPConnect::SecurityManager()->
                                   IsSystemPrincipal(mPrincipal, &isChrome);
        MOZ_ASSERT(NS_SUCCEEDED(rv));

        JS::ContextOptionsRef(mJSContext).setDontReportUncaught(true)
                                         .setPrivateIsNSISupports(true);
        js::SetDefaultObjectForContext(mJSContext, aSandbox);
        JS_SetContextPrivate(mJSContext, this);
    }
}

ContextHolder::~ContextHolder()
{
    if (mJSContext)
        JS_DestroyContextNoGC(mJSContext);
}

nsresult
xpc::EvalInSandbox(JSContext *cx, HandleObject sandboxArg, const nsAString& source,
                   const nsACString& filename, int32_t lineNo,
                   JSVersion jsVersion, bool returnStringOnly, MutableHandleValue rval)
{
    JS_AbortIfWrongThread(JS_GetRuntime(cx));
    rval.set(UndefinedValue());

    bool waiveXray = xpc::WrapperFactory::HasWaiveXrayFlag(sandboxArg);
    RootedObject sandbox(cx, js::CheckedUnwrap(sandboxArg));
    if (!sandbox || js::GetObjectJSClass(sandbox) != &SandboxClass) {
        return NS_ERROR_INVALID_ARG;
    }

    nsIScriptObjectPrincipal *sop =
        (nsIScriptObjectPrincipal*)xpc_GetJSPrivate(sandbox);
    MOZ_ASSERT(sop, "Invalid sandbox passed");
    nsCOMPtr<nsIPrincipal> prin = sop->GetPrincipal();
    NS_ENSURE_TRUE(prin, NS_ERROR_FAILURE);

    nsAutoCString filenameBuf;
    if (!filename.IsVoid()) {
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
        // Make a special cx for the sandbox and push it.
        // NB: As soon as the RefPtr goes away, the cx goes away. So declare
        // it first so that it disappears last.
        nsRefPtr<ContextHolder> sandcxHolder = new ContextHolder(cx, sandbox, prin);
        JSContext *sandcx = sandcxHolder->GetJSContext();
        if (!sandcx) {
            JS_ReportError(cx, "Can't prepare context for evalInSandbox");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        nsCxPusher pusher;
        pusher.Push(sandcx);
        JSAutoCompartment ac(sandcx, sandbox);

        JS::CompileOptions options(sandcx);
        options.setFileAndLine(filenameBuf.get(), lineNo);
        if (jsVersion != JSVERSION_DEFAULT)
               options.setVersion(jsVersion);
        JS::RootedObject rootedSandbox(sandcx, sandbox);
        ok = JS::Evaluate(sandcx, rootedSandbox, options,
                          PromiseFlatString(source).get(), source.Length(), &v);
        if (ok && returnStringOnly && !v.isUndefined()) {
            JSString *str = ToString(sandcx, v);
            ok = !!str;
            v = ok ? JS::StringValue(str) : JS::UndefinedValue();
        }

        // If the sandbox threw an exception, grab it off the context.
        if (JS_GetPendingException(sandcx, &exn)) {
            MOZ_ASSERT(!ok);
            JS_ClearPendingException(sandcx);
            if (returnStringOnly) {
                // The caller asked for strings only, convert the
                // exception into a string.
                JSString *str = ToString(sandcx, exn);
                exn = str ? JS::StringValue(str) : JS::UndefinedValue();
            }
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

static bool
NonCloningFunctionForwarder(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue v(cx, js::GetFunctionNativeReserved(&args.callee(), 0));
    MOZ_ASSERT(v.isObject(), "weird function");

    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj) {
        return false;
    }
    return JS_CallFunctionValue(cx, obj, v, args, args.rval());
}

/*
 * Forwards the call to the exported function. Clones all the non reflectors, ignores
 * the |this| argument.
 */
static bool
CloningFunctionForwarder(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue v(cx, js::GetFunctionNativeReserved(&args.callee(), 0));
    NS_ASSERTION(v.isObject(), "weird function");
    RootedObject origFunObj(cx, UncheckedUnwrap(&v.toObject()));
    {
        JSAutoCompartment ac(cx, origFunObj);
        // Note: only the arguments are cloned not the |this| or the |callee|.
        // Function forwarder does not use those.
        for (unsigned i = 0; i < args.length(); i++) {
            if (!CloneNonReflectors(cx, args[i])) {
                return false;
            }
        }

        // JS API does not support any JSObject to JSFunction conversion,
        // so let's use JS_CallFunctionValue instead.
        RootedValue functionVal(cx, ObjectValue(*origFunObj));

        if (!JS_CallFunctionValue(cx, JS::NullPtr(), functionVal, args, args.rval()))
            return false;
    }

    // Return value must be wrapped.
    return JS_WrapValue(cx, args.rval());
}

bool
xpc::NewFunctionForwarder(JSContext *cx, HandleId id, HandleObject callable, bool doclone,
                          MutableHandleValue vp)
{
    JSFunction *fun = js::NewFunctionByIdWithReserved(cx, doclone ? CloningFunctionForwarder :
                                                                    NonCloningFunctionForwarder,
                                                                    0,0, JS::CurrentGlobalOrNull(cx), id);

    if (!fun)
        return false;

    JSObject *funobj = JS_GetFunctionObject(fun);
    js::SetFunctionNativeReserved(funobj, 0, ObjectValue(*callable));
    vp.setObject(*funobj);
    return true;
}

bool
xpc::NewFunctionForwarder(JSContext *cx, HandleObject callable, bool doclone,
                          MutableHandleValue vp)
{
    RootedId emptyId(cx);
    RootedValue emptyStringValue(cx, JS_GetEmptyStringValue(cx));
    if (!JS_ValueToId(cx, emptyStringValue, &emptyId))
        return false;

    return NewFunctionForwarder(cx, emptyId, callable, doclone, vp);
}


nsresult
xpc::GetSandboxMetadata(JSContext *cx, HandleObject sandbox, MutableHandleValue rval)
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
xpc::SetSandboxMetadata(JSContext *cx, HandleObject sandbox, HandleValue metadataArg)
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
