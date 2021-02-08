/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParserAtom.h"

#include <memory>  // std::uninitialized_fill_n
#include <type_traits>

#include "jsnum.h"  // CharsToNumber

#include "frontend/CompilationInfo.h"
#include "frontend/NameCollections.h"
#include "frontend/StencilXdr.h"  // CanCopyDataToDisk
#include "vm/JSContext.h"
#include "vm/Printer.h"  // Sprinter, QuoteString
#include "vm/Runtime.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::frontend;

namespace js {

// Iterates over a sequence of ParserAtoms and yield their sequence of
// characters in order. This simulates concatenation of atoms. The underlying
// ParserAtoms may be a mix of Latin1 and char16_t atoms.
template <>
class InflatedChar16Sequence<const ParserAtom*> {
 private:
  const ParserAtom** cur_ = nullptr;
  const ParserAtom** lim_ = nullptr;
  size_t index_ = 0;

  void settle() {
    // Check if we are out-of-bounds for current ParserAtom.
    auto outOfBounds = [this]() { return index_ >= (*cur_)->length(); };

    while (hasMore() && outOfBounds()) {
      // Advance to start of next ParserAtom.
      cur_++;
      index_ = 0;
    }
  }

 public:
  explicit InflatedChar16Sequence(
      const mozilla::Range<const ParserAtom*>& atoms)
      : cur_(atoms.begin().get()), lim_(atoms.end().get()) {
    settle();
  }

  bool hasMore() { return cur_ < lim_; }

  char16_t next() {
    MOZ_ASSERT(hasMore());
    char16_t ch = (*cur_)->hasLatin1Chars() ? (*cur_)->latin1Chars()[index_]
                                            : (*cur_)->twoByteChars()[index_];
    index_++;
    settle();
    return ch;
  }

  HashNumber computeHash() const {
    auto copy = *this;
    HashNumber hash = 0;

    while (copy.hasMore()) {
      hash = mozilla::AddToHash(hash, copy.next());
    }
    return hash;
  }
};

}  // namespace js

namespace js {

template <>
class InflatedChar16Sequence<LittleEndianChars> {
 private:
  LittleEndianChars chars_;
  size_t idx_;
  size_t len_;

 public:
  InflatedChar16Sequence(LittleEndianChars chars, size_t length)
      : chars_(chars), idx_(0), len_(length) {}

  bool hasMore() { return idx_ < len_; }

  char16_t next() {
    MOZ_ASSERT(hasMore());
    return chars_[idx_++];
  }

  HashNumber computeHash() const {
    auto copy = *this;
    HashNumber hash = 0;
    while (copy.hasMore()) {
      hash = mozilla::AddToHash(hash, copy.next());
    }
    return hash;
  }
};

}  // namespace js

namespace js {
namespace frontend {

JSAtom* GetWellKnownAtom(JSContext* cx, WellKnownAtomId atomId) {
#define ASSERT_OFFSET_(_, NAME, _2)              \
  static_assert(offsetof(JSAtomState, NAME) ==   \
                int32_t(WellKnownAtomId::NAME) * \
                    sizeof(js::ImmutablePropertyNamePtr));
  FOR_EACH_COMMON_PROPERTYNAME(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

#define ASSERT_OFFSET_(NAME, _)                  \
  static_assert(offsetof(JSAtomState, NAME) ==   \
                int32_t(WellKnownAtomId::NAME) * \
                    sizeof(js::ImmutablePropertyNamePtr));
  JS_FOR_EACH_PROTOTYPE(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

  static_assert(int32_t(WellKnownAtomId::abort) == 0,
                "Unexpected order of WellKnownAtom");

  return (&cx->names().abort)[int32_t(atomId)];
}

#ifdef DEBUG
void TaggedParserAtomIndex::validateRaw() {
  if (isParserAtomIndex()) {
    MOZ_ASSERT(toParserAtomIndex().index < IndexLimit);
  } else if (isWellKnownAtomId()) {
    MOZ_ASSERT(uint32_t(toWellKnownAtomId()) <
               uint32_t(WellKnownAtomId::Limit));
  } else if (isStaticParserString1()) {
    MOZ_ASSERT(size_t(toStaticParserString1()) <
               WellKnownParserAtoms_ROM::ASCII_STATIC_LIMIT);
  } else if (isStaticParserString2()) {
    MOZ_ASSERT(size_t(toStaticParserString2()) <
               WellKnownParserAtoms_ROM::NUM_LENGTH2_ENTRIES);
  } else {
    MOZ_ASSERT(isNull());
  }
}
#endif

template <typename CharT, typename SeqCharT>
/* static */ ParserAtom* ParserAtom::allocate(
    JSContext* cx, LifoAlloc& alloc, InflatedChar16Sequence<SeqCharT> seq,
    uint32_t length, HashNumber hash) {
  constexpr size_t HeaderSize = sizeof(ParserAtom);
  void* raw = alloc.alloc(HeaderSize + (sizeof(CharT) * length));
  if (!raw) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }

  constexpr bool hasTwoByteChars = (sizeof(CharT) == 2);
  static_assert(sizeof(CharT) == 1 || sizeof(CharT) == 2,
                "CharT should be 1 or 2 byte type");
  ParserAtom* entry = new (raw) ParserAtom(length, hash, hasTwoByteChars);
  CharT* entryBuf = entry->chars<CharT>();
  drainChar16Seq(entryBuf, seq, length);
  return entry;
}

/* static */ ParserAtom* ParserAtom::allocateRaw(JSContext* cx,
                                                 LifoAlloc& alloc,
                                                 const uint8_t* srcRaw,
                                                 size_t totalLength) {
  void* raw = alloc.alloc(totalLength);
  if (!raw) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }

  memcpy(raw, srcRaw, totalLength);

  return static_cast<ParserAtom*>(raw);
}

bool ParserAtom::equalsJSAtom(JSAtom* other) const {
  // Compare hashes and lengths first.
  if (hash_ != other->hash() || length_ != other->length()) {
    return false;
  }

  JS::AutoCheckCannotGC nogc;

  if (hasTwoByteChars()) {
    // Compare heap-allocated 16-bit chars to atom.
    return other->hasLatin1Chars()
               ? EqualChars(twoByteChars(), other->latin1Chars(nogc), length_)
               : EqualChars(twoByteChars(), other->twoByteChars(nogc), length_);
  }

  MOZ_ASSERT(hasLatin1Chars());
  return other->hasLatin1Chars()
             ? EqualChars(latin1Chars(), other->latin1Chars(nogc), length_)
             : EqualChars(latin1Chars(), other->twoByteChars(nogc), length_);
}

bool ParserAtom::isIndex(uint32_t* indexp) const {
  size_t len = length();
  if (len == 0 || len > UINT32_CHAR_BUFFER_LENGTH) {
    return false;
  }
  if (hasLatin1Chars()) {
    return mozilla::IsAsciiDigit(*latin1Chars()) &&
           js::CheckStringIsIndex(latin1Chars(), len, indexp);
  }
  return mozilla::IsAsciiDigit(*twoByteChars()) &&
         js::CheckStringIsIndex(twoByteChars(), len, indexp);
}

bool ParserAtom::isPrivateName() const {
  return length() >= 2 && (hasLatin1Chars() ? latin1Chars()[0] == '#'
                                            : twoByteChars()[0] == '#');
}

JSAtom* ParserAtom::toJSAtom(JSContext* cx, TaggedParserAtomIndex index,
                             CompilationAtomCache& atomCache) const {
  if (index.isParserAtomIndex()) {
    JSAtom* atom = atomCache.getAtomAt(index.toParserAtomIndex());
    if (atom) {
      return atom;
    }

    return instantiate(cx, index, atomCache);
  }

  if (index.isWellKnownAtomId()) {
    return GetWellKnownAtom(cx, index.toWellKnownAtomId());
  }

  if (index.isStaticParserString1()) {
    char16_t ch = static_cast<char16_t>(index.toStaticParserString1());
    return cx->staticStrings().getUnit(ch);
  }

  MOZ_ASSERT(index.isStaticParserString2());
  size_t s = static_cast<size_t>(index.toStaticParserString2());
  return cx->staticStrings().getLength2FromIndex(s);
}

JSAtom* ParserAtom::instantiate(JSContext* cx, TaggedParserAtomIndex index,
                                CompilationAtomCache& atomCache) const {
  MOZ_ASSERT(index.isParserAtomIndex());

  JSAtom* atom;
  if (hasLatin1Chars()) {
    atom = AtomizeChars(cx, hash(), latin1Chars(), length());
  } else {
    atom = AtomizeChars(cx, hash(), twoByteChars(), length());
  }
  if (!atom) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }
  if (!atomCache.setAtomAt(cx, index.toParserAtomIndex(), atom)) {
    return nullptr;
  }

  return atom;
}

bool ParserAtom::toNumber(JSContext* cx, double* result) const {
  return hasLatin1Chars() ? CharsToNumber(cx, latin1Chars(), length(), result)
                          : CharsToNumber(cx, twoByteChars(), length(), result);
}

#if defined(DEBUG) || defined(JS_JITSPEW)
void ParserAtom::dump() const {
  js::Fprinter out(stderr);
  out.put("\"");
  dumpCharsNoQuote(out);
  out.put("\"\n");
}

void ParserAtom::dumpCharsNoQuote(js::GenericPrinter& out) const {
  if (hasLatin1Chars()) {
    JSString::dumpCharsNoQuote<Latin1Char>(latin1Chars(), length(), out);
  } else {
    JSString::dumpCharsNoQuote<char16_t>(twoByteChars(), length(), out);
  }
}

void ParserAtomsTable::dumpCharsNoQuote(js::GenericPrinter& out,
                                        TaggedParserAtomIndex index) const {
  const auto* atom = getParserAtom(index);
  atom->dumpCharsNoQuote(out);
}

#endif

ParserAtomsTable::ParserAtomsTable(JSRuntime* rt, LifoAlloc& alloc)
    : wellKnownTable_(*rt->commonParserNames), alloc_(alloc) {}

TaggedParserAtomIndex ParserAtomsTable::addEntry(JSContext* cx,
                                                 EntryMap::AddPtr& addPtr,
                                                 ParserAtom* entry) {
  MOZ_ASSERT(!addPtr);
  ParserAtomIndex index = ParserAtomIndex(entries_.length());
  if (size_t(index) >= TaggedParserAtomIndex::IndexLimit) {
    ReportAllocationOverflow(cx);
    return TaggedParserAtomIndex::null();
  }
  if (!entries_.append(entry)) {
    js::ReportOutOfMemory(cx);
    return TaggedParserAtomIndex::null();
  }
  auto taggedIndex = TaggedParserAtomIndex(index);
  if (!entryMap_.add(addPtr, entry, taggedIndex)) {
    js::ReportOutOfMemory(cx);
    return TaggedParserAtomIndex::null();
  }
  return taggedIndex;
}

template <typename AtomCharT, typename SeqCharT>
TaggedParserAtomIndex ParserAtomsTable::internChar16Seq(
    JSContext* cx, EntryMap::AddPtr& addPtr, HashNumber hash,
    InflatedChar16Sequence<SeqCharT> seq, uint32_t length) {
  MOZ_ASSERT(!addPtr);

  ParserAtom* entry =
      ParserAtom::allocate<AtomCharT>(cx, alloc_, seq, length, hash);
  if (!entry) {
    return TaggedParserAtomIndex::null();
  }
  return addEntry(cx, addPtr, entry);
}

static const uint16_t MAX_LATIN1_CHAR = 0xff;

TaggedParserAtomIndex ParserAtomsTable::internAscii(JSContext* cx,
                                                    const char* asciiPtr,
                                                    uint32_t length) {
  // ASCII strings are strict subsets of Latin1 strings.
  const Latin1Char* latin1Ptr = reinterpret_cast<const Latin1Char*>(asciiPtr);
  return internLatin1(cx, latin1Ptr, length);
}

TaggedParserAtomIndex ParserAtomsTable::internLatin1(
    JSContext* cx, const Latin1Char* latin1Ptr, uint32_t length) {
  // Check for tiny strings which are abundant in minified code.
  if (auto tiny = wellKnownTable_.lookupTinyIndex(latin1Ptr, length)) {
    return tiny;
  }

  // Check for well-known atom.
  InflatedChar16Sequence<Latin1Char> seq(latin1Ptr, length);
  SpecificParserAtomLookup<Latin1Char> lookup(seq);
  if (auto wk = wellKnownTable_.lookupChar16Seq(lookup)) {
    return wk;
  }

  // Check for existing atom.
  auto addPtr = entryMap_.lookupForAdd(lookup);
  if (addPtr) {
    return addPtr->value();
  }

  return internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq, length);
}

ParserAtomSpanBuilder::ParserAtomSpanBuilder(JSRuntime* rt,
                                             ParserAtomSpan& entries)
    : wellKnownTable_(*rt->commonParserNames), entries_(entries) {}

bool ParserAtomSpanBuilder::allocate(JSContext* cx, LifoAlloc& alloc,
                                     size_t count) {
  if (count >= TaggedParserAtomIndex::IndexLimit) {
    ReportAllocationOverflow(cx);
    return false;
  }

  auto* p = alloc.newArrayUninitialized<ParserAtom*>(count);
  if (!p) {
    js::ReportOutOfMemory(cx);
    return false;
  }
  std::uninitialized_fill_n(p, count, nullptr);

  entries_ = mozilla::Span(p, count);
  return true;
}

TaggedParserAtomIndex ParserAtomsTable::internUtf8(
    JSContext* cx, const mozilla::Utf8Unit* utf8Ptr, uint32_t nbyte) {
  // Check for tiny strings which are abundant in minified code.
  // NOTE: The tiny atoms are all ASCII-only so we can directly look at the
  //        UTF-8 data without worrying about surrogates.
  if (auto tiny = wellKnownTable_.lookupTinyIndex(
          reinterpret_cast<const Latin1Char*>(utf8Ptr), nbyte)) {
    return tiny;
  }

  // If source text is ASCII, then the length of the target char buffer
  // is the same as the length of the UTF8 input.  Convert it to a Latin1
  // encoded string on the heap.
  JS::UTF8Chars utf8(utf8Ptr, nbyte);
  JS::SmallestEncoding minEncoding = FindSmallestEncoding(utf8);
  if (minEncoding == JS::SmallestEncoding::ASCII) {
    // As ascii strings are a subset of Latin1 strings, and each encoding
    // unit is the same size, we can reliably cast this `Utf8Unit*`
    // to a `Latin1Char*`.
    const Latin1Char* latin1Ptr = reinterpret_cast<const Latin1Char*>(utf8Ptr);
    return internLatin1(cx, latin1Ptr, nbyte);
  }

  // Check for existing.
  // NOTE: Well-known are all ASCII so have been handled above.
  InflatedChar16Sequence<mozilla::Utf8Unit> seq(utf8Ptr, nbyte);
  SpecificParserAtomLookup<mozilla::Utf8Unit> lookup(seq);
  MOZ_ASSERT(!wellKnownTable_.lookupChar16Seq(lookup));
  EntryMap::AddPtr addPtr = entryMap_.lookupForAdd(lookup);
  if (addPtr) {
    return addPtr->value();
  }

  // Compute length in code-points.
  uint32_t length = 0;
  InflatedChar16Sequence<mozilla::Utf8Unit> seqCopy = seq;
  while (seqCopy.hasMore()) {
    mozilla::Unused << seqCopy.next();
    length += 1;
  }

  // Otherwise, add new entry.
  bool wide = (minEncoding == JS::SmallestEncoding::UTF16);
  return wide
             ? internChar16Seq<char16_t>(cx, addPtr, lookup.hash(), seq, length)
             : internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq,
                                           length);
}

TaggedParserAtomIndex ParserAtomsTable::internChar16(JSContext* cx,
                                                     const char16_t* char16Ptr,
                                                     uint32_t length) {
  // Check for tiny strings which are abundant in minified code.
  if (auto tiny = wellKnownTable_.lookupTinyIndex(char16Ptr, length)) {
    return tiny;
  }

  // Check against well-known.
  InflatedChar16Sequence<char16_t> seq(char16Ptr, length);
  SpecificParserAtomLookup<char16_t> lookup(seq);
  if (auto wk = wellKnownTable_.lookupChar16Seq(lookup)) {
    return wk;
  }

  // Check for existing atom.
  EntryMap::AddPtr addPtr = entryMap_.lookupForAdd(lookup);
  if (addPtr) {
    return addPtr->value();
  }

  // Compute the target encoding.
  // NOTE: Length in code-points will be same, even if we deflate to Latin1.
  bool wide = false;
  InflatedChar16Sequence<char16_t> seqCopy = seq;
  while (seqCopy.hasMore()) {
    char16_t ch = seqCopy.next();
    if (ch > MAX_LATIN1_CHAR) {
      wide = true;
      break;
    }
  }

  // Otherwise, add new entry.
  return wide
             ? internChar16Seq<char16_t>(cx, addPtr, lookup.hash(), seq, length)
             : internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq,
                                           length);
}

TaggedParserAtomIndex ParserAtomsTable::internJSAtom(
    JSContext* cx, CompilationAtomCache& atomCache, JSAtom* atom) {
  TaggedParserAtomIndex parserAtom;
  {
    JS::AutoCheckCannotGC nogc;

    parserAtom =
        atom->hasLatin1Chars()
            ? internLatin1(cx, atom->latin1Chars(nogc), atom->length())
            : internChar16(cx, atom->twoByteChars(nogc), atom->length());
    if (!parserAtom) {
      return TaggedParserAtomIndex::null();
    }
  }

  if (parserAtom.isParserAtomIndex()) {
    ParserAtomIndex index = parserAtom.toParserAtomIndex();
    if (!atomCache.hasAtomAt(index)) {
      if (!atomCache.setAtomAt(cx, index, atom)) {
        return TaggedParserAtomIndex::null();
      }
    }
  }

  // We should (infallibly) map back to the same JSAtom.
  MOZ_ASSERT(getParserAtom(parserAtom)->toJSAtom(cx, parserAtom, atomCache) ==
             atom);

  return parserAtom;
}

TaggedParserAtomIndex ParserAtomsTable::concatAtoms(
    JSContext* cx, mozilla::Range<const ParserAtom*> atoms) {
  MOZ_ASSERT(atoms.length() >= 2,
             "concatAtoms should only be used for multiple inputs");

  // Compute final length and encoding.
  bool catLatin1 = true;
  uint32_t catLen = 0;
  for (const ParserAtom* atom : atoms) {
    if (atom->hasTwoByteChars()) {
      catLatin1 = false;
    }
    // Overflow check here, length
    if (atom->length() >= (ParserAtom::MAX_LENGTH - catLen)) {
      js::ReportOutOfMemory(cx);
      return TaggedParserAtomIndex::null();
    }
    catLen += atom->length();
  }

  // Short Latin1 strings must check for both Tiny and WellKnown atoms so simple
  // concatenate onto stack and use `internLatin1`.
  if (catLatin1 && (catLen <= WellKnownParserAtoms::MaxWellKnownLength)) {
    Latin1Char buf[WellKnownParserAtoms::MaxWellKnownLength];
    size_t offset = 0;
    for (const ParserAtom* atom : atoms) {
      mozilla::PodCopy(buf + offset, atom->latin1Chars(), atom->length());
      offset += atom->length();
    }
    return internLatin1(cx, buf, catLen);
  }

  // NOTE: We have ruled out Tiny and WellKnown atoms and can ignore below.

  InflatedChar16Sequence<const ParserAtom*> seq(atoms);
  SpecificParserAtomLookup<const ParserAtom*> lookup(seq);

  // Check for existing atom.
  auto addPtr = entryMap_.lookupForAdd(lookup);
  if (addPtr) {
    return addPtr->value();
  }

  // Otherwise, add new entry.
  return catLatin1 ? internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq,
                                                 catLen)
                   : internChar16Seq<char16_t>(cx, addPtr, lookup.hash(), seq,
                                               catLen);
}

const ParserAtom* WellKnownParserAtoms::getWellKnown(
    WellKnownAtomId atomId) const {
#define ASSERT_OFFSET_(_, NAME, _2)                     \
  static_assert(offsetof(WellKnownParserAtoms, NAME) == \
                int32_t(WellKnownAtomId::NAME) * sizeof(ParserAtom*));
  FOR_EACH_COMMON_PROPERTYNAME(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

#define ASSERT_OFFSET_(NAME, _)                         \
  static_assert(offsetof(WellKnownParserAtoms, NAME) == \
                int32_t(WellKnownAtomId::NAME) * sizeof(ParserAtom*));
  JS_FOR_EACH_PROTOTYPE(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

  static_assert(int32_t(WellKnownAtomId::abort) == 0,
                "Unexpected order of WellKnownAtom");

  return (&abort)[int32_t(atomId)];
}

/* static */
const ParserAtom* WellKnownParserAtoms::getStatic1(StaticParserString1 s) {
  return &WellKnownParserAtoms::rom_.length1Table[size_t(s)];
}

/* static */
const ParserAtom* WellKnownParserAtoms::getStatic2(StaticParserString2 s) {
  return &WellKnownParserAtoms::rom_.length2Table[size_t(s)];
}

const ParserAtom* ParserAtomsTable::getWellKnown(WellKnownAtomId atomId) const {
  return wellKnownTable_.getWellKnown(atomId);
}

const ParserAtom* ParserAtomsTable::getStatic1(StaticParserString1 s) const {
  return WellKnownParserAtoms::getStatic1(s);
}

const ParserAtom* ParserAtomsTable::getStatic2(StaticParserString2 s) const {
  return WellKnownParserAtoms::getStatic2(s);
}

const ParserAtom* ParserAtomsTable::getParserAtom(ParserAtomIndex index) const {
  return entries_[index];
}

const ParserAtom* ParserAtomsTable::getParserAtom(
    TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    return getParserAtom(index.toParserAtomIndex());
  }

  if (index.isWellKnownAtomId()) {
    return getWellKnown(index.toWellKnownAtomId());
  }

  if (index.isStaticParserString1()) {
    return getStatic1(index.toStaticParserString1());
  }

  if (index.isStaticParserString2()) {
    return getStatic2(index.toStaticParserString2());
  }

  MOZ_ASSERT(index.isNull());
  return nullptr;
}

void ParserAtomsTable::markUsedByStencil(TaggedParserAtomIndex index) const {
  getParserAtom(index)->markUsedByStencil();
}

bool ParserAtomsTable::isPrivateName(TaggedParserAtomIndex index) const {
  if (!index.isParserAtomIndex()) {
    MOZ_ASSERT(getParserAtom(index)->isPrivateName() == false);
    return false;
  }

  return getParserAtom(index.toParserAtomIndex())->isPrivateName();
}

bool ParserAtomsTable::isIndex(TaggedParserAtomIndex index,
                               uint32_t* indexp) const {
  return getParserAtom(index)->isIndex(indexp);
}

uint32_t ParserAtomsTable::length(TaggedParserAtomIndex index) const {
  return getParserAtom(index)->length();
}

bool ParserAtomsTable::toNumber(JSContext* cx, TaggedParserAtomIndex index,
                                double* result) const {
  return getParserAtom(index)->toNumber(cx, result);
}

UniqueChars ParserAtomsTable::toNewUTF8CharsZ(
    JSContext* cx, TaggedParserAtomIndex index) const {
  const auto* atom = getParserAtom(index);
  return UniqueChars(
      atom->hasLatin1Chars()
          ? JS::CharsToNewUTF8CharsZ(cx, atom->latin1Range()).c_str()
          : JS::CharsToNewUTF8CharsZ(cx, atom->twoByteRange()).c_str());
}

template <typename CharT>
UniqueChars ToPrintableStringImpl(JSContext* cx, mozilla::Range<CharT> str,
                                  char quote = '\0') {
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return nullptr;
  }
  if (!QuoteString<QuoteTarget::String>(&sprinter, str, quote)) {
    return nullptr;
  }
  return sprinter.release();
}

UniqueChars ParserAtomsTable::toPrintableString(
    JSContext* cx, TaggedParserAtomIndex index) const {
  const auto* atom = getParserAtom(index);
  size_t length = atom->length();
  return atom->hasLatin1Chars()
             ? ToPrintableStringImpl(
                   cx, mozilla::Range(atom->latin1Chars(), length))
             : ToPrintableStringImpl(
                   cx, mozilla::Range(atom->twoByteChars(), length));
}

UniqueChars ParserAtomsTable::toQuotedString(
    JSContext* cx, TaggedParserAtomIndex index) const {
  const auto* atom = getParserAtom(index);
  size_t length = atom->length();
  return atom->hasLatin1Chars()
             ? ToPrintableStringImpl(
                   cx, mozilla::Range(atom->latin1Chars(), length), '\"')
             : ToPrintableStringImpl(
                   cx, mozilla::Range(atom->twoByteChars(), length), '\"');
}

JSAtom* ParserAtomsTable::toJSAtom(JSContext* cx, TaggedParserAtomIndex index,
                                   CompilationAtomCache& atomCache) const {
  const auto* atom = getParserAtom(index);
  return atom->toJSAtom(cx, index, atomCache);
}

bool InstantiateMarkedAtoms(JSContext* cx, const ParserAtomSpan& entries,
                            CompilationAtomCache& atomCache) {
  for (size_t i = 0; i < entries.size(); i++) {
    const auto& entry = entries[i];
    if (!entry) {
      continue;
    }
    if (entry->isUsedByStencil()) {
      auto index = ParserAtomIndex(i);
      if (!atomCache.hasAtomAt(index)) {
        if (!entry->instantiate(cx, TaggedParserAtomIndex(index), atomCache)) {
          return false;
        }
      }
    }
  }
  return true;
}

template <typename CharT>
TaggedParserAtomIndex WellKnownParserAtoms::lookupChar16Seq(
    const SpecificParserAtomLookup<CharT>& lookup) const {
  EntryMap::Ptr ptr = wellKnownMap_.readonlyThreadsafeLookup(lookup);
  if (ptr) {
    return ptr->value();
  }
  return TaggedParserAtomIndex::null();
}

bool WellKnownParserAtoms::initSingle(JSContext* cx, const ParserAtom** name,
                                      const ParserAtom& romEntry,
                                      TaggedParserAtomIndex index) {
  MOZ_ASSERT(name != nullptr);

  unsigned int len = romEntry.length();
  const Latin1Char* str = romEntry.latin1Chars();

  // Well-known atoms are all currently ASCII with length <= MaxWellKnownLength.
  MOZ_ASSERT(len <= MaxWellKnownLength);
  MOZ_ASSERT(romEntry.isAscii());

  // Strings matched by lookupTiny are stored in static table and aliases should
  // only be added using initTinyStringAlias.
  MOZ_ASSERT(lookupTinyIndex(str, len) == TaggedParserAtomIndex::null(),
             "Well-known atom matches a tiny StaticString. Did you add it to "
             "the wrong CommonPropertyNames.h list?");

  InflatedChar16Sequence<Latin1Char> seq(str, len);
  SpecificParserAtomLookup<Latin1Char> lookup(seq, romEntry.hash());

  // Save name for returning after moving entry into set.
  if (!wellKnownMap_.putNew(lookup, &romEntry, index)) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  *name = &romEntry;
  return true;
}

bool WellKnownParserAtoms::initTinyStringAlias(JSContext* cx,
                                               const ParserAtom** name,
                                               const char* str) {
  MOZ_ASSERT(name != nullptr);

  unsigned int len = strlen(str);

  // Well-known atoms are all currently ASCII with length <= MaxWellKnownLength.
  MOZ_ASSERT(len <= MaxWellKnownLength);
  MOZ_ASSERT(FindSmallestEncoding(JS::UTF8Chars(str, len)) ==
             JS::SmallestEncoding::ASCII);

  // NOTE: If this assert fails, you may need to change which list is it belongs
  //       to in CommonPropertyNames.h.
  auto tiny = lookupTiny(str, len);
  MOZ_ASSERT(tiny, "Tiny common name was not found");

  // Set alias to existing atom.
  *name = tiny;
  return true;
}

bool WellKnownParserAtoms::init(JSContext* cx) {
  // Tiny strings with a common name need a named alias to an entry in the
  // WellKnownParserAtoms_ROM.
#define COMMON_NAME_INIT_(_, NAME, TEXT)         \
  if (!initTinyStringAlias(cx, &(NAME), TEXT)) { \
    return false;                                \
  }
  FOR_EACH_TINY_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_

  // Initialize the named fields to point to entries in the ROM. This also adds
  // the atom to the lookup HashMap. The HashMap is used for dynamic lookups
  // later and does not change once this init method is complete.
#define COMMON_NAME_INIT_(_, NAME, _2)                         \
  if (!initSingle(cx, &(NAME), rom_.NAME,                      \
                  TaggedParserAtomIndex::WellKnown::NAME())) { \
    return false;                                              \
  }
  FOR_EACH_NONTINY_COMMON_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_
#define COMMON_NAME_INIT_(NAME, _)                             \
  if (!initSingle(cx, &(NAME), rom_.NAME,                      \
                  TaggedParserAtomIndex::WellKnown::NAME())) { \
    return false;                                              \
  }
  JS_FOR_EACH_PROTOTYPE(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_

  return true;
}

} /* namespace frontend */
} /* namespace js */

// XDR code.
namespace js {

template <XDRMode mode>
XDRResult XDRParserAtom(XDRState<mode>* xdr, ParserAtom** atomp) {
  static_assert(CanCopyDataToDisk<ParserAtom>::value,
                "ParserAtom cannot be bulk-copied to disk.");

  MOZ_TRY(xdr->align32());

  const ParserAtom* header;
  if (mode == XDR_ENCODE) {
    header = *atomp;
  } else {
    MOZ_TRY(xdr->peekData(&header));
  }

  const uint32_t CharSize =
      header->hasLatin1Chars() ? sizeof(JS::Latin1Char) : sizeof(char16_t);
  uint32_t totalLength = sizeof(ParserAtom) + (CharSize * header->length());

  MOZ_TRY(xdr->borrowedData(atomp, totalLength));

  return Ok();
}

template XDRResult XDRParserAtom(XDRState<XDR_ENCODE>* xdr, ParserAtom** atomp);
template XDRResult XDRParserAtom(XDRState<XDR_DECODE>* xdr, ParserAtom** atomp);

} /* namespace js */

bool JSRuntime::initializeParserAtoms(JSContext* cx) {
  MOZ_ASSERT(!commonParserNames);

  if (parentRuntime) {
    commonParserNames = parentRuntime->commonParserNames;
    return true;
  }

  UniquePtr<js::frontend::WellKnownParserAtoms> names(
      js_new<js::frontend::WellKnownParserAtoms>());
  if (!names || !names->init(cx)) {
    return false;
  }

  commonParserNames = names.release();
  return true;
}

void JSRuntime::finishParserAtoms() {
  if (!parentRuntime) {
    js_delete(commonParserNames.ref());
  }
}
