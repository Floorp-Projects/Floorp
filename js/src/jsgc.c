/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JS Mark-and-Sweep Garbage Collector.
 *
 * This GC allocates only fixed-sized things big enough to contain two words
 * (pointers) on any host architecture.  It allocates from an arena pool (see
 * prarena.h).  It uses a parallel arena-pool array of flag bytes to hold the
 * mark bit, finalizer type index, etc.
 *
 * XXX swizzle page to freelist for better locality of reference
 */
#include "jsstddef.h"
#include <stdlib.h>     /* for free, called by PR_ARENA_DESTROY */
#include <string.h>	/* for memset, called by prarena.h macros if DEBUG */
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
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

static PRHashNumber   gc_hash_root(const void *key);

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

    PR_InitArenaPool(&rt->gcArenaPool, "gc-arena", GC_ARENA_SIZE,
		     sizeof(JSGCThing));
    PR_InitArenaPool(&rt->gcFlagsPool, "gc-flags", GC_FLAGS_SIZE,
		     sizeof(uint8));
    rt->gcRootsHash = PR_NewHashTable(GC_ROOTS_SIZE, gc_hash_root,
				      PR_CompareValues, PR_CompareValues,
				      NULL, NULL);
    if (!rt->gcRootsHash)
	return JS_FALSE;
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
#ifdef PR_ARENAMETER
    PR_DumpArenaStats(fp);
#endif
}
#endif

void
js_FinishGC(JSRuntime *rt)
{
#ifdef PR_ARENAMETER
    PR_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    PR_FinishArenaPool(&rt->gcArenaPool);
    PR_FinishArenaPool(&rt->gcFlagsPool);
    PR_ArenaFinish();
    PR_HashTableDestroy(rt->gcRootsHash);
    rt->gcRootsHash = NULL;
    rt->gcFreeList = NULL;
}

JSBool
js_AddRoot(JSContext *cx, void *rp, const char *name)
{
    JSRuntime *rt;
    JSBool ok;

    rt = cx->runtime;
    JS_LOCK_GC_VOID(rt,
	ok = (PR_HashTableAdd(rt->gcRootsHash, rp, (void *)name) != NULL));
    if (!ok)
	JS_ReportOutOfMemory(cx);
    return ok;
}

JSBool
js_RemoveRoot(JSContext *cx, void *rp)
{
    JSRuntime *rt;

    rt = cx->runtime;
    JS_LOCK_GC_VOID(rt, PR_HashTableRemove(rt->gcRootsHash, rp));
    return JS_TRUE;
}

void *
js_AllocGCThing(JSContext *cx, uintN flags)
{
    JSRuntime *rt;
    JSGCThing *thing;
    uint8 *flagp;
#ifdef TOO_MUCH_GC
    JSBool tried_gc = JS_TRUE;
    js_GC(cx);
#else
    JSBool tried_gc = JS_FALSE;
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
	if (rt->gcBytes < rt->gcMaxBytes) {
	    PR_ARENA_ALLOCATE(thing, &rt->gcArenaPool, sizeof(JSGCThing));
	    PR_ARENA_ALLOCATE(flagp, &rt->gcFlagsPool, sizeof(uint8));
	}
	if (!thing || !flagp) {
	    if (thing)
		PR_ARENA_RELEASE(&rt->gcArenaPool, thing);
	    if (!tried_gc) {
		JS_UNLOCK_GC(rt);
		js_GC(cx);
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
    pruword index, offset, length;
    PRArena *a, *fa;

    index = 0;
    for (a = rt->gcArenaPool.first.next; a; a = a->next) {
	offset = PR_UPTRDIFF(thing, a->base);
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

JSBool
js_LockGCThing(JSContext *cx, void *thing)
{
    uint8 *flagp, flags;

    if (!thing)
	return JS_TRUE;
    flagp = gc_find_flags(cx->runtime, thing);
    if (!flagp)
	return JS_FALSE;
    flags = *flagp;
    if ((flags & GCF_LOCKMASK) != GCF_LOCKMASK) {
	*flagp = (uint8)(flags + GCF_LOCK);
    } else {
	METER(cx->runtime->gcStats.stuck++);
    }
    METER(cx->runtime->gcStats.lock++);
    return JS_TRUE;
}

JSBool
js_UnlockGCThing(JSContext *cx, void *thing)
{
    uint8 *flagp, flags;

    if (!thing)
	return JS_TRUE;
    flagp = gc_find_flags(cx->runtime, thing);
    if (!flagp)
	return JS_FALSE;
    flags = *flagp;
    if ((flags & GCF_LOCKMASK) != GCF_LOCKMASK) {
	*flagp = (uint8)(flags - GCF_LOCK);
    } else {
	METER(cx->runtime->gcStats.unstuck++);
    }
    METER(cx->runtime->gcStats.unlock++);
    return JS_TRUE;
}

#ifdef GC_MARK_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include "prprf.h"

JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
JS_FRIEND_DATA(void *) js_LiveThingToFind;

typedef struct GCMarkNode GCMarkNode;

struct GCMarkNode {
    void        *thing;
    char        *name;
    GCMarkNode  *next;
    GCMarkNode  *prev;
};

static void
gc_dump_thing(JSGCThing *thing, uint8 flags, GCMarkNode *prev, FILE *fp)
{
    GCMarkNode *next = NULL;
    char *path = NULL;
    JSObject *obj;
    JSClass *clasp;

    while (prev) {
	next = prev;
	prev = prev->prev;
    }
    while (next) {
	path = PR_sprintf_append(path, "%s.", next->name);
	next = next->next;
    }
    if (!path)
	return;

    fprintf(fp, "%08lx ", (long)thing);
    switch (flags & GCF_TYPEMASK) {
      case GCX_OBJECT:
	obj = (JSObject *)thing;
	clasp = JSVAL_TO_PRIVATE(obj->slots[JSSLOT_CLASS]);
	fprintf(fp, "object %s", clasp->name);
	break;
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
    PR_BEGIN_MACRO                                                            \
	GCMarkNode _node;                                                     \
	_node.thing = _thing;                                                 \
	_node.name  = _name;                                                  \
	_node.next  = NULL;                                                   \
	_node.prev  = _prev;                                                  \
	if (_prev) ((GCMarkNode *)(_prev))->next = &_node;                    \
	gc_mark_node(_rt, _thing, &_node);                                    \
    PR_END_MACRO

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
	    PR_snprintf(name, sizeof name, "'%s'",
			JS_GetStringBytes(JSVAL_TO_STRING(key)));
	} else {
	    PR_snprintf(name, sizeof name, "<%x>", key);
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
	gc_dump_thing(thing, flags, prev, stderr);
#endif

    if (flags & GCF_MARK)
	return;
    *flagp |= GCF_MARK;
    METER(if (++rt->gcStats.depth > rt->gcStats.maxdepth)
	      rt->gcStats.maxdepth = rt->gcStats.depth);

#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap)
	gc_dump_thing(thing, flags, prev, js_DumpGCHeap);
#endif

    if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
	obj = thing;
	vp = obj->slots;
	if (vp) {
	    scope = OBJ_IS_NATIVE(obj) ? (JSScope *) obj->map : NULL;
	    if (scope) {
		clasp = JSVAL_TO_PRIVATE(obj->slots[JSSLOT_CLASS]);

		if (clasp == &js_ScriptClass) {
		    v = vp[JSSLOT_PRIVATE];
		    if (!JSVAL_IS_VOID(v)) {
			script = JSVAL_TO_PRIVATE(v);
			if (script)
			    GC_MARK_SCRIPT(rt, script, prev);
		    }
		}

		if (clasp == &js_FunctionClass) {
		    v = vp[JSSLOT_PRIVATE];
		    if (!JSVAL_IS_VOID(v)) {
			fun = JSVAL_TO_PRIVATE(v);
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
				    PR_snprintf(name, sizeof name,
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
				    PR_snprintf(name, sizeof name, "%ld",
						(long)JSVAL_TO_INT(nval));
				} else if (JSVAL_IS_STRING(nval)) {
				    PR_snprintf(name, sizeof name, "%s",
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

static PRHashNumber
gc_hash_root(const void *key)
{
    PRHashNumber num = (PRHashNumber) key;	/* help lame MSVC1.5 on Win16 */

    return num >> 2;
}

PR_STATIC_CALLBACK(intN)
gc_root_marker(PRHashEntry *he, intN i, void *arg)
{
    void **rp = (void **)he->key;

    if (*rp)
	GC_MARK(arg, *rp, he->value ? he->value : "root", NULL);
    return HT_ENUMERATE_NEXT;
}

JS_FRIEND_API(void)
js_ForceGC(JSContext *cx)
{
    cx->newborn[GCX_OBJECT] = NULL;
    cx->newborn[GCX_STRING] = NULL;
    cx->newborn[GCX_DOUBLE] = NULL;
    cx->runtime->gcPoke = JS_TRUE;
    js_GC(cx);
    PR_ArenaFinish();
}

void
js_GC(JSContext *cx)
{
    JSRuntime *rt;
    JSContext *iter, *acx;
    PRArena *a, *ma, *fa, **ap, **fap;
    jsval v, *vp, *sp;
    pruword begin, end;
    JSStackFrame *fp;
    void *mark;
    uint8 flags, *flagp;
    JSGCThing *thing, *final, **flp, **oflp;
    GCFinalizeOp finalizer;
    JSBool a_all_clear, f_all_clear;

    rt = cx->runtime;
#ifdef JS_THREADSAFE
    /* Avoid deadlock. */
    PR_ASSERT(!JS_IS_RUNTIME_LOCKED(rt));
#endif

    /* Let the API user decide to defer a GC if it wants to. */
    if (rt->gcCallback && !rt->gcCallback(cx, JSGC_BEGIN))
	return;

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
    /* Bump gcLevel and return rather than nest on this context. */
    if (cx->gcActive) {
	rt->gcLevel++;
	METER(if (rt->gcLevel > rt->gcStats.maxlevel)
		  rt->gcStats.maxlevel = rt->gcLevel);
	if (rt->gcLevel > 1) {
	    JS_UNLOCK_GC(rt);
	    return;
	}
    }

    /* If we're in a request, indicate, temporarily, that we're inactive. */
    if (cx->requestDepth) {
	rt->requestCount--;
	JS_NOTIFY_REQUEST_DONE(rt);
    }

    /* If another thread is already in GC, don't attempt GC; wait instead. */
    if (rt->gcLevel > 0) {
	while (rt->gcLevel > 0)
	    JS_AWAIT_GC_DONE(rt);
	if (cx->requestDepth)
	    rt->requestCount++;
	JS_UNLOCK_GC(rt);
	return;
    }

    /* No other thread is in GC, so indicate that we're now in GC. */
    rt->gcLevel = 1;

    /* Also indicate that GC is active on this context. */
    cx->gcActive = JS_TRUE;

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

    /* Drop atoms held by the property cache, and clear property weak links. */
    js_FlushPropertyCache(cx);
restart:
    rt->gcNumber++;

    /* Mark phase. */
    PR_HashTableEnumerateEntries(rt->gcRootsHash, gc_root_marker, rt);
    js_MarkAtomState(&rt->atomState, gc_mark);
    iter = NULL;
    while ((acx = js_ContextIterator(rt, &iter)) != NULL) {
	fp = acx->fp;
	if (fp) {
	    sp = fp->sp;
	    if (sp) {
		for (a = acx->stackPool.first.next; a; a = a->next) {
		    begin = a->base;
		    end = a->avail;
		    if (PR_UPTRDIFF(sp, begin) < PR_UPTRDIFF(end, begin))
			end = (pruword)sp;
		    for (vp = (jsval *)begin; vp < (jsval *)end; vp++) {
			v = *vp;
			if (JSVAL_IS_GCTHING(v))
			    GC_MARK(rt, JSVAL_TO_GCTHING(v), "stack", NULL);
		    }
		    if (end == (pruword)sp)
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
	GC_MARK(rt, acx->globalObject, "global object", NULL);
	GC_MARK(rt, acx->newborn[GCX_OBJECT], "newborn object", NULL);
	GC_MARK(rt, acx->newborn[GCX_STRING], "newborn string", NULL);
	GC_MARK(rt, acx->newborn[GCX_DOUBLE], "newborn double", NULL);
    }

    /* Sweep phase.  Mark in tempPool for release at label out:. */
    ma = cx->tempPool.current;
    mark = PR_ARENA_MARK(&cx->tempPool);
    js_SweepAtomState(&rt->atomState);
    fa = rt->gcFlagsPool.first.next;
    flagp = (uint8 *)fa->base;
    for (a = rt->gcArenaPool.first.next; a; a = a->next) {
	for (thing = (JSGCThing *)a->base; thing < (JSGCThing *)a->avail;
	     thing++) {
	    if (flagp >= (uint8 *)fa->avail) {
		fa = fa->next;
		PR_ASSERT(fa);
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
		PR_ARENA_ALLOCATE(final, &cx->tempPool, sizeof(JSGCThing));
		if (!final)
		    goto out;
		final->next = thing;
		final->flagp = flagp;
		PR_ASSERT(rt->gcBytes >= sizeof(JSGCThing) + sizeof(uint8));
		rt->gcBytes -= sizeof(JSGCThing) + sizeof(uint8);
	    }
	    flagp++;
	}
    }

    /* Finalize phase.  Don't hold the GC lock while running finalizers! */
    JS_UNLOCK_GC(rt);
    for (final = mark; ; final++) {
	if ((pruword)final >= ma->avail) {
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

    /* Free unused arenas and rebuild the freelist. */
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
	    PR_ASSERT(a);
	    if (!a) {
		METER(rt->gcStats.badarena++);
		goto out;
	    }
	    if (thing >= (JSGCThing *)a->avail) {
		if (a_all_clear) {
		    PR_ARENA_DESTROY(&rt->gcArenaPool, a, ap);
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
	    PR_ARENA_DESTROY(&rt->gcFlagsPool, fa, fap);
	    METER(rt->gcStats.fafree++);
	} else {
	    fap = &fa->next;
	    f_all_clear = JS_TRUE;
	}
    }

    /* Terminate the new freelist. */
    *flp = NULL;

out:
    PR_ARENA_RELEASE(&cx->tempPool, mark);
    if (rt->gcLevel > 1) {
	rt->gcLevel = 1;
	goto restart;
    }
    rt->gcLevel = 0;
    rt->gcLastBytes = rt->gcBytes;

#ifdef JS_THREADSAFE
    /* If we were invoked during a request, undo the temporary decrement. */
    if (cx->requestDepth)
	rt->requestCount++;
    cx->gcActive = JS_FALSE;
    JS_NOTIFY_GC_DONE(rt);
    JS_UNLOCK_GC(rt);
#endif
    if (rt->gcCallback)
	(void) rt->gcCallback(cx, JSGC_END);
}
