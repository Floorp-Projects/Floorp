/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ModuleBuilder_h
#define vm_ModuleBuilder_h

#include "mozilla/Attributes.h"  // MOZ_STACK_CLASS

#include "builtin/ModuleObject.h"  // js::{{Im,Ex}portEntry,Requested{Module,}}Object
#include "frontend/EitherParser.h"  // js::frontend::EitherParser
#include "js/GCHashTable.h"         // JS::GCHash{Map,Set}
#include "js/GCVector.h"            // JS::GCVector
#include "js/RootingAPI.h"          // JS::{Handle,Rooted}
#include "vm/AtomsTable.h"          // js::AtomSet

struct JSContext;
class JSAtom;

namespace js {

namespace frontend {

class BinaryNode;
class ListNode;
class ParseNode;

}  // namespace frontend

// Process a module's parse tree to collate the import and export data used when
// creating a ModuleObject.
class MOZ_STACK_CLASS ModuleBuilder {
  explicit ModuleBuilder(JSContext* cx, JS::Handle<ModuleObject*> module,
                         const frontend::EitherParser& eitherParser);

 public:
  template <class Parser>
  explicit ModuleBuilder(JSContext* cx, JS::Handle<ModuleObject*> module,
                         Parser* parser)
      : ModuleBuilder(cx, module, frontend::EitherParser(parser)) {}

  bool processImport(frontend::BinaryNode* importNode);
  bool processExport(frontend::ParseNode* exportNode);
  bool processExportFrom(frontend::BinaryNode* exportNode);

  bool hasExportedName(JSAtom* name) const;

  using ExportEntryVector = GCVector<ExportEntryObject*>;
  const ExportEntryVector& localExportEntries() const {
    return localExportEntries_;
  }

  bool buildTables();
  bool initModule();

 private:
  using RequestedModuleVector = JS::GCVector<RequestedModuleObject*>;
  using AtomSet = JS::GCHashSet<JSAtom*>;
  using ImportEntryMap = JS::GCHashMap<JSAtom*, ImportEntryObject*>;
  using RootedExportEntryVector = JS::Rooted<ExportEntryVector>;
  using RootedRequestedModuleVector = JS::Rooted<RequestedModuleVector>;
  using RootedAtomSet = JS::Rooted<AtomSet>;
  using RootedImportEntryMap = JS::Rooted<ImportEntryMap>;

  JSContext* cx_;
  JS::Rooted<ModuleObject*> module_;
  frontend::EitherParser eitherParser_;
  RootedAtomSet requestedModuleSpecifiers_;
  RootedRequestedModuleVector requestedModules_;
  RootedImportEntryMap importEntries_;
  RootedExportEntryVector exportEntries_;
  RootedAtomSet exportNames_;
  RootedExportEntryVector localExportEntries_;
  RootedExportEntryVector indirectExportEntries_;
  RootedExportEntryVector starExportEntries_;

  ImportEntryObject* importEntryFor(JSAtom* localName) const;

  bool processExportBinding(frontend::ParseNode* pn);
  bool processExportArrayBinding(frontend::ListNode* array);
  bool processExportObjectBinding(frontend::ListNode* obj);

  bool appendImportEntryObject(JS::Handle<ImportEntryObject*> importEntry);

  bool appendExportEntry(JS::Handle<JSAtom*> exportName,
                         JS::Handle<JSAtom*> localName,
                         frontend::ParseNode* node = nullptr);

  bool appendExportFromEntry(JS::Handle<JSAtom*> exportName,
                             JS::Handle<JSAtom*> moduleRequest,
                             JS::Handle<JSAtom*> importName,
                             frontend::ParseNode* node);

  bool appendExportEntryObject(JS::Handle<ExportEntryObject*> exportEntry);

  bool maybeAppendRequestedModule(JS::Handle<JSAtom*> specifier,
                                  frontend::ParseNode* node);

  template <typename T>
  ArrayObject* createArray(const JS::Rooted<JS::GCVector<T>>& vector);
  template <typename K, typename V>
  ArrayObject* createArray(const JS::Rooted<JS::GCHashMap<K, V>>& map);
};

}  // namespace js

#endif  // vm_ModuleBuilder_h
