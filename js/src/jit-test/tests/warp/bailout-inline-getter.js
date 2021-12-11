// |jit-test| --fast-warmup; --no-threads

var iter = 0;

class A {
  get foo() {
    var x = iter;
    bailout();
    return x;
  }
}

var a = new A();
function bar() {
  return a.foo;
}

with ({}) {}
for(var i = 0; i < 100; i++) {
  iter = i;
  assertEq(bar(), i);
}
