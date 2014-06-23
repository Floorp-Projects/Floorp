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

    // 7.1.3 ToNumber
    assertEq(+sym, NaN);
    assertEq(sym | 0, 0);

    // 7.1.12 ToString
    assertThrowsInstanceOf(() => String(sym), TypeError);
    assertThrowsInstanceOf(() => "" + sym, TypeError);
    assertThrowsInstanceOf(() => sym + "", TypeError);
    assertThrowsInstanceOf(() => "" + [1, 2, Symbol()], TypeError);
    assertThrowsInstanceOf(() => ["simple", "thimble", Symbol()].join(), TypeError);

    // 7.1.13 ToObject
    var obj = Object(sym);
    assertEq(typeof obj, "object");
    assertEq(Object.prototype.toString.call(obj), "[object Symbol]");
    assertEq(Object.getPrototypeOf(obj), Symbol.prototype);
    assertEq(Object.getOwnPropertyNames(obj).length, 0);
    assertEq(Object(sym) === Object(sym), false);  // new object each time
    var f = function () { return this; };
    assertEq(f.call(sym) === f.call(sym), false);  // new object each time
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
