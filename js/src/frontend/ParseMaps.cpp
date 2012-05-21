/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParseMaps-inl.h"
#include "jscompartment.h"

using namespace js;

void
ParseMapPool::checkInvariants()
{
    /*
     * Having all values be of the same size permits us to easily reuse the
     * allocated space for each of the map types.
     */
    JS_STATIC_ASSERT(sizeof(Definition *) == sizeof(jsatomid));
    JS_STATIC_ASSERT(sizeof(Definition *) == sizeof(DefnOrHeader));
    JS_STATIC_ASSERT(sizeof(AtomDefnMap::Entry) == sizeof(AtomIndexMap::Entry));
    JS_STATIC_ASSERT(sizeof(AtomDefnMap::Entry) == sizeof(AtomDOHMap::Entry));
    JS_STATIC_ASSERT(sizeof(AtomMapT::Entry) == sizeof(AtomDOHMap::Entry));
    /* Ensure that the HasTable::clear goes quickly via memset. */
    JS_STATIC_ASSERT(tl::IsPodType<AtomIndexMap::WordMap::Entry>::result);
    JS_STATIC_ASSERT(tl::IsPodType<AtomDOHMap::WordMap::Entry>::result);
    JS_STATIC_ASSERT(tl::IsPodType<AtomDefnMap::WordMap::Entry>::result);
}

void
ParseMapPool::purgeAll()
{
    for (void **it = all.begin(), **end = all.end(); it != end; ++it)
        cx->delete_<AtomMapT>(asAtomMap(*it));

    all.clearAndFree();
    recyclable.clearAndFree();
}

void *
ParseMapPool::allocateFresh()
{
    size_t newAllLength = all.length() + 1;
    if (!all.reserve(newAllLength) || !recyclable.reserve(newAllLength))
        return NULL;

    AtomMapT *map = cx->new_<AtomMapT>(cx);
    if (!map)
        return NULL;

    all.infallibleAppend(map);
    return (void *) map;
}

#ifdef DEBUG
void
AtomDecls::dump()
{
    for (AtomDOHRange r = map->all(); !r.empty(); r.popFront()) {
        fprintf(stderr, "atom: ");
        js_DumpAtom(r.front().key());
        const DefnOrHeader &doh = r.front().value();
        if (doh.isHeader()) {
            AtomDeclNode *node = doh.header();
            do {
                fprintf(stderr, "  node: %p\n", (void *) node);
                fprintf(stderr, "    defn: %p\n", (void *) node->defn);
                node = node->next;
            } while (node);
        } else {
            fprintf(stderr, "  defn: %p\n", (void *) doh.defn());
        }
    }
}

void
DumpAtomDefnMap(const AtomDefnMapPtr &map)
{
    if (map->empty()) {
        fprintf(stderr, "empty\n");
        return;
    }

    for (AtomDefnRange r = map->all(); !r.empty(); r.popFront()) {
        fprintf(stderr, "atom: ");
        js_DumpAtom(r.front().key());
        fprintf(stderr, "defn: %p\n", (void *) r.front().value());
    }
}
#endif

AtomDeclNode *
AtomDecls::allocNode(Definition *defn)
{
    AtomDeclNode *p = cx->tempLifoAlloc().new_<AtomDeclNode>(defn);
    if (!p) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }
    return p;
}

bool
AtomDecls::addShadow(JSAtom *atom, Definition *defn)
{
    AtomDeclNode *node = allocNode(defn);
    if (!node)
        return false;

    AtomDOHAddPtr p = map->lookupForAdd(atom);
    if (!p)
        return map->add(p, atom, DefnOrHeader(node));

    AtomDeclNode *toShadow;
    if (p.value().isHeader()) {
        toShadow = p.value().header();
    } else {
        toShadow = allocNode(p.value().defn());
        if (!toShadow)
            return false;
    }
    node->next = toShadow;
    p.value() = DefnOrHeader(node);
    return true;
}

AtomDeclNode *
AtomDecls::lastAsNode(DefnOrHeader *doh)
{
    if (doh->isHeader()) {
        AtomDeclNode *last = doh->header();
        while (last->next)
            last = last->next;
        return last;
    }

    /* Otherwise, we need to turn the existing defn into a node. */
    AtomDeclNode *node = allocNode(doh->defn());
    if (!node)
        return NULL;
    *doh = DefnOrHeader(node);
    return node;
}

bool
AtomDecls::addHoist(JSAtom *atom, Definition *defn)
{
    AtomDeclNode *node = allocNode(defn);
    if (!node)
        return false;

    AtomDOHAddPtr p = map->lookupForAdd(atom);
    if (p) {
        AtomDeclNode *last = lastAsNode(&p.value());
        if (!last)
            return false;
        last->next = node;
        return true;
    }

    return map->add(p, atom, DefnOrHeader(node));
}
