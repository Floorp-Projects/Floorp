#define MOZ_NONHEAP_CLASS __attribute__((annotate("moz_nonheap_class")))
#ifndef MOZ_HEAP_ALLOCATOR
#define MOZ_HEAP_ALLOCATOR \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wgcc-compat\"") \
  __attribute__((annotate("moz_heap_allocator"))) \
  _Pragma("GCC diagnostic pop")
#endif

#include <stdlib.h>
#include <memory>

struct MOZ_NONHEAP_CLASS X {
};

void *operator new(size_t x, int qual) MOZ_HEAP_ALLOCATOR {
  return ::operator new(x);
}

template <typename T>
T *customAlloc() MOZ_HEAP_ALLOCATOR {
  T *arg =  static_cast<T*>(malloc(sizeof(T)));
  return new (arg) T();
}

void misuseX() {
  X *foo = customAlloc<X>(); // expected-error {{variable of type 'X' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
  X *foo2 = new (100) X(); // expected-error {{variable of type 'X' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
}
