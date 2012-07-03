// The LHS of a for-of loop is not evaluated until after the .next() method returns.

var s;
function f() {
    s += 'f';
    return {};
}

// Test 1: .next() throws StopIteration right away. f is never called.
s = '';
for (f().x of [])
    s += '.';
assertEq(s, '');

// Test 2: check proper interleaving of f calls, iterator.next() calls, and the loop body.
function g() {
    s += 'g';
    yield 0;
    s += 'g';
    yield 1;
    s += 'g';
}
for (f().x of g())
    s += '.';
assertEq(s, 'gf.gf.g');
