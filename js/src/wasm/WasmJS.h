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
class GlobalObject;
class StructTypeDescr;
class TypedArrayObject;
class WasmFunctionScope;
class WasmInstanceScope;
class SharedArrayRawBuffer;

namespace wasm {

// Return whether WebAssembly can be compiled on this platform.
// This must be checked and must be true to call any of the top-level wasm
// eval/compile methods.

bool HasCompilerSupport(JSContext* cx);

// Return whether WebAssembly has support for an optimized compiler backend.

bool HasOptimizedCompilerTier(JSContext* cx);

// Return whether WebAssembly is supported on this platform. This determines
// whether the WebAssembly object is exposed to JS and takes into account
// configuration options that disable various modes.

bool HasSupport(JSContext* cx);

// Return whether WebAssembly streaming/caching is supported on this platform.
// This takes into account prefs and necessary embedding callbacks.

bool HasStreamingSupport(JSContext* cx);

bool HasCachingSupport(JSContext* cx);

// Returns true if WebAssembly as configured by compile-time flags and run-time
// options can support reference types and stack walking.

bool HasReftypesSupport(JSContext* cx);

// Returns true if WebAssembly as configured by compile-time flags and run-time
// options can support (ref T) types and structure types, etc (evolving).

bool HasGcSupport(JSContext* cx);

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
bool CheckFuncRefValue(JSContext* cx, HandleValue v, MutableHandleFunction fun);

Instance& ExportedFunctionToInstance(JSFunction* fun);
WasmInstanceObject* ExportedFunctionToInstanceObject(JSFunction* fun);
uint32_t ExportedFunctionToFuncIndex(JSFunction* fun);

bool IsSharedWasmMemoryObject(JSObject* obj);

}  // namespace wasm

// The class of the WebAssembly global namespace object.

extern const JSClass WebAssemblyClass;

JSObject* InitWebAssemblyClass(JSContext* cx, Handle<GlobalObject*> global);

// The class of WebAssembly.Module. Each WasmModuleObject owns a
// wasm::Module. These objects are used both as content-facing JS objects and as
// internal implementation details of asm.js.

class WasmModuleObject : public NativeObject {
  static const unsigned MODULE_SLOT = 0;
  static const JSClassOps classOps_;
  static void finalize(JSFreeOp* fop, JSObject* obj);
  static bool imports(JSContext* cx, unsigned argc, Value* vp);
  static bool exports(JSContext* cx, unsigned argc, Value* vp);
  static bool customSections(JSContext* cx, unsigned argc, Value* vp);

 public:
  static const unsigned RESERVED_SLOTS = 1;
  static const JSClass class_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  static WasmModuleObject* create(JSContext* cx, const wasm::Module& module,
                                  HandleObject proto = nullptr);
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
    wasm::AnyRef ref;
    Cell() : i64(0) {}
    ~Cell() {}
  };

  static const unsigned RESERVED_SLOTS = 3;
  static const JSClass class_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  static WasmGlobalObject* create(JSContext* cx, wasm::HandleVal value,
                                  bool isMutable);
  bool isNewborn() { return getReservedSlot(CELL_SLOT).isUndefined(); }

  wasm::ValType type() const;
  void val(wasm::MutableHandleVal outval) const;
  bool isMutable() const;
  // value() will MOZ_CRASH if the type is int64
  Value value(JSContext* cx) const;
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

  using GlobalObjectVector = GCVector<WasmGlobalObject*, 0, ZoneAllocPolicy>;
  GlobalObjectVector& indirectGlobals() const;
};

// The class of WebAssembly.Memory. A WasmMemoryObject references an ArrayBuffer
// or SharedArrayBuffer object which owns the actual memory.

class WasmMemoryObject : public NativeObject {
  static const unsigned BUFFER_SLOT = 0;
  static const unsigned OBSERVERS_SLOT = 1;
  static const JSClassOps classOps_;
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
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];
  static bool construct(JSContext*, unsigned, Value*);

  // Note that, after creation, a WasmTableObject's table() is not initialized
  // and must be initialized before use.

  static WasmTableObject* create(JSContext* cx, const wasm::Limits& limits,
                                 wasm::TableKind tableKind);
  wasm::Table& table() const;
};

}  // namespace js

#endif  // wasm_js_h
