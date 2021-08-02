load(libdir + "asserts.js");

function testReturn() {
  class C extends class {} {
    constructor() {
      for (;;) {
        return;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(), ReferenceError);
  }
}
testReturn();

function testReturnSuper() {
  class C extends class {} {
    constructor() {
      super();
      for (;;) {
        return;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    // No error.
    new C();
  }
}
testReturnSuper();

function testReturnPrimitive() {
  class C extends class {} {
    constructor() {
      for (;;) {
        return 0;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(), TypeError);
  }
}
testReturnPrimitive();

function testReturnPrimitiveSuper() {
  class C extends class {} {
    constructor() {
      super();
      for (;;) {
        return 0;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(), TypeError);
  }
}
testReturnPrimitiveSuper();
