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

#include "nanojit.h"

//#define DOPROF
#include "../vprof/vprof.h"

#ifdef FEATURE_NANOJIT

namespace nanojit
{
    static const bool verbose = false;
#if defined(NANOJIT_ARM)
    // ARM requires single-page allocations, due to the constant pool that
    // lives on each page that must be reachable by a 4kb pcrel load.
    static const int pagesPerAlloc = 1;
#else
    static const int pagesPerAlloc = 16;
#endif

    CodeAlloc::CodeAlloc()
        : heapblocks(0)
        , availblocks(0)
        , totalAllocated(0)
        , bytesPerPage(VMPI_getVMPageSize())
        , bytesPerAlloc(pagesPerAlloc * bytesPerPage)
    {
    }

    CodeAlloc::~CodeAlloc() {
        reset();
    }

    void CodeAlloc::reset() {
        // give all memory back to gcheap.  Assumption is that all
        // code is done being used by now.
        for (CodeList* hb = heapblocks; hb != 0; ) {
            _nvprof("free page",1);
            CodeList* next = hb->next;
            CodeList* fb = firstBlock(hb);
            markBlockWrite(fb);
            freeCodeChunk(fb, bytesPerAlloc);
            totalAllocated -= bytesPerAlloc;
            hb = next;
        }
        NanoAssert(!totalAllocated);
        heapblocks = availblocks = 0;
    }

    CodeList* CodeAlloc::firstBlock(CodeList* term) {
        // use uintptr_t, rather than char*, to avoid "increases required alignment" warning
        uintptr_t end = (uintptr_t)alignUp(term, bytesPerPage);
        return (CodeList*) (end - (uintptr_t)bytesPerAlloc);
    }

    static int round(size_t x) {
        return (int)((x + 512) >> 10);
    }

    void CodeAlloc::logStats() {
        size_t total = 0;
        size_t frag_size = 0;
        size_t free_size = 0;
        int free_count = 0;
        for (CodeList* hb = heapblocks; hb != 0; hb = hb->next) {
            total += bytesPerAlloc;
            for (CodeList* b = hb->lower; b != 0; b = b->lower) {
                if (b->isFree) {
                    free_count++;
                    free_size += b->blockSize();
                    if (b->size() < minAllocSize)
                        frag_size += b->blockSize();
                }
            }
        }
        avmplus::AvmLog("code-heap: %dk free %dk fragmented %d\n",
            round(total), round(free_size), frag_size);
    }

    inline void CodeAlloc::markBlockWrite(CodeList* b) {
        NanoAssert(b->terminator != NULL);
        CodeList* term = b->terminator;
        if (term->isExec) {
            markCodeChunkWrite(firstBlock(term), bytesPerAlloc);
            term->isExec = false;
        }
    }

    void CodeAlloc::alloc(NIns* &start, NIns* &end) {
        //  Reuse a block if possible.
        if (availblocks) {
            markBlockWrite(availblocks);
            CodeList* b = removeBlock(availblocks);
            b->isFree = false;
            start = b->start();
            end = b->end;
            if (verbose)
                avmplus::AvmLog("alloc %p-%p %d\n", start, end, int(end-start));
            return;
        }
        // no suitable block found, get more memory
        void *mem = allocCodeChunk(bytesPerAlloc); // allocations never fail
        totalAllocated += bytesPerAlloc;
        NanoAssert(mem != NULL); // see allocCodeChunk contract in CodeAlloc.h
        _nvprof("alloc page", uintptr_t(mem)>>12);
        CodeList* b = addMem(mem, bytesPerAlloc);
        b->isFree = false;
        start = b->start();
        end = b->end;
        if (verbose)
            avmplus::AvmLog("alloc %p-%p %d\n", start, end, int(end-start));
    }

    void CodeAlloc::free(NIns* start, NIns *end) {
        NanoAssert(heapblocks);
        CodeList *blk = getBlock(start, end);
        if (verbose)
            avmplus::AvmLog("free %p-%p %d\n", start, end, (int)blk->size());

        NanoAssert(!blk->isFree);

        // coalesce adjacent blocks.
        bool already_on_avail_list;

        if (blk->lower && blk->lower->isFree) {
            // combine blk into blk->lower (destroy blk)
            CodeList* lower = blk->lower;
            CodeList* higher = blk->higher;
            already_on_avail_list = lower->size() >= minAllocSize;
            lower->higher = higher;
            higher->lower = lower;
            blk = lower;
        }
        else
            already_on_avail_list = false;

        // the last block in each heapblock is a terminator block,
        // which is never free, therefore blk->higher != null
        if (blk->higher->isFree) {
            CodeList *higher = blk->higher->higher;
            CodeList *coalescedBlock = blk->higher;

            if ( coalescedBlock->size() >= minAllocSize ) {
                // Unlink coalescedBlock from the available block chain.
                if ( availblocks == coalescedBlock ) {
                    removeBlock(availblocks);
                }
                else {
                    CodeList* free_block = availblocks;
                    while ( free_block && free_block->next != coalescedBlock) {
                        NanoAssert(free_block->size() >= minAllocSize);
                        NanoAssert(free_block->isFree);
                        NanoAssert(free_block->next);
                        free_block = free_block->next;
                    }
                    NanoAssert(free_block && free_block->next == coalescedBlock);
                    free_block->next = coalescedBlock->next;
                }
            }

            // combine blk->higher into blk (destroy coalescedBlock)
            blk->higher = higher;
            higher->lower = blk;
        }
        blk->isFree = true;
        NanoAssert(!blk->lower || !blk->lower->isFree);
        NanoAssert(blk->higher && !blk->higher->isFree);
        //memset(blk->start(), 0xCC, blk->size()); // INT 3 instruction
        if ( !already_on_avail_list && blk->size() >= minAllocSize )
            addBlock(availblocks, blk);

        NanoAssert(heapblocks);
        debug_only(sanity_check();)
    }

    void CodeAlloc::freeAll(CodeList* &code) {
        while (code) {
            CodeList *b = removeBlock(code);
            free(b->start(), b->end);
        }
    }

#if defined NANOJIT_ARM && defined UNDER_CE
    // Use a single flush for the whole CodeList, when we have no
    // finer-granularity flush support, as on WinCE.
    void CodeAlloc::flushICache(CodeList* &/*blocks*/) {
        FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
    }
#else
    void CodeAlloc::flushICache(CodeList* &blocks) {
        for (CodeList *b = blocks; b != 0; b = b->next)
            flushICache(b->start(), b->size());
    }
#endif

#if defined(AVMPLUS_UNIX) && defined(NANOJIT_ARM)
#include <asm/unistd.h>
extern "C" void __clear_cache(char *BEG, char *END);
#endif

#if defined(AVMPLUS_UNIX) && defined(NANOJIT_MIPS)
#include <asm/cachectl.h>
extern  "C" int cacheflush(char *addr, int nbytes, int cache);
#endif

#ifdef AVMPLUS_SPARC
// Note: the linux #define provided by the compiler.
#ifdef linux  // bugzilla 502369
void sync_instruction_memory(caddr_t v, u_int len)
{
    caddr_t end = v + len;
    caddr_t p = v;
    while (p < end) {
        asm("flush %0" : : "r" (p));
        p += 32;
    }
}
#else
extern  "C" void sync_instruction_memory(caddr_t v, u_int len);
#endif
#endif

#if defined NANOJIT_IA32 || defined NANOJIT_X64
    // intel chips have dcache/icache interlock
    void CodeAlloc::flushICache(void *start, size_t len) {
        // Tell Valgrind that new code has been generated, and it must flush
        // any translations it has for the memory range generated into.
        (void)start;
        (void)len;
        VALGRIND_DISCARD_TRANSLATIONS(start, len);
    }

#elif defined NANOJIT_ARM && defined UNDER_CE
    // On arm/winmo, just flush the whole icache. The
    // WinCE docs indicate that this function actually ignores its
    // 2nd and 3rd arguments, and wants them to be NULL.
    void CodeAlloc::flushICache(void *, size_t) {
        FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
    }

#elif defined AVMPLUS_MAC && defined NANOJIT_PPC

#  ifdef NANOJIT_64BIT
    extern "C" void sys_icache_invalidate(const void*, size_t len);
    extern "C" void sys_dcache_flush(const void*, size_t len);

    // mac 64bit requires 10.5 so use that api
    void CodeAlloc::flushICache(void *start, size_t len) {
        sys_dcache_flush(start, len);
        sys_icache_invalidate(start, len);
    }
#  else
    // mac ppc 32 could be 10.0 or later
    // uses MakeDataExecutable() from Carbon api, OSUtils.h
    // see http://developer.apple.com/documentation/Carbon/Reference/Memory_Manag_nt_Utilities/Reference/reference.html#//apple_ref/c/func/MakeDataExecutable
    void CodeAlloc::flushICache(void *start, size_t len) {
        MakeDataExecutable(start, len);
    }
#  endif

#elif defined NANOJIT_ARM && defined VMCFG_SYMBIAN
    void CodeAlloc::flushICache(void *ptr, size_t len) {
        uint32_t start = (uint32_t)ptr;
        uint32_t rangeEnd = start + len;
        User::IMB_Range((TAny*)start, (TAny*)rangeEnd);
    }

#elif defined AVMPLUS_SPARC
    // fixme: sync_instruction_memory is a solaris api, test for solaris not sparc
    void CodeAlloc::flushICache(void *start, size_t len) {
            sync_instruction_memory((char*)start, len);
    }

#elif defined(AVMPLUS_UNIX) && defined(NANOJIT_MIPS)
    void CodeAlloc::flushICache(void *start, size_t len) {
        // FIXME Use synci on MIPS32R2
        cacheflush((char *)start, len, BCACHE);
    }

#elif defined AVMPLUS_UNIX
    #ifdef ANDROID
    void CodeAlloc::flushICache(void *start, size_t len) {
        cacheflush((int)start, (int)start + len, 0);
    }
    #else
    // fixme: __clear_cache is a libgcc feature, test for libgcc or gcc
    void CodeAlloc::flushICache(void *start, size_t len) {
        __clear_cache((char*)start, (char*)start + len);
    }
    #endif
#endif // AVMPLUS_MAC && NANOJIT_PPC

    void CodeAlloc::addBlock(CodeList* &blocks, CodeList* b) {
        b->next = blocks;
        blocks = b;
    }

    CodeList* CodeAlloc::addMem(void *mem, size_t bytes) {
        CodeList* b = (CodeList*)mem;
        b->lower = 0;
        b->end = (NIns*) (uintptr_t(mem) + bytes - sizeofMinBlock);
        b->next = 0;
        b->isFree = true;

        // create a tiny terminator block, add to fragmented list, this way
        // all other blocks have a valid block at b->higher
        CodeList* terminator = b->higher;
        b->terminator = terminator;
        terminator->lower = b;
        terminator->end = 0; // this is how we identify the terminator
        terminator->isFree = false;
        terminator->isExec = false;
        terminator->terminator = 0;
        debug_only(sanity_check();)

        // add terminator to heapblocks list so we can track whole blocks
        addBlock(heapblocks, terminator);
        return b;
    }

    CodeList* CodeAlloc::getBlock(NIns* start, NIns* end) {
        CodeList* b = (CodeList*) (uintptr_t(start) - offsetof(CodeList, code));
        NanoAssert(b->end == end && b->next == 0); (void) end;
        return b;
    }

    CodeList* CodeAlloc::removeBlock(CodeList* &blocks) {
        CodeList* b = blocks;
        NanoAssert(b != NULL);
        blocks = b->next;
        b->next = 0;
        return b;
    }

    void CodeAlloc::add(CodeList* &blocks, NIns* start, NIns* end) {
        addBlock(blocks, getBlock(start, end));
    }

    /**
     * split a block by freeing the hole in the middle defined by [holeStart,holeEnd),
     * and adding the used prefix and suffix parts to the blocks CodeList.
     */
    void CodeAlloc::addRemainder(CodeList* &blocks, NIns* start, NIns* end, NIns* holeStart, NIns* holeEnd) {
        NanoAssert(start < end && start <= holeStart && holeStart <= holeEnd && holeEnd <= end);
        // shrink the hole by aligning holeStart forward and holeEnd backward
        holeStart = (NIns*) ((uintptr_t(holeStart) + sizeof(NIns*)-1) & ~(sizeof(NIns*)-1));
        holeEnd = (NIns*) (uintptr_t(holeEnd) & ~(sizeof(NIns*)-1));
        size_t minHole = minAllocSize;
        if (minHole < 2*sizeofMinBlock)
            minHole = 2*sizeofMinBlock;
        if (uintptr_t(holeEnd) - uintptr_t(holeStart) < minHole) {
            // the hole is too small to make a new free block and a new used block. just keep
            // the whole original block and don't free anything.
            add(blocks, start, end);
        } else if (holeStart == start && holeEnd == end) {
            // totally empty block.  free whole start-end range
            this->free(start, end);
        } else if (holeStart == start) {
            // hole is lower-aligned with start, so just need one new block
            // b1 b2
            CodeList* b1 = getBlock(start, end);
            CodeList* b2 = (CodeList*) (uintptr_t(holeEnd) - offsetof(CodeList, code));
            b2->terminator = b1->terminator;
            b2->isFree = false;
            b2->next = 0;
            b2->higher = b1->higher;
            b2->lower = b1;
            b2->higher->lower = b2;
            b1->higher = b2;
            debug_only(sanity_check();)
            this->free(b1->start(), b1->end);
            addBlock(blocks, b2);
        } else if (holeEnd == end) {
            // hole is right-aligned with end, just need one new block
            // todo
            NanoAssert(false);
        } else {
            // there's enough space left to split into three blocks (two new ones)
            CodeList* b1 = getBlock(start, end);
            CodeList* b2 = (CodeList*) holeStart;
            CodeList* b3 = (CodeList*) (uintptr_t(holeEnd) - offsetof(CodeList, code));
            b1->higher = b2;
            b2->lower = b1;
            b2->higher = b3;
            b2->isFree = false; // redundant, since we're about to free, but good hygiene
            b2->terminator = b1->terminator;
            b3->lower = b2;
            b3->end = end;
            b3->isFree = false;
            b3->higher->lower = b3;
            b3->terminator = b1->terminator;
            b2->next = 0;
            b3->next = 0;
            debug_only(sanity_check();)
            this->free(b2->start(), b2->end);
            addBlock(blocks, b3);
            addBlock(blocks, b1);
        }
    }

#ifdef PERFM
    // This method is used only for profiling purposes.
    // See CodegenLIR::emitMD() in Tamarin for an example.

    size_t CodeAlloc::size(const CodeList* blocks) {
        size_t size = 0;
        for (const CodeList* b = blocks; b != 0; b = b->next)
            size += int((uintptr_t)b->end - (uintptr_t)b);
        return size;
    }
#endif

    size_t CodeAlloc::size() {
        return totalAllocated;
    }

    // check that all block neighbors are correct
    #ifdef _DEBUG
    void CodeAlloc::sanity_check() {
        for (CodeList* hb = heapblocks; hb != 0; hb = hb->next) {
            NanoAssert(hb->higher == 0);
            for (CodeList* b = hb->lower; b != 0; b = b->lower) {
                NanoAssert(b->higher->lower == b);
            }
        }
        for (CodeList* avail = this->availblocks; avail; avail = avail->next) {
            NanoAssert(avail->isFree && avail->size() >= minAllocSize);
        }

        #if CROSS_CHECK_FREE_LIST
        for(CodeList* term = heapblocks; term; term = term->next) {
            for(CodeList* hb = term->lower; hb; hb = hb->lower) {
                if (hb->isFree && hb->size() >= minAllocSize) {
                    bool found_on_avail = false;
                    for (CodeList* avail = this->availblocks; !found_on_avail && avail; avail = avail->next) {
                        found_on_avail = avail == hb;
                    }

                    NanoAssert(found_on_avail);
                }
            }
        }
        for (CodeList* avail = this->availblocks; avail; avail = avail->next) {
            bool found_in_heapblocks = false;
            for(CodeList* term = heapblocks; !found_in_heapblocks && term; term = term->next) {
                for(CodeList* hb = term->lower; !found_in_heapblocks && hb; hb = hb->lower) {
                    found_in_heapblocks = hb == avail;
                }
            }
            NanoAssert(found_in_heapblocks);
        }
        #endif /* CROSS_CHECK_FREE_LIST */
    }
    #endif

    void CodeAlloc::markAllExec() {
        for (CodeList* hb = heapblocks; hb != NULL; hb = hb->next) {
            if (!hb->isExec) {
                hb->isExec = true;
                markCodeChunkExec(firstBlock(hb), bytesPerAlloc);
            }
        }
    }
}
#endif // FEATURE_NANOJIT
