// |jit-test| error: TypeError

/* GETPROP of a known constant where the lvalue may not be an object. */

function foo(ox) {
  var x = ox;
  var n = 0;
  for (var i = 0; i < 90; i++) {
    n += x.f.g;
    if (i >= 80)
      x = undefined;
  }
  print(n);
}
var n = 1;
function f() {}
function g() {}
g.g = 1;
f.prototype = {f:g};
foo(new f());
