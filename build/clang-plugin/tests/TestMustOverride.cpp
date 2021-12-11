#define MOZ_MUST_OVERRIDE __attribute__((annotate("moz_must_override")))
// Ignore warnings not related to static analysis here
#pragma GCC diagnostic ignored "-Woverloaded-virtual"

struct S {
  virtual void f() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  virtual void g() MOZ_MUST_OVERRIDE;
  virtual void h() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
};
struct C : S { // expected-error {{'C' must override 'f'}} expected-error {{'C' must override 'h'}}
  virtual void g() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  virtual void h(int);
  void q() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
};
struct D : C { // expected-error {{'D' must override 'g'}} expected-error {{'D' must override 'q'}}
  virtual void f();
};

struct Base {
  virtual void VirtMethod() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  void NonVirtMethod() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  static void StaticMethod() MOZ_MUST_OVERRIDE;
};

struct DoesNotPropagate : Base {
  virtual void VirtMethod();
  void NonVirtMethod();
  static void StaticMethod();
};

struct Final : DoesNotPropagate { };

struct Propagates : Base {
  virtual void VirtMethod() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  void NonVirtMethod() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  static void StaticMethod() MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
};

struct FailsFinal : Propagates { }; // expected-error {{'FailsFinal' must override 'VirtMethod'}} expected-error {{'FailsFinal' must override 'NonVirtMethod'}} expected-error {{'FailsFinal' must override 'StaticMethod'}}

struct WrongOverload : Base { // expected-error {{'WrongOverload' must override 'VirtMethod'}} expected-error {{'WrongOverload' must override 'NonVirtMethod'}}
  virtual void VirtMethod() const;
  void NonVirtMethod(int param);
  static void StaticMethod();
};

namespace A { namespace B { namespace C {
  struct Param {};
  struct Base {
    void f(Param p) MOZ_MUST_OVERRIDE; // expected-note {{function to override is here}}
  };
}}}

struct Param {};

struct Derived : A::B::C::Base {
  typedef A::B::C::Param Typedef;
  void f(Typedef t);
};

struct BadDerived : A::B::C::Base { // expected-error {{'BadDerived' must override 'f'}}
  void f(Param p);
};
