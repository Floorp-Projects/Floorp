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

#include "jsstddef.h"
#include "jsconfig.h"

#if JS_HAS_XML_SUPPORT

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
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

#ifdef DEBUG
#include <string.h>     /* for #ifdef DEBUG memset calls */
#endif

/*
 * NOTES
 * - in the js shell, you must options('xml') to start using XML literals
 *
 * TODO
 * - XML_NYI
 * - equality
 * - concatenation
 * - for each in loops
 * - Improve code in js_Interpret for CDATA, COMMENT, and PI
 * - JSCLASS_W3C_XML_INFO_ITEM support -- live two-way binding to Gecko's DOM!
 * - JS_DEFAULT_XML_NAMESPACE_ID is bogus, see jsapi.h XXXbe comment
 * - Scan basic XML entities: &lt; &gt; &amp; &quot;
 * - clean up GC_MARK_DEBUG to include private reference paths?
 * - JSCLASS_NO_DEFAULT_NEW_OBJ or some such to avoid wasted obj alloc?
 * - JS_TypeOfValue sure could use a cleaner interface to "types"
 * - see also XXXbe
 */

/*
 * Random utilities and global functions.
 */
const char js_isXMLName_str[]   = "isXMLName";
const char js_Namespace_str[]   = "Namespace";
const char js_QName_str[]       = "QName";
const char js_XML_str[]         = "XML";
const char js_XMLList_str[]     = "XMLList";
const char js_localName_str[]   = "localName";
const char js_prefix_str[]      = "prefix";
const char js_toXMLString_str[] = "toXMLString";
const char js_uri_str[]         = "uri";

const char js_amp_entity_str[]  = "&amp;";
const char js_gt_entity_str[]   = "&gt;";
const char js_lt_entity_str[]   = "&lt;";
const char js_quot_entity_str[] = "&quot;";

#define IS_EMPTY(str) (JSSTRING_LENGTH(str) == 0)
#define IS_STAR(str)  (JSSTRING_LENGTH(str) == 1 && *JSSTRING_CHARS(str) == '*')

static JSBool
xml_isXMLName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    *rval = BOOLEAN_TO_JSVAL(js_IsXMLName(cx, argv[0]));
    return JS_TRUE;
}

/*
 * Namespace class and library functions.
 */
static JSBool
namespace_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static void
namespace_finalize(JSContext *cx, JSObject *obj)
{
    JSXMLNamespace *ns;

    ns = (JSXMLNamespace *) JS_GetPrivate(cx, obj);
    if (ns) {
        JS_ASSERT(ns->object);
        ns->object = NULL;
        js_DestroyXMLNamespace(cx, ns);
    }
}

static void
namespace_mark_private(JSContext *cx, JSXMLNamespace *ns, void *arg)
{
    JS_MarkGCThing(cx, ns->prefix, js_prefix_str, arg);
    JS_MarkGCThing(cx, ns->uri, js_uri_str, arg);
}

static void
namespace_mark_vector(JSContext *cx, JSXMLNamespace **vec, uint32 len,
                      void *arg)
{
    uint32 i;
    JSXMLNamespace *ns;

    for (i = 0; i < len; i++) {
        ns = vec[i];
        if (ns->object) {
#ifdef GC_MARK_DEBUG
            char buf[100];

            JS_snprintf(buf, sizeof buf, "%s=%s",
                        ns->prefix ? JS_GetStringBytes(ns->prefix) : "",
                        JS_GetStringBytes(ns->uri));
#else
            const char *buf = NULL;
#endif
            JS_MarkGCThing(cx, ns->object, buf, arg);
        } else {
            namespace_mark_private(cx, ns, arg);
        }
    }
}

static uint32
namespace_mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSXMLNamespace *ns;

    ns = (JSXMLNamespace *) JS_GetPrivate(cx, obj);
    namespace_mark_private(cx, ns, arg);
    return 0;
}

static JSClass namespace_class = {
    "Namespace",       JSCLASS_HAS_PRIVATE | JSCLASS_CONSTRUCT_PROTOTYPE,
    JS_PropertyStub,   JS_PropertyStub,   namespace_getProperty, NULL,
    JS_EnumerateStub,  JS_ResolveStub,    JS_ConvertStub,    namespace_finalize,
    NULL,              NULL,              NULL,              NULL,
    NULL,              NULL,              namespace_mark,    NULL
};

enum namespace_tinyid {
    NAMESPACE_PREFIX = -1,
    NAMESPACE_URI = -2
};

#define NAMESPACE_ATTRS (JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED)

static JSPropertySpec namespace_props[] = {
    {js_prefix_str,    NAMESPACE_PREFIX,  NAMESPACE_ATTRS,   0, 0},
    {js_uri_str,       NAMESPACE_URI,     NAMESPACE_ATTRS,   0, 0},
    {0,0,0,0,0}
};

static JSBool
namespace_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSXMLNamespace *ns;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    ns = (JSXMLNamespace *)
         JS_GetInstancePrivate(cx, obj, &namespace_class, NULL);
    if (!ns)
        return JS_TRUE;

    switch (JSVAL_TO_INT(id)) {
      case NAMESPACE_PREFIX:
        *vp = ns->prefix ? STRING_TO_JSVAL(ns->prefix) : JSVAL_VOID;
        break;
      case NAMESPACE_URI:
        *vp = STRING_TO_JSVAL(ns->uri);
        break;
    }
    return JS_TRUE;
}

static JSBool
namespace_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
    JSXMLNamespace *ns;

    ns = (JSXMLNamespace *)
         JS_GetInstancePrivate(cx, obj, &namespace_class, argv);
    if (!ns)
        return JS_FALSE;

    *rval = STRING_TO_JSVAL(ns->uri);
    return JS_TRUE;
}

static JSFunctionSpec namespace_methods[] = {
    {js_toString_str,  namespace_toString,        0,0,0},
    {0,0,0,0,0}
};

JSXMLNamespace *
js_NewXMLNamespace(JSContext *cx, JSString *prefix, JSString *uri)
{
    JSXMLNamespace *ns;

    ns = (JSXMLNamespace *) JS_malloc(cx, sizeof(JSXMLNamespace));
    if (!ns)
        return NULL;
    ns->object = NULL;
    ns->prefix = prefix;
    ns->uri = uri;
    return ns;
}

void
js_DestroyXMLNamespace(JSContext *cx, JSXMLNamespace *ns)
{
    if (!ns->object)
        JS_free(cx, ns);
}

JSObject *
js_NewXMLNamespaceObject(JSContext *cx, JSString *prefix, JSString *uri)
{
    JSXMLNamespace *ns;
    JSObject *obj;

    ns = js_NewXMLNamespace(cx, prefix, uri);
    if (!ns)
        return NULL;
    obj = js_GetXMLNamespaceObject(cx, ns);
    if (!obj)
        js_DestroyXMLNamespace(cx, ns);
    return obj;
}

JSObject *
js_GetXMLNamespaceObject(JSContext *cx, JSXMLNamespace *ns)
{
    JSObject *obj;

    obj = ns->object;
    if (obj) {
        JS_ASSERT(JS_GetPrivate(cx, obj) == ns);
        return obj;
    }
    obj = js_NewObject(cx, &namespace_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, ns)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    ns->object = obj;
    return obj;
}

/*
 * QName class and library functions.
 */
static JSBool
qname_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static void
qname_finalize(JSContext *cx, JSObject *obj)
{
    JSXMLQName *qn;

    qn = (JSXMLQName *) JS_GetPrivate(cx, obj);
    if (qn) {
        JS_ASSERT(qn->object);
        qn->object = NULL;
        js_DestroyXMLQName(cx, qn);
    }
}

static void
qname_mark_private(JSContext *cx, JSXMLQName *qn, void *arg)
{
    JS_MarkGCThing(cx, qn->uri, js_uri_str, arg);
    JS_MarkGCThing(cx, qn->prefix, js_prefix_str, arg);
    JS_MarkGCThing(cx, qn->localName, js_localName_str, arg);
}

static uint32
qname_mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSXMLQName *qn;

    qn = (JSXMLQName *) JS_GetPrivate(cx, obj);
    qname_mark_private(cx, qn, arg);
    return 0;
}

static JSClass qname_class = {
    "QName",           JSCLASS_HAS_PRIVATE | JSCLASS_CONSTRUCT_PROTOTYPE,
    JS_PropertyStub,   JS_PropertyStub,   qname_getProperty, NULL,
    JS_EnumerateStub,  JS_ResolveStub,    JS_ConvertStub,    qname_finalize,
    NULL,              NULL,              NULL,              NULL,
    NULL,              NULL,              qname_mark,        NULL
};

/*
 * Uninitialized classes for AttributeName and AnyName, which are like QName,
 * except that they're never constructed and they have no getters.
 */
static JSClass attribute_name_class = {
    "AttributeName",   JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    JS_ConvertStub,    qname_finalize,
    NULL,              NULL,              NULL,              NULL,
    NULL,              NULL,              qname_mark,        NULL
};

static JSClass any_name_class = {
    "AnyName",         JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    JS_ConvertStub,    qname_finalize,
    NULL,              NULL,              NULL,              NULL,
    NULL,              NULL,              qname_mark,        NULL
};

enum qname_tinyid {
    QNAME_URI = -1,
    QNAME_LOCALNAME = -2
};

#define QNAME_ATTRS     (JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED)

static JSPropertySpec qname_props[] = {
    {js_uri_str,       QNAME_URI,         QNAME_ATTRS,       0, 0},
    {js_localName_str, QNAME_LOCALNAME,   QNAME_ATTRS,       0, 0},
    {0,0,0,0,0}
};

static JSBool
qname_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSXMLQName *qn;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    qn = (JSXMLQName *) JS_GetInstancePrivate(cx, obj, &qname_class, NULL);
    if (!qn)
        return JS_TRUE;

    switch (JSVAL_TO_INT(id)) {
      case QNAME_URI:
        *vp = qn->uri ? STRING_TO_JSVAL(qn->uri) : JSVAL_NULL;
        break;
      case QNAME_LOCALNAME:
        *vp = STRING_TO_JSVAL(qn->localName);
        break;
    }
    return JS_TRUE;
}

static JSBool
qname_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    JSXMLQName *qn;
    JSString *str, *qualstr;

    qn = (JSXMLQName *) JS_GetInstancePrivate(cx, obj, &qname_class, argv);
    if (!qn)
        return JS_FALSE;

    if (!qn->uri) {
        /* No uri means wildcard qualifier. */
        str = ATOM_TO_STRING(cx->runtime->atomState.starQualifierAtom);
    } else if (IS_EMPTY(qn->uri)) {
        /* Empty string for uri means localName is in no namespace. */
        str = cx->runtime->emptyString;
    } else {
        qualstr = ATOM_TO_STRING(cx->runtime->atomState.qualifierAtom);
        str = js_ConcatStrings(cx, qn->uri, qualstr);
        if (!str)
            return JS_FALSE;
    }
    str = js_ConcatStrings(cx, str, qn->localName);
    if (!str)
        return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSFunctionSpec qname_methods[] = {
    {js_toString_str,  qname_toString,    0,0,0},
    {0,0,0,0,0}
};

JSXMLQName *
js_NewXMLQName(JSContext *cx, JSString *uri, JSString *prefix,
               JSString *localName)
{
    JSXMLQName *qn;

    qn = (JSXMLQName *) JS_malloc(cx, sizeof(JSXMLQName));
    if (!qn)
        return NULL;
    qn->object = NULL;
    qn->uri = uri;
    qn->prefix = prefix;
    qn->localName = localName;
    return qn;
}

void
js_DestroyXMLQName(JSContext *cx, JSXMLQName *qn)
{
    if (!qn->object)
        JS_free(cx, qn);
}

JSObject *
js_NewXMLQNameObject(JSContext *cx, JSString *uri, JSString *prefix,
                     JSString *localName)
{
    JSXMLQName *qn;
    JSObject *obj;

    qn = js_NewXMLQName(cx, uri, prefix, localName);
    if (!qn)
        return NULL;
    obj = js_GetXMLQNameObject(cx, qn);
    if (!obj)
        js_DestroyXMLQName(cx, qn);
    return obj;
}

JSObject *
js_GetXMLQNameObject(JSContext *cx, JSXMLQName *qn)
{
    JSObject *obj;

    obj = qn->object;
    if (obj) {
        JS_ASSERT(JS_GetPrivate(cx, obj) == qn);
        return obj;
    }
    obj = js_NewObject(cx, &qname_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, qn)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    qn->object = obj;
    return obj;
}

JSObject *
js_ConstructQNameObject(JSContext *cx, jsval nsval, jsval lnval)
{
    jsval argv[2];

    /*
     * ECMA-357 11.1.2,
     * The _QualifiedIdentifier : PropertySelector :: PropertySelector_
     * production, step 2.
     */
    if (!JSVAL_IS_PRIMITIVE(nsval) &&
        OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(nsval)) == &any_name_class) {
        nsval = JSVAL_NULL;
    }

    argv[0] = nsval;
    argv[1] = lnval;
    return js_ConstructObject(cx, &qname_class, NULL, NULL, 2, argv);
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
    JSBool rv;
    JSErrorReporter older;
    JSXMLQName *qn;
    JSString *name;

    rv = JS_FALSE;
    older = JS_SetErrorReporter(cx, NULL);

    /*
     * Inline specialization of the QName constructor called with v passed as
     * the only argument, to compute the localName for the constructed qname,
     * without actually allocating the object or computing its uri and prefix.
     * See ECMA-357 13.1.2.1 step 1 and 13.3.2.
     */
    if (!JSVAL_IS_PRIMITIVE(v) &&
        OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == &qname_class) {
        qn = (JSXMLQName *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(v));
        name = qn->localName;
    } else {
        name = js_ValueToString(cx, v);
        if (!name)
            JS_ClearPendingException(cx);
    }

    if (name)
        rv = IsXMLName(JSSTRING_CHARS(name), JSSTRING_LENGTH(name));

    JS_SetErrorReporter(cx, older);
    return rv;
}

static JSBool
Namespace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval urival, prefixval;
    JSObject *uriobj;
    JSBool isNamespace, isQName;
    JSClass *clasp;
    JSString *empty, *prefix;
    JSXMLNamespace *ns, *ns2;
    JSXMLQName *qn;

    urival = argv[argc > 1];
    isNamespace = isQName = JS_FALSE;
    if (!JSVAL_IS_PRIMITIVE(urival)) {
        uriobj = JSVAL_TO_OBJECT(urival);
        clasp = OBJ_GET_CLASS(cx, uriobj);
        isNamespace = (clasp == &namespace_class);
        isQName = (clasp == &qname_class);
    }
#ifdef __GNUC__         /* suppress bogus gcc warnings */
    else uriobj = NULL;
#endif

    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
        /* Namespace called as function. */
        if (argc == 1 && isNamespace) {
            /* Namespace called with one Namespace argument is identity. */
            *rval = urival;
            return JS_TRUE;
        }

        /* Create and return a new QName object exactly as if constructed. */
        obj = js_NewObject(cx, &namespace_class, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }

    /*
     * Create and connect private data to rooted obj early, so we don't have
     * to worry about rooting string newborns hanging off of the private data
     * further below.
     */
    empty = cx->runtime->emptyString;
    ns = js_NewXMLNamespace(cx, empty, empty);
    if (!ns)
        return JS_FALSE;
    if (!JS_SetPrivate(cx, obj, ns)) {
        js_DestroyXMLNamespace(cx, ns);
        return JS_FALSE;
    }
    ns->object = obj;

    if (argc == 1) {
        if (isNamespace) {
            ns2 = (JSXMLNamespace *) JS_GetPrivate(cx, uriobj);
            ns->uri = ns2->uri;
            ns->prefix = ns2->prefix;
        } else if (isQName &&
                   (qn = (JSXMLQName *) JS_GetPrivate(cx, uriobj))->uri) {
            ns->uri = qn->uri;
            ns->prefix = qn->prefix;
        } else {
            ns->uri = js_ValueToString(cx, urival);
            if (!ns->uri)
                return JS_FALSE;

            /* NULL here represents *undefined* in ECMA-357 13.2.2 3(c)iii. */
            if (!IS_EMPTY(ns->uri))
                ns->prefix = NULL;
        }
    } else if (argc == 2) {
        if (isQName &&
            (qn = (JSXMLQName *) JS_GetPrivate(cx, uriobj))->uri) {
            ns->uri = qn->uri;
        } else {
            ns->uri = js_ValueToString(cx, urival);
            if (!ns->uri)
                return JS_FALSE;
        }

        prefixval = argv[0];
        if (IS_EMPTY(ns->uri)) {
            if (!JSVAL_IS_VOID(prefixval)) {
                prefix = js_ValueToString(cx, prefixval);
                if (!prefix)
                    return JS_FALSE;
                if (!IS_EMPTY(prefix)) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_BAD_XML_NAMESPACE);
                    return JS_FALSE;
                }
            }
        } else if (JSVAL_IS_VOID(prefixval) || !js_IsXMLName(cx, prefixval)) {
            /* NULL here represents *undefined* in ECMA-357 13.2.2 4(d) etc. */
            ns->prefix = NULL;
        } else {
            prefix = js_ValueToString(cx, prefixval);
            if (!prefix)
                return JS_FALSE;
            ns->prefix = prefix;
        }
    }

    return JS_TRUE;
}

static JSBool
QName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval nameval, nsval;
    JSBool isQName, isNamespace;
    JSXMLQName *qn;
    JSString *uri, *prefix, *name;
    JSObject *nsobj;
    JSClass *clasp;
    JSXMLNamespace *ns;

    nameval = argv[argc > 1];
    isQName = !JSVAL_IS_PRIMITIVE(nameval) &&
              OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(nameval)) == &qname_class;

    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
        /* QName called as function. */
        if (argc == 1 && isQName) {
            /* QName called with one QName argument is identity. */
            *rval = nameval;
            return JS_TRUE;
        }

        /* Create and return a new QName object exactly as if constructed. */
        obj = js_NewObject(cx, &qname_class, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }

    if (isQName) {
        /* If namespace is not specified and name is a QName, clone it. */
        qn = (JSXMLQName *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(nameval));
        if (argc == 1) {
            uri = qn->uri;
            prefix = qn->prefix;
            name = qn->localName;
            goto out;
        }

        /* Namespace and qname were passed -- use the qname's localName. */
        nameval = STRING_TO_JSVAL(qn->localName);
    }

    if (argc == 0) {
        name = cx->runtime->emptyString;
    } else {
        name = js_ValueToString(cx, nameval);
        if (!name)
            return JS_FALSE;

        /* Use argv[1] as a local root for name, even if it was not passed. */
        argv[1] = STRING_TO_JSVAL(name);
    }

    nsval = argv[0];
    if (argc == 1 || JSVAL_IS_VOID(nsval)) {
        if (IS_STAR(name)) {
            nsval = JSVAL_NULL;
        } else {
            if (!js_GetDefaultXMLNamespace(cx, &nsval))
                return JS_FALSE;
        }
    }

    if (JSVAL_IS_NULL(nsval)) {
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
            nsobj = JSVAL_TO_OBJECT(nsval);
            clasp = OBJ_GET_CLASS(cx, nsobj);
            isNamespace = (clasp == &namespace_class);
            isQName = (clasp == &qname_class);
        }
#ifdef __GNUC__         /* suppress bogus gcc warnings */
        else nsobj = NULL;
#endif

        if (isNamespace) {
            ns = (JSXMLNamespace *) JS_GetPrivate(cx, nsobj);
            uri = ns->uri;
            prefix = ns->prefix;
        } else if (isQName &&
                   (qn = (JSXMLQName *) JS_GetPrivate(cx, nsobj))->uri) {
            uri = qn->uri;
            prefix = qn->prefix;
        } else {
            uri = js_ValueToString(cx, nsval);
            if (!uri)
                return JS_FALSE;

            /* NULL here represents *undefined* in ECMA-357 13.2.2 3(c)iii. */
            prefix = IS_EMPTY(uri) ? cx->runtime->emptyString : NULL;
        }
    }

out:
    qn = js_NewXMLQName(cx, uri, prefix, name);
    if (!qn)
        return JS_FALSE;
    if (!JS_SetPrivate(cx, obj, qn)) {
        js_DestroyXMLQName(cx, qn);
        return JS_FALSE;
    }
    qn->object = obj;
    return JS_TRUE;
}

/*
 * XMLArray library functions.
 */
static JSBool
namespace_identity(const void *a, const void *b)
{
    const JSXMLNamespace *nsa = (const JSXMLNamespace *) a;
    const JSXMLNamespace *nsb = (const JSXMLNamespace *) b;

    if (nsa->prefix && nsb->prefix &&
        js_CompareStrings(nsa->prefix, nsb->prefix)) {
        return JS_FALSE;
    }
    if (nsa->prefix || nsb->prefix)
        return JS_FALSE;
    return !js_CompareStrings(nsa->uri, nsb->uri);
}

static JSBool
attr_identity(const void *a, const void *b)
{
    const JSXML *xmla = (const JSXML *) a;
    const JSXML *xmlb = (const JSXML *) b;
    JSXMLQName *qna = xmla->name;
    JSXMLQName *qnb = xmlb->name;

    JS_ASSERT(qna && qnb);
    if (!qna->uri ^ !qnb->uri)
        return JS_FALSE;
    if (qna->uri && js_CompareStrings(qna->uri, qnb->uri))
        return JS_FALSE;
    return !js_CompareStrings(qna->localName, qnb->localName);
}

JSBool
XMLArrayInit(JSContext *cx, JSXMLArray *set, uint32 length)
{
    set->count = 0;
    set->length = length;
    if (length == 0) {
        set->vector = NULL;
    } else {
        if ((size_t)length > ~(size_t)0 / sizeof(void *) ||
            !(set->vector = (void **) calloc(length, sizeof(void *)))) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

typedef void
(* JS_DLL_CALLBACK JSXMLArrayElemDtor)(JSContext *cx, void *elt);

void
XMLArrayFinish(JSContext *cx, JSXMLArray *set, JSXMLArrayElemDtor dtor)
{
    uint32 i, n;

    for (i = 0, n = set->count; i < n; i++)
        dtor(cx, set->vector[i]);
    JS_free(cx, set->vector);
#ifdef DEBUG
    memset(set, 0xd5, sizeof *set);
#endif
}

#define NOT_FOUND       ((uint32) -1)

uint32
XMLArrayHasMember(JSXMLArray *set, void *elt, JSIdentityOp identity)
{
    void **vector;
    uint32 i, n;

    /* The identity op must not reallocate set->vector. */
    vector = set->vector;
    if (identity) {
        for (i = 0, n = set->count; i < n; i++) {
            if (identity(vector[i], elt))
                return i;
        }
    } else {
        for (i = 0, n = set->count; i < n; i++) {
            if (vector[i] == elt)
                return i;
        }
    }
    return NOT_FOUND;
}

/*
 * Grow set vector length by powers of two till LINEAR_THRESHOLD, after which
 * we grow it by LINEAR_INCREMENT.  Both must be powers of two, and threshold
 * should be greater than increment.
 */
#define LINEAR_THRESHOLD        256
#define LINEAR_INCREMENT        32

JSBool
XMLArrayInsert(JSContext *cx, JSXMLArray *set, uint32 index, void *elt)
{
    uint32 length, i;
    int log2;

    if (index >= set->count) {
        if (index >= set->length) {
            length = index + 1;
            if (index >= LINEAR_THRESHOLD) {
                length = JS_ROUNDUP(length, LINEAR_INCREMENT);
            } else {
                JS_CEILING_LOG2(log2, length);
                length = JS_BIT(log2);
            }
            if ((size_t)length > ~(size_t)0 / sizeof(void *) ||
                !(set->vector = (void **)
                                realloc(set->vector,
                                        length * sizeof(void *)))) {
                JS_ReportOutOfMemory(cx);
                return JS_FALSE;
            }
            set->length = length;
            for (i = set->count; i < index; i++)
                set->vector[i] = NULL;
        }
        set->count = index + 1;
    }

    set->vector[index] = elt;
    return JS_TRUE;
}

void *
XMLArrayDelete(JSContext *cx, JSXMLArray *set, uint32 index, JSBool compress)
{
    uint32 count;
    void **vector, *elt;

    count = set->count;
    if (index >= count)
        return NULL;

    vector = set->vector;
    elt = vector[index];
    if (compress) {
        while (++index < count)
            vector[index-1] = vector[index];
        set->count = count - 1;
    } else {
        vector[index] = NULL;
    }
    return elt;
}

#define XMLARRAY_INIT(x,a,l)        XMLArrayInit(x, a, l)
#define XMLARRAY_FINISH(x,a,d)      XMLArrayFinish(x, a, (JSXMLArrayElemDtor)d)
#define XMLARRAY_HAS_MEMBER(a,e,i)  XMLArrayHasMember(a, (void *)(e), i)
#define XMLARRAY_MEMBER(a,i,t)      ((t *) (a)->vector[i])
#define XMLARRAY_SET_MEMBER(a,i,e)  ((a)->vector[i] = (void *)(e))
#define XMLARRAY_INSERT(x,a,i,e)    XMLArrayInsert(x, a, i, (void *)(e))
#define XMLARRAY_APPEND(x,a,e)      XMLArrayInsert(x, a, (a)->count, (void*)(e))
#define XMLARRAY_DELETE(x,a,i,c,t)  ((t *) XMLArrayDelete(x, a, i, c))

/*
 * XML helper, object-ops, and library functions.  We start with the helpers,
 * in ECMA-357 order, but merging XML (9.1) and XMLList (9.2) helpers.
 */
static JSXML *
ParseXMLSource(JSContext *cx, JSString *src)
{
    jsval nsval;
    JSXMLNamespace *ns;
    size_t urilen, srclen, length, offset;
    jschar *chars;
    void *mark;
    JSTokenStream *ts;
    JSParseNode *pn;
    JSXML *xml;

    static const char prefix[] = "<parent xmlns='";
    static const char middle[] = "'>";
    static const char suffix[] = "</parent>";

#define constrlen(constr)   (sizeof(constr) - 1)

    if (!js_GetDefaultXMLNamespace(cx, &nsval))
        return NULL;
    ns = (JSXMLNamespace *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(nsval));

    urilen = JSSTRING_LENGTH(ns->uri);
    srclen = JSSTRING_LENGTH(src);
    length = constrlen(prefix) + urilen + constrlen(middle) + srclen +
             constrlen(suffix);

    chars = (jschar *) JS_malloc(cx, (length + 1) * sizeof(jschar));
    if (!chars)
        return NULL;

    js_InflateStringToBuffer(chars, prefix, constrlen(prefix));
    offset = constrlen(prefix);
    js_strncpy(chars + offset, JSSTRING_CHARS(ns->uri), urilen);
    offset += urilen;
    js_InflateStringToBuffer(chars + offset, middle, constrlen(middle));
    offset += constrlen(middle);
    js_strncpy(chars + offset, JSSTRING_CHARS(src), srclen);
    offset += srclen;
    js_InflateStringToBuffer(chars + offset, suffix, constrlen(suffix));

    mark = JS_ARENA_MARK(&cx->tempPool);
    ts = js_NewBufferTokenStream(cx, chars, length);
    if (!ts)
        return NULL;

    pn = js_ParseXMLTokenStream(cx, cx->fp->scopeChain, ts, JS_FALSE);
    if (!pn) {
        xml = NULL;
    } else {
        xml = js_ParseNodeToXML(cx, pn, 0);
        if (!xml)
            return JS_FALSE;
    }

    JS_ARENA_RELEASE(&cx->tempPool, mark);
    JS_free(cx, chars);
    return xml;

#undef constrlen
}

static JSXML *
ToXML(JSContext *cx, jsval v)
{
    JSObject *obj;
    JSClass *clasp;
    JSXML *xml;
    JSString *str;
    uint32 length;

    if (JSVAL_IS_PRIMITIVE(v)) {
        if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
            goto bad;
    } else {
        obj = JSVAL_TO_OBJECT(v);
        clasp = OBJ_GET_CLASS(cx, obj);
        if (clasp == &js_XMLClass) {
            xml = (JSXML *) JS_GetPrivate(cx, obj);
            if (xml->xml_class == JSXML_CLASS_LIST) {
                if (xml->xml_kids.count != 1)
                    goto bad;
                xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
                JS_ASSERT(xml->xml_class != JSXML_CLASS_LIST);
            }
            return xml;
        }

        if (clasp->flags & JSCLASS_W3C_XML_INFO_ITEM) {
        }

        if (clasp != &js_StringClass &&
            clasp != &js_NumberClass &&
            clasp != &js_BooleanClass) {
            goto bad;
        }
    }

    str = js_ValueToString(cx, v);
    if (!str)
        return NULL;
    if (IS_EMPTY(str)) {
        length = 0;
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
        xml = (JSXML *) JS_GetPrivate(cx, obj);
        xml->xml_value = cx->runtime->emptyString;
    } else if (length == 1) {
        xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
        if (!js_GetXMLObject(cx, xml))
            return NULL;
        xml->parent = NULL;
    } else {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    return xml;

bad:
    str = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK, v, NULL);
    if (str) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_XML_CONVERSION,
                             JS_GetStringBytes(str));
    }
    return NULL;
}

static JSBool
Append(JSContext *cx, JSXML *list, JSXML *kid);

static JSXML *
ToXMLList(JSContext *cx, jsval v)
{
    JSObject *obj, *listobj;
    JSClass *clasp;
    JSXML *xml, *list, *kid;
    JSString *str;
    uint32 i, length;

    if (JSVAL_IS_PRIMITIVE(v)) {
        if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
            goto bad;
    } else {
        obj = JSVAL_TO_OBJECT(v);
        clasp = OBJ_GET_CLASS(cx, obj);
        if (clasp == &js_XMLClass) {
            xml = (JSXML *) JS_GetPrivate(cx, obj);
            if (xml->xml_class != JSXML_CLASS_LIST) {
                listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
                if (!listobj)
                    return NULL;
                list = (JSXML *) JS_GetPrivate(cx, listobj);
                if (!Append(cx, list, xml))
                    return NULL;
                return list;
            }
            return xml;
        }

        if (clasp->flags & JSCLASS_W3C_XML_INFO_ITEM) {
            JS_ASSERT(0);
        }

        if (clasp != &js_StringClass &&
            clasp != &js_NumberClass &&
            clasp != &js_BooleanClass) {
            goto bad;
        }
    }

    str = js_ValueToString(cx, v);
    if (!str)
        return NULL;
    if (IS_EMPTY(str)) {
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

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return NULL;
    list = (JSXML *) JS_GetPrivate(cx, listobj);
    for (i = 0; i < length; i++) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
        kid->parent = NULL;
        if (!Append(cx, list, kid))
            return NULL;
    }
    return list;

bad:
    str = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK, v, NULL);
    if (str) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_XMLLIST_CONVERSION,
                             JS_GetStringBytes(str));
    }
    return NULL;
}

/*
 * ECMA-357 10.2.1 17(d-g) pulled out into a common subroutine that appends
 * equals, a double quote, an attribute value, and a closing double quote.
 */
static void
AppendAttributeValue(JSStringBuffer *sb, JSString *valstr)
{
    js_AppendCString(sb, "=\"");
    js_AppendJSString(sb, valstr);
    js_AppendChar(sb, '"');
}

/*
 * ECMA-357 10.2.1.1 EscapeElementValue helper method.
 * This function takes ownership of sb->base, if sb is non-null, in all cases.
 */
static JSString *
EscapeElementValue(JSContext *cx, JSStringBuffer *sb, JSString *str)
{
    size_t length, newlength;
    const jschar *cp, *start, *end;
    jschar c;

    length = newlength = JSSTRING_LENGTH(str);
    for (cp = start = JSSTRING_CHARS(str), end = cp + length; cp < end; cp++) {
        c = *cp;
        if (c == '<' || c == '>')
            newlength += 3;
        else if (c == '&')
            newlength += 4;
    }
    if ((sb && STRING_BUFFER_OFFSET(sb) != 0) || newlength > length) {
        JSStringBuffer localSB;
        if (!sb) {
            sb = &localSB;
            js_InitStringBuffer(sb);
        }
        if (!sb->grow(sb, STRING_BUFFER_OFFSET(sb) + newlength)) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        for (cp = start; cp < end; cp++) {
            c = *cp;
            if (c == '<')
                js_AppendCString(sb, js_lt_entity_str);
            else if (c == '>')
                js_AppendCString(sb, js_gt_entity_str);
            else if (c == '&')
                js_AppendCString(sb, js_amp_entity_str);
            else
                js_AppendChar(sb, c);
        }
        JS_ASSERT(STRING_BUFFER_OK(sb));
        str = js_NewString(cx, sb->base, STRING_BUFFER_OFFSET(sb), 0);
        if (!str)
            js_FinishStringBuffer(sb);
    }
    return str;
}

/*
 * ECMA-357 10.2.1.2 EscapeAttributeValue helper method.
 * This function takes ownership of sb->base, if sb is non-null, in all cases.
 */
static JSString *
EscapeAttributeValue(JSContext *cx, JSStringBuffer *sb, JSString *str)
{
    size_t length, newlength;
    const jschar *cp, *start, *end;
    jschar c;

    length = newlength = JSSTRING_LENGTH(str);
    for (cp = start = JSSTRING_CHARS(str), end = cp + length; cp < end; cp++) {
        c = *cp;
        if (c == '"')
            newlength += 5;
        else if (c == '<')
            newlength += 3;
        else if (c == '&' || c == '\n' || c == '\r' || c == '\t')
            newlength += 4;
    }
    if ((sb && STRING_BUFFER_OFFSET(sb) != 0) || newlength > length) {
        JSStringBuffer localSB;
        if (!sb) {
            sb = &localSB;
            js_InitStringBuffer(sb);
        }
        if (!sb->grow(sb, STRING_BUFFER_OFFSET(sb) + newlength)) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        for (cp = start; cp < end; cp++) {
            c = *cp;
            if (c == '"')
                js_AppendCString(sb, js_quot_entity_str);
            else if (c == '<')
                js_AppendCString(sb, js_lt_entity_str);
            else if (c == '&')
                js_AppendCString(sb, js_amp_entity_str);
            else if (c == '\n')
                js_AppendCString(sb, "&#xA;");
            else if (c == '\r')
                js_AppendCString(sb, "&#xD;");
            else if (c == '\t')
                js_AppendCString(sb, "&#x9;");
            else
                js_AppendChar(sb, c);
        }
        JS_ASSERT(STRING_BUFFER_OK(sb));
        str = js_NewString(cx, sb->base, STRING_BUFFER_OFFSET(sb), 0);
        if (!str)
            js_FinishStringBuffer(sb);
    }
    return str;
}

static const char js_ignoreComments_str[]   = "ignoreComments";
static const char js_ignoreProcessingInstructions_str[]
                                            = "ignoreProcessingInstructions";
static const char js_ignoreWhitespace_str[] = "ignoreWhitespace";
static const char js_prettyPrinting_str[]   = "prettyPrinting";
static const char js_prettyIndent_str[]     = "prettyIndent";

static JSBool
GetXMLSetting(JSContext *cx, const char *name, jsval *vp)
{
    jsval v;

    if (!js_FindConstructor(cx, NULL, js_XML_str, &v))
        return JS_FALSE;
    if (!JSVAL_IS_FUNCTION(cx, v)) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }
    return JS_GetProperty(cx, JSVAL_TO_OBJECT(v), name, vp);
}

static JSString *
ChompXMLWhitespace(JSContext *cx, JSString *str)
{
    size_t length, newlength, offset;
    const jschar *cp, *start, *end;
    jschar c;

    length = JSSTRING_LENGTH(str);
    for (cp = start = JSSTRING_CHARS(str), end = cp + length; cp < end; cp++) {
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
    newlength = PTRDIFF(end, cp, jschar);
    if (newlength == length)
        return str;
    offset = PTRDIFF(cp, start, jschar);
    return js_NewDependentString(cx, str, offset, newlength, 0);
}

/* 13.3.5.4 [[GetNamespace]]([InScopeNamespaces]) */
static JSXMLNamespace *
GetNamespace(JSContext *cx, JSXMLQName *qn, JSXMLArray *inScopeNSes)
{
    JSXMLNamespace *match, *ns;
    uint32 i, n;

    JS_ASSERT(qn->uri);
    if (!qn->uri) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_XML_NAMESPACE);
        return NULL;
    }

    /* Look for a matching namespace in inScopeNSes, if provided. */
    match = NULL;
    if (inScopeNSes) {
        for (i = 0, n = inScopeNSes->count; i < n; i++) {
            ns = XMLARRAY_MEMBER(inScopeNSes, i, JSXMLNamespace);
            if (!js_CompareStrings(ns->uri, qn->uri) &&
                (ns->prefix == qn->prefix ||
                 (ns->prefix && qn->prefix &&
                  !js_CompareStrings(ns->prefix, qn->prefix)))) {
                match = ns;
                break;
            }
        }
    }

    /* If we didn't match, make a new namespace from qn. */
    if (!match)
        match = js_NewXMLNamespace(cx, qn->prefix, qn->uri);

    /* Share ns reference by converting it to GC'd allocation. */
    if (match && !js_GetXMLNamespaceObject(cx, match))
        return NULL;
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
    JSXMLNamespace *ns;
    JSString *prefix;

    /*
     * Try peeling off the last filename suffix or pathname component till
     * we have a valid XML name.  This heuristic will prefer "xul" given
     * ".../there.is.only.xul", "xbl" given ".../xbl", and "xbl2" given any
     * likely URI of the form ".../xbl2/2005".
     */
    start = JSSTRING_CHARS(uri);
    cp = end = start + JSSTRING_LENGTH(uri);
    while (--cp >= start) {
        if (*cp == '.' || *cp == '/') {
            ++cp;
            if (IsXMLName(cp, PTRDIFF(end, cp, jschar)))
                break;
            end = --cp;
        }
    }
    length = PTRDIFF(end, cp, jschar);

    /*
     * Now search through decls looking for a collision.  If we collide with
     * an existing prefix, start tacking on a hyphen and a serial number.
     */
    serial = 0;
    bp = NULL;
#ifdef __GNUC__         /* suppress bogus gcc warnings */
    newlength = 0;
#endif
    do {
        done = JS_TRUE;
        for (i = 0, n = decls->count; i < n; i++) {
            ns = XMLARRAY_MEMBER(decls, i, JSXMLNamespace);
            if (ns->prefix &&
                JSSTRING_LENGTH(ns->prefix) == length &&
                !memcmp(JSSTRING_CHARS(ns->prefix), cp, length)) {
                if (!bp) {
                    newlength = length + 2 + (size_t) log10(n);
                    bp = (jschar *)
                         JS_malloc(cx, (newlength + 1) * sizeof(jschar));
                    if (!bp)
                        return NULL;
                    js_strncpy(bp, cp, length);
                }

                ++serial;
                JS_ASSERT(serial < n);
                dp = bp + length + 2 + (size_t) log10(serial);
                *dp = 0;
                for (m = serial; m != 0; m /= 10)
                    *--dp = '0' + m % 10;
                *--dp = '-';
                JS_ASSERT(dp == cp + length);

                done = JS_FALSE;
                break;
            }
        }
    } while (!done);

    if (!bp) {
        offset = PTRDIFF(cp, start, jschar);
        prefix = js_NewDependentString(cx, uri, offset, length, 0);
    } else {
        prefix = js_NewString(cx, bp, newlength, 0);
        if (!prefix)
            JS_free(cx, bp);
    }
    return prefix;
}

/* ECMA-357 10.2.1 and 10.2.2 */
static JSString *
XMLToXMLString(JSContext *cx, JSXML *xml, JSXMLArray *ancestorNSes,
               uintN indentLevel)
{
    jsval v;
    JSBool pretty, indentKids;
    JSStringBuffer sb;
    JSString *str, *name, *prefix, *kidstr;
    size_t length, newlength, namelength;
    jschar *bp, *base;
    uint32 i, n;
    JSXMLArray empty, decls, ancdecls;
    JSXMLNamespace *ns, *ns2;
    uintN nextIndentLevel;
    JSXML *attr, *kid;

    static const jschar comment_prefix_ucNstr[] = {'<', '!', '-', '-'};
    static const jschar comment_suffix_ucNstr[] = {'-', '-', '>'};
    static const jschar pi_prefix_ucNstr[] = {'<', '?'};
    static const jschar pi_suffix_ucNstr[] = {'?', '>'};

    if (!GetXMLSetting(cx, js_prettyPrinting_str, &v))
        return NULL;
    if (!js_ValueToBoolean(cx, v, &pretty))
        return NULL;

    js_InitStringBuffer(&sb);
    if (pretty)
        js_RepeatChar(&sb, ' ', indentLevel);
    str = NULL;

    /* XXXbe Erratum: shouldn't CDATA be handled specially? */
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
        return EscapeElementValue(cx, &sb, str);

      case JSXML_CLASS_ATTRIBUTE:
        /* Step 5. */
        return EscapeAttributeValue(cx, &sb, xml->xml_value);

      case JSXML_CLASS_COMMENT:
        /* Step 6. */
        str = xml->xml_value;
        length = JSSTRING_LENGTH(str);
        newlength = STRING_BUFFER_OFFSET(&sb) + 4 + length + 3;
        bp = base = (jschar *)
                    JS_realloc(cx, sb.base, (newlength + 1) * sizeof(jschar));
        if (!bp) {
            js_FinishStringBuffer(&sb);
            return NULL;
        }

        bp += STRING_BUFFER_OFFSET(&sb);
        js_strncpy(bp, comment_prefix_ucNstr, 4);
        bp += 4;
        js_strncpy(bp, JSSTRING_CHARS(str), length);
        bp += length;
        js_strncpy(bp, comment_suffix_ucNstr, 3);
        bp[3] = 0;
        str = js_NewString(cx, base, newlength, 0);
        if (!str)
            free(base);
        return str;

      case JSXML_CLASS_PROCESSING_INSTRUCTION:
        /* Step 7. */
        name = xml->name->localName;
        namelength = JSSTRING_LENGTH(name);
        str = xml->xml_value;
        length = JSSTRING_LENGTH(str);
        newlength = STRING_BUFFER_OFFSET(&sb) + 2 + namelength + 1 + length + 2;
        bp = base = (jschar *)
                    JS_realloc(cx, sb.base, (newlength + 1) * sizeof(jschar));
        if (!bp) {
            js_FinishStringBuffer(&sb);
            return NULL;
        }

        bp += STRING_BUFFER_OFFSET(&sb);
        js_strncpy(bp, pi_prefix_ucNstr, 2);
        bp += 2;
        js_strncpy(bp, JSSTRING_CHARS(name), namelength);
        bp += namelength;
        *bp++ = (jschar) ' ';
        js_strncpy(bp, JSSTRING_CHARS(str), length);
        bp += length;
        js_strncpy(bp, pi_suffix_ucNstr, 2);
        bp[2] = 0;
        str = js_NewString(cx, base, newlength, 0);
        if (!str)
            free(base);
        return str;

      case JSXML_CLASS_LIST:
        /* ECMA-357 10.2.2. */
        for (i = 0, n = xml->xml_kids.count; i < n; i++) {
            if (pretty && i != 0)
                js_AppendChar(&sb, '\n');

            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            kidstr = XMLToXMLString(cx, kid, ancestorNSes, indentLevel);
            if (!kidstr)
                goto list_out;

            js_AppendJSString(&sb, kidstr);
        }

        if (!sb.base) {
            if (!STRING_BUFFER_OK(&sb)) {
                JS_ReportOutOfMemory(cx);
                return NULL;
            }
            return cx->runtime->emptyString;
        }

        str = js_NewString(cx, sb.base, STRING_BUFFER_OFFSET(&sb), 0);
      list_out:
        if (!str)
            js_FinishStringBuffer(&sb);
        return str;

      default:;
    }

    /* ECMA-357 10.2.1 step 8 onward: handle ToXMLString on an XML element. */
    if (!ancestorNSes) {
        XMLARRAY_INIT(cx, &empty, 0);
        ancestorNSes = &empty;
    }
    XMLARRAY_INIT(cx, &decls, 0);
    ancdecls.length = 0;

    /* Clone in-scope namespaces not in ancestorNSes into decls. */
    for (i = 0, n = xml->xml_namespaces.count; i < n; i++) {
        ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSXMLNamespace);
        if (!XMLARRAY_HAS_MEMBER(ancestorNSes, ns, namespace_identity)) {
            /* NOTE: may want to exclude unused namespaces here. */
            ns2 = js_NewXMLNamespace(cx, ns->prefix, ns->uri);
            if (!ns2)
                goto out;
            if (!XMLARRAY_APPEND(cx, &decls, ns2)) {
                js_DestroyXMLNamespace(cx, ns2);
                goto out;
            }
        }
    }

    /*
     * Union ancestorNSes and decls into ancdecls.  Note that ancdecls does
     * not own its member references.  In the spec, ancdecls has no name, but
     * is always written out as (AncestorNamespaces U namespaceDeclarations).
     */
    if (!XMLARRAY_INIT(cx, &ancdecls, ancestorNSes->count + decls.count))
        goto out;
    for (i = 0, n = ancestorNSes->count; i < n; i++) {
        ns2 = XMLARRAY_MEMBER(ancestorNSes, i, JSXMLNamespace);
        JS_ASSERT(!XMLARRAY_HAS_MEMBER(&decls, ns2, namespace_identity));
        if (!XMLARRAY_APPEND(cx, &ancdecls, ns2))
            goto out;
    }
    for (i = 0, n = decls.count; i < n; i++) {
        ns2 = XMLARRAY_MEMBER(&decls, i, JSXMLNamespace);
        JS_ASSERT(!XMLARRAY_HAS_MEMBER(&ancdecls, ns2, namespace_identity));
        if (!XMLARRAY_APPEND(cx, &ancdecls, ns2))
            goto out;
    }

    /* Step 11, except we don't clone ns unless its prefix is undefined. */
    ns = GetNamespace(cx, xml->name, &ancdecls);
    if (!ns)
        goto out;

    /* Step 12 (NULL means *undefined* here), plus the deferred ns cloning. */
    if (!ns->prefix) {
        /*
         * Create a namespace prefix that isn't used by any member of decls.
         * Assign the new prefix to a copy of ns.
         */
        prefix = GeneratePrefix(cx, ns->uri, &ancdecls);
        if (!prefix)
            goto out;
        ns = js_NewXMLNamespace(cx, prefix, ns->uri);
        if (!ns)
            goto out;
        if (!XMLARRAY_APPEND(cx, &decls, ns)) {
            js_DestroyXMLNamespace(cx, ns);
            goto out;
        }
    }

    /* Format the element or point-tag into sb. */
    js_AppendChar(&sb, '<');

    if (!IS_EMPTY(ns->prefix)) {
        js_AppendJSString(&sb, ns->prefix);
        js_AppendChar(&sb, ':');
    }
    js_AppendJSString(&sb, xml->name->localName);

    /*
     * Step 16 makes a union to avoid writing two loops in step 17, to share
     * common attribute value appending spec-code.  We prefer two loops for
     * faster code and less data overhead.
     */

    /* Step 17(c): append XML namespace declarations. */
    for (i = 0, n = decls.count; i < n; i++) {
        ns2 = XMLARRAY_MEMBER(&decls, i, JSXMLNamespace);
        js_AppendCString(&sb, " xmlns");

        /* 17(c)(ii): NULL means *undefined* here. */
        if (!ns2->prefix) {
            prefix = GeneratePrefix(cx, ns2->uri, &ancdecls);
            if (!prefix)
                goto out;
            ns2->prefix = prefix;
        }

        /* 17(c)(iii). */
        if (!IS_EMPTY(ns2->prefix)) {
            js_AppendChar(&sb, ':');
            js_AppendJSString(&sb, ns2->prefix);
        }

        /* 17(d-g). */
        AppendAttributeValue(&sb, ns2->uri);
    }

    /* Step 17(b): append attributes. */
    for (i = 0, n = xml->xml_attrs.count; i < n; i++) {
        attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
        js_AppendChar(&sb, ' ');
        ns2 = GetNamespace(cx, attr->name, ancestorNSes);
        if (!ns2)
            goto out;

        /* 17(b)(ii): NULL means *undefined* here. */
        if (!ns2->prefix) {
            prefix = GeneratePrefix(cx, ns2->uri, &ancdecls);
            if (!prefix)
                goto out;

            /* Again, we avoid copying ns2 until we know it's prefix-less. */
            ns2 = js_NewXMLNamespace(cx, prefix, ns2->uri);
            if (!ns2)
                goto out;
            if (!XMLARRAY_APPEND(cx, &decls, ns2)) {
                js_DestroyXMLNamespace(cx, ns2);
                goto out;
            }
        }

        /* 17(b)(iii). */
        if (!IS_EMPTY(ns2->prefix)) {
            js_AppendJSString(&sb, ns2->prefix);
            js_AppendChar(&sb, ':');
        }

        /* 17(b)(iv). */
        js_AppendJSString(&sb, attr->name->localName);

        /* 17(d-g). */
        AppendAttributeValue(&sb, attr->xml_value);
    }

    /* Step 18: handle point tags. */
    n = xml->xml_kids.count;
    if (n == 0) {
        js_AppendCString(&sb, "/>");
    } else {
        /* Steps 19 through 25: handle element tag content, and the end-tag. */
        js_AppendChar(&sb, '>');
        indentKids = n > 1 ||
                     (n == 1 &&
                      XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML)->xml_class
                      != JSXML_CLASS_TEXT);

        if (pretty && indentKids) {
            if (!GetXMLSetting(cx, js_prettyIndent_str, &v))
                return NULL;
            if (!js_ValueToECMAUint32(cx, v, &i))
                return NULL;
            nextIndentLevel = indentLevel + i;
        } else {
            nextIndentLevel = 0;
        }

        for (i = 0; i < n; i++) {
            if (pretty && indentKids)
                js_AppendChar(&sb, '\n');

            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            kidstr = XMLToXMLString(cx, kid, &ancdecls, nextIndentLevel);
            if (!kidstr)
                goto out;

            js_AppendJSString(&sb, kidstr);
        }

        if (pretty && indentKids) {
            js_AppendChar(&sb, '\n');
            js_RepeatChar(&sb, ' ', indentLevel);
        }
        js_AppendCString(&sb, "</");

        /* Step 26. */
        if (!IS_EMPTY(ns->prefix)) {
            js_AppendJSString(&sb, ns->prefix);
            js_AppendChar(&sb, ':');
        }

        /* Step 27. */
        js_AppendJSString(&sb, xml->name->localName);
        js_AppendChar(&sb, '>');
    }

    if (!STRING_BUFFER_OK(&sb)) {
        JS_ReportOutOfMemory(cx);
        goto out;
    }

    str = js_NewString(cx, sb.base, STRING_BUFFER_OFFSET(&sb), 0);
out:
    if (!str)
        js_FinishStringBuffer(&sb);
    XMLARRAY_FINISH(cx, &decls, js_DestroyXMLNamespace);
    if (ancdecls.length != 0)
        XMLARRAY_FINISH(cx, &ancdecls, NULL);
    return str;
}

/* ECMA-357 10.2 */
static JSString *
ToXMLString(JSContext *cx, jsval v, JSXMLArray *ancestorNSes, uintN indentLevel)
{
    JSObject *obj;
    JSString *str;
    JSXML *xml;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_XML_CONVERSION,
                             js_type_str[JSVAL_IS_NULL(v)
                                         ? JSTYPE_NULL
                                         : JSTYPE_VOID]);
        return NULL;
    }

    if (JSVAL_IS_BOOLEAN(v) || JSVAL_IS_NUMBER(v))
        return js_ValueToString(cx, v);

    if (JSVAL_IS_STRING(v))
        return EscapeElementValue(cx, NULL, JSVAL_TO_STRING(v));

    obj = JSVAL_TO_OBJECT(v);
    if (!OBJECT_IS_XML(cx, obj)) {
        if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_STRING, &v))
            return NULL;
        str = js_ValueToString(cx, v);
        if (!str)
            return NULL;
        return EscapeElementValue(cx, NULL, str);
    }

    /* Handle non-element cases in this switch, returning from each case. */
    xml = (JSXML *) JS_GetPrivate(cx, obj);
    return XMLToXMLString(cx, xml, ancestorNSes, indentLevel);
}

static JSXMLQName *
ToAttributeName(JSContext *cx, jsval v)
{
    JSString *name, *uri, *prefix;
    JSObject *obj;
    JSClass *clasp;
    JSXMLQName *qn;

    if (JSVAL_IS_STRING(v)) {
        name = JSVAL_TO_STRING(v);
        uri = prefix = cx->runtime->emptyString;
    } else if (!JSVAL_IS_PRIMITIVE(v)) {
        obj = JSVAL_TO_OBJECT(v);
        clasp = OBJ_GET_CLASS(cx, obj);
        if (clasp == &attribute_name_class)
            return (JSXMLQName *) JS_GetPrivate(cx, obj);
        if (clasp == &qname_class) {
            qn = (JSXMLQName *) JS_GetPrivate(cx, obj);
            uri = qn->uri;
            prefix = qn->prefix;
            name = qn->localName;
        } else {
            if (clasp == &any_name_class) {
                name = ATOM_TO_STRING(cx->runtime->atomState.starAtom);
            } else {
                name = js_ValueToString(cx, v);
                if (!name)
                    return NULL;
            }
            uri = prefix = cx->runtime->emptyString;
        }
    } else {
        name = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK, v, NULL);
        if (name) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_XML_ATTRIBUTE_NAME,
                                 JS_GetStringBytes(name));
        }
        return NULL;
    }

    qn = js_NewXMLQName(cx, uri, prefix, name);
    if (!qn)
        return NULL;
    obj = js_NewObject(cx, &attribute_name_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, qn)) {
        js_DestroyXMLQName(cx, qn);
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    qn->object = obj;
    return qn;
}

static JSXMLQName *
ToXMLName(JSContext *cx, jsval v)
{
    JSString *name;
    JSObject *obj;
    JSClass *clasp;
    uint32 index;

    if (JSVAL_IS_STRING(v)) {
        name = JSVAL_TO_STRING(v);
    } else if (!JSVAL_IS_PRIMITIVE(v)) {
        obj = JSVAL_TO_OBJECT(v);
        clasp = OBJ_GET_CLASS(cx, obj);
        if (clasp == &attribute_name_class || clasp == &qname_class)
            goto out;
        if (clasp == &any_name_class) {
            name = ATOM_TO_STRING(cx->runtime->atomState.starAtom);
        } else {
            name = js_ValueToString(cx, v);
            if (!name)
                return NULL;
        }
    } else {
        name = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK, v, NULL);
        if (name)
            goto badname;
        return NULL;
    }

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
    if (js_IdIsIndex(STRING_TO_JSVAL(name), &index))
        goto badname;

    if (*JSSTRING_CHARS(name) == '@') {
        name = js_NewDependentString(cx, name, 1, JSSTRING_LENGTH(name) - 1, 0);
        if (!name)
            return NULL;
        return ToAttributeName(cx, STRING_TO_JSVAL(name));
    }

    v = STRING_TO_JSVAL(name);
    obj = js_ConstructObject(cx, &qname_class, NULL, NULL, 1, &v);
    if (!obj)
        return NULL;
out:
    return (JSXMLQName *) JS_GetPrivate(cx, obj);

badname:
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_BAD_XML_NAME,
                         js_ValueToPrintableString(cx, STRING_TO_JSVAL(name)));
    return NULL;
}

/* ECMA-357 9.1.1.13 XML [[AddInScopeNamespace]]. */
static JSBool
AddInScopeNamespace(JSContext *cx, JSXML *xml, JSXMLNamespace *ns)
{
    JSXMLNamespace *match, *ns2;
    uint32 i, n, m;

    if (xml->xml_class != JSXML_CLASS_ELEMENT)
        return JS_TRUE;

    /* NULL means *undefined* here -- see ECMA-357 9.1.1.13 step 2. */
    if (!ns->prefix) {
        match = NULL;
        for (i = 0, n = xml->xml_namespaces.count; i < n; i++) {
            ns2 = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSXMLNamespace);
            if (!js_CompareStrings(ns2->uri, ns->uri)) {
                match = ns2;
                break;
            }
        }
        if (!match && !XMLARRAY_INSERT(cx, &xml->xml_namespaces, n, ns))
            return JS_FALSE;
    } else {
        if (IS_EMPTY(ns->prefix) && IS_EMPTY(xml->name->uri))
            return JS_TRUE;
        match = NULL;
#ifdef __GNUC__         /* suppress bogus gcc warnings */
        m = NOT_FOUND;
#endif
        for (i = 0, n = xml->xml_namespaces.count; i < n; i++) {
            ns2 = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSXMLNamespace);
            if (!js_CompareStrings(ns2->prefix, ns->prefix)) {
                match = ns2;
                m = i;
                break;
            }
        }
        if (match && js_CompareStrings(match->uri, ns->uri)) {
            ns2 = XMLARRAY_DELETE(cx, &xml->xml_namespaces, m, JS_TRUE,
                                  JSXMLNamespace);
            JS_ASSERT(ns2 == match);
            match->prefix = NULL;
            if (!AddInScopeNamespace(cx, xml, match)) {
                js_DestroyXMLNamespace(cx, match);
                return JS_FALSE;
            }
        }
        if (!XMLARRAY_APPEND(cx, &xml->xml_namespaces, ns))
            return JS_FALSE;
    }

    /* OPTION: enforce that descendants have superset namespaces. */
    return JS_TRUE;
}

/* ECMA-357 9.2.1.6 XMLList [[Append]]. */
static JSBool
Append(JSContext *cx, JSXML *list, JSXML *kid)
{
    uint32 i, j, n;
    JSXML *grandkid;
    JSBool ok, locked;

    JS_ASSERT(list->xml_class == JSXML_CLASS_LIST);
    i = list->xml_kids.count;
    n = 1;
    if (kid->xml_class == JSXML_CLASS_LIST) {
        if (kid->object) {
            JS_LOCK_OBJ(cx, kid->object);
            locked = JS_TRUE;
        } else {
            locked = JS_FALSE;
        }
        list->xml_target = kid->xml_target;
        list->xml_targetprop = kid->xml_targetprop;
        n = JSXML_LENGTH(kid);
        ok = JS_TRUE;
        for (j = 0; j < n; j++) {
            grandkid = XMLARRAY_MEMBER(&kid->xml_kids, j, JSXML);
            ok = js_GetXMLObject(cx, grandkid) != NULL;
            if (!ok)
                break;
            ok = XMLARRAY_INSERT(cx, &list->xml_kids, i + j, grandkid);
            if (!ok)
                break;
        }
        if (locked)
            JS_UNLOCK_OBJ(cx, kid->object);
        return ok;
    }

    list->xml_target = kid->parent;
    if (kid->xml_class == JSXML_CLASS_PROCESSING_INSTRUCTION)
        list->xml_targetprop = NULL;
    else
        list->xml_targetprop = kid->name;
    if (!js_GetXMLObject(cx, kid))
        return JS_FALSE;
    if (!XMLARRAY_INSERT(cx, &list->xml_kids, i, kid))
        return JS_FALSE;
    return JS_TRUE;
}

/* ECMA-357 9.1.1.7 XML [[DeepCopy]] and 9.2.1.7 XMLList [[DeepCopy]]. */
static JSXML *
DeepCopyInLRS(JSContext *cx, JSXML *xml);

static JSXML *
DeepCopy(JSContext *cx, JSXML *xml)
{
    JSXML *copy;

    /* Our caller may not be protecting newborns with a local root scope. */
    if (!JS_EnterLocalRootScope(cx))
        return NULL;
    copy = DeepCopyInLRS(cx, xml);
    JS_LeaveLocalRootScope(cx);
    return copy;
}

/*
 * (i) We must be in a local root scope (InLRS).
 * (ii) parent must have a rooted object.
 */
static JSBool
DeepCopySetInLRS(JSContext *cx, JSXMLArray *from, JSXMLArray *to, JSXML *parent)
{
    uint32 i, n;
    JSXML *kid, *kid2;

    JS_ASSERT(cx->localRootStack);
    JS_ASSERT(parent->object);

    for (i = 0, n = from->count; i < n; i++) {
        kid = XMLARRAY_MEMBER(from, i, JSXML);
        kid2 = DeepCopyInLRS(cx, kid);
        if (!kid2)
            return JS_FALSE;
        if (!XMLARRAY_INSERT(cx, to, i, kid2))
            return JS_FALSE;
        kid2->parent = parent;
        JS_ForgetLocalRoot(cx, kid2->object);
    }
    return JS_TRUE;
}

static JSXML *
DeepCopyInLRS(JSContext *cx, JSXML *xml)
{
    JSObject *copyobj;
    JSXML *copy;
    JSBool ok;
    uint32 i, n;
    JSXMLNamespace *ns, *ns2;

    /* Our caller must be protecting newborn objects. */
    JS_ASSERT(cx->localRootStack);

    copyobj = js_NewXMLObject(cx, xml->xml_class);
    if (!copyobj)
        return NULL;

    if (xml->object)
        OBJ_SET_PROTO(cx, copyobj, OBJ_GET_PROTO(cx, xml->object));
    copy = (JSXML *) JS_GetPrivate(cx, copyobj);
    copy->name = xml->name;

    if (JSXML_HAS_VALUE(xml)) {
        copy->xml_value = xml->xml_value;
        ok = JS_TRUE;
    } else {
        ok = DeepCopySetInLRS(cx, &xml->xml_kids, &copy->xml_kids, copy);
        if (!ok)
            goto out;

        if (xml->xml_class == JSXML_CLASS_LIST) {
            copy->xml_target = xml->xml_target;
            copy->xml_targetprop = xml->xml_targetprop;
        } else {
            for (i = 0, n = xml->xml_namespaces.count; i < n; i++) {
                ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSXMLNamespace);
                ns2 = js_NewXMLNamespace(cx, ns->prefix, ns->uri);
                if (!ns2) {
                    ok = JS_FALSE;
                    goto out;
                }
                ok = XMLARRAY_INSERT(cx, &copy->xml_namespaces, i, ns2);
                if (!ok) {
                    js_DestroyXMLNamespace(cx, ns2);
                    goto out;
                }
            }

            ok = DeepCopySetInLRS(cx, &xml->xml_attrs, &copy->xml_attrs, copy);
            if (!ok)
                goto out;
        }
    }

out:
    if (!ok) {
        copy->object = NULL;
        js_DestroyXML(cx, copy);
        return NULL;
    }
    return copy;
}

static void
ReportBadXMLName(JSContext *cx, jsval id)
{
    JSString *name;

    name = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK, id, NULL);
    if (name) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_XML_NAME,
                             JS_GetStringBytes(name));
    }
}

/* ECMA-357 9.1.1.4 XML [[DeleteByIndex]]. */
static JSBool
DeleteByIndex(JSContext *cx, JSXML *xml, jsval id, jsval *vp)
{
    uint32 index;
    JSXML *kid;

    if (!js_IdIsIndex(id, &index)) {
        ReportBadXMLName(cx, id);
        return JS_FALSE;
    }

    if (JSXML_HAS_KIDS(xml) && index < xml->xml_kids.count) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
        kid->parent = NULL;
        XMLArrayDelete(cx, &xml->xml_kids, index, JS_TRUE);
    }

    *vp = JSVAL_TRUE;
    return JS_TRUE;
}

static JSBool
MatchAttrName(JSXMLQName *nameqn, JSXMLQName *attrqn)
{
    return (IS_STAR(nameqn->localName) ||
            !js_CompareStrings(attrqn->localName, nameqn->localName)) &&
           (!nameqn->uri ||
            !js_CompareStrings(attrqn->uri, nameqn->uri));
}

static JSBool
MatchElemName(JSXMLQName *nameqn, JSXML *elem)
{
    return (IS_STAR(nameqn->localName) ||
            (elem->xml_class == JSXML_CLASS_ELEMENT &&
             !js_CompareStrings(elem->name->localName, nameqn->localName))) &&
           (!nameqn->uri ||
            (elem->xml_class == JSXML_CLASS_ELEMENT &&
             !js_CompareStrings(elem->name->uri, nameqn->uri)));
}

/* ECMA-357 9.1.1.8 XML [[Descendants]] and 9.2.1.8 XMLList [[Descendants]]. */
static JSBool
DescendantsHelper(JSContext *cx, JSXML *xml, JSXMLQName *nameqn, JSXML *list)
{
    uint32 i, n;
    JSXML *attr, *kid;

    if (OBJ_GET_CLASS(cx, nameqn->object) == &attribute_name_class) {
        for (i = 0, n = xml->xml_attrs.count; i < n; i++) {
            attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
            if (MatchAttrName(nameqn, attr->name)) {
                if (!Append(cx, list, attr))
                    return JS_FALSE;
            }
        }
    }

    for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
        kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
        if (MatchElemName(nameqn, kid)) {
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
    JSXMLQName *nameqn;
    JSObject *listobj;
    JSXML *list;

    nameqn = ToXMLName(cx, id);
    if (!nameqn)
        return NULL;

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return NULL;
    list = (JSXML *) JS_GetPrivate(cx, listobj);

    /*
     * Protect nameqn's object and strings from GC by linking list to it
     * temporarily.  The cx->newborn[GCX_OBJECT] GC root protects listobj,
     * which protects list.  No further object allocations occur beneath
     * the DescendantsHelper call here.
     */
    list->name = nameqn;
    if (!DescendantsHelper(cx, xml, nameqn, list)) {
        js_DestroyXML(cx, list);
        return NULL;
    }
    list->name = NULL;
    return list;
}

/* Recursive helper for Equals. */
static JSBool
XMLEquals(JSContext *cx, JSXML *xml, JSXML *vxml)
{
    JSXMLQName *qn, *vqn;
    uint32 i, j, n;
    JSXML **xvec, **vvec, *attr, *vattr;

    if (xml->xml_class != vxml->xml_class)
        return JS_FALSE;

    qn = xml->name;
    vqn = vxml->name;
    if (qn) {
        if (!vqn)
            return JS_FALSE;
        if (js_CompareStrings(qn->localName, vqn->localName))
            return JS_FALSE;
        if (js_CompareStrings(qn->uri, vqn->uri))
            return JS_FALSE;
    } else {
        if (vqn)
            return JS_FALSE;
    }

    if (JSXML_HAS_VALUE(xml)) {
        if (js_CompareStrings(xml->xml_value, vxml->xml_value))
            return JS_FALSE;
    } else {
        n = xml->xml_kids.count;
        if (n != vxml->xml_kids.count)
            return JS_FALSE;
        xvec = (JSXML **) xml->xml_kids.vector;
        vvec = (JSXML **) vxml->xml_kids.vector;
        for (i = 0; i < n; i++) {
            if (!XMLEquals(cx, xvec[i], vvec[i]))
                return JS_FALSE;
        }

        if (xml->xml_class == JSXML_CLASS_ELEMENT) {
            n = xml->xml_attrs.count;
            if (n != vxml->xml_attrs.count)
                return JS_FALSE;
            for (i = 0; i < n; i++) {
                attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
                j = XMLARRAY_HAS_MEMBER(&vxml->xml_attrs, attr, attr_identity);
                if (j == NOT_FOUND)
                    return JS_FALSE;
                vattr = XMLARRAY_MEMBER(&vxml->xml_attrs, j, JSXML);
                if (js_CompareStrings(attr->xml_value, vattr->xml_value))
                    return JS_FALSE;
            }
        }
    }

    return JS_TRUE;
}

/* ECMA-357 9.1.1.9 XML [[Equals]] and 9.2.1.9 XMLList [[Equals]]. */
static JSBool
Equals(JSContext *cx, JSXML *xml, jsval v)
{
    JSObject *vobj;
    JSXML *vxml;

    if (JSVAL_IS_PRIMITIVE(v))
        return JS_FALSE;

    vobj = JSVAL_TO_OBJECT(v);
    if (OBJ_GET_CLASS(cx, vobj) != &js_XMLClass)
        return JS_FALSE;
    vxml = (JSXML *) JS_GetPrivate(cx, vobj);
    return XMLEquals(cx, xml, vxml);
}

static JSBool
Replace(JSContext *cx, JSXML *xml, jsval id, jsval v);

/* ECMA-357 9.1.1.11 XML [[Insert]]. */
static JSBool
Insert(JSContext *cx, JSXML *xml, jsval id, jsval v)
{
    uint32 i, j, n;
    JSXML *vxml, *kid;
    JSObject *vobj;

    if (!JSXML_HAS_KIDS(xml))
        return JS_TRUE;

    if (!js_IdIsIndex(id, &i)) {
        ReportBadXMLName(cx, id);
        return JS_FALSE;
    }

    n = 1;
    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (OBJ_GET_CLASS(cx, vobj) == &js_XMLClass) {
            vxml = (JSXML *) JS_GetPrivate(cx, vobj);
            if (vxml->xml_class == JSXML_CLASS_LIST)
                n = vxml->xml_kids.count;
        }
    }

    if (n == 0)
        return JS_TRUE;

    j = xml->xml_kids.count;
    JS_ASSERT(j >= i);
    while (j != i) {
        --j;
        if (!XMLARRAY_INSERT(cx, &xml->xml_kids, j + n,
                             xml->xml_kids.vector[j])) {
            return JS_FALSE;
        }
    }

    if (vxml && vxml->xml_class == JSXML_CLASS_LIST) {
        for (j = 0; j < n; j++) {
            kid = XMLARRAY_MEMBER(&vxml->xml_kids, j, JSXML);
            if (!js_GetXMLObject(cx, kid))
                return JS_FALSE;
            kid->parent = xml;
            XMLARRAY_SET_MEMBER(&xml->xml_kids, i + j, kid);

            /* OPTION: enforce that descendants have superset namespaces. */
        }
    } else {
        if (!Replace(cx, xml, id, v))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
IndexToIdVal(JSContext *cx, uint32 index, jsval *idvp)
{
    JSString *str;

    if (index <= JSVAL_INT_MAX) {
        *idvp = INT_TO_JSVAL(index);
    } else {
        str = js_NumberToString(cx, (jsdouble) index);
        if (!str)
            return JS_FALSE;
        *idvp = STRING_TO_JSVAL(str);
    }
    return JS_TRUE;
}

/* ECMA-357 9.1.1.12 XML [[Replace]]. */
static JSBool
Replace(JSContext *cx, JSXML *xml, jsval id, jsval v)
{
    uint32 i, n;
    JSXML *vxml, *kid;
    JSObject *vobj;
    jsval junk;
    JSString *str;

    if (!JSXML_HAS_KIDS(xml))
        return JS_TRUE;

    if (!js_IdIsIndex(id, &i)) {
        ReportBadXMLName(cx, id);
        return JS_FALSE;
    }

    /*
     * 9.1.1.12
     * [[Replace]] handles _i >= x.[[Length]]_ by incrementing _x.[[Length]_.
     * It should therefore constrain callers to pass in _i <= x.[[Length]]_.
     */
    n = xml->xml_kids.count;
    JS_ASSERT(i <= n);
    if (i >= n) {
        if (!IndexToIdVal(cx, n, &id))
            return JS_FALSE;
        if (!XMLARRAY_INSERT(cx, &xml->xml_kids, n, NULL))
            return JS_FALSE;
        i = n;
    }

    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (OBJ_GET_CLASS(cx, vobj) == &js_XMLClass)
            vxml = (JSXML *) JS_GetPrivate(cx, vobj);
    }

    switch (vxml ? vxml->xml_class : JSXML_CLASS_LIMIT) {
      case JSXML_CLASS_ELEMENT:
      case JSXML_CLASS_COMMENT:
      case JSXML_CLASS_PROCESSING_INSTRUCTION:
      case JSXML_CLASS_TEXT:
        vxml->parent = xml;
        if (i < n) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            kid->parent = NULL;
        }
        XMLARRAY_SET_MEMBER(&xml->xml_kids, i, vxml);

        /* OPTION: enforce that descendants have superset namespaces. */
        break;

      case JSXML_CLASS_LIST:
        if (!DeleteByIndex(cx, xml, id, &junk))
            return JS_FALSE;
        if (!Insert(cx, xml, id, v))
            return JS_FALSE;
        break;

      default:
        str = js_ValueToString(cx, v);
        if (!str)
            return JS_FALSE;

        vxml = js_NewXML(cx, JSXML_CLASS_TEXT);
        if (!vxml)
            return JS_FALSE;
        vxml->parent = xml;
        vxml->xml_value = str;

        if (i < n) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            kid->parent = NULL;
        }
        XMLARRAY_SET_MEMBER(&xml->xml_kids, i, vxml);
        break;
    }

    return JS_TRUE;
}

/* ECMA-357 9.1.1.10 XML [[ResolveValue]], 9.2.1.10 XMLList [[ResolveValue]]. */
static JSBool
ResolveValue(JSContext *cx, JSXML *list, JSXML **result)
{
    JSXML *target, *base;
    JSXMLQName *targetprop;
    jsval id, tv;

    /* Our caller must be protecting newborn objects. */
    JS_ASSERT(cx->localRootStack);

    if (list->xml_class != JSXML_CLASS_LIST || list->xml_kids.count != 0) {
        if (!js_GetXMLObject(cx, list))
            return JS_FALSE;
        *result = list;
        return JS_TRUE;
    }

    target = list->xml_target;
    targetprop = list->xml_targetprop;
    if (!target ||
        !targetprop ||
        OBJ_GET_CLASS(cx, targetprop->object) == &attribute_name_class ||
        IS_STAR(targetprop->localName)) {
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

    id = OBJECT_TO_JSVAL(targetprop->object);
    if (!js_XMLClass.getProperty(cx, base->object, id, &tv))
        return JS_FALSE;
    target = (JSXML *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(tv));

    if (JSXML_LENGTH(target) == 0) {
        if (base->xml_class == JSXML_CLASS_LIST && JSXML_LENGTH(base) > 1) {
            *result = NULL;
            return JS_TRUE;
        }
        tv = STRING_TO_JSVAL(cx->runtime->emptyString);
        if (!js_XMLClass.setProperty(cx, base->object, id, &tv))
            return JS_FALSE;
        if (!js_XMLClass.getProperty(cx, base->object, id, &tv))
            return JS_FALSE;
        target = (JSXML *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(tv));
    }

    *result = target;
    return JS_TRUE;
}

/* ECMA-357 9.1.1.3 XML [[Delete]], 9.2.1.3 XML [[Delete]]. */
static JSBool
DeleteProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSXML *xml, *kid, *parent, *attr;
    JSBool isIndex;
    uint32 index, deleteCount;
    JSXMLQName *nameqn;
    JSObject *nameobj;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    isIndex = js_IdIsIndex(id, &index);

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* ECMA-357 9.2.1.3. */
        if (isIndex && index < xml->xml_kids.count) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
            parent = kid->parent;
            if (parent) {
                JS_ASSERT(parent->object);
                JS_ASSERT(JSXML_HAS_KIDS(parent));

                if (kid->xml_class == JSXML_CLASS_ATTRIBUTE) {
                    nameqn = kid->name;
                    nameobj = js_GetXMLQNameObject(cx, nameqn);
                    if (!nameobj)
                        return JS_FALSE;

                    id = OBJECT_TO_JSVAL(nameobj);
                    if (!DeleteProperty(cx, parent->object, id, vp))
                        return JS_FALSE;
                } else {
                    index = XMLARRAY_HAS_MEMBER(&parent->xml_kids, kid, NULL);
                    JS_ASSERT(index != NOT_FOUND);
                    if (!IndexToIdVal(cx, index, &id))
                        return JS_FALSE;
                    if (!DeleteByIndex(cx, parent, id, vp))
                        return JS_FALSE;
                }
            }

            XMLArrayDelete(cx, &xml->xml_kids, index, JS_TRUE);
        }
    } else {
        /* ECMA-357 9.1.1.3. */
        if (isIndex) {
            /* See NOTE in spec: this variation is reserved for future use. */
            ReportBadXMLName(cx, id);
            return JS_FALSE;
        }

        nameqn = ToXMLName(cx, id);
        if (!nameqn)
            return JS_FALSE;
        nameobj = nameqn->object;

        if (OBJ_GET_CLASS(cx, nameobj) == &attribute_name_class) {
            if (xml->xml_class == JSXML_CLASS_ELEMENT &&
                (index = xml->xml_attrs.count) != 0) {
                deleteCount = 0;
                do {
                    --index;
                    attr = XMLARRAY_MEMBER(&xml->xml_attrs, index, JSXML);
                    if (MatchAttrName(nameqn, attr->name)) {
                        attr->parent = NULL;
                        XMLArrayDelete(cx, &xml->xml_attrs, index, JS_FALSE);
                        ++deleteCount;
                    }
                } while (index != 0);
                xml->xml_attrs.count -= deleteCount;
            }
        } else {
            if (JSXML_HAS_KIDS(xml) && (index = xml->xml_kids.count) != 0) {
                deleteCount = 0;
                do {
                    --index;
                    kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
                    if (MatchElemName(nameqn, kid)) {
                        kid->parent = NULL;
                        XMLArrayDelete(cx, &xml->xml_kids, index, JS_FALSE);
                        ++deleteCount;
                    }
                } while (index != 0);
                xml->xml_kids.count -= deleteCount;
            }
        }
    }

    *vp = JSVAL_TRUE;
    return JS_TRUE;
}

/* ECMA-357 9.1.1.1 XML [[Get]] and 9.2.1.1 XMLList [[Get]]. */
static JSBool
GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSXML *xml, *list, *attr, *kid;
    uint32 index, i, n;
    JSXMLQName *nameqn;
    JSObject *listobj, *kidobj, *nameobj;
    JSBool ok;
    jsval kidval;

    xml = (JSXML *) JS_GetInstancePrivate(cx, obj, &js_XMLClass, NULL);
    if (!xml)
        return JS_TRUE;

retry:
    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* ECMA-357 9.2.1.1 starts here. */
        if (js_IdIsIndex(id, &index))
            return js_ObjectClass.getProperty(cx, obj, id, vp);

        nameqn = ToXMLName(cx, id);
        if (!nameqn)
            return JS_FALSE;

        /*
         * Recursion through GetProperty may allocate more list objects, so
         * we make use of local root scopes here.  Each new allocation will
         * push the newborn onto the local root stack.
         */
        ok = JS_EnterLocalRootScope(cx);
        if (!ok)
            return JS_FALSE;

        /*
         * NB: nameqn is already protected from GC by cx->newborn[GCX_OBJECT]
         * until listobj is created.  After that, a local root keeps listobj
         * alive, and listobj's private keeps nameqn alive via targetprop.
         */
        listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
        if (!listobj) {
            ok = JS_FALSE;
        } else {
            list = (JSXML *) JS_GetPrivate(cx, listobj);
            list->xml_target = xml;
            list->xml_targetprop = nameqn;

            for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
                kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
                if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                    kidobj = js_GetXMLObject(cx, kid);
                    if (!kidobj) {
                        ok = JS_FALSE;
                        break;
                    }
                    ok = GetProperty(cx, kidobj, id, &kidval);
                    if (!ok)
                        break;
                    kidobj = JSVAL_TO_OBJECT(kidval);
                    kid = (JSXML *) JS_GetPrivate(cx, kidobj);
                    if (JSXML_LENGTH(kid) > 0) {
                        ok = Append(cx, list, kid);
                        if (!ok)
                            break;
                    }
                }
            }
        }

        JS_LeaveLocalRootScope(cx);
        if (!ok)
            return JS_FALSE;
    } else {
        /* ECMA-357 9.1.1.1 starts here. */
        if (js_IdIsIndex(id, &index)) {
            xml = ToXMLList(cx, OBJECT_TO_JSVAL(obj));
            if (!xml)
                return JS_FALSE;
            obj = xml->object;
            goto retry;
        }

        nameqn = ToXMLName(cx, id);
        if (!nameqn)
            return JS_FALSE;
        nameobj = nameqn->object;

        /*
         * See comment in JSXML_CLASS_LIST block above on newborn rooting.
         * Unlike the LIST case, we make no recursive calls below, nor do we
         * allocate any other object.  So we don't need to root listobj or
         * enter a local root scope.
         */
        listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
        if (!listobj)
            return JS_FALSE;
        list = (JSXML *) JS_GetPrivate(cx, listobj);
        list->xml_target = xml;
        list->xml_targetprop = nameqn;

        if (OBJ_GET_CLASS(cx, nameobj) == &attribute_name_class) {
            for (i = 0, n = xml->xml_attrs.count; i < n; i++) {
                attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
                if (MatchAttrName(nameqn, attr->name)) {
                    if (!Append(cx, list, attr))
                        return JS_FALSE;
                }
            }
        } else {
            for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
                kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
                if (MatchElemName(nameqn, kid)) {
                    if (!Append(cx, list, kid))
                        return JS_FALSE;
                }
            }
        }
    }

    *vp = OBJECT_TO_JSVAL(listobj);
    return JS_TRUE;
}

static JSString *
KidToString(JSContext *cx, JSXML *xml, uint32 index)
{
    JSXML *kid;
    JSObject *kidobj;

    kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
    kidobj = js_GetXMLObject(cx, kid);
    if (!kidobj)
        return NULL;
    return js_ValueToString(cx, OBJECT_TO_JSVAL(kidobj));
}

/* ECMA-357 9.1.1.2 XML [[Put]] and 9.2.1.2 XMLList [[Put]]. */
static JSBool
PutProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBool ok, primitiveAssign;
    JSXML *xml, *vxml, *rxml, *kid, *attr, *parent, *copy, *kid2, *match;
    JSObject *vobj, *nameobj, *attrobj, *parentobj, *kidobj, *copyobj;
    JSXMLQName *targetprop, *nameqn, *attrqn;
    uint32 index, i, j, k, n, q;
    jsval attrval, nsval, junk;
    JSString *left, *right, *space;
    JSXMLNamespace *ns;

    xml = (JSXML *) JS_GetInstancePrivate(cx, obj, &js_XMLClass, NULL);
    if (!xml)
        return JS_TRUE;
    if ((xml->flags & XML_COPY_ON_WRITE) && xml->object != obj) {
        /* XXXbe waste an XML object here. */
        xml = DeepCopy(cx, xml);
        if (!xml || !JS_SetPrivate(cx, obj, xml))
            return JS_FALSE;
        xml->object = obj;
    }

    /* Precompute vxml for 9.2.1.2 2(c)(vii)(2-3) and 2(d) and 9.1.1.2 1. */
    vxml = NULL;
    if (!JSVAL_IS_PRIMITIVE(*vp)) {
        vobj = JSVAL_TO_OBJECT(*vp);
        if (OBJ_GET_CLASS(cx, vobj) == &js_XMLClass)
            vxml = (JSXML *) JS_GetPrivate(cx, vobj);
    }

    /* Control flow after here must exit via label out. */
    ok = JS_EnterLocalRootScope(cx);
    if (!ok)
        return JS_FALSE;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* ECMA-357 9.2.1.2. */
        if (js_IdIsIndex(id, &index)) {
            /* Step 1 sets i to the property index. */
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
            if (index >= xml->xml_kids.count) {
                /* 2(c)(i). */
                if (rxml && rxml->xml_class == JSXML_CLASS_LIST) {
                    if (rxml->xml_kids.count != 1)
                        goto out;
                    rxml = XMLARRAY_MEMBER(&rxml->xml_kids, 0, JSXML);
                    ok = js_GetXMLObject(cx, rxml) != NULL;
                    if (!ok)
                        goto out;
                }

                /* 2(c)(ii) is distributed below as several js_NewXML calls. */
                targetprop = xml->xml_targetprop;
                if (!targetprop || IS_STAR(targetprop->localName)) {
                    /* 2(c)(iv)(1-2), out of order w.r.t. 2(c)(iii). */
                    kid = js_NewXML(cx, JSXML_CLASS_TEXT);
                    if (!kid)
                        goto bad;
                } else {
                    nameobj = targetprop->object;
                    if (nameobj &&
                        OBJ_GET_CLASS(cx, nameobj) == &attribute_name_class) {
                        /*
                         * 2(c)(iii)(1-3).
                         * Note that rxml can't be null here, because target
                         * and targetprop are non-null.
                         */
                        ok = GetProperty(cx, rxml->object, id, &attrval);
                        if (!ok)
                            goto out;
                        attrobj = JSVAL_TO_OBJECT(attrval);
                        attr = (JSXML *) JS_GetPrivate(cx, attrobj);
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
                i = xml->xml_kids.count;
                if (kid->xml_class != JSXML_CLASS_ATTRIBUTE) {
                    /*
                     * 2(c)(vii)(1) tests whether _y.[[Parent]]_ is not null.
                     * y.[[Parent]] is here called kid->parent, which we know
                     * from 2(c)(ii) is _r_, here called rxml.  So let's just
                     * test that!  Erratum, the spec should be simpler here.
                     */
                    if (rxml) {
                        j = n = rxml->xml_kids.count - 1;
                        if (i != 0) {
                            for (j = 0; j < n; j++) {
                                if (rxml->xml_kids.vector[j] ==
                                    xml->xml_kids.vector[i-1]) {
                                    break;
                                }
                            }
                        }

                        ok = XMLARRAY_INSERT(cx, &rxml->xml_kids, j+1, kid);
                        if (!ok) {
                            js_DestroyXML(cx, kid);
                            goto out;
                        }
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
                if (!ok) {
                    js_DestroyXML(cx, kid);
                    goto out;
                }
            }

            /* 2(d). */
            if (!vxml ||
                vxml->xml_class == JSXML_CLASS_TEXT ||
                vxml->xml_class == JSXML_CLASS_ATTRIBUTE) {
                ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
                if (!ok)
                    goto out;
            }

            /* 2(e). */
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            parent = kid->parent;
            if (kid->xml_class == JSXML_CLASS_ATTRIBUTE) {
                nameobj = js_GetXMLQNameObject(cx, kid->name);
                if (!nameobj)
                    goto bad;
                id = OBJECT_TO_JSVAL(nameobj);

                /* 2(e)(i). */
                parentobj = parent->object;
                ok = PutProperty(cx, parentobj, id, vp);
                if (!ok)
                    goto out;

                /* 2(e)(ii). */
                ok = GetProperty(cx, parentobj, id, vp);
                if (!ok)
                    goto out;
                attr = (JSXML *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(*vp));

                /* 2(e)(iii). */
                xml->xml_kids.vector[i] = attr->xml_kids.vector[0];
            }

            /* 2(f). */
            else if (vxml && vxml->xml_class == JSXML_CLASS_LIST) {
                /* 2(f)(i) Create a shallow copy _c_ of _V_. */
                copyobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
                if (!copyobj)
                    goto bad;
                copy = (JSXML *) JS_GetPrivate(cx, copyobj);
                for (k = 0, n = vxml->xml_kids.count; k < n; k++) {
                    kid2 = XMLARRAY_MEMBER(&vxml->xml_kids, k, JSXML);
                    ok = js_GetXMLObject(cx, kid2) != NULL;
                    if (!ok)
                        goto out;
                    ok = XMLARRAY_INSERT(cx, &copy->xml_kids, k, kid2);
                    if (!ok)
                        goto out;
                }

                if (parent) {
                    q = XMLARRAY_HAS_MEMBER(&parent->xml_kids, kid, NULL);
                    JS_ASSERT(q != NOT_FOUND);

                    ok = IndexToIdVal(cx, q, &id);
                    if (!ok)
                        goto out;
                    ok = Replace(cx, parent, id, OBJECT_TO_JSVAL(copyobj));
                    if (!ok)
                        goto out;

                    /* Erratum: isn't this loop useless? */
                    for (j = 0, n = copy->xml_kids.count; j < n; j++) {
                        kid2 = XMLARRAY_MEMBER(&parent->xml_kids, q + j, JSXML);
                        ok = js_GetXMLObject(cx, kid2) != NULL;
                        if (!ok)
                            goto out;
                        XMLARRAY_SET_MEMBER(&copy->xml_kids, j, kid2);
                    }
                }

                /* 2(f)(iv, vi). */
                j = xml->xml_kids.count;
                k = copy->xml_kids.count;
                while (j != i) {
                    --j;
                    kid2 = XMLARRAY_MEMBER(&xml->xml_kids, j, JSXML);
                    ok = XMLARRAY_INSERT(cx, &xml->xml_kids, j + k, kid2);
                    if (!ok)
                        goto out;
                }

                /* 2(f)(v). */
                for (j = 0; j < k; j++) {
                    kid2 = XMLARRAY_MEMBER(&copy->xml_kids, j, JSXML);
                    XMLARRAY_SET_MEMBER(&xml->xml_kids, i + j, kid2);
                }
            }

            /* 2(g). */
            else if (vxml || JSXML_HAS_VALUE(kid)) {
                if (parent) {
                    q = XMLARRAY_HAS_MEMBER(&parent->xml_kids, kid, NULL);
                    JS_ASSERT(q != NOT_FOUND);

                    ok = IndexToIdVal(cx, q, &id);
                    if (!ok)
                        goto out;
                    ok = Replace(cx, parent, id, *vp);
                    if (!ok)
                        goto out;

                    vxml = XMLARRAY_MEMBER(&parent->xml_kids, q, JSXML);
                    *vp = OBJECT_TO_JSVAL(vxml->object);
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
                    vxml = ToXML(cx, *vp);
                    if (!vxml)
                        goto bad;
                }
                XMLARRAY_SET_MEMBER(&xml->xml_kids, i, vxml);
            }

            /* 2(h). */
            else {
                kidobj = js_GetXMLObject(cx, kid);
                if (!kidobj)
                    goto bad;
                id = ATOM_KEY(cx->runtime->atomState.starAtom);
                ok = PutProperty(cx, kidobj, id, vp);
                if (!ok)
                    goto out;
            }
        } else {
            /* 3. */
            n = JSXML_LENGTH(xml);
            if (n <= 1) {
                if (n == 0) {
                    ok = ResolveValue(cx, xml, &rxml);
                    if (!ok)
                        goto out;
                    if (!rxml || JSXML_LENGTH(rxml) != 1)
                        goto out;
                    ok = Append(cx, xml, rxml);
                    if (!ok)
                        goto out;
                }
                JS_ASSERT(JSXML_LENGTH(xml) == 1);
                kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
                kidobj = js_GetXMLObject(cx, kid);
                if (!kidobj)
                    goto bad;
                ok = PutProperty(cx, kidobj, id, vp);
                if (!ok)
                    goto out;
            }
        }
    } else {
        /*
         * ECMA-357 9.1.1.2.
         * Erratum: move steps 3 and 4 to before 1 and 2, to avoid wasted
         * effort in ToString or [[DeepCopy]].
         */
        if (js_IdIsIndex(id, &index)) {
            /* See NOTE in spec: this variation is reserved for future use. */
            ReportBadXMLName(cx, id);
            goto bad;
        }

        if (JSXML_HAS_VALUE(xml))
            goto out;

        if (!vxml ||
            vxml->xml_class == JSXML_CLASS_TEXT ||
            vxml->xml_class == JSXML_CLASS_ATTRIBUTE) {
            ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
            if (!ok)
                goto out;
        } else {
            rxml = DeepCopyInLRS(cx, vxml);
            if (!rxml)
                goto bad;
            vxml = rxml;
            *vp = OBJECT_TO_JSVAL(vxml->object);
        }

        nameqn = ToXMLName(cx, id);
        if (!nameqn)
            goto bad;
        nameobj = nameqn->object;

        /*
         * 6.
         * Erratum: why is this done here, so early? use is way later....
         */
        ok = js_GetDefaultXMLNamespace(cx, &nsval);
        if (!ok)
            goto out;

        if (OBJ_GET_CLASS(cx, nameobj) == &attribute_name_class) {
            /* 7(a). */
            if (!js_IsXMLName(cx, OBJECT_TO_JSVAL(nameobj)))
                goto out;

            /* 7(b-c). */
            if (vxml && vxml->xml_class == JSXML_CLASS_LIST) {
                n = vxml->xml_kids.count;
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

                    *vp = STRING_TO_JSVAL(left);
                }
            } else {
                ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
                if (!ok)
                    goto out;
            }

            /* 7(d-e). */
            match = NULL;
            for (i = 0, n = xml->xml_attrs.count; i < n; i++) {
                attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
                attrqn = attr->name;
                if (!js_CompareStrings(attrqn->localName, nameqn->localName) &&
                    (!nameqn->uri ||
                     !js_CompareStrings(attrqn->uri, nameqn->uri))) {
                    if (!match) {
                        match = attr;
                    } else {
                        nameobj = js_GetXMLQNameObject(cx, attrqn);
                        if (!nameobj)
                            goto bad;

                        id = OBJECT_TO_JSVAL(nameobj);
                        ok = DeleteProperty(cx, obj, id, &junk);
                        if (!ok)
                            goto out;

                        /* XXXbe O(n^2) badness here! */
                        --i;
                    }
                }
            }

            /* 7(f). */
            attr = match;
            if (!attr) {
                /* 7(f)(i-ii). */
                if (!nameqn->uri) {
                    left = right = cx->runtime->emptyString;
                } else {
                    left = nameqn->uri;
                    right = nameqn->prefix;
                }
                nameqn = js_NewXMLQName(cx, left, right, nameqn->localName);
                if (!nameqn)
                    goto bad;

                /* 7(f)(iii). */
                attr = js_NewXML(cx, JSXML_CLASS_ATTRIBUTE);
                if (!attr)
                    goto bad;
                attr->parent = xml;
                attr->name = nameqn;

                /* 7(f)(iv). */
                ok = XMLARRAY_INSERT(cx, &xml->xml_attrs, n, attr);
                if (!ok) {
                    js_DestroyXML(cx, attr);
                    goto out;
                }

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
            !IS_STAR(nameqn->localName)) {
            goto out;
        }

        /* 10-11. */
        id = JSVAL_VOID;
        primitiveAssign = !vxml && !IS_STAR(nameqn->localName);

        /* 12. */
        k = n = xml->xml_kids.count;
        while (k != 0) {
            --k;
            kid = XMLARRAY_MEMBER(&xml->xml_kids, k, JSXML);
            if (MatchElemName(nameqn, kid)) {
                if (!JSVAL_IS_VOID(id)) {
                    ok = DeleteByIndex(cx, xml, id, &junk);
                    if (!ok)
                        goto out;
                }
                ok = IndexToIdVal(cx, k, &id);
                if (!ok)
                    goto out;
            }
        }

        /* 13. */
        if (JSVAL_IS_VOID(id)) {
            /* 13(a). */
            ok = IndexToIdVal(cx, n, &id);
            if (!ok)
                goto out;

            /* 13(b). */
            if (primitiveAssign) {
                if (nameqn->uri) {
                    ns = (JSXMLNamespace *)
                         JS_GetPrivate(cx, JSVAL_TO_OBJECT(nsval));
                    left = ns->uri;
                    right = ns->prefix;
                } else {
                    left = nameqn->uri;
                    right = nameqn->prefix;
                }
                nameqn = js_NewXMLQName(cx, left, right, nameqn->localName);
                if (!nameqn)
                    goto bad;

                /* 13(b)(iii). */
                vobj = js_NewXMLObject(cx, JSXML_CLASS_ELEMENT);
                if (!vxml)
                    goto bad;
                vxml = (JSXML *) JS_GetPrivate(cx, vobj);
                vxml->parent = xml;
                vxml->name = nameqn;

                /* 13(b)(iv-vi). */
                ns = GetNamespace(cx, nameqn, NULL);
                if (!ns)
                    goto bad;
                ok = Replace(cx, xml, id, OBJECT_TO_JSVAL(vobj));
                if (!ok)
                    goto out;
                ok = AddInScopeNamespace(cx, vxml, ns);
                if (!ok)
                    goto out;
            }
        }

        /* 14. */
        if (primitiveAssign) {
            js_IdIsIndex(id, &index);
            kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
            if (JSXML_HAS_KIDS(kid)) {
                XMLARRAY_FINISH(cx, &kid->xml_kids, js_DestroyXML);
                XMLARRAY_INIT(cx, &kid->xml_kids, 1);
            }

            /* 14(b-c). */
            ok = JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp);
            if (!ok)
                goto out;
            if (!IS_EMPTY(JSVAL_TO_STRING(*vp)))
                ok = Replace(cx, kid, JSVAL_ZERO, *vp);
        } else {
            /* 15(a). */
            ok = Replace(cx, xml, id, *vp);
        }
    }

out:
    JS_LeaveLocalRootScope(cx);
    return ok;
bad:
    ok = JS_FALSE;
    goto out;
}

/* ECMA-357 9.1.1.6 XML [[HasProperty]] and 9.2.1.5 XMLList [[HasProperty]]. */
static JSBool
HasProperty(JSContext *cx, JSObject *obj, jsval id, JSBool *foundp)
{
    JSXML *xml;
    uint32 i, n;
    JSXML *kid, *attr;
    JSObject *kidobj;
    JSXMLQName *qn;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    if (xml->xml_class == JSXML_CLASS_LIST) {
        n = JSXML_LENGTH(xml);
        if (js_IdIsIndex(id, &i)) {
            *foundp = i < n;
            return JS_TRUE;
        }

        for (i = 0; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                kidobj = js_GetXMLObject(cx, kid);
                if (!kidobj || !HasProperty(cx, kidobj, id, foundp))
                    return JS_FALSE;
                if (*foundp)
                    return JS_TRUE;
            }
        }
    } else if (xml->xml_class == JSXML_CLASS_ELEMENT) {
        if (js_IdIsIndex(id, &i)) {
            *foundp = (i == 0);
            return JS_TRUE;
        }

        qn = ToXMLName(cx, id);
        if (!qn)
            return JS_FALSE;

        if (OBJ_GET_CLASS(cx, qn->object) == &attribute_name_class) {
            for (i = 0, n = xml->xml_attrs.count; i < n; i++) {
                attr = XMLARRAY_MEMBER(&xml->xml_attrs, i, JSXML);
                *foundp = MatchAttrName(qn, attr->name);
                if (*foundp)
                    return JS_TRUE;
            }
        } else {
            for (i = 0, n = xml->xml_kids.count; i < n; i++) {
                kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
                *foundp = MatchElemName(qn, kid);
                if (*foundp)
                    return JS_TRUE;
            }
        }
    }

    *foundp = JS_FALSE;
    return JS_TRUE;
}

static void
xml_finalize(JSContext *cx, JSObject *obj)
{
    JSXML *xml;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    if (xml) {
        if (xml->object == obj)
            xml->object = NULL;
        js_DestroyXML(cx, xml);
    }
}

static void
xml_mark_private(JSContext *cx, JSXML *xml, void *arg);

static void
xml_mark_vector(JSContext *cx, JSXML **vec, uint32 len, void *arg)
{
    uint32 i;
    JSXML *elt;

    for (i = 0; i < len; i++) {
        elt = vec[i];
        if (elt->object) {
#ifdef GC_MARK_DEBUG
            char buf[100];
            JSXMLQName *qn = elt->name;

            JS_snprintf(buf, sizeof buf, "%s::%s",
                        qn->uri ? JS_GetStringBytes(qn->uri) : "*",
                        JS_GetStringBytes(qn->localName));
#else
            const char *buf = NULL;
#endif
            JS_MarkGCThing(cx, elt->object, buf, arg);
        } else {
            xml_mark_private(cx, elt, arg);
        }
    }
}

static void
xml_mark_private(JSContext *cx, JSXML *xml, void *arg)
{
    JSXMLQName *qn;

    qn = xml->name;
    if (qn) {
        if (qn->object)
            JS_MarkGCThing(cx, qn->object, "name", arg);
        else
            qname_mark_private(cx, qn, arg);
    }

    if (JSXML_HAS_VALUE(xml)) {
        JS_MarkGCThing(cx, xml->xml_value, "value", arg);
    } else {
        xml_mark_vector(cx,
                        (JSXML **) xml->xml_kids.vector,
                        xml->xml_kids.count,
                        arg);

        if (xml->xml_class == JSXML_CLASS_LIST) {
            if (xml->xml_target)
                xml_mark_private(cx, xml->xml_target, arg);
            if (xml->xml_targetprop)
                qname_mark_private(cx, xml->xml_targetprop, arg);
        } else {
            namespace_mark_vector(cx,
                                  (JSXMLNamespace **)
                                  xml->xml_namespaces.vector,
                                  xml->xml_namespaces.count,
                                  arg);
            xml_mark_vector(cx,
                            (JSXML **) xml->xml_attrs.vector,
                            xml->xml_attrs.count,
                            arg);
        }
    }
}

/*
 * js_XMLObjectOps.newObjectMap == js_NewObjectMap, so XML objects appear to
 * be native.  Therefore, xml_lookupProperty must return a valid JSProperty
 * pointer parameter via *propp to signify "property found".
 *
 * We use an all-zeroes static here to return a unique non-null address via
 * *propp.  The layer that calls into JSObjectOps never inspects *propp,
 * although it could expect that the id member match the sought-after jsid.
 * Nevertheless, we use all-zeroes for now, pending a more sweeping cleanup
 * of JSObjectOps.
 */
static JSScopeProperty dummy_sprop;

static JSBool
xml_lookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                   JSProperty **propp)
{
    JSBool ok, found;

    JS_LOCK_OBJ_VOID(cx, obj,
                     ok = HasProperty(cx, obj, ID_TO_VALUE(id), &found));
    if (!ok)
        return JS_FALSE;
    *objp = obj;
    *propp = found ? (JSProperty *) &dummy_sprop : NULL;
    return JS_TRUE;
}

static JSBool
xml_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                   JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                   JSProperty **propp)
{
    JSBool ok;

    if (JSVAL_IS_FUNCTION(cx, value) || getter || setter ||
        (attrs & JSPROP_ENUMERATE) == 0 ||
        (attrs & (JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED))) {
        ok = js_DefineProperty(cx, obj, id, value, getter, setter, attrs,
                               propp);
    } else {
        JS_LOCK_OBJ_VOID(cx, obj,
                         ok = PutProperty(cx, obj, ID_TO_VALUE(id), &value));
        if (!ok)
            return JS_FALSE;
        if (propp)
            *propp = (JSProperty *) &dummy_sprop;
    }
    return ok;
}

static JSBool
xml_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSBool ok;

    if (id == JS_DEFAULT_XML_NAMESPACE_ID) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    JS_LOCK_OBJ_VOID(cx, obj, ok = GetProperty(cx, obj, ID_TO_VALUE(id), vp));
    return ok;
}

static JSBool
xml_setProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSBool ok;

    JS_LOCK_OBJ_VOID(cx, obj, ok = PutProperty(cx, obj, ID_TO_VALUE(id), vp));
    return ok;
}

static JSBool
FoundProperty(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
              JSBool *foundp)
{
    JSBool ok;

    if (prop == (JSProperty *) &dummy_sprop) {
        ok = *foundp = JS_TRUE;
    } else {
        JS_LOCK_OBJ_VOID(cx, obj,
                         ok = HasProperty(cx, obj, ID_TO_VALUE(id), foundp));
    }
    return ok;
}

static JSBool
xml_getAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                  uintN *attrsp)
{
    JSBool found;

    if (!FoundProperty(cx, obj, id, prop, &found))
        return JS_FALSE;
    *attrsp = found ? JSPROP_ENUMERATE : 0;
    return JS_TRUE;
}

static JSBool
xml_setAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                  uintN *attrsp)
{
    JSBool found;

    if (!FoundProperty(cx, obj, id, prop, &found))
        return JS_FALSE;
    if (found) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_SET_XML_ATTRS);
    }
    return !found;
}

static JSBool
xml_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    JSBool ok;

    JS_LOCK_OBJ_VOID(cx, obj,
                     ok = DeleteProperty(cx, obj, ID_TO_VALUE(id), rval));
    return ok;
}

static JSBool
xml_defaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    return JS_CallFunctionName(cx, obj, js_toString_str, 0, NULL, vp);
}

static JSBool
xml_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
              jsval *statep, jsid *idp)
{
    return JS_TRUE;
}

static JSBool
xml_checkAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                jsval *vp, uintN *attrsp)
{
    if (!cx->runtime->checkObjectAccess)
        return JS_TRUE;
    return cx->runtime->checkObjectAccess(cx, obj, ID_TO_VALUE(id), mode, vp);
}

static JSBool
xml_hasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    return JS_TRUE;
}

static uint32
xml_mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSXML *xml;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    xml_mark_private(cx, xml, arg);
    return 0;
}

static void
xml_clear(JSContext *cx, JSObject *obj)
{
}

static JSBool
xml_callMethod(JSContext *cx, JSObject *obj, jsid id, uintN argc, jsval *argv,
               jsval *rval)
{
    JSStackFrame *fp;
    jsval fval;
    JSXML *xml, *kid;

    if (!JS_InstanceOf(cx, obj, &js_XMLClass, argv))
        return JS_FALSE;
    fp = cx->fp;

retry:
    /* 11.2.2.1 Step 3(d). */
    if (!js_GetProperty(cx, OBJ_GET_PROTO(cx, obj), id, &fval))
        return JS_FALSE;

    if (JSVAL_IS_VOID(fval)) {
        xml = (JSXML *) JS_GetPrivate(cx, obj);
        if (xml->xml_class == JSXML_CLASS_LIST) {
            JSScope *scope;

            JS_LOCK_OBJ(cx, obj);
            scope = OBJ_SCOPE(obj);
            if (xml->xml_kids.count == 1) {
                xml = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
                obj = js_GetXMLObject(cx, xml);
                JS_UNLOCK_SCOPE(cx, scope);
                if (!obj)
                    return JS_FALSE;
                fp->argv[-1] = OBJECT_TO_JSVAL(obj);
                fp->thisp = obj;
                goto retry;
            }
            JS_UNLOCK_SCOPE(cx, scope);
        }

        if (xml->xml_class != JSXML_CLASS_LIST) {
            JSBool simple;
            uint32 i, n;
            JSString *str;

            switch (xml->xml_class) {
              case JSXML_CLASS_COMMENT:
              case JSXML_CLASS_PROCESSING_INSTRUCTION:
                simple = JS_FALSE;
                break;
              default:
                simple = JS_TRUE;
                JS_LOCK_OBJ(cx, obj);
                for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
                    kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
                    if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                        simple = JS_FALSE;
                        break;
                    }
                }
                JS_UNLOCK_OBJ(cx, obj);
                break;
            }
            if (simple) {
                str = js_ValueToString(cx, OBJECT_TO_JSVAL(obj));
                if (!str || !js_ValueToObject(cx, STRING_TO_JSVAL(str), &obj))
                    return JS_FALSE;
                fp->argv[-1] = OBJECT_TO_JSVAL(obj);
                fp->thisp = obj;
                goto retry;
            }
        }
    }

    fp->argv[-2] = fval;
    if (!js_Invoke(cx, fp->argc, JSFRAME_SKIP_CALLER))
        return JS_FALSE;
    *rval = fp->sp[-1];
    return JS_TRUE;
}

static JSBool
xml_enumerateValues(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                    jsval *statep, jsval *vp)
{
    return JS_TRUE;
}

static JSBool
xml_equality(JSContext *cx, JSObject *obj, jsval v, jsval *vp)
{
    return JS_TRUE;
}

static JSBool
xml_concatenate(JSContext *cx, JSObject *obj, jsval v, jsval *vp)
{
    return JS_TRUE;
}

JSXMLObjectOps js_XMLObjectOps = {
    {
        /* Use js_NewObjectMap so XML objects satisfy OBJ_IS_NATIVE tests. */
        js_NewObjectMap,        js_DestroyObjectMap,
        xml_lookupProperty,     xml_defineProperty,
        xml_getProperty,        xml_setProperty,
        xml_getAttributes,      xml_setAttributes,
        xml_deleteProperty,     xml_defaultValue,
        xml_enumerate,          xml_checkAccess,
        NULL,                   NULL,
        NULL,                   NULL,
        NULL,                   xml_hasInstance,
        js_SetProtoOrParent,    js_SetProtoOrParent,
        xml_mark,               xml_clear,
        NULL,                   NULL
    },
    xml_callMethod,             xml_enumerateValues,
    xml_equality,               xml_concatenate
};

static JSObjectOps *
xml_getObjectOps(JSContext *cx, JSClass *clasp)
{
    return &js_XMLObjectOps.base;
}

JSClass js_XMLClass = {
    js_XML_str,
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    JS_ConvertStub,    xml_finalize,
    xml_getObjectOps,  NULL,              NULL,              NULL,
    NULL,              NULL,              NULL,              NULL
};

static JSBool
xml_addNamespace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    JSXML *xml;
    JSObject *nsobj;
    JSXMLNamespace *ns;
    JSBool ok;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    nsobj = js_ConstructObject(cx, &namespace_class, NULL, NULL, 1, argv);
    if (!nsobj)
        return JS_FALSE;
    ns = (JSXMLNamespace *) JS_GetPrivate(cx, nsobj);
    JS_LOCK_OBJ_VOID(cx, obj, ok = AddInScopeNamespace(cx, xml, ns));
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
xml_appendChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    jsval name, v;
    JSBool ok;
    JSObject *vobj;
    JSXML *vxml;

    if (!js_GetAnyName(cx, &name))
        return JS_FALSE;

    JS_LOCK_OBJ(cx, obj);
    ok = GetProperty(cx, obj, name, &v);
    if (!ok)
        goto out;
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
    vobj = JSVAL_TO_OBJECT(v);
    JS_ASSERT(OBJECT_IS_XML(cx, vobj));
    vxml = (JSXML *) JS_GetPrivate(cx, vobj);
    JS_ASSERT(vxml->xml_class == JSXML_CLASS_LIST);

    ok = IndexToIdVal(cx, vxml->xml_kids.count, &name);
    if (!ok)
        goto out;
    ok = PutProperty(cx, JSVAL_TO_OBJECT(v), name, &argv[0]);
out:
    JS_UNLOCK_OBJ(cx, obj);
    *rval = OBJECT_TO_JSVAL(obj);
    return ok;
}

/* XML and XMLList */
static JSBool
xml_attribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    JSXMLQName *qn;
    jsval name;
    JSBool ok;

    qn = ToAttributeName(cx, argv[0]);
    if (!qn)
        return JS_FALSE;
    name = OBJECT_TO_JSVAL(qn->object);
    JS_LOCK_OBJ_VOID(cx, obj, ok = GetProperty(cx, obj, name, rval));
    return ok;
}

/* XML and XMLList */
static JSBool
xml_attributes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    jsval name;
    JSXMLQName *qn;
    JSBool ok;

    name = ATOM_KEY(cx->runtime->atomState.starAtom);
    qn = ToAttributeName(cx, name);
    if (!qn)
        return JS_FALSE;
    name = OBJECT_TO_JSVAL(qn->object);
    JS_LOCK_OBJ_VOID(cx, obj, ok = GetProperty(cx, obj, name, rval));
    return ok;
}

/* XML and XMLList */
static JSBool
xml_child_helper(JSContext *cx, JSObject *obj, JSXML *xml, jsval name,
                 jsval *rval)
{
    uint32 index;
    JSBool ok;
    JSXML *kid;
    JSObject *kidobj;

    /* ECMA-357 13.4.4.6 */
    JS_ASSERT(xml->xml_class != JSXML_CLASS_LIST);

    if (js_IdIsIndex(name, &index)) {
        ok = JS_TRUE;
        JS_LOCK_OBJ(cx, obj);
        if (index >= JSXML_LENGTH(xml)) {
            *rval = JSVAL_VOID;
        } else {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, index, JSXML);
            kidobj = js_GetXMLObject(cx, kid);
            if (!kidobj)
                ok = JS_FALSE;
            else
                *rval = OBJECT_TO_JSVAL(kidobj);
        }
        JS_UNLOCK_OBJ(cx, obj);
        return ok;
    }

    JS_LOCK_OBJ_VOID(cx, obj, ok = GetProperty(cx, obj, name, rval));
    return ok;
}

static JSBool
xml_child(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSXML *xml, *list, *kid, *vxml;
    jsval name, v;
    uint32 i, n;
    JSObject *listobj, *kidobj;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    name = argv[0];
    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* ECMA-357 13.5.4.4 */
        listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
        if (!listobj)
            return JS_FALSE;

        *rval = OBJECT_TO_JSVAL(listobj);
        list = (JSXML *) JS_GetPrivate(cx, listobj);
        list->xml_target = xml;

        for (i = 0, n = xml->xml_kids.count; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            kidobj = js_GetXMLObject(cx, kid);
            if (!kidobj)
                return JS_FALSE;
            if (!xml_child_helper(cx, kidobj, kid, name, &v))
                return JS_FALSE;

            JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
            vxml = (JSXML *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(v));
            if (JSXML_LENGTH(vxml) != 0 && !Append(cx, list, vxml))
                return JS_FALSE;
        }
        return JS_TRUE;
    }

    return xml_child_helper(cx, obj, xml, name, rval);
}

static JSBool
xml_childIndex(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    JSXML *xml, *parent;
    JSBool locked;
    uint32 i, n;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    parent = xml->parent;
    if (!parent || xml->xml_class == JSXML_CLASS_ATTRIBUTE) {
        *rval = DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
        return JS_TRUE;
    }
    if (parent->object) {
        JS_LOCK_OBJ(cx, parent->object);
        locked = JS_TRUE;
    } else {
        locked = JS_FALSE;
    }
    for (i = 0, n = JSXML_LENGTH(parent); i < n; i++) {
        if (XMLARRAY_MEMBER(&parent->xml_kids, i, JSXML) == xml)
            break;
    }
    if (locked)
        JS_UNLOCK_OBJ(cx, parent->object);
    JS_ASSERT(i < n);
    return js_NewNumberValue(cx, i, rval);
}

/* XML and XMLList */
static JSBool
xml_children(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    jsval name;
    JSBool ok;

    name = ATOM_KEY(cx->runtime->atomState.starAtom);
    JS_LOCK_OBJ_VOID(cx, obj, ok = GetProperty(cx, obj, name, rval));
    return ok;
}

/* XML and XMLList */
static JSBool
xml_comments(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    JSXML *xml, *list, *kid, *vxml;
    JSObject *listobj, *kidobj;
    JSBool ok;
    uint32 i, n;
    jsval v;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return JS_FALSE;

    *rval = OBJECT_TO_JSVAL(listobj);
    list = (JSXML *) JS_GetPrivate(cx, listobj);
    list->xml_target = xml;

    ok = JS_TRUE;
    JS_LOCK_OBJ(cx, obj);

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* 13.5.4.6 Step 2. */
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = JS_EnterLocalRootScope(cx);
                if (!ok)
                    break;
                kidobj = js_GetXMLObject(cx, kid);
                ok = kidobj
                     ? xml_comments(cx, kidobj, argc, argv, &v)
                     : JS_FALSE;
                JS_LeaveLocalRootScope(cx);
                if (!ok)
                    break;
                vxml = (JSXML *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(v));
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
            if (kid->xml_class == JSXML_CLASS_COMMENT) {
                ok = Append(cx, list, kid);
                if (!ok)
                    break;
            }
        }
    }

    JS_UNLOCK_OBJ(cx, obj);
    return ok;
}

/* XML and XMLList */
static JSBool
xml_contains(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    JSXML *xml;
    JSBool ok;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    JS_LOCK_OBJ_VOID(cx, obj, ok = Equals(cx, xml, argv[0]));
    return ok;
}

/* XML and XMLList */
static JSBool
xml_copy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSXML *xml, *copy;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    JS_LOCK_OBJ_VOID(cx, obj, copy = DeepCopy(cx, xml));
    if (!copy)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(copy->object);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_descendants(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    JSXML *xml, *list;
    jsval name;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    name = (argc == 0) ? ATOM_KEY(cx->runtime->atomState.starAtom) : argv[0];
    JS_LOCK_OBJ_VOID(cx, obj, list = Descendants(cx, xml, name));
    if (!list)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(list->object);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_elements(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    JSXML *xml, *list, *kid, *vxml;
    jsval name, v;
    JSXMLQName *nameqn;
    JSObject *listobj, *kidobj;
    JSBool ok;
    uint32 i, n;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    name = (argc == 0) ? ATOM_KEY(cx->runtime->atomState.starAtom) : argv[0];
    nameqn = ToXMLName(cx, name);
    if (!nameqn)
        return JS_FALSE;

    listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!listobj)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(listobj);
    list = (JSXML *) JS_GetPrivate(cx, listobj);
    list->xml_target = xml;
    list->xml_targetprop = nameqn;

    ok = JS_TRUE;
    JS_LOCK_OBJ(cx, obj);

    if (xml->xml_class == JSXML_CLASS_LIST) {
        /* 13.5.4.6 */
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                ok = JS_EnterLocalRootScope(cx);
                if (!ok)
                    break;
                kidobj = js_GetXMLObject(cx, kid);
                ok = kidobj
                     ? xml_elements(cx, kidobj, argc, argv, &v)
                     : JS_FALSE;
                JS_LeaveLocalRootScope(cx);
                if (!ok)
                    break;
                vxml = (JSXML *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(v));
                if (JSXML_LENGTH(vxml) != 0) {
                    ok = Append(cx, list, vxml);
                    if (!ok)
                        break;
                }
            }
        }
    } else {
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class == JSXML_CLASS_ELEMENT &&
                MatchElemName(nameqn, kid)) {
                ok = Append(cx, list, kid);
                if (!ok)
                    break;
            }
        }
    }

    JS_UNLOCK_OBJ(cx, obj);
    return ok;
}

/* XML and XMLList */
static JSBool
xml_hasOwnProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
    jsval name;
    JSBool ok, found;

    name = argv[0];
    JS_LOCK_OBJ_VOID(cx, obj, ok = HasProperty(cx, obj, name, &found));
    if (!ok)
        return JS_FALSE;
    if (!found)
        return js_obj_hasOwnProperty(cx, obj, argc, argv, rval);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_hasComplexContent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                      jsval *rval)
{
    JSXML *xml, *kid;
    JSObject *kidobj;
    uint32 i, n;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    switch (xml->xml_class) {
      case JSXML_CLASS_ATTRIBUTE:
      case JSXML_CLASS_COMMENT:
      case JSXML_CLASS_PROCESSING_INSTRUCTION:
      case JSXML_CLASS_TEXT:
        *rval = JSVAL_FALSE;
        break;
      case JSXML_CLASS_LIST:
        JS_LOCK_OBJ(cx, obj);
        if (xml->xml_kids.count == 0) {
            *rval = JSVAL_TRUE;
        } else if (xml->xml_kids.count == 1) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
            kidobj = js_GetXMLObject(cx, kid);
            JS_UNLOCK_OBJ(cx, obj);
            if (!kidobj)
                return JS_FALSE;
            return xml_hasComplexContent(cx, kidobj, argc, argv, rval);
        }
        /* FALL THROUGH */
      default:
        *rval = JSVAL_FALSE;
        for (i = 0, n = xml->xml_kids.count; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                *rval = JSVAL_TRUE;
                break;
            }
        }
        JS_UNLOCK_OBJ(cx, obj);
        break;
    }
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_hasSimpleContent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
    JSXML *xml, *kid;
    JSObject *kidobj;
    uint32 i, n;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    switch (xml->xml_class) {
      case JSXML_CLASS_COMMENT:
      case JSXML_CLASS_PROCESSING_INSTRUCTION:
        *rval = JSVAL_FALSE;
        break;
      case JSXML_CLASS_LIST:
        JS_LOCK_OBJ(cx, obj);
        if (xml->xml_kids.count == 0) {
            *rval = JSVAL_TRUE;
        } else if (xml->xml_kids.count == 1) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, 0, JSXML);
            kidobj = js_GetXMLObject(cx, kid);
            JS_UNLOCK_OBJ(cx, obj);
            if (!kidobj)
                return JS_FALSE;
            return xml_hasSimpleContent(cx, kidobj, argc, argv, rval);
        }
        /* FALL THROUGH */
      default:
        *rval = JSVAL_TRUE;
        for (i = 0, n = JSXML_LENGTH(xml); i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class == JSXML_CLASS_ELEMENT) {
                *rval = JSVAL_FALSE;
                break;
            }
        }
        JS_UNLOCK_OBJ(cx, obj);
        break;
    }
    return JS_TRUE;
}

static JSBool
xml_inScopeNamespaces(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                      jsval *rval)
{
    JSObject *arrayobj, *nsobj;
    JSXML *xml;
    JSBool ok, locked;
    uint32 length, i, j, n;
    JSXMLNamespace *ns, *ns2;
    jsval v;

    arrayobj = js_NewArrayObject(cx, 0, NULL);
    if (!arrayobj)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(arrayobj);
    length = 0;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    ok = JS_TRUE;
    do {
        if (xml->object) {
            JS_LOCK_OBJ(cx, obj);
            locked = JS_TRUE;
        } else {
            locked = JS_FALSE;
        }
        for (i = 0, n = xml->xml_namespaces.count; i < n; i++) {
            ns = XMLARRAY_MEMBER(&xml->xml_namespaces, i, JSXMLNamespace);

            for (j = 0; j < length; j++) {
                ok = JS_GetElement(cx, arrayobj, j, &v);
                if (!ok)
                    goto break2;
                nsobj = JSVAL_TO_OBJECT(v);
                ns2 = (JSXMLNamespace *) JS_GetPrivate(cx, nsobj);
                if ((ns2->prefix && ns->prefix)
                    ? !js_CompareStrings(ns2->prefix, ns->prefix)
                    : !js_CompareStrings(ns2->uri, ns->uri)) {
                    break;
                }
            }

            if (j == length) {
                nsobj = js_GetXMLNamespaceObject(cx, ns);
                if (!nsobj) {
                    ok = JS_FALSE;
                    break;
                }
                v = OBJECT_TO_JSVAL(nsobj);
                ok = JS_SetElement(cx, arrayobj, length, &v);
                if (!ok)
                    break;
                ++length;
            }
        }

      break2:
        xml = xml->parent;
        if (locked)
            JS_UNLOCK_OBJ(cx, obj);
    } while (xml && ok);
    return ok;
}

#define XML_NYI(name)                                                         \
    JS_BEGIN_MACRO                                                            \
        JS_ReportError(cx, #name " is not yet implemented");                  \
        return JS_FALSE;                                                      \
    JS_END_MACRO

static JSBool
xml_insertChildAfter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
    XML_NYI(insertChildAfter);
}

static JSBool
xml_insertChildBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                      jsval *rval)
{
    XML_NYI(insertChildBefore);
}

/* XML and XMLList */
static JSBool
xml_length(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSXML *xml;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    if (xml->xml_class != JSXML_CLASS_LIST) {
        *rval = JSVAL_ONE;
    } else {
        if (!js_NewNumberValue(cx, xml->xml_kids.count, rval))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
xml_localName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    JSXML *xml;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    *rval = xml->name ? STRING_TO_JSVAL(xml->name->localName) : JSVAL_NULL;
    return JS_TRUE;
}

static JSBool
xml_name(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSXML *xml;
    JSObject *nameobj;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    if (!xml->name) {
        *rval = JSVAL_NULL;
    } else {
        JS_LOCK_OBJ_VOID(cx, obj,
                         nameobj = js_GetXMLQNameObject(cx, xml->name));
        if (!nameobj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(nameobj);
    }
    return JS_TRUE;
}

static JSBool
xml_namespace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    XML_NYI(namespace);
}

static JSBool
xml_namespaceDeclarations(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                          jsval *rval)
{
    XML_NYI(namespaceDeclarations);
}

static JSBool
xml_nodeKind(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    XML_NYI(nodeKind);
}

/* XML and XMLList */
static JSBool
xml_normalize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    XML_NYI(normalize);
}

/* XML and XMLList */
static JSBool
xml_parent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    XML_NYI(parent);
}

/* XML and XMLList */
static JSBool
xml_processingInstructions(JSContext *cx, JSObject *obj, uintN argc,
                           jsval *argv, jsval *rval)
{
    XML_NYI(processingInstructions);
}

static JSBool
xml_prependChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    XML_NYI(prependChild);
}

/* XML and XMLList */
static JSBool
xml_propertyIsEnumerable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                         jsval *rval)
{
    XML_NYI(propertyIsEnumerable);
}

static JSBool
xml_removeNamespace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                    jsval *rval)
{
    XML_NYI(removeNamespace);
}

static JSBool
xml_replace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    XML_NYI(replace);
}

static JSBool
xml_setChildren(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    XML_NYI(setChildren);
}

static JSBool
xml_setLocalName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    XML_NYI(setLocalName);
}

static JSBool
xml_setName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    XML_NYI(setName);
}

static JSBool
xml_setNamespace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    XML_NYI(setNamespace);
}

/* XML and XMLList */
static JSBool
xml_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    XML_NYI(text);
}

/* XML and XMLList */
static JSBool
xml_toXMLString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    JSString *str;

    str = ToXMLString(cx, OBJECT_TO_JSVAL(obj), NULL, 0);
    if (!str)
        return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    JSXML *xml, *kid;
    JSString *str;
    uint32 i, n;

    xml = (JSXML *) JS_GetPrivate(cx, obj);
    if (xml->xml_class == JSXML_CLASS_ATTRIBUTE ||
        xml->xml_class == JSXML_CLASS_TEXT) {
        str = xml->xml_value;
    } else {
        if (!xml_hasSimpleContent(cx, obj, argc, argv, rval))
            return JS_FALSE;
        if (*rval == JSVAL_FALSE)
            return xml_toXMLString(cx, obj, argc, argv, rval);

        str = cx->runtime->emptyString;
        for (i = 0, n = xml->xml_kids.count; i < n; i++) {
            kid = XMLARRAY_MEMBER(&xml->xml_kids, i, JSXML);
            if (kid->xml_class != JSXML_CLASS_COMMENT &&
                kid->xml_class != JSXML_CLASS_PROCESSING_INSTRUCTION) {
                str = js_ConcatStrings(cx, str, kid->xml_value);
                if (!str)
                    return JS_FALSE;
            }
        }
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/* XML and XMLList */
static JSBool
xml_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSFunctionSpec xml_methods[] = {
    {"addNamespace",          xml_addNamespace,          1,0,0},
    {"appendChild",           xml_appendChild,           1,0,0},
    {"attribute",             xml_attribute,             1,0,0},
    {"attributes",            xml_attributes,            0,0,0},
    {"child",                 xml_child,                 1,0,0},
    {"childIndex",            xml_childIndex,            0,0,0},
    {"children",              xml_children,              0,0,0},
    {"comments",              xml_comments,              0,0,0},
    {"contains",              xml_contains,              1,0,0},
    {"copy",                  xml_copy,                  0,0,0},
    {"descendants",           xml_descendants,           1,0,0},
    {"elements",              xml_elements,              1,0,0},
    {"hasOwnProperty",        xml_hasOwnProperty,        1,0,0},
    {"hasComplexContent",     xml_hasComplexContent,     1,0,0},
    {"hasSimpleContent",      xml_hasSimpleContent,      1,0,0},
    {"inScopeNamespaces",     xml_inScopeNamespaces,     0,0,0},
    {"insertChildAfter",      xml_insertChildAfter,      2,0,0},
    {"insertChildBefore",     xml_insertChildBefore,     2,0,0},
    {js_length_str,           xml_length,                0,0,0},
    {js_localName_str,        xml_localName,             0,0,0},
    {"name",                  xml_name,                  0,0,0},
    {js_namespace_str,        xml_namespace,             1,0,0},
    {"namespaceDeclarations", xml_namespaceDeclarations, 0,0,0},
    {"nodeKind",              xml_nodeKind,              0,0,0},
    {"normalize",             xml_normalize,             0,0,0},
    {"parent",                xml_parent,                0,0,0},
    {"processingInstructions",xml_processingInstructions,1,0,0},
    {"prependChild",          xml_prependChild,          1,0,0},
    {"propertyIsEnumerable",  xml_propertyIsEnumerable,  1,0,0},
    {"removeNamespace",       xml_removeNamespace,       1,0,0},
    {"replace",               xml_replace,               2,0,0},
    {"setChildren",           xml_setChildren,           1,0,0},
    {"setLocalName",          xml_setLocalName,          1,0,0},
    {"setName",               xml_setName,               1,0,0},
    {"setNamespace",          xml_setNamespace,          1,0,0},
    {"text",                  xml_text,                  0,0,0},
    {js_toString_str,         xml_toString,              0,0,0},
    {js_toXMLString_str,      xml_toXMLString,           0,0,0},
    {js_valueOf_str,          xml_valueOf,               0,0,0},
    {0,0,0,0,0}
};

/*
 * NB: These tinyids must
 * (a) not collide with the generic negative tinyids at the top of jsfun.c;
 * (b) index their corresponding xml_static_props array elements.
 * Don't change 'em!
 */
enum xml_static_tinyid {
    XML_IGNORE_COMMENTS,
    XML_IGNORE_PROCESSING_INSTRUCTIONS,
    XML_IGNORE_WHITESPACE,
    XML_PRETTY_PRINTING,
    XML_PRETTY_INDENT
};

static JSPropertySpec xml_static_props[] = {
    {js_ignoreComments_str,     XML_IGNORE_COMMENTS,   JSPROP_PERMANENT, 0, 0},
    {js_ignoreProcessingInstructions_str,
     XML_IGNORE_PROCESSING_INSTRUCTIONS,               JSPROP_PERMANENT, 0, 0},
    {js_ignoreWhitespace_str,   XML_IGNORE_WHITESPACE, JSPROP_PERMANENT, 0, 0},
    {js_prettyPrinting_str,     XML_PRETTY_PRINTING,   JSPROP_PERMANENT, 0, 0},
    {js_prettyIndent_str,       XML_PRETTY_INDENT,     JSPROP_PERMANENT, 0, 0},
    {0,0,0,0,0}
};

static JSBool
CopyXMLSettings(JSContext *cx, JSObject *from, JSObject *to)
{
    int i;
    const char *name;
    jsval v;

    for (i = XML_IGNORE_COMMENTS; i < XML_PRETTY_INDENT; i++) {
        name = xml_static_props[i].name;
        if (!JS_GetProperty(cx, from, name, &v))
            return JS_FALSE;
        if (JSVAL_IS_BOOLEAN(v) && !JS_SetProperty(cx, to, name, &v))
            return JS_FALSE;
    }

    name = xml_static_props[i].name;
    if (!JS_GetProperty(cx, from, name, &v))
        return JS_FALSE;
    if (JSVAL_IS_NUMBER(v) && !JS_SetProperty(cx, to, name, &v))
        return JS_FALSE;
    return JS_TRUE;
}

static JSBool
SetDefaultXMLSettings(JSContext *cx, JSObject *obj)
{
    int i;
    jsval v;

    for (i = XML_IGNORE_COMMENTS; i < XML_PRETTY_INDENT; i++) {
        v = JSVAL_TRUE;
        if (!JS_SetProperty(cx, obj, xml_static_props[i].name, &v))
            return JS_FALSE;
    }
    v = INT_TO_JSVAL(2);
    return JS_SetProperty(cx, obj, xml_static_props[i].name, &v);
}

static JSBool
xml_settings(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *settings;

    settings = JS_NewObject(cx, NULL, NULL, NULL);
    if (!settings)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(settings);
    return CopyXMLSettings(cx, obj, settings);
}

static JSBool
xml_setSettings(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    jsval v;
    JSObject *settings;

    v = argv[0];
    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        return SetDefaultXMLSettings(cx, obj);

    if (JSVAL_IS_PRIMITIVE(v))
        return JS_TRUE;
    settings = JSVAL_TO_OBJECT(v);
    return CopyXMLSettings(cx, settings, obj);
}

static JSBool
xml_defaultSettings(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                    jsval *rval)
{
    JSObject *settings;

    settings = JS_NewObject(cx, NULL, NULL, NULL);
    if (!settings)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(settings);
    return SetDefaultXMLSettings(cx, settings);
}

static JSFunctionSpec xml_static_methods[] = {
    {"settings",         xml_settings,          0,0,0},
    {"setSettings",      xml_setSettings,       1,0,0},
    {"defaultSettings",  xml_defaultSettings,   0,0,0},
    {0,0,0,0,0}
};

static JSBool
XML(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    JSXML *xml, *copy;
    JSObject *xobj, *vobj;
    JSClass *clasp;

    v = argv[0];
    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        v = STRING_TO_JSVAL(cx->runtime->emptyString);

    xml = ToXML(cx, v);
    if (!xml)
        return JS_FALSE;
    xobj = xml->object;
    *rval = OBJECT_TO_JSVAL(xobj);

    if ((cx->fp->flags & JSFRAME_CONSTRUCTING) && !JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        clasp = OBJ_GET_CLASS(cx, vobj);
        if (clasp == &js_XMLClass ||
            (clasp->flags & JSCLASS_W3C_XML_INFO_ITEM)) {
            copy = DeepCopy(cx, xml);
            if (!copy)
                return JS_FALSE;
            *rval = OBJECT_TO_JSVAL(copy->object);
            return JS_TRUE;
        }
    }
    return JS_TRUE;
}

static JSBool
XMLList(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    JSObject *vobj, *listobj;
    JSXML *xml, *list;

    v = argv[0];
    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v))
        v = STRING_TO_JSVAL(cx->runtime->emptyString);

    if ((cx->fp->flags & JSFRAME_CONSTRUCTING) && !JSVAL_IS_PRIMITIVE(v)) {
        vobj = JSVAL_TO_OBJECT(v);
        if (OBJ_GET_CLASS(cx, vobj) == &js_XMLClass) {
            xml = (JSXML *) JS_GetPrivate(cx, vobj);
            if (xml->xml_class == JSXML_CLASS_LIST) {
                listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
                if (!listobj)
                    return JS_FALSE;
                *rval = OBJECT_TO_JSVAL(listobj);

                list = (JSXML *) JS_GetPrivate(cx, listobj);
                if (!Append(cx, list, xml))
                    return JS_FALSE;
                return JS_TRUE;
            }
        }
    }

    list = ToXMLList(cx, v);
    if (!list)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(list->object);
    return JS_TRUE;
}

#define JSXML_LIST_SIZE         sizeof(JSXML)
#define JSXML_ELEMENT_SIZE      sizeof(JSXML)
#define JSXML_SIMPLEX_SIZE      (offsetof(JSXML, u) + sizeof(JSString *))

static size_t sizeof_JSXML[JSXML_CLASS_LIMIT] = {
    JSXML_LIST_SIZE,            /* JSXML_CLASS_LIST */
    JSXML_ELEMENT_SIZE,         /* JSXML_CLASS_ELEMENT */
    JSXML_SIMPLEX_SIZE,         /* JSXML_CLASS_TEXT */
    JSXML_SIMPLEX_SIZE,         /* JSXML_CLASS_ATTRIBUTE */
    JSXML_SIMPLEX_SIZE,         /* JSXML_CLASS_COMMENT */
    JSXML_SIMPLEX_SIZE          /* JSXML_CLASS_PROCESSING_INSTRUCTION */
};

JSXML *
js_NewXML(JSContext *cx, JSXMLClass xml_class)
{
    JSXML *xml;

    xml = (JSXML *) JS_malloc(cx, sizeof_JSXML[xml_class]);
    if (!xml)
        return NULL;

    xml->object = NULL;
    xml->parent = NULL;
    xml->name = NULL;
    xml->nrefs = 1;
    xml->flags = 0;
    xml->xml_class = xml_class;
    if (JSXML_CLASS_HAS_VALUE(xml_class)) {
        xml->xml_value = cx->runtime->emptyString;
    } else {
        XMLARRAY_INIT(cx, &xml->xml_kids, 0);
        if (xml_class == JSXML_CLASS_LIST) {
            xml->xml_target = NULL;
            xml->xml_targetprop = NULL;
        } else {
            XMLARRAY_INIT(cx, &xml->xml_namespaces, 0);
            XMLARRAY_INIT(cx, &xml->xml_attrs, 0);
        }
    }
    return xml;
}

void
js_DestroyXML(JSContext *cx, JSXML *xml)
{
    if (xml->object || JS_ATOMIC_DECREMENT(&xml->nrefs) != 0)
        return;
    if (xml->name)
        js_DestroyXMLQName(cx, xml->name);
    if (JSXML_HAS_KIDS(xml)) {
        XMLARRAY_FINISH(cx, &xml->xml_kids, js_DestroyXML);
        if (xml->xml_class != JSXML_CLASS_LIST) {
            XMLARRAY_FINISH(cx, &xml->xml_namespaces, js_DestroyXMLNamespace);
            XMLARRAY_FINISH(cx, &xml->xml_attrs, js_DestroyXML);
        }
    }
}

static JSXMLQName *
StringToQName(JSContext *cx, JSString *str)
{
    jsval nsval;
    JSXMLNamespace *ns;

    JS_ASSERT(JSSTRING_LENGTH(str) != 0 && *JSSTRING_CHARS(str) != '@');
    JS_ASSERT(JSSTRING_LENGTH(str) != 1 || *JSSTRING_CHARS(str) != '*');

    if (!js_GetDefaultXMLNamespace(cx, &nsval))
        return NULL;
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(nsval));
    ns = (JSXMLNamespace *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(nsval));
    return js_NewXMLQName(cx, ns->uri, ns->prefix, str);
}

JSXML *
js_ParseNodeToXML(JSContext *cx, JSParseNode *pn, uint32 flags)
{
    JSXML *xml, *kid, *attr;
    JSParseNode *pn2;
    uint32 i;
    JSXMLQName *qn;
    JSString *str;
    JSXMLClass xml_class;

    xml = NULL;
    switch (pn->pn_type) {
      case TOK_XMLELEM:
        pn2 = pn->pn_head;
        xml = js_ParseNodeToXML(cx, pn2, flags);
        if (!xml)
            return NULL;

        for (i = 0; (pn2 = pn2->pn_next) != NULL; i++) {
            if (!pn2->pn_next) {
                /* Don't append the end tag! */
                JS_ASSERT(pn2->pn_type == TOK_XMLETAGO);
                break;
            }

            kid = js_ParseNodeToXML(cx, pn2, flags);
            if (!kid || !XMLARRAY_INSERT(cx, &xml->xml_kids, i, kid)) {
                if (kid)
                    js_DestroyXML(cx, kid);
                goto fail;
            }
            kid->parent = xml;
        }
        break;

      case TOK_XMLLIST:
        xml = js_NewXML(cx, JSXML_CLASS_LIST);
        if (!xml)
            return NULL;
        for (i = 0, pn2 = pn->pn_head; pn2; i++, pn2 = pn2->pn_next) {
            kid = js_ParseNodeToXML(cx, pn2, flags);
            if (!kid || !XMLARRAY_INSERT(cx, &xml->xml_kids, i, kid)) {
                if (kid)
                    js_DestroyXML(cx, kid);
                goto fail;
            }
        }
        xml->flags = flags;
        break;

      case TOK_XMLSTAGO:
      case TOK_XMLPTAGC:
        pn2 = pn->pn_head;
        xml = js_ParseNodeToXML(cx, pn2, flags);
        if (!xml)
            return NULL;

        for (i = 0; (pn2 = pn2->pn_next) != NULL; i++) {
            if (pn2->pn_type != TOK_XMLNAME || pn2->pn_arity != PN_NULLARY)
                goto syntax;
            qn = StringToQName(cx, ATOM_TO_STRING(pn2->pn_atom));
            if (!qn)
                goto fail;

            pn2 = pn2->pn_next;
            JS_ASSERT(pn2);
            if (pn2->pn_type != TOK_XMLATTR) {
                js_DestroyXMLQName(cx, qn);
                goto syntax;
            }
            attr = js_NewXML(cx, JSXML_CLASS_ATTRIBUTE);
            if (!attr || !XMLARRAY_INSERT(cx, &xml->xml_attrs, i, attr)) {
                if (attr)
                    js_DestroyXML(cx, attr);
                js_DestroyXMLQName(cx, qn);
                goto fail;
            }

            attr->parent = xml;
            attr->name = qn;
            attr->flags = flags;
            attr->xml_value = ATOM_TO_STRING(pn2->pn_atom);
        }
        break;

      case TOK_XMLNAME:
        if (pn->pn_arity == PN_LIST)
            goto syntax;
        JS_ASSERT(pn->pn_arity == PN_NULLARY);
        qn = StringToQName(cx, ATOM_TO_STRING(pn->pn_atom));
        if (!qn)
            return NULL;

        xml = js_NewXML(cx, JSXML_CLASS_ELEMENT);
        if (!xml) {
            js_DestroyXMLQName(cx, qn);
            return NULL;
        }
        xml->name = qn;
        xml->flags = flags;
        break;

      case TOK_XMLTEXT:
      case TOK_XMLCDATA:
      case TOK_XMLCOMMENT:
      case TOK_XMLPI:
        qn = NULL;
        str = ATOM_TO_STRING(pn->pn_atom);
        if (pn->pn_type == TOK_XMLCOMMENT) {
            xml_class = JSXML_CLASS_COMMENT;
        } else if (pn->pn_type == TOK_XMLPI) {
            qn = StringToQName(cx, str);
            if (!qn)
                return NULL;
            str = pn->pn_atom2
                  ? ATOM_TO_STRING(pn->pn_atom2)
                  : cx->runtime->emptyString;
            xml_class = JSXML_CLASS_PROCESSING_INSTRUCTION;
        } else {
            /* CDATA section content, or element text. */
            xml_class = JSXML_CLASS_TEXT;
        }

        xml = js_NewXML(cx, xml_class);
        if (!xml) {
            if (qn)
                js_DestroyXMLQName(cx, qn);
            return NULL;
        }
        xml->name = qn;
        xml->flags = flags;
        xml->xml_value = str;
        break;

      default:
        goto syntax;
    }
    return xml;

syntax:
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_XML_MARKUP);
fail:
    if (xml)
        js_DestroyXML(cx, xml);
    return NULL;
}

JSObject *
js_NewXMLObject(JSContext *cx, JSXMLClass xml_class)
{
    JSXML *xml;
    JSObject *obj;

    xml = js_NewXML(cx, xml_class);
    if (!xml)
        return NULL;
    obj = js_GetXMLObject(cx, xml);
    if (!obj)
        js_DestroyXML(cx, xml);
    return obj;
}

JSObject *
js_GetXMLObject(JSContext *cx, JSXML *xml)
{
    JSObject *obj;

    obj = xml->object;
    if (obj) {
        JS_ASSERT(JS_GetPrivate(cx, obj) == xml);
        return obj;
    }

    /*
     * A JSXML cannot be shared among threads unless it has an object.
     * A JSXML cannot be given an object unless:
     * (a) it has no parent; or
     * (b) its parent has no object (therefore is thread-private); or
     * (c) its parent's object is locked.
     */
    JS_ASSERT(!xml->parent ||
              !xml->parent->object ||
              JS_IS_OBJ_LOCKED(cx, xml->parent->object));

    obj = js_NewObject(cx, &js_XMLClass, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, xml)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    xml->object = obj;
    return obj;
}

JSObject *
js_InitNamespaceClass(JSContext *cx, JSObject *obj)
{
    return JS_InitClass(cx, obj, NULL, &namespace_class, Namespace, 2,
                        namespace_props, namespace_methods, NULL, NULL);
}

JSObject *
js_InitQNameClass(JSContext *cx, JSObject *obj)
{
    return JS_InitClass(cx, obj, NULL, &qname_class, QName, 2,
                        qname_props, qname_methods, NULL, NULL);
}

JSObject *
js_InitXMLClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto, *pobj, *ctor;
    JSXML *xml;
    JSProperty *prop;
    JSScopeProperty *sprop;
    jsval cval, argv[1], junk;

    /* Define the isXMLName function. */
    if (!JS_DefineFunction(cx, obj, js_isXMLName_str, xml_isXMLName, 1, 0))
        return NULL;

    /* Define the XML class constructor and prototype. */
    proto = JS_InitClass(cx, obj, NULL, &js_XMLClass, XML, 1,
                         NULL, xml_methods,
                         xml_static_props, xml_static_methods);
    if (!proto)
        return NULL;
    xml = js_NewXML(cx, JSXML_CLASS_TEXT);
    if (!xml || !JS_SetPrivate(cx, proto, xml))
        return NULL;
    xml->object = proto;

    /*
     * Prepare to set default settings on the XML constructor we just made.
     * NB: We can't use JS_GetConstructor, because it calls OBJ_GET_PROPERTY,
     * which is xml_getProperty, which creates a new XMLList every time!  We
     * must instead call js_LookupProperty directly.
     */
    if (!js_LookupProperty(cx, proto,
                           ATOM_TO_JSID(cx->runtime->atomState.constructorAtom),
                           &pobj, &prop)) {
        return NULL;
    }
    JS_ASSERT(prop);
    sprop = (JSScopeProperty *) prop;
    JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(pobj)));
    cval = OBJ_GET_SLOT(cx, pobj, sprop->slot);
    OBJ_DROP_PROPERTY(cx, pobj, prop);
    JS_ASSERT(JSVAL_IS_FUNCTION(cx, cval));

    /* Set default settings. */
    ctor = JSVAL_TO_OBJECT(cval);
    argv[0] = JSVAL_VOID;
    if (!xml_setSettings(cx, ctor, 0, argv, &junk))
        return NULL;

    /* Define the XMLList function. */
    if (!JS_DefineFunction(cx, obj, js_XMLList_str, XMLList, 1, 0))
        return NULL;
    return proto;
}

JSObject *
js_InitXMLClasses(JSContext *cx, JSObject *obj)
{
    if (!js_InitNamespaceClass(cx, obj) || !js_InitQNameClass(cx, obj))
        return NULL;
    return js_InitXMLClass(cx, obj);
}

/*
 * Note the asymmetry between js_GetDefaultXMLNamespace and js_SetDefaultXML-
 * Namespace.  Get searches fp->scopeChain for JS_DEFAULT_XML_NAMESPACE_ID,
 * while Set sets JS_DEFAULT_XML_NAMESPACE_ID in fp->varobj (unless fp is a
 * lightweight function activation).  There's no requirement that fp->varobj
 * lie directly on fp->scopeChain, although it should be reachable using the
 * prototype chain from a scope object (cf. JSOPTION_VAROBJFIX in jsapi.h).
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
    JSStackFrame *fp;
    JSObject *nsobj, *obj, *tmp;
    jsval v;

    fp = cx->fp;
    nsobj = fp->xmlNamespace;
    if (nsobj) {
        *vp = OBJECT_TO_JSVAL(nsobj);
        return JS_TRUE;
    }

    obj = NULL;
    for (tmp = fp->scopeChain; tmp; tmp = OBJ_GET_PARENT(cx, obj)) {
        obj = tmp;
        if (!OBJ_GET_PROPERTY(cx, obj, JS_DEFAULT_XML_NAMESPACE_ID, &v))
            return JS_FALSE;
        if (!JSVAL_IS_PRIMITIVE(v)) {
            fp->xmlNamespace = JSVAL_TO_OBJECT(v);
            *vp = v;
            return JS_TRUE;
        }
    }

    nsobj = js_ConstructObject(cx, &namespace_class, NULL, obj, 0, NULL);
    if (!nsobj)
        return JS_FALSE;
    v = OBJECT_TO_JSVAL(nsobj);
    if (obj && !OBJ_SET_PROPERTY(cx, obj, JS_DEFAULT_XML_NAMESPACE_ID, &v))
        return JS_FALSE;
    fp->xmlNamespace = nsobj;
    *vp = v;
    return JS_TRUE;
}

JSBool
js_SetDefaultXMLNamespace(JSContext *cx, jsval v)
{
    jsval argv[2];
    JSObject *nsobj, *varobj;
    JSStackFrame *fp;

    argv[0] = STRING_TO_JSVAL(cx->runtime->emptyString);
    argv[1] = v;
    nsobj = js_ConstructObject(cx, &namespace_class, NULL, NULL, 2, argv);
    if (!nsobj)
        return JS_FALSE;
    v = OBJECT_TO_JSVAL(nsobj);

    fp = cx->fp;
    varobj = fp->varobj;
    if (varobj) {
        if (!OBJ_SET_PROPERTY(cx, varobj, JS_DEFAULT_XML_NAMESPACE_ID, &v))
            return JS_FALSE;
    } else {
        JS_ASSERT(fp->fun && !(fp->fun->flags & JSFUN_HEAVYWEIGHT));
    }
    fp->xmlNamespace = JSVAL_TO_OBJECT(v);
    return JS_TRUE;
}

JSBool
js_ToAttributeName(JSContext *cx, jsval *vp)
{
    JSXMLQName *qn;

    qn = ToAttributeName(cx, *vp);
    if (!qn)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(qn->object);
    return JS_TRUE;
}

JSString *
js_EscapeAttributeValue(JSContext *cx, JSString *str)
{
    return EscapeAttributeValue(cx, NULL, str);
}

JSString *
js_AddAttributePart(JSContext *cx, JSBool isName, JSString *str, JSString *str2)
{
    size_t len, len2, newlen;
    jschar *chars;

    JS_ASSERT(!JSSTRING_IS_DEPENDENT(str) &&
              (*js_GetGCThingFlags(str) & GCF_MUTABLE));
    len = str->length;
    len2 = JSSTRING_LENGTH(str2);
    newlen = (isName) ? len + 1 + len2 : len + 2 + len2 + 1;
    chars = (jschar *)JS_realloc(cx, str->chars, (newlen + 1) * sizeof(jschar));
    if (!chars)
        return NULL;
    str->chars = chars;
    str->length = newlen;
    chars += len;
    if (isName) {
        *chars++ = ' ';
        js_strncpy(chars, JSSTRING_CHARS(str2), len2);
        chars += len2;
    } else {
        *chars++ = '=';
        *chars++ = '"';
        js_strncpy(chars, JSSTRING_CHARS(str2), len2);
        chars += len2;
        *chars++ = '"';
    }
    *chars = 0;
    return str;
}

JSBool
js_GetAnyName(JSContext *cx, jsval *vp)
{
    JSRuntime *rt;
    JSAtom *atom;
    JSXMLQName *qn;
    JSObject *obj;

    rt = cx->runtime;
    atom = rt->atomState.lazy.anynameAtom;
    if (!atom) {
        qn = js_NewXMLQName(cx, rt->emptyString, rt->emptyString,
                            ATOM_TO_STRING(rt->atomState.starAtom));
        if (!qn)
            return JS_FALSE;

        obj = js_NewObject(cx, &any_name_class, NULL, NULL);
        if (!obj || !JS_SetPrivate(cx, obj, qn)) {
            cx->newborn[GCX_OBJECT] = NULL;
            return JS_FALSE;
        }
        qn->object = obj;

        atom = js_AtomizeObject(cx, obj, ATOM_PINNED);
        if (!atom)
            return JS_FALSE;
        rt->atomState.lazy.anynameAtom = atom;
    }
    *vp = ATOM_KEY(atom);
    return JS_TRUE;
}

JSBool
js_BindXMLProperty(JSContext *cx, jsval lval, jsval rval)
{
    JSString *str;
    uint32 index;

    if (!JSVAL_IS_PRIMITIVE(lval) &&
        OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(lval)) == &js_XMLClass) {
        /*
         * Text fairly buried in 11.6.1 says that a numeric property id used
         * with an XML object is reserved and should throw a TypeError.
         */
        if (!js_IdIsIndex(rval, &index))
            return JS_TRUE;
        lval = rval;
    }

    str = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, lval, NULL);
    if (str) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_IS_NOT_XML_OBJECT,
                             JS_GetStringBytes(str));
    }
    return JS_FALSE;
}

JSBool
js_FindXMLProperty(JSContext *cx, jsval name, JSObject **objp, jsval *namep)
{
    JSXMLQName *qn;
    jsid id;
    JSObject *obj, *pobj, *lastobj;
    JSProperty *prop;

    qn = ToXMLName(cx, name);
    if (!qn)
        return JS_FALSE;
    id = OBJECT_TO_JSID(qn->object);

    obj = cx->fp->scopeChain;
    do {
        if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop))
            return JS_FALSE;
        if (prop) {
            OBJ_DROP_PROPERTY(cx, pobj, prop);

            if (OBJECT_IS_XML(cx, pobj)) {
                *objp = pobj;
                *namep = ID_TO_VALUE(id);
                return JS_TRUE;
            }
        }

        lastobj = obj;
    } while ((obj = OBJ_GET_PARENT(cx, obj)) != NULL);

    *objp = lastobj;
    *namep = JSVAL_VOID;
    return JS_TRUE;
}

JSBool
js_GetXMLProperty(JSContext *cx, JSObject *obj, jsval name, jsval *vp)
{
    JSBool ok;

    JS_LOCK_OBJ_VOID(cx, obj, ok = GetProperty(cx, obj, name, vp));
    return ok;
}

JSBool
js_SetXMLProperty(JSContext *cx, JSObject *obj, jsval name, jsval *vp)
{
    JSBool ok;

    JS_LOCK_OBJ_VOID(cx, obj, ok = PutProperty(cx, obj, name, vp));
    return ok;
}

static JSXML *
GetPrivate(JSContext *cx, JSObject *obj, const char *method)
{
    JSXML *xml;

    xml = (JSXML *) JS_GetInstancePrivate(cx, obj, &js_XMLClass, NULL);
    if (!xml) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_METHOD,
                             js_XML_str, method, OBJ_GET_CLASS(cx, obj)->name);
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
js_FilterXMLList(JSContext *cx, JSObject *obj, jsbytecode *pc, uint32 len,
                 jsval *vp)
{
    JSBool ok, inLRS, match;
    JSStackFrame *fp, frame;
    JSObject *listobj, *resobj, *withobj, *kidobj;
    JSXML *xml, *list, *result, *kid;
    JSScript script;
    uint32 i, n;

    ok = JS_TRUE;
    inLRS = JS_FALSE;
    fp = cx->fp;
    frame = *fp;
    frame.down = fp;
    frame.flags |= JSFRAME_EVAL;
    cx->fp = &frame;

    xml = GetPrivate(cx, obj, "filtering predicate operator");
    if (!xml)
        goto bad;

    if (xml->xml_class == JSXML_CLASS_LIST) {
        list = xml;
    } else {
        ok = JS_EnterLocalRootScope(cx);
        if (!ok)
            goto out;
        inLRS = JS_TRUE;
        listobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
        if (!listobj)
            goto bad;
        list = (JSXML *) JS_GetPrivate(cx, listobj);
        ok = Append(cx, list, xml);
        if (!ok)
            goto out;
    }

    /* Root resobj via frame.rval, whether or not we are inLRS. */
    resobj = js_NewXMLObject(cx, JSXML_CLASS_LIST);
    if (!resobj)
        goto bad;
    result = (JSXML *) JS_GetPrivate(cx, resobj);
    frame.rval = OBJECT_TO_JSVAL(resobj);

    /* Hoist the scope chain update out of the loop over kids. */
    withobj = js_NewObject(cx, &js_WithClass, NULL, fp->scopeChain);
    if (!withobj)
        goto bad;
    frame.scopeChain = withobj;

    script = *fp->script;
    script.code = script.main = pc;
    script.length = len;
    frame.script = &script;

    for (i = 0, n = list->xml_kids.count; i < n; i++) {
        kid = XMLARRAY_MEMBER(&list->xml_kids, i, JSXML);
        kidobj = js_GetXMLObject(cx, kid);
        if (!kidobj)
            goto bad;
        OBJ_SET_PROTO(cx, withobj, kidobj);
        ok = js_Interpret(cx, vp);
        if (!ok)
            goto out;
        ok = js_ValueToBoolean(cx, *vp, &match);
        if (!ok)
            goto out;
        if (match) {
            ok = Append(cx, result, kid);
            if (!ok)
                goto out;
        }
    }

    *vp = OBJECT_TO_JSVAL(resobj);

out:
    if (inLRS)
        JS_LeaveLocalRootScope(cx);
    cx->fp = fp;
    return ok;
bad:
    ok = JS_FALSE;
    goto out;
}

JSObject *
js_ValueToXMLObject(JSContext *cx, jsval v)
{
    JSXML *xml;

    xml = ToXML(cx, v);
    if (!xml)
        return NULL;
    return js_GetXMLObject(cx, xml);
}

JSObject *
js_ValueToXMLListObject(JSContext *cx, jsval v)
{
    JSXML *xml;

    xml = ToXMLList(cx, v);
    if (!xml)
        return NULL;
    return js_GetXMLObject(cx, xml);
}

JSObject *
js_CloneXMLObject(JSContext *cx, JSObject *obj)
{
    JSObject *newobj;
    JSXML *xml;

    newobj = js_NewObject(cx, &js_XMLClass, NULL, NULL);
    xml = (JSXML *) JS_GetPrivate(cx, obj);
    if (!newobj || !JS_SetPrivate(cx, newobj, xml)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    JS_ATOMIC_INCREMENT(&xml->nrefs);
    return newobj;
}

#endif /* JS_HAS_XML_SUPPORT */
