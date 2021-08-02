load(libdir + "asserts.js");

function testReturn() {
  class C extends class {} {
    constructor(n) {
      for (let i = 0; i < n; ++i) {
        return;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(1), ReferenceError);
  }
}
testReturn();

function testReturnSuper() {
  class C extends class {} {
    constructor(n) {
      super();
      for (let i = 0; i < n; ++i) {
        return;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    // No error.
    new C(1);
  }
}
testReturnSuper();

function testReturnPrimitive() {
  class C extends class {} {
    constructor(n) {
      for (let i = 0; i < n; ++i) {
        return 0;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(1), TypeError);
  }
}
testReturnPrimitive();

function testReturnPrimitiveSuper() {
  class C extends class {} {
    constructor(n) {
      super();
      for (let i = 0; i < n; ++i) {
        return 0;
      }
      assertEq(true, false, "unreachable");
    }
  }

  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(1), TypeError);
  }
}
testReturnPrimitiveSuper();
