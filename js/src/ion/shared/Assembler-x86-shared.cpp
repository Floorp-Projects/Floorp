/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/IonMacroAssembler.h"
#include "gc/Marking.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

void
AssemblerX86Shared::copyJumpRelocationTable(uint8_t *dest)
{
    if (jumpRelocations_.length())
        memcpy(dest, jumpRelocations_.buffer(), jumpRelocations_.length());
}

void
AssemblerX86Shared::copyDataRelocationTable(uint8_t *dest)
{
    if (dataRelocations_.length())
        memcpy(dest, dataRelocations_.buffer(), dataRelocations_.length());
}

void
AssemblerX86Shared::copyPreBarrierTable(uint8_t *dest)
{
    if (preBarriers_.length())
        memcpy(dest, preBarriers_.buffer(), preBarriers_.length());
}

static void
TraceDataRelocations(JSTracer *trc, uint8_t *buffer, CompactBufferReader &reader)
{
    while (reader.more()) {
        size_t offset = reader.readUnsigned();
        void **ptr = JSC::X86Assembler::getPointerRef(buffer + offset);

#ifdef JS_PUNBOX64
        // All pointers on x64 will have the top bits cleared. If those bits
        // are not cleared, this must be a Value.
        uintptr_t *word = reinterpret_cast<uintptr_t *>(ptr);
        if (*word >> JSVAL_TAG_SHIFT) {
            jsval_layout layout;
            layout.asBits = *word;
            Value v = IMPL_TO_JSVAL(layout);
            gc::MarkValueUnbarriered(trc, &v, "ion-masm-value");
            JS_ASSERT(*word == JSVAL_TO_IMPL(v).asBits);
            continue;
        }
#endif

        // No barrier needed since these are constants.
        gc::MarkGCThingUnbarriered(trc, reinterpret_cast<void **>(ptr), "ion-masm-ptr");
    }
}


void
AssemblerX86Shared::TraceDataRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    ::TraceDataRelocations(trc, code->raw(), reader);
}

void
AssemblerX86Shared::trace(JSTracer *trc)
{
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        if (rp.kind == Relocation::IONCODE) {
            IonCode *code = IonCode::FromExecutable((uint8_t *)rp.target);
            MarkIonCodeUnbarriered(trc, &code, "masmrel32");
            JS_ASSERT(code == IonCode::FromExecutable((uint8_t *)rp.target));
        }
    }
    if (dataRelocations_.length()) {
        CompactBufferReader reader(dataRelocations_);
        ::TraceDataRelocations(trc, masm.buffer(), reader);
    }
}

void
AssemblerX86Shared::executableCopy(void *buffer)
{
    masm.executableCopy(buffer);
}

void
AssemblerX86Shared::processCodeLabels(uint8_t *rawCode)
{
    for (size_t i = 0; i < codeLabels_.length(); i++) {
        CodeLabel label = codeLabels_[i];
        Bind(rawCode, label.dest(), rawCode + label.src()->offset());
    }
}

AssemblerX86Shared::Condition
AssemblerX86Shared::InvertCondition(Condition cond)
{
    switch (cond) {
      case Zero:
        return NonZero;
      case NonZero:
        return Zero;
      case LessThan:
        return GreaterThanOrEqual;
      case LessThanOrEqual:
        return GreaterThan;
      case GreaterThan:
        return LessThanOrEqual;
      case GreaterThanOrEqual:
        return LessThan;
      case Above:
        return BelowOrEqual;
      case AboveOrEqual:
        return Below;
      case Below:
        return AboveOrEqual;
      case BelowOrEqual:
        return Above;
      default:
        JS_NOT_REACHED("unexpected condition");
        return Equal;
    }
}

void
AutoFlushCache::update(uintptr_t newStart, size_t len)
{
}

void
AutoFlushCache::flushAnyway()
{
}

AutoFlushCache::~AutoFlushCache()
{
    if (!runtime_)
        return;

    if (runtime_->flusher() == this)
        runtime_->setFlusher(NULL);
}
