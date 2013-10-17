// Test yield* with iter.next and monkeypatching.

function* g(n) { for (var i=0; i<n; i++) yield i; }
function* delegate(iter) { return yield* iter; }
var GeneratorObjectPrototype = Object.getPrototypeOf(g).prototype;
var GeneratorObjectPrototype_next = GeneratorObjectPrototype.next;

// Monkeypatch next on an iterator.
var inner = g(20);
var outer = delegate(inner);
assertIteratorResult(outer.next(), 0, false);
assertIteratorResult(outer.next(), 1, false);
inner.next = function() { return 0; };
// 42 yielded directly without re-boxing.
assertEq(0, outer.next());
// Outer generator not terminated.
assertEq(0, outer.next());
// Restore.
inner.next = GeneratorObjectPrototype_next;
assertIteratorResult(outer.next(), 2, false);
// Repatch.
inner.next = function() { return { value: 42, done: true }; };
assertIteratorResult(outer.next(), 42, true);

// Monkeypunch next on the prototype.
var inner = g(20);
var outer = delegate(inner);
assertIteratorResult(outer.next(), 0, false);
assertIteratorResult(outer.next(), 1, false);
GeneratorObjectPrototype.next = function() { return 0; };
// 42 yielded directly without re-boxing.
assertEq(0, GeneratorObjectPrototype_next.call(outer));
// Outer generator not terminated.
assertEq(0, GeneratorObjectPrototype_next.call(outer));
// Restore.
GeneratorObjectPrototype.next = GeneratorObjectPrototype_next;
assertIteratorResult(outer.next(), 2, false);

if (typeof reportCompare == "function")
    reportCompare(true, true);
