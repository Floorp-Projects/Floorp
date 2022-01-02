setJitCompilerOption("inlining.bytecode-max-length", 200);

function escape() { with ({}) {} }

class Foo {
  constructor(i) {
    this.value = i;
  }
}

class Bar extends Foo {
  constructor(n, ...args) {
    escape(args);
    super(...args);
  }
}

// |Bar| must be small enough to be inlinable.
assertEq(isSmallFunction(Bar), true);

class Baz extends Bar {
  constructor(a, n) {
    super(n, n);
  }
}

var sum = 0;
for (var i = 0; i < 10000; i++) {
  let obj = new Baz(0, 1);
  sum += obj.value;
}
assertEq(sum, 10000);
