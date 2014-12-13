#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

struct Foo {
  Foo(int); // expected-error {{bad implicit conversion constructor for 'Foo'}}
  Foo(int, char=0); // expected-error {{bad implicit conversion constructor for 'Foo'}}
  Foo(...); // expected-error {{bad implicit conversion constructor for 'Foo'}}
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
