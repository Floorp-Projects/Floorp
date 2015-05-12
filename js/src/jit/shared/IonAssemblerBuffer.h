/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_IonAssemblerBuffer_h
#define jit_shared_IonAssemblerBuffer_h

#include "mozilla/Assertions.h"

#include "jit/shared/Assembler-shared.h"

namespace js {
namespace jit {

// The offset into a buffer, in bytes.
class BufferOffset
{
    int offset;

  public:
    friend BufferOffset nextOffset();

    BufferOffset()
      : offset(INT_MIN)
    { }

    explicit BufferOffset(int offset_)
      : offset(offset_)
    { }

    explicit BufferOffset(Label* l)
      : offset(l->offset())
    { }

    explicit BufferOffset(RepatchLabel* l)
      : offset(l->offset())
    { }

    int getOffset() const { return offset; }
    bool assigned() const { return offset != INT_MIN; }

    // A BOffImm is a Branch Offset Immediate. It is an architecture-specific
    // structure that holds the immediate for a pc relative branch. diffB takes
    // the label for the destination of the branch, and encodes the immediate
    // for the branch. This will need to be fixed up later, since A pool may be
    // inserted between the branch and its destination.
    template <class BOffImm>
    BOffImm diffB(BufferOffset other) const {
        return BOffImm(offset - other.offset);
    }

    template <class BOffImm>
    BOffImm diffB(Label* other) const {
        MOZ_ASSERT(other->bound());
        return BOffImm(offset - other->offset());
    }
};

template<int SliceSize>
class BufferSlice
{
  protected:
    BufferSlice<SliceSize>* prev_;
    BufferSlice<SliceSize>* next_;

    size_t bytelength_;

  public:
    mozilla::Array<uint8_t, SliceSize> instructions;

  public:
    explicit BufferSlice()
      : prev_(nullptr), next_(nullptr), bytelength_(0)
    { }

    size_t length() const { return bytelength_; }
    static inline size_t Capacity() { return SliceSize; }

    BufferSlice* getNext() const { return next_; }
    BufferSlice* getPrev() const { return prev_; }

    void setNext(BufferSlice<SliceSize>* next) {
        MOZ_ASSERT(next_ == nullptr);
        MOZ_ASSERT(next->prev_ == nullptr);
        next_ = next;
        next->prev_ = this;
    }

    void putBytes(size_t numBytes, const uint8_t* source) {
        MOZ_ASSERT(bytelength_ + numBytes <= SliceSize);
        if (source)
            memcpy(&instructions[length()], source, numBytes);
        bytelength_ += numBytes;
    }
};

template<int SliceSize, class Inst>
class AssemblerBuffer
{
    typedef BufferSlice<SliceSize> Slice;
    typedef AssemblerBuffer<SliceSize, Inst> AssemblerBuffer_;

  protected:
    // Doubly-linked list of BufferSlices, with the most recent in tail position.
    Slice* head;
    Slice* tail;

  public:
    bool m_oom;
    bool m_bail;

    // How many bytes has been committed to the buffer thus far.
    // Does not include tail.
    uint32_t bufferSize;
    uint32_t lastInstSize;

    // Finger for speeding up accesses.
    Slice* finger;
    int finger_offset;

    LifoAlloc lifoAlloc_;

  public:
    explicit AssemblerBuffer()
      : head(nullptr),
        tail(nullptr),
        m_oom(false),
        m_bail(false),
        bufferSize(0),
        lastInstSize(0),
        finger(nullptr),
        finger_offset(0),
        lifoAlloc_(8192)
    { }

  public:
    bool isAligned(int alignment) const {
        MOZ_ASSERT(IsPowerOfTwo(alignment));
        return !(size() & (alignment - 1));
    }

    virtual Slice* newSlice(LifoAlloc& a) {
        Slice* tmp = static_cast<Slice*>(a.alloc(sizeof(Slice)));
        if (!tmp) {
            fail_oom();
            return nullptr;
        }
        return new (tmp) Slice;
    }

    bool ensureSpace(int size) {
        // Space can exist in the most recent Slice.
        if (tail && tail->length() + size <= tail->Capacity())
            return true;

        // Otherwise, a new Slice must be added.
        Slice* slice = newSlice(lifoAlloc_);
        if (slice == nullptr)
            return fail_oom();

        // If this is the first Slice in the buffer, add to head position.
        if (!head) {
            head = slice;
            finger = slice;
            finger_offset = 0;
        }

        // Finish the last Slice and add the new Slice to the linked list.
        if (tail) {
            bufferSize += tail->length();
            tail->setNext(slice);
        }
        tail = slice;

        return true;
    }

    BufferOffset putByte(uint8_t value) {
        return putBytes(sizeof(value), (uint8_t*)&value);
    }

    BufferOffset putShort(uint16_t value) {
        return putBytes(sizeof(value), (uint8_t*)&value);
    }

    BufferOffset putInt(uint32_t value) {
        return putBytes(sizeof(value), (uint8_t*)&value);
    }
    BufferOffset putBytes(uint32_t instSize, uint8_t* inst) {
        if (!ensureSpace(instSize))
            return BufferOffset();

        BufferOffset ret = nextOffset();
        tail->putBytes(instSize, inst);
        return ret;
    }

    unsigned int size() const {
        if (tail)
            return bufferSize + tail->length();
        return bufferSize;
    }

    bool oom() const { return m_oom || m_bail; }
    bool bail() const { return m_bail; }

    bool fail_oom() {
        m_oom = true;
        return false;
    }
    bool fail_bail() {
        m_bail = true;
        return false;
    }
    void update_finger(Slice* finger_, int fingerOffset_) {
        finger = finger_;
        finger_offset = fingerOffset_;
    }

  private:
    static const unsigned SliceDistanceRequiringFingerUpdate = 3;

    Inst* getInstForwards(BufferOffset off, Slice* start, int startOffset, bool updateFinger = false) {
        const int offset = off.getOffset();

        int cursor = startOffset;
        unsigned slicesSkipped = 0;

        MOZ_ASSERT(offset >= cursor);

        for (Slice *slice = start; slice != nullptr; slice = slice->getNext()) {
            const int slicelen = slice->length();

            // Is the offset within the bounds of this slice?
            if (offset < cursor + slicelen) {
                if (updateFinger || slicesSkipped >= SliceDistanceRequiringFingerUpdate)
                    update_finger(slice, cursor);

                MOZ_ASSERT(offset - cursor < (int)slice->length());
                return (Inst*)&slice->instructions[offset - cursor];
            }

            cursor += slicelen;
            slicesSkipped++;
        }

        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Invalid instruction cursor.");
    }

    Inst* getInstBackwards(BufferOffset off, Slice* start, int startOffset, bool updateFinger = false) {
        const int offset = off.getOffset();

        int cursor = startOffset; // First (lowest) offset in the start Slice.
        unsigned slicesSkipped = 0;

        MOZ_ASSERT(offset < int(cursor + start->length()));

        for (Slice* slice = start; slice != nullptr; ) {
            // Is the offset within the bounds of this slice?
            if (offset >= cursor) {
                if (updateFinger || slicesSkipped >= SliceDistanceRequiringFingerUpdate)
                    update_finger(slice, cursor);

                MOZ_ASSERT(offset - cursor < (int)slice->length());
                return (Inst*)&slice->instructions[offset - cursor];
            }

            // Move the cursor to the start of the previous slice.
            Slice* prev = slice->getPrev();
            cursor -= prev->length();

            slice = prev;
            slicesSkipped++;
        }

        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Invalid instruction cursor.");
    }

  public:
    Inst* getInst(BufferOffset off) {
        const int offset = off.getOffset();

        // Is the instruction in the last slice?
        if (offset >= int(bufferSize))
            return (Inst*)&tail->instructions[offset - bufferSize];

        // How close is this offset to the previous one we looked up?
        // If it is sufficiently far from the start and end of the buffer,
        // use the finger to start midway through the list.
        int finger_dist = abs(offset - finger_offset);
        if (finger_dist < Min(offset, int(bufferSize - offset))) {
            if (finger_offset < offset)
                return getInstForwards(off, finger, finger_offset, true);
            return getInstBackwards(off, finger, finger_offset, true);
        }

        // Is the instruction closer to the start or to the end?
        if (offset < int(bufferSize - offset))
            return getInstForwards(off, head, 0);

        // The last slice was already checked above, so start at the second-to-last.
        Slice* prev = tail->getPrev();
        return getInstBackwards(off, prev, bufferSize - prev->length());
    }

    BufferOffset nextOffset() const {
        if (tail)
            return BufferOffset(bufferSize + tail->length());
        return BufferOffset(bufferSize);
    }

    // Break the instruction stream so we can go back and edit it at this point
    void perforate() {
        Slice* slice = newSlice(lifoAlloc_);
        if (!slice) {
            fail_oom();
            return;
        }

        bufferSize += tail->length();
        tail->setNext(slice);
        tail = slice;
    }

    class AssemblerBufferInstIterator
    {
        BufferOffset bo;
        AssemblerBuffer_* m_buffer;

      public:
        explicit AssemblerBufferInstIterator(BufferOffset off, AssemblerBuffer_* buffer)
          : bo(off), m_buffer(buffer)
        { }

        Inst* next() {
            Inst* i = m_buffer->getInst(bo);
            bo = BufferOffset(bo.getOffset() + i->size());
            return cur();
        }

        Inst* cur() {
            return m_buffer->getInst(bo);
        }
    };
};

} // namespace ion
} // namespace js

#endif // jit_shared_IonAssemblerBuffer_h
