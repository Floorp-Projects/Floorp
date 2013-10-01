// Test for-of with iter.next and monkeypatching.

function* g(n) { for (var i=0; i<n; i++) yield i; }
var GeneratorObjectPrototype = Object.getPrototypeOf(g).prototype;
var GeneratorObjectPrototype_next = GeneratorObjectPrototype.next;

// Monkeypatch next on an iterator.
var inner = g(20);
var n = 0;
for (let x of inner) {
    if (n == 0) {
        assertEq(x, n++);
    } else if (n == 1) {
        assertEq(x, n++);
        inner.next = function() { return { value: 42, done: false }; };
    } else if (n == 2) {
        assertEq(x, 42);
        inner.next = function() { return { value: 100, done: true }; };
    } else
        throw 'not reached';
}

// Monkeypatch next on the prototype.
var inner = g(20);
var n = 0;
for (let x of inner) {
    if (n == 0) {
        assertEq(x, n++);
    } else if (n == 1) {
        assertEq(x, n++);
        GeneratorObjectPrototype.next =
            function() { return { value: 42, done: false }; };
    } else if (n == 2) {
        n++;
        assertEq(x, 42);
        GeneratorObjectPrototype.next = GeneratorObjectPrototype_next;
    } else if (n <= 20) {
        assertEq(x, n++ - 1);
    } else
        throw 'not reached';
}
