#define MOZ_TRIVIAL_CTOR_DTOR __attribute__((annotate("moz_trivial_ctor_dtor")))

struct MOZ_TRIVIAL_CTOR_DTOR EmptyClass{};

template <class T>
struct MOZ_TRIVIAL_CTOR_DTOR TemplateEmptyClass{};

struct MOZ_TRIVIAL_CTOR_DTOR BadUserDefinedCtor { // expected-error {{class 'BadUserDefinedCtor' must have trivial constructors and destructors}}
  BadUserDefinedCtor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadUserDefinedDtor { // expected-error {{class 'BadUserDefinedDtor' must have trivial constructors and destructors}}
  ~BadUserDefinedDtor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadVirtualDtor { // expected-error {{class 'BadVirtualDtor' must have trivial constructors and destructors}}
  virtual ~BadVirtualDtor() {}
};

struct MOZ_TRIVIAL_CTOR_DTOR BadVirtualMember { // expected-error {{class 'BadVirtualMember' must have trivial constructors and destructors}}
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

struct MOZ_TRIVIAL_CTOR_DTOR BadVirtualMemberInMember { // expected-error {{class 'BadVirtualMemberInMember' must have trivial constructors and destructors}}
  VirtualMember m;
};
