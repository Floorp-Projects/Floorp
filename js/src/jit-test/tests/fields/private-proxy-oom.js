// |jit-test| skip-if: !('oomTest' in this);
// Check for proxy expando OOM issues.

function assertThrowsTypeError(f) {
  assertThrowsInstanceOf(f, TypeError);
}


function testing() {
  var target = {};
  var p1 = new Proxy(target, {});
  var p2 = new Proxy(target, {});

  class A extends class {
    constructor(o) {
      return o;
    }
  }
  {
    #field = 10;
    static gf(o) {
      return o.#field;
    }
    static sf(o) {
      o.#field = 15;
    }
  }

  // Verify field handling on the proxy we install it on.
  new A(p1);
  assertEq(A.gf(p1), 10);
  A.sf(p1)
  assertEq(A.gf(p1), 15);

  // Should't be on the target
  assertThrowsTypeError(() => A.gf(target));

  // Can't set the field, doesn't exist
  assertThrowsTypeError(() => A.sf(p2));

  // Definitely can't get the field, doesn't exist.
  assertThrowsTypeError(() => A.gf(p2));

  // Still should't be on the target.
  assertThrowsTypeError(() => A.gf(target));
}

oomTest(testing);