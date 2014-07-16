/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_IonAssemblerBuffer_h
#define jit_shared_IonAssemblerBuffer_h

// needed for the definition of Label :(
#include "jit/shared/Assembler-shared.h"

namespace js {
namespace jit {

// This should theoretically reside inside of AssemblerBuffer, but that won't be nice
// AssemblerBuffer is templated, BufferOffset would be indirectly.
// A BufferOffset is the offset into a buffer, expressed in bytes of instructions.

class BufferOffset
{
    int offset;
  public:
    friend BufferOffset nextOffset();
    explicit BufferOffset(int offset_) : offset(offset_) {}
    // Return the offset as a raw integer.
    int getOffset() const { return offset; }

    // A BOffImm is a Branch Offset Immediate. It is an architecture-specific
    // structure that holds the immediate for a pc relative branch.
    // diffB takes the label for the destination of the branch, and encodes
    // the immediate for the branch.  This will need to be fixed up later, since
    // A pool may be inserted between the branch and its destination
    template <class BOffImm>
    BOffImm diffB(BufferOffset other) const {
        return BOffImm(offset - other.offset);
    }

    template <class BOffImm>
    BOffImm diffB(Label *other) const {
        JS_ASSERT(other->bound());
        return BOffImm(offset - other->offset());
    }

    explicit BufferOffset(Label *l) : offset(l->offset()) {
    }
    explicit BufferOffset(RepatchLabel *l) : offset(l->offset()) {
    }

    BufferOffset() : offset(INT_MIN) {}
    bool assigned() const { return offset != INT_MIN; };
};

template<int SliceSize>
struct BufferSlice {
  protected:
    BufferSlice<SliceSize> *prev;
    BufferSlice<SliceSize> *next;
    // How much data has been added to the current node.
    uint32_t nodeSize;
  public:
    BufferSlice *getNext() { return this->next; }
    BufferSlice *getPrev() { return this->prev; }
    void setNext(BufferSlice<SliceSize> *next_) {
        JS_ASSERT(this->next == nullptr);
        JS_ASSERT(next_->prev == nullptr);
        this->next = next_;
        next_->prev = this;
    }

    mozilla::Array<uint8_t, SliceSize> instructions;
    unsigned int size() {
        return nodeSize;
    }
    BufferSlice() : prev(nullptr), next(nullptr), nodeSize(0) {}
    void putBlob(uint32_t instSize, uint8_t* inst) {
        if (inst != nullptr)
            memcpy(&instructions[size()], inst, instSize);
        nodeSize += instSize;
    }
};

template<int SliceSize, class Inst>
struct AssemblerBuffer
{
  public:
    AssemblerBuffer() : head(nullptr), tail(nullptr), m_oom(false), m_bail(false), bufferSize(0), LifoAlloc_(8192) {}
  protected:
    typedef BufferSlice<SliceSize> Slice;
    typedef AssemblerBuffer<SliceSize, Inst> AssemblerBuffer_;
    Slice *head;
    Slice *tail;
  public:
    bool m_oom;
    bool m_bail;
    // How much data has been added to the buffer thusfar.
    uint32_t bufferSize;
    uint32_t lastInstSize;
    bool isAligned(int alignment) const {
        // make sure the requested alignment is a power of two.
        JS_ASSERT((alignment & (alignment-1)) == 0);
        return !(size() & (alignment - 1));
    }
    virtual Slice *newSlice(LifoAlloc &a) {
        Slice *tmp = static_cast<Slice*>(a.alloc(sizeof(Slice)));
        if (!tmp) {
            m_oom = true;
            return nullptr;
        }
        new (tmp) Slice;
        return tmp;
    }
    bool ensureSpace(int size) {
        if (tail != nullptr && tail->size()+size <= SliceSize)
            return true;
        Slice *tmp = newSlice(LifoAlloc_);
        if (tmp == nullptr)
            return false;
        if (tail != nullptr) {
            bufferSize += tail->size();
            tail->setNext(tmp);
        }
        tail = tmp;
        if (head == nullptr) {
            finger = tmp;
            finger_offset = 0;
            head = tmp;
        }
        return true;
    }

    BufferOffset putByte(uint8_t value) {
        return putBlob(sizeof(value), (uint8_t*)&value);
    }

    BufferOffset putShort(uint16_t value) {
        return putBlob(sizeof(value), (uint8_t*)&value);
    }

    BufferOffset putInt(uint32_t value) {
        return putBlob(sizeof(value), (uint8_t*)&value);
    }
    BufferOffset putBlob(uint32_t instSize, uint8_t *inst) {
        if (!ensureSpace(instSize))
            return BufferOffset();
        BufferOffset ret = nextOffset();
        tail->putBlob(instSize, inst);
        return ret;
    }
    unsigned int size() const {
        int executableSize;
        if (tail != nullptr)
            executableSize = bufferSize + tail->size();
        else
            executableSize = bufferSize;
        return executableSize;
    }
    unsigned int uncheckedSize() const {
        return size();
    }
    bool oom() const {
        return m_oom || m_bail;
    }
    bool bail() const {
        return m_bail;
    }
    void fail_oom() {
        m_oom = true;
    }
    void fail_bail() {
        m_bail = true;
    }
    // finger for speeding up accesses
    Slice *finger;
    unsigned int finger_offset;
    Inst *getInst(BufferOffset off) {
        int local_off = off.getOffset();
        // don't update the structure's finger in place, so there is the option
        // to not update it.
        Slice *cur = nullptr;
        int cur_off;
        // get the offset that we'd be dealing with by walking through backwards
        int end_off = bufferSize - local_off;
        // If end_off is negative, then it is in the last chunk, and there is no
        // real work to be done.
        if (end_off <= 0) {
            return (Inst*)&tail->instructions[-end_off];
        }
        bool used_finger = false;
        int finger_off = abs((int)(local_off - finger_offset));
        if (finger_off < Min(local_off, end_off)) {
            // The finger offset is minimal, use the finger.
            cur = finger;
            cur_off = finger_offset;
            used_finger = true;
        } else if (local_off < end_off) {
            // it is closest to the start
            cur = head;
            cur_off = 0;
        } else {
            // it is closest to the end
            cur = tail;
            cur_off = bufferSize;
        }
        int count = 0;
        if (local_off < cur_off) {
            for (; cur != nullptr; cur = cur->getPrev(), cur_off -= cur->size()) {
                if (local_off >= cur_off) {
                    local_off -= cur_off;
                    break;
                }
                count++;
            }
            JS_ASSERT(cur != nullptr);
        } else {
            for (; cur != nullptr; cur = cur->getNext()) {
                int cur_size = cur->size();
                if (local_off < cur_off + cur_size) {
                    local_off -= cur_off;
                    break;
                }
                cur_off += cur_size;
                count++;
            }
            JS_ASSERT(cur != nullptr);
        }
        if (count > 2 || used_finger) {
            finger = cur;
            finger_offset = cur_off;
        }
        // the offset within this node should not be larger than the node itself.
        JS_ASSERT(local_off < (int)cur->size());
        return (Inst*)&cur->instructions[local_off];
    }
    BufferOffset nextOffset() const {
        if (tail != nullptr)
            return BufferOffset(bufferSize + tail->size());
        else
            return BufferOffset(bufferSize);
    }
    BufferOffset prevOffset() const {
        MOZ_ASSUME_UNREACHABLE("Don't current record lastInstSize");
    }

    // Break the instruction stream so we can go back and edit it at this point
    void perforate() {
        Slice *tmp = newSlice(LifoAlloc_);
        if (!tmp) {
            m_oom = true;
            return;
        }
        bufferSize += tail->size();
        tail->setNext(tmp);
        tail = tmp;
    }

    void executableCopy(uint8_t *dest_) {
        if (this->oom())
            return;

        for (Slice *cur = head; cur != nullptr; cur = cur->getNext()) {
            memcpy(dest_, &cur->instructions, cur->size());
            dest_ += cur->size();
        }
    }

    class AssemblerBufferInstIterator {
      private:
        BufferOffset bo;
        AssemblerBuffer_ *m_buffer;
      public:
        AssemblerBufferInstIterator(BufferOffset off, AssemblerBuffer_ *buff) : bo(off), m_buffer(buff) {}
        Inst *next() {
            Inst *i = m_buffer->getInst(bo);
            bo = BufferOffset(bo.getOffset()+i->size());
            return cur();
        };
        Inst *cur() {
            return m_buffer->getInst(bo);
        }
    };
  public:
    LifoAlloc LifoAlloc_;
};

} // ion
} // js
#endif /* jit_shared_IonAssemblerBuffer_h */
