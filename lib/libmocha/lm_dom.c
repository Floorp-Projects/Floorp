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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Glue to connect lib/libdom to lib/layout.
 */

#include "lm.h"
#include "lm_dom.h"
#include "layout.h"
#include "pa_tags.h"
#include "pa_parse.h"
#include "intl_csi.h"

#ifdef DEBUG_shaver
/* #define DEBUG_shaver_verbose */
#endif

#define LAYLOCKED(code)                                                       \
PR_BEGIN_MACRO                                                                \
    LO_LockLayout();                                                          \
    code;                                                                     \
    LO_UnlockLayout();                                                        \
PR_END_MACRO

/* from et_moz.c */
int
ET_DOMReflow(MWContext *context, LO_Element *element, PRBool reflow,
             int32 doc_id);

static JSBool
lm_DOMInsertBefore(JSContext *cx, DOM_Node *node, DOM_Node *child,
                   DOM_Node *ref, JSBool before)
{
    return JS_TRUE;
}

static JSBool
lm_DOMReplaceChild(JSContext *cx, DOM_Node *node, DOM_Node *child,
                   DOM_Node *old, JSBool before)
{
    return JS_TRUE;
}

static JSBool
lm_DOMRemoveChild(JSContext *cx, DOM_Node *node, DOM_Node *child,
                  JSBool before)
{
    return JS_TRUE;
}

static JSBool
lm_DOMAppendChild(JSContext *cx, DOM_Node *node, DOM_Node *child,
                  JSBool before)
{
    return JS_TRUE;
}

static JSObject *
lm_DOMReflectNode(JSContext *cx, DOM_Node *node)
{
    MochaDecoder *decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));

    if (!decoder || !node)
        return NULL;

    switch(node->type) {
      case NODE_TYPE_ELEMENT:
        return DOM_NewElementObject(cx, (DOM_Element *)node);
      case NODE_TYPE_TEXT:
        return DOM_NewTextObject(cx, (DOM_Text *)node);
      case NODE_TYPE_ATTRIBUTE:
        return DOM_NewAttributeObject(cx, (DOM_Attribute *)node);
      default:
        XP_ASSERT((0 && "unknown DOM type"));
    }
    return NULL;
}

DOM_NodeOps lm_NodeOps = {
    lm_DOMInsertBefore, lm_DOMReplaceChild, lm_DOMRemoveChild,
    lm_DOMAppendChild, DOM_DestroyNodeStub, lm_DOMReflectNode
};

static JSBool
lm_DOMSetAttributes(JSContext *cx, DOM_Element *element, const char *name,
                    const char *value)
{
    JSBool matched = JS_FALSE;
    MochaDecoder *decoder;
    MWContext *context;
    DOM_HTMLElementPrivate *priv;
    void *ele1, *ele2;

    decoder = (MochaDecoder *)JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    context = decoder->window_context;
    priv = (DOM_HTMLElementPrivate *)element->node.data;

    switch(priv->tagtype) {
      case P_TABLE_DATA:
        if (!XP_STRCASECMP("valign", name) ||
            !XP_STRCASECMP("halign", name) ||
            !XP_STRCASECMP("bgcolor", name)) {
            ele1 = priv->ele_start;
            matched = JS_TRUE;
        }
        break;
      default:;
    }

    if (!matched) {
        DOM_SignalException(cx, DOM_INVALID_NAME_ERR);
        return JS_FALSE;
    }

    ET_DOMSetAttributes(context, priv->tagtype, ele1, ele2,
                        name, value, decoder->doc_id);

    return JS_TRUE;
}

static void
lm_BreakLayoutNodeLinkRecurse(DOM_Node *node)
{
    DOM_HTMLElementPrivate *priv;
    if (node->type == NODE_TYPE_TEXT ||
        node->type == NODE_TYPE_ELEMENT) {
        priv = (DOM_HTMLElementPrivate *)node->data;
        if (priv)
            priv->ele_start = priv->ele_end = NULL;
    }

    for (node = node->child; node; node = node->sibling)
        lm_BreakLayoutNodeLinkRecurse(node);
}

void
lm_DestroyDocumentNodes(MWContext *context)
{
    JSContext *cx;
    lo_TopState *top;
    cx = context->mocha_context;
    top = lo_FetchTopState(context->doc_id);

    /* XXX LO_LockLayout(); */
    lm_BreakLayoutNodeLinkRecurse(top->top_node);
    /* XXX LO_UnlockLayout(); */    
    DOM_DestroyTree(cx, top->top_node);

}

DOM_ElementOps lm_ElementOps = {
    lm_DOMSetAttributes, DOM_GetAttributeStub, DOM_GetNumAttrsStub
};

/*
 * Handle text alterations.  Most of the time, we're just carving up
 * new text blocks, but sometimes text inside stuff is magic, like
 * <TITLE> and maybe <HREF>.
 */
static JSBool
lm_CDataOp(JSContext *cx, DOM_CharacterData *cdata, DOM_CDataOperationCode op)
{
    JSBool ok;
    DOM_Node *node;
    MWContext *context;
    MochaDecoder *decoder;
    char *data;
    LO_TextBlock *text;

    node = cdata->node.parent;
    data = cdata->data;

    decoder = (MochaDecoder *)JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    context = decoder->window_context;
    
    if (node->type == NODE_TYPE_ELEMENT) {
        switch(ELEMENT_PRIV(node)->tagtype) {
          case P_TITLE:
            /*
             * XXX clean up the text, find the current charset, make
             * sure we're not a subdoc, etc.  All this logic lives in
             * lo_process_title_tag, and should be refactored.
             */
            FE_SetDocTitle(context, data);
            return JS_TRUE;
        }
    }
    text = (LO_TextBlock *)ELEMENT_PRIV(cdata)->ele_start;
    if (!text) {
#ifdef DEBUG
        fprintf(stderr, "no data for text node on %s %s\n",
                ((DOM_Element *)node)->tagName, node->name);
#endif
        return JS_TRUE;
    }

    /*
     * Tell layout to use the new text instead.
     */
    LAYLOCKED(ok = lo_ChangeText(text, data));
    if (!ok)
        return JS_FALSE;

    return (JSBool)ET_DOMReflow(context, (LO_Element *)text, PR_TRUE,
                                decoder->doc_id);
                                 
}

static DOM_Node *
lm_NodeForTag(PA_Tag *tag, DOM_Node *current, MWContext *context, int16 csid)
{
    DOM_Node *node;
    DOM_Element *element;
    DOM_HTMLElementPrivate *elepriv;

    if (current->type == NODE_TYPE_TEXT)
        /*
         * it's going to get skipped back over by the HTMLPushNode stuff,
         * so build a tag as though we were pushing on its parent.
         */
        current = current->parent;

    switch (tag->type) {
      case P_TEXT:
        if (current->type != NODE_TYPE_ELEMENT)
            return NULL;
        /* create Text node */
        node = (DOM_Node *)DOM_NewText((char *)tag->data, tag->data_len,
                                       lm_CDataOp, &lm_NodeOps);
        if (!node)
            return NULL;
        break;
#if 0                           /* Urgh...we don't reflect comments! */
      case P_COMMENT:
        /* create Comment node */
        break;
#endif
      case P_UNKNOWN:
        return NULL;
      default:
        /* just a regular old element */
        element = XP_NEW_ZAP(DOM_Element);
        if (!element) {
            return NULL;
        }
        element->ops = &lm_ElementOps;
        element->tagName = PA_TagString(tag->type);

        node = (DOM_Node *)element;
        node->type = NODE_TYPE_ELEMENT;
        /* what about ID? */
        node->name = (char *)PA_FetchParamValue(tag, "name", csid);
        node->ops = &lm_NodeOps;
    }
    elepriv = XP_NEW_ZAP(DOM_HTMLElementPrivate);
    if (!elepriv) {
        XP_FREE(element);
        return NULL;
    }
    elepriv->tagtype = tag->type;
    node->data = elepriv;
    return node;
}

#ifdef DEBUG_shaver
static int LM_Node_indent;
#endif

JSBool
DOM_HTMLPushNode(DOM_Node *node, DOM_Node *parent)
{
    DOM_Element *element = (DOM_Element *)node;
    DOM_Element *parent_el = (DOM_Element *)parent;

    /* XXX factor this information out, and share with parser */
    if (parent->type == NODE_TYPE_ELEMENT) {
        if ( parent_el->ops == &lm_ElementOps &&
             lo_IsEmptyTag(ELEMENT_PRIV(parent_el)->tagtype)) {
            /* these don't have contents */
            parent = parent->parent;
#ifdef DEBUG_shaver
            LM_Node_indent -= 2;
#endif
        } else {                /* not an empty tag */
            switch(ELEMENT_PRIV(parent_el)->tagtype) {
            case P_PARAGRAPH:
            case P_LIST_ITEM:
            case P_HEADER_1:
            case P_HEADER_2:
            case P_HEADER_3:
            case P_HEADER_4:
            case P_HEADER_5:
            case P_HEADER_6:
            case P_ANCHOR:
            case P_OPTION:
                /* these don't nest with themselves */
                if (node->type == NODE_TYPE_ELEMENT &&
                    ELEMENT_PRIV(parent_el)->tagtype == 
                    ELEMENT_PRIV(element)->tagtype) {
                    parent = parent->parent;
#ifdef DEBUG_shaver
                    LM_Node_indent -= 2;
#endif
                }
                break;
            default:;
            }
        }
    } else if (parent->type != NODE_TYPE_DOCUMENT) {
        /* if it's not an element, it doesn't get kids XXX oversimplified */
#ifdef DEBUG_shaver_verbose
        fprintf(stderr, "can't put kids on type %d\n", parent->type);
#endif
        parent = parent->parent;
    }
#if 0
    if (node->type == NODE_TYPE_TEXT &&
        parent->type != NODE_TYPE_ELEMENT)
        /* only elements can have text XXX oversimplified*/
        return JS_FALSE;
#endif 
#ifdef DEBUG_shaver_verbose
    {
        TagType dbgtype = 0, partype = 0;       /* text */
        if (node->type == NODE_TYPE_ELEMENT)
            dbgtype = ELEMENT_PRIV(element)->tagtype;
        if (parent->type == NODE_TYPE_ELEMENT)
            partype = ELEMENT_PRIV(parent)->tagtype;
        fprintf(stderr, "%*s<%s %s> on <%s %s>\n", LM_Node_indent, "",
                PA_TagString(dbgtype), node->name ? node->name : "",
                PA_TagString(partype), parent->name ? parent->name : "");
        if (dbgtype)
            LM_Node_indent += 2;
    }
#endif
    
    if (!DOM_PushNode(node, parent))
        return JS_FALSE;
    
    return JS_TRUE;
}

void /* DOM_Node */ *
LM_ReflectTagNode(PA_Tag *tag, void *doc_state, MWContext *context)
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
    int16 csid = INTL_GetCSIWinCSID(c);
    lo_DocState *doc = (lo_DocState *)doc_state;
    DOM_Node *node;

    if (!TOP_NODE(doc)) {
#if 0
        node = DOM_NewDocument(context, doc);
#else
        node = XP_NEW_ZAP(DOM_Node);
#endif
        if (!node)
            return NULL;
        node->type = NODE_TYPE_DOCUMENT;
        node->name = XP_STRDUP("#document");
        CURRENT_NODE(doc) = TOP_NODE(doc) = node;
    }

    if (!tag) {
#ifdef DEBUG_shaver
        fprintf(stderr, "finished reflecting document\n");
        LM_Node_indent = 0;
#endif
        CURRENT_NODE(doc) = TOP_NODE(doc);
        return CURRENT_NODE(doc);
    }
    
    if (tag->type == -1)
        return CURRENT_NODE(doc);

    if (tag->is_end) {
        DOM_Node *last_node;
        if (CURRENT_NODE(doc) == TOP_NODE(doc)) {
            XP_ASSERT(CURRENT_NODE(doc)->type == NODE_TYPE_DOCUMENT);
#ifdef DEBUG_shaver_verbose
            fprintf(stderr, "not popping tag </%s> from doc-only stack\n",
                    PA_TagString(tag->type));
#endif
            return CURRENT_NODE(doc);
        }
        last_node = (DOM_Node *)DOM_HTMLPopElementByType(tag->type,
                                           (DOM_Element *)CURRENT_NODE(doc));
        CURRENT_NODE(doc) = last_node->parent;
        return last_node;
    }

    node = lm_NodeForTag(tag, CURRENT_NODE(doc), context, csid);
    if (node) {
        if (!DOM_HTMLPushNode(node, CURRENT_NODE(doc))) {
#ifdef DEBUG_shaver
            fprintf(stderr, "bad push of node %d for tag %d\n",
                    node->type, tag->type);
#endif
            if (node->ops->destroyNode)
                node->ops->destroyNode(context->mocha_context,
                                       node);
            return NULL;
        }
        /* 
         * we always have to have CURRENT_NODE(doc) pointing at the
         * node for which we are parsing a tag.  Even in the case of
         * Text and other child-less nodes (Comments?), we need layout
         * to be able to find the node for which it's generating
         * LO_Elements.  This means that the `Text has no children' logic
         * has to be in the DOM_HTMLPushNode code, and has to be driven
         * by pre-check of parent.  Sorry.
         */
        CURRENT_NODE(doc) = node;
    } else {
#ifdef DEBUG_shaver
        fprintf(stderr, "lm_NodeForTag returned NULL for tag %d\n",
                tag->type);
#endif
    }
    PR_ASSERT(!CURRENT_NODE(doc)->parent || 
              CURRENT_NODE(doc)->parent->type != NODE_TYPE_TEXT);

    return node;
}

JSBool
lm_DOMInit(MochaDecoder *decoder)
{
    JSObject *win = decoder->window_object;
    JSContext *cx = decoder->js_context;

    return DOM_Init(cx, win);
}

JSObject *
lm_DOMGetDocumentElement(MochaDecoder *decoder, JSObject *docobj)
{
    JSObject *obj;
    JSDocument *doc;
    lo_TopState *top_state;

    doc = JS_GetPrivate(decoder->js_context, docobj);
    if (!doc)
        return NULL;
    obj = doc->dom_documentElement;
    if (obj)
        return obj;

    top_state = lo_GetMochaTopState(decoder->window_context);
    if (!top_state)
        return NULL;
    
    /* XXX really create a DocumentElement
    obj = NewDocument(decoder, docobj, doc);
    */

    obj = DOM_NewNodeObject(decoder->js_context,
                            top_state->top_node);

    doc->dom_documentElement = obj;
    return obj;
}

DOM_Element *
DOM_HTMLPopElementByType(TagType type, DOM_Element *element)
{
    DOM_Element *closing;
#ifdef DEBUG_shaver_verbose
    int new_indent = LM_Node_indent;
#endif

    if (element->node.type == NODE_TYPE_DOCUMENT) {
#ifdef DEBUG_shaver_verbose
            fprintf(stderr, "popping tag </%s> from empty stack\n",
                    PA_TagString(type));
#endif
        return element;
    }

    if (element->node.type == NODE_TYPE_TEXT)
        /* really, we're closing the enclosing parent */
        element = (DOM_Element *)element->node.parent;

    PR_ASSERT(element->node.type == NODE_TYPE_ELEMENT);
    if (element->node.type != NODE_TYPE_ELEMENT)
        return NULL;

    closing = element;
    while (closing && 
           closing->node.type == NODE_TYPE_ELEMENT && 
           ELEMENT_PRIV(closing)->tagtype != type) {
#ifdef DEBUG_shaver_verbose
        fprintf(stderr, "skipping <%s> in search of <%s>\n",
                closing->tagName, PA_TagString(type));
        new_indent -= 2;
#endif
        closing = (DOM_Element *)closing->node.parent;
    }

    if (!closing || closing->node.type == NODE_TYPE_DOCUMENT) {
#ifdef DEBUG_shaver_verbose
        fprintf(stderr, "ignoring </%s> with no matching <%s>\n",
                PA_TagString(type), PA_TagString(type));
#endif
    return element;
    }

#ifdef DEBUG_shaver_verbose
    LM_Node_indent = new_indent - 2;
    fprintf(stderr, "%*s</%s %s>\n", LM_Node_indent, "",
            closing->tagName,
            closing->node.name ? closing->node.name : "");
    if (!closing->node.parent)
        fprintf(stderr, "empty node stack\n");
#endif

    /* return the popped node */
    return closing;
}

