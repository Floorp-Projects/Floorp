/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ModuleObject_h
#define builtin_ModuleObject_h

#include "jsapi.h"

#include "js/TraceableVector.h"

#include "vm/NativeObject.h"

namespace js {

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

class ModuleObject : public NativeObject
{
  public:
    enum
    {
        ScriptSlot = 0,
        RequestedModulesSlot,
        ImportEntriesSlot,
        SlotCount
    };

    static const Class class_;

    static bool isInstance(HandleValue value);

    static ModuleObject* create(ExclusiveContext* cx);
    void init(HandleScript script);
    void initImportExportData(HandleArrayObject requestedModules,
                              HandleArrayObject importEntries);

    JSScript* script() const;
    ArrayObject& requestedModules() const;
    ArrayObject& importEntries() const;

  private:
    static void trace(JSTracer* trc, JSObject* obj);
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

    JSContext* cx_;
    RootedAtomVector requestedModules_;
    RootedAtomVector importedBoundNames_;
    RootedImportEntryVector importEntries_;

    bool processImport(frontend::ParseNode* pn);
    bool processExportFrom(frontend::ParseNode* pn);

    bool maybeAppendRequestedModule(HandleAtom module);

    template <typename T>
    ArrayObject* createArray(const TraceableVector<T>& vector);
};

JSObject* InitModuleClass(JSContext* cx, HandleObject obj);
JSObject* InitImportEntryClass(JSContext* cx, HandleObject obj);

} // namespace js

#endif /* builtin_ModuleObject_h */
