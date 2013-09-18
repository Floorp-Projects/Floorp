/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_Assembler_shared_h
#define jit_shared_Assembler_shared_h

#include "mozilla/PodOperations.h"

#include <limits.h>

#include "jsworkers.h"

#include "jit/IonAllocPolicy.h"
#include "jit/Registers.h"
#include "jit/RegisterSets.h"

#if defined(JS_CPU_X64) || defined(JS_CPU_ARM)
// JS_SMALL_BRANCH means the range on a branch instruction
// is smaller than the whole address space
#    define JS_SMALL_BRANCH
#endif
namespace js {
namespace jit {

enum Scale {
    TimesOne = 0,
    TimesTwo = 1,
    TimesFour = 2,
    TimesEight = 3
};

static inline unsigned
ScaleToShift(Scale scale)
{
    return unsigned(scale);
}

static inline bool
IsShiftInScaleRange(int i)
{
    return i >= TimesOne && i <= TimesEight;
}

static inline Scale
ShiftToScale(int i)
{
    JS_ASSERT(IsShiftInScaleRange(i));
    return Scale(i);
}

static inline Scale
ScaleFromElemWidth(int shift)
{
    switch (shift) {
      case 1:
        return TimesOne;
      case 2:
        return TimesTwo;
      case 4:
        return TimesFour;
      case 8:
        return TimesEight;
    }

    MOZ_ASSUME_UNREACHABLE("Invalid scale");
}

// Used for 32-bit immediates which do not require relocation.
struct Imm32
{
    int32_t value;

    explicit Imm32(int32_t value) : value(value)
    { }

    static inline Imm32 ShiftOf(enum Scale s) {
        switch (s) {
          case TimesOne:
            return Imm32(0);
          case TimesTwo:
            return Imm32(1);
          case TimesFour:
            return Imm32(2);
          case TimesEight:
            return Imm32(3);
        };
        MOZ_ASSUME_UNREACHABLE("Invalid scale");
    }

    static inline Imm32 FactorOf(enum Scale s) {
        return Imm32(1 << ShiftOf(s).value);
    }
};

// Pointer-sized integer to be embedded as an immediate in an instruction.
struct ImmWord
{
    uintptr_t value;

    explicit ImmWord(uintptr_t value) : value(value)
    { }
};

// Pointer to be embedded as an immediate in an instruction.
struct ImmPtr
{
    void *value;

    explicit ImmPtr(const void *value) : value(const_cast<void*>(value))
    { }

    template <class R>
    explicit ImmPtr(R (*pf)())
      : value(JS_FUNC_TO_DATA_PTR(void *, pf))
    { }

    template <class R, class A1>
    explicit ImmPtr(R (*pf)(A1))
      : value(JS_FUNC_TO_DATA_PTR(void *, pf))
    { }

    template <class R, class A1, class A2>
    explicit ImmPtr(R (*pf)(A1, A2))
      : value(JS_FUNC_TO_DATA_PTR(void *, pf))
    { }

    template <class R, class A1, class A2, class A3>
    explicit ImmPtr(R (*pf)(A1, A2, A3))
      : value(JS_FUNC_TO_DATA_PTR(void *, pf))
    { }

    template <class R, class A1, class A2, class A3, class A4>
    explicit ImmPtr(R (*pf)(A1, A2, A3, A4))
      : value(JS_FUNC_TO_DATA_PTR(void *, pf))
    { }

};

// Used for immediates which require relocation.
struct ImmGCPtr
{
    uintptr_t value;

    explicit ImmGCPtr(const gc::Cell *ptr) : value(reinterpret_cast<uintptr_t>(ptr))
    {
        JS_ASSERT(!IsPoisonedPtr(ptr));
        JS_ASSERT_IF(ptr, ptr->isTenured());
    }

  protected:
    ImmGCPtr() : value(0) {}
};

// Used for immediates which require relocation and may be traced during minor GC.
struct ImmMaybeNurseryPtr : public ImmGCPtr
{
    explicit ImmMaybeNurseryPtr(gc::Cell *ptr)
    {
        this->value = reinterpret_cast<uintptr_t>(ptr);
        JS_ASSERT(!IsPoisonedPtr(ptr));
    }
};

// Pointer to be embedded as an immediate that is loaded/stored from by an
// instruction.
struct AbsoluteAddress {
    void *addr;

    explicit AbsoluteAddress(const void *addr)
      : addr(const_cast<void*>(addr))
    { }

    AbsoluteAddress offset(ptrdiff_t delta) {
        return AbsoluteAddress(((uint8_t *) addr) + delta);
    }
};

// The same as AbsoluteAddress except that the intention is to patch this
// instruction. The initial value of the immediate is 'addr' and this value is
// either clobbered or used in the patching process.
struct PatchedAbsoluteAddress {
    void *addr;

    explicit PatchedAbsoluteAddress()
      : addr(NULL)
    { }
    explicit PatchedAbsoluteAddress(const void *addr)
      : addr(const_cast<void*>(addr))
    { }
};

// Specifies an address computed in the form of a register base and a constant,
// 32-bit offset.
struct Address
{
    Register base;
    int32_t offset;

    Address(Register base, int32_t offset) : base(base), offset(offset)
    { }

    Address() { mozilla::PodZero(this); }
};

// Specifies an address computed in the form of a register base, a register
// index with a scale, and a constant, 32-bit offset.
struct BaseIndex
{
    Register base;
    Register index;
    Scale scale;
    int32_t offset;

    BaseIndex(Register base, Register index, Scale scale, int32_t offset = 0)
      : base(base), index(index), scale(scale), offset(offset)
    { }

    BaseIndex() { mozilla::PodZero(this); }
};

class Relocation {
  public:
    enum Kind {
        // The target is immovable, so patching is only needed if the source
        // buffer is relocated and the reference is relative.
        HARDCODED,

        // The target is the start of an IonCode buffer, which must be traced
        // during garbage collection. Relocations and patching may be needed.
        IONCODE
    };
};

struct LabelBase
{
  protected:
    // offset_ >= 0 means that the label is either bound or has incoming
    // uses and needs to be bound.
    int32_t offset_ : 31;
    bool bound_   : 1;

    // Disallow assignment.
    void operator =(const LabelBase &label);
  public:
    static const int32_t INVALID_OFFSET = -1;

    LabelBase() : offset_(INVALID_OFFSET), bound_(false)
    { }
    LabelBase(const LabelBase &label)
      : offset_(label.offset_),
        bound_(label.bound_)
    { }

    // If the label is bound, all incoming edges have been patched and any
    // future incoming edges will be immediately patched.
    bool bound() const {
        return bound_;
    }
    int32_t offset() const {
        JS_ASSERT(bound() || used());
        return offset_;
    }
    // Returns whether the label is not bound, but has incoming uses.
    bool used() const {
        return !bound() && offset_ > INVALID_OFFSET;
    }
    // Binds the label, fixing its final position in the code stream.
    void bind(int32_t offset) {
        JS_ASSERT(!bound());
        offset_ = offset;
        bound_ = true;
        JS_ASSERT(offset_ == offset);
    }
    // Marks the label as neither bound nor used.
    void reset() {
        offset_ = INVALID_OFFSET;
        bound_ = false;
    }
    // Sets the label's latest used position, returning the old use position in
    // the process.
    int32_t use(int32_t offset) {
        JS_ASSERT(!bound());

        int32_t old = offset_;
        offset_ = offset;
        JS_ASSERT(offset_ == offset);

        return old;
    }
};

// A label represents a position in an assembly buffer that may or may not have
// already been generated. Labels can either be "bound" or "unbound", the
// former meaning that its position is known and the latter that its position
// is not yet known.
//
// A jump to an unbound label adds that jump to the label's incoming queue. A
// jump to a bound label automatically computes the jump distance. The process
// of binding a label automatically corrects all incoming jumps.
class Label : public LabelBase
{
  public:
    Label()
    { }
    Label(const Label &label) : LabelBase(label)
    { }
    ~Label()
    {
#ifdef DEBUG
        // Note: the condition is a hack to silence this assert when OOM testing,
        // see bug 756614.
        if (MaybeGetIonContext() && !OffThreadIonCompilationEnabled(GetIonContext()->runtime))
            JS_ASSERT_IF(!GetIonContext()->runtime->hadOutOfMemory, !used());
#endif
    }
};

// Label's destructor asserts that if it has been used it has also been bound.
// In the case long-lived labels, however, failed compilation (e.g. OOM) will
// trigger this failure innocuously. This Label silences the assertion.
class NonAssertingLabel : public Label
{
  public:
    ~NonAssertingLabel()
    {
#ifdef DEBUG
        if (used())
            bind(0);
#endif
    }
};

class RepatchLabel
{
    static const int32_t INVALID_OFFSET = 0xC0000000;
    int32_t offset_ : 31;
    uint32_t bound_ : 1;
  public:

    RepatchLabel() : offset_(INVALID_OFFSET), bound_(0) {}

    void use(uint32_t newOffset) {
        JS_ASSERT(offset_ == INVALID_OFFSET);
        JS_ASSERT(newOffset != (uint32_t)INVALID_OFFSET);
        offset_ = newOffset;
    }
    bool bound() const {
        return bound_;
    }
    void bind(int32_t dest) {
        JS_ASSERT(!bound_);
        JS_ASSERT(dest != INVALID_OFFSET);
        offset_ = dest;
        bound_ = true;
    }
    int32_t target() {
        JS_ASSERT(bound());
        int32_t ret = offset_;
        offset_ = INVALID_OFFSET;
        return ret;
    }
    int32_t offset() {
        JS_ASSERT(!bound());
        return offset_;
    }
    bool used() const {
        return !bound() && offset_ != (INVALID_OFFSET);
    }

};
// An absolute label is like a Label, except it represents an absolute
// reference rather than a relative one. Thus, it cannot be patched until after
// linking.
struct AbsoluteLabel : public LabelBase
{
  public:
    AbsoluteLabel()
    { }
    AbsoluteLabel(const AbsoluteLabel &label) : LabelBase(label)
    { }
    int32_t prev() const {
        JS_ASSERT(!bound());
        if (!used())
            return INVALID_OFFSET;
        return offset();
    }
    void setPrev(int32_t offset) {
        use(offset);
    }
    void bind() {
        bound_ = true;

        // These labels cannot be used after being bound.
        offset_ = -1;
    }
};

// A code label contains an absolute reference to a point in the code
// Thus, it cannot be patched until after linking
class CodeLabel
{
    // The destination position, where the absolute reference should get patched into
    AbsoluteLabel dest_;

    // The source label (relative) in the code to where the
    // the destination should get patched to.
    Label src_;

  public:
    CodeLabel()
    { }
    CodeLabel(const AbsoluteLabel &dest)
       : dest_(dest)
    { }
    AbsoluteLabel *dest() {
        return &dest_;
    }
    Label *src() {
        return &src_;
    }
};

// Location of a jump or label in a generated IonCode block, relative to the
// start of the block.

class CodeOffsetJump
{
    size_t offset_;

#ifdef JS_SMALL_BRANCH
    size_t jumpTableIndex_;
#endif

  public:

#ifdef JS_SMALL_BRANCH
    CodeOffsetJump(size_t offset, size_t jumpTableIndex)
        : offset_(offset), jumpTableIndex_(jumpTableIndex)
    {}
    size_t jumpTableIndex() const {
        return jumpTableIndex_;
    }
#else
    CodeOffsetJump(size_t offset) : offset_(offset) {}
#endif

    CodeOffsetJump() {
        mozilla::PodZero(this);
    }

    size_t offset() const {
        return offset_;
    }
    void fixup(MacroAssembler *masm);
};

class CodeOffsetLabel
{
    size_t offset_;

  public:
    CodeOffsetLabel(size_t offset) : offset_(offset) {}
    CodeOffsetLabel() : offset_(0) {}

    size_t offset() const {
        return offset_;
    }
    void fixup(MacroAssembler *masm);

};

// Absolute location of a jump or a label in some generated IonCode block.
// Can also encode a CodeOffset{Jump,Label}, such that the offset is initially
// set and the absolute location later filled in after the final IonCode is
// allocated.

class CodeLocationJump
{
    uint8_t *raw_;
#ifdef DEBUG
    enum State { Uninitialized, Absolute, Relative };
    State state_;
    void setUninitialized() {
        state_ = Uninitialized;
    }
    void setAbsolute() {
        state_ = Absolute;
    }
    void setRelative() {
        state_ = Relative;
    }
#else
    void setUninitialized() const {
    }
    void setAbsolute() const {
    }
    void setRelative() const {
    }
#endif

#ifdef JS_SMALL_BRANCH
    uint8_t *jumpTableEntry_;
#endif

  public:
    CodeLocationJump() {
        raw_ = NULL;
        setUninitialized();
#ifdef JS_SMALL_BRANCH
        jumpTableEntry_ = (uint8_t *) 0xdeadab1e;
#endif
    }
    CodeLocationJump(IonCode *code, CodeOffsetJump base) {
        *this = base;
        repoint(code);
    }

    void operator = (CodeOffsetJump base) {
        raw_ = (uint8_t *) base.offset();
        setRelative();
#ifdef JS_SMALL_BRANCH
        jumpTableEntry_ = (uint8_t *) base.jumpTableIndex();
#endif
    }

    void repoint(IonCode *code, MacroAssembler* masm = NULL);

    uint8_t *raw() const {
        JS_ASSERT(state_ == Absolute);
        return raw_;
    }
    uint8_t *offset() const {
        JS_ASSERT(state_ == Relative);
        return raw_;
    }

#ifdef JS_SMALL_BRANCH
    uint8_t *jumpTableEntry() const {
        JS_ASSERT(state_ == Absolute);
        return jumpTableEntry_;
    }
#endif
};

class CodeLocationLabel
{
    uint8_t *raw_;
#ifdef DEBUG
    enum State { Uninitialized, Absolute, Relative };
    State state_;
    void setUninitialized() {
        state_ = Uninitialized;
    }
    void setAbsolute() {
        state_ = Absolute;
    }
    void setRelative() {
        state_ = Relative;
    }
#else
    void setUninitialized() const {
    }
    void setAbsolute() const {
    }
    void setRelative() const {
    }
#endif

  public:
    CodeLocationLabel() {
        raw_ = NULL;
        setUninitialized();
    }
    CodeLocationLabel(IonCode *code, CodeOffsetLabel base) {
        *this = base;
        repoint(code);
    }
    CodeLocationLabel(IonCode *code) {
        raw_ = code->raw();
        setAbsolute();
    }
    CodeLocationLabel(uint8_t *raw) {
        raw_ = raw;
        setAbsolute();
    }

    void operator = (CodeOffsetLabel base) {
        raw_ = (uint8_t *)base.offset();
        setRelative();
    }
    ptrdiff_t operator - (const CodeLocationLabel &other) {
        return raw_ - other.raw_;
    }

    void repoint(IonCode *code, MacroAssembler *masm = NULL);

#ifdef DEBUG
    bool isSet() const {
        return state_ != Uninitialized;
    }
#endif

    uint8_t *raw() const {
        JS_ASSERT(state_ == Absolute);
        return raw_;
    }
    uint8_t *offset() const {
        JS_ASSERT(state_ == Relative);
        return raw_;
    }
};


} // namespace jit
} // namespace js

#endif /* jit_shared_Assembler_shared_h */
