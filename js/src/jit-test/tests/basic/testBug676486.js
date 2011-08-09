var proxy = Proxy.createFunction(
    {},
    function() {
        return (function () { eval("foo") })();
    });

try {
    new proxy();
} catch (e) {
}
