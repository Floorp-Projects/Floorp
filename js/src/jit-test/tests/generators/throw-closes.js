// When a generator function throws, the generator is closed.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

// Star generator, next() throws.
function *g() {
    yield 1;
    yield 2;
    throw 3;
    yield 4;
}
var i = g();
assertIteratorNext(i, 1);
assertIteratorNext(i, 2);
assertThrowsValue(() => i.next(), 3);
assertIteratorDone(i);
assertIteratorDone(i);

// Star generator, throw() throws.
function *h() {
    yield 1;
    yield 2;
}
var i = h();
assertIteratorNext(i, 1);
assertThrowsValue(() => i.throw(4), 4);
assertIteratorDone(i);

// Star generator, return() throws.
function *h2() {
    try {
	yield 1;
	yield 2;
    } finally {
	throw 6;
    }
}
var i = h2();
assertIteratorNext(i, 1);
assertThrowsValue(() => i.return(4), 6);
assertIteratorDone(i);

// Legacy generator, throw() throws.
function l1() {
    yield 1;
    yield 2;
}
var i = l1();
assertEq(i.next(), 1);
assertThrowsValue(() => i.throw(5), 5);
assertThrowsInstanceOf(() => i.next(), StopIteration);

// Legacy generator, next() throws.
function l2() {
    yield 1;
    throw 6;
    yield 2;
}
var i = l2();
assertEq(i.next(), 1);
assertThrowsValue(() => i.next(), 6);
assertThrowsInstanceOf(() => i.next(), StopIteration);
