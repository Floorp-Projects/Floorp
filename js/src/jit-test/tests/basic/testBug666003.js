function f() {
    f = function() { g(); };
    f();
}
g = f;

var caught = false;
try {
    f();
} catch(e) {
    caught = true;
}
assertEq(caught, true);
