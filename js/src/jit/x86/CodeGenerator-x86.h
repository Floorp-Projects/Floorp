/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_CodeGenerator_x86_h
#define jit_x86_CodeGenerator_x86_h

#include "jit/x86-shared/CodeGenerator-x86-shared.h"
#include "jit/x86/Assembler-x86.h"

namespace js {
namespace jit {

class OutOfLineTruncate;
class OutOfLineTruncateFloat32;

class CodeGeneratorX86 : public CodeGeneratorX86Shared
{
  protected:
    CodeGeneratorX86(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm);

    ValueOperand ToValue(LInstruction* ins, size_t pos);
    ValueOperand ToTempValue(LInstruction* ins, size_t pos);

    template <typename T> void emitWasmLoad(T* ins);
    template <typename T> void emitWasmStore(T* ins);
    template <typename T> void emitWasmStoreOrExchangeAtomicI64(T* ins, const wasm::MemoryAccessDesc& access);

  public:
    void visitOutOfLineTruncate(OutOfLineTruncate* ool);
    void visitOutOfLineTruncateFloat32(OutOfLineTruncateFloat32* ool);
};

typedef CodeGeneratorX86 CodeGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_x86_CodeGenerator_x86_h */
