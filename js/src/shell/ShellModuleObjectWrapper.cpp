/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4
 * -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/ShellModuleObjectWrapper.h"

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"

#include "jsapi.h"  // JS_GetProperty, JS::Call, JS_NewPlainObject, JS_DefineProperty

#include "builtin/ModuleObject.h"     // js::ModuleObject
#include "js/CallAndConstruct.h"      // JS::Call
#include "js/CallArgs.h"              // JS::CallArgs
#include "js/CallNonGenericMethod.h"  // CallNonGenericMethod
#include "js/Class.h"                 // JSClass, JSCLASS_*
#include "js/ColumnNumber.h"          // JS::ColumnNumberOneOrigin
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

using mozilla::Span;

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

#define DEFINE_NATIVE_CLASS_IMPL(CLASS)                                     \
  CLASS* Shell##CLASS##Wrapper::get() {                                     \
    return static_cast<CLASS*>(getReservedSlot(TargetSlot).toPrivate());    \
  }                                                                         \
  /* static */ const JSClass Shell##CLASS##Wrapper::class_ = {              \
      "Shell" #CLASS "Wrapper",                                             \
      JSCLASS_HAS_RESERVED_SLOTS(Shell##CLASS##Wrapper::SlotCount)};        \
  MOZ_ALWAYS_INLINE bool IsShell##CLASS##Wrapper(JS::Handle<JS::Value> v) { \
    return v.isObject() && v.toObject().is<Shell##CLASS##Wrapper>();        \
  }

#define DEFINE_NATIVE_CLASS(CLASS)                                    \
  class Shell##CLASS##Wrapper : public js::NativeObject {             \
   public:                                                            \
    using Target = CLASS;                                             \
    enum ModuleSlot { OwnerSlot = 0, TargetSlot, SlotCount };         \
    static const JSClass class_;                                      \
    static Shell##CLASS##Wrapper* create(JSContext* cx,               \
                                         JS::Handle<JSObject*> owner, \
                                         CLASS* obj);                 \
    CLASS* get();                                                     \
  };                                                                  \
  DEFINE_NATIVE_CLASS_IMPL(CLASS)

DEFINE_CLASS(ModuleRequestObject)
DEFINE_NATIVE_CLASS(ImportEntry)
DEFINE_NATIVE_CLASS(ExportEntry)
DEFINE_NATIVE_CLASS(RequestedModule)
// NOTE: We don't need wrapper for IndirectBindingMap and ModuleNamespaceObject
DEFINE_CLASS_IMPL(ModuleObject)

#undef DEFINE_CLASS
#undef DEFINE_CLASS_IMPL
#undef DEFINE_NATIVE_CLASS
#undef DEFINE_NATIVE_CLASS_IMPL

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

static Value ColumnNumberOneOriginValue(JS::ColumnNumberOneOrigin x) {
  uint32_t column = x.oneOriginValue();
  MOZ_ASSERT(column <= INT32_MAX);
  return Int32Value(column);
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
  JS::Rooted<T*> wrapper(cx, &args.thisv().toObject().as<T>());
  JS::Rooted<JS::Value> raw(cx, rawGetter(wrapper->get()));

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

template <class T>
bool SpanToArrayFilter(JSContext* cx, JS::Handle<JSObject*> owner,
                       Span<const typename T::Target> from,
                       JS::MutableHandle<JS::Value> to) {
  size_t length = from.Length();
  JS::Rooted<ArrayObject*> toArray(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!toArray) {
    return false;
  }

  toArray->ensureDenseInitializedLength(0, length);

  for (uint32_t i = 0; i < length; i++) {
    auto* element = const_cast<typename T::Target*>(&from[i]);
    JS::Rooted<T*> filtered(cx, T::create(cx, owner, element));
    if (!filtered) {
      return false;
    }
    toArray->initDenseElement(i, ObjectValue(*filtered));
  }

  to.setObject(*toArray);
  return true;
}

template <class T, typename RawGetterT, typename FilterT>
bool ShellModuleNativeWrapperGetter(JSContext* cx, const JS::CallArgs& args,
                                    RawGetterT rawGetter, FilterT filter) {
  JS::Rooted<T*> wrapper(cx, &args.thisv().toObject().as<T>());
  JS::Rooted<typename T::Target*> owner(cx, wrapper->get());

  JS::Rooted<JS::Value> filtered(cx);
  if (!filter(cx, owner, rawGetter(owner), &filtered)) {
    return false;
  }

  args.rval().set(filtered);
  return true;
}

#define DEFINE_NATIVE_GETTER_FUNCTIONS(CLASS, PROP, FILTER)                    \
  static auto Shell##CLASS##Wrapper_##PROP##Getter_raw(CLASS* obj) {           \
    return obj->PROP();                                                        \
  }                                                                            \
  static bool Shell##CLASS##Wrapper_##PROP##Getter_impl(                       \
      JSContext* cx, const JS::CallArgs& args) {                               \
    return ShellModuleNativeWrapperGetter<Shell##CLASS##Wrapper>(              \
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

DEFINE_GETTER_FUNCTIONS(ImportEntry, moduleRequest, ObjectOrNullValue,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ImportEntry, importName, StringOrNullValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntry, localName, StringValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntry, lineNumber, Uint32Value, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ImportEntry, columnNumber, ColumnNumberOneOriginValue,
                        IdentFilter)

static const JSPropertySpec ShellImportEntryWrapper_accessors[] = {
    JS_PSG("moduleRequest", ShellImportEntryWrapper_moduleRequestGetter, 0),
    JS_PSG("importName", ShellImportEntryWrapper_importNameGetter, 0),
    JS_PSG("localName", ShellImportEntryWrapper_localNameGetter, 0),
    JS_PSG("lineNumber", ShellImportEntryWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellImportEntryWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ExportEntry, exportName, StringOrNullValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntry, moduleRequest, ObjectOrNullValue,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(ExportEntry, importName, StringOrNullValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntry, localName, StringOrNullValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntry, lineNumber, Uint32Value, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ExportEntry, columnNumber, ColumnNumberOneOriginValue,
                        IdentFilter)

static const JSPropertySpec ShellExportEntryWrapper_accessors[] = {
    JS_PSG("exportName", ShellExportEntryWrapper_exportNameGetter, 0),
    JS_PSG("moduleRequest", ShellExportEntryWrapper_moduleRequestGetter, 0),
    JS_PSG("importName", ShellExportEntryWrapper_importNameGetter, 0),
    JS_PSG("localName", ShellExportEntryWrapper_localNameGetter, 0),
    JS_PSG("lineNumber", ShellExportEntryWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellExportEntryWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(RequestedModule, moduleRequest, ObjectOrNullValue,
                        SingleFilter<ShellModuleRequestObjectWrapper>)
DEFINE_GETTER_FUNCTIONS(RequestedModule, lineNumber, Uint32Value, IdentFilter)
DEFINE_GETTER_FUNCTIONS(RequestedModule, columnNumber,
                        ColumnNumberOneOriginValue, IdentFilter)

static const JSPropertySpec ShellRequestedModuleWrapper_accessors[] = {
    JS_PSG("moduleRequest", ShellRequestedModuleWrapper_moduleRequestGetter, 0),
    JS_PSG("lineNumber", ShellRequestedModuleWrapper_lineNumberGetter, 0),
    JS_PSG("columnNumber", ShellRequestedModuleWrapper_columnNumberGetter, 0),
    JS_PS_END};

DEFINE_GETTER_FUNCTIONS(ModuleObject, namespace_, ObjectOrNullValue,
                        IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, status, StatusValue, IdentFilter)
DEFINE_GETTER_FUNCTIONS(ModuleObject, maybeEvaluationError, Value, IdentFilter)
DEFINE_NATIVE_GETTER_FUNCTIONS(ModuleObject, requestedModules,
                               SpanToArrayFilter<ShellRequestedModuleWrapper>)
DEFINE_NATIVE_GETTER_FUNCTIONS(ModuleObject, importEntries,
                               SpanToArrayFilter<ShellImportEntryWrapper>)
DEFINE_NATIVE_GETTER_FUNCTIONS(ModuleObject, localExportEntries,
                               SpanToArrayFilter<ShellExportEntryWrapper>)
DEFINE_NATIVE_GETTER_FUNCTIONS(ModuleObject, indirectExportEntries,
                               SpanToArrayFilter<ShellExportEntryWrapper>)
DEFINE_NATIVE_GETTER_FUNCTIONS(ModuleObject, starExportEntries,
                               SpanToArrayFilter<ShellExportEntryWrapper>)
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
#undef DEFINE_NATIVE_GETTER_FUNCTIONS

#define DEFINE_CREATE(CLASS, ACCESSORS, FUNCTIONS)                      \
  /* static */                                                          \
  Shell##CLASS##Wrapper* Shell##CLASS##Wrapper::create(                 \
      JSContext* cx, JS::Handle<CLASS*> target) {                       \
    JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &class_));           \
    if (!obj) {                                                         \
      return nullptr;                                                   \
    }                                                                   \
    if (!DefinePropertiesAndFunctions(cx, obj, ACCESSORS, FUNCTIONS)) { \
      return nullptr;                                                   \
    }                                                                   \
    auto* wrapper = &obj->as<Shell##CLASS##Wrapper>();                  \
    wrapper->initReservedSlot(TargetSlot, ObjectValue(*target));        \
    return wrapper;                                                     \
  }

#define DEFINE_NATIVE_CREATE(CLASS, ACCESSORS, FUNCTIONS)               \
  /* static */                                                          \
  Shell##CLASS##Wrapper* Shell##CLASS##Wrapper::create(                 \
      JSContext* cx, JS::Handle<JSObject*> owner, CLASS* target) {      \
    JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &class_));           \
    if (!obj) {                                                         \
      return nullptr;                                                   \
    }                                                                   \
    if (!DefinePropertiesAndFunctions(cx, obj, ACCESSORS, FUNCTIONS)) { \
      return nullptr;                                                   \
    }                                                                   \
    auto* wrapper = &obj->as<Shell##CLASS##Wrapper>();                  \
    wrapper->initReservedSlot(OwnerSlot, ObjectValue(*owner));          \
    wrapper->initReservedSlot(TargetSlot, PrivateValue(target));        \
    return wrapper;                                                     \
  }

DEFINE_CREATE(ModuleRequestObject, ShellModuleRequestObjectWrapper_accessors,
              nullptr)
DEFINE_NATIVE_CREATE(ImportEntry, ShellImportEntryWrapper_accessors, nullptr)
DEFINE_NATIVE_CREATE(ExportEntry, ShellExportEntryWrapper_accessors, nullptr)
DEFINE_NATIVE_CREATE(RequestedModule, ShellRequestedModuleWrapper_accessors,
                     nullptr)
DEFINE_CREATE(ModuleObject, ShellModuleObjectWrapper_accessors, nullptr)

#undef DEFINE_CREATE
#undef DEFINE_NATIVE_CREATE
