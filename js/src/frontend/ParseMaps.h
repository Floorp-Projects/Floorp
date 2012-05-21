/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParseMaps_h__
#define ParseMaps_h__

#include "mozilla/Attributes.h"

#include "ds/InlineMap.h"
#include "js/HashTable.h"
#include "js/Vector.h"

namespace js {

struct Definition;

/*
 * A pool that permits the reuse of the backing storage for the defn, index, or
 * defn-or-header (multi) maps.
 *
 * The pool owns all the maps that are given out, and is responsible for
 * relinquishing all resources when |purgeAll| is triggered.
 */
class ParseMapPool
{
    typedef Vector<void *, 32, SystemAllocPolicy> RecyclableMaps;

    RecyclableMaps      all;
    RecyclableMaps      recyclable;
    JSContext           *cx;

    void checkInvariants();

    void recycle(void *map) {
        JS_ASSERT(map);
#ifdef DEBUG
        bool ok = false;
        /* Make sure the map is in |all| but not already in |recyclable|. */
        for (void **it = all.begin(), **end = all.end(); it != end; ++it) {
            if (*it == map) {
                ok = true;
                break;
            }
        }
        JS_ASSERT(ok);
        for (void **it = recyclable.begin(), **end = recyclable.end(); it != end; ++it)
            JS_ASSERT(*it != map);
#endif
        JS_ASSERT(recyclable.length() < all.length());
        recyclable.infallibleAppend(map); /* Reserved in allocateFresh. */
    }

    void *allocateFresh();
    void *allocate();

    /* Arbitrary atom map type, that has keys and values of the same kind. */
    typedef AtomIndexMap AtomMapT;

    static AtomMapT *asAtomMap(void *ptr) {
        return reinterpret_cast<AtomMapT *>(ptr);
    }

  public:
    explicit ParseMapPool(JSContext *cx) : cx(cx) {}

    ~ParseMapPool() {
        purgeAll();
    }

    void purgeAll();

    bool empty() const {
        return all.empty();
    }

    /* Fallibly aquire one of the supported map types from the pool. */
    template <typename T>
    T *acquire();

    /* Release one of the supported map types back to the pool. */

    void release(AtomIndexMap *map) {
        recycle((void *) map);
    }

    void release(AtomDefnMap *map) {
        recycle((void *) map);
    }

    void release(AtomDOHMap *map) {
        recycle((void *) map);
    }
}; /* ParseMapPool */

/*
 * N.B. This is a POD-type so that it can be included in the ParseNode union.
 * If possible, use the corresponding |OwnedAtomThingMapPtr| variant.
 */
template <class Map>
struct AtomThingMapPtr
{
    Map *map_;

    void init() { clearMap(); }

    bool ensureMap(JSContext *cx);
    void releaseMap(JSContext *cx);

    bool hasMap() const { return map_; }
    Map *getMap() { return map_; }
    void setMap(Map *newMap) { JS_ASSERT(!map_); map_ = newMap; }
    void clearMap() { map_ = NULL; }

    Map *operator->() { return map_; }
    const Map *operator->() const { return map_; }
    Map &operator*() const { return *map_; }
};

struct AtomDefnMapPtr : public AtomThingMapPtr<AtomDefnMap>
{
    JS_ALWAYS_INLINE
    Definition *lookupDefn(JSAtom *atom) {
        AtomDefnMap::Ptr p = map_->lookup(atom);
        return p ? p.value() : NULL;
    }
};

typedef AtomThingMapPtr<AtomIndexMap> AtomIndexMapPtr;

/*
 * Wrapper around an AtomThingMapPtr (or its derivatives) that automatically
 * releases a map on destruction, if one has been acquired.
 */
template <typename AtomThingMapPtrT>
class OwnedAtomThingMapPtr : public AtomThingMapPtrT
{
    JSContext *cx;

  public:
    explicit OwnedAtomThingMapPtr(JSContext *cx) : cx(cx) {
        AtomThingMapPtrT::init();
    }

    ~OwnedAtomThingMapPtr() {
        AtomThingMapPtrT::releaseMap(cx);
    }
};

typedef OwnedAtomThingMapPtr<AtomDefnMapPtr> OwnedAtomDefnMapPtr;
typedef OwnedAtomThingMapPtr<AtomIndexMapPtr> OwnedAtomIndexMapPtr;

/* Node structure for chaining in AtomDecls. */
struct AtomDeclNode
{
    Definition *defn;
    AtomDeclNode *next;

    explicit AtomDeclNode(Definition *defn)
      : defn(defn), next(NULL)
    {}
};

/*
 * Tagged union of a Definition and an AtomDeclNode, for use in AtomDecl's
 * internal map.
 */
class DefnOrHeader
{
    union {
        Definition    *defn;
        AtomDeclNode    *head;
        uintptr_t       bits;
    } u;

  public:
    DefnOrHeader() {
        u.bits = 0;
    }

    explicit DefnOrHeader(Definition *defn) {
        u.defn = defn;
        JS_ASSERT(!isHeader());
    }

    explicit DefnOrHeader(AtomDeclNode *node) {
        u.head = node;
        u.bits |= 0x1;
        JS_ASSERT(isHeader());
    }

    bool isHeader() const {
        return u.bits & 0x1;
    }

    Definition *defn() const {
        JS_ASSERT(!isHeader());
        return u.defn;
    }

    AtomDeclNode *header() const {
        JS_ASSERT(isHeader());
        return (AtomDeclNode *) (u.bits & ~0x1);
    }

#ifdef DEBUG
    void dump();
#endif
};

namespace tl {

template <> struct IsPodType<DefnOrHeader> {
    static const bool result = true;
};

} /* namespace tl */

/*
 * Multimap for function-scope atom declarations.
 *
 * Wraps an internal DeclOrHeader map with multi-map functionality.
 *
 * In the common case, no block scoping is used, and atoms have a single
 * associated definition. In the uncommon (block scoping) case, we map the atom
 * to a chain of definition nodes.
 */
class AtomDecls
{
    /* AtomDeclsIter needs to get at the DOHMap directly. */
    friend class AtomDeclsIter;

    JSContext   *cx;
    AtomDOHMap  *map;

    AtomDecls(const AtomDecls &other) MOZ_DELETE;
    void operator=(const AtomDecls &other) MOZ_DELETE;

    AtomDeclNode *allocNode(Definition *defn);

    /*
     * Fallibly return the value in |doh| as a node.
     * Update the defn currently occupying |doh| to a node if necessary.
     */
    AtomDeclNode *lastAsNode(DefnOrHeader *doh);

  public:
    explicit AtomDecls(JSContext *cx)
      : cx(cx), map(NULL)
    {}

    ~AtomDecls();

    bool init();

    void clear() {
        map->clear();
    }

    /* Return the definition at the head of the chain for |atom|. */
    inline Definition *lookupFirst(JSAtom *atom);

    /* Perform a lookup that can iterate over the definitions associated with |atom|. */
    inline MultiDeclRange lookupMulti(JSAtom *atom);

    /* Add-or-update a known-unique definition for |atom|. */
    inline bool addUnique(JSAtom *atom, Definition *defn);
    bool addShadow(JSAtom *atom, Definition *defn);
    bool addHoist(JSAtom *atom, Definition *defn);

    /* Updating the definition for an entry that is known to exist is infallible. */
    void updateFirst(JSAtom *atom, Definition *defn) {
        JS_ASSERT(map);
        AtomDOHMap::Ptr p = map->lookup(atom);
        JS_ASSERT(p);
        if (p.value().isHeader())
            p.value().header()->defn = defn;
        else
            p.value() = DefnOrHeader(defn);
    }

    /* Remove the node at the head of the chain for |atom|. */
    void remove(JSAtom *atom) {
        JS_ASSERT(map);
        AtomDOHMap::Ptr p = map->lookup(atom);
        if (!p)
            return;

        DefnOrHeader &doh = p.value();
        if (!doh.isHeader()) {
            map->remove(p);
            return;
        }

        AtomDeclNode *node = doh.header();
        AtomDeclNode *newHead = node->next;
        if (newHead)
            p.value() = DefnOrHeader(newHead);
        else
            map->remove(p);
    }

    AtomDOHMap::Range all() {
        JS_ASSERT(map);
        return map->all();
    }

#ifdef DEBUG
    void dump();
#endif
};

/*
 * Lookup state tracker for those situations where the caller wants to traverse
 * multiple definitions associated with a single atom. This occurs due to block
 * scoping.
 */
class MultiDeclRange
{
    friend class AtomDecls;

    AtomDeclNode *node;
    Definition *defn;

    explicit MultiDeclRange(Definition *defn) : node(NULL), defn(defn) {}
    explicit MultiDeclRange(AtomDeclNode *node) : node(node), defn(node->defn) {}

  public:
    void popFront() {
        JS_ASSERT(!empty());
        if (!node) {
            defn = NULL;
            return;
        }
        node = node->next;
        defn = node ? node->defn : NULL;
    }

    Definition *front() {
        JS_ASSERT(!empty());
        return defn;
    }

    bool empty() const {
        JS_ASSERT_IF(!defn, !node);
        return !defn;
    }
};

/* Iterates over all the definitions in an AtomDecls. */
class AtomDeclsIter
{
    AtomDOHMap::Range   r;     /* Range over the map. */
    AtomDeclNode        *link; /* Optional next node in the current atom's chain. */

  public:
    explicit AtomDeclsIter(AtomDecls *decls) : r(decls->all()), link(NULL) {}

    Definition *next() {
        if (link) {
            JS_ASSERT(link != link->next);
            Definition *result = link->defn;
            link = link->next;
            JS_ASSERT(result);
            return result;
        }

        if (r.empty())
            return NULL;

        const DefnOrHeader &doh = r.front().value();
        r.popFront();

        if (!doh.isHeader())
            return doh.defn();

        JS_ASSERT(!link);
        AtomDeclNode *node = doh.header();
        link = node->next;
        return node->defn;
    }
};

typedef AtomDefnMap::Range      AtomDefnRange;
typedef AtomDefnMap::AddPtr     AtomDefnAddPtr;
typedef AtomDefnMap::Ptr        AtomDefnPtr;
typedef AtomIndexMap::AddPtr    AtomIndexAddPtr;
typedef AtomIndexMap::Ptr       AtomIndexPtr;
typedef AtomDOHMap::Ptr         AtomDOHPtr;
typedef AtomDOHMap::AddPtr      AtomDOHAddPtr;
typedef AtomDOHMap::Range       AtomDOHRange;

} /* namepsace js */

#endif
