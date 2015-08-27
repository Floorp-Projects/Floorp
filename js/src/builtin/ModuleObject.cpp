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
typedef JS::Rooted<ExportEntryObject*> RootedExportEntry;

template<typename T, Value ValueGetter(T* obj)>
static bool
ModuleValueGetterImpl(JSContext* cx, const CallArgs& args)
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

#define DEFINE_ATOM_OR_NULL_ACCESSOR_METHOD(cls, name)                        \
    JSAtom*                                                                   \
    cls::name()                                                               \
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
// ExportEntryObject

/* static */ const Class
ExportEntryObject::class_ = {
    "ExportEntry",
    JSCLASS_HAS_RESERVED_SLOTS(ExportEntryObject::SlotCount) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ExportEntry) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_IMPLEMENTS_BARRIERS
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

/* static */ JSObject*
ExportEntryObject::initClass(JSContext* cx, HandleObject obj)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("exportName", ExportEntryObject_exportNameGetter, 0),
        JS_PSG("moduleRequest", ExportEntryObject_moduleRequestGetter, 0),
        JS_PSG("importName", ExportEntryObject_importNameGetter, 0),
        JS_PSG("localName", ExportEntryObject_localNameGetter, 0),
        JS_PS_END
    };

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedObject proto(cx, global->createBlankPrototype<PlainObject>(cx));
    if (!proto)
        return nullptr;

    if (!DefinePropertiesAndFunctions(cx, proto, protoAccessors, nullptr))
        return nullptr;

    global->setPrototype(JSProto_ExportEntry, ObjectValue(*proto));
    return proto;
}

JSObject*
js::InitExportEntryClass(JSContext* cx, HandleObject obj)
{
    return ExportEntryObject::initClass(cx, obj);
}

static Value
StringOrNullValue(JSString* maybeString)
{
    return maybeString ? StringValue(maybeString) : NullValue();
}

/* static */ ExportEntryObject*
ExportEntryObject::create(JSContext* cx,
                          HandleAtom maybeExportName,
                          HandleAtom maybeModuleRequest,
                          HandleAtom maybeImportName,
                          HandleAtom maybeLocalName)
{
    RootedExportEntry self(cx, NewBuiltinClassInstance<ExportEntryObject>(cx));
    if (!self)
        return nullptr;
    self->initReservedSlot(ExportNameSlot, StringOrNullValue(maybeExportName));
    self->initReservedSlot(ModuleRequestSlot, StringOrNullValue(maybeModuleRequest));
    self->initReservedSlot(ImportNameSlot, StringOrNullValue(maybeImportName));
    self->initReservedSlot(LocalNameSlot, StringOrNullValue(maybeLocalName));
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
    return NewBuiltinClassInstance<ModuleObject>(cx, TenuredObject);
}

void
ModuleObject::init(HandleScript script)
{
    initReservedSlot(ScriptSlot, PrivateValue(script));
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
}

JSScript*
ModuleObject::script() const
{
    return static_cast<JSScript*>(getReservedSlot(ScriptSlot).toPrivate());
}

ModuleEnvironmentObject&
ModuleObject::initialEnvironment() const
{
    return getReservedSlot(InitialEnvironmentSlot).toObject().as<ModuleEnvironmentObject>();
}

/* static */ void
ModuleObject::trace(JSTracer* trc, JSObject* obj)
{
    ModuleObject& module = obj->as<ModuleObject>();
    JSScript* script = module.script();
    TraceManuallyBarrieredEdge(trc, &script, "Module script");
    module.setReservedSlot(ScriptSlot, PrivateValue(script));
}

DEFINE_GETTER_FUNCTIONS(ModuleObject, initialEnvironment, InitialEnvironmentSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, requestedModules, RequestedModulesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, importEntries, ImportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, localExportEntries, LocalExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, indirectExportEntries, IndirectExportEntriesSlot)
DEFINE_GETTER_FUNCTIONS(ModuleObject, starExportEntries, StarExportEntriesSlot)

JSObject*
js::InitModuleClass(JSContext* cx, HandleObject obj)
{
    static const JSPropertySpec protoAccessors[] = {
        JS_PSG("initialEnvironment", ModuleObject_initialEnvironmentGetter, 0),
        JS_PSG("requestedModules", ModuleObject_requestedModulesGetter, 0),
        JS_PSG("importEntries", ModuleObject_importEntriesGetter, 0),
        JS_PSG("localExportEntries", ModuleObject_localExportEntriesGetter, 0),
        JS_PSG("indirectExportEntries", ModuleObject_indirectExportEntriesGetter, 0),
        JS_PSG("starExportEntries", ModuleObject_starExportEntriesGetter, 0),
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
    importEntries_(cx, ImportEntryVector(cx)),
    exportEntries_(cx, ExportEntryVector(cx)),
    localExportEntries_(cx, ExportEntryVector(cx)),
    indirectExportEntries_(cx, ExportEntryVector(cx)),
    starExportEntries_(cx, ExportEntryVector(cx))
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
          case PNK_EXPORT_DEFAULT:
            if (!processExport(pn))
                return false;
            break;

          case PNK_EXPORT_FROM:
            if (!processExportFrom(pn))
                return false;
            break;

          default:
            break;
        }
    }

    for (const auto& e : exportEntries_) {
        RootedExportEntry exp(cx_, e);
        if (!exp->moduleRequest()) {
            RootedImportEntry importEntry(cx_, importEntryFor(exp->localName()));
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
                    RootedExportEntry exportEntry(cx_);
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

    module->initImportExportData(requestedModules,
                                 importEntries,
                                 localExportEntries,
                                 indirectExportEntries,
                                 starExportEntries);

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
        if (!importEntry || !importEntries_.append(importEntry))
            return false;
    }

    return true;
}

bool
ModuleBuilder::processExport(frontend::ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_UNARY));

    ParseNode* kid = pn->pn_kid;
    bool isDefault = pn->getKind() == PNK_EXPORT_DEFAULT;

    switch (kid->getKind()) {
      case PNK_EXPORT_SPEC_LIST:
        MOZ_ASSERT(!isDefault);
        for (ParseNode* spec = kid->pn_head; spec; spec = spec->pn_next) {
            MOZ_ASSERT(spec->isKind(PNK_EXPORT_SPEC));
            RootedAtom localName(cx_, spec->pn_left->pn_atom);
            RootedAtom exportName(cx_, spec->pn_right->pn_atom);
            if (!appendLocalExportEntry(exportName, localName))
                return false;
        }
        break;

      case PNK_FUNCTION: {
          RootedFunction func(cx_, kid->pn_funbox->function());
          RootedAtom localName(cx_, func->atom());
          RootedAtom exportName(cx_, isDefault ? cx_->names().default_ : localName.get());
          if (!appendLocalExportEntry(exportName, localName))
              return false;
          break;
      }

      case PNK_CLASS: {
          const ClassNode& cls = kid->as<ClassNode>();
          MOZ_ASSERT(cls.names());
          RootedAtom localName(cx_, cls.names()->innerBinding()->pn_atom);
          RootedAtom exportName(cx_, isDefault ? cx_->names().default_ : localName.get());
          if (!appendLocalExportEntry(exportName, localName))
              return false;
          break;
      }

      case PNK_VAR:
      case PNK_CONST:
      case PNK_GLOBALCONST:
      case PNK_LET: {
          MOZ_ASSERT(kid->isArity(PN_LIST));
          for (ParseNode* var = kid->pn_head; var; var = var->pn_next) {
              if (var->isKind(PNK_ASSIGN))
                  var = var->pn_left;
              MOZ_ASSERT(var->isKind(PNK_NAME));
              RootedAtom localName(cx_, var->pn_atom);
              RootedAtom exportName(cx_, isDefault ? cx_->names().default_ : localName.get());
              if (!appendLocalExportEntry(exportName, localName))
                  return false;
          }
          break;
      }

      default:
        MOZ_ASSERT(isDefault);
        RootedAtom localName(cx_, cx_->names().starDefaultStar);
        RootedAtom exportName(cx_, cx_->names().default_);
        if (!appendLocalExportEntry(exportName, localName))
            return false;
        break;
    }
    return true;
}

bool
ModuleBuilder::processExportFrom(frontend::ParseNode* pn)
{
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
            if (!appendIndirectExportEntry(exportName, module, bindingName))
                return false;
        } else {
            MOZ_ASSERT(spec->isKind(PNK_EXPORT_BATCH_SPEC));
            RootedAtom importName(cx_, cx_->names().star);
            if (!appendIndirectExportEntry(nullptr, module, importName))
                return false;
        }
    }

    return true;
}

ImportEntryObject*
ModuleBuilder::importEntryFor(JSAtom* localName)
{
    for (auto import : importEntries_) {
        if (import->localName() == localName)
            return import;
    }
    return nullptr;
}

bool
ModuleBuilder::appendLocalExportEntry(HandleAtom exportName, HandleAtom localName)
{
    Rooted<ExportEntryObject*> exportEntry(cx_);
    exportEntry = ExportEntryObject::create(cx_, exportName, nullptr, nullptr, localName);
    return exportEntry && exportEntries_.append(exportEntry);
}

bool
ModuleBuilder::appendIndirectExportEntry(HandleAtom exportName, HandleAtom moduleRequest,
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
