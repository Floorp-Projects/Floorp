#define MOZ_NON_TEMPORARY_CLASS __attribute__((annotate("moz_non_temporary_class")))
#define MOZ_STACK_CLASS __attribute__((annotate("moz_stack_class")))

class MOZ_NON_TEMPORARY_CLASS MOZ_STACK_CLASS TestClass {};

TestClass foo; // expected-error {{variable of type 'TestClass' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}

TestClass f()
{
  TestClass bar;
  return bar;
}

void gobbleref(const TestClass&) { }

void g()
{
  gobbleref(f()); // expected-error {{variable of type 'TestClass' is not valid in a temporary}} expected-note {{value incorrectly allocated in a temporary}}
}
