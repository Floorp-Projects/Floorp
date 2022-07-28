/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4
 * -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/ShellModuleObjectWrapper.h"

#include "mozilla/Maybe.h"

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

static Value StringOrNullValue(JSString* maybeString) {
  if (!maybeString) {
    return NullValue();
  }

  return StringValue(maybeString);
}

static Value Uint32Value(uint32_t x) {
  MOZ_ASSERT(x <= INT32_MAX);
  return Int32Value(x);
}

static Value Uint32OrUndefinedValue(mozilla::Maybe<uint32_t> x) {
  if (x.isNothing()) {
    return UndefinedValue();
  }

  return Uint32Value(x.value());
}

static Value StatusValue(ModuleStatus status) {
  return Int32Value(int32_t(status));
}

static Value ObjectOrUndefinedValue(JSObject* object) {
  if (!object) {
    return UndefinedValue();
  }

  return ObjectValue(*object);
}

template <class T, typename RawGetterT, typename FilterT>
bool ShellModuleWrapperGetter(JSContext* cx, const JS::CallArgs& args,
                              RawGetterT rawGetter, FilterT filter) {
  using TargetT = typename T::Target;

  JS::Rooted<TargetT*> obj(cx, args.thisv().toObject().as<T>().get());
  JS::Rooted<JS::Value> raw(cx, rawGetter(obj));

  JS::Rooted<JS::Value> filtered(cx);
  if (!filter(cx, raw, &filtered)) {
    return false;
  }

  args.rval().set(filtered);
  return true;
}

#define DEFINE_GETTER_FUNCTIONS(CLASS, PROP, TO_VALUE, FILTER)                 \
  static Value Shell##CLASS##Wrapper_##PROP##Getter_raw(CLASS* obj) {          \
    return TO_VALUE(obj->PROP());                                              \
  }                                                                            \
  static bool Shell##CLASS##Wrapper_##PROP##Getter_impl(                       \
      JSContext* cx, const JS::CallArgs& args) {                               \
    return ShellModuleWrapperGetter<Shell##CLASS##Wrapper>(                    \
        cx, args, Shell##CLASS##Wrapper_##PROP##Getter_raw, FILTER);           \
  }                                                                            \
  static bool Shell##CLASS##Wrapper_##PROP##Getter(JSContext* cx,              \
                                                   unsigned argc, Value* vp) { \
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);                          \
    return CallNonGenericMethod<IsShell##CLASS##Wrapper,                       \
                                Shell##CLASS##Wrapper_##PROP##Getter_impl>(    \
        cx, args);                                                             \
  }

DEFINE_GETTER_FUNCTIONS(ModuleRequestObject, specifier, StringOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleRequestObject, assertions, ObjectOrNullValue,
                        IdentFilter)

static const JSPropertySpec ShellModuleRequestObjectWrapper_accessors[] = {
    JS_PSG("specifier", ShellModuleRequestObjectWrapper_specifierGetter, 0),
    JS_PSG("assertions", ShellModuleRequestObjectWrapper_assertionsGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ImportEntryObject, moduleRequest, ObjectOrNullValue,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, importName, StringOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, localName, StringValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, lineNumber, Uint32Value, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, columnNumber, Uint32Value,
                        IdentFilter)

static const JSPropertySpec ShellImportEntryObjectWrapper_accessors[] = {
    JS_PSG("moduleRequest", ShellImportEntryObjectWrapper_moduleRequestGetter,
           0),
    JS_PSG("importName", ShellImportEntryObjectWrapper_importNameGetter, 0),
    JS_PSG("localName", ShellImportEntryObjectWrapper_localNameGetter, 0),
    JS_PSG("lineNumber", ShellImportEntryObjectWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellImportEntryObjectWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ExportEntryObject, exportName, StringOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, moduleRequest, ObjectOrNullValue,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, importName, StringOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, localName, StringOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, lineNumber, Uint32Value, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, columnNumber, Uint32Value,
                        IdentFilter)

static const JSPropertySpec ShellExportEntryObjectWrapper_accessors[] = {
    JS_PSG("exportName", ShellExportEntryObjectWrapper_exportNameGetter, 0),
    JS_PSG("moduleRequest", ShellExportEntryObjectWrapper_moduleRequestGetter,
           0),
    JS_PSG("importName", ShellExportEntryObjectWrapper_importNameGetter, 0),
    JS_PSG("localName", ShellExportEntryObjectWrapper_localNameGetter, 0),
    JS_PSG("lineNumber", ShellExportEntryObjectWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellExportEntryObjectWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, moduleRequest, ObjectOrNullValue,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, lineNumber, Uint32Value,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, columnNumber, Uint32Value,
                        IdentFilter)

static const JSPropertySpec ShellRequestedModuleObjectWrapper_accessors[] = {
    JS_PSG("moduleRequest",
           ShellRequestedModuleObjectWrapper_moduleRequestGetter, 0),
    JS_PSG("lineNumber", ShellRequestedModuleObjectWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellRequestedModuleObjectWrapper_columnNumberGetter,
           0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ModuleObject, namespace_, ObjectOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, status, StatusValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybeEvaluationError, Value, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, requestedModules, ObjectValue,
                        ArrayFilter<ShellRequestedModuleObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, importEntries, ObjectValue,
                        ArrayFilter<ShellImportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, localExportEntries, ObjectValue,
                        ArrayFilter<ShellExportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, indirectExportEntries, ObjectValue,
                        ArrayFilter<ShellExportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, starExportEntries, ObjectValue,
                        ArrayFilter<ShellExportEntryObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybeDfsIndex, Uint32OrUndefinedValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybeDfsAncestorIndex,
                        Uint32OrUndefinedValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, hasTopLevelAwait, BooleanValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybeTopLevelCapability,
                        ObjectOrUndefinedValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, isAsyncEvaluating, BooleanValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybeAsyncEvaluatingPostOrder,
                        Uint32OrUndefinedValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, asyncParentModules, ObjectOrNullValue,
                        ListToArrayFilter<ShellModuleObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybePendingAsyncDependencies,
                        Uint32OrUndefinedValue, IdentFilter)

static const JSPropertySpec ShellModuleObjectWrapper_accessors[] = {
    JS_PSG("namespace", ShellModuleObjectWrapper_namespace_Getter, 0),
    JS_PSG("status", ShellModuleObjectWrapper_statusGetter, 0),
    JS_PSG("evaluationError",
           ShellModuleObjectWrapper_maybeEvaluationErrorGetter, 0),
    JS_PSG("requestedModules", ShellModuleObjectWrapper_requestedModulesGetter,
           0),
    JS_PSG("importEntries", ShellModuleObjectWrapper_importEntriesGetter, 0),
    JS_PSG("localExportEntries",
           ShellModuleObjectWrapper_localExportEntriesGetter, 0),
    JS_PSG("indirectExportEntries",
           ShellModuleObjectWrapper_indirectExportEntriesGetter, 0),
    JS_PSG("starExportEntries",
           ShellModuleObjectWrapper_starExportEntriesGetter, 0),
    JS_PSG("dfsIndex", ShellModuleObjectWrapper_maybeDfsIndexGetter, 0),
    JS_PSG("dfsAncestorIndex",
           ShellModuleObjectWrapper_maybeDfsAncestorIndexGetter, 0),
    JS_PSG("hasTopLevelAwait", ShellModuleObjectWrapper_hasTopLevelAwaitGetter,
           0),
    JS_PSG("topLevelCapability",
           ShellModuleObjectWrapper_maybeTopLevelCapabilityGetter, 0),
    JS_PSG("isAsyncEvaluating",
           ShellModuleObjectWrapper_isAsyncEvaluatingGetter, 0),
    JS_PSG("asyncEvaluatingPostOrder",
           ShellModuleObjectWrapper_maybeAsyncEvaluatingPostOrderGetter, 0),
    JS_PSG("asyncParentModules",
           ShellModuleObjectWrapper_asyncParentModulesGetter, 0),
    JS_PSG("pendingAsyncDependencies",
           ShellModuleObjectWrapper_maybePendingAsyncDependenciesGetter, 0),
    JS_PS_END};

#undef DEFINE_GETTER_FUNCTIONS

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
DEFINE_CREATE(ModuleObject, ShellModuleObjectWrapper_accessors, nullptr)

#undef DEFINE_CREATE
