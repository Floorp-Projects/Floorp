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
#include "frontend/ParserAtom.h"  // TaggedParserAtomIndex, ParserAtomsTable, ParserAtom
#include "frontend/SharedContext.h"
#include "gc/GCContext.h"
#include "gc/Tracer.h"
#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "js/Modules.h"  // JS::GetModulePrivate, JS::ModuleDynamicImportHook
#include "vm/EqualityOperations.h"  // js::SameValue
#include "vm/Interpreter.h"    // Execute, Lambda, ReportRuntimeLexicalError
#include "vm/ModuleBuilder.h"  // js::ModuleBuilder
#include "vm/Modules.h"
#include "vm/PlainObject.h"    // js::PlainObject
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/SharedStencil.h"  // js::GCThingIndex

#include "builtin/HandlerFunction-inl.h"  // js::ExtraValueFromHandler, js::NewHandler{,WithExtraValue}, js::TargetFromHandler
#include "gc/GCContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/List-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

static_assert(ModuleStatus::Unlinked < ModuleStatus::Linking &&
                  ModuleStatus::Linking < ModuleStatus::Linked &&
                  ModuleStatus::Linked < ModuleStatus::Evaluating &&
                  ModuleStatus::Evaluating < ModuleStatus::EvaluatingAsync &&
                  ModuleStatus::EvaluatingAsync < ModuleStatus::Evaluated &&
                  ModuleStatus::Evaluated < ModuleStatus::Evaluated_Error,
              "Module statuses are ordered incorrectly");

static inline Value ModuleStatusValue(ModuleStatus status) {
  return Int32Value(int32_t(status));
}

static Value StringOrNullValue(JSString* maybeString) {
  return maybeString ? StringValue(maybeString) : NullValue();
}

#define DEFINE_ATOM_ACCESSOR_METHOD(cls, name, slot) \
  JSAtom* cls::name() const {                        \
    Value value = getReservedSlot(slot);             \
    return &value.toString()->asAtom();              \
  }

#define DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(cls, name, slot) \
  JSAtom* cls::name() const {                                \
    Value value = getReservedSlot(slot);                     \
    if (value.isNull()) {                                    \
      return nullptr;                                        \
    }                                                        \
    return &value.toString()->asAtom();                      \
  }

#define DEFINE_UINT32_ACCESSOR_METHOD(cls, name, slot) \
  uint32_t cls::name() const {                         \
    Value value = getReservedSlot(slot);               \
    MOZ_ASSERT(value.toNumber() >= 0);                 \
    if (value.isInt32()) {                             \
      return value.toInt32();                          \
    }                                                  \
    return JS::ToUint32(value.toDouble());             \
  }

///////////////////////////////////////////////////////////////////////////
// ImportEntryObject

/* static */ const JSClass ImportEntryObject::class_ = {
    "ImportEntry", JSCLASS_HAS_RESERVED_SLOTS(ImportEntryObject::SlotCount)};

DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ImportEntryObject, importName,
                                    ImportNameSlot)
DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, localName, LocalNameSlot)
DEFINE_UINT32_ACCESSOR_METHOD(ImportEntryObject, lineNumber, LineNumberSlot)
DEFINE_UINT32_ACCESSOR_METHOD(ImportEntryObject, columnNumber, ColumnNumberSlot)

ModuleRequestObject* ImportEntryObject::moduleRequest() const {
  Value value = getReservedSlot(ModuleRequestSlot);
  return &value.toObject().as<ModuleRequestObject>();
}

/* static */
bool ImportEntryObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ImportEntryObject>();
}

/* static */
ImportEntryObject* ImportEntryObject::create(
    JSContext* cx, HandleObject moduleRequest, Handle<JSAtom*> maybeImportName,
    Handle<JSAtom*> localName, uint32_t lineNumber, uint32_t columnNumber) {
  ImportEntryObject* self =
      NewObjectWithGivenProto<ImportEntryObject>(cx, nullptr);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ModuleRequestSlot, ObjectValue(*moduleRequest));
  self->initReservedSlot(ImportNameSlot, StringOrNullValue(maybeImportName));
  self->initReservedSlot(LocalNameSlot, StringValue(localName));
  self->initReservedSlot(LineNumberSlot, NumberValue(lineNumber));
  self->initReservedSlot(ColumnNumberSlot, NumberValue(columnNumber));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// ExportEntryObject

/* static */ const JSClass ExportEntryObject::class_ = {
    "ExportEntry", JSCLASS_HAS_RESERVED_SLOTS(ExportEntryObject::SlotCount)};

DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, exportName,
                                    ExportNameSlot)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, importName,
                                    ImportNameSlot)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, localName, LocalNameSlot)
DEFINE_UINT32_ACCESSOR_METHOD(ExportEntryObject, lineNumber, LineNumberSlot)
DEFINE_UINT32_ACCESSOR_METHOD(ExportEntryObject, columnNumber, ColumnNumberSlot)

ModuleRequestObject* ExportEntryObject::moduleRequest() const {
  Value value = getReservedSlot(ModuleRequestSlot);
  return &value.toObject().as<ModuleRequestObject>();
}

/* static */
bool ExportEntryObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ExportEntryObject>();
}

/* static */
ExportEntryObject* ExportEntryObject::create(
    JSContext* cx, Handle<JSAtom*> maybeExportName, HandleObject moduleRequest,
    Handle<JSAtom*> maybeImportName, Handle<JSAtom*> maybeLocalName,
    uint32_t lineNumber, uint32_t columnNumber) {
  // Line and column numbers are optional for export entries since direct
  // entries are checked at parse time.

  ExportEntryObject* self =
      NewObjectWithGivenProto<ExportEntryObject>(cx, nullptr);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ExportNameSlot, StringOrNullValue(maybeExportName));
  self->initReservedSlot(ModuleRequestSlot, ObjectValue(*moduleRequest));
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

DEFINE_UINT32_ACCESSOR_METHOD(RequestedModuleObject, lineNumber, LineNumberSlot)
DEFINE_UINT32_ACCESSOR_METHOD(RequestedModuleObject, columnNumber,
                              ColumnNumberSlot)

ModuleRequestObject* RequestedModuleObject::moduleRequest() const {
  Value value = getReservedSlot(ModuleRequestSlot);
  return &value.toObject().as<ModuleRequestObject>();
}

/* static */
bool RequestedModuleObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<RequestedModuleObject>();
}

/* static */
RequestedModuleObject* RequestedModuleObject::create(JSContext* cx,
                                                     HandleObject moduleRequest,
                                                     uint32_t lineNumber,
                                                     uint32_t columnNumber) {
  RequestedModuleObject* self =
      NewObjectWithGivenProto<RequestedModuleObject>(cx, nullptr);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ModuleRequestSlot, ObjectValue(*moduleRequest));
  self->initReservedSlot(LineNumberSlot, NumberValue(lineNumber));
  self->initReservedSlot(ColumnNumberSlot, NumberValue(columnNumber));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// ResolvedBindingObject

/* static */ const JSClass ResolvedBindingObject::class_ = {
    "ResolvedBinding",
    JSCLASS_HAS_RESERVED_SLOTS(ResolvedBindingObject::SlotCount)};

ModuleObject* ResolvedBindingObject::module() const {
  Value value = getReservedSlot(ModuleSlot);
  return &value.toObject().as<ModuleObject>();
}

JSAtom* ResolvedBindingObject::bindingName() const {
  Value value = getReservedSlot(BindingNameSlot);
  return &value.toString()->asAtom();
}

/* static */
bool ResolvedBindingObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ResolvedBindingObject>();
}

/* static */
ResolvedBindingObject* ResolvedBindingObject::create(
    JSContext* cx, Handle<ModuleObject*> module, Handle<JSAtom*> bindingName) {
  ResolvedBindingObject* self =
      NewObjectWithGivenProto<ResolvedBindingObject>(cx, nullptr);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(ModuleSlot, ObjectValue(*module));
  self->initReservedSlot(BindingNameSlot, StringValue(bindingName));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// ModuleRequestObject
/* static */ const JSClass ModuleRequestObject::class_ = {
    "ModuleRequest",
    JSCLASS_HAS_RESERVED_SLOTS(ModuleRequestObject::SlotCount)};

DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ModuleRequestObject, specifier,
                                    SpecifierSlot)

ArrayObject* ModuleRequestObject::assertions() const {
  JSObject* obj = getReservedSlot(AssertionSlot).toObjectOrNull();
  if (!obj) {
    return nullptr;
  }

  return &obj->as<ArrayObject>();
}

/* static */
bool ModuleRequestObject::isInstance(HandleValue value) {
  return value.isObject() && value.toObject().is<ModuleRequestObject>();
}

/* static */
ModuleRequestObject* ModuleRequestObject::create(
    JSContext* cx, Handle<JSAtom*> specifier,
    Handle<ArrayObject*> maybeAssertions) {
  ModuleRequestObject* self =
      NewObjectWithGivenProto<ModuleRequestObject>(cx, nullptr);
  if (!self) {
    return nullptr;
  }

  self->initReservedSlot(SpecifierSlot, StringOrNullValue(specifier));
  self->initReservedSlot(AssertionSlot, ObjectOrNullValue(maybeAssertions));
  return self;
}

///////////////////////////////////////////////////////////////////////////
// IndirectBindingMap

IndirectBindingMap::Binding::Binding(ModuleEnvironmentObject* environment,
                                     jsid targetName, PropertyInfo prop)
    : environment(environment),
#ifdef DEBUG
      targetName(targetName),
#endif
      prop(prop) {
}

void IndirectBindingMap::trace(JSTracer* trc) {
  if (!map_) {
    return;
  }

  for (Map::Enum e(*map_); !e.empty(); e.popFront()) {
    Binding& b = e.front().value();
    TraceEdge(trc, &b.environment, "module bindings environment");
#ifdef DEBUG
    TraceEdge(trc, &b.targetName, "module bindings target name");
#endif
    mozilla::DebugOnly<jsid> prev(e.front().key());
    TraceEdge(trc, &e.front().mutableKey(), "module bindings binding name");
    MOZ_ASSERT(e.front().key() == prev);
  }
}

bool IndirectBindingMap::put(JSContext* cx, HandleId name,
                             Handle<ModuleEnvironmentObject*> environment,
                             HandleId targetName) {
  if (!map_) {
    map_.emplace(cx->zone());
  }

  mozilla::Maybe<PropertyInfo> prop = environment->lookup(cx, targetName);
  MOZ_ASSERT(prop.isSome());
  if (!map_->put(name, Binding(environment, targetName, *prop))) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool IndirectBindingMap::lookup(jsid name, ModuleEnvironmentObject** envOut,
                                mozilla::Maybe<PropertyInfo>* propOut) const {
  if (!map_) {
    return false;
  }

  auto ptr = map_->lookup(name);
  if (!ptr) {
    return false;
  }

  const Binding& binding = ptr->value();
  MOZ_ASSERT(binding.environment);
  MOZ_ASSERT(
      binding.environment->containsPure(binding.targetName, binding.prop));
  *envOut = binding.environment;
  *propOut = Some(binding.prop);
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
    JSContext* cx, Handle<ModuleObject*> module, Handle<ArrayObject*> exports,
    UniquePtr<IndirectBindingMap> bindings) {
  RootedValue priv(cx, ObjectValue(*module));
  ProxyOptions options;
  options.setLazyProto(true);
  Rooted<UniquePtr<IndirectBindingMap>> rootedBindings(cx, std::move(bindings));
  RootedObject object(
      cx, NewProxyObject(cx, &proxyHandler, priv, nullptr, options));
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

ArrayObject& ModuleNamespaceObject::exports() {
  return GetProxyReservedSlot(this, ExportsSlot).toObject().as<ArrayObject>();
}

IndirectBindingMap& ModuleNamespaceObject::bindings() {
  Value value = GetProxyReservedSlot(this, BindingsSlot);
  auto* bindings = static_cast<IndirectBindingMap*>(value.toPrivate());
  MOZ_ASSERT(bindings);
  return *bindings;
}

bool ModuleNamespaceObject::hasBindings() const {
  // Import bindings may not be present if we hit OOM in initialization.
  return !GetProxyReservedSlot(this, BindingsSlot).isUndefined();
}

bool ModuleNamespaceObject::addBinding(JSContext* cx,
                                       Handle<JSAtom*> exportedName,
                                       Handle<ModuleObject*> targetModule,
                                       Handle<JSAtom*> targetName) {
  Rooted<ModuleEnvironmentObject*> environment(
      cx, &targetModule->initialEnvironment());
  RootedId exportedNameId(cx, AtomToId(exportedName));
  RootedId targetNameId(cx, AtomToId(targetName));
  return bindings().put(cx, exportedNameId, environment, targetNameId);
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
    MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (id.isSymbol()) {
    if (id.isWellKnownSymbol(JS::SymbolCode::toStringTag)) {
      desc.set(Some(PropertyDescriptor::Data(StringValue(cx->names().Module))));
      return true;
    }

    desc.reset();
    return true;
  }

  const IndirectBindingMap& bindings = ns->bindings();
  ModuleEnvironmentObject* env;
  mozilla::Maybe<PropertyInfo> prop;
  if (!bindings.lookup(id, &env, &prop)) {
    // Not found.
    desc.reset();
    return true;
  }

  RootedValue value(cx, env->getSlot(prop->slot()));
  if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  desc.set(
      Some(PropertyDescriptor::Data(value, {JS::PropertyAttribute::Enumerable,
                                            JS::PropertyAttribute::Writable})));
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
  if (id.isSymbol()) {
    if (id.isWellKnownSymbol(JS::SymbolCode::toStringTag)) {
      RootedValue value(cx, StringValue(cx->names().Module));
      return ValidatePropertyDescriptor(cx, desc, false, false, false, value,
                                        result);
    }
    return result.fail(JSMSG_CANT_DEFINE_PROP_OBJECT_NOT_EXTENSIBLE);
  }

  const IndirectBindingMap& bindings =
      proxy->as<ModuleNamespaceObject>().bindings();
  ModuleEnvironmentObject* env;
  mozilla::Maybe<PropertyInfo> prop;
  if (!bindings.lookup(id, &env, &prop)) {
    return result.fail(JSMSG_CANT_DEFINE_PROP_OBJECT_NOT_EXTENSIBLE);
  }

  RootedValue value(cx, env->getSlot(prop->slot()));
  if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  return ValidatePropertyDescriptor(cx, desc, true, true, false, value, result);
}

bool ModuleNamespaceObject::ProxyHandler::has(JSContext* cx, HandleObject proxy,
                                              HandleId id, bool* bp) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (id.isSymbol()) {
    *bp = id.isWellKnownSymbol(JS::SymbolCode::toStringTag);
    return true;
  }

  *bp = ns->bindings().has(id);
  return true;
}

bool ModuleNamespaceObject::ProxyHandler::get(JSContext* cx, HandleObject proxy,
                                              HandleValue receiver, HandleId id,
                                              MutableHandleValue vp) const {
  Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
  if (id.isSymbol()) {
    if (id.isWellKnownSymbol(JS::SymbolCode::toStringTag)) {
      vp.setString(cx->names().Module);
      return true;
    }

    vp.setUndefined();
    return true;
  }

  ModuleEnvironmentObject* env;
  mozilla::Maybe<PropertyInfo> prop;
  if (!ns->bindings().lookup(id, &env, &prop)) {
    vp.setUndefined();
    return true;
  }

  RootedValue value(cx, env->getSlot(prop->slot()));
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
  if (id.isSymbol()) {
    if (id.isWellKnownSymbol(JS::SymbolCode::toStringTag)) {
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
  Rooted<ArrayObject*> exports(cx, &ns->exports());
  uint32_t count = exports->length();
  if (!props.reserve(props.length() + count + 1)) {
    return false;
  }

  Rooted<ValueVector> names(cx, ValueVector(cx));
  if (!names.resize(count) || !GetElements(cx, exports, count, names.begin())) {
    return false;
  }

  for (uint32_t i = 0; i < count; i++) {
    props.infallibleAppend(AtomToId(&names[i].toString()->asAtom()));
  }

  props.infallibleAppend(
      PropertyKey::Symbol(cx->wellKnownSymbols().toStringTag));

  return true;
}

void ModuleNamespaceObject::ProxyHandler::trace(JSTracer* trc,
                                                JSObject* proxy) const {
  auto& self = proxy->as<ModuleNamespaceObject>();

  if (self.hasBindings()) {
    self.bindings().trace(trc);
  }
}

void ModuleNamespaceObject::ProxyHandler::finalize(JS::GCContext* gcx,
                                                   JSObject* proxy) const {
  auto& self = proxy->as<ModuleNamespaceObject>();

  if (self.hasBindings()) {
    gcx->delete_(proxy, &self.bindings(), MemoryUse::ModuleBindingMap);
  }
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

// Declared as static function instead of ModuleObject method in order to
// avoid recursive #include dependency between frontend and VM.
static frontend::FunctionDeclarationVector* GetFunctionDeclarations(
    ModuleObject* module) {
  Value value = module->getReservedSlot(ModuleObject::FunctionDeclarationsSlot);
  if (value.isUndefined()) {
    return nullptr;
  }

  return static_cast<frontend::FunctionDeclarationVector*>(value.toPrivate());
}

static void InitFunctionDeclarations(
    ModuleObject* module, frontend::FunctionDeclarationVector&& decls) {
  *GetFunctionDeclarations(module) = std::move(decls);
}

/* static */
ModuleObject* ModuleObject::create(JSContext* cx) {
  Rooted<ModuleObject*> self(
      cx, NewObjectWithGivenProto<ModuleObject>(cx, nullptr));
  if (!self) {
    return nullptr;
  }

  IndirectBindingMap* bindings = cx->new_<IndirectBindingMap>();
  if (!bindings) {
    return nullptr;
  }

  InitReservedSlot(self, ImportBindingsSlot, bindings,
                   MemoryUse::ModuleBindingMap);

  frontend::FunctionDeclarationVector* funDecls =
      cx->new_<frontend::FunctionDeclarationVector>();
  if (!funDecls) {
    return nullptr;
  }

  self->initReservedSlot(FunctionDeclarationsSlot, PrivateValue(funDecls));
  return self;
}

/* static */
void ModuleObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  ModuleObject* self = &obj->as<ModuleObject>();
  if (self->hasImportBindings()) {
    gcx->delete_(obj, &self->importBindings(), MemoryUse::ModuleBindingMap);
  }
  if (frontend::FunctionDeclarationVector* funDecls =
          GetFunctionDeclarations(self)) {
    // Not tracked as these may move between zones on merge.
    gcx->deleteUntracked(funDecls);
  }
}

ModuleEnvironmentObject& ModuleObject::initialEnvironment() const {
  Value value = getReservedSlot(EnvironmentSlot);
  return value.toObject().as<ModuleEnvironmentObject>();
}

ModuleEnvironmentObject* ModuleObject::environment() const {
  // Note that this it's valid to call this even if there was an error
  // evaluating the module.

  // According to the spec the environment record is created during linking, but
  // we create it earlier than that.
  if (status() < ModuleStatus::Linked) {
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

bool ModuleObject::initAsyncSlots(JSContext* cx, bool hasTopLevelAwait,
                                  HandleObject asyncParentModulesList) {
  initReservedSlot(HasTopLevelAwaitSlot, BooleanValue(hasTopLevelAwait));
  initReservedSlot(AsyncParentModulesSlot,
                   ObjectValue(*asyncParentModulesList));
  return true;
}

static uint32_t NextPostOrder(JSRuntime* rt) {
  uint32_t ordinal = rt->moduleAsyncEvaluatingPostOrder;
  MOZ_ASSERT(ordinal != ASYNC_EVALUATING_POST_ORDER_TRUE);
  MOZ_ASSERT(ordinal < MAX_UINT32);
  rt->moduleAsyncEvaluatingPostOrder++;
  return ordinal;
}

// Reset the runtime's moduleAsyncEvaluatingPostOrder counter when the last
// module that was async evaluating is finished.
//
// The graph is not re-entrant and any future modules will be independent from
// this one.
static void MaybeResetPostOrderCounter(JSRuntime* rt,
                                       uint32_t finishedPostOrder) {
  if (rt->moduleAsyncEvaluatingPostOrder == finishedPostOrder + 1) {
    rt->moduleAsyncEvaluatingPostOrder = ASYNC_EVALUATING_POST_ORDER_INIT;
  }
}

void ModuleObject::setAsyncEvaluating() {
  initReservedSlot(AsyncEvaluatingPostOrderSlot,
                   PrivateUint32Value(NextPostOrder(runtimeFromMainThread())));
}

void ModuleObject::initScriptSlots(HandleScript script) {
  MOZ_ASSERT(script);
  initReservedSlot(ScriptSlot, PrivateGCThingValue(script));
  initReservedSlot(ScriptSourceObjectSlot,
                   ObjectValue(*script->sourceObject()));
}

void ModuleObject::setInitialEnvironment(
    Handle<ModuleEnvironmentObject*> initialEnvironment) {
  initReservedSlot(EnvironmentSlot, ObjectValue(*initialEnvironment));
}

void ModuleObject::initStatusSlot() {
  initReservedSlot(StatusSlot, ModuleStatusValue(ModuleStatus::Unlinked));
}

void ModuleObject::initImportExportData(
    Handle<ArrayObject*> requestedModules, Handle<ArrayObject*> importEntries,
    Handle<ArrayObject*> localExportEntries,
    Handle<ArrayObject*> indirectExportEntries,
    Handle<ArrayObject*> starExportEntries) {
  initReservedSlot(RequestedModulesSlot, ObjectValue(*requestedModules));
  initReservedSlot(ImportEntriesSlot, ObjectValue(*importEntries));
  initReservedSlot(LocalExportEntriesSlot, ObjectValue(*localExportEntries));
  initReservedSlot(IndirectExportEntriesSlot,
                   ObjectValue(*indirectExportEntries));
  initReservedSlot(StarExportEntriesSlot, ObjectValue(*starExportEntries));
}

static bool FreezeObjectProperty(JSContext* cx, Handle<NativeObject*> obj,
                                 uint32_t slot) {
  RootedObject property(cx, &obj->getSlot(slot).toObject());
  return FreezeObject(cx, property);
}

/* static */
bool ModuleObject::Freeze(JSContext* cx, Handle<ModuleObject*> self) {
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
                                             Handle<NativeObject*> obj,
                                             uint32_t slot, bool* result) {
  RootedObject property(cx, &obj->getSlot(slot).toObject());
  return CheckObjectFrozen(cx, property, result);
}

/* static */ inline bool ModuleObject::AssertFrozen(
    JSContext* cx, Handle<ModuleObject*> self) {
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
  MOZ_ASSERT(status >= ModuleStatus::Unlinked &&
             status <= ModuleStatus::Evaluated_Error);
}

ModuleStatus ModuleObject::status() const {
  ModuleStatus status = ModuleStatus(getReservedSlot(StatusSlot).toInt32());
  AssertValidModuleStatus(status);
  if (status == ModuleStatus::Evaluated_Error) {
    return ModuleStatus::Evaluated;
  }
  return status;
}

void ModuleObject::setStatus(ModuleStatus newStatus) {
  AssertValidModuleStatus(newStatus);

  // Note that under OOM conditions we can fail the module linking process even
  // after modules have been marked as linked.
  MOZ_ASSERT((status() <= ModuleStatus::Linked &&
              newStatus == ModuleStatus::Unlinked) ||
                 newStatus > status(),
             "New module status inconsistent with current status");

  setReservedSlot(StatusSlot, ModuleStatusValue(newStatus));
}

bool ModuleObject::hasTopLevelAwait() const {
  return getReservedSlot(HasTopLevelAwaitSlot).toBoolean();
}

bool ModuleObject::isAsyncEvaluating() const {
  return !getReservedSlot(AsyncEvaluatingPostOrderSlot).isUndefined();
}

Maybe<uint32_t> ModuleObject::maybeDfsIndex() const {
  Value value = getReservedSlot(DFSIndexSlot);
  if (value.isUndefined()) {
    return mozilla::Nothing();
  }

  int32_t index = value.toInt32();
  MOZ_ASSERT(index >= 0);
  return Some(index);
}

uint32_t ModuleObject::dfsIndex() const { return maybeDfsIndex().value(); }

void ModuleObject::setDfsIndex(uint32_t index) {
  MOZ_ASSERT(index <= INT32_MAX);
  setReservedSlot(DFSIndexSlot, Int32Value(index));
}

Maybe<uint32_t> ModuleObject::maybeDfsAncestorIndex() const {
  Value value = getReservedSlot(DFSAncestorIndexSlot);
  if (value.isUndefined()) {
    return mozilla::Nothing();
  }

  int32_t index = value.toInt32();
  MOZ_ASSERT(index >= 0);
  return Some(index);
}

uint32_t ModuleObject::dfsAncestorIndex() const {
  return maybeDfsAncestorIndex().value();
}

void ModuleObject::setDfsAncestorIndex(uint32_t index) {
  MOZ_ASSERT(index <= INT32_MAX);
  setReservedSlot(DFSAncestorIndexSlot, Int32Value(index));
}

void ModuleObject::clearDfsIndexes() {
  setReservedSlot(DFSIndexSlot, UndefinedValue());
  setReservedSlot(DFSAncestorIndexSlot, UndefinedValue());
}

PromiseObject* ModuleObject::maybeTopLevelCapability() const {
  Value value = getReservedSlot(TopLevelCapabilitySlot);
  if (value.isUndefined()) {
    return nullptr;
  }

  return &value.toObject().as<PromiseObject>();
}

PromiseObject* ModuleObject::topLevelCapability() const {
  PromiseObject* capability = maybeTopLevelCapability();
  MOZ_RELEASE_ASSERT(capability);
  return capability;
}

PromiseObject* ModuleObject::createTopLevelCapability(
    JSContext* cx, Handle<ModuleObject*> module) {
  MOZ_ASSERT(module->getReservedSlot(TopLevelCapabilitySlot).isUndefined());
  Rooted<PromiseObject*> resultPromise(cx, CreatePromiseObjectForAsync(cx));
  if (!resultPromise) {
    return nullptr;
  }
  module->setInitialTopLevelCapability(resultPromise);
  return resultPromise;
}

void ModuleObject::setInitialTopLevelCapability(
    Handle<PromiseObject*> capability) {
  initReservedSlot(TopLevelCapabilitySlot, ObjectValue(*capability));
}

ListObject* ModuleObject::asyncParentModules() const {
  return &getReservedSlot(AsyncParentModulesSlot).toObject().as<ListObject>();
}

bool ModuleObject::appendAsyncParentModule(JSContext* cx,
                                           Handle<ModuleObject*> self,
                                           Handle<ModuleObject*> parent) {
  Rooted<Value> parentValue(cx, ObjectValue(*parent));
  return self->asyncParentModules()->append(cx, parentValue);
}

Maybe<uint32_t> ModuleObject::maybePendingAsyncDependencies() const {
  Value value = getReservedSlot(PendingAsyncDependenciesSlot);
  if (value.isUndefined()) {
    return Nothing();
  }

  return Some(value.toInt32());
}

uint32_t ModuleObject::pendingAsyncDependencies() const {
  return maybePendingAsyncDependencies().value();
}

bool ModuleObject::hasAsyncEvaluatingPostOrder() const {
  Value value = getReservedSlot(AsyncEvaluatingPostOrderSlot);
  return !value.isUndefined() &&
         value.toPrivateUint32() != ASYNC_EVALUATING_POST_ORDER_TRUE;
}

Maybe<uint32_t> ModuleObject::maybeAsyncEvaluatingPostOrder() const {
  Value value = getReservedSlot(AsyncEvaluatingPostOrderSlot);
  if (value.isUndefined()) {
    return Nothing();
  }

  return Some(value.toPrivateUint32());
}

uint32_t ModuleObject::getAsyncEvaluatingPostOrder() const {
  MOZ_ASSERT(hasAsyncEvaluatingPostOrder());
  return maybeAsyncEvaluatingPostOrder().value();
}

void ModuleObject::clearAsyncEvaluatingPostOrder() {
  MOZ_ASSERT(status() == ModuleStatus::Evaluated);

  JSRuntime* rt = runtimeFromMainThread();
  MaybeResetPostOrderCounter(rt, getAsyncEvaluatingPostOrder());

  return setReservedSlot(AsyncEvaluatingPostOrderSlot,
                         PrivateUint32Value(ASYNC_EVALUATING_POST_ORDER_TRUE));
}

void ModuleObject::setPendingAsyncDependencies(uint32_t newValue) {
  return setReservedSlot(PendingAsyncDependenciesSlot, NumberValue(newValue));
}

void ModuleObject::setCycleRoot(ModuleObject* cycleRoot) {
  return setReservedSlot(CycleRootSlot, ObjectValue(*cycleRoot));
}

ModuleObject* ModuleObject::getCycleRoot() const {
  Value cycleRoot = getReservedSlot(CycleRootSlot);
  MOZ_RELEASE_ASSERT(cycleRoot.isObject());
  return &cycleRoot.toObject().as<ModuleObject>();
}

bool ModuleObject::hasTopLevelCapability() const {
  return !getReservedSlot(TopLevelCapabilitySlot).isUndefined();
}

bool ModuleObject::hadEvaluationError() const {
  ModuleStatus fullStatus = ModuleStatus(getReservedSlot(StatusSlot).toInt32());
  return fullStatus == ModuleStatus::Evaluated_Error;
}

void ModuleObject::setEvaluationError(HandleValue newValue) {
  MOZ_ASSERT(status() != ModuleStatus::Unlinked);
  MOZ_ASSERT(!hadEvaluationError());

  setReservedSlot(StatusSlot, ModuleStatusValue(ModuleStatus::Evaluated_Error));
  setReservedSlot(EvaluationErrorSlot, newValue);

  MOZ_ASSERT(status() == ModuleStatus::Evaluated);
  MOZ_ASSERT(hadEvaluationError());
}

Value ModuleObject::maybeEvaluationError() const {
  return getReservedSlot(EvaluationErrorSlot);
}

Value ModuleObject::evaluationError() const {
  MOZ_ASSERT(hadEvaluationError());
  return maybeEvaluationError();
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

/* static */
void ModuleObject::trace(JSTracer* trc, JSObject* obj) {
  ModuleObject& module = obj->as<ModuleObject>();

  if (module.hasImportBindings()) {
    module.importBindings().trace(trc);
  }
}

/* static */
bool ModuleObject::instantiateFunctionDeclarations(JSContext* cx,
                                                   Handle<ModuleObject*> self) {
#ifdef DEBUG
  MOZ_ASSERT(self->status() == ModuleStatus::Linking);
  if (!AssertFrozen(cx, self)) {
    return false;
  }
#endif
  // |self| initially manages this vector.
  frontend::FunctionDeclarationVector* funDecls =
      GetFunctionDeclarations(self.get());
  if (!funDecls) {
    JS_ReportErrorASCII(
        cx, "Module function declarations have already been instantiated");
    return false;
  }

  Rooted<ModuleEnvironmentObject*> env(cx, &self->initialEnvironment());
  RootedObject obj(cx);
  RootedValue value(cx);
  RootedFunction fun(cx);
  Rooted<PropertyName*> name(cx);

  for (GCThingIndex funIndex : *funDecls) {
    fun.set(self->script()->getFunction(funIndex));
    obj = Lambda(cx, fun, env);
    if (!obj) {
      return false;
    }

    name = fun->explicitName()->asPropertyName();
    value = ObjectValue(*obj);
    if (!SetProperty(cx, env, name, value)) {
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
bool ModuleObject::execute(JSContext* cx, Handle<ModuleObject*> self) {
#ifdef DEBUG
  MOZ_ASSERT(self->status() == ModuleStatus::Evaluating ||
             self->status() == ModuleStatus::EvaluatingAsync ||
             self->status() == ModuleStatus::Evaluated);
  MOZ_ASSERT(!self->hadEvaluationError());
  if (!AssertFrozen(cx, self)) {
    return false;
  }
#endif

  RootedScript script(cx, self->script());

  auto guardA = mozilla::MakeScopeExit([&] {
    if (self->hasTopLevelAwait()) {
      // Handled in AsyncModuleExecutionFulfilled and
      // AsyncModuleExecutionRejected.
      return;
    }
    ModuleObject::onTopLevelEvaluationFinished(self);
  });

  Rooted<ModuleEnvironmentObject*> env(cx, self->environment());
  if (!env) {
    JS_ReportErrorASCII(cx,
                        "Module declarations have not yet been instantiated");
    return false;
  }

  Rooted<Value> ignored(cx);
  return Execute(cx, script, env, &ignored);
}

/* static */
void ModuleObject::onTopLevelEvaluationFinished(ModuleObject* module) {
  // ScriptSlot is used by debugger to access environments during evaluating
  // the top-level script.
  // Clear the reference at exit to prevent us keeping this alive unnecessarily.
  module->setReservedSlot(ScriptSlot, UndefinedValue());
}

/* static */
ModuleNamespaceObject* ModuleObject::createNamespace(JSContext* cx,
                                                     Handle<ModuleObject*> self,
                                                     HandleObject exports) {
  MOZ_ASSERT(!self->namespace_());
  MOZ_ASSERT(exports->is<ArrayObject>());

  auto bindings = cx->make_unique<IndirectBindingMap>();
  if (!bindings) {
    return nullptr;
  }

  auto* ns = ModuleNamespaceObject::create(cx, self, exports.as<ArrayObject>(),
                                           std::move(bindings));
  if (!ns) {
    return nullptr;
  }

  self->initReservedSlot(NamespaceSlot, ObjectValue(*ns));
  return ns;
}

/* static */
bool ModuleObject::createEnvironment(JSContext* cx,
                                     Handle<ModuleObject*> self) {
  Rooted<ModuleEnvironmentObject*> env(
      cx, ModuleEnvironmentObject::create(cx, self));
  if (!env) {
    return false;
  }

  self->setInitialEnvironment(env);
  return true;
}

///////////////////////////////////////////////////////////////////////////
// ModuleBuilder

ModuleBuilder::ModuleBuilder(JSContext* cx,
                             const frontend::EitherParser& eitherParser)
    : cx_(cx),
      eitherParser_(eitherParser),
      requestedModuleSpecifiers_(cx),
      importEntries_(cx),
      exportEntries_(cx),
      exportNames_(cx) {}

bool ModuleBuilder::noteFunctionDeclaration(JSContext* cx, uint32_t funIndex) {
  if (!functionDecls_.emplaceBack(funIndex)) {
    js::ReportOutOfMemory(cx);
    return false;
  }
  return true;
}

void ModuleBuilder::noteAsync(frontend::StencilModuleMetadata& metadata) {
  metadata.isAsync = true;
}

bool ModuleBuilder::buildTables(frontend::StencilModuleMetadata& metadata) {
  // https://tc39.es/ecma262/#sec-parsemodule
  // 15.2.1.17.1 ParseModule, Steps 4-11.

  // Step 4.
  metadata.requestedModules = std::move(requestedModules_);

  // Step 5.
  if (!metadata.importEntries.reserve(importEntries_.count())) {
    js::ReportOutOfMemory(cx_);
    return false;
  }
  for (auto r = importEntries_.all(); !r.empty(); r.popFront()) {
    frontend::StencilModuleEntry& entry = r.front().value();
    metadata.importEntries.infallibleAppend(entry);
  }

  // Steps 6-11.
  for (const frontend::StencilModuleEntry& exp : exportEntries_) {
    if (!exp.specifier) {
      frontend::StencilModuleEntry* importEntry = importEntryFor(exp.localName);
      if (!importEntry) {
        if (!metadata.localExportEntries.append(exp)) {
          js::ReportOutOfMemory(cx_);
          return false;
        }
      } else {
        if (!importEntry->importName) {
          if (!metadata.localExportEntries.append(exp)) {
            js::ReportOutOfMemory(cx_);
            return false;
          }
        } else {
          // All names should have already been marked as used-by-stencil.
          auto entry = frontend::StencilModuleEntry::exportFromEntry(
              importEntry->specifier, importEntry->importName, exp.exportName,
              exp.lineno, exp.column);
          if (!metadata.indirectExportEntries.append(entry)) {
            js::ReportOutOfMemory(cx_);
            return false;
          }
        }
      }
    } else if (!exp.importName && !exp.exportName) {
      if (!metadata.starExportEntries.append(exp)) {
        js::ReportOutOfMemory(cx_);
        return false;
      }
    } else {
      if (!metadata.indirectExportEntries.append(exp)) {
        js::ReportOutOfMemory(cx_);
        return false;
      }
    }
  }

  return true;
}

void ModuleBuilder::finishFunctionDecls(
    frontend::StencilModuleMetadata& metadata) {
  metadata.functionDecls = std::move(functionDecls_);
}

enum class ModuleArrayType {
  ImportEntryObject,
  ExportEntryObject,
  RequestedModuleObject,
};

static ArrayObject* ModuleBuilderInitArray(
    JSContext* cx, frontend::CompilationAtomCache& atomCache,
    ModuleArrayType arrayType,
    const frontend::StencilModuleMetadata::EntryVector& vector) {
  Rooted<ArrayObject*> resultArray(
      cx, NewDenseFullyAllocatedArray(cx, vector.length()));
  if (!resultArray) {
    return nullptr;
  }

  resultArray->ensureDenseInitializedLength(0, vector.length());

  Rooted<JSAtom*> specifier(cx);
  Rooted<JSAtom*> localName(cx);
  Rooted<JSAtom*> importName(cx);
  Rooted<JSAtom*> exportName(cx);
  RootedObject req(cx);
  RootedObject moduleRequest(cx);
  Rooted<ArrayObject*> assertionArray(cx);
  Rooted<PlainObject*> assertionObject(cx);
  RootedId assertionKey(cx);
  RootedValue assertionValue(cx);

  for (uint32_t i = 0; i < vector.length(); ++i) {
    const frontend::StencilModuleEntry& entry = vector[i];

    if (entry.specifier) {
      specifier = atomCache.getExistingAtomAt(cx, entry.specifier);
      MOZ_ASSERT(specifier);
    } else {
      MOZ_ASSERT(!specifier);
    }

    if (entry.localName) {
      localName = atomCache.getExistingAtomAt(cx, entry.localName);
      MOZ_ASSERT(localName);
    } else {
      MOZ_ASSERT(!localName);
    }

    if (entry.importName) {
      importName = atomCache.getExistingAtomAt(cx, entry.importName);
      MOZ_ASSERT(importName);
    } else {
      importName = nullptr;
    }

    if (entry.exportName) {
      exportName = atomCache.getExistingAtomAt(cx, entry.exportName);
      MOZ_ASSERT(exportName);
    } else {
      MOZ_ASSERT(!exportName);
    }

    uint32_t numberOfAssertions = entry.assertions.length();
    if (numberOfAssertions > 0) {
      assertionArray = NewDenseFullyAllocatedArray(cx, numberOfAssertions);
      if (!assertionArray) {
        return nullptr;
      }
      assertionArray->ensureDenseInitializedLength(0, numberOfAssertions);

      for (uint32_t j = 0; j < numberOfAssertions; ++j) {
        assertionObject = NewPlainObject(cx);
        if (!assertionObject) {
          return nullptr;
        }

        JSAtom* jsatom =
            atomCache.getExistingAtomAt(cx, entry.assertions[j].key);
        MOZ_ASSERT(jsatom);
        assertionKey = AtomToId(jsatom);

        jsatom = atomCache.getExistingAtomAt(cx, entry.assertions[j].value);
        MOZ_ASSERT(jsatom);
        assertionValue = StringValue(jsatom);

        if (!DefineDataProperty(cx, assertionObject, assertionKey,
                                assertionValue, JSPROP_ENUMERATE)) {
          return nullptr;
        }

        assertionArray->initDenseElement(j, ObjectValue(*assertionObject));
      }
    } else {
      assertionArray = nullptr;
    }

    moduleRequest = ModuleRequestObject::create(cx, specifier, assertionArray);
    if (!moduleRequest) {
      return nullptr;
    }

    switch (arrayType) {
      case ModuleArrayType::ImportEntryObject:
        MOZ_ASSERT(localName);
        req = ImportEntryObject::create(cx, moduleRequest, importName,
                                        localName, entry.lineno, entry.column);
        break;
      case ModuleArrayType::ExportEntryObject:
        req =
            ExportEntryObject::create(cx, exportName, moduleRequest, importName,
                                      localName, entry.lineno, entry.column);
        break;
      case ModuleArrayType::RequestedModuleObject:
        req = RequestedModuleObject::create(cx, moduleRequest, entry.lineno,
                                            entry.column);
        // TODO: Make this consistent with other object types.
        if (req && !FreezeObject(cx, req)) {
          return nullptr;
        }
        break;
    }
    if (!req) {
      return nullptr;
    }
    resultArray->initDenseElement(i, ObjectValue(*req));
  }

  return resultArray;
}

// Use StencilModuleMetadata data to fill in ModuleObject
bool frontend::StencilModuleMetadata::initModule(
    JSContext* cx, frontend::CompilationAtomCache& atomCache,
    JS::Handle<ModuleObject*> module) const {
  Rooted<ArrayObject*> requestedModulesObject(
      cx, ModuleBuilderInitArray(cx, atomCache,
                                 ModuleArrayType::RequestedModuleObject,
                                 requestedModules));
  if (!requestedModulesObject) {
    return false;
  }

  Rooted<ArrayObject*> importEntriesObject(
      cx,
      ModuleBuilderInitArray(cx, atomCache, ModuleArrayType::ImportEntryObject,
                             importEntries));
  if (!importEntriesObject) {
    return false;
  }

  Rooted<ArrayObject*> localExportEntriesObject(
      cx,
      ModuleBuilderInitArray(cx, atomCache, ModuleArrayType::ExportEntryObject,
                             localExportEntries));
  if (!localExportEntriesObject) {
    return false;
  }

  Rooted<ArrayObject*> indirectExportEntriesObject(
      cx,
      ModuleBuilderInitArray(cx, atomCache, ModuleArrayType::ExportEntryObject,
                             indirectExportEntries));
  if (!indirectExportEntriesObject) {
    return false;
  }

  Rooted<ArrayObject*> starExportEntriesObject(
      cx,
      ModuleBuilderInitArray(cx, atomCache, ModuleArrayType::ExportEntryObject,
                             starExportEntries));
  if (!starExportEntriesObject) {
    return false;
  }

  // Copy the vector of declarations to the ModuleObject.
  FunctionDeclarationVector functionDeclsCopy;
  if (!functionDeclsCopy.appendAll(functionDecls)) {
    js::ReportOutOfMemory(cx);
    return false;
  }
  InitFunctionDeclarations(module.get(), std::move(functionDeclsCopy));

  Rooted<ListObject*> asyncParentModulesList(cx, ListObject::create(cx));
  if (!asyncParentModulesList) {
    return false;
  }

  if (!module->initAsyncSlots(cx, isAsync, asyncParentModulesList)) {
    return false;
  }

  module->initImportExportData(
      requestedModulesObject, importEntriesObject, localExportEntriesObject,
      indirectExportEntriesObject, starExportEntriesObject);

  return true;
}

static bool CallSupportedAssertionsHook(JSContext* cx,
                                        JS::ImportAssertionVector& values) {
  JS::SupportedAssertionsHook hook = cx->runtime()->supportedAssertionsHook;
  if (!hook) {
    return true;
  }

  return hook(cx, values);
}

bool ModuleBuilder::isAssertionSupported(JS::ImportAssertion supportedAssertion,
                                         frontend::TaggedParserAtomIndex key) {
  if (!key.isWellKnownAtomId()) {
    return false;
  }

  bool result = false;

  switch (supportedAssertion) {
    case JS::ImportAssertion::Type:
      result = key.toWellKnownAtomId() == WellKnownAtomId::type;
      break;
  }

  return result;
}

bool ModuleBuilder::processAssertions(frontend::StencilModuleEntry& entry,
                                      frontend::ListNode* assertionList) {
  using namespace js::frontend;

  JS::ImportAssertionVector supportedAssertions;

  bool succeeded = CallSupportedAssertionsHook(cx_, supportedAssertions);
  if (!succeeded) {
    return false;
  }

  for (ParseNode* assertionItem : assertionList->contents()) {
    BinaryNode* assertion = &assertionItem->as<BinaryNode>();
    MOZ_ASSERT(assertion->isKind(ParseNodeKind::ImportAssertion));

    auto key = assertion->left()->as<NameNode>().atom();
    auto value = assertion->right()->as<NameNode>().atom();

    for (JS::ImportAssertion assertion : supportedAssertions) {
      if (isAssertionSupported(assertion, key)) {
        markUsedByStencil(key);
        markUsedByStencil(value);

        StencilModuleAssertion assertionStencil(key, value);
        if (!entry.assertions.append(assertionStencil)) {
          js::ReportOutOfMemory(cx_);
          return false;
        }
      }
    }
  }

  return true;
}

bool ModuleBuilder::processImport(frontend::BinaryNode* importNode) {
  using namespace js::frontend;

  MOZ_ASSERT(importNode->isKind(ParseNodeKind::ImportDecl));

  auto* specList = &importNode->left()->as<ListNode>();
  MOZ_ASSERT(specList->isKind(ParseNodeKind::ImportSpecList));

  auto* moduleRequest = &importNode->right()->as<BinaryNode>();
  MOZ_ASSERT(moduleRequest->isKind(ParseNodeKind::ImportModuleRequest));

  auto* moduleSpec = &moduleRequest->left()->as<NameNode>();
  MOZ_ASSERT(moduleSpec->isKind(ParseNodeKind::StringExpr));

  auto* assertionList = &moduleRequest->right()->as<ListNode>();
  MOZ_ASSERT(assertionList->isKind(ParseNodeKind::ImportAssertionList));

  auto module = moduleSpec->atom();
  if (!maybeAppendRequestedModule(module, moduleSpec, assertionList)) {
    return false;
  }

  for (ParseNode* item : specList->contents()) {
    uint32_t line;
    uint32_t column;
    eitherParser_.computeLineAndColumn(item->pn_pos.begin, &line, &column);

    StencilModuleEntry entry;
    TaggedParserAtomIndex localName;
    if (item->isKind(ParseNodeKind::ImportSpec)) {
      auto* spec = &item->as<BinaryNode>();

      auto* importNameNode = &spec->left()->as<NameNode>();
      auto* localNameNode = &spec->right()->as<NameNode>();

      auto importName = importNameNode->atom();
      localName = localNameNode->atom();

      markUsedByStencil(module);
      markUsedByStencil(localName);
      markUsedByStencil(importName);
      entry = StencilModuleEntry::importEntry(module, localName, importName,
                                              line, column);
    } else {
      MOZ_ASSERT(item->isKind(ParseNodeKind::ImportNamespaceSpec));
      auto* spec = &item->as<UnaryNode>();

      auto* localNameNode = &spec->kid()->as<NameNode>();

      localName = localNameNode->atom();

      markUsedByStencil(module);
      markUsedByStencil(localName);
      entry = StencilModuleEntry::importNamespaceEntry(module, localName, line,
                                                       column);
    }

    if (!processAssertions(entry, assertionList)) {
      return false;
    }

    if (!importEntries_.put(localName, entry)) {
      return false;
    }
  }

  return true;
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
    auto localName = TaggedParserAtomIndex::WellKnown::default_();
    auto exportName = TaggedParserAtomIndex::WellKnown::default_();
    return appendExportEntry(exportName, localName);
  }

  switch (kid->getKind()) {
    case ParseNodeKind::ExportSpecList: {
      MOZ_ASSERT(!isDefault);
      for (ParseNode* item : kid->as<ListNode>().contents()) {
        BinaryNode* spec = &item->as<BinaryNode>();
        MOZ_ASSERT(spec->isKind(ParseNodeKind::ExportSpec));

        NameNode* localNameNode = &spec->left()->as<NameNode>();
        NameNode* exportNameNode = &spec->right()->as<NameNode>();

        auto localName = localNameNode->atom();
        auto exportName = exportNameNode->atom();

        if (!appendExportEntry(exportName, localName, spec)) {
          return false;
        }
      }
      break;
    }

    case ParseNodeKind::ClassDecl: {
      const ClassNode& cls = kid->as<ClassNode>();
      MOZ_ASSERT(cls.names());
      auto localName = cls.names()->innerBinding()->atom();
      auto exportName =
          isDefault ? TaggedParserAtomIndex::WellKnown::default_() : localName;
      if (!appendExportEntry(exportName, localName)) {
        return false;
      }
      break;
    }

    case ParseNodeKind::VarStmt:
    case ParseNodeKind::ConstDecl:
    case ParseNodeKind::LetDecl: {
      for (ParseNode* binding : kid->as<ListNode>().contents()) {
        if (binding->isKind(ParseNodeKind::AssignExpr)) {
          binding = binding->as<AssignmentNode>().left();
        } else {
          MOZ_ASSERT(binding->isKind(ParseNodeKind::Name));
        }

        if (binding->isKind(ParseNodeKind::Name)) {
          auto localName = binding->as<NameNode>().atom();
          auto exportName = isDefault
                                ? TaggedParserAtomIndex::WellKnown::default_()
                                : localName;
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
      auto localName = box->explicitName();
      auto exportName =
          isDefault ? TaggedParserAtomIndex::WellKnown::default_() : localName;
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
    auto name = binding->as<NameNode>().atom();
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

  auto* specList = &exportNode->left()->as<ListNode>();
  MOZ_ASSERT(specList->isKind(ParseNodeKind::ExportSpecList));

  auto* moduleRequest = &exportNode->right()->as<BinaryNode>();
  MOZ_ASSERT(moduleRequest->isKind(ParseNodeKind::ImportModuleRequest));

  auto* moduleSpec = &moduleRequest->left()->as<NameNode>();
  MOZ_ASSERT(moduleSpec->isKind(ParseNodeKind::StringExpr));

  auto* assertionList = &moduleRequest->right()->as<ListNode>();
  MOZ_ASSERT(assertionList->isKind(ParseNodeKind::ImportAssertionList));

  auto module = moduleSpec->atom();

  if (!maybeAppendRequestedModule(module, moduleSpec, assertionList)) {
    return false;
  }

  for (ParseNode* spec : specList->contents()) {
    uint32_t line;
    uint32_t column;
    eitherParser_.computeLineAndColumn(spec->pn_pos.begin, &line, &column);

    StencilModuleEntry entry;
    TaggedParserAtomIndex exportName;
    if (spec->isKind(ParseNodeKind::ExportSpec)) {
      auto* importNameNode = &spec->as<BinaryNode>().left()->as<NameNode>();
      auto* exportNameNode = &spec->as<BinaryNode>().right()->as<NameNode>();

      auto importName = importNameNode->atom();
      exportName = exportNameNode->atom();

      markUsedByStencil(module);
      markUsedByStencil(importName);
      markUsedByStencil(exportName);
      entry = StencilModuleEntry::exportFromEntry(module, importName,
                                                  exportName, line, column);
    } else if (spec->isKind(ParseNodeKind::ExportNamespaceSpec)) {
      auto* exportNameNode = &spec->as<UnaryNode>().kid()->as<NameNode>();

      exportName = exportNameNode->atom();

      markUsedByStencil(module);
      markUsedByStencil(exportName);
      entry = StencilModuleEntry::exportNamespaceFromEntry(module, exportName,
                                                           line, column);
    } else {
      MOZ_ASSERT(spec->isKind(ParseNodeKind::ExportBatchSpecStmt));

      markUsedByStencil(module);
      entry = StencilModuleEntry::exportBatchFromEntry(module, line, column);
    }

    if (!exportEntries_.append(entry)) {
      return false;
    }
    if (exportName && !exportNames_.put(exportName)) {
      return false;
    }
  }

  return true;
}

frontend::StencilModuleEntry* ModuleBuilder::importEntryFor(
    frontend::TaggedParserAtomIndex localName) const {
  MOZ_ASSERT(localName);
  auto ptr = importEntries_.lookup(localName);
  if (!ptr) {
    return nullptr;
  }

  return &ptr->value();
}

bool ModuleBuilder::hasExportedName(
    frontend::TaggedParserAtomIndex name) const {
  MOZ_ASSERT(name);
  return exportNames_.has(name);
}

bool ModuleBuilder::appendExportEntry(
    frontend::TaggedParserAtomIndex exportName,
    frontend::TaggedParserAtomIndex localName, frontend::ParseNode* node) {
  uint32_t line = 0;
  uint32_t column = 0;
  if (node) {
    eitherParser_.computeLineAndColumn(node->pn_pos.begin, &line, &column);
  }

  markUsedByStencil(localName);
  markUsedByStencil(exportName);
  auto entry = frontend::StencilModuleEntry::exportAsEntry(
      localName, exportName, line, column);
  if (!exportEntries_.append(entry)) {
    return false;
  }

  if (!exportNames_.put(exportName)) {
    return false;
  }

  return true;
}

bool ModuleBuilder::maybeAppendRequestedModule(
    frontend::TaggedParserAtomIndex specifier, frontend::ParseNode* node,
    frontend::ListNode* assertionList) {
  if (requestedModuleSpecifiers_.has(specifier)) {
    return true;
  }

  uint32_t line;
  uint32_t column;
  eitherParser_.computeLineAndColumn(node->pn_pos.begin, &line, &column);

  markUsedByStencil(specifier);
  auto entry =
      frontend::StencilModuleEntry::moduleRequest(specifier, line, column);

  if (!processAssertions(entry, assertionList)) {
    return false;
  }

  if (!requestedModules_.append(entry)) {
    js::ReportOutOfMemory(cx_);
    return false;
  }

  return requestedModuleSpecifiers_.put(specifier);
}

void ModuleBuilder::markUsedByStencil(frontend::TaggedParserAtomIndex name) {
  // Imported/exported identifiers must be atomized.
  eitherParser_.parserAtoms().markUsedByStencil(
      name, frontend::ParserAtom::Atomize::Yes);
}

template <typename T>
ArrayObject* js::CreateArray(JSContext* cx,
                             const JS::Rooted<GCVector<T>>& vector) {
  uint32_t length = vector.length();
  Rooted<ArrayObject*> array(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!array) {
    return nullptr;
  }

  array->setDenseInitializedLength(length);
  for (uint32_t i = 0; i < length; i++) {
    array->initDenseElement(i, ObjectValue(*vector[i]));
  }

  return array;
}

JSObject* js::GetOrCreateModuleMetaObject(JSContext* cx,
                                          HandleObject moduleArg) {
  Handle<ModuleObject*> module = moduleArg.as<ModuleObject>();
  if (JSObject* obj = module->metaObject()) {
    return obj;
  }

  RootedObject metaObject(cx, NewPlainObjectWithProto(cx, nullptr));
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

ModuleObject* js::CallModuleResolveHook(JSContext* cx,
                                        HandleValue referencingPrivate,
                                        HandleObject moduleRequest) {
  JS::ModuleResolveHook moduleResolveHook = cx->runtime()->moduleResolveHook;
  if (!moduleResolveHook) {
    JS_ReportErrorASCII(cx, "Module resolve hook not set");
    return nullptr;
  }

  RootedObject result(cx,
                      moduleResolveHook(cx, referencingPrivate, moduleRequest));
  if (!result) {
    return nullptr;
  }

  if (!result->is<ModuleObject>()) {
    JS_ReportErrorASCII(cx, "Module resolve hook did not return Module object");
    return nullptr;
  }

  return &result->as<ModuleObject>();
}

bool ModuleObject::topLevelCapabilityResolve(JSContext* cx,
                                             Handle<ModuleObject*> module) {
  RootedValue rval(cx);
  Rooted<PromiseObject*> promise(
      cx, &module->topLevelCapability()->as<PromiseObject>());
  return AsyncFunctionReturned(cx, promise, rval);
}

bool ModuleObject::topLevelCapabilityReject(JSContext* cx,
                                            Handle<ModuleObject*> module,
                                            HandleValue error) {
  Rooted<PromiseObject*> promise(
      cx, &module->topLevelCapability()->as<PromiseObject>());
  return AsyncFunctionThrown(cx, promise, error);
}

// https://tc39.es/proposal-import-assertions/#sec-evaluate-import-call
// NOTE: The caller needs to handle the promise.
static bool EvaluateDynamicImportOptions(
    JSContext* cx, HandleValue optionsArg,
    MutableHandle<ArrayObject*> assertionArrayArg) {
  // Step 10. If options is not undefined, then.
  if (optionsArg.isUndefined()) {
    return true;
  }

  // Step 10.a. If Type(options) is not Object,
  if (!optionsArg.isObject()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE, "import",
        "object or undefined", InformalValueTypeName(optionsArg));
    return false;
  }

  RootedObject assertWrapperObject(cx, &optionsArg.toObject());
  RootedValue assertValue(cx);

  // Step 10.b. Let assertionsObj be Get(options, "assert").
  RootedId assertId(cx, NameToId(cx->names().assert_));
  if (!GetProperty(cx, assertWrapperObject, assertWrapperObject, assertId,
                   &assertValue)) {
    return false;
  }

  // Step 10.d. If assertionsObj is not undefined.
  if (assertValue.isUndefined()) {
    return true;
  }

  // Step 10.d.i. If Type(assertionsObj) is not Object.
  if (!assertValue.isObject()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE, "import",
        "object or undefined", InformalValueTypeName(assertValue));
    return false;
  }

  // Step 10.d.i. Let keys be EnumerableOwnPropertyNames(assertionsObj, key).
  RootedObject assertObject(cx, &assertValue.toObject());
  RootedIdVector assertions(cx);
  if (!GetPropertyKeys(cx, assertObject, JSITER_OWNONLY, &assertions)) {
    return false;
  }

  uint32_t numberOfAssertions = assertions.length();
  if (numberOfAssertions == 0) {
    return true;
  }

  // Step 9 (reordered). Let assertions be a new empty List.
  Rooted<ArrayObject*> assertionArray(
      cx, NewDenseFullyAllocatedArray(cx, numberOfAssertions));
  if (!assertionArray) {
    return false;
  }
  assertionArray->ensureDenseInitializedLength(0, numberOfAssertions);

  // Step 10.d.iv. Let supportedAssertions be
  // !HostGetSupportedImportAssertions().
  JS::ImportAssertionVector supportedAssertions;
  bool succeeded = CallSupportedAssertionsHook(cx, supportedAssertions);
  if (!succeeded) {
    return false;
  }

  size_t numberOfValidAssertions = 0;

  // Step 10.d.v. For each String key of keys,
  RootedId key(cx);
  for (size_t i = 0; i < numberOfAssertions; i++) {
    key = assertions[i];

    // Step 10.d.v.1. Let value be Get(assertionsObj, key).
    RootedValue value(cx);
    if (!GetProperty(cx, assertObject, assertObject, key, &value)) {
      return false;
    }

    // Step 10.d.v.3. If Type(value) is not String, then.
    if (!value.isString()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_NOT_EXPECTED_TYPE, "import", "string",
                                InformalValueTypeName(value));
      return false;
    }

    // Step 10.d.v.4. If supportedAssertions contains key, then Append {
    // [[Key]]: key, [[Value]]: value } to assertions.
    for (JS::ImportAssertion assertion : supportedAssertions) {
      bool supported = false;
      switch (assertion) {
        case JS::ImportAssertion::Type: {
          supported = key.toAtom() == cx->names().type;
        } break;
      }

      if (supported) {
        Rooted<PlainObject*> assertionObj(cx, NewPlainObject(cx));
        if (!assertionObj) {
          return false;
        }

        if (!DefineDataProperty(cx, assertionObj, key, value,
                                JSPROP_ENUMERATE)) {
          return false;
        }

        assertionArray->initDenseElement(numberOfValidAssertions,
                                         ObjectValue(*assertionObj));
        ++numberOfValidAssertions;
      }
    }
  }

  if (numberOfValidAssertions == 0) {
    return true;
  }

  assertionArray->setLength(numberOfValidAssertions);
  assertionArrayArg.set(assertionArray);

  return true;
}

JSObject* js::StartDynamicModuleImport(JSContext* cx, HandleScript script,
                                       HandleValue specifierArg,
                                       HandleValue optionsArg) {
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

  RootedValue referencingPrivate(cx, script->sourceObject()->getPrivate());
  cx->runtime()->addRefScriptPrivate(referencingPrivate);

  Rooted<JSAtom*> specifierAtom(cx, AtomizeString(cx, specifier));
  if (!specifierAtom) {
    if (!RejectPromiseWithPendingError(cx, promise)) {
      return nullptr;
    }
    return promise;
  }

  Rooted<ArrayObject*> assertionArray(cx);
  if (!EvaluateDynamicImportOptions(cx, optionsArg, &assertionArray)) {
    if (!RejectPromiseWithPendingError(cx, promise)) {
      return nullptr;
    }
    return promise;
  }

  RootedObject moduleRequest(
      cx, ModuleRequestObject::create(cx, specifierAtom, assertionArray));
  if (!moduleRequest) {
    if (!RejectPromiseWithPendingError(cx, promise)) {
      return nullptr;
    }
    return promise;
  }

  if (!importHook(cx, referencingPrivate, moduleRequest, promise)) {
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

static bool OnRootModuleRejected(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  HandleValue error = args.get(0);

  ReportExceptionClosure reportExn(error);
  PrepareScriptEnvironmentAndInvoke(cx, cx->global(), reportExn);

  args.rval().setUndefined();
  return true;
};

bool js::OnModuleEvaluationFailure(JSContext* cx,
                                   HandleObject evaluationPromise,
                                   JS::ModuleErrorBehaviour errorBehaviour) {
  if (evaluationPromise == nullptr) {
    return false;
  }

  // To allow module evaluation to happen synchronously throw the error
  // immediately. This assumes that any error will already have caused the
  // promise to be rejected, and doesn't support top-level await.
  if (errorBehaviour == JS::ThrowModuleErrorsSync) {
    JS::PromiseState state = JS::GetPromiseState(evaluationPromise);
    MOZ_DIAGNOSTIC_ASSERT(state == JS::PromiseState::Rejected ||
                          state == JS::PromiseState::Fulfilled);

    JS::SetSettledPromiseIsHandled(cx, evaluationPromise);
    if (state == JS::PromiseState::Fulfilled) {
      return true;
    }

    RootedValue error(cx, JS::GetPromiseResult(evaluationPromise));
    JS_SetPendingException(cx, error);
    return false;
  }

  RootedFunction onRejected(
      cx, NewHandler(cx, OnRootModuleRejected, evaluationPromise));
  if (!onRejected) {
    return false;
  }

  return JS::AddPromiseReactions(cx, evaluationPromise, nullptr, onRejected);
}

// Adjustment for Top-level await;
// See: https://github.com/tc39/proposal-dynamic-import/pull/71/files
static bool OnResolvedDynamicModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.get(0).isUndefined());

  // This is a hack to allow us to have the 2 extra variables needed
  // for FinishDynamicModuleImport in the resolve callback.
  Rooted<ListObject*> resolvedModuleParams(cx,
                                           ExtraFromHandler<ListObject>(args));
  MOZ_ASSERT(resolvedModuleParams->length() == 2);
  RootedValue referencingPrivate(cx, resolvedModuleParams->get(0));

  Rooted<JSAtom*> specifier(
      cx, AtomizeString(cx, resolvedModuleParams->get(1).toString()));
  if (!specifier) {
    return false;
  }

  Rooted<PromiseObject*> promise(cx, TargetFromHandler<PromiseObject>(args));

  auto releasePrivate = mozilla::MakeScopeExit(
      [&] { cx->runtime()->releaseScriptPrivate(referencingPrivate); });

  RootedObject moduleRequest(
      cx, ModuleRequestObject::create(cx, specifier, nullptr));
  if (!moduleRequest) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  RootedObject result(
      cx, CallModuleResolveHook(cx, referencingPrivate, moduleRequest));

  if (!result) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  Rooted<ModuleObject*> module(cx, &result->as<ModuleObject>());
  if (module->status() != ModuleStatus::EvaluatingAsync &&
      module->status() != ModuleStatus::Evaluated) {
    JS_ReportErrorASCII(
        cx, "Unevaluated or errored module returned by module resolve hook");
    return RejectPromiseWithPendingError(cx, promise);
  }

  MOZ_ASSERT(module->getCycleRoot()
                 ->topLevelCapability()
                 ->as<PromiseObject>()
                 .state() == JS::PromiseState::Fulfilled);

  RootedObject ns(cx, GetOrCreateModuleNamespace(cx, module));
  if (!ns) {
    return RejectPromiseWithPendingError(cx, promise);
  }

  args.rval().setUndefined();
  RootedValue value(cx, ObjectValue(*ns));
  return PromiseObject::resolve(cx, promise, value);
};

static bool OnRejectedDynamicModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  HandleValue error = args.get(0);

  RootedValue referencingPrivate(cx, ExtraValueFromHandler(args));
  Rooted<PromiseObject*> promise(cx, TargetFromHandler<PromiseObject>(args));

  auto releasePrivate = mozilla::MakeScopeExit(
      [&] { cx->runtime()->releaseScriptPrivate(referencingPrivate); });

  args.rval().setUndefined();
  return PromiseObject::reject(cx, promise, error);
};

bool FinishDynamicModuleImport_impl(JSContext* cx,
                                    HandleObject evaluationPromise,
                                    HandleValue referencingPrivate,
                                    HandleObject moduleRequest,
                                    HandleObject promiseArg) {
  Rooted<ListObject*> resolutionArgs(cx, ListObject::create(cx));
  if (!resolutionArgs->append(cx, referencingPrivate)) {
    return false;
  }
  Rooted<Value> stringValue(
      cx, StringValue(moduleRequest->as<ModuleRequestObject>().specifier()));
  if (!resolutionArgs->append(cx, stringValue)) {
    return false;
  }

  Rooted<Value> resolutionArgsValue(cx, ObjectValue(*resolutionArgs));

  RootedFunction onResolved(
      cx, NewHandlerWithExtraValue(cx, OnResolvedDynamicModule, promiseArg,
                                   resolutionArgsValue));
  if (!onResolved) {
    return false;
  }

  RootedFunction onRejected(
      cx, NewHandlerWithExtraValue(cx, OnRejectedDynamicModule, promiseArg,
                                   referencingPrivate));
  if (!onRejected) {
    return false;
  }

  return JS::AddPromiseReactionsIgnoringUnhandledRejection(
      cx, evaluationPromise, onResolved, onRejected);
}

bool js::FinishDynamicModuleImport(JSContext* cx,
                                   HandleObject evaluationPromise,
                                   HandleValue referencingPrivate,
                                   HandleObject moduleRequest,
                                   HandleObject promiseArg) {
  // If we do not have an evaluation promise or a module request for the module,
  // we can assume that evaluation has failed or been interrupted -- we can
  // reject the dynamic module.
  auto releasePrivate = mozilla::MakeScopeExit(
      [&] { cx->runtime()->releaseScriptPrivate(referencingPrivate); });

  if (!evaluationPromise || !moduleRequest) {
    Handle<PromiseObject*> promise = promiseArg.as<PromiseObject>();
    return RejectPromiseWithPendingError(cx, promise);
  }

  if (!FinishDynamicModuleImport_impl(cx, evaluationPromise, referencingPrivate,
                                      moduleRequest, promiseArg)) {
    return false;
  }

  releasePrivate.release();
  return true;
}
