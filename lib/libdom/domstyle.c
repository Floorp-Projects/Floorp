/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "domstyle.h"

static DOM_StyleSelector *
GetBaseSelector(JSContext *cx, DOM_StyleDatabase *db, DOM_StyleToken type,
		DOM_StyleToken pseudo)
{
    DOM_StyleSelector *sel = NULL;
    /* sel = HASH_LOOKUP(db->hashtable, type); */
    return sel;
}

static void
DestroySelector(JSContext *cx, DOM_StyleSelector *sel)
{
    XP_FREE(sel->selector);
    if (sel->pseudo)
	XP_FREE(sel->pseudo);
    if (sel->rules) {
	DOM_StyleRule *iter, *next;
	iter = sel->rules;
	do {
	    next = iter->next;
	    XP_FREE(iter->entry.name);
	    XP_FREE(iter->entry.value);
	    XP_FREE(iter);
	    iter = next;
	} while (iter);
    }
    XP_FREE(sel);
}

static DOM_StyleSelector *
NewSelector(JSContext *cx, DOM_StyleToken selector, DOM_StyleToken pseudo)
{
    DOM_StyleSelector *sel;
    sel = XP_NEW_ZAP(DOM_StyleSelector);
    if (!sel)
	return NULL;
    if (selector[0] == '.') {
	sel->type = SELECTOR_CLASS;
	selector++;
    } else if (selector[0] == '#') {
	sel->type = SELECTOR_ID;
	selector++;
    } else {
	sel->type = SELECTOR_TAG;
    }

    sel->selector = XP_STRDUP(selector);
    if (!sel->selector) {
	DestroySelector(cx, sel);
	return NULL;
    }

    if (pseudo) {
	sel->pseudo = XP_STRDUP(pseudo);
	if (!sel->pseudo) {
	    DestroySelector(cx, sel);
	    return NULL;
	}
    }
    
    return sel;
}

/* the pseudoclass, if any, is stored in a magic attribute */
static DOM_StyleToken
GetPseudo(JSContext *cx, DOM_Element *element)
{
    DOM_AttributeEntry *entry;

    if (!DOM_GetElementAttribute(cx, element, ":pseudoclass", &entry))
	/* report error? */
	return NULL;
    return entry ? entry->value : NULL;
}

#define ELEMENT_IS_TYPE(node, element) \
  (!XP_STRCMP(((element))->tagName, (type)))

#define PSEUDO_MATCHES(p1, p2) (!XP_STRCMP((p1), (p2)))

#define MATCH() 							      \
#ifdef DEBUG_shaver							      \
	fprintf(stderr, "selector %s:%s matches element %s\n",		      \
		sel->selector, sel->pseudo ? sel->pseudo : "",		      \
		element->tagName);					      \
#endif

#define NO_MATCH() 							      \
#ifdef DEBUG_shaver							      \
	fprintf(stderr, "selector %s:%s doesn't match element %s\n",          \
		sel->selector, sel->pseudo ? sel->pseudo : "",		      \
		element->tagName);					      \
#endif

static JSBool
SelectorMatchesToken(JSContext *cx, DOM_StyleSelector *sel,
		     DOM_StyleToken token, DOM_StyleToken pseudo)
{
    /* XXX handle #ID and .class */
    return !XPSTRCMP(sel->type, token);
}

static JSBool
SelectorMatchesElement(JSContext *cx, DOM_Element *element,
		       DOM_StyleSelector *sel)
{
    /* XXX handle class and ID */
    if (ELEMENT_IS_TYPE(element, sel->selector)) {
	/* check pseudo, if any */
	if (sel->pseudo) {
	    DOM_StyleToken elementPseudo = GetPseudo(element);
	    if (PSEUDO_MATCHES(sel->pseudo, elementPseudo)) {
		MATCH();
		return JS_TRUE;
	    }
	} else {
	    MATCH();
	    return JS_TRUE;
	}
    }
    return JS_FALSE;
}

#define SELECTOR_SCORE(sel, specificity) (specificity)

static DOM_AttributeEntry *
RuleValueFor(JSContext *cx, DOM_StyleRule *rule, DOM_StyleToken property)
{
    DOM_StyleRule *iter = rule;
    do {
	if (!XP_STRCMP(iter->entry.name, property))
	    return &iter->entry;
	iter = iter->next;
    } while (iter);
    return NULL;
}

/* Find the parent element of appropriate type for given element */
static DOM_Element *
AncestorOfType(JSContext *cx, DOM_Element *element, DOM_StyleSelector *sel)
{
    do {
	/* check type */
	if (SelectorMatchesElement(cx, element, sel))
	    return element;
	element = (DOM_Element *)element.node->parent;
    } while (element);

    return NULL;
}

/*
 * Determine if the given selector is a best-yet match for element, and
 * recurse/iterate appropriately over enclosing and sibling elements.
 */
static JSBool
CheckSelector(JSContext *cx, DOM_Element *element, DOM_StyleSelector *sel,
	      DOM_StyleToken property, DOM_AttributeEntry **entryp,
	      uintN *best, uintN specificity)
{
    DOM_AttributeEntry *entry;
    DOM_Element *next;

    /* check self */
    if (SelectorMatchesElement(cx, element, sel)) {
	/* if we have rules, maybe get an entry from them */
	if (sel->rules) {
	    uintN score = SELECTOR_SCORE(sel, specificity);
	    if (score > *best) {	/* are we the best so far? */
		entry = RuleValueFor(sel, property);
		if (entry) {	/* do we have a value for this property? */
#ifdef DEBUG_shaver
		    fprintf(stderr, "- score %d, value %s\n",
			    score, entry->value);
#endif
		    *best = score;
		    *entryp = entry;
		}
	    }
	}

	/* now, check our enclosing selector */
	if (sel->enclosing) {
	    next = AncestorOfType(cx, element, sel->enclosing);
	    if (next)
		if (!CheckSelector(cx, next, sel->enclosing, property, entryp,
				   best, specificity + 1))
		    return JS_FALSE;
	}
    }

    /* check our sibling */
    if (sel->sibling)
	if (!CheckSelector(cx, element, sel->sibling, property, entryp,
			   best, specificity))
	    return JS_FALSE;

    return JS_TRUE;
}

JSBool
DOM_StyleGetProperty(JSContext *cx, DOM_StyleDatabase *db,
		     DOM_Node *node, DOM_StyleToken property.
		     DOM_StyleToken pseudo, DOM_AttributeEntry **entryp)
{
    DOM_StyleSelector *sel;
    DOM_Element *element;
    uintN best = 0;
    
    if (node->type != NODE_TYPE_ELEMENT) {
	element = (DOM_Element *)node->parent;
	XP_ASSERT(element->node.type == NODE_TYPE_ELEMENT);
    } else {
	element = (DOM_Element *)node;
    }

    *entryp = NULL;

    sel = GetBaseSelector(db, element->tagName);
    if (!sel)
	return JS_TRUE;

    /*
     * CheckSelector will recursively find the best match for a given
     * property.
     */
    return CheckSelector(cx, element, sel, property, entryp, &best, 1);
}

DOM_StyleSelector *
DOM_StyleFindSelector(JSContext *cx, DOM_StyleDatabase *db,
		      DOM_StyleSelector *base, DOM_StyleToken enclosing,
		      DOM_StyleToken pseudo)
{
    DOM_StyleSelector *sel;

    /* looking for the base one */
    if (!base) {
	sel = GetBaseSelector(cx, db, enclosing, pseudo);
	if (!sel)
	    sel = NewSelector(cx, enclosing, pseudo);
	if (sel)
	    InsertBaseSelector(cx, db, sel);
	return sel;
    }
    
    if (!base->enclosing) {
	sel = NewSelector(cx, enclosing, pseudo);
	if (!sel)
	    return NULL;
	base->enclosing = sel;
	return sel;
    }
    
    /* check existing enclosing selectors */
    sel = base->enclosing;
    do {
	if (SelectorMatchesToken(cx, sel, enclosing, pseudo))
	    return sel;
	sel = sel->sibling;
    } while (sel);

    /* nothing found that matches, so create a new one */
    sel = NewSelector(cx, enclosing, pseudo);
    if (!sel)
	return NULL;
    sel->sibling = base->enclosing;
    base->enclosing = sel;

    return sel;
}
