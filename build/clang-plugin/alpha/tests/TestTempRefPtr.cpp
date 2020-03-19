#include <mozilla/RefPtr.h>

using namespace mozilla;

struct RefCountedBase {
  void AddRef();
  void Release();

  void method_test();
};

struct RefCountedBaseHolder {
  RefPtr<RefCountedBase> GetRefCountedBase() const {
    return mRefCountedBase;
  }

private:
  RefPtr<RefCountedBase> mRefCountedBase = MakeRefPtr<RefCountedBase>();
};


void test_arrow_temporary_new_refptr_function_style_cast() {
  RefPtr<RefCountedBase>(new RefCountedBase())->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}}
}

void test_arrow_temporary_new_refptr_brace() {
  RefPtr<RefCountedBase>{new RefCountedBase()}->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}}
}

void test_arrow_temporary_new_c_style_cast() {
  ((RefPtr<RefCountedBase>)(new RefCountedBase()))->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}}
}

void test_arrow_temporary_new_static_cast() {
  static_cast<RefPtr<RefCountedBase>>(new RefCountedBase())->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}}
}

void test_arrow_temporary_new_refptr_makerefptr() {
  MakeRefPtr<RefCountedBase>()->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}}
}

void test_arrow_temporary_get_refptr_from_member_function() {
  const RefCountedBaseHolder holder;
  holder.GetRefCountedBase()->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}} expected-note {{consider changing function RefCountedBaseHolder::GetRefCountedBase to return a raw reference instead}}
}

void test_ref(RefCountedBase &aRefCountedBase);

void test_star_temporary_new_refptr_function_style_cast() {
  // TODO: Should we warn about operator* as well?
  test_ref(*RefPtr<RefCountedBase>(new RefCountedBase()));
}
