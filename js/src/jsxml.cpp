/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
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

#include "jsversion.h"

#if JS_HAS_XML_SUPPORT

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jsprf.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsxml.h"
#include "jsstaticcheck.h"
#include "jsvector.h"

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsstrinlines.h"

#ifdef DEBUG
#include <string.h>     /* for #ifdef DEBUG memset calls */
#endif

using namespace js;
using namespace js::gc;

/*
 * NOTES
 * - in the js shell, you must use the -x command line option, or call
 *   options('xml') before compiling anything that uses XML literals
 *
 * TODO
 * - XXXbe patrol
 * - Fuse objects and their JSXML* private data into single GC-things
 * - fix function::foo vs. x.(foo == 42) collision using proper namespacing
 * - JSCLASS_DOCUMENT_OBSERVER support -- live two-way binding to Gecko's DOM!
 */

static inline bool
js_EnterLocalRootScope(JSContext *cx)
{
    return true;
}

static inline void
js_LeaveLocalRootScope(JSContext *cx)
{
}

static inline void
js_LeaveLocalRootScopeWithResult(JSContext *cx, jsval rval)
{
}

static inline void
js_LeaveLocalRootScopeWithResult(JSContext *cx, Value rval)
{
}

static inline void
js_LeaveLocalRootScopeWithResult(JSContext *cx, void *rval)
{
}

#ifdef XML_METERING
static struct {
    jsrefcount  qname;
    jsrefcount  xmlnamespace;
    jsrefcount  xml;
    jsrefcount  xmlobj;
} xml_stats;

#define METER(x)        JS_ATOMIC_INCREMENT(&(x))
#define UNMETER(x)      JS_ATOMIC_DECREMENT(&(x))
#else
#define METER(x)        /* nothing */
#define UNMETER(x)      /* nothing */
#endif

/*
 * Random utilities and global functions.
 */
const char js_isXMLName_str[]     = "isXMLName";
const char js_XMLList_str[]       = "XMLList";
const char js_localName_str[]     = "localName";
const char js_xml_parent_str[]    = "parent";
const char js_prefix_str[]        = "prefix";
const char js_toXMLString_str[]   = "toXMLString";
const char js_uri_str[]           = "uri";

const char js_amp_entity_str[]    = "&amp;";
const char js_gt_entity_str[]     = "&gt;";
const char js_lt_entity_str[]     = "&lt;";
const char js_quot_entity_str[]   = "&quot;";
const char js_leftcurly_entity_str[]   = "&#123;";

#define IS_STAR(str)  ((str)->length() == 1 && *(str)->chars() == '*')

static JSBool
GetXMLFunction(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JS_INLINE JSString *
GetPrefix(const JSObject *obj)
{
    jsval v = obj->getNamePrefix();
    if (JSVAL_IS_VOID(v))
        return NULL;
    JS_ASSERT(JSVAL_IS_STRING(v));
    return JSVAL_TO_STRING(v);
}

static JSString *
GetURI(const JSObject *obj)
{
    jsval v = obj->getNameURI();
    if (JSVAL_IS_VOID(v))
        return NULL;
    JS_ASSERT(JSVAL_IS_STRING(v));
    return JSVAL_TO_STRING(v);
}

static JSString *
GetLocalName(const JSObject *obj)
{
    jsval v = obj->getQNameLocalName();
    if (JSVAL_IS_VOID(v))
        return NULL;
    JS_ASSERT(JSVAL_IS_STRING(v));
    return JSVAL_TO_STRING(v);
}

static JSBool
IsDeclared(const JSObject *obj)
{
    jsval v;

    JS_ASSERT(obj->getClass() == &js_NamespaceClass);
    v = obj->getNamespaceDeclared();
    JS_ASSERT(JSVAL_IS_VOID(v) || v == JSVAL_TRUE);
    return v == JSVAL_TRUE;
}

static JSBool
xml_isXMLName(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = BOOLEAN_TO_JSVAL(js_IsXMLName(cx, argc ? vp[2] : JSVAL_VOID));
    return JS_TRUE;
}

static inline bool
AppendString(JSCharBuffer &cb, JSString *str)
{
    const jschar *chars, *end;
    str->getCharsAndEnd(chars, end);
    return cb.append(chars, end);
}

/*
 * This wrapper is needed because NewBuiltinClassInstance doesn't
 * call the constructor, and we need a place to set the
 * HAS_EQUALITY bit.
 */
static inline JSObject *
NewBuiltinClassInstanceXML(JSContext *cx, Class *clasp)
{
    JSObject *obj = NewBuiltinClassInstance(cx, clasp);
    if (obj)
        obj->syncSpecialEquality();
    return obj;
}

#define DEFINE_GETTER(name,code)                                               \
    static JSBool                                                              \
    name(JSContext *cx, JSObject *obj, jsid id, jsval *vp)                     \
    {                                                                          \
        code;                                                                  \
        return true;                                                           \
    }

/*
 * Namespace class and library functions.
 */
DEFINE_GETTER(NamePrefix_getter,
              if (obj->getClass() == &js_NamespaceClass) *vp = obj->getNamePrefix())
DEFINE_GETTER(NameURI_getter,
              if (obj->getClass() == &js_NamespaceClass) *vp = obj->getNameURI())

static void
namespace_finalize(JSContext *cx, JSObject *obj)
{
    if (obj->compartment()->functionNamespaceObject == obj)
        obj->compartment()->functionNamespaceObject = NULL;
}

static JSBool
namespace_equality(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    JSObject *obj2;

    JS_ASSERT(v->isObjectOrNull());
    obj2 = v->toObjectOrNull();
    *bp = (!obj2 || obj2->getClass() != &js_NamespaceClass)
          ? JS_FALSE
          : js_EqualStrings(GetURI(obj), GetURI(obj2));
    return JS_TRUE;
}

JS_FRIEND_DATA(Class) js_NamespaceClass = {
    "Namespace",
    JSCLASS_CONSTRUCT_PROTOTYPE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::NAMESPACE_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Namespace),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    namespace_finalize,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    NULL,           /* mark        */
    {
        namespace_equality,
        NULL,       /* outerObject    */
        NULL,       /* innerObject    */
        NULL,       /* iteratorObject */
        NULL,       /* wrappedObject  */
    }
};

#define NAMESPACE_ATTRS                                                       \
    (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED)

static JSPropertySpec namespace_props[] = {
    {js_prefix_str, 0, NAMESPACE_ATTRS, NamePrefix_getter, 0},
    {js_uri_str,    0, NAMESPACE_ATTRS, NameURI_getter,    0},
    {0,0,0,0,0}
};

static JSBool
namespace_toString(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;

    obj = ComputeThisFromVp(cx, vp);
    if (!JS_InstanceOf(cx, obj, Jsvalify(&js_NamespaceClass), Jsvalify(vp + 2)))
        return JS_FALSE;
    *vp = Valueify(obj->getNameURI());
    return JS_TRUE;
}

static JSFunctionSpec namespace_methods[] = {
    JS_FN(js_toString_str,  namespace_toString,        0,0),
    JS_FS_END
};

static JSObject *
NewXMLNamespace(JSContext *cx, JSString *prefix, JSString *uri, JSBool declared)
{
    JSObject *obj;

    obj = NewBuiltinClassInstanceXML(cx, &js_NamespaceClass);
    if (!obj)
        return JS_FALSE;
    JS_ASSERT(JSVAL_IS_VOID(obj->getNamePrefix()));
    JS_ASSERT(JSVAL_IS_VOID(obj->getNameURI()));
    JS_ASSERT(JSVAL_IS_VOID(obj->getNamespaceDeclared()));
    if (prefix)
        obj->setNamePrefix(STRING_TO_JSVAL(prefix));
    if (uri)
        obj->setNameURI(STRING_TO_JSVAL(uri));
    if (declared)
        obj->setNamespaceDeclared(JSVAL_TRUE);
    METER(xml_stats.xmlnamespace);
    return obj;
}

/*
 * QName class and library functions.
 */
DEFINE_GETTER(QNameNameURI_getter,
              if (obj->getClass() == &js_QNameClass)
                  *vp = JSVAL_IS_VOID(obj->getNameURI()) ? JSVAL_NULL : obj->getNameURI())
DEFINE_GETTER(QNameLocalName_getter,
              if (obj->getClass() == &js_QNameClass)
                  *vp = obj->getQNameLocalName())

static void
anyname_finalize(JSContext* cx, JSObject* obj)
{
    /* Make sure the next call to js_GetAnyName doesn't try to use obj. */
    if (obj->compartment()->anynameObject == obj)
        obj->compartment()->anynameObject = NULL;
}

static JSBool
qname_identity(JSObject *qna, JSObject *qnb)
{
    JSString *uri1 = GetURI(qna);
    JSString *uri2 = GetURI(qnb);

    if (!uri1 ^ !uri2)
        return JS_FALSE;
    if (uri1 && !js_EqualStrings(uri1, uri2))
        return JS_FALSE;
    return js_EqualStrings(GetLocalName(qna), GetLocalName(qnb));
}

static JSBool
qname_equality(JSContext *cx, JSObject *qn, const Value *v, JSBool *bp)
{
    JSObject *obj2;

    obj2 = v->toObjectOrNull();
    *bp = (!obj2 || obj2->getClass() != &js_QNameClass)
          ? JS_FALSE
          : qname_identity(qn, obj2);
    return JS_TRUE;
}

JS_FRIEND_DATA(Class) js_QNameClass = {
    "QName",
    JSCLASS_CONSTRUCT_PROTOTYPE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::QNAME_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_QName),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    FinalizeStub,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    NULL,           /* mark        */
    {
        qname_equality,
        NULL,       /* outerObject    */
        NULL,       /* innerObject    */
        NULL,       /* iteratorObject */
        NULL,       /* wrappedObject  */
    }
};

/*
 * Classes for the ECMA-357-internal types AttributeName and AnyName, which
 * are like QName, except that they have no property getters.  They share the
 * qname_toString method, and therefore are exposed as constructable objects
 * in this implementation.
 */
JS_FRIEND_DATA(Class) js_AttributeNameClass = {
    js_AttributeName_str,
    JSCLASS_CONSTRUCT_PROTOTYPE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::QNAME_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_AttributeName),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

JS_FRIEND_DATA(Class) js_AnyNameClass = {
    js_AnyName_str,
    JSCLASS_CONSTRUCT_PROTOTYPE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::QNAME_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_AnyName),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    anyname_finalize
};

#define QNAME_ATTRS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED)

static JSPropertySpec qname_props[] = {
    {js_uri_str,       0, QNAME_ATTRS, QNameNameURI_getter,   0},
    {js_localName_str, 0, QNAME_ATTRS, QNameLocalName_getter, 0},
    {0,0,0,0,0}
};

static JSBool
qname_toString(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    Class *clasp;
    JSString *uri, *str, *qualstr;
    size_t length;
    jschar *chars;

    obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return JS_FALSE;
    clasp = obj->getClass();
    if (clasp != &js_AttributeNameClass &&
        clasp != &js_AnyNameClass &&
        !JS_InstanceOf(cx, obj, Jsvalify(&js_QNameClass), Jsvalify(vp + 2))) {
            return JS_FALSE;
    }

    uri = GetURI(obj);
    if (!uri) {
        /* No uri means wildcard qualifier. */
        str = ATOM_TO_STRING(cx->runtime->atomState.starQualifierAtom);
    } else if (uri->empty()) {
        /* Empty string for uri means localName is in no namespace. */
        str = cx->runtime->emptyString;
    } else {
        qualstr = ATOM_TO_STRING(cx->runtime->atomState.qualifierAtom);
        str = js_ConcatStrings(cx, uri, qualstr);
        if (!str)
            return JS_FALSE;
    }
    str = js_ConcatStrings(cx, str, GetLocalName(obj));
    if (!str)
        return JS_FALSE;

    if (str && clasp == &js_AttributeNameClass) {
        length = str->length();
        chars = (jschar *) cx->malloc((length + 2) * sizeof(jschar));
        if (!chars)
            return JS_FALSE;
        *chars = '@';
        js_strncpy(chars + 1, str->chars(), length);
        chars[++length] = 0;
        str = js_NewString(cx, chars, length);
        if (!str) {
            cx->free(chars);
            return JS_FALSE;
        }
    }

    vp->setString(str);
    return JS_TRUE;
}

static JSFunctionSpec qname_methods[] = {
    JS_FN(js_toString_str,  qname_toString,    0,0),
    JS_FS_END
};


static void
InitXMLQName(JSObject *obj, JSString *uri, JSString *prefix,
             JSString *localName)
{
    JS_ASSERT(JSVAL_IS_VOID(obj->getNamePrefix()));
    JS_ASSERT(JSVAL_IS_VOID(obj->getNameURI()));
    JS_ASSERT(JSVAL_IS_VOID(obj->getQNameLocalName()));
    if (uri)
        obj->setNameURI(STRING_TO_JSVAL(uri));
    if (prefix)
        obj->setNamePrefix(STRING_TO_JSVAL(prefix));
    if (localName)
        obj->setQNameLocalName(STRING_TO_JSVAL(localName));
}

static JSObject *
NewXMLQName(JSContext *cx, JSString *uri, JSString *prefix, JSString *localName,
            Class *clasp = &js_QNameClass)
{
    JSObject *obj = NewBuiltinClassInstanceXML(cx, clasp);
    if (!obj)
        return NULL;
    JS_ASSERT(obj->isQName());
    InitXMLQName(obj, uri, prefix, localName);
    METER(xml_stats.qname);
    return obj;
}

JSObject *
js_ConstructXMLQNameObject(JSContext *cx, const Value &nsval, const Value &lnval)
{
    Value argv[2];

    /*
     * ECMA-357 11.1.2,
     * The _QualifiedIdentifier : PropertySelector :: PropertySelector_
     * production, step 2.
     */
    if (nsval.isObject() &&
        nsval.toObject().getClass() == &js_AnyNameClass) {
        argv[0].setNull();
    } else {
        argv[0] = nsval;
    }
    argv[1] = lnval;
    return js_ConstructObject(cx, &js_QNameClass, NULL, NULL, 2, argv);
}

static JSBool
IsXMLName(const jschar *cp, size_t n)
{
    JSBool rv;
    jschar c;

    rv = JS_FALSE;
    if (n != 0 && JS_ISXMLNSSTART(*cp)) {
        while (--n != 0) {
            c = *++cp;
            if (!JS_ISXMLNS(c))
                return rv;
        }
        rv = JS_TRUE;
    }
    return rv;
}

JSBool
js_IsXMLName(JSContext *cx, jsval v)
{
    JSString *name;
    JSErrorReporter older;

    /*
     * Inline specialization of the QName constructor called with v passed as
     * the only argument, to compute the localName for the constructed qname,
     * without actually allocating the object or computing its uri and prefix.
     * See ECMA-357 13.1.2.1 step 1 and 13.3.2.
     */
    if (!JSVAL_IS_PRIMITIVE(v) &&
        JSVAL_TO_OBJECT(v)->isQName()) {
        name = GetLocalName(JSVAL_TO_OBJECT(v));
    } else {
        older = JS_SetErrorReporter(cx, NULL);
        name = js_ValueToString(cx, Valueify(v));
        JS_SetErrorReporter(cx, older);
        if (!name) {
            JS_ClearPendingException(cx);
            return JS_FALSE;
        }
    }

    return IsXMLName(name->chars(), name->length());
}

/*
 * When argc is -1, it indicates argv is empty but the code should behave as
 * if argc is 1 and argv[0] is JSVAL_VOID.
 */
static JSBool
NamespaceHelper(JSContext *cx, JSObject *obj, intN argc, jsval *argv,
                jsval *rval)
{
    jsval urival, prefixval;
    JSObject *uriobj;
    JSBool isNamespace, isQName;
    Class *clasp;
    JSString *empty, *uri, *prefix;

    isNamespace = isQName = JS_FALSE;
#ifdef __GNUC__         /* suppress bogus gcc warnings */
    uriobj = NULL;
#endif
    if (argc <= 0) {
        urival = JSVAL_VOID;
    } else {
        urival = argv[argc > 1];
        if (!JSVAL_IS_PRIMITIVE(urival)) {
            uriobj = JSVAL_TO_OBJECT(urival);
            clasp = uriobj->getClass();
            isNamespace = (clasp == &js_NamespaceClass);
            isQName = (clasp == &js_QNameClass);
        }
    }

    if (!obj) {
        /* Namespace called as function. */
        if (argc == 1 && isNamespace) {
            /* Namespace called with one Namespace argument is identity. */
            *rval = urival;
            return JS_TRUE;
        }

        obj = NewBuiltinClassInstanceXML(cx, &js_NamespaceClass);
        if (!obj)
            return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(obj);
    METER(xml_stats.xmlnamespace);

    empty = cx->runtime->emptyString;
    obj->setNamePrefix(STRING_TO_JSVAL(empty));
    obj->setNameURI(STRING_TO_JSVAL(empty));

    if (argc == 1 || argc == -1) {
        if (isNamespace) {
            obj->setNameURI(uriobj->getNameURI());
            obj->setNamePrefix(uriobj->getNamePrefix());
        } else if (isQName && (uri = GetURI(uriobj))) {
            obj->setNameURI(STRING_TO_JSVAL(uri));
            obj->setNamePrefix(uriobj->getNamePrefix());
        } else {
            uri = js_ValueToString(cx, Valueify(urival));
            if (!uri)
                return JS_FALSE;
            obj->setNameURI(STRING_TO_JSVAL(uri));
            if (!uri->empty())
                obj->setNamePrefix(JSVAL_VOID);
        }
    } else if (argc == 2) {
        if (!isQName || !(uri = GetURI(uriobj))) {
            uri = js_ValueToString(cx, Valueify(urival));
            if (!uri)
                return JS_FALSE;
        }
        obj->setNameURI(STRING_TO_JSVAL(uri));

        prefixval = argv[0];
        if (uri->empty()) {
            if (!JSVAL_IS_VOID(prefixval)) {
                prefix = js_ValueToString(cx, Valueify(prefixval));
                if (!prefix)
                    return JS_FALSE;
                if (!prefix->empty()) {
                    Value v = StringValue(prefix);
                    JSAutoByteString bytes;
                    if (js_ValueToPrintable(cx, v, &bytes)) {
                        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                             JSMSG_BAD_XML_NAMESPACE, bytes.ptr());
                    }
                    return JS_FALSE;
                }
            }
        } else if (JSVAL_IS_VOID(prefixval) || !js_IsXMLName(cx, prefixval)) {
            obj->setNamePrefix(JSVAL_VOID);
        } else {
            prefix = js_ValueToString(cx, Valueify(prefixval));
            if (!prefix)
                return JS_FALSE;
            obj->setNamePrefix(STRING_TO_JSVAL(prefix));
        }
    }
    return JS_TRUE;
}

static JSBool
Namespace(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *thisobj = NULL;
    (void)IsConstructing_PossiblyWithGivenThisObject(vp, &thisobj);
    return NamespaceHelper(cx, thisobj, argc, Jsvalify(vp + 2), Jsvalify(vp));
}

/*
 * When argc is -1, it indicates argv is empty but the code should behave as
 * if argc is 1 and argv[0] is JSVAL_VOID.
 */
static JSBool
QNameHelper(JSContext *cx, JSObject *obj, Class *clasp, intN argc,
            jsval *argv, jsval *rval)
{
    jsval nameval, nsval;
    JSBool isQName, isNamespace;
    JSObject *qn;
    JSString *uri, *prefix, *name;
    JSObject *obj2;

    JS_ASSERT(clasp == &js_QNameClass ||
              clasp == &js_AttributeNameClass);
    if (argc <= 0) {
        nameval = JSVAL_VOID;
        isQName = JS_FALSE;
    } else {
        nameval = argv[argc > 1];
        isQName =
            !JSVAL_IS_PRIMITIVE(nameval) &&
            JSVAL_TO_OBJECT(nameval)->getClass() == &js_QNameClass;
    }

    if (!obj) {
        /* QName called as function. */
        if (argc == 1 && isQName) {
            /* QName called with one QName argument is identity. */
            *rval = nameval;
            return JS_TRUE;
        }

        /*
         * Create and return a new QName or AttributeName object exactly as if
         * constructed.
         */
        obj = NewBuiltinClassInstanceXML(cx, clasp);
        if (!obj)
            return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(obj);
    METER(xml_stats.qname);

    if (isQName) {
        /* If namespace is not specified and name is a QName, clone it. */
        qn = JSVAL_TO_OBJECT(nameval);
        if (argc == 1) {
            uri = GetURI(qn);
            prefix = GetPrefix(qn);
            name = GetLocalName(qn);
            goto out;
        }

        /* Namespace and qname were passed -- use the qname's localName. */
        nameval = qn->getQNameLocalName();
    }

    if (argc == 0) {
        name = cx->runtime->emptyString;
    } else if (argc < 0) {
        name = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    } else {
        name = js_ValueToString(cx, Valueify(nameval));
        if (!name)
            return JS_FALSE;
        argv[argc > 1] = STRING_TO_JSVAL(name);
    }

    if (argc > 1 && !JSVAL_IS_VOID(argv[0])) {
        nsval = argv[0];
    } else if (IS_STAR(name)) {
        nsval = JSVAL_NULL;
    } else {
        if (!js_GetDefaultXMLNamespace(cx, &nsval))
            return JS_FALSE;
        JS_ASSERT(!JSVAL_IS_PRIMITIVE(nsval));
        JS_ASSERT(JSVAL_TO_OBJECT(nsval)->getClass() ==
                  &js_NamespaceClass);
    }

    if (JSVAL_IS_NULL(nsval)) {
        /* NULL prefix represents *undefined* in ECMA-357 13.3.2 5(a). */
        uri = prefix = NULL;
    } else {
        /*
         * Inline specialization of the Namespace constructor called with
         * nsval passed as the only argument, to compute the uri and prefix
         * for the constructed namespace, without actually allocating the
         * object or computing other members.  See ECMA-357 13.3.2 6(a) and
         * 13.2.2.
         */
        isNamespace = isQName = JS_FALSE;
        if (!JSVAL_IS_PRIMITIVE(nsval)) {
            obj2 = JSVAL_TO_OBJECT(nsval);
            clasp = obj2->getClass();
            isNamespace = (clasp == &js_NamespaceClass);
            isQName = (clasp == &js_QNameClass);
        }
#ifdef __GNUC__         /* suppress bogus gcc warnings */
        else obj2 = NULL;
#endif

        if (isNamespace) {
            uri = GetURI(obj2);
            prefix = GetPrefix(obj2);
        } else if (isQName && (uri = GetURI(obj2))) {
            JS_ASSERT(argc > 1);
            prefix = GetPrefix(obj2);
        } else {
            JS_ASSERT(argc > 1);
            uri = js_ValueToString(cx, Valueify(nsval));
            if (!uri)
                return JS_FALSE;
            argv[0] = STRING_TO_JSVAL(uri);     /* local root */

            /* NULL here represents *undefined* in ECMA-357 13.2.2 3(c)iii. */
            prefix = uri->empty() ? cx->runtime->emptyString : NULL;
        }
    }

out:
    InitXMLQName(obj, uri, prefix, name);
    return JS_TRUE;
}

static JSBool
QName(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *thisobj = NULL;
    (void)IsConstructing_PossiblyWithGivenThisObject(vp, &thisobj);
    return QNameHelper(cx, thisobj, &js_QNameClass, argc, Jsvalify(vp + 2), Jsvalify(vp));
}

static JSBool
AttributeName(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *thisobj = NULL;
    (void)IsConstructing_PossiblyWithGivenThisObject(vp, &thisobj);
    return QNameHelper(cx, thisobj, &js_AttributeNameClass, argc, Jsvalify(vp + 2), Jsvalify(vp));
}

/*
 * XMLArray library functions.
 */
static JSBool
namespace_identity(const void *a, const void *b)
{
    const JSObject *nsa = (const JSObject *) a;
    const JSObject *nsb = (const JSObject *) b;
    JSString *prefixa = GetPrefix(nsa);
    JSString *prefixb = GetPrefix(nsb);

    if (prefixa && prefixb) {
        if (!js_EqualStrings(prefixa, prefixb))
            return JS_FALSE;
    } else {
        if (prefixa || prefixb)
            return JS_FALSE;
    }
    return js_EqualStrings(GetURI(nsa), GetURI(nsb));
}

static JSBool
attr_identity(const void *a, const void *b)
{
    const JSXML *xmla = (const JSXML *) a;
    const JSXML *xmlb = (const JSXML *) b;

    return qname_identity(xmla->name, xmlb->name);
}

void
JSXMLArrayCursor::trace(JSTracer *trc) {
#ifdef DEBUG
    size_t index = 0;
#endif
    for (JSXMLArrayCursor *cursor = this; cursor; cursor = cursor->next)
        js::gc::MarkGCThing(trc, cursor->root, "cursor_root", index++);
}

static void
XMLArrayCursorTrace(JSTracer *trc, JSXMLArrayCursor *cursor)
{
    cursor->trace(trc);
}

/* NB: called with null cx from the GC, via xml_trace => JSXMLArray::trim. */
bool
JSXMLArray::setCapacity(JSContext *cx, uint32 newCapacity)
{
    if (newCapacity == 0) {
        /* We could let realloc(p, 0) free this, but purify gets confused. */
        if (vector) {
            if (cx)
                cx->free(vector);
            else
                js_free(vector);
        }
        vector = NULL;
    } else {
        void **tmp;

        if (
#if JS_BITS_PER_WORD == 32
            (size_t)newCapacity > ~(size_t)0 / sizeof(void *) ||
#endif
            !(tmp = (void **) js_realloc(vector, newCapacity * sizeof(void *)))) {
            if (cx)
                JS_ReportOutOfMemory(cx);
            return false;
        }
        vector = tmp;
    }
    capacity = JSXML_PRESET_CAPACITY | newCapacity;
    return true;
}

void
JSXMLArray::trim()
{
    if (capacity & JSXML_PRESET_CAPACITY)
        return;
    if (length < capacity)
        setCapacity(NULL, length);
}

void
JSXMLArray::finish(JSContext *cx)
{
    cx->free(vector);

    while (JSXMLArrayCursor *cursor = cursors)
        cursor->disconnect();

#ifdef DEBUG
    memset(this, 0xd5, sizeof *this);
#endif
}

#define XML_NOT_FOUND   ((uint32) -1)

static uint32
XMLArrayFindMember(const JSXMLArray *array, void *elt, JSIdentityOp identity)
{
    void **vector;
    uint32 i, n;

    /* The identity op must not reallocate array->vector. */
    vector = array->vector;
    if (identity) {
        for (i = 0, n = array->length; i < n; i++) {
            if (identity(vector[i], elt))
                return i;
        }
    } else {
        for (i = 0, n = array->length; i < n; i++) {
            if (vector[i] == elt)
                return i;
        }
    }
    return XML_NOT_FOUND;
}

/*
 * Grow array vector capacity by powers of two to LINEAR_THRESHOLD, and after
 * that, grow by LINEAR_INCREMENT.  Both must be powers of two, and threshold
 * should be greater than increment.
 */
#define LINEAR_THRESHOLD        256
#define LINEAR_INCREMENT        32

static JSBool
XMLArrayAddMember(JSContext *cx, JSXMLArray *array, uint32 index, void *elt)
{
    uint32 capacity, i;
    int log2;
    void **vector;

    if (index >= array->length) {
        if (index >= JSXML_CAPACITY(array)) {
            /* Arrange to clear JSXML_PRESET_CAPACITY from array->capacity. */
            capacity = index + 1;
            if (index >= LINEAR_THRESHOLD) {
                capacity = JS_ROUNDUP(capacity, LINEAR_INCREMENT);
            } else {
                JS_CEILING_LOG2(log2, capacity);
                capacity = JS_BIT(log2);
            }
            if (
#if JS_BITS_PER_WORD == 32
                (size_t)capacity > ~(size_t)0 / sizeof(void *) ||
#endif
                !(vector = (void **)
                           js_realloc(array->vector, capacity * sizeof(void *)))) {
                JS_ReportOutOfMemory(cx);
                return JS_FALSE;
            }
            array->capacity = capacity;
            array->vector = vector;
            for (i = array->length; i < index; i++)
                vector[i] = NULL;
        }
        array->length = index + 1;
    }

    array->vector[index] = elt;
    return JS_TRUE;
}

static JSBool
XMLArrayInsert(JSContext *cx, JSXMLArray *array, uint32 i, uint32 n)
{
    uint32 j;
    JSXMLArrayCursor *cursor;

    j = array->length;
    JS_ASSERT(i <= j);
    if (!array->setCapacity(cx, j + n))
        return JS_FALSE;

    array->length = j + n;
    JS_ASSERT(n != (uint32)-1);
    while (j != i) {
        --j;
        array->vector[j + n] = array->vector[j];
    }

    for (cursor = array->cursors; cursor; cursor = cursor->next) {
        if (cursor->index > i)
            cursor->index += n;
    }
    return JS_TRUE;
}

static void *
XMLArrayDelete(JSContext *cx, JSXMLArray *array, uint32 index, JSBool compress)
{
    uint32 length;
    void **vector, *elt;
    JSXMLArrayCursor *cursor;

    length = array->length;
    if (index >= length)
        return NULL;

    vector = array->vector;
    elt = vector[index];
    if (compress) {
        while (++index < length)
            vector[index-1] = vector[index];
        array->length = length - 1;
        array->capacity = JSXML_CAPACITY(array);
    } else {
        vector[index] = NULL;
    }

    for (cursor = array->cursors; cursor; cursor = cursor->next) {
        if (cursor->index > index)
            --cursor->index;
    }
    return elt;
}

static void
XMLArrayTruncate(JSContext *cx, JSXMLArray *array, uint32 length)
{
    void **vector;

    JS_ASSERT(!array->cursors);
    if (length >= array->length)
        return;

    if (length == 0) {
        if (array->vector)
            cx->free(array->vector);
        vector = NULL;
    } else {
        vector = (void **) js_realloc(array->vector, length * sizeof(void *));
        if (!vector)
            return;
    }

    if (array->length > length)
        array->length = length;
    array->capacity = length;
    array->vector = vector;
}

#define XMLARRAY_FIND_MEMBER(a,e,f) XMLArrayFindMember(a, (void *)(e), f)
#define XMLARRAY_HAS_MEMBER(a,e,f)  (XMLArrayFindMember(a, (void *)(e), f) != \
                                     XML_NOT_FOUND)
#define XMLARRAY_MEMBER(a,i,t)      (((i) < (a)->length)                      \
                                     ? (t *) (a)->vector[i]                   \
                                     : NULL)
#define XMLARRAY_SET_MEMBER(a,i,e)  JS_BEGIN_MACRO                            \
                                        if ((a)->length <= (i))               \
                                            (a)->length = (i) + 1;            \
                                        ((a)->vector[i] = (void *)(e));       \
                                    JS_END_MACRO
#define XMLARRAY_ADD_MEMBER(x,a,i,e)XMLArrayAddMember(x, a, i, (void *)(e))
#define XMLARRAY_INSERT(x,a,i,n)    XMLArrayInsert(x, a, i, n)
#define XMLARRAY_APPEND(x,a,e)      XMLARRAY_ADD_MEMBER(x, a, (a)->length, (e))
#define XMLARRAY_DELETE(x,a,i,c,t)  ((t *) XMLArrayDelete(x, a, i, c))
#define XMLARRAY_TRUNCATE(x,a,n)    XMLArrayTruncate(x, a, n)

/*
 * Define XML setting property strings and constants early, so everyone can
 * use the same names.
 */
static const char js_ignoreComments_str[]   = "ignoreComments";
static const char js_ignoreProcessingInstructions_str[]
                                            = "ignoreProcessingInstructions";
static const char js_ignoreWhitespace_str[] = "ignoreWhitespace";
static const char js_prettyPrinting_str[]   = "prettyPrinting";
static const char js_prettyIndent_str[]     = "prettyIndent";

#define XSF_IGNORE_COMMENTS                JS_BIT(0)
#define XSF_IGNORE_PROCESSING_INSTRUCTIONS JS_BIT(1)
#define XSF_IGNORE_WHITESPACE              JS_BIT(2)
#define XSF_PRETTY_PRINTING                JS_BIT(3)

static JSPropertySpec xml_static_props[] = {
    {js_ignoreComments_str, 0, JSPROP_PERMANENT, NULL, NULL},
    {js_ignoreProcessingInstructions_str, 0, JSPROP_PERMANENT, NULL, NULL},
    {js_ignoreWhitespace_str, 0, JSPROP_PERMANENT, NULL, NULL},
    {js_prettyPrinting_str, 0, JSPROP_PERMANENT, NULL, NULL},
    {js_prettyIndent_str, 0, JSPROP_PERMANENT, NULL, NULL},
    {0,0,0,0,0}
};

/* Macros for special-casing xml:, xmlns= and xmlns:foo= in ParseNodeToQName. */
#define IS_XML(str)                                                           \
    (str->length() == 3 && IS_XML_CHARS(str->chars()))

#define IS_XMLNS(str)                                                         \
    (str->length() == 5 && IS_XMLNS_CHARS(str->chars()))

#define IS_XML_CHARS(chars)                                                   \
    (JS_TOLOWER((chars)[0]) == 'x' &&                                         \
     JS_TOLOWER((chars)[1]) == 'm' &&                                         \
     JS_TOLOWER((chars)[2]) == 'l')

#define HAS_NS_AFTER_XML(chars)                                               \
    (JS_TOLOWER((chars)[3]) == 'n' &&                                         \
     JS_TOLOWER((chars)[4]) == 's')

#define IS_XMLNS_CHARS(chars)                                                 \
    (IS_XML_CHARS(chars) && HAS_NS_AFTER_XML(chars))

#define STARTS_WITH_XML(chars,length)                                         \
    (length >= 3 && IS_XML_CHARS(chars))

static const char xml_namespace_str[] = "http://www.w3.org/XML/1998/namespace";
static const char xmlns_namespace_str[] = "http://www.w3.org/2000/xmlns/";

static JSObject *
ParseNodeToQName(Parser *parser, JSParseNode *pn,
                 JSXMLArray *inScopeNSes, JSBool isAttributeName)
{
    JSContext *cx = parser->context;
    JSString *str, *uri, *prefix, *localName;
    size_t length, offset;
    const jschar *start, *limit, *colon;
    uint32 n;
    JSObject *ns;
    JSString *nsprefix;

    JS_ASSERT(pn->pn_arity == PN_NULLARY);
    str = ATOM_TO_STRING(pn->pn_atom);
    str->getCharsAndLength(start, length);
    JS_ASSERT(length != 0 && *start != '@');
    JS_ASSERT(length != 1 || *start != '*');

    uri = cx->runtime->emptyString;
    limit = start + length;
    colon = js_strchr_limit(start, ':', limit);
    if (colon) {
        offset = colon - start;
        prefix = js_NewDependentString(cx, str, 0, offset);
        if (!prefix)
            return NULL;

        if (STARTS_WITH_XML(start, offset)) {
            if (offset == 3) {
                uri = JS_InternString(cx, xml_namespace_str);
                if (!uri)
                    return NULL;
            } else if (offset == 5 && HAS_NS_AFTER_XML(start)) {
                uri = JS_InternString(cx, xmlns_namespace_str);
                if (!uri)
                    return NULL;
            } else {
                uri = NULL;
            }
        } else {
            uri = NULL;
            n = inScopeNSes->length;
            while (n != 0) {
                --n;
                ns = XMLARRAY_MEMBER(inScopeNSes, n, JSObject);
                nsprefix = GetPrefix(ns);
                if (nsprefix && js_EqualStrings(nsprefix, prefix)) {
                    uri = GetURI(ns);
                    break;
                }
            }
        }

        if (!uri) {
            Value v = StringValue(prefix);
            JSAutoByteString bytes;
            if (js_ValueToPrintable(parser->context, v, &bytes)) {
                ReportCompileErrorNumber(parser->context, &parser->tokenStream, pn,
                                         JSREPORT_ERROR, JSMSG_BAD_XML_NAMESPACE, bytes.ptr());
            }
            return NULL;
        }

        localName = js_NewStringCopyN(parser->context, colon + 1, length - (offset + 1));
        if (!localName)
            return NULL;
    } else {
        if (isAttributeName) {
            /*
             * An unprefixed attribute is not in any namespace, so set prefix
             * as well as uri to the empty string.
             */
            prefix = uri;
        } else {
            /*
             * Loop from back to front looking for the closest declared default
             * namespace.
             */
            n = inScopeNSes->length;
            while (n != 0) {
                --n;
                ns = XMLARRAY_MEMBER(inScopeNSes, n, JSObject);
                nsprefix = GetPrefix(ns);
                if (!nsprefix || nsprefix->empty()) {
                    uri = GetURI(ns);
                    break;
                }
            }
            prefix = uri->empty() ? parser->context->runtime->emptyString : NULL;
        }
        localName = str;
    }

    return NewXMLQName(parser->context, uri, prefix, localName);
}

static JSString *
ChompXMLWhitespace(JSContext *cx, JSString *str)
{
    size_t length, newlength, offset;
    const jschar *cp, *start, *end;
    jschar c;

    str->getCharsAndLength(start, length);
    for (cp = start, end = cp + length; cp < end; cp++) {
        c = *cp;
        if (!JS_ISXMLSPACE(c))
            break;
    }
    while (end > cp) {
        c = end[-1];
        if (!JS_ISXMLSPACE(c))
            break;
        --end;
    }
    newlength = end - cp;
    if (newlength == length)
        return str;
    offset = cp - start;
    return js_NewDependentString(cx, str, offset, newlength);
}

static JSXML *
ParseNodeToXML(Parser *parser, JSParseNode *pn,
               JSXMLArray *inScopeNSes, uintN flags)
{
    JSContext *cx = parser->context;
    JSXML *xml, *kid, *attr, *attrj;
    JSString *str;
    uint32 length, n, i, j;
    JSParseNode *pn2, *pn3, *head, **pnp;
    JSObject *ns;
    JSObject *qn, *attrjqn;
    JSXMLClass xml_class;
    int stackDummy;

    if (!JS_CHECK_STACK_SIZE(cx->stackLimit, &stackDummy)) {
        ReportCompileErrorNumber(cx, &parser->tokenStream, pn, JSREPORT_ERROR,
                                 JSMSG_OVER_RECURSED);
        return NULL;
    }

#define PN2X_SKIP_CHILD ((JSXML *) 1)

    /*
     * Cases return early to avoid common code that gets an outermost xml's
     * object, which protects GC-things owned by xml and its descendants from
     * garbage collection.
     */
    xml = NULL;
    if (!js_EnterLocalRootScope(cx))
        return NULL;
    switch (pn->pn_type) {
      case TOK_XMLELEM:
        length = inScopeNSes->length;
        pn2 = pn->pn_head;
        xml = ParseNodeToXML(parser, pn2, inScopeNSes, flags);
        if (!xml)
            goto fail;

        n = pn->pn_count;
        JS_ASSERT(n >= 2);
        n -= 2;
        if (!xml->xml_kids.setCapacity(cx, n))
            goto fail;

        i = 0;
        while ((pn2 = pn2->pn_next) != NULL) {
            if (!pn2->pn_next) {
                /* Don't append the end tag! */
                JS_ASSERT(pn2->pn_type == TOK_XMLETAGO);
                break;
            }

            if ((flags & XSF_IGNORE_WHITESPACE) &&
                n > 1 && pn2->pn_type == TOK_XMLSPACE) {
                --n;
                continue;
            }

            kid = ParseNodeToXML(parser, pn2, inScopeNSes, flags);
            if (kid == PN2X_SKIP_CHILD) {
                --n;
                continue;
            }

            if (!kid)
                goto fail;

            /* Store kid in xml right away, to protect it from GC. */
            XMLARRAY_SET_MEMBER(&xml->xml_kids, i, kid);
            kid->parent = xml;
            ++i;

            /* XXX where is this documented in an XML spec, or in E4X? */
            if ((flags & XSF_IGNORE_WHITESPACE) &&
                n > 1 && kid->xml_class == JSXML_CLASS_TEXT) {
                str = ChompXMLWhitespace(cx, kid->xml_value);
                if (!str)
                    goto fail;
                kid->xml_value = str;
            }
        }

        JS_ASSERT(i == n);
        if (n < pn->pn_count - 2)
            xml->xml_kids.trim();
        XMLARRAY_TRUNCATE(cx, inScopeNSes, length);
        break;

      case TOK_XMLLIST:
        xml = js_NewXML(cx, JSXML_CLASS_LIST);
        if (!xml)
            goto fail;

        n = pn->pn_count;
        if (!xml->xml_kids.setCapacity(cx, n))
            goto fail;

        i = 0;
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            /*
             * Always ignore insignificant whitespace in lists -- we shouldn't
             * condition this on an XML.ignoreWhitespace setting when the list
             * constructor is XMLList (note XML/XMLList unification hazard).
             */
            if (pn2->pn_type == TOK_XMLSPACE) {
                --n;
                continue;
            }

            kid = ParseNodeToXML(parser, pn2, inScopeNSes, flags);
            if (kid == PN2X_SKIP_CHILD) {
                --n;
                continue;
            }

            if (!kid)
                goto fail;

            XMLARRAY_SET_MEMBER(&xml->xml_kids, i, kid);
            ++i;
        }

        if (n < pn->pn_count)
            xml->xml_kids.trim();
        break;

      case TOK_XMLSTAGO:
      case TOK_XMLPTAGC:
        length = inScopeNSes->length;
        pn2 = pn->pn_head;
        JS_ASSERT(pn2->pn_type == TOK_XMLNAME);
        if (pn2->pn_arity == PN_LIST)
            goto syntax;

        xml = js_NewXML(cx, JSXML_CLASS_ELEMENT);
        if (!xml)
            goto fail;

        /* First pass: check syntax and process namespace declarations. */
        JS_ASSERT(pn->pn_count >= 1);
        n = pn->pn_count - 1;
        pnp = &pn2->pn_next;
        head = *pnp;
        while ((pn2 = *pnp) != NULL) {
            size_t length;
            const jschar *chars;

            if (pn2->pn_type != TOK_XMLNAME || pn2->pn_arity != PN_NULLARY)
                goto syntax;

            /* Enforce "Well-formedness constraint: Unique Att Spec". */
            for (pn3 = head; pn3 != pn2; pn3 = pn3->pn_next->pn_next) {
                if (pn3->pn_atom == pn2->pn_atom) {
                    Value v = StringValue(ATOM_TO_STRING(pn2->pn_atom));
                    JSAutoByteString bytes;
                    if (js_ValueToPrintable(cx, v, &bytes)) { 
                        ReportCompileErrorNumber(cx, &parser->tokenStream, pn2,
                                                 JSREPORT_ERROR, JSMSG_DUPLICATE_XML_ATTR,
                                                 bytes.ptr());
                    }
                    goto fail;
                }
            }

            str = ATOM_TO_STRING(pn2->pn_atom);
            pn2 = pn2->pn_next;
            JS_ASSERT(pn2);
            if (pn2->pn_type != TOK_XMLATTR)
                goto syntax;

            str->getCharsAndLength(chars, length);
            if (length >= 5 &&
                IS_XMLNS_CHARS(chars) &&
                (length == 5 || chars[5] == ':')) {
                JSString *uri, *prefix;

                uri = ATOM_TO_STRING(pn2->pn_atom);
                if (length == 5) {
                    /* 10.3.2.1. Step 6(h)(i)(1)(a). */
                    prefix = cx->runtime->emptyString;
                } else {
                    prefix = js_NewStringCopyN(cx, chars + 6, length - 6);
                    if (!prefix)
                        goto fail;
                }

                /*
                 * Once the new ns is appended to xml->xml_namespaces, it is
                 * protected from GC by the object that owns xml -- which is
                 * either xml->object if outermost, or the object owning xml's
                 * oldest ancestor if !outermost.
                 */
                ns = NewXMLNamespace(cx, prefix, uri, JS_TRUE);
                if (!ns)
                    goto fail;

                /*
                 * Don't add a namespace that's already in scope.  If someone
                 * extracts a child property from its parent via [[Get]], then
                 * we enforce the invariant, noted many times in ECMA-357, that
                 * the child's namespaces form a possibly-improper superset of
                 * its ancestors' namespaces.
                 */
                if (!XMLARRAY_HAS_MEMBER(inScopeNSes, ns, namespace_identity)) {
                    if (!XMLARRAY_APPEND(cx, inScopeNSes, ns) ||
                        !XMLARRAY_APPEND(cx, &xml->xml_namespaces, ns)) {
                        goto fail;
                    }
                }

                JS_ASSERT(n >= 2);
                n -= 2;
                *pnp = pn2->pn_next;
                /* XXXbe recycle pn2 */
                continue;
            }

            pnp = &pn2->pn_next;
        }

        xml->xml_namespaces.trim();

        /* Second pass: process tag name and attributes, using namespaces. */
        pn2 = pn->pn_head;
        qn = ParseNodeToQName(parser, pn2, inScopeNSes, JS_FALSE);
        if (!qn)
            goto fail;
        xml->name = qn;

        JS_ASSERT((n & 1) == 0);
        n >>= 1;
        if (!xml->xml_attrs.setCapacity(cx, n))
            goto fail;

        for (i = 0; (pn2 = pn2->pn_next) != NULL; i++) {
            qn = ParseNodeToQName(parser, pn2, inScopeNSes, JS_TRUE);
            if (!qn) {
                xml->xml_attrs.length = i;
                goto fail;
            }

            /*
             * Enforce "Well-formedness constraint: Unique Att Spec", part 2:
             * this time checking local name and namespace URI.
             */
            for (j = 0; j < i; j++) {
                attrj = XMLARRAY_MEMBER(&xml->xml_attrs, j, JSXML);
                attrjqn = attrj->name;
                if (js_EqualStrings(GetURI(attrjqn), GetURI(qn)) &&
                    js_EqualStrings(GetLocalName(attrjqn), GetLocalName(qn))) {
                    Value v = StringValue(ATOM_TO_STRING(pn2->pn_atom));
                    JSAutoByteString bytes;
                    if (js_ValueToPrintable(cx, v, &bytes)) {
                        ReportCompileErrorNumber(cx, &parser->tokenStream, pn2,
                                                 JSREPORT_ERROR, JSMSG_DUPLICATE_XML_ATTR,
                                                 bytes.ptr());
                    }
                    goto fail;
                }
            }

            pn2 = pn2->pn_next;
            JS_ASSERT(pn2);
            JS_ASSERT(pn2->pn_type == TOK_XMLATTR);

            attr = js_NewXML(cx, JSXML_CLASS_ATTRIBUTE);
            if (!attr)
                goto fail;

            XMLARRAY_SET_MEMBER(&xml->xml_attrs, i, attr);
            attr->parent = xml;
            attr->name = qn;
            attr->xml_value = ATOM_TO_STRING(pn2->pn_atom);
        }

        /* Point tag closes its own namespace scope. */
        if (pn->pn_type == TOK_XMLPTAGC)
            XMLARRAY_TRUNCATE(cx, inScopeNSes, length);
        break;

      case TOK_XMLSPACE:
      case TOK_XMLTEXT:
      case TOK_XMLCDATA:
      case TOK_XMLCOMMENT:
      case TOK_XMLPI:
        str = ATOM_TO_STRING(pn->pn_atom);
        qn = NULL;
        if (pn->pn_type == TOK_XMLCOMMENT) {
            if (flags & XSF_IGNORE_COMMENTS)
                goto skip_child;
            xml_class = JSXML_CLASS_COMMENT;
        } else if (pn->pn_type == TOK_XMLPI) {
            if (IS_XML(str)) {
                Value v = StringValue(str);
                JSAutoByteString bytes;
                if (js_ValueToPrintable(cx, v, &bytes)) { 
                    ReportCompileErrorNumber(cx, &parser->tokenStream, pn,
                                             JSREPORT_ERROR, JSMSG_RESERVED_ID, bytes.ptr());
                }
                goto fail;
            }

            if (flags & XSF_IGNORE_PROCESSING_INSTRUCTIONS)
                goto skip_child;

            qn = ParseNodeToQName(parser, pn, inScopeNSes, JS_FALSE);
            if (!qn)
                goto fail;

            str = pn->pn_atom2
                  ? ATOM_TO_STRING(pn->pn_atom2)
                  : cx->runtime->emptyString;
            xml_class = JSXML_CLASS_PROCESSING_INSTRUCTION;
        } else {
            /* CDATA section content, or element text. */
            xml_class = JSXML_CLASS_TEXT;
        }

        xml = js_NewXML(cx, xml_class);
        if (!xml)
            goto fail;
        xml->name = qn;
        if (pn->pn_type == TOK_XMLSPACE)
            xml->xml_flags |= XMLF_WHITESPACE_TEXT;
        xml->xml_value = str;
        break;

      default:
        goto syntax;
    }

    js_LeaveLocalRootScopeWithResult(cx, xml);
    return xml;

skip_child:
    js_LeaveLocalRootScope(cx);
    return PN2X_SKIP_CHILD;

#undef PN2X_SKIP_CHILD

syntax:
    ReportCompileErrorNumber(cx, &parser->tokenStream, pn, JSREPORT_ERROR, JSMSG_BAD_XML_MARKUP);
fail:
    js_LeaveLocalRootScope(cx);
    return NULL;
}

/*
 * XML helper, object-ops, and library functions.  We start with the helpers,
 * in ECMA-357 order, but merging XML (9.1) and XMLList (9.2) helpers.
 */
static JSBool
GetXMLSetting(JSContext *cx, const char *name, jsval *vp)
{
    jsval v;

    if (!js_FindClassObject(cx, NULL, JSProto_XML, Valueify(&v)))
        return JS_FALSE;
    if (!VALUE_IS_FUNCTION(cx, v)) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }
    return JS_GetProperty(cx, JSVAL_TO_OBJECT(v), name, vp);
}

static JSBool
GetBooleanXMLSetting(JSContext *cx, const char *name, JSBool *bp)
{
    jsval v;

    return GetXMLSetting(cx, name, &v) && JS_ValueToBoolean(cx, v, bp);
}

static JSBool
GetUint32XMLSetting(JSContext *cx, const char *name, uint32 *uip)
{
    jsval v;

    return GetXMLSetting(cx, name, &v) && JS_ValueToECMAUint32(cx, v, uip);
}

static JSBool
GetXMLSettingFlags(JSContext *cx, uintN *flagsp)
{
    JSBool flag[4];

    if (!GetBooleanXMLSetting(cx, js_ignoreComments_str, &flag[0]) ||
        !GetBooleanXMLSetting(cx, js_ignoreProcessingInstructions_str, &flag[1]) ||
        !GetBooleanXMLSetting(cx, js_ignoreWhitespace_str, &flag[2]) ||
        !GetBooleanXMLSetting(cx, js_prettyPrinting_str, &flag[3])) {
        return false;
    }

    *flagsp = 0;
    for (size_t n = 0; n < 4; ++n)
        if (flag[n])
            *flagsp |= JS_BIT(n);
    return true;
}

static JSXML *
ParseXMLSource(JSContext *cx, JSString *src)
{
    jsval nsval;
    JSString *uri;
    size_t urilen, srclen, length, offset, dstlen;
    jschar *chars;
    const jschar *srcp, *endp;
    JSXML *xml;
    const char *filename;
    uintN lineno;
    JSOp op;

    static const char prefix[] = "<parent xmlns=\"";
    static const char middle[] = "\">";
    static const char suffix[] = "</parent>";

#define constrlen(constr)   (sizeof(constr) - 1)

    if (!js_GetDefaultXMLNamespace(cx, &nsval))
        return NULL;
    uri = GetURI(JSVAL_TO_OBJECT(nsval));
    uri = js_EscapeAttributeValue(cx, uri, JS_FALSE);
    if (!uri)
        return NULL;

    urilen = uri->length();
    srclen = src->length();
    length = constrlen(prefix) + urilen + constrlen(middle) + srclen +
             constrlen(suffix);

    chars = (jschar *) cx->malloc((length + 1) * sizeof(jschar));
    if (!chars)
        return NULL;

    dstlen = length;
    js_InflateStringToBuffer(cx, prefix, constrlen(prefix), chars, &dstlen);
    offset = dstlen;
    js_strncpy(chars + offset, uri->chars(), urilen);
    offset += urilen;
    dstlen = length - offset + 1;
    js_InflateStringToBuffer(cx, middle, constrlen(middle), chars + offset,
                             &dstlen);
    offset += dstlen;
    srcp = src->chars();
    js_strncpy(chars + offset, srcp, srclen);
    offset += srclen;
    dstlen = length - offset + 1;
    js_InflateStringToBuffer(cx, suffix, constrlen(suffix), chars + offset,
                             &dstlen);
    chars [offset + dstlen] = 0;

    LeaveTrace(cx);
    xml = NULL;
    FrameRegsIter i(cx);
    for (; !i.done() && !i.pc(); ++i)
        JS_ASSERT(!i.fp()->isScriptFrame());
    filename = NULL;
    lineno = 1;
    if (!i.done()) {
        JSStackFrame *fp = i.fp();
        op = (JSOp) *i.pc();
        if (op == JSOP_TOXML || op == JSOP_TOXMLLIST) {
            filename = fp->script()->filename;
            lineno = js_FramePCToLineNumber(cx, fp);
            for (endp = srcp + srclen; srcp < endp; srcp++) {
                if (*srcp == '\n')
                    --lineno;
            }
        }
    }

    {
        Parser parser(cx);
        if (parser.init(chars, length, filename, lineno)) {
            JSObject *scopeChain = GetScopeChain(cx);
            if (!scopeChain)
                return NULL;
            JSParseNode *pn = parser.parseXMLText(scopeChain, false);
            uintN flags;
            if (pn && GetXMLSettingFlags(cx, &flags)) {
                AutoNamespaceArray namespaces(cx);
                if (namespaces.array.setCapacity(cx, 1))
                    xml = ParseNodeToXML(&parser, pn, &namespaces.array, flags);
            }
        }
    }

    cx->free(chars);
    return xml;

#undef constrlen
}

/*
 * Errata in 10.3.1, 10.4.1, and 13.4.4.24 (at least).
 *
 * 10.3.1 Step 6(a) fails to NOTE that implementations that do not enforce
 * the constraint:
 *
 *     for all x belonging to XML:
 *         x.[[InScopeNamespaces]] >= x.[[Parent]].[[InScopeNamespaces]]
 *
 * must union x.[[InScopeNamespaces]] into x[0].[[InScopeNamespaces]] here
 * (in new sub-step 6(a), renumbering the others to (b) and (c)).
 *
 * Same goes for 10.4.1 Step 7(a).
 *
 * In order for XML.prototype.namespaceDeclarations() to work correctly, the
 * default namespace thereby unioned into x[0].[[InScopeNamespaces]] must be
 * flagged as not declared, so that 13.4.4.24 Step 8(a) can exclude all such
 * undeclared namespaces associated with x not belonging to ancestorNS.
 */
static JSXML *
OrphanXMLChild(JSContext *cx, JSXML *xml, uint32 i)
{
    JSObject *ns;

    ns = XMLARRAY_MEMBER(&xml->xml_namespaces, 0, JSObject);
    xml = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
    if (!ns || !xml)
        return xml;
    if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        if (!XMLARRAY_APPEND(cx, &xml->xml_namespaces, ns))
            return NULL;
        ns->setNamespaceDeclared(JSVAL_VOID);
    }
    xml->parent = NULL;
    return xml;
}

static JSObject *
ToXML(JSContext *cx, jsval v)
{
    JSObject *obj;
    JSXML *xml;
    Class *clasp;
    JSString *str;
    uint32 length;

    if (JSVAL_IS_PRIMITIVE(v)) {
        if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
            goto bad;
    } else {
        obj = JSVAL_TO_OBJECT(v);
        if (obj->isXML()) {
            xml = (JSXML *) obj->getPrivate();
            if (xml->xml_class == JSXML_CLASS_LIST) {
                if (xml->xml_kids.length != 1)
                    goto bad;
                xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
                if (xml) {
                    JS_ASSERT(xml->xml_class != JSXML_CLASS_LIST);
                    return js_GetXMLObject(cx, xml);
                }
            }
            return obj;
        }

        clasp = obj->getClass();
        if (clasp->flags & JSCLASS_DOCUMENT_OBSERVER) {
            JS_ASSERT(0);
        }

        if (clasp != &js_StringClass &&
            clasp != &js_NumberClass &&
            clasp != &js_BooleanClass) {
            goto bad;
        }
    }

    str = js_ValueToString(cx, Valueify(v));
    if (!str)
        return NULL;
    if (str->empty()) {
        length = 0;
#ifdef __GNUC__         /* suppress bogus gcc warnings */
        xml = NULL;
#endif
    } else {
        xml = ParseXMLSource(cx, str);
        if (!xml)
            return NULL;
        length = JSXML_LENGTH(xml);
    }

    if (length == 0) {
        obj = js_NewXMLObject(cx, JSXML_CLASS_TEXT);
        if (!obj)
            return NULL;
    } else if (length == 1) {
        xml = OrphanXMLChild(cx, xml, 0);
        if (!xml)
            return NULL;
        obj = js_GetXMLObject(cx, xml);
        if (!obj)
            return NULL;
    } else {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    return obj;

bad:
    js_ReportValueError(cx, JSMSG_BAD_XML_CONVERSION,
                        JSDVG_IGNORE_STACK, Valueify(v), NULL);
    return NULL;
}

static JSBool
Append(JSContext *cx, JSXML *list, JSXML *kid);

static JSObject *
ToXMLList(JSContext *cx, jsval v)
{
    JSObject *obj, *listobj;
    JSXML *xml, *list, *kid;
    Class *clasp;
    JSString *str;
    uint32 i, length;

    if (JSVAL_IS_PRIMITIVE(v)) {
        if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
            goto bad;
    } else {
        obj = JSVAL_TO_OBJECT(v);
        if (obj->isXML()) {
            xml = (JSXML *) obj->getPrivate();
            if (xml->xml_class != JSXML_CLASS_LIST) {
                listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
                if (!listobj)
                    return NULL;
                list = (JSXML *) listobj->getPrivate();
                if (!Append(cx, list, xml))
                    return NULL;
                return listobj;
            }
            return obj;
        }

        clasp = obj->getClass();
        if (clasp->flags & JSCLASS_DOCUMENT_OBSERVER) {
            JS_ASSERT(0);
        }

        if (clasp != &js_StringClass &&
            clasp != &js_NumberClass &&
            clasp != &js_BooleanClass) {
            goto bad;
        }
    }

    str = js_ValueToString(cx, Valueify(v));
    if (!str)
        return NULL;
    if (str->empty()) {
        xml = NULL;
        length = 0;
    } else {
        if (!js_EnterLocalRootScope(cx))
            return NULL;
        xml = ParseXMLSource(cx, str);
        if (!xml) {
            js_LeaveLocalRootScope(cx);
            return NULL;
        }
        length = JSXML_LENGTH(xml);
    }

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (listobj) {
        list = (JSXML *) listobj->getPrivate();
        for (i = 0; i < length; i++) {
            kid = OrphanXMLChild(cx, xml, i);
            if (!kid || !Append(cx, list, kid)) {
                listobj = NULL;
                break;
            }
        }
    }

    if (xml)
        js_LeaveLocalRootScopeWithResult(cx, listobj);
    return listobj;

bad:
    js_ReportValueError(cx, JSMSG_BAD_XMLLIST_CONVERSION,
                        JSDVG_IGNORE_STACK, Valueify(v), NULL);
    return NULL;
}

/*
 * ECMA-357 10.2.1 Steps 5-7 pulled out as common subroutines of XMLToXMLString
 * and their library-public js_* counterparts.  The guts of MakeXMLCDataString,
 * MakeXMLCommentString, and MakeXMLPIString are further factored into a common
 * MakeXMLSpecialString subroutine.
 *
 * These functions mutate cb, leaving it empty.
 */
static JSString *
MakeXMLSpecialString(JSContext *cx, JSCharBuffer &cb,
                     JSString *str, JSString *str2,
                     const jschar *prefix, size_t prefixlength,
                     const jschar *suffix, size_t suffixlength)
{
    if (!cb.append(prefix, prefixlength) || !AppendString(cb, str))
        return NULL;
    if (str2 && !str2->empty()) {
        if (!cb.append(' ') || !AppendString(cb, str2))
            return NULL;
    }
    if (!cb.append(suffix, suffixlength))
        return NULL;

    return js_NewStringFromCharBuffer(cx, cb);
}

static JSString *
MakeXMLCDATAString(JSContext *cx, JSCharBuffer &cb, JSString *str)
{
    static const jschar cdata_prefix_ucNstr[] = {'<', '!', '[',
                                                 'C', 'D', 'A', 'T', 'A',
                                                 '['};
    static const jschar cdata_suffix_ucNstr[] = {']', ']', '>'};

    return MakeXMLSpecialString(cx, cb, str, NULL,
                                cdata_prefix_ucNstr, 9,
                                cdata_suffix_ucNstr, 3);
}

static JSString *
MakeXMLCommentString(JSContext *cx, JSCharBuffer &cb, JSString *str)
{
    static const jschar comment_prefix_ucNstr[] = {'<', '!', '-', '-'};
    static const jschar comment_suffix_ucNstr[] = {'-', '-', '>'};

    return MakeXMLSpecialString(cx, cb, str, NULL,
                                comment_prefix_ucNstr, 4,
                                comment_suffix_ucNstr, 3);
}

static JSString *
MakeXMLPIString(JSContext *cx, JSCharBuffer &cb, JSString *name,
                JSString *value)
{
    static const jschar pi_prefix_ucNstr[] = {'<', '?'};
    static const jschar pi_suffix_ucNstr[] = {'?', '>'};

    return MakeXMLSpecialString(cx, cb, name, value,
                                pi_prefix_ucNstr, 2,
                                pi_suffix_ucNstr, 2);
}

/*
 * ECMA-357 10.2.1 17(d-g) pulled out into a common subroutine that appends
 * equals, a double quote, an attribute value, and a closing double quote.
 */
static bool
AppendAttributeValue(JSContext *cx, JSCharBuffer &cb, JSString *valstr)
{
    if (!cb.append('='))
        return false;
    valstr = js_EscapeAttributeValue(cx, valstr, JS_TRUE);
    return valstr && AppendString(cb, valstr);
}

/*
 * ECMA-357 10.2.1.1 EscapeElementValue helper method.

 * These functions mutate cb, leaving it empty.
 */
static JSString *
EscapeElementValue(JSContext *cx, JSCharBuffer &cb, JSString *str, uint32 toSourceFlag)
{
    size_t length;
    const jschar *start;
    str->getCharsAndLength(start, length);

    for (const jschar *cp = start, *end = start + length; cp != end; ++cp) {
        jschar c = *cp;
        switch (*cp) {
          case '<':
            if (!js_AppendLiteral(cb, js_lt_entity_str))
                return NULL;
            break;
          case '>':
            if (!js_AppendLiteral(cb, js_gt_entity_str))
                return NULL;
            break;
          case '&':
            if (!js_AppendLiteral(cb, js_amp_entity_str))
                return NULL;
            break;
          case '{':
            /*
             * If EscapeElementValue is called by toSource/uneval, we also need
             * to escape '{'. See bug 463360.
             */
            if (toSourceFlag) {
                if (!js_AppendLiteral(cb, js_leftcurly_entity_str))
                    return NULL;
                break;
            }
            /* FALL THROUGH */
          default:
            if (!cb.append(c))
                return NULL;
        }
    }
    return js_NewStringFromCharBuffer(cx, cb);
}

/*
 * ECMA-357 10.2.1.2 EscapeAttributeValue helper method.
 *
 * These functions mutate cb, leaving it empty.
 */
static JSString *
EscapeAttributeValue(JSContext *cx, JSCharBuffer &cb, JSString *str,
                     JSBool quote)
{
    size_t length;
    const jschar *start;
    str->getCharsAndLength(start, length);

    if (quote && !cb.append('"'))
        return NULL;

    for (const jschar *cp = start, *end = start + length; cp != end; ++cp) {
        jschar c = *cp;
        switch (c) {
          case '"':
            if (!js_AppendLiteral(cb, js_quot_entity_str))
                return NULL;
            break;
          case '<':
            if (!js_AppendLiteral(cb, js_lt_entity_str))
                return NULL;
            break;
          case '&':
            if (!js_AppendLiteral(cb, js_amp_entity_str))
                return NULL;
            break;
          case '\n':
            if (!js_AppendLiteral(cb, "&#xA;"))
                return NULL;
            break;
          case '\r':
            if (!js_AppendLiteral(cb, "&#xD;"))
                return NULL;
            break;
          case '\t':
            if (!js_AppendLiteral(cb, "&#x9;"))
                return NULL;
            break;
          default:
            if (!cb.append(c))
                return NULL;
        }
    }

    if (quote && !cb.append('"'))
        return NULL;

    return js_NewStringFromCharBuffer(cx, cb);
}

/* 13.3.5.4 [[GetNamespace]]([InScopeNamespaces]) */
static JSObject *
GetNamespace(JSContext *cx, JSObject *qn, const JSXMLArray *inScopeNSes)
{
    JSString *uri, *prefix, *nsprefix;
    JSObject *match, *ns;
    uint32 i, n;
    jsval argv[2];

    uri = GetURI(qn);
    prefix = GetPrefix(qn);
    JS_ASSERT(uri);
    if (!uri) {
        JSAutoByteString bytes;
        const char *s = !prefix ?
                        js_undefined_str
                        : js_ValueToPrintable(cx, StringValue(prefix), &bytes);
        if (s)
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_XML_NAMESPACE, s);
        return NULL;
    }

    /* Look for a matching namespace in inScopeNSes, if provided. */
    match = NULL;
    if (inScopeNSes) {
        for (i = 0, n = inScopeNSes->length; i < n; i++) {
            ns = XMLARRAY_MEMBER(inScopeNSes, i, JSObject);
            if (!ns)
                continue;

            /*
             * Erratum, very tricky, and not specified in ECMA-357 13.3.5.4:
             * If we preserve prefixes, we must match null prefix against
             * an empty prefix of ns, in order to avoid generating redundant
             * prefixed and default namespaces for cases such as:
             *
             *   x = <t xmlns="http://foo.com"/>
             *   print(x.toXMLString());
             *
             * Per 10.3.2.1, the namespace attribute in t has an empty string
             * prefix (*not* a null prefix), per 10.3.2.1 Step 6(h)(i)(1):
             *
             *   1. If the [local name] property of a is "xmlns"
             *      a. Map ns.prefix to the empty string
             *
             * But t's name has a null prefix in this implementation, meaning
             * *undefined*, per 10.3.2.1 Step 6(c)'s NOTE (which refers to
             * the http://www.w3.org/TR/xml-infoset/ spec, item 2.2.3, without
             * saying how "no value" maps to an ECMA-357 value -- but it must
             * map to the *undefined* prefix value).
             *
             * Since "" != undefined (or null, in the current implementation)
             * the ECMA-357 spec will fail to match in [[GetNamespace]] called
             * on t with argument {} U {(prefix="", uri="http://foo.com")}.
             * This spec bug leads to ToXMLString results that duplicate the
             * declared namespace.
             */
            if (js_EqualStrings(GetURI(ns), uri)) {
                nsprefix = GetPrefix(ns);
                if (nsprefix == prefix ||
                    ((nsprefix && prefix)
                     ? js_EqualStrings(nsprefix, prefix)
                     : (nsprefix ? nsprefix : prefix)->empty())) {
                    match = ns;
                    break;
                }
            }
        }
    }

    /* If we didn't match, make a new namespace from qn. */
    if (!match) {
        argv[0] = prefix ? STRING_TO_JSVAL(prefix) : JSVAL_VOID;
        argv[1] = STRING_TO_JSVAL(uri);
        ns = js_ConstructObject(cx, &js_NamespaceClass, NULL, NULL,
                                2, Valueify(argv));
        if (!ns)
            return NULL;
        match = ns;
    }
    return match;
}

static JSString *
GeneratePrefix(JSContext *cx, JSString *uri, JSXMLArray *decls)
{
    const jschar *cp, *start, *end;
    size_t length, newlength, offset;
    uint32 i, n, m, serial;
    jschar *bp, *dp;
    JSBool done;
    JSObject *ns;
    JSString *nsprefix, *prefix;

    JS_ASSERT(!uri->empty());

    /*
     * If there are no *declared* namespaces, skip all collision detection and
     * return a short prefix quickly; an example of such a situation:
     *
     *   var x = <f/>;
     *   var n = new Namespace("http://example.com/");
     *   x.@n::att = "val";
     *   x.toXMLString();
     *
     * This is necessary for various log10 uses below to be valid.
     */
    if (decls->length == 0)
        return JS_NewStringCopyZ(cx, "a");

    /*
     * Try peeling off the last filename suffix or pathname component till
     * we have a valid XML name.  This heuristic will prefer "xul" given
     * ".../there.is.only.xul", "xbl" given ".../xbl", and "xbl2" given any
     * likely URI of the form ".../xbl2/2005".
     */
    uri->getCharsAndEnd(start, end);
    cp = end;
    while (--cp > start) {
        if (*cp == '.' || *cp == '/' || *cp == ':') {
            ++cp;
            length = end - cp;
            if (IsXMLName(cp, length) && !STARTS_WITH_XML(cp, length))
                break;
            end = --cp;
        }
    }
    length = end - cp;

    /*
     * If the namespace consisted only of non-XML names or names that begin
     * case-insensitively with "xml", arbitrarily create a prefix consisting
     * of 'a's of size length (allowing dp-calculating code to work with or
     * without this branch executing) plus the space for storing a hyphen and
     * the serial number (avoiding reallocation if a collision happens).
     */
    bp = (jschar *) cp;
    newlength = length;
    if (STARTS_WITH_XML(cp, length) || !IsXMLName(cp, length)) {
        newlength = length + 2 + (size_t) log10((double) decls->length);
        bp = (jschar *)
             cx->malloc((newlength + 1) * sizeof(jschar));
        if (!bp)
            return NULL;

        bp[newlength] = 0;
        for (i = 0; i < newlength; i++)
             bp[i] = 'a';
    }

    /*
     * Now search through decls looking for a collision.  If we collide with
     * an existing prefix, start tacking on a hyphen and a serial number.
     */
    serial = 0;
    do {
        done = JS_TRUE;
        for (i = 0, n = decls->length; i < n; i++) {
            ns = XMLARRAY_MEMBER(decls, i, JSObject);
            if (ns && (nsprefix = GetPrefix(ns)) &&
                nsprefix->length() == newlength &&
                !memcmp(nsprefix->chars(), bp,
                        newlength * sizeof(jschar))) {
                if (bp == cp) {
                    newlength = length + 2 + (size_t) log10((double) n);
                    bp = (jschar *)
                         cx->malloc((newlength + 1) * sizeof(jschar));
                    if (!bp)
                        return NULL;
                    js_strncpy(bp, cp, length);
                }

                ++serial;
                JS_ASSERT(serial <= n);
                dp = bp + length + 2 + (size_t) log10((double) serial);
                *dp = 0;
                for (m = serial; m != 0; m /= 10)
                    *--dp = (jschar)('0' + m % 10);
                *--dp = '-';
                JS_ASSERT(dp == bp + length);

                done = JS_FALSE;
                break;
            }
        }
    } while (!done);

    if (bp == cp) {
        offset = cp - start;
        prefix = js_NewDependentString(cx, uri, offset, length);
    } else {
        prefix = js_NewString(cx, bp, newlength);
        if (!prefix)
            cx->free(bp);
    }
    return prefix;
}

static JSBool
namespace_match(const void *a, const void *b)
{
    const JSObject *nsa = (const JSObject *) a;
    const JSObject *nsb = (const JSObject *) b;
    JSString *prefixa, *prefixb = GetPrefix(nsb);

    if (prefixb) {
        prefixa = GetPrefix(nsa);
        return prefixa && js_EqualStrings(prefixa, prefixb);
    }
    return js_EqualStrings(GetURI(nsa), GetURI(nsb));
}

/* ECMA-357 10.2.1 and 10.2.2 */
#define TO_SOURCE_FLAG 0x80000000

static JSString *
XMLToXMLString(JSContext *cx, JSXML *xml, const JSXMLArray *ancestorNSes,
               uint32 indentLevel)
{
    JSBool pretty, indentKids;
    JSCharBuffer cb(cx);
    JSString *str, *prefix, *nsuri;
    uint32 i, n, nextIndentLevel;
    JSObject *ns, *ns2;
    AutoNamespaceArray empty(cx), decls(cx), ancdecls(cx);

    if (!GetBooleanXMLSetting(cx, js_prettyPrinting_str, &pretty))
        return NULL;

    if (pretty) {
        if (!cb.appendN(' ', indentLevel & ~TO_SOURCE_FLAG))
            return NULL;
    }

    str = NULL;

    switch (xml->xml_class) {
      case JSXML_CLASS_TEXT:
        /* Step 4. */
        if (pretty) {
            str = ChompXMLWhitespace(cx, xml->xml_value);
            if (!str)
                return NULL;
        } else {
            str = xml->xml_value;
        }
        return EscapeElementValue(cx, cb, str, indentLevel & TO_SOURCE_FLAG);

      case JSXML_CLASS_ATTRIBUTE:
        /* Step 5. */
        return EscapeAttributeValue(cx, cb, xml->xml_value,
                                    (indentLevel & TO_SOURCE_FLAG) != 0);

      case JSXML_CLASS_COMMENT:
        /* Step 6. */
        return MakeXMLCommentString(cx, cb, xml->xml_value);

      case JSXML_CLASS_PROCESSING_INSTRUCTION:
        /* Step 7. */
        return MakeXMLPIString(cx, cb, GetLocalName(xml->name),
                               xml->xml_value);

      case JSXML_CLASS_LIST:
        /* ECMA-357 10.2.2. */
        {
            JSXMLArrayCursor cursor(&xml->xml_kids);
            i = 0;
            while (JSXML *kid = (JSXML *) cursor.getNext()) {
                if (pretty && i != 0) {
                    if (!cb.append('\n'))
                        return NULL;
                }

                JSString *kidstr = XMLToXMLString(cx, kid, ancestorNSes, indentLevel);
                if (!kidstr || !AppendString(cb, kidstr))
                    return NULL;
                ++i;
            }
        }

        if (cb.empty())
            return cx->runtime->emptyString;
        return js_NewStringFromCharBuffer(cx, cb);

      default:;
    }

    /* After this point, control must flow through label out: to exit. */
    if (!js_EnterLocalRootScope(cx))
        return NULL;

    /* ECMA-357 10.2.1 step 8 onward: handle ToXMLString on an XML element. */
    if (!ancestorNSes)
        ancestorNSes = &empty.array;

    /* Clone in-scope namespaces not in ancestorNSes into decls. */
    {
        JSXMLArrayCursor cursor(&xml->xml_namespaces);
        while ((ns = (JSObject *) cursor.getNext()) != NULL) {
            if (!IsDeclared(ns))
                continue;
            if (!XMLARRAY_HAS_MEMBER(ancestorNSes, ns, namespace_identity)) {
                /* NOTE: may want to exclude unused namespaces here. */
                ns2 = NewXMLNamespace(cx, GetPrefix(ns), GetURI(ns), JS_TRUE);
                if (!ns2 || !XMLARRAY_APPEND(cx, &decls.array, ns2))
                    goto out;
            }
        }
    }

    /*
     * Union ancestorNSes and decls into ancdecls.  Note that ancdecls does
     * not own its member references.  In the spec, ancdecls has no name, but
     * is always written out as (AncestorNamespaces U namespaceDeclarations).
     */

    if (!ancdecls.array.setCapacity(cx, ancestorNSes->length + decls.length()))
        goto out;
    for (i = 0, n = ancestorNSes->length; i < n; i++) {
        ns2 = XMLARRAY_MEMBER(ancestorNSes, i, JSObject);
        if (!ns2)
            continue;
        JS_ASSERT(!XMLARRAY_HAS_MEMBER(&decls.array, ns2, namespace_identity));
        if (!XMLARRAY_APPEND(cx, &ancdecls.array, ns2))
            goto out;
    }
    for (i = 0, n = decls.length(); i < n; i++) {
        ns2 = XMLARRAY_MEMBER(&decls.array, i, JSObject);
        if (!ns2)
            continue;
        JS_ASSERT(!XMLARRAY_HAS_MEMBER(&ancdecls.array, ns2, namespace_identity));
        if (!XMLARRAY_APPEND(cx, &ancdecls.array, ns2))
            goto out;
    }

    /* Step 11, except we don't clone ns unless its prefix is undefined. */
    ns = GetNamespace(cx, xml->name, &ancdecls.array);
    if (!ns)
        goto out;

    /* Step 12 (NULL means *undefined* here), plus the deferred ns cloning. */
    prefix = GetPrefix(ns);
    if (!prefix) {
        /*
         * Create a namespace prefix that isn't used by any member of decls.
         * Assign the new prefix to a copy of ns.  Flag this namespace as if
         * it were declared, for assertion-testing's sake later below.
         *
         * Erratum: if prefix and xml->name are both null (*undefined* in
         * ECMA-357), we know that xml was named using the default namespace
         * (proof: see GetNamespace and the Namespace constructor called with
         * two arguments).  So we ought not generate a new prefix here, when
         * we can declare ns as the default namespace for xml.
         *
         * This helps descendants inherit the namespace instead of redundantly
         * redeclaring it with generated prefixes in each descendant.
         */
        nsuri = GetURI(ns);
        if (!GetPrefix(xml->name)) {
            prefix = cx->runtime->emptyString;
        } else {
            prefix = GeneratePrefix(cx, nsuri, &ancdecls.array);
            if (!prefix)
                goto out;
        }
        ns = NewXMLNamespace(cx, prefix, nsuri, JS_TRUE);
        if (!ns)
            goto out;

        /*
         * If the xml->name was unprefixed, we must remove any declared default
         * namespace from decls before appending ns.  How can you get a default
         * namespace in decls that doesn't match the one from name?  Apparently
         * by calling x.setNamespace(ns) where ns has no prefix.  The other way
         * to fix this is to update x's in-scope namespaces when setNamespace
         * is called, but that's not specified by ECMA-357.
         *
         * Likely Erratum here, depending on whether the lack of update to x's
         * in-scope namespace in XML.prototype.setNamespace (13.4.4.36) is an
         * erratum or not.  Note that changing setNamespace to update the list
         * of in-scope namespaces will change x.namespaceDeclarations().
         */
        if (prefix->empty()) {
            i = XMLArrayFindMember(&decls.array, ns, namespace_match);
            if (i != XML_NOT_FOUND)
                XMLArrayDelete(cx, &decls.array, i, JS_TRUE);
        }

        /*
         * In the spec, ancdecls has no name, but is always written out as
         * (AncestorNamespaces U namespaceDeclarations).  Since we compute
         * that union in ancdecls, any time we append a namespace strong
         * ref to decls, we must also append a weak ref to ancdecls.  Order
         * matters here: code at label out: releases strong refs in decls.
         */
        if (!XMLARRAY_APPEND(cx, &ancdecls.array, ns) ||
            !XMLARRAY_APPEND(cx, &decls.array, ns)) {
            goto out;
        }
    }

    /* Format the element or point-tag into cb. */
    if (!cb.append('<'))
        goto out;

    if (prefix && !prefix->empty()) {
        if (!AppendString(cb, prefix) || !cb.append(':'))
            goto out;
    }
    if (!AppendString(cb, GetLocalName(xml->name)))
        goto out;

    /*
     * Step 16 makes a union to avoid writing two loops in step 17, to share
     * common attribute value appending spec-code.  We prefer two loops for
     * faster code and less data overhead.
     */

    /* Step 17(b): append attributes. */
    {
        JSXMLArrayCursor cursor(&xml->xml_attrs);
        while (JSXML *attr = (JSXML *) cursor.getNext()) {
            if (!cb.append(' '))
                goto out;
            ns2 = GetNamespace(cx, attr->name, &ancdecls.array);
            if (!ns2)
                goto out;

            /* 17(b)(ii): NULL means *undefined* here. */
            prefix = GetPrefix(ns2);
            if (!prefix) {
                prefix = GeneratePrefix(cx, GetURI(ns2), &ancdecls.array);
                if (!prefix)
                    goto out;

                /* Again, we avoid copying ns2 until we know it's prefix-less. */
                ns2 = NewXMLNamespace(cx, prefix, GetURI(ns2), JS_TRUE);
                if (!ns2)
                    goto out;

                /*
                 * In the spec, ancdecls has no name, but is always written out as
                 * (AncestorNamespaces U namespaceDeclarations).  Since we compute
                 * that union in ancdecls, any time we append a namespace strong
                 * ref to decls, we must also append a weak ref to ancdecls.  Order
                 * matters here: code at label out: releases strong refs in decls.
                 */
                if (!XMLARRAY_APPEND(cx, &ancdecls.array, ns2) ||
                    !XMLARRAY_APPEND(cx, &decls.array, ns2)) {
                    goto out;
                }
            }

            /* 17(b)(iii). */
            if (!prefix->empty()) {
                if (!AppendString(cb, prefix) || !cb.append(':'))
                    goto out;
            }

            /* 17(b)(iv). */
            if (!AppendString(cb, GetLocalName(attr->name)))
                goto out;

            /* 17(d-g). */
            if (!AppendAttributeValue(cx, cb, attr->xml_value))
                goto out;
        }
    }

    /* Step 17(c): append XML namespace declarations. */
    {
        JSXMLArrayCursor cursor(&decls.array);
        while (JSObject *ns3 = (JSObject *) cursor.getNext()) {
            JS_ASSERT(IsDeclared(ns3));

            if (!js_AppendLiteral(cb, " xmlns"))
                goto out;

            /* 17(c)(ii): NULL means *undefined* here. */
            prefix = GetPrefix(ns3);
            if (!prefix) {
                prefix = GeneratePrefix(cx, GetURI(ns3), &ancdecls.array);
                if (!prefix)
                    goto out;
                ns3->setNamePrefix(STRING_TO_JSVAL(prefix));
            }

            /* 17(c)(iii). */
            if (!prefix->empty()) {
                if (!cb.append(':') || !AppendString(cb, prefix))
                    goto out;
            }

            /* 17(d-g). */
            if (!AppendAttributeValue(cx, cb, GetURI(ns3)))
                goto out;
        }
    }

    /* Step 18: handle point tags. */
    n = xml->xml_kids.length;
    if (n == 0) {
        if (!js_AppendLiteral(cb, "/>"))
            goto out;
    } else {
        /* Steps 19 through 25: handle element content, and open the end-tag. */
        if (!cb.append('>'))
            goto out;
        {
            JSXML *kid;
            indentKids = n > 1 ||
                         (n == 1 &&
                          (kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML)) &&
                          kid->xml_class != JSXML_CLASS_TEXT);
        }

        if (pretty && indentKids) {
            if (!GetUint32XMLSetting(cx, js_prettyIndent_str, &i))
                goto out;
            nextIndentLevel = indentLevel + i;
        } else {
            nextIndentLevel = indentLevel & TO_SOURCE_FLAG;
        }

        {
            JSXMLArrayCursor cursor(&xml->xml_kids);
            while (JSXML *kid = (JSXML *) cursor.getNext()) {
                if (pretty && indentKids) {
                    if (!cb.append('\n'))
                        goto out;
                }

                JSString *kidstr = XMLToXMLString(cx, kid, &ancdecls.array, nextIndentLevel);
                if (!kidstr)
                    goto out;

                if (!AppendString(cb, kidstr))
                    goto out;
            }
        }

        if (pretty && indentKids) {
            if (!cb.append('\n') ||
                !cb.appendN(' ', indentLevel & ~TO_SOURCE_FLAG))
                goto out;
        }
        if (!js_AppendLiteral(cb, "</"))
            goto out;

        /* Step 26. */
        prefix = GetPrefix(ns);
        if (prefix && !prefix->empty()) {
            if (!AppendString(cb, prefix) || !cb.append(':'))
                goto out;
        }

        /* Step 27. */
        if (!AppendString(cb, GetLocalName(xml->name)) || !cb.append('>'))
            goto out;
    }

    str = js_NewStringFromCharBuffer(cx, cb);
out:
    js_LeaveLocalRootScopeWithResult(cx, str);
    return str;
}

/* ECMA-357 10.2 */
static JSString *
ToXMLString(JSContext *cx, jsval v, uint32 toSourceFlag)
{
    JSObject *obj;
    JSString *str;
    JSXML *xml;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_XML_CONVERSION,
                             JSVAL_IS_NULL(v) ? js_null_str : js_undefined_str);
        return NULL;
    }

    if (JSVAL_IS_BOOLEAN(v) || JSVAL_IS_NUMBER(v))
        return js_ValueToString(cx, Valueify(v));

    if (JSVAL_IS_STRING(v)) {
        JSCharBuffer cb(cx);
        return EscapeElementValue(cx, cb, JSVAL_TO_STRING(v), toSourceFlag);
    }

    obj = JSVAL_TO_OBJECT(v);
    if (!obj->isXML()) {
        if (!DefaultValue(cx, obj, JSTYPE_STRING, Valueify(&v)))
            return NULL;
        str = js_ValueToString(cx, Valueify(v));
        if (!str)
            return NULL;
        JSCharBuffer cb(cx);
        return EscapeElementValue(cx, cb, str, toSourceFlag);
    }

    /* Handle non-element cases in this switch, returning from each case. */
    xml = (JSXML *) obj->getPrivate();
    return XMLToXMLString(cx, xml, NULL, toSourceFlag | 0);
}

static JSObject *
ToAttributeName(JSContext *cx, jsval v)
{
    JSString *name, *uri, *prefix;
    JSObject *obj;
    Class *clasp;
    JSObject *qn;

    if (JSVAL_IS_STRING(v)) {
        name = JSVAL_TO_STRING(v);
        uri = prefix = cx->runtime->emptyString;
    } else {
        if (JSVAL_IS_PRIMITIVE(v)) {
            js_ReportValueError(cx, JSMSG_BAD_XML_ATTR_NAME,
                                JSDVG_IGNORE_STACK, Valueify(v), NULL);
            return NULL;
        }

        obj = JSVAL_TO_OBJECT(v);
        clasp = obj->getClass();
        if (clasp == &js_AttributeNameClass)
            return obj;

        if (clasp == &js_QNameClass) {
            qn = obj;
            uri = GetURI(qn);
            prefix = GetPrefix(qn);
            name = GetLocalName(qn);
        } else {
            if (clasp == &js_AnyNameClass) {
                name = ATOM_TO_STRING(cx->runtime->atomState.starAtom);
            } else {
                name = js_ValueToString(cx, Valueify(v));
                if (!name)
                    return NULL;
            }
            uri = prefix = cx->runtime->emptyString;
        }
    }

    qn = NewXMLQName(cx, uri, prefix, name, &js_AttributeNameClass);
    if (!qn)
        return NULL;
    return qn;
}

static void
ReportBadXMLName(JSContext *cx, const Value &idval)
{
    js_ReportValueError(cx, JSMSG_BAD_XML_NAME, JSDVG_IGNORE_STACK, idval, NULL);
}

static JSBool
IsFunctionQName(JSContext *cx, JSObject *qn, jsid *funidp)
{
    JSAtom *atom;
    JSString *uri;

    atom = cx->runtime->atomState.functionNamespaceURIAtom;
    uri = GetURI(qn);
    if (uri &&
        (uri == ATOM_TO_STRING(atom) ||
         js_EqualStrings(uri, ATOM_TO_STRING(atom)))) {
        return JS_ValueToId(cx, STRING_TO_JSVAL(GetLocalName(qn)), funidp);
    }
    *funidp = JSID_VOID;
    return JS_TRUE;
}

JSBool
js_IsFunctionQName(JSContext *cx, JSObject *obj, jsid *funidp)
{
    if (obj->getClass() == &js_QNameClass)
        return IsFunctionQName(cx, obj, funidp);
    *funidp = JSID_VOID;
    return JS_TRUE;
}

static JSObject *
ToXMLName(JSContext *cx, jsval v, jsid *funidp)
{
    JSAtom *atomizedName;
    JSString *name;
    JSObject *obj;
    Class *clasp;
    uint32 index;

    if (JSVAL_IS_STRING(v)) {
        name = JSVAL_TO_STRING(v);
    } else {
        if (JSVAL_IS_PRIMITIVE(v)) {
            ReportBadXMLName(cx, Valueify(v));
            return NULL;
        }

        obj = JSVAL_TO_OBJECT(v);
        clasp = obj->getClass();
        if (clasp == &js_AttributeNameClass || clasp == &js_QNameClass)
            goto out;
        if (clasp == &js_AnyNameClass) {
            name = ATOM_TO_STRING(cx->runtime->atomState.starAtom);
            goto construct;
        }
        name = js_ValueToString(cx, Valueify(v));
        if (!name)
            return NULL;
    }

    atomizedName = js_AtomizeString(cx, name, 0);
    if (!atomizedName)
        return NULL;

    /*
     * ECMA-357 10.6.1 step 1 seems to be incorrect.  The spec says:
     *
     * 1. If ToString(ToNumber(P)) == ToString(P), throw a TypeError exception
     *
     * First, _P_ should be _s_, to refer to the given string.
     *
     * Second, why does ToXMLName applied to the string type throw TypeError
     * only for numeric literals without any leading or trailing whitespace?
     *
     * If the idea is to reject uint32 property names, then the check needs to
     * be stricter, to exclude hexadecimal and floating point literals.
     */
    if (js_IdIsIndex(ATOM_TO_JSID(atomizedName), &index))
        goto bad;

    if (*name->chars() == '@') {
        name = js_NewDependentString(cx, name, 1, name->length() - 1);
        if (!name)
            return NULL;
        *funidp = JSID_VOID;
        return ToAttributeName(cx, STRING_TO_JSVAL(name));
    }

construct:
    v = STRING_TO_JSVAL(name);
    obj = js_ConstructObject(cx, &js_QNameClass, NULL, NULL, 1, Valueify(&v));
    if (!obj)
        return NULL;

out:
    if (!IsFunctionQName(cx, obj, funidp))
        return NULL;
    return obj;

bad:
    JSAutoByteString bytes;
    if (js_ValueToPrintable(cx, StringValue(name), &bytes))
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_XML_NAME, bytes.ptr());
    return NULL;
}

/* ECMA-357 9.1.1.13 XML [[AddInScopeNamespace]]. */
static JSBool
AddInScopeNamespace(JSContext *cx, JSXML *xml, JSObject *ns)
{
    JSString *prefix, *prefix2;
    JSObject *match, *ns2;
    uint32 i, n, m;

    if (xml->xml_class != JSXML_CLASS_ELEMENT)
        return JS_TRUE;

    /* NULL means *undefined* here -- see ECMA-357 9.1.1.13 step 2. */
    prefix = GetPrefix(ns);
    if (!prefix) {
        match = NULL;
        for (i = 0, n = xml->xml_namespaces.length; i < n; i++) {
            ns2 = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSObject);
            if (ns2 && js_EqualStrings(GetURI(ns2), GetURI(ns))) {
                match = ns2;
                break;
            }
        }
        if (!match && !XMLARRAY_ADD_MEMBER(cx, &xml->xml_namespaces, n, ns))
            return JS_FALSE;
    } else {
        if (prefix->empty() && GetURI(xml->name)->empty())
            return JS_TRUE;
        match = NULL;
#ifdef __GNUC__         /* suppress bogus gcc warnings */
        m = XML_NOT_FOUND;
#endif
        for (i = 0, n = xml->xml_namespaces.length; i < n; i++) {
            ns2 = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSObject);
            if (ns2 && (prefix2 = GetPrefix(ns2)) &&
                js_EqualStrings(prefix2, prefix)) {
                match = ns2;
                m = i;
                break;
            }
        }
        if (match && !js_EqualStrings(GetURI(match), GetURI(ns))) {
            ns2 = XMLARRAY_DELETE(cx, &xml->xml_namespaces, m, JS_TRUE,
                                  JSObject);
            JS_ASSERT(ns2 == match);
            match->setNamePrefix(JSVAL_VOID);
            if (!AddInScopeNamespace(cx, xml, match))
                return JS_FALSE;
        }
        if (!XMLARRAY_APPEND(cx, &xml->xml_namespaces, ns))
            return JS_FALSE;
    }

    /* OPTION: enforce that descendants have superset namespaces. */
    return JS_TRUE;
}

/* ECMA-357 9.2.1.6 XMLList [[Append]]. */
static JSBool
Append(JSContext *cx, JSXML *list, JSXML *xml)
{
    uint32 i, j, k, n;
    JSXML *kid;

    JS_ASSERT(list->xml_class == JSXML_CLASS_LIST);
    i = list->xml_kids.length;
    n = 1;
    if (xml->xml_class == JSXML_CLASS_LIST) {
        list->xml_target = xml->xml_target;
        list->xml_targetprop = xml->xml_targetprop;
        n = JSXML_LENGTH(xml);
        k = i + n;
        if (!list->xml_kids.setCapacity(cx, k))
            return JS_FALSE;
        for (j = 0; j < n; j++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, j, JSXML);
            if (kid)
                XMLARRAY_SET_MEMBER(&list->xml_kids, i + j, kid);
        }
        return JS_TRUE;
    }

    list->xml_target = xml->parent;
    if (xml->xml_class == JSXML_CLASS_PROCESSING_INSTRUCTION)
        list->xml_targetprop = NULL;
    else
        list->xml_targetprop = xml->name;
    if (!XMLARRAY_ADD_MEMBER(cx, &list->xml_kids, i, xml))
        return JS_FALSE;
    return JS_TRUE;
}

/* ECMA-357 9.1.1.7 XML [[DeepCopy]] and 9.2.1.7 XMLList [[DeepCopy]]. */
static JSXML *
DeepCopyInLRS(JSContext *cx, JSXML *xml, uintN flags);

static JSXML *
DeepCopy(JSContext *cx, JSXML *xml, JSObject *obj, uintN flags)
{
    JSXML *copy;

    /* Our caller may not be protecting newborns with a local root scope. */
    if (!js_EnterLocalRootScope(cx))
        return NULL;
    copy = DeepCopyInLRS(cx, xml, flags);
    if (copy) {
        if (obj) {
            /* Caller provided the object for this copy, hook 'em up. */
            obj->setPrivate(copy);
            copy->object = obj;
        } else if (!js_GetXMLObject(cx, copy)) {
            copy = NULL;
        }
    }
    js_LeaveLocalRootScopeWithResult(cx, copy);
    return copy;
}

/*
 * (i) We must be in a local root scope (InLRS).
 * (ii) parent must have a rooted object.
 * (iii) from's owning object must be locked if not thread-local.
 */
static JSBool
DeepCopySetInLRS(JSContext *cx, JSXMLArray *from, JSXMLArray *to, JSXML *parent,
                 uintN flags)
{
    uint32 j, n;
    JSXML *kid2;
    JSString *str;

    n = from->length;
    if (!to->setCapacity(cx, n))
        return JS_FALSE;

    JSXMLArrayCursor cursor(from);
    j = 0;
    while (JSXML *kid = (JSXML *) cursor.getNext()) {
        if ((flags & XSF_IGNORE_COMMENTS) &&
            kid->xml_class == JSXML_CLASS_COMMENT) {
            continue;
        }
        if ((flags & XSF_IGNORE_PROCESSING_INSTRUCTIONS) &&
            kid->xml_class == JSXML_CLASS_PROCESSING_INSTRUCTION) {
            continue;
        }
        if ((flags & XSF_IGNORE_WHITESPACE) &&
            (kid->xml_flags & XMLF_WHITESPACE_TEXT)) {
            continue;
        }
        kid2 = DeepCopyInLRS(cx, kid, flags);
        if (!kid2) {
            to->length = j;
            return JS_FALSE;
        }

        if ((flags & XSF_IGNORE_WHITESPACE) &&
            n > 1 && kid2->xml_class == JSXML_CLASS_TEXT) {
            str = ChompXMLWhitespace(cx, kid2->xml_value);
            if (!str) {
                to->length = j;
                return JS_FALSE;
            }
            kid2->xml_value = str;
        }

        XMLARRAY_SET_MEMBER(to, j, kid2);
        ++j;
        if (parent->xml_class != JSXML_CLASS_LIST)
            kid2->parent = parent;
    }

    if (j < n)
        to->trim();
    return JS_TRUE;
}

static JSXML *
DeepCopyInLRS(JSContext *cx, JSXML *xml, uintN flags)
{
    JSXML *copy;
    JSObject *qn;
    JSBool ok;
    uint32 i, n;
    JSObject *ns, *ns2;

    JS_CHECK_RECURSION(cx, return NULL);

    copy = js_NewXML(cx, JSXMLClass(xml->xml_class));
    if (!copy)
        return NULL;
    qn = xml->name;
    if (qn) {
        qn = NewXMLQName(cx, GetURI(qn), GetPrefix(qn), GetLocalName(qn));
        if (!qn) {
            ok = JS_FALSE;
            goto out;
        }
    }
    copy->name = qn;
    copy->xml_flags = xml->xml_flags;

    if (JSXML_HAS_VALUE(xml)) {
        copy->xml_value = xml->xml_value;
        ok = JS_TRUE;
    } else {
        ok = DeepCopySetInLRS(cx, &xml->xml_kids, &copy->xml_kids, copy, flags);
        if (!ok)
            goto out;

        if (xml->xml_class == JSXML_CLASS_LIST) {
            copy->xml_target = xml->xml_target;
            copy->xml_targetprop = xml->xml_targetprop;
        } else {
            n = xml->xml_namespaces.length;
            ok = copy->xml_namespaces.setCapacity(cx, n);
            if (!ok)
                goto out;
            for (i = 0; i < n; i++) {
                ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSObject);
                if (!ns)
                    continue;
                ns2 = NewXMLNamespace(cx, GetPrefix(ns), GetURI(ns),
                                      IsDeclared(ns));
                if (!ns2) {
                    copy->xml_namespaces.length = i;
                    ok = JS_FALSE;
                    goto out;
                }
                XMLARRAY_SET_MEMBER(&copy->xml_namespaces, i, ns2);
            }

            ok = DeepCopySetInLRS(cx, &xml->xml_attrs, &copy->xml_attrs, copy,
                                  0);
            if (!ok)
                goto out;
        }
    }

out:
    if (!ok)
        return NULL;
    return copy;
}

/* ECMA-357 9.1.1.4 XML [[DeleteByIndex]]. */
static void
DeleteByIndex(JSContext *cx, JSXML *xml, uint32 index)
{
    JSXML *kid;

    if (JSXML_HAS_KIDS(xml) && index < xml->xml_kids.length) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
        if (kid)
            kid->parent = NULL;
        XMLArrayDelete(cx, &xml->xml_kids, index, JS_TRUE);
    }
}

typedef JSBool (*JSXMLNameMatcher)(JSObject *nameqn, JSXML *xml);

static JSBool
MatchAttrName(JSObject *nameqn, JSXML *attr)
{
    JSObject *attrqn = attr->name;
    JSString *localName = GetLocalName(nameqn);
    JSString *uri;

    return (IS_STAR(localName) ||
            js_EqualStrings(GetLocalName(attrqn), localName)) &&
           (!(uri = GetURI(nameqn)) ||
            js_EqualStrings(GetURI(attrqn), uri));
}

static JSBool
MatchElemName(JSObject *nameqn, JSXML *elem)
{
    JSString *localName = GetLocalName(nameqn);
    JSString *uri;

    return (IS_STAR(localName) ||
            (elem->xml_class == JSXML_CLASS_ELEMENT &&
             js_EqualStrings(GetLocalName(elem->name), localName))) &&
           (!(uri = GetURI(nameqn)) ||
            (elem->xml_class == JSXML_CLASS_ELEMENT &&
             js_EqualStrings(GetURI(elem->name), uri)));
}

/* ECMA-357 9.1.1.8 XML [[Descendants]] and 9.2.1.8 XMLList [[Descendants]]. */
static JSBool
DescendantsHelper(JSContext *cx, JSXML *xml, JSObject *nameqn, JSXML *list)
{
    uint32 i, n;
    JSXML *attr, *kid;

    JS_CHECK_RECURSION(cx, return JS_FALSE);

    if (xml->xml_class == JSXML_CLASS_ELEMENT &&
        nameqn->getClass() == &js_AttributeNameClass) {
        for (i = 0, n = xml->xml_attrs.length; i < n; i++) {
            attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
            if (attr && MatchAttrName(nameqn, attr)) {
                if (!Append(cx, list, attr))
                    return JS_FALSE;
            }
        }
    }

    for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
        if (!kid)
            continue;
        if (nameqn->getClass() != &js_AttributeNameClass &&
            MatchElemName(nameqn, kid)) {
            if (!Append(cx, list, kid))
                return JS_FALSE;
        }
        if (!DescendantsHelper(cx, kid, nameqn, list))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSXML *
Descendants(JSContext *cx, JSXML *xml, jsval id)
{
    jsid funid;
    JSObject *nameqn;
    JSObject *listobj;
    JSXML *list, *kid;
    uint32 i, n;
    JSBool ok;

    nameqn = ToXMLName(cx, id, &funid);
    if (!nameqn)
        return NULL;

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return NULL;
    list = (JSXML *) listobj->getPrivate();
    if (!JSID_IS_VOID(funid))
        return list;

    /*
     * Protect nameqn's object and strings from GC by linking list to it
     * temporarily.  The newborn GC root for the last allocated object
     * protects listobj, which protects list. Any other object allocations
     * occurring beneath DescendantsHelper use local roots.
     */
    list->name = nameqn;
    if (!js_EnterLocalRootScope(cx))
        return NULL;
    if (xml->xml_class == JSXML_CLASS_LIST) {
        ok = JS_TRUE;
        for (i = 0, n = xml->xml_kids.length; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = DescendantsHelper(cx, kid, nameqn, list);
                if (!ok)
                    break;
            }
        }
    } else {
        ok = DescendantsHelper(cx, xml, nameqn, list);
    }
    js_LeaveLocalRootScopeWithResult(cx, list);
    if (!ok)
        return NULL;
    list->name = NULL;
    return list;
}

/* Recursive (JSXML *) parameterized version of Equals. */
static JSBool
XMLEquals(JSContext *cx, JSXML *xml, JSXML *vxml, JSBool *bp)
{
    JSObject *qn, *vqn;
    uint32 i, j, n;
    JSXML *kid, *vkid, *attr, *vattr;
    JSObject *xobj, *vobj;

retry:
    if (xml->xml_class != vxml->xml_class) {
        if (xml->xml_class == JSXML_CLASS_LIST && xml->xml_kids.length == 1) {
            xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
            if (xml)
                goto retry;
        }
        if (vxml->xml_class == JSXML_CLASS_LIST && vxml->xml_kids.length == 1) {
            vxml = XMLARRAY_MEMBER(&vxml->xml_kids, 0, JSXML);
            if (vxml)
                goto retry;
        }
        *bp = JS_FALSE;
        return JS_TRUE;
    }

    qn = xml->name;
    vqn = vxml->name;
    if (qn) {
        *bp = vqn &&
              js_EqualStrings(GetLocalName(qn), GetLocalName(vqn)) &&
              js_EqualStrings(GetURI(qn), GetURI(vqn));
    } else {
        *bp = vqn == NULL;
    }
    if (!*bp)
        return JS_TRUE;

    if (JSXML_HAS_VALUE(xml)) {
        *bp = js_EqualStrings(xml->xml_value, vxml->xml_value);
    } else if (xml->xml_kids.length != vxml->xml_kids.length) {
        *bp = JS_FALSE;
    } else {
        {
            JSXMLArrayCursor cursor(&xml->xml_kids);
            JSXMLArrayCursor vcursor(&vxml->xml_kids);
            for (;;) {
                kid = (JSXML *) cursor.getNext();
                vkid = (JSXML *) vcursor.getNext();
                if (!kid || !vkid) {
                    *bp = !kid && !vkid;
                    break;
                }
                xobj = js_GetXMLObject(cx, kid);
                vobj = js_GetXMLObject(cx, vkid);
                if (!xobj || !vobj ||
                    !js_TestXMLEquality(cx, ObjectValue(*xobj), ObjectValue(*vobj), bp))
                    return JS_FALSE;
                if (!*bp)
                    break;
            }
        }

        if (*bp && xml->xml_class == JSXML_CLASS_ELEMENT) {
            n = xml->xml_attrs.length;
            if (n != vxml->xml_attrs.length)
                *bp = JS_FALSE;
            for (i = 0; *bp && i < n; i++) {
                attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
                if (!attr)
                    continue;
                j = XMLARRAY_FIND_MEMBER(&vxml->xml_attrs, attr, attr_identity);
                if (j == XML_NOT_FOUND) {
                    *bp = JS_FALSE;
                    break;
                }
                vattr = XMLARRAY_MEMBER(&vxml->xml_attrs, j, JSXML);
                if (!vattr)
                    continue;
                *bp = js_EqualStrings(attr->xml_value, vattr->xml_value);
            }
        }
    }

    return JS_TRUE;
}

/* ECMA-357 9.1.1.9 XML [[Equals]] and 9.2.1.9 XMLList [[Equals]]. */
static JSBool
Equals(JSContext *cx, JSXML *xml, jsval v, JSBool *bp)
{
    JSObject *vobj;
    JSXML *vxml;

    if (JSVAL_IS_PRIMITIVE(v)) {
        *bp = JS_FALSE;
        if (xml->xml_class == JSXML_CLASS_LIST) {
            if (xml->xml_kids.length == 1) {
                vxml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
                if (!vxml)
                    return JS_TRUE;
                vobj = js_GetXMLObject(cx, vxml);
                if (!vobj)
                    return JS_FALSE;
                return js_TestXMLEquality(cx, ObjectValue(*vobj), Valueify(v), bp);
            }
            if (JSVAL_IS_VOID(v) && xml->xml_kids.length == 0)
                *bp = JS_TRUE;
        }
    } else {
        vobj = JSVAL_TO_OBJECT(v);
        if (!vobj->isXML()) {
            *bp = JS_FALSE;
        } else {
            vxml = (JSXML *) vobj->getPrivate();
            if (!XMLEquals(cx, xml, vxml, bp))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSBool
CheckCycle(JSContext *cx, JSXML *xml, JSXML *kid)
{
    JS_ASSERT(kid->xml_class != JSXML_CLASS_LIST);

    do {
        if (xml == kid) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_CYCLIC_VALUE, js_XML_str);
            return JS_FALSE;
        }
    } while ((xml = xml->parent) != NULL);

    return JS_TRUE;
}

/* ECMA-357 9.1.1.11 XML [[Insert]]. */
static JSBool
Insert(JSContext *cx, JSXML *xml, uint32 i, jsval v)
{
    uint32 j, n;
    JSXML *vxml, *kid;
    JSObject *vobj;
    JSString *str;

    if (!JSXML_HAS_KIDS(xml))
        return JS_TRUE;

    n = 1;
    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (vobj->isXML()) {
            vxml = (JSXML *) vobj->getPrivate();
            if (vxml->xml_class == JSXML_CLASS_LIST) {
                n = vxml->xml_kids.length;
                if (n == 0)
                    return JS_TRUE;
                for (j = 0; j < n; j++) {
                    kid = XMLARRAY_MEMBER(&vxml->xml_kids, j, JSXML);
                    if (!kid)
                        continue;
                    if (!CheckCycle(cx, xml, kid))
                        return JS_FALSE;
                }
            } else if (vxml->xml_class == JSXML_CLASS_ELEMENT) {
                /* OPTION: enforce that descendants have superset namespaces. */
                if (!CheckCycle(cx, xml, vxml))
                    return JS_FALSE;
            }
        }
    }
    if (!vxml) {
        str = js_ValueToString(cx, Valueify(v));
        if (!str)
            return JS_FALSE;

        vxml = js_NewXML(cx, JSXML_CLASS_TEXT);
        if (!vxml)
            return JS_FALSE;
        vxml->xml_value = str;
    }

    if (i > xml->xml_kids.length)
        i = xml->xml_kids.length;

    if (!XMLArrayInsert(cx, &xml->xml_kids, i, n))
        return JS_FALSE;

    if (vxml->xml_class == JSXML_CLASS_LIST) {
        for (j = 0; j < n; j++) {
            kid = XMLARRAY_MEMBER(&vxml->xml_kids, j, JSXML);
            if (!kid)
                continue;
            kid->parent = xml;
            XMLARRAY_SET_MEMBER(&xml->xml_kids, i + j, kid);

            /* OPTION: enforce that descendants have superset namespaces. */
        }
    } else {
        vxml->parent = xml;
        XMLARRAY_SET_MEMBER(&xml->xml_kids, i, vxml);
    }
    return JS_TRUE;
}

static JSBool
IndexToId(JSContext *cx, uint32 index, jsid *idp)
{
    JSAtom *atom;
    JSString *str;

    if (index <= JSID_INT_MAX) {
        *idp = INT_TO_JSID(index);
    } else {
        str = js_NumberToString(cx, (jsdouble) index);
        if (!str)
            return JS_FALSE;
        atom = js_AtomizeString(cx, str, 0);
        if (!atom)
            return JS_FALSE;
        *idp = ATOM_TO_JSID(atom);
    }
    return JS_TRUE;
}

/* ECMA-357 9.1.1.12 XML [[Replace]]. */
static JSBool
Replace(JSContext *cx, JSXML *xml, uint32 i, jsval v)
{
    uint32 n;
    JSXML *vxml, *kid;
    JSObject *vobj;
    JSString *str;

    if (!JSXML_HAS_KIDS(xml))
        return JS_TRUE;

    /*
     * 9.1.1.12
     * [[Replace]] handles _i >= x.[[Length]]_ by incrementing _x.[[Length]_.
     * It should therefore constrain callers to pass in _i <= x.[[Length]]_.
     */
    n = xml->xml_kids.length;
    if (i > n)
        i = n;

    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (vobj->isXML())
            vxml = (JSXML *) vobj->getPrivate();
    }

    switch (vxml ? JSXMLClass(vxml->xml_class) : JSXML_CLASS_LIMIT) {
      case JSXML_CLASS_ELEMENT:
        /* OPTION: enforce that descendants have superset namespaces. */
        if (!CheckCycle(cx, xml, vxml))
            return JS_FALSE;
      case JSXML_CLASS_COMMENT:
      case JSXML_CLASS_PROCESSING_INSTRUCTION:
      case JSXML_CLASS_TEXT:
        goto do_replace;

      case JSXML_CLASS_LIST:
        if (i < n)
            DeleteByIndex(cx, xml, i);
        if (!Insert(cx, xml, i, v))
            return JS_FALSE;
        break;

      default:
        str = js_ValueToString(cx, Valueify(v));
        if (!str)
            return JS_FALSE;

        vxml = js_NewXML(cx, JSXML_CLASS_TEXT);
        if (!vxml)
            return JS_FALSE;
        vxml->xml_value = str;

      do_replace:
        vxml->parent = xml;
        if (i < n) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid)
                kid->parent = NULL;
        }
        if (!XMLARRAY_ADD_MEMBER(cx, &xml->xml_kids, i, vxml))
            return JS_FALSE;
        break;
    }

    return JS_TRUE;
}

/* ECMA-357 9.1.1.3 XML [[Delete]], 9.2.1.3 XML [[Delete]] qname cases. */
static void
DeleteNamedProperty(JSContext *cx, JSXML *xml, JSObject *nameqn,
                    JSBool attributes)
{
    JSXMLArray *array;
    uint32 index, deleteCount;
    JSXML *kid;
    JSXMLNameMatcher matcher;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        array = &xml->xml_kids;
        for (index = 0; index < array->length; index++) {
            kid = XMLARRAY_MEMBER(array, index, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT)
                DeleteNamedProperty(cx, kid, nameqn, attributes);
        }
    } else if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        if (attributes) {
            array = &xml->xml_attrs;
            matcher = MatchAttrName;
        } else {
            array = &xml->xml_kids;
            matcher = MatchElemName;
        }
        deleteCount = 0;
        for (index = 0; index < array->length; index++) {
            kid = XMLARRAY_MEMBER(array, index, JSXML);
            if (kid && matcher(nameqn, kid)) {
                kid->parent = NULL;
                XMLArrayDelete(cx, array, index, JS_FALSE);
                ++deleteCount;
            } else if (deleteCount != 0) {
                XMLARRAY_SET_MEMBER(array,
                                    index - deleteCount,
                                    array->vector[index]);
            }
        }
        array->length -= deleteCount;
    }
}

/* ECMA-357 9.2.1.3 index case. */
static void
DeleteListElement(JSContext *cx, JSXML *xml, uint32 index)
{
    JSXML *kid, *parent;
    uint32 kidIndex;

    JS_ASSERT(xml->xml_class == JSXML_CLASS_LIST);

    if (index < xml->xml_kids.length) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
        if (kid) {
            parent = kid->parent;
            if (parent) {
                JS_ASSERT(parent != xml);
                JS_ASSERT(JSXML_HAS_KIDS(parent));

                if (kid->xml_class == JSXML_CLASS_ATTRIBUTE) {
                    DeleteNamedProperty(cx, parent, kid->name, JS_TRUE);
                } else {
                    kidIndex = XMLARRAY_FIND_MEMBER(&parent->xml_kids, kid,
                                                    NULL);
                    JS_ASSERT(kidIndex != XML_NOT_FOUND);
                    DeleteByIndex(cx, parent, kidIndex);
                }
            }
            XMLArrayDelete(cx, &xml->xml_kids, index, JS_TRUE);
        }
    }
}

static JSBool
SyncInScopeNamespaces(JSContext *cx, JSXML *xml)
{
    JSXMLArray *nsarray;
    uint32 i, n;
    JSObject *ns;

    nsarray = &xml->xml_namespaces;
    while ((xml = xml->parent) != NULL) {
        for (i = 0, n = xml->xml_namespaces.length; i < n; i++) {
            ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSObject);
            if (ns && !XMLARRAY_HAS_MEMBER(nsarray, ns, namespace_identity)) {
                if (!XMLARRAY_APPEND(cx, nsarray, ns))
                    return JS_FALSE;
            }
        }
    }
    return JS_TRUE;
}

static JSBool
GetNamedProperty(JSContext *cx, JSXML *xml, JSObject* nameqn, JSXML *list)
{
    JSXMLArray *array;
    JSXMLNameMatcher matcher;
    JSBool attrs;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        JSXMLArrayCursor cursor(&xml->xml_kids);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            if (kid->xml_class == JSXML_CLASS_ELEMENT &&
                !GetNamedProperty(cx, kid, nameqn, list)) {
                return JS_FALSE;
            }
        }
    } else if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        attrs = (nameqn->getClass() == &js_AttributeNameClass);
        if (attrs) {
            array = &xml->xml_attrs;
            matcher = MatchAttrName;
        } else {
            array = &xml->xml_kids;
            matcher = MatchElemName;
        }

        JSXMLArrayCursor cursor(array);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            if (matcher(nameqn, kid)) {
                if (!attrs &&
                    kid->xml_class == JSXML_CLASS_ELEMENT &&
                    !SyncInScopeNamespaces(cx, kid)) {
                    return JS_FALSE;
                }
                if (!Append(cx, list, kid))
                    return JS_FALSE;
            }
        }
    }

    return JS_TRUE;
}

/* ECMA-357 9.1.1.1 XML [[Get]] and 9.2.1.1 XMLList [[Get]]. */
static JSBool
GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSXML *xml, *list, *kid;
    uint32 index;
    JSObject *kidobj, *listobj;
    JSObject *nameqn;
    jsid funid;

    xml = (JSXML *) GetInstancePrivate(cx, obj, &js_XMLClass, NULL);
    if (!xml)
        return true;

    if (js_IdIsIndex(id, &index)) {
        if (!JSXML_HAS_KIDS(xml)) {
            *vp = (index == 0) ? OBJECT_TO_JSVAL(obj) : JSVAL_VOID;
        } else {
            /*
             * ECMA-357 9.2.1.1 starts here.
             *
             * Erratum: 9.2 is not completely clear that indexed properties
             * correspond to kids, but that's what it seems to say, and it's
             * what any sane user would want.
             */
            if (index < xml->xml_kids.length) {
                kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
                if (!kid) {
                    *vp = JSVAL_VOID;
                    return true;
                }
                kidobj = js_GetXMLObject(cx, kid);
                if (!kidobj)
                    return false;

                *vp = OBJECT_TO_JSVAL(kidobj);
            } else {
                *vp = JSVAL_VOID;
            }
        }
        return true;
    }

    /*
     * ECMA-357 9.2.1.1/9.1.1.1 qname case.
     */
    nameqn = ToXMLName(cx, IdToJsval(id), &funid);
    if (!nameqn)
        return false;
    if (!JSID_IS_VOID(funid))
        return GetXMLFunction(cx, obj, funid, vp);

    jsval roots[2] = { OBJECT_TO_JSVAL(nameqn), JSVAL_NULL };
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(roots), Valueify(roots));

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return false;

    roots[1] = OBJECT_TO_JSVAL(listobj);

    list = (JSXML *) listobj->getPrivate();
    if (!GetNamedProperty(cx, xml, nameqn, list))
        return false;

    /*
     * Erratum: ECMA-357 9.1.1.1 misses that [[Append]] sets the
     * given list's [[TargetProperty]] to the property that is being
     * appended. This means that any use of the internal [[Get]]
     * property returns a list which, when used by e.g. [[Insert]]
     * duplicates the last element matched by id. See bug 336921.
     */
    list->xml_target = xml;
    list->xml_targetprop = nameqn;
    *vp = OBJECT_TO_JSVAL(listobj);
    return true;
}

static JSXML *
CopyOnWrite(JSContext *cx, JSXML *xml, JSObject *obj)
{
    JS_ASSERT(xml->object != obj);

    xml = DeepCopy(cx, xml, obj, 0);
    if (!xml)
        return NULL;

    JS_ASSERT(xml->object == obj);
    return xml;
}

#define CHECK_COPY_ON_WRITE(cx,xml,obj)                                       \
    (xml->object == obj ? xml : CopyOnWrite(cx, xml, obj))

static JSString *
KidToString(JSContext *cx, JSXML *xml, uint32 index)
{
    JSXML *kid;
    JSObject *kidobj;

    kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
    if (!kid)
        return cx->runtime->emptyString;
    kidobj = js_GetXMLObject(cx, kid);
    if (!kidobj)
        return NULL;
    return js_ValueToString(cx, ObjectValue(*kidobj));
}

/* Forward declared -- its implementation uses other statics that call it. */
static JSBool
ResolveValue(JSContext *cx, JSXML *list, JSXML **result);

/* ECMA-357 9.1.1.2 XML [[Put]] and 9.2.1.2 XMLList [[Put]]. */
static JSBool
PutProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSBool ok, primitiveAssign;
    enum { OBJ_ROOT, ID_ROOT, VAL_ROOT };
    JSXML *xml, *vxml, *rxml, *kid, *attr, *parent, *copy, *kid2, *match;
    JSObject *vobj, *nameobj, *attrobj, *parentobj, *kidobj, *copyobj;
    JSObject *targetprop, *nameqn, *attrqn;
    uint32 index, i, j, k, n, q, matchIndex;
    jsval attrval, nsval;
    jsid funid;
    JSString *left, *right, *space, *uri;
    JSObject *ns;

    xml = (JSXML *) GetInstancePrivate(cx, obj, &js_XMLClass, NULL);
    if (!xml)
        return JS_TRUE;

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    /* Precompute vxml for 9.2.1.2 2(c)(vii)(2-3) and 2(d) and 9.1.1.2 1. */
    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(*vp)) {
        vobj = JSVAL_TO_OBJECT(*vp);
        if (vobj->isXML())
            vxml = (JSXML *) vobj->getPrivate();
    }

    ok = js_EnterLocalRootScope(cx);
    if (!ok)
        return JS_FALSE;

    MUST_FLOW_THROUGH("out");
    jsval roots[3];
    roots[OBJ_ROOT] = OBJECT_TO_JSVAL(obj);
    roots[ID_ROOT] = IdToJsval(id);
    roots[VAL_ROOT] = *vp;
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(roots), Valueify(roots));

    if (js_IdIsIndex(id, &index)) {
        if (xml->xml_class != JSXML_CLASS_LIST) {
            /* See NOTE in spec: this variation is reserved for future use. */
            ReportBadXMLName(cx, IdToValue(id));
            goto bad;
        }

        /*
         * Step 1 of ECMA-357 9.2.1.2 index case sets i to the property index.
         */
        i = index;

        /* 2(a-b). */
        if (xml->xml_target) {
            ok = ResolveValue(cx, xml->xml_target, &rxml);
            if (!ok)
                goto out;
            if (!rxml)
                goto out;
            JS_ASSERT(rxml->object);
        } else {
            rxml = NULL;
        }

        /* 2(c). */
        if (index >= xml->xml_kids.length) {
            /* 2(c)(i). */
            if (rxml) {
                if (rxml->xml_class == JSXML_CLASS_LIST) {
                    if (rxml->xml_kids.length != 1)
                        goto out;
                    rxml = XMLARRAY_MEMBER(&rxml->xml_kids, 0, JSXML);
                    if (!rxml)
                        goto out;
                    ok = js_GetXMLObject(cx, rxml) != NULL;
                    if (!ok)
                        goto out;
                }

                /*
                 * Erratum: ECMA-357 9.2.1.2 step 2(c)(ii) sets
                 * _y.[[Parent]] = r_ where _r_ is the result of
                 * [[ResolveValue]] called on _x.[[TargetObject]] in
                 * 2(a)(i).  This can result in text parenting text:
                 *
                 *    var MYXML = new XML();
                 *    MYXML.appendChild(new XML("<TEAM>Giants</TEAM>"));
                 *
                 * (testcase from Werner Sharp <wsharp@macromedia.com>).
                 *
                 * To match insertChildAfter, insertChildBefore,
                 * prependChild, and setChildren, we should silently
                 * do nothing in this case.
                 */
                if (!JSXML_HAS_KIDS(rxml))
                    goto out;
            }

            /* 2(c)(ii) is distributed below as several js_NewXML calls. */
            targetprop = xml->xml_targetprop;
            if (!targetprop || IS_STAR(GetLocalName(targetprop))) {
                /* 2(c)(iv)(1-2), out of order w.r.t. 2(c)(iii). */
                kid = js_NewXML(cx, JSXML_CLASS_TEXT);
                if (!kid)
                    goto bad;
            } else {
                nameobj = targetprop;
                if (nameobj->getClass() == &js_AttributeNameClass) {
                    /*
                     * 2(c)(iii)(1-3).
                     * Note that rxml can't be null here, because target
                     * and targetprop are non-null.
                     */
                    ok = GetProperty(cx, rxml->object, id, &attrval);
                    if (!ok)
                        goto out;
                    if (JSVAL_IS_PRIMITIVE(attrval))    /* no such attribute */
                        goto out;
                    attrobj = JSVAL_TO_OBJECT(attrval);
                    attr = (JSXML *) attrobj->getPrivate();
                    if (JSXML_LENGTH(attr) != 0)
                        goto out;

                    kid = js_NewXML(cx, JSXML_CLASS_ATTRIBUTE);
                } else {
                    /* 2(c)(v). */
                    kid = js_NewXML(cx, JSXML_CLASS_ELEMENT);
                }
                if (!kid)
                    goto bad;

                /* An important bit of 2(c)(ii). */
                kid->name = targetprop;
            }

            /* Final important bit of 2(c)(ii). */
            kid->parent = rxml;

            /* 2(c)(vi-vii). */
            i = xml->xml_kids.length;
            if (kid->xml_class != JSXML_CLASS_ATTRIBUTE) {
                /*
                 * 2(c)(vii)(1) tests whether _y.[[Parent]]_ is not null.
                 * y.[[Parent]] is here called kid->parent, which we know
                 * from 2(c)(ii) is _r_, here called rxml.  So let's just
                 * test that!  Erratum, the spec should be simpler here.
                 */
                if (rxml) {
                    JS_ASSERT(JSXML_HAS_KIDS(rxml));
                    n = rxml->xml_kids.length;
                    j = n - 1;
                    if (n != 0 && i != 0) {
                        for (n = j, j = 0; j < n; j++) {
                            if (rxml->xml_kids.vector[j] ==
                                xml->xml_kids.vector[i-1]) {
                                break;
                            }
                        }
                    }

                    kidobj = js_GetXMLObject(cx, kid);
                    if (!kidobj)
                        goto bad;
                    ok = Insert(cx, rxml, j + 1, OBJECT_TO_JSVAL(kidobj));
                    if (!ok)
                        goto out;
                }

                /*
                 * 2(c)(vii)(2-3).
                 * Erratum: [[PropertyName]] in 2(c)(vii)(3) must be a
                 * typo for [[TargetProperty]].
                 */
                if (vxml) {
                    kid->name = (vxml->xml_class == JSXML_CLASS_LIST)
                        ? vxml->xml_targetprop
                        : vxml->name;
                }
            }

            /* 2(c)(viii). */
            ok = Append(cx, xml, kid);
            if (!ok)
                goto out;
        }

        /* 2(d). */
        if (!vxml ||
            vxml->xml_class == JSXML_CLASS_TEXT ||
            vxml->xml_class == JSXML_CLASS_ATTRIBUTE) {
            ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
            if (!ok)
                goto out;
            roots[VAL_ROOT] = *vp;
        }

        /* 2(e). */
        kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
        if (!kid)
            goto out;
        parent = kid->parent;
        if (kid->xml_class == JSXML_CLASS_ATTRIBUTE) {
            nameobj = kid->name;
            if (nameobj->getClass() != &js_AttributeNameClass) {
                nameobj = NewXMLQName(cx, GetURI(nameobj), GetPrefix(nameobj),
                                      GetLocalName(nameobj),
                                      &js_AttributeNameClass);
                if (!nameobj)
                    goto bad;
            }
            id = OBJECT_TO_JSID(nameobj);

            if (parent) {
                /* 2(e)(i). */
                parentobj = js_GetXMLObject(cx, parent);
                if (!parentobj)
                    goto bad;
                ok = PutProperty(cx, parentobj, id, vp);
                if (!ok)
                    goto out;

                /* 2(e)(ii). */
                ok = GetProperty(cx, parentobj, id, vp);
                if (!ok)
                    goto out;
                attr = (JSXML *) JSVAL_TO_OBJECT(*vp)->getPrivate();

                /* 2(e)(iii) - the length check comes from the bug 375406. */
                if (attr->xml_kids.length != 0)
                    xml->xml_kids.vector[i] = attr->xml_kids.vector[0];
            }
        }

        /* 2(f). */
        else if (vxml && vxml->xml_class == JSXML_CLASS_LIST) {
            /*
             * 2(f)(i)
             *
             * Erratum: the spec says to create a shallow copy _c_ of _V_, but
             * if we do that we never change the parent of each child in the
             * list.  Since [[Put]] when called on an XML object deeply copies
             * the provided list _V_, we also do so here.  Perhaps the shallow
             * copy was a misguided optimization?
             */
            copy = DeepCopyInLRS(cx, vxml, 0);
            if (!copy)
                goto bad;
            copyobj = js_GetXMLObject(cx, copy);
            if (!copyobj)
                goto bad;

            JS_ASSERT(parent != xml);
            if (parent) {
                q = XMLARRAY_FIND_MEMBER(&parent->xml_kids, kid, NULL);
                JS_ASSERT(q != XML_NOT_FOUND);
                ok = Replace(cx, parent, q, OBJECT_TO_JSVAL(copyobj));
                if (!ok)
                    goto out;

#ifdef DEBUG
                /* Erratum: this loop in the spec is useless. */
                for (j = 0, n = copy->xml_kids.length; j < n; j++) {
                    kid2 = XMLARRAY_MEMBER(&parent->xml_kids, q + j, JSXML);
                    JS_ASSERT(XMLARRAY_MEMBER(&copy->xml_kids, j, JSXML)
                              == kid2);
                }
#endif
            }

            /*
             * 2(f)(iv-vi).
             * Erratum: notice the unhandled zero-length V basis case and
             * the off-by-one errors for the n != 0 cases in the spec.
             */
            n = copy->xml_kids.length;
            if (n == 0) {
                XMLArrayDelete(cx, &xml->xml_kids, i, JS_TRUE);
            } else {
                ok = XMLArrayInsert(cx, &xml->xml_kids, i + 1, n - 1);
                if (!ok)
                    goto out;

                for (j = 0; j < n; j++)
                    xml->xml_kids.vector[i + j] = copy->xml_kids.vector[j];
            }
        }

        /* 2(g). */
        else if (vxml || JSXML_HAS_VALUE(kid)) {
            if (parent) {
                q = XMLARRAY_FIND_MEMBER(&parent->xml_kids, kid, NULL);
                JS_ASSERT(q != XML_NOT_FOUND);
                ok = Replace(cx, parent, q, *vp);
                if (!ok)
                    goto out;

                vxml = XMLARRAY_MEMBER(&parent->xml_kids, q, JSXML);
                if (!vxml)
                    goto out;
                roots[VAL_ROOT] = *vp = OBJECT_TO_JSVAL(vxml->object);
            }

            /*
             * 2(g)(iii).
             * Erratum: _V_ may not be of type XML, but all index-named
             * properties _x[i]_ in an XMLList _x_ must be of type XML,
             * according to 9.2.1.1 Overview and other places in the spec.
             *
             * Thanks to 2(d), we know _V_ (*vp here) is either a string
             * or an XML/XMLList object.  If *vp is a string, call ToXML
             * on it to satisfy the constraint.
             */
            if (!vxml) {
                JS_ASSERT(JSVAL_IS_STRING(*vp));
                vobj = ToXML(cx, *vp);
                if (!vobj)
                    goto bad;
                roots[VAL_ROOT] = *vp = OBJECT_TO_JSVAL(vobj);
                vxml = (JSXML *) vobj->getPrivate();
            }
            XMLARRAY_SET_MEMBER(&xml->xml_kids, i, vxml);
        }

        /* 2(h). */
        else {
            kidobj = js_GetXMLObject(cx, kid);
            if (!kidobj)
                goto bad;
            id = ATOM_TO_JSID(cx->runtime->atomState.starAtom);
            ok = PutProperty(cx, kidobj, id, vp);
            if (!ok)
                goto out;
        }
    } else {
        /*
         * ECMA-357 9.2.1.2/9.1.1.2 qname case.
         */
        nameqn = ToXMLName(cx, IdToJsval(id), &funid);
        if (!nameqn)
            goto bad;
        if (!JSID_IS_VOID(funid)) {
            ok = js_SetProperty(cx, obj, funid, Valueify(vp), false);
            goto out;
        }
        nameobj = nameqn;
        roots[ID_ROOT] = OBJECT_TO_JSVAL(nameobj);

        if (xml->xml_class == JSXML_CLASS_LIST) {
            /*
             * Step 3 of 9.2.1.2.
             * Erratum: if x.[[Length]] > 1 or [[ResolveValue]] returns null
             * or an r with r.[[Length]] != 1, throw TypeError.
             */
            n = JSXML_LENGTH(xml);
            if (n > 1)
                goto type_error;
            if (n == 0) {
                ok = ResolveValue(cx, xml, &rxml);
                if (!ok)
                    goto out;
                if (!rxml || JSXML_LENGTH(rxml) != 1)
                    goto type_error;
                ok = Append(cx, xml, rxml);
                if (!ok)
                    goto out;
            }
            JS_ASSERT(JSXML_LENGTH(xml) == 1);
            xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
            if (!xml)
                goto out;
            JS_ASSERT(xml->xml_class != JSXML_CLASS_LIST);
            obj = js_GetXMLObject(cx, xml);
            if (!obj)
                goto bad;
            roots[OBJ_ROOT] = OBJECT_TO_JSVAL(obj);

            /* FALL THROUGH to non-list case */
        }

        /*
         * ECMA-357 9.1.1.2.
         * Erratum: move steps 3 and 4 to before 1 and 2, to avoid wasted
         * effort in ToString or [[DeepCopy]].
         */

        if (JSXML_HAS_VALUE(xml))
            goto out;

        if (!vxml ||
            vxml->xml_class == JSXML_CLASS_TEXT ||
            vxml->xml_class == JSXML_CLASS_ATTRIBUTE) {
            ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
            if (!ok)
                goto out;
        } else {
            rxml = DeepCopyInLRS(cx, vxml, 0);
            if (!rxml || !js_GetXMLObject(cx, rxml))
                goto bad;
            vxml = rxml;
            *vp = OBJECT_TO_JSVAL(vxml->object);
        }
        roots[VAL_ROOT] = *vp;

        /*
         * 6.
         * Erratum: why is this done here, so early? use is way later....
         */
        ok = js_GetDefaultXMLNamespace(cx, &nsval);
        if (!ok)
            goto out;

        if (nameobj->getClass() == &js_AttributeNameClass) {
            /* 7(a). */
            if (!js_IsXMLName(cx, OBJECT_TO_JSVAL(nameobj)))
                goto out;

            /* 7(b-c). */
            if (vxml && vxml->xml_class == JSXML_CLASS_LIST) {
                n = vxml->xml_kids.length;
                if (n == 0) {
                    *vp = STRING_TO_JSVAL(cx->runtime->emptyString);
                } else {
                    left = KidToString(cx, vxml, 0);
                    if (!left)
                        goto bad;

                    space = ATOM_TO_STRING(cx->runtime->atomState.spaceAtom);
                    for (i = 1; i < n; i++) {
                        left = js_ConcatStrings(cx, left, space);
                        if (!left)
                            goto bad;
                        right = KidToString(cx, vxml, i);
                        if (!right)
                            goto bad;
                        left = js_ConcatStrings(cx, left, right);
                        if (!left)
                            goto bad;
                    }

                    roots[VAL_ROOT] = *vp = STRING_TO_JSVAL(left);
                }
            } else {
                ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
                if (!ok)
                    goto out;
                roots[VAL_ROOT] = *vp;
            }

            /* 7(d-e). */
            match = NULL;
            for (i = 0, n = xml->xml_attrs.length; i < n; i++) {
                attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
                if (!attr)
                    continue;
                attrqn = attr->name;
                if (js_EqualStrings(GetLocalName(attrqn),
                                    GetLocalName(nameqn))) {
                    uri = GetURI(nameqn);
                    if (!uri || js_EqualStrings(GetURI(attrqn), uri)) {
                        if (!match) {
                            match = attr;
                        } else {
                            DeleteNamedProperty(cx, xml, attrqn, JS_TRUE);
                            --i;
                        }
                    }
                }
            }

            /* 7(f). */
            attr = match;
            if (!attr) {
                /* 7(f)(i-ii). */
                uri = GetURI(nameqn);
                if (!uri) {
                    left = right = cx->runtime->emptyString;
                } else {
                    left = uri;
                    right = GetPrefix(nameqn);
                }
                nameqn = NewXMLQName(cx, left, right, GetLocalName(nameqn));
                if (!nameqn)
                    goto bad;

                /* 7(f)(iii). */
                attr = js_NewXML(cx, JSXML_CLASS_ATTRIBUTE);
                if (!attr)
                    goto bad;
                attr->parent = xml;
                attr->name = nameqn;

                /* 7(f)(iv). */
                ok = XMLARRAY_ADD_MEMBER(cx, &xml->xml_attrs, n, attr);
                if (!ok)
                    goto out;

                /* 7(f)(v-vi). */
                ns = GetNamespace(cx, nameqn, NULL);
                if (!ns)
                    goto bad;
                ok = AddInScopeNamespace(cx, xml, ns);
                if (!ok)
                    goto out;
            }

            /* 7(g). */
            attr->xml_value = JSVAL_TO_STRING(*vp);
            goto out;
        }

        /* 8-9. */
        if (!js_IsXMLName(cx, OBJECT_TO_JSVAL(nameobj)) &&
            !IS_STAR(GetLocalName(nameqn))) {
            goto out;
        }

        /* 10-11. */
        id = JSID_VOID;
        primitiveAssign = !vxml && !IS_STAR(GetLocalName(nameqn));

        /* 12. */
        k = n = xml->xml_kids.length;
        matchIndex = XML_NOT_FOUND;
        kid2 = NULL;
        while (k != 0) {
            --k;
            kid = XMLARRAY_MEMBER(&xml->xml_kids, k, JSXML);
            if (kid && MatchElemName(nameqn, kid)) {
                if (matchIndex != XML_NOT_FOUND)
                    DeleteByIndex(cx, xml, matchIndex);
                matchIndex = k;
                kid2 = kid;
            }
        }

        /*
         * Erratum: ECMA-357 specified child insertion inconsistently:
         * insertChildBefore and insertChildAfter insert an arbitrary XML
         * instance, and therefore can create cycles, but appendChild as
         * specified by the "Overview" of 13.4.4.3 calls [[DeepCopy]] on
         * its argument.  But the "Semantics" in 13.4.4.3 do not include
         * any [[DeepCopy]] call.
         *
         * Fixing this (https://bugzilla.mozilla.org/show_bug.cgi?id=312692)
         * required adding cycle detection, and allowing duplicate kids to
         * be created (see comment 6 in the bug).  Allowing duplicate kid
         * references means the loop above will delete all but the lowest
         * indexed reference, and each [[DeleteByIndex]] nulls the kid's
         * parent.  Thus the need to restore parent here.  This is covered
         * by https://bugzilla.mozilla.org/show_bug.cgi?id=327564.
         */
        if (kid2) {
            JS_ASSERT(kid2->parent == xml || !kid2->parent);
            if (!kid2->parent)
                kid2->parent = xml;
        }

        /* 13. */
        if (matchIndex == XML_NOT_FOUND) {
            /* 13(a). */
            matchIndex = n;

            /* 13(b). */
            if (primitiveAssign) {
                uri = GetURI(nameqn);
                if (!uri) {
                    ns = JSVAL_TO_OBJECT(nsval);
                    left = GetURI(ns);
                    right = GetPrefix(ns);
                } else {
                    left = uri;
                    right = GetPrefix(nameqn);
                }
                nameqn = NewXMLQName(cx, left, right, GetLocalName(nameqn));
                if (!nameqn)
                    goto bad;

                /* 13(b)(iii). */
                vobj = js_NewXMLObject(cx, JSXML_CLASS_ELEMENT);
                if (!vobj)
                    goto bad;
                vxml = (JSXML *) vobj->getPrivate();
                vxml->parent = xml;
                vxml->name = nameqn;

                /* 13(b)(iv-vi). */
                ns = GetNamespace(cx, nameqn, NULL);
                if (!ns)
                    goto bad;
                ok = Replace(cx, xml, matchIndex, OBJECT_TO_JSVAL(vobj));
                if (!ok)
                    goto out;
                ok = AddInScopeNamespace(cx, vxml, ns);
                if (!ok)
                    goto out;
            }
        }

        /* 14. */
        if (primitiveAssign) {
            JSXMLArrayCursor cursor(&xml->xml_kids);
            cursor.index = matchIndex;
            kid = (JSXML *) cursor.getCurrent();
            if (JSXML_HAS_KIDS(kid)) {
                kid->xml_kids.finish(cx);
                kid->xml_kids.init();
                ok = kid->xml_kids.setCapacity(cx, 1);
            }

            /* 14(b-c). */
            /* XXXbe Erratum? redundant w.r.t. 7(b-c) else clause above */
            if (ok) {
                ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
                if (ok && !JSVAL_TO_STRING(*vp)->empty()) {
                    roots[VAL_ROOT] = *vp;
                    if ((JSXML *) cursor.getCurrent() == kid)
                        ok = Replace(cx, kid, 0, *vp);
                }
            }
        } else {
            /* 15(a). */
            ok = Replace(cx, xml, matchIndex, *vp);
        }
    }

out:
    js_LeaveLocalRootScope(cx);
    return ok;

type_error:
    {
        JSAutoByteString bytes;
        if (js_ValueToPrintable(cx, IdToValue(id), &bytes))
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_XMLLIST_PUT, bytes.ptr());
    }
bad:
    ok = JS_FALSE;
    goto out;
}

/* ECMA-357 9.1.1.10 XML [[ResolveValue]], 9.2.1.10 XMLList [[ResolveValue]]. */
static JSBool
ResolveValue(JSContext *cx, JSXML *list, JSXML **result)
{
    JSXML *target, *base;
    JSObject *targetprop;
    jsid id;
    jsval tv;

    if (list->xml_class != JSXML_CLASS_LIST || list->xml_kids.length != 0) {
        if (!js_GetXMLObject(cx, list))
            return JS_FALSE;
        *result = list;
        return JS_TRUE;
    }

    target = list->xml_target;
    targetprop = list->xml_targetprop;
    if (!target || !targetprop || IS_STAR(GetLocalName(targetprop))) {
        *result = NULL;
        return JS_TRUE;
    }

    if (targetprop->getClass() == &js_AttributeNameClass) {
        *result = NULL;
        return JS_TRUE;
    }

    if (!ResolveValue(cx, target, &base))
        return JS_FALSE;
    if (!base) {
        *result = NULL;
        return JS_TRUE;
    }
    if (!js_GetXMLObject(cx, base))
        return JS_FALSE;

    id = OBJECT_TO_JSID(targetprop);
    if (!GetProperty(cx, base->object, id, &tv))
        return JS_FALSE;
    target = (JSXML *) JSVAL_TO_OBJECT(tv)->getPrivate();

    if (JSXML_LENGTH(target) == 0) {
        if (base->xml_class == JSXML_CLASS_LIST && JSXML_LENGTH(base) > 1) {
            *result = NULL;
            return JS_TRUE;
        }
        tv = STRING_TO_JSVAL(cx->runtime->emptyString);
        if (!PutProperty(cx, base->object, id, &tv))
            return JS_FALSE;
        if (!GetProperty(cx, base->object, id, &tv))
            return JS_FALSE;
        target = (JSXML *) JSVAL_TO_OBJECT(tv)->getPrivate();
    }

    *result = target;
    return JS_TRUE;
}

static JSBool
HasNamedProperty(JSXML *xml, JSObject *nameqn)
{
    JSBool found;
    JSXMLArray *array;
    JSXMLNameMatcher matcher;
    uint32 i, n;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        found = JS_FALSE;
        JSXMLArrayCursor cursor(&xml->xml_kids);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            found = HasNamedProperty(kid, nameqn);
            if (found)
                break;
        }
        return found;
    }

    if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        if (nameqn->getClass() == &js_AttributeNameClass) {
            array = &xml->xml_attrs;
            matcher = MatchAttrName;
        } else {
            array = &xml->xml_kids;
            matcher = MatchElemName;
        }
        for (i = 0, n = array->length; i < n; i++) {
            JSXML *kid = XMLARRAY_MEMBER(array, i, JSXML);
            if (kid && matcher(nameqn, kid))
                return JS_TRUE;
        }
    }

    return JS_FALSE;
}

static JSBool
HasIndexedProperty(JSXML *xml, uint32 i)
{
    if (xml->xml_class == JSXML_CLASS_LIST)
        return i < JSXML_LENGTH(xml);

    if (xml->xml_class == JSXML_CLASS_ELEMENT)
        return i == 0;

    return JS_FALSE;
}

static JSBool
HasSimpleContent(JSXML *xml);

static JSBool
HasFunctionProperty(JSContext *cx, JSObject *obj, jsid funid, JSBool *found)
{
    JSObject *pobj;
    JSProperty *prop;
    JSXML *xml;

    JS_ASSERT(obj->getClass() == &js_XMLClass);

    if (!js_LookupProperty(cx, obj, funid, &pobj, &prop))
        return false;
    if (!prop) {
        xml = (JSXML *) obj->getPrivate();
        if (HasSimpleContent(xml)) {
            AutoObjectRooter tvr(cx);

            /*
             * Search in String.prototype to set found whenever
             * GetXMLFunction returns existing function.
             */
            if (!js_GetClassPrototype(cx, NULL, JSProto_String, tvr.addr()))
                return false;

            JS_ASSERT(tvr.object());
            if (!js_LookupProperty(cx, tvr.object(), funid, &pobj, &prop))
                return false;
        }
    }
    *found = (prop != NULL);
    return true;
}

/* ECMA-357 9.1.1.6 XML [[HasProperty]] and 9.2.1.5 XMLList [[HasProperty]]. */
static JSBool
HasProperty(JSContext *cx, JSObject *obj, jsval id, JSBool *found)
{
    JSXML *xml;
    uint32 i;
    JSObject *qn;
    jsid funid;

    xml = (JSXML *) obj->getPrivate();
    if (js_IdValIsIndex(id, &i)) {
        *found = HasIndexedProperty(xml, i);
    } else {
        qn = ToXMLName(cx, id, &funid);
        if (!qn)
            return JS_FALSE;
        if (!JSID_IS_VOID(funid)) {
            if (!HasFunctionProperty(cx, obj, funid, found))
                return JS_FALSE;
        } else {
            *found = HasNamedProperty(xml, qn);
        }
    }
    return JS_TRUE;
}

static void
xml_finalize(JSContext *cx, JSObject *obj)
{
    JSXML *xml = (JSXML *) obj->getPrivate();
    if (!xml)
        return;
    if (xml->object == obj)
        xml->object = NULL;
}

static void
xml_trace_vector(JSTracer *trc, JSXML **vec, uint32 len)
{
    uint32 i;
    JSXML *xml;

    for (i = 0; i < len; i++) {
        xml = vec[i];
        if (xml) {
            JS_SET_TRACING_INDEX(trc, "xml_vector", i);
            Mark(trc, xml);
        }
    }
}

/*
 * XML objects are native. Thus xml_lookupProperty must return a valid
 * Shape pointer parameter via *propp to signify "property found". Since the
 * only call to xml_lookupProperty is via JSObject::lookupProperty, and then
 * only from js_FindProperty (in jsobj.c, called from jsinterp.c) or from
 * JSOP_IN case in the interpreter, the only time we add a Shape here is when
 * an unqualified name is being accessed or when "name in xml" is called.
 *
 * This scope property keeps the JSOP_NAME code in js_Interpret happy by
 * giving it an shape with (getter, setter) == (GetProperty, PutProperty).
 *
 * NB: xml_deleteProperty must take care to remove any property added here.
 *
 * FIXME This clashes with the function namespace implementation which also
 * uses native properties. Effectively after xml_lookupProperty any property
 * stored previously using assignments to xml.function::name will be removed.
 * We partially workaround the problem in GetXMLFunction. There we take
 * advantage of the fact that typically function:: is used to access the
 * functions from XML.prototype. So when js_GetProperty returns a non-function
 * property, we assume that it represents the result of GetProperty setter
 * hiding the function and use an extra prototype chain lookup to recover it.
 * For a proper solution see bug 355257.
*/
static JSBool
xml_lookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                   JSProperty **propp)
{
    JSBool found;
    JSXML *xml;
    uint32 i;
    JSObject *qn;
    jsid funid;

    xml = (JSXML *) obj->getPrivate();
    if (js_IdIsIndex(id, &i)) {
        found = HasIndexedProperty(xml, i);
    } else {
        qn = ToXMLName(cx, IdToJsval(id), &funid);
        if (!qn)
            return JS_FALSE;
        if (!JSID_IS_VOID(funid))
            return js_LookupProperty(cx, obj, funid, objp, propp);
        found = HasNamedProperty(xml, qn);
    }
    if (!found) {
        *objp = NULL;
        *propp = NULL;
    } else {
        const Shape *shape =
            js_AddNativeProperty(cx, obj, id,
                                 Valueify(GetProperty), Valueify(PutProperty),
                                 SHAPE_INVALID_SLOT, JSPROP_ENUMERATE,
                                 0, 0);
        if (!shape)
            return JS_FALSE;

        *objp = obj;
        *propp = (JSProperty *) shape;
    }
    return JS_TRUE;
}

static JSBool
xml_defineProperty(JSContext *cx, JSObject *obj, jsid id, const Value *v,
                   PropertyOp getter, PropertyOp setter, uintN attrs)
{
    if (IsFunctionObject(*v) || getter || setter ||
        (attrs & JSPROP_ENUMERATE) == 0 ||
        (attrs & (JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED))) {
        return js_DefineProperty(cx, obj, id, v, getter, setter, attrs);
    }

    jsval tmp = Jsvalify(*v);
    return PutProperty(cx, obj, id, &tmp);
}

static JSBool
xml_getProperty(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    if (JSID_IS_DEFAULT_XML_NAMESPACE(id)) {
        vp->setUndefined();
        return JS_TRUE;
    }

    return GetProperty(cx, obj, id, Jsvalify(vp));
}

static JSBool
xml_setProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    return PutProperty(cx, obj, id, Jsvalify(vp));
}

static JSBool
xml_getAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    JSBool found;
    if (!HasProperty(cx, obj, IdToJsval(id), &found))
        return false;

    *attrsp = found ? JSPROP_ENUMERATE : 0;
    return JS_TRUE;
}

static JSBool
xml_setAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    JSBool found;
    if (!HasProperty(cx, obj, IdToJsval(id), &found))
        return false;

    if (found) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_SET_XML_ATTRS);
        return false;
    }
    return true;
}

static JSBool
xml_deleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    JSXML *xml;
    jsval idval;
    uint32 index;
    JSObject *nameqn;
    jsid funid;

    idval = IdToJsval(id);
    xml = (JSXML *) obj->getPrivate();
    if (js_IdIsIndex(id, &index)) {
        if (xml->xml_class != JSXML_CLASS_LIST) {
            /* See NOTE in spec: this variation is reserved for future use. */
            ReportBadXMLName(cx, IdToValue(id));
            return false;
        }

        /* ECMA-357 9.2.1.3. */
        DeleteListElement(cx, xml, index);
    } else {
        nameqn = ToXMLName(cx, idval, &funid);
        if (!nameqn)
            return false;
        if (!JSID_IS_VOID(funid))
            return js_DeleteProperty(cx, obj, funid, rval, false);

        DeleteNamedProperty(cx, xml, nameqn,
                            nameqn->getClass() == &js_AttributeNameClass);
    }

    /*
     * If this object has its own (mutable) scope,  then we may have added a
     * property to the scope in xml_lookupProperty for it to return to mean
     * "found" and to provide a handle for access operations to call the
     * property's getter or setter. But now it's time to remove any such
     * property, to purge the property cache and remove the scope entry.
     */
    if (!obj->nativeEmpty() && !js_DeleteProperty(cx, obj, id, rval, false))
        return false;

    rval->setBoolean(true);
    return true;
}

JSBool
xml_convert(JSContext *cx, JSObject *obj, JSType type, Value *rval)
{
    return js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL, rval);
}

static JSBool
xml_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op, Value *statep, jsid *idp)
{
    JSXML *xml;
    uint32 length, index;
    JSXMLArrayCursor *cursor;

    xml = (JSXML *)obj->getPrivate();
    length = JSXML_LENGTH(xml);

    switch (enum_op) {
      case JSENUMERATE_INIT:
      case JSENUMERATE_INIT_ALL:
        if (length == 0) {
            statep->setInt32(0);
        } else {
            cursor = cx->create<JSXMLArrayCursor>(&xml->xml_kids);
            if (!cursor)
                return JS_FALSE;
            statep->setPrivate(cursor);
        }
        if (idp)
            *idp = INT_TO_JSID(length);
        break;

      case JSENUMERATE_NEXT:
        if (statep->isInt32(0)) {
            statep->setNull();
            break;
        }
        cursor = (JSXMLArrayCursor *) statep->toPrivate();
        if (cursor && cursor->array && (index = cursor->index) < length) {
            *idp = INT_TO_JSID(index);
            cursor->index = index + 1;
            break;
        }
        /* FALL THROUGH */

      case JSENUMERATE_DESTROY:
        if (!statep->isInt32(0)) {
            cursor = (JSXMLArrayCursor *) statep->toPrivate();
            if (cursor)
                cx->destroy(cursor);
        }
        statep->setNull();
        break;
    }
    return JS_TRUE;
}

static JSType
xml_typeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_XML;
}

static JSBool
xml_hasInstance(JSContext *cx, JSObject *obj, const Value *, JSBool *bp)
{
    return JS_TRUE;
}

static void
xml_trace(JSTracer *trc, JSObject *obj)
{
    JSXML *xml = (JSXML *) obj->getPrivate();
    if (xml)
        JS_CALL_TRACER(trc, xml, JSTRACE_XML, "private");
}

static JSBool
xml_fix(JSContext *cx, JSObject *obj, bool *success, AutoIdVector *props)
{
    JS_ASSERT(obj->isExtensible());
    *success = false;
    return true;
}

static void
xml_clear(JSContext *cx, JSObject *obj)
{
}

static JSBool
HasSimpleContent(JSXML *xml)
{
    JSXML *kid;
    JSBool simple;
    uint32 i, n;

again:
    switch (xml->xml_class) {
      case JSXML_CLASS_COMMENT:
      case JSXML_CLASS_PROCESSING_INSTRUCTION:
        return JS_FALSE;
      case JSXML_CLASS_LIST:
        if (xml->xml_kids.length == 0)
            return JS_TRUE;
        if (xml->xml_kids.length == 1) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
            if (kid) {
                xml = kid;
                goto again;
            }
        }
        /* FALL THROUGH */
      default:
        simple = JS_TRUE;
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT) {
                simple = JS_FALSE;
                break;
            }
        }
        return simple;
    }
}

/*
 * 11.2.2.1 Step 3(d) onward.
 */
JSBool
js_GetXMLMethod(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT(JS_InstanceOf(cx, obj, Jsvalify(&js_XMLClass), NULL));

    if (JSID_IS_OBJECT(id)) {
        jsid funid;

        if (!js_IsFunctionQName(cx, JSID_TO_OBJECT(id), &funid))
            return JS_FALSE;
        if (!JSID_IS_VOID(funid))
            id = funid;
    }

    /*
     * As our callers have a bad habit of passing a pointer to an unrooted
     * local value as vp, we use a proper root here.
     */
    AutoValueRooter tvr(cx);
    JSBool ok = GetXMLFunction(cx, obj, id, Jsvalify(tvr.addr()));
    *vp = tvr.value();
    return ok;
}

JSBool
js_TestXMLEquality(JSContext *cx, const Value &v1, const Value &v2, JSBool *bp)
{
    JSXML *xml, *vxml;
    JSObject *vobj;
    JSBool ok;
    JSString *str, *vstr;
    jsdouble d, d2;

    JSObject *obj;
    jsval v;
    if (v1.isObject() && v1.toObject().isXML()) {
        obj = &v1.toObject();
        v = Jsvalify(v2);
    } else {
        v = Jsvalify(v1);
        obj = &v2.toObject();
    }

    JS_ASSERT(JS_InstanceOf(cx, obj, Jsvalify(&js_XMLClass), NULL));

    xml = (JSXML *) obj->getPrivate();
    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (vobj->isXML())
            vxml = (JSXML *) vobj->getPrivate();
    }

    if (xml->xml_class == JSXML_CLASS_LIST) {
        ok = Equals(cx, xml, v, bp);
    } else if (vxml) {
        if (vxml->xml_class == JSXML_CLASS_LIST) {
            ok = Equals(cx, vxml, OBJECT_TO_JSVAL(obj), bp);
        } else {
            if (((xml->xml_class == JSXML_CLASS_TEXT ||
                  xml->xml_class == JSXML_CLASS_ATTRIBUTE) &&
                 HasSimpleContent(vxml)) ||
                ((vxml->xml_class == JSXML_CLASS_TEXT ||
                  vxml->xml_class == JSXML_CLASS_ATTRIBUTE) &&
                 HasSimpleContent(xml))) {
                ok = js_EnterLocalRootScope(cx);
                if (ok) {
                    ok = (str = js_ValueToString(cx, ObjectValue(*obj))) &&
                         (vstr = js_ValueToString(cx, Valueify(v)));
                    if (ok)
                        *bp = js_EqualStrings(str, vstr);
                    js_LeaveLocalRootScope(cx);
                }
            } else {
                ok = XMLEquals(cx, xml, vxml, bp);
            }
        }
    } else {
        ok = js_EnterLocalRootScope(cx);
        if (ok) {
            if (HasSimpleContent(xml)) {
                ok = (str = js_ValueToString(cx, ObjectValue(*obj))) &&
                     (vstr = js_ValueToString(cx, Valueify(v)));
                if (ok)
                    *bp = js_EqualStrings(str, vstr);
            } else if (JSVAL_IS_STRING(v) || JSVAL_IS_NUMBER(v)) {
                str = js_ValueToString(cx, ObjectValue(*obj));
                if (!str) {
                    ok = JS_FALSE;
                } else if (JSVAL_IS_STRING(v)) {
                    *bp = js_EqualStrings(str, JSVAL_TO_STRING(v));
                } else {
                    ok = JS_ValueToNumber(cx, STRING_TO_JSVAL(str), &d);
                    if (ok) {
                        d2 = JSVAL_IS_INT(v) ? JSVAL_TO_INT(v)
                                             : JSVAL_TO_DOUBLE(v);
                        *bp = JSDOUBLE_COMPARE(d, ==, d2, JS_FALSE);
                    }
                }
            } else {
                *bp = JS_FALSE;
            }
            js_LeaveLocalRootScope(cx);
        }
    }
    return ok;
}

JSBool
js_ConcatenateXML(JSContext *cx, JSObject *obj, JSObject *robj, Value *vp)
{
    JSBool ok;
    JSObject *listobj;
    JSXML *list, *lxml, *rxml;

    JS_ASSERT(JS_InstanceOf(cx, obj, Jsvalify(&js_XMLClass), NULL));
    ok = js_EnterLocalRootScope(cx);
    if (!ok)
        return JS_FALSE;

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj) {
        ok = JS_FALSE;
        goto out;
    }

    list = (JSXML *) listobj->getPrivate();
    lxml = (JSXML *) obj->getPrivate();
    ok = Append(cx, list, lxml);
    if (!ok)
        goto out;

    JS_ASSERT(robj->isXML());
    rxml = (JSXML *) robj->getPrivate();
    ok = Append(cx, list, rxml);
    if (!ok)
        goto out;

    vp->setObject(*listobj);
out:
    js_LeaveLocalRootScopeWithResult(cx, *vp);
    return ok;
}

JS_FRIEND_DATA(Class) js_XMLClass = {
    js_XML_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_XML),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    xml_convert,
    xml_finalize,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    xml_hasInstance,
    JS_CLASS_TRACE(xml_trace),
    JS_NULL_CLASS_EXT,
    {
        xml_lookupProperty,
        xml_defineProperty,
        xml_getProperty,
        xml_setProperty,
        xml_getAttributes,
        xml_setAttributes,
        xml_deleteProperty,
        xml_enumerate,
        xml_typeOf,
        NULL,       /* trace */
        xml_fix,
        NULL,       /* thisObject     */
        xml_clear
    }
};

static JSXML *
StartNonListXMLMethod(JSContext *cx, jsval *vp, JSObject **objp)
{
    JSXML *xml;
    JSFunction *fun;
    char numBuf[12];

    JS_ASSERT(VALUE_IS_FUNCTION(cx, *vp));

    *objp = JS_THIS_OBJECT(cx, vp);
    xml = (JSXML *) GetInstancePrivate(cx, *objp, &js_XMLClass, Valueify(vp + 2));
    if (!xml || xml->xml_class != JSXML_CLASS_LIST)
        return xml;

    if (xml->xml_kids.length == 1) {
        xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
        if (xml) {
            *objp = js_GetXMLObject(cx, xml);
            if (!*objp)
                return NULL;
            vp[1] = OBJECT_TO_JSVAL(*objp);
            return xml;
        }
    }

    fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(*vp));
    JS_snprintf(numBuf, sizeof numBuf, "%u", xml->xml_kids.length);
    JSAutoByteString funNameBytes;
    if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NON_LIST_XML_METHOD,
                             funName, numBuf);
    }
    return NULL;
}

/* Beware: these two are not bracketed by JS_BEGIN/END_MACRO. */
#define XML_METHOD_PROLOG                                                     \
    JSObject *obj = JS_THIS_OBJECT(cx, vp);                                   \
    JSXML *xml = (JSXML *)GetInstancePrivate(cx, obj, &js_XMLClass, Valueify(vp+2)); \
    if (!xml)                                                                 \
        return JS_FALSE

#define NON_LIST_XML_METHOD_PROLOG                                            \
    JSObject *obj;                                                            \
    JSXML *xml = StartNonListXMLMethod(cx, vp, &obj);                         \
    if (!xml)                                                                 \
        return JS_FALSE;                                                      \
    JS_ASSERT(xml->xml_class != JSXML_CLASS_LIST)

static JSBool
xml_addNamespace(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *ns;

    NON_LIST_XML_METHOD_PROLOG;
    if (xml->xml_class != JSXML_CLASS_ELEMENT)
        goto done;
    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    if (!NamespaceHelper(cx, NULL, argc == 0 ? -1 : 1, vp + 2, vp))
        return JS_FALSE;
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(*vp));

    ns = JSVAL_TO_OBJECT(*vp);
    if (!AddInScopeNamespace(cx, xml, ns))
        return JS_FALSE;
    ns->setNamespaceDeclared(JSVAL_TRUE);

  done:
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
xml_appendChild(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    JSObject *vobj;
    JSXML *vxml;

    NON_LIST_XML_METHOD_PROLOG;
    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    jsid name;
    if (!js_GetAnyName(cx, &name))
        return JS_FALSE;

    if (!GetProperty(cx, obj, name, &v))
        return JS_FALSE;

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
    vobj = JSVAL_TO_OBJECT(v);
    JS_ASSERT(vobj->isXML());
    vxml = (JSXML *) vobj->getPrivate();
    JS_ASSERT(vxml->xml_class == JSXML_CLASS_LIST);

    if (!IndexToId(cx, vxml->xml_kids.length, &name))
        return JS_FALSE;
    *vp = (argc != 0) ? vp[2] : JSVAL_VOID;

    if (!PutProperty(cx, JSVAL_TO_OBJECT(v), name, vp))
        return JS_FALSE;

    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_attribute(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *qn;

    if (argc == 0) {
        js_ReportMissingArg(cx, Valueify(*vp), 0);
        return JS_FALSE;
    }

    qn = ToAttributeName(cx, vp[2]);
    if (!qn)
        return JS_FALSE;
    vp[2] = OBJECT_TO_JSVAL(qn);        /* local root */

    jsid id = OBJECT_TO_JSID(qn);
    return GetProperty(cx, JS_THIS_OBJECT(cx, vp), id, vp);
}

/* XML and XMLList */
static JSBool
xml_attributes(JSContext *cx, uintN argc, jsval *vp)
{
    jsval name = ATOM_TO_JSVAL(cx->runtime->atomState.starAtom);
    JSObject *qn = ToAttributeName(cx, name);
    if (!qn)
        return JS_FALSE;

    AutoObjectRooter tvr(cx, qn);
    jsid id = OBJECT_TO_JSID(qn);
    return GetProperty(cx, JS_THIS_OBJECT(cx, vp), id, vp);
}

static JSXML *
xml_list_helper(JSContext *cx, JSXML *xml, jsval *rval)
{
    JSObject *listobj;
    JSXML *list;

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return NULL;

    *rval = OBJECT_TO_JSVAL(listobj);
    list = (JSXML *) listobj->getPrivate();
    list->xml_target = xml;
    return list;
}

static JSBool
ValueToId(JSContext *cx, jsval v, AutoIdRooter *idr)
{
    if (JSVAL_IS_INT(v)) {
        jsint i = JSVAL_TO_INT(v);
        if (INT_FITS_IN_JSID(i))
            *idr->addr() = INT_TO_JSID(i);
        else if (!js_ValueToStringId(cx, Valueify(v), idr->addr()))
            return JS_FALSE;
    } else if (JSVAL_IS_STRING(v)) {
        JSAtom *atom = js_AtomizeString(cx, JSVAL_TO_STRING(v), 0);
        if (!atom)
            return JS_FALSE;
        *idr->addr() = ATOM_TO_JSID(atom);
    } else if (!JSVAL_IS_PRIMITIVE(v)) {
        *idr->addr() = OBJECT_TO_JSID(JSVAL_TO_OBJECT(v));
    } else {
        ReportBadXMLName(cx, Valueify(v));
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
xml_child_helper(JSContext *cx, JSObject *obj, JSXML *xml, jsval name,
                 jsval *rval)
{
    uint32 index;
    JSXML *kid;
    JSObject *kidobj;

    /* ECMA-357 13.4.4.6 */
    JS_ASSERT(xml->xml_class != JSXML_CLASS_LIST);

    if (js_IdValIsIndex(name, &index)) {
        if (index >= JSXML_LENGTH(xml)) {
            *rval = JSVAL_VOID;
        } else {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
            if (!kid) {
                *rval = JSVAL_VOID;
            } else {
                kidobj = js_GetXMLObject(cx, kid);
                if (!kidobj)
                    return JS_FALSE;
                *rval = OBJECT_TO_JSVAL(kidobj);
            }
        }
        return JS_TRUE;
    }

    AutoIdRooter idr(cx);
    if (!ValueToId(cx, name, &idr))
        return JS_FALSE;

    return GetProperty(cx, obj, idr.id(), rval);
}

/* XML and XMLList */
static JSBool
xml_child(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    JSXML *list, *vxml;
    JSObject *kidobj;

    XML_METHOD_PROLOG;
    jsval name = argc != 0 ? vp[2] : JSVAL_VOID;
    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* ECMA-357 13.5.4.4 */
        list = xml_list_helper(cx, xml, vp);
        if (!list)
            return JS_FALSE;

        JSXMLArrayCursor cursor(&xml->xml_kids);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            kidobj = js_GetXMLObject(cx, kid);
            if (!kidobj)
                return JS_FALSE;
            if (!xml_child_helper(cx, kidobj, kid, name, &v))
                return JS_FALSE;
            if (JSVAL_IS_VOID(v)) {
                /* The property didn't exist in this kid. */
                continue;
            }

            JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
            vxml = (JSXML *) JSVAL_TO_OBJECT(v)->getPrivate();
            if ((!JSXML_HAS_KIDS(vxml) || vxml->xml_kids.length != 0) &&
                !Append(cx, list, vxml)) {
                return JS_FALSE;
            }
        }
        return JS_TRUE;
    }

    /* ECMA-357 Edition 2 13.3.4.6 (note 13.3, not 13.4 as in Edition 1). */
    if (!xml_child_helper(cx, obj, xml, name, vp))
        return JS_FALSE;
    if (JSVAL_IS_VOID(*vp) && !xml_list_helper(cx, xml, vp))
        return JS_FALSE;
    return JS_TRUE;
}

static JSBool
xml_childIndex(JSContext *cx, uintN argc, jsval *vp)
{
    JSXML *parent;
    uint32 i, n;

    NON_LIST_XML_METHOD_PROLOG;
    parent = xml->parent;
    if (!parent || xml->xml_class == JSXML_CLASS_ATTRIBUTE) {
        *vp = DOUBLE_TO_JSVAL(js_NaN);
        return JS_TRUE;
    }
    for (i = 0, n = JSXML_LENGTH(parent); i < n; i++) {
        if (XMLARRAY_MEMBER(&parent->xml_kids, i, JSXML) == xml)
            break;
    }
    JS_ASSERT(i < n);
    if (i <= JSVAL_INT_MAX)
        *vp = INT_TO_JSVAL(i);
    else
        *vp = DOUBLE_TO_JSVAL(i);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_children(JSContext *cx, uintN argc, jsval *vp)
{
    jsid name = ATOM_TO_JSID(cx->runtime->atomState.starAtom);
    return GetProperty(cx, JS_THIS_OBJECT(cx, vp), name, vp);
}

/* XML and XMLList */
static JSBool
xml_comments_helper(JSContext *cx, JSObject *obj, JSXML *xml, jsval *vp)
{
    JSXML *list, *kid, *vxml;
    JSBool ok;
    uint32 i, n;
    JSObject *kidobj;
    jsval v;

    list = xml_list_helper(cx, xml, vp);
    if (!list)
        return JS_FALSE;

    ok = JS_TRUE;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* 13.5.4.6 Step 2. */
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = js_EnterLocalRootScope(cx);
                if (!ok)
                    break;
                kidobj = js_GetXMLObject(cx, kid);
                if (kidobj) {
                    ok = xml_comments_helper(cx, kidobj, kid, &v);
                } else {
                    ok = JS_FALSE;
                    v = JSVAL_NULL;
                }
                js_LeaveLocalRootScopeWithResult(cx, Valueify(v));
                if (!ok)
                    break;
                vxml = (JSXML *) JSVAL_TO_OBJECT(v)->getPrivate();
                if (JSXML_LENGTH(vxml) != 0) {
                    ok = Append(cx, list, vxml);
                    if (!ok)
                        break;
                }
            }
        }
    } else {
        /* 13.4.4.9 Step 2. */
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_COMMENT) {
                ok = Append(cx, list, kid);
                if (!ok)
                    break;
            }
        }
    }

    return ok;
}

static JSBool
xml_comments(JSContext *cx, uintN argc, jsval *vp)
{
    XML_METHOD_PROLOG;
    return xml_comments_helper(cx, obj, xml, vp);
}

/* XML and XMLList */
static JSBool
xml_contains(JSContext *cx, uintN argc, jsval *vp)
{
    jsval value;
    JSBool eq;
    JSObject *kidobj;

    XML_METHOD_PROLOG;
    value = argc != 0 ? vp[2] : JSVAL_VOID;
    if (xml->xml_class == JSXML_CLASS_LIST) {
        eq = JS_FALSE;
        JSXMLArrayCursor cursor(&xml->xml_kids);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            kidobj = js_GetXMLObject(cx, kid);
            if (!kidobj || !js_TestXMLEquality(cx, ObjectValue(*kidobj), Valueify(value), &eq))
                return JS_FALSE;
            if (eq)
                break;
        }
    } else {
        if (!js_TestXMLEquality(cx, ObjectValue(*obj), Valueify(value), &eq))
            return JS_FALSE;
    }
    *vp = BOOLEAN_TO_JSVAL(eq);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_copy(JSContext *cx, uintN argc, jsval *vp)
{
    JSXML *copy;

    XML_METHOD_PROLOG;
    copy = DeepCopy(cx, xml, NULL, 0);
    if (!copy)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(copy->object);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_descendants(JSContext *cx, uintN argc, jsval *vp)
{
    jsval name;
    JSXML *list;

    XML_METHOD_PROLOG;
    name = argc == 0 ? ATOM_TO_JSVAL(cx->runtime->atomState.starAtom) : vp[2];
    list = Descendants(cx, xml, name);
    if (!list)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(list->object);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_elements_helper(JSContext *cx, JSObject *obj, JSXML *xml,
                    JSObject *nameqn, jsval *vp)
{
    JSXML *list, *vxml;
    jsval v;
    JSBool ok;
    JSObject *kidobj;
    uint32 i, n;

    list = xml_list_helper(cx, xml, vp);
    if (!list)
        return JS_FALSE;

    list->xml_targetprop = nameqn;
    ok = JS_TRUE;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* 13.5.4.6 */
        JSXMLArrayCursor cursor(&xml->xml_kids);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = js_EnterLocalRootScope(cx);
                if (!ok)
                    break;
                kidobj = js_GetXMLObject(cx, kid);
                if (kidobj) {
                    ok = xml_elements_helper(cx, kidobj, kid, nameqn, &v);
                } else {
                    ok = JS_FALSE;
                    v = JSVAL_NULL;
                }
                js_LeaveLocalRootScopeWithResult(cx, Valueify(v));
                if (!ok)
                    break;
                vxml = (JSXML *) JSVAL_TO_OBJECT(v)->getPrivate();
                if (JSXML_LENGTH(vxml) != 0) {
                    ok = Append(cx, list, vxml);
                    if (!ok)
                        break;
                }
            }
        }
    } else {
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            JSXML *kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT &&
                MatchElemName(nameqn, kid)) {
                ok = Append(cx, list, kid);
                if (!ok)
                    break;
            }
        }
    }

    return ok;
}

static JSBool
xml_elements(JSContext *cx, uintN argc, jsval *vp)
{
    jsval name;
    JSObject *nameqn;
    jsid funid;

    XML_METHOD_PROLOG;

    name = (argc == 0) ? ATOM_TO_JSVAL(cx->runtime->atomState.starAtom) : vp[2];
    nameqn = ToXMLName(cx, name, &funid);
    if (!nameqn)
        return JS_FALSE;
    vp[2] = OBJECT_TO_JSVAL(nameqn);

    if (!JSID_IS_VOID(funid))
        return xml_list_helper(cx, xml, vp) != NULL;

    return xml_elements_helper(cx, obj, xml, nameqn, vp);
}

/* XML and XMLList */
static JSBool
xml_hasOwnProperty(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsval name;
    JSBool found;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!InstanceOf(cx, obj, &js_XMLClass, Valueify(vp + 2)))
        return JS_FALSE;

    name = argc != 0 ? vp[2] : JSVAL_VOID;
    if (!HasProperty(cx, obj, name, &found))
        return JS_FALSE;
    if (found) {
        *vp = JSVAL_TRUE;
        return JS_TRUE;
    }
    return js_HasOwnPropertyHelper(cx, js_LookupProperty, argc, Valueify(vp));
}

/* XML and XMLList */
static JSBool
xml_hasComplexContent(JSContext *cx, uintN argc, jsval *vp)
{
    JSXML *kid;
    JSObject *kidobj;
    uint32 i, n;

    XML_METHOD_PROLOG;
again:
    switch (xml->xml_class) {
      case JSXML_CLASS_ATTRIBUTE:
      case JSXML_CLASS_COMMENT:
      case JSXML_CLASS_PROCESSING_INSTRUCTION:
      case JSXML_CLASS_TEXT:
        *vp = JSVAL_FALSE;
        break;
      case JSXML_CLASS_LIST:
        if (xml->xml_kids.length == 0) {
            *vp = JSVAL_TRUE;
        } else if (xml->xml_kids.length == 1) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
            if (kid) {
                kidobj = js_GetXMLObject(cx, kid);
                if (!kidobj)
                    return JS_FALSE;
                obj = kidobj;
                xml = (JSXML *) obj->getPrivate();
                goto again;
            }
        }
        /* FALL THROUGH */
      default:
        *vp = JSVAL_FALSE;
        for (i = 0, n = xml->xml_kids.length; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT) {
                *vp = JSVAL_TRUE;
                break;
            }
        }
        break;
    }
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_hasSimpleContent(JSContext *cx, uintN argc, jsval *vp)
{
    XML_METHOD_PROLOG;
    *vp = BOOLEAN_TO_JSVAL(HasSimpleContent(xml));
    return JS_TRUE;
}

static JSBool
FindInScopeNamespaces(JSContext *cx, JSXML *xml, JSXMLArray *nsarray)
{
    uint32 length, i, j, n;
    JSObject *ns, *ns2;
    JSString *prefix, *prefix2;

    length = nsarray->length;
    do {
        if (xml->xml_class != JSXML_CLASS_ELEMENT)
            continue;
        for (i = 0, n = xml->xml_namespaces.length; i < n; i++) {
            ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSObject);
            if (!ns)
                continue;

            prefix = GetPrefix(ns);
            for (j = 0; j < length; j++) {
                ns2 = XMLARRAY_MEMBER(nsarray, j, JSObject);
                if (ns2) {
                    prefix2 = GetPrefix(ns2);
                    if ((prefix2 && prefix)
                        ? js_EqualStrings(prefix2, prefix)
                        : js_EqualStrings(GetURI(ns2), GetURI(ns))) {
                        break;
                    }
                }
            }

            if (j == length) {
                if (!XMLARRAY_APPEND(cx, nsarray, ns))
                    return JS_FALSE;
                ++length;
            }
        }
    } while ((xml = xml->parent) != NULL);
    JS_ASSERT(length == nsarray->length);

    return JS_TRUE;
}

/*
 * Populate a new JS array with elements of array and place the result into
 * rval.  rval must point to a rooted location.
 */
static bool
NamespacesToJSArray(JSContext *cx, JSXMLArray *array, jsval *rval)
{
    JSObject *arrayobj = NewDenseEmptyArray(cx);
    if (!arrayobj)
        return false;
    *rval = OBJECT_TO_JSVAL(arrayobj);

    AutoValueRooter tvr(cx);
    for (uint32 i = 0, n = array->length; i < n; i++) {
        JSObject *ns = XMLARRAY_MEMBER(array, i, JSObject);
        if (!ns)
            continue;
        tvr.set(ObjectValue(*ns));
        if (!arrayobj->setProperty(cx, INT_TO_JSID(i), tvr.addr(), false))
            return false;
    }
    return true;
}

static JSBool
xml_inScopeNamespaces(JSContext *cx, uintN argc, jsval *vp)
{
    NON_LIST_XML_METHOD_PROLOG;

    AutoNamespaceArray namespaces(cx);
    return FindInScopeNamespaces(cx, xml, &namespaces.array) &&
           NamespacesToJSArray(cx, &namespaces.array, vp);
}

static JSBool
xml_insertChildAfter(JSContext *cx, uintN argc, jsval *vp)
{
    jsval arg;
    JSXML *kid;
    uint32 i;

    NON_LIST_XML_METHOD_PROLOG;
    *vp = OBJECT_TO_JSVAL(obj);
    if (!JSXML_HAS_KIDS(xml) || argc == 0)
        return JS_TRUE;

    arg = vp[2];
    if (JSVAL_IS_NULL(arg)) {
        kid = NULL;
        i = 0;
    } else {
        if (!VALUE_IS_XML(arg))
            return JS_TRUE;
        kid = (JSXML *) JSVAL_TO_OBJECT(arg)->getPrivate();
        i = XMLARRAY_FIND_MEMBER(&xml->xml_kids, kid, NULL);
        if (i == XML_NOT_FOUND)
            return JS_TRUE;
        ++i;
    }

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;
    return Insert(cx, xml, i, argc >= 2 ? vp[3] : JSVAL_VOID);
}

static JSBool
xml_insertChildBefore(JSContext *cx, uintN argc, jsval *vp)
{
    jsval arg;
    JSXML *kid;
    uint32 i;

    NON_LIST_XML_METHOD_PROLOG;
    *vp = OBJECT_TO_JSVAL(obj);
    if (!JSXML_HAS_KIDS(xml) || argc == 0)
        return JS_TRUE;

    arg = vp[2];
    if (JSVAL_IS_NULL(arg)) {
        kid = NULL;
        i = xml->xml_kids.length;
    } else {
        if (!VALUE_IS_XML(arg))
            return JS_TRUE;
        kid = (JSXML *) JSVAL_TO_OBJECT(arg)->getPrivate();
        i = XMLARRAY_FIND_MEMBER(&xml->xml_kids, kid, NULL);
        if (i == XML_NOT_FOUND)
            return JS_TRUE;
    }

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;
    return Insert(cx, xml, i, argc >= 2 ? vp[3] : JSVAL_VOID);
}

/* XML and XMLList */
static JSBool
xml_length(JSContext *cx, uintN argc, jsval *vp)
{
    XML_METHOD_PROLOG;
    if (xml->xml_class != JSXML_CLASS_LIST) {
        *vp = JSVAL_ONE;
    } else {
        uint32 l = xml->xml_kids.length;
        if (l <= JSVAL_INT_MAX)
            *vp = INT_TO_JSVAL(l);
        else
            *vp = DOUBLE_TO_JSVAL(l);
    }
    return JS_TRUE;
}

static JSBool
xml_localName(JSContext *cx, uintN argc, jsval *vp)
{
    NON_LIST_XML_METHOD_PROLOG;
    *vp = xml->name ? xml->name->getQNameLocalName() : JSVAL_NULL;
    return JS_TRUE;
}

static JSBool
xml_name(JSContext *cx, uintN argc, jsval *vp)
{
    NON_LIST_XML_METHOD_PROLOG;
    *vp = OBJECT_TO_JSVAL(xml->name);
    return JS_TRUE;
}

static JSBool
xml_namespace(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *prefix, *nsprefix;
    jsuint i, length;
    JSObject *ns;

    NON_LIST_XML_METHOD_PROLOG;
    if (argc == 0 && !JSXML_HAS_NAME(xml)) {
        *vp = JSVAL_NULL;
        return true;
    }

    if (argc == 0) {
        prefix = NULL;
    } else {
        prefix = js_ValueToString(cx, Valueify(vp[2]));
        if (!prefix)
            return false;
        vp[2] = STRING_TO_JSVAL(prefix);      /* local root */
    }

    AutoNamespaceArray inScopeNSes(cx);
    if (!FindInScopeNamespaces(cx, xml, &inScopeNSes.array))
        return false;

    if (!prefix) {
        ns = GetNamespace(cx, xml->name, &inScopeNSes.array);
        if (!ns)
            return false;
    } else {
        ns = NULL;
        for (i = 0, length = inScopeNSes.array.length; i < length; i++) {
            ns = XMLARRAY_MEMBER(&inScopeNSes.array, i, JSObject);
            if (ns) {
                nsprefix = GetPrefix(ns);
                if (nsprefix && js_EqualStrings(nsprefix, prefix))
                    break;
                ns = NULL;
            }
        }
    }

    *vp = (!ns) ? JSVAL_VOID : OBJECT_TO_JSVAL(ns);
    return true;
}

static JSBool
xml_namespaceDeclarations(JSContext *cx, uintN argc, jsval *vp)
{
    NON_LIST_XML_METHOD_PROLOG;
    if (JSXML_HAS_VALUE(xml))
        return true;

    AutoNamespaceArray ancestors(cx);
    AutoNamespaceArray declared(cx);

    JSXML *yml = xml;
    while ((yml = yml->parent) != NULL) {
        JS_ASSERT(yml->xml_class == JSXML_CLASS_ELEMENT);
        for (uint32 i = 0, n = yml->xml_namespaces.length; i < n; i++) {
            JSObject *ns = XMLARRAY_MEMBER(&yml->xml_namespaces, i, JSObject);
            if (ns && !XMLARRAY_HAS_MEMBER(&ancestors.array, ns, namespace_match)) {
                if (!XMLARRAY_APPEND(cx, &ancestors.array, ns))
                    return false;
            }
        }
    }

    for (uint32 i = 0, n = xml->xml_namespaces.length; i < n; i++) {
        JSObject *ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSObject);
        if (!ns)
            continue;
        if (!IsDeclared(ns))
            continue;
        if (!XMLARRAY_HAS_MEMBER(&ancestors.array, ns, namespace_match)) {
            if (!XMLARRAY_APPEND(cx, &declared.array, ns))
                return false;
        }
    }

    return NamespacesToJSArray(cx, &declared.array, vp);
}

static const char js_attribute_str[] = "attribute";
static const char js_text_str[]      = "text";

/* Exported to jsgc.c #ifdef DEBUG. */
const char *js_xml_class_str[] = {
    "list",
    "element",
    js_attribute_str,
    "processing-instruction",
    js_text_str,
    "comment"
};

static JSBool
xml_nodeKind(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    NON_LIST_XML_METHOD_PROLOG;
    str = JS_InternString(cx, js_xml_class_str[xml->xml_class]);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static void
NormalizingDelete(JSContext *cx, JSXML *xml, uint32 index)
{
    if (xml->xml_class == JSXML_CLASS_LIST)
        DeleteListElement(cx, xml, index);
    else
        DeleteByIndex(cx, xml, index);
}

/* XML and XMLList */
static JSBool
xml_normalize_helper(JSContext *cx, JSObject *obj, JSXML *xml)
{
    JSXML *kid, *kid2;
    uint32 i, n;
    JSObject *kidobj;
    JSString *str;

    if (!JSXML_HAS_KIDS(xml))
        return JS_TRUE;

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    for (i = 0, n = xml->xml_kids.length; i < n; i++) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
        if (!kid)
            continue;
        if (kid->xml_class == JSXML_CLASS_ELEMENT) {
            kidobj = js_GetXMLObject(cx, kid);
            if (!kidobj || !xml_normalize_helper(cx, kidobj, kid))
                return JS_FALSE;
        } else if (kid->xml_class == JSXML_CLASS_TEXT) {
            while (i + 1 < n &&
                   (kid2 = XMLARRAY_MEMBER(&xml->xml_kids, i + 1, JSXML)) &&
                   kid2->xml_class == JSXML_CLASS_TEXT) {
                str = js_ConcatStrings(cx, kid->xml_value, kid2->xml_value);
                if (!str)
                    return JS_FALSE;
                NormalizingDelete(cx, xml, i + 1);
                n = xml->xml_kids.length;
                kid->xml_value = str;
            }
            if (kid->xml_value->empty()) {
                NormalizingDelete(cx, xml, i);
                n = xml->xml_kids.length;
                --i;
            }
        }
    }

    return JS_TRUE;
}

static JSBool
xml_normalize(JSContext *cx, uintN argc, jsval *vp)
{
    XML_METHOD_PROLOG;
    *vp = OBJECT_TO_JSVAL(obj);
    return xml_normalize_helper(cx, obj, xml);
}

/* XML and XMLList */
static JSBool
xml_parent(JSContext *cx, uintN argc, jsval *vp)
{
    JSXML *parent, *kid;
    uint32 i, n;
    JSObject *parentobj;

    XML_METHOD_PROLOG;
    parent = xml->parent;
    if (xml->xml_class == JSXML_CLASS_LIST) {
        *vp = JSVAL_VOID;
        n = xml->xml_kids.length;
        if (n == 0)
            return JS_TRUE;

        kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
        if (!kid)
            return JS_TRUE;
        parent = kid->parent;
        for (i = 1; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->parent != parent)
                return JS_TRUE;
        }
    }

    if (!parent) {
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }

    parentobj = js_GetXMLObject(cx, parent);
    if (!parentobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(parentobj);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_processingInstructions_helper(JSContext *cx, JSObject *obj, JSXML *xml,
                                  JSObject *nameqn, jsval *vp)
{
    JSXML *list, *vxml;
    JSBool ok;
    JSObject *kidobj;
    jsval v;
    uint32 i, n;
    JSString *localName;

    list = xml_list_helper(cx, xml, vp);
    if (!list)
        return JS_FALSE;

    list->xml_targetprop = nameqn;
    ok = JS_TRUE;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* 13.5.4.17 Step 4 (misnumbered 9 -- Erratum?). */
        JSXMLArrayCursor cursor(&xml->xml_kids);
        while (JSXML *kid = (JSXML *) cursor.getNext()) {
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = js_EnterLocalRootScope(cx);
                if (!ok)
                    break;
                kidobj = js_GetXMLObject(cx, kid);
                if (kidobj) {
                    ok = xml_processingInstructions_helper(cx, kidobj, kid,
                                                           nameqn, &v);
                } else {
                    ok = JS_FALSE;
                    v = JSVAL_NULL;
                }
                js_LeaveLocalRootScopeWithResult(cx, Valueify(v));
                if (!ok)
                    break;
                vxml = (JSXML *) JSVAL_TO_OBJECT(v)->getPrivate();
                if (JSXML_LENGTH(vxml) != 0) {
                    ok = Append(cx, list, vxml);
                    if (!ok)
                        break;
                }
            }
        }
    } else {
        /* 13.4.4.28 Step 4. */
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            JSXML *kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_PROCESSING_INSTRUCTION) {
                localName = GetLocalName(nameqn);
                if (IS_STAR(localName) ||
                    js_EqualStrings(localName, GetLocalName(kid->name))) {
                    ok = Append(cx, list, kid);
                    if (!ok)
                        break;
                }
            }
        }
    }

    return ok;
}

static JSBool
xml_processingInstructions(JSContext *cx, uintN argc, jsval *vp)
{
    jsval name;
    JSObject *nameqn;
    jsid funid;

    XML_METHOD_PROLOG;

    name = (argc == 0) ? ATOM_TO_JSVAL(cx->runtime->atomState.starAtom) : vp[2];
    nameqn = ToXMLName(cx, name, &funid);
    if (!nameqn)
        return JS_FALSE;
    vp[2] = OBJECT_TO_JSVAL(nameqn);

    if (!JSID_IS_VOID(funid))
        return xml_list_helper(cx, xml, vp) != NULL;

    return xml_processingInstructions_helper(cx, obj, xml, nameqn, vp);
}

static JSBool
xml_prependChild(JSContext *cx, uintN argc, jsval *vp)
{
    NON_LIST_XML_METHOD_PROLOG;
    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(obj);
    return Insert(cx, xml, 0, argc != 0 ? vp[2] : JSVAL_VOID);
}

/* XML and XMLList */
static JSBool
xml_propertyIsEnumerable(JSContext *cx, uintN argc, jsval *vp)
{
    uint32 index;

    XML_METHOD_PROLOG;
    *vp = JSVAL_FALSE;
    if (argc != 0 && js_IdValIsIndex(vp[2], &index)) {
        if (xml->xml_class == JSXML_CLASS_LIST) {
            /* 13.5.4.18. */
            *vp = BOOLEAN_TO_JSVAL(index < xml->xml_kids.length);
        } else {
            /* 13.4.4.30. */
            *vp = BOOLEAN_TO_JSVAL(index == 0);
        }
    }
    return JS_TRUE;
}

static JSBool
namespace_full_match(const void *a, const void *b)
{
    const JSObject *nsa = (const JSObject *) a;
    const JSObject *nsb = (const JSObject *) b;
    JSString *prefixa = GetPrefix(nsa);
    JSString *prefixb;

    if (prefixa) {
        prefixb = GetPrefix(nsb);
        if (prefixb && !js_EqualStrings(prefixa, prefixb))
            return JS_FALSE;
    }
    return js_EqualStrings(GetURI(nsa), GetURI(nsb));
}

static JSBool
xml_removeNamespace_helper(JSContext *cx, JSXML *xml, JSObject *ns)
{
    JSObject *thisns, *attrns;
    uint32 i, n;
    JSXML *attr, *kid;

    thisns = GetNamespace(cx, xml->name, &xml->xml_namespaces);
    JS_ASSERT(thisns);
    if (thisns == ns)
        return JS_TRUE;

    for (i = 0, n = xml->xml_attrs.length; i < n; i++) {
        attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
        if (!attr)
            continue;
        attrns = GetNamespace(cx, attr->name, &xml->xml_namespaces);
        JS_ASSERT(attrns);
        if (attrns == ns)
            return JS_TRUE;
    }

    i = XMLARRAY_FIND_MEMBER(&xml->xml_namespaces, ns, namespace_full_match);
    if (i != XML_NOT_FOUND)
        XMLArrayDelete(cx, &xml->xml_namespaces, i, JS_TRUE);

    for (i = 0, n = xml->xml_kids.length; i < n; i++) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
        if (kid && kid->xml_class == JSXML_CLASS_ELEMENT) {
            if (!xml_removeNamespace_helper(cx, kid, ns))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSBool
xml_removeNamespace(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *ns;

    NON_LIST_XML_METHOD_PROLOG;
    if (xml->xml_class != JSXML_CLASS_ELEMENT)
        goto done;
    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    if (!NamespaceHelper(cx, NULL, argc == 0 ? -1 : 1, vp + 2, vp))
        return JS_FALSE;
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(*vp));
    ns = JSVAL_TO_OBJECT(*vp);

    /* NOTE: remove ns from each ancestor if not used by that ancestor. */
    if (!xml_removeNamespace_helper(cx, xml, ns))
        return JS_FALSE;
  done:
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
xml_replace(JSContext *cx, uintN argc, jsval *vp)
{
    jsval value;
    JSXML *vxml, *kid;
    uint32 index, i;
    JSObject *nameqn;

    NON_LIST_XML_METHOD_PROLOG;
    if (xml->xml_class != JSXML_CLASS_ELEMENT)
        goto done;

    if (argc <= 1) {
        value = ATOM_TO_JSVAL(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    } else {
        value = vp[3];
        vxml = VALUE_IS_XML(value)
               ? (JSXML *) JSVAL_TO_OBJECT(value)->getPrivate()
               : NULL;
        if (!vxml) {
            if (!JS_ConvertValue(cx, value, JSTYPE_STRING, &vp[3]))
                return JS_FALSE;
            value = vp[3];
        } else {
            vxml = DeepCopy(cx, vxml, NULL, 0);
            if (!vxml)
                return JS_FALSE;
            value = vp[3] = OBJECT_TO_JSVAL(vxml->object);
        }
    }

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    if (argc == 0 || !js_IdValIsIndex(vp[2], &index)) {
        /*
         * Call function QName per spec, not ToXMLName, to avoid attribute
         * names.
         */
        if (!QNameHelper(cx, NULL, &js_QNameClass, argc == 0 ? -1 : 1,
                         vp + 2, vp)) {
            return JS_FALSE;
        }
        JS_ASSERT(!JSVAL_IS_PRIMITIVE(*vp));
        nameqn = JSVAL_TO_OBJECT(*vp);

        i = xml->xml_kids.length;
        index = XML_NOT_FOUND;
        while (i != 0) {
            --i;
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && MatchElemName(nameqn, kid)) {
                if (i != XML_NOT_FOUND)
                    DeleteByIndex(cx, xml, i);
                index = i;
            }
        }

        if (index == XML_NOT_FOUND)
            goto done;
    }

    if (!Replace(cx, xml, index, value))
        return JS_FALSE;

  done:
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
xml_setChildren(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    if (!StartNonListXMLMethod(cx, vp, &obj))
        return JS_FALSE;

    *vp = argc != 0 ? vp[2] : JSVAL_VOID;     /* local root */
    if (!PutProperty(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.starAtom), vp))
        return JS_FALSE;

    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
xml_setLocalName(JSContext *cx, uintN argc, jsval *vp)
{
    jsval name;
    JSObject *nameqn;
    JSString *namestr;

    NON_LIST_XML_METHOD_PROLOG;
    if (!JSXML_HAS_NAME(xml))
        return JS_TRUE;

    if (argc == 0) {
        namestr = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    } else {
        name = vp[2];
        if (!JSVAL_IS_PRIMITIVE(name) &&
            JSVAL_TO_OBJECT(name)->getClass() == &js_QNameClass) {
            nameqn = JSVAL_TO_OBJECT(name);
            namestr = GetLocalName(nameqn);
        } else {
            if (!JS_ConvertValue(cx, name, JSTYPE_STRING, &vp[2]))
                return JS_FALSE;
            name = vp[2];
            namestr = JSVAL_TO_STRING(name);
        }
    }

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;
    xml->name->setQNameLocalName(namestr ? STRING_TO_JSVAL(namestr) : JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
xml_setName(JSContext *cx, uintN argc, jsval *vp)
{
    jsval name;
    JSObject *nameqn;
    JSXML *nsowner;
    JSXMLArray *nsarray;
    uint32 i, n;
    JSObject *ns;

    NON_LIST_XML_METHOD_PROLOG;
    if (!JSXML_HAS_NAME(xml))
        return JS_TRUE;

    if (argc == 0) {
        name = ATOM_TO_JSVAL(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    } else {
        name = vp[2];
        if (!JSVAL_IS_PRIMITIVE(name) &&
            JSVAL_TO_OBJECT(name)->getClass() == &js_QNameClass &&
            !GetURI(nameqn = JSVAL_TO_OBJECT(name))) {
            name = vp[2] = nameqn->getQNameLocalName();
        }
    }

    nameqn = js_ConstructObject(cx, &js_QNameClass, NULL, NULL, 1, Valueify(&name));
    if (!nameqn)
        return JS_FALSE;

    /* ECMA-357 13.4.4.35 Step 4. */
    if (xml->xml_class == JSXML_CLASS_PROCESSING_INSTRUCTION)
        nameqn->setNameURI(STRING_TO_JSVAL(cx->runtime->emptyString));

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;
    xml->name = nameqn;

    /*
     * Erratum: nothing in 13.4.4.35 talks about making the name match the
     * in-scope namespaces, either by finding an in-scope namespace with a
     * matching uri and setting the new name's prefix to that namespace's
     * prefix, or by extending the in-scope namespaces for xml (which are in
     * xml->parent if xml is an attribute or a PI).
     */
    if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        nsowner = xml;
    } else {
        if (!xml->parent || xml->parent->xml_class != JSXML_CLASS_ELEMENT)
            return JS_TRUE;
        nsowner = xml->parent;
    }

    if (GetPrefix(nameqn)) {
        /*
         * The name being set has a prefix, which originally came from some
         * namespace object (which may be the null namespace, where both the
         * prefix and uri are the empty string).  We must go through a full
         * GetNamespace in case that namespace is in-scope in nsowner.
         *
         * If we find such an in-scope namespace, we return true right away,
         * in this block.  Otherwise, we fall through to the final return of
         * AddInScopeNamespace(cx, nsowner, ns).
         */
        ns = GetNamespace(cx, nameqn, &nsowner->xml_namespaces);
        if (!ns)
            return JS_FALSE;

        /* XXXbe have to test membership to see whether GetNamespace added */
        if (XMLARRAY_HAS_MEMBER(&nsowner->xml_namespaces, ns, NULL))
            return JS_TRUE;
    } else {
        /*
         * At this point, we know prefix of nameqn is null, so its uri can't
         * be the empty string (the null namespace always uses the empty string
         * for both prefix and uri).
         *
         * This means we must inline GetNamespace and specialize it to match
         * uri only, never prefix.  If we find a namespace with nameqn's uri
         * already in nsowner->xml_namespaces, then all that we need do is set
         * prefix of nameqn to that namespace's prefix.
         *
         * If no such namespace exists, we can create one without going through
         * the constructor, because we know uri of nameqn is non-empty (so
         * prefix does not need to be converted from null to empty by QName).
         */
        JS_ASSERT(!GetURI(nameqn)->empty());

        nsarray = &nsowner->xml_namespaces;
        for (i = 0, n = nsarray->length; i < n; i++) {
            ns = XMLARRAY_MEMBER(nsarray, i, JSObject);
            if (ns && js_EqualStrings(GetURI(ns), GetURI(nameqn))) {
                nameqn->setNamePrefix(ns->getNamePrefix());
                return JS_TRUE;
            }
        }

        ns = NewXMLNamespace(cx, NULL, GetURI(nameqn), JS_TRUE);
        if (!ns)
            return JS_FALSE;
    }

    if (!AddInScopeNamespace(cx, nsowner, ns))
        return JS_FALSE;
    vp[0] = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
xml_setNamespace(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *qn;
    JSObject *ns;
    jsval qnargv[2];
    JSXML *nsowner;

    NON_LIST_XML_METHOD_PROLOG;
    if (!JSXML_HAS_NAME(xml))
        return JS_TRUE;

    xml = CHECK_COPY_ON_WRITE(cx, xml, obj);
    if (!xml)
        return JS_FALSE;

    ns = js_ConstructObject(cx, &js_NamespaceClass, NULL, obj,
                            argc == 0 ? 0 : 1, Valueify(vp + 2));
    if (!ns)
        return JS_FALSE;
    vp[0] = OBJECT_TO_JSVAL(ns);
    ns->setNamespaceDeclared(JSVAL_TRUE);

    qnargv[0] = OBJECT_TO_JSVAL(ns);
    qnargv[1] = OBJECT_TO_JSVAL(xml->name);
    qn = js_ConstructObject(cx, &js_QNameClass, NULL, NULL, 2, Valueify(qnargv));
    if (!qn)
        return JS_FALSE;

    xml->name = qn;

    /*
     * Erratum: the spec fails to update the governing in-scope namespaces.
     * See the erratum noted in xml_setName, above.
     */
    if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        nsowner = xml;
    } else {
        if (!xml->parent || xml->parent->xml_class != JSXML_CLASS_ELEMENT)
            return JS_TRUE;
        nsowner = xml->parent;
    }
    if (!AddInScopeNamespace(cx, nsowner, ns))
        return JS_FALSE;
    vp[0] = JSVAL_VOID;
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_text_helper(JSContext *cx, JSObject *obj, JSXML *xml, jsval *vp)
{
    JSXML *list, *kid, *vxml;
    uint32 i, n;
    JSBool ok;
    JSObject *kidobj;
    jsval v;

    list = xml_list_helper(cx, xml, vp);
    if (!list)
        return JS_FALSE;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        ok = JS_TRUE;
        for (i = 0, n = xml->xml_kids.length; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = js_EnterLocalRootScope(cx);
                if (!ok)
                    break;
                kidobj = js_GetXMLObject(cx, kid);
                if (kidobj) {
                    ok = xml_text_helper(cx, kidobj, kid, &v);
                } else {
                    ok = JS_FALSE;
                    v = JSVAL_NULL;
                }
                js_LeaveLocalRootScopeWithResult(cx, Valueify(v));
                if (!ok)
                    return JS_FALSE;
                vxml = (JSXML *) JSVAL_TO_OBJECT(v)->getPrivate();
                if (JSXML_LENGTH(vxml) != 0 && !Append(cx, list, vxml))
                    return JS_FALSE;
            }
        }
    } else {
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid && kid->xml_class == JSXML_CLASS_TEXT) {
                if (!Append(cx, list, kid))
                    return JS_FALSE;
            }
        }
    }
    return JS_TRUE;
}

static JSBool
xml_text(JSContext *cx, uintN argc, jsval *vp)
{
    XML_METHOD_PROLOG;
    return xml_text_helper(cx, obj, xml, vp);
}

/* XML and XMLList */
static JSString *
xml_toString_helper(JSContext *cx, JSXML *xml)
{
    JSString *str, *kidstr;

    if (xml->xml_class == JSXML_CLASS_ATTRIBUTE ||
        xml->xml_class == JSXML_CLASS_TEXT) {
        return xml->xml_value;
    }

    if (!HasSimpleContent(xml))
        return ToXMLString(cx, OBJECT_TO_JSVAL(xml->object), 0);

    str = cx->runtime->emptyString;
    if (!js_EnterLocalRootScope(cx))
        return NULL;
    JSXMLArrayCursor cursor(&xml->xml_kids);
    while (JSXML *kid = (JSXML *) cursor.getNext()) {
        if (kid->xml_class != JSXML_CLASS_COMMENT &&
            kid->xml_class != JSXML_CLASS_PROCESSING_INSTRUCTION) {
            kidstr = xml_toString_helper(cx, kid);
            if (!kidstr) {
                str = NULL;
                break;
            }
            str = js_ConcatStrings(cx, str, kidstr);
            if (!str)
                break;
        }
    }
    js_LeaveLocalRootScopeWithResult(cx, str);
    return str;
}

static JSBool
xml_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    jsval thisv;
    JSString *str;

    thisv = JS_THIS(cx, vp);
    if (JSVAL_IS_NULL(thisv))
        return JS_FALSE;
    str = ToXMLString(cx, thisv, TO_SOURCE_FLAG);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
xml_toString(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    XML_METHOD_PROLOG;
    str = xml_toString_helper(cx, xml);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_toXMLString(JSContext *cx, uintN argc, jsval *vp)
{
    jsval thisv;
    JSString *str;

    thisv = JS_THIS(cx, vp);
    if (JSVAL_IS_NULL(thisv))
        return JS_FALSE;
    str = ToXMLString(cx, thisv, 0);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_valueOf(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = JS_THIS(cx, vp);
    return !JSVAL_IS_NULL(*vp);
}

static JSFunctionSpec xml_methods[] = {
    JS_FN("addNamespace",          xml_addNamespace,          1,0),
    JS_FN("appendChild",           xml_appendChild,           1,0),
    JS_FN(js_attribute_str,        xml_attribute,             1,0),
    JS_FN("attributes",            xml_attributes,            0,0),
    JS_FN("child",                 xml_child,                 1,0),
    JS_FN("childIndex",            xml_childIndex,            0,0),
    JS_FN("children",              xml_children,              0,0),
    JS_FN("comments",              xml_comments,              0,0),
    JS_FN("contains",              xml_contains,              1,0),
    JS_FN("copy",                  xml_copy,                  0,0),
    JS_FN("descendants",           xml_descendants,           1,0),
    JS_FN("elements",              xml_elements,              1,0),
    JS_FN("hasOwnProperty",        xml_hasOwnProperty,        1,0),
    JS_FN("hasComplexContent",     xml_hasComplexContent,     1,0),
    JS_FN("hasSimpleContent",      xml_hasSimpleContent,      1,0),
    JS_FN("inScopeNamespaces",     xml_inScopeNamespaces,     0,0),
    JS_FN("insertChildAfter",      xml_insertChildAfter,      2,0),
    JS_FN("insertChildBefore",     xml_insertChildBefore,     2,0),
    JS_FN(js_length_str,           xml_length,                0,0),
    JS_FN(js_localName_str,        xml_localName,             0,0),
    JS_FN(js_name_str,             xml_name,                  0,0),
    JS_FN(js_namespace_str,        xml_namespace,             1,0),
    JS_FN("namespaceDeclarations", xml_namespaceDeclarations, 0,0),
    JS_FN("nodeKind",              xml_nodeKind,              0,0),
    JS_FN("normalize",             xml_normalize,             0,0),
    JS_FN(js_xml_parent_str,       xml_parent,                0,0),
    JS_FN("processingInstructions",xml_processingInstructions,1,0),
    JS_FN("prependChild",          xml_prependChild,          1,0),
    JS_FN("propertyIsEnumerable",  xml_propertyIsEnumerable,  1,0),
    JS_FN("removeNamespace",       xml_removeNamespace,       1,0),
    JS_FN("replace",               xml_replace,               2,0),
    JS_FN("setChildren",           xml_setChildren,           1,0),
    JS_FN("setLocalName",          xml_setLocalName,          1,0),
    JS_FN("setName",               xml_setName,               1,0),
    JS_FN("setNamespace",          xml_setNamespace,          1,0),
    JS_FN(js_text_str,             xml_text,                  0,0),
    JS_FN(js_toSource_str,         xml_toSource,              0,0),
    JS_FN(js_toString_str,         xml_toString,              0,0),
    JS_FN(js_toXMLString_str,      xml_toXMLString,           0,0),
    JS_FN(js_valueOf_str,          xml_valueOf,               0,0),
    JS_FS_END
};

static JSBool
CopyXMLSettings(JSContext *cx, JSObject *from, JSObject *to)
{
    int i;
    const char *name;
    jsval v;

    /* Note: PRETTY_INDENT is not a boolean setting. */
    for (i = 0; xml_static_props[i].name; i++) {
        name = xml_static_props[i].name;
        if (!JS_GetProperty(cx, from, name, &v))
            return false;
        if (name == js_prettyIndent_str) {
            if (!JSVAL_IS_NUMBER(v))
                continue;
        } else {
            if (!JSVAL_IS_BOOLEAN(v))
                continue;
        }
        if (!JS_SetProperty(cx, to, name, &v))
            return false;
    }

    return true;
}

static JSBool
SetDefaultXMLSettings(JSContext *cx, JSObject *obj)
{
    int i;
    jsval v;

    /* Note: PRETTY_INDENT is not a boolean setting. */
    for (i = 0; xml_static_props[i].name; i++) {
        v = (xml_static_props[i].name != js_prettyIndent_str)
            ? JSVAL_TRUE : INT_TO_JSVAL(2);
        if (!JS_SetProperty(cx, obj, xml_static_props[i].name, &v))
            return JS_FALSE;
    }
    return true;
}

static JSBool
xml_settings(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *settings;
    JSObject *obj;

    settings = JS_NewObject(cx, NULL, NULL, NULL);
    if (!settings)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(settings);
    obj = JS_THIS_OBJECT(cx, vp);
    return obj && CopyXMLSettings(cx, obj, settings);
}

static JSBool
xml_setSettings(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj, *settings;
    jsval v;
    JSBool ok;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    v = (argc == 0) ? JSVAL_VOID : vp[2];
    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
        ok = SetDefaultXMLSettings(cx, obj);
    } else {
        if (JSVAL_IS_PRIMITIVE(v))
            return JS_TRUE;
        settings = JSVAL_TO_OBJECT(v);
        ok = CopyXMLSettings(cx, settings, obj);
    }
    return ok;
}

static JSBool
xml_defaultSettings(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *settings;

    settings = JS_NewObject(cx, NULL, NULL, NULL);
    if (!settings)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(settings);
    return SetDefaultXMLSettings(cx, settings);
}

static JSFunctionSpec xml_static_methods[] = {
    JS_FN("settings",         xml_settings,          0,0),
    JS_FN("setSettings",      xml_setSettings,       1,0),
    JS_FN("defaultSettings",  xml_defaultSettings,   0,0),
    JS_FS_END
};

static JSBool
XML(JSContext *cx, uintN argc, Value *vp)
{
    JSXML *xml, *copy;
    JSObject *xobj, *vobj;
    Class *clasp;

    jsval v = argc ? Jsvalify(vp[2]) : JSVAL_VOID;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        v = STRING_TO_JSVAL(cx->runtime->emptyString);

    xobj = ToXML(cx, v);
    if (!xobj)
        return JS_FALSE;
    xml = (JSXML *) xobj->getPrivate();

    if (IsConstructing(vp) && !JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        clasp = vobj->getClass();
        if (clasp == &js_XMLClass ||
            (clasp->flags & JSCLASS_DOCUMENT_OBSERVER)) {
            copy = DeepCopy(cx, xml, NULL, 0);
            if (!copy)
                return JS_FALSE;
            vp->setObject(*copy->object);
            return JS_TRUE;
        }
    }

    vp->setObject(*xobj);
    return JS_TRUE;
}

static JSBool
XMLList(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *vobj, *listobj;
    JSXML *xml, *list;

    jsval v = argc ? vp[2] : JSVAL_VOID;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        v = STRING_TO_JSVAL(cx->runtime->emptyString);

    if (IsConstructing(Valueify(vp)) && !JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (vobj->isXML()) {
            xml = (JSXML *) vobj->getPrivate();
            if (xml->xml_class == JSXML_CLASS_LIST) {
                listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
                if (!listobj)
                    return JS_FALSE;
                *vp = OBJECT_TO_JSVAL(listobj);

                list = (JSXML *) listobj->getPrivate();
                if (!Append(cx, list, xml))
                    return JS_FALSE;
                return JS_TRUE;
            }
        }
    }

    /* Toggle on XML support since the script has explicitly requested it. */
    listobj = ToXMLList(cx, v);
    if (!listobj)
        return JS_FALSE;

    *vp = OBJECT_TO_JSVAL(listobj);
    return JS_TRUE;
}

#ifdef DEBUG_notme
JSCList xml_leaks = JS_INIT_STATIC_CLIST(&xml_leaks);
uint32  xml_serial;
#endif

JSXML *
js_NewXML(JSContext *cx, JSXMLClass xml_class)
{
    JSXML *xml = js_NewGCXML(cx);
    if (!xml)
        return NULL;

    xml->object = NULL;
    xml->domnode = NULL;
    xml->parent = NULL;
    xml->name = NULL;
    xml->xml_class = xml_class;
    xml->xml_flags = 0;
    if (JSXML_CLASS_HAS_VALUE(xml_class)) {
        xml->xml_value = cx->runtime->emptyString;
    } else {
        xml->xml_kids.init();
        if (xml_class == JSXML_CLASS_LIST) {
            xml->xml_target = NULL;
            xml->xml_targetprop = NULL;
        } else {
            xml->xml_namespaces.init();
            xml->xml_attrs.init();
        }
    }

#ifdef DEBUG_notme
    JS_APPEND_LINK(&xml->links, &xml_leaks);
    xml->serial = xml_serial++;
#endif
    METER(xml_stats.xml);
    return xml;
}

void
js_TraceXML(JSTracer *trc, JSXML *xml)
{
    if (xml->object)
        MarkObject(trc, *xml->object, "object");
    if (xml->name)
        MarkObject(trc, *xml->name, "name");
    if (xml->parent)
        JS_CALL_TRACER(trc, xml->parent, JSTRACE_XML, "xml_parent");

    if (JSXML_HAS_VALUE(xml)) {
        if (xml->xml_value)
            MarkString(trc, xml->xml_value, "value");
        return;
    }

    xml_trace_vector(trc,
                     (JSXML **) xml->xml_kids.vector,
                     xml->xml_kids.length);
    XMLArrayCursorTrace(trc, xml->xml_kids.cursors);
    if (IS_GC_MARKING_TRACER(trc))
        xml->xml_kids.trim();

    if (xml->xml_class == JSXML_CLASS_LIST) {
        if (xml->xml_target)
            JS_CALL_TRACER(trc, xml->xml_target, JSTRACE_XML, "target");
        if (xml->xml_targetprop)
            MarkObject(trc, *xml->xml_targetprop, "targetprop");
    } else {
        MarkObjectRange(trc, xml->xml_namespaces.length,
                        (JSObject **) xml->xml_namespaces.vector,
                        "xml_namespaces");
        XMLArrayCursorTrace(trc, xml->xml_namespaces.cursors);
        if (IS_GC_MARKING_TRACER(trc))
            xml->xml_namespaces.trim();

        xml_trace_vector(trc,
                         (JSXML **) xml->xml_attrs.vector,
                         xml->xml_attrs.length);
        XMLArrayCursorTrace(trc, xml->xml_attrs.cursors);
        if (IS_GC_MARKING_TRACER(trc))
            xml->xml_attrs.trim();
    }
}

JSObject *
js_NewXMLObject(JSContext *cx, JSXMLClass xml_class)
{
    JSXML *xml = js_NewXML(cx, xml_class);
    if (!xml)
        return NULL;

    AutoXMLRooter root(cx, xml);
    return js_GetXMLObject(cx, xml);
}

static JSObject *
NewXMLObject(JSContext *cx, JSXML *xml)
{
    JSObject *obj;

    obj = NewNonFunction<WithProto::Class>(cx, &js_XMLClass, NULL, NULL);
    if (!obj)
        return NULL;
    obj->setPrivate(xml);
    METER(xml_stats.xmlobj);
    return obj;
}

JSObject *
js_GetXMLObject(JSContext *cx, JSXML *xml)
{
    JSObject *obj;

    obj = xml->object;
    if (obj) {
        JS_ASSERT(obj->getPrivate() == xml);
        return obj;
    }

    obj = NewXMLObject(cx, xml);
    if (!obj)
        return NULL;
    xml->object = obj;
    return obj;
}

JSObject *
js_InitNamespaceClass(JSContext *cx, JSObject *obj)
{
    return js_InitClass(cx, obj, NULL, &js_NamespaceClass, Namespace, 2,
                        namespace_props, namespace_methods, NULL, NULL);
}

JSObject *
js_InitQNameClass(JSContext *cx, JSObject *obj)
{
    return js_InitClass(cx, obj, NULL, &js_QNameClass, QName, 2,
                        qname_props, qname_methods, NULL, NULL);
}

JSObject *
js_InitAttributeNameClass(JSContext *cx, JSObject *obj)
{
    return js_InitClass(cx, obj, NULL, &js_AttributeNameClass, AttributeName, 2,
                        qname_props, qname_methods, NULL, NULL);
}

JSObject *
js_InitAnyNameClass(JSContext *cx, JSObject *obj)
{
    jsid id;

    if (!js_GetAnyName(cx, &id))
        return NULL;
    return JSID_TO_OBJECT(id);
}

JSObject *
js_InitXMLClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto, *pobj;
    JSFunction *fun;
    JSXML *xml;
    JSProperty *prop;
    Shape *shape;
    jsval cval, vp[3];

    /* Define the isXMLName function. */
    if (!JS_DefineFunction(cx, obj, js_isXMLName_str, xml_isXMLName, 1, 0))
        return NULL;

    /* Define the XML class constructor and prototype. */
    proto = js_InitClass(cx, obj, NULL, &js_XMLClass, XML, 1,
                         NULL, xml_methods,
                         xml_static_props, xml_static_methods);
    if (!proto)
        return NULL;

    xml = js_NewXML(cx, JSXML_CLASS_TEXT);
    if (!xml)
        return NULL;
    proto->setPrivate(xml);
    xml->object = proto;
    METER(xml_stats.xmlobj);

    /*
     * Prepare to set default settings on the XML constructor we just made.
     * NB: We can't use JS_GetConstructor, because it calls
     * JSObject::getProperty, which is xml_getProperty, which creates a new
     * XMLList every time!  We must instead call js_LookupProperty directly.
     */
    if (!js_LookupProperty(cx, proto,
                           ATOM_TO_JSID(cx->runtime->atomState.constructorAtom),
                           &pobj, &prop)) {
        return NULL;
    }
    JS_ASSERT(prop);
    shape = (Shape *) prop;
    cval = Jsvalify(pobj->nativeGetSlot(shape->slot));
    JS_ASSERT(VALUE_IS_FUNCTION(cx, cval));

    /* Set default settings. */
    vp[0] = JSVAL_NULL;
    vp[1] = cval;
    vp[2] = JSVAL_VOID;
    if (!xml_setSettings(cx, 1, vp))
        return NULL;

    /* Define the XMLList function and give it the same prototype as XML. */
    fun = JS_DefineFunction(cx, obj, js_XMLList_str, XMLList, 1, JSFUN_CONSTRUCTOR);
    if (!fun)
        return NULL;
    if (!js_SetClassPrototype(cx, FUN_OBJECT(fun), proto,
                              JSPROP_READONLY | JSPROP_PERMANENT)) {
        return NULL;
    }
    return proto;
}

JSObject *
js_InitXMLClasses(JSContext *cx, JSObject *obj)
{
    if (!js_InitNamespaceClass(cx, obj))
        return NULL;
    if (!js_InitQNameClass(cx, obj))
        return NULL;
    if (!js_InitAttributeNameClass(cx, obj))
        return NULL;
    if (!js_InitAnyNameClass(cx, obj))
        return NULL;
    return js_InitXMLClass(cx, obj);
}

JSBool
js_GetFunctionNamespace(JSContext *cx, Value *vp)
{
    JSObject *obj;
    JSString *prefix, *uri;

    obj = cx->compartment->functionNamespaceObject;
    if (!obj) {
        JSRuntime *rt = cx->runtime;
        prefix = ATOM_TO_STRING(rt->atomState.typeAtoms[JSTYPE_FUNCTION]);
        uri = ATOM_TO_STRING(rt->atomState.functionNamespaceURIAtom);
        obj = NewXMLNamespace(cx, prefix, uri, JS_FALSE);
        if (!obj)
            return false;

        /*
         * Avoid entraining any in-scope Object.prototype.  The loss of
         * Namespace.prototype is not detectable, as there is no way to
         * refer to this instance in scripts.  When used to qualify method
         * names, its prefix and uri references are copied to the QName.
         */
        obj->clearProto();
        obj->clearParent();

        cx->compartment->functionNamespaceObject = obj;
    }
    vp->setObject(*obj);

    return true;
}

/*
 * Note the asymmetry between js_GetDefaultXMLNamespace and js_SetDefaultXML-
 * Namespace.  Get searches fp->scopeChain for JS_DEFAULT_XML_NAMESPACE_ID,
 * while Set sets JS_DEFAULT_XML_NAMESPACE_ID in fp->varobj. There's no
 * requirement that fp->varobj lie directly on fp->scopeChain, although
 * it should be reachable using the prototype chain from a scope object (cf.
 * JSOPTION_VAROBJFIX in jsapi.h).
 *
 * If Get can't find JS_DEFAULT_XML_NAMESPACE_ID along the scope chain, it
 * creates a default namespace via 'new Namespace()'.  In contrast, Set uses
 * its v argument as the uri of a new Namespace, with "" as the prefix.  See
 * ECMA-357 12.1 and 12.1.1.  Note that if Set is called with a Namespace n,
 * the default XML namespace will be set to ("", n.uri).  So the uri string
 * is really the only usefully stored value of the default namespace.
 */
JSBool
js_GetDefaultXMLNamespace(JSContext *cx, jsval *vp)
{
    JSObject *ns, *obj, *tmp;
    jsval v;

    JSObject *scopeChain = GetScopeChain(cx);

    obj = NULL;
    for (tmp = scopeChain; tmp; tmp = tmp->getParent()) {
        Class *clasp = tmp->getClass();
        if (clasp == &js_BlockClass || clasp == &js_WithClass)
            continue;
        if (!tmp->getProperty(cx, JS_DEFAULT_XML_NAMESPACE_ID, Valueify(&v)))
            return JS_FALSE;
        if (!JSVAL_IS_PRIMITIVE(v)) {
            *vp = v;
            return JS_TRUE;
        }
        obj = tmp;
    }

    ns = js_ConstructObject(cx, &js_NamespaceClass, NULL, obj, 0, NULL);
    if (!ns)
        return JS_FALSE;
    v = OBJECT_TO_JSVAL(ns);
    if (!obj->defineProperty(cx, JS_DEFAULT_XML_NAMESPACE_ID, Valueify(v),
                             PropertyStub, PropertyStub, JSPROP_PERMANENT)) {
        return JS_FALSE;
    }
    *vp = v;
    return JS_TRUE;
}

JSBool
js_SetDefaultXMLNamespace(JSContext *cx, const Value &v)
{
    Value argv[2];
    argv[0].setString(cx->runtime->emptyString);
    argv[1] = v;
    JSObject *ns = js_ConstructObject(cx, &js_NamespaceClass, NULL, NULL, 2, argv);
    if (!ns)
        return JS_FALSE;

    JSStackFrame *fp = js_GetTopStackFrame(cx);
    JSObject &varobj = fp->varobj(cx);
    if (!varobj.defineProperty(cx, JS_DEFAULT_XML_NAMESPACE_ID, ObjectValue(*ns),
                               PropertyStub, PropertyStub, JSPROP_PERMANENT)) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSBool
js_ToAttributeName(JSContext *cx, Value *vp)
{
    JSObject *qn;

    qn = ToAttributeName(cx, Jsvalify(*vp));
    if (!qn)
        return JS_FALSE;
    vp->setObject(*qn);
    return JS_TRUE;
}

JSString *
js_EscapeAttributeValue(JSContext *cx, JSString *str, JSBool quote)
{
    JSCharBuffer cb(cx);
    return EscapeAttributeValue(cx, cb, str, quote);
}

JSString *
js_AddAttributePart(JSContext *cx, JSBool isName, JSString *str, JSString *str2)
{
    size_t len, len2, newlen;
    const jschar *chars, *chars2;
    jschar *newchars;

    str->getCharsAndLength(chars, len);
    str2->getCharsAndLength(chars2, len2);
    newlen = (isName) ? len + 1 + len2 : len + 2 + len2 + 1;
    newchars = (jschar *) cx->malloc((newlen+1) * sizeof(jschar));
    if (!newchars)
        return NULL;

    js_strncpy(newchars, chars, len);
    newchars += len;
    if (isName) {
        *newchars++ = ' ';
        js_strncpy(newchars, chars2, len2);
        newchars += len2;
    } else {
        *newchars++ = '=';
        *newchars++ = '"';
        js_strncpy(newchars, chars2, len2);
        newchars += len2;
        *newchars++ = '"';
    }
    *newchars = 0;
    return js_NewString(cx, newchars - newlen, newlen);
}

JSString *
js_EscapeElementValue(JSContext *cx, JSString *str)
{
    JSCharBuffer cb(cx);
    return EscapeElementValue(cx, cb, str, 0);
}

JSString *
js_ValueToXMLString(JSContext *cx, const Value &v)
{
    return ToXMLString(cx, Jsvalify(v), 0);
}

static JSBool
anyname_toString(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = ATOM_TO_JSVAL(cx->runtime->atomState.starAtom);
    return JS_TRUE;
}

JSBool
js_GetAnyName(JSContext *cx, jsid *idp)
{
    JSObject *obj;

    obj = cx->compartment->anynameObject;
    if (!obj) {
        JSRuntime *rt = cx->runtime;

        obj = NewNonFunction<WithProto::Given>(cx, &js_AnyNameClass, NULL, NULL);
        if (!obj)
            return false;

        InitXMLQName(obj, rt->emptyString, rt->emptyString,
                     ATOM_TO_STRING(rt->atomState.starAtom));
        METER(xml_stats.qname);

        /*
         * Avoid entraining any Object.prototype found via cx's scope
         * chain or global object.  This loses the default toString,
         * but no big deal: we want to customize toString anyway for
         * clearer diagnostics.
         */
        if (!JS_DefineFunction(cx, obj, js_toString_str,
                               anyname_toString, 0, 0))
            return false;

        JS_ASSERT(!obj->getProto());
        JS_ASSERT(!obj->getParent());

        cx->compartment->anynameObject = obj;
    }

    *idp = OBJECT_TO_JSID(obj);
    return true;
}

JSBool
js_FindXMLProperty(JSContext *cx, const Value &nameval, JSObject **objp, jsid *idp)
{
    JSObject *nameobj;
    jsval v;
    JSObject *qn;
    jsid funid;
    JSObject *obj, *target, *proto, *pobj;
    JSXML *xml;
    JSBool found;
    JSProperty *prop;

    JS_ASSERT(nameval.isObject());
    nameobj = &nameval.toObject();
    if (nameobj->getClass() == &js_AnyNameClass) {
        v = ATOM_TO_JSVAL(cx->runtime->atomState.starAtom);
        nameobj = js_ConstructObject(cx, &js_QNameClass, NULL, NULL, 1,
                                     Valueify(&v));
        if (!nameobj)
            return JS_FALSE;
    } else {
        JS_ASSERT(nameobj->getClass() == &js_AttributeNameClass ||
                  nameobj->getClass() == &js_QNameClass);
    }

    qn = nameobj;
    if (!IsFunctionQName(cx, qn, &funid))
        return JS_FALSE;

    obj = &js_GetTopStackFrame(cx)->scopeChain();
    do {
        /* Skip any With object that can wrap XML. */
        target = obj;
        while (target->getClass() == &js_WithClass) {
             proto = target->getProto();
             if (!proto)
                 break;
             target = proto;
        }

        if (target->isXML()) {
            if (JSID_IS_VOID(funid)) {
                xml = (JSXML *) target->getPrivate();
                found = HasNamedProperty(xml, qn);
            } else {
                if (!HasFunctionProperty(cx, target, funid, &found))
                    return JS_FALSE;
            }
            if (found) {
                *idp = OBJECT_TO_JSID(nameobj);
                *objp = target;
                return JS_TRUE;
            }
        } else if (!JSID_IS_VOID(funid)) {
            if (!target->lookupProperty(cx, funid, &pobj, &prop))
                return JS_FALSE;
            if (prop) {
                *idp = funid;
                *objp = target;
                return JS_TRUE;
            }
        }
    } while ((obj = obj->getParent()) != NULL);

    JSAutoByteString printable;
    if (js_ValueToPrintable(cx, ObjectValue(*nameobj), &printable)) {
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                     JSMSG_UNDEFINED_XML_NAME, printable.ptr());
    }
    return JS_FALSE;
}

static JSBool
GetXMLFunction(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JS_ASSERT(obj->isXML());

    /*
     * See comments before xml_lookupProperty about the need for the proto
     * chain lookup.
     */
    JSObject *target = obj;
    AutoObjectRooter tvr(cx);
    for (;;) {
        if (!js_GetProperty(cx, target, id, Valueify(vp)))
            return false;
        if (VALUE_IS_FUNCTION(cx, *vp))
            return true;
        target = target->getProto();
        if (target == NULL)
            break;
        tvr.setObject(target);
    }

    JSXML *xml = (JSXML *) obj->getPrivate();
    if (!HasSimpleContent(xml))
        return true;

    /* Search in String.prototype to implement 11.2.2.1 Step 3(f). */
    if (!js_GetClassPrototype(cx, NULL, JSProto_String, tvr.addr()))
        return false;

    JS_ASSERT(tvr.object());
    return tvr.object()->getProperty(cx, id, Valueify(vp));
}

static JSXML *
GetPrivate(JSContext *cx, JSObject *obj, const char *method)
{
    JSXML *xml;

    xml = (JSXML *) GetInstancePrivate(cx, obj, &js_XMLClass, NULL);
    if (!xml) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_METHOD,
                             js_XML_str, method, obj->getClass()->name);
    }
    return xml;
}

JSBool
js_GetXMLDescendants(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSXML *xml, *list;

    xml = GetPrivate(cx, obj, "descendants internal method");
    if (!xml)
        return JS_FALSE;

    list = Descendants(cx, xml, id);
    if (!list)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(list->object);
    return JS_TRUE;
}

JSBool
js_DeleteXMLListElements(JSContext *cx, JSObject *listobj)
{
    JSXML *list;
    uint32 n;

    list = (JSXML *) listobj->getPrivate();
    for (n = list->xml_kids.length; n != 0; --n)
        DeleteListElement(cx, list, 0);

    return JS_TRUE;
}

struct JSXMLFilter
{
    JSXML               *list;
    JSXML               *result;
    JSXML               *kid;
    JSXMLArrayCursor    cursor;

    JSXMLFilter(JSXML *list, JSXMLArray *array)
      : list(list), result(NULL), kid(NULL), cursor(array) {}

    ~JSXMLFilter() {}
};

static void
xmlfilter_trace(JSTracer *trc, JSObject *obj)
{
    JSXMLFilter *filter = (JSXMLFilter *) obj->getPrivate();
    if (!filter)
        return;

    JS_ASSERT(filter->list);
    JS_CALL_TRACER(trc, filter->list, JSTRACE_XML, "list");
    if (filter->result)
        JS_CALL_TRACER(trc, filter->result, JSTRACE_XML, "result");
    if (filter->kid)
        JS_CALL_TRACER(trc, filter->kid, JSTRACE_XML, "kid");

    /*
     * We do not need to trace the cursor as that would be done when
     * tracing the filter->list.
     */
}

static void
xmlfilter_finalize(JSContext *cx, JSObject *obj)
{
    JSXMLFilter *filter = (JSXMLFilter *) obj->getPrivate();
    if (!filter)
        return;

    cx->destroy(filter);
}

Class js_XMLFilterClass = {
    "XMLFilter",
    JSCLASS_HAS_PRIVATE | JSCLASS_IS_ANONYMOUS | JSCLASS_MARK_IS_TRACE,
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    xmlfilter_finalize,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    JS_CLASS_TRACE(xmlfilter_trace)
};

JSBool
js_StepXMLListFilter(JSContext *cx, JSBool initialized)
{
    jsval *sp;
    JSObject *obj, *filterobj, *resobj, *kidobj;
    JSXML *xml, *list;
    JSXMLFilter *filter;

    LeaveTrace(cx);
    sp = Jsvalify(cx->regs->sp);
    if (!initialized) {
        /*
         * We haven't iterated yet, so initialize the filter based on the
         * value stored in sp[-2].
         */
        if (!VALUE_IS_XML(sp[-2])) {
            js_ReportValueError(cx, JSMSG_NON_XML_FILTER, -2, Valueify(sp[-2]), NULL);
            return JS_FALSE;
        }
        obj = JSVAL_TO_OBJECT(sp[-2]);
        xml = (JSXML *) obj->getPrivate();

        if (xml->xml_class == JSXML_CLASS_LIST) {
            list = xml;
        } else {
            obj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
            if (!obj)
                return JS_FALSE;

            /*
             * Root just-created obj. sp[-2] cannot be used yet for rooting
             * as it may be the only root holding xml.
             */
            sp[-1] = OBJECT_TO_JSVAL(obj);
            list = (JSXML *) obj->getPrivate();
            if (!Append(cx, list, xml))
                return JS_FALSE;
        }

        filterobj = NewNonFunction<WithProto::Given>(cx, &js_XMLFilterClass, NULL, NULL);
        if (!filterobj)
            return JS_FALSE;

        /*
         * Init all filter fields before setPrivate exposes it to
         * xmlfilter_trace or xmlfilter_finalize.
         */
        filter = cx->create<JSXMLFilter>(list, &list->xml_kids);
        if (!filter)
            return JS_FALSE;
        filterobj->setPrivate(filter);

        /* Store filterobj to use in the later iterations. */
        sp[-2] = OBJECT_TO_JSVAL(filterobj);

        resobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
        if (!resobj)
            return JS_FALSE;

        /* This also roots resobj. */
        filter->result = (JSXML *) resobj->getPrivate();
    } else {
        /* We have iterated at least once. */
        JS_ASSERT(!JSVAL_IS_PRIMITIVE(sp[-2]));
        JS_ASSERT(JSVAL_TO_OBJECT(sp[-2])->getClass() == &js_XMLFilterClass);
        filter = (JSXMLFilter *) JSVAL_TO_OBJECT(sp[-2])->getPrivate();
        JS_ASSERT(filter->kid);

        /* Check if the filter expression wants to append the element. */
        if (js_ValueToBoolean(Valueify(sp[-1])) &&
            !Append(cx, filter->result, filter->kid)) {
            return JS_FALSE;
        }
    }

    /* Do the iteration. */
    filter->kid = (JSXML *) filter->cursor.getNext();
    if (!filter->kid) {
        /*
         * Do not defer finishing the cursor until the next GC cycle to avoid
         * accumulation of dead cursors associated with filter->list.
         */
        filter->cursor.disconnect();
        JS_ASSERT(filter->result->object);
        sp[-2] = OBJECT_TO_JSVAL(filter->result->object);
        kidobj = NULL;
    } else {
        kidobj = js_GetXMLObject(cx, filter->kid);
        if (!kidobj)
            return JS_FALSE;
    }

    /* Null as kidobj at sp[-1] signals filter termination. */
    sp[-1] = OBJECT_TO_JSVAL(kidobj);
    return JS_TRUE;
}

JSObject *
js_ValueToXMLObject(JSContext *cx, const Value &v)
{
    return ToXML(cx, Jsvalify(v));
}

JSObject *
js_ValueToXMLListObject(JSContext *cx, const Value &v)
{
    return ToXMLList(cx, Jsvalify(v));
}

JSObject *
js_NewXMLSpecialObject(JSContext *cx, JSXMLClass xml_class, JSString *name,
                       JSString *value)
{
    uintN flags;
    JSObject *obj;
    JSXML *xml;
    JSObject *qn;

    if (!GetXMLSettingFlags(cx, &flags))
        return NULL;

    if ((xml_class == JSXML_CLASS_COMMENT &&
         (flags & XSF_IGNORE_COMMENTS)) ||
        (xml_class == JSXML_CLASS_PROCESSING_INSTRUCTION &&
         (flags & XSF_IGNORE_PROCESSING_INSTRUCTIONS))) {
        return js_NewXMLObject(cx, JSXML_CLASS_TEXT);
    }

    obj = js_NewXMLObject(cx, xml_class);
    if (!obj)
        return NULL;
    xml = (JSXML *) obj->getPrivate();
    if (name) {
        qn = NewXMLQName(cx, cx->runtime->emptyString, NULL, name);
        if (!qn)
            return NULL;
        xml->name = qn;
    }
    xml->xml_value = value;
    return obj;
}

JSString *
js_MakeXMLCDATAString(JSContext *cx, JSString *str)
{
    JSCharBuffer cb(cx);
    return MakeXMLCDATAString(cx, cb, str);
}

JSString *
js_MakeXMLCommentString(JSContext *cx, JSString *str)
{
    JSCharBuffer cb(cx);
    return MakeXMLCommentString(cx, cb, str);
}

JSString *
js_MakeXMLPIString(JSContext *cx, JSString *name, JSString *str)
{
    JSCharBuffer cb(cx);
    return MakeXMLPIString(cx, cb, name, str);
}

#endif /* JS_HAS_XML_SUPPORT */
