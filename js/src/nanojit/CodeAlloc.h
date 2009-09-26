/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __nanojit_CodeAlloc__
#define __nanojit_CodeAlloc__

namespace nanojit
{
    /** return true if ptr is in the range [start, end] */
    inline bool containsPtr(const NIns* start, const NIns* end, const NIns* ptr) {
        return ptr >= start && ptr <= end;
    }

    /**
     * CodeList is a linked list of non-contigous blocks of code.  Clients use CodeList*
     * to point to a list, and each CodeList instance tracks a single contiguous
     * block of code.
     */
    class CodeList
    {
        friend class CodeAlloc;

        /** for making singly linked lists of blocks in any order */
        CodeList* next;

        /** adjacent block at lower address.  This field plus higher
            form a doubly linked list of blocks in address order, used
            for splitting and coalescing blocks. */
        CodeList* lower;

        /** true if block is free, false otherwise */
        bool isFree;
        union {
            // this union is used in leu of pointer punning in code
            // the end of this block is always the address of the next higher block
            CodeList* higher;   // adjacent block at higher address
            NIns* end;          // points just past the end
        };

        /** code holds this block's payload of binary code, from
            here to this->end */
        NIns  code[1]; // more follows

        /** return the starting address for this block only */
        NIns* start() { return &code[0]; }

        /** return just the usable size of this block */
        size_t size() const { return uintptr_t(end) - uintptr_t(&code[0]); }

        /** return the whole size of this block including overhead */
        size_t blockSize() const { return uintptr_t(end) - uintptr_t(this); }

        /** return true if just this block contains p */
        bool contains(NIns* p) const  { return containsPtr(&code[0], end, p); }
    };

    /**
     * Code memory allocator.
     * Long lived manager for many code blocks,
     * manages interaction with an underlying code memory allocator,
     * setting page permissions, api's for allocating and freeing
     * individual blocks of code memory (for methods, stubs, or compiled
     * traces), and also static functions for managing lists of allocated
     * code.
     */
    class CodeAlloc
    {
        static const size_t sizeofMinBlock = offsetof(CodeList, code);
        static const size_t minAllocSize = LARGEST_UNDERRUN_PROT;

        /** Terminator blocks.  All active and free allocations
            are reachable by traversing this chain and each
            element's lower chain. */
        CodeList* heapblocks;

        /** Reusable blocks. */
        CodeList* availblocks;
        size_t totalAllocated;

        /** remove one block from a list */
        static CodeList* removeBlock(CodeList* &list);

        /** add one block to a list */
        static void addBlock(CodeList* &blocks, CodeList* b);

        /** compute the CodeList pointer from a [start, end) range */
        static CodeList* getBlock(NIns* start, NIns* end);

        /** add raw memory to the free list */
        CodeList* addMem(void* mem, size_t bytes);

        /** make sure all the higher/lower pointers are correct for every block */
        void sanity_check();

        /** find the beginning of the heapblock terminated by term */
        static CodeList* firstBlock(CodeList* term);

        //
        // CodeAlloc's SPI.  Implementations must be defined by nanojit embedder.
        // allocation failures should cause an exception or longjmp; nanojit
        // intentionally does not check for null.
        //

        /** allocate nbytes of memory to hold code.  Never return null! */
        void* allocCodeChunk(size_t nbytes);

        /** free a block previously allocated by allocCodeMem.  nbytes will
         * match the previous allocCodeMem, but is provided here as well
         * to mirror the mmap()/munmap() api. */
        void freeCodeChunk(void* addr, size_t nbytes);

    public:
        CodeAlloc();
        ~CodeAlloc();

        /** return all the memory allocated through this allocator to the gcheap. */
        void reset();

        /** allocate some memory for code, return pointers to the region. */
        void alloc(NIns* &start, NIns* &end);

        /** free a block of memory previously returned by alloc() */
        void free(NIns* start, NIns* end);

        /** free several blocks */
        void freeAll(CodeList* &code);

        /** flush the icache for all code in the list, before executing */
        void flushICache(CodeList* &blocks);

        /** add the ranges [start, holeStart) and [holeEnd, end) to code, and
            free [holeStart, holeEnd) if the hole is >= minsize */
        void addRemainder(CodeList* &code, NIns* start, NIns* end, NIns* holeStart, NIns* holeEnd);

        /** add a block previously returned by alloc(), to code */
        static void add(CodeList* &code, NIns* start, NIns* end);

        /** move all the code in list "from" to list "to", and leave from empty. */
        static void moveAll(CodeList* &to, CodeList* &from);

        /** return true if any block in list "code" contains the code pointer p */
        static bool contains(const CodeList* code, NIns* p);

        /** return the number of bytes in all the code blocks in "code", including block overhead */
        static size_t size(const CodeList* code);

        /** return the total number of bytes held by this CodeAlloc. */
        size_t size();

        /** print out stats about heap usage */
        void logStats();

        enum CodePointerKind {
            kUnknown, kFree, kUsed
        };

        /** determine whether the given address is not code, or is allocated or free */
        CodePointerKind classifyPtr(NIns *p);

        /** return any completely empty pages */
        void sweep();
    };
}

#endif // __nanojit_CodeAlloc__
