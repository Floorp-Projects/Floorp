function foo() {
  arguments.length = 2;
  var args = Array.prototype.slice.call(arguments);
  return bar(args);
}

function bar(args) {
  var result = 0;
  for (var x of args) {
    result += x;
  }
  return result;
}

var sum = 0;
for (var i = 0; i < 100; i++) {
  sum += foo(1, 2, 3, 4, 5, 6);
}
assertEq(sum, 300);
