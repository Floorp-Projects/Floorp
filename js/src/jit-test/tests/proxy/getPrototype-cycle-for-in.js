// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration("wasi")
timeout(0.5);

var proxy = new Proxy({}, {
    getPrototypeOf() {
        return proxy;
    }
});

var obj = {a: 1, b: 2, __proto__: proxy};
for (var x in obj) {}
assertEq(0, 1); // Should timeout.
