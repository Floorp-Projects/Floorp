#include <mozilla/RefPtr.h>
#include <mozilla/Maybe.h>

#define MOZ_CAN_RUN_SCRIPT __attribute__((annotate("moz_can_run_script")))
#define MOZ_CAN_RUN_SCRIPT_BOUNDARY __attribute__((annotate("moz_can_run_script_boundary")))

MOZ_CAN_RUN_SCRIPT void test() {

}

void test_parent() { // expected-note {{caller function declared here}}
  test(); // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT can only be called from functions also marked as MOZ_CAN_RUN_SCRIPT}}
}

MOZ_CAN_RUN_SCRIPT void test_parent2() {
  test();
}

struct RefCountedBase;
MOZ_CAN_RUN_SCRIPT void test2(RefCountedBase* param) {

}

struct RefCountedBase {
  void AddRef();
  void Release();

  MOZ_CAN_RUN_SCRIPT void method_test() {
    test();
  }

  MOZ_CAN_RUN_SCRIPT void method_test2() {
    test2(this);
  }

  virtual void method_test3() { // expected-note {{caller function declared here}}
    test(); // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT can only be called from functions also marked as MOZ_CAN_RUN_SCRIPT}}
  }

  MOZ_CAN_RUN_SCRIPT void method_test4() {
    method_test();
  }

  MOZ_CAN_RUN_SCRIPT void method_test5() {
    this->method_test();
  }
};

MOZ_CAN_RUN_SCRIPT void testLambda() {
  auto doIt = []() MOZ_CAN_RUN_SCRIPT {
    test();
  };

  auto doItWrong = []() { // expected-note {{caller function declared here}}
    test(); // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT can only be called from functions also marked as MOZ_CAN_RUN_SCRIPT}}
  };

  doIt();
  doItWrong();
}

void test2_parent() { // expected-note {{caller function declared here}}
  test2(new RefCountedBase); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'new RefCountedBase' is neither.}} \
                             // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT can only be called from functions also marked as MOZ_CAN_RUN_SCRIPT}}
}

MOZ_CAN_RUN_SCRIPT void test2_parent2() {
  test2(new RefCountedBase); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'new RefCountedBase' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test2_parent3(RefCountedBase* param) {
  test2(param);
}

MOZ_CAN_RUN_SCRIPT void test2_parent4() {
  RefPtr<RefCountedBase> refptr = new RefCountedBase;
  test2(refptr);
}

MOZ_CAN_RUN_SCRIPT void test2_parent5() {
  test2(MOZ_KnownLive(new RefCountedBase));
}

MOZ_CAN_RUN_SCRIPT void test2_parent6() {
  RefPtr<RefCountedBase> refptr = new RefCountedBase;
  refptr->method_test();
  refptr->method_test2();
}

MOZ_CAN_RUN_SCRIPT void test2_parent7() {
  RefCountedBase* t = new RefCountedBase;
  t->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  't' is neither.}}
  t->method_test2(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  't' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test2_parent8() {
  test2(nullptr);
}

MOZ_CAN_RUN_SCRIPT void test3(int* param) {}

MOZ_CAN_RUN_SCRIPT void test3_parent() {
  test3(new int);
}

struct RefCountedChild : public RefCountedBase {
  virtual void method_test3() override; // expected-note {{overridden function declared here}} expected-note {{overridden function declared here}} expected-note {{caller function declared here}}
};

void RefCountedChild::method_test3() {
  test(); // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT can only be called from functions also marked as MOZ_CAN_RUN_SCRIPT}}
}

struct RefCountedSubChild : public RefCountedChild {
  MOZ_CAN_RUN_SCRIPT void method_test3() override; // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT cannot override functions that are not marked MOZ_CAN_RUN_SCRIPT}}
};

void RefCountedSubChild::method_test3() { // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT cannot override functions that are not marked MOZ_CAN_RUN_SCRIPT}}
  test();
}

MOZ_CAN_RUN_SCRIPT void test4() {
  RefPtr<RefCountedBase> refptr1 = new RefCountedChild;
  refptr1->method_test3();

  RefPtr<RefCountedBase> refptr2 = new RefCountedSubChild;
  refptr2->method_test3();

  RefPtr<RefCountedChild> refptr3 = new RefCountedSubChild;
  refptr3->method_test3();

  RefPtr<RefCountedSubChild> refptr4 = new RefCountedSubChild;
  refptr4->method_test3();
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void test5() {
  RefPtr<RefCountedBase> refptr1 = new RefCountedChild;
  refptr1->method_test3();

  RefPtr<RefCountedBase> refptr2 = new RefCountedSubChild;
  refptr2->method_test3();

  RefPtr<RefCountedChild> refptr3 = new RefCountedSubChild;
  refptr3->method_test3();

  RefPtr<RefCountedSubChild> refptr4 = new RefCountedSubChild;
  refptr4->method_test3();
}

// We should be able to call test5 from a non-can_run_script function.
void test5_b() {
  test5();
}

MOZ_CAN_RUN_SCRIPT void test6() {
  void* x = new RefCountedBase();
  test2((RefCountedBase*)x); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'x' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_ref(const RefCountedBase&) {

}

MOZ_CAN_RUN_SCRIPT void test_ref_1() {
  RefCountedBase* t = new RefCountedBase;
  test_ref(*t); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*t' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_ref_2() {
  RefCountedBase* t = new RefCountedBase;
  (*t).method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*t' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_ref_3() {
  RefCountedBase* t = new RefCountedBase;
  auto& ref = *t;
  test_ref(ref); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'ref' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_ref_4() {
  RefCountedBase* t = new RefCountedBase;
  auto& ref = *t;
  ref.method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'ref' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_ref_5() {
  RefPtr<RefCountedBase> t = new RefCountedBase;
  test_ref(*t);
}

MOZ_CAN_RUN_SCRIPT void test_ref_6() {
  RefPtr<RefCountedBase> t = new RefCountedBase;
  (*t).method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ref_7() {
  RefPtr<RefCountedBase> t = new RefCountedBase;
  auto& ref = *t;
  MOZ_KnownLive(ref).method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ref_8() {
  RefPtr<RefCountedBase> t = new RefCountedBase;
  auto& ref = *t;
  test_ref(MOZ_KnownLive(ref));
}

MOZ_CAN_RUN_SCRIPT void test_ref_9() {
  void* x = new RefCountedBase();
  test_ref(*(RefCountedBase*)x); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*(RefCountedBase*)x' is neither.}}
}

// Ignore warning not related to static analysis here
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvoid-ptr-dereference"
MOZ_CAN_RUN_SCRIPT void test_ref_10() {
  void* x = new RefCountedBase();
  test_ref((RefCountedBase&)*x); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*x' is neither.}}
}
#pragma GCC diagnostic pop

MOZ_CAN_RUN_SCRIPT void test_maybe() {
  mozilla::Maybe<RefCountedBase*> unsafe;
  unsafe.emplace(new RefCountedBase);
  (*unsafe)->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*unsafe' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_maybe_2() {
  // FIXME(bz): This should not generate an error!
  mozilla::Maybe<RefPtr<RefCountedBase>> safe;
  safe.emplace(new RefCountedBase);
  (*safe)->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '(*safe)' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_defaults_helper_1(RefCountedBase* arg = nullptr) {
}

MOZ_CAN_RUN_SCRIPT void test_defaults_1() {
  test_defaults_helper_1();
}

MOZ_CAN_RUN_SCRIPT void test_defaults_2() {
  RefCountedBase* t = new RefCountedBase;
  test_defaults_helper_1(t); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  't' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_defaults_3() {
  RefPtr<RefCountedBase> t = new RefCountedBase;
  test_defaults_helper_1(t);
}

MOZ_CAN_RUN_SCRIPT void test_defaults_helper_2(RefCountedBase* arg = new RefCountedBase()) { // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'new RefCountedBase()' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_defaults_4() {
  test_defaults_helper_2();
}

MOZ_CAN_RUN_SCRIPT void test_defaults_5() {
  RefCountedBase* t = new RefCountedBase;
  test_defaults_helper_2(t); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  't' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_defaults_6() {
  RefPtr<RefCountedBase> t = new RefCountedBase;
  test_defaults_helper_2(t);
}

MOZ_CAN_RUN_SCRIPT void test_arg_deref_helper(RefCountedBase&) {
}

MOZ_CAN_RUN_SCRIPT void test_arg_deref(RefCountedBase* arg) {
  test_arg_deref_helper(*arg);
}

struct RefCountedDerefTester : public RefCountedBase {
  MOZ_CAN_RUN_SCRIPT void foo() {
    test_arg_deref_helper(*this);
  }
};

struct DisallowMemberArgs {
  RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mRefCounted->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}
  }
};

struct DisallowMemberArgsWithGet {
  RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mRefCounted.get()->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted.get()' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted.get()); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted.get()' is neither.}}
  }
};

struct AllowKnownLiveMemberArgs {
  RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    MOZ_KnownLive(mRefCounted)->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(MOZ_KnownLive(mRefCounted));
  }
};

struct WeakPtrReturner : public RefCountedBase {
  RefCountedBase* getWeakPtr() { return new RefCountedBase(); }
};

struct DisallowMemberCallsOnRandomKnownLive {
  RefPtr<WeakPtrReturner> mWeakPtrReturner1;
  WeakPtrReturner* mWeakPtrReturner2;

  MOZ_CAN_RUN_SCRIPT void test_refptr_method() {
    MOZ_KnownLive(mWeakPtrReturner1)->getWeakPtr()->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'MOZ_KnownLive(mWeakPtrReturner1)->getWeakPtr()' is neither.}}
  }

  MOZ_CAN_RUN_SCRIPT void test_refptr_function() {
    test2(MOZ_KnownLive(mWeakPtrReturner1)->getWeakPtr()); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'MOZ_KnownLive(mWeakPtrReturner1)->getWeakPtr()' is neither.}}
  }

  MOZ_CAN_RUN_SCRIPT void test_raw_method() {
    MOZ_KnownLive(mWeakPtrReturner2)->getWeakPtr()->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'MOZ_KnownLive(mWeakPtrReturner2)->getWeakPtr()' is neither.}}
  }

  MOZ_CAN_RUN_SCRIPT void test_raw_function() {
    test2(MOZ_KnownLive(mWeakPtrReturner2)->getWeakPtr()); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'MOZ_KnownLive(mWeakPtrReturner2)->getWeakPtr()' is neither.}}
  }
};

struct AllowConstMemberArgs {
  const RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mRefCounted->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted);
  }
};

struct AllowConstMemberArgsWithExplicitThis {
  const RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    this->mRefCounted->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(this->mRefCounted);
  }
};

struct DisallowConstMemberArgsOfMembers {
  RefPtr<AllowConstMemberArgs> mMember;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mMember->mRefCounted->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mMember->mRefCounted' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mMember->mRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mMember->mRefCounted' is neither.}}
  }
};

struct DisallowConstNonRefPtrMemberArgs {
  RefCountedBase* const mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mRefCounted->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}
  }
};

MOZ_CAN_RUN_SCRIPT void test_temporary_1() {
  RefPtr<RefCountedBase>(new RefCountedBase())->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_temporary_2() {
  test_ref(*RefPtr<RefCountedBase>(new RefCountedBase()));
}

struct WeakSmartPtr {
  RefCountedBase* member;

  explicit WeakSmartPtr(RefCountedBase* arg) : member(arg) {}

  RefCountedBase* operator->() const {
    return member;
  }

  RefCountedBase& operator*() const {
    return *member;
  }

  operator RefCountedBase*() const {
    return member;
  }
};

MOZ_CAN_RUN_SCRIPT void test_temporary_3() {
  WeakSmartPtr(new RefCountedBase())->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'WeakSmartPtr(new RefCountedBase())' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_temporary_4() {
  test_ref(*WeakSmartPtr(new RefCountedBase())); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*WeakSmartPtr(new RefCountedBase())' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_temporary_5() {
  test2(WeakSmartPtr(new RefCountedBase())); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'WeakSmartPtr(new RefCountedBase())' is neither.}}
}


template<typename T>
struct TArray {
  TArray() {
    mArray[0] = new RefCountedBase();
  }
  T& operator[](unsigned int index) { return mArray[index]; }
  T mArray[1];
};

struct DisallowRawTArrayElement {
  TArray<RefCountedBase*> mArray;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mArray[0]->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mArray[0]' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mArray[0]); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mArray[0]' is neither.}}
  }
};

struct DisallowRefPtrTArrayElement {
  TArray<RefPtr<RefCountedBase>> mArray;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mArray[0]->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mArray[0]' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mArray[0]); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mArray[0]' is neither.}}
  }
};

struct AllowConstexprMembers {
  static constexpr RefCountedBase* mRefCounted = nullptr;
  static constexpr RefCountedBase* mRefCounted2 = nullptr;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mRefCounted->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted);
  }
  MOZ_CAN_RUN_SCRIPT void baz() {
    test_ref(*mRefCounted);
  }
};

MOZ_CAN_RUN_SCRIPT void test_constexpr_1() {
  AllowConstexprMembers::mRefCounted->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_constexpr_2() {
  test2(AllowConstexprMembers::mRefCounted);
}

MOZ_CAN_RUN_SCRIPT void test_constexpr_3() {
  test_ref(*AllowConstexprMembers::mRefCounted);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_1(RefCountedBase* arg1, RefCountedBase* arg2) {
  (arg1 ? arg1 : arg2)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_2(RefCountedBase* arg1, RefCountedBase* arg2) {
  test2(arg1 ? arg1 : arg2);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_3(RefCountedBase* arg1, RefCountedBase& arg2) {
  (arg1 ? *arg1 : arg2).method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_4(RefCountedBase* arg1, RefCountedBase& arg2) {
  test_ref(arg1 ? *arg1 : arg2);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_5(RefCountedBase* arg) {
  RefPtr<RefCountedBase> local = new RefCountedBase();
  (arg ? arg : local.get())->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_6(RefCountedBase* arg) {
  RefPtr<RefCountedBase> local = new RefCountedBase();
  test2(arg ? arg : local.get());
}

MOZ_CAN_RUN_SCRIPT void test_ternary_7(RefCountedBase* arg) {
  RefPtr<RefCountedBase> local = new RefCountedBase();
  (arg ? *arg : *local).method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_8(RefCountedBase* arg) {
  RefPtr<RefCountedBase> local = new RefCountedBase();
  test_ref(arg ? *arg : *local);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_9(RefCountedBase* arg) {
  (arg ? arg : AllowConstexprMembers::mRefCounted)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_10(RefCountedBase* arg) {
  test2(arg ? arg : AllowConstexprMembers::mRefCounted);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_11(RefCountedBase* arg) {
  (arg ? *arg : *AllowConstexprMembers::mRefCounted).method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_12(RefCountedBase* arg) {
  test_ref(arg ? *arg : *AllowConstexprMembers::mRefCounted);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_13(RefCountedBase* arg1, RefCountedBase& arg2) {
  (arg1 ? arg1 : &arg2)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_44(RefCountedBase* arg1, RefCountedBase& arg2) {
  test2(arg1 ? arg1 : &arg2);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_13(bool arg) {
  (arg ?
   AllowConstexprMembers::mRefCounted :
   AllowConstexprMembers::mRefCounted2)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_14(bool arg) {
  test2(arg ?
	AllowConstexprMembers::mRefCounted :
	AllowConstexprMembers::mRefCounted2);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_15(bool arg) {
  (arg ?
   *AllowConstexprMembers::mRefCounted :
   *AllowConstexprMembers::mRefCounted2).method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_16(bool arg) {
  test_ref(arg ?
	   *AllowConstexprMembers::mRefCounted :
	   *AllowConstexprMembers::mRefCounted2);
}

MOZ_CAN_RUN_SCRIPT void test_pointer_to_ref_1(RefCountedBase& arg) {
  (&arg)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_pointer_to_ref_2(RefCountedBase& arg) {
  test2(&arg);
}
