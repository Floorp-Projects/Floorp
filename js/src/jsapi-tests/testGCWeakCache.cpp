/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Policy.h"
#include "js/GCHashTable.h"
#include "js/RootingAPI.h"
#include "js/SweepingAPI.h"

#include "jsapi-tests/tests.h"

// Exercise WeakCache<GCHashSet>.
BEGIN_TEST(testWeakCacheSet)
{
    // Create two objects tenured and two in the nursery. If zeal is on,
    // this may fail and we'll get more tenured objects. That's fine:
    // the test will continue to work, it will just not test as much.
    JS::RootedObject tenured1(cx, JS_NewPlainObject(cx));
    JS::RootedObject tenured2(cx, JS_NewPlainObject(cx));
    JS_GC(cx);
    JS::RootedObject nursery1(cx, JS_NewPlainObject(cx));
    JS::RootedObject nursery2(cx, JS_NewPlainObject(cx));

    using ObjectSet = js::GCHashSet<JS::Heap<JSObject*>,
                                    js::MovableCellHasher<JS::Heap<JSObject*>>,
                                    js::SystemAllocPolicy>;
    using Cache = JS::WeakCache<ObjectSet>;
    auto cache = Cache(JS::GetObjectZone(tenured1), ObjectSet());
    CHECK(cache.init());

    cache.put(tenured1);
    cache.put(tenured2);
    cache.put(nursery1);
    cache.put(nursery2);

    // Verify relocation and that we don't sweep too aggressively.
    JS_GC(cx);
    CHECK(cache.has(tenured1));
    CHECK(cache.has(tenured2));
    CHECK(cache.has(nursery1));
    CHECK(cache.has(nursery2));

    // Unroot two entries and verify that they get removed.
    tenured2 = nursery2 = nullptr;
    JS_GC(cx);
    CHECK(cache.has(tenured1));
    CHECK(cache.has(nursery1));
    CHECK(cache.count() == 2);

    return true;
}
END_TEST(testWeakCacheSet)

// Exercise WeakCache<GCHashMap>.
BEGIN_TEST(testWeakCacheMap)
{
    // Create two objects tenured and two in the nursery. If zeal is on,
    // this may fail and we'll get more tenured objects. That's fine:
    // the test will continue to work, it will just not test as much.
    JS::RootedObject tenured1(cx, JS_NewPlainObject(cx));
    JS::RootedObject tenured2(cx, JS_NewPlainObject(cx));
    JS_GC(cx);
    JS::RootedObject nursery1(cx, JS_NewPlainObject(cx));
    JS::RootedObject nursery2(cx, JS_NewPlainObject(cx));

    using ObjectMap = js::GCHashMap<JS::Heap<JSObject*>, uint32_t,
                                    js::MovableCellHasher<JS::Heap<JSObject*>>>;
    using Cache = JS::WeakCache<ObjectMap>;
    Cache cache(JS::GetObjectZone(tenured1), cx);
    CHECK(cache.init());

    cache.put(tenured1, 1);
    cache.put(tenured2, 2);
    cache.put(nursery1, 3);
    cache.put(nursery2, 4);

    JS_GC(cx);
    CHECK(cache.has(tenured1));
    CHECK(cache.has(tenured2));
    CHECK(cache.has(nursery1));
    CHECK(cache.has(nursery2));

    tenured2 = nursery2 = nullptr;
    JS_GC(cx);
    CHECK(cache.has(tenured1));
    CHECK(cache.has(nursery1));
    CHECK(cache.count() == 2);

    return true;
}
END_TEST(testWeakCacheMap)

// Exercise WeakCache<GCVector>.
BEGIN_TEST(testWeakCacheGCVector)
{
    // Create two objects tenured and two in the nursery. If zeal is on,
    // this may fail and we'll get more tenured objects. That's fine:
    // the test will continue to work, it will just not test as much.
    JS::RootedObject tenured1(cx, JS_NewPlainObject(cx));
    JS::RootedObject tenured2(cx, JS_NewPlainObject(cx));
    JS_GC(cx);
    JS::RootedObject nursery1(cx, JS_NewPlainObject(cx));
    JS::RootedObject nursery2(cx, JS_NewPlainObject(cx));

    using ObjectVector = js::GCVector<JS::Heap<JSObject*>>;
    using Cache = JS::WeakCache<ObjectVector>;
    auto cache = Cache(JS::GetObjectZone(tenured1), ObjectVector(cx));

    CHECK(cache.append(tenured1));
    CHECK(cache.append(tenured2));
    CHECK(cache.append(nursery1));
    CHECK(cache.append(nursery2));

    JS_GC(cx);
    CHECK(cache.get().length() == 4);
    CHECK(cache.get()[0] == tenured1);
    CHECK(cache.get()[1] == tenured2);
    CHECK(cache.get()[2] == nursery1);
    CHECK(cache.get()[3] == nursery2);

    tenured2 = nursery2 = nullptr;
    JS_GC(cx);
    CHECK(cache.get().length() == 2);
    CHECK(cache.get()[0] == tenured1);
    CHECK(cache.get()[1] == nursery1);

    return true;
}
END_TEST(testWeakCacheGCVector)
