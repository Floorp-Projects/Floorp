load(libdir + "asserts.js");

// eval({__proto__}) changes [[Prototype]]
var obj = eval('({"__proto__": null})');
assertEq(Object.getPrototypeOf(obj), null);
assertEq(Object.prototype.hasOwnProperty.call(obj, "__proto__"), false);

// JSON.parse({__proto__}) creates new property __proto__
obj = JSON.parse('{"__proto__": null}');
assertEq(Object.getPrototypeOf(obj), Object.prototype);
assertEq(Object.prototype.hasOwnProperty.call(obj, "__proto__"), true);

// If __proto__ appears more than once as quoted or unquoted property name in an
// object initializer expression, that's an error.
//(No other property name has this restriction.)"
assertThrowsInstanceOf(() =>
    eval('({ "__proto__" : null, "__proto__" : null })'), SyntaxError);

assertThrowsInstanceOf(() =>
    eval('  ({ "__proto__" : null, "__proto__" : null })'), SyntaxError);

// JSON.parse doesn't care about duplication, the last definition counts.
obj = JSON.parse('{"__proto__": null, "__proto__": 5}');
assertEq(Object.getPrototypeOf(obj), Object.prototype);
assertEq(obj["__proto__"], 5);
