/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating all cached property names. */

#ifndef vm_CommonPropertyNames_h
#define vm_CommonPropertyNames_h

#include "jsprototypes.h"

#define FOR_EACH_COMMON_PROPERTYNAME(macro) \
    macro(add, add, "add") \
    macro(allowContentIter, allowContentIter, "allowContentIter") \
    macro(anonymous, anonymous, "anonymous") \
    macro(Any, Any, "Any") \
    macro(apply, apply, "apply") \
    macro(arguments, arguments, "arguments") \
    macro(ArrayBufferSpecies, ArrayBufferSpecies, "ArrayBufferSpecies") \
    macro(ArrayIterator, ArrayIterator, "Array Iterator") \
    macro(ArrayIteratorNext, ArrayIteratorNext, "ArrayIteratorNext") \
    macro(ArraySpecies, ArraySpecies, "ArraySpecies") \
    macro(ArraySpeciesCreate, ArraySpeciesCreate, "ArraySpeciesCreate") \
    macro(ArrayToLocaleString, ArrayToLocaleString, "ArrayToLocaleString") \
    macro(ArrayType, ArrayType, "ArrayType") \
    macro(ArrayValues, ArrayValues, "ArrayValues") \
    macro(ArrayValuesAt, ArrayValuesAt, "ArrayValuesAt") \
    macro(as, as, "as") \
    macro(Async, Async, "Async") \
    macro(AsyncFunction, AsyncFunction, "AsyncFunction") \
    macro(AsyncWrapped, AsyncWrapped, "AsyncWrapped") \
    macro(async, async, "async") \
    macro(await, await, "await") \
    macro(Bool8x16, Bool8x16, "Bool8x16") \
    macro(Bool16x8, Bool16x8, "Bool16x8") \
    macro(Bool32x4, Bool32x4, "Bool32x4") \
    macro(Bool64x2, Bool64x2, "Bool64x2") \
    macro(boundWithSpace, boundWithSpace, "bound ") \
    macro(breakdown, breakdown, "breakdown") \
    macro(buffer, buffer, "buffer") \
    macro(builder, builder, "builder") \
    macro(by, by, "by") \
    macro(byteAlignment, byteAlignment, "byteAlignment") \
    macro(byteLength, byteLength, "byteLength") \
    macro(byteOffset, byteOffset, "byteOffset") \
    macro(bytes, bytes, "bytes") \
    macro(BYTES_PER_ELEMENT, BYTES_PER_ELEMENT, "BYTES_PER_ELEMENT") \
    macro(call, call, "call") \
    macro(callContentFunction, callContentFunction, "callContentFunction") \
    macro(callee, callee, "callee") \
    macro(caller, caller, "caller") \
    macro(callFunction, callFunction, "callFunction") \
    macro(caseFirst, caseFirst, "caseFirst") \
    macro(class_, class_, "class") \
    macro(close, close, "close") \
    macro(Collator, Collator, "Collator") \
    macro(collections, collections, "collections") \
    macro(columnNumber, columnNumber, "columnNumber") \
    macro(comma, comma, ",") \
    macro(compare, compare, "compare") \
    macro(configurable, configurable, "configurable") \
    macro(construct, construct, "construct") \
    macro(constructContentFunction, constructContentFunction, "constructContentFunction") \
    macro(constructor, constructor, "constructor") \
    macro(ConvertAndCopyTo, ConvertAndCopyTo, "ConvertAndCopyTo") \
    macro(copyWithin, copyWithin, "copyWithin") \
    macro(count, count, "count") \
    macro(CreateResolvingFunctions, CreateResolvingFunctions, "CreateResolvingFunctions") \
    macro(currency, currency, "currency") \
    macro(currencyDisplay, currencyDisplay, "currencyDisplay") \
    macro(DateTimeFormat, DateTimeFormat, "DateTimeFormat") \
    macro(day, day, "day") \
    macro(dayPeriod, dayPeriod, "dayPeriod") \
    macro(decimal, decimal, "decimal") \
    macro(decodeURI, decodeURI, "decodeURI") \
    macro(decodeURIComponent, decodeURIComponent, "decodeURIComponent") \
    macro(DefaultBaseClassConstructor, DefaultBaseClassConstructor, "DefaultBaseClassConstructor") \
    macro(DefaultDerivedClassConstructor, DefaultDerivedClassConstructor, "DefaultDerivedClassConstructor") \
    macro(default_, default_, "default") \
    macro(defineGetter, defineGetter, "__defineGetter__") \
    macro(defineProperty, defineProperty, "defineProperty") \
    macro(defineSetter, defineSetter, "__defineSetter__") \
    macro(delete, delete_, "delete") \
    macro(deleteProperty, deleteProperty, "deleteProperty") \
    macro(displayURL, displayURL, "displayURL") \
    macro(done, done, "done") \
    macro(dotGenerator, dotGenerator, ".generator") \
    macro(dotThis, dotThis, ".this") \
    macro(each, each, "each") \
    macro(elementType, elementType, "elementType") \
    macro(empty, empty, "") \
    macro(emptyRegExp, emptyRegExp, "(?:)") \
    macro(encodeURI, encodeURI, "encodeURI") \
    macro(encodeURIComponent, encodeURIComponent, "encodeURIComponent") \
    macro(endTimestamp, endTimestamp, "endTimestamp") \
    macro(entries, entries, "entries") \
    macro(enumerable, enumerable, "enumerable") \
    macro(enumerate, enumerate, "enumerate") \
    macro(era, era, "era") \
    macro(ErrorToStringWithTrailingNewline, ErrorToStringWithTrailingNewline, "ErrorToStringWithTrailingNewline") \
    macro(escape, escape, "escape") \
    macro(eval, eval, "eval") \
    macro(exec, exec, "exec") \
    macro(false, false_, "false") \
    macro(fieldOffsets, fieldOffsets, "fieldOffsets") \
    macro(fieldTypes, fieldTypes, "fieldTypes") \
    macro(fileName, fileName, "fileName") \
    macro(fill, fill, "fill") \
    macro(find, find, "find") \
    macro(findIndex, findIndex, "findIndex") \
    macro(firstDayOfWeek, firstDayOfWeek, "firstDayOfWeek") \
    macro(fix, fix, "fix") \
    macro(flags, flags, "flags") \
    macro(float32, float32, "float32") \
    macro(Float32x4, Float32x4, "Float32x4") \
    macro(float64, float64, "float64") \
    macro(Float64x2, Float64x2, "Float64x2") \
    macro(forceInterpreter, forceInterpreter, "forceInterpreter") \
    macro(forEach, forEach, "forEach") \
    macro(format, format, "format") \
    macro(formatToParts, formatToParts, "formatToParts") \
    macro(fraction, fraction, "fraction") \
    macro(frame, frame, "frame") \
    macro(from, from, "from") \
    macro(fulfilled, fulfilled, "fulfilled") \
    macro(futexNotEqual, futexNotEqual, "not-equal") \
    macro(futexOK, futexOK, "ok") \
    macro(futexTimedOut, futexTimedOut, "timed-out") \
    macro(gcCycleNumber, gcCycleNumber, "gcCycleNumber") \
    macro(Generator, Generator, "Generator") \
    macro(GeneratorFunction, GeneratorFunction, "GeneratorFunction") \
    macro(get, get, "get") \
    macro(getInternals, getInternals, "getInternals") \
    macro(getOwnPropertyDescriptor, getOwnPropertyDescriptor, "getOwnPropertyDescriptor") \
    macro(getOwnPropertyNames, getOwnPropertyNames, "getOwnPropertyNames") \
    macro(getPrefix, getPrefix, "get ") \
    macro(getPropertyDescriptor, getPropertyDescriptor, "getPropertyDescriptor") \
    macro(getPrototypeOf, getPrototypeOf, "getPrototypeOf") \
    macro(global, global, "global") \
    macro(group, group, "group") \
    macro(Handle, Handle, "Handle") \
    macro(has, has, "has") \
    macro(hasOwn, hasOwn, "hasOwn") \
    macro(hasOwnProperty, hasOwnProperty, "hasOwnProperty") \
    macro(hour, hour, "hour") \
    macro(ignoreCase, ignoreCase, "ignoreCase") \
    macro(ignorePunctuation, ignorePunctuation, "ignorePunctuation") \
    macro(includes, includes, "includes") \
    macro(incumbentGlobal, incumbentGlobal, "incumbentGlobal") \
    macro(index, index, "index") \
    macro(infinity, infinity, "infinity") \
    macro(Infinity, Infinity, "Infinity") \
    macro(InitializeCollator, InitializeCollator, "InitializeCollator") \
    macro(InitializeDateTimeFormat, InitializeDateTimeFormat, "InitializeDateTimeFormat") \
    macro(InitializeNumberFormat, InitializeNumberFormat, "InitializeNumberFormat") \
    macro(InitializePluralRules, InitializePluralRules, "InitializePluralRules") \
    macro(innermost, innermost, "innermost") \
    macro(inNursery, inNursery, "inNursery") \
    macro(input, input, "input") \
    macro(int8, int8, "int8") \
    macro(int16, int16, "int16") \
    macro(int32, int32, "int32") \
    macro(Int8x16, Int8x16, "Int8x16") \
    macro(Int16x8, Int16x8, "Int16x8") \
    macro(Int32x4, Int32x4, "Int32x4") \
    macro(integer, integer, "integer") \
    macro(InterpretGeneratorResume, InterpretGeneratorResume, "InterpretGeneratorResume") \
    macro(isEntryPoint, isEntryPoint, "isEntryPoint") \
    macro(isExtensible, isExtensible, "isExtensible") \
    macro(isFinite, isFinite, "isFinite") \
    macro(isNaN, isNaN, "isNaN") \
    macro(isPrototypeOf, isPrototypeOf, "isPrototypeOf") \
    macro(IterableToList, IterableToList, "IterableToList") \
    macro(iterate, iterate, "iterate") \
    macro(iteratorIntrinsic, iteratorIntrinsic, "__iterator__") \
    macro(join, join, "join") \
    macro(js, js, "js") \
    macro(keys, keys, "keys") \
    macro(label, label, "label") \
    macro(lastIndex, lastIndex, "lastIndex") \
    macro(LegacyGeneratorCloseInternal, LegacyGeneratorCloseInternal, "LegacyGeneratorCloseInternal") \
    macro(length, length, "length") \
    macro(let, let, "let") \
    macro(line, line, "line") \
    macro(lineNumber, lineNumber, "lineNumber") \
    macro(literal, literal, "literal") \
    macro(loc, loc, "loc") \
    macro(locale, locale, "locale") \
    macro(lookupGetter, lookupGetter, "__lookupGetter__") \
    macro(lookupSetter, lookupSetter, "__lookupSetter__") \
    macro(MapConstructorInit, MapConstructorInit, "MapConstructorInit") \
    macro(MapIterator, MapIterator, "Map Iterator") \
    macro(maximumFractionDigits, maximumFractionDigits, "maximumFractionDigits") \
    macro(maximumSignificantDigits, maximumSignificantDigits, "maximumSignificantDigits") \
    macro(message, message, "message") \
    macro(minDays, minDays, "minDays") \
    macro(minimumFractionDigits, minimumFractionDigits, "minimumFractionDigits") \
    macro(minimumIntegerDigits, minimumIntegerDigits, "minimumIntegerDigits") \
    macro(minimumSignificantDigits, minimumSignificantDigits, "minimumSignificantDigits") \
    macro(minusSign, minusSign, "minusSign") \
    macro(minute, minute, "minute") \
    macro(missingArguments, missingArguments, "missingArguments") \
    macro(module, module, "module") \
    macro(Module, Module, "Module") \
    macro(ModuleDeclarationInstantiation, ModuleDeclarationInstantiation, "ModuleDeclarationInstantiation") \
    macro(ModuleEvaluation, ModuleEvaluation, "ModuleEvaluation") \
    macro(month, month, "month") \
    macro(multiline, multiline, "multiline") \
    macro(name, name, "name") \
    macro(nan, nan, "nan") \
    macro(NaN, NaN, "NaN") \
    macro(NegativeInfinity, NegativeInfinity, "-Infinity") \
    macro(new, new_, "new") \
    macro(next, next, "next") \
    macro(NFC, NFC, "NFC") \
    macro(NFD, NFD, "NFD") \
    macro(NFKC, NFKC, "NFKC") \
    macro(NFKD, NFKD, "NFKD") \
    macro(noFilename, noFilename, "noFilename") \
    macro(nonincrementalReason, nonincrementalReason, "nonincrementalReason") \
    macro(noStack, noStack, "noStack") \
    macro(NumberFormat, NumberFormat, "NumberFormat") \
    macro(NumberFormatFormatToParts, NumberFormatFormatToParts, "Intl_NumberFormat_formatToParts") \
    macro(numeric, numeric, "numeric") \
    macro(objectArguments, objectArguments, "[object Arguments]") \
    macro(objectArray, objectArray, "[object Array]") \
    macro(objectBoolean, objectBoolean, "[object Boolean]") \
    macro(objectDate, objectDate, "[object Date]") \
    macro(objectError, objectError, "[object Error]") \
    macro(objectFunction, objectFunction, "[object Function]") \
    macro(objectNull, objectNull, "[object Null]") \
    macro(objectNumber, objectNumber, "[object Number]") \
    macro(objectRegExp, objectRegExp, "[object RegExp]") \
    macro(objects, objects, "objects") \
    macro(objectString, objectString, "[object String]") \
    macro(objectUndefined, objectUndefined, "[object Undefined]") \
    macro(of, of, "of") \
    macro(offset, offset, "offset") \
    macro(optimizedOut, optimizedOut, "optimizedOut") \
    macro(other, other, "other") \
    macro(outOfMemory, outOfMemory, "out of memory") \
    macro(ownKeys, ownKeys, "ownKeys") \
    macro(parseFloat, parseFloat, "parseFloat") \
    macro(parseInt, parseInt, "parseInt") \
    macro(pattern, pattern, "pattern") \
    macro(pending, pending, "pending") \
    macro(PluralRules, PluralRules, "PluralRules") \
    macro(PluralRulesSelect, PluralRulesSelect, "Intl_PluralRules_Select") \
    macro(percentSign, percentSign, "percentSign") \
    macro(plusSign, plusSign, "plusSign") \
    macro(preventExtensions, preventExtensions, "preventExtensions") \
    macro(promise, promise, "promise") \
    macro(propertyIsEnumerable, propertyIsEnumerable, "propertyIsEnumerable") \
    macro(proto, proto, "__proto__") \
    macro(prototype, prototype, "prototype") \
    macro(proxy, proxy, "proxy") \
    macro(raw, raw, "raw") \
    macro(reason, reason, "reason") \
    macro(RegExpBuiltinExec, RegExpBuiltinExec, "RegExpBuiltinExec") \
    macro(RegExpFlagsGetter, RegExpFlagsGetter, "RegExpFlagsGetter") \
    macro(RegExpMatcher, RegExpMatcher, "RegExpMatcher") \
    macro(RegExpSearcher, RegExpSearcher, "RegExpSearcher") \
    macro(RegExpTester, RegExpTester, "RegExpTester") \
    macro(RegExp_prototype_Exec, RegExp_prototype_Exec, "RegExp_prototype_Exec") \
    macro(Reify, Reify, "Reify") \
    macro(reject, reject, "reject") \
    macro(rejected, rejected, "rejected") \
    macro(RequireObjectCoercible, RequireObjectCoercible, "RequireObjectCoercible") \
    macro(resolve, resolve, "resolve") \
    macro(resumeGenerator, resumeGenerator, "resumeGenerator") \
    macro(return, return_, "return") \
    macro(revoke, revoke, "revoke") \
    macro(script, script, "script") \
    macro(scripts, scripts, "scripts") \
    macro(second, second, "second") \
    macro(selfHosted, selfHosted, "self-hosted") \
    macro(sensitivity, sensitivity, "sensitivity") \
    macro(set, set, "set") \
    macro(SetConstructorInit, SetConstructorInit, "SetConstructorInit") \
    macro(SetIterator, SetIterator, "Set Iterator") \
    macro(setPrefix, setPrefix, "set ") \
    macro(setPrototypeOf, setPrototypeOf, "setPrototypeOf") \
    macro(shape, shape, "shape") \
    macro(size, size, "size") \
    macro(source, source, "source") \
    macro(SpeciesConstructor, SpeciesConstructor, "SpeciesConstructor") \
    macro(stack, stack, "stack") \
    macro(star, star, "*") \
    macro(starDefaultStar, starDefaultStar, "*default*") \
    macro(StarGeneratorNext, StarGeneratorNext, "StarGeneratorNext") \
    macro(StarGeneratorThrow, StarGeneratorThrow, "StarGeneratorThrow") \
    macro(startTimestamp, startTimestamp, "startTimestamp") \
    macro(state, state, "state") \
    macro(static, static_, "static") \
    macro(std_Function_apply, std_Function_apply, "std_Function_apply") \
    macro(sticky, sticky, "sticky") \
    macro(StringIterator, StringIterator, "String Iterator") \
    macro(strings, strings, "strings") \
    macro(StructType, StructType, "StructType") \
    macro(style, style, "style") \
    macro(super, super, "super") \
    macro(Symbol_iterator_fun, Symbol_iterator_fun, "[Symbol.iterator]") \
    macro(target, target, "target") \
    macro(test, test, "test") \
    macro(then, then, "then") \
    macro(throw, throw_, "throw") \
    macro(timestamp, timestamp, "timestamp") \
    macro(timeZone, timeZone, "timeZone") \
    macro(timeZoneName, timeZoneName, "timeZoneName") \
    macro(toGMTString, toGMTString, "toGMTString") \
    macro(toISOString, toISOString, "toISOString") \
    macro(toJSON, toJSON, "toJSON") \
    macro(toLocaleString, toLocaleString, "toLocaleString") \
    macro(toSource, toSource, "toSource") \
    macro(toString, toString, "toString") \
    macro(toUTCString, toUTCString, "toUTCString") \
    macro(true, true_, "true") \
    macro(type, type, "type") \
    macro(uint8, uint8, "uint8") \
    macro(uint8Clamped, uint8Clamped, "uint8Clamped") \
    macro(uint16, uint16, "uint16") \
    macro(uint32, uint32, "uint32") \
    macro(Uint8x16, Uint8x16, "Uint8x16") \
    macro(Uint16x8, Uint16x8, "Uint16x8") \
    macro(Uint32x4, Uint32x4, "Uint32x4") \
    macro(unescape, unescape, "unescape") \
    macro(uneval, uneval, "uneval") \
    macro(unicode, unicode, "unicode") \
    macro(uninitialized, uninitialized, "uninitialized") \
    macro(unsized, unsized, "unsized") \
    macro(unwatch, unwatch, "unwatch") \
    macro(UnwrapAndCallRegExpBuiltinExec, UnwrapAndCallRegExpBuiltinExec, "UnwrapAndCallRegExpBuiltinExec") \
    macro(url, url, "url") \
    macro(usage, usage, "usage") \
    macro(useAsm, useAsm, "use asm") \
    macro(useGrouping, useGrouping, "useGrouping") \
    macro(useStrict, useStrict, "use strict") \
    macro(value, value, "value") \
    macro(valueOf, valueOf, "valueOf") \
    macro(values, values, "values") \
    macro(var, var, "var") \
    macro(variable, variable, "variable") \
    macro(void0, void0, "(void 0)") \
    macro(wasm, wasm, "wasm") \
    macro(wasmcall, wasmcall, "wasmcall") \
    macro(watch, watch, "watch") \
    macro(WeakMapConstructorInit, WeakMapConstructorInit, "WeakMapConstructorInit") \
    macro(WeakSetConstructorInit, WeakSetConstructorInit, "WeakSetConstructorInit") \
    macro(WeakSet_add, WeakSet_add, "WeakSet_add") \
    macro(weekday, weekday, "weekday") \
    macro(weekendEnd, weekendEnd, "weekendEnd") \
    macro(weekendStart, weekendStart, "weekendStart") \
    macro(writable, writable, "writable") \
    macro(year, year, "year") \
    macro(yield, yield, "yield") \
    /* Type names must be contiguous and ordered; see js::TypeName. */ \
    macro(undefined, undefined, "undefined") \
    macro(object, object, "object") \
    macro(function, function, "function") \
    macro(string, string, "string") \
    macro(number, number, "number") \
    macro(boolean, boolean, "boolean") \
    macro(null, null, "null") \
    macro(symbol, symbol, "symbol") \

#endif /* vm_CommonPropertyNames_h */
