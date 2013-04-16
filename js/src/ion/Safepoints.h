/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_include_safepoints_h_
#define jsion_include_safepoints_h_

#include "Registers.h"
#include "CompactBuffer.h"
#include "BitSet.h"

#include "shared/Assembler-shared.h"

namespace js {
namespace ion {

struct SafepointNunboxEntry;
class LAllocation;
class LSafepoint;

static const uint32_t INVALID_SAFEPOINT_OFFSET = uint32_t(-1);

class SafepointWriter
{
    CompactBufferWriter stream_;
    BitSet *frameSlots_;

  public:
    bool init(uint32_t slotCount);

  private:
    // A safepoint entry is written in the order these functions appear.
    uint32_t startEntry();

    void writeOsiCallPointOffset(uint32_t osiPointOffset);
    void writeGcRegs(LSafepoint *safepoint);
    void writeGcSlots(LSafepoint *safepoint);
    void writeValueSlots(LSafepoint *safepoint);

#ifdef JS_NUNBOX32
    void writeNunboxParts(LSafepoint *safepoint);
#endif

    void endEntry();

  public:
    void encode(LSafepoint *safepoint);

    size_t size() const {
        return stream_.length();
    }
    const uint8_t *buffer() const {
        return stream_.buffer();
    }
};

class SafepointReader
{
    CompactBufferReader stream_;
    uint32_t frameSlots_;
    uint32_t currentSlotChunk_;
    uint32_t currentSlotChunkNumber_;
    uint32_t osiCallPointOffset_;
    GeneralRegisterSet gcSpills_;
    GeneralRegisterSet valueSpills_;
    GeneralRegisterSet allSpills_;
    uint32_t nunboxSlotsRemaining_;

  private:
    void advanceFromGcRegs();
    void advanceFromGcSlots();
    void advanceFromValueSlots();
    bool getSlotFromBitmap(uint32_t *slot);

  public:
    SafepointReader(IonScript *script, const SafepointIndex *si);

    static CodeLocationLabel InvalidationPatchPoint(IonScript *script, const SafepointIndex *si);

    uint32_t osiCallPointOffset() const {
        return osiCallPointOffset_;
    }
    GeneralRegisterSet gcSpills() const {
        return gcSpills_;
    }
    GeneralRegisterSet valueSpills() const {
        return valueSpills_;
    }
    GeneralRegisterSet allSpills() const {
        return allSpills_;
    }
    uint32_t osiReturnPointOffset() const;

    // Returns true if a slot was read, false if there are no more slots.
    bool getGcSlot(uint32_t *slot);

    // Returns true if a slot was read, false if there are no more value slots.
    bool getValueSlot(uint32_t *slot);

    // Returns true if a nunbox slot was read, false if there are no more
    // nunbox slots.
    bool getNunboxSlot(LAllocation *type, LAllocation *payload);
};

} // namespace ion
} // namespace js

#endif // jsion_include_safepoints_h_

