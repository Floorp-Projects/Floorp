load(libdir + "asserts.js");
// Revoked proxies should throw before calling the handler

var called = false;
var target = function () { };
var handler = { apply: () => called = true };
var holder = Proxy.revocable(target, handler);

holder.revoke();

assertThrowsInstanceOf(() => holder.proxy(), TypeError);
assertEq(called, false);
