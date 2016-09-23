/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#include "asmjs/WasmTypes.h"
#include "gc/Policy.h"
#include "vm/NativeObject.h"

namespace js {

class TypedArrayObject;

namespace wasm {

// Creates a testing-only NaN JS object with fields as described above, for
// T=float or T=double.

template<typename T>
JSObject*
CreateCustomNaNObject(JSContext* cx, T* addr);

// Converts a testing-only NaN JS object with a nan_low field to a float32 NaN
// with nan_low as the payload.

bool
ReadCustomFloat32NaNObject(JSContext* cx, HandleValue v, uint32_t* ret);

// Converts a testing-only NaN JS object with nan_{low,high} components to a
// double NaN with nan_low|(nan_high)>>32 as the payload.

bool
ReadCustomDoubleNaNObject(JSContext* cx, HandleValue v, uint64_t* ret);

// Creates a JS object containing two fields (low: low 32 bits; high: high 32
// bits) of a given Int64 value. For testing purposes only.

JSObject*
CreateI64Object(JSContext* cx, int64_t i64);

// Reads an int64 from a JS object with the same shape as described in the
// comment above. For testing purposes only.

bool
ReadI64Object(JSContext* cx, HandleValue v, int64_t* i64);

// Return whether WebAssembly can be compiled on this platform.
// This must be checked and must be true to call any of the top-level wasm
// eval/compile methods.

bool
HasCompilerSupport(ExclusiveContext* cx);

// Compiles the given binary wasm module given the ArrayBufferObject
// and links the module's imports with the given import object.

MOZ_MUST_USE bool
Eval(JSContext* cx, Handle<TypedArrayObject*> code, HandleObject importObj,
     MutableHandleWasmInstanceObject instanceObj);

// The field name of the export object on the instance object.

extern const char InstanceExportField[];

// These accessors can be used to probe JS values for being an exported wasm
// function.

extern bool
IsExportedFunction(JSFunction* fun);

extern bool
IsExportedWasmFunction(JSFunction* fun);

extern bool
IsExportedFunction(const Value& v, MutableHandleFunction f);

extern Instance&
ExportedFunctionToInstance(JSFunction* fun);

extern WasmInstanceObject*
ExportedFunctionToInstanceObject(JSFunction* fun);

extern uint32_t
ExportedFunctionToDefinitionIndex(JSFunction* fun);

} // namespace wasm

// 'Wasm' and its one function 'instantiateModule' are transitional APIs and
// will be removed (replaced by 'WebAssembly') before release.

extern const Class WasmClass;

JSObject*
InitWasmClass(JSContext* cx, HandleObject global);

// The class of the WebAssembly global namespace object.

extern const Class WebAssemblyClass;

JSObject*
InitWebAssemblyClass(JSContext* cx, HandleObject global);

// The class of WebAssembly.Module. Each WasmModuleObject owns a
// wasm::Module. These objects are used both as content-facing JS objects and as
// internal implementation details of asm.js.

class WasmModuleObject : public NativeObject
{
    static const unsigned MODULE_SLOT = 0;
    static const ClassOps classOps_;
    static void finalize(FreeOp* fop, JSObject* obj);
  public:
    static const unsigned RESERVED_SLOTS = 1;
    static const Class class_;
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static bool construct(JSContext*, unsigned, Value*);

    static WasmModuleObject* create(ExclusiveContext* cx,
                                    wasm::Module& module,
                                    HandleObject proto = nullptr);
    wasm::Module& module() const;
};

// The class of WebAssembly.Instance. Each WasmInstanceObject owns a
// wasm::Instance. These objects are used both as content-facing JS objects and
// as internal implementation details of asm.js.

class WasmInstanceObject : public NativeObject
{
    static const unsigned INSTANCE_SLOT = 0;
    static const unsigned EXPORTS_SLOT = 1;
    static const ClassOps classOps_;
    bool isNewborn() const;
    static void finalize(FreeOp* fop, JSObject* obj);
    static void trace(JSTracer* trc, JSObject* obj);

    // ExportMap maps from function definition index to exported function
    // object. This map is weak to avoid holding objects alive; the point is
    // just to ensure a unique object identity for any given function object.
    using ExportMap = GCHashMap<uint32_t,
                                ReadBarrieredFunction,
                                DefaultHasher<uint32_t>,
                                SystemAllocPolicy>;
    using WeakExportMap = JS::WeakCache<ExportMap>;
    WeakExportMap& exports() const;

  public:
    static const unsigned RESERVED_SLOTS = 2;
    static const Class class_;
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static bool construct(JSContext*, unsigned, Value*);

    static WasmInstanceObject* create(JSContext* cx,
                                      UniquePtr<wasm::Code> code,
                                      HandleWasmMemoryObject memory,
                                      Vector<RefPtr<wasm::Table>, 0, SystemAllocPolicy>&& tables,
                                      Handle<FunctionVector> funcImports,
                                      const wasm::ValVector& globalImports,
                                      HandleObject proto);
    wasm::Instance& instance() const;

    static bool getExportedFunction(JSContext* cx,
                                    HandleWasmInstanceObject instanceObj,
                                    uint32_t funcIndex,
                                    MutableHandleFunction fun);

    const wasm::CodeRange& getExportedFunctionCodeRange(HandleFunction fun);
};

// The class of WebAssembly.Memory. A WasmMemoryObject references an ArrayBuffer
// or SharedArrayBuffer object which owns the actual memory.

class WasmMemoryObject : public NativeObject
{
    static const unsigned BUFFER_SLOT = 0;
    static const unsigned OBSERVERS_SLOT = 1;
    static const ClassOps classOps_;
    static void finalize(FreeOp* fop, JSObject* obj);
    static bool bufferGetterImpl(JSContext* cx, const CallArgs& args);
    static bool bufferGetter(JSContext* cx, unsigned argc, Value* vp);
    static bool growImpl(JSContext* cx, const CallArgs& args);
    static bool grow(JSContext* cx, unsigned argc, Value* vp);

    using InstanceSet = GCHashSet<ReadBarrieredWasmInstanceObject,
                                  MovableCellHasher<ReadBarrieredWasmInstanceObject>,
                                  SystemAllocPolicy>;
    using WeakInstanceSet = JS::WeakCache<InstanceSet>;
    bool hasObservers() const;
    WeakInstanceSet& observers() const;
    WeakInstanceSet* getOrCreateObservers(JSContext* cx);

  public:
    static const unsigned RESERVED_SLOTS = 2;
    static const Class class_;
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static bool construct(JSContext*, unsigned, Value*);

    static WasmMemoryObject* create(ExclusiveContext* cx,
                                    Handle<ArrayBufferObjectMaybeShared*> buffer,
                                    HandleObject proto);
    ArrayBufferObjectMaybeShared& buffer() const;

    bool movingGrowable() const;
    bool addMovingGrowObserver(JSContext* cx, WasmInstanceObject* instance);
    static uint32_t grow(HandleWasmMemoryObject memory, uint32_t delta, JSContext* cx);
};

// The class of WebAssembly.Table. A WasmTableObject holds a refcount on a
// wasm::Table, allowing a Table to be shared between multiple Instances
// (eventually between multiple threads).

class WasmTableObject : public NativeObject
{
    static const unsigned TABLE_SLOT = 0;
    static const ClassOps classOps_;
    bool isNewborn() const;
    static void finalize(FreeOp* fop, JSObject* obj);
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
    static const Class class_;
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static bool construct(JSContext*, unsigned, Value*);

    // Note that, after creation, a WasmTableObject's table() is not initialized
    // and must be initialized before use.

    static WasmTableObject* create(JSContext* cx, wasm::ResizableLimits limits);
    wasm::Table& table() const;
};

} // namespace js

#endif // wasm_js_h
