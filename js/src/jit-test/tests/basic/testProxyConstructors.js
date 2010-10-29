// proxies can return primitives
assertEq(new (Proxy.createFunction({}, function(){}, function(){})), undefined);

x = Proxy.createFunction((function () {}), Uint16Array, wrap)
new(wrap(x))

// proxies can return the callee
var x = Proxy.createFunction({}, function (q) { return q; });
new x(x);
