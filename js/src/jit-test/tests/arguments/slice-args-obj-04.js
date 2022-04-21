var result;

function g(a, b) {
  with ({}) {}
  result = a + b;
}

function escape() { with({}) {} }

function f() {
  escape(arguments);
  for (var i = 0; i < 50; ++i) {
    var args = Array.prototype.slice.call(arguments);
    g(args[0], args[1]);
  }
}

f(1, 2);
assertEq(result, 3);

f("");
assertEq(result, "undefined");
