#define MOZ_TEMPORARY_CLASS __attribute__((annotate("moz_temporary_class")))
#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

#include <stddef.h>

struct MOZ_TEMPORARY_CLASS Temporary {
  int i;
  Temporary() {}
  MOZ_IMPLICIT Temporary(int a) {}
  Temporary(int a, int b) {}
  void *operator new(size_t x) throw() { return 0; }
  void *operator new(size_t blah, char *buffer) { return buffer; }
};

template <class T>
struct MOZ_TEMPORARY_CLASS TemplateClass {
  T i;
};

void gobble(void *) { }

void gobbleref(const Temporary&) { }

template <class T>
void gobbleanyref(const T&) { }

void misuseNonTemporaryClass(int len) {
  // All of these should error.
  Temporary invalid; // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated in an automatic variable}}
  Temporary alsoInvalid[2]; // expected-error-re {{variable of type 'Temporary{{ ?}}[2]' is only valid as a temporary}} expected-note {{value incorrectly allocated in an automatic variable}} expected-note-re {{'Temporary{{ ?}}[2]' is a temporary type because it is an array of temporary type 'Temporary'}}
  static Temporary invalidStatic; // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated in a global variable}}
  static Temporary alsoInvalidStatic[2]; // expected-error-re {{variable of type 'Temporary{{ ?}}[2]' is only valid as a temporary}} expected-note {{value incorrectly allocated in a global variable}} expected-note-re {{'Temporary{{ ?}}[2]' is a temporary type because it is an array of temporary type 'Temporary'}}

  gobble(&invalid);
  gobble(&invalidStatic);
  gobble(&alsoInvalid[0]);

  // All of these should be fine.
  gobbleref(Temporary());
  gobbleref(Temporary(10, 20));
  gobbleref(Temporary(10));
  gobbleref(10);
  gobbleanyref(TemplateClass<int>());

  // All of these should error.
  gobble(new Temporary); // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new Temporary[10]); // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated on the heap}}
  gobble(new TemplateClass<int>); // expected-error {{variable of type 'TemplateClass<int>' is only valid as a temporary}} expected-note {{value incorrectly allocated on the heap}}
  gobble(len <= 5 ? &invalid : new Temporary); // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated on the heap}}

  // Placement new is odd, but okay.
  char buffer[sizeof(Temporary)];
  gobble(new (buffer) Temporary);
}

void defaultArg(const Temporary& arg = Temporary()) { // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated in an automatic variable}}
}

// Can't be a global, this should error.
Temporary invalidStatic; // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated in a global variable}}

struct RandomClass {
  Temporary nonstaticMember; // This is okay if RandomClass is only used as a temporary.
  static Temporary staticMember; // expected-error {{variable of type 'Temporary' is only valid as a temporary}} expected-note {{value incorrectly allocated in a global variable}}
};

struct BadInherit : Temporary {};

void useStuffWrongly() {
  gobbleanyref(BadInherit());
  gobbleanyref(RandomClass());
}
