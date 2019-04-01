/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ProtoKey_h
#define js_ProtoKey_h

/* A higher-order macro for enumerating all JSProtoKey values. */
/*
 * Consumers define macros as follows:
 * MACRO(name, init, clasp)
 *   name:    The canonical name of the class.
 *   init:    Initialization function. These are |extern "C";|, and clients
 *            should use |extern "C" {}| as appropriate when using this macro.
 *   clasp:   The JSClass for this object, or "dummy" if it doesn't exist.
 *
 *
 * Consumers wishing to iterate over all the JSProtoKey values, can use
 * JS_FOR_EACH_PROTOTYPE. However, there are certain values that don't
 * correspond to real constructors, like Null or constructors that are disabled
 * via preprocessor directives. We still need to include these in the JSProtoKey
 * list in order to maintain binary XDR compatibility, but we need to provide a
 * tool to handle them differently. JS_FOR_PROTOTYPES fills this niche.
 *
 * Consumers pass two macros to JS_FOR_PROTOTYPES - |REAL| and |IMAGINARY|. The
 * former is invoked for entries that have real client-exposed constructors, and
 * the latter is called for the rest. Consumers that don't care about this
 * distinction can simply pass the same macro to both, which is exactly what
 * JS_FOR_EACH_PROTOTYPE does.
 */

#define CLASP(NAME) (&NAME##Class)
#define OCLASP(NAME) (&NAME##Object::class_)
#define TYPED_ARRAY_CLASP(TYPE) (&TypedArrayObject::classes[Scalar::TYPE])
#define ERROR_CLASP(TYPE) (&ErrorObject::classes[TYPE])

#ifdef EXPOSE_INTL_API
#  define IF_INTL(REAL, IMAGINARY) REAL
#else
#  define IF_INTL(REAL, IMAGINARY) IMAGINARY
#endif

#ifdef ENABLE_TYPED_OBJECTS
#  define IF_TYPEDOBJ(REAL, IMAGINARY) REAL
#else
#  define IF_TYPEDOBJ(REAL, IMAGINARY) IMAGINARY
#endif

#ifdef ENABLE_SHARED_ARRAY_BUFFER
#  define IF_SAB(REAL, IMAGINARY) REAL
#else
#  define IF_SAB(REAL, IMAGINARY) IMAGINARY
#endif

#define JS_FOR_PROTOTYPES_(REAL, IMAGINARY, REAL_IF_INTL, REAL_IF_BDATA,     \
                           REAL_IF_SAB)                                      \
  IMAGINARY(Null, InitNullClass, dummy)                                      \
  REAL(Object, InitViaClassSpec, OCLASP(Plain))                              \
  REAL(Function, InitViaClassSpec, &JSFunction::class_)                      \
  REAL(Array, InitViaClassSpec, OCLASP(Array))                               \
  REAL(Boolean, InitBooleanClass, OCLASP(Boolean))                           \
  REAL(JSON, InitJSONClass, CLASP(JSON))                                     \
  REAL(Date, InitViaClassSpec, OCLASP(Date))                                 \
  REAL(Math, InitMathClass, CLASP(Math))                                     \
  REAL(Number, InitNumberClass, OCLASP(Number))                              \
  REAL(String, InitStringClass, OCLASP(String))                              \
  REAL(RegExp, InitViaClassSpec, OCLASP(RegExp))                             \
  REAL(Error, InitViaClassSpec, ERROR_CLASP(JSEXN_ERR))                      \
  REAL(InternalError, InitViaClassSpec, ERROR_CLASP(JSEXN_INTERNALERR))      \
  REAL(EvalError, InitViaClassSpec, ERROR_CLASP(JSEXN_EVALERR))              \
  REAL(RangeError, InitViaClassSpec, ERROR_CLASP(JSEXN_RANGEERR))            \
  REAL(ReferenceError, InitViaClassSpec, ERROR_CLASP(JSEXN_REFERENCEERR))    \
  REAL(SyntaxError, InitViaClassSpec, ERROR_CLASP(JSEXN_SYNTAXERR))          \
  REAL(TypeError, InitViaClassSpec, ERROR_CLASP(JSEXN_TYPEERR))              \
  REAL(URIError, InitViaClassSpec, ERROR_CLASP(JSEXN_URIERR))                \
  REAL(DebuggeeWouldRun, InitViaClassSpec,                                   \
       ERROR_CLASP(JSEXN_DEBUGGEEWOULDRUN))                                  \
  REAL(CompileError, InitViaClassSpec, ERROR_CLASP(JSEXN_WASMCOMPILEERROR))  \
  REAL(LinkError, InitViaClassSpec, ERROR_CLASP(JSEXN_WASMLINKERROR))        \
  REAL(RuntimeError, InitViaClassSpec, ERROR_CLASP(JSEXN_WASMRUNTIMEERROR))  \
  REAL(ArrayBuffer, InitViaClassSpec, OCLASP(ArrayBuffer))                   \
  REAL(Int8Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Int8))                 \
  REAL(Uint8Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Uint8))               \
  REAL(Int16Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Int16))               \
  REAL(Uint16Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Uint16))             \
  REAL(Int32Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Int32))               \
  REAL(Uint32Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Uint32))             \
  REAL(Float32Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Float32))           \
  REAL(Float64Array, InitViaClassSpec, TYPED_ARRAY_CLASP(Float64))           \
  REAL(Uint8ClampedArray, InitViaClassSpec, TYPED_ARRAY_CLASP(Uint8Clamped)) \
  REAL(BigInt64Array, InitViaClassSpec, TYPED_ARRAY_CLASP(BigInt64))         \
  REAL(BigUint64Array, InitViaClassSpec, TYPED_ARRAY_CLASP(BigUint64))       \
  REAL(BigInt, InitViaClassSpec, OCLASP(BigInt))                             \
  REAL(Proxy, InitProxyClass, &js::ProxyClass)                               \
  REAL(WeakMap, InitViaClassSpec, OCLASP(WeakMap))                           \
  REAL(Map, InitViaClassSpec, OCLASP(Map))                                   \
  REAL(Set, InitViaClassSpec, OCLASP(Set))                                   \
  REAL(DataView, InitViaClassSpec, OCLASP(DataView))                         \
  REAL(Symbol, InitSymbolClass, OCLASP(Symbol))                              \
  REAL(SharedArrayBuffer, InitViaClassSpec, OCLASP(SharedArrayBuffer))       \
  REAL_IF_INTL(Intl, InitIntlClass, CLASP(Intl))                             \
  REAL_IF_BDATA(TypedObject, InitTypedObjectModuleObject,                    \
                OCLASP(TypedObjectModule))                                   \
  REAL(Reflect, InitReflect, nullptr)                                        \
  REAL(WeakSet, InitViaClassSpec, OCLASP(WeakSet))                           \
  REAL(TypedArray, InitViaClassSpec,                                         \
       &js::TypedArrayObject::sharedTypedArrayPrototypeClass)                \
  REAL(Atomics, InitAtomicsClass, OCLASP(Atomics))                           \
  REAL(SavedFrame, InitViaClassSpec, &js::SavedFrame::class_)                \
  REAL(Promise, InitViaClassSpec, OCLASP(Promise))                           \
  REAL(ReadableStream, InitViaClassSpec, &js::ReadableStream::class_)        \
  REAL(ReadableStreamDefaultReader, InitViaClassSpec,                        \
       &js::ReadableStreamDefaultReader::class_)                             \
  REAL(ReadableStreamDefaultController, InitViaClassSpec,                    \
       &js::ReadableStreamDefaultController::class_)                         \
  REAL(ReadableByteStreamController, InitViaClassSpec,                       \
       &js::ReadableByteStreamController::class_)                            \
  IMAGINARY(WritableStream, dummy, dummy)                                    \
  IMAGINARY(WritableStreamDefaultWriter, dummy, dummy)                       \
  IMAGINARY(WritableStreamDefaultController, dummy, dummy)                   \
  REAL(ByteLengthQueuingStrategy, InitViaClassSpec,                          \
       &js::ByteLengthQueuingStrategy::class_)                               \
  REAL(CountQueuingStrategy, InitViaClassSpec,                               \
       &js::CountQueuingStrategy::class_)                                    \
  REAL(WebAssembly, InitWebAssemblyClass, CLASP(WebAssembly))                \
  IMAGINARY(WasmModule, dummy, dummy)                                        \
  IMAGINARY(WasmInstance, dummy, dummy)                                      \
  IMAGINARY(WasmMemory, dummy, dummy)                                        \
  IMAGINARY(WasmTable, dummy, dummy)                                         \
  IMAGINARY(WasmGlobal, dummy, dummy)

#define JS_FOR_PROTOTYPES(REAL, IMAGINARY)                      \
  JS_FOR_PROTOTYPES_(REAL, IMAGINARY, IF_INTL(REAL, IMAGINARY), \
                     IF_TYPEDOBJ(REAL, IMAGINARY), IF_SAB(REAL, IMAGINARY))

#define JS_FOR_EACH_PROTOTYPE(MACRO) JS_FOR_PROTOTYPES(MACRO, MACRO)

#endif /* js_ProtoKey_h */
