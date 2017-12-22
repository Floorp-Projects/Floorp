// ES6 19.4.3.4 Symbol.prototype[@@toPrimitive](hint)

// This method gets the primitive symbol from a Symbol wrapper object.
var sym = Symbol.for("truth")
var obj = Object(sym);
assertEq(obj[Symbol.toPrimitive]("default"), sym);

// The hint argument is ignored.
assertEq(obj[Symbol.toPrimitive]("number"), sym);
assertEq(obj[Symbol.toPrimitive]("string"), sym);
assertEq(obj[Symbol.toPrimitive](), sym);
assertEq(obj[Symbol.toPrimitive](Math.atan2), sym);

// The this value can also be a primitive symbol.
assertEq(sym[Symbol.toPrimitive](), sym);

// Or a wrapper to a Symbol object in another compartment.
var obj2 = newGlobal().Object(sym);
assertEq(obj2[Symbol.toPrimitive]("default"), sym);

// Otherwise a TypeError is thrown.
var symbolToPrimitive = Symbol.prototype[Symbol.toPrimitive];
var nonSymbols = [
    undefined, null, true, 13, NaN, "justice", {}, [sym],
    symbolToPrimitive,
    new Proxy(obj, {})
];
for (var value of nonSymbols) {
    assertThrowsInstanceOf(() => symbolToPrimitive.call(value, "string"), TypeError);
}

// Surface features:
assertEq(symbolToPrimitive.name, "[Symbol.toPrimitive]");
var desc = Object.getOwnPropertyDescriptor(Symbol.prototype, Symbol.toPrimitive);
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, false);

reportCompare(0, 0);
