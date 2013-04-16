/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ion_assembler_buffer_h
#define __ion_assembler_buffer_h
// needed for the definition of Label :(
#include "ion/shared/Assembler-shared.h"

namespace js {
namespace ion {

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
struct BufferSlice : public InlineForwardListNode<BufferSlice<SliceSize> > {
  protected:
    // How much data has been added to the current node.
    uint32_t nodeSize;
  public:
    BufferSlice *getNext() { return static_cast<BufferSlice *>(this->next); }
    void setNext(BufferSlice<SliceSize> *next_) {
        JS_ASSERT(this->next == NULL);
        this->next = next_;
    }
    uint8_t instructions [SliceSize];
    unsigned int size() {
        return nodeSize;
    }
    BufferSlice() : InlineForwardListNode<BufferSlice<SliceSize> >(NULL), nodeSize(0) {}
    void putBlob(uint32_t instSize, uint8_t* inst) {
        if (inst != NULL)
            memcpy(&instructions[size()], inst, instSize);
        nodeSize += instSize;
    }
};

template<int SliceSize, class Inst>
struct AssemblerBuffer
{
  public:
    AssemblerBuffer() : head(NULL), tail(NULL), m_oom(false), m_bail(false), bufferSize(0), LifoAlloc_(8192) {}
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
            return NULL;
        }
        new (tmp) Slice;
        return tmp;
    }
    bool ensureSpace(int size) {
        if (tail != NULL && tail->size()+size <= SliceSize)
            return true;
        Slice *tmp = newSlice(LifoAlloc_);
        if (tmp == NULL)
            return false;
        if (tail != NULL) {
            bufferSize += tail->size();
            tail->setNext(tmp);
        }
        tail = tmp;
        if (head == NULL)
            head = tmp;
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
        if (tail != NULL)
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
    Inst *getInst(BufferOffset off) {
        unsigned int local_off = off.getOffset();
        Slice *cur = NULL;
        if (local_off > bufferSize) {
            local_off -= bufferSize;
            cur = tail;
        } else {
            for (cur = head; cur != NULL; cur = cur->getNext()) {
                if (local_off < cur->size())
                    break;
                local_off -= cur->size();
            }
            JS_ASSERT(cur != NULL);
        }
        // the offset within this node should not be larger than the node itself.
        JS_ASSERT(local_off < cur->size());
        return (Inst*)&cur->instructions[local_off];
    }
    BufferOffset nextOffset() const {
        if (tail != NULL)
            return BufferOffset(bufferSize + tail->size());
        else
            return BufferOffset(bufferSize);
    }
    BufferOffset prevOffset() const {
        JS_NOT_REACHED("Don't current record lastInstSize");
        return BufferOffset(bufferSize + tail->nodeSize - lastInstSize);
    }

    // Break the instruction stream so we can go back and edit it at this point
    void perforate() {
        Slice *tmp = newSlice(LifoAlloc_);
        if (!tmp)
            m_oom = true;
        bufferSize += tail->size();
        tail->setNext(tmp);
        tail = tmp;
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
#endif // __ion_assembler_buffer_h
