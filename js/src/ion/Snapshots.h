/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsion_snapshots_h__
#define jsion_snapshots_h__

#include "Ion.h"
#include "IonCode.h"
#include "IonRegisters.h"
#include "CompactBuffer.h"
#include "Bailouts.h"

namespace js {
namespace ion {

#ifdef TRACK_SNAPSHOTS
class LInstruction;
#endif

static const uint32 SNAPSHOT_MAX_NARGS = 127;
static const uint32 SNAPSHOT_MAX_STACK = 127;

// A snapshot reader reads the entries out of the compressed snapshot buffer in
// a script. These entries describe the stack state of an Ion frame at a given
// position in JIT code.
class SnapshotReader
{
    CompactBufferReader reader_;

    uint32 pcOffset_;           // Offset from script->code.
    uint32 slotCount_;          // Number of slots.
    uint32 frameCount_;
    BailoutKind bailoutKind_;

#ifdef DEBUG
    // In debug mode we include the JSScript in order to make a few assertions.
    JSScript *script_;
    uint32 slotsRead_;
#endif

#ifdef TRACK_SNAPSHOTS
    uint32 pcOpcode_;
    uint32 mirOpcode_;
    uint32 mirId_;
    uint32 lirOpcode_;
    uint32 lirId_;
#endif

    void readSnapshotHeader();
    void readSnapshotBody();

    template <typename T> inline T readVariableLength();

  public:
    enum SlotMode
    {
        CONSTANT,           // An index into the constant pool.
        DOUBLE_REG,         // Type is double, payload is in a register.
        TYPED_REG,          // Type is constant, payload is in a register.
        TYPED_STACK,        // Type is constant, payload is on the stack.
        UNTYPED,            // Type is not known.
        JS_UNDEFINED,       // UndefinedValue()
        JS_NULL,            // NullValue()
        JS_INT32            // Int32Value(n)
    };

    class Location
    {
        friend class SnapshotReader;

        Register::Code reg_;
        int32 stackSlot_;

        static Location From(const Register &reg) {
            Location loc;
            loc.reg_ = reg.code();
            loc.stackSlot_ = INVALID_STACK_SLOT;
            return loc;
        }
        static Location From(int32 stackSlot) {
            Location loc;
            loc.reg_ = Register::Code(0);      // Quell compiler warnings.
            loc.stackSlot_ = stackSlot;
            return loc;
        }

      public:
        Register reg() const {
            JS_ASSERT(!isStackSlot());
            return Register::FromCode(reg_);
        }
        int32 stackSlot() const {
            JS_ASSERT(isStackSlot());
            return stackSlot_;
        }
        bool isStackSlot() const {
            return stackSlot_ != INVALID_STACK_SLOT;
        }
    };

    class Slot
    {
        friend class SnapshotReader;

        SlotMode mode_;

        union {
            FloatRegister::Code fpu_;
            struct {
                JSValueType type;
                Location payload;
            } known_type_;
#if defined(JS_NUNBOX32)
            struct {
                Location type;
                Location payload;
            } unknown_type_;
#elif defined(JS_PUNBOX64)
            struct {
                Location value;
            } unknown_type_;
#endif
            int32 value_;
        };

        Slot(SlotMode mode, JSValueType type, const Location &loc)
          : mode_(mode)
        {
            known_type_.type = type;
            known_type_.payload = loc;
        }
        Slot(const FloatRegister &reg)
          : mode_(DOUBLE_REG)
        {
            fpu_ = reg.code();
        }
        Slot(SlotMode mode)
          : mode_(mode)
        { }
        Slot(SlotMode mode, uint32 index)
          : mode_(mode)
        {
            JS_ASSERT(mode == CONSTANT || mode == JS_INT32);
            value_ = index;
        }

      public:
        SlotMode mode() const {
            return mode_;
        }
        uint32 constantIndex() const {
            JS_ASSERT(mode() == CONSTANT);
            return value_;
        }
        int32 int32Value() const {
            JS_ASSERT(mode() == JS_INT32);
            return value_;
        }
        JSValueType knownType() const {
            JS_ASSERT(mode() == TYPED_REG || mode() == TYPED_STACK);
            return known_type_.type;
        }
        Register reg() const {
            JS_ASSERT(mode() == TYPED_REG && knownType() != JSVAL_TYPE_DOUBLE);
            return known_type_.payload.reg();
        }
        FloatRegister floatReg() const {
            JS_ASSERT(mode() == DOUBLE_REG);
            return FloatRegister::FromCode(fpu_);
        }
        int32 stackSlot() const {
            JS_ASSERT(mode() == TYPED_STACK);
            return known_type_.payload.stackSlot();
        }
#if defined(JS_NUNBOX32)
        Location payload() const {
            JS_ASSERT(mode() == UNTYPED);
            return unknown_type_.payload;
        }
        Location type() const {
            JS_ASSERT(mode() == UNTYPED);
            return unknown_type_.type;
        }
#elif defined(JS_PUNBOX64)
        Location value() const {
            JS_ASSERT(mode() == UNTYPED);
            return unknown_type_.value;
        }
#endif

        bool liveInReg() const;
    };

  public:
    SnapshotReader(const uint8 *buffer, const uint8 *end);

    uint32 pcOffset() const {
        return pcOffset_;
    }
    uint32 slots() const {
        return slotCount_;
    }
    BailoutKind bailoutKind() const {
        return bailoutKind_;
    }

    Slot readSlot();
    void finishReadingFrame();
    uint32 remainingFrameCount() const { return frameCount_; }
};

class FrameRecovery;

class SnapshotIterator
{
  private:
    const FrameRecovery &in_;
    Maybe<SnapshotReader> reader_;
    uint32 unreadSlots_;

    uintptr_t fromLocation(const SnapshotReader::Location &loc);
    void skipLocation(const SnapshotReader::Location &loc);
    static Value FromTypedPayload(JSValueType type, uintptr_t payload);

  public:
    SnapshotIterator(const FrameRecovery &in);

    typedef SnapshotReader::Slot Slot;

    // Iterate on values inside an inlined frame.
    Slot readSlot();
    Value slotValue(const Slot &);
    void skip(const Slot &);

    Value read() {
        return slotValue(readSlot());
    }

    bool more() {
        if (!unreadSlots_)
            reader_.ref().finishReadingFrame();
        return unreadSlots_;
    }

    // Iterate on inline frames.  A snapshot has at least one frame.
    void readFrame() {
        unreadSlots_ = slots();
    }

    bool moreFrames() {
        return reader_.ref().remainingFrameCount() > 1;
    }

    uint32 slots() const {
        return reader_.ref().slots();
    }

    uint32 pcOffset() const {
        return reader_.ref().pcOffset();
    }

    BailoutKind bailoutKind() const {
        return reader_.ref().bailoutKind();
    }
};

static const SnapshotOffset INVALID_SNAPSHOT_OFFSET = uint32(-1);

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
    SnapshotOffset startSnapshot(uint32 frameCount, BailoutKind kind);
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

#endif // jsion_snapshots_h__

