function bar(x, y) {
  return x + y;
}

function foo(x, y) {
  function closeOver() { return x; }
  var args = Array.prototype.slice.call(arguments);
  return bar(args[0], args[1]);
}

var sum = 0;
for (var i = 0; i < 100; i++) {
  sum += foo(1, 2);
}
assertEq(sum, 300)
