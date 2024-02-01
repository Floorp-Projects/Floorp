#include <functional>

#include <mozilla/RefPtr.h>

struct RefCountedBase {
  void AddRef();
  void Release();
};

struct Bar;

struct Foo : RefCountedBase {
  void foo();
};

struct Bar : RefCountedBase {
  Bar() {
    RefPtr<Bar> self = this; // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    auto self2 = RefPtr(this); // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    auto self3 = RefPtr{this}; // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    RefPtr<Bar> self4(this); // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    RefPtr<Bar> self5{this}; // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    [self=RefPtr{this}]{}(); // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    refptr(RefPtr{this}); // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    refptr(this); // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
  }

  explicit Bar(float f) {
    // Does not match member expressions with `this`
    RefPtr<Foo> foo = this->mFoo;
    foo->foo();
    auto foo2 = RefPtr(this->mFoo);
    foo2->foo();
    auto foo3 = RefPtr{this->mFoo};
    foo3->foo();
  }

  explicit Bar(short i);

  explicit Bar(int i): mBar(this) {} // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
  explicit Bar(int i, int i2): mBar(RefPtr(this)) {} // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}

  void Init() {
    // Does not match outside the constructor
    RefPtr<Bar> self = this;
    auto self2 = RefPtr(this);
    auto self3 = RefPtr{this};
  }

  void refptr(const RefPtr<Bar>& aBar) {}

  RefPtr<Foo> mFoo;
  RefPtr<Bar> mBar;
};

// Test case for non-inline constructor
Bar::Bar(short i) {
  RefPtr<Bar> self = this; // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
  auto self2 = RefPtr(this); // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
  auto self3 = RefPtr{this}; // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
}

// Same goes for any class with MOZ_IS_REFPTR (not including nsCOMPtr because SM
// doesn't have it)
template <typename T> class MOZ_IS_REFPTR MyRefPtr {
public:
  MOZ_IMPLICIT MyRefPtr(T *aPtr) : mPtr(aPtr) {}
  T *mPtr;
};

class Baz : RefCountedBase {
  Baz() {
    MyRefPtr<Baz> self = this; // expected-error {{Refcounting `this` inside the constructor is a footgun, `this` may be destructed at the end of the constructor unless there's another strong reference. Consider adding a separate Create function and do the work there.}}
    (void)self;
  }
};
