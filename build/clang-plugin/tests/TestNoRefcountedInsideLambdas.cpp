#include <functional>
#include "mozilla/Function.h"
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

void c() {
  R* ptr;
  SmartPtr<R> sp;
  mozilla::function<void(R*)>([&](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  mozilla::function<void(SmartPtr<R>)>([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  mozilla::function<void(R*)>([&](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  mozilla::function<void(SmartPtr<R>)>([&](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  mozilla::function<void(R*)>([=](R* argptr) {
    R* localptr;
    ptr->method(); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    argptr->method();
    localptr->method();
  });
  mozilla::function<void(SmartPtr<R>)>([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  mozilla::function<void(R*)>([=](R* argptr) {
    R* localptr;
    take(ptr); // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    take(argptr);
    take(localptr);
  });
  mozilla::function<void(SmartPtr<R>)>([=](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  mozilla::function<void(R*)>([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  mozilla::function<void(SmartPtr<R>)>([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  mozilla::function<void(R*)>([ptr](R* argptr) { // expected-error{{Refcounted variable 'ptr' of type 'R' cannot be captured by a lambda}} expected-note{{Please consider using a smart pointer}}
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  mozilla::function<void(SmartPtr<R>)>([sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    take(sp);
    take(argsp);
    take(localsp);
  });
  mozilla::function<void(R*)>([&ptr](R* argptr) {
    R* localptr;
    ptr->method();
    argptr->method();
    localptr->method();
  });
  mozilla::function<void(SmartPtr<R>)>([&sp](SmartPtr<R> argsp) {
    SmartPtr<R> localsp;
    sp->method();
    argsp->method();
    localsp->method();
  });
  mozilla::function<void(R*)>([&ptr](R* argptr) {
    R* localptr;
    take(ptr);
    take(argptr);
    take(localptr);
  });
  mozilla::function<void(SmartPtr<R>)>([&sp](SmartPtr<R> argsp) {
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
    return ([&](R* argptr) {
      R* localptr;
      ptr->method();
      argptr->method();
      localptr->method();
    });
  };
  auto e2 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      sp->method();
      argsp->method();
      localsp->method();
    });
  };
  auto e3 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](R* argptr) {
      R* localptr;
      take(ptr);
      take(argptr);
      take(localptr);
    });
  };
  auto e4 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      take(sp);
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
    return ([&ptr](R* argptr) {
      R* localptr;
      ptr->method();
      argptr->method();
      localptr->method();
    });
  };
  auto e15 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&sp](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      sp->method();
      argsp->method();
      localsp->method();
    });
  };
  auto e16 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&ptr](R* argptr) {
      R* localptr;
      take(ptr);
      take(argptr);
      take(localptr);
    });
  };
  auto e17 = []() {
    R* ptr;
    SmartPtr<R> sp;
    return ([&sp](SmartPtr<R> argsp) {
      SmartPtr<R> localsp;
      take(sp);
      take(argsp);
      take(localsp);
    });
  };
}
