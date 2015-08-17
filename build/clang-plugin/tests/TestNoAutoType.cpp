#define MOZ_NON_AUTOABLE __attribute__((annotate("moz_non_autoable")))

template<class T>
struct MOZ_NON_AUTOABLE ExplicitTypeTemplate {};
struct MOZ_NON_AUTOABLE ExplicitType {};
struct NonExplicitType {};

void f() {
  {
    ExplicitType a;
    auto b = a; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitType'}} expected-note {{Please write out this type explicitly}}
    auto &br = a; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitType &'}} expected-note {{Please write out this type explicitly}}
    const auto &brc = a; // expected-error {{Cannot use auto to declare a variable of type 'const ExplicitType &'}} expected-note {{Please write out this type explicitly}}
    auto *bp = &a; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitType *'}} expected-note {{Please write out this type explicitly}}
    const auto *bpc = &a; // expected-error {{Cannot use auto to declare a variable of type 'const ExplicitType *'}} expected-note {{Please write out this type explicitly}}
  }

  {
    ExplicitTypeTemplate<int> a;
    auto b = a; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitTypeTemplate<int>'}} expected-note {{Please write out this type explicitly}}
    auto &br = a; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitTypeTemplate<int> &'}} expected-note {{Please write out this type explicitly}}
    const auto &brc = a; // expected-error {{Cannot use auto to declare a variable of type 'const ExplicitTypeTemplate<int> &'}} expected-note {{Please write out this type explicitly}}
    auto *bp = &a; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitTypeTemplate<int> *'}} expected-note {{Please write out this type explicitly}}
    const auto *bpc = &a; // expected-error {{Cannot use auto to declare a variable of type 'const ExplicitTypeTemplate<int> *'}} expected-note {{Please write out this type explicitly}}
  }

  {
    NonExplicitType c;
    auto d = c;
    auto &dr = c;
    const auto &drc = c;
    auto *dp = &c;
    const auto *dpc = &c;
  }
}

ExplicitType A;
auto B = A; // expected-error {{Cannot use auto to declare a variable of type 'ExplicitType'}} expected-note {{Please write out this type explicitly}}

NonExplicitType C;
auto D = C;
