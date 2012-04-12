/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "jspubtd.h"
#include "jsobj.h"
#include "jscell.h"

#include "gc/Barrier.h"

extern const char js_AnyName_str[];
extern const char js_AttributeName_str[];
extern const char js_isXMLName_str[];
extern const char js_XMLList_str[];

extern const char js_amp_entity_str[];
extern const char js_gt_entity_str[];
extern const char js_lt_entity_str[];
extern const char js_quot_entity_str[];

template<class T>
struct JSXMLArrayCursor;

template<class T>
struct JSXMLArray
{
    uint32_t            length;
    uint32_t            capacity;
    js::HeapPtr<T>      *vector;
    JSXMLArrayCursor<T> *cursors;

    void init() {
        length = capacity = 0;
        vector = NULL;
        cursors = NULL;
    }

    void finish(js::FreeOp *fop);

    bool setCapacity(JSContext *cx, uint32_t capacity);
    void trim();
};

template<class T>
struct JSXMLArrayCursor
{
    typedef js::HeapPtr<T> HeapPtrT;

    JSXMLArray<T>       *array;
    uint32_t            index;
    JSXMLArrayCursor<T> *next;
    JSXMLArrayCursor<T> **prevp;
    HeapPtrT            root;

    JSXMLArrayCursor(JSXMLArray<T> *array)
      : array(array), index(0), next(array->cursors), prevp(&array->cursors),
        root(NULL)
    {
        if (next)
            next->prevp = &next;
        array->cursors = this;
    }

    ~JSXMLArrayCursor() { disconnect(); }

    void disconnect() {
        if (!array)
            return;
        if (next)
            next->prevp = prevp;
        *prevp = next;
        array = NULL;
        root.~HeapPtrT();
    }

    T *getNext() {
        if (!array || index >= array->length)
            return NULL;
        return root = array->vector[index++];
    }

    T *getCurrent() {
        if (!array || index >= array->length)
            return NULL;
        return root = array->vector[index];
    }
};

void js_XMLArrayCursorTrace(JSTracer *trc, JSXMLArrayCursor<JSXML> *cursor);
void js_XMLArrayCursorTrace(JSTracer *trc, JSXMLArrayCursor<JSObject> *cursor);

#define JSXML_PRESET_CAPACITY   JS_BIT(31)
#define JSXML_CAPACITY_MASK     JS_BITMASK(31)
#define JSXML_CAPACITY(array)   ((array)->capacity & JSXML_CAPACITY_MASK)

/*
 * NB: don't reorder this enum without changing all array initializers that
 * depend on it in jsxml.c.
 */
typedef enum JSXMLClass {
    JSXML_CLASS_LIST,
    JSXML_CLASS_ELEMENT,
    JSXML_CLASS_ATTRIBUTE,
    JSXML_CLASS_PROCESSING_INSTRUCTION,
    JSXML_CLASS_TEXT,
    JSXML_CLASS_COMMENT,
    JSXML_CLASS_LIMIT
} JSXMLClass;

#define JSXML_CLASS_HAS_KIDS(class_)    ((class_) < JSXML_CLASS_ATTRIBUTE)
#define JSXML_CLASS_HAS_VALUE(class_)   ((class_) >= JSXML_CLASS_ATTRIBUTE)
#define JSXML_CLASS_HAS_NAME(class_)                                          \
    ((unsigned)((class_) - JSXML_CLASS_ELEMENT) <=                               \
     (unsigned)(JSXML_CLASS_PROCESSING_INSTRUCTION - JSXML_CLASS_ELEMENT))

#ifdef DEBUG_notme
#include "jsclist.h"
#endif

typedef struct JSXMLListVar {
    JSXMLArray<JSXML>   kids;           /* NB: must come first */
    js::HeapPtrXML      target;
    js::HeapPtrObject   targetprop;
} JSXMLListVar;

typedef struct JSXMLElemVar {
    JSXMLArray<JSXML>    kids;          /* NB: must come first */
    JSXMLArray<JSObject> namespaces;
    JSXMLArray<JSXML>    attrs;
} JSXMLElemVar;

/* union member shorthands */
#define xml_kids        list.kids
#define xml_target      list.target
#define xml_targetprop  list.targetprop
#define xml_namespaces  elem.namespaces
#define xml_attrs       elem.attrs
#define xml_value       value

/* xml_class-testing macros */
#define JSXML_HAS_KIDS(xml)     JSXML_CLASS_HAS_KIDS((xml)->xml_class)
#define JSXML_HAS_VALUE(xml)    JSXML_CLASS_HAS_VALUE((xml)->xml_class)
#define JSXML_HAS_NAME(xml)     JSXML_CLASS_HAS_NAME((xml)->xml_class)
#define JSXML_LENGTH(xml)       (JSXML_CLASS_HAS_KIDS((xml)->xml_class)       \
                                 ? (xml)->xml_kids.length                     \
                                 : 0)

struct JSXML : js::gc::Cell {
#ifdef DEBUG_notme
    JSCList             links;
    uint32_t            serial;
#endif
    js::HeapPtrObject   object;
    void                *domnode;       /* DOM node if mapped info item */
    js::HeapPtrXML      parent;
    js::HeapPtrObject   name;
    uint32_t            xml_class;      /* discriminates u, below */
    uint32_t            xml_flags;      /* flags, see below */

    JSXMLListVar        list;
    JSXMLElemVar        elem;
    js::HeapPtrString   value;

#if JS_BITS_PER_WORD == 32
    /* The size of every GC thing must be divisible by the FreeCell size. */
    void *pad;
#endif

    void finalize(js::FreeOp *fop);

    static void writeBarrierPre(JSXML *xml);
    static void writeBarrierPost(JSXML *xml, void *addr);
};

/* xml_flags values */
#define XMLF_WHITESPACE_TEXT    0x1

extern JSXML *
js_NewXML(JSContext *cx, JSXMLClass xml_class);

extern void
js_TraceXML(JSTracer *trc, JSXML *xml);

extern JSObject *
js_NewXMLObject(JSContext *cx, JSXMLClass xml_class);

extern JSObject *
js_GetXMLObject(JSContext *cx, JSXML *xml);

extern JSObject *
js_InitNamespaceClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitQNameClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitXMLClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitXMLClasses(JSContext *cx, JSObject *obj);

/*
 * If obj is a QName corresponding to function::name, set *funidp to name's id
 * and return true, else return false.
 */
extern bool
js_GetLocalNameFromFunctionQName(JSObject *obj, jsid *funidp, JSContext *cx);

extern JSBool
js_GetDefaultXMLNamespace(JSContext *cx, jsval *vp);

extern JSBool
js_SetDefaultXMLNamespace(JSContext *cx, const js::Value &v);

/*
 * Return true if v is a XML QName object, or if it converts to a string that
 * contains a valid XML qualified name (one containing no :), false otherwise.
 * NB: This function is an infallible predicate, it hides exceptions.
 */
extern JSBool
js_IsXMLName(JSContext *cx, jsval v);

extern JSBool
js_ToAttributeName(JSContext *cx, js::Value *vp);

extern JSFlatString *
js_EscapeAttributeValue(JSContext *cx, JSString *str, JSBool quote);

extern JSString *
js_AddAttributePart(JSContext *cx, JSBool isName, JSString *str,
                    JSString *str2);

extern JSFlatString *
js_EscapeElementValue(JSContext *cx, JSString *str);

extern JSString *
js_ValueToXMLString(JSContext *cx, const js::Value &v);

extern JSObject *
js_ConstructXMLQNameObject(JSContext *cx, const js::Value & nsval,
                           const js::Value & lnval);

extern JSBool
js_GetAnyName(JSContext *cx, jsid *idp);

/*
 * Note: nameval must be either QName, AttributeName, or AnyName.
 */
extern JSBool
js_FindXMLProperty(JSContext *cx, const js::Value &nameval, JSObject **objp, jsid *idp);

extern JSBool
js_GetXMLMethod(JSContext *cx, js::HandleObject obj, jsid id, js::Value *vp);

extern JSBool
js_GetXMLDescendants(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_DeleteXMLListElements(JSContext *cx, JSObject *listobj);

extern JSBool
js_StepXMLListFilter(JSContext *cx, JSBool initialized);

extern JSObject *
js_ValueToXMLObject(JSContext *cx, const js::Value &v);

extern JSObject *
js_ValueToXMLListObject(JSContext *cx, const js::Value &v);

extern JSObject *
js_NewXMLSpecialObject(JSContext *cx, JSXMLClass xml_class, JSString *name,
                       JSString *value);

extern JSString *
js_MakeXMLCDATAString(JSContext *cx, JSString *str);

extern JSString *
js_MakeXMLCommentString(JSContext *cx, JSString *str);

extern JSString *
js_MakeXMLPIString(JSContext *cx, JSString *name, JSString *str);

/* The caller must ensure that either v1 or v2 is an object. */
extern JSBool
js_TestXMLEquality(JSContext *cx, const js::Value &v1, const js::Value &v2,
                   JSBool *bp);

extern JSBool
js_ConcatenateXML(JSContext *cx, JSObject *obj1, JSObject *obj2, js::Value *vp);

namespace js {

extern bool
GetLocalNameFromFunctionQName(JSObject *qn, JSAtom **namep, JSContext *cx);

} /* namespace js */

#endif /* jsxml_h___ */
