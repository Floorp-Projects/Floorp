#define MOZ_NO_ADDREF_RELEASE_ON_RETURN __attribute__((annotate("moz_no_addref_release_on_return")))

struct Test {
  void AddRef();
  void Release();
  void foo();
};

struct TestD : Test {};

struct S {
  Test* f() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  Test& g() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  Test  h() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
};

struct SD {
  TestD* f() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  TestD& g() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  TestD  h() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
};

template<class T>
struct X {
  T* f() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  T& g() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  T  h() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
};

template<class T>
struct SP {
  T* operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
};

Test* f() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
Test& g() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
Test  h() MOZ_NO_ADDREF_RELEASE_ON_RETURN;

TestD* fd() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
TestD& gd() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
TestD  hd() MOZ_NO_ADDREF_RELEASE_ON_RETURN;

void test() {
  S s;
  s.f()->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'S::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  s.f()->Release(); // expected-error{{'Release' must not be called on the return value of 'S::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  s.f()->foo();
  s.g().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'S::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  s.g().Release(); // expected-error{{'Release' must not be called on the return value of 'S::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  s.g().foo();
  s.h().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'S::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  s.h().Release(); // expected-error{{'Release' must not be called on the return value of 'S::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  s.h().foo();
  SD sd;
  sd.f()->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'SD::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sd.f()->Release(); // expected-error{{'Release' must not be called on the return value of 'SD::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sd.f()->foo();
  sd.g().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'SD::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sd.g().Release(); // expected-error{{'Release' must not be called on the return value of 'SD::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sd.g().foo();
  sd.h().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'SD::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sd.h().Release(); // expected-error{{'Release' must not be called on the return value of 'SD::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sd.h().foo();
  X<Test> x;
  x.f()->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'X<Test>::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  x.f()->Release(); // expected-error{{'Release' must not be called on the return value of 'X<Test>::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  x.f()->foo();
  x.g().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'X<Test>::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  x.g().Release(); // expected-error{{'Release' must not be called on the return value of 'X<Test>::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  x.g().foo();
  x.h().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'X<Test>::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  x.h().Release(); // expected-error{{'Release' must not be called on the return value of 'X<Test>::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  x.h().foo();
  X<TestD> xd;
  xd.f()->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'X<TestD>::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  xd.f()->Release(); // expected-error{{'Release' must not be called on the return value of 'X<TestD>::f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  xd.f()->foo();
  xd.g().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'X<TestD>::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  xd.g().Release(); // expected-error{{'Release' must not be called on the return value of 'X<TestD>::g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  xd.g().foo();
  xd.h().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'X<TestD>::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  xd.h().Release(); // expected-error{{'Release' must not be called on the return value of 'X<TestD>::h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  xd.h().foo();
  SP<Test> sp;
  sp->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'SP<Test>::operator->' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sp->Release(); // expected-error{{'Release' must not be called on the return value of 'SP<Test>::operator->' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  sp->foo();
  SP<TestD> spd;
  spd->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'SP<TestD>::operator->' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  spd->Release(); // expected-error{{'Release' must not be called on the return value of 'SP<TestD>::operator->' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  spd->foo();
  f()->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  f()->Release(); // expected-error{{'Release' must not be called on the return value of 'f' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  f()->foo();
  g().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  g().Release(); // expected-error{{'Release' must not be called on the return value of 'g' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  g().foo();
  h().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  h().Release(); // expected-error{{'Release' must not be called on the return value of 'h' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  h().foo();
  fd()->AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'fd' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  fd()->Release(); // expected-error{{'Release' must not be called on the return value of 'fd' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  fd()->foo();
  gd().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'gd' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  gd().Release(); // expected-error{{'Release' must not be called on the return value of 'gd' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  gd().foo();
  hd().AddRef(); // expected-error{{'AddRef' must not be called on the return value of 'hd' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  hd().Release(); // expected-error{{'Release' must not be called on the return value of 'hd' which is marked with MOZ_NO_ADDREF_RELEASE_ON_RETURN}}
  hd().foo();
}
