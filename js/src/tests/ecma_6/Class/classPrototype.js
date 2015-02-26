var test = `

// The prototype of a class is a non-writable, non-configurable, non-enumerable data property.
class a { constructor() { } }
var protoDesc = Object.getOwnPropertyDescriptor(a, "prototype");
assertEq(protoDesc.writable, false);
assertEq(protoDesc.configurable, false);
assertEq(protoDesc.enumerable, false);

var prototype = protoDesc.value;
assertEq(typeof prototype, "object");
assertEq(Object.getPrototypeOf(prototype), Object.prototype);
assertEq(Object.isExtensible(prototype), true);

var desiredPrototype = {};
Object.defineProperty(desiredPrototype, "constructor", { writable: true,
                                                         configurable: true,
                                                         enumerable: false,
                                                         value: a });
assertDeepEq(prototype, desiredPrototype);
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
