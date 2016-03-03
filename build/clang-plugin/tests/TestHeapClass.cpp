#define MOZ_HEAP_CLASS __attribute__((annotate("moz_heap_class")))
#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

#include <stddef.h>

struct MOZ_HEAP_CLASS Heap {
  int i;
  Heap() {}
  MOZ_IMPLICIT Heap(int a) {}
  Heap(int a, int b) {}
  void *operator new(size_t x) throw() { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_HEAP_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void gobbleref(const Heap&) { }

void misuseHeapClass(int len) {
  Heap invalid; // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in an automatic variable}}
  Heap alsoInvalid[2]; // expected-error {{variable of type 'Heap [2]' only valid on the heap}} expected-note {{value incorrectly allocated in an automatic variable}} expected-note {{'Heap [2]' is a heap type because it is an array of heap type 'Heap'}}
  static Heap invalidStatic; // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a global variable}}
  static Heap alsoInvalidStatic[2]; // expected-error {{variable of type 'Heap [2]' only valid on the heap}} expected-note {{value incorrectly allocated in a global variable}} expected-note {{'Heap [2]' is a heap type because it is an array of heap type 'Heap'}}

  gobble(&invalid);
  gobble(&invalidStatic);
  gobble(&alsoInvalid[0]);

  gobbleref(Heap()); // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleref(Heap(10, 20)); // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleref(Heap(10)); // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleref(10); // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a temporary}}

  gobble(new Heap);
  gobble(new Heap[10]);
  gobble(new TemplateClass<int>);
  gobble(len <= 5 ? &invalid : new Heap);

  char buffer[sizeof(Heap)];
  gobble(new (buffer) Heap);
}

Heap invalidStatic; // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a global variable}}
struct RandomClass {
  Heap nonstaticMember; // expected-note {{'RandomClass' is a heap type because member 'nonstaticMember' is a heap type 'Heap'}}
  static Heap staticMember; // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a global variable}}
};
struct MOZ_HEAP_CLASS RandomHeapClass {
  Heap nonstaticMember;
  static Heap staticMember; // expected-error {{variable of type 'Heap' only valid on the heap}} expected-note {{value incorrectly allocated in a global variable}}
};

struct BadInherit : Heap {}; // expected-note {{'BadInherit' is a heap type because it inherits from a heap type 'Heap'}}
struct MOZ_HEAP_CLASS GoodInherit : Heap {};

void useStuffWrongly() {
  BadInherit i; // expected-error {{variable of type 'BadInherit' only valid on the heap}} expected-note {{value incorrectly allocated in an automatic variable}}
  RandomClass r; // expected-error {{variable of type 'RandomClass' only valid on the heap}} expected-note {{value incorrectly allocated in an automatic variable}}
}
