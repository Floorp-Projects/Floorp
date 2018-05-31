/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Iterators for various data structures.
 */

#ifndef gc_PublicIterators_h
#define gc_PublicIterators_h

#include "mozilla/Maybe.h"

#include "gc/Zone.h"
#include "vm/JSCompartment.h"

namespace js {

// Using the atoms zone without holding the exclusive access lock is dangerous
// because worker threads may be using it simultaneously. Therefore, it's
// better to skip the atoms zone when iterating over zones. If you need to
// iterate over the atoms zone, consider taking the exclusive access lock first.
enum ZoneSelector {
    WithAtoms,
    SkipAtoms
};

// Iterate over all zones in the runtime, except those which may be in use by
// parse threads.
class ZonesIter
{
    gc::AutoEnterIteration iterMarker;
    JS::Zone* atomsZone;
    JS::Zone** it;
    JS::Zone** end;

  public:
    ZonesIter(JSRuntime* rt, ZoneSelector selector)
      : iterMarker(&rt->gc),
        atomsZone(selector == WithAtoms ? rt->gc.atomsZone.ref() : nullptr),
        it(rt->gc.zones().begin()),
        end(rt->gc.zones().end())
    {
        if (!atomsZone)
            skipHelperThreadZones();
    }

    bool done() const { return !atomsZone && it == end; }

    void next() {
        MOZ_ASSERT(!done());
        if (atomsZone)
            atomsZone = nullptr;
        else
            it++;
        skipHelperThreadZones();
    }

    void skipHelperThreadZones() {
        while (!done() && get()->usedByHelperThread())
            it++;
    }

    JS::Zone* get() const {
        MOZ_ASSERT(!done());
        return atomsZone ? atomsZone : *it;
    }

    operator JS::Zone*() const { return get(); }
    JS::Zone* operator->() const { return get(); }
};

struct CompartmentsInZoneIter
{
    using ItemType = JSCompartment;

    explicit CompartmentsInZoneIter(JS::Zone* zone) : zone(zone) {
        it = zone->compartments().begin();
    }

    bool done() const {
        MOZ_ASSERT(it);
        return it < zone->compartments().begin() ||
               it >= zone->compartments().end();
    }
    void next() {
        MOZ_ASSERT(!done());
        it++;
    }

    JSCompartment* get() const {
        MOZ_ASSERT(it);
        return *it;
    }

    operator JSCompartment*() const { return get(); }
    JSCompartment* operator->() const { return get(); }

  private:
    JS::Zone* zone;
    JSCompartment** it;
};

class RealmsInCompartmentIter
{
    JSCompartment* comp;
    JS::Realm** it;

  public:
    explicit RealmsInCompartmentIter(JSCompartment* comp)
      : comp(comp)
    {
        it = comp->realms().begin();
    }

    bool done() const {
        MOZ_ASSERT(it);
        return it < comp->realms().begin() ||
               it >= comp->realms().end();
    }
    void next() {
        MOZ_ASSERT(!done());
        it++;
    }

    JS::Realm* get() const {
        MOZ_ASSERT(!done());
        return *it;
    }

    operator JS::Realm*() const { return get(); }
    JS::Realm* operator->() const { return get(); }
};

class RealmsInZoneIter
{
    CompartmentsInZoneIter comp;
    mozilla::Maybe<RealmsInCompartmentIter> realm;

  public:
    using ItemType = JS::Realm;

    explicit RealmsInZoneIter(JS::Zone* zone)
      : comp(zone)
    {
        settleOnCompartment();
    }

    void settleOnCompartment() {
        if (!comp.done()) {
            realm.emplace(comp.get());
            MOZ_ASSERT(!realm->done(), "compartment must have at least one realm");
        }
    }

    bool done() const {
        MOZ_ASSERT(comp.done() == realm.isNothing());
        return comp.done();
    }
    void next() {
        MOZ_ASSERT(!done());

        realm->next();

        if (realm->done()) {
            realm.reset();
            comp.next();
            settleOnCompartment();
        }
    }

    JS::Realm* get() const {
        return realm->get();
    }

    operator JS::Realm*() const { return get(); }
    JS::Realm* operator->() const { return get(); }
};

// This iterator iterates over all the compartments or realms in a given set of
// zones. The set of zones is determined by iterating ZoneIterT. The set of
// compartments or realms is determined by InnerIterT.
template<class ZonesIterT, class InnerIterT>
class CompartmentsOrRealmsIterT
{
    using T = typename InnerIterT::ItemType;

    gc::AutoEnterIteration iterMarker;
    ZonesIterT zone;
    mozilla::Maybe<InnerIterT> inner;

  public:
    explicit CompartmentsOrRealmsIterT(JSRuntime* rt)
      : iterMarker(&rt->gc), zone(rt, SkipAtoms)
    {
        if (!zone.done())
            inner.emplace(zone);
    }

    bool done() const { return zone.done(); }

    void next() {
        MOZ_ASSERT(!done());
        MOZ_ASSERT(!inner.ref().done());
        inner->next();
        if (inner->done()) {
            inner.reset();
            zone.next();
            if (!zone.done())
                inner.emplace(zone);
        }
    }

    T* get() const {
        MOZ_ASSERT(!done());
        return *inner;
    }

    operator T*() const { return get(); }
    T* operator->() const { return get(); }
};

using CompartmentsIter = CompartmentsOrRealmsIterT<ZonesIter, CompartmentsInZoneIter>;
using RealmsIter = CompartmentsOrRealmsIterT<ZonesIter, RealmsInZoneIter>;

} // namespace js

#endif // gc_PublicIterators_h
