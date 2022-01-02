#include <cstddef>
#include <utility>

#define MOZ_MUST_RETURN_FROM_CALLER_IF_THIS_IS_ARG __attribute__((annotate("moz_must_return_from_caller_if_this_is_arg")))
#define MOZ_MAY_CALL_AFTER_MUST_RETURN __attribute__((annotate("moz_may_call_after_must_return")))

struct Thrower {
  void MOZ_MUST_RETURN_FROM_CALLER_IF_THIS_IS_ARG Throw() {}
};

void DoAnythingElse();
int MakeAnInt();
int MOZ_MAY_CALL_AFTER_MUST_RETURN SafeMakeInt();
bool Condition();

// It might be nicer to #include "mozilla/ScopeExit.h" and use that here -- but
// doing so also will #define the two attribute-macros defined above, running a
// risk of redefinition errors.  Just stick to the normal clang-plugin test
// style and use as little external code as possible.

template<typename Func>
class ScopeExit {
  Func exitFunction;
  bool callOnDestruction;
public:
  explicit ScopeExit(Func&& func)
    : exitFunction(std::move(func))
    , callOnDestruction(true)
  {}

  ~ScopeExit() {
    if (callOnDestruction) {
      exitFunction();
    }
  }

  void release() { callOnDestruction = false; }
};

template<typename ExitFunction>
ScopeExit<ExitFunction>
MakeScopeExit(ExitFunction&& func)
{
  return ScopeExit<ExitFunction>(std::move(func));
}

class Foo {
public:
  __attribute__((annotate("moz_implicit"))) Foo(std::nullptr_t);
  Foo();
};

void a1(Thrower& thrower) {
  thrower.Throw();
}

int a2(Thrower& thrower) {
  thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  return MakeAnInt();
}

int a3(Thrower& thrower) {
  // RAII operations happening after a must-immediately-return are fine.
  auto atExit = MakeScopeExit([] { DoAnythingElse(); });
  thrower.Throw();
  return 5;
}

int a4(Thrower& thrower) {
  thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  return Condition() ? MakeAnInt() : MakeAnInt();
}

void a5(Thrower& thrower) {
  thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  DoAnythingElse();
}

int a6(Thrower& thrower) {
  thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  DoAnythingElse();
  return MakeAnInt();
}

int a7(Thrower& thrower) {
  thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  DoAnythingElse();
  return Condition() ? MakeAnInt() : MakeAnInt();
}

int a8(Thrower& thrower) {
  thrower.Throw();
  return SafeMakeInt();
}

int a9(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return SafeMakeInt();
}

int a10(Thrower& thrower) {
  auto atExit = MakeScopeExit([] { DoAnythingElse(); });

  if (Condition()) {
    thrower.Throw();
    return SafeMakeInt();
  }

  atExit.release();
  DoAnythingElse();
  return 5;
}

void b1(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
}

int b2(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  }
  return MakeAnInt();
}

int b3(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return 5;
}

// Explicit test in orer to also verify the `UnaryOperator` node in the `CFG`
int b3a(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return -1;
}

float b3b(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return 1.0f;
}

bool b3c(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return false;
}

int b4(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  }
  return Condition() ? MakeAnInt() : MakeAnInt();
}

void b5(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  }
  DoAnythingElse();
}

void b6(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
    DoAnythingElse();
  }
}

void b7(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
    return;
  }
  DoAnythingElse();
}

void b8(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
    DoAnythingElse();
    return;
  }
  DoAnythingElse();
}

void b9(Thrower& thrower) {
  while (Condition()) {
    thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  }
}

void b10(Thrower& thrower) {
  while (Condition()) {
    thrower.Throw();
    return;
  }
}

void b11(Thrower& thrower) {
  thrower.Throw(); // expected-error {{You must immediately return after calling this function}}
  if (Condition()) {
    return;
  } else {
    return;
  }
}

void b12(Thrower& thrower) {
  switch (MakeAnInt()) {
  case 1:
    break;
  default:
    thrower.Throw();
    return;
  }
}

void b13(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return;
}

Foo b14(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
    return nullptr;
  }
  return nullptr;
}

Foo b15(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return nullptr;
}

Foo b16(Thrower& thrower) {
  if (Condition()) {
    thrower.Throw();
  }
  return Foo();
}

void c1() {
  Thrower thrower;
  thrower.Throw();
  DoAnythingElse(); // Should be allowed, since our thrower is not an arg
}

class TestRet {
  TestRet *b13(Thrower &thrower) {
    if (Condition()) {
      thrower.Throw();
    }
    return this;
  }
};
