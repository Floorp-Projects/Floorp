#define MOZ_STACK_CLASS __attribute__((annotate("moz_stack_class")))
#include <stddef.h>

struct MOZ_STACK_CLASS Stack {
  int i;
  void *operator new(size_t x) { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_STACK_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void misuseStackClass(int len) {
  Stack valid;
  Stack alsoValid[2];
  static Stack notValid; // expected-error {{variable of type 'Stack' only valid on the stack}}
  static Stack alsoNotValid[2]; // expected-error {{variable of type 'Stack [2]' only valid on the stack}}

  gobble(&valid);
  gobble(&notValid);
  gobble(&alsoValid[0]);

  gobble(new Stack); // expected-error {{variable of type 'Stack' only valid on the stack}}
  gobble(new Stack[10]); // expected-error {{variable of type 'Stack' only valid on the stack}}
  gobble(new TemplateClass<int>); // expected-error {{variable of type 'TemplateClass<int>' only valid on the stack}}
  gobble(len <= 5 ? &valid : new Stack); // expected-error {{variable of type 'Stack' only valid on the stack}}

  char buffer[sizeof(Stack)];
  gobble(new (buffer) Stack);
}

Stack notValid; // expected-error {{variable of type 'Stack' only valid on the stack}}
struct RandomClass {
  Stack nonstaticMember; // expected-error {{member of type 'Stack' in class 'RandomClass' that is not a stack class}}
  static Stack staticMember; // expected-error {{variable of type 'Stack' only valid on the stack}}
};
struct MOZ_STACK_CLASS RandomStackClass {
  Stack nonstaticMember;
  static Stack staticMember; // expected-error {{variable of type 'Stack' only valid on the stack}}
};

struct BadInherit : Stack {}; // expected-error {{'BadInherit' inherits from a stack class 'Stack'}}
struct MOZ_STACK_CLASS GoodInherit : Stack {};
