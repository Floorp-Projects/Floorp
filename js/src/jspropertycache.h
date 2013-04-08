/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=98:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jspropertycache_h___
#define jspropertycache_h___

#include "mozilla/PodOperations.h"

#include "jsapi.h"
#include "jsprvtd.h"
#include "jstypes.h"

#include "vm/String.h"

namespace js {

/*
 * Property cache with structurally typed capabilities for invalidation, for
 * polymorphic callsite method/get/set speedups.  For details, see
 * <https://developer.mozilla.org/en/SpiderMonkey/Internals/Property_cache>.
 */
class PropertyCache;

struct PropertyCacheEntry
{
    jsbytecode    *kpc;           /* pc of cache-testing bytecode */
    Shape         *kshape;        /* shape of direct (key) object */
    Shape         *pshape;        /* shape of owning object */
    Shape         *prop;          /* shape of accessed property */

    friend class PropertyCache;

  private:
    /* Index into the prototype chain from the object for this entry. */
    uint8_t       protoIndex;

  public:
    static const size_t MaxProtoIndex = 15;

    /*
     * True iff the property lookup will find an own property on the object if
     * the entry matches.
     *
     * This test is applicable only to property lookups, not to identifier
     * lookups.  It is meaningless to ask this question of an entry for an
     * identifier lookup.
     */
    bool isOwnPropertyHit() const { return protoIndex == 0; }

    /*
     * True iff the property lookup will find the property on the prototype of
     * the object if the entry matches.
     *
     * This test is applicable only to property lookups, not to identifier
     * lookups.  It is meaningless to ask this question of an entry for an
     * identifier lookup.
     */
    bool isPrototypePropertyHit() const { return protoIndex == 1; }

    void assign(jsbytecode *kpc, Shape *kshape, Shape *pshape, Shape *prop, unsigned protoIndex) {
        JS_ASSERT(protoIndex <= MaxProtoIndex);

        this->kpc = kpc;
        this->kshape = kshape;
        this->pshape = pshape;
        this->prop = prop;
        this->protoIndex = uint8_t(protoIndex);
    }
};

/*
 * Special value for functions returning PropertyCacheEntry * to distinguish
 * between failure and no no-cache-fill cases.
 */
#define JS_NO_PROP_CACHE_FILL ((js::PropertyCacheEntry *) NULL + 1)

#if defined DEBUG_brendan || defined DEBUG_brendaneich
#define JS_PROPERTY_CACHE_METERING 1
#endif

class PropertyCache
{
  private:
    enum {
        SIZE_LOG2 = 8,
        SIZE = JS_BIT(SIZE_LOG2),
        MASK = JS_BITMASK(SIZE_LOG2)
    };

    PropertyCacheEntry  table[SIZE];
    JSBool              empty;

  public:
#ifdef JS_PROPERTY_CACHE_METERING
    PropertyCacheEntry  *pctestentry;   /* entry of the last PC-based test */
    uint32_t            fills;          /* number of cache entry fills */
    uint32_t            nofills;        /* couldn't fill (e.g. default get) */
    uint32_t            rofills;        /* set on read-only prop can't fill */
    uint32_t            disfills;       /* fill attempts on disabled cache */
    uint32_t            oddfills;       /* fill attempt after setter deleted */
    uint32_t            add2dictfills;  /* fill attempt on dictionary object */
    uint32_t            modfills;       /* fill that rehashed to a new entry */
    uint32_t            brandfills;     /* scope brandings to type structural
                                           method fills */
    uint32_t            noprotos;       /* resolve-returned non-proto pobj */
    uint32_t            longchains;     /* overlong scope and/or proto chain */
    uint32_t            recycles;       /* cache entries recycled by fills */
    uint32_t            tests;          /* cache probes */
    uint32_t            pchits;         /* fast-path polymorphic op hits */
    uint32_t            protopchits;    /* pchits hitting immediate prototype */
    uint32_t            initests;       /* cache probes from JSOP_INITPROP */
    uint32_t            inipchits;      /* init'ing next property pchit case */
    uint32_t            inipcmisses;    /* init'ing next property pc misses */
    uint32_t            settests;       /* cache probes from JOF_SET opcodes */
    uint32_t            addpchits;      /* adding next property pchit case */
    uint32_t            setpchits;      /* setting existing property pchit */
    uint32_t            setpcmisses;    /* setting/adding property pc misses */
    uint32_t            setmisses;      /* JSOP_SET{NAME,PROP} total misses */
    uint32_t            kpcmisses;      /* slow-path key id == atom misses */
    uint32_t            kshapemisses;   /* slow-path key object misses */
    uint32_t            vcapmisses;     /* value capability misses */
    uint32_t            misses;         /* cache misses */
    uint32_t            flushes;        /* cache flushes */
    uint32_t            pcpurges;       /* shadowing purges on proto chain */

# define PCMETER(x)     x
#else
# define PCMETER(x)     ((void)0)
#endif

    PropertyCache() {
        mozilla::PodZero(this);
    }

  private:
    static inline uintptr_t
    hash(jsbytecode *pc, const Shape *kshape)
    {
        return (((uintptr_t(pc) >> SIZE_LOG2) ^ uintptr_t(pc) ^ ((uintptr_t)kshape >> 3)) & MASK);
    }

    static inline bool matchShape(JSContext *cx, JSObject *obj, uint32_t shape);

    PropertyName *
    fullTest(JSContext *cx, jsbytecode *pc, JSObject **objp,
             JSObject **pobjp, PropertyCacheEntry *entry);

#ifdef DEBUG
    void assertEmpty();
#else
    inline void assertEmpty() {}
#endif

  public:
    JS_ALWAYS_INLINE void test(JSContext *cx, jsbytecode *pc,
                               JSObject **obj, JSObject **pobj,
                               PropertyCacheEntry **entry, PropertyName **name);

    /*
     * Test for cached information about a property set on *objp at pc.
     *
     * On a hit, set *entryp to the entry and return true.
     *
     * On a miss, set *namep to the name of the property being set and return false.
     */
    JS_ALWAYS_INLINE bool testForSet(JSContext *cx, jsbytecode *pc, JSObject *obj,
                                     PropertyCacheEntry **entryp, JSObject **obj2p,
                                     PropertyName **namep);

    /*
     * Fill property cache entry for key cx->fp->pc, optimized value word
     * computed from obj and shape, and entry capability forged from
     * obj->shape() and an 8-bit protoIndex.
     *
     * Return the filled cache entry or JS_NO_PROP_CACHE_FILL if caching was
     * not possible.
     */
    PropertyCacheEntry *fill(JSContext *cx, JSObject *obj, JSObject *pobj, js::Shape *shape);

    void purge(JSRuntime *rt);

    /* Restore an entry that may have been purged during a GC. */
    void restore(PropertyCacheEntry *entry);
};

} /* namespace js */

#endif /* jspropertycache_h___ */
