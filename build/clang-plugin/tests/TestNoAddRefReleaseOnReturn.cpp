#define MOZ_NO_ADDREF_RELEASE_ON_RETURN __attribute__((annotate("moz_no_addref_release_on_return")))

struct Test {
  void AddRef();
  void Release();
  void foo();
};

struct S {
  Test* f() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  Test& g() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
  Test  h() MOZ_NO_ADDREF_RELEASE_ON_RETURN;
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

void test() {
  S s;
  s.f()->AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'f'}}
  s.f()->Release(); // expected-error{{'Release' cannot be called on the return value of 'f'}}
  s.f()->foo();
  s.g().AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'g'}}
  s.g().Release(); // expected-error{{'Release' cannot be called on the return value of 'g'}}
  s.g().foo();
  s.h().AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'h'}}
  s.h().Release(); // expected-error{{'Release' cannot be called on the return value of 'h'}}
  s.h().foo();
  X<Test> x;
  x.f()->AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'f'}}
  x.f()->Release(); // expected-error{{'Release' cannot be called on the return value of 'f'}}
  x.f()->foo();
  x.g().AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'g'}}
  x.g().Release(); // expected-error{{'Release' cannot be called on the return value of 'g'}}
  x.g().foo();
  x.h().AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'h'}}
  x.h().Release(); // expected-error{{'Release' cannot be called on the return value of 'h'}}
  x.h().foo();
  SP<Test> sp;
  sp->AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'operator->'}}
  sp->Release(); // expected-error{{'Release' cannot be called on the return value of 'operator->'}}
  sp->foo();
  f()->AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'f'}}
  f()->Release(); // expected-error{{'Release' cannot be called on the return value of 'f'}}
  f()->foo();
  g().AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'g'}}
  g().Release(); // expected-error{{'Release' cannot be called on the return value of 'g'}}
  g().foo();
  h().AddRef(); // expected-error{{'AddRef' cannot be called on the return value of 'h'}}
  h().Release(); // expected-error{{'Release' cannot be called on the return value of 'h'}}
  h().foo();
}
