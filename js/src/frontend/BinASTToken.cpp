/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTToken.h"

#include "mozilla/Maybe.h"

#include <sys/types.h>

#include "frontend/BinASTRuntimeSupport.h"
#include "frontend/TokenStream.h"
#include "js/Result.h"
#include "vm/JSContext.h"

namespace js {
namespace frontend {

const BinaryASTSupport::CharSlice BINASTKIND_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME, _2) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_KIND(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice BINASTFIELD_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_FIELD(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice BINASTVARIANT_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_VARIANT(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice BINASTLIST_DESCRIPTIONS[] = {
#define NOTHING(_)
#define EMIT_NAME(_list_name, _content, SPEC_NAME, _type_name) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_LIST(EMIT_NAME, NOTHING, NOTHING, NOTHING, NOTHING, NOTHING,
                      NOTHING, NOTHING, NOTHING)
#undef EMIT_NAME
#undef NOTHING
};

const BinaryASTSupport::CharSlice BINASTINTERFACEANDFIELD_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_INTERFACE_AND_FIELD(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice& getBinASTKind(const BinASTKind& variant) {
  return BINASTKIND_DESCRIPTIONS[static_cast<size_t>(variant)];
}

const BinaryASTSupport::CharSlice& getBinASTVariant(
    const BinASTVariant& variant) {
  return BINASTVARIANT_DESCRIPTIONS[static_cast<size_t>(variant)];
}

const BinaryASTSupport::CharSlice& getBinASTField(const BinASTField& variant) {
  return BINASTFIELD_DESCRIPTIONS[static_cast<size_t>(variant)];
}

const BinaryASTSupport::CharSlice& getBinASTList(const BinASTList& list) {
  return BINASTLIST_DESCRIPTIONS[static_cast<size_t>(list)];
}

const BinaryASTSupport::CharSlice& getBinASTInterfaceAndField(
    const BinASTInterfaceAndField& interfaceAndField) {
  return BINASTINTERFACEANDFIELD_DESCRIPTIONS[static_cast<size_t>(
      interfaceAndField)];
}

const char* describeBinASTKind(const BinASTKind& kind) {
  return getBinASTKind(kind).begin();
}

const char* describeBinASTField(const BinASTField& field) {
  return getBinASTField(field).begin();
}

const char* describeBinASTVariant(const BinASTVariant& variant) {
  return getBinASTVariant(variant).begin();
}

const char* describeBinASTList(const BinASTList& list) {
  return getBinASTList(list).begin();
}

const char* describeBinASTInterfaceAndField(
    const BinASTInterfaceAndField& interfaceAndField) {
  return getBinASTInterfaceAndField(interfaceAndField).begin();
}

size_t getBinASTKindSortKey(const BinASTKind& kind) {
  return static_cast<size_t>(kind);
}

size_t getBinASTVariantSortKey(const BinASTVariant& variant) {
  return static_cast<size_t>(variant);
}

}  // namespace frontend

BinaryASTSupport::BinaryASTSupport()
    : binASTKindMap_(frontend::BINASTKIND_LIMIT),
      binASTFieldMap_(frontend::BINASTFIELD_LIMIT),
      binASTVariantMap_(frontend::BINASTVARIANT_LIMIT) {}

/**
 * It is expected that all bin tables are initialized on the main thread, and
 * that any helper threads will find the read-only tables properly initialized,
 * so that they can do their accesses safely without taking any locks.
 */
bool BinaryASTSupport::ensureBinTablesInitialized(JSContext* cx) {
  return ensureBinASTKindsInitialized(cx) &&
         ensureBinASTVariantsInitialized(cx);
}

bool BinaryASTSupport::ensureBinASTKindsInitialized(JSContext* cx) {
  MOZ_ASSERT(!cx->isHelperThreadContext());
  if (binASTKindMap_.empty()) {
    for (size_t i = 0; i < frontend::BINASTKIND_LIMIT; ++i) {
      const BinASTKind variant = static_cast<BinASTKind>(i);
      const CharSlice& key = getBinASTKind(variant);
      auto ptr = binASTKindMap_.lookupForAdd(key);
      MOZ_ASSERT(!ptr);
      if (!binASTKindMap_.add(ptr, key, variant)) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
  }

  return true;
}

bool BinaryASTSupport::ensureBinASTVariantsInitialized(JSContext* cx) {
  MOZ_ASSERT(!cx->isHelperThreadContext());
  if (binASTVariantMap_.empty()) {
    for (size_t i = 0; i < frontend::BINASTVARIANT_LIMIT; ++i) {
      const BinASTVariant variant = static_cast<BinASTVariant>(i);
      const CharSlice& key = getBinASTVariant(variant);
      auto ptr = binASTVariantMap_.lookupForAdd(key);
      MOZ_ASSERT(!ptr);
      if (!binASTVariantMap_.add(ptr, key, variant)) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
  }
  return true;
}

JS::Result<const js::frontend::BinASTKind*> BinaryASTSupport::binASTKind(
    JSContext* cx, const CharSlice key) {
  MOZ_ASSERT_IF(cx->isHelperThreadContext(), !binASTKindMap_.empty());
  if (!cx->isHelperThreadContext()) {
    // Initialize Lazily if on main thread.
    if (!ensureBinASTKindsInitialized(cx)) {
      return cx->alreadyReportedError();
    }
  }

  auto ptr = binASTKindMap_.readonlyThreadsafeLookup(key);
  if (!ptr) {
    return nullptr;
  }

  return &ptr->value();
}

JS::Result<const js::frontend::BinASTVariant*> BinaryASTSupport::binASTVariant(
    JSContext* cx, const CharSlice key) {
  MOZ_ASSERT_IF(cx->isHelperThreadContext(), !binASTVariantMap_.empty());
  if (!cx->isHelperThreadContext()) {
    // Initialize lazily if on main thread.
    if (!ensureBinASTVariantsInitialized(cx)) {
      return cx->alreadyReportedError();
    }
  }

  auto ptr = binASTVariantMap_.readonlyThreadsafeLookup(key);
  if (!ptr) {
    return nullptr;
  }

  return &ptr->value();
}

}  // namespace js
