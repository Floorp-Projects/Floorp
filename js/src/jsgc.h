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

#ifndef jsgc_h___
#define jsgc_h___
/*
 * JS Garbage Collector.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

JS_BEGIN_EXTERN_C

/* GC thing type indexes. */
#define GCX_OBJECT              0               /* JSObject */
#define GCX_STRING              1               /* JSString */
#define GCX_DOUBLE              2               /* jsdouble */
#define GCX_EXTERNAL_STRING     3               /* JSString w/ external chars */
#define GCX_NTYPES_LOG2         3
#define GCX_NTYPES              JS_BIT(GCX_NTYPES_LOG2)

/* GC flag definitions (type index goes in low bits). */
#define GCF_TYPEMASK    JS_BITMASK(GCX_NTYPES_LOG2)
#define GCF_MARK        JS_BIT(GCX_NTYPES_LOG2)
#define GCF_FINAL       JS_BIT(GCX_NTYPES_LOG2 + 1)
#define GCF_LOCKSHIFT   (GCX_NTYPES_LOG2 + 2)   /* lock bit shift and mask */
#define GCF_LOCKMASK    (JS_BITMASK(8 - GCF_LOCKSHIFT) << GCF_LOCKSHIFT)
#define GCF_LOCK        JS_BIT(GCF_LOCKSHIFT)   /* lock request bit in API */

#if 1
/*
 * Since we're forcing a GC from JS_GC anyway, don't bother wasting cycles
 * loading oldval.  XXX remove implied force, etc.
 */
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JS_TRUE)
#else
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JSVAL_IS_GCTHING(oldval))
#endif

extern intN
js_ChangeExternalStringFinalizer(JSStringFinalizeOp oldop,
                                 JSStringFinalizeOp newop);

extern JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes);

extern void
js_FinishGC(JSRuntime *rt);

extern JSBool
js_AddRoot(JSContext *cx, void *rp, const char *name);

extern JSBool
js_RemoveRoot(JSRuntime *rt, void *rp);

extern void *
js_AllocGCThing(JSContext *cx, uintN flags);

extern JSBool
js_LockGCThing(JSContext *cx, void *thing);

extern JSBool
js_UnlockGCThing(JSContext *cx, void *thing);

extern void
js_MarkAtom(JSContext *cx, JSAtom *atom, void *arg);

extern void
js_MarkGCThing(JSContext *cx, void *thing, void *arg);

#ifdef GC_MARK_DEBUG

typedef struct GCMarkNode GCMarkNode;

struct GCMarkNode {
    void        *thing;
    const char  *name;
    GCMarkNode  *next;
    GCMarkNode  *prev;
};

#define GC_MARK(_cx, _thing, _name, _prev)                                    \
    JS_BEGIN_MACRO                                                            \
        GCMarkNode _node;                                                     \
        _node.thing = _thing;                                                 \
        _node.name  = _name;                                                  \
        _node.next  = NULL;                                                   \
        _node.prev  = _prev;                                                  \
        if (_prev) ((GCMarkNode *)(_prev))->next = &_node;                    \
        js_MarkGCThing(_cx, _thing, &_node);                                  \
    JS_END_MACRO

#else  /* !GC_MARK_DEBUG */

#define GC_MARK(cx, thing, name, prev)   js_MarkGCThing(cx, thing, NULL)

#endif /* !GC_MARK_DEBUG */

extern JS_FRIEND_API(void)
js_ForceGC(JSContext *cx);

/*
 * Flags to modify how a GC marks and sweeps:
 *   GC_KEEP_ATOMS      Don't sweep unmarked atoms, they may be in use by the
 *                      compiler, or by an API function that calls js_Atomize,
 *                      when the GC is called from js_AllocGCThing, due to a
 *                      malloc failure or runtime GC-thing limit.
 *   GC_LAST_CONTEXT    Called from js_DestroyContext for last JSContext in a
 *                      JSRuntime, when it is imperative that rt->gcPoke gets
 *                      cleared early in js_GC, if it is set.
 */
#define GC_KEEP_ATOMS       0x1
#define GC_LAST_CONTEXT     0x2

extern void
js_GC(JSContext *cx, uintN gcflags);

#ifdef JS_GCMETER

typedef struct JSGCStats {
    uint32  alloc;      /* number of allocation attempts */
    uint32  freelen;    /* gcFreeList length */
    uint32  recycle;    /* number of things recycled through gcFreeList */
    uint32  retry;      /* allocation attempt retries after running the GC */
    uint32  fail;       /* allocation failures */
    uint32  lock;       /* valid lock calls */
    uint32  unlock;     /* valid unlock calls */
    uint32  stuck;      /* stuck reference counts seen by lock calls */
    uint32  unstuck;    /* unlock calls that saw a stuck lock count */
    uint32  depth;      /* mark recursion depth */
    uint32  maxdepth;   /* maximum mark recursion depth */
    uint32  maxlevel;   /* maximum GC nesting (indirect recursion) level */
    uint32  poke;       /* number of potentially useful GC calls */
    uint32  nopoke;     /* useless GC calls where js_PokeGC was not set */
    uint32  badarena;   /* thing arena corruption */
    uint32  badflag;    /* flags arena corruption */
    uint32  afree;      /* thing arenas freed so far */
    uint32  fafree;     /* flags arenas freed so far */
} JSGCStats;

extern void
js_DumpGCStats(JSRuntime *rt, FILE *fp);

#endif /* JS_GCMETER */

JS_END_EXTERN_C

#endif /* jsgc_h___ */
