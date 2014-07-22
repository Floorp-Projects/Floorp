load(libdir + "asserts.js");
// Revoked proxies should throw before calling the handler

var called = false;
var target = {};
var handler = { get: () => called = true };
var holder = Proxy.revocable(target, handler);

holder.revoke();

assertThrowsInstanceOf(() => holder.proxy.foo, TypeError);
assertEq(called, false);
