/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
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
    uint32 nslots_;
    uint32 slotsWritten_;
    uint32 nframes_;
    uint32 framesWritten_;
    SnapshotOffset lastStart_;

    void writeSlotHeader(JSValueType type, uint32 regCode);

  public:
    SnapshotOffset startSnapshot(uint32 frameCount, BailoutKind kind, bool resumeAfter);
    void startFrame(JSFunction *fun, JSScript *script, jsbytecode *pc, uint32 exprStack);
#ifdef TRACK_SNAPSHOTS
    void trackFrame(uint32 pcOpcode, uint32 mirOpcode, uint32 mirId,
                                     uint32 lirOpcode, uint32 lirId);
#endif
    void endFrame();

    void addSlot(const FloatRegister &reg);
    void addSlot(JSValueType type, const Register &reg);
    void addSlot(JSValueType type, int32 stackIndex);
    void addUndefinedSlot();
    void addNullSlot();
    void addInt32Slot(int32 value);
    void addConstantPoolSlot(uint32 index);
#if defined(JS_NUNBOX32)
    void addSlot(const Register &type, const Register &payload);
    void addSlot(const Register &type, int32 payloadStackIndex);
    void addSlot(int32 typeStackIndex, const Register &payload);
    void addSlot(int32 typeStackIndex, int32 payloadStackIndex);
#elif defined(JS_PUNBOX64)
    void addSlot(const Register &value);
    void addSlot(int32 valueStackSlot);
#endif
    void endSnapshot();

    bool oom() const {
        return writer_.oom() || writer_.length() >= MAX_BUFFER_SIZE;
    }

    size_t size() const {
        return writer_.length();
    }
    const uint8 *buffer() const {
        return writer_.buffer();
    }
};

}
}

#endif // jsion_snapshot_writer_h__

