#define MOZ_TRIVIAL_CTOR_DTOR __attribute__((annotate("moz_trivial_ctor_dtor")))

struct MOZ_TRIVIAL_CTOR_DTOR EmptyClass{};

template <class T>
struct MOZ_TRIVIAL_CTOR_DTOR TemplateEmptyClass{};

struct MOZ_TRIVIAL_CTOR_DTOR NonEmptyClass {
  void *m;
};

template <class T>
struct MOZ_TRIVIAL_CTOR_DTOR TemplateNonEmptyClass {
  T* m;
};

struct MOZ_TRIVIAL_CTOR_DTOR BadUserDefinedCtor { // expected-error {{class 'BadUserDefinedCtor' must have trivial constructors and destructors}}
  BadUserDefinedCtor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadUserDefinedDtor { // expected-error {{class 'BadUserDefinedDtor' must have trivial constructors and destructors}}
  ~BadUserDefinedDtor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadVirtualDtor { // expected-error {{class 'BadVirtualDtor' must have trivial constructors and destructors}}
  virtual ~BadVirtualDtor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR OkVirtualMember {
  virtual void f();
};

void foo();
struct MOZ_TRIVIAL_CTOR_DTOR BadNonEmptyCtorDtor { // expected-error {{class 'BadNonEmptyCtorDtor' must have trivial constructors and destructors}}
  BadNonEmptyCtorDtor() { foo(); }
  ~BadNonEmptyCtorDtor() { foo(); }
};

struct NonTrivialCtor {
  NonTrivialCtor() { foo(); }
};

struct NonTrivialDtor {
  ~NonTrivialDtor() { foo(); }
};

struct VirtualMember {
  virtual void f();
};

struct MOZ_TRIVIAL_CTOR_DTOR BadNonTrivialCtorInBase : NonTrivialCtor { // expected-error {{class 'BadNonTrivialCtorInBase' must have trivial constructors and destructors}}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadNonTrivialDtorInBase : NonTrivialDtor { // expected-error {{class 'BadNonTrivialDtorInBase' must have trivial constructors and destructors}}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadNonTrivialCtorInMember { // expected-error {{class 'BadNonTrivialCtorInMember' must have trivial constructors and destructors}}
  NonTrivialCtor m;
};

struct MOZ_TRIVIAL_CTOR_DTOR BadNonTrivialDtorInMember { // expected-error {{class 'BadNonTrivialDtorInMember' must have trivial constructors and destructors}}
  NonTrivialDtor m;
};

struct MOZ_TRIVIAL_CTOR_DTOR OkVirtualMemberInMember {
  VirtualMember m;
};

struct MOZ_TRIVIAL_CTOR_DTOR OkConstExprConstructor {
  constexpr OkConstExprConstructor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR OkConstExprConstructorInMember {
  OkConstExprConstructor m;
};

// XXX: This error is unfortunate, but is unlikely to come up in real code.
// In this situation, it should be possible to define a constexpr constructor
// which explicitly initializes the members.
struct MOZ_TRIVIAL_CTOR_DTOR BadUnfortunateError { // expected-error {{class 'BadUnfortunateError' must have trivial constructors and destructors}}
  OkConstExprConstructor m;
  void *n;
};
