/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"  // JS::Call, JS::Construct, JS::IsCallable
#include "js/Exception.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefinePropertyById
#include "js/Proxy.h"
#include "js/Wrapper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "js/Object.h"  // JS::GetCompartment

using namespace mozilla;
using namespace mozilla::dom;
using namespace JS;

namespace xpc {

bool IsReflector(JSObject* obj, JSContext* cx) {
  obj = js::CheckedUnwrapDynamic(obj, cx, /* stopAtWindowProxy = */ false);
  if (!obj) {
    return false;
  }
  return IS_WN_REFLECTOR(obj) || dom::IsDOMObject(obj);
}

enum StackScopedCloneTags {
  SCTAG_BASE = JS_SCTAG_USER_MIN,
  SCTAG_REFLECTOR,
  SCTAG_BLOB,
  SCTAG_FUNCTION,
};

class MOZ_STACK_CLASS StackScopedCloneData : public StructuredCloneHolderBase {
 public:
  StackScopedCloneData(JSContext* aCx, StackScopedCloneOptions* aOptions)
      : mOptions(aOptions), mReflectors(aCx), mFunctions(aCx) {}

  ~StackScopedCloneData() { Clear(); }

  JSObject* CustomReadHandler(JSContext* aCx, JSStructuredCloneReader* aReader,
                              const JS::CloneDataPolicy& aCloneDataPolicy,
                              uint32_t aTag, uint32_t aData) override {
    if (aTag == SCTAG_REFLECTOR) {
      MOZ_ASSERT(!aData);

      size_t idx;
      if (!JS_ReadBytes(aReader, &idx, sizeof(size_t))) {
        return nullptr;
      }

      RootedObject reflector(aCx, mReflectors[idx]);
      MOZ_ASSERT(reflector, "No object pointer?");
      MOZ_ASSERT(IsReflector(reflector, aCx),
                 "Object pointer must be a reflector!");

      if (!JS_WrapObject(aCx, &reflector)) {
        return nullptr;
      }

      return reflector;
    }

    if (aTag == SCTAG_FUNCTION) {
      MOZ_ASSERT(aData < mFunctions.length());

      RootedValue functionValue(aCx);
      RootedObject obj(aCx, mFunctions[aData]);

      if (!JS_WrapObject(aCx, &obj)) {
        return nullptr;
      }

      FunctionForwarderOptions forwarderOptions;
      if (!xpc::NewFunctionForwarder(aCx, JSID_VOIDHANDLE, obj,
                                     forwarderOptions, &functionValue)) {
        return nullptr;
      }

      return &functionValue.toObject();
    }

    if (aTag == SCTAG_BLOB) {
      MOZ_ASSERT(!aData);

      size_t idx;
      if (!JS_ReadBytes(aReader, &idx, sizeof(size_t))) {
        return nullptr;
      }

      nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
      MOZ_ASSERT(global);

      // RefPtr<File> needs to go out of scope before toObjectOrNull() is called
      // because otherwise the static analysis thinks it can gc the JSObject via
      // the stack.
      JS::Rooted<JS::Value> val(aCx);
      {
        RefPtr<Blob> blob = Blob::Create(global, mBlobImpls[idx]);
        if (NS_WARN_IF(!blob)) {
          return nullptr;
        }

        if (!ToJSValue(aCx, blob, &val)) {
          return nullptr;
        }
      }

      return val.toObjectOrNull();
    }

    MOZ_ASSERT_UNREACHABLE("Encountered garbage in the clone stream!");
    return nullptr;
  }

  bool CustomWriteHandler(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                          JS::Handle<JSObject*> aObj,
                          bool* aSameProcessScopeRequired) override {
    {
      JS::Rooted<JSObject*> obj(aCx, aObj);
      Blob* blob = nullptr;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, &obj, blob))) {
        BlobImpl* blobImpl = blob->Impl();
        MOZ_ASSERT(blobImpl);

        // XXX(Bug 1631371) Check if this should use a fallible operation as it
        // pretended earlier.
        mBlobImpls.AppendElement(blobImpl);

        size_t idx = mBlobImpls.Length() - 1;
        return JS_WriteUint32Pair(aWriter, SCTAG_BLOB, 0) &&
               JS_WriteBytes(aWriter, &idx, sizeof(size_t));
      }
    }

    if (mOptions->wrapReflectors && IsReflector(aObj, aCx)) {
      if (!mReflectors.append(aObj)) {
        return false;
      }

      size_t idx = mReflectors.length() - 1;
      if (!JS_WriteUint32Pair(aWriter, SCTAG_REFLECTOR, 0)) {
        return false;
      }
      if (!JS_WriteBytes(aWriter, &idx, sizeof(size_t))) {
        return false;
      }
      return true;
    }

    if (JS::IsCallable(aObj)) {
      if (mOptions->cloneFunctions) {
        if (!mFunctions.append(aObj)) {
          return false;
        }
        return JS_WriteUint32Pair(aWriter, SCTAG_FUNCTION,
                                  mFunctions.length() - 1);
      } else {
        JS_ReportErrorASCII(
            aCx, "Permission denied to pass a Function via structured clone");
        return false;
      }
    }

    JS_ReportErrorASCII(aCx,
                        "Encountered unsupported value type writing "
                        "stack-scoped structured clone");
    return false;
  }

  StackScopedCloneOptions* mOptions;
  RootedObjectVector mReflectors;
  RootedObjectVector mFunctions;
  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
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
bool StackScopedClone(JSContext* cx, StackScopedCloneOptions& options,
                      HandleObject sourceScope, MutableHandleValue val) {
  StackScopedCloneData data(cx, &options);
  {
    // For parsing val we have to enter (a realm in) its compartment.
    JSAutoRealm ar(cx, sourceScope);
    if (!data.Write(cx, val)) {
      return false;
    }
  }

  // Now recreate the clones in the target realm.
  if (!data.Read(cx, val)) {
    return false;
  }

  // Deep-freeze if requested.
  if (options.deepFreeze && val.isObject()) {
    RootedObject obj(cx, &val.toObject());
    if (!JS_DeepFreezeObject(cx, obj)) {
      return false;
    }
  }

  return true;
}

// Note - This function mirrors the logic of CheckPassToChrome in
// ChromeObjectWrapper.cpp.
static bool CheckSameOriginArg(JSContext* cx, FunctionForwarderOptions& options,
                               HandleValue v) {
  // Consumers can explicitly opt out of this security check. This is used in
  // the web console to allow the utility functions to accept cross-origin
  // Windows.
  if (options.allowCrossOriginArguments) {
    return true;
  }

  // Primitives are fine.
  if (!v.isObject()) {
    return true;
  }
  RootedObject obj(cx, &v.toObject());
  MOZ_ASSERT(JS::GetCompartment(obj) != js::GetContextCompartment(cx),
             "This should be invoked after entering the compartment but before "
             "wrapping the values");

  // Non-wrappers are fine.
  if (!js::IsWrapper(obj)) {
    return true;
  }

  // Wrappers leading back to the scope of the exported function are fine.
  if (JS::GetCompartment(js::UncheckedUnwrap(obj)) ==
      js::GetContextCompartment(cx)) {
    return true;
  }

  // Same-origin wrappers are fine.
  if (AccessCheck::wrapperSubsumes(obj)) {
    return true;
  }

  // Badness.
  JS_ReportErrorASCII(cx,
                      "Permission denied to pass object to exported function");
  return false;
}

// Sanitize the exception on cx (which comes from calling unwrappedFun), if the
// current Realm of cx shouldn't have access to it.  unwrappedFun is generally
// _not_ in the current Realm of cx here.
static void MaybeSanitizeException(JSContext* cx,
                                   JS::Handle<JSObject*> unwrappedFun) {
  // Ensure that we are not propagating more-privileged exceptions
  // to less-privileged code.
  nsIPrincipal* callerPrincipal = nsContentUtils::SubjectPrincipal(cx);

  // Re-enter the unwrappedFun Realm to do get the current exception, so we
  // don't end up unnecessarily wrapping exceptions.
  {  // Scope for JSAutoRealm
    JSAutoRealm ar(cx, unwrappedFun);

    JS::ExceptionStack exnStack(cx);
    // If JS::GetPendingExceptionStack returns false, this was an uncatchable
    // exception, or we somehow failed to wrap the exception into our
    // compartment.  In either case, treating this as uncatchable exception,
    // by returning without setting any exception on the JSContext,
    // seems fine.
    if (!JS::GetPendingExceptionStack(cx, &exnStack)) {
      JS_ClearPendingException(cx);
      return;
    }

    // Let through non-objects as-is, because some APIs rely on
    // that and accidental exceptions are never non-objects.
    if (!exnStack.exception().isObject() ||
        callerPrincipal->Subsumes(nsContentUtils::ObjectPrincipal(
            js::UncheckedUnwrap(&exnStack.exception().toObject())))) {
      // Just leave exn as-is.
      return;
    }

    // Whoever we are throwing the exception to should not have access to
    // the exception.  Sanitize it. First clear the existing exception.
    JS_ClearPendingException(cx);
    {  // Scope for AutoJSAPI
      AutoJSAPI jsapi;
      if (jsapi.Init(unwrappedFun)) {
        JS::SetPendingExceptionStack(cx, exnStack);
      }
      // If Init() fails, we can't report the exception, but oh, well.

      // Now just let the AutoJSAPI go out of scope and it will report the
      // exception in its destructor.
    }
  }

  // Now back in our original Realm again, throw a sanitized exception.
  ErrorResult rv;
  rv.ThrowInvalidStateError("An exception was thrown");
  // Can we provide a better context here?
  Unused << rv.MaybeSetPendingException(cx);
}

static bool FunctionForwarder(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Grab the options from the reserved slot.
  RootedObject optionsObj(
      cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  FunctionForwarderOptions options(cx, optionsObj);
  if (!options.Parse()) {
    return false;
  }

  // Grab and unwrap the underlying callable.
  RootedValue v(cx, js::GetFunctionNativeReserved(&args.callee(), 0));
  RootedObject unwrappedFun(cx, js::UncheckedUnwrap(&v.toObject()));

  RootedValue thisVal(cx, NullValue());
  if (!args.isConstructing()) {
    RootedObject thisObject(cx);
    if (!args.computeThis(cx, &thisObject)) {
      return false;
    }
    thisVal.setObject(*thisObject);
  }

  bool ok = true;
  {
    // We manually implement the contents of CrossCompartmentWrapper::call
    // here, because certain function wrappers (notably content->nsEP) are
    // not callable.
    JSAutoRealm ar(cx, unwrappedFun);
    bool crossCompartment =
        JS::GetCompartment(unwrappedFun) != JS::GetCompartment(&args.callee());
    if (crossCompartment) {
      if (!CheckSameOriginArg(cx, options, thisVal) ||
          !JS_WrapValue(cx, &thisVal)) {
        return false;
      }

      for (size_t n = 0; n < args.length(); ++n) {
        if (!CheckSameOriginArg(cx, options, args[n]) ||
            !JS_WrapValue(cx, args[n])) {
          return false;
        }
      }
    }

    RootedValue fval(cx, ObjectValue(*unwrappedFun));
    if (args.isConstructing()) {
      RootedObject obj(cx);
      ok = JS::Construct(cx, fval, args, &obj);
      if (ok) {
        args.rval().setObject(*obj);
      }
    } else {
      ok = JS::Call(cx, thisVal, fval, args, args.rval());
    }
  }

  // Now that we are back in our original Realm, we can check whether to
  // sanitize the exception.
  if (!ok) {
    MaybeSanitizeException(cx, unwrappedFun);
    return false;
  }

  // Rewrap the return value into our compartment.
  return JS_WrapValue(cx, args.rval());
}

bool NewFunctionForwarder(JSContext* cx, HandleId idArg, HandleObject callable,
                          FunctionForwarderOptions& options,
                          MutableHandleValue vp) {
  RootedId id(cx, idArg);
  if (id == JSID_VOIDHANDLE) {
    id = GetJSIDByIndex(cx, XPCJSContext::IDX_EMPTYSTRING);
  }

  // If our callable is a (possibly wrapped) function, we can give
  // the exported thing the right number of args.
  unsigned nargs = 0;
  RootedObject unwrapped(cx, js::UncheckedUnwrap(callable));
  if (unwrapped) {
    if (JSFunction* fun = JS_GetObjectFunction(unwrapped)) {
      nargs = JS_GetFunctionArity(fun);
    }
  }

  // We have no way of knowing whether the underlying function wants to be a
  // constructor or not, so we just mark all forwarders as constructors, and
  // let the underlying function throw for construct calls if it wants.
  JSFunction* fun = js::NewFunctionByIdWithReserved(
      cx, FunctionForwarder, nargs, JSFUN_CONSTRUCTOR, id);
  if (!fun) {
    return false;
  }

  // Stash the callable in slot 0.
  AssertSameCompartment(cx, callable);
  RootedObject funobj(cx, JS_GetFunctionObject(fun));
  js::SetFunctionNativeReserved(funobj, 0, ObjectValue(*callable));

  // Stash the options in slot 1.
  RootedObject optionsObj(cx, options.ToJSObject(cx));
  if (!optionsObj) {
    return false;
  }
  js::SetFunctionNativeReserved(funobj, 1, ObjectValue(*optionsObj));

  vp.setObject(*funobj);
  return true;
}

bool ExportFunction(JSContext* cx, HandleValue vfunction, HandleValue vscope,
                    HandleValue voptions, MutableHandleValue rval) {
  bool hasOptions = !voptions.isUndefined();
  if (!vscope.isObject() || !vfunction.isObject() ||
      (hasOptions && !voptions.isObject())) {
    JS_ReportErrorASCII(cx, "Invalid argument");
    return false;
  }

  RootedObject funObj(cx, &vfunction.toObject());
  RootedObject targetScope(cx, &vscope.toObject());
  ExportFunctionOptions options(cx,
                                hasOptions ? &voptions.toObject() : nullptr);
  if (hasOptions && !options.Parse()) {
    return false;
  }

  // Restrictions:
  // * We must subsume the scope we are exporting to.
  // * We must subsume the function being exported, because the function
  //   forwarder manually circumvents security wrapper CALL restrictions.
  targetScope = js::CheckedUnwrapDynamic(targetScope, cx);
  // For the function we can just CheckedUnwrapStatic, because if it's
  // not callable we're going to fail out anyway.
  funObj = js::CheckedUnwrapStatic(funObj);
  if (!targetScope || !funObj) {
    JS_ReportErrorASCII(cx, "Permission denied to export function into scope");
    return false;
  }

  if (js::IsScriptedProxy(targetScope)) {
    JS_ReportErrorASCII(cx, "Defining property on proxy object is not allowed");
    return false;
  }

  {
    // We need to operate in the target scope from here on, let's enter
    // its realm.
    JSAutoRealm ar(cx, targetScope);

    // Unwrapping to see if we have a callable.
    funObj = UncheckedUnwrap(funObj);
    if (!JS::IsCallable(funObj)) {
      JS_ReportErrorASCII(cx, "First argument must be a function");
      return false;
    }

    RootedId id(cx, options.defineAs);
    if (JSID_IS_VOID(id)) {
      // If there wasn't any function name specified, copy the name from the
      // function being imported.  But be careful in case the callable we have
      // is not actually a JSFunction.
      RootedString funName(cx);
      JSFunction* fun = JS_GetObjectFunction(funObj);
      if (fun) {
        funName = JS_GetFunctionId(fun);
      }
      if (!funName) {
        funName = JS_AtomizeAndPinString(cx, "");
      }
      JS_MarkCrossZoneIdValue(cx, StringValue(funName));

      if (!JS_StringToId(cx, funName, &id)) {
        return false;
      }
    } else {
      JS_MarkCrossZoneId(cx, id);
    }
    MOZ_ASSERT(JSID_IS_STRING(id));

    // The function forwarder will live in the target compartment. Since
    // this function will be referenced from its private slot, to avoid a
    // GC hazard, we must wrap it to the same compartment.
    if (!JS_WrapObject(cx, &funObj)) {
      return false;
    }

    // And now, let's create the forwarder function in the target compartment
    // for the function the be exported.
    FunctionForwarderOptions forwarderOptions;
    forwarderOptions.allowCrossOriginArguments =
        options.allowCrossOriginArguments;
    if (!NewFunctionForwarder(cx, id, funObj, forwarderOptions, rval)) {
      JS_ReportErrorASCII(cx, "Exporting function failed");
      return false;
    }

    // We have the forwarder function in the target compartment. If
    // defineAs was set, we also need to define it as a property on
    // the target.
    if (!JSID_IS_VOID(options.defineAs)) {
      if (!JS_DefinePropertyById(cx, targetScope, id, rval, JSPROP_ENUMERATE)) {
        return false;
      }
    }
  }

  // Finally we have to re-wrap the exported function back to the caller
  // compartment.
  if (!JS_WrapValue(cx, rval)) {
    return false;
  }

  return true;
}

bool CreateObjectIn(JSContext* cx, HandleValue vobj,
                    CreateObjectInOptions& options, MutableHandleValue rval) {
  if (!vobj.isObject()) {
    JS_ReportErrorASCII(cx, "Expected an object as the target scope");
    return false;
  }

  // cx represents the caller Realm.
  RootedObject scope(cx, js::CheckedUnwrapDynamic(&vobj.toObject(), cx));
  if (!scope) {
    JS_ReportErrorASCII(
        cx, "Permission denied to create object in the target scope");
    return false;
  }

  bool define = !JSID_IS_VOID(options.defineAs);

  if (define && js::IsScriptedProxy(scope)) {
    JS_ReportErrorASCII(cx, "Defining property on proxy object is not allowed");
    return false;
  }

  RootedObject obj(cx);
  {
    JSAutoRealm ar(cx, scope);
    JS_MarkCrossZoneId(cx, options.defineAs);

    obj = JS_NewPlainObject(cx);
    if (!obj) {
      return false;
    }

    if (define) {
      if (!JS_DefinePropertyById(cx, scope, options.defineAs, obj,
                                 JSPROP_ENUMERATE))
        return false;
    }
  }

  rval.setObject(*obj);
  if (!WrapperFactory::WaiveXrayAndWrap(cx, rval)) {
    return false;
  }

  return true;
}

} /* namespace xpc */
