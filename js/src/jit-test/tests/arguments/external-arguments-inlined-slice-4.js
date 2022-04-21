function foo() {
  return bar(arguments);
}
function bar(argsFromFoo) {
  var args = Array.prototype.slice.call(argsFromFoo, 1, 3);
  return baz(args[0], args[1]);
}
function baz(a,b) { return a + b; }

var sum = 0;
with ({}) {}
for (var i = 0; i < 100; i++) {
    sum += foo(0, 1, 2, 3);
}
assertEq(sum, 300);
