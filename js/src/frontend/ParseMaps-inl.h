/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParseMapPool_inl_h__
#define ParseMapPool_inl_h__

#include "jscntxt.h"

#include "frontend/ParseNode.h" /* Need sizeof(js::Definition). */

#include "ParseMaps.h"

namespace js {

template <>
inline AtomDefnMap *
ParseMapPool::acquire<AtomDefnMap>()
{
    return reinterpret_cast<AtomDefnMap *>(allocate());
}

template <>
inline AtomIndexMap *
ParseMapPool::acquire<AtomIndexMap>()
{
    return reinterpret_cast<AtomIndexMap *>(allocate());
}

template <>
inline AtomDefnListMap *
ParseMapPool::acquire<AtomDefnListMap>()
{
    return reinterpret_cast<AtomDefnListMap *>(allocate());
}

inline void *
ParseMapPool::allocate()
{
    if (recyclable.empty())
        return allocateFresh();

    void *map = recyclable.popCopy();
    asAtomMap(map)->clear();
    return map;
}

inline Definition *
AtomDecls::lookupFirst(JSAtom *atom)
{
    JS_ASSERT(map);
    AtomDefnListPtr p = map->lookup(atom);
    if (!p)
        return NULL;
    return p.value().front();
}

inline DefinitionList::Range
AtomDecls::lookupMulti(JSAtom *atom)
{
    JS_ASSERT(map);
    if (AtomDefnListPtr p = map->lookup(atom))
        return p.value().all();
    return DefinitionList::Range();
}

inline bool
AtomDecls::addUnique(JSAtom *atom, Definition *defn)
{
    JS_ASSERT(map);
    AtomDefnListAddPtr p = map->lookupForAdd(atom);
    if (!p)
        return map->add(p, atom, DefinitionList(defn));
    JS_ASSERT(!p.value().isMultiple());
    p.value() = DefinitionList(defn);
    return true;
}

template <class Map>
inline bool
AtomThingMapPtr<Map>::ensureMap(JSContext *cx)
{
    if (map_)
        return true;
    map_ = cx->parseMapPool().acquire<Map>();
    return !!map_;
}

template <class Map>
inline void
AtomThingMapPtr<Map>::releaseMap(JSContext *cx)
{
    if (!map_)
        return;
    cx->parseMapPool().release(map_);
    map_ = NULL;
}

inline bool
AtomDecls::init()
{
    map = cx->parseMapPool().acquire<AtomDefnListMap>();
    return map;
}

inline
AtomDecls::~AtomDecls()
{
    if (map)
        cx->parseMapPool().release(map);
}

} /* namespace js */

#endif
