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

static JS::OOM PARSER_ATOMS_OOM;

static JSAtom* GetWellKnownAtom(JSContext* cx, WellKnownAtomId atomId) {
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

mozilla::GenericErrorResult<OOM> RaiseParserAtomsOOMError(JSContext* cx) {
  js::ReportOutOfMemory(cx);
  return mozilla::Err(PARSER_ATOMS_OOM);
}

template <typename CharT, typename SeqCharT>
/* static */ JS::Result<UniquePtr<ParserAtomEntry>, OOM>
ParserAtomEntry::allocate(JSContext* cx, InflatedChar16Sequence<SeqCharT> seq,
                          uint32_t length, HashNumber hash) {
  constexpr size_t HeaderSize = sizeof(ParserAtomEntry);
  uint8_t* raw = cx->pod_malloc<uint8_t>(HeaderSize + (sizeof(CharT) * length));
  if (!raw) {
    return RaiseParserAtomsOOMError(cx);
  }

  constexpr bool hasTwoByteChars = (sizeof(CharT) == 2);
  static_assert(sizeof(CharT) == 1 || sizeof(CharT) == 2,
                "CharT should be 1 or 2 byte type");
  UniquePtr<ParserAtomEntry> entry(
      new (raw) ParserAtomEntry(length, hash, hasTwoByteChars));
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
  switch (atomIndexKind_) {
    case AtomIndexKind::AtomIndex:
      return atomCache.atoms[atomIndex_];

    case AtomIndexKind::WellKnown:
      return GetWellKnownAtom(cx, WellKnownAtomId(atomIndex_));

    case AtomIndexKind::Static1: {
      char16_t ch = static_cast<char16_t>(atomIndex_);
      return cx->staticStrings().getUnit(ch);
    }

    case AtomIndexKind::Static2:
      return cx->staticStrings().getLength2FromIndex(atomIndex_);

    case AtomIndexKind::NotInstantiatedAndNotMarked:
    case AtomIndexKind::NotInstantiatedAndMarked:
      // NOTE: toJSAtom can be called on not-marked atom, outside of
      //       stencil instantiation.
      break;
  }

  return instantiate(cx, atomCache);
}

JSAtom* ParserAtomEntry::toExistingJSAtom(
    JSContext* cx, CompilationAtomCache& atomCache) const {
  switch (atomIndexKind_) {
    case AtomIndexKind::AtomIndex:
      return atomCache.atoms[atomIndex_];

    case AtomIndexKind::WellKnown:
      return GetWellKnownAtom(cx, WellKnownAtomId(atomIndex_));

    case AtomIndexKind::Static1: {
      char16_t ch = static_cast<char16_t>(atomIndex_);
      return cx->staticStrings().getUnit(ch);
    }

    case AtomIndexKind::Static2:
      return cx->staticStrings().getLength2FromIndex(atomIndex_);

    case AtomIndexKind::NotInstantiatedAndNotMarked:
    case AtomIndexKind::NotInstantiatedAndMarked:
      MOZ_CRASH("ParserAtom should already be instantiatedd");
  }

  return nullptr;
}

JSAtom* ParserAtomEntry::instantiate(JSContext* cx,
                                     CompilationAtomCache& atomCache) const {
  // NOTE: toJSAtom can be called on not-marked atom, outside of
  //       stencil instantiation.
  MOZ_ASSERT(isNotInstantiatedAndNotMarked() || isNotInstantiatedAndMarked());

  JSAtom* atom;
  if (hasLatin1Chars()) {
    atom = AtomizeChars(cx, latin1Chars(), length());
  } else {
    atom = AtomizeChars(cx, twoByteChars(), length());
  }
  if (!atom) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }
  auto index = atomCache.atoms.length();

  // This cannot be infallibleAppend because there are toJSAtom consumers that
  // doesn't reserve CompilationAtomCache.atoms beforehand
  if (!atomCache.atoms.append(atom)) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }

  const_cast<ParserAtomEntry*>(this)->setAtomIndex(AtomIndex(index));

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

ParserAtomsTable::ParserAtomsTable(JSRuntime* rt)
    : wellKnownTable_(*rt->commonParserNames) {}

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::addEntry(
    JSContext* cx, EntrySet::AddPtr& addPtr, UniquePtr<ParserAtomEntry> entry) {
  ParserAtomEntry* entryPtr = entry.get();
  MOZ_ASSERT(!addPtr);
  if (!entrySet_.add(addPtr, std::move(entry))) {
    return RaiseParserAtomsOOMError(cx);
  }
  return entryPtr->asAtom();
}

template <typename AtomCharT, typename SeqCharT>
JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internChar16Seq(
    JSContext* cx, EntrySet::AddPtr& addPtr, HashNumber hash,
    InflatedChar16Sequence<SeqCharT> seq, uint32_t length) {
  MOZ_ASSERT(!addPtr);

  UniquePtr<ParserAtomEntry> entry;
  MOZ_TRY_VAR(entry,
              ParserAtomEntry::allocate<AtomCharT>(cx, seq, length, hash));
  return addEntry(cx, addPtr, std::move(entry));
}

static const uint16_t MAX_LATIN1_CHAR = 0xff;

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internAscii(
    JSContext* cx, const char* asciiPtr, uint32_t length) {
  // ASCII strings are strict subsets of Latin1 strings.
  const Latin1Char* latin1Ptr = reinterpret_cast<const Latin1Char*>(asciiPtr);
  return internLatin1(cx, latin1Ptr, length);
}

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internLatin1(
    JSContext* cx, const Latin1Char* latin1Ptr, uint32_t length) {
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

// For XDR we should only need to intern user strings so skip checks for tiny
// and well-known atoms.
JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internLatin1ForXDR(
    JSContext* cx, const Latin1Char* latin1Ptr, uint32_t length) {
  InflatedChar16Sequence<Latin1Char> seq(latin1Ptr, length);
  SpecificParserAtomLookup<Latin1Char> lookup(seq);

  auto addPtr = entrySet_.lookupForAdd(lookup);

  MOZ_ASSERT(wellKnownTable_.lookupTiny(latin1Ptr, length) == nullptr);
  MOZ_ASSERT(wellKnownTable_.lookupChar16Seq(lookup) == nullptr);
  MOZ_ASSERT(!addPtr);

  return internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq, length);
}

// For XDR
JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internChar16LE(
    JSContext* cx, LittleEndianChars twoByteLE, uint32_t length) {
  // Check for tiny strings which are abundant in minified code.
  if (const ParserAtom* tiny = wellKnownTable_.lookupTiny(twoByteLE, length)) {
    return tiny;
  }

  InflatedChar16Sequence<LittleEndianChars> seq(twoByteLE, length);

  // Check for well-known atom.
  SpecificParserAtomLookup<LittleEndianChars> lookup(seq);
  if (const ParserAtom* wk = wellKnownTable_.lookupChar16Seq(lookup)) {
    return wk;
  }

  // An XDR interning is guaranteed to be unique: there should be no
  // existing atom with the same contents, except for well-known atoms.
  EntrySet::AddPtr addPtr = entrySet_.lookupForAdd(lookup);
  MOZ_ASSERT(!addPtr);

  // Compute the target encoding.
  // NOTE: Length in code-points will be same, even if we deflate to Latin1.
  bool wide = false;
  InflatedChar16Sequence<LittleEndianChars> seqCopy = seq;
  while (seqCopy.hasMore()) {
    char16_t ch = seqCopy.next();
    if (ch > MAX_LATIN1_CHAR) {
      wide = true;
      break;
    }
  }

  // Add new entry.
  return wide
             ? internChar16Seq<char16_t>(cx, addPtr, lookup.hash(), seq, length)
             : internChar16Seq<Latin1Char>(cx, addPtr, lookup.hash(), seq,
                                           length);
}

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internUtf8(
    JSContext* cx, const mozilla::Utf8Unit* utf8Ptr, uint32_t nbyte) {
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

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internChar16(
    JSContext* cx, const char16_t* char16Ptr, uint32_t length) {
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

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::internJSAtom(
    JSContext* cx, CompilationInfo& compilationInfo, JSAtom* atom) {
  const ParserAtom* id;
  {
    JS::AutoCheckCannotGC nogc;

    auto result =
        atom->hasLatin1Chars()
            ? internLatin1(cx, atom->latin1Chars(nogc), atom->length())
            : internChar16(cx, atom->twoByteChars(nogc), atom->length());
    if (result.isErr()) {
      return result;
    }
    id = result.unwrap();
  }

  if (id->isNotInstantiatedAndNotMarked() || id->isNotInstantiatedAndMarked()) {
    MOZ_ASSERT(id->equalsJSAtom(atom));

    auto index = AtomIndex(compilationInfo.input.atomCache.atoms.length());
    if (!compilationInfo.input.atomCache.atoms.append(atom)) {
      return RaiseParserAtomsOOMError(cx);
    }

    const_cast<ParserAtom*>(id)->setAtomIndex(AtomIndex(index));
  }

  // We should (infallibly) map back to the same JSAtom.
  MOZ_ASSERT(id->toJSAtom(cx, compilationInfo.input.atomCache) == atom);

  return id;
}

JS::Result<const ParserAtom*, OOM> ParserAtomsTable::concatAtoms(
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
      return RaiseParserAtomsOOMError(cx);
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

const ParserAtom* ParserAtomsTable::getWellKnown(WellKnownAtomId atomId) const {
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

  return (&wellKnownTable_.abort)[int32_t(atomId)];
}

const ParserAtom* ParserAtomsTable::getStatic1(StaticParserString1 s) const {
  return WellKnownParserAtoms::rom_.length1Table[size_t(s)].asAtom();
}

const ParserAtom* ParserAtomsTable::getStatic2(StaticParserString2 s) const {
  return WellKnownParserAtoms::rom_.length2Table[size_t(s)].asAtom();
}

size_t ParserAtomsTable::requiredNonStaticAtomCount() const {
  size_t count = 0;
  for (auto iter = entrySet_.iter(); !iter.done(); iter.next()) {
    const auto& entry = iter.get();
    if (entry->isNotInstantiatedAndMarked() || entry->isAtomIndex()) {
      count++;
    }
  }
  return count;
}

bool ParserAtomsTable::instantiateMarkedAtoms(
    JSContext* cx, CompilationAtomCache& atomCache) const {
  for (auto iter = entrySet_.iter(); !iter.done(); iter.next()) {
    const auto& entry = iter.get();
    if (entry->isNotInstantiatedAndMarked()) {
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
    return get->get()->asAtom();
  }
  return nullptr;
}

bool WellKnownParserAtoms::initSingle(JSContext* cx, const ParserName** name,
                                      const char* str, WellKnownAtomId atomId) {
  MOZ_ASSERT(name != nullptr);

  unsigned int len = strlen(str);

  // Well-known atoms are all currently ASCII with length <= MaxWellKnownLength.
  MOZ_ASSERT(len <= MaxWellKnownLength);
  MOZ_ASSERT(FindSmallestEncoding(JS::UTF8Chars(str, len)) ==
             JS::SmallestEncoding::ASCII);

  // Strings matched by lookupTiny are stored in static table and aliases should
  // only be added using initTinyStringAlias.
  MOZ_ASSERT(lookupTiny(str, len) == nullptr,
             "Well-known atom matches a tiny StaticString. Did you add it to "
             "the wrong CommonPropertyNames.h list?");

  InflatedChar16Sequence<Latin1Char> seq(
      reinterpret_cast<const Latin1Char*>(str), len);
  SpecificParserAtomLookup<Latin1Char> lookup(seq);
  HashNumber hash = lookup.hash();

  auto maybeEntry = ParserAtomEntry::allocate<Latin1Char>(cx, seq, len, hash);
  if (maybeEntry.isErr()) {
    return false;
  }
  UniquePtr<ParserAtomEntry> entry = maybeEntry.unwrap();
  entry->setWellKnownAtomId(atomId);

  // Save name for returning after moving entry into set.
  const ParserName* nm = entry.get()->asName();
  if (!wellKnownSet_.putNew(lookup, std::move(entry))) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  *name = nm;
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
  // NOTE: Well-known tiny strings (with length <= 2) are stored in the
  // WellKnownParserAtoms_ROM table. This uses static constexpr initialization
  // so we don't need to do anything here.

  // Tiny strings with a common name need a named alias to an entry in the
  // WellKnownParserAtoms_ROM.
#define COMMON_NAME_INIT_(idpart, id, text)    \
  if (!initTinyStringAlias(cx, &(id), text)) { \
    return false;                              \
  }
  FOR_EACH_TINY_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_

  // Initialize well-known ParserAtoms that use hash set lookup. These also
  // point the compile-time names to the own atoms.
#define COMMON_NAME_INIT_(idpart, id, text)                \
  if (!initSingle(cx, &(id), text, WellKnownAtomId::id)) { \
    return false;                                          \
  }
  FOR_EACH_NONTINY_COMMON_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_
#define COMMON_NAME_INIT_(name, clasp)                          \
  if (!initSingle(cx, &(name), #name, WellKnownAtomId::name)) { \
    return false;                                               \
  }
  JS_FOR_EACH_PROTOTYPE(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_

  return true;
}

} /* namespace frontend */
} /* namespace js */

// XDR code.
namespace js {

enum class ParserAtomTag : uint32_t {
  Normal = 0,
  WellKnown,
  Static1,
  Static2,
};

template <XDRMode mode>
static XDRResult XDRParserAtomTaggedIndex(XDRState<mode>* xdr,
                                          ParserAtomTag* tag, uint32_t* index) {
  // We encode 2 bit (tag) + 32 bit (index) data in the following format:
  //
  // index = 0bAABB'CCDD'EEFF'GGHH'IIJJ'KKLL'MMNN'OOPP
  // tag = 0bTT
  //
  // if index < 0b0011'1111'1111'1111'1111'1111'1111'1111:
  //   single uint32_t = 0bBBCC'DDEE'FFGG'HHII'JJKK'LLMM'NNOO'PPTT
  // else:
  //   two uint32_t    = 0b1111'1111'1111'1111'1111'1111'1111'11TT,
  //                     0bAABB'CCDD'EEFF'GGHH'IIJJ'KKLL'MMNN'OOPP

  constexpr uint32_t TagShift = 2;
  constexpr uint32_t TagMask = 0b0011;
  constexpr uint32_t TwoUnitPattern = UINT32_MAX ^ TagMask;
  constexpr uint32_t CodeLimit = TwoUnitPattern >> TagShift;

  MOZ_ASSERT((uint32_t(*tag) & TagMask) == uint32_t(*tag));

  if (mode == XDR_ENCODE) {
    if (*index < CodeLimit) {
      uint32_t data = (*index) << TagShift | uint32_t(*tag);
      return xdr->codeUint32(&data);
    }

    uint32_t data = TwoUnitPattern | uint32_t(*tag);
    MOZ_TRY(xdr->codeUint32(&data));
    return xdr->codeUint32(index);
  }

  MOZ_ASSERT(mode == XDR_DECODE);

  uint32_t data;
  MOZ_TRY(xdr->codeUint32(&data));

  *tag = ParserAtomTag(data & TagMask);

  if ((data & TwoUnitPattern) == TwoUnitPattern) {
    MOZ_TRY(xdr->codeUint32(index));
  } else {
    *index = data >> TagShift;
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRParserAtomData(XDRState<mode>* xdr, const ParserAtom** atomp) {
  static_assert(JSString::MAX_LENGTH <= INT32_MAX,
                "String length must fit in 31 bits");

  bool latin1 = false;
  uint32_t length = 0;
  uint32_t lengthAndEncoding = 0;

  /* Encode/decode the length and string-data encoding (Latin1 or TwoByte). */

  if (mode == XDR_ENCODE) {
    latin1 = (*atomp)->hasLatin1Chars();
    length = (*atomp)->length();
    lengthAndEncoding = (length << 1) | uint32_t(latin1);
  }

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
  JS::Result<const ParserAtom*, JS::OOM> mbAtom(nullptr);
  if (latin1) {
    const Latin1Char* chars = nullptr;
    if (length) {
      const uint8_t* ptr = nullptr;
      MOZ_TRY(xdr->peekData(&ptr, length * sizeof(Latin1Char)));
      chars = reinterpret_cast<const Latin1Char*>(ptr);
    }
    mbAtom = xdr->frontendAtoms().internLatin1ForXDR(cx, chars, length);
  } else {
    const uint8_t* twoByteCharsLE = nullptr;
    if (length) {
      MOZ_TRY(xdr->peekData(&twoByteCharsLE, length * sizeof(char16_t)));
    }
    LittleEndianChars leTwoByte(twoByteCharsLE);
    mbAtom = xdr->frontendAtoms().internChar16LE(cx, leTwoByte, length);
  }

  const ParserAtom* atom = mbAtom.unwrapOr(nullptr);
  if (!atom) {
    return xdr->fail(JS::TranscodeResult_Throw);
  }

  // We only transcoded ParserAtoms used for Stencils so on decode, all
  // ParserAtoms should be marked as in-use by Stencil.
  atom->markUsedByStencil();

  *atomp = atom;
  return Ok();
}

template XDRResult XDRParserAtomData(XDRState<XDR_ENCODE>* xdr,
                                     const ParserAtom** atomp);
template XDRResult XDRParserAtomData(XDRState<XDR_DECODE>* xdr,
                                     const ParserAtom** atomp);

template <XDRMode mode>
XDRResult XDRParserAtom(XDRState<mode>* xdr, const ParserAtom** atomp) {
  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(xdr->hasAtomMap());

    uint32_t atomIndex;
    ParserAtomTag tag = ParserAtomTag::Normal;
    if ((*atomp)->isWellKnownAtomId()) {
      atomIndex = uint32_t((*atomp)->toWellKnownAtomId());
      tag = ParserAtomTag::WellKnown;
    } else if ((*atomp)->isStaticParserString1()) {
      atomIndex = uint32_t((*atomp)->toStaticParserString1());
      tag = ParserAtomTag::Static1;
    } else if ((*atomp)->isStaticParserString2()) {
      atomIndex = uint32_t((*atomp)->toStaticParserString2());
      tag = ParserAtomTag::Static2;
    } else {
      // Either AtomIndexKind::NotInstantiated* or AtomIndexKind::AtomIndex.

      // Atom contents are encoded in a separate buffer, which is joined to the
      // final result in XDRIncrementalEncoder::linearize. References to atoms
      // are encoded as indices into the atom stream.
      XDRParserAtomMap::AddPtr p = xdr->parserAtomMap().lookupForAdd(*atomp);
      if (p) {
        atomIndex = p->value();
      } else {
        xdr->switchToAtomBuf();
        MOZ_TRY(XDRParserAtomData(xdr, atomp));
        xdr->switchToMainBuf();

        atomIndex = xdr->natoms();
        xdr->natoms() += 1;
        if (!xdr->parserAtomMap().add(p, *atomp, atomIndex)) {
          return xdr->fail(JS::TranscodeResult_Throw);
        }
      }
      tag = ParserAtomTag::Normal;
    }
    MOZ_TRY(XDRParserAtomTaggedIndex(xdr, &tag, &atomIndex));
    return Ok();
  }

  MOZ_ASSERT(mode == XDR_DECODE && xdr->hasAtomTable());

  uint32_t atomIndex = 0;
  ParserAtomTag tag = ParserAtomTag::Normal;
  MOZ_TRY(XDRParserAtomTaggedIndex(xdr, &tag, &atomIndex));

  switch (tag) {
    case ParserAtomTag::Normal:
      if (atomIndex >= xdr->parserAtomTable().length()) {
        return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
      }
      *atomp = xdr->parserAtomTable()[atomIndex];
      break;
    case ParserAtomTag::WellKnown:
      if (atomIndex >= uint32_t(WellKnownAtomId::Limit)) {
        return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
      }
      *atomp = xdr->frontendAtoms().getWellKnown(WellKnownAtomId(atomIndex));
      break;
    case ParserAtomTag::Static1:
      if (atomIndex >= WellKnownParserAtoms_ROM::ASCII_STATIC_LIMIT) {
        return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
      }
      *atomp = xdr->frontendAtoms().getStatic1(StaticParserString1(atomIndex));
      break;
    case ParserAtomTag::Static2:
      if (atomIndex >= WellKnownParserAtoms_ROM::NUM_LENGTH2_ENTRIES) {
        return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
      }
      *atomp = xdr->frontendAtoms().getStatic2(StaticParserString2(atomIndex));
      break;
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
