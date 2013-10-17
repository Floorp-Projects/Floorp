// With yield*, inner and outer iterators can be invoked separately.

function* g(n) { for (var i=0; i<n; i++) yield i; }
function* delegate(iter) { return yield* iter; }

var inner = g(20);
var outer1 = delegate(inner);
var outer2 = delegate(inner);

assertIteratorResult(outer1.next(), 0, false);
assertIteratorResult(outer2.next(), 1, false);
assertIteratorResult(inner.next(), 2, false);
assertIteratorResult(outer1.next(), 3, false);
assertIteratorResult(outer2.next(), 4, false);
assertIteratorResult(inner.next(), 5, false);

if (typeof reportCompare == "function")
    reportCompare(true, true);
