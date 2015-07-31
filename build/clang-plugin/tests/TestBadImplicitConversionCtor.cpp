#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

struct Foo {
  Foo(int); // expected-error {{bad implicit conversion constructor for 'Foo'}} expected-note {{consider adding the explicit keyword to the constructor}}
  Foo(int, char=0); // expected-error {{bad implicit conversion constructor for 'Foo'}} expected-note {{consider adding the explicit keyword to the constructor}}
  Foo(...); // expected-error {{bad implicit conversion constructor for 'Foo'}} expected-note {{consider adding the explicit keyword to the constructor}}
  template<class T>
  Foo(float); // expected-error {{bad implicit conversion constructor for 'Foo'}} expected-note {{consider adding the explicit keyword to the constructor}}
  Foo(int, unsigned);
  Foo(Foo&);
  Foo(const Foo&);
  Foo(volatile Foo&);
  Foo(const volatile Foo&);
  Foo(Foo&&);
  Foo(const Foo&&);
  Foo(volatile Foo&&);
  Foo(const volatile Foo&&);
};

struct Bar {
  explicit Bar(int);
  explicit Bar(int, char=0);
  explicit Bar(...);
};

struct Baz {
  MOZ_IMPLICIT Baz(int);
  MOZ_IMPLICIT Baz(int, char=0);
  MOZ_IMPLICIT Baz(...);
};

struct Barn {
  Barn(int) = delete;
  Barn(int, char=0) = delete;
  Barn(...) = delete;
};

struct Abstract {
  Abstract(int);
  Abstract(int, char=0);
  Abstract(...);
  virtual void f() = 0;
};

template<class T>
struct Template {
  Template(int); // expected-error {{bad implicit conversion constructor for 'Template'}} expected-note {{consider adding the explicit keyword to the constructor}}
  template<class U>
  Template(float); // expected-error {{bad implicit conversion constructor for 'Template'}} expected-note {{consider adding the explicit keyword to the constructor}}
};
