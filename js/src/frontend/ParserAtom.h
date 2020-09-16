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

mozilla::GenericErrorResult<OOM&> RaiseParserAtomsOOMError(JSContext* cx);

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
 * The atom contents are stored in one of four locations:
 *  1. Inline Latin1Char storage (immediately after the ParserAtomEntry memory).
 *  2. Inline char16_t storage (immediately after the ParserAtomEntry memory).
 *  3. An owned pointer to a heap-allocated Latin1Char buffer.
 *  4. An owned pointer to a heap-allocated char16_t buffer.
 */
class alignas(alignof(void*)) ParserAtomEntry {
  friend class ParserAtomsTable;
  friend class WellKnownParserAtoms;

  template <typename CharT>
  static constexpr uint32_t MaxInline() {
    return std::is_same_v<CharT, Latin1Char>
               ? JSFatInlineString::MAX_LENGTH_LATIN1
               : JSFatInlineString::MAX_LENGTH_TWO_BYTE;
  }

  /**
   * This single-word variant struct multiplexes between four representations.
   *    1. An owned pointer to a heap-allocated Latin1Char buffer.
   *    2. An owned pointer to a heap-allocated char16_t buffer.
   *    3. A weak Latin1Char pointer to the inline buffer in an entry.
   *    4. A weak char16_t pointer to the inline buffer in an entry.
   *
   * The lowest bit of the tagged pointer is used to distinguish between
   * character widths.
   *
   * The second lowest bit of the tagged pointer is used to distinguish
   * between heap-ptr contents and inline contents.
   */
  struct ContentPtrVariant {
    uintptr_t taggedPtr;

    static const uintptr_t CHARTYPE_MASK = 0x1;
    static const uintptr_t CHARTYPE_LATIN1 = 0x0;
    static const uintptr_t CHARTYPE_TWO_BYTE = 0x1;

    static const uintptr_t LOCATION_MASK = 0x2;
    static const uintptr_t LOCATION_INLINE = 0x0;
    static const uintptr_t LOCATION_HEAP = 0x2;

    static const uintptr_t LOWBITS_MASK = CHARTYPE_MASK | LOCATION_MASK;

    // A tagged ptr representation for no contents.  The taggedPtr
    // field is set to when contents are moved out of a ParserAtomEntry,
    // so that the original atom (moved from) does not try to destroy/free
    // its contents.
    static const uintptr_t EMPTY_TAGGED_PTR =
        CHARTYPE_LATIN1 | LOCATION_INLINE | (0x0 /* nullptr */);

    template <typename CharT>
    static uintptr_t Tag(const CharT* ptr, bool isInline) {
      static_assert(std::is_same_v<CharT, Latin1Char> ||
                    std::is_same_v<CharT, char16_t>);
      return uintptr_t(ptr) |
             (std::is_same_v<CharT, Latin1Char> ? CHARTYPE_LATIN1
                                                : CHARTYPE_TWO_BYTE) |
             (isInline ? LOCATION_INLINE : LOCATION_HEAP);
    }

    // The variant owns data, so move semantics apply.
    ContentPtrVariant(const ContentPtrVariant& other) = delete;

    // Raw pointer constructor.
    template <typename CharT>
    ContentPtrVariant(const CharT* weakContents, bool isInline)
        : taggedPtr(Tag(weakContents, isInline)) {
      static_assert(std::is_same_v<CharT, Latin1Char> ||
                    std::is_same_v<CharT, char16_t>);
      MOZ_ASSERT((reinterpret_cast<uintptr_t>(weakContents) & LOWBITS_MASK) ==
                 0);
    }

    // Owned pointer construction.
    template <typename CharT>
    explicit ContentPtrVariant(
        mozilla::UniquePtr<CharT[], JS::FreePolicy> owned)
        : ContentPtrVariant(owned.release(), false) {}

    // Move construction.
    // Clear the other variant's contents to not free content after move.
    ContentPtrVariant(ContentPtrVariant&& other) : taggedPtr(other.taggedPtr) {
      other.taggedPtr = EMPTY_TAGGED_PTR;
    }

    ~ContentPtrVariant() {
      if (isInline()) {
        return;
      }

      // Re-construct UniqueChars<CharT[]> and destroy them.
      if (hasCharType<Latin1Char>()) {
        UniqueLatin1Chars chars(getUnchecked<Latin1Char>());
      } else {
        UniqueTwoByteChars chars(getUnchecked<char16_t>());
      }
    }

    template <typename T>
    const T* getUnchecked() const {
      return reinterpret_cast<const T*>(taggedPtr & ~LOWBITS_MASK);
    }
    template <typename T>
    T* getUnchecked() {
      return reinterpret_cast<T*>(taggedPtr & ~LOWBITS_MASK);
    }
    template <typename CharT>
    bool hasCharType() const {
      static_assert(std::is_same_v<CharT, Latin1Char> ||
                    std::is_same_v<CharT, char16_t>);
      return (taggedPtr & CHARTYPE_MASK) ==
             (std::is_same_v<CharT, Latin1Char> ? CHARTYPE_LATIN1
                                                : CHARTYPE_TWO_BYTE);
    }
    bool isInline() const {
      return (taggedPtr & LOCATION_MASK) == LOCATION_INLINE;
    }
  };

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
  // Owned characters, either 8-bit Latin1Char, or 16-bit char16_t
  ContentPtrVariant variant_;

  // The length of the buffer in chars_.
  uint32_t length_;

  // The JSAtom-compatible hash of the string.
  HashNumber hash_;

  // Used to dynamically optimize the mapping of ParserAtoms to JSAtom*s.
  //
  // If this ParserAtomEntry is a part of WellKnownParserAtoms, this should
  // hold WellKnownAtomId that maps to an item in cx->names().
  //
  // Otherwise, this should hold AtomIndex into CompilationInfo.atoms,
  // or empty if the JSAtom isn't yet allocated.
  using AtomIndexType =
      mozilla::Variant<mozilla::Nothing, AtomIndex, WellKnownAtomId,
                       StaticParserString1, StaticParserString2>;
  mutable AtomIndexType atomIndex_ = AtomIndexType(mozilla::Nothing());

 public:
  static const uint32_t MAX_LENGTH = JSString::MAX_LENGTH;

  template <typename CharT>
  ParserAtomEntry(mozilla::UniquePtr<CharT[], JS::FreePolicy> chars,
                  uint32_t length, HashNumber hash)
      : variant_(std::move(chars)), length_(length), hash_(hash) {}

  template <typename CharT>
  ParserAtomEntry(const CharT* chars, uint32_t length, HashNumber hash)
      : variant_(chars, /* isInline = */ true), length_(length), hash_(hash) {}

  template <typename CharT>
  static CharT* inlineBufferPtr(ParserAtomEntry* entry) {
    return reinterpret_cast<CharT*>(entry + 1);
  }
  template <typename CharT>
  static const CharT* inlineBufferPtr(const ParserAtomEntry* entry) {
    return reinterpret_cast<const CharT*>(entry + 1);
  }

  template <typename CharT>
  static JS::Result<UniquePtr<ParserAtomEntry>, OOM&> allocate(
      JSContext* cx, mozilla::UniquePtr<CharT[], JS::FreePolicy>&& ptr,
      uint32_t length, HashNumber hash);

  template <typename CharT, typename SeqCharT>
  static JS::Result<UniquePtr<ParserAtomEntry>, OOM&> allocateInline(
      JSContext* cx, InflatedChar16Sequence<SeqCharT> seq, uint32_t length,
      HashNumber hash);

 public:
  // ParserAtomEntries may own their content buffers in variant_, and thus
  // cannot be copy-constructed - as a new chars would need to be allocated.
  ParserAtomEntry(const ParserAtomEntry&) = delete;

  ParserAtomEntry(ParserAtomEntry&& other) = delete;

  ParserAtom* asAtom() { return reinterpret_cast<ParserAtom*>(this); }
  const ParserAtom* asAtom() const {
    return reinterpret_cast<const ParserAtom*>(this);
  }

  inline ParserName* asName();
  inline const ParserName* asName() const;

  bool hasLatin1Chars() const { return variant_.hasCharType<Latin1Char>(); }
  bool hasTwoByteChars() const { return variant_.hasCharType<char16_t>(); }

  const Latin1Char* latin1Chars() const {
    MOZ_ASSERT(hasLatin1Chars());
    return variant_.getUnchecked<Latin1Char>();
  }
  const char16_t* twoByteChars() const {
    MOZ_ASSERT(hasTwoByteChars());
    return variant_.getUnchecked<char16_t>();
  }
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

  void setAtomIndex(AtomIndex index) const {
    atomIndex_ = mozilla::AsVariant(index);
  }
  void setWellKnownAtomId(WellKnownAtomId kind) const {
    atomIndex_ = mozilla::AsVariant(kind);
  }
  void setStaticParserString1(StaticParserString1 s) const {
    atomIndex_ = mozilla::AsVariant(s);
  }
  void setStaticParserString2(StaticParserString2 s) const {
    atomIndex_ = mozilla::AsVariant(s);
  }

  // Convert this entry to a js-atom.  The first time this method is called
  // the entry will cache the JSAtom pointer to return later.
  JS::Result<JSAtom*, OOM&> toJSAtom(JSContext* cx,
                                     CompilationInfo& compilationInfo) const;

  // Convert this entry to a number.
  bool toNumber(JSContext* cx, double* result) const;

#if defined(DEBUG) || defined(JS_JITSPEW)
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

 private:
  using EntrySet = HashSet<UniquePtr<ParserAtomEntry>, ParserAtomLookupHasher,
                           js::SystemAllocPolicy>;
  EntrySet entrySet_;

  static const size_t ASCII_STATIC_LIMIT = 128U;
  static const size_t NUM_SMALL_CHARS = StaticStrings::NUM_SMALL_CHARS;
  const ParserAtom* length1StaticTable_[ASCII_STATIC_LIMIT] = {};
  const ParserAtom* length2StaticTable_[NUM_SMALL_CHARS * NUM_SMALL_CHARS] = {};

  bool initSingle(JSContext* cx, const ParserName** name, const char* str,
                  WellKnownAtomId kind);

  bool initStaticStrings(JSContext* cx);

  const ParserAtom* getLength1String(char16_t ch) const {
    MOZ_ASSERT(ch < StaticStrings::UNIT_STATIC_LIMIT);
    return length1StaticTable_[static_cast<size_t>(ch)];
  }
  const ParserAtom* getLength2String(char16_t ch0, char16_t ch1) const {
    return length2StaticTable_[StaticStrings::getLength2Index(ch0, ch1)];
  }

 public:
  WellKnownParserAtoms() = default;

  bool init(JSContext* cx);

  template <typename CharT>
  const ParserAtom* lookupChar16Seq(
      const SpecificParserAtomLookup<CharT>& lookup) const;

  // Fast-path tiny strings since they are abundant in minified code.
  template <typename CharT>
  const ParserAtom* lookupTiny(const CharT* charPtr, uint32_t length) const {
    switch (length) {
      case 0:
        return empty;

      case 1: {
        if (char16_t(charPtr[0]) < ASCII_STATIC_LIMIT) {
          return getLength1String(charPtr[0]);
        }
        break;
      }

      case 2:
        if (StaticStrings::fitsInSmallChar(charPtr[0]) &&
            StaticStrings::fitsInSmallChar(charPtr[1])) {
          return getLength2String(charPtr[0], charPtr[1]);
        }
        break;
    }

    // No match on tiny Atoms
    return nullptr;
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
  // Custom AddPtr for the ParserAtomsTable.
  struct AddPtr {
    struct InnerAddPtr {
      EntrySet::AddPtr entrySetAddPtr;
      HashNumber hash;
      InnerAddPtr(EntrySet::AddPtr e, HashNumber h)
          : entrySetAddPtr(e), hash(h) {}
    };
    using VariantType = mozilla::Variant<const ParserAtomEntry*, InnerAddPtr>;
    VariantType atomOrAdd;

    explicit AddPtr(const ParserAtomEntry* atom) : atomOrAdd(atom) {}

    AddPtr(EntrySet::AddPtr entrySetAddPtr, HashNumber hash)
        : atomOrAdd((const ParserAtomEntry*)nullptr) {
      if (entrySetAddPtr) {
        atomOrAdd = VariantType(
            const_cast<const ParserAtomEntry*>(entrySetAddPtr->get()));
      } else {
        atomOrAdd = VariantType(InnerAddPtr(entrySetAddPtr, hash));
      }
    }

    explicit operator bool() const {
      return atomOrAdd.is<const ParserAtomEntry*>();
    }
    const ParserAtomEntry* get() const {
      return atomOrAdd.as<const ParserAtomEntry*>();
    }
    InnerAddPtr& inner() { return atomOrAdd.as<InnerAddPtr>(); }
  };

  // Look up a sequence pointer for add.  Returns either the found
  // parser-atom pointer, or and AddPtr to insert into the entry-set.
  template <typename CharT>
  AddPtr lookupForAdd(JSContext* cx, InflatedChar16Sequence<CharT> seq);

  // Internal APIs for interning to the table after well-known atoms cases have
  // been tested.
  JS::Result<const ParserAtom*, OOM&> addEntry(
      JSContext* cx, AddPtr& addPtr, UniquePtr<ParserAtomEntry> entry);
  JS::Result<const ParserAtom*, OOM&> internLatin1Seq(
      JSContext* cx, AddPtr& addPtr, const Latin1Char* latin1Ptr,
      uint32_t length);
  template <typename AtomCharT, typename SeqCharT>
  JS::Result<const ParserAtom*, OOM&> internChar16Seq(
      JSContext* cx, AddPtr& addPtr, InflatedChar16Sequence<SeqCharT> seq,
      uint32_t length);

 public:
  JS::Result<const ParserAtom*, OOM&> internAscii(JSContext* cx,
                                                  const char* asciiPtr,
                                                  uint32_t length);

  JS::Result<const ParserAtom*, OOM&> internLatin1(JSContext* cx,
                                                   const Latin1Char* latin1Ptr,
                                                   uint32_t length);

  JS::Result<const ParserAtom*, OOM&> internUtf8(
      JSContext* cx, const mozilla::Utf8Unit* utf8Ptr, uint32_t nbyte);

  JS::Result<const ParserAtom*, OOM&> internChar16(JSContext* cx,
                                                   const char16_t* char16Ptr,
                                                   uint32_t length);

  JS::Result<const ParserAtom*, OOM&> internJSAtom(
      JSContext* cx, CompilationInfo& compilationInfo, JSAtom* atom);

  JS::Result<const ParserAtom*, OOM&> concatAtoms(
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
