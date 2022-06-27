/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/ShadowRealm.h"

#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "builtin/ModuleObject.h"
#include "builtin/Promise.h"
#include "builtin/WrappedFunctionObject.h"
#include "frontend/BytecodeCompilation.h"
#include "js/CompilationAndEvaluation.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/Principals.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/ShadowRealmCallbacks.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/TypeDecls.h"
#include "js/Wrapper.h"
#include "vm/GlobalObject.h"
#include "vm/ObjectOperations.h"

#include "builtin/HandlerFunction-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;

using JS::AutoStableStringChars;
using JS::CompileOptions;
using JS::SourceOwnership;
using JS::SourceText;

static JSObject* DefaultNewShadowRealmGlobal(JSContext* cx,
                                             JS::RealmOptions& options,
                                             JSPrincipals* principals,
                                             HandleObject unused) {
  static const JSClass shadowRealmGlobal = {
      "ShadowRealmGlobal", JSCLASS_GLOBAL_FLAGS, &JS::DefaultGlobalClassOps};

  return JS_NewGlobalObject(cx, &shadowRealmGlobal, principals,
                            JS::FireOnNewGlobalHook, options);
}

// https://tc39.es/proposal-shadowrealm/#sec-shadowrealm-constructor
/*static*/
bool ShadowRealmObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1. If NewTarget is undefined, throw a TypeError exception.
  if (!args.isConstructing()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CONSTRUCTOR, "ShadowRealm");
    return false;
  }

  // Step 2. Let O be ? OrdinaryCreateFromConstructor(NewTarget,
  // "%ShadowRealm.prototype%", « [[ShadowRealm]], [[ExecutionContext]] »).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_ShadowRealm,
                                          &proto)) {
    return false;
  }

  Rooted<ShadowRealmObject*> shadowRealmObj(
      cx, NewObjectWithClassProto<ShadowRealmObject>(cx, proto));
  if (!shadowRealmObj) {
    return false;
  }

  // Instead of managing Realms, spidermonkey associates a realm with a global
  // object, and so we will manage and store a global.

  // Step 3. Let realmRec be CreateRealm().

  // Initially steal creation options from current realm:
  JS::RealmOptions options(cx->realm()->creationOptions(),
                           cx->realm()->behaviors());

  // We don't want to have to deal with CCWs in addition to
  // WrappedFunctionObjects.
  options.creationOptions().setExistingCompartment(cx->compartment());

  JS::GlobalCreationCallback newGlobal =
      cx->runtime()->getShadowRealmGlobalCreationCallback();
  // If an embedding didn't provide a callback to initialize the global,
  // use the basic default one.
  if (!newGlobal) {
    newGlobal = DefaultNewShadowRealmGlobal;
  }

  // Our shadow realm inherits the principals of the current realm,
  // but is otherwise constrained.
  JSPrincipals* principals = JS::GetRealmPrincipals(cx->realm());

  // Steps 5-11: In SpiderMonkey these fall under the aegis of the global
  //             creation. It's worth noting that the newGlobal callback
  //             needs to respect the SetRealmGlobalObject call below, which
  //             sets the global to
  //             OrdinaryObjectCreate(intrinsics.[[%Object.prototype%]]).
  //
  // Step 5. Let context be a new execution context.
  // Step 6. Set the Function of context to null.
  // Step 7. Set the Realm of context to realmRec.
  // Step 8. Set the ScriptOrModule of context to null.
  // Step 9. Set O.[[ExecutionContext]] to context.
  // Step 10. Perform ? SetRealmGlobalObject(realmRec, undefined, undefined).
  // Step 11. Perform ? SetDefaultGlobalBindings(O.[[ShadowRealm]]).
  RootedObject global(cx, newGlobal(cx, options, principals, cx->global()));
  if (!global) {
    return false;
  }

  // Make sure the new global hook obeyed our request in the
  // creation options to have a same compartment global.
  MOZ_RELEASE_ASSERT(global->compartment() == cx->compartment());

  // Step 4. Set O.[[ShadowRealm]] to realmRec.
  shadowRealmObj->initFixedSlot(GlobalSlot, ObjectValue(*global));

  // Step 12. Perform ? HostInitializeShadowRealm(O.[[ShadowRealm]]).
  JS::GlobalInitializeCallback hostInitializeShadowRealm =
      cx->runtime()->getShadowRealmInitializeGlobalCallback();
  if (hostInitializeShadowRealm) {
    if (!hostInitializeShadowRealm(cx, global)) {
      return false;
    }
  }

  // Step 13. Return O.
  args.rval().setObject(*shadowRealmObj);
  return true;
}

// https://tc39.es/proposal-shadowrealm/#sec-validateshadowrealmobject
// (slightly modified into a cast operator too)
static ShadowRealmObject* ValidateShadowRealmObject(JSContext* cx,
                                                    HandleObject O) {
  // Step 1. Perform ? RequireInternalSlot(O, [[ShadowRealm]]).
  // Step 2. Perform ? RequireInternalSlot(O, [[ExecutionContext]]).
  RootedObject maybeUnwrappedO(cx, O);
  if (IsCrossCompartmentWrapper(O)) {
    maybeUnwrappedO = CheckedUnwrapDynamic(O, cx);
    // Unwrapping failed; security wrapper denied.
    if (!maybeUnwrappedO) {
      return nullptr;
    }
  }

  if (!maybeUnwrappedO->is<ShadowRealmObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_SHADOW_REALM);
    return nullptr;
  }

  return &maybeUnwrappedO->as<ShadowRealmObject>();
}

//  PerformShadowRealmEval ( sourceText: a String, callerRealm: a Realm Record,
//                          evalRealm: a Realm Record, )
//
// https://tc39.es/proposal-shadowrealm/#sec-performshadowrealmeval
static bool PerformShadowRealmEval(JSContext* cx, HandleString sourceText,
                                   Realm* callerRealm, Realm* evalRealm,
                                   MutableHandleValue rval) {
  MOZ_ASSERT(callerRealm != evalRealm);

  // Step 1. Perform ? HostEnsureCanCompileStrings(callerRealm, evalRealm).
  if (!cx->isRuntimeCodeGenEnabled(JS::RuntimeCode::JS, sourceText)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_CSP_BLOCKED_SHADOWREALM);
    return false;
  }

  // Need to compile the script into the realm we will execute into.
  //
  // We hoist the error handling out however to ensure that errors
  // are thrown from the correct realm.
  bool compileSuccess = false;
  bool evalSuccess = false;

  do {
    Rooted<GlobalObject*> evalRealmGlobal(cx, evalRealm->maybeGlobal());
    AutoRealm ar(cx, evalRealmGlobal);

    // Step 2. Perform the following substeps in an implementation-defined
    // order, possibly interleaving parsing and error detection:
    //     a. Let script be ParseText(! StringToCodePoints(sourceText), Script).
    //     b. If script is a List of errors, throw a SyntaxError exception.
    //     c. If script Contains ScriptBody is false, return undefined.
    //     d. Let body be the ScriptBody of script.
    //     e. If body Contains NewTarget is true, throw a SyntaxError exception.
    //     f. If body Contains SuperProperty is true, throw a SyntaxError
    //     exception. g. If body Contains SuperCall is true, throw a SyntaxError
    //     exception.

    AutoStableStringChars linearChars(cx);
    if (!linearChars.initTwoByte(cx, sourceText)) {
      return false;
    }
    SourceText<char16_t> srcBuf;

    const char16_t* chars = linearChars.twoByteRange().begin().get();
    SourceOwnership ownership = linearChars.maybeGiveOwnershipToCaller()
                                    ? SourceOwnership::TakeOwnership
                                    : SourceOwnership::Borrowed;
    if (!srcBuf.init(cx, chars, linearChars.length(), ownership)) {
      return false;
    }

    // Lets propagate some information into the compilation here.
    //
    // We may need to censor the stacks eventually, see
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1770017
    RootedScript callerScript(cx);
    const char* filename;
    unsigned lineno;
    uint32_t pcOffset;
    bool mutedErrors;
    DescribeScriptedCallerForCompilation(cx, &callerScript, &filename, &lineno,
                                         &pcOffset, &mutedErrors);

    CompileOptions options(cx);
    options.setIsRunOnce(true)
        .setNoScriptRval(false)
        .setMutedErrors(mutedErrors)
        .setFileAndLine(filename, lineno);

    Rooted<Scope*> enclosing(cx, &evalRealmGlobal->emptyGlobalScope());
    RootedScript script(
        cx, frontend::CompileEvalScript(cx, options, srcBuf, enclosing,
                                        evalRealmGlobal));

    compileSuccess = !!script;
    if (!compileSuccess) {
      break;
    }

    // Step 3. Let strictEval be IsStrict of script.
    // Step 4. Let runningContext be the running execution context.
    // Step 5. Let lexEnv be NewDeclarativeEnvironment(evalRealm.[[GlobalEnv]]).
    // Step 6. Let varEnv be evalRealm.[[GlobalEnv]].
    // Step 7. If strictEval is true, set varEnv to lexEnv.
    // Step 8. If runningContext is not already suspended, suspend
    // runningContext. Step 9. Let evalContext be a new ECMAScript code
    // execution context. Step 10. Set evalContext's Function to null. Step 11.
    // Set evalContext's Realm to evalRealm. Step 12. Set evalContext's
    // ScriptOrModule to null. Step 13. Set evalContext's VariableEnvironment to
    // varEnv. Step 14. Set evalContext's LexicalEnvironment to lexEnv. Step 15.
    // Push evalContext onto the execution context stack; evalContext is
    //          now the running execution context.
    // Step 16. Let result be  EvalDeclarationInstantiation(body, varEnv,
    //            lexEnv,  null, strictEval).
    // Step 17. If result.[[Type]] is normal, then
    //     a. Set result to the result of evaluating body.
    // Step 18. If result.[[Type]] is normal and result.[[Value]] is empty, then
    //     a. Set result to NormalCompletion(undefined).

    // Step 19. Suspend evalContext and remove it from the execution context
    // stack.
    // Step 20. Resume the context that is now on the top of the execution
    // context stack as the running execution context.
    RootedObject environment(cx, &evalRealmGlobal->lexicalEnvironment());
    evalSuccess = ExecuteKernel(cx, script, environment,
                                /* evalInFrame = */ NullFramePtr(), rval);
  } while (false);  // AutoRealm

  if (!compileSuccess) {
    // MG:XXX: figure out how to extract the syntax error information for
    // re-throw here (See DebuggerObject::getErrorColumnNumber ?)
    //
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1769848

    // The SyntaxError here needs to come from the calling global, so has to
    // happen outside the AutoRealm above.
    JS_ClearPendingException(cx);
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SHADOW_REALM_GENERIC_SYNTAX);
    return false;
  }

  if (!evalSuccess) {
    // Step 21. If result.[[Type]]  is not normal, throw a TypeError
    // exception.
    //
    // The type error here needs to come from the calling global, so has to
    // happen outside the AutoRealm above.

    // MG:XXX: Figure out how to extract the error message and include in
    // message of TypeError (if possible): See discussion in
    // https://github.com/tc39/proposal-shadowrealm/issues/353 for some
    // potential pitfalls (i.e. what if the error has a getter on the message
    // property?)
    //
    // I imagine we could do something like GetPropertyPure, and have a nice
    // error message if we *don't* have anything to worry about.
    //
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1769849

    JS_ClearPendingException(cx);
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SHADOW_REALM_EVALUATE_FAILURE);
    return false;
  }

  // Step 22. Return ? GetWrappedValue(callerRealm, result.[[Value]]).
  return GetWrappedValue(cx, callerRealm, rval, rval);
}

// ShadowRealm.prototype.evaluate ( sourceText )
// https://tc39.es/proposal-shadowrealm/#sec-shadowrealm.prototype.evaluate
static bool ShadowRealm_evaluate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1. Let O be this value (implicit ToObject)
  RootedObject obj(cx, ToObject(cx, args.thisv()));
  if (!obj) {
    return false;
  }

  // Step 2. Perform ? ValidateShadowRealmObject(O)
  Rooted<ShadowRealmObject*> shadowRealm(cx,
                                         ValidateShadowRealmObject(cx, obj));
  if (!shadowRealm) {
    return false;
  }

  // Step 3. If Type(sourceText) is not String, throw a TypeError exception.
  if (!args.get(0).isString()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SHADOW_REALM_EVALUATE_NOT_STRING);
    return false;
  }
  RootedString sourceText(cx, args.get(0).toString());

  // Step 4. Let callerRealm be the current Realm Record.
  Realm* callerRealm = cx->realm();

  // Step 5. Let evalRealm be O.[[ShadowRealm]].
  Realm* evalRealm = shadowRealm->getShadowRealm();
  // Step 6. Return ? PerformShadowRealmEval(sourceText, callerRealm,
  // evalRealm).
  return PerformShadowRealmEval(cx, sourceText, callerRealm, evalRealm,
                                args.rval());
}

//  ShadowRealm.prototype.importValue ( specifier, exportName )
// https://tc39.es/proposal-shadowrealm/#sec-shadowrealm.prototype.importvalue
static bool ShadowRealm_importValue(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_SHADOW_REALM_IMPORTVALUE_NOT_IMPLEMENTED);
  return false;
}

static const JSFunctionSpec shadowrealm_methods[] = {
    JS_FN("evaluate", ShadowRealm_evaluate, 1, 0),
    JS_FN("importValue", ShadowRealm_importValue, 2, 0), JS_FS_END};

static const JSPropertySpec shadowrealm_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "ShadowRealm", JSPROP_READONLY), JS_PS_END};

static const ClassSpec ShadowRealmObjectClassSpec = {
    GenericCreateConstructor<ShadowRealmObject::construct, 0,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<ShadowRealmObject>,
    nullptr,                // Static methods
    nullptr,                // Static properties
    shadowrealm_methods,    // Methods
    shadowrealm_properties  // Properties
};

const JSClass ShadowRealmObject::class_ = {
    "ShadowRealm",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ShadowRealm) |
        JSCLASS_HAS_RESERVED_SLOTS(ShadowRealmObject::SlotCount),
    JS_NULL_CLASS_OPS, &ShadowRealmObjectClassSpec};

const JSClass ShadowRealmObject::protoClass_ = {
    "ShadowRealm.prototype", JSCLASS_HAS_CACHED_PROTO(JSProto_ShadowRealm),
    JS_NULL_CLASS_OPS, &ShadowRealmObjectClassSpec};
