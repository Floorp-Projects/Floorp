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
try {
    function x() {}
    assertEq(String(b), "function () {}");
} catch (e) { throw (e); }
