function foo() {
    bar(1,2,3,4,5,6,7,8,9);
}

function bar() {
    foo(1,2,3,4,5,6,7,8,9);
}

var caught = false;
try {
    foo();
} catch (e) {
    caught = true;
}
assertEq(caught, true);
