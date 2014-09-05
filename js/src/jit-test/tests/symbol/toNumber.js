load(libdir + "asserts.js");

function add2(x) {
    return x + 2;
}

function sqr(x) {
    return x * x;
}

function bit_or(x) {
    return x | x;
}

function bit_not(x) {
    return ~x;
}

function plus(x) {
    return +x;
}

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

if (typeof Symbol === "function") {
    var sym = Symbol();

    for (var i = 0; i < 9; i++)
        assertThrowsInstanceOf(() => add2(sym), TypeError);
    for (var i = 0; i < 9; i++)
        assertThrowsInstanceOf(() => sqr(sym), TypeError);
    for (var i = 0; i < 9; i++)
        assertThrowsInstanceOf(() => bit_or(sym), TypeError);
    for (var i = 0; i < 9; i++)
        assertThrowsInstanceOf(() => bit_not(sym), TypeError);
    for (var i = 0; i < 9; i++)
        assertThrowsInstanceOf(() => plus(sym), TypeError);
    for (var i = 0; i < 9; i++)
        testPoly();

    for (var i = 0; i < 9; i++)
        assertThrowsInstanceOf(() => assertEq(f(Symbol("14"), "40"), NaN), TypeError);
}
