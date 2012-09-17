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
    macro(etago, "</") \
    macro(functionNamespaceURI, "@mozilla.org/js/function") \
    macro(namespace, "namespace") \
    macro(ptagc, "/>") \
    macro(qualifier, "::") \
    macro(space, " ") \
    macro(stago, "<") \
    macro(star, "*") \
    macro(starQualifier, "*::") \
    macro(tagc, ">") \
    macro(XMLList, "XMLList")
#else
#define FOR_EACH_XML_ONLY_NAME(macro) /* nothing */
#endif /* JS_HAS_XML_SUPPORT */

#define FOR_EACH_COMMON_PROPERTYNAME(macro) \
    macro(anonymous, "anonymous") \
    macro(apply, "apply") \
    macro(arguments, "arguments") \
    macro(buffer, "buffer") \
    macro(builder, "builder") \
    macro(byteLength, "byteLength") \
    macro(byteOffset, "byteOffset") \
    macro(BYTES_PER_ELEMENT, "BYTES_PER_ELEMENT") \
    macro(call, "call") \
    macro(callee, "callee") \
    macro(caller, "caller") \
    macro(_CallFunction, "_CallFunction") \
    macro(classPrototype, "prototype") \
    macro(columnNumber, "columnNumber") \
    macro(configurable, "configurable") \
    macro(construct, "construct") \
    macro(constructor, "constructor") \
    macro(decodeURI, "decodeURI") \
    macro(decodeURIComponent, "decodeURIComponent") \
    macro(defineProperty, "defineProperty") \
    macro(defineGetter, "__defineGetter__") \
    macro(defineSetter, "__defineSetter__") \
    macro(delete, "delete") \
    macro(deleteProperty, "deleteProperty") \
    macro(each, "each") \
    macro(empty, "") \
    macro(encodeURI, "encodeURI") \
    macro(encodeURIComponent, "encodeURIComponent") \
    macro(enumerable, "enumerable") \
    macro(enumerate, "enumerate") \
    macro(escape, "escape") \
    macro(eval, "eval") \
    macro(false, "false") \
    macro(fileName, "fileName") \
    macro(fix, "fix") \
    macro(get, "get") \
    macro(getOwnPropertyDescriptor, "getOwnPropertyDescriptor") \
    macro(getOwnPropertyNames, "getOwnPropertyNames") \
    macro(getPropertyDescriptor, "getPropertyDescriptor") \
    macro(global, "global") \
    macro(has, "has") \
    macro(hasOwn, "hasOwn") \
    macro(hasOwnProperty, "hasOwnProperty") \
    macro(ignoreCase, "ignoreCase") \
    macro(index, "index") \
    macro(innermost, "innermost") \
    macro(input, "input") \
    macro(isFinite, "isFinite") \
    macro(isNaN, "isNaN") \
    macro(isPrototypeOf, "isPrototypeOf") \
    macro(isXMLName, "isXMLName") \
    macro(iterate, "iterate") \
    macro(Infinity, "Infinity") \
    macro(iterator, "iterator") \
    macro(iteratorIntrinsic, "__iterator__") \
    macro(join, "join") \
    macro(keys, "keys") \
    macro(lastIndex, "lastIndex") \
    macro(length, "length") \
    macro(line, "line") \
    macro(lineNumber, "lineNumber") \
    macro(loc, "loc") \
    macro(lookupGetter, "__lookupGetter__") \
    macro(lookupSetter, "__lookupSetter__") \
    macro(message, "message") \
    macro(multiline, "multiline") \
    macro(name, "name") \
    macro(NaN, "NaN") \
    macro(next, "next") \
    macro(noSuchMethod, "__noSuchMethod__") \
    macro(objectNull, "[object Null]") \
    macro(objectUndefined, "[object Undefined]") \
    macro(of, "of") \
    macro(parseFloat, "parseFloat") \
    macro(parseInt, "parseInt") \
    macro(propertyIsEnumerable, "propertyIsEnumerable") \
    macro(proto, "__proto__") \
    macro(return, "return") \
    macro(set, "set") \
    macro(shape, "shape") \
    macro(source, "source") \
    macro(stack, "stack") \
    macro(sticky, "sticky") \
    macro(test, "test") \
    macro(throw, "throw") \
    macro(toGMTString, "toGMTString") \
    macro(toISOString, "toISOString") \
    macro(toJSON, "toJSON") \
    macro(toLocaleString, "toLocaleString") \
    macro(toSource, "toSource") \
    macro(toString, "toString") \
    macro(toUTCString, "toUTCString") \
    macro(true, "true") \
    macro(unescape, "unescape") \
    macro(uneval, "uneval") \
    macro(unwatch, "unwatch") \
    macro(url, "url") \
    macro(useStrict, "use strict") \
    macro(value, "value") \
    macro(valueOf, "valueOf") \
    macro(var, "var") \
    macro(void0, "(void 0)") \
    macro(watch, "watch") \
    macro(writable, "writable") \
    /* Type names must be contiguous and ordered; see js::TypeName. */ \
    macro(undefined, "undefined") \
    macro(object, "object") \
    macro(function, "function") \
    macro(string, "string") \
    macro(number, "number") \
    macro(boolean, "boolean") \
    macro(null, "null") \
    macro(xml, "xml") \
    FOR_EACH_XML_ONLY_NAME(macro)

#endif /* CommonPropertyNames_h__ */
