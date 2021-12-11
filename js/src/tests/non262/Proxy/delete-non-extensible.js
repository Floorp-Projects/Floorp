"use strict";

var target = { test: true };
Object.preventExtensions(target);

var proxy = new Proxy(target, {
    deleteProperty(target, property) {
        return true;
    }
});

assertEq(delete proxy.missing, true);
assertEq(Reflect.deleteProperty(proxy, "missing"), true);

assertThrowsInstanceOf(() => { delete proxy.test; }, TypeError);
assertThrowsInstanceOf(() => Reflect.deleteProperty(proxy, "test"), TypeError);

reportCompare(0, 0);
