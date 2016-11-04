/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef wasm_stubs_h
#define wasm_stubs_h

#include "wasm/WasmTypes.h"

namespace js {

namespace jit { class MacroAssembler; class Label; }

namespace wasm {

class FuncExport;
class FuncImport;

extern Offsets
GenerateEntry(jit::MacroAssembler& masm, const FuncExport& fe);

extern FuncOffsets
GenerateImportFunction(jit::MacroAssembler& masm, const FuncImport& fi, SigIdDesc sigId);

extern ProfilingOffsets
GenerateImportInterpExit(jit::MacroAssembler& masm, const FuncImport& fi, uint32_t funcImportIndex,
                         jit::Label* throwLabel);

extern ProfilingOffsets
GenerateImportJitExit(jit::MacroAssembler& masm, const FuncImport& fi, jit::Label* throwLabel);

extern ProfilingOffsets
GenerateTrapExit(jit::MacroAssembler& masm, Trap trap, jit::Label* throwLabel);

extern Offsets
GenerateOutOfBoundsExit(jit::MacroAssembler& masm, jit::Label* throwLabel);

extern Offsets
GenerateUnalignedExit(jit::MacroAssembler& masm, jit::Label* throwLabel);

extern Offsets
GenerateInterruptExit(jit::MacroAssembler& masm, jit::Label* throwLabel);

extern Offsets
GenerateThrowStub(jit::MacroAssembler& masm, jit::Label* throwLabel);

} // namespace wasm
} // namespace js

#endif // wasm_stubs_h
