/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9.1 code, released
 * June 30, 2009.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Igor Bukanov <igor@mir2.org>
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

#ifndef jsgchunk_h__
#define jsgchunk_h__

#include "jsprvtd.h"
#include "jsutil.h"

namespace js {

#if defined(WINCE) && !defined(MOZ_MEMORY_WINCE6)
const size_t GC_CHUNK_SHIFT = 21;
#else
const size_t GC_CHUNK_SHIFT = 20;
#endif

const size_t GC_CHUNK_SIZE = size_t(1) << GC_CHUNK_SHIFT;
const size_t GC_CHUNK_MASK = GC_CHUNK_SIZE - 1;

JS_FRIEND_API(void *)
AllocGCChunk();

JS_FRIEND_API(void)
FreeGCChunk(void *p);

class GCChunkAllocator {
  public:
    GCChunkAllocator() {}
    
    void *alloc() {
        void *chunk = doAlloc();
        JS_ASSERT(!(reinterpret_cast<jsuword>(chunk) & GC_CHUNK_MASK));
        return chunk;
    }

    void free(void *chunk) {
        JS_ASSERT(chunk);
        JS_ASSERT(!(reinterpret_cast<jsuword>(chunk) & GC_CHUNK_MASK));
        doFree(chunk);
    }
    
  private:
    virtual void *doAlloc() {
        return AllocGCChunk();
    }
    
    virtual void doFree(void *chunk) {
        FreeGCChunk(chunk);
    }

    /* No copy or assignment semantics. */
    GCChunkAllocator(const GCChunkAllocator &);
    void operator=(const GCChunkAllocator &);
};

extern GCChunkAllocator defaultGCChunkAllocator;

}

#endif /* jsgchunk_h__ */
