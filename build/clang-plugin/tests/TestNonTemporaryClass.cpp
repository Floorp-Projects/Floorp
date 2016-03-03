#define MOZ_NON_TEMPORARY_CLASS __attribute__((annotate("moz_non_temporary_class")))
#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

#include <stddef.h>

struct MOZ_NON_TEMPORARY_CLASS NonTemporary {
  int i;
  NonTemporary() {}
  MOZ_IMPLICIT NonTemporary(int a) {}
  NonTemporary(int a, int b) {}
  void *operator new(size_t x) throw() { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_NON_TEMPORARY_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void gobbleref(const NonTemporary&) { }

template <class T>
void gobbleanyref(const T&) { }

void misuseNonTemporaryClass(int len) {
  NonTemporary invalid;
  NonTemporary alsoInvalid[2];
  static NonTemporary invalidStatic;
  static NonTemporary alsoInvalidStatic[2];

  gobble(&invalid);
  gobble(&invalidStatic);
  gobble(&alsoInvalid[0]);

  gobbleref(NonTemporary()); // expected-error {{variable of type 'NonTemporary' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleref(NonTemporary(10, 20)); // expected-error {{variable of type 'NonTemporary' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleref(NonTemporary(10)); // expected-error {{variable of type 'NonTemporary' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleref(10); // expected-error {{variable of type 'NonTemporary' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleanyref(TemplateClass<int>()); // expected-error {{variable of type 'TemplateClass<int>' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}

  gobble(new NonTemporary);
  gobble(new NonTemporary[10]);
  gobble(new TemplateClass<int>);
  gobble(len <= 5 ? &invalid : new NonTemporary);

  char buffer[sizeof(NonTemporary)];
  gobble(new (buffer) NonTemporary);
}

void defaultArg(const NonTemporary& arg = NonTemporary()) {
}

NonTemporary invalidStatic;
struct RandomClass {
  NonTemporary nonstaticMember; // expected-note {{'RandomClass' is a non-temporary type because member 'nonstaticMember' is a non-temporary type 'NonTemporary'}}
  static NonTemporary staticMember;
};
struct MOZ_NON_TEMPORARY_CLASS RandomNonTemporaryClass {
  NonTemporary nonstaticMember;
  static NonTemporary staticMember;
};

struct BadInherit : NonTemporary {}; // expected-note {{'BadInherit' is a non-temporary type because it inherits from a non-temporary type 'NonTemporary'}}

void useStuffWrongly() {
  gobbleanyref(BadInherit()); // expected-error {{variable of type 'BadInherit' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
  gobbleanyref(RandomClass()); // expected-error {{variable of type 'RandomClass' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
}
