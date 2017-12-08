var BUGNUMBER = 1151149;
var summary = "Proxy constructor should throw if either the target or handler is a revoked proxy.";

print(BUGNUMBER + ": " + summary);

var p = new Proxy({}, {});

new Proxy(p, {});
new Proxy({}, p);

var r = Proxy.revocable({}, {});
p = r.proxy;

new Proxy(p, {});
new Proxy({}, p);

r.revoke();

assertThrowsInstanceOf(() => new Proxy(p, {}), TypeError);
assertThrowsInstanceOf(() => new Proxy({}, p), TypeError);


var r2 = Proxy.revocable({}, {});
r = Proxy.revocable(r2.proxy, {});
p = r.proxy;

new Proxy(p, {});
new Proxy({}, p);

r2.revoke();

new Proxy(p, {});
new Proxy({}, p);

r.revoke();

assertThrowsInstanceOf(() => new Proxy(p, {}), TypeError);
assertThrowsInstanceOf(() => new Proxy({}, p), TypeError);


var g = newGlobal();
p = g.eval(`var r = Proxy.revocable({}, {}); r.proxy;`);

new Proxy(p, {});
new Proxy({}, p);

g.eval(`r.revoke();`);

assertThrowsInstanceOf(() => new Proxy(p, {}), TypeError);
assertThrowsInstanceOf(() => new Proxy({}, p), TypeError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
