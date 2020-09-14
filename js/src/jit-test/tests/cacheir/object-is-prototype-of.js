function testPrimitive() {
  for (var i = 0; i < 100; ++i) {
    // Null and undefined.
    assertEq(Object.prototype.isPrototypeOf(null), false);
    assertEq(Object.prototype.isPrototypeOf(void 0), false);

    // Primitive wrappers.
    assertEq(String.prototype.isPrototypeOf(""), false);
    assertEq(Number.prototype.isPrototypeOf(0), false);
    assertEq(Boolean.prototype.isPrototypeOf(true), false);
    assertEq(BigInt.prototype.isPrototypeOf(0n), false);
    assertEq(Symbol.prototype.isPrototypeOf(Symbol.hasInstance), false);
  }
}
testPrimitive();

function testObject() {
  for (var i = 0; i < 100; ++i) {
    assertEq(Object.prototype.isPrototypeOf({}), true);
    assertEq(Object.prototype.isPrototypeOf([]), true);

    assertEq(Array.prototype.isPrototypeOf({}), false);
    assertEq(Array.prototype.isPrototypeOf([]), true);
  }
}
testObject();

function testProxy() {
  var proxy = new Proxy({}, new Proxy({}, {
    get(t, pk, r) {
      assertEq(pk, "getPrototypeOf");
      return Reflect.get(t, pk, r);
    }
  }));

  for (var i = 0; i < 100; ++i) {
    assertEq(Object.prototype.isPrototypeOf(proxy), true);
    assertEq(Array.prototype.isPrototypeOf(proxy), false);
  }
}
testProxy();
