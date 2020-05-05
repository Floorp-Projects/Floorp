/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_js_h
#define wasm_js_h

#include "gc/Policy.h"
#include "gc/ZoneAllocator.h"
#include "vm/NativeObject.h"
#include "wasm/WasmTypes.h"

namespace js {

class ArrayBufferObjectMaybeShared;
class JSStringBuilder;
class StructTypeDescr;
class TypedArrayObject;
class WasmFunctionScope;
class WasmInstanceScope;
class SharedArrayRawBuffer;

namespace wasm {

// Return whether WebAssembly can in principle be compiled on this platform (ie
// combination of hardware and OS), assuming at least one of the compilers that
// supports the platform is not disabled by other settings.
//
// This predicate must be checked and must be true to call any of the top-level
// wasm eval/compile methods.

bool HasPlatformSupport(JSContext* cx);

// Return whether WebAssembly is supported on this platform. This determines
// whether the WebAssembly object is exposed to JS and takes into account
// configuration options that disable various modes.  It also checks that at
// least one compiler is (currently) available.

bool HasSupport(JSContext* cx);

// Predicates for compiler availability.
//
// These three predicates together select zero or one baseline compiler and zero
// or one optimizing compiler, based on: what's compiled into the executable,
// what's supported on the current platform, what's selected by options, and the
// current run-time environment.  As it is possible for the computed values to
// change (when a value changes in about:config or the debugger pane is shown or
// hidden), it is inadvisable to cache these values in such a way that they
// could become invalid.  Generally it is cheap always to recompute them.

bool BaselineAvailable(JSContext* cx);
bool IonAvailable(JSContext* cx);
bool CraneliftAvailable(JSContext* cx);

// Predicates for white-box compiler disablement testing.
//
// These predicates determine whether the optimizing compilers were disabled by
// features that are enabled at compile-time or run-time.  They do not consider
// the hardware platform on whether other compilers are enabled.
//
// If `reason` is not null then it is populated with a string that describes
// the specific features that disable the compiler.
//
// Returns false on OOM (which happens only when a reason is requested),
// otherwise true, with the result in `*isDisabled` and optionally the reason in
// `*reason`.

bool IonDisabledByFeatures(JSContext* cx, bool* isDisabled,
                           JSStringBuilder* reason = nullptr);
bool CraneliftDisabledByFeatures(JSContext* cx, bool* isDisabled,
                                 JSStringBuilder* reason = nullptr);

// Predicates for feature availability.
//
// The following predicates check whether particular wasm features are enabled,
// and for each, whether at least one compiler is (currently) available that
// supports the feature.

// Streaming compilation.
bool StreamingCompilationAvailable(JSContext* cx);

// Caching of optimized code.  Implies both streaming compilation and an
// optimizing compiler tier.
bool CodeCachingAvailable(JSContext* cx);

// General reference types (anyref, funcref, nullref) and operations on them.
bool ReftypesAvailable(JSContext* cx);

// Experimental (ref T) types and structure types.
bool GcTypesAvailable(JSContext* cx);

// Multi-value block and function returns.
bool MultiValuesAvailable(JSContext* cx);

// I64<->BigInt interconversion at the wasm/JS boundary.
bool I64BigIntConversionAvailable(JSContext* cx);

// Shared memory and atomics.
bool ThreadsAvailable(JSContext* cx);

// SIMD data and operations.
bool SimdAvailable(JSContext* cx);

// Compiles the given binary wasm module given the ArrayBufferObject
// and links the module's imports with the given import object.

MOZ_MUST_USE bool Eval(JSContext* cx, Handle<TypedArrayObject*> code,
                       HandleObject importObj,
                       MutableHandleWasmInstanceObject instanceObj);

// Extracts the various imports from the given import object into the given
// ImportValues structure while checking the imports against the given module.
// The resulting structure can be passed to WasmModule::instantiate.

struct ImportValues;
MOZ_MUST_USE bool GetImports(JSContext* cx, const Module& module,
                             HandleObject importObj, ImportValues* imports);

// For testing cross-process (de)serialization, this pair of functions are
// responsible for, in the child process, compiling the given wasm bytecode
// to a wasm::Module that is serialized into the given byte array, and, in
// the parent process, deserializing the given byte array into a
// WebAssembly.Module object.

MOZ_MUST_USE bool CompileAndSerialize(const ShareableBytes& bytecode,
                                      Bytes* serialized);

MOZ_MUST_USE bool DeserializeModule(JSContext* cx, const Bytes& serialized,
                                    MutableHandleObject module);

// A WebAssembly "Exported Function" is the spec name for the JS function
// objects created to wrap wasm functions. This predicate returns false
// for asm.js functions which are semantically just normal JS functions
// (even if they are implemented via wasm under the hood). The accessor
// functions for extracting the instance and func-index of a wasm function
// can be used for both wasm and asm.js, however.

bool IsWasmExportedFunction(JSFunction* fun);
MOZ_MUST_USE bool CheckFuncRefValue(JSContext* cx, HandleValue v,
                                    MutableHandleFunction fun);

Instance& ExportedFunctionToInstance(JSFunction* fun);
WasmInstanceObject* ExportedFunctionToInstanceObject(JSFunction* fun);
uint32_t ExportedFunctionToFuncIndex(JSFunction* fun);

bool IsSharedWasmMemoryObject(JSObject* obj);

// Check a value against the given reference type kind.  If the targetTypeKind
// is RefType::Any then the test always passes, but the value may be boxed.  If
// the test passes then the value is stored either in fnval (for RefType::Func)
// or in refval (for other types); this split is not strictly necessary but is
// convenient for the users of this function.
//
// This can return false if the type check fails, or if a boxing into AnyRef
// throws an OOM.
MOZ_MUST_USE bool CheckRefType(JSContext* cx, RefType::Kind targetTypeKind,
                               HandleValue v, MutableHandleFunction fnval,
                               MutableHandleAnyRef refval);

}  // namespace wasm

// The class of the WebAssembly global namespace object.

extern const JSClass WebAssemblyClass;

// The class of WebAssembly.Module. Each WasmModuleObject owns a
// wasm::Module. These objects are used both as content-facing JS objects and as
// internal implementation details of asm.js.

class WasmModuleObject : public NativeObject {
  static const unsigned MODULE_SLOT = 0;
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
  static void finalize(JSFreeOp* fop, JSObject* obj);
  static bool imports(JSContext* cx, unsigned argc, Value* vp);
  static bool exports(JSContext* cx, unsigned argc, Value* vp);
  static bool customSections(JSContext* cx, unsigned argc, Value* vp);

 public:
  static const unsigned RESERVED_SLOTS = 1;
  static const JSClass class_;
  static const JSClass& protoClass_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  static WasmModuleObject* create(JSContext* cx, const wasm::Module& module,
                                  HandleObject proto);
  const wasm::Module& module() const;
};

// The class of WebAssembly.Global.  This wraps a storage location, and there is
// a per-agent one-to-one relationship between the WasmGlobalObject and the
// storage location (the Cell) it wraps: if a module re-exports an imported
// global, the imported and exported WasmGlobalObjects are the same, and if a
// module exports a global twice, the two exported WasmGlobalObjects are the
// same.

// TODO/AnyRef-boxing: With boxed immediates and strings, JSObject* is no longer
// the most appropriate representation for Cell::anyref.
STATIC_ASSERT_ANYREF_IS_JSOBJECT;

class WasmGlobalObject : public NativeObject {
  static const unsigned TYPE_SLOT = 0;
  static const unsigned MUTABLE_SLOT = 1;
  static const unsigned CELL_SLOT = 2;

  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
  static void finalize(JSFreeOp*, JSObject* obj);
  static void trace(JSTracer* trc, JSObject* obj);

  static bool valueGetterImpl(JSContext* cx, const CallArgs& args);
  static bool valueGetter(JSContext* cx, unsigned argc, Value* vp);
  static bool valueSetterImpl(JSContext* cx, const CallArgs& args);
  static bool valueSetter(JSContext* cx, unsigned argc, Value* vp);

 public:
  // For exposed globals the Cell holds the value of the global; the
  // instance's global area holds a pointer to the Cell.
  union Cell {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    wasm::V128 v128;
    wasm::AnyRef ref;
    Cell() : v128() {}
    ~Cell() = default;
  };

  static const unsigned RESERVED_SLOTS = 3;
  static const JSClass class_;
  static const JSClass& protoClass_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  static WasmGlobalObject* create(JSContext* cx, wasm::HandleVal value,
                                  bool isMutable, HandleObject proto);
  bool isNewborn() { return getReservedSlot(CELL_SLOT).isUndefined(); }

  wasm::ValType type() const;
  void setVal(JSContext* cx, wasm::HandleVal value);
  void val(wasm::MutableHandleVal outval) const;
  bool isMutable() const;
  bool value(JSContext* cx, MutableHandleValue out);
  Cell* cell() const;
};

// The class of WebAssembly.Instance. Each WasmInstanceObject owns a
// wasm::Instance. These objects are used both as content-facing JS objects and
// as internal implementation details of asm.js.

class WasmInstanceObject : public NativeObject {
  static const unsigned INSTANCE_SLOT = 0;
  static const unsigned EXPORTS_OBJ_SLOT = 1;
  static const unsigned EXPORTS_SLOT = 2;
  static const unsigned SCOPES_SLOT = 3;
  static const unsigned INSTANCE_SCOPE_SLOT = 4;
  static const unsigned GLOBALS_SLOT = 5;

  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
  static bool exportsGetterImpl(JSContext* cx, const CallArgs& args);
  static bool exportsGetter(JSContext* cx, unsigned argc, Value* vp);
  bool isNewborn() const;
  static void finalize(JSFreeOp* fop, JSObject* obj);
  static void trace(JSTracer* trc, JSObject* obj);

  // ExportMap maps from function index to exported function object.
  // This allows the instance to lazily create exported function
  // objects on demand (instead up-front for all table elements) while
  // correctly preserving observable function object identity.
  using ExportMap = GCHashMap<uint32_t, HeapPtr<JSFunction*>,
                              DefaultHasher<uint32_t>, ZoneAllocPolicy>;
  ExportMap& exports() const;

  // WeakScopeMap maps from function index to js::Scope. This maps is weak
  // to avoid holding scope objects alive. The scopes are normally created
  // during debugging.
  using ScopeMap =
      JS::WeakCache<GCHashMap<uint32_t, WeakHeapPtr<WasmFunctionScope*>,
                              DefaultHasher<uint32_t>, ZoneAllocPolicy>>;
  ScopeMap& scopes() const;

 public:
  static const unsigned RESERVED_SLOTS = 6;
  static const JSClass class_;
  static const JSClass& protoClass_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  static WasmInstanceObject* create(
      JSContext* cx, RefPtr<const wasm::Code> code,
      const wasm::DataSegmentVector& dataSegments,
      const wasm::ElemSegmentVector& elemSegments, wasm::UniqueTlsData tlsData,
      HandleWasmMemoryObject memory,
      Vector<RefPtr<wasm::Table>, 0, SystemAllocPolicy>&& tables,
      GCVector<HeapPtr<StructTypeDescr*>, 0, SystemAllocPolicy>&&
          structTypeDescrs,
      const JSFunctionVector& funcImports,
      const wasm::GlobalDescVector& globals,
      const wasm::ValVector& globalImportValues,
      const WasmGlobalObjectVector& globalObjs, HandleObject proto,
      UniquePtr<wasm::DebugState> maybeDebug);
  void initExportsObj(JSObject& exportsObj);

  wasm::Instance& instance() const;
  JSObject& exportsObj() const;

  static bool getExportedFunction(JSContext* cx,
                                  HandleWasmInstanceObject instanceObj,
                                  uint32_t funcIndex,
                                  MutableHandleFunction fun);

  const wasm::CodeRange& getExportedFunctionCodeRange(JSFunction* fun,
                                                      wasm::Tier tier);

  static WasmInstanceScope* getScope(JSContext* cx,
                                     HandleWasmInstanceObject instanceObj);
  static WasmFunctionScope* getFunctionScope(
      JSContext* cx, HandleWasmInstanceObject instanceObj, uint32_t funcIndex);

  using GlobalObjectVector =
      GCVector<HeapPtr<WasmGlobalObject*>, 0, ZoneAllocPolicy>;
  GlobalObjectVector& indirectGlobals() const;
};

// The class of WebAssembly.Memory. A WasmMemoryObject references an ArrayBuffer
// or SharedArrayBuffer object which owns the actual memory.

class WasmMemoryObject : public NativeObject {
  static const unsigned BUFFER_SLOT = 0;
  static const unsigned OBSERVERS_SLOT = 1;
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
  static void finalize(JSFreeOp* fop, JSObject* obj);
  static bool bufferGetterImpl(JSContext* cx, const CallArgs& args);
  static bool bufferGetter(JSContext* cx, unsigned argc, Value* vp);
  static bool growImpl(JSContext* cx, const CallArgs& args);
  static bool grow(JSContext* cx, unsigned argc, Value* vp);
  static uint32_t growShared(HandleWasmMemoryObject memory, uint32_t delta);

  using InstanceSet =
      JS::WeakCache<GCHashSet<WeakHeapPtrWasmInstanceObject,
                              MovableCellHasher<WeakHeapPtrWasmInstanceObject>,
                              ZoneAllocPolicy>>;
  bool hasObservers() const;
  InstanceSet& observers() const;
  InstanceSet* getOrCreateObservers(JSContext* cx);

 public:
  static const unsigned RESERVED_SLOTS = 2;
  static const JSClass class_;
  static const JSClass& protoClass_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  static WasmMemoryObject* create(JSContext* cx,
                                  Handle<ArrayBufferObjectMaybeShared*> buffer,
                                  HandleObject proto);

  // `buffer()` returns the current buffer object always.  If the buffer
  // represents shared memory then `buffer().byteLength()` never changes, and
  // in particular it may be a smaller value than that returned from
  // `volatileMemoryLength()` below.
  //
  // Generally, you do not want to call `buffer().byteLength()`, but to call
  // `volatileMemoryLength()`, instead.
  ArrayBufferObjectMaybeShared& buffer() const;

  // The current length of the memory.  In the case of shared memory, the
  // length can change at any time.  Also note that this will acquire a lock
  // for shared memory, so do not call this from a signal handler.
  uint32_t volatileMemoryLength() const;

  bool isShared() const;
  bool isHuge() const;
  bool movingGrowable() const;
  uint32_t boundsCheckLimit() const;

  // If isShared() is true then obtain the underlying buffer object.
  SharedArrayRawBuffer* sharedArrayRawBuffer() const;

  bool addMovingGrowObserver(JSContext* cx, WasmInstanceObject* instance);
  static uint32_t grow(HandleWasmMemoryObject memory, uint32_t delta,
                       JSContext* cx);
};

// The class of WebAssembly.Table. A WasmTableObject holds a refcount on a
// wasm::Table, allowing a Table to be shared between multiple Instances
// (eventually between multiple threads).

class WasmTableObject : public NativeObject {
  static const unsigned TABLE_SLOT = 0;
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
  bool isNewborn() const;
  static void finalize(JSFreeOp* fop, JSObject* obj);
  static void trace(JSTracer* trc, JSObject* obj);
  static bool lengthGetterImpl(JSContext* cx, const CallArgs& args);
  static bool lengthGetter(JSContext* cx, unsigned argc, Value* vp);
  static bool getImpl(JSContext* cx, const CallArgs& args);
  static bool get(JSContext* cx, unsigned argc, Value* vp);
  static bool setImpl(JSContext* cx, const CallArgs& args);
  static bool set(JSContext* cx, unsigned argc, Value* vp);
  static bool growImpl(JSContext* cx, const CallArgs& args);
  static bool grow(JSContext* cx, unsigned argc, Value* vp);

 public:
  static const unsigned RESERVED_SLOTS = 1;
  static const JSClass class_;
  static const JSClass& protoClass_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  // Note that, after creation, a WasmTableObject's table() is not initialized
  // and must be initialized before use.

  static WasmTableObject* create(JSContext* cx, const wasm::Limits& limits,
                                 wasm::TableKind tableKind, HandleObject proto);
  wasm::Table& table() const;
};

}  // namespace js

#endif  // wasm_js_h
