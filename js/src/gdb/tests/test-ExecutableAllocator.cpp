#include "gdb-tests.h"
#include "jsapi.h"

#include "jscntxt.h"

#include "jit/ExecutableAllocator.h"

FRAGMENT(ExecutableAllocator, empty) {
    using namespace js::jit;
    ExecutableAllocator execAlloc(cx->runtime());

    breakpoint();

    (void) execAlloc;
}

FRAGMENT(ExecutableAllocator, onepool) {
    using namespace js::jit;
    ExecutablePool* pool = nullptr;
    ExecutableAllocator execAlloc(cx->runtime());
    execAlloc.alloc(16 * 1024, &pool, BASELINE_CODE);

    breakpoint();

    (void) pool;
    (void) execAlloc;
}

FRAGMENT(ExecutableAllocator, twopools) {
    using namespace js::jit;
    ExecutablePool* init = nullptr;
    ExecutablePool* pool = nullptr;
    ExecutableAllocator execAlloc(cx->runtime());

    execAlloc.alloc(16 * 1024, &init, BASELINE_CODE);

    do { // Keep allocating until we get a second pool.
        execAlloc.alloc(32 * 1024, &pool, ION_CODE);
    } while (pool == init);

    breakpoint();

    (void) execAlloc;
}
