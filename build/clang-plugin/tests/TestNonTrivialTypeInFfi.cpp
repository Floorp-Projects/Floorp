// clang warns for some of these on its own, but we're not testing that, plus
// some of them (TrivialT<int>) is a false positive (clang doesn't realize the
// type is fully specialized below).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

struct Opaque;
struct Trivial {
  int foo;
  char bar;
  Opaque* baz;
};

template <typename T>
struct TrivialT {
  int foo;
  char bar;
  T* baz;
};

struct NonTrivial {
  ~NonTrivial() {
  }

  Opaque* ptr;
};

template <typename T>
struct NonTrivialT {
  ~NonTrivialT() {
    delete ptr;
  }

  T* ptr;
};

struct TransitivelyNonTrivial {
  NonTrivial nontrivial;
};

extern "C" void Foo();
extern "C" Trivial Foo1();
extern "C" NonTrivial Foo2(); // expected-error {{Type 'NonTrivial' must not be used as return type of extern "C" function}} expected-note {{Please consider using a pointer or reference instead}}
extern "C" NonTrivialT<int> Foo3(); // expected-error {{Type 'NonTrivialT<int>' must not be used as return type of extern "C" function}} expected-note {{Please consider using a pointer or reference, or explicitly instantiating the template instead}}
extern "C" NonTrivialT<float> Foo4(); // expected-error {{Type 'NonTrivialT<float>' must not be used as return type of extern "C" function}} expected-note {{Please consider using a pointer or reference, or explicitly instantiating the template instead}}

extern "C" NonTrivial* Foo5();

extern "C" TrivialT<int> Foo6();
extern "C" TrivialT<float> Foo7(); // expected-error {{Type 'TrivialT<float>' must not be used as return type of extern "C" function}} expected-note {{Please consider using a pointer or reference, or explicitly instantiating the template instead}}
extern "C" Trivial* Foo8();

extern "C" void Foo9(Trivial);
extern "C" void Foo10(NonTrivial); // expected-error {{Type 'NonTrivial' must not be used as parameter to extern "C" function}} expected-note {{Please consider using a pointer or reference instead}}
extern "C" void Foo11(NonTrivial*);
extern "C" void Foo12(NonTrivialT<int>); // expected-error {{Type 'NonTrivialT<int>' must not be used as parameter to extern "C" function}} expected-note {{Please consider using a pointer or reference, or explicitly instantiating the template instead}}
extern "C" void Foo13(TrivialT<int>);
extern "C" void Foo14(TrivialT<float>); // expected-error {{Type 'TrivialT<float>' must not be used as parameter to extern "C" function}} expected-note {{Please consider using a pointer or reference, or explicitly instantiating the template instead}}

extern "C" TransitivelyNonTrivial Foo15(); // expected-error {{Type 'TransitivelyNonTrivial' must not be used as return type of extern "C" function}} expected-note {{Please consider using a pointer or reference instead}}
extern "C" void Foo16(TransitivelyNonTrivial); // expected-error {{Type 'TransitivelyNonTrivial' must not be used as parameter to extern "C" function}} expected-note {{Please consider using a pointer or reference instead}}

template struct TrivialT<int>;

#pragma GCC diagnostic pop
