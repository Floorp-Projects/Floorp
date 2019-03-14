/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinASTSupport_h
#define frontend_BinASTSupport_h

#include "mozilla/HashFunctions.h"

#include "frontend/BinASTToken.h"
#include "gc/DeletePolicy.h"

#include "js/AllocPolicy.h"
#include "js/HashTable.h"
#include "js/Result.h"
#include "js/UniquePtr.h"
#include "js/Vector.h"

namespace js {

class ScriptSource;

// Support for parsing JS Binary ASTs.
struct BinaryASTSupport {
  using BinASTVariant = js::frontend::BinASTVariant;
  using BinASTField = js::frontend::BinASTField;
  using BinASTKind = js::frontend::BinASTKind;

  // A structure designed to perform fast char* + length lookup
  // without copies.
  struct CharSlice {
    const char* start_;
    uint32_t byteLen_;
    CharSlice(const CharSlice& other)
        : start_(other.start_), byteLen_(other.byteLen_) {}
    CharSlice(const char* start, const uint32_t byteLen)
        : start_(start), byteLen_(byteLen) {}
    explicit CharSlice(JSContext*) : CharSlice(nullptr, 0) {}
    const char* begin() const { return start_; }
    const char* end() const { return start_ + byteLen_; }
#ifdef DEBUG
    void dump() const {
      for (auto c : *this) {
        fprintf(stderr, "%c", c);
      }

      fprintf(stderr, " (%d)", byteLen_);
    }
#endif  // DEBUG

    typedef const CharSlice Lookup;
    static js::HashNumber hash(Lookup l) {
      return mozilla::HashString(l.start_, l.byteLen_);
    }
    static bool match(const Lookup key, Lookup lookup) {
      if (key.byteLen_ != lookup.byteLen_) {
        return false;
      }
      return strncmp(key.start_, lookup.start_, key.byteLen_) == 0;
    }
  };

  BinaryASTSupport();

  JS::Result<const BinASTVariant*> binASTVariant(JSContext*, const CharSlice);
  JS::Result<const BinASTKind*> binASTKind(JSContext*, const CharSlice);

  bool ensureBinTablesInitialized(JSContext*);

 private:
  bool ensureBinASTKindsInitialized(JSContext*);
  bool ensureBinASTVariantsInitialized(JSContext*);

 private:
  // A HashMap that can be queried without copies from a CharSlice key.
  // Initialized on first call. Keys are CharSlices into static strings.
  using BinASTKindMap = js::HashMap<const CharSlice, BinASTKind, CharSlice,
                                    js::SystemAllocPolicy>;
  BinASTKindMap binASTKindMap_;

  using BinASTFieldMap = js::HashMap<const CharSlice, BinASTField, CharSlice,
                                     js::SystemAllocPolicy>;
  BinASTFieldMap binASTFieldMap_;

  using BinASTVariantMap = js::HashMap<const CharSlice, BinASTVariant,
                                       CharSlice, js::SystemAllocPolicy>;
  BinASTVariantMap binASTVariantMap_;
};

namespace frontend {

class BinASTSourceMetadata {
  using CharSlice = BinaryASTSupport::CharSlice;

  const uint32_t numStrings_;
  const uint32_t numBinASTKinds_;

  // The data lives inline in the allocation, after this class.
  inline JSAtom** atomsBase() {
    return reinterpret_cast<JSAtom**>(reinterpret_cast<uintptr_t>(this + 1));
  }
  inline CharSlice* sliceBase() {
    return reinterpret_cast<CharSlice*>(
        reinterpret_cast<uintptr_t>(atomsBase()) +
        numStrings_ * sizeof(JSAtom*));
  }
  inline BinASTKind* binASTKindBase() {
    return reinterpret_cast<BinASTKind*>(
        reinterpret_cast<uintptr_t>(sliceBase()) +
        numStrings_ * sizeof(CharSlice));
  }

  static inline size_t totalSize(uint32_t numBinASTKinds, uint32_t numStrings) {
    return sizeof(BinASTSourceMetadata) + numStrings * sizeof(JSAtom*) +
           numStrings * sizeof(CharSlice) + numBinASTKinds * sizeof(BinASTKind);
  }

  BinASTSourceMetadata(uint32_t numBinASTKinds, uint32_t numStrings)
      : numStrings_(numStrings), numBinASTKinds_(numBinASTKinds) {}

  friend class js::ScriptSource;

 public:
  static BinASTSourceMetadata* Create(const Vector<BinASTKind>& binASTKinds,
                                      uint32_t numStrings);

  inline uint32_t numBinASTKinds() { return numBinASTKinds_; }

  inline uint32_t numStrings() { return numStrings_; }

  inline BinASTKind& getBinASTKind(uint32_t index) {
    MOZ_ASSERT(index < numBinASTKinds_);
    return binASTKindBase()[index];
  }

  inline CharSlice& getSlice(uint32_t index) {
    MOZ_ASSERT(index < numStrings_);
    return sliceBase()[index];
  }

  inline JSAtom*& getAtom(uint32_t index) {
    MOZ_ASSERT(index < numStrings_);
    return atomsBase()[index];
  }

  void trace(JSTracer* tracer);
};

}  // namespace frontend

typedef UniquePtr<frontend::BinASTSourceMetadata,
                  GCManagedDeletePolicy<frontend::BinASTSourceMetadata>>
    UniqueBinASTSourceMetadataPtr;

}  // namespace js

#endif  // frontend_BinASTSupport_h
