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
#include "jsfun.h"
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

typedef struct GCMarkNode GCMarkNode;

struct GCMarkNode {
    void        *thing;
    char        *name;
    GCMarkNode  *next;
    GCMarkNode  *prev;
};

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

static void
gc_mark_node(JSRuntime *rt, void *thing, GCMarkNode *prev);

#define GC_MARK(_rt, _thing, _name, _prev)                                    \
    JS_BEGIN_MACRO                                                            \
	GCMarkNode _node;                                                     \
	_node.thing = _thing;                                                 \
	_node.name  = _name;                                                  \
	_node.next  = NULL;                                                   \
	_node.prev  = _prev;                                                  \
	if (_prev) ((GCMarkNode *)(_prev))->next = &_node;                    \
	gc_mark_node(_rt, _thing, &_node);                                    \
    JS_END_MACRO

static void
gc_mark(JSRuntime *rt, void *thing)
{
    GC_MARK(rt, thing, "atom", NULL);
}

#define GC_MARK_ATOM(rt, atom, prev)     gc_mark_atom(rt, atom, prev)
#define GC_MARK_SCRIPT(rt, script, prev) gc_mark_script(rt, script, prev)

#else  /* !GC_MARK_DEBUG */

#define GC_MARK(rt, thing, name, prev)   gc_mark(rt, thing)
#define GC_MARK_ATOM(rt, atom, prev)     gc_mark_atom(rt, atom)
#define GC_MARK_SCRIPT(rt, script, prev) gc_mark_script(rt, script)

static void
gc_mark(JSRuntime *rt, void *thing);

#endif /* !GC_MARK_DEBUG */

static void
gc_mark_atom(JSRuntime *rt, JSAtom *atom
#ifdef GC_MARK_DEBUG
  , GCMarkNode *prev
#endif
)
{
    jsval key;

    if (!atom || atom->flags & ATOM_MARK)
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
	GC_MARK(rt, JSVAL_TO_GCTHING(key), name, prev);
    }
}

static void
gc_mark_script(JSRuntime *rt, JSScript *script
#ifdef GC_MARK_DEBUG
  , GCMarkNode *prev
#endif
)
{
    JSAtomMap *map;
    uintN i, length;
    JSAtom **vector;

    map = &script->atomMap;
    length = map->length;
    vector = map->vector;
    for (i = 0; i < length; i++)
	GC_MARK_ATOM(rt, vector[i], prev);
}

static void
#ifdef GC_MARK_DEBUG
gc_mark_node(JSRuntime *rt, void *thing, GCMarkNode *prev)
#else
gc_mark(JSRuntime *rt, void *thing)
#endif
{
    uint8 flags, *flagp;
    JSObject *obj;
    jsval v, *vp, *end;
    JSScope *scope;
    JSClass *clasp;
    JSScript *script;
    JSFunction *fun;
    JSScopeProperty *sprop;
    JSSymbol *sym;

    if (!thing)
	return;
    flagp = gc_find_flags(rt, thing);
    if (!flagp)
	return;

    /* Check for something on the GC freelist to handle recycled stack. */
    flags = *flagp;
    if (flags == GCF_FINAL)
	return;

#ifdef GC_MARK_DEBUG
    if (js_LiveThingToFind == thing)
        gc_dump_thing(rt, thing, flags, prev, stderr);
#endif

    if (flags & GCF_MARK)
	return;
    *flagp |= GCF_MARK;
    METER(if (++rt->gcStats.depth > rt->gcStats.maxdepth)
	      rt->gcStats.maxdepth = rt->gcStats.depth);

#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap)
        gc_dump_thing(rt, thing, flags, prev, js_DumpGCHeap);
#endif

    if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
	obj = (JSObject *) thing;
	vp = obj->slots;
	if (vp) {
	    scope = OBJ_IS_NATIVE(obj) ? OBJ_SCOPE(obj) : NULL;
	    if (scope) {
		clasp = (JSClass *) JSVAL_TO_PRIVATE(obj->slots[JSSLOT_CLASS]);

		if (clasp == &js_ScriptClass) {
		    v = vp[JSSLOT_PRIVATE];
		    if (!JSVAL_IS_VOID(v)) {
			script = (JSScript *) JSVAL_TO_PRIVATE(v);
			if (script)
			    GC_MARK_SCRIPT(rt, script, prev);
		    }
		}

		if (clasp == &js_FunctionClass) {
		    v = vp[JSSLOT_PRIVATE];
		    if (!JSVAL_IS_VOID(v)) {
			fun = (JSFunction *) JSVAL_TO_PRIVATE(v);
			if (fun) {
			    if (fun->atom)
				GC_MARK_ATOM(rt, fun->atom, prev);
			    if (fun->script)
				GC_MARK_SCRIPT(rt, fun->script, prev);
			}
		    }
		}

		for (sprop = scope->props; sprop; sprop = sprop->next) {
		    for (sym = sprop->symbols; sym; sym = sym->next) {
			if (JSVAL_IS_INT(sym_id(sym)))
			    continue;
			GC_MARK_ATOM(rt, sym_atom(sym), prev);
		    }
#if JS_HAS_GETTER_SETTER
                    if (sprop->attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
#ifdef GC_MARK_DEBUG
                        char buf[64];
                        JSAtom *atom = sym_atom(sprop->symbols);
                        const char *id = (atom && ATOM_IS_STRING(atom))
                                       ? JS_GetStringBytes(ATOM_TO_STRING(atom))
                                       : "unknown";
#endif

                        if (sprop->attrs & JSPROP_GETTER) {
#ifdef GC_MARK_DEBUG
                            JS_snprintf(buf, sizeof buf, "%s %s",
                                        id, js_getter_str);
#endif
                            GC_MARK(rt,
                                    JSVAL_TO_GCTHING((jsval)
                                        SPROP_GETTER_SCOPE(sprop, scope)),
                                    buf,
                                    prev);
                        }
                        if (sprop->attrs & JSPROP_SETTER) {
#ifdef GC_MARK_DEBUG
                            JS_snprintf(buf, sizeof buf, "%s %s",
                                        id, js_setter_str);
#endif
                            GC_MARK(rt,
                                    JSVAL_TO_GCTHING((jsval)
                                        SPROP_SETTER_SCOPE(sprop, scope)),
                                    buf,
                                    prev);
                        }
                    }
#endif /* JS_HAS_GETTER_SETTER */
		}
	    }
	    if (!scope || scope->object == obj)
		end = vp + obj->map->freeslot;
	    else
		end = vp + JS_INITIAL_NSLOTS;
	    for (; vp < end; vp++) {
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
		    GC_MARK(rt, JSVAL_TO_GCTHING(v), name, prev);
		}
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
        JSRuntime *rt = (JSRuntime *)arg;
#ifdef DEBUG
        JSArena *a;
        JSBool root_points_to_gcArenaPool = JS_FALSE;
	void *thing = JSVAL_TO_GCTHING(v);

        for (a = rt->gcArenaPool.first.next; a; a = a->next) {
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

        GC_MARK(rt, JSVAL_TO_GCTHING(v), he->value ? he->value : "root", NULL);
    }
    return HT_ENUMERATE_NEXT;
}

JS_STATIC_DLL_CALLBACK(intN)
gc_lock_marker(JSHashEntry *he, intN i, void *arg)
{
    void *thing = (void *)he->key;
    JSRuntime *rt = (JSRuntime *)arg;

    GC_MARK(rt, thing, "locked object", NULL);
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
    JS_HashTableEnumerateEntries(rt->gcRootsHash, gc_root_marker, rt);
    if (rt->gcLocksHash)
        JS_HashTableEnumerateEntries(rt->gcLocksHash, gc_lock_marker, rt);
    js_MarkAtomState(&rt->atomState, gc_mark);
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
			    GC_MARK(rt, JSVAL_TO_GCTHING(v), "stack", NULL);
		    }
		    if (end == (jsuword)sp)
			break;
		}
	    }
	    do {
		GC_MARK(rt, fp->scopeChain, "scope chain", NULL);
		GC_MARK(rt, fp->thisp, "this", NULL);
		if (JSVAL_IS_GCTHING(fp->rval))
		    GC_MARK(rt, JSVAL_TO_GCTHING(fp->rval), "rval", NULL);
		if (fp->callobj)
		    GC_MARK(rt, fp->callobj, "call object", NULL);
		if (fp->argsobj)
		    GC_MARK(rt, fp->argsobj, "arguments object", NULL);
		if (fp->script)
		    GC_MARK_SCRIPT(rt, fp->script, NULL);
		if (fp->sharpArray)
		    GC_MARK(rt, fp->sharpArray, "sharp array", NULL);
	    } while ((fp = fp->down) != NULL);
	}

	/* Cleanup temporary "dormant" linkage. */
	if (acx->fp)
	    acx->fp->dormantNext = NULL;

        /* Mark other roots-by-definition in acx. */
	GC_MARK(rt, acx->globalObject, "global object", NULL);
	GC_MARK(rt, acx->newborn[GCX_OBJECT], "newborn object", NULL);
	GC_MARK(rt, acx->newborn[GCX_STRING], "newborn string", NULL);
	GC_MARK(rt, acx->newborn[GCX_DOUBLE], "newborn double", NULL);
#if JS_HAS_EXCEPTIONS
	if (acx->throwing && JSVAL_IS_GCTHING(acx->exception))
	    GC_MARK(rt, JSVAL_TO_GCTHING(acx->exception), "exception", NULL);
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
		    goto out;
		final->next = thing;
		final->flagp = flagp;
		JS_ASSERT(rt->gcBytes >= sizeof(JSGCThing) + sizeof(uint8));
		rt->gcBytes -= sizeof(JSGCThing) + sizeof(uint8);
	    }
	    flagp++;
	}
    }

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
