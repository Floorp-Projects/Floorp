/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParseMapPool_inl_h__
#define ParseMapPool_inl_h__

#include "jscntxt.h"

#include "frontend/ParseNode.h" /* Need sizeof(js::Definition). */
#include "frontend/ParseMaps.h"

namespace js {
namespace frontend {

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

template <typename ParseHandler>
inline typename ParseHandler::DefinitionNode
AtomDecls<ParseHandler>::lookupFirst(JSAtom *atom) const
{
    JS_ASSERT(map);
    AtomDefnListPtr p = map->lookup(atom);
    if (!p)
        return ParseHandler::nullDefinition();
    return p.value().front<ParseHandler>();
}

template <typename ParseHandler>
inline DefinitionList::Range
AtomDecls<ParseHandler>::lookupMulti(JSAtom *atom) const
{
    JS_ASSERT(map);
    if (AtomDefnListPtr p = map->lookup(atom))
        return p.value().all();
    return DefinitionList::Range();
}

template <typename ParseHandler>
inline bool
AtomDecls<ParseHandler>::addUnique(JSAtom *atom, DefinitionNode defn)
{
    JS_ASSERT(map);
    AtomDefnListAddPtr p = map->lookupForAdd(atom);
    if (!p)
        return map->add(p, atom, DefinitionList(ParseHandler::definitionToBits(defn)));
    JS_ASSERT(!p.value().isMultiple());
    p.value() = DefinitionList(ParseHandler::definitionToBits(defn));
    return true;
}

template <class Map>
inline bool
AtomThingMapPtr<Map>::ensureMap(JSContext *cx)
{
    if (map_)
        return true;
    map_ = cx->runtime->parseMapPool.acquire<Map>();
    return !!map_;
}

template <class Map>
inline void
AtomThingMapPtr<Map>::releaseMap(JSContext *cx)
{
    if (!map_)
        return;
    cx->runtime->parseMapPool.release(map_);
    map_ = NULL;
}

template <typename ParseHandler>
inline bool
AtomDecls<ParseHandler>::init()
{
    map = cx->runtime->parseMapPool.acquire<AtomDefnListMap>();
    return map;
}

template <typename ParseHandler>
inline
AtomDecls<ParseHandler>::~AtomDecls()
{
    if (map)
        cx->runtime->parseMapPool.release(map);
}

} /* namespace frontend */
} /* namespace js */

#endif
