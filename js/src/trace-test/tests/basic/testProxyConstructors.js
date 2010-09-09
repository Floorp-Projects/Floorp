// proxies can return primitives
assertEq(new (Proxy.createFunction({}, function(){}, function(){})), undefined);

// proxies can return the callee
var x = Proxy.createFunction({}, function (q) { return q; });
new x(x);
