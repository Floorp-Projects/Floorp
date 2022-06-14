/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParserAtom.h"

#include "mozilla/TextUtils.h"  // mozilla::IsAscii

#include <memory>  // std::uninitialized_fill_n

#include "jsnum.h"  // CharsToNumber

#include "frontend/BytecodeCompiler.h"  // IsIdentifier
#include "frontend/CompilationStencil.h"
#include "util/StringBuffer.h"  // StringBuffer
#include "util/Text.h"          // AsciiDigitToNumber
#include "util/Unicode.h"
#include "vm/JSContext.h"
#include "vm/Printer.h"  // Sprinter, QuoteString
#include "vm/Runtime.h"
#include "vm/SelfHosting.h"  // ExtendedUnclonedSelfHostedFunctionNamePrefix
#include "vm/StaticStrings.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::frontend;

namespace js {
namespace frontend {

JSAtom* GetWellKnownAtom(JSContext* cx, WellKnownAtomId atomId) {
#define ASSERT_OFFSET_(_, NAME, _2)              \
  static_assert(offsetof(JSAtomState, NAME) ==   \
                int32_t(WellKnownAtomId::NAME) * \
                    sizeof(js::ImmutableTenuredPtr<PropertyName*>));
  FOR_EACH_COMMON_PROPERTYNAME(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

#define ASSERT_OFFSET_(NAME, _)                  \
  static_assert(offsetof(JSAtomState, NAME) ==   \
                int32_t(WellKnownAtomId::NAME) * \
                    sizeof(js::ImmutableTenuredPtr<PropertyName*>));
  JS_FOR_EACH_PROTOTYPE(ASSERT_OFFSET_);
#undef ASSERT_OFFSET_

#define ASSERT_OFFSET_(NAME)                     \
  static_assert(offsetof(JSAtomState, NAME) ==   \
                int32_t(WellKnownAtomId::NAME) * \
                    sizeof(js::ImmutableTenuredPtr<PropertyName*>));
  JS_FOR_EACH_WELL_KNOWN_SYMBOL(ASSERT_OFFSET_);
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
  } else if (isLength1StaticParserString()) {
    // always valid
  } else if (isLength2StaticParserString()) {
    MOZ_ASSERT(size_t(toLength2StaticParserString()) < Length2StaticLimit);
  } else if (isLength3StaticParserString()) {
    // always valid
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

bool ParserAtom::isInstantiatedAsJSAtom() const {
  if (isMarkedAtomize()) {
    return true;
  }

  // Always use JSAtom for short strings.
  if (length() < MinimumLengthForNonAtom) {
    return true;
  }

  return false;
}

JSString* ParserAtom::instantiateString(JSContext* cx, ParserAtomIndex index,
                                        CompilationAtomCache& atomCache) const {
  MOZ_ASSERT(!isInstantiatedAsJSAtom());

  JSString* str;
  if (hasLatin1Chars()) {
    str = NewStringCopyNDontDeflateNonStaticValidLength<CanGC>(
        cx, latin1Chars(), length(), gc::TenuredHeap);
  } else {
    str = NewStringCopyNDontDeflateNonStaticValidLength<CanGC>(
        cx, twoByteChars(), length(), gc::TenuredHeap);
  }
  if (!str) {
    return nullptr;
  }
  if (!atomCache.setAtomAt(cx, index, str)) {
    return nullptr;
  }

  return str;
}

JSAtom* ParserAtom::instantiateAtom(JSContext* cx, ParserAtomIndex index,
                                    CompilationAtomCache& atomCache) const {
  MOZ_ASSERT(isInstantiatedAsJSAtom());

  JSAtom* atom;
  if (hasLatin1Chars()) {
    atom =
        AtomizeCharsNonStaticValidLength(cx, hash(), latin1Chars(), length());
  } else {
    atom =
        AtomizeCharsNonStaticValidLength(cx, hash(), twoByteChars(), length());
  }
  if (!atom) {
    return nullptr;
  }
  if (!atomCache.setAtomAt(cx, index, atom)) {
    return nullptr;
  }
  return atom;
}

JSAtom* ParserAtom::instantiatePermanentAtom(
    JSContext* cx, AtomSet& atomSet, ParserAtomIndex index,
    CompilationAtomCache& atomCache) const {
  MOZ_ASSERT(!cx->zone());

  MOZ_ASSERT(hasLatin1Chars());
  MOZ_ASSERT(length() <= JSString::MAX_LENGTH);
  JSAtom* atom = PermanentlyAtomizeCharsNonStaticValidLength(
      cx, atomSet, hash(), latin1Chars(), length());
  if (!atom) {
    return nullptr;
  }
  if (!atomCache.setAtomAt(cx, index, atom)) {
    return nullptr;
  }
  return atom;
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

void ParserAtomsTable::dump(TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    getParserAtom(index.toParserAtomIndex())->dump();
    return;
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    js::Fprinter out(stderr);
    out.put("\"");
    out.put(info.content, info.length);
    out.put("\"");
    return;
  }

  if (index.isLength1StaticParserString()) {
    js::Fprinter out(stderr);
    out.put("\"");
    dumpCharsNoQuote(out, index.toLength1StaticParserString());
    out.put("\"\n");
    return;
  }

  if (index.isLength2StaticParserString()) {
    js::Fprinter out(stderr);
    out.put("\"");
    dumpCharsNoQuote(out, index.toLength2StaticParserString());
    out.put("\"\n");
    return;
  }

  if (index.isLength3StaticParserString()) {
    js::Fprinter out(stderr);
    out.put("\"");
    dumpCharsNoQuote(out, index.toLength3StaticParserString());
    out.put("\"\n");
    return;
  }

  MOZ_ASSERT(index.isNull());
  js::Fprinter out(stderr);
  out.put("#<null>");
}

void ParserAtomsTable::dumpCharsNoQuote(js::GenericPrinter& out,
                                        TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    getParserAtom(index.toParserAtomIndex())->dumpCharsNoQuote(out);
    return;
  }

  if (index.isWellKnownAtomId()) {
    dumpCharsNoQuote(out, index.toWellKnownAtomId());
    return;
  }

  if (index.isLength1StaticParserString()) {
    dumpCharsNoQuote(out, index.toLength1StaticParserString());
    return;
  }

  if (index.isLength2StaticParserString()) {
    dumpCharsNoQuote(out, index.toLength2StaticParserString());
    return;
  }

  if (index.isLength3StaticParserString()) {
    dumpCharsNoQuote(out, index.toLength3StaticParserString());
    return;
  }

  MOZ_ASSERT(index.isNull());
  out.put("#<null>");
}

/* static */
void ParserAtomsTable::dumpCharsNoQuote(js::GenericPrinter& out,
                                        WellKnownAtomId id) {
  const auto& info = GetWellKnownAtomInfo(id);
  out.put(info.content, info.length);
}

/* static */
void ParserAtomsTable::dumpCharsNoQuote(js::GenericPrinter& out,
                                        Length1StaticParserString index) {
  Latin1Char content[1];
  getLength1Content(index, content);
  out.putChar(content[0]);
}

/* static */
void ParserAtomsTable::dumpCharsNoQuote(js::GenericPrinter& out,
                                        Length2StaticParserString index) {
  char content[2];
  getLength2Content(index, content);
  out.putChar(content[0]);
  out.putChar(content[1]);
}

/* static */
void ParserAtomsTable::dumpCharsNoQuote(js::GenericPrinter& out,
                                        Length3StaticParserString index) {
  char content[3];
  getLength3Content(index, content);
  out.putChar(content[0]);
  out.putChar(content[1]);
  out.putChar(content[2]);
}
#endif

ParserAtomsTable::ParserAtomsTable(JSRuntime* rt, LifoAlloc& alloc)
    : wellKnownTable_(*rt->commonParserNames), alloc_(&alloc) {}

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
      ParserAtom::allocate<AtomCharT>(cx, *alloc_, seq, length, hash);
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

bool IsWide(const InflatedChar16Sequence<char16_t>& seq) {
  InflatedChar16Sequence<char16_t> seqCopy = seq;
  while (seqCopy.hasMore()) {
    char16_t ch = seqCopy.next();
    if (ch > MAX_LATIN1_CHAR) {
      return true;
    }
  }

  return false;
}

template <typename AtomCharT>
TaggedParserAtomIndex ParserAtomsTable::internExternalParserAtomImpl(
    JSContext* cx, const ParserAtom* atom) {
  InflatedChar16Sequence<AtomCharT> seq(atom->chars<AtomCharT>(),
                                        atom->length());
  SpecificParserAtomLookup<AtomCharT> lookup(seq, atom->hash());

  // Check for existing atom.
  auto addPtr = entryMap_.lookupForAdd(lookup);
  if (addPtr) {
    auto index = addPtr->value();

    // Copy UsedByStencilFlag and AtomizeFlag.
    MOZ_ASSERT(entries_[index.toParserAtomIndex()]->hasTwoByteChars() ==
               atom->hasTwoByteChars());
    entries_[index.toParserAtomIndex()]->flags_ |= atom->flags_;
    return index;
  }

  auto index =
      internChar16Seq<AtomCharT>(cx, addPtr, atom->hash(), seq, atom->length());
  if (!index) {
    return TaggedParserAtomIndex::null();
  }

  // Copy UsedByStencilFlag and AtomizeFlag.
  MOZ_ASSERT(entries_[index.toParserAtomIndex()]->hasTwoByteChars() ==
             atom->hasTwoByteChars());
  entries_[index.toParserAtomIndex()]->flags_ |= atom->flags_;
  return index;
}

TaggedParserAtomIndex ParserAtomsTable::internExternalParserAtom(
    JSContext* cx, const ParserAtom* atom) {
  if (atom->hasLatin1Chars()) {
    return internExternalParserAtomImpl<JS::Latin1Char>(cx, atom);
  }
  return internExternalParserAtomImpl<char16_t>(cx, atom);
}

bool ParserAtomsTable::addPlaceholder(JSContext* cx) {
  ParserAtomIndex index = ParserAtomIndex(entries_.length());
  if (size_t(index) >= TaggedParserAtomIndex::IndexLimit) {
    ReportAllocationOverflow(cx);
    return false;
  }
  if (!entries_.append(nullptr)) {
    js::ReportOutOfMemory(cx);
    return false;
  }
  return true;
}

TaggedParserAtomIndex ParserAtomsTable::internExternalParserAtomIndex(
    JSContext* cx, const CompilationStencil& context,
    TaggedParserAtomIndex atom) {
  // When the atom is not a parser atom index, the value represent the atom
  // without the need for a ParserAtom, and thus we can skip interning it.
  if (!atom.isParserAtomIndex()) {
    return atom;
  }
  auto index = atom.toParserAtomIndex();
  return internExternalParserAtom(cx, context.parserAtomData[index]);
}

bool ParserAtomsTable::isEqualToExternalParserAtomIndex(
    TaggedParserAtomIndex internal, const CompilationStencil& context,
    TaggedParserAtomIndex external) const {
  // If one is null, well-known or static, then testing the equality of the bits
  // of the TaggedParserAtomIndex is sufficient.
  if (!internal.isParserAtomIndex() || !external.isParserAtomIndex()) {
    return internal == external;
  }

  // Otherwise we have to compare 2 atom-indexes from different ParserAtomTable.
  ParserAtom* internalAtom = getParserAtom(internal.toParserAtomIndex());
  ParserAtom* externalAtom =
      context.parserAtomData[external.toParserAtomIndex()];

  if (internalAtom->hash() != externalAtom->hash()) {
    return false;
  }

  HashNumber hash = internalAtom->hash();
  size_t length = internalAtom->length();
  if (internalAtom->hasLatin1Chars()) {
    const Latin1Char* chars = internalAtom->latin1Chars();
    InflatedChar16Sequence<Latin1Char> seq(chars, length);
    return externalAtom->equalsSeq(hash, seq);
  }

  const char16_t* chars = internalAtom->twoByteChars();
  InflatedChar16Sequence<char16_t> seq(chars, length);
  return externalAtom->equalsSeq(hash, seq);
}

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

static inline bool IsLatin1(mozilla::Utf8Unit c1, mozilla::Utf8Unit c2) {
  auto u1 = c1.toUint8();
  auto u2 = c2.toUint8();

  // 0x80-0xBF
  if (u1 == 0xC2 && 0x80 <= u2 && u2 <= 0xBF) {
    return true;
  }

  // 0xC0-0xFF
  if (u1 == 0xC3 && 0x80 <= u2 && u2 <= 0xBF) {
    return true;
  }

  return false;
}

TaggedParserAtomIndex ParserAtomsTable::internUtf8(
    JSContext* cx, const mozilla::Utf8Unit* utf8Ptr, uint32_t nbyte) {
  if (auto tiny = wellKnownTable_.lookupTinyIndexUTF8(utf8Ptr, nbyte)) {
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
    (void)seqCopy.next();
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

  // Otherwise, add new entry.
  return IsWide(seq)
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
  MOZ_ASSERT(toJSAtom(cx, parserAtom, atomCache) == atom);

  return parserAtom;
}

ParserAtom* ParserAtomsTable::getParserAtom(ParserAtomIndex index) const {
  return entries_[index];
}

void ParserAtomsTable::markUsedByStencil(TaggedParserAtomIndex index,
                                         ParserAtom::Atomize atomize) const {
  if (!index.isParserAtomIndex()) {
    return;
  }

  getParserAtom(index.toParserAtomIndex())->markUsedByStencil(atomize);
}

void ParserAtomsTable::markAtomize(TaggedParserAtomIndex index,
                                   ParserAtom::Atomize atomize) const {
  if (!index.isParserAtomIndex()) {
    return;
  }

  getParserAtom(index.toParserAtomIndex())->markAtomize(atomize);
}

bool ParserAtomsTable::isIdentifier(TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    return atom->hasLatin1Chars()
               ? IsIdentifier(atom->latin1Chars(), atom->length())
               : IsIdentifier(atom->twoByteChars(), atom->length());
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return IsIdentifier(reinterpret_cast<const Latin1Char*>(info.content),
                        info.length);
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    if (MOZ_UNLIKELY(content[0] > 127)) {
      return IsIdentifier(content, 1);
    }
    return IsIdentifierASCII(char(content[0]));
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    return IsIdentifierASCII(content[0], content[1]);
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
#ifdef DEBUG
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  MOZ_ASSERT(!reinterpret_cast<const Latin1Char*>(
      IsIdentifier(reinterpret_cast<const Latin1Char*>(content), 3)));
#endif
  return false;
}

bool ParserAtomsTable::isPrivateName(TaggedParserAtomIndex index) const {
  if (!index.isParserAtomIndex()) {
    return false;
  }

  const auto* atom = getParserAtom(index.toParserAtomIndex());
  return atom->isPrivateName();
}

bool ParserAtomsTable::isExtendedUnclonedSelfHostedFunctionName(
    TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    if (atom->length() < 2) {
      return false;
    }

    return atom->charAt(0) == ExtendedUnclonedSelfHostedFunctionNamePrefix;
  }

  if (index.isWellKnownAtomId()) {
    switch (index.toWellKnownAtomId()) {
      case WellKnownAtomId::ArrayBufferSpecies:
      case WellKnownAtomId::ArraySpecies:
      case WellKnownAtomId::ArrayValues:
      case WellKnownAtomId::RegExpFlagsGetter:
      case WellKnownAtomId::RegExpToString: {
#ifdef DEBUG
        const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
        MOZ_ASSERT(info.content[0] ==
                   ExtendedUnclonedSelfHostedFunctionNamePrefix);
#endif
        return true;
      }
      default: {
#ifdef DEBUG
        const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
        MOZ_ASSERT(info.length == 0 ||
                   info.content[0] !=
                       ExtendedUnclonedSelfHostedFunctionNamePrefix);
#endif
        break;
      }
    }
    return false;
  }

  // Length-1/2/3 shouldn't be used for extented uncloned self-hosted
  // function name, and this query shouldn't be used for them.
#ifdef DEBUG
  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    MOZ_ASSERT(content[0] != ExtendedUnclonedSelfHostedFunctionNamePrefix);
  } else if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    MOZ_ASSERT(content[0] != ExtendedUnclonedSelfHostedFunctionNamePrefix);
  } else {
    MOZ_ASSERT(index.isLength3StaticParserString());
    char content[3];
    getLength3Content(index.toLength3StaticParserString(), content);
    MOZ_ASSERT(content[0] != ExtendedUnclonedSelfHostedFunctionNamePrefix);
  }
#endif
  return false;
}

static bool HasUnpairedSurrogate(mozilla::Range<const char16_t> chars) {
  for (auto ptr = chars.begin(); ptr < chars.end();) {
    char16_t ch = *ptr++;
    if (unicode::IsLeadSurrogate(ch)) {
      if (ptr == chars.end() || !unicode::IsTrailSurrogate(*ptr++)) {
        return true;
      }
    } else if (unicode::IsTrailSurrogate(ch)) {
      return true;
    }
  }
  return false;
}

bool ParserAtomsTable::isModuleExportName(TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const ParserAtom* name = getParserAtom(index.toParserAtomIndex());
    return name->hasLatin1Chars() ||
           !HasUnpairedSurrogate(name->twoByteRange());
  }

  // Well-known/length-2 are ASCII.
  // length-1 are Latin1.
  return true;
}

bool ParserAtomsTable::isIndex(TaggedParserAtomIndex index,
                               uint32_t* indexp) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    size_t len = atom->length();
    if (len == 0 || len > UINT32_CHAR_BUFFER_LENGTH) {
      return false;
    }
    if (atom->hasLatin1Chars()) {
      return mozilla::IsAsciiDigit(*atom->latin1Chars()) &&
             js::CheckStringIsIndex(atom->latin1Chars(), len, indexp);
    }
    return mozilla::IsAsciiDigit(*atom->twoByteChars()) &&
           js::CheckStringIsIndex(atom->twoByteChars(), len, indexp);
  }

  if (index.isWellKnownAtomId()) {
#ifdef DEBUG
    // Well-known atom shouldn't start with digit.
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    MOZ_ASSERT(info.length == 0 || !mozilla::IsAsciiDigit(info.content[0]));
#endif
    return false;
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    if (mozilla::IsAsciiDigit(content[0])) {
      *indexp = AsciiDigitToNumber(content[0]);
      return true;
    }
    return false;
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    // Leading '0' isn't allowed.
    // See CheckStringIsIndex comment.
    if (content[0] != '0' && mozilla::IsAsciiDigit(content[0]) &&
        mozilla::IsAsciiDigit(content[1])) {
      *indexp =
          AsciiDigitToNumber(content[0]) * 10 + AsciiDigitToNumber(content[1]);
      return true;
    }
    return false;
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  *indexp = uint32_t(index.toLength3StaticParserString());
#ifdef DEBUG
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  MOZ_ASSERT(uint32_t(AsciiDigitToNumber(content[0])) * 100 +
                 uint32_t(AsciiDigitToNumber(content[1])) * 10 +
                 uint32_t(AsciiDigitToNumber(content[2])) ==
             *indexp);
  MOZ_ASSERT(100 <= *indexp);
#endif
  return true;
}

bool ParserAtomsTable::isInstantiatedAsJSAtom(
    TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    return atom->isInstantiatedAsJSAtom();
  }

  // Everything else are always JSAtom.
  return true;
}

uint32_t ParserAtomsTable::length(TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    return getParserAtom(index.toParserAtomIndex())->length();
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return info.length;
  }

  if (index.isLength1StaticParserString()) {
    return 1;
  }

  if (index.isLength2StaticParserString()) {
    return 2;
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  return 3;
}

double ParserAtomsTable::toNumber(TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    size_t len = atom->length();
    return atom->hasLatin1Chars() ? CharsToNumber(atom->latin1Chars(), len)
                                  : CharsToNumber(atom->twoByteChars(), len);
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return CharsToNumber(reinterpret_cast<const Latin1Char*>(info.content),
                         info.length);
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    return CharsToNumber(content, 1);
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    return CharsToNumber(reinterpret_cast<const Latin1Char*>(content), 2);
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  double result = double(index.toLength3StaticParserString());
#ifdef DEBUG
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  double tmp = CharsToNumber(reinterpret_cast<const Latin1Char*>(content), 3);
  MOZ_ASSERT(tmp == result);
#endif
  return result;
}

UniqueChars ParserAtomsTable::toNewUTF8CharsZ(
    JSContext* cx, TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    return UniqueChars(
        atom->hasLatin1Chars()
            ? JS::CharsToNewUTF8CharsZ(cx, atom->latin1Range()).c_str()
            : JS::CharsToNewUTF8CharsZ(cx, atom->twoByteRange()).c_str());
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return UniqueChars(
        JS::CharsToNewUTF8CharsZ(
            cx,
            mozilla::Range(reinterpret_cast<const Latin1Char*>(info.content),
                           info.length))
            .c_str());
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    return UniqueChars(
        JS::CharsToNewUTF8CharsZ(cx, mozilla::Range(content, 1)).c_str());
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    return UniqueChars(
        JS::CharsToNewUTF8CharsZ(
            cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(content), 2))
            .c_str());
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  return UniqueChars(
      JS::CharsToNewUTF8CharsZ(
          cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(content), 3))
          .c_str());
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
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    return atom->hasLatin1Chars()
               ? ToPrintableStringImpl(cx, atom->latin1Range())
               : ToPrintableStringImpl(cx, atom->twoByteRange());
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return ToPrintableStringImpl(
        cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(info.content),
                           info.length));
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    return ToPrintableStringImpl(cx,
                                 mozilla::Range<const Latin1Char>(content, 1));
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    return ToPrintableStringImpl(
        cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(content), 2));
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  return ToPrintableStringImpl(
      cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(content), 3));
}

UniqueChars ParserAtomsTable::toQuotedString(
    JSContext* cx, TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    return atom->hasLatin1Chars()
               ? ToPrintableStringImpl(cx, atom->latin1Range(), '\"')
               : ToPrintableStringImpl(cx, atom->twoByteRange(), '\"');
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return ToPrintableStringImpl(
        cx,
        mozilla::Range(reinterpret_cast<const Latin1Char*>(info.content),
                       info.length),
        '\"');
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    return ToPrintableStringImpl(
        cx, mozilla::Range<const Latin1Char>(content, 1), '\"');
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    return ToPrintableStringImpl(
        cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(content), 2),
        '\"');
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  return ToPrintableStringImpl(
      cx, mozilla::Range(reinterpret_cast<const Latin1Char*>(content), 3),
      '\"');
}

JSAtom* ParserAtomsTable::toJSAtom(JSContext* cx, TaggedParserAtomIndex index,
                                   CompilationAtomCache& atomCache) const {
  // This function can be called before we instantiate atoms based on
  // AtomizeFlag.

  if (index.isParserAtomIndex()) {
    auto atomIndex = index.toParserAtomIndex();

    // If we already instantiated this parser atom, it should always be JSAtom.
    // `asAtom()` called in getAtomAt asserts that.
    JSAtom* atom = atomCache.getAtomAt(atomIndex);
    if (atom) {
      return atom;
    }

    // For consistency, mark atomize.
    ParserAtom* parserAtom = getParserAtom(atomIndex);
    parserAtom->markAtomize(ParserAtom::Atomize::Yes);
    return parserAtom->instantiateAtom(cx, atomIndex, atomCache);
  }

  if (index.isWellKnownAtomId()) {
    return GetWellKnownAtom(cx, index.toWellKnownAtomId());
  }

  if (index.isLength1StaticParserString()) {
    char16_t ch = static_cast<char16_t>(index.toLength1StaticParserString());
    return cx->staticStrings().getUnit(ch);
  }

  if (index.isLength2StaticParserString()) {
    size_t s = static_cast<size_t>(index.toLength2StaticParserString());
    return cx->staticStrings().getLength2FromIndex(s);
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  uint32_t s = uint32_t(index.toLength3StaticParserString());
  return cx->staticStrings().getUint(s);
}

bool ParserAtomsTable::appendTo(StringBuffer& buffer,
                                TaggedParserAtomIndex index) const {
  if (index.isParserAtomIndex()) {
    const auto* atom = getParserAtom(index.toParserAtomIndex());
    size_t length = atom->length();
    return atom->hasLatin1Chars() ? buffer.append(atom->latin1Chars(), length)
                                  : buffer.append(atom->twoByteChars(), length);
  }

  if (index.isWellKnownAtomId()) {
    const auto& info = GetWellKnownAtomInfo(index.toWellKnownAtomId());
    return buffer.append(info.content, info.length);
  }

  if (index.isLength1StaticParserString()) {
    Latin1Char content[1];
    getLength1Content(index.toLength1StaticParserString(), content);
    return buffer.append(content[0]);
  }

  if (index.isLength2StaticParserString()) {
    char content[2];
    getLength2Content(index.toLength2StaticParserString(), content);
    return buffer.append(content, 2);
  }

  MOZ_ASSERT(index.isLength3StaticParserString());
  char content[3];
  getLength3Content(index.toLength3StaticParserString(), content);
  return buffer.append(content, 3);
}

bool InstantiateMarkedAtoms(JSContext* cx, const ParserAtomSpan& entries,
                            CompilationAtomCache& atomCache) {
  MOZ_ASSERT(cx->zone());

  for (size_t i = 0; i < entries.size(); i++) {
    const auto& entry = entries[i];
    if (!entry) {
      continue;
    }
    if (!entry->isUsedByStencil()) {
      continue;
    }

    auto index = ParserAtomIndex(i);
    if (atomCache.hasAtomAt(index)) {
      continue;
    }

    if (!entry->isInstantiatedAsJSAtom()) {
      if (!entry->instantiateString(cx, index, atomCache)) {
        return false;
      }
    } else {
      if (!entry->instantiateAtom(cx, index, atomCache)) {
        return false;
      }
    }
  }
  return true;
}

bool InstantiateMarkedAtomsAsPermanent(JSContext* cx, AtomSet& atomSet,
                                       const ParserAtomSpan& entries,
                                       CompilationAtomCache& atomCache) {
  MOZ_ASSERT(!cx->zone());

  for (size_t i = 0; i < entries.size(); i++) {
    const auto& entry = entries[i];
    if (!entry) {
      continue;
    }
    if (!entry->isUsedByStencil()) {
      continue;
    }

    auto index = ParserAtomIndex(i);
    if (atomCache.hasAtomAt(index)) {
      MOZ_ASSERT(atomCache.getAtomAt(index)->isPermanentAtom());
      continue;
    }

    if (!entry->instantiatePermanentAtom(cx, atomSet, index, atomCache)) {
      return false;
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

TaggedParserAtomIndex WellKnownParserAtoms::lookupTinyIndexUTF8(
    const mozilla::Utf8Unit* utf8Ptr, size_t nbyte) const {
  // Check for tiny strings which are abundant in minified code.
  if (nbyte == 2 && IsLatin1(utf8Ptr[0], utf8Ptr[1])) {
    // Special case the length-1 non-ASCII range.
    InflatedChar16Sequence<mozilla::Utf8Unit> seq(utf8Ptr, 2);
    char16_t u = seq.next();
    const Latin1Char c = u;
    MOZ_ASSERT(!seq.hasMore());
    auto tiny = lookupTinyIndex(&c, 1);
    MOZ_ASSERT(tiny);
    return tiny;
  }

  // NOTE: Other than length-1 non-ASCII range, the tiny atoms are all
  //       ASCII-only so we can directly look at the UTF-8 data without
  //       worrying about surrogates.
  return lookupTinyIndex(reinterpret_cast<const Latin1Char*>(utf8Ptr), nbyte);
}

bool WellKnownParserAtoms::initSingle(JSContext* cx,
                                      const WellKnownAtomInfo& info,
                                      TaggedParserAtomIndex index) {
  unsigned int len = info.length;
  const Latin1Char* str = reinterpret_cast<const Latin1Char*>(info.content);

  // Well-known atoms are all currently ASCII with length <= MaxWellKnownLength.
  MOZ_ASSERT(len <= MaxWellKnownLength);
  MOZ_ASSERT(mozilla::IsAscii(mozilla::Span(info.content, len)));

  // Strings matched by lookupTinyIndex are stored in static table and aliases
  // should be initialized directly in WellKnownParserAtoms::init.
  MOZ_ASSERT(lookupTinyIndex(str, len) == TaggedParserAtomIndex::null(),
             "Well-known atom matches a tiny StaticString. Did you add it to "
             "the wrong CommonPropertyNames.h list?");

  InflatedChar16Sequence<Latin1Char> seq(str, len);
  SpecificParserAtomLookup<Latin1Char> lookup(seq, info.hash);

  // Save name for returning after moving entry into set.
  if (!wellKnownMap_.putNew(lookup, &info, index)) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool WellKnownParserAtoms::init(JSContext* cx) {
  // Add well-known strings to the HashMap. The HashMap is used for dynamic
  // lookups later and does not change once this init method is complete.
#define COMMON_NAME_INIT_(_, NAME, _2)                             \
  if (!initSingle(cx, GetWellKnownAtomInfo(WellKnownAtomId::NAME), \
                  TaggedParserAtomIndex::WellKnown::NAME())) {     \
    return false;                                                  \
  }
  FOR_EACH_NONTINY_COMMON_PROPERTYNAME(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_
#define COMMON_NAME_INIT_(NAME, _)                                 \
  if (!initSingle(cx, GetWellKnownAtomInfo(WellKnownAtomId::NAME), \
                  TaggedParserAtomIndex::WellKnown::NAME())) {     \
    return false;                                                  \
  }
  JS_FOR_EACH_PROTOTYPE(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_
#define COMMON_NAME_INIT_(NAME)                                    \
  if (!initSingle(cx, GetWellKnownAtomInfo(WellKnownAtomId::NAME), \
                  TaggedParserAtomIndex::WellKnown::NAME())) {     \
    return false;                                                  \
  }
  JS_FOR_EACH_WELL_KNOWN_SYMBOL(COMMON_NAME_INIT_)
#undef COMMON_NAME_INIT_

  return true;
}

} /* namespace frontend */
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
