/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_snapshot_writer_h__
#define jsion_snapshot_writer_h__

#include "Ion.h"
#include "IonCode.h"
#include "Registers.h"
#include "CompactBuffer.h"
#include "Bailouts.h"

namespace js {
namespace ion {

// Collects snapshots in a contiguous buffer, which is copied into IonScript
// memory after code generation.
class SnapshotWriter
{
    CompactBufferWriter writer_;

    // These are only used to assert sanity.
    uint32_t nslots_;
    uint32_t slotsWritten_;
    uint32_t nframes_;
    uint32_t framesWritten_;
    SnapshotOffset lastStart_;

    void writeSlotHeader(JSValueType type, uint32_t regCode);

  public:
    SnapshotOffset startSnapshot(uint32_t frameCount, BailoutKind kind, bool resumeAfter);
    void startFrame(JSFunction *fun, JSScript *script, jsbytecode *pc, uint32_t exprStack);
#ifdef TRACK_SNAPSHOTS
    void trackFrame(uint32_t pcOpcode, uint32_t mirOpcode, uint32_t mirId,
                                     uint32_t lirOpcode, uint32_t lirId);
#endif
    void endFrame();

    void addSlot(const FloatRegister &reg);
    void addSlot(JSValueType type, const Register &reg);
    void addSlot(JSValueType type, int32_t stackIndex);
    void addUndefinedSlot();
    void addNullSlot();
    void addInt32Slot(int32_t value);
    void addConstantPoolSlot(uint32_t index);
#if defined(JS_NUNBOX32)
    void addSlot(const Register &type, const Register &payload);
    void addSlot(const Register &type, int32_t payloadStackIndex);
    void addSlot(int32_t typeStackIndex, const Register &payload);
    void addSlot(int32_t typeStackIndex, int32_t payloadStackIndex);
#elif defined(JS_PUNBOX64)
    void addSlot(const Register &value);
    void addSlot(int32_t valueStackSlot);
#endif
    void endSnapshot();

    bool oom() const {
        return writer_.oom() || writer_.length() >= MAX_BUFFER_SIZE;
    }

    size_t size() const {
        return writer_.length();
    }
    const uint8_t *buffer() const {
        return writer_.buffer();
    }
};

}
}

#endif // jsion_snapshot_writer_h__

