/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey E4X code, released August, 2004.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsxml_h___
#define jsxml_h___

#include "jsstddef.h"
#include "jspubtd.h"

/* XXXbe move to jsprvtd.h or jspubtd.h */
typedef struct JSXML            JSXML;
typedef struct JSXMLNamespace   JSXMLNamespace;
typedef struct JSXMLQName       JSXMLQName;
typedef struct JSXMLArray       JSXMLArray;

extern const char js_isXMLName_str[];
extern const char js_Namespace_str[];
extern const char js_QName_str[];
extern const char js_XML_str[];
extern const char js_XMLList_str[];

extern const char js_amp_entity_str[];
extern const char js_gt_entity_str[];
extern const char js_lt_entity_str[];
extern const char js_quot_entity_str[];

struct JSXMLNamespace {
    JSObject    *object;
    JSString    *prefix;
    JSString    *uri;
};

extern JSXMLNamespace *
js_NewXMLNamespace(JSContext *cx, JSString *prefix, JSString *uri);

extern void
js_DestroyXMLNamespace(JSContext *cx, JSXMLNamespace *ns);

extern JSObject *
js_NewXMLNamespaceObject(JSContext *cx, JSString *prefix, JSString *uri);

extern JSObject *
js_GetXMLNamespaceObject(JSContext *cx, JSXMLNamespace *ns);

struct JSXMLQName {
    JSObject    *object;
    JSString    *uri;
    JSString    *prefix;
    JSString    *localName;
};

extern JSXMLQName *
js_NewXMLQName(JSContext *cx, JSString *uri, JSString *prefix,
               JSString *localName);

extern void
js_DestroyXMLQName(JSContext *cx, JSXMLQName *qn);

extern JSObject *
js_NewXMLQNameObject(JSContext *cx, JSString *uri, JSString *prefix,
                     JSString *localName);

extern JSObject *
js_GetXMLQNameObject(JSContext *cx, JSXMLQName *qn);

extern JSObject *
js_ConstructQNameObject(JSContext *cx, jsval nsval, jsval lnval);

typedef JSBool
(* JS_DLL_CALLBACK JSIdentityOp)(const void *a, const void *b);

struct JSXMLArray {
    uint32              count;
    uint32              length;
    void                **vector;
};

typedef enum JSXMLClass {
    JSXML_CLASS_LIST,
    JSXML_CLASS_ELEMENT,
    JSXML_CLASS_TEXT,
    JSXML_CLASS_ATTRIBUTE,
    JSXML_CLASS_COMMENT,
    JSXML_CLASS_PROCESSING_INSTRUCTION,
    JSXML_CLASS_LIMIT
} JSXMLClass;

#define JSXML_CLASS_HAS_KIDS(xml_class)  ((xml_class) < JSXML_CLASS_TEXT)
#define JSXML_CLASS_HAS_VALUE(xml_class) ((xml_class) >= JSXML_CLASS_TEXT)

struct JSXML {
    JSObject            *object;
    JSXML               *parent;
    JSXMLQName          *name;
    jsrefcount          nrefs;          /* references from JSObjects */
    uint32              flags;
    JSXMLClass          xml_class;
    union {
        struct {
            JSXMLArray  kids;           /* NB: must come first */
            JSXML       *target;
            JSXMLQName  *targetprop;
        } list;
        struct {
            JSXMLArray  kids;           /* NB: must come first */
            JSXMLArray  namespaces;
            JSXMLArray  attrs;
        } elem;
        JSString        *value;
    } u;
};

#define xml_kids        u.list.kids
#define xml_target      u.list.target
#define xml_targetprop  u.list.targetprop
#define xml_namespaces  u.elem.namespaces
#define xml_attrs       u.elem.attrs
#define xml_value       u.value

#define JSXML_HAS_KIDS(xml)     JSXML_CLASS_HAS_KIDS((xml)->xml_class)
#define JSXML_HAS_VALUE(xml)    JSXML_CLASS_HAS_VALUE((xml)->xml_class)
#define JSXML_LENGTH(xml)       (JSXML_CLASS_HAS_KIDS((xml)->xml_class)       \
                                 ? (xml)->xml_kids.count                      \
                                 : 0)

/* JSXML flag definitions. */
#define XML_COPY_ON_WRITE       0x1

extern JSXML *
js_NewXML(JSContext *cx, JSXMLClass xml_class);

extern void
js_DestroyXML(JSContext *cx, JSXML *xml);

extern JSXML *
js_ParseNodeToXML(JSContext *cx, JSParseNode *pn, uint32 flags);

extern JSObject *
js_NewXMLObject(JSContext *cx, JSXMLClass xml_class);

extern JSObject *
js_GetXMLObject(JSContext *cx, JSXML *xml);

extern JS_FRIEND_DATA(JSXMLObjectOps)   js_XMLObjectOps;
extern JS_FRIEND_DATA(JSClass)          js_XMLClass;

/*
 * NB: jsapi.h and jsobj.h must be included before any call to this macro.
 */
#define OBJECT_IS_XML(cx,obj)   ((obj)->map->ops == &js_XMLObjectOps.base)
#define JSVAL_IS_XML(cx,v)      (!JSVAL_IS_PRIMITIVE(v) &&                    \
                                 OBJECT_IS_XML(cx, JSVAL_TO_OBJECT(v)))

extern JSObject *
js_InitNamespaceClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitQNameClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitXMLClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitXMLClasses(JSContext *cx, JSObject *obj);

extern JSBool
js_GetDefaultXMLNamespace(JSContext *cx, jsval *vp);

extern JSBool
js_SetDefaultXMLNamespace(JSContext *cx, jsval v);

/*
 * Return true if v is a XML QName object, or if it converts to a string that
 * contains a valid XML qualified name (one containing no :), false otherwise.
 * NB: This function is an infallible predicate, it hides exceptions.
 */
extern JSBool
js_IsXMLName(JSContext *cx, jsval v);

extern JSBool
js_ToAttributeName(JSContext *cx, jsval *vp);

extern JSString *
js_EscapeAttributeValue(JSContext *cx, JSString *str);

extern JSString *
js_AddAttributePart(JSContext *cx, JSBool isName, JSString *str,
                    JSString *str2);

extern JSBool
js_GetAnyName(JSContext *cx, jsval *vp);

extern JSBool
js_BindXMLProperty(JSContext *cx, jsval lval, jsval rval);

extern JSBool
js_FindXMLProperty(JSContext *cx, jsval name, JSObject **objp, jsval *namep);

extern JSBool
js_GetXMLProperty(JSContext *cx, JSObject *obj, jsval name, jsval *vp);

extern JSBool
js_SetXMLProperty(JSContext *cx, JSObject *obj, jsval name, jsval *vp);

extern JSBool
js_GetXMLDescendants(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_FilterXMLList(JSContext *cx, JSObject *obj, jsbytecode *pc, uint32 len,
                 jsval *vp);

extern JSObject *
js_ValueToXMLObject(JSContext *cx, jsval v);

extern JSObject *
js_ValueToXMLListObject(JSContext *cx, jsval v);

extern JSObject *
js_CloneXMLObject(JSContext *cx, JSObject *obj);

#endif /* jsxml_h___ */
