function foo() {
  return bar(arguments);
}
function bar(argsFromFoo) {
  return baz.apply({}, argsFromFoo)
}
function baz(a,b) { return a + b; }

var sum = 0;
with ({}) {}
for (var i = 0; i < 100; i++) {
    sum += foo(1,2);
}
assertEq(sum, 300);
