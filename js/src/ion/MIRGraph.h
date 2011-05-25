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
 *   David Anderson <danderson@mozilla.com>
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

#ifndef jsion_mirgraph_h__
#define jsion_mirgraph_h__

#include "MIR.h"

namespace js {
namespace ion {

class MIRGraph
{
    Vector<MBasicBlock *, 8, IonAllocPolicy> blocks_;
    uint32 idGen_;

  public:
    MIRGraph();

    bool addBlock(MBasicBlock *block);

    size_t numBlocks() const {
        return blocks_.length();
    }
    MBasicBlock *getBlock(size_t i) const {
        return blocks_[i];
    }
    uint32 allocInstructionId() {
        idGen_ += 2;
        return idGen_;
    }
};

typedef InlineList<MInstruction>::iterator MInstructionIterator;

class MBasicBlock : public TempObject
{
    static const uint32 NotACopy = uint32(-1);

    struct StackSlot {
        MInstruction *ins;
        uint32 copyOf;
        union {
            uint32 firstCopy; // copyOf == NotACopy: first copy in the linked list
            uint32 nextCopy;  // copyOf != NotACopy: next copy in the linked list
        };

        void set(MInstruction *ins) {
            this->ins = ins;
            copyOf = NotACopy;
            firstCopy = NotACopy;
        }
        bool isCopy() const {
            return copyOf != NotACopy;
        }
        bool isCopied() const {
            if (isCopy())
                return false;
            return firstCopy != NotACopy;
        }
    };

  private:
    MBasicBlock(MIRGenerator *gen, jsbytecode *pc);
    bool init();
    void copySlots(MBasicBlock *from);
    bool inherit(MBasicBlock *pred);
    void assertUsesAreNotWithin(MUse *use);

    // Sets a slot, taking care to rewrite copies.
    void setSlot(uint32 slot, MInstruction *ins);

    // Pushes a copy of a slot.
    void pushCopy(uint32 slot);

    // Pushes a copy of a local variable or argument.
    void pushVariable(uint32 slot);

    // Sets a variable slot to the top of the stack, correctly creating copies
    // as needed.
    bool setVariable(uint32 slot);

  public:
    ///////////////////////////////////////////////////////
    ////////// BEGIN GRAPH BUILDING INSTRUCTIONS //////////
    ///////////////////////////////////////////////////////

    // Creates a new basic block for a MIR generator. If |pred| is not NULL,
    // its slots and stack depth are initialized from |pred|.
    static MBasicBlock *New(MIRGenerator *gen, MBasicBlock *pred, jsbytecode *entryPc);
    static MBasicBlock *NewLoopHeader(MIRGenerator *gen, MBasicBlock *pred, jsbytecode *entryPc);

    // Copies the stack state to the header. This should only be called if the
    // block was created with no predecessor, and should be called once the
    // stack state is initialized.
    bool initHeader();

    void setId(uint32 id) {
        id_ = id;
    }

    // Gets the instruction associated with various slot types.
    MInstruction *peek(int32 depth);

    // Initializes a slot value; must not be called for normal stack
    // operations, as it will not create new SSA names for copies.
    void initSlot(uint32 index, MInstruction *ins);

    // Sets the instruction associated with various slot types. The
    // instruction must lie at the top of the stack.
    bool setLocal(uint32 local);
    bool setArg(uint32 arg);

    // Tracks an instruction as being pushed onto the operand stack.
    void push(MInstruction *ins);
    void pushArg(uint32 arg);
    void pushLocal(uint32 local);

    // Returns the top of the stack, then decrements the virtual stack pointer.
    MInstruction *pop();

    // Adds an instruction to this block's instruction list. |ins| may be NULL
    // to simplify OOM checking.
    bool add(MInstruction *ins);

    // Marks the last instruction of the block; no further instructions
    // can be added.
    bool end(MControlInstruction *ins);

    // Adds a phi instruction.
    bool addPhi(MPhi *phi);

    // Adds a predecessor. Every predecessor must have the same exit stack
    // depth as the entry state to this block. Adding a predecessor
    // automatically creates phi nodes and rewrites uses as needed.
    bool addPredecessor(MBasicBlock *pred);

    // Sets a back edge. This places phi nodes and rewrites instructions within
    // the current loop as necessary, and corrects the successor block's initial
    // state at the same time. There may be only one backedge per block.
    bool setBackedge(MBasicBlock *block, MBasicBlock *successor);

    bool insertBefore(MInstruction *at, MInstruction *ins);
    bool insertAfter(MInstruction *at, MInstruction *ins);

    ///////////////////////////////////////////////////////
    /////////// END GRAPH BUILDING INSTRUCTIONS ///////////
    ///////////////////////////////////////////////////////

    jsbytecode *pc() const {
        return pc_;
    }
    uint32 id() const {
        return id_;
    }
    uint32 numPredecessors() const {
        return predecessors_.length();
    }
    MBasicBlock *getPredecessor(uint32 i) const {
        return predecessors_[i];
    }
    MControlInstruction *lastIns() const {
        return lastIns_;
    }
    size_t numEntrySlots() const {
        return headerSlots_;
    }
    MInstruction *getEntrySlot(size_t i) const {
        JS_ASSERT(i < numEntrySlots());
        return header_[i];
    }
    size_t numPhis() const {
        return phis_.length();
    }
    MPhi *getPhi(size_t i) const {
        return phis_[i];
    }
    MInstructionIterator begin() {
        return instructions_.begin();
    }
    MInstructionIterator end() {
        return instructions_.end();
    }
    bool isLoopHeader() const {
        return loopSuccessor_ != NULL;
    }
    MBasicBlock *getLoopSuccessor() const {
        JS_ASSERT(isLoopHeader());
        return loopSuccessor_;
    }
    MIRGenerator *gen() {
        return gen_;
    }
    uint32 stackDepth() const {
        return stackPosition_;
    }

    // This function retrieves the internal instruction associated with a
    // slot, and should not be used for normal stack operations. It is an
    // internal helper that is also used to enhance spew.
    MInstruction *getSlot(uint32 index);

  private:
    MIRGenerator *gen_;
    InlineList<MInstruction> instructions_;
    Vector<MBasicBlock *, 1, IonAllocPolicy> predecessors_;
    Vector<MPhi *, 2, IonAllocPolicy> phis_;
    StackSlot *slots_;
    uint32 stackPosition_;
    MControlInstruction *lastIns_;
    jsbytecode *pc_;
    uint32 id_;

    // Stack state at the entry point to the basic block. This is required to
    // compute phi nodes at the back edge to a loop header. It is placed on all
    // blocks, anyway, to assist in debugging.
    MInstruction **header_;
    uint32 headerSlots_;

    // If not NULL, the successor block of the loop for which this block is the
    // header.
    MBasicBlock *loopSuccessor_;
};

} // namespace ion
} // namespace js

#endif // jsion_mirgraph_h__

