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
    JS_STATIC_ASSERT(sizeof(JSDefinition *) == sizeof(jsatomid));
    JS_STATIC_ASSERT(sizeof(JSDefinition *) == sizeof(DefnOrHeader));
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
AtomDecls::allocNode(JSDefinition *defn)
{
    AtomDeclNode *p = cx->tempLifoAlloc().new_<AtomDeclNode>(defn);
    if (!p) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }
    return p;
}

bool
AtomDecls::addShadow(JSAtom *atom, JSDefinition *defn)
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
AtomDecls::addHoist(JSAtom *atom, JSDefinition *defn)
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
