/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParserAtom_h
#define frontend_ParserAtom_h

#include "mozilla/HashFunctions.h"  // HashString
#include "mozilla/Variant.h"        // mozilla::Variant

#include "ds/LifoAlloc.h"  // LifoAlloc
#include "js/HashTable.h"  // HashSet
#include "js/UniquePtr.h"  // js::UniquePtr
#include "js/Vector.h"     // Vector
#include "vm/CommonPropertyNames.h"
#include "vm/StringType.h"  // CompareChars, StringEqualsAscii

namespace js {
namespace frontend {

class ParserNameId;

template <typename CharT>
class SpecificParserAtomLookup;

class ParserAtomsTable;

mozilla::GenericErrorResult<OOM&> RaiseParserAtomsOOMError(JSContext* cx);

/**
 * A ParserAtomEntry is an in-parser representation of an interned atomic
 * string.  It mostly mirrors the information carried by a JSAtom*.
 *
 * ParserAtomEntry structs are individually heap-allocated and own their
 * heap-allocated contents.
 */
class ParserAtomEntry {
  friend class ParserAtomsTable;

 public:
  // Owned characters, either 8-bit Latin1, or 16-bit Char16
  mozilla::Variant<JS::UniqueLatin1Chars, JS::UniqueTwoByteChars> chars_;

  // The length of the buffer in chars_.
  uint32_t length_;

  // The JSAtom-compatible hash of the string.
  HashNumber hash_;

  // Used to dynamically optimize the mapping of ParserAtoms to JSAtom*s.
  // If the entry comes from an atom or has been mapped to an
  // atom previously, the atom reference is kept here.
  mutable JSAtom* jsatom_ = nullptr;

  template <typename CharsT>
  ParserAtomEntry(CharsT&& chars, uint32_t length, HashNumber hash)
      : chars_(std::forward<CharsT&&>(chars)), length_(length), hash_(hash) {}

 public:
  // ParserAtomEntries own their content buffers in chars_, and thus cannot
  // be copy-constructed - as a new chars would need to be allocated.
  ParserAtomEntry(const ParserAtomEntry&) = delete;

  ParserAtomEntry(ParserAtomEntry&& other) = default;

  template <typename CharT>
  static ParserAtomEntry make(mozilla::UniquePtr<CharT[], JS::FreePolicy>&& ptr,
                              uint32_t length, HashNumber hash) {
    return ParserAtomEntry(std::move(ptr), length, hash);
  }

  bool hasLatin1Chars() const { return chars_.is<UniqueLatin1Chars>(); }
  bool hasTwoByteChars() const { return chars_.is<UniqueTwoByteChars>(); }

  const Latin1Char* latin1Chars() const {
    MOZ_ASSERT(hasLatin1Chars());
    return chars_.as<UniqueLatin1Chars>().get();
  }
  const char16_t* twoByteChars() const {
    MOZ_ASSERT(hasTwoByteChars());
    return chars_.as<UniqueTwoByteChars>().get();
  }

  bool isIndex(uint32_t* indexp) const;

  HashNumber hash() const { return hash_; }
  uint32_t length() const { return length_; }

  bool equalsJSAtom(JSAtom* other) const;

  template <typename CharT>
  bool equalsSeq(HashNumber hash, InflatedChar16Sequence<CharT> seq) const;

  void setAtom(JSAtom* atom) const {
    MOZ_ASSERT(atom != nullptr);
    if (jsatom_ != nullptr) {
      MOZ_ASSERT(jsatom_ == atom);
      return;
    }
    MOZ_ASSERT(equalsJSAtom(atom));
    jsatom_ = atom;
  }

  // Convert this entry to a js-atom.  The first time this method is called
  // the entry will cache the JSAtom pointer to return later.
  JS::Result<JSAtom*, OOM&> toJSAtom(JSContext* cx) const;

  // Convert this entry to a number.
  bool toNumber(JSContext* cx, double* result) const;
};

class ParserAtomId {
 protected:
  const ParserAtomEntry* entry_;

  struct InitInvalid {};

  explicit ParserAtomId(InitInvalid) : entry_(nullptr) {}

 public:
  explicit ParserAtomId(const ParserAtomEntry* entry) : entry_(entry) {
    MOZ_ASSERT(entry_ != nullptr);
  }
  ParserAtomId() = default;

  bool isValid() const { return entry_ != nullptr; }
  const ParserAtomEntry* entry() const {
    MOZ_ASSERT(isValid());
    return entry_;
  }
  MOZ_IMPLICIT operator bool() const { return isValid(); }

  static ParserAtomId Invalid() { return ParserAtomId(InitInvalid{}); }

  // As the "unchecked" tag signifies, this method should only be called
  // after it has been confirmed that this atom is a name and not an index.
  inline ParserNameId toNameIdUnchecked() const;

  bool operator==(const ParserAtomId& other) { return entry_ == other.entry_; }
  bool operator!=(const ParserAtomId& other) { return !(*this == other); }

  bool isIndex(uint32_t* indexp) const { return entry()->isIndex(indexp); }
  bool equalsJSAtom(JSAtom* other) const {
    return entry()->equalsJSAtom(other);
  }

  size_t length() const { return entry()->length(); }
  size_t empty() const { return length() == 0; }

  struct Hasher {
    using Lookup = ParserAtomId;

    static inline HashNumber hash(const Lookup& l) {
      return DefaultHasher<const ParserAtomEntry*>::hash(l.entry());
    }
    static inline bool match(const ParserAtomId& entry,
                             const ParserAtomId& lookup) {
      return lookup == entry;
    }
  };
};

class ParserNameId : public ParserAtomId {
  explicit ParserNameId(InitInvalid) : ParserAtomId(InitInvalid{}) {}

 public:
  ParserNameId() = default;
  explicit ParserNameId(const ParserAtomEntry* entry) : ParserAtomId(entry) {}
  ParserNameId(const ParserNameId& other) = default;

  static ParserNameId Invalid() { return ParserNameId(InitInvalid{}); }
};

inline ParserNameId ParserAtomId::toNameIdUnchecked() const {
  return ParserNameId(entry_);
}

UniqueChars ParserAtomToPrintableString(JSContext* cx, ParserAtomId atom);

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
 * WellKnown maintains a well-structured reference to common names.
 * A single instance of it is held on the main Runtime, and allows
 * for the looking up of names, but not addition after initialization.
 */
class WellKnownParserAtoms {
 public:
  /* Various built-in or commonly-used names. */
#define PROPERTYNAME_FIELD(idpart, id, text) ParserNameId id{};
  FOR_EACH_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD)
#undef PROPERTYNAME_FIELD

 private:
  using EntrySet = HashSet<UniquePtr<ParserAtomEntry>, ParserAtomLookupHasher,
                           TempAllocPolicy>;
  EntrySet entrySet_;

  bool initSingle(JSContext* cx, ParserNameId* name, const char* str);

 public:
  explicit WellKnownParserAtoms(JSContext* cx) : entrySet_(cx) {}

  bool init(JSContext* cx);

  template <typename CharT>
  ParserAtomId lookupChar16Seq(InflatedChar16Sequence<CharT> seq) const;
};

/**
 * A ParserAtomsTable owns and manages the vector of ParserAtom entries
 * associated with a given compile session.
 */
class ParserAtomsTable {
 private:
  using EntrySet = HashSet<UniquePtr<ParserAtomEntry>, ParserAtomLookupHasher,
                           TempAllocPolicy>;
  EntrySet entrySet_;
  const WellKnownParserAtoms& wellKnownTable_;

 public:
  explicit ParserAtomsTable(JSContext* cx);

 private:
  JS::Result<ParserAtomId, OOM&> addEntry(JSContext* cx,
                                          EntrySet::AddPtr addPtr,
                                          ParserAtomEntry&& entry);

  template <typename AtomCharT, typename SeqCharT>
  JS::Result<ParserAtomId, OOM&> internChar16Seq(
      JSContext* cx, EntrySet::AddPtr add, InflatedChar16Sequence<SeqCharT> seq,
      uint32_t length, HashNumber hash);

  template <typename CharT>
  JS::Result<ParserAtomId, OOM&> lookupOrInternChar16Seq(
      JSContext* cx, InflatedChar16Sequence<CharT> seq);

 public:
  JS::Result<ParserAtomId, OOM&> internChar16(JSContext* cx,
                                              const char16_t* char16Ptr,
                                              uint32_t length);

  JS::Result<ParserAtomId, OOM&> internAscii(JSContext* cx,
                                             const char* asciiPtr,
                                             uint32_t length);

  JS::Result<ParserAtomId, OOM&> internLatin1(JSContext* cx,
                                              const Latin1Char* latin1Ptr,
                                              uint32_t length);

  JS::Result<ParserAtomId, OOM&> internUtf8(JSContext* cx,
                                            const mozilla::Utf8Unit* utf8Ptr,
                                            uint32_t length);

  JS::Result<ParserAtomId, OOM&> internJSAtom(JSContext* cx, JSAtom* atom);

  JS::Result<ParserAtomId, OOM&> concatAtoms(JSContext* cx, ParserAtomId prefix,
                                             ParserAtomId suffix);

  // Lift this code
  // Once all the parser code has been changed to use a ParserAtomId, these
  // can go away.
  JS::Result<JSAtom*, OOM&> toJSAtom(JSContext* cx, ParserAtomId id) const {
    return id.entry()->toJSAtom(cx);
  }
};

template <typename CharT>
class SpecificParserAtomLookup : public ParserAtomLookup {
  // The sequence of characters to look up.
  InflatedChar16Sequence<CharT> seq_;

 public:
  explicit SpecificParserAtomLookup(const InflatedChar16Sequence<CharT>& seq)
      : SpecificParserAtomLookup(seq, computeHash(seq)) {}

  SpecificParserAtomLookup(const InflatedChar16Sequence<CharT>& seq,
                           HashNumber hash)
      : ParserAtomLookup(hash), seq_(seq) {
    MOZ_ASSERT(computeHash(seq_) == hash);
  }

  virtual bool equalsEntry(const ParserAtomEntry* entry) const override {
    return entry->equalsSeq<CharT>(hash_, seq_);
  }

 private:
  static HashNumber computeHash(InflatedChar16Sequence<CharT> seq) {
    HashNumber hash = 0;
    while (seq.hasMore()) {
      hash = mozilla::AddToHash(hash, seq.next());
    }
    return hash;
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
    if (seq.hasMore()) {
      return false;
    }

  } else {
    const Latin1Char* chars = latin1Chars();
    for (uint32_t i = 0; i < length_; i++) {
      if (!seq.hasMore() || char16_t(chars[i]) != seq.next()) {
        return false;
      }
    }
    if (seq.hasMore()) {
      return false;
    }
  }
  return true;
}

} /* namespace frontend */
} /* namespace js */

#endif  // frontend_ParserAtom_h
