/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ModuleBuilder_h
#define vm_ModuleBuilder_h

#include "mozilla/Attributes.h"  // MOZ_STACK_CLASS

#include "jstypes.h"               // JS_PUBLIC_API
#include "builtin/ModuleObject.h"  // js::{{Im,Ex}portEntry,Requested{Module,}}Object
#include "frontend/EitherParser.h"  // js::frontend::EitherParser
#include "frontend/ParserAtom.h"  // js::frontend::{ParserAtom, TaggedParserAtomIndex}
#include "frontend/Stencil.h"  // js::frontend::StencilModuleEntry
#include "js/GCHashTable.h"    // JS::GCHash{Map,Set}
#include "js/GCVector.h"       // JS::GCVector
#include "js/RootingAPI.h"     // JS::{Handle,Rooted}
#include "vm/AtomsTable.h"     // js::AtomSet

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSAtom;

namespace js {

namespace frontend {

class BinaryNode;
class ListNode;
class ParseNode;

}  // namespace frontend

class TaggedParserAtomIndexHasher {
 public:
  using Lookup = frontend::TaggedParserAtomIndex;

  static inline HashNumber hash(const Lookup& l) {
    return HashNumber(
        *const_cast<frontend::TaggedParserAtomIndex&>(l).rawData());
  }
  static inline bool match(frontend::TaggedParserAtomIndex entry,
                           const Lookup& l) {
    return l == entry;
  }
};

// Process a module's parse tree to collate the import and export data used when
// creating a ModuleObject.
class MOZ_STACK_CLASS ModuleBuilder {
  explicit ModuleBuilder(JSContext* cx,
                         const frontend::EitherParser& eitherParser);

 public:
  template <class Parser>
  explicit ModuleBuilder(JSContext* cx, Parser* parser)
      : ModuleBuilder(cx, frontend::EitherParser(parser)) {}

  bool processImport(frontend::BinaryNode* importNode);
  bool processExport(frontend::ParseNode* exportNode);
  bool processExportFrom(frontend::BinaryNode* exportNode);

  bool hasExportedName(const frontend::ParserAtom* name) const;

  bool buildTables(frontend::StencilModuleMetadata& metadata);

  // During BytecodeEmitter we note top-level functions, and afterwards we must
  // call finishFunctionDecls on the list.
  bool noteFunctionDeclaration(JSContext* cx, uint32_t funIndex);
  void finishFunctionDecls(frontend::StencilModuleMetadata& metadata);

  void noteAsync(frontend::StencilModuleMetadata& metadata);

 private:
  using RequestedModuleVector =
      Vector<frontend::StencilModuleEntry, 0, js::SystemAllocPolicy>;
  using AtomSet = HashSet<const frontend::ParserAtom*>;
  using ExportEntryVector = Vector<frontend::StencilModuleEntry>;
  using ImportEntryMap =
      HashMap<frontend::TaggedParserAtomIndex, frontend::StencilModuleEntry,
              TaggedParserAtomIndexHasher>;

  JSContext* cx_;
  frontend::EitherParser eitherParser_;

  // These are populated while parsing.
  AtomSet requestedModuleSpecifiers_;
  RequestedModuleVector requestedModules_;
  ImportEntryMap importEntries_;
  ExportEntryVector exportEntries_;
  AtomSet exportNames_;

  // These are populated while emitting bytecode.
  frontend::FunctionDeclarationVector functionDecls_;

  frontend::StencilModuleEntry* importEntryFor(
      frontend::TaggedParserAtomIndex localName) const;

  bool processExportBinding(frontend::ParseNode* pn);
  bool processExportArrayBinding(frontend::ListNode* array);
  bool processExportObjectBinding(frontend::ListNode* obj);

  bool appendExportEntry(const frontend::ParserAtom* exportName,
                         const frontend::ParserAtom* localName,
                         frontend::ParseNode* node = nullptr);

  bool appendExportFromEntry(const frontend::ParserAtom* exportName,
                             const frontend::ParserAtom* moduleRequest,
                             const frontend::ParserAtom* importName,
                             frontend::ParseNode* node);

  bool maybeAppendRequestedModule(const frontend::ParserAtom* specifier,
                                  frontend::ParseNode* node);
};

template <typename T>
ArrayObject* CreateArray(JSContext* cx,
                         const JS::Rooted<JS::GCVector<T>>& vector);

}  // namespace js

#endif  // vm_ModuleBuilder_h
