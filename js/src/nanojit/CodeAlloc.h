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
    /**
     * CodeList is a single block of code.  The next field is used to
     * form linked lists of non-contiguous blocks of code.  Clients use CodeList*
     * to point to the first block in a list.
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

        /** pointer to the heapblock terminal that represents the code chunk containing this block */
        CodeList* terminator;

        /** true if block is free, false otherwise */
        bool isFree;

        /** (only valid for terminator blocks).  Set true just before calling
         * markCodeChunkExec() and false just after markCodeChunkWrite() */
        bool isExec;

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

    public:
        /** true is the given NIns is contained within this block */
        bool isInBlock(NIns* n) { return (n >= this->start() && n < this->end); }
    };

    /**
     * Code memory allocator is a long lived manager for many code blocks that
     * manages interaction with an underlying code memory allocator,
     * sets page permissions.  CodeAlloc provides APIs for allocating and freeing
     * individual blocks of code memory (for methods, stubs, or compiled
     * traces), static functions for managing lists of allocated code, and has
     * a few pure virtual methods that embedders must implement to provide
     * memory to the allocator.
     *
     * A "chunk" is a region of memory obtained from allocCodeChunk; it must
     * be page aligned and be a multiple of the system page size.
     *
     * A "block" is a region of memory within a chunk.  It can be arbitrarily
     * sized and aligned, but is always contained within a single chunk.
     * class CodeList represents one block; the members of CodeList track the
     * extent of the block and support creating lists of blocks.
     *
     * The allocator coalesces free blocks when it can, in free(), but never
     * coalesces chunks.
     */
    class CodeAlloc
    {
        static const size_t sizeofMinBlock = offsetof(CodeList, code);
        static const size_t minAllocSize = LARGEST_UNDERRUN_PROT;

        // Return the number of bytes needed for the header of 'n' blocks
        static size_t headerSpaceFor(uint32_t nbrBlks)  { return nbrBlks * sizeofMinBlock; }

        // Return the number of bytes needed in order to safely construct 'n' blocks
        static size_t blkSpaceFor(uint32_t nbrBlks)     { return (nbrBlks * minAllocSize) + headerSpaceFor(nbrBlks); }

        /** Terminator blocks.  All active and free allocations
            are reachable by traversing this chain and each
            element's lower chain. */
        CodeList* heapblocks;

        /** Reusable blocks. */
        CodeList* availblocks;
        size_t totalAllocated;

        /** Cached value of VMPI_getVMPageSize */
        const size_t bytesPerPage;

        /** Number of bytes to request from VMPI layer, always a multiple of the page size */
        const size_t bytesPerAlloc;

        /** remove one block from a list */
        static CodeList* removeBlock(CodeList* &list);

        /** add one block to a list */
        static void addBlock(CodeList* &blocks, CodeList* b);

        /** compute the CodeList pointer from a [start, end) range */
        static CodeList* getBlock(NIns* start, NIns* end);

        /** add raw memory to the free list */
        void addMem();

        /** make sure all the higher/lower pointers are correct for every block */
        void sanity_check();

        /** find the beginning of the heapblock terminated by term */
        CodeList* firstBlock(CodeList* term);

        //
        // CodeAlloc's SPI (Service Provider Interface).  Implementations must be
        // defined by nanojit embedder.  Allocation failures should cause an exception
        // or longjmp; nanojit intentionally does not check for null.
        //

        /** allocate nbytes of memory to hold code.  Never return null! */
        void* allocCodeChunk(size_t nbytes);

        /** free a block previously allocated by allocCodeMem.  nbytes will
         * match the previous allocCodeMem, but is provided here as well
         * to mirror the mmap()/munmap() api.  markCodeChunkWrite() will have
         * been called if necessary, so it is not necessary for freeCodeChunk()
         * to do it again. */
        void freeCodeChunk(void* addr, size_t nbytes);

        /** make this specific extent ready to execute (might remove write) */
        void markCodeChunkExec(void* addr, size_t nbytes);

        /** make this extent ready to modify (might remove exec) */
        void markCodeChunkWrite(void* addr, size_t nbytes);

    public:
        CodeAlloc();
        ~CodeAlloc();

        /** return all the memory allocated through this allocator to the gcheap. */
        void reset();

        /** allocate some memory (up to 'byteLimit' bytes) for code returning pointers to the region.  A zero 'byteLimit' means no limit */
        void alloc(NIns* &start, NIns* &end, size_t byteLimit);

        /** free a block of memory previously returned by alloc() */
        void free(NIns* start, NIns* end);

        /** free several blocks */
        void freeAll(CodeList* &code);

        /** flush the icache for all code in the list, before executing */
        static void flushICache(CodeList* &blocks);

        /** flush the icache for a specific extent */
        static void flushICache(void *start, size_t len);

        /** add the ranges [start, holeStart) and [holeEnd, end) to code, and
            free [holeStart, holeEnd) if the hole is >= minsize */
        void addRemainder(CodeList* &code, NIns* start, NIns* end, NIns* holeStart, NIns* holeEnd);

        /** add a block previously returned by alloc(), to code */
        static void add(CodeList* &code, NIns* start, NIns* end);

        /** return the number of bytes in all the code blocks in "code", including block overhead */
#ifdef PERFM
        static size_t size(const CodeList* code);
#endif

        /** return the total number of bytes held by this CodeAlloc. */
        size_t size();

        /** print out stats about heap usage */
        void logStats();

        /** protect all code managed by this CodeAlloc */
        void markAllExec();

        /** protect all mem in the block list */
        void markExec(CodeList* &blocks);

        /** protect an entire chunk */
        void markChunkExec(CodeList* term);

        /** unprotect the code chunk containing just this one block */
        void markBlockWrite(CodeList* b);
    };
}

#endif // __nanojit_CodeAlloc__
