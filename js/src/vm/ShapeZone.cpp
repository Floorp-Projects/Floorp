/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS symbol tables. */

#include "vm/ShapeZone.h"

#include "gc/Marking-inl.h"
#include "vm/Shape-inl.h"

using namespace js;
using namespace js::gc;

void ShapeZone::fixupPropMapShapeTableAfterMovingGC() {
  for (PropMapShapeSet::Enum e(propMapShapes); !e.empty(); e.popFront()) {
    SharedShape* shape = MaybeForwarded(e.front().unbarrieredGet());
    SharedPropMap* map = shape->propMapMaybeForwarded();
    BaseShape* base = MaybeForwarded(shape->base());

    PropMapShapeSet::Lookup lookup(base, shape->numFixedSlots(), map,
                                   shape->propMapLength(),
                                   shape->objectFlags());
    e.rekeyFront(lookup, shape);
  }
}

#ifdef JSGC_HASH_TABLE_CHECKS
void ShapeZone::checkTablesAfterMovingGC() {
  CheckTableAfterMovingGC(initialPropMaps, [](const auto& entry) {
    SharedPropMap* map = entry.unbarrieredGet();
    CheckGCThingAfterMovingGC(map);
    PropertyKey key = map->getKey(0);
    if (key.isGCThing()) {
      CheckGCThingAfterMovingGC(key.toGCThing());
    }

    return InitialPropMapHasher::Lookup(key, map->getPropertyInfo(0));
  });

  CheckTableAfterMovingGC(baseShapes, [](const auto& entry) {
    BaseShape* base = entry.unbarrieredGet();
    CheckGCThingAfterMovingGC(base);
    CheckProtoAfterMovingGC(base->proto());

    return BaseShapeHasher::Lookup(base->clasp(), base->realm(), base->proto());
  });

  CheckTableAfterMovingGC(initialShapes, [](const auto& entry) {
    SharedShape* shape = entry.unbarrieredGet();
    CheckGCThingAfterMovingGC(shape);
    CheckProtoAfterMovingGC(shape->proto());

    return InitialShapeHasher::Lookup(shape->getObjectClass(), shape->realm(),
                                      shape->proto(), shape->numFixedSlots(),
                                      shape->objectFlags());
  });

  CheckTableAfterMovingGC(propMapShapes, [](const auto& entry) {
    SharedShape* shape = entry.unbarrieredGet();
    CheckGCThingAfterMovingGC(shape);
    CheckGCThingAfterMovingGC(shape->base());
    CheckGCThingAfterMovingGC(shape->propMap());

    return PropMapShapeHasher::Lookup(shape->base(), shape->numFixedSlots(),
                                      shape->propMap(), shape->propMapLength(),
                                      shape->objectFlags());
  });

  CheckTableAfterMovingGC(proxyShapes, [](const auto& entry) {
    ProxyShape* shape = entry.unbarrieredGet();
    CheckGCThingAfterMovingGC(shape);
    CheckProtoAfterMovingGC(shape->proto());

    return ProxyShapeHasher::Lookup(shape->getObjectClass(), shape->realm(),
                                    shape->proto(), shape->objectFlags());
  });

  CheckTableAfterMovingGC(wasmGCShapes, [](const auto& entry) {
    WasmGCShape* shape = entry.unbarrieredGet();
    CheckGCThingAfterMovingGC(shape);
    CheckProtoAfterMovingGC(shape->proto());

    return WasmGCShapeHasher::Lookup(shape->getObjectClass(), shape->realm(),
                                     shape->proto(), shape->recGroup(),
                                     shape->objectFlags());
  });
}
#endif  // JSGC_HASH_TABLE_CHECKS

ShapeZone::ShapeZone(Zone* zone)
    : baseShapes(zone),
      initialPropMaps(zone),
      initialShapes(zone),
      propMapShapes(zone),
      proxyShapes(zone),
      wasmGCShapes(zone) {}

void ShapeZone::purgeShapeCaches(JS::GCContext* gcx) {
  for (Shape* shape : shapesWithCache) {
    MaybeForwarded(shape)->purgeCache(gcx);
  }
  shapesWithCache.clearAndFree();
}

void ShapeZone::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                       size_t* initialPropMapTable,
                                       size_t* shapeTables) {
  *shapeTables += baseShapes.sizeOfExcludingThis(mallocSizeOf);
  *initialPropMapTable += initialPropMaps.sizeOfExcludingThis(mallocSizeOf);
  *shapeTables += initialShapes.sizeOfExcludingThis(mallocSizeOf);
  *shapeTables += propMapShapes.sizeOfExcludingThis(mallocSizeOf);
  *shapeTables += proxyShapes.sizeOfExcludingThis(mallocSizeOf);
  *shapeTables += wasmGCShapes.sizeOfExcludingThis(mallocSizeOf);
  *shapeTables += shapesWithCache.sizeOfExcludingThis(mallocSizeOf);
}
