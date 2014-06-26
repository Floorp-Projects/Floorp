/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Recover_h
#define jit_Recover_h

#include "mozilla/Attributes.h"

#include "jit/Snapshots.h"

struct JSContext;

namespace js {
namespace jit {

#define RECOVER_OPCODE_LIST(_)                  \
    _(ResumePoint)                              \
    _(BitNot)                                   \
    _(BitAnd)                                   \
    _(BitOr)                                    \
    _(BitXor)                                   \
    _(Lsh)                                      \
    _(Rsh)                                      \
    _(Ursh)                                     \
    _(Add)                                      \
    _(Sub)                                      \
    _(Mul)                                      \
    _(Div)                                      \
    _(Mod)                                      \
    _(Concat)                                   \
    _(Floor)                                    \
    _(Round)                                    \
    _(CharCodeAt)                               \
    _(FromCharCode)                             \
    _(Pow)                                      \
    _(PowHalf)                                  \
    _(NewObject)                                \
    _(NewDerivedTypedObject)

class RResumePoint;
class SnapshotIterator;

class RInstruction
{
  public:
    enum Opcode
    {
#   define DEFINE_OPCODES_(op) Recover_##op,
        RECOVER_OPCODE_LIST(DEFINE_OPCODES_)
#   undef DEFINE_OPCODES_
        Recover_Invalid
    };

    virtual Opcode opcode() const = 0;

    // As opposed to the MIR, there is no need to add more methods as every
    // other instruction is well abstracted under the "recover" method.
    bool isResumePoint() const {
        return opcode() == Recover_ResumePoint;
    }
    inline const RResumePoint *toResumePoint() const;

    // Number of allocations which are encoded in the Snapshot for recovering
    // the current instruction.
    virtual uint32_t numOperands() const = 0;

    // Function used to recover the value computed by this instruction. This
    // function reads its arguments from the allocations listed on the snapshot
    // iterator and stores its returned value on the snapshot iterator too.
    virtual bool recover(JSContext *cx, SnapshotIterator &iter) const = 0;

    // Decode an RInstruction on top of the reserved storage space, based on the
    // tag written by the writeRecoverData function of the corresponding MIR
    // instruction.
    static void readRecoverData(CompactBufferReader &reader, RInstructionStorage *raw);
};

#define RINSTRUCTION_HEADER_(op)                                        \
  private:                                                              \
    friend class RInstruction;                                          \
    explicit R##op(CompactBufferReader &reader);                        \
                                                                        \
  public:                                                               \
    Opcode opcode() const {                                             \
        return RInstruction::Recover_##op;                              \
    }

class RResumePoint MOZ_FINAL : public RInstruction
{
  private:
    uint32_t pcOffset_;           // Offset from script->code.
    uint32_t numOperands_;        // Number of slots.

  public:
    RINSTRUCTION_HEADER_(ResumePoint)

    uint32_t pcOffset() const {
        return pcOffset_;
    }
    virtual uint32_t numOperands() const {
        return numOperands_;
    }
    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RBitNot MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(BitNot)

    virtual uint32_t numOperands() const {
        return 1;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RBitAnd MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(BitAnd)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RBitOr MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(BitOr)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RBitXor MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(BitXor)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RLsh MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Lsh)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RRsh MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Rsh)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RUrsh MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Ursh)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RAdd MOZ_FINAL : public RInstruction
{
  private:
    bool isFloatOperation_;

  public:
    RINSTRUCTION_HEADER_(Add)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RSub MOZ_FINAL : public RInstruction
{
  private:
    bool isFloatOperation_;

  public:
    RINSTRUCTION_HEADER_(Sub)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RMul MOZ_FINAL : public RInstruction
{
  private:
    bool isFloatOperation_;

  public:
    RINSTRUCTION_HEADER_(Mul)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RDiv MOZ_FINAL : public RInstruction
{
  private:
    bool isFloatOperation_;

  public:
    RINSTRUCTION_HEADER_(Div)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RMod MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Mod)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RConcat MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Concat)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RFloor MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Floor)

    virtual uint32_t numOperands() const {
        return 1;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RRound MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Round)

    virtual uint32_t numOperands() const {
        return 1;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RCharCodeAt MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(CharCodeAt)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RFromCharCode MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(FromCharCode)

    virtual uint32_t numOperands() const {
        return 1;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RPow MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(Pow)

    virtual uint32_t numOperands() const {
        return 2;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RPowHalf MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(PowHalf)

    virtual uint32_t numOperands() const {
        return 1;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RNewObject MOZ_FINAL : public RInstruction
{
  private:
    bool templateObjectIsClassPrototype_;

  public:
    RINSTRUCTION_HEADER_(NewObject)

    virtual uint32_t numOperands() const {
        return 1;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

class RNewDerivedTypedObject MOZ_FINAL : public RInstruction
{
  public:
    RINSTRUCTION_HEADER_(NewDerivedTypedObject)

    virtual uint32_t numOperands() const {
        return 3;
    }

    bool recover(JSContext *cx, SnapshotIterator &iter) const;
};

#undef RINSTRUCTION_HEADER_

const RResumePoint *
RInstruction::toResumePoint() const
{
    MOZ_ASSERT(isResumePoint());
    return static_cast<const RResumePoint *>(this);
}

}
}

#endif /* jit_Recover_h */
