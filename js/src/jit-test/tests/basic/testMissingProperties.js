function isnan(n) { return n !== n }

function f(x) {
    var sum = 0;
    for (var i = 0; i < 100; ++i)
        sum += x.x;
    return sum;
}
var o = {};
assertEq(isnan(f(o)), true);
o.x = 1;
assertEq(f(o), 100);
var o = {a:1, b:2};
assertEq(isnan(f(o)), true);
o.x = 2;
assertEq(f(o), 200);

function g(x) {
    var sum = 0;
    for (var i = 0; i < 100; ++i)
        sum += x.x;
    return sum;
}
var o = {c:1, x:1};
assertEq(g(o), 100);
var o = {};
assertEq(isnan(g(o)), true);

function h(x) {
    var sum = 0;
    for (var i = 0; i < 100; ++i)
        sum += x.x;
    return sum;
}

var proto1 = {};
var proto2 = Object.create(proto1);
var o = Object.create(proto2);
assertEq(isnan(f(o)), true);
assertEq(isnan(g(o)), true);
assertEq(isnan(h(o)), true);
proto2.x = 2;
assertEq(f(o), 200);
assertEq(g(o), 200);
assertEq(h(o), 200);
var o = {}
assertEq(isnan(f(o)), true);
assertEq(isnan(g(o)), true);
assertEq(isnan(h(o)), true);
