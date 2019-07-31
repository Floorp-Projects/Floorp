#define MOZ_STATIC_LOCAL_CLASS __attribute__((annotate("moz_static_local_class")))
#include <stddef.h>

struct MOZ_STATIC_LOCAL_CLASS StaticLocal {
  int i;
  void *operator new(size_t x) throw() { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_STATIC_LOCAL_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void misuseStaticLocalClass(int len) {
  StaticLocal notValid; // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated in an automatic variable}}
  StaticLocal alsoNotValid[2]; // expected-error {{variable of type 'StaticLocal [2]' is only valid as a static local}} expected-note {{'StaticLocal [2]' is a static-local type because it is an array of static-local type 'StaticLocal'}} expected-note {{value incorrectly allocated in an automatic variable}}
  static StaticLocal valid;
  static StaticLocal alsoValid[2];

  gobble(&notValid);
  gobble(&valid);
  gobble(&alsoValid[0]);

  gobble(new StaticLocal); // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new StaticLocal[10]); // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new TemplateClass<int>); // expected-error {{variable of type 'TemplateClass<int>' is only valid as a static local}} expected-note {{value incorrectly allocated on the heap}}
  gobble(len <= 5 ? &valid : new StaticLocal); // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated on the heap}}

  char buffer[sizeof(StaticLocal)];
  gobble(new (buffer) StaticLocal);
}

StaticLocal notValid; // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated in a global variable}}

struct RandomClass {
  StaticLocal nonstaticMember; // expected-note {{'RandomClass' is a static-local type because member 'nonstaticMember' is a static-local type 'StaticLocal'}}
  static StaticLocal staticMember; // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated in a global variable}}
};

struct MOZ_STATIC_LOCAL_CLASS RandomStaticLocalClass {
  StaticLocal nonstaticMember;
  static StaticLocal staticMember; // expected-error {{variable of type 'StaticLocal' is only valid as a static local}} expected-note {{value incorrectly allocated in a global variable}}
};

struct BadInherit : StaticLocal {}; // expected-note {{'BadInherit' is a static-local type because it inherits from a static-local type 'StaticLocal'}}
struct MOZ_STATIC_LOCAL_CLASS GoodInherit : StaticLocal {};

void misuseStaticLocalClassEvenMore(int len) {
  BadInherit moreInvalid; // expected-error {{variable of type 'BadInherit' is only valid as a static local}} expected-note {{value incorrectly allocated in an automatic variable}}
  RandomClass evenMoreInvalid; // expected-error {{variable of type 'RandomClass' is only valid as a static local}} expected-note {{value incorrectly allocated in an automatic variable}}
}
