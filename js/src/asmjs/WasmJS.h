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

#include "js/UniquePtr.h"
#include "vm/NativeObject.h"

namespace js {

class TypedArrayObject;
class WasmInstanceObject;

namespace wasm {

// This is a widespread header, so keep out core wasm impl definitions.

class Module;
class Instance;
class Table;

typedef UniquePtr<Instance> UniqueInstance;

// Return whether WebAssembly can be compiled on this platform.
// This must be checked and must be true to call any of the top-level wasm
// eval/compile methods.

bool
HasCompilerSupport(ExclusiveContext* cx);

// Compiles the given binary wasm module given the ArrayBufferObject
// and links the module's imports with the given import object.

MOZ_MUST_USE bool
Eval(JSContext* cx, Handle<TypedArrayObject*> code, HandleObject importObj,
     MutableHandle<WasmInstanceObject*> instanceObj);

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
    static bool construct(JSContext*, unsigned, Value*);

    static WasmModuleObject* create(ExclusiveContext* cx,
                                    wasm::Module& module,
                                    HandleObject proto = nullptr);
    wasm::Module& module() const;
};

typedef Rooted<WasmModuleObject*> RootedWasmModuleObject;
typedef Handle<WasmModuleObject*> HandleWasmModuleObject;
typedef MutableHandle<WasmModuleObject*> MutableHandleWasmModuleObject;

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
  public:
    static const unsigned RESERVED_SLOTS = 2;
    static const Class class_;
    static const JSPropertySpec properties[];
    static bool construct(JSContext*, unsigned, Value*);

    static WasmInstanceObject* create(ExclusiveContext* cx,
                                      HandleObject proto = nullptr);
    void init(wasm::UniqueInstance module);
    void initExportsObject(HandleObject exportObj);
    wasm::Instance& instance() const;
    JSObject& exportsObject() const;
};

typedef GCVector<WasmInstanceObject*> WasmInstanceObjectVector;
typedef Rooted<WasmInstanceObject*> RootedWasmInstanceObject;
typedef Handle<WasmInstanceObject*> HandleWasmInstanceObject;
typedef MutableHandle<WasmInstanceObject*> MutableHandleWasmInstanceObject;

// The class of WebAssembly.Memory. A WasmMemoryObject references an ArrayBuffer
// or SharedArrayBuffer object which owns the actual memory.

class WasmMemoryObject : public NativeObject
{
    static const unsigned BUFFER_SLOT = 0;
    static const ClassOps classOps_;
  public:
    static const unsigned RESERVED_SLOTS = 1;
    static const Class class_;
    static const JSPropertySpec properties[];
    static bool construct(JSContext*, unsigned, Value*);

    static WasmMemoryObject* create(ExclusiveContext* cx,
                                    Handle<ArrayBufferObjectMaybeShared*> buffer,
                                    HandleObject proto);
    ArrayBufferObjectMaybeShared& buffer() const;
};

typedef GCPtr<WasmMemoryObject*> GCPtrWasmMemoryObject;
typedef Rooted<WasmMemoryObject*> RootedWasmMemoryObject;
typedef Handle<WasmMemoryObject*> HandleWasmMemoryObject;
typedef MutableHandle<WasmMemoryObject*> MutableHandleWasmMemoryObject;

// The class of WebAssembly.Table. A WasmTableObject holds a refcount on a
// wasm::Table, allowing a Table to be shared between multiple Instances
// (eventually between multiple threads).

class WasmTableObject : public NativeObject
{
    static const unsigned TABLE_SLOT = 0;
  public:
    static const unsigned RESERVED_SLOTS = 1;
    static const Class class_;
    static const JSPropertySpec properties[];
    static bool construct(JSContext*, unsigned, Value*);

    static WasmTableObject* create(JSContext* cx, wasm::Table& table);
    wasm::Table& table() const;
};

typedef Rooted<WasmTableObject*> RootedWasmTableObject;
typedef Handle<WasmTableObject*> HandleWasmTableObject;
typedef MutableHandle<WasmTableObject*> MutableHandleWasmTableObject;

} // namespace js

#endif // wasm_js_h
