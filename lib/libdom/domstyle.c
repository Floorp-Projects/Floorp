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
 * DOM Style information, using JSSS.
 */

#include "domstyle.h"
#include "dom_priv.h"

#ifdef DEBUG_shaver
/* #define DEBUG_shaver_style_primitives 1 */
/* #define DEBUG_shaver_verbose 1 */
/* #define DEBUG_shaver_SME 1 */
#endif

#define STYLE_DB_FROM_CX(db, cx)                                              \
PR_BEGIN_MACRO                                                                \
    {                                                                         \
        (db) = DOM_StyleDatabaseFromContext(cx);                              \
        if (!db)                                                              \
            return JS_FALSE;                                                  \
    }                                                                         \
PR_END_MACRO

/* if either is present, they must both be and match */
#define PSEUDO_MATCHES(p1, p2) ((p1 || p2) ?                                  \
                                (p1 && p2 && !XP_STRCMP(p1, p2)) :            \
                                JS_TRUE)

static int
CompareSelectors(const void *v1, const void *v2)
{
    DOM_StyleSelector *s1 = (DOM_StyleSelector *)v1,
        *s2 = (DOM_StyleSelector *)v2;

    return s1->type == s2->type &&
        !XP_STRCMP(s1->selector, s2->selector) &&

        /* if both have a psuedo, they have to match, else both must be NULL */
        (s1->pseudo && s2->pseudo ?
         XP_STRCMP(s1->pseudo, s2->pseudo) :
         !s1->pseudo && !s2->pseudo);
}

DOM_StyleDatabase *
DOM_NewStyleDatabase(JSContext *cx)
{
    DOM_StyleDatabase *db = XP_NEW_ZAP(DOM_StyleDatabase);
    if (!db)
        return NULL;

    db->ht = PL_NewHashTable(32, PL_HashString, PL_CompareStrings,
                             CompareSelectors, NULL, NULL);
    if (!db->ht) {
        XP_FREE(db);
        return NULL;
    }

    return db;
}

void
DOM_DestroyStyleDatabase(JSContext *cx, DOM_StyleDatabase *db)
{
    PL_HashTableDestroy(db->ht);
    XP_FREE(db);
}

static JSBool
InsertBaseSelector(JSContext *cx, DOM_StyleDatabase *db,
                   DOM_StyleSelector *sel)
{
    DOM_StyleSelector *base, *iter;
    base = PL_HashTableLookup(db->ht, sel->selector);
    if (!base) {
        return PL_HashTableAdd(db->ht, sel->selector, sel) != NULL;
    } 

    /*
     * We need to keep these properly ordered, so that we always find the
     * best match first.  This means that "selector"/"extra" must always
     * appear before "selector"/NULL.
     */
    if (sel->extra) {
        /* these go at the beginning, so we can stick it right here */
        sel->sibling = base;
        return PL_HashTableRemove(db->ht, sel->selector) && 
            (PL_HashTableAdd(db->ht, sel->selector, sel) != NULL);
    }

    /*
     * This is O(n), which is a bit sucky.  We could cache the tail
     * somewhere if it becomes an issue.
     */
    for (iter = base; iter->sibling; iter = iter->sibling)
        /* nothing */ ;
    iter->sibling = sel;
    return JS_TRUE;
}

/*
 * Does the selector match the tokens (the parts that matter for this class)?
 * strict means that document.classes.foo.all does NOT match
 * document.classes.foo.H4, for example.
 */

static JSBool
SelectorMatchesToken(JSContext *cx, DOM_StyleSelector *sel, uint8 type,
                     DOM_StyleToken token, DOM_StyleToken extra,
                     DOM_StyleToken pseudo, JSBool strict)
{

    if (sel->type != type ||
        XP_STRCMP(sel->selector, token))
        return JS_FALSE;

    switch (type) {
      case SELECTOR_CLASS:
        if (strict) {
            if (!PSEUDO_MATCHES(sel->extra, extra))
                return JS_FALSE;
        } else {
            if (sel->extra && (!extra || XP_STRCMP(extra, sel->extra)))
                return JS_FALSE;
        }
        /* FALLTHROUGH */
      case SELECTOR_TAG:
        if (strict) {
            if (!PSEUDO_MATCHES(sel->pseudo, pseudo))
                return JS_FALSE;
        } else {
            if (sel->pseudo && (!pseudo || XP_STRCMP(pseudo, sel->pseudo)))
                return JS_FALSE;
        }
      case SELECTOR_ID:
        return JS_TRUE;
      default:
        XP_ASSERT(0 && "unknown selector type in base selector");
        return JS_FALSE;
    }
}

static DOM_StyleSelector *
GetBaseSelector(JSContext *cx, DOM_StyleDatabase *db, uint8 type,
                DOM_StyleToken token, DOM_StyleToken extra,
                DOM_StyleToken pseudo, JSBool strict)
{
    DOM_StyleSelector *sel;

    if (!cx)
        return NULL;

    if (!db) {
        db = DOM_StyleDatabaseFromContext(cx);
#ifdef DEBUG_shaver
        fprintf(stderr, "got new db %p\n", db);
#endif
        if (!db)
            return NULL;
    }

    sel = PL_HashTableLookup(db->ht, token);
    for (; sel; sel = sel->sibling) {
        if (SelectorMatchesToken(cx, sel, type, token, extra, pseudo, strict))
            return sel;
    }
    return NULL;
}

static void
DestroySelector(JSContext *cx, DOM_StyleSelector *sel)
{
    XP_FREE((char *)sel->selector);
    if (sel->pseudo)
        XP_FREE((char *)sel->pseudo);
    if (sel->extra)
        XP_FREE((char *)sel->extra);
    if (sel->rules) {
        DOM_StyleRule *iter, *next;
        iter = sel->rules;
        do {
            next = iter->next;
            XP_FREE((char *)iter->entry.name);
            XP_FREE((char *)iter->entry.value);
            XP_FREE(iter);
            iter = next;
        } while (iter);
    }
    XP_FREE(sel);
}

static DOM_StyleSelector *
NewSelector(JSContext *cx, uint8 type, DOM_StyleToken selector,
            DOM_StyleToken extra, DOM_StyleToken pseudo)
{
    DOM_StyleSelector *sel;
    sel = XP_NEW_ZAP(DOM_StyleSelector);
    if (!sel)
        return NULL;
    sel->type = type;
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

    if (extra) {
        sel->extra = XP_STRDUP(extra);
        if (!sel->extra) {
            DestroySelector(cx, sel);
            return NULL;
        }
    }

    return sel;
}

/* the pseudoclass, if any, is stored in a magic attribute */
DOM_StyleToken
DOM_GetElementPseudo(JSContext *cx, DOM_Element *element)
{
    DOM_AttributeEntry *entry;

    if (!DOM_GetElementAttribute(cx, element, "dom:pseudoclass", &entry))
        /* report error? */
        return NULL;
    return entry ? entry->value : NULL;
}

JSBool
DOM_SetElementPseudo(JSContext *cx, DOM_Element *element,
                     DOM_StyleToken pseudo)
{
    /* don't run the callbacks for attribute setting */
    return dom_SetElementAttribute(cx, element, XP_STRDUP("dom:pseudoclass"),
                                   XP_STRDUP(pseudo), JS_FALSE);
}

#ifdef DEBUG_shaver_SME
#define MATCH()                                                               \
        fprintf(stderr, "[selector %s:%s matches element %s:%s]",             \
                sel->selector, sel->pseudo ? sel->pseudo : "<none>",          \
                element->tagName, elementPseudo);
#else
#define MATCH()
#endif

#ifdef DEBUG_shaver_SME
#define NO_MATCH()                                                            \
        fprintf(stderr, "[selector %s:%s doesn't match element %s:%s]",       \
                sel->selector, sel->pseudo ? sel->pseudo : "<none>",          \
                element->tagName, elementPseudo);
#else
#define NO_MATCH()
#endif

static JSBool
SelectorMatchesElement(JSContext *cx, DOM_Element *element,
                       DOM_StyleSelector *sel)
{
    DOM_StyleToken elementPseudo = DOM_GetElementPseudo(cx, element);

#ifdef DEBUG_shaver_SME
    fprintf(stderr,
            "[checking selector %d:%s/%s:%s against element %s/%s/%s:%s]",
            sel->type, sel->selector, sel->extra, sel->pseudo,
            element->styleID, element->styleClass, element->tagName,
            elementPseudo);
#endif
    switch(sel->type) {
      case SELECTOR_TAG:
        if (XP_STRCMP(sel->selector, element->tagName))
            return JS_FALSE;
        /* check pseudo, if any */
        if (sel->pseudo) {
            if (PSEUDO_MATCHES(sel->pseudo, elementPseudo)) {
                MATCH();
                return JS_TRUE;
            }
            NO_MATCH();
            return JS_FALSE;
        } else {
            MATCH();
            return JS_TRUE;
        }
      case SELECTOR_ID:
        return element->styleID && !XP_STRCMP(sel->selector, element->styleID);
      case SELECTOR_CLASS:
        if (XP_STRCMP(sel->selector, element->styleClass) ||
            (sel->extra && XP_STRCMP(sel->extra, element->tagName)))
            return JS_FALSE;
        return JS_TRUE;
    }
    return JS_FALSE;
}

static DOM_AttributeEntry *
RuleValueFor(JSContext *cx, DOM_StyleRule *rule, DOM_StyleToken property)
{
    DOM_StyleRule *iter = rule;
    do {
        if (!XP_STRCMP(iter->entry.name, property)) {
            return &iter->entry;
        }
        iter = iter->next;
    } while (iter);
    return NULL;
}

static DOM_StyleRule *
AddRule(JSContext *cx, DOM_StyleSelector *sel, const char *name,
        const char *value, int16 weight)
{
    DOM_StyleRule *rule;
#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "adding rule %s = %s to %d:%s/%s:%s\n", name, value,
            sel->type, sel->selector, sel->extra, sel->pseudo);
#endif
    rule = XP_NEW_ZAP(DOM_StyleRule);
    if (!rule)
        return NULL;
    rule->entry.name = XP_STRDUP(name);
    rule->entry.value = XP_STRDUP(value);
    rule->entry.dirty = JS_TRUE;
    rule->weight = weight;
    rule->next = sel->rules;
    sel->rules = rule;
    return rule;
}

/* Find the parent element of appropriate type for given element */
static DOM_Element *
AncestorOfType(JSContext *cx, DOM_Element *element, DOM_StyleSelector *sel)
{
    do {
        /* check type */
        if (SelectorMatchesElement(cx, element, sel)) {
            return element;
        }
        element = (DOM_Element *)element->node.parent;
    } while (element && element->node.type == NODE_TYPE_ELEMENT);

    return NULL;
}

/*
 * From libstyle/jssrules.h:
 * "Specificity is implemented as three 8-bit components: the number
 * of tags in the selector (the least significant component), the
 * number of classes in the selector, and the number of ids in the
 * selector (the most significant component)."
 *
 * We use the lower 8 bits to count the ``enclosure depth'' of a rule
 * in SELECTOR_SCORE below, so that "H1 EM {color:blue}" can override
 * "EM {color:red}" without interfering with the other specificity
 * scores.  Because we use the lower 8 bits for this, the depth will
 * only matter for selectors that otherwise had the same score, unless
 * we get a depth of more than 255 elements.  In that case, I'll try
 * to care.
 */
#define CSS_SPECIFICITY(ids, classes, tags) \
  ((uint32)(ids << 24) | (uint32)(classes << 16) | (uint32)(tags << 8))

static uint32
ScoreSelector(DOM_StyleSelector *sel)
{
    switch(sel->type) {
      case SELECTOR_ID:
        return CSS_SPECIFICITY(1, 0, 0);
      case SELECTOR_CLASS:
        if (sel->extra)        /* sel->extra is NULL for .all */
            return CSS_SPECIFICITY(0, 1, 1);
        else
            return CSS_SPECIFICITY(0, 1, 0);
      case SELECTOR_TAG:
        return CSS_SPECIFICITY(0, 0, 1);
      default:
        XP_ASSERT(0 && "bogus selector type in ScoreSelector");
        return 0;
    }
}

#define SELECTOR_SCORE(sel, specificity) (ScoreSelector(sel) + specificity)

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
    DOM_StyleSelector *iter;

    /* check self */
    if (SelectorMatchesElement(cx, element, sel)) {
        /* if we have rules, maybe get an entry from them */
        if (sel->rules) {
            uintN score = SELECTOR_SCORE(sel, specificity);
            if (score > *best) {        /* are we the best so far? */
                entry = RuleValueFor(cx, sel->rules, property);
                if (entry) {    /* do we have a value for this property? */
#ifdef DEBUG_shaver_style_primitives
                    fprintf(stderr, "[+SCORE %d, VALUE %s]", score,
                            entry->value);
#endif
                    *best = score;
                    *entryp = entry;
                }
            } else {
#ifdef DEBUG_shaver_style_primitives
                entry = RuleValueFor(cx, sel->rules, property);
                if (entry) {    /* do we have a value for this property? */
                    fprintf(stderr, "[-score %d, value %s]", score,
                            entry->value);
                }
#endif
            }
        }

        /* now, check our enclosing selector */
        iter = sel->enclosing;
        while(iter) {
            next = AncestorOfType(cx, element, iter);
            if (next) {
                if (!CheckSelector(cx, next, iter, property, entryp,
                                   best, specificity + 1))
                    return JS_FALSE;
                return JS_TRUE;
            }
            iter = iter->sibling;
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
                     DOM_Node *node, DOM_StyleToken property,
                     DOM_AttributeEntry **entryp)
{
    DOM_StyleSelector *sel;
    DOM_Element *element, *iter;
    JSBool ok;
    uintN best = 0;
    uint8 type;
    
    *entryp = NULL;

    if (node->type != NODE_TYPE_ELEMENT) {
        element = (DOM_Element *)node->parent;
        if (!element || element->node.type != NODE_TYPE_ELEMENT)
            return JS_TRUE;
#ifdef DEBUG_shaver_style_primitives
        fprintf(stderr, "(node is type %d, using parent %d/%s:%s@%p) ",
                node->type, node->parent->type, element->tagName,
                DOM_GetElementPseudo(cx, element), node->parent);
#endif
    } else {
        element = (DOM_Element *)node;
    }

    if (!db)
        STYLE_DB_FROM_CX(db, cx);

#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "\nGetProperty: looking for %s/%s/%s:%s.%s: ",
            element->styleID, element->styleClass, element->tagName,
            DOM_GetElementPseudo(cx, element), property);
#endif
    /*
     * We have to handle TAGs, .classes and #IDs.  Basically, we run through
     * this loop 3 times, finding the best match for IDs, then classes, then
     * tags.
     */
    iter = element;
    do {
        const char *token, *extra, *pseudo;
        pseudo = DOM_GetElementPseudo(cx, iter);
        for (type = SELECTOR_ID; type <= SELECTOR_TAG; type++) {
            switch(type) {
              case SELECTOR_ID:
                token = iter->styleID;
                extra = NULL;
                break;
              case SELECTOR_CLASS:
                token = iter->styleClass;
                extra = iter->tagName;
                break;
              case SELECTOR_TAG:
                token = iter->tagName;
                extra = NULL;
                break;
              default:
                continue;
            }
            if (!token)
                continue;
            sel = GetBaseSelector(cx, db, type, token, extra, pseudo,
                                  JS_FALSE);
            if (sel) {
#ifdef DEBUG_shaver_style_primitives
                fprintf(stderr, "[BASE selector for %d:%s/%s:%s -- checking]",
                        type, token, extra, pseudo);
#endif
                ok = CheckSelector(cx, iter, sel, property, entryp, &best, 0);
                if (ok) {
                    /*
                     * the first one we find that gives this property is
                     * correct, because we check the innermost nodes first.
                     */
                    if (*entryp) {
#ifdef DEBUG_shaver_style_primitives
                        fprintf(stderr, "found %s ", (*entryp)->value);
#endif
                        goto found_it; /* oh, for a labelled break */
                    }
                } else {
#ifdef DEBUG_shaver
                    fprintf(stderr, "CheckSelector failed\n");
#endif
                    return JS_FALSE;
                }
            }
        }
        iter = (DOM_Element *)iter->node.parent;
    } while (iter && iter->node.type == NODE_TYPE_ELEMENT);

 found_it:

#ifdef DEBUG_shaver_verbose
    if (!sel)
        fprintf(stderr, "no base selector found");
    else
#endif
#ifdef DEBUG_shaver_style_primitives
        if (!*entryp)
            fprintf(stderr, "no match found");
    fputs("\n", stderr);
#endif
    
    return JS_TRUE;
}

DOM_AttributeEntry *
DOM_StyleAddRule(JSContext *cx, DOM_StyleDatabase *db, DOM_StyleSelector *sel,
                 DOM_StyleToken name, const char *value)
{
    DOM_StyleRule *rule;
    DOM_AttributeEntry *entry;
    
    if (sel->rules) {
        entry = RuleValueFor(cx, sel->rules, name);
        if (entry) {
            if (!XP_STRCMP(entry->value, value))
                return entry;
#ifdef DEBUG_shaver_style_primitives
            fprintf(stderr, "overwriting %s=%s with %s=%s\n", name,
                    entry->value, name, value);
#endif
            XP_FREE((char *)entry->value);
            entry->value = XP_STRDUP(value);
            if (!entry->value)
                return NULL;
            entry->dirty = JS_TRUE;
            return entry;
        }
    }

    /*
     * XXXshaver we should allow a weight to be specified as well, so that
     * user-agent/author/user sheets can compete correctly.  Right now, we
     * always use a weight of 0.
     */ 
    rule = AddRule(cx, sel, name, value, 0);
    if (!rule)
        return NULL;
    return &rule->entry;
}

static DOM_StyleSelector *
FindSelector(JSContext *cx, DOM_StyleDatabase *db, DOM_StyleSelector *base,
             uint8 type, DOM_StyleToken enclosing, DOM_StyleToken extra,
             DOM_StyleToken pseudo)
{
    DOM_StyleSelector *sel;

    /* looking for the base one */
    if (!base) {
        sel = GetBaseSelector(cx, db, type, enclosing, extra, pseudo, JS_TRUE);
        if (!sel) {
            sel = NewSelector(cx, type, enclosing, extra, pseudo);
            if (!sel)
                return NULL;
            InsertBaseSelector(cx, db, sel);
        }
        return sel;
    }

    /* check existing enclosing selectors */
    for(sel = base->enclosing; sel; sel = sel->sibling) {
        if (SelectorMatchesToken(cx, sel, type, enclosing, extra, pseudo,
                                 JS_TRUE))
            return sel;
    }

    /* nothing found that matches, so create a new one */
    sel = NewSelector(cx, type, enclosing, extra, pseudo);
    if (!sel)
        return NULL;
    sel->sibling = base->enclosing;
    base->enclosing = sel;

    return sel;
}

#define HANDLE_SELECTOR_TYPE(token, type)                                     \
PR_BEGIN_MACRO                                                                \
    if (*token == '.') {                                                      \
        type = SELECTOR_CLASS;                                                \
        token++;                                                              \
    } else if (*token == '#') {                                               \
        type = SELECTOR_ID;                                                   \
        token++;                                                              \
    } else {                                                                  \
        type = SELECTOR_TAG;                                                  \
    }                                                                         \
PR_END_MACRO

DOM_StyleSelector *
DOM_StyleFindSelector(JSContext *cx, DOM_StyleDatabase *db,
                      DOM_StyleSelector *base, DOM_StyleToken enclosing,
                      DOM_StyleToken pseudo)
{
    uint8 type;

    HANDLE_SELECTOR_TYPE(enclosing, type);
    return FindSelector(cx, db, base, type, enclosing, NULL, pseudo);
}

DOM_StyleSelector *
DOM_StyleFindSelectorFull(JSContext *cx, DOM_StyleDatabase *db,
                          DOM_StyleSelector *base, uint8 type,
                          DOM_StyleToken enclosing, DOM_StyleToken extra,
                          DOM_StyleToken pseudo)
{
    if (type == SELECTOR_ID)
        extra = NULL;

    return FindSelector(cx, db, base, type, enclosing, extra, pseudo);
}

/*
 * JS classes
 */

/*
 * StyleSelector class: document.tags.H1 or contextual(tags.H1, tags.EM) or
 * document.ids.mine or document.classes.punk.all or whatever.
 */

static JSBool
StyleSelector_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    DOM_StyleSelector *sel;
    DOM_StyleDatabase *db;
    JSString *rule, *value;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;
    sel = JS_GetPrivate(cx, obj);

    if (!sel)
	return JS_TRUE;

    rule = JSVAL_TO_STRING(id);
    value = JS_ValueToString(cx, *vp);
    if (!value)
	return JS_FALSE;

    STYLE_DB_FROM_CX(db, cx);
    
#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "%s.", sel->selector);
    switch (sel->type) {
      case SELECTOR_CLASS:
        fprintf(stderr, "%s.", sel->extra ? sel->extra : "all");
        break;
      default:
        break;
    }
    fprintf(stderr, "%s = %s\n", JS_GetStringBytes(rule),
            JS_GetStringBytes(value));
#endif
    return DOM_StyleAddRule(cx, db, sel, JS_GetStringBytes(rule),
			    JS_GetStringBytes(value)) != NULL;
}

static JSBool
StyleSelector_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    return JS_TRUE;
}

static JSBool
StyleSelector_convert(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    JSString *str;
    DOM_StyleSelector *sel;
    if (hint == JSTYPE_STRING) {
        /* XXXshaver handle A.class:visited and A#id, etc. */
        char * selstring;
        uintN len;
        sel = JS_GetPrivate(cx, obj);
        if (!sel || !sel->selector)
            return JS_TRUE;
        if (sel->pseudo) {
            /* "selector:pseudo" */
            selstring = JS_malloc(cx, (len = XP_STRLEN(sel->selector)) +
                                      XP_STRLEN(sel->pseudo) +
                                      2 /* : and \0 */);
            if (!selstring)
                return JS_FALSE;
            XP_STRCPY(selstring, sel->selector);
            selstring[len] = ':';
            selstring[len + 1] = '\0';
            XP_STRCAT(selstring, sel->pseudo);
        } else {
            selstring = (char *)sel->selector;
        }
        str = JS_NewStringCopyZ(cx, selstring);
        if (sel->pseudo)
            JS_free(cx, selstring);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
    }
    return JS_TRUE;
}

static JSBool
StyleSelector_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;

    /* XXX we should check that it's a valid JSSS property name first */
    return JS_DefineProperty(cx, obj, JS_GetStringBytes(JSVAL_TO_STRING(id)),
                             JSVAL_VOID, 0, 0,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT);
}

static JSClass StyleSelectorClass = {
    "StyleSelector", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,           JS_PropertyStub,  StyleSelector_getProperty,
    StyleSelector_setProperty, JS_EnumerateStub, StyleSelector_resolve,
    StyleSelector_convert,     JS_FinalizeStub
};

static JSBool
StyleSelector(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    return JS_TRUE;
}

JSObject *
dom_StyleSelectorInit(JSContext *cx, JSObject *scope)
{
    JSObject *proto;
    proto = JS_InitClass(cx, scope, NULL, &StyleSelectorClass,
			 StyleSelector, 0,
			 0, 0, 0, 0);
    return proto;
}

JSObject *
DOM_NewStyleSelectorObject(JSContext *cx, DOM_StyleSelector *sel)
{
    JSObject *obj;

    obj = JS_ConstructObject(cx, &StyleSelectorClass, NULL, NULL);
    if (!obj)
        return NULL;
    
    if (!JS_SetPrivate(cx, obj, sel))
        return NULL;

    sel->mocha_object = obj;
    
    return obj;
}

JSObject *
DOM_ObjectForStyleSelector(JSContext *cx, DOM_StyleSelector *sel)
{
    if (!sel)
        return NULL;
    if (sel->mocha_object)
        return sel->mocha_object;
    
    return DOM_NewStyleSelectorObject(cx, sel);
}

/*
 * Tags object: document.tags
 */

static JSBool
Tags_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSString *tag;
    DOM_StyleDatabase *db;
    char *tagString, *pseudo, *selName;
    DOM_StyleSelector *sel;
    JSObject *tagObj;
    
    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    
    STYLE_DB_FROM_CX(db, cx);
    
    tag = JSVAL_TO_STRING(id);
    /* XXXshaver use GetStringChars and DefinePropertyUC? */
    tagString = JS_GetStringBytes(tag);
    if (!tagString)
        return JS_FALSE;
#ifdef DEBUG_shaver
    fprintf(stderr, "tags.%s\n", tagString);
#endif
    pseudo = strchr(tagString, ':');
    if (pseudo) {
        ptrdiff_t pseudoOff = pseudo - tagString;
        selName = XP_STRDUP(tagString);
        if (!selName)
            return JS_FALSE;
        selName[pseudoOff] = 0;
        pseudo = selName + pseudoOff + 1;
#ifdef DEBUG_shaver
        fprintf(stderr, "found pseudo: \"%s\":\"%s\"\n", selName, pseudo);
#endif
    } else {
        selName = tagString;
    }

    sel = DOM_StyleFindSelectorFull(cx, db, NULL, SELECTOR_TAG,
                                    selName, NULL, pseudo);
    if (pseudo)
        XP_FREE(selName);

    tagObj = DOM_ObjectForStyleSelector(cx, sel);
    return JS_DefineProperty(cx, obj, tagString, OBJECT_TO_JSVAL(tagObj),
                             0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    
}

static JSClass TagsClass = {
    "Tags", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, Tags_resolve,    JS_ConvertStub,  JS_FinalizeStub
};

static JSObject *
TagsObjectInit(JSContext *cx, JSObject *scope)
{
    JSObject *obj = JS_NewObject(cx, &TagsClass, NULL, scope);
    return obj;
}

/*
 * Classes object: document.classes and document.classes.punk
 */

static void
ClassHolder_finalize(JSContext *cx, JSObject *obj)
{
    char *cls = JS_GetPrivate(cx, obj);
    if (cls)
        XP_FREE(cls);
}

static JSBool
ClassHolder_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSString *tag;
    DOM_StyleDatabase *db;
    char *clsString, *tagString, *extra;
    DOM_StyleSelector *sel;
    JSObject *clsObj;
    
    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    
    clsString = JS_GetPrivate(cx, obj);
    if (!clsString)
        return JS_TRUE;

    STYLE_DB_FROM_CX(db, cx);
    
    tag = JSVAL_TO_STRING(id);
    /* XXXshaver use GetStringChars and DefinePropertyUC? */
    tagString = JS_GetStringBytes(tag);

    if (!tagString)
        return JS_FALSE;

#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "classes.%s.%s\n", clsString, tagString);
#endif
    /* XXX handle pseudo */
    if (XP_STRCMP(tagString, "all"))
        extra = tagString;
    else
        extra = NULL;

    sel = DOM_StyleFindSelectorFull(cx, db, NULL, SELECTOR_CLASS,
                                    clsString, extra, NULL);

    clsObj = DOM_ObjectForStyleSelector(cx, sel);
    return JS_DefineProperty(cx, obj, tagString, OBJECT_TO_JSVAL(clsObj),
                             0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
}

static JSClass ClassHolderClass = {
    "ClassHolder", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, ClassHolder_resolve, JS_ConvertStub,  ClassHolder_finalize
};

static JSObject *
ClassHolder(JSContext *cx, const char *cls)
{
    char *clsDup;
    JSObject *obj = JS_NewObject(cx, &ClassHolderClass, NULL,
                                 JS_GetGlobalObject(cx));
    if (!obj)
        return NULL;
    clsDup = XP_STRDUP(cls);
    if (!clsDup)
        return NULL;

    if (!JS_SetPrivate(cx, obj, clsDup))
        return NULL;

    return obj;
}

static JSBool
Classes_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSString *cls;
    char *clsString;
    JSObject *clsObj;
    
    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    
    cls = JSVAL_TO_STRING(id);
    /* XXXshaver use GetStringChars and DefinePropertyUC? */
    clsString = JS_GetStringBytes(cls);

    if (!clsString)
        return JS_FALSE;
#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "classes.%s\n", clsString);
#endif

    clsObj = ClassHolder(cx, clsString);

    if (!clsObj)
        return JS_FALSE;

    return JS_DefineProperty(cx, obj, clsString, OBJECT_TO_JSVAL(clsObj),
                             0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
}

static JSClass ClassesClass = {
    "Classes", 0,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, Classes_resolve, JS_ConvertStub,  JS_FinalizeStub
};

static JSObject *
ClassesObjectInit(JSContext *cx, JSObject *scope)
{
    JSObject *obj = JS_NewObject(cx, &ClassesClass, NULL, scope);
    return obj;
}

/*
 * Ids object: document.ids
 */

static JSBool
Ids_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSString *str;
    DOM_StyleDatabase *db;
    char *idString;
    DOM_StyleSelector *sel;
    JSObject *idObj;
    
    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    
    STYLE_DB_FROM_CX(db, cx);
    
    str = JSVAL_TO_STRING(id);
    /* XXXshaver use GetStringChars and DefinePropertyUC? */
    idString = JS_GetStringBytes(str);
    if (!idString)
        return JS_FALSE;
#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "ids.%s\n", idString);
#endif
    /* XXX handle pseudo and tag */
    sel = DOM_StyleFindSelectorFull(cx, db, NULL, SELECTOR_ID,
                                    idString, NULL, NULL);

    idObj = DOM_ObjectForStyleSelector(cx, sel);
    return JS_DefineProperty(cx, obj, idString, OBJECT_TO_JSVAL(idObj),
                             0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
}

static JSClass IdsClass = {
    "Ids", 0,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, Ids_resolve,     JS_ConvertStub,  JS_FinalizeStub
};

static JSObject *
IdsObjectInit(JSContext *cx, JSObject *scope)
{
    JSObject *obj = JS_NewObject(cx, &IdsClass, NULL, scope);
    return obj;
}

JSBool
DOM_DocObjectResolveStyleProps(JSContext *cx, JSObject *obj, jsval id)
{
    char *name;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

#ifdef DEBUG_shaver_style_primitives
    fprintf(stderr, "resolving document.%s\n", name);
#endif
    if (!XP_STRCMP(name, "tags")) {
        JSObject *tags;

        tags = TagsObjectInit(cx, obj);
        if (!tags)
            return JS_FALSE;
        return JS_DefineProperty(cx, obj, "tags", OBJECT_TO_JSVAL(tags),
                                 0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    } else if (!XP_STRCMP(name, "classes")) {
        JSObject *classes;

        classes = ClassesObjectInit(cx, obj);
        if (!classes)
            return JS_FALSE;
        return JS_DefineProperty(cx, obj, "classes", OBJECT_TO_JSVAL(classes),
                                 0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    } else if (!XP_STRCMP(name, "ids")) {
        JSObject *ids;

        ids = IdsObjectInit(cx, obj);
        if (!ids)
            return JS_FALSE;
        return JS_DefineProperty(cx, obj, "ids", OBJECT_TO_JSVAL(ids),
                                 0, 0, JSPROP_ENUMERATE | JSPROP_PERMANENT);

    } else if (!XP_STRCMP(name, "contextual")) {
        return (JSBool)(JS_DefineFunction(cx, obj, "contextual", 
                                          DOM_JSContextual, 1, 0) != NULL);
    }

    return JS_TRUE;
}

/*
 * Return a contextual selector: contextual("H1", tags.EM);
 */
JSBool
DOM_JSContextual(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    DOM_StyleDatabase *db;
    intN i;
    DOM_StyleSelector *sel = NULL;
    JSObject *selObj;
    JSString *str;

    if (!argc)
        return JS_TRUE;

    STYLE_DB_FROM_CX(db, cx);
    
    for (i = argc - 1; i >= 0; i--) {
        JSObject *obj2;
        const char *selName, *selPseudo, *selExtra;
        int8 selType;
        if (JSVAL_IS_OBJECT(argv[i]) &&
            (obj2 = JSVAL_TO_OBJECT(argv[i]), 
#ifdef JS_THREADSAFE
             JS_GetClass(cx, obj2)
#else
             JS_GetClass(obj2)
#endif
             == &StyleSelectorClass)) {
            /* tags.H1 or ids.mine or classes.punk.all */
            DOM_StyleSelector *sel2 = JS_GetPrivate(cx, obj2);
            if (!sel2)
                /* have prototype, will travel (and ignore it) */
                continue;
            selName = sel2->selector;
            selType = sel2->type;
            selExtra = sel2->extra;
            selPseudo = sel2->pseudo;
        } else {
            /* "H1" or ".myclass" or "#ID" or whatever */
            str = JS_ValueToString(cx, argv[i]);
            if (!str)
                return JS_FALSE;
            selName = JS_GetStringBytes(str);
            if (!selName)
                return JS_FALSE;
            /* XXX parse and handle class/id and pseudo/extra! */
            selPseudo = NULL;
            selExtra = NULL;
            selType = SELECTOR_TAG;
        }
#ifdef DEBUG_shaver_style_primitives
        fprintf(stderr, "contextual: going from %d:%s/%s to %d:%s/%s\n",
                sel ? sel->type : -1, sel ? sel->selector : "<none>",
                sel ? sel->extra : "<none>", selType, selName, selExtra);
#endif
        sel = DOM_StyleFindSelectorFull(cx, db, sel, selType, selName,
                                        selExtra, selPseudo);
        if (!sel) {
            JS_ReportError(cx, "Error getting selector %s\n", selName);
            return JS_FALSE;
        }
    }

    selObj = DOM_ObjectForStyleSelector(cx, sel);
    *rval = OBJECT_TO_JSVAL(selObj);

    return JS_TRUE;
}
