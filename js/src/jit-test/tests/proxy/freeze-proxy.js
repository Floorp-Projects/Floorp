var called = [];
var proxy = new Proxy({a: 1, get b() {}}, {
    getOwnPropertyDescriptor(target, P) {
        called.push("getOwnPropertyDescriptor");
        return Object.getOwnPropertyDescriptor(target, P);
    },
    defineProperty(target, P, desc) {
        called.push("defineProperty");
        if (P == "a") {
            assertEq(Object.getOwnPropertyNames(desc).length, 2);
            assertEq(desc.configurable, false);
            assertEq(desc.writable, false);
        } else {
            assertEq(Object.getOwnPropertyNames(desc).length, 1);
            assertEq(desc.configurable, false);
        }
        return Object.defineProperty(target, P, desc);
    }
});

Object.freeze(proxy);
assertEq(called.toString(), "getOwnPropertyDescriptor,defineProperty,getOwnPropertyDescriptor,defineProperty");
