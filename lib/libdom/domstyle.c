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
GetBaseSelector(DOM_StyleDatabase *db, DOM_StyleToken type)
{
    DOM_StyleSelector *sel = NULL;
    /* sel = HASH_LOOKUP(db->hashtable, type); */
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

    return CheckSelector(cx, element, sel, property, entryp, &best,
			 1);
}
