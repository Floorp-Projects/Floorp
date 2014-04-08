/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Recover_h
#define jit_Recover_h

#include "jit/Snapshots.h"

namespace js {
namespace jit {

enum RecoverOpcode
{
    Recover_ResumePoint = 0
};

class RResumePoint
{
  private:
    uint32_t pcOffset_;           // Offset from script->code.
    uint32_t numOperands_;        // Number of slots.

    RResumePoint(CompactBufferReader &reader);

  public:
    static void readRecoverData(CompactBufferReader &reader, RInstructionStorage *raw);

    uint32_t pcOffset() const {
        return pcOffset_;
    }
    uint32_t numOperands() const {
        return numOperands_;
    }
};

}
}

#endif /* jit_Recover_h */
