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
#include "mozilla/Variant.h"        // mozilla::Variant

#include "ds/LifoAlloc.h"         // LifoAlloc
#include "frontend/TypedIndex.h"  // TypedIndex
#include "js/HashTable.h"         // HashSet
#include "js/UniquePtr.h"         // js::UniquePtr
#include "js/Vector.h"            // Vector
#include "vm/CommonPropertyNames.h"
#include "vm/StringType.h"  // CompareChars, StringEqualsAscii

namespace js {
namespace frontend {

struct CompilationInfo;
class ParserAtom;
class ParserName;

template <typename CharT>
class SpecificParserAtomLookup;

class ParserAtomsTable;

mozilla::GenericErrorResult<OOM> RaiseParserAtomsOOMError(JSContext* cx);

// An index into CompilationInfo.atoms.
// This is local to the current compilation.
using AtomIndex = TypedIndex<JSAtom*>;

// An index to map WellKnownParserAtoms to cx->names().
// This is consistent across multiple compilation.
//
// GetWellKnownAtom in ParserAtom.cpp relies on the fact that
// JSAtomState fields and this enum variants use the same order.
enum class WellKnownAtomId : uint32_t {
#define ENUM_ENTRY_(idpart, id, text) id,
  FOR_EACH_COMMON_PROPERTYNAME(ENUM_ENTRY_)
#undef ENUM_ENTRY_

#define ENUM_ENTRY_(name, clasp) name,
      JS_FOR_EACH_PROTOTYPE(ENUM_ENTRY_)
#undef ENUM_ENTRY_
};

// These types correspond into indices in the StaticStrings arrays.
enum class StaticParserString1 : uint8_t;
enum class StaticParserString2 : uint16_t;

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

  // Mapping into from ParserAtoms to JSAtoms.
  enum class AtomIndexKind : uint8_t {
    Unresolved,  // Not yet resolved
    AtomIndex,   // Index into CompilationInfo atoms map
    WellKnown,   // WellKnownAtomId to index into cx->names() set
    Static1,     // Index into StaticStrings length-1 set
    Static2,     // Index into StaticStrings length-2 set
  };
  uint32_t atomIndex_ = 0;
  AtomIndexKind atomIndexKind_ = AtomIndexKind::Unresolved;

  // Encoding type.
  bool hasTwoByteChars_ = false;

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
  static JS::Result<UniquePtr<ParserAtomEntry>, OOM> allocate(
      JSContext* cx, InflatedChar16Sequence<SeqCharT> seq, uint32_t length,
      HashNumber hash);

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

  HashNumber hash() const { return hash_; }
  uint32_t length() const { return length_; }

  bool equalsJSAtom(JSAtom* other) const;

  template <typename CharT>
  bool equalsSeq(HashNumber hash, InflatedChar16Sequence<CharT> seq) const;

 private:
  void setAtomIndex(AtomIndex index) {
    atomIndex_ = index;
    atomIndexKind_ = AtomIndexKind::AtomIndex;
  }
  constexpr void setWellKnownAtomId(WellKnownAtomId kind) {
    atomIndex_ = static_cast<uint32_t>(kind);
    atomIndexKind_ = AtomIndexKind::WellKnown;
  }
  constexpr void setStaticParserString1(StaticParserString1 s) {
    atomIndex_ = static_cast<uint32_t>(s);
    atomIndexKind_ = AtomIndexKind::Static1;
  }
  constexpr void setStaticParserString2(StaticParserString2 s) {
    atomIndex_ = static_cast<uint32_t>(s);
    atomIndexKind_ = AtomIndexKind::Static2;
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
  JS::Result<JSAtom*, OOM> toJSAtom(JSContext* cx,
                                    CompilationInfo& compilationInfo) const;

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
  static inline bool match(const UniquePtr<ParserAtomEntry>& entry,
                           const Lookup& l) {
    return l.equalsEntry(entry.get());
  }
};

class WellKnownParserAtoms_ROM {
 public:
  static const size_t ASCII_STATIC_LIMIT = 128U;
  static const size_t NUM_SMALL_CHARS = StaticStrings::NUM_SMALL_CHARS;
  static const size_t NUM_LENGTH2_ENTRIES = NUM_SMALL_CHARS * NUM_SMALL_CHARS;

  StaticParserAtomEntry<0> emptyAtom;
  StaticParserAtomEntry<1> length1Table[ASCII_STATIC_LIMIT];
  StaticParserAtomEntry<2> length2Table[NUM_LENGTH2_ENTRIES];

  constexpr WellKnownParserAtoms_ROM() {
    // Empty atom
    emptyAtom.setHashAndLength(mozilla::HashString(u""), 0);
    emptyAtom.setWellKnownAtomId(WellKnownAtomId::empty);

    // Length-1 static atoms
    for (size_t i = 0; i < ASCII_STATIC_LIMIT; ++i) {
      constexpr size_t len = 1;
      char16_t buf[] = {static_cast<char16_t>(i),
                        /* null-terminator */ 0};
      length1Table[i].setHashAndLength(mozilla::HashString(buf), len);
      length1Table[i].setStaticParserString1(StaticParserString1(i));
      length1Table[i].storage()[0] = buf[0];
    }

    // Length-2 static atoms
    for (size_t i = 0; i < NUM_LENGTH2_ENTRIES; ++i) {
      constexpr size_t len = 2;
      char16_t buf[] = {StaticStrings::fromSmallChar(i >> 6),
                        StaticStrings::fromSmallChar(i & 0x003F),
                        /* null-terminator */ 0};
      length2Table[i].setHashAndLength(mozilla::HashString(buf), len);
      length2Table[i].setStaticParserString2(StaticParserString2(i));
      length2Table[i].storage()[0] = buf[0];
      length2Table[i].storage()[1] = buf[1];
    }
  }

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
  /* Various built-in or commonly-used names. */
#define PROPERTYNAME_FIELD_(idpart, id, text) const ParserName* id{};
  FOR_EACH_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

#define PROPERTYNAME_FIELD_(name, clasp) const ParserName* name{};
  JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD_)
#undef PROPERTYNAME_FIELD_

  // Common tiny strings (such as identifiers in minified code) have ParserAtoms
  // generated into constexpr tables.
  static constexpr WellKnownParserAtoms_ROM rom_ = {};

  // Common property and prototype names are tracked in a hash table. This table
  // is does not key for any items already in a direct-indexing table above.
  using EntrySet = HashSet<UniquePtr<ParserAtomEntry>, ParserAtomLookupHasher,
                           js::SystemAllocPolicy>;
  EntrySet wellKnownSet_;

  bool initTinyStringAlias(JSContext* cx, const ParserName** name,
                           const char* str);
  bool initSingle(JSContext* cx, const ParserName** name, const char* str,
                  WellKnownAtomId kind);

 public:
  WellKnownParserAtoms() = default;

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
};

/**
 * A ParserAtomsTable owns and manages the vector of ParserAtom entries
 * associated with a given compile session.
 */
class ParserAtomsTable {
 private:
  using EntrySet = HashSet<UniquePtr<ParserAtomEntry>, ParserAtomLookupHasher,
                           js::SystemAllocPolicy>;
  EntrySet entrySet_;
  const WellKnownParserAtoms& wellKnownTable_;

 public:
  explicit ParserAtomsTable(JSRuntime* rt);

 private:
  // Internal APIs for interning to the table after well-known atoms cases have
  // been tested.
  JS::Result<const ParserAtom*, OOM> addEntry(JSContext* cx,
                                              EntrySet::AddPtr& addPtr,
                                              UniquePtr<ParserAtomEntry> entry);
  JS::Result<const ParserAtom*, OOM> internLatin1Seq(
      JSContext* cx, EntrySet::AddPtr& addPtr, HashNumber hash,
      const Latin1Char* latin1Ptr, uint32_t length);
  template <typename AtomCharT, typename SeqCharT>
  JS::Result<const ParserAtom*, OOM> internChar16Seq(
      JSContext* cx, EntrySet::AddPtr& addPtr, HashNumber hash,
      InflatedChar16Sequence<SeqCharT> seq, uint32_t length);

 public:
  bool empty() const { return entrySet_.empty(); }

  JS::Result<const ParserAtom*, OOM> internAscii(JSContext* cx,
                                                 const char* asciiPtr,
                                                 uint32_t length);

  JS::Result<const ParserAtom*, OOM> internLatin1(
      JSContext* cx, const JS::Latin1Char* latin1Ptr, uint32_t length);

  JS::Result<const ParserAtom*, OOM> internUtf8(
      JSContext* cx, const mozilla::Utf8Unit* utf8Ptr, uint32_t nbyte);

  JS::Result<const ParserAtom*, OOM> internChar16(JSContext* cx,
                                                  const char16_t* char16Ptr,
                                                  uint32_t length);

  // This only exists for XDR support.
  JS::Result<const ParserAtom*, OOM> internChar16LE(JSContext* cx,
                                                    LittleEndianChars twoByteLE,
                                                    uint32_t length);

  JS::Result<const ParserAtom*, OOM> internJSAtom(
      JSContext* cx, CompilationInfo& compilationInfo, JSAtom* atom);

  JS::Result<const ParserAtom*, OOM> concatAtoms(
      JSContext* cx, mozilla::Range<const ParserAtom*> atoms);
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

} /* namespace frontend */
} /* namespace js */

#endif  // frontend_ParserAtom_h
