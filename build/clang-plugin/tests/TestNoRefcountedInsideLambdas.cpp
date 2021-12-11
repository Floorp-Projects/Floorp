#include <mozilla/StaticAnalysisFunctions.h>

#include <functional>
#define MOZ_STRONG_REF
#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

// Ensure that warnings about returning stack addresses of local variables are
// errors, so our `expected-error` annotations below work correctly.
#pragma GCC diagnostic error "-Wreturn-stack-address"

struct RefCountedBase {
  void AddRef();
  void Release();
};

template <class T>
struct SmartPtr {
  SmartPtr();
  MOZ_IMPLICIT SmartPtr(T*);
  T* MOZ_STRONG_REF t;
  T* operator->() const;
};

struct R : RefCountedBase {
  void method();
private:
  void privateMethod();
};

void take(...);
void foo() {
  R* ptr;
  SmartPtr<R> sp;
  take([&](R* argptr) {
    R* localptr;
    ptr->method();
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
    take(ptr);
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
    ptr->method();
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
    take(ptr);
    take(argptr);
    take(localptr);
  });
  take([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  take([ptr](R* argptr) {
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
  take([ptr](R* argptr) {
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
  take([&ptr](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  take([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  take([&ptr](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  take([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}

void b() {
  R* ptr;
  SmartPtr<R> sp;
  std::function<void(R*)>([&](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  std::function<void(SmartPtr<R>)>([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  std::function<void(R*)>([&](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  std::function<void(SmartPtr<R>)>([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  std::function<void(R*)>([=](R* argptr) {
    R* localptr;
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    argptr->method();
    localptr->method();
  });
  std::function<void(SmartPtr<R>)>([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  std::function<void(R*)>([=](R* argptr) {
    R* localptr;
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    take(argptr);
    take(localptr);
  });
  std::function<void(SmartPtr<R>)>([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  std::function<void(R*)>([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  std::function<void(SmartPtr<R>)>([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  std::function<void(R*)>([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  std::function<void(SmartPtr<R>)>([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  std::function<void(R*)>([&ptr](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  std::function<void(SmartPtr<R>)>([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  std::function<void(R*)>([&ptr](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  std::function<void(SmartPtr<R>)>([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}

// These tests would check c++14 deduced return types, if they were supported in
// our codebase. They are being kept here for convenience in the future if we do
// add support for c++14 deduced return types
#if 0
auto d1() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
}
auto d2() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
}
auto d3() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
}
auto d4() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}
auto d5() {
  R* ptr;
  SmartPtr<R> sp;
  return ([=](R* argptr) {
    R* localptr;
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    argptr->method();
    localptr->method();
  });
}
auto d6() {
  R* ptr;
  SmartPtr<R> sp;
  return ([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
}
auto d8() {
  R* ptr;
  SmartPtr<R> sp;
  return ([=](R* argptr) {
    R* localptr;
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    take(argptr);
    take(localptr);
  });
}
auto d9() {
  R* ptr;
  SmartPtr<R> sp;
  return ([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}
auto d10() {
  R* ptr;
  SmartPtr<R> sp;
  return ([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
}
auto d11() {
  R* ptr;
  SmartPtr<R> sp;
  return ([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
}
auto d12() {
  R* ptr;
  SmartPtr<R> sp;
  return ([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
}
auto d13() {
  R* ptr;
  SmartPtr<R> sp;
  return ([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}
auto d14() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&ptr](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
}
auto d15() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
}
auto d16() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&ptr](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
}
auto d17() {
  R* ptr;
  SmartPtr<R> sp;
  return ([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
}
#endif

void e() {
  auto e1 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](R* argptr) { // expected-error{{address of stack memory associated with local variable 'ptr' returned}}
      R* localptr;
#if __clang_major__ >= 12
      ptr->method(); // expected-note{{implicitly captured by reference due to use here}}
#else
      ptr->method();
#endif
      argptr->method();
      localptr->method();
    });
  };
  auto e2 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](SmartPtr<R> argsp) { // expected-error{{address of stack memory associated with local variable 'sp' returned}}
      SmartPtr<R> localsp;
#if __clang_major__ >= 12
      sp->method(); // expected-note{{implicitly captured by reference due to use here}}
#else
      sp->method();
#endif
      argsp->method();
      localsp->method();
    });
  };
  auto e3 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](R* argptr) { // expected-error{{address of stack memory associated with local variable 'ptr' returned}}
      R* localptr;
#if __clang_major__ >= 12
      take(ptr); // expected-note{{implicitly captured by reference due to use here}}
#else
      take(ptr);
#endif
      take(argptr);
      take(localptr);
    });
  };
  auto e4 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](SmartPtr<R> argsp) { // expected-error{{address of stack memory associated with local variable 'sp' returned}}
      SmartPtr<R> localsp;
#if __clang_major__ >= 12
      take(sp); // expected-note{{implicitly captured by reference due to use here}}
#else
      take(sp);
#endif
      take(argsp);
      take(localsp);
    });
  };
  auto e5 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([=](R* argptr) {
      R* localptr;
      ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
      argptr->method();
      localptr->method();
    });
  };
  auto e6 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([=](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      sp->method();
      argsp->method();
      localsp->method();
    });
  };
  auto e8 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([=](R* argptr) {
      R* localptr;
      take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
      take(argptr);
      take(localptr);
    });
  };
  auto e9 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([=](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      take(sp);
      take(argsp);
      take(localsp);
    });
  };
  auto e10 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
      R* localptr;
      ptr->method();
      argptr->method();
      localptr->method();
    });
  };
  auto e11 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([sp](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      sp->method();
      argsp->method();
      localsp->method();
    });
  };
  auto e12 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
      R* localptr;
      take(ptr);
      take(argptr);
      take(localptr);
    });
  };
  auto e13 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([sp](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      take(sp);
      take(argsp);
      take(localsp);
    });
  };
  auto e14 = []() {
    R* ptr;
    SmartPtr<R> sp;
#if __clang_major__ >= 12
    return ([&ptr](R* argptr) { // expected-error{{address of stack memory associated with local variable 'ptr' returned}} expected-note{{captured by reference here}}
      R* localptr;
      ptr->method();
      argptr->method();
      localptr->method();
    });
#else
    return ([&ptr](R* argptr) { // expected-error{{address of stack memory associated with local variable 'ptr' returned}}
      R* localptr;
      ptr->method();
      argptr->method();
      localptr->method();
    });
#endif
  };
  auto e15 = []() {
    R* ptr;
    SmartPtr<R> sp;
#if __clang_major__ >= 12
    return ([&sp](SmartPtr<R> argsp) { // expected-error{{address of stack memory associated with local variable 'sp' returned}} expected-note{{captured by reference here}}
      SmartPtr<R> localsp;
      sp->method();
      argsp->method();
      localsp->method();
    });
#else
    return ([&sp](SmartPtr<R> argsp) { // expected-error{{address of stack memory associated with local variable 'sp' returned}}
      SmartPtr<R> localsp;
      sp->method();
      argsp->method();
      localsp->method();
    });
#endif
  };
  auto e16 = []() {
    R* ptr;
    SmartPtr<R> sp;
#if __clang_major__ >= 12
    return ([&ptr](R* argptr) { // expected-error{{address of stack memory associated with local variable 'ptr' returned}} expected-note{{captured by reference here}}
      R* localptr;
      take(ptr);
      take(argptr);
      take(localptr);
    });
#else
    return ([&ptr](R* argptr) { // expected-error{{address of stack memory associated with local variable 'ptr' returned}}
      R* localptr;
      take(ptr);
      take(argptr);
      take(localptr);
    });
#endif
  };
  auto e17 = []() {
    R* ptr;
    SmartPtr<R> sp;
#if __clang_major__ >= 12
    return ([&sp](SmartPtr<R> argsp) { // expected-error{{address of stack memory associated with local variable 'sp' returned}} expected-note{{captured by reference here}}
      SmartPtr<R> localsp;
      take(sp);
      take(argsp);
      take(localsp);
    });
#else
    return ([&sp](SmartPtr<R> argsp) { // expected-error{{address of stack memory associated with local variable 'sp' returned}}
      SmartPtr<R> localsp;
      take(sp);
      take(argsp);
      take(localsp);
    });
#endif
  };
}

void
R::privateMethod() {
  SmartPtr<R> self = this;
  std::function<void()>([&]() {
    self->method();
  });
  std::function<void()>([&]() {
    self->privateMethod();
  });
  std::function<void()>([&]() {
    this->method();
  });
  std::function<void()>([&]() {
    this->privateMethod();
  });
  std::function<void()>([=]() {
    self->method();
  });
  std::function<void()>([=]() {
    self->privateMethod();
  });
  std::function<void()>([=]() {
    this->method(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([=]() {
    this->privateMethod(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([self]() {
    self->method();
  });
  std::function<void()>([self]() {
    self->privateMethod();
  });
  std::function<void()>([this]() {
    this->method(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([this]() {
    this->privateMethod(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([this]() {
    method(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([this]() {
    privateMethod(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([=]() {
    method(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([=]() {
    privateMethod(); // expected-error{{Refcounted variable 'this' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
  });
  std::function<void()>([&]() {
    method();
  });
  std::function<void()>([&]() {
    privateMethod();
  });

  std::function<void()>(
      [instance = MOZ_KnownLive(this)]() { instance->privateMethod(); });

  // It should be OK to go through `this` if we have captured a reference to it.
  std::function<void()>([this, self]() {
    this->method();
    this->privateMethod();
    method();
    privateMethod();
  });
}
