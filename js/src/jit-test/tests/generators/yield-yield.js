// Bug 880447

load(libdir + "asserts.js");

function* f() {
    yield yield 1;
}

var g = f();
assertEq(g.next().value, 1);
assertEq(g.return("hello").value, "hello");
assertEq(g.next().value, undefined);
