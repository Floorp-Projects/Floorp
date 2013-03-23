// Forward to the target if the trap is not defined
var target = {};
Object.preventExtensions(new Proxy(target, {}));
assertEq(Object.isExtensible(target), false);
