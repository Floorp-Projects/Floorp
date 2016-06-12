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
namespace wasm {

class Module;
class Instance;

typedef UniquePtr<Module> UniqueModule;
typedef UniquePtr<Instance> UniqueInstance;

} // namespace wasm

// The class of the Wasm global namespace object.

extern const Class WasmClass;

JSObject*
InitWasmClass(JSContext* cx, HandleObject global);

// The class of wasm module object wrappers. Each WasmModuleObject owns a
// wasm::Module. These objects are currently not exposed directly to JS.

class WasmModuleObject : public NativeObject
{
    static const unsigned MODULE_SLOT = 0;
    static const ClassOps classOps_;
    static void finalize(FreeOp* fop, JSObject* obj);
  public:
    static const unsigned RESERVED_SLOTS = 1;
    static const Class class_;

    static WasmModuleObject* create(ExclusiveContext* cx, wasm::UniqueModule module);
    wasm::Module& module() const;
};

typedef Rooted<WasmModuleObject*> RootedWasmModuleObject;
typedef Handle<WasmModuleObject*> HandleWasmModuleObject;
typedef MutableHandle<WasmModuleObject*> MutableHandleWasmModuleObject;

// The class of wasm instance object wrappers. Each WasmInstanceObject owns a
// wasm::Instance. These objects are currently not exposed directly to JS.

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

    static WasmInstanceObject* create(ExclusiveContext* cx);
    void init(wasm::UniqueInstance module);
    void initExportsObject(HandleObject exportObj);
    wasm::Instance& instance() const;
    JSObject& exportsObject() const;
};

typedef GCVector<WasmInstanceObject*> WasmInstanceObjectVector;
typedef Rooted<WasmInstanceObject*> RootedWasmInstanceObject;
typedef Handle<WasmInstanceObject*> HandleWasmInstanceObject;
typedef MutableHandle<WasmInstanceObject*> MutableHandleWasmInstanceObject;

} // namespace js

#endif // wasm_js_h
