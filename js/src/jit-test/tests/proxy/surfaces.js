// Check superficial properties of the Proxy constructor.

var desc = Object.getOwnPropertyDescriptor(this, "Proxy");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, true);
assertEq(desc.value, Proxy);

assertEq(typeof Proxy, "function");
assertEq(Object.getPrototypeOf(Proxy), Function.prototype);
assertEq(Proxy.length, 2);

// Proxy is a constructor but has no .prototype property.
assertEq(Proxy.hasOwnProperty("prototype"), false);
