#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

template <typename T>
class already_AddRefed {
public:
  already_AddRefed();
  T* mPtr;
};

template <typename T>
class RefPtr {
public:
  RefPtr();
  MOZ_IMPLICIT RefPtr(T* aIn);
  MOZ_IMPLICIT RefPtr(already_AddRefed<T> aIn);
  ~RefPtr();
  T* mPtr;
};

template <typename T>
class nsCOMPtr {
public:
  nsCOMPtr();
  MOZ_IMPLICIT nsCOMPtr(T* aIn);
  MOZ_IMPLICIT nsCOMPtr(already_AddRefed<T> aIn);
  ~nsCOMPtr();
  T* mPtr;
};

class Type {
public:
  static nsCOMPtr<Type> someStaticCOMPtr;

  void f(nsCOMPtr<Type> ignoredArgument, Type *param) {
    nsCOMPtr<Type> never_referenced;
    nsCOMPtr<Type> kfdg_t1(this);
    nsCOMPtr<Type> kfdg_t2 = this;
    nsCOMPtr<Type> kfdg_t3 = (this);

    nsCOMPtr<Type> kfdg_m1(p); // expected-error {{Unused "kungFuDeathGrip" 'nsCOMPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m1', or explicitly pass 'kfdg_m1' to `mozilla::Unused`}}
    nsCOMPtr<Type> kfdg_m2 = p; // expected-error {{Unused "kungFuDeathGrip" 'nsCOMPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m2', or explicitly pass 'kfdg_m2' to `mozilla::Unused`}}
    nsCOMPtr<Type> kfdg_m3(p);
    kfdg_m3.mPtr->f(nullptr, nullptr);
    nsCOMPtr<Type> kfdg_m4 = p;
    kfdg_m4.mPtr->f(nullptr, nullptr);

    nsCOMPtr<Type> kfdg_a1((already_AddRefed<Type>()));
    nsCOMPtr<Type> kfdg_a2 = already_AddRefed<Type>();

    nsCOMPtr<Type> kfdg_p1(param);
    nsCOMPtr<Type> kfdg_p2 = param;


    RefPtr<Type> never_referenced2;
    RefPtr<Type> kfdg_t4(this);
    RefPtr<Type> kfdg_t5 = this;
    RefPtr<Type> kfdg_t6 = (this);

    RefPtr<Type> kfdg_m5(p); // expected-error {{Unused "kungFuDeathGrip" 'RefPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m5', or explicitly pass 'kfdg_m5' to `mozilla::Unused`}}
    RefPtr<Type> kfdg_m6 = p; // expected-error {{Unused "kungFuDeathGrip" 'RefPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m6', or explicitly pass 'kfdg_m6' to `mozilla::Unused`}}
    RefPtr<Type> kfdg_m7(p);
    kfdg_m7.mPtr->f(nullptr, nullptr);
    RefPtr<Type> kfdg_m8 = p;
    kfdg_m8.mPtr->f(nullptr, nullptr);

    RefPtr<Type> kfdg_a3((already_AddRefed<Type>()));
    RefPtr<Type> kfdg_a4 = already_AddRefed<Type>();

    RefPtr<Type> kfdg_p3(param);
    RefPtr<Type> kfdg_p4 = param;
  }

  Type *p;
};

void f(nsCOMPtr<Type> ignoredArgument, Type *param) {
  nsCOMPtr<Type> never_referenced;
  Type t;
  // Type *p = nullptr;
  nsCOMPtr<Type> kfdg_m1(t.p); // expected-error {{Unused "kungFuDeathGrip" 'nsCOMPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m1', or explicitly pass 'kfdg_m1' to `mozilla::Unused`}}
  nsCOMPtr<Type> kfdg_m2 = t.p; // expected-error {{Unused "kungFuDeathGrip" 'nsCOMPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m2', or explicitly pass 'kfdg_m2' to `mozilla::Unused`}}
  nsCOMPtr<Type> kfdg_m3(t.p);
  kfdg_m3.mPtr->f(nullptr, nullptr);
  nsCOMPtr<Type> kfdg_m4 = t.p;
  kfdg_m4.mPtr->f(nullptr, nullptr);

  nsCOMPtr<Type> kfdg_a1((already_AddRefed<Type>()));
  nsCOMPtr<Type> kfdg_a2 = already_AddRefed<Type>();

  nsCOMPtr<Type> kfdg_p1(param);
  nsCOMPtr<Type> kfdg_p2 = param;


  RefPtr<Type> never_referenced2;
  RefPtr<Type> kfdg_m5(t.p); // expected-error {{Unused "kungFuDeathGrip" 'RefPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m5', or explicitly pass 'kfdg_m5' to `mozilla::Unused`}}
  RefPtr<Type> kfdg_m6 = t.p; // expected-error {{Unused "kungFuDeathGrip" 'RefPtr<Type>' objects constructed from members are prohibited}} expected-note {{Please switch all accesses to this member to go through 'kfdg_m6', or explicitly pass 'kfdg_m6' to `mozilla::Unused`}}
  RefPtr<Type> kfdg_m7(t.p);
  kfdg_m7.mPtr->f(nullptr, nullptr);
  RefPtr<Type> kfdg_m8 = t.p;
  kfdg_m8.mPtr->f(nullptr, nullptr);

  RefPtr<Type> kfdg_a3((already_AddRefed<Type>()));
  RefPtr<Type> kfdg_a4 = already_AddRefed<Type>();

  RefPtr<Type> kfdg_p3(param);
  RefPtr<Type> kfdg_p4 = param;
}

nsCOMPtr<Type> Type::someStaticCOMPtr(nullptr);
