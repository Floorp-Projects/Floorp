// Section numbers cite ES6 rev 24 (2014 April 27).

var symbols = [
    Symbol(),
    Symbol("one"),
    Symbol.for("two"),
    Symbol.iterator
];

if (Symbol.toPrimitive in Symbol.prototype) {
    // We should test that deleting Symbol.prototype[@@toPrimitive] changes the
    // behavior of ToPrimitive on Symbol objects, but @@toPrimitive is not
    // implemented yet.
    throw new Error("Congratulations on implementing @@toPrimitive! Please update this test.");
}

for (var sym of symbols) {
    // 7.1.1 ToPrimitive
    var symobj = Object(sym);
    assertThrowsInstanceOf(() => Number(symobj), TypeError);
    assertThrowsInstanceOf(() => String(symobj), TypeError);
    assertThrowsInstanceOf(() => symobj < 0, TypeError);
    assertThrowsInstanceOf(() => 0 < symobj, TypeError);
    assertThrowsInstanceOf(() => symobj + 1, TypeError);
    assertThrowsInstanceOf(() => "" + symobj, TypeError);
    assertEq(sym == symobj, true);
    assertEq(sym === symobj, false);
    assertEq(symobj == 0, false);
    assertEq(0 != symobj, true);

    // 7.1.2 ToBoolean
    assertEq(Boolean(sym), true);
    assertEq(!sym, false);
    assertEq(sym || 13, sym);
    assertEq(sym && 13, 13);

    // 7.1.3 ToNumber
    assertThrowsInstanceOf(() => +sym, TypeError);
    assertThrowsInstanceOf(() => sym | 0, TypeError);

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
