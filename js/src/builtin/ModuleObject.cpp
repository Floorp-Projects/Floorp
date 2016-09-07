/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/ModuleObject.h"

#include "builtin/SelfHostingDefines.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "gc/Policy.h"
#include "gc/Tracer.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::frontend;

static_assert(MODULE_STATE_FAILED < MODULE_STATE_PARSED &&
              MODULE_STATE_PARSED < MODULE_STATE_INSTANTIATED &&
              MODULE_STATE_INSTANTIATED < MODULE_STATE_EVALUATED,
              "Module states are ordered incorrectly");

template<typename T, Value ValueGetter(const T* obj)>
static bool
ModuleValueGetterImpl(JSContext* cx, const CallArgs& args)
{
    args.rval().set(ValueGetter(&args.thisv().toObject().as<T>()));
    return true;
}

template<typename T, Value ValueGetter(const T* obj)>
static bool
ModuleValueGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<T::isInstance, ModuleValueGetterImpl<T, ValueGetter>>(cx, args);
}

#define DEFINE_GETTER_FUNCTIONS(cls, name, slot)                              \
    static Value                                                              \
    cls##_##name##Value(const cls* obj) {                                     \
        return obj->getFixedSlot(cls::slot);                                  \
    }                                                                         \
                                                                              \
    static bool                                                               \
    cls##_##name##Getter(JSContext* cx, unsigned argc, Value* vp)             \
    {                                                                         \
        return ModuleValueGetter<cls, cls##_##name##Value>(cx, argc, vp);     \
    }

#define DEFINE_ATOM_ACCESSOR_METHOD(cls, name)                                \
    JSAtom*                                                                   \
    cls::name() const                                                         \
    {                                                                         \
        Value value = cls##_##name##Value(this);                              \
        return &value.toString()->asAtom();                                   \
    }

#define DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(cls, name)                        \
    JSAtom*                                                                   \
    cls::name() const                                                         \
    {                                                                         \
        Value value = cls##_##name##Value(this);                              \
        if (value.isNull())                                                   \
            return nullptr;                                                   \
        return &value.toString()->asAtom();                                   \
    }

///////////////////////////////////////////////////////////////////////////
// ImportEntryObject

/* static */ const Class
ImportEntryObject::class_ = {
    "ImportEntry",
    JSCLASS_HAS_RESERVED_SLOTS(ImportEntryObject::SlotCount) |
    JSCLASS_IS_ANONYMOUS
};

DEFINE_GETTER_FUNCTIONS(ImportEntryObject, moduleRequest, ModuleRequestSlot)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, importName, ImportNameSlot)
DEFINE_GETTER_FUNCTIONS(ImportEntryObject, localName, LocalNameSlot)

DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, moduleRequest)
DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, importName)
DEFINE_ATOM_ACCESSOR_METHOD(ImportEntryObject, localName)

/* static */ bool
ImportEntryObject::isInstance(HandleValue value)
{
    return value.isObject() && value.toObject().is<ImportEntryObject>();
}

/* static */ bool
GlobalObject::initImportEntryProto(JSContext* cx, Handle<GlobalObject*> global)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("moduleRequest", ImportEntryObject_moduleRequestGetter, 0),
        JS_PSG("importName", ImportEntryObject_importNameGetter, 0),
        JS_PSG("localName", ImportEntryObject_localNameGetter, 0),
        JS_PS_END
    };

    RootedObject proto(cx, global->createBlankPrototype<PlainObject>(cx));
    if (!proto)
        return false;

    if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr))
        return false;

    global->setReservedSlot(IMPORT_ENTRY_PROTO, ObjectValue(*proto));
    return true;
}

/* static */ ImportEntryObject*
ImportEntryObject::create(ExclusiveContext* cx,
                          HandleAtom moduleRequest,
                          HandleAtom importName,
                          HandleAtom localName)
{
    RootedObject proto(cx, cx->global()->getImportEntryPrototype());
    RootedObject obj(cx, NewObjectWithGivenProto(cx, &class_, proto));
    if (!obj)
        return nullptr;

    RootedImportEntryObject self(cx, &obj->as<ImportEntryObject>());
    self->initReservedSlot(ModuleRequestSlot, StringValue(moduleRequest));
    self->initReservedSlot(ImportNameSlot, StringValue(importName));
    self->initReservedSlot(LocalNameSlot, StringValue(localName));
    return self;
}

///////////////////////////////////////////////////////////////////////////
// ExportEntryObject

/* static */ const Class
ExportEntryObject::class_ = {
    "ExportEntry",
    JSCLASS_HAS_RESERVED_SLOTS(ExportEntryObject::SlotCount) |
    JSCLASS_IS_ANONYMOUS
};

DEFINE_GETTER_FUNCTIONS(ExportEntryObject, exportName, ExportNameSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, moduleRequest, ModuleRequestSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, importName, ImportNameSlot)
DEFINE_GETTER_FUNCTIONS(ExportEntryObject, localName, LocalNameSlot)

DEFINE_ATOM_ACCESSOR_METHOD(ExportEntryObject, exportName)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, moduleRequest)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, importName)
DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(ExportEntryObject, localName)

/* static */ bool
ExportEntryObject::isInstance(HandleValue value)
{
    return value.isObject() && value.toObject().is<ExportEntryObject>();
}

/* static */ bool
GlobalObject::initExportEntryProto(JSContext* cx, Handle<GlobalObject*> global)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("exportName", ExportEntryObject_exportNameGetter, 0),
        JS_PSG("moduleRequest", ExportEntryObject_moduleRequestGetter, 0),
        JS_PSG("importName", ExportEntryObject_importNameGetter, 0),
        JS_PSG("localName", ExportEntryObject_localNameGetter, 0),
        JS_PS_END
    };

    RootedObject proto(cx, global->createBlankPrototype<PlainObject>(cx));
    if (!proto)
        return false;

    if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr))
        return false;

    global->setReservedSlot(EXPORT_ENTRY_PROTO, ObjectValue(*proto));
    return true;
}

static Value
StringOrNullValue(JSString* maybeString)
{
    return maybeString ? StringValue(maybeString) : NullValue();
}

/* static */ ExportEntryObject*
ExportEntryObject::create(ExclusiveContext* cx,
                          HandleAtom maybeExportName,
                          HandleAtom maybeModuleRequest,
                          HandleAtom maybeImportName,
                          HandleAtom maybeLocalName)
{
    RootedObject proto(cx, cx->global()->getExportEntryPrototype());
    RootedObject obj(cx, NewObjectWithGivenProto(cx, &class_, proto));
    if (!obj)
        return nullptr;

    RootedExportEntryObject self(cx, &obj->as<ExportEntryObject>());
    self->initReservedSlot(ExportNameSlot, StringOrNullValue(maybeExportName));
    self->initReservedSlot(ModuleRequestSlot, StringOrNullValue(maybeModuleRequest));
    self->initReservedSlot(ImportNameSlot, StringOrNullValue(maybeImportName));
    self->initReservedSlot(LocalNameSlot, StringOrNullValue(maybeLocalName));
    return self;
}

///////////////////////////////////////////////////////////////////////////
// IndirectBindingMap

IndirectBindingMap::Binding::Binding(ModuleEnvironmentObject* environment, Shape* shape)
  : environment(environment), shape(shape)
{}

IndirectBindingMap::IndirectBindingMap(Zone* zone)
  : map_(ZoneAllocPolicy(zone))
{
}

bool
IndirectBindingMap::init()
{
    return map_.init();
}

void
IndirectBindingMap::trace(JSTracer* trc)
{
    for (Map::Enum e(map_); !e.empty(); e.popFront()) {
        Binding& b = e.front().value();
        TraceEdge(trc, &b.environment, "module bindings environment");
        TraceEdge(trc, &b.shape, "module bindings shape");
        jsid bindingName = e.front().key();
        TraceManuallyBarrieredEdge(trc, &bindingName, "module bindings binding name");
        MOZ_ASSERT(bindingName == e.front().key());
    }
}

bool
IndirectBindingMap::putNew(JSContext* cx, HandleId name,
                           HandleModuleEnvironmentObject environment, HandleId localName)
{
    RootedShape shape(cx, environment->lookup(cx, localName));
    MOZ_ASSERT(shape);
    if (!map_.putNew(name, Binding(environment, shape))) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

bool
IndirectBindingMap::lookup(jsid name, ModuleEnvironmentObject** envOut, Shape** shapeOut) const
{
    auto ptr = map_.lookup(name);
    if (!ptr)
        return false;

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

/* static */ const ModuleNamespaceObject::ProxyHandler ModuleNamespaceObject::proxyHandler;

/* static */ bool
ModuleNamespaceObject::isInstance(HandleValue value)
{
    return value.isObject() && value.toObject().is<ModuleNamespaceObject>();
}

/* static */ ModuleNamespaceObject*
ModuleNamespaceObject::create(JSContext* cx, HandleModuleObject module)
{
    RootedValue priv(cx, ObjectValue(*module));
    ProxyOptions options;
    options.setLazyProto(true);
    options.setSingleton(true);
    RootedObject object(cx, NewProxyObject(cx, &proxyHandler, priv, nullptr, options));
    if (!object)
        return nullptr;

    RootedId funName(cx, INTERNED_STRING_TO_JSID(cx, cx->names().Symbol_iterator_fun));
    RootedFunction enumerateFun(cx);
    enumerateFun = JS::GetSelfHostedFunction(cx, "ModuleNamespaceEnumerate", funName, 0);
    if (!enumerateFun)
        return nullptr;

    SetProxyExtra(object, ProxyHandler::EnumerateFunctionSlot, ObjectValue(*enumerateFun));

    return &object->as<ModuleNamespaceObject>();
}

ModuleObject&
ModuleNamespaceObject::module()
{
    return GetProxyPrivate(this).toObject().as<ModuleObject>();
}

JSObject&
ModuleNamespaceObject::exports()
{
    JSObject* exports = module().namespaceExports();
    MOZ_ASSERT(exports);
    return *exports;
}

IndirectBindingMap&
ModuleNamespaceObject::bindings()
{
    IndirectBindingMap* bindings = module().namespaceBindings();
    MOZ_ASSERT(bindings);
    return *bindings;
}

bool
ModuleNamespaceObject::addBinding(JSContext* cx, HandleAtom exportedName,
                                  HandleModuleObject targetModule, HandleAtom localName)
{
    IndirectBindingMap* bindings(this->module().namespaceBindings());
    MOZ_ASSERT(bindings);

    RootedModuleEnvironmentObject environment(cx, &targetModule->initialEnvironment());
    RootedId exportedNameId(cx, AtomToId(exportedName));
    RootedId localNameId(cx, AtomToId(localName));
    return bindings->putNew(cx, exportedNameId, environment, localNameId);
}

const char ModuleNamespaceObject::ProxyHandler::family = 0;

ModuleNamespaceObject::ProxyHandler::ProxyHandler()
  : BaseProxyHandler(&family, true)
{}

JS::Value ModuleNamespaceObject::ProxyHandler::getEnumerateFunction(HandleObject proxy) const
{
    return GetProxyExtra(proxy, EnumerateFunctionSlot);
}

bool
ModuleNamespaceObject::ProxyHandler::getPrototype(JSContext* cx, HandleObject proxy,
                                                  MutableHandleObject protop) const
{
    protop.set(nullptr);
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::setPrototype(JSContext* cx, HandleObject proxy,
                                                  HandleObject proto, ObjectOpResult& result) const
{
    return result.failCantSetProto();
}

bool
ModuleNamespaceObject::ProxyHandler::getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy,
                                                            bool* isOrdinary,
                                                            MutableHandleObject protop) const
{
    *isOrdinary = false;
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::setImmutablePrototype(JSContext* cx, HandleObject proxy,
                                                           bool* succeeded) const
{
    *succeeded = true;
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::isExtensible(JSContext* cx, HandleObject proxy,
                                                  bool* extensible) const
{
    *extensible = false;
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::preventExtensions(JSContext* cx, HandleObject proxy,
                                                 ObjectOpResult& result) const
{
    result.succeed();
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy,
                                                              HandleId id,
                                                              MutableHandle<PropertyDescriptor> desc) const
{
    Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
    if (JSID_IS_SYMBOL(id)) {
        Rooted<JS::Symbol*> symbol(cx, JSID_TO_SYMBOL(id));
        if (symbol == cx->wellKnownSymbols().iterator) {
            RootedValue enumerateFun(cx, getEnumerateFunction(proxy));
            desc.object().set(proxy);
            desc.setConfigurable(false);
            desc.setEnumerable(false);
            desc.setValue(enumerateFun);
            return true;
        }

        if (symbol == cx->wellKnownSymbols().toStringTag) {
            RootedValue value(cx, StringValue(cx->names().Module));
            desc.object().set(proxy);
            desc.setWritable(false);
            desc.setEnumerable(false);
            desc.setConfigurable(true);
            desc.setValue(value);
            return true;
        }

        return true;
    }

    const IndirectBindingMap& bindings = ns->bindings();
    ModuleEnvironmentObject* env;
    Shape* shape;
    if (!bindings.lookup(id, &env, &shape))
        return true;

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

bool
ModuleNamespaceObject::ProxyHandler::defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                                                    Handle<PropertyDescriptor> desc,
                                                    ObjectOpResult& result) const
{
    return result.failReadOnly();
}

bool
ModuleNamespaceObject::ProxyHandler::has(JSContext* cx, HandleObject proxy, HandleId id,
                                         bool* bp) const
{
    Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
    if (JSID_IS_SYMBOL(id)) {
        Rooted<JS::Symbol*> symbol(cx, JSID_TO_SYMBOL(id));
        return symbol == cx->wellKnownSymbols().iterator ||
               symbol == cx->wellKnownSymbols().toStringTag;
    }

    *bp = ns->bindings().has(id);
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::get(JSContext* cx, HandleObject proxy, HandleValue receiver,
                                         HandleId id, MutableHandleValue vp) const
{
    Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
    if (JSID_IS_SYMBOL(id)) {
        Rooted<JS::Symbol*> symbol(cx, JSID_TO_SYMBOL(id));
        if (symbol == cx->wellKnownSymbols().iterator) {
            vp.set(getEnumerateFunction(proxy));
            return true;
        }

        if (symbol == cx->wellKnownSymbols().toStringTag) {
            vp.setString(cx->names().Module);
            return true;
        }

        return false;
    }

    ModuleEnvironmentObject* env;
    Shape* shape;
    if (!ns->bindings().lookup(id, &env, &shape))
        return false;

    RootedValue value(cx, env->getSlot(shape->slot()));
    if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
        ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
        return false;
    }

    vp.set(value);
    return true;
}

bool
ModuleNamespaceObject::ProxyHandler::set(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v,
                                         HandleValue receiver, ObjectOpResult& result) const
{
    return result.failReadOnly();
}

bool
ModuleNamespaceObject::ProxyHandler::delete_(JSContext* cx, HandleObject proxy, HandleId id,
                                             ObjectOpResult& result) const
{
    Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
    if (ns->bindings().has(id))
        return result.failReadOnly();

    return result.succeed();
}

bool
ModuleNamespaceObject::ProxyHandler::ownPropertyKeys(JSContext* cx, HandleObject proxy,
                                                     AutoIdVector& props) const
{
    Rooted<ModuleNamespaceObject*> ns(cx, &proxy->as<ModuleNamespaceObject>());
    RootedObject exports(cx, &ns->exports());
    uint32_t count;
    if (!GetLengthProperty(cx, exports, &count) || !props.reserve(props.length() + count))
        return false;

    Rooted<ValueVector> names(cx, ValueVector(cx));
    if (!names.resize(count) || !GetElements(cx, exports, count, names.begin()))
        return false;

    for (uint32_t i = 0; i < count; i++)
        props.infallibleAppend(AtomToId(&names[i].toString()->asAtom()));

    return true;
}

///////////////////////////////////////////////////////////////////////////
// FunctionDeclaration

FunctionDeclaration::FunctionDeclaration(HandleAtom name, HandleFunction fun)
  : name(name), fun(fun)
{}

void FunctionDeclaration::trace(JSTracer* trc)
{
    TraceEdge(trc, &name, "FunctionDeclaration name");
    TraceEdge(trc, &fun, "FunctionDeclaration fun");
}

///////////////////////////////////////////////////////////////////////////
// ModuleObject

/* static */ const ClassOps
ModuleObject::classOps_ = {
    nullptr,        /* addProperty */
    nullptr,        /* delProperty */
    nullptr,        /* getProperty */
    nullptr,        /* setProperty */
    nullptr,        /* enumerate   */
    nullptr,        /* resolve     */
    nullptr,        /* mayResolve  */
    ModuleObject::finalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    ModuleObject::trace
};

/* static */ const Class
ModuleObject::class_ = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(ModuleObject::SlotCount) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_BACKGROUND_FINALIZE,
    &ModuleObject::classOps_
};

#define DEFINE_ARRAY_SLOT_ACCESSOR(cls, name, slot)                           \
    ArrayObject&                                                              \
    cls::name() const                                                         \
    {                                                                         \
        return getFixedSlot(cls::slot).toObject().as<ArrayObject>();          \
    }

DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, importEntries, ImportEntriesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, localExportEntries, LocalExportEntriesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, indirectExportEntries, IndirectExportEntriesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, starExportEntries, StarExportEntriesSlot)

/* static */ bool
ModuleObject::isInstance(HandleValue value)
{
    return value.isObject() && value.toObject().is<ModuleObject>();
}

/* static */ ModuleObject*
ModuleObject::create(ExclusiveContext* cx)
{
    RootedObject proto(cx, cx->global()->getModulePrototype());
    RootedObject obj(cx, NewObjectWithGivenProto(cx, &class_, proto));
    if (!obj)
        return nullptr;

    RootedModuleObject self(cx, &obj->as<ModuleObject>());

    Zone* zone = cx->zone();
    IndirectBindingMap* bindings = zone->new_<IndirectBindingMap>(zone);
    if (!bindings || !bindings->init()) {
        ReportOutOfMemory(cx);
        js_delete<IndirectBindingMap>(bindings);
        return nullptr;
    }

    self->initReservedSlot(ImportBindingsSlot, PrivateValue(bindings));

    FunctionDeclarationVector* funDecls = zone->new_<FunctionDeclarationVector>(zone);
    if (!funDecls) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    self->initReservedSlot(FunctionDeclarationsSlot, PrivateValue(funDecls));
    return self;
}

/* static */ void
ModuleObject::finalize(js::FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->maybeOffMainThread());
    ModuleObject* self = &obj->as<ModuleObject>();
    if (self->hasImportBindings())
        fop->delete_(&self->importBindings());
    if (IndirectBindingMap* bindings = self->namespaceBindings())
        fop->delete_(bindings);
    if (FunctionDeclarationVector* funDecls = self->functionDeclarations())
        fop->delete_(funDecls);
}

ModuleEnvironmentObject*
ModuleObject::environment() const
{
    Value value = getReservedSlot(EnvironmentSlot);
    if (value.isUndefined())
        return nullptr;

    return &value.toObject().as<ModuleEnvironmentObject>();
}

bool
ModuleObject::hasImportBindings() const
{
    // Import bindings may not be present if we hit OOM in initialization.
    return !getReservedSlot(ImportBindingsSlot).isUndefined();
}

IndirectBindingMap&
ModuleObject::importBindings()
{
    return *static_cast<IndirectBindingMap*>(getReservedSlot(ImportBindingsSlot).toPrivate());
}

JSObject*
ModuleObject::namespaceExports()
{
    Value value = getReservedSlot(NamespaceExportsSlot);
    if (value.isUndefined())
        return nullptr;

    return &value.toObject();
}

IndirectBindingMap*
ModuleObject::namespaceBindings()
{
    Value value = getReservedSlot(NamespaceBindingsSlot);
    if (value.isUndefined())
        return nullptr;

    return static_cast<IndirectBindingMap*>(value.toPrivate());
}

ModuleNamespaceObject*
ModuleObject::namespace_()
{
    Value value = getReservedSlot(NamespaceSlot);
    if (value.isUndefined())
        return nullptr;
    return &value.toObject().as<ModuleNamespaceObject>();
}

FunctionDeclarationVector*
ModuleObject::functionDeclarations()
{
    Value value = getReservedSlot(FunctionDeclarationsSlot);
    if (value.isUndefined())
        return nullptr;

    return static_cast<FunctionDeclarationVector*>(value.toPrivate());
}

void
ModuleObject::init(HandleScript script)
{
    initReservedSlot(ScriptSlot, PrivateValue(script));
    initReservedSlot(StateSlot, Int32Value(MODULE_STATE_FAILED));
}

void
ModuleObject::setInitialEnvironment(HandleModuleEnvironmentObject initialEnvironment)
{
    initReservedSlot(InitialEnvironmentSlot, ObjectValue(*initialEnvironment));
}

void
ModuleObject::initImportExportData(HandleArrayObject requestedModules,
                                   HandleArrayObject importEntries,
                                   HandleArrayObject localExportEntries,
                                   HandleArrayObject indirectExportEntries,
                                   HandleArrayObject starExportEntries)
{
    initReservedSlot(RequestedModulesSlot, ObjectValue(*requestedModules));
    initReservedSlot(ImportEntriesSlot, ObjectValue(*importEntries));
    initReservedSlot(LocalExportEntriesSlot, ObjectValue(*localExportEntries));
    initReservedSlot(IndirectExportEntriesSlot, ObjectValue(*indirectExportEntries));
    initReservedSlot(StarExportEntriesSlot, ObjectValue(*starExportEntries));
    setReservedSlot(StateSlot, Int32Value(MODULE_STATE_PARSED));
}

static bool
FreezeObjectProperty(JSContext* cx, HandleNativeObject obj, uint32_t slot)
{
    RootedObject property(cx, &obj->getSlot(slot).toObject());
    return FreezeObject(cx, property);
}

/* static */ bool
ModuleObject::Freeze(JSContext* cx, HandleModuleObject self)
{
    return FreezeObjectProperty(cx, self, RequestedModulesSlot) &&
           FreezeObjectProperty(cx, self, ImportEntriesSlot) &&
           FreezeObjectProperty(cx, self, LocalExportEntriesSlot) &&
           FreezeObjectProperty(cx, self, IndirectExportEntriesSlot) &&
           FreezeObjectProperty(cx, self, StarExportEntriesSlot) &&
           FreezeObject(cx, self);
}

#ifdef DEBUG

static inline bool
IsObjectFrozen(JSContext* cx, HandleObject obj)
{
    bool frozen = false;
    MOZ_ALWAYS_TRUE(TestIntegrityLevel(cx, obj, IntegrityLevel::Frozen, &frozen));
    return frozen;
}

static inline bool
IsObjectPropertyFrozen(JSContext* cx, HandleNativeObject obj, uint32_t slot)
{
    RootedObject property(cx, &obj->getSlot(slot).toObject());
    return IsObjectFrozen(cx, property);
}

/* static */ inline bool
ModuleObject::IsFrozen(JSContext* cx, HandleModuleObject self)
{
    return IsObjectPropertyFrozen(cx, self, RequestedModulesSlot) &&
           IsObjectPropertyFrozen(cx, self, ImportEntriesSlot) &&
           IsObjectPropertyFrozen(cx, self, LocalExportEntriesSlot) &&
           IsObjectPropertyFrozen(cx, self, IndirectExportEntriesSlot) &&
           IsObjectPropertyFrozen(cx, self, StarExportEntriesSlot) &&
           IsObjectFrozen(cx, self);
}

#endif

inline static void
AssertModuleScopesMatch(ModuleObject* module)
{
    MOZ_ASSERT(module->enclosingScope()->is<GlobalScope>());
    MOZ_ASSERT(IsGlobalLexicalEnvironment(&module->initialEnvironment().enclosingEnvironment()));
}

void
ModuleObject::fixEnvironmentsAfterCompartmentMerge(JSContext* cx)
{
    AssertModuleScopesMatch(this);
    initialEnvironment().fixEnclosingEnvironmentAfterCompartmentMerge(script()->global());
    AssertModuleScopesMatch(this);
}

bool
ModuleObject::hasScript() const
{
    // When modules are parsed via the Reflect.parse() API, the module object
    // doesn't have a script.
    return !getReservedSlot(ScriptSlot).isUndefined();
}

JSScript*
ModuleObject::script() const
{
    return static_cast<JSScript*>(getReservedSlot(ScriptSlot).toPrivate());
}

static inline void
AssertValidModuleState(ModuleState state)
{
    MOZ_ASSERT(state >= MODULE_STATE_FAILED && state <= MODULE_STATE_EVALUATED);
}

ModuleState
ModuleObject::state() const
{
    ModuleState state = getReservedSlot(StateSlot).toInt32();
    AssertValidModuleState(state);
    return state;
}

Value
ModuleObject::hostDefinedField() const
{
    return getReservedSlot(HostDefinedSlot);
}

void
ModuleObject::setHostDefinedField(JS::Value value)
{
    setReservedSlot(HostDefinedSlot, value);
}

ModuleEnvironmentObject&
ModuleObject::initialEnvironment() const
{
    return getReservedSlot(InitialEnvironmentSlot).toObject().as<ModuleEnvironmentObject>();
}

Scope*
ModuleObject::enclosingScope() const
{
    return script()->enclosingScope();
}

/* static */ void
ModuleObject::trace(JSTracer* trc, JSObject* obj)
{
    ModuleObject& module = obj->as<ModuleObject>();
    if (module.hasScript()) {
        JSScript* script = module.script();
        TraceManuallyBarrieredEdge(trc, &script, "Module script");
        module.setReservedSlot(ScriptSlot, PrivateValue(script));
    }

    if (module.hasImportBindings())
        module.importBindings().trace(trc);
    if (IndirectBindingMap* bindings = module.namespaceBindings())
        bindings->trace(trc);

    if (FunctionDeclarationVector* funDecls = module.functionDeclarations())
        funDecls->trace(trc);
}

void
ModuleObject::createEnvironment()
{
    // The environment has already been created, we just neet to set it in the
    // right slot.
    MOZ_ASSERT(!getReservedSlot(InitialEnvironmentSlot).isUndefined());
    MOZ_ASSERT(getReservedSlot(EnvironmentSlot).isUndefined());
    setReservedSlot(EnvironmentSlot, getReservedSlot(InitialEnvironmentSlot));
}

bool
ModuleObject::noteFunctionDeclaration(ExclusiveContext* cx, HandleAtom name, HandleFunction fun)
{
    FunctionDeclarationVector* funDecls = functionDeclarations();
    if (!funDecls->emplaceBack(name, fun)) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

/* static */ bool
ModuleObject::instantiateFunctionDeclarations(JSContext* cx, HandleModuleObject self)
{
    MOZ_ASSERT(IsFrozen(cx, self));

    FunctionDeclarationVector* funDecls = self->functionDeclarations();
    if (!funDecls) {
        JS_ReportError(cx, "Module function declarations have already been instantiated");
        return false;
    }

    RootedModuleEnvironmentObject env(cx, &self->initialEnvironment());
    RootedFunction fun(cx);
    RootedValue value(cx);

    for (const auto& funDecl : *funDecls) {
        fun = funDecl.fun;
        RootedObject obj(cx, Lambda(cx, fun, env));
        if (!obj)
            return false;

        value = ObjectValue(*fun);
        if (!SetProperty(cx, env, funDecl.name->asPropertyName(), value))
            return false;
    }

    js_delete(funDecls);
    self->setReservedSlot(FunctionDeclarationsSlot, UndefinedValue());
    return true;
}

void
ModuleObject::setState(int32_t newState)
{
    AssertValidModuleState(newState);
    MOZ_ASSERT(state() != MODULE_STATE_FAILED);
    MOZ_ASSERT(newState == MODULE_STATE_FAILED || newState > state());
    setReservedSlot(StateSlot, Int32Value(newState));
}

/* static */ bool
ModuleObject::evaluate(JSContext* cx, HandleModuleObject self, MutableHandleValue rval)
{
    MOZ_ASSERT(IsFrozen(cx, self));

    RootedScript script(cx, self->script());
    RootedModuleEnvironmentObject scope(cx, self->environment());
    if (!scope) {
        JS_ReportError(cx, "Module declarations have not yet been instantiated");
        return false;
    }

    return Execute(cx, script, *scope, rval.address());
}

/* static */ ModuleNamespaceObject*
ModuleObject::createNamespace(JSContext* cx, HandleModuleObject self, HandleObject exports)
{
    MOZ_ASSERT(!self->namespace_());
    MOZ_ASSERT(exports->is<ArrayObject>() || exports->is<UnboxedArrayObject>());

    RootedModuleNamespaceObject ns(cx, ModuleNamespaceObject::create(cx, self));
    if (!ns)
        return nullptr;

    Zone* zone = cx->zone();
    IndirectBindingMap* bindings = zone->new_<IndirectBindingMap>(zone);
    if (!bindings || !bindings->init()) {
        ReportOutOfMemory(cx);
        js_delete<IndirectBindingMap>(bindings);
        return nullptr;
    }

    self->initReservedSlot(NamespaceSlot, ObjectValue(*ns));
    self->initReservedSlot(NamespaceExportsSlot, ObjectValue(*exports));
    self->initReservedSlot(NamespaceBindingsSlot, PrivateValue(bindings));
    return ns;
}

static bool
InvokeSelfHostedMethod(JSContext* cx, HandleModuleObject self, HandlePropertyName name)
{
    RootedValue fval(cx);
    if (!GlobalObject::getSelfHostedFunction(cx, cx->global(), name, name, 0, &fval))
        return false;

    RootedValue ignored(cx);
    return Call(cx, fval, self, &ignored);
}

/* static */ bool
ModuleObject::DeclarationInstantiation(JSContext* cx, HandleModuleObject self)
{
    return InvokeSelfHostedMethod(cx, self, cx->names().ModuleDeclarationInstantiation);
}

/* static */ bool
ModuleObject::Evaluation(JSContext* cx, HandleModuleObject self)
{
    return InvokeSelfHostedMethod(cx, self, cx->names().ModuleEvaluation);
}

DEFINE_GETTER_FUNCTIONS(ModuleObject, namespace_, NamespaceSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, state, StateSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, importEntries, ImportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, localExportEntries, LocalExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, indirectExportEntries, IndirectExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, starExportEntries, StarExportEntriesSlot)

/* static */ bool
GlobalObject::initModuleProto(JSContext* cx, Handle<GlobalObject*> global)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("namespace", ModuleObject_namespace_Getter, 0),
        JS_PSG("state", ModuleObject_stateGetter, 0),
        JS_PSG("requestedModules", ModuleObject_requestedModulesGetter, 0),
        JS_PSG("importEntries", ModuleObject_importEntriesGetter, 0),
        JS_PSG("localExportEntries", ModuleObject_localExportEntriesGetter, 0),
        JS_PSG("indirectExportEntries", ModuleObject_indirectExportEntriesGetter, 0),
        JS_PSG("starExportEntries", ModuleObject_starExportEntriesGetter, 0),
        JS_PS_END
    };

    static const JSFunctionSpec protoFunctions[] = {
        JS_SELF_HOSTED_FN("getExportedNames", "ModuleGetExportedNames", 1, 0),
        JS_SELF_HOSTED_FN("resolveExport", "ModuleResolveExport", 3, 0),
        JS_SELF_HOSTED_FN("declarationInstantiation", "ModuleDeclarationInstantiation", 0, 0),
        JS_SELF_HOSTED_FN("evaluation", "ModuleEvaluation", 0, 0),
        JS_FS_END
    };

    RootedObject proto(cx, global->createBlankPrototype<PlainObject>(cx));
    if (!proto)
        return false;

    if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, protoFunctions))
        return false;

    global->setReservedSlot(MODULE_PROTO, ObjectValue(*proto));
    return true;
}

#undef DEFINE_GETTER_FUNCTIONS
#undef DEFINE_STRING_ACCESSOR_METHOD
#undef DEFINE_ARRAY_SLOT_ACCESSOR

///////////////////////////////////////////////////////////////////////////
// ModuleBuilder

ModuleBuilder::ModuleBuilder(ExclusiveContext* cx, HandleModuleObject module)
  : cx_(cx),
    module_(cx, module),
    requestedModules_(cx, AtomVector(cx)),
    importedBoundNames_(cx, AtomVector(cx)),
    importEntries_(cx, ImportEntryVector(cx)),
    exportEntries_(cx, ExportEntryVector(cx)),
    localExportEntries_(cx, ExportEntryVector(cx)),
    indirectExportEntries_(cx, ExportEntryVector(cx)),
    starExportEntries_(cx, ExportEntryVector(cx))
{}

bool
ModuleBuilder::buildTables()
{
    for (const auto& e : exportEntries_) {
        RootedExportEntryObject exp(cx_, e);
        if (!exp->moduleRequest()) {
            RootedImportEntryObject importEntry(cx_, importEntryFor(exp->localName()));
            if (!importEntry) {
                if (!localExportEntries_.append(exp))
                    return false;
            } else {
                if (importEntry->importName() == cx_->names().star) {
                    if (!localExportEntries_.append(exp))
                        return false;
                } else {
                    RootedAtom exportName(cx_, exp->exportName());
                    RootedAtom moduleRequest(cx_, importEntry->moduleRequest());
                    RootedAtom importName(cx_, importEntry->importName());
                    RootedExportEntryObject exportEntry(cx_);
                    exportEntry = ExportEntryObject::create(cx_,
                                                            exportName,
                                                            moduleRequest,
                                                            importName,
                                                            nullptr);
                    if (!exportEntry || !indirectExportEntries_.append(exportEntry))
                        return false;
                }
            }
        } else if (exp->importName() == cx_->names().star) {
            if (!starExportEntries_.append(exp))
                return false;
        } else {
            if (!indirectExportEntries_.append(exp))
                return false;
        }
    }

    return true;
}

bool
ModuleBuilder::initModule()
{
    RootedArrayObject requestedModules(cx_, createArray<JSAtom*>(requestedModules_));
    if (!requestedModules)
        return false;

    RootedArrayObject importEntries(cx_, createArray<ImportEntryObject*>(importEntries_));
    if (!importEntries)
        return false;

    RootedArrayObject localExportEntries(cx_, createArray<ExportEntryObject*>(localExportEntries_));
    if (!localExportEntries)
        return false;

    RootedArrayObject indirectExportEntries(cx_);
    indirectExportEntries = createArray<ExportEntryObject*>(indirectExportEntries_);
    if (!indirectExportEntries)
        return false;

    RootedArrayObject starExportEntries(cx_, createArray<ExportEntryObject*>(starExportEntries_));
    if (!starExportEntries)
        return false;

    module_->initImportExportData(requestedModules,
                                 importEntries,
                                 localExportEntries,
                                 indirectExportEntries,
                                 starExportEntries);

    return true;
}

bool
ModuleBuilder::processImport(frontend::ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_IMPORT));
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    MOZ_ASSERT(pn->pn_left->isKind(PNK_IMPORT_SPEC_LIST));
    MOZ_ASSERT(pn->pn_right->isKind(PNK_STRING));

    RootedAtom module(cx_, pn->pn_right->pn_atom);
    if (!maybeAppendRequestedModule(module))
        return false;

    for (ParseNode* spec = pn->pn_left->pn_head; spec; spec = spec->pn_next) {
        MOZ_ASSERT(spec->isKind(PNK_IMPORT_SPEC));
        MOZ_ASSERT(spec->pn_left->isArity(PN_NAME));
        MOZ_ASSERT(spec->pn_right->isArity(PN_NAME));

        RootedAtom importName(cx_, spec->pn_left->pn_atom);
        RootedAtom localName(cx_, spec->pn_right->pn_atom);

        if (!importedBoundNames_.append(localName))
            return false;

        RootedImportEntryObject importEntry(cx_);
        importEntry = ImportEntryObject::create(cx_, module, importName, localName);
        if (!importEntry || !importEntries_.append(importEntry))
            return false;
    }

    return true;
}

bool
ModuleBuilder::processExport(frontend::ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_EXPORT) || pn->isKind(PNK_EXPORT_DEFAULT));
    MOZ_ASSERT(pn->getArity() == pn->isKind(PNK_EXPORT) ? PN_UNARY : PN_BINARY);

    bool isDefault = pn->getKind() == PNK_EXPORT_DEFAULT;
    ParseNode* kid = isDefault ? pn->pn_left : pn->pn_kid;

    switch (kid->getKind()) {
      case PNK_EXPORT_SPEC_LIST:
        MOZ_ASSERT(!isDefault);
        for (ParseNode* spec = kid->pn_head; spec; spec = spec->pn_next) {
            MOZ_ASSERT(spec->isKind(PNK_EXPORT_SPEC));
            RootedAtom localName(cx_, spec->pn_left->pn_atom);
            RootedAtom exportName(cx_, spec->pn_right->pn_atom);
            if (!appendExportEntry(exportName, localName))
                return false;
        }
        break;

      case PNK_CLASS: {
          const ClassNode& cls = kid->as<ClassNode>();
          MOZ_ASSERT(cls.names());
          RootedAtom localName(cx_, cls.names()->innerBinding()->pn_atom);
          RootedAtom exportName(cx_, isDefault ? cx_->names().default_ : localName.get());
          if (!appendExportEntry(exportName, localName))
              return false;
          break;
      }

      case PNK_VAR:
      case PNK_CONST:
      case PNK_LET: {
          MOZ_ASSERT(kid->isArity(PN_LIST));
          for (ParseNode* var = kid->pn_head; var; var = var->pn_next) {
              if (var->isKind(PNK_ASSIGN))
                  var = var->pn_left;
              MOZ_ASSERT(var->isKind(PNK_NAME));
              RootedAtom localName(cx_, var->pn_atom);
              RootedAtom exportName(cx_, isDefault ? cx_->names().default_ : localName.get());
              if (!appendExportEntry(exportName, localName))
                  return false;
          }
          break;
      }

      case PNK_FUNCTION: {
          RootedFunction func(cx_, kid->pn_funbox->function());
          if (!func->isArrow()) {
              RootedAtom localName(cx_, func->name());
              RootedAtom exportName(cx_, isDefault ? cx_->names().default_ : localName.get());
              MOZ_ASSERT_IF(isDefault, localName);
              if (!appendExportEntry(exportName, localName))
                  return false;
              break;
          }
      }

      MOZ_FALLTHROUGH; // Arrow functions are handled below.

      default:
        MOZ_ASSERT(isDefault);
        RootedAtom localName(cx_, cx_->names().starDefaultStar);
        RootedAtom exportName(cx_, cx_->names().default_);
        if (!appendExportEntry(exportName, localName))
            return false;
        break;
    }
    return true;
}

bool
ModuleBuilder::processExportFrom(frontend::ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_EXPORT_FROM));
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    MOZ_ASSERT(pn->pn_left->isKind(PNK_EXPORT_SPEC_LIST));
    MOZ_ASSERT(pn->pn_right->isKind(PNK_STRING));

    RootedAtom module(cx_, pn->pn_right->pn_atom);
    if (!maybeAppendRequestedModule(module))
        return false;

    for (ParseNode* spec = pn->pn_left->pn_head; spec; spec = spec->pn_next) {
        if (spec->isKind(PNK_EXPORT_SPEC)) {
            RootedAtom bindingName(cx_, spec->pn_left->pn_atom);
            RootedAtom exportName(cx_, spec->pn_right->pn_atom);
            if (!appendExportFromEntry(exportName, module, bindingName))
                return false;
        } else {
            MOZ_ASSERT(spec->isKind(PNK_EXPORT_BATCH_SPEC));
            RootedAtom importName(cx_, cx_->names().star);
            if (!appendExportFromEntry(nullptr, module, importName))
                return false;
        }
    }

    return true;
}

ImportEntryObject*
ModuleBuilder::importEntryFor(JSAtom* localName) const
{
    for (auto import : importEntries_) {
        if (import->localName() == localName)
            return import;
    }
    return nullptr;
}

bool
ModuleBuilder::hasExportedName(JSAtom* name) const
{
    for (auto entry : exportEntries_) {
        if (entry->exportName() == name)
            return true;
    }
    return false;
}

bool
ModuleBuilder::appendExportEntry(HandleAtom exportName, HandleAtom localName)
{
    Rooted<ExportEntryObject*> exportEntry(cx_);
    exportEntry = ExportEntryObject::create(cx_, exportName, nullptr, nullptr, localName);
    return exportEntry && exportEntries_.append(exportEntry);
}

bool
ModuleBuilder::appendExportFromEntry(HandleAtom exportName, HandleAtom moduleRequest,
                                     HandleAtom importName)
{
    Rooted<ExportEntryObject*> exportEntry(cx_);
    exportEntry = ExportEntryObject::create(cx_, exportName, moduleRequest, importName, nullptr);
    return exportEntry && exportEntries_.append(exportEntry);
}

bool
ModuleBuilder::maybeAppendRequestedModule(HandleAtom module)
{
    for (auto m : requestedModules_) {
        if (m == module)
            return true;
    }
    return requestedModules_.append(module);
}

static Value
MakeElementValue(JSString *string)
{
    return StringValue(string);
}

static Value
MakeElementValue(JSObject *object)
{
    return ObjectValue(*object);
}

template <typename T>
ArrayObject* ModuleBuilder::createArray(const GCVector<T>& vector)
{
    uint32_t length = vector.length();
    RootedArrayObject array(cx_, NewDenseFullyAllocatedArray(cx_, length));
    if (!array)
        return nullptr;

    array->setDenseInitializedLength(length);
    for (uint32_t i = 0; i < length; i++)
        array->initDenseElement(i, MakeElementValue(vector[i]));

    return array;
}
