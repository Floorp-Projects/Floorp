/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/StaticStrings.h"

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Range.h"

#include <stddef.h>
#include <stdint.h>

#include "NamespaceImports.h"

#include "gc/Allocator.h"
#include "gc/AllocKind.h"
#include "gc/Tracer.h"
#include "js/HashTable.h"
#include "js/TypeDecls.h"

#include "vm/Realm-inl.h"
#include "vm/StringType-inl.h"

using namespace js;

constexpr StaticStrings::SmallCharTable StaticStrings::createSmallCharTable() {
  SmallCharTable array{};
  for (size_t i = 0; i < SMALL_CHAR_TABLE_SIZE; i++) {
    array[i] = toSmallChar(i);
  }
  return array;
}

const StaticStrings::SmallCharTable StaticStrings::toSmallCharTable =
    createSmallCharTable();

bool StaticStrings::init(JSContext* cx) {
  AutoAllocInAtomsZone az(cx);

  static_assert(UNIT_STATIC_LIMIT - 1 <= JSString::MAX_LATIN1_CHAR,
                "Unit strings must fit in Latin1Char.");

  using Latin1Range = mozilla::Range<const Latin1Char>;

  for (uint32_t i = 0; i < UNIT_STATIC_LIMIT; i++) {
    Latin1Char ch = Latin1Char(i);
    JSLinearString* s =
        NewInlineString<NoGC>(cx, Latin1Range(&ch, 1), gc::TenuredHeap);
    if (!s) {
      return false;
    }
    HashNumber hash = mozilla::HashString(&ch, 1);
    unitStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
  }

  for (uint32_t i = 0; i < NUM_LENGTH2_ENTRIES; i++) {
    Latin1Char buffer[] = {firstCharOfLength2(i), secondCharOfLength2(i)};
    JSLinearString* s =
        NewInlineString<NoGC>(cx, Latin1Range(buffer, 2), gc::TenuredHeap);
    if (!s) {
      return false;
    }
    HashNumber hash = mozilla::HashString(buffer, 2);
    length2StaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
  }

  for (uint32_t i = 0; i < INT_STATIC_LIMIT; i++) {
    if (i < 10) {
      intStaticTable[i] = unitStaticTable[i + '0'];
    } else if (i < 100) {
      auto index =
          getLength2IndexStatic(char(i / 10) + '0', char(i % 10) + '0');
      intStaticTable[i] = length2StaticTable[index];
    } else {
      Latin1Char buffer[] = {Latin1Char('0' + (i / 100)),
                             Latin1Char('0' + ((i / 10) % 10)),
                             Latin1Char('0' + (i % 10))};
      JSLinearString* s =
          NewInlineString<NoGC>(cx, Latin1Range(buffer, 3), gc::TenuredHeap);
      if (!s) {
        return false;
      }
      HashNumber hash = mozilla::HashString(buffer, 3);
      intStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
    }

    // Static string initialization can not race, so allow even without the
    // lock.
    intStaticTable[i]->setIsIndex(i);
  }

  return true;
}

inline void TraceStaticString(JSTracer* trc, JSAtom* atom, const char* name) {
  MOZ_ASSERT(atom->isPermanentAtom());
  TraceProcessGlobalRoot(trc, atom, name);
}

void StaticStrings::trace(JSTracer* trc) {
  /* These strings never change, so barriers are not needed. */

  for (auto& s : unitStaticTable) {
    TraceStaticString(trc, s, "unit-static-string");
  }

  for (auto& s : length2StaticTable) {
    TraceStaticString(trc, s, "length2-static-string");
  }

  /* This may mark some strings more than once, but so be it. */
  for (auto& s : intStaticTable) {
    TraceStaticString(trc, s, "int-static-string");
  }
}
