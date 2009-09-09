// bug 504797

function f() {
  g(arguments, 1);
}

function g(a, b) {
  var s = Array.prototype.slice.call(a, b);
  assertEq(s[0] == undefined, false);
}

for (var i = 0; i < 10; ++i) {
  f(1, 2, 3, 4);
}
