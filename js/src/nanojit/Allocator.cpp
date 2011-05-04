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

#ifdef FEATURE_NANOJIT

namespace nanojit
{
    Allocator::Allocator()
        : current_chunk(NULL)
        , current_top(NULL)
        , current_limit(NULL)
    { }

    Allocator::~Allocator()
    {
        reset();
    }

    void Allocator::reset()
    {
        Chunk *c = current_chunk;
        while (c) {
            Chunk *prev = c->prev;
            freeChunk(c);
            c = prev;
        }
        current_chunk = NULL;
        current_top = NULL;
        current_limit = NULL;
        postReset();
    }

    void* Allocator::allocSlow(size_t nbytes, bool fallible)
    {
        NanoAssert((nbytes & 7) == 0);
        if (fill(nbytes, fallible)) {
            NanoAssert(current_top + nbytes <= current_limit);
            void* p = current_top;
            current_top += nbytes;
            return p;
        }
        return NULL;
    }

    bool Allocator::fill(size_t nbytes, bool fallible)
    {
        if (nbytes < MIN_CHUNK_SZB)
            nbytes = MIN_CHUNK_SZB;
        size_t chunkbytes = sizeof(Chunk) + nbytes - sizeof(int64_t);
        void* mem = allocChunk(chunkbytes, fallible);
        if (mem) {
            Chunk* chunk = (Chunk*) mem;
            chunk->prev = current_chunk;
            chunk->size = chunkbytes;
            current_chunk = chunk;
            current_top = (char*)chunk->data;
            current_limit = (char*)mem + chunkbytes;
            return true;
        } else {
            NanoAssert(fallible);
            return false;
        }
    }

    size_t Allocator::getBytesAllocated()
    {
        size_t n = 0;
        Chunk *c = current_chunk;
        while (c) {
            n += c->size;
            c = c->prev;
        }
        return n;
    }
}

#endif // FEATURE_NANOJIT
