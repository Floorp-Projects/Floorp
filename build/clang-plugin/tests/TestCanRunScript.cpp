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

  RefCountedBase*& paramRef = param;
  test2(paramRef);
}

MOZ_CAN_RUN_SCRIPT void test2_parent4() {
  RefPtr<RefCountedBase> refptr = new RefCountedBase;
  test2(refptr);
  RefPtr<RefCountedBase>& refptrRef = refptr;
  test2(refptrRef);
  const RefPtr<RefCountedBase>& refptrConstRef = refptr;
  test2(refptrConstRef);

  RefPtr<RefCountedBase> refptrOther = refptr;
  RefPtr<RefCountedBase>& refptrRef2 = refptr ? refptr : refptrOther;
  test2(refptrRef2);

  RefPtr<RefCountedBase>* refPtrInHeap = new RefPtr<RefCountedBase>;
  RefPtr<RefCountedBase>& refptrRefUnsafe1 = *refPtrInHeap;
  test2(refptrRefUnsafe1); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe1' is neither.}}

  RefPtr<RefCountedBase>& refptrRefUnsafe2 = refPtrInHeap ? *refPtrInHeap : refptr;
  test2(refptrRefUnsafe2); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe2' is neither.}}

  RefPtr<RefCountedBase>& refptrRefUnsafe3 = refPtrInHeap ? refptr : *refPtrInHeap;
  test2(refptrRefUnsafe3); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe3' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test2_parent5() {
  test2(MOZ_KnownLive(new RefCountedBase));
}

MOZ_CAN_RUN_SCRIPT void test2_parent6() {
  RefPtr<RefCountedBase> refptr = new RefCountedBase;
  refptr->method_test();
  refptr->method_test2();
  RefPtr<RefCountedBase>& refptrRef = refptr;
  refptrRef->method_test();
  refptrRef->method_test2();

  RefPtr<RefCountedBase> refptrOther = refptr;
  RefPtr<RefCountedBase>& refptrRef2 = refptr ? refptr : refptrOther;
  refptrRef2->method_test();
  refptrRef2->method_test2();

  RefPtr<RefCountedBase>* refPtrInHeap = new RefPtr<RefCountedBase>;
  RefPtr<RefCountedBase>& refptrRefUnsafe1 = *refPtrInHeap;
  refptrRefUnsafe1->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe1->' is neither.}}
  refptrRefUnsafe1->method_test2(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe1->' is neither.}}

  RefPtr<RefCountedBase>& refptrRefUnsafe2 = refPtrInHeap ? *refPtrInHeap : refptr;
  refptrRefUnsafe2->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe2->' is neither.}}
  refptrRefUnsafe2->method_test2(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe2->' is neither.}}

  RefPtr<RefCountedBase>& refptrRefUnsafe3 = refPtrInHeap ? refptr : *refPtrInHeap;
  refptrRefUnsafe3->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe3->' is neither.}}
  refptrRefUnsafe3->method_test2(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refptrRefUnsafe3->' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test2_parent7() {
  RefCountedBase* t = new RefCountedBase;
  t->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  't' is neither.}}
  t->method_test2(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  't' is neither.}}

  RefCountedBase*& tRef = t;
  tRef->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'tRef' is neither.}}
  tRef->method_test2(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'tRef' is neither.}}
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

MOZ_CAN_RUN_SCRIPT void test_maybe() {
  mozilla::Maybe<RefCountedBase*> unsafe;
  unsafe.emplace(new RefCountedBase);
  (*unsafe)->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*unsafe' is neither.}}
}

MOZ_CAN_RUN_SCRIPT void test_maybe_2() {
  // FIXME(bz): This should not generate an error!
  mozilla::Maybe<RefPtr<RefCountedBase>> safe;
  safe.emplace(new RefCountedBase);
  (*safe)->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '(*safe){{(->)?}}' is neither.}}
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
    mRefCounted->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted{{(->)?}}' is neither.}}

    RefPtr<RefCountedBase>& unsafeMemberRef = mRefCounted;
    unsafeMemberRef->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'unsafeMemberRef{{(->)?}}' is neither.}}

    RefPtr<RefCountedBase> safeRefCounted = mRefCounted;
    RefPtr<RefCountedBase>& maybeUnsafeMemberRef1 = mRefCounted ? mRefCounted : safeRefCounted;
    maybeUnsafeMemberRef1->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef1{{(->)?}}' is neither.}}
    RefPtr<RefCountedBase>& maybeUnsafeMemberRef2 = safeRefCounted ? safeRefCounted : mRefCounted;
    maybeUnsafeMemberRef2->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef2{{(->)?}}' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}

    RefPtr<RefCountedBase>& unsafeMemberRef = mRefCounted;
    test2(unsafeMemberRef); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'unsafeMemberRef' is neither.}}

    RefPtr<RefCountedBase> safeRefCounted = mRefCounted;
    RefPtr<RefCountedBase>& maybeUnsafeMemberRef1 = mRefCounted ? mRefCounted : safeRefCounted;
    test2(maybeUnsafeMemberRef1); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef1' is neither.}}
    RefPtr<RefCountedBase>& maybeUnsafeMemberRef2 = safeRefCounted ? safeRefCounted : mRefCounted;
    test2(maybeUnsafeMemberRef2); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef2' is neither.}}
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

    RefPtr<RefCountedBase>& unsafeMemberRef = mRefCounted;
    MOZ_KnownLive(unsafeMemberRef)->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(MOZ_KnownLive(mRefCounted));

    RefPtr<RefCountedBase>& unsafeMemberRef = mRefCounted;
    test2(MOZ_KnownLive(unsafeMemberRef));
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

    const RefPtr<RefCountedBase>& safeMemberRef1 = mRefCounted;
    safeMemberRef1->method_test();

    const RefPtr<RefCountedBase> safeRefCounted = new RefCountedBase;
    const RefPtr<RefCountedBase>& safeMemberRef2 = safeRefCounted ? safeRefCounted : mRefCounted;
    safeMemberRef2->method_test();
    const RefPtr<RefCountedBase>& safeMemberRef3 = mRefCounted ? mRefCounted : safeRefCounted;
    safeMemberRef3->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted);

    const RefPtr<RefCountedBase>& safeMemberRef1 = mRefCounted;
    test2(safeMemberRef1);

    const RefPtr<RefCountedBase> safeRefCounted = new RefCountedBase;
    const RefPtr<RefCountedBase>& safeMemberRef2 = safeRefCounted ? safeRefCounted : mRefCounted;
    test2(safeMemberRef2);
    const RefPtr<RefCountedBase>& safeMemberRef3 = mRefCounted ? mRefCounted : safeRefCounted;
    test2(safeMemberRef3);
  }
};

struct AllowConstMemberArgsWithExplicitThis {
  const RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    this->mRefCounted->method_test();

    const RefPtr<RefCountedBase>& safeMemberRef1 = this->mRefCounted;
    safeMemberRef1->method_test();

    const RefPtr<RefCountedBase> safeRefCounted = new RefCountedBase;
    const RefPtr<RefCountedBase>& safeMemberRef2 = safeRefCounted ? safeRefCounted : this->mRefCounted;
    safeMemberRef2->method_test();
    const RefPtr<RefCountedBase>& safeMemberRef3 = this->mRefCounted ? this->mRefCounted : safeRefCounted;
    safeMemberRef3->method_test();
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(this->mRefCounted);

    const RefPtr<RefCountedBase>& safeMemberRef1 = this->mRefCounted;
    test2(safeMemberRef1);

    const RefPtr<RefCountedBase> safeRefCounted = new RefCountedBase;
    const RefPtr<RefCountedBase>& safeMemberRef2 = safeRefCounted ? safeRefCounted : this->mRefCounted;
    test2(safeMemberRef2);
    const RefPtr<RefCountedBase>& safeMemberRef3 = this->mRefCounted ? this->mRefCounted : safeRefCounted;
    test2(safeMemberRef3);
  }
};

struct DisallowConstMemberArgsOfMembers {
  RefPtr<AllowConstMemberArgs> mMember;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mMember->mRefCounted->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mMember->mRefCounted{{(->)?}}' is neither.}}

    const RefPtr<RefCountedBase>& unsafeMemberRef = mMember->mRefCounted;
    unsafeMemberRef->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'unsafeMemberRef{{(->)?}}' is neither.}}

    const RefPtr<RefCountedBase> safeRefCounted = new RefCountedBase;
    const RefPtr<RefCountedBase>& maybeUnsafeMemberRef1 = safeRefCounted ? safeRefCounted : mMember->mRefCounted;
    maybeUnsafeMemberRef1->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef1{{(->)?}}' is neither.}}
    const RefPtr<RefCountedBase>& maybeUnsafeMemberRef2 = mMember->mRefCounted ? mMember->mRefCounted : safeRefCounted;
    maybeUnsafeMemberRef2->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef2{{(->)?}}' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mMember->mRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mMember->mRefCounted' is neither.}}

    const RefPtr<RefCountedBase>& unsafeMemberRef = mMember->mRefCounted;
    test2(unsafeMemberRef); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'unsafeMemberRef' is neither.}}

    const RefPtr<RefCountedBase> safeRefCounted = new RefCountedBase;
    const RefPtr<RefCountedBase>& maybeUnsafeMemberRef1 = safeRefCounted ? safeRefCounted : mMember->mRefCounted;
    test2(maybeUnsafeMemberRef1); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef1' is neither.}}
    const RefPtr<RefCountedBase>& maybeUnsafeMemberRef2 = mMember->mRefCounted ? mMember->mRefCounted : safeRefCounted;
    test2(maybeUnsafeMemberRef2); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'maybeUnsafeMemberRef2' is neither.}}
  }
};

struct DisallowConstNonRefPtrMemberArgs {
  RefCountedBase* const mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    mRefCounted->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}

    RefCountedBase* const& unsafeMemberRefCounted = mRefCounted;
    unsafeMemberRefCounted->method_test(); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'unsafeMemberRefCounted' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    test2(mRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mRefCounted' is neither.}}

    RefCountedBase* const& unsafeMemberRefCounted = mRefCounted;
    test2(unsafeMemberRefCounted); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'unsafeMemberRefCounted' is neither.}}
  }
};

MOZ_CAN_RUN_SCRIPT void test_temporary_1() {
#ifdef MOZ_CLANG_PLUGIN_ALPHA
  RefPtr<RefCountedBase>(new RefCountedBase())->method_test(); // expected-warning {{performance issue: temporary 'RefPtr<RefCountedBase>' is only dereferenced here once which involves short-lived AddRef/Release calls}}
#else
  RefPtr<RefCountedBase>(new RefCountedBase())->method_test();
#endif
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
  WeakSmartPtr(new RefCountedBase())->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'WeakSmartPtr(new RefCountedBase()){{(->)?}}' is neither.}}
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
    mArray[0]->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mArray[0]{{(->)?}}' is neither.}}
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
  RefCountedBase*& safeArg = arg1 ? arg1 : arg2;
  safeArg->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_2(RefCountedBase* arg1, RefCountedBase* arg2) {
  test2(arg1 ? arg1 : arg2);
  RefCountedBase*& safeArg = arg1 ? arg1 : arg2;
  test2(safeArg);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_3(RefCountedBase* arg1, RefCountedBase& arg2) {
  (arg1 ? *arg1 : arg2).method_test();
  RefCountedBase& safeArg = arg1 ? *arg1 : arg2;
  safeArg.method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_4(RefCountedBase* arg1, RefCountedBase& arg2) {
  test_ref(arg1 ? *arg1 : arg2);
  RefCountedBase& safeArg = arg1 ? *arg1 : arg2;
  test_ref(safeArg);
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
  RefCountedBase& safeArgOrLocal = arg ? *arg : *local;
  safeArgOrLocal.method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_8(RefCountedBase* arg) {
  RefPtr<RefCountedBase> local = new RefCountedBase();
  test_ref(arg ? *arg : *local);
  RefCountedBase& safeArgOrLocal = arg ? *arg : *local;
  test_ref(safeArgOrLocal);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_9(RefCountedBase* arg) {
  (arg ? arg : AllowConstexprMembers::mRefCounted)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_10(RefCountedBase* arg) {
  test2(arg ? arg : AllowConstexprMembers::mRefCounted);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_11(RefCountedBase* arg) {
  (arg ? *arg : *AllowConstexprMembers::mRefCounted).method_test();
  RefCountedBase& safeArgOrMember = arg ? *arg : *AllowConstexprMembers::mRefCounted;
  safeArgOrMember.method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_12(RefCountedBase* arg) {
  test_ref(arg ? *arg : *AllowConstexprMembers::mRefCounted);
  RefCountedBase& safeArgOrMember = arg ? *arg : *AllowConstexprMembers::mRefCounted;
  test_ref(safeArgOrMember);
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
  RefCountedBase* const& safeConstexprMember = arg ? AllowConstexprMembers::mRefCounted : AllowConstexprMembers::mRefCounted2;
  safeConstexprMember->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_14(bool arg) {
  test2(arg ?
	AllowConstexprMembers::mRefCounted :
	AllowConstexprMembers::mRefCounted2);
  RefCountedBase* const& safeConstexprMember = arg ? AllowConstexprMembers::mRefCounted : AllowConstexprMembers::mRefCounted2;
  test2(safeConstexprMember);
}

MOZ_CAN_RUN_SCRIPT void test_ternary_15(bool arg) {
  (arg ?
   *AllowConstexprMembers::mRefCounted :
   *AllowConstexprMembers::mRefCounted2).method_test();
  RefCountedBase& safeConstexprMember = arg ? *AllowConstexprMembers::mRefCounted : *AllowConstexprMembers::mRefCounted2;
  safeConstexprMember.method_test();
}

MOZ_CAN_RUN_SCRIPT void test_ternary_16(bool arg) {
  test_ref(arg ?
	   *AllowConstexprMembers::mRefCounted :
	   *AllowConstexprMembers::mRefCounted2);
  RefCountedBase& safeConstexprMember = arg ? *AllowConstexprMembers::mRefCounted : *AllowConstexprMembers::mRefCounted2;
  test_ref(safeConstexprMember);
}

MOZ_CAN_RUN_SCRIPT void test_pointer_to_ref_1(RefCountedBase& arg) {
  (&arg)->method_test();
}

MOZ_CAN_RUN_SCRIPT void test_pointer_to_ref_2(RefCountedBase& arg) {
  test2(&arg);
}

struct DisallowMemberArgsViaReferenceAlias {
  RefPtr<RefCountedBase> mRefCounted;
  MOZ_CAN_RUN_SCRIPT void foo() {
    RefPtr<RefCountedBase>& bogus = mRefCounted;
    bogus->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'bogus{{(->)?}}' is neither.}}
  }
  MOZ_CAN_RUN_SCRIPT void bar() {
    RefPtr<RefCountedBase>& bogus = mRefCounted;
    test2(bogus); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'bogus' is neither.}}
  }
};

struct DisallowMemberArgsViaReferenceAlias2 {
  RefPtr<RefCountedBase> mRefCountedArr[2];
  MOZ_CAN_RUN_SCRIPT void foo1() {
    for (RefPtr<RefCountedBase>& item : mRefCountedArr) {
      item->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'item{{(->)?}}' is neither.}}
    }
  }
  MOZ_CAN_RUN_SCRIPT void foo2() {
    for (auto& item : mRefCountedArr) {
      item->method_test(); // expected-error-re {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'item{{(->)?}}' is neither.}}
    }
  }
  MOZ_CAN_RUN_SCRIPT void foo3() {
    for (RefPtr<RefCountedBase> item : mRefCountedArr) {
      item->method_test();
    }
  }
  MOZ_CAN_RUN_SCRIPT void foo4() {
    for (auto item : mRefCountedArr) {
      item->method_test();
    }
  }
  MOZ_CAN_RUN_SCRIPT void bar1() {
    for (RefPtr<RefCountedBase>& item : mRefCountedArr) {
      test2(item); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'item' is neither.}}
    }
  }
  MOZ_CAN_RUN_SCRIPT void bar2() {
    for (auto& item : mRefCountedArr) {
      test2(item); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'item' is neither.}}
    }
  }
  MOZ_CAN_RUN_SCRIPT void bar3() {
    for (RefPtr<RefCountedBase> item : mRefCountedArr) {
      test2(item);
    }
  }
  MOZ_CAN_RUN_SCRIPT void bar4() {
    for (auto item : mRefCountedArr) {
      test2(item);
    }
  }
};

struct AllowMozKnownLiveMember {
public:
  MOZ_KNOWN_LIVE RefCountedBase* mWhatever;
  MOZ_KNOWN_LIVE RefPtr<RefCountedBase> mRefCountedWhatever;
  MOZ_CAN_RUN_SCRIPT void fooPtr(RefCountedBase* aWhatever) {}
  MOZ_CAN_RUN_SCRIPT void fooRef(RefCountedBase& aWhatever) {}
  MOZ_CAN_RUN_SCRIPT void bar() {
    fooPtr(mWhatever);
    fooRef(*mWhatever);
    fooPtr(mRefCountedWhatever);
    fooRef(*mRefCountedWhatever);

    RefCountedBase*& whateverRef = mWhatever;
    fooPtr(whateverRef);
    fooRef(*whateverRef);
    RefPtr<RefCountedBase>&  refCountedWhateverRef = mRefCountedWhatever;
    fooPtr(refCountedWhateverRef);
    fooRef(*refCountedWhateverRef);
  }
};

struct AllowMozKnownLiveMemberParent : AllowMozKnownLiveMember {
  MOZ_CAN_RUN_SCRIPT void baz() {
    fooPtr(mWhatever);
    fooRef(*mWhatever);
    fooPtr(mRefCountedWhatever);
    fooRef(*mRefCountedWhatever);

    RefCountedBase*& whateverRef = mWhatever;
    fooPtr(whateverRef);
    fooRef(*whateverRef);
    RefPtr<RefCountedBase>&  refCountedWhateverRef = mRefCountedWhatever;
    fooPtr(refCountedWhateverRef);
    fooRef(*refCountedWhateverRef);
  }
};

struct AllowMozKnownLiveParamMember {
public:
  MOZ_CAN_RUN_SCRIPT void foo(AllowMozKnownLiveMember& aAllow) {
    aAllow.fooPtr(aAllow.mWhatever);
    aAllow.fooRef(*aAllow.mWhatever);
    aAllow.fooPtr(aAllow.mRefCountedWhatever);
    aAllow.fooRef(*aAllow.mRefCountedWhatever);

    RefCountedBase*& whateverRef = aAllow.mWhatever;
    aAllow.fooPtr(whateverRef);
    aAllow.fooRef(*whateverRef);
    RefPtr<RefCountedBase>&  refCountedWhateverRef = aAllow.mRefCountedWhatever;
    aAllow.fooPtr(refCountedWhateverRef);
    aAllow.fooRef(*refCountedWhateverRef);
  }
  MOZ_CAN_RUN_SCRIPT void bar(AllowMozKnownLiveMemberParent& aAllowParent) {
    aAllowParent.fooPtr(aAllowParent.mWhatever);
    aAllowParent.fooRef(*aAllowParent.mWhatever);
    aAllowParent.fooPtr(aAllowParent.mRefCountedWhatever);
    aAllowParent.fooRef(*aAllowParent.mRefCountedWhatever);

    RefCountedBase*& whateverRef = aAllowParent.mWhatever;
    aAllowParent.fooPtr(whateverRef);
    aAllowParent.fooRef(*whateverRef);
    RefPtr<RefCountedBase>&  refCountedWhateverRef = aAllowParent.mRefCountedWhatever;
    aAllowParent.fooPtr(refCountedWhateverRef);
    aAllowParent.fooRef(*refCountedWhateverRef);
  }
};

MOZ_CAN_RUN_SCRIPT void AllowMozKnownLiveMemberInAutoStorage() {
  AllowMozKnownLiveMember inStack;
  AllowMozKnownLiveMember* inHeap = new AllowMozKnownLiveMember();
  inStack.fooPtr(inStack.mWhatever);
  inStack.fooRef(*inStack.mWhatever);
  inStack.fooPtr(inStack.mRefCountedWhatever);
  inStack.fooRef(*inStack.mRefCountedWhatever);
  RefCountedBase*& whateverRefInStack = inStack.mWhatever;
  inStack.fooPtr(whateverRefInStack);
  inStack.fooRef(*whateverRefInStack);
  RefPtr<RefCountedBase>&  refCountedWhateverRefInStack = inStack.mRefCountedWhatever;
  inStack.fooPtr(refCountedWhateverRefInStack);
  inStack.fooRef(*refCountedWhateverRefInStack);

  inStack.fooPtr(inHeap->mWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'inHeap->mWhatever' is neither.}}
  inStack.fooRef(*inHeap->mWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*inHeap->mWhatever' is neither.}}
  inStack.fooPtr(inHeap->mRefCountedWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'inHeap->mRefCountedWhatever' is neither.}}
  inStack.fooRef(*inHeap->mRefCountedWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*inHeap->mRefCountedWhatever' is neither.}}
  RefCountedBase*& whateverRefInHeap = inHeap->mWhatever;
  inStack.fooPtr(whateverRefInHeap); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'whateverRefInHeap' is neither.}}
  inStack.fooRef(*whateverRefInHeap); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*whateverRefInHeap' is neither.}}
  RefPtr<RefCountedBase>&  refCountedWhateverRefInHeap = inHeap->mRefCountedWhatever;
  inStack.fooPtr(refCountedWhateverRefInHeap); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refCountedWhateverRefInHeap' is neither.}}
  inStack.fooRef(*refCountedWhateverRefInHeap); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*refCountedWhateverRefInHeap' is neither.}}
}

struct DisallowMozKnownLiveMemberNotFromKnownLive {
  AllowMozKnownLiveMember* mMember;
  MOZ_CAN_RUN_SCRIPT void fooPtr(RefCountedBase* aWhatever) {}
  MOZ_CAN_RUN_SCRIPT void fooRef(RefCountedBase& aWhatever) {}
  MOZ_CAN_RUN_SCRIPT void bar() {
    fooPtr(mMember->mWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mMember->mWhatever' is neither.}}
    fooRef(*mMember->mWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*mMember->mWhatever' is neither.}}
    fooPtr(mMember->mRefCountedWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'mMember->mRefCountedWhatever' is neither.}}
    fooRef(*mMember->mRefCountedWhatever); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*mMember->mRefCountedWhatever' is neither.}}
    RefCountedBase*& whateverRef = mMember->mWhatever;
    fooPtr(whateverRef); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'whateverRef' is neither.}}
    fooRef(*whateverRef); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*whateverRef' is neither.}}
    RefPtr<RefCountedBase>& refCountedWhateverRef = mMember->mRefCountedWhatever;
    fooPtr(refCountedWhateverRef); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  'refCountedWhateverRef' is neither.}}
    fooRef(*refCountedWhateverRef); // expected-error {{arguments must all be strong refs or caller's parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument).  '*refCountedWhateverRef' is neither.}}
  }
};

void IncorrectlyUnmarkedEarlyDeclaration(); // expected-note {{The first declaration exists here}}

MOZ_CAN_RUN_SCRIPT void IncorrectlyUnmarkedEarlyDeclaration() {}; // expected-error {{MOZ_CAN_RUN_SCRIPT must be put in front of the declaration, not the definition}}
