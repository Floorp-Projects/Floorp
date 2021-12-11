/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4
 * -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/ShellModuleObjectWrapper.h"

#include "jsapi.h"  // JS_GetProperty, JS::Call, JS_NewPlainObject, JS_DefineProperty

#include "builtin/ModuleObject.h"     // js::ModuleObject
#include "js/CallAndConstruct.h"      // JS::Call
#include "js/CallArgs.h"              // JS::CallArgs
#include "js/CallNonGenericMethod.h"  // CallNonGenericMethod
#include "js/Class.h"                 // JSClass, JSCLASS_*
#include "js/ErrorReport.h"           // JS_ReportErrorASCII
#include "js/PropertyAndElement.h"    // JS_GetProperty
#include "js/PropertySpec.h"  // JSPropertySpec, JS_PSG, JS_PS_END, JSFunctionSpec, JS_FN, JS_FN_END
#include "js/RootingAPI.h"    // JS::Rooted, JS::Handle, JS::MutableHandle
#include "js/Value.h"         // JS::Value
#include "vm/ArrayObject.h"   // ArrayObject, NewDenseFullyAllocatedArray
#include "vm/GlobalObject.h"  // DefinePropertiesAndFunctions
#include "vm/JSFunction.h"    // JSFunction
#include "vm/JSObject.h"      // JSObject
#include "vm/List.h"          // ListObject
#include "vm/NativeObject.h"  // NativeObject
#include "vm/Stack.h"         // FixedInvokeArgs

#include "vm/NativeObject-inl.h"  // NativeObject::ensureDenseInitializedLength

using namespace js;
using namespace js::shell;

#define DEFINE_CLASS_IMPL(CLASS)                                            \
  CLASS* Shell##CLASS##Wrapper::get() {                                     \
    return &getReservedSlot(TargetSlot).toObject().as<CLASS>();             \
  }                                                                         \
  /* static */ const JSClass Shell##CLASS##Wrapper::class_ = {              \
      "Shell" #CLASS "Wrapper",                                             \
      JSCLASS_HAS_RESERVED_SLOTS(Shell##CLASS##Wrapper::SlotCount)};        \
  MOZ_ALWAYS_INLINE bool IsShell##CLASS##Wrapper(JS::Handle<JS::Value> v) { \
    return v.isObject() && v.toObject().is<Shell##CLASS##Wrapper>();        \
  }

#define DEFINE_CLASS(CLASS)                                       \
  class Shell##CLASS##Wrapper : public js::NativeObject {         \
   public:                                                        \
    using Target = CLASS;                                         \
    enum ModuleSlot { TargetSlot = 0, SlotCount };                \
    static const JSClass class_;                                  \
    static Shell##CLASS##Wrapper* create(JSContext* cx,           \
                                         JS::Handle<CLASS*> obj); \
    CLASS* get();                                                 \
  };                                                              \
  DEFINE_CLASS_IMPL(CLASS)

DEFINE_CLASS(ModuleRequestObject)
DEFINE_CLASS(ImportEntryObject)
DEFINE_CLASS(ExportEntryObject)
DEFINE_CLASS(RequestedModuleObject)
// NOTE: We don't need wrapper for IndirectBindingMap and ModuleNamespaceObject
DEFINE_CLASS_IMPL(ModuleObject)

#undef DEFINE_CLASS
#undef DEFINE_CLASS_IMPL

bool IdentFilter(JSContext* cx, JS::Handle<JS::Value> from,
                 JS::MutableHandle<JS::Value> to) {
  to.set(from);
  return true;
}

template <class T>
bool SingleFilter(JSContext* cx, JS::Handle<JS::Value> from,
                  JS::MutableHandle<JS::Value> to) {
  using TargetT = typename T::Target;

  if (!from.isObject() || !from.toObject().is<TargetT>()) {
    to.set(from);
    return true;
  }

  JS::Rooted<TargetT*> obj(cx, &from.toObject().as<TargetT>());
  JS::Rooted<T*> filtered(cx, T::create(cx, obj));
  if (!filtered) {
    return false;
  }
  to.setObject(*filtered);
  return true;
}

template <class T>
bool ArrayFilter(JSContext* cx, JS::Handle<JS::Value> from,
                 JS::MutableHandle<JS::Value> to) {
  using TargetT = typename T::Target;

  if (!from.isObject() || !from.toObject().is<ArrayObject>()) {
    to.set(from);
    return true;
  }

  JS::Rooted<ArrayObject*> fromArray(cx, &from.toObject().as<ArrayObject>());
  uint32_t length = fromArray->length();
  JS::Rooted<ArrayObject*> toArray(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!toArray) {
    return false;
  }

  toArray->ensureDenseInitializedLength(0, length);

  for (uint32_t i = 0; i < length; i++) {
    JS::Rooted<JS::Value> item(cx, fromArray->getDenseElement(i));
    JS::Rooted<TargetT*> req(cx, &item.toObject().as<TargetT>());
    JS::Rooted<T*> filtered(cx, T::create(cx, req));
    if (!filtered) {
      return false;
    }
    toArray->initDenseElement(i, ObjectValue(*filtered));
  }
  to.setObject(*toArray);
  return true;
}

template <class T>
bool ListToArrayFilter(JSContext* cx, JS::Handle<JS::Value> from,
                       JS::MutableHandle<JS::Value> to) {
  using TargetT = typename T::Target;

  if (!from.isObject() || !from.toObject().is<ListObject>()) {
    to.set(from);
    return true;
  }

  JS::Rooted<ListObject*> fromList(cx, &from.toObject().as<ListObject>());
  uint32_t length = fromList->length();
  JS::Rooted<ArrayObject*> toArray(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!toArray) {
    return false;
  }

  toArray->ensureDenseInitializedLength(0, length);

  for (uint32_t i = 0; i < length; i++) {
    JS::Rooted<JS::Value> item(cx, fromList->get(i));
    JS::Rooted<TargetT*> req(cx, &item.toObject().as<TargetT>());
    JS::Rooted<T*> filtered(cx, T::create(cx, req));
    if (!filtered) {
      return false;
    }
    toArray->initDenseElement(i, ObjectValue(*filtered));
  }
  to.setObject(*toArray);
  return true;
}

template <class T, typename FilterT>
bool ShellModuleWrapperGetter(JSContext* cx, const JS::CallArgs& args,
                              const char* prop, FilterT filter) {
  using TargetT = typename T::Target;

  JS::Rooted<TargetT*> obj(cx, args.thisv().toObject().as<T>().get());
  JS::Rooted<JS::Value> rval(cx);
  if (!JS_GetProperty(cx, obj, prop, &rval)) {
    return false;
  }

  JS::Rooted<JS::Value> filtered(cx);
  if (!filter(cx, rval, &filtered)) {
    return false;
  }
  args.rval().set(filtered);
  return true;
}

#define DEFINE_GETTER_FUNCTIONS(CLASS, PROP, FILTER)                           \
  static bool Shell##CLASS##Wrapper_##PROP##Getter_impl(                       \
      JSContext* cx, const JS::CallArgs& args) {                               \
    return ShellModuleWrapperGetter<Shell##CLASS##Wrapper>(cx, args, #PROP,    \
                                                           FILTER);            \
  }                                                                            \
  static bool Shell##CLASS##Wrapper_##PROP##Getter(JSContext* cx,              \
                                                   unsigned argc, Value* vp) { \
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);                          \
    return CallNonGenericMethod<IsShell##CLASS##Wrapper,                       \
                                Shell##CLASS##Wrapper_##PROP##Getter_impl>(    \
        cx, args);                                                             \
  }

DEFINE_GETTER_FUNCTIONS(ModuleRequestObject, specifier, IdentFilter)

static const JSPropertySpec ShellModuleRequestObjectWrapper_accessors[] = {
    JS_PSG("specifier", ShellModuleRequestObjectWrapper_specifierGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ImportEntryObject, moduleRequest,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, importName, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, localName, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, lineNumber, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, columnNumber, IdentFilter)

static const JSPropertySpec ShellImportEntryObjectWrapper_accessors[] = {
    JS_PSG("moduleRequest", ShellImportEntryObjectWrapper_moduleRequestGetter,
           0),
    JS_PSG("importName", ShellImportEntryObjectWrapper_importNameGetter, 0),
    JS_PSG("localName", ShellImportEntryObjectWrapper_localNameGetter, 0),
    JS_PSG("lineNumber", ShellImportEntryObjectWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellImportEntryObjectWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ExportEntryObject, exportName, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, moduleRequest,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, importName, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, localName, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, lineNumber, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, columnNumber, IdentFilter)

static const JSPropertySpec ShellExportEntryObjectWrapper_accessors[] = {
    JS_PSG("exportName", ShellExportEntryObjectWrapper_exportNameGetter, 0),
    JS_PSG("moduleRequest", ShellExportEntryObjectWrapper_moduleRequestGetter,
           0),
    JS_PSG("importName", ShellExportEntryObjectWrapper_importNameGetter, 0),
    JS_PSG("localName", ShellExportEntryObjectWrapper_localNameGetter, 0),
    JS_PSG("lineNumber", ShellExportEntryObjectWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellExportEntryObjectWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, moduleRequest,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, lineNumber, IdentFilter)
DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, columnNumber, IdentFilter)

static const JSPropertySpec ShellRequestedModuleObjectWrapper_accessors[] = {
    JS_PSG("moduleRequest",
           ShellRequestedModuleObjectWrapper_moduleRequestGetter, 0),
    JS_PSG("lineNumber", ShellRequestedModuleObjectWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellRequestedModuleObjectWrapper_columnNumberGetter,
           0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ModuleObject, namespace, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, status, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, evaluationError, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, requestedModules,
                        ArrayFilter<ShellRequestedModuleObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, importEntries,
                        ArrayFilter<ShellImportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, localExportEntries,
                        ArrayFilter<ShellExportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, indirectExportEntries,
                        ArrayFilter<ShellExportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, starExportEntries,
                        ArrayFilter<ShellExportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, dfsIndex, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, dfsAncestorIndex, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, async, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, topLevelCapability, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, asyncEvaluatingPostOrder, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, asyncParentModules,
                        ListToArrayFilter<ShellModuleObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, pendingAsyncDependencies, IdentFilter)

static const JSPropertySpec ShellModuleObjectWrapper_accessors[] = {
    JS_PSG("namespace", ShellModuleObjectWrapper_namespaceGetter, 0),
    JS_PSG("status", ShellModuleObjectWrapper_statusGetter, 0),
    JS_PSG("evaluationError", ShellModuleObjectWrapper_evaluationErrorGetter,
           0),
    JS_PSG("requestedModules", ShellModuleObjectWrapper_requestedModulesGetter,
           0),
    JS_PSG("importEntries", ShellModuleObjectWrapper_importEntriesGetter, 0),
    JS_PSG("localExportEntries",
           ShellModuleObjectWrapper_localExportEntriesGetter, 0),
    JS_PSG("indirectExportEntries",
           ShellModuleObjectWrapper_indirectExportEntriesGetter, 0),
    JS_PSG("starExportEntries",
           ShellModuleObjectWrapper_starExportEntriesGetter, 0),
    JS_PSG("dfsIndex", ShellModuleObjectWrapper_dfsIndexGetter, 0),
    JS_PSG("dfsAncestorIndex", ShellModuleObjectWrapper_dfsAncestorIndexGetter,
           0),
    JS_PSG("async", ShellModuleObjectWrapper_asyncGetter, 0),
    JS_PSG("topLevelCapability",
           ShellModuleObjectWrapper_topLevelCapabilityGetter, 0),
    JS_PSG("asyncEvaluatingPostOrder",
           ShellModuleObjectWrapper_asyncEvaluatingPostOrderGetter, 0),
    JS_PSG("asyncParentModules",
           ShellModuleObjectWrapper_asyncParentModulesGetter, 0),
    JS_PSG("pendingAsyncDependencies",
           ShellModuleObjectWrapper_pendingAsyncDependenciesGetter, 0),
    JS_PS_END};

#undef DEFINE_GETTER_FUNCTIONS

template <class T, size_t ARGC, typename CheckArgsT, typename FilterT>
bool ShellModuleWrapperMethod(JSContext* cx, const JS::CallArgs& args,
                              const char* prop, CheckArgsT checkArgs,
                              FilterT filter) {
  using TargetT = typename T::Target;

  JS::Rooted<TargetT*> obj(cx, args.thisv().toObject().as<T>().get());
  JS::Rooted<JS::Value> methodVal(cx);
  if (!JS_GetProperty(cx, obj, prop, &methodVal)) {
    return false;
  }
  if (!methodVal.isObject() || !methodVal.toObject().is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "method property is not function");
    return false;
  }

  JS::Rooted<JSFunction*> method(cx, &methodVal.toObject().as<JSFunction>());
  if (!checkArgs(cx, args)) {
    return false;
  }

  FixedInvokeArgs<ARGC> invokeArgs(cx);
  for (size_t i = 0; i < ARGC; i++) {
    invokeArgs[i].set(args.get(i));
  }

  JS::Rooted<JS::Value> rval(cx);
  if (!JS::Call(cx, obj, method, invokeArgs, &rval)) {
    return false;
  }

  JS::Rooted<JS::Value> filtered(cx);
  if (!filter(cx, rval, &filtered)) {
    return false;
  }
  args.rval().set(filtered);
  return true;
}

#define DEFINE_METHOD_FUNCTIONS(CLASS, METHOD, ARGC, CHECK, FILTER)           \
  static bool Shell##CLASS##Wrapper_##METHOD##_impl(                          \
      JSContext* cx, const JS::CallArgs& args) {                              \
    return ShellModuleWrapperMethod<Shell##CLASS##Wrapper, ARGC>(             \
        cx, args, #METHOD, CHECK, FILTER);                                    \
  }                                                                           \
  static bool Shell##CLASS##Wrapper##_##METHOD(JSContext* cx, unsigned argc,  \
                                               Value* vp) {                   \
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);                         \
    return CallNonGenericMethod<Is##Shell##CLASS##Wrapper,                    \
                                Shell##CLASS##Wrapper_##METHOD##_impl>(cx,    \
                                                                       args); \
  }

static bool CheckNoArgs(JSContext* cx, const CallArgs& args) { return true; }

DEFINE_METHOD_FUNCTIONS(ModuleObject, declarationInstantiation, 0, CheckNoArgs,
                        IdentFilter)
DEFINE_METHOD_FUNCTIONS(ModuleObject, evaluation, 0, CheckNoArgs, IdentFilter)

static const JSFunctionSpec ShellModuleObjectWrapper_functions[] = {
    JS_FN("declarationInstantiation",
          ShellModuleObjectWrapper_declarationInstantiation, 0, 0),
    JS_FN("evaluation", ShellModuleObjectWrapper_evaluation, 0, 0), JS_FS_END};

#undef DEFINE_METHOD_FUNCTIONS

#define DEFINE_CREATE(CLASS, ACCESSORS, FUNCTIONS)                            \
  /* static */                                                                \
  Shell##CLASS##Wrapper* Shell##CLASS##Wrapper::create(                       \
      JSContext* cx, JS::Handle<CLASS*> obj) {                                \
    JS::Rooted<JSObject*> wrapper(cx, JS_NewObject(cx, &class_));             \
    if (!wrapper) {                                                           \
      return nullptr;                                                         \
    }                                                                         \
    if (!DefinePropertiesAndFunctions(cx, wrapper, ACCESSORS, FUNCTIONS)) {   \
      return nullptr;                                                         \
    }                                                                         \
    wrapper->as<Shell##CLASS##Wrapper>().initReservedSlot(TargetSlot,         \
                                                          ObjectValue(*obj)); \
    return &wrapper->as<Shell##CLASS##Wrapper>();                             \
  }

DEFINE_CREATE(ModuleRequestObject, ShellModuleRequestObjectWrapper_accessors,
              nullptr)
DEFINE_CREATE(ImportEntryObject, ShellImportEntryObjectWrapper_accessors,
              nullptr)
DEFINE_CREATE(ExportEntryObject, ShellExportEntryObjectWrapper_accessors,
              nullptr)
DEFINE_CREATE(RequestedModuleObject,
              ShellRequestedModuleObjectWrapper_accessors, nullptr)
DEFINE_CREATE(ModuleObject, ShellModuleObjectWrapper_accessors,
              ShellModuleObjectWrapper_functions)

#undef DEFINE_CREATE
