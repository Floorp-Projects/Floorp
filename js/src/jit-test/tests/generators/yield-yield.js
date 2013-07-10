// Bug 880447

load(libdir + "asserts.js");

function f() {
    yield yield 1;
}

var g = f();
assertEq(g.next(), 1);
assertEq(g.send("hello"), "hello");
assertThrowsValue(() => g.next(), StopIteration);
