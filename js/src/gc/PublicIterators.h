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

    CompartmentsInZoneIter()
      : zone(nullptr), it(nullptr)
    {}

    // This is for the benefit of CompartmentsIterT::comp.
    friend class mozilla::Maybe<CompartmentsInZoneIter>;
};

// Note: this class currently assumes there's a single realm per compartment.
class RealmsInZoneIter
{
    CompartmentsInZoneIter comp;

  public:
    explicit RealmsInZoneIter(JS::Zone* zone)
      : comp(zone)
    {}

    bool done() const {
        return comp.done();
    }
    void next() {
        MOZ_ASSERT(!done());
        comp.next();
    }

    JS::Realm* get() const {
        return JS::GetRealmForCompartment(comp.get());
    }

    operator JS::Realm*() const { return get(); }
    JS::Realm* operator->() const { return get(); }
};

// This iterator iterates over all the compartments in a given set of zones. The
// set of zones is determined by iterating ZoneIterT.
template<class ZonesIterT>
class CompartmentsIterT
{
    gc::AutoEnterIteration iterMarker;
    ZonesIterT zone;
    mozilla::Maybe<CompartmentsInZoneIter> comp;

  public:
    explicit CompartmentsIterT(JSRuntime* rt)
      : iterMarker(&rt->gc), zone(rt)
    {
        if (zone.done())
            comp.emplace();
        else
            comp.emplace(zone);
    }

    CompartmentsIterT(JSRuntime* rt, ZoneSelector selector)
      : iterMarker(&rt->gc), zone(rt, selector)
    {
        if (zone.done())
            comp.emplace();
        else
            comp.emplace(zone);
    }

    bool done() const { return zone.done(); }

    void next() {
        MOZ_ASSERT(!done());
        MOZ_ASSERT(!comp.ref().done());
        comp->next();
        if (comp->done()) {
            comp.reset();
            zone.next();
            if (!zone.done())
                comp.emplace(zone);
        }
    }

    JSCompartment* get() const {
        MOZ_ASSERT(!done());
        return *comp;
    }

    operator JSCompartment*() const { return get(); }
    JSCompartment* operator->() const { return get(); }
};

using CompartmentsIter = CompartmentsIterT<ZonesIter>;

// This iterator iterates over all the realms in a given set of zones. The
// set of zones is determined by iterating ZoneIterT.
template<class ZonesIterT>
class RealmsIterT
{
    gc::AutoEnterIteration iterMarker;
    CompartmentsIterT<ZonesIterT> comp;

  public:
    explicit RealmsIterT(JSRuntime* rt)
      : iterMarker(&rt->gc), comp(rt)
    {}

    RealmsIterT(JSRuntime* rt, ZoneSelector selector)
      : iterMarker(&rt->gc), comp(rt, selector)
    {}

    bool done() const { return comp.done(); }

    void next() {
        MOZ_ASSERT(!done());
        comp.next();
    }

    JS::Realm* get() const {
        MOZ_ASSERT(!done());
        return JS::GetRealmForCompartment(comp.get());
    }

    operator JS::Realm*() const { return get(); }
    JS::Realm* operator->() const { return get(); }
};

using RealmsIter = RealmsIterT<ZonesIter>;

} // namespace js

#endif // gc_PublicIterators_h
