class Foo {
  Foo(Foo&& f);
};

class Bar {
  explicit Bar(Bar&& f); // expected-error {{Move constructors may not be marked explicit}}
};

class Baz {
  template<typename T>
  explicit Baz(T&& f) {};
};

class Quxx {
  Quxx();
  Quxx(Quxx& q) = delete;
  template<typename T>
  explicit Quxx(T&& f) {};
};

void f() {
  // Move a quxx into a quxx! (This speciailizes Quxx's constructor to look like
  // a move constructor - to make sure it doesn't trigger)
  Quxx(Quxx());
}
