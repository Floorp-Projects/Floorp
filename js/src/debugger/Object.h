/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_Object_h
#define debugger_Object_h

#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT
#include "mozilla/Attributes.h"  // for MOZ_MUST_USE
#include "mozilla/Maybe.h"       // for Maybe
#include "mozilla/Range.h"       // for Range
#include "mozilla/Result.h"      // for Result

#include "jsapi.h"             // for JSContext
#include "NamespaceImports.h"  // for Value, MutableHandleValue, HandleId

#include "gc/Rooting.h"       // for HandleDebuggerObject
#include "js/Promise.h"       // for PromiseState
#include "js/Proxy.h"         // for PropertyDescriptor
#include "vm/JSObject.h"      // for JSObject (ptr only)
#include "vm/NativeObject.h"  // for NativeObject

class JSAtom;

namespace js {

class Completion;
class Debugger;
class EvalOptions;
class GlobalObject;
class PromiseObject;

enum { JSSLOT_DEBUGOBJECT_OWNER, JSSLOT_DEBUGOBJECT_COUNT };

class DebuggerObject : public NativeObject {
 public:
  static const Class class_;

  static NativeObject* initClass(JSContext* cx, Handle<GlobalObject*> global,
                                 HandleObject debugCtor);
  static DebuggerObject* create(JSContext* cx, HandleObject proto,
                                HandleObject referent,
                                HandleNativeObject debugger);

  void trace(JSTracer* trc);

  // Properties
  static MOZ_MUST_USE bool getClassName(JSContext* cx,
                                        HandleDebuggerObject object,
                                        MutableHandleString result);
  static MOZ_MUST_USE bool getParameterNames(
      JSContext* cx, HandleDebuggerObject object,
      MutableHandle<StringVector> result);
  static MOZ_MUST_USE bool getBoundTargetFunction(
      JSContext* cx, HandleDebuggerObject object,
      MutableHandleDebuggerObject result);
  static MOZ_MUST_USE bool getBoundThis(JSContext* cx,
                                        HandleDebuggerObject object,
                                        MutableHandleValue result);
  static MOZ_MUST_USE bool getBoundArguments(JSContext* cx,
                                             HandleDebuggerObject object,
                                             MutableHandle<ValueVector> result);
  static MOZ_MUST_USE bool getAllocationSite(JSContext* cx,
                                             HandleDebuggerObject object,
                                             MutableHandleObject result);
  static MOZ_MUST_USE bool getErrorMessageName(JSContext* cx,
                                               HandleDebuggerObject object,
                                               MutableHandleString result);
  static MOZ_MUST_USE bool getErrorNotes(JSContext* cx,
                                         HandleDebuggerObject object,
                                         MutableHandleValue result);
  static MOZ_MUST_USE bool getErrorLineNumber(JSContext* cx,
                                              HandleDebuggerObject object,
                                              MutableHandleValue result);
  static MOZ_MUST_USE bool getErrorColumnNumber(JSContext* cx,
                                                HandleDebuggerObject object,
                                                MutableHandleValue result);
  static MOZ_MUST_USE bool getScriptedProxyTarget(
      JSContext* cx, HandleDebuggerObject object,
      MutableHandleDebuggerObject result);
  static MOZ_MUST_USE bool getScriptedProxyHandler(
      JSContext* cx, HandleDebuggerObject object,
      MutableHandleDebuggerObject result);
  static MOZ_MUST_USE bool getPromiseValue(JSContext* cx,
                                           HandleDebuggerObject object,
                                           MutableHandleValue result);
  static MOZ_MUST_USE bool getPromiseReason(JSContext* cx,
                                            HandleDebuggerObject object,
                                            MutableHandleValue result);

  // Methods
  static MOZ_MUST_USE bool isExtensible(JSContext* cx,
                                        HandleDebuggerObject object,
                                        bool& result);
  static MOZ_MUST_USE bool isSealed(JSContext* cx, HandleDebuggerObject object,
                                    bool& result);
  static MOZ_MUST_USE bool isFrozen(JSContext* cx, HandleDebuggerObject object,
                                    bool& result);
  static MOZ_MUST_USE JS::Result<Completion> getProperty(
      JSContext* cx, HandleDebuggerObject object, HandleId id,
      HandleValue receiver);
  static MOZ_MUST_USE JS::Result<Completion> setProperty(
      JSContext* cx, HandleDebuggerObject object, HandleId id,
      HandleValue value, HandleValue receiver);
  static MOZ_MUST_USE bool getPrototypeOf(JSContext* cx,
                                          HandleDebuggerObject object,
                                          MutableHandleDebuggerObject result);
  static MOZ_MUST_USE bool getOwnPropertyNames(JSContext* cx,
                                               HandleDebuggerObject object,
                                               MutableHandle<IdVector> result);
  static MOZ_MUST_USE bool getOwnPropertySymbols(
      JSContext* cx, HandleDebuggerObject object,
      MutableHandle<IdVector> result);
  static MOZ_MUST_USE bool getOwnPropertyDescriptor(
      JSContext* cx, HandleDebuggerObject object, HandleId id,
      MutableHandle<PropertyDescriptor> desc);
  static MOZ_MUST_USE bool preventExtensions(JSContext* cx,
                                             HandleDebuggerObject object);
  static MOZ_MUST_USE bool seal(JSContext* cx, HandleDebuggerObject object);
  static MOZ_MUST_USE bool freeze(JSContext* cx, HandleDebuggerObject object);
  static MOZ_MUST_USE bool defineProperty(JSContext* cx,
                                          HandleDebuggerObject object,
                                          HandleId id,
                                          Handle<PropertyDescriptor> desc);
  static MOZ_MUST_USE bool defineProperties(
      JSContext* cx, HandleDebuggerObject object, Handle<IdVector> ids,
      Handle<PropertyDescriptorVector> descs);
  static MOZ_MUST_USE bool deleteProperty(JSContext* cx,
                                          HandleDebuggerObject object,
                                          HandleId id, ObjectOpResult& result);
  static MOZ_MUST_USE mozilla::Maybe<Completion> call(
      JSContext* cx, HandleDebuggerObject object, HandleValue thisv,
      Handle<ValueVector> args);
  static MOZ_MUST_USE bool forceLexicalInitializationByName(
      JSContext* cx, HandleDebuggerObject object, HandleId id, bool& result);
  static MOZ_MUST_USE JS::Result<Completion> executeInGlobal(
      JSContext* cx, HandleDebuggerObject object,
      mozilla::Range<const char16_t> chars, HandleObject bindings,
      const EvalOptions& options);
  static MOZ_MUST_USE bool makeDebuggeeValue(JSContext* cx,
                                             HandleDebuggerObject object,
                                             HandleValue value,
                                             MutableHandleValue result);
  static MOZ_MUST_USE bool makeDebuggeeNativeFunction(
      JSContext* cx, HandleDebuggerObject object, HandleValue value,
      MutableHandleValue result);
  static MOZ_MUST_USE bool unsafeDereference(JSContext* cx,
                                             HandleDebuggerObject object,
                                             MutableHandleObject result);
  static MOZ_MUST_USE bool unwrap(JSContext* cx, HandleDebuggerObject object,
                                  MutableHandleDebuggerObject result);

  // Infallible properties
  bool isCallable() const;
  bool isFunction() const;
  bool isDebuggeeFunction() const;
  bool isBoundFunction() const;
  bool isArrowFunction() const;
  bool isAsyncFunction() const;
  bool isGeneratorFunction() const;
  bool isGlobal() const;
  bool isScriptedProxy() const;
  bool isPromise() const;
  JSAtom* name(JSContext* cx) const;
  JSAtom* displayName(JSContext* cx) const;
  JS::PromiseState promiseState() const;
  double promiseLifetime() const;
  double promiseTimeToResolution() const;

 private:
  enum { OWNER_SLOT };

  static const unsigned RESERVED_SLOTS = 1;

  static const ClassOps classOps_;

  static const JSPropertySpec properties_[];
  static const JSPropertySpec promiseProperties_[];
  static const JSFunctionSpec methods_[];

  JSObject* referent() const {
    JSObject* obj = (JSObject*)getPrivate();
    MOZ_ASSERT(obj);
    return obj;
  }

  Debugger* owner() const;
  PromiseObject* promise() const;

  static MOZ_MUST_USE bool requireGlobal(JSContext* cx,
                                         HandleDebuggerObject object);
  static MOZ_MUST_USE bool requirePromise(JSContext* cx,
                                          HandleDebuggerObject object);
  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  // JSNative properties
  static MOZ_MUST_USE bool callableGetter(JSContext* cx, unsigned argc,
                                          Value* vp);
  static MOZ_MUST_USE bool isBoundFunctionGetter(JSContext* cx, unsigned argc,
                                                 Value* vp);
  static MOZ_MUST_USE bool isArrowFunctionGetter(JSContext* cx, unsigned argc,
                                                 Value* vp);
  static MOZ_MUST_USE bool isAsyncFunctionGetter(JSContext* cx, unsigned argc,
                                                 Value* vp);
  static MOZ_MUST_USE bool isGeneratorFunctionGetter(JSContext* cx,
                                                     unsigned argc, Value* vp);
  static MOZ_MUST_USE bool protoGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool classGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool nameGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool displayNameGetter(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool parameterNamesGetter(JSContext* cx, unsigned argc,
                                                Value* vp);
  static MOZ_MUST_USE bool scriptGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool environmentGetter(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool boundTargetFunctionGetter(JSContext* cx,
                                                     unsigned argc, Value* vp);
  static MOZ_MUST_USE bool boundThisGetter(JSContext* cx, unsigned argc,
                                           Value* vp);
  static MOZ_MUST_USE bool boundArgumentsGetter(JSContext* cx, unsigned argc,
                                                Value* vp);
  static MOZ_MUST_USE bool allocationSiteGetter(JSContext* cx, unsigned argc,
                                                Value* vp);
  static MOZ_MUST_USE bool errorMessageNameGetter(JSContext* cx, unsigned argc,
                                                  Value* vp);
  static MOZ_MUST_USE bool errorNotesGetter(JSContext* cx, unsigned argc,
                                            Value* vp);
  static MOZ_MUST_USE bool errorLineNumberGetter(JSContext* cx, unsigned argc,
                                                 Value* vp);
  static MOZ_MUST_USE bool errorColumnNumberGetter(JSContext* cx, unsigned argc,
                                                   Value* vp);
  static MOZ_MUST_USE bool isProxyGetter(JSContext* cx, unsigned argc,
                                         Value* vp);
  static MOZ_MUST_USE bool proxyTargetGetter(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool proxyHandlerGetter(JSContext* cx, unsigned argc,
                                              Value* vp);
  static MOZ_MUST_USE bool isPromiseGetter(JSContext* cx, unsigned argc,
                                           Value* vp);
  static MOZ_MUST_USE bool promiseStateGetter(JSContext* cx, unsigned argc,
                                              Value* vp);
  static MOZ_MUST_USE bool promiseValueGetter(JSContext* cx, unsigned argc,
                                              Value* vp);
  static MOZ_MUST_USE bool promiseReasonGetter(JSContext* cx, unsigned argc,
                                               Value* vp);
  static MOZ_MUST_USE bool promiseLifetimeGetter(JSContext* cx, unsigned argc,
                                                 Value* vp);
  static MOZ_MUST_USE bool promiseTimeToResolutionGetter(JSContext* cx,
                                                         unsigned argc,
                                                         Value* vp);
  static MOZ_MUST_USE bool promiseAllocationSiteGetter(JSContext* cx,
                                                       unsigned argc,
                                                       Value* vp);
  static MOZ_MUST_USE bool promiseResolutionSiteGetter(JSContext* cx,
                                                       unsigned argc,
                                                       Value* vp);
  static MOZ_MUST_USE bool promiseIDGetter(JSContext* cx, unsigned argc,
                                           Value* vp);
  static MOZ_MUST_USE bool promiseDependentPromisesGetter(JSContext* cx,
                                                          unsigned argc,
                                                          Value* vp);

  // JSNative methods
  static MOZ_MUST_USE bool isExtensibleMethod(JSContext* cx, unsigned argc,
                                              Value* vp);
  static MOZ_MUST_USE bool isSealedMethod(JSContext* cx, unsigned argc,
                                          Value* vp);
  static MOZ_MUST_USE bool isFrozenMethod(JSContext* cx, unsigned argc,
                                          Value* vp);
  static MOZ_MUST_USE bool getPropertyMethod(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool setPropertyMethod(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool getOwnPropertyNamesMethod(JSContext* cx,
                                                     unsigned argc, Value* vp);
  static MOZ_MUST_USE bool getOwnPropertySymbolsMethod(JSContext* cx,
                                                       unsigned argc,
                                                       Value* vp);
  static MOZ_MUST_USE bool getOwnPropertyDescriptorMethod(JSContext* cx,
                                                          unsigned argc,
                                                          Value* vp);
  static MOZ_MUST_USE bool preventExtensionsMethod(JSContext* cx, unsigned argc,
                                                   Value* vp);
  static MOZ_MUST_USE bool sealMethod(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool freezeMethod(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool definePropertyMethod(JSContext* cx, unsigned argc,
                                                Value* vp);
  static MOZ_MUST_USE bool definePropertiesMethod(JSContext* cx, unsigned argc,
                                                  Value* vp);
  static MOZ_MUST_USE bool deletePropertyMethod(JSContext* cx, unsigned argc,
                                                Value* vp);
  static MOZ_MUST_USE bool callMethod(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool applyMethod(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool asEnvironmentMethod(JSContext* cx, unsigned argc,
                                               Value* vp);
  static MOZ_MUST_USE bool forceLexicalInitializationByNameMethod(JSContext* cx,
                                                                  unsigned argc,
                                                                  Value* vp);
  static MOZ_MUST_USE bool executeInGlobalMethod(JSContext* cx, unsigned argc,
                                                 Value* vp);
  static MOZ_MUST_USE bool executeInGlobalWithBindingsMethod(JSContext* cx,
                                                             unsigned argc,
                                                             Value* vp);
  static MOZ_MUST_USE bool makeDebuggeeValueMethod(JSContext* cx, unsigned argc,
                                                   Value* vp);
  static MOZ_MUST_USE bool makeDebuggeeNativeFunctionMethod(JSContext* cx,
                                                            unsigned argc,
                                                            Value* vp);
  static MOZ_MUST_USE bool unsafeDereferenceMethod(JSContext* cx, unsigned argc,
                                                   Value* vp);
  static MOZ_MUST_USE bool unwrapMethod(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool setInstrumentationMethod(JSContext* cx,
                                                    unsigned argc, Value* vp);
  static MOZ_MUST_USE bool setInstrumentationActiveMethod(JSContext* cx,
                                                          unsigned argc,
                                                          Value* vp);
  static MOZ_MUST_USE bool getErrorReport(JSContext* cx,
                                          HandleObject maybeError,
                                          JSErrorReport*& report);
};

} /* namespace js */

#endif /* debugger_Object_h */
