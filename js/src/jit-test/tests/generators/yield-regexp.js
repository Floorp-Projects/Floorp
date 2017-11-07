// Bug 1099956

load(libdir + "asserts.js");

// Parses as IDENT(yield) DIV IDENT(abc) DIV IDENT(g).
eval(`function f1() { yield /abc/g; }`);

// Throws a ReferenceError because no global "yield" variable is defined.
var ex;
try {
  f1();
} catch(e) {
  ex = e;
}
assertEq(ex instanceof ReferenceError, true);

// Parses as YIELD REGEXP(/abc/g).
function* f2() {
  yield /abc/g;
}

g = f2();
v = g.next();
assertEq(v.done, false);
assertEq(v.value instanceof RegExp, true);
assertEq(v.value.toString(), "/abc/g");
v = g.next();
assertEq(v.done, true);
