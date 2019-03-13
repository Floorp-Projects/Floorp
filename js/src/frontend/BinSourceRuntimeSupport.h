/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinSourceSupport_h
#define frontend_BinSourceSupport_h

#include "mozilla/HashFunctions.h"

#include "frontend/BinToken.h"
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
  using BinVariant = js::frontend::BinVariant;
  using BinField = js::frontend::BinField;
  using BinKind = js::frontend::BinKind;

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

  JS::Result<const BinVariant*> binVariant(JSContext*, const CharSlice);
  JS::Result<const BinKind*> binKind(JSContext*, const CharSlice);

  bool ensureBinTablesInitialized(JSContext*);

 private:
  bool ensureBinKindsInitialized(JSContext*);
  bool ensureBinVariantsInitialized(JSContext*);

 private:
  // A HashMap that can be queried without copies from a CharSlice key.
  // Initialized on first call. Keys are CharSlices into static strings.
  using BinKindMap =
      js::HashMap<const CharSlice, BinKind, CharSlice, js::SystemAllocPolicy>;
  BinKindMap binKindMap_;

  using BinFieldMap =
      js::HashMap<const CharSlice, BinField, CharSlice, js::SystemAllocPolicy>;
  BinFieldMap binFieldMap_;

  using BinVariantMap = js::HashMap<const CharSlice, BinVariant, CharSlice,
                                    js::SystemAllocPolicy>;
  BinVariantMap binVariantMap_;
};

namespace frontend {

class BinASTSourceMetadata {
  using CharSlice = BinaryASTSupport::CharSlice;

  const uint32_t numStrings_;
  const uint32_t numBinKinds_;

  // The data lives inline in the allocation, after this class.
  inline JSAtom** atomsBase() {
    return reinterpret_cast<JSAtom**>(reinterpret_cast<uintptr_t>(this + 1));
  }
  inline CharSlice* sliceBase() {
    return reinterpret_cast<CharSlice*>(
        reinterpret_cast<uintptr_t>(atomsBase()) +
        numStrings_ * sizeof(JSAtom*));
  }
  inline BinKind* binKindBase() {
    return reinterpret_cast<BinKind*>(reinterpret_cast<uintptr_t>(sliceBase()) +
                                      numStrings_ * sizeof(CharSlice));
  }

  static inline size_t totalSize(uint32_t numBinKinds, uint32_t numStrings) {
    return sizeof(BinASTSourceMetadata) + numStrings * sizeof(JSAtom*) +
           numStrings * sizeof(CharSlice) + numBinKinds * sizeof(BinKind);
  }

  BinASTSourceMetadata(uint32_t numBinKinds, uint32_t numStrings)
      : numStrings_(numStrings), numBinKinds_(numBinKinds) {}

  friend class js::ScriptSource;

 public:
  static BinASTSourceMetadata* Create(const Vector<BinKind>& binKinds,
                                      uint32_t numStrings);

  inline uint32_t numBinKinds() { return numBinKinds_; }

  inline uint32_t numStrings() { return numStrings_; }

  inline BinKind& getBinKind(uint32_t index) {
    MOZ_ASSERT(index < numBinKinds_);
    return binKindBase()[index];
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

#endif  // frontend_BinSourceSupport_h
