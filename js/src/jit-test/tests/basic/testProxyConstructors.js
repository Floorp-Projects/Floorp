// |jit-test| error: ExitCleanly

var handler = { getPropertyDescriptor() { return undefined; } }

assertEq((new (Proxy.createFunction(handler,
                                    function(){ this.x = 1 },
                                    function(){ this.x = 2 }))).x, 2);
// proxies can return the callee
var x = Proxy.createFunction(handler, function (q) { return q; });
assertEq(new x(x), x);
try {
    var x = (Proxy.createFunction(handler, "".indexOf));
    new x;
    throw "Should not be reached"
}
catch (e) {
    assertEq(String(e.message).indexOf('is not a constructor') === -1, false);
}
throw "ExitCleanly"
