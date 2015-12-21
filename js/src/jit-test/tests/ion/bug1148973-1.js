Object.defineProperty(this, "x", { get: decodeURI, configurable: true })
try {
    String(b = Proxy.createFunction(function() {
        return {
            get: function(r, z) {
                return x[z]
            }
        }
    }(), function() {}))
} catch (e) {};
var log = "";
evaluate(`
try {
    function x() {}
    assertEq(String(b), "function () {}");
} catch (e) { log += "e"; }
`);
assertEq(log, "e");
