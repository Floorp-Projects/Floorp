// Section numbers cite ES6 rev 24 (2014 April 27).

var symbols = [
    Symbol(),
    Symbol("one"),
    Symbol.for("two"),
    Symbol.iterator
];

for (var sym of symbols) {
    // 7.1.2 ToBoolean
    assertEq(Boolean(sym), true);
    assertEq(!sym, false);
    assertEq(sym || 13, sym);
    assertEq(sym && 13, 13);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
