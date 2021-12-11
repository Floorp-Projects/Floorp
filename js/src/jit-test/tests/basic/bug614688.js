function Foo() {
  u = 0;
}

var x = new Foo();
assertEq(Object.getPrototypeOf(x) === Foo.prototype, true);
assertEq(Object.getPrototypeOf(x) === Object.prototype, false);
