load(libdir + "asserts.js");

function add() {
    assertEq(Symbol() + 2, NaN);
}
for (var i = 0; i < 9; i++)
    add();

function mul() {
    assertEq(Symbol() * Symbol(), NaN);
}
for (var i = 0; i < 9; i++)
    mul();

function bit_or() {
    assertEq(Symbol() | Symbol(), 0);
}
for (var i = 0; i < 9; i++)
    bit_or();

function bit_not() {
    assertEq(~Symbol(), -1);
}
for (var i = 0; i < 9; i++)
    bit_not();

function plus() {
    assertEq(+Symbol(), NaN);
}
for (var i = 0; i < 9; i++)
    plus();


function f(a, b) {
    return a + b;
}

function testPoly() {
    assertEq(f(20, 30), 50);
    assertEq(f("one", "two"), "onetwo");
    assertEq(f(Symbol("one"), Symbol("two")), NaN);
    assertEq(f(Symbol("14"), 14), NaN);
    assertEq(f(Symbol("14"), 13.719), NaN);
    assertEq(f(14, Symbol("14")), NaN);
    assertEq(f(13.719, Symbol("14")), NaN);
}

for (var i = 0; i < 9; i++)
    testPoly();

for (var i = 0; i < 9; i++)
    assertThrowsInstanceOf(() => assertEq(f(Symbol("14"), "40"), NaN), TypeError);
