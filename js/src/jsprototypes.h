/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsprototypes_h
#define jsprototypes_h

/* A higher-order macro for enumerating all JSProtoKey values. */
/*
 * Consumers define macros as follows:
 * macro(name, code, init, clasp)
 *   name:    The canonical name of the class.
 *   code:    The enumerator code. There are part of the XDR API, and must not change.
 *   init:    Initialization function. These are |extern "C";|, and clients should use
 *            |extern "C" {}| as appropriate when using this macro.
 *   clasp:   The JSClass for this object, or "dummy" if it doesn't exist.
 *
 *
 * Consumers wishing to iterate over all the JSProtoKey values, can use
 * JS_FOR_EACH_PROTOTYPE. However, there are certain values that don't correspond
 * to real constructors, like Null or constructors that are disabled via
 * preprocessor directives. We still need to include these in the JSProtoKey list
 * in order to maintain binary XDR compatibility, but we need to provide a tool
 * to handle them differently. JS_FOR_PROTOTYPES fills this niche.
 *
 * Consumers pass two macros to JS_FOR_PROTOTYPES - |real| and |imaginary|. The
 * former is invoked for entries that have real client-exposed constructors, and
 * the latter is called for the rest. Consumers that don't care about this
 * distinction can simply pass the same macro to both, which is exactly what
 * JS_FOR_EACH_PROTOTYPE does.
 */

#define CLASP(name)                 (&name##Class)
#define OCLASP(name)                (&name##Object::class_)
#define TYPED_ARRAY_CLASP(type)     (&TypedArrayObject::classes[Scalar::type])
#define ERROR_CLASP(type)           (&ErrorObject::classes[type])

#ifdef EXPOSE_INTL_API
#define IF_INTL(real,imaginary) real
#else
#define IF_INTL(real,imaginary) imaginary
#endif

#ifdef ENABLE_BINARYDATA
#define IF_BDATA(real,imaginary) real
#else
#define IF_BDATA(real,imaginary) imaginary
#endif

#ifdef ENABLE_SIMD
# define IF_SIMD(real,imaginary) real
#else
# define IF_SIMD(real,imaginary) imaginary
#endif

#ifdef ENABLE_SHARED_ARRAY_BUFFER
#define IF_SAB(real,imaginary) real
#else
#define IF_SAB(real,imaginary) imaginary
#endif

#ifdef SPIDERMONKEY_PROMISE
#define IF_PROMISE(real,imaginary) real
#else
#define IF_PROMISE(real,imaginary) imaginary
#endif

#define JS_FOR_PROTOTYPES(real,imaginary) \
    imaginary(Null,              0,     InitNullClass,          dummy) \
    real(Object,                 1,     InitViaClassSpec,       OCLASP(Plain)) \
    real(Function,               2,     InitViaClassSpec,       &JSFunction::class_) \
    real(Array,                  3,     InitViaClassSpec,       OCLASP(Array)) \
    real(Boolean,                4,     InitBooleanClass,       OCLASP(Boolean)) \
    real(JSON,                   5,     InitJSONClass,          CLASP(JSON)) \
    real(Date,                   6,     InitViaClassSpec,       OCLASP(Date)) \
    real(Math,                   7,     InitMathClass,          CLASP(Math)) \
    real(Number,                 8,     InitNumberClass,        OCLASP(Number)) \
    real(String,                 9,     InitStringClass,        OCLASP(String)) \
    real(RegExp,                10,     InitViaClassSpec,       OCLASP(RegExp)) \
    real(Error,                 11,     InitViaClassSpec,       ERROR_CLASP(JSEXN_ERR)) \
    real(InternalError,         12,     InitViaClassSpec,       ERROR_CLASP(JSEXN_INTERNALERR)) \
    real(EvalError,             13,     InitViaClassSpec,       ERROR_CLASP(JSEXN_EVALERR)) \
    real(RangeError,            14,     InitViaClassSpec,       ERROR_CLASP(JSEXN_RANGEERR)) \
    real(ReferenceError,        15,     InitViaClassSpec,       ERROR_CLASP(JSEXN_REFERENCEERR)) \
    real(SyntaxError,           16,     InitViaClassSpec,       ERROR_CLASP(JSEXN_SYNTAXERR)) \
    real(TypeError,             17,     InitViaClassSpec,       ERROR_CLASP(JSEXN_TYPEERR)) \
    real(URIError,              18,     InitViaClassSpec,       ERROR_CLASP(JSEXN_URIERR)) \
    real(DebuggeeWouldRun,      19,     InitViaClassSpec,       ERROR_CLASP(JSEXN_DEBUGGEEWOULDRUN)) \
    real(CompileError,          20,     InitViaClassSpec,       ERROR_CLASP(JSEXN_WASMCOMPILEERROR)) \
    real(RuntimeError,          21,     InitViaClassSpec,       ERROR_CLASP(JSEXN_WASMRUNTIMEERROR)) \
    real(Iterator,              22,     InitLegacyIteratorClass,OCLASP(PropertyIterator)) \
    real(StopIteration,         23,     InitStopIterationClass, OCLASP(StopIteration)) \
    real(ArrayBuffer,           24,     InitArrayBufferClass,   &js::ArrayBufferObject::protoClass) \
    real(Int8Array,             25,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Int8)) \
    real(Uint8Array,            26,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Uint8)) \
    real(Int16Array,            27,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Int16)) \
    real(Uint16Array,           28,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Uint16)) \
    real(Int32Array,            29,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Int32)) \
    real(Uint32Array,           30,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Uint32)) \
    real(Float32Array,          31,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Float32)) \
    real(Float64Array,          32,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Float64)) \
    real(Uint8ClampedArray,     33,     InitViaClassSpec,       TYPED_ARRAY_CLASP(Uint8Clamped)) \
    real(Proxy,                 34,     InitProxyClass,         js::ProxyClassPtr) \
    real(WeakMap,               35,     InitWeakMapClass,       OCLASP(WeakMap)) \
    real(Map,                   36,     InitMapClass,           OCLASP(Map)) \
    real(Set,                   37,     InitSetClass,           OCLASP(Set)) \
    real(DataView,              38,     InitDataViewClass,      OCLASP(DataView)) \
    real(Symbol,                39,     InitSymbolClass,        OCLASP(Symbol)) \
IF_SAB(real,imaginary)(SharedArrayBuffer,       40,     InitSharedArrayBufferClass, &js::SharedArrayBufferObject::protoClass) \
IF_INTL(real,imaginary) (Intl,                  41,     InitIntlClass,          CLASP(Intl)) \
IF_BDATA(real,imaginary)(TypedObject,           42,     InitTypedObjectModuleObject,   OCLASP(TypedObjectModule)) \
    real(Reflect,               43,     InitReflect,            nullptr) \
IF_SIMD(real,imaginary)(SIMD,                   44,     InitSimdClass, OCLASP(Simd)) \
    real(WeakSet,               45,     InitWeakSetClass,       OCLASP(WeakSet)) \
    real(TypedArray,            46,     InitViaClassSpec,       &js::TypedArrayObject::sharedTypedArrayPrototypeClass) \
IF_SAB(real,imaginary)(Atomics, 47,     InitAtomicsClass, OCLASP(Atomics)) \
    real(SavedFrame,            48,     InitViaClassSpec,       &js::SavedFrame::class_) \
    real(Wasm,                  49,     InitWasmClass,          CLASP(Wasm)) \
    real(WebAssembly,           50,     InitWebAssemblyClass,   CLASP(WebAssembly)) \
    imaginary(WasmModule,       51,     dummy,                  dummy) \
    imaginary(WasmInstance,     52,     dummy,                  dummy) \
    imaginary(WasmMemory,       53,     dummy,                  dummy) \
    imaginary(WasmTable,        54,     dummy,                  dummy) \
IF_PROMISE(real,imaginary)(Promise,             55,     InitViaClassSpec, OCLASP(Promise)) \

#define JS_FOR_EACH_PROTOTYPE(macro) JS_FOR_PROTOTYPES(macro,macro)

#endif /* jsprototypes_h */
