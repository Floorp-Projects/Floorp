function g1(x) {
    return x + 1;
}
function f1() {
    var y = 0;
    for (var i=0; i<100; i++) {
        y += g1(g1(i));
    }
    return y;
}
g1(10);
assertEq(f1(), 5150);

x = 1;
other = newGlobal("same-compartment");
other.eval("f = function() { return x; }; x = 2;");

h = other.f;

function testOtherGlobal() {
    var y = 0;
    for (var i=0; i<100; i++) {
        y += h();
    }
    return y;
}
h();
assertEq(testOtherGlobal(), 200);

// Note: this test requires on On-Stack Invalidation.
f2 = function() {
    return x;
}
function test2() {
    var y = 0;
    for (var i=0; i<50; i++) {
        y += f2();
    }
    return y;
}
assertEq(test2(), 50);
f2 = h;
assertEq(test2(), 100);
