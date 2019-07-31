#define MOZ_TRIVIAL_DTOR __attribute__((annotate("moz_trivial_dtor")))

struct MOZ_TRIVIAL_DTOR EmptyClass{};

template <class T>
struct MOZ_TRIVIAL_DTOR TemplateEmptyClass{};

struct MOZ_TRIVIAL_DTOR NonEmptyClass {
  void *m;
};

template <class T>
struct MOZ_TRIVIAL_DTOR TemplateNonEmptyClass {
  T* m;
};

struct MOZ_TRIVIAL_DTOR BadUserDefinedDtor { // expected-error {{class 'BadUserDefinedDtor' must have a trivial destructor}}
  ~BadUserDefinedDtor() {}
};

struct MOZ_TRIVIAL_DTOR BadVirtualDtor { // expected-error {{class 'BadVirtualDtor' must have a trivial destructor}}
  virtual ~BadVirtualDtor() {}
};

struct MOZ_TRIVIAL_DTOR OkVirtualMember {
  virtual void f();
};

void foo();
struct MOZ_TRIVIAL_DTOR BadNonEmptyCtorDtor { // expected-error {{class 'BadNonEmptyCtorDtor' must have a trivial destructor}}
  BadNonEmptyCtorDtor() { foo(); }
  ~BadNonEmptyCtorDtor() { foo(); }
};

struct NonTrivialDtor {
  ~NonTrivialDtor() { foo(); }
};

struct VirtualMember {
  virtual void f();
};

struct MOZ_TRIVIAL_DTOR BadNonTrivialDtorInBase : NonTrivialDtor { // expected-error {{class 'BadNonTrivialDtorInBase' must have a trivial destructor}}
};

struct MOZ_TRIVIAL_DTOR BadNonTrivialDtorInMember { // expected-error {{class 'BadNonTrivialDtorInMember' must have a trivial destructor}}
  NonTrivialDtor m;
};

struct MOZ_TRIVIAL_DTOR OkVirtualMemberInMember {
  VirtualMember m;
};

