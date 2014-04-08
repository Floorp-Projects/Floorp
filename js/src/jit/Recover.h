/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Recover_h
#define jit_Recover_h

#include "mozilla/Attributes.h"

#include "jit/Snapshots.h"

namespace js {
namespace jit {

class RResumePoint;

class RInstruction
{
  public:
    enum Opcode
    {
        Recover_ResumePoint = 0
    };

    virtual Opcode opcode() const = 0;

    bool isResumePoint() const {
        return opcode() == Recover_ResumePoint;
    }
    inline const RResumePoint *toResumePoint() const;

    virtual uint32_t numOperands() const = 0;

    static void readRecoverData(CompactBufferReader &reader, RInstructionStorage *raw);
};

class RResumePoint MOZ_FINAL : public RInstruction
{
  private:
    uint32_t pcOffset_;           // Offset from script->code.
    uint32_t numOperands_;        // Number of slots.

    friend class RInstruction;
    RResumePoint(CompactBufferReader &reader);

  public:
    virtual Opcode opcode() const {
        return Recover_ResumePoint;
    }

    uint32_t pcOffset() const {
        return pcOffset_;
    }
    virtual uint32_t numOperands() const {
        return numOperands_;
    }
};

const RResumePoint *
RInstruction::toResumePoint() const
{
    MOZ_ASSERT(isResumePoint());
    return static_cast<const RResumePoint *>(this);
}

}
}

#endif /* jit_Recover_h */
