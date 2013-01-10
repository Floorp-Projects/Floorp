Function.prototype.callX = Function.prototype.call;
var x;
function f() {
    x = f.caller;
    return x;
}

function g() {
    return f.callX(null);
}

function h1() {
    // native
    return ([0].map(f))[0];
}

function h2() {
    // self-hosted
    x = null;
    [0].forEach(f);
    return x;
}

function k() {
    x = null;
    [0, 1].sort(f);
    return x;
}

function l() {
    return f();
}

assertEq(g(), g);
assertEq(h1(), h1);
assertEq(h2(), h2);
assertEq(k(), k);
assertEq(l(), l);

var baz;
var foo = {callX: function() { return "m"; }};
function bar() {
    return baz.caller;
}
function m() {
    return baz.callX(null);
}

baz = foo;
assertEq(m(), "m");
baz = bar;
assertEq(m(), m);
assertEq(m(), m);
