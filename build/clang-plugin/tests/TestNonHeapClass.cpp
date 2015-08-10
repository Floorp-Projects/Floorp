#define MOZ_NONHEAP_CLASS __attribute__((annotate("moz_nonheap_class")))
#define MOZ_STACK_CLASS __attribute__((annotate("moz_stack_class")))
#include <stddef.h>

struct MOZ_NONHEAP_CLASS NonHeap {
  int i;
  void *operator new(size_t x) throw() { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_NONHEAP_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void misuseNonHeapClass(int len) {
  NonHeap valid;
  NonHeap alsoValid[2];
  static NonHeap validStatic;
  static NonHeap alsoValidStatic[2];

  gobble(&valid);
  gobble(&validStatic);
  gobble(&alsoValid[0]);

  gobble(new NonHeap); // expected-error {{variable of type 'NonHeap' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new NonHeap[10]); // expected-error {{variable of type 'NonHeap' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new TemplateClass<int>); // expected-error {{variable of type 'TemplateClass<int>' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
  gobble(len <= 5 ? &valid : new NonHeap); // expected-error {{variable of type 'NonHeap' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}

  char buffer[sizeof(NonHeap)];
  gobble(new (buffer) NonHeap);
}

NonHeap validStatic;
struct RandomClass {
  NonHeap nonstaticMember; // expected-note {{'RandomClass' is a non-heap type because member 'nonstaticMember' is a non-heap type 'NonHeap'}}
  static NonHeap staticMember;
};
struct MOZ_NONHEAP_CLASS RandomNonHeapClass {
  NonHeap nonstaticMember;
  static NonHeap staticMember;
};

struct BadInherit : NonHeap {}; // expected-note {{'BadInherit' is a non-heap type because it inherits from a non-heap type 'NonHeap'}}
struct MOZ_NONHEAP_CLASS GoodInherit : NonHeap {};

void useStuffWrongly() {
  gobble(new BadInherit); // expected-error {{variable of type 'BadInherit' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new RandomClass); // expected-error {{variable of type 'RandomClass' is not valid on the heap}} expected-note {{value incorrectly allocated on the heap}}
}

// Stack class overrides non-heap typees.
struct MOZ_STACK_CLASS StackClass {};
struct MOZ_NONHEAP_CLASS InferredStackClass : GoodInherit {
  NonHeap nonstaticMember;
  StackClass stackClass; // expected-note {{'InferredStackClass' is a stack type because member 'stackClass' is a stack type 'StackClass'}}
};

InferredStackClass global; // expected-error {{variable of type 'InferredStackClass' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
