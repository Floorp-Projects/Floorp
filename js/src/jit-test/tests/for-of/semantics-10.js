// The LHS of a for-loop is not bound to a particular scope until after the .next() method returns.

var obj = {};

// Test 1
function g() {
    obj.x = 0;
    yield 1;
}
var x = 2, n = 0;
with (obj) {
    for (x of g())  // g().next() inserts a binding for x on obj
        n++;
}
assertEq(x, 2);
assertEq(obj.x, 1);
assertEq(n, 1);

// Test 2
function h() {
    delete obj.x;
    yield 3;
}
n = 0;
with (obj) {
    for (x of h())  // h().next() deletes the binding for x on obj
        n++;
}
assertEq(x, 3);
assertEq("x" in obj, false);
assertEq(n, 1);
