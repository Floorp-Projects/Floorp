// Direct proxies pass through the receiver argument to [[Set]] to their targets.
// This also tests that an ordinary object's [[Set]] method can change the length
// of an array passed as the receiver.

load(libdir + "asserts.js");

var a = [0, 1, 2, 3];
var p = new Proxy({}, {});
Reflect.set(p, "length", 2, a);
assertEq("length" in p, false);
assertEq(a.length, 2);
assertDeepEq(a, [0, 1]);
