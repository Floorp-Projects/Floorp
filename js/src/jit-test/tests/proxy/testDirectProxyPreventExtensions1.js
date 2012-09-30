// Forward to the target if the trap is not defined
target = {};
var proxy = new Proxy(target, {});
Object.preventExtensions(proxy);
assertEq(Object.isExtensible(target), false);
assertEq(Object.isExtensible(proxy), false);
