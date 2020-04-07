/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating all cached property names. */

#ifndef vm_CommonPropertyNames_h
#define vm_CommonPropertyNames_h

#include "js/ProtoKey.h"

#define FOR_EACH_COMMON_PROPERTYNAME(MACRO)                                    \
  MACRO(abort, abort, "abort")                                                 \
  MACRO(add, add, "add")                                                       \
  MACRO(allowContentIter, allowContentIter, "allowContentIter")                \
  MACRO(anonymous, anonymous, "anonymous")                                     \
  MACRO(Any, Any, "Any")                                                       \
  MACRO(apply, apply, "apply")                                                 \
  MACRO(args, args, "args")                                                    \
  MACRO(arguments, arguments, "arguments")                                     \
  MACRO(ArrayBufferSpecies, ArrayBufferSpecies, "$ArrayBufferSpecies")         \
  MACRO(ArrayIterator, ArrayIterator, "Array Iterator")                        \
  MACRO(ArrayIteratorNext, ArrayIteratorNext, "ArrayIteratorNext")             \
  MACRO(ArraySort, ArraySort, "ArraySort")                                     \
  MACRO(ArraySpecies, ArraySpecies, "$ArraySpecies")                           \
  MACRO(ArraySpeciesCreate, ArraySpeciesCreate, "ArraySpeciesCreate")          \
  MACRO(ArrayToLocaleString, ArrayToLocaleString, "ArrayToLocaleString")       \
  MACRO(ArrayType, ArrayType, "ArrayType")                                     \
  MACRO(ArrayValues, ArrayValues, "$ArrayValues")                              \
  MACRO(as, as, "as")                                                          \
  MACRO(Async, Async, "Async")                                                 \
  MACRO(AsyncFromSyncIterator, AsyncFromSyncIterator,                          \
        "Async-from-Sync Iterator")                                            \
  MACRO(AsyncFunctionNext, AsyncFunctionNext, "AsyncFunctionNext")             \
  MACRO(AsyncFunctionThrow, AsyncFunctionThrow, "AsyncFunctionThrow")          \
  MACRO(AsyncGenerator, AsyncGenerator, "AsyncGenerator")                      \
  MACRO(AsyncGeneratorNext, AsyncGeneratorNext, "AsyncGeneratorNext")          \
  MACRO(AsyncGeneratorReturn, AsyncGeneratorReturn, "AsyncGeneratorReturn")    \
  MACRO(AsyncGeneratorThrow, AsyncGeneratorThrow, "AsyncGeneratorThrow")       \
  MACRO(AsyncWrapped, AsyncWrapped, "AsyncWrapped")                            \
  MACRO(async, async, "async")                                                 \
  MACRO(autoAllocateChunkSize, autoAllocateChunkSize, "autoAllocateChunkSize") \
  MACRO(await, await, "await")                                                 \
  MACRO(bigint64, bigint64, "bigint64")                                        \
  MACRO(biguint64, biguint64, "biguint64")                                     \
  MACRO(Bool8x16, Bool8x16, "Bool8x16")                                        \
  MACRO(Bool16x8, Bool16x8, "Bool16x8")                                        \
  MACRO(Bool32x4, Bool32x4, "Bool32x4")                                        \
  MACRO(Bool64x2, Bool64x2, "Bool64x2")                                        \
  MACRO(boundWithSpace, boundWithSpace, "bound ")                              \
  MACRO(break, break_, "break")                                                \
  MACRO(breakdown, breakdown, "breakdown")                                     \
  MACRO(buffer, buffer, "buffer")                                              \
  MACRO(builder, builder, "builder")                                           \
  MACRO(by, by, "by")                                                          \
  MACRO(byob, byob, "byob")                                                    \
  MACRO(byteAlignment, byteAlignment, "byteAlignment")                         \
  MACRO(byteLength, byteLength, "byteLength")                                  \
  MACRO(byteOffset, byteOffset, "byteOffset")                                  \
  MACRO(bytes, bytes, "bytes")                                                 \
  MACRO(BYTES_PER_ELEMENT, BYTES_PER_ELEMENT, "BYTES_PER_ELEMENT")             \
  MACRO(calendar, calendar, "calendar")                                        \
  MACRO(call, call, "call")                                                    \
  MACRO(callContentFunction, callContentFunction, "callContentFunction")       \
  MACRO(callee, callee, "callee")                                              \
  MACRO(caller, caller, "caller")                                              \
  MACRO(callFunction, callFunction, "callFunction")                            \
  MACRO(cancel, cancel, "cancel")                                              \
  MACRO(case, case_, "case")                                                   \
  MACRO(caseFirst, caseFirst, "caseFirst")                                     \
  MACRO(catch, catch_, "catch")                                                \
  MACRO(class, class_, "class")                                                \
  MACRO(cleanupSome, cleanupSome, "cleanupSome")                               \
  MACRO(close, close, "close")                                                 \
  MACRO(collation, collation, "collation")                                     \
  MACRO(collections, collections, "collections")                               \
  MACRO(columnNumber, columnNumber, "columnNumber")                            \
  MACRO(comma, comma, ",")                                                     \
  MACRO(compare, compare, "compare")                                           \
  MACRO(configurable, configurable, "configurable")                            \
  MACRO(const, const_, "const")                                                \
  MACRO(construct, construct, "construct")                                     \
  MACRO(constructContentFunction, constructContentFunction,                    \
        "constructContentFunction")                                            \
  MACRO(constructor, constructor, "constructor")                               \
  MACRO(continue, continue_, "continue")                                       \
  MACRO(ConvertAndCopyTo, ConvertAndCopyTo, "ConvertAndCopyTo")                \
  MACRO(CopyDataProperties, CopyDataProperties, "CopyDataProperties")          \
  MACRO(CopyDataPropertiesUnfiltered, CopyDataPropertiesUnfiltered,            \
        "CopyDataPropertiesUnfiltered")                                        \
  MACRO(copyWithin, copyWithin, "copyWithin")                                  \
  MACRO(compact, compact, "compact")                                           \
  MACRO(compactDisplay, compactDisplay, "compactDisplay")                      \
  MACRO(count, count, "count")                                                 \
  MACRO(CreateResolvingFunctions, CreateResolvingFunctions,                    \
        "CreateResolvingFunctions")                                            \
  MACRO(currency, currency, "currency")                                        \
  MACRO(currencyDisplay, currencyDisplay, "currencyDisplay")                   \
  MACRO(currencySign, currencySign, "currencySign")                            \
  MACRO(day, day, "day")                                                       \
  MACRO(dayPeriod, dayPeriod, "dayPeriod")                                     \
  MACRO(debugger, debugger, "debugger")                                        \
  MACRO(decimal, decimal, "decimal")                                           \
  MACRO(decodeURI, decodeURI, "decodeURI")                                     \
  MACRO(decodeURIComponent, decodeURIComponent, "decodeURIComponent")          \
  MACRO(DefaultBaseClassConstructor, DefaultBaseClassConstructor,              \
        "DefaultBaseClassConstructor")                                         \
  MACRO(DefaultDerivedClassConstructor, DefaultDerivedClassConstructor,        \
        "DefaultDerivedClassConstructor")                                      \
  MACRO(default, default_, "default")                                          \
  MACRO(defineGetter, defineGetter, "__defineGetter__")                        \
  MACRO(defineProperty, defineProperty, "defineProperty")                      \
  MACRO(defineSetter, defineSetter, "__defineSetter__")                        \
  MACRO(delete, delete_, "delete")                                             \
  MACRO(deleteProperty, deleteProperty, "deleteProperty")                      \
  MACRO(direction, direction, "direction")                                     \
  MACRO(displayURL, displayURL, "displayURL")                                  \
  MACRO(do, do_, "do")                                                         \
  MACRO(domNode, domNode, "domNode")                                           \
  MACRO(done, done, "done")                                                    \
  MACRO(dotAll, dotAll, "dotAll")                                              \
  MACRO(dotGenerator, dotGenerator, ".generator")                              \
  MACRO(dotThis, dotThis, ".this")                                             \
  MACRO(dotInitializers, dotInitializers, ".initializers")                     \
  MACRO(dotFieldKeys, dotFieldKeys, ".fieldKeys")                              \
  MACRO(dotStaticInitializers, dotStaticInitializers, ".staticInitializers")   \
  MACRO(dotStaticFieldKeys, dotStaticFieldKeys, ".staticFieldKeys")            \
  MACRO(each, each, "each")                                                    \
  MACRO(element, element, "element")                                           \
  MACRO(elementType, elementType, "elementType")                               \
  MACRO(else, else_, "else")                                                   \
  MACRO(empty, empty, "")                                                      \
  MACRO(emptyRegExp, emptyRegExp, "(?:)")                                      \
  MACRO(encodeURI, encodeURI, "encodeURI")                                     \
  MACRO(encodeURIComponent, encodeURIComponent, "encodeURIComponent")          \
  MACRO(endTimestamp, endTimestamp, "endTimestamp")                            \
  MACRO(entries, entries, "entries")                                           \
  MACRO(enum, enum_, "enum")                                                   \
  MACRO(enumerable, enumerable, "enumerable")                                  \
  MACRO(enumerate, enumerate, "enumerate")                                     \
  MACRO(era, era, "era")                                                       \
  MACRO(ErrorToStringWithTrailingNewline, ErrorToStringWithTrailingNewline,    \
        "ErrorToStringWithTrailingNewline")                                    \
  MACRO(escape, escape, "escape")                                              \
  MACRO(eval, eval, "eval")                                                    \
  MACRO(exec, exec, "exec")                                                    \
  MACRO(exponentInteger, exponentInteger, "exponentInteger")                   \
  MACRO(exponentMinusSign, exponentMinusSign, "exponentMinusSign")             \
  MACRO(exponentSeparator, exponentSeparator, "exponentSeparator")             \
  MACRO(export, export_, "export")                                             \
  MACRO(extends, extends, "extends")                                           \
  MACRO(false, false_, "false")                                                \
  MACRO(fieldOffsets, fieldOffsets, "fieldOffsets")                            \
  MACRO(fieldTypes, fieldTypes, "fieldTypes")                                  \
  MACRO(fileName, fileName, "fileName")                                        \
  MACRO(fill, fill, "fill")                                                    \
  MACRO(finally, finally_, "finally")                                          \
  MACRO(find, find, "find")                                                    \
  MACRO(findIndex, findIndex, "findIndex")                                     \
  MACRO(firstDayOfWeek, firstDayOfWeek, "firstDayOfWeek")                      \
  MACRO(fix, fix, "fix")                                                       \
  MACRO(flags, flags, "flags")                                                 \
  MACRO(flat, flat, "flat")                                                    \
  MACRO(flatMap, flatMap, "flatMap")                                           \
  MACRO(float32, float32, "float32")                                           \
  MACRO(Float32x4, Float32x4, "Float32x4")                                     \
  MACRO(float64, float64, "float64")                                           \
  MACRO(Float64x2, Float64x2, "Float64x2")                                     \
    MACRO(for, for_, "for")                                                    \
  MACRO(forceInterpreter, forceInterpreter, "forceInterpreter")                \
  MACRO(forEach, forEach, "forEach")                                           \
  MACRO(format, format, "format")                                              \
  MACRO(fraction, fraction, "fraction")                                        \
  MACRO(fractionalSecond, fractionalSecond, "fractionalSecond")                \
  MACRO(frame, frame, "frame")                                                 \
  MACRO(from, from, "from")                                                    \
  MACRO(fulfilled, fulfilled, "fulfilled")                                     \
  MACRO(futexNotEqual, futexNotEqual, "not-equal")                             \
  MACRO(futexOK, futexOK, "ok")                                                \
  MACRO(futexTimedOut, futexTimedOut, "timed-out")                             \
  MACRO(gcCycleNumber, gcCycleNumber, "gcCycleNumber")                         \
  MACRO(Generator, Generator, "Generator")                                     \
  MACRO(GeneratorNext, GeneratorNext, "GeneratorNext")                         \
  MACRO(GeneratorReturn, GeneratorReturn, "GeneratorReturn")                   \
  MACRO(GeneratorThrow, GeneratorThrow, "GeneratorThrow")                      \
  MACRO(get, get, "get")                                                       \
  MACRO(GetAggregateError, GetAggregateError, "GetAggregateError")             \
  MACRO(GetInternalError, GetInternalError, "GetInternalError")                \
  MACRO(getBigInt64, getBigInt64, "getBigInt64")                               \
  MACRO(getBigUint64, getBigUint64, "getBigUint64")                            \
  MACRO(getInternals, getInternals, "getInternals")                            \
  MACRO(GetModuleNamespace, GetModuleNamespace, "GetModuleNamespace")          \
  MACRO(getOwnPropertyDescriptor, getOwnPropertyDescriptor,                    \
        "getOwnPropertyDescriptor")                                            \
  MACRO(getOwnPropertyNames, getOwnPropertyNames, "getOwnPropertyNames")       \
  MACRO(getPrefix, getPrefix, "get ")                                          \
  MACRO(getPropertySuper, getPropertySuper, "getPropertySuper")                \
  MACRO(getPrototypeOf, getPrototypeOf, "getPrototypeOf")                      \
  MACRO(GetTypeError, GetTypeError, "GetTypeError")                            \
  MACRO(global, global, "global")                                              \
  MACRO(globalThis, globalThis, "globalThis")                                  \
  MACRO(group, group, "group")                                                 \
  MACRO(Handle, Handle, "Handle")                                              \
  MACRO(has, has, "has")                                                       \
  MACRO(hasOwn, hasOwn, "hasOwn")                                              \
  MACRO(hasOwnProperty, hasOwnProperty, "hasOwnProperty")                      \
  MACRO(highWaterMark, highWaterMark, "highWaterMark")                         \
  MACRO(hour, hour, "hour")                                                    \
  MACRO(hourCycle, hourCycle, "hourCycle")                                     \
  MACRO(if, if_, "if")                                                         \
  MACRO(ignoreCase, ignoreCase, "ignoreCase")                                  \
  MACRO(ignorePunctuation, ignorePunctuation, "ignorePunctuation")             \
  MACRO(implements, implements, "implements")                                  \
  MACRO(import, import, "import")                                              \
  MACRO(in, in, "in")                                                          \
  MACRO(includes, includes, "includes")                                        \
  MACRO(incumbentGlobal, incumbentGlobal, "incumbentGlobal")                   \
  MACRO(index, index, "index")                                                 \
  MACRO(infinity, infinity, "infinity")                                        \
  MACRO(Infinity, Infinity, "Infinity")                                        \
  MACRO(initial, initial, "initial")                                           \
  MACRO(InitializeCollator, InitializeCollator, "InitializeCollator")          \
  MACRO(InitializeDateTimeFormat, InitializeDateTimeFormat,                    \
        "InitializeDateTimeFormat")                                            \
  MACRO(InitializeListFormat, InitializeListFormat, "InitializeListFormat")    \
  MACRO(InitializeLocale, InitializeLocale, "InitializeLocale")                \
  MACRO(InitializeNumberFormat, InitializeNumberFormat,                        \
        "InitializeNumberFormat")                                              \
  MACRO(InitializePluralRules, InitializePluralRules, "InitializePluralRules") \
  MACRO(InitializeRelativeTimeFormat, InitializeRelativeTimeFormat,            \
        "InitializeRelativeTimeFormat")                                        \
  MACRO(innermost, innermost, "innermost")                                     \
  MACRO(inNursery, inNursery, "inNursery")                                     \
  MACRO(input, input, "input")                                                 \
  MACRO(instanceof, instanceof, "instanceof")                                  \
  MACRO(int8, int8, "int8")                                                    \
  MACRO(int16, int16, "int16")                                                 \
  MACRO(int32, int32, "int32")                                                 \
  MACRO(Int8x16, Int8x16, "Int8x16")                                           \
  MACRO(Int16x8, Int16x8, "Int16x8")                                           \
  MACRO(Int32x4, Int32x4, "Int32x4")                                           \
  MACRO(integer, integer, "integer")                                           \
  MACRO(interface, interface, "interface")                                     \
  MACRO(InterpretGeneratorResume, InterpretGeneratorResume,                    \
        "InterpretGeneratorResume")                                            \
  MACRO(InvalidDate, InvalidDate, "Invalid Date")                              \
  MACRO(isBreakpoint, isBreakpoint, "isBreakpoint")                            \
  MACRO(isEntryPoint, isEntryPoint, "isEntryPoint")                            \
  MACRO(isExtensible, isExtensible, "isExtensible")                            \
  MACRO(isFinite, isFinite, "isFinite")                                        \
  MACRO(isNaN, isNaN, "isNaN")                                                 \
  MACRO(isPrototypeOf, isPrototypeOf, "isPrototypeOf")                         \
  MACRO(isStepStart, isStepStart, "isStepStart")                               \
  MACRO(IterableToList, IterableToList, "IterableToList")                      \
  MACRO(iterate, iterate, "iterate")                                           \
  MACRO(join, join, "join")                                                    \
  MACRO(js, js, "js")                                                          \
  MACRO(keys, keys, "keys")                                                    \
  MACRO(label, label, "label")                                                 \
  MACRO(language, language, "language")                                        \
  MACRO(lastIndex, lastIndex, "lastIndex")                                     \
  MACRO(length, length, "length")                                              \
  MACRO(let, let, "let")                                                       \
  MACRO(line, line, "line")                                                    \
  MACRO(lineNumber, lineNumber, "lineNumber")                                  \
  MACRO(literal, literal, "literal")                                           \
  MACRO(loc, loc, "loc")                                                       \
  MACRO(locale, locale, "locale")                                              \
  MACRO(lookupGetter, lookupGetter, "__lookupGetter__")                        \
  MACRO(lookupSetter, lookupSetter, "__lookupSetter__")                        \
  MACRO(ltr, ltr, "ltr")                                                       \
  MACRO(MapConstructorInit, MapConstructorInit, "MapConstructorInit")          \
  MACRO(MapIterator, MapIterator, "Map Iterator")                              \
  MACRO(maxColumn, maxColumn, "maxColumn")                                     \
  MACRO(maximumFractionDigits, maximumFractionDigits, "maximumFractionDigits") \
  MACRO(maximumSignificantDigits, maximumSignificantDigits,                    \
        "maximumSignificantDigits")                                            \
  MACRO(maxLine, maxLine, "maxLine")                                           \
  MACRO(maxOffset, maxOffset, "maxOffset")                                     \
  MACRO(message, message, "message")                                           \
  MACRO(meta, meta, "meta")                                                    \
  MACRO(minColumn, minColumn, "minColumn")                                     \
  MACRO(minDays, minDays, "minDays")                                           \
  MACRO(minimumFractionDigits, minimumFractionDigits, "minimumFractionDigits") \
  MACRO(minimumIntegerDigits, minimumIntegerDigits, "minimumIntegerDigits")    \
  MACRO(minimumSignificantDigits, minimumSignificantDigits,                    \
        "minimumSignificantDigits")                                            \
  MACRO(minLine, minLine, "minLine")                                           \
  MACRO(minOffset, minOffset, "minOffset")                                     \
  MACRO(minusSign, minusSign, "minusSign")                                     \
  MACRO(minute, minute, "minute")                                              \
  MACRO(missingArguments, missingArguments, "missingArguments")                \
  MACRO(mode, mode, "mode")                                                    \
  MACRO(module, module, "module")                                              \
  MACRO(Module, Module, "Module")                                              \
  MACRO(ModuleInstantiate, ModuleInstantiate, "ModuleInstantiate")             \
  MACRO(ModuleEvaluate, ModuleEvaluate, "ModuleEvaluate")                      \
  MACRO(month, month, "month")                                                 \
  MACRO(multiline, multiline, "multiline")                                     \
  MACRO(name, name, "name")                                                    \
  MACRO(nan, nan, "nan")                                                       \
  MACRO(NaN, NaN, "NaN")                                                       \
  MACRO(NegativeInfinity, NegativeInfinity, "-Infinity")                       \
  MACRO(new, new_, "new")                                                      \
  MACRO(next, next, "next")                                                    \
  MACRO(NFC, NFC, "NFC")                                                       \
  MACRO(NFD, NFD, "NFD")                                                       \
  MACRO(NFKC, NFKC, "NFKC")                                                    \
  MACRO(NFKD, NFKD, "NFKD")                                                    \
  MACRO(noFilename, noFilename, "noFilename")                                  \
  MACRO(nonincrementalReason, nonincrementalReason, "nonincrementalReason")    \
  MACRO(noStack, noStack, "noStack")                                           \
  MACRO(notation, notation, "notation")                                        \
  MACRO(notes, notes, "notes")                                                 \
  MACRO(numberingSystem, numberingSystem, "numberingSystem")                   \
  MACRO(numeric, numeric, "numeric")                                           \
  MACRO(objectArguments, objectArguments, "[object Arguments]")                \
  MACRO(objectArray, objectArray, "[object Array]")                            \
  MACRO(objectBigInt, objectBigInt, "[object BigInt]")                         \
  MACRO(objectBoolean, objectBoolean, "[object Boolean]")                      \
  MACRO(objectDate, objectDate, "[object Date]")                               \
  MACRO(objectError, objectError, "[object Error]")                            \
  MACRO(objectFunction, objectFunction, "[object Function]")                   \
  MACRO(objectNull, objectNull, "[object Null]")                               \
  MACRO(objectNumber, objectNumber, "[object Number]")                         \
  MACRO(objectObject, objectObject, "[object Object]")                         \
  MACRO(objectRegExp, objectRegExp, "[object RegExp]")                         \
  MACRO(objects, objects, "objects")                                           \
  MACRO(objectString, objectString, "[object String]")                         \
  MACRO(objectUndefined, objectUndefined, "[object Undefined]")                \
  MACRO(of, of, "of")                                                          \
  MACRO(offset, offset, "offset")                                              \
  MACRO(optimizedOut, optimizedOut, "optimizedOut")                            \
  MACRO(other, other, "other")                                                 \
  MACRO(outOfMemory, outOfMemory, "out of memory")                             \
  MACRO(ownKeys, ownKeys, "ownKeys")                                           \
  MACRO(Object_valueOf, Object_valueOf, "Object_valueOf")                      \
  MACRO(package, package, "package")                                           \
  MACRO(parseFloat, parseFloat, "parseFloat")                                  \
  MACRO(parseInt, parseInt, "parseInt")                                        \
  MACRO(pattern, pattern, "pattern")                                           \
  MACRO(pending, pending, "pending")                                           \
  MACRO(percentSign, percentSign, "percentSign")                               \
  MACRO(pipeTo, pipeTo, "pipeTo")                                              \
  MACRO(plusSign, plusSign, "plusSign")                                        \
  MACRO(public, public_, "public")                                             \
  MACRO(pull, pull, "pull")                                                    \
  MACRO(preventAbort, preventAbort, "preventAbort")                            \
  MACRO(preventClose, preventClose, "preventClose")                            \
  MACRO(preventCancel, preventCancel, "preventCancel")                         \
  MACRO(preventExtensions, preventExtensions, "preventExtensions")             \
  MACRO(private, private_, "private")                                          \
  MACRO(promise, promise, "promise")                                           \
  MACRO(propertyIsEnumerable, propertyIsEnumerable, "propertyIsEnumerable")    \
  MACRO(protected, protected_, "protected")                                    \
  MACRO(proto, proto, "__proto__")                                             \
  MACRO(prototype, prototype, "prototype")                                     \
  MACRO(proxy, proxy, "proxy")                                                 \
  MACRO(quarter, quarter, "quarter")                                           \
  MACRO(raw, raw, "raw")                                                       \
  MACRO(reason, reason, "reason")                                              \
  MACRO(RegExpFlagsGetter, RegExpFlagsGetter, "$RegExpFlagsGetter")            \
  MACRO(RegExpStringIterator, RegExpStringIterator, "RegExp String Iterator")  \
  MACRO(RegExpToString, RegExpToString, "$RegExpToString")                     \
  MACRO(region, region, "region")                                              \
  MACRO(register, register_, "register")                                       \
  MACRO(Reify, Reify, "Reify")                                                 \
  MACRO(reject, reject, "reject")                                              \
  MACRO(rejected, rejected, "rejected")                                        \
  MACRO(relatedYear, relatedYear, "relatedYear")                               \
  MACRO(RelativeTimeFormatFormat, RelativeTimeFormatFormat,                    \
        "Intl_RelativeTimeFormat_Format")                                      \
  MACRO(RequireObjectCoercible, RequireObjectCoercible,                        \
        "RequireObjectCoercible")                                              \
  MACRO(resolve, resolve, "resolve")                                           \
  MACRO(result, result, "result")                                              \
  MACRO(resumeGenerator, resumeGenerator, "resumeGenerator")                   \
  MACRO(return, return_, "return")                                             \
  MACRO(revoke, revoke, "revoke")                                              \
  MACRO(rtl, rtl, "rtl")                                                       \
  MACRO(script, script, "script")                                              \
  MACRO(scripts, scripts, "scripts")                                           \
  MACRO(second, second, "second")                                              \
  MACRO(selfHosted, selfHosted, "self-hosted")                                 \
  MACRO(sensitivity, sensitivity, "sensitivity")                               \
  MACRO(set, set, "set")                                                       \
  MACRO(setBigInt64, setBigInt64, "setBigInt64")                               \
  MACRO(setBigUint64, setBigUint64, "setBigUint64")                            \
  MACRO(SetConstructorInit, SetConstructorInit, "SetConstructorInit")          \
  MACRO(SetIterator, SetIterator, "Set Iterator")                              \
  MACRO(setPrefix, setPrefix, "set ")                                          \
  MACRO(setPrototypeOf, setPrototypeOf, "setPrototypeOf")                      \
  MACRO(shape, shape, "shape")                                                 \
  MACRO(signal, signal, "signal")                                              \
  MACRO(signDisplay, signDisplay, "signDisplay")                               \
  MACRO(size, size, "size")                                                    \
  MACRO(source, source, "source")                                              \
  MACRO(SpeciesConstructor, SpeciesConstructor, "SpeciesConstructor")          \
  MACRO(stack, stack, "stack")                                                 \
  MACRO(star, star, "*")                                                       \
  MACRO(start, start, "start")                                                 \
  MACRO(startTimestamp, startTimestamp, "startTimestamp")                      \
  MACRO(state, state, "state")                                                 \
  MACRO(static, static_, "static")                                             \
  MACRO(status, status, "status")                                              \
  MACRO(std_Function_apply, std_Function_apply, "std_Function_apply")          \
  MACRO(sticky, sticky, "sticky")                                              \
  MACRO(StringIterator, StringIterator, "String Iterator")                     \
  MACRO(strings, strings, "strings")                                           \
  MACRO(String_split, String_split, "String_split")                            \
  MACRO(StructType, StructType, "StructType")                                  \
  MACRO(style, style, "style")                                                 \
  MACRO(super, super, "super")                                                 \
  MACRO(switch, switch_, "switch")                                             \
  MACRO(Symbol_iterator_fun, Symbol_iterator_fun, "[Symbol.iterator]")         \
  MACRO(target, target, "target")                                              \
  MACRO(test, test, "test")                                                    \
  MACRO(then, then, "then")                                                    \
  MACRO(this, this_, "this")                                                   \
  MACRO(throw, throw_, "throw")                                                \
  MACRO(timestamp, timestamp, "timestamp")                                     \
  MACRO(timeZone, timeZone, "timeZone")                                        \
  MACRO(timeZoneName, timeZoneName, "timeZoneName")                            \
  MACRO(trimEnd, trimEnd, "trimEnd")                                           \
  MACRO(trimLeft, trimLeft, "trimLeft")                                        \
  MACRO(trimRight, trimRight, "trimRight")                                     \
  MACRO(trimStart, trimStart, "trimStart")                                     \
  MACRO(toGMTString, toGMTString, "toGMTString")                               \
  MACRO(toISOString, toISOString, "toISOString")                               \
  MACRO(toJSON, toJSON, "toJSON")                                              \
  MACRO(toLocaleString, toLocaleString, "toLocaleString")                      \
  MACRO(ToNumeric, ToNumeric, "ToNumeric")                                     \
  MACRO(toSource, toSource, "toSource")                                        \
  MACRO(toString, toString, "toString")                                        \
  MACRO(toUTCString, toUTCString, "toUTCString")                               \
  MACRO(true, true_, "true")                                                   \
  MACRO(try, try_, "try")                                                      \
  MACRO(type, type, "type")                                                    \
  MACRO(typeof, typeof_, "typeof")                                             \
  MACRO(uint8, uint8, "uint8")                                                 \
  MACRO(uint8Clamped, uint8Clamped, "uint8Clamped")                            \
  MACRO(uint16, uint16, "uint16")                                              \
  MACRO(uint32, uint32, "uint32")                                              \
  MACRO(Uint8x16, Uint8x16, "Uint8x16")                                        \
  MACRO(Uint16x8, Uint16x8, "Uint16x8")                                        \
  MACRO(Uint32x4, Uint32x4, "Uint32x4")                                        \
  MACRO(unescape, unescape, "unescape")                                        \
  MACRO(uneval, uneval, "uneval")                                              \
  MACRO(unicode, unicode, "unicode")                                           \
  MACRO(unit, unit, "unit")                                                    \
  MACRO(unitDisplay, unitDisplay, "unitDisplay")                               \
  MACRO(uninitialized, uninitialized, "uninitialized")                         \
  MACRO(unknown, unknown, "unknown")                                           \
  MACRO(unregister, unregister, "unregister")                                  \
  MACRO(unsized, unsized, "unsized")                                           \
  MACRO(unwatch, unwatch, "unwatch")                                           \
  MACRO(url, url, "url")                                                       \
  MACRO(usage, usage, "usage")                                                 \
  MACRO(useAsm, useAsm, "use asm")                                             \
  MACRO(useGrouping, useGrouping, "useGrouping")                               \
  MACRO(useStrict, useStrict, "use strict")                                    \
  MACRO(void, void_, "void")                                                   \
  MACRO(value, value, "value")                                                 \
  MACRO(valueOf, valueOf, "valueOf")                                           \
  MACRO(values, values, "values")                                              \
  MACRO(var, var, "var")                                                       \
  MACRO(variable, variable, "variable")                                        \
  MACRO(void0, void0, "(void 0)")                                              \
  MACRO(wasm, wasm, "wasm")                                                    \
  MACRO(WasmAnyRef, WasmAnyRef, "WasmAnyRef")                                  \
  MACRO(wasmcall, wasmcall, "wasmcall")                                        \
  MACRO(watch, watch, "watch")                                                 \
  MACRO(WeakMapConstructorInit, WeakMapConstructorInit,                        \
        "WeakMapConstructorInit")                                              \
  MACRO(WeakSetConstructorInit, WeakSetConstructorInit,                        \
        "WeakSetConstructorInit")                                              \
  MACRO(WeakSet_add, WeakSet_add, "WeakSet_add")                               \
  MACRO(week, week, "week")                                                    \
  MACRO(weekday, weekday, "weekday")                                           \
  MACRO(weekendEnd, weekendEnd, "weekendEnd")                                  \
  MACRO(weekendStart, weekendStart, "weekendStart")                            \
  MACRO(while, while_, "while")                                                \
  MACRO(with, with, "with")                                                    \
  MACRO(writable, writable, "writable")                                        \
  MACRO(write, write, "write")                                                 \
  MACRO(year, year, "year")                                                    \
  MACRO(yearName, yearName, "yearName")                                        \
  MACRO(yield, yield, "yield")                                                 \
  /* Type names must be contiguous and ordered; see js::TypeName. */           \
  MACRO(undefined, undefined, "undefined")                                     \
  MACRO(object, object, "object")                                              \
  MACRO(function, function, "function")                                        \
  MACRO(string, string, "string")                                              \
  MACRO(number, number, "number")                                              \
  MACRO(boolean, boolean, "boolean")                                           \
  MACRO(null, null, "null")                                                    \
  MACRO(symbol, symbol, "symbol")                                              \
  MACRO(bigint, bigint, "bigint")                                              \
  MACRO(defineDataPropertyIntrinsic, defineDataPropertyIntrinsic,              \
        "_DefineDataProperty")

#endif /* vm_CommonPropertyNames_h */
