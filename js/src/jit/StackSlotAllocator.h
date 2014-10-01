/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_StackSlotAllocator_h
#define jit_StackSlotAllocator_h

#include "jit/Registers.h"

namespace js {
namespace jit {

class StackSlotAllocator
{
    js::Vector<uint32_t, 4, SystemAllocPolicy> normalSlots;
    js::Vector<uint32_t, 4, SystemAllocPolicy> doubleSlots;
    js::Vector<uint32_t, 4, SystemAllocPolicy> quadSlots;
    uint32_t height_;

    void freeSlot(uint32_t index) {
        normalSlots.append(index);
    }
    void freeDoubleSlot(uint32_t index) {
        doubleSlots.append(index);
    }
    void freeQuadSlot(uint32_t index) {
        MOZ_ASSERT(SupportsSimd);
        quadSlots.append(index);
    }

    uint32_t allocateQuadSlot() {
        MOZ_ASSERT(SupportsSimd);
        // This relies on the fact that any architecture specific
        // alignment of the stack pointer is done a priori.
        if (!quadSlots.empty())
            return quadSlots.popCopy();
        if (height_ % 8 != 0)
            normalSlots.append(height_ += 4);
        if (height_ % 16 != 0)
            doubleSlots.append(height_ += 8);
        return height_ += 16;
    }
    uint32_t allocateDoubleSlot() {
        if (!doubleSlots.empty())
            return doubleSlots.popCopy();
        if (!quadSlots.empty()) {
            uint32_t index = quadSlots.popCopy();
            doubleSlots.append(index - 8);
            return index;
        }
        if (height_ % 8 != 0)
            normalSlots.append(height_ += 4);
        return height_ += 8;
    }
    uint32_t allocateSlot() {
        if (!normalSlots.empty())
            return normalSlots.popCopy();
        if (!doubleSlots.empty()) {
            uint32_t index = doubleSlots.popCopy();
            normalSlots.append(index - 4);
            return index;
        }
        if (!quadSlots.empty()) {
            uint32_t index = quadSlots.popCopy();
            normalSlots.append(index - 4);
            doubleSlots.append(index - 8);
            return index;
        }
        return height_ += 4;
    }

  public:
    StackSlotAllocator() : height_(0)
    { }

    void freeSlot(LDefinition::Type type, uint32_t index) {
        switch (type) {
#if JS_BITS_PER_WORD == 32
          case LDefinition::GENERAL:
          case LDefinition::OBJECT:
          case LDefinition::SLOTS:
#endif
          case LDefinition::INT32:
          case LDefinition::FLOAT32:   return freeSlot(index);
#if JS_BITS_PER_WORD == 64
          case LDefinition::GENERAL:
          case LDefinition::OBJECT:
          case LDefinition::SLOTS:
#endif
#ifdef JS_PUNBOX64
          case LDefinition::BOX:
#endif
#ifdef JS_NUNBOX32
          case LDefinition::TYPE:
          case LDefinition::PAYLOAD:
#endif
          case LDefinition::DOUBLE:    return freeDoubleSlot(index);
          case LDefinition::FLOAT32X4:
          case LDefinition::INT32X4:   return freeQuadSlot(index);
        }
        MOZ_CRASH("Unknown slot type");
    }

    uint32_t allocateSlot(LDefinition::Type type) {
        switch (type) {
#if JS_BITS_PER_WORD == 32
          case LDefinition::GENERAL:
          case LDefinition::OBJECT:
          case LDefinition::SLOTS:
#endif
          case LDefinition::INT32:
          case LDefinition::FLOAT32:   return allocateSlot();
#if JS_BITS_PER_WORD == 64
          case LDefinition::GENERAL:
          case LDefinition::OBJECT:
          case LDefinition::SLOTS:
#endif
#ifdef JS_PUNBOX64
          case LDefinition::BOX:
#endif
#ifdef JS_NUNBOX32
          case LDefinition::TYPE:
          case LDefinition::PAYLOAD:
#endif
          case LDefinition::DOUBLE:    return allocateDoubleSlot();
          case LDefinition::FLOAT32X4:
          case LDefinition::INT32X4:   return allocateQuadSlot();
        }
        MOZ_CRASH("Unknown slot type");
    }

    uint32_t stackHeight() const {
        return height_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_StackSlotAllocator_h */
