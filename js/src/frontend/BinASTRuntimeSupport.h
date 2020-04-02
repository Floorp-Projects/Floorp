/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinASTSupport_h
#define frontend_BinASTSupport_h

#include "mozilla/Assertions.h"     // MOZ_ASSERT
#include "mozilla/HashFunctions.h"  // mozilla::HashString
#include "mozilla/Likely.h"         // MOZ_UNLIKELY

#include <stdint.h>  // uint8_t, uint32_t
#include <string.h>  // strncmp

#include "frontend/BinASTToken.h"  // BinASTVariant, BinASTField, BinASTKind
#include "gc/DeletePolicy.h"       // GCManagedDeletePolicy

#include "js/AllocPolicy.h"  // SystemAllocPolicy
#include "js/HashTable.h"    // HashMap, HashNumber
#include "js/Result.h"       // JS::Result
#include "js/TracingAPI.h"   // JSTracer
#include "js/UniquePtr.h"    // UniquePtr
#include "js/Vector.h"       // Vector

class JSAtom;
struct JS_PUBLIC_API JSContext;

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
    CharSlice(const CharSlice& other) = default;
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

      fprintf(stderr, " (%u)", byteLen_);
    }
#endif  // DEBUG

    using Lookup = const CharSlice;
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

class BinASTSourceMetadataMultipart;
class BinASTSourceMetadataContext;

class BinASTSourceMetadata {
 public:
  enum class Type : uint8_t {
    Multipart = 0,
    Context = 1,
  };

 private:
  Type type_;

 public:
  BinASTSourceMetadata() = delete;
  explicit BinASTSourceMetadata(Type type) : type_(type) {}
#ifdef JS_BUILD_BINAST
  ~BinASTSourceMetadata();
#else
  // If JS_BUILD_BINAST isn't defined, BinASTRuntimeSupport.cpp isn't built,
  // but still the destructor is referred from ScriptSource::BinAST.
  ~BinASTSourceMetadata() {}
#endif  // JS_BUILD_BINAST

  Type type() const { return type_; }

  void setType(Type type) { type_ = type; }

  bool isMultipart() const { return type_ == Type::Multipart; }
  bool isContext() const { return type_ == Type::Context; }

  inline BinASTSourceMetadataMultipart* asMultipart();
  inline BinASTSourceMetadataContext* asContext();

  void trace(JSTracer* tracer);
};

class alignas(uintptr_t) BinASTSourceMetadataMultipart
    : public BinASTSourceMetadata {
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
    return sizeof(BinASTSourceMetadataMultipart) +
           numStrings * sizeof(JSAtom*) + numStrings * sizeof(CharSlice) +
           numBinASTKinds * sizeof(BinASTKind);
  }

  BinASTSourceMetadataMultipart(uint32_t numBinASTKinds, uint32_t numStrings)
      : BinASTSourceMetadata(Type::Multipart),
        numStrings_(numStrings),
        numBinASTKinds_(numBinASTKinds) {}

  void release() {}

  friend class BinASTSourceMetadata;

  friend class js::ScriptSource;

 public:
  // Create the BinASTSourceMetadataMultipart instance, with copying
  // `binASTKinds`, and allocating `numStrings` atoms/slices.
  //
  // Use this when the list of BinASTKind is known at the point of allocating
  // metadata, like when parsing .binjs file.
  static BinASTSourceMetadataMultipart* create(
      const Vector<BinASTKind>& binASTKinds, uint32_t numStrings);

  // Create the BinASTSourceMetadataMultipart instance, with allocating
  // `numBinASTKinds` BinASTKinds and `numStrings` atoms/slices.
  //
  // Use this when the list of BinASTKind is unknown at the point of allocating
  // metadata, like when performing XDR decode.
  static BinASTSourceMetadataMultipart* create(uint32_t numBinASTKinds,
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

class HuffmanDictionaryForMetadata;

class alignas(uintptr_t) BinASTSourceMetadataContext
    : public BinASTSourceMetadata {
  const uint32_t numStrings_;
  HuffmanDictionaryForMetadata* dictionary_;

  // The data lives inline in the allocation, after this class.
  inline JSAtom** atomsBase() {
    return reinterpret_cast<JSAtom**>(reinterpret_cast<uintptr_t>(this + 1));
  }

  static inline size_t totalSize(uint32_t numStrings) {
    return sizeof(BinASTSourceMetadataContext) + numStrings * sizeof(JSAtom*);
  }

  explicit BinASTSourceMetadataContext(uint32_t numStrings)
      : BinASTSourceMetadata(Type::Context),
        numStrings_(numStrings),
        dictionary_(nullptr) {}

  void release();

  friend class BinASTSourceMetadata;

  friend class js::ScriptSource;

 public:
  // Create the BinASTSourceMetadataContext instance, with allocating
  // `numStrings` atoms.
  static BinASTSourceMetadataContext* create(uint32_t numStrings);

  HuffmanDictionaryForMetadata* dictionary() { return dictionary_; }
  void setDictionary(HuffmanDictionaryForMetadata* dictionary) {
    MOZ_ASSERT(!dictionary_);
    MOZ_ASSERT(dictionary);

    dictionary_ = dictionary;
  }

  inline uint32_t numStrings() { return numStrings_; }

  inline JSAtom*& getAtom(uint32_t index) {
    MOZ_ASSERT(index < numStrings_);
    return atomsBase()[index];
  }

  void trace(JSTracer* tracer);
};

BinASTSourceMetadataMultipart* BinASTSourceMetadata::asMultipart() {
  MOZ_ASSERT(isMultipart());
  return reinterpret_cast<BinASTSourceMetadataMultipart*>(this);
}

BinASTSourceMetadataContext* BinASTSourceMetadata::asContext() {
  MOZ_ASSERT(isContext());
  return reinterpret_cast<BinASTSourceMetadataContext*>(this);
}

}  // namespace frontend

using UniqueBinASTSourceMetadataPtr =
    UniquePtr<frontend::BinASTSourceMetadata,
              GCManagedDeletePolicy<frontend::BinASTSourceMetadata>>;

}  // namespace js

#endif  // frontend_BinASTSupport_h
