/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Leary <cdleary@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ParseMapPool_inl_h__
#define ParseMapPool_inl_h__

#include "jscntxt.h"
#include "jsparse.h" /* Need sizeof(JSDefinition). */

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
inline AtomDOHMap *
ParseMapPool::acquire<AtomDOHMap>()
{
    return reinterpret_cast<AtomDOHMap *>(allocate());
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

inline JSDefinition *
AtomDecls::lookupFirst(JSAtom *atom)
{
    JS_ASSERT(map);
    AtomDOHPtr p = map->lookup(atom);
    if (!p)
        return NULL;
    if (p.value().isHeader()) {
        /* Just return the head defn. */
        return p.value().header()->defn;
    }
    return p.value().defn();
}

inline MultiDeclRange
AtomDecls::lookupMulti(JSAtom *atom)
{
    JS_ASSERT(map);
    AtomDOHPtr p = map->lookup(atom);
    if (!p)
        return MultiDeclRange((JSDefinition *) NULL);

    DefnOrHeader &doh = p.value();
    if (doh.isHeader())
        return MultiDeclRange(doh.header());
    return MultiDeclRange(doh.defn());
}

inline bool
AtomDecls::addUnique(JSAtom *atom, JSDefinition *defn)
{
    JS_ASSERT(map);
    AtomDOHAddPtr p = map->lookupForAdd(atom);
    if (p) {
        JS_ASSERT(!p.value().isHeader());
        p.value() = DefnOrHeader(defn);
        return true;
    }
    return map->add(p, atom, DefnOrHeader(defn));
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
    map = cx->parseMapPool().acquire<AtomDOHMap>();
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
