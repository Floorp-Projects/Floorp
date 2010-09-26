/* ***** BEGIN LICENSE BLOCK *****
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

#ifndef json_h___
#define json_h___

#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsvalue.h"
#include "jsvector.h"

#define JSON_MAX_DEPTH  2048
#define JSON_PARSER_BUFSIZE 1024

extern js::Class js_JSONClass;

extern JSObject *
js_InitJSONClass(JSContext *cx, JSObject *obj);

extern JSBool
js_Stringify(JSContext *cx, js::Value *vp, JSObject *replacer,
             const js::Value &space, JSCharBuffer &cb);

extern JSBool js_TryJSON(JSContext *cx, js::Value *vp);

/* JSON parsing states; most permit leading whitespace. */
enum JSONParserState {
    /* Start of string. */
    JSON_PARSE_STATE_INIT,

    /* JSON fully processed, expecting only trailing whitespace. */
    JSON_PARSE_STATE_FINISHED,

    /* Start of JSON value. */
    JSON_PARSE_STATE_VALUE,

    /* Start of first key/value pair in object, or at }. */
    JSON_PARSE_STATE_OBJECT_INITIAL_PAIR,

    /* Start of subsequent key/value pair in object, after delimiting comma. */
    JSON_PARSE_STATE_OBJECT_PAIR,

    /* At : in key/value pair in object. */
    JSON_PARSE_STATE_OBJECT_IN_PAIR,

    /* Immediately after key/value pair in object: at , or }. */
    JSON_PARSE_STATE_OBJECT_AFTER_PAIR,

    /* Start of first element of array or at ]. */
    JSON_PARSE_STATE_ARRAY_INITIAL_VALUE,

    /* Immediately after element in array: at , or ]. */
    JSON_PARSE_STATE_ARRAY_AFTER_ELEMENT,


    /* The following states allow no leading whitespace. */

    /* Within string literal. */
    JSON_PARSE_STATE_STRING,

    /* At first character after \ in string literal. */
    JSON_PARSE_STATE_STRING_ESCAPE,

    /* Within numbers in \uXXXX in string literal. */
    JSON_PARSE_STATE_STRING_HEX,

    /* Within numeric literal. */
    JSON_PARSE_STATE_NUMBER,

    /* Handling keywords (only null/true/false pass validity post-check). */
    JSON_PARSE_STATE_KEYWORD
};

enum JSONDataType {
    JSON_DATA_STRING,
    JSON_DATA_KEYSTRING,
    JSON_DATA_NUMBER,
    JSON_DATA_KEYWORD
};

struct JSONParser;

extern JSONParser *
js_BeginJSONParse(JSContext *cx, js::Value *rootVal, bool suppressErrors = false);

/* Aargh, Windows. */
#ifdef STRICT
#undef STRICT
#endif
#ifdef LEGACY
#undef LEGACY
#endif

/*
 * The type of JSON decoding to perform.  Strict decoding is to-the-spec;
 * legacy decoding accepts a few non-JSON syntaxes historically accepted by the
 * implementation.  (Full description of these deviations is deliberately
 * omitted.)  New users should use strict decoding rather than legacy decoding,
 * as legacy decoding might be removed at a future time.
 */
enum DecodingMode { STRICT, LEGACY };

extern JS_FRIEND_API(JSBool)
js_ConsumeJSONText(JSContext *cx, JSONParser *jp, const jschar *data, uint32 len,
                   DecodingMode decodingMode = STRICT);

extern bool
js_FinishJSONParse(JSContext *cx, JSONParser *jp, const js::Value &reviver);

#endif /* json_h___ */
