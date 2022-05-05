/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2021 Mozilla Foundation
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

#ifndef wasm_type_decls_h
#define wasm_type_decls_h

#include "NamespaceImports.h"
#include "gc/Barrier.h"
#include "js/GCVector.h"
#include "js/HashTable.h"
#include "js/RootingAPI.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Vector.h"

namespace js {

using JSFunctionVector = GCVector<JSFunction*, 0, SystemAllocPolicy>;

class WasmMemoryObject;
using GCPtrWasmMemoryObject = GCPtr<WasmMemoryObject*>;
using RootedWasmMemoryObject = Rooted<WasmMemoryObject*>;
using HandleWasmMemoryObject = Handle<WasmMemoryObject*>;
using MutableHandleWasmMemoryObject = MutableHandle<WasmMemoryObject*>;

class WasmModuleObject;
using RootedWasmModuleObject = Rooted<WasmModuleObject*>;
using HandleWasmModuleObject = Handle<WasmModuleObject*>;
using MutableHandleWasmModuleObject = MutableHandle<WasmModuleObject*>;

class WasmInstanceObject;
using WasmInstanceObjectVector = GCVector<WasmInstanceObject*>;
using RootedWasmInstanceObject = Rooted<WasmInstanceObject*>;
using HandleWasmInstanceObject = Handle<WasmInstanceObject*>;
using MutableHandleWasmInstanceObject = MutableHandle<WasmInstanceObject*>;

class WasmTableObject;
using WasmTableObjectVector = GCVector<WasmTableObject*, 0, SystemAllocPolicy>;
using RootedWasmTableObject = Rooted<WasmTableObject*>;
using HandleWasmTableObject = Handle<WasmTableObject*>;
using MutableHandleWasmTableObject = MutableHandle<WasmTableObject*>;

class WasmGlobalObject;
using WasmGlobalObjectVector =
    GCVector<WasmGlobalObject*, 0, SystemAllocPolicy>;
using RootedWasmGlobalObject = Rooted<WasmGlobalObject*>;

class WasmTagObject;
using GCPtrWasmTagObject = GCPtr<WasmTagObject*>;
using WasmTagObjectVector = GCVector<WasmTagObject*, 0, SystemAllocPolicy>;
using RootedWasmTagObject = Rooted<WasmTagObject*>;
using HandleWasmTagObject = Handle<WasmTagObject*>;

class WasmExceptionObject;
using RootedWasmExceptionObject = Rooted<WasmExceptionObject*>;

class RttValue;
using HandleRttValue = Handle<RttValue*>;
using MutableHandleRttValue = MutableHandle<RttValue*>;

namespace wasm {

struct ModuleEnvironment;
class CodeRange;
class CodeTier;
class ModuleSegment;
struct Metadata;
struct MetadataTier;
class Decoder;
class GeneratedSourceMap;
class Instance;
class Module;

class Code;
using SharedCode = RefPtr<const Code>;
using MutableCode = RefPtr<Code>;

class Table;
using SharedTable = RefPtr<Table>;
using SharedTableVector = Vector<SharedTable, 0, SystemAllocPolicy>;

class DebugState;
using UniqueDebugState = UniquePtr<DebugState>;

struct DataSegment;
using MutableDataSegment = RefPtr<DataSegment>;
using SharedDataSegment = RefPtr<const DataSegment>;
using DataSegmentVector = Vector<SharedDataSegment, 0, SystemAllocPolicy>;

struct ElemSegment;
using MutableElemSegment = RefPtr<ElemSegment>;
using SharedElemSegment = RefPtr<const ElemSegment>;
using ElemSegmentVector = Vector<SharedElemSegment, 0, SystemAllocPolicy>;

struct ExceptionTag;
using SharedExceptionTag = RefPtr<ExceptionTag>;
using SharedExceptionTagVector =
    Vector<SharedExceptionTag, 0, SystemAllocPolicy>;

class Val;
using ValVector = GCVector<Val, 0, SystemAllocPolicy>;

// Uint32Vector has initial size 8 on the basis that the dominant use cases
// (line numbers and control stacks) tend to have a small but nonzero number
// of elements.
using Uint32Vector = Vector<uint32_t, 8, SystemAllocPolicy>;

using Bytes = Vector<uint8_t, 0, SystemAllocPolicy>;
using UniqueBytes = UniquePtr<Bytes>;
using UniqueConstBytes = UniquePtr<const Bytes>;
using UTF8Bytes = Vector<char, 0, SystemAllocPolicy>;
using InstanceVector = Vector<Instance*, 0, SystemAllocPolicy>;
using UniqueCharsVector = Vector<UniqueChars, 0, SystemAllocPolicy>;
using RenumberVector = Vector<uint32_t, 0, SystemAllocPolicy>;

}  // namespace wasm
}  // namespace js

#endif  // wasm_type_decls_h
