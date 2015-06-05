#define MOZ_STRONG_REF __attribute__((annotate("moz_strong_ref")))

struct RefCountedBase {
  void AddRef();
  void Release();
};

template <class T>
struct SmartPtr {
  T* MOZ_STRONG_REF t;
  T* operator->() const;
};

struct R : RefCountedBase {
  void method();
};

void take(...);
void foo() {
  R* ptr;
  SmartPtr<R> sp;
  take([&]() {
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be used inside a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  take([&]() {
    sp->method();
  });
  take([&]() {
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be used inside a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  take([&]() {
    take(sp);
  });
  take([=]() {
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be used inside a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  take([=]() {
    sp->method();
  });
  take([=]() {
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be used inside a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  take([=]() {
    take(sp);
  });
  take([ptr]() {
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be used inside a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  take([sp]() {
    sp->method();
  });
  take([ptr]() {
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be used inside a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  take([sp]() {
    take(sp);
  });
}
