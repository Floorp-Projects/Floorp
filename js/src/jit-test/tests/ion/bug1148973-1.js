Object.defineProperty(this, "x", { get: decodeURI, configurable: true })
try {
    String(b = new Proxy(function() { }, {
        get: function(r, z) {
            return x[z]
        }
    }))
} catch (e) {};
var log = "";
evaluate(`
try {
    function x() {}
    assertEq(String(b), "function () {}");
} catch (e) { log += "e"; }
`);
assertEq(log, "e");
