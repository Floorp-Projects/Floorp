
function bar() {
  foo.arguments.length = 10;
}

function foo(x) {
  var a = arguments;
  var n = 0;
  bar();
  assertEq(x, 5);
  assertEq(a.length, 10);
}

foo(5);
