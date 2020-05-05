/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/ModuleObject.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/EnumSet.h"
#include "mozilla/ScopeExit.h"

#include "builtin/Promise.h"
#include "builtin/SelfHostingDefines.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "gc/FreeOp.h"
#include "gc/Policy.h"
#include "gc/Tracer.h"
#include "js/Modules.h"  // JS::GetModulePrivate, JS::ModuleDynamicImportHook
#include "js/PropertySpec.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/EqualityOperations.h"  // js::SameValue
#include "vm/ModuleBuilder.h"       // js::ModuleBuilder
#include "vm/PlainObject.h"         // js::PlainObject
#include "vm/PromiseObject.h"       // js::PromiseObject
#include "vm/SelfHosting.h"

#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;

static_assert(MODULE_STATUS_UNINSTANTIATED < MODULE_STATUS_INSTANTIATING &&
                  MODULE_STATUS_INSTANTIATING < MODULE_STATUS_INSTANTIATED &&
                  MODULE_STATUS_INSTANTIATED < MODULE_STATUS_EVALUATED &&
                  MODULE_STATUS_EVALUATED < MODULE_STATUS_EVALUATED_ERROR,
              "Module statuses are ordered incorrectly");

template <typename T, Value ValueGetter(const T* obj)>
static bool ModuleValueGetterImpl(JSContext* cx, const CallArgs& args) {
  args.rval().set(ValueGetter(&args.thisv().toObject().as<T>()));
  return true;
}

template <typename T, Value ValueGetter(const T* obj)>
static bool ModuleValueGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<T::isInstance,
                              ModuleValueGetterImpl<T, ValueGetter>>(cx, args);
}

#define DEFINE_GETTER_FUNCTIONS(cls, name, slot)                              \
  static Value cls##_##name##Value(const cls* obj) {                          \
    return obj->getReservedSlot(cls::slot);                                   \
  }                                                                           \
                                                                              \
  static bool cls##_##name##Getter(JSContext* cx, unsigned argc, Value* vp) { \
    return ModuleValueGetter<cls, cls##_##name##Value>(cx, argc, vp);         \
  }

#define DEFINE_ATOM_ACCESSOR_METHOD(cls, name) \
  JSAtom* cls::name() const {                  \
    Value value = cls##_##name##Value(this);   \
    return &value.toString()->asAtom();        \
  }

#define DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(cls, name) \
  JSAtom* cls::name() const {                          \
    Value value = cls##_##name##Value(this);           \
    if (value.isNull()) return nullptr;                \
    return &value.toString()->asAtom();                \
  }

#define DEFINE_UINT32_ACCESSOR_METHOD(cls, name) \
  uint32_t cls::name() const {                   \
    Value value = cls##_##name##Value(this);     \
    MOZ_ASSERT(value.toNumber() >= 0);           \
    if (value.isInt32()) return value.toInt32(); \
    return JS::ToUint32(value.toDouble());       \
  }

///////////////////////////////////////////////////////////////////////////
// ImportEntryObject

/* static */ const JSClass ImportEntryObject::class_ = {
    "ImportEntry", JSCLASS_HAS_RESERVED_SLOTS(ImportEntryObject::SlotCount)};

DEFINE_GETTER_FUNCTIONS(ImportEntryObject, moduleRequest, ModuleRequestSlot)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, importName, ImportNameSlot)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, localName, LocalNameSlot)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, lineNumber, LineNumberSlot)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, columnNumber, ColumnNumberSlot)

DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, moduleRequest)
DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, importName)
DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, localName)
DEFINE_UINT32_ACCESSOR_METHOD(ImportEntryObject, lineNumber)
DEFINE_UINT32_ACCESSOR_METHOD(ImportEntryObject, columnNumber)

/* static */
bool ImportEntryObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ImportEntryObject>();
}

/* static */
bool GlobalObject::initImportEntryProto(JSContext* cx,
                                        Handle<GlobalObject*> global) {
  static const JSPropertySpec protoAccessors[] = {
      JS_PSG("moduleRequest", ImportEntryObject_moduleRequestGetter, 0),
      JS_PSG("importName", ImportEntryObject_importNameGetter, 0),
      JS_PSG("localName", ImportEntryObject_localNameGetter, 0),
      JS_PSG("lineNumber", ImportEntryObject_lineNumberGetter, 0),
      JS_PSG("columnNumber", ImportEntryObject_columnNumberGetter, 0),
      JS_PS_END};

  RootedObject proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto) {
    return false;
  }

  if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr)) {
    return false;
  }

  global->initReservedSlot(IMPORT_ENTRY_PROTO, ObjectValue(*proto));
  return true;
}

/* static */
ImportEntryObject* ImportEntryObject::create(
    JSContext* cx, HandleAtom moduleRequest, HandleAtom importName,
    HandleAtom localName, uint32_t lineNumber, uint32_t columnNumber) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateImportEntryPrototype(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  ImportEntryObject* self =
      NewObjectWithGivenProto<ImportEntryObject>(cx, proto);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ModuleRequestSlot, StringValue(moduleRequest));
  self->initReservedSlot(ImportNameSlot, StringValue(importName));
  self->initReservedSlot(LocalNameSlot, StringValue(localName));
  self->initReservedSlot(LineNumberSlot, NumberValue(lineNumber));
  self->initReservedSlot(ColumnNumberSlot, NumberValue(columnNumber));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// ExportEntryObject

/* static */ const JSClass ExportEntryObject::class_ = {
    "ExportEntry", JSCLASS_HAS_RESERVED_SLOTS(ExportEntryObject::SlotCount)};

DEFINE_GETTER_FUNCTIONS(ExportEntryObject, exportName, ExportNameSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, moduleRequest, ModuleRequestSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, importName, ImportNameSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, localName, LocalNameSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, lineNumber, LineNumberSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, columnNumber, ColumnNumberSlot)

DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, exportName)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, moduleRequest)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, importName)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, localName)
DEFINE_UINT32_ACCESSOR_METHOD(ExportEntryObject, lineNumber)
DEFINE_UINT32_ACCESSOR_METHOD(ExportEntryObject, columnNumber)

/* static */
bool ExportEntryObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ExportEntryObject>();
}

/* static */
bool GlobalObject::initExportEntryProto(JSContext* cx,
                                        Handle<GlobalObject*> global) {
  static const JSPropertySpec protoAccessors[] = {
      JS_PSG("exportName", ExportEntryObject_exportNameGetter, 0),
      JS_PSG("moduleRequest", ExportEntryObject_moduleRequestGetter, 0),
      JS_PSG("importName", ExportEntryObject_importNameGetter, 0),
      JS_PSG("localName", ExportEntryObject_localNameGetter, 0),
      JS_PSG("lineNumber", ExportEntryObject_lineNumberGetter, 0),
      JS_PSG("columnNumber", ExportEntryObject_columnNumberGetter, 0),
      JS_PS_END};

  RootedObject proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto) {
    return false;
  }

  if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr)) {
    return false;
  }

  global->initReservedSlot(EXPORT_ENTRY_PROTO, ObjectValue(*proto));
  return true;
}

static Value StringOrNullValue(JSString* maybeString) {
  return maybeString ? StringValue(maybeString) : NullValue();
}

/* static */
ExportEntryObject* ExportEntryObject::create(
    JSContext* cx, HandleAtom maybeExportName, HandleAtom maybeModuleRequest,
    HandleAtom maybeImportName, HandleAtom maybeLocalName, uint32_t lineNumber,
    uint32_t columnNumber) {
  // Line and column numbers are optional for export entries since direct
  // entries are checked at parse time.

  RootedObject proto(
      cx, GlobalObject::getOrCreateExportEntryPrototype(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  ExportEntryObject* self =
      NewObjectWithGivenProto<ExportEntryObject>(cx, proto);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ExportNameSlot, StringOrNullValue(maybeExportName));
  self->initReservedSlot(ModuleRequestSlot,
                         StringOrNullValue(maybeModuleRequest));
  self->initReservedSlot(ImportNameSlot, StringOrNullValue(maybeImportName));
  self->initReservedSlot(LocalNameSlot, StringOrNullValue(maybeLocalName));
  self->initReservedSlot(LineNumberSlot, NumberValue(lineNumber));
  self->initReservedSlot(ColumnNumberSlot, NumberValue(columnNumber));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// RequestedModuleObject

/* static */ const JSClass RequestedModuleObject::class_ = {
    "RequestedModule",
    JSCLASS_HAS_RESERVED_SLOTS(RequestedModuleObject::SlotCount)};

DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, moduleSpecifier,
                        ModuleSpecifierSlot)
DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, lineNumber, LineNumberSlot)
DEFINE_GETTER_FUNCTIONS(RequestedModuleObject, columnNumber, ColumnNumberSlot)

DEFINE_ATOM_ACCESSOR_METHOD(RequestedModuleObject, moduleSpecifier)
DEFINE_UINT32_ACCESSOR_METHOD(RequestedModuleObject, lineNumber)
DEFINE_UINT32_ACCESSOR_METHOD(RequestedModuleObject, columnNumber)

/* static */
bool RequestedModuleObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<RequestedModuleObject>();
}

/* static */
bool GlobalObject::initRequestedModuleProto(JSContext* cx,
                                            Handle<GlobalObject*> global) {
  static const JSPropertySpec protoAccessors[] = {
      JS_PSG("moduleSpecifier", RequestedModuleObject_moduleSpecifierGetter, 0),
      JS_PSG("lineNumber", RequestedModuleObject_lineNumberGetter, 0),
      JS_PSG("columnNumber", RequestedModuleObject_columnNumberGetter, 0),
      JS_PS_END};

  RootedObject proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto) {
    return false;
  }

  if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr)) {
    return false;
  }

  global->initReservedSlot(REQUESTED_MODULE_PROTO, ObjectValue(*proto));
  return true;
}

/* static */
RequestedModuleObject* RequestedModuleObject::create(JSContext* cx,
                                                     HandleAtom moduleSpecifier,
                                                     uint32_t lineNumber,
                                                     uint32_t columnNumber) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateRequestedModulePrototype(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  RequestedModuleObject* self =
      NewObjectWithGivenProto<RequestedModuleObject>(cx, proto);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ModuleSpecifierSlot, StringValue(moduleSpecifier));
  self->initReservedSlot(LineNumberSlot, NumberValue(lineNumber));
  self->initReservedSlot(ColumnNumberSlot, NumberValue(columnNumber));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// IndirectBindingMap

IndirectBindingMap::Binding::Binding(ModuleEnvironmentObject* environment,
                                     Shape* shape)
    : environment(environment), shape(shape) {}

void IndirectBindingMap::trace(JSTracer* trc) {
  if (!map_) {
    return;
  }

  for (Map::Enum e(*map_); !e.empty(); e.popFront()) {
    Binding& b = e.front().value();
    TraceEdge(trc, &b.environment, "module bindings environment");
    TraceEdge(trc, &b.shape, "module bindings shape");
    mozilla::DebugOnly<jsid> prev(e.front().key());
    TraceEdge(trc, &e.front().mutableKey(), "module bindings binding name");
    MOZ_ASSERT(e.front().key() == prev);
  }
}

bool IndirectBindingMap::put(JSContext* cx, HandleId name,
                             HandleModuleEnvironmentObject environment,
                             HandleId localName) {
  // This object might have been allocated on the background parsing thread in
  // different zone to the final module. Lazily allocate the map so we don't
  // have to switch its zone when merging realms.
  if (!map_) {
    MOZ_ASSERT(!cx->zone()->createdForHelperThread());
    map_.emplace(cx->zone());
  }

  RootedShape shape(cx, environment->lookup(cx, localName));
  MOZ_ASSERT(shape);
  if (!map_->put(name, Binding(environment, shape))) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool IndirectBindingMap::lookup(jsid name, ModuleEnvironmentObject** envOut,
                                Shape** shapeOut) const {
  if (!map_) {
    return false;
  }

  auto ptr = map_->lookup(name);
  if (!ptr) {
    return false;
  }

  const Binding& binding = ptr->value();
  MOZ_ASSERT(binding.environment);
  MOZ_ASSERT(!binding.environment->inDictionaryMode());
  MOZ_ASSERT(binding.environment->containsPure(binding.shape));
  *envOut = binding.environment;
  *shapeOut = binding.shape;
  return true;
}

///////////////////////////////////////////////////////////////////////////
// ModuleNamespaceObject

/* static */
const ModuleNamespaceObject::ProxyHandler ModuleNamespaceObject::proxyHandler;

/* static */
bool ModuleNamespaceObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ModuleNamespaceObject>();
}

/* static */
ModuleNamespaceObject* ModuleNamespaceObject::create(
    JSContext* cx, HandleModuleObject module, HandleObject exports,
    UniquePtr<IndirectBindingMap> bindings) {
  RootedValue priv(cx, ObjectValue(*module));
  ProxyOptions options;
  options.setLazyProto(true);
  Rooted<UniquePtr<IndirectBindingMap>> rootedBindings(cx, std::move(bindings));
  RootedObject object(
      cx, NewSingletonProxyObject(cx, &proxyHandler, priv, nullptr, options));
  if (!object) {
    return nullptr;
  }

  SetProxyReservedSlot(object, ExportsSlot, ObjectValue(*exports));
  SetProxyReservedSlot(object, BindingsSlot,
                       PrivateValue(rootedBindings.release()));
  AddCellMemory(object, sizeof(IndirectBindingMap),
                MemoryUse::ModuleBindingMap);

  return &object->as<ModuleNamespaceObject>();
}

ModuleObject& ModuleNamespaceObject::module() {
  return GetProxyPrivate(this).toObject().as<ModuleObject>();
}

JSObject& ModuleNamespaceObject::exports() {
  return GetProxyReservedSlot(this, ExportsSlot).toObject();
}

IndirectBindingMap& ModuleNamespaceObject::bindings() {
  Value value = GetProxyReservedSlot(this, BindingsSlot);
  auto bindings = static_cast<IndirectBindingMap*>(value.toPrivate());
  MOZ_ASSERT(bindings);
  return *bindings;
}

bool ModuleNamespaceObject::hasBindings() const {
  // Import bindings may not be present if we hit OOM in initialization.
  return !GetProxyReservedSlot(this, BindingsSlot).isUndefined();
}

bool ModuleNamespaceObject::addBinding(JSContext* cx, HandleAtom exportedName,
                                       HandleModuleObject targetModule,
                                       HandleAtom localName) {
  RootedModuleEnvironmentObject environment(
      cx, &targetModule->initialEnvironment());
  RootedId exportedNameId(cx, AtomToId(exportedName));
  RootedId localNameId(cx, AtomToId(localName));
  return bindings().put(cx, exportedNameId, environment, localNameId);
}

const char ModuleNamespaceObject::ProxyHandler::family = 0;

ModuleNamespaceObject::ProxyHandler::ProxyHandler()
    : BaseProxyHandler(&family, false) {}

bool ModuleNamespaceObject::ProxyHandler::getPrototype(
    JSContext* cx, HandleObject proxy, MutableHandleObject protop) const {
  protop.set(nullptr);
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::setPrototype(
    JSContext* cx, HandleObject proxy, HandleObject proto,
    ObjectOpResult& result) const {
  if (!proto) {
    return result.succeed();
  }
  return result.failCantSetProto();
}

bool ModuleNamespaceObject::ProxyHandler::getPrototypeIfOrdinary(
    JSContext* cx, HandleObject proxy, bool* isOrdinary,
    MutableHandleObject protop) const {
  *isOrdinary = false;
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::setImmutablePrototype(
    JSContext* cx, HandleObject proxy, bool* succeeded) const {
  *succeeded = true;
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::isExtensible(JSContext* cx,
                                                       HandleObject proxy,
                                                       bool* extensible) const {
  *extensible = false;
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::preventExtensions(
    JSContext* cx, HandleObject proxy, ObjectOpResult& result) const {
  result.succeed();
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::getOwnPropertyDescriptor(
    JSContext* cx, HandleObject proxy, HandleId id,
    MutableHandle<PropertyDescriptor> desc) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (JSID_IS_SYMBOL(id)) {
    if (JSID_TO_SYMBOL(id) == cx->wellKnownSymbols().toStringTag) {
      RootedValue value(cx, StringValue(cx->names().Module));
      desc.object().set(proxy);
      desc.setWritable(false);
      desc.setEnumerable(false);
      desc.setConfigurable(false);
      desc.setValue(value);
      return true;
    }

    return true;
  }

  const IndirectBindingMap& bindings = ns->bindings();
  ModuleEnvironmentObject* env;
  Shape* shape;
  if (!bindings.lookup(id, &env, &shape)) {
    return true;
  }

  RootedValue value(cx, env->getSlot(shape->slot()));
  if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  desc.object().set(env);
  desc.setConfigurable(false);
  desc.setEnumerable(true);
  desc.setValue(value);
  return true;
}

static bool ValidatePropertyDescriptor(
    JSContext* cx, Handle<PropertyDescriptor> desc, bool expectedWritable,
    bool expectedEnumerable, bool expectedConfigurable,
    HandleValue expectedValue, ObjectOpResult& result) {
  if (desc.isAccessorDescriptor()) {
    return result.fail(JSMSG_CANT_REDEFINE_PROP);
  }

  if (desc.hasWritable() && desc.writable() != expectedWritable) {
    return result.fail(JSMSG_CANT_REDEFINE_PROP);
  }

  if (desc.hasEnumerable() && desc.enumerable() != expectedEnumerable) {
    return result.fail(JSMSG_CANT_REDEFINE_PROP);
  }

  if (desc.hasConfigurable() && desc.configurable() != expectedConfigurable) {
    return result.fail(JSMSG_CANT_REDEFINE_PROP);
  }

  if (desc.hasValue()) {
    bool same;
    if (!SameValue(cx, desc.value(), expectedValue, &same)) {
      return false;
    }
    if (!same) {
      return result.fail(JSMSG_CANT_REDEFINE_PROP);
    }
  }

  return result.succeed();
}

bool ModuleNamespaceObject::ProxyHandler::defineProperty(
    JSContext* cx, HandleObject proxy, HandleId id,
    Handle<PropertyDescriptor> desc, ObjectOpResult& result) const {
  if (JSID_IS_SYMBOL(id)) {
    if (JSID_TO_SYMBOL(id) == cx->wellKnownSymbols().toStringTag) {
      RootedValue value(cx, StringValue(cx->names().Module));
      return ValidatePropertyDescriptor(cx, desc, false, false, false, value,
                                        result);
    }
    return result.fail(JSMSG_CANT_DEFINE_PROP_OBJECT_NOT_EXTENSIBLE);
  }

  const IndirectBindingMap& bindings =
      proxy->as<ModuleNamespaceObject>().bindings();
  ModuleEnvironmentObject* env;
  Shape* shape;
  if (!bindings.lookup(id, &env, &shape)) {
    return result.fail(JSMSG_CANT_DEFINE_PROP_OBJECT_NOT_EXTENSIBLE);
  }

  RootedValue value(cx, env->getSlot(shape->slot()));
  if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  return ValidatePropertyDescriptor(cx, desc, true, true, false, value, result);
}

bool ModuleNamespaceObject::ProxyHandler::has(JSContext* cx, HandleObject proxy,
                                              HandleId id, bool* bp) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (JSID_IS_SYMBOL(id)) {
    *bp = JSID_TO_SYMBOL(id) == cx->wellKnownSymbols().toStringTag;
    return true;
  }

  *bp = ns->bindings().has(id);
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::get(JSContext* cx, HandleObject proxy,
                                              HandleValue receiver, HandleId id,
                                              MutableHandleValue vp) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (JSID_IS_SYMBOL(id)) {
    if (JSID_TO_SYMBOL(id) == cx->wellKnownSymbols().toStringTag) {
      vp.setString(cx->names().Module);
      return true;
    }

    vp.setUndefined();
    return true;
  }

  ModuleEnvironmentObject* env;
  Shape* shape;
  if (!ns->bindings().lookup(id, &env, &shape)) {
    vp.setUndefined();
    return true;
  }

  RootedValue value(cx, env->getSlot(shape->slot()));
  if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  vp.set(value);
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::set(JSContext* cx, HandleObject proxy,
                                              HandleId id, HandleValue v,
                                              HandleValue receiver,
                                              ObjectOpResult& result) const {
  return result.failReadOnly();
}

bool ModuleNamespaceObject::ProxyHandler::delete_(
    JSContext* cx, HandleObject proxy, HandleId id,
    ObjectOpResult& result) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (JSID_IS_SYMBOL(id)) {
    if (JSID_TO_SYMBOL(id) == cx->wellKnownSymbols().toStringTag) {
      return result.failCantDelete();
    }

    return result.succeed();
  }

  if (ns->bindings().has(id)) {
    return result.failCantDelete();
  }

  return result.succeed();
}

bool ModuleNamespaceObject::ProxyHandler::ownPropertyKeys(
    JSContext* cx, HandleObject proxy, MutableHandleIdVector props) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  RootedObject exports(cx, &ns->exports());
  uint32_t count;
  if (!GetLengthProperty(cx, exports, &count) ||
      !props.reserve(props.length() + count + 1)) {
    return false;
  }

  Rooted<ValueVector> names(cx, ValueVector(cx));
  if (!names.resize(count) || !GetElements(cx, exports, count, names.begin())) {
    return false;
  }

  for (uint32_t i = 0; i < count; i++) {
    props.infallibleAppend(AtomToId(&names[i].toString()->asAtom()));
  }

  props.infallibleAppend(SYMBOL_TO_JSID(cx->wellKnownSymbols().toStringTag));

  return true;
}

void ModuleNamespaceObject::ProxyHandler::trace(JSTracer* trc,
                                                JSObject* proxy) const {
  auto& self = proxy->as<ModuleNamespaceObject>();

  if (self.hasBindings()) {
    self.bindings().trace(trc);
  }
}

void ModuleNamespaceObject::ProxyHandler::finalize(JSFreeOp* fop,
                                                   JSObject* proxy) const {
  auto& self = proxy->as<ModuleNamespaceObject>();

  if (self.hasBindings()) {
    fop->delete_(proxy, &self.bindings(), MemoryUse::ModuleBindingMap);
  }
}

///////////////////////////////////////////////////////////////////////////
// FunctionDeclaration

FunctionDeclaration::FunctionDeclaration(HandleAtom name, uint32_t funIndex)
    : name(name), funIndex(funIndex) {}

void FunctionDeclaration::trace(JSTracer* trc) {
  TraceEdge(trc, &name, "FunctionDeclaration name");
}

///////////////////////////////////////////////////////////////////////////
// ModuleObject

/* static */ const JSClassOps ModuleObject::classOps_ = {
    nullptr,                 // addProperty
    nullptr,                 // delProperty
    nullptr,                 // enumerate
    nullptr,                 // newEnumerate
    nullptr,                 // resolve
    nullptr,                 // mayResolve
    ModuleObject::finalize,  // finalize
    nullptr,                 // call
    nullptr,                 // hasInstance
    nullptr,                 // construct
    ModuleObject::trace,     // trace
};

/* static */ const JSClass ModuleObject::class_ = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(ModuleObject::SlotCount) |
        JSCLASS_BACKGROUND_FINALIZE,
    &ModuleObject::classOps_};

#define DEFINE_ARRAY_SLOT_ACCESSOR(cls, name, slot)                 \
  ArrayObject& cls::name() const {                                  \
    return getReservedSlot(cls::slot).toObject().as<ArrayObject>(); \
  }

DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, importEntries, ImportEntriesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, localExportEntries,
                           LocalExportEntriesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, indirectExportEntries,
                           IndirectExportEntriesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, starExportEntries,
                           StarExportEntriesSlot)

/* static */
bool ModuleObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ModuleObject>();
}

/* static */
ModuleObject* ModuleObject::create(JSContext* cx) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateModulePrototype(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  RootedModuleObject self(cx, NewObjectWithGivenProto<ModuleObject>(cx, proto));
  if (!self) {
    return nullptr;
  }

  IndirectBindingMap* bindings = cx->new_<IndirectBindingMap>();
  if (!bindings) {
    return nullptr;
  }

  InitReservedSlot(self, ImportBindingsSlot, bindings,
                   MemoryUse::ModuleBindingMap);

  FunctionDeclarationVector* funDecls = cx->new_<FunctionDeclarationVector>();
  if (!funDecls) {
    return nullptr;
  }

  self->initReservedSlot(FunctionDeclarationsSlot, PrivateValue(funDecls));
  return self;
}

/* static */
void ModuleObject::finalize(JSFreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->maybeOnHelperThread());
  ModuleObject* self = &obj->as<ModuleObject>();
  if (self->hasImportBindings()) {
    fop->delete_(obj, &self->importBindings(), MemoryUse::ModuleBindingMap);
  }
  if (FunctionDeclarationVector* funDecls = self->functionDeclarations()) {
    // Not tracked as these may move between zones on merge.
    fop->deleteUntracked(funDecls);
  }
}

ModuleEnvironmentObject& ModuleObject::initialEnvironment() const {
  Value value = getReservedSlot(EnvironmentSlot);
  return value.toObject().as<ModuleEnvironmentObject>();
}

ModuleEnvironmentObject* ModuleObject::environment() const {
  // Note that this it's valid to call this even if there was an error
  // evaluating the module.

  // According to the spec the environment record is created during
  // instantiation, but we create it earlier than that.
  if (status() < MODULE_STATUS_INSTANTIATED) {
    return nullptr;
  }

  return &initialEnvironment();
}

bool ModuleObject::hasImportBindings() const {
  // Import bindings may not be present if we hit OOM in initialization.
  return !getReservedSlot(ImportBindingsSlot).isUndefined();
}

IndirectBindingMap& ModuleObject::importBindings() {
  return *static_cast<IndirectBindingMap*>(
      getReservedSlot(ImportBindingsSlot).toPrivate());
}

ModuleNamespaceObject* ModuleObject::namespace_() {
  Value value = getReservedSlot(NamespaceSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return &value.toObject().as<ModuleNamespaceObject>();
}

ScriptSourceObject* ModuleObject::scriptSourceObject() const {
  return &getReservedSlot(ScriptSourceObjectSlot)
              .toObject()
              .as<ScriptSourceObject>();
}

FunctionDeclarationVector* ModuleObject::functionDeclarations() {
  Value value = getReservedSlot(FunctionDeclarationsSlot);
  if (value.isUndefined()) {
    return nullptr;
  }

  return static_cast<FunctionDeclarationVector*>(value.toPrivate());
}

void ModuleObject::initScriptSlots(HandleScript script) {
  MOZ_ASSERT(script);
  initReservedSlot(ScriptSlot, PrivateGCThingValue(script));
  initReservedSlot(ScriptSourceObjectSlot,
                   ObjectValue(*script->sourceObject()));
}

void ModuleObject::setInitialEnvironment(
    HandleModuleEnvironmentObject initialEnvironment) {
  initReservedSlot(EnvironmentSlot, ObjectValue(*initialEnvironment));
}

void ModuleObject::initStatusSlot() {
  initReservedSlot(StatusSlot, Int32Value(MODULE_STATUS_UNINSTANTIATED));
}

void ModuleObject::initImportExportData(HandleArrayObject requestedModules,
                                        HandleArrayObject importEntries,
                                        HandleArrayObject localExportEntries,
                                        HandleArrayObject indirectExportEntries,
                                        HandleArrayObject starExportEntries) {
  initReservedSlot(RequestedModulesSlot, ObjectValue(*requestedModules));
  initReservedSlot(ImportEntriesSlot, ObjectValue(*importEntries));
  initReservedSlot(LocalExportEntriesSlot, ObjectValue(*localExportEntries));
  initReservedSlot(IndirectExportEntriesSlot,
                   ObjectValue(*indirectExportEntries));
  initReservedSlot(StarExportEntriesSlot, ObjectValue(*starExportEntries));
}

static bool FreezeObjectProperty(JSContext* cx, HandleNativeObject obj,
                                 uint32_t slot) {
  RootedObject property(cx, &obj->getSlot(slot).toObject());
  return FreezeObject(cx, property);
}

/* static */
bool ModuleObject::Freeze(JSContext* cx, HandleModuleObject self) {
  return FreezeObjectProperty(cx, self, RequestedModulesSlot) &&
         FreezeObjectProperty(cx, self, ImportEntriesSlot) &&
         FreezeObjectProperty(cx, self, LocalExportEntriesSlot) &&
         FreezeObjectProperty(cx, self, IndirectExportEntriesSlot) &&
         FreezeObjectProperty(cx, self, StarExportEntriesSlot) &&
         FreezeObject(cx, self);
}

#ifdef DEBUG

static inline bool CheckObjectFrozen(JSContext* cx, HandleObject obj,
                                     bool* result) {
  return TestIntegrityLevel(cx, obj, IntegrityLevel::Frozen, result);
}

static inline bool CheckObjectPropertyFrozen(JSContext* cx,
                                             HandleNativeObject obj,
                                             uint32_t slot, bool* result) {
  RootedObject property(cx, &obj->getSlot(slot).toObject());
  return CheckObjectFrozen(cx, property, result);
}

/* static */ inline bool ModuleObject::AssertFrozen(JSContext* cx,
                                                    HandleModuleObject self) {
  static const mozilla::EnumSet<ModuleSlot> slotsToCheck = {
      RequestedModulesSlot, ImportEntriesSlot, LocalExportEntriesSlot,
      IndirectExportEntriesSlot, StarExportEntriesSlot};

  bool frozen = false;
  for (auto slot : slotsToCheck) {
    if (!CheckObjectPropertyFrozen(cx, self, slot, &frozen)) {
      return false;
    }
    MOZ_ASSERT(frozen);
  }

  if (!CheckObjectFrozen(cx, self, &frozen)) {
    return false;
  }
  MOZ_ASSERT(frozen);

  return true;
}

#endif

inline static void AssertModuleScopesMatch(ModuleObject* module) {
  MOZ_ASSERT(module->enclosingScope()->is<GlobalScope>());
  MOZ_ASSERT(IsGlobalLexicalEnvironment(
      &module->initialEnvironment().enclosingEnvironment()));
}

void ModuleObject::fixEnvironmentsAfterRealmMerge() {
  AssertModuleScopesMatch(this);
  initialEnvironment().fixEnclosingEnvironmentAfterRealmMerge(
      script()->global());
  AssertModuleScopesMatch(this);
}

JSScript* ModuleObject::maybeScript() const {
  Value value = getReservedSlot(ScriptSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  BaseScript* script = value.toGCThing()->as<BaseScript>();
  MOZ_ASSERT(script->hasBytecode(),
             "Module scripts should always have bytecode");
  return script->asJSScript();
}

JSScript* ModuleObject::script() const {
  JSScript* ptr = maybeScript();
  MOZ_RELEASE_ASSERT(ptr);
  return ptr;
}

static inline void AssertValidModuleStatus(ModuleStatus status) {
  MOZ_ASSERT(status >= MODULE_STATUS_UNINSTANTIATED &&
             status <= MODULE_STATUS_EVALUATED_ERROR);
}

ModuleStatus ModuleObject::status() const {
  ModuleStatus status = getReservedSlot(StatusSlot).toInt32();
  AssertValidModuleStatus(status);
  return status;
}

bool ModuleObject::hadEvaluationError() const {
  return status() == MODULE_STATUS_EVALUATED_ERROR;
}

Value ModuleObject::evaluationError() const {
  MOZ_ASSERT(hadEvaluationError());
  return getReservedSlot(EvaluationErrorSlot);
}

JSObject* ModuleObject::metaObject() const {
  Value value = getReservedSlot(MetaObjectSlot);
  if (value.isObject()) {
    return &value.toObject();
  }

  MOZ_ASSERT(value.isUndefined());
  return nullptr;
}

void ModuleObject::setMetaObject(JSObject* obj) {
  MOZ_ASSERT(obj);
  MOZ_ASSERT(!metaObject());
  setReservedSlot(MetaObjectSlot, ObjectValue(*obj));
}

Scope* ModuleObject::enclosingScope() const {
  return script()->enclosingScope();
}

/* static */
void ModuleObject::trace(JSTracer* trc, JSObject* obj) {
  ModuleObject& module = obj->as<ModuleObject>();

  if (module.hasImportBindings()) {
    module.importBindings().trace(trc);
  }

  if (FunctionDeclarationVector* funDecls = module.functionDeclarations()) {
    funDecls->trace(trc);
  }
}

bool ModuleObject::noteFunctionDeclaration(JSContext* cx, HandleAtom name,
                                           uint32_t funIndex) {
  FunctionDeclarationVector* funDecls = functionDeclarations();
  if (!funDecls->emplaceBack(name, funIndex)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/* static */
bool ModuleObject::instantiateFunctionDeclarations(JSContext* cx,
                                                   HandleModuleObject self) {
#ifdef DEBUG
  MOZ_ASSERT(self->status() == MODULE_STATUS_INSTANTIATING);
  if (!AssertFrozen(cx, self)) {
    return false;
  }
#endif

  // |self| initially manages this vector.
  FunctionDeclarationVector* funDecls = self->functionDeclarations();
  if (!funDecls) {
    JS_ReportErrorASCII(
        cx, "Module function declarations have already been instantiated");
    return false;
  }

  RootedModuleEnvironmentObject env(cx, &self->initialEnvironment());
  RootedObject obj(cx);
  RootedValue value(cx);
  RootedFunction fun(cx);
  for (const auto& funDecl : *funDecls) {
    uint32_t funIndex = funDecl.funIndex;
    fun.set(self->script()->getFunction(funIndex));
    obj = Lambda(cx, fun, env);
    if (!obj) {
      return false;
    }

    value = ObjectValue(*obj);
    if (!SetProperty(cx, env, funDecl.name->asPropertyName(), value)) {
      return false;
    }
  }

  // Transfer ownership of the vector from |self|, then free the vector, once
  // its contents are no longer needed.
  self->setReservedSlot(FunctionDeclarationsSlot, UndefinedValue());
  js_delete(funDecls);
  return true;
}

/* static */
bool ModuleObject::execute(JSContext* cx, HandleModuleObject self,
                           MutableHandleValue rval) {
#ifdef DEBUG
  MOZ_ASSERT(self->status() == MODULE_STATUS_EVALUATING);
  if (!AssertFrozen(cx, self)) {
    return false;
  }
#endif

  RootedScript script(cx, self->script());

  // The top-level script if a module is only ever executed once. Clear the
  // reference at exit to prevent us keeping this alive unnecessarily. This is
  // kept while executing so it is available to the debugger.
  auto guardA = mozilla::MakeScopeExit(
      [&] { self->setReservedSlot(ScriptSlot, UndefinedValue()); });

  RootedModuleEnvironmentObject env(cx, self->environment());
  if (!env) {
    JS_ReportErrorASCII(cx,
                        "Module declarations have not yet been instantiated");
    return false;
  }

  return Execute(cx, script, env, rval);
}

/* static */
ModuleNamespaceObject* ModuleObject::createNamespace(JSContext* cx,
                                                     HandleModuleObject self,
                                                     HandleObject exports) {
  MOZ_ASSERT(!self->namespace_());
  MOZ_ASSERT(exports->is<ArrayObject>());

  auto bindings = cx->make_unique<IndirectBindingMap>();
  if (!bindings) {
    return nullptr;
  }

  auto ns =
      ModuleNamespaceObject::create(cx, self, exports, std::move(bindings));
  if (!ns) {
    return nullptr;
  }

  self->initReservedSlot(NamespaceSlot, ObjectValue(*ns));
  return ns;
}

/* static */
bool ModuleObject::createEnvironment(JSContext* cx, HandleModuleObject self) {
  RootedModuleEnvironmentObject env(cx,
                                    ModuleEnvironmentObject::create(cx, self));
  if (!env) {
    return false;
  }

  self->setInitialEnvironment(env);
  return true;
}

static bool InvokeSelfHostedMethod(JSContext* cx, HandleModuleObject self,
                                   HandlePropertyName name) {
  RootedValue thisv(cx, ObjectValue(*self));
  FixedInvokeArgs<0> args(cx);

  RootedValue ignored(cx);
  return CallSelfHostedFunction(cx, name, thisv, args, &ignored);
}

/* static */
bool ModuleObject::Instantiate(JSContext* cx, HandleModuleObject self) {
  return InvokeSelfHostedMethod(cx, self, cx->names().ModuleInstantiate);
}

/* static */
bool ModuleObject::Evaluate(JSContext* cx, HandleModuleObject self) {
  return InvokeSelfHostedMethod(cx, self, cx->names().ModuleEvaluate);
}

/* static */
ModuleNamespaceObject* ModuleObject::GetOrCreateModuleNamespace(
    JSContext* cx, HandleModuleObject self) {
  FixedInvokeArgs<1> args(cx);
  args[0].setObject(*self);

  RootedValue result(cx);
  if (!CallSelfHostedFunction(cx, cx->names().GetModuleNamespace,
                              UndefinedHandleValue, args, &result)) {
    return nullptr;
  }

  return &result.toObject().as<ModuleNamespaceObject>();
}

DEFINE_GETTER_FUNCTIONS(ModuleObject, namespace_, NamespaceSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, status, StatusSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, evaluationError, EvaluationErrorSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, importEntries, ImportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, localExportEntries,
                        LocalExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, indirectExportEntries,
                        IndirectExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, starExportEntries, StarExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, dfsIndex, DFSIndexSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, dfsAncestorIndex, DFSAncestorIndexSlot)

/* static */
bool GlobalObject::initModuleProto(JSContext* cx,
                                   Handle<GlobalObject*> global) {
  static const JSPropertySpec protoAccessors[] = {
      JS_PSG("namespace", ModuleObject_namespace_Getter, 0),
      JS_PSG("status", ModuleObject_statusGetter, 0),
      JS_PSG("evaluationError", ModuleObject_evaluationErrorGetter, 0),
      JS_PSG("requestedModules", ModuleObject_requestedModulesGetter, 0),
      JS_PSG("importEntries", ModuleObject_importEntriesGetter, 0),
      JS_PSG("localExportEntries", ModuleObject_localExportEntriesGetter, 0),
      JS_PSG("indirectExportEntries", ModuleObject_indirectExportEntriesGetter,
             0),
      JS_PSG("starExportEntries", ModuleObject_starExportEntriesGetter, 0),
      JS_PSG("dfsIndex", ModuleObject_dfsIndexGetter, 0),
      JS_PSG("dfsAncestorIndex", ModuleObject_dfsAncestorIndexGetter, 0),
      JS_PS_END};

  static const JSFunctionSpec protoFunctions[] = {
      JS_SELF_HOSTED_FN("getExportedNames", "ModuleGetExportedNames", 1, 0),
      JS_SELF_HOSTED_FN("resolveExport", "ModuleResolveExport", 2, 0),
      JS_SELF_HOSTED_FN("declarationInstantiation", "ModuleInstantiate", 0, 0),
      JS_SELF_HOSTED_FN("evaluation", "ModuleEvaluate", 0, 0), JS_FS_END};

  RootedObject proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto) {
    return false;
  }

  if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors,
                                    protoFunctions)) {
    return false;
  }

  global->setReservedSlot(MODULE_PROTO, ObjectValue(*proto));
  return true;
}

#undef DEFINE_GETTER_FUNCTIONS
#undef DEFINE_STRING_ACCESSOR_METHOD
#undef DEFINE_ARRAY_SLOT_ACCESSOR

///////////////////////////////////////////////////////////////////////////
// ModuleBuilder

ModuleBuilder::ModuleBuilder(JSContext* cx,
                             const frontend::EitherParser& eitherParser)
    : cx_(cx),
      eitherParser_(eitherParser),
      requestedModuleSpecifiers_(cx, AtomSet(cx)),
      requestedModules_(cx, RequestedModuleVector(cx)),
      importEntries_(cx, ImportEntryMap(cx)),
      exportEntries_(cx, ExportEntryVector(cx)),
      exportNames_(cx, AtomSet(cx)),
      localExportEntries_(cx, ExportEntryVector(cx)),
      indirectExportEntries_(cx, ExportEntryVector(cx)),
      starExportEntries_(cx, ExportEntryVector(cx)) {}

bool ModuleBuilder::buildTables() {
  for (const auto& e : exportEntries_) {
    RootedExportEntryObject exp(cx_, e);
    if (!exp->moduleRequest()) {
      RootedImportEntryObject importEntry(cx_,
                                          importEntryFor(exp->localName()));
      if (!importEntry) {
        if (!localExportEntries_.append(exp)) {
          return false;
        }
      } else {
        if (importEntry->importName() == cx_->names().star) {
          if (!localExportEntries_.append(exp)) {
            return false;
          }
        } else {
          RootedAtom exportName(cx_, exp->exportName());
          RootedAtom moduleRequest(cx_, importEntry->moduleRequest());
          RootedAtom importName(cx_, importEntry->importName());
          RootedExportEntryObject exportEntry(cx_);
          exportEntry = ExportEntryObject::create(
              cx_, exportName, moduleRequest, importName, nullptr,
              exp->lineNumber(), exp->columnNumber());
          if (!exportEntry || !indirectExportEntries_.append(exportEntry)) {
            return false;
          }
        }
      }
    } else if (exp->importName() == cx_->names().star) {
      if (!starExportEntries_.append(exp)) {
        return false;
      }
    } else {
      if (!indirectExportEntries_.append(exp)) {
        return false;
      }
    }
  }

  return true;
}

bool ModuleBuilder::initModule(JS::Handle<ModuleObject*> module) {
  RootedArrayObject requestedModules(cx_,
                                     js::CreateArray(cx_, requestedModules_));
  if (!requestedModules) {
    return false;
  }

  RootedArrayObject importEntries(cx_, createArrayFromHashMap(importEntries_));
  if (!importEntries) {
    return false;
  }

  RootedArrayObject localExportEntries(
      cx_, js::CreateArray(cx_, localExportEntries_));
  if (!localExportEntries) {
    return false;
  }

  RootedArrayObject indirectExportEntries(
      cx_, js::CreateArray(cx_, indirectExportEntries_));
  if (!indirectExportEntries) {
    return false;
  }

  RootedArrayObject starExportEntries(cx_,
                                      js::CreateArray(cx_, starExportEntries_));
  if (!starExportEntries) {
    return false;
  }

  module->initImportExportData(requestedModules, importEntries,
                               localExportEntries, indirectExportEntries,
                               starExportEntries);

  return true;
}

bool ModuleBuilder::processImport(frontend::BinaryNode* importNode) {
  using namespace js::frontend;

  MOZ_ASSERT(importNode->isKind(ParseNodeKind::ImportDecl));

  ListNode* specList = &importNode->left()->as<ListNode>();
  MOZ_ASSERT(specList->isKind(ParseNodeKind::ImportSpecList));

  NameNode* moduleSpec = &importNode->right()->as<NameNode>();
  MOZ_ASSERT(moduleSpec->isKind(ParseNodeKind::StringExpr));

  RootedAtom module(cx_, moduleSpec->atom());
  if (!maybeAppendRequestedModule(module, moduleSpec)) {
    return false;
  }

  RootedAtom importName(cx_);
  RootedAtom localName(cx_);
  for (ParseNode* item : specList->contents()) {
    BinaryNode* spec = &item->as<BinaryNode>();
    MOZ_ASSERT(spec->isKind(ParseNodeKind::ImportSpec));

    NameNode* importNameNode = &spec->left()->as<NameNode>();

    NameNode* localNameNode = &spec->right()->as<NameNode>();

    importName = importNameNode->atom();
    localName = localNameNode->atom();

    uint32_t line;
    uint32_t column;
    eitherParser_.computeLineAndColumn(importNameNode->pn_pos.begin, &line,
                                       &column);

    RootedImportEntryObject importEntry(cx_);
    importEntry = ImportEntryObject::create(cx_, module, importName, localName,
                                            line, column);
    if (!importEntry || !appendImportEntryObject(importEntry)) {
      return false;
    }
  }

  return true;
}

bool ModuleBuilder::appendImportEntryObject(
    HandleImportEntryObject importEntry) {
  MOZ_ASSERT(importEntry->localName());
  return importEntries_.put(importEntry->localName(), importEntry);
}

bool ModuleBuilder::processExport(frontend::ParseNode* exportNode) {
  using namespace js::frontend;

  MOZ_ASSERT(exportNode->isKind(ParseNodeKind::ExportStmt) ||
             exportNode->isKind(ParseNodeKind::ExportDefaultStmt));

  bool isDefault = exportNode->isKind(ParseNodeKind::ExportDefaultStmt);
  ParseNode* kid = isDefault ? exportNode->as<BinaryNode>().left()
                             : exportNode->as<UnaryNode>().kid();

  if (isDefault && exportNode->as<BinaryNode>().right()) {
    // This is an export default containing an expression.
    HandlePropertyName localName = cx_->names().default_;
    HandlePropertyName exportName = cx_->names().default_;
    return appendExportEntry(exportName, localName);
  }

  switch (kid->getKind()) {
    case ParseNodeKind::ExportSpecList: {
      MOZ_ASSERT(!isDefault);
      RootedAtom localName(cx_);
      RootedAtom exportName(cx_);
      for (ParseNode* item : kid->as<ListNode>().contents()) {
        BinaryNode* spec = &item->as<BinaryNode>();
        MOZ_ASSERT(spec->isKind(ParseNodeKind::ExportSpec));

        NameNode* localNameNode = &spec->left()->as<NameNode>();
        NameNode* exportNameNode = &spec->right()->as<NameNode>();
        localName = localNameNode->atom();
        exportName = exportNameNode->atom();
        if (!appendExportEntry(exportName, localName, spec)) {
          return false;
        }
      }
      break;
    }

    case ParseNodeKind::ClassDecl: {
      const ClassNode& cls = kid->as<ClassNode>();
      MOZ_ASSERT(cls.names());
      RootedAtom localName(cx_, cls.names()->innerBinding()->atom());
      RootedAtom exportName(
          cx_, isDefault ? cx_->names().default_ : localName.get());
      if (!appendExportEntry(exportName, localName)) {
        return false;
      }
      break;
    }

    case ParseNodeKind::VarStmt:
    case ParseNodeKind::ConstDecl:
    case ParseNodeKind::LetDecl: {
      RootedAtom localName(cx_);
      RootedAtom exportName(cx_);
      for (ParseNode* binding : kid->as<ListNode>().contents()) {
        if (binding->isKind(ParseNodeKind::AssignExpr)) {
          binding = binding->as<AssignmentNode>().left();
        } else {
          MOZ_ASSERT(binding->isKind(ParseNodeKind::Name));
        }

        if (binding->isKind(ParseNodeKind::Name)) {
          localName = binding->as<NameNode>().atom();
          exportName = isDefault ? cx_->names().default_ : localName.get();
          if (!appendExportEntry(exportName, localName)) {
            return false;
          }
        } else if (binding->isKind(ParseNodeKind::ArrayExpr)) {
          if (!processExportArrayBinding(&binding->as<ListNode>())) {
            return false;
          }
        } else {
          MOZ_ASSERT(binding->isKind(ParseNodeKind::ObjectExpr));
          if (!processExportObjectBinding(&binding->as<ListNode>())) {
            return false;
          }
        }
      }
      break;
    }

    case ParseNodeKind::Function: {
      FunctionBox* box = kid->as<FunctionNode>().funbox();
      MOZ_ASSERT(!box->isArrow());
      RootedAtom localName(cx_, box->explicitName());
      RootedAtom exportName(
          cx_, isDefault ? cx_->names().default_ : localName.get());
      MOZ_ASSERT_IF(isDefault, localName);
      if (!appendExportEntry(exportName, localName)) {
        return false;
      }
      break;
    }

    default:
      MOZ_CRASH("Unexpected parse node");
  }

  return true;
}

bool ModuleBuilder::processExportBinding(frontend::ParseNode* binding) {
  using namespace js::frontend;

  if (binding->isKind(ParseNodeKind::Name)) {
    RootedAtom name(cx_, binding->as<NameNode>().atom());
    return appendExportEntry(name, name);
  }

  if (binding->isKind(ParseNodeKind::ArrayExpr)) {
    return processExportArrayBinding(&binding->as<ListNode>());
  }

  MOZ_ASSERT(binding->isKind(ParseNodeKind::ObjectExpr));
  return processExportObjectBinding(&binding->as<ListNode>());
}

bool ModuleBuilder::processExportArrayBinding(frontend::ListNode* array) {
  using namespace js::frontend;

  MOZ_ASSERT(array->isKind(ParseNodeKind::ArrayExpr));

  for (ParseNode* node : array->contents()) {
    if (node->isKind(ParseNodeKind::Elision)) {
      continue;
    }

    if (node->isKind(ParseNodeKind::Spread)) {
      node = node->as<UnaryNode>().kid();
    } else if (node->isKind(ParseNodeKind::AssignExpr)) {
      node = node->as<AssignmentNode>().left();
    }

    if (!processExportBinding(node)) {
      return false;
    }
  }

  return true;
}

bool ModuleBuilder::processExportObjectBinding(frontend::ListNode* obj) {
  using namespace js::frontend;

  MOZ_ASSERT(obj->isKind(ParseNodeKind::ObjectExpr));

  for (ParseNode* node : obj->contents()) {
    MOZ_ASSERT(node->isKind(ParseNodeKind::MutateProto) ||
               node->isKind(ParseNodeKind::PropertyDefinition) ||
               node->isKind(ParseNodeKind::Shorthand) ||
               node->isKind(ParseNodeKind::Spread));

    ParseNode* target;
    if (node->isKind(ParseNodeKind::Spread)) {
      target = node->as<UnaryNode>().kid();
    } else {
      if (node->isKind(ParseNodeKind::MutateProto)) {
        target = node->as<UnaryNode>().kid();
      } else {
        target = node->as<BinaryNode>().right();
      }

      if (target->isKind(ParseNodeKind::AssignExpr)) {
        target = target->as<AssignmentNode>().left();
      }
    }

    if (!processExportBinding(target)) {
      return false;
    }
  }

  return true;
}

bool ModuleBuilder::processExportFrom(frontend::BinaryNode* exportNode) {
  using namespace js::frontend;

  MOZ_ASSERT(exportNode->isKind(ParseNodeKind::ExportFromStmt));

  ListNode* specList = &exportNode->left()->as<ListNode>();
  MOZ_ASSERT(specList->isKind(ParseNodeKind::ExportSpecList));

  NameNode* moduleSpec = &exportNode->right()->as<NameNode>();
  MOZ_ASSERT(moduleSpec->isKind(ParseNodeKind::StringExpr));

  RootedAtom module(cx_, moduleSpec->atom());
  if (!maybeAppendRequestedModule(module, moduleSpec)) {
    return false;
  }

  RootedAtom bindingName(cx_);
  RootedAtom exportName(cx_);
  for (ParseNode* spec : specList->contents()) {
    if (spec->isKind(ParseNodeKind::ExportSpec)) {
      NameNode* localNameNode = &spec->as<BinaryNode>().left()->as<NameNode>();
      NameNode* exportNameNode =
          &spec->as<BinaryNode>().right()->as<NameNode>();
      bindingName = localNameNode->atom();
      exportName = exportNameNode->atom();
      if (!appendExportFromEntry(exportName, module, bindingName,
                                 localNameNode)) {
        return false;
      }
    } else {
      MOZ_ASSERT(spec->isKind(ParseNodeKind::ExportBatchSpecStmt));
      exportName = cx_->names().star;
      if (!appendExportFromEntry(nullptr, module, exportName, spec)) {
        return false;
      }
    }
  }

  return true;
}

ImportEntryObject* ModuleBuilder::importEntryFor(JSAtom* localName) const {
  MOZ_ASSERT(localName);
  auto ptr = importEntries_.lookup(localName);
  if (!ptr) {
    return nullptr;
  }

  return ptr->value();
}

bool ModuleBuilder::hasExportedName(JSAtom* name) const {
  MOZ_ASSERT(name);
  return exportNames_.has(name);
}

bool ModuleBuilder::appendExportEntry(HandleAtom exportName,
                                      HandleAtom localName,
                                      frontend::ParseNode* node) {
  uint32_t line = 0;
  uint32_t column = 0;
  if (node) {
    eitherParser_.computeLineAndColumn(node->pn_pos.begin, &line, &column);
  }

  Rooted<ExportEntryObject*> exportEntry(cx_);
  exportEntry = ExportEntryObject::create(cx_, exportName, nullptr, nullptr,
                                          localName, line, column);
  return exportEntry && appendExportEntryObject(exportEntry);
}

bool ModuleBuilder::appendExportFromEntry(HandleAtom exportName,
                                          HandleAtom moduleRequest,
                                          HandleAtom importName,
                                          frontend::ParseNode* node) {
  uint32_t line;
  uint32_t column;
  eitherParser_.computeLineAndColumn(node->pn_pos.begin, &line, &column);

  Rooted<ExportEntryObject*> exportEntry(cx_);
  exportEntry = ExportEntryObject::create(cx_, exportName, moduleRequest,
                                          importName, nullptr, line, column);
  return exportEntry && appendExportEntryObject(exportEntry);
}

bool ModuleBuilder::appendExportEntryObject(
    HandleExportEntryObject exportEntry) {
  if (!exportEntries_.append(exportEntry)) {
    return false;
  }

  JSAtom* exportName = exportEntry->exportName();
  return !exportName || exportNames_.put(exportName);
}

bool ModuleBuilder::maybeAppendRequestedModule(HandleAtom specifier,
                                               frontend::ParseNode* node) {
  if (requestedModuleSpecifiers_.has(specifier)) {
    return true;
  }

  uint32_t line;
  uint32_t column;
  eitherParser_.computeLineAndColumn(node->pn_pos.begin, &line, &column);

  JSContext* cx = cx_;
  RootedRequestedModuleObject req(
      cx, RequestedModuleObject::create(cx, specifier, line, column));
  if (!req) {
    return false;
  }

  return FreezeObject(cx, req) && requestedModules_.append(req) &&
         requestedModuleSpecifiers_.put(specifier);
}

template <typename T>
ArrayObject* js::CreateArray(JSContext* cx,
                             const JS::Rooted<GCVector<T>>& vector) {
  uint32_t length = vector.length();
  RootedArrayObject array(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!array) {
    return nullptr;
  }

  array->setDenseInitializedLength(length);
  for (uint32_t i = 0; i < length; i++) {
    array->initDenseElement(i, ObjectValue(*vector[i]));
  }

  return array;
}

template <typename K, typename V>
ArrayObject* ModuleBuilder::createArrayFromHashMap(
    const JS::Rooted<GCHashMap<K, V>>& map) {
  uint32_t length = map.count();
  RootedArrayObject array(cx_, NewDenseFullyAllocatedArray(cx_, length));
  if (!array) {
    return nullptr;
  }

  array->setDenseInitializedLength(length);

  uint32_t i = 0;
  for (auto r = map.all(); !r.empty(); r.popFront()) {
    array->initDenseElement(i++, ObjectValue(*r.front().value()));
  }

  return array;
}

JSObject* js::GetOrCreateModuleMetaObject(JSContext* cx,
                                          HandleObject moduleArg) {
  HandleModuleObject module = moduleArg.as<ModuleObject>();
  if (JSObject* obj = module->metaObject()) {
    return obj;
  }

  RootedObject metaObject(cx,
                          NewObjectWithGivenProto<PlainObject>(cx, nullptr));
  if (!metaObject) {
    return nullptr;
  }

  JS::ModuleMetadataHook func = cx->runtime()->moduleMetadataHook;
  if (!func) {
    JS_ReportErrorASCII(cx, "Module metadata hook not set");
    return nullptr;
  }

  RootedValue modulePrivate(cx, JS::GetModulePrivate(module));
  if (!func(cx, modulePrivate, metaObject)) {
    return nullptr;
  }

  module->setMetaObject(metaObject);

  return metaObject;
}

JSObject* js::CallModuleResolveHook(JSContext* cx,
                                    HandleValue referencingPrivate,
                                    HandleString specifier) {
  JS::ModuleResolveHook moduleResolveHook = cx->runtime()->moduleResolveHook;
  if (!moduleResolveHook) {
    JS_ReportErrorASCII(cx, "Module resolve hook not set");
    return nullptr;
  }

  RootedObject result(cx, moduleResolveHook(cx, referencingPrivate, specifier));
  if (!result) {
    return nullptr;
  }

  if (!result->is<ModuleObject>()) {
    JS_ReportErrorASCII(cx, "Module resolve hook did not return Module object");
    return nullptr;
  }

  return result;
}

JSObject* js::StartDynamicModuleImport(JSContext* cx, HandleScript script,
                                       HandleValue specifierArg) {
  RootedObject promiseConstructor(cx, JS::GetPromiseConstructor(cx));
  if (!promiseConstructor) {
    return nullptr;
  }

  RootedObject promiseObject(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promiseObject) {
    return nullptr;
  }

  Handle<PromiseObject*> promise = promiseObject.as<PromiseObject>();

  JS::ModuleDynamicImportHook importHook =
      cx->runtime()->moduleDynamicImportHook;

  if (!importHook) {
    // Dynamic import can be disabled by a pref and is not supported in all
    // contexts (e.g. web workers).
    JS_ReportErrorASCII(
        cx,
        "Dynamic module import is disabled or not supported in this context");
    if (!RejectPromiseWithPendingError(cx, promise)) {
      return nullptr;
    }
    return promise;
  }

  RootedString specifier(cx, ToString(cx, specifierArg));
  if (!specifier) {
    if (!RejectPromiseWithPendingError(cx, promise)) {
      return nullptr;
    }
    return promise;
  }

  RootedValue referencingPrivate(cx,
                                 script->sourceObject()->canonicalPrivate());
  cx->runtime()->addRefScriptPrivate(referencingPrivate);

  if (!importHook(cx, referencingPrivate, specifier, promise)) {
    cx->runtime()->releaseScriptPrivate(referencingPrivate);

    // If there's no exception pending then the script is terminating
    // anyway, so just return nullptr.
    if (!cx->isExceptionPending() ||
        !RejectPromiseWithPendingError(cx, promise)) {
      return nullptr;
    }
    return promise;
  }

  return promise;
}

bool js::FinishDynamicModuleImport(JSContext* cx,
                                   HandleValue referencingPrivate,
                                   HandleString specifier,
                                   HandleObject promiseArg) {
  Handle<PromiseObject*> promise = promiseArg.as<PromiseObject>();

  auto releasePrivate = mozilla::MakeScopeExit(
      [&] { cx->runtime()->releaseScriptPrivate(referencingPrivate); });

  if (cx->isExceptionPending()) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  RootedObject result(cx,
                      CallModuleResolveHook(cx, referencingPrivate, specifier));
  if (!result) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  RootedModuleObject module(cx, &result->as<ModuleObject>());
  if (module->status() != MODULE_STATUS_EVALUATED) {
    JS_ReportErrorASCII(
        cx, "Unevaluated or errored module returned by module resolve hook");
    return RejectPromiseWithPendingError(cx, promise);
  }

  RootedObject ns(cx, ModuleObject::GetOrCreateModuleNamespace(cx, module));
  if (!ns) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  RootedValue value(cx, ObjectValue(*ns));
  return PromiseObject::resolve(cx, promise, value);
}

template <XDRMode mode>
XDRResult js::XDRExportEntries(XDRState<mode>* xdr,
                               MutableHandleArrayObject vec) {
  JSContext* cx = xdr->cx();
  Rooted<GCVector<ExportEntryObject*>> expVec(cx);
  RootedExportEntryObject expObj(cx);
  RootedAtom exportName(cx);
  RootedAtom moduleRequest(cx);
  RootedAtom importName(cx);
  RootedAtom localName(cx);

  uint32_t length = 0;
  uint32_t lineNumber = 0;
  uint32_t columnNumber = 0;

  if (mode == XDR_ENCODE) {
    length = vec->length();
  }
  MOZ_TRY(xdr->codeUint32(&length));
  for (uint32_t i = 0; i < length; i++) {
    if (mode == XDR_ENCODE) {
      expObj = &vec->getDenseElement(i).toObject().as<ExportEntryObject>();

      exportName = expObj->exportName();
      moduleRequest = expObj->moduleRequest();
      importName = expObj->importName();
      localName = expObj->localName();
      lineNumber = expObj->lineNumber();
      columnNumber = expObj->columnNumber();
    }

    MOZ_TRY(XDRAtomOrNull(xdr, &exportName));
    MOZ_TRY(XDRAtomOrNull(xdr, &moduleRequest));
    MOZ_TRY(XDRAtomOrNull(xdr, &importName));
    MOZ_TRY(XDRAtomOrNull(xdr, &localName));

    MOZ_TRY(xdr->codeUint32(&lineNumber));
    MOZ_TRY(xdr->codeUint32(&columnNumber));

    if (mode == XDR_DECODE) {
      expObj.set(ExportEntryObject::create(cx, exportName, moduleRequest,
                                           importName, localName, lineNumber,
                                           columnNumber));
      if (!expObj) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
      if (!expVec.append(expObj)) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  }

  if (mode == XDR_DECODE) {
    RootedArrayObject expArr(cx, js::CreateArray(cx, expVec));
    if (!expArr) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    vec.set(expArr);
  }

  return Ok();
}

template <XDRMode mode>
XDRResult js::XDRRequestedModuleObject(
    XDRState<mode>* xdr, MutableHandleRequestedModuleObject reqObj) {
  JSContext* cx = xdr->cx();
  RootedAtom moduleSpecifier(cx);
  uint32_t lineNumber = 0;
  uint32_t columnNumber = 0;
  if (mode == XDR_ENCODE) {
    moduleSpecifier = reqObj->moduleSpecifier();
    lineNumber = reqObj->lineNumber();
    columnNumber = reqObj->columnNumber();
  }

  MOZ_TRY(XDRAtom(xdr, &moduleSpecifier));
  MOZ_TRY(xdr->codeUint32(&lineNumber));
  MOZ_TRY(xdr->codeUint32(&columnNumber));

  if (mode == XDR_DECODE) {
    reqObj.set(RequestedModuleObject::create(cx, moduleSpecifier, lineNumber,
                                             columnNumber));
    if (!reqObj) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  return Ok();
}

template <XDRMode mode>
XDRResult js::XDRImportEntryObject(XDRState<mode>* xdr,
                                   MutableHandleImportEntryObject impObj) {
  JSContext* cx = xdr->cx();
  RootedAtom moduleRequest(cx);
  RootedAtom importName(cx);
  RootedAtom localName(cx);
  uint32_t lineNumber = 0;
  uint32_t columnNumber = 0;
  if (mode == XDR_ENCODE) {
    moduleRequest = impObj->moduleRequest();
    importName = impObj->importName();
    localName = impObj->localName();
    lineNumber = impObj->lineNumber();
    columnNumber = impObj->columnNumber();
  }

  MOZ_TRY(XDRAtomOrNull(xdr, &moduleRequest));
  MOZ_TRY(XDRAtomOrNull(xdr, &importName));
  MOZ_TRY(XDRAtomOrNull(xdr, &localName));
  MOZ_TRY(xdr->codeUint32(&lineNumber));
  MOZ_TRY(xdr->codeUint32(&columnNumber));

  if (mode == XDR_DECODE) {
    impObj.set(ImportEntryObject::create(cx, moduleRequest, importName,
                                         localName, lineNumber, columnNumber));
    if (!impObj) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  return Ok();
}

template <XDRMode mode>
XDRResult js::XDRModuleObject(XDRState<mode>* xdr,
                              MutableHandleModuleObject modp) {
  JSContext* cx = xdr->cx();
  RootedModuleObject module(cx, modp);

  RootedScope enclosingScope(cx);
  RootedScript script(cx);

  RootedArrayObject requestedModules(cx);
  RootedArrayObject importEntries(cx);
  RootedArrayObject localExportEntries(cx);
  RootedArrayObject indirectExportEntries(cx);
  RootedArrayObject starExportEntries(cx);
  // funcDecls points to data traced by the ModuleObject,
  // but is itself heap-allocated so we don't need to
  // worry about rooting it again here.
  FunctionDeclarationVector* funcDecls;

  uint32_t requestedModulesLength = 0;
  uint32_t importEntriesLength = 0;
  uint32_t funcDeclLength = 0;

  if (mode == XDR_ENCODE) {
    module = modp.get();

    script.set(module->script());
    enclosingScope.set(module->enclosingScope());
    MOZ_ASSERT(!enclosingScope->as<GlobalScope>().hasBindings());

    requestedModules = &module->requestedModules();
    importEntries = &module->importEntries();
    localExportEntries = &module->localExportEntries();
    indirectExportEntries = &module->indirectExportEntries();
    starExportEntries = &module->starExportEntries();
    funcDecls = module->functionDeclarations();

    requestedModulesLength = requestedModules->length();
    importEntriesLength = importEntries->length();
    funcDeclLength = funcDecls->length();
  }

  /* ScriptSourceObject slot - ScriptSourceObject is created in XDRScript and is
   * set when init is called. */
  if (mode == XDR_DECODE) {
    enclosingScope.set(&cx->global()->emptyGlobalScope());
    module.set(ModuleObject::create(cx));
    if (!module) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  /* Script slot */
  MOZ_TRY(XDRScript(xdr, enclosingScope, nullptr, module, &script));

  if (mode == XDR_DECODE) {
    module->initScriptSlots(script);
    module->initStatusSlot();
  }

  /* Environment Slot */
  if (mode == XDR_DECODE) {
    if (!ModuleObject::createEnvironment(cx, module)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  /* Namespace Slot, Status Slot, EvaluationErrorSlot, MetaObject - Initialized
   * at instantiation */

  /* RequestedModules slot */
  RootedRequestedModuleVector reqVec(cx, GCVector<RequestedModuleObject*>(cx));
  RootedRequestedModuleObject reqObj(cx);
  MOZ_TRY(xdr->codeUint32(&requestedModulesLength));
  for (uint32_t i = 0; i < requestedModulesLength; i++) {
    if (mode == XDR_ENCODE) {
      reqObj = &module->requestedModules()
                    .getDenseElement(i)
                    .toObject()
                    .as<RequestedModuleObject>();
    }
    MOZ_TRY(XDRRequestedModuleObject(xdr, &reqObj));
    if (mode == XDR_DECODE) {
      if (!reqVec.append(reqObj)) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  }
  if (mode == XDR_DECODE) {
    RootedArrayObject reqArr(cx, js::CreateArray(cx, reqVec));
    if (!reqArr) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    requestedModules.set(reqArr);
  }

  /* ImportEntries slot */
  RootedImportEntryVector impVec(cx, GCVector<ImportEntryObject*>(cx));
  RootedImportEntryObject impObj(cx);
  MOZ_TRY(xdr->codeUint32(&importEntriesLength));
  for (uint32_t i = 0; i < importEntriesLength; i++) {
    if (mode == XDR_ENCODE) {
      impObj = &module->importEntries()
                    .getDenseElement(i)
                    .toObject()
                    .as<ImportEntryObject>();
    }
    MOZ_TRY(XDRImportEntryObject(xdr, &impObj));
    if (mode == XDR_DECODE) {
      if (!impVec.append(impObj)) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  }

  if (mode == XDR_DECODE) {
    RootedArrayObject impArr(cx, js::CreateArray(cx, impVec));
    if (!impArr) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    importEntries.set(impArr);
  }

  /* LocalExportEntries slot */
  MOZ_TRY(XDRExportEntries(xdr, &localExportEntries));
  /* IndirectExportEntries slot */
  MOZ_TRY(XDRExportEntries(xdr, &indirectExportEntries));
  /* StarExportEntries slot */
  MOZ_TRY(XDRExportEntries(xdr, &starExportEntries));

  /* FunctionDeclarations slot */
  RootedAtom funcName(cx);
  uint32_t funIndex = 0;
  MOZ_TRY(xdr->codeUint32(&funcDeclLength));
  for (uint32_t i = 0; i < funcDeclLength; i++) {
    if (mode == XDR_ENCODE) {
      FunctionDeclaration fd = (*funcDecls)[i];
      funcName = fd.name;
      funIndex = fd.funIndex;
    }

    MOZ_TRY(XDRAtom(xdr, &funcName));
    MOZ_TRY(xdr->codeUint32(&funIndex));

    if (mode == XDR_DECODE) {
      FunctionDeclaration funcDecl(funcName, funIndex);
      if (!module->functionDeclarations()->append(funcDecl)) {
        ReportOutOfMemory(cx);
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  }

  /* ImportBindings slot, DFSIndex slot, DFSAncestorIndex slot -
   * Initialized at instantiation */
  if (mode == XDR_DECODE) {
    module->initImportExportData(requestedModules, importEntries,
                                 localExportEntries, indirectExportEntries,
                                 starExportEntries);
  }

  modp.set(module);
  return Ok();
}

template XDRResult js::XDRModuleObject(XDRState<XDR_ENCODE>* xdr,
                                       MutableHandleModuleObject scriptp);

template XDRResult js::XDRModuleObject(XDRState<XDR_DECODE>* xdr,
                                       MutableHandleModuleObject scriptp);
