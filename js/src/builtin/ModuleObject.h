/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ModuleObject_h
#define builtin_ModuleObject_h

#include "jsapi.h"
#include "jsatom.h"

#include "js/TraceableVector.h"

#include "vm/NativeObject.h"

namespace js {

class ModuleEnvironmentObject;

namespace frontend {
class ParseNode;
} /* namespace frontend */

class ImportEntryObject : public NativeObject
{
  public:
    enum
    {
        ModuleRequestSlot = 0,
        ImportNameSlot,
        LocalNameSlot,
        SlotCount
    };

    static const Class class_;
    static JSObject* initClass(JSContext* cx, HandleObject obj);
    static bool isInstance(HandleValue value);
    static ImportEntryObject* create(JSContext* cx,
                                     HandleAtom moduleRequest,
                                     HandleAtom importName,
                                     HandleAtom localName);
    JSAtom* moduleRequest();
    JSAtom* importName();
    JSAtom* localName();
};

class ExportEntryObject : public NativeObject
{
  public:
    enum
    {
        ExportNameSlot = 0,
        ModuleRequestSlot,
        ImportNameSlot,
        LocalNameSlot,
        SlotCount
    };

    static const Class class_;
    static JSObject* initClass(JSContext* cx, HandleObject obj);
    static bool isInstance(HandleValue value);
    static ExportEntryObject* create(JSContext* cx,
                                     HandleAtom maybeExportName,
                                     HandleAtom maybeModuleRequest,
                                     HandleAtom maybeImportName,
                                     HandleAtom maybeLocalName);
    JSAtom* exportName();
    JSAtom* moduleRequest();
    JSAtom* importName();
    JSAtom* localName();
};

struct IndirectBinding
{
    IndirectBinding(Handle<ModuleEnvironmentObject*> environment, HandleId localName);
    RelocatablePtr<ModuleEnvironmentObject*> environment;
    RelocatableId localName;
};

typedef HashMap<jsid, IndirectBinding, JsidHasher, SystemAllocPolicy> IndirectBindingMap;

class ModuleObject : public NativeObject
{
  public:
    enum
    {
        ScriptSlot = 0,
        InitialEnvironmentSlot,
        EnvironmentSlot,
        EvaluatedSlot,
        RequestedModulesSlot,
        ImportEntriesSlot,
        LocalExportEntriesSlot,
        IndirectExportEntriesSlot,
        StarExportEntriesSlot,
        ImportBindingsSlot,
        SlotCount
    };

    static const Class class_;

    static bool isInstance(HandleValue value);

    static ModuleObject* create(ExclusiveContext* cx);
    void init(HandleScript script);
    void setInitialEnvironment(Handle<ModuleEnvironmentObject*> initialEnvironment);
    void initImportExportData(HandleArrayObject requestedModules,
                              HandleArrayObject importEntries,
                              HandleArrayObject localExportEntries,
                              HandleArrayObject indiretExportEntries,
                              HandleArrayObject starExportEntries);

    JSScript* script() const;
    ModuleEnvironmentObject& initialEnvironment() const;
    ModuleEnvironmentObject* environment() const;
    bool evaluated() const;
    ArrayObject& requestedModules() const;
    ArrayObject& importEntries() const;
    ArrayObject& localExportEntries() const;
    ArrayObject& indirectExportEntries() const;
    ArrayObject& starExportEntries() const;
    JSObject* enclosingStaticScope() const;
    IndirectBindingMap& importBindings();

    void createEnvironment();

    void setEvaluated();
    bool evaluate(JSContext*cx, MutableHandleValue rval);

  private:
    static void trace(JSTracer* trc, JSObject* obj);
    static void finalize(js::FreeOp* fop, JSObject* obj);

    bool hasScript() const;
};

typedef Rooted<ModuleObject*> RootedModuleObject;
typedef Handle<ModuleObject*> HandleModuleObject;

// Process a module's parse tree to collate the import and export data used when
// creating a ModuleObject.
class MOZ_STACK_CLASS ModuleBuilder
{
  public:
    explicit ModuleBuilder(JSContext* cx);

    bool buildAndInit(frontend::ParseNode* pn, HandleModuleObject module);

  private:
    using AtomVector = TraceableVector<JSAtom*>;
    using RootedAtomVector = JS::Rooted<AtomVector>;
    using ImportEntryVector = TraceableVector<ImportEntryObject*>;
    using RootedImportEntryVector = JS::Rooted<ImportEntryVector>;
    using ExportEntryVector = TraceableVector<ExportEntryObject*> ;
    using RootedExportEntryVector = JS::Rooted<ExportEntryVector> ;

    JSContext* cx_;
    RootedAtomVector requestedModules_;

    RootedAtomVector importedBoundNames_;
    RootedImportEntryVector importEntries_;
    RootedExportEntryVector exportEntries_;
    RootedExportEntryVector localExportEntries_;
    RootedExportEntryVector indirectExportEntries_;
    RootedExportEntryVector starExportEntries_;

    bool processImport(frontend::ParseNode* pn);
    bool processExport(frontend::ParseNode* pn);
    bool processExportFrom(frontend::ParseNode* pn);

    ImportEntryObject* importEntryFor(JSAtom* localName);

    bool appendLocalExportEntry(HandleAtom exportName, HandleAtom localName);
    bool appendIndirectExportEntry(HandleAtom exportName, HandleAtom moduleRequest,
                                   HandleAtom importName);

    bool maybeAppendRequestedModule(HandleAtom module);

    template <typename T>
    ArrayObject* createArray(const TraceableVector<T>& vector);
};

JSObject* InitModuleClass(JSContext* cx, HandleObject obj);
JSObject* InitImportEntryClass(JSContext* cx, HandleObject obj);
JSObject* InitExportEntryClass(JSContext* cx, HandleObject obj);

} // namespace js

#endif /* builtin_ModuleObject_h */
