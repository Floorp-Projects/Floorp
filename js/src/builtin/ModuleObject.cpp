/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/ModuleObject.h"

#include "gc/Tracer.h"

#include "jsobjinlines.h"

using namespace js;

typedef JS::Rooted<ImportEntryObject*> RootedImportEntry;

template<typename T, Value ValueGetter(T* obj)>
static bool
ModuleValueGetterImpl(JSContext* cx, CallArgs args)
{
    args.rval().set(ValueGetter(&args.thisv().toObject().as<T>()));
    return true;
}

template<typename T, Value ValueGetter(T* obj)>
static bool
ModuleValueGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<T::isInstance, ModuleValueGetterImpl<T, ValueGetter>>(cx, args);
}

#define DEFINE_GETTER_FUNCTIONS(cls, name, slot)                              \
    static Value                                                              \
    cls##_##name##Value(cls* obj) {                                           \
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
    cls::name()                                                               \
    {                                                                         \
        Value value = cls##_##name##Value(this);                              \
        return &value.toString()->asAtom();                                   \
    }

///////////////////////////////////////////////////////////////////////////
// ImportEntryObject

/* static */ const Class
ImportEntryObject::class_ = {
    "ImportEntry",
    JSCLASS_HAS_RESERVED_SLOTS(ImportEntryObject::SlotCount) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ImportEntry) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_IMPLEMENTS_BARRIERS
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

/* static */ JSObject*
ImportEntryObject::initClass(JSContext* cx, HandleObject obj)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("moduleRequest", ImportEntryObject_moduleRequestGetter, 0),
        JS_PSG("importName", ImportEntryObject_importNameGetter, 0),
        JS_PSG("localName", ImportEntryObject_localNameGetter, 0),
        JS_PS_END
    };

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedObject proto(cx, global->createBlankPrototype<PlainObject>(cx));
    if (!proto)
        return nullptr;

    if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr))
        return nullptr;

    global->setPrototype(JSProto_ImportEntry, ObjectValue(*proto));
    return proto;
}

JSObject*
js::InitImportEntryClass(JSContext* cx, HandleObject obj)
{
    return ImportEntryObject::initClass(cx, obj);
}

/* static */ ImportEntryObject*
ImportEntryObject::create(JSContext* cx,
                          HandleAtom moduleRequest,
                          HandleAtom importName,
                          HandleAtom localName)
{
    RootedImportEntry self(cx, NewBuiltinClassInstance<ImportEntryObject>(cx));
    if (!self)
        return nullptr;
    self->initReservedSlot(ModuleRequestSlot, StringValue(moduleRequest));
    self->initReservedSlot(ImportNameSlot, StringValue(importName));
    self->initReservedSlot(LocalNameSlot, StringValue(localName));
    return self;
}

///////////////////////////////////////////////////////////////////////////
// ModuleObject

/* static */ const Class
ModuleObject::class_ = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(ModuleObject::SlotCount) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Module) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_IMPLEMENTS_BARRIERS,
    nullptr,        /* addProperty */
    nullptr,        /* delProperty */
    nullptr,        /* getProperty */
    nullptr,        /* setProperty */
    nullptr,        /* enumerate   */
    nullptr,        /* resolve     */
    nullptr,        /* mayResolve  */
    nullptr,        /* convert     */
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    ModuleObject::trace
};

#define DEFINE_ARRAY_SLOT_ACCESSOR(cls, name, slot)                           \
    ArrayObject&                                                              \
    cls::name() const                                                         \
    {                                                                         \
        return getFixedSlot(cls::slot).toObject().as<ArrayObject>();          \
    }

DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_ARRAY_SLOT_ACCESSOR(ModuleObject, importEntries, ImportEntriesSlot)

/* static */ bool
ModuleObject::isInstance(HandleValue value)
{
    return value.isObject() && value.toObject().is<ModuleObject>();
}

/* static */ ModuleObject*
ModuleObject::create(ExclusiveContext* cx)
{
    return NewBuiltinClassInstance<ModuleObject>(cx, TenuredObject);
}

void
ModuleObject::init(HandleScript script)
{
    initReservedSlot(ScriptSlot, PrivateValue(script));
}

void
ModuleObject::initImportExportData(HandleArrayObject requestedModules,
                                   HandleArrayObject importEntries)
{
    initReservedSlot(RequestedModulesSlot, ObjectValue(*requestedModules));
    initReservedSlot(ImportEntriesSlot, ObjectValue(*importEntries));
}

JSScript*
ModuleObject::script() const
{
    return static_cast<JSScript*>(getReservedSlot(ScriptSlot).toPrivate());
}

/* static */ void
ModuleObject::trace(JSTracer* trc, JSObject* obj)
{
    ModuleObject& module = obj->as<ModuleObject>();
    JSScript* script = module.script();
    TraceManuallyBarrieredEdge(trc, &script, "Module script");
    module.setReservedSlot(ScriptSlot, PrivateValue(script));
}

DEFINE_GETTER_FUNCTIONS(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, importEntries, ImportEntriesSlot)

JSObject*
js::InitModuleClass(JSContext* cx, HandleObject obj)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("requestedModules", ModuleObject_requestedModulesGetter, 0),
        JS_PSG("importEntries", ModuleObject_importEntriesGetter, 0),
        JS_PS_END
    };

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedObject proto(cx, global->createBlankPrototype<PlainObject>(cx));
    if (!proto)
        return nullptr;

    if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr))
        return nullptr;

    global->setPrototype(JSProto_Module, ObjectValue(*proto));
    return proto;
}

#undef DEFINE_GETTER_FUNCTIONS
#undef DEFINE_STRING_ACCESSOR_METHOD
#undef DEFINE_ARRAY_SLOT_ACCESSOR

///////////////////////////////////////////////////////////////////////////
// ModuleBuilder

ModuleBuilder::ModuleBuilder(JSContext* cx)
  : cx_(cx),
    requestedModules_(cx, AtomVector(cx)),
    importedBoundNames_(cx, AtomVector(cx)),
    importEntries_(cx, ImportEntryVector(cx))
{}

bool
ModuleBuilder::buildAndInit(frontend::ParseNode* moduleNode, HandleModuleObject module)
{
    MOZ_ASSERT(moduleNode->isKind(PNK_MODULE));

    ParseNode* stmtsNode = moduleNode->pn_expr;
    MOZ_ASSERT(stmtsNode->isKind(PNK_STATEMENTLIST));
    MOZ_ASSERT(stmtsNode->isArity(PN_LIST));

    for (ParseNode* pn = stmtsNode->pn_head; pn; pn = pn->pn_next) {
        switch (pn->getKind()) {
          case PNK_IMPORT:
            if (!processImport(pn))
                return false;
            break;

          case PNK_EXPORT:
            break;

          case PNK_EXPORT_FROM:
            if (!processExportFrom(pn))
                return false;
            break;

          case PNK_EXPORT_DEFAULT:
            break;

          default:
            break;
        }
    }

    RootedArrayObject requestedModules(cx_, createArray<JSAtom*>(requestedModules_));
    if (!requestedModules)
        return false;

    RootedArrayObject importEntries(cx_, createArray<ImportEntryObject*>(importEntries_));
    if (!importEntries)
        return false;

    module->initImportExportData(requestedModules,
                                 importEntries);

    return true;
}

bool
ModuleBuilder::processImport(frontend::ParseNode* pn)
{
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

        RootedImportEntry importEntry(cx_);
        importEntry = ImportEntryObject::create(cx_, module, importName, localName);
        if (!importEntry)
            return false;

        if (!importEntries_.append(importEntry)) {
            ReportOutOfMemory(cx_);
            return false;
        }
    }

    return true;
}

bool
ModuleBuilder::processExportFrom(frontend::ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    MOZ_ASSERT(pn->pn_right->isKind(PNK_STRING));

    RootedAtom module(cx_, pn->pn_right->pn_atom);
    if (!maybeAppendRequestedModule(module))
        return false;

    return true;
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
ArrayObject* ModuleBuilder::createArray(const TraceableVector<T>& vector)
{
    uint32_t length = vector.length();
    RootedArrayObject array(cx_, NewDenseFullyAllocatedArray(cx_, length));
    if (!array)
        return nullptr;

    array->setDenseInitializedLength(length);
    for (uint32_t i = 0; i < length; i++)
        array->initDenseElement(i, MakeElementValue(vector[i]));
    if (!JS_FreezeObject(cx_, array))
        return nullptr;

    return array;
}
