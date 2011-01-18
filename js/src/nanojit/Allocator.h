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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef __nanojit_Allocator__
#define __nanojit_Allocator__

namespace nanojit
{
    /**
     * Allocator is a bump-pointer allocator with an SPI for getting more
     * memory from embedder-implemented allocator, such as malloc()/free().
     *
     * alloc() never returns NULL.  The implementation of allocChunk()
     * is expected to perform a longjmp or exception when an allocation can't
     * proceed.  fallibleAlloc() (and fallibleAllocChunk()) may return NULL.
     * They should be used for large allocations whose failure can be handled
     * without too much difficulty.
     */
    class Allocator {
    public:
        Allocator();
        ~Allocator();

        // Usable space in the minimum chunk size;  there are also a few bytes
        // used for administration.
        static const size_t MIN_CHUNK_SZB = 2000;

        void reset();

        /** alloc memory, never return null. */
        void* alloc(size_t nbytes) {
            void* p;
            nbytes = (nbytes + 7) & ~7; // round up
            if (current_top + nbytes <= current_limit) {
                p = current_top;
                current_top += nbytes;
            } else {
                p = allocSlow(nbytes, /* fallible = */false);
                NanoAssert(p);
            }
            return p;
        }

        /** alloc memory, maybe return null. */
        void* fallibleAlloc(size_t nbytes) {
            void* p;
            nbytes = (nbytes + 7) & ~7; // round up
            if (current_top + nbytes <= current_limit) {
                p = current_top;
                current_top += nbytes;
            } else {
                p = allocSlow(nbytes, /* fallible = */true);
            }
            return p;
        }

    protected:
        void* allocSlow(size_t nbytes, bool fallible = false);
        bool fill(size_t minbytes, bool fallible);

        class Chunk {
        public:
            Chunk* prev;
            int64_t data[1]; // int64_t forces 8-byte alignment.
        };

        Chunk* current_chunk;
        char* current_top;
        char* current_limit;

        // allocator SPI

        /** allocate another block from a host provided allocator */
        void* allocChunk(size_t nbytes, bool fallible);

        /** free back to the same allocator */
        void freeChunk(void*);

        /** hook for post-reset action. */
        void postReset();
    };
}

/** global new overload enabling this pattern:  new (allocator) T(...) */
inline void* operator new(size_t size, nanojit::Allocator &a) {
    return a.alloc(size);
}

/** global new overload enabling this pattern:  new (allocator) T(...) */
inline void* operator new(size_t size, nanojit::Allocator *a) {
    return a->alloc(size);
}

/** global new[] overload enabling this pattern: new (allocator) T[] */
inline void* operator new[](size_t size, nanojit::Allocator& a) {
    return a.alloc(size);
}

/** global new[] overload enabling this pattern: new (allocator) T[] */
inline void* operator new[](size_t size, nanojit::Allocator* a) {
    return a->alloc(size);
}

#endif // __nanojit_Allocator__
