// Interactions between yield and array comprehensions.


function assertIteratorResult(result, value, done) {
    assertDeepEq(result.value, value);
    assertEq(result.done, done);
}

function* t1() { return [for (x of yield 0) x*2] }

var o = t1();
assertIteratorResult(o.next(), 0, false);
assertIteratorResult(o.next([0, 1, 2]), [0, 2, 4], true);

function* t2() { return [for (x of [1,2,3]) yield x] }

o = t2();
assertIteratorResult(o.next(), 1, false);
assertIteratorResult(o.next(5), 2, false);
assertIteratorResult(o.next(6), 3, false);
assertIteratorResult(o.next(7), [5, 6, 7], true);

reportCompare(null, null, "test");
