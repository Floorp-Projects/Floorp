/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#include <algorithm>

#include "parser.h"
#include "numerics.h"
#include "js2runtime.h"

#include "jsstring.h"
#include "regexp.h"

namespace JavaScript {    
namespace JS2Runtime {


JSValue String_Constructor(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    JSValue thatValue = thisValue;
    if (thatValue.isNull())
        thatValue = String_Type->newInstance(cx);
    ASSERT(thatValue.isObject());
    JSObject *thisObj = thatValue.object;
    JSStringInstance *strInst = checked_cast<JSStringInstance *>(thisObj);

    if (argc > 0)
        thisObj->mPrivate = (void *)(new String(*argv[0].toString(cx).string));
    else
        thisObj->mPrivate = (void *)(&cx->Empty_StringAtom);
    strInst->mLength = ((String *)(thisObj->mPrivate))->size();
    return thatValue;
}

JSValue String_TypeCast(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return JSValue(&cx->Empty_StringAtom);
    else
        return argv[0].toString(cx);
}


JSValue String_fromCharCode(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    String *resultStr = new String();   // can't use cx->Empty_StringAtom; because we're modifying this below
    resultStr->reserve(argc);
    for (uint32 i = 0; i < argc; i++)
        *resultStr += (char16)(argv[i].toUInt16(cx).f64);

    return JSValue(resultStr);
}

static JSValue String_toString(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject());
    if (thisValue.getType() != String_Type)
        cx->reportError(Exception::typeError, "String.toString called on something other than a string thing");
    JSObject *thisObj = thisValue.object;
    return JSValue((String *)thisObj->mPrivate);
}

static JSValue String_valueOf(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject());
    if (thisValue.getType() != String_Type)
        cx->reportError(Exception::typeError, "String.valueOf called on something other than a string thing");
    JSObject *thisObj = thisValue.object;
    return JSValue((String *)thisObj->mPrivate);
}

static JSValue String_search(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    JSValue S = thisValue.toString(cx);

    JSValue regexp = argv[0];
    if ((argc == 0) || !regexp.isObject() || (regexp.object->mType != RegExp_Type)) {
        regexp = kNullValue;
        regexp = RegExp_Constructor(cx, regexp, argv, 1);
    }
    REParseState *parseResult = (REParseState *)(regexp.object->mPrivate);

    /* save & restore lastIndex as it's not to be modified */
    uint32 lastIndex = parseResult->lastIndex;
    parseResult->lastIndex = 0;
    REState *regexp_result = REExecute(parseResult, S.string->begin(), S.string->length());
    parseResult->lastIndex = lastIndex;

    if (regexp_result)
        return JSValue((float64)(regexp_result->startIndex));
    else
        return JSValue(-1.0);

}

static JSValue String_match(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    JSValue S = thisValue.toString(cx);

    JSValue regexp = argv[0];
    if ((argc == 0) || !regexp.isObject() || (regexp.object->mType != RegExp_Type)) {
        regexp = kNullValue;
        regexp = RegExp_Constructor(cx, regexp, argv, 1);
    }

    REParseState *parseResult = (REParseState *)(regexp.object->mPrivate);
    if ((parseResult->flags & GLOBAL) == 0) {
        return RegExp_exec(cx, regexp, &S, 1);                
    }
    else {
        JSArrayInstance *A = (JSArrayInstance *)Array_Type->newInstance(cx);
        parseResult->lastIndex = 0;
        int32 index = 0;
        while (true) {
            REState *regexp_result = REExecute(parseResult, S.string->begin(), S.string->length());
            if (regexp_result == NULL)
                break;
            if (parseResult->lastIndex == index)
                parseResult->lastIndex++;
            String *matchStr = new String(S.string->substr(regexp_result->startIndex, regexp_result->endIndex - regexp_result->startIndex));
            A->setProperty(cx, *numberToString(index++), NULL, JSValue(matchStr));
        }
        regexp.object->setProperty(cx, cx->LastIndex_StringAtom, NULL, JSValue((float64)(parseResult->lastIndex)));
        return JSValue(A);
    }
}

static const String interpretDollar(Context *cx, const String *replaceStr, uint32 dollarPos, const String *searchStr, REState *regexp_result, uint32 &skip)
{
    skip = 2;
    const char16 *dollarValue = replaceStr->begin() + dollarPos + 1;
    switch (*dollarValue) {
    case '$':
	return cx->Dollar_StringAtom;
    case '&':
	return searchStr->substr(regexp_result->startIndex, regexp_result->endIndex - regexp_result->startIndex);
    case '`':
	return searchStr->substr(0, regexp_result->startIndex);
    case '\'':
	return searchStr->substr(regexp_result->endIndex, searchStr->length() - regexp_result->endIndex);
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	{
	    int32 num = (uint32)(*dollarValue - '0');
	    if (num <= regexp_result->n) {
		if ((dollarPos < (replaceStr->length() - 2))
			&& (dollarValue[1] >= '0') && (dollarValue[1] <= '9')) {
		    int32 tmp = (num * 10) + (dollarValue[1] - '0');
		    if (tmp <= regexp_result->n) {
			num = tmp;
			skip = 3;
		    }
		}
		return searchStr->substr((uint32)(regexp_result->parens[num - 1].index), (uint32)(regexp_result->parens[num - 1].length));
	    }
	}
	// fall thru
    default:
	skip = 1;
	return cx->Dollar_StringAtom;
    }
}

static JSValue String_replace(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    JSValue S = thisValue.toString(cx);

    JSValue searchValue;
    JSValue replaceValue;

    if (argc > 0) searchValue = argv[0];
    if (argc > 1) replaceValue = argv[1];
    const String *replaceStr = replaceValue.toString(cx).string;

    if (searchValue.isObject() && (searchValue.object->mType == RegExp_Type)) {
	REParseState *parseResult = (REParseState *)(searchValue.object->mPrivate);
	REState *regexp_result;
	uint32 m = parseResult->parenCount;
	String newString;
        uint32 index = 0;

	while (true) {
	    if (parseResult->flags & GLOBAL)
		parseResult->lastIndex = (int32)index;
            regexp_result = REExecute(parseResult, S.string->begin(), S.string->length());
	    if (regexp_result) {
		String insertString;
		uint32 start = 0;
		while (true) {
		    uint32 dollarPos = replaceStr->find('$', start);
		    if ((dollarPos != String::npos) && (dollarPos < (replaceStr->length() - 1))) {
			uint32 skip;
			insertString += replaceStr->substr(start, dollarPos - start);
			insertString += interpretDollar(cx, replaceStr, dollarPos, S.string, regexp_result, skip);
			start = dollarPos + skip;
		    }
		    else {
			insertString += replaceStr->substr(start, replaceStr->length() - start);
			break;
		    }
		}
		newString += S.string->substr(index, regexp_result->startIndex - index);
		newString += insertString;
	    }
	    else
		break;
	    index = regexp_result->endIndex;
	    if ((parseResult->flags & GLOBAL) == 0)
		break;
	}
	newString += S.string->substr(index, S.string->length() - index);
	return JSValue(new String(newString));
    }
    else {
	const String *searchStr = searchValue.toString(cx).string;
	REState regexp_result;
        uint32 pos = S.string->find(*searchStr, 0);
	if (pos == String::npos)
	    return JSValue(S.string);
	regexp_result.startIndex = (int32)pos;
	regexp_result.endIndex = regexp_result.startIndex + searchStr->length();
	regexp_result.n = 0;
	String insertString;
	String newString;
	uint32 start = 0;
	while (true) {
	    uint32 dollarPos = replaceStr->find('$', start);
	    if ((dollarPos != String::npos) && (dollarPos < (replaceStr->length() - 1))) {
		uint32 skip;
		insertString += replaceStr->substr(start, dollarPos - start);
		insertString += interpretDollar(cx, replaceStr, dollarPos, S.string, &regexp_result, skip);
		start = dollarPos + skip;
	    }
	    else {
		insertString += replaceStr->substr(start, replaceStr->length() - start);
		break;
	    }
	}
	newString += S.string->substr(0, regexp_result.startIndex);
	newString += insertString;
	uint32 index = regexp_result.endIndex;
	newString += S.string->substr(index, S.string->length() - index);
	return JSValue(new String(newString));
    }
}

struct MatchResult {
    bool failure;
    uint32 endIndex;
    uint32 capturesCount;
    JSValue *captures;
};

static void strSplitMatch(const String *S, uint32 q, const String *R, MatchResult &result)
{
    result.failure = true;
    result.captures = NULL;
    result.capturesCount = 0;

    uint32 r = R->size();
    uint32 s = S->size();
    if ((q + r) > s)
        return;
    for (uint32 i = 0; i < r; i++) {
        if ((*S)[q + i] != (*R)[i])
            return;
    }
    result.endIndex = q + r;
    result.failure = false;
}

static void regexpSplitMatch(const String *S, uint32 q, REParseState *RE, MatchResult &result)
{
    result.failure = true;
    result.captures = NULL;

    REState *regexp_result = REMatch(RE, S->begin() + q, S->length() - q);

    if (regexp_result) {
        result.endIndex = regexp_result->startIndex + q;
        result.failure = false;
        result.capturesCount = regexp_result->n;
        if (regexp_result->n) {
            result.captures = new JSValue[regexp_result->n];
            for (int32 i = 0; i < regexp_result->n; i++) {
                if (regexp_result->parens[i].index != -1) {
                    String *parenStr = new String(S->substr((uint32)(regexp_result->parens[i].index + q), 
                                                    (uint32)(regexp_result->parens[i].length)));
                    result.captures[i] = JSValue(parenStr);
                }
		else
                    result.captures[i] = kUndefinedValue;
            }
        }
    }

}

static JSValue String_split(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    JSValue S = thisValue.toString(cx);

    JSArrayInstance *A = (JSArrayInstance *)Array_Type->newInstance(cx);
    uint32 lim;
    JSValue separatorV = (argc > 0) ? argv[0] : kUndefinedValue;
    JSValue limitV = (argc > 1) ? argv[1] : kUndefinedValue;
        
    if (limitV.isUndefined())
        lim = (uint32)(two32minus1);
    else
        lim = (uint32)(limitV.toUInt32(cx).f64);

    uint32 s = S.string->size();
    uint32 p = 0;

    REParseState *RE = NULL;
    const String *R = NULL;
    if (separatorV.isObject() && (separatorV.object->mType == RegExp_Type))
        RE = (REParseState *)(separatorV.object->mPrivate);
    else
        R = separatorV.toString(cx).string;

    if (lim == 0) 
        return JSValue(A);

/* XXX standard requires this, but Monkey doesn't do it and the tests break

    if (separatorV.isUndefined()) {
        A->setProperty(cx, widenCString("0"), NULL, S);
        return JSValue(A);
    }
*/
    if (s == 0) {
        MatchResult z;
        if (RE)
            regexpSplitMatch(S.string, 0, RE, z);
        else
            strSplitMatch(S.string, 0, R, z);
        if (!z.failure)
            return JSValue(A);
        A->setProperty(cx, widenCString("0"), NULL, S);
        return JSValue(A);
    }

    while (true) {
        uint32 q = p;
step11:
        if (q == s) {
            String *T = new String(*S.string, p, (s - p));
            JSValue v(T);
            A->setProperty(cx, *numberToString(A->mLength), NULL, v);
            return JSValue(A);
        }
        MatchResult z;
        if (RE)
            regexpSplitMatch(S.string, q, RE, z);
        else
            strSplitMatch(S.string, q, R, z);
        if (z.failure) {
            q = q + 1;
            goto step11;
        }
        uint32 e = z.endIndex;
        if (e == p) {
            q = q + 1;
            goto step11;
        }
        String *T = new String(*S.string, p, (q - p));
        JSValue v(T);
        A->setProperty(cx, *numberToString(A->mLength), NULL, v);
        if (A->mLength == lim)
            return JSValue(A);
        p = e;

        for (uint32 i = 0; i < z.capturesCount; i++) {
            A->setProperty(cx, *numberToString(A->mLength), NULL, JSValue(z.captures[i]));
            if (A->mLength == lim)
                return JSValue(A);
        }
    }
}

static JSValue String_charAt(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    const String *str = thisValue.toString(cx).string;

    uint32 pos = 0;
    if (argc > 0)
        pos = (uint32)(argv[0].toInt32(cx).f64);

    if ((pos < 0) || (pos >= str->size()))
        return JSValue(&cx->Empty_StringAtom);
    else
        return JSValue(new String(1, (*str)[pos]));
    
}

static JSValue String_charCodeAt(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    const String *str = thisValue.toString(cx).string;

    uint32 pos = 0;
    if (argc > 0)
        pos = (uint32)(argv[0].toInt32(cx).f64);

    if ((pos < 0) || (pos >= str->size()))
        return kNaNValue;
    else
        return JSValue((float64)(*str)[pos]);
}

static JSValue String_concat(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    const String *str = thisValue.toString(cx).string;
    String *result = new String(*str);

    for (uint32 i = 0; i < argc; i++) {
        *result += *argv[i].toString(cx).string;
    }

    return JSValue(result);
}

static JSValue String_indexOf(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    if (argc == 0)
        return JSValue(-1.0);

    const String *str = thisValue.toString(cx).string;
    const String *searchStr = argv[0].toString(cx).string;
    uint32 pos = 0;

    if (argc > 1) {
        float64 fpos = argv[1].toNumber(cx).f64;
        if (JSDOUBLE_IS_NaN(fpos))
            pos = 0;
        if (fpos < 0)
            pos = 0;
        else
            if (fpos >= str->size()) 
                pos = str->size();
            else
                pos = (uint32)(fpos);
    }
    pos = str->find(*searchStr, pos);
    if (pos == String::npos)
        return JSValue(-1.0);
    return JSValue((float64)pos);
}

static JSValue String_lastIndexOf(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    if (argc == 0)
        return JSValue(-1.0);

    const String *str = thisValue.toString(cx).string;
    const String *searchStr = argv[0].toString(cx).string;
    uint32 pos = str->size();

    if (argc > 1) {
        float64 fpos = argv[1].toNumber(cx).f64;
        if (JSDOUBLE_IS_NaN(fpos))
            pos = str->size();
        else {
            if (fpos < 0)
                pos = 0;
            else
                if (fpos >= str->size()) 
                    pos = str->size();
                else
                    pos = (uint32)(fpos);
        }
    }
    pos = str->rfind(*searchStr, pos);
    if (pos == String::npos)
        return JSValue(-1.0);
    return JSValue((float64)pos);
}

static JSValue String_localeCompare(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue * /*argv*/, uint32 /*argc*/)
{
    return kUndefinedValue;
}

static JSValue String_toLowerCase(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    JSValue S = thisValue.toString(cx);

    String *result = new String(*S.string);
    for (String::iterator i = result->begin(), end = result->end(); i != end; i++)
        *i = toLower(*i);

    return JSValue(result);
}

static JSValue String_toUpperCase(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    JSValue S = thisValue.toString(cx);

    String *result = new String(*S.string);
    for (String::iterator i = result->begin(), end = result->end(); i != end; i++)
        *i = toUpper(*i);

    return JSValue(result);
}

static JSValue String_slice(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    const String *sourceString = thisValue.toString(cx).string;

    uint32 sourceLength = sourceString->size();
    uint32 start, end;

    if (argc > 0) {
        int32 arg0 = (int32)(argv[0].toInt32(cx).f64);
        if (arg0 < 0) {
            arg0 += sourceLength;
            if (arg0 < 0)
                start = 0;
            else
                start = toUInt32(arg0);
        }
        else {
            if (toUInt32(arg0) < sourceLength)
                start = toUInt32(arg0);
            else
                start = sourceLength;
        }            
    }
    else
        start = 0;

    if (argc > 1) {
        int32 arg1 = (int32)(argv[1].toInt32(cx).f64);
        if (arg1 < 0) {
            arg1 += sourceLength;
            if (arg1 < 0)
                end = 0;
            else
                end = toUInt32(arg1);
        }
        else {
            if (toUInt32(arg1) < sourceLength)
                end = toUInt32(arg1);
            else
                end = sourceLength;
        }            
    }
    else
        end = sourceLength;

    if (start > end)
        return JSValue(&cx->Empty_StringAtom);
    return JSValue(new String(sourceString->substr(start, end - start)));
}

static JSValue String_substring(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(thisValue.isObject() || thisValue.isFunction() || thisValue.isType());
    const String *sourceString = thisValue.toString(cx).string;

    uint32 sourceLength = sourceString->size();
    uint32 start, end;

    if (argc > 0) {
        float64 farg0 = argv[0].toNumber(cx).f64;
        if (JSDOUBLE_IS_NaN(farg0) || (farg0 < 0))
            start = 0;
        else {
            if (!JSDOUBLE_IS_FINITE(farg0))
                start = sourceLength;
            else {
                start = JSValue::float64ToUInt32(farg0);
                if (start > sourceLength)
                    start = sourceLength;
            }
        }
    }
    else
        start = 0;

    if (argc > 1) {
        float64 farg1 = argv[1].toNumber(cx).f64;
        if (JSDOUBLE_IS_NaN(farg1) || (farg1 < 0))
            end = 0;
        else {
            if (!JSDOUBLE_IS_FINITE(farg1))
                end = sourceLength;
            else {
                end = JSValue::float64ToUInt32(farg1);
                if (end > sourceLength)
                    end = sourceLength;
            }
        }
    }
    else
        end = sourceLength;

    if (start > end) {
        uint32 t = start;
        start = end;
        end = t;
    }
        
    return JSValue(new String(sourceString->substr(start, end - start)));
}


Context::PrototypeFunctions *getStringProtos()
{
    Context::ProtoFunDef stringProtos[] = 
    {
        { "toString",           String_Type, 0, String_toString },
        { "valueOf",            String_Type, 0, String_valueOf },
        { "charAt",             String_Type, 1, String_charAt },
        { "charCodeAt",         Number_Type, 1, String_charCodeAt },
        { "concat",             String_Type, 1, String_concat },
        { "indexOf",            Number_Type, 2, String_indexOf },   // XXX ECMA spec says 1, but tests want 2 XXX
        { "lastIndexOf",        Number_Type, 2, String_lastIndexOf },   // XXX ECMA spec says 1, but tests want 2 XXX
        { "localeCompare",      Number_Type, 1, String_localeCompare },
        { "match",              Array_Type,  1, String_match },
        { "replace",            String_Type, 2, String_replace },
        { "search",             Number_Type, 1, String_search },
        { "slice",              String_Type, 2, String_slice },
        { "split",              Array_Type,  1, String_split },         // XXX ECMA spec says 2, but tests want 1 XXX
        { "substring",          String_Type, 2, String_substring },
        { "toSource",           String_Type, 0, String_toString },
        { "toLocaleUpperCase",  String_Type, 0, String_toUpperCase },  // (sic)
        { "toLocaleLowerCase",  String_Type, 0, String_toLowerCase },  // (sic)
        { "toUpperCase",        String_Type, 0, String_toUpperCase },
        { "toLowerCase",        String_Type, 0, String_toLowerCase },
        { NULL }
    };
    return new Context::PrototypeFunctions(&stringProtos[0]);
}

}
}
