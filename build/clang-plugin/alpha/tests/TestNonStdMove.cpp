#include <mozilla/RefPtr.h>

// we can't include nsCOMPtr.h here, so let's redefine a basic version
template<typename T>
struct nsCOMPtr {
  nsCOMPtr() = default;

  template<typename U>
  MOZ_IMPLICIT nsCOMPtr(already_AddRefed<U>&& aSrc);

  template<typename U>
  nsCOMPtr& operator=(already_AddRefed<U>&& aSrc);
};


using namespace mozilla;

struct RefCountedBase {
  void AddRef();
  void Release();

  void method_test();
};

struct RefCountedDerived : RefCountedBase {};

struct RefCountedBaseHolder {
  RefPtr<RefCountedBase> GetRefCountedBase() const {
    return mRefCountedBase;
  }

private:
  RefPtr<RefCountedBase> mRefCountedBase = MakeRefPtr<RefCountedBase>();
};


void test_assign_same_type() {
  RefPtr<RefCountedBase> a = MakeRefPtr<RefCountedBase>();
  RefPtr<RefCountedBase> b;

  b = a.forget(); // expected-warning {{non-standard move assignment}}
}

void test_assign_implicit_cast() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  RefPtr<RefCountedBase> b;

  b = a.forget(); // expected-warning {{non-standard move assignment}}
}

void test_assign_different_template() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  nsCOMPtr<RefCountedBase> b;

  b = a.forget(); // expected-warning {{non-standard move assignment}}
}

void test_construct_different_template() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  nsCOMPtr<RefCountedBase> b = a.forget(); // expected-warning {{non-standard move construction}}
}

void test_assign_already_addrefed() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  already_AddRefed<RefCountedDerived> b;

  b = a.forget();
}

void test_construct_already_addrefed() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  already_AddRefed<RefCountedDerived> b = a.forget();
}

void test_construct_same_type() {
  RefPtr<RefCountedBase> a = MakeRefPtr<RefCountedBase>();
  RefPtr<RefCountedBase> b = a.forget(); // expected-warning {{non-standard move construction}}
}

void test_construct_implicit_cast() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  RefPtr<RefCountedBase> b = a.forget(); // expected-warning {{non-standard move construction}}
}

void test_construct_brace_same_type() {
  RefPtr<RefCountedBase> a = MakeRefPtr<RefCountedBase>();
  auto b = RefPtr<RefCountedBase>{a.forget()}; // expected-warning {{non-standard move construction}}
}

void test_construct_brace_implicit_cast() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  auto b = RefPtr<RefCountedBase>{a.forget()}; // expected-warning {{non-standard move construction}}
}

void test_construct_function_style_same_type() {
  RefPtr<RefCountedBase> a = MakeRefPtr<RefCountedBase>();
  auto b = RefPtr<RefCountedBase>(a.forget()); // expected-warning {{non-standard move construction}}
}

void test_construct_function_style_implicit_cast() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  auto b = RefPtr<RefCountedBase>(a.forget());  // expected-warning {{non-standard move construction}}
}

void test_construct_result_type() {
  RefPtr<RefCountedBase> a = MakeRefPtr<RefCountedBase>();
  already_AddRefed<RefCountedBase> b = a.forget();
}

void test_construct_implicitly_cast_result_type() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  already_AddRefed<RefCountedBase> b = a.forget();
}

void foo(already_AddRefed<RefCountedBase>&& aArg);

void test_call_with_result_type() {
  RefPtr<RefCountedBase> a = MakeRefPtr<RefCountedBase>();
  foo(a.forget());
}

void test_call_with_implicitly_cast_result_type() {
  RefPtr<RefCountedDerived> a = MakeRefPtr<RefCountedDerived>();
  foo(a.forget());
}
