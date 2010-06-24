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
#include "jsdtoa.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsobj.h"
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
#include "jsobjinlines.h"

using namespace js;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4351)
#endif

struct JSONParser
{
    JSONParser(JSContext *cx)
     : hexChar(), numHex(), statep(), stateStack(), rootVal(), objectStack(),
       objectKey(cx), buffer(cx)
    {}

    /* Used while handling \uNNNN in strings */
    jschar hexChar;
    uint8 numHex;

    JSONParserState *statep;
    JSONParserState stateStack[JSON_MAX_DEPTH];
    jsval *rootVal;
    JSObject *objectStack;
    js::Vector<jschar, 8> objectKey;
    js::Vector<jschar, 8> buffer;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

JSClass js_JSONClass = {
    js_JSON_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_JSON),
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool
js_json_parse(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *s = NULL;
    jsval *argv = vp + 2;
    AutoValueRooter reviver(cx, JSVAL_NULL);

    if (!JS_ConvertArguments(cx, argc, argv, "S / v", &s, reviver.addr()))
        return JS_FALSE;

    JSONParser *jp = js_BeginJSONParse(cx, vp);
    JSBool ok = jp != NULL;
    if (ok) {
        const jschar *chars;
        size_t length;
        s->getCharsAndLength(chars, length);
        ok = js_ConsumeJSONText(cx, jp, chars, length);
        ok &= !!js_FinishJSONParse(cx, jp, reviver.value());
    }

    return ok;
}

JSBool
js_json_stringify(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = vp + 2;
    AutoValueRooter space(cx, JSVAL_NULL);
    AutoObjectRooter replacer(cx);

    // Must throw an Error if there isn't a first arg
    if (!JS_ConvertArguments(cx, argc, argv, "v / o v", vp, replacer.addr(), space.addr()))
        return JS_FALSE;

    JSCharBuffer cb(cx);

    if (!js_Stringify(cx, vp, replacer.object(), space.value(), cb))
        return JS_FALSE;

    // XXX This can never happen to nsJSON.cpp, but the JSON object
    // needs to support returning undefined. So this is a little awkward
    // for the API, because we want to support streaming writers.
    if (!cb.empty()) {
        JSString *str = js_NewStringFromCharBuffer(cx, cb);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
    } else {
        *vp = JSVAL_VOID;
    }

    return JS_TRUE;
}

JSBool
js_TryJSON(JSContext *cx, jsval *vp)
{
    // Checks whether the return value implements toJSON()
    JSBool ok = JS_TRUE;

    if (!JSVAL_IS_PRIMITIVE(*vp)) {
        JSObject *obj = JSVAL_TO_OBJECT(*vp);
        ok = js_TryMethod(cx, obj, cx->runtime->atomState.toJSONAtom, 0, NULL, vp);
    }

    return ok;
}


static const char quote = '\"';
static const char backslash = '\\';
static const char unicodeEscape[] = "\\u00";

static JSBool
write_string(JSContext *cx, JSCharBuffer &cb, const jschar *buf, uint32 len)
{
    if (!cb.append(quote))
        return JS_FALSE;

    uint32 mark = 0;
    uint32 i;
    for (i = 0; i < len; ++i) {
        if (buf[i] == quote || buf[i] == backslash) {
            if (!cb.append(&buf[mark], i - mark) || !cb.append(backslash) ||
                !cb.append(buf[i])) {
                return JS_FALSE;
            }
            mark = i + 1;
        } else if (buf[i] <= 31 || buf[i] == 127) {
            if (!cb.append(&buf[mark], i - mark) ||
                !js_AppendLiteral(cb, unicodeEscape)) {
                return JS_FALSE;
            }
            char ubuf[3];
            size_t len = JS_snprintf(ubuf, sizeof(ubuf), "%.2x", buf[i]);
            JS_ASSERT(len == 2);
            jschar wbuf[3];
            size_t wbufSize = JS_ARRAY_LENGTH(wbuf);
            if (!js_InflateStringToBuffer(cx, ubuf, len, wbuf, &wbufSize) ||
                !cb.append(wbuf, wbufSize)) {
                return JS_FALSE;
            }
            mark = i + 1;
        }
    }

    if (mark < len && !cb.append(&buf[mark], len - mark))
        return JS_FALSE;

    return cb.append(quote);
}

class StringifyContext
{
public:
    StringifyContext(JSContext *cx, JSCharBuffer &cb, JSObject *replacer)
    : cb(cb), gap(cx), replacer(replacer), depth(0)
    {}

    JSCharBuffer &cb;
    JSCharBuffer gap;
    JSObject *replacer;
    uint32 depth;
};

static JSBool CallReplacerFunction(JSContext *cx, jsid id, JSObject *holder,
                                   StringifyContext *scx, jsval *vp);
static JSBool Str(JSContext *cx, jsid id, JSObject *holder,
                  StringifyContext *scx, jsval *vp, bool callReplacer = true);

static JSBool
WriteIndent(JSContext *cx, StringifyContext *scx, uint32 limit)
{
    if (!scx->gap.empty()) {
        if (!scx->cb.append('\n'))
            return JS_FALSE;
        for (uint32 i = 0; i < limit; i++) {
            if (!scx->cb.append(scx->gap.begin(), scx->gap.end()))
                return JS_FALSE;
        }
    }

    return JS_TRUE;
}

static JSBool
JO(JSContext *cx, jsval *vp, StringifyContext *scx)
{
    JSObject *obj = JSVAL_TO_OBJECT(*vp);

    if (!scx->cb.append('{'))
        return JS_FALSE;

    jsval vec[4] = {JSVAL_NULL, JSVAL_NULL, JSVAL_NULL, JSVAL_NULL};
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(vec), vec);
    jsval& outputValue = vec[0];
    jsval& whitelistElement = vec[1];
    jsid& id = vec[2];

    jsval *keySource = vp;
    bool usingWhitelist = false;

    // if the replacer is an array, we use the keys from it
    if (scx->replacer && JS_IsArrayObject(cx, scx->replacer)) {
        usingWhitelist = true;
        vec[3] = OBJECT_TO_JSVAL(scx->replacer);
        keySource = &vec[3];
    }

    JSBool memberWritten = JS_FALSE;
    AutoIdArray ida(cx, JS_Enumerate(cx, JSVAL_TO_OBJECT(*keySource)));
    if (!ida)
        return JS_FALSE;

    for (jsint i = 0, len = ida.length(); i < len; i++) {
        outputValue = JSVAL_VOID;

        if (!usingWhitelist) {
            if (!js_ValueToStringId(cx, ida[i], &id))
                return JS_FALSE;
        } else {
            // skip non-index properties
            jsuint index = 0;
            if (!js_IdIsIndex(ID_TO_VALUE(ida[i]), &index))
                continue;

            if (!scx->replacer->getProperty(cx, ID_TO_VALUE(ida[i]), &whitelistElement))
                return JS_FALSE;

            if (!js_ValueToStringId(cx, whitelistElement, &id))
                return JS_FALSE;
        }

        // We should have a string id by this point. Either from 
        // JS_Enumerate's id array, or by converting an element
        // of the whitelist.
        JS_ASSERT(JSVAL_IS_STRING(ID_TO_VALUE(id)));

        if (!JS_GetPropertyById(cx, obj, id, &outputValue))
            return JS_FALSE;

        if (JSVAL_IS_OBJECT(outputValue) && !js_TryJSON(cx, &outputValue))
            return JS_FALSE;

        // call this here, so we don't write out keys if the replacer function
        // wants to elide the value.
        if (!CallReplacerFunction(cx, id, obj, scx, &outputValue))
            return JS_FALSE;

        JSType type = JS_TypeOfValue(cx, outputValue);

        // elide undefined values and functions and XML
        if (outputValue == JSVAL_VOID || type == JSTYPE_FUNCTION || type == JSTYPE_XML)
            continue;

        // output a comma unless this is the first member to write
        if (memberWritten && !scx->cb.append(','))
            return JS_FALSE;
        memberWritten = JS_TRUE;

        if (!WriteIndent(cx, scx, scx->depth))
            return JS_FALSE;

        // Be careful below, this string is weakly rooted
        JSString *s = js_ValueToString(cx, ID_TO_VALUE(id));
        if (!s)
            return JS_FALSE;

        const jschar *chars;
        size_t length;
        s->getCharsAndLength(chars, length);
        if (!write_string(cx, scx->cb, chars, length) ||
            !scx->cb.append(':') ||
            !Str(cx, id, obj, scx, &outputValue, true)) {
            return JS_FALSE;
        }
    }

    if (memberWritten && !WriteIndent(cx, scx, scx->depth - 1))
        return JS_FALSE;

    return scx->cb.append('}');
}

static JSBool
JA(JSContext *cx, jsval *vp, StringifyContext *scx)
{
    JSObject *obj = JSVAL_TO_OBJECT(*vp);

    if (!scx->cb.append('['))
        return JS_FALSE;

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    if (length != 0 && !WriteIndent(cx, scx, scx->depth))
        return JS_FALSE;

    AutoValueRooter outputValue(cx, JSVAL_NULL);

    jsid id;
    jsuint i;
    for (i = 0; i < length; i++) {
        id = INT_TO_JSID(i);

        if (!obj->getProperty(cx, id, outputValue.addr()))
            return JS_FALSE;

        if (!Str(cx, id, obj, scx, outputValue.addr()))
            return JS_FALSE;

        if (outputValue.value() == JSVAL_VOID) {
            if (!js_AppendLiteral(scx->cb, "null"))
                return JS_FALSE;
        }

        if (i < length - 1) {
            if (!scx->cb.append(','))
                return JS_FALSE;
            if (!WriteIndent(cx, scx, scx->depth))
                return JS_FALSE;
        }
    }

    if (length != 0 && !WriteIndent(cx, scx, scx->depth - 1))
        return JS_FALSE;

    return scx->cb.append(']');
}

static JSBool
CallReplacerFunction(JSContext *cx, jsid id, JSObject *holder, StringifyContext *scx, jsval *vp)
{
    if (scx->replacer && scx->replacer->isCallable()) {
        jsval vec[2] = {ID_TO_VALUE(id), *vp};
        if (!JS_CallFunctionValue(cx, holder, OBJECT_TO_JSVAL(scx->replacer), 2, vec, vp))
            return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
Str(JSContext *cx, jsid id, JSObject *holder, StringifyContext *scx, jsval *vp, bool callReplacer)
{
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    if (!JSVAL_IS_PRIMITIVE(*vp) && !js_TryJSON(cx, vp))
        return JS_FALSE;

    if (callReplacer && !CallReplacerFunction(cx, id, holder, scx, vp))
        return JS_FALSE;

    // catches string and number objects with no toJSON
    if (!JSVAL_IS_PRIMITIVE(*vp)) {
        JSClass *clasp = JSVAL_TO_OBJECT(*vp)->getClass();
        if (clasp == &js_StringClass || clasp == &js_NumberClass)
            *vp = JSVAL_TO_OBJECT(*vp)->getPrimitiveThis();
    }

    if (JSVAL_IS_STRING(*vp)) {
        const jschar *chars;
        size_t length;
        JSVAL_TO_STRING(*vp)->getCharsAndLength(chars, length);
        return write_string(cx, scx->cb, chars, length);
    }

    if (JSVAL_IS_NULL(*vp)) {
        return js_AppendLiteral(scx->cb, "null");
    }

    if (JSVAL_IS_BOOLEAN(*vp)) {
        return JSVAL_TO_BOOLEAN(*vp) ? js_AppendLiteral(scx->cb, "true")
                                     : js_AppendLiteral(scx->cb, "false");
    }

    if (JSVAL_IS_NUMBER(*vp)) {
        if (JSVAL_IS_DOUBLE(*vp)) {
            jsdouble d = *JSVAL_TO_DOUBLE(*vp);
            if (!JSDOUBLE_IS_FINITE(d))
                return js_AppendLiteral(scx->cb, "null");
        }

        char numBuf[DTOSTR_STANDARD_BUFFER_SIZE], *numStr;
        jsdouble d = JSVAL_IS_INT(*vp) ? jsdouble(JSVAL_TO_INT(*vp)) : *JSVAL_TO_DOUBLE(*vp);
        numStr = js_dtostr(JS_THREAD_DATA(cx)->dtoaState, numBuf, sizeof numBuf,
                           DTOSTR_STANDARD, 0, d);
        if (!numStr) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }

        jschar dstr[DTOSTR_STANDARD_BUFFER_SIZE];
        size_t dbufSize = DTOSTR_STANDARD_BUFFER_SIZE;
        if (!js_InflateStringToBuffer(cx, numStr, strlen(numStr), dstr, &dbufSize))
            return JS_FALSE;

        return scx->cb.append(dstr, dbufSize);
    }

    if (JSVAL_IS_OBJECT(*vp) && !VALUE_IS_FUNCTION(cx, *vp) && !VALUE_IS_XML(*vp)) {
        JSBool ok;

        scx->depth++;
        ok = (JS_IsArrayObject(cx, JSVAL_TO_OBJECT(*vp)) ? JA : JO)(cx, vp, scx);
        scx->depth--;

        return ok;
    }

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
InitializeGap(JSContext *cx, jsval space, JSCharBuffer &cb)
{
    AutoValueRooter gap(cx, space);

    if (!JSVAL_IS_PRIMITIVE(space)) {
        JSObject *obj = JSVAL_TO_OBJECT(space);
        JSClass *clasp = obj->getClass();
        if (clasp == &js_NumberClass || clasp == &js_StringClass)
            *gap.addr() = obj->getPrimitiveThis();
    }

    if (JSVAL_IS_STRING(gap.value())) {
        if (!js_ValueToCharBuffer(cx, gap.value(), cb))
            return JS_FALSE;
        if (cb.length() > 10)
            cb.resize(10);
    }

    if (JSVAL_IS_NUMBER(gap.value())) {
        jsdouble d = JSVAL_IS_INT(gap.value())
                     ? JSVAL_TO_INT(gap.value())
                     : js_DoubleToInteger(*JSVAL_TO_DOUBLE(gap.value()));
        d = JS_MIN(10, d);
        if (d >= 1 && !cb.appendN(' ', uint32(d)))
            return JS_FALSE;
    }

    return JS_TRUE;
}

JSBool
js_Stringify(JSContext *cx, jsval *vp, JSObject *replacer, jsval space,
             JSCharBuffer &cb)
{
    StringifyContext scx(cx, cb, replacer);
    if (!InitializeGap(cx, space, scx.gap))
        return JS_FALSE;

    JSObject *obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!obj)
        return JS_FALSE;

    AutoObjectRooter tvr(cx, obj);
    if (!obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.emptyAtom),
                             *vp, NULL, NULL, JSPROP_ENUMERATE)) {
        return JS_FALSE;
    }

    return Str(cx, ATOM_TO_JSID(cx->runtime->atomState.emptyAtom), obj, &scx, vp);
}

// helper to determine whether a character could be part of a number
static JSBool IsNumChar(jschar c)
{
    return ((c <= '9' && c >= '0') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E');
}

static JSBool HandleData(JSContext *cx, JSONParser *jp, JSONDataType type);
static JSBool PopState(JSContext *cx, JSONParser *jp);

static bool
Walk(JSContext *cx, jsid id, JSObject *holder, jsval reviver, jsval *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    if (!holder->getProperty(cx, id, vp))
        return false;

    JSObject *obj;

    if (!JSVAL_IS_PRIMITIVE(*vp) && !(obj = JSVAL_TO_OBJECT(*vp))->isCallable()) {
        AutoValueRooter propValue(cx, JSVAL_NULL);

        if(obj->isArray()) {
            jsuint length = 0;
            if (!js_GetLengthProperty(cx, obj, &length))
                return false;

            for (jsuint i = 0; i < length; i++) {
                jsid index;
                if (!js_IndexToId(cx, i, &index))
                    return false;

                if (!Walk(cx, index, obj, reviver, propValue.addr()))
                    return false;

                if (!obj->defineProperty(cx, index, propValue.value(), NULL, NULL, JSPROP_ENUMERATE))
                    return false;
            }
        } else {
            AutoIdArray ida(cx, JS_Enumerate(cx, obj));
            if (!ida)
                return false;

            for (jsint i = 0, len = ida.length(); i < len; i++) {
                jsid idName = ida[i];
                if (!Walk(cx, idName, obj, reviver, propValue.addr()))
                    return false;
                if (propValue.value() == JSVAL_VOID) {
                    if (!js_DeleteProperty(cx, obj, idName, propValue.addr()))
                        return false;
                } else {
                    if (!obj->defineProperty(cx, idName, propValue.value(), NULL, NULL,
                                             JSPROP_ENUMERATE)) {
                        return false;
                    }
                }
            }
        }
    }

    // return reviver.call(holder, key, value);
    jsval value = *vp;
    JSString *key = js_ValueToString(cx, ID_TO_VALUE(id));
    if (!key)
        return false;

    jsval vec[2] = {STRING_TO_JSVAL(key), value};
    jsval reviverResult;
    if (!JS_CallFunctionValue(cx, holder, reviver, 2, vec, &reviverResult))
        return false;

    *vp = reviverResult;
    return true;
}

static bool
Revive(JSContext *cx, jsval reviver, jsval *vp)
{

    JSObject *obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!obj)
        return false;

    AutoValueRooter tvr(cx, obj);
    if (!obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.emptyAtom),
                             *vp, NULL, NULL, JSPROP_ENUMERATE)) {
        return false;
    }

    return Walk(cx, ATOM_TO_JSID(cx->runtime->atomState.emptyAtom), obj, reviver, vp);
}

JSONParser *
js_BeginJSONParse(JSContext *cx, jsval *rootVal)
{
    if (!cx)
        return NULL;

    JSObject *arr = js_NewArrayObject(cx, 0, NULL);
    if (!arr)
        return NULL;

    JSONParser *jp = cx->create<JSONParser>(cx);
    if (!jp)
        return NULL;

    jp->objectStack = arr;
    if (!JS_AddNamedObjectRoot(cx, &jp->objectStack, "JSON parse stack"))
        goto bad;

    jp->statep = jp->stateStack;
    *jp->statep = JSON_PARSE_STATE_INIT;
    jp->rootVal = rootVal;

    return jp;

bad:
    js_FinishJSONParse(cx, jp, JSVAL_NULL);
    return NULL;
}

bool
js_FinishJSONParse(JSContext *cx, JSONParser *jp, jsval reviver)
{
    if (!jp)
        return true;

    JSBool early_ok = JS_TRUE;

    // Check for unprocessed primitives at the root. This doesn't happen for
    // strings because a closing quote triggers value processing.
    if ((jp->statep - jp->stateStack) == 1) {
        if (*jp->statep == JSON_PARSE_STATE_KEYWORD) {
            early_ok = HandleData(cx, jp, JSON_DATA_KEYWORD);
            if (early_ok)
                PopState(cx, jp);
        } else if (*jp->statep == JSON_PARSE_STATE_NUMBER) {
            early_ok = HandleData(cx, jp, JSON_DATA_NUMBER);
            if (early_ok)
                PopState(cx, jp);
        }
    }

    /* This internal API is infallible, in spite of its JSBool return type. */
    js_RemoveRoot(cx->runtime, &jp->objectStack);

    bool ok = *jp->statep == JSON_PARSE_STATE_FINISHED;
    jsval *vp = jp->rootVal;

    cx->destroy(jp);

    if (!early_ok)
        return false;

    if (!ok) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return false;
    }

    if (!JSVAL_IS_PRIMITIVE(reviver) && JSVAL_TO_OBJECT(reviver)->isCallable())
        ok = Revive(cx, reviver, vp);

    return ok;
}

static JSBool
PushState(JSContext *cx, JSONParser *jp, JSONParserState state)
{
    if (*jp->statep == JSON_PARSE_STATE_FINISHED) {
        // extra input
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    jp->statep++;
    if ((uint32)(jp->statep - jp->stateStack) >= JS_ARRAY_LENGTH(jp->stateStack)) {
        // too deep
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    *jp->statep = state;

    return JS_TRUE;
}

static JSBool
PopState(JSContext *cx, JSONParser *jp)
{
    jp->statep--;
    if (jp->statep < jp->stateStack) {
        jp->statep = jp->stateStack;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    if (*jp->statep == JSON_PARSE_STATE_INIT)
        *jp->statep = JSON_PARSE_STATE_FINISHED;

    return JS_TRUE;
}

static JSBool
PushValue(JSContext *cx, JSONParser *jp, JSObject *parent, jsval value)
{
    JSBool ok;
    if (parent->isArray()) {
        jsuint len;
        ok = js_GetLengthProperty(cx, parent, &len);
        if (ok) {
            jsid index;
            if (!js_IndexToId(cx, len, &index))
                return JS_FALSE;
            ok = parent->defineProperty(cx, index, value, NULL, NULL, JSPROP_ENUMERATE);
        }
    } else {
        ok = JS_DefineUCProperty(cx, parent, jp->objectKey.begin(),
                                 jp->objectKey.length(), value,
                                 NULL, NULL, JSPROP_ENUMERATE);
        jp->objectKey.clear();
    }

    return ok;
}

static JSBool
PushObject(JSContext *cx, JSONParser *jp, JSObject *obj)
{
    jsuint len;
    if (!js_GetLengthProperty(cx, jp->objectStack, &len))
        return JS_FALSE;
    if (len >= JSON_MAX_DEPTH) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    jsval v = OBJECT_TO_JSVAL(obj);
    AutoValueRooter tvr(cx, v);

    // Check if this is the root object
    if (len == 0) {
        *jp->rootVal = v;
        // This property must be enumerable to keep the array dense
        if (!jp->objectStack->defineProperty(cx, INT_TO_JSID(0), *jp->rootVal,
                                             NULL, NULL, JSPROP_ENUMERATE)) {
            return JS_FALSE;
        }
        return JS_TRUE;
    }

    jsval p;
    if (!jp->objectStack->getProperty(cx, INT_TO_JSID(len - 1), &p))
        return JS_FALSE;

    JS_ASSERT(JSVAL_IS_OBJECT(p));
    JSObject *parent = JSVAL_TO_OBJECT(p);
    if (!PushValue(cx, jp, parent, v))
        return JS_FALSE;

    // This property must be enumerable to keep the array dense
    if (!jp->objectStack->defineProperty(cx, INT_TO_JSID(len), v,
                                         NULL, NULL, JSPROP_ENUMERATE)) {
        return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
OpenObject(JSContext *cx, JSONParser *jp)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!obj)
        return JS_FALSE;

    return PushObject(cx, jp, obj);
}

static JSBool
OpenArray(JSContext *cx, JSONParser *jp)
{
    // Add an array to an existing array or object
    JSObject *arr = js_NewArrayObject(cx, 0, NULL);
    if (!arr)
        return JS_FALSE;

    return PushObject(cx, jp, arr);
}

static JSBool
CloseObject(JSContext *cx, JSONParser *jp)
{
    jsuint len;
    if (!js_GetLengthProperty(cx, jp->objectStack, &len))
        return JS_FALSE;
    if (!js_SetLengthProperty(cx, jp->objectStack, len - 1))
        return JS_FALSE;

    return JS_TRUE;
}

static JSBool
CloseArray(JSContext *cx, JSONParser *jp)
{
    return CloseObject(cx, jp);
}

static JSBool
PushPrimitive(JSContext *cx, JSONParser *jp, jsval value)
{
    AutoValueRooter tvr(cx, value);

    jsuint len;
    if (!js_GetLengthProperty(cx, jp->objectStack, &len))
        return JS_FALSE;

    if (len > 0) {
        jsval o;
        if (!jp->objectStack->getProperty(cx, INT_TO_JSID(len - 1), &o))
            return JS_FALSE;

        JS_ASSERT(!JSVAL_IS_PRIMITIVE(o));
        return PushValue(cx, jp, JSVAL_TO_OBJECT(o), value);
    }

    // root value must be primitive
    *jp->rootVal = value;
    return JS_TRUE;
}

static JSBool
HandleNumber(JSContext *cx, JSONParser *jp, const jschar *buf, uint32 len)
{
    const jschar *ep;
    double val;
    if (!js_strtod(cx, buf, buf + len, &ep, &val))
        return JS_FALSE;
    if (ep != buf + len) {
        // bad number input
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    jsval numVal;
    if (!JS_NewNumberValue(cx, val, &numVal))
        return JS_FALSE;

    return PushPrimitive(cx, jp, numVal);
}

static JSBool
HandleString(JSContext *cx, JSONParser *jp, const jschar *buf, uint32 len)
{
    JSString *str = js_NewStringCopyN(cx, buf, len);
    if (!str)
        return JS_FALSE;

    return PushPrimitive(cx, jp, STRING_TO_JSVAL(str));
}

static JSBool
HandleKeyword(JSContext *cx, JSONParser *jp, const jschar *buf, uint32 len)
{
    jsval keyword;
    TokenKind tt = js_CheckKeyword(buf, len);
    if (tt != TOK_PRIMARY) {
        // bad keyword
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    if (buf[0] == 'n') {
        keyword = JSVAL_NULL;
    } else if (buf[0] == 't') {
        keyword = JSVAL_TRUE;
    } else if (buf[0] == 'f') {
        keyword = JSVAL_FALSE;
    } else {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
        return JS_FALSE;
    }

    return PushPrimitive(cx, jp, keyword);
}

static JSBool
HandleData(JSContext *cx, JSONParser *jp, JSONDataType type)
{
    JSBool ok;

    switch (type) {
      case JSON_DATA_STRING:
        ok = HandleString(cx, jp, jp->buffer.begin(), jp->buffer.length());
        break;

      case JSON_DATA_KEYSTRING:
        ok = jp->objectKey.append(jp->buffer.begin(), jp->buffer.end());
        break;

      case JSON_DATA_NUMBER:
        ok = HandleNumber(cx, jp, jp->buffer.begin(), jp->buffer.length());
        break;

      default:
        JS_ASSERT(type == JSON_DATA_KEYWORD);
        ok = HandleKeyword(cx, jp, jp->buffer.begin(), jp->buffer.length());
        break;
    }

    if (ok)
        jp->buffer.clear();
    return ok;
}

JSBool
js_ConsumeJSONText(JSContext *cx, JSONParser *jp, const jschar *data, uint32 len)
{
    uint32 i;

    if (*jp->statep == JSON_PARSE_STATE_INIT) {
        PushState(cx, jp, JSON_PARSE_STATE_VALUE);
    }

    for (i = 0; i < len; i++) {
        jschar c = data[i];
        switch (*jp->statep) {
          case JSON_PARSE_STATE_VALUE:
            if (c == ']') {
                // empty array
                if (!PopState(cx, jp))
                    return JS_FALSE;

                if (*jp->statep != JSON_PARSE_STATE_ARRAY) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                    return JS_FALSE;
                }

                if (!CloseArray(cx, jp) || !PopState(cx, jp))
                    return JS_FALSE;

                break;
            }

            if (c == '}') {
                // we should only find these in OBJECT_KEY state
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }

            if (c == '"') {
                *jp->statep = JSON_PARSE_STATE_STRING;
                break;
            }

            if (IsNumChar(c)) {
                *jp->statep = JSON_PARSE_STATE_NUMBER;
                if (!jp->buffer.append(c))
                    return JS_FALSE;
                break;
            }

            if (JS7_ISLET(c)) {
                *jp->statep = JSON_PARSE_STATE_KEYWORD;
                if (!jp->buffer.append(c))
                    return JS_FALSE;
                break;
            }

          // fall through in case the value is an object or array
          case JSON_PARSE_STATE_OBJECT_VALUE:
            if (c == '{') {
                *jp->statep = JSON_PARSE_STATE_OBJECT;
                if (!OpenObject(cx, jp) || !PushState(cx, jp, JSON_PARSE_STATE_OBJECT_PAIR))
                    return JS_FALSE;
            } else if (c == '[') {
                *jp->statep = JSON_PARSE_STATE_ARRAY;
                if (!OpenArray(cx, jp) || !PushState(cx, jp, JSON_PARSE_STATE_VALUE))
                    return JS_FALSE;
            } else if (!JS_ISXMLSPACE(c)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_OBJECT:
            if (c == '}') {
                if (!CloseObject(cx, jp) || !PopState(cx, jp))
                    return JS_FALSE;
            } else if (c == ',') {
                if (!PushState(cx, jp, JSON_PARSE_STATE_OBJECT_PAIR))
                    return JS_FALSE;
            } else if (c == ']' || !JS_ISXMLSPACE(c)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_ARRAY :
            if (c == ']') {
                if (!CloseArray(cx, jp) || !PopState(cx, jp))
                    return JS_FALSE;
            } else if (c == ',') {
                if (!PushState(cx, jp, JSON_PARSE_STATE_VALUE))
                    return JS_FALSE;
            } else if (!JS_ISXMLSPACE(c)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_OBJECT_PAIR :
            if (c == '"') {
                // we want to be waiting for a : when the string has been read
                *jp->statep = JSON_PARSE_STATE_OBJECT_IN_PAIR;
                if (!PushState(cx, jp, JSON_PARSE_STATE_STRING))
                    return JS_FALSE;
            } else if (c == '}') {
                // pop off the object pair state and the object state
                if (!CloseObject(cx, jp) || !PopState(cx, jp) || !PopState(cx, jp))
                    return JS_FALSE;
            } else if (c == ']' || !JS_ISXMLSPACE(c)) {
              JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
              return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_OBJECT_IN_PAIR:
            if (c == ':') {
                *jp->statep = JSON_PARSE_STATE_VALUE;
            } else if (!JS_ISXMLSPACE(c)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_STRING:
            if (c == '"') {
                if (!PopState(cx, jp))
                    return JS_FALSE;
                JSONDataType jdt;
                if (*jp->statep == JSON_PARSE_STATE_OBJECT_IN_PAIR) {
                    jdt = JSON_DATA_KEYSTRING;
                } else {
                    jdt = JSON_DATA_STRING;
                }
                if (!HandleData(cx, jp, jdt))
                    return JS_FALSE;
            } else if (c == '\\') {
                *jp->statep = JSON_PARSE_STATE_STRING_ESCAPE;
            } else if (c < 31) {
                // The JSON lexical grammer does not allow a JSONStringCharacter to be
                // any of the Unicode characters U+0000 thru U+001F (control characters).
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            } else {
                if (!jp->buffer.append(c))
                    return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_STRING_ESCAPE:
            switch (c) {
              case '"':
              case '\\':
              case '/':
                break;
              case 'b' : c = '\b'; break;
              case 'f' : c = '\f'; break;
              case 'n' : c = '\n'; break;
              case 'r' : c = '\r'; break;
              case 't' : c = '\t'; break;
              default :
                if (c == 'u') {
                    jp->numHex = 0;
                    jp->hexChar = 0;
                    *jp->statep = JSON_PARSE_STATE_STRING_HEX;
                    continue;
                } else {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                    return JS_FALSE;
                }
            }

            if (!jp->buffer.append(c))
                return JS_FALSE;
            *jp->statep = JSON_PARSE_STATE_STRING;
            break;

          case JSON_PARSE_STATE_STRING_HEX:
            if (('0' <= c) && (c <= '9')) {
                jp->hexChar = (jp->hexChar << 4) | (c - '0');
            } else if (('a' <= c) && (c <= 'f')) {
                jp->hexChar = (jp->hexChar << 4) | (c - 'a' + 0x0a);
            } else if (('A' <= c) && (c <= 'F')) {
                jp->hexChar = (jp->hexChar << 4) | (c - 'A' + 0x0a);
            } else {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }

            if (++(jp->numHex) == 4) {
                if (!jp->buffer.append(jp->hexChar))
                    return JS_FALSE;
                jp->hexChar = 0;
                jp->numHex = 0;
                *jp->statep = JSON_PARSE_STATE_STRING;
            }
            break;

          case JSON_PARSE_STATE_KEYWORD:
            if (JS7_ISLET(c)) {
                if (!jp->buffer.append(c))
                    return JS_FALSE;
            } else {
                // this character isn't part of the keyword, process it again
                i--;
                if (!PopState(cx, jp))
                    return JS_FALSE;

                if (!HandleData(cx, jp, JSON_DATA_KEYWORD))
                    return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_NUMBER:
            if (IsNumChar(c)) {
                if (!jp->buffer.append(c))
                    return JS_FALSE;
            } else {
                // this character isn't part of the number, process it again
                i--;
                if (!PopState(cx, jp))
                    return JS_FALSE;
                if (!HandleData(cx, jp, JSON_DATA_NUMBER))
                    return JS_FALSE;
            }
            break;

          case JSON_PARSE_STATE_FINISHED:
            if (!JS_ISXMLSPACE(c)) {
                // extra input
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_JSON_BAD_PARSE);
                return JS_FALSE;
            }
            break;

          default:
            JS_NOT_REACHED("Invalid JSON parser state");
        }
    }

    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
static JSBool
json_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = ATOM_KEY(CLASS_ATOM(cx, JSON));
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
    JSObject *JSON;

    JSON = JS_NewObject(cx, &js_JSONClass, NULL, obj);
    if (!JSON)
        return NULL;
    if (!JS_DefineProperty(cx, obj, js_JSON_str, OBJECT_TO_JSVAL(JSON),
                           JS_PropertyStub, JS_PropertyStub, 0))
        return NULL;

    if (!JS_DefineFunctions(cx, JSON, json_static_methods))
        return NULL;

    return JSON;
}
