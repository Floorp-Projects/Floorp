load(libdir + "asserts.js");

var sym = Symbol();

function add2(x) {
    return x + 2;
}
for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => add2(sym), TypeError);

function sqr(x) {
    return x * x;
}
for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => sqr(sym), TypeError);

function bit_or(x) {
    return x | x;
}
for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => bit_or(sym), TypeError);

function bit_not(x) {
    return ~x;
}
for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => bit_not(sym), TypeError);

function plus(x) {
    return +x;
}
for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => plus(sym), TypeError);

function f(a, b) {
    return a + b;
}

function testPoly() {
    assertEq(f(20, 30), 50);
    assertEq(f("one", "two"), "onetwo");
    assertThrowsInstanceOf(() => f(Symbol("one"), Symbol("two")), TypeError);
    assertThrowsInstanceOf(() => f(Symbol("14"), 14), TypeError);
    assertThrowsInstanceOf(() => f(Symbol("14"), 13.719), TypeError);
    assertThrowsInstanceOf(() => f(14, Symbol("14")), TypeError);
    assertThrowsInstanceOf(() => f(13.719, Symbol("14")), TypeError);
}

for (var i = 0; i < 9; i++)
    testPoly();

for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => assertEq(f(Symbol("14"), "40"), NaN), TypeError);
