what = 0;

function f(x) {
  g(x);
}

function g(x) {
  var a = <a></a>;
  eval("what = true");
}

f(10);
assertEq(what, true);
