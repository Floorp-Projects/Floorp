/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParserAtom_h
#define frontend_ParserAtom_h

#include "mozilla/DebugOnly.h"      // mozilla::DebugOnly
#include "mozilla/HashFunctions.h"  // HashString
#include "mozilla/Range.h"          // mozilla::Range
#include "mozilla/Span.h"           // mozilla::Span
#include "mozilla/Variant.h"        // mozilla::Variant

#include "ds/LifoAlloc.h"         // LifoAlloc
#include "frontend/TypedIndex.h"  // TypedIndex
#include "js/HashTable.h"         // HashMap
#include "js/UniquePtr.h"         // js::UniquePtr
#include "js/Vector.h"            // Vector
#include "vm/CommonPropertyNames.h"
#include "vm/StringType.h"  // CompareChars, StringEqualsAscii

namespace js {
namespace frontend {

struct CompilationAtomCache;
struct CompilationStencil;
class ParserAtom;

template <typename CharT>
class SpecificParserAtomLookup;

class ParserAtomsTable;

// An index to map WellKnownParserAtoms to cx->names().
// This is consistent across multiple compilation.
//
// GetWellKnownAtom in ParserAtom.cpp relies on the fact that
// JSAtomState fields and this enum variants use the same order.
enum class WellKnownAtomId : uint32_t {
#define ENUM_ENTRY_(_, NAME, _2) NAME,
  FOR_EACH_COMMON_PROPERTYNAME(ENUM_ENTRY_)
#undef ENUM_ENTRY_

#define ENUM_ENTRY_(NAME, _) NAME,
      JS_FOR_EACH_PROTOTYPE(ENUM_ENTRY_)
#undef ENUM_ENTRY_
          Limit,
};

// These types correspond into indices in the StaticStrings arrays.
enum class StaticParserString1 : uint8_t;
enum class StaticParserString2 : uint16_t;

class ParserAtom;
using ParserAtomIndex = TypedIndex<ParserAtom>;

// ParserAtomIndex, WellKnownAtomId, StaticParserString1, StaticParserString2,
// or null.
//
// 0x0000_0000  Null atom
//
// 0x1YYY_YYYY  28-bit ParserAtom
//
// 0x2000_YYYY  Well-known atom ID
// 0x2001_YYYY  Static length-1 atom
// 0x2002_YYYY  Static length-2 atom
class TaggedParserAtomIndex {
  uint32_t data_;

 public:
  static constexpr size_t IndexBit = 28;
  static constexpr size_t IndexMask = BitMask(IndexBit);

  static constexpr size_t TagShift = IndexBit;
  static constexpr size_t TagBit = 4;
  static constexpr size_t TagMask = BitMask(TagBit) << TagShift;

  enum class Kind : uint32_t {
    Null = 0,
    ParserAtomIndex,
    WellKnown,
  };

 private:
  static constexpr size_t SmallIndexBit = 16;
  static constexpr size_t SmallIndexMask = BitMask(SmallIndexBit);

  static constexpr size_t SubTagShift = SmallIndexBit;
  static constexpr size_t SubTagBit = 2;
  static constexpr size_t SubTagMask = BitMask(SubTagBit) << SubTagShift;

 public:
  static constexpr uint32_t NullTag = uint32_t(Kind::Null) << TagShift;
  static constexpr uint32_t ParserAtomIndexTag = uint32_t(Kind::ParserAtomIndex)
                                                 << TagShift;
  static constexpr uint32_t WellKnownTag = uint32_t(Kind::WellKnown)
                                           << TagShift;

 private:
  static constexpr uint32_t WellKnownSubTag = 0 << SubTagShift;
  static constexpr uint32_t Static1SubTag = 1 << SubTagShift;
  static constexpr uint32_t Static2SubTag = 2 << SubTagShift;

 public:
  static constexpr uint32_t IndexLimit = Bit(IndexBit);
  static constexpr uint32_t SmallIndexLimit = Bit(SmallIndexBit);

 private:
  explicit TaggedParserAtomIndex(uint32_t data) : data_(data) {}

 public:
  constexpr TaggedParserAtomIndex() : data_(NullTag) {}

  explicit constexpr TaggedParserAtomIndex(ParserAtomIndex index)
      : data_(index.index | ParserAtomIndexTag) {
    MOZ_ASSERT(index.index < IndexLimit);
  }
  explicit constexpr TaggedParserAtomIndex(WellKnownAtomId index)
      : data_(uint32_t(index) | WellKnownTag | WellKnownSubTag) {
    MOZ_ASSERT(uint32_t(index) < SmallIndexLimit);

    // Static1/Static2 string shouldn't use WellKnownAtomId.
#define CHECK_(_, NAME, _2) MOZ_ASSERT(index != WellKnownAtomId::NAME);
    FOR_EACH_NON_EMPTY_TINY_PROPERTYNAME(CHECK_)
#undef CHECK_
  }
  explicit constexpr TaggedParserAtomIndex(StaticParserString1 index)
      : data_(uint32_t(index) | WellKnownTag | Static1SubTag) {}
  explicit constexpr TaggedParserAtomIndex(StaticParserString2 index)
      : data_(uint32_t(index) | WellKnownTag | Static2SubTag) {}

  class WellKnown {
   public:
#define METHOD_(_, NAME, _2)                             \
  static constexpr TaggedParserAtomIndex NAME() {        \
    return TaggedParserAtomIndex(WellKnownAtomId::NAME); \
  }
    FOR_EACH_NONTINY_COMMON_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(NAME, _)                                 \
  static constexpr TaggedParserAtomIndex NAME() {        \
    return TaggedParserAtomIndex(WellKnownAtomId::NAME); \
  }
    JS_FOR_EACH_PROTOTYPE(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                    \
  static constexpr TaggedParserAtomIndex NAME() {                \
    return TaggedParserAtomIndex(StaticParserString1((STR)[0])); \
  }
    FOR_EACH_LENGTH1_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                         \
  static constexpr TaggedParserAtomIndex NAME() {                     \
    return TaggedParserAtomIndex(StaticParserString2(                 \
        (StaticStrings::getLength2IndexStatic((STR)[0], (STR)[1])))); \
  }
    FOR_EACH_LENGTH2_PROPERTYNAME(METHOD_)
#undef METHOD_

    static constexpr TaggedParserAtomIndex empty() {
      return TaggedParserAtomIndex(WellKnownAtomId::empty);
    }
  };

  // The value of rawData() for WellKnown TaggedParserAtomIndex.
  // For using in switch-case.
  class WellKnownRawData {
   public:
#define METHOD_(_, NAME, _2)                                                 \
  static constexpr uint32_t NAME() {                                         \
    return uint32_t(WellKnownAtomId::NAME) | WellKnownTag | WellKnownSubTag; \
  }
    FOR_EACH_NONTINY_COMMON_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(NAME, _)                                                     \
  static constexpr uint32_t NAME() {                                         \
    return uint32_t(WellKnownAtomId::NAME) | WellKnownTag | WellKnownSubTag; \
  }
    JS_FOR_EACH_PROTOTYPE(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                 \
  static constexpr uint32_t NAME() {                          \
    return uint32_t((STR)[0]) | WellKnownTag | Static1SubTag; \
  }
    FOR_EACH_LENGTH1_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                              \
  static constexpr uint32_t NAME() {                                       \
    return uint32_t(                                                       \
               StaticStrings::getLength2IndexStatic((STR)[0], (STR)[1])) | \
           WellKnownTag | Static2SubTag;                                   \
  }
    FOR_EACH_LENGTH2_PROPERTYNAME(METHOD_)
#undef METHOD_

    static constexpr uint32_t empty() {
      return uint32_t(WellKnownAtomId::empty) | WellKnownTag | WellKnownSubTag;
    }
  };

  // NOTE: this is not well-known "null".
  static TaggedParserAtomIndex null() { return TaggedParserAtomIndex(); }

#ifdef DEBUG
  void validateRaw();
#endif

  static TaggedParserAtomIndex fromRaw(uint32_t data) {
    auto result = TaggedParserAtomIndex(data);
#ifdef DEBUG
    result.validateRaw();
#endif
    return result;
  }

  bool isParserAtomIndex() const {
    return (data_ & TagMask) == ParserAtomIndexTag;
  }
  bool isWellKnownAtomId() const {
    return (data_ & (TagMask | SubTagMask)) == (WellKnownTag | WellKnownSubTag);
  }
  bool isStaticParserString1() const {
    return (data_ & (TagMask | SubTagMask)) == (WellKnownTag | Static1SubTag);
  }
  bool isStaticParserString2() const {
    return (data_ & (TagMask | SubTagMask)) == (WellKnownTag | Static2SubTag);
  }
  bool isNull() const {
    bool result = !data_;
    MOZ_ASSERT_IF(result, (data_ & TagMask) == NullTag);
    return result;
  }

  ParserAtomIndex toParserAtomIndex() const {
    MOZ_ASSERT(isParserAtomIndex());
    return ParserAtomIndex(data_ & IndexMask);
  }
  WellKnownAtomId toWellKnownAtomId() const {
    MOZ_ASSERT(isWellKnownAtomId());
    return WellKnownAtomId(data_ & SmallIndexMask);
  }
  StaticParserString1 toStaticParserString1() const {
    MOZ_ASSERT(isStaticParserString1());
    return StaticParserString1(data_ & SmallIndexMask);
  }
  StaticParserString2 toStaticParserString2() const {
    MOZ_ASSERT(isStaticParserString2());
    return StaticParserString2(data_ & SmallIndexMask);
  }

  uint32_t* rawDataRef() { return &data_; }
  uint32_t rawData() const { return data_; }

  bool operator==(const TaggedParserAtomIndex& rhs) const {
    return data_ == rhs.data_;
  }
  bool operator!=(const TaggedParserAtomIndex& rhs) const {
    return data_ != rhs.data_;
  }

  explicit operator bool() const { return !isNull(); }
};

// Trivial variant of TaggedParserAtomIndex, to use in collection that requires
// trivial type.
// Provides minimal set of methods to use in collection.
class TrivialTaggedParserAtomIndex {
  uint32_t data_;

 public:
  static TrivialTaggedParserAtomIndex from(TaggedParserAtomIndex index) {
    TrivialTaggedParserAtomIndex result;
    result.data_ = index.rawData();
    return result;
  }

  operator TaggedParserAtomIndex() const {
    return TaggedParserAtomIndex::fromRaw(data_);
  }

  static TrivialTaggedParserAtomIndex null() {
    TrivialTaggedParserAtomIndex result;
    result.data_ = 0;
    return result;
  }

  bool isNull() const {
    static_assert(TaggedParserAtomIndex::NullTag == 0);
    return data_ == 0;
  }

  uint32_t rawData() const { return data_; }

  bool operator==(const TrivialTaggedParserAtomIndex& rhs) const {
    return data_ == rhs.data_;
  }
  bool operator!=(const TrivialTaggedParserAtomIndex& rhs) const {
    return data_ != rhs.data_;
  }

  explicit operator bool() const { return !isNull(); }
};

/**
 * A ParserAtom is an in-parser representation of an interned atomic
 * string.  It mostly mirrors the information carried by a JSAtom*.
 *
 * The atom contents are stored in one of two locations:
 *  1. Inline Latin1Char storage (immediately after the ParserAtom memory).
 *  2. Inline char16_t storage (immediately after the ParserAtom memory).
 */
class alignas(alignof(uint32_t)) ParserAtom {
  friend class ParserAtomsTable;
  friend class WellKnownParserAtoms;
  friend class WellKnownParserAtoms_ROM;

  static const uint16_t MAX_LATIN1_CHAR = 0xff;

  // Bit flags inside flags_.
  static constexpr uint32_t HasTwoByteCharsFlag = 1 << 0;
  static constexpr uint32_t UsedByStencilFlag = 1 << 1;
  static constexpr uint32_t WellKnownOrStaticFlag = 1 << 2;

  // Helper routine to read some sequence of two-byte chars, and write them
  // into a target buffer of a particular character width.
  //
  // The characters in the sequence must have been verified prior
  template <typename CharT, typename SeqCharT>
  static void drainChar16Seq(CharT* buf, InflatedChar16Sequence<SeqCharT> seq,
                             uint32_t length) {
    static_assert(
        std::is_same_v<CharT, char16_t> || std::is_same_v<CharT, Latin1Char>,
        "Invalid target buffer type.");
    CharT* cur = buf;
    while (seq.hasMore()) {
      char16_t ch = seq.next();
      if constexpr (std::is_same_v<CharT, Latin1Char>) {
        MOZ_ASSERT(ch <= MAX_LATIN1_CHAR);
      }
      MOZ_ASSERT(cur < (buf + length));
      *cur = ch;
      cur++;
    }
  }

 private:
  // The JSAtom-compatible hash of the string.
  HashNumber hash_ = 0;

  // The length of the buffer in chars_.
  uint32_t length_ = 0;

  uint32_t flags_ = 0;

  // End of fields.

  static const uint32_t MAX_LENGTH = JSString::MAX_LENGTH;

  ParserAtom(uint32_t length, HashNumber hash, bool hasTwoByteChars)
      : hash_(hash),
        length_(length),
        flags_(hasTwoByteChars ? HasTwoByteCharsFlag : 0) {}

 public:
  // The constexpr constructor is used by StaticParserAtom and XDR
  constexpr ParserAtom() = default;

  // ParserAtoms may own their content buffers in variant_, and thus
  // cannot be copy-constructed - as a new chars would need to be allocated.
  ParserAtom(const ParserAtom&) = delete;
  ParserAtom(ParserAtom&& other) = delete;

  template <typename CharT, typename SeqCharT>
  static ParserAtom* allocate(JSContext* cx, LifoAlloc& alloc,
                              InflatedChar16Sequence<SeqCharT> seq,
                              uint32_t length, HashNumber hash);

  static ParserAtom* allocateRaw(JSContext* cx, LifoAlloc& alloc,
                                 const uint8_t* srcRaw, size_t totalLength);

  bool hasLatin1Chars() const { return !(flags_ & HasTwoByteCharsFlag); }
  bool hasTwoByteChars() const { return flags_ & HasTwoByteCharsFlag; }

  template <typename CharT>
  const CharT* chars() const {
    MOZ_ASSERT(sizeof(CharT) == (hasTwoByteChars() ? 2 : 1));
    return reinterpret_cast<const CharT*>(this + 1);
  }

  template <typename CharT>
  CharT* chars() {
    MOZ_ASSERT(sizeof(CharT) == (hasTwoByteChars() ? 2 : 1));
    return reinterpret_cast<CharT*>(this + 1);
  }

  const Latin1Char* latin1Chars() const { return chars<Latin1Char>(); }
  const char16_t* twoByteChars() const { return chars<char16_t>(); }
  mozilla::Range<const Latin1Char> latin1Range() const {
    return mozilla::Range(latin1Chars(), length_);
  }
  mozilla::Range<const char16_t> twoByteRange() const {
    return mozilla::Range(twoByteChars(), length_);
  }

 private:
  bool isIndex(uint32_t* indexp) const;
  bool isPrivateName() const;

 public:
  bool isAscii() const {
    if (hasTwoByteChars()) {
      return false;
    }
    for (Latin1Char ch : latin1Range()) {
      if (!mozilla::IsAscii(ch)) {
        return false;
      }
    }
    return true;
  }

  HashNumber hash() const { return hash_; }
  uint32_t length() const { return length_; }

  bool isUsedByStencil() const { return flags_ & UsedByStencilFlag; }
  void markUsedByStencil() const {
    if (!isWellKnownOrStatic()) {
      // Use const method + const_cast here to avoid marking static strings'
      // field mutable.
      const_cast<ParserAtom*>(this)->flags_ |= UsedByStencilFlag;
    }
  }

  bool equalsJSAtom(JSAtom* other) const;

  template <typename CharT>
  bool equalsSeq(HashNumber hash, InflatedChar16Sequence<CharT> seq) const;

 private:
  bool isWellKnownOrStatic() const { return flags_ & WellKnownOrStaticFlag; }

  constexpr void setWellKnownOrStatic() { flags_ |= WellKnownOrStaticFlag; }

  constexpr void setHashAndLength(HashNumber hash, uint32_t length) {
    hash_ = hash;
    length_ = length;
  }

  // Convert this entry to a js-atom.  The first time this method is called
  // the entry will cache the JSAtom pointer to return later.
  JSAtom* toJSAtom(JSContext* cx, TaggedParserAtomIndex index,
                   CompilationAtomCache& atomCache) const;

 public:
  // Convert NotInstantiated and usedByStencil entry to a js-atom.
  JSAtom* instantiate(JSContext* cx, TaggedParserAtomIndex index,
                      CompilationAtomCache& atomCache) const;

 private:
  // Convert this entry to a number.
  bool toNumber(JSContext* cx, double* result) const;

 public:
#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump() const;
  void dumpCharsNoQuote(js::GenericPrinter& out) const;
#endif
};

// A ParserAtom with explicit inline storage. This is compatible with
// constexpr to have builtin atoms. Care must be taken to ensure these atoms are
// unique.
template <size_t Length>
class StaticParserAtom : public ParserAtom {
  alignas(alignof(ParserAtom)) char storage_[Length] = {};

 public:
  constexpr StaticParserAtom() = default;

  constexpr char* storage() {
    static_assert(offsetof(StaticParserAtom, storage_) == sizeof(ParserAtom),
                  "StaticParserAtom storage should follow ParserAtom");
    return storage_;
  }
};

template <>
class StaticParserAtom<0> : public ParserAtom {
 public:
  constexpr StaticParserAtom() = default;
};

/**
 * A lookup structure that allows for querying ParserAtoms in
 * a hashtable using a flexible input type that supports string
 * representations of various forms.
 */
class ParserAtomLookup {
 protected:
  HashNumber hash_;

  ParserAtomLookup(HashNumber hash) : hash_(hash) {}

 public:
  HashNumber hash() const { return hash_; }

  virtual bool equalsEntry(const ParserAtom* entry) const = 0;
};

struct ParserAtomLookupHasher {
  using Lookup = ParserAtomLookup;

  static inline HashNumber hash(const Lookup& l) { return l.hash(); }
  static inline bool match(const ParserAtom* entry, const Lookup& l) {
    return l.equalsEntry(entry);
  }
};

// We use this class to build a read-only constexpr table of ParserAtoms for the
// well-known atoms set. This should be resolved at compile-time (including hash
// computation) thanks to C++ constexpr.
//
// This table contains similar information as JSAtomState+StaticStrings,
// with some differences:
//
//   | frontend                   | VM                                |
//   |----------------------------|-----------------------------------|
//   | emptyAtom                  | JSAtomState.empty                 |
//   | length1Table (*1)          | StaticStrings.unitStaticTable     |
//   | length2Table               | StaticStrings.length2StaticTable  |
//   | -                          | StaticStrings.intStaticTable      |
//   | non-tiny common names (*2) | JSAtomState common property names |
//   | prototype names            | JSAtomState prototype names       |
//   | -                          | JSAtomState symbol names          |
//
// *1: StaticStrings.unitStaticTable uses entire Latin1 range, but
//     WellKnownParserAtoms_ROM.length2Table uses only ASCII range,
//     given non-ASCII chars won't appear frequently inside source code
// *2: tiny common property names are stored in length1/length2 tables
//
class WellKnownParserAtoms_ROM {
  // NOTE: While the well-known strings are all Latin1, we must use char16_t in
  //       some places in order to have constexpr mozilla::HashString.
  using CharTraits = std::char_traits<char>;
  using Char16Traits = std::char_traits<char16_t>;

 public:
  static constexpr size_t ASCII_STATIC_LIMIT = 128U;
  static constexpr size_t NUM_LENGTH2_ENTRIES =
      StaticStrings::NUM_LENGTH2_ENTRIES;

  StaticParserAtom<0> emptyAtom;
  StaticParserAtom<1> length1Table[ASCII_STATIC_LIMIT];
  StaticParserAtom<2> length2Table[NUM_LENGTH2_ENTRIES];

#define PROPERTYNAME_FIELD_(_, NAME, TEXT) \
  StaticParserAtom<CharTraits::length(TEXT)> NAME;
  FOR_EACH_NONTINY_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

#define PROPERTYNAME_FIELD_(NAME, _) \
  StaticParserAtom<CharTraits::length(#NAME)> NAME;
  JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

 public:
  constexpr WellKnownParserAtoms_ROM() {
    // Empty atom
    emptyAtom.setHashAndLength(mozilla::HashString(u""), 0);
    emptyAtom.setWellKnownOrStatic();

    // Length-1 static atoms
    for (size_t i = 0; i < ASCII_STATIC_LIMIT; ++i) {
      init(length1Table[i], i);
    }

    // Length-2 static atoms
    for (size_t i = 0; i < NUM_LENGTH2_ENTRIES; ++i) {
      init(length2Table[i], i);
    }

    // Initialize each well-known property atoms
#define PROPERTYNAME_FIELD_(_, NAME, TEXT) \
  init(NAME, NAME.storage(), u"" TEXT, WellKnownAtomId::NAME);
    FOR_EACH_NONTINY_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

    // Initialize each well-known prototype atoms
#define PROPERTYNAME_FIELD_(NAME, _) \
  init(NAME, NAME.storage(), u"" #NAME, WellKnownAtomId::NAME);
    JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_
  }

 private:
  // Initialization moved out of the constructor to workaround bug 1668238.
  static constexpr void init(StaticParserAtom<1>& entry, size_t i) {
    size_t len = 1;
    char16_t buf[] = {static_cast<char16_t>(i),
                      /* null-terminator */ 0};
    entry.setHashAndLength(mozilla::HashString(buf), len);
    entry.setWellKnownOrStatic();
    entry.storage()[0] = buf[0];
  }

  static constexpr void init(StaticParserAtom<2>& entry, size_t i) {
    size_t len = 2;
    char16_t buf[] = {StaticStrings::firstCharOfLength2(i),
                      StaticStrings::secondCharOfLength2(i),
                      /* null-terminator */ 0};
    entry.setHashAndLength(mozilla::HashString(buf), len);
    entry.setWellKnownOrStatic();
    entry.storage()[0] = buf[0];
    entry.storage()[1] = buf[1];
  }

  static constexpr void init(ParserAtom& entry, char* storage,
                             const char16_t* text, WellKnownAtomId id) {
    size_t len = Char16Traits::length(text);
    entry.setHashAndLength(mozilla::HashString(text), len);
    entry.setWellKnownOrStatic();
    for (size_t i = 0; i < len; ++i) {
      storage[i] = text[i];
    }
  }

 public:
  // Fast-path tiny strings since they are abundant in minified code.
  template <typename CharsT>
  const ParserAtom* lookupTiny(CharsT chars, size_t length) const {
    static_assert(std::is_same_v<CharsT, const Latin1Char*> ||
                      std::is_same_v<CharsT, const char16_t*> ||
                      std::is_same_v<CharsT, const char*> ||
                      std::is_same_v<CharsT, char16_t*> ||
                      std::is_same_v<CharsT, LittleEndianChars>,
                  "This assert mostly explicitly documents the calling types, "
                  "and forces that to be updated if new types show up.");
    switch (length) {
      case 0:
        return &emptyAtom;

      case 1: {
        if (char16_t(chars[0]) < ASCII_STATIC_LIMIT) {
          size_t index = static_cast<size_t>(chars[0]);
          return &length1Table[index];
        }
        break;
      }

      case 2:
        if (StaticStrings::fitsInSmallChar(chars[0]) &&
            StaticStrings::fitsInSmallChar(chars[1])) {
          size_t index = StaticStrings::getLength2Index(chars[0], chars[1]);
          return &length2Table[index];
        }
        break;
    }

    // No match on tiny Atoms
    return nullptr;
  }

  template <typename CharsT>
  TaggedParserAtomIndex lookupTinyIndex(CharsT chars, size_t length) const {
    static_assert(std::is_same_v<CharsT, const Latin1Char*> ||
                      std::is_same_v<CharsT, const char16_t*> ||
                      std::is_same_v<CharsT, const char*> ||
                      std::is_same_v<CharsT, char16_t*> ||
                      std::is_same_v<CharsT, LittleEndianChars>,
                  "This assert mostly explicitly documents the calling types, "
                  "and forces that to be updated if new types show up.");
    switch (length) {
      case 0:
        return TaggedParserAtomIndex::WellKnown::empty();

      case 1: {
        if (char16_t(chars[0]) < ASCII_STATIC_LIMIT) {
          return TaggedParserAtomIndex(StaticParserString1(chars[0]));
        }
        break;
      }

      case 2:
        if (StaticStrings::fitsInSmallChar(chars[0]) &&
            StaticStrings::fitsInSmallChar(chars[1])) {
          return TaggedParserAtomIndex(StaticParserString2(
              StaticStrings::getLength2Index(chars[0], chars[1])));
        }
        break;
    }

    // No match on tiny Atoms
    return TaggedParserAtomIndex::null();
  }
};

using ParserAtomVector = Vector<ParserAtom*, 0, js::SystemAllocPolicy>;
using ParserAtomSpan = mozilla::Span<ParserAtom*>;

/**
 * WellKnownParserAtoms reserves a set of common ParserAtoms on the JSRuntime
 * in a read-only format to be used by parser. These reserved atoms can be
 * translated to equivalent JSAtoms in constant time.
 *
 * The common-names set allows the parser to lookup up specific atoms in
 * constant time.
 *
 * We also reserve tiny (length 1/2) parser-atoms for fast lookup similar to
 * the js::StaticStrings mechanism. This speeds up parsing minified code.
 */
class WellKnownParserAtoms {
 public:
  // Named fields allow quickly finding an atom if it is known at compile time.
  // This is particularly useful for the Parser.
#define PROPERTYNAME_FIELD_(_, NAME, _2) const ParserAtom* NAME{};
  FOR_EACH_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

#define PROPERTYNAME_FIELD_(NAME, _) const ParserAtom* NAME{};
  JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

  // The ParserAtom of all well-known and tiny ParserAtoms are generated at
  // compile-time into a ROM that is computed using constexpr. This results in
  // the data being in the .rodata section of binary and easily shared by
  // multiple JS processes.
  static constexpr WellKnownParserAtoms_ROM rom_ = {};

  // Common property and prototype names are tracked in a hash table. This table
  // does not key for any items already in a direct-indexing tiny atom table.
  using EntryMap = HashMap<const ParserAtom*, TaggedParserAtomIndex,
                           ParserAtomLookupHasher, js::SystemAllocPolicy>;
  EntryMap wellKnownMap_;

  bool initTinyStringAlias(JSContext* cx, const ParserAtom** name,
                           const char* str);
  bool initSingle(JSContext* cx, const ParserAtom** name,
                  const ParserAtom& romEntry, TaggedParserAtomIndex index);

 public:
  bool init(JSContext* cx);

  // Maximum length of any well known atoms. This can be increased if needed.
  static constexpr size_t MaxWellKnownLength = 32;

  template <typename CharT>
  TaggedParserAtomIndex lookupChar16Seq(
      const SpecificParserAtomLookup<CharT>& lookup) const;

  template <typename CharsT>
  const ParserAtom* lookupTiny(CharsT chars, size_t length) const {
    return rom_.lookupTiny(chars, length);
  }

  template <typename CharsT>
  TaggedParserAtomIndex lookupTinyIndex(CharsT chars, size_t length) const {
    return rom_.lookupTinyIndex(chars, length);
  }

  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  static const ParserAtom* getStatic1(StaticParserString1 s);
  static const ParserAtom* getStatic2(StaticParserString2 s);
};

bool InstantiateMarkedAtoms(JSContext* cx, const ParserAtomSpan& entries,
                            CompilationAtomCache& atomCache);

/**
 * A ParserAtomsTable owns and manages the vector of ParserAtom entries
 * associated with a given compile session.
 */
class ParserAtomsTable {
 private:
  const WellKnownParserAtoms& wellKnownTable_;

  LifoAlloc& alloc_;

  // The ParserAtom are owned by the LifoAlloc.
  using EntryMap = HashMap<const ParserAtom*, TaggedParserAtomIndex,
                           ParserAtomLookupHasher, js::SystemAllocPolicy>;
  EntryMap entryMap_;
  ParserAtomVector entries_;

 public:
  ParserAtomsTable(JSRuntime* rt, LifoAlloc& alloc);
  ParserAtomsTable(ParserAtomsTable&&) = default;

 private:
  // Internal APIs for interning to the table after well-known atoms cases have
  // been tested.
  TaggedParserAtomIndex addEntry(JSContext* cx, EntryMap::AddPtr& addPtr,
                                 ParserAtom* entry);
  template <typename AtomCharT, typename SeqCharT>
  TaggedParserAtomIndex internChar16Seq(JSContext* cx, EntryMap::AddPtr& addPtr,
                                        HashNumber hash,
                                        InflatedChar16Sequence<SeqCharT> seq,
                                        uint32_t length);

 public:
  TaggedParserAtomIndex internAscii(JSContext* cx, const char* asciiPtr,
                                    uint32_t length);

  TaggedParserAtomIndex internLatin1(JSContext* cx,
                                     const JS::Latin1Char* latin1Ptr,
                                     uint32_t length);

  TaggedParserAtomIndex internUtf8(JSContext* cx,
                                   const mozilla::Utf8Unit* utf8Ptr,
                                   uint32_t nbyte);

  TaggedParserAtomIndex internChar16(JSContext* cx, const char16_t* char16Ptr,
                                     uint32_t length);

  TaggedParserAtomIndex internJSAtom(JSContext* cx,
                                     CompilationAtomCache& atomCache,
                                     JSAtom* atom);

  TaggedParserAtomIndex concatAtoms(JSContext* cx,
                                    mozilla::Range<const ParserAtom*> atoms);

 private:
  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  const ParserAtom* getStatic1(StaticParserString1 s) const;
  const ParserAtom* getStatic2(StaticParserString2 s) const;

  template <class T>
  friend const ParserAtom* GetParserAtom(T self, TaggedParserAtomIndex index);

 public:
  const ParserAtom* getParserAtom(ParserAtomIndex index) const;
  const ParserAtom* getParserAtom(TaggedParserAtomIndex index) const;

  void markUsedByStencil(TaggedParserAtomIndex index) const;

  const ParserAtomVector& entries() const { return entries_; }

  // Accessors for querying atom properties.
  bool isPrivateName(TaggedParserAtomIndex index) const;
  bool isIndex(TaggedParserAtomIndex index, uint32_t* indexp) const;

  // Methods for atom.
  bool toNumber(JSContext* cx, TaggedParserAtomIndex index,
                double* result) const;
  UniqueChars toNewUTF8CharsZ(JSContext* cx, TaggedParserAtomIndex index) const;
  UniqueChars toPrintableString(JSContext* cx,
                                TaggedParserAtomIndex index) const;
  UniqueChars toQuotedString(JSContext* cx, TaggedParserAtomIndex index) const;
  JSAtom* toJSAtom(JSContext* cx, TaggedParserAtomIndex index,
                   CompilationAtomCache& atomCache) const;

 public:
#if defined(DEBUG) || defined(JS_JITSPEW)
  void dumpCharsNoQuote(js::GenericPrinter& out,
                        TaggedParserAtomIndex index) const;
#endif
};

// Lightweight version of ParserAtomsTable.
// This doesn't support deduplication.
// Used while decoding XDR.
class ParserAtomSpanBuilder {
 private:
  const WellKnownParserAtoms& wellKnownTable_;
  ParserAtomSpan& entries_;

 public:
  ParserAtomSpanBuilder(JSRuntime* rt, ParserAtomSpan& entries);

  bool allocate(JSContext* cx, LifoAlloc& alloc, size_t count);
  size_t size() const { return entries_.size(); }

  void set(ParserAtomIndex index, const ParserAtom* atom) {
    entries_[index] = const_cast<ParserAtom*>(atom);
  }

 private:
  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  const ParserAtom* getStatic1(StaticParserString1 s) const;
  const ParserAtom* getStatic2(StaticParserString2 s) const;

  template <class T>
  friend const ParserAtom* GetParserAtom(T self, TaggedParserAtomIndex index);

 public:
  const ParserAtom* getParserAtom(ParserAtomIndex index) const;
  const ParserAtom* getParserAtom(TaggedParserAtomIndex index) const;
};

template <typename CharT>
class SpecificParserAtomLookup : public ParserAtomLookup {
  // The sequence of characters to look up.
  InflatedChar16Sequence<CharT> seq_;

 public:
  explicit SpecificParserAtomLookup(const InflatedChar16Sequence<CharT>& seq)
      : SpecificParserAtomLookup(seq, seq.computeHash()) {}

  SpecificParserAtomLookup(const InflatedChar16Sequence<CharT>& seq,
                           HashNumber hash)
      : ParserAtomLookup(hash), seq_(seq) {
    MOZ_ASSERT(seq_.computeHash() == hash);
  }

  virtual bool equalsEntry(const ParserAtom* entry) const override {
    return entry->equalsSeq<CharT>(hash_, seq_);
  }
};

template <typename CharT>
inline bool ParserAtom::equalsSeq(HashNumber hash,
                                  InflatedChar16Sequence<CharT> seq) const {
  // Compare hashes first.
  if (hash_ != hash) {
    return false;
  }

  if (hasTwoByteChars()) {
    const char16_t* chars = twoByteChars();
    for (uint32_t i = 0; i < length_; i++) {
      if (!seq.hasMore() || chars[i] != seq.next()) {
        return false;
      }
    }
  } else {
    const Latin1Char* chars = latin1Chars();
    for (uint32_t i = 0; i < length_; i++) {
      if (!seq.hasMore() || char16_t(chars[i]) != seq.next()) {
        return false;
      }
    }
  }
  return !seq.hasMore();
}

JSAtom* GetWellKnownAtom(JSContext* cx, WellKnownAtomId atomId);

} /* namespace frontend */
} /* namespace js */

#endif  // frontend_ParserAtom_h
