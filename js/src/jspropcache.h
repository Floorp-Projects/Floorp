/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jspropcache_h___
#define jspropcache_h___

/*
 * Property cache with structurally typed capabilities for invalidation, for
 * polymorphic callsite method/get/set speedups.  For details, see
 * <https://developer.mozilla.org/en/SpiderMonkey/Internals/Property_cache>.
 */
const unsigned PROPERTY_CACHE_LOG2 = 12;
const jsuword PROPERTY_CACHE_SIZE = JS_BIT(PROPERTY_CACHE_LOG2);
const jsuword PROPERTY_CACHE_MASK = JS_BITMASK(PROPERTY_CACHE_LOG2);

/*
 * Property cache value capability macros.
 */
const unsigned PCVCAP_PROTOBITS = 4;
const jsuword PCVCAP_PROTOSIZE = JS_BIT(PCVCAP_PROTOBITS);
const jsuword PCVCAP_PROTOMASK = JS_BITMASK(PCVCAP_PROTOBITS);

const unsigned PCVCAP_SCOPEBITS = 4;
const jsuword PCVCAP_SCOPESIZE = JS_BIT(PCVCAP_SCOPEBITS);
const jsuword PCVCAP_SCOPEMASK = JS_BITMASK(PCVCAP_SCOPEBITS);

const jsuword PCVCAP_TAGBITS = PCVCAP_PROTOBITS + PCVCAP_SCOPEBITS;
const jsuword PCVCAP_TAGMASK = JS_BITMASK(PCVCAP_TAGBITS);
inline jsuword PCVCAP_TAG(jsuword t) { return t & PCVCAP_TAGMASK; }

inline jsuword PCVCAP_MAKE(jsuword t, jsuword s, jsuword p)
{
    return (uint32(t) << PCVCAP_TAGBITS) | (s << PCVCAP_PROTOBITS) | p;
}

inline jsuword PCVCAP_SHAPE(jsuword t) { return t >> PCVCAP_TAGBITS; }

const jsuword SHAPE_OVERFLOW_BIT = JS_BIT(32 - PCVCAP_TAGBITS);

struct JSPropCacheEntry {
    jsbytecode          *kpc;           /* pc if vcap tag is <= 1, else atom */
    jsuword             kshape;         /* key shape if pc, else obj for atom */
    jsuword             vcap;           /* value capability, see above */
    jsuword             vword;          /* value word, see PCVAL_* below */

    bool adding() const {
        return PCVCAP_TAG(vcap) == 0 && kshape != PCVCAP_SHAPE(vcap);
    }

    bool directHit() const {
        return PCVCAP_TAG(vcap) == 0 && kshape == PCVCAP_SHAPE(vcap);
    }
};

/*
 * Special value for functions returning JSPropCacheEntry * to distinguish
 * between failure and no no-cache-fill cases.
 */
JSPropCacheEntry * const JS_NO_PROP_CACHE_FILL = (JSPropCacheEntry *) NULL + 1;

#if defined DEBUG_brendan || defined DEBUG_brendaneich
#define JS_PROPERTY_CACHE_METERING 1
#endif

class JSPropertyCache {
public:
    JS_ALWAYS_INLINE JS_REQUIRES_STACK void
    test(JSContext *cx, jsbytecode *pc, JSObject **objp,
         JSObject **pobjp, JSPropCacheEntry **entryp, JSAtom **atomp);

    JS_ALWAYS_INLINE JS_REQUIRES_STACK bool
    testForSet(JSContext *cx, jsbytecode *pc, JSObject **objp,
               JSObject **pobjp, JSPropCacheEntry **entryp, JSAtom **atomp);

    JS_ALWAYS_INLINE bool
    testForInit(JSContext *cx, jsbytecode *pc, JSObject *obj,
                JSPropCacheEntry **entryp, JSScopeProperty **spropp);

    /*
     * Fill property cache entry for key cx->fp->pc, optimized value word
     * computed from obj and sprop, and entry capability forged from 24-bit
     * OBJ_SHAPE(obj), 4-bit scopeIndex, and 4-bit protoIndex.
     *
     * Return the filled cache entry or JS_NO_PROP_CACHE_FILL if caching was
     * not possible.
     */
    JS_REQUIRES_STACK JSPropCacheEntry *
    fill(JSContext *cx, JSObject *obj, uintN scopeIndex, uintN protoIndex, JSObject *pobj,
         JSScopeProperty *sprop, JSBool adding);

    void purge(JSContext *cx);
    void purgeForScript(JSContext *cx, JSScript *script);

private:
    JS_REQUIRES_STACK JSAtom *
    fullTest(JSContext *cx, jsbytecode *pc, JSObject **objp, JSObject **pobjp,
             JSPropCacheEntry **entryp);

    inline void assertEmpty();

    inline JSPropCacheEntry *fillEntry(jsuword khash, jsbytecode *pc, jsuword kshape,
                                       jsuword vcap, jsuword vword);

    inline JSPropCacheEntry *fillByPC(jsbytecode *pc, jsuword kshape,
                                      jsuword vshape, jsuword scopeIndex, jsuword protoIndex,
                                      jsuword vword);

    inline JSPropCacheEntry *fillByAtom(JSAtom *atom, JSObject *obj,
                                        jsuword vshape, jsuword scopeIndex, jsuword protoIndex,
                                        jsuword vword);

    JSPropCacheEntry    table[PROPERTY_CACHE_SIZE];
    JSBool              empty;

#ifdef JS_PROPERTY_CACHE_METERING
public:
    JSPropCacheEntry    *pctestentry;   /* entry of the last PC-based test */
    uint32              fills;          /* number of cache entry fills */
    uint32              nofills;        /* couldn't fill (e.g. default get) */
    uint32              rofills;        /* set on read-only prop can't fill */
    uint32              disfills;       /* fill attempts on disabled cache */
    uint32              oddfills;       /* fill attempt after setter deleted */
    uint32              modfills;       /* fill that rehashed to a new entry */
    uint32              brandfills;     /* scope brandings to type structural
                                           method fills */
    uint32              noprotos;       /* resolve-returned non-proto pobj */
    uint32              longchains;     /* overlong scope and/or proto chain */
    uint32              recycles;       /* cache entries recycled by fills */
    uint32              pcrecycles;     /* pc-keyed entries recycled by atom-
                                           keyed fills */
    uint32              tests;          /* cache probes */
    uint32              pchits;         /* fast-path polymorphic op hits */
    uint32              protopchits;    /* pchits hitting immediate prototype */
    uint32              initests;       /* cache probes from JSOP_INITPROP */
    uint32              inipchits;      /* init'ing next property pchit case */
    uint32              inipcmisses;    /* init'ing next property pc misses */
    uint32              settests;       /* cache probes from JOF_SET opcodes */
    uint32              addpchits;      /* adding next property pchit case */
    uint32              setpchits;      /* setting existing property pchit */
    uint32              setpcmisses;    /* setting/adding property pc misses */
    uint32              slotchanges;    /* clasp->reserveSlots result variance-
                                           induced slot changes */
    uint32              setmisses;      /* JSOP_SET{NAME,PROP} total misses */
    uint32              idmisses;       /* slow-path key id == atom misses */
    uint32              komisses;       /* slow-path key object misses */
    uint32              vcmisses;       /* value capability misses */
    uint32              misses;         /* cache misses */
    uint32              flushes;        /* cache flushes */
    uint32              pcpurges;       /* shadowing purges on proto chain */
# define PCMETER(x)     x
#else
# define PCMETER(x)     ((void)0)
#endif
};

/*
 * Property cache value tagging/untagging macros.
 */
const jsuword PCVAL_OBJECT = 0;
const jsuword PCVAL_SLOT = 1;
const jsuword PCVAL_SPROP = 2;

const unsigned PCVAL_TAGBITS = 2;
const jsuword PCVAL_TAGMASK = JS_BITMASK(PCVAL_TAGBITS);

inline jsuword PCVAL_TAG(jsuword v) { return v & PCVAL_TAGMASK; }
inline jsuword PCVAL_CLRTAG(jsuword v) { return v & ~PCVAL_TAGMASK; }
inline jsuword PCVAL_SETTAG(jsuword v, jsuword t) { return v | t; }

const jsuword PCVAL_NULL = 0;
inline bool PCVAL_IS_NULL(jsuword v) { return v == PCVAL_NULL; }

inline bool PCVAL_IS_OBJECT(jsuword v) { return PCVAL_TAG(v) == PCVAL_OBJECT; }
inline JSObject *PCVAL_TO_OBJECT(jsuword v) { return (JSObject *) v; }
inline jsuword OBJECT_TO_PCVAL(JSObject *obj) { return (jsuword) obj; }

inline jsval PCVAL_OBJECT_TO_JSVAL(jsuword v) { return OBJECT_TO_JSVAL(PCVAL_TO_OBJECT(v)); }
inline jsuword JSVAL_OBJECT_TO_PCVAL(jsval v) { return OBJECT_TO_PCVAL(JSVAL_TO_OBJECT(v)); }

inline bool PCVAL_IS_SLOT(jsuword v) { return (v & PCVAL_SLOT) != 0; }
inline jsuint PCVAL_TO_SLOT(jsuword v) { return (jsuint) v >> 1; }
inline jsuword SLOT_TO_PCVAL(jsuint i) { return ((jsuword) i << 1) | PCVAL_SLOT; }

inline bool PCVAL_IS_SPROP(jsuword v) { return PCVAL_TAG(v) == PCVAL_SPROP; }
inline JSScopeProperty *PCVAL_TO_SPROP(jsuword v) { return (JSScopeProperty *) PCVAL_CLRTAG(v); }

inline jsuword
SPROP_TO_PCVAL(JSScopeProperty *sprop)
{
    return PCVAL_SETTAG((jsuword) sprop, PCVAL_SPROP);
}

#endif /* jspropcache_h___ */
