function foo(x) {
  "use strict";
  var ok = false;
  try {
    x.a = 10;
  } catch(e) {
    ok = true;
  }
  assertEq(ok, true);
  assertEq(x.a, 0);
}
x = {a:0,b:1};
Object.freeze(x);
foo(x);
