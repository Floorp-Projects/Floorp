/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParserAtom.h"

#include <type_traits>

#include "jsnum.h"

#include "frontend/CompilationInfo.h"
#include "frontend/NameCollections.h"
#include "vm/JSContext.h"
#include "vm/Printer.h"
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
#define ASSERT_OFFSET_(idpart, id, text)       \
  static_assert(offsetof(JSAtomState, id) ==   \
                int32_t(WellKnownAtomId::id) * \
                    sizeof(js::ImmutablePropertyNamePtr));
  FOR_EACH_COMMON_PROPERTYNAME(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

#define ASSERT_OFFSET_(name, clasp)              \
  static_assert(offsetof(JSAtomState, name) ==   \
                int32_t(WellKnownAtomId::name) * \
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
/* static */ ParserAtomEntry* ParserAtomEntry::allocate(
    JSContext* cx, LifoAlloc& alloc, InflatedChar16Sequence<SeqCharT> seq,
    uint32_t length, HashNumber hash) {
  constexpr size_t HeaderSize = sizeof(ParserAtomEntry);
  void* raw = alloc.alloc(HeaderSize + (sizeof(CharT) * length));
  if (!raw) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }

  constexpr bool hasTwoByteChars = (sizeof(CharT) == 2);
  static_assert(sizeof(CharT) == 1 || sizeof(CharT) == 2,
                "CharT should be 1 or 2 byte type");
  ParserAtomEntry* entry =
      new (raw) ParserAtomEntry(length, hash, hasTwoByteChars);
  CharT* entryBuf = entry->chars<CharT>();
  drainChar16Seq(entryBuf, seq, length);
  return entry;
}

bool ParserAtomEntry::equalsJSAtom(JSAtom* other) const {
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

template <typename CharT>
UniqueChars ToPrintableStringImpl(JSContext* cx, mozilla::Range<CharT> str) {
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return nullptr;
  }
  if (!QuoteString<QuoteTarget::String>(&sprinter, str)) {
    return nullptr;
  }
  return sprinter.release();
}

UniqueChars ParserAtomToPrintableString(JSContext* cx, const ParserAtom* atom) {
  size_t length = atom->length();

  return atom->hasLatin1Chars()
             ? ToPrintableStringImpl(
                   cx, mozilla::Range(atom->latin1Chars(), length))
             : ToPrintableStringImpl(
                   cx, mozilla::Range(atom->twoByteChars(), length));
}

bool ParserAtomEntry::isIndex(uint32_t* indexp) const {
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

JSAtom* ParserAtomEntry::toJSAtom(JSContext* cx,
                                  CompilationAtomCache& atomCache) const {
  if (isParserAtomIndex()) {
    JSAtom* atom = atomCache.getAtomAt(toParserAtomIndex());
    if (atom) {
      return atom;
    }

    return instantiate(cx, atomCache);
  }

  if (isWellKnownAtomId()) {
    return GetWellKnownAtom(cx, toWellKnownAtomId());
  }

  if (isStaticParserString1()) {
    char16_t ch = static_cast<char16_t>(toStaticParserString1());
    return cx->staticStrings().getUnit(ch);
  }

  MOZ_ASSERT(isStaticParserString2());
  size_t s = static_cast<size_t>(toStaticParserString2());
  return cx->staticStrings().getLength2FromIndex(s);
}

JSAtom* ParserAtomEntry::toExistingJSAtom(
    JSContext* cx, CompilationAtomCache& atomCache) const {
  if (isParserAtomIndex()) {
    JSAtom* atom = atomCache.getExistingAtomAt(toParserAtomIndex());
    MOZ_ASSERT(atom);
    return atom;
  }

  if (isWellKnownAtomId()) {
    return GetWellKnownAtom(cx, toWellKnownAtomId());
  }

  if (isStaticParserString1()) {
    char16_t ch = static_cast<char16_t>(toStaticParserString1());
    return cx->staticStrings().getUnit(ch);
  }

  MOZ_ASSERT(isStaticParserString2());
  size_t s = static_cast<size_t>(toStaticParserString2());
  return cx->staticStrings().getLength2FromIndex(s);
}

JSAtom* ParserAtomEntry::instantiate(JSContext* cx,
                                     CompilationAtomCache& atomCache) const {
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
  if (!atomCache.setAtomAt(cx, toParserAtomIndex(), atom)) {
    return nullptr;
  }

  return atom;
}

bool ParserAtomEntry::toNumber(JSContext* cx, double* result) const {
  return hasLatin1Chars() ? CharsToNumber(cx, latin1Chars(), length(), result)
                          : CharsToNumber(cx, twoByteChars(), length(), result);
}

#if defined(DEBUG) || defined(JS_JITSPEW)
void ParserAtomEntry::dump() const {
  js::Fprinter out(stderr);
  out.put("\"");
  dumpCharsNoQuote(out);
  out.put("\"\n");
}

void ParserAtomEntry::dumpCharsNoQuote(js::GenericPrinter& out) const {
  if (hasLatin1Chars()) {
    JSString::dumpCharsNoQuote<Latin1Char>(latin1Chars(), length(), out);
  } else {
    JSString::dumpCharsNoQuote<char16_t>(twoByteChars(), length(), out);
  }
}
#endif

ParserAtomsTable::ParserAtomsTable(JSRuntime* rt, LifoAlloc& alloc,
                                   ParserAtomVector& entries)
    : wellKnownTable_(*rt->commonParserNames),
      alloc_(alloc),
      entries_(entries) {}

const ParserAtom* ParserAtomsTable::addEntry(JSContext* cx,
                                             EntrySet::AddPtr& addPtr,
                                             ParserAtomEntry* entry) {
  MOZ_ASSERT(!addPtr);
  ParserAtomIndex index = ParserAtomIndex(entries_.length());
  if (size_t(index) >= TaggedParserAtomIndex::IndexLimit) {
    ReportAllocationOverflow(cx);
    return nullptr;
  }
  if (!entries_.append(entry)) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }
  entry->setParserAtomIndex(index);
  if (!entrySet_.add(addPtr, entry)) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }
  return entry->asAtom();
}

template <typename AtomCharT, typename SeqCharT>
const ParserAtom* ParserAtomsTable::internChar16Seq(
    JSContext* cx, EntrySet::AddPtr& addPtr, HashNumber hash,
    InflatedChar16Sequence<SeqCharT> seq, uint32_t length) {
  MOZ_ASSERT(!addPtr);

  ParserAtomEntry* entry =
      ParserAtomEntry::allocate<AtomCharT>(cx, alloc_, seq, length, hash);
  if (!entry) {
    return nullptr;
  }
  return addEntry(cx, addPtr, entry);
}

static const uint16_t MAX_LATIN1_CHAR = 0xff;

const ParserAtom* ParserAtomsTable::internAscii(JSContext* cx,
                                                const char* asciiPtr,
                                                uint32_t length) {
  // ASCII strings are strict subsets of Latin1 strings.
  const Latin1Char* latin1Ptr = reinterpret_cast<const Latin1Char*>(asciiPtr);
  return internLatin1(cx, latin1Ptr, length);
}

const ParserAtom* ParserAtomsTable::internLatin1(JSContext* cx,
                                                 const Latin1Char* latin1Ptr,
                                                 uint32_t length) {
  // Check for tiny strings which are abundant in minified code.
  if (const ParserAtom* tiny = wellKnownTable_.lookupTiny(latin1Ptr, length)) {
    return tiny;
  }

  // Check for well-known atom.
  InflatedChar16Sequence<Latin1Char> seq(latin1Ptr, length);
  SpecificParserAtomLookup<Latin1Char> lookup(seq);
  if (const ParserAtom* wk = wellKnownTable_.lookupChar16Seq(lookup)) {
    return wk;
  }

  // Check for existing atom.
  auto addPtr = entrySet_.lookupForAdd(lookup);
  if (addPtr) {
    return (*addPtr)->asAtom();
  }

  return internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq, length);
}

ParserAtomVectorBuilder::ParserAtomVectorBuilder(JSRuntime* rt,
                                                 LifoAlloc& alloc,
                                                 ParserAtomVector& entries)
    : wellKnownTable_(*rt->commonParserNames),
      alloc_(&alloc),
      entries_(entries) {}

bool ParserAtomVectorBuilder::resize(JSContext* cx, size_t count) {
  if (count >= TaggedParserAtomIndex::IndexLimit) {
    ReportAllocationOverflow(cx);
    return false;
  }
  if (!entries_.resize(count)) {
    ReportOutOfMemory(cx);
    return false;
  }
  return true;
}

const ParserAtom* ParserAtomVectorBuilder::internLatin1At(
    JSContext* cx, const Latin1Char* latin1Ptr, HashNumber hash,
    uint32_t length, ParserAtomIndex index) {
  return internAt<Latin1Char, Latin1Char>(cx, latin1Ptr, hash, length, index);
}

const ParserAtom* ParserAtomVectorBuilder::internChar16At(
    JSContext* cx, LittleEndianChars twoByteLE, HashNumber hash,
    uint32_t length, ParserAtomIndex index) {
#ifdef DEBUG
  InflatedChar16Sequence<LittleEndianChars> seq(twoByteLE, length);
  bool wide = false;
  while (seq.hasMore()) {
    char16_t ch = seq.next();
    if (ch > MAX_LATIN1_CHAR) {
      wide = true;
      break;
    }
  }
  MOZ_ASSERT(wide);
#endif

  return internAt<char16_t, LittleEndianChars>(cx, twoByteLE, hash, length,
                                               index);
}

template <typename CharT, typename SeqCharT, typename InputCharsT>
const ParserAtom* ParserAtomVectorBuilder::internAt(JSContext* cx,
                                                    InputCharsT chars,
                                                    HashNumber hash,
                                                    uint32_t length,
                                                    ParserAtomIndex index) {
  InflatedChar16Sequence<SeqCharT> seq(chars, length);

#ifdef DEBUG
  SpecificParserAtomLookup<SeqCharT> lookup(seq);

  MOZ_ASSERT(wellKnownTable_.lookupTiny(chars, length) == nullptr);
  MOZ_ASSERT(wellKnownTable_.lookupChar16Seq(lookup) == nullptr);
#endif

  ParserAtomEntry* entry =
      ParserAtomEntry::allocate<CharT>(cx, *alloc_, seq, length, hash);
  if (!entry) {
    return nullptr;
  }

  entry->setParserAtomIndex(index);
  entries_[index] = entry;

  return entry->asAtom();
}

const ParserAtom* ParserAtomsTable::internUtf8(JSContext* cx,
                                               const mozilla::Utf8Unit* utf8Ptr,
                                               uint32_t nbyte) {
  // Check for tiny strings which are abundant in minified code.
  // NOTE: The tiny atoms are all ASCII-only so we can directly look at the
  //        UTF-8 data without worrying about surrogates.
  if (const ParserAtom* tiny = wellKnownTable_.lookupTiny(
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
  MOZ_ASSERT(wellKnownTable_.lookupChar16Seq(lookup) == nullptr);
  EntrySet::AddPtr addPtr = entrySet_.lookupForAdd(lookup);
  if (addPtr) {
    return (*addPtr)->asAtom();
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

const ParserAtom* ParserAtomsTable::internChar16(JSContext* cx,
                                                 const char16_t* char16Ptr,
                                                 uint32_t length) {
  // Check for tiny strings which are abundant in minified code.
  if (const ParserAtom* tiny = wellKnownTable_.lookupTiny(char16Ptr, length)) {
    return tiny;
  }

  // Check against well-known.
  InflatedChar16Sequence<char16_t> seq(char16Ptr, length);
  SpecificParserAtomLookup<char16_t> lookup(seq);
  if (const ParserAtom* wk = wellKnownTable_.lookupChar16Seq(lookup)) {
    return wk;
  }

  // Check for existing atom.
  EntrySet::AddPtr addPtr = entrySet_.lookupForAdd(lookup);
  if (addPtr) {
    return (*addPtr)->asAtom();
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

const ParserAtom* ParserAtomsTable::internJSAtom(
    JSContext* cx, CompilationInfo& compilationInfo, JSAtom* atom) {
  const ParserAtom* parserAtom;
  {
    JS::AutoCheckCannotGC nogc;

    parserAtom =
        atom->hasLatin1Chars()
            ? internLatin1(cx, atom->latin1Chars(nogc), atom->length())
            : internChar16(cx, atom->twoByteChars(nogc), atom->length());
    if (!parserAtom) {
      return nullptr;
    }
  }

  if (parserAtom->isParserAtomIndex()) {
    ParserAtomIndex index = parserAtom->toParserAtomIndex();
    auto& atomCache = compilationInfo.input.atomCache;

    if (!atomCache.hasAtomAt(index)) {
      if (!atomCache.setAtomAt(cx, index, atom)) {
        return nullptr;
      }
    }
  }

  // We should (infallibly) map back to the same JSAtom.
  MOZ_ASSERT(parserAtom->toJSAtom(cx, compilationInfo.input.atomCache) == atom);

  return parserAtom;
}

const ParserAtom* ParserAtomsTable::concatAtoms(
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
    if (atom->length() >= (ParserAtomEntry::MAX_LENGTH - catLen)) {
      js::ReportOutOfMemory(cx);
      return nullptr;
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
  auto addPtr = entrySet_.lookupForAdd(lookup);
  if (addPtr) {
    return (*addPtr)->asAtom();
  }

  // Otherwise, add new entry.
  return catLatin1 ? internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq,
                                                 catLen)
                   : internChar16Seq<char16_t>(cx, addPtr, lookup.hash(), seq,
                                               catLen);
}

const ParserAtom* WellKnownParserAtoms::getWellKnown(
    WellKnownAtomId atomId) const {
#define ASSERT_OFFSET_(idpart, id, text)              \
  static_assert(offsetof(WellKnownParserAtoms, id) == \
                int32_t(WellKnownAtomId::id) * sizeof(ParserAtom*));
  FOR_EACH_COMMON_PROPERTYNAME(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

#define ASSERT_OFFSET_(name, clasp)                     \
  static_assert(offsetof(WellKnownParserAtoms, name) == \
                int32_t(WellKnownAtomId::name) * sizeof(ParserAtom*));
  JS_FOR_EACH_PROTOTYPE(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

  static_assert(int32_t(WellKnownAtomId::abort) == 0,
                "Unexpected order of WellKnownAtom");

  return (&abort)[int32_t(atomId)];
}

/* static */
const ParserAtom* WellKnownParserAtoms::getStatic1(StaticParserString1 s) {
  return WellKnownParserAtoms::rom_.length1Table[size_t(s)].asAtom();
}

/* static */
const ParserAtom* WellKnownParserAtoms::getStatic2(StaticParserString2 s) {
  return WellKnownParserAtoms::rom_.length2Table[size_t(s)].asAtom();
}

const ParserAtom* ParserAtomVectorBuilder::getWellKnown(
    WellKnownAtomId atomId) const {
  return wellKnownTable_.getWellKnown(atomId);
}

const ParserAtom* ParserAtomVectorBuilder::getStatic1(
    StaticParserString1 s) const {
  return WellKnownParserAtoms::getStatic1(s);
}

const ParserAtom* ParserAtomVectorBuilder::getStatic2(
    StaticParserString2 s) const {
  return WellKnownParserAtoms::getStatic2(s);
}

const ParserAtom* ParserAtomVectorBuilder::getParserAtom(
    ParserAtomIndex index) const {
  return entries_[index]->asAtom();
}

template <class T>
const ParserAtom* GetParserAtom(T self, TaggedParserAtomIndex index) {
  if (index.isParserAtomIndex()) {
    return self->getParserAtom(index.toParserAtomIndex());
  }

  if (index.isWellKnownAtomId()) {
    return self->getWellKnown(index.toWellKnownAtomId());
  }

  if (index.isStaticParserString1()) {
    return self->getStatic1(index.toStaticParserString1());
  }

  if (index.isStaticParserString2()) {
    return self->getStatic2(index.toStaticParserString2());
  }

  MOZ_ASSERT(index.isNull());
  return nullptr;
}

const ParserAtom* ParserAtomVectorBuilder::getParserAtom(
    TaggedParserAtomIndex index) const {
  return GetParserAtom(this, index);
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
  return entries_[index]->asAtom();
}

const ParserAtom* ParserAtomsTable::getParserAtom(
    TaggedParserAtomIndex index) const {
  return GetParserAtom(this, index);
}

bool InstantiateMarkedAtoms(JSContext* cx, const ParserAtomVector& entries,
                            CompilationAtomCache& atomCache) {
  for (const auto& entry : entries) {
    if (!entry) {
      continue;
    }
    if (entry->isUsedByStencil() && entry->isParserAtomIndex() &&
        !atomCache.hasAtomAt(entry->toParserAtomIndex())) {
      if (!entry->instantiate(cx, atomCache)) {
        return false;
      }
    }
  }
  return true;
}

template <typename CharT>
const ParserAtom* WellKnownParserAtoms::lookupChar16Seq(
    const SpecificParserAtomLookup<CharT>& lookup) const {
  EntrySet::Ptr get = wellKnownSet_.readonlyThreadsafeLookup(lookup);
  if (get) {
    return (*get)->asAtom();
  }
  return nullptr;
}

bool WellKnownParserAtoms::initSingle(JSContext* cx, const ParserName** name,
                                      const ParserAtomEntry& romEntry) {
  MOZ_ASSERT(name != nullptr);

  unsigned int len = romEntry.length();
  const Latin1Char* str = romEntry.latin1Chars();

  // Well-known atoms are all currently ASCII with length <= MaxWellKnownLength.
  MOZ_ASSERT(len <= MaxWellKnownLength);
  MOZ_ASSERT(romEntry.isAscii());

  // Strings matched by lookupTiny are stored in static table and aliases should
  // only be added using initTinyStringAlias.
  MOZ_ASSERT(lookupTiny(str, len) == nullptr,
             "Well-known atom matches a tiny StaticString. Did you add it to "
             "the wrong CommonPropertyNames.h list?");

  InflatedChar16Sequence<Latin1Char> seq(str, len);
  SpecificParserAtomLookup<Latin1Char> lookup(seq, romEntry.hash());

  // Save name for returning after moving entry into set.
  if (!wellKnownSet_.putNew(lookup, &romEntry)) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  *name = romEntry.asName();
  return true;
}

bool WellKnownParserAtoms::initTinyStringAlias(JSContext* cx,
                                               const ParserName** name,
                                               const char* str) {
  MOZ_ASSERT(name != nullptr);

  unsigned int len = strlen(str);

  // Well-known atoms are all currently ASCII with length <= MaxWellKnownLength.
  MOZ_ASSERT(len <= MaxWellKnownLength);
  MOZ_ASSERT(FindSmallestEncoding(JS::UTF8Chars(str, len)) ==
             JS::SmallestEncoding::ASCII);

  // NOTE: If this assert fails, you may need to change which list is it belongs
  //       to in CommonPropertyNames.h.
  const ParserAtom* tiny = lookupTiny(str, len);
  MOZ_ASSERT(tiny, "Tiny common name was not found");

  // Set alias to existing atom.
  *name = tiny->asName();
  return true;
}

bool WellKnownParserAtoms::init(JSContext* cx) {
  // Tiny strings with a common name need a named alias to an entry in the
  // WellKnownParserAtoms_ROM.
#define COMMON_NAME_INIT_(_, name, text)         \
  if (!initTinyStringAlias(cx, &(name), text)) { \
    return false;                                \
  }
  FOR_EACH_TINY_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_

  // Initialize the named fields to point to entries in the ROM. This also adds
  // the atom to the lookup HashSet. The HashSet is used for dynamic lookups
  // later and does not change once this init method is complete.
#define COMMON_NAME_INIT_(_, name, _2)       \
  if (!initSingle(cx, &(name), rom_.name)) { \
    return false;                            \
  }
  FOR_EACH_NONTINY_COMMON_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_
#define COMMON_NAME_INIT_(name, _)           \
  if (!initSingle(cx, &(name), rom_.name)) { \
    return false;                            \
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
XDRResult XDRTaggedParserAtomIndex(XDRState<mode>* xdr,
                                   TaggedParserAtomIndex* taggedIndex) {
  MOZ_TRY(xdr->codeUint32(taggedIndex->rawData()));

  if (mode == XDR_ENCODE) {
    return Ok();
  }

  if (taggedIndex->isParserAtomIndex()) {
    auto index = taggedIndex->toParserAtomIndex();
    if (size_t(index) >= xdr->frontendAtoms().length()) {
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }
    return Ok();
  }

  if (taggedIndex->isWellKnownAtomId()) {
    auto index = taggedIndex->toWellKnownAtomId();
    if (size_t(index) >= uint32_t(WellKnownAtomId::Limit)) {
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }
    return Ok();
  }

  if (taggedIndex->isStaticParserString1()) {
    auto index = taggedIndex->toStaticParserString1();
    if (size_t(index) >= WellKnownParserAtoms_ROM::ASCII_STATIC_LIMIT) {
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }
    return Ok();
  }

  if (taggedIndex->isStaticParserString2()) {
    auto index = taggedIndex->toStaticParserString2();
    if (size_t(index) >= WellKnownParserAtoms_ROM::NUM_LENGTH2_ENTRIES) {
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }
    return Ok();
  }

  return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
}

template XDRResult XDRTaggedParserAtomIndex(XDRState<XDR_ENCODE>* xdr,
                                            TaggedParserAtomIndex* taggedIndex);
template XDRResult XDRTaggedParserAtomIndex(XDRState<XDR_DECODE>* xdr,
                                            TaggedParserAtomIndex* taggedIndex);

template <XDRMode mode>
XDRResult XDRParserAtomDataAt(XDRState<mode>* xdr, const ParserAtom** atomp,
                              ParserAtomIndex index) {
  static_assert(JSString::MAX_LENGTH <= INT32_MAX,
                "String length must fit in 31 bits");

  uint32_t hash = 0;
  bool latin1 = false;
  uint32_t length = 0;
  uint32_t lengthAndEncoding = 0;

  /* Encode/decode the length and string-data encoding (Latin1 or TwoByte). */

  if (mode == XDR_ENCODE) {
    hash = (*atomp)->hash();
    latin1 = (*atomp)->hasLatin1Chars();
    length = (*atomp)->length();
    lengthAndEncoding = (length << 1) | uint32_t(latin1);
  }

  MOZ_TRY(xdr->codeUint32(&hash));
  MOZ_TRY(xdr->codeUint32(&lengthAndEncoding));

  if (mode == XDR_DECODE) {
    length = lengthAndEncoding >> 1;
    latin1 = !!(lengthAndEncoding & 0x1);
  }

  /* Encode the character data. */
  if (mode == XDR_ENCODE) {
    return latin1
               ? xdr->codeChars(
                     const_cast<JS::Latin1Char*>((*atomp)->latin1Chars()),
                     length)
               : xdr->codeChars(const_cast<char16_t*>((*atomp)->twoByteChars()),
                                length);
  }

  /* Decode the character data. */
  MOZ_ASSERT(mode == XDR_DECODE);
  JSContext* cx = xdr->cx();
  const ParserAtom* atom = nullptr;
  if (latin1) {
    const Latin1Char* chars = nullptr;
    if (length) {
      const uint8_t* ptr = nullptr;
      MOZ_TRY(xdr->peekData(&ptr, length * sizeof(Latin1Char)));
      chars = reinterpret_cast<const Latin1Char*>(ptr);
    }
    atom = xdr->frontendAtoms().internLatin1At(cx, chars, hash, length, index);
  } else {
    const uint8_t* twoByteCharsLE = nullptr;
    if (length) {
      MOZ_TRY(xdr->peekData(&twoByteCharsLE, length * sizeof(char16_t)));
    }
    LittleEndianChars leTwoByte(twoByteCharsLE);
    atom =
        xdr->frontendAtoms().internChar16At(cx, leTwoByte, hash, length, index);
  }
  if (!atom) {
    return xdr->fail(JS::TranscodeResult_Throw);
  }

  // We only transcoded ParserAtoms used for Stencils so on decode, all
  // ParserAtoms should be marked as in-use by Stencil.
  atom->markUsedByStencil();

  *atomp = atom;
  return Ok();
}

template XDRResult XDRParserAtomDataAt(XDRState<XDR_ENCODE>* xdr,
                                       const ParserAtom** atomp,
                                       ParserAtomIndex index);
template XDRResult XDRParserAtomDataAt(XDRState<XDR_DECODE>* xdr,
                                       const ParserAtom** atomp,
                                       ParserAtomIndex index);

template <XDRMode mode>
XDRResult XDRParserAtom(XDRState<mode>* xdr, const ParserAtom** atomp) {
  TaggedParserAtomIndex taggedIndex;
  if (mode == XDR_ENCODE) {
    taggedIndex = (*atomp)->toIndex();
  }
  MOZ_TRY(XDRTaggedParserAtomIndex(xdr, &taggedIndex));
  if (mode == XDR_DECODE) {
    MOZ_ASSERT(xdr->hasAtomTable());
    *atomp = xdr->frontendAtoms().getParserAtom(taggedIndex);
  }

  return Ok();
}

template XDRResult XDRParserAtom(XDRState<XDR_ENCODE>* xdr,
                                 const ParserAtom** atomp);

template XDRResult XDRParserAtom(XDRState<XDR_DECODE>* xdr,
                                 const ParserAtom** atomp);

template <XDRMode mode>
XDRResult XDRParserAtomOrNull(XDRState<mode>* xdr, const ParserAtom** atomp) {
  uint8_t isNull = false;
  if (mode == XDR_ENCODE) {
    if (!*atomp) {
      isNull = true;
    }
  }

  MOZ_TRY(xdr->codeUint8(&isNull));

  if (!isNull) {
    MOZ_TRY(XDRParserAtom(xdr, atomp));
  } else if (mode == XDR_DECODE) {
    *atomp = nullptr;
  }

  return Ok();
}

template XDRResult XDRParserAtomOrNull(XDRState<XDR_ENCODE>* xdr,
                                       const ParserAtom** atomp);

template XDRResult XDRParserAtomOrNull(XDRState<XDR_DECODE>* xdr,
                                       const ParserAtom** atomp);

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
