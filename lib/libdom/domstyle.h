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
 * Perignon: store style information in the DOM, using CSS-1 selectors.
 */

#include "jsapi.h"
#include "dom.h"
#include "plhash.h"

#ifndef DOM_STYLE_H
#define DOM_STYLE_H

typedef struct DOM_StyleDatabase DOM_StyleDatabase;
typedef struct DOM_StyleSelector DOM_StyleSelector;
typedef struct DOM_StyleRule DOM_StyleRule;

/*
 * DOM_StyleTokens are used for identification of elements ("H1"), style
 * properties ("color"), style values ("blue") and pseudo-classes ("visited").
 */

/* this may become int or something later, for speed */
typedef const char *DOM_StyleToken;

enum {
    SELECTOR_UNKNOWN = 0,
    SELECTOR_ID,
    SELECTOR_CLASS,
    SELECTOR_TAG
};

struct DOM_StyleDatabase {
    PLHashTable *ht;
};

DOM_StyleDatabase *
DOM_NewStyleDatabase(JSContext *cx);

void
DOM_DestroyStyleDatabase(JSContext *cx, DOM_StyleDatabase *db);

/*
 * Find or create the StyleDatabase for the given JSContext.
 * The embedder must provide an implementation, or #define MOZILLA_CLIENT
 * to get the Mozilla-specific one which depends on MochaDecoder and 
 * MWContext and lo_TopState and stuff.
 */
DOM_StyleDatabase *
DOM_StyleDatabaseFromContext(JSContext *cx);

struct DOM_StyleSelector {
    int8 type;
    DOM_StyleToken selector;
    DOM_StyleToken pseudo;
    DOM_StyleToken extra;
    DOM_StyleSelector *enclosing;
    DOM_StyleSelector *sibling;
    DOM_StyleRule *rules;
    JSObject *mocha_object;     /* reflection for this selector's rules */
};

/*
 * Find or create a selector for the given enclosing element type, starting
 * from the provided (optional) base selector.
 *
 * Usage examples:
 *
 * Find/create a selector for "B"
 * sel = DOM_StyleFindSelector(cx, db, NULL, "B", NULL);
 *
 * Now find/create a selector for "CODE B":
 * sel2 = DOM_StyleFindSelector(cx, db, sel, "CODE", NULL);
 *
 * And ".myclass CODE B":
 * sel3 = DOM_StyleFindSelector(cx, db, sel2, ".myclass", NULL);
 */
DOM_StyleSelector *
DOM_StyleFindSelector(JSContext *cx, DOM_StyleDatabase *db,
                      DOM_StyleSelector *base, DOM_StyleToken enclosing,
                      DOM_StyleToken pseudo);

/*
 * As above, but take type explicitly rather than parsing leading # or . for
 * ID or class.  For classes or extra is a tag or NULL.  For tags, extra is an
 * ID or NULL.  (For ID, extra is ignored.)
 */
DOM_StyleSelector *
DOM_StyleFindSelectorFull(JSContext *cx, DOM_StyleDatabase *db,
                          DOM_StyleSelector *base, uint8 type,
                          DOM_StyleToken enclosing, DOM_StyleToken extra,
                          DOM_StyleToken pseudo);

struct DOM_StyleRule {
    DOM_AttributeEntry entry;
    int16 weight;
    DOM_StyleRule *next;
};

/*
 * Parses a style rule and adds it to the style database.
 * If len is 0, rule is presumed to be NUL-terminated. (XXX NYI)
 *
 * Usage example:
 * 
 * DOM_StyleParseRule(JSContext *cx, DOM_StyleDatabase *db,
 *                    "A:visited { color: blue }", 0, DOM_STYLE_WEIGHT_AUTHOR);
 */

#define DOM_STYLE_WEIGHT_NONE      0
#define DOM_STYLE_WEIGHT_UA        2
#define DOM_STYLE_WEIGHT_USER      4
#define DOM_STYLE_WEIGHT_AUTHOR    6

JSBool
DOM_StyleParseRule(JSContext *cx, DOM_StyleDatabase *db, const char *rule,
                   uintN len, intN baseWeight);

/*
 * Get a style property for a node.
 * The node must be an Element or Text node.  If it's a text node, the
 * enclosing Element is used for finding matches.  The implementation is
 * necessarily somewhat hairy.  See domstyle.c for details.
 *
 * If db is NULL, DOM_StyleDatabaseFromContext is used to find it.
 *
 * Usage examples:
 * 
 * Get the color for a section of text:
 * DOM_StyleGetProperty(cx, db, (DOM_Node *)text, "color", &entry);
 *
 * Find the display property of a list item:
 * DOM_StyleGetProperty(cx, db, (DOM_Node *)listItem, "display", &entry);
 */

JSBool
DOM_StyleGetProperty(JSContext *cx, DOM_StyleDatabase *db, DOM_Node *node,
                     DOM_StyleToken property, DOM_AttributeEntry **entryp);

/*
 * Get/set the pseudoclass for an element
 */
DOM_StyleToken
DOM_GetElementPseudo(JSContext *cx, DOM_Element *element);

JSBool
DOM_SetElementPseudo(JSContext *cx, DOM_Element *element,
                     DOM_StyleToken pseudo);

/*
 * Add a property to the provided selector.
 *
 * DOM_StyleAddRule(cx, db, sel, "color", "blue");
 */
DOM_AttributeEntry *
DOM_StyleAddRule(JSContext *cx, DOM_StyleDatabase *db, DOM_StyleSelector *sel,
                 DOM_StyleToken name, const char *value);

/*
 * Resolve classes, tags, ids, contextual on the given object.
 */
JSBool
DOM_DocObjectResolveStyleProps(JSContext *cx, JSObject *obj, jsval id);

/*
 * The contextual selector JS function: contextual("H1", "EM");
 */
JSBool
DOM_JSContextual(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval);

#endif /* DOM_STYLE_H */
