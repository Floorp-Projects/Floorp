#include "mozilla/MacroArgs.h"

static __attribute__((always_inline)) bool MOZ_AssertAssignmentTest(bool expr) {
  return expr;
}

#define MOZ_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#define MOZ_CRASH() do { } while(0)
#define MOZ_CHECK_ASSERT_ASSIGNMENT(expr) MOZ_AssertAssignmentTest(!!(expr))

#define MOZ_ASSERT_HELPER1(expr) \
  do { \
    if (MOZ_UNLIKELY(!MOZ_CHECK_ASSERT_ASSIGNMENT(expr))) { \
      MOZ_CRASH();\
    } \
  } while(0) \

/* Now the two-argument form. */
#define MOZ_ASSERT_HELPER2(expr, explain) \
  do { \
    if (MOZ_UNLIKELY(!MOZ_CHECK_ASSERT_ASSIGNMENT(expr))) { \
      MOZ_CRASH();\
    } \
  } while(0) \

#define MOZ_RELEASE_ASSERT_GLUE(a, b) a b
#define MOZ_RELEASE_ASSERT(...) \
  MOZ_RELEASE_ASSERT_GLUE( \
    MOZ_PASTE_PREFIX_AND_ARG_COUNT(MOZ_ASSERT_HELPER, __VA_ARGS__), \
    (__VA_ARGS__))

#define MOZ_ASSERT(...) MOZ_RELEASE_ASSERT(__VA_ARGS__)

void FunctionTest(int p) {
  MOZ_ASSERT(p = 1); // expected-error {{Forbidden assignment in assert expression}}
}

void FunctionTest2(int p) {
  MOZ_ASSERT(((p = 1)));  // expected-error {{Forbidden assignment in assert expression}}
}

void FunctionTest3(int p) {
  MOZ_ASSERT(p != 3);
}

class TestOverloading {
  int value;
public:
  explicit TestOverloading(int _val) : value(_val) {}
  // different operators
  explicit operator bool() const { return true; }
  TestOverloading& operator=(const int _val) { value = _val; return *this; }

  int& GetInt() {return value;}
};

void TestOverloadingFunc() {
  TestOverloading p(2);
  int f;

  MOZ_ASSERT(p);
  MOZ_ASSERT(p = 3); // expected-error {{Forbidden assignment in assert expression}}
  MOZ_ASSERT(p, "p is not valid");
  MOZ_ASSERT(p = 3, "p different than 3"); // expected-error {{Forbidden assignment in assert expression}}
  MOZ_ASSERT(p.GetInt() = 2); // expected-error {{Forbidden assignment in assert expression}}
  MOZ_ASSERT(p.GetInt() == 2);
  MOZ_ASSERT(p.GetInt() == 2, f = 3);
}
