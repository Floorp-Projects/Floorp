#include <mozilla/RefPtr.h>

#define MOZ_KNOWN_LIVE __attribute__((annotate("moz_known_live")))

class Foo {
  // dummy refcounting
public:
  uint32_t AddRef() { return 0; }
  uint32_t Release() { return 0; }

private:
  ~Foo() = default;
};

class Bar {
  MOZ_KNOWN_LIVE RefPtr<Foo> mFoo;
  Bar() : mFoo(new Foo()) {}
  ~Bar() { mFoo = nullptr; }

  void Baz() {
    mFoo = nullptr; // expected-error {{MOZ_KNOWN_LIVE members can only be modified by constructors and destructors}}
  }
};

class Bar2 {
  MOZ_KNOWN_LIVE Foo *mFoo;
  Bar2() : mFoo(new Foo()) {}
  ~Bar2() { mFoo = nullptr; }

  void Baz() {
    mFoo = nullptr; // expected-error {{MOZ_KNOWN_LIVE members can only be modified by constructors and destructors}}
  }
};
