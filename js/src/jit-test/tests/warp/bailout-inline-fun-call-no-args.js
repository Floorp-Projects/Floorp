// |jit-test| --fast-warmup; --no-threads

var iter = 0;

function foo() {
  var x = iter;
  bailout();
  return x;
}

function bar(x) {
  return foo.call();
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  iter = i;
  assertEq(bar(), i);
}
