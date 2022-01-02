var called = [];
var proxy = new Proxy({a: 1}, {
    getOwnPropertyDescriptor(target, P) {
        called.push("getOwnPropertyDescriptor");
        return Object.getOwnPropertyDescriptor(target, P);
    },
    defineProperty(target, P, desc) {
        called.push("defineProperty");
        assertEq(Object.getOwnPropertyNames(desc).length, 1);
        assertEq(desc.configurable, false);
        return Object.defineProperty(target, P, desc);
    }
});

Object.seal(proxy);
assertEq(called.toString(), "defineProperty");
