#include <mozilla/RefPtr.h>

#define MOZ_CAN_RUN_SCRIPT __attribute__((annotate("moz_can_run_script")))

MOZ_CAN_RUN_SCRIPT void test() {

}

void test_parent() { // expected-note {{parent function declared here}}
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

  virtual void method_test3() {
    test();
  }
};

void test2_parent() { // expected-note {{parent function declared here}}
  test2(new RefCountedBase); // expected-error {{arguments must all be strong refs or parent parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument)}} \
                             // expected-error {{functions marked as MOZ_CAN_RUN_SCRIPT can only be called from functions also marked as MOZ_CAN_RUN_SCRIPT}}
}

MOZ_CAN_RUN_SCRIPT void test2_parent2() {
  test2(new RefCountedBase); // expected-error {{arguments must all be strong refs or parent parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument)}}
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
  t->method_test(); // expected-error {{arguments must all be strong refs or parent parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument)}}
  t->method_test2(); // expected-error {{arguments must all be strong refs or parent parameters when calling a function marked as MOZ_CAN_RUN_SCRIPT (including the implicit object argument)}}
}

MOZ_CAN_RUN_SCRIPT void test3(int* param) {}

MOZ_CAN_RUN_SCRIPT void test3_parent() {
  test3(new int);
}

struct RefCountedChild : public RefCountedBase {
  virtual void method_test3() override;
};

void RefCountedChild::method_test3() {
  test();
}

struct RefCountedSubChild : public RefCountedChild {
  MOZ_CAN_RUN_SCRIPT void method_test3() override;
};

void RefCountedSubChild::method_test3() {
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