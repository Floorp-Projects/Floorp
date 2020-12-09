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

#include "ds/LifoAlloc.h"         // LifoAlloc
#include "frontend/TypedIndex.h"  // TypedIndex
#include "js/HashTable.h"         // HashSet
#include "js/UniquePtr.h"         // js::UniquePtr
#include "js/Vector.h"            // Vector
#include "vm/CommonPropertyNames.h"
#include "vm/StringType.h"  // CompareChars, StringEqualsAscii

namespace js {
namespace frontend {

struct CompilationAtomCache;
struct CompilationInfo;
class ParserAtom;
class ParserName;

template <typename CharT>
class SpecificParserAtomLookup;

class ParserAtomsTable;

// An index to map WellKnownParserAtoms to cx->names().
// This is consistent across multiple compilation.
//
// GetWellKnownAtom in ParserAtom.cpp relies on the fact that
// JSAtomState fields and this enum variants use the same order.
enum class WellKnownAtomId : uint32_t {
#define ENUM_ENTRY_(_, name, _2) name,
  FOR_EACH_COMMON_PROPERTYNAME(ENUM_ENTRY_)
#undef ENUM_ENTRY_

#define ENUM_ENTRY_(name, _) name,
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
#define CHECK_(_, name, _2) MOZ_ASSERT(index != WellKnownAtomId::name);
    FOR_EACH_NON_EMPTY_TINY_PROPERTYNAME(CHECK_)
#undef CHECK_
  }
  explicit constexpr TaggedParserAtomIndex(StaticParserString1 index)
      : data_(uint32_t(index) | WellKnownTag | Static1SubTag) {}
  explicit constexpr TaggedParserAtomIndex(StaticParserString2 index)
      : data_(uint32_t(index) | WellKnownTag | Static2SubTag) {}

  static TaggedParserAtomIndex star() {
    return TaggedParserAtomIndex(StaticParserString1('*'));
  }
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

  uint32_t* rawData() { return &data_; }

  bool operator==(const TaggedParserAtomIndex& rhs) const {
    return data_ == rhs.data_;
  }

  explicit operator bool() const { return !isNull(); }
};

/**
 * A ParserAtomEntry is an in-parser representation of an interned atomic
 * string.  It mostly mirrors the information carried by a JSAtom*.
 *
 * The atom contents are stored in one of two locations:
 *  1. Inline Latin1Char storage (immediately after the ParserAtomEntry memory).
 *  2. Inline char16_t storage (immediately after the ParserAtomEntry memory).
 */
class alignas(alignof(uint32_t)) ParserAtomEntry {
  friend class ParserAtomsTable;
  friend class WellKnownParserAtoms;
  friend class WellKnownParserAtoms_ROM;

  static const uint16_t MAX_LATIN1_CHAR = 0xff;

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

  TaggedParserAtomIndex index_;

  // Encoding type.
  bool hasTwoByteChars_ = false;

  // Mutable flags.
  bool usedByStencil_ = false;

  // End of fields.

  static const uint32_t MAX_LENGTH = JSString::MAX_LENGTH;

  ParserAtomEntry(uint32_t length, HashNumber hash, bool hasTwoByteChars)
      : hash_(hash), length_(length), hasTwoByteChars_(hasTwoByteChars) {}

 protected:
  // The constexpr constructor is used by StaticParserAtomEntry.
  constexpr ParserAtomEntry() = default;

 public:
  // ParserAtomEntries may own their content buffers in variant_, and thus
  // cannot be copy-constructed - as a new chars would need to be allocated.
  ParserAtomEntry(const ParserAtomEntry&) = delete;
  ParserAtomEntry(ParserAtomEntry&& other) = delete;

  template <typename CharT, typename SeqCharT>
  static ParserAtomEntry* allocate(JSContext* cx, LifoAlloc& alloc,
                                   InflatedChar16Sequence<SeqCharT> seq,
                                   uint32_t length, HashNumber hash);

  ParserAtom* asAtom() { return reinterpret_cast<ParserAtom*>(this); }
  const ParserAtom* asAtom() const {
    return reinterpret_cast<const ParserAtom*>(this);
  }

  inline ParserName* asName();
  inline const ParserName* asName() const;

  bool hasLatin1Chars() const { return !hasTwoByteChars_; }
  bool hasTwoByteChars() const { return hasTwoByteChars_; }

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

  bool isIndex(uint32_t* indexp) const;
  bool isIndex() const {
    uint32_t index;
    return isIndex(&index);
  }

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

  bool isUsedByStencil() const { return usedByStencil_; }
  void markUsedByStencil() const {
    if (isParserAtomIndex()) {
      // Use const method + const_cast here to avoid marking static strings'
      // field mutable.
      const_cast<ParserAtomEntry*>(this)->usedByStencil_ = true;
    }
  }

  bool equalsJSAtom(JSAtom* other) const;

  template <typename CharT>
  bool equalsSeq(HashNumber hash, InflatedChar16Sequence<CharT> seq) const;

  TaggedParserAtomIndex toIndex() const { return index_; }

  ParserAtomIndex toParserAtomIndex() const {
    return index_.toParserAtomIndex();
  }
  WellKnownAtomId toWellKnownAtomId() const {
    return index_.toWellKnownAtomId();
  }
  StaticParserString1 toStaticParserString1() const {
    return index_.toStaticParserString1();
  }
  StaticParserString2 toStaticParserString2() const {
    return index_.toStaticParserString2();
  }

  bool isParserAtomIndex() const { return index_.isParserAtomIndex(); }
  bool isWellKnownAtomId() const { return index_.isWellKnownAtomId(); }
  bool isStaticParserString1() const { return index_.isStaticParserString1(); }
  bool isStaticParserString2() const { return index_.isStaticParserString2(); }

  void setParserAtomIndex(ParserAtomIndex index) {
    index_ = TaggedParserAtomIndex(index);
  }

 private:
  constexpr void setWellKnownAtomId(WellKnownAtomId atomId) {
    index_ = TaggedParserAtomIndex(atomId);
  }
  constexpr void setStaticParserString1(StaticParserString1 s) {
    index_ = TaggedParserAtomIndex(s);
  }
  constexpr void setStaticParserString2(StaticParserString2 s) {
    index_ = TaggedParserAtomIndex(s);
  }
  constexpr void setHashAndLength(HashNumber hash, uint32_t length,
                                  bool hasTwoByteChars = false) {
    hash_ = hash;
    length_ = length;
    hasTwoByteChars_ = hasTwoByteChars;
  }

 public:
  // Convert this entry to a js-atom.  The first time this method is called
  // the entry will cache the JSAtom pointer to return later.
  JSAtom* toJSAtom(JSContext* cx, CompilationAtomCache& atomCache) const;

  // Same as toJSAtom, but this is guaranteed to be instantiated.
  JSAtom* toExistingJSAtom(JSContext* cx,
                           CompilationAtomCache& atomCache) const;

  // Convert NotInstantiated and usedByStencil entry to a js-atom.
  JSAtom* instantiate(JSContext* cx, CompilationAtomCache& atomCache) const;

  // Convert this entry to a number.
  bool toNumber(JSContext* cx, double* result) const;

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump() const;
  void dumpCharsNoQuote(js::GenericPrinter& out) const;
#endif
};

class ParserAtom : public ParserAtomEntry {
  ParserAtom() = delete;
  ParserAtom(const ParserAtom&) = delete;
};

class ParserName : public ParserAtom {
  ParserName() = delete;
  ParserName(const ParserName&) = delete;
};

UniqueChars ParserAtomToPrintableString(JSContext* cx, const ParserAtom* atom);

inline ParserName* ParserAtomEntry::asName() {
  MOZ_ASSERT(!isIndex());
  return static_cast<ParserName*>(this);
}
inline const ParserName* ParserAtomEntry::asName() const {
  MOZ_ASSERT(!isIndex());
  return static_cast<const ParserName*>(this);
}

// A ParserAtomEntry with explicit inline storage. This is compatible with
// constexpr to have builtin atoms. Care must be taken to ensure these atoms are
// unique.
template <size_t Length>
class StaticParserAtomEntry : public ParserAtomEntry {
  alignas(alignof(ParserAtomEntry)) char storage_[Length] = {};

 public:
  constexpr StaticParserAtomEntry() = default;

  constexpr char* storage() {
    static_assert(
        offsetof(StaticParserAtomEntry, storage_) == sizeof(ParserAtomEntry),
        "StaticParserAtomEntry storage should follow ParserAtomEntry");
    return storage_;
  }
};

template <>
class StaticParserAtomEntry<0> : public ParserAtomEntry {
 public:
  constexpr StaticParserAtomEntry() = default;
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

  virtual bool equalsEntry(const ParserAtomEntry* entry) const = 0;
};

struct ParserAtomLookupHasher {
  using Lookup = ParserAtomLookup;

  static inline HashNumber hash(const Lookup& l) { return l.hash(); }
  static inline bool match(const ParserAtomEntry* entry, const Lookup& l) {
    return l.equalsEntry(entry);
  }
};

// We use this class to build a read-only constexpr table of ParserAtoms for the
// well-known atoms set. This should be resolved at compile-time (including hash
// computation) thanks to C++ constexpr.
class WellKnownParserAtoms_ROM {
  // NOTE: While the well-known strings are all Latin1, we must use char16_t in
  //       some places in order to have constexpr mozilla::HashString.
  using CharTraits = std::char_traits<char>;
  using Char16Traits = std::char_traits<char16_t>;

 public:
  static const size_t ASCII_STATIC_LIMIT = 128U;
  static const size_t NUM_SMALL_CHARS = StaticStrings::NUM_SMALL_CHARS;
  static const size_t NUM_LENGTH2_ENTRIES = NUM_SMALL_CHARS * NUM_SMALL_CHARS;

  StaticParserAtomEntry<0> emptyAtom;
  StaticParserAtomEntry<1> length1Table[ASCII_STATIC_LIMIT];
  StaticParserAtomEntry<2> length2Table[NUM_LENGTH2_ENTRIES];

#define PROPERTYNAME_FIELD_(_, name, text) \
  StaticParserAtomEntry<CharTraits::length(text)> name;
  FOR_EACH_NONTINY_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

#define PROPERTYNAME_FIELD_(name, _) \
  StaticParserAtomEntry<CharTraits::length(#name)> name;
  JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

 public:
  constexpr WellKnownParserAtoms_ROM() {
    // Empty atom
    emptyAtom.setHashAndLength(mozilla::HashString(u""), 0);
    emptyAtom.setWellKnownAtomId(WellKnownAtomId::empty);

    // Length-1 static atoms
    for (size_t i = 0; i < ASCII_STATIC_LIMIT; ++i) {
      init(length1Table[i], i);
    }

    // Length-2 static atoms
    for (size_t i = 0; i < NUM_LENGTH2_ENTRIES; ++i) {
      init(length2Table[i], i);
    }

    // Initialize each well-known property atoms
#define PROPERTYNAME_FIELD_(_, name, text) \
  init(name, name.storage(), u"" text, WellKnownAtomId::name);
    FOR_EACH_NONTINY_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

    // Initialize each well-known prototype atoms
#define PROPERTYNAME_FIELD_(name, _) \
  init(name, name.storage(), u"" #name, WellKnownAtomId::name);
    JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_
  }

 private:
  // Initialization moved out of the constructor to workaround bug 1668238.
  static constexpr void init(StaticParserAtomEntry<1>& entry, size_t i) {
    size_t len = 1;
    char16_t buf[] = {static_cast<char16_t>(i),
                      /* null-terminator */ 0};
    entry.setHashAndLength(mozilla::HashString(buf), len);
    entry.setStaticParserString1(StaticParserString1(i));
    entry.storage()[0] = buf[0];
  }

  static constexpr void init(StaticParserAtomEntry<2>& entry, size_t i) {
    size_t len = 2;
    char16_t buf[] = {StaticStrings::fromSmallChar(i >> 6),
                      StaticStrings::fromSmallChar(i & 0x003F),
                      /* null-terminator */ 0};
    entry.setHashAndLength(mozilla::HashString(buf), len);
    entry.setStaticParserString2(StaticParserString2(i));
    entry.storage()[0] = buf[0];
    entry.storage()[1] = buf[1];
  }

  static constexpr void init(ParserAtomEntry& entry, char* storage,
                             const char16_t* text, WellKnownAtomId id) {
    size_t len = Char16Traits::length(text);
    entry.setHashAndLength(mozilla::HashString(text), len);
    entry.setWellKnownAtomId(id);
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
        return emptyAtom.asAtom();

      case 1: {
        if (char16_t(chars[0]) < ASCII_STATIC_LIMIT) {
          size_t index = static_cast<size_t>(chars[0]);
          return length1Table[index].asAtom();
        }
        break;
      }

      case 2:
        if (StaticStrings::fitsInSmallChar(chars[0]) &&
            StaticStrings::fitsInSmallChar(chars[1])) {
          size_t index = StaticStrings::getLength2Index(chars[0], chars[1]);
          return length2Table[index].asAtom();
        }
        break;
    }

    // No match on tiny Atoms
    return nullptr;
  }
};

using ParserAtomVector = Vector<ParserAtomEntry*, 0, js::SystemAllocPolicy>;

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
#define PROPERTYNAME_FIELD_(_, name, _2) const ParserName* name{};
  FOR_EACH_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

#define PROPERTYNAME_FIELD_(name, _) const ParserName* name{};
  JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

  // The ParserAtomEntry of all well-known and tiny ParserAtoms are generated at
  // compile-time into a ROM that is computed using constexpr. This results in
  // the data being in the .rodata section of binary and easily shared by
  // multiple JS processes.
  static constexpr WellKnownParserAtoms_ROM rom_ = {};

  // Common property and prototype names are tracked in a hash table. This table
  // does not key for any items already in a direct-indexing tiny atom table.
  using EntrySet = HashSet<const ParserAtomEntry*, ParserAtomLookupHasher,
                           js::SystemAllocPolicy>;
  EntrySet wellKnownSet_;

  bool initTinyStringAlias(JSContext* cx, const ParserName** name,
                           const char* str);
  bool initSingle(JSContext* cx, const ParserName** name,
                  const ParserAtomEntry& romEntry);

 public:
  bool init(JSContext* cx);

  // Maximum length of any well known atoms. This can be increased if needed.
  static constexpr size_t MaxWellKnownLength = 32;

  template <typename CharT>
  const ParserAtom* lookupChar16Seq(
      const SpecificParserAtomLookup<CharT>& lookup) const;

  template <typename CharsT>
  const ParserAtom* lookupTiny(CharsT chars, size_t length) const {
    return rom_.lookupTiny(chars, length);
  }

  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  static const ParserAtom* getStatic1(StaticParserString1 s);
  static const ParserAtom* getStatic2(StaticParserString2 s);
};

bool InstantiateMarkedAtoms(JSContext* cx, const ParserAtomVector& entries,
                            CompilationAtomCache& atomCache);

/**
 * A ParserAtomsTable owns and manages the vector of ParserAtom entries
 * associated with a given compile session.
 */
class ParserAtomsTable {
 private:
  const WellKnownParserAtoms& wellKnownTable_;

  LifoAlloc& alloc_;

  // The ParserAtomEntry are owned by the LifoAlloc.
  using EntrySet = HashSet<const ParserAtomEntry*, ParserAtomLookupHasher,
                           js::SystemAllocPolicy>;
  EntrySet entrySet_;
  ParserAtomVector& entries_;

 public:
  ParserAtomsTable(JSRuntime* rt, LifoAlloc& alloc, ParserAtomVector& entries);
  ParserAtomsTable(ParserAtomsTable&&) = default;

 private:
  // Internal APIs for interning to the table after well-known atoms cases have
  // been tested.
  const ParserAtom* addEntry(JSContext* cx, EntrySet::AddPtr& addPtr,
                             ParserAtomEntry* entry);
  template <typename AtomCharT, typename SeqCharT>
  const ParserAtom* internChar16Seq(JSContext* cx, EntrySet::AddPtr& addPtr,
                                    HashNumber hash,
                                    InflatedChar16Sequence<SeqCharT> seq,
                                    uint32_t length);

 public:
  const ParserAtom* internAscii(JSContext* cx, const char* asciiPtr,
                                uint32_t length);

  const ParserAtom* internLatin1(JSContext* cx, const JS::Latin1Char* latin1Ptr,
                                 uint32_t length);

  const ParserAtom* internUtf8(JSContext* cx, const mozilla::Utf8Unit* utf8Ptr,
                               uint32_t nbyte);

  const ParserAtom* internChar16(JSContext* cx, const char16_t* char16Ptr,
                                 uint32_t length);

  const ParserAtom* internJSAtom(JSContext* cx,
                                 CompilationInfo& compilationInfo,
                                 JSAtom* atom);

  const ParserAtom* concatAtoms(JSContext* cx,
                                mozilla::Range<const ParserAtom*> atoms);

  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  const ParserAtom* getStatic1(StaticParserString1 s) const;
  const ParserAtom* getStatic2(StaticParserString2 s) const;
  const ParserAtom* getParserAtom(ParserAtomIndex index) const;
  const ParserAtom* getParserAtom(TaggedParserAtomIndex index) const;
};

// Lightweight version of ParserAtomsTable.
// This doesn't support deduplication.
// Used while decoding XDR.
class ParserAtomVectorBuilder {
 private:
  const WellKnownParserAtoms& wellKnownTable_;
  LifoAlloc* alloc_;
  ParserAtomVector& entries_;

 public:
  ParserAtomVectorBuilder(JSRuntime* rt, LifoAlloc& alloc,
                          ParserAtomVector& entries);

  bool resize(JSContext* cx, size_t count);
  size_t length() const { return entries_.length(); }

  const ParserAtom* internLatin1At(JSContext* cx,
                                   const JS::Latin1Char* latin1Ptr,
                                   HashNumber hash, uint32_t length,
                                   ParserAtomIndex index);

  const ParserAtom* internChar16At(JSContext* cx,
                                   const LittleEndianChars twoByteLE,
                                   HashNumber hash, uint32_t length,
                                   ParserAtomIndex index);

 private:
  template <typename CharT, typename SeqCharT, typename InputCharsT>
  const ParserAtom* internAt(JSContext* cx, InputCharsT chars, HashNumber hash,
                             uint32_t length, ParserAtomIndex index);

 public:
  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  const ParserAtom* getStatic1(StaticParserString1 s) const;
  const ParserAtom* getStatic2(StaticParserString2 s) const;
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

  virtual bool equalsEntry(const ParserAtomEntry* entry) const override {
    return entry->equalsSeq<CharT>(hash_, seq_);
  }
};

template <typename CharT>
inline bool ParserAtomEntry::equalsSeq(
    HashNumber hash, InflatedChar16Sequence<CharT> seq) const {
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
