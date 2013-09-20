// With yield*, inner and outer iterators can be invoked separately.

function* g(n) { for (var i=0; i<n; i++) yield i; }
function* delegate(iter) { return yield* iter; }

var inner = g(20);
var outer1 = delegate(inner);
var outer2 = delegate(inner);

assertIteratorResult(0, false, outer1.next());
assertIteratorResult(1, false, outer2.next());
assertIteratorResult(2, false, inner.next());
assertIteratorResult(3, false, outer1.next());
assertIteratorResult(4, false, outer2.next());
assertIteratorResult(5, false, inner.next());

if (typeof reportCompare == "function")
    reportCompare(true, true);
