setJitCompilerOption("inlining.bytecode-max-length", 200);

class Bar {
  constructor(x, y) {
    this.value = x + y;
  }
}

class Foo extends Bar {
  constructor(...args) {
    args[0] = 3;
    super(...args);
  }
}

// |Foo| must be small enough to be inlinable.
assertEq(isSmallFunction(Foo), true);

with ({}) {}

var sum = 0;
for (var i = 0; i < 100; i++) {
  let obj = new Foo(1, 2);
  sum += obj.value;
}
assertEq(sum, 500);
