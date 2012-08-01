/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_lir_x86_h__
#define jsion_lir_x86_h__

#include "ion/TypeOracle.h"

namespace js {
namespace ion {

class LBox : public LInstructionHelper<2, 1, 0>
{
    MIRType type_;

  public:
    LIR_HEADER(Box);

    LBox(const LAllocation &in_payload, MIRType type)
      : type_(type)
    {
        setOperand(0, in_payload);
    }

    MIRType type() const {
        return type_;
    }
};

class LBoxDouble : public LInstructionHelper<2, 1, 1>
{
  public:
    LIR_HEADER(BoxDouble);

    LBoxDouble(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
};

class LUnbox : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(Unbox);

    MUnbox *mir() const {
        return mir_->toUnbox();
    }
    const LAllocation *payload() {
        return getOperand(0);
    }
    const LAllocation *type() {
        return getOperand(1);
    }
};

class LUnboxDouble : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(UnboxDouble);

    static const size_t Input = 0;

    MUnbox *mir() const {
        return mir_->toUnbox();
    }
};

// Constant double.
class LDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Double);

    LDouble(const LConstantIndex &cindex) {
        setOperand(0, cindex);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_lir_x86_h__

