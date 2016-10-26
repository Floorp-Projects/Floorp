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

#include "asmjs/WasmBinaryFormat.h"

#include "mozilla/CheckedInt.h"

#include "jsprf.h"

using namespace js;
using namespace js::wasm;

using mozilla::CheckedInt;

bool
wasm::DecodePreamble(Decoder& d)
{
    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return d.fail("failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return d.fail("binary version 0x%" PRIx32 " does not match expected version 0x%" PRIx32,
                      u32, EncodingVersion);

    return true;
}

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

bool
wasm::DecodeLocalEntries(Decoder& d, ValTypeVector* locals)
{
    uint32_t numLocalEntries;
    if (!d.readVarU32(&numLocalEntries))
        return false;

    for (uint32_t i = 0; i < numLocalEntries; i++) {
        uint32_t count;
        if (!d.readVarU32(&count))
            return false;

        if (MaxLocals - locals->length() < count)
            return false;

        ValType type;
        if (!d.readValType(&type))
            return false;

        if (!locals->appendN(type, count))
            return false;
    }

    return true;
}

bool
wasm::DecodeGlobalType(Decoder& d, ValType* type, bool* isMutable)
{
    if (!d.readValType(type))
        return d.fail("bad global type");

    uint32_t flags;
    if (!d.readVarU32(&flags))
        return d.fail("expected global flags");

    if (flags & ~uint32_t(GlobalFlags::AllowedMask))
        return d.fail("unexpected bits set in global flags");

    *isMutable = flags & uint32_t(GlobalFlags::IsMutable);
    return true;
}

bool
wasm::DecodeInitializerExpression(Decoder& d, const GlobalDescVector& globals, ValType expected,
                                  InitExpr* init)
{
    Expr expr;
    if (!d.readExpr(&expr))
        return d.fail("failed to read initializer type");

    switch (expr) {
      case Expr::I32Const: {
        int32_t i32;
        if (!d.readVarS32(&i32))
            return d.fail("failed to read initializer i32 expression");
        *init = InitExpr(Val(uint32_t(i32)));
        break;
      }
      case Expr::I64Const: {
        int64_t i64;
        if (!d.readVarS64(&i64))
            return d.fail("failed to read initializer i64 expression");
        *init = InitExpr(Val(uint64_t(i64)));
        break;
      }
      case Expr::F32Const: {
        RawF32 f32;
        if (!d.readFixedF32(&f32))
            return d.fail("failed to read initializer f32 expression");
        *init = InitExpr(Val(f32));
        break;
      }
      case Expr::F64Const: {
        RawF64 f64;
        if (!d.readFixedF64(&f64))
            return d.fail("failed to read initializer f64 expression");
        *init = InitExpr(Val(f64));
        break;
      }
      case Expr::GetGlobal: {
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

    Expr end;
    if (!d.readExpr(&end) || end != Expr::End)
        return d.fail("failed to read end of initializer expression");

    return true;
}

bool
wasm::DecodeLimits(Decoder& d, Limits* limits)
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

bool
wasm::DecodeDataSection(Decoder& d, bool usesMemory, uint32_t minMemoryByteLength,
                        const GlobalDescVector& globals, DataSegmentVector* segments)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Data, &sectionStart, &sectionSize, "data"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (!usesMemory)
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
        if (!DecodeInitializerExpression(d, globals, ValType::I32, &seg.offset))
            return false;

        if (!d.readVarU32(&seg.length))
            return d.fail("expected segment size");

        if (seg.offset.isVal()) {
            uint32_t off = seg.offset.val().i32();
            if (off > minMemoryByteLength || minMemoryByteLength - off < seg.length)
                return d.fail("data segment does not fit");
        }

        seg.bytecodeOffset = d.currentOffset();

        if (!d.readBytes(seg.length))
            return d.fail("data segment shorter than declared");

        if (!segments->append(seg))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "data"))
        return false;

    return true;
}

bool
wasm::DecodeMemoryLimits(Decoder& d, bool hasMemory, Limits* memory)
{
    if (hasMemory)
        return d.fail("already have default memory");

    if (!DecodeLimits(d, memory))
        return false;

    CheckedInt<uint32_t> initialBytes = memory->initial;
    initialBytes *= PageSize;
    if (!initialBytes.isValid() || initialBytes.value() > uint32_t(INT32_MAX))
        return d.fail("initial memory size too big");

    memory->initial = initialBytes.value();

    if (memory->maximum) {
        CheckedInt<uint32_t> maximumBytes = *memory->maximum;
        maximumBytes *= PageSize;
        if (!maximumBytes.isValid())
            return d.fail("maximum memory size too big");

        memory->maximum = Some(maximumBytes.value());
    }

    return true;
}

bool
wasm::DecodeMemorySection(Decoder& d, bool hasMemory, Limits* memory, bool *present)
{
    *present = false;

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Memory, &sectionStart, &sectionSize, "memory"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    *present = true;

    uint32_t numMemories;
    if (!d.readVarU32(&numMemories))
        return d.fail("failed to read number of memories");

    if (numMemories != 1)
        return d.fail("the number of memories must be exactly one");

    if (!DecodeMemoryLimits(d, hasMemory, memory))
        return false;

    if (!d.finishSection(sectionStart, sectionSize, "memory"))
        return false;

    return true;
}

bool
wasm::DecodeUnknownSections(Decoder& d)
{
    while (!d.done()) {
        if (!d.skipUserDefinedSection())
            return false;
    }

    return true;
}
