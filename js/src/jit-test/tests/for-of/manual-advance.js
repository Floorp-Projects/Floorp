// Manually advancing the iterator.

load(libdir + 'iteration.js');

function* g(n) { for (var i=0; i<n; i++) yield i; }

var inner = g(20);

var n = 0;
for (var x of inner) {
    assertEq(x, n * 2);
    assertIteratorResult(inner.next(), n * 2 + 1, false);
    n++;
}
assertEq(n, 10);
