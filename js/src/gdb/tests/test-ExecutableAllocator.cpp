#include "gdb-tests.h"
#include "jsapi.h"

#include "jit/ExecutableAllocator.h"
#include "vm/JSContext.h"

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
    execAlloc.alloc(cx, 16 * 1024, &pool, CodeKind::Baseline);

    breakpoint();

    (void) pool;
    (void) execAlloc;
}

FRAGMENT(ExecutableAllocator, twopools) {
    using namespace js::jit;
    ExecutablePool* init = nullptr;
    ExecutablePool* pool = nullptr;
    ExecutableAllocator execAlloc(cx->runtime());

    execAlloc.alloc(cx, 16 * 1024, &init, CodeKind::Baseline);

    do { // Keep allocating until we get a second pool.
        execAlloc.alloc(cx, 32 * 1024, &pool, CodeKind::Ion);
    } while (pool == init);

    breakpoint();

    (void) execAlloc;
}
