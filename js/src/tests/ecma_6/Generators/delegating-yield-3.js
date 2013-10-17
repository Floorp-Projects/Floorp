// Test yield* with iter.next and monkeypatching.

function* g(n) { for (var i=0; i<n; i++) yield i; }
function* delegate(iter) { return yield* iter; }
var GeneratorObjectPrototype = Object.getPrototypeOf(g).prototype;
var GeneratorObjectPrototype_next = GeneratorObjectPrototype.next;

// Monkeypatch next on an iterator.
var inner = g(20);
var outer = delegate(inner);
assertIteratorNext(outer, 0);
assertIteratorNext(outer, 1);
inner.next = function() { return 0; };
// 42 yielded directly without re-boxing.
assertEq(0, outer.next());
// Outer generator not terminated.
assertEq(0, outer.next());
// Restore.
inner.next = GeneratorObjectPrototype_next;
assertIteratorNext(outer, 2);
// Repatch.
inner.next = function() { return { value: 42, done: true }; };
assertIteratorDone(outer, 42);

// Monkeypunch next on the prototype.
var inner = g(20);
var outer = delegate(inner);
assertIteratorNext(outer, 0);
assertIteratorNext(outer, 1);
GeneratorObjectPrototype.next = function() { return 0; };
// 42 yielded directly without re-boxing.
assertEq(0, GeneratorObjectPrototype_next.call(outer));
// Outer generator not terminated.
assertEq(0, GeneratorObjectPrototype_next.call(outer));
// Restore.
GeneratorObjectPrototype.next = GeneratorObjectPrototype_next;
assertIteratorNext(outer, 2);

if (typeof reportCompare == "function")
    reportCompare(true, true);
