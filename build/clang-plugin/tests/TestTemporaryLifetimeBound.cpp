#define MOZ_LIFETIME_BOUND __attribute__((annotate("moz_lifetime_bound")))

struct Foo {};

struct Bar {
  MOZ_LIFETIME_BOUND const Foo &AsFoo() const; // expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}}
  MOZ_LIFETIME_BOUND operator const Foo &() const; // expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}} expected-note {{member function declared here}}
};

Bar MakeBar() { return Bar(); }

Bar testReturnsInstance_Constructed() { return Bar(); }

const Foo &testReturnsReference_Static() {
  static constexpr auto bar = Bar{};
  return bar;
}

/* TODO This is bad as well... but not related to a temporary.
const Foo& testReturnsReference_Local() {
  constexpr auto bar = Bar{};
  return bar;
}
*/

const Foo &testReturnsReferenceToTemporaryViaLifetimeBound_Constructed() {
  return Bar(); // expected-error {{cannot return result of lifetime-bound function 'operator const Foo &' on temporary of type 'Bar'}}
}

const Foo &testReturnsReferenceToTemporaryViaLifetimeBound2_Constructed() {
  return static_cast<const Foo &>(Bar()); // expected-error {{cannot return result of lifetime-bound function 'operator const Foo &' on temporary of type 'Bar'}}
}

const Foo &testReturnsReferenceToTemporaryViaLifetimeBound3_Constructed() {
  return Bar().AsFoo(); // expected-error {{cannot return result of lifetime-bound function 'AsFoo' on temporary of type 'Bar'}}
}

const Foo &
testReturnsReferenceToTemporaryViaLifetimeBound4_Constructed(bool aCond) {
  static constexpr Foo foo;
  return aCond ? foo : Bar().AsFoo(); // expected-error {{cannot return result of lifetime-bound function 'AsFoo' on temporary of type 'Bar'}}
}

Foo testReturnsValueViaLifetimeBoundFunction_Constructed() { return Bar(); }

Foo testReturnsValueViaLifetimeBoundFunction2_Constructed() {
  return static_cast<const Foo &>(Bar());
}

Foo testReturnsValueViaLifetimeBoundFunction3_Constructed() {
  return Bar().AsFoo();
}

Bar testReturnInstance_Returned() { return MakeBar(); }

const Foo &testReturnsReferenceToTemporaryViaLifetimeBound_Returned() {
  return MakeBar(); // expected-error {{cannot return result of lifetime-bound function 'operator const Foo &' on temporary of type 'Bar'}}
}

const Foo &testReturnsReferenceToTemporaryViaLifetimeBound2_Returned() {
  return static_cast<const Foo &>(MakeBar()); // expected-error {{cannot return result of lifetime-bound function 'operator const Foo &' on temporary of type 'Bar'}}
}

const Foo &testReturnsReferenceToTemporaryViaLifetimeBound3_Returned() {
  return MakeBar().AsFoo(); // expected-error {{cannot return result of lifetime-bound function 'AsFoo' on temporary of type 'Bar'}}
}

Foo testReturnsValueViaLifetimeBoundFunction_Returned() { return MakeBar(); }

Foo testReturnsValueViaLifetimeBoundFunction2_Returned() {
  return static_cast<const Foo &>(MakeBar());
}

Foo testReturnsValueViaLifetimeBoundFunction3_Returned() {
  return MakeBar().AsFoo();
}

void testNoLifetimeExtension() {
  const Foo &foo = Bar(); // expected-error {{cannot bind result of lifetime-bound function 'operator const Foo &' on temporary of type 'Bar' to reference, does not extend lifetime}}
}

void testNoLifetimeExtension2() {
  const auto &foo = static_cast<const Foo &>(MakeBar()); // expected-error {{cannot bind result of lifetime-bound function 'operator const Foo &' on temporary of type 'Bar' to reference, does not extend lifetime}}
}

void testNoLifetimeExtension3() {
  const Foo &foo = Bar().AsFoo(); // expected-error {{cannot bind result of lifetime-bound function 'AsFoo' on temporary of type 'Bar' to reference, does not extend lifetime}}
}

void testNoLifetimeExtension4(bool arg) {
  const Foo foo;
  const Foo &fooRef = arg ? foo : Bar().AsFoo(); // expected-error {{cannot bind result of lifetime-bound function 'AsFoo' on temporary of type 'Bar' to reference, does not extend lifetime}}
}

// While this looks similar to testNoLifetimeExtension4, this is actually fine,
// as the coerced type of the conditional operator is `Foo` here rather than
// `const Foo&`, and thus an implicit copy of `Bar().AsFoo()` is created, whose
// lifetime is actually extended.
void testLifetimeExtension(bool arg) {
  const Foo &foo = arg ? Foo() : Bar().AsFoo();
}

void testConvertToValue() { const Foo foo = Bar(); }

Foo testReturnConvertToValue() {
  return static_cast<Foo>(Bar());
}

void FooFunc(const Foo &aFoo);

// We want to allow binding to parameters of the target reference type though.
// This is the very reason the annotation is required, and the function cannot
// be restricted to lvalues. Lifetime is not an issue here, as the temporary's
// lifetime is until the end of the full expression anyway.

void testBindToParameter() {
  FooFunc(Bar());
  FooFunc(static_cast<const Foo &>(Bar()));
  FooFunc(Bar().AsFoo());
  FooFunc(MakeBar());
}

// This should be OK, because the return value isn't necessarily coming from the
// argument (and it should be OK for any type).
const Foo &RandomFunctionCall(const Foo &aFoo);
const Foo &testReturnFunctionCall() { return RandomFunctionCall(Bar()); }
