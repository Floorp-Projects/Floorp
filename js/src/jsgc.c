/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JS Mark-and-Sweep Garbage Collector.
 *
 * This GC allocates only fixed-sized things big enough to contain two words
 * (pointers) on any host architecture.  It allocates from an arena pool (see
 * jsarena.h).  It uses a parallel arena-pool array of flag bytes to hold the
 * mark bit, finalizer type index, etc.
 *
 * XXX swizzle page to freelist for better locality of reference
 */
#include "jsstddef.h"
#include <stdlib.h>     /* for free, called by JS_ARENA_DESTROY */
#include <string.h>	/* for memset, called by jsarena.h macros if DEBUG */
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

/*
 * Arena sizes, the first must be a multiple of the second so the two arena
 * pools can be maintained (in particular, arenas may be destroyed from the
 * middle of each pool) in parallel.
 */
#define GC_ARENA_SIZE	8192		/* 1024 (512 on Alpha) objects */
#define GC_FLAGS_SIZE	(GC_ARENA_SIZE / sizeof(JSGCThing))
#define GC_ROOTS_SIZE	256		/* SWAG, small enough to amortize */

static JSHashNumber   gc_hash_root(const void *key);

struct JSGCThing {
    JSGCThing       *next;
    uint8           *flagp;
};

typedef void (*GCFinalizeOp)(JSContext *cx, JSGCThing *thing);

static GCFinalizeOp gc_finalizers[GCX_NTYPES];

#ifdef JS_GCMETER
#define METER(x) x
#else
#define METER(x) /* nothing */
#endif

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    if (!gc_finalizers[GCX_OBJECT]) {
	gc_finalizers[GCX_OBJECT] = (GCFinalizeOp)js_FinalizeObject;
	gc_finalizers[GCX_STRING] = (GCFinalizeOp)js_FinalizeString;
	gc_finalizers[GCX_DOUBLE] = (GCFinalizeOp)js_FinalizeDouble;
    }

    JS_InitArenaPool(&rt->gcArenaPool, "gc-arena", GC_ARENA_SIZE,
		     sizeof(JSGCThing));
    JS_InitArenaPool(&rt->gcFlagsPool, "gc-flags", GC_FLAGS_SIZE,
		     sizeof(uint8));
    rt->gcRootsHash = JS_NewHashTable(GC_ROOTS_SIZE, gc_hash_root,
				      JS_CompareValues, JS_CompareValues,
				      NULL, NULL);
    if (!rt->gcRootsHash)
	return JS_FALSE;
    rt->gcLocksHash = NULL;     /* create lazily */
    rt->gcMaxBytes = maxbytes;
    return JS_TRUE;
}

#ifdef JS_GCMETER
void
js_DumpGCStats(JSRuntime *rt, FILE *fp)
{
    fprintf(fp, "\nGC allocation statistics:\n");
    fprintf(fp, "     bytes currently allocated: %lu\n", rt->gcBytes);
    fprintf(fp, "                alloc attempts: %lu\n", rt->gcStats.alloc);
    fprintf(fp, "            GC freelist length: %lu\n", rt->gcStats.freelen);
    fprintf(fp, "  recycles through GC freelist: %lu\n", rt->gcStats.recycle);
    fprintf(fp, "alloc retries after running GC: %lu\n", rt->gcStats.retry);
    fprintf(fp, "           allocation failures: %lu\n", rt->gcStats.fail);
    fprintf(fp, "              valid lock calls: %lu\n", rt->gcStats.lock);
    fprintf(fp, "            valid unlock calls: %lu\n", rt->gcStats.unlock);
    fprintf(fp, "   locks that hit stuck counts: %lu\n", rt->gcStats.stuck);
    fprintf(fp, " unlocks that saw stuck counts: %lu\n", rt->gcStats.unstuck);
    fprintf(fp, "          mark recursion depth: %lu\n", rt->gcStats.depth);
    fprintf(fp, "  maximum mark recursion depth: %lu\n", rt->gcStats.maxdepth);
    fprintf(fp, "      maximum GC nesting level: %lu\n", rt->gcStats.maxlevel);
    fprintf(fp, "   potentially useful GC calls: %lu\n", rt->gcStats.poke);
    fprintf(fp, "              useless GC calls: %lu\n", rt->gcStats.nopoke);
    fprintf(fp, "        thing arena corruption: %lu\n", rt->gcStats.badarena);
    fprintf(fp, "        flags arena corruption: %lu\n", rt->gcStats.badflag);
    fprintf(fp, "     thing arenas freed so far: %lu\n", rt->gcStats.afree);
    fprintf(fp, "     flags arenas freed so far: %lu\n", rt->gcStats.fafree);
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(fp);
#endif
}
#endif

void
js_FinishGC(JSRuntime *rt)
{
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    JS_FinishArenaPool(&rt->gcArenaPool);
    JS_FinishArenaPool(&rt->gcFlagsPool);
    JS_ArenaFinish();
    JS_HashTableDestroy(rt->gcRootsHash);
    rt->gcRootsHash = NULL;
    if (rt->gcLocksHash) {
        JS_HashTableDestroy(rt->gcLocksHash);
        rt->gcLocksHash = NULL;
    }
    rt->gcFreeList = NULL;
}

JSBool
js_AddRoot(JSContext *cx, void *rp, const char *name)
{
    JSRuntime *rt;
    JSBool ok;

    rt = cx->runtime;
    JS_LOCK_GC_VOID(rt,
	ok = (JS_HashTableAdd(rt->gcRootsHash, rp, (void *)name) != NULL));
    if (!ok)
	JS_ReportOutOfMemory(cx);
    return ok;
}

JSBool
js_RemoveRoot(JSRuntime *rt, void *rp)
{
    JS_LOCK_GC(rt);
    JS_HashTableRemove(rt->gcRootsHash, rp);
    rt->gcPoke = JS_TRUE;
    JS_UNLOCK_GC(rt);
    return JS_TRUE;
}

void *
js_AllocGCThing(JSContext *cx, uintN flags)
{
    JSBool tried_gc;
    JSRuntime *rt;
    JSGCThing *thing;
    uint8 *flagp;

#ifdef TOO_MUCH_GC
    js_GC(cx, GC_KEEP_ATOMS);
    tried_gc = JS_TRUE;
#else
    tried_gc = JS_FALSE;
#endif

    rt = cx->runtime;
    JS_LOCK_GC(rt);
    JS_ASSERT(rt->gcLevel == 0);
    METER(rt->gcStats.alloc++);
retry:
    thing = rt->gcFreeList;
    if (thing) {
	rt->gcFreeList = thing->next;
	flagp = thing->flagp;
	METER(rt->gcStats.freelen--);
	METER(rt->gcStats.recycle++);
    } else {
        flagp = NULL;
	if (rt->gcBytes < rt->gcMaxBytes &&
            (tried_gc || rt->gcMallocBytes < rt->gcMaxBytes))
        {
	    JS_ARENA_ALLOCATE(thing, &rt->gcArenaPool, sizeof(JSGCThing));
            if (thing)
                JS_ARENA_ALLOCATE(flagp, &rt->gcFlagsPool, sizeof(uint8));
	}
	if (!thing || !flagp) {
	    if (thing)
		JS_ARENA_RELEASE(&rt->gcArenaPool, thing);
	    if (!tried_gc) {
		JS_UNLOCK_GC(rt);
		js_GC(cx, GC_KEEP_ATOMS);
		tried_gc = JS_TRUE;
		JS_LOCK_GC(rt);
		METER(rt->gcStats.retry++);
		goto retry;
	    }
	    METER(rt->gcStats.fail++);
	    JS_UNLOCK_GC(rt);
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
    }
    *flagp = (uint8)flags;
    rt->gcBytes += sizeof(JSGCThing) + sizeof(uint8);
    cx->newborn[flags & GCF_TYPEMASK] = thing;

    /*
     * Clear thing before unlocking in case a GC run is about to scan it,
     * finding it via cx->newborn[].
     */
    thing->next = NULL;
    thing->flagp = NULL;
    JS_UNLOCK_GC(rt);
    return thing;
}

static uint8 *
gc_find_flags(JSRuntime *rt, void *thing)
{
    jsuword index, offset, length;
    JSArena *a, *fa;

    index = 0;
    for (a = rt->gcArenaPool.first.next; a; a = a->next) {
	offset = JS_UPTRDIFF(thing, a->base);
	length = a->avail - a->base;
	if (offset < length) {
	    index += offset / sizeof(JSGCThing);
	    for (fa = rt->gcFlagsPool.first.next; fa; fa = fa->next) {
		offset = fa->avail - fa->base;
		if (index < offset)
		    return (uint8 *)fa->base + index;
		index -= offset;
	    }
	    return NULL;
	}
	index += length / sizeof(JSGCThing);
    }
    return NULL;
}

static JSHashNumber
gc_hash_thing(const void *key)
{
    JSHashNumber num = (JSHashNumber) key;	/* help lame MSVC1.5 on Win16 */

    return num >> JSVAL_TAGBITS;
}

#define gc_lock_get_count(he)   ((jsword)(he)->value)
#define gc_lock_set_count(he,n) ((jsword)((he)->value = (void *)(n)))
#define gc_lock_increment(he)   gc_lock_set_count(he, gc_lock_get_count(he)+1)
#define gc_lock_decrement(he)   gc_lock_set_count(he, gc_lock_get_count(he)-1)

JSBool
js_LockGCThing(JSContext *cx, void *thing)
{
    JSRuntime *rt;
    uint8 *flagp, flags, lockbits;
    JSBool ok;
    JSHashEntry **hep, *he;

    if (!thing)
	return JS_TRUE;
    rt = cx->runtime;
    flagp = gc_find_flags(rt, thing);
    if (!flagp)
	return JS_FALSE;

    ok = JS_TRUE;
    JS_LOCK_GC(rt);
    flags = *flagp;
    lockbits = (flags & GCF_LOCKMASK);
    if (lockbits != GCF_LOCKMASK) {
        if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
            /* Objects may require "deep locking", i.e., rooting by value. */
            if (lockbits == 0) {
                if (!rt->gcLocksHash) {
                    rt->gcLocksHash = JS_NewHashTable(GC_ROOTS_SIZE,
                                                      gc_hash_thing,
                                                      JS_CompareValues,
                                                      JS_CompareValues,
                                                      NULL, NULL);
                    if (!rt->gcLocksHash)
                        goto outofmem;
                } else {
                    JS_ASSERT(!JS_HashTableLookup(rt->gcLocksHash, thing));
                }
                he = JS_HashTableAdd(rt->gcLocksHash, thing, NULL);
                if (!he)
                    goto outofmem;
                gc_lock_set_count(he, 1);
                *flagp = (uint8)(flags + GCF_LOCK);
            } else {
                JS_ASSERT(lockbits == GCF_LOCK);
                hep = JS_HashTableRawLookup(rt->gcLocksHash,
                                            gc_hash_thing(thing),
                                            thing);
                he = *hep;
                JS_ASSERT(he);
                if (he) {
                    JS_ASSERT(gc_lock_get_count(he) >= 1);
                    gc_lock_increment(he);
                }
            }
        } else {
            *flagp = (uint8)(flags + GCF_LOCK);
        }
    } else {
	METER(rt->gcStats.stuck++);
    }

    METER(rt->gcStats.lock++);
out:
    JS_UNLOCK_GC(rt);
    return ok;

outofmem:
    JS_ReportOutOfMemory(cx);
    ok = JS_FALSE;
    goto out;
}

JSBool
js_UnlockGCThing(JSContext *cx, void *thing)
{
    JSRuntime *rt;
    uint8 *flagp, flags, lockbits;
    JSHashEntry **hep, *he;

    if (!thing)
	return JS_TRUE;
    rt = cx->runtime;
    flagp = gc_find_flags(rt, thing);
    if (!flagp)
	return JS_FALSE;

    JS_LOCK_GC(rt);
    flags = *flagp;
    lockbits = (flags & GCF_LOCKMASK);
    if (lockbits != GCF_LOCKMASK) {
        if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
            /* Defend against a call on an unlocked object. */
            if (lockbits != 0) {
                JS_ASSERT(lockbits == GCF_LOCK);
                hep = JS_HashTableRawLookup(rt->gcLocksHash,
                                            gc_hash_thing(thing),
                                            thing);
                he = *hep;
                JS_ASSERT(he);
                if (he && gc_lock_decrement(he) == 0) {
                    JS_HashTableRawRemove(rt->gcLocksHash, hep, he);
                    *flagp = (uint8)(flags & ~GCF_LOCKMASK);
                }
            }
        } else {
            *flagp = (uint8)(flags - GCF_LOCK);
        }
    } else {
	METER(rt->gcStats.unstuck++);
    }

    rt->gcPoke = JS_TRUE;
    METER(rt->gcStats.unlock++);
    JS_UNLOCK_GC(rt);
    return JS_TRUE;
}

#ifdef GC_MARK_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include "jsprf.h"

JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
JS_EXPORT_DATA(void *) js_LiveThingToFind;

#ifdef HAVE_XPCONNECT
#include "dump_xpc.h"
#endif

static const char*
gc_object_class_name(JSRuntime* rt, void* thing)
{
    uint8 *flagp = gc_find_flags(rt, thing);
    const char *className = "";

    if (flagp && ((*flagp & GCF_TYPEMASK) == GCX_OBJECT)) {
        JSObject  *obj = (JSObject *)thing;
        JSClass   *clasp = JSVAL_TO_PRIVATE(obj->slots[JSSLOT_CLASS]);
        className = clasp->name;
#ifdef HAVE_XPCONNECT
        if ((clasp->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) &&
            (clasp->flags & JSCLASS_HAS_PRIVATE)) {
            jsval privateValue = obj->slots[JSSLOT_PRIVATE];
            void  *privateThing = JSVAL_IS_VOID(privateValue)
                                  ? NULL
                                  : JSVAL_TO_PRIVATE(privateValue);

            const char* xpcClassName = GetXPCObjectClassName(privateThing);
            if (xpcClassName)
                className = xpcClassName;
        }
#endif
    }

    return className;
}

static void
gc_dump_thing(JSRuntime* rt, JSGCThing *thing, uint8 flags, GCMarkNode *prev,
              FILE *fp)
{
    GCMarkNode *next = NULL;
    char *path = NULL;

    while (prev) {
        next = prev;
        prev = prev->prev;
    }
    while (next) {
        path = JS_sprintf_append(path, "%s(%s).",
                                 next->name,
                                 gc_object_class_name(rt, next->thing));
        next = next->next;
    }
    if (!path)
        return;

    fprintf(fp, "%08lx ", (long)thing);
    switch (flags & GCF_TYPEMASK) {
      case GCX_OBJECT:
      {
        JSObject  *obj = (JSObject *)thing;
        jsval     privateValue = obj->slots[JSSLOT_PRIVATE];
        void      *privateThing = JSVAL_IS_VOID(privateValue)
                                  ? NULL
                                  : JSVAL_TO_PRIVATE(privateValue);
        const char* className = gc_object_class_name(rt, thing);
        fprintf(fp, "object %08p %s", privateThing, className);
        break;
      }
      case GCX_STRING:
        fprintf(fp, "string %s", JS_GetStringBytes((JSString *)thing));
        break;
      case GCX_DOUBLE:
        fprintf(fp, "double %g", *(jsdouble *)thing);
        break;
      case GCX_DECIMAL:
        break;
    }
    fprintf(fp, " via %s\n", path);
    free(path);
}

#endif /* !GC_MARK_DEBUG */

static void
gc_mark_atom_key_thing(void *thing, void *arg)
{
    JSContext *cx = (JSContext *) arg;
    GC_MARK(cx, thing, "atom", NULL);
}

void
js_MarkAtom(JSContext *cx, JSAtom *atom, void *arg)
{
    jsval key;

    if (atom->flags & ATOM_MARK)
	return;
    atom->flags |= ATOM_MARK;
    key = ATOM_KEY(atom);
    if (JSVAL_IS_GCTHING(key)) {
#ifdef GC_MARK_DEBUG
	char name[32];

	if (JSVAL_IS_STRING(key)) {
	    JS_snprintf(name, sizeof name, "'%s'",
			JS_GetStringBytes(JSVAL_TO_STRING(key)));
	} else {
	    JS_snprintf(name, sizeof name, "<%x>", key);
	}
#endif
	GC_MARK(cx, JSVAL_TO_GCTHING(key), name, arg);
    }
}

void
js_MarkGCThing(JSContext *cx, void *thing, void *arg)
{
    JSRuntime *rt;
    uint8 flags, *flagp;
    JSObject *obj;
    uint32 nslots;
    jsval v, *vp, *end;
#ifdef GC_MARK_DEBUG
    JSScope *scope;
    JSScopeProperty *sprop;
#endif

    if (!thing)
	return;
    rt = cx->runtime;
    flagp = gc_find_flags(rt, thing);
    if (!flagp)
	return;

    /* Check for something on the GC freelist to handle recycled stack. */
    flags = *flagp;
    if (flags == GCF_FINAL)
	return;

#ifdef GC_MARK_DEBUG
    if (js_LiveThingToFind == thing)
        gc_dump_thing(rt, thing, flags, arg, stderr);
#endif

    if (flags & GCF_MARK)
	return;
    *flagp |= GCF_MARK;
    METER(if (++rt->gcStats.depth > rt->gcStats.maxdepth)
	      rt->gcStats.maxdepth = rt->gcStats.depth);

#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap)
        gc_dump_thing(rt, thing, flags, arg, js_DumpGCHeap);
#endif

    if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
	obj = (JSObject *) thing;
	vp = obj->slots;
        nslots = (obj->map->ops->mark)
                 ? obj->map->ops->mark(cx, obj, arg)
                 : obj->map->freeslot;
#ifdef GC_MARK_DEBUG
        scope = OBJ_IS_NATIVE(obj) ? OBJ_SCOPE(obj) : NULL;
#endif
        for (end = vp + nslots; vp < end; vp++) {
            v = *vp;
            if (JSVAL_IS_GCTHING(v)) {
#ifdef GC_MARK_DEBUG
                char name[32];

                if (scope) {
                    uint32 slot;
                    jsval nval;

                    slot = vp - obj->slots;
                    for (sprop = scope->props; ; sprop = sprop->next) {
                        if (!sprop) {
                            switch (slot) {
                              case JSSLOT_PROTO:
                                strcpy(name, "__proto__");
                                break;
                              case JSSLOT_PARENT:
                                strcpy(name, "__parent__");
                                break;
                              case JSSLOT_PRIVATE:
                                strcpy(name, "__private__");
                                break;
                              default:
                                JS_snprintf(name, sizeof name,
                                            "**UNKNOWN SLOT %ld**",
                                            (long)slot);
                                break;
                            }
                            break;
                        }
                        if (sprop->slot == slot) {
                            nval = sprop->symbols
                                   ? js_IdToValue(sym_id(sprop->symbols))
                                   : sprop->id;
                            if (JSVAL_IS_INT(nval)) {
                                JS_snprintf(name, sizeof name, "%ld",
                                            (long)JSVAL_TO_INT(nval));
                            } else if (JSVAL_IS_STRING(nval)) {
                                JS_snprintf(name, sizeof name, "%s",
                                  JS_GetStringBytes(JSVAL_TO_STRING(nval)));
                            } else {
                                strcpy(name, "**FINALIZED ATOM KEY**");
                            }
                            break;
                        }
                    }
                }
#endif
                GC_MARK(cx, JSVAL_TO_GCTHING(v), name, arg);
            }
        }
    }
    METER(rt->gcStats.depth--);
}

static JSHashNumber
gc_hash_root(const void *key)
{
    JSHashNumber num = (JSHashNumber) key;	/* help lame MSVC1.5 on Win16 */

    return num >> 2;
}

JS_STATIC_DLL_CALLBACK(intN)
gc_root_marker(JSHashEntry *he, intN i, void *arg)
{
    jsval *rp = (jsval *)he->key;
    jsval v = *rp;

    /* Ignore null object and scalar values. */
    if (!JSVAL_IS_NULL(v) && JSVAL_IS_GCTHING(v)) {
        JSContext *cx = (JSContext *)arg;
#ifdef DEBUG
        JSArena *a;
        JSBool root_points_to_gcArenaPool = JS_FALSE;
	void *thing = JSVAL_TO_GCTHING(v);

        for (a = cx->runtime->gcArenaPool.first.next; a; a = a->next) {
	    if (JS_UPTRDIFF(thing, a->base) < a->avail - a->base) {
                root_points_to_gcArenaPool = JS_TRUE;
                break;
            }
        }
        if (!root_points_to_gcArenaPool && he->value) {
            fprintf(stderr, 
              "Error: The address passed to JS_AddNamedRoot currently "
              "holds an invalid jsval.\n "
              "  This is usually caused by a missing call to JS_RemoveRoot.\n "
              "  Root name is \"%s\".\n", (const char *) he->value); 
        }
        JS_ASSERT(root_points_to_gcArenaPool);
#endif

        GC_MARK(cx, JSVAL_TO_GCTHING(v), he->value ? he->value : "root", NULL);
    }
    return HT_ENUMERATE_NEXT;
}

JS_STATIC_DLL_CALLBACK(intN)
gc_lock_marker(JSHashEntry *he, intN i, void *arg)
{
    void *thing = (void *)he->key;
    JSContext *cx = (JSContext *)arg;

    GC_MARK(cx, thing, "locked object", NULL);
    return HT_ENUMERATE_NEXT;
}

JS_FRIEND_API(void)
js_ForceGC(JSContext *cx)
{
    cx->newborn[GCX_OBJECT] = NULL;
    cx->newborn[GCX_STRING] = NULL;
    cx->newborn[GCX_DOUBLE] = NULL;
    cx->runtime->gcPoke = JS_TRUE;
    js_GC(cx, 0);
    JS_ArenaFinish();
}

void
js_GC(JSContext *cx, uintN gcflags)
{
    JSRuntime *rt;
    JSContext *iter, *acx;
    JSArena *a, *ma, *fa, **ap, **fap;
    jsval v, *vp, *sp;
    jsuword begin, end;
    JSStackFrame *fp, *chain;
    void *mark;
    uint8 flags, *flagp;
    JSGCThing *thing, *final, **flp, **oflp;
    GCFinalizeOp finalizer;
    JSBool a_all_clear, f_all_clear;
#ifdef JS_THREADSAFE
    jsword currentThread;
    uint32 requestDebit;
#endif

    rt = cx->runtime;
#ifdef JS_THREADSAFE
    /* Avoid deadlock. */
    JS_ASSERT(!JS_IS_RUNTIME_LOCKED(rt));
#endif
    if (!(gcflags & GC_LAST_CONTEXT)) {
        if (rt->gcDisabled)
            return;

        /* Let the API user decide to defer a GC if it wants to. */
        if (rt->gcCallback && !rt->gcCallback(cx, JSGC_BEGIN))
            return;
    }

    /* Lock out other GC allocator and collector invocations. */
    JS_LOCK_GC(rt);

    /* Do nothing if no assignment has executed since the last GC. */
    if (!rt->gcPoke) {
	METER(rt->gcStats.nopoke++);
	JS_UNLOCK_GC(rt);
	return;
    }
    rt->gcPoke = JS_FALSE;
    METER(rt->gcStats.poke++);

#ifdef JS_THREADSAFE
    /* Bump gcLevel and return rather than nest on this thread. */
    currentThread = js_CurrentThreadId();
    if (rt->gcThread == currentThread) {
	JS_ASSERT(rt->gcLevel > 0);
	rt->gcLevel++;
	METER(if (rt->gcLevel > rt->gcStats.maxlevel)
		  rt->gcStats.maxlevel = rt->gcLevel);
        JS_UNLOCK_GC(rt);
        return;
    }

    /*
     * If we're in one or more requests (possibly on more than one context)
     * running on the current thread, indicate, temporarily, that all these
     * requests are inactive.  NB: if cx->thread is 0, then cx is not using
     * the request model, and does not contribute to rt->requestCount.
     */
    requestDebit = 0;
    if (cx->thread) {
        /*
         * Check all contexts for any with the same thread-id.  XXX should we
         * keep a sub-list of contexts having the same id?
         */
        iter = NULL;
        while ((acx = js_ContextIterator(rt, &iter)) != NULL) {
            if (acx->thread == cx->thread && acx->requestDepth)
                requestDebit++;
        }
    } else {
        /*
         * We assert, but check anyway, in case someone is misusing the API.
         * Avoiding the loop over all of rt's contexts is a win in the event
         * that the GC runs only on request-less contexts with 0 thread-ids,
         * in a special thread such as the UI/DOM/Layout "mozilla" or "main"
         * thread in Mozilla-the-browser.
         */
        JS_ASSERT(cx->requestDepth == 0);
        if (cx->requestDepth)
            requestDebit = 1;
    }
    if (requestDebit) {
        JS_ASSERT(requestDebit >= rt->requestCount);
        rt->requestCount -= requestDebit;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);
    }

    /* If another thread is already in GC, don't attempt GC; wait instead. */
    if (rt->gcLevel > 0) {
        /* Bump gcLevel to restart the current GC, so it finds new garbage. */
	rt->gcLevel++;
	METER(if (rt->gcLevel > rt->gcStats.maxlevel)
		  rt->gcStats.maxlevel = rt->gcLevel);

        /* Wait for the other thread to finish, then resume our request. */
        while (rt->gcLevel > 0)
            JS_AWAIT_GC_DONE(rt);
	if (cx->requestDepth)
	    rt->requestCount++;
	JS_UNLOCK_GC(rt);
	return;
    }

    /* No other thread is in GC, so indicate that we're now in GC. */
    rt->gcLevel = 1;
    rt->gcThread = currentThread;

    /* Wait for all other requests to finish. */
    while (rt->requestCount > 0)
	JS_AWAIT_REQUEST_DONE(rt);

#else  /* !JS_THREADSAFE */

    /* Bump gcLevel and return rather than nest; the outer gc will restart. */
    rt->gcLevel++;
    METER(if (rt->gcLevel > rt->gcStats.maxlevel)
	      rt->gcStats.maxlevel = rt->gcLevel);
    if (rt->gcLevel > 1)
	return;

#endif /* !JS_THREADSAFE */

    /* Reset malloc counter */
    rt->gcMallocBytes = 0;

    /* Drop atoms held by the property cache, and clear property weak links. */
    js_FlushPropertyCache(cx);
restart:
    rt->gcNumber++;

    /*
     * Mark phase.
     */
    JS_HashTableEnumerateEntries(rt->gcRootsHash, gc_root_marker, cx);
    if (rt->gcLocksHash)
        JS_HashTableEnumerateEntries(rt->gcLocksHash, gc_lock_marker, cx);
    js_MarkAtomState(&rt->atomState, gc_mark_atom_key_thing, cx);
    iter = NULL;
    while ((acx = js_ContextIterator(rt, &iter)) != NULL) {
	/*
	 * Iterate frame chain and dormant chains. Temporarily tack current
	 * frame onto the head of the dormant list to ease iteration.
	 *
	 * (NB: see comment on this whole "dormant" thing in js_Execute.)
	 */
	chain = acx->fp;
	if (chain) {
	    JS_ASSERT(!chain->dormantNext);
	    chain->dormantNext = acx->dormantFrameChain;
	} else {
	    chain = acx->dormantFrameChain;
	}

	for (fp = chain; fp; fp = chain = chain->dormantNext) {
	    sp = fp->sp;
	    if (sp) {
		for (a = acx->stackPool.first.next; a; a = a->next) {
                    /*
                     * Don't scan beyond the current context's top of stack,
                     * because we may be nesting a GC from within a call to
                     * js_AllocGCThing originating from a conversion call made
                     * by js_Interpret with local variables holding the only
                     * references to other, unrooted GC-things (e.g., a non-
                     * newborn object that was just popped off the stack).
                     *
                     * Yes, this means we're not doing "exact GC", exactly.
                     * This temporary failure to collect garbage held only
                     * by unrecycled stack space should be fixed, but it is
                     * not a leak bug, and the bloat issue should also be
                     * small and transient (the next GC will likely get any
                     * true garbage, as the stack will have pulsated).  But
                     * it deserves an XXX.
                     */
                    begin = a->base;
                    end = a->avail;
                    if (acx != cx &&
                        JS_UPTRDIFF(sp, begin) < JS_UPTRDIFF(end, begin)) {
                        end = (jsuword)sp;
                    }
		    for (vp = (jsval *)begin; vp < (jsval *)end; vp++) {
			v = *vp;
			if (JSVAL_IS_GCTHING(v))
			    GC_MARK(cx, JSVAL_TO_GCTHING(v), "stack", NULL);
		    }
		    if (end == (jsuword)sp)
			break;
		}
	    }
	    do {
		GC_MARK(cx, fp->scopeChain, "scope chain", NULL);
		GC_MARK(cx, fp->thisp, "this", NULL);
		if (JSVAL_IS_GCTHING(fp->rval))
		    GC_MARK(cx, JSVAL_TO_GCTHING(fp->rval), "rval", NULL);
		if (fp->callobj)
		    GC_MARK(cx, fp->callobj, "call object", NULL);
		if (fp->argsobj)
		    GC_MARK(cx, fp->argsobj, "arguments object", NULL);
		if (fp->script)
		    js_MarkScript(cx, fp->script, NULL);
		if (fp->sharpArray)
		    GC_MARK(cx, fp->sharpArray, "sharp array", NULL);
	    } while ((fp = fp->down) != NULL);
	}

	/* Cleanup temporary "dormant" linkage. */
	if (acx->fp)
	    acx->fp->dormantNext = NULL;

        /* Mark other roots-by-definition in acx. */
	GC_MARK(cx, acx->globalObject, "global object", NULL);
	GC_MARK(cx, acx->newborn[GCX_OBJECT], "newborn object", NULL);
	GC_MARK(cx, acx->newborn[GCX_STRING], "newborn string", NULL);
	GC_MARK(cx, acx->newborn[GCX_DOUBLE], "newborn double", NULL);
#if JS_HAS_EXCEPTIONS
	if (acx->throwing && JSVAL_IS_GCTHING(acx->exception))
	    GC_MARK(cx, JSVAL_TO_GCTHING(acx->exception), "exception", NULL);
#endif
#if JS_HAS_LVALUE_RETURN
        if (acx->rval2set && JSVAL_IS_GCTHING(acx->rval2))
            GC_MARK(cx, JSVAL_TO_GCTHING(acx->rval2), "rval2", NULL);
#endif
    }

    /*
     * Sweep phase.
     * Mark tempPool for release of finalization records at label out.
     */
    ma = cx->tempPool.current;
    mark = JS_ARENA_MARK(&cx->tempPool);
    js_SweepAtomState(&rt->atomState, gcflags);
    fa = rt->gcFlagsPool.first.next;
    flagp = (uint8 *)fa->base;
    for (a = rt->gcArenaPool.first.next; a; a = a->next) {
	for (thing = (JSGCThing *)a->base; thing < (JSGCThing *)a->avail;
	     thing++) {
	    if (flagp >= (uint8 *)fa->avail) {
		fa = fa->next;
		JS_ASSERT(fa);
		if (!fa) {
		    METER(rt->gcStats.badflag++);
		    goto out;
		}
		flagp = (uint8 *)fa->base;
	    }
	    flags = *flagp;
	    if (flags & GCF_MARK) {
		*flagp &= ~GCF_MARK;
	    } else if (!(flags & (GCF_LOCKMASK | GCF_FINAL))) {
		JS_ARENA_ALLOCATE(final, &cx->tempPool, sizeof(JSGCThing));
		if (!final)
		    goto finalize_phase;
		final->next = thing;
		final->flagp = flagp;
		JS_ASSERT(rt->gcBytes >= sizeof(JSGCThing) + sizeof(uint8));
		rt->gcBytes -= sizeof(JSGCThing) + sizeof(uint8);
	    }
	    flagp++;
	}
    }

finalize_phase:
    /*
     * Finalize phase.
     * Don't hold the GC lock while running finalizers!
     */
    JS_UNLOCK_GC(rt);
    for (final = (JSGCThing *) mark; ; final++) {
	if ((jsuword)final >= ma->avail) {
	    ma = ma->next;
	    if (!ma)
		break;
	    final = (JSGCThing *)ma->base;
	}
	thing = final->next;
	flagp = final->flagp;
	flags = *flagp;
	finalizer = gc_finalizers[flags & GCF_TYPEMASK];
	if (finalizer) {
	    *flagp |= GCF_FINAL;
	    finalizer(cx, thing);
	}

	/*
	 * Set flags to GCF_FINAL, signifying that thing is free, but don't
	 * thread thing onto rt->gcFreeList.  We need the GC lock to rebuild
	 * the freelist below while also looking for free-able arenas.
	 */
	*flagp = GCF_FINAL;
    }
    JS_LOCK_GC(rt);

    /*
     * Free phase.
     * Free any unused arenas and rebuild the JSGCThing freelist.
     */
    ap = &rt->gcArenaPool.first.next;
    a = *ap;
    if (!a)
	goto out;
    thing = (JSGCThing *)a->base;
    a_all_clear = f_all_clear = JS_TRUE;
    flp = oflp = &rt->gcFreeList;
    *flp = NULL;
    METER(rt->gcStats.freelen = 0);

    fap = &rt->gcFlagsPool.first.next;
    while ((fa = *fap) != NULL) {
	/* XXX optimize by unrolling to use word loads */
	for (flagp = (uint8 *)fa->base; ; flagp++) {
	    JS_ASSERT(a);
	    if (!a) {
		METER(rt->gcStats.badarena++);
		goto out;
	    }
	    if (thing >= (JSGCThing *)a->avail) {
		if (a_all_clear) {
		    JS_ARENA_DESTROY(&rt->gcArenaPool, a, ap);
		    flp = oflp;
		    METER(rt->gcStats.afree++);
		} else {
		    ap = &a->next;
		    a_all_clear = JS_TRUE;
		    oflp = flp;
		}
		a = *ap;
		if (!a)
		    break;
		thing = (JSGCThing *)a->base;
	    }
	    if (flagp >= (uint8 *)fa->avail)
		break;
	    if (*flagp != GCF_FINAL) {
		a_all_clear = f_all_clear = JS_FALSE;
	    } else {
		thing->flagp = flagp;
		*flp = thing;
		flp = &thing->next;
		METER(rt->gcStats.freelen++);
	    }
	    thing++;
	}
	if (f_all_clear) {
	    JS_ARENA_DESTROY(&rt->gcFlagsPool, fa, fap);
	    METER(rt->gcStats.fafree++);
	} else {
	    fap = &fa->next;
	    f_all_clear = JS_TRUE;
	}
    }

    /* Terminate the new freelist. */
    *flp = NULL;

out:
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    if (rt->gcLevel > 1) {
	rt->gcLevel = 1;
	goto restart;
    }
    rt->gcLevel = 0;
    rt->gcLastBytes = rt->gcBytes;

#ifdef JS_THREADSAFE
    /* If we were invoked during a request, pay back the temporary debit. */
    if (requestDebit)
	rt->requestCount += requestDebit;
    rt->gcThread = 0;
    JS_NOTIFY_GC_DONE(rt);
    JS_UNLOCK_GC(rt);
#endif
    if (rt->gcCallback)
	(void) rt->gcCallback(cx, JSGC_END);
}
