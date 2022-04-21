function foo(x,y) {
  function capture() { return x; }
  return bar(arguments);
}

function bar(x) {
  var args = Array.prototype.slice.call(x, 1, 3);
  return baz(args[0], args[1]) + arguments.length;
}

function baz(x,y) {
  return x + y;
}

var sum = 0;
for (var i = 0; i < 100; i++) {
  sum += foo(0, 1, 2, 3);
}
assertEq(sum, 400)
