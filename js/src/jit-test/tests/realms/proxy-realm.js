// Based on a test written by André Bargull (bug 1297179).

load(libdir + "asserts.js");

var g = newGlobal({sameCompartmentAs: this});
var {proxy, revoke} = g.eval(`Proxy.revocable(function(){}, {})`);

revoke();

assertEq(objectGlobal(proxy), g);
assertThrowsInstanceOf(() => proxy(), TypeError);
assertThrowsInstanceOf(() => new proxy(), TypeError);
assertThrowsInstanceOf(() => proxy.foo, TypeError);
assertThrowsInstanceOf(() => proxy.foo = 1, TypeError);
