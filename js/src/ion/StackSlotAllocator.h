/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_stack_slot_allocator_h_
#define jsion_stack_slot_allocator_h_

#include "Registers.h"

namespace js {
namespace ion {

class StackSlotAllocator
{
    js::Vector<uint32, 4, SystemAllocPolicy> normalSlots;
    js::Vector<uint32, 4, SystemAllocPolicy> doubleSlots;
    uint32 height_;

  public:
    StackSlotAllocator() : height_(0)
    { }

    void freeSlot(uint32 index) {
        normalSlots.append(index);
    }
    void freeDoubleSlot(uint32 index) {
        doubleSlots.append(index);
    }
    void freeValueSlot(uint32 index) {
        freeDoubleSlot(index);
    }

    uint32 allocateDoubleSlot() {
        if (!doubleSlots.empty())
            return doubleSlots.popCopy();
        if (ComputeByteAlignment(height_, DOUBLE_STACK_ALIGNMENT))
            normalSlots.append(++height_);
        height_ += (sizeof(double) / STACK_SLOT_SIZE);
        return height_;
    }
    uint32 allocateSlot() {
        if (!normalSlots.empty())
            return normalSlots.popCopy();
        if (!doubleSlots.empty()) {
            uint32 index = doubleSlots.popCopy();
            normalSlots.append(index - 1);
            return index;
        }
        return ++height_;
    }
    uint32 allocateValueSlot() {
        return allocateDoubleSlot();
    }
    uint32 stackHeight() const {
        return height_;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_stack_slot_allocator_h_

