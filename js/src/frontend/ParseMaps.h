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

typedef InlineMap<JSAtom *, DefinitionList, 24> AtomDefnListMap;

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

    void release(AtomDefnListMap *map) {
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

/*
 * A nonempty list containing one or more pointers to Definitions.
 *
 * By far the most common case is that the list contains exactly one
 * Definition, so the implementation is optimized for that case.
 *
 * Nodes for the linked list (if any) are allocated from the tempPool of a
 * context the caller passes into pushFront and pushBack. This means the
 * DefinitionList does not own the memory for the nodes: the JSContext does.
 * As a result, DefinitionList is a POD type; it can be safely and cheaply
 * copied.
 */
class DefinitionList
{
  public:
    class Range;

  private:
    friend class Range;

    /* A node in a linked list of Definitions. */
    struct Node
    {
        Definition *defn;
        Node *next;

        Node(Definition *defn, Node *next) : defn(defn), next(next) {}
    };

    union {
        Definition *defn;
        Node *head;
        uintptr_t bits;
    } u;

    Definition *defn() const {
        JS_ASSERT(!isMultiple());
        return u.defn;
    }

    Node *firstNode() const {
        JS_ASSERT(isMultiple());
        return (Node *) (u.bits & ~0x1);
    }

    static Node *
    allocNode(JSContext *cx, Definition *head, Node *tail);
            
  public:
    class Range
    {
        friend class DefinitionList;

        Node *node;
        Definition *defn;

        explicit Range(const DefinitionList &list) {
            if (list.isMultiple()) {
                node = list.firstNode();
                defn = node->defn;
            } else {
                node = NULL;
                defn = list.defn();
            }
        }

      public:
        /* An empty Range. */
        Range() : node(NULL), defn(NULL) {}

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

    DefinitionList() {
        u.bits = 0;
    }

    explicit DefinitionList(Definition *defn) {
        u.defn = defn;
        JS_ASSERT(!isMultiple());
    }

    explicit DefinitionList(Node *node) {
        u.head = node;
        u.bits |= 0x1;
        JS_ASSERT(isMultiple());
    }

    bool isMultiple() const { return (u.bits & 0x1) != 0; }

    Definition *front() {
        return isMultiple() ? firstNode()->defn : defn();
    }

    /*
     * If there are multiple Definitions in this list, remove the first and
     * return true. Otherwise there is exactly one Definition in the list; do
     * nothing and return false.
     */
    bool popFront() {
        if (!isMultiple())
            return false;

        Node *node = firstNode();
        Node *next = node->next;
        if (next->next)
            *this = DefinitionList(next);
        else
            *this = DefinitionList(next->defn);
        return true;
    }

    /*
     * Add a definition to the front of this list.
     *
     * Return true on success. On OOM, report on cx and return false.
     */
    bool pushFront(JSContext *cx, Definition *val);

    /* Like pushFront, but add the given val to the end of the list. */
    bool pushBack(JSContext *cx, Definition *val);

    /* Overwrite the first Definition in the list. */
    void setFront(Definition *val) {
        if (isMultiple())
            firstNode()->defn = val;
        else
            *this = DefinitionList(val);
    }

    Range all() const { return Range(*this); }

#ifdef DEBUG
    void dump();
#endif
};

namespace tl {

template <> struct IsPodType<DefinitionList> {
    static const bool result = true;
};

} /* namespace tl */

/*
 * Multimap for function-scope atom declarations.
 *
 * Wraps an internal DefinitionList map with multi-map functionality.
 *
 * In the common case, no block scoping is used, and atoms have a single
 * associated definition. In the uncommon (block scoping) case, we map the atom
 * to a chain of definition nodes.
 */
class AtomDecls
{
    /* AtomDeclsIter needs to get at the DefnListMap directly. */
    friend class AtomDeclsIter;

    JSContext   *cx;
    AtomDefnListMap  *map;

    AtomDecls(const AtomDecls &other) MOZ_DELETE;
    void operator=(const AtomDecls &other) MOZ_DELETE;

  public:
    explicit AtomDecls(JSContext *cx) : cx(cx), map(NULL) {}

    ~AtomDecls();

    bool init();

    void clear() {
        map->clear();
    }

    /* Return the definition at the head of the chain for |atom|. */
    inline Definition *lookupFirst(JSAtom *atom);

    /* Perform a lookup that can iterate over the definitions associated with |atom|. */
    inline DefinitionList::Range lookupMulti(JSAtom *atom);

    /* Add-or-update a known-unique definition for |atom|. */
    inline bool addUnique(JSAtom *atom, Definition *defn);
    bool addShadow(JSAtom *atom, Definition *defn);
    bool addHoist(JSAtom *atom, Definition *defn);

    /* Updating the definition for an entry that is known to exist is infallible. */
    void updateFirst(JSAtom *atom, Definition *defn) {
        JS_ASSERT(map);
        AtomDefnListMap::Ptr p = map->lookup(atom);
        JS_ASSERT(p);
        p.value().setFront(defn);
    }

    /* Remove the node at the head of the chain for |atom|. */
    void remove(JSAtom *atom) {
        JS_ASSERT(map);
        AtomDefnListMap::Ptr p = map->lookup(atom);
        if (!p)
            return;

        DefinitionList &list = p.value();
        if (!list.popFront()) {
            map->remove(p);
            return;
        }
    }

    AtomDefnListMap::Range all() {
        JS_ASSERT(map);
        return map->all();
    }

#ifdef DEBUG
    void dump();
#endif
};

typedef AtomDefnMap::Range      AtomDefnRange;
typedef AtomDefnMap::AddPtr     AtomDefnAddPtr;
typedef AtomDefnMap::Ptr        AtomDefnPtr;
typedef AtomIndexMap::AddPtr    AtomIndexAddPtr;
typedef AtomIndexMap::Ptr       AtomIndexPtr;
typedef AtomDefnListMap::Ptr    AtomDefnListPtr;
typedef AtomDefnListMap::AddPtr AtomDefnListAddPtr;
typedef AtomDefnListMap::Range  AtomDefnListRange;

} /* namepsace js */

#endif
