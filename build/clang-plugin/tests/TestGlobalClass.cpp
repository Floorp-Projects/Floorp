#define MOZ_GLOBAL_CLASS __attribute__((annotate("moz_global_class")))
#include <stddef.h>

struct MOZ_GLOBAL_CLASS Global {
  int i;
  void *operator new(size_t x) throw() { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_GLOBAL_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void misuseGlobalClass(int len) {
  Global notValid; // expected-error {{variable of type 'Global' only valid as global}}
  Global alsoNotValid[2]; // expected-error {{variable of type 'Global [2]' only valid as global}}
  static Global valid; // expected-error {{variable of type 'Global' only valid as global}}
  static Global alsoValid[2]; // expected-error {{variable of type 'Global [2]' only valid as global}}

  gobble(&valid);
  gobble(&notValid);
  gobble(&alsoValid[0]);

  gobble(new Global); // expected-error {{variable of type 'Global' only valid as global}}
  gobble(new Global[10]); // expected-error {{variable of type 'Global' only valid as global}}
  gobble(new TemplateClass<int>); // expected-error {{variable of type 'TemplateClass<int>' only valid as global}}
  gobble(len <= 5 ? &valid : new Global); // expected-error {{variable of type 'Global' only valid as global}}

  char buffer[sizeof(Global)];
  gobble(new (buffer) Global); // expected-error {{variable of type 'Global' only valid as global}}
}

Global valid;
struct RandomClass {
  Global nonstaticMember; // expected-note {{'RandomClass' is a global class because member 'nonstaticMember' is a global class 'Global'}}
  static Global staticMember;
};
struct MOZ_GLOBAL_CLASS RandomGlobalClass {
  Global nonstaticMember;
  static Global staticMember;
};

struct BadInherit : Global {}; // expected-note {{'BadInherit' is a global class because it inherits from a global class 'Global'}}
struct MOZ_GLOBAL_CLASS GoodInherit : Global {};

void misuseGlobalClassEvenMore(int len) {
  BadInherit moreInvalid; // expected-error {{variable of type 'BadInherit' only valid as global}}
  RandomClass evenMoreInvalid; // expected-error {{variable of type 'RandomClass' only valid as global}}
}
