#include "gdb-tests.h"

#include "jit/ExecutableAllocator.h"
#include "vm/JSContext.h"

FRAGMENT(ExecutableAllocator, empty) {
  using namespace js::jit;
  ExecutableAllocator execAlloc;

  breakpoint();

  use(execAlloc);
}

FRAGMENT(ExecutableAllocator, onepool) {
  using namespace js::jit;
  ExecutablePool* pool = nullptr;
  ExecutableAllocator execAlloc;
  execAlloc.alloc(cx, 16 * 1024, &pool, CodeKind::Baseline);

  breakpoint();

  use(pool);
  use(execAlloc);
}

FRAGMENT(ExecutableAllocator, twopools) {
  using namespace js::jit;
  const size_t INIT_ALLOC_SIZE = 16 * 1024;
  const size_t ALLOC_SIZE = 32 * 1024;
  ExecutablePool* init = nullptr;
  ExecutablePool* pool = nullptr;
  ExecutableAllocator execAlloc;
  size_t allocated = 0;

  execAlloc.alloc(cx, INIT_ALLOC_SIZE, &init, CodeKind::Baseline);

  do {  // Keep allocating until we get a second pool.
    execAlloc.alloc(cx, ALLOC_SIZE, &pool, CodeKind::Ion);
    allocated += ALLOC_SIZE;
  } while (pool == init);

  breakpoint();

  use(execAlloc);
  init->release(INIT_ALLOC_SIZE, CodeKind::Baseline);
  init->release(allocated - ALLOC_SIZE, CodeKind::Ion);
  pool->release(ALLOC_SIZE, CodeKind::Ion);
}
