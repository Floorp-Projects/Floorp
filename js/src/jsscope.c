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
 * JS symbol tables.
 */
#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsscope.h"
#include "jsstr.h"

PR_STATIC_CALLBACK(PRHashNumber)
js_hash_id(const void *key)
{
    jsval v;
    const JSAtom *atom;

    v = (jsval)key;
    if (JSVAL_IS_INT(v))
	return JSVAL_TO_INT(v);
    atom = key;
    return atom->number;
}

PR_STATIC_CALLBACK(void *)
js_alloc_scope_space(void *priv, size_t size)
{
    return JS_malloc(priv, size);
}

PR_STATIC_CALLBACK(void)
js_free_scope_space(void *priv, void *item)
{
    JS_free(priv, item);
}

PR_STATIC_CALLBACK(PRHashEntry *)
js_alloc_symbol(void *priv, const void *key)
{
    JSContext *cx;
    JSSymbol *sym;
    JSAtom *atom;

    cx = priv;
    PR_ASSERT(JS_IS_LOCKED(cx));
    sym = JS_malloc(cx, sizeof(JSSymbol));
    if (!sym)
	return NULL;
    sym->entry.key = key;
    if (!JSVAL_IS_INT((jsval)key)) {
	atom = (JSAtom *)key;
	js_HoldAtom(cx, atom);
    }
    return &sym->entry;
}

PR_STATIC_CALLBACK(void)
js_free_symbol(void *priv, PRHashEntry *he, uintN flag)
{
    JSContext *cx;
    JSSymbol *sym, **sp;
    JSProperty *prop;

    cx = priv;
    PR_ASSERT(JS_IS_LOCKED(cx));
    sym = (JSSymbol *)he;
    prop = sym->entry.value;
    if (prop) {
	sym->entry.value = NULL;
	prop = js_DropProperty(cx, prop);
	if (prop) {
	    for (sp = &prop->symbols; *sp; sp = &(*sp)->next) {
		if (*sp == sym) {
		    *sp = sym->next;
		    if (!*sp)
			break;
		}
	    }
	    sym->next = NULL;
	}
    }

    if (flag == HT_FREE_ENTRY) {
	if (!JSVAL_IS_INT(sym_id(sym)))
	    JS_LOCK_VOID(cx, js_DropAtom(cx, sym_atom(sym)));
	JS_free(cx, he);
    }
}

static PRHashAllocOps hash_scope_alloc_ops = {
    js_alloc_scope_space, js_free_scope_space,
    js_alloc_symbol, js_free_symbol
};

/************************************************************************/

PR_STATIC_CALLBACK(JSSymbol *)
js_hash_scope_lookup(JSContext *cx, JSScope *scope, jsval id, PRHashNumber hash)
{
    PRHashTable *table = scope->data;
    PRHashEntry **hep;
    JSSymbol *sym;

    hep = PR_HashTableRawLookup(table, hash, (const void *)id);
    sym = (JSSymbol *) *hep;
    return sym;
}

#define SCOPE_ADD(CLASS_SPECIFIC_CODE)                                        \
    PR_BEGIN_MACRO                                                            \
	if (sym) {                                                            \
	    if (sym->entry.value == prop)                                     \
		return sym;                                                   \
	    if (sym->entry.value)                                             \
		js_free_symbol(cx, &sym->entry, HT_FREE_VALUE);               \
	} else {                                                              \
	    CLASS_SPECIFIC_CODE                                               \
	    sym->scope = scope;                                               \
	    sym->next = NULL;                                                 \
	}                                                                     \
	if (prop) {                                                           \
	    sym->entry.value = js_HoldProperty(cx, prop);                     \
	    for (sp = &prop->symbols; *sp; sp = &(*sp)->next)                 \
		;                                                             \
	    *sp = sym;                                                        \
	} else {                                                              \
	    sym->entry.value = NULL;                                          \
	}                                                                     \
    PR_END_MACRO

PR_STATIC_CALLBACK(JSSymbol *)
js_hash_scope_add(JSContext *cx, JSScope *scope, jsval id, JSProperty *prop)
{
    PRHashTable *table = scope->data;
    const void *key;
    PRHashNumber keyHash;
    PRHashEntry **hep;
    JSSymbol *sym, **sp;

    PR_ASSERT(JS_IS_LOCKED(cx));
    table->allocPriv = cx;
    key = (const void *)id;
    keyHash = js_hash_id(key);
    hep = PR_HashTableRawLookup(table, keyHash, key);
    sym = (JSSymbol *) *hep;
    SCOPE_ADD(
	sym = (JSSymbol *) PR_HashTableRawAdd(table, hep, keyHash, key, NULL);
	if (!sym)
	    return NULL;
    );
    return sym;
}

PR_STATIC_CALLBACK(JSBool)
js_hash_scope_remove(JSContext *cx, JSScope *scope, jsval id)
{
    PRHashTable *table = scope->data;

    PR_ASSERT(JS_IS_LOCKED(cx));
    table->allocPriv = cx;
    return PR_HashTableRemove(table, (const void *)id);
}

/* Forward declaration for use by js_hash_scope_clear(). */
extern JSScopeOps js_list_scope_ops;

PR_STATIC_CALLBACK(void)
js_hash_scope_clear(JSContext *cx, JSScope *scope)
{
    PRHashTable *table = scope->data;

    PR_ASSERT(JS_IS_LOCKED(cx));
    table->allocPriv = cx;
    PR_HashTableDestroy(table);
    scope->ops = &js_list_scope_ops;
    scope->data = NULL;
}

JSScopeOps js_hash_scope_ops = {
    js_hash_scope_lookup,
    js_hash_scope_add,
    js_hash_scope_remove,
    js_hash_scope_clear
};

/************************************************************************/

PR_STATIC_CALLBACK(JSSymbol *)
js_list_scope_lookup(JSContext *cx, JSScope *scope, jsval id, PRHashNumber hash)
{
    JSSymbol *sym, **sp;

    for (sp = (JSSymbol **)&scope->data; (sym = *sp) != 0;
	 sp = (JSSymbol **)&sym->entry.next) {
	if (sym_id(sym) == id) {
	    /* Move sym to the front for shorter searches. */
	    *sp = (JSSymbol *)sym->entry.next;
	    sym->entry.next = scope->data;
	    scope->data = sym;
	    return sym;
	}
    }
    return NULL;
}

#define HASH_THRESHOLD	5

PR_STATIC_CALLBACK(JSSymbol *)
js_list_scope_add(JSContext *cx, JSScope *scope, jsval id, JSProperty *prop)
{
    JSSymbol *list = scope->data;
    uint32 nsyms;
    JSSymbol *sym, *next, **sp;
    PRHashTable *table;
    PRHashEntry **hep;

    PR_ASSERT(JS_IS_LOCKED(cx));
    nsyms = 0;
    for (sym = list; sym; sym = (JSSymbol *)sym->entry.next) {
	if (sym_id(sym) == id)
	    break;
	nsyms++;
    }

    if (nsyms >= HASH_THRESHOLD) {
	table = PR_NewHashTable(nsyms, js_hash_id,
			        PR_CompareValues, PR_CompareValues,
			        &hash_scope_alloc_ops, cx);
	if (table) {
	    for (sym = list; sym; sym = next) {
		/* Save next for loop update, before it changes in lookup. */
		next = (JSSymbol *)sym->entry.next;

		/* Now compute missing keyHash fields. */
		sym->entry.keyHash = js_hash_id(sym->entry.key);
		sym->entry.next = NULL;
		hep = PR_HashTableRawLookup(table,
					    sym->entry.keyHash,
					    sym->entry.key);
		*hep = &sym->entry;
	    }
	    table->nentries = nsyms;
	    scope->ops = &js_hash_scope_ops;
	    scope->data = table;
	    return scope->ops->add(cx, scope, id, prop);
	}
    }

    SCOPE_ADD(
	sym = (JSSymbol *)js_alloc_symbol(cx, (const void *)id);
	if (!sym)
	    return NULL;
	/* Don't set keyHash until we know we need it, above. */
	sym->entry.next = scope->data;
	scope->data = sym;
    );
    return sym;
}

PR_STATIC_CALLBACK(JSBool)
js_list_scope_remove(JSContext *cx, JSScope *scope, jsval id)
{
    JSSymbol *sym, **sp;

    PR_ASSERT(JS_IS_LOCKED(cx));
    for (sp = (JSSymbol **)&scope->data; (sym = *sp) != 0;
	 sp = (JSSymbol **)&sym->entry.next) {
	if (sym_id(sym) == id) {
	    *sp = (JSSymbol *)sym->entry.next;
	    js_free_symbol(cx, &sym->entry, HT_FREE_ENTRY);
	    return JS_TRUE;
	}
    }
    return JS_FALSE;
}

PR_STATIC_CALLBACK(void)
js_list_scope_clear(JSContext *cx, JSScope *scope)
{
    JSSymbol *sym;

    PR_ASSERT(JS_IS_LOCKED(cx));
    while ((sym = scope->data) != NULL) {
	scope->data = sym->entry.next;
	js_free_symbol(cx, &sym->entry, HT_FREE_ENTRY);
    }
}

JSScopeOps js_list_scope_ops = {
    js_list_scope_lookup,
    js_list_scope_add,
    js_list_scope_remove,
    js_list_scope_clear
};

/************************************************************************/

JSScope *
js_GetMutableScope(JSContext *cx, JSObject *obj)
{
    JSScope *scope, *newscope;

    PR_ASSERT(JS_IS_LOCKED(cx));
    scope = (JSScope *)obj->map;
    if (scope->object == obj)
	return scope;
    newscope = js_NewScope(cx, obj->map->clasp, obj);
    if (!newscope)
	return NULL;
    obj->map = (JSObjectMap *)js_HoldScope(cx, newscope);
    js_DropScope(cx, scope);
    return newscope;
}

#if SCOPE_TABLE
static PR_HashTable *scope_table;

#define entry2scope(he) ((JSScope *)((char *)(he) - offsetof(JSScope, entry)))

PR_STATIC_CALLBACK(PRHashNumber)
js_scope_hash(const void *key)
{
}

PR_STATIC_CALLBACK(int)
js_scope_compare(const void *v1, const void *v2)
{
}

PR_STATIC_CALLBACK(PRHashEntry *)
js_alloc_scope(void *priv, const void *key)
{
    JSScope *scope;

    scope = JS_malloc(priv, sizeof(JSScope));
    if (!scope)
	return NULL;
    return &scope->entry;
}

PR_STATIC_CALLBACK(void)
js_free_scope(void *priv, PRHashEntry *he, uintN flag)
{
    JSScope *scope;

    scope = entry2scope(he);
    JS_free(priv, scope);
}

static PRHashAllocOps scope_table_alloc_ops = {
    js_alloc_scope_space, js_free_scope_space,
    js_alloc_scope, js_free_scope
};
#endif /* SCOPE_TABLE */

JSScope *
js_MutateScope(JSContext *cx, JSObject *obj, jsval id,
	       JSPropertyOp getter, JSPropertyOp setter, uintN flags,
	       JSProperty **propp)
{
    PR_ASSERT(JS_IS_LOCKED(cx));
    /* XXX pessimal */
    *propp = NULL;
    return js_GetMutableScope(cx, obj);
}

JSScope *
js_NewScope(JSContext *cx, JSClass *clasp, JSObject *obj)
{
    JSScope *scope;
#if SCOPE_TABLE
    PRHashEntry *he, **hep;

    if (scope_table) {
	scope_table = PR_NewHashTable(INIT_SCOPE_TABLE_SIZE, js_scope_hash,
				      js_scope_compare, PR_CompareValues,
				      &scope_table_alloc_ops, cx);
	if (!scope_table)
	    return NULL;
    } else {
	scope_table->allocPriv = cx;
    }
    hep = PR_HashTableRawLookup(scope_table, NULL, NULL);
    he  = PR_HashTableRawAdd(scope_table, hep, NULL, NULL, hep);
    if (!he)
	return NULL;
    scope = entry2scope(he);
#else
    scope = JS_malloc(cx, sizeof(JSScope));
    if (!scope)
	return NULL;
#endif
    scope->map.nrefs = 0;
    scope->map.nslots = 0;
    if (clasp->flags & JSCLASS_HAS_PRIVATE)
	scope->map.freeslot = JSSLOT_PRIVATE + 1;
    else
	scope->map.freeslot = JSSLOT_START;
    scope->map.clasp = clasp;
    scope->map.props = NULL;
    scope->object = obj;
    scope->proptail = &scope->map.props;
    scope->ops = &js_list_scope_ops;
    scope->data = NULL;
    return scope;
}

void
js_DestroyScope(JSContext *cx, JSScope *scope)
{
    JSProperty *prop;

    PR_ASSERT(JS_IS_LOCKED(cx));
    for (prop = scope->map.props; prop; prop = prop->next)
	prop->object = NULL;
    scope->ops->clear(cx, scope);
#if SCOPE_TABLE
    PR_HashTableRawRemove(scope_table, scope->entry.value, &scope->entry);
#endif
    JS_free(cx, scope);
}

JSScope *
js_HoldScope(JSContext *cx, JSScope *scope)
{
    PR_ASSERT(JS_IS_LOCKED(cx));
    PR_ASSERT(scope->map.nrefs >= 0);
    scope->map.nrefs++;
    return scope;
}

JSScope *
js_DropScope(JSContext *cx, JSScope *scope)
{
    PR_ASSERT(JS_IS_LOCKED(cx));
    PR_ASSERT(scope->map.nrefs > 0);
    if (--scope->map.nrefs == 0) {
	js_DestroyScope(cx, scope);
	return NULL;
    }
    return scope;
}

PRHashNumber
js_HashValue(jsval v)
{
    return js_hash_id((const void *)v);
}

jsval
js_IdToValue(jsval id)
{
    JSAtom *atom;

    if (JSVAL_IS_INT(id))
	return id;
    atom = (JSAtom *)id;
    return ATOM_KEY(atom);
}

JSProperty *
js_NewProperty(JSContext *cx, JSScope *scope, jsval id,
	       JSPropertyOp getter, JSPropertyOp setter, uintN flags)
{
    uint32 slot;
    JSProperty *prop;

    PR_ASSERT(JS_IS_LOCKED(cx));
    if (!js_AllocSlot(cx, scope->object, &slot))
	return NULL;
    prop = JS_malloc(cx, sizeof(JSProperty));
    if (!prop) {
	js_FreeSlot(cx, scope->object, slot);
	return NULL;
    }
    prop->nrefs = 0;
    prop->id = js_IdToValue(id);
    prop->getter = getter;
    prop->setter = setter;
    prop->slot = slot;
    prop->flags = flags;
    prop->spare = 0;
    prop->object = scope->object;
    prop->symbols = NULL;
    prop->next = NULL;
    prop->prevp = scope->proptail;
    *scope->proptail = prop;
    scope->proptail = &prop->next;
    return prop;
}

void
js_DestroyProperty(JSContext *cx, JSProperty *prop)
{
    JSObject *obj;
    JSScope *scope;

    /*
     * Test whether obj was finalized before prop's last dereference.
     *
     * This can happen when a deleted property is held by a property iterator
     * object (which points to obj, keeping obj alive so long as the property
     * iterator is reachable).  As soon as the GC finds the iterator to be
     * unreachable, it will finalize the iterator, and may also finalize obj,
     * in an unpredictable order.  If obj is finalized first, its map will be
     * null below, and we need only free prop.
     *
     * In the more typical case of a property whose last reference (from a
     * symbol in obj's scope) goes away before obj is finalized, we must be
     * sure to free prop's slot and unlink it from obj's property list.
     */
    obj = prop->object;
    if (obj) {
	scope = (JSScope *)obj->map;
	if (scope && scope->object) {
	    js_FreeSlot(cx, obj, prop->slot);
	    *prop->prevp = prop->next;
	    if (prop->next)
		prop->next->prevp = prop->prevp;
	    else
		scope->proptail = prop->prevp;
	}
    }

    /* Purge any cached weak links to prop, then free it. */
    js_FlushPropertyCacheByProp(cx, prop);
    JS_free(cx, prop);
}

JSProperty *
js_HoldProperty(JSContext *cx, JSProperty *prop)
{
    PR_ASSERT(JS_IS_LOCKED(cx));
    if (prop) {
	PR_ASSERT(prop->nrefs >= 0);
	prop->nrefs++;
    }
    return prop;
}

JSProperty *
js_DropProperty(JSContext *cx, JSProperty *prop)
{
    PR_ASSERT(JS_IS_LOCKED(cx));
    if (prop) {
	PR_ASSERT(prop->nrefs > 0);
	if (--prop->nrefs == 0) {
	    js_DestroyProperty(cx, prop);
	    prop = NULL;
	}
    }
    return prop;
}
