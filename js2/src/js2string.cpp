/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <list>
#include <map>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {    
namespace MetaData {


js2val String_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    // XXX GC might happen after the new StringInstance, but before the
    // object gets rooted somewhere - this is a general problem...
    js2val thatValue = OBJECT_TO_JS2VAL(new StringInstance(meta->stringClass));
    StringInstance *strInst = checked_cast<StringInstance *>(JS2VAL_TO_OBJECT(thatValue));

    if (argc > 0)
        strInst->mValue = meta->engine->allocStringPtr(meta->toString(argv[0]));
    else
        strInst->mValue = meta->engine->allocStringPtr("");
    return thatValue;
}

js2val String_fromCharCode(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    String *resultStr = meta->engine->allocStringPtr((String *)NULL);   // can't use Empty_StringAtom; because we're modifying this below
    resultStr->reserve(argc);
    for (uint32 i = 0; i < argc; i++)
        *resultStr += (char16)(JS2Engine::float64toUInt16(argv[i]));

    return STRING_TO_JS2VAL(resultStr);
}

static js2val String_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (meta->objectType(thisValue) != meta->stringClass)
        meta->reportError(Exception::typeError, "String.toString called on something other than a string thing", meta->engine->errorPos());
    StringInstance *strInst = checked_cast<StringInstance *>(JS2VAL_TO_OBJECT(thisValue));
    return STRING_TO_JS2VAL(strInst->mValue);
}

static js2val String_valueOf(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (meta->objectType(thisValue) != meta->stringClass)
        meta->reportError(Exception::typeError, "String.toString called on something other than a string thing", meta->engine->errorPos());
    StringInstance *strInst = checked_cast<StringInstance *>(JS2VAL_TO_OBJECT(thisValue));
    return STRING_TO_JS2VAL(strInst->mValue);
}

/*
 * 15.5.4.12 String.prototype.search (regexp)
 *
 * If regexp is not an object whose [[Class]] property is "RegExp", it is replaced with the result of the expression new
 * RegExp(regexp). Let string denote the result of converting the this value to a string.
 * The value string is searched from its beginning for an occurrence of the regular expression pattern regexp. The
 * result is a number indicating the offset within the string where the pattern matched, or -1 if there was no match.
 * NOTE This method ignores the lastIndex and global properties of regexp. The lastIndex property of regexp is left
 * unchanged.
*/
static js2val String_search(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val S = STRING_TO_JS2VAL(meta->toString(thisValue));

    js2val regexp = argv[0];
    
    if ((argc == 0) || (meta->objectType(thisValue) != meta->regexpClass)) {        
        regexp = JS2VAL_NULL;
        regexp = RegExp_Constructor(meta, regexp, argv, 1);
    }
    REState *pState = (checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(regexp)))->mRegExp;

    const String *str = JS2VAL_TO_STRING(S);
    REMatchState *match = REExecute(pState, str->begin(), 0, (int32)str->length(), false);
    if (match)
        return meta->engine->allocNumber((float64)(match->startIndex));
    else
        return meta->engine->allocNumber(-1.0);

}

/*
 * 15.5.4.10 String.prototype.match (regexp)
 * 
 * If regexp is not an object whose [[Class]] property is "RegExp", it is replaced with the result of the expression new
 * RegExp(regexp). Let string denote the result of converting the this value to a string. Then do one of the following:
 * - If regexp.global is false: Return the result obtained by invoking RegExp.prototype.exec (see section
 *    15.10.6.2) on regexp with string as parameter.
 * - If regexp.global is true: Set the regexp.lastIndex property to 0 and invoke RegExp.prototype.exec
 *    repeatedly until there is no match. If there is a match with an empty string (in other words, if the value of
 *    regexp.lastIndex is left unchanged), increment regexp.lastIndex by 1. Let n be the number of matches. The
 *    value returned is an array with the length property set to n and properties 0 through n-1 corresponding to the
 *    first elements of the results of all matching invocations of RegExp.prototype.exec.
 */
 
static js2val String_match(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val S = STRING_TO_JS2VAL(meta->toString(thisValue));

    js2val regexp = argv[0];
    if ((argc == 0) || (meta->objectType(thisValue) != meta->regexpClass)) {        
        regexp = JS2VAL_NULL;
        regexp = RegExp_Constructor(meta, regexp, argv, 1);
    }

    RegExpInstance *thisInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(regexp));
    REState *pState = thisInst->mRegExp;
    if ((pState->flags & RE_GLOBAL) == 0) {
        return RegExp_exec(meta, regexp, &S, 1);                
    }
    else {
        PrototypeInstance *A = new PrototypeInstance(meta->objectClass->prototype, meta->objectClass);
        int32 index = 0;
        int32 lastIndex = 0;
        while (true) {
            REMatchState *match = REExecute(pState, JS2VAL_TO_STRING(S)->begin(), lastIndex, toInt32(JS2VAL_TO_STRING(S)->length()), false);
            if (match == NULL)
                break;
            if (lastIndex == match->endIndex)
                lastIndex++;
            else
                lastIndex = match->endIndex;
            js2val matchStr = meta->engine->allocString(JS2VAL_TO_STRING(S)->substr(toUInt32(match->startIndex), toUInt32(match->endIndex) - match->startIndex));
            Multiname mname(&meta->world.identifiers[*meta->engine->numberToString(index)], meta->publicNamespace);
            index++;
            meta->writeDynamicProperty(A, &mname, true, matchStr, RunPhase);
        }
        thisInst->setLastIndex(meta, meta->engine->allocNumber((float64)lastIndex));
        return OBJECT_TO_JS2VAL(A);
    }
}

static const String interpretDollar(JS2Metadata *meta, const String *replaceStr, uint32 dollarPos, const String *searchStr, REMatchState *match, uint32 &skip)
{
    skip = 2;
    const char16 *dollarValue = replaceStr->begin() + dollarPos + 1;
    switch (*dollarValue) {
    case '$':
	return *meta->engine->Dollar_StringAtom;
    case '&':
	return searchStr->substr((uint32)match->startIndex, (uint32)match->endIndex - match->startIndex);
    case '`':
	return searchStr->substr(0, (uint32)match->startIndex);
    case '\'':
	return searchStr->substr((uint32)match->endIndex, (uint32)searchStr->length() - match->endIndex);
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
	    int32 num = (int32)(*dollarValue - '0');
	    if (num <= match->parenCount) {
		if ((dollarPos < (replaceStr->length() - 2))
			&& (dollarValue[1] >= '0') && (dollarValue[1] <= '9')) {
		    int32 tmp = (num * 10) + (dollarValue[1] - '0');
		    if (tmp <= match->parenCount) {
			num = tmp;
			skip = 3;
		    }
		}
		return searchStr->substr((uint32)(match->parens[num - 1].index), (uint32)(match->parens[num - 1].length));
	    }
	}
	// fall thru
    default:
	skip = 1;
	return *meta->engine->Dollar_StringAtom;
    }
}

/*
 * 15.5.4.11 String.prototype.replace (searchValue, replaceValue)
 * 
 * Let string denote the result of converting the this value to a string.
 * 
 * If searchValue is a regular expression (an object whose [[Class]] property is "RegExp"), do the following: If
 * searchValue.global is false, then search string for the first match of the regular expression searchValue. If
 * searchValue.global is true, then search string for all matches of the regular expression searchValue. Do the search
 * in the same manner as in String.prototype.match, including the update of searchValue.lastIndex. Let m
 * be the number of left capturing parentheses in searchValue (NCapturingParens as specified in section 15.10.2.1).
 * 
 * If searchValue is not a regular expression, let searchString be toString(searchValue) and search string for the first
 * occurrence of searchString. Let m be 0.
 * 
 * If replaceValue is a function, then for each matched substring, call the function with the following m + 3 arguments.
 * Argument 1 is the substring that matched. If searchValue is a regular expression, the next m arguments are all of
 * the captures in the MatchResult (see section 15.10.2.1). Argument m + 2 is the offset within string where the match
 * occurred, and argument m + 3 is string. The result is a string value derived from the original input by replacing each
 * matched substring with the corresponding return value of the function call, converted to a string if need be.
 * 
 * Otherwise, let newstring denote the result of converting replaceValue to a string. The result is a string value derived
 * from the original input string by replacing each matched substring with a string derived from newstring by replacing
 * characters in newstring by replacement text as specified in the following table. These $ replacements are done left-to-
 * right, and, once such a replacement is performed, the new replacement text is not subject to further
 * replacements. For example, "$1,$2".replace(/(\$(\d))/g, "$$1-$1$2") returns "$1-$11,$1-$22". A
 * $ in newstring that does not match any of the forms below is left as is.
 */


static js2val String_replace(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *S = meta->toString(thisValue);

    js2val searchValue = JS2VAL_UNDEFINED;
    js2val replaceValue = JS2VAL_UNDEFINED;

    if (argc > 0) searchValue = argv[0];
    if (argc > 1) replaceValue = argv[1];
    const String *replaceStr = meta->toString(replaceValue);

    if (meta->objectType(searchValue) != meta->regexpClass) {
        RegExpInstance *reInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(searchValue)); 
	REState *pState = reInst->mRegExp;
	REMatchState *match;
	String newString;
        int32 lastIndex = 0;

	while (true) {
            match = REExecute(pState, S->begin(), lastIndex, toInt32(S->length()), false);
	    if (match) {
		String insertString;
		uint32 start = 0;
		while (true) {
                    // look for '$' in the replacement string and interpret it as necessary
		    uint32 dollarPos = replaceStr->find('$', start);
		    if ((dollarPos != String::npos) && (dollarPos < (replaceStr->length() - 1))) {
			uint32 skip;
			insertString += replaceStr->substr(start, dollarPos - start);
			insertString += interpretDollar(meta, replaceStr, dollarPos, S, match, skip);
			start = dollarPos + skip;
		    }
		    else {
                        // otherwise, absorb the entire replacement string
			insertString += replaceStr->substr(start, replaceStr->length() - start);
			break;
		    }
		}
                // grab everything preceding the match
		newString += S->substr(toUInt32(lastIndex), toUInt32(match->startIndex) - lastIndex);
                // and then add the replacement string
		newString += insertString;
	    }
	    else
		break;
	    lastIndex = match->endIndex;        // use lastIndex to grab remainder after break
	    if ((pState->flags & RE_GLOBAL) == 0)
		break;
	}
        newString += S->substr(toUInt32(lastIndex), toUInt32(S->length()) - lastIndex);
	if ((pState->flags & RE_GLOBAL) == 0)
            reInst->setLastIndex(meta, meta->engine->allocNumber((float64)lastIndex));
        return meta->engine->allocString(newString);
    }
    else {
        const String *searchStr = meta->toString(searchValue);
	REMatchState match;
        uint32 pos = S->find(*searchStr, 0);
	if (pos == String::npos)
            return STRING_TO_JS2VAL(S);
	match.startIndex = (int32)pos;
	match.endIndex = match.startIndex + (int32)searchStr->length();
	match.parenCount = 0;
	String insertString;
	String newString;
	uint32 start = 0;
	while (true) {
	    uint32 dollarPos = replaceStr->find('$', start);
	    if ((dollarPos != String::npos) && (dollarPos < (replaceStr->length() - 1))) {
		uint32 skip;
		insertString += replaceStr->substr(start, dollarPos - start);
		insertString += interpretDollar(meta, replaceStr, dollarPos, S, &match, skip);
		start = dollarPos + skip;
	    }
	    else {
		insertString += replaceStr->substr(start, replaceStr->length() - start);
		break;
	    }
	}
	newString += S->substr(0, (uint32)match.startIndex);
	newString += insertString;
	uint32 index = (uint32)match.endIndex;
	newString += S->substr(index, S->length() - index);
        return meta->engine->allocString(newString);
    }
}

struct MatchResult {
    bool failure;
    uint32 endIndex;
    uint32 capturesCount;
    js2val *captures;
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

static void regexpSplitMatch(JS2Metadata *meta, const String *S, uint32 q, REState *RE, MatchResult &result)
{
    result.failure = true;
    result.captures = NULL;

    REMatchState *match = REMatch(RE, S->begin() + q, (int32)(S->length() - q));

    if (match) {
        result.endIndex = match->startIndex + q;
        result.failure = false;
        result.capturesCount = toUInt32(match->parenCount);
        if (match->parenCount) {
            result.captures = new js2val[match->parenCount];
            for (int32 i = 0; i < match->parenCount; i++) {
                if (match->parens[i].index != -1)
                    result.captures[i] = meta->engine->allocString(S->substr((uint32)(match->parens[i].index + q), 
                                                                    (uint32)(match->parens[i].length)));
		else
                    result.captures[i] = JS2VAL_UNDEFINED;
            }
        }
    }

}

static js2val String_split(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *S = meta->toString(thisValue);

    js2val result = OBJECT_TO_JS2VAL(new ArrayInstance(meta->arrayClass));
    ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));

    uint32 lim;
    js2val separatorV = (argc > 0) ? argv[0] : JS2VAL_UNDEFINED;
    js2val limitV = (argc > 1) ? argv[1] : JS2VAL_UNDEFINED;
        
    if (JS2VAL_IS_UNDEFINED(limitV))
        lim = JS2Engine::float64toUInt32(two32minus1);
    else
        lim = toUInt32(meta->toInteger(limitV));

    uint32 s = S->size();
    uint32 p = 0;

    REState *RE = NULL;
    const String *R = NULL;
    if (meta->objectType(separatorV) == meta->regexpClass)
        RE = (checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(separatorV)))->mRegExp;
    else
        R = meta->toString(separatorV);

    if (lim == 0) 
        return result;

/* XXX standard requires this, but Monkey doesn't do it and the tests break

    if (separatorV.isUndefined()) {
        A->setProperty(cx, widenCString("0"), NULL, S);
        return JSValue(A);
    }
*/
    Multiname mn(NULL, meta->publicNamespace);
    if (s == 0) {
        MatchResult z;
        if (RE)
            regexpSplitMatch(meta, S, 0, RE, z);
        else
            strSplitMatch(S, 0, R, z);
        if (!z.failure)
            return result;
        mn.name = meta->engine->numberToString((int32)0);       // XXX
        meta->writeDynamicProperty(A, &mn, true, STRING_TO_JS2VAL(S), RunPhase);
        return result;
    }

    while (true) {
        uint32 q = p;
step11:
        if (q == s) {
            js2val v = meta->engine->allocString(new String(*S, p, (s - p)));
            mn.name = meta->engine->numberToString(getLength(meta, A));
            meta->writeDynamicProperty(A, &mn, true, v, RunPhase);
            return result;
        }
        MatchResult z;
        if (RE)
            regexpSplitMatch(meta, S, q, RE, z);
        else
            strSplitMatch(S, q, R, z);
        if (z.failure) {
            q = q + 1;
            goto step11;
        }
        uint32 e = z.endIndex;
        if (e == p) {
            q = q + 1;
            goto step11;
        }
        String *T = meta->engine->allocStringPtr(new String(*S, p, (q - p)));   // XXX
        js2val v = STRING_TO_JS2VAL(T);
        mn.name = meta->engine->numberToString(getLength(meta, A));
        meta->writeDynamicProperty(A, &mn, true, v, RunPhase);
        if (getLength(meta, A) == lim)
            return result;
        p = e;

        for (uint32 i = 0; i < z.capturesCount; i++) {
            mn.name = meta->engine->numberToString(getLength(meta, A));
            meta->writeDynamicProperty(A, &mn, true, z.captures[i], RunPhase);
            if (getLength(meta, A) == lim)
                return result;
        }
    }
}

static js2val String_charAt(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *str = meta->toString(thisValue);

    uint32 pos = 0;
    if (argc > 0)
        pos = toUInt32(meta->toInteger(argv[0]));

    if ((pos < 0) || (pos >= str->size()))
        return STRING_TO_JS2VAL(meta->engine->Empty_StringAtom);
    else
        return meta->engine->allocString(new String(1, (*str)[pos]));   // XXX
    
}

static js2val String_charCodeAt(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *str = meta->toString(thisValue);

    uint32 pos = 0;
    if (argc > 0)
        pos = toUInt32(meta->toInteger(argv[0]));

    if ((pos < 0) || (pos >= str->size()))
        return meta->engine->nanValue;
    else
        return meta->engine->allocNumber((float64)(*str)[pos]);
}

static js2val String_concat(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *str = meta->toString(thisValue);
    String *result = meta->engine->allocStringPtr(str);

    for (uint32 i = 0; i < argc; i++) {
        *result += *meta->toString(argv[i]);
    }

    return STRING_TO_JS2VAL(result);
}

static js2val String_indexOf(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->allocNumber(-1.0);

    const String *str = meta->toString(thisValue);
    const String *searchStr = meta->toString(argv[0]);
    uint32 pos = 0;

    if (argc > 1) {
        float64 fpos = meta->toFloat64(argv[1]);
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
        return meta->engine->allocNumber(-1.0);
    return meta->engine->allocNumber((float64)pos);
}

static js2val String_lastIndexOf(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->allocNumber(-1.0);

    const String *str = meta->toString(thisValue);
    const String *searchStr = meta->toString(argv[0]);
    uint32 pos = str->size();

    if (argc > 1) {
        float64 fpos = meta->toFloat64(argv[1]);
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
        return meta->engine->allocNumber(-1.0);
    return meta->engine->allocNumber((float64)pos);
}

static js2val String_localeCompare(JS2Metadata * /* meta */, const js2val /*thisValue*/, js2val * /*argv*/, uint32 /*argc*/)
{
    return JS2VAL_UNDEFINED;
}

static js2val String_toLowerCase(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val S = STRING_TO_JS2VAL(meta->toString(thisValue));

    String *result = meta->engine->allocStringPtr(JS2VAL_TO_STRING(S));
    for (String::iterator i = result->begin(), end = result->end(); i != end; i++)
        *i = toLower(*i);

    return STRING_TO_JS2VAL(result);
}

static js2val String_toUpperCase(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val S = STRING_TO_JS2VAL(meta->toString(thisValue));

    String *result = meta->engine->allocStringPtr(JS2VAL_TO_STRING(S));
    for (String::iterator i = result->begin(), end = result->end(); i != end; i++)
        *i = toUpper(*i);

    return STRING_TO_JS2VAL(result);
}

/*
 * 15.5.4.13 String.prototype.slice (start, end)
 * 
 * The slice method takes two arguments, start and end, and returns a substring of the result of converting this
 * object to a string, starting from character position start and running to, but not including, character position end (or
 * through the end of the string if end is undefined). If start is negative, it is treated as (sourceLength+start) where
 * sourceLength is the length of the string. If end is negative, it is treated as (sourceLength+end) where sourceLength
 * is the length of the string. The result is a string value, not a String object. 
 * 
 *  The following steps are taken:
 * 1. Call ToString, giving it the this value as its argument.
 * 2. Compute the number of characters in Result(1).
 * 3. Call ToInteger(start).
 * 4. If end is undefined, use Result(2); else use ToInteger(end).
 * 5. If Result(3) is negative, use max(Result(2)+Result(3),0); else use min(Result(3),Result(2)).
 * 6. If Result(4) is negative, use max(Result(2)+Result(4),0); else use min(Result(4),Result(2)).
 * 7. Compute max(Result(6)–Result(5),0).
 * 8. Return a string containing Result(7) consecutive characters from Result(1) beginning with the character at
 *       position Result(5).
 */

static js2val String_slice(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *sourceString = meta->toString(thisValue);

    uint32 sourceLength = sourceString->size();
    uint32 start, end;

    if (argc > 0) {
        int32 arg0 = meta->toInteger(argv[0]);
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
        start = 0;      // XXX argc must be > 1 since the length of the function is 1

    if (argc > 1) {
        int32 arg1 = meta->toInteger(argv[1]);
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
        return STRING_TO_JS2VAL(meta->engine->Empty_StringAtom);
    return meta->engine->allocString(sourceString->substr(start, end - start));
}

/*
 * 15.5.4.15 String.prototype.substring (start, end)
 * The substring method takes two arguments, start and end, and returns a substring of the result of converting this
 * object to a string, starting from character position start and running to, but not including, character position end of
 * the string (or through the end of the string is end is undefined). The result is a string value, not a String object.
 * If either argument is NaN or negative, it is replaced with zero; if either argument is larger than the length of the
 * string, it is replaced with the length of the string.
 * If start is larger than end, they are swapped.
 * 
 *   The following steps are taken:
 * 1. Call ToString, giving it the this value as its argument.
 * 2. Compute the number of characters in Result(1).
 * 3. Call ToInteger(start).
 * 4. If end is undefined, use Result(2); else use ToInteger(end).
 * 5. Compute min(max(Result(3), 0), Result(2)).
 * 6. Compute min(max(Result(4), 0), Result(2)).
 * 7. Compute min(Result(5), Result(6)).
 * 8. Compute max(Result(5), Result(6)).
 * 9. Return a string whose length is the difference between Result(8) and Result(7), containing characters from
 *       Result(1), namely the characters with indices Result(7) through Result(8)-1, in ascending order.
 */

static js2val String_substring(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    const String *sourceString = meta->toString(thisValue);

    uint32 sourceLength = sourceString->size();
    uint32 start, end;

    if (argc > 0) {
        float64 farg0 = meta->toFloat64(argv[0]);
        if (JSDOUBLE_IS_NaN(farg0) || (farg0 < 0))
            start = 0;
        else {
            if (!JSDOUBLE_IS_FINITE(farg0))
                start = sourceLength;
            else {
                start = JS2Engine::float64toUInt32(farg0);
                if (start > sourceLength)
                    start = sourceLength;
            }
        }
    }
    else
        start = 0;

    if (argc > 1) {
        float64 farg1 = meta->toFloat64(argv[1]);
        if (JSDOUBLE_IS_NaN(farg1) || (farg1 < 0))
            end = 0;
        else {
            if (!JSDOUBLE_IS_FINITE(farg1))
                end = sourceLength;
            else {
                end = JS2Engine::float64toUInt32(farg1);
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
        
    return meta->engine->allocString(sourceString->substr(start, end - start));
}

void initStringObject(JS2Metadata *meta)
{

    typedef struct {
        char *name;
        uint16 length;
        NativeCode *code;
    } PrototypeFunction;

    PrototypeFunction prototypeFunctions[] =
    {
        { "toString",           0, String_toString },
        { "valueOf",            0, String_valueOf },
        { "charAt",             1, String_charAt },
        { "charCodeAt",         1, String_charCodeAt },
        { "concat",             1, String_concat },
        { "indexOf",            2, String_indexOf },   // XXX ECMA spec says 1, but tests want 2 XXX
        { "lastIndexOf",        2, String_lastIndexOf },   // XXX ECMA spec says 1, but tests want 2 XXX
        { "localeCompare",      1, String_localeCompare },
        { "match",              1, String_match },
        { "replace",            2, String_replace },
        { "search",             1, String_search },
        { "slice",              2, String_slice },
        { "split",              1, String_split },         // XXX ECMA spec says 2, but tests want 1 XXX
        { "substring",          2, String_substring },
        { "toSource",           0, String_toString },
        { "toLocaleUpperCase",  0, String_toUpperCase },  // (sic)
        { "toLocaleLowerCase",  0, String_toLowerCase },  // (sic)
        { "toUpperCase",        0, String_toUpperCase },
        { "toLowerCase",        0, String_toLowerCase },
        { NULL }
    };

    NamespaceList publicNamespaceList;
    publicNamespaceList.push_back(meta->publicNamespace);

    meta->stringClass->construct = String_Constructor;

    PrototypeFunction *pf = &prototypeFunctions[0];
    while (pf->name) {
        SimpleInstance *fInst = new SimpleInstance(meta->functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code);
/*
XXX not prototype object function properties, like ECMA3, but members of the String class
        meta->writeDynamicProperty(meta->stringClass->prototype, new Multiname(meta->world.identifiers[pf->name], meta->publicNamespace), true, OBJECT_TO_JS2VAL(fInst), RunPhase);
*/
        InstanceMember *m = new InstanceMethod(fInst);
        meta->defineInstanceMember(meta->stringClass, &meta->cxt, &meta->world.identifiers[pf->name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, m, 0);
        pf++;
    }


}

}
}
