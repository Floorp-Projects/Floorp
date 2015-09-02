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
  take([&](R* argptr) {
    R* localptr;
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    argptr->method();
    localptr->method();
  });
  take([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  take([&](R* argptr) {
    R* localptr;
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    take(argptr);
    take(localptr);
  });
  take([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  take([=](R* argptr) {
    R* localptr;
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    argptr->method();
    localptr->method();
  });
  take([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  take([=](R* argptr) {
    R* localptr;
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    take(argptr);
    take(localptr);
  });
  take([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  take([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  take([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  take([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  take([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}
