// arrow functions are not implicitly strict-mode code

load(libdir + "asserts.js");

var f = a => { with (a) return f(); };
assertEq(f({f: () => 7}), 7);

f = a => function () { with (a) return f(); };
assertEq(f({f: () => 7})(), 7);

f = (a = {x: 1, x: 2}) => b => { "use strict"; return a.x; };
assertEq(f()(0), 2);

