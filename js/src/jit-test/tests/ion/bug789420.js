function g1(x, y) {
    return 0 & y;
}
var c1 = 0;
function f1() {
    for (var i=0; i<100; i++) {
        g1(i, i);
        g1(i, {valueOf: function() { c1++; return 0; }});
    }
}
f1();
assertEq(c1, 100);

function g2(x, y) {
    ~y;
}
var c2 = 0;
function f2() {
    for (var i=0; i<100; i++) {
        g2(i, i);
        g2(i, {valueOf: function() { c2++; return 0; }});
    }
}
f2();
assertEq(c2, 100);

function g3(x, y) {
    return 0 >>> y;
}
var c3 = 0;
function f3() {
    for (var i=0; i<100; i++) {
        g3(i, i);
        g3(i, {valueOf: function() { c3++; return 0; }});
    }
}
f3();
assertEq(c3, 100);
