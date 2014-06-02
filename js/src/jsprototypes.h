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
#define TYPED_ARRAY_CLASP(type)     (&TypedArrayObject::classes[ScalarTypeDescr::type])

#ifdef ENABLE_PARALLEL_JS
#define IF_PJS(real,imaginary) real
#else
#define IF_PJS(real,imaginary) imaginary
#endif

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

#ifdef ENABLE_SHARED_ARRAY_BUFFER
#define IF_SAB(real,imaginary) real
#else
#define IF_SAB(real,imaginary) imaginary
#endif

#define JS_FOR_PROTOTYPES(real,imaginary) \
    imaginary(Null,              0,     js_InitNullClass,          dummy) \
    real(Object,                 1,     js_InitViaClassSpec,       &JSObject::class_) \
    real(Function,               2,     js_InitViaClassSpec,       &JSFunction::class_) \
    real(Array,                  3,     js_InitViaClassSpec,       OCLASP(Array)) \
    real(Boolean,                4,     js_InitBooleanClass,       OCLASP(Boolean)) \
    real(JSON,                   5,     js_InitJSONClass,          CLASP(JSON)) \
    real(Date,                   6,     js_InitViaClassSpec,       OCLASP(Date)) \
    real(Math,                   7,     js_InitMathClass,          CLASP(Math)) \
    real(Number,                 8,     js_InitNumberClass,        OCLASP(Number)) \
    real(String,                 9,     js_InitStringClass,        OCLASP(String)) \
    real(RegExp,                10,     js_InitRegExpClass,        OCLASP(RegExp)) \
    real(Error,                 11,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(InternalError,         12,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(EvalError,             13,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(RangeError,            14,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(ReferenceError,        15,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(SyntaxError,           16,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(TypeError,             17,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(URIError,              18,     js_InitExceptionClasses,   OCLASP(Error)) \
    real(Iterator,              19,     js_InitIteratorClasses,    OCLASP(PropertyIterator)) \
    real(StopIteration,         20,     js_InitIteratorClasses,    OCLASP(StopIteration)) \
    real(ArrayBuffer,           21,     js_InitArrayBufferClass,   &js::ArrayBufferObject::protoClass) \
    real(Int8Array,             22,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_INT8)) \
    real(Uint8Array,            23,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_UINT8)) \
    real(Int16Array,            24,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_INT16)) \
    real(Uint16Array,           25,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_UINT16)) \
    real(Int32Array,            26,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_INT32)) \
    real(Uint32Array,           27,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_UINT32)) \
    real(Float32Array,          28,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_FLOAT32)) \
    real(Float64Array,          29,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_FLOAT64)) \
    real(Uint8ClampedArray,     30,     js_InitViaClassSpec,       TYPED_ARRAY_CLASP(TYPE_UINT8_CLAMPED)) \
    real(Proxy,                 31,     js_InitProxyClass,         &ProxyObject::uncallableClass_) \
    real(WeakMap,               32,     js_InitWeakMapClass,       OCLASP(WeakMap)) \
    real(Map,                   33,     js_InitMapClass,           OCLASP(Map)) \
    real(Set,                   34,     js_InitSetClass,           OCLASP(Set)) \
    real(DataView,              35,     js_InitDataViewClass,      OCLASP(DataView)) \
IF_SAB(real,imaginary)(SharedArrayBuffer,       36,     js_InitSharedArrayBufferClass, &js::SharedArrayBufferObject::protoClass) \
IF_INTL(real,imaginary) (Intl,                  37,     js_InitIntlClass,          CLASP(Intl)) \
IF_BDATA(real,imaginary)(TypedObject,           38,     js_InitTypedObjectModuleObject,   OCLASP(TypedObjectModule)) \
    imaginary(GeneratorFunction,     39,     js_InitIteratorClasses, dummy) \
IF_BDATA(real,imaginary)(SIMD,                  40,     js_InitSIMDClass, OCLASP(SIMD)) \

#define JS_FOR_EACH_PROTOTYPE(macro) JS_FOR_PROTOTYPES(macro,macro)

#endif /* jsprototypes_h */
