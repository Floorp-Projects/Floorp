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

#include "wasm/WasmValidate.h"

#include "mozilla/CheckedInt.h"

#include "jsprf.h"

#include "jit/JitOptions.h"
#include "wasm/WasmBinaryIterator.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;

// Decoder implementation.

bool
Decoder::fail(const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    UniqueChars str(JS_vsmprintf(msg, ap));
    va_end(ap);
    if (!str)
        return false;

    return fail(Move(str));
}

bool
Decoder::fail(UniqueChars msg)
{
    MOZ_ASSERT(error_);
    UniqueChars strWithOffset(JS_smprintf("at offset %" PRIuSIZE ": %s", currentOffset(), msg.get()));
    if (!strWithOffset)
        return false;

    *error_ = Move(strWithOffset);
    return false;
}

bool
Decoder::startSection(SectionId id, ModuleEnvironment* env, uint32_t* sectionStart,
                      uint32_t* sectionSize, const char* sectionName)
{
    // Record state at beginning of section to allow rewinding to this point
    // if, after skipping through several custom sections, we don't find the
    // section 'id'.
    const uint8_t* const initialCur = cur_;
    const size_t initialCustomSectionsLength = env->customSections.length();

    // Maintain a pointer to the current section that gets updated as custom
    // sections are skipped.
    const uint8_t* currentSectionStart = cur_;

    // Only start a section with 'id', skipping any custom sections before it.

    uint32_t idValue;
    if (!readVarU32(&idValue))
        goto rewind;

    while (idValue != uint32_t(id)) {
        if (idValue != uint32_t(SectionId::Custom))
            goto rewind;

        // Rewind to the beginning of the current section since this is what
        // skipCustomSection() assumes.
        cur_ = currentSectionStart;
        if (!skipCustomSection(env))
            return false;

        // Having successfully skipped a custom section, consider the next
        // section.
        currentSectionStart = cur_;
        if (!readVarU32(&idValue))
            goto rewind;
    }

    // Found it, now start the section.

    if (!readVarU32(sectionSize) || bytesRemain() < *sectionSize)
        goto fail;

    *sectionStart = cur_ - beg_;
    return true;

  rewind:
    cur_ = initialCur;
    env->customSections.shrinkTo(initialCustomSectionsLength);
    *sectionStart = NotStarted;
    return true;

  fail:
    return fail("failed to start %s section", sectionName);
}

bool
Decoder::finishSection(uint32_t sectionStart, uint32_t sectionSize, const char* sectionName)
{
    if (resilientMode_)
        return true;
    if (sectionSize != (cur_ - beg_) - sectionStart)
        return fail("byte size mismatch in %s section", sectionName);
    return true;
}

bool
Decoder::startCustomSection(const char* expected, size_t expectedLength, ModuleEnvironment* env,
                            uint32_t* sectionStart, uint32_t* sectionSize)
{
    // Record state at beginning of section to allow rewinding to this point
    // if, after skipping through several custom sections, we don't find the
    // section 'id'.
    const uint8_t* const initialCur = cur_;
    const size_t initialCustomSectionsLength = env->customSections.length();

    while (true) {
        // Try to start a custom section. If we can't, rewind to the beginning
        // since we may have skipped several custom sections already looking for
        // 'expected'.
        if (!startSection(SectionId::Custom, env, sectionStart, sectionSize, "custom"))
            return false;
        if (*sectionStart == NotStarted)
            goto rewind;

        NameInBytecode name;
        if (!readVarU32(&name.length) || name.length > bytesRemain())
            goto fail;

        name.offset = currentOffset();
        uint32_t payloadOffset = name.offset + name.length;
        uint32_t payloadEnd = *sectionStart + *sectionSize;
        if (payloadOffset > payloadEnd)
            goto fail;

        // Now that we have a valid custom section, record its offsets in the
        // metadata which can be queried by the user via Module.customSections.
        // Note: after an entry is appended, it may be popped if this loop or
        // the loop in startSection needs to rewind.
        if (!env->customSections.emplaceBack(name, payloadOffset, payloadEnd - payloadOffset))
            return false;

        // If this is the expected custom section, we're done.
        if (!expected || (expectedLength == name.length && !memcmp(cur_, expected, name.length))) {
            cur_ += name.length;
            return true;
        }

        // Otherwise, blindly skip the custom section and keep looking.
        finishCustomSection(*sectionStart, *sectionSize);
    }
    MOZ_CRASH("unreachable");

  rewind:
    cur_ = initialCur;
    env->customSections.shrinkTo(initialCustomSectionsLength);
    return true;

  fail:
    return fail("failed to start custom section");
}

void
Decoder::finishCustomSection(uint32_t sectionStart, uint32_t sectionSize)
{
    MOZ_ASSERT(cur_ >= beg_);
    MOZ_ASSERT(cur_ <= end_);
    cur_ = (beg_ + sectionStart) + sectionSize;
    MOZ_ASSERT(cur_ <= end_);
    clearError();
}

bool
Decoder::skipCustomSection(ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!startCustomSection(nullptr, 0, env, &sectionStart, &sectionSize))
        return false;
    if (sectionStart == NotStarted)
        return fail("expected custom section");

    finishCustomSection(sectionStart, sectionSize);
    return true;
}

// Misc helpers.

bool
wasm::EncodeLocalEntries(Encoder& e, const ValTypeVector& locals)
{
    uint32_t numLocalEntries = 0;
    ValType prev = ValType(TypeCode::Limit);
    for (ValType t : locals) {
        if (t != prev) {
            numLocalEntries++;
            prev = t;
        }
    }

    if (!e.writeVarU32(numLocalEntries))
        return false;

    if (numLocalEntries) {
        prev = locals[0];
        uint32_t count = 1;
        for (uint32_t i = 1; i < locals.length(); i++, count++) {
            if (prev != locals[i]) {
                if (!e.writeVarU32(count))
                    return false;
                if (!e.writeValType(prev))
                    return false;
                prev = locals[i];
                count = 0;
            }
        }
        if (!e.writeVarU32(count))
            return false;
        if (!e.writeValType(prev))
            return false;
    }

    return true;
}

static bool
DecodeValType(Decoder& d, ModuleKind kind, ValType* type)
{
    uint8_t unchecked;
    if (!d.readValType(&unchecked))
        return false;

    switch (unchecked) {
      case uint8_t(ValType::I32):
      case uint8_t(ValType::F32):
      case uint8_t(ValType::F64):
      case uint8_t(ValType::I64):
        *type = ValType(unchecked);
        return true;
      case uint8_t(ValType::I8x16):
      case uint8_t(ValType::I16x8):
      case uint8_t(ValType::I32x4):
      case uint8_t(ValType::F32x4):
      case uint8_t(ValType::B8x16):
      case uint8_t(ValType::B16x8):
      case uint8_t(ValType::B32x4):
        if (kind != ModuleKind::AsmJS)
            return d.fail("bad type");
        *type = ValType(unchecked);
        return true;
      default:
        break;
    }
    return d.fail("bad type");
}

bool
wasm::DecodeLocalEntries(Decoder& d, ModuleKind kind, ValTypeVector* locals)
{
    uint32_t numLocalEntries;
    if (!d.readVarU32(&numLocalEntries))
        return d.fail("failed to read number of local entries");

    for (uint32_t i = 0; i < numLocalEntries; i++) {
        uint32_t count;
        if (!d.readVarU32(&count))
            return d.fail("failed to read local entry count");

        if (MaxLocals - locals->length() < count)
            return d.fail("too many locals");

        ValType type;
        if (!DecodeValType(d, kind, &type))
            return false;

        if (!locals->appendN(type, count))
            return false;
    }

    return true;
}

// Function body validation.

struct ValidatingPolicy : OpIterPolicy
{
    // Validation is what we're all about here.
    static const bool Validate = true;
};

typedef OpIter<ValidatingPolicy> ValidatingOpIter;

class FunctionDecoder
{
    const ModuleEnvironment& env_;
    const ValTypeVector& locals_;
    ValidatingOpIter iter_;

  public:
    FunctionDecoder(const ModuleEnvironment& env, const ValTypeVector& locals, Decoder& d)
      : env_(env), locals_(locals), iter_(d)
    {}

    const ModuleEnvironment& env() const { return env_; }
    ValidatingOpIter& iter() { return iter_; }
    const ValTypeVector& locals() const { return locals_; }

    bool checkHasMemory() {
        if (!env().usesMemory())
            return iter().fail("can't touch memory without memory");
        return true;
    }
};

static bool
DecodeCallArgs(FunctionDecoder& f, const Sig& sig)
{
    const ValTypeVector& args = sig.args();
    uint32_t numArgs = args.length();
    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        if (!f.iter().readCallArg(argType, numArgs, i, nullptr))
            return false;
    }

    return f.iter().readCallArgsEnd(numArgs);
}

static bool
DecodeCallReturn(FunctionDecoder& f, const Sig& sig)
{
    return f.iter().readCallReturn(sig.ret());
}

static bool
DecodeCall(FunctionDecoder& f)
{
    uint32_t funcIndex;
    if (!f.iter().readCall(&funcIndex))
        return false;

    if (funcIndex >= f.env().numFuncs())
        return f.iter().fail("callee index out of range");

    if (!f.iter().inReachableCode())
        return true;

    const Sig* sig = f.env().funcSigs[funcIndex];

    return DecodeCallArgs(f, *sig) &&
           DecodeCallReturn(f, *sig);
}

static bool
DecodeCallIndirect(FunctionDecoder& f)
{
    if (!f.env().numTables())
        return f.iter().fail("can't call_indirect without a table");

    uint32_t sigIndex;
    if (!f.iter().readCallIndirect(&sigIndex, nullptr))
        return false;

    if (sigIndex >= f.env().numSigs())
        return f.iter().fail("signature index out of range");

    if (!f.iter().inReachableCode())
        return true;

    const Sig& sig = f.env().sigs[sigIndex];
    if (!DecodeCallArgs(f, sig))
        return false;

    return DecodeCallReturn(f, sig);
}

static bool
DecodeBrTable(FunctionDecoder& f)
{
    uint32_t tableLength;
    ExprType type = ExprType::Limit;
    if (!f.iter().readBrTable(&tableLength, &type, nullptr, nullptr))
        return false;

    uint32_t depth;
    for (size_t i = 0, e = tableLength; i < e; ++i) {
        if (!f.iter().readBrTableEntry(&type, nullptr, &depth))
            return false;
    }

    // Read the default label.
    return f.iter().readBrTableDefault(&type, nullptr, &depth);
}

static bool
DecodeFunctionBodyExprs(FunctionDecoder& f)
{
#define CHECK(c) if (!(c)) return false; break

    while (true) {
        uint16_t op;
        if (!f.iter().readOp(&op))
            return false;

        switch (op) {
          case uint16_t(Op::End):
            if (!f.iter().readEnd(nullptr, nullptr, nullptr))
                return false;
            if (f.iter().controlStackEmpty())
                return true;
            break;
          case uint16_t(Op::Nop):
            CHECK(f.iter().readNop());
          case uint16_t(Op::Drop):
            CHECK(f.iter().readDrop());
          case uint16_t(Op::Call):
            CHECK(DecodeCall(f));
          case uint16_t(Op::CallIndirect):
            CHECK(DecodeCallIndirect(f));
          case uint16_t(Op::I32Const):
            CHECK(f.iter().readI32Const(nullptr));
          case uint16_t(Op::I64Const):
            CHECK(f.iter().readI64Const(nullptr));
          case uint16_t(Op::F32Const):
            CHECK(f.iter().readF32Const(nullptr));
          case uint16_t(Op::F64Const):
            CHECK(f.iter().readF64Const(nullptr));
          case uint16_t(Op::GetLocal):
            CHECK(f.iter().readGetLocal(f.locals(), nullptr));
          case uint16_t(Op::SetLocal):
            CHECK(f.iter().readSetLocal(f.locals(), nullptr, nullptr));
          case uint16_t(Op::TeeLocal):
            CHECK(f.iter().readTeeLocal(f.locals(), nullptr, nullptr));
          case uint16_t(Op::GetGlobal):
            CHECK(f.iter().readGetGlobal(f.env().globals, nullptr));
          case uint16_t(Op::SetGlobal):
            CHECK(f.iter().readSetGlobal(f.env().globals, nullptr, nullptr));
          case uint16_t(Op::Select):
            CHECK(f.iter().readSelect(nullptr, nullptr, nullptr, nullptr));
          case uint16_t(Op::Block):
            CHECK(f.iter().readBlock());
          case uint16_t(Op::Loop):
            CHECK(f.iter().readLoop());
          case uint16_t(Op::If):
            CHECK(f.iter().readIf(nullptr));
          case uint16_t(Op::Else):
            CHECK(f.iter().readElse(nullptr, nullptr));
          case uint16_t(Op::I32Clz):
          case uint16_t(Op::I32Ctz):
          case uint16_t(Op::I32Popcnt):
            CHECK(f.iter().readUnary(ValType::I32, nullptr));
          case uint16_t(Op::I64Clz):
          case uint16_t(Op::I64Ctz):
          case uint16_t(Op::I64Popcnt):
            CHECK(f.iter().readUnary(ValType::I64, nullptr));
          case uint16_t(Op::F32Abs):
          case uint16_t(Op::F32Neg):
          case uint16_t(Op::F32Ceil):
          case uint16_t(Op::F32Floor):
          case uint16_t(Op::F32Sqrt):
          case uint16_t(Op::F32Trunc):
          case uint16_t(Op::F32Nearest):
            CHECK(f.iter().readUnary(ValType::F32, nullptr));
          case uint16_t(Op::F64Abs):
          case uint16_t(Op::F64Neg):
          case uint16_t(Op::F64Ceil):
          case uint16_t(Op::F64Floor):
          case uint16_t(Op::F64Sqrt):
          case uint16_t(Op::F64Trunc):
          case uint16_t(Op::F64Nearest):
            CHECK(f.iter().readUnary(ValType::F64, nullptr));
          case uint16_t(Op::I32Add):
          case uint16_t(Op::I32Sub):
          case uint16_t(Op::I32Mul):
          case uint16_t(Op::I32DivS):
          case uint16_t(Op::I32DivU):
          case uint16_t(Op::I32RemS):
          case uint16_t(Op::I32RemU):
          case uint16_t(Op::I32And):
          case uint16_t(Op::I32Or):
          case uint16_t(Op::I32Xor):
          case uint16_t(Op::I32Shl):
          case uint16_t(Op::I32ShrS):
          case uint16_t(Op::I32ShrU):
          case uint16_t(Op::I32Rotl):
          case uint16_t(Op::I32Rotr):
            CHECK(f.iter().readBinary(ValType::I32, nullptr, nullptr));
          case uint16_t(Op::I64Add):
          case uint16_t(Op::I64Sub):
          case uint16_t(Op::I64Mul):
          case uint16_t(Op::I64DivS):
          case uint16_t(Op::I64DivU):
          case uint16_t(Op::I64RemS):
          case uint16_t(Op::I64RemU):
          case uint16_t(Op::I64And):
          case uint16_t(Op::I64Or):
          case uint16_t(Op::I64Xor):
          case uint16_t(Op::I64Shl):
          case uint16_t(Op::I64ShrS):
          case uint16_t(Op::I64ShrU):
          case uint16_t(Op::I64Rotl):
          case uint16_t(Op::I64Rotr):
            CHECK(f.iter().readBinary(ValType::I64, nullptr, nullptr));
          case uint16_t(Op::F32Add):
          case uint16_t(Op::F32Sub):
          case uint16_t(Op::F32Mul):
          case uint16_t(Op::F32Div):
          case uint16_t(Op::F32Min):
          case uint16_t(Op::F32Max):
          case uint16_t(Op::F32CopySign):
            CHECK(f.iter().readBinary(ValType::F32, nullptr, nullptr));
          case uint16_t(Op::F64Add):
          case uint16_t(Op::F64Sub):
          case uint16_t(Op::F64Mul):
          case uint16_t(Op::F64Div):
          case uint16_t(Op::F64Min):
          case uint16_t(Op::F64Max):
          case uint16_t(Op::F64CopySign):
            CHECK(f.iter().readBinary(ValType::F64, nullptr, nullptr));
          case uint16_t(Op::I32Eq):
          case uint16_t(Op::I32Ne):
          case uint16_t(Op::I32LtS):
          case uint16_t(Op::I32LtU):
          case uint16_t(Op::I32LeS):
          case uint16_t(Op::I32LeU):
          case uint16_t(Op::I32GtS):
          case uint16_t(Op::I32GtU):
          case uint16_t(Op::I32GeS):
          case uint16_t(Op::I32GeU):
            CHECK(f.iter().readComparison(ValType::I32, nullptr, nullptr));
          case uint16_t(Op::I64Eq):
          case uint16_t(Op::I64Ne):
          case uint16_t(Op::I64LtS):
          case uint16_t(Op::I64LtU):
          case uint16_t(Op::I64LeS):
          case uint16_t(Op::I64LeU):
          case uint16_t(Op::I64GtS):
          case uint16_t(Op::I64GtU):
          case uint16_t(Op::I64GeS):
          case uint16_t(Op::I64GeU):
            CHECK(f.iter().readComparison(ValType::I64, nullptr, nullptr));
          case uint16_t(Op::F32Eq):
          case uint16_t(Op::F32Ne):
          case uint16_t(Op::F32Lt):
          case uint16_t(Op::F32Le):
          case uint16_t(Op::F32Gt):
          case uint16_t(Op::F32Ge):
            CHECK(f.iter().readComparison(ValType::F32, nullptr, nullptr));
          case uint16_t(Op::F64Eq):
          case uint16_t(Op::F64Ne):
          case uint16_t(Op::F64Lt):
          case uint16_t(Op::F64Le):
          case uint16_t(Op::F64Gt):
          case uint16_t(Op::F64Ge):
            CHECK(f.iter().readComparison(ValType::F64, nullptr, nullptr));
          case uint16_t(Op::I32Eqz):
            CHECK(f.iter().readConversion(ValType::I32, ValType::I32, nullptr));
          case uint16_t(Op::I64Eqz):
          case uint16_t(Op::I32WrapI64):
            CHECK(f.iter().readConversion(ValType::I64, ValType::I32, nullptr));
          case uint16_t(Op::I32TruncSF32):
          case uint16_t(Op::I32TruncUF32):
          case uint16_t(Op::I32ReinterpretF32):
            CHECK(f.iter().readConversion(ValType::F32, ValType::I32, nullptr));
          case uint16_t(Op::I32TruncSF64):
          case uint16_t(Op::I32TruncUF64):
            CHECK(f.iter().readConversion(ValType::F64, ValType::I32, nullptr));
          case uint16_t(Op::I64ExtendSI32):
          case uint16_t(Op::I64ExtendUI32):
            CHECK(f.iter().readConversion(ValType::I32, ValType::I64, nullptr));
          case uint16_t(Op::I64TruncSF32):
          case uint16_t(Op::I64TruncUF32):
            CHECK(f.iter().readConversion(ValType::F32, ValType::I64, nullptr));
          case uint16_t(Op::I64TruncSF64):
          case uint16_t(Op::I64TruncUF64):
          case uint16_t(Op::I64ReinterpretF64):
            CHECK(f.iter().readConversion(ValType::F64, ValType::I64, nullptr));
          case uint16_t(Op::F32ConvertSI32):
          case uint16_t(Op::F32ConvertUI32):
          case uint16_t(Op::F32ReinterpretI32):
            CHECK(f.iter().readConversion(ValType::I32, ValType::F32, nullptr));
          case uint16_t(Op::F32ConvertSI64):
          case uint16_t(Op::F32ConvertUI64):
            CHECK(f.iter().readConversion(ValType::I64, ValType::F32, nullptr));
          case uint16_t(Op::F32DemoteF64):
            CHECK(f.iter().readConversion(ValType::F64, ValType::F32, nullptr));
          case uint16_t(Op::F64ConvertSI32):
          case uint16_t(Op::F64ConvertUI32):
            CHECK(f.iter().readConversion(ValType::I32, ValType::F64, nullptr));
          case uint16_t(Op::F64ConvertSI64):
          case uint16_t(Op::F64ConvertUI64):
          case uint16_t(Op::F64ReinterpretI64):
            CHECK(f.iter().readConversion(ValType::I64, ValType::F64, nullptr));
          case uint16_t(Op::F64PromoteF32):
            CHECK(f.iter().readConversion(ValType::F32, ValType::F64, nullptr));
          case uint16_t(Op::I32Load8S):
          case uint16_t(Op::I32Load8U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 1, nullptr));
          case uint16_t(Op::I32Load16S):
          case uint16_t(Op::I32Load16U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 2, nullptr));
          case uint16_t(Op::I32Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I32, 4, nullptr));
          case uint16_t(Op::I64Load8S):
          case uint16_t(Op::I64Load8U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 1, nullptr));
          case uint16_t(Op::I64Load16S):
          case uint16_t(Op::I64Load16U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 2, nullptr));
          case uint16_t(Op::I64Load32S):
          case uint16_t(Op::I64Load32U):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 4, nullptr));
          case uint16_t(Op::I64Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::I64, 8, nullptr));
          case uint16_t(Op::F32Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::F32, 4, nullptr));
          case uint16_t(Op::F64Load):
            CHECK(f.checkHasMemory() && f.iter().readLoad(ValType::F64, 8, nullptr));
          case uint16_t(Op::I32Store8):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 1, nullptr, nullptr));
          case uint16_t(Op::I32Store16):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 2, nullptr, nullptr));
          case uint16_t(Op::I32Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I32, 4, nullptr, nullptr));
          case uint16_t(Op::I64Store8):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 1, nullptr, nullptr));
          case uint16_t(Op::I64Store16):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 2, nullptr, nullptr));
          case uint16_t(Op::I64Store32):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 4, nullptr, nullptr));
          case uint16_t(Op::I64Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::I64, 8, nullptr, nullptr));
          case uint16_t(Op::F32Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::F32, 4, nullptr, nullptr));
          case uint16_t(Op::F64Store):
            CHECK(f.checkHasMemory() && f.iter().readStore(ValType::F64, 8, nullptr, nullptr));
          case uint16_t(Op::GrowMemory):
            CHECK(f.checkHasMemory() && f.iter().readGrowMemory(nullptr));
          case uint16_t(Op::CurrentMemory):
            CHECK(f.checkHasMemory() && f.iter().readCurrentMemory());
          case uint16_t(Op::Br):
            CHECK(f.iter().readBr(nullptr, nullptr, nullptr));
          case uint16_t(Op::BrIf):
            CHECK(f.iter().readBrIf(nullptr, nullptr, nullptr, nullptr));
          case uint16_t(Op::BrTable):
            CHECK(DecodeBrTable(f));
          case uint16_t(Op::Return):
            CHECK(f.iter().readReturn(nullptr));
          case uint16_t(Op::Unreachable):
            CHECK(f.iter().readUnreachable());
          default:
            return f.iter().unrecognizedOpcode(op);
        }
    }

    MOZ_CRASH("unreachable");

#undef CHECK
}

bool
wasm::ValidateFunctionBody(const ModuleEnvironment& env, uint32_t funcIndex, Decoder& d)
{
    ValTypeVector locals;
    const Sig& sig = *env.funcSigs[funcIndex];
    if (!locals.appendAll(sig.args()))
        return false;

    if (!DecodeLocalEntries(d, ModuleKind::Wasm, &locals))
        return false;

    FunctionDecoder f(env, locals, d);

    if (!f.iter().readFunctionStart(sig.ret()))
        return false;

    if (!DecodeFunctionBodyExprs(f))
        return false;

    return f.iter().readFunctionEnd();
}

// Section macros.

static bool
DecodePreamble(Decoder& d)
{
    if (d.bytesRemain() > MaxModuleBytes)
        return d.fail("module too big");

    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return d.fail("failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return d.fail("binary version 0x%" PRIx32 " does not match expected version 0x%" PRIx32,
                      u32, EncodingVersion);

    return true;
}

static bool
DecodeTypeSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Type, env, &sectionStart, &sectionSize, "type"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!d.readVarU32(&numSigs))
        return d.fail("expected number of signatures");

    if (numSigs > MaxTypes)
        return d.fail("too many signatures");

    if (!env->sigs.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t form;
        if (!d.readVarU32(&form) || form != uint32_t(TypeCode::Func))
            return d.fail("expected function form");

        uint32_t numArgs;
        if (!d.readVarU32(&numArgs))
            return d.fail("bad number of function args");

        if (numArgs > MaxParams)
            return d.fail("too many arguments in signature");

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!DecodeValType(d, ModuleKind::Wasm, &args[i]))
                return false;
        }

        uint32_t numRets;
        if (!d.readVarU32(&numRets))
            return d.fail("bad number of function returns");

        if (numRets > 1)
            return d.fail("too many returns in signature");

        ExprType result = ExprType::Void;

        if (numRets == 1) {
            ValType type;
            if (!DecodeValType(d, ModuleKind::Wasm, &type))
                return false;

            result = ToExprType(type);
        }

        env->sigs[sigIndex] = Sig(Move(args), result);
    }

    if (!d.finishSection(sectionStart, sectionSize, "type"))
        return false;

    return true;
}

static UniqueChars
DecodeName(Decoder& d)
{
    uint32_t numBytes;
    if (!d.readVarU32(&numBytes))
        return nullptr;

    if (numBytes > MaxStringBytes)
        return nullptr;

    const uint8_t* bytes;
    if (!d.readBytes(numBytes, &bytes))
        return nullptr;

    UniqueChars name(js_pod_malloc<char>(numBytes + 1));
    if (!name)
        return nullptr;

    memcpy(name.get(), bytes, numBytes);
    name[numBytes] = '\0';

    return name;
}

static bool
DecodeSignatureIndex(Decoder& d, const SigWithIdVector& sigs, uint32_t* sigIndex)
{
    if (!d.readVarU32(sigIndex))
        return d.fail("expected signature index");

    if (*sigIndex >= sigs.length())
        return d.fail("signature index out of range");

    return true;
}

static bool
DecodeLimits(Decoder& d, Limits* limits)
{
    uint32_t flags;
    if (!d.readVarU32(&flags))
        return d.fail("expected flags");

    if (flags & ~uint32_t(0x1))
        return d.fail("unexpected bits set in flags: %" PRIu32, (flags & ~uint32_t(0x1)));

    if (!d.readVarU32(&limits->initial))
        return d.fail("expected initial length");

    if (flags & 0x1) {
        uint32_t maximum;
        if (!d.readVarU32(&maximum))
            return d.fail("expected maximum length");

        if (limits->initial > maximum) {
            return d.fail("memory size minimum must not be greater than maximum; "
                          "maximum length %" PRIu32 " is less than initial length %" PRIu32,
                          maximum, limits->initial);
        }

        limits->maximum.emplace(maximum);
    }

    return true;
}

static bool
DecodeTableLimits(Decoder& d, TableDescVector* tables)
{
    uint32_t elementType;
    if (!d.readVarU32(&elementType))
        return d.fail("expected table element type");

    if (elementType != uint32_t(TypeCode::AnyFunc))
        return d.fail("expected 'anyfunc' element type");

    Limits limits;
    if (!DecodeLimits(d, &limits))
        return false;

    if (limits.initial > MaxTableLength)
        return d.fail("too many table elements");

    if (tables->length())
        return d.fail("already have default table");

    return tables->emplaceBack(TableKind::AnyFunction, limits);
}

static bool
GlobalIsJSCompatible(Decoder& d, ValType type, bool isMutable)
{
    switch (type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
        break;
      case ValType::I64:
        if (!jit::JitOptions.wasmTestMode)
            return d.fail("can't import/export an Int64 global to JS");
        break;
      default:
        return d.fail("unexpected variable type in global import/export");
    }

    if (isMutable)
        return d.fail("can't import/export mutable globals in the MVP");

    return true;
}

static bool
DecodeGlobalType(Decoder& d, ValType* type, bool* isMutable)
{
    if (!DecodeValType(d, ModuleKind::Wasm, type))
        return false;

    uint32_t flags;
    if (!d.readVarU32(&flags))
        return d.fail("expected global flags");

    if (flags & ~uint32_t(GlobalFlags::AllowedMask))
        return d.fail("unexpected bits set in global flags");

    *isMutable = flags & uint32_t(GlobalFlags::IsMutable);
    return true;
}

static bool
DecodeMemoryLimits(Decoder& d, ModuleEnvironment* env)
{
    if (env->usesMemory())
        return d.fail("already have default memory");

    Limits memory;
    if (!DecodeLimits(d, &memory))
        return false;

    CheckedInt<uint32_t> initialBytes = memory.initial;
    initialBytes *= PageSize;
    if (!initialBytes.isValid() || initialBytes.value() > uint32_t(INT32_MAX))
        return d.fail("initial memory size too big");

    memory.initial = initialBytes.value();

    if (memory.maximum) {
        CheckedInt<uint32_t> maximumBytes = *memory.maximum;
        maximumBytes *= PageSize;
        if (!maximumBytes.isValid())
            return d.fail("maximum memory size too big");

        memory.maximum = Some(maximumBytes.value());
    }

    env->memoryUsage = MemoryUsage::Unshared;
    env->minMemoryLength = memory.initial;
    env->maxMemoryLength = memory.maximum;
    return true;
}

static bool
DecodeImport(Decoder& d, ModuleEnvironment* env)
{
    UniqueChars moduleName = DecodeName(d);
    if (!moduleName)
        return d.fail("expected valid import module name");

    UniqueChars funcName = DecodeName(d);
    if (!funcName)
        return d.fail("expected valid import func name");

    uint32_t rawImportKind;
    if (!d.readVarU32(&rawImportKind))
        return d.fail("failed to read import kind");

    DefinitionKind importKind = DefinitionKind(rawImportKind);

    switch (importKind) {
      case DefinitionKind::Function: {
        uint32_t sigIndex;
        if (!DecodeSignatureIndex(d, env->sigs, &sigIndex))
            return false;
        if (!env->funcSigs.append(&env->sigs[sigIndex]))
            return false;
        if (env->funcSigs.length() > MaxFuncs)
            return d.fail("too many functions");
        break;
      }
      case DefinitionKind::Table: {
        if (!DecodeTableLimits(d, &env->tables))
            return false;
        env->tables.back().external = true;
        break;
      }
      case DefinitionKind::Memory: {
        if (!DecodeMemoryLimits(d, env))
            return false;
        break;
      }
      case DefinitionKind::Global: {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;
        if (!GlobalIsJSCompatible(d, type, isMutable))
            return false;
        if (!env->globals.append(GlobalDesc(type, isMutable, env->globals.length())))
            return false;
        if (env->globals.length() > MaxGlobals)
            return d.fail("too many globals");
        break;
      }
      default:
        return d.fail("unsupported import kind");
    }

    return env->imports.emplaceBack(Move(moduleName), Move(funcName), importKind);
}

static bool
DecodeImportSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Import, env, &sectionStart, &sectionSize, "import"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!d.readVarU32(&numImports))
        return d.fail("failed to read number of imports");

    if (numImports > MaxImports)
        return d.fail("too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        if (!DecodeImport(d, env))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "import"))
        return false;

    // The global data offsets will be filled in by ModuleGenerator::init.
    if (!env->funcImportGlobalDataOffsets.resize(env->funcSigs.length()))
        return false;

    return true;
}

static bool
DecodeFunctionSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Function, env, &sectionStart, &sectionSize, "function"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDefs;
    if (!d.readVarU32(&numDefs))
        return d.fail("expected number of function definitions");

    CheckedInt<uint32_t> numFuncs = env->funcSigs.length();
    numFuncs += numDefs;
    if (!numFuncs.isValid() || numFuncs.value() > MaxFuncs)
        return d.fail("too many functions");

    if (!env->funcSigs.reserve(numFuncs.value()))
        return false;

    for (uint32_t i = 0; i < numDefs; i++) {
        uint32_t sigIndex;
        if (!DecodeSignatureIndex(d, env->sigs, &sigIndex))
            return false;
        env->funcSigs.infallibleAppend(&env->sigs[sigIndex]);
    }

    if (!d.finishSection(sectionStart, sectionSize, "function"))
        return false;

    return true;
}

static bool
DecodeTableSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Table, env, &sectionStart, &sectionSize, "table"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numTables;
    if (!d.readVarU32(&numTables))
        return d.fail("failed to read number of tables");

    if (numTables != 1)
        return d.fail("the number of tables must be exactly one");

    if (!DecodeTableLimits(d, &env->tables))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "table"))
        return false;

    return true;
}

static bool
DecodeMemorySection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Memory, env, &sectionStart, &sectionSize, "memory"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numMemories;
    if (!d.readVarU32(&numMemories))
        return d.fail("failed to read number of memories");

    if (numMemories != 1)
        return d.fail("the number of memories must be exactly one");

    if (!DecodeMemoryLimits(d, env))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "memory"))
        return false;

    return true;
}

static bool
DecodeInitializerExpression(Decoder& d, const GlobalDescVector& globals, ValType expected,
                            InitExpr* init)
{
    uint16_t op;
    if (!d.readOp(&op))
        return d.fail("failed to read initializer type");

    switch (op) {
      case uint16_t(Op::I32Const): {
        int32_t i32;
        if (!d.readVarS32(&i32))
            return d.fail("failed to read initializer i32 expression");
        *init = InitExpr(Val(uint32_t(i32)));
        break;
      }
      case uint16_t(Op::I64Const): {
        int64_t i64;
        if (!d.readVarS64(&i64))
            return d.fail("failed to read initializer i64 expression");
        *init = InitExpr(Val(uint64_t(i64)));
        break;
      }
      case uint16_t(Op::F32Const): {
        float f32;
        if (!d.readFixedF32(&f32))
            return d.fail("failed to read initializer f32 expression");
        *init = InitExpr(Val(f32));
        break;
      }
      case uint16_t(Op::F64Const): {
        double f64;
        if (!d.readFixedF64(&f64))
            return d.fail("failed to read initializer f64 expression");
        *init = InitExpr(Val(f64));
        break;
      }
      case uint16_t(Op::GetGlobal): {
        uint32_t i;
        if (!d.readVarU32(&i))
            return d.fail("failed to read get_global index in initializer expression");
        if (i >= globals.length())
            return d.fail("global index out of range in initializer expression");
        if (!globals[i].isImport() || globals[i].isMutable())
            return d.fail("initializer expression must reference a global immutable import");
        *init = InitExpr(i, globals[i].type());
        break;
      }
      default: {
        return d.fail("unexpected initializer expression");
      }
    }

    if (expected != init->type())
        return d.fail("type mismatch: initializer type and expected type don't match");

    uint16_t end;
    if (!d.readOp(&end) || end != uint16_t(Op::End))
        return d.fail("failed to read end of initializer expression");

    return true;
}

static bool
DecodeGlobalSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Global, env, &sectionStart, &sectionSize, "global"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDefs;
    if (!d.readVarU32(&numDefs))
        return d.fail("expected number of globals");

    CheckedInt<uint32_t> numGlobals = env->globals.length();
    numGlobals += numDefs;
    if (!numGlobals.isValid() || numGlobals.value() > MaxGlobals)
        return d.fail("too many globals");

    if (!env->globals.reserve(numGlobals.value()))
        return false;

    for (uint32_t i = 0; i < numDefs; i++) {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;

        InitExpr initializer;
        if (!DecodeInitializerExpression(d, env->globals, type, &initializer))
            return false;

        env->globals.infallibleAppend(GlobalDesc(initializer, isMutable));
    }

    if (!d.finishSection(sectionStart, sectionSize, "global"))
        return false;

    return true;
}

typedef HashSet<const char*, CStringHasher, SystemAllocPolicy> CStringSet;

static UniqueChars
DecodeExportName(Decoder& d, CStringSet* dupSet)
{
    UniqueChars exportName = DecodeName(d);
    if (!exportName) {
        d.fail("expected valid export name");
        return nullptr;
    }

    CStringSet::AddPtr p = dupSet->lookupForAdd(exportName.get());
    if (p) {
        d.fail("duplicate export");
        return nullptr;
    }

    if (!dupSet->add(p, exportName.get()))
        return nullptr;

    return Move(exportName);
}

static bool
DecodeExport(Decoder& d, ModuleEnvironment* env, CStringSet* dupSet)
{
    UniqueChars fieldName = DecodeExportName(d, dupSet);
    if (!fieldName)
        return false;

    uint32_t exportKind;
    if (!d.readVarU32(&exportKind))
        return d.fail("failed to read export kind");

    switch (DefinitionKind(exportKind)) {
      case DefinitionKind::Function: {
        uint32_t funcIndex;
        if (!d.readVarU32(&funcIndex))
            return d.fail("expected function index");

        if (funcIndex >= env->numFuncs())
            return d.fail("exported function index out of bounds");

        return env->exports.emplaceBack(Move(fieldName), funcIndex, DefinitionKind::Function);
      }
      case DefinitionKind::Table: {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return d.fail("expected table index");

        if (tableIndex >= env->tables.length())
            return d.fail("exported table index out of bounds");

        MOZ_ASSERT(env->tables.length() == 1);
        env->tables[0].external = true;

        return env->exports.emplaceBack(Move(fieldName), DefinitionKind::Table);
      }
      case DefinitionKind::Memory: {
        uint32_t memoryIndex;
        if (!d.readVarU32(&memoryIndex))
            return d.fail("expected memory index");

        if (memoryIndex > 0 || !env->usesMemory())
            return d.fail("exported memory index out of bounds");

        return env->exports.emplaceBack(Move(fieldName), DefinitionKind::Memory);
      }
      case DefinitionKind::Global: {
        uint32_t globalIndex;
        if (!d.readVarU32(&globalIndex))
            return d.fail("expected global index");

        if (globalIndex >= env->globals.length())
            return d.fail("exported global index out of bounds");

        const GlobalDesc& global = env->globals[globalIndex];
        if (!GlobalIsJSCompatible(d, global.type(), global.isMutable()))
            return false;

        return env->exports.emplaceBack(Move(fieldName), globalIndex, DefinitionKind::Global);
      }
      default:
        return d.fail("unexpected export kind");
    }

    MOZ_CRASH("unreachable");
}

static bool
DecodeExportSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Export, env, &sectionStart, &sectionSize, "export"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    CStringSet dupSet;
    if (!dupSet.init())
        return false;

    uint32_t numExports;
    if (!d.readVarU32(&numExports))
        return d.fail("failed to read number of exports");

    if (numExports > MaxExports)
        return d.fail("too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!DecodeExport(d, env, &dupSet))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "export"))
        return false;

    return true;
}

static bool
DecodeStartSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Start, env, &sectionStart, &sectionSize, "start"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t funcIndex;
    if (!d.readVarU32(&funcIndex))
        return d.fail("failed to read start func index");

    if (funcIndex >= env->numFuncs())
        return d.fail("unknown start function");

    const Sig& sig = *env->funcSigs[funcIndex];
    if (!IsVoid(sig.ret()))
        return d.fail("start function must not return anything");

    if (sig.args().length())
        return d.fail("start function must be nullary");

    env->startFuncIndex = Some(funcIndex);

    if (!d.finishSection(sectionStart, sectionSize, "start"))
        return false;

    return true;
}

static bool
DecodeElemSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Elem, env, &sectionStart, &sectionSize, "elem"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return d.fail("failed to read number of elem segments");

    if (numSegments > MaxElemSegments)
        return d.fail("too many elem segments");

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return d.fail("expected table index");

        MOZ_ASSERT(env->tables.length() <= 1);
        if (tableIndex >= env->tables.length())
            return d.fail("table index out of range");

        InitExpr offset;
        if (!DecodeInitializerExpression(d, env->globals, ValType::I32, &offset))
            return false;

        uint32_t numElems;
        if (!d.readVarU32(&numElems))
            return d.fail("expected segment size");

        if (numElems > MaxTableLength)
            return d.fail("too many table elements");

        Uint32Vector elemFuncIndices;
        if (!elemFuncIndices.resize(numElems))
            return false;

        for (uint32_t i = 0; i < numElems; i++) {
            if (!d.readVarU32(&elemFuncIndices[i]))
                return d.fail("failed to read element function index");
            if (elemFuncIndices[i] >= env->numFuncs())
                return d.fail("table element out of range");
        }

        if (!env->elemSegments.emplaceBack(0, offset, Move(elemFuncIndices)))
            return false;

        env->tables[env->elemSegments.back().tableIndex].external = true;
    }

    if (!d.finishSection(sectionStart, sectionSize, "elem"))
        return false;

    return true;
}

bool
wasm::DecodeModuleEnvironment(Decoder& d, ModuleEnvironment* env)
{
    if (!DecodePreamble(d))
        return false;

    if (!DecodeTypeSection(d, env))
        return false;

    if (!DecodeImportSection(d, env))
        return false;

    if (!DecodeFunctionSection(d, env))
        return false;

    if (!DecodeTableSection(d, env))
        return false;

    if (!DecodeMemorySection(d, env))
        return false;

    if (!DecodeGlobalSection(d, env))
        return false;

    if (!DecodeExportSection(d, env))
        return false;

    if (!DecodeStartSection(d, env))
        return false;

    if (!DecodeElemSection(d, env))
        return false;

    return true;
}

static bool
DecodeFunctionBody(Decoder& d, const ModuleEnvironment& env, uint32_t funcIndex)
{
    uint32_t bodySize;
    if (!d.readVarU32(&bodySize))
        return d.fail("expected number of function body bytes");

    if (bodySize > MaxFunctionBytes)
        return d.fail("function body too big");

    if (d.bytesRemain() < bodySize)
        return d.fail("function body length too big");

    const uint8_t* bodyBegin = d.currentPosition();

    if (!ValidateFunctionBody(env, funcIndex, d))
        return false;

    if (d.currentPosition() != bodyBegin + bodySize)
        return d.fail("function body length mismatch");

    return true;
}

static bool
DecodeCodeSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Code, env, &sectionStart, &sectionSize, "code"))
        return false;

    if (sectionStart == Decoder::NotStarted) {
        if (env->numFuncDefs() != 0)
            return d.fail("expected function bodies");
        return true;
    }

    uint32_t numFuncDefs;
    if (!d.readVarU32(&numFuncDefs))
        return d.fail("expected function body count");

    if (numFuncDefs != env->numFuncDefs())
        return d.fail("function body count does not match function signature count");

    for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
        if (!DecodeFunctionBody(d, *env, env->numFuncImports() + funcDefIndex))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return true;
}

static bool
DecodeDataSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Data, env, &sectionStart, &sectionSize, "data"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (!env->usesMemory())
        return d.fail("data section requires a memory section");

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return d.fail("failed to read number of data segments");

    if (numSegments > MaxDataSegments)
        return d.fail("too many data segments");

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t linearMemoryIndex;
        if (!d.readVarU32(&linearMemoryIndex))
            return d.fail("expected linear memory index");

        if (linearMemoryIndex != 0)
            return d.fail("linear memory index must currently be 0");

        DataSegment seg;
        if (!DecodeInitializerExpression(d, env->globals, ValType::I32, &seg.offset))
            return false;

        if (!d.readVarU32(&seg.length))
            return d.fail("expected segment size");

        seg.bytecodeOffset = d.currentOffset();

        if (!d.readBytes(seg.length))
            return d.fail("data segment shorter than declared");

        if (!env->dataSegments.append(seg))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "data"))
        return false;

    return true;
}

static void
MaybeDecodeNameSectionBody(Decoder& d, ModuleEnvironment* env)
{
    // For simplicity, ignore all failures, even OOM. Failure will simply result
    // in the names section not being included for this module.

    uint32_t numFuncNames;
    if (!d.readVarU32(&numFuncNames))
        return;

    if (numFuncNames > MaxFuncs)
        return;

    // Use a local vector (and not env->funcNames) since it could result in a
    // partially initialized result in case of failure in the middle.
    NameInBytecodeVector funcNames;
    if (!funcNames.resize(numFuncNames))
        return;

    for (uint32_t i = 0; i < numFuncNames; i++) {
        uint32_t numBytes;
        if (!d.readVarU32(&numBytes))
            return;
        if (numBytes > MaxStringLength)
            return;

        NameInBytecode name;
        name.offset = d.currentOffset();
        name.length = numBytes;
        funcNames[i] = name;

        if (!d.readBytes(numBytes))
            return;

        // Skip local names for a function.
        uint32_t numLocals;
        if (!d.readVarU32(&numLocals))
            return;
        if (numLocals > MaxLocals)
            return;

        for (uint32_t j = 0; j < numLocals; j++) {
            uint32_t numBytes;
            if (!d.readVarU32(&numBytes))
                return;
            if (numBytes > MaxStringLength)
                return;

            if (!d.readBytes(numBytes))
                return;
        }
    }

    env->funcNames = Move(funcNames);
}

static bool
DecodeNameSection(Decoder& d, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startCustomSection(NameSectionName, env, &sectionStart, &sectionSize))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    // Once started, custom sections do not report validation errors.

    MaybeDecodeNameSectionBody(d, env);

    d.finishCustomSection(sectionStart, sectionSize);
    return true;
}

bool
wasm::DecodeModuleTail(Decoder& d, ModuleEnvironment* env)
{
    if (!DecodeDataSection(d, env))
        return false;

    if (!DecodeNameSection(d, env))
        return false;

    while (!d.done()) {
        if (!d.skipCustomSection(env)) {
            if (d.resilientMode()) {
                d.clearError();
                return true;
            }
            return false;
        }
    }

    return true;
}

// Validate algorithm.

bool
wasm::Validate(const ShareableBytes& bytecode, UniqueChars* error)
{
    Decoder d(bytecode.begin(), bytecode.end(), error);

    ModuleEnvironment env;
    if (!DecodeModuleEnvironment(d, &env))
        return false;

    if (!DecodeCodeSection(d, &env))
        return false;

    if (!DecodeModuleTail(d, &env))
        return false;

    MOZ_ASSERT(!*error, "unreported error in decoding");
    return true;
}
