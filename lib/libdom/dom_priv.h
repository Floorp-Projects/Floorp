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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Module-private stuff for the DOM lib.
 */

#ifndef DOM_PRIV_H
#define DOM_PRIV_H

#include "dom.h"
#ifdef XP_PC
/* XXX this pulls in half the freaking client! but it's needed for some bogus
       reason probably to do with compiled headers, to avoid unresolved refs
       to XP_MEMCPY and XP_STRDUP */
#include "xp.h"
#else
#include "xpassert.h"
#include "xp_mem.h"
#include "xp_str.h"
#endif

JSObject *
dom_NodeInit(JSContext *cx, JSObject *obj);

JSObject *
dom_ElementInit(JSContext *cx, JSObject *obj, JSObject *node_proto);

JSBool
dom_SetElementAttribute(JSContext *cx, DOM_Element *element, const char *name,
                        const char *value, JSBool runCallback);

JSObject *
dom_AttributeInit(JSContext *cx, JSObject *scope, JSObject *node_proto);

JSObject *
dom_CharacterDataInit(JSContext *cx, JSObject *scope, JSObject *node_proto);

JSObject *
dom_TextInit(JSContext *cx, JSObject *scope, JSObject *data_proto);

JSObject *
dom_CommentInit(JSContext *cx, JSObject *scope, JSObject *data_proto);

JSBool
dom_node_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JSBool
dom_node_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

void
dom_node_finalize(JSContext *cx, JSObject *obj);

/* if you adjust these enums, be sure to adjust the various setters */
enum {
    DOM_NODE_NODENAME = -1,
    DOM_NODE_NODEVALUE = -2,
    DOM_NODE_NODETYPE = -3,
    DOM_NODE_PARENTNODE = -4,
    DOM_NODE_CHILDNODES = -5,
    DOM_NODE_FIRSTCHILD = -6,
    DOM_NODE_LASTCHILD = -7,
    DOM_NODE_PREVIOUSSIBLING = -8,
    DOM_NODE_NEXTSIBLING = -9,
    DOM_NODE_ATTRIBUTES = -10,
    DOM_NODE_HASCHILDNODES = -11,
    DOM_ELEMENT_TAGNAME = -12,
};

extern JSPropertySpec dom_node_props[];

#endif /* DOM_PRIV_H */
