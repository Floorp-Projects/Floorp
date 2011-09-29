#include "LifoAlloc.h"

#include <new>

using namespace js;

namespace js {
namespace detail {

BumpChunk *
BumpChunk::new_(size_t chunkSize)
{
    JS_ASSERT(RoundUpPow2(chunkSize) == chunkSize);
    void *mem = js_malloc(chunkSize);
    if (!mem)
        return NULL;
    BumpChunk *result = new (mem) BumpChunk(chunkSize - sizeof(BumpChunk));

    /* 
     * We assume that the alignment of sAlign is less than that of
     * the underlying memory allocator -- creating a new BumpChunk should
     * always satisfy the sAlign alignment constraint.
     */
    JS_ASSERT(AlignPtr(result->bump) == result->bump);
    return result;
}

void *
BumpChunk::tryAllocUnaligned(size_t n)
{
    char *oldBump = bump;
    char *newBump = bump + n;
    if (newBump > limit)
        return NULL;

    setBump(newBump);
    return oldBump;
}

} /* namespace detail */
} /* namespace js */

void
LifoAlloc::freeAll()
{
    while (first) {
        BumpChunk *victim = first;
        first = first->next();
        BumpChunk::delete_(victim);
    }
    first = latest = NULL;
}

void
LifoAlloc::freeUnused()
{
    /* Don't free anything if we have outstanding marks. */
    if (markCount || !first)
        return; 

    JS_ASSERT(first && latest);

    /* Rewind through any unused chunks. */
    if (!latest->used()) {
        BumpChunk *lastUsed = NULL;
        for (BumpChunk *it = first; it != latest; it = it->next()) {
            if (it->used())
                lastUsed = it;
        }
        if (!lastUsed) {
            freeAll();
            return;
        }
        latest = lastUsed;
    }

    /* Free all chunks after |latest|. */
    size_t freed = 0;
    for (BumpChunk *victim = latest->next(); victim; victim = victim->next()) {
        BumpChunk::delete_(victim);
        freed++;
    }
}

LifoAlloc::BumpChunk *
LifoAlloc::getOrCreateChunk(size_t n)
{
    if (first) {
        /* Look for existing, unused BumpChunks to satisfy the request. */
        while (latest->next()) {
            latest = latest->next();
            latest->resetBump(); /* This was an unused BumpChunk on the chain. */
            if (latest->canAlloc(n))
                return latest;
        }
    }

    size_t defaultChunkFreeSpace = defaultChunkSize_ - sizeof(BumpChunk);
    size_t chunkSize = n > defaultChunkFreeSpace
                       ? RoundUpPow2(n + sizeof(BumpChunk))
                       : defaultChunkSize_;

    /* If we get here, we couldn't find an existing BumpChunk to fill the request. */
    BumpChunk *newChunk = BumpChunk::new_(chunkSize);
    if (!newChunk)
        return NULL;
    if (!first) {
        latest = first = newChunk;
    } else {
        JS_ASSERT(latest && !latest->next());
        latest->setNext(newChunk);
        latest = newChunk;
    }
    return newChunk;
}

void *
LifoAlloc::allocUnaligned(size_t n)
{
    void *result;
    if (latest && (result = latest->tryAllocUnaligned(n)))
        return result;

    return alloc(n);
}

void *
LifoAlloc::reallocUnaligned(void *origPtr, size_t origSize, size_t incr)
{
    JS_ASSERT(first && latest);

    /*
     * Maybe we can grow the latest allocation in a BumpChunk.
     *
     * Note: we could also realloc the whole BumpChunk in the !canAlloc
     * case, but this should not be a frequently hit case.
     */
    if (latest
        && origPtr == (char *) latest->mark() - origSize
        && latest->canAllocUnaligned(incr)) {
        JS_ALWAYS_TRUE(allocUnaligned(incr));
        return origPtr;
    }

    /* Otherwise, memcpy. */
    size_t newSize = origSize + incr;
    void *newPtr = allocUnaligned(newSize);
    return newPtr ? memcpy(newPtr, origPtr, origSize) : NULL;
}
