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
#define LOCAL_ASSERT PR_ASSERT
#else
#define LOCAL_ASSERT XP_ASSERT
#endif

#ifdef DEBUG_shaver
/* #define DEBUG_shaver_verbose */
#define DEBUG_shaver_treegen
#endif

#define LAYLOCKED(code)                                                       \
PR_BEGIN_MACRO                                                                \
    LO_LockLayout();                                                          \
    code;                                                                     \
    LO_UnlockLayout();                                                        \
PR_END_MACRO

JSBool
lm_CheckNodeDocId(MWContext *context, DOM_HTMLElementPrivate *priv)
{
    return priv->doc_id == context->doc_id;
}

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

/*
 * Signal an exception on doc_id mismatch.  Must be called with layout lock
 * held (unlocks on failure).
 */
#define CHECK_DOC_ID(context, priv)                                           \
PR_BEGIN_MACRO                                                                \
    if (!lm_CheckNodeDocId(context, priv)) {                                  \
        DOM_SignalException(cx, DOM_WRONG_DOCUMENT_ERR);                      \
        LO_UnlockLayout();                                                    \
        return JS_FALSE;                                                      \
    }                                                                         \
PR_END_MACRO

static JSBool
lm_DOMSetAttributes(JSContext *cx, DOM_Element *element, const char *name,
                    const char *value)
{
    JSBool matched = JS_FALSE;
    MochaDecoder *decoder;
    MWContext *context;
    DOM_HTMLElementPrivate *priv;
    void *ele = NULL;
    lo_DocState *doc; 

    decoder = (MochaDecoder *)JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    context = decoder->window_context;
    doc = (lo_FetchTopState(context->doc_id))->doc_state;
    priv = (DOM_HTMLElementPrivate *)element->node.data;

    switch(priv->tagtype) {
      case P_TABLE_DATA: {
        LO_Element *iter, *start, *end;
        lo_TableCell *cell;

        LO_LockLayout();
        CHECK_DOC_ID(context, priv);
        cell = (lo_TableCell *)priv->ele_start;
        if (!cell)
            goto out_unlock;
            
        if (!XP_STRCASECMP("valign", name)) {
            /* tweak valign */
        } else if(!XP_STRCASECMP("halign", name)) {
            /* tweak halign */
        } else if(!XP_STRCASECMP("bgcolor", name)) {
            LO_Color rgb;
            start = cell->cell->cell_list;
            end = cell->cell->cell_list_end;
            if (!start ||
                !LO_ParseRGB((char *)value, &rgb.red, &rgb.green, &rgb.blue))
                goto out_unlock;
            for (iter = start; iter && iter != end; iter = iter->lo_any.next)
                lo_SetColor(iter, &rgb, doc, TRUE);
            if (iter != start)
                lo_SetColor(start, &rgb, doc, TRUE);
        } else {
            /* No match */
            matched = JS_FALSE;
            LO_UnlockLayout();
            break;
        }
        LO_UnlockLayout();
        matched = JS_TRUE;
        break;
      }
      default:;
    }

    if (!matched) {
        DOM_SignalException(cx, DOM_INVALID_NAME_ERR);
        return JS_FALSE;
    }

    return (JSBool)ET_DOMReflow(context, (LO_Element *)ele, PR_TRUE,
                                decoder->doc_id);
  out_unlock:
    LO_UnlockLayout();
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
            LAYLOCKED(
                CHECK_DOC_ID(context, ELEMENT_PRIV(node));
                FE_SetDocTitle(context, data);
                );
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
lm_NodeForTag(JSContext *cx, PA_Tag *tag, DOM_Node *current,
              MWContext *context, int16 csid)
{
    DOM_Node *node;
    DOM_HTMLElementPrivate *elepriv;
    char **names, **values;
    uintN nattrs;

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
        node = (DOM_Node *)
            DOM_NewElement(PA_TagString(tag->type), &lm_ElementOps,
                           (char *)PA_FetchParamValue(tag, "name", csid),
                           (char *)PA_FetchParamValue(tag, "class", csid),
                           (char *)PA_FetchParamValue(tag, "id", csid),
                           &lm_NodeOps);
        if (!node)
            return NULL;
        nattrs = PA_FetchAllNameValues(tag, &names, &values, csid);
        if (!DOM_SetElementAttributes(cx, (DOM_Element *)node,
                                      (const char**)names,
                                      (const char **)values, nattrs)) {
            /* XXX free data */
            return NULL;
        }
        break;
    }

    elepriv = XP_NEW_ZAP(DOM_HTMLElementPrivate);
    if (!elepriv) {
        XP_FREE(node);
        return NULL;
    }
    elepriv->tagtype = tag->type;
    elepriv->doc_id = context->doc_id;
    node->data = elepriv;
    return node;
}

#ifdef DEBUG_shaver
static int LM_Node_indent;
#endif

DOM_Node *
DOM_HTMLPushNode(DOM_Node *node, DOM_Node *parent)
{
    LOCAL_ASSERT(parent->type == NODE_TYPE_ELEMENT);

    /* XXX factor this information out, and share with parser */
     if (node->type == NODE_TYPE_ELEMENT) {
        /*
         * Because HTML is such a cool markup language, we have to
         * worry about tags that are implicitly closed by new tags.
         * There are a couple of cases:
         *
         * <P> et alii: they close the ``enclosing'' one of the same type,
         * so that you get:
         * <SOME>
         *   <P>
         *   <P>
         * and not
         * <SOME>
         *   <P>
         *     <P>
         *
         * <DL>/<DT>/<DD>: these close up to an enclosing DL element, so
         * that you get:
         * <DL>
         *   <DT>
         *   <DD>
         * and not
         * <DL>
         *   <DT>
         *     <DD>
         * We could enforce further stricture here, I guess, by only
         * allowing <DT> and <DD> elements as children of <DL>, etc.
         * Maybe later.
         */
        TagType type = ELEMENT_PRIV(node)->tagtype;
        DOM_Node *iter;
        TagType breakType = P_UNKNOWN;
        JSBool breakIsNewParent = JS_FALSE;
        
        switch(type) {
          case P_DESC_TITLE:
          case P_DESC_TEXT:
            breakType = P_DESC_LIST;
            breakIsNewParent = JS_TRUE;
            /* fallthrough */
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
            iter = parent;
            if (breakType == P_UNKNOWN)
                breakType = type;
            while (iter &&
                   iter->parent->type == NODE_TYPE_ELEMENT) {
                /* find the enclosing tag for this type */
                if (ELEMENT_PRIV(iter)->tagtype == breakType) {
                    if (breakIsNewParent)
                        parent = iter;
                    else
                        parent = iter->parent;
                    break;
                }
                /* XXX CLOSE_NODE(iter); */
                iter = iter->parent;
            }
#ifdef DEBUG_shaver
            LM_Node_indent -= 2;
#endif
            break;
          default:;
        }
    }

#ifdef DEBUG_shaver
        LM_Node_indent -= 2;
#endif
#ifdef DEBUG_shaver_verbose
    {
        TagType dbgtype = 0, partype = 0;       /* text */
        if (node->type == NODE_TYPE_ELEMENT)
            dbgtype = ELEMENT_PRIV(node)->tagtype;
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
        return NULL;
    LOCAL_ASSERT(node->parent == parent);
    if (node->type == NODE_TYPE_TEXT ||
        (node->type == NODE_TYPE_ELEMENT &&
         lo_IsEmptyTag(ELEMENT_PRIV(node)->tagtype))) {
        return parent;
    } else {
        return node;
    }

}

void /* DOM_Node */ *
LM_ReflectTagNode(PA_Tag *tag, void *doc_state, MWContext *context)
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
    int16 csid = INTL_GetCSIWinCSID(c);
    lo_DocState *doc = (lo_DocState *)doc_state;
    DOM_Node *node;
    JSContext *cx;
    
    cx = context->mocha_context;

    if (!TOP_NODE(doc)) {
        DOM_HTMLElementPrivate *elepriv;
#if 0
        node = DOM_NewDocument(context, doc);
#else
        node = XP_NEW_ZAP(DOM_Node);
#endif
        if (!node)
            return NULL;
        node->type = NODE_TYPE_DOCUMENT;
        node->name = XP_STRDUP("#document");
        TOP_NODE(doc) = node;

        /* now put a single <HTML> element as child */
        node = (DOM_Node *)DOM_NewElement ("HTML", &lm_ElementOps,
                                           NULL, NULL, NULL, &lm_NodeOps);
        if (!node)
            return NULL;
        elepriv = XP_NEW_ZAP(DOM_HTMLElementPrivate);
        if (!elepriv) {
            XP_FREE(node);
            return NULL;
        }
        elepriv->tagtype = P_HTML;
        elepriv->doc_id = context->doc_id;

        node->data = elepriv;
        TOP_NODE(doc)->child = node;
        node->parent = TOP_NODE(doc);
        CURRENT_NODE(doc) = node;
        ACTIVE_NODE(doc) = node;
    }

    if (!tag) {
        CURRENT_NODE(doc) = TOP_NODE(doc)->child;
        ACTIVE_NODE(doc) = CURRENT_NODE(doc);
        return CURRENT_NODE(doc);
    }
    
    if (tag->type == P_UNKNOWN ||
        tag->type == P_HTML)
        return CURRENT_NODE(doc);

    if (tag->is_end) {
        DOM_Node *last_node, *closing_node;
        LOCAL_ASSERT(CURRENT_NODE(doc)->type == NODE_TYPE_ELEMENT ||
                     CURRENT_NODE(doc)->type == NODE_TYPE_TEXT);
        closing_node = CURRENT_NODE(doc);
        last_node = (DOM_Node *)DOM_HTMLPopElementByType(tag->type,
                                            (DOM_Element *)CURRENT_NODE(doc));
        if (!last_node)
            return NULL;
        LOCAL_ASSERT(last_node->parent &&
                     last_node->type == NODE_TYPE_ELEMENT);
        /*
         * The caller is interested in the node that we just closed.
         */
        CURRENT_NODE(doc) = last_node;
        ACTIVE_NODE(doc) = CURRENT_NODE(doc);
        return closing_node;
    }

    node = lm_NodeForTag(cx, tag, CURRENT_NODE(doc), context, csid);
    if (node) {
        DOM_Node *newCurrent = DOM_HTMLPushNode(node, CURRENT_NODE(doc));
        if (!newCurrent) {
#ifdef DEBUG_shaver
            fprintf(stderr, "bad push of node %d for tag %d\n",
                    node->type, tag->type);
#endif
            if (node->ops && node->ops->destroyNode)
                node->ops->destroyNode(context->mocha_context,
                                       node);
            return NULL;
        }
        ACTIVE_NODE(doc) = node;
        LOCAL_ASSERT(node->parent &&
                     node->parent->type != NODE_TYPE_DOCUMENT);
        CURRENT_NODE(doc) = newCurrent;

    } else {
    }
    XP_ASSERT(!CURRENT_NODE(doc)->parent || 
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

/*
 * Close the element, popping it (and enclosing elements that are marked
 * closed) if it's the current element, otherwise marking it closed and
 * returning.
 */

DOM_Element *
DOM_HTMLPopElementByType(TagType type, DOM_Element *element)
{
    DOM_Node *closing;
#ifdef DEBUG_shaver_verbose
    int new_indent = LM_Node_indent;
#endif

    LOCAL_ASSERT(element->node.type != NODE_TYPE_DOCUMENT);
    if (element->node.type == NODE_TYPE_DOCUMENT)
        return element;

    LOCAL_ASSERT(type != P_HTML && type != NODE_TYPE_TEXT);

    closing = (DOM_Node *)element;

    if (closing->type == NODE_TYPE_TEXT)
        /* really, we're closing the enclosing parent */
        closing = closing->parent;

    /* if we don't match, just mark it closed */
    if (ELEMENT_PRIV(closing)->tagtype != type) {
#ifdef DEBUG_shaver_treegen
        fprintf(stderr, "</%s> doesn't close <%s> ", PA_TagString(type),
                PA_TagString(ELEMENT_PRIV(closing)->tagtype));
#endif
        /* find it, mark it closed, and return. */
        do {
#ifdef DEBUG_shaver_treegen
            fprintf(stderr, "skipping <%s>",
                    PA_TagString(ELEMENT_PRIV(closing)->tagtype));
#endif
            closing = closing->parent;
        } while (closing->type == NODE_TYPE_ELEMENT &&
                 ELEMENT_PRIV(closing)->tagtype != type &&
                 ELEMENT_PRIV(closing)->tagtype != P_HTML);
        if (closing->type == NODE_TYPE_ELEMENT &&
            ELEMENT_PRIV(closing)->tagtype == type) {
#ifdef DEBUG_shaver_treegen
            fprintf(stderr, "found <%s>, marking closed\n",
                    PA_TagString(ELEMENT_PRIV(closing)->tagtype));
#endif
            LM_SetNodeFlags(closing, NODE_CLOSED);
        } else {
#ifdef DEBUG_shaver_treegen
            fprintf(stderr, "didn't find <%s>\n", PA_TagString(type));
#endif
        }
        return element;
    }

    /*
     * This matches, so close it off.
     * We should call LO_CloseNode so that it can close layers and the like
     * as appropriate.
     */
#ifdef DEBUG_shaver_treegen
    fprintf(stderr, "closing matched <%s> ", PA_TagString(type));
#endif
    closing = element->node.parent;
    if (ELEMENT_PRIV(closing)->flags & NODE_CLOSED) {
        do {
#ifdef DEBUG_shaver_treegen
            fprintf(stderr, "<%s> already closed ",
                    PA_TagString(ELEMENT_PRIV(closing)->tagtype));
#endif
            LM_ClearNodeFlags(closing, NODE_CLOSED);
            /* XXX CLOSE_NODE(closing) */
            closing = closing->parent;
            LOCAL_ASSERT(closing);
        } while (closing->type == NODE_TYPE_ELEMENT &&
                 ELEMENT_PRIV(closing)->tagtype != P_HTML &&
                 (ELEMENT_PRIV(closing)->flags & NODE_CLOSED));
    }

    fputs("\n", stderr);

    return (DOM_Element *)closing;
}

JSBool
LM_SetNodeFlags(DOM_Node *node, uint32 flags)
{
    DOM_HTMLElementPrivate *priv;
    XP_ASSERT(PR_CurrentThread() == mozilla_thread);
    if (!(node->type == NODE_TYPE_ELEMENT ||
          node->type == NODE_TYPE_TEXT))
        return JS_FALSE;
    priv = ELEMENT_PRIV(node);
    if (!priv)
        return JS_FALSE;
    priv->flags |= flags;
    return JS_TRUE;
}

JSBool
LM_ClearNodeFlags(DOM_Node *node, uint32 flags)
{
    DOM_HTMLElementPrivate *priv = ELEMENT_PRIV(node);
    XP_ASSERT(PR_CurrentThread() == mozilla_thread);
    if (!(node->type == NODE_TYPE_ELEMENT ||
          node->type == NODE_TYPE_TEXT) ||
        !priv)
        return JS_FALSE;
        return JS_FALSE;
    priv->flags &= ~flags;
    return JS_TRUE;
}

#include "laystyle.h"
#define IMAGE_DEF_ANCHOR_BORDER         2
#define IMAGE_DEF_VERTICAL_SPACE	0

DOM_StyleDatabase *
DOMMOZ_NewStyleDatabase(JSContext *cx, lo_DocState *state)
{
    DOM_StyleDatabase *db;
    LO_Color visitCol, linkCol;
    DOM_StyleSelector *sel, *imgsel;
    DOM_AttributeEntry *entry;
    lo_TopState *top = state->top_state;

    /*
     * Install default rules.
     * In an ideal world (perhaps 5.0?), we would parse .netscape/ua.css
     * at startup and keep the JSSS style buffer around for execution
     * right here.  That would be very cool in many ways, including the
     * fact that people could have ua.css at all.  We might want to
     * make the weighting stuff work correctly at the same time, too,
     * but I don't think it's vital.
     */
    db = DOM_NewStyleDatabase(cx);
    if (!db)
        return NULL;

    linkCol.red = STATE_UNVISITED_ANCHOR_RED(state);
    linkCol.green = STATE_UNVISITED_ANCHOR_GREEN(state);
    linkCol.blue = STATE_UNVISITED_ANCHOR_BLUE(state);
    visitCol.red = STATE_VISITED_ANCHOR_RED(state);
    visitCol.green = STATE_VISITED_ANCHOR_GREEN(state);
    visitCol.blue = STATE_VISITED_ANCHOR_BLUE(state);

    top->style_db = db;

    sel = DOM_StyleFindSelectorFull(cx, db, NULL, SELECTOR_TAG,
                                    "A", NULL, "link");
    if (!sel)
        goto error;

#define SET_DEFAULT_VALUE(name, value)                                        \
        entry = DOM_StyleAddRule(cx, db, sel, name, "default");               \
        if (!entry)                                                           \
            goto error;                                                       \
        entry->dirty = JS_FALSE;                                              \
        entry->data = value;
        
    /* A:link { color:prefLinkColor } */
    SET_DEFAULT_VALUE(COLOR_STYLE, *(uint32*)&linkCol);

    /* A:link { text-decoration:underline } */
    if (lo_underline_anchors() &&
        !DOM_StyleAddRule(cx, db, sel, TEXTDECORATION_STYLE, "underline"))
        goto error;

    sel = DOM_StyleFindSelectorFull(cx, db, NULL, SELECTOR_TAG,
                                    "A", NULL, "visited");
    if (!sel)
        goto error;

    /* A:visited { color:prefVisitedLinkColor } */
    SET_DEFAULT_VALUE(COLOR_STYLE, *(uint32*)&visitCol);

    /* A:visited { text-decoration:underline } */
    if (lo_underline_anchors() &&
        !DOM_StyleAddRule(cx, db, sel, TEXTDECORATION_STYLE, "underline"))
        goto error;

    /* set styles for IMG within A:link and A:visited */
    /* XXX should set (and teach layout about) borderTop/Bottom, etc. */
    imgsel = DOM_StyleFindSelectorFull(cx, db, NULL, SELECTOR_TAG,
                                       "IMG", NULL, NULL);
    if (!imgsel)
        goto error;

    /* set border styles for ``A:link IMG'' */
    sel = DOM_StyleFindSelectorFull(cx, db, imgsel, SELECTOR_TAG,
                                    "A", NULL, "link");
    if (!sel)
        goto error;
    SET_DEFAULT_VALUE(BORDERWIDTH_STYLE, IMAGE_DEF_ANCHOR_BORDER);
    SET_DEFAULT_VALUE(PADDING_STYLE, IMAGE_DEF_VERTICAL_SPACE);

    /* set border styles for ``A:visited IMG'' */
    sel = DOM_StyleFindSelectorFull(cx, db, imgsel, SELECTOR_TAG,
                                    "A", NULL, "visited");
    if (!sel)
        goto error;
    SET_DEFAULT_VALUE(BORDERWIDTH_STYLE, IMAGE_DEF_ANCHOR_BORDER);
    SET_DEFAULT_VALUE(PADDING_STYLE, IMAGE_DEF_VERTICAL_SPACE);

#ifdef DEBUG_shaver
    fprintf(stderr, "successfully added all default rules to db %p\n",
            db);
#endif
    return db;

 error:
    if (db)
        DOM_DestroyStyleDatabase(cx, db);
    return NULL;
}

DOM_StyleDatabase *
DOM_StyleDatabaseFromContext(JSContext *cx)
{
    MochaDecoder *decoder;
    lo_TopState *top;
    lo_DocState *state;
    DOM_StyleDatabase *db = NULL;

    if (!cx)
        return NULL;

    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    if (!decoder)
        return NULL;
    
    LO_LockLayout();
    top = lo_FetchTopState(decoder->window_context->doc_id);
    if (!top)
        goto out;

    if (top->style_db) {
        LO_UnlockLayout();
        return (DOM_StyleDatabase *)top->style_db;
    }

    state = top->doc_state;
    if (!state)
        goto out;

    db = DOMMOZ_NewStyleDatabase(cx, state);
    top->style_db = db;

out:
    LO_UnlockLayout();
    return db;
}

