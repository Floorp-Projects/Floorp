// Test yield* with iter.next and monkeypatching.

function* g(n) { for (var i=0; i<n; i++) yield i; }
function* delegate(iter) { return yield* iter; }
var GeneratorObjectPrototype = Object.getPrototypeOf(g).prototype;
var GeneratorObjectPrototype_next = GeneratorObjectPrototype.next;

// Monkeypatch next on an iterator.
var inner = g(20);
var outer = delegate(inner);
assertIteratorResult(0, false, outer.next());
assertIteratorResult(1, false, outer.next());
inner.next = function() { return 0; };
// 42 yielded directly without re-boxing.
assertEq(0, outer.next());
// Outer generator not terminated.
assertEq(0, outer.next());
// Restore.
inner.next = GeneratorObjectPrototype_next;
assertIteratorResult(2, false, outer.next());
// Repatch.
inner.next = function() { return { value: 42, done: true }; };
assertIteratorResult(42, true, outer.next());

// Monkeypunch next on the prototype.
var inner = g(20);
var outer = delegate(inner);
assertIteratorResult(0, false, outer.next());
assertIteratorResult(1, false, outer.next());
GeneratorObjectPrototype.next = function() { return 0; };
// 42 yielded directly without re-boxing.
assertEq(0, GeneratorObjectPrototype_next.call(outer));
// Outer generator not terminated.
assertEq(0, GeneratorObjectPrototype_next.call(outer));
// Restore.
GeneratorObjectPrototype.next = GeneratorObjectPrototype_next;
assertIteratorResult(2, false, outer.next());

if (typeof reportCompare == "function")
    reportCompare(true, true);
