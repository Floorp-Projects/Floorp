/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS atom table.
 */

#include "vm/JSAtom-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/HashFunctions.h"  // mozilla::HashStringKnownLength
#include "mozilla/RangedPtr.h"

#include <iterator>
#include <string.h>

#include "jstypes.h"

#include "gc/GC.h"
#include "gc/Marking.h"
#include "gc/MaybeRooted.h"
#include "js/CharacterEncoding.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/Symbol.h"
#include "util/Text.h"
#include "vm/JSContext.h"
#include "vm/StaticStrings.h"
#include "vm/SymbolType.h"
#include "vm/WellKnownAtom.h"  // js_*_str

#include "gc/AtomMarking-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/Realm-inl.h"
#include "vm/StringType-inl.h"

using namespace js;

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::RangedPtr;

template <typename CharT>
extern void InflateUTF8CharsToBufferAndTerminate(const JS::UTF8Chars src,
                                                 CharT* dst, size_t dstLen,
                                                 JS::SmallestEncoding encoding);

template <typename CharT>
extern bool UTF8EqualsChars(const JS::UTF8Chars utf8, const CharT* chars);

extern bool GetUTF8AtomizationData(JSContext* cx, const JS::UTF8Chars utf8,
                                   size_t* outlen,
                                   JS::SmallestEncoding* encoding,
                                   HashNumber* hashNum);

struct js::AtomHasher::Lookup {
  union {
    const JS::Latin1Char* latin1Chars;
    const char16_t* twoByteChars;
    const char* utf8Bytes;
  };
  enum { TwoByteChar, Latin1, UTF8 } type;
  size_t length;
  size_t byteLength;
  const JSAtom* atom; /* Optional. */
  JS::AutoCheckCannotGC nogc;

  HashNumber hash;

  MOZ_ALWAYS_INLINE Lookup(const char* utf8Bytes, size_t byteLen, size_t length,
                           HashNumber hash)
      : utf8Bytes(utf8Bytes),
        type(UTF8),
        length(length),
        byteLength(byteLen),
        atom(nullptr),
        hash(hash) {}

  MOZ_ALWAYS_INLINE Lookup(const char16_t* chars, size_t length)
      : twoByteChars(chars),
        type(TwoByteChar),
        length(length),
        atom(nullptr),
        hash(mozilla::HashString(chars, length)) {}

  MOZ_ALWAYS_INLINE Lookup(const JS::Latin1Char* chars, size_t length)
      : latin1Chars(chars),
        type(Latin1),
        length(length),
        atom(nullptr),
        hash(mozilla::HashString(chars, length)) {}

  MOZ_ALWAYS_INLINE Lookup(HashNumber hash, const char16_t* chars,
                           size_t length)
      : twoByteChars(chars),
        type(TwoByteChar),
        length(length),
        atom(nullptr),
        hash(hash) {
    MOZ_ASSERT(hash == mozilla::HashString(chars, length));
  }

  MOZ_ALWAYS_INLINE Lookup(HashNumber hash, const JS::Latin1Char* chars,
                           size_t length)
      : latin1Chars(chars),
        type(Latin1),
        length(length),
        atom(nullptr),
        hash(hash) {
    MOZ_ASSERT(hash == mozilla::HashString(chars, length));
  }

  inline explicit Lookup(const JSAtom* atom)
      : type(atom->hasLatin1Chars() ? Latin1 : TwoByteChar),
        length(atom->length()),
        atom(atom),
        hash(atom->hash()) {
    if (type == Latin1) {
      latin1Chars = atom->latin1Chars(nogc);
      MOZ_ASSERT(mozilla::HashString(latin1Chars, length) == hash);
    } else {
      MOZ_ASSERT(type == TwoByteChar);
      twoByteChars = atom->twoByteChars(nogc);
      MOZ_ASSERT(mozilla::HashString(twoByteChars, length) == hash);
    }
  }
};

inline HashNumber js::AtomHasher::hash(const Lookup& l) { return l.hash; }

MOZ_ALWAYS_INLINE bool js::AtomHasher::match(const WeakHeapPtrAtom& entry,
                                             const Lookup& lookup) {
  JSAtom* key = entry.unbarrieredGet();
  if (lookup.atom) {
    return lookup.atom == key;
  }
  if (key->length() != lookup.length || key->hash() != lookup.hash) {
    return false;
  }

  if (key->hasLatin1Chars()) {
    const Latin1Char* keyChars = key->latin1Chars(lookup.nogc);
    switch (lookup.type) {
      case Lookup::Latin1:
        return mozilla::ArrayEqual(keyChars, lookup.latin1Chars, lookup.length);
      case Lookup::TwoByteChar:
        return EqualChars(keyChars, lookup.twoByteChars, lookup.length);
      case Lookup::UTF8: {
        JS::UTF8Chars utf8(lookup.utf8Bytes, lookup.byteLength);
        return UTF8EqualsChars(utf8, keyChars);
      }
    }
  }

  const char16_t* keyChars = key->twoByteChars(lookup.nogc);
  switch (lookup.type) {
    case Lookup::Latin1:
      return EqualChars(lookup.latin1Chars, keyChars, lookup.length);
    case Lookup::TwoByteChar:
      return mozilla::ArrayEqual(keyChars, lookup.twoByteChars, lookup.length);
    case Lookup::UTF8: {
      JS::UTF8Chars utf8(lookup.utf8Bytes, lookup.byteLength);
      return UTF8EqualsChars(utf8, keyChars);
    }
  }

  MOZ_ASSERT_UNREACHABLE("AtomHasher::match unknown type");
  return false;
}

UniqueChars js::AtomToPrintableString(JSContext* cx, JSAtom* atom) {
  return QuoteString(cx, atom);
}

// Use a low initial capacity for the permanent atoms table to avoid penalizing
// runtimes that create a small number of atoms.
static const uint32_t JS_PERMANENT_ATOM_SIZE = 64;

MOZ_ALWAYS_INLINE AtomSet::Ptr js::FrozenAtomSet::readonlyThreadsafeLookup(
    const AtomSet::Lookup& l) const {
  return mSet->readonlyThreadsafeLookup(l);
}

bool JSRuntime::initializeAtoms(JSContext* cx) {
  MOZ_ASSERT(!atoms_);
  MOZ_ASSERT(!permanentAtomsDuringInit_);
  MOZ_ASSERT(!permanentAtoms_);

  if (parentRuntime) {
    permanentAtoms_ = parentRuntime->permanentAtoms_;

    staticStrings = parentRuntime->staticStrings;
    commonNames = parentRuntime->commonNames;
    emptyString = parentRuntime->emptyString;
    wellKnownSymbols = parentRuntime->wellKnownSymbols;

    atoms_ = js_new<AtomsTable>();
    return bool(atoms_);
  }

  permanentAtomsDuringInit_ = js_new<AtomSet>(JS_PERMANENT_ATOM_SIZE);
  if (!permanentAtomsDuringInit_) {
    return false;
  }

  staticStrings = js_new<StaticStrings>();
  if (!staticStrings || !staticStrings->init(cx)) {
    return false;
  }

  // The bare symbol names are already part of the well-known set, but their
  // descriptions are not, so enumerate them here and add them to the initial
  // permanent atoms set below.
  static const WellKnownAtomInfo symbolDescInfo[] = {
#define COMMON_NAME_INFO(NAME)                                  \
  {uint32_t(sizeof("Symbol." #NAME) - 1),                       \
   mozilla::HashStringKnownLength("Symbol." #NAME,              \
                                  sizeof("Symbol." #NAME) - 1), \
   "Symbol." #NAME},
      JS_FOR_EACH_WELL_KNOWN_SYMBOL(COMMON_NAME_INFO)
#undef COMMON_NAME_INFO
  };

  commonNames = js_new<JSAtomState>();
  if (!commonNames) {
    return false;
  }

  ImmutablePropertyNamePtr* names =
      reinterpret_cast<ImmutablePropertyNamePtr*>(commonNames.ref());
  for (size_t i = 0; i < uint32_t(WellKnownAtomId::Limit); i++) {
    const auto& info = wellKnownAtomInfos[i];
    JSAtom* atom = Atomize(cx, info.hash, info.content, info.length);
    if (!atom) {
      return false;
    }
    names->init(atom->asPropertyName());
    names++;
  }

  for (const auto& info : symbolDescInfo) {
    JSAtom* atom = Atomize(cx, info.hash, info.content, info.length);
    if (!atom) {
      return false;
    }
    names->init(atom->asPropertyName());
    names++;
  }
  MOZ_ASSERT(uintptr_t(names) == uintptr_t(commonNames + 1));

  emptyString = commonNames->empty;

  // Create the well-known symbols.
  auto wks = js_new<WellKnownSymbols>();
  if (!wks) {
    return false;
  }

  // Prevent GC until we have fully initialized the well known symbols table.
  // Faster than zeroing the array and null checking during every GC.
  gc::AutoSuppressGC nogc(cx);

  ImmutablePropertyNamePtr* descriptions =
      commonNames->wellKnownSymbolDescriptions();
  ImmutableSymbolPtr* symbols = reinterpret_cast<ImmutableSymbolPtr*>(wks);
  for (size_t i = 0; i < JS::WellKnownSymbolLimit; i++) {
    HandlePropertyName description = descriptions[i];
    JS::Symbol* symbol = JS::Symbol::new_(cx, JS::SymbolCode(i), description);
    if (!symbol) {
      ReportOutOfMemory(cx);
      return false;
    }
    symbols[i].init(symbol);
  }

  wellKnownSymbols = wks;
  return true;
}

void JSRuntime::finishAtoms() {
  js_delete(atoms_.ref());

  if (!parentRuntime) {
    js_delete(permanentAtomsDuringInit_.ref());
    js_delete(permanentAtoms_.ref());
    js_delete(staticStrings.ref());
    js_delete(commonNames.ref());
    js_delete(wellKnownSymbols.ref());
  }

  atoms_ = nullptr;
  permanentAtomsDuringInit_ = nullptr;
  permanentAtoms_ = nullptr;
  staticStrings = nullptr;
  commonNames = nullptr;
  wellKnownSymbols = nullptr;
  emptyString = nullptr;
}

AtomsTable::AtomsTable()
    : atoms(InitialTableSize), atomsAddedWhileSweeping(nullptr) {}

AtomsTable::~AtomsTable() { MOZ_ASSERT(!atomsAddedWhileSweeping); }

void AtomsTable::tracePinnedAtoms(JSTracer* trc) {
  for (JSAtom* atom : pinnedAtoms) {
    TraceRoot(trc, &atom, "pinned atom");
  }
}

void js::TraceAtoms(JSTracer* trc) {
  JSRuntime* rt = trc->runtime();
  if (rt->permanentAtomsPopulated()) {
    rt->atoms().tracePinnedAtoms(trc);
  }
}

static void TracePermanentAtoms(JSTracer* trc, AtomSet::Range atoms) {
  for (; !atoms.empty(); atoms.popFront()) {
    JSAtom* atom = atoms.front().unbarrieredGet();
    MOZ_ASSERT(atom->isPermanentAtom());
    TraceProcessGlobalRoot(trc, atom, "permanent atom");
  }
}

void JSRuntime::tracePermanentThingsDuringInit(JSTracer* trc) {
  // Permanent atoms and symbols only need to be traced during initialization.
  if (!permanentAtomsDuringInit_) {
    return;
  }

  // This table is not used in child runtimes, who share the permanent atoms and
  // symbols from the parent.
  MOZ_ASSERT(!parentRuntime);

  TracePermanentAtoms(trc, permanentAtomsDuringInit_->all());
  TraceWellKnownSymbols(trc);
}

void js::TraceWellKnownSymbols(JSTracer* trc) {
  JSRuntime* rt = trc->runtime();
  MOZ_ASSERT(!rt->parentRuntime);

  if (WellKnownSymbols* wks = rt->wellKnownSymbols) {
    for (size_t i = 0; i < JS::WellKnownSymbolLimit; i++) {
      TraceProcessGlobalRoot(trc, wks->get(i).get(), "well_known_symbol");
    }
  }
}

void AtomsTable::traceWeak(JSTracer* trc) {
  for (AtomSet::Enum e(atoms); !e.empty(); e.popFront()) {
    JSAtom* atom = e.front().unbarrieredGet();
    MOZ_DIAGNOSTIC_ASSERT(atom);
    if (!TraceManuallyBarrieredWeakEdge(trc, &atom, "AtomsTable::atoms")) {
      e.removeFront();
    } else {
      MOZ_ASSERT(atom == e.front().unbarrieredGet());
    }
  }
}

bool AtomsTable::startIncrementalSweep(Maybe<SweepIterator>& atomsToSweepOut) {
  MOZ_ASSERT(JS::RuntimeHeapIsCollecting());
  MOZ_ASSERT(atomsToSweepOut.isNothing());
  MOZ_ASSERT(!atomsAddedWhileSweeping);

  atomsAddedWhileSweeping = js_new<AtomSet>();
  if (!atomsAddedWhileSweeping) {
    return false;
  }

  atomsToSweepOut.emplace(atoms);

  return true;
}

void AtomsTable::mergeAtomsAddedWhileSweeping() {
  // Add atoms that were added to the secondary table while we were sweeping
  // the main table.

  AutoEnterOOMUnsafeRegion oomUnsafe;

  auto newAtoms = atomsAddedWhileSweeping;
  atomsAddedWhileSweeping = nullptr;

  for (auto r = newAtoms->all(); !r.empty(); r.popFront()) {
    if (!atoms.putNew(AtomHasher::Lookup(r.front().unbarrieredGet()),
                      r.front())) {
      oomUnsafe.crash("Adding atom from secondary table after sweep");
    }
  }

  js_delete(newAtoms);
}

bool AtomsTable::sweepIncrementally(SweepIterator& atomsToSweep,
                                    SliceBudget& budget) {
  // Sweep the table incrementally until we run out of work or budget.
  while (!atomsToSweep.empty()) {
    budget.step();
    if (budget.isOverBudget()) {
      return false;
    }

    JSAtom* atom = atomsToSweep.front().unbarrieredGet();
    MOZ_DIAGNOSTIC_ASSERT(atom);
    if (IsAboutToBeFinalizedUnbarriered(atom)) {
      MOZ_ASSERT(!atom->isPinned());
      atomsToSweep.removeFront();
    } else {
      MOZ_ASSERT(atom == atomsToSweep.front().unbarrieredGet());
    }
    atomsToSweep.popFront();
  }

  mergeAtomsAddedWhileSweeping();
  return true;
}

size_t AtomsTable::sizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t size = sizeof(AtomsTable);
  size += atoms.shallowSizeOfExcludingThis(mallocSizeOf);
  if (atomsAddedWhileSweeping) {
    size += atomsAddedWhileSweeping->shallowSizeOfExcludingThis(mallocSizeOf);
  }
  size += pinnedAtoms.sizeOfExcludingThis(mallocSizeOf);
  return size;
}

bool JSRuntime::initMainAtomsTables(JSContext* cx) {
  MOZ_ASSERT(!parentRuntime);
  MOZ_ASSERT(!permanentAtomsPopulated());

  gc.freezePermanentSharedThings();

  // The permanent atoms table has now been populated.
  permanentAtoms_ =
      js_new<FrozenAtomSet>(permanentAtomsDuringInit_);  // Takes ownership.
  permanentAtomsDuringInit_ = nullptr;

  // Initialize the main atoms table.
  MOZ_ASSERT(!atoms_);
  atoms_ = js_new<AtomsTable>();
  return atoms_;
}

template <typename CharT>
static MOZ_ALWAYS_INLINE JSAtom* AtomizeAndCopyCharsFromLookup(
    JSContext* cx, const CharT* chars, size_t length,
    const AtomHasher::Lookup& lookup, const Maybe<uint32_t>& indexValue);

template <typename CharT>
static MOZ_NEVER_INLINE JSAtom* PermanentlyAtomizeAndCopyChars(
    JSContext* cx, Maybe<AtomSet::AddPtr>& zonePtr, const CharT* chars,
    size_t length, const Maybe<uint32_t>& indexValue,
    const AtomHasher::Lookup& lookup);

template <typename CharT>
static MOZ_ALWAYS_INLINE JSAtom* AtomizeAndCopyCharsFromLookup(
    JSContext* cx, const CharT* chars, size_t length,
    const AtomHasher::Lookup& lookup, const Maybe<uint32_t>& indexValue) {
  // Try the per-Zone cache first. If we find the atom there we can avoid the
  // markAtom call, and the multiple HashSet lookups below.
  // We don't use the per-Zone cache if we want a pinned atom: handling that
  // is more complicated and pinning atoms is relatively uncommon.
  Zone* zone = cx->zone();
  Maybe<AtomSet::AddPtr> zonePtr;
  if (MOZ_LIKELY(zone)) {
    zonePtr.emplace(zone->atomCache().lookupForAdd(lookup));
    if (zonePtr.ref()) {
      // The cache is purged on GC so if we're in the middle of an
      // incremental GC we should have barriered the atom when we put
      // it in the cache.
      JSAtom* atom = zonePtr.ref()->unbarrieredGet();
      MOZ_ASSERT(AtomIsMarked(zone, atom));
      return atom;
    }
  }

  // This function can be called during initialization, while the permanent
  // atoms table is being created. In this case all atoms created are added to
  // the permanent atoms table.
  if (!cx->permanentAtomsPopulated()) {
    return PermanentlyAtomizeAndCopyChars(cx, zonePtr, chars, length,
                                          indexValue, lookup);
  }

  AtomSet::Ptr pp = cx->permanentAtoms().readonlyThreadsafeLookup(lookup);
  if (pp) {
    JSAtom* atom = pp->get();
    if (zonePtr && MOZ_UNLIKELY(!zone->atomCache().add(*zonePtr, atom))) {
      ReportOutOfMemory(cx);
      return nullptr;
    }

    return atom;
  }

  if (MOZ_UNLIKELY(!JSString::validateLength(cx, length))) {
    return nullptr;
  }

  JSAtom* atom =
      cx->atoms().atomizeAndCopyChars(cx, chars, length, indexValue, lookup);
  if (!atom) {
    return nullptr;
  }

  if (MOZ_UNLIKELY(!cx->atomMarking().inlinedMarkAtomFallible(cx, atom))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  if (zonePtr && MOZ_UNLIKELY(!zone->atomCache().add(*zonePtr, atom))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return atom;
}

template <typename CharT>
static MOZ_ALWAYS_INLINE JSAtom* AllocateNewAtom(
    JSContext* cx, const CharT* chars, size_t length,
    const Maybe<uint32_t>& indexValue, const AtomHasher::Lookup& lookup);

template <typename CharT>
MOZ_ALWAYS_INLINE JSAtom* AtomsTable::atomizeAndCopyChars(
    JSContext* cx, const CharT* chars, size_t length,
    const Maybe<uint32_t>& indexValue, const AtomHasher::Lookup& lookup) {
  AtomSet::AddPtr p;

  if (!atomsAddedWhileSweeping) {
    p = atoms.lookupForAdd(lookup);
  } else {
    // We're currently sweeping the main atoms table and all new atoms will
    // be added to a secondary table. Check this first.
    p = atomsAddedWhileSweeping->lookupForAdd(lookup);

    // If that fails check the main table but check if any atom found there
    // is dead.
    if (!p) {
      if (AtomSet::AddPtr p2 = atoms.lookupForAdd(lookup)) {
        JSAtom* atom = p2->unbarrieredGet();
        if (!IsAboutToBeFinalizedUnbarriered(atom)) {
          p = p2;
        }
      }
    }
  }

  if (p) {
    return p->get();
  }

  JSAtom* atom = AllocateNewAtom(cx, chars, length, indexValue, lookup);
  if (!atom) {
    return nullptr;
  }

  // The operations above can't GC; therefore the atoms table has not been
  // modified and p is still valid.
  AtomSet* addSet = atomsAddedWhileSweeping ? atomsAddedWhileSweeping : &atoms;
  if (MOZ_UNLIKELY(!addSet->add(p, atom))) {
    ReportOutOfMemory(cx); /* SystemAllocPolicy does not report OOM. */
    return nullptr;
  }

  return atom;
}

/* |chars| must not point into an inline or short string. */
template <typename CharT>
static MOZ_ALWAYS_INLINE JSAtom* AtomizeAndCopyChars(
    JSContext* cx, const CharT* chars, size_t length,
    const Maybe<uint32_t>& indexValue) {
  if (JSAtom* s = cx->staticStrings().lookup(chars, length)) {
    return s;
  }

  AtomHasher::Lookup lookup(chars, length);
  return AtomizeAndCopyCharsFromLookup(cx, chars, length, lookup, indexValue);
}

template <typename CharT>
static MOZ_NEVER_INLINE JSAtom* PermanentlyAtomizeAndCopyChars(
    JSContext* cx, Maybe<AtomSet::AddPtr>& zonePtr, const CharT* chars,
    size_t length, const Maybe<uint32_t>& indexValue,
    const AtomHasher::Lookup& lookup) {
  MOZ_ASSERT(!cx->permanentAtomsPopulated());
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));

  JSRuntime* rt = cx->runtime();
  AtomSet& atoms = *rt->permanentAtomsDuringInit();
  AtomSet::AddPtr p = atoms.lookupForAdd(lookup);
  if (p) {
    return p->get();
  }

  JSAtom* atom = AllocateNewAtom(cx, chars, length, indexValue, lookup);
  if (!atom) {
    return nullptr;
  }

  atom->morphIntoPermanentAtom();

  // We are single threaded at this point, and the operations we've done since
  // then can't GC; therefore the atoms table has not been modified and p is
  // still valid.
  if (!atoms.add(p, atom)) {
    ReportOutOfMemory(cx); /* SystemAllocPolicy does not report OOM. */
    return nullptr;
  }

  if (zonePtr && MOZ_UNLIKELY(!cx->zone()->atomCache().add(*zonePtr, atom))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return atom;
}

struct AtomizeUTF8CharsWrapper {
  JS::UTF8Chars utf8;
  JS::SmallestEncoding encoding;

  AtomizeUTF8CharsWrapper(const JS::UTF8Chars& chars,
                          JS::SmallestEncoding minEncode)
      : utf8(chars), encoding(minEncode) {}
};

// MakeLinearStringForAtomization has 4 variants.
// This is used by Latin1Char and char16_t.
template <typename CharT>
static MOZ_ALWAYS_INLINE JSLinearString* MakeLinearStringForAtomization(
    JSContext* cx, const CharT* chars, size_t length) {
  return NewStringCopyN<NoGC>(cx, chars, length, gc::TenuredHeap);
}

template <typename CharT>
static MOZ_ALWAYS_INLINE JSLinearString* MakeUTF8AtomHelper(
    JSContext* cx, const AtomizeUTF8CharsWrapper* chars, size_t length) {
  if (JSInlineString::lengthFits<CharT>(length)) {
    CharT* storage;
    JSInlineString* str =
        AllocateInlineString<NoGC>(cx, length, &storage, gc::TenuredHeap);
    if (!str) {
      return nullptr;
    }

    InflateUTF8CharsToBufferAndTerminate(chars->utf8, storage, length,
                                         chars->encoding);
    return str;
  }

  // MakeAtomUTF8Helper is called from deep in the Atomization path, which
  // expects functions to fail gracefully with nullptr on OOM, without throwing.
  //
  // Flat strings are null-terminated. Leave room with length + 1
  UniquePtr<CharT[], JS::FreePolicy> newStr(
      js_pod_arena_malloc<CharT>(js::StringBufferArena, length + 1));
  if (!newStr) {
    return nullptr;
  }

  InflateUTF8CharsToBufferAndTerminate(chars->utf8, newStr.get(), length,
                                       chars->encoding);

  return JSLinearString::new_<NoGC>(cx, std::move(newStr), length,
                                    gc::TenuredHeap);
}

// Another 2 variants of MakeLinearStringForAtomization.
static MOZ_ALWAYS_INLINE JSLinearString* MakeLinearStringForAtomization(
    JSContext* cx, const AtomizeUTF8CharsWrapper* chars, size_t length) {
  if (length == 0) {
    return cx->emptyString();
  }

  if (chars->encoding == JS::SmallestEncoding::UTF16) {
    return MakeUTF8AtomHelper<char16_t>(cx, chars, length);
  }
  return MakeUTF8AtomHelper<JS::Latin1Char>(cx, chars, length);
}

template <typename CharT>
static MOZ_ALWAYS_INLINE JSAtom* AllocateNewAtom(
    JSContext* cx, const CharT* chars, size_t length,
    const Maybe<uint32_t>& indexValue, const AtomHasher::Lookup& lookup) {
  AutoAllocInAtomsZone ac(cx);

  JSLinearString* linear = MakeLinearStringForAtomization(cx, chars, length);
  if (!linear) {
    // Grudgingly forgo last-ditch GC. The alternative would be to manually GC
    // here, and retry from the top.
    ReportOutOfMemory(cx);
    return nullptr;
  }

  JSAtom* atom = linear->morphAtomizedStringIntoAtom(lookup.hash);
  MOZ_ASSERT(atom->hash() == lookup.hash);

  if (indexValue) {
    atom->setIsIndex(*indexValue);
  } else {
    // We need to call isIndexSlow directly to avoid the flag check in isIndex,
    // because we still have to initialize that flag.
    uint32_t index;
    if (atom->isIndexSlow(&index)) {
      atom->setIsIndex(index);
    }
  }

  return atom;
}

JSAtom* js::AtomizeString(JSContext* cx, JSString* str) {
  if (str->isAtom()) {
    return &str->asAtom();
  }

  JSLinearString* linear = str->ensureLinear(cx);
  if (!linear) {
    return nullptr;
  }

  if (cx->isMainThreadContext()) {
    if (JSAtom* atom = cx->caches().stringToAtomCache.lookup(linear)) {
      return atom;
    }
  }

  Maybe<uint32_t> indexValue;
  if (str->hasIndexValue()) {
    indexValue.emplace(str->getIndexValue());
  }

  JS::AutoCheckCannotGC nogc;
  JSAtom* atom = linear->hasLatin1Chars()
                     ? AtomizeAndCopyChars(cx, linear->latin1Chars(nogc),
                                           linear->length(), indexValue)
                     : AtomizeAndCopyChars(cx, linear->twoByteChars(nogc),
                                           linear->length(), indexValue);
  if (!atom) {
    return nullptr;
  }

  if (cx->isMainThreadContext()) {
    cx->caches().stringToAtomCache.maybePut(linear, atom);
  }

  return atom;
}

bool js::AtomIsPinned(JSContext* cx, JSAtom* atom) { return atom->isPinned(); }

bool js::PinAtom(JSContext* cx, JSAtom* atom) {
  JS::AutoCheckCannotGC nogc;
  return cx->runtime()->atoms().maybePinExistingAtom(cx, atom);
}

bool AtomsTable::maybePinExistingAtom(JSContext* cx, JSAtom* atom) {
  MOZ_ASSERT(atom);

  if (atom->isPinned()) {
    return true;
  }

  if (!pinnedAtoms.append(atom)) {
    return false;
  }

  atom->setPinned();
  return true;
}

JSAtom* js::Atomize(JSContext* cx, const char* bytes, size_t length,
                    const Maybe<uint32_t>& indexValue) {
  const Latin1Char* chars = reinterpret_cast<const Latin1Char*>(bytes);
  return AtomizeAndCopyChars(cx, chars, length, indexValue);
}

JSAtom* js::Atomize(JSContext* cx, HashNumber hash, const char* bytes,
                    size_t length) {
  const Latin1Char* chars = reinterpret_cast<const Latin1Char*>(bytes);
  if (JSAtom* s = cx->staticStrings().lookup(chars, length)) {
    return s;
  }

  AtomHasher::Lookup lookup(hash, chars, length);
  return AtomizeAndCopyCharsFromLookup(cx, chars, length, lookup, Nothing());
}

template <typename CharT>
JSAtom* js::AtomizeChars(JSContext* cx, const CharT* chars, size_t length) {
  return AtomizeAndCopyChars(cx, chars, length, Nothing());
}

template JSAtom* js::AtomizeChars(JSContext* cx, const Latin1Char* chars,
                                  size_t length);

template JSAtom* js::AtomizeChars(JSContext* cx, const char16_t* chars,
                                  size_t length);

/* |chars| must not point into an inline or short string. */
template <typename CharT>
JSAtom* js::AtomizeChars(JSContext* cx, HashNumber hash, const CharT* chars,
                         size_t length) {
  if (JSAtom* s = cx->staticStrings().lookup(chars, length)) {
    return s;
  }

  AtomHasher::Lookup lookup(hash, chars, length);
  return AtomizeAndCopyCharsFromLookup(cx, chars, length, lookup, Nothing());
}

template JSAtom* js::AtomizeChars(JSContext* cx, HashNumber hash,
                                  const Latin1Char* chars, size_t length);

template JSAtom* js::AtomizeChars(JSContext* cx, HashNumber hash,
                                  const char16_t* chars, size_t length);

JSAtom* js::AtomizeUTF8Chars(JSContext* cx, const char* utf8Chars,
                             size_t utf8ByteLength) {
  {
    // Permanent atoms,|JSRuntime::atoms_|, and  static strings are disjoint
    // sets.  |AtomizeAndCopyCharsFromLookup| only consults the first two sets,
    // so we must map any static strings ourselves.  See bug 1575947.
    StaticStrings& statics = cx->staticStrings();

    // Handle all pure-ASCII UTF-8 static strings.
    if (JSAtom* s = statics.lookup(utf8Chars, utf8ByteLength)) {
      return s;
    }

    // The only non-ASCII static strings are the single-code point strings
    // U+0080 through U+00FF, encoded as
    //
    //   0b1100'00xx 0b10xx'xxxx
    //
    // where the encoded code point is the concatenation of the 'x' bits -- and
    // where the highest 'x' bit is necessarily 1 (because U+0080 through U+00FF
    // all contain an 0x80 bit).
    if (utf8ByteLength == 2) {
      auto first = static_cast<uint8_t>(utf8Chars[0]);
      if ((first & 0b1111'1110) == 0b1100'0010) {
        auto second = static_cast<uint8_t>(utf8Chars[1]);
        if (mozilla::IsTrailingUnit(mozilla::Utf8Unit(second))) {
          uint8_t unit =
              static_cast<uint8_t>(first << 6) | (second & 0b0011'1111);

          MOZ_ASSERT(StaticStrings::hasUnit(unit));
          return statics.getUnit(unit);
        }
      }

      // Fallthrough code handles the cases where the two units aren't a Latin-1
      // code point or are invalid.
    }
  }

  size_t length;
  HashNumber hash;
  JS::SmallestEncoding forCopy;
  JS::UTF8Chars utf8(utf8Chars, utf8ByteLength);
  if (!GetUTF8AtomizationData(cx, utf8, &length, &forCopy, &hash)) {
    return nullptr;
  }

  AtomizeUTF8CharsWrapper chars(utf8, forCopy);
  AtomHasher::Lookup lookup(utf8Chars, utf8ByteLength, length, hash);
  return AtomizeAndCopyCharsFromLookup(cx, &chars, length, lookup, Nothing());
}

bool js::IndexToIdSlow(JSContext* cx, uint32_t index, MutableHandleId idp) {
  MOZ_ASSERT(index > JSID_INT_MAX);

  char16_t buf[UINT32_CHAR_BUFFER_LENGTH];
  RangedPtr<char16_t> end(std::end(buf), buf, std::end(buf));
  RangedPtr<char16_t> start = BackfillIndexInCharBuffer(index, end);

  JSAtom* atom = AtomizeChars(cx, start.get(), end - start);
  if (!atom) {
    return false;
  }

  idp.set(JS::PropertyKey::fromNonIntAtom(atom));
  return true;
}

template <AllowGC allowGC>
static JSAtom* ToAtomSlow(
    JSContext* cx, typename MaybeRooted<Value, allowGC>::HandleType arg) {
  MOZ_ASSERT(!arg.isString());

  Value v = arg;
  if (!v.isPrimitive()) {
    MOZ_ASSERT(!cx->isHelperThreadContext());
    if (!allowGC) {
      return nullptr;
    }
    RootedValue v2(cx, v);
    if (!ToPrimitive(cx, JSTYPE_STRING, &v2)) {
      return nullptr;
    }
    v = v2;
  }

  if (v.isString()) {
    JSAtom* atom = AtomizeString(cx, v.toString());
    if (!allowGC && !atom) {
      cx->recoverFromOutOfMemory();
    }
    return atom;
  }
  if (v.isInt32()) {
    JSAtom* atom = Int32ToAtom(cx, v.toInt32());
    if (!allowGC && !atom) {
      cx->recoverFromOutOfMemory();
    }
    return atom;
  }
  if (v.isDouble()) {
    JSAtom* atom = NumberToAtom(cx, v.toDouble());
    if (!allowGC && !atom) {
      cx->recoverFromOutOfMemory();
    }
    return atom;
  }
  if (v.isBoolean()) {
    return v.toBoolean() ? cx->names().true_ : cx->names().false_;
  }
  if (v.isNull()) {
    return cx->names().null;
  }
  if (v.isSymbol()) {
    MOZ_ASSERT(!cx->isHelperThreadContext());
    if (allowGC) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_SYMBOL_TO_STRING);
    }
    return nullptr;
  }
  if (v.isBigInt()) {
    RootedBigInt i(cx, v.toBigInt());
    JSAtom* atom = BigIntToAtom<allowGC>(cx, i);
    if (!allowGC && !atom) {
      cx->recoverFromOutOfMemory();
    }
    return atom;
  }
  MOZ_ASSERT(v.isUndefined());
  return cx->names().undefined;
}

template <AllowGC allowGC>
JSAtom* js::ToAtom(JSContext* cx,
                   typename MaybeRooted<Value, allowGC>::HandleType v) {
  if (!v.isString()) {
    return ToAtomSlow<allowGC>(cx, v);
  }

  JSString* str = v.toString();
  if (str->isAtom()) {
    return &str->asAtom();
  }

  JSAtom* atom = AtomizeString(cx, str);
  if (!atom && !allowGC) {
    MOZ_ASSERT_IF(!cx->isHelperThreadContext(), cx->isThrowingOutOfMemory());
    cx->recoverFromOutOfMemory();
  }
  return atom;
}

template JSAtom* js::ToAtom<CanGC>(JSContext* cx, HandleValue v);

template JSAtom* js::ToAtom<NoGC>(JSContext* cx, const Value& v);

Handle<PropertyName*> js::ClassName(JSProtoKey key, JSContext* cx) {
  return ClassName(key, cx->names());
}
