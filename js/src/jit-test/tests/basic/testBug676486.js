var proxy = new Proxy(function() {
        return (function () { eval("foo") })();
    }, {});

try {
    new proxy();
} catch (e) {
}
