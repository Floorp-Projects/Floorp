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
 * JSAPI DOM stuff, distinct from any HTML/XML embedding.
 * See http://www.w3.org/DOM/ for details.
 */

#ifndef DOM_H
#define DOM_H

#include "jsapi.h"

typedef enum DOM_NodeType {
    NODE_TYPE_ELEMENT = 1,
    NODE_TYPE_ATTRIBUTE,
    NODE_TYPE_TEXT,
    NODE_TYPE_CDATA,
    NODE_TYPE_ENTITY_REF,
    NODE_TYPE_ENTITY,
    NODE_TYPE_PI,
    NODE_TYPE_COMMENT,
    NODE_TYPE_DOCUMENT,
    NODE_TYPE_DOCTYPE,
    NODE_TYPE_DOCFRAGMENT,
    NODE_TYPE_NOTATION
} DOM_NodeType;

typedef enum DOM_ExceptionCode {
    DOM_NONE = 0,
    DOM_INDEX_SIZE_ERR,
    DOM_WSTRING_SIZE_ERR,
    DOM_HIERARCHY_REQUEST_ERR,
    DOM_WRONG_DOCUMENT_ERR,
    DOM_INVALID_NAME_ERR,
    DOM_NO_DATA_ALLOWED_ERR,
    DOM_NO_MODIFICATION_ALLOWED_ERR,
    DOM_NOT_FOUND_ERR,
    DOM_NOT_SUPPORTED_ERR,
    DOM_INUSE_ATTRIBUTE_ERR,
    DOM_UNSUPPORTED_DOCUMENT_ERR
} DOM_ExceptionCode;

/* struct typedefs */
typedef struct DOM_Node                 DOM_Node;
typedef struct DOM_NodeOps              DOM_NodeOps;
typedef struct DOM_AttributeEntry       DOM_AttributeEntry;
typedef struct DOM_Element              DOM_Element;
typedef struct DOM_ElementOps           DOM_ElementOps;
typedef struct DOM_AttributeList        DOM_AttributeList;
typedef struct DOM_Attribute            DOM_Attribute;
typedef struct DOM_Document             DOM_Document;
typedef struct DOM_CharacterData        DOM_CharacterData;
typedef struct DOM_Text                 DOM_Text;

/*
 * All of these handlers are called before the DOM tree is manipulated, with
 * before == JS_TRUE, and then again afterwards, with before == JS_FALSE.
 *
 * Before:
 * Handlers should do nothing but check for legality of the operation in
 * question, and shouldn't make any changes to the underlying document
 * structure.  They should signal an error with DOM_SignalException as
 * appropriate and return false if the operation should not be permitted.
 * This handler will never be called if the DOM code can detect that the
 * operation in question is illegal (DOM_NOT_CHILD, for example).
 *
 * After:
 * The handlers should perform whatever back-end manipulation is appropriate
 * at this point, or signal an exception and return false.
 */
struct DOM_NodeOps {
    JSBool      (*insertBefore)(JSContext *cx, DOM_Node *node, DOM_Node *child,
                                DOM_Node *ref, JSBool before);
    JSBool      (*replaceChild)(JSContext *cx, DOM_Node *node, DOM_Node *child,
                                DOM_Node *old, JSBool before);
    JSBool      (*removeChild) (JSContext *cx, DOM_Node *node, DOM_Node *old,
                                JSBool before);
    JSBool      (*appendChild) (JSContext *cx, DOM_Node *node, DOM_Node *child,
                                JSBool before);

    /* free up Node-private data */
    void        (*destroyNode) (JSContext *cx, DOM_Node *node);

    /* construct a JSObject and fill in Node-private data as appropriate. */
    JSObject *  (*reflectNode) (JSContext *cx, DOM_Node *node);

};

/* stubs */
JSBool
DOM_InsertBeforeStub(JSContext *cx, DOM_Node *node, DOM_Node *child,
                     DOM_Node *ref, JSBool before);

JSBool
DOM_ReplaceChildStub(JSContext *cx, DOM_Node *node, DOM_Node *child,
                     DOM_Node *old, JSBool before);

JSBool
DOM_RemoveChildStub(JSContext *cx, DOM_Node *node, DOM_Node *old,
                    JSBool before);

JSBool
DOM_AppendChildStub(JSContext *cx, DOM_Node *node, DOM_Node *child,
                    JSBool before);

void
DOM_DestroyNodeStub(JSContext *cx, DOM_Node *node);

JSObject *
DOM_ReflectNodeStub(JSContext *cx, DOM_Node *node);

/*
 * ElementOps are called before any JS or DOM reflection occurs,
 * and an error return (JS_FALSE or NULL) will prevent such
 * manipulation.
 */
struct DOM_ElementOps {
    /*
     * if this succeeds, pre-existing JS reflections (DOM_Attributes)
     * will be updated as well.  Removal (removeAttribute, etc.) of an
     * attribute is signalled by passing a value of NULL.  The
     * setAttribute function is responsible for doing
     * DOM_SignalException(cx, DOM_INVALID_NAME_ERR) if it's an
     * invalid name.  (The DOM_NO_MODIFICATION_ALLOWED_ERR case is
     * handled in the DOM code itself, and no call will be made to the
     * setAttribute handler in that case.)
     */
    JSBool              (*setAttribute)(JSContext *cx, DOM_Element *element,
                                        const char *name, const char *value);

    /* returns attribute value (caller must copy) or NULL if not found */
    const char *        (*getAttribute)(JSContext *cx, DOM_Element *element,
                                        const char *name, JSBool *cacheable);

    /* returns number of attributes, or -1 if the number isn't known */
    intN                (*getNumAttrs)(JSContext *cx, DOM_Element *element,
                                       JSBool *cacheable);
};

/* stubs */

JSBool
DOM_SetAttributeStub(JSContext *cx, DOM_Element *element, const char *name,
                     const char *value);

const char *
DOM_GetAttributeStub(JSContext *cx, DOM_Element *element, const char *name,
                     JSBool *cacheable);

intN
DOM_GetNumAttrsStub(JSContext *cx, DOM_Element *element, JSBool *cacheable);

JSBool
DOM_SignalException(JSContext *cx, DOM_ExceptionCode exception);

/* The basic node */

struct DOM_Node {
    DOM_NodeType        type;
    DOM_NodeOps         *ops;
    char                *name;
    struct DOM_Node     *sibling;
    struct DOM_Node     *prev_sibling;
    struct DOM_Node     *child;
    struct DOM_Node     *parent;
    JSObject            *mocha_object;
    void                *data;           /* embedding-private data */
};

JSBool
DOM_Init(JSContext *cx, JSObject *scope);

void
DOM_DestroyNode(JSContext *cx, DOM_Node *node);

void
DOM_DestroyTree(JSContext *cx, DOM_Node *top);

JSObject *
DOM_NewNodeObject(JSContext *cx, DOM_Node *node);

JSObject *
DOM_ObjectForNode(JSContext *cx, DOM_Node *node);

JSBool
DOM_PushNode(DOM_Node *node, DOM_Node *parent);

DOM_Node *
DOM_PopNode(DOM_Node *node);

/*
 * An attribute entry, for name="value".
 * data is for use by the underlying embedding, to keep it from having to parse
 * "value" every time.  Dirty is set to true when a set operation happens
 * through the DOM interface, and the embedding can key on that to reparse
 * "value", and reset data and dirty.  The "dtor" is called when the entry
 * is destroyed.  It should return JS_TRUE if it's OK to free the entry, and
 * JS_FALSE otherwise.
 */

struct DOM_AttributeEntry;      /* forward decl */

typedef JSBool(*DOM_DataParser)(const char *str, uint32 *data, void *closure);
typedef JSBool(*DOM_DataDestructor)(DOM_AttributeEntry *entry);

struct DOM_AttributeEntry {
    const char *name;
    const char *value;
    uint32 data;
    JSBool dirty;
    DOM_DataDestructor dtor;
};

JSBool
DOM_GetCleanEntryData(JSContext *cx, DOM_AttributeEntry *entry,
                      DOM_DataParser parser, DOM_DataDestructor dtor,
                      uint32 *data, void *closure);

struct DOM_Element {
    DOM_Node            node;
    DOM_ElementOps      *ops;
    const char          *tagName;
    uintN               nattrs;
    DOM_AttributeEntry  *attrs;
    char                *styleClass;
    char                *styleID;
};

/*
 * Create a new Element node.
 * Caller hands off name and tagName; they're freed at destruction time.
 */

DOM_Element *
DOM_NewElement(const char *tagName, DOM_ElementOps *eleops, char *name,
               char *styleClass, char *styleID, DOM_NodeOps *nodeops);

JSObject *
DOM_NewElementObject(JSContext *cx, DOM_Element *element);

JSObject *
DOM_ObjectForElement(JSContext *cx, DOM_Element *element);

JSBool
DOM_GetElementAttribute(JSContext *cx, DOM_Element *element, const char *name,
                        DOM_AttributeEntry **entryp);

JSBool
DOM_SetElementAttribute(JSContext *cx, DOM_Element *element, const char *name,
                        const char *value);


/*
 * Set the attributes from a pair of synchronized lists.
 * (This is what PA_FetchAllNameValues provides, handily enough.)
 *
 * The names and values lists should be malloc'd by the caller, and will
 * be freed by the engine as appropriate.
 */
JSBool
DOM_SetElementAttributes(JSContext *cx, DOM_Element *element,
                         const char **names, const char **values, uintN count);

/*
 * Set the attributes from a list that looks like:
 * name1, value1, name2, value2, ..., name<n>, value<n>, NULL.
 *
 * By pure coincidence, this is what the expat start-element callback
 * provides.  What luck!
 * 
 * The attrs list should be malloc'd by the caller, as well as all the
 * entries.  The DOM engine will free them as appropriate.
 */
JSBool
DOM_SetElementAttributes2(JSContext *cx, DOM_Element *element,
                          const char **attrs);

/*
 * Remove all attributes from an element.
 */
JSBool
DOM_ClearElementAttributes(JSContext *cx, DOM_Element *element);

struct DOM_Attribute {
    DOM_Node node;
    char * name;
    DOM_Element *element;
};

DOM_Attribute *
DOM_NewAttribute(const char *name, const char *value, DOM_Element *element);

JSObject *
DOM_NewAttributeObject(JSContext *cx, DOM_Attribute *attr);

JSObject *
DOM_ObjectForAttribute(JSContext *cx, DOM_Attribute *attr);

JSBool
DOM_SignalException(JSContext *cx, DOM_ExceptionCode code);

JSObject *
DOM_ObjectForNodeDowncast(JSContext *cx, DOM_Node *node);

struct DOM_Document {
    DOM_Node node;
};

typedef enum DOM_CDataOperationCode {
    CDATA_APPEND,
    CDATA_INSERT,
    CDATA_DELETE,
    CDATA_REPLACE
} DOM_CDataOperationCode;

typedef JSBool (*DOM_CDataOp)(JSContext *cx, DOM_CharacterData *cdata,
                              DOM_CDataOperationCode op);

struct DOM_CharacterData {
    DOM_Node node;
    char *data;
    uintN len;
    DOM_CDataOp notify;
};

struct DOM_Text {
    DOM_CharacterData cdata;
};

DOM_Text *
DOM_NewText(const char *data, int64 len, DOM_CDataOp notify,
            DOM_NodeOps *ops);

JSObject *
DOM_NewTextObject(JSContext *cx, DOM_Text *text);

JSObject *
DOM_ObjectForText(JSContext *cx, DOM_Text *text);

struct DOM_Comment {
    DOM_CharacterData cdata;
};

#endif /* DOM_H */
