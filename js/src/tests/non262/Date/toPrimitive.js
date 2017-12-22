// ES6 20.3.4.45 Date.prototype[@@toPrimitive](hint)

// The toPrimitive method throws if the this value isn't an object.
var toPrimitive = Date.prototype[Symbol.toPrimitive];
assertThrowsInstanceOf(() => toPrimitive.call(undefined, "default"), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(3, "default"), TypeError);

// It doesn't have to be a Date object, though.
var obj = {
    toString() { return "str"; },
    valueOf() { return "val"; }
};
assertEq(toPrimitive.call(obj, "number"), "val");
assertEq(toPrimitive.call(obj, "string"), "str");
assertEq(toPrimitive.call(obj, "default"), "str");

// It throws if the hint argument is missing or not one of the three allowed values.
assertThrowsInstanceOf(() => toPrimitive.call(obj), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, undefined), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, "boolean"), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, ["number"]), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, {toString() { throw "FAIL"; }}), TypeError);

// The next few tests cover the OrdinaryToPrimitive algorithm, specified in
// ES6 7.1.1 ToPrimitive(input [, PreferredType]).

// Date.prototype.toString or .valueOf can be overridden.
var dateobj = new Date();
Date.prototype.toString = function () {
    assertEq(this, dateobj);
    return 14;
};
Date.prototype.valueOf = function () {
    return "92";
};
assertEq(dateobj[Symbol.toPrimitive]("number"), "92");
assertEq(dateobj[Symbol.toPrimitive]("string"), 14);
assertEq(dateobj[Symbol.toPrimitive]("default"), 14);
assertEq(dateobj == 14, true);  // equality comparison: passes "default"

// If this.toString is a non-callable value, this.valueOf is called instead.
Date.prototype.toString = {};
assertEq(dateobj[Symbol.toPrimitive]("string"), "92");
assertEq(dateobj[Symbol.toPrimitive]("default"), "92");

// And vice versa.
Date.prototype.toString = function () { return 15; };
Date.prototype.valueOf = "ponies";
assertEq(dateobj[Symbol.toPrimitive]("number"), 15);

// If neither is callable, it throws a TypeError.
Date.prototype.toString = "ponies";
assertThrowsInstanceOf(() => dateobj[Symbol.toPrimitive]("default"), TypeError);

// Surface features.
assertEq(toPrimitive.name, "[Symbol.toPrimitive]");
var desc = Object.getOwnPropertyDescriptor(Date.prototype, Symbol.toPrimitive);
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, false);

reportCompare(0, 0);
