/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is SpiderMonkey JSON.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Sayre <sayrer@gmail.com>
 *   Dave Camp <dcamp@mozilla.com>
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

#include <string.h>
#include "jsapi.h"
#include "jsarena.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsonparser.h"
#include "jsprf.h"
#include "jsscan.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsxml.h"
#include "jsvector.h"

#include "json.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsstrinlines.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

Class js::JSONClass = {
    js_JSON_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_JSON),
    PropertyStub,        /* addProperty */
    PropertyStub,        /* delProperty */
    PropertyStub,        /* getProperty */
    StrictPropertyStub,  /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

/* ES5 15.12.2. */
JSBool
js_json_parse(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    JSLinearString *linear;
    if (argc >= 1) {
        JSString *str = js_ValueToString(cx, vp[2]);
        if (!str)
            return false;
        linear = str->ensureLinear(cx);
        if (!linear)
            return false;
    } else {
        linear = cx->runtime->atomState.typeAtoms[JSTYPE_VOID];
    }
    JS::Anchor<JSString *> anchor(linear);

    Value reviver = (argc >= 2) ? vp[3] : UndefinedValue();

    /* Steps 2-5. */
    return ParseJSONWithReviver(cx, linear->chars(), linear->length(), reviver, vp);
}

/* ES5 15.12.3. */
JSBool
js_json_stringify(JSContext *cx, uintN argc, Value *vp)
{
    *vp = (argc >= 1) ? vp[2] : UndefinedValue();
    JSObject *replacer = (argc >= 2 && vp[3].isObject())
                         ? &vp[3].toObject()
                         : NULL;
    Value space = (argc >= 3) ? vp[4] : UndefinedValue();

    StringBuffer sb(cx);
    if (!js_Stringify(cx, vp, replacer, space, sb))
        return false;

    // XXX This can never happen to nsJSON.cpp, but the JSON object
    // needs to support returning undefined. So this is a little awkward
    // for the API, because we want to support streaming writers.
    if (!sb.empty()) {
        JSString *str = sb.finishString();
        if (!str)
            return false;
        vp->setString(str);
    } else {
        vp->setUndefined();
    }

    return true;
}

static inline bool IsQuoteSpecialCharacter(jschar c)
{
    JS_STATIC_ASSERT('\b' < ' ');
    JS_STATIC_ASSERT('\f' < ' ');
    JS_STATIC_ASSERT('\n' < ' ');
    JS_STATIC_ASSERT('\r' < ' ');
    JS_STATIC_ASSERT('\t' < ' ');
    return c == '"' || c == '\\' || c < ' ';
}

/* ES5 15.12.3 Quote. */
static bool
Quote(JSContext *cx, StringBuffer &sb, JSString *str)
{
    JS::Anchor<JSString *> anchor(str);
    size_t len = str->length();
    const jschar *buf = str->getChars(cx);
    if (!buf)
        return false;

    /* Step 1. */
    if (!sb.append('"'))
        return false;

    /* Step 2. */
    for (size_t i = 0; i < len; ++i) {
        /* Batch-append maximal character sequences containing no escapes. */
        size_t mark = i;
        do {
            if (IsQuoteSpecialCharacter(buf[i]))
                break;
        } while (++i < len);
        if (i > mark) {
            if (!sb.append(&buf[mark], i - mark))
                return false;
            if (i == len)
                break;
        }

        jschar c = buf[i];
        if (c == '"' || c == '\\') {
            if (!sb.append('\\') || !sb.append(c))
                return false;
        } else if (c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
           jschar abbrev = (c == '\b')
                         ? 'b'
                         : (c == '\f')
                         ? 'f'
                         : (c == '\n')
                         ? 'n'
                         : (c == '\r')
                         ? 'r'
                         : 't';
           if (!sb.append('\\') || !sb.append(abbrev))
               return false;
           mark = i + 1;
        } else {
            JS_ASSERT(c < ' ');
            if (!sb.append("\\u00"))
                return false;
            JS_ASSERT((c >> 4) < 10);
            uint8 x = c >> 4, y = c % 16;
            if (!sb.append('0' + x) || !sb.append(y < 10 ? '0' + y : 'a' + (y - 10)))
                return false;
        }
    }

    /* Steps 3-4. */
    return sb.append('"');
}

class StringifyContext
{
  public:
    StringifyContext(JSContext *cx, StringBuffer &sb, const StringBuffer &gap,
                     JSObject *replacer, const AutoIdVector &propertyList)
      : sb(sb),
        gap(gap),
        replacer(replacer),
        propertyList(propertyList),
        depth(0),
        objectStack(cx)
    {}

    bool init() {
        return objectStack.init(16);
    }

#ifdef DEBUG
    ~StringifyContext() { JS_ASSERT(objectStack.empty()); }
#endif

    StringBuffer &sb;
    const StringBuffer &gap;
    JSObject * const replacer;
    const AutoIdVector &propertyList;
    uint32 depth;
    HashSet<JSObject *> objectStack;
};

static JSBool Str(JSContext *cx, const Value &v, StringifyContext *scx);

static JSBool
WriteIndent(JSContext *cx, StringifyContext *scx, uint32 limit)
{
    if (!scx->gap.empty()) {
        if (!scx->sb.append('\n'))
            return JS_FALSE;
        for (uint32 i = 0; i < limit; i++) {
            if (!scx->sb.append(scx->gap.begin(), scx->gap.end()))
                return JS_FALSE;
        }
    }

    return JS_TRUE;
}

class CycleDetector
{
  public:
    CycleDetector(StringifyContext *scx, JSObject *obj)
      : objectStack(scx->objectStack), obj(obj) {
    }

    bool init(JSContext *cx) {
        HashSet<JSObject *>::AddPtr ptr = objectStack.lookupForAdd(obj);
        if (ptr) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CYCLIC_VALUE, js_object_str);
            return false;
        }
        return objectStack.add(ptr, obj);
    }

    ~CycleDetector() {
        objectStack.remove(obj);
    }

  private:
    HashSet<JSObject *> &objectStack;
    JSObject *const obj;
};

/*
 * ES5 15.12.3 Str, steps 2-4, extracted to enable preprocessing of property
 * values when stringifying objects in JO.
 */
static bool
PreprocessValue(JSContext *cx, JSObject *holder, jsid key, Value *vp, StringifyContext *scx)
{
    JSString *keyStr = NULL;

    /* Step 2. */
    if (vp->isObject()) {
        Value toJSON;
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.toJSONAtom);
        if (!js_GetMethod(cx, &vp->toObject(), id, JSGET_NO_METHOD_BARRIER, &toJSON))
            return false;

        if (js_IsCallable(toJSON)) {
            keyStr = IdToString(cx, key);
            if (!keyStr)
                return false;

            LeaveTrace(cx);
            InvokeArgsGuard args;
            if (!cx->stack.pushInvokeArgs(cx, 1, &args))
                return false;

            args.calleev() = toJSON;
            args.thisv() = *vp;
            args[0] = StringValue(keyStr);

            if (!Invoke(cx, args))
                return false;
            *vp = args.rval();
        }
    }

    /* Step 3. */
    if (scx->replacer && scx->replacer->isCallable()) {
        if (!keyStr) {
            keyStr = IdToString(cx, key);
            if (!keyStr)
                return false;
        }

        LeaveTrace(cx);
        InvokeArgsGuard args;
        if (!cx->stack.pushInvokeArgs(cx, 2, &args))
            return false;

        args.calleev() = ObjectValue(*scx->replacer);
        args.thisv() = ObjectValue(*holder);
        args[0] = StringValue(keyStr);
        args[1] = *vp;

        if (!Invoke(cx, args))
            return false;
        *vp = args.rval();
    }

    /* Step 4. */
    if (vp->isObject()) {
        JSObject *obj = &vp->toObject();
        Class *clasp = obj->getClass();
        if (clasp == &NumberClass) {
            double d;
            if (!ToNumber(cx, *vp, &d))
                return false;
            vp->setNumber(d);
        } else if (clasp == &StringClass) {
            JSString *str = js_ValueToString(cx, *vp);
            if (!str)
                return false;
            vp->setString(str);
        } else if (clasp == &BooleanClass) {
            *vp = obj->getPrimitiveThis();
            JS_ASSERT(vp->isBoolean());
        }
    }

    return true;
}

/*
 * Determines whether a value which has passed by ES5 150.2.3 Str steps 1-4's
 * gauntlet will result in Str returning |undefined|.  This function is used to
 * properly omit properties resulting in such values when stringifying objects,
 * while properly stringifying such properties as null when they're encountered
 * in arrays.
 */
static inline bool
IsFilteredValue(const Value &v)
{
    return v.isUndefined() || js_IsCallable(v) || (v.isObject() && v.toObject().isXML());
}

/* ES5 15.12.3 JO. */
static JSBool
JO(JSContext *cx, JSObject *obj, StringifyContext *scx)
{
    /*
     * This method implements the JO algorithm in ES5 15.12.3, but:
     *
     *   * The algorithm is somewhat reformulated to allow the final string to
     *     be streamed into a single buffer, rather than be created and copied
     *     into place incrementally as the ES5 algorithm specifies it.  This
     *     requires moving portions of the Str call in 8a into this algorithm
     *     (and in JA as well).
     */

    /* Steps 1-2, 11. */
    CycleDetector detect(scx, obj);
    if (!detect.init(cx))
        return JS_FALSE;

    if (!scx->sb.append('{'))
        return JS_FALSE;

    /* Steps 5-7. */
    Maybe<AutoIdVector> ids;
    const AutoIdVector *props;
    if (scx->replacer && !scx->replacer->isCallable()) {
        JS_ASSERT(JS_IsArrayObject(cx, scx->replacer));
        props = &scx->propertyList;
    } else {
        JS_ASSERT_IF(scx->replacer, scx->propertyList.length() == 0);
        ids.construct(cx);
        if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, ids.addr()))
            return false;
        props = ids.addr();
    }

    /* My kingdom for not-quite-initialized-from-the-start references. */
    const AutoIdVector &propertyList = *props;

    /* Steps 8-10, 13. */
    bool wroteMember = false;
    for (size_t i = 0, len = propertyList.length(); i < len; i++) {
        /*
         * Steps 8a-8b.  Note that the call to Str is broken up into 1) getting
         * the property; 2) processing for toJSON, calling the replacer, and
         * handling boxed Number/String/Boolean objects; 3) filtering out
         * values which process to |undefined|, and 4) stringifying all values
         * which pass the filter.
         */
        const jsid &id = propertyList[i];
        Value outputValue;
        if (!obj->getProperty(cx, id, &outputValue))
            return false;
        if (!PreprocessValue(cx, obj, id, &outputValue, scx))
            return false;
        if (IsFilteredValue(outputValue))
            continue;

        /* Output a comma unless this is the first member to write. */
        if (wroteMember && !scx->sb.append(','))
            return false;
        wroteMember = true;

        if (!WriteIndent(cx, scx, scx->depth))
            return false;

        JSString *s = IdToString(cx, id);
        if (!s)
            return false;

        if (!Quote(cx, scx->sb, s) ||
            !scx->sb.append(':') ||
            !(scx->gap.empty() || scx->sb.append(' ')) ||
            !Str(cx, outputValue, scx))
        {
            return false;
        }
    }

    if (wroteMember && !WriteIndent(cx, scx, scx->depth - 1))
        return false;

    return scx->sb.append('}');
}

/* ES5 15.12.3 JA. */
static JSBool
JA(JSContext *cx, JSObject *obj, StringifyContext *scx)
{
    /*
     * This method implements the JA algorithm in ES5 15.12.3, but:
     *
     *   * The algorithm is somewhat reformulated to allow the final string to
     *     be streamed into a single buffer, rather than be created and copied
     *     into place incrementally as the ES5 algorithm specifies it.  This
     *     requires moving portions of the Str call in 8a into this algorithm
     *     (and in JO as well).
     */

    /* Steps 1-2, 11. */
    CycleDetector detect(scx, obj);
    if (!detect.init(cx))
        return JS_FALSE;

    if (!scx->sb.append('['))
        return JS_FALSE;

    /* Step 6. */
    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /* Steps 7-10. */
    if (length != 0) {
        /* Steps 4, 10b(i). */
        if (!WriteIndent(cx, scx, scx->depth))
            return JS_FALSE;

        /* Steps 7-10. */
        Value outputValue;
        for (jsuint i = 0; i < length; i++) {
            jsid id = INT_TO_JSID(i);

            /*
             * Steps 8a-8c.  Again note how the call to the spec's Str method
             * is broken up into getting the property, running it past toJSON
             * and the replacer and maybe unboxing, and interpreting some
             * values as |null| in separate steps.
             */
            if (!obj->getProperty(cx, id, &outputValue))
                return JS_FALSE;
            if (!PreprocessValue(cx, obj, id, &outputValue, scx))
                return JS_FALSE;
            if (IsFilteredValue(outputValue)) {
                if (!scx->sb.append("null"))
                    return JS_FALSE;
            } else {
                if (!Str(cx, outputValue, scx))
                    return JS_FALSE;
            }

            /* Steps 3, 4, 10b(i). */
            if (i < length - 1) {
                if (!scx->sb.append(','))
                    return JS_FALSE;
                if (!WriteIndent(cx, scx, scx->depth))
                    return JS_FALSE;
            }
        }

        /* Step 10(b)(iii). */
        if (!WriteIndent(cx, scx, scx->depth - 1))
            return JS_FALSE;
    }

    return scx->sb.append(']');
}

static JSBool
Str(JSContext *cx, const Value &v, StringifyContext *scx)
{
    /* Step 11 must be handled by the caller. */
    JS_ASSERT(!IsFilteredValue(v));

    JS_CHECK_RECURSION(cx, return false);

    /*
     * This method implements the Str algorithm in ES5 15.12.3, but:
     *
     *   * We move property retrieval (step 1) into callers to stream the
     *     stringification process and avoid constantly copying strings.
     *   * We move the preprocessing in steps 2-4 into a helper function to
     *     allow both JO and JA to use this method.  While JA could use it
     *     without this move, JO must omit any |undefined|-valued property per
     *     so it can't stream out a value using the Str method exactly as
     *     defined by ES5.
     *   * We move step 11 into callers, again to ease streaming.
     */

    /* Step 8. */
    if (v.isString())
        return Quote(cx, scx->sb, v.toString());

    /* Step 5. */
    if (v.isNull())
        return scx->sb.append("null");

    /* Steps 6-7. */
    if (v.isBoolean())
        return v.toBoolean() ? scx->sb.append("true") : scx->sb.append("false");

    /* Step 9. */
    if (v.isNumber()) {
        if (v.isDouble()) {
            if (!JSDOUBLE_IS_FINITE(v.toDouble()))
                return scx->sb.append("null");
        }

        StringBuffer sb(cx);
        if (!NumberValueToStringBuffer(cx, v, sb))
            return false;

        return scx->sb.append(sb.begin(), sb.length());
    }

    /* Step 10. */
    JS_ASSERT(v.isObject());
    JSBool ok;

    scx->depth++;
    ok = (JS_IsArrayObject(cx, &v.toObject()) ? JA : JO)(cx, &v.toObject(), scx);
    scx->depth--;

    return ok;
}

/* ES5 15.12.3. */
JSBool
js_Stringify(JSContext *cx, Value *vp, JSObject *replacer, Value space, StringBuffer &sb)
{
    /* Step 4. */
    AutoIdVector propertyList(cx);
    if (replacer) {
        if (replacer->isCallable()) {
            /* Step 4a(i): use replacer to transform values.  */
        } else if (JS_IsArrayObject(cx, replacer)) {
            /*
             * Step 4b: The spec algorithm is unhelpfully vague about the exact
             * steps taken when the replacer is an array, regarding the exact
             * sequence of [[Get]] calls for the array's elements, when its
             * overall length is calculated, whether own or own plus inherited
             * properties are considered, and so on.  A rewrite was proposed in
             * <https://mail.mozilla.org/pipermail/es5-discuss/2011-April/003976.html>,
             * whose steps are copied below, and which are implemented here.
             *
             * i.   Let PropertyList be an empty internal List.
             * ii.  Let len be the result of calling the [[Get]] internal
             *      method of replacer with the argument "length".
             * iii. Let i be 0.
             * iv.  While i < len:
             *      1. Let item be undefined.
             *      2. Let v be the result of calling the [[Get]] internal
             *         method of replacer with the argument ToString(i).
             *      3. If Type(v) is String then let item be v.
             *      4. Else if Type(v) is Number then let item be ToString(v).
             *      5. Else if Type(v) is Object then
             *         a. If the [[Class]] internal property of v is "String"
             *            or "Number" then let item be ToString(v).
             *      6. If item is not undefined and item is not currently an
             *         element of PropertyList then,
             *         a. Append item to the end of PropertyList.
             *      7. Let i be i + 1.
             */

            /* Step 4b(ii). */
            jsuint len;
            JS_ALWAYS_TRUE(js_GetLengthProperty(cx, replacer, &len));
            if (replacer->isDenseArray())
                len = JS_MIN(len, replacer->getDenseArrayCapacity());

            HashSet<jsid> idSet(cx);
            if (!idSet.init(len))
                return false;

            /* Step 4b(iii). */
            jsuint i = 0;

            /* Step 4b(iv). */
            for (; i < len; i++) {
                /* Step 4b(iv)(2). */
                Value v;
                if (!replacer->getProperty(cx, INT_TO_JSID(i), &v))
                    return false;

                jsid id;
                if (v.isNumber()) {
                    /* Step 4b(iv)(4). */
                    int32_t n;
                    if (v.isNumber() && ValueFitsInInt32(v, &n) && INT_FITS_IN_JSID(n)) {
                        id = INT_TO_JSID(n);
                    } else {
                        if (!js_ValueToStringId(cx, v, &id))
                            return false;
                        id = js_CheckForStringIndex(id);
                    }
                } else if (v.isString() ||
                           (v.isObject() && (v.toObject().isString() || v.toObject().isNumber())))
                {
                    /* Step 4b(iv)(3), 4b(iv)(5). */
                    if (!js_ValueToStringId(cx, v, &id))
                        return false;
                    id = js_CheckForStringIndex(id);
                } else {
                    continue;
                }

                /* Step 4b(iv)(6). */
                HashSet<jsid>::AddPtr p = idSet.lookupForAdd(id);
                if (!p) {
                    /* Step 4b(iv)(6)(a). */
                    if (!idSet.add(p, id) || !propertyList.append(id))
                        return false;
                }
            }
        } else {
            replacer = NULL;
        }
    }

    /* Step 5. */
    if (space.isObject()) {
        JSObject &spaceObj = space.toObject();
        if (spaceObj.isNumber()) {
            jsdouble d;
            if (!ToNumber(cx, space, &d))
                return false;
            space = NumberValue(d);
        } else if (spaceObj.isString()) {
            JSString *str = js_ValueToString(cx, space);
            if (!str)
                return false;
            space = StringValue(str);
        }
    }

    StringBuffer gap(cx);

    if (space.isNumber()) {
        /* Step 6. */
        jsdouble d;
        JS_ALWAYS_TRUE(ToInteger(cx, space, &d));
        d = JS_MIN(10, d);
        if (d >= 1 && !gap.appendN(' ', uint32(d)))
            return false;
    } else if (space.isString()) {
        /* Step 7. */
        JSLinearString *str = space.toString()->ensureLinear(cx);
        if (!str)
            return false;
        JS::Anchor<JSString *> anchor(str);
        size_t len = JS_MIN(10, space.toString()->length());
        if (!gap.append(str->chars(), len))
            return false;
    } else {
        /* Step 8. */
        JS_ASSERT(gap.empty());
    }

    /* Step 9. */
    JSObject *wrapper = NewBuiltinClassInstance(cx, &ObjectClass);
    if (!wrapper)
        return false;

    /* Step 10. */
    jsid emptyId = ATOM_TO_JSID(cx->runtime->atomState.emptyAtom);
    if (!DefineNativeProperty(cx, wrapper, emptyId, *vp, PropertyStub, StrictPropertyStub,
                              JSPROP_ENUMERATE, 0, 0))
    {
        return false;
    }

    /* Step 11. */
    StringifyContext scx(cx, sb, gap, replacer, propertyList);
    if (!scx.init())
        return false;

    if (!PreprocessValue(cx, wrapper, emptyId, vp, &scx))
        return false;
    if (IsFilteredValue(*vp))
        return true;

    return Str(cx, *vp, &scx);
}

/* ES5 15.12.2 Walk. */
static bool
Walk(JSContext *cx, JSObject *holder, jsid name, const Value &reviver, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    /* Step 1. */
    Value val;
    if (!holder->getProperty(cx, name, &val))
        return false;

    /* Step 2. */
    if (val.isObject()) {
        JSObject *obj = &val.toObject();

        if (obj->isArray()) {
            /* Step 2a(ii). */
            jsuint length = obj->getArrayLength();

            /* Step 2a(i), 2a(iii-iv). */
            for (jsuint i = 0; i < length; i++) {
                jsid id;
                if (!IndexToId(cx, i, &id))
                    return false;

                /* Step 2a(iii)(1). */
                Value newElement;
                if (!Walk(cx, obj, id, reviver, &newElement))
                    return false;

                /*
                 * Arrays which begin empty and whose properties are always
                 * incrementally appended are always dense, no matter their
                 * length, under current dense/slow array heuristics.
                 * Also, deleting a property from a dense array which is not
                 * currently being enumerated never makes it slow.  This array
                 * is never exposed until the reviver sees it below, so it must
                 * be dense and isn't currently being enumerated.  Therefore
                 * property definition and deletion will always succeed,
                 * and we need not check for failure.
                 */
                if (newElement.isUndefined()) {
                    /* Step 2a(iii)(2). */
                    JS_ALWAYS_TRUE(array_deleteProperty(cx, obj, id, &newElement, false));
                } else {
                    /* Step 2a(iii)(3). */
                    JS_ALWAYS_TRUE(array_defineProperty(cx, obj, id, &newElement, PropertyStub,
                                                        StrictPropertyStub, JSPROP_ENUMERATE));
                }
            }
        } else {
            /* Step 2b(i). */
            AutoIdVector keys(cx);
            if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &keys))
                return false;

            /* Step 2b(ii). */
            for (size_t i = 0, len = keys.length(); i < len; i++) {
                /* Step 2b(ii)(1). */
                Value newElement;
                jsid id = keys[i];
                if (!Walk(cx, obj, id, reviver, &newElement))
                    return false;

                if (newElement.isUndefined()) {
                    /* Step 2b(ii)(2). */
                    if (!js_DeleteProperty(cx, obj, id, &newElement, false))
                        return false;
                } else {
                    /* Step 2b(ii)(3). */
                    JS_ASSERT(obj->isNative());
                    if (!DefineNativeProperty(cx, obj, id, newElement, PropertyStub,
                                              StrictPropertyStub, JSPROP_ENUMERATE, 0, 0))
                    {
                        return false;
                    }
                }
            }
        }
    }

    /* Step 3. */
    JSString *key = IdToString(cx, name);
    if (!key)
        return false;

    LeaveTrace(cx);
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, 2, &args))
        return false;

    args.calleev() = reviver;
    args.thisv() = ObjectValue(*holder);
    args[0] = StringValue(key);
    args[1] = val;

    if (!Invoke(cx, args))
        return false;
    *vp = args.rval();
    return true;
}

static bool
Revive(JSContext *cx, const Value &reviver, Value *vp)
{

    JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass);
    if (!obj)
        return false;

    AutoObjectRooter tvr(cx, obj);
    if (!obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.emptyAtom),
                             *vp, NULL, NULL, JSPROP_ENUMERATE)) {
        return false;
    }

    return Walk(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.emptyAtom), reviver, vp);
}

namespace js {

JSBool
ParseJSONWithReviver(JSContext *cx, const jschar *chars, size_t length, const Value &reviver,
                     Value *vp, DecodingMode decodingMode /* = STRICT */)
{
    /* 15.12.2 steps 2-3. */
    JSONParser parser(cx, chars, length,
                      decodingMode == STRICT ? JSONParser::StrictJSON : JSONParser::LegacyJSON);
    if (!parser.parse(vp))
        return false;

    /* 15.12.2 steps 4-5. */
    if (js_IsCallable(reviver))
        return Revive(cx, reviver, vp);
    return true;
}

} /* namespace js */

#if JS_HAS_TOSOURCE
static JSBool
json_toSource(JSContext *cx, uintN argc, Value *vp)
{
    vp->setString(CLASS_ATOM(cx, JSON));
    return JS_TRUE;
}
#endif

static JSFunctionSpec json_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  json_toSource,      0, 0),
#endif
    JS_FN("parse",          js_json_parse,      2, 0),
    JS_FN("stringify",      js_json_stringify,  3, 0),
    JS_FS_END
};

JSObject *
js_InitJSONClass(JSContext *cx, JSObject *obj)
{
    JSObject *JSON = NewNonFunction<WithProto::Class>(cx, &JSONClass, NULL, obj);
    if (!JSON || !JSON->setSingletonType(cx))
        return NULL;

    if (!JS_DefineProperty(cx, obj, js_JSON_str, OBJECT_TO_JSVAL(JSON),
                           JS_PropertyStub, JS_StrictPropertyStub, 0))
        return NULL;

    if (!JS_DefineFunctions(cx, JSON, json_static_methods))
        return NULL;

    MarkStandardClassInitializedNoProto(obj, &JSONClass);

    return JSON;
}
