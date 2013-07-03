load(libdir + "asserts.js");

// Throw a TypeError if Proxy is called with less than two arguments
assertThrowsInstanceOf(function () { Proxy(); }, TypeError);
assertThrowsInstanceOf(function () { new Proxy(); }, TypeError);
assertThrowsInstanceOf(function () { Proxy({}); }, TypeError);
assertThrowsInstanceOf(function () { new Proxy({}); }, TypeError);

// Throw a TypeError if the first argument is not a non-null object
assertThrowsInstanceOf(function () { Proxy(0, {}); }, TypeError);
assertThrowsInstanceOf(function () { new Proxy(0, {}); }, TypeError);
assertThrowsInstanceOf(function () { Proxy(null, {}); }, TypeError);
assertThrowsInstanceOf(function () { new Proxy(null, {}); }, TypeError);

// Throw a TypeError if the second argument is not a non-null object
assertThrowsInstanceOf(function () { Proxy({}, 0); }, TypeError);
assertThrowsInstanceOf(function () { new Proxy({}, 0); }, TypeError);
assertThrowsInstanceOf(function () { Proxy({}, null); }, TypeError);
assertThrowsInstanceOf(function () { new Proxy({}, null); }, TypeError);

// Result of the call should be an object
assertEq(typeof Proxy({}, {}), 'object');
assertEq(typeof new Proxy({}, {}), 'object');
