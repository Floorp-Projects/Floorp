/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS symbol tables. */

#include "vm/ShapeZone.h"

#include "gc/Marking-inl.h"
#include "vm/JSContext-inl.h"

using namespace js;

#ifdef JSGC_HASH_TABLE_CHECKS
void ShapeZone::checkTablesAfterMovingGC() {
  for (auto r = baseShapes.all(); !r.empty(); r.popFront()) {
    BaseShape* base = r.front().unbarrieredGet();
    CheckGCThingAfterMovingGC(base);

    BaseShapeHasher::Lookup lookup(base->clasp(), base->realm(), base->proto());
    BaseShapeSet::Ptr ptr = baseShapes.lookup(lookup);
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
  }

  // Assert that the postbarriers have worked and that nothing is left in
  // initialShapes that points into the nursery, and that the hash table
  // entries are discoverable.
  for (auto r = initialShapes.all(); !r.empty(); r.popFront()) {
    Shape* shape = r.front().unbarrieredGet();

    CheckGCThingAfterMovingGC(shape);

    using Lookup = InitialShapeHasher::Lookup;
    Lookup lookup(shape->getObjectClass(), shape->realm(), shape->proto(),
                  shape->numFixedSlots(), shape->objectFlags());
    InitialShapeSet::Ptr ptr = initialShapes.lookup(lookup);
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
  }
}
#endif  // JSGC_HASH_TABLE_CHECKS

ShapeZone::ShapeZone(Zone* zone)
    : propertyTree(zone), baseShapes(zone), initialShapes(zone) {}

void ShapeZone::clearTables() {
  baseShapes.clear();
  initialShapes.clear();
}

size_t ShapeZone::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  size_t size = 0;
  size += baseShapes.sizeOfExcludingThis(mallocSizeOf);
  size += initialShapes.sizeOfExcludingThis(mallocSizeOf);
  return size;
}
