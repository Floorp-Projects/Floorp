/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "WrapperFactory.h"
#include "jsfriendapi.h"
#include "jsproxy.h"
#include "jswrapper.h"
#include "js/StructuredClone.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"

using namespace mozilla;
using namespace JS;
using namespace js;

namespace xpc {

bool
IsReflector(JSObject *obj)
{
    return IS_WN_REFLECTOR(obj) || dom::IsDOMObject(obj);
}

enum StackScopedCloneTags {
    SCTAG_BASE = JS_SCTAG_USER_MIN,
    SCTAG_REFLECTOR,
    SCTAG_FUNCTION
};

class MOZ_STACK_CLASS StackScopedCloneData {
public:
    StackScopedCloneData(JSContext *aCx, StackScopedCloneOptions *aOptions)
        : mOptions(aOptions)
        , mReflectors(aCx)
        , mFunctions(aCx)
    {}

    StackScopedCloneOptions *mOptions;
    AutoObjectVector mReflectors;
    AutoObjectVector mFunctions;
};

static JSObject *
StackScopedCloneRead(JSContext *cx, JSStructuredCloneReader *reader, uint32_t tag,
                     uint32_t data, void *closure)
{
    MOZ_ASSERT(closure, "Null pointer!");
    StackScopedCloneData *cloneData = static_cast<StackScopedCloneData *>(closure);
    if (tag == SCTAG_REFLECTOR) {
        MOZ_ASSERT(!data);

        size_t idx;
        if (!JS_ReadBytes(reader, &idx, sizeof(size_t)))
            return nullptr;

        RootedObject reflector(cx, cloneData->mReflectors[idx]);
        MOZ_ASSERT(reflector, "No object pointer?");
        MOZ_ASSERT(IsReflector(reflector), "Object pointer must be a reflector!");

        if (!JS_WrapObject(cx, &reflector))
            return nullptr;

        return reflector;
    }

    if (tag == SCTAG_FUNCTION) {
      MOZ_ASSERT(data < cloneData->mFunctions.length());

      RootedValue functionValue(cx);
      RootedObject obj(cx, cloneData->mFunctions[data]);

      if (!JS_WrapObject(cx, &obj))
          return nullptr;

      if (!xpc::NewFunctionForwarder(cx, obj, true, &functionValue))
          return nullptr;

      return &functionValue.toObject();
    }

    MOZ_ASSERT_UNREACHABLE("Encountered garbage in the clone stream!");
    return nullptr;
}

// The HTML5 structured cloning algorithm includes a few DOM objects, notably
// Blob and FileList. That wouldn't in itself be a reason to support them here,
// but we've historically supported them for Cu.cloneInto (where we didn't support
// other reflectors), so we need to continue to do so in the wrapReflectors == false
// case to maintain compatibility.
//
// Blob and FileList clones are supposed to give brand new objects, rather than
// cross-compartment wrappers. For this, our current implementation relies on the
// fact that these objects are implemented with XPConnect and have one reflector
// per scope. This will need to be fixed when Blob and File move to WebIDL. See
// bug 827823 comment 6.
bool IsBlobOrFileList(JSObject *obj)
{
    nsISupports *supports = UnwrapReflectorToISupports(obj);
    if (!supports)
        return false;
    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(supports);
    if (blob)
        return true;
    nsCOMPtr<nsIDOMFileList> fileList = do_QueryInterface(supports);
    if (fileList)
        return true;
    return false;
}

static bool
StackScopedCloneWrite(JSContext *cx, JSStructuredCloneWriter *writer,
                      Handle<JSObject *> objArg, void *closure)
{
    MOZ_ASSERT(closure, "Null pointer!");
    StackScopedCloneData *cloneData = static_cast<StackScopedCloneData *>(closure);

    // The SpiderMonkey structured clone machinery does a CheckedUnwrap, but
    // doesn't strip off outer windows. Do that to avoid confusing the reflector
    // detection.
    RootedObject obj(cx, JS_ObjectToInnerObject(cx, objArg));
    if ((cloneData->mOptions->wrapReflectors && IsReflector(obj)) ||
        IsBlobOrFileList(obj))
    {
        if (!cloneData->mReflectors.append(obj))
            return false;

        size_t idx = cloneData->mReflectors.length() - 1;
        if (!JS_WriteUint32Pair(writer, SCTAG_REFLECTOR, 0))
            return false;
        if (!JS_WriteBytes(writer, &idx, sizeof(size_t)))
            return false;
        return true;
    }

    if (cloneData->mOptions->cloneFunctions && JS_ObjectIsCallable(cx, obj)) {
        cloneData->mFunctions.append(obj);
        return JS_WriteUint32Pair(writer, SCTAG_FUNCTION, cloneData->mFunctions.length() - 1);
    }

    JS_ReportError(cx, "Encountered unsupported value type writing stack-scoped structured clone");
    return false;
}

static const JSStructuredCloneCallbacks gStackScopedCloneCallbacks = {
    StackScopedCloneRead,
    StackScopedCloneWrite,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

/*
 * General-purpose structured-cloning utility for cases where the structured
 * clone buffer is only used in stack-scope (that is to say, the buffer does
 * not escape from this function). The stack-scoping allows us to pass
 * references to various JSObjects directly in certain situations without
 * worrying about lifetime issues.
 *
 * This function assumes that |cx| is already entered the compartment we want
 * to clone to, and that |val| may not be same-compartment with cx. When the
 * function returns, |val| is set to the result of the clone.
 */
bool
StackScopedClone(JSContext *cx, StackScopedCloneOptions &options,
                 MutableHandleValue val)
{
    JSAutoStructuredCloneBuffer buffer;
    StackScopedCloneData data(cx, &options);
    {
        // For parsing val we have to enter its compartment.
        // (unless it's a primitive)
        Maybe<JSAutoCompartment> ac;
        if (val.isObject()) {
            ac.construct(cx, &val.toObject());
        } else if (val.isString() && !JS_WrapValue(cx, val)) {
            return false;
        }

        if (!buffer.write(cx, val, &gStackScopedCloneCallbacks, &data))
            return false;
    }

    // Now recreate the clones in the target compartment.
    return buffer.read(cx, val, &gStackScopedCloneCallbacks, &data);
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
        StackScopedCloneOptions options;
        options.wrapReflectors = true;
        for (unsigned i = 0; i < args.length(); i++) {
            if (!StackScopedClone(cx, options, args[i])) {
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
bool
NewFunctionForwarder(JSContext *cx, HandleId id, HandleObject callable, bool doclone,
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
NewFunctionForwarder(JSContext *cx, HandleObject callable, bool doclone,
                          MutableHandleValue vp)
{
    RootedId emptyId(cx);
    RootedValue emptyStringValue(cx, JS_GetEmptyStringValue(cx));
    if (!JS_ValueToId(cx, emptyStringValue, &emptyId))
        return false;

    return NewFunctionForwarder(cx, emptyId, callable, doclone, vp);
}

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
            StackScopedCloneOptions cloneOptions;
            cloneOptions.wrapReflectors = true;
            if (StackScopedClone(wndCx, cloneOptions, &exn))
                js::SetPendingExceptionCrossContext(cx, exn);

            return false;
        }
    }

    // Let's clone the return value back to the callers compartment.
    StackScopedCloneOptions cloneOptions;
    cloneOptions.wrapReflectors = true;
    if (!StackScopedClone(cx, cloneOptions, rval)) {
        rval.set(UndefinedValue());
        return false;
    }

    return true;
}

bool
CreateObjectIn(JSContext *cx, HandleValue vobj, CreateObjectInOptions &options,
               MutableHandleValue rval)
{
    if (!vobj.isObject()) {
        JS_ReportError(cx, "Expected an object as the target scope");
        return false;
    }

    RootedObject scope(cx, js::CheckedUnwrap(&vobj.toObject()));
    if (!scope) {
        JS_ReportError(cx, "Permission denied to create object in the target scope");
        return false;
    }

    bool define = !JSID_IS_VOID(options.defineAs);

    if (define && js::IsScriptedProxy(scope)) {
        JS_ReportError(cx, "Defining property on proxy object is not allowed");
        return false;
    }

    RootedObject obj(cx);
    {
        JSAutoCompartment ac(cx, scope);
        obj = JS_NewObject(cx, nullptr, JS::NullPtr(), scope);
        if (!obj)
            return false;

        if (define) {
            if (!JS_DefinePropertyById(cx, scope, options.defineAs, obj, JSPROP_ENUMERATE,
                                       JS_PropertyStub, JS_StrictPropertyStub))
                return false;
        }
    }

    rval.setObject(*obj);
    if (!WrapperFactory::WaiveXrayAndWrap(cx, rval))
        return false;

    return true;
}

} /* namespace xpc */
