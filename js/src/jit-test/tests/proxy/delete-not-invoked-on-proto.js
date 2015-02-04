// Create Proxy that throws for everything.
var {proxy, revoke} = Proxy.revocable({}, {});

var obj = {__proto__: proxy, a: 1};
// This revokes the proxy, so every operation on it THROWS.
revoke();

assertEq(delete obj.a, true);
assertEq(delete obj.b, true);
// Should not have invoked anything on [[Prototype]]
