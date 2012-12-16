/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating all cached property names. */

#ifndef CommonPropertyNames_h__
#define CommonPropertyNames_h__

#include "jsprototypes.h"
#include "jsversion.h"

#if JS_HAS_XML_SUPPORT
#define FOR_EACH_XML_ONLY_NAME(macro) \
    macro(etago, etago, "</") \
    macro(functionNamespaceURI, functionNamespaceURI, "@mozilla.org/js/function") \
    macro(namespace, namespace_, "namespace") \
    macro(ptagc, ptagc, "/>") \
    macro(qualifier, qualifier, "::") \
    macro(space, space, " ") \
    macro(stago, stago, "<") \
    macro(star, star, "*") \
    macro(starQualifier, starQualifier, "*::") \
    macro(tagc, tagc, ">") \
    macro(XMLList, XMLList, "XMLList")
#else
#define FOR_EACH_XML_ONLY_NAME(macro) /* nothing */
#endif /* JS_HAS_XML_SUPPORT */

#define FOR_EACH_COMMON_PROPERTYNAME(macro) \
    macro(anonymous, anonymous, "anonymous") \
    macro(apply, apply, "apply") \
    macro(arguments, arguments, "arguments") \
    macro(buffer, buffer, "buffer") \
    macro(builder, builder, "builder") \
    macro(byteLength, byteLength, "byteLength") \
    macro(byteOffset, byteOffset, "byteOffset") \
    macro(BYTES_PER_ELEMENT, BYTES_PER_ELEMENT, "BYTES_PER_ELEMENT") \
    macro(call, call, "call") \
    macro(callee, callee, "callee") \
    macro(caller, caller, "caller") \
    macro(callFunction, callFunction, "callFunction") \
    macro(classPrototype, classPrototype, "prototype") \
    macro(Collator, Collator, "Collator") \
    macro(columnNumber, columnNumber, "columnNumber") \
    macro(configurable, configurable, "configurable") \
    macro(construct, construct, "construct") \
    macro(constructor, constructor, "constructor") \
    macro(DateTimeFormat, DateTimeFormat, "DateTimeFormat") \
    macro(decodeURI, decodeURI, "decodeURI") \
    macro(decodeURIComponent, decodeURIComponent, "decodeURIComponent") \
    macro(defineProperty, defineProperty, "defineProperty") \
    macro(defineGetter, defineGetter, "__defineGetter__") \
    macro(defineSetter, defineSetter, "__defineSetter__") \
    macro(delete, delete_, "delete") \
    macro(deleteProperty, deleteProperty, "deleteProperty") \
    macro(each, each, "each") \
    macro(empty, empty, "") \
    macro(encodeURI, encodeURI, "encodeURI") \
    macro(encodeURIComponent, encodeURIComponent, "encodeURIComponent") \
    macro(enumerable, enumerable, "enumerable") \
    macro(enumerate, enumerate, "enumerate") \
    macro(escape, escape, "escape") \
    macro(eval, eval, "eval") \
    macro(false, false_, "false") \
    macro(fileName, fileName, "fileName") \
    macro(fix, fix, "fix") \
    macro(get, get, "get") \
    macro(getOwnPropertyDescriptor, getOwnPropertyDescriptor, "getOwnPropertyDescriptor") \
    macro(getOwnPropertyNames, getOwnPropertyNames, "getOwnPropertyNames") \
    macro(getPropertyDescriptor, getPropertyDescriptor, "getPropertyDescriptor") \
    macro(global, global, "global") \
    macro(has, has, "has") \
    macro(hasOwn, hasOwn, "hasOwn") \
    macro(hasOwnProperty, hasOwnProperty, "hasOwnProperty") \
    macro(ignoreCase, ignoreCase, "ignoreCase") \
    macro(index, index, "index") \
    macro(InitializeCollator, InitializeCollator, "intl_InitializeCollator") \
    macro(InitializeDateTimeFormat, InitializeDateTimeFormat, "intl_InitializeDateTimeFormat") \
    macro(InitializeNumberFormat, InitializeNumberFormat, "intl_InitializeNumberFormat") \
    macro(innermost, innermost, "innermost") \
    macro(input, input, "input") \
    macro(isFinite, isFinite, "isFinite") \
    macro(isNaN, isNaN, "isNaN") \
    macro(isPrototypeOf, isPrototypeOf, "isPrototypeOf") \
    macro(isXMLName, isXMLName, "isXMLName") \
    macro(iterate, iterate, "iterate") \
    macro(Infinity, Infinity, "Infinity") \
    macro(iterator, iterator, "iterator") \
    macro(iteratorIntrinsic, iteratorIntrinsic, "__iterator__") \
    macro(join, join, "join") \
    macro(keys, keys, "keys") \
    macro(lastIndex, lastIndex, "lastIndex") \
    macro(length, length, "length") \
    macro(line, line, "line") \
    macro(lineNumber, lineNumber, "lineNumber") \
    macro(loc, loc, "loc") \
    macro(lookupGetter, lookupGetter, "__lookupGetter__") \
    macro(lookupSetter, lookupSetter, "__lookupSetter__") \
    macro(message, message, "message") \
    macro(multiline, multiline, "multiline") \
    macro(name, name, "name") \
    macro(NaN, NaN, "NaN") \
    macro(next, next, "next") \
    macro(noSuchMethod, noSuchMethod, "__noSuchMethod__") \
    macro(NumberFormat, NumberFormat, "NumberFormat") \
    macro(objectNull, objectNull, "[object Null]") \
    macro(objectUndefined, objectUndefined, "[object Undefined]") \
    macro(of, of, "of") \
    macro(parseFloat, parseFloat, "parseFloat") \
    macro(parseInt, parseInt, "parseInt") \
    macro(propertyIsEnumerable, propertyIsEnumerable, "propertyIsEnumerable") \
    macro(proto, proto, "__proto__") \
    macro(return, return_, "return") \
    macro(set, set, "set") \
    macro(shape, shape, "shape") \
    macro(source, source, "source") \
    macro(stack, stack, "stack") \
    macro(sticky, sticky, "sticky") \
    macro(test, test, "test") \
    macro(throw, throw_, "throw") \
    macro(toGMTString, toGMTString, "toGMTString") \
    macro(toISOString, toISOString, "toISOString") \
    macro(toJSON, toJSON, "toJSON") \
    macro(toLocaleString, toLocaleString, "toLocaleString") \
    macro(toSource, toSource, "toSource") \
    macro(toString, toString, "toString") \
    macro(toUTCString, toUTCString, "toUTCString") \
    macro(true, true_, "true") \
    macro(unescape, unescape, "unescape") \
    macro(uneval, uneval, "uneval") \
    macro(unwatch, unwatch, "unwatch") \
    macro(url, url, "url") \
    macro(useStrict, useStrict, "use strict") \
    macro(value, value, "value") \
    macro(valueOf, valueOf, "valueOf") \
    macro(var, var, "var") \
    macro(void0, void0, "(void 0)") \
    macro(watch, watch, "watch") \
    macro(writable, writable, "writable") \
    /* Type names must be contiguous and ordered; see js::TypeName. */ \
    macro(undefined, undefined, "undefined") \
    macro(object, object, "object") \
    macro(function, function, "function") \
    macro(string, string, "string") \
    macro(number, number, "number") \
    macro(boolean, boolean, "boolean") \
    macro(null, null, "null") \
    macro(xml, xml, "xml") \
    FOR_EACH_XML_ONLY_NAME(macro)

#endif /* CommonPropertyNames_h__ */
